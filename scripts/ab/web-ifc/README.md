# web-ifc's IFC loader with std::unordered_map, 4.8.1, 5.1.0 and 5.3.1 (#320)

Re-runs [engine_web-ifc#1952](https://github.com/ThatOpen/engine_web-ifc/pull/1952), which moved the
IFC loader's caches (`IfcLoader::_lines`, express ID to line, and the maps in `IfcCache`) from
`std::unordered_map` to this map. web-ifc's code is the same from 4.8.1 to 5.0.1 (its commit
360a660a changes only the pinned tag), so one source tree builds against every header.

`bench_load.cpp` parses a file with `IfcLoader::LoadFile`, then builds an `IfcGeometryProcessor`
(which builds the `IfcCache`) and calls `GetFlatMesh` on every element and `GetGeometry` on every
geometry, skipping openings and spaces. It prints the two phases' seconds. The file is read into
memory before the clock starts.

1. `gra clone --no-tmux --no-upstream https://github.com/ThatOpen/engine_web-ifc`; `WEBIFC` is its
   `src/cpp` (default `/home/martinus/gra/engine_web-ifc/safepear/src/cpp`).
2. Models, in `$WIFC/models/` (default `WIFC=/home/martinus/gra/wifc320`): unzip
   `tests/ifcfiles/public/ISSUE_053_20181220Holter_Tower_10.ifczip` (177 MB, the "Hotel" of #1952)
   and `LTU_A-House_redesign.ifczip` (181 MB), and copy `ISSUE_098_*.ifc` (73 MB) and
   `ISSUE_068_*.ifc` (57 MB). #1952's "Resort" (940 MB) is not public.
3. `build.sh`: the three headers from this repository's tags with `git archive`, a copy of web-ifc's
   source with `ankerl::unordered_dense::map` replaced by `std::unordered_map`, and eight binaries
   (`b-{clang,gcc}-{std,v4.8.1,v5.1.0,v5.3.1}`). Check each binary's version namespace with `nm -C`.
4. `ROUNDS=11 run.sh | tee run.txt`: every binary on every model once per round, order rotated, on
   core 2, with peak RSS from `/usr/bin/time`.
5. `summarize.py run.txt`: medians (min-max) per model and phase, ratios to std and to 4.8.1 of the
   same compiler.
