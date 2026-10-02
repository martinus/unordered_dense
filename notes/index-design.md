# The measurements behind unordered_dense

Every experiment run on `unordered_dense`'s index, hash, insert path and benchmarks, with the numbers that decided it. `CLAUDE.md` holds the rules that came out of them. This file is the evidence behind those rules, so that a rule can be checked and a rejected idea is not proposed a second time.

## How to use this file

- **Search before you propose an optimization.** Grep for the idea (`grep -n "double hashing" notes/index-design.md`) or for a symbol (`grep -n move_home notes/index-design.md`). Most ideas recorded here lost. Several were proposed twice because nobody looked up the first rejection.
- **Start with the section summary.** Each `##` section opens with a few sentences and a **Where it stands** list. That list gives the current answer for the area and names the entry that holds the evidence.
- **One `###` heading is one entry.** The heading is the claim: what was tried and what happened. The line under it is `*date · issues · status · setup*`. The evidence follows: tables, the mechanism, and what the result does and does not say. A bold sentence inside an entry opens a sub-result.
- **Status** is one of:
  - `kept`: shipped or adopted.
  - `rejected`: measured and not adopted.
  - `retracted`: the result did not hold.
  - `superseded`: a later entry replaced it.
  - `open`: not resolved.
  - `method`: a lesson about measuring.
  - `info`: a measurement that decided nothing.
- **A result is true of the header on the day it was measured.** Re-measure anything older than the change you are asking about.
- **Corrections stay where they were made.** `**Correction (date):**` marks a result that did not hold. `**Later (date):**` points from an older entry to the entry that changed its conclusion. Nothing retracted is deleted, because the retraction is often the more useful half.
- **Order:** sections run from how to measure, through the parts of the map, to other maps, real programs and tooling. Within a section, entries run oldest first.

## Adding an entry

1. Put it at the end of the section it belongs to.
2. Heading: one sentence that says what was tried and what happened.
3. Status line: `*YYYY-MM-DD · #issue · status · machine, compilers, scripts*`.
4. Body: tables, then "what this says and does not say".
5. If it changes the conclusion of an older entry, add a `**Later (date):**` line to that entry. Refer to other entries by link or by title, never by "above" or "below".
6. Update the section's **Where it stands** list if the answer changed.
7. Run `scripts/lint/lint-notes.py --fix`. It regenerates the contents list below. As part of `scripts/lint/all.py` it fails on:
   - a stale contents list;
   - an entry without a status line;
   - a merge-conflict marker;
   - a link to a heading that does not exist;
   - a phrase that the code or docs quote from this file and that is no longer in it.

## Contents

<!-- contents -->

- [How to measure: the scored workloads and the harness lessons](#how-to-measure-the-scored-workloads-and-the-harness-lessons)
  - [Where the time goes](#where-the-time-goes) · info
  - [String keys must not all be the same length.](#string-keys-must-not-all-be-the-same-length) · 2026-09-02 · kept
  - [Integer keys must not be small sequential values.](#integer-keys-must-not-be-small-sequential-values) · 2026-09-02 · kept
  - [Inserting has to grow the table somewhere.](#inserting-has-to-grow-the-table-somewhere) · 2026-09-02 · kept
  - [A mapped value of eight bytes hides what a dense map is for.](#a-mapped-value-of-eight-bytes-hides-what-a-dense-map-is-for) · 2026-09-03 · kept
  - [`build` measured the kernel's page fault handler as much as the map, until `tame_allocator()`.](#build-measured-the-kernels-page-fault-handler-as-much-as-the-map-until-tame_allocator) · 2026-09-03 · kept
  - [Nothing measured a table that only churns.](#nothing-measured-a-table-that-only-churns) · 2026-09-03 · kept
  - [Lookup benchmarks must not replay.](#lookup-benchmarks-must-not-replay) · kept
  - [Two regimes the score does not cover, measured 2026-09-06 when asking what a more realistic benchmark would be.](#two-regimes-the-score-does-not-cover-measured-2026-09-06-when-asking-what-a-more-realistic-benchmark-would-be) · 2026-09-06 · info
  - [The size sweep, and the measurement mistake it took three tries to get right](#the-size-sweep-and-the-measurement-mistake-it-took-three-tries-to-get-right) · 2026-09-06 · method
  - [Every workload is measured for both key types](#every-workload-is-measured-for-both-key-types) · 2026-09-06 · kept
  - [The sweep replayed its key sequence, which flattered the branchiest probe by 2.7x](#the-sweep-replayed-its-key-sequence-which-flattered-the-branchiest-probe-by-27x) · 2026-09-06 · method
  - [Two more things the sweep got wrong, found on 2026-09-06 while adding a confidence band to the absolute chart.](#two-more-things-the-sweep-got-wrong-found-on-2026-09-06-while-adding-a-confidence-band-to-the-absolute-chart) · 2026-09-06 · method
  - [Every boost comparison in this file that predates 2026-09-07 was taken at one size, and five of them cross 1.00 when the octave is averaged instead.](#every-boost-comparison-in-this-file-that-predates-2026-09-07-was-taken-at-one-size-and-five-of-them-cross-100-when-the-octave-is-averaged-instead) · 2026-09-07 · method
  - [The paired harness has a systematic bias on `rmissstr`, about 3.6%](#the-paired-harness-has-a-systematic-bias-on-rmissstr-about-36) · 2026-09-11 · method
  - [Fifty points draw the load-factor sawtooth that five average out, and five points are worth 26% of a cross-family ratio](#fifty-points-draw-the-load-factor-sawtooth-that-five-average-out-and-five-points-are-worth-26-of-a-cross-family-ratio) · 2026-09-12 · method
  - [Comparing today's runs against a stored CSV invented a 2-3% regression that does not exist](#comparing-todays-runs-against-a-stored-csv-invented-a-2-3-regression-that-does-not-exist) · 2026-09-14 · method
  - [The lookup harnesses measure throughput, and a million entries is not past the cache here](#the-lookup-harnesses-measure-throughput-and-a-million-entries-is-not-past-the-cache-here) · 2026-09-14 · method
  - [The teardown harness ran its sides in a fixed order, and the side that runs first in a pass reads 2-4% slower on the string first build; the 5-18% first-build gap at 50000 that #309 could not explain does not reproduce with the same two headers (0.4-3.0%), so the harness now rotates its sides](#the-teardown-harness-ran-its-sides-in-a-fixed-order-and-the-side-that-runs-first-in-a-pass-reads-2-4-slower-on-the-string-first-build-the-5-18-first-build-gap-at-50000-that-309-could-not-explain-does-not-reproduce-with-the-same-two-headers-04-30-so-the-harness-now-rotates-its-sides) · 2026-09-28 · method
- [Measuring memory, the README benchmark run, and the charts](#measuring-memory-the-readme-benchmark-run-and-the-charts)
  - [Peak memory across every harness is now the process's resident high-water mark, and one build of the map is enough to measure it](#peak-memory-across-every-harness-is-now-the-processs-resident-high-water-mark-and-one-build-of-the-map-is-enough-to-measure-it) · 2026-09-13 · method
  - [`max_rss::of` was charging every map 128 KB of its own, and the counter beside it could not see `aligned_alloc`](#max_rssof-was-charging-every-map-128-kb-of-its-own-and-the-counter-beside-it-could-not-see-aligned_alloc) · 2026-09-14 · method
  - [Every configuration of 5.2.0, twelve rows in the README run (#349): `pmr` is free, `group_big` costs 16-34% on the integer build, find, churn and memory below 2^32 elements, `segmented_map` builds 1.9x faster at 0.58x the peak memory and iterates strings 3.38x slower with its default 4 KB segment, and huge pages are the largest gain on every integer timing panel](#every-configuration-of-520-twelve-rows-in-the-readme-run-349-pmr-is-free-group_big-costs-16-34-on-the-integer-build-find-churn-and-memory-below-232-elements-segmented_map-builds-19x-faster-at-058x-the-peak-memory-and-iterates-strings-338x-slower-with-its-default-4-kb-segment-and-huge-pages-are-the-largest-gain-on-every-integer-timing-panel) · 2026-10-01/02 · info
  - [5.2.0 against 5.0.0 on the README workloads: level within 1.4% everywhere except the integer build, 2.1% faster, and the integer build + destroy, 3.6% faster; what 5.1 and 5.2 changed is invisible to this harness](#520-against-500-on-the-readme-workloads-level-within-14-everywhere-except-the-integer-build-21-faster-and-the-integer-build--destroy-36-faster-what-51-and-52-changed-is-invisible-to-this-harness) · 2026-10-02 · info
  - [A fifth series, and it is a control](#a-fifth-series-and-it-is-a-control) · 2026-09-06 · kept
  - [The charts are also a page you can interrogate](#the-charts-are-also-a-page-you-can-interrogate) · 2026-09-06 · kept
  - [The four charts worth keeping, and the two axes that had none](#the-four-charts-worth-keeping-and-the-two-axes-that-had-none) · 2026-09-06 · info
  - [A chart for the hash, and its own control says how much of it to believe](#a-chart-for-the-hash-and-its-own-control-says-how-much-of-it-to-believe) · 2026-09-07 · info
  - [An SVG in an `<img>` follows the reader's colour scheme, not the page's](#an-svg-in-an-img-follows-the-readers-colour-scheme-not-the-pages) · 2026-09-08 · kept
  - [A value label never goes inside its bar](#a-value-label-never-goes-inside-its-bar) · 2026-09-08 · kept
  - [`scripts/ab/regen.sh` rebuilds everything in `doc/`](#scriptsabregensh-rebuilds-everything-in-doc) · info
- [The index layout: group size, counters, block layout, load factor, probing](#the-index-layout-group-size-counters-block-layout-load-factor-probing)
  - [Fingerprints and counters in two arrays instead of one 24 byte group](#fingerprints-and-counters-in-two-arrays-instead-of-one-24-byte-group) · 2026-09-05 · rejected
  - [Which counter a probe consults at each step of its sequence](#which-counter-a-probe-consults-at-each-step-of-its-sequence) · 2026-09-05 · rejected
  - [The width of the overflow counter, all four divisions of a group's eight counter bytes measured against the design's eight one-byte counters](#the-width-of-the-overflow-counter-all-four-divisions-of-a-groups-eight-counter-bytes-measured-against-the-designs-eight-one-byte-counters) · 2026-09-05 · kept
  - [Two optimizations the charts point at, one measured and one not yet](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet) · 2026-09-06 · kept
  - [Not zeroing the value index](#not-zeroing-the-value-index) · 2026-09-06 · rejected
  - [Three ideas the eighteen-map comparison suggested, all measured, none kept](#three-ideas-the-eighteen-map-comparison-suggested-all-measured-none-kept) · 2026-09-07 · rejected
  - [And the fifth point on that axis: an exact counter, worth 2-3%](#and-the-fifth-point-on-that-axis-an-exact-counter-worth-2-3) · 2026-09-07 · rejected
  - [Three layouts borrowed from other maps, all lost: line-aligned value indices, a second fingerprint in the index word, a 16 bit index](#three-layouts-borrowed-from-other-maps-all-lost-line-aligned-value-indices-a-second-fingerprint-in-the-index-word-a-16-bit-index) · 2026-09-05 · rejected
  - [A growth factor below 2, and the premise that suggested it was wrong](#a-growth-factor-below-2-and-the-premise-that-suggested-it-was-wrong) · 2026-09-08 · rejected
  - [The sliding window, built and measured rather than simulated](#the-sliding-window-built-and-measured-rather-than-simulated) · 2026-09-08 · rejected
  - [The lane-contention mechanism, confirmed by transplant, and it runs the other way from the guess](#the-lane-contention-mechanism-confirmed-by-transplant-and-it-runs-the-other-way-from-the-guess) · 2026-09-08 · info
  - [A dense map on flat_wmap's structure, measured against the shipped one, and the comparison is not what it looks like](#a-dense-map-on-flat_wmaps-structure-measured-against-the-shipped-one-and-the-comparison-is-not-what-it-looks-like) · rejected
  - [Eleven slots and twenty-four slots, and why sixteen is where it stops](#eleven-slots-and-twenty-four-slots-and-why-sixteen-is-where-it-stops) · 2026-09-10 · rejected
  - [Tiny pointers, and the bound insertion order puts on the value index](#tiny-pointers-and-the-bound-insertion-order-puts-on-the-value-index) · 2026-09-10/11 · rejected
  - [The default maximum load factor swept from 0.75 to 0.9: every step above 0.8 costs both compilers the same, and the one step below buys 0.8% for 6.7% more index](#the-default-maximum-load-factor-swept-from-075-to-09-every-step-above-08-costs-both-compilers-the-same-and-the-one-step-below-buys-08-for-67-more-index) · 2026-09-22 · rejected
  - [Double hashing with the step taken from the fingerprint, re-measured one header per binary after the churn harness was fixed: churned misses 1.07-1.18x faster, the score level, a fresh integer hit up to 5% slower under clang](#double-hashing-with-the-step-taken-from-the-fingerprint-re-measured-one-header-per-binary-after-the-churn-harness-was-fixed-churned-misses-107-118x-faster-the-score-level-a-fresh-integer-hit-up-to-5-slower-under-clang) · 2026-10-02 · superseded
  - [Seven probe sequences and five ways of computing their step against the triangular one: a key-dependent step shortens a churned miss, and every way of computing it costs the walk a third live value that no source shape hides](#seven-probe-sequences-and-five-ways-of-computing-their-step-against-the-triangular-one-a-key-dependent-step-shortens-a-churned-miss-and-every-way-of-computing-it-costs-the-walk-a-third-live-value-that-no-source-shape-hides) · 2026-10-02 · rejected
- [Lookup: the probe, SIMD compare and prefetch](#lookup-the-probe-simd-compare-and-prefetch)
  - [A miss had no bound, and eight chosen keys made it loop forever](#a-miss-had-no-bound-and-eight-chosen-keys-made-it-loop-forever) · 2026-09-05 · kept
  - [gcc left `probe` out of line, and forcing it inline is the largest single gcc gain on the branch](#gcc-left-probe-out-of-line-and-forcing-it-inline-is-the-largest-single-gcc-gain-on-the-branch) · 2026-09-05 · kept
  - [The gcc string-lookup gap, explained and mostly closed](#the-gcc-string-lookup-gap-explained-and-mostly-closed) · 2026-09-05 · kept
  - [Where a string hit's ~90 cycles go, from perf, and two more things it led to that lost](#where-a-string-hits-90-cycles-go-from-perf-and-two-more-things-it-led-to-that-lost) · 2026-09-07 · rejected
  - [Seven ideas from reading folly F14 and from the F14Vector string gap, all measured on 2026-09-07, none kept](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept) · 2026-09-07 · rejected
  - [The SSE probe audited, and its one real redundancy is compiler-dependent](#the-sse-probe-audited-and-its-one-real-redundancy-is-compiler-dependent) · 2026-09-07 · kept
  - [Splitting the probe past the home group: kept, for keys whose compare is a call](#splitting-the-probe-past-the-home-group-kept-for-keys-whose-compare-is-a-call) · 2026-09-10 · kept
  - [A block's prefetches should step from its start, not jump to its end](#a-blocks-prefetches-should-step-from-its-start-not-jump-to-its-end) · 2026-09-11 · kept
  - [`probe_result`'s shape swept two ways, a third argued down from the return sequence, and the one that ships is the best of them](#probe_results-shape-swept-two-ways-a-third-argued-down-from-the-return-sequence-and-the-one-that-ships-is-the-best-of-them) · 2026-09-12 · rejected
  - [`prefetch_index` was a line short for `group_big`, and covering that line is not the fix](#prefetch_index-was-a-line-short-for-group_big-and-covering-that-line-is-not-the-fix) · 2026-09-12 · kept
  - [An empty table reads a shared, never written sentinel index, so `find` and the insert's inlined lookup drop their `empty()` test: `find` up to 5% fewer instructions (3-5% for integer keys on both compilers; clang's big-value find +0.9%), gcc's small-table counting now ahead of 4.1.2, clang's 0.1-0.3 cycles closer, the score level; the index became a pointer and a count, so the map is 64 bytes, 8 fewer than 5.2.0](#an-empty-table-reads-a-shared-never-written-sentinel-index-so-find-and-the-inserts-inlined-lookup-drop-their-empty-test-find-up-to-5-fewer-instructions-3-5-for-integer-keys-on-both-compilers-clangs-big-value-find-09-gccs-small-table-counting-now-ahead-of-412-clangs-01-03-cycles-closer-the-score-level-the-index-became-a-pointer-and-a-count-so-the-map-is-64-bytes-8-fewer-than-520) · 2026-09-27 · kept
  - [#341, main's `find` hit against 4.5.0's in Redpanda's loop: half of the +20-27 instructions per call is the group probe itself (+11-14 in a bare loop) and half the caller around it; boost's `unordered_flat_map` runs as many instructions and 9-16% fewer cycles in that loop, and three candidates -- the walk past home out of line, no index prefetch, a speculative value prefetch from a preferred lane -- each cost cycles or did not move them, so nothing changed](#341-mains-find-hit-against-450s-in-redpandas-loop-half-of-the-20-27-instructions-per-call-is-the-group-probe-itself-11-14-in-a-bare-loop-and-half-the-caller-around-it-boosts-unordered_flat_map-runs-as-many-instructions-and-9-16-fewer-cycles-in-that-loop-and-three-candidates----the-walk-past-home-out-of-line-no-index-prefetch-a-speculative-value-prefetch-from-a-preferred-lane----each-cost-cycles-or-did-not-move-them-so-nothing-changed) · 2026-09-28 · rejected
  - [`prefetch_index`'s two lines re-measured one header per binary: dropping either is within 3% on both compilers from 50000 to 16M entries, so both stay, but not for the reason the audit gave](#prefetch_indexs-two-lines-re-measured-one-header-per-binary-dropping-either-is-within-3-on-both-compilers-from-50000-to-16m-entries-so-both-stay-but-not-for-the-reason-the-audit-gave) · 2026-10-02 · kept
- [Platforms: ARM, Windows and the CI matrix](#platforms-arm-windows-and-the-ci-matrix)
  - [Before NEON: on ARM the branch was 1.11x main, the same overall as on x86, split the opposite way](#before-neon-on-arm-the-branch-was-111x-main-the-same-overall-as-on-x86-split-the-opposite-way) · 2026-09-05 · superseded
  - [NEON closed the ARM lookup gap, and it was the whole gap](#neon-closed-the-arm-lookup-gap-and-it-was-the-whole-gap) · 2026-09-05 · kept
  - [The whole matrix, run on CI rather than on the desktop](#the-whole-matrix-run-on-ci-rather-than-on-the-desktop) · 2026-09-06 · info
- [Insert: what it inlines into its caller, and the call boundary](#insert-what-it-inlines-into-its-caller-and-the-call-boundary)
  - [The insert path is split in two by clang, and that is most of the build gap to boost.](#the-insert-path-is-split-in-two-by-clang-and-that-is-most-of-the-build-gap-to-boost) · 2026-09-05 · kept
  - [The insert path's instructions, counted one by one: the clang/gcc gap is the call boundary, and PGO removes all of it](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it) · 2026-09-10 · info
  - [Taking the hash apart once per insert instead of twice, and the shared pipeline that was not worth it](#taking-the-hash-apart-once-per-insert-instead-of-twice-and-the-shared-pipeline-that-was-not-worth-it) · 2026-09-11 · kept
  - [`try_emplace` on a present key, attacked from the hit side: three source shapes and `flatten` on the caller, and clang's count does not come down](#try_emplace-on-a-present-key-attacked-from-the-hit-side-three-source-shapes-and-flatten-on-the-caller-and-clangs-count-does-not-come-down) · 2026-09-22 · rejected
  - [`try_emplace` split at the home group: the hit and the common placement inlined into the caller, the walk past home behind a call for clang only, and every scored workload at or under main's instruction count on both compilers](#try_emplace-split-at-the-home-group-the-hit-and-the-common-placement-inlined-into-the-caller-the-walk-past-home-behind-a-call-for-clang-only-and-every-scored-workload-at-or-under-mains-instruction-count-on-both-compilers) · 2026-09-26 · kept
  - [`insert()` and `emplace()` look the key up first when the arguments already are the value: a set's string hit 2.2x, a map's `insert({k, v})` hit 4.5x under clang, and `emplace(k, new int)` must still build its value](#insert-and-emplace-look-the-key-up-first-when-the-arguments-already-are-the-value-a-sets-string-hit-22x-a-maps-insertk-v-hit-45x-under-clang-and-emplacek-new-int-must-still-build-its-value) · 2026-09-27 · kept
  - [udb3's `++h[key]` ran 70% slower under clang on 5.2.0 than on 5.1.0, and it was a loop variable stored and reloaded across a store with a late address: on Zen 4 that, with a division on the way to the next key, stops the loop overlapping its misses, 5x in a loop with no map at all; the part of the insert after a miss is called again under clang](#udb3s-hkey-ran-70-slower-under-clang-on-520-than-on-510-and-it-was-a-loop-variable-stored-and-reloaded-across-a-store-with-a-late-address-on-zen-4-that-with-a-division-on-the-way-to-the-next-key-stops-the-loop-overlapping-its-misses-5x-in-a-loop-with-no-map-at-all-the-part-of-the-insert-after-a-miss-is-called-again-under-clang) · 2026-09-27 · superseded
  - [The caller corpus: nine loops the way programs write them, one binary each, both compilers, in cache and past it, judged by each header's worst loop and not by a geomean; 5.2.0's worst was 3.3x under clang and 3.6x under gcc, the clang miss call of #310 brings clang's to 1.10, and under gcc nothing that removes the cliff is worth its price](#the-caller-corpus-nine-loops-the-way-programs-write-them-one-binary-each-both-compilers-in-cache-and-past-it-judged-by-each-headers-worst-loop-and-not-by-a-geomean-520s-worst-was-33x-under-clang-and-36x-under-gcc-the-clang-miss-call-of-310-brings-clangs-to-110-and-under-gcc-nothing-that-removes-the-cliff-is-worth-its-price) · 2026-09-27 · superseded
  - [The shape search: sixteen combinations of what the insert inlines, each judged by its worst ratio to the best combination anywhere measured, with the rule fixed before the results; it picks one shape for every compiler -- the home-group lookup inlined, the miss path and the walk past home called -- worst 1.11 under clang and 1.58 under gcc, where 5.2.0's everything-inlined reads 3.32 and 3.54](#the-shape-search-sixteen-combinations-of-what-the-insert-inlines-each-judged-by-its-worst-ratio-to-the-best-combination-anywhere-measured-with-the-rule-fixed-before-the-results-it-picks-one-shape-for-every-compiler----the-home-group-lookup-inlined-the-miss-path-and-the-walk-past-home-called----worst-111-under-clang-and-158-under-gcc-where-520s-everything-inlined-reads-332-and-354) · 2026-09-27 · kept
  - [`always_inline` on `place_element_at` re-measured on today's insert shape: clang's integer build 1.19-1.22x faster with it, gcc's unchanged, so it stays](#always_inline-on-place_element_at-re-measured-on-todays-insert-shape-clangs-integer-build-119-122x-faster-with-it-gccs-unchanged-so-it-stays) · 2026-10-02 · kept
- [Erase and churn](#erase-and-churn)
  - [Three on the value vector, the one part nothing had touched, all keeping it dense](#three-on-the-value-vector-the-one-part-nothing-had-touched-all-keeping-it-dense) · 2026-09-05 · rejected
  - [Why a dense erase is not slow, and where it is](#why-a-dense-erase-is-not-slow-and-where-it-is) · 2026-09-06 · info
  - [Pulling a displaced sibling home on erase, to take the churn drift back](#pulling-a-displaced-sibling-home-on-erase-to-take-the-churn-drift-back) · 2026-09-06 · rejected
  - [A mutating hit moves itself home, and that takes the churn drift back](#a-mutating-hit-moves-itself-home-and-that-takes-the-churn-drift-back) · 2026-09-06 · kept
  - [What `move_home` is actually worth, re-measured](#what-move_home-is-actually-worth-re-measured) · 2026-09-07 · kept
  - [Hoisting the moved element's hash out of `finish_erase`, so its latency overlaps the erase's own work](#hoisting-the-moved-elements-hash-out-of-finish_erase-so-its-latency-overlaps-the-erases-own-work) · 2026-09-08 · rejected
  - [`finish_erase` packed a slot that the store took straight back apart](#finish_erase-packed-a-slot-that-the-store-took-straight-back-apart) · 2026-09-11 · kept
  - [An unbounded probe that hangs -- and the 10% speedup that came with it is an inlining artifact](#an-unbounded-probe-that-hangs----and-the-10-speedup-that-came-with-it-is-an-inlining-artifact) · 2026-09-11 · kept
  - [Nothing holds a flat slot number any more, and gcc was the one paying for it](#nothing-holds-a-flat-slot-number-any-more-and-gcc-was-the-one-paying-for-it) · 2026-09-12 · kept
  - [The two value walks do not want the index prefetch the probe wants](#the-two-value-walks-do-not-want-the-index-prefetch-the-probe-wants) · 2026-09-12 · kept
  - [A slot back-pointer, re-tested across the cache boundary: the win is real, and it is cancelled by the inserts that put the elements there](#a-slot-back-pointer-re-tested-across-the-cache-boundary-the-win-is-real-and-it-is-cancelled-by-the-inserts-that-put-the-elements-there) · 2026-09-12 · rejected
  - [The #260/#262 sweep run over find and churn, and it comes back empty](#the-260262-sweep-run-over-find-and-churn-and-it-comes-back-empty) · 2026-09-12 · rejected
- [The rehash and its pipeline](#the-rehash-and-its-pipeline)
  - [Indexing the value container in the rehash cost clang a memory latency per element](#indexing-the-value-container-in-the-rehash-cost-clang-a-memory-latency-per-element) · 2026-09-05 · kept
  - [The rehash loop pipelined, and the partitioned rehash it was measured against](#the-rehash-loop-pipelined-and-the-partitioned-rehash-it-was-measured-against) · 2026-09-06 · kept
  - [The pipelined rehash does not transfer to `boost::unordered_flat_map`, and no other map has one](#the-pipelined-rehash-does-not-transfer-to-boostunordered_flat_map-and-no-other-map-has-one) · 2026-09-08 · rejected
  - [The rehash pipeline below 200000 entries, where it had never been measured: it costs a gcc integer build up to 4.2% in a band, and it stays](#the-rehash-pipeline-below-200000-entries-where-it-had-never-been-measured-it-costs-a-gcc-integer-build-up-to-42-in-a-band-and-it-stays) · 2026-09-15 · kept
- [Bulk operations: replace(), merge(), range insert, visit()](#bulk-operations-replace-merge-range-insert-visit)
  - [`replace()`'s gate was re-measured on a fixed harness and kept, and the harness is the finding](#replaces-gate-was-re-measured-on-a-fixed-harness-and-kept-and-the-harness-is-the-finding) · 2026-09-11 · kept
  - [`replace()`'s two loops walk with cursors too, and one cursor too many is slower than none](#replaces-two-loops-walk-with-cursors-too-and-one-cursor-too-many-is-slower-than-none) · 2026-09-11 · kept
  - [`merge()`, and why it is not the loop the caller would write](#merge-and-why-it-is-not-the-loop-the-caller-would-write) · 2026-09-11 · kept
  - [`replace()` hashes ahead too, once the reason it could not was looked at properly](#replace-hashes-ahead-too-once-the-reason-it-could-not-was-looked-at-properly) · 2026-09-11 · kept
  - [`insert(first, last)` sizing the table from the range: built, measured, declined](#insertfirst-last-sizing-the-table-from-the-range-built-measured-declined) · 2026-09-11 · rejected
  - [The other two pipelines do not want the cache gate, and the gate's own footprint model was wrong](#the-other-two-pipelines-do-not-want-the-cache-gate-and-the-gates-own-footprint-model-was-wrong) · 2026-09-11 · kept
  - [Chunks or a sliding ring: the rehash and the bulk visit want opposite answers, and the reason generalises](#chunks-or-a-sliding-ring-the-rehash-and-the-bulk-visit-want-opposite-answers-and-the-reason-generalises) · 2026-09-11 · kept
  - [A bulk `visit()`, and the `prefetch(key)` API it replaced -- which was measured against the wrong baseline](#a-bulk-visit-and-the-prefetchkey-api-it-replaced----which-was-measured-against-the-wrong-baseline) · 2026-09-11 · kept
  - [`visit()` re-measured across the cache boundary: the cache-resident loss is a hit-rate effect](#visit-re-measured-across-the-cache-boundary-the-cache-resident-loss-is-a-hit-rate-effect) · 2026-09-13 · info
  - [`replace()` + `extract()` as the way to unique a vector, and the duplicate rate that turns it over](#replace--extract-as-the-way-to-unique-a-vector-and-the-duplicate-rate-that-turns-it-over) · 2026-09-13 · info
  - [Loading a map from its values and its index: an owning load is 3-17x faster than building at 1M-64M entries and a view costs 0.85 ns per entry to check, but the slot check is most of an owning load, so the owning constructor takes `trust` too](#loading-a-map-from-its-values-and-its-index-an-owning-load-is-3-17x-faster-than-building-at-1m-64m-entries-and-a-view-costs-085-ns-per-entry-to-check-but-the-slot-check-is-most-of-an-owning-load-so-the-owning-constructor-takes-trust-too) · 2026-10-02 · kept
- [The hash function](#the-hash-function)
  - [The string hash restructured: independent blocks from 17 to 144 bytes](#the-string-hash-restructured-independent-blocks-from-17-to-144-bytes) · 2026-09-06 · kept
  - [The last structural lever on the hash, taken and found to weigh nothing](#the-last-structural-lever-on-the-hash-taken-and-found-to-weigh-nothing) · 2026-09-07 · rejected
  - [Four attempts at tuning the hash for latency, all worthless in the map, and the microbenchmark that said otherwise was wrong](#four-attempts-at-tuning-the-hash-for-latency-all-worthless-in-the-map-and-the-microbenchmark-that-said-otherwise-was-wrong) · 2026-09-07 · rejected
  - [Latency is what a map pays, tested rather than argued](#latency-is-what-a-map-pays-tested-rather-than-argued) · 2026-09-07 · rejected
  - [Six more hashes tried, none adopted, and the pair of columns says why](#six-more-hashes-tried-none-adopted-and-the-pair-of-columns-says-why) · 2026-09-09 · rejected
  - [Why `absl::Hash` is faster below 32 bytes, and what taking it would cost](#why-abslhash-is-faster-below-32-bytes-and-what-taking-it-would-cost) · 2026-09-09 · rejected
  - [The hash measured against the ones the other libraries ship](#the-hash-measured-against-the-ones-the-other-libraries-ship) · 2026-09-09 · info
  - [`hash_bytes` reads a key of compile-time length 8, 12 or 16 as 4 byte words under gcc and clang: a lookup right after writing a 12 byte key field by field goes 146 -> 40 cycles on both compilers, the hash value unchanged, strings untouched, a key already in memory 1.4 cycles more](#hash_bytes-reads-a-key-of-compile-time-length-8-12-or-16-as-4-byte-words-under-gcc-and-clang-a-lookup-right-after-writing-a-12-byte-key-field-by-field-goes-146---40-cycles-on-both-compilers-the-hash-value-unchanged-strings-untouched-a-key-already-in-memory-14-cycles-more) · 2026-09-27 · kept
- [Memory: huge pages, segmented_map, teardown order](#memory-huge-pages-segmented_map-teardown-order)
  - [What `segmented_map` gives up in 5.0.0, found in review.](#what-segmented_map-gives-up-in-500-found-in-review) · kept
  - [Those memory figures are a point on the sawtooth, and the octave says something else](#those-memory-figures-are-a-point-on-the-sawtooth-and-the-octave-says-something-else) · 2026-09-07 · method
  - [The opt-in huge page allocator, measured across the size axis](#the-opt-in-huge-page-allocator-measured-across-the-size-axis) · 2026-09-12 · kept
  - [The score runs on 4 KB pages and pays 1.6 billion L1 dTLB misses for it](#the-score-runs-on-4-kb-pages-and-pays-16-billion-l1-dtlb-misses-for-it) · 2026-09-12 · info
  - [Tearing a segmented_map down in reverse: glibc keeps the memory for the next build instead of handing it to the kernel, warm builds 1.8-3.2x when values own heap memory](#tearing-a-segmented_map-down-in-reverse-glibc-keeps-the-memory-for-the-next-build-instead-of-handing-it-to-the-kernel-warm-builds-18-32x-when-values-own-heap-memory) · 2026-09-26 · kept
  - [`segmented_map` lookups against `map`, one cell per binary: a hit costs 2-15 instructions more and a miss 0.4-2.4, and of that only about 5 instructions of clang's `find` hit are avoidable -- clang splits the value index a second time for `it->second`, where gcc reuses the probe's split -- so nothing was changed](#segmented_map-lookups-against-map-one-cell-per-binary-a-hit-costs-2-15-instructions-more-and-a-miss-04-24-and-of-that-only-about-5-instructions-of-clangs-find-hit-are-avoidable----clang-splits-the-value-index-a-second-time-for-it-second-where-gcc-reuses-the-probes-split----so-nothing-was-changed) · 2026-09-28 · rejected
  - [A `segmented_map`'s segment size, apart from the page: a map of strings iterates 3.3x slower than `map` with 4 KB segments and 1.24x with 256 KB because the segments sit between the strings' own buffers; for values without heap memory the size changes nothing, and exact-page segments lose everywhere](#a-segmented_maps-segment-size-apart-from-the-page-a-map-of-strings-iterates-33x-slower-than-map-with-4-kb-segments-and-124x-with-256-kb-because-the-segments-sit-between-the-strings-own-buffers-for-values-without-heap-memory-the-size-changes-nothing-and-exact-page-segments-lose-everywhere) · 2026-10-02 · kept
  - [A map_view over a mapped file: on hugetlbfs its random hits are 1.02-1.16x faster than on the file's 4 KB pages from 4M to 64M entries and 2.2-2.4x at 1M, it starts in 4 ms and is shared, so hugetlbfs is worth having and not required; a lazy 4 KB mapping of a file not in the page cache needs 8.7 s for its first 100000 lookups at 64M](#a-map_view-over-a-mapped-file-on-hugetlbfs-its-random-hits-are-102-116x-faster-than-on-the-files-4-kb-pages-from-4m-to-64m-entries-and-22-24x-at-1m-it-starts-in-4-ms-and-is-shared-so-hugetlbfs-is-worth-having-and-not-required-a-lazy-4-kb-mapping-of-a-file-not-in-the-page-cache-needs-87-s-for-its-first-100000-lookups-at-64m) · 2026-10-02 · kept
- [Small tables](#small-tables)
  - [#331, counting into a small table under clang 11-12% behind 4.1.2: about one cycle per row of latency in the 16-slot group's compare, on a table whose first slot almost always hits; #329's sentinel takes 0.1-0.3 cycles of it under clang and all of it under gcc, and nothing tried closes the rest](#331-counting-into-a-small-table-under-clang-11-12-behind-412-about-one-cycle-per-row-of-latency-in-the-16-slot-groups-compare-on-a-table-whose-first-slot-almost-always-hits-329s-sentinel-takes-01-03-cycles-of-it-under-clang-and-all-of-it-under-gcc-and-nothing-tried-closes-the-rest) · 2026-09-27 · open
  - [Small maps, two variants from #304 prototyped and measured against `main` over 20000 maps of 1 to 32 entries: no index below eight entries makes a map of up to four entries 1.06-2.3x faster to build, 1.2-2.2x faster to destroy and its lookups 1.5-3.7x faster, but puts back the empty-table test #329 took out of every lookup; a two-group minimum index helps a one- or two-entry integer map 2-28% and builds 32 entries 1.17-1.61x slower; neither was kept](#small-maps-two-variants-from-304-prototyped-and-measured-against-main-over-20000-maps-of-1-to-32-entries-no-index-below-eight-entries-makes-a-map-of-up-to-four-entries-106-23x-faster-to-build-12-22x-faster-to-destroy-and-its-lookups-15-37x-faster-but-puts-back-the-empty-table-test-329-took-out-of-every-lookup-a-two-group-minimum-index-helps-a-one--or-two-entry-integer-map-2-28-and-builds-32-entries-117-161x-slower-neither-was-kept) · 2026-09-28 · rejected
  - [Hits in cache-resident tables (1000 to 16000 entries), 4.5.0 against main (#346): main takes 0.41-0.61 of 4.5.0's time on scrambled integer keys and 0.71-0.79 on strings, from a quiet loop and a busy one, and 1.02-1.43x of it on dense ids 0..n-1, which a multiplicative hash places without collisions -- 4.5.0's best case, and the keys Redpanda's and OSRM's callers have](#hits-in-cache-resident-tables-1000-to-16000-entries-450-against-main-346-main-takes-041-061-of-450s-time-on-scrambled-integer-keys-and-071-079-on-strings-from-a-quiet-loop-and-a-busy-one-and-102-143x-of-it-on-dense-ids-0n-1-which-a-multiplicative-hash-places-without-collisions----450s-best-case-and-the-keys-redpandas-and-osrms-callers-have) · 2026-09-28 · info
- [Other maps](#other-maps)
  - [`ie64` ties boost while executing 58% more instructions, and the counts say why](#ie64-ties-boost-while-executing-58-more-instructions-and-the-counts-say-why) · 2026-09-05 · info
  - [The same-hash convention was flattering boost on every string chart, and the control found it](#the-same-hash-convention-was-flattering-boost-on-every-string-chart-and-the-control-found-it) · 2026-09-06 · info
  - [Read boost's `unordered_flat_map` again after it turned out to have the probe bound this map was missing](#read-boosts-unordered_flat_map-again-after-it-turned-out-to-have-the-probe-bound-this-map-was-missing) · 2026-09-06 · kept
  - [That last sentence stopped being true on 2026-09-05: building against boost after the rehash fix](#that-last-sentence-stopped-being-true-on-2026-09-05-building-against-boost-after-the-rehash-fix) · 2026-09-05 (memory re-measured 2026-09-06) · superseded
  - [Every other map on the same workloads, in one harness](#every-other-map-on-the-same-workloads-in-one-harness) · 2026-09-07 · info
  - [ihtab and ixhtab measured, and a bug in one of them reported upstream](#ihtab-and-ixhtab-measured-and-a-bug-in-one-of-them-reported-upstream) · 2026-09-07 · info
  - [Verstable measured rather than read](#verstable-measured-rather-than-read) · 2026-09-07 · info
  - [The F14Vector string miss, re-measured, and the old explanation of it is wrong](#the-f14vector-string-miss-re-measured-and-the-old-explanation-of-it-is-wrong) · 2026-09-10 · info
  - [Seventeen maps in their own default configurations, at a million entries: the dense layout wins iteration by 5.5x to 110x and the string build-and-destroy by 2.1x to 2.6x, and loses the integer find and churn to boost by 37% and 64%](#seventeen-maps-in-their-own-default-configurations-at-a-million-entries-the-dense-layout-wins-iteration-by-55x-to-110x-and-the-string-build-and-destroy-by-21x-to-26x-and-loses-the-integer-find-and-churn-to-boost-by-37-and-64) · 2026-09-12 · info
  - [Why boost's `unordered_flat_map` finds faster (#341): it needs no value index, so the first thing a lookup touches is 1 byte of metadata per slot against this map's 5.5, which leaves L2 at a far smaller table; from 46080 entries to 460800 this map takes 1.7-2.1x boost's L3 fills per find and 14-35% more cycles under clang (0.93-1.04 under gcc), while in L2 it is compiler codegen alone (this map's cycles 0.83-0.88 of boost's under gcc, 1.22-1.25 under clang)](#why-boosts-unordered_flat_map-finds-faster-341-it-needs-no-value-index-so-the-first-thing-a-lookup-touches-is-1-byte-of-metadata-per-slot-against-this-maps-55-which-leaves-l2-at-a-far-smaller-table-from-46080-entries-to-460800-this-map-takes-17-21x-boosts-l3-fills-per-find-and-14-35-more-cycles-under-clang-093-104-under-gcc-while-in-l2-it-is-compiler-codegen-alone-this-maps-cycles-083-088-of-boosts-under-gcc-122-125-under-clang) · 2026-09-28 · info
- [Real programs: MySQL, ClickHouse, STP, Redpanda, Valhalla, OSRM](#real-programs-mysql-clickhouse-stp-redpanda-valhalla-osrm)
  - [MySQL's int join never reaches the map, which makes it the control for how much relinking mysqld moves a query: 2.4%, the size of every MySQL difference measured for #321 and #323](#mysqls-int-join-never-reaches-the-map-which-makes-it-the-control-for-how-much-relinking-mysqld-moves-a-query-24-the-size-of-every-mysql-difference-measured-for-321-and-323) · 2026-09-27 · method
  - [MySQL's `EXCEPT` read 7-11% slower on 5.2.0 than on 4.4.0, and it is where the linker put the map's code: an inactive block added next to the call took it to +1% and removing it brought the +7-11% back, with `do_find` and `hash_bytes` byte-identical in both binaries](#mysqls-except-read-7-11-slower-on-520-than-on-440-and-it-is-where-the-linker-put-the-maps-code-an-inactive-block-added-next-to-the-call-took-it-to-1-and-removing-it-brought-the-7-11-back-with-do_find-and-hash_bytes-byte-identical-in-both-binaries) · 2026-09-27 · info
  - [ClickHouse's aggregation benchmark on real Yandex.Metrica columns, where 4.1.2 was published behind absl on every large column: main is within 1-5% of absl on the large columns except WatchID under clang (1.16), uses less memory than absl on two of the three, and is 12-13% behind 4.1.2 under clang on the columns with few distinct keys](#clickhouses-aggregation-benchmark-on-real-yandexmetrica-columns-where-412-was-published-behind-absl-on-every-large-column-main-is-within-1-5-of-absl-on-the-large-columns-except-watchid-under-clang-116-uses-less-memory-than-absl-on-two-of-the-three-and-is-12-13-behind-412-under-clang-on-the-columns-with-few-distinct-keys) · 2026-09-27 · info
  - [#326's op-cache footprint was taken away by #328: MySQL's `EXCEPT` went from 26-45x 4.4.0's op-cache misses on 5.2.0 to 1.9x and 1.1x in two layouts of main, and the query is level with 4.4.0 or faster in both](#326s-op-cache-footprint-was-taken-away-by-328-mysqls-except-went-from-26-45x-440s-op-cache-misses-on-520-to-19x-and-11x-in-two-layouts-of-main-and-the-query-is-level-with-440-or-faster-in-both) · 2026-09-27 · info
  - [STP's parser with 4.5.0 against 5.1.0 and main, the two instances from stp#560: 5.1.0 parses 4-5% faster on both instances under both compilers, with the same instructions within 1.3%, and 8% less peak memory on the larger instance; main reads within 3.1 points of 5.1.0](#stps-parser-with-450-against-510-and-main-the-two-instances-from-stp560-510-parses-4-5-faster-on-both-instances-under-both-compilers-with-the-same-instructions-within-13-and-8-less-peak-memory-on-the-larger-instance-main-reads-within-31-points-of-510) · 2026-09-28 · info
  - [stp#567's constant bit propagator tables re-checked on STP master: against the same four tables as std::unordered_*, 4.5.0 takes propagation to 0.912 and this repository's main to 0.859, nearly all of it on one instance (testcase15 0.74 and 0.69), and total solve time is level within 1% where #567 read -22% propagation and -4.9% total](#stp567s-constant-bit-propagator-tables-re-checked-on-stp-master-against-the-same-four-tables-as-stdunordered_-450-takes-propagation-to-0912-and-this-repositorys-main-to-0859-nearly-all-of-it-on-one-instance-testcase15-074-and-069-and-total-solve-time-is-level-within-1-where-567-read--22-propagation-and--49-total) · 2026-09-28 · info
  - [Redpanda's leader balancer benchmark (redpanda#17182) re-run standalone against main: main's `map` is 3-10% slower than 4.5.0 on the benchmark as written, whose map holds 80 keys, and 15-20% slower with one entry per raft group as in a real cluster, where it executes 20-27 more instructions per lookup; the `segmented_map` Redpanda ships is level (clang 0.917, gcc 1.042)](#redpandas-leader-balancer-benchmark-redpanda17182-re-run-standalone-against-main-mains-map-is-3-10-slower-than-450-on-the-benchmark-as-written-whose-map-holds-80-keys-and-15-20-slower-with-one-entry-per-raft-group-as-in-a-real-cluster-where-it-executes-20-27-more-instructions-per-lookup-the-segmented_map-redpanda-ships-is-level-clang-0917-gcc-1042) · 2026-09-28 · open
  - [Valhalla's CostMatrix (valhalla#4552) re-run with the map Valhalla ships (4.5.0), main and absl: all three within 1.1% on every row, as the original found ("all in all it hardly matters"), because the map is a few percent of a matrix request (`ReachedMap::add` 3.6%, out-of-line map code 0.36%)](#valhallas-costmatrix-valhalla4552-re-run-with-the-map-valhalla-ships-450-main-and-absl-all-three-within-11-on-every-row-as-the-original-found-all-in-all-it-hardly-matters-because-the-map-is-a-few-percent-of-a-matrix-request-reachedmapadd-36-out-of-line-map-code-036) · 2026-09-28 · info
  - [OSRM's e2e benchmarks (osrm-backend#6922) re-run on a quiet, pinned core with the 4.4.0 #6922 tried and with main: both take map matching to 1.21-1.27x the requests per second of `std::unordered_map`, CH table to 1.10x and CH trip to 1.05x, and main is not ahead of 4.4.0 -- equal on most rows, behind on CH match (1.255 against 1.274) and MLD table (0.986 against 1.009)](#osrms-e2e-benchmarks-osrm-backend6922-re-run-on-a-quiet-pinned-core-with-the-440-6922-tried-and-with-main-both-take-map-matching-to-121-127x-the-requests-per-second-of-stdunordered_map-ch-table-to-110x-and-ch-trip-to-105x-and-main-is-not-ahead-of-440----equal-on-most-rows-behind-on-ch-match-1255-against-1274-and-mld-table-0986-against-1009) · 2026-09-28 · info
- [The robin hood index before 5.0, and its dead ends](#the-robin-hood-index-before-50-and-its-dead-ends)
  - [Optimization dead ends (verified with paired A/B runs; re-test before assuming they still hold)](#optimization-dead-ends-verified-with-paired-ab-runs-re-test-before-assuming-they-still-hold) · rejected
  - [Where a year of this got to, measured against 4.8.1](#where-a-year-of-this-got-to-measured-against-481) · 2026-09-06 · info
- [Tooling: tests, mutation testing, fuzzing, CI, offline builds](#tooling-tests-mutation-testing-fuzzing-ci-offline-builds)
  - [Testing](#testing)
  - [Mutation testing](#mutation-testing) · method
  - [Mutation triage after the bound](#mutation-triage-after-the-bound) · 2026-09-05, re-run 2026-09-07 · info
  - [Fuzzing](#fuzzing) · method
  - [CI](#ci) · method
  - [Offline builds](#offline-builds) · method

<!-- /contents -->

## How to measure: the scored workloads and the harness lessons

The first part lists the rules the scored workloads (`bench_quick_overall_udm`, `test/bench/workloads.h`) obey; each exists because a workload once measured its input instead of the map. The second part is harness lessons, each one a measurement that first gave a wrong answer. Take away two things: never compare runs taken at different times, and never quote a ratio against another map family from one table size or from five points.

**Where it stands** (as of 2026-09-28)

- Scored workloads: string keys of 8 to 135 bytes skewed short, integer keys through a 64 bit bijection, a workload that grows, a 64 byte `big_value`, `tame_allocator()` everywhere, a fixed-size `churn`, a lookup rng that does not replay. Scores across a workload change are not comparable ([String keys must not all be the same length](#string-keys-must-not-all-be-the-same-length) and the six entries after it).
- Not scored: tables past the cache. boost leads a half-hit lookup by 1.20x at 200000 entries and an independent all-hit lookup by 1.69x at 1M, 1.89x at 4M and 1.63x at 16M; the old 8M half-hit point falls under the retraction of every number above 1M ([Two regimes the score does not cover](#two-regimes-the-score-does-not-cover-measured-2026-09-06-when-asking-what-a-more-realistic-benchmark-would-be), [The lookup harnesses measure throughput](#the-lookup-harnesses-measure-throughput-and-a-million-entries-is-not-past-the-cache-here)).
- Never compare runs from different times: a week-old CSV invented a 2-3% regression. Put both headers in one process with `maps.sh -r` ([Comparing today's runs against a stored CSV](#comparing-todays-runs-against-a-stored-csv-invented-a-2-3-regression-that-does-not-exist)).
- Lookup harnesses measure throughput, not latency. A million entries is L3-resident here; two million is the first size clearly past ([The lookup harnesses measure throughput](#the-lookup-harnesses-measure-throughput-and-a-million-entries-is-not-past-the-cache-here)).
- A cross-family ratio wants fifty points per octave (five carry up to 26% error against boost); a header-against-header A/B wants five (0.3-1.1%, 2.4% on `rhitstr`). A boost ratio not labelled octave geomean is a point measurement ([Fifty points draw the load-factor sawtooth](#fifty-points-draw-the-load-factor-sawtooth-that-five-average-out-and-five-points-are-worth-26-of-a-cross-family-ratio), [Every boost comparison in this file that predates 2026-09-07](#every-boost-comparison-in-this-file-that-predates-2026-09-07-was-taken-at-one-size-and-five-of-them-cross-100-when-the-octave-is-averaged-instead)).
- Take the null run (`scripts/ab/run.sh -r HEAD`) per build before believing a sub-benchmark delta under about 5% ([The paired harness has a systematic bias on `rmissstr`](#the-paired-harness-has-a-systematic-bias-on-rmissstr-about-36)).
- The size sweep interleaves its maps, picks its epoch count from a target interval width (at most 400; 101 fixed epochs gave about 5% before), rebuilds every map at every point, discards the first point and is capped at 1M; all claims above 1M are retracted ([The size sweep, and the measurement mistake](#the-size-sweep-and-the-measurement-mistake-it-took-three-tries-to-get-right), [Two more things the sweep got wrong](#two-more-things-the-sweep-got-wrong-found-on-2026-09-06-while-adding-a-confidence-band-to-the-absolute-chart)).
- A benchmark whose per-epoch batch is small enough to memorise must advance its own randomness ([The sweep replayed its key sequence](#the-sweep-replayed-its-key-sequence-which-flattered-the-branchiest-probe-by-27x)).
- A fixed side order makes the first side read 2-4% slower on the string first build; the teardown harness now rotates its sides ([The teardown harness ran its sides in a fixed order](#the-teardown-harness-ran-its-sides-in-a-fixed-order-and-the-side-that-runs-first-in-a-pass-reads-2-4-slower-on-the-string-first-build-the-5-18-first-build-gap-at-50000-that-309-could-not-explain-does-not-reproduce-with-the-same-two-headers-04-30-so-the-harness-now-rotates-its-sides)).

### Where the time goes

*no date · info · `scripts/ab/README.md`*

The measurements in `scripts/ab/README.md`, and the paired A/B harness behind them, were taken on the robin hood index. The u64 workloads were bound by branch mispredictions, the string workloads by the latency of a ~167 instruction lookup of which the hash is ~59. A model of 16 cycles per misprediction plus instructions at ~3.5 per cycle predicted changes within a cycle or two. The group index mispredicts far less (one branch per group, not one per bucket), so those numbers describe the workloads, which have not changed, and not the current lookup. The workload rules from [String keys must not all be the same length](#string-keys-must-not-all-be-the-same-length) through [Lookup benchmarks must not replay](#lookup-benchmarks-must-not-replay) still hold.

### String keys must not all be the same length.

*2026-09-02 · kept*

Until 2026-09-02 every string key of `bench_quick_overall_udm` was exactly 200 bytes. A hash dispatches on length, and one length makes that dispatch perfectly predictable: wyhash costs 0.31 branch mispredictions per hash on lengths spread over 4 to 200 bytes and 0.01 on a fixed length. 200 bytes also put every key on the heap, where a real workload keeps many inside the `std::string`. The keys now run from 8 to 135 bytes, skewed towards short. Scores before and after are not comparable.

### Integer keys must not be small sequential values.

*2026-09-02 · kept*

`insert_erase` and `iterate` draw values from a range that grows to 20000, and until 2026-09-02 the value was the key. The hash of a `uint64_t` is one multiply, and the top bits of a multiple of a small integer walk a lattice: 10000 such keys in 16384 buckets landed at most one to a bucket, with 39% of the buckets empty where a uniform hash leaves 54%. Nothing collided, so the probe never probed and the shifts never shifted: 82% of erases moved nothing, against 58% for the same values as strings. Two changes to the shift loops read as losses on `ie64` that were wins on every honest table. The value is now scrambled through a 64 bit bijection, so every checksum is unchanged and `ie64` sees the same table `iestr` does. A benchmark that rewards a hash for the one input it is perfect on is measuring the input.

### Inserting has to grow the table somewhere.

*2026-09-02 · kept · `build`*

Until 2026-09-02 no scored workload grew the table. `insert_erase` hovers at ~10k entries and doubles its bucket array about a dozen times in eight million operations. Building a million entries costs 52% more than after `reserve` for `uint64_t` keys and 31% more for strings, all of it rehashing, so a map that grew badly would have scored the same as one that grew well. `build` covers growth, and it showed at once that this is where the map is weakest (see `scripts/ab/README.md`).

### A mapped value of eight bytes hides what a dense map is for.

*2026-09-03 · kept · `workloads::big_value`*

Until 2026-09-03 both scored maps held a `size_t`. A flat map writes the whole value into a hash-scattered slot, so every cost scales with `sizeof(value_type)`; a dense map writes eight bytes there and appends the payload to a vector. On `build` with a `uint64_t` key, `boost::unordered_flat_map` is 1.22x ahead at a 16 byte `value_type` and **2.14x behind** at 64, the same property that wins `buildstr` and loses `build64`. `map<Key, SomeStruct>` is at least as common as `map<Key, size_t>`. `workloads::big_value` is 64 bytes and trivially copyable so that the variable is the value's size, not the heap. Its checksums equal the small-value maps', so one set of constants verifies all three.

**Later (2026-09-06):** after the rehash fix this map builds faster than boost at every value size measured (1.37x at 8 bytes, 2.10x at 64), see [The four charts worth keeping, and the two axes that had none](#the-four-charts-worth-keeping-and-the-two-axes-that-had-none).

### `build` measured the kernel's page fault handler as much as the map, until `tame_allocator()`.

*2026-09-03 · kept · found while adding the big-value map; glibc only*

`workloads::tame_allocator()` raises `M_MMAP_THRESHOLD` and `M_TRIM_THRESHOLD` to 64 MB, so pages are faulted once per process, not once per repetition. On `build64`: 6.61ms to 3.78ms per build, 41141 page faults to 2289, total cycles within 4% of user cycles.

Before, glibc returned every block above its mmap threshold to the OS, so each repetition faulted in fresh pages: 38% of `build64`'s cycles were kernel (2057 page faults per build), 65% of `buildbig`'s. Whether it was paid depended on what ran *before* in the process, since freeing a large block raises the threshold. So `buildbig` reported 4.31ms for a build costing 14.9ms alone, while executing *more* cycles (19.2M against 16.6M) and instructions than `build64`, which reported 6.04ms. Every workload now calls `tame_allocator()`. Isolated and in-score come close for all three maps (`buildbig` was 14.90 against 4.31, now 4.94 against 4.26, still 16% apart), and all run at the same effective clock (corrected 2026-10-02: said "agree for all three maps"). A warm allocation before the build does *not* work, touched or not (6.60ms and 6.11ms).

A program that builds one map in a fresh process does pay this cost, but once; the benchmark paid it every repetition. glibc only; elsewhere a no-op.

### Nothing measured a table that only churns.

*2026-09-03 · kept · `churn`*

Until 2026-09-03 every scored workload grew or used a just-built table. Backward shift deletion leaves the table as if the erased element was never inserted. A design that frees a slot without undoing what probed past it has probe sequences that only grow, repaired by a rehash. `build` and `insert_erase` keep growing, and a growth rehash resets that damage for free, so the score could not tell the two apart.

`churn` fills to 50000, reserves, then erases one and inserts one with two lookups between, at a fixed size. Over two million operations at 200000 entries, `boost::unordered_flat_map`'s lookups degrade to **1.31x** of their fresh cost and snap back on an in-place rehash every third round (its bucket count never changes; that round costs +7ns per operation). This map's stay flat within the noise. With string keys both degrade, because the heap holding the key bodies degrades, and this map churns **1.28x faster** than boost throughout. Against boost the sustained gap is much smaller than the fresh-table one: 1.32x on `churn64`, where `rhit64` on a fresh table is 1.71x, and 1.03x on `churnstr`, the narrowest string workload it does not already win. Scores before this workload are not comparable with scores after it.

**Open (2026-10-02):** the entry does not record the table size behind each of 1.31x, 1.28x, 1.32x and 1.03x, and "1.28x faster" on strings sits badly with boost leading `churnstr` by 1.03x; the run log is gone and the robin hood index cannot be re-measured.

**Later (2026-09-07):** the churn ratios against boost in this entry are single-size measurements; over an octave boost is 1.15-1.29x ahead on churn, see [Every boost comparison in this file that predates 2026-09-07 was taken at one size](#every-boost-comparison-in-this-file-that-predates-2026-09-07-was-taken-at-one-size-and-five-of-them-cross-100-when-the-octave-is-averaged-instead)

### Lookup benchmarks must not replay.

*2026-09, no day given · kept*

Until 2026-09 the find workload of `bench_quick_overall_udm` reset its search rng to the insertion rng's seed, so its hits and misses repeated and a TAGE-style predictor learned much of it: 0.6 mispredictions per lookup where a random sequence costs the scalar probe 1.35. That under-reported branchy probing and hid most of the SSE2 probe's gain. The workload now uses an rng of its own. `find_random.cpp` still replays. The same mistake recurred in the size sweep (see [The sweep replayed its key sequence](#the-sweep-replayed-its-key-sequence-which-flattered-the-branchiest-probe-by-27x)).

### Two regimes the score does not cover, measured 2026-09-06 when asking what a more realistic benchmark would be.

*2026-09-06 · info · `scripts/ab/sweep.cpp`, `scripts/ab/plot.py`, `scripts/ab/regen.sh`*

*Small, short-lived maps*: build, use and destroy, 2000 times, `map<uint64_t, size_t>`. Expected to be a weakness, because the index is now two allocations rather than one and a lookup touches three cache lines rather than two. It is the opposite: against main 1.90x at 8 entries, 2.55x at 32, 1.64x at 128, 1.52x at 1024, and ahead of boost except at 8 (boost 1.97x main). Branch mispredictions run 0.0-0.8% against main's 0.8-3.4%: the robin hood shift is a coin flip, the group probe is not. Nothing to fix; a workload would only keep it that way.

*Tables far larger than cache*: the real gap. The score's largest table, 200000 entries, has an index of about 1 MB that sits in L2 or L3, and the value index is a dependent load a flat map does not pay. Half-hit lookups, same hash, this desktop:

| entries | main | this map | boost |
|---|---|---|---|
| 200000 | 9.8 ns | 7.1 ns | 5.9 ns |
| 8000000 | 54.4 ns | 42.8 ns | 30.4 ns |

boost's lead grows from **1.20x to 1.41x**, and the score sees only the small end. This map still beats main at both sizes (1.38x and 1.27x), so it is a design cost, not a regression, and the one cost the suite understates, as it once understated growth, churn and big values.

**Both of those points come from one unpaired measurement each and should be read as indicative.** Sweeping the size axis to 134M produced a shape (lead peaking near 2M, then narrowing) that did not survive re-measurement and is retracted (see [Two more things the sweep got wrong](#two-more-things-the-sweep-got-wrong-found-on-2026-09-06-while-adding-a-confidence-band-to-the-absolute-chart) and [The size sweep](#the-size-sweep-and-the-measurement-mistake-it-took-three-tries-to-get-right)). Safe to say: the lead exists at both sizes, and any claim about it must name its size.

It is a tool, not a scored workload, because 8M entries is ~170 MB and about a second to build. `scripts/ab/sweep.cpp` walks every power of two from 16 to 8M for the working tree, a baseline revision and boost; `scripts/ab/plot.py` draws the CSV as an SVG with only the standard library. The result is `doc/find_vs_size.svg`; `scripts/ab/README.md` says how to regenerate it. Under `doc/` only the two `allocated_memory` files are tracked; CSVs and charts are produced by `scripts/ab/regen.sh`, so a fresh clone has none until it runs it. The chart measures the scored find case (random lookup, 50% hit rate, own rng) in two linear panels: sizes to 64K, and the whole range, now to 134M entries, about 5 GB of map, where the curve flattens into the memory-latency plateau and the ratios stop moving. Three flat, close lines run to about 128K entries, then all turn upward and the gap to boost opens. With nothing reserved and twelve points per octave, each line shows a sawtooth as the load factor climbs and falls back at the doubling. Robin hood swings by about a factor of two between an empty and a full table; the group index and boost barely swing, because a group is compared whole whatever its occupancy.

**Later (2026-09-12):** `doc/` also tracks the README's `bench_readme.csv` and four SVGs from `scripts/ab/bench_readme.sh`; what `scripts/ab/regen.sh` writes is still untracked, see [Seventeen maps in their own default configurations](#seventeen-maps-in-their-own-default-configurations-at-a-million-entries-the-dense-layout-wins-iteration-by-55x-to-110x-and-the-string-build-and-destroy-by-21x-to-26x-and-loses-the-integer-find-and-churn-to-boost-by-37-and-64).

**Later (2026-09-06):** the sweep is capped at 1M entries and every number above 1M is retracted as not reproducible, so the 8M and 134M ranges above are no longer what it runs, and the 8M row of the half-hit table falls under the same retraction; see [Two more things the sweep got wrong](#two-more-things-the-sweep-got-wrong-found-on-2026-09-06-while-adding-a-confidence-band-to-the-absolute-chart)

### The size sweep, and the measurement mistake it took three tries to get right

*2026-09-06 · method · `doc/*_vs_size.svg`, `scripts/ab/sweep.cpp`*

Three charts (find at 50% hits, churn, insert-erase) of one map grown through 377 sample points, twenty-four per octave (193 and twelve until the regeneration later that day), nothing reserved, to 1M entries.

**The mistake is the part worth keeping.** The first two versions measured main to completion, then this map, then boost. Any drift between phases (clock ramp, noisy neighbour, page placement) landed entirely on one map. Two runs of *identical* work disagreed by up to 140% above 1M entries and by tens of percent below; one run put a whole octave 70% high while looking smooth. That run produced the "insert-erase is anomalously slow at 256 buckets" feature; the map was never wrong. The A/B harness's `compare()` interleaves alternatives round by round so drift cancels, and the sweep now does the same and reports each point's interval. Even paired it needs **101 epochs** for intervals around 5% of the ratio; at nanobench's default 11 they were 25% wide.

**Later (2026-09-06):** the sweep now asks nanobench for an interval width (at most 400 epochs) instead of naming a count, see [`scripts/ab/regen.sh` rebuilds everything in `doc/`](#scriptsabregensh-rebuilds-everything-in-doc).

With that fixed, and only out to 1M: **Summarise across an octave, never at a chosen load.** The earlier version quoted ratios at "load 0.79", the last point before *this map* doubles. That is a biased subsample: boost sizes differently (max load 0.875, 1966079 buckets at a million entries) and swings 4-6x across its own octave. It produced "this map is 2.2x ahead of boost on churn below 100000 entries", which the octave geomean does not support. Retracted.

Octave geomean, this/boost / this/main, above 1.00 meaning the other map is ahead, twenty-four points per octave, octave starting at each size:

| workload | 1K | 32K | 128K | 512K |
|---|---|---|---|---|
| find, all hits | 1.07 / 0.76 | 1.21 / 0.72 | 1.14 / 0.76 | 1.28 / 0.70 |
| churn | 1.13 / 0.68 | 1.25 / 0.66 | 1.46 / 0.72 | 1.90 / 0.85 |
| insert and erase | **0.97** / 0.74 | 1.22 / 0.73 | 1.31 / 0.78 | 1.56 / 0.74 |

boost is ahead on all three everywhere (1.07-1.13x at a thousand entries, 1.28-1.90x at half a million) except insert-and-erase at a thousand, where this map is 3% ahead. This map beats robin hood everywhere by 1.2-1.5x. String keys, cells are this/boost with this map's hash / this/main / this/boost with boost's own hash:

| workload | 1K | 32K | 128K | 256K |
|---|---|---|---|---|
| find, all hits | 1.06 / 0.94 / **0.77** | 1.11 / 0.93 / 0.95 | 1.16 / 0.91 / 0.94 | 1.18 / 0.90 / 0.96 |
| churn | 1.05 / 0.83 / 0.82 | 1.19 / 0.92 / 1.08 | 1.28 / 0.92 / 1.12 | 1.38 / 0.92 / 1.21 |
| insert and erase | 1.04 / 0.85 / 0.78 | 1.13 / 0.92 / 0.95 | 1.16 / 0.92 / 0.98 | 1.22 / 0.91 / 1.03 |

The this/main string column shows the new hash; main has the old one. The dense map's answer is the two charts boost is not on: 10.9x on iteration, and a faster build at every value size. Not true, though asserted here for a day: that the counters buy a win over boost on churn at small sizes.

The flatness claim survives, since it is about one map's own curve: over a fully sampled octave from 1K to 64K, cheapest to dearest point, this map swings 1.2-1.5x on churn, boost 4.2-6.1x and 4.8.1 2.1-2.4x.

### Every workload is measured for both key types

*2026-09-06 · kept · `sweep.cpp`, `valuesize.cpp`, `memory.cpp`*

`sweep.cpp`, `valuesize.cpp` and `memory.cpp` take a key argument; strings use the scored `key_for` (8 to 135 bytes skewed short, since one fixed length makes the hash's length dispatch predictable). Miss keys now come from a disjoint pool built up front, not `key ^ 1`, so a miss is absent by construction and not a neighbour of a present key. Churn takes inserts from a spare pool and returns what it erased, so no key is constructed in the timed region (for a string that would measure the allocator). `memory.cpp` replaces global `operator new` instead of using a container allocator, because `std::allocator<char>` allocates a string's body unseen by the container. For integer keys the two methods agree to the byte.

### The sweep replayed its key sequence, which flattered the branchiest probe by 2.7x

*2026-09-06 · method · found because the 4.8.1 line came out ahead of this map; `perf` on one-map binaries*

Each workload seeded its `Rng` inside the timed function, so all 400 epochs looked up the same 20000 keys with the same hit-or-miss decisions, which the predictor learns (mechanism in [Lookup benchmarks must not replay](#lookup-benchmarks-must-not-replay)). One binary, per 20000 all-hits lookups at 33000 entries:

| | replayed | carried-on |
|---|---|---|
| main | 118.6 us | 120.8 us |
| this map | 104.6 us | 96.1 us |
| 4.8.1 | **79.8 us** | **154.1 us** |

The scalar robin hood probe is the only one with branches to mispredict: 0.575 per lookup against main's 0.021 and this map's 0.024. The scored workloads escape only because `find_all` does 10M lookups per epoch, the sweep 20000. Every chart in `doc/` predating the fix was wrong; the sweep's rngs now live in a `state` that outlives the epochs. The rule: a benchmark whose per-epoch batch is small enough to memorise must advance its own randomness. One-map-per-binary `perf` catches it: the fixed sweep agrees with it to 3-8% and orders the maps identically; the replayed one did not.

### Two more things the sweep got wrong, found on 2026-09-06 while adding a confidence band to the absolute chart.

*2026-09-06 · method · `scripts/ab/sweep.cpp`*

Neither is about the map.

*The first sample point of a process reads high*: cold caches, allocator and clock are paid by the first point, and pairing cannot cancel it because the *point* is cold, not one alternative. At 16 entries boost read 19% above its value at 17, on 1.5% intervals. The sweep now measures the first point twice and discards the first answer.

*And a single paired run is still not trustworthy point by point.* In one of five find runs this map alone read 15-54% high at four adjacent sizes (4.59 ns at 64 entries against 2.86 to 3.03), with 0.5% intervals and the other maps normal. A fourth map later put boost at 4.72 ns at 128 entries against 2.4-2.6 elsewhere, wrong from 128 to about 8192. The cause: the sweep *grew* its maps through the points, so addresses depended on the others' interleaved allocations and a bad layout persisted. It now rebuilds every map at every point, about a second over the whole sweep, so a bad layout spoils one point. Still, read a chart for its shape and check a surprising *point* against a second run. On a quiet machine two runs agree to 0.78% median on the median epoch, 0.49% on the fastest epoch and 0.92% on the ratio, but differ by 9.3%, 6.9% and 6.0% at their worst points, which the charts' confidence bands do *not* bound.

**Retracted, because the data behind them does not reproduce**: that boost's find lead "peaks around 2M and narrows again", that main overtakes this map on churn above 8M, and every other number for tables above 1M. The sweep is capped at 1M. Above it, measuring needs a fresh process per size, because at multi-GB sizes page placement varies from run to run.

### Every boost comparison in this file that predates 2026-09-07 was taken at one size, and five of them cross 1.00 when the octave is averaged instead.

*2026-09-07 · method · one binary, `main` against the working tree against boost, the same 20 workloads at one size and at five sizes per octave*

Above 1.00 means this map is faster.

| | vs main | vs boost | vs boost, no iteration |
|---|---|---|---|
| one size | 1.242 | 1.586 | 1.144 |
| octave geomean | 1.208 | 1.502 | **1.066** |

No `vs main` workload changes sign; five boost workloads do:

| workload | boost, one size | boost, octave |
|---|---|---|
| `churn64` | 1.191 | **0.776** |
| `churnbig` | 1.237 | **0.797** |
| `churnstr` | 1.087 | **0.866** |
| `rmiss64` | 1.112 | **0.916** |
| `iebig` | 0.984 | 1.002 |
| `buildstr` | 1.544 | 1.993 |
| `buildbig` | 1.928 | 1.775 |

**The reason the two columns behave differently is the whole point.** This map and `main` have power-of-two bucket counts and the same maximum load factor, so they double at the *same* sizes: their sawtooths are in phase and cancel, and every same-family paired A/B is sound however sampled. boost has non-power-of-two bucket counts (1966079 at a million entries) and maximum load 0.875, so it is out of phase, and one size compares one map near its peak against the other anywhere in its cycle.

So **"this map is ahead of boost on churn at a fixed size" is a single-size artefact**, stated in the table of [That last sentence stopped being true on 2026-09-05](#that-last-sentence-stopped-being-true-on-2026-09-05-building-against-boost-after-the-rehash-fix) (`churn at a fixed size` 1.16 and 1.21) and in [Nothing measured a table that only churns](#nothing-measured-a-table-that-only-churns) (`churn64` 1.32x). Over an octave boost is 1.15-1.29x ahead on churn for all three value types, and the lead over boost outside iteration is 1.066, not 1.144. The lead over `main` is unaffected. **Every boost ratio anywhere in this file and in `scripts/ab/README.md` that is not explicitly labelled as an octave geomean is a point measurement**; re-take it with `run.sh` at its default `-p 5`. The size-axis charts here ([The size sweep, and the measurement mistake it took three tries to get right](#the-size-sweep-and-the-measurement-mistake-it-took-three-tries-to-get-right)) and in `scripts/ab/README.md` already summarise by octave.

**Open (2026-10-02):** the `churn64` 1.32x in [Nothing measured a table that only churns](#nothing-measured-a-table-that-only-churns) most likely reports boost ahead (its fresh-table hit lead of 1.71x over its 1.31x churn degradation is 1.31x), not this map ahead; the 2026-09-03 run log is gone, so this is not settled.

**Later (2026-09-12):** five points still carry up to 26% sampling error on a boost churn ratio and 16% on `buildbig`; a cross-family ratio wants fifty, see [Fifty points draw the load-factor sawtooth that five average out](#fifty-points-draw-the-load-factor-sawtooth-that-five-average-out-and-five-points-are-worth-26-of-a-cross-family-ratio).

### The paired harness has a systematic bias on `rmissstr`, about 3.6%

*2026-09-11 · method · `scripts/ab/run.sh -r HEAD`*

It showed up as a 1.037-1.039 "win" in two consecutive PRs, against two baselines, for changes that could not reach a lookup. `scripts/ab/run.sh -r HEAD` on a clean tree, **the same header on both sides**, reads `rmissstr` **1.036**, `buildbig` 0.983, `buildstr` 0.984 and `hashstr` 0.872. The candidate side reads faster on `rmissstr` and slower on the other three (corrected 2026-10-02: said "The candidate side is systematically faster there"), so a single-digit-percent reading on them means nothing. Take the null run, one run's cost, before believing any sub-benchmark delta under about 5%.

Re-taken 2026-09-12 after the lookup workloads stopped rebuilding their table (see [Fifty points draw the load-factor sawtooth](#fifty-points-draw-the-load-factor-sawtooth-that-five-average-out-and-five-points-are-worth-26-of-a-cross-family-ratio)): `rmissstr` reads 0.993 to 1.008 and `rhit64` 0.976 to 1.000 across builds, and `-falign-functions=32` moves that 0.976 to 0.997. **The null run has to be re-taken per build, not per harness**: each build is one sample of the layout band, and the biased workload changes.

### Fifty points draw the load-factor sawtooth that five average out, and five points are worth 26% of a cross-family ratio

*2026-09-12 · #274 · method · Ryzen 9 7950X, clang 22, `scripts/ab/run.sh -p`, `scripts/ab/ab.cpp`, `test/bench/workloads.h`*

A cross-family ratio (against boost, or an index with another number of slots per group) wants fifty points per octave. A header-against-header A/B does not: five points are worth 0.3-1.1% there, 2.4% on `rhitstr`, and `-p 5` stays the default (corrected 2026-10-02: said "five points are worth 0.6% there"). The harness has swept five sizes per octave since 2026-09-07 (see [Every boost comparison in this file that predates 2026-09-07](#every-boost-comparison-in-this-file-that-predates-2026-09-07-was-taken-at-one-size-and-five-of-them-cross-100-when-the-octave-is-averaged-instead)). #274 asked: five points cannot hit the *peak*, the size just before a doubling where probes are longest, so is a denser sweep affordable, is the octave one whole cycle, and can the payload be cut?

**The switch did not mean what it said.** `-p N` ran the first N of five fixed scale factors, so `-p 3` swept n to 1.32n (three fifths of an octave) and printed "octave geomean" over it. No recorded entry used it (every `run.sh` entry says five). Now `run.sh -p` rebuilds the binary (sizes are template arguments), and point i of P is `n * 2^(i/P)`, exactly one doubling for any P.

**What fifty points see that five do not.** Dearest point over cheapest, per element, same header both sides, twelve epochs:

| | 5 points | 50 points | dearest at |
|---|---|---|---|
| `rmiss64` | 1.286x | **1.458x** | load 0.795 |
| `churn64` | 1.142x | **1.284x** | load 0.795 |
| `churnbig` | 1.148x | 1.247x | load 0.795 |
| `rhit64` | 1.115x | 1.168x | load 0.742 |
| `churnstr` | 1.055x | 1.120x | load 0.752 |
| `rmissstr` | 1.061x | 1.099x | load 0.795 |

Five points reach at most load 0.763 of a maximum 0.800; fifty reach 0.795. A third of `rmiss64`'s amplitude (0.17 of 0.46) is in that last 4% of load: a miss costs superlinearly in the load factor, so the peak is a spike that a coarse sweep steps past (corrected 2026-10-02: said "Half of `rmiss64`'s amplitude").

**The spike belongs to the miss and not to the hit.** `rmiss64` steps 32.2% and `rmissstr` 10.3% at the doubling. `rhit64`'s largest step is 2.0% and `rhitstr`'s 2.9%, neither at the doubling; their 1.168x and 1.097x (`rhitstr`'s is not in the table) climb with table size, not load. A present key is found in its home group at any load; an absent one walks.

**And what five points are worth, measured rather than argued.** A fifty-point run holds ten five-point sweeps (every tenth point). The range of the ten five-point geomeans, to compare with the fifty-point column in the next table (corrected 2026-10-02: said "Each one's geomean over the fifty-point one"):

| | `base/cand`, the same header twice | `boost/cand` |
|---|---|---|
| `churn64` | 0.999 .. 1.005 | **0.742 .. 1.023** |
| `churnbig` | 0.995 .. 1.002 | 0.787 .. 1.069 |
| `churnstr` | 0.998 .. 1.001 | 0.855 .. 1.038 |
| `rmiss64` | 0.989 .. 0.996 | 0.833 .. 1.010 |
| `build64` | 0.992 .. 1.003 | 1.459 .. 1.571 |
| `rhit64` | 0.974 .. 0.980 | 0.753 .. 0.776 |
| `rhitstr` | 0.993 .. 1.017 | 0.858 .. 0.881 |

**Against another revision of this header, five points are worth 0.3-1.1%, 2.4% on `rhitstr`**: the sawtooths are in phase and cancel (corrected 2026-10-02: said "worth 0.6%"). **Against boost they are worth 26%**: `churn64` reads 0.742 or 1.023 depending only on the starting point. Two fifty-point runs the same day reproduce that spread within 0.4%, so it is phase, not noise. Five- and fifty-point geomeans differ by 2.3% or less on eight of ten workloads, 4.3% on `build64` and **16% on `buildbig`** (1.986 against 1.716; see the `buildbig` paragraph below) (corrected 2026-10-02: said "differ under 4% on nine of ten workloads").

Quote the fifty-point column from now on. `boost::unordered_flat_map` with this library's hash, **boost's time over this map's, so above 1.00 means this map is faster**:

| | 5 points | 50 points |
|---|---|---|
| `build64` | 1.591 | 1.526 |
| `buildstr` | 2.026 | 2.054 |
| `buildbig` | 1.986 | **1.716** |
| `churn64` | 0.805 | 0.810 |
| `churnstr` | 0.890 | 0.898 |
| `churnbig` | 0.843 | 0.858 |
| `rhit64` | 0.761 | 0.762 |
| `rmiss64` | 0.868 | 0.888 |
| `rhitstr` | 0.875 | 0.869 |
| `rmissstr` | 0.832 | 0.837 |

The columns agree because one five-point phase was run, as a separate `-p 5` binary at the sizes of the first sub-sweep; the previous table shows all ten sub-sweeps of the fifty-point run, and `build64`'s 1.591 lies 1.3% above their range, run-to-run noise (corrected 2026-10-02: said "one of ten five-point sweeps was run; the previous table shows the other nine").

**`buildbig`'s octave has a 5.9x step in it that has nothing to do with the load factor.** A `map<uint64_t, big_value>` build costs 3.90 ms at 249666 entries and 23.09 ms at 263902, 5.7% larger, and stays there. 72 bytes (key and 64 byte value) times a quarter million is 18 MB of values, and the vector doubles at 262144 elements; doubling it needs old and new at once, which a 32 MB L3 stops holding (corrected 2026-10-02: said "64 bytes times a quarter million is 17 MB of values"). Dearest over cheapest for `buildbig`, not in the sawtooth table, reads 6.45x: a cache cliff, not a cycle. Five points put two below it and three above, fifty put five and forty-five, and the geomean moves accordingly: the "sweep across the cache boundary" rule in a workload never thought of as a size sweep.

**The sizes are template arguments, and that is worth at most 1.4%, which is the layout band, on seven of eight workloads; `build64` reads 0.959, opaque faster** (corrected 2026-10-02: said "worth at most 1.4%, which is the layout band"). The premise in `workloads.h`, "turning a literal loop bound into a runtime value is a change even when the value is the same", had never been measured. Two builds from one source, the second passing every size through `asm volatile("" : "+r"(v))`, which hides it from the compiler as a function argument would. Candidate time, opaque over constant, five sizes:

| `build64` | `churn64` | `ie64` | `find64` | `buildstr` | `churnstr` | `rhit64` | `rmissstr` |
|---|---|---|---|---|---|---|---|
| 0.959 | 1.014 | 1.006 | 1.006 | 1.010 | 0.995 | **0.998** | **0.986** |

The last two are the control: `rhit*`/`rmiss*` take their trip count from a body constant, so their code is identical in both binaries, and 0.998 and 0.986 is the two-binary layout band. Every changed workload but `build64` is inside it, both directions; `build64`'s 0.959 says the *opaque* build was faster, which is layout luck (corrected 2026-10-02: said "Every changed workload is inside it"). **Open (2026-10-02):** `build64` is 4.1% off, three times this control band, so "layout luck" is untested; a second layout (`-falign-functions=32`) and gcc would decide it. So the scored instantiation keeps its literal (silently editing the benchmark is the thing to avoid), but for swept points it buys nothing. It costs compile time: 17.4 s at fifty sizes against 3.6 s at five. Per-point control ratios scatter by 0.25-0.92% standard deviation within one run, mostly under nanobench's interval, and the geomean's standard error is 0.04-0.33%. **If a grid finer than fifty is ever wanted, passing the swept sizes at runtime is the change to make, and this is the measurement that says it is safe.**

**The growth is measured now, not assumed.** An octave is one cycle only if the bucket array doubles, which is a policy (a growth factor under two is a recorded knob). Before timing, the harness records every size where `bucket_count()` changes:

    # cand  grows at 1639 3277 6554 13108 26215 52429 104858 209716   x2.000 -- one octave is exactly one sawtooth
    # boost grows at 1680 3360 6720 13440 26880 53760 107520 215040   x2.000 -- one octave is exactly one sawtooth

Both double, so the octave geomean is unbiased for both. They double 2.5% apart, which is why the boost column needs the points. A ratio other than 2.000 prints a warning. Every point line also shows its load factor.

**The run does not get ten times longer, and the reason is where a point's cost was cut.** All twenty workloads, twelve epochs, clang:

| | five points | fifty points |
|---|---|---|
| before this change | 3m19 | |
| after | **2m21** | **5m34** |
| with boost as a third side, before | 4m51 | |
| with boost as a third side, after | 3m32 | 8m37 |
| compile | 3.6s | 17.4s |

- **The two lookup workloads search a table built outside the timed region**, a million times instead of ten million. Before, every timed iteration filled a fifty thousand entry map first. `rhit*`/`rmiss*` cost 8.2 s and 2.9 s at fifty points, against 7.7 s and 3.0 s at five before.
- **The two workloads that grow a map from empty keep five points.** Their cost integrates over every doubling, so no cycle is left. Measured at fifty anyway: `ie64` moves 1.021x cheapest to dearest against five points' 1.026x, `find64` 1.137x against 1.110x, monotonic in size. **Neither has a single step over 1.3%**, where `rmiss64` steps 32.2%, `churn64` 22.1% and `build64` 18.1% in one 1.4% size increment, at exactly the 52429 and 209716 the growth probe prints. At fifty points those two cost 4m43 to reproduce what five points said.
- **Nothing else needed anything.** `build*` and `churn*` run the full payload at every point; that is the 2m21 to 5m34.

**Two traps inside that restructuring, both of them this file's own rules.** Passing the table by pointer made the lookup loop 6.28 ns per hit against 6.05: the rng's state store may alias the map's members through that pointer, so they were reloaded per lookup. Rng and key pointer in locals, rng written back at the end, restores 6.05 (the value-walk aliasing again). The control for those four workloads reads 0.976 in one build and 1.000 in another, `-falign-functions=32` moving 0.976 to 0.997: the +-3% layout band, plainly visible in a single tight timed loop.

**What this says and does not say.** The ten boost ratios in this file taken at five points carry up to 26% sampling error on churn and 16% on `buildbig`. Header-against-header, 0.3-1.1% (2.4% on `rhitstr`) is below every other error, so `-p 5` stays default (corrected 2026-10-02: said "0.6% is below every other error"). Absolute `rhit*`/`rmiss*` times are not comparable with runs before 2026-09-12 (a tenth of the payload, no rebuild inside the measurement). **And their boost ratios are not either**: `rhit64` moved 0.798 to 0.760, `rmiss64` 0.917 to 0.868, `rmissstr` 0.931 to 0.832, `rhitstr` 0.886 to 0.874. This map's own cost is unchanged (6.05 ns per hit before, 6.00-6.10 after), so the move is on boost's side; how much is the fill leaving and how much the +-2% control spread across builds was not separated. Ratios against another revision of this header did not move. `bench_quick_overall_udm` is untouched: the new knobs are template parameters with the old defaults.

### Comparing today's runs against a stored CSV invented a 2-3% regression that does not exist

*2026-09-14 · method · while re-taking the index-structures post, `scripts/ab/maps.sh -r REV`*

The question was whether the map had moved since the post's numbers at `fab7984`. Diffing a fresh sweep against `scripts/ab/data/maps_*.csv` said `map<uint64_t, big_value>` lookups got 2-3% slower at the 32000 octave: `hit` +2.4%, `half` +3.1%, `ie` +2.9%, `miss` +2.8%, one direction, stable across five runs with 0.5-1.6% spread. Repetition made each side precise and did nothing about the gap between them.

**Both headers in one process says the opposite.** `maps.sh -r fab7984` puts any revision in its own namespace in the second-map slot, and `compare()` interleaves them epoch by epoch. Current over post-era, three rounds, below 1.00 = current faster, `big` at 32000:

| hit | miss | half | ie | churn | build |
|---|---|---|---|---|---|
| 1.005 | 1.011 | 1.001 | **0.977** | 0.993 | 0.989 |

The `ie` cell is 2.3% *faster*, not 2.9% slower (corrected 2026-10-02: said "The CSV's most confident cell is 2.3% *faster*, not 2.8% slower"). The run also found gains the CSV hid: integer `ie` **0.957-0.976** across the three octaves (the #260/#262/#268 erase work) and string `miss` **0.916-0.951** (the probe split).

**So the rule already in `CLAUDE.md` -- never compare runs from different times -- is not about drift within an afternoon.** A week-old CSV and a fresh run differ by a systematic few percent of machine state that no repetition sees. To ask "did this header move", put both headers in one process; `maps.sh -r` always could.

**What could not do it is `solo.sh -r fab7984`.** It swaps only `unordered_dense.h` into a copy of today's tree, and `huge_page_allocator.h` needs `default_segment_size_bytes` and `detail::segmented_container_for`, which did not exist on 2026-09-07. It reaches back only as far as the test suite.

### The lookup harnesses measure throughput, and a million entries is not past the cache here

*2026-09-14 · method · raised by a reader of the index-structures post, `scripts/ab/latency.cpp`*

One claim was wrong; the other was never made, so it is a missing label, not a correction.

**A million entries is L3-resident on this machine.** 64 MB of L3 in two 32 MB slices, `AB_CORE` pins to one core, so a measured process gets 32 MB. `map<uint64_t, uint64_t>` at a million entries: this map 11.0 MB of index plus 15.3 MB of values = **26.3 MB**, `boost::unordered_flat_map` 2.0 MB of metadata plus 30.0 MB of slots = **32.0 MB**, exactly on the line. Two million is the first size clearly past (52.5 and 64.0). `doc/benchmarks.md`'s "past every cache level" for the README charts was wrong at the bottom of their range.

**Every lookup workload here is reciprocal throughput, and nothing said so either way.** `lookups()` in `maps.h` draws its key from `st.rng()`, so several lookups can be in flight, as in a caller's loop. It is not one lookup's latency. Every "latency" in the post refers to the hash, the rehash loop or tail latency, never a map lookup: an omission, not an error. ns per hit, chain = each key is the value the last lookup returned:

| | 1M chain | 1M indep | 4M chain | 4M indep | 16M chain | 16M indep |
|---|---|---|---|---|---|---|
| this map | 16.63 | 12.24 | 56.62 | 38.73 | 93.33 | 45.25 |
| boost | 22.02 | 7.26 | 81.27 | 20.45 | 101.21 | 27.79 |

**The two do not rank the maps the same way, and the published one is the unflattering one.** Boost takes an independent hit **1.69x** faster at a million and loses the chain, 22.02 against 16.63. A flat map has one dependent load and more to overlap; a dense map's second load is already on the chain. Throughput understates this map, which is no reason to leave it unlabelled.

**And the trap in measuring it.** The independent loop must do the chain's work minus the dependency. The first version read a shuffled index array, 8 MB at a million entries, measured its misses and came out **slower** than the chain (udm 30.45 indep against 18.32 chain). Keys are now `mix(1..n)`, so an in-register rng reaches a present key with no memory access.

**What this does not change.** Every ratio in the post and in `doc/` compares two maps measured the same way, so none moves; only the meaning of a single quoted number changes.

### The teardown harness ran its sides in a fixed order, and the side that runs first in a pass reads 2-4% slower on the string first build; the 5-18% first-build gap at 50000 that #309 could not explain does not reproduce with the same two headers (0.4-3.0%), so the harness now rotates its sides

*2026-09-28 · #313, #309 · method · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/teardown_order.sh ... 5 7 50000`, first builds, medians of five passes*

| first build, ms | same header, fixed order: base / cand / blocksfwd | #309's pair, `7ac1e3a~1` / `7ac1e3a` | same header, rotated: base / cand / blocksfwd |
|---|---|---|---|
| clang u64 | 0.901 / 0.896 / 0.894 | 0.962 / 0.953 | 0.915 / 0.901 / 0.900 |
| clang str | **3.174** / 3.117 / 3.066 | 3.138 / 3.062 | 3.074 / 3.067 / 3.076 |
| clang owned | 3.023 / 3.008 / 3.013 | 3.043 / 3.032 | 3.010 / 3.015 / 3.006 |
| gcc u64 | 0.866 / 0.855 / 0.857 | 0.831 / 0.817 | 0.867 / 0.855 / 0.851 |
| gcc str | **3.170** / 3.068 / 3.060 | 3.117 / 3.027 | 3.070 / 3.074 / 3.067 |
| gcc owned | 2.955 / 2.974 / 2.964 | 2.943 / 2.924 | 2.958 / 2.966 / 2.966 |

With one header on all three sides, the first side of every pass (`base`) read the string build 2-4% slower under both compilers and took the outliers (3.388, 3.447). #309's two headers read 0.4-3.0% apart, where #309 read 5-18%, and the side that moved is main's (clang u64 0.96 now against 1.16 then, owned 3.04 against 3.39), not #309's. That run's machine state is not recoverable; a process left on the pinned core, which spoiled another run that week, would explain it. Rotating the side order per pass brings identical headers within 0.3% for strings and owned values and 1.9% for integers.

What this says and does not say: #309's conclusion did not rest on the first build, and its warm-build and teardown columns (1.5-3.2x) are far outside either effect. The first-position effect is 2-4% and shows only on the string first build here. Other harnesses with a fixed variant order within a pass may carry it too; not checked.

## Measuring memory, the README benchmark run, and the charts

How the harnesses measure peak memory, the README benchmark runs of 5.2.0, and the tooling that draws the charts in `doc/`. Memory has two metrics, resident bytes and bytes asked for, and they can give opposite verdicts: quote both or say which. Every README number here is one machine and one size octave from 1M, inside the cache boundary.

**Where it stands** (as of 2026-10-02)

- Peak resident memory comes from `scripts/ab/max_rss.h`: fork per measurement, reset `VmHWM` through `clear_refs`, subtract the instrument's ~128 KB floor. The residual is ~20 KB, so there is no thousand-entry memory column ([`max_rss::of` was charging every map 128 KB of its own](#max_rssof-was-charging-every-map-128-kb-of-its-own-and-the-counter-beside-it-could-not-see-aligned_alloc)).
- Bytes asked for come from `scripts/ab/count_alloc.h`, which also forks and interposes `aligned_alloc` and `posix_memalign`. Resident over asked sorts maps by family: node 0.85-1.04x, dense 1.05-1.55x, flat 1.50-1.97x in `maps.cpp` for the index-structures post (same entry). At a million `uint64_t` entries in the README run (#349) the order holds and the ranges are lower: node 1.00-1.01x, `unordered_dense` 1.14x and `f14-vector` 1.18x, flat 1.25-1.34x.
- Any benchmark that allocates needs a `volatile` sink, or clang elides the allocation (same entry).
- Against 5.2.0's `map`: `pmr` is within 3.5%, `group_big` costs up to 1.34x on the integer build, `segmented_map` builds at 0.54 and peaks at 0.58 but iterates strings 3.38x slower, huge pages build at 0.58 ([Every configuration of 5.2.0, twelve rows in the README run (#349)](#every-configuration-of-520-twelve-rows-in-the-readme-run-349-pmr-is-free-group_big-costs-16-34-on-the-integer-build-find-churn-and-memory-below-232-elements-segmented_map-builds-19x-faster-at-058x-the-peak-memory-and-iterates-strings-338x-slower-with-its-default-4-kb-segment-and-huge-pages-are-the-largest-gain-on-every-integer-timing-panel)).
- 5.2.0 and 5.0.0 are level on the README workloads except the integer build at 0.979 and the integer build + destroy at 0.964, which is one layout sample ([5.2.0 against 5.0.0 on the README workloads](#520-against-500-on-the-readme-workloads-level-within-14-everywhere-except-the-integer-build-21-faster-and-the-integer-build--destroy-36-faster-what-51-and-52-changed-is-invisible-to-this-harness)).
- Charts: no value label inside a bar ([A value label never goes inside its bar](#a-value-label-never-goes-inside-its-bar)), no `prefers-color-scheme` block in a `mapsplot.py` chart unless a surface rect switches with it; the blog charts have none, the README charts have both since 80490aa ([An SVG in an `<img>` follows the reader's colour scheme](#an-svg-in-an-img-follows-the-readers-colour-scheme-not-the-pages)). `scripts/ab/regen.sh` measures and draws the untracked sweep charts and the page, and redraws the README charts from `doc/bench_readme.csv`. The tracked `bench_readme.csv` and README SVGs come from `bench_readme.sh`, `allocated_memory.png` from `alloc_timeline.sh`.
- The hash chart's honest gain over 4.11.0 is the latency column, 10-18% ([A chart for the hash](#a-chart-for-the-hash-and-its-own-control-says-how-much-of-it-to-believe)).

**Memory measurement**

### Peak memory across every harness is now the process's resident high-water mark, and one build of the map is enough to measure it

*2026-09-13 · #277 follow-up · method · Ryzen 9 7950X, Fedora 44, clang 22.1.8, THP `madvise`, `scripts/ab/max_rss.h` used by `bench_readme.cpp` and `maps.cpp`*

Both harnesses now call one header, `scripts/ab/max_rss.h`. `maps.cpp` had counted with `mallinfo2()`, which cannot see `mmap` and read the huge page allocator's blocks as nothing; `bench_readme.cpp` had its own RSS reader. Each part of the method carries load:

- *Fork per measurement.* glibc does not hand a grown arena back, so a second fill in one process reuses resident pages. Unforked, `boost` with string keys read 187.5 B/entry against 145.1 forked, and several maps read *below* the bytes they demonstrably allocated.
- *Reset `VmHWM` through `/proc/self/clear_refs`* (CLEAR_REFS_MM_HIWATER_RSS), or the mark covers the life of the process and the key pools dominate it.
- *Subtract the resident set from just before.* The key pools are built in the parent and only read, so they are in the baseline and never copied.

**Huge pages are counted correctly, checked rather than assumed.** A 1M-entry `huge_page::map<uint64_t,uint64_t>` reports 28 MB of `AnonHugePages` in `/proc/self/smaps_rollup` against 0 on 4 KB pages, and a `VmHWM` 2 MB higher: the rounding up to a whole huge page, which a caller pays. The huge-page variants read *below* the plain map here, while counted bytes put them above: the allocator `munmap`s a superseded mapping and the kernel takes the pages back at once, where glibc keeps a freed arena block resident. The same mechanism, from the other side, is the ~⅓ that every doubling flat map carries.

**One build is enough.** The result is deterministic to the page: three runs of one cell come back byte-identical. So `bench_readme.sh` measures `rss` and `memory` in the first round only, 5-6 minutes of a full run rather than 15.

What this does not say: the ~30% a flat map carries is glibc's retention policy, which another allocator will not reproduce, so bytes-requested stays measured beside it in the CSV under `memory`. `alloc_timeline.cpp` still counts bytes, and should: it draws a timeline of individual allocations, which resident-set sampling cannot reproduce at that resolution.

**Later (2026-09-14):** the result is deterministic, but deterministically 128 KB too high, because the fork and the `/proc` reads fault in pages of their own; see [`max_rss::of` was charging every map 128 KB of its own](#max_rssof-was-charging-every-map-128-kb-of-its-own-and-the-counter-beside-it-could-not-see-aligned_alloc).

### `max_rss::of` was charging every map 128 KB of its own, and the counter beside it could not see `aligned_alloc`

*2026-09-14 · no issue (re-taking the index-structures post) · method · Ryzen 9 7950X, Fedora 44, clang 22.1.8, `scripts/ab/max_rss.h`, `scripts/ab/count_alloc.h`, `scripts/ab/maps.cpp`*

Three defects in the memory panels, found because cells at a thousand entries read 200-500 bytes per entry for a map holding 16 KB of data and disagreed by up to 1.64x between runs.

**The instrument has a floor and was not subtracting it.** `of()` returned `VmHWM - VmRSS` measured in a forked child. A child that does nothing at all does not read zero: forking, writing `/proc/self/clear_refs` and reading `/proc/self/status` fault in ~128 KB of their own. An empty closure read 128.0 KB in five repetitions, and still 128.0 KB after the parent grew by 256 MB: a constant, not drift. So [Peak memory across every harness is now the process's resident high-water mark](#peak-memory-across-every-harness-is-now-the-processs-resident-high-water-mark-and-one-build-of-the-map-is-enough-to-measure-it) is right that the result is deterministic to the page, and it was deterministically 128 KB too high. Against known work, a constant +148: 1 MB read 1172 KB, 16 MB read 16532, 64 MB read 65684. Subtracting a same-shaped empty child leaves a ~20 KB residual: 0.1% at 16 MB, the whole answer at 16 KB. **Two hypotheses were wrong before this one**, both disproven by measurement: copy-on-write faults from the parent (an empty closure would then drift, and it does not), and the child inheriting a warm arena (a 1 MB cell read 1424 KB cold and 1408 KB after the parent had freed 64 MB).

**So there is no thousand-entry memory column any more.** With the floor subtracted, the n=1000 cells get *relatively* worse (worst spread between two runs 1.217 before, 1.307 after), because the absolute error is unchanged and the subject is 16 KB. n=32000 improved 1.042 to 1.032 and n=500000 1.003 to 1.002. The method needs the subject large against the residual; the header says so now.

**Adding a counted column re-broke the resident one, which is the same fork lesson from the other side.** The counted pass filled a map in the *caller's* process, and glibc does not hand the arena back, so every later `max_rss::of` child served its allocation from already-resident freed space. Every map after the first read 2 to 20 resident bytes per entry against a true 50 to 70; emilib read 2.0. **Open (2026-10-02):** the cell for "a true 50 to 70" is not named, and the 42.0 and 48.8 below are lower; not re-checked. `count_alloc::of` forks too, for that reason alone: it counts requests, so a warm arena cannot flatter it directly.

**`aligned_alloc` and `posix_memalign` were not interposed.** ihtab asks for its whole group array with `std::aligned_alloc`, so it reported **8.3 bytes per entry against a true 35.3**: element array counted, index invisible. `free` had always charged those blocks through `malloc_usable_size`, so the counter leaked in one direction only, which gives a plausible wrong number rather than an obvious one. Same failure as counting only `operator new` and missing emilib's direct `malloc` (see [Seventeen maps in their own default configurations, at a million entries](#seventeen-maps-in-their-own-default-configurations-at-a-million-entries-the-dense-layout-wins-iteration-by-55x-to-110x-and-the-string-build-and-destroy-by-21x-to-26x-and-loses-the-integer-find-and-churn-to-boost-by-37-and-64)).

**And `measure_memory` charged the benchmark's own key vectors to the map.** The churn closure copied `present` and `spare` *inside* the measured region: two vectors of n keys, 16 bytes an entry for an integer key, added to every map's churned column in both metrics. The comment claimed they were copied before the fork. Hoisted out, the asked column goes flat across a turnover for every tombstone-free map (udm 31.8 to 31.8, boost 28.5 to 28.5) and ihtab doubles 35.3 to 70.6: the property the panel exists to show, which the inflated numbers had buried.

**The check that the fixed counter is right**: it reproduces the `mallinfo2` figures this file replaced, to the decimal, on all eighteen maps at the 32000 octave: absl 26.4/30.3, boost 28.5, udm 31.8, emhash8 37.1, std 44.4, boost-node 47.4. Two independent accountings agreeing to 0.1 is what makes either believable.

**What the two metrics say that neither says alone.** Resident over asked sorts the field by family more cleanly than anything else measured for the post:

| family | 8 byte value | 64 byte value |
|---|---|---|
| node | 0.85-1.04x | 0.90-1.00 |
| dense | 1.05-1.55x | 1.41-1.63 |
| flat | 1.50-1.97x | 1.86-1.97 |

**Later (2026-10-01):** the ranges depend on the size. At a million `uint64_t` entries in the README run (`doc/bench_readme.csv`, #349) the family order holds and every range is lower: node 1.00-1.01x, `unordered_dense` 1.14x and `f14-vector` 1.18x, flat 1.25-1.34x.

A doubling flat map supersedes its whole slot array at `sizeof(value_type)` a slot and glibc keeps it resident; a dense map supersedes a 5.5 byte index. So abseil asks for 26.4 bytes an entry and occupies 48.8, where `unordered_dense` asks for 31.8 and occupies 42.0. The verdict inverts, and either column alone answers a different question than a reader thinks.

**And clang elides the obvious benchmark.** Both diagnostics above first read "no effect at all": `auto v = std::vector<char>(n); memset(v.data(), 1, n); if (v[0] == 0) ...` folds to a constant and the allocation disappears. A touched 16 MB measured as an empty closure, and a 1 MB `malloc`/`free` counted zero bytes. Anything that measures allocation needs a `volatile` sink or an accumulation the compiler cannot see through. This caused two false starts.

**README benchmark runs**

### Every configuration of 5.2.0, twelve rows in the README run (#349): `pmr` is free, `group_big` costs 16-34% on the integer build, find, churn and memory below 2^32 elements, `segmented_map` builds 1.9x faster at 0.58x the peak memory and iterates strings 3.38x slower with its default 4 KB segment, and huge pages are the largest gain on every integer timing panel

*2026-10-01/02 · #349, #350 · info · Ryzen 9 7950X, clang 22.1.8, `scripts/ab/bench_readme.sh -u v5.2.0`, 25 maps x 2 key types, 1M base octave, 3 rounds, median, 23:47 to 02:30, `AB_CORE=2`*

`-u REV` is new for this run: it builds every `unordered_dense` row from REV's three headers (checked byte-identical to the tag; `-H` shows all three resolved from the copy) and writes the version to `doc/bench_readme.ref`, which `mapsplot.py` uses as the reference label. The twelve rows are values {`std::vector`, `segmented_map`} x index {`group`, `group_big`} x allocator {`std::allocator`, `pmr::` on the default resource, `huge_page_allocator`}. `pmr` and huge pages share the allocator slot. Huge page segmented rows use 16 MB segments, the others the 4096 byte default. Ratio to 5.2.0's `map`, `uint64_t` / `std::string`:

| configuration | build + destroy | find | churn | iterate | peak rss |
|---|---|---|---|---|---|
| `pmr` | 1.03 / 1.02 | 1.00 / 0.99 | 1.01 / 1.00 | 0.99 / 1.03 | 1.00 / 1.00 |
| `group_big` | 1.34 / 1.04 | 1.18 / 1.03 | 1.16 / 1.05 | 1.01 / 1.03 | 1.21 / 1.07 |
| `group_big`, `pmr` | 1.36 / 1.05 | 1.17 / 1.03 | 1.16 / 1.05 | 1.02 / 1.02 | 1.21 / 1.07 |
| segmented | 0.54 / 0.66 | 1.04 / 1.00 | 1.11 / 1.03 | 1.51 / 3.38 | 0.58 / 0.90 |
| segmented, `pmr` | 0.54 / 0.66 | 1.05 / 1.00 | 1.11 / 1.03 | 1.51 / 3.39 | 0.58 / 0.90 |
| segmented, `group_big` | 0.74 / 0.70 | 1.20 / 1.04 | 1.46 / 1.08 | 1.59 / 3.40 | 0.78 / 0.97 |
| segmented, `group_big`, `pmr` | 0.73 / 0.70 | 1.21 / 1.04 | 1.46 / 1.08 | 1.58 / 3.47 | 0.79 / 0.97 |
| huge | 0.58 / 0.70 | 0.91 / 0.97 | 0.80 / 0.96 | 0.84 / 0.97 | 0.78 / 0.98 |
| huge, `group_big` | 0.66 / 0.77 | 1.08 / 0.99 | 0.99 / 1.00 | 0.91 / 0.97 | 0.99 / 1.04 |
| segmented 16 MB, huge | 0.50 / 0.50 | 0.95 / 0.97 | 0.88 / 0.96 | 1.35 / 1.00 | 0.66 / 0.91 |
| segmented 16 MB, huge, `group_big` | 0.57 / 0.53 | 1.10 / 0.99 | 1.19 / 1.00 | 1.46 / 1.02 | 0.86 / 0.98 |

5.2.0's `map` itself:

| key | build + destroy (ns) | find (ns) | churn (ns) | iterate (ns) | peak (B/entry) |
|---|---|---|---|---|---|
| `uint64_t` | 23.24 | 29.70 | 88.33 | 0.20 | 48.6 |
| `std::string` | 92.72 | 126.00 | 418.96 | 0.67 | 122.8 |

Every `pmr` row is within 3.5% of its `std::allocator` twin. `group_big` against `group` is the 152 byte block against 88; with huge pages under both the integer find ratio stays (1.18 plain, 1.18 on huge pages), so it is not the TLB alone. The segmented string iteration (3.38) against 16 MB segments on huge pages (1.00) changes two things at once; #350 separates them.

The same run with `main`'s headers (ee1adaf plus nothing in `include/`), 19:58 to 22:41 the same evening, is recorded and not compared, being a run from a different time. There, segmented `uint64_t` read find 1.15 and churn 1.00 against its own `map`, where 5.2.0's reads 1.04 and 1.11; `group_big` 1.15 / 1.13; huge 0.92 / 0.74. If the segmented swap between find and churn is real, it comes from one of the nine header commits since v5.2.0, and only a paired run (`maps.sh -r`) can say.

The library rows in this run replace the 2026-09 CSV wholesale, which was measured against an older header and so is not compared either: integer find and churn against boost 0.73 / 0.61 then, 0.73 / 0.59 now; `std::unordered_map` iterate 110x then, 112x now. folly and emhash had to be re-cloned at the pinned commits (`~/gra/folly/hightent`, `~/gra/emhash/fastsong`) with a hand-written `folly-config.h`. Without them, `bench_readme.sh` drops their rows with only a `missing:` line on stderr. Side readings with unchanged source: `spill_trap.cpp` read 54 cycles for the no-trap loop against the 48 recorded; `latency.cpp` read 17.0 / 12.0 ns for `unordered_dense` and 22.4 / 7.4 for boost, against 16.6 / 12.2 and 22.0 / 7.3.

`doc/allocated_memory.png` was redrawn from v5.2.0 the same night (`alloc_timeline.sh -u v5.2.0`, which got the same option): byte counts identical to the previous chart to 0.1 MB, fill times 0.38 s boost, 0.39 s `segmented_map`, 0.44 s abseil, 0.49 s `map`.

What this says and does not say: one machine, one size octave from 1M, inside the cache boundary. Not how `group_big` behaves past 2^32 elements, the only place it is needed, nor what a `pmr` resource other than `new_delete_resource` does (a monotonic resource would keep every superseded block).

**Correction (2026-10-02):** the heading said "`group_big` costs 16-34% on integer keys below 2^32 elements" and "huge pages are the largest gain on every integer panel". The table says otherwise in two cells: `group_big` iterates integers at 1.01, and on integer peak memory plain `segmented_map` (0.58) beats every huge page row (0.66 at best). Huge pages win the four integer timing panels.

### 5.2.0 against 5.0.0 on the README workloads: level within 1.4% everywhere except the integer build, 2.1% faster, and the integer build + destroy, 3.6% faster; what 5.1 and 5.2 changed is invisible to this harness

*2026-10-02 · after #349; #321, #310 · info · Ryzen 9 7950X, clang 22.1.8, `bench_readme.sh -u v5.2.0` with its renamed second header pointed at v5.0.0 instead of v4.11.0 (a temporary copy), rows `udm` and that one only, 1M base octave, 3 rounds interleaved, `AB_CORE=2`, 04:18 to 04:29*

Median ns per operation (bytes per entry for the last two rows), 5.2.0 / 5.0.0:

| | `uint64_t` | ratio | `std::string` | ratio |
|---|---|---|---|---|
| build | 22.07 / 22.54 | 0.979 | 77.19 / 78.18 | 0.987 |
| build + destroy | 23.26 / 24.13 | 0.964 | 92.06 / 92.90 | 0.991 |
| find | 29.70 / 29.79 | 0.997 | 124.97 / 125.19 | 0.998 |
| churn | 88.98 / 88.52 | 1.005 | 418.43 / 419.57 | 0.997 |
| iterate | 0.192 / 0.193 | 0.996 | 0.656 / 0.665 | 0.986 |
| peak rss | 48.55 / 48.59 | 0.999 | 122.73 / 122.85 | 0.999 |
| bytes asked | 42.43 / 42.43 | 1.000 | 137.56 / 137.56 | 1.000 |

The integer build + destroy is the only cell whose rounds do not overlap (5.2.0 22.92-23.44, 5.0.0 24.08-24.13), and 3.6% is at the edge of the ±3% code layout band: one layout sample, not yet a result. Everything else is inside 2.1%, and only the integer build (0.979) is above 1.4% (corrected 2026-10-02: said "Everything else is inside run-to-run drift"; the heading said "except the integer build, 3.6% faster", which is the build + destroy row). That is what the header changes between the two tags predict (`git log v5.0.0..v5.2.0 -- include/`). #321 moved the home-group hit and placement of `try_emplace` in front of any call, which changes what an insert inlines into its *caller*; a harness whose loop holds nothing live across the call cannot see that. The caller corpus ("stored and reloaded") is where it shows: 5.2.0 read 3.3x / 3.6x behind the best shape there, which #310 fixed after the release. The segmented teardown order is in 5.1.0, but these are plain `map` rows. The memory layout did not change, to the byte.

**Charts and docs tooling**

### A fifth series, and it is a control

*2026-09-06 · no issue · kept · `plot.py`, `scripts/ab/dashboard.py`*

The charts gained `boost::unordered_flat_map` with the hash it ships with, beside the one given `unordered_dense`'s hash. Every other alternative gets `unordered_dense`'s hash, so what differs between them is the index. This series says what the hash choice is worth on its own, and it is what a caller gets by passing no third template argument. It is drawn dashed in boost's colour, hatched on the bar charts, because no fifth hue clears the colourblind floor against the other four (the best candidate is 2.7 apart from the blue under deuteranopia, against a floor of 8), and colour for the map, style for the hash is the truer encoding anyway. `robin hood (main)` is relabelled **4.11.0**, which is what it is.

The two value-size charts are **bars**, the size-axis ones lines: six value sizes are categories, and a line between 32 and 48 bytes interpolates something nobody measured. `--bars` in both `plot.py` and the dashboard: grouped, anchored at zero, 2px of surface between neighbours. Build and iteration are separate charts, since they differ by two orders of magnitude.

### The charts are also a page you can interrogate

*2026-09-06 · no issue · kept · `scripts/ab/dashboard.py`*

`scripts/ab/dashboard.py` writes `doc/charts.html` from the CSVs in `doc/`: every chart, a legend that shows and hides a map across all of them, a y axis that rescales to what is left, a crosshair that reads exact values and each map's multiple of the fastest, the hidden set in the URL so a view is a link, and light and dark from the validated palette `plot.py` uses. Self-contained, stdlib only, no network. It does not replace the SVGs, because GitHub strips scripts out of an SVG. The READMEs embed the SVGs; the page is what you open when a line looks wrong.

### The four charts worth keeping, and the two axes that had none

*2026-09-06 · no issue · info · `scripts/ab/valuesize.cpp`, `scripts/ab/memory.cpp`*

The four graphs that decide a map: find-that-hits against size, churn against size, build-and-iterate against value size, memory against size (`scripts/ab/README.md` says why each and why not the alternatives). Two axes were unmeasured and needed new tools: `scripts/ab/valuesize.cpp` (build and iteration against `sizeof(mapped_type)`) and `scripts/ab/memory.cpp` (bytes per entry, steady and peak, from a counting allocator).

**Value size**: boost against `unordered_dense` is 1.37x on a build at an 8 byte value and 2.10x at 64, and 10.5x on iteration at 8 bytes, falling to 2.3x at 64. This updates the older note in [A mapped value of eight bytes hides what a dense map is for](#a-mapped-value-of-eight-bytes-hides-what-a-dense-map-is-for), which had boost *ahead* at 16 bytes and behind at 64. The crossover moved off the chart when the rehash store-to-load fix made building 1.80x faster, so `unordered_dense` now leads at every value size measured. The axis stops at 64 bytes: 200000 entries of a 64 byte value is 14 MB and still in L3, 128 bytes is 27 MB and is not, and past that every line bends upward together. **Open (2026-10-02):** a pinned process gets a 32 MB L3 slice (see "not past the cache"), so 27 MB is inside it and the bend past 64 bytes may not be L3; not re-measured.

**Memory**: the axis that matters is the mapped-value size, not the table size. Memory per entry barely moves with the entry count; the size chart is sixteen identical octaves. The ratio to a flat map depends on the value. At a million entries, `unordered_dense` against boost:

| value | steady | growth peak |
|---|---|---|
| 8 bytes | **1.19x** | 1.48x |
| 64 bytes | 1.65x | 1.81x |
| 256 bytes | 1.81x | 1.86x |

**Later (2026-09-07):** these are single-size points on the load-factor sawtooth. Averaged over an octave the 64 byte value is a wash (118.3 against 118.5 bytes per entry) and boost is ahead at 8 bytes (32.6 against 29.2), see [Those memory figures are a point on the sawtooth](#those-memory-figures-are-a-point-on-the-sawtooth-and-the-octave-says-something-else).

A flat map pays for its empty slots at the full width of the value, where a dense one pays 5.5 bytes of index (corrected 2026-10-02: said "four bytes"; the group index of 2026-09-05 is 88 bytes per 16 slots). The first version charted the 8 byte case alone, which shows the dense design at its least impressive. Absolute, at a power of two: 27 bytes per entry against 32 for main (here the robin hood index, now labelled 4.11.0), 4.8.1 and boost; peak 32.5 against boost's 48 and main's 40. 4.8.1's peak is 32, *lower* than main's 40. That is the price of `8d0e17e`: building the new bucket array before releasing the old one makes growth exception-safe, and it costs a taller transient.

### A chart for the hash, and its own control says how much of it to believe

*2026-09-07 · no issue · info · `doc/hash_vs_length.svg`, `scripts/ab/hash.cpp`*

The scored suite's one hash workload reports one number over a mix of lengths. That is the right summary, but it hides the shape: a hash dispatches on length, and its cost is a staircase. Two panels, and the pair is the point. **Throughput** (independent keys, as many in flight as the machine has multipliers) is what a hashing loop pays and what hash benchmarks report. **Latency** (a byte of each answer fed into the next key, so nothing overlaps) is what a *map* pays, since the hash's result is the address of the group to probe. That distinction decided AES-NI: a quarter faster on the throughput panel, half again slower on the latency panel.

Geomean per length range, 4.11.0's time over `unordered_dense`'s, above 1.00 meaning `unordered_dense` is faster:

| bytes | throughput | latency | what the code does there |
|---|---|---|---|
| 1-16 | 1.09 | **1.00** | identical source: the short path was not touched |
| 17-48 | 1.21 | 1.18 | independent blocks, two or three multiplies |
| 49-96 | 1.11 | 1.10 | four to six |
| 97-144 | 0.99 | 1.17 | seven to nine |
| 145-256 | 0.99 | 0.99 | identical source: the chained lanes were not touched |

**The two identical-source rows are the control, and they disagree with each other.** The 145-256 row reads 0.99/0.99, which is what identical code should read. The 1-16 row reads 1.09 in throughput and 1.00 in latency: the same instructions, 9% apart, because restructuring what sits *after* the short path's early return moved the code around it. So the throughput panel carries about 9% of layout and the latency panel does not. The honest reading of the 17-144 range is the latency column: **10-18%, clean, with a control at 1.00 either side of it**. The throughput gain is real too, but read 1.21 as "up to 1.21, of which up to 9% is not the algorithm".

The chart shows two more things the score cannot. `boost::hash<std::string>` is **2.0-2.2x slower than this hash in throughput and 1.1-1.7x in latency**, growing with length; the mix number (8-31%) is dominated by short keys. And 4.8.1's hash is *faster than this one in throughput* over 17-144 (1.06) while being **1.24x slower in latency**. That is the clearest statement of what July's work traded and why the mix could not see it.

The SVG is capped at 256 bytes, on a linear axis (`--xlin`, `--xmax`, both new in `plot.py`). A table size is exponential and belongs on log2; a key length is not, and past 256 every line is straight. The CSV keeps the full range to 1024. There is one length per point, so the length dispatch is perfectly predicted here, where the scored keys cost the shipped hash 0.50 branch misses per hash (corrected 2026-10-02: said "0.31", which is the old wyhash on the old 4-200 byte keys; 0.50 is from [Four attempts at tuning the hash for latency](#four-attempts-at-tuning-the-hash-for-latency-all-worthless-in-the-map-and-the-microbenchmark-that-said-otherwise-was-wrong)). `hashstr` is the number that includes the dispatch; this chart is the shape.

### An SVG in an `<img>` follows the reader's colour scheme, not the page's

*2026-09-08 · no issue · kept · `mapsplot.py`, `plot.py`*

The blog charts carried a `@media (prefers-color-scheme: dark)` block. The page they sit on is a blog whose background is hard-coded `#FFFFFF`. So a reader whose OS is set to dark got the title at `#f9fafb` on white, **1.05:1**, and every label at `#9ca3af`, 2.54:1. It is invisible, and no light-mode contrast check can see it, which is how it survived the audit in [A value label never goes inside its bar](#a-value-label-never-goes-inside-its-bar).

Verified both ways with `google-chrome --headless --blink-settings=preferredColorScheme=0|1` and a one-line HTML that embeds the SVG; that is the cheap way to test this class of thing. The block is gone from `mapsplot.py`. Put one back only alongside a surface rect that switches with it, or a page that does. `doc/`'s charts from `plot.py` keep theirs, because GitHub's own dark mode darkens the page underneath them.

**Later (2026-09-12):** `mapsplot.py` writes the block again for the README charts (80490aa), together with a `.surface` rect that switches with it, as this entry allows; the blog charts still have none, see [Seventeen maps in their own default configurations, at a million entries](#seventeen-maps-in-their-own-default-configurations-at-a-million-entries-the-dense-layout-wins-iteration-by-55x-to-110x-and-the-string-build-and-destroy-by-21x-to-26x-and-loses-the-integer-find-and-churn-to-boost-by-37-and-64).

### A value label never goes inside its bar

*2026-09-08 · no issue · kept · `mapsplot.py`*

The panel now reserves the widest label's width and the scale shrinks to fit, which costs the longest bar 23% of its length and puts every label in the same colour in the same place. Before, `mapsplot.py` drew the value in white inside the bar whenever it would have overflowed the panel: 6 to 11 labels per chart. White on a bar drawn at `opacity="0.8"` composites to 3.0-4.1:1 against the page, under the 4.5 an 11px label needs, worst on the dense green. No ink is dark enough for the inside of a mid-tone bar either (#1f2937 on the full-strength green is 3.4), so the fix is room rather than colour. The sawtooth's direct labels were tinted with their series colour and had the same problem. They now carry a swatch of the line beside them and use the ordinary label ink.

**Do not fix this class of thing by darkening the palette**: every green that clears 4.5:1 as text drops the palette's tritanopia separation from 6.5 to 4.3. The colour is a *mark*, which needs 3:1 and has it.

### `scripts/ab/regen.sh` rebuilds everything in `doc/`

*no date (vendored nanobench since 2026-09-08) · martinus/nanobench#189 · info · `scripts/ab/regen.sh`*

`regen.sh` builds the baseline headers, the four tools, the measurements, the SVGs and the page, and is the only way to get them, since none of its output is tracked. `--redraw` does the drawing half alone in about two seconds (corrected 2026-10-02: said "a fifth of a second"; the script has said ~2 seconds since its first commit) and reproduces every SVG byte for byte from unchanged CSVs: use it after touching `plot.py` or `dashboard.py`. `--quick` runs the whole pipeline coarsely in ten minutes, which finds a tool that no longer compiles without spending four hours.

**Later (2026-09-12):** since 80490aa a full run takes ~10 hours and `--quick` ~20 minutes (the script's header), and `--redraw` also redraws the tracked README charts from `doc/bench_readme.csv`. That CSV, its four SVGs and `allocated_memory.png` are tracked and come from `bench_readme.sh` and `alloc_timeline.sh`, which `regen.sh` does not run, see [Seventeen maps in their own default configurations, at a million entries](#seventeen-maps-in-their-own-default-configurations-at-a-million-entries-the-dense-layout-wins-iteration-by-55x-to-110x-and-the-string-build-and-destroy-by-21x-to-26x-and-loses-the-integer-find-and-churn-to-boost-by-37-and-64).

The sweep asks nanobench for a precision instead of naming a round count, which needs `targetIntervalWidth()` and `render(CompareResult)`. That landed upstream as martinus/nanobench#189 and has been in the vendored copy since 2026-09-08, so `NANOBENCH_INCLUDE` no longer has to point anywhere. The guard in `regen.sh` stays, because that variable can still name something older.

## The index layout: group size, counters, block layout, load factor, probing

This section covers the shape of the group index: slots per group, counter width and which counter a probe reads, the memory layout of fingerprints, counters and value indices, the probe sequence, the growth factor and the maximum load factor. The shipped design (16 slots, eight one-byte counters, one merged 88 byte block, triangular probing, growth 2, maximum load 0.8) is a measured local optimum on each axis. Most alternatives lose because they add work to every lookup, or because they save L1 fills that were never on the critical path.

**Where it stands** (as of 2026-10-02)

- Sixteen slots stays: eleven nets below 1.00, twenty-four reads 0.85 to 1.00 everywhere (it dilutes the eight counter classes), twelve 0.9888. [Eleven slots and twenty-four slots](#eleven-slots-and-twenty-four-slots-and-why-sixteen-is-where-it-stops)
- Eight one-byte counters per group is the top of the counter axis: one counter 0.959, 16 nibbles 0.986, 32 two-bit 0.988. [The width of the overflow counter](#the-width-of-the-overflow-counter-all-four-divisions-of-a-groups-eight-counter-bytes-measured-against-the-designs-eight-one-byte-counters)
- An exact in-home counter is worth 2-3% in cache only; ~80% of what the counter misses is siblings. [And the fifth point on that axis](#and-the-fifth-point-on-that-axis-an-exact-counter-worth-2-3). Per-step counter choice is a no-op or noise: [Which counter a probe consults at each step of its sequence](#which-counter-a-probe-consults-at-each-step-of-its-sequence)
- The merged 88 byte block is kept (7% fewer lookup instructions, 28% fewer dTLB misses at 4M): [Two optimizations the charts point at](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet). Split arrays tie: [Fingerprints and counters in two arrays instead of ...](#fingerprints-and-counters-in-two-arrays-instead-of-one-24-byte-group). Line-aligned indices 0.993, second fingerprint 0.975, 16-bit indices 0.986: [Three layouts borrowed from other maps, all lost](#three-layouts-borrowed-from-other-maps-all-lost-line-aligned-value-indices-a-second-fingerprint-in-the-index-word-a-16-bit-index)
- Triangular probing stays (#355, decided 2026-10-02): a step from the key takes 7-18% off a churned miss, all of it in the first step, and every way of computing it keeps a third value live across a walk whose start is on the integer hit path, +1.8-10% of the score's find instructions; peeling the home group off to avoid that loses 14-18% of a churned lookup under clang. [Seven probe sequences](#seven-probe-sequences-and-five-ways-of-computing-their-step-against-the-triangular-one-a-key-dependent-step-shortens-a-churned-miss-and-every-way-of-computing-it-costs-the-walk-a-third-live-value-that-no-source-shape-hides), following [the one-header re-measurement](#double-hashing-with-the-step-taken-from-the-fingerprint-re-measured-one-header-per-binary-after-the-churn-harness-was-fixed-churned-misses-107-118x-faster-the-score-level-a-fresh-integer-hit-up-to-5-slower-under-clang); a per-table seed is free on lookups, 3.5% of a build, macro material: [Three ideas the eighteen-map comparison suggested](#three-ideas-the-eighteen-map-comparison-suggested-all-measured-none-kept)
- The sliding window wins 1-5% of a hit, nothing on a miss against counters, loses 24% of churn at 1M and forecloses counters and merged block: [The sliding window, built and measured rather than simulated](#the-sliding-window-built-and-measured-rather-than-simulated), [A dense map on flat_wmap's structure, measured against the shipped one](#a-dense-map-on-flat_wmaps-structure-measured-against-the-shipped-one-and-the-comparison-is-not-what-it-looks-like)
- A narrower value index or tiny pointers: at most ~9% of memory, no speed (#229), closed: [Tiny pointers, and the bound insertion order puts ...](#tiny-pointers-and-the-bound-insertion-order-puts-on-the-value-index). Not zeroing the index: 1.7% and UB, reverted: [Not zeroing the value index](#not-zeroing-the-value-index)
- Growth below 2 for the value vector: a knob, not the default: [A growth factor below 2](#a-growth-factor-below-2-and-the-premise-that-suggested-it-was-wrong)
- Maximum load factor stays 0.8 (#306); in cache only, past the cache not swept: [The default maximum load factor swept from 0.75 to 0.9](#the-default-maximum-load-factor-swept-from-075-to-09-every-step-above-08-costs-both-compilers-the-same-and-the-one-step-below-buys-08-for-67-more-index)

### Fingerprints and counters in two arrays instead of one 24 byte group

*2026-09-05 · rejected · paired on the nine index workloads, plus a 20M entry table*

A tie in cache and out of it; not kept, since one array is simpler than two. The layout sweep in `martinus/ai#3` kept fingerprints and counters together in all eleven of its layouts, so the split was the one form never measured. The case for it: sixteen fingerprints are a quarter of a cache line, so four groups fit a line exactly and none straddles, where a 24 byte group straddles one time in four. The case against: a miss then needs a second line for its counter.

Measured paired on the nine workloads that touch the index, with the unchanged layout as a control: every ratio is within noise (`rhit64` 1.00, `rmiss64` 1.01, `find64` 0.99, `churn64` 1.00, `ie64` 1.02, control 1.00 throughout). On a 20M entry table, whose index is 37 MB and lives in DRAM: 57.1 against 57.0 ns per hit and 34.3 against 34.1 per miss.

**Open (2026-10-02):** the 37 MB matches no part of the index at 20M entries (2^21 groups: 50 MB of fingerprints and counters, 134 MB of value indices); either the size or the entry count is wrong. Not re-measured; the tie does not depend on it.

Why the split costs nothing: the straddle is free because the second line is the adjacent one, and the spatial prefetcher brings it in with the first. The counter line is free because its address depends only on the group, so its load issues beside the fingerprint load rather than after it. The same reasoning says the padded 32 byte group and the 16-fingerprint-plus-8-counter split are the same question, already answered.

### Which counter a probe consults at each step of its sequence

*2026-09-05 · rejected · compile-time switch, paired on the six workloads that can see it*

Neither alternative is kept. The design reads the counter of the fingerprint's class (its low three bits) at every group. The false continues of a churned miss come largely from *siblings*: entries with the same home group, which walk the same sequence. A displaced sibling of the same class carries the miss along its whole displacement. 17.5% of churned misses continue at step 0, and about half of those continue again at step 1, far above the 1/8 a fresh class check would give. Two alternatives:

- **Class plus distance, `(fp + d) & 7`**: exactly a no-op, as the arithmetic says. The sibling and the miss add the same d at every step, so if they agree at step 0 they agree everywhere. Churned miss 1.263 groups against 1.262.
- **Three fresh hash bits per step** (bits above the fingerprint, rotated by three per group): this does break the lockstep. The churned miss drops from 1.262 to 1.238 groups, with the step-0 rate unchanged by construction. But that is 2% of a miss's probe work in the tail, on a path 17.5% of churned misses reach. Paired: `rmiss64` 1.00, `churn64` 0.99, `find64` 0.99, `churnbig` 0.98, `findbig` 1.03, `rmissstr` 0.98, refactor control 1.00 everywhere, so noise. The saving is real and smaller than the cost of the register the rotating word occupies.

What would move a miss is the step-0 decision, which is the counter's class count (see [The width of the overflow counter](#the-width-of-the-overflow-counter-all-four-divisions-of-a-groups-eight-counter-bytes-measured-against-the-designs-eight-one-byte-counters)).

### The width of the overflow counter, all four divisions of a group's eight counter bytes measured against the design's eight one-byte counters

*2026-09-05 · kept (the design's eight one-byte counters) · prompted by asking whether folly's single per-group counter would help*

The design already sits at the top of this axis, and the two directions off it lose for opposite reasons. Fresh and after 200 turnovers at load 0.76, the share of misses that continue past their home group, and the score:

| counters per group | fresh miss | churned miss | misses continuing (churned) | saturated at 200 turnovers | score |
|---|---|---|---|---|---|
| 1, F14 style (class ignored) | 1.21 | 2.79 | 60% | 0 | **0.959** |
| 8 x 1 byte (the design) | 1.06 | 1.26 | 17.5% | 0 | 1.000 |
| 16 x nibble | 1.03 | 1.13 | 9.7% | 0 | 0.986 |
| 32 x 2 bit | 1.02 | 1.15 rising | rising | 36063 | 0.988 |

**Folly's one counter is the worst of the four**, not the best. It does not know the fingerprint class, so *any* overflow past a group makes every later miss into that group continue. In a churned table most groups have seen an overflow, so 60% of misses go on. Score 0.959, churn 0.83. The idea that a shared counter saturates faster and so helps aims at the wrong thing: saturation never stopped a miss, the `delta == m_group_mask` bound does. Once saturated, a counter is *worse*, because it never comes back down. That sinks the 2 bit counter: it filters best of all when fresh (1.3% continue), but its max of 3 is reached constantly under churn, and a saturated counter lengthens every later miss for the life of the array.

**The nibble filters better and still loses.** Half as many fingerprints per counter, so 9.7% of churned misses continue against the design's 17.5%, at no memory cost, and its max of 15 was never reached even after 200 turnovers. But a sub-byte counter is a load-mask-compare on the read and a read-modify-write on the increment. That is paid on *every* lookup, while the continuation it saves was already rare (4% fresh). So `find64` 0.908, `rhit64` 0.940, `findbig` 0.923.

A filter only pays where nothing cheaper filtered first. Here the group's own fingerprint compare already did most of the work, so a finer counter refines a decision that is nearly always already made. A byte per class is the point where the counter is a single aligned load and still per-class.

### Two optimizations the charts point at, one measured and one not yet

*2026-09-06 · kept (merged block); huge pages: opt-in, partly retracted 2026-09-12 · one map per binary, paired runs under clang and gcc*

**Huge pages are worth 22% of a large lookup and nothing asks for them.** The charts' worst regime for `unordered_dense` is past L3. At 800000 entries and all hits, it takes **1.48 dTLB misses and 7.03 L1 misses per lookup against boost's 0.89 and 5.15**. It executes only 14% more instructions (95.3 against 83.2) for 33% more cycles. So the gap is memory-side, and a third of it is address translation: this map touches three regions per lookup (group metadata, value index, values), a flat map touches one. `/sys/kernel/mm/transparent_hugepage/enabled` is `madvise` on this machine, a common default, and neither the map nor the benchmark ever madvises, so everything runs on 4 KB pages.

Both maps got an allocator that `mmap`s 2 MB-aligned and `madvise(MADV_HUGEPAGE)`s. ns per hit:

| entries | this map | boost |
|---|---|---|
| 800000 | 17.10 -> 13.32 | 9.75 -> 7.58 |
| 200000 | 7.18 -> 7.10 | 5.47 -> 5.29 |

Both gain about 22% at 800000 and nothing at 200000. So it is free speed in the regime the score cannot see (200000 entries is the largest table the score builds), and it does not change the ranking, since it helps both equally. Worth an opt-in allocator and documentation; it does not close the gap to boost.

**Correction (2026-09-12):** Retracted in part. "the regime the score cannot see" was wrong. The 200000 figure above is hits against a warm map, the one shape that is throughput-bound and overlaps its translations. The score itself moves 2.6% under gcc and 3.6% under clang on 2 MB pages, and churn and the 50% find move 5-8%: see [The score runs on 4 KB pages and ...](#the-score-runs-on-4-kb-pages-and-pays-16-billion-l1-dtlb-misses-for-it).

**Merging the group metadata with its own value indices: measured, and kept.**

**Correction (2026-09-06):** this entry first said merging had never been tried. That was wrong: the split's own comment in the header recorded trying it and finding the split "10% faster on a build for the same lookups". The claim came from reading `CLAUDE.md` and not the header. So this is a re-test, with the same shape as the back-pointer case: the earlier verdict came from a regime that does not cover where the cost is now.

The layout: one 88 byte block per group, 16 fingerprints, 8 counters, 16 value indices, `struct block : Group` so every existing use of the metadata reads unchanged. No padding, so it is the same bytes the two arrays took, in one allocation rather than two. Memory is unchanged to the byte: 27.0 per entry steady and 32.5 at the growth peak.

Against `HEAD`, two independent paired runs under clang and one under gcc: score **1.018, 1.022 and 1.015**, find **1.045, 1.053 and 1.044** as a group, `rhit64` 7.1-7.5%, `findbig` 6.2%. Builds, churn and insert-erase are neutral (`build64` 100.0% and 101.2% under clang, build 0.999 under gcc, intervals straddling parity). So the build advantage the split was kept for is gone, most likely taken by the rehash's store-to-load fix, which changed where a build spends its time.

The mechanism shows in the counters, which is what makes it believable. One map per binary, all-hits lookups, split against merged:

| | 200000 | 800000 | 4M |
|---|---|---|---|
| instructions per lookup | 66.9 -> 62.0 (**7% fewer**) | | |
| L1 misses | **12-14% fewer** | | |
| dTLB misses per lookup | | | 5.30 -> 3.79 (**28% fewer**) |
| cycles per lookup | 46.6 -> 42.5 | 96.1 -> 88.6 | 470.8 -> 457.5 |

Fewer instructions because the index sits at a fixed offset from the group rather than at a second address to compute. Fewer dTLB misses because a lookup touches two regions rather than three.

The control `hashstr` never touches the map and read 0.923 in one run and 1.126 in the other. That is the size of pure code-layout luck in this binary, and it is why this result rests on the counters rather than the score.

Three `pmr` tests failed, all counting allocations: a table is now the values plus one index array rather than two. They asserted 5, 3, 3, 6 and 2 as literals. They now derive from `index_t::array_count`, which is what that constant is for and what `lazy_bucket_allocation.cpp` already did.

### Not zeroing the value index

*2026-09-06 · rejected (reverted) · two binaries, alternated, four rounds; from a code review that put it at ~7% of a build*

Worth **1.7%** of a large build, not 7%, and reverted as undefined behaviour. `std::vector::resize()` value-initialises, so growing the index writes 64 bytes of zeros per group that nothing reads. A slot's index is written when an entry is placed there, and read only for a slot whose fingerprint already says it is occupied. An allocator adaptor whose no-argument `construct()` default-initialises removes the writes. For the group array beside it the zeroing is load-bearing and stays, since a zero fingerprint is what empty means.

Measured properly: 42.2 ms to 41.5 on a two million element build, the same direction every round, and nothing on the score, whose builds are 200000 elements and zero about 2 MB instead of 32.

**Correction:** the first measurement said 6.8% and was wrong. It compared runs made minutes apart, the mistake the benchmarking section of this file exists to prevent.

Reverted for a reason that outranks the number: `assign()` copies the whole index array, so with default-initialised entries the copy constructor reads indeterminate `std::uint32_t` values. That works everywhere and is undefined behaviour anyway, and a library header should not have it in a copy constructor to buy 1.7% of a large build. Copying only the occupied slots would avoid it and costs more than it saves. All 34 CI legs, valgrind included, passed with the adaptor in: valgrind tracks definedness through a copy without complaining, so it would not have caught this either.

### Three ideas the eighteen-map comparison suggested, all measured, none kept

*2026-09-07 · rejected · from asking what the post's own findings imply for this map; instrumented header, paired score, one map per binary*

**Double hashing instead of the triangular sequence, so siblings take different tours.** Folly uses `probeDelta = 2*tag+1`, with the comment that quadratic and linear "result in longer probe lengths". It aims at the finding that ~80% of what a counter fails to filter is siblings (see [And the fifth point on that axis](#and-the-fifth-point-on-that-axis-an-exact-counter-worth-2-3)): keys homed in the same group, which under a triangular sequence walk the *same* groups a later miss walks. The step comes from bits 8-15 of the hash, which the group (top bits) and the fingerprint (low byte) do not use. It is odd, so it still reaches every group of a power-of-two array exactly once. `uncount`, `place_group`, `slot_of_value` and the rehash's own copy of the placement walk all take it too. Six failing tests first found the missed rehash copy; the seventh failure is `erase_uncounts`, which asserts a comparison count that encodes the triangular shape.

**The mechanism works and the time is worse.** Instrumented, 4096 groups, 200 turnovers, groups per lookup, triangular -> double hashed:

| load | fresh miss | churned miss |
|---|---|---|
| 0.76 | 1.0522 -> **1.0349** | 1.0609 -> 1.0497 |
| 0.799 | 1.0856 -> **1.0542** | 1.1223 -> 1.0959 |

So it removes a third of the excess on a fresh miss, as intended. Paired on the score: `rmiss64` **0.915**, `build64` 0.950, `churnbig` 0.970, `rhit64` 0.984, control `hashstr` 1.04. One map per binary at 50000 entries: +4.6 instructions per lookup (57.2 -> 61.8 on a miss, 60.5 -> 65.4 on a hit), cycles 20.6 -> 21.3 and 29.5 -> 31.1, branch misses slightly *better* (0.108 -> 0.093), L1 misses unchanged. It costs three cheap ops and a live register in three loops, to save 0.03 groups on a path 5% of misses reach. **the group compare and the counter have already taken the probe down to 1.03-1.09 groups, so the shape of the sequence past home has nothing left to win.** **Correction (2026-10-02):** the churned column above came from a harness that churned in sequential keys; on a table churned with random keys the triangular sequence reads 1.265 and 1.432 groups per miss at 0.76 and 0.799, and double hashing takes that to 1.174 and 1.268. Re-measured one header per binary, the score is level and churned misses are 1.07-1.18x faster, see [Double hashing with the step taken from the fingerprint](#double-hashing-with-the-step-taken-from-the-fingerprint-re-measured-one-header-per-binary-after-the-churn-harness-was-fixed-churned-misses-107-118x-faster-the-score-level-a-fresh-integer-hit-up-to-5-slower-under-clang).

**An ungrouped, unaligned window, which is what `indivi::flat_wmap` does -- rejected on a simulation rather than built.** `flat_wmap` has the fastest hit of the eighteen maps and is 1.15-1.31x faster than its own grouped sibling, so the window itself is worth pricing. Simulated with the same keys at the same load, windows visited per placement, bucketized (home is a group of sixteen) against sliding (home is a slot, first free slot within sixteen): at load 0.799 **1.0481 against 1.0396**, at 0.76 1.0318 against 1.0238. Slot-level placement removes about a *fifth* of an excess already under 5%. That is 0.0085 windows per placement, against the 0.107 groups `move_home` takes off a churned miss (corrected 2026-10-02: said "That is a quarter of what `move_home` is worth"), and `move_home` is worth 11% of an in-cache miss and nothing out of cache. To collect it, the map would have to give up the merged block, since sixteen fingerprints starting at an arbitrary slot are not contiguous in an 88 byte block, and the merged block is measured at 2% of the score and 28% of the dTLB misses at 4M (see [Two optimizations the charts point at](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet)). Ceiling below cost; not built. **Later (2026-09-07):** `move_home` is worth about a tenth of a churned miss in cache and out of it, see [What `move_home` is actually worth, re-measured](#what-move_home-is-actually-worth-re-measured).

**Later (2026-09-08):** the window was built and timed, and the simulation turned out unable to see the time, see [The sliding window, built and measured rather than simulated](#the-sliding-window-built-and-measured-rather-than-simulated).

**And the reason `flat_wmap` is actually fast is not the window.** One map per binary, hits, its grouped sibling against it: **53.3 against 47.3 instructions** at 1000 entries, 54.6 against 48.3 at 50000, 72.6 against 64.4 at 1M. It has fewer L1 misses at every size, *including* the one that fits in L1 (0.876 against 0.378). The difference is six fewer instructions and half the metadata per slot (one byte against two), not the alignment. `unordered_dense` is at 60.8 instructions and 4.2 L1 misses per hit at 50000 against `flat_wmap`'s 48.3 and 3.3; closing *that* is a different and still-open question.

**A per-table seed, abseil's defence against keys chosen for a known hash.** `mixed_hash` returns `hash ^ m_seed`, with the seed scrambled from the table's own address, so two live tables differ and ASLR makes two processes differ. **On lookups it is free**: one map per binary at 50000 entries, +1.0 instruction and **0.0 cycles** on both a hit and a miss (miss 21.4 against 21.4, hit 29.6 against 29.6), ns/op identical to two decimals. On a build it costs **3.5%** (7.13 -> 7.38 ns per element, +1.7 cycles), because the pipelined rehash is latency-bound and the xor sits between the hash and the group address.

Two things make it a feature rather than a patch. First, the seed has to travel with the index it built through **six sites**: the allocator-aware copy and move constructors, `copy_everything_from`, `move_everything_from`'s two branches and `swap`. The suite caught every one of them (85 failures, then 70, then 11). The 11 that remain are `avalanching.cpp` asserting that `mixed_hash` returns an avalanching hash *unchanged*, which the seed contradicts by design, plus one steered `erase_uncounts` case; they test a value where they would have to test the property. Second, iteration order stops being reproducible between runs. So: worth having behind a macro, not worth making the default, since everyone pays the price and the threat is not everyone's.

**The paired harness read this one wrong, which is the rule working.** With the two headers in one binary the seed measured `build64` 0.936, `rmiss64` 0.940, `rhit64` 0.968, where one map per binary says 0.0 cycles on both lookup paths. The control `hashstr`, which never touches a map, read 1.027 in the same run. A paired two-header run decides a 10% question and not a 3% one.

**Also: the churned drift figures recorded below do not reproduce.** This is about the drift figures in [A mutating hit moves itself home](#a-mutating-hit-moves-itself-home-and-that-takes-the-churn-drift-back): at load 0.76 after 200 turnovers, 1.143 groups per hit and 1.265 per miss against a fresh 1.032 and 1.052. An instrumented header that reproduces the **fresh** pair to three digits (1.0311 and 1.0522) measures the churned pair at **1.0358 and 1.0609**, and it saturates: 5, 20, 100 and 400 turnovers give 1.039, 1.036, 1.035 and 1.035 per hit. It is load-sensitive as expected (at 0.799, 1.0660 and 1.1223) but never approaches 1.14/1.27 at 0.76. Either that harness churned differently in a way that matters or the figure is wrong. The churn here erases a uniformly random live key and inserts one the map has never held, at a constant size, on a reserved table. `move_home` itself is not in doubt: it was kept on a one-map-per-binary timing (misses 5.16 to 4.64 ns), not on the drift figure. But **the drift it takes back is smaller than recorded**, so the headline "a churned table probes 1.14 groups per hit against a fresh 1.03" that `CLAUDE.md` used to carry had to be re-derived before being quoted again. That was done on 2026-09-10 with `scripts/ab/probe_length.sh`, which reproduces the figures in this entry; `CLAUDE.md` and the README now quote those instead.

**Correction (2026-10-02):** this paragraph is wrong, and the drift figures it doubted are right. The instrumented header is `scripts/ab/probe_length.{cpp,sh}`, added on 2026-09-07 (commit 2d89d04), not 2026-09-10. Its churn inserted `present[at] = next++`, a counter, and consecutive integers through the integer hash spread over the groups more evenly than random keys, so the churned table it built had almost no drift. With the replacement key drawn at random (`r() >> 1`), the same harness reads **1.1364 groups per hit and 1.2649 per miss** at load 0.76 after 200 turnovers, and 1.2643 per miss at 0.763. That reproduces the 1.143 and 1.265 of [A mutating hit moves itself home](#a-mutating-hit-moves-itself-home-and-that-takes-the-churn-drift-back) and the 1.266 of [And the fifth point on that axis](#and-the-fifth-point-on-that-axis-an-exact-counter-worth-2-3). With one and four writing hits per round it reads 1.094 / 1.163 and 1.056 / 1.099 (hit / miss), against that entry's 1.091 / 1.162 and 1.054 / 1.093. Erasing in FIFO order, so that every key left in the table is sequential, gives exactly 1.0000 per hit and per miss, the extreme of the same effect. The headers at 5bdbd58 and 188b25a give the same 1.0358 / 1.0609 with the counter keys, so no header change explains the gap. The saturation series and the 0.799 figures above (1.0660 and 1.1223) measure the counter-key churn too. So the drift `move_home` takes back is as recorded, not smaller. Neither `CLAUDE.md` nor `README.md` quotes a probe-length figure today. `probe_length.cpp` and `move_home.cpp` draw random replacement keys from 2026-10-02. `probe_length.sh` patches a `probe()` loop that has since been split into `probe`, `probe_from` and `probe_past_home`, so it stops with an error until its patch is updated; its header comment's "a churned table probes *better* than a freshly built one" is the same counter-key artifact (with random keys, four writing hits read 1.099 per miss at 0.76 against a fresh 1.052).

### And the fifth point on that axis: an exact counter, worth 2-3%

*2026-09-07 · rejected (not built) · instrumented copy of the header, exact answer rebuilt offline*

This is the one idea Verstable has that this map does not (see [Verstable measured rather than read](#verstable-measured-rather-than-read)). `m_overflows[g][c]` counts every live entry of class c that *passed* group g on its own sequence, whatever its home. Verstable's in-home-bucket bit answers the narrower, exact question "does anything belong here". The analogue here is a second set of eight counters per group holding "entries of class c whose home **is** g and which did not fit in g", consulted at step 0, where the current test costs a whole extra group visit whenever it is wrong.

Measured before writing any of it, on an instrumented copy of the header that rebuilds the exact answer offline by hashing every occupied slot. Checked against the invariant it must obey (an entry displaced out of g incremented g's counter on the way out) over 4096 groups and eight classes, with no violation. The instrumented probe reproduces earlier figures to three digits: churned miss 1.266 groups at load 0.763 after 200 turnovers against the 1.265 recorded in [A mutating hit moves itself home](#a-mutating-hit-moves-itself-home-and-that-takes-the-churn-drift-back), and 17.6% of misses continuing at step 0 against 17.5%. On the table the shipped header actually leaves, with `move_home` firing:

| load | groups per miss now | with the exact counter | continuing at step 0 | of those, siblings |
|---|---|---|---|---|
| 0.763 fresh | 1.046 | 1.040 | 3.7% | 85.5% |
| 0.763, 200 turnovers | 1.159 | **1.134** | 11.4% to 9.3% | **81.5%** |
| 0.793, 200 turnovers | 1.242 | **1.201** | 16.1% to 12.9% | **79.7%** |
| 0.50, churned | 1.003 | 1.003 | 0.3% | ~99% |

**About 80% of what the counter fails to filter is siblings**: entries that genuinely home in that group and genuinely did not fit in it. Both tests say continue for those, and both are right, since the key really could be further along. Being exact only removes the strangers, and strangers are a fifth of the problem.

Calibration against a change that was kept: `move_home` took 0.107 groups off a churned miss, worth 11% off an in-cache miss and nothing out of cache. The exact counter takes **0.025**, under a quarter of that: 2-3% in cache on a churned table, nothing on a fresh one, nothing at half load, nothing out of cache. The costs:
- 8 more bytes per group (88 to 96, so 5.5 to 6.0 bytes per slot, 9% more index memory);
- a store into the home group on every insert that does not fit home, every matching erase and every `move_home`;
- a second counter for the rehash, the erase and `move_home` to keep consistent, more of exactly the invariant the mutation sweeps keep finding uncovered.

**Correction (2026-10-02):** the calibration above scaled from `move_home`'s worth as first measured, "11% in cache and nothing out of cache". Re-measured with random churn keys, `move_home` takes 0.10 groups off a churned miss (1.265 to 1.163 at load 0.76) and is worth 26-30% of that miss in cache and out, see [What `move_home` is actually worth, re-measured](#what-move_home-is-actually-worth-re-measured). Scaled the same way, the exact counter's 0.025 groups would be worth roughly 6-7% of a churned miss, in and out of cache, nothing on a fresh table. That is an estimate from the same proportion, not a measurement: the exact counter was never built. The rejection rests on its cost (a store into the home group on many inserts and erases, a second counter invariant), which is unchanged, so it stands; but the gap it leaves is larger than the 2-3% recorded here. **Open (2026-10-02):** worth building and timing on a churn-at-fixed-size workload with misses before calling the counter axis closed.

So **this map's approximate counter is within 2-3% of the exact version of itself**, and the counter axis is closed: folly's single counter 0.959 on the score, nibble counters 0.986, two-bit counters 0.988, three fresh hash bits per step noise, exactness at step 0 worth 2-3% in cache only. What is left of a churned miss is siblings. The only thing that removes a sibling is putting it back home: `move_home` for the writing case, and for the reading case the erase-side pull-back, rejected for costing 20 ns per erase (see [Pulling a displaced sibling home on erase](#pulling-a-displaced-sibling-home-on-erase-to-take-the-churn-drift-back)).

### Three layouts borrowed from other maps, all lost: line-aligned value indices, a second fingerprint in the index word, a 16 bit index

*2026-09-05 · rejected · paired score*

The first two came from reading how other maps do it, and both lose for the same kind of reason: they add work to a path that runs on every lookup in order to help a case that is rare.

- **Cache-line aligning the value indices**, which boost and abseil get for free because their groups are aligned. A group's sixteen indices are exactly 64 bytes, and glibc hands back large allocations at 16 mod 64, so *every* group's indices straddle two lines. That is why `prefetch_index` asks for two. A 64 byte aligned block type for the index array does what it should on lookups (`find64` and `rhit64` both 1.02) and costs 4-5% on `build64`, `churn64` and `churnbig`, for a geomean of 0.993. The likely mechanism is conflict misses: with the group array and the index array both at power-of-two offsets, a group's metadata and its indices collide in the same cache sets more often than when one of them is skewed. (This was the layout of the time, a separate index array. Since 2026-09-06 the indices live in the 88 byte merged block, and `prefetch_index`'s two lines are the block's second and third, see [A block's prefetches should step from its start, not jump to its end](#a-blocks-prefetches-should-step-from-its-start-not-jump-to-its-end).)
- **A second fingerprint in the spare high bits of the value index**, emhash8's trick. The index word has to be loaded to reach the value, so bits spent there are free, and they reject a fingerprint collision before the value vector is touched. Eight bits cost nothing until a table wants more than 2^24 slots. Measured: geomean 0.975, and the losses are exactly on lookups (`find64` 0.912, `findbig` 0.919, `rhit64` 0.930, `churn64` 0.915). It puts an xor, a shift and a compare into the dependent chain of *every* lookup, to avoid a value access on the 3% that have a fingerprint collision. It is free for emhash8 because that map has no group-level fingerprint and must consult the word anyway. Here the group already filtered, so the second filter is redundant work in the hot spot. A filter only pays where nothing cheaper has filtered first.
- **A 16 bit value index for small maps**, CPython's compact dict: it stores 1, 2, 4 or 8 byte indices depending on capacity, where this map always stores 4. A `bucket_type::group_small` with `value_idx_type = std::uint16_t` makes the index 3.5 bytes per slot instead of 5.5 and puts two groups' indices in one cache line. Measured on the thirteen scored workloads that fit under 2^16 elements: geomean **0.986**, with only `rhit64` (1.03) and `findstr` (1.02) ahead and `churn64`, `churnstr` and `findbig` 3-4% behind. The reason kills the adaptive version too: a map small enough to be indexed in 16 bits has an index of at most 128 KB, already inside L2, so halving something that already fits buys nothing. The maps whose index footprint hurts are exactly the ones that need more than 16 bits. The narrow loads also cost a zero-extension on every use. `group_small` was not kept.

### A growth factor below 2, and the premise that suggested it was wrong

*2026-09-08 · rejected as a default, documented as a knob · asked as "should we benchmark folly's 1.406"; octave geomean, twelve points, heap counted by a replaced global `operator new` (`/tmp/gf.cpp`)*

**Folly doubles.** Printing `bucket_count()` after every insert for five maps: F14Value goes 24, 48, 96, 192, 384; F14Vector 20, 40, 80, 160; boost, abseil and `unordered_dense` likewise exactly 2x from the third step on. The `minGrowth` of `origCapacity * 1.406` in `reserveForInsertImpl` only binds on an explicit `reserve(n)`, never on growth by insertion. So there is no shipped sub-2x design in the field to copy. The suggestion to test it came from reading one line of folly rather than running it.

The question survives for **the value vector**, which is where this map's memory goes, a `std::vector` doubling on its own cadence. The index cannot change: a power-of-two group count is what `hash >> m_shifts`, the mask and the triangular probe's reaches-every-group property all rest on. The vector is a template parameter, so a different factor needs no header change: `grow_vec<T, A, NUM, DEN>` derives from `std::vector`, reserves `capacity * NUM / DEN` in `emplace_back`, and is passed as `AllocatorOrContainer`.

| factor | build ns/el | steady B/el | peak B/el | 64 B value: build / steady | string: build / steady |
|---|---|---|---|---|---|
| 2.00, shipped | 9.94 | 33.16 | 43.55 | 13.18 / 113.34 | 24.43 / 116.09 |
| 1.50 | 11.34 (+14%) | **29.80 (-10%)** | 40.35 (-7%) | 15.63 (+19%) / **98.88 (-13%)** | 26.72 (+9%) / **107.71 (-7%)** |
| 1.25 | 11.37 (+14%) | **27.95 (-16%)** | 40.63 (-7%) | 19.19 (+46%) / 90.55 (-20%) | -- |
| 1.125 | 13.95 (+40%) | 27.09 (-18%) | 41.19 (-5%) | -- | -- |

Reproduced on a second octave from 200000 to within 1% on every memory figure. So **1.5x buys 7-13% of steady memory for 9-19% of a build**. The peak barely moves, because at the growth peak the index doubling is alive too.

**Not worth changing the default, and worth documenting as a knob.** Building is this map's strongest column, 1.66x ahead of boost and 1.61x of abseil at an 8 byte value. Memory is its weakest, 32.6 bytes per entry against boost's 29.2 and abseil's 27.0. Spending 14% of the former to gain 10% of the latter lands exactly level with boost on memory and gives up the lead that pays for the dense layout. A caller who is memory-bound rather than build-bound can have it today with six lines and no fork, which is the right place for a trade that depends on which of the two is scarce.

### The sliding window, built and measured rather than simulated

*2026-09-08 · rejected · `scripts/ab/window.{cpp,sh}`, one variant per binary; asked as "I also want to try to completely switch the group index to this layout"*

[Three ideas the eighteen-map comparison suggested](#three-ideas-the-eighteen-map-comparison-suggested-all-measured-none-kept) rejected `indivi::flat_wmap`'s ungrouped window on a *placement* simulation: windows visited per placement, 1.0481 bucketized against 1.0396 sliding at load 0.799, a fifth of an excess already under 5%. That simulation could not see a time, and the time is bigger than it implied.

Setup: two index layouts over one implementation. They share the value vector, the hash, the fingerprint encoding, the load factor, tombstones, growth, the dense erase and the SSE2 helpers, and differ only in the home unit and the probe step. Both were cross-checked against `std::unordered_map` over a mixed stream first.

**The window wins the lookup, and it wins it at the branch predictor.** Per operation at 200000 entries:

| | instructions | cycles | branch misses | L1 misses |
|---|---|---|---|---|
| hit, grouped | 71.9 | 56.8 | 0.198 | 4.615 |
| hit, window | **70.5** | **53.6** | **0.167** | **4.852** |
| miss, grouped | 70.1 | 47.9 | 0.518 | 2.810 |
| miss, window | **67.4** | **44.0** | **0.436** | **3.018** |

So 16% fewer branch misses on both, three to four fewer cycles, one to three fewer instructions, and *more* L1 misses, because an unaligned sixteen byte load straddles two cache lines where an aligned one does not. Timed over three sizes and three runs: hits 1-5% faster; misses 0.5% slower at 32000, **13% faster at 200000** and 4% at a million; builds and memory a wash.

**The miss result is against the wrong baseline, and the probe lengths say so.** Both variants stop a miss on an *empty slot*, because a per-class counter has no group to hang on in the ungrouped one. So the grouped variant here is not the shipped index; it is the shipped index with its counters removed. Windows visited per miss, over the timed region only: at 200000 entries (load 0.763) **1.2962 grouped against 1.2246 window**, at a million (load 0.477) 1.0060 against 1.0027. The shipped index, which stops on a counter at home, visits **1.046** on a fresh miss at load 0.763 and about 1.003 at load 0.5 (corrected 2026-10-02: said "at any load"). So the 13% is the window leaving the first window slightly less often *when the miss test is an empty slot*. It evaporates where a miss stops at home anyway: at a million entries the window is 2% slower on a miss. Against the real counter-based miss there is close to nothing here. The hit advantage is smaller and more robust: 1 to 5%, and still 7% at a million where both variants visit 1.000 windows, so that part is addressing and instructions (the grouped home costs a multiply by sixteen), not probe length.

**And it loses churn for a reason worth having, which is that it recycles tombstones half as well.** At a million entries the window variant ends a churn run with **4194304 slots against 2097152**, one extra doubling, and 24% slower churn. Where placements land: 33.8% of the grouped variant's placements reuse a tombstone against **15.8%** of the window's; at 200000 it is 71.2% against 69.3%. The mechanism: `ctz` takes the lowest available lane. For a window that is the home slot itself; for a group it is the group's lane 0, a fixed position that all sixteen homes in that group probe first, so it is tombstoned and reused constantly. A window's first lane differs for every home and is more often a slot that has never been used. Burning fresh slots drives a load factor that counts live plus tombstones, so it buys an extra growth.

**Later (2026-09-08):** a transplant of only the per-key starting lane into the grouped variant reproduced the window's recycling, and showed that contending on one lane is what makes recycling work; it also cannot apply to `unordered_dense`, which has no tombstones. See [The lane-contention mechanism, confirmed by transplant](#the-lane-contention-mechanism-confirmed-by-transplant-and-it-runs-the-other-way-from-the-guess).

**So the answer to "switch the group index to this layout" is no, and the chain is what makes it no.** The window means no per-group counters (no group to hang them on). That means the miss stops on an empty slot, which means tombstones, and this map is tombstone-free, the property `churn` exists to protect. It also means giving up the merged 88 byte block, worth 7% of a lookup's instructions and 28% of its dTLB misses at 4M, since sixteen fingerprints starting at an arbitrary slot are not contiguous in it. Paying all that for 13% of a miss at one size, plus a worse churn, is the wrong trade. What the experiment leaves: **the win is branch misses, not cache lines**, and an aligned group's lane 0 being contended by all sixteen of its homes is a real effect that no probe-length simulation shows.

### The lane-contention mechanism, confirmed by transplant, and it runs the other way from the guess

*2026-09-08 · info · `scripts/ab/window.cpp` with `-DLANE_ROTATE`*

The window recycles tombstones at half the grouped rate. The reason offered was that `ctz` takes the lowest free lane: for a window that is the home slot, different for every key; for a group it is lane 0, shared by all sixteen of that group's homes. The test transplants *only* that property: the grouped variant gets a per-key starting lane from bits 8-11 of the hash and rotates the available mask by it, nothing else changes. Placement cost is a rotate and an and. No lookup is affected, because a group is compared whole either way.

**It reproduces the window's behaviour precisely.** At a million entries, churn, grouped against grouped-with-a-per-key-start, three runs each with no overlap:

| | grouped | grouped, per-key start | window |
|---|---|---|---|
| tombstones recycled | **33.8%** | **16.3%** | 15.8% |
| slots after the run | **2097152** | **4194304** | 4194304 |
| churn, ns | **67.2** | **87.5** | |

One property moved and the whole behaviour moved with it.

**So the direction is the opposite of what I wrote when I proposed the experiment.** Spreading the preferred lane does not improve recycling, it destroys it, and *contending on one lane is the feature*. A tombstone is created wherever a key was. If every key prefers lane 0, lane 0 is where the keys are, so lane 0 is where the tombstones are, so the next placement lands on one instead of consuming a fresh slot. This matters for `absl::flat_hash_map` and `emilib`, which both take the lowest lane and should keep doing so. It is an argument *against* any per-key lane spreading in a tombstone design, and it is why a sliding window, whose first lane is the home slot by construction, cannot recycle well.

**And it cannot apply to this map at all, which is the part I got wrong twice.** `erase_group_slot` writes a fingerprint of **0**, a genuinely empty slot, not a tombstone, so there is nothing to recycle. Lane position within a group does not affect any lookup, because `match_fingerprint` compares all sixteen in one instruction. Rotating the preferred lane here is a no-op by construction. Proposing it carried a finding across a design boundary without checking which side of it the finding lived on.

### A dense map on flat_wmap's structure, measured against the shipped one, and the comparison is not what it looks like

*undated (follows the 2026-09-08 window experiment) · rejected · `scripts/ab/window.cpp` variants, one binary each; asked as "basically indivi map with the uint32 values"*

Variant 1 of the window experiment already *is* that map: sliding window, one metadata byte per slot, a `uint32` index, a dense value vector. A third variant was added that is `ankerl::unordered_dense` itself behind the same interface, and all three run identical workload code. Variant 2 reproduces the production harness to within 4% (build at 200000: 9.10 ns per element median here, one run 8.87, 8.77 from `maps_one.sh`) (corrected 2026-10-02: said "to within 1% (build at 200000: 8.87 ns per element here, 8.77 from `maps_one.sh`)"), which checks that the workloads are honest.

| per op, median of three | grouped+tombstones | window+dense | shipped |
|---|---|---|---|
| build, 200000 | 16.69 | 16.66 | **9.10** |
| hit, 200000 | 8.62 | 7.74 | **6.77** |
| miss, 200000 | 7.76 | 6.65 | **4.72** |
| churn, 1M | 65.71 | 83.87 | **62.01** |
| bytes/entry, 1M | 27.26 | **27.26** | 28.31 |

**Read that as a prototype against a tuned library, not as a design comparison.** The build gap is 83% (corrected 2026-10-02: said "83-143%"; no recorded figure gives 143%), and almost none of it is the index: the shipped map has the pipelined rehash, the prototype rebuilds one element at a time. The lookup gap is the merged block and the prefetches. The design question is answered by variant 0 against variant 1 (same author, same afternoon, same quality), as recorded in [The sliding window, built and measured rather than simulated](#the-sliding-window-built-and-measured-rather-than-simulated): the window is worth 1-5% on a hit, nothing on a miss once the baseline has counters, and costs 24% of churn at a million entries.

**So: not worth adapting, and the reason is structural rather than a number.** The window forecloses both the per-class counters (so tombstones) and the merged block (see [The sliding window, built and measured rather than simulated](#the-sliding-window-built-and-measured-rather-than-simulated)). The counters are worth 1.4-1.7x of a miss against an otherwise identical SwissTable, and the merged block 7% of a lookup's instructions and 28% of its dTLB misses at 4M. The window is worth a few percent of a hit and 2-4% of memory. It is on the wrong side of that trade by an order of magnitude, and tuning the prototype cannot change which side it is on.

**And what the prototype cannot answer, which is why `indivi::flat_wmap` is fast in absolute terms.** Both prototype variants are dense, with a value index between the metadata and the key, and have identical metadata width. `flat_wmap` is *flat*, with the key in the slot the window found, and carries one metadata byte per slot against this map's 5.5. [Every other map on the same workloads, in one harness](#every-other-map-on-the-same-workloads-in-one-harness) already attributes its speed to those differences: 48.0 instructions per hit against `flat_umap`'s 54.3 and this map's 60.5. The prototype holds both fixed on purpose, because it was built to ask "is the window worth anything, all else equal". The answer is a few percent. The answer to "why is that map fast" is the flat family and one byte of metadata, and the window is the smallest of the three.

### Eleven slots and twenty-four slots, and why sixteen is where it stops

*2026-09-10 · rejected · paired octave against the shipped sixteen, above 1.00 = the candidate is faster; asked as "how about doing only 11 lanes", "12 fingerprints, 4 counters, 12 indices" and "how about going bigger"*

The 12-slot block was measured on 2026-09-07 at 0.9888 (see [Seven ideas from reading folly F14 and from the F14Vector string gap](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept)).

**Eleven slots: 11 fingerprints, 8 counters, 11 four-byte indices, one pad byte, `alignas(64)`.** Exactly one cache line with *full* `uint32` indices, so unlike the 3-byte-index prototype it caps nothing, and the block address becomes `shl $6` instead of the `imulq $0x58` that sits on the address path of every lookup. Results: `rmiss64` **1.037**, `rmissstr` 1.012, `findstr` 1.010, `rhit64` 1.007, `find64` 1.000, `build64` 0.981, `ie64` **0.963**, `churnbig` 0.946, `churn64` **0.944**; control `hashstr` 0.893. Misses 1-4% better, churn and insert-erase 4-6% worse, net below 1.00. The `rmiss64` figure reproduces the 1.039 that `CLAUDE.md`'s octave rule and `scripts/ab/README.md` record for an eleven-slot group (commit 0042e9b: `rmiss64` 1.384 at one size and 1.039 over the octave, `churn64` 1.199 and 0.974), so that footnote and this are most likely the same design. `churn64` reads 0.944 here against 0.974 there, inside the layout band.

**What eleven adds that twelve could not say.** The 12-slot layout changed two things at once, a shorter group *and* four counter classes instead of eight, and lost churn 7%. Eleven keeps all eight classes and still loses churn 5.6%, which separates the two: **the counters were not the problem, the group length was.** At load 0.8 a group of sixteen holds 12.8 and overflows at 0.89 standard deviations of the number of keys homed in it (Poisson, sd = sqrt(mean)); a group of eleven holds 8.8 and overflows at 0.74 (corrected 2026-10-02: said 2.0 and 1.65, the spread of independently full slots, which cannot overflow). Shorter groups fill more often, more entries land away from home, and every insert and erase then walks and updates more counters.

**Twenty-four slots: 24 fingerprints, 8 counters, 24 indices, exactly 128 bytes**, 5.33 bytes per slot, slightly *less* than the shipped 5.5. The trade was supposed to invert: a group of 24 overflows at 1.10 standard deviations (corrected 2026-10-02: said 2.45), so churn should have come back. **It is the worst of the three and it loses everywhere.** Nineteen of nineteen scored workloads at or below 1.005 (the tables list sixteen; `itstr`, `iestr` and `itbig` were not recorded):

| `rmiss64` | `churn64` | `churnbig` | `find64` | `ie64` | `iebig` | `findbig` | `it64` | `rhit64` |
|---|---|---|---|---|---|---|---|---|
| **0.853** | 0.863 | 0.870 | 0.919 | 0.923 | 0.927 | 0.933 | 0.935 | 0.949 |

| `rmissstr` | `findstr` | `build64` | `rhitstr` | `churnstr` | `buildstr` | `buildbig` | control `hashstr` |
|---|---|---|---|---|---|---|---|
| 0.967 | 0.972 | 0.977 | 0.987 | 0.992 | 1.001 | 1.005 | 1.114 |

**And the reason is the counters, in the direction opposite to the prediction.** The largest single loss is the *miss*, which is what the counters exist to stop. There are still only eight classes, so a group of 24 puts **three slots in each class instead of two**. Any given class holds more entries, its counter is nonzero more often, and a miss continues past home more often. Enlarging the group without enlarging the counter array dilutes the filter. That is the counter-width axis reached from the other end (one class scored 0.959, sixteen nibbles filtered better but cost arithmetic), and it says the eight counters and the sixteen slots are not two choices but one.

**So sixteen is a local optimum on three axes at once**, and moving either way breaks one of them:

| | 11 slots | 16 slots, shipped | 24 slots |
|---|---|---|---|
| overflow frequency (corrected 2026-10-02: said 1.65 / 2.0 / 2.45 sd) | worse (0.74 sd) | 0.89 sd | better (1.10 sd) |
| slots per counter class | 1.4 | **2** | 3, and the miss pays for it |
| the compare | one SSE2 load | **one SSE2 load** | two |
| paired octave | below 1.00 | -- | 0.85 to 1.00 everywhere |

Both prototypes needed the same two fixes before they were correct, and either is a trap for the next attempt. The group count must be rounded down to a power of two, because `max_bucket_count() / slots_per_group` is not one when the slot count is not, and `max_size()` has to follow it. Without the first, `bucket.cpp`'s one-byte-index `group_micro` corrupts the heap (the same failure the 12-slot entry records); without the second it segfaults instead.

**Why none of them could have won, which is the part worth keeping.** All three shrink or align the block to remove L1 fills, and **the fills are not on the critical path**. The 12-slot entry said it first (1.4 fewer L1 misses per string lookup, cycles within 1%, 103.3 to 102.5 and 135.1 to 135.7) (corrected 2026-10-02: said "cycles unchanged to the tenth"). [The F14Vector string miss, re-measured](#the-f14vector-string-miss-re-measured-and-the-old-explanation-of-it-is-wrong), the same day, says it independently: removing both index prefetches removes **1.08 L1 fills per string miss** (from 5.19 to 4.11, against F14Vector's 3.91) and buys **1.8% of time**. A prefetch is asynchronous by construction, so the counter moves and the clock does not. Any future idea justified by "it touches fewer cache lines" has to answer that first.

### Tiny pointers, and the bound insertion order puts on the value index

*2026-09-10/11 · #229 · rejected (closed) · `scripts/ab/value_prefetch.{cpp,sh}`*

Asked whether a tiny pointer (Bender et al., SODA '23; Flattened-TPHT, VLDB '26) could shrink the four of five and a half bytes per slot that are the `uint32_t` value index. **Half of it is settled without building anything, and the half that needed a measurement is now measured and lost.**

**A tiny pointer needs the referrer to choose where the referent goes.** The value vector of `unordered_dense` is insertion-ordered, and that order is public contract: `values()`, iteration, `extract()`, `replace()`. The user chooses every position, so the map from slot to vector position is an arbitrary permutation of n things and the index has to encode it: **log2(n) - 1.44 bits per entry**, however the bits are arranged, at home or away from it. The issue's premise, a narrow index into a range the group implies, has nothing to stand on: the group implies nothing about where the value sits. That bound caps every width reduction, tiny or otherwise:

| value index | index B/entry (4 x 1/load, octave geomean 1.77) | total B/entry, 8 byte value |
|---|---|---|
| `uint32_t`, shipped | 7.1 | 32.6 |
| 24 bit, built and measured, score 0.995 clang / 0.983 gcc, capped at 2^24 | 5.3 | ~30.8 |
| the information-theoretic floor at n = 2^20 | 4.1 | ~29.6 |

So the whole axis is worth **at most ~9% of memory and no speed** inside the contract, and the L1 fills a narrower block saves are not on the critical path (see [Eleven slots and twenty-four slots](#eleven-slots-and-twenty-four-slots-and-why-sixteen-is-where-it-stops), the paragraph [Why none of them could have won](#eleven-slots-and-twenty-four-slots-and-why-sixteen-is-where-it-stops)). It should not be prototyped.

**What that leaves is a different container**: values in a hash-addressed table, so the *map* chooses each value's slot. Its one possible speed win is that the value's line becomes a function of the group, so it can be fetched in parallel with the fingerprints instead of after them. `unordered_dense` pays two dependent memory accesses on a hit where a flat map pays one, which is most of the 10-13% boost leads by on a fresh hit. Whether a software prefetch recovers that latency is the one thing argument cannot settle, so it was measured by making the prediction true inside the *shipped* map: `scripts/ab/value_prefetch.{cpp,sh}` inserts keys in home-group order, exactly twelve per group, so the values of group g are twelve contiguous entries at `values + g * 12`, and a patched `probe()` prefetches that address before touching the group. Perfect packing and no holes, so it is an upper bound on what the container could get.

Four variants: **A** random fill and no prefetch (today), **B** grouped fill and no prefetch (locality alone), **C** grouped and prefetching one to three lines (the question), **D** random and prefetching (the tax on a wrong address). ns per lookup, `uint64_t` key, n = 12 x 2^p:

| | p=16, hit | p=18, hit | p=20, hit | p=16, miss | p=18, miss | p=20, miss |
|---|---|---|---|---|---|---|
| A random, no prefetch | 14.74 | 52.17 | 64.69 | 6.87 | 25.58 | 38.06 |
| B grouped, no prefetch | 14.95 | 51.65 | 65.53 | 6.68 | 26.26 | 38.30 |
| C grouped, 1 line | 13.75 | 48.06 | 58.87 | 8.30 | 30.40 | 38.65 |
| C grouped, 3 lines | 15.28 | 46.60 | **53.98** | 11.21 | 36.33 | 41.38 |
| D random, 1 line | 15.15 | 54.91 | 65.70 | 8.18 | 30.55 | 38.66 |

**Three things kill it.**

*The gain does not clear the bar, and the bar was set before the run.* The rule was hit(C) at or below 0.75 x hit(B) at both DRAM sizes, for integer and string keys. The best case is 0.824 at p=20 with an integer key, and **strings are 0.99** (148.74 to 147.11 ns at p=18, noise). For a string the value load was never the bottleneck: the key compare is a second dependent load into the string body and it happens either way.

*The miss gets worse everywhere.* Integer +24% at p=16, +16% at p=18; string +13% and +18%. A miss never reads a value, so every one of those prefetches is wasted work on the critical path. A 50/50 workload is a **net loss** at p=18 and about 8% ahead only at p=20 with an integer key.

*Locality alone is worth nothing.* B equals A to within 3%, in both directions, at every size and both key types (corrected 2026-10-02: said "to within half a percent"; the miss, which never reads a value, moves as much as the hit), so the container gets nothing for free from owning placement; its whole case is the prefetch. Even the ceiling is modest: hit(B) minus miss(B) at p=20 is 27.2 ns, the whole value load, and three lines of prefetch recover 11.6 of it, 42%, for six instructions and 2.8 extra L1 fills.

So the container would give up insertion order, `values()` and vector-speed iteration, land at an estimated 28-30 bytes per entry (between boost and absl, not below them), lose on misses, pay a 14-19% tax in cache, and win at most 17.6% on an integer hit at twelve million entries. **Closed.**

**And a measurement trap worth more than the result.** The first run said the grouped layout alone was worth 18-25% at p=20. That is impossible, since a miss never reads a value and the index is structurally identical either way. The tell was in the instruction counts: **300 per lookup for a probe that stops in its home group**. `perf stat` counts the whole process, and building a table of twelve million entries by rejection sampling is most of the run at that size. So the "locality" column measured that a random-order fill is slower than a grouped one. The harness now reports every figure as the **slope of two rep counts**, which subtracts any fixed cost exactly. A and B then agree on instructions to the tenth, the physical check that the subtraction worked. Any harness that builds its own table and measures the process has this bug available to it.

### The default maximum load factor swept from 0.75 to 0.9: every step above 0.8 costs both compilers the same, and the one step below buys 0.8% for 6.7% more index

*2026-09-22 · #306 · rejected (kept at 0.8) · Ryzen 9 7950X, clang 22 and gcc 16, THP `madvise`, `scripts/ab/load_factor.sh`, `run.sh -p 50 all 12` pinned to one core*

**Kept at 0.8.** 0.75 is the only value ahead, by 0.76% under clang and 0.77% under gcc, and pays 6.7% more index for it, which is the trade #306 said not to take. Nothing changes in the header; the knob is `max_load_factor(float)` for a caller who weighs the two differently.

`default_max_load_factor` had been 0.8 since the group index landed, and nothing in this file measured another value; boost ships 0.875. Each value became the candidate header's default and ran against main's 0.8. Fifty points, not five, because every maximum doubles the table at a different size (0.875 grows at 1793, 3585, ... where 0.8 grows at 1639, 3277, ...), so the two sawtooths are out of phase: [Fifty points draw the load-factor sawtooth](#fifty-points-draw-the-load-factor-sawtooth-that-five-average-out-and-five-points-are-worth-26-of-a-cross-family-ratio) (see [Fifty points draw the load-factor sawtooth that five average out](#fifty-points-draw-the-load-factor-sawtooth-that-five-average-out-and-five-points-are-worth-26-of-a-cross-family-ratio)). The first column of each compiler is main against itself. baseline/candidate, above 1 means the candidate is faster:

| workload | clang 0.8 | clang 0.75 | clang 0.85 | clang 0.875 | clang 0.9 | gcc 0.8 | gcc 0.75 | gcc 0.85 | gcc 0.875 | gcc 0.9 |
|---|---|---|---|---|---|---|---|---|---|---|
| `it64` | 0.997 | 0.998 | 0.998 | 0.998 | 0.998 | 1.001 | 1.001 | 1.000 | 0.999 | 0.999 |
| `ie64` | 0.993 | 1.012 | 0.968 | 0.945 | 0.922 | 1.000 | 1.023 | 0.977 | 0.948 | 0.922 |
| `build64` | 0.999 | 1.012 | 0.968 | 0.937 | 0.904 | 1.001 | 1.030 | 0.944 | 0.907 | 0.859 |
| `churn64` | 1.001 | 1.016 | 0.979 | 0.952 | 0.924 | 1.001 | 1.018 | 0.972 | 0.956 | 0.927 |
| `find64` | 1.003 | 1.012 | 0.989 | 0.978 | 0.964 | 1.000 | 1.008 | 0.986 | 0.971 | 0.960 |
| `itstr` | 1.000 | 1.000 | 1.000 | 1.000 | 0.999 | 1.001 | 1.001 | 0.999 | 0.999 | 0.999 |
| `iestr` | 1.014 | 1.016 | 0.996 | 0.984 | 0.975 | 1.002 | 1.012 | 0.988 | 0.977 | 0.962 |
| `buildstr` | 1.014 | 1.006 | 0.987 | 0.982 | 0.968 | 1.013 | 1.013 | 0.998 | 0.987 | 0.978 |
| `churnstr` | 1.008 | 1.009 | 0.993 | 0.982 | 0.971 | 1.000 | 1.008 | 0.985 | 0.975 | 0.962 |
| `findstr` | 1.000 | 1.002 | 0.999 | 0.994 | 0.988 | 0.999 | 1.001 | 0.998 | 0.992 | 0.987 |
| `itbig` | 1.000 | 0.999 | 1.000 | 0.999 | 1.000 | 0.999 | 0.999 | 1.000 | 1.000 | 0.999 |
| `iebig` | 0.994 | 1.012 | 0.971 | 0.948 | 0.925 | 1.000 | 1.019 | 0.976 | 0.952 | 0.930 |
| `buildbig` | 0.999 | 0.998 | 0.996 | 0.989 | 0.981 | 1.000 | 0.991 | 0.996 | 1.002 | 0.998 |
| `churnbig` | 0.997 | 1.013 | 0.973 | 0.954 | 0.930 | 1.002 | 1.005 | 0.986 | 0.975 | 0.959 |
| `findbig` | 1.002 | 1.006 | 0.987 | 0.979 | 0.965 | 1.003 | 1.009 | 0.983 | 0.973 | 0.962 |
| `rhit64` | 1.002 | 1.002 | 0.998 | 1.005 | 1.000 | 1.034 | 1.030 | 1.034 | 1.025 | 1.011 |
| `rmiss64` | 0.953 | 1.019 | 0.928 | 0.908 | 0.867 | 1.004 | 1.024 | 0.970 | 0.942 | 0.929 |
| `rhitstr` | 1.011 | 1.004 | 1.001 | 0.998 | 0.998 | 1.000 | 1.007 | 1.005 | 1.002 | 0.996 |
| `rmissstr` | 1.010 | 1.006 | 0.989 | 0.980 | 0.973 | 1.000 | 1.006 | 0.988 | 0.978 | 0.967 |
| `hashstr` | 0.918 | 0.924 | 1.022 | 1.020 | 1.030 | 1.001 | 1.000 | 1.000 | 1.004 | 1.001 |
| geomean, 19 without `hashstr` | **0.9998** | **1.0074** | **0.9850** | **0.9740** | **0.9599** | **1.0031** | **1.0108** | **0.9886** | **0.9765** | **0.9628** |
| geomean over the control | 1.0000 | 1.0076 | 0.9852 | 0.9742 | 0.9601 | 1.0000 | 1.0077 | 0.9855 | 0.9735 | 0.9598 |

**Monotone, and the two compilers agree to 0.1% at every value**: over the control, 0.85 reads 0.985 under both, 0.875 reads 0.974 and 0.974, 0.9 reads 0.960 and 0.960. That is 0.4% of the geomean per 0.025 of maximum load below 0.8, rising to 1.4% at the top, all of it on the workloads that place or miss: `build64`, `ie64`, `iebig`, `churn64`, `churnbig` and `rmiss64` lose 4-14% at 0.9 and `iestr` and `churnstr` 2.5-4% (corrected 2026-10-02: said "about 1.3% of the geomean per 0.025 of maximum load" and "`build64`, `ie*`, `churn*` and `rmiss64` lose 5-14% at 0.9"), the 50% `find*` 1-4%, all-hit `rhit*` and iteration nothing. It is the superlinear miss cost that [Fifty points draw the load-factor sawtooth that five average out](#fifty-points-draw-the-load-factor-sawtooth-that-five-average-out-and-five-points-are-worth-26-of-a-cross-family-ratio) found at the top of each cycle, now paid over a longer stretch of it.

**What the index saves, computed rather than measured, because it is arithmetic**: the table grows when `size() > floor(16 * groups * max_load_factor)` and the index is 88 bytes a group, so its bytes per entry over an octave follow from the maximum alone. Geometric mean over 2000 sizes across the octave from 2^20:

| maximum load | 0.75 | 0.8 | 0.85 | 0.875 | 0.9 |
|---|---|---|---|---|---|
| index bytes per entry, mean | 10.37 | 9.72 | 9.15 | 8.89 | 8.64 |
| range across the octave | 7.34-14.67 | 6.88-13.75 | 6.47-12.94 | 6.29-12.57 | 6.11-12.22 |
| against 0.8 | 1.067 | 1 | 0.941 | 0.914 | 0.889 |

For `map<uint64_t, uint64_t>` the values are 16 bytes an entry before the vector's own slack, so 0.875's 8.6% of the index is under 3% of the map; for a 64 byte value it is under 1.5%. boost's 0.875 would cost `unordered_dense` 2.6% of the score to save that.

What this says and does not say: the paired harness at fifty points per octave, sizes 50000 to 200000, all in cache. Past the cache a smaller index misses less, which would move the answer towards a higher maximum; that is the size axis and it was not swept. The control's `rmiss64` reads 0.953 under clang, the harness's per-build band on the miss workloads ([The paired harness has a systematic bias on `rmissstr`](#the-paired-harness-has-a-systematic-bias-on-rmissstr-about-36)), so a single miss cell is good to about 5%. The geomeans, and the agreement between compilers, are what to quote. `hashstr` is the hash alone and is left out of the geomean.

### Double hashing with the step taken from the fingerprint, re-measured one header per binary after the churn harness was fixed: churned misses 1.07-1.18x faster, the score level, a fresh integer hit up to 5% slower under clang

*2026-10-02 · no issue · superseded · Ryzen 9 7950X, clang 22 and gcc 16, one header per binary, 3 rounds alternated (5 for the score), medians, `AB_CORE=2`; variant generated by a patcher outside the tree (step = 2 * fingerprint + 1, as folly's `probeDelta`; `delta` stays the step counter so every termination bound keeps its meaning)*

Re-taken because the churned half of the 2026-09-07 rejection (see [Three ideas the eighteen-map comparison suggested](#three-ideas-the-eighteen-map-comparison-suggested-all-measured-none-kept)) came from `scripts/ab/probe_length.cpp` churning in sequential keys, which leaves almost no drift to save. The 2026-09-07 variant took its step from hash bits 8-15; this one takes it from the fingerprint, which every probe site already holds, so no signature changed. Ten sites walk the sequence (`probe_from`, `probe_after_home`, `place_group`, `uncount`, `erase_group_slot`, `move_home`, `slot_of_value`, `repoint_value`, `fill_buckets_from_values` and `next_group` itself). The unit suite passes except the one test that encodes "the next group" (`erase_uncounts.cpp`, `a_rehash_rebuilds_the_overflow_counters_as_they_were`, 19 compares against 27).

**Groups per lookup**, `scripts/ab/probe_length.sh`, 200 turnovers, triangular -> double hashing:

| load | fresh hit | fresh miss | churned hit | churned miss | churned miss, one writing hit per round |
|---|---|---|---|---|---|
| 0.76 | 1.0311 -> 1.0272 | 1.0522 -> 1.0361 | 1.1364 -> 1.1134 | 1.2649 -> **1.1740** | 1.1634 -> 1.1064 |
| 0.799 | 1.0390 -> 1.0331 | 1.0856 -> 1.0542 | 1.2035 -> 1.1621 | 1.4316 -> **1.2676** | 1.2792 -> 1.1535 |

**Time**, triangular over double hashing (above 1.00 means double hashing is faster); churned tables from `scripts/ab/move_home.cpp` (load 0.799, 40 turnovers, `move_home` on as shipped), fresh lookups from `scripts/ab/prefetch_index.cpp`'s `find_all`, builds from `scripts/ab/place_inline.cpp`:

| | clang | gcc |
|---|---|---|
| churned miss, no writing hits, 52363 / 838860 / 3355443 | **1.129 / 1.131 / 1.106** | **1.176 / 1.177 / 1.148** |
| churned miss, one writing hit per round | 1.086 / 1.084 / 1.065 | 1.127 / 1.128 / 1.097 |
| churned hit, no writing hits | 1.012 / 0.986 / 1.006 | 1.017 / 0.993 / 0.973 |
| churned hit, one writing hit per round | 0.992 / 0.914 / 1.009 | 1.009 / 1.001 / 0.982 |
| churn round, 52363 | 0.996 | 1.052 |
| fresh `uint64_t` hit, 50000 / 1M / 4M | **0.954 / 0.981 / 0.974** | 1.018 / 0.997 / 1.006 |
| fresh string hit, 50000 / 1M / 4M | 0.998 / 0.989 / 0.999 | 0.998 / 1.000 / 1.001 |
| fresh `uint64_t` miss, 50000 / 1M / 4M | 1.014 / 0.994 / 0.988 | 1.036 / 1.023 / 1.020 |
| build from empty, `uint64_t` / string, 200000 | 1.022 / 1.000 | 1.009 / 0.988 |
| build from empty, `uint64_t` / string, 1M | 1.002 / 0.999 | 0.999 / 0.995 |

**The score**, `scripts/ab/solo.sh` in a scratch copy of the tree, baseline over candidate: **1.0008 under clang, 1.0010 under gcc**. Instructions per operation (`perwl.sh`, candidate over baseline): clang +2.1 to +4.6% on the integer and big-value find, churn and insert-erase, everything else within 0.5%; gcc +7.0% on the integer find, -2.7% and -1.7% on the integer and big-value builds, everything else within 1%.

**What this says.** The 2026-09-07 numbers (`rmiss64` 0.915, `build64` 0.950 paired) do not reproduce one header per binary for this variant: the score is level, builds are level, fresh misses are level or better. What double hashing buys is real and is on a table churned at a fixed size: 7-18% off a miss, the same size as `move_home`'s gain, and the two stack (the one-writing-hit rows). What it costs is a fresh integer hit under clang, 2-5% (two more instructions on the critical path, the step), and nothing measurable elsewhere. The "nothing left to win" of 2026-09-07 was a statement about the sequential-key table.

**What this does not say.** Two side effects were reasoned about and not measured: while a table has sixteen groups or fewer, every key of one counter class has the same step modulo the group count, so the smallest tables give a class one shared sequence; and the first step off home is no longer the adjacent block, which the hardware prefetcher served. ARM is unmeasured. The caller corpus and the shape search were not re-run, and a change to `next_group`'s inputs changes what the probe keeps live. It is a candidate for its own issue, not a change made here.

**Later (2026-10-02):** #355 measured seven sequences and five ways of computing their step, and none is at or under the triangular sequence everywhere; see [Seven probe sequences](#seven-probe-sequences-and-five-ways-of-computing-their-step-against-the-triangular-one-a-key-dependent-step-shortens-a-churned-miss-and-every-way-of-computing-it-costs-the-walk-a-third-live-value-that-no-source-shape-hides). The "two more instructions on the critical path" above is four under clang in that loop: the step's register spills the caller's accumulator.

### Seven probe sequences and five ways of computing their step against the triangular one: a key-dependent step shortens a churned miss, and every way of computing it costs the walk a third live value that no source shape hides

*2026-10-02 · #355 · rejected · Ryzen 9 7950X, clang 22 and gcc 16, `AB_CORE=2`; `scripts/ab/probe_sequence.{py,sh}` (variants), `scripts/ab/probe_length.sh`, `scripts/ab/solo.sh -h` + `perwl.sh` (score, 5 rounds), `scripts/ab/prefetch_index.cpp` under `perf stat` (instructions per lookup)*

#355 asked for a sequence that keeps [double hashing's churned-miss gain](#double-hashing-with-the-step-taken-from-the-fingerprint-re-measured-one-header-per-binary-after-the-churn-harness-was-fixed-churned-misses-107-118x-faster-the-score-level-a-fresh-integer-hit-up-to-5-slower-under-clang) and loses nothing anywhere else. None does. `scripts/ab/probe_sequence.py` writes every variant as a patched copy of the header in which all ten walks go through one `advance()`; `--check` proves for every variant, every fingerprint and 4 to 65536 groups that the walk visits every group within its bound (a variant that visits some group twice gets the bound raised by as many steps; shortening that bound makes `--check` fail). The unit suite passes against every variant except, for the ones whose first step is not +1, the test that encodes "the next group" (`a_rehash_rebuilds_the_overflow_counters_as_they_were`).

| variant | step `delta` (1, 2, ...) |
|---|---|
| `tri` | `delta` (shipped) |
| `dh` | `2*fp + 1` (folly's `probeDelta`) |
| `dh1`, `dh2` | `delta` for the first one or two steps, then `2*fp + 1` |
| `dhhi`, `dh1hi` | `2*(fp >> 3) + 1`, which the counter class `fp & 7` does not use; and with home+1 first |
| `dhcls` | `2*class + 1`: the class is in a register anyway |
| `tric1` | `2*class + 1` for the first step, triangular after it |
| `dhp`, `dhq` | `dh` with the step packed into `delta` (`steps << 9 \| step`, and `step << 32 \| steps`) |
| `dhv`, `dhclsv`, `tric1v` | the step's input behind an empty `asm volatile`, so it cannot be hoisted (diagnostic) |
| `<v>peel` | any of them with the home group peeled off `probe()`, `place_group`, `slot_of_value` and `repoint_value` |

**Groups per lookup**, 4096 groups, 200 turnovers, no writing hits (churned miss / churned hit; fresh miss at 0.799):

| | 0.76 churned miss | 0.76 churned hit | 0.799 churned miss | 0.799 churned hit | 0.799 fresh miss |
|---|---|---|---|---|---|
| `tri` | 1.265 | 1.136 | 1.432 | 1.204 | 1.086 |
| `dh` | **1.174** | 1.113 | **1.268** | 1.162 | 1.054 |
| `dhhi` | 1.172 | 1.115 | 1.279 | 1.165 | 1.054 |
| `dhcls` | 1.205 | 1.118 | 1.337 | 1.167 | 1.067 |
| `tric1` | 1.204 | 1.119 | 1.319 | 1.169 | 1.067 |
| `dh1` | 1.236 | 1.130 | 1.363 | 1.189 | 1.075 |
| `dh2` | 1.254 | 1.136 | 1.415 | 1.202 | 1.084 |

The gain is in the first step. Keeping home+1 first (`dh1`) gives back two thirds of it and `dh2` nearly all: a full group's overflow all going to the one adjacent group is what lengthens the churned walk, not the steps after it. The decorrelated step (`dhhi`) is no better than `dh` at 4096 groups. Spreading the first step over eight neighbours by class (`dhcls`, `tric1`) gets two thirds of `dh`'s gain. On 4, 8 and 16 groups (2000 turnovers) the cells move in both directions by up to 0.1 group with a few hundred keys and no variant is consistently worse than `tri`; the class/step correlation the issue worried about does not show.

**Instructions per lookup**, the score's `find_all` at 50000 entries, (20 calls minus 0 calls) / 20M, hit / miss:

| | clang | gcc |
|---|---|---|
| `tri` | 54.0 / 42.8 | 51.0 / 39.8 |
| `dh` | 57.9 / 43.3 | 53.0 / 42.5 |
| `dhcls` | 55.9 / 42.5 | 54.9 / 43.6 |
| `tric1` | 58.0 / 43.7 | 55.1 / 43.8 |
| `dhp` | 58.9 / 44.4 | 55.2 / 42.8 |
| `dhq` | 56.9 / 44.4 | 53.0 / 41.6 |
| `dhv` | 58.0 / 48.5 | 53.0 / 42.6 |
| `tripeel` | 55.9 / 40.6 | 51.1 / 37.8 |
| `dhpeel` | 58.0 / 42.4 | 52.1 / 37.5 |

Where `dh`'s four come from, `perf annotate` of clang's loop: the triangular walk needs the group and `delta`, and after the hash nothing else is live but the class and the broadcast fingerprint. A step from the key is a third value. For an integer key the home group is the walk's first iteration, so the step's computation is loop-invariant and is hoisted in front of the home group, and the register it holds pushes the caller's accumulator to the stack: `movzbl` + `lea` for the step, and a load and a store around every hit's `add`. Every way round it was tried and each fails on clang. `dhp`: scalar evolution sees that `delta & 511` never changes and splits the step back out into its own register. `dhq`: one instruction back, not four. `dhv`: the step is no longer hoisted, but the word it is made from now needs the register instead. `dhclsv`/`tric1v`: the barrier on the class stops the hoist, and the table's members are then reloaded every lookup. `dhcls`/`tric1`: `2*class+1` is hoisted into a register of its own even though the class is live. A per-lookup count in one caller's loop also moves with the inliner (`find_all` is inlined into `main` for `tri` and out of line for most variants), which is why the score decides.

**The score**, `solo.sh -h`, baseline over candidate, and `perwl.sh` (candidate over baseline, ins/op), the cells that miss +1%:

| | clang score | gcc score | clang ins/op | gcc ins/op |
|---|---|---|---|---|
| `dh` | 0.9969 | 0.9982 | integer/big find +3.6/+4.6%, insert-erase +3.3/+2.5%, churn +2.1/+3.5% | integer find +7.0% |
| `dhcls` | 0.9986 | 0.9990 | find +1.8/+2.8%, insert-erase +2.7/+2.0%, churn +1.9/+2.8% | integer find +10.0%, insert-erase +2.7/+2.4%, churn +2.0/+2.1% |
| `tric1` | 0.9944 | 0.9956 | find +3.7/+4.8%, insert-erase +5.5/+5.2%, churn +4.6/+3.6% | integer find +9.1%, insert-erase +3.8/+5.2%, churn +2.5/+3.3% |
| `dhpeel` | 0.9969 | **1.0082** | find 1.000/0.999, insert-erase +2.4/+1.4%, churn +3.9/+5.9% | **none**: nothing above +0.1%, integer find -3.1% |
| `tripeel` | 1.0020 | 1.0034 | none: nothing above +0.2%, integer churn -2.3% | none: nothing above +0.8%, -2 to -4% on the integer workloads |

**Peeling loses time although it saves instructions.** `tripeel` changes nothing but the peel and does fewer instructions in the score on both compilers, and it is slower: tri over variant, `move_home.cpp` at 52363 entries churned 40 times and the fresh `find_all` at 50000, 5 rounds rotated, medians:

| | clang `tripeel` | clang `dhpeel` | gcc `tripeel` | gcc `dhpeel` | gcc `dh` |
|---|---|---|---|---|---|
| churned miss | **0.851** | 0.946 | 0.996 | **1.180** | **1.173** |
| churned hit | 0.857 | 0.872 | 0.976 | 1.020 | 1.024 |
| churned miss, one writing hit | 0.825 | 0.886 | 0.998 | 1.135 | 1.132 |
| churn round | 0.964 | 0.959 | 1.022 | 1.059 | 1.040 |
| fresh `uint64_t` hit | 0.968 | 0.951 | 0.973 | 0.948 | 1.012 |
| fresh `uint64_t` miss | 1.000 | 1.059 | 0.985 | 1.057 | 1.034 |
| fresh string hit, builds | 0.987-1.004 | 1.001-1.004 | 1.003-1.021 | 1.002-1.015 | 1.000-1.021 |

Under clang the peeled `probe()` makes a churned lookup 14-18% slower; under gcc it costs the fresh integer hit 3-5%. The home group inlined twice over (once peeled, once as the loop's first iteration in `probe_from`, which other callers still use) is more code in every caller, and the instruction count does not see what it does to the front end. Peeling is rejected with or without a new sequence.

**What this says.** A key-dependent step takes 7-18% off a churned miss, and the first step is where all of it is. The price is one more value live across the walk, and the walk's preheader is on the hit path of every integer lookup. No source shape removes that: the compilers split, hoist or reload around every attempt, and the shape that does remove it (peeling) costs more in time than it saves. Plain `dh` is the best of them in time (gcc within the ±3% band or faster in every cell; clang 2-5% slower on a fresh integer hit), but it adds 3.6-7% instructions to the score's integer find, so #355's "instructions not above +1%" is not met by any variant on either compiler.

**Decision (2026-10-02):** the triangular sequence stays. Plain `dh` was offered as a trade (gcc churned misses 13-17% faster and everything else within ±3%, against a 2-5% slower clang fresh integer hit and +3.6% of its instructions) and declined.

**What would reopen it:** a sequence whose step needs no value that the triangular walk does not already hold (class-only steps were the attempt, and the compilers still give the step a register of its own), or a caller where a register is free, which is not something the header can know. ARM, the caller corpus and `fuzz_group_index` were not run, since no variant became a candidate.

## Lookup: the probe, SIMD compare and prefetch

This section covers the lookup path of the group index: the probe loop and its bound, the SSE2 match, what is inlined and what is called, the shape of `probe_result`, the two prefetch helpers, the empty-table sentinel, and the hit against 4.5.0 and boost in a real caller. The probe, the match and the prefetches have been audited down to the instruction, and most proposals to change them measured as losses. Why boost's hit takes fewer cycles at the same instruction count (#341) is answered in [Why boost's `unordered_flat_map` finds faster (#341)](#why-boosts-unordered_flat_map-finds-faster-341-it-needs-no-value-index-so-the-first-thing-a-lookup-touches-is-1-byte-of-metadata-per-slot-against-this-maps-55-which-leaves-l2-at-a-far-smaller-table-from-46080-entries-to-460800-this-map-takes-17-21x-boosts-l3-fills-per-find-and-14-35-more-cycles-under-clang-093-104-under-gcc-while-in-l2-it-is-compiler-codegen-alone-this-maps-cycles-083-088-of-boosts-under-gcc-122-125-under-clang): the value index past L2, codegen inside it.

**Where it stands** (as of 2026-09-28)

- Every miss is bounded by `|| delta == m_group_mask`; without it eight chosen keys hang `contains()` ([A miss had no bound](#a-miss-had-no-bound-and-eight-chosen-keys-made-it-loop-forever)).
- `probe` is force-inlined; under gcc that took the score against main from 1.149 to 1.244 ([gcc left `probe` out of line](#gcc-left-probe-out-of-line-and-forcing-it-inline-is-the-largest-single-gcc-gain-on-the-branch)). Fingerprint words come from a 256-entry table ([The gcc string-lookup gap, explained and mostly closed](#the-gcc-string-lookup-gap-explained-and-mostly-closed)).
- The probe past the home group is split off only for keys whose compare is a call (`detail::key_compare_is_call<Key>`): `std::string` miss 140.6 to 128.8 instructions, integer codegen byte-identical ([Splitting the probe past the home group](#splitting-the-probe-past-the-home-group-kept-for-keys-whose-compare-is-a-call)).
- The shipped `probe_result` is a local optimum; both narrower shapes cost the string find (+4.1% and +1.1% under clang) ([`probe_result`'s shape swept two ways, a third argued down from the return sequence](#probe_results-shape-swept-two-ways-a-third-argued-down-from-the-return-sequence-and-the-one-that-ships-is-the-best-of-them)).
- The SSE2 match has nothing left to remove. `prefetch_index` names two lines; dropping either one is within 3% on both compilers from 50000 to 16M entries (2026-10-02), so both stay. Dropping both costs gcc 12% at four million entries ([`prefetch_index`'s two lines re-measured](#prefetch_indexs-two-lines-re-measured-one-header-per-binary-dropping-either-is-within-3-on-both-compilers-from-50000-to-16m-entries-so-both-stay-but-not-for-the-reason-the-audit-gave); the first audit's reason is corrected there).
- Both prefetch helpers ask for two consecutive lines from where they start, clamped into the block. Naming a third line loses ([A block's prefetches should step from its start](#a-blocks-prefetches-should-step-from-its-start-not-jump-to-its-end), [`prefetch_index` was a line short for `group_big`](#prefetch_index-was-a-line-short-for-group_big-and-covering-that-line-is-not-the-fix)).
- A string hit is the hash, one cache miss for the stored key and `memcmp`; none has a lever left ([Where a string hit's ~90 cycles go, from perf](#where-a-string-hits-90-cycles-go-from-perf-and-two-more-things-it-led-to-that-lost)).
- Seven folly F14 ideas, none kept ([Seven ideas from reading folly F14 and from the F14Vector string gap](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept)).
- An empty table reads a static sentinel index, so `find` has no `empty()` test, and the map is 64 bytes (#329).
- In Redpanda's loop main's hit is +20-27 instructions over 4.5.0, half probe and half caller; three candidates lost and nothing changed (#341). Its keys are dense ids; on scrambled integer keys main is 2.2x faster than 4.5.0 (#346, [Hits in cache-resident tables](#hits-in-cache-resident-tables-1000-to-16000-entries-450-against-main-346-main-takes-041-061-of-450s-time-on-scrambled-integer-keys-and-071-079-on-strings-from-a-quiet-loop-and-a-busy-one-and-102-143x-of-it-on-dense-ids-0n-1-which-a-multiplicative-hash-places-without-collisions----450s-best-case-and-the-keys-redpandas-and-osrms-callers-have)).

### A miss had no bound, and eight chosen keys made it loop forever

*2026-09-05 · no issue · kept · found in the review before release*

The fix is `|| delta == m_group_mask` on the miss exit. A key that exists was placed within one cycle of its sequence, so a walk that has seen every group can stop. By mechanism it is free. Per lookup on a 200k table: 83.6 to 82.7 instructions on a hit, 69.5 to 67.6 on a miss, cycles and mispredictions unchanged. The paired score read 0.99, with three workloads at 0.95 beside a 1.16 on `hashstr`, which never touches the map, so that run's layout moved.

The bug. The probe stopped only at a group whose counter for the key's class was zero. The argument was that exact counters put a zero right after the furthest entry of that class. The argument is wrong: a counter counts entries that overflowed past its group on *their* probe sequences, not on the one being walked. Fill a group, send one key of class 1 past it, and erase the fillers. The passer stays, so the counter stays. Do that for every group: eight live keys, every class-1 counter positive, and `contains()` on an absent class-1 key never returns.

Any hash the caller controls reaches this. The default hash with attacker-chosen keys does too, since only the top few bits and the low byte need steering. `indivi::flat_umap`, where the counters came from, has the same hole: `find_impl` loops on `gIndex <= mGMask`, which the mask makes always true. The same eight keys hang it.

`test/unit/probe_termination.cpp` builds both this table and a saturated counter with the identity hash. The corpus fuzzers, on the default hash or (only `fuzz_insert_erase`) an identity over the whole key, could not have reached either (corrected 2026-10-02: said "all on wyhash").

The bound has a second effect. It turns a *missing or wrong-home* erase decrement from a hang into a silent slowdown. Before, that fault was loud: the counters only grew, a miss found no zero, and the suite hung. Now the miss stops at the end of the array and the table stays correct, only slower. The erase decrement was then uncovered by any correctness test: mutating it away SURVIVED all 771 cases.

**Later (2026-09-07):** the gap is closed by `test/unit/erase_uncounts.cpp`. The way to test anything in this family is to measure the lengthening, not an answer. The map gets a counting `KeyEqual`. A table that reached its contents by erasing a run of entries that had overflowed one group into the next must compare a miss exactly as often as a table built from the survivors directly. Three cases cover the erase path, the rehash that rebuilds the counters, and the two together.

That is the deliberate trade of making the map robust to a hostile hash: a hang is loud, degradation is quiet, and the map has to prefer the quiet one.

### gcc left `probe` out of line, and forcing it inline is the largest single gcc gain on the branch

*2026-09-05 · no issue · kept · paired harness against the commit before it*

`ANKERL_UNORDERED_DENSE_FORCEINLINE` on `probe` moved the full score against main under gcc from 1.149 to **1.244** with SSE2 (`build64` 1.97, `buildbig` 1.68) and from 1.097 to **1.165** without. The gcc string lookups, the one workload family behind main, are now ahead of it: `rhitstr` 1.13, `rmissstr` 1.05, `findstr` 1.10.

How it was found: the full score without SSE2 under gcc had random integer misses at 0.66 of main, while a standalone loop had gcc's SWAR miss *faster* than clang's. `perf` on the harness binary put 59% of the time in `table::probe<unsigned long>` as its own symbol, called from `find_all`. main's lookup was fully inlined. In a large translation unit gcc's unit-growth budget runs out, and the probe, bigger with the SWAR match, is the function it stops inlining. The symbol exists in the SSE2 binary as well.

Paired against the commit before it, ratios against main:

- **gcc without SSE2**: `rmiss64` 0.66 to 1.23, `rhit64` 1.16 to 1.50, `churn64` 1.19 to 1.48.
- **gcc with SSE2**: `churn64` 1.32, `rhitstr` 1.22, `build64` 1.42.
- **clang**: 1.00 on every workload, with and without SSE2, since it inlined the probe already.

The whole design assumes the probe is inlined: the prefetch, the hoisted pointers and the early exit only pay inside the caller. The attribute says what the code already meant.

### The gcc string-lookup gap, explained and mostly closed

*2026-09-05 · no issue · kept · paired, both compilers*

A 256-entry table of finished fingerprint words replaced the computation of the word, and the result was kept. Paired, both compilers: integer misses 1.05-1.06x, `findbig` 1.03x clang and **1.14x gcc**, `rhit64` 1.02x gcc, `ie64` 1.03x gcc, strings 1.00-1.01.

The gap: under gcc the branch's string lookups measured 0.88-0.91 of main, the one workload family it lost. Per string hit, main ran 200 instructions and the branch 224 under gcc, against 235 and 239 under clang. Branch misses were identical at 1.8. gcc compiles main's robin hood probe unusually tightly. On string keys there are no probe mispredictions for the group index to win back (the 1.8 are the hash's length dispatch and `memcmp`), so the group probe's extra instructions show undiluted.

The assembly has the shape CLAUDE.md recorded for the old SSE2 probe: gcc spills the loop state, the broadcast fingerprint among it, around the `memcmp` call. That plus the prefetches, movemask, tzcnt and the second array are the two dozen extra instructions. Five of them were fixable. Building the fingerprint word was an and, a compare, a shift, an or and a multiply on the critical path of every probe, placement and erase. A 256-entry table of the finished words makes it one L1 load. The prototype had that table and the header had lost it.

The rest of the string gap under gcc was the probe not being inlined at all in that binary, and is closed (see [gcc left `probe` out of line](#gcc-left-probe-out-of-line-and-forcing-it-inline-is-the-largest-single-gcc-gain-on-the-branch)).

### Where a string hit's ~90 cycles go, from perf, and two more things it led to that lost

*2026-09-07 · no issue · rejected · asked as "try perf"; one map per binary, `find_all<map<std::string, size_t>, true>` at 50000 entries, `cycles:P`, by symbol*

A string hit is the hash's arithmetic and dispatch, one cache miss for the stored key through the value index, and `memcmp`. None of the three has a lever that a hash or compare change can pull. Both experiments the profile suggested lost. Stop here.

| | clang | gcc | what it is |
|---|---|---|---|
| `wyhash::hash` | **41%** | **46%** | the hash, **called out of line**: neither compiler inlines it |
| `do_find_hashed` | 30% | (inlined into the harness) | the probe: group compare, index load, the stored key's size, the compare's setup, and its own prologue and epilogue. It is an out-of-line call too |
| `__memcmp_evex_movbe` + plt | 11% | 12% | the key compare |
| the harness | 17% | | rng, key selection |

Inside the hash, by line: ~25% on the multiplies, ~16% draining at the finalizer (the tail of the chain, where a latency-bound pipeline empties), ~15% on the length compares (mispredict skid), ~10% on the loads. Branch misses: **1.1 per lookup**, 829M branches per 30M lookups.

**The per-instruction miss attribution is not to be trusted here, and the check that shows it is cheap.** `perf annotate` put 0.30 misses per lookup on the string `operator==`'s size check. That is a `jne` on `cmp 0x8(%rbp,%rax,8), %r15`, the stored key's size loaded from the value vector at a random index. It put 0.32 inside `memcmp` on its `vpcmpnequb` instructions. A counting `KeyEqual` says the size check actually fails **0.025 times per lookup** and the content compare 0.0005, and that `jne` has zero *cycle* samples. Both attributions are skid. On this machine (IBS) a miss lands on the first branch after a load that is waiting on memory, and the size load is a cache miss on every lookup. So the symbol-level split is roughly right and the instruction-level one is not. The probe's own branches are clean: 2.5% of lookups see a wrong-sized fingerprint collision first, which the predictor absorbs.

Two things the profile pointed at, both measured in the map with a same-code control:

- **Force-inlining the hash** (the restated body under `always_inline`, the lane path left out of line, confirmed inlined by `nm`): clang `rhitstr` 1.015 against a control of 1.040, `findstr` 0.991 against 1.011, so nothing, or slightly worse. gcc 1.035 against 1.023, `rmissstr` 1.059 against 1.010: one to four percent, inside a control that swings up to 10% in this binary. The 41% is the hash's work, not its call.
- **A block-structured string compare in place of `memcmp`**, eight bytes at a time, with length branches at the hash's own thresholds so the predictor has just seen their outcomes: **3-7% behind the control on hits under both compilers** (clang 0.969 against 1.046, gcc 0.970 against 1.030). libc compares 32 bytes per instruction. The "0.32 mispredicts inside memcmp" that motivated this were the skid above.

What is left is the shape of the thing. The hash's arithmetic and dispatch are at their floor (four experiments). The cache miss for the stored key is the dense design's structural extra hop, documented since the first chart. And libc does `memcmp` well.

### Seven ideas from reading folly F14 and from the F14Vector string gap, all measured on 2026-09-07, none kept

*2026-09-07 · no issue · rejected · asked as "where does our map differ from indivi", "how can F14Vector beat us at string", "read folly for tricks", "try the 12 fingerprint layout", "find more ideas"; one map per binary, `perf stat`, instruction counts; harness `scripts/ab/maps_one.sh -k str`; minimal single-map binaries `/tmp/hb.cpp` (lookups), `/tmp/rh.cpp` (`rehash(0)` on a built map), `/tmp/ins.cpp` (reserved inserts)*

None of the ideas was kept. The method is the one prescribed for anything under 10%: one map per binary and instruction counts as the number that cannot be argued with.

**Where the F14Vector string gap actually is.** Paired octave geomeans had F14VectorMap 6-12% ahead on string lookups and `unordered_dense` 1.27x ahead overall on integers. At 32000 entries, one map per binary, the hit is a 2% tie (137.2 against 134.4 cycles) and the **miss is 9.5%** (103.8 against 94.8). The same binaries on `uint64_t` keys have `unordered_dense` 17% ahead on the miss. What it is not:

- Not the hash. The harness hands F14 our wyhash, and `perf record` puts the identical `wyhash::hash` symbol at 44.2% and 44.6% of the two binaries.
- Not the load factor. Both have 4096 groups holding 7.8 entries each at that size; F14's `bucket_count()` of 40960 is `chunkCount * capacityScale`, not slots.
- Not the value indirection, which F14Vector has too.

It is **clang leaving `do_find_hashed` out of line for `std::string` keys** and inlining it for `uint64_t` (`nm` on the two binaries), plus our two index prefetches, which on a miss with no fingerprint match are pure waste (1.1 of the 1.3 extra L1 fills per miss). Force-inlining it: 103.8 to 99.1 cycles at an *identical* instruction count, half the gap. Removing the prefetches adds nothing on top of that. The remaining 4.5% is eight instructions of ordinary difference between two probe loops.

**Later (2026-09-10):** on that day's header `nm` shows the current map's probe inlined in both the one-map and the sixteen-map binary, so the out-of-line diagnosis no longer applies and the +8 instructions are explained elsewhere, see [The F14Vector string miss, re-measured](#the-f14vector-string-miss-re-measured-and-the-old-explanation-of-it-is-wrong).

**Force-inlining `do_find_hashed`, paired on the score: not kept.** clang 0.9975, with `rmissstr` 1.022 and `rmiss64` 1.016, in a run whose `hashstr` control read 1.106. **gcc 0.9946, with `rmiss64` 0.844 and `rhit64` 0.948, against a clean control of 1.001.** gcc was inlining it already, so the attribute can only have moved the inlining of what surrounds it in that translation unit, and it moved it the wrong way by more than the clang gain. A `#if defined(__clang__)` would take the 4.5% on string misses. Not done: a compiler-conditional inlining attribute on the lookup is the kind of thing the next compiler release silently reverses.

**Splitting the hash into an inlinable short path and an out-of-line tail** (`hash()` for `len <= 16`, `hash_long()` `noinline` for the rest, `secret` hoisted, values identical by checksum over lengths 0-1200): **a loss under clang in every configuration, including all-short keys**, where it removes the call outright.

| | clang cycles | clang instructions | gcc |
|---|---|---|---|
| 8-16 byte miss (all short) | 43.9 to 45.6 | +6 | −4% |
| scored lengths | 103.4 to 108.8 | +18.6 | 0 to +2% |

The 24 instructions of the short path, inlined into a caller that has the probe's state live, cost spills. That is the `do_place_element` mechanism again. The earlier force-inlining of the hash read "nothing, or slightly worse" in a paired run that could not resolve it (see [Where a string hit's ~90 cycles go, from perf](#where-a-string-hits-90-cycles-go-from-perf-and-two-more-things-it-led-to-that-lost)). This measurement can, and the sign is the same. Both binaries in the F14 comparison call the identical out-of-line hash, so nothing done to the hash could have moved that ratio anyway.

**folly's `fullness[]` byte array in the rehash** (`allocateTag`: one occupancy byte per chunk on the stack, so placement never loads the destination chunk's tags): a loss of up to 22% on the loop in ten of twelve cells, and a 1-3% win in the other two (corrected 2026-10-02: said "a 3-20% loss on the loop"). Isolated `rehash(0)`, ns per element, u64/str:

| | 200K | 1M | 4M |
|---|---|---|---|
| clang, shipped | 1.66/3.92 | 1.73/4.19 | 6.60/8.97 |
| clang, `fullness[]` | 1.72/4.06 | **1.89/4.91** | 6.54/9.48 |
| gcc, shipped | 1.85/4.48 | 1.81/4.63 | 6.46/8.93 |
| gcc, `fullness[]` | 1.92/4.36 | **2.21/5.40** | 6.71/**10.37** |

It trades a random load that the sixteen-ahead prefetch already hides for a random load nothing prefetches, plus a store that the next element to the same group has to forward from. F14 needs it because its rehash is not pipelined.

**The 12-slot, 64 byte, cache-line-aligned block** (F14VectorMap's `kCapacity = 12` for 4 byte items: 12 fingerprints + 4 counters + 12 x 4 byte indices, `alignas(64)`, counter class `& 3`, the top four lanes of the compare masked off, no index prefetch). It does exactly what the L1 counters predicted and nothing else: 1.4 fewer L1 misses per string lookup (5.27 to 3.89 on a miss, 8.35 to 6.95 on a hit) and **cycles unchanged to the tenth** (103.3 to 102.5, 135.1 to 135.7). The lines it saves were being prefetched, so they were never on the critical path. Paired on the score: **0.9888**. Lookups +1-2% (`find64` 1.024, `rhit64` 1.014, `findstr` 1.011); churn and insert-erase −5-7% (`churn64` 0.925, `churnbig` 0.936, `ie64` 0.955), because a 12-slot group is full more often at the same load and four classes filter a churned miss worse than eight. Also found: for a one byte value index, 12 slots make the group count non-power-of-two (256 / 12 = 21 groups), which is heap corruption via `hash >> shifts` and an endless `calc_shifts_for_size`. `bucket.cpp`'s `group_micro` cases caught it. Not fixed, since the layout is rejected.

**Prefetching the back element's string body before an erase's probe**, so the hash of the moved key overlaps the probe instead of following it (a `prefetch_key` hook, a no-op except for `std::basic_string`): string churn 436.7 to 429.1 cycles and insert-erase 680 to 674 at 32000; nothing at 200000 (1565 to 1577, 2502 to 2526). 1-2% in cache, nothing out of it, the same shape as the erase-side pull-back. Not kept.

**Fusing the insert's probe with its placement** (`probe_for_insert` also returns the home group's empty-lane mask, so a miss whose home has room places without the second walk; no counter can have moved because no full group was passed; anything else falls back to `place_group`): **more instructions, not fewer**. Reserved u64 inserts: clang 97.8 to 101.0, gcc 65.9 to 70.0, gcc cycles 16.4 to 17.7. The `match_empty` on home is paid on every insert probe and the two extra live values cost spills, while the second walk it removes was an L1 hit on a line just loaded.

**The one fact left standing is the clang/gcc gap itself**, and it is not inlining. On identical source, a reserved `uint64_t` insert is **97.8 instructions under clang and 65.9 under gcc**, a string miss 142 against 110, an 8-16 byte string miss 115 against 79. Force-inlining `do_try_emplace` or `do_find_hashed` leaves clang's count exactly where it was (97.8, 143.2). The disassembly shows clang spilling the loop state at function entry (six pushes, the broadcast, counter, mask, delta, `this`, the groups pointer), where gcc sinks the same spills into the fingerprint-match branch that a miss never takes. That is a register allocator's choice and not something a source change has been found to steer; every attempt above that moved code into a caller made it worse.

**Later (2026-09-10):** listing the instructions one by one showed this explanation was wrong: the clang/gcc gap is the call boundary, and PGO removes all of it, see [The insert path's instructions, counted one by one](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it).

### The SSE probe audited, and its one real redundancy is compiler-dependent

*2026-09-07 · no issue · kept (left as it is) · asked as "take a good look at the SSE code, is there anything that can be optimized there"; one map per binary, 30M all-hits lookups*

The match sequence has nothing left in it: `movdqu`, `pcmpeqb` against a broadcast that *both* compilers hoist out of the loop (checked in the disassembly, not assumed), `pmovmskb`, `test`. The broadcast is only that cheap because `fingerprint_words` already holds the byte in all four positions. So `_mm_set1_epi32` is `movd` plus `pshufd`, where a genuine SSE2 byte broadcast would also need a `punpcklbw`. `match_empty` compiles to a `pxor`-zeroed compare, `lanes &= lanes - 1` to `blsr`, and `first_lane` to `tzcnt`.

The redundancy is in `prefetch_index`, which asks for `p + 64` and `p + sizeof(block) - 1` = `p + 87`. With an 88 byte stride a block's offset within a cache line cycles through eight values, and **six of the eight put both prefetches on the same line**, so one of them is waste three times in four. Removing it is a clang win and a larger gcc loss. ns per hit, both prefetches / last only / `+64` only / none:

| | 50000 | 1000000 | 4000000 |
|---|---|---|---|
| clang | 6.47 / **6.03** / 6.16 / 6.15 | 29.71 / **26.42** / 27.94 / 28.03 | 51.73 / 49.05 / **48.69** / 49.48 |
| gcc | **5.78** / 5.87 / 5.91 / 6.17 | | **44.79** / 45.64 / 45.35 / 51.28 |

Dropping both costs gcc **12% at four million entries**: 313.5 cycles per lookup against 277.9, while executing *fewer* instructions. That is a cache miss that stopped being hidden. The cause is scheduling, not source. gcc emits the `movdqu` before the two prefetches; clang emits both prefetches before it, so under clang they take load-port slots in front of the load that is on the critical path. **Left as it is**, because the gcc gain is larger than the clang cost and both compilers are in CI. **Correction (2026-10-02):** the 12% gcc figure is for dropping *both* prefetches; the table above says dropping one is a clang win and a 1-2% gcc loss, the reverse of this sentence. Re-measured one header per binary, dropping either line is within 3% on both compilers at every size, so the decision stands on that, see [`prefetch_index`'s two lines re-measured](#prefetch_indexs-two-lines-re-measured-one-header-per-binary-dropping-either-is-within-3-on-both-compilers-from-50000-to-16m-entries-so-both-stay-but-not-for-the-reason-the-audit-gave). Misses are unaffected under either, which fits: a miss with no fingerprint match never reads the index. The paired score could not settle this. It read 1.006 for the single-prefetch variant in a run where `hashstr`, which never touches the map, read 1.13.

This answers the x86 half of the question the boost review left open ("boost tunes the prefetch per architecture ... this map issues the same two or three prefetches everywhere ... it has not been asked", see [Read boost's `unordered_flat_map` again after it turned out ...](#read-boosts-unordered_flat_map-again-after-it-turned-out-to-have-the-probe-bound-this-map-was-missing)). For x86 there is nothing to tune that is right for both compilers. The ARM half is still unasked, and `bench.yml` could settle it.

One trap: `-march=native` silently upgrades these intrinsics to AVX-512 on this machine (`vpcmpeqb` into `%k0`, `kmovd`, no `pmovmskb` at all). A profile taken that way is not the code most callers run.

### Splitting the probe past the home group: kept, for keys whose compare is a call

*2026-09-10 · #233 · kept · one map per binary, instructions per lookup, clang 22 and gcc*

The split is gated on `detail::key_compare_is_call<Key>`. `std::string` misses drop from 140.6 to 128.8 instructions (-8.4%) and from 19.65 ns to 17.85, and **integer codegen is byte-identical to the shipped header** on both compilers.

The same split was tried earlier on 2026-09-10 and reverted (see [The F14Vector string miss, re-measured](#the-f14vector-string-miss-re-measured-and-the-old-explanation-of-it-is-wrong)): string miss 138.3 to 126.5 instructions, integer miss 55.8 to 65.9 and integer hit 69.1 to 85.0, "it buys 8% of the one workload we lose and spends 20% of the ones we lead by most". Both halves of that reproduce exactly. What was missing is that the **key type** selects which half applies, so they can be had separately.

**Why the frame is there.** For a `std::string` key the probe builds a frame and spills the loop state (the counter, `this`, the mask, the delta, the hash and the broadcast fingerprint) before the first group is compared. The key compare is a `memcmp` call, and everything the loop keeps live has to survive it. Only about 3% of lookups leave the home group, and the delta and the mask are the only things the rest of the walk needs. So every lookup pays the frame for a path almost none of them take. Where the compare is a register compare there is no call, nothing has to survive anything, and splitting only adds a call.

**Measured per key type**, one map per binary, instructions per lookup, clang 22 (miss):

| key | shipped | split | |
|---|---|---|---|
| `std::string` | 140.6 | **128.8** | **-8.4%**, and 19.65 ns to 17.85 |
| `std::string_view` | 134.2 | **122.4** | **-8.8%** |
| `std::uint64_t` | 56.1 | 67.1 | +20% |
| `std::pair<std::uint64_t, std::uint64_t>` | 50.2 | 70.2 | **+40%** |
| 64 byte POD, `memcmp` compare | 111.4 | 109.4 | -1.8% |

Integer instructions before and after, on miss, hit and half: 56.1 / 59.9 / 70.0 under clang and 58.1 / 57.0 / 68.6 under gcc. The `map<uint64_t, big_value>` workloads are identical to the tenth as well. Strings gain everywhere:

| string workload | clang | gcc |
|---|---|---|
| miss | -8.2% | -4.0% |
| half | -4.3% | -2.3% |
| hit | -1.7% | -1.4% |
| insert | -3.2% | -3.0% |
| churn and insert-erase | -1 to -2% | -1 to -2% |
| `++m[k]` on a present key (the one loss) | +2.2% | +0.3% |

Timings, three runs each, medians, every cell improves:

| ns | miss | hit | half |
|---|---|---|---|
| clang | **19.65 to 17.85** | 25.00 to 24.55 | 24.60 to 24.00 |
| gcc | 16.15 to 15.80 | 22.95 to 22.65 | 22.45 to 22.35 |

**The trait is `is_trivially_copy_constructible`, and the difference from `is_trivially_copyable` is a 40% bug.** `std::pair` and `std::tuple` write their own copy *assignment*, so both libstdc++ and libc++ report `is_trivially_copyable_v<std::pair<std::uint64_t, std::uint64_t>>` as **false**. The first version keyed on that and split one of the commonest key types after string and integer. `std::string_view` is the other way round: trivially copyable and still a call. No trait separates it from `pair<uint64_t, uint64_t>`, since both are sixteen bytes and hold no indirection the language can see. It is named explicitly.

A pair and a tuple are then asked about their *elements*, not about themselves. CI found this, not reasoning: **MSVC's `std::tuple<int, int>` is not trivially copy-constructible where libstdc++'s and libc++'s is**. Asking the aggregate gave a tuple key a different probe on Windows than everywhere else, and all four Windows legs went red on the `static_assert` that pins it. Recursing is also what comparing a pair does. A user type that is trivially copy-constructible and still compares through a call is treated as cheap and loses the ~2% in the last row of the per-key table, which is the harmless direction to be wrong in.

**And the shape of the code is load-bearing.** Factoring the per-group compare into a helper returning a `probe_result`, so that the split and unsplit paths share it, is tidier and costs **gcc 26% of an integer hit** (57.0 to 72.0) and clang 6.7%, fully inlined, purely from returning a twelve-byte struct through an inner function. The loop is written out instead. The unsplit path is the shipped loop unchanged, which is what makes the integer columns identical, not merely close.

**Mutation swept, and the one hole it found is closed.** The split duplicates the probe's "did anything of this class overflow past me" test into an inline home-group check. Turning that `== 0` into `== 1` was **caught by nothing**. It stops a probe at home whenever exactly one entry of the key's class went past, so that entry becomes unfindable. Reaching it by chance needs a string whose home counter happens to be one. `test/unit/probe_split.cpp` reaches it on purpose with `fuzz_group_index`'s construction: an identity hash and a key type whose copy constructor is user provided, so the key names its group and its fingerprint class *and* takes the split path. Fill group 0, send one key of class 3 past it, look that key up. The other survivors:

- performance-only: dropping the `!` on the gate, deleting a prefetch, `||` to `&&`, deleting an early return;
- writes to `slot` and `value_idx` in a result whose `found` is false, which the type documents as meaningless;
- `m_group_mask == 0`, equivalent, since the smallest array is four groups and the disjunct never fires either way.

**What this does not claim.** The scored benchmark's string workloads are a third of it, so the score moves by about 1%; that is not the argument. The argument is the instruction counts, which neither code layout nor drift can move, and the rule that a paired A/B cannot measure a change that moves an inlining boundary. Against F14VectorMap, the one lookup this map lost, the string miss goes from 140.6 instructions to 128.8 against F14's 131.4: ahead on instructions, still 2.3% behind on time.

### A block's prefetches should step from its start, not jump to its end

*2026-09-11 · #250 · kept · `scripts/ab/prefetch_lines.cpp`, Ryzen 9 7950X, clang 22*

`prefetch_block` now asks for `p` and `p + 64` instead of `p` and `p + 87`, worth 3.2% on a past-cache block walk (corrected 2026-10-02: said "3.3%"). The review that raised it said "a quarter of blocks span three cache lines and `prefetch_block` only asks for two". The geometry is right, the first fix for it was wrong, and the second is the one shipped.

*The geometry.* A block is 88 bytes with `alignof == 4`, so blocks sit at `base + 88i`. `88 % 64` is 24 and `gcd(24, 64)` is 8, so `p % 64` walks the entire cycle `{0, 24, 48, 8, 32, 56, 16, 40}` from any starting offset. **No allocation avoids this, and over-aligning the array does nothing.** The two offsets above 40 make the block span three cache lines, so exactly 25% do. For those, the middle line begins at byte 8 or 16 *of the block*: it holds all eight overflow counters and, at offset 56, half the fingerprints too (at 48 the first line holds all sixteen), which is what the probe reads first (corrected 2026-10-02: said "half or more of the fingerprints").

*The obvious fix is a loss.* `prefetch_block` asked for `p` and `p + 87`, the first line and the last. Adding a third at `p + 64`, so all three are named, makes it **slower**: 12.85 ns per block against 12.67. The hardware fetches what it can see coming, and the extra prefetch costs an instruction and a load port slot without buying a line.

*Stepping instead of spanning is the fix.* `p` and `p + 64` are always two consecutive lines. For the three quarters of blocks that span two lines, that is exactly what `p` and `p + 87` already prefetched. For the other quarter it takes the middle line instead of the last, and the last holds only `m_index[14..15]` (offset 48) or `m_index[12..15]` (offset 56), read only when the matching lane is high (corrected 2026-10-02: said "only `m_index[14..15]`"). Same instruction count. Setup: an array of two million blocks (176 MiB, far past the last level cache), walked in a materialised random order with the same sixteen-deep lookahead the pipelined loops use, reading sixteen fingerprints, one counter and one index at a lane derived from the fingerprints, so the index read cannot start before the compare:

| prefetches | median ns/block |
|---|---|
| none | 44.9 |
| `p` | 13.19 |
| `p`, `p + 87`: first and last, the old shape | 12.67 |
| `p`, `p + 64`, `p + 87`: all three | 12.85 |
| **`p`, `p + 64`: stepped, shipped** | **12.28** |

Seven rounds of seven, with no overlap between the stepped and the old distributions. It reproduces on the real harnesses at four million entries: the bulk visit 0.971 and a reserved range insert 0.952, five of five below 1.0 each. It is smaller but the same sign at a quarter million and in grow mode.

So `prefetch_block` is now a loop stepping by 64 to the end of the block, which both compilers fully unroll (two prefetches, no branch). **The loop is what makes it right for `group_big`**: 152 bytes spans up to four lines, and first-and-last covered two of them and skipped as many as two in the middle. Stepping gives it three.

**Later (2026-09-12):** three lines was the wrong answer for `group_big`; capping the loop at two consecutive lines reads 15.814 against 16.169 ns/block, see [`prefetch_index` was a line short for `group_big`](#prefetch_index-was-a-line-short-for-group_big-and-covering-that-line-is-not-the-fix).

*What nearly went wrong.* The third-prefetch version, measured on the three real harnesses first, read -1.3% on the bulk visit and -2 to -3% on the range insert, and looked like a win. Five rounds at each size put all of it between 0.975 and 1.017, **inside the +-3% layout luck**. The L1 miss counts did not move either (44.6M against 44.9M at four million). A provably true mechanism does not make a delta of that size real. The microbenchmark separated the two candidate fixes: on the real harnesses both are a smear, and the difference between them is 4.6% (12.85 against 12.28) (corrected 2026-10-02: said "3.3%").

**Later (2026-09-12):** this entry's harness picked the prefetch mode inside the timed loop, so all its numbers carried that dispatch; re-taken on the fixed harness they keep the same ordering and the same 3% (12.336 against 12.719), see [`prefetch_index` was a line short for `group_big`](#prefetch_index-was-a-line-short-for-group_big-and-covering-that-line-is-not-the-fix).

*The score does not move and should not.* Paired against main, ten epochs, five points an octave: geomean **1.001** over nineteen workloads, everything inside 1.5% except `rmissstr` at 1.039. That one is **not attributable**. A lookup's probe calls `prefetch_index`, which this did not touch, and `prefetch_block` is reached on the score only through the rehash inside `build` and `churn`. Two runs agreeing on it mean nothing either, since they are the same two binaries and therefore the same code layout. It is the +-3% layout luck, measured slightly above its usual size. The change's actual subjects (the bulk visit, the range insert, `replace()`) are not in the score at all, which is why the microbenchmark carries this one.

One machine. `prefetch_lines.cpp` is committed so that a CPU with different prefetchers can be asked the same question, which is the one thing that would change the answer.

### `probe_result`'s shape swept two ways, a third argued down from the return sequence, and the one that ships is the best of them

*2026-09-12 · #267, #262 · rejected (shipped shape kept) · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/solo.sh` and `scripts/ab/perwl.sh`, one header per binary, five alternating rounds for the score; instruction counts need none*

Both narrower shapes cost the `std::string` find and buy nothing anywhere: dropping `value_idx` costs 4.1% more instructions under clang, folding `found` into a sentinel 1.1%. The shipped `probe_result` stays.

Why the sweep: #262 repacked `probe_result` and the `std::string` 50%-find row moved 0.9749 in instructions under gcc. The opposite is also recorded: a `probe_result` returned from an inner function cost gcc 26% of an integer hit (see [Splitting the probe past the home group](#splitting-the-probe-past-the-home-group-kept-for-keys-whose-compare-is-a-call)). Two accidents, opposite signs, no sweep. #267 asked for one: does the caller need `value_idx` back, is `found` better as a sentinel, is an out-parameter or a single word better, and does the out-of-line `probe_past_home` want a different shape from the inlined `probe_from`.

**Instructions per operation, candidate over baseline, so below 1.00 is fewer.** The two variants:

- the whole struct narrowed: `value_idx` dropped, the caller re-reading `m_index[lane]` from the slot the probe stopped at. Twelve bytes to eight, two return registers to one.
- `found` folded into `lane` as a `0xFF` sentinel. Still twelve bytes; the bool sat in padding, so this tests the store and the caller's test of it.

| | no `value_idx`, clang | gcc | `found` as sentinel, clang | gcc |
|---|---|---|---|---|
| `std::string` 50% find | **1.0407** | 1.0041 | **1.0109** | 1.0038 |
| `std::string` churn | 1.0130 | 1.0087 | 0.9978 | 1.0014 |
| `std::string` insert erase | 0.9976 | 1.0185 | 1.0032 | 0.9985 |
| `uint64_t` build | 1.0136 | 1.0154 | 1.0000 | 1.0000 |
| `uint64_t` churn | 1.0064 | 1.0142 | 1.0000 | 0.9971 |
| **`uint64_t` 50% find** | **1.0000** | **1.0001** | **0.9998** | **1.0001** |
| score, one header per binary | 0.9956 | 1.0001 | 0.9954 | 0.9970 |

**Both cost the string find and neither buys anything anywhere.** Dropping `value_idx` is the worse of the two, by the larger margin under clang: 4.1% more instructions on the workload the issue is about. `find()` is the shape's biggest consumer, and re-reading the slot costs more than the return register it saves. The insert paths also take `value_idx` out of a probe, and they pay 1.4-1.5% on an integer build for the same reason. The sentinel is a smaller version of the same answer, 1.1% under clang.

**Why there was nothing to win, read off the baseline.** The return sequence of `probe_past_home` for a string key:

    shl $0x20,%r12 ; or %rsi,%r12      group_idx and value_idx into one register or  $0x100,%r13d                   lane and found into the other mov %r12,%rax ; mov %r13d,%edx ; ret

Clang already packs the twelve bytes into the two registers the ABI allows, with two ALU ops and no memory at all. There is no hidden pointer to remove and no spill to save. The only thing a narrower struct can do is make the caller re-derive what it stopped carrying.

**The third variant the issue lists, an out-parameter, was not built, and what follows is an argument and not a measurement.** Against that return sequence it replaces two register moves with a store and a load, which is the narrowing's trade in a more expensive currency. It is the one cell of the sweep that is reasoning. If the shape is ever revisited, it is the one to measure.

**The two probes do want different things, and it is not a shape.** Every effect in the table landed where a return value is formed. A key whose compare needs no call never goes through `probe_past_home` at all, because `probe_from` is force-inlined into the caller. So the struct's shape is free on the integer *find* by construction, which is what that row of the table says. The integer insert paths do move, because they take `value_idx` out of a probe and the narrowing makes them re-read it. The delicacy is the *call boundary*, not the fields. It is worth something only where a boundary exists, and there it is negative for both ways of making the struct smaller.

**What this says and does not say.** It says the shipped `probe_result` is a local optimum on the two axes anyone has proposed. It says the 26% and the 0.9749 were both about crossing the call boundary, not about the struct's fields. And it says `uint64_t` lookups cannot be moved from here at all. It does not say the boundary itself is optimal: `probe_past_home` exists because a string compare is a call, and whether *that* split is still right is a different question from how its result is packed. And it is instructions: the score moved 0.9954 to 1.0001 across the four cells, which is inside this harness's layout band and is not evidence of anything on its own.

### `prefetch_index` was a line short for `group_big`, and covering that line is not the fix

*2026-09-12 · #252, #250 · kept · Ryzen 9 7950X, `scripts/ab/prefetch_lines.cpp`; 152 byte blocks, 304 MiB, probe-shaped reads, sixteen-deep lookahead, medians of seven interleaved rounds from one binary*

Both prefetch helpers now ask for **two consecutive lines from where they start, clamped into the block**. For the 88 byte `group` that is what they already did, and the binary is byte identical. For the 152 byte `group_big` it is `p + 64, p + 128` and `p, p + 64`. Naming the skipped line as a third prefetch does nothing.

The bug. `prefetch_index` asked for `p + 64` and `p + sizeof(Block) - 1`, skipping the first line because the probe is reading the fingerprints out of it. For the 88 byte `group` those two addresses cover every line a block can have past the first; enumerating every alignment, nothing is missed. That is why #250 left this function alone when it fixed `prefetch_block`. `group_big` is 152 bytes and reaches a *fourth* line whenever `p % 64 > 40`. `152 % 64 == 24` and `gcd(24, 64) == 8`, so `p % 64` only takes the eight multiples of eight, and 2 of them are above 40: **a quarter of blocks**. That is the same fraction, by the same argument, that `prefetch_block`'s comment gives for the 88 byte block. First-and-last then skips the middle line: at `p % 64 == 48` it holds `m_index[7..14]`, eight of the sixteen value indices.

| what is asked for | ns/block |
|---|---|
| `p + 64`, `p + 151` (first and last, as shipped) | 15.664 |
| `p + 64`, `p + 128`, `p + 151` (every line) | 15.664 |
| **`p + 64`, `p + 128`** (two consecutive lines) | **15.006** |

Same answer #250 gave for `prefetch_block`, for the same reason: the hardware fetches what it can see coming, so the job is to start it in the right place, not to name every line. The third prefetch buys back nothing and occupies a slot. It holds across the cache boundary, with the same two modes (every line against two consecutive):

| blocks | every line | two consecutive |
|---|---|---|
| 50k | 2.980 | 2.881 |
| 200k | 5.265 | 5.173 |
| 800k | 13.854 | 13.226 |
| 2M, gcc, its own build | 15.60 | 15.00 |

**`prefetch_block` had the same bug and this entry created it.** Its loop runs from the block's start, which is two prefetches at 88 bytes and *three* at 152. For one release the two helpers asked for opposite things about the same block, and `prefetch_block`'s comment claimed "the loop is what makes it right for group_big too" (see [A block's prefetches should step from its start](#a-blocks-prefetches-should-step-from-its-start-not-jump-to-its-end)). Capping the loop at two lines reads **15.814 against 16.169**. The one place it goes the other way is a rehash-shaped read of every byte, 18.887 against 18.707. That is the minority of its call sites: five of the six are lookups, and the rehash issues one prefetch per *element*, not per block.

For `group`, a six function `map<uint64_t, uint64_t>` translation unit compiles byte identical before and after under clang *and* gcc. That is the evidence that this costs the default type nothing, and it is stronger than any timing. Dropping the second prefetch entirely, instead of clamping it, would cost the 88 byte block 5%: 13.231 against 12.582.

The 88 byte block is also where the harness checks itself. `pair` and `steptail` name the *same two addresses* there, and they read 12.582 and 12.578, so on this build the floor is a few hundredths of a nanosecond. It is not always that good. The modes are separate instantiations, and between two builds of the file the same mode moves by a percent or two, which is why every comparison here is interleaved inside one binary.

End to end, one header per binary against `main` under clang: the `group_big` score (`bench_quick_overall_udm_bigbucket`, the fifteen workloads nothing else in the tree runs) reads **1.0042** over five rounds, and the default score **1.0033** over five, both candidate-faster.

**The harness was charging its own dispatch to the prefetch shape.** `prefetch_lines.cpp` chose what to prefetch with `how == "pair"` inside the measured loop: a `std::string` compare per iteration, and a different number of them per mode. Two modes that name the *identical* pair of addresses on the 88 byte block read 14.11 and 13.43 ns/block. The mode and the read shape are template parameters now. All of #250's numbers carried that overhead, so they were re-taken on the fixed harness: `p, p + 64` 12.336, `p, p + 87` 12.719, all three lines 12.736, `p` alone 13.153. Same ordering and the same 3% #250 reported (12.28 against 12.67), so its conclusion survives its harness being wrong.

### An empty table reads a shared, never written sentinel index, so `find` and the insert's inlined lookup drop their `empty()` test: `find` up to 5% fewer instructions (3-5% for integer keys on both compilers; clang's big-value find +0.9%), gcc's small-table counting now ahead of 4.1.2, clang's 0.1-0.3 cycles closer, the score level; the index became a pointer and a count, so the map is 64 bytes, 8 fewer than 5.2.0

*2026-09-27 · #329, #331 · kept · Ryzen 9 7950X, clang 22 and gcc 16; the caller corpus, `solo.sh` + `perwl.sh`, #331's counting loop*

**Correction (2026-10-02):** the heading said "`find` 3-5% fewer instructions on both compilers". The table below has clang's big-value `find` at 1.009 (0.9% more) and gcc's string `find` at 0.981.

The design. The sentinel is a static array of as many groups as the smallest table has (4), all slots empty and all counters zero. `group_storage`, which the library owns, holds a pointer `m_data` that is `m_blocks.data()` or the sentinel. `sync()` sets it after every operation that can change the vector, so `data()` returns it without a test. A test inside `data()` would have put the check back on every lookup's address, which is why the pointer is stored. A `static_assert` in the table ties the sentinel's size to `initial_shifts`. Every path that leaves a table without an array sets `m_shifts` back to it (the moves, `copy_buckets` for an empty source, `reset_to_empty`), so `hash >> m_shifts` stays inside the sentinel.

After the review the sentinel moved out of `group_storage` to one per group type (`detail::sentinel_blocks<Group>()`, 352 bytes of BSS), not one per group type and allocator. The `empty()` tests left in the precomputed-hash `find` and in `erase(key)` went too. `visit` keeps its test, which saves hashing a whole range.

What keeps it unwritten: the miss path allocates before it places. An attempt to rely on the other guard alone was measured and taken back. That guard is that a table without an array has a capacity of zero, so a placement grows first. But then the first insert grows into the array and rehashes, which the suite caught twice: as the first array coming out 8 groups instead of 4 (`bucket_count() == 64`), and as the first key hashed twice (`transparent.cpp`'s hash counts). With the allocation back, the mutation "deallocate_buckets keeps the capacity" is equivalent and was not added to the bug files.

`the_sentinel_index_is_never_written` in `test/unit/lazy_bucket_allocation.cpp` checks every way to arrive at a table without an array (default, moved from, move assigned from, copy assigned from an empty table), lookups, erases and a first insert in it, and that a fresh table afterwards still finds nothing. It kills the mutation that drops both guards (no allocation in the miss path, no growth for the first element): the first insert writes the sentinel and the test crashes. Dropping either guard alone survives it, because the other one still allocates first.

Score, one header per binary, against main: 1.0032 under clang and under gcc (inside the band). Instructions per workload, u64 / string / big value:

| | clang | gcc |
|---|---|---|
| `find` | 0.964 / 0.970 / 1.009 | 0.952 / 0.981 / 0.952 |
| churn | 0.975 / 0.988 / 0.953 | |
| insert-erase | | 0.997 / 1.003 / 0.962 |
| build | 0.987 / 0.997 / 0.989 | 1.006 / 0.998 / 1.000 |

The caller corpus: worst ratio 1.09 under clang on both headers, 1.09 -> 1.03 under gcc; nearly every cell 1-3 instructions fewer. One cell went the other way: clang's `struct_key` at 64k, 88.2 -> 94.2 instructions and 46.1 -> 50.1 cycles (2% at 4M). The `empty()` test is gone from its loop, and clang now reloads a variable from the stack inside it. That is a register allocation of this one loop, not a property of the change.

#331's counting loop, cycles per row, main -> this (4.1.2):

| | AdvEngineID | CounterID |
|---|---|---|
| clang | 10.08 -> 10.00 (9.02) | 10.88 -> 10.55 (9.35) |
| gcc | 9.52 -> 9.05 (10.57) | 9.73 -> 9.27 (9.28) |

What this says and does not say: an empty table now hashes the key of a lookup that finds nothing, where before it returned before hashing. The insert path and every other lookup save the test.

The size of the map. The stored pointer first cost every table 8 bytes: `sizeof(map<uint64_t, uint64_t>)` 72 -> 80. `group_storage` then became a pointer and a group count instead of a `std::vector` plus that pointer. The table never grows the array in place, so the vector's capacity was a word nobody read, and its data pointer was a second copy of `m_data`. The map is now 64 bytes on a 64 bit target, 8 fewer than 5.2.0, for both bucket types and every key type; `pmr::map` 88 -> 80, `segmented_map` 80 -> 72. The allocator sits in an empty base when it is empty. Propagation on copy, move and swap follows `std::vector`'s rules, because the table's assignments rely on them. `clear()` now frees the array, which is what every caller did next anyway.

A fancy pointer (`boost::interprocess`'s `offset_ptr`) stores null instead of the sentinel, and `data()` tests for it: a raw pointer to a static, stored in a map that lives in shared memory, means nothing to the next process.

**Correction:** the first version of this entry stored a raw `m_data` for every allocator and had exactly that flaw, which no test covers.

MSVC's debug build allocates a proxy per `std::vector`, so the index no longer adds one to an empty table's allocation count there (`vector_count`, 1 -> 0). The storage offers only what the table does with it. It is neither copied nor moved as a whole. `take()` moves an array between two equal allocators (the table checks, or built the source itself), and `set_allocator()` is copy assignment's pocca. A mutation run over the first version, 90 mutants, found the general copy and move assignment branches unreachable, and found that no test would notice a destructor that leaks or a swap that forgets the allocator. `the_index_frees_through_the_allocator_that_allocated_it` now kills both. Score, one header per binary, against the sentinel with the vector: 1.0033 clang, 1.0066 gcc, three rounds each. The equivalent survivor in the mutation run over the diff is the `static_assert`'s comparison, since both sizes are 4.

### #341, main's `find` hit against 4.5.0's in Redpanda's loop: half of the +20-27 instructions per call is the group probe itself (+11-14 in a bare loop) and half the caller around it; boost's `unordered_flat_map` runs as many instructions and 9-16% fewer cycles in that loop, and three candidates -- the walk past home out of line, no index prefetch, a speculative value prefetch from a preferred lane -- each cost cycles or did not move them, so nothing changed

*2026-09-28 · #341, #317, #250, #252 · rejected · Ryzen 9 7950X, clang 22 and gcc 16, boost 1.90, abseil master; Redpanda's loop is `redpanda_lb.cpp` with `-DUNIQUE_GROUPS` from #317, the loop's share taken as a whole run minus a construction-only run, `perf stat`, 20 rounds; the bare loop is the same 92160-entry `map<int64_t, 8 byte value>` looked up with keys drawn beforehand, 2^23 finds counted in-process*

Nothing in the header changed. #341 stays open for whatever explains boost's fewer cycles. **Later (2026-09-28):** explained by the value index past L2 and by codegen inside it, see [Why boost's `unordered_flat_map` finds faster (#341)](#why-boosts-unordered_flat_map-finds-faster-341-it-needs-no-value-index-so-the-first-thing-a-lookup-touches-is-1-byte-of-metadata-per-slot-against-this-maps-55-which-leaves-l2-at-a-far-smaller-table-from-46080-entries-to-460800-this-map-takes-17-21x-boosts-l3-fills-per-find-and-14-35-more-cycles-under-clang-093-104-under-gcc-while-in-l2-it-is-compiler-codegen-alone-this-maps-cycles-083-088-of-boosts-under-gcc-122-125-under-clang).

Per `generate_reassignment()` call (one `find` that hits, plus Redpanda's RNG and swap), and per bare `find`:

| | Redpanda loop, clang instr / cycles | gcc | bare loop, clang | gcc |
|---|---|---|---|---|
| 4.5.0 | 146.3 / 108-110 | 152.3 / 121-122 | 23.3 / 12.3 | 25.3 / 13.6-13.7 |
| main | 173.6 / 125-128 | 172.5 / 136-137 | 37.0 / 15.6-15.8 | 36.0 / 14.8-14.9 |
| `boost::unordered_flat_map` | 178.9 / 114.4 | 159.5 / 115.3 | 36.0 / 11.7 | 39.0 / 14.3 |
| `absl::flat_hash_map` | 177.5 / 105.7 | 192.8 / 125.1 | | |
| A: walk past home out of line for every key | 163.6 / 128.4 | 169.0 / 134.8 | | |
| P: no `prefetch_index` (diagnostic) | 171.1 / 126.3 | 166.5 / 133.0 | | |
| S: speculative value prefetch | 185.6 / 126.9 | 183.0 / 139.7 | 45.0 / 17.4 | 43.0 / 17.1 |

(Ranges are the same binary measured in two sessions.)

The split: the bare loop puts main's own hit at +13.7 (clang) / +10.7 (gcc) instructions over 4.5.0. The rest of the +27 / +20 in Redpanda's loop is the caller: the `end()` comparison, six of the loop's values reloaded from the stack every call, and the RNG's constants moved and rematerialized. main's hit is 36 instructions: the hash, the fingerprint word's table load, broadcast, `imul` by the 88 byte block, two prefetches, the 16 byte compare, movemask, tzcnt, the index load and the key compare. The counter and the walk's delta are computed before the hit test, although only a miss uses them. 4.5.0's hit is one scalar compare of the bucket's distance-and-fingerprint, then the key.

The candidates:
- A (`probe_past_home` for every key, not only one whose compare is a call): 10 fewer instructions under clang and 2.5 more cycles; gcc -3.5 instructions, -1 cycle.
- P (no index prefetch): clang -2.5 instructions, cycles level; gcc -6 and -2.8 cycles. It is the index prefetch's measured win on larger tables (gcc 12-14% at four million entries, see [The SSE probe audited](#the-sse-probe-audited-and-its-one-real-redundancy-is-compiler-dependent)), so it is a diagnostic here, not a proposal (corrected 2026-10-02: said "#250/#252's measured win").
- S (each key prefers the lane its fingerprint's low four bits name, and is placed there when that lane is free; `probe()` reads that lane's value index and prefetches the value before the match): engaged, with 89% of hits in their preferred lane at 80 entries, 67% at 92160 and 64% at 200000. It costs +8 instructions and +1.7-2.2 cycles in the bare loop, +2 (clang) and +4 (gcc) cycles in Redpanda's. The dependent load it was meant to hide is not what these loops wait on: consecutive lookups are independent, and the core already overlaps them.

What this says and does not say: main's hit is more instructions than 4.5.0's, and boost's is as many, so the instruction count is not what separates main from boost. boost's fewer cycles at the same count (9-11% clang, 15-16% gcc in Redpanda's loop; 25% and 3% in the bare one) are not explained here. The two loops are throughput loops (their lookups do not depend on each other), so a latency argument does not apply to them, which is what S showed.

[2026-09-28, #346: the keys here are dense ids. With keys 0..n-1 a multiplicative hash leaves 4.x's robin hood probe collision-free, so it never walks and never mispredicts; with scrambled integer keys 4.5.0 mispredicts 0.6-1.1 branches per lookup and main is 2.2x faster at the same sizes. See [Hits in cache-resident tables](#hits-in-cache-resident-tables-1000-to-16000-entries-450-against-main-346-main-takes-041-061-of-450s-time-on-scrambled-integer-keys-and-071-079-on-strings-from-a-quiet-loop-and-a-busy-one-and-102-143x-of-it-on-dense-ids-0n-1-which-a-multiplicative-hash-places-without-collisions----450s-best-case-and-the-keys-redpandas-and-osrms-callers-have).]

### `prefetch_index`'s two lines re-measured one header per binary: dropping either is within 3% on both compilers from 50000 to 16M entries, so both stay, but not for the reason the audit gave

*2026-10-02 · no issue · kept (both lines) · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/prefetch_index.{cpp,sh}`, one header per binary, 5 rounds rotated, medians, `AB_CORE=2`*

The cross-read of 2026-10-02 found that [The SSE probe audited, and its one real redundancy is compiler-dependent](#the-sse-probe-audited-and-its-one-real-redundancy-is-compiler-dependent) kept both prefetches "because the gcc gain is larger than the clang cost", while its own table said the opposite for dropping *one* line (clang 6-11% faster, gcc 1.3-1.9% slower); the 12% gcc figure it cited is for dropping *both*. Re-taken with the scored benchmark's own `find_all` over a `lookup_table`, a million lookups per call, twenty calls per run. ns per lookup, both / only `p + 87` / only `p + 64`, and both over each single-line variant (above 1.00 means the single line is faster):

| compiler | workload | 50000 | 200000 | 1M | 4M | 16M |
|---|---|---|---|---|---|---|
| clang | hit64, both/last | 1.008 | 1.013 | 0.988 | 0.996 | 0.992 |
| clang | hit64, both/first | 0.998 | 1.013 | 0.983 | 1.004 | 1.007 |
| clang | hitstr, both/last | 1.003 | 1.011 | 0.983 | 1.000 | 1.000 |
| clang | hitstr, both/first | 1.002 | 1.022 | 0.982 | 1.004 | 1.008 |
| clang | miss64, both/last | 1.010 | 1.004 | 1.001 | 0.981 | 0.982 |
| clang | miss64, both/first | 1.010 | 1.003 | 1.034 | 0.984 | 1.011 |
| gcc | hit64, both/last | 0.990 | 0.996 | 0.976 | 0.988 | 0.993 |
| gcc | hit64, both/first | 0.986 | 0.990 | 0.971 | 0.996 | 1.005 |
| gcc | hitstr, both/last | 0.993 | 0.992 | 1.002 | 1.004 | 0.999 |
| gcc | hitstr, both/first | 0.992 | 0.996 | 0.994 | 1.003 | 1.004 |
| gcc | miss64, both/last | 1.008 | 0.998 | 1.002 | 0.993 | 0.995 |
| gcc | miss64, both/first | 1.008 | 0.999 | 1.014 | 0.991 | 1.009 |

Absolute, clang hit64: 5.86 / 7.37 / 21.78 / 45.06 / 49.98 ns for both lines; gcc 5.76 / 7.25 / 20.63 / 44.03 / 49.13.

**Every cell is within 0.97-1.03; none clears the ±3% code-layout band.** The audit's clang win does not reproduce (its table was one run per cell, on a different loop). Under gcc, keeping both is slightly better at 1M (0.971-0.976). So the decision stands, both lines stay, and the reason is that no single-line variant is measurably better, not that a gcc gain outweighs a clang cost.

What this does not say: the lookups draw their key from an array (`find_all` reads `keys[]`), which adds the same misses to every variant and dilutes a ratio past the cache; it is the scored loop, so that is the loop the decision is for. ARM is still unmeasured.

## Platforms: ARM, Windows and the CI matrix

This section covers the map on machines other than the x86 desktop: the ARM runner before and after the NEON match, and the nine-job CI benchmark matrix including Windows. The NEON match closed the ARM lookup gap. The map is ahead of main on every CI machine and ahead of boost on every machine that has boost; `it64` is the one column that does not transfer across machines.

**Where it stands** (as of 2026-09-06)

- NEON (`vceqq_u8` plus a narrowing shift) is shipped for little endian AArch64; on Neoverse N2 the score against main went 1.112 to 1.244 ([NEON closed the ARM lookup gap, and it was the whole gap](#neon-closed-the-arm-lookup-gap-and-it-was-the-whole-gap)).
- The CI matrix (`bench.yml`, nine jobs) reads 1.04 to 1.24 against main and 1.18 to 1.49 against boost ([The whole matrix, run on CI rather than on the desktop](#the-whole-matrix-run-on-ci-rather-than-on-the-desktop)).
- Only the ARM pair measures what the vector compare is worth: on x86, `ANKERL_UNORDERED_DENSE_HAS_SSE2=0` handicaps main too.
- `it64` is the column to distrust across machines: 0.77 on an EPYC 7763 runner, 1.35 for the same binary on the 7950X.
- Push any branch to `bench` to re-measure every machine in CI.
- Prefetch tuning on ARM is still unasked (see [The SSE probe audited](#the-sse-probe-audited-and-its-one-real-redundancy-is-compiler-dependent)).

### Before NEON: on ARM the branch was 1.11x main, the same overall as on x86, split the opposite way

*2026-09-05 · no issue · superseded · `.github/workflows/bench.yml`: the paired harness on a GitHub `ubuntu-24.04-arm` runner, Neoverse N2, 4 cores, clang 18, 12 interleaved epochs, `origin/main` against the branch*

Main's scalar robin hood probe against the branch's SWAR fingerprint compare; neither has a vector path there. Geomean of the scored fifteen **1.112**, without iteration 1.14. Builds are far ahead (`build64` 1.67, `buildbig` 1.54, `buildstr` 1.25) and churn is ahead (`churn64` 1.20, `churnstr` 1.09), but **lookups are behind main**: `rhit64` 0.914, `findbig` 0.912, `rmissstr` 0.939, `findstr` 0.967; `find64` 1.03 is level (corrected 2026-10-02: listed `find64` 1.03 among the lookups behind main).

That is the SWAR match: about 36 instructions for two words, where SSE2 does sixteen bytes in three, paid on every probe. The robin hood probe it competes with was never vectorised on ARM either, so it lost nothing. This is the number a NEON match is for: `vceqq_u8` plus a narrowing shift is a handful of instructions, and indivi has the port.

Push any branch to `bench` to re-measure this and every other machine in CI. A `workflow_dispatch` would need the file on main first.

**Later (2026-09-05):** the NEON match took the score to 1.244 and put every lookup but `findstr` ahead of main, see [NEON closed the ARM lookup gap, and it was the whole gap](#neon-closed-the-arm-lookup-gap-and-it-was-the-whole-gap).

### NEON closed the ARM lookup gap, and it was the whole gap

*2026-09-05 · no issue · kept · same runner as "Before NEON: on ARM the branch was 1.11x main, the same overall as on x86, split the opposite way"*

With `vceqq_u8` and a narrowing shift, nothing is behind main any more except `findstr` at 0.97, where the hash, not the probe, is the cost. The score goes 1.112 to **1.244**, and without iteration 1.140 to **1.312**. That puts ARM ahead of where x86 sat before the rehash fix and level with it now.

The SWAR measurement prompted it (see [Before NEON: on ARM the branch was 1.11x main](#before-neon-on-arm-the-branch-was-111x-main-the-same-overall-as-on-x86-split-the-opposite-way)). On a Neoverse N2 the branch was 1.11x main overall but *behind* on four lookups (0.91-0.97) and only level on the rest (corrected 2026-10-02: said "*behind* on every lookup"), because the word-at-a-time compare replaces a robin hood probe that was never vectorised on ARM and so lost nothing to begin with.

Same runner, main's time over the branch's, SWAR first and NEON second:

| workload | SWAR | NEON |
|---|---|---|
| `rhit64` | 0.91 | **1.48** |
| `rmiss64` | 1.01 | **1.62** |
| `find64` | 1.03 | 1.26 |
| `findbig` | 0.91 | 1.18 |
| `rhitstr` | 1.02 | 1.10 |
| `build64` | 1.67 | 2.08 |
| `churn64` | 1.20 | 1.43 |

Builds and churn gained too, since both place through the same compare.

NEON has no movemask, and the cheap stand-in does not give the same mask shape: the sixteen answers land one nibble apart in a 64 bit word. So the mask type and a lane stride are named once, and `first_lane()` divides by the stride. Testing for a match, taking the lowest and clearing it with `m & (m - 1)` are then written once for all three backends. Guarded to little endian AArch64 (the mask reads the comparison as one word, and 32 bit ARM has no horizontal ops), with SWAR, which is correct everywhere, behind both.

### The whole matrix, run on CI rather than on the desktop

*2026-09-06 · no issue · info · `bench.yml`, nine jobs, 12 paired epochs, against `origin/main` and against boost where boost is installable*

Ahead of main on every machine, and ahead of boost on every machine that has boost. That is the first time either has been said about anything but one desktop. Geomean of the fifteen scored workloads:

| machine | vs main | without iteration | vs boost |
|---|---|---|---|
| linux clang SSE2 | 1.22 | 1.29 | 1.48 |
| linux clang no SSE2 | 1.14 | 1.17 | 1.29 |
| linux gcc SSE2 | 1.14 | 1.20 | 1.33 |
| linux gcc no SSE2 | 1.19 | 1.24 | 1.18 |
| arm clang NEON | 1.24 | 1.31 | 1.49 |
| arm clang no NEON | 1.14 | 1.18 | 1.33 |
| arm gcc NEON | 1.22 | 1.28 | 1.48 |
| arm gcc no NEON | 1.04 | 1.05 | 1.26 |
| windows clang SSE2 | 1.12 | 1.15 | not installed |

The desktop's own numbers (1.21 clang, 1.25 gcc) sit inside this spread, so the desktop was not flattering itself. But the spread is wider than the run-to-run noise on a quiet machine, and these runners are neither quiet nor identical. Read a column as "which side wins and roughly by how much", not to three digits.

**Only the ARM pair measures what the vector compare is worth**, and reading the x86 pair that way is a mistake, because main is in the comparison too. On ARM main has no vector path at all, so it is the same scalar code in both rows and the difference is entirely this map's: arm gcc falls from 1.22 to 1.04 without NEON, and its lookups go with it (`rhit64` 1.53 to 1.02, `rmiss64` 1.68 to 1.06). On x86, `ANKERL_UNORDERED_DENSE_HAS_SSE2=0` also takes away main's four-bucket SSE2 probe and its vector shifts. Both sides are handicapped and the ratio can move either way, which is why linux gcc reads *higher* without SSE2 (1.19) than with it (1.14). That number says main lost more than this map did, not that turning SSE2 off is good.

**Windows had never been measured at all**, and it is the mildest machine of the nine. It is ahead of main everywhere except `find64` at 0.98, but builds read 1.25 where linux clang reads 2.13. That is what a different allocator does to a workload that grows: `tame_allocator()` is a glibc trick and a no-op there.

**The one outlier chased down, and it is the CPU rather than the code**: linux gcc SSE2 iterates at **0.77** of main, the only figure anywhere below 0.9.

- Not noise: err% 0.0, a 76.9-78.6 interval, and 0.774 then 0.773 on two independent runs with fresh runners.
- Not the compiler: built with the runner's exact package (`Ubuntu 13.3.0-6ubuntu2~24.04.1`) in a container on this desktop, the same binary reads **1.35**, and upstream gcc 13.4 reads 1.01.

Same compiler, same flags, same source, so the same assembly. The machines are an EPYC 7763 on the runner against a Ryzen 7950X here, and the answer swings 1.75x between them.

`it64` can do that because it is the workload least about the index: 5000 inserts and 5000 erases against 25 million element visits. It is almost entirely a vectorisable sum over the value vector, which is the same `std::vector<std::pair<K, V>>` in both maps. Whichever way the two loops happen to land, one of them wins by a lot on a given microarchitecture, and nothing about the group index is being measured. The lesson for reading the matrix is narrow: `it64` is the column to distrust across machines, and a difference there is not evidence about the index. Every other workload agrees between the desktop and both runner architectures.

## Insert: what it inlines into its caller, and the call boundary

These entries measure how much of an insert the compiler places in the caller's loop and how much sits behind a call. Clang's extra instructions per insert, against gcc and against boost, are the call boundary (prologue, epilogue, argument setup), not register allocation; PGO removes them, and no header attribute moves the boundary past the outermost un-annotated function. The current shape came from a fixed-rule search over sixteen combinations: the home-group lookup is inlined, the miss path and the walk past home are called, on every compiler. Any change to what an insert inlines is judged by the caller corpus's worst ratio, not by the score.

**Where it stands** (as of 2026-09-27)

- One shape for every compiler: `do_find_or_place` (the home-group lookup) inlined, `find_or_place_miss` and `find_or_place_far` called, `flatten` on `append_value`. Worst ratio 1.11 under clang and 1.58 under gcc, against 3.32 and 3.54 for 5.2.0's everything-inlined shape. A rule weighting real programs would pick 5.2.0's shape for gcc again ([The shape search](#the-shape-search-sixteen-combinations-of-what-the-insert-inlines-each-judged-by-its-worst-ratio-to-the-best-combination-anywhere-measured-with-the-rule-fixed-before-the-results-it-picks-one-shape-for-every-compiler----the-home-group-lookup-inlined-the-miss-path-and-the-walk-past-home-called----worst-111-under-clang-and-158-under-gcc-where-520s-everything-inlined-reads-332-and-354)).
- A caller loop that stores and reloads a variable across a store with a cache-missing address, with a division before the next key, stops overlapping its misses on Zen 4: 380.7 against 48.1 cycles with no map at all. An inlined insert can push a caller into this trap ([udb3's `++h[key]` ran 70% slower under clang on 5.2.0 than on 5.1.0](#udb3s-hkey-ran-70-slower-under-clang-on-520-than-on-510-and-it-was-a-loop-variable-stored-and-reloaded-across-a-store-with-a-late-address-on-zen-4-that-with-a-division-on-the-way-to-the-next-key-stops-the-loop-overlapping-its-misses-5x-in-a-loop-with-no-map-at-all-the-part-of-the-insert-after-a-miss-is-called-again-under-clang)).
- Judge an insert-shape change by the caller corpus's worst-ratio line per compiler, not by a geomean ([The caller corpus](#the-caller-corpus-nine-loops-the-way-programs-write-them-one-binary-each-both-compilers-in-cache-and-past-it-judged-by-each-headers-worst-loop-and-not-by-a-geomean-520s-worst-was-33x-under-clang-and-36x-under-gcc-the-clang-miss-call-of-310-brings-clangs-to-110-and-under-gcc-nothing-that-removes-the-cliff-is-worth-its-price)).
- The clang hit was fixed by moving the hit out of the out-of-line function, not by reshaping that function: clang hit u64 72.0 -> 49.5 instructions (51.3 after V18a) ([`try_emplace` split at the home group: ](#try_emplace-split-at-the-home-group-the-hit-and-the-common-placement-inlined-into-the-caller-the-walk-past-home-behind-a-call-for-clang-only-and-every-scored-workload-at-or-under-mains-instruction-count-on-both-compilers)). Three source shapes and `flatten` on the caller's loop did not bring it down (`flatten` made it 77.0) ([`try_emplace` on a present key, attacked from the hit side](#try_emplace-on-a-present-key-attacked-from-the-hit-side-three-source-shapes-and-flatten-on-the-caller-and-clangs-count-does-not-come-down)).
- `insert()` and `emplace()` look the key up first when their arguments already are the value: set string hit 270.0 -> 104.6 instructions under clang. Arguments that need a conversion still build the value, because `unique_ptr_fill` leaked otherwise ([`insert()` and `emplace()` look the key up first ](#insert-and-emplace-look-the-key-up-first-when-the-arguments-already-are-the-value-a-sets-string-hit-22x-a-maps-insertk-v-hit-45x-under-clang-and-emplacek-new-int-must-still-build-its-value)).
- On 2026-09-10, when gcc inlined the whole insert, the clang/gcc gap was the call boundary: 15.00 prologue/epilogue instructions per insert under clang (also on boost) against gcc's 1.00. PGO removed it (96.4 -> 69.3). Since the shape search both compilers call the miss path after a home-group miss; the gap has not been re-counted. The real gap to boost is about eleven instructions, the second walk in `place_group` ([The insert path's instructions, counted one by one: ](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it)).
- `always_inline` on the placement stays. It was measured on `do_place_element` (14 to 20% of a one-map build) and re-measured on today's `place_element_at`: clang's integer build 1.19-1.22x faster with it, strings 1.02-1.04, gcc unchanged for integers ([re-measured 2026-10-02](#always_inline-on-place_element_at-re-measured-on-todays-insert-shape-clangs-integer-build-119-122x-faster-with-it-gccs-unchanged-so-it-stays)). The translation unit's *size* decides what the attribute is worth: the ~90-TU scored suite reads the opposite sign.
- An insert derives the fingerprint word, counter and group from its hash once: 97.0 -> 93.0 clang instructions. A shared pipelining helper for rehash and range insert costs 1.9 instructions per element and was not adopted ([Taking the hash apart once per insert instead of twice, ](#taking-the-hash-apart-once-per-insert-instead-of-twice-and-the-shared-pipeline-that-was-not-worth-it)).

### The insert path is split in two by clang, and that is most of the build gap to boost.

*2026-09-05 · no issue · kept · reserved table, instructions per insert net of the benchmark loop; `scripts/ab/run.sh -c g++`*

Under clang, `do_try_emplace` is its own function with a six register prologue. It calls `do_place_element` out of line, because clang refuses to inline it at cost 480 against a threshold of 250. `vector::emplace_back` with `piecewise_construct` is 225 of that cost. gcc inlines the whole insert into the caller on its own.

| compiler | this map, insert (miss) | boost | this map, `operator[]` on a present key |
|---|---|---|---|
| clang 22 | 128 instructions, 39 cycles | 64, 26.5 | 74 instructions |
| gcc 16 | 82 instructions, 26 cycles | 55, 23 | 68 instructions |

So the gap to boost on the insert path is 32% on cycles under clang and 11% on cycles under gcc (corrected 2026-10-02: said "39% under clang and 11% under gcc"; on instructions the table reads 2.0x and 1.49x). The score moves with it. **Under gcc this map is 1.25x ahead of boost on `build64`**, where under clang it is 0.70x behind. On the geomean without iteration gcc has it 1.067 ahead of boost and clang 0.96. The full gcc score against main is 1.165, against 1.109 for clang, with `build64` 1.56 and `buildbig` 1.84. `scripts/ab/run.sh -c g++` reproduces it.

**Later (2026-09-10):** boost's 64 instructions under clang did not reproduce (82.9), so the gap under clang is 16% on instructions, not the 2.0x this table reads, see [The insert path's instructions, counted one by one](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it).

What was tried for clang, all measured paired on the score:

**Forcing `do_place_element` and `place_group` inline** (`always_inline`). It was applied in 2026-09 on a paired geomean of **1.012**, removed on 2026-09-08, and **put back the same day**. (**Later (2026-10-02):** re-measured on `place_element_at`, which replaced `do_place_element`: clang integer builds 1.19-1.22x faster with the attribute, gcc unchanged, see [the re-measurement](#always_inline-on-place_element_at-re-measured-on-todays-insert-shape-clangs-integer-build-119-122x-faster-with-it-gccs-unchanged-so-it-stays).)

- The removal rested on the scored suite built one header per binary: 1.7% faster under clang, 3.9% under gcc. That binary is ~90 translation units of test suite. Its inlining budget is exhausted, so an `always_inline` there displaces something else.
- In a unit holding **one** map, which is what a caller compiles, building from empty is **14 to 20% slower without the attribute at every size**. The instruction counts settle it, since neither layout nor drift moves them: **17 to 20% more work retired without it**. `maps.cpp` (eighteen maps in one unit) agrees: build 17% slower at 32000.

| entries | ns with | ns without | instructions with | instructions without |
|---|---|---|---|---|
| 32000 | 251633 | 287833 | 5.08M | 5.98M |
| 200000 | 1749840 | 2087600 | 28.09M | 33.71M |
| a million | 13064800 | 15670600 | 162.3M | 190.4M |

**The rule is narrower than "one header per binary": the translation unit's *size* decides what an `always_inline` is worth, a benchmark binary is the largest unit anyone compiles this into, and an instruction count is the only number none of that moves.** The paired harness cannot see this class of change at all.

What the attribute costs is real and unchanged. `operator[]` on a present key pays the placement code's register pressure on a path that never places (clang 73.2 instructions against 48.4). That cost is smaller than 17% of a build. When first applied, the attribute did what the entry then said: miss path 128 to 100 instructions, `build64` 1.070, `ie64` 0.967 (the merged function pays the placement code's register pressure on the path that never places). Three things landed on `do_place_element` since: the merged block, `move_home` and the pipelined rehash. The inlined body is bigger, so that pressure is worse.

One map per binary at 50000 entries, forced against not forced:

| | forced: instructions, cycles | not forced |
|---|---|---|
| clang, reserved insert | 97.8, 31.3 | 106.8, **27.2** |
| gcc, reserved insert | 124.9, 38.0 | **68.9, 23.9** |
| clang, `try_emplace` on a present key | 73.2, 16.8 | **48.4, 11.5** |

**Later (2026-09-10):** these three rows did not reproduce with `maps_one.sh`, one map per binary: gcc insert 67.6 forced against 65.5 not, clang insert 96.4 against 124.4, and `++m[k]` under clang 84.7 against 83.7, see [The insert path's instructions, counted one by one](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it).

The attribute is no longer the no-op for gcc it was described as. The scored benchmark, built one header per binary and alternated, three rounds, 0.1% spread: **clang 0.017720 to 0.017416 and gcc 0.018664 to 0.017940**, 1.7% and 3.9% faster without it. A paired run read 0.979 and 0.995. Removing `probe`'s attribute as well is the worst of the three (gcc 0.019078, 2.2% *worse* than shipped). That control says this is about one function and not about `always_inline` in general.

**Handing the probe's fingerprint word and home group to an out-of-line `do_place_element`**, so the insert derives nothing twice: 141.7 to 143.7, i.e. nothing.

**Returning the value index in a register** instead of a `pair<iterator, bool>`: 141.7 to 141.7. Clang already returns that pair in registers. The 28 instructions are the call boundary itself (prologue, epilogue, argument setup), and only merging removes them.

**`increase_size` out of line**, so the hot function shrinks: no change. Clang's cost is in `emplace_back`, not in the growth path.

**The erase path** is already flat: `erase(key)` is 102 instructions under clang with or without forced inlining of `do_erase`, `erase_group_slot` and `finish_erase`.

The next thing to try, not yet done: get `do_place_element` under clang's threshold *honestly*. That means making the common-case append cheaper for clang's cost model than `emplace_back(piecewise_construct, forward_as_tuple(key), forward_as_tuple(args...))`. For a map of trivially constructible types that is a 16 byte store dressed as 225 units of inline cost. gcc already inlines the same code fully, so this is a clang-only 7% on builds and churn, waiting on codegen and not on design.

**Later (2026-09-27):** not pursued. #321 and the shape search put the call after a home-group miss on purpose, see [The shape search](#the-shape-search-sixteen-combinations-of-what-the-insert-inlines-each-judged-by-its-worst-ratio-to-the-best-combination-anywhere-measured-with-the-rule-fixed-before-the-results-it-picks-one-shape-for-every-compiler----the-home-group-lookup-inlined-the-miss-path-and-the-walk-past-home-called----worst-111-under-clang-and-158-under-gcc-where-520s-everything-inlined-reads-332-and-354).

### The insert path's instructions, counted one by one: the clang/gcc gap is the call boundary, and PGO removes all of it

*2026-09-10 · #234 (step 1) · info · `scripts/ab/maps_one.{cpp,sh}`, `valgrind --tool=callgrind`, n = 50000 and up to 4000000*

The clang/gcc instruction gap on a reserved insert is the function-call boundary, and PGO removes it. Since 2026-09-08 this file had said that "a reserved `uint64_t` insert is 97.8 instructions under clang and 65.9 under gcc, on identical source". It explained the difference as clang spilling the probe's loop state at function entry, where gcc sinks it into a branch a miss never takes: "a register allocator's choice and not something a source change has been found to steer" (see [The one fact left standing is the clang/gcc gap itself](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept) in [Seven ideas from reading folly F14 and from the F14Vector string gap](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept)). Nobody had listed the instructions. They are listed here, and that explanation was wrong.

**First, the tool.** The `/tmp/ins.cpp` that produced the old numbers was lost with `/tmp`, so the measurement could not be repeated. It is now `insert` and `bump` in `scripts/ab/maps_one.{cpp,sh}`:

- `insert`: a *reserved* build, so no growth and no rehash is in it.
- `bump`: `++m[k]` on a key already present.
- Both count `reps` in operations, so every column is per operation. Both run the round in a `noinline` function, so `objdump` has a symbol.

**Reproduction, n = 50000, per insert, three runs agreeing on instructions to 0.1%:**

| | this map | boost | notes' 2026-09-08 figure |
|---|---|---|---|
| clang 22.1.8 | **96.4** instr, 33.8 cyc, 6.4 ns | 82.9, 21.3, 4.1 | this map 97.8, reproduces |
| gcc 16.2.1 | **67.5**, 25.0, 4.7 | 57.0, 16.9, 3.2 | this map 65.9, reproduces |

Both of this map's figures reproduce. **Boost's does not.** This file recorded boost at 64 instructions under clang, and it is 82.9. The 64 comes from the 2026-09-05 table (see [The insert path is split in two by clang](#the-insert-path-is-split-in-two-by-clang-and-that-is-most-of-the-build-gap-to-boost)), taken with the lost tool and before the merged block. The 2026-09-08 entry restated this map's numbers but never boost's. So the headline "39% behind boost under clang on the insert path" compared two different measurements. It is **16%** on instructions.

**Second, exact instruction counts, by category.** Sampling cannot answer this: `perf record -e instructions` skids, and it attributed 8.4 prefetches per insert to a path that issues 2. `valgrind --tool=callgrind --dump-instr=yes` counts every instruction exactly. It agrees with `perf stat` to 0.5% once the once-per-process key generation is subtracted. Per insert, in each binary's own code:

| category | gcc, this map | clang, this map | gcc, boost | clang, boost |
|---|---|---|---|---|
| **prologue/epilogue** | **1.00** | **15.00** | **0.00** | **15.00** |
| call | 0.00 | 1.00 | 0.00 | 1.00 |
| move/load/store | 16.94 | 30.56 | 14.77 | 26.50 |
| arithmetic/address | 16.21 | 20.15 | 19.51 | 19.43 |
| compare/branch | 13.57 | 18.19 | 9.42 | 9.15 |
| fingerprint compare (SSE2) | 10.89 | 9.92 | 9.24 | 9.19 |
| spill + reload + frame | 11.53 | 4.20 | 8.69 | 7.29 |
| prefetch | 2.01 | 2.01 | 0.00 | 0.00 |
| **total** | **67.1** | **96.0** | **56.5** | **82.5** |

**Open (2026-10-02):** the rows sum to about 5 more than the total in every column (72.2, 101.0, 61.6, 87.6). The totals match `perf stat`, so some instructions are counted in two rows. Which ones is not known: the callgrind dumps were not kept.

Clang pays **fifteen instructions of prologue and epilogue per insert**: six pushes, six pops, a frame adjust on either side and a `ret`, plus the call. gcc pays one. **Clang pays the same fifteen on boost.** That settles it: this is not a register allocator mishandling this map's probe. It is *the function-call boundary*, paid because clang leaves the operation out of line and gcc inlines it into the caller. The spill and reload column, which the old diagnosis named, goes the other way: gcc spills more than clang, 11.53 against 4.20.

**Third, PGO, and it is decisive.** Hot/cold outlining and inlining are profile decisions, so the question was whether clang does it when told. Instrument, run, rebuild:

| | instructions | cycles |
|---|---|---|
| clang, this map, plain | 96.4 | 34.0 |
| clang, this map, **PGO** | **69.3** | **17.5** |
| clang, boost, plain | 82.9 | 23.7 |
| clang, boost, **PGO** | **57.8** | **14.2** |
| gcc, this map, plain | 67.6 | 31.2 |
| gcc, this map, PGO | 69.5 | 25.8 |

**PGO removes 27 instructions from clang and halves its cycles, on both maps.** After PGO, clang and gcc retire the same work on this map (69.3 against 67.6), and clang's code is much the faster of the two. gcc gains nothing on instructions because it had already inlined. So the clang/gcc instruction gap is a missing profile and nothing else. No source shuffle was going to close it, which is why the six that were tried did not. **Open (2026-10-02):** gcc plain reads 31.2 cycles here and 25.0 in the reproduction table above, same binary, and boost's 23.7 -> 14.2 is 0.60, not half. Not re-measured.

**What is left, once the boundary is out of the way, is about eleven instructions.** This map against boost, PGO to PGO under clang: 69.3 against 57.8. Under gcc: 67.6 against 57.0. By category under gcc:

| category | this map minus boost |
|---|---|
| compare/branch | +4.2 |
| frame accesses | +3.8 |
| move | +2.2 |
| prefetch (the probe's two `prefetch_index`, which boost does not issue on an insert) | +2.0 |
| vector compare | +1.7 |
| arithmetic | -3.3 |

That is the real design difference, and it is roughly the second walk. This map probes to find the key absent and then walks again in `place_group`. Boost's probe returns the position it will insert at. Eleven instructions, not thirty-four. **Open (2026-10-02):** no insert gap in this file reads 34, so which figure the thirty-four refers to is not known.

**And it is an in-cache statement only.** The same measurement at four million entries, ns per insert:

| n | clang, this map | clang, boost | gcc, this map | gcc, boost |
|---|---|---|---|---|
| 50000 | 6.4 ns | 4.1 | 4.7 | 3.2 |
| 200000 | 6.9 | 5.6 | 6.3 | 3.9 |
| **4000000** | **35.6** | **34.6** | **28.7** | **28.3** |

At four million the maps are level, within 3% on both compilers. This map takes **0.80 dTLB misses per insert against boost's 1.37**, and 1.07 branch misses against 1.26. Whatever the insert path costs stops mattering where the table stops fitting in cache.

**The other two workloads, for the record.** `++m[k]` on a key already present, n = 50000: this map 84.7 instructions under clang and 90.9 under gcc, boost 80.9 and 57.1. Here *both* compilers leave this map's `do_try_emplace` out of line (15.4 instructions of prologue in both), while gcc inlines boost's. That is 15 of the 34 instructions that separate them under gcc. This path is also where `always_inline` on `do_place_element` is paid without being used. The placement code makes `do_try_emplace` too big for the caller, so every `operator[]` buys a call boundary. That is the trade [The insert path is split in two by clang](#the-insert-path-is-split-in-two-by-clang-and-that-is-most-of-the-build-gap-to-boost) describes from the other side, now with a number on it.

A **string** insert reverses the whole picture: 491 instructions under clang against boost's 454, and **27.3 ns against 34.9**. That is 21% *faster* on 8% more instructions, on 7.2 L1 misses against 11.2 and 1.45 branch misses against 2.16.

**So the standing sentence in `CLAUDE.md`, "clang splits the insert path and gcc does not, which is most of the build difference between them; no source change has been found that steers it", is right about the mechanism and wrong about the remedy.** A profile steers it completely. Nothing here recommends shipping a PGO build; a header library cannot. It does say where the next attempt should not go: the probe's register allocation, which was never the problem.

**And the one candidate the list suggested is dead too, which is the useful half of it.** The callgrind table says `always_inline` on `do_place_element` makes `do_try_emplace` too big for its caller, so every writing lookup buys a call boundary. The obvious answer is to move the attribute. Five variants, one map per binary, instructions per operation (per element for `build`), both compilers:

| | clang insert | clang bump | clang build | gcc insert | gcc bump | gcc build |
|---|---|---|---|---|---|---|
| **A, shipped** (placement `always_inline`) | **96.4** | 84.7 | **140.4** | **67.6** | 90.9 | **123.3** |
| B, placement plain | 124.4 | 83.7 | 168.5 | 65.5 | 90.9 | 125.2 |
| C, placement `noinline` | 124.4 | 83.7 | 168.5 | 144.4 | **72.8** | 198.0 |
| D, placement **and** `do_try_emplace` `always_inline` | **96.4** | 84.7 | **140.4** | **67.6** | 90.9 | **123.3** |
| E, C plus `do_try_emplace` `always_inline` | 124.4 | 83.8 | 168.5 | 144.4 | **72.8** | 198.0 |

**D is identical to A in every column**, not close but identical, and the binaries differ. In A the out-of-line symbol is `do_try_emplace`. In D that symbol is gone and **`try_emplace` is out of line instead**. Forcing the inner function into its caller moves the boundary outward by one level and changes nothing. The outermost function without the attribute is the one that pays, and there is always one. A header cannot inline its way into the caller's loop. That is why PGO was the only thing that removed the boundary: it inlines the whole chain into the loop that calls it.

**C is the trade named out loud and it is a bad one**: 20% off a gcc `++m[k]` (90.9 to 72.8) for **61% onto a gcc build** (123.3 to 198.0) and 20% onto a clang one. B reproduces this file's own 17-20% build regression exactly, now as an instruction count instead of a time: clang build 140.4 to 168.5. So `always_inline` on `do_place_element` stays, for the third time. The call boundary on `operator[]` is its price, and nothing in the source can refuse it.

**Later (2026-09-26):** #321 forced every level of the chain inline down to the home-group lookup and put the call after a home-group miss, so a hit in its home group no longer pays a boundary, see [`try_emplace` split at the home group](#try_emplace-split-at-the-home-group-the-hit-and-the-common-placement-inlined-into-the-caller-the-walk-past-home-behind-a-call-for-clang-only-and-every-scored-workload-at-or-under-mains-instruction-count-on-both-compilers).

### Taking the hash apart once per insert instead of twice, and the shared pipeline that was not worth it

*2026-09-11 · #244 · kept · from four cleanup reviews of the range insert*

Three of the four review items were measured. Two are in, one is out, and one was not attempted.

**In: an insert derived the same three things from its hash twice.** `probe` computed the fingerprint word, the counter and the group, and `place_group` computed all three again. The two functions are separated by a fingerprint store and a possible reallocation. So for a key whose compare is a call, where `m_equal` is opaque, the compiler eliminates no common subexpression across them. (The pipelined insert's lookahead also derives a group, but for the element *sixteen ahead*, so it is not a duplicate and cannot be shared.) `probe`, `place_group` and `do_place_element` each gained a variant that takes the pieces, with the old signature as a thin wrapper. **That alone is 4% of a single clang insert, 97.0 instructions to 93.0, and 3% of a gcc `++m[k]`.** `place_group` had been re-deriving the fingerprint word across an inline boundary from a caller that had just computed it.

**In: the probe does not ask for a block the caller is already holding.** A pipelined insert formed the home group sixteen elements earlier to prefetch it. It now hands that group itself to `probe_at_home`, not a number to find it by. The prefetch is then skipped by construction, and only for the one group the caller knows about. Another 3%.

This started as a `template <bool Prefetch>` flag on the probe. The four review passes found the flag wrong twice; the measurement did not:

- it suppressed the prefetch for *every* group of the overflow walk, which nobody had asked for;
- it kept suppressing it for the sixteen elements after a growth, whose earlier prefetch went to the array the growth had just replaced.

A compile-time flag stood in for a run-time fact. Passing the group makes the fact true instead of asserted.

Together, ns per element rebuilding a map from a vector of pairs:

| | reserved | growing |
|---|---|---|
| 200000 | 5.42 to 5.08 | 8.41 to 8.04 |
| two million | 7.50 to 7.18 | 26.48 to 26.08 |

Lookups are byte identical on both compilers, which is the bar for touching the probe.

**Out: one shared pipelining helper for the rehash and the range insert.** All four reviews raised the duplicated ring, and the concern is right: the read-before-refill ordering is written twice, and a drifted copy returns a wrong answer instead of crashing. The helper was built anyway, taking the fetch and the per-element work as callables so each caller keeps its own hoisting. **It costs the rehash 1.9 instructions per element**, 45.7 to 47.6, and 1.3% of its time on **six of six** interleaved pairs. Strings were neutral. Passing the element index through the callable, on the theory that a captured counter was the cost, recovered none of it. The price is the indirection.

The rehash needs `groups`, `mask` and `shifts` in locals, and that is load-bearing. A fingerprint store is a `std::uint8_t` store that may alias the container's own data pointer, so reading them through `this` costs a reload on the address chain of every element. The range insert *cannot* hold them in locals, because growth moves the array underneath it. So the two loops differ in exactly the part that matters. What is left to share is the ordering, which is now stated once, in a comment each site points at, at no cost. **The duplication is measured to be cheaper than the fix.** It is recorded so it is not re-proposed.

**Not attempted: pipelining `replace()`.** It is the last bulk build path without a lookahead, and it is harder: the loop swap-erases duplicates from the back, so `m_values.back()` moves under a lookahead, and the index does not advance on a duplicate. The argument for doing it was that a shared helper would make it tractable. With that helper rejected on measurement, it would be a third hand-written ring for a path most callers never take. Left open.

**Later (2026-09-11):** `replace()` got its lookahead after all, because a duplicate's pull disturbs only the last position, see [`replace()` hashes ahead too](#replace-hashes-ahead-too-once-the-reason-it-could-not-was-looked-at-properly).

### `try_emplace` on a present key, attacked from the hit side: three source shapes and `flatten` on the caller, and clang's count does not come down

*2026-09-22 · #305 · rejected · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/try_emplace_hit.{cpp,sh}`*

No source shape and no `flatten` brought clang's 72.04 instructions per hit down, so nothing was kept and the header did not change.

#305 was filed on the 73.2 against 48.4 instructions quoted in "The insert path is split in two by clang, and that is most of the build gap to boost." and in `do_place_element`'s comment, as an untried lever. It was not untried. "And the one candidate the list suggested is dead too" (in "The insert path's instructions, counted one by one: the clang/gcc gap is the call boundary, and PGO removes all of it") had already moved the `always_inline` outward (variant D, identical to shipped) and put the placement out of line (variant C, 61% onto a gcc build). "The one fact left standing" (in "Seven ideas from reading folly F14 and from the F14Vector string gap, all measured on 2026-09-07, none kept") names the register allocator. The issue should have been checked against those two first. New here: a harness that counts in-process around the loop only, the shrink-wrapping question that neither entry asked, and `flatten`.

The harness: one map and one mode per binary, `map<uint64_t, uint64_t>` and `map<std::string, uint64_t>` (8-40 bytes) at 50000 entries. `hit` = `try_emplace` on a present key in random order. `miss` = `try_emplace` of fresh keys into tables reserved for them. 4M operations, instructions and cycles from `perf_event_open` around the loop, user space only. Main against itself reads identical instruction counts in every cell, so any difference is the header. Instructions per operation:

| | clang hit u64 | clang hit str | clang miss u64 | clang miss str | gcc hit u64 | gcc hit str | gcc miss u64 | gcc miss str |
|---|---|---|---|---|---|---|---|---|
| main (`ad06cce`) | **72.04** | 147.11 | **95.97** | **312.89** | **45.12** | 109.10 | 77.90 | 291.67 |
| 1: placement out of line, given only the hash | 74.04 | **140.14** | 108.03 | 334.91 | 50.15 | 113.16 | 119.91 | 327.32 |
| 2: `always_inline` on `do_try_emplace` | 72.04 | 147.11 | 95.97 | 312.89 | 47.15 | **109.07** | **76.87** | **288.67** |
| 3: only the hash held across the probe | 72.04 | 144.11 | 95.97 | 319.38 | 45.12 | 109.10 | 77.90 | 291.67 |
| main, caller's loop `flatten` | 77.04 | 152.11 | | | 55.10 | 111.08 | | |

**Why the prologue cannot be shrink-wrapped, from the disassembly of clang's out-of-line `do_try_emplace`.** A hit pays six pushes, a stack adjustment and a spill on entry and the same on return: 16 of its 72 instructions. The obvious fix is clang's shrink-wrapping, which would put those saves on the placement path only. That needs the hit path to touch no callee-saved register. It touches six. The hit path uses fifteen general purpose registers: all nine that x86-64 lets a function use without saving, and six more. They hold the key, the hash, the home group, the group, delta, the mask, the group and value base pointers, the block, the lanes, the lane, the counter, `this` and the argument pointer.

- Candidate 1 takes the placement's state out of the hit path entirely, and the prologue stays, because the probe alone needs more than nine.
- Candidate 3 hands the placement the hash instead of its three pieces, and clang's integer counts do not move by one instruction: clang had already folded the two shapes into one.
- Getting the probe under nine means reloading the mask, the bases or `this` on every step of every lookup, including `find`, which shares the loop.

**`flatten` says the prologue is not even the cost.** With the whole chain inlined into the caller's loop (gcc's shape, and the one PGO produced in [The insert path's instructions, counted one by one: ](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it)), clang reads 77.04: five *more* than with the boundary. The pressure the out-of-line function pays as a prologue, the inlined code pays inside the loop. gcc inlines `try_emplace` into the loop on its own and reads 45.12 on identical source. Forcing it with `flatten` makes gcc worse too (55.10), because `flatten` also inlines the vector's growth path. So of the 27 instructions between the compilers on a hit, the call boundary is at most the 16 of the prologue. The rest is how each compiler allocates the same inlined body.

Candidate 3's string cells are the only non-null difference, and they cancel: a string hit 3.0 instructions fewer (2.0%), a string insert 6.5 more (2.1%), gcc unmoved.

What this says and does not say: one machine, one value type, 50000 entries, all in cache. The counts are the decisive number; the cycles (one run, not tabled) moved with them. It does not cover `operator[]` separately; it is the same `do_try_emplace`. It says nothing about MSVC. The standing sentence in `CLAUDE.md` is unchanged: nothing in the source has been found to steer clang's allocation here, and now four more shapes have been tried.

**Later (2026-09-26):** #321 took the hit out of the out-of-line function instead of reshaping it: `do_try_emplace` forced inline with the home-group hit, clang hit u64 72.0 -> 49.5 instructions, see [`try_emplace` split at the home group](#try_emplace-split-at-the-home-group-the-hit-and-the-common-placement-inlined-into-the-caller-the-walk-past-home-behind-a-call-for-clang-only-and-every-scored-workload-at-or-under-mains-instruction-count-on-both-compilers).

### `try_emplace` split at the home group: the hit and the common placement inlined into the caller, the walk past home behind a call for clang only, and every scored workload at or under main's instruction count on both compilers

*2026-09-26 · #321 · kept · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/try_emplace_hit.{cpp,sh}`, `scripts/ab/solo.sh`, `scripts/ab/perwl.sh`*

Shipped as V16 plus V18a. One map, clang hit u64 72.0 -> 49.5 instructions (51.3 after V18a), clang miss 96.0 -> 84.2 (81.2), gcc miss 77.9 -> 74.0 (68.96). Score with `solo.sh`: clang 1.0219, gcc 1.0767.

#321 was filed on two things: 5.1.0 being slower than 4.4.0 in MySQL's `EXCEPT`, and the 72 instructions a clang hit takes against gcc's 45. "`try_emplace` on a present key, attacked from the hit side: ..." had put the 72 down to the register allocator. It was right that no source shape moves the out-of-line function. It did not try making the hit stop being in that function.

`do_try_emplace` is now forced inline. It does three things before anything else is called:

1. the home group's fingerprint match (a hit returns there);
2. `allocate_buckets_if_none()`;
3. a look at the home group's overflow counter for the key's class. When the counter is zero, the key is nowhere else, and it is placed in home.

Only a key whose class has overflowed home reaches `try_emplace_far`. That walks with `probe_after_home` and `move_home`s a hit, as before.

Three supporting changes, each found by a variant that lost without it:

- `append_value()`, the value container's `emplace_back` under `always_inline` + `flatten`. With the insert inlined into a large caller, gcc runs out of inlining budget at `emplace_back` and calls it. A map held as a local of the caller (scalar-replaced into nine fields) is then stored to the stack and reloaded around every call. That was a 45% slower u64 build in the score binary (V10).
- `increase_size()` and `fill_buckets_from_values()` `noinline`. Both run once per doubling, and inlined into every insert site they cost gcc's loop. Taking each back out of V16 on its own (V17a, V17b), gcc's u64 build goes from 0.992 of main's instructions to 1.025 and 1.050. Without the first, the big-value `random insert erase` goes 0.827 -> 0.859. Clang's counts do not move.
- The walk past home, `try_emplace_far`: `noinline` under clang, `always_inline` under gcc (`ANKERL_UNORDERED_DENSE_TRY_EMPLACE_FAR`). Out of line, gcc takes 11% more cycles on every insert of a fresh key, not only the ones that walk (V11 against V12). Inlined, clang retires 8% more instructions building a `map<uint64_t, big_value>` (V15 against V14).

**One map per binary, instructions per operation** (cycles in parentheses, one run each; V1-V13 in one pass, V14-V16 in a second, which instruction counts do not care about). Hit = `try_emplace` on a present key, miss = fresh keys into a reserved table, 50000 entries.

| | clang hit u64 | clang hit str | clang miss u64 | clang miss str | gcc hit u64 | gcc hit str | gcc miss u64 | gcc miss str |
|---|---|---|---|---|---|---|---|---|
| main | 72.0 (31.9) | 147.1 (107.2) | 96.0 (38.8) | 312.9 (130.2) | 45.1 (24.0) | 109.1 (90.7) | 77.9 (30.5) | 291.7 (121.2) |
| V1: home hit inline, rest `noinline` | 44.0 (23.3) | 125.3 (89.3) | 108.0 (43.1) | 323.9 (132.9) | 42.4 (22.2) | 106.2 (83.2) | 115.8 (42.7) | 332.7 (136.9) |
| V2: rest left to the compiler | 44.0 (22.6) | 125.3 (89.2) | 108.0 (42.5) | 323.9 (132.7) | 42.5 (23.7) | 108.3 (87.8) | 85.0 (33.4) | 302.2 (119.0) |
| V3: V2 + hash pieces passed on | 46.1 (23.0) | 126.4 (91.9) | 106.0 (41.6) | 324.9 (134.8) | 42.3 (24.1) | 107.2 (87.3) | 80.0 (33.5) | 304.7 (120.4) |
| V4: `try_emplace`/`operator[]` `always_inline`, rest `noinline` | 45.1 (22.9) | 104.6 (86.9) | 106.0 (41.2) | 324.8 (135.8) | 43.4 (22.9) | 112.2 (86.3) | 113.9 (42.6) | 332.3 (136.3) |
| V5: V4 + `increase_size` `noinline` | 45.1 (23.0) | 104.6 (86.6) | 106.0 (42.4) | 324.8 (134.8) | 43.4 (23.1) | 112.2 (86.4) | 104.9 (39.4) | 331.1 (134.6) |
| V7: V5, rest `always_inline` under gcc | 45.1 (23.1) | 104.6 (87.0) | 106.0 (41.6) | 324.8 (134.9) | 44.4 (23.3) | 110.2 (85.5) | 76.0 (29.9) | 295.3 (113.6) |
| V8: V7 without `increase_size` `noinline` | 45.1 (22.8) | 104.6 (87.4) | 106.0 (41.6) | 324.8 (133.5) | 42.3 (24.0) | 109.2 (88.4) | 78.0 (31.9) | 300.8 (116.8) |
| V9: V7 + `fill_buckets_from_values` `noinline` | 45.1 (23.3) | 104.6 (86.7) | 106.0 (40.9) | 324.8 (133.0) | 44.4 (23.6) | 110.2 (84.6) | 76.0 (30.4) | 295.3 (119.8) |
| V10: V9, counter-zero placement inline, walk `noinline` | 45.8 (23.2) | 105.2 (86.6) | 101.1 (38.0) | 319.9 (133.4) | 45.7 (24.2) | 116.4 (87.1) | 79.2 (34.6) | 295.9 (121.4) |
| V11: V10 + `append_value` `flatten` | 45.8 (23.8) | 105.2 (86.5) | 101.1 (39.7) | 319.9 (134.4) | 46.6 (23.8) | 118.3 (87.2) | 82.0 (33.2) | 296.0 (120.2) |
| V12: V11, walk `always_inline` under gcc | 45.8 (23.5) | 105.2 (86.9) | 101.1 (39.2) | 319.9 (134.0) | 44.4 (24.2) | 111.2 (85.6) | 74.0 (29.8) | 293.8 (113.5) |
| V13: V12, `flatten` only when size != capacity | 45.8 (23.4) | 105.2 (86.8) | 117.1 (40.2) | 323.4 (133.4) | 45.4 (24.0) | 112.2 (84.3) | 81.0 (35.4) | 295.7 (122.3) |
| V14: V12, placement inline under clang too | 49.5 (24.8) | 111.7 (88.6) | 85.2 (36.9) | 303.7 (134.2) | = V12 | | | |
| V15: V14, walk inline under clang too | 50.8 (25.4) | 109.9 (86.6) | 82.1 (35.0) | 301.6 (124.6) | = V12 | | | |
| **V16 = V14 folded, shipped** | **49.5 (24.9)** | **111.7 (87.1)** | **84.2 (35.1)** | **303.7 (124.4)** | **44.4 (23.7)** | **111.2 (85.7)** | **74.0 (29.7)** | **293.8 (113.9)** |

V1-V9 fix the clang hit and pay for it in gcc's miss (V1 and V4: +40% cycles) or in clang's miss (+10-13% instructions). A call to the placement costs both compilers more than the prologue it saves. Every variant up to V12 left clang's miss above main.

V13 tried to stop `flatten` pulling the vector's growth path into every insert site by testing `size() != capacity()` first. The extra compare costs gcc 19% cycles on the u64 miss, and the code was no smaller, so it was dropped.

V14 is the first variant under main in six of the eight cells on instructions, because the placement no longer sits behind a call either. The other two are gcc's string hit, +1.9% on instructions and -5.5% on cycles, and gcc's string miss, +0.7% and -6.0% (corrected 2026-10-02: said "seven of the eight cells" and named only the string hit).

V18a, from `/simplify`: the hit branch already relies on a table with values having buckets, so the miss's `allocate_buckets_if_none()` moves into the `else`, and a miss tests emptiness once. Kept. One map:

| | V16 | V18a |
|---|---|---|
| gcc miss u64 / str | 73.96 / 293.8 | 68.96 / 287.3 |
| clang miss u64 / str | 84.2 / 303.7 | 81.2 / 294.0 |
| clang hit u64 | 49.5 | 51.3 |

Against V16 the score is inside the band on time and 0.984-1.003 on instructions per workload (builds -1.2 to -1.6% on both compilers). V18b, the same review's other find, was dropped. It had clang's out-of-line walk call `probe_from` directly instead of `probe_after_home`, which re-tests the counter its caller just tested and, for a string key, makes a second call. It saved at most one instruction a hit and would copy the walk's termination test under a compiler `#if`.

**The score, one header per binary** (`solo.sh`, five alternated rounds, then `perwl.sh`). V12 differs from V11 only under gcc (the walk `always_inline` under gcc), so clang's V12 is V11 and has no number of its own; likewise V16 = V14 under clang, and V14 and V15 = V12 under gcc.

| | clang | gcc |
|---|---|---|
| V7 | 0.9948 | 0.9841 |
| V9 | | 0.9898 |
| V10 | | 1.0159 (u64 build +45% time) |
| V11 | 1.0023 | 1.0693 |
| V12 | = V11 | 1.0820 |
| V14 | 1.0219 | = V12 |
| V15 | 1.0185 | = V12 |
| V16 | = V14 | 1.0767 (per-workload counts identical to V12) |
| V17a: V16, `increase_size` inlinable | not run (one-map counts identical to V16) | 1.0726 |
| V17b: V16, `fill_buckets_from_values` inlinable | not run (one-map counts identical to V16) | 1.0728 |
| V18a: V16, `allocate_buckets_if_none()` only on the empty branch, against V16 | 0.9967 | 1.0004 |

Per-workload instruction ratios against main (u64 / string / big value). V11 had clang's u64 build at 1.051 and churn at 1.060 / 1.010 / 1.073 over main.

| workload | clang V14 | gcc V12 |
|---|---|---|
| `random insert erase` | 0.873 / 0.942 / 0.880 | 0.956 / 0.942 / 0.827 |
| `build` | 0.928 / 0.974 / 0.921 | 0.992 / 0.893 / 0.792 |
| `churn` | 0.979 / 0.987 / 0.988 | 0.879 / 0.900 / 0.826 |
| `find` | 0.999-1.000 | 0.999 / 0.974 / 0.999 |
| `iterate` | 0.999-1.000 | |

**Scenario matrix** (one map per binary, the same harness with `segmented_map`, a 64-byte value, a transparent string hash, and `-O2`). V14 is under main in every clang cell on both instructions and cycles: `segmented_map` u64 hit 91.2 -> 61.8, `-O2` u64 miss 127.0 -> 85.2. Under gcc (V12) three cells are above main on instructions, none by more than 1.3%: big-value miss 81.9 -> 83.0, `segmented_map` string miss 301.5 -> 303.0, transparent hit 106.2 -> 106.4. One cell is above main on cycles: the big-value hit, 24.5 -> 25.4 (+3.7%) on 1.5% fewer instructions, which V11 read at +5.8%.

**ClickHouse's aggregation benchmark** (`hash-table-aggregation-benchmark`, 100M rows per column, `perf stat` cycles per row, median of 3-5, 5.1.0 -> V14/V16):

| column | clang | gcc |
|---|---|---|
| CounterID | -15.6% | -6.6% |
| AdvEngineID | -19.4% | -7.5% |
| RegionID | -13.5% | -9.9% |
| TraficSourceID | -18.4% | -6.8% |
| UserID | -9.0% | -2.8% |
| WatchID | -11.1% | -3.9% |

The four small columns are almost all hits. V12 (the placement still behind a call under clang) read them 1-3 points better, and WatchID, mostly misses, 8 points worse (-3.2%).

**MySQL 9.7.2** (gcc, `hash join` / `INTERSECT` / `EXCEPT` on a million rows, median of three rotated rounds, seconds):

| | 4.4.0 | 5.1.0 | V12 | V11 (earlier run) | 5.1.0 (earlier run) |
|---|---|---|---|---|---|
| string join | 0.637 | 0.590 | 0.606 | 0.593 | 0.595 |
| int join | 8.097 | 7.826 | 7.911 | 7.849 | 7.868 |
| INTERSECT | 1.593 | 1.550 | 1.550 | 1.535 | 1.542 |
| EXCEPT | 0.252 | 0.269 | 0.260 | 0.262 | 0.272 |

MySQL's hash join calls only `emplace` and `find`, and `hash_join_buffer.cc` compiles to byte-identical code under V11 and V12. So the joins' 0.593 against 0.606 is this harness's run-to-run spread (about 2%), not the change. `EXCEPT` recovers half of #321's gap to 4.4.0 in both runs.

**Code size.** `flatten` inlines the vector's growth path (`_M_realloc_append`) into every insert site. The score binary holds more insert sites than any caller will. There, gcc's `.text` goes 8.73 -> 12.72 MB (+46%; `flatten` +1.95 MB, the inlined walk +0.96 MB) and clang's 6.09 -> 6.93 MB (+14%). A caller's binary with one map moves by less than two kilobytes. The one-map harness's `.text` in bytes:

| | u64 hit | u64 miss | string miss |
|---|---|---|---|
| clang | 5531 -> 7244 | 10946 -> 11618 | 14597 -> 15480 |
| gcc | 7731 -> 7573 | 9411 -> 8789 | 13953 -> 14465 |

**Past the cache, the README's own harness** (2026-09-27, clang, `bench_readme.cpp` one map per binary, a million entries = the geomean of five sizes over the octave, `udm` built once from main and once from this branch, five rounds alternating the order, pinned to one core; median ns per operation, range over the rounds):

| panel | key | 5.1.0 | this | speedup |
|---|---|---|---|---|
| build | u64 | 25.99 [25.88-26.27] | 25.08 [24.95-25.35] | 1.036 |
| buildfree | u64 | 27.82 [27.63-28.20] | 26.96 [26.69-27.11] | 1.032 |
| churn | u64 | 97.10 [96.63-98.57] | 97.95 [96.14-98.94] | 0.991 |
| find | u64 | 32.14 [31.91-32.25] | 31.88 [31.78-31.95] | 1.008 |
| build | str | 90.58 [89.90-90.84] | 90.14 [89.32-90.36] | 1.005 |
| buildfree | str | 107.59 [106.90-108.31] | 106.65 [106.41-107.13] | 1.009 |
| churn | str | 447.32 [447.21-447.87] | 446.46 [445.27-447.95] | 1.002 |
| find | str | 132.97 [132.72-133.31] | 132.75 [132.57-133.21] | 1.002 |

At a million entries the insert waits on memory, and this change saves instructions. Only the integer build moves, 3-4%; the rest is inside 1%. The README's graphs were not redrawn for it. A first run of the alternating comparison shared its core with a stray process and read every cell twice as slow; the table is the re-run.

The same comparison in `maps.sh -r main` (both headers in one binary) read the integer `find` at 0.81 at all five sizes. That is the harness's inlining band, not the change: one map per binary, `count()` at a million entries retires 50.24 instructions under clang and 49.22 under gcc with either header, cycles within 1%. The real effect is smaller and belongs to the caller. With the fill loop in the same function as the lookup loop, clang's lookup went 51.74 -> 54.74 instructions, because the inlined insert changes the register allocation of the function around it. With the fill moved into its own function the difference is zero.

What this says and does not say: one machine, in cache (50000 entries one-map, the score's sizes) except the million-entry table; the counts are the decisive numbers. MSVC takes the gcc branch of the macro and gets no `flatten`, and has not been measured. The compiler split is a measured choice per compiler, not a principle. A third compiler, or a future clang or gcc, may want the other shape, and the one-map harness and `solo.sh` are what would tell.

**Later (2026-09-27):** under clang the part after a home-group miss went behind a call again (`find_or_place_miss` `noinline`, as in V12), taking the V14-against-V12 trade the other way after a caller-side cliff was measured, see [udb3's `++h[key]` ran 70% slower under clang on 5.2.0 than on 5.1.0](#udb3s-hkey-ran-70-slower-under-clang-on-520-than-on-510-and-it-was-a-loop-variable-stored-and-reloaded-across-a-store-with-a-late-address-on-zen-4-that-with-a-division-on-the-way-to-the-next-key-stops-the-loop-overlapping-its-misses-5x-in-a-loop-with-no-map-at-all-the-part-of-the-insert-after-a-miss-is-called-again-under-clang). The shape search then made that call the shape for gcc too, see [The shape search](#the-shape-search-sixteen-combinations-of-what-the-insert-inlines-each-judged-by-its-worst-ratio-to-the-best-combination-anywhere-measured-with-the-rule-fixed-before-the-results-it-picks-one-shape-for-every-compiler----the-home-group-lookup-inlined-the-miss-path-and-the-walk-past-home-called----worst-111-under-clang-and-158-under-gcc-where-520s-everything-inlined-reads-332-and-354).

### `insert()` and `emplace()` look the key up first when the arguments already are the value: a set's string hit 2.2x, a map's `insert({k, v})` hit 4.5x under clang, and `emplace(k, new int)` must still build its value

*2026-09-27 · #321 (follow-up) · kept · Ryzen 9 7950X, clang 22 and gcc 16, one map per binary: `scripts/ab/try_emplace_hit.cpp` with the operation swapped for `insert`, `solo.sh`, `perwl.sh`*

#321 put `try_emplace`'s hit and common placement in front of any call. Every other single-element insert still built the value first (`emplace_back`), probed, and popped it on a hit. A set has no `try_emplace` at all. The comment on `do_insert_hashed` had named this as "left for its own change".

The shape: `do_try_emplace`'s body is `do_find_or_place<Piecewise>(key, args...)`. `emplace()` routes to it when its arguments are one `value_type`, or for a map a `Key` and a `mapped_type` (each by value or reference). The set's transparent `emplace(K&&)` routes to it too. Every `insert()` overload goes through `emplace()`, and all six are now `always_inline`. What is built is built from the same arguments as before; only a present key skips it.

**Instructions and cycles per operation, #321's merged header against this, 50000 entries** (map = `map.insert({k, v})`, set = `set.insert(k)` with `k` a non-const lvalue for a miss and a const one for a hit, so both `insert` overloads are covered):

| | clang instr | clang cycles | gcc instr | gcc cycles |
|---|---|---|---|---|
| map hit u64 | 88.0 -> 50.3 | 121.4 -> 26.8 | 58.2 -> 43.2 | 27.3 -> 23.6 |
| map hit string | 300.6 -> 227.2 | 191.1 -> 175.9 | 282.2 -> 227.9 | 192.9 -> 177.2 |
| map miss u64 | 98.0 -> 81.2 | 99.7 -> 40.9 | 76.0 -> 67.9 | 31.5 -> 29.1 |
| map miss string | 331.6 -> 311.9 | 160.0 -> 148.6 | 310.9 -> 305.1 | 153.4 -> 150.6 |
| set hit u64 | 80.0 -> 49.3 | 33.0 -> 21.1 | 55.1 -> 42.2 | 22.4 -> 20.3 |
| set hit string | 270.0 -> 104.6 | 185.1 -> 83.4 | 251.7 -> 107.0 | 181.1 -> 82.2 |
| set miss u64 | 92.0 -> 76.2 | 40.3 -> 33.1 | 65.0 -> 63.9 | 25.3 -> 25.3 |
| set miss string | 304.8 -> 279.8 | 147.8 -> 120.3 | 277.7 -> 278.8 | 148.4 -> 110.4 |

The string hits gain because the key is no longer copied. Clang's integer map hit took 121 cycles for 88 instructions in the old shape; why was not looked into, since the shape is gone. `try_emplace`'s own counts are identical to the merged header's in all eight cells of its harness. The score (`solo.sh` against the merged header): clang 0.9962, gcc 0.9979, inside the band. Every workload's instructions are identical except clang's `find` at 1.017 (u64) and 1.026 (big value). This change does not touch that code: it is the inliner, the same caller-side effect as in [`try_emplace` split at the home group: ](#try_emplace-split-at-the-home-group-the-hit-and-the-common-placement-inlined-into-the-caller-the-walk-past-home-behind-a-call-for-clang-only-and-every-scored-workload-at-or-under-mains-instruction-count-on-both-compilers).

**Three shapes lost on the way.**

- V19 routed only `insert(value_type const&/&&)` and missed the common case. `set.insert(k)` with a non-const `k` binds to the template `insert(P&&)`, which went to `emplace()`, so the set miss did not move at all.
- V20 put the dispatch in `emplace()` and left the `insert()` overloads un-annotated. gcc's integer map miss went to 94.9 instructions against 76.0 and the set miss to 89.9 against 65.0: `insert` stayed a call and the caller's map was spilled around it, #321's mechanism again. `always_inline` on the six overloads (V21) took it back.
- V19-V21 built `try_emplace`'s `piecewise_construct` tuples in `do_try_emplace` and handed them down. That cost clang 6 instructions on an integer `try_emplace` miss, because the tuples were held across the lookup. The `Piecewise` flag forms them at the placement (V22).

**The rule has to stop at conversions**, which ASan found. `unique_ptr_fill` calls `emplace(k, new int(i))` twice per key, and its comment says the element "is still constructed, so there's no memory leak here". Looked up first, the second call built nothing, and the pointer leaked, 1000 per map type. So a key-first `emplace()` takes only arguments that already are what they would become, where skipping construction skips a copy and nothing else. `emplace(k, 20)` into a map whose mapped type is built from an `int` still builds and destroys it, and a test counts that. MySQL's hash join (`emplace(key, LinkedImmutableString{nullptr})`) passes a `mapped_type` and takes the new path.

What this says and does not say: in cache only, one machine; MySQL, ClickHouse and the million-entry harness were not re-run. `emplace(Args...)` with anything else, the hinted `emplace_hint`, and `insert_or_assign` keep their shapes. A present key now leaves an rvalue `insert` argument unmoved, where before it was moved from into an element that was then popped.

### udb3's `++h[key]` ran 70% slower under clang on 5.2.0 than on 5.1.0, and it was a loop variable stored and reloaded across a store with a late address: on Zen 4 that, with a division on the way to the next key, stops the loop overlapping its misses, 5x in a loop with no map at all; the part of the insert after a miss is called again under clang

*2026-09-27 · #310, #321, #323 · superseded · Ryzen 9 7950X, clang 22 and gcc 16; udb3 with its own `test.cpp` and flags, `scripts/ab/insert_sweep.cpp`, `scripts/ab/spill_trap.cpp`, `scripts/ab/try_emplace_hit.cpp`, `solo.sh`, `perwl.sh`*

Under clang, 5.2.0's fully inlined insert pushed udb3's loop variables to the stack, and that made the loop stop overlapping its cache misses. Calling the part after a home-group miss again took udb3 insert-only from 81.2 to 41.9 ns per input.

**udb3** (80M `uint32_t` inputs, 16.6M distinct keys, its `Hash32`), ns per input, median of three, pinned:

| | 4.4.0 | 5.1.0 | 5.2.0 | 5.2.0, insert behind a `noinline` call | this |
|---|---|---|---|---|---|
| insert-only (`++h[key]`), gcc | 88.4 | 80.7 | 80.1 | 81.9 | = 5.2.0 |
| insert-only, clang | 89.4 | 47.8 | **81.2** | 47.9 | **41.9** |
| insert+delete (`try_emplace`, `erase`), gcc | 59.9 | **83.5** | **60.3** | 84.5 | = 5.2.0 |
| insert+delete, clang | 79.7 | 47.2 | **60.6** | 45.9 | **41.6** |

#310's own complaint, gcc's insert+delete at 83.5 against 4.4.0's 59.9 (59.4 and 83.9 in the issue's run), was already fixed by 5.2.0 (#321, #323) (corrected 2026-10-02: said "at 83.5 against 4.4.0's 59.4"). The clang rows show what 5.2.0 did. Clang, 5.1.0 against 5.2.0 on insert-only: the same L1D misses (0.369 G, 0.371 G), the same dTLB misses (0.152 G), the same branch misses, 16% fewer instructions and 72% more cycles. The same misses stopped overlapping.

**Not the map, found in a loop without one** (`spill_trap.cpp`: two arrays of 16M entries, a key, a load of `idx[key]`, a load of `val[idx[key]]`), cycles per iteration:

| | clang | gcc |
|---|---|---|
| none of: generator state through memory, a 64 bit `%`, `++` on the found element | 48.1 | 52.2 |
| any one or two of them | 56.8-75.6 | 54.1-75.9 |
| all three | **380.7** | **380.4** |
| all three, the store to a fixed address instead | 64.6 | 68.1 |
| all three, only the divisor reloaded (never stored) | 80.6 | 80.6 |

The trap needs three things:

1. a variable stored to and reloaded from the stack every iteration;
2. a store in between whose address comes from a cache miss;
3. the division between the reload and the next iteration's misses.

The reload waits for that store's address. udb3 has all three: `y % (n >> 2)`, the splitmix state behind `&x`, and `++h[key]`. The map's part is register pressure: 5.2.0 inlines the whole insert, and the caller's loop variables no longer fit in registers.

**The sweep, and how chaotic it is** (`insert_sweep.cpp`, `map<uint64_t, uint64_t>`, cycles per operation).

- Build from empty is 4-12% faster on 5.2.0 than 5.1.0 at every size from 50k to 16M on both compilers.
- Churn under clang goes 744 -> 505 at 16M; under gcc it is unchanged.
- The hit (`++m[k]`, key `r() % n`) runs at two speeds 2.2-2.6x apart at every size (corrected 2026-10-02: said "2.2x"): 31.8 against 82.5 at 50k, 222 against 490 at 16M. Which binary is fast follows no pattern: clang 5.1.0 fast, clang 5.2.0 slow inlined and slow called, gcc slow in every version.
- Without the store (`m[k]`) or without the division (a mask) the gap goes, and 5.2.0 is ahead: 58.9 against 68.0, 59.3 against 67.5 cycles at 1M. It is the spill trap, set off by the harness's own `r() % n`.
- One cold branch decides it. The first version of this change left `allocate_buckets_if_none()` in the inlined part, in an `else` that only an empty table takes, and the hit read 145 cycles at 1M. Moved into the called part, 56.

**What shipped: the part after a home-group miss (`find_or_place_miss`, the placement and the walk) is `noinline` under clang, as in #321's V12, and inlined under gcc, byte-identical to 5.2.0** (checked on five binaries). Under clang, against 5.2.0:

| | 5.2.0 | this |
|---|---|---|
| udb3 insert-only / insert+delete, ns | 81.2 / 60.6 | 41.9 / 41.6 |
| sweep hit at 1M / 16M, cycles | 143 / 492 | 56 / 181 |
| sweep build at 1M / 16M | 62 / 191 | 68 / 211 |
| sweep churn at 1M / 16M | 157 / 505 | 159 / 509 |
| one map, instructions (cycles): hit u64 | 51.3 (25.4) | 45.8 (24.2) |
| hit string | 109.6 (87.1) | 105.2 (86.6) |
| miss u64 | 81.2 (33.8) | 101.1 (39.3) |
| miss string | 294.0 (123.8) | 319.9 (129.3) |
| score, `solo.sh` against 5.2.0 | | 0.9698 |

The score's per-workload instructions under clang, against 5.2.0 (u64 / string / big value): build 1.149 / 1.045 / 1.129, churn 1.075 / 1.031 / 1.079, insert-erase 1.057 / 1.033 / 1.053, find and iterate within 2%.

ClickHouse's aggregation benchmark, cycles per row against 5.1.0 (the same baseline as in [`try_emplace` split at the home group: ](#try_emplace-split-at-the-home-group-the-hit-and-the-common-placement-inlined-into-the-caller-the-walk-past-home-behind-a-call-for-clang-only-and-every-scored-workload-at-or-under-mains-instruction-count-on-both-compilers)):

| column | this | 5.2.0 |
|---|---|---|
| CounterID | -16.7% | -15.6% |
| AdvEngineID | -21.9% | -19.4% |
| RegionID | -15.2% | -13.5% |
| TraficSourceID | -20.8% | -18.4% |
| UserID | -8.2% | -9.0% |
| WatchID | -2.9% | -11.1% |

This is #321's V14-against-V12 trade taken the other way, now that one side of it has a measured cliff. The price under clang: the score 3%; ClickHouse's WatchID (100M rows, almost all distinct) 8 points of the 11 that 5.2.0 had won; an insert of a fresh key 16% more cycles in cache (4% for a string key). What it buys: 1.9x in udb3 and 2.6x in the sweep's hit, in the loops that spill. A cliff that depends on the caller's code was taken as the worse failure; the numbers to reverse that decision are here.

Under gcc no shape avoided the trap in udb3's insert loop (about 80 ns in every version and shape). gcc keeps 5.2.0's shape, because called it lost udb3's insert+delete (60.3 -> 84.5).

**Later (2026-09-27):** the same called shape for gcc was measured once more against the caller corpus and rejected again: the score 0.940, ClickHouse's WatchID +22.0% against 5.1.0 where 5.2.0 read -3.9%, see [The caller corpus](#the-caller-corpus-nine-loops-the-way-programs-write-them-one-binary-each-both-compilers-in-cache-and-past-it-judged-by-each-headers-worst-loop-and-not-by-a-geomean-520s-worst-was-33x-under-clang-and-36x-under-gcc-the-clang-miss-call-of-310-brings-clangs-to-110-and-under-gcc-nothing-that-removes-the-cliff-is-worth-its-price).

**Later (2026-09-27):** superseded by the shape search: the call is now the shape for every compiler, gcc included, picked by a fixed rule over sixteen shapes. The clang numbers here stand. See [The shape search](#the-shape-search-sixteen-combinations-of-what-the-insert-inlines-each-judged-by-its-worst-ratio-to-the-best-combination-anywhere-measured-with-the-rule-fixed-before-the-results-it-picks-one-shape-for-every-compiler----the-home-group-lookup-inlined-the-miss-path-and-the-walk-past-home-called----worst-111-under-clang-and-158-under-gcc-where-520s-everything-inlined-reads-332-and-354).

What this says and does not say: one CPU. The trap needs the three ingredients, and it was measured on Zen 4 only. No shape of the map can promise that a caller's loop does not spill, which is why `doc/benchmarks.md` now says what the ingredients are. A benchmark that picks keys with `%` and stores into what it found measures this, and this project's own sweep harness did.

### The caller corpus: nine loops the way programs write them, one binary each, both compilers, in cache and past it, judged by each header's worst loop and not by a geomean; 5.2.0's worst was 3.3x under clang and 3.6x under gcc, the clang miss call of #310 brings clang's to 1.10, and under gcc nothing that removes the cliff is worth its price

*2026-09-27 · #310, #311, #312, #321 · superseded · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/caller_corpus.{cpp,sh}`*

Each loop stands for a slowdown that happened:

- udb3's insert and insert+delete (#310);
- `++m[f(r() % n)]` and churn with keys from a modulo (the spill trap in its plainest form);
- `++m[k]` with a mask (#321's counting loop);
- a build from empty;
- a `std::string` hit;
- `segmented_map` with a 12 byte key built field by field and hashed through memory (#311, #312);
- MySQL's `EXCEPT` call pattern.

Each at 65536 and 4194304 entries, three rounds, every header once per cell and round, median; instruction counts deterministic.

Worst ratio of a header's cycles to the fastest header in the same cell, over all 18 cells:

| | 5.1.0 | 5.2.0 | #310 (clang: miss called) | V24 (miss called, both compilers) | V28 (the whole insert behind two calls) |
|---|---|---|---|---|---|
| clang | 1.29 | 3.33 | **1.10** | = #310 | 1.21 |
| gcc | 1.58 | 2.76 / 3.59 | = 5.2.0 | 1.57 | 1.59 |

Two runs for gcc; the second had 5.2.0, V24 and V28 only. 5.2.0's worst read 2.76 and 3.59, in the same loop, `bump_mod`. The cells behind them, cycles per operation, 65536 / 4194304 entries:

| | clang 5.2.0 | clang #310 | gcc 5.2.0 | gcc V24 | gcc V28 |
|---|---|---|---|---|---|
| `bump_mod` | 90.2 / 365.3 | 27.1 / 156.9 | 91.4 / 370.4 | 25.5 / 140.3 | 33.3 / 192.2 |
| `churn_mod` | 134.8 / 568.4 | 80.7 / 271.3 | 108.5 / 359.9 | 134.3 / 566.0 | 140.1 / 573.7 |
| `udb3_del` | 102.7 / 222.4 | 75.6 / 161.2 | 112.3 / 313.7 | 129.1 / 329.3 | 121.8 / 321.0 |
| `build` | 48.1 / 118.1 | 52.2 / 129.4 | 44.9 / 108.7 | 51.9 / 133.1 | 53.1 / 145.2 |
| `mysql_except` | 70.8 / 245.0 | 74.4 / 262.7 | 74.7 / 217.7 | 85.2 / 261.6 | 88.3 / 290.7 |
| `bump_mask` | 25.0 / 156.5 | 24.7 / 154.9 | 25.4 / 141.0 | 23.4 / 137.9 | 30.7 / 186.8 |

Three things it shows.

First, the inlined shapes are fastest wherever the caller does not spill and fall off a 2.3-3.6x cliff where it does. Which loop that is changes with the compiler and with unrelated lines: a `reserve(1)` in the sweep harness moved 5.2.0's hit from 143 to 62 cycles.

Second, a call boundary bounds the cliff, because the caller's loop variables then live in callee-saved registers whatever the map does. With only the home-group hit inlined, the lean hit path has not tripped the trap in any loop of this corpus under clang. It did in one loop of the earlier five-loop version, where the map was passed by reference (`++m[f(r() % n)]` at 1M: 141 against 5.1.0's 72 cycles). So it lowers the odds and does not remove them.

Third, V28 put the whole insert behind a call, with the hit in a function of its own and the rest tail-called. It loses to #310 in nearly every cell and costs in cache (one map, hit u64 27.4 against 25.1 cycles under clang, a miss up to 45% under gcc), so it was dropped. The earlier five-loop corpus had recommended V28; the nine-loop one with two sizes overturned it.

Under gcc the call that fixes clang costs more than it saves. V24 takes gcc's worst from 3.59 to 1.57, and the one loop it rescues is `bump_mod`. Against that:

- gcc's score reads 0.9404 (instructions: build 1.338 / 1.095 / 1.294, u64 / string / big value; churn 1.186 / 1.088 / 1.180; insert-erase 1.177 / 1.077 / 1.212);
- ClickHouse's WatchID (100M rows, almost all new keys) +22.0% against 5.1.0, where 5.2.0 read -3.9%;
- UserID +0.3%, where 5.2.0 read -2.8%;
- udb3's insert+delete 83.9 against 59.9 ns.

The likely mechanism is #321's V10 one: a call that receives the map's `this` keeps a map declared in the caller in memory instead of in registers, and every insert of a new key pays for it. So gcc keeps 5.2.0's shape and its cliff, and `doc/benchmarks.md` says what a caller can do about it.

**Later (2026-09-27):** the gcc decision was reversed by the shape search. This entry weighed ClickHouse and udb3 above the corpus loop by judgment; a rule fixed before the results, the smallest worst ratio over all of them, picks the call for gcc too. See [The shape search](#the-shape-search-sixteen-combinations-of-what-the-insert-inlines-each-judged-by-its-worst-ratio-to-the-best-combination-anywhere-measured-with-the-rule-fixed-before-the-results-it-picks-one-shape-for-every-compiler----the-home-group-lookup-inlined-the-miss-path-and-the-walk-past-home-called----worst-111-under-clang-and-158-under-gcc-where-520s-everything-inlined-reads-332-and-354).

What this says and does not say: nine loops are a sample, chosen from slowdowns that happened, and a tenth may disagree; one CPU. The corpus is the check for any change to what an insert inlines (`CLAUDE.md`, the measurement table): its worst-ratio line, not the score, is what such a change has to hold.

### The shape search: sixteen combinations of what the insert inlines, each judged by its worst ratio to the best combination anywhere measured, with the rule fixed before the results; it picks one shape for every compiler -- the home-group lookup inlined, the miss path and the walk past home called -- worst 1.11 under clang and 1.58 under gcc, where 5.2.0's everything-inlined reads 3.32 and 3.54

*2026-09-27 · #310 · kept · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/shape_search/`*

This replaced a week of one-variant-at-a-time argument, in which the gcc choice flipped twice.

The four switches, each on or off:

- `do_find_or_place` (the home-group lookup) inlined or called;
- `find_or_place_miss` inlined or called;
- `find_or_place_far` inlined or called;
- `flatten` on `append_value` or not.

`gen.py` writes all sixteen shapes from the tree's header. Every shape is measured on the caller corpus (18 cells, cycles), udb3 (2 modes, ns), ClickHouse (WatchID and CounterID, cycles) and the score (15 workloads, instructions, reported beside the grade since instructions are not time). The grade is the worst ratio over the timed metrics, each against the best shape on that metric.

| | clang worst | where | clang score instr | gcc worst | where | gcc score instr |
|---|---|---|---|---|---|---|
| hit inlined, miss and walk called, flatten (**shipped**) | 1.11 | corpus build | 1.050 | 1.58 | corpus churn 4M | 1.105 |
| hit inlined, miss called, walk inlined, flatten | 1.13 | corpus build | 1.047 | 1.57 | corpus churn 4M | 1.107 |
| hit called, miss called | 1.17-1.18 | udb3_del, mysql 4M | 1.06 | 1.59-1.60 | corpus churn 4M | 1.14-1.16 |
| hit called, miss inlined | 1.27-1.28 | bump_mask 4M | 1.04 | 1.61-1.67 | bump_mask / bump_mod 4M | 1.12-1.15 |
| everything inlined (5.2.0) | 3.32-3.36 | bump_mod 64k | 1.004-1.011 | 3.54-3.58 | bump_mod 64k | 1.000-1.070 |

What the shipped shape gives up against the best shape on each metric:

| | gcc | clang |
|---|---|---|
| corpus churn at 4M | 1.58 | |
| corpus build | | 1.11 |
| udb3 insert+delete | 1.39 (83.7 against 59.9 ns) | |
| ClickHouse WatchID | 1.24 (373.6 against 301.6 cycles per row) | 1.10 (352.3 against 321.3) |
| ClickHouse CounterID | 1.07 | |

What it removes: the 3.3-3.6x cliff in `bump_mod` on both compilers, and udb3's 1.9x under clang. The `flatten` and walk switches move nothing by more than 0.02 in the grade under either compiler. They are kept where clang's grade put them: flatten on and the walk called. Under gcc the inlined walk reads 0.01 better, inside the noise (corrected 2026-10-02: said "as they were measured best").

Per metric, where the switches go (cycles or ns, best to worst group):

- udb3 under clang wants the miss called (41.8 against 60-81 inlined);
- udb3 under gcc wants everything inlined, walk included (59.9; with only the walk called 82.7);
- ClickHouse WatchID wants the miss inlined on both compilers (clang 321 against 352, gcc 302-327 against 350-390);
- calling the hit loses on every ClickHouse cell.

No shape wins all of them, which is why a rule and not a judgment has to pick.

What this says and does not say: the grade weights a synthetic corpus loop the same as a real program's benchmark, on purpose. The rule is the smallest worst case, which is what "not brittle" means. A different rule (e.g. weighting real programs) would pick 5.2.0's shape for gcc again. One CPU. The search is the tool for the next such question: `run.sh gen`, the stages, `run.sh rank`, about three hours unattended.

### `always_inline` on `place_element_at` re-measured on today's insert shape: clang's integer build 1.19-1.22x faster with it, gcc's unchanged, so it stays

*2026-10-02 · no issue · kept · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/place_inline.{cpp,sh}`, one map per translation unit, a build from empty, 5 rounds alternated, median of each run's best of 20 builds; `perf stat` instructions*

The attribute's evidence (14-20% slower one-map builds without it, see [The insert path is split in two by clang, and that is most of the build gap to boost.](#the-insert-path-is-split-in-two-by-clang-and-that-is-most-of-the-build-gap-to-boost)) was taken on `do_place_element`, which the key-first insert removed (8ea5c8e). Since #321 the placement is `place_element_at`, reached from the inlined home-group miss and from inside the `noinline` `find_or_place_miss`, so the cross-read asked whether the old number still describes the shipped shape. Same experiment on today's header, with the attribute against `inline`, `try_emplace` of n keys into an empty map, without/with (above 1.00 means the attribute makes the build faster); instructions per insert include generating the keys, which is the same work for both:

| compiler | key | 32000 | 200000 | 1000000 | instructions without/with |
|---|---|---|---|---|---|
| clang | `uint64_t` | 1.197 (8.53 / 7.12 ns) | 1.221 (9.82 / 8.04) | 1.189 (13.14 / 11.06) | 1.14-1.15 |
| clang | `std::string` | 1.035 | 1.023 | 1.026 | 1.03 |
| gcc | `uint64_t` | 0.986 | 0.994 | 0.983 | 1.000-1.002 |
| gcc | `std::string` | 1.017 | 1.025 | 1.015 | 1.016-1.017 |

**Under clang the attribute is worth 19-22% of an integer build and 2-4% of a string build**, the same size as when it was first measured on `do_place_element`. **Under gcc it changes nothing for integer keys** (identical instruction counts: gcc inlines the function anyway) and 1.5-2.5% for strings. It stays.

What this does not say: the scored suite, a ~90-TU binary whose inlining budget is exhausted, read the opposite sign when the attribute was removed from `do_place_element` on 2026-09-08 (1.7% faster without, clang; 3.9%, gcc). That trade is unchanged and was not re-measured here; the one-map translation unit is what a caller compiles.

## Erase and churn

What an erase costs in `unordered_dense` and what was tried to make it cheaper: the backfill that re-finds the moved value's slot by a second hash, the drift a churned table builds up, and the round trips and prefetches on the erase path. The slot back-pointer that removes the second hash lost twice, because the insert pays what the erase saves. `move_home` is the one drift repair kept.

**Where it stands** (as of 2026-09-12)

- Slot back-pointer: rejected, default and knob. String erase by key 0.893-0.915 from 50000 to 4000000, churn a wash, score 0.9870 clang / 0.9729 gcc, +18.6% live bytes on an integer map (#266, [A slot back-pointer, re-tested across the cache boundary](#a-slot-back-pointer-re-tested-across-the-cache-boundary-the-win-is-real-and-it-is-cancelled-by-the-inserts-that-put-the-elements-there)). With distance nibbles: geomean 0.959 ([Three on the value vector](#three-on-the-value-vector-the-one-part-nothing-had-touched-all-keeping-it-dense)).
- The second hash is about 7 ns for an integer key and about 50 ns for a string key at a million entries ([Why a dense erase is not slow, and where it is](#why-a-dense-erase-is-not-slow-and-where-it-is)).
- `move_home`: kept, 1.26-1.30x on a miss of a churned table in and out of cache, 1.01-1.10x on a hit, 1.08x on the churn round itself, score 1.000 (re-measured 2026-10-02 with random churn keys; the first measurement read a tenth because its harness churned in sequential keys) ([What `move_home` is actually worth, re-measured](#what-move_home-is-actually-worth-re-measured)). Pulling a sibling home on erase: rejected, churn round 39.9 to 60.3 ns.
- Slot-number round trips removed in #260 and #262; the #268 sweep found nothing more ([The #260/#262 sweep run over find and churn](#the-260262-sweep-run-over-find-and-churn-and-it-comes-back-empty)).
- The value walks are bounded for correctness; their 10% clang speedup is an inlining artifact (#254). They carry no `prefetch_index` (#263). Hoisting the moved element's hash: neutral, rejected.

### Three on the value vector, the one part nothing had touched, all keeping it dense

*2026-09-05 · no issue · rejected*

None kept. Scale: growth is 54% of a 200000 element integer build here and 59% of boost's, so the rehash is not where `unordered_dense` loses. The pure insert path is: 140 instructions per insert against boost's 77, spread over a probe, a vector append, a second walk in `place_group` (4% of cycles, the ceiling on any one-pass insert) and the spills of holding two index arrays plus a vector live. The vector's own reallocations are 3% of an integer build and 11% of a 64 byte value build. **Later (2026-09-10):** recounted on a reserved table, the insert is 96.4 instructions against boost's 82.9 under clang and 67.5 against 57.0 under gcc, see [The insert path's instructions, counted one by one](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it).

- **Reserving the values to the index's capacity at every index growth**, so the two grow together: `build64` 0.985, `buildstr` 0.972, `buildbig` 0.967, nothing elsewhere. The bytes copied are the same; now a full copy of the values lands just before a rehash that wants the cache for the index.
- **A slot back-pointer per value** (a parallel `std::vector<value_idx_type>`, +4 bytes per entry), so the backfill repoints the moved element's slot directly instead of hashing its key and walking to it. Wins where that hash is expensive: `churnstr` 1.045, `iestr` 1.024. Costs wherever the vector grows: `build64` 0.953, `buildbig` 0.960, `iebig` 0.977, `churnbig` 0.984; integer churn ties. Net loss on the score, and 19% more memory for an 8 byte value. The robin hood era's note about storing 32 hash bits per value instead measured a 1.4% gain for the same 4 bytes per element, see [Optimization dead ends (verified with paired A/B runs; ...](#optimization-dead-ends-verified-with-paired-ab-runs-re-test-before-assuming-they-still-hold) (corrected 2026-10-02: said "The 2025 note about storing the hash instead measured the same shape"; that note was first committed on 2026-09-02). **Later (2026-09-12):** re-tested across the cache boundary in #266 and rejected again, see [A slot back-pointer, re-tested across the cache boundary](#a-slot-back-pointer-re-tested-across-the-cache-boundary-the-win-is-real-and-it-is-cancelled-by-the-inserts-that-put-the-elements-there).
- **The same back-pointer plus indivi's 4 bit distance nibbles**, so that `erase(iterator)` needs no hash: slot from the back-pointer, counter from the slot's fingerprint, home by reversing the quadratic walk by the stored distance. On find, `erase(it)`, insert on a reserved table: **1.10x faster with string keys** (1006 to 888 instructions per round, one wyhash and two probes gone), **1.10x slower with integer keys** (352 to 363: the saved hash is 8 instructions, the back-pointer on every insert costs more). The score erases by key, so it only costs: geomean 0.959, `build64` 0.831, `buildbig` 0.893, `churn64` 0.900, `iestr` 1.052 the one workload ahead. Memory 31 to 38 MB per million 8 byte values. Not an opt-in either: the win needs an expensive key *and* erase by iterator, and a caller with both has `erase(key)` with the hash already paid by their own `find`.
- **Growing the values with `realloc`** instead of allocate-move-free, for trivially copyable values: a third of the vector's growth cost goes (0.242 to 0.164 ms for 16 byte pairs, 0.713 to 0.476 for 72 byte ones, doubling to 200000), about 1% of an integer build and 4% of a big value one. It needs a container that is not `std::vector`, which `AllocatorOrContainer` already accepts, so it is an opt-in today and not worth changing what `values()` returns.

### Why a dense erase is not slow, and where it is

*2026-09-06 · no issue · info · asked as "don't we have to hash more because of the moved element?"*

Yes. `finish_erase` moves `m_values.back()` into the hole, re-hashes its key and runs a second probe, `slot_of_value`, to find the slot pointing at it: two hashes and two probes per successful erase. At a million entries, erase of a random key plus an insert, against the same two cold probes with no move (a find, an insert, and an erase of the element already last):

| | with the move | without |
|---|---|---|
| `uint64_t` keys | 57.0 ns | 56.2 ns |
| `std::string` keys | 410.4 ns | 377.7 ns |

**For an integer key it is free**: the moved element is always the back of the value vector, which in a churn loop stays hot; `do_erase` first prefetches `m_values.back()`, so that load runs under the counter walk; and the hash is one multiply, so its group access overlaps the erase's probe and the insert's placement. Net of the extra operation the no-move sequence needs, about 7 ns.

**For a string key it costs about 50 ns**: wyhash over 8 to 135 bytes behind a heap pointer is a dependent load and a long chain, and none of it overlaps. The rejected back-pointer measured this from the other side (`churnstr` 1.045, `iestr` 1.024, against `build64` 0.953; see [Three on the value vector](#three-on-the-value-vector-the-one-part-nothing-had-touched-all-keeping-it-dense)), on 200000-entry cache-resident string tables. The charts now cover where the cost is largest, so it is a candidate for the re-test the merged block just had (see [Two optimizations the charts point at](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet)).

**Later (2026-09-12):** re-tested as #266 (the "string erase second hash" in `CLAUDE.md`): erase by key 0.893-0.915 at every size, rejected again because the inserts pay the same, see [A slot back-pointer, re-tested across the cache boundary](#a-slot-back-pointer-re-tested-across-the-cache-boundary-the-win-is-real-and-it-is-cancelled-by-the-inserts-that-put-the-elements-there).

### Pulling a displaced sibling home on erase, to take the churn drift back

*2026-09-06 · no issue · rejected · paired, `uint64_t` keys*

It takes back half the drift of a hit and a third of a miss's, and costs 20 ns per erase. When an erase frees a slot in group g and any of g's eight counters is nonzero, look one group along g's sequence for an entry whose class has a nonzero counter, hash its key to confirm its home is g, and if so move it into the freed slot and decrement the counter. The empty case is one 8 byte load. After 200 turnovers at load 0.76, 1.24M pull-backs in 10M erases:

| groups per lookup | fresh | churned | churned, with pull-back |
|---|---|---|---|
| hit | 1.032 | 1.137 | 1.086 |
| miss | 1.052 | 1.252 | 1.181 |

A separate run of the same setup in the next entry reads 1.143/1.265 churned; the spread between runs is about 1%.

Half the drift of a hit and a third of a miss's, one step deep (corrected 2026-10-02: said "Half the drift, one step deep"). But some counter of the freed group is nonzero on **46% of erases**, fresh or churned: counters count everything that passed the group, not only g's siblings. Each such erase scans the next group and hashes two or three candidates' keys, two or three value loads.

| | base | pull-back |
|---|---|---|
| churn round (erase, two lookups, insert), 50000 entries | 39.9 ns | **60.3** |
| churn round, 2M entries | 231 | 278 |
| hit / miss on the churned table, in cache | 6.20 / 4.19 | 5.81 / 3.58 |
| hit / miss, out of cache | 42.2 / 13.0 | 42.1 / 13.0 |

**Nothing at all** out of cache: a one-step displacement lands in the adjacent block, which the spatial prefetcher already brought in. 20 ns per erase to save 0.4-0.6 ns per lookup in cache is break-even at thirty to fifty lookups per erase, and never out of cache. Not kept; the lazy version is, see [A mutating hit moves itself home](#a-mutating-hit-moves-itself-home-and-that-takes-the-churn-drift-back).

**Later (2026-09-07):** the same adjacent-block reasoning was wrong for `move_home`, which is worth as much out of cache as in it (a tenth as first measured, a quarter with random churn keys, 2026-10-02), see [What `move_home` is actually worth, re-measured](#what-move_home-is-actually-worth-re-measured). This entry's out-of-cache figures are paired and were not re-taken; at 20 ns per erase the rejection stands.

### A mutating hit moves itself home, and that takes the churn drift back

*2026-09-06 · no issue · kept · one map per binary, `uint64_t` keys; `sweep.cpp` mode 5; tests in `move_home.cpp`*

About a tenth off a miss on an in-cache table churned with writes, at no measured cost. The lazy version of [Pulling a displaced sibling home on erase](#pulling-a-displaced-sibling-home-on-erase-to-take-the-churn-drift-back): a hit's home is known without another hash, and whether it has room is one `match_empty` on a group the probe just visited. `move_home` runs on every hit in `try_emplace`, `operator[]`, `insert`, `emplace` and `insert_or_assign`. At home it costs a compare; one group out with room at home, a load, two stores, one zero and the counter walk `erase` already does. Not in `find()`, const or not: callers treat a non-const `find` on a shared map as read-only, so it would be a data race.

Drift, groups per lookup, 50000 entries, load 0.76, 200 turnovers:

| | per hit | per miss |
|---|---|---|
| fresh | 1.032 | 1.052 |
| churned, base | 1.143 | 1.265 |
| one `operator[]` hit per erase | **1.091** | **1.162** |
| four per erase | **1.054** | **1.093** |

It converges rather than plateaus: every displaced entry touched again goes home.

**What it is worth, and the measurement that had to be done three times to find out.** One map per binary, a table churned forty times with a writing hit per round, two runs each:

| 50000 entries, churned | base | move-home |
|---|---|---|
| churn round (`operator[]` hit, erase, miss, insert) | 42.7 ns | 42.5 |
| find, hit | 5.9 | 5.7 (1.04x) |
| find, miss | 5.16 | **4.64 (1.11x)** |
| branch misses, whole run | 7.59M | 6.26M |

**Correction:** the first figure, from a paired run of the two headers in one binary at 50000 entries, was misses 4.02 to 2.70 ns (1.49x), and it was wrong.

At 2M entries every figure is within 1-2% either way: a one-step displacement is the adjacent block. +2% instructions per writing hit. Kept: thirty lines, no cost anywhere measured. The branch misses show the drift's cost: with a quarter of misses continuing past home the stop-or-continue branch mispredicts, with a tenth it does not.

**Later (2026-09-07):** a no-op-controlled re-measurement finds the same gain out of cache (1.099 at 3355443 entries, 1.262 once its harness churned in random keys, 2026-10-02), not "nothing out of cache", see [What `move_home` is actually worth, re-measured](#what-move_home-is-actually-worth-re-measured).

**Correction (2026-10-02):** a 2026-09-07 note in [Three ideas the eighteen-map comparison suggested](#three-ideas-the-eighteen-map-comparison-suggested-all-measured-none-kept) says the churned figures 1.143 and 1.265 do not reproduce (an instrumented header read 1.0358 and 1.0609). That note was wrong. Its harness (`scripts/ab/probe_length.cpp`, `next++`) churns in sequential keys, which the hash spreads as a lattice, so that table barely drifts. With random fresh keys the current header reads 1.136/1.265 churned, 1.094/1.163 with one writing hit per erase and 1.056/1.099 with four: the table above reproduces to about 1%.

The score reads **1.000 under clang and 1.022 under gcc**; the gcc figure is `buildbig` at 1.40, a workload that never calls `move_home`, where gcc's inlining in the harness TU moved (the story of [gcc left `probe` out of line](#gcc-left-probe-out-of-line-and-forcing-it-inline-is-the-largest-single-gcc-gain-on-the-branch)). Scored workloads either grow (a rehash resets the drift) or churn with `find` between erase and insert, which moves nothing. `sweep.cpp` mode 5, added for this, is the scored churn with the hit through `operator[]`.

**The sweep cannot resolve a question this size, and the way to know is to run the same code on both sides.** Two builds of the same working tree against the same baseline:

| | build A | build B |
|---|---|---|
| mode 1 (no lookups, `move_home` never runs), octave 128-255 | **0.876** | **1.158** |
| mode 1, 524K-1M | 0.997 | 1.067 |
| mode 5, 1M | | 0.860 (mode 1: 1.085) |

The one-map binaries tie at 1M to 0.1% with identical counters. A same-code control (the working tree renamed into both namespaces) reads 1.00-1.02 above 128K: not the harness but the code layout of a TU holding two headers, moving 3% at large sizes and 30% at L1-resident ones whenever either header changes, invisible in the within-run intervals the charts draw. Rule, as the `hashstr` control already said: a paired two-header measurement decides a 10% question, not a 3% one; smaller is decided one map per binary, with counters.

Tests in `move_home.cpp`: a churn with a mutating hit every cycle checking every key and value, and a steered one on the identity hash that fills a group, sends one key past it, makes room and provokes the move. Mutation sweep of `uncount` and `move_home`, 41 mutants: 14 caught by a test, 17 by the compiler, 3 hung, 7 survived. Four are the counter decrement, the uncovered decrement recorded in [A miss had no bound](#a-miss-had-no-bound-and-eight-chosen-keys-made-it-loop-forever) (now shared by erase and the move); two are the "already at home" early return and its comparison, which mutated either move an entry to another lane of its home group (legal) or skip the repair. Nothing that changes an answer survived. **Open (2026-10-02):** 7 survived but only six are named here; the seventh is not recorded, so that sentence is unchecked for it. Found by mode 1: `uncount` takes the group pointer, mask, home and counter as arguments loaded *before* the caller's fingerprint store, since a `std::uint8_t` store may alias them and a reload lands on the address chain of every step of the walk. `CLAUDE.md`'s "nothing moves after placement" is qualified by exactly this: a hit inside a write may move one step, to its home.

### What `move_home` is actually worth, re-measured

*2026-09-07 · no issue · kept · `scripts/ab/move_home.{cpp,sh}`, one map per binary, `perf stat`*

**About a tenth of a miss**, in cache and out, **nothing on a hit**, and nothing on the writing path that pays for it. Asked as "I am now sceptical this is of any use", since the drift entry it was justified by does not reproduce (see [Three ideas the eighteen-map comparison suggested](#three-ideas-the-eighteen-map-comparison-suggested-all-measured-none-kept)). The same header with `move_home` a no-op beside it; a reserved table at load 0.80 churned with `hits` writing lookups per round, then the region under test timed alone. **The control is the part that makes it believable**: at `hits` 0 `move_home` never fires, so any difference is code layout. Off over on, above 1.00 means `move_home` is faster:

| entries | control (no writing hits) | one writing hit per round | hits | churn round |
|---|---|---|---|---|
| 52363 (in L2) | 0.951 | **1.107** | 1.041 | 1.012 |
| 838860 (L3) | 0.998 | **1.101** | 1.003 | -- |
| 3355443 (past L3) | 0.997 | **1.099** | 0.991 | -- |

The churn round reads 1.00-1.01. **It is worth the same tenth out of cache as in it** (3.4M entries, a 23 MB index), correcting [A mutating hit moves itself home](#a-mutating-hit-moves-itself-home-and-that-takes-the-churn-drift-back), which said "nothing out of cache".

`perf stat` per lookup at 52363 entries: no writing hits, **27.59 cycles and 0.2118 branch misses against 27.52 and 0.2116**; one writing hit per round, **25.58 and 0.1591 against 28.93 and 0.2177**. Instructions barely move (53.9 against 54.9). A quarter of the branch misses and an eighth of the cycles go, on 0.04 fewer groups per miss: **most of what the drift costs is the stop-or-continue branch becoming unpredictable, not the extra group visit**. Hence a time effect (11%) three times the probe-length effect (3.7%), surviving out of cache where the extra group is in the adjacent block.

**Correction (2026-10-02): it is worth about a quarter of a miss, not a tenth.** `scripts/ab/move_home.cpp` replaced every erased key with a counter (`present[at] = next++`), so a long-churned table held sequential keys, which the hash spreads almost evenly. That table had about a quarter of the real drift (1.061 groups per miss instead of 1.265 at load 0.76, measured with `scripts/ab/probe_length.sh`), so everything above measured `move_home` taking back a drift that was mostly not there. Re-taken with random replacement keys, the same three sizes, clang, three rounds alternated, median, off over on (`scripts/ab/move_home.sh` with its patch updated for today's `move_home(found_in, from_lane, mh)`):

| entries | control (no writing hits) | one writing hit per round, miss | hit | churn round |
|---|---|---|---|---|
| 52363 | 1.000 | **1.297** | 1.097 | **1.079** |
| 838860 | 1.002 | **1.279** | 1.014 | -- |
| 3355443 | 1.006 | **1.262** | 1.017 | -- |

Groups per miss at load 0.76 after 200 turnovers: 1.265 churned, 1.163 with one writing hit per round, 1.099 with four, against 1.052 fresh. The mechanism stated above holds with the corrected drift: whole-run `perf stat` at 52363 entries with one writing hit per round reads 12.6M against 19.3M branch misses (-35%), 1.34G against 1.64G cycles (-18%) and 2.25G against 2.38G instructions (-5.5%); with no writing hits all three are equal. And the churn round itself is 8% faster with `move_home` (35.8 against 38.6 ns), where the sequential-key table read it at 1.012: the inserts and erases of the churn probe the same drifted table. The decision does not change, it was kept; its worth is roughly three times what this entry first recorded.

**What this does not say.** A workload that only reads gets nothing; the control row is that workload and reads 0.997-0.998 at L3 and past it and 0.951 in L2, where `perf stat` shows equal cycles, so that 5% is layout (corrected 2026-10-02: said "reads 1.00"). The gain is all on misses. It pays for a map that churns at a fixed size, is written by key, and is asked about absent keys: a dedup set, a cache with negative lookups, any `++m[k]` counter over a sliding window. No scored workload has that shape, so the score reads 1.000 and always will.

### Hoisting the moved element's hash out of `finish_erase`, so its latency overlaps the erase's own work

*2026-09-08 · no issue · rejected · one map per binary, 200000 entries, three repetitions; asked as "calculate the hash of the last element early but use the result as late as possible"*

Neutral. The premise holds: `finish_erase` computes `mixed_hash(get_key(val))` *after* `val = std::move(m_values.back())`, a store no compiler can prove does not alias the key, so the hash waits for the move. It is the `fill_buckets_from_values` shape, but fixable only by computing the hash in the caller and passing it in. **All three are instruction-neutral where it matters and none is a time win.**

| placement | instructions | time |
|---|---|---|
| before the counter walk, guarded for no move | +9 integer, +32 string (the branch, `back_mh` live across the callback) | `churn64` 10% slower |
| before the counter walk, unguarded | +4 | 12% slower on integers: the hash's loads issue *in front of* the erase's own probe |
| after the counter walk | 299.5 against 299.5 | `churn64` 32.19 ns against 32.16, `churnstr` 279.3 against 270.0 |

**And the measurement is the lesson.** A first run showed `churnstr` 265.0 against 270.0, the predicted 1.8% win; the next two read 288.3 and 279.3 against 273.6 and 267.4. Three runs of churnstr at 200000 spread 8.8%, so nothing this size resolves there. The instruction counts, spread 0.4%, say neutral. It reproduces the robin hood era's "computing the moved element's hash early: out-of-order execution already hides these latencies" (see [Optimization dead ends (verified with paired A/B runs; ...](#optimization-dead-ends-verified-with-paired-ab-runs-re-test-before-assuming-they-still-hold)) and the erase-side `prefetch_key` at 1-2% in cache and nothing out of it (see [Seven ideas from reading folly F14 and from the F14Vector string gap](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept)). Same reason: `do_erase` already prefetches `m_values.back()` before the counter walk, and the out-of-order window covers the rest.

### `finish_erase` packed a slot that the store took straight back apart

*2026-09-11 · #260 · kept · Ryzen 9 7950X, clang 22 and gcc 16, `perf stat`, single-header binaries*

5 instructions off every erase on both compilers; time follows under gcc only. The backfill was

```cpp
m_buckets.index_at(slot_of_value(mh, values_idx_back)) = value_idx_to_remove;
```

`slot_of_value` packed the group base and lane into `group_idx * slots_per_group + lane`, and `index_at`, its only caller, divided it back into `slot / slots` and `slot % slots`. Clang folds this on `erase(iterator)` (slot to `erase_group_slot`) but not here. **gcc folds it nowhere**: `shr $0x4` and `and $0xf` remain in `erase(iterator)`, `erase(key)` and the writing hit's `move_home`, which all pack a slot from `probe_result` for a consumer that unpacks it: three more sites, fed by four producers, fixed in #262 (corrected 2026-10-02: said "four more sites") (see [Nothing holds a flat slot number any more](#nothing-holds-a-flat-slot-number-any-more-and-gcc-was-the-one-paying-for-it)). `repoint_value(mh, value_idx, new_value_idx)` is the same walk storing in place, with the same `delta == m_group_mask` bound and `on_error_key_changed()` exhaustion #254 gave the original. `index_at` is gone.

**The whole binary is identical apart from `finish_erase`** and the addresses after it. In the tail, `shl $0x4 / add / mov / shr $0x4 / imul $0x58 / add / and $0xf` becomes one `mov %esi,0x18(%r11,%r14,4)`; clang then hoists `add %rax,%r11` and `movzwl` out of the lane loop and moves `lanes &= lanes - 1` after the compare, so the common found case skips three more.

Instructions per erase, build-only subtracted, three repeats (no variation):

| | 50k | 200k | 1M |
|---|---|---|---|
| clang, before | 119.57 | 119.43 | 117.59 |
| clang, after | **114.50** | **114.36** | **112.58** |
| gcc, before | | 84.95 | 83.09 |
| gcc, after | | **80.43** | **78.58** |

gcc time: 6.04 to 5.67 ns/erase at 200k, 5.97 to 5.65 at 1M. Clang: flat at 50k and 200k; at 1M the median of nine alternating runs reads 10.44 before, 10.76 after, 3% the wrong way, with loads, stores, branch misses and L1 misses equal to three digits and cycles of the *same* binary swinging 51.7 to 59.4. That is the layout band; everything after `finish_erase` sits 48 bytes lower. Score, one header per binary against `origin/main`: clang **1.0076** (5 of 5), gcc **1.0019** (4 of 5).

Returning `value_idx_type*` for the caller to store through gives a **byte-identical binary** under clang (gcc moves only the benchmark's stack slots), so that choice is cosmetic. #254 predicted this would lose, since `place_group`, returning `void`, measured 0.1-0.2 instructions per insert *worse* and `repoint_value` returns `void`; that prediction was about the wrong thing (and its explanation is retracted in [An unbounded probe that hangs](#an-unbounded-probe-that-hangs----and-the-10-speedup-that-came-with-it-is-an-inlining-artifact)). What goes here is a pack and an unpack, visible on both compilers and in the disassembly, which is how a property of the change differs from an inlining accident.

### An unbounded probe that hangs -- and the 10% speedup that came with it is an inlining artifact

*2026-09-11 · #254 · kept (the bound); the speedup's explanation retracted · Ryzen 9 7950X, clang 22 and gcc 16, `perf stat`, single-header binaries*

**The performance half of this entry is a retraction of what it said when first written.**

`slot_of_value()` ("which slot points at this value", from `erase(it)`, `extract(it)`, `replace_key()` and `finish_erase`) probed with `while (true)`. Its precondition, that some slot points there, always holds for the map's callers. But the map hands out a **mutable key**:

```cpp
auto it = m.find("key 42");
it->first = "something else entirely";  // compiles; README lists no-const-Key as a disadvantage
m.erase(it);                            // ran until killed, at 100% of one core
```

The caller's mistake (`replace_key()` is the supported way), but other maps make it impossible to write, this one compiles it and then spun. The miss path had the same shape, found by a fuzz hang that "survived every other target and 767 unit tests" (see [A miss had no bound](#a-miss-had-no-bound-and-eight-chosen-keys-made-it-loop-forever)). This one was found when `scripts/ab/merge.cpp` moved the key into the destination and then called `src.erase(it)`: fine for a `uint64_t` key, a hang for `std::string`. The bound is `delta == m_group_mask`, as in `probe_from`; exhaustion calls `on_error_key_changed()`, which throws or aborts. "Not found" is not an option: every caller's contract says the element is there, and the return is a slot number. **That is the whole justification, and it is enough.**

*What was claimed.* The bound also measured 10% faster. Instructions per erase, erase path alone, three repeats, reproduced in two containers:

| variant | clang, as written |
|---|---|
| unbounded `while (true)` | 132.99 / 132.98 / 132.97 |
| bounded, exhaustion is `[[noreturn]]` | 119.63 / 119.65 / 119.50 |
| bounded, exhaustion returns a `0` sentinel | 136.01 / 136.00 / 136.14 |

The entry explained it as: a loop with one value-producing exit and a dead end is cheaper than one the compiler cannot prove terminates. That went into `CLAUDE.md` as a rule.

**Correction:** the numbers reproduce; the explanation was invented to fit them. The unbounded version already had exactly one value-producing exit. Two controls, neither run before the rule was written:

| control | unbounded | `[[noreturn]]` | sentinel |
|---|---|---|---|
| clang, as written | 132.95 | 119.54 | 136.11 |
| clang, `slot_of_value` forced `noinline` | 137.05 | 138.10 | 137.05 |
| gcc, normal inlining | 85.49 | 85.06 | 84.52 |

**Pin the inlining and the effect disappears. Change compiler and it never existed.** In both controls the three are within one instruction, and no variant is consistently cheapest: `[[noreturn]]` is worst by the compare it adds under clang `noinline`, the unbounded loop is worst under gcc (corrected 2026-10-02: said "and `[[noreturn]]` is the *worst* by the compare it adds"). Adding the cold call changed what clang inlined around `slot_of_value`; any unrelated edit can flip it back. Nothing about `[[noreturn]]` or bounded loops transfers.

*What is retracted is the explanation, not the speedup.* Under clang the shipped binary beats `origin/main`: 8.765 ns against 9.233 per erase at two hundred thousand elements and 6.924 against 7.568 at fifty thousand, medians of nine, **0.92 to 0.95**. Nothing says it survives the next compiler release; gcc gets nothing (85.5 against 85.1 instructions). Bank it, build no rule on it; the hang justifies the change alone.

**instructions and time come apart here.** The `noinline` controls retire 137.0 / 138.1 / 137.1 instructions and take 9.718 / 9.332 / 9.240 ns: the unbounded loop, tied for fewest instructions, is the slowest by 4-5% (corrected 2026-10-02: said "most instructions, fastest"). The erase path is partly memory-bound, so a count settles "different work", not "faster".

It also re-explains the insert path: `place_group` and the rehash's placement loop, given the same bound, came back 0.1-0.2 instructions per insert *worse*, written up as "they return `void`, which is a different shape". Simpler: their bound did not move clang's inlining, so only the compare's cost remained, which the controls show is the honest baseline everywhere.

*The rule that came out of it*, now in `CLAUDE.md`: an instruction count is immune to code layout but **not** to inlining. Force the function out of line and re-measure, and check a second compiler.

### Nothing holds a flat slot number any more, and gcc was the one paying for it

*2026-09-12 · #262 · kept · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/solo.sh` and `perf stat`*

gcc's erase workloads lose 2-5.5% of their instructions; clang's time is inside the layout band. After #260 (see [`finish_erase` packed a slot that the store took ...](#finish_erase-packed-a-slot-that-the-store-took-straight-back-apart)) the round trip remained on the *lookup* half of every erase: `probe_result` carried `slot = group_idx * slots_per_group + lane`, which `erase_group_slot` split into `slot / slots_per_group` and `slot % slots_per_group`, and `move_home` likewise. Producers: `probe_from`, the two home-group loops, `slot_of_value`. Clang folded most of it (left: an `and` and a zero-extending `mov` per site). **gcc folded none of it.** `shr $0x4` / `and $0xf` pairs in a three-entry-point TU:

| | gcc before | gcc after | clang before | clang after |
|---|---|---|---|---|
| `erase(iterator)` | 2 | 0 | 0 | 0 |
| `erase(key)` | 2 | 0 | 0 | 0 |
| writing hit, through `move_home` | 2 | 0 | 0 | 0 |

`probe_result` now carries `group_idx` and a `std::uint8_t lane`; `slot_of_value` returns the pair as a `group_slot`; `erase_group_slot`, `move_home`, `do_erase` and `located` take it. **The struct does not grow**: 12 bytes for `group`, 24 for `group_big`, so `probe_past_home`'s return-register question never comes up. Instructions, score binaries, one header per binary, candidate over baseline:

| workload | gcc | clang |
|---|---|---|
| `uint64_t` random insert erase | **0.9549** | 0.9859 |
| `uint64_t` churn at a fixed size | 0.9778 | 1.0025 |
| `std::string` random insert erase | **0.9451** | 0.9905 |
| `std::string` churn | 0.9808 | 0.9991 |
| `std::string` 50% probability to find | **0.9749** | 0.9893 |
| `big_value` random insert erase | 0.9610 | 0.9866 |
| `big_value` churn | 0.9805 | 1.0000 |
| every build and iterate workload | 1.0000 | 1.0000 |

The string *find* row is not the erase path: a string key takes the out-of-line `probe_past_home`, which returns `probe_result`, and the repacked return is cheaper. Yet a `probe_result` returned from an inner function once cost gcc 26% of an integer hit (see [Splitting the probe past the home group](#splitting-the-probe-past-the-home-group-kept-for-keys-whose-compare-is-a-call)), so the axis cuts both ways and nobody has swept it.

**Later (2026-09-12):** #267 swept it, see [`probe_result`'s shape swept two ways, a third argued down from the return sequence](#probe_results-shape-swept-two-ways-a-third-argued-down-from-the-return-sequence-and-the-one-that-ships-is-the-best-of-them).

**And the time is a lesson in why this is settled on instructions.** Score, one header per binary against `main`:

| | ratio | rounds |
|---|---|---|
| gcc | 1.0017 | 5 of 5 candidate-faster |
| clang, default alignment | 0.9975, then 0.9956 on a rerun | candidate-slower |
| clang, `-falign-functions=32` | **1.0052** | 5 of 5 candidate-faster |

Same source and compiler, opposite sign, from where the functions landed. Two runs of the *same two binaries* are one layout sample, not two. The instruction counts never moved.

### The two value walks do not want the index prefetch the probe wants

*2026-09-12 · #263 · kept · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/solo.sh`*

`slot_of_value` and `repoint_value` no longer open a group with `prefetch_index`, inherited from `probe_from` and never measured there. In the probe the index is an *address*: `m_values[value_idx]` is a second miss behind the first, so prefetching the block takes a level off a two-level chain. In these walks the index **ends** the chain, feeding a compare (and in `repoint_value` a final store). It is still a dependent load, often on a line the prefetch would have started, but the second level is gone, and with it about what the two instructions cost. Instructions, one header per binary, candidate / baseline:

| workload | gcc | clang |
|---|---|---|
| `uint64_t` random insert erase | 0.9966 | 0.9970 |
| `uint64_t` churn at a fixed size | 0.9967 | 0.9942 |
| `std::string` random insert erase | 0.9988 | 0.9989 |
| `std::string` churn | 0.9979 | 0.9988 |
| `big_value` random insert erase | 1.0000 | 0.9946 |
| `big_value` churn | 0.9971 | 0.9946 |
| every build, find and iterate workload | 1.0000 | 1.0000 |

Only erase-heavy workloads move, by 0.1-0.6%: all six under clang and five of six under gcc (corrected 2026-10-02: said "Exactly the erase-heavy workloads, on both compilers"). Score **1.0039** under gcc (5 of 5 rounds), **0.9991** under clang with rounds either side of 1.00 (`solo.sh` prints baseline/candidate, above 1.00 is candidate faster, the opposite of `run.sh`). **this ships on the instruction count.**

A cost the score cannot see: in a TU that calls `erase` itself, clang stops inlining `do_erase` into its four entry points. A six function TU over `map<uint64_t, uint64_t>` goes from one out-of-line `do_erase` to three, 7880 to 8211 bytes of text, +4.2%; such a caller pays the call boundary's 15 instructions against the 0.1-0.6% saved (corrected 2026-10-02: said "0.3-0.6%"). gcc does not move (28162 to 28065 bytes).

**The scratch benchmark said the opposite and was wrong.** A one-map TU erasing every key read 114.36 instructions per erase with the prefetches and **126.82** without under clang: two removed instructions moved an inlining decision. That is #254's lesson from the other side (see [An unbounded probe that hangs](#an-unbounded-probe-that-hangs----and-the-10-speedup-that-came-with-it-is-an-inlining-artifact)). The `CLAUDE.md` rule means two binaries of the actual score, not a scratch TU.

The probe's own prefetches stay: removing either was measured in 2026-09 as a clang win and a bigger gcc loss, a different function with a dependent load behind it (see [The SSE probe audited](#the-sse-probe-audited-and-its-one-real-redundancy-is-compiler-dependent)).

### A slot back-pointer, re-tested across the cache boundary: the win is real, and it is cancelled by the inserts that put the elements there

*2026-09-12 · #266 · rejected · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/back_pointer.sh`, `scripts/ab/solo.sh`, `scripts/ab/run.sh`, `scripts/ab/perwl.sh`*

String erase by key is 9-11% cheaper at every size from 50000 to 4000000, and the inserts pay it back. `finish_erase` hashes the moved element's key a second time to find its slot: free for an integer key, about 50 ns for a string (see [Why a dense erase is not slow, and where it is](#why-a-dense-erase-is-not-slow-and-where-it-is)), and the earlier rejection was on cache-resident string tables, where that costs least. #266 asked for the size axis. The rule, fixed before looking: a win only for large strings, paid in memory everywhere and in `build64`, is a knob and not a default; #248's principle rules out guessing at caller data.

**The win is exactly where it was predicted, and it is the only one.** One packed slot per value in a vector parallel to `m_values`. Back-pointer over base, ns per operation, one variant per binary, three alternating rounds, medians; `find` is the control the change cannot reach. `std::string`:

| n | erase by key | churn (erase+insert) | erase by iterator | build | find (control) |
|---|---|---|---|---|---|
| 50000 | **0.915** | 0.949 | 0.938 | 1.031 | 1.001 |
| 200000 | **0.908** | 0.936 | 1.001 | 1.034 | 1.034 |
| 800000 | **0.893** | 1.033 | 0.971 | 1.060 | 1.006 |
| 2000000 | **0.900** | 1.006 | 0.975 | 1.049 | 1.000 |
| 4000000 | **0.901** | 0.993 | 0.956 | 1.011 | 0.999 |

The absolute saving grows with the table: 5.4 ns at fifty thousand, **40.8 ns at four million**, where nothing the second hash touches is cached, the largest single cost named in the churn workloads.

**A 64 byte mapped value is the shape where the memory objection is weakest, and it loses worse.** Four bytes is 5% of a `map<uint64_t, big_value>` entry and 19% of a `map<uint64_t, size_t>` one. Erase by key wins the same (0.994, 0.975, 0.930, 0.909, **0.898** over the five sizes), but **churn loses at every one of them**: 1.058, 1.122, 1.069, 1.066, 1.070, against a control at 1.010-1.060. The extra store per insert competes for the cache the value wants.

**It does not survive the insert.** Every insert pays a store and the value vector a parallel growth: `build` 1.1-6.0% slower for strings, 6.9-23.3% for integer keys. Instructions per operation, which layout cannot move: **+12.8% `build64`**, +10.4% `buildbig`, +2.6% `buildstr`, against -3.8% `churn64`, -4.5% `churnstr`, -4.9% `iestr`. So a table held at a fixed size by erasing one and inserting one (`churn`, the shape of a real cache) reads **0.936 to 1.033 for strings**, a wash in a control band of 1.000-1.034, and a loss for both other value shapes. Erase win and insert tax are the same size.

**`erase(iterator)` gains nothing, which is the half the issue expected most from.** 0.970 to 1.008 for strings, control 1.003-1.016. The back-pointer hands `finish_erase` its slot, but `erase(iterator)` still hashes for `erase_group_slot`, which needs the counter class and the *home group* to walk the counters back from. The slot's fingerprint gives the class; only a stored distance gives home. So the back-pointer removes one hash from the backfill and none from the locate; the no-hash erase needs the nibbles, at 0.959 on the score (see [Three on the value vector](#three-on-the-value-vector-the-one-part-nothing-had-touched-all-keeping-it-dense)).

**Correction (2026-10-02):** the range above matches neither column of the final table. Erase by iterator reads 0.938 to 1.001 for strings, control 0.999 to 1.034: a gain of up to 6%, at four of the five sizes, against 9-11% for erase by key. So `erase(iterator)` gains a few percent at most, not nothing. The verdict on the nibbles does not change: they were rejected on their own score, 0.959.

**The score, three ways, and they agree.** One header per binary, five alternating rounds: **0.9870 under clang** (0.9862, 0.9832, 0.9904, 0.9904, 0.9846; no round disagrees), **0.9729 under gcc**. Paired in one process: 0.988 over the fifteen, builds 0.961, churn 0.983, insert-and-erase 1.020; per workload `build64` 0.930, `buildstr` 0.977, `buildbig` 0.976, `churnbig` 0.930 against `churnstr` 1.031 and `iestr` 1.041. The 2026-09-05 rejection ([Three on the value vector](#three-on-the-value-vector-the-one-part-nothing-had-touched-all-keeping-it-dense)) recorded the same shape at the same sizes from a different harness: `churnstr` 1.045, `iestr` 1.024, `build64` 0.953 (corrected 2026-10-02: said "The 2025 rejection").

**Memory, quoted next to the speed as the issue required.** Live bytes per entry, allocations minus frees through a replaced global `operator new` (the experiment's vector uses the default allocator, which an allocator-side counter cannot see):

| | base | back-pointer | |
|---|---|---|---|
| `map<uint64_t, size_t>` | 28.2 | 33.4 | **+18.6%** |
| `map<std::string, size_t>` | 107.9 | 113.1 | +4.9% |
| `map<uint64_t, big_value>` | 101.6 | 106.8 | +5.2% |

The 2026-09-05 entry's "19% more memory for an 8 byte value" reproduces (+18.6%) (corrected 2026-10-02: said "The 2025 note's ... reproduces exactly").

**So it is rejected, and this time the size axis is covered.** Not a default: it loses the score on both compilers and costs a fifth of an integer map's memory. Not a knob: it wins only when *erasing without inserting*, and no scored workload separates the two. A caller who drains a table paid the build tax to fill it; one who holds it at a fixed size pays the insert tax on every erase saved. For a caller whose fill is off the measured path, #248's rule answers: the map cannot see which caller it has and ships no knobs that guess. The issue said a negative re-test means the nibble variant cannot rescue it, and it cannot: the nibbles pay for `erase(iterator)`, measured here at nothing.

**Two things found on the way.**

*The first version of the harness diluted the very ratio the verdict rests on.* Its `churn` indexed a prepared array of 3n keys for both the erase and the insert key: two random reads into 384 MB at four million string entries inside the timed loop, compressing every ratio toward 1.00, which is exactly how "a wash on churn" gets manufactured. Rebuilt in `workloads::churn`'s shape (a live set of key *values*, the key made from one), the wash held: 0.957-1.022 before, 0.936-1.033 after, control 1.000-1.034.

*The harness measured its own inlining until it was split.* Both variants in one TU: the integer erase-by-iterator cell reads 1.77 at 200000 entries. One variant per binary: 1.27, and the `find` control moves from 1.02 to 0.95, the layout band the one-binary run hid. Two headers in one unit share an inlining budget (`scripts/ab/window.cpp` says so; `CLAUDE.md` makes it a rule), and this change alters `finish_erase`'s size. All numbers above are one variant per binary. The harnesses agree on everything large (`erasekey` at 2M: 0.896 and 0.894) and disagree on everything small. **Open (2026-10-02):** the final string table reads 0.900 at 2M; which key type and which harness versions the 0.896 and 0.894 come from is not recorded.

*The back-pointer trades a loud failure for a silent one, which is a third reason against it.* `mutated_key_found_by_the_backfill_terminates` requires a key changed through an iterator to be caught when the backfill walks to the moved element's slot. Without the walk nothing throws: four test cases fail, the only four of 835, under clang, gcc and ASan+UBSan. The table is as corrupt as before (the element sits in a slot whose fingerprint and home belong to its old key, so `find(new_key)` will not reach it and `find(old_key)` will not match it) and now says nothing. A shipped back-pointer would owe a replacement check (verify the fingerprint at the back-pointed slot before writing through it), which would eat part of the win.

The experiment: `scripts/ab/back_pointer.patch` (ten maintenance sites, listed at its top), with `scripts/ab/back_pointer.cpp` and `.sh` to re-take any number here.

### The #260/#262 sweep run over find and churn, and it comes back empty

*2026-09-12 · #268 · rejected · Ryzen 9 7950X, gcc 16 and clang 22, `scripts/ab/solo.sh`, `perf record` and `perf stat`*

Three candidates, none worth anything. #260 and #262 came from reading a hot path's disassembly for instructions the caller never wanted, both paid by gcc alone (see [`finish_erase` packed a slot that the store took ...](#finish_erase-packed-a-slot-that-the-store-took-straight-back-apart) and [Nothing holds a flat slot number any more](#nothing-holds-a-flat-slot-number-any-more-and-gcc-was-the-one-paying-for-it)). #268 swept the rest: `find` at 50% hits for both key types, the churn insert path, and the two placement walks.

**Two were already closed by this file.** The SSE probe was audited on 2026-09-07 (`movdqu`, `pcmpeqb` against a broadcast both compilers hoist, `pmovmskb`, `test`, `blsr` and `tzcnt` for the lane loop) with nothing left. Its one redundancy, `prefetch_index` naming `p + 64` and `p + 87`, which share a cache line for six of the eight offsets an 88 byte stride can have, was kept: removing it is a clang win and a 12% gcc loss at four million entries (see [The SSE probe audited](#the-sse-probe-audited-and-its-one-real-redundancy-is-compiler-dependent)).

**The third is real and measures nothing.** `move_home(found_in, from_lane, mh)` re-derived `group_idx_from_hash(mh)` (a load of `m_shifts` and a shift) on its early exit and `fingerprint_words[mh & 0xFF] & 7` on its walk, while all three writing callers (`do_try_emplace`, `do_insert_hashed`, `do_merge`'s walk) held both pieces: the #260 shape. gcc emits 26 full out-of-line copies of `move_home` in the benchmark binary, so the early exit costs a call and nine instructions. Passing `home_idx` and `counter` makes the condition a compare of two arguments, `-fpartial-inlining` splits it, and all 32 copies become `.part.0` clones reached only when the element is *not* at home. **Open (2026-10-02):** 26 copies, then 32: whether the candidate emits more copies than the baseline or one number is a typo is not re-checked. clang inlines it everywhere and gains nothing. Instructions under gcc, candidate over baseline, two bit-identical runs:

| workload | gcc |
|---|---|
| `uint64_t` churn at a fixed size | 1.0075 |
| `std::string` random insert erase | 1.0048 |
| `uint64_t` `big_value` build from empty | 1.0048 |
| `uint64_t` `big_value` churn | 0.9846 |
| `std::string` 50% probability to find | 0.9962 |
| the other ten | 1.0000 +- 0.0002 |

**The workloads that moved are the ones that cannot call it.** Churn's `map[next]` takes a key it has never held, build is all misses, find is read-only: `move_home` never runs in the three largest movements. gcc's inliner moved: one `churn` instantiation went from 7065 to 10384 bytes. Score 0.9957 over five rounds under gcc, inside the layout band. Reverted.

**Why the ceiling was 0.2% and could have been read off in one command.** `perf record` on the baseline score binary shows `move_home` nowhere above 0.2%: the hot workload functions inline `do_try_emplace` and `move_home` with it, and the out-of-line copies serve cold instantiations. An out-of-line symbol is not evidence the hot call site pays for it; counting symbols is not counting calls. Profile first, then read the disassembly of what the profile named.

**And the one candidate that looked best came from a scratch translation unit and was an artifact.** `place_group`'s loop stores `group.m_overflows[counter]`, a `std::uint8_t` store that may alias anything, then calls `next_group`, which reads `m_group_mask` through `this`: the hazard `uncount` was refactored for and `fill_buckets_from_values` is hand-written to avoid. In a one-map TU gcc emits `and 0x38(%r10),%r9d`, a reload every step. In the benchmark binary, inlined into `workloads::build`, it reads `and %r13d,%r10d`, mask and group pointer hoisted across the store. **A scratch TU can invent a redundancy as well as hide one**, the other half of #254's rule.

## The rehash and its pipeline

This section covers `fill_buckets_from_values`, the loop that places every value into a new index on growth or `rehash()`. Two changes shipped: the loop walks the value container with an iterator, and it hashes sixteen elements ahead and prefetches the target group. A radix-partitioned rehash was measured twice, once in `unordered_dense` and once ported into boost, and lost end to end both times because its scratch memory faults in on every growth.

**Where it stands** (as of 2026-09-15)

- The rehash walks the value container with an iterator. Under clang growth went from 10.43 to 2.74 ns per insert and the paired score geomean read 1.076; gcc 1.006. See [Indexing the value container in the rehash cost ...](#indexing-the-value-container-in-the-rehash-cost-clang-a-memory-latency-per-element).
- The pipelined loop is kept: score 1.002 clang, 1.003 gcc; isolated `rehash(0)` at 2M goes 6.71 to 3.52 ns per element for u64 and 24.8 to 9.4 for strings. The radix partition is dropped: it ties end to end (u64 8M 53.8 against 53.4 ns per element). See [The rehash loop pipelined](#the-rehash-loop-pipelined-and-the-partitioned-rehash-it-was-measured-against).
- Below 200000 entries the pipeline is kept with no gate. It costs a gcc integer build 1.7 to 4.2% from 43 KB to 486 KB of index and costs clang nothing (worst cell 0.979). See [The rehash pipeline below 200000 entries, where it had never been measured](#the-rehash-pipeline-below-200000-entries-where-it-had-never-been-measured-it-costs-a-gcc-integer-build-up-to-42-in-a-band-and-it-stays).
- No other map pipelines its rehash, and the pipeline does not help boost: 68-75% of a boost build is growth, and that cost is moving values, not a hideable load. A radix partition in boost's rehash is 1.7-2x slower. See [The pipelined rehash does not transfer to `boost::unordered_flat_map`](#the-pipelined-rehash-does-not-transfer-to-boostunordered_flat_map-and-no-other-map-has-one).

### Indexing the value container in the rehash cost clang a memory latency per element

*2026-09-05 · no issue · kept · found by comparing the two compilers' absolute times on the branch, not their ratios to main*

Walking the value container with an iterator instead of `m_values[value_idx]` took clang's growth from **10.43 ns per insert to 2.74, the whole build 16.72 to 8.96**. Clang's build went from 1.7x slower than gcc's to faster than it.

`build64` was the last large branch-specific compiler gap: 3.36 ms under clang against 1.97 under gcc. On main the gcc/clang ratio for the same workload is 1.04. Splitting a 200000 element build into inserts into a reserved table plus the growth on top showed where it lived. Clang was **1.44x faster** on the reserved inserts (6.30 ns against 9.08) and **4.9x slower** on growth (10.43 ns per insert against 2.15). So it was never the insert path. It was `fill_buckets_from_values`.

Mechanism: placing an entry stores a fingerprint. A `std::uint8_t` store may alias any object, including the container's own data pointer. So after every placement, the indexed read has to load that pointer back from the container before it can form the address of the next key. The random group access that follows waits for that load. The chain costs one memory latency per element, and the placements, which are independent, run one at a time. An iterator keeps the address in a register.

Paired on the score: clang `build64` **1.80**, `buildbig` **1.61**, geomean **1.076**, everything else within noise. gcc 1.03 and 1.03, geomean 1.006, because gcc disambiguated on its own.

What did *not* work, all measured:
- hoisting `m_buckets.index()` out of `place_group`: the same store-to-load shape, but that pointer was already in a register;
- `__restrict__` on the group and index pointers: it says nothing about the container's pointer, which is the one being reloaded;
- every combination of forcing `place_group` and `fill_buckets_from_values` inline or out of line: a 3x2 matrix, all within 2%.

The lesson is the one the old rehash section already stated for a different loop: what matters is what sits on the path to an address. The new part is that a byte-sized store is enough to put a container's own bookkeeping there.

**The rest of the header was then audited for the same shape, and it is the only instance.** Every site that reads a container by index after a store:
- `replace()` has the identical loop and is *correct* as written. That loop moves elements and pops the back, so the pointer and the size really change and the reload is required.
- `erase_if` recomputes `begin()` around an `erase()` that moves elements, likewise correct.
- `probe` reads `m_values` to compare keys but stores nothing, so nothing serialises inside one probe.
- `place_group` then still asked for `m_buckets.index()` after writing a fingerprint (gone since the merged block, 2026-09-06). Re-measured once the big effect was gone, it is worth nothing either way: reserved inserts 6.92 ns against 5.84, growth 2.17 against 2.38, mixed and within noise.

The rehash was unique because the reload was the *only* work between iterations, so it landed directly on the address path of the next random group access. Everywhere else, enough independent work hides it.

**Later (2026-09-11):** the audit was wrong about `replace()`: holding cursors and asking `size()` after a pop is 0.68-0.98 of the indexed loop in all 24 cells, see [`replace()`'s two loops walk with cursors too](#replaces-two-loops-walk-with-cursors-too-and-one-cursor-too-many-is-slower-than-none). `merge()`'s walk had the same shape when it was written, see [`merge()`](#merge-and-why-it-is-not-the-loop-the-caller-would-write).

Comparing the two compilers found this, and the method has a blind spot: it only sees a loop where one compiler disambiguates and the other does not. A loop both compilers serialise looks normal. After the fix no workload shows a branch-specific compiler gap: `build64` is 1.82 ms under clang against 1.88 under gcc, where it was 3.36 against 1.97. The only large remaining difference, integer iteration at 1.65x slower under gcc, is shared with main and so is not about this index.

### The rehash loop pipelined, and the partitioned rehash it was measured against

*2026-09-06 · no issue · kept · `rehash(0)` on a built map, isolated; then paired, three variants in one binary, build from empty*

The pipelined loop is kept and the radix partition is dropped. Isolated, the pipeline wins below cache and for strings everywhere, and the partition wins integer rehashes from 4M up. End to end in a build the two tie.

The idea came from asking what else the group structure is good for: placement is shift-free, so a rehash can place in any order, which is what a database-style radix partition needs.

*Pipelined* hashes sixteen elements ahead of the one it places and prefetches the group each will land in. The group pointer, mask and shift are held in locals, because a fingerprint store may alias any of them through `this`, the same shape as in [Indexing the value container in the rehash cost ...](#indexing-the-value-container-in-the-rehash-cost-clang-a-memory-latency-per-element). *Partitioned* splits by the top bits of the group: one histogram pass, one scatter of an 8 byte packed entry per element, then placement partition by partition.

`rehash(0)` on a built map, ns per element:

| entries | index | u64 before | pipelined | partitioned | string before | pipelined | partitioned |
|---|---|---|---|---|---|---|---|
| 200K | 1.4 MB | 2.05 | **1.63** | 3.22 | 8.7 | 8.4 | 8.5 |
| 1M | 11 MB | 2.27 | **1.78** | 5.53 | 11.9 | 8.4 | 8.6 |
| 2M | 22 MB | 6.71 | **3.52** | 5.31 | 24.8 | **9.4** | 10.0 |
| 4M | 44 MB | 10.29 | 7.63 | **5.34** | 30.6 | 12.4 | 12.1 |
| 16M | 176 MB | 12.58 | 12.46 | **8.65** | | | |

- Below cache the pipelining alone is 1.26x: the hash chain and the random placement no longer wait on each other. CLAUDE.md's old note that prefetching ahead in the rehash was worthless was measured at 200000 entries, in cache, where it has to be.
- Above cache it does everything for strings, whose hash has work to hide a miss behind (2.5x at 2M and 4M).
- It does nothing for integers at 176 MB, because that loop is bound by the TLB, not by latency: **1.15 dTLB misses per placement** on 4 KB pages, and a prefetch cannot hide a page walk. Partitioning cuts that to 0.24, nearly halves the loop at 4M (10.29 to 5.34) and takes a third off it at 16M (12.58 to 8.65) (corrected 2026-10-02: said "halves the loop from 4M up").

**Kept the pipelined loop, dropped the partition.** Paired, three variants in one binary, a build from empty: pipelined and partitioned are indistinguishable end to end (u64 8M 53.8 against 53.4 ns per element, strings 4M 128.5 against 128.8). Both are 3-18% ahead of the plain loop.

Timing every rehash inside a build shows why the isolated 2x disappears. The scatter costs 3.6 ns there instead of 1.4, because the scratch is fresh memory every time and faulting it in costs about a microsecond a page. This is the `tame_allocator()` lesson, paid by the map itself. And a rehash is a minority of a large build anyway: 15 of 64 ns per element at 16M; the inserts are the rest.

So the partition is worth 0-7% of an integer build above 32 MB. Its costs: 8 bytes of scratch per element at the growth peak, an allocation inside `rehash()` that changes what an allocator sees and what an exception path has to undo, and a hundred lines. The pipelined loop is thirty lines and needs no memory. The score is exactly neutral on it (1.002 clang, 1.003 gcc, `buildbig` 1.04 and 1.07), which is what a 200000-entry suite should say about a loop whose gains are above cache. Unit suite green.

What did not matter: the partition size, 16 KB to 256 KB (place phase 3.4 ns throughout, so it was never L2 misses); the prefetch distance, 8 to 32 (16 best by a hair). What an integer rehash above cache would actually need is huge pages. Those were measured at 22% on lookups (see [Two optimizations the charts point at](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet), paragraph [Huge pages are worth 22% of a large ...](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet)), and the map cannot ask for them.

**Later (2026-09-15):** the pipeline was measured below 200000 entries for the first time; an integer key loses 3 to 4.7% of the loop between 86 and 243 KB of index, and the pipeline stays with no gate, see [The rehash pipeline below 200000 entries, where it had never been measured](#the-rehash-pipeline-below-200000-entries-where-it-had-never-been-measured-it-costs-a-gcc-integer-build-up-to-42-in-a-band-and-it-stays).

### The pipelined rehash does not transfer to `boost::unordered_flat_map`, and no other map has one

*2026-09-08 · no issue · rejected · asked as "does any other map use a pipelined rehash" and "would it transfer to boost"; copy of boost's `core.hpp`, scratch harnesses `/tmp/brh.cpp` against `/tmp/boostpf`, `/tmp/rhperf.cpp`, `/tmp/bsplit2.cpp`*

No other map pipelines its rehash, and porting the ring into boost is a wash to a loss under clang. Under gcc it gains 1.5x at 200K integers only, and that is a codegen fix. Boost's rehash is not waiting on a load: it spends its time moving values.

Every rehash loop in the field, read:
- folly's `prefetchBeforeRehash` prefetches the *source* values of the chunk about to be hashed and places synchronously;
- abseil's `GrowToNextCapacity` is a different idea: two passes, the elements that would probe encoded as `(h2, source_offset, h1)` into a stack buffer and placed second, so nothing is hashed twice and most elements never probe;
- boost, indivi, emhash8, emilib, Verstable and ihtab hash and place one element at a time with no prefetch at all.

So the hash-sixteen-ahead loop is `unordered_dense`'s alone.

The port: `unchecked_rehash` with a sixteen-entry ring of element pointer and hash, prefetching the destination group and, in the second variant, all four cache lines of the group's fifteen slots. Isolated `rehash()` to double the bucket count and back, ns per element:

| | u64 200K | u64 1M | str 200K | str 1M |
|---|---|---|---|---|
| clang, boost as shipped | 6.5 | 10.6 | 19.0-19.5 | 77.4 |
| clang, pipelined, four slot lines | 7.0-7.2 | 11.1-11.3 | 17.9-18.7 | 80-82 |
| gcc, as shipped | **10.2-10.5** | 10.4-11.5 | 17.1-17.8 | 71-73 |
| gcc, pipelined, four slot lines | **7.0** | 10.2-10.5 | 16.7-17.2 | 73-77 |

gcc's straight loop is 1.5x slower than clang's on the same source, and the restructured loop takes it to clang's floor. That is one compiler serialising something the other does not: the same shape as `unordered_dense`'s own `fill_buckets_from_values` story (see [Indexing the value container in the rehash cost ...](#indexing-the-value-container-in-the-rehash-cost-clang-a-memory-latency-per-element)) with the compilers swapped. Prefetching only the first slot line, the first attempt, was a loss everywhere. Boost's fifteen 16-byte slots span four lines and fill from lane 0, so late placements land on lines never asked for.

**Why, measured rather than argued** (`/tmp/rhperf.cpp`, nothing but rehashes, so `perf stat` counts the loop). Per element rehashed at a million `uint64_t` entries:

| | instructions | cycles | L1 load misses | dTLB load misses |
|---|---|---|---|---|
| boost | **97.5** | **110.9** | 3.81 | 0.051 |
| `unordered_dense` | **51.9** | **46.7** | 3.73 | 0.628 |

So it is not the TLB, and not a load a prefetch could have hidden. The first draft of this entry asserted both and had measured neither. A flat map's rehash moves the `value_type` into a hash-scattered slot, which is where the extra instructions go. The random writes that follow are write-allocate misses, which an `__builtin_prefetch` of the destination does not cover. A lookahead hides a load's latency behind a hash chain, and this loop is not waiting on a load.

**What that says about boost's build, which is its weakest column** (`/tmp/bsplit2.cpp`, build from empty against a reserved build, ns per element):

| | insert path, 200K u64 | insert path, 1M u64 | growth per element, 200K | growth per element, 1M |
|---|---|---|---|---|
| boost | 3.53 | 5.97 | 7.7 | 17.9 |
| `unordered_dense` | 7.48 | 8.62 | 0.94 | 3.43 |
| abseil | | | 7.11 | 20.60 |
| boost, 64 byte value | | | 18.2 | 59.9 |
| `unordered_dense`, 64 byte value | | | 4.3 | 23.6 |

Boost's *insert path is faster than this map's*, but **68-75% of a boost build is growth against 11-28% here**. Abseil, whose two-pass encoder is the cleverest rehash in the field, is 8% better than boost at 200K and *worse* at 1M. So the encoder is not the fix either. The cost belongs to the family: a flat map moves every value on every doubling, a dense one moves four byte indices and leaves the values in place.

**And the radix partition does not rescue it either, which was the one idea left** (2026-09-08, asked as "give 2 a try and measure it"). Growth is 68-75% of a boost build against 11-28% here, so the partition that measured end-to-end neutral in `unordered_dense` (see [The rehash loop pipelined](#the-rehash-loop-pipelined-and-the-partitioned-rehash-it-was-measured-against)) has five times as much to gain in boost. Ported into `unchecked_rehash`: hash every element and histogram the partition of the new group array it lands in, scatter `(element*, hash)` into partition order, then place. At 16, 64 and 256 partitions it is **1.7-2x slower at every size and on both compilers**, because the scratch is 32 bytes per element, faulted fresh on every growth. That is the same failure `unordered_dense`'s own version had.

The scratch cost can be separated from the idea. Keeping the scratch in a `static thread_local` across rehashes turns the isolated 4M integer rehash into a **19% win** (21.3 to 17.2 ns per element, three rounds, both compilers). But:
- the win is a bump, not a trend: at 8M the same code is **25% slower** (21.2 to 26.5), because the scratch has grown to 256 MB and its own scatter pass becomes the cost;
- it does not survive end to end. A build from empty at 4M goes **43.0 to 47.7 ns per element under clang and 42.1 to 44.8 under gcc**, slower, because a build doubles twenty-odd times and the scratch grows with it, so the pages the isolated harness faults once are faulted again at every doubling;
- strings lose throughout (80.8 to 85.1 at 4M): the scatter of a 40 byte `value_type` costs more than the locality buys.

So the answer to "what could help boost's build" is: not a better rehash loop. Its insert path is already 1.4x to 2.1x as fast as `unordered_dense`'s (corrected 2026-10-02: said "twice as fast"). What it pays for is moving every value on every doubling, which is what the flat layout *is*. The levers left are a growth factor below 2x (folly's 1.406, untested here) and not being flat.

**Later (2026-09-08):** folly's 1.406 turned out to apply only to an explicit `reserve(n)`; folly doubles on growth by insertion, so there is no shipped sub-2x design to copy, see [A growth factor below 2](#a-growth-factor-below-2-and-the-premise-that-suggested-it-was-wrong).

### The rehash pipeline below 200000 entries, where it had never been measured: it costs a gcc integer build up to 4.2% in a band, and it stays

*2026-09-15 · no issue, asked for directly after 5.0.1 · kept · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/rehash_size.sh` with `scripts/ab/rehash_plain.patch`, one variant per binary*

The pipeline stays, with no gate. For `rehash(0)` alone, a string key wants it at nearly every size (up to 1.53x). An integer key wants it up to 61 KB of index and from 344 KB on, and loses 3 to 4.7% between. In a build, clang loses nothing anywhere, and gcc loses 1.7 to 4.2% on integers from 43 KB to 486 KB of index.

[The rehash loop pipelined](#the-rehash-loop-pipelined-and-the-partitioned-rehash-it-was-measured-against) measured the pipelined loop from 200000 entries up. Below that there was no measurement. `fill_buckets_from_values` has no gate, while `replace()` has one at 256 KB of index. The question was whether the rehash is the shape `replace()` turned out to be: a pipeline shipped above its crossover and never looked at below it.

Setup:
- The control is the same loop with the ring and the prefetch removed. Everything else stays: the iterator walk, the hoisted group pointer, mask and shift, and the placement itself.
- One variant per binary, because a ring changes the function's size and two headers in one translation unit share an inlining budget.
- The subject is `rehash(0)` on a map already at the bucket count `rehash(0)` asks for. The header then skips the allocation and runs `clear_buckets()` plus the loop. The memset is in both variants, so it dilutes the ratio rather than steering it.
- Nineteen sizes, four per octave, because a rehash places every element and its cost per element follows the load factor, which sweeps half to maximum between doublings.
- Rounds alternate the two binaries. Each cell is the median of 49 rounds for the loop and 25 for the build.
- Every ratio is plain/pipelined, so above 1 the pipeline wins.

#### `rehash(0)`, the loop on its own

| entries | index | u64 clang | u64 gcc | str clang | str gcc |
|---|---|---|---|---|---|
| 1000 | 5 KB | 1.087 | 1.064 | 1.037 | 0.946 |
| 1414 | 8 KB | 1.098 | 1.020 | 1.040 | 0.945 |
| 2000 | 11 KB | 1.027 | 1.052 | 1.071 | 0.982 |
| 2828 | 15 KB | 1.073 | 1.037 | 1.081 | 0.990 |
| 4000 | 21 KB | 1.036 | 1.041 | 1.105 | 1.011 |
| 5657 | 30 KB | 1.016 | 1.006 | 1.118 | 1.019 |
| 8000 | 43 KB | 1.007 | 0.975 | 1.137 | 1.031 |
| 11314 | 61 KB | 1.000 | 0.995 | 1.131 | 1.030 |
| 16000 | 86 KB | 0.966 | 0.966 | 1.148 | 1.044 |
| 22627 | 122 KB | **0.956** | **0.961** | 1.154 | 1.052 |
| 32000 | 172 KB | **0.953** | **0.956** | 1.279 | 1.181 |
| 45255 | 243 KB | 0.953 | 0.971 | 1.285 | 1.186 |
| 64000 | 344 KB | 1.079 | 1.082 | 1.439 | 1.334 |
| 90510 | 486 KB | 1.072 | 1.073 | 1.432 | 1.331 |
| 128000 | 688 KB | 1.207 | 1.222 | 1.534 | 1.414 |
| 181019 | 972 KB | 1.197 | 1.206 | 1.518 | 1.394 |
| 256000 | 1.3 MB | 1.271 | 1.273 | 1.527 | 1.413 |
| 362039 | 1.9 MB | 1.253 | 1.264 | 1.463 | 1.362 |
| 512000 | 2.7 MB | 1.274 | 1.284 | 1.497 | 1.414 |

A string key wants the pipeline at nearly every size. The two gcc cells at 1000 and 1414 entries (0.946 and 0.945) are the only ones below 0.98. An integer key wants it up to 61 KB of index and from 344 KB on, and **does not want it between**, where it costs 3 to 4.7% on both compilers. Two runs of the same sweep agreed on every cell of that dip to within 1.5%.

#### Build from empty, which is where the loop actually runs

| entries | index | u64 clang | u64 gcc | str clang | str gcc |
|---|---|---|---|---|---|
| 1000 | 5 KB | 1.063 | 1.048 | 0.999 | 0.998 |
| 1414 | 8 KB | 1.055 | 1.028 | 0.985 | 0.985 |
| 2000 | 11 KB | 1.049 | 1.031 | 1.003 | 0.996 |
| 2828 | 15 KB | 1.056 | 1.015 | 1.002 | 0.996 |
| 4000 | 21 KB | 1.043 | 0.992 | 1.014 | 1.002 |
| 5657 | 30 KB | 1.087 | 0.994 | 1.012 | 1.003 |
| 8000 | 43 KB | 1.025 | 0.979 | 1.024 | 1.009 |
| 11314 | 61 KB | 1.023 | 0.983 | 1.016 | 1.008 |
| 16000 | 86 KB | 1.023 | **0.965** | 1.027 | 1.006 |
| 22627 | 122 KB | 1.023 | 0.974 | 1.016 | 1.006 |
| 32000 | 172 KB | 0.979 | **0.958** | 1.032 | 1.017 |
| 45255 | 243 KB | 1.004 | 0.963 | 1.021 | 1.013 |
| 64000 | 344 KB | 1.013 | 0.977 | 1.054 | 1.044 |
| 90510 | 486 KB | 1.019 | 0.980 | 1.037 | 1.030 |
| 128000 | 688 KB | 1.030 | 1.004 | 1.070 | 1.061 |
| 181019 | 972 KB | 1.035 | 1.002 | 1.050 | 1.044 |
| 256000 | 1.3 MB | 1.056 | 1.024 | 1.083 | 1.075 |
| 362039 | 1.9 MB | 1.051 | 1.018 | 1.077 | 1.072 |
| 512000 | 2.7 MB | 1.070 | 1.038 | 1.144 | 1.133 |

Under clang the pipeline costs a build nothing anywhere: the worst cell over both key types is 0.979, the best 1.144. Under gcc it costs an integer build 1.7 to 4.2% from 43 KB to 486 KB of index, the same band the loop loses in, and pays it back from 688 KB on. A string build is a wash under both compilers below 344 KB and a win above.

#### Eleven instructions per element, and where they buy nothing

`perf stat` with the repetition count fixed at compile time, so both variants do the same work. u64 keys, clang, the build phase included in both and identical in both:

| entries | index | pipelined instr/el | plain instr/el | pipelined cycles/el | plain cycles/el |
|---|---|---|---|---|---|
| 4000 | 21 KB | 35.9 | 24.8 | 9.87 | 10.75 |
| 22627 | 122 KB | 37.7 | 26.4 | 10.73 | 9.91 |
| 512000 | 2.7 MB | 81.0 | 61.8 | 36.64 | 39.10 |

The ring costs 11 instructions per element at every size, the same order as the 13 it costs everywhere else in this header. What changes is what they buy: at 21 KB they take 8% of the cycles out, at 122 KB they add 8%, at 2.7 MB they take out 6%. The loop fails to pay for its bookkeeping in exactly one place: a cheap hash over an index too big for L1 and too small to miss.

**Open (2026-10-02):** the prose says 11 instructions per element at every size, the table reads 11.1, 11.3 and 19.2 (81.0 - 61.8 at 2.7 MB); not re-measured.

**Kept, no gate.** A gate here would have to be on index bytes, like `replace()`'s, and the losing band is bounded on both sides, so one threshold cannot express it. The obvious one, "no pipeline below 688 KB of index", gives a gcc integer build back its 1.7 to 4.2% in the 43 KB to 486 KB band (corrected 2026-10-02: said "2 to 4.2%"). It also takes 1.5 to 4.8% off a gcc integer build below 15 KB, 4.3 to 8.7% off a clang integer build below 30 KB, 1 to 3% off a clang string build in the middle, and every string cell above 43 KB on both compilers. The loss is one compiler, one key type and one band. The win is the other compiler at the same sizes, both compilers above the band, and strings nearly everywhere. Separating them needs to know whether the key's hash is expensive. That is a fact about the caller's data, not about the table, and `unordered_dense` does not gate on those; the range insert's `distance()` sizing was declined for the same reason (see [`insert(first, last)` sizing the table from the range](#insertfirst-last-sizing-the-table-from-the-range-built-measured-declined)).

**The harness had to be pinned before any of this was worth reading.** The first version put both timed functions in one translation unit, and the second one changed the first. Adding `measure_build` and a nanobench include moved the pipelined side of the `rehash` mode from 1.45 to 1.62 ns per element at 22627 entries, while the plain side did not move. That turned a 4% dip into 13%. The build is deterministic: the same source and flags give a byte-identical binary, checked with `md5sum`. So that was the inliner, not code layout. It is the "inlining artifact" rule met from a new direction: not a count that moved, but a *time* that moved, in a file where nothing the clock covers had changed. Fixes: `REHASH_MODE_BUILD` gives each mode its own translation unit, and the `perf` build takes its repetition count from an `#if`, so the default translation unit stays byte-identical to the one the tables came from. The same sweep run twice agrees to 1.5%.

What this says and does not say:
- One machine, two compilers, one value type (`std::uint64_t`), and an integer key through the workloads' bijection.
- The build column is a build and nothing else. It does not say what a table that churns pays, where the rehash runs over values that are already resident.
- The `rehash(0)` column includes `clear_buckets()`'s memset in both variants. That pulls every ratio toward 1: the loop's own gains, and its own loss in the 86-243 KB dip, are both slightly larger than the column shows (corrected 2026-10-02: said "That makes the loop's own ratio slightly better than the column shows, not worse.").
- Nothing here was measured above 512000 entries. [The rehash loop pipelined](#the-rehash-loop-pipelined-and-the-partitioned-rehash-it-was-measured-against) covers 2M and 4M and says the pipeline wins there by more.

## Bulk operations: replace(), merge(), range insert, visit()

The bulk paths `replace()`, `merge()`, `insert(first, last)` and `visit()`, and the lookahead rings and cache gates inside them. A ring pays only once its index is out of cache, and where that happens depends on the rest of the loop and on the caller's duplicate rate, so each gate is fitted on its own loop and swept across sizes. Sizing a table from a range it has not seen is a guess about caller data and was declined even at 2x.

**Where it stands** (as of 2026-10-02)

- `replace()`'s ring is gated on `index_bytes > 256 KiB` (`pipeline_min_index_bytes`): geomean 0.9081 over 28 cells, worst cell 1.163. Its payoff depends on the duplicate rate (1.5x with none, 16% slower with a quarter, at 352 KiB). See [`replace()`'s gate was re-measured on a fixed harness and kept](#replaces-gate-was-re-measured-on-a-fixed-harness-and-kept-and-the-harness-is-the-finding).
- `replace()`'s loops hold iterator cursors and ask `size()` rather than hold `last` (a fourth live value cost 3.5% cycles). See [`replace()`'s two loops walk with cursors too](#replaces-two-loops-walk-with-cursors-too-and-one-cursor-too-many-is-slower-than-none).
- `merge()` compacts the source and rebuilds its index once: 1.8x to 2.4x over a caller's `try_emplace` + `erase` loop, 5 to 8% behind only for a small table at 90% overlap. Gate `merge_min_index_bytes` = 1 MiB; `reserve(size() + source.size())` declined. See [`merge()`, and why it is not the loop the caller would write](#merge-and-why-it-is-not-the-loop-the-caller-would-write).
- Range insert sizing (#248) declined at 2x: every version is a heuristic about caller data. See [`insert(first, last)` sizing the table from the range](#insertfirst-last-sizing-the-table-from-the-range-built-measured-declined).
- The range insert and bulk visit rings have no gate (#247). See [The other two pipelines do not want the cache gate](#the-other-two-pipelines-do-not-want-the-cache-gate-and-the-gates-own-footprint-model-was-wrong).
- Chunk when an element has two dependent accesses, stream when it has one: `visit()` chunks, the rehash streams. See [Chunks or a sliding ring](#chunks-or-a-sliding-ring-the-rehash-and-the-bulk-visit-want-opposite-answers-and-the-reason-generalises).
- Loading from values and index (#299): a checked owning load is 3.1-7.6x faster than building for integers and 3.9-17x for strings, 1M-64M; the slot check costs it 1.4-6.1x, so it takes `trust` like the view; a view's check is 0.85 ns per entry. See [Loading a map from its values and its index](#loading-a-map-from-its-values-and-its-index-an-owning-load-is-3-17x-faster-than-building-at-1m-64m-entries-and-a-view-costs-085-ns-per-entry-to-check-but-the-slot-check-is-most-of-an-owning-load-so-the-owning-constructor-takes-trust-too).
- `visit(first, last, f)` replaced `prefetch(key)`, which was measured against the wrong baseline; batching keys is itself 1.5x. Below the cache `visit()` loses only when every key hits (0.89x at 1000 entries); with half misses it wins 1.15x to 1.24x at every size. See [A bulk `visit()`, and the `prefetch(key)` API it replaced](#a-bulk-visit-and-the-prefetchkey-api-it-replaced----which-was-measured-against-the-wrong-baseline) and [`visit()` re-measured across the cache boundary](#visit-re-measured-across-the-cache-boundary-the-cache-resident-loss-is-a-hit-rate-effect).
- `replace()` + `extract()` dedups a `std::vector<std::string>` 10x to 27x faster than a boost set copied out; the lead erodes with duplicates. A forward compaction crosses over at 25-30% and is not applied. See [`replace()` + `extract()` as the way to unique a vector](#replace--extract-as-the-way-to-unique-a-vector-and-the-duplicate-rate-that-turns-it-over).

### `replace()`'s gate was re-measured on a fixed harness and kept, and the harness is the finding

*2026-09-11 · #257 · kept · `scripts/ab/replace_bulk.cpp`, Ryzen 9 7950X, clang 22, medians of seven*

`pipeline_min_index_bytes` stays at 256 KiB. It was fitted in #249 on `map<uint64_t, size_t>` alone, and a sweep during #256 suggested it was too low for a string key. The harness could not answer: at sixteen to a hundred thousand `uint64_t` elements the same binary read 4.40, 2.85, 2.88, 4.76, 5.19, 2.76 ns per element between repeats. **Open (2026-10-02):** the same six numbers are given in [`replace()`'s two loops walk with cursors too](#replaces-two-loops-walk-with-cursors-too-and-one-cursor-too-many-is-slower-than-none) for the indexed binary at eight thousand elements; which size they belong to is not recorded.

*The harness, first.* Two causes, both fixed. Every round built a new map, so the timed region held a fresh index allocation, whose cost depended on what the allocator had just done with the container copy. And the reported number was the mean over rounds, so one round that faulted carried it. `replace_bulk.cpp` now replaces into the **same** map every round, after one untimed round that allocates the index, and reports the **median round**. Four repeats of one cell went from 3.32 / 3.33 / 3.31 / 3.27 (max 6.14) to **3.158 / 3.141 / 3.145 / 3.147** (max 3.92). `cold` restores the old behaviour: right for "what does a caller pay end to end", wrong for "does the pipeline inside it pay".

*The answer, with numbers that hold still.* Ring over the same loop with the ring deleted:

| index | `uint64_t` 0% | `uint64_t` 25% | `string` 0% | `string` 25% | geomean |
|---|---|---|---|---|---|
| 88 KiB | 1.265 | 1.225 | 1.006 | 1.037 | 1.128 |
| 176 KiB | 0.833 | 1.194 | 1.156 | 1.033 | 1.044 |
| **352 KiB** | **0.667** | **1.163** | **1.064** | **1.002** | **0.954** |
| 704 KiB | 0.632 | 0.999 | 1.039 | 0.987 | 0.897 |
| 1408 KiB | 0.626 | 0.944 | 0.998 | 0.985 | 0.873 |
| 2816 KiB | 0.565 | 0.916 | 0.973 | 0.979 | 0.838 |
| 5632 KiB | 0.537 | 0.912 | 0.950 | 0.944 | 0.814 |

The four curves cross in four places (176, 704, 1408 and 352 KiB), and not because of the key type alone. **The ring's payoff depends on the duplicate rate**: a duplicate refills its ring slot from the element that just moved into it, which the next iteration reads, so there is no distance to prefetch over. At 352 KiB the gate is worth **1.5x** to a `uint64_t` with no duplicates and costs **16%** with a quarter of them. The map cannot know which until it has walked: the wall #248 hit from the other side.

So the constant is a chosen compromise. Geomean over all 28 cells and worst cell, per threshold:

| threshold | geomean | worst cell |
|---|---|---|
| 128 KiB | 0.9137 | 1.194 |
| **256 KiB (kept)** | **0.9081** | 1.163 |
| 512 KiB | 0.9143 | 1.039 |
| 1 MiB | 0.9286 | 1.000 |

**256 KiB is the best of the four on the mean**, and the combined crossover sits there too: 1.044 at 176 KiB, 0.954 at 352 KiB. A mebibyte never loses, but costs two points of geomean to remove a worst cell of 1.163. The issue was half right: the constant *was* fitted on `uint64_t` and the string loss is real, but it is one octave, at most 1.064, paid for by a 1.5x in the same octave.

*Considered, not built:* a walk that counts duplicates and drops the ring when it sees too many. It adapts to data in hand instead of guessing, the distinction #248 turns on, but it is still a threshold on caller data, and it would be fitted on the same sweep as the table above.

### `replace()`'s two loops walk with cursors too, and one cursor too many is slower than none

*2026-09-11 · #242 follow-up · kept · `scripts/ab/replace_bulk.cpp`, Ryzen 9 7950X, clang 22, medians of seven, paired within each run*

`do_replace_pipelined` and `replace()`'s closing plain loop now hold iterator cursors instead of indexing. Cursor over indexed is below 1.0 in all 24 cells. `merge()`'s walk had read 0.89-0.95 for the same change, so every bulk loop was checked. Only these two walk `m_values` sequentially while placing (storing fingerprints). `do_visit` indexes from the probe, a random index with no cursor to hold, and never places. `fill_buckets_from_values` and `do_insert_range` were already iterator-based.

| n | dup 0% | 25% | 75% | | n | dup 0% | 25% | 75% |
|---|---|---|---|---|---|---|---|---|
| `uint64_t` 8000 | 0.684 | 0.982 | 0.948 | | `std::string` 8000 | 0.902 | 0.937 | 0.935 |
| 32000 | 0.956 | 0.933 | 0.812 | | 32000 | 0.879 | 0.952 | 0.960 |
| 200000 | 0.957 | 0.945 | 0.873 | | 200000 | 0.886 | 0.955 | 0.964 |
| 2000000 | 0.962 | 0.972 | 0.890 | | 2000000 | 0.933 | 0.976 | 0.958 |

The 0.684 is not a 1.46x. At eight thousand elements the *indexed* binary is bimodal between repeats (4.40, 2.85, 2.88, 4.76, 5.19, 2.76) and the cursor one is tight at 2.39-2.65: the round rebuilds the container and the allocator does not do the same thing twice, as `range_insert.cpp` records at the same sizes. That the cursor version is the *stable* one is the finding there.

Instructions and cycles drop in every cell measured: 80.93 to 79.92 instructions and 20.47 to 19.01 cycles per element at 32000 with no duplicates, 74.76 to 69.99 and 34.51 to 30.22 at 200000 with 75%. For a **string** key instructions stay put (487.48 to 487.21) and cycles drop (176.23 to 170.57): the reload is not extra work but a memory latency on the critical path before the next hash.

*One cursor too many is worse than none.* The first version held `read`, `look` **and** `last` and tested `last - read > pipeline_depth + 1`. It retired two fewer instructions per element than the indexed loop and took **3.5% more cycles** than the indexed loop (1.03 to 1.04 over nine repeats), and 8% more time than the two-cursor version (3.83 ns against 3.54 at two hundred thousand, no duplicates) (corrected 2026-10-02: said "took **3.5% more cycles**: 3.83 ns against 3.54 for two cursors"). `last` is a fourth live value in a loop already holding three ring arrays. It also cannot be decremented on a pop, because `std::deque::pop_back` invalidates the past-the-end iterator and a deque is a supported value container, so it needs an `m_values.end()` after every duplicate. `size()` answers the same question for nothing. Holding only `read` and indexing the lookahead lands between (3.77).

*`replace()`'s gate was not re-fitted, because its gap is older than this.* Re-fitting after the loop gets cheaper is `merge()`'s rule, so it was tried. The `uint64_t` side is unusable (same bimodality: 0.885, 0.654, 1.005, 0.688 at neighbouring sizes). The string side is clean: the ring *loses* 2 to 10% from 176 KiB to 1408 KiB of index. On `origin/main`, with indexed loops: **1.116 / 1.043 / 1.072 / 0.988 there against 1.072 / 1.051 / 1.021 / 0.992 here**, the same band. So the cursors did not cause it: `pipeline_min_index_bytes` was fitted on `uint64_t` in #249 and is too low for a key whose hash is long. Filed separately rather than changed on a sweep this noisy.

**Later (2026-09-11):** the follow-up, #257, fixed the harness's bimodality, found the string loss is one octave (at most 1.064) paid for by a 1.5x in the same octave, and kept 256 KiB, see [`replace()`'s gate was re-measured on a fixed harness and kept](#replaces-gate-was-re-measured-on-a-fixed-harness-and-kept-and-the-harness-is-the-finding).

### `merge()`, and why it is not the loop the caller would write

*2026-09-11 · #242 · kept · `scripts/ab/merge.cpp`, Ryzen 9 7950X, clang 22, medians of five, both maps rebuilt from the same input every round, only the merge inside the clock*

`merge()` walks the source once, compacts what stays, and rebuilds the source's index once: 1.8x to 2.4x faster than the caller's loop where merges usually are, 5 to 8% slower only for a small table at 90% overlap.

The standard's `merge` splices nodes, so references stay valid and nothing is copied. A dense map has no nodes: every element that moves is move-constructed into the destination's vector and has to come out of the source's. What is left to choose is how the *source* is repaired.

*The loop a caller writes* (`try_emplace` here, `erase(it)` there) repairs the source's index per element taken: a second hash of the leaving key and a third of whatever the backfill drags into the hole. It costs work per element that **goes**. It also cannot move the key out: `erase(it)` hashes the key to find its slot, so a moved-from key makes that probe not terminate (#254), and the hand-written loop copies every key. *What shipped* compacts the elements that stay down over the gaps and rebuilds the index once. It costs work per element that **stays**.

Shipped over the caller's loop, by overlap:

| source | 0% | 25% | 50% | 75% | 90% | 100% |
|---|---|---|---|---|---|---|
| `uint64_t`, 4000 | 0.453 | 0.546 | 0.551 | 0.811 | **1.077** | 0.901 |
| `uint64_t`, 64000 | 0.413 | 0.487 | 0.495 | 0.689 | 0.917 | 0.879 |
| `uint64_t`, 1000000 | 0.506 | 0.569 | 0.498 | 0.597 | 0.733 | 0.713 |
| `std::string`, 4000 | 0.459 | | 0.576 | | **1.053** | |
| `std::string`, 64000 | 0.431 | | 0.526 | | 0.878 | |
| `std::string`, 500000 | 0.556 | | 0.609 | | 0.825 | |

So **1.8x to 2.4x where a merge normally is**, with two mostly disjoint sets. The losing corner, a small table whose source is nine tenths duplicates, is the backfill's best case and the compaction's worst. It closes as the map grows (1.08 at four thousand, 0.92 at sixty-four thousand, 0.73 at a million), because above cache the loop's extra hashes cost more than a rebuild. It is not fixed: choosing a strategy needs the overlap, which is known only after the probes are paid. A probing pre-pass would have to carry every hash into the second pass (eight bytes of scratch per source element) or hash taken elements twice, making the common case pay for the rare one.

*The ring, and the gate's constant is not `replace()`'s.* The walk is the fifth `pipeline_depth` ring in the file, hashing sixteen elements ahead, gated on index bytes like `replace()`'s but **two doublings higher**. Three binaries from one header (ring deleted, always, gated), alternated, medians of five, overlaps 0 / 50 / 90%:

| source | end index | ring / no ring | gated / no ring |
|---|---|---|---|
| 1000 | 22 KiB | 1.10 / 1.19 / 1.14 | 0.97 / 0.97 / 1.00 |
| 16000 | 352 KiB | 1.07 / 1.12 / 1.09 | 0.99 / 1.01 / 1.00 |
| 32000 | 704 KiB | 1.02 / 1.06 / 1.02 | 0.99 / 1.01 / 0.99 |
| 64000 | 1408 KiB | 0.96 / 0.98 / 0.99 | 0.96 / 0.99 / 0.98 |
| 128000 | 2816 KiB | 0.92 / 0.92 / 0.97 | 0.92 / 0.92 / 0.97 |
| 2000000 | 44 MiB | 0.81 / 0.74 / 0.77 | 0.81 / 0.75 / 0.77 |

The crossover is **704 KiB to 1408 KiB of index**, not `replace()`'s 256 KB.

**Correction:** the first version of this entry claimed otherwise, on a reading taken before the walk used cursors. A crossover is where the ring's fixed 13-17 instructions per element stop paying for the misses they hide, so it moves with the cost of the *rest* of the loop: making the ringless loop 5 to 11% cheaper pushed `merge`'s crossover two doublings up. A shared constant would now cost 7 to 12% in the octave at 352 KiB. `merge_min_index_bytes` is a mebibyte, a doubling of margin on each side; with it the worst cell on the sweep is **1.009**.

The gate reads the index the destination will *end* with, `index_bytes_for(calc_shifts_for_size(size() + source.size()))`. The ring matters most for a large source going into a small map, where the current index says nothing about the walk's.

*Three cursors, not three indices.* The walk holds `read`, `write` and `look` as iterators. Indexing `src_values[i]` is the trap in [Indexing the value container in the rehash cost ...](#indexing-the-value-container-in-the-rehash-cost-clang-a-memory-latency-per-element): a placement stores a `std::uint8_t` fingerprint that may alias the container's data pointer, so every indexed read after it reloads that pointer. Against the indexed loop: **0.89 to 0.95 in cache** (0.92 / 0.89 / 0.91 at four thousand) and 0.95 to 1.00 above it, 26 of 27 cells at or below 1.00, 2.0 to 4.5 fewer instructions per element at every size. Review caught the indexed first version.

*The gate has to be two loops, not a branch.* With the gate read per element, gated-off sizes read 4.80 ns against 4.53 for the ringless loop, 5 to 10% lost below the crossover. A predicted branch is not free when a ring, a lambda and an array hang off it and must stay live. The table's gated column is `walk(std::true_type{})` against `walk(std::false_type{})`.

*`reserve(size() + source.size())` was measured and declined*, #248's answer from the other side. The bound is exact but still guesses the *overlap*. Reserved over not, at overlaps 0 / 50 / 100%: 0.837 / 1.257 / 1.604 at sixty-four thousand `uint64_t`, 0.707 / 1.066 / 1.606 at half a million, 0.818 / 1.244 / 1.618 at sixty-four thousand strings, 0.870 / 1.103 / **2.629** at half a million strings. It buys 13-30% where it is least needed, costs up to 2.6x in a case the map cannot tell apart, and holds an index and value vector up to twice the needed size. Growth doubling is right here for the same reason as in `insert(first, last)`.

*Mutation.* 113 mutants, **84% killed**, 55 of them by a test. The first run killed 62%: the whole test file was below the 256 KB gate, so `delete: walk(std::true_type{})` survived. A test whose *destination* carries the size, with a source of nought to forty elements, turns the ring on cheaply and edges the priming loop. The rest was exception repair: a moved-from `merge_bomb` that keeps its value hides "the source kept a husk", so the bomb now marks what it moved from, and a throwing *key compare* was added because the move bomb only produces the drop-it case. The eighteen survivors are behaviour-identical by construction, none in the walk: fourteen on the two pipeline gates (thresholds, `index_bytes_for`'s multiply, the two comparisons), one on `move_home`, one on the `-fno-exceptions` arm the test build does not compile, and two early exits that skip work with the same answer. One of those is the self-merge guard: without it the walk finds every key in itself and rebuilds nothing, O(n) of wasted probing, not a wrong answer.

*What the source keeps.* References and iterators into either map are invalidated, and the source's order changes as elements close the gaps. Both match `erase()` and are documented in `doc/usage.md`, section `void merge(map& source)`.

### `replace()` hashes ahead too, once the reason it could not was looked at properly

*2026-09-11 · #244 item 4 · kept · asked as "would it be simpler going from back to front"*

`replace()`, the last bulk build path without lookahead, now hashes ahead. The obstacle looked structural: removing a duplicate pulls `back()` into the hole, and the lookahead has already hashed elements near the back.

**Back to front does not help**, it makes it worse: everything above the cursor is already *placed*, so moving `back()` leaves a bucket pointing at a dead index that must be found and repointed. A stale ring is traded for a stale index.

**What does work is noticing that the pull disturbs exactly one position**, the last. While the container is longer than the window by more than one, the window cannot hold the moved element, so the ring stays valid and a duplicate costs one re-hash of its slot, not a flush. Only the last `pipeline_depth + 1` elements run unpipelined. Survivors, final order and number of moves are unchanged; the test checks element by element against an independent copy of the plain loop.

ns per element, rebuilding through `replace()`, medians of three:

| | no duplicates | 5% duplicates | 50% duplicates |
|---|---|---|---|
| 200000, before | 5.77 | 8.51 | 8.73 |
| 200000, after | **4.87** | **5.54** | 8.48 |
| 2000000, before | 10.76 | 14.99 | 16.85 |
| 2000000, after | **8.04** | **9.93** | 16.15 |

1.19-1.34x with no duplicates and **1.5x at five percent**. A duplicate does not advance the lookahead, so at fifty percent the pipeline barely fills: 1.03x, nothing lost.

**The boundary has slack on both sides, which is the useful thing the mutation sweep said.** Six survivors, all on the loop condition. The exact requirement is `size > value_idx + pipeline_depth`; the shipped `+ 1` is one stricter. Tightening it is correct (one survivor), and loosening it by one is *also* correct, because the iteration that could read a stale entry is the one after which the loop exits (two more). The rest turn the pipelined loop off. The margin is kept: an off-by-one here returns a wrong answer rather than crashing.

One survivor, `lookahead < size()` to `<=`, read `m_values[size()]`: undefined, but inside the vector's *capacity*, so neither sanitizer objected (the same class as the note elsewhere about reading one slot past a bucket). The guard was dead, since `lookahead` was always `value_idx + pipeline_depth` and the loop condition already guarantees it; cleanup removed it and the variable.

**The ring holds the hash taken apart, not the hash**: fingerprint word, home group and block pointer, filled by the `fetch` that issues the prefetch. Decomposing at the far end of a sixteen-deep ring does the table load, the `& 7` and the shift twice and forms the block address three times. Slope of two round counts, 2000000 elements: **90.8 instructions per element to 85.6** with no duplicates, 83.7 to 81.9 at fifty percent. Wall clock moved 3-5% the other way, which is code layout luck; the count is the measurement. The bulk visit found the same a day earlier: the 88 byte block wants its address formed once and reused.

**And the pipeline only runs once the work is out of cache, which the first version of this got wrong.** The ring costs **13.2 instructions per element** (73.4 to 86.6 at n = 1024), and a prefetch of a cached line still occupies a load port. Ungated, with no duplicates, it was a fifth slower at 32768 and level at 65536. The first two sizes measured, 200000 and 2000000, are both above the crossover. **Two sizes on the same side of a cache is one size**: the octave rule, in a place with no harness.

With the gate and the decomposed ring, against main, three interleaved repeats, median:

| n | no duplicates | 5% | 50% |
|---|---|---|---|
| 8192 | 0.98 | 0.98 | 0.96 |
| 65536 | 0.81 | 0.94 | 0.91 |
| 262144 | 0.64 | 0.65 | 1.01 |
| 2000000 | 0.72 | 0.68 | 1.01 |

The 1.20x below the gate is gone and the wins above grew: **1.5x at a quarter million**, where the ungated version read 0.90. Fifty percent duplicates ties everywhere, as before.

The gate costs mutation score: survivors went from 6 to 26, the twenty new ones all on the footprint arithmetic, the threshold and the two prefetches. A correctness test cannot catch a mutant that moves a *performance* gate. Read it as: no survivor is in the loop body. Do not chase it by loosening the tests. `do_insert_range` and `do_visit` also pipeline and need no gate, see [The other two pipelines do not want the cache gate](#the-other-two-pipelines-do-not-want-the-cache-gate-and-the-gates-own-footprint-model-was-wrong).

**Later (2026-09-11):** the gate shipped here compared values-plus-index against 1 MiB. A sweep with a 1032 byte value showed the crossover follows index size, and the gate became `index_bytes > 256 KiB`, see [The other two pipelines do not want the cache gate](#the-other-two-pipelines-do-not-want-the-cache-gate-and-the-gates-own-footprint-model-was-wrong).

### `insert(first, last)` sizing the table from the range: built, measured, declined

*2026-09-11 · #248 · rejected · `scripts/ab/range_insert.cpp`, Ryzen 9 7950X, clang 22*

A range insert that sizes the table from the range is worth **2x** on the common case and was not shipped: every version that gets the 2x is a heuristic about data the map cannot see. Three attempts.

*First: `reserve(size() + std::distance(first, last))`, as the issue proposed.* A range is not its number of distinct keys. At 99% duplicates it asks for **128 times** the needed index, 2097152 buckets for 9923 elements, for the map's lifetime. It is not even faster, since every probe misses in a mostly empty table: **1.21** against growing.

*Second: sample the head and extrapolate the fresh-key rate.* On `range_insert.cpp`'s own duplicate model it reproduced the growth bucket count **exactly** at every rate from 0% to 99%, because that model draws duplicates from a pool growing with the range, so the fresh rate is stationary. Real ranges are not. **2048 keys drawn from ten thousand come back 90% new**: the birthday bound, not a duplicate rate. Over a million elements that predicts nine hundred thousand distinct keys instead of ten thousand, 64x off. The unit tests caught it; the benchmark could not have (lesson in CLAUDE.md).

*Third: read the sample as a capture-recapture.* Split the sample. If the range draws from `total` distinct keys and the first half caught `warmed`, the share of the second half that is a **repeat** estimates `warmed / total`, so `total = warmed * measured / again`. Repeats are informative exactly where freshness is not. Buckets at a million elements, against 16384 / 131072 / 1048576 needed:

| distinct keys | first try | second try | capture-recapture |
|---|---|---|---|
| 10000 | 2097152 | 1048576 | **16384** |
| 100000 | 2097152 | 2097152 | **131072** |
| 500000 | 2097152 | 2097152 | **1048576** |

0.51 when fully distinct, 0.48 at 70-90% distinct, neutral below half, clamped to never exceed reserving the range's length. **It was still declined.** An ordered range (every key once, then the set again) defeats any prefix sample, and nothing fixes that. The map would be guessing at its caller's data, with a 4x memory cost when wrong. The 2x is not worth that.

*The mechanism is not the one the issue assumed* (index rehash per doubling, lost in-flight prefetches). Reserving **only the value container** captures nearly all the win; **only the buckets** is worse than nothing. A million distinct keys, ns per element: no reserve 19.6, values only 10.4, values and buckets 9.9, buckets only 22.3. The cost is the value vector's geometric reallocation, copying 16 MB repeatedly. That rules out the cheap half too: a value is 16 bytes against the index's 5.5 per slot, so a wrong guess on values costs more.

*`std::distance` is only free on a random access iterator.* Over a `std::list` it walks the range twice, a **1.20 loss** at a high duplicate rate; any future attempt must exclude merely-forward ranges. Kept: `range_insert.cpp` gained a duplicate rate, a `std::list` source and a bucket count in its output.

### The other two pipelines do not want the cache gate, and the gate's own footprint model was wrong

*2026-09-11 · #247 · kept · `scripts/ab/{range_insert,bulk_visit,replace_bulk}.cpp`, Ryzen 9 7950X, clang 22, `uint64_t` keys; each loop built as shipped and with the ring removed (plain per-element loop, same checksums), one binary each, alternated*

The range insert and the bulk visit keep ungated rings; `replace()`'s gate now compares index bytes.

*The premium is flat and predicts nothing.* Instructions per element, plain to pipelined: `replace()` 73.4 to 86.6, `do_visit` 70.3 to 86.0, `do_insert_range` 81.0 to 97.9, i.e. +13.2, +15.7, +16.9, each constant across sizes to 0.2. The largest premium belongs to the loop that never needs a gate.

*The range insert.* Medians of seven, pipelined over plain: 1.02 and 1.01 at n = 1024 and 4096 reserved, 1.08 at 4096 growing, then 0.75 at 16384, 0.65 at 32768, 0.48 at 262144, 0.54 at four million: **slightly negative below a few thousand elements and a clear win from sixteen thousand**. Small sizes are bimodal (0.46 and 1.52 both at n = 4096) because each round rebuilds the map. No gate: the loss is inside the noise where it happens.

*The bulk visit.* Medians of five. All hits: 1.08 / 1.05 / 1.01 at 1k / 4k / 16k, 0.90 at 128k, 0.83 at four million. Half missing: 0.91 / 0.89 / 0.88, then 0.84. The only loss is when **all** keys hit a map below about sixteen thousand, and there a 50% hit rate *wins* 11%. A footprint gate would give up that win, on an axis unknown until a chunk is done. Measuring the first chunk and falling back would get both, but was rejected on cost: a second loop body beside three passes is an inlining-budget change, which the paired harness cannot measure, for at most 8% on one slice.

**And the same sweep, run with a second value size, says `replace()`'s gate was comparing the wrong number.** It compared values-plus-index against 1 MiB. At one value size that and an index-only model are both proportional to n, so they could not be told apart. With a 1032 byte value they separate:

| n | index | 16 byte value | 1032 byte value |
|---|---|---|---|
| 10000 | 88 KiB | 1.05 | 1.14 |
| 14000 | 176 KiB | 0.75 | 1.06 |
| 20000 | 176 KiB | 1.05 | 0.98 |
| 28000 | 352 KiB | 0.84 | 0.98 |
| 40000 | 352 KiB | 0.82 | 0.97 |
| 80000 | 704 KiB | 0.91 | 0.91 |
| 160000 | 1408 KiB | 0.68 | 0.91 |

Both cross at the same **index** size, as the mechanism predicts: the prefetch is only for the index, and the values are walked in order. The footprint model opens the gate for a 1032 byte value at ten thousand elements, where the measurement says 1.14. The gate is now `index_bytes > 256 KiB`, and `pipeline_min_bytes` is `pipeline_min_index_bytes`.

176 KiB reads 0.75 at n = 14000 and 1.05 at n = 20000, same array, because load factor runs 0.43 to 0.61 between doublings. **The octave rule applies to this axis too**: one n per size measures neither a cache effect nor a load-factor effect, and the first version of these numbers took one.

Unconfirmed: the ring may pay only where the out-of-order window cannot already find independent work, which would explain why the range insert (vector append plus capacity branch) never loses. A stall count on the two bodies would settle it.

The gate change touched a test twice. The 64 KiB mapped type that crossed the footprint gate at sixteen elements never crosses an index gate, so the window boundary is now swept by walking a duplicate through each of the last forty positions of a thirty-thousand element container. And mutation found an untested state: `replace()` keeps an already grown index, so replacing a large map with a handful of elements leaves the index past the gate and the container shorter than the window. The `&&` stops the prologue reading past the end there; `||` survived until that case got a test.

Two measurement notes. The control first tried for the range insert, a caller's loop of single inserts, also carries ~15 instructions of call boundary per element (see [The insert path's instructions, counted one by one](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it)) and shows the range winning everywhere, hiding the pipeline penalty. The control has to be the same entry point with the ring removed. And `replace_bulk.cpp` timed the container copy with the call, invisible at 16 bytes a value and dominant at 1032; the second table was taken with the copy moved out of the timed region.

### Chunks or a sliding ring: the rehash and the bulk visit want opposite answers, and the reason generalises

*2026-09-11 · no issue · kept · asked as "would a streaming approach perform better" and then "but doesn't resize use that streaming too, it looks like it would be better with chunking"*

Both shapes were built for both loops, and each loop is fastest in the shape it already had.

**The bulk visit wants chunks.** Three passes beat a sliding ring at four and sixteen million entries with identical instruction counts; the ring loses in the *value* loads. The numbers are in [A bulk `visit()`, and the `prefetch(key)` API it replaced](#a-bulk-visit-and-the-prefetchkey-api-it-replaced----which-was-measured-against-the-wrong-baseline), under [Three passes rather than a sliding ring](#a-bulk-visit-and-the-prefetchkey-api-it-replaced----which-was-measured-against-the-wrong-baseline).

**The rehash wants the ring.** Chunked against the shipped streaming version, ns per element:

| | 200000 | 1000000 | 4000000 |
|---|---|---|---|
| `uint64_t`, streaming | **2.24** | **6.16** | **17.19** |
| `uint64_t`, chunked | 2.42 | 6.89 | 18.03 |
| string, streaming | **7.70** | **10.73** | **20.68** |
| string, chunked | 8.25 | 12.57 | 22.91 |

For a string key both retire ~137 instructions per element and the chunked one spends **12.5% more cycles** (99.1 against 88.1).

**What decides it is how many dependent random accesses an element has.** A visit has two: the block, then the value the slot points at. Separate passes let the second batch issue sixteen at once, the only parallelism for an access that is not prefetched. A rehash has **one**: it reads the block and writes into it. With nothing to batch, only the time between prefetch and use matters, and a ring gives every element sixteen *placements* of cover. A chunk gives its first elements only the rest of pass one, which is cheap hashing, so they stall.

So: **chunk when an element has two dependent accesses, stream when it has one.** The string column checks it: the most work per element, moving most in both loops, in opposite directions.

### A bulk `visit()`, and the `prefetch(key)` API it replaced -- which was measured against the wrong baseline

*2026-09-11 · #232 · kept · asked as "would it make sense to have a bulk api"*

`prefetch(key)` shipped documented at 1.5x and was reverted the same day; `visit(first, last, f)` replaced it.

**The baseline was the bug.** Its harness (`scripts/ab/prefetch_api.cpp`, deleted with it) compared a pipelined loop against a plain one that *acquired the key inside the loop body*, a random index into another array, so each key miss sat in front of its map miss. Against that, pipelining read 1.5x. But **collecting the keys into a batch first and looking them up afterwards is worth 1.5x on its own**, with no API: 50.9 ns to 33.4 per lookup at four million entries, because two short loops each saturate their own memory parallelism. In the batched loop (the README example's shape, keys already in a container) `prefetch(key)` was **a 3% loss**, 34.3 against 33.4. With `visit` in, it covered a narrow case badly, and removing it before 5.0 was free where later would have been breaking.

**What replaced it is boost's shape**: `concurrent_flat_map::visit(first, last, f)` with `bulk_visit_size = 16`, a chunk in three passes: hash every key and fetch its home block; match fingerprints once the blocks arrive; compare keys and call `f`. Against the same batch looked up one key at a time, `map<uint64_t, size_t>`, medians of three:

| entries | work | one at a time | `visit` | |
|---|---|---|---|---|
| 4000000 | hit | 33.97 | **26.34** | 1.29x |
| 4000000 | half | 34.37 | **28.44** | 1.21x |
| 16000000 | hit | 36.53 | **30.40** | 1.20x |
| 16000000 | half | 37.31 | **31.53** | 1.18x |

The first working version read 1.07-1.14x. It computed `groups[home[i]]`, a multiply for an 88 byte block, three times per key (prefetch, match, compare). Holding the pointer is worth **10% of the whole operation**: non-power-of-two address arithmetic is not free.

**The reason it was built is not the reason it works.** Boost prefetches the *element* in pass 2. The value load is the dependent access a single lookup cannot hide, which issue #229 measured and closed; in a batch its address is known. Isolated, that prefetch is **+3.8% on all hits and -4.8% at half hits**, so it is not shipped: a miss has nothing to fetch, and a hit is already covered by the out-of-order window once the passes are separate. **The gain is the separation, not the fetch.**
#229's conclusion stands.

**Three passes rather than a sliding ring, and the ring is the one that loses** (asked 2026-09-11 as "would a streaming approach perform better, I guess not because this can unroll"). `map<uint64_t, size_t>`, medians of three, ns per lookup:

| | 200000 hit | 4M hit | 16M hit | 4M half | 16M half |
|---|---|---|---|---|---|
| three passes | **6.94** | **26.63** | **30.35** | **28.27** | **31.67** |
| sliding ring | 7.53 | 31.64 | 34.72 | 29.28 | 32.05 |

**And it is not code generation**: both retire **224.5 instructions per lookup**, identical to the tenth, and the ring spends **15% more cycles** (339.0 against 294.5 at four million, all hits). The gap is 14-19% on all hits above cache and 1-4% at half hits (corrected 2026-10-02: said "15% on all hits and 6% at half hits"), and **a miss reads no value at all**, so the ring loses the *value* loads: the third pass issues sixteen back to back, the ring one per iteration. The ring even has the *better* block prefetch (a full depth of work for every element, where a chunk gives its last element less than its first), and still loses by 19%.

A ring must also read `ring[i % depth]` before slot `i + depth` overwrites it. Getting that backwards returns a wrong answer, not a crash; it happened twice here, in a test and in a README example, and only the test caught it. The chunk is faster and harder to get wrong.

**A chunk of 16 is not load-bearing**: 8 through 32 measured within 2%. Sixteen is what boost uses.

**Mutation swept: seven survivors, all of them correct-but-slower or unreachable**, as a pipeline's should be. Deleting the block prefetch has no observable result. `++i` to `--i` still visits every key once, because the outer loop re-chunks from wherever `first` reached, at sixteen times the hashing. Deleting the `break` in the lane walk scans lanes that cannot match. A counter test that always passes runs the fallback for every key. `m_group_mask != 1` is unreachable while the smallest array is four groups. The two real ones were **caught**: `--first`, and the counter's `!= 0` turned into `!= 1`, the probe split's hole, covered by the same steered construction.

### `visit()` re-measured across the cache boundary: the cache-resident loss is a hit-rate effect

*2026-09-13 · no issue · info · `scripts/ab/bulk_visit.cpp`, Ryzen 9 7950X, clang 22, `map<uint64_t, size_t>`, 4000000 lookups per cell, `taskset -c 2`*

Below the cache, `visit()` loses only when every key hits; with half misses it wins at every size. The README gave 1.18x to 1.29x from 4 million and 16 million entries, both past every cache here, and called the cache-resident case "a small loss". Two sizes on one side of a cache are one size, so it was re-taken over six sizes and two hit rates. ns per lookup, `plain` (one key at a time) against `bulk` (`visit()` on the same batch):

| entries | plain, all hits | bulk | | plain, half hits | bulk | |
|---:|---:|---:|---:|---:|---:|---:|
| 1000 | 3.95 | 4.43 | 0.89x | 8.16 | 6.83 | 1.19x |
| 10000 | 4.44 | 4.63 | 0.96x | 8.82 | 7.16 | 1.23x |
| 100000 | 7.13 | 6.55 | 1.09x | 11.44 | 9.40 | 1.22x |
| 1000000 | 18.41 | 15.35 | 1.20x | 23.03 | 20.10 | 1.15x |
| 4000000 | 38.10 | 25.46 | 1.50x | 34.80 | 28.01 | 1.24x |
| 16000000 | 36.49 | 29.03 | 1.26x | 37.35 | 30.68 | 1.22x |

**Open (2026-10-02):** the 4M all-hit plain cell (38.10) reads 12% above the earlier entry's 33.97 and above the 16M cell; treat its 1.50x as unconfirmed. Not re-measured.

**The prose was half wrong.** The cache-resident loss is real with every key present (0.89x at a thousand, 0.96x at ten thousand) and gone once half miss (1.19x and 1.23x there, 1.15x to 1.24x above). So what decides `visit()` is size and hit rate together, as `bulk_visit.cpp`'s header comment says. The reading that a miss the chunk absorbs is a branch the one-at-a-time loop mispredicts is not measured separately.

The batching underneath, `inline` (fetch and look up in one loop body) against `plain`, is 1.30x at a million all hits, 1.32x at four million, 1.55x at sixteen million and 1.57x at four million half missing. The naive loop against a batch handed to `visit()` is 50.26 to 25.46 ns at four million: **1.97x**, of which the batch is the larger half. That is what a caller sees. **Open (2026-10-02):** by this table the batch is 1.32x and `visit()` 1.50x, so `visit()` is the larger half; the earlier entry read 1.52x and 1.29x. It turns on the 4M plain cell above; not re-measured.

### `replace()` + `extract()` as the way to unique a vector, and the duplicate rate that turns it over

*2026-09-13 · no issue · info · `scripts/ab/unique.cpp`, also built `-DUDM_UNIQUE_U64`, Ryzen 9 7950X, clang 22, string keys 8 to 135 bytes skewed short, median of 31 rounds or more, `taskset -c 2`*

With no duplicates, `replace()` + `extract()` dedups a vector 10x to 27x faster than a non-dense set copied out and 2.7x to 5.1x faster than the range constructor. At a million elements the range constructor wins at 50% and 90% duplicates.

`set.replace(std::move(v))` makes the caller's `std::vector<std::string>` the set's own storage and `std::move(set).extract()` hands it back, so no string is copied or allocated. Five variants, checked against one order-independent checksum and unique count: `replace()` + `extract()`; the same set built from the range, then extracted; `boost::unordered_flat_set<std::string>` copied into a fresh vector; a `boost::unordered_flat_set<std::string_view>` over the caller's vector, compacted afterwards (moving an element invalidates the view the set holds); `std::sort` + `std::unique`.

**Every variant has to release the input inside the clock, and the first version of this harness let two of them off.** `replace()` consumes the vector and destroys its duplicates; a separate set leaves the caller all n elements still to destroy. Fixing it moved the million-element insertion column from 40.07 to 70.46 ns at no duplicates and reversed the winner at half duplicates. Same trap as the README's build panel: a benchmark must say what state each contestant ends in.

**And eleven rounds was not enough to exclude an outlier.** The first sweep read 86.78 ns for `replace()` at a million, half duplicates; five later runs read 61.4 to 64.1, a rebuild of the *old* binary 61.0 to 62.3. The harness now prints min and max beside the median.

**The range constructor wants a `std::make_move_iterator` pair, and that is worth 1.4x to 2.4x.** The range insert probes before constructing, so only kept elements are moved out and duplicates stay untouched; with `extract()` that is a unique with no string copy. No duplicates, ns per element, copy to move: 40.20 to 23.57 at a hundred thousand and 70.64 to 49.83 at a million for 8 to 135 byte keys, 48.07 to 25.39 and 141.32 to 59.44 for 200 bytes and up. It grows with the key because below the small string buffer a move is a copy. A set's elements are `const`, so the way *out* of a non-dense set stays a copy; that is what `extract()` is for.

ns per input element, no duplicates, one run:

| | 1000 | 10000 | 100000 | 1000000 |
|---|---:|---:|---:|---:|
| `replace()` + `extract()` | 4.97 | 6.48 | 8.62 | 9.73 |
| range constructor (moved) + `extract()` | 14.82 | 23.68 | 23.57 | 49.83 |
| boost, copied out | 58.23 | 89.16 | 88.15 | 264.32 |
| boost set of views, compacted after | 10.72 | 15.91 | 18.10 | 28.83 |
| `std::sort` + `std::unique` | 44.90 | 115.12 | 146.80 | 222.62 |

10x to 27x over a non-dense set, 2.1x to 3.0x over the hand-written no-copy version, 2.7x to 5.1x over the range constructor. The top two rows differ only in the container: `replace()` gets a vector of the right size, the range constructor doubles its own and moves everything at each step.

Duplicates narrow it. A million elements, ns per input element:

| | 0% | 50% | 90% |
|---|---:|---:|---:|
| `replace()` + `extract()` | 9.73 | 61.57 | 43.84 |
| range constructor (moved) + `extract()` | 49.83 | 54.36 | 28.60 |
| boost, copied out | 264.32 | 133.59 | 38.26 |
| boost set of views, compacted after | 28.83 | 59.44 | 42.77 |
| `std::sort` + `std::unique` | 222.62 | 256.39 | 267.38 |

Across sizes at half duplicates `replace()` reads 10.41 / 16.36 / 24.07 / 61.57 against the moved range constructor's 14.83 / 26.71 / 27.16 / 54.36: ahead to a hundred thousand, behind at a million. At nine tenths, 15.86 / 20.00 / 27.50 / 43.84 against 14.10 / 21.32 / 23.79 / 28.60.

**Retracted, 2026-09-13, same day it was written: "insertion pays no `free` for a duplicate, because it never copied it anywhere."** That explained the turn-over and does not survive the harness's own accounting.

**Correction (2026-09-13):** both release every duplicate, `replace()` as it walks, the range constructor when the caller's vector goes, and the corrected harness charges both. What differs is *when*: `replace()` inside a walk whose probes go to random addresses, the range constructor afterwards in one sequential sweep. That is a reading of the numbers, not a direct measurement.

**The erosion is the dedup walk, and the element type shows it on its own.** With `std::uint64_t` at a million, `replace()` reads 3.81 / 6.97 / 4.93 against insertion's 8.72 / 8.45 / 4.91 at 0 / 50 / 90%: the lead erodes from 2.3x to level and never reverses. A duplicate's removal pulls `back()` into the hole and hashes it one iteration before its probe, so its block arrives with no lookahead, and a duplicate-heavy input is mostly such elements.

**A forward compaction removes the re-hash entirely, and is not a free win** (`scripts/ab/replace_forward.patch`, measured 2026-09-13, one header per binary). Stepping over duplicates and moving survivors down hashes every element once with full lookahead and keeps the order stable. It moves one element per element *kept*, where the pull moves one per duplicate:

| duplicates | 5% | 10% | 20% | 30% | 40% | 50% | 60% | 75% | 90% |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100000 elements | 0.83 | 0.84 | 0.89 | 1.02 | 1.02 | 1.10 | 1.15 | 1.24 | 1.27 |
| 1000000 elements | 0.92 | 0.93 | 0.98 | 1.07 | 1.18 | 1.33 | 1.41 | 1.51 | 1.47 |

Crossover about 25-30% on both sizes, nothing at 0%. At the top it takes a million strings at nine tenths duplicates from 44.2 to 29.2 (1.51x; **Open (2026-10-02):** the sweep above reads 1.47 at 90%, likely a separate run, not re-measured) and `uint64_t` from 4.93 to 3.68, putting the integer case ahead of insertion at every rate measured.

**Destroying the leftover tail front to back is worth 1.03x at 90% and 1.04x at 50%** (corrected 2026-10-02: said "1.04x at 90% and 1.05x at 50%") (29.2 to 28.3, 47.5 to 45.6): the tail holds live duplicate strings and glibc cares about release order. It needs `erase(first, last)`, which `std::vector`, `std::deque` and `boost::interprocess::vector` have and `segmented_vector` does not, and the header deliberately does not require it ("the container is only required to have pop_back", `drop_tail_and_reindex`).

**And the other call site that has that same comment on it gains nothing from the change** (measured 2026-09-13, `scripts/ab/merge.cpp -DUDM_MERGE_STR`, medians of nine, one header per binary). `merge()`'s `drop_tail_and_reindex` with `erase(keep, end)` instead of the `pop_back` loop: 23.414 to 23.270 ns at 200000 and 0% overlap, 24.033 to 24.266 at 25%, 53.461 to 54.388 at a million and 0%, 52.469 to 52.547 at 25%. Inside 2% with the sign flipping: a null. Merge's tail is *moved-from* elements, and an empty `std::string` husk destructs without a `free`, so release order is moot. Only a tail of live elements, as the dedup leaves, has anything to reorder.

So a `segmented_vector::erase(first, last)` would today serve one unapplied patch. If the forward compaction lands, the cheap shape is an `if constexpr` on whether the value container has `erase(first, last)`: it covers `std::vector`, the default, asks nothing new of a custom container, and spares `segmented_vector` a cross-segment move. Both call sites only truncate a suffix, which `segmented_vector` already does front to back in its private `resize_shrink`.

**Later (2026-09-26):** `segmented_vector` now truncates back to front in `destroy_tail` (no `resize_shrink`), see [Tearing a segmented_map down in reverse](#tearing-a-segmented_map-down-in-reverse-glibc-keeps-the-memory-for-the-next-build-instead-of-handing-it-to-the-kernel-warm-builds-18-32x-when-values-own-heap-memory). A dedup tail through it would be released in the order measured above as 1.03-1.04x slower.

Not applied. It regresses the 5-25% band, where a bulk load most likely is, and it changes `values()` afterwards from "partly reordered" to input order, which four tests in `test/unit/replace.cpp` and `fuzz_replace_map` pin. Worth revisiting for a caller whose input really is mostly duplicates.

What this says and does not say: one machine, one compiler, one key distribution. A vector of 200 byte keys would move every row, since more of the time is hashing and less is `free`. It says nothing about `map`, only `set`. `std::sort` is in the tables for reference, not as a rival: it also sorts.

### Loading a map from its values and its index: an owning load is 3-17x faster than building at 1M-64M entries and a view costs 0.85 ns per entry to check, but the slot check is most of an owning load, so the owning constructor takes `trust` too

*2026-10-02 · #299 · kept · Ryzen 9 7950X, clang 22 and gcc 16, THP `madvise`, `AB_CORE=2`; `scripts/ab/index_load.sh` (one binary per mode, median of 7 rounds, the loaded map destroyed outside the clock, RSS of one load in a forked child before the timed rounds)*

`index()`, `index_format_id`, the owning constructor, `map_view`/`set_view`, `view()`, the owning constructor from a view and `verify()` shipped in #299. The values and the index sit in 64-aligned memory before the clock starts, so no row measures the page cache or the page size (#301 owns that question). `map<uint64_t, uint64_t>` and `map<std::string, size_t>` (workload string keys), ns per entry:

| | n | build (reserved) | owning, checked | owning, unchecked | check cost | view, checked | view, unchecked | `verify(full)` |
|---|---|---|---|---|---|---|---|---|
| clang u64 | 1M | 9.27 | 2.99 | 0.494 | 6.05x | 0.855 | 0 | 3.20 |
| clang u64 | 4M | 32.8 | 6.36 | 6.85 | 0.93x | 0.849 | 0 | 12.7 |
| clang u64 | 16M | 41.6 | 7.11 | 4.26 | 1.67x | 0.851 | 0 | 16.0 |
| clang u64 | 64M | 45.4 | 7.08 | 4.14 | 1.71x | 0.851 | 0 | 17.5 |
| gcc u64 | 1M | 8.06 | 1.91 | 0.449 | 4.25x | 0.872 | 0 | 3.17 |
| gcc u64 | 4M | 33.0 | 5.31 | 6.79 | 0.78x | 0.871 | 0 | 11.7 |
| gcc u64 | 16M | 41.4 | 5.89 | 4.26 | 1.38x | 0.871 | 0 | 15.0 |
| gcc u64 | 64M | 44.0 | 5.80 | 4.11 | 1.41x | 0.870 | 0 | 16.8 |
| clang str | 1M | 24.0 | 6.13 | 3.74 | 1.64x | 0.857 | 0 | 12.1 |
| clang str | 4M | 95.6 | 6.62 | 5.32 | 1.24x | 0.849 | 0 | 34.0 |
| gcc str | 1M | 23.0 | 5.29 | 3.76 | 1.41x | 0.881 | 0 | 11.3 |
| gcc str | 4M | 97.4 | 5.65 | 5.33 | 1.06x | 0.871 | 0 | 31.4 |

`verify(spot)` is 16 lookups, under 0.0001 ns per entry at every size. Peak RSS of one load, bytes per entry: building, owning checked and owning unchecked are within 0.5 of each other (27.5-27.8 for `uint64_t`, 110.9-111.4 for strings, the values included), and a view is 0: it holds two pointers and two sizes.

**The issue's rule for the check fired.** It said that if the check fused into the copy cost more than 10% of the owning load at any size, the owning constructor gets a `trust` argument back. It costs 4.3-6.1x at 1M, where the copy alone runs in cache, and 1.38-1.71x at 16M-64M. The check reads every slot with a branch on a random fingerprint, and, for an owning table, sets a bit per value in a bitmap of n bits (below). Four rewrites of the loop, measured at 1M with clang on the owning load against 0.46-0.52 unchecked: the first version, a branch per slot, 2.34-2.81 across builds; branchless with a spare bit for empty slots 4.63; with a spare word per lane 4.19; branchy reading a copy of the block 3.11. Branchless pays for the view, whose check has no bitmap: 0.85 against 1.16. So the owning load keeps the branch and the view the branchless loop.

**Why the owning check needs the bitmap, found by `fuzz_index_view`.** The first check was the one the issue specified: every full slot in range, as many full slots as values. Under `_GLIBCXX_ASSERTIONS` the fuzzer found bytes that pass it and still read out of bounds: two slots pointing at value n-1, one value pointed at by none. An erase moves value n-1 into the hole and repoints the one slot `repoint_value` finds, and the other slot is left pointing at n-1, one past the end after the pop. ASan did not see it: the read is inside the vector's capacity. Requiring every value to be pointed at exactly once makes every later operation keep it so, and costs the owning load one bit per value. A view never erases and does not need it.

**Later (2026-10-02):** the file and the page size are measured in [A map_view over a mapped file](#a-map_view-over-a-mapped-file-on-hugetlbfs-its-random-hits-are-102-116x-faster-than-on-the-files-4-kb-pages-from-4m-to-64m-entries-and-22-24x-at-1m-it-starts-in-4-ms-and-is-shared-so-hugetlbfs-is-worth-having-and-not-required-a-lazy-4-kb-mapping-of-a-file-not-in-the-page-cache-needs-87-s-for-its-first-100000-lookups-at-64m): a view on the file's 4 KB pages looks up at the owning map's speed from 4M entries up, and 2 MB pages are worth 1.03-1.13x there.

What this says and does not say: a load from memory, not from a file; the 4M `uint64_t` row reads the unchecked load slower than the checked one on both compilers (6.85 against 6.36, 6.79 against 5.31), which nothing in the code explains, and is most likely where the allocator puts a fresh 27 MB index next to a 512 KB bitmap or without one -- unexplained, and the other three sizes agree with each other. The 1M and 4M rows of the build are in cache and out of it; the loads are bandwidth: 7 ns per entry at 64M is 27.5 bytes of values and index per entry (the RSS column), plus the page faults of the fresh index.

## The hash function

The string hash `hash_bytes`: the restructure that ships, the attempts to make it cheaper, the comparison with twelve other hashes, and the store-forwarding fix for small fixed-size keys. A map pays a hash's latency, not its throughput, so a hash that wins a hashing loop can lose in the map. The hash is at its floor: every cheaper structure that still avalanches measured as nothing in the map.

**Where it stands** (as of 2026-09-27)

- The shipped string hash mixes each 16 byte block from 17 to 144 bytes independently into one finalizer: `hashstr` 1.13 clang and 1.12 gcc, score 1.006 and 1.039 ([The string hash restructured](#the-string-hash-restructured-independent-blocks-from-17-to-144-bytes)).
- A map's hash is chosen on latency: AES-NI is 28% faster at hashing and 9-37% slower at everything the map does ([Latency is what a map pays, tested rather than argued](#latency-is-what-a-map-pays-tested-rather-than-argued)).
- Of twelve hashes, nothing that passes avalanche has lower latency than this one; `absl::Hash` and foldhash-fast buy their speed with avalanche ([Six more hashes tried, none adopted](#six-more-hashes-tried-none-adopted-and-the-pair-of-columns-says-why)).
- Dropping a multiply from the block range is +1.75 instructions per lookup and cycles in the noise; the short path cannot lose one at all. The hash is done ([Why `absl::Hash` is faster below 32 bytes](#why-abslhash-is-faster-below-32-bytes-and-what-taking-it-would-cost)).
- Length-dispatch tunings are worth nothing in the map; a microbenchmark that chains through key selection puts the key's length on the chain ([Four attempts at tuning the hash for latency, all worthless in the map](#four-attempts-at-tuning-the-hash-for-latency-all-worthless-in-the-map-and-the-microbenchmark-that-said-otherwise-was-wrong)).
- A key of compile-time length 8, 12 or 16 is read as 4 byte words under gcc and clang: a lookup right after writing a 12 byte key goes 146 -> 40 cycles (#311, the `hash_bytes` entry).

### The string hash restructured: independent blocks from 17 to 144 bytes

*2026-09-06 · no issue · kept · one function per binary on the scored keys, then paired on the score*

Asked as "make the hash faster, the values may change". Every 16 byte block up to 144 bytes is now mixed on its own with its own pair of secrets and xor-folded into one finalizer. Latency is one multiply plus the finalizer for any length in the range. wyhash chained its 16 byte blocks through `seed`: a 48 byte key was three multiplies in a row before the finalizer, and the map cannot form a group address until the last resolves. Its block loop's trip count was a data-dependent branch that mispredicts whenever lengths vary, which the scored keys do on purpose; now the branches are a short chain of compares on `len` that the predictor learns from the top. The short path and the long chained lanes are unchanged, so lengths up to 16 and past 144 hash as before. `hash_golden.cpp` was regenerated for the range between, as its own comment says to do.

Scored keys (8 to 135 bytes, skewed short), one function per binary, ns per hash:

| | clang throughput | clang latency | gcc throughput | gcc latency |
|---|---|---|---|---|
| wyhash as it was | 2.52 | 8.64 | 2.18 | 8.47 |
| 4.8.1's wyhash | 2.19 | 9.14 | 2.21 | 9.16 |
| independent blocks | **2.00** | **7.69** | **2.05** | **7.81** |

Paired on the score, clang / gcc: `hashstr` **1.13 / 1.12**, `rmissstr` 1.08 / 1.10, `iestr` 1.09 / 1.08, `buildstr` 1.07 / 1.06, `findstr` 1.06 / 1.06, `rhitstr` 1.04 / 1.04, `churnstr` 1.05 / 1.00, every integer workload at parity; score 1.006 / 1.039. This closes the "string hash is 4-7% slower than 4.8.1's" line in [Where a year of this got to, measured against 4.8.1](#where-a-year-of-this-got-to-measured-against-481), in the other direction.

Measured and rejected on the way, all on the same keys:

- **Branchless middle**: always mix three overlapping blocks for 17-48 and six for 49-96, so the length decides only the read offsets. Slower on both compilers (2.48 against 2.35 throughput under clang, 2.45 against 2.04 under gcc): the redundant multiplies cost more than the mispredictions they remove. The probe found the opposite, because a multiply is a real unit of work and a group compare is not.
- **One multiply on the short path** (`mix(a ^ s ^ len, b ^ seed)` and no finalizer): the fastest thing measured, 1.96 and 1.95 throughput, 7.41 and 7.74 latency, and it fails avalanche outright. At 8 bytes some output bits never flip for some input bits (worst |p - 1/2| of 0.50, mean 0.17 against 0.003 for the two multiply version): the two reads are the same eight bytes, and one product of them has no second chance to mix. Dropping the finalizer in the block range fails more gently (bits biased 0.42/0.58 at every length). Both multiplies stay; "the values may change" does not extend to a hash that does not avalanche.
- **AES-NI**, asked as "how about SSE for the hash": one `aesenc` per 16 byte block into an accumulator and two finishing rounds, compiled with `-maes`. Throughput 1.63 against 2.18 under clang and 1.47 against 2.07 under gcc, a quarter faster; latency **12.0 against 7.85** and 11.4 against 7.82, half again slower, and latency is what the map pays. In the map, measured a day later, it is worse than that (`rhit64`'s string twin at 0.63, see [Latency is what a map pays, tested rather than argued](#latency-is-what-a-map-pays-tested-rather-than-argued)). An `aesenc` is four cycles and the rounds are a chain. This is the 2025 gxhash finding again, on the right key lengths this time, and it needs a flag the header cannot assume. SSE2, the one vector ISA the header may rely on, has nothing that beats a scalar 64x64 multiply for mixing: `pmuludq` is two 32x32 products, and four of them plus the adds are slower than one `mulx`. A 16 byte load is no faster than two 8 byte ones. So: no.

**Open (2026-10-02):** the shipped hash's clang throughput reads 2.00 in the table above, 2.35 in the branchless-middle bullet and 2.18 in the AES bullet; the runs behind each are not recorded, so only each bullet's own ratio is comparable. Not re-measured.

### The last structural lever on the hash, taken and found to weigh nothing

*2026-09-07 · no issue · rejected · per-bit avalanche under two seeds; six hashers interleaved with a same-code control in the map*

Asked as "you are the most advanced model, improve the hash for the map". A cheaper repair for the block range's second multiply passes avalanche at boundary lengths, and in the map it is worth nothing. The block range's chain is two dependent multiplies; the second only repairs the bits a product leaves weak. The map reads the top bits for the group and the bottom byte for the fingerprint, and a per-bit avalanche of the fold *without* its finalizer says those are exactly the weak places: worst |p - 1/2| 0.076 in bits 0-7 and 0.074 in bits 36-63, 0.024 in between.

Two cheaper repairs work at every boundary length from 17 to 300 and under two seeds: **`x * C`** (one `imul`, 3 cycles, a bijection) and **`x ^ rotl(x, 32)`** (2 cycles), both at the 0.02 noise floor everywhere the current finalizer is. `hi64(x * C)` does not (0.041 in the top bits: the high half of a product by a constant is weak at the top).

**Correction (2026-09-09):** not everywhere. Boundary lengths are the ones that cannot see this shape fail; sweeping every length finds it failing at `len % 16 == 2` (see [Why `absl::Hash` is faster below 32 bytes](#why-abslhash-is-faster-below-32-bytes-and-what-taking-it-would-cost)).

Neither works on the short path: at 8 bytes the two reads are the same bytes and at 9-15 they overlap, so the inner multiply's operands are correlated and only a real second multiply repairs that (0.36-0.50 otherwise). So the candidate was scoped: the short path keeps its two multiplies, and the 73% of scored keys past 16 bytes lose one.

**In the map it gains nothing**, and `rotl` loses 2.9% on `rmissstr` (corrected 2026-10-02: said "In the map it is worth nothing."). Six hashers interleaved with a same-code control reading 0.996-1.008:

| hasher | `rhitstr` | `findstr` | `churnstr` | `iestr` | `rmissstr` |
|---|---|---|---|---|---|
| block-range `imul` | 1.006 | 1.009 | 1.009 | 1.009 | 0.988 |
| block-range `rotl` | 1.001 | 1.006 | 1.008 | 1.008 | 0.971 |

`hashstr` read 1.09-1.18 against a control of 1.08, which is that benchmark's layout swing and not a result. Three cycles off the hash's chain does not show in a lookup. Not adopted: when speed ties, the stronger finalizer is the one to keep for everyone who uses `hash<std::string>` outside the map.

### Four attempts at tuning the hash for latency, all worthless in the map, and the microbenchmark that said otherwise was wrong

*2026-09-07 · no issue · rejected · standalone latency harness on both compilers; six hashers interleaved with a same-code control in the map*

Asked as "can you do more latency tuning". A standalone harness showed 1.40x; in the map every variant reads at the control, because the harness put the key's length on the chain. The premise was sound: on unpredictable lengths the length dispatch costs **0.50 branch mispredictions per hash**, which at ~16 cycles each is about 8 cycles, two multiplies (corrected 2026-10-02: said "which at ~16 cycles is a whole multiply"). Four structures, all keeping the short path and the long lanes:

- **v2, 32 byte steps instead of 16**: half as many decisions for the predictor, at the price of up to one extra pair of reads and one extra multiply, which are independent and cost throughput rather than latency. Branch misses 0.50 to 0.42.
- **v5**: `len` out of the finalizer, so the last multiply has a compile-time constant operand.
- **v6**: `len` mixed into the head block as well as the finalizer.
- **v3**: both.

Standalone latency, reproduced on both compilers to 0.02 ns: v0 6.87, v5 5.52, v6 5.44, **v3 4.90**, a 1.40x improvement. clang and gcc agreeing exactly normally rules out layout.

**In the map every one of them is nothing.** Same map, the scored string workloads, six hashers interleaved with a same-code control: `rhitstr` v3 0.998, v5 0.996, v6 0.997 against a control of 0.996; `findstr`, `churnstr`, `iestr`, `buildstr` all 0.99-1.01. v2, the one that really removed mispredictions, is a consistent **loss** (0.92-0.97): the extra multiplies cost more than the branches they save, the same answer for the same reason as the branchless middle in [The string hash restructured](#the-string-hash-restructured-independent-blocks-from-17-to-144-bytes).

**Why the harness lied, and it is a trap worth naming.** To make lengths unpredictable it took the next key's index out of the previous hash, `x = hash(keys[x & mask])`. That is the standard way to build a dependency chain; the hash chart's latency panel avoids it by writing a byte of each answer into the next key (corrected 2026-10-02: said "and the hash chart's latency panel does the same"). But it puts the key's length on the chain: `len` is `keys[x & mask].size()`, so it arrives late, and a finalizer that consumes `len` extends the chain. A real lookup has no such edge: the caller holds the key, its length is known before the hash starts, and only the *bytes* are loaded. The assembly agrees: v0 and v5 differ by exactly one `xor` instruction before the final `mul`, with no spills in either, so one cycle, not the five the harness reported. `v7` hoists `S[1] ^ len` into a named variable and is the control that proves it is not scheduling: 6.84 against v0's 6.87.

So: **a latency microbenchmark that chains through key selection measures a chain the map does not have, and it overstates anything that touches the key's length or address.** The hash chart's latency panel does not have this edge, since it writes into the key rather than choosing it (corrected 2026-10-02: said "The hash chart's latency panel has the same edge, a second reason its numbers are an ordering rather than an amount").

The block range is at its floor: two dependent multiplies, and the second cannot go, since dropping it fails avalanche (bits biased 0.42/0.58) (corrected 2026-10-02: said "fails avalanche outright"). One multiply is ~4 cycles on about a third of a string lookup, ~1.5% of a lookup even if it were free, under the noise of the score. The remaining time is not in the hash.

**Open (2026-10-02):** "about a third of a string lookup" is unclear. 73% of the scored keys take the block range, and ~1.5% follows either from a third of lookups at ~90 cycles each or from 73% of them at ~180; neither lookup cost is measured in this file. Not re-measured.

**Later (2026-09-09):** with one rotate on the tail block's product the second multiply can go without failing avalanche, and in the map that is worth nothing, see [Why `absl::Hash` is faster below 32 bytes](#why-abslhash-is-faster-below-32-bytes-and-what-taking-it-would-cost).

### Latency is what a map pays, tested rather than argued

*2026-09-07 · no issue · rejected · `map<std::string, size_t>`, scored string workloads, three hashers interleaved by `compare()` in one binary; counters one hasher per binary*

AES-NI is **28% faster at hashing and 9-37% slower at every single thing a map does**. This replaces the reasoning-only AES-NI rejection in [The string hash restructured](#the-string-hash-restructured-independent-blocks-from-17-to-144-bytes) with a measurement. The third hasher is a *control*: `wy2`, the same wyhash written out as a second hasher type so every template is a second instantiation. What the control reads is what layout is worth here, and nothing smaller can be claimed for AES.

| workload | control | AES | what the operation can overlap |
|---|---|---|---|
| `hashstr` (a hashing loop) | 1.00 | **1.28** | everything: independent hashes |
| `buildstr` | 1.01 | 0.91 | a lot: the rehash hashes sixteen ahead |
| `iestr` | 1.02 | 0.80 | some |
| `churnstr` | 1.01 | 0.82 | some |
| `rmissstr` | **1.13** | 0.75 | little |
| `findstr` | 1.01 | 0.71 | little |
| `rhitstr` | 1.04 | **0.63** | nothing: one lookup, one dependent chain |

The order of the map rows is the mechanism, not noise: the workload that can overlap its hashes loses least, the one that cannot loses most. The prediction that `buildstr` might *win* was wrong in sign and right in rank: a build is only about half rehashing, and the other half is inserts that each pay the hash's latency in full.

Counters, one hasher per binary, 30M all-hits lookups: AES executes **fewer instructions** (5.15G against 5.35G) and takes **59% more cycles** (6.54G against 4.12G), IPC 1.30 down to 0.79. That is a dependency chain, not extra work. Not port contention with the SSE2 group compare: building both with `ANKERL_UNORDERED_DENSE_HAS_SSE2=0` leaves `rhitstr` at 0.66 instead of 0.63. Not a missed inline: `aeshash::hash` appears in no symbol table, and the 36 `aesenc` instructions in the binary are all inline. Not collisions: the hash avalanches indistinguishably from wyhash at every length tested.

**What does not add up, and is worth knowing before quoting the chart's right panel.** AES's standalone latency disadvantage is 4.2 ns per hash, and it costs **14.9 ns per lookup**, three and a half times what adding the two would predict. So the latency panel of `hash_vs_length.svg` *understates* what a map pays. In that loop the next key's loads still issue while the current chain runs; in a lookup the chain's result is the address of the next dependent load, and nothing after it can start. Read that panel as an ordering, not as a number to add to a lookup.

The general rule: **a hash for a map is chosen on latency; a hash for a loop is chosen on throughput; and the two can order candidates oppositely by more than 2x** (AES against this wyhash: 1.28 one way, 0.63 the other, same machine, same day). It depends on whether the caller's code can have several hashes in flight: the map's rehash can, which is why it hashes sixteen ahead, and a lookup cannot.

### Six more hashes tried, none adopted, and the pair of columns says why

*2026-09-09 · no issue · rejected · `scripts/ab/hash_others.{cpp,sh}`, ports in `scripts/ab/hash_ports.h`; median of three processes*

Asked as "give these hashes a try as well": rapidhashNano, foldhash, komihash, polymur-hash, AquaHash and gxhash. On latency nothing that passes avalanche beats the shipped hash; on throughput it is fifth of twelve. All six are in `scripts/ab/hash_others.{cpp,sh}`, so the table re-runs. The two Rust crates are ported in `scripts/ab/hash_ports.h` and checked against a `cargo run` of the real thing: **gxhash reproduces the three `is_stable` vectors from its own test module**, and **foldhash is bit-identical to the crate** at eleven lengths from 0 to 300 bytes for both variants.

**Strict avalanche first, because it decides which rows are even candidates.** Worst and mean `|P(output bit flips) - 1/2|` over every (input bit, output bit) pair, 12000 samples, noise floor 0.014. Seven of the nine are clean at every length: udm5, rapidhashNano, foldhash-*quality*, komihash, polymur, AquaHash and gxhash all read 0.015 to 0.022. Two are not:

| key bytes | `absl::Hash` | foldhash-fast |
|---|---|---|
| 4, 8, 12 | 0.495 to 0.500 | 0.497 to 0.500 |
| 16 | 0.078 | 0.074 |
| 17 | 0.067 | **0.500** |
| 48 | 0.076 | 0.179 |
| 128 | 0.079 | 0.072 |

foldhash says so itself: the fast variant "is optimized purely for speed in hash tables and has known statistical imperfections", and `foldhash::quality` is the same hash with one more folded multiply, which is exactly what repairs it.

**Then the two time columns, and they order the field oppositely.** Median of three processes, the scored key mix (8 to 135 bytes skewed short), latency net of the chain floor, relative to this hash:

| hash | latency | throughput | avalanche |
|---|---|---|---|
| foldhash-fast | **0.98x** | 0.87x | fails |
| **udm5** | **1.00x** | 1.00x | clean |
| `absl::Hash` | 1.01x | 1.03x | fails |
| udm4 (4.11.0) | 1.14x | 1.08x | clean |
| foldhash-quality | 1.15x | **0.98x** | clean |
| rapidhashNano | 1.22x | **0.95x** | clean |
| komihash | 1.37x | 1.38x | clean |
| AquaHash | 1.55x | 1.14x | clean |
| gxhash | 1.67x | **0.76x** | clean |
| `boost::hash` | 1.68x | 2.17x | -- |
| polymur-hash | 1.95x | 3.53x | clean |
| `folly::hasher` | 2.98x | 4.12x | -- |

**On latency, which is what a map lookup pays, nothing clean is faster than what unordered_dense already ships.** The one hash ahead is foldhash-fast, by 2%, bought the way abseil buys it: one folded multiply where this hash has two. **On throughput, which is what a hashing loop pays, this hash is fifth of twelve**, and the two AES designs land where the earlier gxhash port said they would. gxhash is the fastest of all twelve in a loop (0.76x) and fourth from last on latency (1.67x) (corrected 2026-10-02: said "third from last"). AquaHash is 2.2x ahead of this hash at 256 bytes in throughput (3.33 ns against 7.24) and 1.55x behind on latency. Two AES hashes, measured independently, say what one of them said in July, and both need `-maes`, which a header cannot assume.

**rapidhashNano ties this hash exactly at 8 and 16 bytes** (4.12 and 4.11 ns against 4.11 and 4.11) and loses from 32 up (5.27 against 4.52; 10.71 against 8.02 at 256). The short end is the same code, since the two-overlapping-reads trick came from rapidhash; the long end measures the independent-block change against the design it was derived from.

**polymur-hash is not slow by accident.** It is the only hash here with a provable universality bound, a Carter-Wegman construction over a Mersenne prime field, and 1.95x latency is what that costs: a different goal, priced.

**And a throughput number taken through a function pointer is not a throughput number.** The first version of this harness dispatched every hash through a `uint64_t (*)(void const*, size_t)`, which prices the call rather than the hash and hurts a big function most: gxhash read **1.33x** that way and **0.76x** inlined into the loop, as `hash_others.cpp` has always done it. The latency column barely moved (every ratio within 0.03 of the inlined one, on all nine hashes), because a call is a constant added to a chain. The two harnesses agree on latency and disagree on throughput by 1.75x, and only one measures what a caller sees.

### Why `absl::Hash` is faster below 32 bytes, and what taking it would cost

*2026-09-09 · no issue · rejected · avalanche at 20000 samples; in the map one map per binary, `map<std::string, size_t>`, 20M lookups, three rounds*

Asked as "figure out why abseil's hash is faster up to 32 byte, can we learn something from them". `absl::Hash` has lower latency up to 32 bytes and higher above (numbers in [The hash measured against the ones the other libraries ship](#the-hash-measured-against-the-ones-the-other-libraries-ship)), and it pays for that with avalanche. The idea transfers to the block range with one rotate, and in the map that is worth nothing.

The mechanism is one number: **abseil is one 128-bit multiply deep at every length up to 32, where this hash is two.** At 8 bytes and below it is `Mix(state ^ v, kMul)`, one operand a compile-time constant; at 9 to 16 `Mix(state ^ first8, kMul ^ last8)`; at 17 to 32 two *parallel* mixes of overlapping 16 byte ranges xored together. None has a finalizer, because the length is mixed in at the *start*: `PrecombineLengthMix` xors an unaligned load from a 40 byte constant table indexed by `len`, which issues immediately since the caller knows the length. Past 32 bytes it calls an out-of-line function, which is where it gives the latency back.

**What it costs, measured rather than assumed.** Strict avalanche, worst / mean |P(output bit flips) - 1/2| over every (input bit, output bit) pair, 20000 samples, noise floor 0.011:

| key bytes | this hash | `absl::Hash` |
|---|---|---|
| 8 | 0.0149 / 0.0028 | **0.5000** / 0.2557 |
| 12 | 0.0164 / 0.0028 | **0.4981** / 0.0314 |
| 15 | 0.0141 / 0.0028 | **0.4923** / 0.0041 |
| 16 | 0.0139 / 0.0028 | 0.0710 / 0.0030 |
| 17 to 64 | 0.014 to 0.017 | 0.064 to 0.081 |

A worst of 0.50 is a deterministic pair. At 8 bytes the fold is `hi ^ lo` of `x * K`; flipping x's top bit changes `hi` by `K >> 1` plus a carry, which cannot reach bit 63, while `lo`'s bit 63 always flips. Measured directly, **flipping input bit 63 flips output bit 63 in 1,000,000 of 1,000,000 cases**: a linear relation in the bits this map uses for the group. abseil's header says as much in its own way, worrying only about a zero operand.

**The idea does transfer to the block range, and the earlier test of it above is wrong.** [The last structural lever on the hash](#the-last-structural-lever-on-the-hash-taken-and-found-to-weigh-nothing) says `x * C` and `x ^ rotl(x, 32)` sit at the noise floor "at every boundary length from 17 to 300". Boundary lengths are exactly the lengths that cannot see the failure. Swept over *every* length from 17 to 144, the single-multiply block range fails at **len % 16 == 2** and nowhere else: worst 0.055 to 0.073 at 18, 34, 50, 66, 82, 98, 114 and 130, against 0.009 to 0.012 for the shipped hash, on two seeds at 6000 and 40000 samples. The tail block is then a two byte shift of the last front block, and one fold cannot separate two near-duplicates.

**One rotate repairs it exactly.** Rotating the tail block's product by 27 before the xor breaks the shift symmetry: worst 0.0345 / mean 0.0053 over 17 to 144 against the shipped 0.0368 / 0.0052, no bad lengths, no equal-content length collisions. The length then needs no finalizer and no abseil table: a plain `^ len` into the first block's second operand measures the same as the table lookup and is two instructions cheaper. Standalone that shape is **1.14x at 17 to 32 bytes, 1.12x on the scored mix net of the harness's chain**, and identical below 17 bytes, where nothing changed.

**And in the map it is worth nothing, again.** Patched header against shipped: **+1.75 instructions per lookup** at every size and both outcomes, and cycles inside the noise on all four of hit and miss at 32,000 and 200,000. Measured with independent lookups and again with the next key drawn from the previous answer so nothing overlaps; the dependent version does not favour it either. Two cycles off a chain of 180 is 1%, which this instrument cannot resolve and a user cannot feel. Not adopted, now for quality rather than an unresolved tie: the shipped finalizer is worth keeping for `hash<std::string>` outside the map.

**What cannot be taken at all is the short path.** Below 12 bytes the two 8 byte reads overlap by five bytes or more, and at exactly 8 they are the same word, so one product has correlated operands. Six one-multiply shapes were tried: one mul alone, plus `x * C`, plus `x ^ rotl(x, 32)`, two parallel muls with swapped operands, the same with a rotate, and abseil's constant-operand form. Every one reads 0.16 to 0.50 worst at 8 to 11 bytes. From 12 bytes up one multiply plus `x ^ rotl(x, 32)` is clean, but 12 to 16 bytes is only 8.6% of the scored key mix, so there is nothing to chase.

With [Four attempts at tuning the hash for latency, all worthless in the map](#four-attempts-at-tuning-the-hash-for-latency-all-worthless-in-the-map-and-the-microbenchmark-that-said-otherwise-was-wrong), the picture is complete enough to stop. The serial chain is two multiplies, and removing one is invisible. A length-branch structure that removes mispredictions costs more than it saves. A hash that is faster in every hashing loop (AES) loses 37% in the map (see [Latency is what a map pays, tested rather than argued](#latency-is-what-a-map-pays-tested-rather-than-argued)). **The hash is done; what a string lookup still pays is in the probe and the key compare, not in hashing.** If the string lookup is the target, the next thing to measure is where its ~90 cycles go.

**Open (2026-10-02):** this entry gives a string lookup as a chain of 180 cycles and as ~90 cycles, and neither is derived here. The nearest measurement, `maps_one.sh` at 32000 on 2026-09-10, reads 131.7 cycles per independent string hit and 100.0 per miss. Not re-measured.

### The hash measured against the ones the other libraries ship

*2026-09-09 · no issue · info · `scripts/ab/hash_others.{cpp,sh}`, all hashes interleaved in one process, `-n 3`*

Asked for the blog post. `hash.cpp` charts this hash against its own older versions; this harness puts it beside `boost::hash`, `absl::Hash` and `folly::hasher` at 8, 16, 32, 64, 128 and 256 bytes and on the scored mix, latency and throughput separately. Latency net of the chain (a row that hashes nothing prices the chain at ~1.5 ns), ns per hash, on the mix: **this 4.8, absl 4.8, 4.11.0 5.4, boost 8.0, folly 14.0**. Throughput on the same keys: 2.22, 2.25, 2.37, 4.48, 8.80. Same ordering, wider margins.

**Open (2026-10-02):** as ratios these throughputs give boost 2.02x and folly 3.96x, against 2.17x and 4.12x in the table of [Six more hashes tried](#six-more-hashes-tried-none-adopted-and-the-pair-of-columns-says-why), from the same harness on the same day. Neither entry says which run it quotes. Not re-measured.

**`absl::Hash` is the one to beat and nothing here says otherwise**: 9 to 22% *lower* latency than this hash at 8, 16 and 32 bytes and 12 to 23% higher at 64, 128 and 256, so on a mix of mostly short keys the two are level. That is the mechanism behind the own-hash control rows in `maps.sh`, which had only been observed from the outside (abseil loses 1-4% to its own hash, boost loses 31% on a string hit), and it says the control measures the hash, not an interaction with the index. `folly::hasher<std::string>` is `SpookyHashV2` and is the slowest here at every length, 2.9x on the mix. It is what `F14FastMap<std::string, V>` uses unless the caller says otherwise.

Against 4.11.0 the independent-block rewrite reads 17% at 32 bytes, 23% at 128, **12% on the mix**, 0% below 17 bytes and at 256 (unchanged code, which is the control), and **-3% at 64 bytes**.

**Three process-level repetitions are what make those digits mean anything, and `-n` does it.** A within-run interval says how well one process resolved its own median, and nothing about what changed between processes, which on a machine that is not idle is the larger of the two. The first numbers here were taken with a browser and a jekyll watcher running and read 0.5 to 1.5% high across the board. With those paused, `-n 3` merging by median per point, `warmup(200)` and `minEpochTime(1ms)`, the three runs agree to **0.9% on every hash cell**. Only the do-nothing floor at 1.5 ns does not, where a 6% spread is timer granularity, not the machine.

**And do not edit a shell script while it is running**: bash re-reads the file at its old byte offset, so a sweep that was already running re-executed its last line and appended a second run's output to the first one's file. That is what a doubled header in a CSV means.

### `hash_bytes` reads a key of compile-time length 8, 12 or 16 as 4 byte words under gcc and clang: a lookup right after writing a 12 byte key field by field goes 146 -> 40 cycles on both compilers, the hash value unchanged, strings untouched, a key already in memory 1.4 cycles more

*2026-09-27 · #311 · kept · Ryzen 9 7950X, clang 22 and gcc 16, one variant per binary; the caller corpus*

The cause is store forwarding. The 8-16 byte path read `r8(p)` and `r8(p + len - 8)`. For a key the caller has just written, a load that spans two of the caller's stores cannot be forwarded from them and waits until both are written to the cache. Stores are written in order, so it holds up the lookups' misses. gcc writes three `int32_t` fields as three 4 byte stores, which the first read already spans; clang writes 8 + 4, which the second read spans. A 4 byte read lies inside one store in both cases. clang fuses `r4(p) | r4(p + 4) << 32` back into one 8 byte read (why the issue's "four 4 byte loads" row helped gcc only), so each word goes through an empty asm that makes its value opaque. The new path is taken only when `__builtin_constant_p(len) && len % 4 == 0`, only under gcc and clang, and only on little-endian. Whether it runs depends on inlining, so both paths must give the same value, and `r4 | r4 << 32 == r8` holds on little-endian only. A unit test compares the two paths for 8, 12 and 16 bytes; swapping the two words of `a` fails it.

A `segmented_map<coord, uint64_t>` of three `int32_t`, the key written field by field and looked up at once, cycles per lookup:

| | clang 64k | clang 1M | gcc 64k | gcc 1M |
|---|---|---|---|---|
| two 8 byte reads (before) | 144.1 | 368.7 | 146.2 | 380.0 |
| 4 byte words (this) | 39.4 | 117.5 | 40.1 | 118.5 |
| the fields hashed as values (`hash_int`) | 35.5 | 104.4 | 35.6 | 107.7 |

Hashing 12 byte keys already in memory, cycles per hash: clang 4.60 -> 6.01, gcc 4.09 -> 5.53 (3 loads, 2 shifts and 2 ors against 2 loads). Strings of 8-16 bytes of run-time length: identical instructions (clang 25.00, gcc 22.00), identical cycles. Reading each word once instead of the middle word twice changed nothing: the asm is not volatile, so the compiler already merges the two reads. The caller corpus, main against this: `struct_key` (#311's loop) 150.3 -> 46.5 and 840.7 -> 242.6 under clang, 150.3 -> 41.6 and 836.3 -> 242.6 under gcc; every other cell within 1.02 (clang) and 1.06 (gcc, the build at 4M, identical instructions). gcc's string loops lost instructions (`string_hit` 119.5 -> 110.5, the MySQL pattern 136.8 -> 131.8) although strings never take the new path: gcc's inliner decides differently about the larger `hash_bytes`, a gain here and not a property of the change.

What this says and does not say: keys of other sizes, keys with fields narrower than 4 bytes (a 2 byte field stored on its own is still spanned) and MSVC keep the 8 byte reads; for those the usage note in `doc/usage.md` recommends hashing the fields as values. One CPU: store forwarding rules differ between microarchitectures, and that a 4 byte load inside an 8 byte store is forwarded was measured on Zen 4 only.

## Memory: huge pages, segmented_map, teardown order

What the page size, the `segmented_map` container and the order of frees at teardown do to speed and memory. 2 MB pages are the largest measured lever left on the score (2.6-3.6%), but they belong to the allocator or the environment, not the map's code. Memory figures follow the same sawtooth as time, so quote them over an octave, never at one size.

**Where it stands** (as of 2026-10-02)

- `segmented_map` segments only the values since 5.0.0; the index still doubles beside itself (a 1.1 GB spike for 100M `uint64_t` pairs). Kept; `reserve()` removes the doubling. See [What `segmented_map` gives up in 5.0.0, found in review](#what-segmented_map-gives-up-in-500-found-in-review)
- Memory per entry must be summarised over an octave: at 64 byte values `unordered_dense` and boost are a wash (118.3 against 118.5), at 8 bytes boost is ahead (32.6 against 29.2). See [Those memory figures are a point on the sawtooth](#those-memory-figures-are-a-point-on-the-sawtooth-and-the-octave-says-something-else).
- The opt-in `huge_page_allocator` (#231) ships with a 2 MB threshold: integer builds 1.5-1.7x from 200000 entries (string builds 1.1-1.3x), churn 1.33x at 800000, nothing at 50000 except the string build. 16 MB segments on it are the fastest build measured for string keys and 64 byte values; for integers the vector on the allocator stays 6-11% ahead. `segmented_map` takes the segment size as a template parameter since #272. See [The opt-in huge page allocator](#the-opt-in-huge-page-allocator-measured-across-the-size-axis).
- The score runs on 4 KB pages; the same binary on 2 MB pages scores 1.0256 (gcc) and 1.0364 (clang). Not the map's to set. See [The score runs on 4 KB pages and ...](#the-score-runs-on-4-kb-pages-and-pays-16-billion-l1-dtlb-misses-for-it).
- `segmented_vector` destroys and frees last to first: warm builds 1.8-2.3x for strings, 2.6-3.2x for owning values, at the price of RSS staying until reuse or `malloc_trim(0)`. See [Tearing a segmented_map down in reverse](#tearing-a-segmented_map-down-in-reverse-glibc-keeps-the-memory-for-the-next-build-instead-of-handing-it-to-the-kernel-warm-builds-18-32x-when-values-own-heap-memory).
- `default_segment_size_bytes` stays 4096. A map of strings iterates 3.3x slower than `map` with it and 1.24x with 256 KB segments (the 2560 byte segments sit between the strings' buffers); for other values the segment size changes nothing; exact-page segments lose everywhere. See [A `segmented_map`'s segment size, apart from the page](#a-segmented_maps-segment-size-apart-from-the-page-a-map-of-strings-iterates-33x-slower-than-map-with-4-kb-segments-and-124x-with-256-kb-because-the-segments-sit-between-the-strings-own-buffers-for-values-without-heap-memory-the-size-changes-nothing-and-exact-page-segments-lose-everywhere).
- A `map_view` over a mapped file does not need hugetlbfs, and is better on it: 1.02-1.16x faster lookups than on the file's 4 KB pages from 4M to 64M entries (2.2-2.4x at 1M), a 4 ms start whether the file was evicted or not, one copy for every process. `mapped_view.h` (#301) defaults to the lazy shared mapping, which is hugetlbfs when the file is there; a 4 KB file not in the page cache wants `mapping::file_populated` (8.7 s against 0.3 s to the first 100000 lookups at 64M). See [A map_view over a mapped file](#a-map_view-over-a-mapped-file-on-hugetlbfs-its-random-hits-are-102-116x-faster-than-on-the-files-4-kb-pages-from-4m-to-64m-entries-and-22-24x-at-1m-it-starts-in-4-ms-and-is-shared-so-hugetlbfs-is-worth-having-and-not-required-a-lazy-4-kb-mapping-of-a-file-not-in-the-page-cache-needs-87-s-for-its-first-100000-lookups-at-64m).
- A `segmented_map` hit costs 2-15 instructions more than `map` (a `find` hit 7-15, a clang `contains` hit 2); only about 5 of clang's `find` hit are avoidable, and fixing them needs a 24-byte iterator. Declined. See [`segmented_map` lookups against `map`, one cell per binary](#segmented_map-lookups-against-map-one-cell-per-binary-a-hit-costs-2-15-instructions-more-and-a-miss-04-24-and-of-that-only-about-5-instructions-of-clangs-find-hit-are-avoidable----clang-splits-the-value-index-a-second-time-for-it-second-where-gcc-reuses-the-probes-split----so-nothing-was-changed).

### What `segmented_map` gives up in 5.0.0, found in review.

*no date (source neighbours are 2026-09-06) · no issue · kept · code review*

The index still doubles beside itself. `IsSegmented` used to segment the bucket array as well, through the `BucketContainer` parameter this branch removed. Now it segments only the values, and the index is one plain contiguous array of 88 byte group blocks for every table (corrected 2026-10-02: said "two plain contiguous arrays"). So a segmented map keeps stable references and smooth *value* growth, and loses the promise that nothing spikes.

At 5.5 bytes per slot the transient is ~16.5 bytes per slot. For a 100M entry `map<uint64_t, uint64_t>` that is a 1.1 GB spike against 1.6 GB of values.

Kept rather than reverted. Segmenting the index means an indirection per group access in the probe, the one path everything else here is spent making short. `reserve()` removes the doubling for a caller who cares. The README now says so plainly. It previously called the spike "small next to the values", which is only true when the value is large.

### Those memory figures are a point on the sawtooth, and the octave says something else

*2026-09-07 · no issue · method · found while measuring Verstable's footprint*

This entry corrects the million-entry memory figures in [The four charts worth keeping, and the two axes that had none](#the-four-charts-worth-keeping-and-the-two-axes-that-had-none) and [That last sentence stopped being true on 2026-09-05](#that-last-sentence-stopped-being-true-on-2026-09-05-building-against-boost-after-the-rehash-fix).

Bytes per entry with a 64 byte value, `unordered_dense` against boost: 87.0 against 143.7 at a million entries, which is the 1.65x in the four-charts table. But **135.4 against 119.7 at 1.2M and 108.4 against 95.8 at 1.5M, where boost is 12% ahead**.

A million is near this map's best point: the value vector's capacity overhangs by 4.9%. It is near boost's worst: 1966079 buckets for a million keys is load 0.51.

Averaged over an octave, the two are a wash at a 64 byte value (118.3 against 118.5 at 200000) and boost is ahead at an 8 byte one (32.6 against 29.2). The dense value vector's doubling overhang is a cost a point measurement can miss entirely.

Every one of these numbers is true; only the octave ones are a summary. The rule stated for time, summarise across an octave, never at a chosen load (see [The size sweep](#the-size-sweep-and-the-measurement-mistake-it-took-three-tries-to-get-right)), applies to memory too. The figures it corrects predate that rule.

### The opt-in huge page allocator, measured across the size axis

*2026-09-12 · #231 · kept · Ryzen 9 7950X, clang 22 and gcc 16, `scripts/ab/huge_pages.{cpp,sh}`, `include/ankerl/huge_page_allocator.h`*

Integer builds gain 1.5-1.7x from 200000 entries (string builds 1.1-1.3x) (corrected 2026-10-02: said "Builds gain 1.5-1.7x"), churn gains 1.33x at 800000, and at 50000 entries only the string build moves.

`huge_page_allocator<T, Threshold = 2 MB>`: a block of at least `Threshold` bytes is `mmap`ed on its own, 2 MB aligned, rounded up to a multiple of 2 MB and `madvise(MADV_HUGEPAGE)`d. Smaller blocks go to `std::allocator`. Given to the map as its allocator, it covers both regions, since the index rebinds the value allocator.

The harness instantiates the score's own workloads (`test/bench/workloads.h`) on the map with each allocator. One cell per process, so the variant is a template instantiation; five interleaved rounds, medians. ns per operation, `std::allocator` / `huge_page_allocator`, ratio above 1 means huge pages are faster:

| clang | 50000 | 200000 | 800000 | 4000000 |
|---|---|---|---|---|
| `build` u64 | 18.27 / 18.55 (0.98) | 20.40 / 13.30 (**1.53**) | 22.07 / 12.78 (**1.73**) | 38.35 / 24.58 (**1.56**) |
| `build` str | 68.49 / 58.45 (**1.17**) | 71.91 / 63.59 (**1.13**) | 85.46 / 68.64 (**1.25**) | 121.82 / 94.45 (**1.29**) |
| `churn` u64 | 10.61 / 10.65 (1.00) | 12.66 / 12.26 (1.03) | 16.13 / 12.12 (**1.33**) | 53.74 / 49.40 (1.09) |
| `churn` str | 33.95 / 34.15 (0.99) | 44.59 / 40.74 (1.09) | 100.67 / 93.91 (1.07) | 156.20 / 146.53 (1.07) |
| `churn` big | -- | 15.08 / 12.40 (**1.22**) | 32.93 / 28.13 (**1.17**) | 58.31 / 53.13 (1.10) |
| `find` u64 | 4.47 / 4.48 (1.00) | 5.49 / 5.45 (1.01) | 6.50 / 6.21 (1.05) | 16.27 / 14.25 (**1.14**) |
| `find` str | 16.65 / 16.66 (1.00) | 19.27 / 18.91 (1.02) | 28.13 / 25.63 (1.10) | 65.97 / 60.81 (1.08) |

gcc at the two headline sizes agrees:

| gcc | 800000 | 4000000 |
|---|---|---|
| `build` u64 | 24.18 / 13.92 (**1.74**) | 43.12 / 27.00 (**1.60**) |
| `build` str | 90.14 / 74.78 (1.21) | 126.88 / 99.81 (1.27) |
| `churn` u64 | 18.35 / 13.84 (**1.33**) | 60.23 / 55.89 (1.08) |
| `churn` str | 106.66 / 99.60 (1.07) | 162.69 / 152.21 (1.07) |
| `find` u64 | 6.10 / 5.80 (1.05) | 15.13 / 13.17 (**1.15**) |
| `find` str | 28.34 / 25.91 (1.09) | 66.14 / 61.38 (1.08) |

**Three things the table says.** First, the size axis is the whole result, as the header comment says. At 50000 entries only the string build moves, because only the string values vector (its capacity, 65536 x 40 bytes = 2.6 MB) reaches a 2 MB block (corrected 2026-10-02: said "2 MB at 50000 x 40 bytes"). The score's 50000-entry churn gained 7% from the glibc route (see [The score runs on 4 KB pages and ...](#the-score-runs-on-4-kb-pages-and-pays-16-billion-l1-dtlb-misses-for-it)) and 0% here. Both are right: a per-block allocator has no neighbour to share a huge page with.

Second, `build` gains far more than the TLB explains. A doubling vector faults in every new block, and on 2 MB pages that is 512 times fewer faults. That is the cost `tame_allocator()` removed from the score, halved again, and for integers it starts at 200000.

Third, the allocator's reach ends at what the map allocates. `perf stat` per operation, L1 misses served by the L2 TLB / page walks, 4 KB then huge:

| workload | 4 KB | huge |
|---|---|---|
| `find` u64 4M | 1.07 / 0.94 | 0.55 / 0.016 |
| `churn` u64 800k | 1.32 / 0.79 | 0.48 / 0.0006 |
| `churn` str 4M | 0.37 / 2.68 | 0.29 / 1.11 |
| `find` str 4M | 0.95 / 1.53 | 0.57 / 0.45 |

The string rows fall less because a string's *body* is `malloc`ed outside the allocator and still lives on 4 KB pages. The environment route covers those; this one cannot.

**The segmented container, with its segment sized for the page** (asked as "would it make sense to size one chunk exactly as one huge page"). Yes, and for a power-of-two element size it is the cleanest fit. A 2 MB segment of 16 byte pairs is 131072 elements, exactly one huge page. It was verified 2 MB aligned at elements 0, 131072 and 262144 of a 300000 entry map, with no rounding to amortize and no copy on growth.

The catch is `num_bits_closest`. It rounds the element count *down* to a power of two so the index stays a shift and a mask. A "2 MB" segment of 40 byte pairs is 32768 elements, 1.28 MB, below the threshold, and gets nothing. 16 MB segments bound the rounding at 2 MB per segment for any element size. The `segmented_map` alias hardcodes 4096 bytes, so the recipe goes through the map's container slot.

ns per operation, clang. Columns in each cell: `std::vector` on `std::allocator` / vector on the allocator / segmented on `std::allocator` / segmented 2 MB on the allocator / segmented 16 MB on the allocator:

| | 200000 | 800000 | 4000000 |
|---|---|---|---|
| `churn` u64 | 11.80 / 11.38 / 13.98 / 13.39 / 13.47 | 16.05 / 12.00 / 18.61 / 14.19 / 14.52 | 53.94 / 49.55 / 65.95 / 61.23 / 61.17 |
| `churn` str | 44.60 / 40.71 / 45.69 / 45.79 / 41.72 | 100.99 / 93.88 / 102.59 / 100.44 / 95.59 | 155.93 / 145.94 / 158.30 / 154.39 / 148.84 |
| `find` u64 | 5.55 / 5.42 / 5.86 / 5.85 / 5.94 | 6.55 / 6.25 / 7.02 / 6.85 / 6.82 | 16.34 / 14.27 / 17.32 / 15.40 / 15.26 |
| `find` str | 19.43 / 19.04 / 19.79 / 19.92 / 19.51 | 28.13 / 25.74 / 28.99 / 28.40 / 26.25 | 66.18 / 60.92 / 67.64 / 65.55 / 62.40 |
| `build` u64 | 20.34 / 13.56 / 19.17 / 15.10 / 15.02 | 22.04 / 13.19 / 21.21 / 15.04 / 14.65 | 39.33 / 24.69 / 39.02 / 26.36 / 26.19 |
| `build` str | 72.57 / 64.14 / 72.44 / 71.67 / 62.32 | 85.64 / 69.17 / 76.44 / 74.02 / **63.28** | 121.14 / 94.78 / 105.77 / 98.23 / **91.31** |
| `churn` big | | 32.98 / 28.10 / 37.65 / 35.74 / 31.74 | |
| `build` big | | 77.09 / 25.50 / 40.51 / 36.26 / **20.49** | |

Three readings:
- The 2 MB segment gets most of the vector's gain for 16 byte pairs (`churn` u64 800k 18.61 to 14.19, 1.31 against the vector's 1.34) and little for 40 and 72 byte ones (1.00-1.08 for strings, 1.05-1.12 for 64 byte values, presumably the index, which is on huge pages either way), as the rounding predicts (corrected 2026-10-02: said "the vector's whole gain ... the same 1.31 the vector reads) and nothing for 40 and 72 byte ones, exactly as the rounding predicts"). 16 MB segments get it for all three.
- The segmented container itself is 14-22% slower than the vector on integer and 64 byte churn, 6-7% on integer lookups and 2-3% on strings (corrected 2026-10-02: said "10-20% slower than the vector on churn and lookups"). This file had not recorded that number before. It is the second dependent load of every value access, which huge pages do not remove: segmented on huge pages lands about where the vector on 4 KB pages was.
- On builds the order inverts. A segmented map neither copies on growth nor faults a new block in, so 16 MB segments on the allocator are the fastest build measured for string keys and 64 byte values, while the vector on the allocator stays 6-11% ahead for integers (corrected 2026-10-02: said "the fastest build measured"): 63.3 against the vector's 69.2 for strings and 20.5 against 25.5 for 64 byte values, 3.8x the plain vector.

Whether `segmented_map` should expose the segment size is an API question, not a measurement one. The recipe needs the container slot today.

**Later (2026-09-12, #272):** `segmented_map` and `segmented_set` take the segment size as a trailing template parameter, `MaxSegmentSizeBytes` (commit eb93569), so the recipe no longer needs the container slot.

**boost::unordered_flat_map on the same allocator**, with the hash `scripts/ab/ab.cpp` gives it. [Two optimizations the charts point at](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet) (2026-09-06) found huge pages worth the same 22% to both maps at one size and one workload; the question was whether that holds. Point measurements, one per cell, five rounds, ns per operation: `unordered_dense` / `unordered_dense` on the allocator / boost / boost on the allocator. **Not octave geomeans**, so the boost-against-`unordered_dense` ratios are the kind CLAUDE.md says must be re-taken before being quoted as a ranking. The allocator-against-itself ratios are what this table is for.

| | 200000 | 800000 | 4000000 |
|---|---|---|---|
| `churn` u64 | 11.96 / 11.41 / 13.33 / 12.68 | 16.32 / 12.22 / 17.89 / 15.96 | 54.30 / 49.58 / 30.55 / 28.54 |
| `churn` str | 44.79 / 40.61 / 54.07 / 49.81 | 101.17 / 94.14 / 109.01 / 104.04 | 156.17 / 146.11 / 111.76 / 105.80 |
| `churn` big | | 33.08 / 28.19 / 49.35 / 30.41 | |
| `find` u64 | 5.56 / 5.40 / 4.76 / 4.79 | 6.56 / 6.24 / 5.61 / 5.36 | 16.34 / 14.19 / 11.54 / 10.45 |
| `find` str | 19.41 / 19.10 / 18.66 / 18.20 | 28.53 / 26.23 / 25.93 / 24.77 | 66.17 / 60.85 / 57.10 / 55.27 |
| `build` u64 | 20.32 / 13.46 / 23.19 / 15.44 | 21.78 / 12.98 / 26.54 / 14.71 | 38.67 / 24.84 / 47.05 / 24.86 |
| `build` str | 72.56 / 63.86 / 86.76 / 80.54 | 86.06 / 68.78 / 153.40 / 139.82 | 124.73 / 95.31 / 275.52 / 229.78 |
| `build` big | | 77.61 / 25.36 / 93.54 / 36.20 | |

Huge pages help both, by amounts that follow each layout:
- boost gains most where its one region holds everything. The 64 byte value churn at 800k goes 49.35 to 30.41 (1.62x), because its values are inline in its bucket array and one huge-paged region gets all of it. `unordered_dense`'s values are a second region, and its churn goes 33.08 to 28.19 (1.17x).
- boost's integer builds gain 1.5-1.9x, `unordered_dense`'s 1.5-1.7x.
- `unordered_dense` gains more on the integer churn at 800k (1.34x against 1.12x): its two regions are two translations per access, boost's one is one.
- The counters on the integer find at 4M agree for both. L1 misses served by the L2 TLB per operation: 1.07 to 0.55 for `unordered_dense`, 1.34 to 0.46 for boost. Page walks: 0.94 to 0.016 and 0.53 to 0.016.

Nothing changes who is ahead where. boost stays ahead on lookups and on the large integer and string churn. `unordered_dense` stays ahead on every build and on churn up to 800k. At 4M the two integer builds land on the same number (24.84 and 24.86) with or without huge pages (corrected 2026-10-02: only *with* huge pages; on 4 KB pages the table reads 38.67 against 47.05, `unordered_dense` 1.22x ahead). That is the "level at 4M" that [The insert path's instructions, counted one by one](#the-insert-paths-instructions-counted-one-by-one-the-clanggcc-gap-is-the-call-boundary-and-pgo-removes-all-of-it) recorded, unmoved by the page size.

**Correction (2026-10-02):** the two "level at 4M" statements measure different things. The insert-path entry's 35.6 against 34.6 is an insert into a *reserved* table; this harness's `build` grows from empty (`workloads::build` does not reserve), and growing is where `unordered_dense` leads. Re-taken with `scripts/ab/huge_pages.sh -k u64 -w build`, five rounds, `AB_CORE=2`, ns per insert, `unordered_dense` / boost (same hash):

| | 4 KB pages | huge pages |
|---|---|---|
| clang, 2M | 29.93 / 43.75 (1.46x) | 15.47 / 22.49 (1.45x) |
| clang, 4M | 38.08 / 45.93 (1.21x) | 23.31 / 24.95 (1.07x) |
| gcc, 2M | 28.79 / 48.24 (1.68x) | 16.18 / 26.84 (1.66x) |
| gcc, 4M | 36.99 / 51.11 (1.38x) | 25.10 / 30.09 (1.20x) |

The 2026-09-12 clang column reproduces to within 3% (38.67 / 47.05 then). So a build from empty is ahead of boost past the cache on both page sizes, by less on huge pages, and a reserved insert is level at 4M. No decision rests on either.

**The threshold is 2 MB, not #231's 4 MB.** `huge4`, the same allocator at a 4 MB threshold, tracks the 2 MB one wherever every block is past both, and loses wherever blocks fall between. `build` u64 at 200000 reads 13.30 against 17.08, because the vector's 2 MiB block (131072 pairs, filled on the way to 200000 and then copied) qualifies at one threshold and not the other; the final 4 MiB block qualifies at both (corrected 2026-10-02: said "because the 3.2 MB values vector qualifies at one threshold and not the other"). There is no size at which 4 MB wins, since a block below 2 MB is never rounded either way.

### The score runs on 4 KB pages and pays 1.6 billion L1 dTLB misses for it

*2026-09-12 · found while closing #268 · info · Ryzen 9 7950X, gcc 16 and clang 22, glibc 2.43, THP mode `madvise`, same binary run twice*

The same score binary on 2 MB pages is 2.6% faster under gcc and 3.6% under clang. Churn and the 50% find move 5-8%; the `iterate` controls do not move.

This overturns a conclusion in [Two optimizations the charts point at](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet) (2026-09-06). That entry measured hits against a warm 200000 entry map, read 7.18 to 7.10 ns, and concluded huge pages live "in the one regime the score cannot see" (it is retracted in place there). That measurement was the one shape of workload that cannot see them. A hit loop is throughput-bound: its lookups are independent, so an L1 dTLB miss served by the L2 TLB overlaps the next lookup's work. `churn` and `insert_erase` are chains (probe, then value, then the erase's second walk), and a translation on a chain is latency.

The geometry is the whole argument. Zen 4's L1 dTLB holds 72 entries, which is 288 KB of 4 KB pages. `iterate` works on about 125 KB and fits. Everything else in the score does not: `insert_erase` peaks near 0.3-0.6 MB, `find_50` and `churn` sit on about 1.2 MB, `build` reaches 4.6 MB. So every random access to the index or the values in those twelve workloads is an L1 dTLB miss served by the L2 TLB, about seven cycles, and a lookup does two of them.

**Zero-code A/B.** glibc 2.43's `GLIBC_TUNABLES=glibc.malloc.hugetlb=1` keeps the heap top 2 MB aligned and `madvise(MADV_HUGEPAGE)`s every extension of at least 2 MB (observed: `madvise(0x11a00000, 8388608, MADV_HUGEPAGE)` on an 8 MB block). `tame_allocator()` already routes every block the score allocates through that heap. So the *same binary* runs twice, alternating, and only the environment differs: no second build, no code layout, no ±3% band. The score process held 24 MB of `AnonHugePages` mid-run.

Score, `bench_quick_overall_udm`, 4 KB over 2 MB, above 1.00 means huge pages are faster:

| round | gcc | clang |
|---|---|---|
| 1 | 1.0218 | 1.0285 |
| 2 | 1.0262 | 1.0454 |
| 3 | 1.0221 | 1.0407 |
| 4 | 1.0379 | 1.0246 |
| 5 | 1.0203 | 1.0429 |
| geomean | **1.0256** | **1.0364** |

Per workload under gcc, ns/op, same ratio:

**Open (2026-10-02):** the geomean of these fifteen ratios is 1.0324, not the score's 1.0256 from the same five rounds; which round or summary the table is from is not recorded. Not re-measured.

| | `uint64_t` | `std::string` | `big_value` |
|---|---|---|---|
| iterate while adding then removing | 0.9996 | 1.0002 | 0.9990 |
| random insert erase | 1.0010 | 1.0226 | 1.0011 |
| build from empty | 1.0430 | 1.0050 | 1.0161 |
| churn at a fixed size | **1.0713** | **1.0700** | **1.0822** |
| 50% probability to find | **1.0559** | **1.0553** | **1.0708** |

The three `iterate` rows are the control and read 1.000 to the third decimal: the only workloads whose working set fits the L1 dTLB are the only ones that do not move. `perf stat` over a whole score pass, gcc: `ls_l1_d_tlb_miss.tlb_reload_4k_l2_hit` **1,595,516,981 to 14,881,142**, page walks (`all_l2_miss`) 6,121,399 to 294,657. Instructions retired: 230.5 G against 222.2 G in the same ~104 G cycles, which is nanobench's time budget doing 3.7% more work.

**What this says and does not say.** It is the largest measured, unclaimed lever left on the score, and it is not in the map's code. The header cannot ask for a page size; an allocator can (#231), and so can a user's environment. It also says two things about the benchmark itself:
- A machine whose THP mode is `always` scores 2.6-3.6% higher than one on `madvise`. None of the nine bench runners records its mode, so cross-runner absolutes were already meaningless, and this is one more reason.
- The paired harness cannot measure it. Both sides share one process, so an environment variable cancels out of the ratio. This needs two whole runs of one binary, which is the method above.

### Tearing a segmented_map down in reverse: glibc keeps the memory for the next build instead of handing it to the kernel, warm builds 1.8-3.2x when values own heap memory

*2026-09-26 · no issue, found through [facontidavide/Bonxai#69](https://github.com/facontidavide/Bonxai/pull/69) · kept · Ryzen 9 7950X, clang 22 and gcc 16, glibc 2.43, THP `madvise`, `scripts/ab/teardown_order.sh`, `scripts/ab/teardown_blocks_forward.patch`*

`segmented_vector` destroyed its elements first to last and freed its blocks first to last. It now does both last to first, the order a built-in array uses. One loop (`destroy_tail`) does it, shared by the destructor, `clear()` and a shrinking `resize()`. `map` and `set` keep `std::vector`, which libstdc++ and MSVC destroy front to back, and are untouched.

**How it was found.** Bonxai's root map (`facontidavide/Bonxai`, `doc/root_map_benchmark.md`) measured `segmented_map` 1.8x slower than `std::unordered_map` to build a 46k-root voxel grid. The reconstruction (1M random voxels, best of 5, `std::unordered_map` swapped for the map through a `RootMap` alias) reproduced 1.48x, and all of it was in the rounds after the first:
- In a fresh process `segmented_map` built in 854 ms against 884, with the same 62k instructions and 10.2 page faults per root.
- In the warm rounds it took 1.8 page faults and 8800 instructions per root more.
- With `glibc.malloc.trim_threshold` and `glibc.malloc.mmap_threshold` pinned, the two warm builds read 203 and 207 ms. So it was the allocator.
- A `free` wrapper that logged every drop of the program break found one trim per teardown. The free of a 104 byte Bonxai `LeafGrid` set it off, and it handed back 400 MB under `segmented_map` and 138 MB under `std::unordered_map`, whose teardown runs in bucket order.
- Emptying the map from the back before destroying it cut the trim to 90 MB and the warm build to 229 ms, ahead of `std::unordered_map`'s 250.

Separately, Bonxai's own `std::hash<CoordT>` (OpenVDB's three-product XOR) gives 29226 distinct values for 46656 root keys and puts every key in one bucket by the top bits. That costs both maps 0.5 extra key compares a hit. It is Bonxai's to fix; a murmur3-finalised pack of the three coordinates has no collisions on any lattice tried.

**The measurement in this repository**: one binary per header and key kind, one process per size. `segmented_map` is built from empty, destroyed and built again seven times; the median of three passes, alternated with main's header. Keys are the workloads' (`workloads::key_source`). `owned` is a `uint64_t` key whose value owns a 96 byte allocation, Bonxai's shape reduced to the map. Times in ms. "released" is what the main arena shrank by at teardown (`mallinfo2().arena`). C is the change (elements and blocks last first); B has only the elements reversed:

| compiler | kind | n | first build main / C | warm build main / C / B | teardown main / C / B | released MB main / C |
|---|---|---|---|---|---|---|
| clang | u64 | 50000 | 1.16 / 0.95 | 0.81 / **0.67** / 0.81 | 0.05 / **0.03** / 0.05 | 1.0 / 0.6 |
| clang | u64 | 200000 | 4.65 / 4.36 | 3.31 / **3.32** / 3.35 | 0.15 / **0.15** / 0.15 | 2.9 / 2.9 |
| clang | u64 | 1000000 | 31.14 / 30.98 | 24.68 / **24.37** / 24.59 | 1.43 / **1.35** / 1.41 | 23.2 / 21.9 |
| clang | str | 50000 | 3.23 / 3.05 | 2.75 / **1.26** / 1.25 | 0.68 / **0.44** / 0.44 | 4.6 / 0.0 |
| clang | str | 200000 | 13.67 / 13.79 | 12.45 / **5.60** / 5.58 | 3.10 / **1.92** / 1.96 | 20.0 / 0.0 |
| clang | str | 1000000 | 85.87 / 84.59 | 79.74 / **43.70** / 43.36 | 20.45 / **13.13** / 13.14 | 105.8 / 0.0 |
| clang | owned | 50000 | 3.39 / 3.04 | 2.27 / **0.88** / 0.87 | 0.74 / **0.51** / 0.52 | 4.4 / 0.0 |
| clang | owned | 200000 | 13.62 / 13.79 | 12.16 / **4.22** / 4.15 | 3.59 / **2.19** / 2.18 | 23.7 / 0.0 |
| clang | owned | 1000000 | 78.66 / 78.13 | 73.94 / **28.17** / 28.12 | 21.54 / **12.22** / 12.24 | 131.2 / 0.0 |
| gcc | u64 | 50000 | 0.87 / 0.82 | 0.67 / **0.53** / 0.67 | 0.05 / **0.03** / 0.05 | 1.0 / 0.6 |
| gcc | u64 | 200000 | 3.90 / 3.69 | 2.64 / **2.64** / 2.62 | 0.15 / **0.15** / 0.15 | 2.9 / 2.9 |
| gcc | u64 | 1000000 | 26.93 / 26.38 | 20.28 / **19.81** / 20.25 | 1.44 / **1.36** / 1.41 | 23.2 / 21.9 |
| gcc | str | 50000 | 3.21 / 3.00 | 2.73 / **1.24** / 1.24 | 0.68 / **0.46** / 0.44 | 4.6 / 0.0 |
| gcc | str | 200000 | 13.54 / 13.62 | 12.33 / **5.46** / 5.46 | 3.06 / **1.90** / 1.94 | 20.0 / 0.0 |
| gcc | str | 1000000 | 85.30 / 84.64 | 79.13 / **42.56** / 43.05 | 20.73 / **13.25** / 13.25 | 105.8 / 0.0 |
| gcc | owned | 50000 | 3.29 / 2.94 | 2.19 / **0.78** / 0.80 | 0.76 / **0.51** / 0.51 | 4.4 / 0.0 |
| gcc | owned | 200000 | 13.43 / 13.21 | 11.73 / **3.62** / 3.62 | 3.54 / **2.19** / 2.10 | 23.7 / 0.0 |
| gcc | owned | 1000000 | 76.53 / 76.01 | 71.55 / **24.66** / 25.13 | 21.53 / **11.91** / 11.82 | 131.2 / 0.0 |

Warm builds are 1.8-2.3x faster for string keys and 2.6-3.2x for owning values; teardown is 1.5-1.8x faster for both; integers are level from 200000 up. The teardown is faster because main's included the kernel taking back 20-131 MB per round. Reversing the elements is nearly all of it. Reversing the blocks as well is what moves a small integer map (0.81 to 0.67 at 50000 under clang, 0.67 to 0.53 under gcc), and it costs nothing anywhere, so C ships.

The first build runs before any teardown, so the order cannot touch it. At 200000 and a million it agrees within 2%, except `u64` at 200000, where C reads 5.7-6.7% faster (corrected 2026-10-02: said "At 200000 and a million it agrees within 2%"). At 50000, where it is a millisecond, C reads 5-18% faster in every cell under both compilers, which this measurement does not explain.

**Correction (2026-09-28):** #313: it does not reproduce. The same two headers re-measured read 0.4-3.0% apart at 50000, inside the harness's own band, and main's side is the one that moved (clang u64 0.96, not 1.16). The first build column of the table above is not evidence either way; see [The teardown harness ran its sides in a fixed order](#the-teardown-harness-ran-its-sides-in-a-fixed-order-and-the-side-that-runs-first-in-a-pass-reads-2-4-slower-on-the-string-first-build-the-5-18-first-build-gap-at-50000-that-309-could-not-explain-does-not-reproduce-with-the-same-two-headers-04-30-so-the-harness-now-rotates-its-sides).

`resize()` to a smaller size and `clear()` go through the same backward loop as the destructor. Bonxai's harness with the change: 46k roots 342 to 230 ms against `std::unordered_map`'s 248, 787k roots 297 to 246 against 452, reads unchanged.

**The price, measured**: resident memory after destroying a million-entry `segmented_map<std::string, uint64_t>`, above the keys it was built from:

| | built | after teardown | after `malloc_trim(0)` |
|---|---|---|---|
| main | 114 MB | 0 | |
| C | 114 | **114** | 0 |
| `std::unordered_map` | 137 | 137 | 0 |

The memory is in malloc's free lists, and the next allocation of the process reuses it. That is the whole speedup, but a caller watching RSS after a teardown sees it stay. `std::unordered_map` does the same. `map` returns it all, since its values are one `std::vector` block.

What this says and does not say: glibc 2.43 only. jemalloc, tcmalloc and mimalloc were not run, and their trim policies differ. The mechanism is the order of frees against glibc's heap trim. The trim fires on a free whose coalesced chunk reaches 64 KB and returns whatever lies above the last live chunk. Freed last first, the small chunks near the top of the heap go to tcache and the fastbins first and are not coalesced into the top, so the later large frees find no top to trim. That explanation is read from the counters and the trim log, not from glibc's source. Nothing here touches the score, which has no segmented workload.

### `segmented_map` lookups against `map`, one cell per binary: a hit costs 2-15 instructions more and a miss 0.4-2.4, and of that only about 5 instructions of clang's `find` hit are avoidable -- clang splits the value index a second time for `it->second`, where gcc reuses the probe's split -- so nothing was changed

*2026-09-28 · #312 · rejected · Ryzen 9 7950X, clang 22 and gcc 16; `scripts/ab/find_segmented.sh` builds `scripts/ab/find_segmented.cpp` once per map, operation, mode, key type and size, and counts user-space instructions and cycles around the loop with `perf_event_open`; 2^23 lookups in random order, `find` followed by `it->second`, or `contains`*

Instructions per lookup, `segmented_map` minus `map`:

| | clang `find` | clang `contains` | gcc `find` | gcc `contains` |
|---|---|---|---|---|
| hit, `uint64_t` | +11.1 | +2.1 | +7.1 | +9.1 |
| hit, `std::string` | +9.2 | +2.2 | +15.1 | +5.1 |
| miss, either key | +1.2 | +1.2 | +1.3 | +0.4 to +2.4 |

The heading said "a hit costs 7-15 instructions more" until 2026-10-02 (corrected: clang's `contains` hits cost 2.1-2.2).

Absolute counts:

| 50000 entries, instr/op | `map` | `segmented_map` |
|---|---|---|
| clang `find` hit u64 / string | 43.84 / 127.85 | 54.93 / 137.05 |
| clang `contains` hit u64 / string | 44.87 / 127.85 | 46.96 / 130.05 |
| gcc `find` hit u64 / string | 38.92 / 103.80 | 45.97 / 118.87 |
| gcc `contains` hit u64 / string | 37.92 / 103.80 | 46.97 / 108.94 |

The counts at 1M entries differ from these by at most 3.8 instructions, and the gaps by at most 2.0. In cache, cycles follow the instructions (clang `find` hit u64 27.9 against 31.1). At 1M, `segmented_map` over `map` in cycles reads 0.95 (clang `contains` string hit) to 1.25 (gcc `contains` u64 hit), one run each, so read no ranking from the 1M cycles.

Where clang's 11 go, read from the disassembly of the `uint64_t` `find` hit (53 instructions against 42 per hit):
- About 4 in the key comparison: the probe reaches the stored key through the block pointer (shift, load, mask, scale) where `map` scales once. This is what segmenting is.
- About 5 in `it->second`: the iterator is a block-array pointer and an index, so dereferencing it splits the index again. The probe did the same split two instructions earlier, and clang does not reuse it. gcc does: its found path loads the value from the address it compared the key at.
- About 3 in register pressure: a spilled pointer reloaded every iteration and two extra moves.
- Not the `end()` comparison, which is one `cmp` in both, and not the element multiply, a shift for the 16 byte pair.

What this says and does not say: the avoidable part is clang only, `find` hits only, about 5 instructions of 55 on an in-cache integer table, and nothing for `contains`, misses or gcc. Removing it needs the iterator to carry the element pointer: 16 -> 24 bytes, a block-boundary test in `++`, and `it + n` or `end()` must never read a block that is not allocated. That changes what every iteration over a `segmented_map` runs, for a gain one compiler's `find` hit sees. Declined on that trade without building it. The harness is there if the Bonxai read (see [Tearing a segmented_map down in reverse](#tearing-a-segmented_map-down-in-reverse-glibc-keeps-the-memory-for-the-next-build-instead-of-handing-it-to-the-kernel-warm-builds-18-32x-when-values-own-heap-memory)) or another caller makes the clang hit matter. The Bonxai read was not re-run, since nothing changed.

### A `segmented_map`'s segment size, apart from the page: a map of strings iterates 3.3x slower than `map` with 4 KB segments and 1.24x with 256 KB because the segments sit between the strings' own buffers; for values without heap memory the size changes nothing, and exact-page segments lose everywhere

*2026-10-02 · #350 · kept · Ryzen 9 7950X, clang 22, THP `madvise`, `AB_CORE=2`; `scripts/ab/segment_size.sh` (one binary per variant, `bench_readme`'s workloads through `maps.h`, octave geomean at four bases, one round), `scripts/ab/segment_exact_page.patch`, `perf stat`*

The #349 run read string iteration at 3.47x `map` for `segmented_map` with 4 KB segments and 1.03x for `huge_page::segmented_map` with 16 MB segments, which changes the segment size and the page at once. This separates them: power-of-two segments from 4 KB to 16 MB on `std::allocator` and on `huge_page_allocator`, and `exact4096`, one page-aligned 4096 byte allocation per segment holding `4096 / sizeof(T)` elements, indexed by a division. Elements of 16 (`uint64_t` pair), 40 (`std::string` pair) and 72 bytes (64 byte value). Ratios to `map` on the same allocator (`huge-*` rows: to `map` on `std::allocator`), lower is better.

Iteration, by base size (octave geomean):

| | str 50k | str 200k | str 1M | str 4M | u64 1M | u64 4M | 72 B 1M | 72 B 4M |
|---|---|---|---|---|---|---|---|---|
| `map`, ns/element | 0.286 | 0.330 | 0.807 | 0.776 | 0.277 | 0.380 | 1.49 | 1.47 |
| 4 KB segments | 1.69 | 2.06 | 3.06 | 3.33 | 1.29 | 1.25 | 1.17 | 1.16 |
| 16 KB | 1.50 | 1.66 | 2.40 | 2.80 | 1.30 | 1.24 | 1.13 | 1.13 |
| 64 KB | 1.27 | 1.33 | 1.62 | 1.70 | 1.27 | 1.24 | 1.13 | 1.12 |
| 256 KB | 1.15 | 1.11 | 1.22 | 1.24 | 1.31 | 1.20 | 1.12 | 1.12 |
| 2 MB | 1.13 | 1.11 | 1.08 | 1.09 | 1.28 | 1.22 | 1.11 | 1.10 |
| 16 MB | 1.12 | 1.06 | 1.06 | 1.06 | 1.32 | 1.25 | 1.10 | 1.11 |
| exact page | 1.86 | 2.06 | 3.04 | 3.33 | 3.17 | 3.09 | 2.37 | 2.49 |
| 4 KB on `huge_page_allocator` | 1.72 | 2.09 | 3.13 | 3.33 | 1.33 | 1.22 | 1.17 | 1.17 |
| 16 MB on `huge_page_allocator` | 1.13 | 1.05 | 1.01 | 1.05 | 1.16 | 1.01 | 0.95 | 0.91 |

**For strings it is the segment size and not the page**: 4 KB segments on the huge page allocator read the same as on `std::allocator`, and 16 MB segments on 4 KB pages read 1.06. **For values that own no heap memory the segment size changes nothing**: 1.20-1.32x for `uint64_t` and 1.10-1.17x for 64 byte values at every size from 4 KB to 16 MB. That remainder is the segmented iterator's own index arithmetic; only 2 MB pages under 2 MB+ segments take part of it back.

The mechanism, `perf stat` over 3G element visits at 1M strings (the run includes the fills, so the differences are what counts): 4 KB segments cost 24.6G cycles more than `map`, +8 per element, with +1 instruction per element, +44M dTLB load misses (0.015 per element, at most ~1G cycles of it) and +0.27G L1 misses. 256 KB and 16 MB segments read the same as `map` on all of them. So it is neither instructions nor the TLB. What differs between the key types is the allocation pattern: a 40 byte pair rounds to 64 elements, 2560 bytes per segment, and each segment is a `malloc` between the strings' own buffers, so the walk restarts cold every 64 elements; a `uint64_t` map's 4096 byte segments are consecutive `malloc`s with nothing between them. This last step is an inference from the counters and the contrast, not measured directly (no prefetcher counter was read).

Everything else, at 1M and 4M:

| | build + destroy | find | churn | peak RSS |
|---|---|---|---|---|
| str, 4 KB / 256 KB / 16 MB | 0.58-0.67 / 0.51-0.57 / 0.51-0.57 | 0.99-1.01 for all | 1.01-1.03 for all | 0.90-0.94 for all |
| u64, 4 KB / 256 KB / 16 MB | 0.57-0.83 / 0.56-0.83 / 0.63-0.86 | 1.03-1.07 for all | 0.94-1.04 for all | 0.58-0.71 / 0.60-0.71 / 0.75-0.78 |
| 72 B, 4 KB / 256 KB / 16 MB | 0.61-0.66 / 0.51-0.60 / 0.50-0.55 | 1.02-1.07 for all | 0.97-1.04 for all | 0.53-0.72 / 0.53-0.71 / 0.58-0.74 |
| exact page, str / u64 / 72 B | 0.63-0.69 / 0.68-1.10 / 0.98-1.04 | 1.01-1.19 | 0.97-1.05 | 0.90-0.94 / 0.90-1.09 / 1.00-1.34 |

**Exact-page segments are the worst segmented variant, or tied with the worst, on every workload and every element size**, and the idea that motivated them, one segment per page, buys nothing measurable: string iteration reads the same as 4 KB power-of-two segments, `uint64_t` iteration 3.1x because the index is now a division, and the page-aligned allocations cost glibc up to a page of padding each (RSS 0.90-1.09 of `map` for `uint64_t`, against 0.58-0.71). At 50000 entries a `uint64_t` map's RSS rises from 0.64 of `map` with 4 KB segments to 0.83 with 2 MB and 16 MB: a non-empty map holds whole segments.

What this says and does not say: one round per cell and one compiler; the drift between neighbouring segment sizes is 1-3%, smaller than every effect quoted. The string grid ran against the header before #299 and the other two after it; `segmented_vector` is the same in both and every ratio is within one grid. Decision (owner, 2026-10-02): `default_segment_size_bytes` stays 4096, because a larger default costs every small segmented map a whole segment (a 1-entry map holds 4 KB today), and `doc/usage.md` and the header comment tell a map of strings to pass 256 KB. Exact-page segments closed with these numbers.

### A map_view over a mapped file: on hugetlbfs its random hits are 1.02-1.16x faster than on the file's 4 KB pages from 4M to 64M entries and 2.2-2.4x at 1M, it starts in 4 ms and is shared, so hugetlbfs is worth having and not required; a lazy 4 KB mapping of a file not in the page cache needs 8.7 s for its first 100000 lookups at 64M

*2026-10-02 · #301 · kept · Ryzen 9 7950X, clang 22 and gcc 16, THP `madvise`, `nr_hugepages` 0, btrfs on NVMe, `AB_CORE=2`; `scripts/ab/mapped_view.sh` (one binary per mode, median of 7 rounds of 1M hits, `perf_event_open` around the loop)*

`map<uint64_t, uint64_t>` written to a file with both arrays at 2 MB boundaries (30 MB at 1M entries, 1684 MB at 64M), then read back five ways. **No hugetlbfs row was measured directly**: the machine has no huge pages reserved and the user is not root, so the 2 MB row is the file's bytes copied into an anonymous, 2 MB-aligned `MADV_HUGEPAGE` mapping, the proxy the issue named. `AnonHugePages` read mid-run equals the whole region at every size (30, 108, 424, 1684 MB) and page walks per lookup are 0.000-0.002, so the mechanism was engaged. `MADV_COLLAPSE` on the file mapping returns `EINVAL` (no `CONFIG_READ_ONLY_THP_FOR_FS`, btrfs): file pages here are 4 KB, and `FilePmdMapped` is 0 everywhere. `MAP_HUGETLB` fails for want of reserved pages.

**Warm lookups**, ns per random hit (throughput: the key comes from an rng, several are in flight), clang / gcc:

| | 1M | 4M | 16M | 64M |
|---|---|---|---|---|
| owning `map`, `std::allocator` | 11.97 / 9.51 | 33.33 / 28.79 | 38.43 / 32.75 | 39.89 / 34.18 |
| owning `huge_page::map` | 6.03 / 4.83 | 30.43 / 26.58 | 34.64 / 29.60 | 35.26 / 30.42 |
| view, file mapping (4 KB) | 8.22 / 8.30 | 32.55 / 28.34 | 38.42 / 32.68 | 39.99 / 34.32 |
| view, file mapping, `MAP_POPULATE` | 7.97 / 8.54 | 32.64 / 28.72 | 38.34 / 32.73 | 40.03 / 34.11 |
| view, 2 MB copy (hugetlbfs proxy) | 5.95 / 4.87 | 30.34 / 27.53 | 34.86 / 30.26 | 35.56 / 30.91 |
| file / 2 MB | 1.38 / 1.70 | 1.07 / 1.03 | 1.10 / 1.08 | 1.12 / 1.11 |

Per lookup, clang, cycles / L1 dTLB misses / page walks (`ls_l1_d_tlb_miss.all_l2_miss`): owning 64.6 / 2.01 / 1.09 at 1M and 216.5 / 2.04 / 2.02 at 64M; file view 43.7 / 1.94 / 0.004 at 1M and 217.1 / 2.02 / 1.98 at 64M; 2 MB copy 31.4 / 0.00 / 0.00 at 1M and 192.6 / 1.85 / 0.00 at 64M. Two misses per hit is the block and the value, each on its own page.

In this first sweep the 2 MB row was the `MADV_HUGEPAGE` proxy: a view on it looked up 1.03-1.13x faster than one on the file's 4 KB pages from 4M to 64M entries and 1.38x (clang) to 1.70x (gcc) at 1M, within 3.6% of the owning map on `huge_page::map`, and the 4 KB view matched the owning map on `std::allocator` within 2.4% from 4M up. The owner then reserved huge pages and mounted hugetlbfs, and the real thing is measured further down this entry, under "On hugetlbfs itself".

The 1M column has an oddity: the owning map on `std::allocator` is 1.46x (clang) and 1.15x (gcc) *slower* than the 4 KB file view, with a page walk per lookup where the file view has almost none (1.09 against 0.004), at the same 30 MB and the same number of L1 misses. The L2 TLB holds 3072 4 KB entries, 12 MB, so the file mapping is covered by something the heap is not. A plausible cause is the page cache's large folios, which give physically contiguous runs that Zen 4 coalesces into one TLB entry; it was not measured (`tlb_reload_coalesced_page_hit` was not read). Unexplained, and from 4M up the two agree. **Later (2026-10-02, the same day):** measured, and it is the page cache's history, below in this entry.

**Cold**, ms from the start of the load until the first 100000 random hits are done, clang, one run per cell (gcc's run of the same cells differs by up to 27% on cells under 60 ms and by at most 5.2% on the rest; that spread is the noise of a single cold run). "Evicted" is after `POSIX_FADV_DONTNEED` on the file:

| | 1M | 4M | 16M | 64M |
|---|---|---|---|---|
| owning `map`, file cached: load + lookups | 13.8 + 2.7 | 60.3 + 3.7 | 249 + 4.2 | 1007 + 5.7 |
| owning `map`, evicted | 29.7 + 2.7 | 84.1 + 3.7 | 308 + 4.3 | 1222 + 5.9 |
| owning `huge_page::map`, file cached | 6.9 + 1.8 | 29.9 + 3.5 | 120 + 3.6 | 493 + 3.9 |
| view, file mapping, cached | 0.01 + 1.9 | 0.01 + 4.3 | 0.01 + 7.0 | 0.01 + 16.7 |
| view, file mapping, evicted | 0.01 + 17.3 | 0.01 + 87.3 | 0.02 + 233 | 0.02 + **8616** |
| view, `MAP_POPULATE`, cached | 1.3 + 1.8 | 4.6 + 3.5 | 20.0 + 3.9 | 97.7 + 5.2 |
| view, `MAP_POPULATE`, evicted | 7.6 + 1.9 | 31.8 + 3.5 | 88.1 + 4.1 | 307 + 5.0 |
| view, 2 MB copy, cached | 3.1 + 1.2 | 9.6 + 3.1 | 33.2 + 3.7 | 129.8 + 3.7 |
| view, 2 MB copy, evicted | 7.6 + 1.7 | 36.5 + 3.3 | 95.6 + 3.6 | 335 + 3.7 |

The evicted lazy mapping at 64M took 26577 major and 16981 minor faults for its 100000 lookups: one synchronous read per touched page, at random, about 0.3 ms each. At 16M the same mapping took 155 major faults, because readahead around each fault brings in most of a 424 MB file. With the file cached, a fresh lazy mapping is the fastest way to start serving at every size (1.9-16.7 ms against 16.5-1013 ms for the owning load).

**Shared**, two processes, each maps the file `MAP_SHARED` (or reads it, for the owning rows) and looks up every key, MB (identical for both processes and both compilers to 1 MB):

| | 1M: RSS / file / PSS | 64M: RSS / file / PSS | page cache, 64M |
|---|---|---|---|
| owning `map` | 28 / 1 / 26 | 1682 / 1 / 1680 | 1682 |
| view, file mapping | 27 / 27 / 13 | 1682 / 1682 / 840 | 1684 |
| view, `MAP_POPULATE` | 31 / 31 / 15 | 1685 / 1685 / 842 | 1684 |
| view, 2 MB copy | 31 / 1 / 30 | 1685 / 1 / 1684 | 1684 |

"One copy" holds for the file mapping: each process's PSS is half its RSS, and the page cache holds the file once. The owning map and the 2 MB copy are two private copies plus the page cache's (the page cache is reclaimable; the copies are not).

**What shipped.** `include/ankerl/mapped_view.h`: `mapped_file` (open, map, own), `mapped_layout` (offsets and counts; the caller frames the file) and `mapped_view<View>`, which owns a `mapped_file` and the view over it. `mapping::file` (lazy, `MAP_SHARED`) is the default, from the shared and the cached-cold tables; `mapping::file_populated` (`MAP_POPULATE`) is there for the evicted row; `mapping::huge_copy` (`MAP_HUGETLB`, else `MADV_HUGEPAGE`) for a process that looks up a lot and does not share. A file on hugetlbfs gets 2 MB pages through `mapping::file` with nothing more to ask.

**Re-measured through `mapped_view.h` itself**, the same day after the review asked whether the harness's own `mmap` stands for the header: four modes that construct `ud::mapped_view` (`header_file`, `header_populated`, `header_huge`, and `header_file_checked` = `mapping::file` with `trust::checked`), in one sweep with four of the modes above, both compilers, the machine idle. ns per random hit, clang / gcc:

| | 1M | 4M | 16M | 64M |
|---|---|---|---|---|
| owning `map` | 11.67 / 10.05 | 32.77 / 28.34 | 38.08 / 32.68 | 39.71 / 33.96 |
| harness file mapping | 9.25 / 9.96 | 32.27 / 28.75 | 38.12 / 33.20 | 39.79 / 34.56 |
| `mapping::file` | 9.22 / 9.46 | 31.98 / 27.82 | 38.10 / 32.63 | 39.74 / 34.12 |
| `mapping::file_populated` | 9.21 / 9.42 | 32.51 / 27.88 | 38.01 / 32.64 | 39.70 / 34.19 |
| harness 2 MB copy | 5.43 / 4.77 | 29.82 / 26.42 | 34.51 / 29.89 | 35.33 / 30.65 |
| `mapping::huge_copy` | 5.44 / 4.58 | 29.80 / 25.85 | 34.51 / 29.68 | 35.34 / 30.37 |
| `mapping::file` / `huge_copy` | 1.69 / 2.07 | 1.07 / 1.08 | 1.10 / 1.10 | 1.12 / 1.12 |

The header reads as the harness's own mappings do: within 1% under clang, within 5.0% under gcc (the 1M cells; 3.3% at 4M, under 2% above), in both directions, and the same cold and shared. So the first table stands for the shipped code.

**What `trust::checked` costs a lazy `file` mapping**, ms until 100000 lookups are done, clang: with the file cached 2.6 / 7.7 / 21.3 / 72.8 at 1M / 4M / 16M / 64M against 1.7 / 4.2 / 7.2 / 16.8 unchecked; with it evicted 15.3 / 65.7 / 191 / 684 against 17.4 / 84.7 / 247 / **8601** unchecked and 9.9 / 37.1 / 101 / 312 for `mapping::file_populated` (one run per cell; gcc's run agrees within 5% except the evicted checked cell at 4M, 49.0). The check reads the index front to back, which readahead serves in large reads, so it pages in the index far faster than random lookups would; the values are still faulted in one page at a time. Checked costs a cached file 4.3x at 64M and saves an evicted one 12.6x.

**The 1M anomaly is the page cache's history.** The file view's page walks per lookup at 1M read 0.000 in clang's half of this sweep and 0.505 in gcc's, the same file, the same code. `perf stat` on the clang `header_file` binary, 1M, 3 rounds: with the page cache as the sweep left it (filled by the random faults of the evicted runs) 0.69 walks per lookup, 0.99M coalesced reloads against 3.16M 4 KB ones; after evicting it and reading the file once with `cat`, 0.000 walks, 6.59M coalesced reloads against 16K 4 KB ones. A sequential read fills the cache with large folios, physically contiguous runs the L2 TLB holds as one coalesced entry; random faults fill it with small ones. At 1M that is the difference between the whole file fitting the L2 TLB and not: 10.6 against 11.4 ns in those two `perf stat` runs. The owning map's heap gets no such runs (1.09 walks either way). From 4M up the file is past what coalescing can cover and the walks are the same either way. (The `perf stat` events sharing the process with the harness's own three counters may have multiplexed; the ratios, not the counts, are the evidence.)

**On hugetlbfs itself.** The owner reserved 2048 huge pages (`vm.nr_hugepages`) and mounted a hugetlbfs owned by the user (`mount -t hugetlbfs -o uid=...,pagesize=2M none /mnt/huge`), and a third sweep ran with the file copied there (`header_hugetlbfs`: `mapping::file` over it; hugetlbfs has no `write()`, so the copy goes through a mapping, before the clock) and with `huge_copy` now getting `MAP_HUGETLB` (`hugetlb_MB` in `smaps_rollup`, 0 page walks). ns per random hit, clang / gcc, one sweep:

| | 1M | 4M | 16M | 64M |
|---|---|---|---|---|
| owning `map` | 12.97 / 10.06 | 34.76 / 28.72 | 38.11 / 33.13 | 40.07 / 34.36 |
| owning `huge_page::map` | 6.29 / 4.63 | 31.86 / 26.28 | 34.78 / 30.20 | 35.21 / 31.22 |
| `mapping::file`, 4 KB file | 13.84 / 10.93 | 33.38 / 30.05 | 38.59 / 34.14 | 40.00 / 35.35 |
| `mapping::file`, file on hugetlbfs | 6.20 / 4.55 | 32.69 / 26.24 | 34.88 / 30.04 | 35.68 / 30.53 |
| `mapping::huge_copy` (`MAP_HUGETLB`) | 6.11 / 4.77 | 32.00 / 27.10 | 35.17 / 33.14 | 35.45 / 31.02 |
| `MADV_HUGEPAGE` copy (the proxy) | 5.67 / 4.95 | 32.24 / 26.88 | 34.63 / 30.72 | 35.68 / 30.94 |
| 4 KB file / hugetlbfs | 2.23 / 2.40 | 1.02 / 1.15 | 1.11 / 1.14 | 1.12 / 1.16 |

**Hugetlbfs against 4 KB: a `map_view` over a file on hugetlbfs looks up 1.02-1.16x faster than over the same file on 4 KB pages from 4M to 64M entries, and 2.2-2.4x at 1M; it matches the owning map on `huge_page::map` within 3% from 4M up, and the proxy stood for it within 2.4% there (9% at 1M).** gcc's `huge_copy` at 16M (33.14) is the one cell off its neighbours; it was not re-run. The 1M 4 KB cells are the page cache's history again (13.84 here against 9.22 in the second sweep, with 0.002 walks per lookup; the L1 misses were not compared).

Cold, ms until 100000 lookups are done, clang / gcc: the file on hugetlbfs 0.67 / 1.58, 3.13 / 2.72, 3.76 / 3.29, 4.28 / 3.82 at 1M / 4M / 16M / 64M, the same after `POSIX_FADV_DONTNEED` (hugetlbfs pages are never evicted), against 16.95 / 16.74 cached and 8715 / 8679 evicted for the 4 KB file at 64M. Shared: two processes on the hugetlbfs file took 0 reserved pages between them beyond the file's own (`HugePages_Free` before and while both held it); two on `huge_copy` took 60, 216, 848 and 3368 MB, two copies.

So hugetlbfs is the best of every column here: the speed of the 2 MB copy, a start as fast as a mapping of a cached file, one copy for all processes. It needs root once (reserve the pages, mount), the pages are pinned whether used or not, and the file must be written through a mapping. `mapping::file` gets all of it without asking when the file is there, which is why it stays the default.

What this says and does not say: hits only, throughput; misses and dependent chains were not run. One machine, one filesystem: btrfs with whatever folio sizes its page cache uses here; another filesystem may coalesce differently, and the 1M file view depends on how the file entered the page cache (above). The first sweep's modes do their own `mmap` (`MAP_PRIVATE` outside the shared phase, at a 2 MB-aligned address); the `header_*` modes of the later sweeps go through `mapped_view.h` and read the same, as shown above. The first two sweeps' 2 MB rows are a proxy; the third measures hugetlbfs itself, with 2048 pages reserved on a 62 GB machine. The evicted numbers are NVMe; a spinning disk would make the lazy row far worse. The sweep ended at 16:47:17; the owner started a game on the machine shortly after, and the gcc half agrees with the clang half in the direction of every ratio, so it is taken as undisturbed.

## Small tables

Tables that fit in L1 or L2: counting into a table of a few thousand keys, maps of 1 to 32 entries, and hits in tables of 1000 to 16000 entries. Against 4.x, `unordered_dense`'s group probe loses only where 4.x's robin hood probe never walks: dense ids 0..n-1 and a table whose first slot almost always hits. A small-map mode without an index would make tiny maps 1.5-3.7x faster to look up but costs every other map the empty-table test #329 removed.

**Where it stands** (as of 2026-09-28)

- Counting into a small table (#331): clang is 9-10% behind 4.1.2 with #329 in (11-12% before it), about one cycle per row of group-compare latency; #329 closes it under gcc and not under clang; nothing tried closes the rest. Open. See [#331, counting into a small table under clang ...](#331-counting-into-a-small-table-under-clang-11-12-behind-412-about-one-cycle-per-row-of-latency-in-the-16-slot-groups-compare-on-a-table-whose-first-slot-almost-always-hits-329s-sentinel-takes-01-03-cycles-of-it-under-clang-and-all-of-it-under-gcc-and-nothing-tried-closes-the-rest).
- Small maps (#304): neither a two-group minimum index nor no-index-below-eight was kept for the default map. An opt-in small mode is worth building only for a caller with a measured need. See [Small maps, two variants from #304 prototyped and ...](#small-maps-two-variants-from-304-prototyped-and-measured-against-main-over-20000-maps-of-1-to-32-entries-no-index-below-eight-entries-makes-a-map-of-up-to-four-entries-106-23x-faster-to-build-12-22x-faster-to-destroy-and-its-lookups-15-37x-faster-but-puts-back-the-empty-table-test-329-took-out-of-every-lookup-a-two-group-minimum-index-helps-a-one--or-two-entry-integer-map-2-28-and-builds-32-entries-117-161x-slower-neither-was-kept).
- Hits in cache-resident tables (#346): main takes 0.41-0.61 of 4.5.0's time on scrambled integer keys and 0.71-0.79 on strings, and 1.02-1.43x on dense ids. Dense ids are what #347 has to win back; `bench_small_hits_udm` stays out of the score. See [Hits in cache-resident tables (1000 to 16000 entries)](#hits-in-cache-resident-tables-1000-to-16000-entries-450-against-main-346-main-takes-041-061-of-450s-time-on-scrambled-integer-keys-and-071-079-on-strings-from-a-quiet-loop-and-a-busy-one-and-102-143x-of-it-on-dense-ids-0n-1-which-a-multiplicative-hash-places-without-collisions----450s-best-case-and-the-keys-redpandas-and-osrms-callers-have).

### #331, counting into a small table under clang 11-12% behind 4.1.2: about one cycle per row of latency in the 16-slot group's compare, on a table whose first slot almost always hits; #329's sentinel takes 0.1-0.3 cycles of it under clang and all of it under gcc, and nothing tried closes the rest

*2026-09-27 · #331, #329, #315 · open · Ryzen 9 7950X, clang 22 and gcc 16; ClickHouse's counting loop, `++map[*current]` over the real AdvEngineID and CounterID columns, 100M rows, `absl::Hash` linked from the benchmark's own abseil build, the loop alone timed, best of five, one variant per binary*

The #315 numbers include the benchmark's file loading. Timing the loop alone makes the gap larger in relative terms. A stand-in hash (a multiply fold, not marked avalanching) did not reproduce the gap: main came out 1.46x ahead on AdvEngineID. So the hash is part of the picture, and `absl::Hash` had to be linked for real.

Cycles (instructions) per row:

| | 4.1.2 | main | main, no `empty()` test (#329) | main, fingerprint by arithmetic | main, pre-broadcast 16 byte table |
|---|---|---|---|---|---|
| clang AdvEngineID (19 keys) | **9.08** (36.1) | 10.08 (39.0) | 10.00 (38.0) | 10.94 (42.0) | 10.73 (40.0) |
| clang CounterID (6506 keys) | **9.67** (43.4) | 10.88 (44.3) | 10.55 (42.2) | 11.99 (48.7) | 12.06 (48.5) |
| gcc AdvEngineID | 10.57 (50.9) | 9.52 (40.0) | **8.95** (36.0) | | |
| gcc CounterID | 9.27 (47.6) | 9.72 (41.7) | **9.20** (37.8) | | |

**Open (2026-10-02):** the #329 entry's counting table is a second run the same evening, and it reads 4.1.2 under clang at 9.02 and 9.35 where this one reads 9.08 and 9.67, up to 3.4% apart; with its figures the clang gap after #329 is 11-13%, with these 9-10%. Not re-measured.

The profile of the loop under clang puts main's cycles on the key compare's branch after the group compare, the vector broadcast (`pshufd`) and the index load. main executes 1-3 more instructions per row than 4.1.2 and takes about 1 cycle more, so it is latency and not work. 4.x compared one 8 byte bucket (fingerprint and distance, with the value index beside it) and went to the key. 5.x broadcasts the fingerprint, compares sixteen lanes, extracts a mask, finds its first bit and loads the index. That is the group design's price on a table that fits in L1 and hits its first slot. This is an inference from these numbers, not a measured latency chain.

The two ways around the broadcast both lost:
- Computing the fingerprint instead of the table lookup. The table was measured right in the first place, in [The gcc string-lookup gap, explained and mostly closed](#the-gcc-string-lookup-gap-explained-and-mostly-closed): integer misses 1.05-1.06x.
- A 4 KB table of fingerprints already broadcast to 16 bytes. It keeps the scalar table load for the counter and the stored fingerprint as well, and ends with more instructions.

What this says and does not say: two columns of one benchmark, in cache. The gap is clang's; under gcc main is level or ahead once #329 lands. No change of its own; #329 carries the part that can be had.

### Small maps, two variants from #304 prototyped and measured against `main` over 20000 maps of 1 to 32 entries: no index below eight entries makes a map of up to four entries 1.06-2.3x faster to build, 1.2-2.2x faster to destroy and its lookups 1.5-3.7x faster, but puts back the empty-table test #329 took out of every lookup; a two-group minimum index helps a one- or two-entry integer map 2-28% and builds 32 entries 1.17-1.61x slower; neither was kept

*2026-09-28 · #304, #329 · rejected · Ryzen 9 7950X, clang 22 and gcc 16; `scripts/ab/small_maps.cpp`, `small_maps.sh` and `small_maps_variants.py`, which writes the five headers compared; five rounds alternated between the variants, medians*

The harness builds 20000 maps of one size, runs `count` on each map's own keys and as many it does not hold (50% hits, and it checks the hits), then destroys them. Each phase is timed and reported in ns per map, after an untimed pass over every size that faults the heap in. The variants:
- `two`: the smallest index is two groups (176 bytes) instead of four (352). One group needs `hash >> 64`, which is undefined, or a mask on every lookup.
- `A8`, `A16`: no index for the first 8 or 16 values. An insert scans them with `KeyEqual` and appends; value N+1 allocates the index and places all of them. `find` tests for "no index" before hashing and scans.
- `B8`: the same insert, and a `find` that hashes, probes the sentinel and scans only on a miss, so a hit in a large map pays nothing.

They are prototypes of build, `count` and destroy only. The switch to an index hashes the inserted key twice, which is what the 12- to 32-entry cells of A and B pay for; a real version would not.

Ratios to `main`, build / lookup / destroy, below 1 is faster:

| clang++, entries | `main` ns build / lookup / destroy | two groups | A8 | A16 | B8 |
|---|---|---|---|---|---|
| u64 1 | 27.5 / 9.2 / 23.1 | 0.88 / 0.79 / 0.93 | 0.44 / 0.67 / 0.48 | 0.43 / 0.55 / 0.47 | 0.45 / 0.70 / 0.48 |
| u64 2 | 45.2 / 11.9 / 22.4 | 0.91 / 0.76 / 0.96 | 0.62 / 0.61 / 0.50 | 0.59 / 0.55 / 0.50 | 0.62 / 1.01 / 0.50 |
| u64 4 | 70.1 / 23.7 / 22.0 | 0.96 / 0.93 / 0.97 | 0.70 / 0.68 / 0.50 | 0.69 / 0.65 / 0.50 | 0.73 / 1.14 / 0.50 |
| u64 8 | 108.9 / 42.6 / 23.0 | 0.98 / 0.92 / 0.93 | 0.78 / 1.35 / 0.49 | 0.77 / 1.31 / 0.48 | 0.82 / 1.48 / 0.49 |
| u64 12 | 158.1 / 56.3 / 37.4 | 1.02 / 0.96 / 1.02 | 1.99 / 1.22 / 1.87 | 0.86 / 1.94 / 0.63 | 2.00 / 1.04 / 1.75 |
| u64 16 | 211.5 / 59.4 / 36.4 | 1.05 / 1.06 / 0.96 | 1.73 / 1.16 / 1.68 | 0.93 / 3.09 / 0.62 | 1.74 / 1.09 / 1.66 |
| u64 32 | 404.5 / 125.5 / 59.6 | 1.61 / 1.03 / 1.19 | 1.39 / 1.16 / 1.39 | 1.52 / 1.16 / 1.53 | 1.40 / 1.09 / 1.43 |
| string 1 | 99.8 / 60.7 / 34.5 | 0.96 / 0.90 / 0.97 | 0.79 / 0.44 / 0.64 | 0.76 / 0.43 / 0.63 | 0.79 / 0.97 / 0.64 |
| string 2 | 226.2 / 168.1 / 43.1 | 0.96 / 0.98 / 1.02 | 0.86 / 0.29 / 0.73 | 0.84 / 0.28 / 0.74 | 0.86 / 1.00 / 0.74 |
| string 4 | 288.3 / 224.8 / 82.5 | 1.03 / 1.00 / 1.02 | 0.92 / 0.62 / 0.84 | 0.92 / 0.61 / 0.84 | 0.92 / 1.18 / 0.85 |
| string 8 | 535.0 / 326.0 / 143.7 | 0.99 / 0.97 / 1.00 | 0.93 / 0.82 / 0.90 | 0.93 / 0.81 / 0.90 | 0.93 / 1.48 / 0.93 |
| string 12 | 841.8 / 399.8 / 214.5 | 0.97 / 0.97 / 1.00 | 1.27 / 1.04 / 1.14 | 0.98 / 1.11 / 0.92 | 1.29 / 1.05 / 1.14 |
| string 16 | 1017.8 / 515.2 / 259.9 | 1.00 / 0.96 / 1.01 | 1.23 / 0.98 / 1.09 | 1.03 / 1.25 / 0.94 | 1.23 / 1.12 / 1.12 |
| string 32 | 1811.4 / 660.0 / 590.7 | 1.18 / 1.11 / 1.24 | 1.12 / 1.03 / 0.88 | 1.23 / 1.04 / 1.10 | 1.13 / 1.08 / 0.91 |

| g++, entries | `main` ns build / lookup / destroy | two groups | A8 | A16 | B8 |
|---|---|---|---|---|---|
| u64 1 | 25.8 / 8.9 / 23.5 | 0.88 / 0.78 / 0.91 | 0.47 / 0.57 / 0.46 | 0.47 / 0.48 / 0.47 | 0.48 / 0.75 / 0.46 |
| u64 2 | 43.9 / 12.7 / 22.4 | 0.91 / 0.72 / 0.98 | 0.65 / 0.45 / 0.48 | 0.64 / 0.43 / 0.50 | 0.66 / 1.05 / 0.48 |
| u64 4 | 68.8 / 22.1 / 21.6 | 0.93 / 0.94 / 1.00 | 0.74 / 0.57 / 0.50 | 0.74 / 0.57 / 0.53 | 0.75 / 1.19 / 0.50 |
| u64 8 | 108.8 / 40.0 / 23.0 | 0.97 / 0.93 / 0.93 | 0.81 / 1.00 / 0.49 | 0.81 / 0.93 / 0.50 | 0.82 / 1.60 / 0.47 |
| u64 12 | 158.3 / 54.8 / 39.6 | 1.01 / 0.94 / 0.98 | 1.98 / 0.93 / 1.64 | 0.90 / 1.46 / 0.60 | 2.01 / 0.97 / 1.65 |
| u64 16 | 210.5 / 55.0 / 36.4 | 1.05 / 1.06 / 0.98 | 1.74 / 0.99 / 1.64 | 0.97 / 2.81 / 0.63 | 1.76 / 1.04 / 1.74 |
| u64 32 | 405.7 / 117.5 / 63.0 | 1.60 / 1.03 / 1.13 | 1.38 / 0.99 / 1.32 | 1.58 / 0.99 / 1.34 | 1.39 / 1.03 / 1.38 |
| string 1 | 97.6 / 56.8 / 35.7 | 0.95 / 0.92 / 0.95 | 0.83 / 0.46 / 0.63 | 0.85 / 0.49 / 0.62 | 0.82 / 0.91 / 0.62 |
| string 2 | 221.8 / 156.9 / 44.7 | 0.98 / 0.98 / 0.99 | 0.87 / 0.27 / 0.71 | 0.89 / 0.29 / 0.73 | 0.89 / 1.02 / 0.72 |
| string 4 | 281.6 / 213.4 / 83.5 | 0.94 / 0.96 / 1.00 | 0.94 / 0.56 / 0.83 | 0.93 / 0.55 / 0.82 | 0.93 / 1.03 / 0.85 |
| string 8 | 526.1 / 297.1 / 143.0 | 0.96 / 1.00 / 1.00 | 0.93 / 0.83 / 0.91 | 0.93 / 0.86 / 0.91 | 0.94 / 1.25 / 0.93 |
| string 12 | 810.4 / 365.5 / 212.9 | 1.01 / 1.00 / 0.99 | 1.31 / 1.01 / 1.16 | 1.03 / 1.07 / 0.93 | 1.30 / 1.00 / 1.18 |
| string 16 | 986.6 / 450.7 / 261.4 | 1.00 / 1.01 / 1.00 | 1.24 / 1.01 / 1.09 | 1.04 / 1.31 / 0.95 | 1.29 / 1.02 / 1.11 |
| string 32 | 1789.2 / 593.2 / 581.3 | 1.17 / 1.11 / 1.26 | 1.12 / 1.02 / 0.91 | 1.22 / 1.01 / 1.11 | 1.12 / 1.02 / 0.92 |

Index bytes per map: `main` 352 from the first entry; `two` 176 up to 25 entries, then 352; A8 0 up to 8 entries.

What this says and does not say:
- `two` is 2-28% faster for one or two integer entries (less to allocate and zero), within 8% of `main` from 4 to 16 entries, and 1.17 (string) to 1.61 (integer) times slower to build 32 entries: every size from 26 to 51 now grows once more. Not a win.
- B is out: a small map hashes and then scans, so its lookups are slower than `main`'s from two entries on.
- A is the one with a real gain: up to four entries, build 0.44-0.94, destroy 0.46-0.84, lookups 0.27-0.68 (a string key is never hashed). At eight integer entries its lookups are 1.35x under clang, because a miss scans all eight, so N would be about 4.
- Its price is on every other map: `find` has to test for "no index" before it hashes. That is the test #329 removed, measured there at 2-5% of `find`'s instructions for integer and string keys (0.964 / 0.970 clang, 0.952 / 0.981 gcc for u64 / string) (corrected 2026-10-02: said "3-5% of `find`'s instructions"). That price was not re-measured on the prototype. The score, which has no small maps, can only show the cost.
- A full version touches every path the issue lists (about fifteen), each needing tests across the transition both ways.

Declined for the default map on that trade: the large-map lookup keeps #329's gain. An opt-in small mode (a template option or its own alias) would not have the price, and is worth building only for a caller with many tiny maps and a measured need.

### Hits in cache-resident tables (1000 to 16000 entries), 4.5.0 against main (#346): main takes 0.41-0.61 of 4.5.0's time on scrambled integer keys and 0.71-0.79 on strings, from a quiet loop and a busy one, and 1.02-1.43x of it on dense ids 0..n-1, which a multiplicative hash places without collisions -- 4.5.0's best case, and the keys Redpanda's and OSRM's callers have

*2026-09-28 · #346 · info · Ryzen 9 7950X, clang 22 and gcc 16; `scripts/ab/small_hits.sh 5 v4.5.0 origin/main` over `scripts/ab/small_hits.cpp`, one header per binary, five rounds with the binaries rotated, medians; each size is five tables across its octave, geomean*

The loops are in `test/bench/workloads.h`. `bench_small_hits_udm` (in `quick_overall_map.cpp`, not in the score) runs them for the current header:
- `find_all<true>` (quiet);
- `find_hits_busy`, a loop shaped like #317's caller: a 64-bit division picks the key, and the found value goes into a random slot of a 4 MB array;
- the same two over `dense_id_table`, whose keys are 0..n-1 unscrambled.

| main / 4.5.0, ns per hit (5.x ns / 4.5.0 ns) | clang quiet | clang busy | gcc quiet | gcc busy |
|---|---|---|---|---|
| `uint64_t`, scrambled, 1000 | 0.442 (3.65 / 8.28) | 0.522 (5.21 / 9.97) | 0.444 (3.64 / 8.19) | 0.413 (4.90 / 11.88) |
| `uint64_t`, scrambled, 4000 | 0.469 (4.35 / 9.27) | 0.518 (5.80 / 11.20) | 0.473 (4.31 / 9.12) | 0.418 (5.47 / 13.08) |
| `uint64_t`, scrambled, 16000 | 0.513 (5.23 / 10.20) | 0.606 (7.63 / 12.59) | 0.516 (5.18 / 10.04) | 0.487 (7.14 / 14.66) |
| `uint64_t`, dense ids 0..n-1, 1000 | 1.431 (2.48 / 1.73) | 1.347 (3.49 / 2.59) | 1.151 (2.16 / 1.88) | 1.289 (3.40 / 2.63) |
| `uint64_t`, dense ids 0..n-1, 4000 | 1.347 (2.71 / 2.02) | 1.299 (3.60 / 2.77) | 1.116 (2.37 / 2.13) | 1.226 (3.52 / 2.87) |
| `uint64_t`, dense ids 0..n-1, 16000 | 1.231 (3.16 / 2.57) | 1.207 (4.56 / 3.78) | 1.018 (2.78 / 2.73) | 1.138 (4.39 / 3.86) |
| `std::string`, 1000 | 0.713 (18.40 / 25.79) | 0.750 (21.34 / 28.45) | 0.721 (17.80 / 24.68) | 0.761 (22.95 / 30.14) |
| `std::string`, 4000 | 0.729 (19.90 / 27.30) | 0.759 (23.30 / 30.69) | 0.740 (19.32 / 26.10) | 0.762 (24.77 / 32.49) |
| `std::string`, 16000 | 0.763 (25.14 / 32.93) | 0.781 (28.60 / 36.64) | 0.789 (24.85 / 31.50) | 0.782 (29.82 / 38.14) |

Why, from `perf stat` on the quiet integer loop at 1000 entries: 4.5.0 executes fewer instructions (45.1 against 56.3 per lookup) but mispredicts 0.44 branches per lookup against main's 0.01. That is the whole of 28.6 against 16.2 cycles. A robin hood probe walks until the distance says stop, and with random keys where it stops is not predictable. The group probe answers with one SIMD compare.

**Open (2026-10-02):** for the same loop at 1000 scrambled entries, this paragraph says 0.44 mispredicts and 28.6 against 16.2 cycles, and the table below says 0.61 and 28.5 against 12.3; not re-measured.

Same loop, by key kind, cycles per lookup:

| keys | entries | 4.5.0 mispredicts | 4.5.0 cycles | main cycles |
|---|---|---|---|---|
| 0..n-1 | 1000 / 16000 / 92160 | 0.00-0.04 | 7.5 / 12.2 / 23.2 | 12.0 / 15.7 / 27.8 |
| scrambled | 1000 / 16000 / 92160 | 0.61 / 0.56 / 1.08 | 28.5 / 31.8 / 63.7 | 12.3 / 17.6 / 29.6 |

main is within 0.3-2 cycles of itself on either kind of key.

What this says and does not say:
- The earlier finding that main's hit costs more than 4.x's in real callers (#317, #319, #341) holds for dense integer ids and not in general. Redpanda's raft group ids are 0..n-1 in its benchmark, #341's bare loop used 0..92159, and OSRM's node ids are dense indices (inferred from OSRM's data model, not checked in its code). For any other integer keys, and for strings, main's hit is 1.3-2.4x faster than 4.5.0's in tables of this size.
- Dense ids are what #347 has to win back; `bench_small_hits_udm`'s dense-id rows are its benchmark.
- Decision on the score: these loops stay out of it. The score scrambles integer keys on purpose (`key_source`), and a score that included dense ids would reward a probe tuned to one key pattern; the scrambled hit loops would add a case main already wins by 2x. Any change for #347 reports `bench_small_hits_udm` beside the score instead.
- In cache only (up to 16000 entries); #341's size sweep covers larger tables on dense ids.

## Other maps

This section compares `unordered_dense` against other hash maps: boost's `unordered_flat_map`, abseil, folly F14, emhash, emilib, indivi, Verstable, ihtab/ixhtab and `std::unordered_map`. Two things matter most. First, boost finds faster on hits because it has no value index, while `unordered_dense` wins iteration against every map that is not dense, the string build-and-destroy and most misses. Second, how a comparison is taken decides its sign: a single table size, a shared hash, or a counting layer linked into a timed binary has each produced a wrong ranking here, and the corrections are kept in place.

**Where it stands** (as of 2026-09-28)

- Boost's `unordered_flat_map` finds faster on hits because it needs no value index: its first access is about 1 byte of metadata per slot against 5.5 here. From 46080 to 460800 entries `unordered_dense` takes 1.7-2.1x boost's L3 fills per find; in L2 the difference is codegen alone. It is the price of fast iteration. See [Why boost's `unordered_flat_map` finds faster (#341)](#why-boosts-unordered_flat_map-finds-faster-341-it-needs-no-value-index-so-the-first-thing-a-lookup-touches-is-1-byte-of-metadata-per-slot-against-this-maps-55-which-leaves-l2-at-a-far-smaller-table-from-46080-entries-to-460800-this-map-takes-17-21x-boosts-l3-fills-per-find-and-14-35-more-cycles-under-clang-093-104-under-gcc-while-in-l2-it-is-compiler-codegen-alone-this-maps-cycles-083-088-of-boosts-under-gcc-122-125-under-clang).
- At a million entries, each map in its own default configuration, against 5.2.0 (#349): integer iteration 5.4x to 112x ahead of every map that is not dense (emhash8 and F14Vector, also dense, are level), string build-and-destroy 2.1x to 2.6x ahead of every flat map, integer find 37% and churn 69% behind boost (0.73 and 0.59), and `absl` builds integers at 0.82. See [Every configuration of 5.2.0, twelve rows in the README run (#349)](#every-configuration-of-520-twelve-rows-in-the-readme-run-349-pmr-is-free-group_big-costs-16-34-on-the-integer-build-find-churn-and-memory-below-232-elements-segmented_map-builds-19x-faster-at-058x-the-peak-memory-and-iterates-strings-338x-slower-with-its-default-4-kb-segment-and-huge-pages-are-the-largest-gain-on-every-integer-timing-panel) and [Seventeen maps in their own default configurations](#seventeen-maps-in-their-own-default-configurations-at-a-million-entries-the-dense-layout-wins-iteration-by-55x-to-110x-and-the-string-build-and-destroy-by-21x-to-26x-and-loses-the-integer-find-and-churn-to-boost-by-37-and-64).
- Handing every map this project's wyhash flatters boost on strings. With its own hash, boost takes 1.04-1.22x `unordered_dense`'s time on string lookups; on integers boost leads with either hash. See [The same-hash convention was flattering boost on every string chart](#the-same-hash-convention-was-flattering-boost-on-every-string-chart-and-the-control-found-it).
- On a miss the overflow counter is worth 1.4-1.7x against an otherwise identical SwissTable (abseil 1.38 on a miss, 0.73 on a hit). Chained designs lose the miss badly (emhash8 2.13, Verstable 2.10). See [Every other map on the same workloads, in one harness](#every-other-map-on-the-same-workloads-in-one-harness) and [Verstable measured rather than read](#verstable-measured-rather-than-read).
- Building is no longer the weakness against boost after the rehash fix (build 1.62 clang / 1.77 gcc) in the score's sizes with a shared hash. At a million entries with each map's own hash, boost builds integers level (0.97-0.99) and abseil faster (0.81-0.82), see the second bullet above. The churn lead over boost (1.16/1.21) is a single-size artefact: over an octave boost is 1.15-1.29x ahead on churn. See [That last sentence stopped being true on 2026-09-05](#that-last-sentence-stopped-being-true-on-2026-09-05-building-against-boost-after-the-rehash-fix) and [Every boost comparison in this file that predates 2026-09-07 was taken at one size](#every-boost-comparison-in-this-file-that-predates-2026-09-07-was-taken-at-one-size-and-five-of-them-cross-100-when-the-octave-is-averaged-instead).
- Memory against boost is a sawtooth: the 27.0 MB vs 32.0 and 113.5 vs 205.5 figures at a million are one point. Over an octave boost is ahead with an 8 byte value (32.6 against 29.2) and the two are a wash at 64 bytes. Tombstone maps grow under turnover. Node maps win at a 64 byte value. See [Every other map on the same workloads, in one harness](#every-other-map-on-the-same-workloads-in-one-harness) and [Those memory figures are a point on the sawtooth](#those-memory-figures-are-a-point-on-the-sawtooth-and-the-octave-says-something-else).
- The only boost idea taken was the MSVC prefetch (unmeasured). Per-architecture prefetch tuning is open on ARM only; x86 was measured and has nothing to tune that is right for both compilers. A built-in probe-statistics facility is open only as a user feature: `scripts/ab/probe_length.sh` measures probe lengths repeatably from a patched copy. See [Read boost's `unordered_flat_map` again after it turned out ...](#read-boosts-unordered_flat_map-again-after-it-turned-out-to-have-the-probe-bound-this-map-was-missing).
- Measurement rules found here: never link an allocation counter into a binary that measures time, say whether a build panel destroys the map, and check `maps_one.sh`'s third argument (the number of lookups).

### `ie64` ties boost while executing 58% more instructions, and the counts say why

*2026-09-05 · info · the workload run on each map alone under `perf stat`, net of its own rng and key scrambling, `/tmp/ie_count.cpp`*

| per operation | this map | boost |
|---|---|---|
| instructions | 89 | 56 |
| cycles | 43.8 | 43.5 |
| branch mispredictions | 0.61 | 0.67 |
| L1 data misses | 1.7 | 1.2 |
| L2 misses | ~0 | ~0 |

Neither map is instruction-bound. Boost retires 1.3 instructions per cycle and `unordered_dense` 2.0, on a core that can do four. Both wait on the workload. Every `operator[]` and every `erase` in `ie64` is a coin flip on whether the key is present: 49.9% hits for both. So each operation costs about half a misprediction at ~16 cycles whatever the map. Then comes one chain of dependent loads, which at 10k entries sit in L2: the hash, the group metadata at a random address, the element to compare.

Boost's chain is metadata, then a 16 byte slot. The chain of `unordered_dense` is metadata, index, value. But the index line is prefetched from the group address before the fingerprints arrive, so the extra hop mostly overlaps. The 33 extra instructions (vector append and pop, the counter walks, the second probe on a successful erase) run in the shadow of those stalls, with issue slots to spare. Boost pays slightly more in mispredictions: its overflow bits stay set until the next rehash, and the table churns between growths, so its misses walk one group further than a fresh table's.

The same numbers say where the tie ends. On a table that does not fit in cache, or a workload with no coin flip, the memory chain dominates and boost's shorter chain shows. That is the 11% on lookups (see [Read boost's `unordered_flat_map` again after it turned out ...](#read-boosts-unordered_flat_map-again-after-it-turned-out-to-have-the-probe-bound-this-map-was-missing)). Nothing here is spare on the instruction side: a change that lengthens the dependent chain costs at once, and a change that only saves instructions is invisible on this workload.

### The same-hash convention was flattering boost on every string chart, and the control found it

*2026-09-06 · info · geomean per octave*

Handing every alternative this map's hash is the right way to compare *indexes*, and this project has always done it. But it is not what a caller gets, and on string keys it reverses the answer.

Boost with the hash it ships with, against boost given this wyhash, geomean per octave (above 1.00 means its own hash is slower):

| workload | uint64_t, 1K to 512K | std::string, 1K to 256K |
|---|---|---|
| find, all hits | 0.93 to 0.99 | **1.30 to 1.22** |
| find, 50% hits | 0.98 to 0.99 | 1.31 to 1.27 |
| churn | 1.00 flat | 1.17 to 1.09 |
| insert and erase | 0.99 to 1.00 | 1.26 to 1.19 |

For an integer key boost's default is **1-7% faster** than this wyhash. `boost::hash<uint64_t>` is close to the identity, foa mixes internally anyway, and the multiply is pure cost. Once the table leaves cache the multiply buys nothing, which is why the column walks back to 1.00. For a string, boost's default is **9-31% slower**, because its default string hash is not wyhash (corrected 2026-10-02: said "8-31%").

**That flips the string ranking.** `unordered_dense` against boost, geomean per octave, above 1.00 meaning boost is ahead:

| string workload | vs boost + this wyhash | vs boost's own hash |
|---|---|---|
| find, all hits | 1.07 to 1.17 | **0.82 to 0.96** |
| insert and erase | 1.10 to 1.12 | **0.87 to 0.97** |
| churn | 1.09 to 1.33 | 0.93 at 1K, boost ahead above |

So "boost is ahead on string lookups", said repeatedly in this file and its README, holds only for boost *with this project's hash*. Out of the box, `boost::unordered_flat_map<std::string, V>` takes 1.04-1.22x `unordered_dense`'s time on string lookups (corrected 2026-10-02: said "is 4-18% slower than `unordered_dense`"). The integer picture is unchanged: boost leads there with either hash. Both views are on the charts, but only one of them is what a reader gets by typing the type name.

### Read boost's `unordered_flat_map` again after it turned out to have the probe bound this map was missing

*2026-09-06 · kept (MSVC prefetch) · source reading*

Three things came back, most valuable first:

- **MSVC had no prefetch at all.** `ANKERL_UNORDERED_DENSE_PREFETCH` was `__builtin_prefetch` for gcc and clang and `static_cast<void>` for everything else. So the index-line prefetch, measured at 3 cycles off every hit, was silently absent on a whole compiler. boost spells it `_mm_prefetch(p, _MM_HINT_T0)` on MSVC x86-64 and `__prefetch` on MSVC ARM64. Taken. Not measurable here, since none of the benchmarking runs on MSVC: it closes a gap by construction and demonstrates no win.
- **boost tunes the prefetch per architecture** and says so in a comment: "ARM architectures get a higher speedup when around the first half of the element slots in a group are prefetched, whereas for Intel just the first cache line is best." `unordered_dense` issues the same two or three prefetches everywhere. With `bench.yml` this is a measurable question. It has not been asked. **Later (2026-09-07):** the x86 half is answered, with nothing to tune that is right for both compilers; ARM is still open, see [The SSE probe audited, and its one real redundancy is compiler-dependent](#the-sse-probe-audited-and-its-one-real-redundancy-is-compiler-dependent).
- **boost has an opt-in statistics facility** (`BOOST_UNORDERED_ENABLE_STATS`, `cumulative_stats.hpp`). It keeps a running mean and variance of probe lengths and comparisons per lookup with Welford's algorithm. Every probe-length number in this file came from hand-editing a copy of the header instead. A built-in equivalent would make these measurements repeatable. It is the one idea here that is a feature rather than a fix. **Later (2026-09-07):** `scripts/ab/probe_length.sh` patches the counter into a copy of the probe, so probe lengths are now measured repeatably without it, and its source says the counter does not belong in the header, where it would be in everybody's lookup; see [Three ideas the eighteen-map comparison suggested](#three-ideas-the-eighteen-map-comparison-suggested-all-measured-none-kept), which re-derived its figures with it.

Two details looked at and deliberately not taken:

- boost reserves *two* metadata values, 0 for empty and 1 for a sentinel that ends iteration, and remaps hashes 0 and 1 to 8 and 9. `unordered_dense` reserves only 0 and so has one more usable fingerprint. It iterates the value vector and needs no sentinel.
- boost's 16 byte group is 15 fingerprints plus the overflow byte, read with an *aligned* load and masked with `& 0x7FFF`. The 24 byte group here holds 16 fingerprints and eight counters and is read unaligned, which the layout sweep measured as free.

Read and found to have nothing to transfer, with the reason in each case:

- **folly F14** is the closest relative. Its `outboundOverflowCount_` is exactly this map's overflow counter: saturating, decremented on erase, used to stop a miss. Arrived at independently. One difference favours `unordered_dense`: F14 keeps *one* counter per 14 slot chunk, where this map keeps eight per 16 slot group, split by fingerprint class, so a miss here stops sooner.
- **abseil**'s probe sequence is the same triangular one, `(i^2+i)/2` over a power-of-two number of groups. Its `next()` adds `Width` per step where `next_group()` adds one group: the same progression, written differently. Its newer small-object optimization holds one element without allocating. `unordered_dense` already allocates nothing until the first insert, and the score has no workload of tiny maps, so it was not pursued.
- **bytell** puts a chain-head bit and a 7 bit index into a 126 entry jump-distance table in one byte per slot. Chains are linked lists with one byte links, and the first sixteen distances are 0..15 to keep short chains inside a block. It buys the ability to *skip* groups. A lookup here visits 1.03 groups fresh and 1.27 churned, so there is nothing to skip.
- **Verstable**: same conclusion from reading. It confirms one detail: it takes the hash fragment from the *high* bits because the bucket comes from the low ones. `unordered_dense` gets the same independence by taking the group from the top of the hash and the fingerprint from the bottom. Its layout is described in [Verstable measured rather than read](#verstable-measured-rather-than-read).
- **tsl::hopscotch_map** keeps a per-bucket bitmap of which of the next N buckets hold keys belonging here. It is positional where the counters are numeric, and *coarser*: one bitmap per bucket against eight counters per group. It keeps its invariant by moving elements closer to home, which is the work this design exists to avoid.
- **Go's map** evacuates one bucket per operation instead of rehashing at once. That trades total throughput for tail latency, a different goal from the one the score measures, and it is a redesign of growth rather than a transfer. Not attempted.

**Later (2026-09-07):** Verstable was benchmarked rather than only read, and that entry supersedes the Verstable bullet above; its one transferable idea, the exact in-home-bucket test, is measured and rejected in [And the fifth point on that axis](#and-the-fifth-point-on-that-axis-an-exact-counter-worth-2-3). See [Verstable measured rather than read](#verstable-measured-rather-than-read).

Where the map stood against others on the score, same hash for all, measured on 2026-09-05 before the rehash fix, the evening before the source reading above (corrected 2026-10-02: said "measured the same day"): excluding the three iteration workloads, `boost::unordered_flat_map` is level (0.96) and **ahead on lookups alone by 11%**. `emilib` is 12% behind, `emhash7` 20%, `emhash8` 27%, `emhash5` 28%. With iteration included `unordered_dense` leads all of them, because only `emhash8` is dense as well and the rest lose 3-10x there. The standing weakness is the same one this file has always named: building.

**Later:** building stopped being the weakness after the rehash fix: boost's time over this map's on build is 1.62 (clang) and 1.77 (gcc), see [That last sentence stopped being true on 2026-09-05](#that-last-sentence-stopped-being-true-on-2026-09-05-building-against-boost-after-the-rehash-fix).

**Later (2026-09-07):** every boost comparison taken before 2026-09-07 was at one size, including the ratios in this entry; averaged over an octave, five of them cross 1.00, see [Every boost comparison in this file that predates 2026-09-07 was taken at one size](#every-boost-comparison-in-this-file-that-predates-2026-09-07-was-taken-at-one-size-and-five-of-them-cross-100-when-the-octave-is-averaged-instead).

### That last sentence stopped being true on 2026-09-05: building against boost after the rehash fix

*2026-09-05 (memory re-measured 2026-09-06) · superseded · same hash, 12 paired epochs*

[That last sentence](#that-last-sentence-stopped-being-true-on-2026-09-05-building-against-boost-after-the-rehash-fix) is "The standing weakness is the same one this file has always named: building", the end of [Read boost's `unordered_flat_map` again after it turned out ...](#read-boosts-unordered_flat_map-again-after-it-turned-out-to-have-the-probe-bound-this-map-was-missing). This entry answers it, so it follows it here.

Re-measured against `boost::unordered_flat_map` after the rehash fix, same hash, 12 paired epochs, boost's time over this map's:

| | clang | gcc |
|---|---|---|
| score, 15 workloads | **1.56** | **1.49** |
| without the three iteration workloads | 1.13 | 1.17 |
| iterate | 5.76 | 3.91 |
| build | 1.62 | 1.77 |
| churn at a fixed size | 1.16 | 1.21 |
| insert and erase | 0.96 | 0.99 |
| find, half hits | 0.90 | 0.88 |
| random hits and misses | 0.92 | 0.92 |

Building is now a *win*: `build64` 1.39x and 1.64x, `buildbig` 2.09x on both compilers. The store-to-load chain in the rehash held it back, not the design.

What is left of boost's advantage is exactly one thing, fresh-table lookups. Boost takes **13-20% less time on a hit** (`rhit64` 0.80, `findbig` 0.87) (corrected 2026-10-02: said "is 10-13% faster on a hit"), and `unordered_dense` is faster on a miss (`rmiss64` 1.12 under clang). That is the value-index indirection: one more dependent load than a flat map needs. It is the price of the dense value vector, the same property that pays 3.9-5.8x on iteration and 2.1x on a 64 byte build.

Memory for a million entries, re-measured 2026-09-06 with a counting allocator and again with a replaced global `operator new`, the two agreeing exactly: **27.0 MB against boost's 32.0** steady with an 8 byte value, and 113.5 against 205.5 peak with a 64 byte one.

**Correction (2026-09-06):** the figures this line used to carry, 39.5 against 67.9 and 144.5 against 209.0, do not reproduce under either method. The boost steady one cannot be right by arithmetic: a million entries in 1966079 slots of a 16 byte `value_type` is 31.5 MB of slots plus 2.1 MB of group metadata, 33.6 MB or 32.0 MiB, which is what both methods return; the figures in this entry are MiB (corrected 2026-10-02: said "is 31.5 MB, which is what both methods return").

**Later (2026-09-07):** the churn row (1.16 and 1.21) is a single-size artefact; averaged over an octave boost is 1.15-1.29x ahead on churn, see [Every boost comparison in this file that predates 2026-09-07 was taken at one size](#every-boost-comparison-in-this-file-that-predates-2026-09-07-was-taken-at-one-size-and-five-of-them-cross-100-when-the-octave-is-averaged-instead).

**Later (2026-09-07):** the memory figures at a million are one point on the sawtooth, near this map's best point and boost's worst; over an octave boost is ahead with an 8 byte value (32.6 against 29.2) and the two are a wash at 64 bytes, see [Those memory figures are a point on the sawtooth](#those-memory-figures-are-a-point-on-the-sawtooth-and-the-octave-says-something-else).

### Every other map on the same workloads, in one harness

*2026-09-07 · info · `scripts/ab/maps.{h,cpp,sh}`, `maps_one.{cpp,sh}`, `mapsplot.py`, `diagrams.py`; written for the blog post on index structures*

The harness interleaves eighteen maps for an integer key and sixteen for a string, by `compare()` in one process:

- this header, and this header at `v4.11.0` renamed the way `run.sh` does,
- boost flat and node, abseil flat and node (each also with its own hash as a control),
- F14 Value/Vector/Node, emhash8, emilib, indivi `flat_umap` and `flat_wmap`, Verstable, ihtab and `std::unordered_map`.

Three key shapes (`uint64_t`, `std::string`, and `uint64_t` with a 64 byte value), three octaves each, five sizes per octave. Every adapter was checked against `unordered_dense` over 400000 mixed operations first, and again under ASan/UBSan. **Two independent runs agree: 372 of 378 integer ratios within 5%, worst 1.12 on `iterate` at a thousand entries.**

The whole table is in the post. Four results are worth having here.

**A miss is where the counter earns its keep, and abseil is the control that proves it.** At the 32000 octave, time relative to `unordered_dense`:

| map | hit | miss |
|---|---|---|
| abseil | 0.73 (fastest of these three; corrected 2026-10-02: said "fastest measured") | **1.38** |
| boost | 0.79 | **0.83** |
| indivi `flat_umap` | 0.82 | 0.97 |

Same group compare, same SIMD, same load factor within 0.075. The difference: abseil's miss has to find an *empty control byte*, and at load 7/8 that is often not in the home group. Boost's overflow bit, indivi's counter and this map's counter all stop at home. So the overflow byte and the overflow counter are worth 1.4-1.7x of a miss against an otherwise identical SwissTable, measured across families rather than by patching this one.

**`indivi::flat_wmap` is the fastest hit of anything measured**: 0.71 at 32000, 0.62 at 500000. It is the *simplest* index in the comparison: one byte per slot, no groups at all, tombstones, load 0.8. Its sixteen byte window is read **unaligned starting at the home bucket**, so the home is lane 0 and a key at home is the first bit of the first mask. An aligned group puts the home in the middle, with half the group behind it. The price is the widest sawtooth here: 2.12x between the cheapest and dearest point of the 32000 octave, against 1.5-1.6x for the group designs. Worth testing on the group index. It may not transfer because this map's value indices are addressed per group. **Later (2026-09-08):** tested and rejected: 1-5% of a hit, and it forecloses the counters and the merged block, see [The sliding window, built and measured rather than simulated](#the-sliding-window-built-and-measured-rather-than-simulated) and [A dense map on flat_wmap's structure](#a-dense-map-on-flat_wmaps-structure-measured-against-the-shipped-one-and-the-comparison-is-not-what-it-looks-like).

**The chained designs lose the miss badly**: emhash8 2.13 and Verstable 2.10, both against this map's counters, and Verstable executes 22% *fewer* instructions per miss than `unordered_dense` does in the table below (17% in [Verstable measured rather than read](#verstable-measured-rather-than-read), whose counts read 8-13 fewer instructions per lookup for every map) (corrected 2026-10-02: said "17%"). A chain must be walked to its end, and whether there is one is unpredictable. That is the same result the counter-width table records from inside this header (see [The width of the overflow counter](#the-width-of-the-overflow-counter-all-four-divisions-of-a-groups-eight-counter-bytes-measured-against-the-designs-eight-one-byte-counters)).

**And the own-hash control moved further than expected.** For a string key `boost::hash` costs boost 31% on a hit and 48% on a miss (0.87 to 1.14, 0.85 to 1.26). It turns a map that is ahead of `unordered_dense` into one that is behind it. But **`absl::Hash<std::string>` costs abseil 1-4% and nothing else**. So the "its own string hash is slower" line (see [The same-hash convention was flattering boost on every string chart](#the-same-hash-convention-was-flattering-boost-on-every-string-chart-and-the-control-found-it)) is about boost specifically, not about defaults in general. For an integer key both defaults are *cheaper* than this wyhash, and abseil's is worth **1.4x on a build** (`absl-own` 1.12 against `absl` 1.61), the largest single effect of a hash anywhere in this comparison.

**The counters, one map per binary, 30M lookups at 50000 entries** (`scripts/ab/maps_one.sh`). This is where the sub-10% questions were settled. Per lookup:

| case | map | instructions | cycles | branch misses |
|---|---|---|---|---|
| all hits | `indivi::flat_wmap` | 48.0 | **19.8** | 0.035 |
| all hits | absl | 56.1 | 21.4 | 0.044 |
| all hits | boost | 57.0 | 24.8 | 0.094 |
| all hits | this map | 60.5 | 29.4 | 0.065 |
| all hits | emhash8 | **48.4** | 36.6 | 0.420 |
| all hits | Verstable | 61.2 | 34.8 | 0.426 |
| all hits | `std::unordered_map` | **45.1** | 52.4 | 0.325 |
| all misses | this map | 57.2 | **20.7** | 0.108 |
| all misses | boost | 54.2 | 20.4 | 0.164 |
| all misses | absl | **61.1** | **32.6** | **0.362** |
| all misses | Verstable | **44.6** | **40.8** | **0.806** (IPC 1.09) |

Nobody here is instruction-bound (IPC 0.86 to 2.76 in this table, on a four-wide core) (corrected 2026-10-02: said "IPC 0.78 to 3.26"). The fast maps are the ones the predictor gets right. At a million entries the dTLB column is the family split: 1.34-1.37 misses per hit for the flat maps that touch one region, 1.78-2.21 for the dense ones that touch two, 2.58 for a node map. That is the same 22%-worth-of-huge-pages gap recorded in [Two optimizations the charts point at](#two-optimizations-the-charts-point-at-one-measured-and-one-not-yet), measured from the outside for the first time.

**Memory, and the second place tombstones show up.** Bytes of heap per live entry, `mallinfo2` around a build and around a full turnover, octave geomean at 32000.

| value | map | after build | after turnover |
|---|---|---|---|
| 8 byte | absl | 27.0 | **31.0** |
| 8 byte | indivi-w | 31.0 | **35.7** |
| 8 byte | ihtab | 36.1 | **72.3** |
| 8 byte | this map | 32.6 | 32.6 |
| 8 byte | boost | 29.2 | 29.2 |
| 8 byte | F14Value | 29.2 | 29.2 |
| 8 byte | indivi-u | 28.6 | 28.6 |
| 8 byte | Verstable | 28.6 | 28.6 |
| 8 byte | emhash8 | 38.0 | 38.0 |

*Every* tombstone-free map stays flat to the byte. With a 64 byte value **the node maps win** (94-95 against this map's 107.6, boost's 122.1 and emilib's 130.2). A flat map pays for every empty slot at the full width of the value, and a node map pays a pointer. That reverses the eight byte ordering completely, and it is the one column where `std::unordered_map` (108.4) is competitive with anything. emilib has tombstones and does *not* grow, because it counts only live elements against its limit. It pays in probe length instead.

### ihtab and ixhtab measured, and a bug in one of them reported upstream

*2026-09-07 · info · vnmakarov/ihtab#2 · `/home/martinus/gra/ihtab/sololynx`, vnmakarov/ihtab*

ihtab builds fastest by buying probe length with memory. ixhtab has a bug that makes its heap grow without bound under churn; it was reported upstream with a fix.

`iht::ihtab` is an eight slot SSE group: eight one byte tags interleaved with eight four byte indices, at a **50% maximum load**, with tombstones it never reclaims. `EMPTY` is `0xc0` and `DELETED` `0x80`, chosen so that `match_empty` is one `movemask(g & (g << 1))`. `els_bound` only ever grows, so a full element array is compacted by rebuilding the whole table. `ixht::ixhtab` puts extendible hashing on top: a directory of bins, each an `ihtab` with sixteen bit indices, split once a bin reaches `1 << MAX_BIN_SIZE_POWER`.

Measured with the same harness as [Verstable measured rather than read](#verstable-measured-rather-than-read), but before it grew the octave sweep. These are single sizes, so single points of four different sawtooths: read them as an ordering, not as ratios. ns per operation:

| n | map | build | hit | 50% hits | iterate | churn | insert/erase |
|---|---|---|---|---|---|---|---|
| 50000 | this map | 7.80 | 5.65 | 10.38 | **0.11** | 33.87 | 26.41 |
| | boost | 10.34 | **4.53** | 9.47 | 0.91 | 35.57 | 27.19 |
| | ihtab | **5.89** | 4.83 | **9.13** | 1.12 | 35.62 | 27.54 |
| | ixhtab | 12.38 | 6.37 | 11.23 | 1.38 | 165.28 | 111.70 |
| 1000000 | this map | 13.71 | 34.28 | 36.04 | **0.27** | 180.40 | 112.36 |
| | boost | 20.10 | **25.41** | **28.80** | 2.08 | **79.15** | **74.90** |
| | ihtab | **10.88** | 32.99 | 34.25 | 1.12 | 116.06 | 95.07 |
| | ixhtab | 24.16 | 54.44 | 60.17 | 1.41 | 212.24 | 185.38 |

ihtab is quick for the reason on the label: at a 50% load factor a lookup almost always lands home, and it pays with twice the slots. Any of these designs can buy probe length with memory. It is not an index idea: it is the same axis the two bit counter sat on, filtering best when fresh. ixhtab's churn column is not a design property but the bug below.

**The bug.** `ixhtab.hpp:290` decides whether a full bin should grow with `if (2 * els_num >= indexes_size)`. `els_num` is the **whole table's** live count (member at `ixhtab.hpp:102`), and `indexes_size` is **one bin's** index size. For any table larger than a single bin the test is always true. So `grow` is always set, and the code splits instead of compacting in place. A deleted slot is never reclaimed, so a bin fills its element array from tombstones alone, however few of its elements are live. Each bin then splits about once per turnover, each split halves the live occupancy of both halves, and nothing merges back.

At a constant 50000 live elements over 40 turnovers the heap goes **1.4 MB to 44.8 MB**, 29.5 to 938.9 bytes per element and still doubling, and a hit goes from 8.2 ns to 17-30. `ihtab::rebuild()` has the same-shaped test and is correct there, because both quantities describe the same single table. That is why ihtab stays flat. The bug is in all four headers (`ixhtab.hpp:290`, `ixhtab.h:290`, `ixhtab-v0.hpp:298`, `ixhtab-v0.h:298`) and was reported as vnmakarov/ihtab#2 with a self-contained reproducer, the cause and a fix.

The transferable part is the test that found it: **a workload that holds the element count exactly constant while churning is the only one that can see this class of fault**, which is why `churn` is in the score.

### Verstable measured rather than read

*2026-09-07 · info · integer keys, every map given this map's wyhash, five sizes per octave, geomean; asked as "here is another hashtable I want compared"*

In cache Verstable loses to both `unordered_dense` and boost on everything except the build at 1K, where it beats boost (1.30 against 1.51) (corrected 2026-10-02: said "on everything"). Out of cache it converges on boost and ties it on hits at half a million entries. A Verstable miss runs 17% fewer instructions and takes twice the cycles, because its chain walk is a data-dependent branch.

**Design.** One `uint16_t` per bucket: four bits of hash fragment, one bit saying "the key here belongs here", and an eleven bit quadratic displacement to the next key in this bucket's chain. Every key homed at a bucket sits on one linked list threaded through otherwise-unused buckets, and a lookup visits *only* buckets holding keys that belong to it. Key and value sit inline in a flat bucket array, both arrays come out of one `malloc`, `MAX_LOAD` is 0.9, it is tombstone-free, and an insert evicts at most one key to keep the invariant that a chain starts at its home bucket.

**Setup.** It compiles as C++ unchanged, so the comparison is one translation unit and every lookup inlines. Its buckets are raw `malloc` memory that is never constructed, so the key has to be trivially copyable: this is an integer-key comparison. The adapter was cross-checked against `unordered_dense` over 400000 mixed operations under ASan/UBSan first. That caught a real mapping error: Verstable's `_insert` *replaces* an existing value, like `insert_or_assign`, so the honest counterpart of `try_emplace` is `_get_or_insert`. The octave geomean is needed because 0.9 against boost's 0.875 against sixteen slots per group is three maps that double at three different sizes.

Time relative to `unordered_dense`, below 1.00 meaning faster than it:

| workload | verst 1K | boost 1K | verst 32K | boost 32K | verst 500K | boost 500K |
|---|---|---|---|---|---|---|
| build from empty | 1.30 | 1.51 | **3.24** | 1.73 | 2.75 | 1.64 |
| find, all hits | 1.50 | 0.88 | 1.09 | 0.79 | **0.75** | 0.75 |
| find, all misses | 2.48 | 1.05 | 2.04 | 0.90 | 0.97 | 0.76 |
| find, 50% hits | 1.84 | 0.99 | 1.64 | 0.88 | 0.85 | 0.77 |
| iterate | 15.4 | 11.8 | 10.6 | 9.0 | 5.9 | 6.4 |
| churn at a fixed size | 1.71 | 0.83 | 1.25 | 0.68 | 0.62 | 0.49 |
| insert and erase | 1.81 | 0.92 | 1.31 | 0.75 | 0.67 | 0.60 |

gcc agrees on the ranking at every workload (verst against `unordered_dense` at 32K: 2.08, 1.20, 2.08, 1.65, 7.96, 1.21, 1.24), so it is not clang layout. Verstable's own integer hash, a three-op xorshift-multiply-xorshift, costs it a further **3-14%** against being handed this wyhash, largest in cache. That is the opposite sign from boost, whose own integer hash is 1-7% *faster*.

**Why, and it is a mechanism this file keeps arriving at from new directions.** One map per binary, 30M lookups at 50000 entries:

| | instructions | cycles | branch misses | L1 misses |
|---|---|---|---|---|
| miss, this map | 44.1 | 19.0 | 0.107 | 3.24 |
| miss, boost | 45.1 | 19.1 | 0.162 | 1.89 |
| miss, Verstable | **36.7** | **38.0** | **0.812** | 1.96 |
| hit, this map | 50.5 | 29.1 | 0.065 | 4.22 |
| hit, Verstable | 49.0 | 33.2 | 0.418 | 3.26 |

**A Verstable miss executes 17% fewer instructions than this map's and takes twice the cycles.** The design delivers what it advertises, fewest instructions and fewest cache lines touched, and hands all of it back at the branch predictor. "Is my home bucket a chain head, and how long is the chain" is a data-dependent decision on every lookup, where a group compare is not. At load 0.9 about 59% of misses land on a chain head and have to walk it. That is the robin-hood-against-group-probe result again, reached by a completely different design.

**The build gap is the insert path.** At 200000 entries, ns per element:

| | this map | boost | Verstable |
|---|---|---|---|
| reserved inserts | 6.84 | 3.80 | 11.32 |
| from empty | 9.64 | 12.89 | **26.49** |
| branch misses per element, from empty | 0.132 | 0.312 | **2.398** |

Growth costs Verstable 143 instructions and 79 cycles per element against `unordered_dense`'s 44 and 12, because a rehash re-runs the whole insert for every key. `find_first_empty` quadratic-probes for a free slot, `find_insert_location_in_chain` walks the chain to keep it ordered by displacement, and an occupied home bucket calls `evict`, which re-hashes the occupant and walks *its* chain. "Only moves one existing key" is a statement about moves and says nothing about probing.

**Memory**, bytes per entry with an 8 byte value, octave geomean: this map 32.6-33.9, boost 27.7-29.2, **Verstable 27.1-28.6**. Verstable is the leanest of the three, at 18 bytes per slot against boost's 16 at a lower maximum load. All three are flat across churn, which checks that all three are tombstone-free. With a 64 byte value Verstable is a flat map and builds like one (84.9 ns per element against this map's 55.5, boost 87.1). It iterates 1.6x better than boost (2.84 against 4.49, this map 1.10), because the two byte metadata array finds a sparse table's occupied buckets without touching the buckets themselves.

The one transferable idea in it, the exact in-home-bucket test, is measured and rejected in [And the fifth point on that axis](#and-the-fifth-point-on-that-axis-an-exact-counter-worth-2-3). What makes Verstable competitive out of cache is not that. It is key and value inline behind no value index, and one allocation rather than two, which at a million entries is 0.13 dTLB misses per miss against this map's 0.77. That is the dense design's known structural cost, not a new idea.

### The F14Vector string miss, re-measured, and the old explanation of it is wrong

*2026-09-10 · info · asked as "is my map the fastest dense map"; one map per binary (`scripts/ab/maps_one.sh`), 30M lookups, three runs agreeing to 0.5%*

At 32000 entries the string hit ties F14VectorMap, the integer miss is 13% faster here, and the one loss is the string miss (+8 instructions, +1.3 L1 fills). The old diagnosis, clang leaving the probe out of line, no longer applies.

The paired octave still has F14VectorMap ahead on string lookups: miss 1.06 at the 1000 octave and 1.09 at 32000, hit 1.02-1.03, half 1.04-1.06. Per size across the 32000 octave the miss ratio runs 1.072, 0.990, 1.125, 1.145, 1.118. Per lookup at 32000:

| | ns | instructions | cycles | branch misses | L1 misses |
|---|---|---|---|---|---|
| str miss, this map | 18.99 | 138.4 | 100.0 | **1.046** | 5.19 |
| str miss, F14Vector | **17.20** | **130.5** | **90.7** | 1.13 | **3.91** |
| str hit, this map | 24.9 | **166.2** | 131.7 | 1.068 | 8.36 |
| str hit, F14Vector | 25.0 | 170.8 | 132.4 | 1.079 | **7.72** |
| u64 miss, this map | **3.24** | **55.8** | **16.8** | **0.038** | 3.26 |
| u64 miss, F14Vector | 3.67 | 61.2 | 19.0 | 0.103 | **1.99** |

So the string **hit** is a tie on time with fewer instructions here, the integer miss is **13% ours** (3.24 against 3.67 ns) (corrected 2026-10-02: said "27% ours"), and the one loss is the string miss: **+8 instructions and +1.3 L1 fills**, while winning on branch misses. The L1 column is a constant of the design. `unordered_dense` takes ~1.3 more L1 fills per miss and ~0.6 more per string hit, and wins the other two anyway (corrected 2026-10-02: said "~1.3 more fills per lookup on every workload").

**The entry above that blames clang for leaving `do_find_hashed` out of line no longer applies.** That entry is the [Where the F14Vector string gap actually is](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept) paragraph of [Seven ideas from reading folly F14 and from the F14Vector string gap](#seven-ideas-from-reading-folly-f14-and-from-the-f14vector-string-gap-all-measured-on-2026-09-07-none-kept). `nm` on today's binaries: the only out-of-line probe symbols in the sixteen-map binary belong to the **4.11.0 baseline copy** (`udmbase::...::v4_11_0`). The current map's probe is inlined in both the one-map and the sixteen-map binary. The +8 instructions need another explanation.

**Where they are, from `perf annotate`.** `do_find` for a `std::string` key opens with six callee-saved pushes, a 72 byte frame and six spills, **all before the first group is compared**: the counter, `this`, the group mask, the delta, the hash, and `movdqa %xmm0, 0x30(%rsp)`, the broadcast fingerprint itself. The broadcast is spilled because `m_equal` is a call and xmm registers are caller-saved. So every lookup pays a vector spill to survive a `memcmp` that a miss reaches about 2.5% of the time.

**Splitting the probe so the miss path stops paying for it: measured, and it costs more than it saves.** Everything past the home group moved into a `noinline` continuation taking the word, the counter and the group index, so the fast path compares one group and stops. It does what it was meant to on the string miss: **138.3 to 126.5 instructions**, 99.4 to 95.8 cycles, 2.8% faster. It wrecks the integer paths: `u64 miss` 55.8 to 65.9 instructions (+18%), `u64 hit` at a million 69.1 to 85.0 instructions and 31.13 to 39.66 ns (+27%). A call in a lookup that was fully inlined makes the register allocator treat the caller-saved registers as clobbered, so the spills reappear around the call site. `[[gnu::cold]]` on the continuation plus marking the branch likely changed nothing: the fast path still has to be able to make the call. Reverted. It buys 8% of the one workload we lose and spends 20% of the ones we lead by most.

**Later (2026-09-10):** the split was kept after all for keys whose compare is a call: the two halves are selected by the key type and can be had separately, see [Splitting the probe past the home group](#splitting-the-probe-past-the-home-group-kept-for-keys-whose-compare-is-a-call) (#233).

**And a measurement trap that cost most of a session.** `scripts/ab/maps_one.sh`'s third argument is the **number of lookups**, and its default is 30,000,000. Passing 20, and then 400, measures 20 and 400 lookups. Three runs of the *unmodified* header then spanned 417500 to 491000 ns, a **16% spread**, and a "removing the prefetches is worth 16%" conclusion was built on two samples inside it. At 30M the same comparison resolves to 0.5%, and the true figure is 1.8%. Instruction counts were stable to 0.7% throughout and would have caught it at once. That is a rule this file already states, and it was not followed.

### Seventeen maps in their own default configurations, at a million entries: the dense layout wins iteration by 5.5x to 110x and the string build-and-destroy by 2.1x to 2.6x, and loses the integer find and churn to boost by 37% and 64%

*2026-09-12 · #277 · info · Ryzen 9 7950X, Fedora 44, clang 22.1.8, THP `madvise`; `scripts/ab/bench_readme.{cpp,sh}` and `scripts/ab/mapsplot.py readme`, one binary per map, five octave sizes from 1000000, ten million operations per timed cell, three rounds interleaved rounds-outermost with an untimed warm-up before each cell, median*

This is the harness behind the README's four graphs, two per key type. One graph holds `unordered_dense` and 4.11.0 against the other libraries. The other holds the shapes this map can be asked to take, because a reader choosing between libraries should not have to read past three rows that are one library configured three ways. The first published chart was wrong; see the correction below. Every number in this entry is from the re-run.

Ratios to `ankerl::unordered_dense::map`, `uint64_t` keys (`build+destroy` 23.80 ns/entry, `find` 29.29 ns/lookup at 50% hits, `churn` 84.63 ns/pair, `iterate` 0.19 ns/element, `peak RSS` 48.84 B/entry peak):

| map | build+destroy | find | churn | iterate | peak RSS |
|---|---|---|---|---|---|
| huge pages | 0.60 | 0.91 | 0.79 | 0.84 | 0.78 |
| segmented + huge | 0.57 | 0.94 | 0.95 | 1.38 | 0.66 |
| segmented | 0.64 | 1.05 | 1.13 | 1.54 | 0.58 |
| 4.11.0 | 1.59 | 1.29 | 1.49 | 1.02 | 1.14 |
| boost flat | 0.97 | 0.73 | 0.61 | 9.51 | 1.21 |
| absl flat | 0.81 | 0.81 | 0.81 | 13.33 | 1.11 |
| indivi flat_umap | 1.21 | 0.81 | 0.66 | 5.69 | 1.18 |
| indivi flat_wmap | 1.50 | 0.71 | 0.70 | 7.52 | 1.28 |
| emilib | 0.90 | 0.82 | 0.73 | 5.47 | 1.04 |
| F14Value | 1.22 | 1.15 | 1.08 | 6.75 | 1.20 |
| F14Vector | 1.53 | 1.09 | 1.13 | 1.11 | 1.23 |
| emhash8 | 1.38 | 0.97 | 1.08 | 0.89 | 0.97 |
| absl node | 3.64 | 1.15 | 1.70 | 24.49 | 0.99 |
| boost node | 3.82 | 1.08 | 1.35 | 45.12 | 1.04 |
| F14Node | 3.77 | 1.17 | 2.05 | 35.07 | 0.99 |
| `std::unordered_map` | 4.84 | 1.98 | 2.84 | 110.30 | 0.94 |

`std::string` keys (`build+destroy` 92.84 ns, `find` 124.38 ns, `churn` 416.86 ns, `iterate` 0.65 ns, `peak RSS` 123.19 B/entry) give a different picture. The build-and-destroy lead is 2.1x to 2.6x over every flat map. The two the integer key loses are much closer: boost finds at 1.15 and churns at 0.91, where the integer key reads 0.73 and 0.61.

| map | build+destroy | find | churn | iterate | peak RSS |
|---|---|---|---|---|---|
| huge pages | 0.71 | 0.97 | 0.95 | 1.00 | 0.98 |
| segmented + huge | 0.63 | 0.97 | 0.96 | 1.05 | 0.91 |
| segmented | 0.99 | 1.00 | 1.03 | 3.48 | 0.90 |
| 4.11.0 | 1.26 | 1.13 | 1.09 | 1.01 | 1.05 |
| boost flat | 2.35 | 1.15 | 0.91 | 3.04 | 1.22 |
| absl flat | 2.07 | 0.96 | 0.92 | 4.03 | 1.14 |
| indivi flat_umap | 2.12 | 0.97 | 0.91 | 2.96 | 1.16 |
| indivi flat_wmap | 2.45 | 0.91 | 0.89 | 3.19 | 1.27 |
| emilib | 2.56 | 1.34 | 0.95 | 2.75 | 1.13 |
| F14Value | 2.46 | 1.03 | 0.97 | 3.36 | 1.14 |
| F14Vector | 1.67 | 1.02 | 1.00 | 0.97 | 1.06 |
| emhash8 | 2.34 | 1.38 | 1.43 | 0.95 | 1.00 |
| absl node | 2.51 | 0.96 | 1.08 | 8.38 | 1.00 |
| boost node | 3.08 | 1.18 | 1.11 | 15.32 | 1.02 |
| F14Node | 2.26 | 0.97 | 1.05 | 11.57 | 1.00 |
| `std::unordered_map` | 3.64 | 1.79 | 1.57 | 99.88 | 1.11 |

**The `uint64_t` build corroborates #231 independently.** The huge page allocator reads 1/0.63 = 1.59x on the inserts alone at a million (1/0.60 = 1.67x with the destroy, the table's column), against #231's 1.5-1.7x from 200000 upwards, through a harness that shares no code with #231. The CSV's inserts-only column says `absl::flat_hash_map` builds at 0.82 with its own hash, and 0.81 with the destroy as in the table (corrected 2026-10-02: said "The same row says `absl::flat_hash_map` builds at 0.82"), so `unordered_dense` is not the fastest builder for integers either.

**Five defects in this harness, each of which would have published a wrong number, and each found only by looking at a figure that made no sense.** The first three were found before publishing. The last two were found after, when they had already been published in the PR. All five have the general shape of rules already in `CLAUDE.md`, met again in a new tool.

- *No warm-up.* The first of the five octave sizes read up to 2.3x the third: it is the size that pays for the process's first touch of every page. Fix: one untimed round first.
- *Cells of about a millisecond, and a loop with the maps outside the rounds.* Repeated runs of the same configuration disagreed by 6-13%, larger than most of the ratios being drawn. Ten million operations per cell and rounds outermost took that to 0.1-3.5%.
- *A memory counter that could not see two of the three allocation paths.* Counting only global `operator new` reported `emilib` at 0.00 B/entry (it calls `malloc` directly) and the huge-page segmented map at 0.35x (its blocks come from `mmap`). Interposing `malloc`, `calloc`, `realloc`, `free`, `mmap` and `munmap` fixed both.
- *And then the fixed counter charged the request rather than the chunk.* glibc serves a 24 byte node out of a 32 byte chunk, so counting `n` reported every node map as *cheaper* per `uint64_t` entry than `unordered_dense`: `std::unordered_map` at 0.88, where `malloc_usable_size` plus the header says 1.06. `scripts/ab/alloc_timeline.cpp` had had the right policy since it was written.
- The fifth defect is the correction below.

**Correction:** ***Retraction, and the largest of the five: the whole chart was drawn from binaries that taxed every allocation.*** The counter above was swapped in and only the `memory` panel was re-measured. So `build`, `find`, `churn` and `iterate` still came from binaries whose `malloc` kept a sixteen byte header of its own. Reconstructing that binary reproduces the published figures exactly: `std::unordered_map` build 157.0 against the 155.69 that shipped, `unordered_dense` 23.6 against 23.97. The same binary without the header reads 121. That is a 29% tax on a map that allocates per element and 0% on a dense one. The *replacement* interposer is not free either: measured over three rounds, it costs `std::unordered_map` 4.4% of the integer build and `unordered_dense` 0.0%. The four timed workloads now run from a binary with no counting layer compiled in (`-DUDM_COUNT_ALLOC` gates it, and only the `memory` binary gets it). Separately, `maps.h`'s `build()` destroys the map before returning, so the destructor was inside the clock: 33% of the timed region for `std::unordered_map` and about 5% for `unordered_dense`. Together the two put `absl node`'s integer build at 4.68 where it is 1.46, and `std::unordered_map`'s at 6.49 where it is 3.50. Every number in this entry is from the re-run; the superseded ones are named here rather than deleted.

**What this says and does not say.** It is one size band, one machine, one compiler and one workload shape. The octave starts at a million, which is past every cache on this machine; the `doc/` size-axis charts are what orders these maps below cache. `iterate` is a full pass over every element, and a program that never does one should read the chart without that column. The memory column is peak rather than steady: that is where a caller's ceiling is, not where its resident set sits. Nothing here is a paired A/B, and none of it transfers to a header-against-header question. For that, use `run.sh` and `solo.sh`; five octave points rather than fifty are only enough when both sides are this header (#274).

**Two of the five panels were the wrong measurement, and both were changed after the first publication.**

*Peak memory is now the process's resident high-water mark, not the bytes the map asked for.* The two disagree systematically. Every flat map reads 1.04-1.28x per `uint64_t` entry under RSS, where counted bytes put `absl` and `emilib` *below* `unordered_dense` at 0.96. The node maps read 0.94-1.04 against 1.06-1.18 counted. Which families move shows the mechanism. A flat map that doubles leaves every superseded array freed but resident in glibc's arena, and the next allocation is twice its size, so it cannot be reused. A node map allocates a million uniform blocks that are all reusable, and its two numbers agree to 1%. The huge page variants go the other way, 0.85, because the counter charges the whole `mmap` length and RSS only the pages touched. Both numbers are in the CSV (`rss` and `memory`). The ~30% is glibc's retention policy rather than a property of the map, and another allocator will not reproduce it. Measuring RSS needs a fork per fill. The first version read the second fill in the same process; glibc does not hand a grown arena back, so that fill reused resident pages and read below the bytes the map demonstrably allocated.

*The first panel is build **and destroy**.* Timing only the inserts hid the largest difference between the designs on this chart. Teardown is 62% of a node map's `uint64_t` lifetime and 6% of this one's, so `absl node` reads 1.46 on inserts alone and 3.64 on the whole thing. With string keys the split is sharper, and it is not about node versus flat: freeing a million `std::string` buffers costs `unordered_dense` 14 ns per entry and every flat map 64-68 ns. The strings are identical. A dense map holds them in insertion order and frees them in the order the heap was filled. A flat map holds them in hash order and frees them in an order unrelated to how they were allocated. `build` on its own stays in the CSV, and the difference between the columns is the teardown.

**Later (2026-10-02):** re-run against 5.2.0, find and churn against boost read 0.73 / 0.59 and `std::unordered_map` iterates 112x slower, see [Every configuration of 5.2.0, twelve rows in the README run (#349)](#every-configuration-of-520-twelve-rows-in-the-readme-run-349-pmr-is-free-group_big-costs-16-34-on-the-integer-build-find-churn-and-memory-below-232-elements-segmented_map-builds-19x-faster-at-058x-the-peak-memory-and-iterates-strings-338x-slower-with-its-default-4-kb-segment-and-huge-pages-are-the-largest-gain-on-every-integer-timing-panel).

### Why boost's `unordered_flat_map` finds faster (#341): it needs no value index, so the first thing a lookup touches is 1 byte of metadata per slot against this map's 5.5, which leaves L2 at a far smaller table; from 46080 entries to 460800 this map takes 1.7-2.1x boost's L3 fills per find and 14-35% more cycles under clang (0.93-1.04 under gcc), while in L2 it is compiler codegen alone (this map's cycles 0.83-0.88 of boost's under gcc, 1.22-1.25 under clang)

*2026-09-28 · #341 · info · Ryzen 9 7950X, clang 22 and gcc 16, boost 1.90; `scripts/ab/lookup_fills.cpp`, `map<int64_t, 8 byte value>`, 2^23 finds of keys drawn beforehand, all hits, pinned to one core, `perf stat -e ls_dmnd_fills_from_sys.local_ccx,cycles:u` around the process, one run per cell*

Top-down at 92160 entries (`perf stat -M PipelineL1,PipelineL2`, clang):

| | this map | boost |
|---|---|---|
| backend bound | 55.5% | 43.0% |
| of it memory | 53.6% | 41.8% |
| bad speculation | about 1% | about 1% |
| L1 misses per find | 3.4 | 3.0 |
| dTLB misses | negligible | negligible |
| demand fills per find from L2 | 1.05 | 1.13 |
| demand fills per find from L3 | **0.91** | **0.44** |

Over the size axis:

| entries | this map, clang: cycles, L3 fills | boost, clang | this map, gcc | boost, gcc |
|---|---|---|---|---|
| 10000 | 12.0, 0.00 | 9.8, 0.00 | 10.8, 0.00 | 13.0, 0.00 |
| 23040 | 12.8, 0.02 | 10.2, 0.01 | 12.1, 0.02 | 13.7, 0.02 |
| 46080 | 15.0, 0.28 | 11.7, 0.15 | 13.9, 0.32 | 14.5, 0.16 |
| 92160 | 18.1, 0.91 | 13.4, 0.44 | 16.5, 0.90 | 16.0, 0.44 |
| 200000 | 19.4, 1.39 | 16.1, 0.79 | 19.7, 1.39 | 19.0, 0.81 |
| 460800 | 24.5, 1.81 | 21.5, 1.02 | 23.5, 1.81 | 25.3, 1.00 |
| 1000000 | 50.3, 1.74 | 36.8, 1.34 | 50.0, 1.82 | 44.4, 1.30 |
| 2000000 | 91.2, 1.38 | 72.3, 1.49 | 97.5, 1.40 | 79.8, 1.46 |

**Why.** Every lookup first reads its group's metadata. boost keeps its elements in its slot array and needs only 16 bytes of metadata per 15 slots. That array is about 1 byte per slot and stays in L2 (1 MB here) up to far larger tables. `unordered_dense` keeps its values in one dense vector, so a group has to say where each of its entries lives: sixteen 4-byte indices beside the 24 bytes of fingerprints and counters, 88 bytes per 16 slots, 5.5 bytes per slot. The first access leaves L2 at a table about five times smaller, and the value access comes on top of it.

Where the index goes does not remove the cost. Beside the fingerprints (today) it bloats the first access. In its own array (until 2026-09-06, "merged block") it adds a third access. Narrower (16-bit indices 0.986) and tiny pointers (#229, ~9% memory, no speed) were measured before. It is the cost of dense values: the price of fast iteration.

What this says and does not say:

- In L2 (10000 and 23040 entries) neither map takes L3 fills, and the difference is code generation. `unordered_dense`'s cycles are 0.83-0.88 of boost's under gcc (10.8 against 13.0, 12.1 against 13.7) and 1.22-1.25 under clang (12.0 against 9.8, 12.8 against 10.2), at about the same instruction counts. Not investigated further.
- From 46080 to 460800 entries `unordered_dense` takes 1.7-2.1x boost's L3 fills. In cycles it is 1.14-1.35 of boost's under clang and 0.93-1.04 under gcc, where gcc's codegen advantage offsets it. Redpanda's loop (#317) sits at 92160.
- Past the L3 (2000000) the fills even out and both pay DRAM. `unordered_dense` is 1.26 (clang) and 1.22 (gcc) of boost's cycles there. Not examined.
- The scored benchmark's tables are up to 200000 entries with 50% misses. A miss reads only the metadata, which is why the group layout wins there. This entry is the all-hits case, one fill source per run and one run per cell.

[2026-09-28, #346: the keys here are dense ids (0..n-1, looked up at random). How boost compares on scrambled integer keys was not measured; 4.x, the other map these entries weigh main against, loses 1.65-2.4x to main on scrambled keys at 1000-16000 entries and 2.15x in cycles at 92160; larger tables were not measured on scrambled keys (corrected 2026-10-02: said "loses 2.2x to main on scrambled keys of the same sizes"). See [Hits in cache-resident tables](#hits-in-cache-resident-tables-1000-to-16000-entries-450-against-main-346-main-takes-041-061-of-450s-time-on-scrambled-integer-keys-and-071-079-on-strings-from-a-quiet-loop-and-a-busy-one-and-102-143x-of-it-on-dense-ids-0n-1-which-a-multiplicative-hash-places-without-collisions----450s-best-case-and-the-keys-redpandas-and-osrms-callers-have).]

## Real programs: MySQL, ClickHouse, STP, Redpanda, Valhalla, OSRM

These entries put `unordered_dense` into real programs and measure the program, not the map. Two things to take away. First, a relinked binary moves a whole query by a few percent through code layout alone, so a program-level number needs its instruction count and a control that does not run the map. Second, in most programs the map is a few percent of the time, and the results that move are where a small, cache-resident table is mostly hit (Redpanda, OSRM). There main's group probe costs more than 4.x's robin hood hit, but only for dense integer ids 0..n-1 (#346).

**Where it stands** (as of 2026-09-28)

- Relinking mysqld moves a query by 2.4% with identical instructions; every MySQL difference measured for #321 and #323 is inside that band ([MySQL's int join never reaches the map, which makes it the control for how much relinking mysqld moves a query](#mysqls-int-join-never-reaches-the-map-which-makes-it-the-control-for-how-much-relinking-mysqld-moves-a-query-24-the-size-of-every-mysql-difference-measured-for-321-and-323)).
- MySQL's `EXCEPT` on 5.2.0 read 7-11% slower than 4.4.0 because of where the linker put the map's code; an inactive block next to the call took it to +1% ([MySQL's `EXCEPT` read 7-11% slower on 5.2.0 than on 4.4.0](#mysqls-except-read-7-11-slower-on-520-than-on-440-and-it-is-where-the-linker-put-the-maps-code-an-inactive-block-added-next-to-the-call-took-it-to-1-and-removing-it-brought-the-7-11-back-with-do_find-and-hash_bytes-byte-identical-in-both-binaries)).
- 5.2.0's 26-45x op cache misses against 4.4.0 are gone on main (#328): 1.9x and 1.1x in two layouts against 4.4.0 measured in the same session (1.56 M), 2.3x and 1.3x against the 4.4.0 the 26-45x was measured on (1.3 M), and the query is level with 4.4.0 or faster ([#326's op-cache footprint was taken away by #328: ](#326s-op-cache-footprint-was-taken-away-by-328-mysqls-except-went-from-26-45x-440s-op-cache-misses-on-520-to-19x-and-11x-in-two-layouts-of-main-and-the-query-is-level-with-440-or-faster-in-both)).
- At MySQL's default 256 KB join buffer the drop-in pays 8.3% on the string hash join for an over-counted index; `index_bytes()` takes it back (1.833 against 1.812 s) ([MySQL's `EXCEPT` read 7-11% slower ](#mysqls-except-read-7-11-slower-on-520-than-on-440-and-it-is-where-the-linker-put-the-maps-code-an-inactive-block-added-next-to-the-call-took-it-to-1-and-removing-it-brought-the-7-11-back-with-do_find-and-hash_bytes-byte-identical-in-both-binaries)).
- ClickHouse's aggregation benchmark: main is within 1-5% of absl on the large columns except WatchID under clang (1.16), uses less memory than absl on two of the three, and is 12-13% behind 4.1.2 under clang on the columns with few distinct keys, open as #331 ([ClickHouse's aggregation benchmark on real Yandex.Metrica columns](#clickhouses-aggregation-benchmark-on-real-yandexmetrica-columns-where-412-was-published-behind-absl-on-every-large-column-main-is-within-1-5-of-absl-on-the-large-columns-except-watchid-under-clang-116-uses-less-memory-than-absl-on-two-of-the-three-and-is-12-13-behind-412-under-clang-on-the-columns-with-few-distinct-keys)).
- STP parses 4-5% faster on 5.1.0 than on 4.5.0, on cycles, not instructions ([STP's parser with 4.5.0 against 5.1.0 and main, ](#stps-parser-with-450-against-510-and-main-the-two-instances-from-stp560-510-parses-4-5-faster-on-both-instances-under-both-compilers-with-the-same-instructions-within-13-and-8-less-peak-memory-on-the-larger-instance-main-reads-within-31-points-of-510)). stp#567's propagation gain did not survive into STP master on the files screened; total solve time is level within 1% ([stp#567's constant bit propagator tables re-checked on STP master](#stp567s-constant-bit-propagator-tables-re-checked-on-stp-master-against-the-same-four-tables-as-stdunordered_-450-takes-propagation-to-0912-and-this-repositorys-main-to-0859-nearly-all-of-it-on-one-instance-testcase15-074-and-069-and-total-solve-time-is-level-within-1-where-567-read--22-propagation-and--49-total)).
- Redpanda's leader balancer: main's `map` is 3-10% slower than 4.5.0 with 80 keys and 15-20% slower with 92160; the `segmented_map` Redpanda ships is level (clang 0.917, gcc 1.042). The keys are dense ids, 4.x's best case; winning them back is #347 ([Redpanda's leader balancer benchmark (redpanda#17182) re-run standalone against main](#redpandas-leader-balancer-benchmark-redpanda17182-re-run-standalone-against-main-mains-map-is-3-10-slower-than-450-on-the-benchmark-as-written-whose-map-holds-80-keys-and-15-20-slower-with-one-entry-per-raft-group-as-in-a-real-cluster-where-it-executes-20-27-more-instructions-per-lookup-the-segmented_map-redpanda-ships-is-level-clang-0917-gcc-1042)).
- Valhalla's CostMatrix: 4.5.0, main and absl within 1.1% on every row; the map is a few percent of a request ([Valhalla's CostMatrix (valhalla#4552) re-run ](#valhallas-costmatrix-valhalla4552-re-run-with-the-map-valhalla-ships-450-main-and-absl-all-three-within-11-on-every-row-as-the-original-found-all-in-all-it-hardly-matters-because-the-map-is-a-few-percent-of-a-matrix-request-reachedmapadd-36-out-of-line-map-code-036)).
- OSRM: both 4.4.0 and main take map matching to 1.21-1.27x the requests per second of `std::unordered_map`; main is not ahead of 4.4.0 ([OSRM's e2e benchmarks (osrm-backend#6922) re-run on a quiet](#osrms-e2e-benchmarks-osrm-backend6922-re-run-on-a-quiet-pinned-core-with-the-440-6922-tried-and-with-main-both-take-map-matching-to-121-127x-the-requests-per-second-of-stdunordered_map-ch-table-to-110x-and-ch-trip-to-105x-and-main-is-not-ahead-of-440----equal-on-most-rows-behind-on-ch-match-1255-against-1274-and-mld-table-0986-against-1009)).
- udb3 is not in this section: see [udb3's `++h[key]` ran 70% slower under clang on 5.2.0 than on 5.1.0](#udb3s-hkey-ran-70-slower-under-clang-on-520-than-on-510-and-it-was-a-loop-variable-stored-and-reloaded-across-a-store-with-a-late-address-on-zen-4-that-with-a-division-on-the-way-to-the-next-key-stops-the-loop-overlapping-its-misses-5x-in-a-loop-with-no-map-at-all-the-part-of-the-insert-after-a-miss-is-called-again-under-clang).

### MySQL's int join never reaches the map, which makes it the control for how much relinking mysqld moves a query: 2.4%, the size of every MySQL difference measured for #321 and #323

*2026-09-27 · #321, #323 · method · MySQL 9.7.2 built with gcc 16 `-O2`, no LTO, Ryzen 9 7950X; `perf stat` and `perf record -c 1000003` on the pinned mysqld during the query; `mysqlish.cpp` beside it, one map per binary*

The int join does not run the map, so its 2.4% is linker layout. A MySQL number needs the instruction count beside it, and a query that does not run the map as its control.

The `bench.py` rounds for 5.1.0 against main after #323 read the int join 7.828 -> 8.016 s and `EXCEPT` 0.270 -> 0.276 s. Both had non-overlapping ranges. #321's run had `EXCEPT` at 0.260. Counted:

| query | instructions 5.1.0 -> main | cycles |
|---|---|---|
| hash join, int key, duplicates | 429,278,525,275 -> 429,278,668,041 | +2.4% |
| `EXCEPT`, int | 13.616 G -> 13.413 G (-1.5%) | +2.9% |

The int join's profile holds no `unordered_dense`, no hash join iterator and no `StoreRow`. The only hash in it is InnoDB's adaptive hash index. The time is `row_search_mvcc` and the record compares. Its instructions agree to seven digits because none of the changed code runs. Its 2.4% is where the linker put InnoDB.

`EXCEPT` does use the map: `MaterializeIterator` calls `segmented_map::emplace(key, LinkedImmutableString{nullptr})`, which #323 routes key-first. It retires fewer instructions, and its cycles move by the control's amount. Its profile has ~1170 samples a build, and the symbols move with inlining (5.1.0's out-of-line `emplace` holds 37 samples, main's `MaterializeIterator` gains 58), so the profile cannot split the two. That call alone, replayed in one map per binary (`segmented_map<std::string_view, pointer>`, 8-byte keys, 1M `emplace`s of which 49% are new, from empty, gcc `-O2`), is 123.6 -> 99.6 cycles (gcc) and 121.5 -> 100.1 (clang) against 5.1.0. #321 alone does not move it (123.5, 121.7).

So the MySQL rounds in "split at the home group" (see [`try_emplace` split at the home group](#try_emplace-split-at-the-home-group-the-hit-and-the-common-placement-inlined-into-the-caller-the-walk-past-home-behind-a-call-for-clang-only-and-every-scored-workload-at-or-under-mains-instruction-count-on-both-compilers)) and its 0.260 for `EXCEPT` are inside this band as well. The int join there was never a measurement of the map.

**Later (2026-09-27):** `EXCEPT`'s difference was split after all, but on another pair: 5.2.0 against 4.4.0, not 5.1.0 against main. There it was not the map's work but code layout. By analogy, this entry's +2.9% is layout too. See [MySQL's `EXCEPT` read 7-11% slower on 5.2.0 than on 4.4.0, and it is where the linker put the map's code](#mysqls-except-read-7-11-slower-on-520-than-on-440-and-it-is-where-the-linker-put-the-maps-code-an-inactive-block-added-next-to-the-call-took-it-to-1-and-removing-it-brought-the-7-11-back-with-do_find-and-hash_bytes-byte-identical-in-both-binaries).

### MySQL's `EXCEPT` read 7-11% slower on 5.2.0 than on 4.4.0, and it is where the linker put the map's code: an inactive block added next to the call took it to +1% and removing it brought the +7-11% back, with `do_find` and `hash_bytes` byte-identical in both binaries

*2026-09-27 · info · MySQL 9.7.2 built with gcc 16 `-O2`, no LTO, Ryzen 9 7950X; `perf stat`, `perf record -c 200003` over ten queries on the pinned mysqld; `scripts/ab/mysql_except_replay.cpp`, `scripts/ab/mysql_except_dump.patch`, `/home/martinus/gra/mysqlbench/bench2.py`*

With 2 GB buffers, ten `EXCEPT`s per server and two alternating rounds, 5.2.0 as a drop-in read 1.1887 / 1.1882 G cycles a query against 4.4.0's 1.1085 / 1.1105, on 1.3% fewer instructions. The int join, which never calls the map (see [MySQL's int join never reaches the map, which makes it the control for how much relinking mysqld moves a query](#mysqls-int-join-never-reaches-the-map-which-makes-it-the-control-for-how-much-relinking-mysqld-moves-a-query-24-the-size-of-every-mysql-difference-measured-for-321-and-323)), went 1.5% the other way. The profile put the difference in the map: `do_find` 1693 -> 3190 samples, the hash 1120 -> 1347, `memcmp` 3832 -> 4952. The A/B/A below shows it is code layout.

**Three explanations that the replay rules out.** The replay runs MySQL's call pattern on the map alone: `segmented_map<ImmutableStringWithLength, LinkedImmutableString>` with MySQL's hasher, keys as pointers to a one-byte length and eight bytes in an arena, the first table `emplace`d and the second `find`-ed. Per operation, 4.4.0 -> 5.2.0:

| keys | `emplace` cycles | `find` cycles | compares per `find` |
|---|---|---|---|
| `splitmix` of sysbench's `k`, 632k distinct | 147.2 -> 71.9 | 80.3 -> 64.8 | 0.6337 -> 0.6583 |
| MySQL's own, dumped from the query, 174k distinct | 54.2 -> 27.2 | 27.5 -> 23.1 | 0.8892 -> 0.9122 |
| MySQL's own, 16 random lines of 64 MB read per operation | 638.0 -> 588.8 | 600.1 -> 575.8 | same |

- False fingerprint matches: 5.x has 16x more of them, and they are 0.023-0.025 compares per lookup. A one-byte fingerprint over sixteen slots gives that rate with any hash.
- The hash on MySQL's keys: those keys are the output of MySQL's own row hash, and structured. About 24 of 64 bits vary, the low byte is always zero, and a million rows of 632k distinct `k` give only 174k distinct keys, so MySQL's hash already collides. `hash_bytes` spreads them as well as random keys: the compare count moves by the same 0.023.
- The group's second cache line and the fingerprint table, evicted between lookups by the scan: with 16 random cache misses of other work per operation, 5.2.0 stays ahead.

**The A/B/A.** The dump patch (a `getenv` test, never true in a timed run) went into `composite_iterators.cc`. With it, `EXCEPT` read 1.1019 / 1.1263 G cycles against 4.4.0's 1.1048 / 1.1004, on 19 M more instructions for the test. Taken out again and rebuilt, it read 1.2054 / 1.2394 against 1.1223 / 1.1155. `do_find` sits 384 bytes further on in the one binary, 64-byte aligned in both. Its instructions are identical except the RIP offset of the fingerprint table, which resolves to the same address. Its samples: 3479 in the slow binary, 1961 in the fast one; the hash 1523 against 581. Counted per query:

| binary | cycles | L1 icache misses | op cache misses | branch misses |
|---|---|---|---|---|
| 5.2.0, slow layout | 1209.6 M | 4.54 M | 59.0 M | 0.55 M |
| 5.2.0, fast layout | 1126.5 M | 0.56 M | 34.1 M | 0.72 M |
| 4.4.0 | 1119.2 M | 0.44 M | 1.3 M | 1.13 M |

The slow layout pays eight times the instruction cache misses of the fast one, 4.54 against 0.56 M (corrected 2026-10-02: said "ten times"). It is the front end, not the branch predictor: the branch predictor does better on 5.2.0 in both layouts. Which lines collide is not known. Counting the sampled hot lines per L1i set does not separate the two binaries (the fast one has more sets over eight ways), because samples show where time went and not every line fetched.

**What is 5.2.0's in every layout: 26-45x the op cache misses of 4.4.0.** #321 and #323 inline the whole insert into the caller, `flatten` pulls the vector's growth path in with it, and MySQL's `check_unique_fields_hash` is such a caller. In the fast layout it costs 0.7% of the cycles. It is also more hot code for an unlucky placement to collide with. Filed as its own issue.

**Later (2026-09-27):** main, which inlines only the home-group lookup (#328), brings this to 1.9x and 1.1x of 4.4.0's op cache misses in two layouts, with the query level with 4.4.0 or faster. Those ratios are against the 4.4.0 of that session (1.56 M); against this entry's 4.4.0 (1.3 M) they are 2.3x and 1.3x. See [#326's op-cache footprint was taken away by #328](#326s-op-cache-footprint-was-taken-away-by-328-mysqls-except-went-from-26-45x-440s-op-cache-misses-on-520-to-19x-and-11x-in-two-layouts-of-main-and-the-query-is-level-with-440-or-faster-in-both).

**Spilling at MySQL's defaults**, the other half of recommending 5.2.0 to MySQL. MySQL's hash join and set operations add `bucket_count() * sizeof(bucket_type)` to their memory and spill to disk past `join_buffer_size` / `set_operations_buffer_size`. That reads 24 bytes per slot on 5.x against 5.5 real ("You sized the index" in `doc/upgrading-to-5.md`). Median of three, two alternating rounds, seconds:

| query | buffer | 4.4.0 | 5.2.0 drop-in | 5.2.0 with `index_bytes()` |
|---|---|---|---|---|
| hash join, string key | 256 KB (default) | 1.812 | 1.962 (+8.3%) | 1.833 (+1.1%) |
| hash join, string key | 16 MB | 1.140 | 1.137 | 1.128 |
| hash join, string key | 64 MB | 1.111 | 1.094 | 1.075 |
| `INTERSECT` | 256 KB | 5.853 | 5.875 | 5.852 |
| `INTERSECT` | 16 MB | 3.079 | 2.999 | 3.045 |
| `INTERSECT` | 64 MB | 3.082 | 3.103 | 3.077 |
| `EXCEPT` | 256 KB | 0.490 | 0.433 | 0.486 |
| `EXCEPT` | 16 MB | 0.255 | 0.274 | 0.262 |
| `EXCEPT` | 64 MB | 0.253 | 0.273 | 0.261 |

The drop-in pays for the over-counted index where it spills, 8.3% on the string join at the default, and `index_bytes()` takes that back. The drop-in's `EXCEPT` at 256 KB reads 12% faster, which is not explained. The `EXCEPT` rows at 16 and 64 MB are the layout effect above (the `index_bytes()` build is a third link of mysqld).

What this says and does not say: one machine, one compiler, no LTO, which is not how MySQL ships. A layout effect of this size is specific to the binary, and another build of the same source may land either way. It does not say which functions collide. For MySQL, 5.2.0 with `index_bytes()` is level with or ahead of 4.4.0 on every query here but `EXCEPT`, which reads between +1% and +11% depending on the link, and whose map operations are 1.2-2x faster alone in cache and 1.04-1.08x faster with 16 cache misses of other work per operation (corrected 2026-10-02: said "1.2-2x faster alone").

### ClickHouse's aggregation benchmark on real Yandex.Metrica columns, where 4.1.2 was published behind absl on every large column: main is within 1-5% of absl on the large columns except WatchID under clang (1.16), uses less memory than absl on two of the three, and is 12-13% behind 4.1.2 under clang on the columns with few distinct keys

*2026-09-27 · #315, #328, #330, #331, #323 · info · Ryzen 9 7950X, clang 22 and gcc 16; `kitaisreal/hash-table-aggregation-benchmark` with its own `CMakeLists.txt`, 4.1.2 from its `contrib`, 5.1.0 and main (5.2.0 + #328 + #330; corrected 2026-10-02: said "5.2.0 + #323 + #328 + #330", and #323 is in 5.2.0) beside it, `absl::Hash` for every map as in the published results; `perf stat` cycles per row, median of three rounds with the maps interleaved, pinned to core 2*

The large columns went from 1.52 / 1.29 / 1.14 of absl (4.1.2) to 1.16 / 1.05 / 1.02 (main) under clang, and from 1.24 / 1.22 / 1.13 to 1.04 / 1.00 / 1.01 under gcc. 5.x's index (5.5 bytes per slot against 4.x's 8) takes main below absl's memory on URLHash and UserID.

The published run was on AWS c6a.4xlarge, in time, 4.1.2 / absl: WatchID 12.93 / 10.01 s = 1.29, UserID 3.17 / 2.65 s = 1.20. Here, 4.1.2 / absl in cycles: 1.52 and 1.14 under clang, 1.24 and 1.13 under gcc. The machine and compilers differ, so compare ratios, not seconds.

Cycles per row (ratio to absl), clang:

| column | ClickHouse HashMap | absl::flat_hash_map | google::dense_hash_map | 4.1.2 | 5.1.0 | main |
|---|---|---|---|---|---|---|
| WatchID | 311.6 (1.00) | 313.1 (1.00) | 427.9 (1.37) | 476.9 (1.52) | 373.9 (1.19) | 362.3 (1.16) |
| URLHash | 123.9 (1.01) | 122.7 (1.00) | 155.3 (1.27) | 158.3 (1.29) | 143.1 (1.17) | 129.3 (1.05) |
| UserID | 110.2 (0.93) | 118.0 (1.00) | 139.0 (1.18) | 135.0 (1.14) | 130.7 (1.11) | 120.1 (1.02) |
| RegionID | 14.8 (0.75) | 19.6 (1.00) | 19.3 (0.98) | 23.4 (1.19) | 24.8 (1.26) | 21.0 (1.07) |
| CounterID | 14.3 (0.75) | 19.1 (1.00) | 19.4 (1.02) | 18.2 (0.96) | 24.7 (1.29) | 20.6 (1.08) |
| TraficSourceID | 12.7 (0.89) | 14.3 (1.00) | 15.0 (1.05) | 15.8 (1.10) | 19.6 (1.37) | 15.5 (1.08) |
| AdvEngineID | 8.0 (0.59) | 13.7 (1.00) | 15.0 (1.09) | 13.4 (0.98) | 19.1 (1.40) | 15.0 (1.09) |

gcc:

| column | ClickHouse HashMap | absl::flat_hash_map | google::dense_hash_map | 4.1.2 | 5.1.0 | main |
|---|---|---|---|---|---|---|
| WatchID | 300.1 (0.82) | 366.8 (1.00) | 399.5 (1.09) | 454.0 (1.24) | 323.6 (0.88) | 383.0 (1.04) |
| URLHash | 125.1 (0.97) | 129.0 (1.00) | 143.5 (1.11) | 157.0 (1.22) | 129.2 (1.00) | 129.2 (1.00) |
| UserID | 112.4 (0.95) | 118.6 (1.00) | 129.6 (1.09) | 133.5 (1.13) | 120.4 (1.02) | 119.7 (1.01) |
| RegionID | 16.2 (0.88) | 18.3 (1.00) | 15.8 (0.86) | 25.5 (1.39) | 26.0 (1.42) | 20.1 (1.10) |
| CounterID | 16.3 (0.87) | 18.7 (1.00) | 15.4 (0.82) | 21.9 (1.17) | 21.6 (1.15) | 21.1 (1.13) |
| TraficSourceID | 16.1 (1.25) | 12.9 (1.00) | 10.9 (0.85) | 25.8 (2.01) | 16.2 (1.26) | 14.8 (1.15) |
| AdvEngineID | 10.1 (0.81) | 12.5 (1.00) | 10.2 (0.82) | 16.2 (1.30) | 15.7 (1.26) | 14.2 (1.14) |

Memory as reported by the benchmark, MiB (the same under both compilers):

| column | ClickHouse HashMap | absl | google dense | 4.1.2 | 5.1.0 | main |
|---|---|---|---|---|---|---|
| WatchID | 4096 | 2176 | 4096 | 2550 | 2230 | 2230 |
| URLHash | 1024 | 544 | 1024 | 572 | 492 | 492 |
| UserID | 1024 | 544 | 1024 | 525 | 445 | 445 |

Under gcc, WatchID (almost all new keys) is 18% slower on main than on 5.1.0, 383.0 against 323.6. That is #328's call after a home-group miss, which the shape search had put at 1.24x the best shape on this very column (see [The shape search](#the-shape-search-sixteen-combinations-of-what-the-insert-inlines-each-judged-by-its-worst-ratio-to-the-best-combination-anywhere-measured-with-the-rule-fixed-before-the-results-it-picks-one-shape-for-every-compiler----the-home-group-lookup-inlined-the-miss-path-and-the-walk-past-home-called----worst-111-under-clang-and-158-under-gcc-where-520s-everything-inlined-reads-332-and-354)). ClickHouse's own map is the fastest on the small columns under clang (0.59-0.89 of absl). Under gcc `google::dense_hash_map` is the fastest on three of the four (0.82-0.86), and ClickHouse's map is 1.25x absl on TraficSourceID. ClickHouse's map takes 1.9x absl's memory on the large columns (corrected 2026-10-02: said "the fastest on the small columns under both compilers (0.59-0.93 of absl)").

Unexpected, and filed on its own (#331): under clang the columns with few distinct keys (CounterID, AdvEngineID: counting into a table that stays in cache) are 12-13% slower on main than on 4.1.2, 20.6 against 18.2 and 15.0 against 13.4 cycles per row. Under gcc 4.1.2 is the slower one there.

What this says and does not say: one machine, 100M rows per column, in the benchmark's own harness. The published numbers are one run on AWS and are quoted as ratios only.

### #326's op-cache footprint was taken away by #328: MySQL's `EXCEPT` went from 26-45x 4.4.0's op-cache misses on 5.2.0 to 1.9x and 1.1x in two layouts of main, and the query is level with 4.4.0 or faster in both

*2026-09-27 · #326, #328 · info · MySQL 9.7.2, gcc 16 `-O2`, Ryzen 9 7950X; `perf stat` over five queries on the pinned mysqld, two rounds*

The issue's bar, within 2x of 4.4.0's op cache misses in every measured layout, is met without a change of its own. 5.2.0 inlined the whole insert, growth path included (`flatten`), into `MaterializeIterator::check_unique_fields_hash` (see [MySQL's `EXCEPT` read 7-11% slower on 5.2.0 than on 4.4.0](#mysqls-except-read-7-11-slower-on-520-than-on-440-and-it-is-where-the-linker-put-the-maps-code-an-inactive-block-added-next-to-the-call-took-it-to-1-and-removing-it-brought-the-7-11-back-with-do_find-and-hash_bytes-byte-identical-in-both-binaries) for the 26-45x). main inlines only the home-group lookup and calls the rest (#328), which is the change #326 asked for, from another direction. The second layout is the same source with the inactive `scripts/ab/mysql_except_dump.patch` applied, which moved 5.2.0's `EXCEPT` by 7-11% before. Per query:

| | op cache misses | L1 icache misses | cycles | instructions |
|---|---|---|---|---|
| 4.4.0 | 1.56 M | 0.45-0.46 M | 1137.7-1149.5 M | 4528.8 M |
| 5.2.0 (the two layouts of the entry above it, from that session: not comparable row by row) | 34.1-59.0 M | 0.56-4.54 M | 1126.5-1209.6 M | 4469.7 M |
| main, layout 1 | 2.97-3.03 M | 0.84-0.86 M | 1098.7-1115.1 M | 4481.1 M |
| main, layout 2 | 1.74-1.75 M | 0.54-0.55 M | 1096.3-1125.7 M | 4497.0 M |

**Correction (2026-10-02):** the 5.2.0 row is from the session of the entry above, whose 4.4.0 read 1.3 M op cache misses and 1119.2 M cycles. The 26-45x in the title is against that 4.4.0, and the 1.9x and 1.1x against this table's 4.4.0 (1.56 M). Against one baseline, 5.2.0 is 22-38x of 1.56 M, and main is 2.3x and 1.3x of 1.3 M, which misses the issue's 2x bar in layout 1. The cycles disagree the same way: 5.2.0's fast layout (1126.5 M) is below this table's 4.4.0, and was 0.65% above 4.4.0 in its own session. A clean ratio needs 4.4.0, 5.2.0 and main in one session; not re-measured.

What this says and does not say: two layouts of one build configuration. The experiment #326 proposed (keeping the growth path out of an inlined insert) was not run, since the insert is no longer inlined past the home group.

### STP's parser with 4.5.0 against 5.1.0 and main, the two instances from stp#560: 5.1.0 parses 4-5% faster on both instances under both compilers, with the same instructions within 1.3%, and 8% less peak memory on the larger instance; main reads within 3.1 points of 5.1.0

*2026-09-28 · #316, stp#560 · info · Ryzen 9 7950X, clang 22 and gcc 16; STP master 51fdb95b, Release, `stp --parse-only`, seven rounds with the variant order rotated per round, pinned to one core, `perf stat -e instructions:u,cycles:u`, medians; ranges are min-max of the seven*

In this program the gain from 5.x is cycles, not instructions: the same work with fewer stalls.

STP pins 4.5.0 by SHA-256 and patches it at configure time (`cmake/PrepareUnorderedDense.cmake`: a non-allocating same-allocator `swap`, and `stp_insertion_may_rehash()`). The 5.1.0 and main builds replace that generated header after configuring. They use the version's own header plus the same `stp_insertion_may_rehash()` (`bucket_count() == 0 || size() >= m_max_bucket_capacity`, the same growth rule in both). 5.x's own `swap` does not allocate, so the other patch has no counterpart. The instances come from the SMT-LIB 2025 release on Zenodo (record 16740866, `QF_BV.tar.zst`): `20230221-oisc-gurtner/AND-NESTED-32-32.smt2` (89 MB) and `asp/Labyrinth/laby_17_17_02.lp.smt2` (32 MB). stp#560's third number, ponylink's preprocessing, has no public instance and was not run.

| | wall, s | ratio | instructions | ratio | cycles | ratio | peak RSS, MB |
|---|---|---|---|---|---|---|---|
| clang AND-NESTED, 4.5.0 | 3.86 (3.84-3.92) | 1 | 40.10 G | 1 | 16.82 G | 1 | 584 |
| clang AND-NESTED, 5.1.0 | 3.70 (3.68-3.76) | 0.959 | 39.76 G | 0.991 | 16.14 G | 0.959 | 538 |
| clang AND-NESTED, main | 3.75 (3.72-3.79) | 0.972 | 40.08 G | 0.999 | 16.36 G | 0.973 | 538 |
| clang laby, 4.5.0 | 0.99 (0.98-0.99) | 1 | 8.64 G | 1 | 4.20 G | 1 | 336 |
| clang laby, 5.1.0 | 0.95 (0.94-0.96) | 0.960 | 8.62 G | 0.997 | 4.03 G | 0.957 | 348 |
| clang laby, main | 0.92 (0.92-0.95) | 0.929 | 8.71 G | 1.008 | 3.92 G | 0.933 | 348 |
| gcc AND-NESTED, 4.5.0 | 3.57 (3.53-3.62) | 1 | 42.20 G | 1 | 15.52 G | 1 | 584 |
| gcc AND-NESTED, 5.1.0 | 3.39 (3.38-3.44) | 0.950 | 42.26 G | 1.001 | 14.77 G | 0.952 | 538 |
| gcc AND-NESTED, main | 3.42 (3.40-3.51) | 0.958 | 42.35 G | 1.003 | 14.87 G | 0.958 | 538 |
| gcc laby, 4.5.0 | 0.97 (0.97-0.99) | 1 | 8.11 G | 1 | 4.15 G | 1 | 336 |
| gcc laby, 5.1.0 | 0.93 (0.93-0.94) | 0.959 | 8.21 G | 1.013 | 3.96 G | 0.953 | 348 |
| gcc laby, main | 0.94 (0.93-0.95) | 0.969 | 8.25 G | 1.017 | 3.98 G | 0.958 | 348 |

Where the map is in a parse: `perf record` of the clang AND-NESTED run puts 5.5% of the self time in out-of-line `ankerl::` symbols with 4.5.0 and 2.9% with 5.1.0 (`std::_Hashtable` 3.8% in both; STP keeps some std tables). Map code inlined into STP's functions is not in those figures, so they are a lower bound on the map's share. The top of the profile is STP's own `isRealTerm()` and `GetChildren()`, 23%.

What this says and does not say: the gain is 4-5% of a parse in which the map's visible share is a few percent, so most of what the map's code can give here, it gives. It does not separate the container from the hash caching stp#560 did at the same time; both builds carry that. It says nothing about solving, only parsing (`--parse-only`, as stp#560 measured). main against 5.1.0 is within the rounds' spread except clang's laby (0.929 against 0.960). The larger instance's 8% less memory fits the index's 5.5 bytes per slot against 4.5.0's 8, which was not checked. The smaller instance's +3.6% was not examined.

### stp#567's constant bit propagator tables re-checked on STP master: against the same four tables as std::unordered_*, 4.5.0 takes propagation to 0.912 and this repository's main to 0.859, nearly all of it on one instance (testcase15 0.74 and 0.69), and total solve time is level within 1% where #567 read -22% propagation and -4.9% total

*2026-09-28 · #316 (asked alongside), stp#567 · info · Ryzen 9 7950X, clang 22; STP master 51fdb95b, Release, `stp -s`, three rounds with the build order rotated per round, pinned to one core, medians; propagation time is the sum of the "After Constant Bit Propagation" phase times that `-s` prints*

The effect stp#567 reported has not survived into STP master on the files this screen found. On today's STP the tables #567 changed are a small part of solving, and a dense map's gain there is one instance's propagation, not the solve.

stp#567 (merged 2026-07-21 as 6b7390ff) moved three things from `std::unordered_*` to 4.5.0: the constant bit propagator's node-to-bits map, its worklist set and its dependents index. It reported propagation 15.1 to 11.8 s summed (-22%) and total solve time -4.9%, on the fifteen most propagation-heavy QF_BV files of six families (not named individually). Master has since rewritten two of those files (#738's flat parent lists, #879's cost-ordered worklist), so the change no longer reverts. The `std` build here switches master's four tables (`NodeToFixedBitsMap`, `WorkList`'s set, `Dependencies`' index map and seen set) back to `std::unordered_map`/`set` with the same hashers and changes nothing else. The `main` build is master with this repository's header plus STP's `stp_insertion_may_rehash()`.

Files: the fifteen that finish within 60 s with the most propagation time. They were screened with the 4.5.0 build from `stp/testcase15`, the 66 `bmc-bv-svcomp14` files, `vlsat3`, `picorv32`, and 80 random files each of Sydr and catchconv (SMT-LIB 2025, Zenodo 16740866). Fourteen of the fifteen are `vlsat3`; the heavier `vlsat3` files do not finish in 60 s.

| file | propagation ms, std / 4.5.0 / main | ratio to std | wall s, std / 4.5.0 / main |
|---|---|---|---|
| testcase15 | 1995 / 1471 / 1374 | 0.74 / 0.69 | 5.27 / 4.60 / 4.28 |
| 14 vlsat3 files, each | | 0.99-1.08 / 0.94-1.02 | |
| sum of all 15 | 5117 / 4667 / 4393 | 0.912 / 0.859 | 162.5 / 162.9 / 161.1 |

Answers (sat/unsat) agree across all 135 runs. The hazard #567 fixed is an iterator or reference into the table held across an insert, which a dense map invalidates and `std::unordered_map` does not. Reading master for it found nothing: `simplify_during_bb` holds `FixedBits*` since #567, the two loops over the whole map insert only into other maps, and every other access takes the pointer out of the iterator before anything inserts.

What this says and does not say: the file set is not #567's (fourteen `vlsat3` against six families), so this does not contradict #567's numbers on its own files at its own commit. main is 0.941 of 4.5.0's propagation over the fifteen, consistent with the parser's 4-5% on #316. Not measured: gcc, and #567's own commit.

### Redpanda's leader balancer benchmark (redpanda#17182) re-run standalone against main: main's `map` is 3-10% slower than 4.5.0 on the benchmark as written, whose map holds 80 keys, and 15-20% slower with one entry per raft group as in a real cluster, where it executes 20-27 more instructions per lookup; the `segmented_map` Redpanda ships is level (clang 0.917, gcc 1.042)

*2026-09-28 · #317, redpanda#17182 · open · Ryzen 9 7950X, clang 22 and gcc 16, abseil master; `scripts/ab/redpanda_lb.sh 4 30` over `redpanda_lb.cpp`, v4.5.0 against main (272e1f1), one binary per header and compiler, the six containers as template parameters in each, 30 rounds with the order rotated, 4 alternations of the binaries, medians of the alternations' medians, pinned to one core*

`redpanda_lb.cpp` is `lb.random_generator` from Redpanda's `leader_balancer_bench.cc` without Seastar. It constructs `random_reassignments` (as of redpanda#17182) from a cluster index of 72 nodes x 16 shards x 80 groups x 3 replicas, then makes 184320 `generate_reassignment()` calls. Each call is a random index (`uniform_int_distribution`, a 64-bit division), a swap in a 4.4 MB replica vector, and one lookup that hits. What differs from Redpanda's code: a flat index instead of `node_hash_map<shard, btree_map<group, replicas>>`, `std::vector` instead of `fragmented_vector`, `std::mt19937_64`, and `segmented_map` on its own `segmented_vector` rather than Redpanda's `chunked_vector`. Redpanda pinned a fork labelled 4.4.0 that differs from v4.5.0 in 23 lines; v4.5.0 stands for it here.

Redpanda's test helper numbers the groups 0..79 on every shard, so the benchmark's `_current_leaders` holds **80 keys** after 92160 assignments. A real cluster gives every raft group its own id, which `-DUNIQUE_GROUPS` does: 92160 entries.

| ms, v4.5.0 / main | 80 keys, clang | 80 keys, gcc | 92160 keys, clang | 92160 keys, gcc |
|---|---|---|---|---|
| `std::map` | 8.861 / 8.863 | 10.256 / 10.256 | 35.25 / 35.49 | 45.72 / 46.03 |
| `absl::flat_hash_map` | 5.828 / 5.835 | 6.712 / 6.723 | 6.562 / 6.447 | 7.494 / 7.343 |
| `absl::node_hash_map` | 5.948 / 5.953 | 6.779 / 6.768 | 10.67 / 10.64 | 11.66 / 11.59 |
| `absl::btree_map` | 9.950 / 9.938 | 10.120 / 10.113 | 24.20 / 24.16 | 23.39 / 23.34 |
| `map` | **5.484** / 6.053 | **6.514** / 6.736 | **5.728** / 6.604 | **5.938** / 7.093 |
| `segmented_map` | 6.007 / 6.283 | 6.656 / 6.760 | 7.604 / 6.973 | 7.107 / 7.408 |

The four other containers are the control. They are the same code in every binary and read within 0.2% (80 keys) and 2.0% (92160 keys) between the two.

Where `map`'s time goes, `main` against 4.5.0 (`perf stat`, one container per run, instructions and cycles per operation; a copy that skips the loop separates the two phases):
- 80 keys: +8 instructions per `operator[]` in construction, +19 per `generate_reassignment()`; +25% cycles and 2.3x branch misses overall.
- 92160 keys: construction +51 (clang) / +72 (gcc) instructions and +10 / +24 cycles per fresh insert, growth included; the loop +27 / +20 instructions and +16 / +14 cycles per call. Branch misses equal.
- The loop's hit, read in clang's disassembly: the group probe (fingerprint-word table load, broadcast, 16-byte compare, movemask, tzcnt, index load, key compare, lane loop) against 4.x's scalar compare on its first bucket. Around it is the #310 spill pattern: the `end()` comparison and six of the loop's values reloaded from the stack every call, and `mt19937_64`'s tempering constants moved between registers and rematerialized. The hash is not it: the 80 keys spread 9-11 per group.

What this says and does not say:
- 4.5.0's `map` beats every container here, and the ranking Redpanda measured holds for 4.5.0 (`map` ahead of absl's two, `segmented_map` level with them). main's `map` falls to level with `absl::flat_hash_map` (ahead under gcc at 92160, behind under clang).
- It is instructions, not memory: at 92160 entries the whole working set is about 7 MB (4.4 of it the replica vector), inside the L3, and the extra cycles are fewer than the extra instructions.
- The caller is the one #310's corpus was built to catch: a loop holding an RNG's state with a division and a store to a random address on the way to the next key. The shape search's fix (the placement out of line) does not reach a lookup: `find` has no placement, and its whole hit path is inline.
- For Redpanda, which ships `segmented_map`, main is level at the size a cluster has (0.917 clang, 1.042 gcc).
- Not established: which part of the +20-27 instructions is the group probe itself and which the spills. The scored benchmark, where main's hits are 1.33x faster than 4.11.0's, runs its lookups in a loop that holds little else, so the caller's shape is the likely difference, not a measured one. Not run: Redpanda's own build, and the `_frag` variants on `fragmented_vector`.

**Later (2026-09-28):** #341 split the +20-27 instructions: half is the group probe itself (+11-14 in a bare loop) and half the caller around it, see [#341, main's `find` hit against 4.5.0's in Redpanda's loop](#341-mains-find-hit-against-450s-in-redpandas-loop-half-of-the-20-27-instructions-per-call-is-the-group-probe-itself-11-14-in-a-bare-loop-and-half-the-caller-around-it-boosts-unordered_flat_map-runs-as-many-instructions-and-9-16-fewer-cycles-in-that-loop-and-three-candidates----the-walk-past-home-out-of-line-no-index-prefetch-a-speculative-value-prefetch-from-a-preferred-lane----each-cost-cycles-or-did-not-move-them-so-nothing-changed).

**Later (2026-09-28, #346):** the keys here are dense ids. With keys 0..n-1 a multiplicative hash leaves 4.x's robin hood probe collision-free, so it never walks and never mispredicts. With scrambled integer keys 4.5.0 mispredicts 0.6-1.1 branches per lookup and main is 1.8-2.3x faster at the same sizes (corrected 2026-10-02: said "2.2x"). See [Hits in cache-resident tables](#hits-in-cache-resident-tables-1000-to-16000-entries-450-against-main-346-main-takes-041-061-of-450s-time-on-scrambled-integer-keys-and-071-079-on-strings-from-a-quiet-loop-and-a-busy-one-and-102-143x-of-it-on-dense-ids-0n-1-which-a-multiplicative-hash-places-without-collisions----450s-best-case-and-the-keys-redpandas-and-osrms-callers-have).

### Valhalla's CostMatrix (valhalla#4552) re-run with the map Valhalla ships (4.5.0), main and absl: all three within 1.1% on every row, as the original found ("all in all it hardly matters"), because the map is a few percent of a matrix request (`ReachedMap::add` 3.6%, out-of-line map code 0.36%)

*2026-09-28 · #318, valhalla#4552 · info · Ryzen 9 7950X; Valhalla master 76cd51059 built with gcc 13.3 in an Ubuntu 24.04 container, rootless podman; `scripts/ab/valhalla/`, which has the steps*

Neither main nor absl moves CostMatrix. A faster map could move it by at most a few percent.

Setup as in the original comment: request files of 20 random `sources_to_targets` requests each, auto costing, inside Berlin's bounding box or the Mannheim-Usedom box the comment links. A long-running `valhalla_service` with one worker, pinned to two cores, gets a warm-up request, and each file is timed as a whole. Four rounds with the variant order rotated, medians. Tiles for all of Germany (Geofabrik, built in 517 s). The `absl` variant replaces both of CostMatrix's containers with `absl::flat_hash_map`/`flat_hash_set` (Ubuntu's abseil 20220623): `ReachedMap`'s `pmr::map<uint64_t, pmr vector>`, keeping its pmr allocator, and the `unfound_connections` set. The rest of Valhalla keeps 4.5.0 in that build. Each binary was checked for its container's symbols.

The Mannheim-Usedom box reaches into Poland and Czechia. With random points 14 of 20 requests failed with error 170 ("Locations are in unconnected regions"), because one bad point of 40 fails the whole request. Its file is built from points that route to Kassel instead (800 of 837 drawn), and all 60 requests then succeed. The matrix distance cap is raised to 2000 km; the default 400 km is shorter than the box.

| seconds per file of 20 requests | 4.5.0 (as shipped) | main | absl |
|---|---|---|---|
| Berlin, 20 sources/targets | 13.04 (13.02-13.10) | 13.00 (12.97-13.04), 0.997 | 13.03 (13.01-13.06), 0.999 |
| Berlin, 50 sources/targets | 46.74 (46.35-46.98) | 47.23 (46.79-47.97), 1.011 | 46.25 (45.80-46.43), 0.990 |
| Mannheim-Usedom, 20 sources/targets | 30.54 (30.49-30.62) | 30.40 (30.26-30.50), 0.995 | 30.43 (29.91-30.81), 0.996 |

(min-max over the four rounds; ratios to 4.5.0.) The original, 2024, on robin-hood-hashing as master: 36 / 118 / 210 for master, 39 / 124 / 215 for `map`, 38 / 126 / 220 for `segmented_map`, 36 / 119 / 215 for absl. That was other hardware, other data and other requests, so only the ranking compares.

Where the time is, `perf record` of main's service on the Berlin 50 file: `SourceToTarget` 7.5%, `CheckConnections` 13.4%, `Expand`/`ExpandInner` 17.9%, `GetAstarHeuristic` 7.9%, the tile reads and edge costs most of the rest. `ReachedMap::add` (a find, an emplace on a miss and a `push_back`, the map inlined) is 3.6%; everything out of line in `ankerl::` is 0.36%. The map's inlined finds inside `CheckConnections` are not separable in this profile.

What this says and does not say: the rows are level within their own spread (main's Berlin 50 reads 1.011 with a range of 46.79-47.97 against 46.35-46.98). One compiler (gcc 13 in the container), one machine, one data snapshot. `segmented_map` was not re-run: Valhalla uses `map`.

### OSRM's e2e benchmarks (osrm-backend#6922) re-run on a quiet, pinned core with the 4.4.0 #6922 tried and with main: both take map matching to 1.21-1.27x the requests per second of `std::unordered_map`, CH table to 1.10x and CH trip to 1.05x, and main is not ahead of 4.4.0 -- equal on most rows, behind on CH match (1.255 against 1.274) and MLD table (0.986 against 1.009)

*2026-09-28 · #319, osrm-backend#6922, #341 · info · Ryzen 9 7950X; OSRM master 214ba8f9f, Release with LTO, gcc 13.3 in an Ubuntu 24.04 container, dependencies from vcpkg; `scripts/ab/osrm/`, which has the steps*

#6922 changed one line: `UnorderedMapStorage` in `include/util/query_heap.hpp`, the node index of the query heaps the search engine keeps per thread. It went from `std::unordered_map` (with `rehash(1000)` in its constructor) to `segmented_map` (without it), and vendored this map's 4.4.0. OSRM declined it and still ships `std::unordered_map`.

Five builds, all in one session: as shipped, and `segmented_map` and `map` each with 4.4.0 and with main. Each binary was checked for its version's symbols. Data as OSRM's CI had it: Berlin from Geofabrik, prepared for CH and MLD, and `test/data/berlin_gps_traces.csv.gz`. Load: OSRM's own `scripts/ci/e2e_benchmark.py`, seeded, 50 warm-ups and 1000 requests per method, against a fresh `osrm-routed -t 1` on core 2 with the client on core 3. Five rounds, build order rotated, medians, ratios to std with the rounds' min-max. The client's rate includes Python's own overhead (about 2 ms a request, most of `route` and `nearest`), so the server-side time is also summed per method from `osrm-routed`'s log.

| | std, requests/s | 4.4.0 `map` | main `map` | 4.4.0 `segmented_map` | main `segmented_map` | server time, 4.4.0 / main `map` |
|---|---|---|---|---|---|---|
| CH route | 544.4 | 1.017 (1.011-1.018) | 1.012 (1.005-1.015) | 1.015 (1.013-1.019) | 1.011 (1.011-1.014) | 0.969 / 0.972 |
| CH nearest | 1920.0 | 1.000 (0.936-1.008) | 1.002 (0.998-1.011) | 0.995 (0.959-1.001) | 0.997 (0.987-1.005) | 0.997 / 0.994 |
| CH trip | 123.5 | 1.052 (1.047-1.058) | 1.049 (1.044-1.056) | 1.052 (1.049-1.055) | 1.051 (1.048-1.053) | 0.934 / 0.937 |
| CH table | 319.2 | 1.099 (1.097-1.104) | 1.096 (1.093-1.098) | 1.101 (1.097-1.102) | 1.095 (1.092-1.096) | 0.889 / 0.893 |
| CH match | 55.1 | 1.274 (1.272-1.278) | 1.255 (1.248-1.262) | 1.253 (1.250-1.258) | 1.209 (1.199-1.211) | 0.767 / 0.781 |
| MLD route | 570.8 | 1.001 (0.994-1.006) | 1.003 (1.000-1.008) | 1.003 (1.001-1.006) | 1.002 (0.997-1.005) | 0.990 / 0.988 |
| MLD nearest | 1915.3 | 1.000 (0.985-1.006) | 1.003 (0.998-1.006) | 0.999 (0.996-1.001) | 1.002 (0.979-1.013) | 0.999 / 1.001 |
| MLD trip | 108.9 | 1.003 (1.000-1.010) | 0.994 (0.984-1.000) | 1.013 (1.005-1.015) | 0.997 (0.989-0.999) | 0.995 / 1.006 |
| MLD table | 193.8 | 1.009 (1.006-1.011) | 0.986 (0.978-0.988) | 1.012 (1.012-1.017) | 0.978 (0.976-0.982) | 0.991 / 1.016 |
| MLD match | 108.5 | 1.248 (1.031-1.254) | 1.263 (1.255-1.268) | 1.233 (1.230-1.242) | 1.225 (1.217-1.229) | 0.769 / 0.756 |

What this says and does not say:
- Against `std::unordered_map` both versions win where the query heap's node index is busy. Matching gains a fifth to a quarter of its server time on both algorithms, CH table a tenth, CH trip 6%. #6922's CI read +16% (CH) and +10% (MLD) matching on a runner its authors called jumpy; on a pinned core the gain is larger.
- main against 4.4.0 is level or behind: CH match 1.255 against 1.274 (`map`) and 1.209 against 1.253 (`segmented_map`), MLD table 0.986 against 1.009 and 0.978 against 1.012, MLD trip slightly behind. main is ahead only on MLD match with `map` (1.263 against 1.248). This is #341's case: a per-query table that is small, stays in cache and is mostly hit, where 4.x's robin hood hit is cheaper than the group probe, and the group index's gains (misses, tables past the cache) do not come into play.
- `route` and `nearest` are level for every build. The client's overhead dominates them, and the server times agree.
- A first run the same day, with std, main `segmented_map` and main `map` only, read the same main `map` ratios within 0.01 on every row except CH table and trip (1.071 / 1.029 then, 1.096 / 1.049 here). std's own rate differed between the two sessions (CH table 414.6 and 319.2 requests/s), which is why only ratios within one session are quoted.
- One compiler (gcc 13), Berlin only (#6922's CI also had a larger Poland run), one machine. MLD's table was not profiled.

**Later (2026-09-28, #346):** the keys here are dense ids. With keys 0..n-1 a multiplicative hash leaves 4.x's robin hood probe collision-free, so it never walks and never mispredicts. With scrambled integer keys 4.5.0 mispredicts 0.6-1.1 branches per lookup and main is 1.8-2.3x faster at the same sizes (corrected 2026-10-02: said "2.2x"). See [Hits in cache-resident tables](#hits-in-cache-resident-tables-1000-to-16000-entries-450-against-main-346-main-takes-041-061-of-450s-time-on-scrambled-integer-keys-and-071-079-on-strings-from-a-quiet-loop-and-a-busy-one-and-102-143x-of-it-on-dense-ids-0n-1-which-a-multiplicative-hash-places-without-collisions----450s-best-case-and-the-keys-redpandas-and-osrms-callers-have).

## The robin hood index before 5.0, and its dead ends

The robin hood index this replaced was removed on 2026-09-05. It had a packed distance-and-fingerprint field per bucket, a four-bucket SSE2 probe, and vector shifts on insert and erase. None of the dead ends below can be re-run against the current header. They are kept because the measurements are real, the reasoning applies to anything that probes a flat array, and the same questions will be asked of the group index. The section also holds one measurement of how far a year of work moved the map from 4.8.1, the scalar robin hood of 1 January 2026.

**Where it stands** (as of 2026-09-06)

- Against 4.8.1 (scalar robin hood), the score geomean is 1.467 under clang and 1.434 under gcc. The year removed the worst case more than it sped up the best: 4.8.1 swings 1.26-1.59x over an octave against 1.04-1.14x for the group index before 5.0.0 ([Where a year of this got to, measured against 4.8.1](#where-a-year-of-this-got-to-measured-against-481)).
- The string hash was 4-7% slower than 4.8.1's (`hashstr` 0.96 clang, 0.93 gcc) ([Where a year of this got to, measured against 4.8.1](#where-a-year-of-this-got-to-measured-against-481)). The hash restructured the same day is faster than 4.8.1's on both compilers ([The string hash restructured](#the-string-hash-restructured-independent-blocks-from-17-to-144-bytes)).
- A 50% hit mix is the least discriminating lookup benchmark: it can order maps differently from both pure cases, which is why `find_hits_vs_size` exists beside `find_vs_size` ([Where a year of this got to, measured against 4.8.1](#where-a-year-of-this-got-to-measured-against-481)).
- On the robin hood index, these regressed and were reverted: force-inlining the hash, a branchless `do_find`, explicit prefetches in `do_find` and `do_erase`, rapidhash (only its short-input reads adopted), an AES-NI gxhash port, several SIMD probe variants, scalar branch removal in `place_and_shift_up`, and four rehash-loop rewrites; 32 stored hash bits per value gained 1.4% and was rejected on cost, 4 bytes per element ([Optimization dead ends (verified with paired A/B runs; ...](#optimization-dead-ends-verified-with-paired-ab-runs-re-test-before-assuming-they-still-hold)).

### Optimization dead ends (verified with paired A/B runs; re-test before assuming they still hold)

*undated list; bullets dated 2026-09-02 and 2026-09-03 · rejected · paired A/B runs against `bench_quick_overall_udm`, on the robin hood index*

The `bench_quick_overall_udm` hot paths are close to machine limits. These ideas consistently **regressed** and were reverted:

- Force-inlining `wyhash::hash` into the map. Instruction cache and register pressure outweigh the saved call overhead.
- A branchless `do_find` fast path for scalar keys (unconditional key compare plus a conditional-move result). The speculative value load doubles cache misses on the ~50% of lookups that miss.
- An explicit `__builtin_prefetch` of `m_values[bucket->m_value_idx]` in `do_find`, and in `do_erase` computing the moved element's hash early and prefetching its home bucket. Out-of-order execution already hides these latencies.
- Replacing wyhash with rapidhash (v3, 2025). The wyhash implementation here is *faster* for inputs ≥ 24 bytes, in both latency and throughput. rapidhash only won at ≤ 16 bytes, and its trick there has been adopted: two plain 8-byte reads instead of building `a`/`b` from four 4-byte reads.
- An AES-NI hash (a port of gxhash, compiled with `-maes`, no dispatch): 30% *slower* than this wyhash on 200 byte keys and 27-55% slower in the map. Its serial `aesenc` chain has worse latency, and latency is what the string loop pays for. Fewer uops do not help a chain. **Later (2026-09-07):** a different AES hash on the scored keys (8-135 bytes) was 28% faster at hashing and still 9-37% slower in the map, see [Latency is what a map pays, tested rather than argued](#latency-is-what-a-map-pays-tested-rather-than-argued).
- SIMD probe variants:
  - An *aligned* group of four (1-4 lanes visible from home) left the "nothing decided, next group" branch random and won nothing.
  - Deciding hit-vs-miss with two branches (scalar home probe, then the vector for the rest) mispredicted *more* than one vector decision (0.90 vs 0.70 per lookup), although each branch is more biased.
  - Keeping the key comparison inside the vector loop made the compiler spill the xmm state around `bcmp` on every string hit.
- Storing the top 32 hash bits per value, so an erase never re-hashes the moved element. Only the successful half of the erases move one, so the bound is ~7% of `iestr`. It measured 1.4%, for 4 bytes per element and a second container to keep consistent.
- Caching the bucket data pointer in the shift loops. The compiler already hoists it.
- Two attempts at the rehash, measured against `build64` and `buildstr` (2026-09-02):
  - Skipping the `memset` in `clear_and_fill_buckets_from_values`, which is redundant because `allocate_buckets_from_shift` hands back a freshly zeroed vector: 0.7% on `build64`, ~1% *worse* on `iestr`.
  - Hashing eight elements ahead in the rehash loop and prefetching the bucket each will land in: nothing on `build64`, ~1% worse on `buildstr`.
  - The redundant memset was removed on the group index on 2026-09-05 as a simplification, not a speedup. Paired on the score it is within 1% everywhere, so the ~1% on `iestr` above was layout luck.
- Scalar attempts to take the branch out of `place_and_shift_up`. The robin hood shift asks "is this bucket occupied" once per bucket, and the answer is a coin flip (73% of inserts shift nothing, 11% shift one), which cost 0.61 mispredictions per insert. Settling the first two buckets with conditional moves and one combined test does not help. Written as `a == 0 || b == 0` it is still two branches. Written as a bitwise or it is one branch that mispredicts just as often (0.608 per insert, ten more instructions).
  - What did work is the vector version that shipped until the robin hood index was removed (2026-09-05), which settles four buckets from one mask: the same trade the probe made, and the same again for the shift down on erase.
  - A two bucket vector version was also tried and lost to it (49.7 against 46.3 cycles per insert): its second slot is still a branch.
  - Both vector shifts first read as ~1% *losses* on `ie64`. That was the benchmark: its integer keys hashed to a lattice with no chains to shift (see [Integer keys must not be small sequential values.](#integer-keys-must-not-be-small-sequential-values)). With honest keys they are 1.07x.
- Four attempts at the rehash loop (2026-09-03), and why they all failed. The loop in `clear_and_fill_buckets_from_values` costs **~21-26 cycles per element at every size from L1 to L3** (4.4 ns at 1000 elements, 5.4 ns at 200000). Memory latency is already hidden. What bounds the loop is an in-core chain through the bucket array: each element's loads sit behind the previous element's stores.
  - Two experiments pin that down. Adding 11 cycles of artificial latency between the probe load and the store address adds 17 cycles per element. Reading four elements' home buckets before storing any of them is 1.5x faster while the array is L1-resident, and nothing at 100000, where the score's rehashes run. Nothing that shortens the *data* side of the store moves it at all.
  - So for this loop, anything that puts a load result on the path to the store address is a large loss, and anything that only saves instructions or mispredictions is invisible. That is also why the earlier prefetch-eight-ahead and memset attempts found nothing.
  - What was tried, against `HEAD` on the isolated loop and paired on `build64`: (1) vectorizing `next_while_less` with the probe's four-lane window: `build64` **1.41x slower**, the rehash 2-3x per element even in L1, with fewer mispredictions, 7% more instructions, and the store address now waiting for the window load; (2) the four-element batch above: neutral at scored sizes, worse at a million; (3) refilling from the *old* bucket array in order, so new homes arrive nearly sorted: **2x slower** at 100000-200000, with or without prefetching the keys, because sorted arrival makes every element probe the bucket the previous one just wrote; (4) replacing the `tzcnt`-indexed blend tables of both vector shifts with a vector prefix-or: rehash neutral, erase 6% slower, because the table loads were never on the chain.
  - Do not read `ls_bad_status2.stli_other` as a cost here. `build` shows 1.5 of them per insert and `churn` 1.3, on bucket-window loads that cannot overlap the previous insert's stores (odds ~3e-5). Shifting the stack of the same binary moves the count from 1.45 to 2.85 per insert while the cycles stay at 98.0 +- 0.5.
  - Placing the bucket before appending the value, to put distance between those stores and the next probe's loads, cost 2-7 cycles per insert in every variant.

### Where a year of this got to, measured against 4.8.1

*2026-09-06 · info · `scripts/ab/run.sh -r 3234af2 -b all 12`; 3234af2 is the revision `main` stood at on 1 January 2026: scalar robin hood, no vector probe anywhere*

In this entry "main" is c3b4667 (4.11.0, the robin hood index with the four-bucket SSE2 probe), and "this map" is the group-index branch at 7503850, released as 5.0.0. Today's main is neither.

Score geomean **1.467 under clang and 1.434 under gcc**, and 1.60 and 1.57 without the three iteration workloads. `build64` 2.60, `churn64` 2.16, `rhit64` 2.16, `churnbig` 1.99 and `rmiss64` 1.98 lead it. Iteration is at 1.00-1.08, which nothing this year touched.

Two results in that run matter more than the geomean. Both are in `scripts/ab/README.md` with their evidence.

The **string hash is 4-7% slower than 4.8.1's**: `hashstr` 0.96 clang, 0.93 gcc, with the gcc interval excluding parity. Boost's row moves with the candidate, which is the control that says it is the hash rather than the map. The suspicion is that July's wyhash work was tuned while every benchmark string was 200 bytes long; it is not confirmed. **Later (2026-09-06):** the restructured hash closed this the same day in the other direction: 2.00 against 4.8.1's 2.19 ns throughput and 7.69 against 9.14 ns latency under clang, and faster under gcc too, see [The string hash restructured](#the-string-hash-restructured-independent-blocks-from-17-to-144-bytes).

And **the lookup gain is mostly at high load**. One map per binary so nothing shares a translation unit, 20M unreplayed all-hits lookups each, ns per `find()`:

| load | entries | 4.8.1 | main (4.11.0) | this map (group index) |
|---|---|---|---|---|
| 0.50 | 33000 in 65536 buckets | 7.21 | 6.28 | **4.94** |
| 0.50 | 132000 in 262144 buckets | 8.52 | 8.47 | **7.01** |
| 0.79 | 52000 | 14.26 | 8.79 | **5.95** |
| 0.79 | 208064 | 16.98 | 10.50 | **7.62** |

So 4.8.1 is 1.01-1.15x of main at the empty end (corrected 2026-10-02: said "level with main") and 1.6x behind at the full end, and 1.2-2.4x behind this map throughout.

**A 50% hit rate is not the average of its parts and can order the maps differently from both.** Paired, one binary, 101 epochs, ms per 200000 lookups at 33000 entries (load 0.50):

| | main (4.11.0) | this map (group index) | 4.8.1 |
|---|---|---|---|
| all hits | 1.367 | **1.089** | 1.552 |
| all misses | 0.811 | **0.649** | 0.650 |
| 50% hits | 2.037 | 1.753 | **1.725** |

This map is fastest on each pure case and loses the mix by 1.6%. An unpredictable outcome costs a clean probe a fresh half misprediction per lookup (0.026 to 0.545). It costs a probe that already mispredicts 0.6 times on every hit almost nothing (0.599 to 0.605). Making the harness's own hit-or-miss select branchless moves none of it, so the branch is the map's own "did I find it".

The whole curve shows how narrow the window is. At 33000 entries, us per 200000 lookups:

| hits | 0% | 10% | 25% | 50% | 75% | 100% |
|---|---|---|---|---|---|---|
| this map | 666 | 975 | 1314 | 1736 | 1437 | 1125 |
| 4.8.1 | 782 | 995 | **1275** | **1729** | 2013 | 1769 |

Every map peaks at 50%, where the outcome is least predictable, so that point is the least discriminating of the three: this map leads by 1.57x at 100% hits and by 1.00x at 50%. At load 0.79 it wins at every hit rate by 1.3-2.1x. So the mix says nothing about which lookup is faster. `find_vs_size` plots the mix, which is why `find_hits_vs_size` exists beside it.

On `find_hits_vs_size`, **4.8.1 is the slowest of the four maps at 164 of 193 sample points**. It is behind this map at every size below 440000, 2.86x behind it at 3251 entries and load 0.79, and it swings 2.05-2.35x across an octave against this map's 1.07-1.28x. The eleven points where it leads are all above 440000 entries, where everything is waiting on memory.

`doc/find_vs_size.svg` carries 4.8.1 as a fourth line and is the picture of that. Over one octave 4.8.1 swings **1.26-1.59x** between its cheapest and dearest point, against 1.11-1.28x for main, 1.04-1.14x for this map and 1.10-1.22x for boost. Point by point, 4.8.1 runs from 0.91x of this map just after a doubling to 1.30x just before one. The year did not make the best case much faster, it removed the worst case.

## Tooling: tests, mutation testing, fuzzing, CI, offline builds

The long form of the tooling rules that `CLAUDE.md` compresses, titled "Tooling, in the detail the rules were compressed from" in the source: unit tests, the mutation tool, the fuzzers, the CI legs, offline builds. The numbers were measured, and they explain choices that otherwise look arbitrary. Take away two things: a `compiler` verdict means the question was never asked, and the unity CI leg must be reproduced whenever a test file is added or removed.

**Where it stands** (as of 2026-09-07)

- Every header change must pass `./builddir/clang_release/test/udm-test`. Every leg of `main.yml`'s main matrix reproduces with `meson setup builddir --force-fallback-for=fmt -Dcpp_std=c++<matrix cpp_std, default 17> <matrix setup_args> && meson test -C builddir --print-errorlogs <matrix test_args>` ("CI").
- `scripts/mutate/mutate.py` puts a named bug back (`--replace`, `--bugs`, `--reverse`) or sweeps (`--diff`, `--lines`, `--operators`); one mutant is ~100 CPU-seconds. `mutate_core.py` is vendored byte-identically into nanobench and oans: change it here, run `scripts/test_mutate.py`, copy, update all three `.sha256` ([The tool is two files](#mutation-testing)).
- One stale block makes the tool refuse a whole bug file: five stale blocks left none of the other 41 in `invariants.txt` checked. Re-run 2026-09-27 (#314) after the blocks went stale again: `invariants.txt` 49 mutants, 41 caught by a test, 5 hung, 1 oom, 2 compiler, none survived; `erase-path.txt` 6 mutants, 5 caught, 1 hung. Check the verdict, not just that a block applies ([Mutation triage after the bound](#mutation-triage-after-the-bound)).
- `fuzz_group_index` is the only fuzz target that reaches the index's structure; with the probe bound removed it re-finds, within seconds, the unbounded miss (found in review) that survived the other targets and 767 unit tests. Keep its `cb8d5c38...` input, which coverage minimization drops. The committed corpus goes *second* ("Fuzzing").
- A unity-build collision surfaces when the chunking shifts, not when it is written. Reproduce `--unity=on --unity-size=16` whenever a test file is added or removed ([A collision surfaces when the chunking changes](#ci)).
- Pinned linters: `clang-tidy-18` and `clang-format` 21; `lint-clang-format.py` skips without 21 ("CI").

### Testing

Any change to `include/ankerl/unordered_dense.h` must pass the unit tests:

```sh
meson test -C builddir/clang_release unit --verbose
# or directly (runs all non-skipped tests):
./builddir/clang_release/test/udm-test
```

### Mutation testing

*method · operator table at 4.9.1 · `scripts/mutate/mutate.py`, `scripts/mutate/mutate_core.py`, `scripts/test_mutate.py`*

Coverage says a line ran. `scripts/mutate/mutate.py` says whether anything would notice it misbehaving: it breaks the header, rebuilds, runs the suite and asks whether anything went red. What nothing notices is a test hole. Every build happens in a throwaway copy; the working tree is never touched.

**The tool is two files, and one of them is shared with nanobench and oans.** `mutate_core.py` holds everything not tied to one project (lanes, mutants, baseline discipline, verdicts, report), and both repositories hold a byte-identical copy. `mutate.py` is this project's adapter: where the header is, that meson builds it, that the binary is `udm-test`, that a lane needs `FUZZ_CORPUS_BASE_DIR`, and the measured constants behind `--dry-run`. Roughly 2900 shared lines against 100 of adapter (corrected 2026-10-02: said "1900").

The core holds three build systems and two test runners; only meson + doctest run here. cmake is nanobench's. `make` + minunit is oans's, a C project whose suite is one `minunit` binary. Hence `Backend.build_argv` takes the whole argument namespace (make remembers nothing, so `--make-arg CC=clang` rides on every build line), and a `Harness` with no filter arguments is not offered the `--test-filter` flags: a flag accepted and ignored would run the whole suite while the fingerprint claimed otherwise.

So `scripts/test_mutate.py` covers code *all three* repositories run, including the cmake and make backends and the minunit harness this project never executes. A core change is: edit here, run that suite, copy the file into the other two, record the new hash in all three:

```sh
sha256sum scripts/mutate/mutate_core.py                    # write it into mutate_core.sha256
cp scripts/mutate/mutate_core.py ../nanobench/src/scripts/mutate/   # ... and into its .sha256 too
cp scripts/mutate/mutate_core.py ../oans/scripts/mutate/           # ... and that one's
```

`lint-mutate-core.py` fails if this copy changed without its hash. That is the one failure vendoring adds: a local fix here leaves the other repositories running untested code. Compare the `.sha256` files to check sync; no lint in one repository can see the others.

**A named bug.** The everyday use is putting a *specific* bug back, which decides whether a new test earns its place. Bugs worth keeping live in `scripts/mutate/bugs/`:

```sh
scripts/mutate/mutate.py --replace OLD NEW               # one, must match exactly once
scripts/mutate/mutate.py --bugs scripts/mutate/bugs/erase-path.txt
scripts/mutate/mutate.py --reverse HEAD                  # undo a fix, keep today's tests
```

A block whose replacement is *meant* to contain the original (an inserted call, an early return in front of code that stays) needs `<<< additive` on its fence. Without it the block is refused: the code under test would not change, and `caught` or `SURVIVED` would be a verdict about nothing. The check came from woswoar, where three such blocks shipped in one session. The one legitimate case in these two repositories is a nanobench bug accepting a `-` sign in front of a digit check that stays.

**Sweeps** look for holes nobody thought of, one change at a time. The modes compose; ask a change both questions at once:

```sh
scripts/mutate/mutate.py --diff                          # whatever is uncommitted
scripts/mutate/mutate.py --diff HEAD~1                   # only what that change touched
scripts/mutate/mutate.py --lines 1278-1290,1400 --dry-run
scripts/mutate/mutate.py --bugs bugs.txt --lines 1278-1290 --reuse
```

`--diff` is the everyday mode. It measures from the merge base, so a branch behind main does not sweep what main changed.

`--dry-run` reports a *range*. The per-mutant constants describe a mutant that **compiles**; one the `-fsyntax-only` pre-filter rejects costs about a tenth.

**Correction:** `--dry-run` used to print one figure, which read high by an order of magnitude where most mutants are invalid: the `negation` sweep in the table was estimated at 11 minutes and took **51 seconds**. Which end applies depends on operator and code, not machine, so it is knowable in advance.

**Operators.** `--operators` picks what to change; the default is all, right for `--diff`, where cost scales with the lines touched. Over the whole header that is ~1600 mutants and about an hour, 66 min summed over the four operators at 4.9.1 (corrected 2026-10-02: said "about an hour and a half"), so a full sweep should name one operator. That is why they have names.

Swept over the whole header at 4.9.1. Re-measure before relying on it: one header, one commit, and kill rates move with every added test.

| operator | mutants | time | killed (incl. compiler) | by a test | survivors to triage |
|---|---|---|---|---|---|
| `tokens` | 841 | ~47 min | not re-measured | | |
| `bitwise` | 76 | 4 min | **99%** | 87% | **1** |
| `deletions` | 665 | 14 min | 94% | 30% | 37 |
| `negation` | 24 | **51 s** | 100% | **0%** | 0 |

`negation` drops a logical `!`. **Every one of its 24 mutants is rejected by the compiler, and not one reaches a test.** That is a fact about this header, not a weak operator: nearly every `!` sits in type-level code (`static_assert(!is_detected_v<...>)`, `enable_if_t<!is_map_v<Q> && ...>`, `if constexpr (!std::is_trivially_destructible_v<T>)`), where dropping it makes the program *ill-formed*. In oans, a C project, the same operator gives 14 sites, 13 caught, and one real finding: a `!` excluding DELALLOC extents from a shared-byte count, which no test held. It stays in the default set because the `-fsyntax-only` pre-filter rejects all 24 at about 2 s each, not a rebuild each: useless here, and nearly free here. Expect such unevenness between projects; the property tests taught the same lesson from the other side.

`bitwise` mutates `^` and `|`, which the token table leaves alone (`&` is bitwise and, address-of and the reference declarator; only a parser can tell them apart). Few sites in a header of masks and fingerprints make it cheap and sharp. A *surviving* bitwise mutant usually means the operands are provably disjoint, as 4.9.1's one survivor was: in its robin hood header, `dist_inc | (hash & fingerprint_mask)` turned into `^` was the same function, guaranteed by the `static_assert(fingerprint_mask < dist_inc)` right above it.

`deletions` removes whole statements. Nearly every bug in `bugs/invariants.txt` is "the code forgot to do this", never one token. It costs *less* than the token sweep, because the `-fsyntax-only` pre-filter rejects half its mutants before a rebuild.

**Reordering is the operator that is *not* here.** It was written, measured and removed. Swapping two adjacent statements killed 45% and left a hundred survivors, nearly all statements that never touched the same state: member-copy chains, the run of `HASH_STATICCAST` macros, declaration blocks. Triaging them produced no test worth writing. The orderings in `bugs/invariants.txt` need a statement moved *out of its enclosing block*, which adjacent swapping cannot do; build it only alongside something that can.

**Not generated:** comments, string literals, preprocessor lines, and `std::enable_if_t<..., bool> = true>` (the SFINAE idiom whose value is never read). A mutant in a branch this configuration does not compile is dropped once the lanes exist, and the run names those lines.

**Cost.** A mutant is one full rebuild: all ~90 translation units include the header, so no incremental build, and ccache cannot help. ~100 CPU-seconds compiling against 3 running the suite. So lanes default to one ninja job each, and a single named bug gets the whole machine. Budget about a minute for a handful of mutants and an hour for a whole function.

**Open (2026-10-02):** the ~100 CPU-seconds is a separate build under 32-way contention, while lanes build unity by default, 27 CPU-seconds isolated (below); the per-mutant cost of a unity lane under contention is not re-measured.

**Verdicts:** `caught` (a test failed; the number worth moving), `compiler` (build refused), `hang`, `oom`, `survived`. Without a sanitizer, a one-slot-past-a-bucket read comes back `survived` however good the tests; re-run surprises with `--meson-arg=-Db_sanitize=address,undefined`.

**Memory caps.** Each lane runs in a memory-capped cgroup (`systemd-run --user --scope`), because a mutated growth policy turns an insert into a request for more memory than the machine has. Capped, that is one `oom` verdict; uncapped, the kernel may kill another lane instead. `--memory-limit` overrides the default, the smaller of a lane's share of the machine and 1 GiB per ninja job; a build is never capped below what its jobs need. With no user scope available (another init, no session bus, an undelegated container) the run says so up front.

**Lanes** are ~90 MB each in a workdir defaulting to `/tmp`, a tmpfs on most distributions, so `--lanes` costs memory as much as it buys parallelism. The run prints the room it will take and whether it is RAM, and refuses before copying. Core dumps are off: a crashing mutant is an ordinary verdict, and each dump would leave ~30 MB in a lane about to be deleted.

Lanes build `--unity=on`, 2.5x less compiling: one mutant rebuild of the suite is 67 CPU-seconds separately, 27 merged. Unity's usual cost (one touched file recompiles its chunk) does not apply to a mutant, which recompiles everything. `--meson-arg=--unity=off` turns it off.

**Baseline.** The baseline refuses to score until the suite is green twice, so one flaky case stops a run. `--exclude-filter NAME` (doctest's `-tce=`) is the honest way past it, because it names the skip in the fingerprint; lowering `--baseline-runs` does not.

**`scripts/test_mutate.py`** covers the half of the tool that decides what a verdict *means*, runs in CI, and is hermetic (no compiler, meson, lanes or cgroups). It also covers the other repositories' halves: cmake and make backends, minunit harness, project seam, root-only ignore patterns. A backend tested only where it is used is untested. The make backend gets the most attention: it has no configure step, its compilation database is read out of `make --dry-run`, and every misreading is silent. A line mistaken for a compile makes the pre-filter check the wrong thing. A missed compile turns the filter off and costs a full rebuild per mutant. A `-l` left on a line a syntax check cannot link makes every mutant `compiler` under `-Werror`, the flattering direction.

### Mutation triage after the bound

*2026-09-05, re-run 2026-09-07 · info · `invariants.txt` and `erase-path.txt` re-run, plus a `bitwise,deletions` sweep of the index functions, lines 1380-1560*

**Mutation triage after the bound** found one real gap, now tested; with it `invariants.txt` is 45 of
46. Copying an *emptied but grown* table by assignment into a grown target left the copy at the source's shift, so its first insert allocated the large array instead of the smallest (2048 buckets against 64). Observable, and unchecked; now `copying_an_emptied_table_starts_from_the_smallest_array` in `lazy_bucket_allocation.cpp`.

Re-run 2026-09-07: **31 of 46 caught by a test, 14 by a hang or the compiler (split not recorded), one survivor** (corrected 2026-10-02: said "45 of 46 with one survivor"). Five blocks had gone stale meanwhile: the merged block renamed `index[slot]` to `group.m_index[lane]` and `m_buckets.index()` to `index_at()`, and `move_home` moved into `emplace`. The tool refuses a file with any block that does not apply, so **none of the other 41 were being checked either**.

Re-deriving them has a trap. Three obvious rewrites are rejected by the compiler, not a test: `if (true)` leaves the `key` parameter unused; `m_equal(key, key)` trips gcc's `-Warray-compare` on the array-keyed map in `transparent.cpp`; dropping a repoint leaves its two locals unused. A `compiler` verdict means the question was never asked. A fourth rewrite computed the same index as the correct code: an equivalent mutant wearing a bug's name. Check the verdict, not just that the block applies.

The one remaining survivor is equivalent: the moved-from mask (every find and erase checks `empty()` before it could read the mask, and a moved-from table is empty) (corrected 2026-10-02: said "... is empty) and the erase decrement just above"). The sweep's sixteen survivors are of three kinds, none a test hole: deletions and bitwise rewrites in the SWAR fallback, which an SSE2 build does not compile; prefetch deletions, semantic no-ops; and the erase decrement again. `erase-path.txt` is 5 of 6, the sixth being that decrement.

**Correction (2026-10-02):** the erase decrement, a survivor of the 2026-09-05 sweep and of `erase-path.txt` that day, was a test hole, not an equivalent mutant: it lengthens probes, see [A miss had no bound](#a-miss-had-no-bound-and-eight-chosen-keys-made-it-loop-forever). `test/unit/erase_uncounts.cpp` catches it since 2026-09-07.

**Later (2026-09-27):** both files had gone stale again and were re-derived in #314 (d10437d): `invariants.txt` 49 mutants, 41 caught by a test, 5 hung, 1 oom, 2 compiler, none survived; the moved-from mask block was removed as unobservable. `erase-path.txt` 6 mutants, 5 caught, 1 hung.

### Fuzzing

*method · `scripts/fuzz_afl.py`, `scripts/fuzz_run.sh`, `scripts/fuzz_merge.sh`, `.github/workflows/fuzz.yml`*

The `fuzz` test suite replays the committed corpora in `data/fuzz/<target>` on every test run, which only re-finds old finds. The libFuzzer targets search. They are clang only and not built by default.

**`fuzz_group_index` is the one that can reach the index's own structure, and the reason it exists is worth keeping.** The other targets hash with the default hash, or in `fuzz_insert_erase` with an identity over the whole 64 bit key (corrected 2026-10-02: said "already hash with an identity over the whole 64 bit key"); they lacked *structure*, not steerability. Filling a group and emptying it means sixteen keys agreeing in their top bits, then sixteen erases of them. A random key stream does not produce that, so an unbounded miss survived all other targets plus 767 unit tests. This target splits a key into three bytes the fuzzer picks separately (group, identity, fingerprint) and has fill-a-group and erase-a-run as single operations, so "fill this group, send one key of this class past it, take the fillers back out" is a few mutations, not a coincidence. Validated by removing the probe's bound: a libFuzzer timeout inside `probe` within seconds, *from the seed corpus alone*. `data/fuzz/fuzz_group_index/cb8d5c38...` is that input, kept as a regression seed. Coverage minimization drops it, because against the fixed header it is no longer distinctive.

```sh
CXX=clang++ meson setup builddir/fuzz
ninja -C builddir/fuzz test/fuzz_api          # or fuzz_insert_erase, fuzz_replace_map, fuzz_string
./builddir/fuzz/test/fuzz_api -max_total_time=60 scratch-dir data/fuzz/fuzz_api
```

libFuzzer writes new inputs into the *first* corpus directory, so keep `data/fuzz/...` second and it stays read-only. Passed alone (`./test/fuzz_api data/fuzz/fuzz_api`), it quietly fills the committed corpus with hundreds of files. To replay only, run the `fuzz` suite (`./builddir/clang_release/test/udm-test -ts=fuzz`), as CI does. `scripts/fuzz_run.sh <target>` drives one target on all cores; `scripts/fuzz_merge.sh <target>` merges a scratch corpus down to the inputs that add coverage. `FUZZ_TEST_CASE` in `test/fuzz/run.h` expands to the doctest replay case normally and to libFuzzer's entry point under `-DFUZZ`, so one body serves both.

**AFL++** builds the same body: `afl-clang-fast++` accepts `-fsanitize=fuzzer` and links its own driver over `LLVMFuzzerTestOneInput`. Name the target; a bare `ninja` fails, because AFL defines `FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION`, which `fuzz/run.h` reads as "honggfuzz is driving":

```sh
CXX=afl-clang-fast++ meson setup builddir/afl
ninja -C builddir/afl test/fuzz_api
afl-fuzz -i data/fuzz/fuzz_api -o out -- ./builddir/afl/test/fuzz_api   # -i is never written to
```

**`scripts/fuzz_afl.py`** does those easy-to-get-wrong steps:

```sh
scripts/fuzz_afl.py run              # every core, every target, until Ctrl-C
scripts/fuzz_afl.py run fuzz_api     # every core on one target
scripts/fuzz_afl.py sweep            # each target in turn, moving on when it goes quiet
scripts/fuzz_afl.py sweep --idle 15m # ... giving each one longer to prove it is done
scripts/fuzz_afl.py minimize         # fold the findings into data/fuzz, shrunk, with coverage
```

It builds what it needs, gives the first target's main instance the terminal for its status screen (the rest log to `fuzz-findings/<target>/afl-*.log`), resumes rather than restarts, and stops all on Ctrl-C. Committing results is left to you. `run` splits the cores across all targets. `sweep` is for leaving alone: one target gets every core until it goes `--idle` without a new find (default 5 minutes), driven by new queue entries across all instances (corrected 2026-10-02: said "driven by the main instance's `last new find` counter"; that screen counter shows roughly the same). Idleness is read from the queue directories, not `fuzzer_stats`: afl-fuzz rewrites that file on its own schedule, and its `last_find` and `corpus_count` can sit unchanged for a minute, long enough to call a target done while it still finds things.

"Every core" means every *physical* core: the script reads `thread_siblings_list` and pins one instance per core with `afl-fuzz -b`. A second instance on a hyperthread sibling mostly slows the first while afl-fuzz counts both busy. Which sibling represents a core is not guessable (core 0's siblings are `0,1` on some machines, `0,n/2` on others), so the kernel decides, not arithmetic. Without readable topology (macOS) it falls back to `os.cpu_count()` and lets afl-fuzz place instances. `-b` skips afl-fuzz's scan for an unused core, so on a busy machine it shares instead of refusing to start.

**Minimizing takes both tools**, as neither subsumes the other: `afl-cmin` covers the same AFL edges with far fewer files but is blind to libFuzzer's finer features, and `-merge=1` onto its output adds back exactly the files carrying a dropped feature. Note the `@@`: `afl-cmin` pipes stdin by default, for which this driver reports no coverage.

```sh
afl-cmin -i data/fuzz/fuzz_api -o corpus-cmin -- ./builddir/afl/test/fuzz_api @@
cp -r corpus-cmin corpus-min && ./builddir/fuzz/test/fuzz_api -merge=1 corpus-min data/fuzz/fuzz_api
```

`.github/workflows/fuzz.yml` runs every target nightly and uploads any crash and the coverage-increasing inputs. Its `minimize` dispatch input runs the two-step shrink instead. Committing either stays a human decision.

### CI

*method · `.github/workflows/main.yml`, `scripts/lint/all.py`*

`.github/workflows/main.yml` builds every leg the same way, so any leg reproduces locally:

```sh
meson setup builddir --force-fallback-for=fmt -Dcpp_std=c++<matrix cpp_std, default 17> <matrix setup_args>
meson test -C builddir --print-errorlogs <matrix test_args>
```

The MinGW job is separate: it sets only `-Dcpp_std=c++<cxx_standard>` (17 or 23).

`--force-fallback-for=fmt` builds against the vendored fmt, not whatever the runner has installed.

One leg builds `--unity=on --unity-size=16`. Unity is off by default because touching one file recompiles its whole chunk, but it catches what separate compilation hides: an anonymous namespace stops isolating a file once neighbours share its chunk. Its first run found only old, invisible problems: `test/app/print.h` had no include guard, and four `test/bench/*.cpp` each defined a `bench()` that became ambiguous when two shared a chunk.

**A collision surfaces when the chunking changes, not when the collision is written**, so the leg fails on an unrelated commit. `precomputed_hash.cpp` (counts hash calls) and `lazy_bucket_allocation.cpp` (counts allocations) had each declared a `counting_map` in an anonymous namespace since long before 2026-09-06. Adding `test/unit/move_home.cpp` to `test/meson.build` shifted every later file one place and put both in chunk 4. One was renamed `hash_counting_map` rather than relying on chunk luck. So reproduce this leg with its own size (`meson setup --unity=on --unity-size=16`; meson's default is 4, with other boundaries) whenever a test file is *added or removed*, not only edited. The second error cluster in that log, `_Rb_tree_color does not name a type` in `stl_tree.h`, is gcc error recovery, not a second bug.

**Linters** (`scripts/lint/lint-*.py`, all via `scripts/lint/all.py`) run in the `lint` job. Two pin their tool, because both tools gain checks or change output between releases: `clang-tidy-18` and `clang-format` 21 (`pip install clang-format==21.1.8`). `lint-clang-format.py` *skips* rather than fails without version 21, so a local run with another clang-format says so instead of reporting the tree broken.

### Offline builds

*method · "Notes for sandboxed / offline environments" in the source*

If meson cannot download the wrap subprojects (e.g. GitHub release tarballs blocked), fetch doctest and fmt into `subprojects/doctest-2.5.3/` and `subprojects/fmt-12.0.0/`. These match the `directory` field of the `.wrap` files, and the version numbers must keep agreeing. Add a minimal `meson.build` in each declaring `doctest_dep` (header-only, include dir `doctest/`) or `fmt_dep` (include dir `include/`, sources `src/format.cc`, `src/os.cc`) and calling `meson.override_dependency()`. Meson skips the download when the directory exists. These directories are gitignored; do not commit them.

