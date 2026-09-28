#!/usr/bin/env bash
# Berlin prepared once, with the std build, for both CH (contract) and MLD (partition, customize).
set -euo pipefail
podman run --rm -v /home/martinus/gra/osrmbench:/ob:Z localhost/osrmbuild:latest bash -c "
  set -e
  mkdir -p /ob/data && cp /ob/berlin-latest.osm.pbf /ob/data/berlin.osm.pbf
  B=/ob/build-std
  \$B/osrm-extract -p /ob/src-std/profiles/car.lua /ob/data/berlin.osm.pbf
  \$B/osrm-partition /ob/data/berlin.osrm
  \$B/osrm-customize /ob/data/berlin.osrm
  \$B/osrm-contract /ob/data/berlin.osrm
" >/home/martinus/gra/osrmbench/prepare.log 2>&1 && echo prepared || { echo "prepare FAILED"; tail -5 /home/martinus/gra/osrmbench/prepare.log; }
