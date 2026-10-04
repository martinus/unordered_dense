#!/bin/bash
# interleaved rounds: every binary on every model once per round, order rotated per round
cd ${WIFC:-/home/martinus/gra/wifc320}
ROUNDS=${ROUNDS:-11}
bins=(b-*/bench_load) # what build.sh built
n=${#bins[@]}
models=(${MODELS:-models/*.ifc})
for ((r = 0; r < ROUNDS; ++r)); do
    for m in "${models[@]}"; do
        for ((i = 0; i < n; ++i)); do
            b=$(dirname ${bins[$(((i + r) % n))]})
            out=$( { /usr/bin/time -f "rss_kb %M" taskset -c 2 ./$b/bench_load "$m" | grep parse_s; } 2>&1 | grep -v '^\[' | tr '\n' ' ')
            echo "round $r bin $b model $(basename $m .ifc) $out"
        done
    done
done
