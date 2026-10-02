import sys,collections
rows=collections.defaultdict(dict)
for f in sys.argv[1:]:
    for l in open(f):
        p=l.split()
        if len(p)<10 or p[-2]!='geomean': continue
        kind,var,work,base=p[0],p[1],p[2],p[3]
        rows[(kind,work,base)][var]=float(p[-1])
order=['seg4096','seg16384','seg65536','seg262144','seg2097152','seg16777216','exact4096','huge-map','huge-seg4096','huge-seg16384','huge-seg65536','huge-seg262144','huge-seg2097152','huge-seg16777216']
for kind in ['u64','str','big']:
  for work in ['buildfree','find','churn','iterate','rss']:
    keys=[k for k in rows if k[0]==kind and k[1]==work]
    if not keys: continue
    bases=sorted({int(k[2]) for k in keys})
    print(f"\n{kind} {work}: ratio to map (lower is better); map absolute")
    print("| variant | "+" | ".join(str(b) for b in bases)+" |")
    print("|---|"+"---|"*len(bases))
    print("| map (ns or B/entry) | "+" | ".join(f"{rows[(kind,work,str(b))].get('map',float('nan')):.3g}" for b in bases)+" |")
    for v in order:
        print(f"| {v} | "+" | ".join(f"{rows[(kind,work,str(b))].get(v,float('nan'))/rows[(kind,work,str(b))]['map']:.2f}" for b in bases)+" |")
