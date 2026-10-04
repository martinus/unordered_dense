#!/usr/bin/env python3
"""run.sh's output -> per model and phase: median (min-max) per binary, ratio to std and to 4.8.1 of the same compiler."""
import re
import statistics
import sys
from collections import defaultdict

rows = defaultdict(lambda: defaultdict(list))
for line in open(sys.argv[1]):
    m = re.match(r"round \d+ bin (\S+) model (\S+) (.*)", line)
    if not m:
        continue
    f = m.group(3).split()
    kv = dict(zip(f[0::2], f[1::2]))
    for k in ("parse_s", "geometry_s", "total_s", "rss_kb"):
        rows[(m.group(2), k)][m.group(1)].append(float(kv[k]))

for (model, k), bins in sorted(rows.items()):
    print(f"{model} {k}")
    for b, v in sorted(bins.items()):
        med = statistics.median(v)
        comp = b.split("-")[1]
        ratios = []
        for ref in ("std", "v4.8.1"):
            r = bins.get(f"b-{comp}-{ref}")
            if r:
                ratios.append(f"/{ref} {med / statistics.median(r):.3f}")
        print(f"  {b:16s} {med:10.3f} ({min(v):.3f}-{max(v):.3f})  {'  '.join(ratios)}  n={len(v)}")
