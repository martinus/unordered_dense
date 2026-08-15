#!/usr/bin/env python3
"""Mutation testing for unordered_dense.

Coverage says a line ran. This says something would have noticed it misbehaving.
It breaks `unordered_dense.h`, rebuilds, runs the suite, and asks whether anything
went red. What nothing notices is a hole in the tests.

Two ways to use it, and the first is the everyday one.

**Put specific bugs back.** The check that decides whether a new test is worth
keeping: break the thing it covers and confirm it goes red. Bugs are independent,
so a batch runs across lanes at once rather than one rebuild after another.

    mutate.py --bugs bugs.txt                  # a file of them, in parallel
    mutate.py --replace OLD NEW                # one, repeatable
    mutate.py --reverse HEAD                   # undo a fix, keep today's tests
    mutate.py --bugs bugs.txt --reuse          # again, against an edited test

`--reuse` keeps the lanes and syncs only what changed next time, which saves the
copying and the configuring - not the compiling, see below.

A bug file is a name and a block each. Old text must match exactly once, and a
block that does not apply stops the run - otherwise a typo substitutes nothing,
the suite stays green, and the report blames your tests for it:

    # the erase moved the last element without fixing up its bucket
    <<<
        auto mh = mixed_hash(get_key(m_values.back()));
    ===
        auto mh = mixed_hash(get_key(*it));
    >>>

Bug files worth keeping live in `scripts/mutate/bugs/`. They are snapshots
against the code as it was, so one that stops applying is not a failure - it is
the tool saying that part has been rewritten and the questions need re-deriving.

**Or sweep for holes you have not thought of**, mutating one token at a time:

    mutate.py --diff                           # whatever is uncommitted
    mutate.py --diff HEAD~1                    # only what that change touched
    mutate.py --lines 1200-1260,1300           # a function, or a scattering
    mutate.py                                  # the whole header
    mutate.py --diff --dry-run                 # how many, and how long

`--diff` is the everyday one, and it measures from the merge base: on a branch
that has not caught up, comparing against a ref's tip sweeps every line main
moved on without you as though it were yours.

`--deletions` adds a second operator that removes whole statements. It is worth
knowing that nearly every bug in `bugs/invariants.txt` is some form of "the code
forgot to do this" -- the shift down that never happens, the pop_back that is
skipped -- and that none of them is one token, so the token sweep cannot reach
any of them. It roughly doubles the count; the ones that cannot compile cost the
pre-filter's half second rather than a rebuild.

Two kinds of mutant are never generated, because nothing could ever catch them.
Comments, string literals and preprocessor lines are not code and are skipped by
the lexer. `std::enable_if_t<..., bool> = true>` is skipped too: the parameter
exists so the substitution has somewhere to fail and nothing reads its value, so
flipping it is 47 rebuilds to prove that nothing happened. A third kind is found
rather than predicted -- a mutant in a branch this configuration does not
compile is dropped once the lanes exist and the preprocessor has been asked
which lines survived, and the run says which lines those were.

The two compose, and a change is best asked both questions at once - the named
bugs the tests were written for, and the sweep for what nobody thought to ask:

    mutate.py --bugs bugs.txt --lines 1200-1260 --reuse

Nothing is scored until the suite is green repeatedly, because a flaky test
counted as a kill inflates the number in the flattering direction. The working
tree is never touched: every build happens in a throwaway copy, so a run that
dies half way cannot leave a mutated header behind, and two runs at once get
their own copies rather than deleting each other's.

Every mutant ends in one of five verdicts, and the difference matters.
`compiler` means the build refused it - real protection, but not your tests
doing the work. `caught` means an assertion failed, which is the number worth
moving. `hang` means it ran forever; a mutated probe step does that rather than
failing. `oom` means it asked for more memory than it was allowed, which is what
a mutated growth policy does. `survived` means nothing noticed, which is the
answer you are looking for. A sixth, `error`, means this tool fell over on that
mutant; it is left out of the score rather than guessed at, and the rest of the
run carries on regardless.

Each lane runs inside a cgroup with a memory cap, because `oom` has to be that
mutant's verdict and not the machine's problem: uncapped, one insert asking for
a terabyte ends with the kernel choosing a victim, and it is as likely to be
another lane, the ninja driving one, or this script. `--memory-limit` sets it,
the default is the smaller of a lane's share of the machine and a gigabyte per
ninja job, and the run says which up front. Where no cgroup can be had - another
init, no session bus, an undelegated container - the run says that instead of
pretending.

The lanes themselves are the other half of that: ~90 MB each, in a workdir that
defaults to /tmp, which on most current distributions is a tmpfs - so `--lanes`
buys memory as much as it buys parallelism. The run says how much room it is
about to take and whether that room is RAM, and refuses before copying rather
than half way through. Core dumps are turned off for the same reason: a crashing
mutant is an ordinary verdict here, and each one would otherwise leave ~30 MB in
a lane that is about to be deleted.

**What a mutant costs here is one full rebuild of the test binary.** Every one of
the ~90 translation units includes the header, so there is no such thing as an
incremental mutant build and ccache cannot help either - each mutant is a
preprocessed source nothing has ever seen. That is around 100 CPU-seconds of
compiling against 3 of running the suite, which is why the lanes default to one
job each (the machine is already full) and why a single named bug instead gets
all the cores to itself. It is also why the -fsyntax-only pre-filter earns its
keep: half a second to reject what would otherwise cost a full rebuild.
"""

import argparse
import bisect
import concurrent.futures
import contextlib
import filecmp
import json
import os
import queue
import random
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# The library is one header, so that is the default target; --file overrides it.
HEADER = os.path.join("include", "ankerl", "unordered_dense.h")

# The TU the syntax pre-filter compiles when the header is what is being mutated.
# It wants to be a file that *instantiates* the map rather than merely including
# it - a mutation inside a member template is a parse error either way, but one
# that only breaks type checking is not seen until something instantiates it.
# fuzz_api touches most of the API surface for about the same half second as any
# other unit file.
SYNTAX_TU = os.path.join("test", "unit", "fuzz_api.cpp")

# What meson builds and what the doctest binary is called inside a lane.
TEST_BINARY = os.path.join("test", "udm-test")

# Copied per lane, so this is about keeping the copy to the sources and the fuzz
# corpora. `subprojects` deliberately stays: it holds the already-downloaded
# doctest and the packagecache, and a lane without them tries to fetch a release
# tarball per lane - slow where that works at all and fatal where it does not.
LANE_IGNORE = shutil.ignore_patterns(".git", "__pycache__", ".cache", ".ccache",
                                     ".vscode", "fuzz-findings", "*.pcm", "*.o",
                                     "a.out", "compile_commands.json")

# Build directories, which only exist at the top level. Matched there rather than
# everywhere, because `build*` as a global pattern also eats `scripts/build.py`.
ROOT_IGNORE = ("builddir", "build", "_build")

# Wall seconds per mutant, and this differs from a single-TU project in a way
# worth stating: the cost is ~100 CPU-seconds of compiling that no amount of
# lanes creates or removes, plus a link and a suite run that are serial inside
# whichever lane they happen in. So the compiling is divided by the *machine*
# and only the tail is divided by the lanes.
#
# Measured on a 32-thread machine, g++ at -O0: 99 CPU-seconds to rebuild all 90
# TUs and 3.4 to run the suite. Mutants the pre-filter rejects cost half a second
# instead of all of that, and the estimate does not model them - so --dry-run
# reads high on a sweep, which is the direction to be wrong in.
CPU_SECONDS_PER_MUTANT = 100.0
TAIL_SECONDS_PER_MUTANT = 4.0
SECONDS_OF_SETUP = 20.0
SECONDS_OF_SETUP_PER_LANE = 1.5

VERDICTS = ("caught", "compiler", "hang", "oom", "survived", "error")

MIB = 1024 * 1024
GIB = 1024 * MIB


def lane_ignore(directory, names):
    skip = list(LANE_IGNORE(directory, names))
    if os.path.abspath(directory) == REPO:
        skip.extend(ROOT_IGNORE)
    return skip


def sync_tree(src, dst):
    """Bring an existing lane back in line with the repo, copying only what
    actually changed.

    The point is the mtimes. A wholesale re-copy would restamp every source and
    cost a full rebuild in every lane, which is most of what reuse was meant to
    save - so a file that matches is left alone, and ninja rebuilds exactly what
    the working tree touched since last time.

    Sameness is decided by *content*, and a file that differs is stamped with
    the current time rather than the repo's. Both halves are load-bearing.
    Comparing mtimes would call the previous run's mutated header a change on
    every file in the tree; and copying the repo's older mtime onto it would
    leave the lane's object files looking newer than their source, so ninja
    would build nothing and the suite would run against the last mutant."""
    for root, dirs, files in os.walk(src):
        skip = set(lane_ignore(root, dirs + files))
        dirs[:] = [d for d in dirs if d not in skip]
        rel = os.path.relpath(root, src)
        target = dst if "." == rel else os.path.join(dst, rel)
        os.makedirs(target, exist_ok=True)
        wanted = set()
        for name in files:
            if name in skip:
                continue
            wanted.add(name)
            source, copy = os.path.join(root, name), os.path.join(target, name)
            if os.path.exists(copy) and filecmp.cmp(source, copy, shallow=False):
                continue
            shutil.copyfile(source, copy)
            os.utime(copy, None)
        # A file deleted from the repo has to go, or it keeps being compiled.
        # Only files: the lane's build directory is a directory and is not in
        # the source tree, so it is never a candidate.
        for name in os.listdir(target):
            stale = os.path.join(target, name)
            if os.path.isfile(stale) and name not in wanted:
                os.remove(stale)


