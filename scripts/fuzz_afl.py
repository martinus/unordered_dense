#!/usr/bin/env python3
"""Fuzz with AFL++ across every core, then fold what it found back into data/fuzz as a minimal set.

    scripts/fuzz_afl.py run                     # every target, one core each, until Ctrl-C
    scripts/fuzz_afl.py run fuzz_api            # one target on every core, until Ctrl-C
    scripts/fuzz_afl.py sweep                   # each target in turn, moving on when it goes quiet
    scripts/fuzz_afl.py sweep --idle 15m        # ... giving each one longer to prove it is done
    scripts/fuzz_afl.py minimize                # fold the findings in and shrink, then git diff

Stopping is safe at any point: afl-fuzz writes every input it keeps to disk as it finds it, so
Ctrl-C loses nothing. Running "run" again resumes from where the last one stopped.

Needs AFL++ (apt install afl++ / brew install afl++) and clang. The builds are created on demand.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ALL_TARGETS = ("fuzz_api", "fuzz_group_index", "fuzz_insert_erase", "fuzz_replace_map", "fuzz_string")
FINDINGS = Path(os.environ.get("FINDINGS") or ROOT / "fuzz-findings")  # gitignored, see .gitignore
BUILD_AFL = Path("builddir/afl")
BUILD_AFL_FAST = Path("builddir/afl-fast")
BUILD_LIBFUZZER = Path("builddir/fuzz")

# Enough for a target that calibrates very slowly, and only there so a stuck one cannot hang the
# script -- an instance that is going to fail has failed long before this.
STARTUP_TIMEOUT = 60

# Where the kernel describes which logical CPUs share a physical core. A name rather than a literal
# so a test can point fuzzing_cpus() at a directory describing a machine this one is not.
CPU_TOPOLOGY = Path("/sys/devices/system/cpu")


def die(message: str):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)


def fuzzing_cpus() -> list[int] | None:
    """One logical CPU per physical core, or None when the topology cannot be read.

    Hyperthread siblings share one core's execution units, so a second instance on the same core
    does not buy a second core's worth of fuzzing -- it mostly slows down the instance already
    there, while afl-fuzz counts both as busy. Every core, one instance each, is what is wanted.

    Which sibling represents a core is not something to assume from the numbering: whether the
    siblings of core 0 are 0 and 1 or 0 and n/2 differs between machines, so it comes from
    thread_siblings_list, where both siblings name the same pair.
    """
    groups: dict[str, list[int]] = {}
    for path in sorted(CPU_TOPOLOGY.glob("cpu[0-9]*")):
        try:
            siblings = (path / "topology" / "thread_siblings_list").read_text().strip()
        except OSError:
            continue
        groups.setdefault(siblings, []).append(int(path.name[3:]))
    return sorted(min(cpus) for cpus in groups.values()) or None


def core_count(cpus: list[int] | None) -> int:
    """How many instances to run, given what fuzzing_cpus() found. Takes the list rather than
    calling for it, because reading the topology means a file per logical CPU and the answer
    cannot change during a run."""
    return len(cpus) if cpus else (os.cpu_count() or 1)


def instance_name(index: int) -> str:
    """Instance 0 is the main one. afl-fuzz names the output subdirectory after this, so the -M/-S
    argument and the path fuzzer_stats appears at have to be decided in one place: get them out of
    step and wait_until_started watches for a file that never arrives."""
    return "main" if index == 0 else f"s{index}"


def sanitized_index(count: int) -> int:
    """Which instance carries the sanitizers. They cost around 7x, so exactly one does and the
    rest cover ground; with only one instance there is nothing to trade and it keeps them."""
    return 1 if count > 1 else 0


def ensure_fuzzing_builds(targets: list[str]):
    """Both builds every fuzzing session needs, which afl_argv then picks between per instance."""
    ensure_built(BUILD_AFL, "afl-clang-fast++", [], targets)
    ensure_built(BUILD_AFL_FAST, "afl-clang-fast++", ["-Dfuzz_sanitizers=false"], targets)


def preflight():
    """Refuse to start rather than fuzz badly. Both checks belong to running AFL at all, not to
    one subcommand -- sweep starts the same processes and used to skip them, so the same broken
    core_pattern was explained by run and left for sweep to fail on obscurely."""
    if shutil.which("afl-fuzz") is None:
        die("afl-fuzz not found. Install AFL++ first.")

    # afl-fuzz refuses to start when the kernel would hand crashes to a crash reporter instead of
    # to it, and it is right to: crashes would be missed or blamed on the wrong input.
    core_pattern = Path("/proc/sys/kernel/core_pattern")
    if core_pattern.is_file() and core_pattern.read_text().startswith("|"):
        die("core_pattern pipes to a crash handler. Fix it with:\n"
            "    sudo sh -c 'echo core > /proc/sys/kernel/core_pattern'")


def ensure_built(builddir: Path, compiler: str, setup_args: list[str], targets: list[str]):
    """Configure the build directory if it is not usable, then build the named targets.

    Both builds of the same target are needed: afl-clang-fast++ for afl-fuzz and afl-cmin, clang++
    for the -merge=1 pass. Targets are asked for by name because a bare ninja would also build
    udm-test, which does not link under AFL++ (it defines FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION,
    which fuzz/run.h reads as "honggfuzz is driving").
    """
    # build.ninja rather than the directory: a meson setup that failed, or one interrupted part way
    # through, leaves the directory behind, and testing for the directory then treats it as ready
    # and hands ninja something it cannot build.
    if not (builddir / "build.ninja").is_file():
        shutil.rmtree(builddir, ignore_errors=True)
        setup = subprocess.run(
            ["meson", "setup", str(builddir), "--force-fallback-for=fmt", *setup_args],
            env={**os.environ, "CXX": compiler},
            capture_output=True,
            text=True,
        )
        if setup.returncode != 0:
            sys.stderr.write(setup.stdout + setup.stderr)
            die(f"meson setup failed for {builddir} with CXX={compiler} {' '.join(setup_args)}".rstrip())

    # Not captured: when a build fails this is the only thing that says why, and when it succeeds
    # it is a handful of progress lines.
    if subprocess.run(["ninja", "-C", str(builddir), *(f"test/{t}" for t in targets)]).returncode != 0:
        die(f"could not build {' '.join(targets)} in {builddir}")


def afl_env(**extra: str) -> dict[str, str]:
    return {**os.environ, "AFL_SKIP_CPUFREQ": "1", "AFL_AUTORESUME": "1", **extra}


def wait_until_started(instances: list[Instance]):
    """Wait for every background instance to either start fuzzing or fail, and report the failures.

    afl-fuzz fails at startup for ordinary reasons: a target the instrumentation did not take to,
    an unreadable corpus, a core_pattern it will not run under. These instances have their UI off
    and their output in a log file nobody is looking at, so without this the run looks healthy --
    the foreground screen is healthy -- while most of the cores sit idle.

    Waiting a fixed couple of seconds and looking for corpses is not enough; measured on fuzz_api,
    an abort takes about as long to happen (3s) as a good instance takes to start fuzzing. So wait
    for one of the two to be true of each instance instead: fuzzer_stats exists, meaning it
    calibrated the corpus and is running, or the process is gone.
    """
    deadline = time.monotonic() + STARTUP_TIMEOUT
    while time.monotonic() < deadline:
        if all(i.stats.is_file() or i.process.poll() is not None for i in instances):
            break
        time.sleep(0.5)

    dead = [i for i in instances if i.process.poll() is not None]
    if dead:
        for instance in dead:
            print(f"--- {instance.log}", file=sys.stderr)
            tail = instance.log.read_text(errors="replace").splitlines()[-20:]
            print("\n".join(tail), file=sys.stderr)
        for instance in instances:
            instance.stop()
        die(f"{len(dead)} of {len(instances)} background instances exited at startup, see above")


class Instance:
    """One running afl-fuzz, with the files that say whether it is alive and why not.

    The one on screen is one of these too, with its UI on and no log to redirect to -- otherwise
    every capability added here would quietly exclude the instance being watched, and its shutdown
    would have to be written again at each call site.
    """

    def __init__(self, process: subprocess.Popen, log: Path | None, stats: Path):
        self.process = process
        self.log = log
        self.stats = stats

    def stop(self):
        if self.process.poll() is None:
            self.process.terminate()
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill()


def afl_argv(target: str, index: int, count: int, out: Path, cpu: int | None) -> list[str]:
    """The command for one instance: which build, which role, and which core it is pinned to.

    Instance 0 is the main one and does the deterministic passes; the rest explore at random and
    share whatever any of them finds through the output directory. A crash any of them finds is
    saved to the same place, so replay it under builddir/afl/test/<target> for the report.

    -b pins it. Left to itself afl-fuzz takes the first core it finds unused, which knows nothing
    about hyperthread siblings and would put two instances on one core while another sits idle.
    """
    build = BUILD_AFL if index == sanitized_index(count) else BUILD_AFL_FAST
    bind = ["-b", str(cpu)] if cpu is not None else []
    return ["afl-fuzz", "-M" if index == 0 else "-S", instance_name(index),
            *bind, "-i", f"data/fuzz/{target}", "-o", str(out),
            "--", f"./{build}/test/{target}"]


def start_instance(target: str, index: int, count: int, out: Path, cpu: int | None,
                   ui: bool = False) -> Instance:
    """Start one afl-fuzz. With ui it keeps the terminal and draws its screen; without, its UI is
    off and its output goes to a log file, because AFL's screen only makes sense one at a time."""
    argv = afl_argv(target, index, count, out, cpu)
    stats = out / instance_name(index) / "fuzzer_stats"
    if ui:
        return Instance(subprocess.Popen(argv, env=afl_env()), None, stats)

    log = out / f"afl-{index}.log"
    with log.open("wb") as handle:
        process = subprocess.Popen(
            argv,
            env=afl_env(AFL_NO_UI="1"),
            stdout=handle,
            stderr=subprocess.STDOUT,
        )
    return Instance(process, log, stats)


