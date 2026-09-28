#!/usr/bin/env python3
# Keep only random points in the Mannheim-Usedom box that route to Kassel (the middle of the
# extract), then write 20 requests of 20 sources and 20 targets from them. Needs a running service.
import json, random, urllib.request, urllib.error
box = (49.493107, 8.475952, 53.923751, 14.210815)
hub = {"lat": 51.3127, "lon": 9.4797}
r = random.Random(3)
good = []
tried = 0
while len(good) < 800:
    tried += 1
    p = {"lat": round(r.uniform(box[0], box[2]), 6), "lon": round(r.uniform(box[1], box[3]), 6)}
    body = json.dumps({"sources": [p], "targets": [hub], "costing": "auto"}).encode()
    try:
        with urllib.request.urlopen(urllib.request.Request("http://localhost:8002/sources_to_targets", data=body,
                                    headers={"Content-Type": "application/json"})) as resp:
            m = json.loads(resp.read())["sources_to_targets"]
            cell = m[0][0] if isinstance(m, list) else None
            if cell and cell.get("time") is not None:
                good.append(p)
    except urllib.error.HTTPError:
        pass
with open("/vb/req/mannheim_usedom_20.jsonl", "w") as f:
    for i in range(20):
        f.write(json.dumps({"sources": good[40 * i:40 * i + 20], "targets": good[40 * i + 20:40 * i + 40],
                            "costing": "auto"}) + "\n")
print(f"{len(good)} of {tried} points connected")
