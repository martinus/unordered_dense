#!/bin/bash
# Builds the files attached to a GitHub release, from the working tree.
#
#   scripts/release_assets.sh <version> [outdir]      # e.g. 5.0.1
#
# One archive rather than loose headers, because since the `stl.h` split a copy of
# `unordered_dense.h` on its own does not compile, and loose files invite exactly that mistake. The
# archive carries the `ankerl/` directory, so unpacking it onto an include path is the whole
# installation and `#include <ankerl/unordered_dense.h>` then works.
#
# `huge_page_allocator.h` is in there too. The map never references it and it needs <sys/mman.h>,
# but it is a public header and someone who vendors the map should not have to come back for it.
#
# Both .tar.gz and .zip, because this is vendored on every platform, and a SHA256SUMS beside them.
# Called by .github/workflows/release-assets.yml; runnable by hand to check what a release will get.
set -euo pipefail
export LC_ALL=C

version=${1:?usage: release_assets.sh <version> [outdir]}
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
out=${2:-$root/release-assets}

# The tag says one thing and the headers say another often enough that the release workflow checks
# it; this is the same check, so that building the assets by hand cannot mislabel them either.
"$root/scripts/lint/lint-version.py" --expect "$version"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/ankerl" "$out"
cp "$root/include/ankerl/unordered_dense.h" \
   "$root/include/ankerl/stl.h" \
   "$root/include/ankerl/huge_page_allocator.h" "$stage/ankerl/"
cp "$root/LICENSE" "$stage/"

# zip records a timestamp per entry and gzip one per stream, both of which would otherwise be "now".
find "$stage" -exec touch -d '2020-01-01T00:00:00Z' {} +

base="unordered_dense-$version-headers"
rm -f "$out/$base.tar.gz" "$out/$base.zip" "$out/SHA256SUMS"

# Reproducible archives: fixed mtime, owner and sort order, so rebuilding the same tag gives the
# same bytes and a mirror can be checked against the published checksum.
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime='UTC 2020-01-01' \
    --use-compress-program='gzip -n' -cf "$out/$base.tar.gz" -C "$stage" ankerl LICENSE
(cd "$stage" && find ankerl LICENSE -type f | sort | zip -qX "$out/$base.zip" -@)

(cd "$out" && sha256sum "$base.tar.gz" "$base.zip" > SHA256SUMS)

echo "$out:"
ls -l "$out"
cat "$out/SHA256SUMS"