def cmd_run(targets: list[str]):
    preflight()
    ensure_fuzzing_builds(targets)

    # One target: every core on it. Several: split the cores between them, at least one each.
    cpus = fuzzing_cpus()
    per_target = max(1, core_count(cpus) // len(targets))

    # The first target's main instance runs in the foreground and keeps the terminal, so there is a
    # live status screen to watch rather than four silent log files.
    watched = targets[0]
    watched_cpu = cpus[0] if cpus else None

    print(f"fuzzing {' '.join(targets)} on {core_count(cpus)} cores "
          f"({per_target} per target), Ctrl-C to stop")
    print(f"showing {watched}, the rest log to {FINDINGS}/<target>/afl-*.log")
    print(f"sanitizers on instance s{sanitized_index(per_target)} of each target, "
          f"off elsewhere for speed")
    print()

    instances: list[Instance] = []
    try:
        slot = 0
        for target in targets:
            out = FINDINGS / target
            out.mkdir(parents=True, exist_ok=True)
            for i in range(per_target):
                # Wraps only when there are more targets than cores, where one each already
                # oversubscribes. Slot 0 is the watched target's, started last, in the foreground.
                cpu = cpus[slot % len(cpus)] if cpus else None
                slot += 1
                if target == watched and i == 0:
                    continue
                instances.append(start_instance(target, i, per_target, out, cpu))

        wait_until_started(instances)

        # A Ctrl-C goes to the whole process group, so afl-fuzz gets it directly and saves its
        # queue before exiting, and this process gets the KeyboardInterrupt -- which is what tells
        # a stop apart from a failure. Popen and wait rather than subprocess.run, which kills the
        # child on KeyboardInterrupt: the child is busy saving its queue, so waiting is the point.
        interrupted = False
        foreground = start_instance(watched, 0, per_target, FINDINGS / watched, watched_cpu, ui=True)
        instances.append(foreground)
        try:
            status = foreground.process.wait()
        except KeyboardInterrupt:
            interrupted = True
            status = foreground.process.wait()
    finally:
        for instance in instances:
            instance.stop()

    if not interrupted and status != 0:
        die(f"afl-fuzz on {watched} exited with {status}")

    print()
    print(f"stopped. Fold the findings in with: scripts/fuzz_afl.py minimize {' '.join(targets)}")


def clock(seconds: float) -> str:
    return f"{int(seconds) // 60}:{int(seconds) % 60:02d}"


def queue_size(out: Path) -> int:
    """How many inputs the instances have kept between them, counted off the disk.

    Deliberately not fuzzer_stats' corpus_count. afl-fuzz rewrites that file on its own schedule,
    so both it and last_find can sit unchanged for a minute at a time -- long enough for a target
    that is still finding things to look like it has gone quiet. A queue entry, on the other hand,
    is written the moment it is found.
    """
    return sum(1 for _ in queue_inputs(out))


def queue_inputs(out: Path):
    """Every queue entry under an output directory, as os.DirEntry.

    scandir rather than Path.glob: this runs on a timer against directories a long session fills
    with six figures of entries, and glob builds a Path and matches a pattern per file. Measured
    over 170k entries, 217ms for the glob against 83ms for this.
    """
    try:
        for instance in os.scandir(out):
            if not instance.is_dir():
                continue
            try:
                for entry in os.scandir(Path(instance.path) / "queue"):
                    if entry.name.startswith("id:") and entry.is_file():
                        yield entry
            except OSError:
                continue  # no queue yet, or it went away underneath us
    except OSError:
        # afl-fuzz is writing into these directories while this walks them. A short count only
        # ever delays the decision, because only an increase counts as a find.
        return


def sweep_one(target: str, idle_seconds: int) -> str:
    """Fuzz one target on every core until it has gone idle_seconds without a new find.

    The main instance keeps the terminal and its status screen, the same as run: its "last new
    find" counter is the thing to watch here, since it is what decides when this moves on. The
    deciding is done by a thread instead, because the screen belongs to afl-fuzz -- it watches
    every instance rather than only the one on display, and stops them all when they have all gone
    quiet, which ends the foreground wait below.
    """
    out = FINDINGS / target
    out.mkdir(parents=True, exist_ok=True)
    cpus = fuzzing_cpus()
    count = core_count(cpus)
    instances = [start_instance(target, i, count, out, cpus[i] if cpus else None)
                 for i in range(1, count)]
    started = time.time()
    outcome_text: str | None = None
    try:
        wait_until_started(instances)

        foreground = start_instance(target, 0, count, out, cpus[0] if cpus else None, ui=True)
        instances.append(foreground)
        done = threading.Event()
        progress = (out / "sweep.log").open("w", buffering=1)

        def watch():
            nonlocal outcome_text
            # Anything raised in here used to end the thread and nothing else, which left the
            # sweep fuzzing one target for as long as it was allowed to: no decision was being
            # made any more and there was nothing on screen to say so.
            try:
                # The clock starts now rather than from anything on disk, so a resumed output
                # directory full of earlier findings cannot make a target look stale before it
                # has had a chance.
                found = queue_size(out)
                last_find = time.time()
                # Five seconds between looks, except for an --idle small enough that five seconds
                # of rounding would be most of it. Looking is a scan of the queue directories,
                # which is not free once a long session has filled them.
                poll = min(5.0, max(1.0, idle_seconds / 10))
                while not done.wait(poll):
                    now = queue_size(out)
                    if now > found:
                        found, last_find = now, time.time()
                    idle = time.time() - last_find
                    # afl-fuzz owns the terminal, and its screen counts finds for the one instance
                    # it is showing while this counts them across all of them -- so on a machine
                    # with many cores the screen can sit at a long "last new find" while a
                    # secondary keeps resetting this clock. Without somewhere to look, that is
                    # indistinguishable from a sweep that has stopped deciding.
                    progress.write(f"{time.strftime('%H:%M:%S')}  queue {found}  "
                                   f"idle {clock(idle)}/{clock(idle_seconds)}\n")
                    if idle >= idle_seconds:
                        outcome_text = (f"{target}: quiet for {clock(idle)}, stopping after "
                                        f"{clock(time.time() - started)} with {found} inputs")
                        # ends the wait below; the finally stops the rest
                        foreground.process.terminate()
                        return
            except Exception as error:  # noqa: BLE001 -- whatever it is, do not fuzz forever
                outcome_text = f"{target}: stopped watching it after {error!r}"
                progress.write(f"watching failed: {error!r}\n")
                foreground.process.terminate()

        watcher = threading.Thread(target=watch, daemon=True)
        watcher.start()
        foreground.process.wait()
        done.set()
        watcher.join(timeout=15)
        progress.close()
        if outcome_text is not None:
            return outcome_text
        return (f"{target}: the main instance exited on its own after "
                f"{clock(time.time() - started)}, with {queue_size(out)} inputs")
    finally:
        for instance in instances:
            instance.stop()


def cmd_sweep(targets: list[str], idle_seconds: int):
    preflight()
    ensure_fuzzing_builds(targets)

    plural = "target" if len(targets) == 1 else "targets"
    print(f"sweeping {len(targets)} {plural} on {core_count(fuzzing_cpus())} cores each, "
          f"one at a time")
    print(f"moving on when a target goes {clock(idle_seconds)} without a new find, Ctrl-C to stop")
    print()

    for target in targets:
        # Said before the screen takes over, because once it does this is the only way to see what
        # the sweep makes of the target -- afl-fuzz's own counters are for one instance.
        print(f"{target}: watch the decision with tail -f {FINDINGS / target / 'sweep.log'}",
              flush=True)
        print(f"  -> {sweep_one(target, idle_seconds)}", flush=True)

    print()
    print(f"crashes, if any: find {FINDINGS} -path '*/crashes/id:*'")
    print(f"fold the findings in with: scripts/fuzz_afl.py minimize {' '.join(targets)}")


def stage_by_content(sources: list[str], dest: Path) -> int:
    """Hardlink every source into dest under the sha1 of its contents, and say how many landed.

    Naming by content is what the final corpus uses anyway, and here it also collapses duplicates
    for free. Instances copy inputs from each other whenever they sync, so a good input ends up
    sitting in several queues and afl-cmin would otherwise execute every copy: measured on one
    four-instance run, 7960 queue files but only 5047 distinct ones.

    Hardlinks because these files are only ever read; the copy is for a FINDINGS on a different
    filesystem, where linking cannot work.
    """
    dest.mkdir(parents=True, exist_ok=True)

    def stage_batch(batch: list[str]):
        # A batch per worker rather than a file per worker: ThreadPoolExecutor.map has no chunksize
        # and each item costs a future, which against six figures of inputs is around a third of
        # the time for three syscalls of actual work.
        for source in batch:
            target = dest / hashlib.sha1(Path(source).read_bytes()).hexdigest()
            try:
                os.link(source, target)
            except FileExistsError:
                pass  # same contents already staged, which is the deduplication
            except OSError:
                shutil.copyfile(source, target)

    workers = core_count(fuzzing_cpus())
    size = max(1, -(-len(sources) // workers))
    with ThreadPoolExecutor(max_workers=workers) as pool:
        list(pool.map(stage_batch, [sources[i:i + size] for i in range(0, len(sources), size)]))
    return sum(1 for _ in os.scandir(dest))


def run_logged(command: list[str], log: Path, what: str, env: dict[str, str] | None = None):
    """Run a command, keeping its output in a log file that is named if it fails."""
    with log.open("wb") as handle:
        if subprocess.run(command, stdout=handle, stderr=subprocess.STDOUT, env=env).returncode != 0:
            die(f"{what} failed, see {log}")


def coverage_of(target: str, corpus: Path) -> str:
    """What libFuzzer says the corpus covers, as its own INITED line."""
    done = subprocess.run(
        [f"./{BUILD_LIBFUZZER}/test/{target}", "-runs=0", str(corpus)],
        capture_output=True,
        text=True,
    )
    found = re.search(r"INITED cov: \d+ ft: \d+", done.stdout + done.stderr)
    return found.group(0) if found else "no coverage reported"


def cmd_minimize(targets: list[str]):
    if shutil.which("afl-cmin") is None:
        die("afl-cmin not found. Install AFL++ first.")
    ensure_built(BUILD_AFL, "afl-clang-fast++", [], targets)
    ensure_built(BUILD_LIBFUZZER, "clang++", [], targets)

    for target in targets:
        work = FINDINGS / ".minimize" / target
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)

        # Unsorted, and dirents rather than Paths: nothing downstream cares about the order,
        # because staging names every file by the sha1 of its contents, and sorting six figures of
        # paths costs more than collecting them does.
        corpus = Path("data/fuzz") / target
        committed = [e.path for e in os.scandir(corpus) if e.is_file()]
        found = [e.path for e in queue_inputs(FINDINGS / target)]
        staged = stage_by_content(committed + found, work / "all")
        before = len(committed)

        # Two passes, because neither minimizer subsumes the other. afl-cmin keeps a set covering
        # the same AFL edges and is far more aggressive; -merge=1 onto its output adds back the
        # files carrying a libFuzzer feature it could not see. The @@ matters: afl-cmin pipes stdin
        # by default, and this driver reports no coverage for that. -T runs the executions across
        # every core -- only the trace collection parallelises and the solve stays single threaded,
        # so it is not a speedup by the core count: 39s to 30s over 5520 inputs on four cores.
        run_logged(
            ["afl-cmin", "-T", "all", "-i", str(work / "all"), "-o", str(work / "cmin"),
             "--", f"./{BUILD_AFL}/test/{target}", "@@"],
            work / "cmin.log",
            "afl-cmin",
            env=afl_env(),
        )
        # Linked, not copied: -merge=1 only adds files to its destination, and both of these are
        # under FINDINGS/.minimize, so they are on one filesystem by construction.
        shutil.copytree(work / "cmin", work / "min", copy_function=os.link)
        run_logged(
            [f"./{BUILD_LIBFUZZER}/test/{target}", "-merge=1", str(work / "min"), str(work / "all")],
            work / "merge.log",
            "merge",
        )

        # Name every file by the sha1 of its contents, which is what -merge=1 does and what the
        # nightly workflow uploads, so a file that was already committed keeps its name.
        shutil.rmtree(corpus)
        after = stage_by_content([e.path for e in os.scandir(work / "min") if e.is_file()], corpus)

        print(f"{target:<20} {before:>5} files -> {after:<5}  ({len(found)} from fuzzing went in, "
              f"{staged} distinct)")
        print(f"{'':<20} {coverage_of(target, corpus)}")

    print()
    print(f"crashes, if any: find {FINDINGS} -path '*/crashes/id:*'")
    print("now check the suite still passes, then commit:")
    print(f"    meson test -C {BUILD_LIBFUZZER}")
    print("    git add data/fuzz && git commit")


def duration(text: str) -> int:
    """Seconds, written the way a fuzzing session is talked about: 5m, 90s, 1h30m.

    Every number needs its unit. A bare --idle 1 reads as one minute to whoever writes it and as
    one second to a parser that has to pick a default, and picking wrong is quiet: the sweep moves
    on almost at once, which looks like a fuzzer that will not fuzz rather than like a typo.
    """
    wanted = text.strip().lower()
    parts = re.findall(r"(\d+)([hms])", wanted)
    if not parts or "".join(n + u for n, u in parts) != wanted:
        raise argparse.ArgumentTypeError(
            f"not a duration: {text!r}. Every number needs a unit, so 1m for a minute and 1s for "
            f"a second; 90s and 1h30m work too")
    seconds = sum(int(n) * {"h": 3600, "m": 60, "s": 1}[u] for n, u in parts)
    if seconds <= 0:
        raise argparse.ArgumentTypeError("duration must be more than zero")
    return seconds


def main():
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n", 1)[0],
        epilog="Stopping is safe at any point: afl-fuzz writes every input it keeps to disk as it "
               "finds it, so Ctrl-C loses nothing, and run and sweep resume rather than restart.",
    )
    commands = parser.add_subparsers(dest="command", required=True)
    for name, help_text in (("run", "fuzz every target at once, until Ctrl-C"),
                            ("sweep", "fuzz each target in turn until it stops finding anything"),
                            ("minimize", "fold the findings into data/fuzz and shrink it")):
        command = commands.add_parser(name, help=help_text)
        # Not argparse choices: with nargs="*" that also validates the empty default, and the
        # error it produces for a typo names the whole list rather than the word that was wrong.
        command.add_argument("targets", nargs="*",
                             help=f"which targets to work on (default: all of {', '.join(ALL_TARGETS)})")
        if name == "sweep":
            command.add_argument("--idle", type=duration, default="5m", metavar="DURATION",
                                 help="move on once a target has gone this long without a new "
                                      "find (default: 5m)")

    args = parser.parse_args()
    os.chdir(ROOT)
    unknown = [t for t in args.targets if t not in ALL_TARGETS]
    if unknown:
        die(f"no such target: {', '.join(unknown)}. Known targets: {', '.join(ALL_TARGETS)}")
    targets = args.targets or list(ALL_TARGETS)
    if args.command == "run":
        cmd_run(targets)
    elif args.command == "sweep":
        cmd_sweep(targets, args.idle)
    else:
        cmd_minimize(targets)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        # Ctrl-C anywhere other than while watching the fuzzer -- during a build, during
        # afl-cmin -- is an ordinary way to stop, not something to print a traceback for.
        print("\ninterrupted", file=sys.stderr)
        sys.exit(130)
