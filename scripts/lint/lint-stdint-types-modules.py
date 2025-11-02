#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "include" / "ankerl" / "unordered_dense.h"


def build_query():
    # Literal names drawn from C++ wrapper headers (<cstddef>, <ctime>, <cstdio>, <cwchar>)
    names = [
        r"u?int(?:_fast|_least)?\d{1,2}_t",
        r"s?size_t",
        r"ptrdiff_t",
        r"u?intmax_t",
        r"u?intptr_t",
        r"nullptr_t",
        r"max_align_t",
        r"time_t",
        r"clock_t",
        r"tm",
        r"FILE",
        r"fpos_t",
        r"mbstate_t",
        r"wint_t",
    ]
    literal_alts = "|".join(n for n in names)
    return re.compile(r"(?<!std::)\b(?:" + literal_alts + r")\b")


def main():
    errors = []
    query = build_query()
    for lineno, line in enumerate(HEADER.read_text().splitlines(), 1):
        if query.search(line):
            errors.append(f"{HEADER}:{lineno}: {line}")

    if errors:
        print("Found stdint types without std:: prefix:")
        print("\n".join(errors))
        sys.exit(1)


if __name__ == "__main__":
    main()
