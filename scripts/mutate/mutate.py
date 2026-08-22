#!/usr/bin/env python3
"""What mutation testing means for unordered_dense in particular.

`--help` prints the shared manual from mutate_core.py first; this is the part
underneath it. The file mutated by default is `include/ankerl/unordered_dense.h`,
the suite is `udm-test`, and meson configures the lanes.

    scripts/mutate/mutate.py --bugs scripts/mutate/bugs/erase-path.txt
    scripts/mutate/mutate.py --replace OLD NEW
    scripts/mutate/mutate.py --diff                # whatever is uncommitted
    scripts/mutate/mutate.py --lines 1200-1260,1300

Nearly every bug in `bugs/invariants.txt` is some form of "the code forgot to do
this" -- the shift down that never happens, the pop_back that is skipped -- and
none of them is one token, so the token sweep cannot reach any of them. Asking
for deletions alone costs *less* than the token sweep here, because half of them
are rejected by the pre-filter in half a second rather than a rebuild:

    scripts/mutate/mutate.py --operators deletions
    scripts/mutate/mutate.py --operators bitwise   # ^ and | alone, four minutes

**What a mutant costs here is one full rebuild of the test binary.** Every one of
the ~90 translation units includes the header, so there is no such thing as an
incremental mutant build and ccache cannot help either - each mutant is a
preprocessed source nothing has ever seen. That is around 100 CPU-seconds of
compiling against 3 of running the suite, which is why the lanes default to one
job each (the machine is already full) and why a single named bug instead gets
all the cores to itself. It is also why the -fsyntax-only pre-filter earns its
keep: half a second to reject what would otherwise cost a full rebuild, and why
the lanes build `--unity=on`, which is 2.5x less compiling for the same work.

Without a sanitizer, a mutant that reads one slot past a bucket comes back
`survived` however good the tests are, so re-run anything surprising with
`--meson-arg=-Db_sanitize=address,undefined` before believing it.
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
    harness = mutate_core.DoctestHarness()

    # The tree is known to survive merged translation units: a CI leg builds it
    # that way on purpose, which is what makes it safe to ask for here.
    unity = True

    # Copied per lane, so this is about keeping the copy to the sources and the
    # fuzz corpora. `subprojects` deliberately stays: it holds the
    # already-downloaded doctest and the packagecache, and a lane without them
    # tries to fetch a release tarball per lane - slow where that works at all
    # and fatal where it does not.
    ignore = mutate_core.Project.ignore + ("fuzz-findings",)
    root_ignore = ("builddir", "build", "_build")

    # Measured: ~21 MB of sources and corpora, ~64 MB of build directory.
    lane_bytes = 90 * mutate_core.MIB

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

    def lane_env(self, lane):
        # The corpus replay walks up from the working directory looking for
        # `.fuzz-corpus-base-dir`, which would find the lane's copy anyway. Said
        # outright because it is the one input the suite reads from outside the
        # binary, and a lane silently replaying nothing would show up as a wall
        # of survivors in exactly the code the fuzzers cover best.
        return {"FUZZ_CORPUS_BASE_DIR": os.path.join(lane.dir, "data", "fuzz")}


if __name__ == "__main__":
    mutate_core.run(UnorderedDense(), __doc__)
