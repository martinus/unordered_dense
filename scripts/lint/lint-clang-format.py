#!/usr/bin/env python3
from pathlib import Path
import re
import shutil
from subprocess import run
import sys

ROOT = Path(__file__).resolve().parents[2]
PATTERNS = ["include/**/*.h", "test/**/*.h", "test/**/*.cpp"]
EXCLUDE_RE = re.compile(r"nanobench\.h|FuzzedDataProvider\.h|/third-party/")


def collect_files(root: Path):
    return [
        f
        for p in PATTERNS
        for f in root.glob(p)
        if f.is_file() and not EXCLUDE_RE.search(str(f))
    ]


def main():
    files = collect_files(ROOT)
    if not files:
        print("could not find any files!")
        raise SystemExit(1)

    if not (clang_format := shutil.which("clang-format")):
        print("clang-format not found in PATH")
        raise SystemExit(2)

    ec = run([clang_format, "--dry-run", "-Werror"] + files).returncode
    print(f"clang-format checked {len(files)} files")
    SystemExit(ec)


if __name__ == "__main__":
    main()
