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

**Or sweep for holes you have not thought of**, changing one place at a time:

    mutate.py --diff                           # whatever is uncommitted
    mutate.py --diff HEAD~1                    # only what that change touched
    mutate.py --lines 1200-1260,1300           # a function, or a scattering
    mutate.py                                  # the whole header
    mutate.py --diff --dry-run                 # how many, and how long

It is worth knowing that nearly every bug in `bugs/invariants.txt` is some form
of "the code forgot to do this" -- the shift down that never happens, the
pop_back that is skipped -- and that none of them is one token, so the token
sweep cannot reach any of them. Asking for deletions alone costs *less* than the
token sweep, because half of them are rejected by the pre-filter in half a
second rather than a rebuild:

    mutate.py --operators deletions            # the survey the sweep cannot do
    mutate.py --operators bitwise              # ^ and | alone, four minutes
    mutate.py --operators tokens,deletions     # both, for a thorough pass

**What a mutant costs here is one full rebuild of the test binary.** Every one of
the ~90 translation units includes the header, so there is no such thing as an
incremental mutant build and ccache cannot help either - each mutant is a
preprocessed source nothing has ever seen. That is around 100 CPU-seconds of
compiling against 3 of running the suite, which is why the lanes default to one
job each (the machine is already full) and why a single named bug instead gets
all the cores to itself. It is also why the -fsyntax-only pre-filter earns its
keep: half a second to reject what would otherwise cost a full rebuild.

Everything above this line is the same in nanobench, and the code that does it
lives in `mutate_core.py`, which is a vendored copy shared with that repository.
What is below is only what this project has to answer for itself.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import mutate_core  # noqa: E402 - the path above is what makes it importable


class UnorderedDense(mutate_core.Project):
    slug = "udm"
    repo = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

    # The library is one header, so that is the default target; --file overrides it.
    target = os.path.join("include", "ankerl", "unordered_dense.h")

    # The TU the syntax pre-filter compiles when the header is what is being
    # mutated. fuzz_api touches most of the API surface for about the same half
    # second as any other unit file, and instantiating is the point: a mutation
    # inside a member template is a parse error either way, but one that only
    # breaks type checking is not seen until something instantiates it.
    syntax_tu = os.path.join("test", "unit", "fuzz_api.cpp")

    build_dir = "builddir"
    test_binary = os.path.join("test", "udm-test")
    backend = mutate_core.MesonBackend()

    # Copied per lane, so this is about keeping the copy to the sources and the
    # fuzz corpora. `subprojects` deliberately stays: it holds the
    # already-downloaded doctest and the packagecache, and a lane without them
    # tries to fetch a release tarball per lane - slow where that works at all
    # and fatal where it does not.
    ignore = mutate_core.Project.ignore + ("fuzz-findings",)
    root_ignore = ("builddir", "build", "_build")

    lane_bytes = 90 * mutate_core.MIB  # ~21 MB of sources and corpora, ~64 MB of build

    # Measured on a 32-thread machine, g++ at -O0. One mutant rebuild costs 67
    # CPU-seconds on an idle machine - 75 unit TUs at ~0.66s each is 85% of it,
    # the benchmarks another 9% - against 3.2 to run the suite and 1 to link.
    # Under the 32-way contention a real run creates it costs more like 110,
    # which is what the figure below is calibrated against rather than the
    # isolated number. So the compiling is divided by the *machine* and only the
    # tail is divided by the lanes. Mutants the pre-filter rejects cost half a
    # second instead of all of that, and the estimate does not model them - so
    # --dry-run reads high on a sweep, which is the direction to be wrong in.
    cpu_seconds_per_mutant = 100.0
    lane_seconds_per_mutant = 4.0
    setup_seconds = 20.0
    setup_seconds_per_lane = 1.5

    def lane_env(self, lane_dir):
        # The corpus replay walks up from the working directory looking for
        # `.fuzz-corpus-base-dir`, which would find the lane's copy anyway. Said
        # outright because it is the one input the suite reads from outside the
        # binary, and a lane silently replaying nothing would show up as a wall
        # of survivors in exactly the code the fuzzers cover best.
        return {"FUZZ_CORPUS_BASE_DIR": os.path.join(lane_dir, "data", "fuzz")}


if __name__ == "__main__":
    mutate_core.run(UnorderedDense(), __doc__)
