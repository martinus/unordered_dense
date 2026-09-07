#!/usr/bin/env python3
"""Build doc/charts.html: every chart in doc/, interactive, from the CSVs beside them.

The SVGs stay, because a README needs images and GitHub strips scripts out of an SVG. This is the
other half: one page that draws the same data with a legend you can click, a crosshair that reads
exact numbers off the lines, and a y axis that rescales when a series is hidden -- which is the
point of hiding one. Hide boost and the three maps that were squashed against the floor separate.

The series toggle is *global*: a map hidden in one chart is hidden in all of them, because the
question a reader has is about a map and not about a panel.

Stdlib only, one self-contained file out, no network at runtime -- the same constraints plot.py
works under, for the same reason.
"""
import csv
import json
import math
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plot import SERIES  # noqa: E402  the one palette, validated once

DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "doc")

# The one fact none of the CSVs carries. Set AB_MACHINE when regenerating elsewhere, so the
# page never claims a machine it was not measured on.
MACHINE = os.environ.get("AB_MACHINE", "Ryzen&nbsp;9&nbsp;7950X, clang&nbsp;22")


def read(name):
    path = os.path.join(DOC, name)
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return list(csv.DictReader(f))


def series_of(rows, column):
    """{map: [[x, y], ...]} for one column, x ascending."""
    out = {}
    for r in rows:
        out.setdefault(r["map"], []).append([float(r["entries"]), float(r[column])])
    for v in out.values():
        v.sort()
    return out


def resizes(rows):
    """The x values where the index doubles, for the dotted verticals. Only the size axis has them."""
    if "buckets" not in rows[0]:
        return []
    seen, out = None, []
    for r in rows:
        if r["map"] != "this":
            continue
        b = int(r["buckets"])
        if seen is not None and b != seen:
            out.append(float(r["entries"]))
        seen = b
    return out


def panel_of(rows, column, title):
    """One panel: a column of one CSV. Panels of a chart may come from different CSVs, which is how
    a chart shows the same workload for both key types side by side."""
    return None if rows is None else {"title": title, "series": series_of(rows, column)}


def chart(title, subtitle, panels, xlabel, unit, what, why, xlog=True, ylog=False, bars=False,
          warn="", resize_rows=None):
    panels = [p for p in panels if p]
    if not panels:
        return None
    return {
        "title": title, "subtitle": subtitle, "xlabel": xlabel, "unit": unit,
        "xlog": xlog, "ylog": ylog, "bars": bars, "what": what, "why": why, "warn": warn,
        "resizes": resizes(resize_rows) if resize_rows else [],
        "panels": panels,
    }


