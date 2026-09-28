#!/usr/bin/env bash
# Build one OSRM variant (release preset, vcpkg manifest mode, shared binary cache) in the container.
set -euo pipefail
v=$1
VC=/home/martinus/gra/vcpkg/slimsong
podman run --rm -v /home/martinus/gra/osrmbench:/ob:Z -v $VC:/vcpkg:Z -e VCPKG_ROOT=/vcpkg \
    -e VCPKG_DEFAULT_BINARY_CACHE=/ob/vcpkg-cache -e VCPKG_DISABLE_METRICS=1 localhost/osrmbuild:latest bash -c "
  set -e
  [ -x /vcpkg/vcpkg ] || /vcpkg/bootstrap-vcpkg.sh -disableMetrics >/ob/vcpkg-bootstrap.txt 2>&1
  mkdir -p /ob/vcpkg-cache
  cd /ob/src-$v
  export PATH=/vcpkg/downloads/tools/cmake-4.4.3-linux/cmake-4.4.3-linux-x86_64/bin:$PATH
  cmake --version | head -1 >/ob/cmake-version.txt
  cmake --preset release -B /ob/build-$v -DENABLE_CCACHE=OFF >/ob/cfg-$v.txt 2>&1
  cmake --build /ob/build-$v -j 32 --target osrm-routed osrm-extract osrm-partition osrm-customize osrm-contract >/ob/build-$v.txt 2>&1
" && echo "$v built" || { echo "$v FAILED"; tail -30 /home/martinus/gra/osrmbench/cfg-$v.txt /home/martinus/gra/osrmbench/build-$v.txt 2>/dev/null | grep -iE 'error|fail' | head; }
