#!/usr/bin/env python3
"""Regression tests for scripts/fuzz_afl.py.

    scripts/test_fuzz_afl.py            # run them
    scripts/test_fuzz_afl.py -v         # ... naming each one

Every test here is hermetic: no afl-fuzz, no meson, no network, and nothing written outside a
temporary directory. That is deliberate -- the point is that a refactoring of fuzz_afl.py can be
checked in a second, on a machine that has never had AFL++ installed, which is also what lets CI
run them.

The cost of that is what they cannot cover: whether afl-fuzz actually honours -b, whether its
screen draws, whether a real Ctrl-C reaches a real fleet. Those were checked by hand against real
AFL++ and are recorded in the commit messages. What is covered here is everything that is a
property of this script rather than of AFL: which instances get started on which cores, how the
queue is counted, how inputs are deduplicated, and every argument-parsing rule.

Most of these exist because the mistake they check for was made once already. Where that is so,
the test says which.
"""

from __future__ import annotations

import importlib.util
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parent / "fuzz_afl.py"


def load_script():
    """A fresh module object, so a test that patches a global cannot leak into the next one."""
    spec = importlib.util.spec_from_file_location("fuzz_afl_under_test", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_topology(root: Path, siblings_of: dict[int, str]) -> Path:
    """A fake /sys/devices/system/cpu describing a machine this one is not."""
    for cpu, siblings in siblings_of.items():
        topology = root / f"cpu{cpu}" / "topology"
        topology.mkdir(parents=True, exist_ok=True)
        (topology / "thread_siblings_list").write_text(siblings + "\n")
    return root


def smt_machine(physical: int, threads: int = 2, adjacent: bool = False) -> dict[int, str]:
    """The two enumerations real machines use: core n's siblings are (2n, 2n+1), or (n, n+physical)."""
    layout = {}
    for core in range(physical):
        if adjacent:
            siblings = [core * threads + t for t in range(threads)]
        else:
            siblings = [core + physical * t for t in range(threads)]
        for cpu in siblings:
            layout[cpu] = ",".join(str(s) for s in siblings)
    return layout


class FakeProcess:
    """Enough of subprocess.Popen for the fleet logic, recording how it was asked to stop."""

    def __init__(self, argv=None, exits_with: int = 0, alive: bool = True):
        self.argv = list(argv or [])
        self.pid = 4242
        self.returncode = exits_with
        self._alive = alive
        self.terminated = False
        self.killed = False

    def poll(self):
        return None if self._alive else self.returncode

    def wait(self, timeout=None):
        self._alive = False
        return self.returncode

    def terminate(self):
        self.terminated = True
        self._alive = False

    def kill(self):
        self.killed = True
        self._alive = False


class ScriptTestCase(unittest.TestCase):
    """Loads a private copy of the script and gives it a temporary world to act on."""

    def setUp(self):
        self.fuzz = load_script()
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.fuzz.FINDINGS = self.tmp / "fuzz-findings"
        self.fuzz.CPU_TOPOLOGY = self.tmp / "no-topology-here"

        # Every test runs somewhere disposable. The script reaches data/fuzz/<target> by a
        # relative path and cmd_minimize rewrites it in place, so a test that gets as far as
        # calling it -- or a mutation that removes a guard stopping it -- would otherwise delete
        # the committed corpus of whoever ran the tests. That is not hypothetical: it happened
        # while checking that these tests fail when they should.
        here = os.getcwd()
        os.chdir(self.tmp)
        self.addCleanup(os.chdir, here)

    def use_topology(self, siblings_of: dict[int, str]):
        self.fuzz.CPU_TOPOLOGY = write_topology(self.tmp / "sys-cpu", siblings_of)

    def record_instances(self):
        """Replace start_instance with a recorder, and report what would have been launched."""
        started = []

        def fake_start(target, index, count, out, cpu, ui=False):
            started.append({"target": target, "index": index, "count": count,
                            "out": out, "cpu": cpu, "ui": ui})
            stats = Path(out) / self.fuzz.instance_name(index) / "fuzzer_stats"
            return self.fuzz.Instance(FakeProcess(), None if ui else Path("/dev/null"), stats)

        self.fuzz.start_instance = fake_start
        self.fuzz.wait_until_started = lambda instances: None
        self.fuzz.preflight = lambda: None
        self.fuzz.ensure_fuzzing_builds = lambda targets: None
        return started


class TestCoreSelection(ScriptTestCase):
    """One instance per physical core, and never two on hyperthread siblings of one core."""

    def test_siblings_adjacent(self):
        self.use_topology(smt_machine(4, adjacent=True))
        self.assertEqual(self.fuzz.fuzzing_cpus(), [0, 2, 4, 6])

    def test_siblings_split(self):
        self.use_topology(smt_machine(4, adjacent=False))
        self.assertEqual(self.fuzz.fuzzing_cpus(), [0, 1, 2, 3])

    def test_without_hyperthreading_every_cpu_is_a_core(self):
        self.use_topology({cpu: str(cpu) for cpu in range(4)})
        self.assertEqual(self.fuzz.fuzzing_cpus(), [0, 1, 2, 3])

    def test_four_way_smt(self):
        self.use_topology(smt_machine(4, threads=4))
        self.assertEqual(self.fuzz.fuzzing_cpus(), [0, 1, 2, 3])

    def test_large_machine_counts_cores_not_threads(self):
        self.use_topology(smt_machine(64))
        self.assertEqual(len(self.fuzz.fuzzing_cpus()), 64)
        self.assertEqual(self.fuzz.core_count(self.fuzz.fuzzing_cpus()), 64)

    def test_unreadable_topology_falls_back(self):
        """macOS and anything else without sysfs: let afl-fuzz place the instances itself."""
        self.assertIsNone(self.fuzz.fuzzing_cpus())
        self.assertEqual(self.fuzz.core_count(None), os.cpu_count() or 1)

    def test_cpus_without_the_file_are_skipped_not_fatal(self):
        root = write_topology(self.tmp / "partial", {0: "0,2", 2: "0,2"})
        (root / "cpu4").mkdir(parents=True)  # present, but no topology/ underneath
        self.fuzz.CPU_TOPOLOGY = root
        self.assertEqual(self.fuzz.fuzzing_cpus(), [0])


class TestInstanceNaming(ScriptTestCase):
    """The -M/-S name and the fuzzer_stats path have to agree.

    They are what wait_until_started watches; when they drifted apart, every instance looked stuck
    until the 60s STARTUP_TIMEOUT elapsed, which reads as "AFL is slow" rather than as a typo.
    """

    def test_main_and_secondaries(self):
        self.assertEqual(self.fuzz.instance_name(0), "main")
        self.assertEqual(self.fuzz.instance_name(1), "s1")
        self.assertEqual(self.fuzz.instance_name(7), "s7")

    def test_argv_role_matches_instance_name(self):
        out = self.tmp / "out"
        for index in range(4):
            argv = self.fuzz.afl_argv("fuzz_api", index, 4, out, None)
            flag = "-M" if index == 0 else "-S"
            self.assertEqual(argv[1], flag)
            self.assertEqual(argv[2], self.fuzz.instance_name(index))

    def test_stats_path_matches_instance_name(self):
        out = self.tmp / "out"
        (out / "main").mkdir(parents=True)
        with mock.patch.object(self.fuzz.subprocess, "Popen", lambda *a, **k: FakeProcess()):
            instance = self.fuzz.start_instance("fuzz_api", 0, 1, out, None, ui=True)
        self.assertEqual(instance.stats, out / "main" / "fuzzer_stats")


class TestSanitizerPlacement(ScriptTestCase):
    """Exactly one instance carries the sanitizers, unless there is only one to carry them."""

    def test_single_instance_keeps_them(self):
        self.assertEqual(self.fuzz.sanitized_index(1), 0)

    def test_several_instances_put_them_on_s1(self):
        for count in (2, 4, 16, 64):
            self.assertEqual(self.fuzz.sanitized_index(count), 1)

    def test_argv_picks_the_matching_build(self):
        out = self.tmp / "out"
        sanitized = self.fuzz.afl_argv("fuzz_api", 1, 4, out, None)[-1]
        fast = self.fuzz.afl_argv("fuzz_api", 2, 4, out, None)[-1]
        self.assertIn(str(self.fuzz.BUILD_AFL), sanitized)
        self.assertIn(str(self.fuzz.BUILD_AFL_FAST), fast)

    def test_lone_instance_uses_the_sanitized_build(self):
        argv = self.fuzz.afl_argv("fuzz_api", 0, 1, self.tmp / "out", None)
        self.assertIn(str(self.fuzz.BUILD_AFL), argv[-1])


class TestAflArgv(ScriptTestCase):
    def test_pins_when_given_a_cpu(self):
        argv = self.fuzz.afl_argv("fuzz_api", 1, 4, self.tmp / "out", 7)
        self.assertIn("-b", argv)
        self.assertEqual(argv[argv.index("-b") + 1], "7")

    def test_does_not_pin_when_topology_is_unknown(self):
        argv = self.fuzz.afl_argv("fuzz_api", 1, 4, self.tmp / "out", None)
        self.assertNotIn("-b", argv)

    def test_corpus_and_output(self):
        out = self.tmp / "out"
        argv = self.fuzz.afl_argv("fuzz_string", 0, 1, out, None)
        self.assertEqual(argv[argv.index("-i") + 1], "data/fuzz/fuzz_string")
        self.assertEqual(argv[argv.index("-o") + 1], str(out))
        self.assertEqual(argv[-2], "--")
        self.assertTrue(argv[-1].endswith("/test/fuzz_string"))


class TestPlacement(ScriptTestCase):
    """No two instances on one physical core -- the reason -b is passed at all."""

    def cpus_used(self, started):
        return [job["cpu"] for job in started]

    def test_sweep_uses_every_core_exactly_once(self):
        self.use_topology(smt_machine(16))
        started = self.record_instances()
        self.fuzz.queue_size = lambda out: 0
        with mock.patch("sys.stdout"):
            self.fuzz.sweep_one("fuzz_api", 300)
        cpus = self.cpus_used(started)
        self.assertEqual(len(cpus), 16, "one instance per physical core, not per thread")
        self.assertEqual(sorted(cpus), sorted(set(cpus)), "a core was used twice")

    def test_run_spreads_every_target_over_every_core(self):
        # Sized from the target list rather than to a fixed 16, so that adding a target changes
        # what the machine has to be for the split to come out even, not whether the test passes.
        cores = 4 * len(self.fuzz.ALL_TARGETS)
        self.use_topology(smt_machine(cores))
        started = self.record_instances()
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(list(self.fuzz.ALL_TARGETS))
        cpus = self.cpus_used(started)
        self.assertEqual(len(cpus), cores, "an even split leaves no core idle")
        self.assertEqual(sorted(cpus), sorted(set(cpus)), "a core was used twice")

    def test_run_with_one_target_uses_every_core(self):
        self.use_topology(smt_machine(8))
        started = self.record_instances()
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(["fuzz_api"])
        self.assertEqual(len(self.cpus_used(started)), 8)
        self.assertEqual(sorted(self.cpus_used(started)), list(range(8)))

    def test_uneven_split_never_doubles_up(self):
        self.use_topology(smt_machine(16))
        started = self.record_instances()
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(["fuzz_api", "fuzz_string", "fuzz_replace_map"])
        cpus = self.cpus_used(started)
        self.assertEqual(len(cpus), 15, "16 // 3 == 5 each; the spare core is left alone")
        self.assertEqual(sorted(cpus), sorted(set(cpus)))

    def test_more_targets_than_cores_shares_rather_than_dropping_targets(self):
        """Documented behaviour, not an accident: refusing to fuzz two of them is worse."""
        self.use_topology(smt_machine(2))
        started = self.record_instances()
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(list(self.fuzz.ALL_TARGETS))
        cpus = self.cpus_used(started)
        self.assertEqual(len(cpus), len(self.fuzz.ALL_TARGETS), "every target still gets an instance")
        self.assertEqual(set(cpus), {0, 1})

    def test_unknown_topology_pins_nothing(self):
        started = self.record_instances()
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(["fuzz_api"])
        self.assertTrue(all(job["cpu"] is None for job in started))

    def test_the_watched_target_is_the_one_with_the_screen(self):
        self.use_topology(smt_machine(4))
        started = self.record_instances()
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(["fuzz_api", "fuzz_string"])
        on_screen = [job for job in started if job["ui"]]
        self.assertEqual(len(on_screen), 1, "exactly one instance draws a screen")
        self.assertEqual(on_screen[0]["target"], "fuzz_api")
        self.assertEqual(on_screen[0]["index"], 0)


class TestFleetCleanup(ScriptTestCase):
    """Nothing may be left fuzzing after the command returns."""

    def test_run_stops_every_instance(self):
        self.use_topology(smt_machine(4))
        started = self.record_instances()
        processes = []
        original = self.fuzz.start_instance

        def remember(*args, **kwargs):
            instance = original(*args, **kwargs)
            processes.append(instance.process)
            return instance

        self.fuzz.start_instance = remember
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(["fuzz_api"])
        self.assertEqual(len(processes), 4)
        self.assertTrue(all(p.poll() is not None for p in processes), "an instance was left running")

    def test_run_stops_the_fleet_even_if_the_foreground_fails_to_start(self):
        """cmd_run had no try/finally: a raising Popen orphaned every background instance."""
        self.use_topology(smt_machine(4))
        started = self.record_instances()
        processes = []
        original = self.fuzz.start_instance

        def fail_on_foreground(target, index, count, out, cpu, ui=False):
            if ui:
                raise OSError("cannot exec afl-fuzz")
            instance = original(target, index, count, out, cpu, ui)
            processes.append(instance.process)
            return instance

        self.fuzz.start_instance = fail_on_foreground
        with mock.patch("sys.stdout"), self.assertRaises(OSError):
            self.fuzz.cmd_run(["fuzz_api"])
        self.assertEqual(len(processes), 3)
        self.assertTrue(all(p.terminated for p in processes), "background instances were orphaned")

    def test_sweep_stops_every_instance(self):
        self.use_topology(smt_machine(4))
        self.record_instances()
        processes = []
        original = self.fuzz.start_instance

        def remember(*args, **kwargs):
            instance = original(*args, **kwargs)
            processes.append(instance.process)
            return instance

        self.fuzz.start_instance = remember
        self.fuzz.queue_size = lambda out: 0
        with mock.patch("sys.stdout"):
            self.fuzz.sweep_one("fuzz_api", 300)
        self.assertTrue(all(p.poll() is not None for p in processes))

    def test_a_watcher_that_raises_stops_instead_of_fuzzing_forever(self):
        """It used to end the thread and nothing else, leaving the sweep running with no decision
        being made and nothing on screen to say so."""
        self.use_topology(smt_machine(4))
        self.record_instances()

        def explode(out):
            raise RuntimeError("boom")

        self.fuzz.queue_size = explode
        with mock.patch("sys.stdout"):
            outcome = self.fuzz.sweep_one("fuzz_api", 300)
        self.assertIn("stopped watching", outcome)
        self.assertIn("boom", outcome)


class TestQueueCounting(ScriptTestCase):
    """The idle clock is driven off these, so what counts as a find is decided here."""

    def make_queue(self, instances: dict[str, int]) -> Path:
        out = self.tmp / "target-out"
        for name, count in instances.items():
            queue = out / name / "queue"
            queue.mkdir(parents=True)
            for i in range(count):
                (queue / f"id:{i:06d},src:000000,time:1,execs:1,op:havoc").write_text("x")
        return out

    def test_counts_entries_across_instances(self):
        out = self.make_queue({"main": 3, "s1": 4, "s2": 5})
        self.assertEqual(self.fuzz.queue_size(out), 12)

    def test_ignores_the_state_directory(self):
        """afl-fuzz keeps queue/.state/ full of files named like queue entries. Counting those
        would inflate every reading, and worse, .state is rewritten as afl-fuzz works, so a target
        that had stopped finding anything would keep looking busy. This is the test that fails if
        the queue is ever walked recursively."""
        out = self.make_queue({"main": 2})
        state = out / "main" / "queue" / ".state"
        state.mkdir()
        for i in range(9):
            (state / f"id:{i:06d},src:000000,time:1,execs:1,op:havoc").write_text("x")
        self.assertEqual(self.fuzz.queue_size(out), 2)

    def test_a_directory_named_like_an_entry_is_not_counted(self):
        out = self.make_queue({"main": 2})
        (out / "main" / "queue" / "id:999999,dir").mkdir()
        self.assertEqual(self.fuzz.queue_size(out), 2)

    def test_ignores_stray_files_and_instances_without_a_queue(self):
        out = self.make_queue({"main": 2})
        (out / "sweep.log").write_text("not a queue entry\n")
        (out / "afl-1.log").write_text("nor this\n")
        (out / "s1").mkdir()
        (out / "main" / "queue" / "README.txt").write_text("skipped: no id: prefix\n")
        self.assertEqual(self.fuzz.queue_size(out), 2)

    def test_missing_directory_is_zero_not_an_exception(self):
        self.assertEqual(self.fuzz.queue_size(self.tmp / "never-created"), 0)

    def test_matches_a_glob_reference(self):
        """scandir replaced Path.glob for speed; it must still answer the same."""
        out = self.make_queue({"main": 7, "s1": 11, "s2": 0})
        (out / "main" / "queue" / ".state").mkdir()
        reference = sum(1 for _ in out.glob("*/queue/id:*"))
        self.assertEqual(self.fuzz.queue_size(out), reference)


class TestStaging(ScriptTestCase):
    """Deduplication by content is what keeps afl-cmin from executing the same input many times."""

    def sources(self, contents: list[bytes]) -> list[str]:
        src = self.tmp / "src"
        src.mkdir(exist_ok=True)
        paths = []
        for i, blob in enumerate(contents):
            path = src / f"input-{i}"
            path.write_bytes(blob)
            paths.append(str(path))
        return paths

    def test_identical_contents_collapse(self):
        sources = self.sources([b"same", b"same", b"same", b"different"])
        dest = self.tmp / "staged"
        self.assertEqual(self.fuzz.stage_by_content(sources, dest), 2)

    def test_named_by_sha1_of_contents(self):
        import hashlib
        sources = self.sources([b"hello"])
        dest = self.tmp / "staged"
        self.fuzz.stage_by_content(sources, dest)
        self.assertTrue((dest / hashlib.sha1(b"hello").hexdigest()).is_file())

    def test_every_distinct_input_survives(self):
        sources = self.sources([bytes([i]) * 32 for i in range(200)])
        dest = self.tmp / "staged"
        self.assertEqual(self.fuzz.stage_by_content(sources, dest), 200)

    def test_falls_back_to_copying_across_filesystems(self):
        """FINDINGS can be pointed somewhere else, where hardlinking cannot work."""
        sources = self.sources([b"a", b"b"])
        dest = self.tmp / "staged"

        def no_links(src, dst):
            raise OSError("Invalid cross-device link")

        with mock.patch.object(self.fuzz.os, "link", no_links):
            self.assertEqual(self.fuzz.stage_by_content(sources, dest), 2)
        self.assertEqual((dest / [p.name for p in dest.iterdir()][0]).read_bytes() in (b"a", b"b"), True)

    def test_empty_input_is_not_an_error(self):
        self.assertEqual(self.fuzz.stage_by_content([], self.tmp / "staged"), 0)


class TestDuration(ScriptTestCase):
    """--idle 1 meant one second and moved on almost at once, which read as a broken fuzzer."""

    def test_units(self):
        for text, seconds in (("1s", 1), ("90s", 90), ("1m", 60), ("5m", 300),
                              ("1h", 3600), ("1h30m", 5400), ("2h15m30s", 8130)):
            self.assertEqual(self.fuzz.duration(text), seconds, text)

    def test_a_bare_number_is_refused(self):
        import argparse
        for text in ("1", "5", "300"):
            with self.assertRaises(argparse.ArgumentTypeError, msg=text) as caught:
                self.fuzz.duration(text)
            self.assertIn("unit", str(caught.exception))

    def test_nonsense_is_refused(self):
        import argparse
        for text in ("", "abc", "5x", "m", "-5", "1d", "1m2x"):
            with self.assertRaises(argparse.ArgumentTypeError, msg=text):
                self.fuzz.duration(text)

    def test_zero_is_refused(self):
        import argparse
        with self.assertRaises(argparse.ArgumentTypeError):
            self.fuzz.duration("0s")


class TestCommandLine(ScriptTestCase):
    def dispatch(self, argv: list[str]) -> dict:
        seen = {}
        self.fuzz.cmd_run = lambda targets: seen.update(command="run", targets=targets)
        self.fuzz.cmd_sweep = lambda targets, idle: seen.update(command="sweep", targets=targets,
                                                                idle=idle)
        self.fuzz.cmd_minimize = lambda targets: seen.update(command="minimize", targets=targets)
        with mock.patch.object(sys, "argv", ["fuzz_afl.py", *argv]), mock.patch.object(os, "chdir"):
            self.fuzz.main()
        return seen

    def test_sweep_defaults_to_five_minutes_and_every_target(self):
        seen = self.dispatch(["sweep"])
        self.assertEqual(seen["idle"], 300)
        self.assertEqual(seen["targets"], list(self.fuzz.ALL_TARGETS))

    def test_idle_is_converted_to_seconds(self):
        self.assertEqual(self.dispatch(["sweep", "--idle", "90s"])["idle"], 90)
        self.assertEqual(self.dispatch(["sweep", "--idle", "1h"])["idle"], 3600)

    def test_each_command_dispatches(self):
        for name in ("run", "sweep", "minimize"):
            self.assertEqual(self.dispatch([name])["command"], name)

    def test_named_targets_are_kept_in_order(self):
        seen = self.dispatch(["run", "fuzz_string", "fuzz_api"])
        self.assertEqual(seen["targets"], ["fuzz_string", "fuzz_api"])

    def test_a_typo_in_a_target_is_refused_rather_than_fuzzed(self):
        with self.assertRaises(SystemExit) as caught:
            with mock.patch("sys.stderr"):
                self.dispatch(["run", "fuzz_appi"])
        self.assertEqual(caught.exception.code, 1)

    def test_no_command_is_an_error(self):
        with self.assertRaises(SystemExit), mock.patch("sys.stderr"):
            self.dispatch([])


class TestPreflight(ScriptTestCase):
    """Both checks belong to running AFL at all; sweep used to skip them and fail obscurely."""

    def test_missing_afl_fuzz_is_reported(self):
        with mock.patch.object(self.fuzz.shutil, "which", return_value=None):
            with self.assertRaises(SystemExit), mock.patch("sys.stderr"):
                self.fuzz.preflight()

    def test_run_checks_before_starting_anything(self):
        self.use_topology(smt_machine(2))
        self.record_instances()
        checked = []
        self.fuzz.preflight = lambda: checked.append(True)
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_run(["fuzz_api"])
        self.assertEqual(len(checked), 1, "cmd_run stopped running the preflight checks")

    def test_sweep_checks_before_starting_anything(self):
        self.record_instances()
        checked = []
        self.fuzz.preflight = lambda: checked.append(True)
        self.fuzz.sweep_one = lambda target, idle: f"{target}: stubbed"
        with mock.patch("sys.stdout"):
            self.fuzz.cmd_sweep(["fuzz_api"], 300)
        self.assertEqual(len(checked), 1, "cmd_sweep stopped running the preflight checks")

    def test_minimize_refuses_without_afl_cmin(self):
        """Before building anything, and before touching data/fuzz: cmd_minimize rewrites the
        corpus in place, so it has to fail while that is still cheap and reversible. Asserting
        only SystemExit would pass whatever went wrong, including a meson failure much later."""
        built = []
        self.fuzz.ensure_built = lambda *args, **kwargs: built.append(args)
        with mock.patch.object(self.fuzz.shutil, "which", return_value=None):
            with self.assertRaises(SystemExit), mock.patch("sys.stderr"):
                self.fuzz.cmd_minimize(["fuzz_api"])
        self.assertEqual(built, [], "it started building before checking for afl-cmin")

    def test_core_pattern_piped_to_a_crash_handler_is_refused(self):
        piped = self.tmp / "core_pattern"
        piped.write_text("|/usr/share/apport/apport %p\n")
        real_path = self.fuzz.Path

        class PathThatRedirectsCorePattern(type(Path())):
            def __new__(cls, *args):
                if args and str(args[0]) == "/proc/sys/kernel/core_pattern":
                    return real_path(piped)
                return real_path(*args)

        with mock.patch.object(self.fuzz.shutil, "which", return_value="/usr/bin/afl-fuzz"), \
                mock.patch.object(self.fuzz, "Path", PathThatRedirectsCorePattern), \
                mock.patch("sys.stderr"):
            with self.assertRaises(SystemExit):
                self.fuzz.preflight()


class TestStartupSupervision(ScriptTestCase):
    """A background instance that dies at startup must stop the run, not leave cores idle."""

    def test_dead_instances_are_reported_and_fatal(self):
        log = self.tmp / "afl-1.log"
        log.write_text("PROGRAM ABORT : Program './x' is a shell script\n")
        dead = self.fuzz.Instance(FakeProcess(alive=False, exits_with=1), log,
                                  self.tmp / "never" / "fuzzer_stats")
        with self.assertRaises(SystemExit), mock.patch("sys.stderr"):
            self.fuzz.wait_until_started([dead])

    def test_a_started_instance_returns_promptly(self):
        stats = self.tmp / "main" / "fuzzer_stats"
        stats.parent.mkdir(parents=True)
        stats.write_text("start_time : 1\n")
        alive = self.fuzz.Instance(FakeProcess(alive=True), self.tmp / "afl-1.log", stats)
        self.fuzz.wait_until_started([alive])  # must not raise, must not wait out STARTUP_TIMEOUT

    def test_no_instances_is_not_a_failure(self):
        self.fuzz.wait_until_started([])


if __name__ == "__main__":
    unittest.main(verbosity=1)
