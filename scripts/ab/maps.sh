#!/bin/bash
# Build and run scripts/ab/maps.cpp: every index structure in one binary, on the same workloads.
#
#   scripts/ab/maps.sh [-c COMPILER] [-s] [-r REV] <check|speed|memory|names> [u64|str|big] [base]
#
#   -c COMPILER  default clang++
#   -s           build with -fsanitize=address,undefined (for `check`)
#   -r REV       the revision the second unordered_dense is taken from (default v4.11.0)
#
# Every map that is found is compiled in; one that is not is left out and said so. Point these at
# your checkouts if they are elsewhere:
#
#   NANOBENCH_INCLUDE  directory holding third-party/nanobench.h, >= martinus/nanobench#189
#                      (targetIntervalWidth; the vendored 4.6.0 does not have it)
#   ABSL_ROOT          an installed abseil: $ABSL_ROOT/include and $ABSL_ROOT/lib64/libabsl_*.a
#   FOLLY_ROOT         a folly checkout; FOLLY_CONFIG a directory holding folly/folly-config.h
#   EMHASH_INCLUDE     holding emhash/hash_table8.hpp and emilib/emihmap1.hpp
#   INDIVI_INCLUDE     holding indivi/flat_umap.h
#   VERSTABLE_INCLUDE  holding verstable.h
#   IHTAB_INCLUDE      holding ihtab.hpp
#
# Env knobs of the binary itself: UDM_POINTS (sizes per octave, default 5), UDM_INTERVAL (the
# confidence interval compare() measures until, default 0.03), UDM_EPOCHS (its ceiling, default
# 200), UDM_CSV (append every octave geomean to this file).
#
# folly's F14 needs C++20, so everything here is built as C++20. That is the harness's dialect and
# not the library's: unordered_dense itself is C++17.
set -euo pipefail
cxx=clang++ san=0 rev=v4.11.0
while getopts "c:sr:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        s) san=1 ;;
        r) rev=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { sed -n '2,27p' "$0"; exit 1; }
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"

nb=${NANOBENCH_INCLUDE:-$root/test}
if ! grep -q targetIntervalWidth "$nb/third-party/nanobench.h" 2>/dev/null; then
    echo "error: $nb/third-party/nanobench.h has no targetIntervalWidth(); point NANOBENCH_INCLUDE at a checkout of martinus/nanobench#189" >&2
    exit 1
fi

# The second unordered_dense, in its own namespace and macro prefix, beside the current one.
git -C "$root" show "$rev:include/ankerl/unordered_dense.h" \
    | sed 's/ankerl::unordered_dense/udmbase::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMBASE_UNORDERED_DENSE/g; s/namespace ankerl/namespace udmbase/g; s|#        include "stl.h"|#        include <ankerl/stl.h>|' \
    > "$build/base411.h"

flags=(-O3 -DNDEBUG -std=c++20 -w -I"$build" -I"$nb" -I"$root/include" -I"$root/test")
libs=()
srcs=()
have=()
missing=()

try() { # name, test-include, flags...
    local name=$1 header=$2
    shift 2
    if printf '#include <%s>\nint main() {}\n' "$header" | "$cxx" -x c++ -std=c++20 -fsyntax-only "$@" - 2>/dev/null; then
        flags+=("$@")
        have+=("$name")
        return 0
    fi
    missing+=("$name")
    return 1
}

absl=${ABSL_ROOT:-/home/martinus/gra/abseil-install}
if try absl absl/container/flat_hash_map.h -I"$absl/include"; then
    libs+=(-Wl,--start-group "$absl"/lib64/libabsl_*.a -Wl,--end-group)
fi
folly=${FOLLY_ROOT:-/home/martinus/gra/folly}
follycfg=${FOLLY_CONFIG:-/home/martinus/gra/folly-config}
if try f14 folly/container/F14Map.h -I"$folly" -I"$follycfg"; then
    # F14's out-of-line half, plus the two files it needs to link.
    srcs+=("$folly/folly/container/detail/F14Table.cpp" "$folly/folly/lang/SafeAssert.cpp"
           "$folly/folly/lang/ToAscii.cpp")
fi
try emhash emhash/hash_table8.hpp -I"${EMHASH_INCLUDE:-/home/martinus/gra/emhash/include}" || true
try indivi indivi/flat_umap.h -I"${INDIVI_INCLUDE:-/home/martinus/gra/indivi_collection/calmsand/src}" || true
# verstable.h is a macro template: it cannot be included without NAME/KEY_TY set, so `try` would
# always say no. Its presence is the test.
vst=${VERSTABLE_INCLUDE:-/home/martinus/gra/Verstable/softwave}
if [ -f "$vst/verstable.h" ]; then
    flags+=(-I"$vst")
    have+=(verstable)
else
    missing+=(verstable)
fi
try ihtab ihtab.hpp -I"${IHTAB_INCLUDE:-/home/martinus/gra/ihtab/sololynx}" || true

# abseil turns on its generation-counter iterator debugging when a sanitizer is present, and the
# installed library half was not built that way -- the mismatch is a null read inside find_small
# before any of this harness runs. The sanitized run is about these adapters, so it leaves abseil
# out rather than pretending to have found something.
if [ $san = 1 ]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -DUDM_NO_ABSL)
    libs=()
fi

nbo="$build/nanobench_$(basename "$cxx").o"
[ -f "$nbo" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" -O2 -DNDEBUG -std=c++20 -w -I"$nb" -c nb.cpp -o "$nbo")

echo "maps: udm udm-4.11($rev) boost std ${have[*]:-} | missing: ${missing[*]:-none} | $cxx, $build" >&2
"$cxx" "${flags[@]}" "$root/scripts/ab/maps.cpp" "$nbo" "${srcs[@]}" "${libs[@]}" -o "$build/maps"
exec "$build/maps" "$@"
