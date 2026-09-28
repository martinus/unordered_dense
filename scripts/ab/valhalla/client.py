#!/usr/bin/env python3
# Sends one request file to a running valhalla_service, one request after another, and prints the
# file's total seconds and how many requests came back without an HTTP error.
import json, sys, time, urllib.request
path = sys.argv[1]
ok = 0
start = time.perf_counter()
for line in open(path):
    req = urllib.request.Request("http://localhost:8002/sources_to_targets", data=line.encode(),
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=3600) as r:
            body = json.loads(r.read())
            ok += 1 if "sources_to_targets" in body or "sources" in body else 0
    except urllib.error.HTTPError as e:
        pass
print(f"{time.perf_counter() - start:.3f} {ok}")
