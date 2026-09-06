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
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plot import SERIES  # noqa: E402  the one palette, validated once

DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "doc")


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
        "insert_erase_vs_size_str.csv", "memory_vs_size_str.csv", "memory_vs_value_size_str.csv")}
    vs = wide_value_size("value_size.csv")
    vs_str = wide_value_size("value_size_str.csv")

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
                   "uniformly and with the rng carrying on across epochs so no sequence repeats. "
                   "One lookup is a hash, a probe, and a comparison of the key that was found.",
              why="<b>The most discriminating lookup chart, and the one to decide by.</b> Every map "
                  "here has to do the same three things, so what differs is how many cache lines the "
                  "probe touches and how predictably it branches — and with the outcome fixed, "
                  "neither is masked by anything else. " + sawtooth),
        chart("Find, every lookup hitting", "nanoseconds per lookup, std::string keys",
              sized("find_hits_vs_size_str.csv"), "entries", "ns",
              resize_rows=r["find_hits_vs_size_str.csv"],
              what="The same, with keys of 8 to 135 bytes skewed towards short. One fixed length "
                   "would make the hash's length dispatch perfectly predictable and hide a third of "
                   "what a string lookup costs.",
              why="<b>The realistic case for most maps, and the one where the index matters least.</b> "
                  "Hashing is 33-38% of a string lookup and the comparison is a memcmp behind a "
                  "pointer the map has to chase, so the four maps converge: whatever the index does "
                  "well is diluted by work none of them can avoid. Worth having precisely because it "
                  "sets the ceiling on what a better index can buy a string map."),
        chart("Find, half the lookups hitting", "nanoseconds per lookup, uint64_t keys",
              sized("find_vs_size.csv"), "entries", "ns", resize_rows=r["find_vs_size.csv"],
              what="The same lookups, but each one decides by a coin flip whether to ask for a key "
                   "that is present or one that is not, from a pool that never was.",
              why="It is the shape of a real membership test, and the misprediction it adds is a real "
                  "cost that a program with an unpredictable hit rate really pays.",
              warn="<b>Do not decide anything on this chart.</b> A 50% hit rate is the maximum-entropy "
                   "point of the hit-rate curve: it adds about half a branch misprediction per lookup "
                   "to every map, which is a flat tax that compresses exactly the differences the "
                   "chart exists to show: at 26000 entries the four maps span 3.08x on the all-hits chart "
                   "and 2.11x here. Worse, it can <i>invert</i> their order: measured, this map is fastest on "
                   "hits (1.25x) and fastest on misses (1.25x) and still loses the 50% mix by 1.6% to "
                   "a map that is slower at both, because that map's probe already mispredicted 0.6 "
                   "times per lookup and an unpredictable outcome costs it nothing more. A number "
                   "that can rank two maps the opposite way from both of its own components is not a "
                   "summary of them. Use the all-hits chart above, and this one only to see what "
                   "outcome unpredictability costs."),
        chart("Find, half the lookups hitting", "nanoseconds per lookup, std::string keys",
              sized("find_vs_size_str.csv"), "entries", "ns", resize_rows=r["find_vs_size_str.csv"],
              what="The 50% mix on 8 to 135 byte keys.",
              why="Both effects at once, and they point the same way: the hash dilutes the index's "
                  "contribution and the coin flip compresses what is left.",
              warn="<b>The same caution as above, doubly.</b> Read the all-hits string chart instead."),
        chart("Churn at a fixed size", "nanoseconds per erase-and-insert pair, uint64_t keys",
              sized("churn_vs_size.csv"), "entries", "ns", resize_rows=r["churn_vs_size.csv"],
              what="Grow to n once, then forever erase a key that is present and insert one that is "
                   "not, so the size never changes and neither does the bucket count. The erased key "
                   "goes back into the spare pool and the inserted one takes its place, so no key is "
                   "built inside the timed region.",
              why="<b>The workload that separates designs rather than constant factors.</b> A table "
                  "that has churned for a long time is not the table you built: a design that frees a "
                  "slot without undoing what probed past it only degrades, and is relieved only by "
                  "growing. That is why boost swings 4.2-6.0x across a single octave here where "
                  "this map swings 1.2-1.5x — its overflow bits only ever get set, where the group "
                  "index's counters come back down on every erase. Nothing else on this page can "
                  "tell a long-lived table from a freshly built one. " + sawtooth),
        chart("Churn at a fixed size", "nanoseconds per erase-and-insert pair, std::string keys",
              sized("churn_vs_size_str.csv"), "entries", "ns", resize_rows=r["churn_vs_size_str.csv"],
              what="The same, on 8 to 135 byte keys.",
              why="Same property, smaller signal, and one extra thing to know: with string keys what "
                  "degrades under churn is partly the heap the key bodies live on rather than the "
                  "table, so this chart is measuring the allocator as well as the map."),
        chart("Insert and erase", "nanoseconds per operator[] and erase pair, uint64_t keys",
              sized("insert_erase_vs_size.csv"), "entries", "ns",
              resize_rows=r["insert_erase_vs_size.csv"],
              what="Four operations a round with the size invariant by construction: an operator[] "
                   "that finds, an erase that finds nothing, an erase that removes, and an "
                   "operator[] that inserts.",
              why="Largely the same story as churn, which is why the four-chart summary leaves it "
                  "out. It is here because half of its operations find nothing, so unlike churn it "
                  "pays for the miss path as well — and because operator[] is the call most programs "
                  "actually write. " + sawtooth),
        chart("Insert and erase", "nanoseconds per operator[] and erase pair, std::string keys",
              sized("insert_erase_vs_size_str.csv"), "entries", "ns",
              resize_rows=r["insert_erase_vs_size_str.csv"],
              what="The same, on 8 to 135 byte keys.",
              why="Mostly a check that nothing about a string key changes the ordering. If it ever "
                  "does, that is the interesting result."),
        chart("Build from empty, against mapped-value size",
              "nanoseconds per entry, 200000 entries, nothing reserved",
              [panel_of(vs, "build", "uint64_t keys"), panel_of(vs_str, "build", "std::string keys")],
              "sizeof(mapped_type), bytes", "ns", bars=True,
              what="Insert n entries into a default-constructed map, so the growth is included: about "
                   "half of a build is rehashing, and a map that grows badly would otherwise score "
                   "like one that grows well. Repeated across mapped values of 8 to 64 bytes.",
              why="<b>The axis that decides dense against flat.</b> A flat map writes the whole "
                  "value_type into a hash-scattered slot and moves it again on every rehash, so all "
                  "of its costs scale with the value; a dense map writes eight bytes there and "
                  "appends the value to a vector in order. Boost's line crosses above robin hood's "
                  "between 48 and 64 bytes. A suite that fixes the mapped type at size_t — as this one did "
                  "until September — ranks the two families wrongly for map&lt;Key, SomeStruct&gt;, "
                  "which is at least as common as map&lt;Key, size_t&gt;. It stops at 64 bytes "
                  "because 200000 entries of a 64 byte value is 14 MB and still in L3, where 128 is "
                  "27 MB and is not; past that cliff every line bends upward together and the chart "
                  "stops being about the value."),
        chart("One iteration pass, against mapped-value size",
              "nanoseconds per entry, 200000 entries",
              [panel_of(vs, "iterate", "uint64_t keys"), panel_of(vs_str, "iterate", "std::string keys")],
              "sizeof(mapped_type), bytes", "ns", bars=True,
              what="Walk every entry once and read one field of each, on a map built and then left "
                   "alone. Separate from the build chart because the two answer different questions "
                   "and differ by two orders of magnitude — on one axis together, the iteration "
                   "would be a flat line along the floor.",
              why="<b>The one place the dense layout wins outright, and by the largest margin on this "
                   "page.</b> A dense map iterates a contiguous vector; a flat map walks its whole "
                   "slot array and skips the empty ones, which at load 0.5 is half of what it "
                   "touches. That is 9.4x at an 8 byte integer-keyed value, narrowing to 2.2x at 64 as the "
                   "payload starts to dominate, and 3.7x with string keys, where the key bodies cost every map alike. If you iterate at all often, this chart is the argument."),
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
                  "but against the value it moves a lot: a flat map pays for its empty slots at the "
                  "full width of the value, a dense one pays four bytes of index for them. The peak "
                  "is the number a caller has to have room for and is where the gap is widest."),
        chart("Memory, against mapped-value size", "megabytes held for 200000 entries, std::string keys",
              [panel_of(r["memory_vs_value_size_str.csv"], "steady", "steady state"),
               panel_of(r["memory_vs_value_size_str.csv"], "peak", "peak during growth")],
              "sizeof(mapped_type), bytes", "MB", bars=True,
              what="The same count with string keys, which is why it counts global new rather than "
                   "the container's allocator: the key bodies are allocated by std::allocator&lt;char&gt; "
                   "inside each string, which a container allocator never sees.",
              why="Worth having because it is the case where the dense layout's memory advantage "
                  "mostly disappears. The key bodies are the same heap for every map, and the value "
                  "vector's capacity overshoots by up to 2x where a slot array is exactly its bucket "
                  "count, so the two effects nearly cancel. Reserve, and the overshoot goes away."),
        chart("Memory, against table size", "megabytes held, uint64_t keys",
              [panel_of(r["memory_vs_size.csv"], "steady", "steady state"),
               panel_of(r["memory_vs_size.csv"], "peak", "peak during growth")],
              "entries", "MB", ylog=True,
              what="The same count, walked over table size at a fixed size_t value.",
              why="Mostly a reference for reading a total off: per entry the picture is sixteen "
                  "near-identical octaves, which is exactly why the value-size chart above is the one "
                  "that discriminates. The staircase is the doubling."),
        chart("Memory, against table size", "megabytes held, std::string keys",
              [panel_of(r["memory_vs_size_str.csv"], "steady", "steady state"),
               panel_of(r["memory_vs_size_str.csv"], "peak", "peak during growth")],
              "entries", "MB", ylog=True,
              what="The same with string keys, the key bodies included.",
              why="Shows how much of a string map is the strings: most of it at small values, which "
                  "is the reason the four maps sit almost on top of each other."),
    ]
    charts = [c for c in charts if c]

    colors = {k: {"light": light, "dark": dark, "label": label} for k, label, light, dark in SERIES}
    order = [k for k, _, _, _ in SERIES]
    out = os.path.join(DOC, "charts.html")
    with open(out, "w") as f:
        css = ("  :root {" + "".join(f" --c-{k}: {v['light']};" for k, v in colors.items()) + " }\n"
               "  @media (prefers-color-scheme: dark) { :root {"
               + "".join(f" --c-{k}: {v['dark']};" for k, v in colors.items()) + " } }")
        f.write(PAGE.replace("__DATA__", json.dumps({"charts": charts, "colors": colors, "order": order}))
                    .replace("__SERIESCSS__", css))
    print(f"wrote {out}: {len(charts)} charts")


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
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>unordered_dense &mdash; measurements</title>
<style>
  :root {
    --surface: #fcfcfb; --ink: #0b0b0b; --ink2: #52514e; --ink3: #86847d;
    --grid: #e6e5e1; --rule: #dedcd6; --chip: #f1f0ec;
    /* Fill the monitor rather than a 1180px column, but stop before a panel gets so wide that its
       own height (the viewBox is 2.2:1) pushes the next chart off the screen. */
    --page: min(100%, 2600px);
    --bar: 52px;
    /* The one reserved status colour on the page, used for exactly one thing: a chart whose number
       should not be used to decide. Kept away from the four series hues on purpose. */
    --warn-ink: #a8442a; --warn-line: #e6c3b6; --warn-bg: #fdf3ef;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --surface: #1a1a19; --ink: #ffffff; --ink2: #c3c2b7; --ink3: #8b8a80;
      --grid: #343431; --rule: #2b2b29; --chip: #232322;
      --warn-ink: #f0a58c; --warn-line: #5a3a2e; --warn-bg: #2a1e19;
    }
  }
