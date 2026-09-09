#!/bin/bash
# Latency and throughput of every string hash in the map comparison, in one process.
#
#   scripts/ab/hash_others.sh [-c compiler] [-r baseline-revision] [table|sweep] [interval-width]
#
# `table` is the six lengths and the scored mix that the blog post quotes; `sweep` walks 4 to 1024
# bytes for the latency chart.
#
# Optional dependencies are found the way maps.sh finds them, and anything missing is left out of
# the table rather than failing the run: ABSL_ROOT, FOLLY_ROOT and FOLLY_CONFIG, boost from the
# system. The baseline revision supplies unordered_dense's older hash under its own namespace.
set -euo pipefail
export LC_ALL=C
cxx=clang++ rev=v4.11.0 runs=1
while getopts "c:r:n:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        r) rev=$OPTARG ;;
        n) runs=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
mode=${1:-table}
width=${2:-0.02}
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
nb=${NANOBENCH_INCLUDE:-$root/test}

git -C "$root" show "$rev:include/ankerl/unordered_dense.h" \
    | sed 's/ankerl::unordered_dense/udmbase::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMBASE_UNORDERED_DENSE/g; s/namespace ankerl/namespace udmbase/g; s|#        include "stl.h"|#        include <ankerl/stl.h>|' \
    > "$build/base.h"

flags=(-O3 -DNDEBUG -std=c++20 -w -I"$build" -I"$nb" -I"$root/include" -I"$root/test" -DUDM_AB_HAVE_BASE)
libs=() srcs=() have=(udm5 "udm4($rev)") missing=()
try() { # name, define, test-include, flags...
    local name=$1 def=$2 header=$3
    shift 3
    if printf '#include <%s>\nint main() {}\n' "$header" | "$cxx" -x c++ -std=c++20 -fsyntax-only "$@" - 2>/dev/null; then
        flags+=("$@" "-D$def")
        have+=("$name")
        return 0
    fi
    missing+=("$name")
    return 1
}
try boost UDM_AB_HAVE_BOOST boost/container_hash/hash.hpp || true
absl=${ABSL_ROOT:-/home/martinus/gra/abseil-install}
if try absl UDM_AB_HAVE_ABSL absl/hash/hash.h -I"$absl/include"; then
    libs+=(-Wl,--start-group "$absl"/lib64/libabsl_*.a -Wl,--end-group)
fi
folly=${FOLLY_ROOT:-/home/martinus/gra/folly}
follycfg=${FOLLY_CONFIG:-/home/martinus/gra/folly-config}
# SpookyHashV2, which is what folly::hasher<std::string> is, lives in a .cpp.
try folly UDM_AB_HAVE_FOLLY folly/hash/Hash.h -I"$folly" -I"$follycfg" &&
    srcs+=("$folly/folly/hash/SpookyHashV2.cpp")

nbo="$build/nb_$(basename "$cxx").o"
[ -f "$nbo" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" -O2 -DNDEBUG -std=c++20 -w -I"$nb" -c nb.cpp -o "$nbo")

echo "hashes: ${have[*]} | missing: ${missing[*]:-none} | $cxx" >&2
"$cxx" "${flags[@]}" "$root/scripts/ab/hash_others.cpp" "$nbo" "${srcs[@]}" "${libs[@]}" -o "$build/hash_others"
if [ "$runs" -le 1 ]; then
    exec taskset -c "${AB_CORE:-2}" "$build/hash_others" "$mode" "$width"
fi

# -n runs the whole thing several times in fresh processes and reports the median of each point,
# with the worst spread on stderr. A within-run interval says how well one process resolved its own
# median; it says nothing about what changed between one process and the next, which on a machine
# that is not idle is the larger of the two.
out=$(mktemp -d)
for i in $(seq 1 "$runs"); do
    echo "  run $i of $runs ..." >&2
    taskset -c "${AB_CORE:-2}" "$build/hash_others" "$mode" "$width" > "$out/r$i.csv"
done
python3 - "$out"/r*.csv <<'PY'
import csv, statistics, sys
from collections import defaultdict
runs = [list(csv.DictReader(open(f))) for f in sys.argv[1:]]
label = list(runs[0][0].keys())[0]
vals = defaultdict(lambda: defaultdict(list))
for r in runs:
    for row in r:
        for col in ("throughput_ns", "latency_ns"):
            vals[(row[label], row["hash"])][col].append(float(row[col]))
w = csv.writer(sys.stdout, lineterminator="\n")
w.writerow([label, "hash", "throughput_ns", "latency_ns"])
worst = (0.0, "")
for key, cols in vals.items():
    row = [key[0], key[1]]
    for col in ("throughput_ns", "latency_ns"):
        xs = cols[col]
        row.append(f"{statistics.median(xs):.3f}")
        spread = (max(xs) - min(xs)) / statistics.median(xs)
        if col == "latency_ns" and spread > worst[0]:
            worst = (spread, f"{key[1]} at {key[0]}")
    w.writerow(row)
print(f"worst spread between runs: {worst[0] * 100:.1f}% ({worst[1]})", file=sys.stderr)
PY