def main():
    r = {n: read(n) for n in (
        "find_hits_vs_size.csv", "find_vs_size.csv", "churn_vs_size.csv", "insert_erase_vs_size.csv",
        "memory_vs_size.csv", "memory_vs_value_size.csv",
        "find_hits_vs_size_str.csv", "find_vs_size_str.csv", "churn_vs_size_str.csv",
        "insert_erase_vs_size_str.csv", "memory_vs_size_str.csv", "memory_vs_value_size_str.csv",
        "hash_vs_length.csv")}
    vs = wide_value_size("value_size.csv")
    vs_str = wide_value_size("value_size_str.csv")

    # Every figure in the prose is derived from a CSV, so a missing one is not a chart that quietly
    # disappears -- it is a sentence with nothing to say. Since doc/ is not tracked, an empty one is
    # the normal state of a fresh clone, so say what to run rather than fail on a None.
    missing = sorted([n for n, rows in r.items() if rows is None] +
                     ([] if vs else ["value_size.csv"]) + ([] if vs_str else ["value_size_str.csv"]))
    if missing:
        print(f"doc/ is missing {len(missing)} of the measurements this page is built from:")
        print("  " + ", ".join(missing))
        print("run scripts/ab/regen.sh to measure and draw, or --redraw once the CSVs are there.")
        return 1

    # Facts, derived rather than written down: every figure the prose states about a measurement
    # comes from the CSV the chart draws, so re-running regen.sh re-derives the sentence with it.
    maps4 = ["this", "main", "jan", "boost"]
    po_u64 = per_octave_count(r["find_hits_vs_size.csv"])
    po_str = per_octave_count(r["find_hits_vs_size_str.csv"])
    biggest = int(x_range(r["find_hits_vs_size.csv"])[1])
    hit_boost = span(ratio_range(r["find_hits_vs_size.csv"], "ns", "this", "boost"))
    hit_main = span(ratio_range(r["find_hits_vs_size.csv"], "ns", "this", "main"), invert=True)
    hit_jan = span(ratio_range(r["find_hits_vs_size.csv"], "ns", "jan", "this"))
    str_hashgap = span(ratio_range(r["find_hits_vs_size_str.csv"], "ns", "boostdef", "boost"))
    str_boostdef = span(ratio_range(r["find_hits_vs_size_str.csv"], "ns", "this", "boostdef"), invert=True)
    mix_at, mix_span = spread_at(r["find_vs_size.csv"], "ns", 26000, maps4)
    _, hits_span = spread_at(r["find_hits_vs_size.csv"], "ns", mix_at, maps4)
    churn_boost = span(ratio_range(r["churn_vs_size.csv"], "ns", "this", "boost"))
    churn_main = span(ratio_range(r["churn_vs_size.csv"], "ns", "this", "main"), invert=True)
    vsizes = sorted({int(float(x["entries"])) for x in vs}) if vs else [8, 64]
    v_lo, v_hi = vsizes[0], vsizes[-1]
    it_lo = num(at_x(vs, "iterate", v_lo, "boost", "this"), 1)
    it_hi = num(at_x(vs, "iterate", v_hi, "boost", "this"), 1)
    bd_lo = num(at_x(vs, "build", v_lo, "boost", "this"), 2)
    bd_hi = num(at_x(vs, "build", v_hi, "boost", "this"), 2)
    mem = r["memory_vs_value_size.csv"]
    mem_lo = num(at_x(mem, "steady", v_lo, "boost", "this"), 2)
    mem_hi = num(at_x(mem, "steady", v_hi, "boost", "this"), 2)
    mem_peak = num(at_x(mem, "peak", v_hi, "boost", "this"), 2)
    hsh = r["hash_vs_length.csv"]
    hsh_t = span(ratio_range(hsh, "throughput", "boostdef", "this", per_octave=False), digits=1)
    hsh_l = span(ratio_range(hsh, "latency", "boostdef", "this", per_octave=False), digits=1)

    sawtooth = ("Nothing is reserved, so each table grows on its own and its load factor sweeps from "
                "about 0.5 just after a doubling to its maximum just before the next one. That is the "
                "sawtooth, and the dotted verticals are where this map doubles. A ratio read at a "
                "power of two is read at the emptiest a table ever is; read it at both ends.")

    def pair(csv, csv_str, column="ns"):
        """Two panels of one workload, one per key type, when there is one CSV for each."""
        return [panel_of(r[csv], column, "uint64_t keys"), panel_of(r[csv_str], column, "std::string keys")]

    def sized(csv, column="ns"):
        """Two panels of one CSV: the sizes most programs build, and the whole range."""
        return [panel_of(r[csv], column, "up to 64K entries"), panel_of(r[csv], column, "all sizes")]

    charts = [
        chart("Find, every lookup hitting", "nanoseconds per lookup, uint64_t keys",
              sized("find_hits_vs_size.csv"), "entries", "ns", resize_rows=r["find_hits_vs_size.csv"],
              what="A table of n entries, then random lookups of keys that are all present, drawn "
                   "uniformly and with the rng carrying on across epochs so no sequence repeats. One "
                   "lookup is a hash, a probe, and a comparison of the key that was found.",
              why="<b>The most discriminating lookup chart, and the one to decide by.</b> Every map "
                  "here does the same three things, so what differs is how many cache lines the probe "
                  "touches and how predictably it branches \u2014 and with the outcome fixed, neither is "
                  f"masked by anything else. Over an octave this map runs {hit_main} faster than the "
                  f"index it replaces and {hit_boost} the time of boost, whose flat layout reaches its "
                  f"value in one fewer dependent load. {sawtooth}"),
        chart("Find, every lookup hitting", "nanoseconds per lookup, std::string keys",
              sized("find_hits_vs_size_str.csv"), "entries", "ns", resize_rows=r["find_hits_vs_size_str.csv"],
              what="The same, with keys of 8 to 135 bytes skewed towards short. One fixed length would "
                   "make the hash's length dispatch perfectly predictable and hide a large part of "
                   "what a string lookup costs.",
              why="<b>The realistic case for most maps, and the one where the index matters least.</b> "
                  "Most of a string lookup is the hash and a memcmp behind a pointer the map has to "
                  "chase, so the maps converge: whatever the index does well is diluted by work none "
                  "of them can avoid. It also sets the ceiling on what a better index can buy a string "
                  f"map. <b>Watch the two green lines here:</b> solid is boost holding this map's "
                  f"wyhash, dashed is boost with the hash it ships with, and the dashed one is "
                  f"{str_hashgap} the solid one's time \u2014 enough that this map is {str_boostdef} "
                  "faster than an out-of-the-box boost while being behind the same map given this "
                  "hash. Which of those two is the honest comparison depends on whether you are "
                  "choosing an index or choosing a map."),
        chart("Find, half the lookups hitting", "nanoseconds per lookup, uint64_t keys",
              sized("find_vs_size.csv"), "entries", "ns", resize_rows=r["find_vs_size.csv"],
              what="The same lookups, but each one decides by a coin flip whether to ask for a key "
                   "that is present or one that is not, from a pool that never was.",
              why="It is the shape of a real membership test, and the misprediction it adds is a real "
                  "cost that a program with an unpredictable hit rate really pays.",
              warn="<b>Do not decide anything on this chart.</b> A 50% hit rate is the maximum-entropy "
                   "point of the hit-rate curve: it adds about half a branch misprediction per lookup "
                   "to every map, which is a flat tax that compresses exactly the differences the "
                   f"chart exists to show. Measured here, at {int(mix_at):,} entries the four maps "
                   f"span {hits_span:.2f}x on the all-hits chart and {mix_span:.2f}x on this one. It "
                   "can also invert their order, because a map whose probe already mispredicts pays "
                   "almost nothing more for an unpredictable outcome while a clean one pays in full. "
                   "A number that can rank two maps the opposite way from both of its own components "
                   "is not a summary of them. Use the all-hits chart above, and this one only to see "
                   "what outcome unpredictability costs."),
        chart("Find, half the lookups hitting", "nanoseconds per lookup, std::string keys",
              sized("find_vs_size_str.csv"), "entries", "ns", resize_rows=r["find_vs_size_str.csv"],
              what="The 50% mix on 8 to 135 byte keys.",
              why="Both effects at once, and they point the same way: the hash dilutes the index's "
                  "contribution and the coin flip compresses what is left.",
              warn="<b>The same caution as above, doubly.</b> Read the all-hits string chart instead."),
        chart("Churn at a fixed size", "nanoseconds per erase-and-insert pair, uint64_t keys",
              sized("churn_vs_size.csv"), "entries", "ns", resize_rows=r["churn_vs_size.csv"],
              what="Grow to n once, then forever erase a key that is present and insert one that is "
                   "not, so the size never changes and neither does the bucket count. What is left is "
                   "the steady state a long-lived table actually runs in.",
              why="<b>The workload that separates designs rather than constant factors.</b> A table "
                  "that has churned for a long time is not the table you built: a design that frees a "
                  "slot without undoing what once probed past it has probe sequences that only grow, "
                  "and repairs them with a rehash. Every other chart on this page is measured on a "
                  "table that has just been built, so this is the only one that can tell the two "
                  f"apart. Depending on where in the size range it is read, this map takes {churn_boost} the time of boost, and it is "
                  f"{churn_main} faster than the index it replaces throughout; watch boost's line for the sawtooth "
                  "of an in-place rehash it pays for at a fixed bucket count."),
        chart("Churn at a fixed size", "nanoseconds per erase-and-insert pair, std::string keys",
              sized("churn_vs_size_str.csv"), "entries", "ns", resize_rows=r["churn_vs_size_str.csv"],
              what="The same, on 8 to 135 byte keys.",
              why="Same property, smaller signal, and one extra thing to know: with string keys what "
                  "degrades under churn is partly the heap the key bodies live on rather than the "
                  "table, so this chart is measuring the allocator as well as the map."),
        chart("Insert and erase", "nanoseconds per operator[] and erase pair, uint64_t keys",
              sized("insert_erase_vs_size.csv"), "entries", "ns", resize_rows=r["insert_erase_vs_size.csv"],
              what="Four operations a round with the size invariant by construction: an operator[] "
                   "that finds, an erase that finds nothing, an erase that removes, and an operator[] "
                   "that inserts. Half of each hits, so every round is a coin flip the predictor "
                   "cannot win.",
              why="Largely the same story as churn, which is why the four-chart summary leaves it out, "
                  "and it is the closest thing here to a mixed read-write path. It is also where a "
                  "dense map does its most awkward work: an erase moves the last value into the hole "
                  "and has to find the slot that pointed at it, which for an integer key is free and "
                  "for a string key means hashing a second key."),
        chart("Insert and erase", "nanoseconds per operator[] and erase pair, std::string keys",
              sized("insert_erase_vs_size_str.csv"), "entries", "ns", resize_rows=r["insert_erase_vs_size_str.csv"],
              what="The same, on 8 to 135 byte keys.",
              why="Mostly a check that nothing about a string key changes the ordering. If it ever "
                  "does, that is the interesting result."),
        chart("Build from empty, against mapped-value size",
              f"nanoseconds per entry, 200000 entries, nothing reserved",
              [panel_of(vs, "build", "uint64_t keys"), panel_of(vs_str, "build", "std::string keys")],
              "sizeof(mapped_type), bytes", "ns", bars=True,
              what=f"Insert n entries into a default-constructed map, so the growth is included: about "
                   f"half of a build is rehashing, and a map that grows badly would otherwise score "
                   f"like one that grows well. Repeated across mapped values of {v_lo} to {v_hi} bytes.",
              why="<b>The axis that decides dense against flat.</b> A flat map writes the whole "
                  "value_type into a hash-scattered slot and moves it again on every rehash, so all of "
                  "its costs scale with the value; a dense map writes eight bytes there and appends "
                  f"the value to a vector in order. Boost takes {bd_lo} this map's time to build at {art(v_lo)} "
                  f"{v_lo} byte value and {bd_hi} at {v_hi}, and the gap widens across the axis, which "
                  "is the whole point of having the axis. A suite that fixes the mapped type at size_t "
                  "ranks the two families wrongly for map&lt;Key, SomeStruct&gt;, which is at least as "
                  f"common as map&lt;Key, size_t&gt;. It stops at {v_hi} bytes because past that the "
                  "working set leaves cache and every line bends upward together, and the chart stops "
                  "being about the value."),
        chart("One iteration pass, against mapped-value size",
              "nanoseconds per entry, 200000 entries",
              [panel_of(vs, "iterate", "uint64_t keys"), panel_of(vs_str, "iterate", "std::string keys")],
              "sizeof(mapped_type), bytes", "ns", bars=True,
              what="Walk every entry once and read one field of each, on a map built and then left "
                   "alone. Separate from the build chart because the two answer different questions "
                   "and differ by orders of magnitude \u2014 on one axis together, the iteration would be "
                   "a flat line along the floor.",
              why="<b>The one place the dense layout wins outright, and by the largest margin on this "
                  "page.</b> A dense map iterates a contiguous vector; a flat map walks its whole slot "
                  "array and skips the empty ones, which at load 0.5 is half of what it touches. That "
                  f"is {it_lo} at {art(v_lo)} {v_lo} byte integer-keyed value, narrowing to {it_hi} at {v_hi} as "
                  "the payload starts to dominate. If you iterate at all often, this chart is the "
                  "argument."),
        chart("String hash cost, against key length",
              "nanoseconds per hash, key bodies cache-resident",
              [panel_of(r["hash_vs_length.csv"], "throughput", "many independent hashes"),
               panel_of(r["hash_vs_length.csv"], "latency", "one at a time, as a lookup pays")],
              "key length, bytes", "ns", xlog=False,
              what="One hash function over 256 keys of a single length, for every length through the "
                   "short path and the block range and then coarsely to the right-hand edge. The left "
                   "panel hashes independent keys, so the machine runs as many at once as it has "
                   "multipliers; the right feeds a byte of each answer into the next key, so no two "
                   "overlap. The keys are cache-resident on purpose: this is a measurement of hashing, "
                   "and the size charts above are where the memory system belongs.",
              why="<b>A hash has two costs and they can disagree completely, which is the whole reason "
                  "for two panels.</b> Throughput is what a hashing loop pays and what most hash "
                  "benchmarks report; latency is what a map lookup pays, because the hash's result is "
                  "the address of the group to probe and nothing can start until the chain of "
                  f"multiplies resolves. Boost's own string hash is {hsh_t} this one's time on the "
                  f"left and {hsh_l} on the right \u2014 the same two measurements disagreeing about how "
                  "much worse. The staircase is real: a hash dispatches on length, so its cost steps "
                  "wherever the implementation changes strategy.",
              warn="One length per point, so the length dispatch is <b>perfectly predicted here</b>, "
                   "where the scored benchmark's mixed keys cost it about a third of a branch miss per "
                   "hash. This chart is the shape; <code>hashstr</code> in the scored suite is the "
                   "number that includes the dispatch. Note also that only two of these lines are a "
                   "choice a caller makes \u2014 the other two are what this library shipped before."),
        chart("Memory, against mapped-value size", "megabytes held for 1000000 entries, uint64_t keys",
              [panel_of(r["memory_vs_value_size.csv"], "steady", "steady state"),
               panel_of(r["memory_vs_value_size.csv"], "peak", "peak during growth")],
              "sizeof(mapped_type), bytes", "MB", bars=True,
              what="Every allocation the process makes while building the map, counted by replacing "
                   "global new and delete. Steady state is what it holds when built; the peak is the "
                   "most it ever held, which is during a growth, when the new array is allocated "
                   "before the old one is freed.",
              why="<b>Speed alone picks the wrong map often enough to deserve a chart, and this is the "
                  "axis memory differentiates on.</b> Per entry it barely moves with the table size, "
                  f"but against the value it moves a lot: boost holds {mem_lo} this map's bytes at {art(v_lo)} "
                  f"{v_lo} byte value and {mem_hi} at {v_hi}, because a flat map pays for its empty "
                  "slots at the full width of the value where a dense one pays four bytes of index. "
                  f"The peak is the number a caller has to have room for and is where the gap is "
                  f"widest: {mem_peak} at {v_hi} bytes."),
        chart("Memory, against mapped-value size", "megabytes held for 200000 entries, std::string keys",
              [panel_of(r["memory_vs_value_size_str.csv"], "steady", "steady state"),
               panel_of(r["memory_vs_value_size_str.csv"], "peak", "peak during growth")],
              "sizeof(mapped_type), bytes", "MB", bars=True,
              what="The same count with string keys, which is why it counts global new rather than the "
                   "container's allocator: the key bodies are allocated by std::allocator&lt;char&gt; "
                   "inside each string, which a container allocator never sees.",
              why="Worth having because it is the case where the dense layout's memory advantage "
                  "mostly disappears. The key bodies are the same heap for every map, and the value "
                  "the map holds is a 32 byte string header whatever the mapped type does."),
        chart("Memory, against table size", "megabytes held, uint64_t keys",
              [panel_of(r["memory_vs_size.csv"], "steady", "steady state"),
               panel_of(r["memory_vs_size.csv"], "peak", "peak during growth")],
              "entries", "MB", ylog=True,
              what="The same count against the number of entries, at a size_t mapped value.",
              why="Here to show that it is the boring axis: per entry, memory barely moves with the "
                  "table size, so this is a stack of near-identical octaves, which is exactly why the "
                  "value-size chart above is the one that discriminates. The staircase is the "
                  "doubling."),
        chart("Memory, against table size", "megabytes held, std::string keys",
              [panel_of(r["memory_vs_size_str.csv"], "steady", "steady state"),
               panel_of(r["memory_vs_size_str.csv"], "peak", "peak during growth")],
              "entries", "MB", ylog=True,
              what="The same with string keys, the key bodies included.",
              why="Shows how much of a string map is the strings: most of it at small values, which is "
                  "the reason the four maps sit almost on top of each other."),
    ]
    charts = [c for c in charts if c]
    for c in charts:
        c["group"] = group_of(c["title"])
    chart_ids(charts)

    colors = {k: {"light": light, "dark": dark, "label": label, "dash": dash}
              for k, label, light, dark, dash in SERIES}
    order = [k for k, _, _, _, _ in SERIES]
    out = os.path.join(DOC, "charts.html")
    with open(out, "w") as f:
        light = "".join(f" --c-{k}: {v['light']};" for k, v in colors.items())
        dark = "".join(f" --c-{k}: {v['dark']};" for k, v in colors.items())
        # Three rules rather than two: `auto` follows the system, and an explicit choice has to beat
        # the media query, which a bare `:root` inside it would not.
        css = (f"  :root {{{light} }}\n"
               f"  @media (prefers-color-scheme: dark) {{ :root:not([data-theme=\"light\"]) {{{dark} }} }}\n"
               f"  :root[data-theme=\"dark\"] {{{dark} }}")
        intro = (
            f"{word(len(SERIES)).capitalize()} hash maps, timed interleaved with each other in one "
            "process, so a clock ramp or a noisy neighbour hits all of them and cancels out of the "
            "comparison. Nothing is reserved, and the tables are sampled "
            f"{word(po_u64)} times per octave ({word(po_str)} for strings), so the load-factor "
            f"sawtooth between doublings is visible rather than aliased away. {MACHINE}.")
        footer = (
            "Absolute times are the median epoch. Two runs of the same sweep on a quiet machine agree "
            "to well under a percent at the median point and several percent at the worst, so read the "
            "shape rather than the last digit, and check a surprising point against a second run. The "
            f"sweeps stop at {si(biggest)} entries because a single incremental pass above that "
            "depends on page placement that varies between runs.")
        f.write(PAGE.replace("__INTRO__", intro).replace("__FOOTER__", footer).replace("__DATA__", json.dumps({"charts": charts, "colors": colors, "order": order,
                                                     "groups": GROUPS, "about": ABOUT}))
                    .replace("__SERIESCSS__", css))
    print(f"wrote {out}: {len(charts)} charts")


