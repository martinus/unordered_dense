#!/usr/bin/env python3
# valhalla#4552's request files: 20 random sources_to_targets requests per file, n sources and n
# targets each, uniformly inside a bounding box, auto costing. Fixed seeds, so every variant gets the
# same requests.
import json, random, sys
boxes = {
    "berlin": (52.3383, 13.0884, 52.6755, 13.7611),          # lat_min, lon_min, lat_max, lon_max
    "mannheim_usedom": (49.493107, 8.475952, 53.923751, 14.210815),  # the bbox in the comment
}
def point(r, b):
    return {"lat": round(r.uniform(b[0], b[2]), 6), "lon": round(r.uniform(b[1], b[3]), 6)}
for name, n, seed in [("berlin", 20, 1), ("berlin", 50, 2), ("mannheim_usedom", 20, 3)]:
    r = random.Random(seed)
    with open(f"/home/martinus/gra/valbench/req/{name}_{n}.jsonl", "w") as f:
        for _ in range(20):
            req = {"sources": [point(r, boxes[name]) for _ in range(n)],
                   "targets": [point(r, boxes[name]) for _ in range(n)],
                   "costing": "auto"}
            f.write(json.dumps(req) + "\n")
