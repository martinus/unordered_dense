#!/usr/bin/env python3
import argparse
import os
import shutil
from subprocess import run
import sys


def main():
    p = argparse.ArgumentParser(
        description=(
            "Run clang-tidy on unordered_dense.h (via test/unit/include_only.cpp)"
        )
    )
    p.add_argument("--std", default="c++17", help="C++ standard (default: c++17)")
    p.add_argument(
        "--binary",
        default=os.environ.get("UNORDERED_DENSE_CLANG_TIDY", "clang-tidy"),
        help=(
            "clang-tidy to run, also settable via UNORDERED_DENSE_CLANG_TIDY. CI pins a "
            "version here: .clang-tidy enables whole families, families gain checks in every "
            "release, so an unpinned clang-tidy turns a toolchain bump into a red build."
        ),
    )
    args = p.parse_args()

    if not (clang_tidy := shutil.which(args.binary)):
        print(f"Error: {args.binary} not found in PATH", file=sys.stderr)
        raise SystemExit(1)

    # Exit with clang-tidy's exit code.
    ec = run(
        [
            clang_tidy,
            "test/unit/include_only.cpp",
            "--header-filter=.*unordered_dense\\.h",
            "--warnings-as-errors=*",
            "--",
            f"-std={args.std}",
            "-I",
            "include",
        ]
    ).returncode
    raise SystemExit(ec)


if __name__ == "__main__":
    main()
