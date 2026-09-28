#!/usr/bin/env bash
# ROUNDS rounds; per round every variant (order rotated) and algorithm gets a fresh osrm-routed with
# one thread on core 2, and CI's e2e client (1 iteration x 1000 seeded requests after 50 warm-ups)
# on core 3 for each method. Prints: round variant algorithm method ops/s
set -uo pipefail
rounds=${1:-3}
vars=(${VARS:-std seg map})
mkdir -p /home/martinus/gra/osrmbench/logs
for ((r = 0; r < rounds; ++r)); do
  for ((i = 0; i < ${#vars[@]}; ++i)); do
    v=${vars[$(((i + r) % ${#vars[@]}))]}
    for alg in ch mld; do
      podman run --rm -v /home/martinus/gra/osrmbench:/ob:Z localhost/osrmbuild:latest bash -c "
        taskset -c 2 /ob/build-$v/osrm-routed --algorithm $alg -t 1 /ob/data/berlin.osrm >/ob/logs/r$r-$v-$alg.log 2>&1 &
        for t in \$(seq 1 60); do curl -sf 'http://127.0.0.1:5000/route/v1/driving/13.388860,52.517037;13.385983,52.496891' >/dev/null && break; sleep 1; done
        for m in route nearest trip table match; do
          ops=\$(taskset -c 3 python3 /ob/src-std/scripts/ci/e2e_benchmark.py --host http://127.0.0.1:5000 --method \$m --iterations 1 --num_requests 1000 --gps_traces_file_path /ob/gps_traces.csv | awk '/^Ops:/{print \$2}')
          echo \"$r $v $alg \$m \$ops\"
        done
      "
    done
  done
done
