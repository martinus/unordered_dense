#!/bin/bash

# runs 'meson test' for all build subdirectories
set -e
cd `git rev-parse --show-toplevel`

./scripts/lint/lint-all.py

c=0
for D in builddir/*/ ; do
    echo
    echo "-- building $D --"
    meson test -q --print-errorlogs -C $D
    ((c = c + 1))
done
echo
echo "${c} builds ok"
