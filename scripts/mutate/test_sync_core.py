#!/usr/bin/env python3
"""Hermetic tests for sync-core.py's decisions. No git, no network, no copying.

Only `state()` is tested, and that is the whole of what can be wrong here in a
way nobody notices. The copying and the git commands fail loudly; the decision
of whether a sibling is in sync, stale, or must be refused is the part that
would quietly do the wrong thing - and one of its three answers exists solely
to catch a mistake a person makes silently, which is changing the core without
bumping its version.
"""

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("sync_core", HERE / "sync-core.py")
sync_core = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync_core)


def core_text(version, body="pass\n"):
    return f"CORE_VERSION = {version}\n{body}"


class TestState(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name) / "sib"
        (self.root / "scripts" / "mutate").mkdir(parents=True)

    def tearDown(self):
        self.dir.cleanup()

    def sibling(self, text=None):
        s = sync_core.Sibling("sib", "scripts/mutate", self.root)
        if text is not None:
            s.core.write_text(text)
        return s

    def state_of(self, sibling_text, canon_text):
        s = self.sibling(sibling_text)
        return s.state(canon_text, sync_core.sha256_text(canon_text))[0]

    def test_an_identical_copy_is_in_sync(self):
        t = core_text(3)
        self.assertEqual("in sync", self.state_of(t, t))

    def test_an_older_copy_is_stale(self):
        self.assertEqual("stale", self.state_of(core_text(2), core_text(3)))

    def test_a_copy_with_no_version_yet_is_stale(self):
        # The bootstrap case: the version constant is newer than the copies.
        self.assertEqual("stale",
                         self.state_of("pass\n", core_text(2)))

    def test_a_changed_core_at_the_same_version_is_refused(self):
        # The guard on forgetting to bump. Propagating here would leave two
        # different files both claiming v3, which is worse than no version.
        self.assertEqual("REFUSED",
                         self.state_of(core_text(3, "pass\n"),
                                       core_text(3, "raise\n")))

    def test_a_sibling_ahead_of_the_canonical_copy_is_refused(self):
        # Someone edited the vendored copy in place, which is exactly what
        # lint-mutate-core.py exists to catch over there. Overwriting it here
        # would destroy that change and hide the mistake.
        self.assertEqual("REFUSED",
                         self.state_of(core_text(4), core_text(3)))

    def test_a_missing_checkout_is_reported_not_crashed(self):
        s = sync_core.Sibling("sib", "scripts/mutate", Path("/nonexistent"))
        self.assertEqual("missing", s.state(core_text(3), "x")[0])


class TestCoreVersion(unittest.TestCase):
    def test_it_reads_the_constant(self):
        self.assertEqual(7, sync_core.core_version(core_text(7)))

    def test_a_file_without_one_answers_none(self):
        self.assertIsNone(sync_core.core_version("import os\n"))

    def test_a_mention_in_prose_is_not_the_constant(self):
        # The assignment starts the line; a docstring discussing CORE_VERSION
        # does not. Getting this wrong reads a number out of a comment, which
        # is how report.py once named a function after a word in prose.
        self.assertEqual(5, sync_core.core_version(
            "#: talk about CORE_VERSION = 99 here\nCORE_VERSION = 5\n"))


if __name__ == "__main__":
    import io

    buf = io.StringIO()
    loader = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2, stream=buf).run(loader)
    if not result.wasSuccessful():
        sys.stderr.write(buf.getvalue())
        sys.exit(1)
    print(f"sync-core selftest: {result.testsRun} tests, sync decisions")
