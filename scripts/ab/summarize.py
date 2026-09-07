#!/usr/bin/env python3
"""Turn one ab run's output into a markdown table.

The harness measures each size-sensitive workload at several sizes across one octave and prints a
`#ratio <workload> <base/cand> <boost/cand> <points>` line per workload carrying the geometric mean
of those. That line is what this reads. Ratios are the other map's time over the candidate's, so
above 1.00 always means the working tree is faster, whichever column it is.

Runs made before 2026-09-07 have no `#ratio` lines, only nanobench's own tables at one size per
workload; those are still parsed, so an old log still summarises. They are not comparable with a
swept run, and the output says which it read.
"""
import math
import re
import sys

# the fifteen the score is the geomean of, then the lookup-only extras the harness also runs
SCORED = ["it64", "ie64", "build64", "churn64", "find64",
          "itstr", "iestr", "buildstr", "churnstr", "findstr",
          "itbig", "iebig", "buildbig", "churnbig", "findbig"]
EXTRA = ["rhit64", "rmiss64", "rhitstr", "rmissstr", "hashstr"]
GROUPS = [("score, 15 workloads", SCORED),
          ("without iteration", [w for w in SCORED if not w.startswith("it")]),
          ("build", [w for w in SCORED if w.startswith("build")]),
          ("churn", [w for w in SCORED if w.startswith("churn")]),
          ("find", [w for w in SCORED if w.startswith("find")]),
          ("insert and erase", [w for w in SCORED if w.startswith("ie")])]


def parse(text):
    """{workload: {'vs_base': r, 'vs_boost': r, 'points': n}} from the harness's #ratio lines."""
    out = {}
    for line in text.splitlines():
        if not line.startswith("#ratio "):
            continue
        parts = line.split()
        if len(parts) != 5:
            continue
        try:
            out[parts[1]] = {"vs_base": float(parts[2]), "vs_boost": float(parts[3]), "points": int(parts[4])}
        except ValueError:
            pass
    return out


def parse_legacy(text):
    """The same, from nanobench's tables, for a log made before the harness swept sizes."""
    out, name = {}, None
    for line in text.splitlines():
        if line.startswith("| relative"):
            name = line.rsplit("|", 1)[1].strip()
            out[name] = {}
        elif name and line.startswith("|") and "`" in line:
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            who = cells[-1].strip("`")
            try:
                out[name][who] = float(cells[2].replace(",", "").replace(" ", ""))
            except ValueError:
                pass
    return out


def to_ratios(ns):
    """nanobench ns/op per alternative -> the ratios the tables are built from."""
    out = {}
    for w, row in ns.items():
        if "base" not in row or "cand" not in row:
            continue
        out[w] = {"vs_base": row["base"] / row["cand"], "points": 1}
        if "boost" in row:
            out[w]["vs_boost"] = row["boost"] / row["cand"]
    return out


def geomean(xs):
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else float("nan")


def main():
    text = open(sys.argv[1]).read() if len(sys.argv) > 1 else sys.stdin.read()
    res = parse(text)
    if not res:
        res = to_ratios(parse_legacy(text))
    have = [w for w in SCORED + EXTRA if w in res]
    if not have:
        print("no complete workload found: the run produced nothing to compare")
        return 1
    with_boost = all("vs_boost" in res[w] for w in have)
    swept = [res[w]["points"] for w in have if res[w]["points"] > 1]

    print("| workload | vs main |" + (" vs boost |" if with_boost else "") + " sizes |")
    print("|---|---|" + ("---|" if with_boost else "") + "---|")
    for w in have:
        row = f"| {w} | {res[w]['vs_base']:.2f} |"
        if with_boost:
            row += f" {res[w]['vs_boost']:.2f} |"
        print(row + f" {res[w]['points']} |")

    print()
    print("| geomean | vs main |" + (" vs boost |" if with_boost else ""))
    print("|---|---|" + ("---|" if with_boost else ""))
    for label, workloads in GROUPS:
        ws = [w for w in workloads if w in have]
        if not ws:
            continue
        row = f"| {label} | **{geomean([res[w]['vs_base'] for w in ws]):.3f}** |"
        if with_boost:
            row += f" **{geomean([res[w]['vs_boost'] for w in ws]):.3f}** |"
        print(row)
    print()
    print(f"_{len(have)} workloads; above 1.00 means this branch is faster._")
    if swept:
        lo, hi = min(swept), max(swept)
        span = str(lo) if lo == hi else f"{lo}-{hi}"
        print(f"_Each ratio is the geometric mean over {span} sizes spanning one octave. Iteration and "
              "hashstr are measured at one size: neither has a bucket array whose load factor sweeps._")
    else:
        print("_One size per workload, so each ratio is a single point on each map's load-factor "
              "sawtooth and is comparable only with other single-size runs._")
    return 0


if __name__ == "__main__":
    sys.exit(main())
