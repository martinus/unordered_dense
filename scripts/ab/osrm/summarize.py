#!/usr/bin/env python3
# Medians of run.sh's output (client requests per second) and of the server-side time per method,
# summed from each osrm-routed log, both as ratios to the std build, for every other build found.
import collections, glob, os, re, statistics as st, sys
O = sys.argv[1] if len(sys.argv) > 1 else "/home/martinus/gra/osrmbench"
client = collections.defaultdict(list)
for line in open(os.path.join(O, "results.txt")):
    f = line.split()
    if len(f) == 5 and f[4]:
        client[(f[2], f[3], f[1])].append(float(f[4]))
server = collections.defaultdict(list)
pat = re.compile(r" ([0-9.]+)ms 127\.0\.0\.1 - python-requests\S* (\d+) /(\w+)/v1/")
for fn in glob.glob(os.path.join(O, "logs", "r*-*-*.log")):
    _, v, alg = os.path.basename(fn)[:-4].split("-")
    total = collections.defaultdict(float)
    for line in open(fn):
        m = pat.search(line)
        if m:
            total[m.group(3)] += float(m.group(1))
    for method, t in total.items():
        server[(alg, method, v)].append(t)
variants = sorted({k[2] for k in client} - {"std"})
for alg in ("ch", "mld"):
    for m in ("route", "nearest", "trip", "table", "match"):
        b, sb = st.median(client[(alg, m, "std")]), st.median(server[(alg, m, "std")])
        cells = [f"{alg} {m}: client std {b:.1f}/s"]
        for v in variants:
            xs = client[(alg, m, v)]
            cells.append(f"{v} {st.median(xs) / b:.3f} ({min(xs) / b:.3f}-{max(xs) / b:.3f})")
        cells.append(f"| server std {sb:.1f} ms, " + ", ".join(f"{v} {st.median(server[(alg, m, v)]) / sb:.3f}" for v in variants))
        print("  ".join(cells))
