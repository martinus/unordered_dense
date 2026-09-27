#!/bin/bash
# The shape search: which parts of the insert path are inlined into the caller and which are called,
# decided by measurement instead of argument (#310). gen.py writes 16 headers from the tree's
# header; each stage measures all of them and appends "shape<TAB>compiler<TAB>metric<TAB>value" rows
# to $SS/results.tsv; rank.py grades every shape by its worst ratio to the best shape on any timed
# metric, per compiler. One stage per call, each under ~45 minutes, never two at once:
#
#   run.sh gen                 # the 16 headers, from include/ankerl/unordered_dense.h
#   run.sh score clang++       # the score's instructions per workload (deterministic), ~10 min
#   run.sh score g++
#   run.sh udb3                # attractivechaos/udb3, insert and insert+delete, ~20 min
#   run.sh ch 1 8              # ClickHouse aggregation, WatchID and CounterID, ~25 min per half
#   run.sh ch 9 16
#   run.sh corpus clang++      # the caller corpus, 2 rounds, ~30 min
#   run.sh corpus g++
#   run.sh rank
#
# Environment: SS the work directory (default /home/martinus/gra/shapesearch; on disk, not /tmp),
# UDB3 a udb3 checkout with ud520/test.cpp (default /home/martinus/gra/udb3/flatswan), CH the
# hash-table-aggregation-benchmark checkout with contrib/udm321, build/, build-gcc/ and data/
# (default /home/martinus/gra/hash-table-aggregation-benchmark/lazytaco), AB_CORE (default 2).
# The 2026-09-27 run and what it decided: notes/index-design.md, "shape search".
set -euo pipefail
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
here=$(cd "$(dirname "$0")" && pwd)
SS=${SS:-/home/martinus/gra/shapesearch}
UDB3=${UDB3:-/home/martinus/gra/udb3/flatswan}
CH=${CH:-/home/martinus/gra/hash-table-aggregation-benchmark/lazytaco}
core=${AB_CORE:-2}
mkdir -p "$SS"
res=$SS/results.tsv
names() { cat "$SS/names.txt"; }

case ${1:?stage} in
gen)
    python3 "$here/gen.py" "$root/include/ankerl/unordered_dense.h" "$SS"
    : > "$res"
    ;;
score)
    cxx=${2:?compiler}
    for n in $(names); do
        src=$SS/src/$n
        if [ ! -d "$src" ]; then
            mkdir -p "$src"
            tar -C "$root" --exclude=./builddir --exclude=./.git --exclude=./subprojects/packagecache -cf - . | tar -C "$src" -xf -
            cp "$SS/shapes/$n/ankerl/unordered_dense.h" "$src/include/ankerl/unordered_dense.h"
        fi
        d=$src/bd_$cxx
        [ -f "$d/build.ninja" ] || CXX=$cxx meson setup --buildtype release "$d" "$src" >/dev/null
        ninja -C "$d" test/udm-test >/dev/null
        taskset -c "$core" "$d/test/udm-test" -ns -tc=bench_quick_overall_udm 2>/dev/null |
            awk -F'|' -v n="$n" -v c="$cxx" '/ankerl::unordered_dense::map</ {
                v = $5; gsub(/[ ,%]/, "", v); w = $11; gsub(/^ +| +$|`/, "", w)
                sub(/ankerl::unordered_dense::map/, "", w); gsub(/ /, "_", w)
                print n "\t" c "\tscore_ins:" w "\t" v }' >> "$res"
    done
    ;;
udb3)
    for n in $(names); do
        d=$UDB3/ss_$n
        mkdir -p "$d"
        cp "$SS/shapes/$n/ankerl/"*.h "$d/" && cp "$UDB3/ud520/test.cpp" "$d/"
        for cxx in clang++ g++; do (cd "$d" && $cxx -O3 -Wall -std=c++17 -I. -I.. test.cpp -o run-$cxx); done
    done
    for cxx in clang++ g++; do for mode in "" "-d"; do for n in $(names); do
        vals=()
        for r in 1 2 3; do
            vals+=($(cd "$UDB3" && taskset -c "$core" "./ss_$n/run-$cxx" $mode | grep -E '^M[ID]' | awk '{s+=$7; k++} END {printf "%.3f", s/k*1000}'))
        done
        printf "%s\t%s\tudb3_%s\t%s\n" "$n" "$cxx" "$([ -z "$mode" ] && echo insert || echo del)" \
            "$(printf "%s\n" "${vals[@]}" | sort -n | sed -n 2p)" >> "$res"
    done; done; done
    ;;
ch)
    rows=99997497
    for n in $(names | sed -n "${2:?first},${3:?last}p"); do
        for f in unordered_dense.h stl.h; do
            sed 's/ankerl::unordered_dense/udm321::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDM321_UNORDERED_DENSE/g; s/namespace ankerl/namespace udm321/g' \
                "$SS/shapes/$n/ankerl/$f" > "$CH/contrib/udm321/$f"
        done
        (cd "$CH/build" && make -j32 >/dev/null 2>&1) && (cd "$CH/build-gcc" && make -j32 >/dev/null 2>&1)
        for pair in build:clang++ build-gcc:g++; do
            b=${pair%%:*} cxx=${pair##*:}
            for col in WatchID CounterID; do
                cyc=() ins=0
                for r in 1 2 3; do
                    o=$(taskset -c "$core" perf stat -x, -e instructions,cycles "$CH/$b/src/hash_table_aggregation_benchmark" \
                        ankerl_unordered_dense_321_hash_map absl_hash "$CH/data/$col.bin" 2>&1 >/dev/null)
                    ins=$(echo "$o" | awk -F, '/instructions/ {print $1}')
                    cyc+=($(echo "$o" | awk -F, '/cycles/ {print $1}'))
                done
                med=$(printf "%s\n" "${cyc[@]}" | sort -n | sed -n 2p)
                printf "%s\t%s\tch_%s_cyc\t%s\n" "$n" "$cxx" "$col" "$(awk -v c="$med" -v r=$rows 'BEGIN{printf "%.3f", c/r}')" >> "$res"
                printf "%s\t%s\tch_%s_ins\t%s\n" "$n" "$cxx" "$col" "$(awk -v c="$ins" -v r=$rows 'BEGIN{printf "%.3f", c/r}')" >> "$res"
            done
        done
    done
    ;;
corpus)
    cxx=${2:?compiler}
    hdrs=()
    for n in $(names); do hdrs+=("$SS/shapes/$n/ankerl/unordered_dense.h"); done
    AB_BUILD=$SS/corpus_$cxx AB_CORE=$core "$root/scripts/ab/caller_corpus.sh" -c "$cxx" -r 2 "${hdrs[@]}" > "$SS/corpus_$cxx.txt"
    python3 - "$SS" "$cxx" <<'PY'
import collections, statistics, sys
ss, cxx = sys.argv[1], sys.argv[2]
names = open(f"{ss}/names.txt").read().split()
cyc = collections.defaultdict(list)
for line in open(f"{ss}/corpus_{cxx}/raw.txt"):
    c, h, loop, n, i, cy = line.split()
    cyc[(int(h), loop, n)].append(float(cy))
with open(f"{ss}/results.tsv", "a") as out:
    for (h, loop, n), v in sorted(cyc.items()):
        out.write(f"{names[h]}\t{cxx}\tcorpus_{loop}_{n}\t{statistics.median(v):.3f}\n")
PY
    ;;
rank)
    python3 "$here/rank.py" "$res"
    ;;
*)
    echo "unknown stage $1" >&2
    exit 1
    ;;
esac