def estimate_seconds(count, lanes, cores):
    lanes, cores = max(1, lanes), max(1, cores)
    return (SECONDS_OF_SETUP + SECONDS_OF_SETUP_PER_LANE * lanes
            + count * (CPU_SECONDS_PER_MUTANT / cores
                       + TAIL_SECONDS_PER_MUTANT / lanes))


def estimate(count, lanes, cores):
    seconds = estimate_seconds(count, lanes, cores)
    if seconds > 5400:
        return "%.1f h" % (seconds / 3600)
    return "%.0f min" % (seconds / 60) if seconds > 90 else "%.0f s" % seconds


# ---------------------------------------------------------------- memory

# Why there is a cap at all, because it is not the obvious one. The suite is
# tiny - all 501 cases green peak at 14 MB - and a single g++ -O0 translation
# unit peaks at ~185 MB. Nothing honest here needs a gigabyte. What needs the
# cap is the mutant: a growth policy, a bucket mask or a shift amount moved by
# one turns an insert into a request for more memory than the machine has, and
# uncapped that ends with the kernel's OOM killer choosing a victim - as likely
# to be another lane, the ninja driving one, or this script as the mutant that
# caused it. One runaway mutant then corrupts every verdict running beside it,
# which is the failure this tool exists to avoid.
#
# The second reason is quieter: lanes are sized off the core count without
# anyone asking what that costs in RAM, and 32 lanes each building is 32
# compilers at once. Capping each lane at its share of the machine makes that
# arithmetic explicit instead of hoping.

# What a build is never capped below, whatever the cap says. Every ninja job is
# a compiler and a translation unit here peaks at ~185 MB, so a cap under that
# would report `oom` for every mutant and blame a growth policy for this tool's
# own arithmetic. Half a gigabyte a job is that figure with room for the link,
# which is the one step holding all of it at once.
MEMORY_PER_JOB = 512 * MIB

SIZE = re.compile(r"^(\d+(?:\.\d+)?)\s*([KMGT]?)(?:i?B)?$", re.IGNORECASE)
SIZE_UNITS = {"": 1, "K": 1024, "M": MIB, "G": GIB, "T": 1024 * GIB}


def parse_size(text):
    """A size like `4G`, `512M` or `0`, in bytes. Bare digits are bytes."""
    m = SIZE.match(text.strip())
    if not m:
        raise argparse.ArgumentTypeError(
            "%r is not a size - write it as 4G, 512M or 0" % text)
    return int(float(m.group(1)) * SIZE_UNITS[m.group(2).upper()])


def human(size):
    if not size:
        return "off"
    return ("%.1f GiB" % (size / GIB)) if size >= GIB else ("%.0f MiB" % (size / MIB))


def total_memory():
    """Bytes this machine will actually hand out, or 0 if it will not say."""
    limits = []
    try:
        limits.append(os.sysconf("SC_PAGE_SIZE") * os.sysconf("SC_PHYS_PAGES"))
    except (ValueError, OSError, AttributeError):
        pass
    # Inside a container the cgroup limit is the real answer and physical RAM is
    # the host's. `memory.max` is readable at this path only when the cgroup
    # namespace root is our own cgroup - which is exactly when that is the case.
    try:
        with open("/sys/fs/cgroup/memory.max", encoding="utf-8") as f:
            limits.append(int(f.read().strip()))
    except (OSError, ValueError):
        pass
    return min(limits) if limits else 0


