# The measurements behind unordered_dense

Every experiment run on this map's index, hash and benchmarks, with the numbers that decided it.
`CLAUDE.md` carries the rules that came out of this; this file is the evidence, and it is here so
that the rules can be checked and so that a rejected idea is not proposed a second time.

**How to use it.** Do not read it front to back -- it is ~2400 lines. Search it for the thing you
are about to try, by the phrase in the index below (`grep -n "double hashing" notes/index-design.md`)
or by a symbol (`grep -n move_home notes/index-design.md`). Each entry opens with a bold sentence
saying what was tried and what happened, and the paragraph under it is the evidence.

**Read it before proposing an optimization.** 149 ideas are recorded here and most of them lost.
Several were proposed twice because the first rejection was not looked up.

**Dates are load-bearing.** An entry is true of the header on the day it was measured. Anything
older than the change it is about has to be re-measured, and entries that were retracted say so in
place rather than being deleted, because the retraction is usually the more useful half.


## Index


**Where the time goes**

- String keys must not all be the same length
- Integer keys must not be small sequential values
- Inserting has to grow the table somewhere
- A mapped value of eight bytes hides what a dense map is for
- `build` measured the kernel's page fault handler as much as the map, until `tame_allocator()`
- Nothing measured a table that only churns
- Lookup benchmarks must not replay

**Dead ends of the group index (paired A/B, 2026-09-05)**

- `replace()`'s gate was re-measured on a fixed harness and kept, and the harness is the finding
- `replace()`'s two loops walk with cursors too, and one cursor too many is slower than none
- `merge()`, and why it is not the loop the caller would write
- `replace()` hashes ahead too, once the reason it could not was looked at properly
- `insert(first, last)` sizing the table from the range: built, measured, declined
- The paired harness has a systematic bias on `rmissstr`, about 3.6%
- A block's prefetches should step from its start, not jump to its end
- The other two pipelines do not want the cache gate, and the gate's own footprint model was wrong
- Taking the hash apart once per insert instead of twice, and the shared pipeline that was not worth it
- Chunks or a sliding ring: the rehash and the bulk visit want opposite answers
- A bulk `visit()`, and the `prefetch(key)` API it replaced
- Tiny pointers, and the bound insertion order puts on the value index
- Splitting the probe past the home group: kept, for keys whose compare is a call
- The insert path's instructions, counted one by one: the clang/gcc gap is the call boundary
- The F14Vector string miss, re-measured, and the old explanation of it is wrong
- Where they are, from `perf annotate`
- Splitting the probe so the miss path stops paying for it
- And a measurement trap that cost most of a session
- Eleven slots and twenty-four slots, and why sixteen is where it stops
- What eleven adds that twelve could not say
- And the reason is the counters, in the direction opposite to the prediction
- So sixteen is a local optimum on three axes at once
- Why none of them could have won, which is the part worth keeping
- A growth factor below 2, and the premise that suggested it was wrong
- Not worth changing the default, and worth documenting as a knob
- The pipelined rehash does not transfer to `boost::unordered_flat_map`, and no other map has one
- Why, measured rather than argued
- What that says about boost's build, which is its weakest column
- And the radix partition does not rescue it either, which was the one idea left
- Where the F14Vector string gap actually is
- Force-inlining `do_find_hashed`, paired on the score: not kept
- Splitting the hash into an inlinable short path and an out-of-line tail
- folly's `fullness[]` byte array in the rehash
- The 12-slot, 64 byte, cache-line-aligned block
- Prefetching the back element's string body before an erase's probe
- Fusing the insert's probe with its placement
- The one fact left standing is the clang/gcc gap itself
- What `move_home` is actually worth, re-measured
- What this does not say
- Three ideas the eighteen-map comparison suggested, all measured, none kept
- Double hashing instead of the triangular sequence, so siblings take different tours
- The mechanism works and the time is worse
- And the reason `flat_wmap` is actually fast is not the window
- A per-table seed, abseil's defence against keys chosen for a known hash
- The paired harness read this one wrong, which is the rule working
- Also: the churned drift figures recorded below do not reproduce
- Every other map on the same workloads, in one harness
- The sliding window, built and measured rather than simulated
- The window wins the lookup, and it wins it at the branch predictor
- 67.4, 44.0, 0.436 and 3.018
- The miss result is against the wrong baseline, and the probe lengths say so
- And it loses churn for a reason worth having, which is that it recycles tombstones half as well
- The lane-contention mechanism, confirmed by transplant, and it runs the other way from the guess
- It reproduces the window's behaviour precisely
- So the direction is the opposite of what I wrote when I proposed the experiment
- And it cannot apply to this map at all, which is the part I got wrong twice
- Read that as a prototype against a tuned library, not as a design comparison
- So: not worth adapting, and the reason is structural rather than a number
- And what the prototype cannot answer, which is why `indivi::flat_wmap` is fast in absolute terms
- And the measurement is the lesson
- An SVG in an `<img>` follows the reader's colour scheme, not the page's
- A value label never goes inside its bar
- A miss is where the counter earns its keep, and abseil is the control that proves it
- 1.38 on a miss
- `indivi::flat_wmap` is the fastest hit of anything measured
- The chained designs lose the miss badly
- And the own-hash control moved further than expected
- The counters, one map per binary, 30M lookups at 50000 entries
- Memory, and the second place tombstones show up
- The size sweep, and the measurement mistake it took three tries to get right
- The mistake is the part worth keeping
- Where a year of this got to, measured against 4.8.1
- 5.95 and 7.62
- `scripts/ab/regen.sh` rebuilds everything in `doc/`
- The same-hash convention was flattering boost on every string chart, and the control found it
- That flips the string ranking
- A fifth series, and it is a control
- Every workload is measured for both key types
- The charts are also a page you can interrogate
- Why a dense erase is not slow, and where it is
- For an integer key it is free
- For a string key it costs about 50 ns
- Two optimizations the charts point at, one measured and one not yet
- Huge pages are worth 22% of a large lookup and nothing asks for them
- Merging the group metadata with its own value indices: measured, and kept
- The four charts worth keeping, and the two axes that had none
- The sweep replayed its key sequence, which flattered the branchiest probe by 2.7x
- Retracted, because the data behind them does not reproduce
- Both of those points come from one unpaired measurement each and should be read as indicative
- Pulling a displaced sibling home on erase, to take the churn drift back
- Where a string hit's ~90 cycles go, from perf, and two more things it led to that lost
- The last structural lever on the hash, taken and found to weigh nothing
- In the map it is worth nothing
- Six more hashes tried, none adopted, and the pair of columns says why
- foldhash is bit-identical to the crate
- Strict avalanche first, because it decides which rows are even candidates
- Then the two time columns, and they order the field oppositely
- rapidhashNano ties this hash exactly at 8 and 16 bytes
- polymur-hash is not slow by accident
- And a throughput number taken through a function pointer is not a throughput number
- Why `absl::Hash` is faster below 32 bytes, and what taking it would cost
- What it costs, measured rather than assumed
- The idea does transfer to the block range, and the earlier test of it above is wrong
- One rotate repairs it exactly
- 1.14x at 17 to 32 bytes, 1.12x on the scored mix net of the harness's chain
- And in the map it is worth nothing, again
- What cannot be taken at all is the short path
- In the map every one of them is nothing
- Why the harness lied, and it is a trap worth naming
- The hash measured against the ones the other libraries ship
- `absl::Hash` is the one to beat and nothing here says otherwise
- Three process-level repetitions are what make those digits mean anything, and `-n` does it
- And do not edit a shell script while it is running
- Latency is what a map pays, tested rather than argued
- What does not add up, and is worth knowing before quoting the chart's right panel
- A chart for the hash, and its own control says how much of it to believe
- The two identical-source rows are the control, and they disagree with each other
- The string hash restructured: independent blocks from 17 to 144 bytes
- A mutating hit moves itself home, and that takes the churn drift back
- What it is worth, and the measurement that had to be done three times to find out
- The rehash loop pipelined, and the partitioned rehash it was measured against
- Kept the pipelined loop, dropped the partition
- Not zeroing the value index
- What `segmented_map` gives up in 5.0.0, found in review
- The SSE probe audited, and its one real redundancy is compiler-dependent
- six of the eight put both prefetches on the same line
- Fingerprints and counters in two arrays instead of one 24 byte group
- Which counter a probe consults at each step of its sequence
- And the fifth point on that axis: an exact counter, worth 2-3%
- About 80% of what the counter fails to filter is siblings
- The insert path is split in two by clang, and that is most of the build gap to boost
- ihtab and ixhtab measured, and a bug in one of them reported upstream
- Verstable measured rather than read
- Why, and it is a mechanism this file keeps arriving at from new directions
- A Verstable miss executes 17% fewer instructions than this map's and takes twice the cycles
- Verstable 27.1-28.6
- The reason the two columns behave differently is the whole point
- That last sentence stopped being true on 2026-09-05
- Those memory figures are a point on the sawtooth, and the octave says something else
- A miss had no bound, and eight chosen keys made it loop forever
- Mutation triage after the bound
- Indexing the value container in the rehash cost clang a memory latency per element
- The rest of the header was then audited for the same shape, and it is the only instance
- gcc left `probe` out of line, and forcing it inline is the largest single gcc gain on the branch
- The gcc string-lookup gap, explained and mostly closed
- The whole matrix, run on CI rather than on the desktop
- Only the ARM pair measures what the vector compare is worth
- Windows had never been measured at all
- The one outlier chased down, and it is the CPU rather than the code
- NEON closed the ARM lookup gap, and it was the whole gap
- Before NEON: on ARM the branch was 1.11x main, the same overall as on x86, split the opposite way
- lookups are behind main
- `ie64` ties boost while executing 58% more instructions, and the counts say why

**The robin hood index this replaced, and its dead ends**


**Optimization dead ends (verified with paired A/B runs; re-test before assuming they still hold)**


**Tooling, in the detail the rules were compressed from**

- Mutation operators: kill rates per operator, and why `negation` is free here and useful in oans
- Why reordering was written, measured and removed
- Lanes, cgroup memory caps and the unity build inside the mutation tool
- What each fuzzing target reaches, and the two-tool corpus minimization
- The CI legs, the unity chunk collision, and the pinned linter versions

---

## Where the time goes

The measurements in `scripts/ab/README.md`, with the paired A/B harness that produced them, were
taken on the robin hood index: the u64 workloads bound by branch mispredictions, the string
workloads by the latency of a ~167 instruction lookup of which the hash is ~59, and a model of 16
cycles per misprediction plus instructions at ~3.5 per cycle predicting changes within a cycle or
two. The group index that replaced it mispredicts far less -- one branch per group rather than one
per bucket -- so treat those numbers as being about the workloads, which have not changed, rather
than about the current lookup. Everything below in this section is about the workloads and still
holds.

**String keys must not all be the same length.** Until 2026-09-02 every string key of
`bench_quick_overall_udm` was exactly 200 bytes. A hash dispatches on length, and one length makes
that dispatch perfectly predictable: wyhash costs 0.31 branch mispredictions per hash on lengths
spread over 4 to 200 bytes and 0.01 on a fixed length, so the benchmark could not see the
difference. 200 bytes also put every key on the heap, where a real workload keeps a good share of
them inside the `std::string`. The keys now run from 8 to 135 bytes, skewed towards short. Scores
from before that change are not comparable with scores after it.

**Integer keys must not be small sequential values.** `insert_erase` and `iterate` draw their
values from a range that grows to 20000, and until 2026-09-02 the value was the key. The hash of a
`uint64_t` is one multiply, and the top bits of a multiple of a small integer walk a lattice: 10000
such keys in 16384 buckets landed at most one to a bucket, with 39% of the buckets empty where a
uniform hash leaves 54%. Nothing collided, so the probe never probed and the shifts never shifted
-- 82% of erases moved nothing against 58% for the same values as strings -- and two changes to
the shift loops read as losses on `ie64` that were wins on every honest table. The value is now
scrambled through a 64 bit bijection before it becomes a key, so every checksum is as it was and
`ie64` sees the same table `iestr` does. A benchmark that rewards a hash for the one input it is
perfect on is measuring the input.

**Inserting has to grow the table somewhere.** Until 2026-09-02 no workload in the score did.
`insert_erase` draws its keys from a range that grows to 20000, so the map hovers at ~10k entries
and doubles its bucket array about a dozen times in eight million operations. Growth is not a
rounding error in general: building a map of a million entries costs 52% more than building the
same map after `reserve` for `uint64_t` keys and 31% more for strings, all of it rehashing, so a
map that grew badly would have scored the same as one that grew well. `build` covers it, and it
showed at once that this is where the map is weakest -- see `scripts/ab/README.md`.

**A mapped value of eight bytes hides what a dense map is for.** Until 2026-09-03 both maps in the
score held a `size_t`. Every cost of a flat map scales with `sizeof(value_type)`, because it writes
the whole value into a hash-scattered slot; a dense map writes eight bytes there and appends the
payload to a vector in order. Measured on `build` with the same `uint64_t` key,
`boost::unordered_flat_map` is 1.22x ahead at a 16 byte `value_type` and **2.14x behind** at 64 --
the same property that wins `buildstr` and loses `build64`. `map<Key, SomeStruct>` is at least as
common as `map<Key, size_t>`, and a change that gave that property up would have scored the same.
`workloads::big_value` is 64 bytes and trivially copyable on purpose: the variable under test is the
size of the value, and an owned allocation would confound it with the heap. Its checksums are the
ones the small-value maps produce, so one set of constants verifies all three.

**`build` measured the kernel's page fault handler as much as the map, until `tame_allocator()`.**
Found 2026-09-03 while adding the big-value map. A build from empty asks for megabytes and gives
them straight back, and glibc returns anything above its mmap threshold to the OS, so every
repetition faults in the same fresh pages again: 38% of `build64`'s cycles were kernel, at 2057
page faults per build, and 65% of `buildbig`'s. Worse than noise, because whether it is paid
depends on what ran *before* in the process -- anything that already freed a block that large
raises the threshold and moves the whole thing into the arena. That is how `buildbig` came to
report 4.31ms for a build costing 14.9ms on its own, while executing *more* cycles (19.2M against
16.6M) and *more* instructions than `build64`, which reported 6.04ms.

`workloads::tame_allocator()` raises `M_MMAP_THRESHOLD` and `M_TRIM_THRESHOLD` to 64 MB, so the
arena is kept and the pages are faulted once per process instead of once per repetition. Every
workload calls it, so the process is in the same state whatever the order. Measured on `build64`:
6.61ms to 3.78ms per build, 41141 page faults to 2289, and total cycles down to within 4% of user
cycles. Isolated-versus-in-score now agrees for all three maps -- `buildbig` was 14.90 against
4.31, and is 4.94 against 4.26 -- and all three run at the same effective clock, which is the check
that the kernel time is gone. A warm allocation before the build does *not* work, touched or not
(6.60ms and 6.11ms); only keeping the arena does.

What this removes is a real cost -- a program that builds one map in a fresh process does pay it --
but it pays it once, where a benchmark repeating the build hundreds of times paid it every time and
drowned the map in it. glibc only; elsewhere it is a no-op.

**Nothing measured a table that only churns.** Until 2026-09-03 every workload in the score
either grew or was measured on a table that had just been built. Backward shift deletion leaves the
table as it would have been had the erased element never been inserted, so a table that has churned
for a long time is as good as a fresh one. A design that frees a slot without undoing what once
probed past it cannot do that -- its probe sequences only grow, and it repairs them with a rehash.
Nothing in the score could tell the two apart, because `build` and `insert_erase` both keep growing
and a growth rehash resets the damage for free. `churn` grows once and then never again: fill to
50000, reserve the room, then erase one and insert one with two lookups between, holding the size
exactly there. Measured that way over two million operations at 200000 entries,
`boost::unordered_flat_map`'s lookups degrade to **1.31x** of their fresh cost and snap back on an
in-place rehash it pays for every third round -- its bucket count never changes, and the round that
repairs costs +7ns per operation -- while this map's stay flat within the noise. With string keys
both degrade, because what degrades there is the heap the key bodies live on rather than the table,
and this map churns **1.28x faster** than boost throughout. Against boost the sustained gap is much
smaller than the fresh-table one: 1.32x on `churn64` where `rhit64` on a fresh table is 1.71x, and
1.03x on `churnstr`, the narrowest of any string workload it does not already win. Scores from
before this workload are not comparable with scores after it.

**Lookup benchmarks must not replay.** Until 2026-09 the find workload of
`bench_quick_overall_udm` reset its search rng to the insertion rng's seed, so its sequence of hits
and misses repeated and a TAGE-style predictor learned much of it: 0.6 mispredictions per lookup
where a random sequence costs the scalar probe 1.35. That under-reported the cost of branchy
probing and rewarded the opposite, and it hid most of the SSE2 probe's gain. The workload now
decides every lookup with an rng of its own. `find_random.cpp` still replays.

