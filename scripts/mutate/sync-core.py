#!/usr/bin/env python3
"""Propagate mutate_core.py to the repositories that vendor it.

This exists because the vendoring failed exactly once and in the dullest
possible way. The file was copied into all three working trees, `.sha256`
updated in each, and then two of the three pull requests were never opened -
one branch pushed with no PR, one branch not pushed at all. For weeks
nanobench and unordered_dense ran a core older than oans's, with every
`lint-mutate-core.py` **green**, because each records the hash of whatever it
has committed and no lint in any of them can see the other two.

So the errand is the thing to automate, not the copying. Copying was never the
part anyone got wrong.

    scripts/mutate/sync-core.py --check       # are the three in sync?
    scripts/mutate/sync-core.py --dry-run     # what would be done
    scripts/mutate/sync-core.py               # branch, copy, commit, push, PR

`--check` is the mode worth wiring into a habit: it answers in one command the
question that was unanswerable from inside any single repository.

This is deliberately *not* part of mutate_core.py. It is about the repositories
rather than about mutation testing, it only ever runs here, and putting it in
the shared file would mean the thing that manages the vendoring is itself
vendored.
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CANON = HERE / "mutate_core.py"
REPO = HERE.parent.parent

#: Where each sibling keeps its copy, relative to that repository's root. The
#: paths differ because the repositories organise scripts differently, which is
#: precisely the sort of detail that makes a hand-run errand go wrong.
SIBLINGS = {
    "nanobench": "src/scripts/mutate",
    "oans": "scripts/mutate",
}

BRANCH = "sync-mutate-core"


def sha256_text(text):
    return hashlib.sha256(text.encode()).hexdigest()


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def core_version(text):
    for line in text.split("\n"):
        if line.startswith("CORE_VERSION"):
            return int(line.split("=")[1].strip())
    return None


def git(repo, *args, check=True):
    return subprocess.run(["git", "-C", str(repo), *args],
                          capture_output=True, text=True, check=check)


class Sibling:
    def __init__(self, name, subdir, root):
        self.name, self.root = name, root
        self.core = root / subdir / "mutate_core.py"
        self.hashfile = root / subdir / "mutate_core.sha256"

    @property
    def exists(self):
        return self.core.is_file()

    def state(self, canon_text, canon_hash):
        """(status, detail) for one sibling, without touching anything."""
        if not self.exists:
            return "missing", f"no checkout at {self.root}"
        mine = self.core.read_text()
        if sha256(self.core) == canon_hash:
            return "in sync", f"v{core_version(mine)}"
        theirs, ours = core_version(mine), core_version(canon_text)
        if theirs is not None and ours is not None and theirs >= ours:
            # The guard on forgetting to bump. Two different files claiming one
            # version is worse than no version, so this refuses rather than
            # quietly overwriting and leaving the numbers meaningless.
            return "REFUSED", (f"differs but its version is v{theirs} against "
                               f"this one's v{ours} - bump CORE_VERSION here")
        return "stale", f"v{theirs} -> v{ours}"

    def dirty(self):
        return bool(git(self.root, "status", "--porcelain").stdout.strip())


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="report whether every copy matches; exit 1 if not")
    ap.add_argument("--dry-run", action="store_true",
                    help="say what would happen, change nothing")
    ap.add_argument("--siblings-root", type=Path, default=REPO.parent,
                    help="directory holding the sibling checkouts "
                         "(default: %(default)s)")
    ap.add_argument("--no-test", action="store_true",
                    help="skip this repository's own core test suite")
    args = ap.parse_args(argv)

    if not CANON.is_file():
        sys.exit(f"no canonical core at {CANON}")
    canon_text = CANON.read_text()
    canon_hash = sha256(CANON)
    print(f"canonical  {CANON.relative_to(REPO)}  "
          f"v{core_version(canon_text)}  {canon_hash[:12]}\n")

    sibs = [Sibling(n, d, args.siblings_root / n) for n, d in SIBLINGS.items()]
    states = {s.name: s.state(canon_text, canon_hash) for s in sibs}
    for s in sibs:
        status, detail = states[s.name]
        print(f"  {s.name:18s} {status:9s} {detail}")

    if any(st == "REFUSED" for st, _ in states.values()):
        sys.exit("\nrefusing to sync: see above")
    if args.check:
        bad = [n for n, (st, _) in states.items() if st != "in sync"]
        if bad:
            sys.exit(f"\nout of sync: {', '.join(bad)}")
        print("\nall copies match")
        return 0

    todo = [s for s in sibs if states[s.name][0] == "stale"]
    if not todo:
        print("\nnothing to do")
        return 0

    # The core is only worth propagating if its own suite passes here - the same
    # baseline discipline the mutation tool applies to itself before scoring.
    if not args.no_test and not args.dry_run:
        print("\nrunning this repository's core suite first")
        r = subprocess.run([sys.executable, str(REPO / "scripts/test_mutate.py")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(r.stdout + r.stderr)
            sys.exit("core suite failed - not propagating")
        print("  ok")

    for s in todo:
        print(f"\n=== {s.name}")
        if s.dirty():
            print(f"  SKIPPED: {s.root} has uncommitted changes")
            continue
        if args.dry_run:
            print(f"  would branch {BRANCH}, copy, commit, push and open a PR")
            continue
        git(s.root, "fetch", "origin")
        default = git(s.root, "symbolic-ref", "--short",
                      "refs/remotes/origin/HEAD", check=False).stdout.strip()
        default = default.split("/")[-1] if default else "main"
        git(s.root, "checkout", "-B", BRANCH, f"origin/{default}")
        shutil.copy2(CANON, s.core)
        s.hashfile.write_text(sha256(s.core) + "\n")
        git(s.root, "add", str(s.core), str(s.hashfile))
        git(s.root, "commit", "-m", commit_message(canon_text))
        git(s.root, "push", "-u", "--force-with-lease", "origin", BRANCH)
        print(f"  pushed {BRANCH}")
        if shutil.which("gh"):
            r = subprocess.run(
                ["gh", "pr", "create", "--repo", f"martinus/{s.name}",
                 "--base", default, "--head", BRANCH,
                 "--title", f"Sync mutate_core.py to v{core_version(canon_text)}",
                 "--body", pr_body(canon_text)],
                capture_output=True, text=True, cwd=s.root)
            print("  " + (r.stdout.strip() or r.stderr.strip()))
        else:
            # Honest rather than silent: no gh here means the PR is still to
            # open, and an unopened PR is the whole failure this script exists
            # to prevent.
            print(f"  NO PR OPENED - gh not on PATH. Run:\n"
                  f"    gh pr create --repo martinus/{s.name} "
                  f"--base {default} --head {BRANCH}")
    return 0


def commit_message(canon_text):
    return (f"Sync mutate_core.py to v{core_version(canon_text)}\n"
            "\n"
            "Re-vendored from unordered_dense, where the change was made and\n"
            "where scripts/test_mutate.py covers it. Only the shared core and\n"
            "its recorded hash change; the adapter beside it is this\n"
            "repository's own and is untouched.\n")


def pr_body(canon_text):
    return (f"Re-vendored `mutate_core.py` at v{core_version(canon_text)} from "
            "unordered_dense, where the change was made and where "
            "`scripts/test_mutate.py` covers it — that suite exercises the "
            "backends and harnesses every vendoring repository runs, not just "
            "its own.\n\n"
            "Only the shared core and its recorded hash change; the adapter "
            "beside it is this repository's own and is untouched.\n\n"
            "Opened by `scripts/mutate/sync-core.py`.\n\n"
            "---\n_Generated by [Claude Code](https://claude.ai/code)_\n")


if __name__ == "__main__":
    sys.exit(main())
