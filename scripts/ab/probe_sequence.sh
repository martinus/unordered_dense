#!/bin/bash
# The probe sequence, one header per variant and one binary per header (#355). The variants are
# written by probe_sequence.py from the working tree's header; `tri` is that header unchanged.
#
#   scripts/ab/probe_sequence.sh probes [variant...]                        # groups per lookup
#   AB_BUILD=/home/martinus/gra/x AB_CORE=2 scripts/ab/probe_sequence.sh time [rounds] [variant...]
#   AB_BUILD=/home/martinus/gra/x AB_CORE=2 scripts/ab/probe_sequence.sh score [rounds] [variant...]
#
# probes: scripts/ab/probe_length.sh for each variant at loads 0.5 / 0.76 / 0.799, churned with 0,
# 1 and 4 writing hits per round, on 4096 groups (200 turnovers) and on 4, 8 and 16 groups (2000
# turnovers), where a step taken from the fingerprint can correlate with the counter class.
# Deterministic: same rng, same keys for every variant.
#
# score: solo.sh (the score, one header per binary, against the working tree's HEAD) and then
# perwl.sh (ins/op per workload), per variant and compiler.
#
# time: a churned table (move_home.cpp, misses and hits, 0 and 1 writing hits per round), fresh
# lookups (prefetch_index.cpp: hit64, hitstr, miss64) and builds from empty (place_inline.cpp),
# both compilers (AB_CXX=clang++ or g++ for one; AB_SMOKE=1 runs the smallest size of each). The variants run rotated within each round; prints the median ns per operation
# of each variant and tri/variant (above 1.00 means the variant is faster than what ships).
set -euo pipefail
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
mode=${1:?probes or time}
shift

headers() { # variant... -> $build/hdr-<v>/ankerl/{unordered_dense.h,stl.h}
    for v in "$@"; do
        mkdir -p "$build/hdr-$v/ankerl"
        cp "$root/include/ankerl/stl.h" "$build/hdr-$v/ankerl/"
        python3 "$root/scripts/ab/probe_sequence.py" "$v" "$root/include/ankerl/unordered_dense.h" \
            "$build/hdr-$v/ankerl/unordered_dense.h"
    done
}

if [ "$mode" = probes ]; then
    build=${AB_BUILD:-$(mktemp -d)}
    variants=("$@")
    [ ${#variants[@]} -gt 0 ] || read -ra variants <<<"$(python3 "$root/scripts/ab/probe_sequence.py" --list)"
    headers "${variants[@]}"
    printf '%-6s %6s %5s %4s   %-28s %-28s\n' variant groups load hits "per hit fresh -> churned" "per miss fresh -> churned"
    for groups in 4096 16 8 4; do
        turnovers=200
        [ "$groups" = 4096 ] || turnovers=2000
        for load in 0.5 0.76 0.799; do
            for hits in 0 1 4; do
                for v in "${variants[@]}"; do
                    AB_HEADER="$build/hdr-$v/ankerl/unordered_dense.h" AB_BUILD="$build/pl-$v" \
                        "$root/scripts/ab/probe_length.sh" "$load" "$turnovers" "$hits" "$groups" |
                        awk -v v="$v" -v g="$groups" -v l="$load" -v h="$hits" '
                            /per hit/  { fh = $5; ch = $7 }
                            /per miss/ { fm = $5; cm = $7 }
                            END { printf "%-6s %6s %5s %4s   %.4f -> %.4f             %.4f -> %.4f\n", v, g, l, h, fh, ch, fm, cm }'
                done
            done
        done
    done
    exit 0
fi

if [ "$mode" = score ]; then
    build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
    rounds=${1:-5}
    shift || true
    variants=("$@")
    [ ${#variants[@]} -gt 0 ] || variants=(dh)
    headers "${variants[@]}"
    for v in "${variants[@]}"; do
        for cxx in clang++ g++; do
            echo "== $v $cxx"
            AB_BUILD="$build/score-$v-$cxx" "$root/scripts/ab/solo.sh" -c "$cxx" \
                -h "$build/hdr-$v/ankerl/unordered_dense.h" "$rounds" | tail -1
            "$root/scripts/ab/perwl.sh" "$build/score-$v-$cxx"
        done
    done
    exit 0
fi

[ "$mode" = time ] || { echo "mode: probes, score or time" >&2; exit 1; }
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
rounds=${1:-5}
shift || true
variants=(tri "$@")
[ $# -gt 0 ] || variants=(tri dh)
read -ra compilers <<<"${AB_CXX:-clang++ g++}"
headers "${variants[@]}"
for v in "${variants[@]}"; do
    for cxx in "${compilers[@]}"; do
        for h in move_home prefetch_index place_inline; do
            "$cxx" -O3 -DNDEBUG -std=c++17 -w -I"$build/hdr-$v" -I"$build/hdr-$v/ankerl" -I"$root/test" \
                "$root/scripts/ab/$h.cpp" -o "$build/${h}_${v}_${cxx}" &
        done
    done
    wait
done

smoke() { if [ -n "${AB_SMOKE:-}" ]; then echo "$1"; else echo "$@"; fi; }

cell() { # label harness cxx args...
    local label=$1 h=$2 cxx=$3
    shift 3
    declare -A res=()
    local n=${#variants[@]}
    for r in $(seq 1 "$rounds"); do
        for k in $(seq 0 $((n - 1))); do
            local v=${variants[$(((r + k) % n))]}
            res[$v]+="$(${AB_CORE:+taskset -c $AB_CORE} "$build/${h}_${v}_${cxx}" "$@" |
                awk '{for (i = 1; i <= NF; i++) if ($i == "ns/op") { print $(i - 1); exit }} NF == 1 { print $1 }') "
        done
    done
    local cols=()
    for v in "${variants[@]}"; do cols+=("$v=${res[$v]}"); done
    python3 - "$cxx" "$label" "${cols[@]}" <<'PY'
import statistics, sys
cxx, label, *cols = sys.argv[1:]
med = {c.split("=")[0]: statistics.median(float(x) for x in c.split("=")[1].split()) for c in cols}
base = med["tri"]
line = f"{cxx:8} {label:30}  tri {base:8.3f}"
for v, m in med.items():
    if v != "tri":
        line += f"   {v} {m:8.3f} {base / m:.3f}"
print(line, flush=True)
PY
}

for cxx in "${compilers[@]}"; do
    for n in $(smoke 52363 838860 3355443); do
        for hits in 0 1; do
            cell "churned miss $n hits=$hits" move_home "$cxx" miss "$n" 40 "$hits" 30000000
            cell "churned hit  $n hits=$hits" move_home "$cxx" hit "$n" 40 "$hits" 30000000
        done
    done
    cell "churn round 52363 hits=1" move_home "$cxx" round 52363 40 1 3000000
    for n in $(smoke 50000 200000 1000000 4000000 16000000); do
        for w in hit64 hitstr miss64; do cell "fresh $w $n" prefetch_index "$cxx" "$w" "$n" 20; done
    done
    for n in $(smoke 32000 200000 1000000); do
        for k in u64 str; do cell "build $k $n" place_inline "$cxx" "$k" "$n" 10; done
    done
done
