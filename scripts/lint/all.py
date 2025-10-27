#!/usr/bin/env python3

from pathlib import Path
from subprocess import run
from time import perf_counter


def main():
    start = perf_counter()
    linters_dir = Path(__file__).parent
    linters = sorted(
        p for p in linters_dir.iterdir() if p.is_file() and p.name.startswith("lint-")
    )

    rc = 0
    for lint in linters:
        res = run([str(lint)])
        if res.returncode:
            print(f"^---- failure(s) from {lint.name}\n")
            rc |= res.returncode

    print(f"{len(linters)} linters in {perf_counter() - start:0.2f}s")
    raise SystemExit(rc)


if __name__ == "__main__":
    main()
