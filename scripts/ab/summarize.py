#!/usr/bin/env python3
"""Turn one ab run's output into a markdown table.

The harness prints a nanobench table per workload with a `base` row, a `cand` row and optionally a
`boost` row. What a reader wants from a run is the ratio per workload and the geometric means, so
this reads the ns/op column back out and prints exactly that. Ratios are the other map's time over
the candidate's, so above 1.00 always means the working tree is faster, whichever column it is.
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
    """{workload: {'base': ns, 'cand': ns, 'boost': ns}} from the harness's tables."""
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


def geomean(xs):
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else float("nan")


def main():
    text = open(sys.argv[1]).read() if len(sys.argv) > 1 else sys.stdin.read()
    res = parse(text)
    have = [w for w in SCORED + EXTRA if w in res and "cand" in res[w] and "base" in res[w]]
    if not have:
        print("no complete workload found: the run produced nothing to compare")
        return 1
    with_boost = all("boost" in res[w] for w in have)

    print("| workload | vs main |" + (" vs boost |" if with_boost else ""))
    print("|---|---|" + ("---|" if with_boost else ""))
    for w in have:
        row = f"| {w} | {res[w]['base'] / res[w]['cand']:.2f} |"
        if with_boost:
            row += f" {res[w]['boost'] / res[w]['cand']:.2f} |"
        print(row)

    print()
    print("| geomean | vs main |" + (" vs boost |" if with_boost else ""))
    print("|---|---|" + ("---|" if with_boost else ""))
    for label, workloads in GROUPS:
        ws = [w for w in workloads if w in have]
        if not ws:
            continue
        row = f"| {label} | **{geomean([res[w]['base'] / res[w]['cand'] for w in ws]):.3f}** |"
        if with_boost:
            row += f" **{geomean([res[w]['boost'] / res[w]['cand'] for w in ws]):.3f}** |"
        print(row)
    print()
    print(f"_{len(have)} workloads; above 1.00 means this branch is faster._")
    return 0


if __name__ == "__main__":
    sys.exit(main())
