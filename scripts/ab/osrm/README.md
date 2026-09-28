# OSRM's e2e benchmarks with std::unordered_map, segmented_map and map (#319)

Re-runs [osrm-backend#6922](https://github.com/Project-OSRM/osrm-backend/pull/6922): `UnorderedMapStorage`
in `include/util/query_heap.hpp` as shipped (`std::unordered_map`), as #6922 had it (`segmented_map`,
`segmented_map.patch`), and as `map` (`map.patch`), both with this repository's header copied to
`include/ankerl/`. Builds run in a rootless podman container (`Containerfile`, on top of the Ubuntu
24.04 image from `../valhalla/`) with vcpkg in manifest mode and one shared binary cache; OSRM needs
CMake 3.29 or newer, so `build.sh` uses the CMake vcpkg downloads. Paths are hard-coded to
`/home/martinus/gra/osrmbench` (mounted as `/ob`) and a vcpkg checkout at
`/home/martinus/gra/vcpkg/slimsong`.

1. `build.sh std|seg|map` on three source copies without `.git`.
2. `prepare.sh`: Berlin from Geofabrik, prepared once with the std build for CH and MLD.
3. `run.sh ROUNDS`: per round, variant (order rotated) and algorithm, a fresh `osrm-routed -t 1` on core 2,
   and OSRM's own `scripts/ci/e2e_benchmark.py` (seeded, 50 warm-ups, 1000 requests) on core 3 for each
   method, with `test/data/berlin_gps_traces.csv.gz` as CI used it. The server logs are kept.
4. `summarize.py`: client requests per second and server-side time per method, as ratios to std.