def default_memory_limit(lanes, jobs, total=None):
    """What one lane may use: the smaller of its share of the machine and what a
    lane can honestly need.

    The share is what keeps the lanes together inside the machine; four fifths
    of it, because the page cache holding ~90 objects per lane is not free
    either. The honest figure is a gigabyte per ninja job - twice the floor a
    build is never capped below, so an ordinary one never comes near it.
    Whichever is smaller is still orders of magnitude above the 14 MB the suite
    itself uses, which is the only number a mutant has to beat to be called
    runaway.
    """
    limits = [max(2, jobs) * 2 * MEMORY_PER_JOB]
    total = total_memory() if total is None else total
    if total:
        limits.append(int(0.8 * total) // max(1, lanes))
    return max(GIB, min(limits))


def build_memory_limit(limit, jobs):
    """The cap a build gets, which is the lane's unless its jobs need more.

    Raising it here rather than raising the lane's keeps the two questions
    apart. What a build needs is known - it is the job count times a compiler -
    and what a mutant needs is the thing being asked about, so an explicit
    `--memory-limit` tightens the second without breaking the first. In
    aggregate nothing is given away either: lanes times jobs is the core count,
    so every lane building at its floor at once is what a plain `ninja` on this
    machine would use anyway.
    """
    return max(limit, jobs * MEMORY_PER_JOB) if limit else 0


def memory_capped(argv, limit):
    """`argv` inside a cgroup that caps it, when there is one to be had.

    `systemd-run --scope` puts the command - and everything it goes on to spawn,
    so ninja's compilers too - into a transient cgroup with MemoryMax set, and
    the kernel stops the lot the moment they exceed it. Swap is pinned to zero
    as well: without that a runaway first fills whatever swap the machine has,
    and on a box already swapping that is minutes of thrashing slowing every
    other lane down before anything is killed at all.

    A scope rather than a service, which is what makes this a wrapper and not a
    different way of running things: the command stays a child of this process,
    so its stdout, its environment, its working directory and its exit status
    all arrive exactly as they would without it.
    """
    if not limit:
        return list(argv)
    return ["systemd-run", "--user", "--scope", "--quiet", "--collect",
            "-p", "MemoryMax=%d" % limit, "-p", "MemorySwapMax=0",
            "--"] + list(argv)


def memory_cap_reason(limit):
    """Why the cap cannot be had here, or "" when it can.

    It needs systemd, a session bus and the memory controller delegated to the
    user - a container, a CI runner or a machine with another init has some
    subset of those. Asking by trying beats testing for the pieces one at a
    time, and it costs one 5 ms scope for the whole run.
    """
    try:
        r = subprocess.run(memory_capped(["true"], limit), capture_output=True,
                           text=True, timeout=60)
    except FileNotFoundError:
        return "systemd-run is not on PATH"
    except (OSError, subprocess.SubprocessError) as e:
        return "systemd-run: %s" % e
    if r.returncode == 0:
        return ""
    # Its own complaint says which piece is missing - "Failed to connect to bus"
    # on a machine with no session, a delegation error on a container - and that
    # is more use than anything this could say in its place.
    said = (r.stderr or r.stdout).strip()
    return said.splitlines()[-1][:100] if said \
        else "systemd-run --user --scope exited %d" % r.returncode


# A cgroup that goes over its limit is stopped, and which signal arrives says
# only which half of the kernel got there first. The kernel OOM-kills the
# offending process outright (-9); when that process was not the one we are
# waiting on, systemd sees a member die and stops the rest of the scope, which
# is a SIGTERM (-15). Nothing this tool runs sends either signal itself, so both
# mean the same thing here: it asked for more than it was allowed.
MEMORY_KILL_SIGNALS = (-9, -15)


def killed_for_memory(proc):
    return proc is not None and proc.returncode in MEMORY_KILL_SIGNALS


def drop_core_dumps():
    """Stop the lanes filling up with cores, which they otherwise do quickly.

    A crashing mutant is an ordinary verdict here - a segfault is one of the
    ways the suite notices - and the default `core_pattern` writes the dump into
    the crashing process's working directory, which is the lane. That is ~30 MB
    of a mutated binary that no longer exists, once per crash, in a directory
    that on most machines is under /tmp and therefore in RAM. Nothing reads
    them: the lane is deleted at the end of the run and the binary they refer to
    was overwritten by the next mutant.

    Lowering a soft limit needs no privilege, and the children inherit it.
    """
    try:
        import resource  # noqa: PLC0415 - POSIX only, and not needed elsewhere
        _, hard = resource.getrlimit(resource.RLIMIT_CORE)
        resource.setrlimit(resource.RLIMIT_CORE, (0, hard))
    except (ImportError, ValueError, OSError):
        pass


LANE_BYTES = 90 * MIB  # ~21 MB of sources and corpora, ~64 MB of build directory


def memory_backed(path):
    """Whether writing to `path` costs RAM rather than disk.

    /tmp is a tmpfs on most current distributions, so the default workdir
    usually is - and a lane there is not free the way a directory normally is.
    """
    try:
        with open("/proc/mounts", encoding="utf-8") as f:
            mounts = [line.split()[1:3] for line in f if len(line.split()) > 2]
    except OSError:
        return False
    path, deepest = os.path.abspath(path), ("", "")
    for point, kind in mounts:
        point = point.replace("\\040", " ")
        under = path == point or path.startswith(point.rstrip("/") + "/")
        if under and len(point) >= len(deepest[0]):
            deepest = (point, kind)
    return deepest[1] in ("tmpfs", "ramfs")


# ---------------------------------------------------------------- lexing

def code_mask(src):
    """True for every byte that is real code.

    False inside comments, string/char/raw-string literals and preprocessor
    directives. Mutating those is the main way a token-level mutator wastes its
    budget: a third of this header is prose, and the `#define` lines hold the
    version macros that a lint job checks separately.
    """
    mask = bytearray(b"\x01" * len(src))

    def blank(start, end):
        end = min(end, len(src))
        mask[start:end] = b"\x00" * max(0, end - start)

    i, n = 0, len(src)
    at_line_start = True
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        if c in " \t":
            i += 1
            continue

        if c == "\n":
            at_line_start = True
            i += 1
            continue

        # preprocessor directive: the whole logical line, continuations included
        if at_line_start and c == "#":
            j = i
            while j < n:
                if src[j] == "\n" and not (j > 0 and src[j - 1] == "\\"):
                    break
                j += 1
            blank(i, j)
            i = j
            continue

        at_line_start = False

        if c == "/" and nxt == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            blank(i, j)
            i = j
            continue

        if c == "/" and nxt == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            blank(i, j)
            i = j
            continue

        # raw string: R"delim( ... )delim"
        if c == "R" and nxt == '"':
            m = re.compile(r'R"([^(]*)\(').match(src, i)
            if m:
                close = ')' + m.group(1) + '"'
                j = src.find(close, m.end())
                j = n if j < 0 else j + len(close)
                blank(i, j)
                i = j
                continue

        if c in '"\'':
            quote = c
            j = i + 1
            while j < n:
                if src[j] == "\\":
                    j += 2
                    continue
                if src[j] == quote:
                    j += 1
                    break
                if src[j] == "\n":  # unterminated, do not run away
                    break
                j += 1
            blank(i, j)
            i = j
            continue

        i += 1

    return mask


# ------------------------------------------------------------- operators

# Longest first, so `<=` is never split into `<`. Shifts, `->` and `::` are left
# alone on purpose: mutating them is a compile error essentially every time, and
# a mutant that cannot build costs a rebuild to tell us nothing. Shifts are the
# painful exclusion here - this header shifts to build the fingerprint and to
# size the bucket array - but `<<` to `>>` is caught by the compiler in the
# places it is not caught by every test at once, and a shift *amount* is a
# number, which is mutated.
OPERATOR_MUTATIONS = [
    ("<<=", []), (">>=", []), ("<<", []), (">>", []), ("->", []), ("::", []),
    ("<=", ["<", ">=", "=="]),
    (">=", [">", "<=", "=="]),
    ("==", ["!="]),
    ("!=", ["=="]),
    ("&&", ["||"]),
    ("||", ["&&"]),
    ("++", ["--"]),
    ("--", ["++"]),
    ("+=", ["-="]),
    ("-=", ["+="]),
    ("*=", ["/="]),
    ("/=", ["*="]),
    ("<", ["<=", ">"]),
    (">", [">=", "<"]),
    ("+", ["-"]),
    ("-", ["+"]),
    ("*", ["/"]),
    ("/", ["*"]),
]

# `# 123 "some/file.h" 1` -- what -E emits when the next line it prints is not
# the one that would follow. The path may be quoted with escapes; nothing here
# has one, and realpath on a mangled path simply fails to match.
LINEMARKER = re.compile(r'^#\s+(\d+)\s+"([^"]*)"')

IDENT = re.compile(r"[A-Za-z_][A-Za-z_0-9]*")
# integer literal, not part of an identifier or a float/exponent
NUMBER = re.compile(r"\b(\d+)([uUlL]*)\b")

WORD_MUTATIONS = {"true": ["false"], "false": ["true"]}

COMPOUND_OPERATOR_TAIL = "=!<>+-*/&|^%"


def nonspace_before(src, offset):
    while offset > 0 and src[offset - 1] in " \t\n":
        offset -= 1
    return (src[offset - 1], offset - 1) if offset > 0 else ("", -1)


def nonspace_after(src, offset):
    n = len(src)
    while offset < n and src[offset] in " \t\n":
        offset += 1
    return src[offset] if offset < n else ""


def is_template_default(src, offset, word):
    """Whether this `true`/`false` is a template parameter's default value.

    `std::enable_if_t<is_map_v<Q>, bool> = true>` is the SFINAE idiom: the
    parameter exists only so the substitution has somewhere to fail, and nothing
    ever reads its value, so flipping it is a mutant that cannot be killed. This
    header has 47 of them and every one costs a full rebuild of ~90 translation
    units to prove that nothing happened.

    Recognised by shape rather than by looking for `enable_if_t`, because the
    idiom is the default value in a `>`-terminated list and not any particular
    trait: preceded by an `=` that is an assignment rather than the tail of `==`
    or `<=`, and followed by the `>` that closes the template parameter list.
    """
    before, at = nonspace_before(src, offset)
    if "=" != before:
        return False
    # Only an *adjacent* character makes the `=` the tail of `==`, `<=` or `>=`.
    # The one that closes `bool>` sits a space away and is the usual sight here,
    # so testing for it without asking about the space rejects every real case.
    prev, prev_at = nonspace_before(src, at)
    if prev_at == at - 1 and prev in COMPOUND_OPERATOR_TAIL:
        return False
    return ">" == nonspace_after(src, offset + len(word))


def is_comparison(src, offset, op):
    """Whether a bare `<`/`>` is a comparison rather than a template bracket.

    Most sites are angle brackets - `std::is_same<`, `static_cast<size_t>`,
    `template <class Key>` - and mutating one is a compile error every time.
    They are not free: each costs a syntax check, and they swamp the `compiler`
    bucket, which is the one the report explicitly calls not the number worth
    moving. The header is clang-formatted, so a comparison always has a space on
    both sides and a template bracket never does.
    """
    if op not in ("<", ">"):
        return True
    before = src[offset - 1] if offset else ""
    after = src[offset + 1] if offset + 1 < len(src) else ""
    return before == " " and after == " "


def line_starts(src):
    starts = [0]
    start = src.find("\n")
    while start >= 0:
        starts.append(start + 1)
        start = src.find("\n", start + 1)
    return starts


def mutation_sites(src, mask, line_filter=None):
    """Every single-token change worth trying, as {offset, original, ...} dicts."""
    sites = []
    n = len(src)
    starts = line_starts(src)

    def line_of(off):
        return bisect.bisect_right(starts, off)

    def add(offset, original, replacement):
        line = line_of(offset)
        if line_filter is not None and line not in line_filter:
            return
        sites.append(dict(offset=offset, original=original,
                          replacement=replacement, line=line,
                          description="%s -> %s" % (original, replacement)))

    i = 0
    while i < n:
        if not mask[i]:
            i += 1
            continue

        m = IDENT.match(src, i)
        if m:
            if not is_template_default(src, i, m.group(0)):
                for rep in WORD_MUTATIONS.get(m.group(0), ()):
                    add(i, m.group(0), rep)
            i = m.end()
            continue

        m = NUMBER.match(src, i)
        if m:
            # A float like 1.5 or 0.8 must not be picked apart into an int. A
            # number at the very start or end of the file has no neighbour, and
            # the sentinel is what stands in for one: `"" in ".0123456789"` is
            # True, so an empty string here would reject the mutation instead.
            before = src[i - 1] if i else "\n"
            after = src[m.end()] if m.end() < n else "\n"
            if before not in ".0123456789" and after not in ".eE0123456789":
                value, suffix = int(m.group(1)), m.group(2)
                for rep_val in (value + 1, value - 1):
                    if rep_val >= 0:
                        add(i, m.group(0), "%d%s" % (rep_val, suffix))
            i = m.end()
            continue

        for op, reps in OPERATOR_MUTATIONS:
            if src.startswith(op, i):
                if is_comparison(src, i, op):
                    for rep in reps:
                        add(i, op, rep)
                i += len(op)
                break
        else:
            i += 1

    return sites


LINE_PART = re.compile(r"^(\d+)(?:-(\d+))?$")


def parse_lines(text):
    """Line numbers from `1290`, `1278-1290`, or any comma-separated mix.

    The list form is what makes a second pass over the first pass's survivors one
    run instead of one per line. Survivors land where they land - a handful of
    lines scattered over three thousand - and the alternatives are both bad: a
    range wide enough to cover them re-answers hundreds of mutants that were
    answered already, and one run per line pays for lanes and a baseline every
    time.

    Matched whole rather than split on the first dash, because every loose
    reading of a part is a silent one: `1-` would be line 1 and not "from 1
    onwards", `20-10` would be no lines at all, and a run that sweeps less than
    it was asked to reports a clean result for lines nobody looked at.
    """
    lines = set()
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        m = LINE_PART.match(part)
        if not m:
            raise argparse.ArgumentTypeError(
                "%r is not a line or a range - write it as 1290, 1278-1290, or "
                "a comma-separated mix" % part)
        first, last = int(m.group(1)), int(m.group(2) or m.group(1))
        if last < first:
            raise argparse.ArgumentTypeError(
                "%r ends before it starts" % part)
        lines.update(range(first, last + 1))
    return lines


# Lines that end in `;` without being a statement anyone can drop: a closing
# brace of an initializer, a declaration the rest of the scope needs, a label.
# Over-matching is not expensive - a deletion that does not compile is rejected
# by the pre-filter in half a second - but a deletion that cannot compile is
# also a mutant that tells you nothing, so the obvious ones are left out.
NOT_A_STATEMENT = re.compile(
    r"^(\}|\{|else\b|do\b|try\b|catch\b|template\b|using\b|typedef\b|namespace\b"
    r"|struct\b|class\b|enum\b|static_assert\b|public:|private:|protected:|friend\b)")


def deletion_sites(src, mask, line_filter=None):
    """Whole statements, taken out.

    The operator the token sweep cannot express, and the one the hand-written
    bugs kept turning out to be. Nearly every bug in bugs/invariants.txt is some
    form of "the code forgot to do this": the shift down that never happens, the
    pop_back that is skipped, the bucket that is never repointed. None of those
    is one token, and none of them is reachable by changing one.

    Only single-line statements, and only ones whose brackets balance on that
    line, so that what is removed is a whole statement rather than the middle of
    one. That leaves out the multi-line calls, which is a real gap and the price
    of not parsing C++.

    Plenty of these will not compile -- a declaration something below it uses, a
    return from a function that has to return something. That is the pre-filter's
    half second rather than a full rebuild, so the budget stands it; what it
    costs is a report with a larger `compiler` column.
    """
    sites = []
    starts = line_starts(src)
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(src)
        line = src[start:end].rstrip("\n")
        # Read through the mask so that a trailing comment does not decide
        # whether this looks like a statement.
        code = "".join(c if mask[start + k] else " " for k, c in enumerate(line))
        stripped = code.strip()
        if not stripped.endswith(";") or NOT_A_STATEMENT.match(stripped):
            continue
        if code.count("(") != code.count(")") or code.count("{") != code.count("}"):
            continue
        lineno = index + 1
        if line_filter is not None and lineno not in line_filter:
            continue
        offset = start + (len(line) - len(line.lstrip()))
        text = src[offset:start + len(line)]
        if not text:
            continue
        sites.append(dict(offset=offset, original=text, replacement="", line=lineno,
                          description="delete: %s" % (stripped[:52] + ("..." if len(stripped) > 52 else ""))))
    return sites


def changed_lines(ref, path):
    """Line numbers of `path` touched since `ref`, for the fast daily mode.

    Against the merge base rather than the ref's tip, which is the difference
    between "what did I change" and "what does this file look like compared to
    over there". On a branch that has not caught up with main the second answer
    includes every line main moved on without you -- lines you have never seen,
    swept as though they were yours. For a ref that is already an ancestor, which
    is what `HEAD` and `HEAD~1` are, the merge base is the ref itself and the two
    readings are the same.

    Falls back to the plain form where git is too old to know the flag: the
    answer is then merely too wide, which costs time rather than correctness.
    """
    argv = ["git", "diff", "--unified=0", "--merge-base", ref, "--", path]
    r = subprocess.run(argv, cwd=REPO, capture_output=True, text=True)
    if r.returncode != 0:
        argv.remove("--merge-base")
        r = subprocess.run(argv, cwd=REPO, capture_output=True, text=True, check=True)
    out = r.stdout
    lines = set()
    for hunk in re.finditer(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", out, re.M):
        start = int(hunk.group(1))
        count = int(hunk.group(2) or 1)
        lines.update(range(start, start + count))
    return lines


# ---------------------------------------------------------------- running

def count_test_cases(stdout):
    """How many cases doctest actually ran, or None if it never got that far.

    The number this guards against is zero. doctest's suites are the ones a
    `TEST_SUITE` names - `fuzz` and `stochastic` here - and not meson's, which
    call the whole binary `unit`; ask for `-ts=unit` and doctest skips all 528
    cases and exits 0. Every mutant then comes back `survived` and the report
    reads as a suite full of holes.
    """
    m = re.search(r"^\[doctest\] test cases:\s*(\d+)", stdout, re.M)
    return int(m.group(1)) if m else None


def parse_test_output(proc):
    """Which tests went red, given a finished run of the suite.

    Kept out of Lane so that spawning a process and interpreting doctest's
    output stay separable - the second is the part that changes when a mutant
    dies in a way doctest has no vocabulary for.
    """
    if proc.returncode == 0:
        return []

    # doctest prints a `TEST CASE:` banner for anything that produces output, a
    # passing MESSAGE included, so the banners alone are not failures - only one
    # with an assertion error under it counts.
    failing, current = [], None
    for line in proc.stdout.splitlines():
        banner = re.match(r"^TEST CASE:\s*(.+)$", line)
        if banner:
            current = banner.group(1).strip()
        elif ("ERROR" in line or "FAILED" in line) and current:
            if current not in failing:
                failing.append(current)
    if failing:
        return failing

    # Nonzero exit with no failed assertion: a sanitizer abort, a crash or a
    # signal. Naming it beats reporting "unknown" - under -Db_sanitize the thing
    # that noticed is often not a test at all, and which runtime complained is
    # the whole answer.
    for stream in (proc.stderr, proc.stdout):
        for pattern in (r"^SUMMARY: (\w+Sanitizer): (.+)$", r"runtime error: (.+)$"):
            m = re.search(pattern, stream, re.M)
            if m:
                return [" ".join(m.groups())[:120]]
    return ["exit %d, no assertion failed" % proc.returncode]


def short_test_name(name, limit=52):
    """A doctest case name, shortened for a terminal.

    Most of the suite is `TEST_CASE_TEMPLATE`, so a name arrives as
    `a_hash_survives_rehashing<ankerl::unordered_dense::v4_9_1::detail::table<
    std::__cxx11::basic_string<char>, int, ...>>` - 300 characters of which the
    first 25 are the part that says what broke. The instantiation is kept as a
    marker rather than dropped, because which one of a template's instantiations
    failed is occasionally the answer, and the full names stay in --json.
    """
    depth, out = 0, []
    for ch in name:
        if ch == "<":
            depth += 1
            if depth == 1:
                out.append("<...>")
        elif ch == ">":
            depth = max(0, depth - 1)
        elif depth == 0:
            out.append(ch)
    short = "".join(out)
    return short if len(short) <= limit else short[:limit - 3] + "..."


def render_caught(caught_by, most=3):
    """The tests that noticed, as much of them as is worth printing."""
    if not caught_by:
        return ""
    shown = ", ".join(short_test_name(t) for t in caught_by[:most])
    extra = len(caught_by) - most
    return shown + (" (+%d more)" % extra if extra > 0 else "")


class Lane:
    """One throwaway copy of the repo, configured once and reused per mutant."""

    def __init__(self, root, index, args):
        self.dir = os.path.join(root, "lane%d" % index)
        self.build = os.path.join(self.dir, "builddir")
        self.target = os.path.join(self.dir, args.file)
        self.jobs = args.jobs
        self.memory = args.memory_limit
        self.setup_args = meson_setup_args(args)
        self.test_args = []
        if args.test_suite:
            self.test_args.append("-ts=" + args.test_suite)
        if args.exclude_suite:
            self.test_args.append("-tse=" + args.exclude_suite)
        if args.test_filter:
            self.test_args.append("-tc=" + args.test_filter)
        self.syntax_cmd = None
        self.syntax_file = args.syntax_tu

        self.env = dict(os.environ)
        # The corpus replay walks up from the working directory looking for
        # `.fuzz-corpus-base-dir`, which would find the lane's copy anyway. Said
        # outright because it is the one input the suite reads from outside the
        # binary, and a lane silently replaying nothing would show up as a wall
        # of survivors in exactly the code the fuzzers cover best.
        self.env["FUZZ_CORPUS_BASE_DIR"] = os.path.join(self.dir, "data", "fuzz")
        # ccache keys on the compiler command line, and every lane's -I is a
        # different absolute path - base_dir rewrites absolute paths beneath it
        # to relative ones so that every lane hashes the same as the developer's
        # own build. Read-only on purpose: the baseline build is then nearly
        # free, while the mutants - each a preprocessed source nothing will ever
        # see again - do not get to evict a real cache with ~90 objects apiece.
        self.env["CCACHE_BASEDIR"] = os.path.abspath(self.dir)
        self.env["CCACHE_READONLY"] = "1"
        # Set unconditionally, exactly as the CI legs do: only a sanitizer
        # runtime reads these, so on an ordinary build they do nothing. Without
        # halt_on_error UBSan prints the error and the binary still exits 0 -
        # and a mutant that only UBSan can see is then reported as a survivor,
        # which is the one way this tool must never be wrong.
        self.env.setdefault("ASAN_OPTIONS", "detect_stack_use_after_return=1")
        self.env.setdefault("UBSAN_OPTIONS",
                            "print_stacktrace=1:halt_on_error=1")

    def setup(self):
        if os.path.isdir(self.dir):
            sync_tree(REPO, self.dir)
            # The mutated file is the one thing whose *content* can be right
            # while the build behind it is wrong: a run that ended with a mutant
            # applied, and was then restored, leaves a binary built from the
            # mutant and a source that matches the repo again. Nothing in a
            # content comparison can see that, so the file that every mutant
            # rewrites is stamped unconditionally. It costs the one rebuild that
            # was going to happen anyway.
            os.utime(self.target, None)
        else:
            shutil.copytree(REPO, self.dir, ignore=lane_ignore)
        # Always, even when reusing: it is a second, and it is what picks up a
        # new test file added to test/meson.build. The sources are listed there
        # by hand, so skipping this would build the previous run's set and score
        # mutants against tests that are not in it.
        configured = os.path.exists(os.path.join(self.build, "build.ninja"))
        cmd = (["meson", "setup"] + self.setup_args
               + (["--reconfigure"] if configured else [])
               + [self.build, self.dir])
        r = subprocess.run(cmd, cwd=self.dir, capture_output=True, text=True,
                           env=self.env)
        if r.returncode != 0:
            raise RuntimeError("meson setup failed in lane:\n" + r.stdout + r.stderr)
        self.syntax_cmd = self._syntax_command()

    def _syntax_command(self):
        """The compile line meson would use for one TU, turned into a syntax check.

        Taken from compile_commands.json rather than reassembled by hand: the
        flags that matter here are the ones the real build uses, -Werror very
        much included, and a hand-written approximation of them drifts the day
        someone adds a warning to meson.build.
        """
        if not self.syntax_file:
            return None
        path = os.path.join(self.build, "compile_commands.json")
        with open(path, encoding="utf-8") as f:
            entries = json.load(f)
        wanted = os.path.normpath(os.path.join(self.dir, self.syntax_file))
        for entry in entries:
            resolved = os.path.normpath(
                os.path.join(entry["directory"], entry["file"]))
            if resolved != wanted:
                continue
            argv, out, i = shlex.split(entry["command"]), [], 0
            while i < len(argv):
                arg = argv[i]
                # -MF/-MQ name ninja's depfile for this object: writing it from
                # here would leave the real build's dependency information
                # describing a compile that never produced an object.
                if arg in ("-o", "-MF", "-MQ", "-MT"):
                    i += 2
                    continue
                if arg not in ("-c", "-MD", "-MMD"):
                    out.append(arg)
                i += 1
            # ccache refuses -fsyntax-only outright, and there is nothing to
            # cache in a compile that produces no object anyway.
            if out and os.path.basename(out[0]) == "ccache":
                out = out[1:]
            out.insert(1, "-fsyntax-only")
            return dict(argv=out, cwd=entry["directory"])
        return None

    def write_target(self, text):
        with open(self.target, "w", encoding="utf-8") as f:
            f.write(text)

    def run_syntax_check(self, timeout):
        """Cheap reject. Plenty of operator mutants are simply not valid C++, and
        this answers that in half a second instead of ~90 translation units."""
        return self._run(self.syntax_cmd["argv"], cwd=self.syntax_cmd["cwd"],
                         timeout=timeout, env=self.env)

    def compiled_lines(self, timeout):
        """Line numbers of the mutated file that survive the preprocessor.

        A mutant in a branch this configuration does not compile cannot be
        caught by anything, and costs a full rebuild of ~90 translation units to
        say so. `mum()` is the standing example: it picks between __uint128_t,
        an MSVC intrinsic and a long-hand multiply, so two thirds of it is never
        seen here -- 35 mutants of pure noise in one function.

        Asked of the compiler rather than by matching `#if` in the text, because
        the answer depends on the flags the build actually uses. `-E` emits a
        linemarker whenever the line it is about to print is not the next one,
        so following those and counting lines in between gives what was kept.

        None when there is nothing to ask - no pre-filter TU, or the run was
        told not to use one - which the caller reads as "do not filter".
        """
        if not self.syntax_cmd:
            return None
        argv = [a for a in self.syntax_cmd["argv"] if a != "-fsyntax-only"]
        argv.insert(1, "-E")
        r = self._run(argv, cwd=self.syntax_cmd["cwd"], timeout=timeout, env=self.env)
        if r is None or r.returncode != 0:
            return None
        wanted = os.path.realpath(self.target)
        lines, current, lineno = set(), None, 0
        for out in r.stdout.splitlines():
            marker = LINEMARKER.match(out)
            if marker:
                lineno = int(marker.group(1))
                # Resolved against the directory the compile runs in, not this
                # process's. The paths are the ones the -I flags produced, so
                # they are relative to the build directory; resolving them here
                # would name a file in whatever tree this script was started
                # from rather than the one in the lane.
                current = os.path.realpath(
                    os.path.join(self.syntax_cmd["cwd"], marker.group(2)))
                continue
            if current == wanted:
                lines.add(lineno)
            lineno += 1
        return lines or None

    def run_build(self, timeout, jobs=None):
        # jobs is overridden for the baseline, which is the one build that has
        # the machine to itself - and the cap follows it, so that build is not
        # measured against a share of the machine it is not sharing.
        jobs = jobs or self.jobs
        return self._run(["ninja", "-C", self.build, "-j", str(jobs)],
                         timeout=timeout, env=self.env,
                         memory=build_memory_limit(self.memory, jobs))

    def run_tests(self, timeout):
        cmd = [os.path.join(self.build, TEST_BINARY)] + self.test_args
        return self._run(cmd, cwd=self.dir, timeout=timeout, env=self.env)

    def _run(self, cmd, timeout, cwd=None, env=None, memory=None):
        # errors="replace" because a mutant's whole job is to make the library
        # misbehave, and a mutated size or hash prints whatever bytes it likes.
        # Strict decoding would turn one such byte into a traceback that kills
        # the pool and loses the whole sweep.
        capped = memory_capped(cmd, self.memory if memory is None else memory)
        try:
            return subprocess.run(capped, cwd=cwd or self.dir, capture_output=True,
                                  text=True, errors="replace", timeout=timeout,
                                  env=env)
        except subprocess.TimeoutExpired:
            return None
        except FileNotFoundError as e:
            raise RuntimeError("missing tool: %s" % e)


def evaluate(lane, mutant, args):
    """Run one mutant. Returns (verdict, tests that caught it).

    The order the checks come in is the order they get cheaper to be wrong
    about: a mutant killed at the memory cap is asked about before the exit
    status is read, because a stopped build looks like a build that failed and
    "the compiler refused it" is the one thing it definitely does not mean.
    """
    lane.write_target(mutant["text"])
    if args.quick_reject and lane.syntax_cmd:
        proc = lane.run_syntax_check(args.build_timeout)
        if killed_for_memory(proc):
            return "oom", []
        if proc is None or proc.returncode != 0:
            return "compiler", []
    proc = lane.run_build(args.build_timeout)
    if killed_for_memory(proc):
        return "oom", []
    if proc is None or proc.returncode != 0:
        return "compiler", []
    proc = lane.run_tests(args.test_timeout)
    if proc is None:
        return "hang", []
    if killed_for_memory(proc):
        return "oom", []
    failing = parse_test_output(proc)
    return ("caught", failing) if failing else ("survived", [])


# ------------------------------------------------------------- mutants

def parse_bug_file(path):
    """Bugs to reintroduce, one block each:

        # name of the bug
        <<<
        the code as it is today
        ===
        the code with the bug back
        >>>

    A block format rather than a diff so that multi-line bodies need no escaping
    and no leading-character rules - these are C++ fragments, and `-` is an
    operator.

    The name is the *first* line of a run of comments, so the ones after it are
    room to explain the bug without any of it ending up in the report. A blank
    line starts a new run, which is what lets a file open with a header of its
    own.
    """
    bugs, name, state, old, new = [], None, None, [], []
    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.rstrip("\n")
            stripped = line.strip()
            if state is None:
                if not stripped:
                    name = None
                elif stripped.startswith("#"):
                    if name is None:
                        name = stripped.lstrip("#").strip()
                elif stripped == "<<<":
                    state, old, new = "old", [], []
                else:
                    raise RuntimeError("%s:%d: expected '#' or '<<<'" % (path, lineno))
            elif stripped == "===" and state == "old":
                state = "new"
            elif stripped == ">>>" and state == "new":
                bugs.append(dict(name=name or "bug %d" % (len(bugs) + 1),
                                 old="\n".join(old), new="\n".join(new)))
                name, state = None, None
            elif state == "old":
                old.append(line)
            else:
                new.append(line)
    if state is not None:
        raise RuntimeError("%s: unterminated block, missing '>>>'" % path)
    return bugs


def bug_mutants(bugs, original):
    """Substitutions to mutated sources, refusing any that does not apply.

    A typo in the `old` text would otherwise substitute nothing, the suite would
    stay green, and the report would say the bug survived - a false alarm about
    the tests when the fault is in the bug list.
    """
    problems = []
    for bug in bugs:
        count = original.count(bug["old"])
        if count != 1:
            problems.append("  %-40s matches %d times, needs exactly 1"
                            % (bug["name"], count))
        elif bug["old"] == bug["new"]:
            problems.append("  %-40s is a no-op" % bug["name"])
    if problems:
        raise RuntimeError("these bugs do not apply:\n" + "\n".join(problems))
    return [dict(name=bug["name"], text=original.replace(bug["old"], bug["new"]))
            for bug in bugs]


def site_mutants(sites, original):
    return [dict(name=site["description"], line=site["line"],
                 offset=site["offset"], description=site["description"],
                 text=(original[:site["offset"]] + site["replacement"]
                       + original[site["offset"] + len(site["original"]):]))
            for site in sites]


def reverse_commit_mutant(ref, original, target):
    """One bug, expressed as 'undo what this commit did to the file'.

    Applied to a scratch copy rather than a lane, so that every mode has produced
    its mutants before any lane exists - otherwise --dry-run has nothing to show
    and the lane count cannot be sized from the number of bugs.
    """
    # --format= drops the commit header, which `git show` prints even when the
    # path it was given is untouched - leaving output that is not empty but
    # holds no patch, so the failure surfaces from `git apply` instead of here.
    diff = subprocess.run(["git", "show", "--format=", ref, "--", target],
                          cwd=REPO, capture_output=True, text=True,
                          check=True).stdout
    if "diff --git" not in diff:
        raise RuntimeError("%s does not touch %s" % (ref, target))
    with tempfile.TemporaryDirectory(prefix="udm-reverse-") as scratch:
        staged = os.path.join(scratch, target)
        os.makedirs(os.path.dirname(staged), exist_ok=True)
        with open(staged, "w", encoding="utf-8") as f:
            f.write(original)
        r = subprocess.run(["git", "apply", "-R", "--unsafe-paths",
                            "--directory", ".", "-p1", "-"],
                           cwd=scratch, input=diff, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError("could not reverse %s:\n%s" % (ref, r.stderr))
        with open(staged, encoding="utf-8") as f:
            text = f.read()
    if text == original:
        raise RuntimeError("reversing %s changes nothing" % ref)
    return dict(name="reverse of %s" % ref, text=text)


# ------------------------------------------------------------- the run

def meson_setup_args(args):
    """What every lane is configured with.

    `-Ddebug=false` is not about debugging: meson's `debug` buildtype means -O0
    *and* -g, and the debug info is a third of the compile time and two thirds of
    the 60 MB each lane's build directory would otherwise be - times however many
    lanes. Nothing here reads a symbol. Skipped if --meson-arg says otherwise, so
    it stays overridable rather than baked in.
    """
    extra = ["--buildtype", args.buildtype]
    if not any(a.startswith("-Ddebug=") for a in args.meson_arg):
        extra.append("-Ddebug=false")
    return extra + list(args.meson_arg)


def plan(args, wanted):
    """Lanes, jobs each, and memory each - settled before anything is copied.

    Together these are most of what a verdict depends on, so they are decided in
    one place and stated in the fingerprint rather than emerging from whichever
    function needed them first.
    """
    args.lanes = max(1, min(args.lanes, wanted))
    # One job per lane once the lanes fill the machine, but all of it for a
    # single named bug - which is the difference between a --replace answering
    # in 20 seconds and in three minutes, given that every mutant rebuilds every
    # translation unit.
    if args.jobs is None:
        args.jobs = max(1, (os.cpu_count() or 4) // args.lanes)
    if args.memory_limit is None:
        args.memory_limit = default_memory_limit(args.lanes, args.jobs)
    args.memory_note = memory_cap_reason(args.memory_limit) if args.memory_limit else ""
    if args.memory_note:
        args.memory_limit = 0


def fingerprint(args):
    """What this run could actually observe.

    A verdict is only meaningful for the build that produced it, and the way that
    goes wrong quietly is memory safety: an off-by-one in a bucket index or a
    dangling reference after a rehash is invisible to a plain build unless it
    happens to corrupt something a test then checks. Without a sanitizer those
    come back `survived` however good the tests are.
    """
    compiler = os.environ.get("CXX")
    if compiler is None:
        try:
            compiler = subprocess.run(["c++", "--version"], capture_output=True,
                                      text=True).stdout.splitlines()[0]
        except (OSError, IndexError):
            compiler = "unknown"
    setup_args = meson_setup_args(args)
    sanitizer = next((a.split("=", 1)[1] for a in setup_args
                      if a.startswith("-Db_sanitize=")), "none")
    tests = " ".join(filter(None, [args.test_suite and "-ts=" + args.test_suite,
                                   args.exclude_suite and "-tse=" + args.exclude_suite,
                                   args.test_filter and "-tc=" + args.test_filter]))
    return dict(compiler=compiler, sanitizer=sanitizer, cores=os.cpu_count(),
                meson=setup_args, tests=tests or "all", file=args.file,
                ccache=shutil.which("ccache") is not None,
                lanes=args.lanes, jobs=args.jobs, memory=args.memory_limit,
                memory_note=args.memory_note)


def render_fingerprint(facts):
    lines = ["compiler        %s" % facts["compiler"],
             "cores           %s" % (facts["cores"] or "?"),
             "lanes           %d, %d job%s each"
             % (facts["lanes"], facts["jobs"], "" if facts["jobs"] == 1 else "s"),
             "memory          %s per lane" % human(facts["memory"]),
             "meson setup     %s" % " ".join(facts["meson"]),
             "sanitizer       %s" % facts["sanitizer"],
             "mutating        %s" % facts["file"],
             "tests           %s" % facts["tests"]]
    if not facts["memory"]:
        lines += ["",
                  "NOTE: nothing caps what a mutant may allocate in this run%s."
                  % (": " + facts["memory_note"] if facts["memory_note"] else ""),
                  "      A mutated growth policy can ask for more memory than",
                  "      the machine has, and the kernel then picks the victim -",
                  "      as likely another lane as the mutant that caused it, so",
                  "      the verdicts around it are no longer trustworthy."]
    if facts["sanitizer"] == "none":
        lines += ["",
                  "NOTE: no sanitizer in this build, so a mutant that reads one",
                  "      slot too far or keeps a reference across a rehash is",
                  "      only seen if it happens to corrupt something a test",
                  "      checks. Re-run a survivor you cannot explain with",
                  "      --meson-arg=-Db_sanitize=address,undefined before",
                  "      concluding the hole is in the tests."]
    if not facts["ccache"]:
        lines += ["",
                  "NOTE: no ccache on PATH. Only the baseline build would have",
                  "      hit it - every mutant is a compile nothing has seen -",
                  "      so this costs one full build, not a slower run."]
    return "\n".join(lines)


def baseline(lane, args, log):
    """Refuse to score anything until the suite is green repeatedly.

    A flaky test scored as a kill inflates the number in the flattering
    direction, which is the worst way for this tool to be wrong.

    Returns how long a green run takes, which is what the per-mutant timeouts
    are derived from - a fixed generous timeout makes every hung mutant cost
    many times what a real one does, and hangs are an expected verdict here.
    """
    log("baseline: building")
    proc = lane.run_build(args.build_timeout, jobs=os.cpu_count() or 4)
    if killed_for_memory(proc):
        raise RuntimeError(
            "the baseline build was killed at the memory cap, and that is this "
            "tree's own build rather than any mutant - raise --memory-limit, or "
            "pass 0 to turn the cap off.")
    if proc is None or proc.returncode != 0:
        raise RuntimeError("baseline build failed - fix the tree first")
    slowest = 0.0
    for attempt in range(args.baseline_runs):
        started = time.time()
        proc = lane.run_tests(args.test_timeout)
        slowest = max(slowest, time.time() - started)
        if proc is None:
            raise RuntimeError("baseline run %d timed out" % (attempt + 1))
        # The unmutated suite peaks at a few tens of megabytes, so this says the
        # cap is set below what an honest run needs - and every mutant under it
        # would come back `oom` no matter what the tests do.
        if killed_for_memory(proc):
            raise RuntimeError(
                "baseline run %d was killed at the memory cap (%s). That is the "
                "suite the mutants are measured against, so the cap is too low "
                "to tell anything apart - raise --memory-limit."
                % (attempt + 1, human(lane.memory)))
        failing, ran = parse_test_output(proc), count_test_cases(proc.stdout)
        if failing:
            raise RuntimeError(
                "baseline run %d failed (%s). The suite must be green and "
                "deterministic before mutants mean anything."
                % (attempt + 1, ", ".join(short_test_name(t) for t in failing)))
        # A filter that matches nothing is green, and every mutant under it
        # survives. Refusing here is the difference between "your filter names
        # no doctest suite" and a report claiming the suite covers nothing.
        if ran == 0:
            raise RuntimeError(
                "baseline run %d passed 0 test cases (%s). doctest's suites are "
                "the ones TEST_SUITE names - 'fuzz' and 'stochastic' - not "
                "meson's, which calls the whole binary 'unit'."
                % (attempt + 1, " ".join(lane.test_args) or "no filter"))
        log("baseline: run %d/%d green, %s case%s"
            % (attempt + 1, args.baseline_runs, "?" if ran is None else ran,
               "" if ran == 1 else "s"))
    return slowest


def check_room_for(workdir, new_lanes, log):
    """Refuse before copying rather than half way through it.

    A lane is a copy of the tree and a build directory of its own, and the count
    comes off the core count without anyone having been asked. Running out part
    way leaves a meson setup failing for a reason that does not mention disk -
    and where the workdir is a tmpfs, which is where /tmp usually is, running
    out means the machine running out of memory rather than of disk.
    """
    needed = new_lanes * LANE_BYTES
    if not needed:
        return
    free = shutil.disk_usage(workdir).free
    in_ram = memory_backed(workdir)
    log("%d new lane%s of about %s in %s, %s free%s"
        % (new_lanes, "" if new_lanes == 1 else "s", human(needed), workdir,
           human(free), " - and it is a tmpfs, so that is memory" if in_ram else ""))
    if needed > free:
        raise RuntimeError(
            "%s has %s free and %d lane%s want about %s%s. Use fewer --lanes, or "
            "--workdir somewhere with room."
            % (workdir, human(free), new_lanes, "" if new_lanes == 1 else "s",
               human(needed),
               " - and it is a tmpfs, so that room is memory" if in_ram else ""))


@contextlib.contextmanager
def lanes_for(args, wanted, log):
    """Copied trees, configured and baselined, cleaned up whatever happens."""
    if args.reuse:
        workdir = args.workdir or os.path.join(tempfile.gettempdir(),
                                               "udm-mutate-reuse")
        os.makedirs(workdir, exist_ok=True)
    else:
        workdir = args.workdir or tempfile.mkdtemp(prefix="udm-mutate-")
        if args.workdir:
            shutil.rmtree(workdir, ignore_errors=True)
            os.makedirs(workdir)
    try:
        count = args.lanes  # settled by plan(), along with jobs and memory
        lanes = [Lane(workdir, i, args) for i in range(count)]
        existing = sum(1 for lane in lanes if os.path.isdir(lane.build))
        log("preparing %d lane%s%s, %d job%s each"
            % (count, "" if count == 1 else "s",
               " (%d reused)" % existing if existing else "",
               args.jobs, "" if args.jobs == 1 else "s"))
        check_room_for(workdir, count - existing, log)
        started = time.time()
        with concurrent.futures.ThreadPoolExecutor(count) as pool:
            list(pool.map(lambda lane: lane.setup(), lanes))
        log("lanes ready in %.1fs" % (time.time() - started))
        started = time.time()
        green_seconds = baseline(lanes[0], args, log)
        log("baseline in %.1fs" % (time.time() - started))
        if args.test_timeout is None:
            args.test_timeout = max(20, int(6 * green_seconds))
            log("test timeout %ds, from a %.1fs green run"
                % (args.test_timeout, green_seconds))
        yield lanes
    finally:
        if not args.reuse:
            shutil.rmtree(workdir, ignore_errors=True)


def drop_uncompiled(lane, mutants, args, log):
    """Mutants in code this configuration does not compile, taken back out.

    Said out loud rather than done quietly: a run that silently swept less than
    it was asked to reads as "everything here is covered", and which lines were
    dropped is the interesting half - it names the branches this build cannot
    answer for, which is a coverage question rather than a test one.
    """
    compiled = lane.compiled_lines(args.build_timeout)
    if compiled is None:
        return mutants
    kept = [m for m in mutants if "line" not in m or m["line"] in compiled]
    dropped = len(mutants) - len(kept)
    if dropped:
        where = sorted({m["line"] for m in mutants if "line" in m and m["line"] not in compiled})
        log("%d mutant%s dropped on %d line%s the preprocessor removes in this "
            "build (%s%s) - nothing could catch them"
            % (dropped, "" if dropped == 1 else "s", len(where),
               "" if len(where) == 1 else "s",
               ", ".join(str(l) for l in where[:8]),
               ", ..." if len(where) > 8 else ""))
    return kept


def run_mutants(lanes, mutants, args, log):
    """Every mutant through a lane, reported in the order they were produced."""
    pending, results, lock = queue.Queue(), [], threading.Lock()
    for index, mutant in enumerate(mutants):
        pending.put((index, mutant))

    def worker(lane):
        while True:
            try:
                index, mutant = pending.get_nowait()
            except queue.Empty:
                return
            # One mutant that blows up must not take the sweep with it: at
            # several seconds each these runs are an hour long, and losing the
            # other verdicts to salvage nothing is the worst possible trade. An
            # 'error' of its own rather than a silent 'survived' - a swallowed
            # exception would flatter the tests, which is the one direction this
            # tool must never be wrong in.
            try:
                verdict, caught_by = evaluate(lane, mutant, args)
            except Exception as e:  # noqa: BLE001 - deliberately total
                verdict, caught_by = "error", ["%s: %s" % (type(e).__name__, e)]
            with lock:
                results.append(dict(mutant, index=index, verdict=verdict,
                                    caught_by=caught_by))
                log("[%d/%d] %-46s %s%s"
                    % (len(results), len(mutants), mutant["name"][:46], verdict,
                       " (%s)" % render_caught(caught_by) if caught_by else ""))

    with concurrent.futures.ThreadPoolExecutor(len(lanes)) as pool:
        list(pool.map(worker, lanes))

    results.sort(key=lambda r: r["index"])
    for r in results:
        r.pop("text", None)  # a whole mutated header per result helps nobody
        r.pop("index", None)
    return results


def report(results, args, original):
    """One summary for both modes. Only the framing differs, not the verdicts."""
    counts = {v: sum(1 for r in results if r["verdict"] == v) for v in VERDICTS}
    survivors = [r for r in results if r["verdict"] == "survived"]

    print("\n" + "=" * 72)
    width = max(len(r["name"]) for r in results)
    if len(results) <= 40:
        for r in results:
            print("%-*s  %-9s  %s"
                  % (width, r["name"], r["verdict"], render_caught(r["caught_by"]) or "-"))
        print()

    tally = ["%d caught by a test" % counts["caught"],
             "%d by the compiler only" % counts["compiler"],
             "%d hung" % counts["hang"]]
    # Only when it happened: on a header where nothing controls an allocation
    # size this is always zero, and a permanent 0 reads as noise.
    if counts["oom"]:
        tally.append("%d killed at the memory cap" % counts["oom"])
    tally.append("%d SURVIVED" % counts["survived"])
    print(", ".join(tally))
    # Errors are not scored either way. Counting them as killed would inflate
    # the number, and as survived would invent holes that may not be there.
    scored = len(results) - counts["error"]
    if counts["error"]:
        print("%d could not be scored - the tool failed, not the tests:"
              % counts["error"])
        for r in results:
            if r["verdict"] == "error":
                print("  %-46s %s" % (r["name"][:46], ", ".join(r["caught_by"])))
    if scored > 1:
        print("score            %.0f%% killed, %.0f%% by a test"
              % (100.0 * (scored - counts["survived"]) / scored,
                 100.0 * counts["caught"] / scored))

    if survivors:
        print("\nnothing noticed these - whatever covers them is decoration:")
        source = original.splitlines()
        for r in survivors:
            if "line" in r:
                text = source[r["line"] - 1].strip() if r["line"] <= len(source) else ""
                print("  %s:%d  %s\n      %s" % (args.file, r["line"], r["name"],
                                                 text[:100]))
            else:
                print("  %s" % r["name"])
    return counts


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--replace", nargs=2, metavar=("OLD", "NEW"), action="append",
                   help="put a specific bug back: substitute OLD with NEW (must "
                        "match exactly once) and report which tests notice. "
                        "Repeatable - a batch runs across lanes in parallel")
    p.add_argument("--bugs", metavar="FILE",
                   help="a file of bugs to reintroduce, '# name' then a "
                        "<<< old === new >>> block each. Runs in parallel")
    p.add_argument("--reverse", metavar="REF",
                   help="put one bug back by reverse-applying REF's changes to "
                        "the file, keeping today's tests")
    p.add_argument("--diff", metavar="REF", nargs="?", const="HEAD",
                   help="only mutate lines changed since REF, measured from the "
                        "merge base so a branch that has not caught up does not "
                        "sweep what main moved on without it. This is the "
                        "everyday mode: bare `--diff` means HEAD, which is "
                        "whatever is uncommitted. Adds to --bugs or --replace "
                        "rather than replacing them, so one run can ask both "
                        "questions")
    p.add_argument("--lines", metavar="LINES", type=parse_lines,
                   help="only mutate these lines, and the same: it adds a sweep "
                        "to whatever bugs were named. A line, a range, or a "
                        "comma-separated mix of both - 1290, 1278-1290, "
                        "'12,40-44,900'. The list form is how a second pass "
                        "asks about exactly the survivors of a first one")
    p.add_argument("--file", metavar="PATH", default=HEADER,
                   help="repo-relative file to mutate (default the header)")
    p.add_argument("--lanes", type=int, default=os.cpu_count() or 4,
                   help="parallel build+test lanes (default: one per hardware "
                        "thread, %d here). Each is a copy of the repo plus its "
                        "own build directory, so it is disk as well as cores"
                        % (os.cpu_count() or 4))
    p.add_argument("--jobs", type=int, default=None,
                   help="ninja parallelism inside one lane (default: the cores "
                        "divided by the lanes actually used, so a single bug "
                        "gets the whole machine and a sweep gets one job each)")
    p.add_argument("--buildtype", default="debug",
                   help="meson buildtype for every lane (default debug, which "
                        "is -O0: a mutant costs one rebuild of every TU, and at "
                        "-O3 that is twice the compiling to answer the same "
                        "question. Use release when the question is about what "
                        "the optimizer does)")
    p.add_argument("--memory-limit", type=parse_size, default=None, metavar="SIZE",
                   help="how much memory one lane may use, as 4G, 512M or a "
                        "plain byte count (default: the smaller of the lane's "
                        "share of the machine and 1G per ninja job). A mutated "
                        "growth policy turns an insert into a request for more "
                        "memory than the machine has; capped, that mutant dies "
                        "alone and is reported as 'oom', uncapped the kernel "
                        "picks a victim and it is as likely to be another lane. "
                        "0 turns the cap off")
    p.add_argument("--deletions", action="store_true",
                   help="also delete whole statements, one at a time. This is "
                        "where the hand-written bugs turn out to live -- nearly "
                        "every one of them is some form of \"the code forgot to "
                        "do this\", which is not one token and cannot be reached "
                        "by changing one. Roughly doubles the mutant count, but "
                        "the ones that cannot compile are rejected by the "
                        "pre-filter in half a second rather than a full rebuild")
    p.add_argument("--limit", type=int, help="stop after N mutants")
    p.add_argument("--shuffle-seed", type=int, default=0,
                   help="sample mutants deterministically when using --limit")
    p.add_argument("--build-timeout", type=int, default=900)
    p.add_argument("--test-timeout", type=int, default=None,
                   help="seconds before a mutant counts as hung (default: six "
                        "times the measured green run)")
    p.add_argument("--baseline-runs", type=int, default=2)
    p.add_argument("--no-quick-reject", dest="quick_reject",
                   action="store_false", default=True,
                   help="skip the -fsyntax-only pre-filter")
    p.add_argument("--meson-arg", metavar="ARG", action="append", default=[],
                   help="extra `meson setup` argument for every lane, "
                        "repeatable. This is how you ask a different question: "
                        "whether some other leg catches the bug, which the "
                        "default build cannot answer. Needs the '=' form, "
                        "because the value starts with a dash: "
                        "--meson-arg=-Db_sanitize=address,undefined "
                        "--meson-arg=-Dcpp_std=c++20")
    p.add_argument("--test-suite", metavar="NAME",
                   help="run only this doctest suite. These are the ones a "
                        "TEST_SUITE names - 'fuzz' and 'stochastic' - and not "
                        "meson's suites, so there is no 'unit' to ask for; the "
                        "bulk of the cases are in no suite at all")
    p.add_argument("--exclude-suite", metavar="NAME",
                   help="skip a doctest suite. 'fuzz' is the one worth "
                        "skipping when a run is long: replaying the corpora is "
                        "six sevenths of the suite's runtime, though it is also "
                        "some of its best coverage of the map internals - so "
                        "confirm a survivor without it before believing it")
    p.add_argument("--test-filter", metavar="PATTERN",
                   help="run only matching doctest cases")
    p.add_argument("--syntax-tu", metavar="PATH", default=None,
                   help="repo-relative TU the -fsyntax-only pre-filter compiles "
                        "(default: the file being mutated if it is itself a TU, "
                        "otherwise %s)" % SYNTAX_TU)
    p.add_argument("--dry-run", action="store_true",
                   help="list the mutants and the likely runtime, then stop")
    p.add_argument("--workdir", default=None, help="where lanes are copied")
    p.add_argument("--reuse", action="store_true",
                   help="keep the lanes afterwards and reuse them next time, "
                        "syncing only what changed. Saves the copying and the "
                        "configuring, not the compiling - every mutant rebuilds "
                        "every TU either way. Uses a fixed workdir unless "
                        "--workdir says otherwise, so two concurrent runs need "
                        "different ones")
    p.add_argument("--json", metavar="FILE", help="write the full result set")
    args = p.parse_args()

    modes = [bool(args.replace), bool(args.bugs), bool(args.reverse)]
    if sum(modes) > 1:
        p.error("--replace, --bugs and --reverse are three ways to say the same "
                "thing; pick one")

    target_path = os.path.join(REPO, args.file)
    if not os.path.exists(target_path):
        p.error("no such file: %s" % args.file)
    with open(target_path, encoding="utf-8") as f:
        original = f.read()

    # The pre-filter compiles one TU. Mutating a header it includes is the case
    # it was built for; mutating a TU means checking that TU itself; and against
    # anything else it would compile something the mutation cannot reach, pass
    # every time, and quietly stop filtering.
    if args.syntax_tu is None:
        if args.file == HEADER:
            args.syntax_tu = SYNTAX_TU
        elif args.file.endswith(".cpp"):
            args.syntax_tu = args.file
        else:
            args.quick_reject = False
    if not args.quick_reject:
        args.syntax_tu = None

    # The two kinds compose, and asking for both in one run is worth doing: they
    # answer different questions over the same code, and everything a second
    # invocation would repeat - copying the lanes, building the baseline, running
    # the suite green twice - is paid once instead. The named bugs come first in
    # the report, which is the order they are read in.
    mutants = []
    if args.bugs:
        mutants = bug_mutants(parse_bug_file(args.bugs), original)
    elif args.replace:
        mutants = bug_mutants(
            [dict(name="%s -> %s" % (old.strip()[:34], new.strip()[:34]),
                  old=old, new=new) for old, new in args.replace], original)
    elif args.reverse:
        mutants = [reverse_commit_mutant(args.reverse, original, args.file)]

    # A sweep is asked for by --diff or --lines, and with nothing named at all it
    # is the whole file - which is what this does when given no arguments.
    if args.diff or args.lines or not mutants:
        line_filter = None
        if args.diff:
            line_filter = changed_lines(args.diff, args.file)
            if line_filter:
                print("%d line%s of %s changed since %s"
                      % (len(line_filter), "" if len(line_filter) == 1 else "s",
                         args.file, args.diff))
        elif args.lines:
            line_filter = args.lines
        if args.diff and not line_filter:
            # Nothing to sweep. Only an error when it was the whole request:
            # alongside a bug file it is a note, not a reason to run nothing.
            print("no changed lines in %s since %s" % (args.file, args.diff))
            if not mutants:
                return 0
        else:
            mask = code_mask(original)
            sites = mutation_sites(original, mask, line_filter)
            if args.deletions:
                sites += deletion_sites(original, mask, line_filter)
                sites.sort(key=lambda s: s["offset"])
            sweep = site_mutants(sites, original)
            if args.limit and len(sweep) > args.limit:
                random.Random(args.shuffle_seed).shuffle(sweep)
                sweep = sweep[:args.limit]
                sweep.sort(key=lambda m: m["offset"])
            mutants += sweep

    if not mutants:
        # Reached by a sweep whose lines hold no code: a comment reflow, a
        # version bump, a `#define`. Saying which is the difference between a
        # clear answer and someone re-running it to find out why.
        print("nothing to mutate in %s - the lines asked for are comments, "
              "preprocessor or whitespace" % args.file)
        return 0

    plan(args, len(mutants))

    if args.dry_run:
        for mutant in mutants:
            print("  %s%s" % ("line %d: " % mutant["line"] if "line" in mutant
                              else "", mutant["name"]))
        print("\n%d mutant%s over %d lane%s, roughly %s, %s"
              % (len(mutants), "" if len(mutants) == 1 else "s", args.lanes,
                 "" if args.lanes == 1 else "s",
                 estimate(len(mutants), args.lanes, os.cpu_count() or 4),
                 "%s of memory each" % human(args.memory_limit)
                 if args.memory_limit else "no memory cap available here"))
        return 0

    facts = fingerprint(args)
    print(render_fingerprint(facts) + "\n")

    # Before any lane exists, because it is the lanes the cores would land in.
    drop_core_dumps()

    print_lock = threading.Lock()

    def log(message):
        with print_lock:
            print(message, flush=True)

    started = time.time()
    with lanes_for(args, len(mutants), log) as lanes:
        mutants = drop_uncompiled(lanes[0], mutants, args, log)
        if not mutants:
            # Everything asked for was in a branch this build does not compile.
            # Not an error: the answer is "there is nothing here to ask", and
            # the line above already said which lines those were.
            print("\nnothing left to run - every mutant was in code this build "
                  "does not compile")
            return 0
        log("%d mutant%s over %d lane%s"
            % (len(mutants), "" if len(mutants) == 1 else "s",
               len(lanes), "" if len(lanes) == 1 else "s"))
        results = run_mutants(lanes, mutants, args, log)

    counts = report(results, args, original)
    print("\n%d mutant%s in %.0fs"
          % (len(results), "" if len(results) == 1 else "s", time.time() - started))

    if args.json:
        with open(args.json, "w") as f:
            json.dump(dict(environment=facts, counts=counts, results=results),
                      f, indent=2)
        print("wrote %s" % args.json)

    return 1 if counts["survived"] else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RuntimeError as e:
        # Every RuntimeError raised here is the tool refusing to answer: a bug
        # block that does not apply, a baseline that is not green, a filter that
        # matches no test. The message is the whole point of those, and a
        # traceback down through the thread pool buries it. Exit 2 keeps them
        # apart from the 1 that means survivors were found.
        print("\n%s" % e, file=sys.stderr)
        sys.exit(2)
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
        sys.exit(130)
