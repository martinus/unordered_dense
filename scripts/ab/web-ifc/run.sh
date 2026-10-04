#!/bin/bash
# interleaved rounds: every binary on every model once per round, order rotated per round
cd ${WIFC:-/home/martinus/gra/wifc320}
ROUNDS=${ROUNDS:-9}
bins=(b-clang-std b-clang-v4.8.1 b-clang-v5.1.0 b-clang-v5.3.1 b-gcc-std b-gcc-v4.8.1 b-gcc-v5.1.0 b-gcc-v5.3.1)
models=(${MODELS:-models/*.ifc})
for ((r = 0; r < ROUNDS; ++r)); do
    for m in "${models[@]}"; do
        n=${#bins[@]}
        for ((i = 0; i < n; ++i)); do
            b=${bins[$(((i + r) % n))]}
            out=$( { /usr/bin/time -f "rss_kb %M" taskset -c 2 ./$b/bench_load "$m" | grep parse_s; } 2>&1 | grep -v '^\[' | tr '\n' ' ')
            echo "round $r bin $b model $(basename $m .ifc) $out"
        done
    done
done
