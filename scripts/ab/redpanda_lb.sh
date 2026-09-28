#!/usr/bin/env bash
# redpanda_lb.cpp against several revisions of the header, both compilers, binaries alternated,
# medians over the alternations (#317). Needs an installed abseil (ABSL_ROOT, default
# /home/martinus/gra/abseil-install). Pinned with AB_CORE (default 2).
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/redpanda_lb.sh [ALTERNATIONS [ROUNDS]] -- REV...
#
# AB_DEFINES is passed to the compiler, e.g. AB_DEFINES=-DUNIQUE_GROUPS for one map entry per group.
#
# Prints per compiler, container and header the median of the per-alternation medians, in ms.
set -euo pipefail
alts=${1:-4}
rounds=${2:-30}
shift 2 || true
[[ "${1:-}" == "--" ]] && shift
revs=("$@")
[ ${#revs[@]} -gt 0 ] || revs=(v4.5.0 origin/main)
root=$(git rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
absl=${ABSL_ROOT:-/home/martinus/gra/abseil-install}
core=${AB_CORE:-2}
for rev in "${revs[@]}"; do
    d="$build/hdr-${rev//\//_}/ankerl"
    mkdir -p "$d"
    for f in unordered_dense.h stl.h; do
        git -C "$root" show "$rev:include/ankerl/$f" >"$d/$f" 2>/dev/null || true
    done
done
for cxx in clang++ g++; do
    for rev in "${revs[@]}"; do
        # shellcheck disable=SC2086
        "$cxx" -O3 -DNDEBUG -std=c++17 ${AB_DEFINES:-} -I"$build/hdr-${rev//\//_}" -I"$absl/include" \
            "$root/scripts/ab/redpanda_lb.cpp" -Wl,--start-group "$absl"/lib64/libabsl_*.a -Wl,--end-group \
            -o "$build/$cxx-${rev//\//_}" &
    done
done
wait
raw="$build/raw.txt"
: >"$raw"
for ((a = 0; a < alts; ++a)); do
    for cxx in clang++ g++; do
        for ((i = 0; i < ${#revs[@]}; ++i)); do
            rev=${revs[$(((i + a) % ${#revs[@]}))]}
            taskset -c "$core" "$build/$cxx-${rev//\//_}" "$rounds" | sed "s|^|$cxx ${rev} |" >>"$raw"
        done
    done
done
python3 - "$raw" <<'PY'
import sys, statistics, collections
d = collections.defaultdict(list)
for line in open(sys.argv[1]):
    cxx, rev, ver, name, med, mn = line.split()
    d[(cxx, name, rev)].append(float(med))
revs = list(dict.fromkeys(k[2] for k in d))
for cxx in ("clang++", "g++"):
    print(f"{cxx}: median ms, " + " / ".join(revs))
    for name in dict.fromkeys(k[1] for k in d):
        print(f"  {name:28}" + " ".join("%7.3f" % statistics.median(d[(cxx, name, r)]) for r in revs))
PY
