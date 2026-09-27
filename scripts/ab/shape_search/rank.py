#!/usr/bin/env python3
"""Rank the shapes per compiler: rank.py [results.tsv]

Every metric is normalised to the best shape on that metric (1.00 = the best). A shape's grade is
its worst ratio over the timed metrics (caller corpus cycles, udb3 ns, ClickHouse cycles): the
largest slowdown it causes anywhere measured. Second column: the geomean of its ratios on the
score's instruction counts (deterministic, not time, so kept out of the worst case). The Pareto set
over the two is marked with *.
"""
import collections, math, sys
rows = [l.rstrip("\n").split("\t") for l in open(sys.argv[1] if len(sys.argv) > 1 else "results.tsv") if l.strip()]
data = collections.defaultdict(dict)  # (compiler) -> {(shape, metric): value}
for shape, cxx, metric, value in rows:
    data[cxx][(shape, metric)] = float(value)
anchors = {}  # optional labels for known shapes, e.g. {"g++": {"hi_mi_fi_F": "5.2.0"}}
for cxx in sorted(data):
    d = data[cxx]
    shapes = sorted({s for s, _ in d})
    metrics = sorted({m for _, m in d})
    best = {m: min(d[(s, m)] for s in shapes if (s, m) in d) for m in metrics}
    table = []
    for s in shapes:
        timed = [(d[(s, m)] / best[m], m) for m in metrics if (s, m) in d and (m.startswith(("corpus_", "udb3_")) or m.endswith("_cyc"))]
        ins = [d[(s, m)] / best[m] for m in metrics if (s, m) in d and m.startswith("score_ins")]
        worst, where = max(timed) if timed else (float("nan"), "-")
        gm = math.exp(sum(math.log(x) for x in ins) / len(ins)) if ins else float("nan")
        table.append((worst, gm, s, where, len(timed), len(ins)))
    pareto = {t[2] for t in table if not any(o[0] <= t[0] and o[1] <= t[1] and (o[0] < t[0] or o[1] < t[1]) for o in table)}
    print(f"== {cxx}: worst timed ratio (and where), score instructions geomean ratio; * = Pareto")
    for worst, gm, s, where, nt, ni in sorted(table):
        tag = anchors.get(cxx, {}).get(s, "")
        print(f"  {'*' if s in pareto else ' '} {s:12} {tag:6} worst {worst:5.2f} ({where:28}) score-ins {gm:5.3f}   [{nt} timed, {ni} ins]")
