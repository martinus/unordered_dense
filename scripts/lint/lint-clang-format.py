#!/usr/bin/env python3
"""Check that the sources are clang-format clean, at a pinned version.

The version is pinned because clang-format's output moves between releases: 18 and 21 disagree
about where a trailing return type breaks, so an unpinned run would call a tree formatted by one
of them broken under the other, and "fixing" that would break it for everyone else. 21 is what
this tree is formatted with.

That is also why a version other than the pinned one is skipped rather than run: its complaints
would be about the version, not about the code. Install the pinned one with

    pip install clang-format==21.1.8

or point UNORDERED_DENSE_CLANG_FORMAT at the binary to use.
"""

import os
from pathlib import Path
import re
import shutil
from subprocess import run

ROOT = Path(__file__).resolve().parents[2]
PATTERNS = ["include/**/*.h", "test/**/*.h", "test/**/*.cpp"]
EXCLUDE_RE = re.compile(r"nanobench\.h|FuzzedDataProvider\.h|/third-party/")
PINNED_MAJOR = 21


def collect_files(root: Path):
    return [
        f
        for p in PATTERNS
        for f in root.glob(p)
        if f.is_file() and not EXCLUDE_RE.search(str(f))
    ]


def major_version_of(binary: str):
    """Major version of a clang-format binary, or None if it cannot be asked."""
    res = run([binary, "--version"], capture_output=True, text=True)
    if res.returncode != 0:
        return None
    m = re.search(r"clang-format version (\d+)\.", res.stdout)
    return int(m.group(1)) if m else None


def find_clang_format():
    candidates = [
        os.environ.get("UNORDERED_DENSE_CLANG_FORMAT"),
        f"clang-format-{PINNED_MAJOR}",
        "clang-format",
    ]
    for candidate in candidates:
        if candidate and (path := shutil.which(candidate)) and major_version_of(path) == PINNED_MAJOR:
            return path
    return None


def main():
    files = collect_files(ROOT)
    if not files:
        print("could not find any files!")
        raise SystemExit(1)

    if not (clang_format := find_clang_format()):
        print(
            f"SKIPPED clang-format: no clang-format {PINNED_MAJOR} available. Install it with "
            f"'pip install clang-format=={PINNED_MAJOR}.1.8', or point "
            "UNORDERED_DENSE_CLANG_FORMAT at it."
        )
        raise SystemExit(0)

    ec = run([clang_format, "--dry-run", "-Werror"] + files).returncode
    print(f"clang-format ({clang_format}) checked {len(files)} files")
    raise SystemExit(ec)


if __name__ == "__main__":
    main()
