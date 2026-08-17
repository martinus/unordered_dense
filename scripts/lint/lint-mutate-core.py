#!/usr/bin/env python3
"""`scripts/mutate/mutate_core.py` is vendored, and this is what says so out loud.

nanobench holds a byte-identical copy of that file and drives it through an adapter of its own.
Sharing it is what lets one test suite -- `scripts/test_mutate.py`, in this repository -- cover the
code both projects run. A copy edited in place ends that quietly: the suite here stays green, the
other repository runs something nothing tests, and the two drift apart one convenient local fix at
a time.

There is no way for a lint in one repository to see the other one, so this checks the next best
thing: that the core still hashes to what was recorded when it was last vendored. Editing the core
means updating `mutate_core.sha256` in the same commit, and then the two repositories hold the same
recorded hash -- which makes "are these in sync?" a question `diff` can answer:

    diff <(cat unordered_dense/scripts/mutate/mutate_core.sha256) \\
         <(cat nanobench/src/scripts/mutate/mutate_core.sha256)

That is deliberately a hand check rather than a promise. What this can enforce is that nobody
changed the shared file without noticing that it is shared.
"""

import hashlib
import importlib.util
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
CORE = REPO / "scripts" / "mutate" / "mutate_core.py"
RECORDED = CORE.with_suffix(".sha256")
ADAPTER = CORE.parent / "mutate.py"


def main():
    if not CORE.exists():
        print("%s is missing" % CORE.relative_to(REPO))
        return 1

    digest = hashlib.sha256(CORE.read_bytes()).hexdigest()
    if not RECORDED.exists():
        print("%s is missing - write %s into it" % (RECORDED.relative_to(REPO), digest))
        return 1

    recorded = RECORDED.read_text(encoding="utf-8").split()[0]
    if digest != recorded:
        print("%s has changed since it was last vendored.\n"
              "  recorded  %s\n"
              "  actual    %s\n"
              "It is shared with nanobench, so a change here is only half of one. Copy the file "
              "into that repository, run its lints, and record the new hash in both:\n"
              "  echo %s > %s"
              % (CORE.relative_to(REPO), recorded, digest, digest,
                 RECORDED.relative_to(REPO)))
        return 1

    # The adapter against the core, which is the same question nanobench's copy
    # of this lint asks. `Project.problems()` is in the core rather than written
    # out here, so adding a required attribute reaches both repositories through
    # the re-vendor that has to happen anyway -- and so that the checks are
    # covered by test_mutate.py rather than only run.
    spec = importlib.util.spec_from_file_location("mutate_adapter", ADAPTER)
    adapter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(adapter)
    project = adapter.UnorderedDense()
    problems = list(project.problems())
    if Path(adapter.mutate_core.__file__).resolve() != CORE:
        problems.append("the adapter imported %s rather than the vendored core beside it"
                        % adapter.mutate_core.__file__)
    if Path(project.repo) != REPO:
        problems.append("repo resolves to %s, not %s - every lane would copy the wrong tree"
                        % (project.repo, REPO))
    # Guarded, because `problems()` has already said if there is no backend at
    # all and a traceback on top of that message helps nobody read it.
    if project.backend is not None and project.backend.arg_flag != "--meson-arg":
        problems.append("the pass-through flag is %s, not --meson-arg" % project.backend.arg_flag)
    if problems:
        print("%s does not fit the core it is vendored with:" % ADAPTER.relative_to(REPO))
        for problem in problems:
            print("  %s" % problem)
        return 1

    # A core that does not import is a broken vendor whatever it hashes to, and
    # the adapter is what a person actually runs. --help exercises both, plus
    # every argparse default that a typo in the project object would break.
    r = subprocess.run([sys.executable, str(ADAPTER), "--help"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("%s --help failed:\n%s%s" % (ADAPTER.relative_to(REPO), r.stdout, r.stderr))
        return 1

    print("mutate_core.py vendored at %s" % digest[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main())
