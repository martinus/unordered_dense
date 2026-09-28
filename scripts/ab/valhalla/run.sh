#!/usr/bin/env bash
# ROUNDS rounds; per round every variant (order rotated) gets a fresh long-running valhalla_service
# (one worker, cores 2-3), a warm-up request, then the three request files timed.
set -uo pipefail
V=/home/martinus/gra/valbench
rounds=${1:-4}
vars=(v450 main absl)
files=(${FILES:-berlin_20 berlin_50 mannheim_usedom_20})
for ((r = 0; r < rounds; ++r)); do
    for ((i = 0; i < ${#vars[@]}; ++i)); do
        v=${vars[$(((i + r) % ${#vars[@]}))]}
        podman run --rm -v $V:/vb:Z localhost/valbuild-absl:latest bash -c "
            taskset -c 2,3 /vb/build-$v/valhalla_service /vb/valhalla.json 1 >/dev/null 2>&1 &
            for t in \$(seq 1 120); do python3 -c 'import urllib.request; urllib.request.urlopen(\"http://localhost:8002/status\")' 2>/dev/null && break; sleep 1; done
            python3 /vb/client.py /vb/req/berlin_20.jsonl >/dev/null
            for f in ${files[*]}; do echo \"$r $v \$f \$(python3 /vb/client.py /vb/req/\$f.jsonl)\"; done
        "
    done
done
