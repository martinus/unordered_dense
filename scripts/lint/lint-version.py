#!/usr/bin/env python3
from pathlib import Path
import re

# fmt: off
ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "include" / "ankerl" / "unordered_dense.h"
CHECKS = [
    (HEADER, r"Version (\d+)\.(\d+)\.(\d+)", 1),
    (ROOT / "CMakeLists.txt", r"^\s+VERSION (\d+)\.(\d+)\.(\d+)", 1),
    (ROOT / "include" / "ankerl" / "stl.h", r"Version (\d+)\.(\d+)\.(\d+)", 1),
    (ROOT / "meson.build", r"version:\s*'?(\d+)\.(\d+)\.(\d+)'?", 1),
    (ROOT / "test" / "unit" / "namespace.cpp", r"unordered_dense::v(\d+)_(\d+)_(\d+)", 1),
]
# fmt: on


def read_version_from_header(p: Path) -> str:
    m = re.findall(
        r"#define\s+ANKERL_UNORDERED_DENSE_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)",
        p.read_text(),
    )
    d = dict(m)
    return f"{d['MAJOR']}.{d['MINOR']}.{d['PATCH']}"


def main():
    ref = read_version_from_header(HEADER)
    errs = []
    for path, pattern, count in CHECKS:
        matches = list(re.finditer(pattern, path.read_text(), re.M))
        if (n := len(matches)) != count:
            errs.append(f"ERROR: {path}: expected {count} matches, found {n}")
        errs.extend(
            f"ERROR: {path}: found version {found}, expected {ref}"
            for m in matches
            if (found := ".".join(m.groups())) != ref
        )

    print("\n".join(errs))
    raise SystemExit(1 if errs else 0)


if __name__ == "__main__":
    main()
