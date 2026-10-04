#!/bin/bash
# Builds bench_load against web-ifc for each header version and compiler, plus web-ifc with
# std::unordered_map in place of the map (#320). Paths: WIFC (work dir), WEBIFC (web-ifc src/cpp).
set -euo pipefail
WIFC=${WIFC:-/home/martinus/gra/wifc320}
WEBIFC=${WEBIFC:-/home/martinus/gra/engine_web-ifc/safepear/src/cpp}
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(git -C "$HERE" rev-parse --git-common-dir)
mkdir -p "$WIFC"
cd "$WIFC"
for v in v4.8.1 v5.1.0 v5.3.1; do
    [ -d ud-$v ] || { mkdir ud-$v && git -C "$REPO" archive $v | tar -x -C ud-$v; }
done
if [ ! -d src-std ]; then
    cp -r "$WEBIFC" src-std
    grep -rl "ankerl::unordered_dense::map" src-std/web-ifc | xargs sed -i 's/ankerl::unordered_dense::map/std::unordered_map/g'
fi
conf() { # build-dir compiler header-version web-ifc-src
    CXX=$2 cmake -S "$HERE" -B $1 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DFETCHCONTENT_BASE_DIR="$WIFC/deps" \
        -DFETCHCONTENT_SOURCE_DIR_UNORDERED_DENSE="$WIFC/ud-$3" -DWEBIFC_SRC="$4" > $1.cfg.log
    ninja -C $1 bench_load > $1.build.log
}
for c in clang gcc; do
    cxx=clang++
    [ $c = gcc ] && cxx=g++
    for v in v4.8.1 v5.1.0 v5.3.1; do conf b-$c-$v $cxx $v "$WEBIFC"; done
    conf b-$c-std $cxx v5.3.1 "$WIFC/src-std"
done
