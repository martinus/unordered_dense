#!/bin/bash
# Build scripts/ab/window.cpp twice -- aligned groups against a sliding window -- and run one
# workload on each. One variant per binary, because this is exactly the size of effect a shared
# translation unit misreports.
#
#   scripts/ab/window.sh [-k u64|str] <check|build|hit|miss|churn|memory> <entries> <reps>
set -euo pipefail
keys=u64
while getopts "k:" opt; do case $opt in k) keys=$OPTARG ;; *) exit 1 ;; esac; done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { sed -n '2,8p' "$0"; exit 1; }
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
nbo="$build/nb.o"
[ -f "$nbo" ] || { printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > "$build/nb.cpp"
    clang++ -O2 -DNDEBUG -std=c++20 -w -I"$root/test" -c "$build/nb.cpp" -o "$nbo"; }
str=""
[ "$keys" = str ] && str="-DWINDOW_STR"
for v in 0 1; do
    bin="$build/window_${keys}_$v"
    [ -f "$bin" ] || clang++ -O3 -DNDEBUG -std=c++20 -w -DVARIANT=$v $str \
        -I"$root/include" -I"$root/test" -I"$root/test/third-party" \
        "$root/scripts/ab/window.cpp" "$nbo" -o "$bin"
    printf '%-8s ' "$([ $v = 0 ] && echo grouped || echo window)"
    "$bin" "$@"
done
