#!/bin/bash
# The default maximum load factor, swept (#306): each value becomes the candidate header's
# `default_max_load_factor` in turn and runs against REV's (default origin/main, 0.8) with
# `run.sh -p 50 all 12`, pinned to AB_CORE (default 2). The first value is REV's own and is the
# same-header control. Fifty points because each maximum doubles the table at different sizes, so
# the two sides' sawtooths are out of phase and five points would read wherever they land.
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/load_factor.sh [-r REV] COMPILER [values...] | tee out.txt
#
# It edits include/ankerl/unordered_dense.h between runs and restores it on exit, so do not touch
# the tree while it runs. About 5.5 minutes per value; the default five values are 28 minutes.
set -euo pipefail
rev=origin/main
if [[ "${1:-}" == "-r" ]]; then rev=$2; shift 2; fi
cxx=${1:?compiler}
shift
values=("$@")
[ ${#values[@]} -gt 0 ] || values=(0.8 0.75 0.85 0.875 0.9)
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
hdr=$root/include/ankerl/unordered_dense.h
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
git -C "$root" diff --quiet -- include/ankerl/unordered_dense.h || { echo "header has local changes" >&2; exit 1; }
trap 'git -C "$root" checkout -- include/ankerl/unordered_dense.h' EXIT
for lf in "${values[@]}"; do
    git -C "$root" checkout -- include/ankerl/unordered_dense.h
    sed -i "s/default_max_load_factor = 0.8F;/default_max_load_factor = ${lf}F;/" "$hdr"
    grep -q "default_max_load_factor = ${lf}F;" "$hdr"
    echo "=== $cxx lf=$lf $(date +%T)"
    AB_BUILD=$build taskset -c "${AB_CORE:-2}" "$root/scripts/ab/run.sh" -r "$rev" -c "$cxx" -p 50 all 12 2>&1
done