__SERIESCSS__
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: var(--bar) 24px 72px; background: var(--surface); color: var(--ink);
    font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
    font-feature-settings: "tnum" 1;
  }
  header { max-width: var(--page); margin: 0 auto; padding: 24px 0 20px; }
  h1 { font-size: 22px; font-weight: 650; letter-spacing: -0.01em; margin: 0 0 6px; }
  .lede { color: var(--ink2); font-size: 13px; line-height: 1.6; max-width: 76ch; margin: 0; }
  .filters {
    position: fixed; top: 0; left: 0; right: 0; z-index: 5;
    background: color-mix(in srgb, var(--surface) 92%, transparent);
    backdrop-filter: blur(8px); -webkit-backdrop-filter: blur(8px);
    border-bottom: 1px solid var(--rule); padding: 9px 24px;
  }
  .filters .row { max-width: var(--page); margin: 0 auto; display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
  .filters .hint { color: var(--ink3); font-size: 12px; margin-right: 4px; }
  button.chip {
    display: inline-flex; align-items: center; gap: 7px; cursor: pointer;
    border: 1px solid var(--rule); background: var(--chip); color: var(--ink);
    border-radius: 999px; padding: 5px 12px 5px 9px; font: inherit; font-size: 12.5px;
  }
  button.chip:focus-visible { outline: 2px solid #2a78d6; outline-offset: 2px; }
  button.chip .dot { width: 9px; height: 9px; border-radius: 50%; flex: none; }
  button.chip[aria-pressed="false"] { color: var(--ink3); }
  button.chip[aria-pressed="false"] .dot { background: none !important; box-shadow: inset 0 0 0 1.5px currentColor; }
  figure { max-width: var(--page); margin: 34px auto 0; padding: 0; }
  figcaption { margin-top: 14px; max-width: 96ch; }
  figcaption p { color: var(--ink2); font-size: 13px; line-height: 1.65; margin: 0 0 8px; }
  figcaption p.what { color: var(--ink3); }
  figcaption b { color: var(--ink); font-weight: 600; }
  .warn {
    display: flex; gap: 10px; align-items: flex-start; margin: 0 0 12px;
    border: 1px solid var(--warn-line); background: var(--warn-bg); border-radius: 8px;
    padding: 10px 12px; color: var(--ink2); font-size: 13px; line-height: 1.6;
  }
  .warnmark {
    flex: none; font-size: 10.5px; font-weight: 700; letter-spacing: .06em; text-transform: uppercase;
    color: var(--warn-ink); border: 1px solid var(--warn-line); border-radius: 4px; padding: 2px 6px;
    margin-top: 1px;
  }
  .warn b { color: var(--warn-ink); font-weight: 650; }
  .ctitle { font-size: 15.5px; font-weight: 650; margin: 0 0 2px; letter-spacing: -0.005em; }
  .csub { color: var(--ink2); font-size: 12.5px; margin: 0 0 10px; }
  .panels { display: grid; grid-template-columns: 1fr 1fr; gap: 34px; }
  @media (max-width: 900px) { .panels { grid-template-columns: 1fr; } }
  .panel { position: relative; }
  .ptitle { font-size: 12.5px; font-weight: 600; color: var(--ink2); margin: 0 0 4px 44px; }
  svg { display: block; width: 100%; height: auto; touch-action: none; }
  .tip {
    position: absolute; pointer-events: none; opacity: 0; transition: opacity .08s;
    background: var(--surface); border: 1px solid var(--rule); border-radius: 7px;
    padding: 7px 9px; font-size: 12px; box-shadow: 0 4px 14px rgba(0,0,0,.13); white-space: nowrap;
  }
  .tip .x { color: var(--ink3); margin-bottom: 4px; }
  .tip table { border-collapse: collapse; }
  .tip td { padding: 1px 0; }
  .tip td.n { text-align: right; padding-left: 12px; font-variant-numeric: tabular-nums; }
  .tip .sw { width: 8px; height: 8px; border-radius: 50%; display: inline-block; margin-right: 6px; }
  .empty { color: var(--ink3); font-size: 13px; padding: 30px 0 0 44px; }
  footer { max-width: var(--page); margin: 56px auto 0; color: var(--ink3); font-size: 12px; line-height: 1.7; }
</style>
</head>
<body>
<header>
  <h1>ankerl::unordered_dense &mdash; measurements</h1>
  <p class="lede">Every alternative is timed interleaved with the others in one process, so a clock
  ramp or a noisy neighbour hits all of them and cancels out of the comparison. Nothing is reserved,
  and the tables are sampled twelve times per octave, so the load-factor sawtooth between doublings
  is visible rather than aliased away. Ryzen&nbsp;9&nbsp;7950X, clang&nbsp;22.</p>
</header>

<div class="filters"><div class="row" id="filters"><span class="hint">Click to show or hide, everywhere:</span></div></div>
<main id="charts"></main>

<footer>
  Absolute times are the median epoch. Two runs of the same sweep on a quiet machine agree to 0.78%
  at the median point and 9.3% at the worst, so read the shape rather than the third digit, and check
  a surprising point against a second run. The sweeps stop at a million entries because a single
  incremental pass above that depends on page placement that varies between runs.
</footer>

<script>
const DATA = __DATA__;
// Which series are hidden lives in the URL, so a view of the data is a link you can send someone --
// "#hide=boost" is "the three unordered_dense lines, rescaled to fill the panel".
const hidden = new Set((new URLSearchParams(location.hash.slice(1)).get("hide") || "")
  .split(",").filter(m => DATA.colors[m]));

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

function niceTicks(max, min) {
  const steps = [0.05, 0.1, 0.2, 0.25, 0.5, 1, 2, 2.5, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 5000];
  for (const s of steps) if ((max - min) / s <= 6) {
    const out = [];
    for (let v = Math.floor(min / s) * s; v <= max + s * 1e-9; v += s) out.push(+v.toFixed(10));
    return out;
  }
  return [min, max];
}

// One panel: axes, grid, the visible lines, and a crosshair that reads values off them.
function drawPanel(host, chart, panel, xDomain) {
  const W = 900, H = 418, L = 54, R = 14, T = 32, B = 40;
  const maps = DATA.order.filter(m => panel.series[m] && !hidden.has(m));
  host.innerHTML = "";
  if (!maps.length) { host.innerHTML = '<div class="empty">every series hidden</div>'; return; }

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
  g.push(`<svg viewBox="0 0 ${W} ${H}" preserveAspectRatio="xMidYMid meet" role="img">`);
  for (const t of ticks) {
    const y = py(logy ? Math.pow(10, t) : t);
    g.push(`<line x1="${L}" y1="${y.toFixed(1)}" x2="${W - R}" y2="${y.toFixed(1)}" stroke="var(--grid)" stroke-width="1"/>`);
    g.push(`<text x="${L - 8}" y="${(y + 4).toFixed(1)}" text-anchor="end" font-size="13" fill="var(--ink3)">${logy ? tick(Math.pow(10, t)) : tick(t)}</text>`);
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
  } else {
    xticks = [];
    for (let v = Math.pow(2, Math.ceil(Math.log2(xDomain[0]))); v <= xDomain[1]; v *= 8) xticks.push(v);
  }
  for (const v of xticks)
    g.push(`<text x="${px(v).toFixed(1)}" y="${H - B + 18}" text-anchor="middle" font-size="13" fill="var(--ink3)">${chart.xlabel === "entries" ? si(v) : v}</text>`);
  g.push(`<text x="${(L + (W - R - L) / 2).toFixed(1)}" y="${H - 6}" text-anchor="middle" font-size="13" fill="var(--ink3)">${chart.xlabel}</text>`);
  // Above the plot area rather than beside it: at T - 6 it landed on the topmost y tick.
  g.push(`<text x="${L - 8}" y="${T - 16}" text-anchor="end" font-size="13" fill="var(--ink3)">${chart.unit}</text>`);

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
        g.push(`<path d="M${(x0 + i * bw + 1).toFixed(1)},${(H - B).toFixed(1)} L${(x0 + i * bw + 1).toFixed(1)},${(y + r).toFixed(1)} ` +
               `Q${(x0 + i * bw + 1).toFixed(1)},${y.toFixed(1)} ${(x0 + i * bw + 1 + r).toFixed(1)},${y.toFixed(1)} ` +
               `L${(x0 + i * bw + 1 + w - r).toFixed(1)},${y.toFixed(1)} Q${(x0 + i * bw + 1 + w).toFixed(1)},${y.toFixed(1)} ${(x0 + i * bw + 1 + w).toFixed(1)},${(y + r).toFixed(1)} ` +
               `L${(x0 + i * bw + 1 + w).toFixed(1)},${(H - B).toFixed(1)} Z" fill="var(--c-${m})"/>`);
      });
    });
  } else {
    for (const m of maps) {
      const d = pts(m).map(p => `${px(p[0]).toFixed(1)},${py(p[1]).toFixed(1)}`).join(" ");
      g.push(`<polyline points="${d}" fill="none" stroke="var(--c-${m})" stroke-width="2" stroke-linejoin="round" stroke-linecap="round" class="ln" data-map="${m}"/>`);
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
  DATA.charts.forEach((chart, ci) => {
    const fig = document.createElement("figure");
    const allx = Object.values(chart.panels[0].series)[0].map(p => p[0]);
    const full = [Math.min(...allx), Math.max(...allx)];
    // Two panels of one measurement over two size ranges, or two measurements over one range: the
    // CSV says which by whether the panels carry the same column name.
    const split = chart.panels.length === 2 && chart.panels[0].title !== chart.panels[1].title &&
                  chart.panels[0].title.includes("entries") && chart.panels[1].title.includes("sizes");
    fig.innerHTML = `<p class="ctitle">${chart.title}</p><p class="csub">${chart.subtitle}</p>` +
      `<div class="panels">${chart.panels.map((p, i) => `<div><p class="ptitle">${p.title}</p><div class="panel" id="p${ci}_${i}"></div></div>`).join("")}</div>` +
      `<figcaption>` +
      (chart.warn ? `<div class="warn"><span class="warnmark">avoid</span><div>${chart.warn}</div></div>` : "") +
      (chart.what ? `<p class="what">${chart.what}</p>` : "") +
      (chart.why ? `<p class="why">${chart.why}</p>` : "") + `</figcaption>`;
    main.appendChild(fig);
    chart.panels.forEach((p, i) => {
      const dom = split ? (i === 0 ? [full[0], Math.min(65536, full[1])] : full) : full;
      drawPanel(document.getElementById(`p${ci}_${i}`), chart, p, dom);
    });
  });
}

function buildFilters() {
  const row = document.getElementById("filters");
  for (const m of DATA.order) {
    const b = document.createElement("button");
    b.className = "chip";
    b.innerHTML = `<span class="dot" style="background:var(--c-${m})"></span>${DATA.colors[m].label}`;
    b.setAttribute("aria-pressed", hidden.has(m) ? "false" : "true");
    b.onclick = () => {
      hidden.has(m) ? hidden.delete(m) : hidden.add(m);
      b.setAttribute("aria-pressed", hidden.has(m) ? "false" : "true");
      history.replaceState(null, "", hidden.size ? "#hide=" + [...hidden].join(",") : location.pathname);
      render();
    };
    row.appendChild(b);
  }
}

buildFilters();
render();
addEventListener("resize", () => { clearTimeout(window._t); window._t = setTimeout(render, 120); });
</script>
</body>
</html>
"""

if __name__ == "__main__":
    main()
