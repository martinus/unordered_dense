#!/usr/bin/env bash
# Build one variant's valhalla_service and valhalla_build_tiles inside the container.
set -euo pipefail
v=$1
podman run --rm -v /home/martinus/gra/valbench:/vb:Z localhost/valbuild-absl:latest bash -c "
  set -e
  cmake -S /vb/src-$v -B /vb/build-$v -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF -DENABLE_PYTHON_BINDINGS=OFF \
        -DENABLE_CCACHE=OFF -DENABLE_SINGLE_FILES_WERROR=OFF >/vb/cfg-$v.txt 2>&1
  cmake --build /vb/build-$v -j 32 --target valhalla_service valhalla_build_tiles >/vb/build-$v.txt 2>&1
" && echo "$v built" || { echo "$v FAILED"; tail -20 /home/martinus/gra/valbench/cfg-$v.txt /home/martinus/gra/valbench/build-$v.txt 2>/dev/null | grep -iE 'error|fail' | head; }
