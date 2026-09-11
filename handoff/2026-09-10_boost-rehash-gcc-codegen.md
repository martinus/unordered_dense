# Session Handoff — 2026-09-10 — gcc is 1.5x slower than clang on boost's rehash loop

## Resume prompt

Paste this into a fresh session:

> Read `handoff/2026-09-10_boost-rehash-gcc-codegen.md` in the unordered_dense worktree
> `/home/martinus/gra/unordered_dense/dawncorn`. A measurement from 2026-09-08 found that
> `boost::unordered_flat_map`'s rehash loop is about 1.5x slower when built with gcc than with
> clang, on identical source, and that merely restructuring the loop takes gcc to clang's floor.
> That points at a compiler serialising something, not at a design problem, and it has never been
> confirmed or reported. Reproduce it, find the instruction that causes it, and decide whether it is
> a gcc bug worth filing or a source change worth sending to Boost.Unordered. Do not change
> `include/ankerl/unordered_dense.h` — this is about boost.

## The finding

Measured 2026-09-08 while asking whether this library's pipelined rehash would transfer to boost.
The port itself failed, which is not what this handoff is about. What it turned up on the way is
the table below: **isolated `rehash()`, doubling the bucket count and back, nanoseconds per element**.

| | u64 200K | u64 1M | str 200K | str 1M |
|---|---|---|---|---|
| clang, boost as shipped | 6.5 | 10.6 | 19.0–19.5 | 77.4 |
| clang, pipelined port | 7.0–7.2 | 11.1–11.3 | 17.9–18.7 | 80–82 |
| **gcc, boost as shipped** | **10.2–10.5** | 10.4–11.5 | 17.1–17.8 | 71–73 |
| **gcc, pipelined port** | **7.0** | 10.2–10.5 | 16.7–17.2 | 73–77 |

Read the first column. Same source, same machine, same boost: clang 6.5, gcc 10.2–10.5. Restructure
the loop and gcc goes to 7.0 while clang gets slightly worse. The effect is specific — it does not
show at 1M, and it does not show for string keys — which is itself a clue: at 200K the index is
still in cache, so a serialised dependency is exposed, and at 1M memory latency hides it.

**This is the same shape as a bug already found and fixed in this library**, with the compilers
swapped. `fill_buckets_from_values` used to read `m_values[value_idx]`; placing an entry stores a
`std::uint8_t` fingerprint, a byte store may alias anything, so after every placement clang had to
reload the container's data pointer before it could form the address of the next key — one memory
latency per element, on the address chain of a random access. Walking with an iterator instead took
that build from 16.72 ms to 8.96. See `notes/index-design.md`, "Indexing the value container in the
rehash cost clang a memory latency per element". **Look for that pattern first.** Boost's
`unchecked_rehash` writes a metadata byte and then computes the next element's address.

## What to do

1. **Reproduce it before anything else.** The numbers are from one afternoon and were never re-run.
   If gcc and clang now agree, stop and record that in `notes/index-design.md`; boost and both
   compilers have moved since.
2. **Find the instruction.** `perf stat` first (instructions, cycles, IPC — is gcc retiring more
   work or stalling on the same work?), then `objdump -d --no-show-raw-insn` on
   `boost::unordered::detail::foa::table_core<...>::unchecked_rehash` from both compilers and diff
   the inner loop. The question to answer is narrow: **what is on the address chain in gcc's loop
   that is not on clang's?**
3. **Then decide which of two things it is.** A gcc bug (report with a reduced test case), or a
   source pattern boost could change (report to Boost.Unordered with the measurement). The
   restructured loop already reaching clang's floor suggests the second is available even if the
   first is true.

## How to build and run it

Boost is the system install, **1.90.0** (`/usr/include/boost/version.hpp`). The harnesses are
beside this file, recovered from `/tmp` before it cleared:

- `2026-09-10_boost-rehash/brh.cpp` — the timing harness that produced the table. Builds a map,
  then calls `rehash()` alternately to double the bucket count and back, timing each; reports the
  best ns per element. Takes the key type, size and repetitions.
- `2026-09-10_boost-rehash/rhperf.cpp` — nothing but rehashes, so `perf stat` counts the loop and
  little else. This is what produced "boost 97.5 instructions and 110.9 cycles per element against
  this map's 51.9 and 46.7".
- `2026-09-10_boost-rehash/bsplit2.cpp` — splits a build into reserved inserts and growth. Not
  needed for the codegen question; included because it is the context (68–75% of a boost build is
  growth, against 11–28% here).

All three include `<bench/workloads.h>` from this repo, so build them with `-I include -I test`:

```sh
cd /home/martinus/gra/unordered_dense/dawncorn
for cxx in clang++ g++; do
    $cxx -O2 -DNDEBUG -std=c++17 -w -Iinclude -Itest \
        handoff/2026-09-10_boost-rehash/brh.cpp -o /tmp/brh_$cxx
done
taskset -c 2 /tmp/brh_clang++ ; taskset -c 2 /tmp/brh_g++
```

**The patched boost is gone.** `/tmp/boostpf` was a copy of `boost/unordered/detail/foa/core.hpp`
with `unchecked_rehash` rewritten as a sixteen-entry ring of (element pointer, hash) that prefetches
the destination group, and in a second variant all four cache lines of the group's fifteen slots.
It has to be rebuilt if you want the "pipelined" rows. For the codegen question you do not need it:
the shipped rows are the ones that disagree.

## Rules for this measurement

From `CLAUDE.md`; these apply here and two of them have burned this exact question before.

- Never compare runs taken at different times. Alternate the two binaries in one sitting.
- A ratio at one size is a point on a sawtooth. This effect is size-specific — it shows at 200K and
  not at 1M — so **sweep the size** rather than quoting 200K, and say where it starts and stops.
- Instruction counts are the number neither code layout nor drift can move. If gcc and clang retire
  the same instructions and differ in cycles, it is a dependency chain; if gcc retires more, it is
  doing more work. Answer that before reading any disassembly.
- `-march=native` silently upgrades boost's SSE2 intrinsics to AVX-512 on this machine, so a profile
  taken that way is not the code most callers run. Use the default `-march`.

## Context you may want, and where it is

`notes/index-design.md` is the lab notebook; search it by phrase.

- "The pipelined rehash does not transfer to `boost::unordered_flat_map`" — the whole experiment,
  including why the port failed (boost's rehash is instruction-bound from moving `value_type`s, not
  latency-bound: 97.5 instructions and 110.9 cycles per element against this map's 51.9 and 46.7, on
  the *same* L1 misses and with *lower* dTLB misses).
- "And the radix partition does not rescue it either" — the other idea, also measured, also a loss.
- "What that says about boost's build, which is its weakest column" — the reserved/growth split.
- "Indexing the value container in the rehash cost clang a memory latency per element" — the bug
  this one probably rhymes with, with the compilers swapped.

## What success looks like

Either a reduced test case in a gcc bug report, or a patch and a measurement for
Boost.Unordered — plus an entry in `notes/index-design.md` saying what it was. A negative result
("it no longer reproduces on boost 1.90 with gcc 16") is a perfectly good outcome and should be
recorded in the same place, because the claim is currently sitting in the notes unqualified.