## Dead ends of the group index (paired A/B, 2026-09-05)
**`replace()`'s gate was re-measured on a fixed harness and kept, and the harness is the finding**
(2026-09-11, issue #257, `scripts/ab/replace_bulk.cpp`, Ryzen 9 7950X, clang 22, medians of seven).

`pipeline_min_index_bytes` was fitted in #249 on `map<uint64_t, size_t>` alone, and a sweep during
#256 suggested it was too low for a string key. Re-fitting needs numbers, and the harness could not
give them: at sixteen to a hundred thousand `uint64_t` elements the same binary read 4.40, 2.85, 2.88,
4.76, 5.19, 2.76 ns per element between repeats.

*The harness, first.* Two things caused it, and both are now fixed. The timed region contained a fresh
index allocation, because every round built a new map -- so what the allocator had just done with the
container copy decided what the allocation cost. And the reported number was the mean over rounds, so
one round that faulted carried it. `replace_bulk.cpp` now replaces into the **same** map every round
after one untimed round that allocates the index, and reports the **median round**. Four repeats of
the same cell go from 3.32 / 3.33 / 3.31 / 3.27 with a max of 6.14 to **3.158 / 3.141 / 3.145 / 3.147**
with a max of 3.92. `cold` restores the old behaviour, which is the right one for "what does a caller
pay end to end" and the wrong one for "does the pipeline inside it pay".

*The answer, with numbers that hold still.* Ring over the same loop with the ring deleted, by index
size, for both key types at two duplicate rates:

| index | `uint64_t` 0% | `uint64_t` 25% | `string` 0% | `string` 25% | geomean |
|---|---|---|---|---|---|
| 88 KiB | 1.265 | 1.225 | 1.006 | 1.037 | 1.128 |
| 176 KiB | 0.833 | 1.194 | 1.156 | 1.033 | 1.044 |
| **352 KiB** | **0.667** | **1.163** | **1.064** | **1.002** | **0.954** |
| 704 KiB | 0.632 | 0.999 | 1.039 | 0.987 | 0.897 |
| 1408 KiB | 0.626 | 0.944 | 0.998 | 0.985 | 0.873 |
| 2816 KiB | 0.565 | 0.916 | 0.973 | 0.979 | 0.838 |
| 5632 KiB | 0.537 | 0.912 | 0.950 | 0.944 | 0.814 |

The four curves cross in four different places -- 176, 704, 1408 and 352 KiB -- and the reason is not
the key type alone. **The ring's payoff depends on the duplicate rate**, because a duplicate refills
its ring slot from the element that just moved into it, which the very next iteration reads: there is
no distance to prefetch over. At 352 KiB the same gate is worth **1.5x** to a `uint64_t` with no
duplicates and costs **16%** to one with a quarter of them. The map cannot know which it has until it
has walked, which is the same wall #248 ran into from the other side.

So the constant is a compromise and was chosen as one. Realised geometric mean over all 28 cells, and
the worst single cell, for four candidate thresholds:

| threshold | geomean | worst cell |
|---|---|---|
| 128 KiB | 0.9137 | 1.194 |
| **256 KiB (kept)** | **0.9081** | 1.163 |
| 512 KiB | 0.9143 | 1.039 |
| 1 MiB | 0.9286 | 1.000 |

**256 KiB is the best of the four on the mean**, and it is also where the combined per-size crossover
sits: 1.044 at 176 KiB against 0.954 at 352 KiB, and the gate opens between them. A threshold that
never loses does exist -- a mebibyte -- and costs two points of geometric mean to buy away a worst
cell of 1.163. The issue's premise was half right: the constant *was* fitted on `uint64_t`, and the
string loss it leaves is real, but it is one octave, at most 1.064, and it is paid for by a 1.5x on
the neighbouring cell of the same octave.

*What was considered and not built.* Making the walk count duplicates and abandon the ring when it
sees too many. It is adaptive on data already in hand rather than a guess about data it has not seen,
which is the distinction #248 turns on -- but it is still a threshold on the caller's data chosen by
whoever fits it, and it would have to be fitted on the same sweep that produced the table above.

**`replace()`'s two loops walk with cursors too, and one cursor too many is slower than none**
(2026-09-11, issue #242's follow-up, `scripts/ab/replace_bulk.cpp`, Ryzen 9 7950X, clang 22, medians
of seven, paired within each run).

`merge()`'s walk was 0.89-0.95 for holding its elements in cursors instead of indexing them, so the
same question was put to every other bulk loop in the file. Only two of them have the shape: a
sequential walk over `m_values` interleaved with placements that store fingerprints. `do_visit`
indexes, but from the probe -- a random index, with no cursor to hold -- and it never places.
`fill_buckets_from_values` and `do_insert_range` were already iterator-based.

The two are `do_replace_pipelined` and the plain loop `replace()` finishes with. Cursor over indexed,
all 24 cells:

| n | dup 0% | 25% | 75% | | n | dup 0% | 25% | 75% |
|---|---|---|---|---|---|---|---|---|
| `uint64_t` 8000 | 0.684 | 0.982 | 0.948 | | `std::string` 8000 | 0.902 | 0.937 | 0.935 |
| 32000 | 0.956 | 0.933 | 0.812 | | 32000 | 0.879 | 0.952 | 0.960 |
| 200000 | 0.957 | 0.945 | 0.873 | | 200000 | 0.886 | 0.955 | 0.964 |
| 2000000 | 0.962 | 0.972 | 0.890 | | 2000000 | 0.933 | 0.976 | 0.958 |

(The 0.684 is not a 1.46x: at eight thousand elements the *indexed* binary is bimodal between repeats
-- 4.40, 2.85, 2.88, 4.76, 5.19, 2.76 -- where the cursor one is tight at 2.39-2.65. The round
rebuilds the container and the allocator does not do the same thing twice, which `range_insert.cpp`
records at the same sizes. The cursor version being the *stable* one is the finding there.)

Instructions and cycles, both down in every cell measured: 80.93 to 79.92 instructions and 20.47 to
19.01 cycles per element at 32000 with no duplicates, 74.76 to 69.99 and 34.51 to 30.22 at 200000 at
a 75% duplicate rate. For a **string** key the instruction count does not move at all (487.48 to
487.21) and the cycles do, 176.23 to 170.57 -- which is the mechanism seen from the other side: there
the reload is not extra work, it is a memory latency sitting on the critical path in front of the next
hash.

*One cursor too many is worse than none.* The first version held `read`, `look` **and** `last`, and
tested `last - read > pipeline_depth + 1` instead of asking the container its size. It retired two
fewer instructions per element than the indexed loop and took **3.5% more cycles** for it -- 3.83 ns
against 3.54 for the two-cursor version at two hundred thousand elements with no duplicates, and 1.03
to 1.04 against the indexed loop it was supposed to beat, reproducibly over nine repeats. Two reasons,
both worth keeping: `last` is a fourth live value across a loop already holding three ring arrays, and
it cannot be decremented on a pop, because `std::deque::pop_back` invalidates the past-the-end
iterator and a deque is a value container this map supports -- so it needs an `m_values.end()` after
every duplicate. `size()` answers the same question for nothing. A variant holding only `read` and
indexing the lookahead was measured too and is between the two (3.77).

*`replace()`'s gate was not re-fitted, and the reason is that its gap is older than this.* Re-fitting
after the loop gets cheaper is the rule `merge()` established, so it was tried. The `uint64_t` side of
that sweep is unusable -- the same bimodality as above, ratios of 0.885, 0.654, 1.005, 0.688 at
neighbouring sizes -- and the string side is clean and says the ring *loses* 2 to 10% from 176 KiB of
index to 1408 KiB. That band was then measured on `origin/main` as well, with the indexed loops:
**1.116 / 1.043 / 1.072 / 0.988 there against 1.072 / 1.051 / 1.021 / 0.992 here**, i.e. the same
band, very slightly better. So it is not something the cursors introduced: `pipeline_min_index_bytes`
was fitted on `uint64_t` in #249 and is too low for a key whose hash is long. Filed separately rather
than changed on a sweep this noisy.

**`merge()`, and why it is not the loop the caller would write** (2026-09-11, issue #242,
`scripts/ab/merge.cpp`, Ryzen 9 7950X, clang 22, medians of five, both maps rebuilt from the same
input every round and only the merge inside the clock).

The standard specifies `merge` on nodes: it splices them, so references to the elements that move
stay valid and nothing is copied. A dense map has no nodes, so every element that moves is
move-constructed into the destination's vector and every element that leaves has to come out of one.
What was left to decide is how the *source* is repaired, and the two candidates have opposite best
cases.

*The loop a caller writes* -- `try_emplace` here, `erase(it)` there -- repairs the source's index once
per element taken. Each repair is a second hash of the key leaving and a third of whichever element
the backfill drags into the hole, so it costs work per element that **goes**. It also cannot move the
key out: `erase(it)` hashes the key to find the slot pointing at it, so a loop that moves the key into
the destination first hands `erase()` a moved-from key, and that probe does not terminate (#254). The
hand-written version therefore copies every key it takes.

*What shipped* walks the source once, compacts the elements that stay behind down over the gaps the
taken ones leave, and rebuilds the source's index once at the end. It costs work per element that
**stays**.

Ratios, shipped over the caller's loop, at overlaps 0 / 25 / 50 / 75 / 90 / 100%:

| source | 0% | 25% | 50% | 75% | 90% | 100% |
|---|---|---|---|---|---|---|
| `uint64_t`, 4000 | 0.453 | 0.546 | 0.551 | 0.811 | **1.077** | 0.901 |
| `uint64_t`, 64000 | 0.413 | 0.487 | 0.495 | 0.689 | 0.917 | 0.879 |
| `uint64_t`, 1000000 | 0.506 | 0.569 | 0.498 | 0.597 | 0.733 | 0.713 |
| `std::string`, 4000 | 0.459 | | 0.576 | | **1.053** | |
| `std::string`, 64000 | 0.431 | | 0.526 | | 0.878 | |
| `std::string`, 500000 | 0.556 | | 0.609 | | 0.825 | |

So **1.8x to 2.4x where a merge normally is** -- two sets that mostly do not overlap -- and one corner
where it still loses: a small table whose source is nine tenths duplicates of it, by 5 to 8%. That
corner is exactly the backfill's best case and the compaction's worst, and it closes on its own as the
map grows (1.08 at four thousand, 0.92 at sixty-four thousand, 0.73 at a million), because above cache
the loop's second and third hash cost more than a rebuild does.

It is not fixed, and the reason is worth keeping: choosing between the two strategies needs the
overlap, and the overlap is not known until the probes that measure it have been paid. A pre-pass that
probes without moving would have to carry every element's hash into the second pass to be worth
anything -- eight bytes of scratch per source element -- or hash the taken elements twice, which is
the common case paying for the rare one.

*The ring, and the gate's constant is not `replace()`'s.* The walk is the fifth `pipeline_depth` ring
in the file, hashing sixteen elements ahead of the one it places, and it is gated on index bytes the
way `replace()`'s is -- but **two doublings higher**, which was got wrong first. Three binaries built
from the same header -- ring deleted, ring always, ring gated -- alternated, medians of five, at
overlaps 0 / 50 / 90%:

| source | end index | ring / no ring | gated / no ring |
|---|---|---|---|
| 1000 | 22 KiB | 1.10 / 1.19 / 1.14 | 0.97 / 0.97 / 1.00 |
| 16000 | 352 KiB | 1.07 / 1.12 / 1.09 | 0.99 / 1.01 / 1.00 |
| 32000 | 704 KiB | 1.02 / 1.06 / 1.02 | 0.99 / 1.01 / 0.99 |
| 64000 | 1408 KiB | 0.96 / 0.98 / 0.99 | 0.96 / 0.99 / 0.98 |
| 128000 | 2816 KiB | 0.92 / 0.92 / 0.97 | 0.92 / 0.92 / 0.97 |
| 2000000 | 44 MiB | 0.81 / 0.74 / 0.77 | 0.81 / 0.75 / 0.77 |

The crossover is at **704 KiB to 1408 KiB of index**, not `replace()`'s 256 KB, and the first version
of this entry claimed otherwise on a reading taken before the walk was written with cursors. The
correction is the useful part: a gate's crossover is where the ring's fixed 13-17 instructions per
element stop being worth the misses they hide, so it moves with what the *rest* of the loop costs --
and making the ringless loop 5 to 11% cheaper pushed `merge`'s crossover two doublings up. A shared
constant would now cost 7 to 12% in the octave at 352 KiB. `merge_min_index_bytes` is a mebibyte, in
the middle of the window with a doubling of margin on each side, and with the gate in, the worst cell
anywhere on the sweep is **1.009**.

The gate reads the index the destination will *end* with,
`index_bytes_for(calc_shifts_for_size(size() + source.size()))`, and not the one it has: the case the
ring is most for is a large source going into a small map, where the index it has says nothing about
the one the walk will run against.

*Three cursors, not three indices.* The walk holds `read`, `write` and `look` as iterators. Indexing
`src_values[i]` is the trap `fill_buckets_from_values` records at length -- placing an element stores
a fingerprint, a `std::uint8_t` store may alias the container's own data pointer, and every indexed
read after a placement therefore has to load that pointer back before it can form the next address.
Alternated against the indexed version of the same loop: **0.89 to 0.95 in cache** (0.92 / 0.89 / 0.91
at four thousand elements) and 0.95 to 1.00 above it, 26 of 27 cells at or below 1.00, and 2.0 to 4.5
fewer instructions retired per element at every size. It was written with indices first and the
review caught it.

*The gate has to be two loops, not a branch.* Written the obvious way -- the gate read per element --
the gated-off sizes came back at 4.80 ns against 4.53 for the same loop with the ring deleted, losing
5 to 10% at every size below the crossover, which is most of what the gate was there to save. A
perfectly predicted branch is not free when a ring, a lambda and an array hang off it and the loop has
to keep them live. `walk(std::true_type{})` against `walk(std::false_type{})` is the column above.

*`reserve(size() + source.size())` was measured and declined*, which is #248's answer arrived at from
the other direction. The bound is exact -- a merge cannot end up larger than that -- and it is still a
guess, because what it is really predicting is the *overlap*. Reserved over not, at overlaps 0 / 50 /
100%: 0.837 / 1.257 / 1.604 at sixty-four thousand `uint64_t`, 0.707 / 1.066 / 1.606 at half a
million, 0.818 / 1.244 / 1.618 at sixty-four thousand strings and 0.870 / 1.103 / **2.629** at half a
million strings. So it buys 13-30% in the case that needs it least and costs up to 2.6x in the case
the map cannot tell apart from it, on top of holding an index and a value vector up to twice the size
the map ended up needing. Growth doubling is the right answer here for the same reason it is in
`insert(first, last)`.

*Mutation.* 113 mutants over the whole change, **84% killed**, 55 of them by a test. The first run killed only 62%
and said why: nothing reached the pipelined half of the walk, because the whole test file was below
the 256 KB gate -- `delete: walk(std::true_type{})` survived. A test whose *destination* carries the
size and whose source is nought to forty elements long turns the ring on cheaply and puts an edge on
the priming loop at the same time. The rest of the gap was the exception repair: a moved-from
`merge_bomb` that keeps its value makes "the source kept a husk" invisible, so the bomb now marks what
it moved from, and a throwing *key compare* was added because the move bomb can only ever produce the
drop-it case and never the keep-it one. The eighteen that survive are all one class -- behaviour-identical by
construction, so no correctness test can see them, and nothing in the walk itself is among them.
Fourteen are the two pipeline gates: their thresholds, `index_bytes_for`'s multiply, and the two
comparisons. One is `move_home`, which only moves an element back towards its home group. One is the
`-fno-exceptions` arm that the test build does not compile. Two are early exits whose only effect is
to skip work that would have produced the same answer -- the self-merge guard is one, and the reason
is worth knowing: without it the walk finds every key in itself, keeps every element and rebuilds
nothing, so it is O(n) of wasted probing rather than a wrong answer.

*What the source keeps.* References and iterators into either map are invalidated, and the source's
order changes -- every element taken out of the middle leaves a gap the elements behind it close.
Both are `erase()`'s existing behaviour and both are in `README.md` 3.3.8 rather than left to be
discovered.

**`replace()` hashes ahead too, once the reason it could not was looked at properly** (2026-09-11,
issue #244 item 4, asked as "would it be simpler going from back to front"). It was the last bulk
build path with no lookahead, and the obstacle looked structural: removing a duplicate pulls
`back()` into the hole, and a lookahead has already hashed the elements near the back.

**Back to front does not help** -- it makes it worse. Going down from the end, everything above the
cursor is already *placed*, so `back()` is in processed territory: moving it leaves a bucket pointing
at a dead index that then has to be found and repointed. A stale ring is traded for a stale index.

**What does work is noticing that the pull disturbs exactly one position**, the last. While the
container is longer than the window by more than one, the window cannot contain the element that
moves, so every hash in the ring stays valid -- and a duplicate costs one re-hash of the slot it
lands in, not a flush of the ring. Only the last `pipeline_depth + 1` elements run unpipelined. The
behaviour is unchanged: same survivors, same final order, same number of moves, which the test checks
element by element against an independent copy of the plain loop rather than against a set.

ns per element, rebuilding through `replace()`, medians of three:

| | no duplicates | 5% duplicates | 50% duplicates |
|---|---|---|---|
| 200000, before | 5.77 | 8.51 | 8.73 |
| 200000, after | **4.87** | **5.54** | 8.48 |
| 2000000, before | 10.76 | 14.99 | 16.85 |
| 2000000, after | **8.04** | **9.93** | 16.15 |

1.19-1.34x with no duplicates and **1.5x at five percent**, which is the shape that matters: a
duplicate does not consume the lookahead, so at fifty percent half the iterations do not advance and
the pipeline barely fills -- 1.03x, and nothing is lost.

**The boundary has slack on both sides, which is the useful thing the mutation sweep said.** Six
mutants survive and all six are on the loop condition. The exact requirement is
`size > value_idx + pipeline_depth`; the shipped `+ 1` is one stricter, so tightening it is correct
(that is one survivor) and loosening it by one is *also* correct, because the iteration that could
read a stale entry is the one after which the loop exits (that is two more). The rest turn the
pipelined loop off entirely and fall through to the plain one. The margin is kept: an off-by-one here
returns a wrong answer rather than crashing, and one element of pipelining is a cheap price for not
sitting on the edge.

One survivor was worth knowing about separately, and cleanup then removed the line it was on:
`lookahead < size()` to `<=` read `m_values[size()]`, which is undefined but lands inside the
vector's *capacity*, so neither sanitizer objected. It is the same class as the note elsewhere about
reading one slot past a bucket. The guard survived mutation because it was dead: `lookahead` was
always `value_idx + pipeline_depth`, and the loop condition already says that element exists. It is
gone, and with it the variable.

**The ring holds the hash taken apart, not the hash** -- fingerprint word, home group and the block
pointer, filled by the same `fetch` that issues the prefetch. Decomposing at the far end of a
sixteen-deep ring instead means the table load, the `& 7` and the shift happen twice, once in the
probe and once in the placement, and the block's address is formed three times. Slope of two round
counts, 2000000 elements: **90.8 instructions per element to 85.6** with no duplicates, 83.7 to 81.9
at fifty percent. Wall clock moved the other way by 3-5%, which is code layout luck; the instruction
count is the measurement. It is the same finding the bulk visit banked a day earlier, in a third
place -- the 88 byte block wants its address formed once and then reused.

**And the pipeline only runs once the work is out of cache, which the first version of this got
wrong.** The ring round trip is not free: it costs **13.2 instructions per element** (73.4 to 86.6 at
n = 1024), and a prefetch of a line already in cache still occupies a load port. Against the
unpipelined loop, no duplicates, it was a fifth slower at 32768 elements and level at 65536 -- and
the two sizes the change was first measured at, 200000 and 2000000, are both above the crossover,
which is how it went unnoticed. **Two sizes on the same side of a cache is one size**, which is the
octave rule from the benchmark harness arriving in a place that has no harness.

Which quantity the gate compares took a second pass to get right; the first one shipped wrong. See
the next entry but one.

With the gate and the decomposed ring in, against main, three interleaved repeats, median:

| n | no duplicates | 5% | 50% |
|---|---|---|---|
| 8192 | 0.98 | 0.98 | 0.96 |
| 65536 | 0.81 | 0.94 | 0.91 |
| 262144 | 0.64 | 0.65 | 1.01 |
| 2000000 | 0.72 | 0.68 | 1.01 |

The 1.20x below the gate is gone and the wins above it got bigger -- **1.5x at a quarter million**,
where the ungated version read 0.90. Fifty percent duplicates is a tie at every size, as it was
before: a duplicate does not advance the cursor, so the pipeline never fills.

The gate costs mutation score, unavoidably: survivors went from 6 to 26, and the twenty new ones are
all on the footprint arithmetic, the threshold and the two prefetches. A mutant that moves a
*performance* gate cannot be caught by a correctness test -- either side of it returns the same
answer -- so the number to read is that no survivor is in the loop body. Do not chase it by
loosening the tests.

`do_insert_range` and `do_visit` pipeline too and have no such gate. Measured, and neither needs
one -- below.

**`insert(first, last)` sizing the table from the range: built, measured, declined** (2026-09-11,
issue #248, `scripts/ab/range_insert.cpp`, Ryzen 9 7950X, clang 22). Worth **2x** on the common case
and not shipped, because every version that gets the 2x is a heuristic about data the map cannot see.
Three attempts, each wrong in a way worth keeping.

*First: `reserve(size() + std::distance(first, last))`, which is what the issue proposed.* A range is
not its number of distinct keys. At a 99% duplicate rate it asks for **128 times** the index the map
ends up needing -- 2097152 buckets for 9923 elements, kept for the map's lifetime -- and it is not
even faster, because every probe then misses in a mostly empty table: **1.21** against growing.

*Second: sample the head, extrapolate the fresh-key rate.* This is the one worth remembering, because
it looked perfect. On `range_insert.cpp`'s own duplicate model it reproduced the growth bucket count
**exactly** at every rate from 0% to 99% -- that model draws duplicates from a pool growing with the
range, so the fresh rate is stationary. Real ranges are not. **2048 keys drawn from ten thousand come
back 90% new**; that is the birthday bound, not a duplicate rate, and extrapolating it over a million
elements predicts nine hundred thousand distinct keys instead of ten thousand. 64x. The unit tests
caught it, the benchmark could not have, and the general lesson is in CLAUDE.md.

*Third: read the sample as a capture-recapture.* Split the sample; if the range draws from `total`
distinct keys and the first half caught `warmed` of them, the share of the second half that is a
**repeat** estimates `warmed / total`, so `total = warmed * measured / again`. The statistic is the
repeats, not the freshness, and it is informative exactly where the rate is not. At a million
elements, buckets against the 16384 / 131072 / 1048576 actually needed:

| distinct keys | first try | second try | capture-recapture |
|---|---|---|---|
| 10000 | 2097152 | 1048576 | **16384** |
| 100000 | 2097152 | 2097152 | **131072** |
| 500000 | 2097152 | 2097152 | **1048576** |

0.51 of the time fully distinct, 0.48 at 70-90% distinct, neutral below half distinct, clamped so no
estimate can exceed reserving the range's length. It works. **It was still declined**, because an
ordered range -- every key once, then the set again -- defeats any prefix sample and no scheme fixes
that; what is left is a hash map guessing at its caller's data with a 4x memory consequence when it
guesses wrong. The 2x is not worth owning that.

*And the mechanism is not the one the issue assumed.* The issue says growth is expensive because each
doubling rehashes the index, and the pipelined loop also loses its sixteen in-flight prefetches.
Neither is the cost. Reserving **only the value container** captures nearly all of the win, and
reserving **only the buckets** is worse than not reserving at all. At a million distinct keys,
ns per element:

| | ns/element |
|---|---|
| no reserve | 19.6 |
| values only | 10.4 |
| values and buckets | 9.9 |
| buckets only | 22.3 |

So it is the value vector's geometric reallocation -- copying 16 MB repeatedly -- and the index
rehash is nearly free beside it. That also rules out the cheap version of the idea: reserving just
the values is *not* the safe half, since a value is 16 bytes against the index's 5.5 per slot, so
guessing wrong there costs more, not less.

*`std::distance` is only free on a random access iterator.* Over a `std::list` it walks the range a
second time, a **1.20 loss** at a high duplicate rate. Any future attempt at this must exclude
merely-forward ranges.

What was kept: `range_insert.cpp` gained a duplicate rate, a `std::list` source and a bucket count in
its output, which is what made all of the above visible.

**The paired harness has a systematic bias on `rmissstr`, about 3.6%** (2026-09-11). It showed up as
a 1.037-1.039 "win" in two consecutive PRs, against two different baselines, for changes whose
mechanism could not reach a lookup at all. Running `scripts/ab/run.sh -r HEAD` on a clean tree --
**the same header on both sides** -- reads `rmissstr` **1.036**, `buildbig` 0.983, `buildstr` 0.984
and `hashstr` 0.872. So the candidate side of a paired run is systematically faster on those, and a
single-digit-percent reading on them means nothing. The null run is the check, it costs one run, and
it should be taken before believing any sub-benchmark delta under about 5%.

**A block's prefetches should step from its start, not jump to its end** (2026-09-11, issue #250,
`scripts/ab/prefetch_lines.cpp`, Ryzen 9 7950X, clang 22). Raised by a cleanup review as "a quarter
of blocks span three cache lines and `prefetch_block` only asks for two". The geometry is right, the
first fix for it was wrong, and the second is worth 3.3%.

*The geometry.* A block is 88 bytes with `alignof == 4`, so blocks sit at `base + 88i`. `88 % 64` is
24 and `gcd(24, 64)` is 8, so `p % 64` walks the entire cycle `{0, 24, 48, 8, 32, 56, 16, 40}` from
any starting offset -- **no allocation avoids this, and over-aligning the array does nothing**. The
two offsets above 40 make the block span three lines, so exactly 25% do, and for those the middle
line begins at byte 8 or 16 *of the block*: it holds all eight overflow counters and half or more of
the fingerprints, which is what the probe reads first.

*The obvious fix is a loss.* `prefetch_block` asked for `p` and `p + 87` -- first line and last. Add
a third at `p + 64` so all three are named, and it gets **slower**: 12.85 ns per block against 12.67.
The hardware fetches what it can see coming, and the extra prefetch buys an instruction and a load
port slot rather than a line.

*Stepping instead of spanning is the fix.* `p` and `p + 64` are always two consecutive lines. For
the three quarters of blocks that span two lines that is exactly what `p` and `p + 87` already
prefetched -- the two are identical there. For the other quarter it takes the middle line instead of
the last, and the last holds only `m_index[14..15]`, read only when the matching lane is high. Same
instruction count, and on an array of two million blocks -- 176 MiB, far past the last level cache --
walked in a materialised random order with the same sixteen-deep lookahead the pipelined loops use,
reading sixteen fingerprints, one counter and one index at a lane derived from the fingerprints so
the index read cannot start before the compare:

| prefetches | median ns/block |
|---|---|
| none | 44.9 |
| `p` | 13.19 |
| `p`, `p + 87` -- first and last, the old shape | 12.67 |
| `p`, `p + 64`, `p + 87` -- all three | 12.85 |
| **`p`, `p + 64` -- stepped, shipped** | **12.28** |

Seven rounds of seven with no overlap between the stepped and the old distributions. It reproduces
on the real harnesses at four million entries -- the bulk visit 0.971 and a reserved range insert
0.952, five of five below 1.0 each -- and is smaller but the same sign at a quarter million and in
grow mode.

So `prefetch_block` is now a loop stepping by 64 to the end of the block, which both compilers fully
unroll (two prefetches, no branch). **The loop is what makes it right for `group_big`**: 152 bytes
spans up to four lines, and first-and-last covered two of them and skipped as many as two in the
middle. Stepping gives it three.

*What nearly went wrong.* The third-prefetch version measured on the three real harnesses first read
-1.3% on the bulk visit and -2 to -3% on the range insert and looked like a win. Five rounds at each
size put all of it between 0.975 and 1.017 -- **inside the +-3% layout luck** -- and the L1 miss
counts did not move either (44.6M against 44.9M at four million). A mechanism that is provably true
does not make a delta of that size real, and the microbenchmark is what separated the two candidate
fixes: on the real harnesses they are both a smear, and the difference between them is 3.3%.

*The score does not move and should not.* Paired against main, ten epochs, five points an octave:
geomean **1.001** over nineteen workloads, everything inside 1.5% except `rmissstr` at 1.039. That
one is **not attributable** -- a lookup's probe calls `prefetch_index`, which this did not touch, and
`prefetch_block` is reached on the score only through the rehash inside `build` and `churn`. Two runs
agreeing on it means nothing either, since they are the same two binaries and therefore the same code
layout. It is the +-3% layout luck, measured slightly above its usual size. The change's actual
subjects -- the bulk visit, the range insert, `replace()` -- are not in the score at all, which is
why the microbenchmark carries this one.

One machine. `prefetch_lines.cpp` is committed so that a CPU with different prefetchers can be asked
the same question, which is the one thing that would change the answer.

**The other two pipelines do not want the cache gate, and the gate's own footprint model was wrong**
(2026-09-11, issue #247, `scripts/ab/{range_insert,bulk_visit,replace_bulk}.cpp`, Ryzen 9 7950X,
clang 22, `uint64_t` keys). Each loop was built twice into its own binary -- once as shipped, once
with the ring taken out and the body written as a plain per-element loop, same checksums -- and the
two alternated.

*The premium is flat and predicts nothing.* Instructions per element, plain against pipelined:
`replace()` 73.4 to 86.6, `do_visit` 70.3 to 86.0, `do_insert_range` 81.0 to 97.9 -- +13.2, +15.7,
+16.9, each constant across sizes to 0.2 instructions. The loop carrying the *largest* premium is the
one that never needs a gate.

*The range insert.* Medians of seven, pipelined over plain: 1.02 and 1.01 at n = 1024 and 4096
reserved, 1.08 at 4096 growing, then 0.75 at 16384, 0.65 at 32768, 0.48 at 262144 and 0.54 at four
million. So it is **slightly negative below a few thousand elements and a clear win from sixteen
thousand**, and the small sizes are bimodal between repeats -- 0.46 and 1.52 both appear at n = 4096,
because the round rebuilds the map and the allocator does not do the same thing twice. No gate: the
loss is inside the noise of the sizes where it happens.

*The bulk visit.* Medians of five: all hits, 1.08 / 1.05 / 1.01 at 1k / 4k / 16k, then 0.90 at 128k
and 0.83 at four million. Half the keys missing, same map sizes: 0.91 / 0.89 / 0.88, then 0.84. So
the only loss is a caller whose keys **all** hit a map below about sixteen thousand, and at those
same sizes a 50% hit rate *wins* 11%. A footprint gate would give up the win to avoid the loss, on an
axis the map does not know until it has already done a chunk's worth of work. It could measure the
first chunk and fall back, which is the way to get both -- rejected on cost: a second loop body
inside a function that already has three passes is an inlining-budget change, which by the rules
above the paired harness cannot measure at all, for at most 8% on one slice.

**And the same sweep, run with a second value size, says `replace()`'s gate was comparing the wrong
number.** It shipped comparing values-plus-index against 1 MiB. At one value size that model and an
index-only model are indistinguishable -- both are proportional to n -- so the original measurement
could not tell them apart. With a 1032 byte value they separate, and the footprint model loses:

| n | index | 16 byte value | 1032 byte value |
|---|---|---|---|
| 10000 | 88 KiB | 1.05 | 1.14 |
| 14000 | 176 KiB | 0.75 | 1.06 |
| 20000 | 176 KiB | 1.05 | 0.98 |
| 28000 | 352 KiB | 0.84 | 0.98 |
| 40000 | 352 KiB | 0.82 | 0.97 |
| 80000 | 704 KiB | 0.91 | 0.91 |
| 160000 | 1408 KiB | 0.68 | 0.91 |

Both columns cross over at the same **index** size, not at the same footprint -- which is what the
mechanism says, since the index is the only array the prefetch is for and the values are walked in
order for the hardware to handle. Under the footprint model a 1032 byte value opens the gate at ten
thousand elements, where the measurement says 1.14. So the gate is now `index_bytes > 256 KiB`, and
`pipeline_min_bytes` is `pipeline_min_index_bytes`.

Note the two rows at each index size: 176 KiB reads 0.75 at n = 14000 and 1.05 at n = 20000, same
array, opposite answers, because the load factor runs from 0.43 to 0.61 between doublings. **The
octave rule applies to this axis too** -- a single n is not a measurement of a cache effect any more
than it is of a load-factor effect, and the first version of these numbers took one point per size.

Unconfirmed mechanism: the ring only pays where the out-of-order window cannot already find the
independent work, which would explain why the range insert -- whose body carries a vector append and
its capacity branch -- is the one that never loses. Not tested; a stall count on the two bodies would
settle it.

The gate change took a test with it, twice over. The 64 KiB mapped type that used to cross the
footprint gate at sixteen elements does not cross an index gate at all, so the window boundary is
now swept a different way: a duplicate walked through every one of the last forty positions of a
thirty-thousand element container, which is the same boundary from the end it actually lives at. And
mutation found a state no test had: `replace()` keeps an index it has already grown, so replacing a
large map with a handful of elements leaves the index past the gate and the container shorter than
the lookahead window. The `&&` is what stops the prologue reading past the end there, and turning it
into `||` survived until that case was written down.

Two measurement notes. The control first tried for the range insert was `insert(first, last)`
against a caller's own loop of single inserts, which is the wrong one: it also carries ~15
instructions of call boundary per element (see the clang/gcc entry) and shows the range winning at
every size, hiding any pipeline penalty underneath. The control has to be the same entry point with
the ring removed. And `replace_bulk.cpp` times the container copy along with the call, which is
invisible at 16 bytes a value and swamps everything at 1032 -- the second table above was taken with
the copy moved outside the timed region.

**Taking the hash apart once per insert instead of twice, and the shared pipeline that was not worth it** (2026-09-11,
issue #244, from four cleanup reviews of the range insert). Three of the four items were measured;
two are in and one is not.

**In: an insert derived the same three things from its hash twice.** `probe` computed the
fingerprint word, the counter and the group; `place_group` computed all three again. They sit in
separate functions separated by a fingerprint store and a possible reallocation, so for a key whose
compare is a call -- where `m_equal` is opaque -- nothing is common-subexpression-eliminated across
them. (The pipelined insert's lookahead derives a group too, but for the element *sixteen ahead*, so
that one is not a duplicate and cannot be shared.) `probe`, `place_group` and `do_place_element` each gained a variant taking the pieces with the
old signature as a thin wrapper. **That alone is 4% of a single clang insert -- 97.0 instructions to
93.0 -- and 3% of a gcc `++m[k]`**, because `place_group` had been re-deriving the fingerprint word
across an inline boundary from a caller that had just computed it.

**In: the probe does not ask for a block the caller is already holding.** A pipelined insert formed
the home group to prefetch against sixteen elements earlier, so it hands the group itself to
`probe_at_home` rather than a number to find it by -- the prefetch is then skipped by construction,
and skipped for exactly the one group the caller knows about. Another 3%.

That started as a `template <bool Prefetch>` flag on the probe and the flag was wrong twice over,
which four review passes caught and the measurement did not: it suppressed the prefetch for *every*
group of the overflow walk, which nobody had asked for, and it kept suppressing it on the sixteen
elements after a growth -- the ones whose earlier prefetch went to the array growth had just
replaced. A compile-time flag was standing in for a run-time fact. Passing the group makes the fact
true rather than asserted.

Together, ns per element rebuilding a map from a vector of pairs: 200000 reserved 5.42 to 5.08,
growing 8.41 to 8.04; two million reserved 7.50 to 7.18, growing 26.48 to 26.08. Lookups byte
identical on both compilers, which is the bar for touching the probe.

**Out: one shared pipelining helper for the rehash and the range insert.** All four reviews raised
the duplicated ring, and the concern is right -- the read-before-refill ordering is written twice and
a drifted copy of it returns a wrong answer rather than crashing. Built anyway, as a helper taking
the fetch and the per-element work as callables so each caller keeps its own hoisting. **It costs the
rehash 1.9 instructions per element**, 45.7 to 47.6, and 1.3% of its time on **six of six**
interleaved pairs; strings were neutral. Passing the element index through the callable, on the
theory that a captured counter was the cost, recovered none of it -- the price is the indirection.

The rehash wants `groups`, `mask` and `shifts` in locals, and that is load-bearing rather than
stylistic: a fingerprint store is a `std::uint8_t` store that may alias the container's own data
pointer, so reading them through `this` costs a reload on the address chain of every element. The
range insert *cannot* hold them, because growth moves the array underneath it. So the two loops
differ in exactly the part that matters, and what is left to share is the ordering -- which is now
stated once, in a comment each site points at, at no cost. **The duplication is measured to be
cheaper than the fix**, which is the kind of thing worth writing down so it is not re-proposed.

**Not attempted: pipelining `replace()`.** It is the last bulk build path without a lookahead, and it
is genuinely harder -- the loop swap-erases duplicates from the back, so `m_values.back()` moves
under a lookahead and the index does not advance on a duplicate. The argument for doing it was that a
shared helper would make it tractable; with that helper rejected on measurement, it would be a third
hand-written ring for a path most callers never take. Left open.

**Chunks or a sliding ring: the rehash and the bulk visit want opposite answers, and the reason
generalises** (2026-09-11, asked as "would a streaming approach perform better" and then "but doesn't
resize use that streaming too, it looks like it would be better with chunking"). Both shapes were
built for both loops. Each loop is faster in the shape it already had, and the principle that decides
it is worth more than either result.

**The bulk visit wants chunks.** Three passes against a sliding ring, `map<uint64_t, size_t>`, ns per
lookup: 26.6 against 31.6 at four million all hits, 30.4 against 34.7 at sixteen million. Both retire
**224.5 instructions per lookup, identical to the tenth**; the ring spends 15% more cycles. The gap is
15% on all hits and 6% at half hits, and a miss reads no value, so what the ring loses is the *value*
loads -- the third pass issues sixteen back to back and nothing else creates that parallelism.

**The rehash wants the ring.** Chunked against the shipped streaming version, ns per element:

| | 200000 | 1000000 | 4000000 |
|---|---|---|---|
| `uint64_t`, streaming | **2.24** | **6.16** | **17.19** |
| `uint64_t`, chunked | 2.42 | 6.89 | 18.03 |
| string, streaming | **7.70** | **10.73** | **20.68** |
| string, chunked | 8.25 | 12.57 | 22.91 |

Same signature, other direction: for a string key both retire ~137 instructions per element and the
chunked one spends **12.5% more cycles** (99.1 against 88.1).

**What decides it is how many dependent random accesses an element has.** A visit has two -- the
group's block, then the value the slot points at. Separating them into passes lets the second batch
issue sixteen at once, which is the only parallelism available on an access that is not prefetched. A
rehash has **one**: it reads the block and writes into it, and the value is already where it belongs.
With nothing to batch, the only thing that matters is how much time sits between a prefetch and its
use -- and a ring maximises exactly that, giving every element a full sixteen *placements* of cover.
A chunk gives the first elements of each chunk only the rest of pass one, and pass one is hashing,
which is far cheaper than placing. So the chunk's early elements stall.

So: **chunk when an element has two dependent accesses, stream when it has one.** Predicting from the
shape of the loop rather than from the code, and the string column is the check -- it is the case with
the most work per element and it moves the most, in both loops, in opposite directions.

**A bulk `visit()`, and the `prefetch(key)` API it replaced -- which was measured against the wrong
baseline** (2026-09-11, issue #232, asked as "would it make sense to have a bulk api"). `prefetch(key)`
shipped documented at 1.5x and was reverted the same day. Both halves are worth keeping, because the
mistake is one any lookup-pipelining feature invites.

**The baseline was the bug.** Its harness (`scripts/ab/prefetch_api.cpp`, deleted with it) compared a pipelined loop
against a plain one in which the key was *acquired inside the loop body* -- a random index into another array -- so
each iteration's key miss sat in front of its map miss and neither overlapped with anything. Against
that, pipelining read 1.5x. But **collecting the keys into a batch first and looking them up
afterwards is worth 1.5x on its own**, with no API at all: 50.9 ns to 33.4 per lookup at four million
entries, because two short loops each saturate their own memory parallelism where one long body does
not. The feature was being credited with the batching. In the batched loop -- which is the shape the
README's own example showed, keys already in a container -- `prefetch(key)` measured **a 3% loss**,
34.3 against 33.4. Reverted rather than re-documented: with `visit` in it covered a narrow case
badly, and removing it before 5.0 was free where after would have been breaking.

**What replaced it is boost's shape**: `concurrent_flat_map::visit(first, last, f)` with
`bulk_visit_size = 16` -- a chunk in three passes rather than a sliding ring. Hash every key in the
chunk and fetch its home block; match the fingerprints, by which time the blocks have arrived;
compare the keys and call `f`. Every block in the chunk is in flight at once. Against the same batch
looked up one key at a time, `map<uint64_t, size_t>`, medians of three:

| entries | work | one at a time | `visit` | |
|---|---|---|---|---|
| 4000000 | hit | 33.97 | **26.34** | 1.29x |
| 4000000 | half | 34.37 | **28.44** | 1.21x |
| 16000000 | hit | 36.53 | **30.40** | 1.20x |
| 16000000 | half | 37.31 | **31.53** | 1.18x |

Those are after a cleanup pass; the first working version read 1.07-1.14x. The difference is one
line: a block is 88 bytes, so `groups[home[i]]` is a multiply, and the first version did it three
times per key -- once to prefetch, once to match, once to compare. Holding the pointer instead is
worth **10% of the whole operation**, which is a reminder that the address arithmetic of a
non-power-of-two block is not free just because it is not a memory access.

**The reason it was built is not the reason it works.** Boost prefetches the *element* in pass 2, and
that was the whole argument for a batch here: the value load is the second dependent access, the one
a single lookup cannot hide because the address is unknown until the block arrives -- which is what
issue #229 measured and closed. In a batch the address *is* known. Isolated, it is worth **+3.8% on
all hits and -4.8% at half hits**, so it is not in the shipped version: an absent key has nothing to
fetch, and a present one is already covered by the out-of-order window once the passes are separated.
**The gain is the separation, not the fetch.** #229's conclusion stands and this does not reopen it.

**Three passes rather than a sliding ring, and the ring is the one that loses** (asked 2026-09-11 as
"would a streaming approach perform better, I guess not because this can unroll"). Built both:
`map<uint64_t, size_t>`, medians of three, ns per lookup.

| | 200000 hit | 4M hit | 16M hit | 4M half | 16M half |
|---|---|---|---|---|---|
| three passes | **6.94** | **26.63** | **30.35** | **28.27** | **31.67** |
| sliding ring | 7.53 | 31.64 | 34.72 | 29.28 | 32.05 |

**And it is not code generation**, which was the reason offered for expecting it: both retire
**224.5 instructions per lookup**, identical to the tenth, and the ring spends **15% more cycles**
(339.0 against 294.5 at four million, all hits). Same work, more waiting.

The split between the columns says what it is waiting for. The gap is 15% on all hits and 6% at half
hits, and **a miss reads no value at all** -- so what the ring gives up is the *value* loads. The
third pass issues sixteen of them back to back, and nothing else creates that parallelism, because
the value is the one access that is not prefetched (see above: prefetching it is a net loss). The
ring issues one per iteration and leaves the overlap to the out-of-order window.

What makes that conclusion clean is that the ring has the *better* block prefetch: every element
gets a full depth of real work between its prefetch and its use, where a chunk gives its last element
less than its first. It wins that axis and still loses by 19%.

A ring is also the shape that has to be read before the slot it frees is refilled -- `ring[i % depth]`
is the slot `i + depth` writes -- and getting that backwards returns a wrong answer rather than
crashing. It was got backwards twice while this was being built, once in a test and once in a README
example, and only the test caught it. So the chunk is both faster and harder to get wrong.

**A chunk of 16 is not load-bearing**: 8 through 32 measured within 2%. Sixteen is what boost uses.

**Mutation swept: seven survivors, all of them correct-but-slower or unreachable**, which is what a
pipeline should look like -- it exists to change timing and nothing else. Deleting the block prefetch
has no observable result at all. `++i` to `--i` leaves every key visited exactly once, because the
outer loop re-chunks from wherever `first` reached, at sixteen times the hashing. Deleting the `break` in the
lane walk keeps scanning lanes whose fingerprints cannot belong to the key. Turning the counter test into one that always passes makes the
fallback run for every key. `m_group_mask != 1` is unreachable while the smallest array is four groups. The
two that would have been real were both **caught**: `--first`, and the counter's `!= 0` turned into
`!= 1`, which is the same hole the probe split had and is covered here by the same steered
construction.

**Tiny pointers, and the bound insertion order puts on the value index** (2026-09-10/11, issue
#229). Asked whether a tiny pointer (Bender et al., SODA '23; Flattened-TPHT, VLDB '26) could shrink
the four of five and a half bytes per slot that are the `uint32_t` value index. **Half of it is
settled without building anything, and the half that needed a measurement is now measured and lost.**

**A tiny pointer needs the referrer to choose where the referent goes.** This map's value vector is
insertion-ordered and that order is public contract -- `values()`, iteration, `extract()`,
`replace()`. The user chooses every position, so the map from slot to vector position is an
arbitrary permutation of n things and the index has to encode it: **log2(n) - 1.44 bits per entry**
however the bits are arranged, at home or away from it. The issue's premise, a narrow index into a
range the group implies, has nothing to stand on -- the group implies nothing about where the value
sits. That bound caps every width reduction, tiny or otherwise:

| value index | index B/entry (4 x 1/load, octave geomean 1.77) | total B/entry, 8 byte value |
|---|---|---|
| `uint32_t`, shipped | 7.1 | 32.6 |
| 24 bit, built and measured, score 0.995 clang / 0.983 gcc, capped at 2^24 | 5.3 | ~30.8 |
| the information-theoretic floor at n = 2^20 | 4.1 | ~29.6 |

So the whole axis is worth **at most ~9% of memory and no speed** inside the contract, and the entry
below ("Why none of them could have won") already says the L1 fills a narrower block saves are not on
the critical path. It should not be prototyped.

**What that leaves is a different container**, values in a hash-addressed table so the *map* chooses
each value's slot. Its one possible speed win is that the value's line becomes a function of the
group, so it can be fetched in parallel with the fingerprints instead of after them -- this map pays
two dependent memory accesses on a hit where a flat map pays one, which is most of the 10-13% boost
leads by on a fresh hit. Whether a software prefetch actually recovers that latency is the one thing
argument cannot settle, so it was measured, by making the prediction true inside the *shipped* map:
`scripts/ab/value_prefetch.{cpp,sh}` inserts keys in home-group order, exactly twelve per group, so
the values of group g are twelve contiguous entries at `values + g * 12`, and a patched `probe()`
prefetches that address before touching the group. Perfect packing and no holes, so it is an upper
bound on what the container could get.

Four variants: **A** random fill and no prefetch (today), **B** grouped fill and no prefetch
(locality alone), **C** grouped and prefetching one to three lines (the question), **D** random and
prefetching (the tax on a wrong address). ns per lookup, `uint64_t` key, n = 12 x 2^p:

| | p=16, hit | p=18, hit | p=20, hit | p=16, miss | p=18, miss | p=20, miss |
|---|---|---|---|---|---|---|
| A random, no prefetch | 14.74 | 52.17 | 64.69 | 6.87 | 25.58 | 38.06 |
| B grouped, no prefetch | 14.95 | 51.65 | 65.53 | 6.68 | 26.26 | 38.30 |
| C grouped, 1 line | 13.75 | 48.06 | 58.87 | 8.30 | 30.40 | 38.65 |
| C grouped, 3 lines | 15.28 | 46.60 | **53.98** | 11.21 | 36.33 | 41.38 |
| D random, 1 line | 15.15 | 54.91 | 65.70 | 8.18 | 30.55 | 38.66 |

**Three things kill it.**

*The gain does not clear the bar, and the bar was set before the run.* The rule was hit(C) at or
below 0.75 x hit(B) at both DRAM sizes, for integer and string keys. Best case is 0.824 at p=20 with
an integer key, and **strings are 0.99** -- 148.74 to 147.11 ns at p=18, which is noise. For a string
the value load was never the bottleneck: the key compare is a second dependent load into the string
body and it happens either way.

*The miss gets worse everywhere.* Integer +24% at p=16, +16% at p=18; string +13% and +18%. A miss
never reads a value, so every one of those prefetches is wasted work on the critical path, and a
50/50 workload is a **net loss** at p=18 and about 8% ahead only at p=20 with an integer key.

*Locality alone is worth nothing.* B is A to within half a percent at every size and both key types,
so the container gets nothing for free from owning placement -- its entire case is the prefetch.
And even the ceiling is modest: hit(B) minus miss(B) at p=20 is 27.2 ns, the whole value load, and
three lines of prefetch recover 11.6 of it, 42%, for six instructions and 2.8 extra L1 fills.

So the container would give up insertion order, `values()` and vector-speed iteration, land at an
estimated 28-30 bytes per entry (between boost and absl, not below them), lose on misses, pay a
14-19% tax in cache, and win at most 17.6% on an integer hit at twelve million entries. **Closed.**

**And a measurement trap worth more than the result.** The first run of this said the grouped layout
alone was worth 18-25% at p=20 -- which is impossible, since a miss never reads a value and the
index is structurally identical either way. The tell was in the instruction counts: **300 per lookup
for a probe that stops in its home group**. `perf stat` counts the whole process, and building a
table of twelve million entries by rejection sampling is most of the run at that size, so what the
"locality" column actually measured was that a random-order fill is slower than a grouped one. The
harness now reports every figure as the **slope of two rep counts**, which subtracts any fixed cost
exactly, whatever it is; A and B then agree on instructions to the tenth, which is the physical
check that the subtraction worked. Any harness that builds its own table and measures the process
has this bug available to it.

**Splitting the probe past the home group: kept, for keys whose compare is a call** (2026-09-10,
issue #233). The entry below records this being tried on 2026-09-10 and reverted -- string miss
138.3 to 126.5 instructions, integer miss 55.8 to 65.9 and integer hit 69.1 to 85.0, "it buys 8% of
the one workload we lose and spends 20% of the ones we lead by most". Both halves of that reproduce
exactly. What was missing is that the two halves are selected by the **key type**, and can be had
separately.

**Why the frame is there.** For a `std::string` key the probe builds a frame and spills the loop
state -- the counter, `this`, the mask, the delta, the hash and the broadcast fingerprint -- before
the first group is compared, because the key compare is a `memcmp` call and everything the loop
keeps live has to survive it. Only about 3% of lookups leave the home group, and the delta and the
mask are the only things the rest of the walk needs, so the frame is paid by every lookup for a path
almost none of them take. Where the compare is a register compare there is no call, nothing has to
survive anything, and splitting only adds a call.

**Measured per key type**, one map per binary, instructions per lookup, clang 22 (miss):

| key | shipped | split | |
|---|---|---|---|
| `std::string` | 140.6 | **128.8** | **-8.4%**, and 19.65 ns to 17.85 |
| `std::string_view` | 134.2 | **122.4** | **-8.8%** |
| `std::uint64_t` | 56.1 | 67.1 | +20% |
| `std::pair<std::uint64_t, std::uint64_t>` | 50.2 | 70.2 | **+40%** |
| 64 byte POD, `memcmp` compare | 111.4 | 109.4 | -1.8% |

So the split is gated on `detail::key_compare_is_call<Key>`, and **integer codegen is byte-identical
to the shipped header** on both compilers -- 56.1 / 59.9 / 70.0 under clang and 58.1 / 57.0 / 68.6
under gcc, before and after, on miss, hit and half. The `map<uint64_t, big_value>` workloads are
identical to the tenth as well. Strings gain everywhere: clang miss -8.2%, half -4.3%, hit -1.7%,
insert -3.2%; gcc miss -4.0%, half -2.3%, hit -1.4%, insert -3.0%; churn and insert-erase -1 to
-2%; the one loss is `++m[k]` on a present string key, +2.2% under clang and +0.3% under gcc.
Timings, three runs each, medians: clang string miss **19.65 to 17.85 ns**, hit 25.00 to 24.55, half
24.60 to 24.00; gcc 16.15 to 15.80, 22.95 to 22.65, 22.45 to 22.35. Every cell improves.

**The trait is `is_trivially_copy_constructible`, and the difference from `is_trivially_copyable` is
a 40% bug.** `std::pair` and `std::tuple` write their own copy *assignment*, so both libstdc++ and
libc++ report `is_trivially_copyable_v<std::pair<std::uint64_t, std::uint64_t>>` as **false**. The
first version of this keyed on that, and split one of the commonest key types after string and
integer. `std::string_view` is the other way round -- trivially copyable and still a call -- and no
trait separates it from `pair<uint64_t, uint64_t>`, since both are sixteen bytes and hold no
indirection the language can see. It is named explicitly.

A pair and a tuple are then asked about their *elements* rather than about themselves, which CI
found rather than reasoning: **MSVC's `std::tuple<int, int>` is not trivially copy-constructible
where libstdc++'s and libc++'s is**, so asking the aggregate gave a tuple key a different probe on
Windows than everywhere else, and all four Windows legs went red on the `static_assert` that pins
it. Recursing is also simply what comparing a pair does. A user type that is trivially
copy-constructible and still compares through a call is treated as cheap and loses the ~2% in the
last row, which is the harmless direction to be wrong in.

**And the shape of the code is load-bearing.** Factoring the per-group compare into a helper
returning a `probe_result`, so that the split and unsplit paths share it, is tidier and costs **gcc
26% of an integer hit** (57.0 to 72.0) and clang 6.7%, fully inlined, purely from returning a
twelve-byte struct through an inner function. The loop is written out instead, and the unsplit path
is the shipped loop unchanged, which is what makes the integer columns identical rather than merely
close.

**Mutation swept, and the one hole it found is closed.** The split duplicates the probe's
"did anything of this class overflow past me" test into an inline home-group check, and turning that
`== 0` into `== 1` was **caught by nothing** -- it stops a probe at home whenever exactly one entry
of the key's class went past, so that entry becomes unfindable. Reaching it by chance needs a string
whose home counter happens to be one; `test/unit/probe_split.cpp` reaches it on purpose, with
`fuzz_group_index`'s construction -- an identity hash and a key type whose copy constructor is user
provided, so the key names its group and its fingerprint class *and* takes the split path. Fill
group 0, send one key of class 3 past it, look that key up. Of the other survivors, all are
performance-only (dropping the `!` on the gate, deleting a prefetch, `||` to `&&`, deleting an early
return) or write to `slot` and `value_idx` in a result whose `found` is false, which the type
documents as meaningless; and `m_group_mask == 0` is equivalent, since the smallest array is four
groups and the disjunct never fires either way.

**What this does not claim.** The scored benchmark's string workloads are a third of it, so the
score moves by about 1%; that is not the argument. The argument is the instruction counts, which
neither code layout nor drift can move, and the rule that a paired A/B cannot measure a change that
moves an inlining boundary. Against F14VectorMap, the one lookup this map lost, the string miss goes
from 140.6 instructions to 128.8 against its 131.4 -- ahead on instructions, still 2.3% behind on
time.

**The insert path's instructions, counted one by one: the clang/gcc gap is the call boundary, and
PGO removes all of it** (2026-09-10, issue #234 step 1, asked as "with which issue would you
start"). This file has said since 2026-09-08 that "a reserved `uint64_t` insert is 97.8
instructions under clang and 65.9 under gcc, on identical source", and explained the difference as
clang spilling the probe's loop state at function entry where gcc sinks it into a branch a miss
never takes -- "a register allocator's choice and not something a source change has been found to
steer". Nobody had ever listed the instructions. They are listed now and the explanation was wrong.

**First, the tool.** The `/tmp/ins.cpp` that produced those numbers went with `/tmp`, so the
measurement could not be repeated at all. It is now `insert` and `bump` in
`scripts/ab/maps_one.{cpp,sh}`: a *reserved* build, so no growth and no rehash is in it, and
`++m[k]` on a key already present, both counting `reps` in operations so every column is per
operation, both with the round in a `noinline` function so `objdump` has a symbol.

**Reproduction, n = 50000, per insert, three runs agreeing on instructions to 0.1%:**

| | this map | boost | notes' 2026-09-08 figure |
|---|---|---|---|
| clang 22.1.8 | **96.4** instr, 33.8 cyc, 6.4 ns | 82.9, 21.3, 4.1 | this map 97.8 -- reproduces |
| gcc 16.2.1 | **67.5**, 25.0, 4.7 | 57.0, 16.9, 3.2 | this map 65.9 -- reproduces |

Both of this map's figures reproduce. **Boost's does not**: this file records boost at 64
instructions under clang and it is 82.9. That figure comes from the 2026-09-05 table, taken with
the lost tool and before the merged block; the 2026-09-08 entry that restates this map's numbers
never restated boost's. So the headline "39% behind boost under clang on the insert path" was
comparing two different measurements. It is **16%** on instructions.

**Second, exact instruction counts, by category.** Sampling cannot answer this -- `perf record -e
instructions` skids, and attributed 8.4 prefetches per insert to a path that issues 2. What can is
`valgrind --tool=callgrind --dump-instr=yes`, which counts every instruction exactly. It agrees
with `perf stat` to 0.5% once the once-per-process key generation is subtracted. Per insert, in
each binary's own code:

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

Read the first row. Clang pays **fifteen instructions of prologue and epilogue per insert** -- six
pushes, six pops, a frame adjust either side, a `ret` -- plus the call. gcc pays one. And **clang
pays the same fifteen on boost**, which is what settles what this is: not a register allocator
mishandling this map's probe, but *the function-call boundary*, paid because clang leaves the
operation out of line and gcc inlines it into the caller. The spill and reload columns, which the
old diagnosis named, go the other way: gcc spills more than clang does, 11.53 against 4.20.

**Third, PGO, and it is decisive.** Hot/cold outlining and inlining are profile decisions, so the
question was whether clang can do it when told. Instrument, run, rebuild:

| | instructions | cycles |
|---|---|---|
| clang, this map, plain | 96.4 | 34.0 |
| clang, this map, **PGO** | **69.3** | **17.5** |
| clang, boost, plain | 82.9 | 23.7 |
| clang, boost, **PGO** | **57.8** | **14.2** |
| gcc, this map, plain | 67.6 | 31.2 |
| gcc, this map, PGO | 69.5 | 25.8 |

**PGO removes 27 instructions from clang and halves its cycles, on both maps.** After it, clang and
gcc retire the same work on this map (69.3 against 67.6) and clang's code is much the faster of the
two. gcc gains nothing on instructions because it had already inlined. So the clang/gcc instruction
gap is a missing profile and nothing else, and no source shuffle was ever going to close it -- which
is why the six that were tried did not.

**What is left, once the boundary is out of the way, is about eleven instructions.** This map
against boost, PGO to PGO under clang, 69.3 against 57.8; under gcc 67.6 against 57.0. By category
under gcc the difference is +4.2 compare/branch, +3.8 frame accesses, +2.2 move, +2.0 prefetch (the
probe's two `prefetch_index`, which boost does not issue on an insert), +1.7 on the vector compare
and -3.3 arithmetic. That is the real design difference and it is roughly the second walk: this map
probes to find the key absent and then walks again in `place_group`, where boost's probe returns the
position it will insert at. Eleven instructions, not thirty-four.

**And it is an in-cache statement only.** The same measurement at four million entries:

| n | clang, this map | clang, boost | gcc, this map | gcc, boost |
|---|---|---|---|---|
| 50000 | 6.4 ns | 4.1 | 4.7 | 3.2 |
| 200000 | 6.9 | 5.6 | 6.3 | 3.9 |
| **4000000** | **35.6** | **34.6** | **28.7** | **28.3** |

At four million they are level -- within 3% on both compilers -- and this map takes **0.80 dTLB
misses per insert against boost's 1.37** and 1.07 branch misses against 1.26. Whatever the insert
path costs, it stops mattering exactly where the table stops fitting in cache.

**The other two workloads, for the record.** `++m[k]` on a key already present, n = 50000: this map
84.7 instructions under clang and 90.9 under gcc, boost 80.9 and 57.1. Here *both* compilers leave
this map's `do_try_emplace` out of line (15.4 instructions of prologue in both) while gcc inlines
boost's, which is 15 of the 34 that separate them under gcc. That path is also where
`always_inline` on `do_place_element` is paid without being used, and the callgrind table says what
it costs: the placement code makes `do_try_emplace` too big for the caller, so every `operator[]`
buys a call boundary. That is the trade the entry below on `always_inline` describes from the other
side, now with a number on it. A **string** insert reverses the whole picture: 491 instructions
under clang against boost's 454, and **27.3 ns against 34.9** -- 21% *faster* on 8% more
instructions, on 7.2 L1 misses against 11.2 and 1.45 branch misses against 2.16.

**So the standing sentence in `CLAUDE.md`, "clang splits the insert path and gcc does not, which is
most of the build difference between them; no source change has been found that steers it", is
right about the mechanism and wrong about the remedy.** A profile steers it completely. Nothing
here recommends shipping a PGO build -- a header library cannot -- but it does say where the next
attempt should not go: not at the probe's register allocation, which was never the problem.

**And the one candidate the list suggested is dead too, which is the useful half of it.** The table
above says `always_inline` on `do_place_element` makes `do_try_emplace` too big for its caller, so
every writing lookup buys a call boundary; the obvious answer is to move the attribute. Five
variants, one map per binary, instructions per operation (per element for `build`), both compilers:

| | clang insert | clang bump | clang build | gcc insert | gcc bump | gcc build |
|---|---|---|---|---|---|---|
| **A, shipped** (placement `always_inline`) | **96.4** | 84.7 | **140.4** | **67.6** | 90.9 | **123.3** |
| B, placement plain | 124.4 | 83.7 | 168.5 | 65.5 | 90.9 | 125.2 |
| C, placement `noinline` | 124.4 | 83.7 | 168.5 | 144.4 | **72.8** | 198.0 |
| D, placement **and** `do_try_emplace` `always_inline` | **96.4** | 84.7 | **140.4** | **67.6** | 90.9 | **123.3** |
| E, C plus `do_try_emplace` `always_inline` | 124.4 | 83.8 | 168.5 | 144.4 | **72.8** | 198.0 |

**D is identical to A in every column** -- not close, identical -- and the binaries differ. In A the
out-of-line symbol is `do_try_emplace`; in D that symbol is gone and **`try_emplace` is out of line
instead**. Forcing the inner function into its caller moves the boundary outward by one level and
changes nothing at all, because the outermost function that does not carry the attribute is the one
that pays, and there is always one. You cannot inline your way to the caller's loop from inside a
header. That is why PGO was the only thing that removed it: it inlines the whole chain into the loop
that calls it.

**C is the trade named out loud and it is a bad one**: 20% off a gcc `++m[k]` (90.9 to 72.8) for
**61% onto a gcc build** (123.3 to 198.0) and 20% onto a clang one. B reproduces this file's own
17-20% build regression exactly, now as an instruction count rather than a time: clang build 140.4
to 168.5. So `always_inline` on `do_place_element` stays, for the third time, and the call boundary
on `operator[]` is its price, which nothing in the source can refuse.

**The F14Vector string miss, re-measured, and the old explanation of it is wrong** (2026-09-10,
asked as "is my map the fastest dense map"). The paired octave still has F14VectorMap ahead on
string lookups -- miss 1.06 at the 1000 octave and 1.09 at 32000, hit 1.02-1.03, half 1.04-1.06 --
and per size across the 32000 octave the miss ratio runs 1.072, 0.990, 1.125, 1.145, 1.118. One map
per binary, 30M lookups, three runs agreeing to 0.5%, per lookup at 32000:

| | ns | instructions | cycles | branch misses | L1 misses |
|---|---|---|---|---|---|
| str miss, this map | 18.99 | 138.4 | 100.0 | **1.046** | 5.19 |
| str miss, F14Vector | **17.20** | **130.5** | **90.7** | 1.13 | **3.91** |
| str hit, this map | 24.9 | **166.2** | 131.7 | 1.068 | 8.36 |
| str hit, F14Vector | 25.0 | 170.8 | 132.4 | 1.079 | **7.72** |
| u64 miss, this map | **3.24** | **55.8** | **16.8** | **0.038** | 3.26 |
| u64 miss, F14Vector | 3.67 | 61.2 | 19.0 | 0.103 | **1.99** |

So the string **hit** is a tie on time with fewer instructions here, the integer miss is **27% ours**,
and the one loss is the string miss: **+8 instructions and +1.3 L1 fills**, while winning on branch
misses. The L1 column is a constant of the design -- we take ~1.3 more fills per lookup on every
workload, and win the other two anyway.

**The entry above that blames clang for leaving `do_find_hashed` out of line no longer applies.**
`nm` on today's binaries: the only out-of-line probe symbols in the sixteen-map binary belong to the
**4.11.0 baseline copy** (`udmbase::...::v4_11_0`), and the current map's probe is inlined in both
the one-map and the sixteen-map binary. Whatever that diagnosis was true of, it is not true of this
header, and the +8 instructions have to be explained some other way.

**Where they are, from `perf annotate`.** `do_find` for a `std::string` key opens with six
callee-saved pushes, a 72 byte frame and six spills -- the counter, `this`, the group mask, the
delta, the hash, and `movdqa %xmm0, 0x30(%rsp)`, the broadcast fingerprint itself -- **all before the
first group is compared**. The broadcast is spilled because `m_equal` is a call and xmm registers
are caller-saved, so a vector spill is paid on every lookup to survive a `memcmp` that a miss
reaches about 2.5% of the time.

**Splitting the probe so the miss path stops paying for it: measured, and it costs more than it
saves.** Everything past the home group moved into a `noinline` continuation taking the word, the
counter and the group index, leaving the fast path to compare one group and stop. It does exactly
what it was meant to on the string miss -- **138.3 to 126.5 instructions**, 99.4 to 95.8 cycles,
2.8% faster -- and wrecks the integer paths: `u64 miss` 55.8 to 65.9 instructions (+18%), `u64 hit`
at a million 69.1 to 85.0 and 31.13 to 39.66 ns (+27%). Introducing a call into a lookup that was
fully inlined makes the register allocator treat the caller-saved registers as clobbered, so the
spills reappear around the call site. `[[gnu::cold]]` on the continuation plus marking the branch
likely changed nothing: the fast path still has to be able to make the call. Reverted. It buys 8% of
the one workload we lose and spends 20% of the ones we lead by most.

**And a measurement trap that cost most of a session.** `scripts/ab/maps_one.sh`'s third argument is
the **number of lookups** and its default is 30,000,000. Passing 20, and then 400, measures 20 and
400 lookups: three runs of the *unmodified* header then spanned 417500 to 491000 ns, a **16% spread**,
and a "removing the prefetches is worth 16%" conclusion was built on two samples inside it. At 30M
the same comparison resolves to 0.5% and the true figure is 1.8%. Instruction counts were stable to
0.7% throughout and would have caught it immediately, which is the rule this file already states and
which was not followed.

**Eleven slots and twenty-four slots, and why sixteen is where it stops** (2026-09-10, asked as
"how about doing only 11 lanes", "12 fingerprints, 4 counters, 12 indices" and "how about going
bigger"). Three block sizes were proposed in one sitting and all three are answered. The 12-slot one
is the entry further down, measured 2026-09-07 at 0.9888. The other two were built today.

**Eleven slots: 11 fingerprints, 8 counters, 11 four-byte indices, one pad byte, `alignas(64)`.**
Exactly one cache line with *full* `uint32` indices, so unlike the 3-byte-index prototype it caps
nothing, and the block address becomes `shl $6` instead of the `imulq $0x58` that sits on the
address path of every lookup. Paired octave against the shipped sixteen, above 1.00 meaning eleven
is faster: `rmiss64` **1.037**, `rmissstr` 1.012, `findstr` 1.010, `rhit64` 1.007, `find64` 1.000,
`build64` 0.981, `ie64` **0.963**, `churnbig` 0.946, `churn64` **0.944**; control `hashstr` 0.893.
Misses 1-4% better, churn and insert-erase 4-6% worse, net below 1.00. The `rmiss64` figure
reproduces the 1.039 this file already recorded for an eleven-slot group as an aside about octave
measurement, so that footnote and this are the same design.

**What eleven adds that twelve could not say.** The 12-slot layout changed two things at once -- a
shorter group *and* four counter classes instead of eight -- and lost churn 7%. Eleven keeps all
eight classes and still loses churn 5.6%, which separates them: **the counters were not the problem,
the group length was.** At load 0.8 a group of sixteen holds 12.8 and overflows at 2.0 standard
deviations; a group of eleven holds 8.8 and overflows at 1.65. Shorter groups fill more often, more
entries land away from home, and every insert and erase then walks and updates more counters.

**Twenty-four slots: 24 fingerprints, 8 counters, 24 indices, exactly 128 bytes**, 5.33 bytes per
slot -- slightly *less* than the shipped 5.5. The trade was supposed to invert: a group of 24
overflows at 2.45 standard deviations, so churn should have come back. **It is the worst of the
three and it loses everywhere.** `rmiss64` **0.853**, `churn64` 0.863, `churnbig` 0.870, `find64`
0.919, `ie64` 0.923, `iebig` 0.927, `findbig` 0.933, `it64` 0.935, `rhit64` 0.949, `rmissstr` 0.967,
`findstr` 0.972, `build64` 0.977, `rhitstr` 0.987, `churnstr` 0.992, `buildstr` 1.001, `buildbig`
1.005; control `hashstr` 1.114. Nineteen of nineteen scored workloads at or below 1.005.

**And the reason is the counters, in the direction opposite to the prediction.** The largest single
loss is the *miss*, which is what the counters exist to stop. There are still only eight classes, so
a group of 24 puts **three slots in each class instead of two**: any given class holds more entries,
its counter is nonzero more often, and a miss continues past home more often. Enlarging the group
without enlarging the counter array dilutes the filter. That is the counter-width axis reached from
the other end -- one class scored 0.959, sixteen nibbles filtered better but cost arithmetic -- and
it says the eight counters and the sixteen slots are not two choices but one.

**So sixteen is a local optimum on three axes at once**, and moving either way breaks one of them:

| | 11 slots | 16 slots, shipped | 24 slots |
|---|---|---|---|
| overflow frequency | worse (1.65 sd) | 2.0 sd | better (2.45 sd) |
| slots per counter class | 1.4 | **2** | 3, and the miss pays for it |
| the compare | one SSE2 load | **one SSE2 load** | two |
| paired octave | below 1.00 | -- | 0.85 to 1.00 everywhere |

Both prototypes needed the same two fixes before they were correct, and either is a trap for the
next attempt: the group count must be rounded down to a power of two, because
`max_bucket_count() / slots_per_group` is not one when the slot count is not, and `max_size()` has
to follow it. Without the first, `bucket.cpp`'s one-byte-index `group_micro` corrupts the heap --
the same failure the 12-slot entry records -- and without the second it segfaults instead.

**Why none of them could have won, which is the part worth keeping.** All three shrink or align the
block to remove L1 fills, and **the fills are not on the critical path**. The 12-slot entry said it
first (1.4 fewer L1 misses per string lookup, cycles unchanged to the tenth) and today's prefetch
experiment says it independently: removing both index prefetches removes **1.08 L1 fills per string
miss** -- taking us from 5.19 to 4.11, against F14Vector's 3.91 -- and buys **1.8% of time**. A
prefetch is asynchronous by construction, so the counter moves and the clock does not. Any future
idea justified by "it touches fewer cache lines" has to answer that first.

**A growth factor below 2, and the premise that suggested it was wrong** (2026-09-08, asked as
"should we benchmark folly's 1.406"). **Folly doubles.** Printing `bucket_count()` after every insert
for five maps: F14Value goes 24, 48, 96, 192, 384; F14Vector 20, 40, 80, 160; boost, abseil and this
map likewise exactly 2x from the third step on. The `minGrowth` of `origCapacity * 1.406` in
`reserveForInsertImpl` only binds on an explicit `reserve(n)`, never on growth by insertion, so there
is no shipped sub-2x design in the field to copy -- the recommendation to test it came out of reading
one line of folly rather than running it.

The question survives for **the value vector**, which is where this map's memory actually goes and is
a `std::vector` doubling on its own cadence. The index cannot change: a power-of-two group count is
what `hash >> m_shifts`, the mask and the triangular probe's reaches-every-group property all rest
on. The vector is a template parameter, so a different factor needs no header change --
`grow_vec<T, A, NUM, DEN>` deriving from `std::vector` and reserving `capacity * NUM / DEN` in
`emplace_back`, passed as `AllocatorOrContainer`. Octave geomean, twelve points, heap counted by a
replaced global `operator new` (`/tmp/gf.cpp`):

| factor | build ns/el | steady B/el | peak B/el | 64 B value: build / steady | string: build / steady |
|---|---|---|---|---|---|
| 2.00, shipped | 9.94 | 33.16 | 43.55 | 13.18 / 113.34 | 24.43 / 116.09 |
| 1.50 | 11.34 (+14%) | **29.80 (-10%)** | 40.35 (-7%) | 15.63 (+19%) / **98.88 (-13%)** | 26.72 (+9%) / **107.71 (-7%)** |
| 1.25 | 11.37 (+14%) | **27.95 (-16%)** | 40.63 (-7%) | 19.19 (+46%) / 90.55 (-20%) | -- |
| 1.125 | 13.95 (+40%) | 27.09 (-18%) | 41.19 (-5%) | -- | -- |

Reproduced on a second octave from 200000 to within 1% on every memory figure. So **1.5x buys 7-13%
of steady memory for 9-19% of a build**, and the peak barely moves, because at the growth peak the
index doubling is alive too.

**Not worth changing the default, and worth documenting as a knob.** Building is this map's strongest
column -- 1.66x ahead of boost and 1.61x of abseil at an 8 byte value -- and memory its weakest, at
32.6 bytes per entry against boost's 29.2 and abseil's 27.0. Spending 14% of the former to gain 10%
of the latter lands exactly level with boost on memory while giving up the lead that pays for the
dense layout in the first place. A caller who is memory-bound rather than build-bound can have it
today with six lines and no fork, which is the right place for a trade that depends on which of the
two is scarce.

**The pipelined rehash does not transfer to `boost::unordered_flat_map`, and no other map has one**
(2026-09-08, asked as "does any other map use a pipelined rehash" and "would it transfer to boost").
Read every rehash loop in the field: folly's `prefetchBeforeRehash` prefetches the *source* values
of the chunk about to be hashed and places synchronously; abseil's `GrowToNextCapacity` is a
different idea, two passes with the elements that would probe encoded as `(h2, source_offset, h1)`
into a stack buffer and placed second, so nothing is hashed twice and most elements never probe;
boost, indivi, emhash8, emilib, Verstable and ihtab hash and place one element at a time with no
prefetch at all. So the hash-sixteen-ahead loop is this map's alone.

Ported to boost in a copy of `core.hpp` (`unchecked_rehash` with a sixteen-entry ring of element
pointer and hash, prefetching the destination group and, in the second variant, all four cache
lines of the group's fifteen slots), isolated `rehash()` to double the bucket count and back, ns
per element, `/tmp/brh.cpp` against `/tmp/boostpf`:

| | u64 200K | u64 1M | str 200K | str 1M |
|---|---|---|---|---|
| clang, boost as shipped | 6.5 | 10.6 | 19.0-19.5 | 77.4 |
| clang, pipelined, four slot lines | 7.0-7.2 | 11.1-11.3 | 17.9-18.7 | 80-82 |
| gcc, as shipped | **10.2-10.5** | 10.4-11.5 | 17.1-17.8 | 71-73 |
| gcc, pipelined, four slot lines | **7.0** | 10.2-10.5 | 16.7-17.2 | 73-77 |

A wash to a loss under clang, and under gcc a 1.5x gain at 200K integers that is a codegen fix
rather than a memory one: gcc's straight loop is 1.5x slower than clang's on the same source and
the restructured loop takes it to clang's floor, which is the compiler serialising something the
other compiler does not, the same shape as this map's own `fill_buckets_from_values` story with the
compilers swapped. Prefetching only the first slot line (the first attempt) was a loss everywhere,
because boost's fifteen 16-byte slots span four lines and fill from lane 0, so late placements land
on lines never asked for.

**Why, measured rather than argued** (`/tmp/rhperf.cpp`, nothing but rehashes so `perf stat` counts
the loop): per element rehashed at a million `uint64_t` entries, **boost 97.5 instructions and 110.9
cycles against this map's 51.9 and 46.7**, on the same L1 load misses (3.81 against 3.73) and with
dTLB load misses *lower* for boost (0.051 against 0.628). So it is not the TLB and not a load a
prefetch could have hidden -- the first draft of this entry asserted both and had measured neither.
A flat map's rehash moves the `value_type` into a hash-scattered slot, which is where the extra
instructions go, and the random writes that follow are write-allocate misses that an
`__builtin_prefetch` of the destination does not cover. A lookahead hides a load's latency behind a
hash chain, and this loop is not waiting on a load.

**What that says about boost's build, which is its weakest column** (`/tmp/bsplit2.cpp`, build from
empty against a reserved build, ns per element): boost's *insert path is faster than this map's* --
3.53 against 7.48 at 200K u64, 5.97 against 8.62 at 1M -- and its growth costs 7.7 and 17.9 ns per
element against 0.94 and 3.43, so **68-75% of a boost build is growth against 11-28% here**. abseil,
whose two-pass encoder is the cleverest rehash in the field, measures 7.11 and 20.60: 8% better than
boost at 200K and *worse* at 1M. So the encoder is not the fix either, and the cost is the family's
-- a flat map moves every value on every doubling, a dense one moves four byte indices and leaves
the values in place. The same split with a 64 byte value: boost 18.2 and 59.9 ns of growth per
element against this map's 4.3 and 23.6.

**And the radix partition does not rescue it either, which was the one idea left** (2026-09-08,
asked as "give 2 a try and measure it"). Since growth is 68-75% of a boost build against 11-28% of
one here, the partition that measured end-to-end neutral for this map has five times as much to gain
there. Ported into `unchecked_rehash` -- hash every element and histogram the partition of the new
group array it lands in, scatter `(element*, hash)` into partition order, then place -- at 16, 64 and
256 partitions it is **1.7-2x slower at every size and on both compilers**, because the scratch is 32
bytes per element faulted fresh on every growth. That is the same failure this map's own version had,
and it is worth separating from the idea: keeping the scratch in a `static thread_local` across
rehashes turns the isolated 4M integer rehash into a **19% win** (21.3 to 17.2 ns per element, three
rounds, both compilers), and

- the win is a bump, not a trend: at 8M the same code is **25% slower** (21.2 to 26.5), the scratch
  having grown to 256 MB and its own scatter pass become the cost;
- and it does not survive end to end. A build from empty at 4M is **43.0 to 47.7 ns per element under
  clang and 42.1 to 44.8 under gcc** -- slower, because a build doubles twenty-odd times and the
  scratch grows with it, so the pages the isolated harness faults once are faulted again at every
  doubling;
- strings lose throughout (80.8 to 85.1 at 4M), the scatter of a 40 byte `value_type` costing more
  than the locality buys.

So the answer to "what could help boost's build" is: not a better rehash loop. Its insert path is
already twice this map's; what it pays is moving every value on every doubling, which is what the
flat layout *is*. The levers left are a growth factor below 2x (folly's 1.406, untested here) and not
being flat.

**Seven ideas from reading folly F14 and from the F14Vector string gap, all measured on 2026-09-07,
none kept** (asked as "where does our map differ from indivi", "how can F14Vector beat us at string",
"read folly for tricks", "try the 12 fingerprint layout", "find more ideas"). The method throughout
is the one the file already prescribes for anything under 10%: one map per binary, `perf stat`, and
instruction counts as the number that cannot be argued with. `scripts/ab/maps_one.sh -k str` is the
harness; the minimal single-map binaries were `/tmp/hb.cpp` (lookups), `/tmp/rh.cpp` (`rehash(0)`
on a built map) and `/tmp/ins.cpp` (reserved inserts).

**Where the F14Vector string gap actually is.** Paired octave geomeans had F14VectorMap 6-12% ahead
on string lookups and us 1.27x ahead overall on integers. At 32000 entries, one map per binary, the
hit is a 2% tie (137.2 against 134.4 cycles) and the **miss is 9.5%** (103.8 against 94.8), while the
same binaries on `uint64_t` keys have us 17% ahead on the miss. The hash is provably not it: the
harness hands F14 our wyhash, and `perf record` puts the identical `wyhash::hash` symbol at 44.2%
and 44.6% of the two binaries. Nor is it the load factor (both have 4096 groups holding 7.8
entries each at that size -- F14's `bucket_count()` of 40960 is `chunkCount * capacityScale`, not
slots). Nor the value indirection, which F14Vector has too. It is **clang leaving `do_find_hashed`
out of line for `std::string` keys** and inlining it for `uint64_t` (`nm` on the two binaries), plus
our two index prefetches, which on a miss with no fingerprint match are pure waste (1.1 of the 1.3
extra L1 fills per miss). Force-inlining it: 103.8 to 99.1 cycles at an *identical* instruction
count, half the gap; the prefetch removal adds nothing on top of that. The remaining 4.5% is eight
instructions of ordinary difference between two probe loops.

**Force-inlining `do_find_hashed`, paired on the score: not kept.** clang 0.9975 with `rmissstr`
1.022 and `rmiss64` 1.016 in a run whose `hashstr` control read 1.106; **gcc 0.9946 with `rmiss64`
0.844 and `rhit64` 0.948 against a clean control of 1.001**. gcc was inlining it already, so the
attribute can only have moved the inlining of what surrounds it in that translation unit, and it
moved it the wrong way by more than the clang gain. A `#if defined(__clang__)` would take the 4.5%
on string misses; not done, because a compiler-conditional inlining attribute on the lookup is the
kind of thing the next compiler release silently reverses.

**Splitting the hash into an inlinable short path and an out-of-line tail** (`hash()` for
`len <= 16`, `hash_long()` `noinline` for the rest, `secret` hoisted, values identical by checksum
over lengths 0-1200): **a loss under clang in every configuration, including all-short keys**, where
it removes the call outright -- 43.9 to 45.6 cycles and +6 instructions on an 8-16 byte miss, 103.4
to 108.8 and +18.6 instructions on the scored lengths; gcc −4% on all-short misses and 0 to +2%
otherwise. The 24 instructions of the short path inlined into a caller that has the probe's state
live cost spills, which is the `do_place_element` mechanism again. The earlier "force-inlining the
hash: nothing or slightly worse" line above was a paired run that could not resolve it; this can,
and the sign is the same. And since both binaries in the F14 comparison call the identical
out-of-line hash, nothing done to the hash could have moved that ratio anyway.

**folly's `fullness[]` byte array in the rehash** (`allocateTag`: one occupancy byte per chunk on
the stack, so placement never loads the destination chunk's tags): isolated `rehash(0)`, ns per
element, u64/str at 200K, 1M, 4M: clang 1.66/3.92, 1.73/4.19, 6.60/8.97 as shipped against
1.72/4.06, **1.89/4.91**, 6.54/9.48 with it; gcc 1.85/4.48, 1.81/4.63, 6.46/8.93 against 1.92/4.36,
**2.21/5.40**, 6.71/**10.37**. A 3-20% loss on the loop. It trades a random load that the
sixteen-ahead prefetch already hides for a random load nothing prefetches, plus a store the next
element to the same group has to forward from. F14 needs it because its rehash is not pipelined.

**The 12-slot, 64 byte, cache-line-aligned block** (F14VectorMap's `kCapacity = 12` for 4 byte
items: 12 fingerprints + 4 counters + 12 x 4 byte indices, `alignas(64)`, counter class `& 3`, the
top four lanes of the compare masked off, no index prefetch). It does exactly what the L1 counters
predicted and nothing else: 1.4 fewer L1 misses per string lookup (5.27 to 3.89 on a miss, 8.35 to
6.95 on a hit) and **cycles unchanged to the tenth** (103.3 to 102.5, 135.1 to 135.7) -- the lines it
saves were being prefetched, so they were never on the critical path. Paired on the score **0.9888**:
lookups +1-2% (`find64` 1.024, `rhit64` 1.014, `findstr` 1.011), churn and insert-erase −5-7%
(`churn64` 0.925, `churnbig` 0.936, `ie64` 0.955), because a 12-slot group is full more often at the
same load and four classes filter a churned miss worse than eight. Also found: 12 slots make the group
count non-power-of-two for a one byte value index (256 / 12 = 21 groups), which is heap corruption
via `hash >> shifts` and an endless `calc_shifts_for_size`; `bucket.cpp`'s `group_micro` cases are
what caught it. Not fixed, since the layout is rejected.

**Prefetching the back element's string body before an erase's probe**, so the hash of the moved
key overlaps the probe instead of following it (a `prefetch_key` hook, a no-op except for
`std::basic_string`): string churn 436.7 to 429.1 cycles and insert-erase 680 to 674 at 32000,
nothing at 200000 (1565 to 1577, 2502 to 2526). 1-2% in cache, nothing out of it -- the same shape as
the erase-side pull-back. Not kept.

**Fusing the insert's probe with its placement** (`probe_for_insert` also returns the home group's
empty-lane mask, so a miss whose home has room places without the second walk, and no counter can
have moved because no full group was passed; anything else falls back to `place_group`): **more
instructions, not fewer** -- reserved u64 inserts clang 97.8 to 101.0, gcc 65.9 to 70.0, gcc cycles
16.4 to 17.7. The `match_empty` on home is paid on every insert probe and the two extra live values
cost spills, while the second walk it removes was an L1 hit on a line just loaded.

**The one fact left standing is the clang/gcc gap itself**, and it is not inlining: a reserved
`uint64_t` insert is **97.8 instructions under clang and 65.9 under gcc**, a string miss 142 against
110, an 8-16 byte string miss 115 against 79, on identical source, and force-inlining
`do_try_emplace` or `do_find_hashed` leaves clang's count exactly where it was (97.8, 143.2). The
disassembly says what it is: clang spills the loop state at function entry (six pushes, the
broadcast, counter, mask, delta, `this`, the groups pointer) where gcc sinks the same spills into
the fingerprint-match branch that a miss never takes. That is a register allocator's choice and not
something a source change has been found to steer; every attempt above that moved code into a
caller made it worse.

**What `move_home` is actually worth, re-measured** (2026-09-07, `scripts/ab/move_home.{cpp,sh}`,
asked as "I am now sceptical this is of any use" -- reasonably, since the drift entry it was
justified by does not reproduce). One map per binary, the same header with `move_home` turned into a
no-op beside it, a reserved table at load 0.80 churned through with `hits` writing lookups per
round, then the region under test timed on its own. **The control is the part that makes it
believable**: with `hits` at 0 `move_home` never fires, so the two binaries have to measure the
same, and where they do not that is code layout to be subtracted.

| entries | control (no writing hits) | with one writing hit per round | hits | churn round |
|---|---|---|---|---|
| 52363 (in L2) | 0.951 | **1.107** | 1.041 | 1.012 |
| 838860 (L3) | 0.998 | **1.101** | 1.003 | -- |
| 3355443 (past L3) | 0.997 | **1.099** | 0.991 | -- |

Ratios are off over `move_home`, so above 1.00 means `move_home` is faster. So it is worth **about a
tenth of a miss**, it is worth **nothing on a hit**, it costs nothing on the writing path that pays
for it (the churn round reads 1.00-1.01), and -- correcting the entry below -- **it is worth the same
tenth out of cache as in it**, at 3.4M entries where the index is 23 MB, not "nothing out of cache".

The mechanism, per lookup at 52363 entries, `perf stat` on the same two binaries: with no writing
hits **27.59 cycles and 0.2118 branch misses against 27.52 and 0.2116** -- identical, as the control
demands -- and with one writing hit per round **25.58 and 0.1591 against 28.93 and 0.2177**.
Instructions barely move (53.9 against 54.9). So a quarter of the branch misses go and an eighth of
the cycles, on 0.04 fewer groups per miss: **most of what the drift costs is the stop-or-continue
branch becoming unpredictable, not the extra group visit**, which is why the time effect (11%) is
three times the probe-length effect (3.7%) and why it survives out of cache where the extra group is
in the adjacent block.

**What this does not say.** `move_home` fires only on a hit inside a writing path, so a workload
that only reads gets exactly nothing from it -- the control row *is* that workload, and it reads
1.00. And the gain is entirely on misses. The profile it pays for is a map that churns at a fixed
size, is written to by key, and is asked about keys that are not there; that is a real shape (a
dedup set, a cache with negative lookups, any `++m[k]` counter over a sliding window) and it is not
the shape of any workload in the scored suite, which is why the score reads 1.000 on it and always
will.

**Three ideas the eighteen-map comparison suggested, all measured, none kept** (2026-09-07, from
asking what the post's own findings imply for this map).

**Double hashing instead of the triangular sequence, so siblings take different tours.** Folly's
`probeDelta = 2*tag+1` with the comment that quadratic and linear "result in longer probe lengths",
aimed at the finding above that ~80% of what a counter fails to filter is siblings -- keys homed in
the same group, which under a triangular sequence walk the *same* groups a later miss walks. Step
taken from bits 8-15 of the hash, which the group (top bits) and the fingerprint (low byte) do not
use; odd, so it still reaches every group of a power-of-two array exactly once, and `uncount`,
`place_group`, `slot_of_value` and the rehash's own copy of the placement walk all take it too.
(Missing the rehash's copy is what six failing tests found first; the seventh failure is
`erase_uncounts`, which asserts a comparison count that encodes the triangular shape.)

**The mechanism works and the time is worse.** Instrumented, 4096 groups, 200 turnovers, groups per
lookup, triangular against double hashed: fresh miss 1.0522 -> **1.0349**, churned miss 1.0609 ->
1.0497 at load 0.76; at load 0.799 fresh miss 1.0856 -> **1.0542** and churned 1.1223 -> 1.0959. So
it removes a third of the excess on a fresh miss, exactly as intended. Paired on the score:
`rmiss64` **0.915**, `build64` 0.950, `churnbig` 0.970, `rhit64` 0.984, control `hashstr` 1.04. One
map per binary at 50000 entries: +4.6 instructions per lookup (57.2 -> 61.8 on a miss, 60.5 -> 65.4
on a hit), cycles 20.6 -> 21.3 and 29.5 -> 31.1, branch misses slightly *better* (0.108 -> 0.093),
L1 misses unchanged. Three cheap ops and a live register in three loops, against 0.03 groups on a
path 5% of misses reach. The general shape again: **the group compare and the counter have already
taken the probe down to 1.03-1.09 groups, so the shape of the sequence past home has nothing left
to win.**

**An ungrouped, unaligned window, which is what `indivi::flat_wmap` does -- rejected on a simulation
rather than built.** It is the fastest hit of the eighteen maps and 1.15-1.31x faster than its own
grouped sibling, so it is worth knowing what the window itself is worth. Simulated with the same
keys at the same load, windows visited per placement, bucketized (home is a group of sixteen)
against sliding (home is a slot, first free slot within sixteen): at load 0.799 **1.0481 against
1.0396**, at 0.76 1.0318 against 1.0238. So slot-level placement removes about a *fifth* of an
excess that is already under 5% -- a quarter of what `move_home` is worth, and `move_home` is worth
11% of an in-cache miss and nothing out of cache. To collect it this map would have to give up the
merged block, since sixteen fingerprints starting at an arbitrary slot are not contiguous in an
88 byte block, and that is measured at 2% of the score and 28% of the dTLB misses at 4M. Ceiling
below cost; not built.

**And the reason `flat_wmap` is actually fast is not the window.** One map per binary, hits, its
grouped sibling against it: **53.3 against 47.3 instructions** at 1000 entries, 54.6 against 48.3 at
50000, 72.6 against 64.4 at 1M, with fewer L1 misses at every size *including* the one that fits in
L1 (0.876 against 0.378). Six fewer instructions and half the metadata per slot (one byte against
two), not the alignment. This map is at 60.8 instructions and 4.2 L1 misses per hit at 50000 against
its 48.3 and 3.3, and closing *that* is a different and still-open question.

**A per-table seed, abseil's defence against keys chosen for a known hash.** `mixed_hash` returns
`hash ^ m_seed`, the seed scrambled from the table's own address so that two live tables differ and
ASLR makes two processes differ. **On lookups it is free**: one map per binary at 50000 entries,
+1.0 instruction and **0.0 cycles** on both a hit and a miss (21.4 against 21.4, 29.6 against 29.6),
ns/op identical to two decimals. On a build it is **3.5%** (7.13 -> 7.38 ns per element, +1.7
cycles), because the pipelined rehash is latency-bound and the xor sits between the hash and the
group address.

Two things make it a feature rather than a patch. The seed has to travel with the index it built
through **six sites** -- the allocator-aware copy and move constructors, `copy_everything_from`,
`move_everything_from`'s two branches and `swap` -- and the suite caught every one of them (85
failures, then 70, then 11). The 11 that remain are `avalanching.cpp` asserting that `mixed_hash`
returns an avalanching hash *unchanged*, which the seed contradicts by design, plus one steered
`erase_uncounts` case; they test a value where they would have to test the property. And iteration
order stops being reproducible between runs. So: worth having behind a macro, not worth making the
default, since the price is paid by everyone and the threat is not everyone's.

**The paired harness read this one wrong, which is the rule working.** With the two headers in one
binary the seed measured `build64` 0.936, `rmiss64` 0.940, `rhit64` 0.968 -- and one map per binary
says 0.0 cycles on both lookup paths. The control `hashstr`, which never touches a map, read 1.027
in the same run. A paired two-header run decides a 10% question and not a 3% one.

**Also: the churned drift figures recorded below do not reproduce.** The `move_home` entry has, at
load 0.76 after 200 turnovers, 1.143 groups per hit and 1.265 per miss against a fresh 1.032 and
1.052. An instrumented header that reproduces the **fresh** pair to three digits (1.0311 and 1.0522)
measures the churned pair at **1.0358 and 1.0609**, and it saturates -- 5, 20, 100 and 400 turnovers
give 1.039, 1.036, 1.035 and 1.035 per hit. It is load-sensitive as expected (at 0.799, 1.0660 and
1.1223) but never approaches 1.14/1.27 at 0.76. Either that harness churned differently in a way
that matters or the figure is wrong; the churn here erases a uniformly random live key and inserts
one the map has never held, at a constant size, on a reserved table. `move_home` itself is not in
doubt -- it was kept on a one-map-per-binary timing (misses 5.16 to 4.64 ns) rather than on the
drift figure -- but **the drift it takes back is smaller than recorded**, which also means the
headline "a churned table probes 1.14 groups per hit against a fresh 1.03" that `CLAUDE.md` used to
carry should be re-derived before it is quoted again. (Done on 2026-09-10 with
`scripts/ab/probe_length.sh`, which reproduces the figures in this entry; `CLAUDE.md` and the
README now quote those instead.)

**Every other map on the same workloads, in one harness** (2026-09-07, `scripts/ab/maps.{h,cpp,sh}`,
`maps_one.{cpp,sh}`, `mapsplot.py`, `diagrams.py`; written for the blog post on index structures).
Eighteen maps for an integer key and sixteen for a string, interleaved by `compare()` in one
process: this header, this header at `v4.11.0` renamed the way `run.sh` does, boost flat and node,
abseil flat and node (each also with its own hash as a control), F14 Value/Vector/Node, emhash8,
emilib, indivi `flat_umap` and `flat_wmap`, Verstable, ihtab and `std::unordered_map`. Three key
shapes -- `uint64_t`, `std::string`, and `uint64_t` with a 64 byte value -- three octaves each, five
sizes per octave, every adapter checked against this map over 400000 mixed operations first and
again under ASan/UBSan. **Two independent runs agree: 372 of 378 integer ratios within 5%, worst
1.12 on `iterate` at a thousand entries.**

**The sliding window, built and measured rather than simulated** (2026-09-08,
`scripts/ab/window.{cpp,sh}`, asked as "I also want to try to completely switch the group index to
this layout"). The entry below rejected `indivi::flat_wmap`'s ungrouped window on a *placement*
simulation -- windows visited per placement, 1.0481 bucketized against 1.0396 sliding at load 0.799,
a fifth of an excess already under 5%. That simulation could not see a time, and the time is bigger
than it implied. Two index layouts over one implementation sharing the value vector, the hash, the
fingerprint encoding, the load factor, tombstones, growth, the dense erase and the SSE2 helpers, and
differing in the home unit and the probe step and nothing else; one variant per binary; both
cross-checked against `std::unordered_map` over a mixed stream first.

**The window wins the lookup, and it wins it at the branch predictor.** Per operation at 200000
entries, grouped against window: a hit 71.9 instructions, 56.8 cycles, 0.198 branch misses and 4.615
L1 misses against **70.5, 53.6, 0.167 and 4.852**; a miss 70.1, 47.9, 0.518 and 2.810 against
**67.4, 44.0, 0.436 and 3.018**. So 16% fewer branch misses on both, three to four fewer cycles, one
to three fewer instructions -- and *more* L1 misses, because an unaligned sixteen byte load straddles
two cache lines where an aligned one does not. Timed over three sizes and three runs: hits 1-5%
faster, misses 0.5% slower at 32000, **13% faster at 200000** and 4% at a million, builds and memory
a wash.

**The miss result is against the wrong baseline, and the probe lengths say so.** Both variants stop
a miss on an *empty slot*, because a per-class counter has no group to hang on in the ungrouped one
-- so the grouped variant here is not the shipped index, it is the shipped index with its counters
removed. Windows visited per miss, measured over the timed region only: at 200000 entries (load
0.763) **1.2962 grouped against 1.2246 window**, and at a million (load 0.477) 1.0060 against
1.0027. The shipped index, which stops on a counter at home, visits **1.046** on a fresh miss at any
load. So the 13% is the window leaving the first window slightly less often *when the miss test is
an empty slot*, and it evaporates where a miss stops at home anyway: at a million entries the window
is 2% slower on a miss, not faster. Against the real counter-based miss there is close to nothing
here. The hit advantage is smaller and more robust -- 1 to 5%, and still 7% at a million where both
variants visit 1.000 windows, so that part is addressing and instructions (the grouped home costs a
multiply by sixteen) rather than probe length.

**And it loses churn for a reason worth having, which is that it recycles tombstones half as well.**
At a million entries the window variant ends a churn run with **4194304 slots against 2097152** --
one extra doubling -- and 24% slower churn. Counting where placements land says why: 33.8% of the
grouped variant's placements reuse a tombstone against **15.8%** of the window's, and at 200000 it is
71.2% against 69.3%. The mechanism is that `ctz` takes the lowest available lane, which for a window
is the home slot itself and for a group is the group's lane 0 -- a fixed position that all sixteen
homes in that group probe first, so it is tombstoned and reused constantly, where a window's first
lane is different for every home and is more often a slot that has never been used. Burning fresh
slots is what drives a load factor that counts live plus tombstones, so it buys an extra growth.

**So the answer to "switch the group index to this layout" is no, and the chain is what makes it
no.** The window means no per-group counters (there is no group to hang them on), which means the
miss stops on an empty slot, which means tombstones -- and this map is tombstone-free, which is the
property `churn` exists to protect. It also means giving up the merged 88 byte block, worth 7% of a
lookup's instructions and 28% of its dTLB misses at 4M, since sixteen fingerprints starting at an
arbitrary slot are not contiguous in it. Paying all of that for 13% of a miss at one size, and
taking a worse churn with it, is the wrong trade. What is worth keeping from the experiment is the
mechanism: **the win is branch misses, not cache lines**, and an aligned group's lane 0 being
contended by all sixteen of its homes is a real effect that no probe-length simulation shows.

**The lane-contention mechanism, confirmed by transplant, and it runs the other way from the guess**
(2026-09-08, `scripts/ab/window.cpp` with `-DLANE_ROTATE`). The window recycles tombstones at half
the grouped rate, and the reason offered was that `ctz` takes the lowest free lane -- which for a
window is the home slot, different for every key, and for a group is lane 0, shared by all sixteen
of that group's homes. The test is to transplant *only* that property: give the grouped variant a
per-key starting lane from bits 8-11 of the hash, rotate the available mask by it, and change
nothing else. Placement cost is a rotate and an and; no lookup is affected at all, because a group
is compared whole either way.

**It reproduces the window's behaviour precisely.** At a million entries, churn, grouped against
grouped-with-a-per-key-start: tombstones recycled **33.8% against 16.3%** (the window: 15.8%), slots
after the run **2097152 against 4194304** (the window: 4194304), churn **67.2 ns against 87.5**,
three runs each with no overlap. One property moved and the whole behaviour moved with it.

**So the direction is the opposite of what I wrote when I proposed the experiment.** Spreading the
preferred lane does not improve recycling, it destroys it -- and *contending on one lane is the
feature*. A tombstone is created wherever a key was; if every key prefers lane 0 then lane 0 is
where the keys are, so lane 0 is where the tombstones are, so the next placement lands on one
instead of consuming a fresh slot. Concentration is what makes a tombstone design recycle. That is
worth knowing about `absl::flat_hash_map` and `emilib`, which both take the lowest lane and should
keep doing so; it is an argument *against* any per-key lane spreading in a tombstone design; and it
is the reason a sliding window, whose first lane is the home slot by construction, cannot recycle
well.

**And it cannot apply to this map at all, which is the part I got wrong twice.** `erase_group_slot`
writes a fingerprint of **0** -- a genuinely empty slot, not a tombstone -- so there is nothing to
recycle, and lane position within a group does not affect any lookup, because `match_fingerprint`
compares all sixteen in one instruction. Rotating the preferred lane here is a no-op by
construction, and proposing it was a mistake made by carrying a finding across a design boundary
without checking which side of it the finding lived on.

**A dense map on flat_wmap's structure, measured against the shipped one, and the comparison is not
what it looks like** (asked as "basically indivi map with the uint32 values"). Variant 1 already *is*
that map -- sliding window, one metadata byte per slot, a `uint32` index, a dense value vector -- so
a third variant was added that is `ankerl::unordered_dense` itself behind the same interface, and
all three run the identical workload code one binary each. Variant 2 reproduces the production
harness to within 1% (build at 200000: 8.87 ns per element here, 8.77 from `maps_one.sh`), which is
the check that the workloads are honest.

| per op, median of three | grouped+tombstones | window+dense | shipped |
|---|---|---|---|
| build, 200000 | 16.69 | 16.66 | **9.10** |
| hit, 200000 | 8.62 | 7.74 | **6.77** |
| miss, 200000 | 7.76 | 6.65 | **4.72** |
| churn, 1M | 65.71 | 83.87 | **62.01** |
| bytes/entry, 1M | 27.26 | **27.26** | 28.31 |

**Read that as a prototype against a tuned library, not as a design comparison.** The build gap is
83-143% and almost none of it is the index: the shipped map has the pipelined rehash, and the
prototype rebuilds one element at a time. The lookup gap is the merged block and the prefetches. The
design question is answered by variant 0 against variant 1 -- same author, same afternoon, same
quality -- and that says the window is worth 1-5% on a hit, nothing on a miss once the baseline has
counters, and costs 24% of churn at a million entries.

**So: not worth adapting, and the reason is structural rather than a number.** A sliding window
forecloses *both* of the things the shipped index is built on -- there is no group to hang a
per-class counter on, so the miss falls back to stopping on an empty slot and the map acquires
tombstones; and sixteen fingerprints from an arbitrary slot are not contiguous, so the merged block
goes too. The counters are worth 1.4-1.7x of a miss against an otherwise identical SwissTable and the
merged block 7% of a lookup's instructions and 28% of its dTLB misses at 4M. The window is worth a
few percent of a hit and 2-4% of memory. It is the wrong side of that trade by an order of
magnitude, and no amount of tuning the prototype changes which side it is on.

**And what the prototype cannot answer, which is why `indivi::flat_wmap` is fast in absolute terms.**
Both of its variants are dense, with a value index between the metadata and the key, and identical
metadata width; `flat_wmap` is *flat*, with the key in the slot the window found, and carries one
metadata byte per slot against this map's 5.5. Those are the differences the eighteen-map comparison
already attributes it to -- 48.0 instructions per hit against `flat_umap`'s 54.3 and this map's 60.5
-- and the prototype holds both of them fixed on purpose, because the question it was built for was
"is the window worth anything, all else equal". The answer to that is a few percent. The answer to
"why is that map fast" is the flat family and one byte of metadata, and the window is the smallest
of the three.

**Hoisting the moved element's hash out of `finish_erase`, so its latency overlaps the erase's own
work** (2026-09-08, asked as "calculate the hash of the last element early but use the result as late
as possible"). The premise is right and worth keeping even though the change is not: `finish_erase`
computes `mixed_hash(get_key(val))` *after* `val = std::move(m_values.back())`, and that store is one
no compiler can prove does not alias the key it would then load, so the hash cannot start until the
move retires. It is the `fill_buckets_from_values` shape again, and unlike that one it cannot be
fixed by reading through an iterator -- only by computing the hash in the caller and passing it in.

Three placements, one map per binary at 200000 entries, three repetitions each: before the counter
walk with a guard for the no-move case, before it without the guard, and after it. **All three are
instruction-neutral where it matters and none is a time win.** The guarded version costs 9
instructions on an integer erase and 32 on a string one -- the branch, plus `back_mh` living across
the callback -- and reads 10% slower on `churn64`. Unguarded that falls to +4 instructions and still
reads 12% slower on integers, because the hash's load chain is issued *in front of* the erase's own
probe, which is the latency that actually matters. Placed after the counter walk it is free
instruction-wise (299.5 against 299.5) and the medians are a wash: `churn64` 32.19 ns against 32.16,
`churnstr` 279.3 against 270.0.

**And the measurement is the lesson.** A single first run showed `churnstr` 265.0 against 270.0 and
looked like the predicted 1.8% win; the next two read 288.3 and 279.3 against 273.6 and 267.4. Three
runs of `churnstr` at 200000 spread **8.8%**, so nothing of this size is resolvable there however
many placements are tried. The instruction counts, which spread 0.4%, are what say the change is
neutral. This reproduces two older entries -- the robin hood era's "computing the moved element's
hash early: out-of-order execution already hides these latencies", and the erase-side
`prefetch_key` at 1-2% in cache and nothing out of it -- and the reason is the same: `do_erase`
already prefetches `m_values.back()` before the counter walk, and the walk is short enough that the
out-of-order window covers the rest.

**An SVG in an `<img>` follows the reader's colour scheme, not the page's** (2026-09-08). The blog
charts carried a `@media (prefers-color-scheme: dark)` block, and the page they sit on is a blog
whose background is hard-coded `#FFFFFF` -- so a reader whose OS is set to dark got the title at
`#f9fafb` on white, **1.05:1**, and every label at `#9ca3af`, 2.54:1. It is invisible, and no
light-mode contrast check can see it, which is how it survived the audit that fixed the entry below.
Verified both ways with `google-chrome --headless --blink-settings=preferredColorScheme=0|1` and a
one-line HTML that embeds the SVG, which is the cheap way to test this class of thing. The block is
gone from `mapsplot.py`; put one back only alongside a surface rect that switches with it, or a page
that does. `doc/`'s charts from `plot.py` keep theirs, because GitHub's own dark mode darkens the
page underneath them.

**A value label never goes inside its bar** (2026-09-08). `mapsplot.py` used to draw the value in
white inside the bar whenever it would have overflowed the panel, which is 6 to 11 labels per chart
-- and white on a bar drawn at `opacity="0.8"` composites to 3.0-4.1:1 against the page, under the
4.5 an 11px label needs, worst on the dense green. There is no ink dark enough for the inside of a
mid-tone bar either (#1f2937 on the full-strength green is 3.4), so the fix is room rather than
colour: the panel now reserves the widest label's width and the scale shrinks to fit, which costs
the longest bar 23% of its length and makes every label the same colour in the same place. The
sawtooth's direct labels were tinted with their series colour for the same reason and had the same
problem; they carry a swatch of the line beside them now and wear the ordinary label ink. **Do not
fix this class of thing by darkening the palette** -- every green that clears 4.5:1 as text drops
the palette's tritanopia separation from 6.5 to 4.3, and the colour is a *mark*, which needs 3:1
and has it.

The whole table is in the post; four results are worth having here.

**A miss is where the counter earns its keep, and abseil is the control that proves it.** At the
32000 octave, time relative to this map: abseil is the fastest map measured on a **hit** (0.73) and
**1.38 on a miss**; boost is 0.79 and **0.83**; indivi's `flat_umap` 0.82 and 0.97. Same group
compare, same SIMD, same load factor within 0.075 -- the difference is that abseil's miss has to
find an *empty control byte* and at load 7/8 that is often not in the home group, where boost's
overflow bit, indivi's counter and this map's counter all stop at home. So the overflow byte and the
overflow counter are worth 1.4-1.7x of a miss against an otherwise identical SwissTable, measured
across families rather than by patching this one.

**`indivi::flat_wmap` is the fastest hit of anything measured** -- 0.71 at 32000, 0.62 at 500000 --
and it is the *simplest* index in the comparison: one byte per slot, no groups at all, tombstones,
load 0.8. The reason is that its sixteen byte window is read **unaligned starting at the home
bucket**, so the home is lane 0 and a key at home is the first bit of the first mask; an aligned
group puts the home in the middle and half the group is behind it. What it pays is the widest
sawtooth here: 2.12x between the cheapest and dearest point of the 32000 octave, against 1.5-1.6x
for the group designs. Worth testing on the group index, and the reason it may not transfer is that
this map's value indices are addressed per group.

**The chained designs lose the miss badly**: emhash8 2.13 and Verstable 2.10, both against this
map's counters -- and Verstable executes 17% *fewer* instructions per miss than this map does. A
chain must be walked to its end and whether there is one is unpredictable, which is the same result
the counter-width table records from inside this header.

**And the own-hash control moved further than expected.** For a string key `boost::hash` costs boost
31% on a hit and 48% on a miss (0.87 to 1.14, 0.85 to 1.26) and turns a map that is ahead of this
one into one that is behind it -- but **`absl::Hash<std::string>` costs abseil 1-4% and nothing
else**, so the "its own string hash is slower" line above is about boost specifically and not about
defaults in general. For an integer key both defaults are *cheaper* than this wyhash, and abseil's
is worth **1.4x on a build** (`absl-own` 1.12 against `absl` 1.61), which is the largest single
effect of a hash anywhere in this comparison.

**The counters, one map per binary, 30M lookups at 50000 entries** (`scripts/ab/maps_one.sh`), which
is where the sub-10% questions were settled. Per lookup, instructions / cycles / branch misses:
all-hits `indivi::flat_wmap` 48.0 / **19.8** / 0.035, absl 56.1 / 21.4 / 0.044, boost 57.0 / 24.8 /
0.094, this map 60.5 / 29.4 / 0.065, emhash8 **48.4** / 36.6 / 0.420, Verstable 61.2 / 34.8 / 0.426,
`std::unordered_map` **45.1** / 52.4 / 0.325. All-misses: this map 57.2 / **20.7** / 0.108, boost
54.2 / 20.4 / 0.164, **absl 61.1 / 32.6 / 0.362**, Verstable **44.6 / 40.8 / 0.806** at IPC 1.09.
Nobody here is instruction-bound (IPC 0.78 to 3.26 on a four-wide core); the fast ones are the ones
the predictor gets right. At a million entries the dTLB column is the family split: 1.34-1.37 misses
per hit for the flat maps that touch one region, 1.78-2.21 for the dense ones that touch two, 2.58
for a node map -- which is the same 22%-worth-of-huge-pages gap recorded above, measured from the
outside for the first time.

**Memory, and the second place tombstones show up.** Bytes of heap per live entry, `mallinfo2`
around a build and around a full turnover, octave geomean at 32000. Eight byte value: absl 27.0 ->
**31.0**, indivi-w 31.0 -> **35.7**, ihtab 36.1 -> **72.3**, and *every* tombstone-free map flat to
the byte (this map 32.6, boost 29.2, F14Value 29.2, indivi-u 28.6, Verstable 28.6, emhash8 38.0).
64 byte value: **the node maps win** (94-95 against this map's 107.6, boost's 122.1 and emilib's
130.2), because a flat map pays for every empty slot at the full width of the value and a node map
pays a pointer. That reverses the eight byte ordering completely and is the one column where
`std::unordered_map` (108.4) is competitive with anything. emilib has tombstones and does *not*
grow, because it counts only live elements against its limit -- it pays in probe length instead.

**The size sweep, and the measurement mistake it took three tries to get right** (2026-09-06,
`doc/*_vs_size.svg`, `scripts/ab/sweep.cpp`). Three charts -- find with a 50% hit rate, churn, and
insert-erase -- of one map grown through 377 sample points, twenty-four per octave (193 and twelve
until the regeneration later the same day), nothing reserved, to 1M entries.

**The mistake is the part worth keeping.** The first two versions measured main to completion, then
this map, then boost. Sequential phases: anything that drifts between them -- a clock ramp, a noisy
neighbour, page placement -- lands entirely on whichever map was running, and none of it cancels.
Two runs of *identical* work then disagreed by up to 140% above 1M entries and by tens of percent
below it, and one contaminated run put a whole octave 70% high while looking perfectly smooth. That
run is what produced the "insert-erase is anomalously slow at 256 buckets" feature; there was never
anything wrong with the map. nanobench's own documentation says this plainly, and the A/B harness
next door has done it correctly all along: `compare()` runs the alternatives interleaved round by
round, so drift cancels out of the ratio. The sweep now does the same, and reports each point's
interval alongside it. Even paired it needs **101 epochs** for intervals around 5% of the ratio;
at nanobench's default 11 they were 25% wide, which is the real reason the early numbers moved.

With that fixed, and only out to 1M. **Summarise across an octave, never at a chosen load.** The
earlier version of this section quoted ratios at "load 0.79", meaning the last sample point before
*this map* doubles -- and that is a biased subsample, because boost sizes differently (max load
0.875, and 1966079 buckets at a million entries, not a power of two) and swings 4-6x across its own
octave. Reading this map at its fullest against boost at wherever its own cycle put it produced
"this map is 2.2x ahead of boost on churn below 100000 entries", which the geometric mean over the
same octave does not support. Retracted; the honest statistic averages over both sawtooths.

Octave geomean, this/boost and this/main, above 1.00 meaning the other map is ahead, from the
2026-09-06 regeneration at twenty-four points per octave (the octave starting at each size):

| workload | 1K | 32K | 128K | 512K |
|---|---|---|---|---|
| find, all hits | 1.07 / 0.76 | 1.21 / 0.72 | 1.14 / 0.76 | 1.28 / 0.70 |
| churn | 1.13 / 0.68 | 1.25 / 0.66 | 1.46 / 0.72 | 1.90 / 0.85 |
| insert and erase | **0.97** / 0.74 | 1.22 / 0.73 | 1.31 / 0.78 | 1.56 / 0.74 |

So boost is ahead of this map on all three at every size except insert-and-erase at a thousand
entries, where this map is 3% ahead -- by 1.07-1.13x at a thousand entries and 1.28-1.90x at half
a million. This map is ahead of robin hood everywhere, by 1.2-1.5x. The same table for string
keys, this/boost with this map's hash and this/main and then this/boost with the hash boost ships,
octaves starting at 1K, 32K, 128K and 256K: find, all hits 1.06 / 0.94 / **0.77**, 1.11 / 0.93 /
0.95, 1.16 / 0.91 / 0.94, 1.18 / 0.90 / 0.96; churn 1.05 / 0.83 / 0.82, 1.19 / 0.92 / 1.08, 1.28 /
0.92 / 1.12, 1.38 / 0.92 / 1.21; insert and erase 1.04 / 0.85 / 0.78, 1.13 / 0.92 / 0.95, 1.16 /
0.92 / 0.98, 1.22 / 0.91 / 1.03. The this/main string column is where the new hash shows, since
main still has the old one. The dense
map's answer to that is the two charts boost is not on: 10.9x on iteration and a build that is faster
at every value size. What is *not* true, and was asserted here for a day, is that the counters buy a
win over boost on churn at small sizes.

The flatness claim survives, because it is about one map's own curve rather than a comparison: over
a fully sampled octave from 1K to 64K, cheapest point to dearest, this map swings 1.2-1.5x on churn
where boost swings 4.2-6.1x and 4.8.1 swings 2.1-2.4x.

**Where a year of this got to, measured against 4.8.1** (2026-09-06, `scripts/ab/run.sh -r 3234af2
-b all 12`, the revision `main` stood at on 1 January 2026: scalar robin hood, no vector probe
anywhere). Score geomean **1.467 under clang and 1.434 under gcc**, 1.60 and 1.57 without the three
iteration workloads, with `build64` 2.60, `churn64` 2.16, `rhit64` 2.16, `churnbig` 1.99 and
`rmiss64` 1.98 leading it and iteration at 1.00-1.08, which nothing this year touched.

Two results in that run matter more than the geomean, and both are in `scripts/ab/README.md` with
their evidence. The **string hash is 4-7% slower than 4.8.1's** -- `hashstr` 0.96 clang, 0.93 gcc
with the gcc interval excluding parity, and boost's row moving with the candidate, which is the
control that says it is the hash rather than the map; the suspicion is that July's wyhash work was
tuned while every benchmark string was 200 bytes long, and is not confirmed. And **the lookup gain
is mostly at high load**. One map per binary so nothing shares a translation unit, 20M unreplayed
all-hits lookups each, ns per `find()`: at load 0.50 (33000 entries in 65536 buckets, and 132000 in
262144) 4.8.1 reads 7.21 and 8.52 against main's 6.28 and 8.47 and this map's **4.94 and 7.01**; at
load 0.79 (52000 and 208064) it reads 14.26 and 16.98 against main's 8.79 and 10.50 and this map's
**5.95 and 7.62**. So 4.8.1 is level with main at the empty end and 1.6x behind at the full end, and
1.2-2.4x behind this map throughout. **A 50% hit rate is not the average of its parts and can order the maps differently from both.**
Paired, one binary, 101 epochs, ms per 200000 lookups at 33000 entries (load 0.50): all hits main
1.367, this **1.089**, 4.8.1 1.552; all misses main 0.811, this **0.649**, 4.8.1 0.650; 50% hits main
2.037, this 1.753, 4.8.1 **1.725**. This map is fastest on each pure case and loses the mix by 1.6%,
because an unpredictable outcome costs a clean probe a fresh half misprediction per lookup (0.026 to
0.545) and costs a probe that already mispredicts 0.6 times on every hit almost nothing (0.599 to
0.605). Making the harness's own hit-or-miss select branchless moves none of it, so the branch is the
map's own "did I find it". The whole curve says how narrow the window is -- at 33000 entries, us per
200000 lookups, this map against 4.8.1: 666/782 at 0% hits, 975/995 at 10%, 1314/**1275** at 25%,
1736/**1729** at 50%, 1437/2013 at 75%, 1125/1769 at 100%. Every map peaks at 50%, which is where
the outcome is least predictable, so that point is the least discriminating of the three: this map
leads by 1.57x at 100% hits and by 1.00x at 50%. At load 0.79 it wins at every hit rate by 1.3-2.1x. So the mix says nothing about which lookup is faster, and `find_vs_size`
plots the mix -- which is why `find_hits_vs_size` exists beside it. On that one **4.8.1 is the
slowest of the four maps at 164 of 193 sample points**, behind this map at every size below 440000,
2.86x behind it at 3251 entries and load 0.79, and swinging 2.05-2.35x across an octave against this
map's 1.07-1.28x. The eleven points where it leads are all above 440000 entries, where everything is
waiting on memory.

`doc/find_vs_size.svg` carries 4.8.1 as a fourth line and is the picture of that. Over one octave
4.8.1 swings **1.26-1.59x** between its cheapest and dearest point, against 1.11-1.28x for main,
1.04-1.14x for this map and 1.10-1.22x for boost; point by point 4.8.1 runs from 0.91x of this map
just after a doubling to 1.30x just before one. The year did not make the best case much faster, it
removed the worst case.

**`scripts/ab/regen.sh` rebuilds everything in `doc/`** -- baseline headers, the four tools, the
measurements, the SVGs and the page -- and it is the only way to get them, since none of its output
is tracked. `--redraw` does the drawing half alone in a fifth of a second, which is what to use after touching `plot.py` or `dashboard.py`, since it reproduces every
SVG byte for byte from unchanged CSVs. `--quick` runs the whole pipeline coarsely in ten minutes,
which is how to find out that a tool no longer compiles without spending four hours.
The sweep asks nanobench for a precision instead of naming a round count, which needs
`targetIntervalWidth()` and `render(CompareResult)`; that landed upstream as martinus/nanobench#189
and has been in the vendored copy since 2026-09-08, so `NANOBENCH_INCLUDE` no longer has to point
anywhere. The guard in `regen.sh` stays, because that variable can still name something older.

**The same-hash convention was flattering boost on every string chart, and the control found it**
(2026-09-06). Handing every alternative this map's hash is the right way to compare *indexes*, and it
is what this project has always done -- but it is not what a caller gets, and on string keys the
difference reverses the answer. Boost with the hash it ships with, against boost given this wyhash,
geomean per octave (above 1.00 means its own hash is slower):

| workload | uint64_t, 1K to 512K | std::string, 1K to 256K |
|---|---|---|
| find, all hits | 0.93 to 0.99 | **1.30 to 1.22** |
| find, 50% hits | 0.98 to 0.99 | 1.31 to 1.27 |
| churn | 1.00 flat | 1.17 to 1.09 |
| insert and erase | 0.99 to 1.00 | 1.26 to 1.19 |

For an integer key boost's default is **1-7% faster** than this wyhash: `boost::hash<uint64_t>` is
close to the identity, foa mixes internally anyway, and the multiply is pure cost -- and once the
table leaves cache it buys nothing, which is why the column walks back to 1.00. For a string it is
**8-31% slower**, its default string hash not being wyhash.

**That flips the string ranking.** This map against boost, geomean per octave, above 1.00 meaning
boost is ahead:

| string workload | vs boost + this wyhash | vs boost's own hash |
|---|---|---|
| find, all hits | 1.07 to 1.17 | **0.82 to 0.96** |
| insert and erase | 1.10 to 1.12 | **0.87 to 0.97** |
| churn | 1.09 to 1.33 | 0.93 at 1K, boost ahead above |

So "boost is ahead on string lookups", said repeatedly in this file and its README, is a statement
about boost *with this project's hash*. Out of the box, `boost::unordered_flat_map<std::string, V>`
is 4-18% slower than this map on string lookups. The integer picture is unchanged -- boost leads
there with either hash. Both are worth having, which is why both are on the charts, but only one of
them is what a reader gets by typing the type name.

**A fifth series, and it is a control** (2026-09-06): the same `boost::unordered_flat_map` with the
hash it ships with, beside the one given this map's hash. Every other alternative on the charts is
handed this map's hash so that what differs between them is the index; that one says what the hash
choice is worth on its own, and is what a caller actually gets by passing no third template argument.
It is drawn dashed in boost's own colour, and hatched on the bar charts, because no fifth hue clears
the colourblind floor against the other four -- the best candidate is 2.7 apart from the blue under
deuteranopia against a floor of 8 -- and because colour for the map and style for the hash is the
truer encoding anyway. `robin hood (main)` is relabelled **4.11.0**, which is what it is.

The two value-size charts are **bars**, the size-axis ones lines: six value sizes are categories, and
a line between 32 and 48 bytes interpolates something nobody measured. `--bars` in both `plot.py` and
the dashboard, grouped, anchored at zero, 2px of surface between neighbours. Build and iteration are
separate charts rather than panels of one, since they differ by two orders of magnitude.

**Every workload is measured for both key types** (2026-09-06): `sweep.cpp`, `valuesize.cpp` and
`memory.cpp` all take a key argument, and the string half uses the scored benchmark's own
`key_for` -- 8 to 135 bytes skewed towards short, because one fixed length makes the hash's length
dispatch perfectly predictable. Two things changed in the tools to make that possible and are
improvements in their own right: the miss keys now come from a disjoint pool built up front rather
than from `key ^ 1`, so a miss is absent by construction and is not a neighbour of a key that is
present; and churn takes its insert from a spare pool and gives back what it erased, so no key is
constructed inside the timed region -- which for a string would have measured the allocator.
`memory.cpp` counts by replacing global `operator new` rather than through a container allocator,
because a string's body is allocated by `std::allocator<char>` inside the string and a container
allocator never sees it; for integer keys the two methods agree to the byte.

**The charts are also a page you can interrogate** (2026-09-06). `scripts/ab/dashboard.py` writes
`doc/charts.html` from the CSVs in `doc/`: every chart, a legend that shows and hides a map across
all of them at once, a y axis that rescales to whatever is left, a crosshair that reads exact values
and each map's multiple of the fastest, the hidden set carried in the URL so a view is a link, and
light and dark from the same validated palette `plot.py` uses. Self-contained, stdlib only, no
network. It does not replace the SVGs, because GitHub strips scripts out of an SVG and will not run
any of it -- the SVGs are what the READMEs embed and the page is what you open when a line looks
wrong.

**Why a dense erase is not slow, and where it is** (2026-09-06, asked as "don't we have to hash more
because of the moved element?"). We do: `finish_erase` moves `m_values.back()` into the hole, then
re-hashes that element's key and runs a second probe, `slot_of_value`, to find the slot pointing at
it. Two hashes and two probes per successful erase. Measured at a million entries, erase of a random
key plus an insert against a sequence with the same two cold probes but no move at all (a find, an
insert, and an erase of the element already last):

| | with the move | without |
|---|---|---|
| `uint64_t` keys | 57.0 ns | 56.2 ns |
| `std::string` keys | 410.4 ns | 377.7 ns |

**For an integer key it is free**, and three things make it so: the moved element is always the back
of the value vector, which in a churn loop is the same handful of cache lines and stays hot; the
first thing `do_erase` does is prefetch `m_values.back()`, so that load runs under the counter walk
that follows; and an integer hash is one multiply, so the group access it produces issues early
enough to overlap the erase's own probe and the insert's placement rather than queue behind them.
Netting out the extra operation the no-move sequence needs, the second hash and probe come to about
7 ns at a million entries.

**For a string key it costs about 50 ns**, because the hash is wyhash over 8 to 135 bytes behind a
heap pointer -- a dependent load, then a long chain -- and none of that overlaps. That is the same
thing the rejected back-pointer measured from the other side: `churnstr` 1.045 and `iestr` 1.024 for
it, against `build64` 0.953. Worth remembering that the rejection was scored on a suite whose string
tables are 200000 entries and cache-resident; the charts now cover the regime where the cost is
largest, so it is a candidate for the same re-test the merged block just had.

**Two optimizations the charts point at, one measured and one not yet** (2026-09-06, from asking
what the size sweeps imply rather than what they say).

**Huge pages are worth 22% of a large lookup and nothing asks for them.** The charts' worst regime
for this map is the one past L3, and counting there says why: at 800000 entries and all hits, this
map takes **1.48 dTLB misses and 7.03 L1 misses per lookup against boost's 0.89 and 5.15**, while
executing only 14% more instructions (95.3 against 83.2) for 33% more cycles. It is memory-side, and
a third of it is address translation -- this map touches three regions per lookup (group metadata,
value index, values) where a flat map touches one. `/sys/kernel/mm/transparent_hugepage/enabled` is
`madvise` on this machine, which is a common default, and neither the map nor the benchmark ever
madvises, so all of it runs on 4 KB pages. Handing both maps an allocator that `mmap`s 2 MB-aligned
and `madvise(MADV_HUGEPAGE)`s, ns per hit: at 800000 entries **this map 17.10 to 13.32 and boost 9.75
to 7.58**, both about 22%; at 200000, 7.18 to 7.10 and 5.47 to 5.29, which is nothing. So it is free
speed in exactly the regime the score cannot see -- 200000 entries is the largest thing the score
builds -- and it does not change the ranking, since it helps both equally. Worth doing as an opt-in
allocator and worth documenting; not worth pretending it closes the gap to boost.

**Merging the group metadata with its own value indices: measured, and kept.** The claim above that
it had never been tried was wrong -- the split's own comment in the header recorded trying it and
finding the split "10% faster on a build for the same lookups". I had read `CLAUDE.md` and not the
header. So this is a re-test, and it is the same shape as the back-pointer case: the earlier verdict
came from a regime that does not cover where the cost now is.

One 88 byte block per group -- 16 fingerprints, 8 counters, 16 value indices, `struct block : Group`
so every existing use of the metadata reads unchanged -- and no padding, so it is the same bytes the
two arrays took, in one allocation rather than two. Memory is unchanged to the byte: 27.0 per entry
steady and 32.5 at the growth peak, exactly as before.

Against `HEAD`, two independent paired runs under clang and one under gcc: score **1.018, 1.022 and
1.015**, find **1.045, 1.053 and 1.044** as a group, `rhit64` 7.1-7.5%, `findbig` 6.2%, and builds,
churn and insert-erase neutral (`build64` 100.0% and 101.2% under clang, build 0.999 under gcc,
intervals straddling parity). So the build advantage the split was kept for is gone, most likely
taken by the rehash's store-to-load fix, which changed where a build spends its time.

The mechanism is in the counters rather than in the score, which is what makes it believable -- one
map per binary, all-hits lookups at 200000, 800000 and 4M entries, split against merged: **7% fewer
instructions** (66.9 to 62.0 per lookup at 200000), because the index is at a fixed offset from the
group rather than a second address to compute; **12-14% fewer L1 misses**; and **28% fewer dTLB
misses at 4M** (5.30 to 3.79), because a lookup touches two regions rather than three. Cycles per
lookup 46.6 to 42.5, 96.1 to 88.6, 470.8 to 457.5.

`hashstr` is the control worth keeping in mind here: it never touches the map, and it read 0.923 in
one run and 1.126 in the other. That is the size of pure code-layout luck in this binary, and it is
why the counters rather than the score are what this rests on.

Three `pmr` tests failed on it, all of them counting allocations: a table is now the values plus one
index array rather than two. They asserted 5, 3, 3, 6 and 2 as literals; they now derive from
`index_t::array_count`, which is what that constant is for and what `lazy_bucket_allocation.cpp`
already did.

**The four charts worth keeping, and the two axes that had none** (2026-09-06). Asked which four
graphs decide a map, the answer needed two new tools, because two of the four axes were unmeasured:
`scripts/ab/valuesize.cpp` (build and iteration against `sizeof(mapped_type)`) and
`scripts/ab/memory.cpp` (bytes per entry, steady and peak, from a counting allocator). The set is
find-that-hits against size, churn against size, build-and-iterate against value size, and memory
against size -- see `scripts/ab/README.md` for why each and why not the alternatives.

What the two new ones say. **Value size**: boost against this map is 1.37x on a build at an 8 byte
value and 2.10x at 64, and 10.5x on iteration at 8 bytes falling to 2.3x at 64. That updates the
older note above, which had boost *ahead* at 16 bytes and behind at 64 -- the crossover moved off the
chart entirely when the rehash store-to-load fix made building 1.80x faster, so this map now leads at
every value size measured. The axis stops at 64 bytes because 200000 entries of a 64 byte value is
14 MB and still in L3 where 128 is 27 MB and is not, and past that every line bends upward together.
**Memory**: the axis that matters is the mapped-value size, not the table size. Per entry, memory
barely moves with the entry count -- the size chart is sixteen identical octaves -- but the ratio to
a flat map is a function of the value: at a million entries this map against boost is **1.19x** at an
8 byte value, 1.65x at 64 and 1.81x at 256 steady, and 1.48x, 1.81x and 1.86x at the growth peak,
because a flat map pays for its empty slots at the full width of the value where a dense one pays
four bytes of index. Charting the 8 byte case alone, which the first version did, shows the dense
design at its least impressive. Absolute, at a power of two: 27 bytes per entry against 32 for main,
4.8.1 and boost, peak 32.5 against boost's 48 and main's 40. 4.8.1's peak is 32, *lower* than main's
40, which is the price of `8d0e17e` -- building the new bucket array before releasing the old one is
what makes growth exception-safe, and it costs a taller transient.

**The sweep replayed its key sequence, which flattered the branchiest probe by 2.7x** (2026-09-06,
found because the 4.8.1 line came out ahead of this map and that was not believable). Each workload
seeded its `Rng` inside the timed function, so all 400 epochs looked up the same 20000 keys in the
same order and made the same hit-or-miss decisions, which a TAGE-style predictor learns. One binary,
replayed against carried-on, per 20000 all-hits lookups at 33000 entries: main 118.6 against 120.8
us, this map 104.6 against 96.1, **4.8.1 79.8 against 154.1**. Nearly all of it lands on the scalar
robin hood probe because it is the only one with branches to mispredict -- 0.575 per lookup against
main's 0.021 and this map's 0.024, from `perf` on one-map binaries. It is the mistake the "Lookup
benchmarks must not replay" entry above records fixing once already, reintroduced in a different
tool; the scored workloads are safe from it only because `find_all` does 10M lookups per epoch where
the sweep does 20000. Every chart in `doc/` predating the fix was wrong, and the sweep now keeps its
rngs in a `state` that outlives the epochs. The rule this leaves: a benchmark whose per-epoch batch
is small enough to memorise must advance its own randomness, and the check that catches it is
one-map-per-binary with `perf` -- the fixed sweep agrees with that to 3-8% and orders the maps
identically, the replayed one did not.

**Two more things the sweep got wrong, found on 2026-09-06 while adding a confidence band to the
absolute chart.** Neither is about the map.

*The first sample point of a process reads high*, because cold caches, a cold allocator and a
ramping clock are all paid by whoever goes first -- and pairing cannot cancel it, since what is cold
is the *point* rather than one alternative. At 16 entries boost came out 19% above its own value at
17, on intervals 1.5% wide. The sweep now measures the first point twice and throws the first answer
away.

*And a single paired run is still not trustworthy point by point.* One of five find runs had this
map alone reading 15-54% high at four adjacent sizes -- 4.59 ns at 64 entries against 2.86 to 3.03
in the other four -- with 0.5% intervals saying nothing was wrong and the other two maps normal
throughout. The second sighting gave the cause: adding a fourth map put boost at 4.72 ns at 128
entries where every other run says 2.4-2.6, wrong from 128 to about 8192. The sweep *grew* its maps
through the sample points, so each map's addresses depended on the interleaved history of the
others' allocations and an unlucky layout persisted until the next reallocation. It now rebuilds
every map at every point, which costs about a second across the whole sweep and makes a bad layout
spoil one point rather than a stretch. The rule stands anyway, since nothing makes one run right on
its own: read a chart for its shape, and check a surprising *point* against a second run. On a quiet machine
two runs agree to 0.78% median on the absolute median epoch, 0.49% on the fastest epoch and 0.92% on
the ratio -- and 9.3%, 6.9% and 6.0% at their worst points, which is what the confidence bands
drawn on the charts do *not* bound, since a within-run interval says nothing about what differs
between two runs.

**Retracted, because the data behind them does not reproduce**: that boost's find lead "peaks around
2M and narrows again", that main overtakes this map on churn above 8M, and every other number this
file previously carried for tables above 1M. The sweep is capped at 1M for that reason. Measuring
the regime above it needs a fresh process per size, not one incremental pass, because at multi-GB
sizes the result depends on page placement that varies from run to run.

**Two regimes the score does not cover, measured 2026-09-06 when asking what a more realistic
benchmark would be.**

*Small, short-lived maps* -- build, use and destroy, 2000 times, `map<uint64_t, size_t>`. Expected
to be a weakness, because the index is now two allocations rather than one and a lookup touches
three cache lines rather than two. It is the opposite: against main 1.90x at 8 entries, 2.55x at
32, 1.64x at 128 and 1.52x at 1024, and ahead of boost at every size except 8 (where boost is
1.97x main against this map's 1.90x). The reason is in the counters: branch mispredictions run
0.0-0.8% here against main's 0.8-3.4%, which is the robin hood shift being a coin flip and the
group probe not being one. Nothing to fix; worth a workload only to keep it that way.

*Tables far larger than cache* -- this is the real gap. The score's largest is 200000 entries,
whose index is about 1 MB and sits in L2 or L3 on any machine that runs it, and the value index is
one dependent load that a flat map does not pay. Half-hit lookups, same hash, this desktop:

| entries | main | this map | boost |
|---|---|---|---|
| 200000 | 9.8 ns | 7.1 ns | 5.9 ns |
| 8000000 | 54.4 ns | 42.8 ns | 30.4 ns |

So between those two sizes boost's lookup lead grows from **1.20x to 1.41x**, and the score only
ever sees the small end of that. Against main this map is still ahead at both sizes (1.38x and
1.27x), so it is a design cost rather than a regression -- but it is the one cost this benchmark
suite systematically understates, in the same way it once understated growth, churn and big values
before workloads were added for them.

**Both of those points come from one unpaired measurement each and should be read as indicative.**
A later attempt to turn them into a trend, by sweeping the size axis to 134M, produced a shape --
lead peaking near 2M and narrowing again -- that did not survive re-measurement and has been
retracted; see the sweep entry above for what went wrong and what the paired version says. What is
safe to say is that the lead exists at both of these sizes and that any claim about it has to name
the size it was taken at. What makes it awkward to add as a *scored* workload is the price: 8M entries is
~170 MB and about a second to build, so it would dominate the suite. So it is a tool rather than a
score entry -- `scripts/ab/sweep.cpp` walks every power of two from 16 to 8M for the working tree,
a baseline revision and boost, and `scripts/ab/plot.py` draws the CSV as an SVG with nothing but
the standard library. `doc/find_vs_size.svg` is the result, and `scripts/ab/README.md` says how to
regenerate it. Nothing under `doc/` is tracked except the two `allocated_memory` files: the CSVs
and the charts drawn from them are derived, so they are produced by `scripts/ab/regen.sh` rather
than committed, and a fresh clone has none of them until it runs it. It measures the scored find workload's case, a
random lookup with a 50% hit rate decided by its own rng, and draws two linear panels: sizes to
64K, which is every table most programs build, and the whole range, which now runs to 134M entries
-- about 5 GB of map, and far enough past the caches that the curve flattens into the
memory-latency-bound plateau where the ratios stop moving. Three flat, close lines to
about 128K entries, then all three turn upward together and the gap to boost opens as they do --
and, because nothing is reserved and the sampling is twelve points per octave, a sawtooth all the
way along as each table's load factor climbs to the maximum and falls back at the doubling. The
amplitude of that sawtooth is itself a result: robin hood swings by about a factor of two between
an empty table and a full one, where the group index and boost barely swing, because a group is
compared whole whatever its occupancy.

**Pulling a displaced sibling home on erase, to take the churn drift back** (2026-09-06). The
idea: when an erase frees a slot in group g and any of g's eight counters is nonzero, look one
group along g's sequence for an entry whose class has a nonzero counter, hash its key to confirm
its home is g, and if so move it into the freed slot and decrement the counter. The counter is what
makes the check cheap when there is nothing to do -- one 8 byte load -- and it does work: after
200 turnovers at load 0.76, groups per hit 1.137 to 1.086 against a fresh 1.032, per miss 1.252 to
1.181 against 1.052, from 1.24M pull-backs in 10M erases. Half the drift, one step deep.

What it costs is the other half of the sentence. At load 0.76 some counter of the freed group is
nonzero on **46% of erases**, fresh or churned, because counters count everything that passed the
group and not only g's own siblings, and the counter cannot tell the two apart. Each of those
scans the next group and hashes two or three candidates' keys to find out, which is two or three
value loads on the erase path. Paired, `uint64_t` keys, a churn round (erase, two lookups, insert)
at 50000 entries **39.9 to 60.3 ns**, and at 2M entries 231 to 278; what it buys on the churned
table is hits 6.20 to 5.81 ns and misses 4.19 to 3.58 in cache, and **nothing at all** out of it
(42.2 against 42.1, 13.0 against 13.0), because a one-step displacement lands in the adjacent
block, which the spatial prefetcher already brought in. So it pays 20 ns per erase to save 0.4-0.6
ns per lookup in cache, break-even at thirty to fifty lookups per erase, and never out of cache.
Not kept. The lazy version is, and it is the entry below.

**Where a string hit's ~90 cycles go, from perf, and two more things it led to that lost**
(2026-09-07, asked as "try perf"). One map per binary, `find_all<map<std::string, size_t>, true>`
at 50000 entries, `cycles:P`, by symbol:

| | clang | gcc | what it is |
|---|---|---|---|
| `wyhash::hash` | **41%** | **46%** | the hash, **called out of line** -- neither compiler inlines it |
| `do_find_hashed` | 30% | (inlined into the harness) | the probe: group compare, index load, the stored key's size, the compare's setup, and its own prologue and epilogue -- it is an out-of-line call too |
| `__memcmp_evex_movbe` + plt | 11% | 12% | the key compare |
| the harness | 17% | | rng, key selection |

Inside the hash, by line: ~25% on the multiplies, ~16% draining at the finalizer (the tail of the
chain, where a latency-bound pipeline empties), ~15% on the length compares (mispredict skid), ~10%
on the loads. Branch misses: **1.1 per lookup**, 829M branches per 30M lookups.

**The per-instruction miss attribution is not to be trusted here, and the check that shows it is
cheap.** `perf annotate` put 0.30 misses per lookup on the string `operator==`'s size check -- a
`jne` on `cmp 0x8(%rbp,%rax,8), %r15`, the stored key's size loaded from the value vector at a random
index -- and 0.32 inside `memcmp` on its `vpcmpnequb` instructions. A counting `KeyEqual` says the
size check actually fails **0.025 times per lookup** and the content compare 0.0005, and that `jne`
has zero *cycle* samples. Both attributions are skid: on this machine (IBS) a miss lands on the
first branch after a load that is waiting on memory, and the size load is a cache miss on every
lookup. So the symbol-level split is roughly right and the instruction-level one is not, and the
probe's own branches are clean -- 2.5% of lookups see a wrong-sized fingerprint collision first,
which the predictor absorbs.

Two things the profile pointed at, both measured in the map with a same-code control:

- **Force-inlining the hash** (the restated body under `always_inline`, the lane path left out of
  line, confirmed inlined by `nm`): clang `rhitstr` 1.015 against a control of 1.040, `findstr`
  0.991 against 1.011 -- nothing, or slightly worse; gcc 1.035 against 1.023, `rmissstr` 1.059
  against 1.010 -- one to four percent, inside a control that swings up to 10% in this binary. The
  41% is the hash's work, not its call.
- **A block-structured string compare in place of `memcmp`**, eight bytes at a time with length
  branches at the hash's own thresholds so the predictor has just seen their outcomes: **3-7%
  behind the control on hits under both compilers** (clang 0.969 against 1.046, gcc 0.970 against
  1.030). libc compares 32 bytes per instruction; the "0.32 mispredicts inside memcmp" that
  motivated this were the skid above.

What is left is the shape of the thing: a string hit is the hash's arithmetic and dispatch (at
their floor, four experiments), one cache miss for the stored key through the value index (the
dense design's structural extra hop, documented since the first chart), and a `memcmp` libc does
well. None of the three has a lever a hash or compare change can pull. Stop here.

**The last structural lever on the hash, taken and found to weigh nothing** (2026-09-07, asked as
"you are the most advanced model, improve the hash for the map"). The block range's chain is two
dependent multiplies and the second exists only to repair the bits a product leaves weak. The map
reads two places -- the top bits for the group, the bottom byte for the fingerprint -- and a per-bit
avalanche of the fold *without* its finalizer says those are exactly the weak places: worst
|p - 1/2| 0.076 in bits 0-7 and 0.074 in bits 36-63, 0.024 in between. So the question was whether
something cheaper than a 128-bit multiply repairs them. Two things do, at every boundary length from
17 to 300 and under two seeds: **`x * C`** (one `imul`, 3 cycles, a bijection) and **`x ^ rotl(x,
32)`** (2 cycles), both at the 0.02 noise floor everywhere the current finalizer is. (Not
everywhere: boundary lengths are the ones that cannot see this shape fail, and the entry above
sweeps every length and finds it failing at `len % 16 == 2`.) `hi64(x * C)`
does not (0.041 in the top bits: the high half of a product by a constant is weak at the top). And
neither works on the short path: at 8 bytes the two reads are the same bytes and at 9-15 they
overlap, the inner multiply's operands are correlated, and only a real second multiply repairs
that (0.36-0.50 otherwise). So the candidate was precisely scoped -- the short path keeps its two
multiplies, the 73% of scored keys past 16 bytes lose one.

**In the map it is worth nothing.** Six hashers interleaved with a same-code control, the control
reading 0.996-1.008: block-range `imul` `rhitstr` 1.006, `findstr` 1.009, `churnstr` 1.009,
`iestr` 1.009, `rmissstr` 0.988; block-range `rotl` 1.001, 1.006, 1.008, 1.008, 0.971; `hashstr`
1.09-1.18 against a control of 1.08, which is that benchmark's layout swing and not a result.
Three cycles off the hash's chain does not show in a lookup at all. Not adopted, since when speed
ties the stronger finalizer is the one to keep for everyone who uses `hash<std::string>` outside
the map.

**Six more hashes tried, none adopted, and the pair of columns says why** (2026-09-09, asked as
"give these hashes a try as well" -- rapidhashNano, foldhash, komihash, polymur-hash, AquaHash and
gxhash). All six are in `scripts/ab/hash_others.{cpp,sh}` now, so the table below re-runs. Two of
them ship as Rust crates and are ported in `scripts/ab/hash_ports.h`; both ports are checked rather
than trusted. **gxhash reproduces the three `is_stable` vectors from its own test module**, and
**foldhash is bit-identical to the crate** at eleven lengths from 0 to 300 bytes for both variants,
against a `cargo run` of the real thing.

**Strict avalanche first, because it decides which rows are even candidates.** Worst and mean
|P(output bit flips) - 1/2| over every (input bit, output bit) pair, 12000 samples, noise floor
0.014. Seven of the nine are clean at every length: udm5, rapidhashNano, foldhash-*quality*,
komihash, polymur, AquaHash and gxhash all read 0.015 to 0.022. Two are not:

| key bytes | `absl::Hash` | foldhash-fast |
|---|---|---|
| 4, 8, 12 | 0.495 to 0.500 | 0.497 to 0.500 |
| 16 | 0.078 | 0.074 |
| 17 | 0.067 | **0.500** |
| 48 | 0.076 | 0.179 |
| 128 | 0.079 | 0.072 |

foldhash says so itself: the fast variant "is optimized purely for speed in hash tables and has
known statistical imperfections", and `foldhash::quality` is the same hash with one more folded
multiply, which is exactly what repairs it.

**Then the two time columns, and they order the field oppositely.** Median of three processes, the
scored key mix (8 to 135 bytes skewed short), latency net of the chain floor, relative to this hash:

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

**On latency, which is what a map lookup pays, nothing clean is faster than what unordered_dense
already ships.** The one hash ahead of it is foldhash-fast, by 2%, and it buys that the same way
abseil does, with one folded multiply where this hash has two. **On throughput, which is what a
hashing loop pays, this hash is fifth of twelve**, and the two AES designs are exactly where the
earlier gxhash port said they would be: gxhash is the fastest of all twelve in a loop (0.76x) and
third from last on latency (1.67x), and AquaHash is 2.2x ahead of this hash at 256 bytes in
throughput (3.33 ns against 7.24) while being 1.55x behind on latency. Two AES hashes, measured
independently, saying what one of them said in July. They also need `-maes`, which a header cannot
assume.

Three things worth keeping beyond the verdict.

**rapidhashNano ties this hash exactly at 8 and 16 bytes** (4.12 and 4.11 ns against 4.11 and 4.11)
and loses from 32 up (5.27 against 4.52, 10.71 against 8.02 at 256). That is the same code at the
short end, since the two-overlapping-reads trick came from rapidhash in the first place, and the
independent-block change measured against the design it was derived from at the long end.

**polymur-hash is not slow by accident.** It is the only hash here with a provable universality
bound, a Carter-Wegman construction over a Mersenne prime field, and 1.95x latency is what that
costs. A different goal, priced.

**And a throughput number taken through a function pointer is not a throughput number.** The first
version of this harness dispatched every hash through a `uint64_t (*)(void const*, size_t)`, which
prices the call rather than the hash and hurts a big function most: gxhash read **1.33x** that way
and **0.76x** when the expression is inlined into the loop the way `hash_others.cpp` has always done
it. The latency column barely moved (every ratio within 0.03 of the inlined one, on all nine
hashes), because a call is a constant added to a chain. So the two harnesses agree on latency and
disagree on throughput by 1.75x, and only one of them is measuring what a caller sees.

**Why `absl::Hash` is faster below 32 bytes, and what taking it would cost** (2026-09-09, asked as
"figure out why abseil's hash is faster up to 32 byte, can we learn something from them"). The
own-hash control rows put `absl::Hash` 9 to 22% below this hash at 8, 16 and 32 bytes and 12 to 23%
above it at 64, 128 and 256. The mechanism is one number: **abseil is one 128-bit multiply deep at
every length up to 32, where this hash is two.** At 8 bytes and below it is `Mix(state ^ v, kMul)`,
one operand a compile-time constant; at 9 to 16 `Mix(state ^ first8, kMul ^ last8)`; at 17 to 32 two
*parallel* mixes of overlapping 16 byte ranges xored together. None of the three has a finalizer,
because the length is mixed in at the *start* instead: `PrecombineLengthMix` xors an unaligned load
from a 40 byte constant table indexed by `len`, which issues immediately since the caller knows the
length. Past 32 bytes it calls an out-of-line function, and that is the 12 to 23% it gives back.

**What it costs, measured rather than assumed.** Strict avalanche, worst and mean |P(output bit
flips) - 1/2| over every (input bit, output bit) pair, 20000 samples, noise floor 0.011:

| key bytes | this hash | `absl::Hash` |
|---|---|---|
| 8 | 0.0149 / 0.0028 | **0.5000** / 0.2557 |
| 12 | 0.0164 / 0.0028 | **0.4981** / 0.0314 |
| 15 | 0.0141 / 0.0028 | **0.4923** / 0.0041 |
| 16 | 0.0139 / 0.0028 | 0.0710 / 0.0030 |
| 17 to 64 | 0.014 to 0.017 | 0.064 to 0.081 |

A worst of 0.50 is a pair that is deterministic, and the one at 8 bytes is easy to name: the fold is
`hi ^ lo` of `x * K`, and flipping x's top bit changes `hi` by `K >> 1` plus a carry, which cannot
reach bit 63, while `lo`'s bit 63 always flips. Measured directly, **flipping input bit 63 flips
output bit 63 in 1,000,000 of 1,000,000 cases**. That is a linear relation in the bits this map uses
for the group. So the speed is bought with avalanche, and abseil's header says as much in its own
way, worrying only about a zero operand.

**The idea does transfer to the block range, and the earlier test of it above is wrong.** The entry
above says `x * C` and `x ^ rotl(x, 32)` sit at the noise floor "at every boundary length from 17 to
300". Boundary lengths are exactly the lengths that cannot see the failure. Swept over *every*
length from 17 to 144, the single-multiply block range fails at **len % 16 == 2** and nowhere else:
worst 0.055 to 0.073 at 18, 34, 50, 66, 82, 98, 114 and 130, against 0.009 to 0.012 for the shipped
hash, on two seeds at 6000 and 40000 samples. The tail block is then a two byte shift of the last
front block, and one fold cannot separate two near-duplicates.

**One rotate repairs it exactly.** Rotating the tail block's product by 27 before the xor breaks the
shift symmetry: worst 0.0345 / mean 0.0053 over 17 to 144 against the shipped 0.0368 / 0.0052, no bad
lengths, and no equal-content length collisions. The length then needs no finalizer either, and
abseil's table is not needed for it: a plain `^ len` into the first block's second operand measures
the same as the table lookup and is two instructions cheaper. Standalone that whole shape is
**1.14x at 17 to 32 bytes, 1.12x on the scored mix net of the harness's chain**, and identical below
17 bytes where nothing changed.

**And in the map it is worth nothing, again.** One map per binary, `map<std::string, size_t>`, 20M
lookups, three rounds, patched header against shipped: **+1.75 instructions per lookup** at every
size and both outcomes, and cycles inside the noise on all four of hit and miss at 32,000 and
200,000. Measured twice over, once with independent lookups and once with the next key drawn from
the previous answer so that nothing overlaps, and the dependent version does not favour it either.
Two cycles off a chain of 180 is 1%, which this instrument cannot resolve and a user cannot feel.
Not adopted, and the reason is now the quality one rather than an unresolved tie: the shipped
finalizer is worth keeping for `hash<std::string>` outside the map.

**What cannot be taken at all is the short path.** Below 12 bytes the two 8 byte reads overlap by
five bytes or more, and at exactly 8 they are the same word, so one product has correlated operands.
Six one-multiply shapes were tried there -- one mul alone, plus `x * C`, plus `x ^ rotl(x, 32)`, two
parallel muls with swapped operands, the same with a rotate, and abseil's constant-operand form --
and every one of them reads 0.16 to 0.50 worst at 8 to 11 bytes. From 12 bytes up one multiply plus
`x ^ rotl(x, 32)` is clean, but 12 to 16 bytes is only 8.6% of the scored key mix, so there is
nothing there to chase either.

Taken together with the entry below, the picture is now complete enough to stop: the hash's serial
chain is two multiplies, removing one is invisible, a length-branch structure that removes
mispredictions costs more than it saves, and a hash that is faster in every hashing loop (AES)
loses 37% in the map. **The hash is done; what a string lookup still pays is in the probe and the
key compare, not in hashing.** The next thing to measure, if the string lookup is the target, is
where its ~90 cycles actually go.

**Four attempts at tuning the hash for latency, all worthless in the map, and the microbenchmark
that said otherwise was wrong** (2026-09-07, asked as "can you do more latency tuning"). The premise
was sound: on unpredictable lengths the length dispatch costs **0.50 branch mispredictions per
hash**, which at ~16 cycles is a whole multiply. Four structures, all keeping the short path and the
long lanes:

- **v2, 32 byte steps instead of 16** -- half as many decisions for the predictor, at the price of
  up to one extra pair of reads and one extra multiply, which are independent and so cost throughput
  rather than latency. It does what it claims: branch misses 0.50 to 0.42.
- **v5**, `len` out of the finalizer, so the last multiply has a compile-time constant operand.
- **v6**, `len` mixed into the head block as well as the finalizer.
- **v3**, both.

In a standalone latency harness these looked decisive and reproduced on both compilers to 0.02 ns:
v0 6.87, v5 5.52, v6 5.44, **v3 4.90** -- a 1.40x improvement, clang and gcc agreeing exactly, which
is normally enough to rule out layout.

**In the map every one of them is nothing.** Same map, the scored string workloads, six hashers
interleaved with a same-code control: `rhitstr` v3 0.998, v5 0.996, v6 0.997 against a control of
0.996; `findstr`, `churnstr`, `iestr`, `buildstr` all 0.99-1.01. And v2, the one that genuinely
removed mispredictions, is a consistent **loss** (0.92-0.97) -- so the extra multiplies cost more
than the branches they save, which is the same answer the fully branchless version gave and for the
same reason.

**Why the harness lied, and it is a trap worth naming.** To make lengths unpredictable it took the
next key's index out of the previous hash -- `x = hash(keys[x & mask])` -- which is the standard way
to build a dependency chain and is what the hash chart's latency panel does. But that puts the
key's *length* on the chain: `len` is `keys[x & mask].size()`, so it arrives late, and a finalizer
that consumes `len` extends the chain. A real lookup has no such edge. The caller already holds the
key, so its length is known before the hash starts and only the *bytes* are loaded. The assembly
says the same thing plainly: v0 and v5 differ by exactly one `xor` instruction before the final
`mul`, with no spills in either -- one cycle, not the five the harness reported. `v7`, which hoists
`S[1] ^ len` into a named variable, is the control that proves it is not about scheduling: 6.84
against v0's 6.87, i.e. nothing.

So: **a latency microbenchmark that chains through key selection measures a chain the map does not
have, and it overstates anything that touches the key's length or address.** The hash chart's
latency panel has the same edge in it, which is a second reason its numbers are an ordering rather
than an amount.

What this leaves is that the block range is already at its floor: two dependent multiplies, and the
second one cannot go, since dropping it fails avalanche outright. One multiply is ~4 cycles on
about a third of a string lookup, which is ~1.5% of a lookup even if it were free -- under the
noise of the score. The hash is not where the remaining time is.

**The hash measured against the ones the other libraries ship** (2026-09-09,
`scripts/ab/hash_others.{cpp,sh}`, asked for the blog post). `hash.cpp` charts this hash against its
own older versions; this one puts it beside `boost::hash`, `absl::Hash` and `folly::hasher` at 8, 16,
32, 64, 128 and 256 bytes and on the scored mix, latency and throughput separately, all interleaved
in one process. Latency net of the chain (a row that hashes nothing prices the chain at ~1.5 ns),
ns per hash, on the mix: **this 4.8, absl 4.8, 4.11.0 5.4, boost 8.0, folly 14.0**. Throughput on
the same keys: 2.22, 2.25, 2.37, 4.48, 8.80 -- same ordering, wider margins.

**`absl::Hash` is the one to beat and nothing here says otherwise**: 9 to 22% *lower* latency than
this hash at 8, 16 and 32 bytes and 12 to 23% higher at 64, 128 and 256, so on a mix that is mostly
short keys the two are level. That is the mechanism behind the own-hash control rows in `maps.sh`,
which had only ever been observed from the outside -- abseil loses 1-4% to its own hash, boost loses
31% on a string hit -- and it says the control is measuring the hash rather than any interaction with
the index. `folly::hasher<std::string>` is `SpookyHashV2` and is the slowest here at every length,
2.9x on the mix, which is worth knowing because it is what `F14FastMap<std::string, V>` uses unless
the caller says otherwise.

Against 4.11.0 the independent-block rewrite reads 17% at 32 bytes, 23% at 128, **12% on the mix**,
0% below 17 bytes and at 256 (unchanged code, which is the control), and **-3% at 64 bytes**.

**Three process-level repetitions are what make those digits mean anything, and `-n` does it.** A
within-run interval says how well one process resolved its own median and nothing about what changed
between one process and the next, which on a machine that is not idle is the larger of the two. The
first set of numbers here was taken with a browser and a jekyll watcher running and read 0.5 to 1.5%
high across the board; with those paused, `-n 3` merging by median per point, `warmup(200)` and
`minEpochTime(1ms)`, the three runs agree to **0.9% on every hash cell**. The one row that does not
is the do-nothing floor at 1.5 ns, where a 6% spread is timer granularity and not the machine.
**And do not edit a shell script while it is running**: bash re-reads the file at its old byte
offset, so a sweep that was already running re-executed its last line and appended a second run's
output to the first one's file, which is what a doubled header in a CSV means.

**Latency is what a map pays, tested rather than argued** (2026-09-07). The AES-NI rejection above
was reasoning -- a quarter faster in throughput, half again slower in latency, so it should lose in
a map -- and reasoning is not a measurement. Measured: the same `map<std::string, size_t>`, the
scored string workloads, three hashers interleaved by `compare()` in one binary, where the third is
a *control* -- `wy2`, the same wyhash written out as a second hasher type so every template is a
second instantiation. Whatever the control reads is what layout is worth here, and nothing smaller
than that can be claimed for AES.

| workload | control | AES | what the operation can overlap |
|---|---|---|---|
| `hashstr` (a hashing loop) | 1.00 | **1.28** | everything: independent hashes |
| `buildstr` | 1.01 | 0.91 | a lot: the rehash hashes sixteen ahead |
| `iestr` | 1.02 | 0.80 | some |
| `churnstr` | 1.01 | 0.82 | some |
| `rmissstr` | **1.13** | 0.75 | little |
| `findstr` | 1.01 | 0.71 | little |
| `rhitstr` | 1.04 | **0.63** | nothing: one lookup, one dependent chain |

So AES is **28% faster at hashing and 9-37% slower at every single thing a map does**, and the order
of the map rows is the mechanism rather than noise: the workload that can overlap its hashes loses
least, the one that cannot loses most. The prediction that `buildstr` might actually *win* was wrong
in sign and right in rank -- a build is only about half rehashing, and the other half is inserts
that each pay the hash's latency in full.

The counters say the same thing from the other side, one hasher per binary, 30M all-hits lookups:
AES executes **fewer instructions** (5.15G against 5.35G) and takes **59% more cycles** (6.54G
against 4.12G), IPC 1.30 down to 0.79. That is a dependency chain, not extra work. Two explanations
ruled out: it is not port contention with the SSE2 group compare, since building both with
`ANKERL_UNORDERED_DENSE_HAS_SSE2=0` leaves `rhitstr` at 0.66 instead of 0.63; and it is not a missed
inline, since `aeshash::hash` appears in no symbol table and the 36 `aesenc` instructions in the
binary are all inline. The hash avalanches indistinguishably from wyhash at every length tested, so
this is not collisions either.

**What does not add up, and is worth knowing before quoting the chart's right panel.** AES's
standalone latency disadvantage is 4.2 ns per hash, and it costs **14.9 ns per lookup** -- three and
a half times more than adding the two would predict. So the latency panel of `hash_vs_length.svg`
*understates* what a map pays: in that loop the next key's loads still issue while the current
chain runs, where in a lookup the chain's result is the address of the next dependent load and
nothing after it can start at all. Read that panel as an ordering, not as a number to add to a
lookup.

The general rule this leaves: **a hash for a map is chosen on latency; a hash for a loop is chosen
on throughput; and the two can order candidates oppositely by more than 2x** (AES against this
wyhash: 1.28 one way, 0.63 the other, on the same machine on the same day). Which one applies is
decided by whether the caller's code can have several hashes in flight -- this map's rehash can,
which is why it hashes sixteen ahead, and a lookup cannot.

**A chart for the hash, and its own control says how much of it to believe** (2026-09-07,
`doc/hash_vs_length.svg`, `scripts/ab/hash.cpp`). The scored suite has one hash workload and it
reports one number over a mix of lengths; that is the right summary and it hides the shape, because
a hash dispatches on length and its cost is a staircase. Two panels, and the pair is the point:
**throughput** (independent keys, as many in flight as the machine has multipliers) is what a
hashing loop pays and what hash benchmarks report; **latency** (a byte of each answer fed into the
next key, so nothing overlaps) is what a *map* pays, since the hash's result is the address of the
group to probe. That distinction is what decided AES-NI -- a quarter faster on the left panel, half
again slower on the right.

Geomean per length range, 4.11.0's time over this map's, above 1.00 meaning this map is faster:

| bytes | throughput | latency | what the code does there |
|---|---|---|---|
| 1-16 | 1.09 | **1.00** | identical source: the short path was not touched |
| 17-48 | 1.21 | 1.18 | independent blocks, two or three multiplies |
| 49-96 | 1.11 | 1.10 | four to six |
| 97-144 | 0.99 | 1.17 | seven to nine |
| 145-256 | 0.99 | 0.99 | identical source: the chained lanes were not touched |

**The two identical-source rows are the control, and they disagree with each other.** The 145-256
row reads 0.99/0.99, which is what identical code should read. The 1-16 row reads 1.09 in
throughput and 1.00 in latency -- the same instructions, 9% apart, because restructuring what sits
*after* the short path's early return moved the code around it. So this chart's throughput panel
carries about 9% of layout in it and its latency panel does not, which means the honest reading of
the 17-144 range is the latency column: **10-18%, clean, with a control at 1.00 either side of it**.
The throughput gain is real too but 1.21 should be read as "up to 1.21, of which up to 9% is not the
algorithm".

Two more things the chart says that the score cannot. `boost::hash<std::string>` is **2.0-2.2x
slower than this hash in throughput and 1.1-1.7x in latency**, growing with length -- the mix number
(8-31%) is dominated by short keys. And 4.8.1's hash is *faster than this one in throughput* over
17-144 (1.06) while being **1.24x slower in latency**, which is the clearest statement of what July's
work traded and why the mix could not see it.

Capped at 256 bytes in the SVG, on a linear axis (`--xlin`, `--xmax`, both new in `plot.py`: a table
size is exponential and belongs on log2, a key length is not, and past 256 every line is straight).
The CSV keeps the full range to 1024. One length per point, so the length dispatch is perfectly
predicted here where the scored keys cost it 0.31 branch misses per hash -- `hashstr` is the number
that includes the dispatch, and this is the shape.

**The string hash restructured: independent blocks from 17 to 144 bytes** (2026-09-06, asked as
"make the hash faster, the values may change"). wyhash chains its 16 byte blocks through `seed`,
so a 48 byte key is three multiplies in a row before the finalizer and the map cannot form a group
address until the last of them resolves; and the block loop's trip count is a data-dependent branch
that mispredicts whenever lengths vary, which the scored keys do on purpose. Now every 16 byte
block up to 144 bytes is mixed on its own with its own pair of secrets and xor-folded into one
finalizer: latency is one multiply plus the finalizer for any length in the range, and the branches
are a short chain of compares on `len` that the predictor learns from the top. The short path and
the long chained lanes are unchanged, so lengths up to 16 and past 144 hash exactly as before;
`hash_golden.cpp` was regenerated for the range between, as its own comment says to do.

Measured on the scored keys (8 to 135 bytes, skewed short), one function per binary, ns per hash:

| | clang throughput | clang latency | gcc throughput | gcc latency |
|---|---|---|---|---|
| wyhash as it was | 2.52 | 8.64 | 2.18 | 8.47 |
| 4.8.1's wyhash | 2.19 | 9.14 | 2.21 | 9.16 |
| independent blocks | **2.00** | **7.69** | **2.05** | **7.81** |

Paired on the score: `hashstr` **1.13 clang and 1.12 gcc**, `rmissstr` 1.08 and 1.10, `iestr` 1.09
and 1.08, `buildstr` 1.07 and 1.06, `findstr` 1.06 and 1.06, `rhitstr` 1.04 and 1.04, `churnstr`
1.05 and 1.00, every integer workload at parity; score 1.006 and 1.039. The "string hash is 4-7%
slower than 4.8.1's" line in the section above is closed by this, in the other direction.

Measured and rejected on the way, all on the same keys:

- **Branchless middle**: always mixing three overlapping blocks for 17-48 and six for 49-96, so
  the length decides nothing but the read offsets. Slower on both compilers (2.48 against 2.35
  throughput under clang, 2.45 against 2.04 under gcc): the redundant multiplies cost more than
  the mispredictions they remove, which is the opposite of what the probe found, because a
  multiply is a real unit of work where a group compare is not.
- **One multiply on the short path** (`mix(a ^ s ^ len, b ^ seed)` and no finalizer): the fastest
  thing measured, 1.96 and 1.95 throughput, 7.41 and 7.74 latency -- and it fails an avalanche
  test outright: at 8 bytes some output bits never flip for some input bits (worst |p - 1/2| of
  0.50, mean 0.17 against 0.003 for the two multiply version), because the two reads are the same
  eight bytes and a single product of them has no second chance to mix. Dropping the finalizer in
  the block range fails the same test more gently (bits biased 0.42/0.58 at every length). Both
  multiplies stay; "the values may change" does not extend to a hash that does not avalanche.
- **AES-NI**, asked as "how about SSE for the hash": one `aesenc` per 16 byte block into an
  accumulator and two finishing rounds, compiled with `-maes`. Throughput 1.63 against 2.18 under
  clang and 1.47 against 2.07 under gcc -- a quarter faster -- and latency **12.0 against 7.85**
  and 11.4 against 7.82, half again slower, which is what the map pays -- measured a day later and
  worse than that, `rhit64`'s string twin at **0.63**; see the entry above. An `aesenc` is four
  cycles and the rounds are a chain. It is the 2025 gxhash finding again on the right key lengths
  this time, and it comes with a flag the header cannot assume. SSE2 itself, the one vector ISA the
  header may rely on, has nothing that beats a scalar 64x64 multiply for mixing: `pmuludq` is two
  32x32 products, and four of them plus the adds are slower than one `mulx`. A 16 byte load is no
  faster than two 8 byte ones. So: no.

**A mutating hit moves itself home, and that takes the churn drift back** (2026-09-06, the lazy
version of the entry above). The entry a hit just found is the one candidate whose home is known
without another hash -- the probe computed it -- and whether that home has room is one
`match_empty` on a group the probe just visited. So `move_home` runs on every hit inside a path
that already writes (`try_emplace`, `operator[]`, `insert`, `emplace`, `insert_or_assign`): a hit
at home costs a compare, a hit one group out with room at home costs a load, two stores, one zero
and the counter walk `erase` already does. It is not in `find()`, const or not: a non-const `find`
on a shared map is treated as read-only by callers, and this would make it a data race.

Drift, groups per lookup at 50000 entries and load 0.76 after 200 turnovers, fresh 1.032 per hit
and 1.052 per miss, base churned 1.143 and 1.265: with one `operator[]` hit per erase **1.091 and
1.162**, with four **1.054 and 1.093** -- nearly a fresh table, and it converges rather than
plateaus because every displaced entry that is touched again goes home.

**What it is worth, and the measurement that had to be done three times to find out.** The first
number, from a paired run of the two headers in one binary at 50000 entries, was misses 4.02 to
2.70 ns -- 1.49x -- and it was wrong. One map per binary, `uint64_t` keys, a table churned forty
times through with a writing hit per round, two runs each:

| 50000 entries, churned | base | move-home |
|---|---|---|
| churn round (`operator[]` hit, erase, miss, insert) | 42.7 ns | 42.5 |
| find, hit | 5.9 | 5.7 (1.04x) |
| find, miss | 5.16 | **4.64 (1.11x)** |
| branch misses, whole run | 7.59M | 6.26M |

At 2M entries every figure is within 1-2% either way, as with the pull-back: a one-step
displacement is the adjacent block, which the prefetcher already has. So: about a tenth off a miss
and a few percent off a hit on an in-cache table that has churned with writes, nothing on the churn
itself, nothing out of cache, +2% instructions per writing hit. Kept, because it is thirty lines
with no cost anywhere measured and the mechanism is the drift's cost itself -- the branch misses
say what that cost was: with a quarter of misses continuing past home the stop-or-continue branch
mispredicts, with a tenth it does not.

The score cannot see it and reads **1.000 under clang and 1.022 under gcc**, the gcc figure being
`buildbig` at 1.40 in a workload that never calls `move_home` -- gcc's inlining in the harness
translation unit moved, which is the `probe`-out-of-line story from the group index's first week
and not this change. Its workloads either grow, and a rehash resets the drift for nothing, or churn
with `find` between the erase and the insert, which moves nothing. `sweep.cpp` mode 5 is the
scored churn's shape with the hit through `operator[]`, added for this, and the size sweeps it
produced are the third measurement and the one to learn from:

**The sweep cannot resolve a question this size, and the way to know is to run the same code on
both sides.** Two builds of the same working tree, mode 1 (no lookups, so `move_home` never runs)
against the same baseline: octave 128-255 read **0.876 in one and 1.158 in the other**, and 524K-1M
read 0.997 and 1.067. Mode 5 read 0.860 at 1M in the build where mode 1 read 1.085, and the one-map
binaries tie there to 0.1% with identical counters. A same-code control (the working tree renamed
into both namespaces) reads 1.00-1.02 at every point above 128K, so it is not the harness; it is
the code layout of a translation unit holding two headers, and it moves 3% at large sizes and 30%
at L1-resident ones every time either header changes. The within-run intervals, which are what the
charts draw, say nothing about it. The rule this leaves is the one the `hashstr` control has been
saying all along: a paired two-header measurement decides a 10% question and not a 3% one, and
anything smaller is decided one map per binary, with counters.

Two tests in `move_home.cpp`: the churn with a mutating hit every cycle, checking every key and
its value, and a steered one on the identity hash that fills a group, sends one key past it,
makes room and provokes the move. Mutation sweep of `uncount` and `move_home`, 41 mutants: 14
caught by a test, 17 by the compiler, 3 hung, 7 survived -- four of them the counter decrement,
which is the same uncovered decrement the termination-bound entry above records (now shared
between erase and the move), and the other two the "already at home" early return and its
comparison, which mutated either move an entry to another lane of its own home group, which is
legal, or silently skip the repair. Nothing that changes an answer survived. One codegen detail
found by mode 1: `uncount` takes the group pointer, mask, home and counter as arguments loaded
*before* the caller's fingerprint store, because a `std::uint8_t` store may alias any of them and
a reload after it lands on the address chain of every step of the walk. The claim in `CLAUDE.md`
that nothing moves after placement is qualified by exactly this much: a
hit inside a write may move one step, to its home, and nothing else moves.

**The rehash loop pipelined, and the partitioned rehash it was measured against** (2026-09-06,
from asking what else the group structure is good for: placement is shift-free, so a rehash can
place in any order, which is what a database-style radix partition needs). First the target,
`rehash(0)` on a built map so the loop is isolated, ns per element:

| entries | index | u64 before | pipelined | partitioned | string before | pipelined | partitioned |
|---|---|---|---|---|---|---|---|
| 200K | 1.4 MB | 2.05 | **1.63** | 3.22 | 8.7 | 8.4 | 8.5 |
| 1M | 11 MB | 2.27 | **1.78** | 5.53 | 11.9 | 8.4 | 8.6 |
| 2M | 22 MB | 6.71 | **3.52** | 5.31 | 24.8 | **9.4** | 10.0 |
| 4M | 44 MB | 10.29 | 7.63 | **5.34** | 30.6 | 12.4 | 12.1 |
| 16M | 176 MB | 12.58 | 12.46 | **8.65** | | | |

*Pipelined* hashes sixteen elements ahead of the one it places and prefetches the group each will
land in, with the group pointer, mask and shift held in locals (a fingerprint store may alias any
of them through `this`, the same shape as the rehash fix above). Below cache it is 1.26x for the
pipelining alone -- hash chain and random placement no longer wait on each other -- and CLAUDE.md's
old note that prefetching ahead in the rehash was worthless was measured at 200000 entries, in
cache, where it must be. Above cache it does everything for strings, whose hash has work to hide a
miss behind (2.5x at 2M and 4M), and nothing for integers at 176 MB, because that loop is bound by
the TLB rather than by latency: **1.15 dTLB misses per placement** on 4 KB pages, and a prefetch
cannot hide a page walk. Partitioning by the top bits of the group -- one histogram pass, one
scatter of an 8 byte packed entry per element, then placement partition by partition -- cuts that
to 0.24 and halves the loop from 4M up.

**Kept the pipelined loop, dropped the partition.** Paired, three variants in one binary, a build
from empty: pipelined and partitioned are indistinguishable end to end (u64 8M 53.8 against 53.4
ns per element, strings 4M 128.5 against 128.8), both 3-18% ahead of the plain loop. Timing every
rehash inside a build says why the isolated 2x disappears: the scatter costs 3.6 ns there instead
of 1.4, because the scratch is fresh memory every time and faulting it in costs about a microsecond
a page -- the `tame_allocator()` lesson, paid by the map itself this time -- and a rehash is a
minority of a large build anyway (15 of 64 ns per element at 16M; the inserts are the rest). So
the partition is worth 0-7% of an integer build above 32 MB, for 8 bytes of scratch per element
at the growth peak, an allocation inside `rehash()` that changes what an allocator sees and what
an exception path has to undo, and a hundred lines. The pipelined loop is thirty lines, no memory,
and the score is exactly neutral on it (1.002 clang, 1.003 gcc, `buildbig` 1.04 and 1.07), which
is what a 200000-entry suite should say about a loop whose gains are above cache. Unit suite green.
What did not matter: the partition size, 16 KB to 256 KB (place phase 3.4 ns throughout, so it was
never L2 misses); the prefetch distance, 8 to 32 (16 best by a hair). What an integer rehash above
cache would actually need is huge pages, which the note above already measured at 22% on lookups
and which the map cannot ask for.

**Not zeroing the value index** (2026-09-06, from a code review that put it at ~7% of a build).
`std::vector::resize()` value-initialises, so growing the index writes 64 bytes of zeros per group
that nothing reads: a slot's index is written when an entry is placed there and read only for a
slot whose fingerprint already says it is occupied. An allocator adaptor whose no-argument
`construct()` default-initialises removes the writes, and for the group array beside it the zeroing
is load-bearing and stays, since a zero fingerprint is what empty means.

Measured properly -- two binaries, alternated, four rounds -- it is worth **1.7%** of a two million
element build, not 7%: 42.2 ms to 41.5, consistent in direction every round, and nothing at all on
the score, whose builds are 200000 elements and zero about 2 MB instead of 32. The first
measurement said 6.8% and was wrong, having compared runs made minutes apart, which is the mistake
the benchmarking section of this file exists to prevent.

Reverted for a reason that outranks the number: `assign()` copies the whole index array, so leaving
entries default-initialised means the copy constructor reads indeterminate `std::uint32_t` values.
That works everywhere and is undefined behaviour anyway, and a library header should not have it in
a copy constructor to buy 1.7% of a large build. Copying only the occupied slots would avoid it and
costs more than it saves. All 34 CI legs including valgrind passed with the adaptor in, which is
worth knowing: valgrind tracks definedness through a copy without complaining, so it would not have
caught this either.

**What `segmented_map` gives up in 5.0.0, found in review.** `IsSegmented` used to segment the
bucket array as well, through the `BucketContainer` parameter this branch removed; now it segments
only the values and the index is two plain contiguous arrays for every table. So a segmented map
keeps stable references and smooth *value* growth, and loses the promise that nothing spikes: the
index still doubles beside itself. At 5.5 bytes per slot the transient is ~16.5 bytes per slot,
which for a 100M entry `map<uint64_t, uint64_t>` is a 1.1 GB spike against 1.6 GB of values. Kept
rather than reverted, because segmenting the index means an indirection per group access in the
probe -- the one path everything else here is spent making short -- and `reserve()` removes the
doubling for a caller who cares. The README says so plainly now; it previously called the spike
"small next to the values", which is only true when the value is large.


**The SSE probe audited, and its one real redundancy is compiler-dependent** (2026-09-07, asked as
"take a good look at the SSE code, is there anything that can be optimized there"). The match
sequence has nothing left in it: `movdqu`, `pcmpeqb` against a broadcast that *both* compilers hoist
out of the loop (checked in the disassembly rather than assumed), `pmovmskb`, `test`. The broadcast
is only that cheap because `fingerprint_words` already holds the byte in all four positions, so
`_mm_set1_epi32` is `movd` plus `pshufd` where a genuine SSE2 byte broadcast would need a
`punpcklbw` as well; `match_empty` compiles to a `pxor`-zeroed compare, `lanes &= lanes - 1` to
`blsr`, and `first_lane` to `tzcnt`.

The redundancy is in `prefetch_index`, which asks for `p + 64` and `p + sizeof(block) - 1` = `p +
87`. With an 88 byte stride a block's offset within a cache line cycles through eight values, and
**six of the eight put both prefetches on the same line**, so one of them is waste three times in
four. Removing it is a clang win and a larger gcc loss. One map per binary, 30M all-hits lookups,
ns per hit, both prefetches / last only / `+64` only / none:

| | 50000 | 1000000 | 4000000 |
|---|---|---|---|
| clang | 6.47 / **6.03** / 6.16 / 6.15 | 29.71 / **26.42** / 27.94 / 28.03 | 51.73 / 49.05 / **48.69** / 49.48 |
| gcc | **5.78** / 5.87 / 5.91 / 6.17 | | **44.79** / 45.64 / 45.35 / 51.28 |

Dropping both costs gcc **12% at four million entries** -- 313.5 cycles per lookup against 277.9
while executing *fewer* instructions, which is a cache miss that stopped being hidden. The cause is
scheduling rather than source: gcc emits the `movdqu` before the two prefetches and clang emits both
prefetches before it, so under clang they take load-port slots in front of the load that is actually
on the critical path. **Left as it is**, because the gcc gain is larger than the clang cost and both
compilers are in CI. Misses are unaffected under either, which fits -- a miss with no fingerprint
match never reads the index at all. The paired score could not settle this: it read 1.006 for the
single-prefetch variant in a run where `hashstr`, which never touches the map, read 1.13.

That is the x86 half of the question the boost review left open above ("boost tunes the prefetch per
architecture ... this map issues the same two or three prefetches everywhere ... it has not been
asked"), and the answer for x86 is that there is nothing to tune that is right for both compilers.
The ARM half is still unasked and `bench.yml` could settle it. One trap worth naming: `-march=native`
silently upgrades these intrinsics to AVX-512 on this machine -- `vpcmpeqb` into `%k0`, `kmovd`, no
`pmovmskb` at all -- so a profile taken that way is not the code most callers run.

**Fingerprints and counters in two arrays instead of one 24 byte group** (2026-09-05). The layout
sweep in `martinus/ai#3` kept the two together in every one of its eleven layouts, so the split was
the one form never measured. The case for it: sixteen fingerprints are a quarter of a cache line,
so four groups fit a line exactly and none straddles, where a 24 byte group straddles one time in
four; the case against: a miss then needs a second line for its counter. Measured paired on the
nine workloads that touch the index, with the unchanged layout as a control: every ratio within
noise (`rhit64` 1.00, `rmiss64` 1.01, `find64` 0.99, `churn64` 1.00, `ie64` 1.02, control 1.00
throughout), and on a 20M entry table whose index is 37 MB and lives in DRAM, 57.1 against 57.0 ns
per hit and 34.3 against 34.1 per miss. A tie both in cache and out of it. The straddle is free
because the second line is the adjacent one, which the spatial prefetcher brings in with the first;
the counter line is free because its address depends only on the group, so the load issues beside
the fingerprint load rather than after it. Not kept: one array is simpler than two for the same
speed, and the same reasoning says the padded 32 byte group and the 16-fingerprint-plus-8-counter
split are the same question, already answered.

**Which counter a probe consults at each step of its sequence** (2026-09-05). The design reads the
class of the fingerprint's low three bits at every group. The false continues of a churned miss
come largely from *siblings* -- entries with the same home group, which walk the same sequence, so
a displaced sibling of the same class carries the miss along its whole displacement: 17.5% of
churned misses continue at step 0 and about half of those continue again at step 1, far above the
1/8 a fresh class check would give. Two alternatives, measured with a compile-time switch:

- **Class plus distance, `(fp + d) & 7`**: exactly a no-op, as the arithmetic says it must be --
  the sibling and the miss add the same d at every step, so if they agree at step 0 they agree
  everywhere. Churned miss 1.263 groups against 1.262.
- **Three fresh hash bits per step** (bits above the fingerprint, rotated by three per group): does
  break the lockstep, and the churned miss drops from 1.262 to 1.238 groups with the step-0 rate
  unchanged by construction. But that is 2% of a miss's probe work in the tail, on a path 17.5% of
  churned misses reach, and the paired measurement on the six workloads that can see it is noise:
  `rmiss64` 1.00, `churn64` 0.99, `find64` 0.99, `churnbig` 0.98, `findbig` 1.03, `rmissstr` 0.98,
  with the refactor control at 1.00 everywhere. The saving is real and smaller than the register the
  rotating word occupies.

Neither kept. What would move a miss is the step-0 decision, and that is the counter's class count,
which the row below settles.

**The width of the overflow counter, all four divisions of a group's eight counter bytes measured
against the design's eight one-byte counters** (2026-09-05, prompted by asking whether folly's
single per-group counter would help). It is the axis the design already sits at the top of, and the
two directions off it lose for opposite reasons. Fresh and after 200 turnovers at load 0.76, the
share of misses that continue past their home group, and the score:

| counters per group | fresh miss | churned miss | misses continuing (churned) | saturated at 200 turnovers | score |
|---|---|---|---|---|---|
| 1, F14 style (class ignored) | 1.21 | 2.79 | 60% | 0 | **0.959** |
| 8 x 1 byte (the design) | 1.06 | 1.26 | 17.5% | 0 | 1.000 |
| 16 x nibble | 1.03 | 1.13 | 9.7% | 0 | 0.986 |
| 32 x 2 bit | 1.02 | 1.15 rising | rising | 36063 | 0.988 |

Folly's one counter is the worst of the four, not the best: it does not know the fingerprint class,
so *any* overflow past a group makes every later miss into it continue, and a churned table where
most groups have seen an overflow sends 60% of misses on. Its score is 0.959 and its churn 0.83.
The idea that a shared counter saturates faster and so helps is aimed at the wrong thing --
saturation was never what stops a miss, the `delta == m_group_mask` bound is, and once saturated a
counter is *worse*, since it never comes back down. That is exactly what sinks the 2 bit counter:
it filters best of all when fresh (1.3% continue) but its max of 3 is reached constantly under
churn, and a saturated counter lengthens every later miss for the life of the array.

The nibble is the interesting direction and still loses. It genuinely filters better -- half as many
fingerprints per counter, so 9.7% of churned misses continue against the design's 17.5%, at no
memory cost and with a max of 15 that nothing reached even after 200 turnovers. But a sub-byte
counter is a load-mask-compare on the read and a read-modify-write on the increment, and that is
paid on *every* lookup, while the continuation it saves was already rare (4% fresh). So `find64`
0.908, `rhit64` 0.940, `findbig` 0.923: the per-probe arithmetic costs more than the rarer group
hop saves. The general shape is the one this file keeps rediscovering -- a filter only pays where
nothing cheaper filtered first, and here the group's own fingerprint compare already did most of it,
so a finer counter is refining a decision that is nearly always already made. A byte per class is
the point where the counter is a single aligned load and still per-class.

**And the fifth point on that axis: an exact counter, worth 2-3%** (2026-09-07, the one idea
Verstable has that this map does not -- see its entry below). `m_overflows[g][c]` counts every live
entry of class c that *passed* group g on its own sequence, whatever its home; Verstable's
in-home-bucket bit answers the narrower and exact question "does anything belong here". The
analogue here is a second set of eight counters per group holding "entries of class c whose home
**is** g and which did not fit in g", consulted at step 0, where the current test costs a whole
extra group visit whenever it is wrong.

Measured before writing any of it, on an instrumented copy of the header that rebuilds the exact
answer offline by hashing every occupied slot -- and checked against the invariant it must obey,
that an entry displaced out of g incremented g's counter on the way out, over 4096 groups and eight
classes with no violation. The instrumented probe reproduces this file's own figures to three
digits (churned miss 1.266 groups at load 0.763 after 200 turnovers against the 1.265 recorded
above in the `move_home` entry, 17.6% of misses continuing at step 0 against 17.5%), which is what
makes the rest of it
believable. On the table the shipped header actually leaves, with `move_home` firing:

| load | groups per miss now | with the exact counter | continuing at step 0 | of those, siblings |
|---|---|---|---|---|
| 0.763 fresh | 1.046 | 1.040 | 3.7% | 85.5% |
| 0.763, 200 turnovers | 1.159 | **1.134** | 11.4% to 9.3% | **81.5%** |
| 0.793, 200 turnovers | 1.242 | **1.201** | 16.1% to 12.9% | **79.7%** |
| 0.50, churned | 1.003 | 1.003 | 0.3% | ~99% |

**About 80% of what the counter fails to filter is siblings** -- entries that genuinely home in that
group and genuinely did not fit in it. Both tests say continue for those and both are right, since
the key really could be further along; being exact only removes the strangers, and strangers are a
fifth of the problem. For calibration against a change that was kept, `move_home` took 0.107 groups
off a churned miss and that was worth 11% off an in-cache miss and nothing out of cache. This takes
**0.025**, under a quarter of it, so 2-3% in cache on a churned table, nothing on a fresh one,
nothing at half load, and nothing out of cache. Against that: 8 more bytes per group (88 to 96, so
5.5 to 6.0 bytes per slot, 9% more index memory), a store into the home group on every insert that
does not fit home and every matching erase and every `move_home`, and a second counter for the
rehash, the erase and `move_home` to keep consistent -- more of exactly the invariant the mutation
sweeps keep finding uncovered.

So **this map's approximate counter is within 2-3% of the exact version of itself**, and the counter
axis is closed: folly's single counter 0.959 on the score, nibble counters 0.986, two-bit counters
0.988, three fresh hash bits per step noise, and exactness at step 0 worth 2-3% in cache only. What
is left of a churned miss is siblings, and the only thing that removes a sibling is putting it back
home -- `move_home` for the writing case, and for the reading case the erase-side pull-back that was
rejected for costing 20 ns per erase.


Both came from reading how other maps do it, and both lose for the same kind of reason: they add
work to a path that runs on every lookup in order to help a case that is rare.

- **Cache-line aligning the value indices**, which boost and abseil get for free because their
  groups are aligned. A group's sixteen indices are exactly 64 bytes, and glibc hands back large
  allocations at 16 mod 64, so *every* group's indices straddle two lines -- which is why
  `prefetch_index` asks for two. Giving the index array a 64 byte aligned block type does what it
  should on lookups (`find64` and `rhit64` both 1.02) and costs 4-5% on `build64`, `churn64` and
  `churnbig`, for a geomean of 0.993. The likely mechanism is conflict misses: with the group array
  and the index array both at power-of-two offsets, a group's metadata and its indices collide in
  the same cache sets more often than they do when one of them is skewed.
- **A second fingerprint in the spare high bits of the value index**, which is emhash8's trick: the
  index word has to be loaded to reach the value, so bits spent there are free, and they reject a
  fingerprint collision before the value vector is touched. Eight bits cost nothing until a table
  wants more than 2^24 slots. Measured: geomean 0.975, and the losses are exactly on lookups
  (`find64` 0.912, `findbig` 0.919, `rhit64` 0.930, `churn64` 0.915). It puts an xor, a shift and a
  compare into the dependent chain of *every* lookup to avoid a value access on the 3% that have a
  fingerprint collision. It is free for emhash8 because that map has no group-level fingerprint and
  must consult the word anyway; here the group already filtered, so the second filter is redundant
  work in the hot spot. The general shape is worth remembering: a filter only pays where nothing
  cheaper has filtered first.

- **A 16 bit value index for small maps**, which is CPython's compact dict: it stores 1, 2, 4 or 8
  byte indices depending on capacity, where this always stores 4. A `bucket_type::group_small` with
  `value_idx_type = std::uint16_t` makes the index 3.5 bytes per slot instead of 5.5 and puts two
  groups' indices in one cache line. Measured on the thirteen scored workloads that fit under 2^16
  elements: geomean **0.986**, with only `rhit64` (1.03) and `findstr` (1.02) ahead and `churn64`,
  `churnstr` and `findbig` 3-4% behind. The reason kills the adaptive version too, not just this
  one: a map small enough to be indexed in 16 bits has an index of at most 128 KB, which is already
  inside L2, so halving something that already fits buys nothing -- and the maps whose index
  footprint actually hurts are exactly the ones that need more than 16 bits. The narrow loads also
  cost a zero-extension on every use. `group_small` was not kept.

Three on the value vector, the one part nothing had touched, all keeping it dense (2026-09-05).
For scale first: growth is 54% of a 200000 element integer build here and 59% of boost's, so the
rehash is not where this map loses; the pure insert path is, 140 instructions per insert against
boost's 77, spread over a probe, a vector append, a second walk in `place_group` (4% of cycles, the
ceiling on any one-pass insert) and the spills of holding two index arrays plus a vector live. The
vector's own reallocations are 3% of an integer build and 11% of a 64 byte value build.

- **Reserving the values to the index's capacity at every index growth**, so the two grow together
  instead of on their own cadences: `build64` 0.985, `buildstr` 0.972, `buildbig` 0.967, nothing
  elsewhere. The total bytes copied are the same either way; what changes is that a full copy of
  the values now lands immediately before a rehash that wants the cache for the index.
- **A slot back-pointer per value** (a parallel `std::vector<value_idx_type>`, +4 bytes per entry),
  so closing the hole an erase leaves repoints the moved element's slot directly instead of hashing
  its key and walking to it. Pays exactly where that hash is expensive: `churnstr` 1.045, `iestr`
  1.024. Costs everywhere the vector grows: `build64` 0.953, `buildbig` 0.960, `iebig` 0.977,
  `churnbig` 0.984; integer churn is a tie. Net loss on the score, and 19% more memory for an
  8 byte value. The 2025 note above about storing the hash instead measured the same shape.
- **The same back-pointer plus indivi's 4 bit distance nibbles**, so that `erase(iterator)` needs
  no hash at all: the slot comes from the back-pointer, the counter from the slot's own
  fingerprint, and home from reversing the quadratic walk by the stored distance. On the one
  pattern it exists for, find then `erase(it)` then insert on a reserved table: **1.10x faster
  with string keys** (1006 to 888 instructions per round, one wyhash and two probes gone) and
  **1.10x slower with integer keys** (352 to 363, the saved hash is 8 instructions and the
  back-pointer maintained on every insert costs more). On the score, where every erase is by key,
  it can only cost, and does: geomean 0.959, `build64` 0.831, `buildbig` 0.893, `churn64` 0.900,
  with `iestr` 1.052 the one workload ahead. Memory 31 to 38 MB per million 8 byte values. Not
  worth an opt-in either: the win needs an expensive key *and* erase by iterator, and a caller with
  both has `erase(key)` with the hash already paid by their own `find`.
- **Growing the values with `realloc`** instead of allocate-move-free, for trivially copyable
  values. In isolation it removes a third of the vector's growth cost (0.242 to 0.164 ms for 16
  byte pairs, 0.713 to 0.476 for 72 byte ones, doubling to 200000), which is about 1% of an integer
  build and 4% of a big value one. It needs a container that is not `std::vector`, which the
  `AllocatorOrContainer` parameter already accepts, so it is available today as an opt-in and not
  worth changing what `values()` returns for.

**The insert path is split in two by clang, and that is most of the build gap to boost.** Found
2026-09-05 by counting instructions per insert on a reserved table, net of the benchmark loop:

| compiler | this map, insert (miss) | boost | this map, `operator[]` on a present key |
|---|---|---|---|
| clang 22 | 128 instructions, 39 cycles | 64, 26.5 | 74 instructions |
| gcc 16 | 82 instructions, 26 cycles | 55, 23 | 68 instructions |

Under clang `do_try_emplace` is its own function with a six register prologue, and it calls
`do_place_element` out of line, which clang refuses to inline at cost 480 against a threshold of
250 (`vector::emplace_back` with `piecewise_construct` is 225 of that). gcc inlines the whole
insert into the caller on its own. So the gap to boost on the insert path is 39% under clang and
11% under gcc, and the score moves with it: **under gcc this map is 1.25x ahead of boost on
`build64`** where under clang it is 0.70x behind, and 1.067 ahead of boost on the geomean without
iteration where clang has it at 0.96. The full gcc score against main is 1.165, against 1.109 for
clang, with `build64` 1.56 and `buildbig` 1.84. `scripts/ab/run.sh -c g++` reproduces it.

What was tried for clang, all measured paired on the score:

- **Forcing `do_place_element` and `place_group` inline** (`always_inline`): applied in 2026-09 on a
  paired geomean of **1.012**, removed on 2026-09-08, and **put back the same day**, which is the
  part worth keeping. The removal rested on the scored suite built one header per binary (1.7%
  faster under clang, 3.9% under gcc) -- and that binary is ~90 translation units of test suite,
  whose inlining budget is exhausted, so an `always_inline` there displaces something else. In a
  unit holding **one** map, which is what a caller compiles, building from empty is **14 to 20%
  slower without the attribute at every size**: 251633 ns against 287833 at 32000 entries, 1749840
  against 2087600 at 200000, 13064800 against 15670600 at a million. The instruction counts settle
  it, since neither layout nor drift moves them -- 5.08M against 5.98M, 28.09M against 33.71M,
  162.3M against 190.4M, **17 to 20% more work retired without it**. `maps.cpp` (eighteen maps in
  one unit) agrees: build 17% slower at 32000. **The rule is narrower than "one header per
  binary": the translation unit's *size* decides what an `always_inline` is worth, a benchmark
  binary is the largest unit anyone compiles this into, and an instruction count is the only
  number none of that moves.** What the attribute costs is real and unchanged -- `operator[]` on a
  present key pays the placement code's register pressure on a path that never places (clang 73.2
  instructions against 48.4) -- it is simply smaller than 17% of a build. The paired harness cannot
  see this class of change at all. The attribute did
  what the entry then said (miss path 128 to 100 instructions, `build64` 1.070; `ie64` 0.967, because
  the merged function pays the placement code's register pressure on the path that never places).
  Three things landed on `do_place_element` since -- the merged block, `move_home`, the pipelined
  rehash -- so the inlined body is bigger and that pressure is worse. One map per binary at 50000
  entries, forced against not: a reserved insert is clang 97.8 instructions and 31.3 cycles against
  106.8 and **27.2**, gcc 124.9 and 38.0 against **68.9 and 23.9**; a `try_emplace` on a key already
  present is clang 73.2 and 16.8 against **48.4 and 11.5**. It is also no longer the no-op for gcc it
  was described as. The scored benchmark built one header per binary and alternated, three rounds,
  0.1% spread: **clang 0.017720 to 0.017416 and gcc 0.018664 to 0.017940**, 1.7% and 3.9% faster
  without it -- against a paired run reading 0.979 and 0.995. Removing `probe`'s attribute as well is
  the worst of the three (gcc 0.019078, 2.2% *worse* than shipped), which is the control saying this
  is about one function and not about `always_inline` in general.
- **Handing the probe's fingerprint word and home group to an out-of-line `do_place_element`**, so
  the insert derives nothing twice: 141.7 to 143.7, i.e. nothing. **Returning the value index in a
  register** instead of a `pair<iterator, bool>`: 141.7 to 141.7, clang already returns that pair
  in registers. The 28 instructions are the call boundary itself -- prologue, epilogue, argument
  setup -- and only merging removes them.
- **`increase_size` out of line** so the hot function shrinks: no change, clang's cost is in
  `emplace_back`, not in the growth path.
- **The erase path** is already flat: `erase(key)` is 102 instructions under clang with or without
  forced inlining of `do_erase`, `erase_group_slot` and `finish_erase`.

The next thing to try, not yet done: get `do_place_element` under clang's threshold *honestly*, by
making the common-case append cheaper for its cost model than `emplace_back(piecewise_construct,
forward_as_tuple(key), forward_as_tuple(args...))` -- for a map of trivially constructible types
that is a 16 byte store dressed as 225 units of inline cost. Under gcc the same code is already
fully inlined, so this is a clang-only 7% on builds and churn waiting on codegen, not on design.

**ihtab and ixhtab measured, and a bug in one of them reported upstream** (2026-09-07,
`/home/martinus/gra/ihtab/sololynx`, vnmakarov/ihtab). `iht::ihtab` is an eight slot SSE group --
eight one byte tags interleaved with eight four byte indices -- at a **50% maximum load**, with
tombstones it never reclaims: `EMPTY` is `0xc0` and `DELETED` `0x80`, chosen so that `match_empty`
is one `movemask(g & (g << 1))`, and `els_bound` only ever grows, so a full element array is
compacted by rebuilding the whole table. `ixht::ixhtab` puts extendible hashing on top: a directory
of bins, each an `ihtab` with sixteen bit indices, split once a bin reaches
`1 << MAX_BIN_SIZE_POWER`.

Measured with the same harness as the Verstable entry below, but before it grew the octave sweep,
so these are single sizes and therefore single points of four different sawtooths -- read them as an
ordering, not as ratios. ns per operation:

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

ihtab is genuinely quick and the reason is on the label: at a 50% load factor a lookup almost always
lands home, and it pays for that with twice the slots. Buying probe length with memory is always
available to any of these designs and is not an index idea -- it is the same axis the two bit
counter sat on, filtering best when fresh. ixhtab's churn column is not a design property but the
bug below.

**The bug.** `ixhtab.hpp:290` decides whether a full bin should grow with `if (2 * els_num >=
indexes_size)`, where `els_num` is the **whole table's** live count (member at `ixhtab.hpp:102`) and
`indexes_size` is **one bin's** index size. For any table larger than a single bin that is always
true, so `grow` is always set and the code splits instead of compacting in place -- and since a
deleted slot is never reclaimed, a bin fills its element array from tombstones alone however few of
its elements are live. Each bin then splits about once per turnover, each split halves the live
occupancy of both halves, and nothing merges back. At a constant 50000 live elements over 40
turnovers the heap goes **1.4 MB to 44.8 MB**, 29.5 to 938.9 bytes per element and still doubling,
and a hit goes from 8.2 ns to 17-30. `ihtab::rebuild()` has the same-shaped test and is correct
there, because both quantities describe the same single table, which is why ihtab stays flat.
Present in all four headers (`ixhtab.hpp:290`, `ixhtab.h:290`, `ixhtab-v0.hpp:298`,
`ixhtab-v0.h:298`) and reported as vnmakarov/ihtab#2 with a self-contained reproducer, the cause and
a fix. The transferable part is the test that found it: **a workload that holds the element count
exactly constant while churning is the only one that can see this class of fault**, which is why
`churn` is in the score.

**Verstable measured rather than read** (2026-09-07, asked as "here is another hashtable I want
compared"; the bullet further down had it from reading the header alone). One `uint16_t` per bucket:
four bits of hash fragment, one bit saying "the key here belongs here", and an eleven bit quadratic
displacement to the next key in this bucket's chain -- so every key homed at a bucket sits on one
linked list threaded through otherwise-unused buckets, and a lookup visits *only* buckets holding
keys that belong to it. Key and value inline in a flat bucket array, both arrays out of one
`malloc`, `MAX_LOAD` 0.9, tombstone-free, and an insert evicts at most one key to keep the invariant
that a chain starts at its home bucket. It compiles as C++ unchanged, so the comparison is one
translation unit and every lookup inlines; its buckets are raw `malloc` memory that is never
constructed, so the key has to be trivially copyable, which is why this is an integer-key
comparison. The adapter was cross-checked against this map over 400000 mixed operations under
ASan/UBSan first, and that caught a real mapping error: Verstable's `_insert` *replaces* an existing
value, like `insert_or_assign`, so the honest counterpart of `try_emplace` is `_get_or_insert`.

Every map given this map's wyhash, five sizes per octave and the geomean, because 0.9 against
boost's 0.875 against sixteen slots per group is three maps that double at three different sizes.
Time relative to this map, below 1.00 meaning faster than it:

| workload | verst 1K | boost 1K | verst 32K | boost 32K | verst 500K | boost 500K |
|---|---|---|---|---|---|---|
| build from empty | 1.30 | 1.51 | **3.24** | 1.73 | 2.75 | 1.64 |
| find, all hits | 1.50 | 0.88 | 1.09 | 0.79 | **0.75** | 0.75 |
| find, all misses | 2.48 | 1.05 | 2.04 | 0.90 | 0.97 | 0.76 |
| find, 50% hits | 1.84 | 0.99 | 1.64 | 0.88 | 0.85 | 0.77 |
| iterate | 15.4 | 11.8 | 10.6 | 9.0 | 5.9 | 6.4 |
| churn at a fixed size | 1.71 | 0.83 | 1.25 | 0.68 | 0.62 | 0.49 |
| insert and erase | 1.81 | 0.92 | 1.31 | 0.75 | 0.67 | 0.60 |

In cache it loses to both maps on everything; out of cache it converges on boost, catching it
exactly on hits at half a million entries. gcc agrees on the ranking at every workload (verst
against this map at 32K: 2.08, 1.20, 2.08, 1.65, 7.96, 1.21, 1.24), so it is not clang layout. Its
own integer hash -- a three-op xorshift-multiply-xorshift -- costs it a further **3-14%** against
being handed this wyhash, largest in cache, which is the opposite sign from boost, whose own integer
hash is 1-7% *faster*.

**Why, and it is a mechanism this file keeps arriving at from new directions.** One map per binary,
30M lookups at 50000 entries:

| | instructions | cycles | branch misses | L1 misses |
|---|---|---|---|---|
| miss, this map | 44.1 | 19.0 | 0.107 | 3.24 |
| miss, boost | 45.1 | 19.1 | 0.162 | 1.89 |
| miss, Verstable | **36.7** | **38.0** | **0.812** | 1.96 |
| hit, this map | 50.5 | 29.1 | 0.065 | 4.22 |
| hit, Verstable | 49.0 | 33.2 | 0.418 | 3.26 |

**A Verstable miss executes 17% fewer instructions than this map's and takes twice the cycles.** The
design delivers exactly what it advertises -- fewest instructions, fewest cache lines touched -- and
hands all of it back at the branch predictor, because "is my home bucket a chain head, and how long
is the chain" is a data-dependent decision on every lookup where a group compare is not. At load 0.9
about 59% of misses land on a chain head and have to walk it. That is the
robin-hood-against-group-probe result again, reached by a completely different design.

The build gap is the insert path. At 200000 entries, ns per element: reserved inserts 6.84 for this
map, 3.80 for boost, 11.32 for Verstable; from empty 9.64, 12.89 and **26.49**, with branch misses
per element 0.132, 0.312 and **2.398**. Growth costs Verstable 143 instructions and 79 cycles per
element against this map's 44 and 12, because a rehash re-runs the whole insert for every key:
`find_first_empty` quadratic-probes for a free slot, `find_insert_location_in_chain` walks the chain
to keep it ordered by displacement, and an occupied home bucket calls `evict`, which re-hashes the
occupant and walks *its* chain. "Only moves one existing key" is a statement about moves and says
nothing about probing.

Memory, bytes per entry with an 8 byte value, octave geomean: this map 32.6-33.9, boost 27.7-29.2,
**Verstable 27.1-28.6** -- the leanest of the three, at 18 bytes per slot against boost's 16 at a
lower maximum load -- and flat across churn for all three, which is the check that all three are
genuinely tombstone-free. At a 64 byte value it is a flat map and behaves like one on a build (84.9
ns per element against this map's 55.5, boost 87.1) while iterating 1.6x better than boost (2.84
against 4.49, this map 1.10), because the two byte metadata array finds a sparse table's occupied
buckets without touching the buckets themselves.

The one transferable idea in it -- the exact in-home-bucket test -- is measured and rejected in the
counter section above. What makes Verstable competitive out of cache is not that: it is key and
value inline behind no value index, and one allocation rather than two, which at a million entries
is 0.13 dTLB misses per miss against this map's 0.77. That is the dense design's known structural
cost and not a new idea.

**Read boost's `unordered_flat_map` again after it turned out to have the probe bound this map was
missing** (2026-09-06). Three things came back, in descending order of worth:

- **MSVC had no prefetch at all.** `ANKERL_UNORDERED_DENSE_PREFETCH` was `__builtin_prefetch` for
  gcc and clang and `static_cast<void>` for everything else, so the index-line prefetch that is
  measured at 3 cycles off every hit was silently absent on a whole compiler. boost spells it
  `_mm_prefetch(p, _MM_HINT_T0)` on MSVC x86-64 and `__prefetch` on MSVC ARM64. Taken. Not
  measurable here, since none of the benchmarking runs on MSVC; it is a gap closed by construction
  rather than a win demonstrated.
- **boost tunes the prefetch per architecture** and says so in a comment: "ARM architectures get a
  higher speedup when around the first half of the element slots in a group are prefetched, whereas
  for Intel just the first cache line is best." This map issues the same two or three prefetches
  everywhere. Now that `bench.yml` exists that is a measurable question rather than a guess, and
  it has not been asked.
- **boost has an opt-in statistics facility** (`BOOST_UNORDERED_ENABLE_STATS`,
  `cumulative_stats.hpp`) that keeps running mean and variance of probe lengths and comparisons per
  lookup with Welford's algorithm. Every probe-length number in this file was produced by hand
  editing a copy of the header instead. A built-in equivalent would make the measurements this
  project keeps needing repeatable, and is the one idea here that is a feature rather than a fix.

Two details looked at and deliberately not taken. boost reserves *two* metadata values, 0 for empty
and 1 for a sentinel that ends iteration, and remaps hashes 0 and 1 to 8 and 9; this map reserves
only 0 and so has one more usable fingerprint, because it iterates the value vector and needs no
sentinel. And boost's 16 byte group is 15 fingerprints plus the overflow byte, read with an
*aligned* load and masked with `& 0x7FFF`; the 24 byte group here holds 16 fingerprints and eight
counters and is read unaligned, which the layout sweep measured as free.

Read and found to have nothing to transfer, with the reason in each case:

- **folly F14** is the closest relative, and its `outboundOverflowCount_` is this map's overflow
  counter exactly -- saturating, decremented on erase, used to stop a miss. Arrived at
  independently. One difference favours this map: F14 keeps *one* counter per 14 slot chunk where
  this keeps eight per 16 slot group, split by fingerprint class, so a miss here stops sooner.
- **abseil**'s probe sequence is the same triangular one, `(i^2+i)/2` over a power-of-two number of
  groups; its `next()` adds `Width` per step where `next_group()` adds one group, which is the same
  progression written differently. Its newer small-object optimization holds one element without
  allocating; this map already allocates nothing until the first insert, and the score has no
  workload of tiny maps, so it was not pursued.
- **bytell** puts a chain-head bit and a 7 bit index into a 126 entry jump-distance table in one
  byte per slot, so chains are linked lists with one byte links and the first sixteen distances are
  0..15 to keep short chains inside a block. It buys the ability to *skip* groups, and a lookup here
  visits 1.03 groups fresh and 1.27 churned. There is nothing to skip.
- **Verstable** packs a 4 bit hash fragment, an in-home-bucket bit and an 11 bit quadratic
  displacement into one 16 bit word per bucket. Same conclusion, and it confirms a detail: it takes
  the fragment from the *high* bits because the bucket comes from the low ones, which is the same
  independence this map gets by taking the group from the top of the hash and the fingerprint from
  the bottom. Benchmarked on 2026-09-07 rather than only read -- see the entry above, which
  supersedes this bullet -- and its one transferable idea, the exact in-home-bucket test, is
  measured and rejected in the counter section.
- **tsl::hopscotch_map** keeps a per-bucket bitmap of which of the next N buckets hold keys
  belonging here. It is positional where the counters are numeric, but it is *coarser* -- one
  bitmap per bucket against eight counters per group -- and it maintains its invariant by moving
  elements closer to home, which is the work this design exists to avoid.
- **Go's map** evacuates one bucket per operation instead of rehashing at once. That trades total
  throughput for tail latency, which is a different goal from the one the score measures, and it is
  a redesign of growth rather than a transfer. Not attempted.

Where the map stands against others on the score, same hash for all, measured the same day:
excluding the three iteration workloads, `boost::unordered_flat_map` is level (0.96) and **ahead on
lookups alone by 11%**; `emilib` is 12% behind, `emhash7` 20%, `emhash8` 27%, `emhash5` 28%. With
iteration included this map leads all of them, because only `emhash8` is dense as well and the rest
lose 3-10x there. The standing weakness is the same one this file has always named: building.

**Every boost comparison in this file that predates 2026-09-07 was taken at one size, and five of
them cross 1.00 when the octave is averaged instead.** Re-run with the octave sweep, one binary,
`main` against the working tree against boost, the same 20 workloads at one size and at five sizes
per octave. Above 1.00 means this map is faster.

| | vs main | vs boost | vs boost, no iteration |
|---|---|---|---|
| one size | 1.242 | 1.586 | 1.144 |
| octave geomean | 1.208 | 1.502 | **1.066** |

The `vs main` column barely moves and no workload in it changes sign; the boost column moves a lot
and five workloads do:

| workload | boost, one size | boost, octave |
|---|---|---|
| `churn64` | 1.191 | **0.776** |
| `churnbig` | 1.237 | **0.797** |
| `churnstr` | 1.087 | **0.866** |
| `rmiss64` | 1.112 | **0.916** |
| `iebig` | 0.984 | 1.002 |
| `buildstr` | 1.544 | 1.993 |
| `buildbig` | 1.928 | 1.775 |

**The reason the two columns behave differently is the whole point.** This map and `main` have
power-of-two bucket counts and the same maximum load factor, so they double at the *same* sizes:
their sawtooths are in phase and cancel out of the ratio, which is why every same-family paired A/B
in this file is sound however it was sampled. boost's bucket counts are not powers of two (1966079
at a million entries) and its maximum load is 0.875, so it is out of phase, and a single size reads
one map near the top of its cycle against the other wherever its own cycle happened to be.

So **"this map is ahead of boost on churn at a fixed size" is a single-size artefact** -- it is
stated in the table just below (`churn at a fixed size` 1.16 and 1.21) and again in the "nothing
measured a table that only churns" section above it (`churn64` 1.32x). Averaged over an octave boost
is 1.15-1.29x ahead on churn for all three value types, and this map's remaining lead over boost
outside iteration is 1.066 rather than 1.144. The lead over `main` is unaffected. **Every boost
ratio anywhere in this file and in `scripts/ab/README.md` that is not explicitly labelled as an
octave geomean is a point measurement**; re-take it with `run.sh` at its default `-p 5` before
quoting it. The charts sections of both files already summarise by octave and are not affected.

**That last sentence stopped being true on 2026-09-05.** Re-measured against
`boost::unordered_flat_map` after the rehash fix, same hash, 12 paired epochs, boost's time over
this map's:

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

Building is now a *win* rather than the weakness -- `build64` 1.39x and 1.64x, `buildbig` 2.09x on
both compilers -- because the store-to-load chain in the rehash was what held it back, not the
design. What is left of boost's advantage is exactly one thing, fresh-table lookups, and it is
worth stating plainly: boost is **10-13% faster on a hit** (`rhit64` 0.80, `findbig` 0.87) and this
map is faster on a miss (`rmiss64` 1.12 under clang). That is the value-index indirection, one more
dependent load than a flat map needs, and it is the price of the dense value vector -- which is the
same property that pays 3.9-5.8x on iteration and 2.1x on a 64 byte build. Memory for a million
entries, re-measured 2026-09-06 with a counting allocator and again with a replaced global
`operator new`, the two agreeing exactly: **27.0 MB against boost's 32.0** steady with an 8 byte
value, and 113.5 against 205.5 peak with a 64 byte one. The figures this line used to carry -- 39.5
against 67.9, and 144.5 against 209.0 -- do not reproduce under either method, and the boost steady
one cannot be right by arithmetic: a million entries in 1966079 slots of a 16 byte `value_type` is
31.5 MB, which is what both methods return.

**Those memory figures are a point on the sawtooth, and the octave says something else** (2026-09-07,
found while measuring Verstable's footprint). Bytes per entry with a 64 byte value, this map against
boost: 87.0 against 143.7 at a million entries, which is the 1.65x above -- and **135.4 against
119.7 at 1.2M and 108.4 against 95.8 at 1.5M, where boost is 12% ahead**. A million is near this
map's best point, the value vector's capacity overhanging by 4.9%, and near boost's worst, 1966079
buckets for a million keys being load 0.51. Averaged over an octave the two are a wash at a 64 byte
value (118.3 against 118.5 at 200000) and boost is ahead at an 8 byte one (32.6 against 29.2),
because the dense value vector's doubling overhang is a cost a point measurement can miss entirely.
Every one of these numbers is true; only the octave ones are a summary. The rule the charts section
states for time -- summarise across an octave, never at a chosen load -- applies to memory, and the
lines above predate it.

**A miss had no bound, and eight chosen keys made it loop forever** (found 2026-09-05, in the
review before release). The probe stopped only at a group whose counter for the key's class was
zero, on the argument that exact counters put a zero right after the furthest entry of that class.
The argument is wrong: a counter counts entries that overflowed past its group on *their* probe
sequences, not on the one being walked. Fill a group, send one key of class 1 past it, erase the
fillers -- the passer stays, so the counter stays -- and do that for every group: eight live keys,
every class-1 counter positive, and `contains()` on an absent class-1 key never returns. Any hash
the caller controls reaches it, and the default hash with attacker-chosen keys does too, since only
the top few bits and the low byte need steering. `indivi::flat_umap`, where the counters came from,
has the same hole (`find_impl` loops on `gIndex <= mGMask`, which the mask makes always true); the
same eight keys hang it. The fix is `|| delta == m_group_mask` on the miss exit: a key that exists
was placed within one cycle of its sequence, so a walk that has seen every group can stop. By
mechanism it is free -- per lookup on a 200k table, 83.6 to 82.7 instructions on a hit, 69.5 to
67.6 on a miss, cycles and mispredictions unchanged -- and the paired score read 0.99 with three
workloads at 0.95 beside a 1.16 on `hashstr`, which never touches the map, so that run's layout
moved. `test/unit/probe_termination.cpp` builds both this table and a saturated counter with the
identity hash; the corpus fuzzers, all on wyhash, could not have reached either. The bound has a
second effect worth knowing: it converts a *missing or wrong-home* erase decrement from a hang into
a silent slowdown. That fault used to be caught loudly -- the counters only grew, a miss found no
zero, the suite hung -- and now the miss stops at the end of the array and the table stays correct,
just slower. That left the erase decrement uncovered by any correctness test for a while -- mutating
it away SURVIVED all 771 cases -- which is closed since 2026-09-07 by `test/unit/erase_uncounts.cpp`.
The way to test anything in this family is to measure the lengthening rather than an answer: the map
is given a counting `KeyEqual`, and a table that reached its contents by erasing a run of entries
that had overflowed one group into the next has to compare a miss exactly as often as a table built
from the survivors directly. Three cases, covering the erase path, the rehash that rebuilds the
counters, and the two together. That is the deliberate trade of making the map robust
to a hostile hash: a hang is loud, degradation is quiet, and the map has to prefer the quiet one.

**Mutation triage after the bound** (2026-09-05, `invariants.txt` and `erase-path.txt` re-run,
plus a `bitwise,deletions` sweep of the index functions, lines 1380-1560). `invariants.txt` is 45
of 46 caught after a test was added for the one real gap the sweep found: copying an *emptied but
grown* table by assignment into a grown target left the copy at the source's shift, so its first
insert allocated the large array instead of the smallest (2048 buckets against 64) -- observable,
and nothing checked it, now `copying_an_emptied_table_starts_from_the_smallest_array` in
`lazy_bucket_allocation.cpp`. Re-run 2026-09-07 it is **45 of 46 with one survivor**, 31 of them
caught by a test rather than by the compiler or a hang. Five of its blocks had gone stale in the
meantime -- the merged block renamed `index[slot]` to `group.m_index[lane]` and `m_buckets.index()`
to `index_at()`, and `move_home` moved into `emplace` -- and because the tool refuses to run a file
with any block that does not apply, **none of the other 41 were being checked either**. Re-deriving
them is a job with a trap in it: three of the obvious rewrites are rejected by the compiler rather
than by a test (`if (true)` leaves the `key` parameter unused; `m_equal(key, key)` trips gcc's
`-Warray-compare` on the array-keyed map in `transparent.cpp`; dropping a repoint leaves its two
locals unused), and a `compiler` verdict means the question was never asked. A fourth rewrite
computed the same index as the correct code and was an equivalent mutant wearing a bug's name. Check
the verdict, not just that the block applies. The one remaining survivor is equivalent: the moved-from mask
(every find and erase checks `empty()` before it could read the mask, and a moved-from table is
empty) and the erase decrement just above. The sweep's sixteen survivors are all one of three
kinds: deletions and bitwise rewrites inside the SWAR fallback, which an SSE2 build does not compile
at all; prefetch deletions, which are semantic no-ops; and the erase decrement again. None is a test
hole. `erase-path.txt` is 5 of 6, the sixth being that same decrement.

**Indexing the value container in the rehash cost clang a memory latency per element** (2026-09-05,
from comparing the two compilers' absolute times on the branch rather than their ratios to main).
`build64` was the one large branch-specific compiler gap left: 3.36 ms under clang against 1.97 under
gcc, where main's own gcc/clang ratio on the same workload is 1.04. Splitting a 200000 element build
into inserts into a reserved table and the growth on top of them says where it lived -- clang was
**1.44x faster** on the reserved inserts (6.30 ns against 9.08) and **4.9x slower** on growth (10.43
ns per insert against 2.15). So it was never the insert path; it was `fill_buckets_from_values`.

The loop read `m_values[value_idx]`. Placing an entry stores a fingerprint, a `std::uint8_t` store
may alias any object at all, and the container's own data pointer is such an object -- so after
every placement an indexed read has to load that pointer back out of the container before it can
form the address of the next key. The random group access that follows cannot start until that
resolves, so the chain costs one memory latency per element and the placements, which are
independent, run one at a time. Walking with an iterator keeps the address in a register: **growth
10.43 ns per insert to 2.74, the whole build 16.72 to 8.96**, i.e. clang's build is now faster than
gcc's rather than 1.7x slower. Paired on the score, clang `build64` **1.80**, `buildbig` **1.61**,
geomean **1.076**, everything else within noise; gcc 1.03 and 1.03, geomean 1.006, since it
disambiguated on its own.

What did *not* work, all measured: hoisting `m_buckets.index()` out of `place_group` (the same
store-to-load shape, but that pointer was already in a register), `__restrict__` on the group and
index pointers (says nothing about the container's pointer, which is the one being reloaded), and
every combination of forcing `place_group` and `fill_buckets_from_values` inline or out of line
(a 3x2 matrix, all within 2%). The lesson is the one the old rehash section already stated for a
different loop -- what matters is what sits on the path to an address -- and the new part is that a
byte-sized store is enough to put a container's own bookkeeping there.

**The rest of the header was then audited for the same shape, and it is the only instance.** Every
site that reads a container by index after a store: `replace()` has the identical loop and is
*correct* as written, because that loop moves elements and pops the back, so the pointer and the
size genuinely change and the reload is required; `erase_if` recomputes `begin()` around an
`erase()` that moves elements, likewise; `probe` reads `m_values` to compare keys but stores
nothing, so nothing serialises inside one probe; and `place_group` still asks for
`m_buckets.index()` after writing a fingerprint, re-measured once the big effect was gone and worth
nothing either way (reserved inserts 6.92 ns against 5.84, growth 2.17 against 2.38, mixed and
within noise). What made the rehash unique is that the reload was the *only* work between
iterations, so it landed directly on the address path of the next random group access; everywhere
else there is enough independent work to hide it.

Checking the two compilers against each other is what found it, and it has a blind spot worth
naming: it can only see a loop where one compiler disambiguates and the other does not. A loop both
serialise looks normal. After the fix no workload shows a branch-specific compiler gap any more --
`build64` is 1.82 ms under clang against 1.88 under gcc, where it was 3.36 against 1.97 -- and the
only large remaining difference, integer iteration at 1.65x slower under gcc, is shared with main
and so is not about this index at all.

**gcc left `probe` out of line, and forcing it inline is the largest single gcc gain on the branch**
(2026-09-05). Found from the full score without SSE2 under gcc, where random integer misses read
0.66 of main while a standalone loop had gcc's SWAR miss *faster* than clang's. `perf` on the
harness binary put 59% of the time in `table::probe<unsigned long>` as its own symbol, called from
`find_all`, where main's lookup was fully inlined: in a large translation unit gcc's unit-growth
budget runs out and the probe, bigger with the SWAR match, is the function it stops inlining. The
symbol exists in the SSE2 binary as well. `ANKERL_UNORDERED_DENSE_FORCEINLINE` on `probe`, paired
against the commit before it: **gcc without SSE2** `rmiss64` 0.66 to 1.23 against main, `rhit64`
1.16 to 1.50, `churn64` 1.19 to 1.48; **gcc with SSE2** `churn64` 1.32, `rhitstr` 1.22, `build64`
1.42; **clang** 1.00 on every workload, with and without SSE2, since it inlined it already. The
whole design assumes the probe is inlined -- the prefetch, the hoisted pointers and the early exit
only pay inside the caller -- so this is the attribute saying what the code already meant. The
full score against main under gcc went from 1.149 to **1.244** with SSE2 (`build64` 1.97,
`buildbig` 1.68) and from 1.097 to **1.165** without, and the gcc string lookups that were the one
workload family behind main are now ahead of it: `rhitstr` 1.13, `rmissstr` 1.05, `findstr` 1.10.

**The gcc string-lookup gap, explained and mostly closed** (2026-09-05). Under gcc the branch's
string lookups measured 0.88-0.91 of main, the one workload family it lost, and the instruction
counts say why: per string hit, main 200 instructions and the branch 224 under gcc, against 235
and 239 under clang, with branch misses identical at 1.8. gcc compiles main's robin hood probe
unusually tightly, and on string keys there are no probe mispredictions for the group index to win
back -- the 1.8 are the hash's length dispatch and `memcmp` -- so the group probe's extra
instructions show undiluted. The assembly is the same shape CLAUDE.md recorded for the old SSE2
probe: gcc spills the loop state, the broadcast fingerprint among it, around the `memcmp` call,
and that plus the prefetches, movemask, tzcnt and the second array are the two dozen. Of those,
five were fixable: building the fingerprint word was an and, a compare, a shift, an or and a
multiply on the critical path of every probe, placement and erase, and a 256-entry table of the
finished words (which the prototype had and the header had lost) makes it one L1 load. Paired,
both compilers: integer misses 1.05-1.06x, `findbig` 1.03x clang and **1.14x gcc**, `rhit64`
1.02x gcc, `ie64` 1.03x gcc, strings 1.00-1.01. Kept. The rest of the string gap under gcc turned out to be the probe not being inlined at all
in that binary -- the entry above this one -- and is closed.

**The whole matrix, run on CI rather than on the desktop** (2026-09-06, `bench.yml`, nine jobs,
12 paired epochs, against `origin/main` and against boost where boost is installable). Geomean of
the fifteen scored workloads:

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

Ahead of main on every machine and ahead of boost on every machine that has boost, which is the
first time either has been said about anything but one desktop. The desktop's own numbers
(1.21 clang, 1.25 gcc) sit inside this spread, so it was not flattering itself, but the spread is
wider than the run-to-run noise on a quiet machine and these runners are neither quiet nor
identical -- read a column as "which side wins and roughly by how much", not to three digits.

**Only the ARM pair measures what the vector compare is worth**, and reading the x86 pair that way
is a mistake worth naming, because main is in the comparison too. On ARM main has no vector path at
all, so it is the same scalar code in both rows and the difference is entirely this map's: arm gcc
falls from 1.22 to 1.04 without NEON and its lookups go with it (`rhit64` 1.53 to 1.02, `rmiss64`
1.68 to 1.06). On x86 `ANKERL_UNORDERED_DENSE_HAS_SSE2=0` also takes away main's four-bucket SSE2
probe and its vector shifts, so both sides are handicapped and the ratio can move either way --
which is why linux gcc reads *higher* without SSE2 (1.19) than with it (1.14). That number says
main lost more than this map did, not that turning SSE2 off is good.

**Windows had never been measured at all**, and it is the mildest machine of the nine: ahead of
main everywhere except `find64` at 0.98, but with builds at 1.25 where linux clang reads 2.13,
which is what a different allocator does to a workload that grows -- `tame_allocator()` is a glibc
trick and a no-op there.

**The one outlier chased down, and it is the CPU rather than the code**: linux gcc SSE2 iterates at
**0.77** of main, the only figure anywhere below 0.9. Not noise -- err% 0.0, a 76.9-78.6 interval,
and 0.774 then 0.773 on two independent runs with fresh runners. Not the compiler either: built
with the runner's exact package (`Ubuntu 13.3.0-6ubuntu2~24.04.1`) in a container on this desktop,
the same binary reads **1.35**, and upstream gcc 13.4 reads 1.01. Same compiler, same flags, same
source, so the same assembly; the machines are an EPYC 7763 on the runner against a Ryzen 7950X
here, and the answer swings 1.75x between them.

What makes `it64` able to do that is that it is the workload least about the index: 5000 inserts
and 5000 erases against 25 million element visits, so it is almost entirely a vectorisable sum over
the value vector, which is the same `std::vector<std::pair<K, V>>` in both maps. Whichever way the
two loops happen to land, one of them wins by a lot on a given microarchitecture, and nothing about
the group index is being measured. The lesson for reading the matrix is narrow and useful: `it64`
is the column to distrust across machines, and a difference there is not evidence about the index.
Every other workload agrees between the desktop and both runner architectures.

**NEON closed the ARM lookup gap, and it was the whole gap** (2026-09-05). The SWAR row below is
what prompted it: on a Neoverse N2 the branch was 1.11x main overall but *behind* on every lookup,
because the word-at-a-time compare replaces a robin hood probe that was never vectorised on ARM and
so lost nothing to begin with. With `vceqq_u8` and a narrowing shift the same runner gives, main's
time over the branch's, SWAR first and NEON second: `rhit64` 0.91 to **1.48**, `rmiss64` 1.01 to
**1.62**, `find64` 1.03 to 1.26, `findbig` 0.91 to 1.18, `rhitstr` 1.02 to 1.10. Nothing is behind
main any more except `findstr` at 0.97, where the hash rather than the probe is the cost. The score
goes 1.112 to **1.244** and without iteration 1.140 to **1.312**, which puts ARM ahead of where x86
sat before the rehash fix and level with it now. Builds and churn gained too (`build64` 1.67 to
2.08, `churn64` 1.20 to 1.43), since both place through the same compare.

NEON has no movemask and the cheap stand-in does not give the same mask shape: the sixteen answers
land one nibble apart in a 64 bit word, so the mask type and a lane stride are named once and
`first_lane()` divides by the stride. Testing for a match, taking the lowest and clearing it with
`m & (m - 1)` are then written once for all three backends. Guarded to little endian AArch64 --
the mask reads the comparison as one word, and 32 bit ARM has no horizontal ops -- with SWAR, which
is correct everywhere, behind both.

**Before NEON: on ARM the branch was 1.11x main, the same overall as on x86, split the opposite way** (2026-09-05,
`.github/workflows/bench.yml`: the paired harness on a GitHub `ubuntu-24.04-arm` runner, Neoverse
N2, 4 cores, clang 18, 12 interleaved epochs, `origin/main` against the branch, so main's scalar
robin hood probe against the branch's SWAR fingerprint compare -- neither has a vector path there).
Geomean of the scored fifteen **1.112**, without iteration 1.14. Builds are far ahead (`build64`
1.67, `buildbig` 1.54, `buildstr` 1.25) and churn is ahead (`churn64` 1.20, `churnstr` 1.09), but
**lookups are behind main**: `rhit64` 0.914, `findbig` 0.912, `rmissstr` 0.939, `findstr` 0.967,
`find64` 1.03. That is the SWAR match, about 36 instructions for two words where SSE2 does sixteen
bytes in three, paid on every probe, against a robin hood probe that on ARM was never vectorised
either and so lost nothing. It is the number a NEON match is for: `vceqq_u8` plus a narrowing
shift is a handful of instructions, and indivi has the port. Push any branch to `bench` to re-measure this and every other machine in CI; a
`workflow_dispatch` would need the file on main first.

**`ie64` ties boost while executing 58% more instructions, and the counts say why** (2026-09-05,
the workload run on each map alone under `perf stat`, net of its own rng and key scrambling,
`/tmp/ie_count.cpp`):

| per operation | this map | boost |
|---|---|---|
| instructions | 89 | 56 |
| cycles | 43.8 | 43.5 |
| branch mispredictions | 0.61 | 0.67 |
| L1 data misses | 1.7 | 1.2 |
| L2 misses | ~0 | ~0 |

Neither map is instruction-bound: boost retires 1.3 per cycle, this map 2.0, on a core that can do
four. What both wait on is the workload. Every `operator[]` and every `erase` in `ie64` is a coin
flip on whether the key is present -- measured 49.9% hits for both -- so each operation costs about
half a misprediction at ~16 cycles whatever the map, and then one chain of dependent loads that at
10k entries sit in L2: hash, group metadata at a random address, the element to compare. Boost's
chain is metadata then a 16 byte slot; this map's is metadata, index, value, but the index line is
prefetched from the group address before the fingerprints arrive, so the extra hop mostly overlaps.
The 33 extra instructions -- vector append and pop, the counter walks, the second probe on a
successful erase -- run in the shadow of those stalls with issue slots to spare. Boost pays slightly
more in mispredictions because its overflow bits stay set until the next rehash and the table
churns between growths, so its misses walk one group further than a fresh table's.

The same numbers say where the tie ends: on a table that does not fit in cache, or a workload with
no coin flip, the memory chain dominates and boost's shorter one shows -- that is the 11% on
lookups. And nothing here is spare on the instruction side: a change that lengthens the dependent
chain costs at once, a change that only saves instructions on this workload is invisible.

## The robin hood index this replaced, and its dead ends

Everything below describes the index that was removed on 2026-09-05: a packed
distance-and-fingerprint field per bucket, a four-bucket SSE2 probe, and vector shifts on insert
and erase. None of it can be re-run against the current header. It is kept because the measurements
are real, the reasoning applies to anything that probes a flat array, and the same questions will
be asked of the group index.

## Optimization dead ends (verified with paired A/B runs; re-test before assuming they still hold)

The `bench_quick_overall_udm` hot paths are close to machine limits. Ideas that consistently
**regressed** and were reverted:

- Force-inlining `wyhash::hash` into the map (icache/register pressure outweighs saved call overhead).
- A branchless `do_find` fast path for scalar keys (unconditional key compare + conditional-move result): the speculative value load doubles cache misses on the ~50% miss lookups.
- Explicit `__builtin_prefetch` of `m_values[bucket->m_value_idx]` in `do_find`, and computing the moved element's hash early + prefetching its home bucket in `do_erase`: out-of-order execution already hides these latencies.
- Replacing wyhash with rapidhash (v3, 2025): the wyhash implementation here is *faster* for inputs ≥ 24 bytes in both latency and throughput; rapidhash only won at ≤ 16 bytes, and that trick (two plain 8-byte reads instead of building `a`/`b` from four 4-byte reads) has been adopted.
- An AES-NI hash (a port of gxhash, compiled with `-maes`, no dispatch): 30% *slower* than this
  wyhash on 200 byte keys and 27-55% slower in the map. Its serial `aesenc` chain has worse latency,
  and latency is what the string loop pays for. Fewer uops do not help a chain.
- SIMD probe variants: an *aligned* group of four (1-4 lanes visible from home) left the "nothing
  decided, next group" branch random and won nothing; deciding hit-vs-miss with two branches
  (scalar home probe, then the vector for the rest) mispredicted *more* than one vector decision
  (0.90 vs 0.70 per lookup) although each branch is more biased; keeping the key comparison inside
  the vector loop made the compiler spill the xmm state around `bcmp` on every string hit.
- Storing the top 32 hash bits per value so an erase never re-hashes the moved element: only the
  successful half of the erases move one, so the bound is ~7% of `iestr`, and it measured 1.4%
  for 4 bytes per element and a second container to keep consistent.
- Caching the bucket data pointer in the shift loops (the compiler already hoists it).
- Two attempts at the rehash, measured against `build64` and `buildstr` (2026-09-02): skipping the
  `memset` in `clear_and_fill_buckets_from_values`, which is redundant because
  `allocate_buckets_from_shift` hands back a freshly zeroed vector (0.7% on `build64`, ~1% *worse*
  on `iestr`); and hashing eight elements ahead in the rehash loop and prefetching the bucket each
  will land in (nothing on `build64`, ~1% worse on `buildstr`). The redundant memset was removed
  on the group index on 2026-09-05 as a simplification, not a speedup: paired on the score it is
  within 1% everywhere, so the ~1% on `iestr` above was layout luck.
- Scalar attempts to take the branch out of `place_and_shift_up`. The robin hood shift asks "is this
  bucket occupied" once per bucket and the answer is a coin flip (73% of inserts shift nothing, 11%
  shift one), which cost 0.61 mispredictions per insert. Settling the first two buckets with
  conditional moves and one combined test does not help: written as `a == 0 || b == 0` it is still
  two branches, written as a bitwise or it is one branch that mispredicts just as often
  (0.608 per insert, ten more instructions). What did work is the vector version now in the
  header, which settles four buckets from one mask -- the same trade the probe made, and the same
  again for the shift down on erase. A two bucket vector version was also tried and lost to it
  (49.7 against 46.3 cycles per insert): its second slot is still a branch. Both vector shifts
  first read as ~1% *losses* on `ie64`, and that was the benchmark: its integer keys hashed to a
  lattice with no chains to shift (see "Where the time goes"). With honest keys they are 1.07x.
- Four attempts at the rehash loop (2026-09-03), and the reason they all failed. The loop in
  `clear_and_fill_buckets_from_values` costs **~21-26 cycles per element at every size from L1 to
  L3** (4.4 ns at 1000 elements, 5.4 ns at 200000): memory latency is already hidden, and what
  bounds it is an in-core chain through the bucket array -- each element's loads sit behind the
  previous element's stores. Two experiments pin that down. Adding 11 cycles of artificial latency
  between the probe load and the store address adds 17 cycles per element; reading four elements'
  home buckets before storing any of them is 1.5x faster while the array is L1-resident and nothing
  at 100000, where the score's rehashes run. Nothing that shortens the *data* side of the store
  moves it at all. So for this loop: anything that puts a load result on the path to the store
  address is a large loss, and anything that only saves instructions or mispredictions is
  invisible -- which is also why the earlier prefetch-eight-ahead and memset attempts found
  nothing. What was tried, against `HEAD` on the isolated loop and paired on `build64`:
  vectorizing `next_while_less` with the probe's four-lane window (`build64` **1.41x slower**,
  the rehash 2-3x per element even in L1: fewer mispredictions, 7% more instructions, and the
  store address now waits for the window load); the four-element batch above (neutral at scored
  sizes, worse at a million); refilling from the *old* bucket array in order so new homes arrive
  nearly sorted (**2x slower** at 100000-200000, with or without prefetching the keys: sorted
  arrival makes every element probe the bucket the previous one just wrote); and replacing the
  `tzcnt`-indexed blend tables of both vector shifts with a vector prefix-or (rehash neutral,
  erase 6% slower: the table loads were never on the chain). Do not read
  `ls_bad_status2.stli_other` as a cost here: `build` shows 1.5 of them per insert and `churn`
  1.3, on bucket-window loads that cannot overlap the previous insert's stores (odds ~3e-5), and
  shifting the stack of the same binary moves the count from 1.45 to 2.85 per insert while the
  cycles stay at 98.0 +- 0.5. Placing the bucket before appending the value, to put distance
  between those stores and the next probe's loads, cost 2-7 cycles per insert in every variant.

---

# Tooling, in the detail the rules were compressed from

`CLAUDE.md` carries the commands and the sharp edges of the mutation tool, the fuzzers and CI. What
follows is the longer version those were cut from: the operator kill rates, why the lanes and the
cgroup caps exist, what each fuzzing target is for, and the CI legs' own history. Kept because the
numbers in it were measured rather than assumed, and because the reasoning explains choices that
otherwise look arbitrary.

## Testing

Any change to `include/ankerl/unordered_dense.h` must pass the unit tests:

```sh
meson test -C builddir/clang_release unit --verbose
# or directly (runs all non-skipped tests):
./builddir/clang_release/test/udm-test
```

## Mutation testing

Coverage says a line ran. `scripts/mutate/mutate.py` says something would have noticed it
misbehaving: it breaks the header, rebuilds, runs the suite and asks whether anything went red.
What nothing notices is a hole in the tests. It never touches the working tree — every build
happens in a throwaway copy of the repo.

**The tool is two files, and one of them is shared with nanobench and oans.** `mutate_core.py` is
everything that is not about any one project — the lanes, the mutants, the baseline discipline, the
verdicts, the report — and both of those repositories hold a byte-identical copy of it. `mutate.py`
beside it is this project's adapter: where the header is, that meson configures the build, that the
binary is `udm-test`, that a lane needs `FUZZ_CORPUS_BASE_DIR`, and the measured constants behind
`--dry-run`. Roughly 1900 lines shared against 100 of adapter.

Three build systems and two test runners live in the core, and only one of each is exercised here:
meson + doctest. cmake is nanobench's, and `make` + minunit is oans's — a C project whose suite is
one `minunit` binary, which is why `Backend.build_argv` takes the whole argument namespace (make
remembers nothing, so `--make-arg CC=clang` has to ride on every build line) and why a `Harness`
that takes no filter arguments does not get offered the `--test-filter` flags at all. A flag
accepted and then ignored would run the whole suite while the fingerprint claimed otherwise.

That arrangement is what makes `scripts/test_mutate.py` worth its length: it covers the code *all
three* repositories run, including the cmake and make backends and the minunit harness this project
will never execute. So a change to the core is only part of a change — make it here, run that suite,
copy the file into the other two, and record the new hash in all three:

```sh
sha256sum scripts/mutate/mutate_core.py                    # write it into mutate_core.sha256
cp scripts/mutate/mutate_core.py ../nanobench/src/scripts/mutate/   # ... and into its .sha256 too
cp scripts/mutate/mutate_core.py ../oans/scripts/mutate/           # ... and that one's
```

`lint-mutate-core.py` fails if this copy has been edited without that hash moving with it, which is
the one failure vendoring introduces: a convenient local fix here leaves the other repositories
running something nothing tests. Comparing the `.sha256` files is how "are they in sync?" gets
answered; no lint in any of the repositories can see the others.

The everyday use is putting a *specific* bug back, which is the check that decides whether a new
test earns its place. Bugs worth keeping live in `scripts/mutate/bugs/`:

```sh
scripts/mutate/mutate.py --replace OLD NEW               # one, must match exactly once
scripts/mutate/mutate.py --bugs scripts/mutate/bugs/erase-path.txt
scripts/mutate/mutate.py --reverse HEAD                  # undo a fix, keep today's tests
```

A block whose replacement is *meant* to contain what it replaced — an inserted call, an early
return in front of code that stays — needs `<<< additive` on its fence. Without the flag such a
block is refused, because the code under test does not change and `caught` or `SURVIVED` would be a
verdict about nothing. That check came from woswoar, where three of them shipped in one session
before it existed; the one legitimate case in these two repositories is a nanobench bug that accepts
a `-` sign in front of a digit check that stays.

The other mode sweeps for holes nobody thought of, changing the header one place at a time. The two
modes compose, and a change is best asked both questions at once:

```sh
scripts/mutate/mutate.py --diff                          # whatever is uncommitted
scripts/mutate/mutate.py --diff HEAD~1                   # only what that change touched
scripts/mutate/mutate.py --lines 1278-1290,1400 --dry-run
scripts/mutate/mutate.py --bugs bugs.txt --lines 1278-1290 --reuse
```

`--diff` is the everyday mode and measures from the merge base, so a branch that has not caught up
with main does not sweep what main moved on without it.

`--dry-run` sizes a run, and reports a *range* rather than one number: the
per-mutant constants describe a mutant that **compiles**, and one the
`-fsyntax-only` pre-filter throws out costs about a tenth of that. The single
figure it used to print read high by an order of magnitude wherever most mutants
are invalid - measured, the `negation` sweep below was estimated at 11 minutes
and took **51 seconds**. Which end applies is decided by the operator and the
code, not by the machine, so it is knowable before running anything.

`--operators` picks what to change. The default is all of them, so a plain run asks every question
this knows how to ask — which is what you want from `--diff`, where the cost is proportional to the
lines you touched. Over the whole header that default is ~1600 mutants and something like an hour
and a half, so a full sweep is usually worth naming one operator instead. That is the reason they
are named at all.

Swept over the whole header at 4.9.1, they are not equally worth your time. Re-measure before
relying on these: they describe one header at one commit, and the kill rates move every time a
test is added.

| operator | mutants | time | killed | by a test | survivors to triage |
|---|---|---|---|---|---|
| `tokens` | 841 | ~47 min | not re-measured | | |
| `bitwise` | 76 | 4 min | **99%** | 87% | **1** |
| `deletions` | 665 | 14 min | 94% | 30% | 37 |
| `negation` | 24 | **51 s** | 100% | **0%** | 0 |

`negation` drops a logical `!`, and its row is the one worth reading twice: **every one of
its 24 mutants is rejected by the compiler, and not one reaches a test.** That is not a weak
operator, it is a fact about this header. Nearly every `!` here sits in type-level code -
`static_assert(!is_detected_v<...>)`, `enable_if_t<!is_map_v<Q> && ...>`, `if constexpr
(!std::is_trivially_destructible_v<T>)` - where dropping it makes the program *ill-formed*
rather than merely wrong. In oans, a C project, the same operator is 14 sites with 13 caught
and one real finding (the `!` excluding DELALLOC extents from a shared-byte count, which no
test held).

It stays in the default set anyway, and the reason is the 51 seconds: the `-fsyntax-only`
pre-filter rejects all 24 at about 2 s each rather than a rebuild each, so an operator that
is useless here is also nearly free here. Expect that unevenness between projects rather
than a uniform number - the same lesson the property tests taught, from the other side.

`bitwise` mutates `^` and `|`, which the token table leaves alone (`&` is three operators sharing a
spelling — bitwise and, address-of, and the reference declarator — and only a parser can tell them
apart). Few sites and a header made of masks and fingerprints is what makes it cheap and sharp. The
mechanism worth remembering rather than the number: a *surviving* bitwise mutant usually means the
two operands are provably disjoint, which is how the one survivor reads — `dist_inc | (hash &
fingerprint_mask)` turned into `^` is the same function, exactly as the
`static_assert(fingerprint_mask < dist_inc)` right above it guarantees.

`deletions` removes whole statements. Nearly every bug in `bugs/invariants.txt` is a form of "the
code forgot to do this", and none of those is one token. It costs *less* than the token sweep, since
half of them are rejected by the `-fsyntax-only` pre-filter rather than costing a rebuild.

Reordering is the operator that is *not* here — it was written, measured and removed, so there is
nothing in the tree to go and look at. Swapping two adjacent statements killed 45% of what it
generated and left a hundred survivors, essentially all of them two statements that never touched
the same state — member-copy chains, the run of `HASH_STATICCAST`
macros, blocks of declarations. Triaging every one of them produced no test worth writing. Reaching
the orderings in `bugs/invariants.txt` needs to move a statement *out of its enclosing block*, which
adjacent-swapping cannot do, so that operator is worth building only alongside something that can.

Mutants that could not have an effect are not generated: comments, string literals and preprocessor
lines are not code, and `std::enable_if_t<..., bool> = true>` is the SFINAE idiom whose value is
never read. A mutant in a branch this configuration does not compile is dropped once the lanes
exist, and the run says which lines those were.

A mutant costs one full rebuild of the test binary — all ~90 translation units include the header,
so there is no incremental mutant build and ccache cannot help either. That is ~100 CPU-seconds of
compiling against 3 of running the suite, which is why the lanes default to one ninja job each and
a single named bug instead gets the whole machine. Budget roughly a minute for a handful of
mutants and an hour for a sweep of a whole function.

Verdicts are `caught` (a test failed — the number worth moving), `compiler` (the build refused it),
`hang`, `oom` and `survived`. Without a sanitizer, a mutant that reads one slot past a bucket comes
back `survived` however good the tests are, so re-run anything surprising with
`--meson-arg=-Db_sanitize=address,undefined` before believing it.

Each lane runs inside a cgroup with a memory cap (`systemd-run --user --scope`), because a mutated
growth policy turns an insert into a request for more memory than the machine has. Capped, that is
one `oom` verdict; uncapped, the kernel picks the victim and it is as likely to be another lane as
the mutant that caused it. `--memory-limit` overrides the default, which is the smaller of a lane's
share of the machine and 1 GiB per ninja job; a build is never capped below what its jobs need. On
a machine with no user scope to be had — another init, no session bus, an undelegated container —
the run says so up front rather than pretending.

The lanes are the other half of the same problem: ~90 MB each in a workdir defaulting to `/tmp`,
which is a tmpfs on most current distributions, so `--lanes` buys memory as much as parallelism.
The run prints how much room it is about to take and whether that room is RAM, and refuses before
copying rather than part way through. Core dumps are disabled for the same reason — a crashing
mutant is an ordinary verdict, and each one would leave ~30 MB in a lane about to be deleted.

Lanes are configured `--unity=on`, which is 2.5x less compiling for the same work — measured, one
mutant rebuild of the whole suite: 67 CPU-seconds separately, 27 merged. The usual objection to
unity builds (touching one file recompiles its whole chunk) cannot apply to a mutant, which
recompiles every file anyway. `--meson-arg=--unity=off` turns it off.

One flaky case is enough to stop a run: the baseline refuses to score until the suite is green
twice, which is the point of it. `--exclude-filter NAME` (doctest's `-tce=`) is the honest way past
that — it names what was skipped in the fingerprint, where lowering `--baseline-runs` would not.

`scripts/test_mutate.py` covers the half of the tool that decides what a verdict *means*, and runs
in CI. It is hermetic: no compiler, no meson, no lanes, no cgroups. Since the core became shared it
also covers the other repositories' halves — the cmake and make backends, the minunit harness, the
project seam, the root-only ignore patterns — because a backend tested only where it is used is
exactly as untested as it was before. The make backend gets the most of that attention, because it
is the one with no configure step: its compilation database is read out of `make --dry-run`, and
every way that reading can be wrong is silent. A line mistaken for a compile has the pre-filter
checking the wrong thing; a real compile missed turns the filter off and costs a full rebuild per
mutant; and a `-l` left on a line that a syntax check cannot link makes every mutant come back
`compiler` under `-Werror`, which is the flattering direction.

## Fuzzing

The `fuzz` test suite replays the committed corpora in `data/fuzz/<target>` on every test run, which
only ever re-finds what has already been found. The libFuzzer targets are what go looking. They are
clang only and not built by default:

**`fuzz_group_index` is the one that can reach the index's own structure, and the reason it exists
is worth keeping.** The other targets already hash with an identity over the whole 64 bit key, so
steerability was never what they lacked -- it is *structure*. Filling a group and then emptying it
again means sixteen keys agreeing in their top bits followed by sixteen erases of those same keys,
which a random key stream does not produce, and that is how an unbounded miss survived all of them
plus 767 unit tests. This target splits a key into three bytes the fuzzer chooses separately --
group, identity, fingerprint -- and gives it fill-a-group and erase-a-run as single operations, so
"fill this group, send one key of this class past it, take the fillers back out" is a handful of
mutations rather than a coincidence. Validated by removing the probe's bound and re-running: it
comes back as a libFuzzer timeout inside `probe` within seconds, *from the seed corpus alone*.
`data/fuzz/fuzz_group_index/cb8d5c38...` is that input, kept as a regression seed; note that
coverage minimization drops it, because against the fixed header it is no longer distinctive.

```sh
CXX=clang++ meson setup builddir/fuzz
ninja -C builddir/fuzz test/fuzz_api          # or fuzz_insert_erase, fuzz_replace_map, fuzz_string
./builddir/fuzz/test/fuzz_api -max_total_time=60 scratch-dir data/fuzz/fuzz_api
```

libFuzzer writes new inputs into the *first* corpus directory it is given, so keep `data/fuzz/...`
second and it stays read-only. Passing it alone — `./test/fuzz_api data/fuzz/fuzz_api` — quietly
fills the committed corpus with hundreds of generated files; to just replay it, run the `fuzz` test
suite (`./builddir/dev/test/udm-test -ts=fuzz`), which is what CI does. `scripts/fuzz_run.sh <target>` drives one across all cores, and
`scripts/fuzz_merge.sh <target>` merges a scratch corpus back down to the inputs that add coverage.
One body serves both modes: `FUZZ_TEST_CASE` in `test/fuzz/run.h` expands to the doctest replay case
normally, and to libFuzzer's entry point under `-DFUZZ`.

The same body also builds under AFL++, because `afl-clang-fast++` accepts `-fsanitize=fuzzer` and
links its own driver over `LLVMFuzzerTestOneInput`. Ask for the target by name — a bare `ninja`
fails, since AFL defines `FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION` itself and that is what
`fuzz/run.h` reads as "honggfuzz is driving":

```sh
CXX=afl-clang-fast++ meson setup builddir/afl
ninja -C builddir/afl test/fuzz_api
afl-fuzz -i data/fuzz/fuzz_api -o out -- ./builddir/afl/test/fuzz_api   # -i is never written to
```

`scripts/fuzz_afl.py` does all of that for you, which is worth using because the steps above are
easy to get subtly wrong:

```sh
scripts/fuzz_afl.py run              # every core, every target, until Ctrl-C
scripts/fuzz_afl.py run fuzz_api     # every core on one target
scripts/fuzz_afl.py sweep            # each target in turn, moving on when it goes quiet
scripts/fuzz_afl.py sweep --idle 15m # ... giving each one longer to prove it is done
scripts/fuzz_afl.py minimize         # fold the findings into data/fuzz, shrunk, with coverage
```

It builds what it needs, gives the first target's main instance the terminal so there is a status
screen to watch (the rest log to `fuzz-findings/<target>/afl-*.log`), resumes rather than restarting,
and stops everything on Ctrl-C. Committing what it produces is left to you.

`run` fuzzes every target at once, splitting the cores between them. `sweep` is for leaving alone:
it gives one target every core and moves on once that target has gone `--idle` without a new find,
which defaults to 5 minutes. The main instance keeps the terminal and its status screen, the same
as `run` — its `last new find` counter is what the moving on is based on. The deciding is done off
the queue directories rather than from `fuzzer_stats`, because afl-fuzz rewrites that file on its
own schedule and both `last_find` and `corpus_count` in it can sit unchanged for a minute at a
time -- long enough to call a target done while it is still finding things.

"Every core" means every *physical* core: the script reads `thread_siblings_list` and starts one
instance per core, pinned to it with `afl-fuzz -b`. Hyperthread siblings share a core's execution
units, so a second instance there mostly slows down the one already on it while afl-fuzz counts
both cores as busy. Which sibling represents a core is not guessable from the numbering — whether
core 0's siblings are `0,1` or `0,n/2` differs between machines — which is why it comes from the
kernel rather than from arithmetic. Where the topology cannot be read (macOS), it falls back to
`os.cpu_count()` and lets afl-fuzz place the instances itself.

Pinning with `-b` skips the scan afl-fuzz otherwise does for an unused core, so on a machine that
is already busy it shares rather than refusing to start.

Minimizing a corpus takes both tools, because neither subsumes the other: `afl-cmin` covers the same
AFL edges with far fewer files but is blind to libFuzzer's finer features, and `-merge=1` onto its
output adds back exactly the files carrying a feature it dropped. Note the `@@` — `afl-cmin` pipes
stdin by default, which this driver reports no coverage for.

```sh
afl-cmin -i data/fuzz/fuzz_api -o corpus-cmin -- ./builddir/afl/test/fuzz_api @@
cp -r corpus-cmin corpus-min && ./builddir/fuzz/test/fuzz_api -merge=1 corpus-min data/fuzz/fuzz_api
```

`.github/workflows/fuzz.yml` runs every target nightly, uploads any crash, and uploads the
coverage-increasing inputs it found. Its `minimize` dispatch input runs the two-step shrink above
instead of fuzzing. Committing what either produces stays a human decision.

## CI

`.github/workflows/main.yml` builds every leg the same way, so any of them reproduces locally:

```sh
meson setup builddir --force-fallback-for=fmt -Dcpp_std=c++17 <matrix setup_args>
meson test -C builddir --print-errorlogs
```

`--force-fallback-for=fmt` is what makes every runner build against the vendored fmt instead of
whatever the machine happens to have installed.

One leg builds `--unity=on --unity-size=16`. It is off by default because merging translation units
is bad for development — touching one file recompiles its whole chunk — but it catches a class of
problem separate compilation hides, and an anonymous namespace stops isolating a file once its
neighbours share the chunk. Everything it caught the first time had been there and invisible:
`test/app/print.h` had no include guard, and four `test/bench/*.cpp` each defined a `bench()` that
only became ambiguous when two landed in the same chunk.

**A collision surfaces when the chunking changes, not when the collision is written**, which makes
this leg fail on a commit that has nothing to do with it. `precomputed_hash.cpp` and
`lazy_bucket_allocation.cpp` had each declared a `counting_map` since long before 2026-09-06 —
one counts hash calls, one counts allocations, both in anonymous namespaces — and adding
`test/unit/move_home.cpp` to `test/meson.build` shifted every later file one place and brought the
two into chunk 4 together. So: reproduce this leg with its own size (`meson setup --unity=on
--unity-size=16`, since meson's default is 4 and the boundaries land elsewhere) whenever a test
file is *added or removed*, not only when one is edited. Renamed to `hash_counting_map` rather
than made distinct by chunk luck. The second error cluster in that log, `_Rb_tree_color does not
name a type` inside `stl_tree.h`, is gcc error recovery after the first failure and not a second
bug.

Linters (`scripts/lint/lint-*.py`, all of them via `scripts/lint/all.py`) run in the `lint` job.
Two of them pin their tool, because both tools gain checks or change their output between
releases: `clang-tidy-18` and `clang-format` 21 (`pip install clang-format==21.1.8`).
`lint-clang-format.py` *skips* rather than fails when it cannot find version 21, so a local run
with a different clang-format says so instead of reporting the tree as broken.

## Notes for sandboxed / offline environments

If meson cannot download the wrap subprojects (e.g. GitHub release tarballs blocked), fetch the doctest and fmt sources manually into `subprojects/doctest-2.5.3/` and `subprojects/fmt-12.0.0/` (matching the `directory` field of the `.wrap` files, which is what these version numbers have to keep agreeing with) with a minimal `meson.build` in each that declares `doctest_dep` (header-only, include dir `doctest/`) and `fmt_dep` (include dir `include/`, sources `src/format.cc`, `src/os.cc`) and calls `meson.override_dependency()`. Meson skips the download when the subproject directory already exists. These directories are gitignored — do not commit them.
