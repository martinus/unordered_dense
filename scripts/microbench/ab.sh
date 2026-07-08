#!/usr/bin/env bash
set -euo pipefail

# A/B benchmark runner for the unordered_dense micro workloads (micro.cpp).
#
# Shared/virtualized machines drift by >10% over minutes, so never compare a
# "before" run against a later "after" run. This script builds a BASELINE
# binary from a git ref and a CANDIDATE binary from the current working tree,
# then runs them strictly interleaved (A B A B ...) and counts pairwise wins.
# Trust a change only if it wins (almost) every pair.
#
# usage:
#   ab.sh build [<git-ref>]              rebuild both binaries; baseline header
#                                        taken from <git-ref> (default: HEAD)
#   ab.sh run <workload> [pairs] [reps]  interleave baseline vs candidate
#   ab.sh all [pairs] [reps]             run all six workloads
#
# workloads: ie64 iestr find64 findstr it64 itstr
# defaults:  pairs=6, reps=3 (each measurement is the min of <reps> in-process runs)

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(git -C "$DIR" rev-parse --show-toplevel)"
BUILD="$DIR/build"
CXX="${CXX:-clang++}"
CXXFLAGS="${CXXFLAGS:--O3 -DNDEBUG -std=c++17}"

build() {
    local ref="${1:-HEAD}"
    mkdir -p "$BUILD/base_include/ankerl"
    git -C "$ROOT" show "$ref:include/ankerl/unordered_dense.h" > "$BUILD/base_include/ankerl/unordered_dense.h"
    git -C "$ROOT" show "$ref:include/ankerl/stl.h" > "$BUILD/base_include/ankerl/stl.h"
    echo "building candidate (working tree) ..."
    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS -I"$ROOT/include" -I"$ROOT/test" \
        "$DIR/micro.cpp" "$DIR/nanobench_impl.cpp" -o "$BUILD/micro_cand"
    echo "building baseline ($ref) ..."
    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS -I"$BUILD/base_include" -I"$ROOT/test" \
        "$DIR/micro.cpp" "$DIR/nanobench_impl.cpp" -o "$BUILD/micro_base"
    echo "done: $BUILD/micro_base $BUILD/micro_cand"
}

run_one() {
    local w="$1" pairs="${2:-6}" reps="${3:-3}"
    if [[ ! -x "$BUILD/micro_base" || ! -x "$BUILD/micro_cand" ]]; then
        echo "binaries missing, run '$0 build [<git-ref>]' first" >&2
        exit 1
    fi
    local awins=0 bwins=0 asum=0 bsum=0 ra rb
    echo "== $w (baseline vs candidate, $pairs pairs, min of $reps reps) =="
    for i in $(seq 1 "$pairs"); do
        ra=$("$BUILD/micro_base" "$w" "$reps")
        rb=$("$BUILD/micro_cand" "$w" "$reps")
        echo "pair $i: base=$ra cand=$rb"
        if awk "BEGIN{exit !($rb < $ra)}"; then bwins=$((bwins + 1)); else awins=$((awins + 1)); fi
        asum=$(awk "BEGIN{print $asum + $ra}")
        bsum=$(awk "BEGIN{print $bsum + $rb}")
    done
    awk "BEGIN{printf \"baseline wins: %d  candidate wins: %d  |  means: base=%.1f cand=%.1f (%+.1f%%)\n\", \
        $awins, $bwins, $asum/$pairs, $bsum/$pairs, ($bsum-$asum)/$asum*100}"
    echo "a change is real only if the candidate wins (almost) every pair"
}

case "${1:-}" in
build)
    build "${2:-HEAD}"
    ;;
run)
    [[ $# -ge 2 ]] || { echo "usage: $0 run <workload> [pairs] [reps]" >&2; exit 1; }
    run_one "$2" "${3:-6}" "${4:-3}"
    ;;
all)
    for w in ie64 iestr find64 findstr it64 itstr; do
        run_one "$w" "${2:-6}" "${3:-3}"
    done
    ;;
*)
    sed -n '3,20p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
    ;;
esac
