#!/bin/bash
# Is a value address that the hash predicts worth prefetching? (issue #229)
#
#   scripts/ab/value_prefetch.sh [-c COMPILER] [-k u64|str|big] [-p "12 14 16"] [-r REPS]
#
# This map pays two dependent memory accesses on a hit -- the group block, then the value -- where a
# flat map pays one. The value's address cannot be predicted, because the value vector is ordered by
# insertion and the map does not choose where a value goes. A container that owned placement could
# predict it, and this asks whether predicting it is worth anything, by making the prediction true
# inside the shipped map: keys are inserted in home-group order, twelve per group, so the values of
# group g are twelve contiguous entries at `values + g * 12`.
#
# Four variants, and the comparison that answers the question is C against B:
#
#   A  random fill,  no prefetch   the shipped map, as it behaves today
#   B  grouped fill, no prefetch   locality alone -- what the container gets for free
#   C  grouped fill, prefetch      the value prefetch, 1/2/3 lines
#   D  random fill,  prefetch      a prefetch of the wrong line: the tax on every miss
#
# The header is patched in a copy, exactly the way probe_length.sh does it; the working tree is
# never touched. It refuses to run if probe() no longer looks the way the patch expects.
set -euo pipefail
export LC_ALL=C
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
cxx=clang++ keys=u64 points="14 16 18 20" reps=30000000
while getopts "c:k:p:r:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        k) keys=$OPTARG ;;
        p) points=$OPTARG ;;
        r) reps=$OPTARG ;;
        *) exit 1 ;;
    esac
done

# the probe, with a prefetch of the value line the group implies
python3 - "$root/include/ankerl/unordered_dense.h" "$build/vp.h" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
old = """        auto const word = fingerprint_word(mh);
        auto const counter = word & 7U;
        auto const home_idx = group_idx_from_hash(mh);
        if constexpr (!detail::key_compare_is_call_v<Key>) {"""
new = """        auto const word = fingerprint_word(mh);
        auto const counter = word & 7U;
        auto const home_idx = group_idx_from_hash(mh);
        udm_prefetch_values(m_values.data() + std::size_t{home_idx} * UDM_VP_PER_GROUP);
        if constexpr (!detail::key_compare_is_call_v<Key>) {"""
if old not in s:
    sys.exit("probe() no longer looks the way this patch expects; update scripts/ab/value_prefetch.sh")
s = s.replace(old, new, 1)
s = s.replace("namespace ankerl::unordered_dense {", """#ifndef UDM_VP_PER_GROUP
#    define UDM_VP_PER_GROUP 12
#endif
#ifndef UDM_VP_LINES
#    define UDM_VP_LINES 0
#endif
// With UDM_VP_LINES 0 the loop is empty and the address arithmetic is dead, so the baseline is
// built from this same patched header and the two binaries differ only in the prefetch.
template <typename P>
inline void udm_prefetch_values(P const* p) {
    auto const* c = reinterpret_cast<char const*>(p);
    for (int i = 0; i < UDM_VP_LINES; ++i) {
        ANKERL_UNORDERED_DENSE_PREFETCH(c + 64 * i);
    }
}
namespace ankerl::unordered_dense {""", 1)
open(dst, "w").write(s)
PY
mkdir -p "$build/vpinc/ankerl"
cp "$build/vp.h" "$build/vpinc/ankerl/unordered_dense.h"
[ -f "$root/include/ankerl/stl.h" ] && cp "$root/include/ankerl/stl.h" "$build/vpinc/ankerl/stl.h"

kf=""
[ "$keys" = str ] && kf="-DUDM_VP_STR"
[ "$keys" = big ] && kf="-DUDM_VP_BIG"
flags=(-O3 -DNDEBUG -std=c++17 -w -I"$build/vpinc" -I"$root/include" -I"$root/test" ${kf:+"$kf"})

# Per lookup, as the *slope* of two rep counts rather than the total over one.
#
# perf counts the whole process, and building a table of twelve million entries -- plus generating
# its keys by rejection sampling -- is not free. At p=20 that setup is most of the run: measured as
# a total, a miss came out at 300 instructions, which is impossible for a probe that stops in its
# home group, and the random and grouped fills differed on a workload that never reads a value.
# Subtracting a quarter-length run removes every fixed cost exactly, whatever it was.
measure() { # binary work order p reps -> "task-clock cycles instructions L1 dTLB"
    local out
    out=$(perf stat -x, -e task-clock,cycles,instructions,L1-dcache-load-misses,dTLB-load-misses \
              ${AB_CORE:+taskset -c "$AB_CORE"} "$1" "$2" "$3" "$4" "$5" 2>&1)
    echo "$out" | awk -F, '$3=="task-clock"{a=$1} $3=="cycles"{b=$1} $3=="instructions"{c=$1}
                           $3=="L1-dcache-load-misses"{d=$1} $3=="dTLB-load-misses"{e=$1}
                           END{print a, b, c, d, e}'
}

run() { # binary work order p -> ns, instructions, cycles, L1, dTLB per lookup
    local lo=$((reps / 4))
    local full short
    full=$(measure "$1" "$2" "$3" "$4" "$reps")
    short=$(measure "$1" "$2" "$3" "$4" "$lo")
    awk -v d=$((reps - lo)) -v f="$full" -v s="$short" \
        'BEGIN { split(f, F, " "); split(s, S, " ");
                 printf "%9.2f %9.1f %9.1f %8.3f %8.3f",
                        (F[1]-S[1])*1e6/d, (F[3]-S[3])/d, (F[2]-S[2])/d, (F[4]-S[4])/d, (F[5]-S[5])/d }'
}

for lines in 0 1 2 3; do
    "$cxx" "${flags[@]}" -DUDM_VP_LINES=$lines "$root/scripts/ab/value_prefetch.cpp" -o "$build/vp_$lines"
done

echo "keys=$keys  $cxx  reps=$reps  (ns, instructions, cycles, L1-miss, dTLB-miss per lookup)"
printf "%-4s %-6s %-9s %-8s %9s %9s %9s %8s %8s\n" p work variant lines ns instr cycles L1 dTLB
for p in $points; do
    for work in hit miss; do
        for spec in "A random 0" "B grouped 0" "C grouped 1" "C grouped 2" "C grouped 3" "D random 1"; do
            set -- $spec
            printf "%-4s %-6s %-9s %-8s %s\n" "$p" "$work" "$1 $2" "$3" "$(run "$build/vp_$3" "$work" "$2" "$p")"
        done
    done
done
