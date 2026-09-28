# Valhalla's CostMatrix with three containers (#318)

Re-runs [valhalla#4552](https://github.com/valhalla/valhalla/pull/4552#issuecomment-1926857554): the
map Valhalla ships (v4.5.0), this repository's `main`, and `absl::flat_hash_map`/`flat_hash_set` in
`src/thor/costmatrix.cc`. Everything builds and runs in a rootless podman container, because the host
lacks Valhalla's dependencies. Paths are hard-coded to `/home/martinus/gra/valbench`, mounted as `/vb`.

1. `valbuild`: an Ubuntu 24.04 image with Valhalla's `scripts/install-linux-deps.sh` applied;
   `Containerfile` adds `libabsl-dev` on top (`localhost/valbuild-absl`).
2. Three source copies of Valhalla master without `.git`: `src-v450` as is, `src-main` with this
   repository's `unordered_dense.h` and `stl.h` in `third_party/unordered_dense/include/ankerl/`, and
   `src-absl` with `absl.patch` applied. `build.sh <variant>` builds `valhalla_service` and
   `valhalla_build_tiles`.
3. Germany from Geofabrik, tiles built once with `build-v450/valhalla_build_tiles`; the config from
   `valhalla_build_config` with `--service-limits-auto-max-matrix-distance 2000000` (Mannheim-Usedom
   is about 800 km, the default cap 400 km).
4. `gen.py` writes the request files; `validate.py` (against a running service) replaces the
   Mannheim-Usedom file with points that route to Kassel, since the box reaches outside Germany and one
   unconnected point fails a whole request (error 170).
5. `run.sh ROUNDS`: per round and variant, a fresh `valhalla_service` with one worker pinned to cores
   2-3, one warm-up request, then each file's 20 requests timed with `client.py`. `FILES=...` limits
   the files.
