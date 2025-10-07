#!/usr/bin/env python3
"""Run clang-tidy only on unordered_dense.h"""

import argparse
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Run clang-tidy on unordered_dense.h (via include_only.cpp)"
    )
    parser.add_argument(
        "--std",
        default="c++17",
        help="C++ standard to use (default: c++17)",
    )
    args = parser.parse_args()

    # Define paths
    source_file = Path("test/unit/include_only.cpp")

    cmd = [
        "clang-tidy",
        str(source_file),
        "--header-filter=.*unordered_dense\\.h",
        "--warnings-as-errors=*",
        "--",
        f"-std={args.std}",
        "-I",
        "include",
    ]

    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
    except FileNotFoundError:
        print("Error: clang-tidy not found in PATH", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