# ---- facts computed from the CSVs, so the prose cannot go stale ----------------------------
#
# Every number the page states about a measurement is derived here from the same CSV the chart
# draws. Writing "18-31% slower" by hand means the sentence is wrong the next time regen.sh runs
# and nothing says so; a sentence built from the data is re-derived with the chart.

def _pts(rows, col, m):
    return sorted((float(r["entries"]), float(r[col])) for r in rows if r["map"] == m)


def _geomean(vals):
    vals = [v for v in vals if v > 0]
    return math.exp(sum(math.log(v) for v in vals) / len(vals)) if vals else float("nan")


def ratio_range(rows, col, num, den, per_octave=True):
    """How `num` compares with `den`, as the octave geomeans run: (smallest, largest).

    Read per octave rather than point by point, because a load-factor sawtooth means one sample can
    be near either end of a factor-of-two swing; the octave is the unit that averages over it."""
    a, b = dict(_pts(rows, col, den)), dict(_pts(rows, col, num))
    shared = sorted(set(a) & set(b))
    if not shared:
        return None
    by_oct = {}
    for n in shared:
        by_oct.setdefault(int(math.floor(math.log2(n))) if per_octave else 0, []).append(b[n] / a[n])
    g = sorted(_geomean(v) for v in by_oct.values())
    return g[0], g[-1]


def spread_at(rows, col, near, maps):
    """The factor between the fastest and slowest of `maps` at the sample nearest `near`."""
    have = [m for m in maps if _pts(rows, col, m)]
    if not have:
        return None
    xs = [x for x, _ in _pts(rows, col, have[0])]
    n = min(xs, key=lambda x: abs(x - near))
    vals = [dict(_pts(rows, col, m))[n] for m in have if n in dict(_pts(rows, col, m))]
    return n, max(vals) / min(vals)


def at_x(rows, col, x, num, den):
    """num / den at exactly x, for a categorical axis such as the value size."""
    a, b = dict(_pts(rows, col, den)), dict(_pts(rows, col, num))
    return b[x] / a[x] if x in a and x in b else None


def x_range(rows):
    xs = [float(r["entries"]) for r in rows]
    return min(xs), max(xs)


def per_octave_count(rows):
    """Sample points per octave, counted in a mid octave: near the bottom of the range consecutive
    samples round to the same integer and collapse, which undercounts the sampling density."""
    xs = sorted({float(r["entries"]) for r in rows})
    base = 2 ** int(math.floor(math.log2(xs[len(xs) // 2])))
    return sum(1 for x in xs if base <= x < base * 2)


def num(v, digits=2):
    """A ratio, written the way the page says ratios: 1.25x."""
    return "n/a" if v is None else f"{v:.{digits}f}x"


def pct_slower(v):
    return "n/a" if v is None else f"{(v - 1) * 100:.0f}%"


def span(rng_, invert=False, digits=2):
    """A (lo, hi) pair of ratios as text: "1.07-1.33x", or one figure when the ends agree.

    `invert` flips it, for saying how much faster the faster side is when the ratio is of times."""
    if rng_ is None:
        return "n/a"
    lo, hi = (1 / rng_[1], 1 / rng_[0]) if invert else rng_
    if abs(hi - lo) < 0.025:
        return f"{(lo + hi) / 2:.{digits}f}x"
    return f"{lo:.{digits}f}\u2013{hi:.{digits}f}x"


def si(n):
    """16, 128, 1K, 1M -- the same shortening the page's axis labels use."""
    for cut, suffix in ((1 << 20, "M"), (1 << 10, "K")):
        if n >= cut:
            v = n / cut
            return f"{v:g}{suffix}"
    return str(int(n))


def slug(text):
    """A stable, readable id: "Find, every lookup hitting" -> find-every-lookup-hitting."""
    return re.sub(r"-+", "-", re.sub(r"[^a-z0-9]+", "-", text.lower())).strip("-")


def chart_ids(charts):
    """An id per chart, disambiguated by key type rather than by a counter: four titles appear
    twice, once per key type, and `-uint64` says which far better than `-2` does."""
    used = {}
    for c in charts:
        base = slug(c["title"])
        sub = c["subtitle"]
        if "uint64_t" in sub:
            base += "-uint64"
        elif "std::string" in sub:
            base += "-string"
        n = used.get(base, 0)
        used[base] = n + 1
        c["id"] = base if n == 0 else f"{base}-{n + 1}"
        c["gid"] = slug(c["group"])


def art(n):
    """"a" or "an" for a number written as digits: an 8 byte value, a 64 byte one."""
    t = str(int(n))
    return "an" if t[0] == "8" or t[:2] in ("11", "18") else "a"


def word(n):
    return {1: "one", 2: "two", 3: "three", 4: "four", 5: "five", 6: "six", 7: "seven",
            8: "eight", 12: "twelve", 16: "sixteen", 24: "twenty-four"}.get(n, str(n))


# The fifteen charts fall into five questions. Grouping them gives the page real structure to
# navigate, and the heading says which question the charts under it answer.
GROUPS = {
    "Lookups": "Reading is what most maps do most of the time. One lookup is a hash, a probe over "
               "groups of sixteen fingerprints, and one comparison of the key that was found.",
    "Churn and mixed writes": "A table that has run at a steady size for a long time, and one that "
                              "mixes finds with inserts and erases. These separate designs rather "
                              "than constant factors.",
    "Building and iterating": "Filling a map from empty and walking it once, against the size of "
                              "the mapped value \u2014 the axis on which a dense layout and a flat "
                              "one trade places.",
    "The hash": "One component on its own: what hashing a string costs against the length of the key.",
    "Memory": "What the process actually holds, counted by replacing global new and delete.",
}

ABOUT = {
    "this": "The group index on this branch: sixteen one-byte fingerprints per group, compared in "
            "one instruction, with the values kept dense in a vector.",
    "main": "The released robin hood index this one replaces.",
    "jan": "Where the library stood on 1 January 2026, before this year's work.",
    "boost": "A flat map \u2014 the values live in the slot array itself. Given this map's hash, so "
             "what differs from the three above is the index.",
    "boostdef": "The same boost map with the hash it ships with: what a caller gets by not passing "
                "a third template argument.",
}


def group_of(title):
    """Which group a chart belongs to, from its own title, so a missing CSV cannot shift the rest."""
    for prefix, name in (("Find", "Lookups"), ("Churn", "Churn and mixed writes"),
                         ("Insert and erase", "Churn and mixed writes"),
                         ("Build from empty", "Building and iterating"),
                         ("One iteration pass", "Building and iterating"),
                         ("String hash cost", "The hash"), ("Memory", "Memory")):
        if title.startswith(prefix):
            return name
    return "Other"


def wide_value_size(name):
    """value_size*.csv is long-form (a `what` column); a chart wants one column per workload."""
    rows = read(name)
    if rows is None:
        return None
    wide = {}
    for r in rows:
        wide.setdefault((r["entries"], r["map"]), {})[r["what"]] = r["ns"]
    return [{"entries": n, "map": m, "build": v["build"], "iterate": v["iterate"]}
            for (n, m), v in wide.items()]


PAGE = r"""<!DOCTYPE html>
<html lang="en" data-theme="auto">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>unordered_dense &mdash; measurements</title>
<style>
  /* Warm neutral chrome on purpose: the four series hues are validated for colourblind separation
     against these two surfaces, and they are the only colour the page is allowed to spend. */
  :root {
    --surface: #fcfcfb; --raised: #f5f4f0; --ink: #1f1e1a; --ink2: #56544c; --ink3: #86847b;
    --grid: #eae8e2; --rule: #e3e1da; --rule-strong: #1f1e1a;
    --caution: #9a4526; --caution-rule: #ddbfae;
    --shadow: 0 6px 20px rgba(31,30,26,.13);
    /* Fill the monitor rather than a 1180px column, but stop before a panel gets so wide that its
       own height (the viewBox is 2.2:1) pushes the next chart off the screen. */
    --page: min(100%, 2600px);
    --bar: 56px;
    --t-hero: 42px; --t-group: 29px; --t-chart: 21px; --t-body: 17px; --t-small: 15px; --t-micro: 13.5px;
  }
  @media (prefers-color-scheme: dark) {
    :root:not([data-theme="light"]) {
      --surface: #171714; --raised: #1f1f1b; --ink: #f4f3ec; --ink2: #b6b4a8; --ink3: #838177;
      --grid: #2c2c28; --rule: #2e2e29; --rule-strong: #6f6d64;
      --caution: #e8a184; --caution-rule: #5b3a2b;
      --shadow: 0 6px 20px rgba(0,0,0,.45);
    }
  }
  :root[data-theme="dark"] {
    --surface: #171714; --raised: #1f1f1b; --ink: #f4f3ec; --ink2: #b6b4a8; --ink3: #838177;
    --grid: #2c2c28; --rule: #2e2e29; --rule-strong: #6f6d64;
    --caution: #e8a184; --caution-rule: #5b3a2b;
    --shadow: 0 6px 20px rgba(0,0,0,.45);
  }
__SERIESCSS__
  * { box-sizing: border-box; }
  html { color-scheme: light dark; }
  body {
    margin: 0; padding: var(--bar) 28px 96px; background: var(--surface); color: var(--ink);
    font-family: system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    font-size: var(--t-body); line-height: 1.65; font-variant-numeric: tabular-nums;
    -webkit-font-smoothing: antialiased;
  }
  .wrap { max-width: var(--page); margin: 0 auto; }
  .measure { max-width: 72ch; }
  a { color: inherit; }

  /* ---- top bar: the map controls, always reachable, plus the theme switch ---- */
  .bar {
    position: fixed; top: 0; left: 0; right: 0; z-index: 20;
    background: color-mix(in srgb, var(--surface) 88%, transparent);
    backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px);
    border-bottom: 1px solid var(--rule);
  }
  .bar .wrap { display: flex; gap: 16px; align-items: center; padding: 9px 0; }
  .bar .chips { display: flex; flex-wrap: wrap; gap: 7px; align-items: center; flex: 1 1 auto; }
  .bar .hint { color: var(--ink3); font-size: var(--t-micro); margin-right: 2px; }
  button.chip {
    display: inline-flex; align-items: center; gap: 8px; cursor: pointer;
    border: 1px solid var(--rule); background: var(--raised); color: var(--ink);
    border-radius: 999px; padding: 5px 13px 5px 10px; font: inherit; font-size: var(--t-micro);
  }
  button.chip .dot { width: 10px; height: 10px; border-radius: 50%; flex: none; }
  button.chip[aria-pressed="false"] { color: var(--ink3); background: transparent; }
  button.chip[aria-pressed="false"] .dot { background: none !important; box-shadow: inset 0 0 0 1.5px currentColor; }
  :is(a, button, [tabindex]):focus-visible { outline: 2px solid var(--c-this); outline-offset: 3px; border-radius: 4px; }

  .theme { display: flex; gap: 2px; padding: 2px; border: 1px solid var(--rule); border-radius: 999px; flex: none; }
  .theme button {
    cursor: pointer; border: 0; background: transparent; color: var(--ink3);
    font: inherit; font-size: var(--t-micro); padding: 3px 11px; border-radius: 999px;
  }
  .theme button[aria-pressed="true"] { background: var(--ink); color: var(--surface); }

  /* ---- title block ---- */
  header { padding: 40px 0 8px; }
  h1 { font-size: var(--t-hero); font-weight: 620; letter-spacing: -0.021em; line-height: 1.08; margin: 0 0 14px; }
  h1 .sub { display: block; font-size: var(--t-group); font-weight: 400; color: var(--ink3); letter-spacing: -0.012em; margin-top: 4px; }
  .lede { color: var(--ink2); margin: 0 0 18px; }

  /* ---- the map key: legend, documentation and control in one ---- */
  .key { margin: 26px 0 0; border-top: 2px solid var(--rule-strong); }
  .key h2 { font-size: var(--t-small); font-weight: 600; color: var(--ink3); margin: 14px 0 10px; letter-spacing: 0; }
  /* Rows separated by hairlines rather than a bordered card: the rows differ in height, and a card
     grid turns that difference into grey blocks. The fifth has no partner, so it takes the width. */
  .key ul { list-style: none; margin: 0; padding: 0; display: grid; gap: 0 48px; }
  @media (min-width: 1100px) { .key ul { grid-template-columns: 1fr 1fr; }
                               .key li:last-child:nth-child(odd) { grid-column: 1 / -1; } }
  .key li { margin: 0; }
  .key button {
    display: grid; grid-template-columns: 26px minmax(0, 1fr); gap: 0 13px; align-items: baseline;
    width: 100%; height: 100%; text-align: left; cursor: pointer; font: inherit;
    border: 0; border-top: 1px solid var(--rule);
    background: transparent; color: var(--ink); padding: 14px 12px; margin: 0 -12px;
  }
  .key button:hover { background: var(--raised); }
  .key .swatch { grid-row: span 2; align-self: center; height: 4px; border-radius: 2px; background: var(--sw); }
  .key .swatch.dashed { background: repeating-linear-gradient(90deg, var(--sw) 0 7px, transparent 7px 12px); }
  .key .name { font-weight: 600; font-size: var(--t-body); }
  .key .desc { color: var(--ink2); font-size: var(--t-small); line-height: 1.55; }
  .key button[aria-pressed="false"] { color: var(--ink3); }
  .key button[aria-pressed="false"] .name { font-weight: 500; text-decoration: line-through; text-decoration-thickness: 1px; }
  .key button[aria-pressed="false"] .swatch { background: var(--rule); }
  .key button[aria-pressed="false"] .desc { color: var(--ink3); }
  .keynote { color: var(--ink3); font-size: var(--t-small); margin: 12px 0 0; }

  /* ---- benchmark groups ---- */
  .group { margin: 92px 0 0; border-top: 2px solid var(--rule-strong); padding-top: 18px; }
  .group h2 { font-size: var(--t-group); font-weight: 600; letter-spacing: -0.014em; margin: 0 0 6px; line-height: 1.15; }
  /* Every heading is a link to itself. The marker sits after the text rather than in the left
     margin, which at this page width has no room for it and would clip on a narrow screen. */
  .group, figure { scroll-margin-top: calc(var(--bar) + 18px); }
  a.anchor { color: inherit; text-decoration: none; }
  a.anchor::after {
    content: "#"; margin-left: .4em; color: var(--ink3); font-weight: 400;
    opacity: 0; transition: opacity .12s;
  }
  a.anchor:hover::after, a.anchor:focus-visible::after { opacity: 1; }
  a.anchor:hover { text-decoration: underline; text-decoration-thickness: 1px; text-underline-offset: 5px; }
  .group .gsub { color: var(--ink2); margin: 0; }

  /* ---- one benchmark ---- */
  figure { margin: 52px 0 0; padding: 0; }
  figure + figure { border-top: 1px solid var(--rule); padding-top: 44px; }
  .ctitle { font-size: var(--t-chart); font-weight: 600; letter-spacing: -0.011em; margin: 0 0 3px; }
  .csub { color: var(--ink2); font-size: var(--t-small); margin: 0 0 20px; }
  .panels { display: grid; grid-template-columns: 1fr 1fr; gap: 38px; }
  @media (max-width: 980px) { .panels { grid-template-columns: 1fr; } }
  .panel { position: relative; }
  .ptitle { font-size: var(--t-small); font-weight: 600; color: var(--ink2); margin: 0 0 6px 46px; }
  svg { display: block; width: 100%; height: auto; touch-action: none; }

  figcaption { margin-top: 22px; }
  .notes { display: grid; gap: 12px 44px; max-width: 132ch; }
  @media (min-width: 1100px) { .notes { grid-template-columns: 1fr 1fr; } }
  .notes h3 { font-size: var(--t-micro); font-weight: 600; color: var(--ink3); margin: 0 0 4px; }
  .notes p { margin: 0; color: var(--ink2); font-size: var(--t-small); line-height: 1.6; }
  .notes b { color: var(--ink); font-weight: 600; }
  /* Quiet by design: this is a qualification on a number, not an error. No fill, no border box. */
  .caution { margin: 0 0 20px; padding-left: 15px; border-left: 2px solid var(--caution-rule); max-width: 96ch; }
  .caution p { margin: 0; color: var(--ink2); font-size: var(--t-small); line-height: 1.6; }
  .caution .lead { color: var(--caution); font-weight: 600; }
  .caution b { color: var(--ink); font-weight: 600; }

  .tip {
    position: absolute; pointer-events: none; opacity: 0; transition: opacity .08s;
    background: var(--surface); border: 1px solid var(--rule); border-radius: 8px;
    padding: 9px 11px; font-size: var(--t-micro); box-shadow: var(--shadow); white-space: nowrap;
  }
  .tip .x { color: var(--ink3); margin-bottom: 5px; }
  .tip table { border-collapse: collapse; }
  .tip td { padding: 1.5px 0; }
  .tip td.n { text-align: right; padding-left: 14px; }
  .tip .sw { width: 9px; height: 9px; border-radius: 50%; display: inline-block; margin-right: 7px; }
  .empty { color: var(--ink3); font-size: var(--t-small); padding: 34px 0 0 46px; }

  footer { margin: 96px 0 0; border-top: 1px solid var(--rule); padding-top: 20px; color: var(--ink3); font-size: var(--t-small); }
  @media (prefers-reduced-motion: reduce) { * { transition: none !important; } }
</style>
</head>
<body>

<div class="bar">
  <div class="wrap">
    <div class="chips" id="chips"><span class="hint">Show or hide, everywhere:</span></div>
    <div class="theme" role="group" aria-label="Colour theme">
      <button type="button" data-theme-set="auto">Auto</button>
      <button type="button" data-theme-set="light">Light</button>
      <button type="button" data-theme-set="dark">Dark</button>
    </div>
  </div>
</div>

<div class="wrap">
<header>
  <h1>ankerl::unordered_dense<span class="sub">Measurements</span></h1>
  <p class="lede measure">__INTRO__</p>

  <section class="key">
    <h2>What is being compared</h2>
    <ul id="key"></ul>
    <p class="keynote measure">The first four are all given <em>this</em> map's hash, so the only
    thing that differs between them is the index. The last one shares boost's colour because it is
    boost: the gap between the two green lines is what the hash choice alone is worth.</p>
  </section>
</header>

<main id="charts"></main>

<footer class="measure">__FOOTER__</footer>
</div>

<script>
const DATA = __DATA__;
// Which series are hidden lives in the URL, so a view of the data is a link you can send someone --
// "#hide=boost" is "the four unordered_dense lines, rescaled to fill the panel".
// The fragment carries two things: which chart to scroll to, and which series are hidden. A bare
// "#churn-at-a-fixed-size-uint64" is what the heading links copy, and the browser scrolls to it on
// its own; "#churn-at-a-fixed-size-uint64&hide=boost" adds the state, and is scrolled to by hand
// because no element has that whole string as an id.
const hidden = new Set((new URLSearchParams(location.hash.slice(1)).get("hide") || "")
  .split(",").filter(m => DATA.colors[m]));
const anchorOf = (h) => (h || "").replace(/^#/, "").split("&").find(t => t && !t.includes("=")) || "";
let anchor = anchorOf(location.hash);

function writeHash() {
  const bits = [];
  if (anchor) bits.push(anchor);
  if (hidden.size) bits.push("hide=" + [...hidden].join(","));
  history.replaceState(null, "", bits.length ? "#" + bits.join("&") : location.pathname);
}

function scrollToAnchor(smooth) {
  if (!anchor) return;
  const el = document.getElementById(anchor);
  if (el) el.scrollIntoView({ behavior: smooth ? "smooth" : "auto", block: "start" });
}

// ---- theme: auto follows the system, light and dark are explicit and remembered ----
const themeButtons = [...document.querySelectorAll("[data-theme-set]")];
function setTheme(t) {
  document.documentElement.setAttribute("data-theme", t);
  try { t === "auto" ? localStorage.removeItem("theme") : localStorage.setItem("theme", t); } catch (e) {}
  for (const b of themeButtons) b.setAttribute("aria-pressed", String(b.dataset.themeSet === t));
}
for (const b of themeButtons) b.onclick = () => setTheme(b.dataset.themeSet);
let saved = "auto";
try { saved = localStorage.getItem("theme") || "auto"; } catch (e) {}
setTheme(saved);

// Values in a tooltip, where three significant figures is the most the measurement supports.
const fmt = (v) => {
  if (v === 0) return "0";
  const a = Math.abs(v);
  if (a >= 1000) return v.toFixed(0);
  if (a >= 100) return v.toFixed(1);
  if (a >= 1) return v.toFixed(2);
  if (a >= 0.01) return v.toFixed(3);
  return v.toExponential(1);
};
// Axis ticks, where trailing zeros are noise: 12.5 and 50, not 12.50 and 50.00.
const tick = (v) => v === 0 ? "0" : String(+v.toPrecision(4));
const si = (n) => {
  if (n >= (1 << 20)) return +(n / (1 << 20)).toPrecision(3) + "M";
  if (n >= 1024) return +(n / 1024).toPrecision(3) + "K";
  return String(Math.round(n));
};

// Ticks that *cover* the data: the last one is at or above max, so a line never leaves the plot.
// Stopping at the last tick below max is what put 23 points of the find chart above its own frame.
function niceTicks(max, min) {
  const steps = [0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 2.5, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 5000, 10000];
  for (const s of steps) if ((max - min) / s <= 6) {
    // The top tick is the first multiple of the step strictly above the data, so the tallest peak
    // has headroom and a 2px stroke is never half-clipped by the frame.
    const lo = Math.floor(min / s) * s, hi = (Math.floor(max / s) + 1) * s;
    const out = [];
    for (let v = lo; v <= hi + s * 1e-9; v += s) out.push(+v.toFixed(10));
    return out;
  }
  return [min, max];
}

// One panel: axes, grid, the visible lines, and a crosshair that reads values off them.
function drawPanel(host, chart, panel, xDomain) {
  const W = 900, H = 418, L = 56, R = 16, T = 34, B = 42;
  const maps = DATA.order.filter(m => panel.series[m] && !hidden.has(m));
  host.innerHTML = "";
  if (!maps.length) { host.innerHTML = '<div class="empty">Every series is hidden. Turn one back on above.</div>'; return; }

  const pts = m => panel.series[m].filter(p => p[0] >= xDomain[0] && p[0] <= xDomain[1]);
  let lo = Infinity, hi = -Infinity;
  for (const m of maps) for (const p of pts(m)) { if (p[1] < lo) lo = p[1]; if (p[1] > hi) hi = p[1]; }
  const logy = chart.ylog;
  let ticks, y0, y1;
  if (logy) {
    y0 = Math.floor(Math.log10(lo)); y1 = Math.ceil(Math.log10(hi));
    ticks = []; for (let i = y0; i <= y1; i++) ticks.push(i);
  } else {
    // zero-based unless the lines sit far off the floor, where a zero baseline would flatten them
    const base = lo > (hi - lo) * 1.5 ? lo - (hi - lo) * 0.25 : 0;
    ticks = niceTicks(hi, base); y0 = ticks[0]; y1 = ticks[ticks.length - 1];
  }
  const lx0 = Math.log2(xDomain[0]), lx1 = Math.log2(xDomain[1]);
  // A handful of value sizes are categories, not a continuum, so they get a band scale and bars:
  // a line between 32 and 48 bytes would draw an interpolation that was never measured.
  const cats = chart.bars ? panel.series[maps[0]].map(p => p[0]) : [];
  const px = v => chart.bars
    ? L + (cats.indexOf(v) + 0.5) / cats.length * (W - L - R)
    : L + (chart.xlog ? (Math.log2(v) - lx0) / (lx1 - lx0) : (v - xDomain[0]) / (xDomain[1] - xDomain[0])) * (W - L - R);
  const py = v => T + (H - T - B) * (1 - ((logy ? Math.log10(v) : v) - y0) / (y1 - y0));

  const g = [];
  g.push(`<svg viewBox="0 0 ${W} ${H}" preserveAspectRatio="xMidYMid meet" role="img" aria-label="${chart.title}, ${panel.title}">`);
  // A hatch carries on a bar the distinction a dash carries on a line: same map, different hash.
  const hatched = maps.filter(m => DATA.colors[m].dash);
  if (hatched.length) {
    g.push("<defs>" + hatched.map(m =>
      `<pattern id="h_${m}" width="7" height="7" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">` +
      `<rect width="7" height="7" fill="var(--c-${m})"/><rect width="3" height="7" fill="var(--surface)" fill-opacity="0.8"/></pattern>`
    ).join("") + "</defs>");
  }
  for (const t of ticks) {
    const y = py(logy ? Math.pow(10, t) : t);
    g.push(`<line x1="${L}" y1="${y.toFixed(1)}" x2="${W - R}" y2="${y.toFixed(1)}" stroke="var(--grid)" stroke-width="1"/>`);
    g.push(`<text x="${L - 9}" y="${(y + 4.5).toFixed(1)}" text-anchor="end" font-size="14" fill="var(--ink3)">${logy ? tick(Math.pow(10, t)) : tick(t)}</text>`);
  }
  for (const rx of chart.resizes) {
    if (rx < xDomain[0] || rx > xDomain[1]) continue;
    g.push(`<line x1="${px(rx).toFixed(1)}" y1="${T}" x2="${px(rx).toFixed(1)}" y2="${H - B}" stroke="var(--grid)" stroke-width="1" stroke-dasharray="2 3"/>`);
  }
  const xs = panel.series[maps[0]].map(p => p[0]).filter(v => v >= xDomain[0] && v <= xDomain[1]);
  // Round numbers, not whichever samples happen to fall every nth: a size axis gets powers of eight
  // off a power of two, so the labels read 16, 128, 1K, 8K rather than 6.7275390625K.
  let xticks;
  if (xs.length <= 8) {
    xticks = xs;
  } else if (chart.xlog) {
    xticks = [];
    for (let v = Math.pow(2, Math.ceil(Math.log2(xDomain[0]))); v <= xDomain[1]; v *= 8) xticks.push(v);
  } else {
    // A length axis is linear, so round multiples of a step rather than powers of anything.
    const span = xDomain[1] - xDomain[0];
    const step = [8, 16, 32, 64, 128, 256, 512, 1024, 2048].find(s => span / s <= 6) || span;
    xticks = [xDomain[0]];
    for (let v = Math.ceil(xDomain[0] / step) * step; v <= xDomain[1]; v += step)
      if (v - xDomain[0] > step / 2) xticks.push(v);
  }
  for (const v of xticks)
    g.push(`<text x="${px(v).toFixed(1)}" y="${H - B + 19}" text-anchor="middle" font-size="14" fill="var(--ink3)">${chart.xlabel === "entries" ? si(v) : v}</text>`);
  g.push(`<text x="${(L + (W - R - L) / 2).toFixed(1)}" y="${H - 6}" text-anchor="middle" font-size="14" fill="var(--ink3)">${chart.xlabel}</text>`);
  // Above the plot area rather than beside it: at T - 6 it landed on the topmost y tick.
  g.push(`<text x="${L - 9}" y="${T - 16}" text-anchor="end" font-size="14" fill="var(--ink3)">${chart.unit}</text>`);

  if (chart.bars) {
    // 2px of surface between neighbours so two bars never read as one, and rounded at the end away
    // from the baseline. Anchored at zero: a bar that starts elsewhere misstates its own length.
    const band = (W - L - R) / cats.length, bw = band * 0.78 / maps.length;
    cats.forEach((c, gi) => {
      const x0 = L + (gi + 0.5) * band - bw * maps.length / 2;
      maps.forEach((m, i) => {
        const p = panel.series[m].find(q => q[0] === c);
        if (!p) return;
        const y = py(p[1]), h = (H - B) - y, w = Math.max(bw - 2, 1), r = Math.min(3, w / 2, h);
        const bf = DATA.colors[m].dash ? `url(#h_${m})` : `var(--c-${m})`;
        g.push(`<path d="M${(x0 + i * bw + 1).toFixed(1)},${(H - B).toFixed(1)} L${(x0 + i * bw + 1).toFixed(1)},${(y + r).toFixed(1)} ` +
               `Q${(x0 + i * bw + 1).toFixed(1)},${y.toFixed(1)} ${(x0 + i * bw + 1 + r).toFixed(1)},${y.toFixed(1)} ` +
               `L${(x0 + i * bw + 1 + w - r).toFixed(1)},${y.toFixed(1)} Q${(x0 + i * bw + 1 + w).toFixed(1)},${y.toFixed(1)} ${(x0 + i * bw + 1 + w).toFixed(1)},${(y + r).toFixed(1)} ` +
               `L${(x0 + i * bw + 1 + w).toFixed(1)},${(H - B).toFixed(1)} Z" fill="${bf}"/>`);
      });
    });
  } else {
    for (const m of maps) {
      const d = pts(m).map(p => `${px(p[0]).toFixed(1)},${py(p[1]).toFixed(1)}`).join(" ");
      const dash = DATA.colors[m].dash ? ` stroke-dasharray="${DATA.colors[m].dash}"` : "";
      g.push(`<polyline points="${d}" fill="none" stroke="var(--c-${m})" stroke-width="2" stroke-linejoin="round" stroke-linecap="round"${dash} class="ln" data-map="${m}"/>`);
    }
  }
  g.push(`<line id="cross" x1="0" y1="${T}" x2="0" y2="${H - B}" stroke="var(--ink3)" stroke-width="1" opacity="0"/>`);
  g.push(`<g id="dots"></g>`);
  g.push(`<rect x="${L}" y="${T}" width="${W - L - R}" height="${H - T - B}" fill="transparent" id="hit"/>`);
  g.push(`</svg>`);
  host.insertAdjacentHTML("beforeend", g.join(""));
  host.insertAdjacentHTML("beforeend", '<div class="tip"></div>');

  const svg = host.querySelector("svg"), tip = host.querySelector(".tip");
  const cross = svg.querySelector("#cross"), dots = svg.querySelector("#dots");
  svg.addEventListener("pointerleave", () => { tip.style.opacity = 0; cross.setAttribute("opacity", 0); dots.innerHTML = ""; });
  svg.addEventListener("pointermove", (e) => {
    const box = svg.getBoundingClientRect();
    const vx = (e.clientX - box.left) / box.width * W;
    if (vx < L || vx > W - R) return;
    let best = null, bd = Infinity;
    for (const v of xs) { const d = Math.abs(px(v) - vx); if (d < bd) { bd = d; best = v; } }
    if (chart.bars) {
      const band = (W - L - R) / cats.length;
      cross.setAttribute("x1", px(best).toFixed(1)); cross.setAttribute("x2", px(best).toFixed(1));
      cross.setAttribute("stroke-width", band * 0.92); cross.setAttribute("opacity", 0.07);
    } else {
      cross.setAttribute("x1", px(best).toFixed(1)); cross.setAttribute("x2", px(best).toFixed(1));
      cross.setAttribute("stroke-width", 1); cross.setAttribute("opacity", 0.45);
    }
    const rows = [];
    dots.innerHTML = "";
    for (const m of maps) {
      const p = panel.series[m].find(q => q[0] === best);
      if (!p) continue;
      rows.push([m, p[1]]);
      if (!chart.bars) {
        dots.insertAdjacentHTML("beforeend",
          `<circle cx="${px(best).toFixed(1)}" cy="${py(p[1]).toFixed(1)}" r="3.5" fill="var(--c-${m})" stroke="var(--surface)" stroke-width="1.5"/>`);
      }
    }
    rows.sort((a, b) => a[1] - b[1]);
    const fastest = rows.length ? rows[0][1] : 1;
    tip.innerHTML = `<div class="x">${chart.xlabel === "entries" ? Number(best).toLocaleString() + " entries" : best + " bytes"}</div><table>` +
      rows.map(([m, v]) => `<tr><td><span class="sw" style="background:var(--c-${m})"></span>${DATA.colors[m].label}</td>` +
        `<td class="n">${fmt(v)} ${chart.unit}</td><td class="n" style="color:var(--ink3)">${v === fastest ? "&mdash;" : "&times;" + (v / fastest).toFixed(2)}</td></tr>`).join("") +
      `</table>`;
    tip.style.opacity = 1;
    const hostBox = host.getBoundingClientRect();
    const left = (px(best) / W) * hostBox.width + 14;
    tip.style.left = Math.max(0, Math.min(left, hostBox.width - tip.offsetWidth - 6)) + "px";
    // Sit opposite the pointer, so the tooltip never covers the point being read.
    const high = (e.clientY - box.top) / box.height < 0.5;
    tip.style.top = high ? (hostBox.height - tip.offsetHeight - 34) + "px" : "8px";
  });
}

function render() {
  const main = document.getElementById("charts");
  main.innerHTML = "";
  let group = null;
  DATA.charts.forEach((chart, ci) => {
    if (chart.group !== group) {
      group = chart.group;
      const sec = document.createElement("section");
      sec.className = "group";
      sec.id = chart.gid;
      sec.innerHTML = `<h2><a class="anchor" href="#${chart.gid}" title="Link to this section">${group}</a></h2>` +
                      `<p class="gsub measure">${DATA.groups[group] || ""}</p>`;
      main.appendChild(sec);
    }
    const fig = document.createElement("figure");
    // Each panel's own x extent, so a panel whose CSV stops an octave earlier -- the string sweeps
    // do -- fills its frame instead of trailing off at nine tenths of the width.
    const extent = p => {
      let a = Infinity, b = -Infinity;
      for (const s of Object.values(p.series)) for (const q of s) { if (q[0] < a) a = q[0]; if (q[0] > b) b = q[0]; }
      return [a, b];
    };
    // Two panels of one measurement over two size ranges, or two measurements over one range: the
    // CSV says which by whether the panels carry the same column name.
    const split = chart.panels.length === 2 && chart.panels[0].title !== chart.panels[1].title &&
                  chart.panels[0].title.includes("entries") && chart.panels[1].title.includes("sizes");
    fig.id = chart.id;
    fig.innerHTML = `<p class="ctitle"><a class="anchor" href="#${chart.id}" title="Link to this chart">${chart.title}</a></p>` +
      `<p class="csub">${chart.subtitle}</p>` +
      `<div class="panels">${chart.panels.map((p, i) => `<div><p class="ptitle">${p.title}</p><div class="panel" id="p${ci}_${i}"></div></div>`).join("")}</div>` +
      `<figcaption>` +
      (chart.warn ? `<div class="caution"><p><span class="lead">Caution.</span> ${chart.warn}</p></div>` : "") +
      `<div class="notes">` +
      (chart.what ? `<div><h3>What it measures</h3><p>${chart.what}</p></div>` : "") +
      (chart.why ? `<div><h3>Why it matters</h3><p>${chart.why}</p></div>` : "") +
      `</div></figcaption>`;
    main.appendChild(fig);
    const full = extent(chart.panels[0]);
    chart.panels.forEach((p, i) => {
      const dom = split ? (i === 0 ? [full[0], Math.min(65536, full[1])] : full) : extent(p);
      drawPanel(document.getElementById(`p${ci}_${i}`), chart, p, dom);
    });
  });
}

// The key and the top bar are two views of one state: whichever is on screen, the click works.
function setHidden(m, hide) {
  hide ? hidden.add(m) : hidden.delete(m);
  for (const el of document.querySelectorAll(`[data-map="${m}"][aria-pressed]`))
    el.setAttribute("aria-pressed", String(!hidden.has(m)));
  writeHash();
  render();
}

function buildControls() {
  const chips = document.getElementById("chips"), key = document.getElementById("key");
  for (const m of DATA.order) {
    const c = DATA.colors[m];
    const b = document.createElement("button");
    b.type = "button"; b.className = "chip"; b.dataset.map = m;
    b.innerHTML = `<span class="dot" style="${c.dash
      ? `background:repeating-linear-gradient(45deg,var(--c-${m}) 0 2px,transparent 2px 4px);border:1px solid var(--c-${m})`
      : `background:var(--c-${m})`}"></span>${c.label}`;
    b.setAttribute("aria-pressed", String(!hidden.has(m)));
    b.onclick = () => setHidden(m, !hidden.has(m));
    chips.appendChild(b);

    const li = document.createElement("li");
    const kb = document.createElement("button");
    kb.type = "button"; kb.dataset.map = m;
    kb.style.setProperty("--sw", `var(--c-${m})`);
    kb.innerHTML = `<span class="swatch${c.dash ? " dashed" : ""}"></span>` +
                   `<span class="name">${c.label}</span><span class="desc">${DATA.about[m] || ""}</span>`;
    kb.setAttribute("aria-pressed", String(!hidden.has(m)));
    kb.onclick = () => setHidden(m, !hidden.has(m));
    li.appendChild(kb); key.appendChild(li);
  }
}

// A heading click keeps whatever is hidden, which a plain href would drop.
addEventListener("click", (e) => {
  const a = e.target.closest && e.target.closest("a.anchor");
  if (!a) return;
  e.preventDefault();
  anchor = a.getAttribute("href").slice(1);
  writeHash();
  scrollToAnchor(true);
});
addEventListener("hashchange", () => { anchor = anchorOf(location.hash); scrollToAnchor(true); });

buildControls();
render();
scrollToAnchor(false);
addEventListener("resize", () => { clearTimeout(window._t); window._t = setTimeout(render, 120); });
</script>
</body>
</html>
"""

if __name__ == "__main__":
    sys.exit(main() or 0)
