#!/usr/bin/env python3
"""Draw scripts/ab/maps.cpp's CSVs as SVGs.

Sixteen maps and seven workloads is a table, not a chart -- so the tables go in the post and these
charts carry the three or four questions worth seeing shaped: how each map compares with the group
index on the workloads that separate the designs, what an entry costs in memory, and what the
load-factor sawtooth does inside one octave.

Bars are horizontal because sixteen map names do not fit under a vertical axis, grouped by panel,
anchored at zero, with the reference map at 1.00 marked. Colour is the *family* -- flat, dense,
node -- because that is the split the chart is about; it is never the rank. Three hues from the
same validated categorical set plot.py uses.

Stdlib only.

    scripts/ab/mapsplot.py bars   <csv> <key> <base> <out.svg> <workload[:title]>...
    scripts/ab/mapsplot.py readme <csv> <key> <base> <out.svg> <workload[:title]>...
    scripts/ab/mapsplot.py memory <csv> <key> <base> <out.svg>
    scripts/ab/mapsplot.py octave <maps.txt> <key> <base> <workload> <out.svg> <map>...
    scripts/ab/mapsplot.py hashlat <hash_others-sweep.csv> <out.svg> [y-max]
    scripts/ab/mapsplot.py table  <csv> <key> <base>            markdown, ratios to the group index
    scripts/ab/mapsplot.py htmltable <csv> <key> <base>         the same, tinted by distance from parity
    scripts/ab/mapsplot.py swing  <maps.txt> <key> <base> <workload>   dearest / cheapest point
    scripts/ab/mapsplot.py merge  <out.csv> <csv>...   geometric mean of independent runs
"""
import csv
import math
import sys
from collections import defaultdict

# family -> (light, dark). Three hues, validated as a categorical set against both surfaces with
# dataviz/scripts/validate_palette.js: adjacent CVD separation 11.0 deuteranopia and 6.5 tritanopia,
# normal-vision floor 23.4, and every one of them at or above 3:1 against the page, which the
# brighter set they replaced was not -- its green sat at 2.74:1 and read as washed out on white.
FAMILY = {
    "flat": ("#2563c9", "#5b9bf0"),
    "dense": ("#0e8f60", "#3cc492"),
    "node": ("#a53393", "#e070cd"),
}
OF = {
    "udm": "dense", "udm-4.11": "dense", "emhash8": "dense", "f14-vector": "dense", "ihtab": "dense",
    "udm-segmented": "dense", "udm-huge": "dense", "udm-seg-huge": "dense",
    "boost": "flat", "boost-own": "flat", "absl": "flat", "absl-own": "flat", "f14-value": "flat",
    "emilib": "flat", "indivi-u": "flat", "indivi-w": "flat", "verstable": "flat",
    "std": "node", "boost-node": "node", "absl-node": "node", "f14-node": "node",
}
PRETTY = {
    "udm": "unordered_dense 5.0", "udm-4.11": "unordered_dense 4.11", "boost": "boost flat",
    "udm-segmented": "unordered_dense segmented",
    "udm-huge": "unordered_dense, huge pages", "udm-seg-huge": "unordered_dense segmented + huge",
    "boost-own": "boost flat, own hash", "absl": "absl flat", "absl-own": "absl flat, own hash",
    "f14-value": "F14Value", "f14-vector": "F14Vector", "f14-node": "F14Node",
    "emhash8": "emhash8", "emilib": "emilib", "indivi-u": "indivi flat_umap",
    "indivi-w": "indivi flat_wmap", "verstable": "Verstable", "ihtab": "ihtab",
    "std": "std::unordered_map", "boost-node": "boost node", "absl-node": "absl node",
}
# One hue per *map* rather than per family, for the charts where two lines are the same family and
# the family colouring would make them indistinguishable. Validated as a categorical set over all
# pairs: worst CVD separation 8.5 deuteranopia, normal-vision floor 20.3, all four at or above 3:1
# against the page.
LINE = ["#2563c9", "#0e8f60", "#b45309", "#a53393"]

FONT = ("Roboto Condensed, Noto Sans Condensed, DejaVu Sans Condensed, Liberation Sans Narrow, "
        "Arial Narrow, Avenir Next Condensed, Inter, system-ui, sans-serif")

REF = "udm"

W = 920
LABEL = 152
ROWH = 17
GAP = 16


# Light values are ordinary attributes so that any renderer shows the chart; the stylesheet only
# overrides them for dark mode. GitHub sanitizes an SVG it serves as an image and can drop the
# <style>, and a surface rect whose only fill lives there renders as a black rectangle over
# everything -- which is the same trap scripts/ab/plot.py documents at its own head().
def head(w, h, surface=False):
    # A condensed family, so that eighteen map names fit beside five panels. An SVG in an <img>
    # can only use fonts the *reader* has, so the stack names one condensed face per platform --
    # Roboto/Noto on Linux, Arial Narrow on Windows, Avenir Next Condensed on macOS -- and the
    # gutters below are still sized for the plain fallback, which is wider.
    s = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" '
         f'font-family="{FONT}" font-size="12">\n'
         '  <style>\n'
         '    .ax{stroke:#e5e7eb;stroke-width:1}\n'
         '    .base{stroke:#9ca3af;stroke-width:1.2}\n'
         '    .ref{stroke:#4b5563;stroke-width:1.2;stroke-dasharray:4 3}\n'
         '    .t{fill:#1f2937;font-size:12.5px}\n'
         '    .m{fill:#4b5563;font-size:11.5px}\n'
         '    .hd{fill:#111827;font-weight:600;font-size:13.5px}\n'
         '    .sub{fill:#4b5563;font-size:12px}\n')
    if surface:
        # For a chart that goes in the README rather than the post: GitHub's dark theme darkens the
        # page under an <img>, and every colour above is ink for a light one -- the title would
        # render #111827 on #0d1117, which is 1.2:1. So the chart brings its own background and
        # switches both with the reader's scheme, the way scripts/ab/plot.py does. The light surface
        # is #ffffff, which is exactly the blog page's colour, so a chart drawn with this on and
        # shown there is unchanged.
        light = "".join(f".f-{f}{{fill:{c[0]}}}" for f, c in FAMILY.items())
        dark = "".join(f".f-{f}{{fill:{c[1]}}}" for f, c in FAMILY.items())
        s += ('    .surface{fill:#ffffff}\n'
              f'    {light}\n'
              '    @media (prefers-color-scheme: dark){\n'
              '      .surface{fill:#0d1117}\n'
              '      .ax{stroke:#30363d}.base{stroke:#6e7681}.ref{stroke:#9ca3af}\n'
              '      .t{fill:#e6edf3}.m{fill:#9ca3af}.hd{fill:#f0f6fc}.sub{fill:#9ca3af}\n'
              f'      {dark}\n'
              '    }\n')
    s += '  </style>\n'
    if surface:
        s += f'  <rect class="surface" fill="#ffffff" width="{w}" height="{h}"/>\n'
    return s


# These charts carried a `@media (prefers-color-scheme: dark)` block until 2026-09-08, and it made
# them unreadable for anyone whose OS is set to dark. An SVG referenced from an <img> follows the
# *reader's* colour scheme, not the surrounding page's, and the page these go on is a blog whose
# background is hard-coded #FFFFFF -- so in dark mode the title rendered #f9fafb on white, which is
# 1.05:1, and the labels #9ca3af, which is 2.54:1. Verified both ways with
# `--blink-settings=preferredColorScheme=0|1`. Put the block back only alongside a surface rect that
# switches with it, or a page that does. doc/'s charts keep theirs, because GitHub's own dark mode
# darkens the page underneath them.


def bar(x, y, w, h, r=2.5):
    r = min(r, w / 2 if w > 0 else 0, h / 2)
    if w <= 0:
        return ""
    return (f'M{x:.1f},{y:.1f} L{x + w - r:.1f},{y:.1f} Q{x + w:.1f},{y:.1f} {x + w:.1f},{y + r:.1f} '
            f'L{x + w:.1f},{y + h - r:.1f} Q{x + w:.1f},{y + h:.1f} {x + w - r:.1f},{y + h:.1f} '
            f'L{x:.1f},{y + h:.1f} Z')


def torn(x, y, w, h, teeth=3):
    """A bar whose right edge is a zigzag: it ran past the axis and was clipped there."""
    if w <= 0:
        return ""
    step = h / teeth
    pts = [f"M{x:.1f},{y:.1f}", f"L{x + w:.1f},{y:.1f}"]
    for i in range(teeth):
        pts.append(f"L{x + w - 7:.1f},{y + step * (i + 0.5):.1f}")
        pts.append(f"L{x + w:.1f},{y + step * (i + 1):.1f}")
    pts.append(f"L{x:.1f},{y + h:.1f} Z")
    return " ".join(pts)


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def ticks(vmax):
    for step in (0.25, 0.5, 1, 2, 5, 10, 20, 50, 100):
        if vmax / step <= 5:
            return [i * step for i in range(int(vmax / step) + 2)]
    return [0, vmax]


def read(path):
    rows = []
    for r in csv.reader(open(path)):
        if len(r) == 6:
            rows.append({"key": r[0], "work": r[1], "base": int(r[2]), "map": r[3],
                         "a": float(r[4]), "b": float(r[5])})
    return rows


def panels(out, title, sub, order, cols, vals, unit, ref_line=True, surface=False, cap=None,
           width=W, label=None, rank=None):
    """cols: list of (key, heading). vals: dict (map, key) -> value. Bars are drawn to scale."""
    n = len(order)
    # ~6.8px per glyph at 12.5px, which is the plain fallback rather than the condensed face: a
    # reader without any of the condensed families still gets names that fit.
    gut = (46 if rank else 8) + 10
    label = label or max(len(PRETTY.get(m, m)) for m in order) * 6.8 + gut
    pw = (width - label - GAP * len(cols)) / len(cols)
    top = 86
    h = top + n * ROWH + 58
    s = head(width, h, surface)
    s += f'  <text x="8" y="20" class="hd" fill="#111827">{esc(title)}</text>\n'
    s += f'  <text x="8" y="38" class="sub" fill="#4b5563">{esc(sub)}</text>\n'
    for ci, (ck, ch) in enumerate(cols):
        x0 = label + ci * (pw + GAP)
        vmax = max(vals.get((m, ck), 0.0) for m in order) * 1.02
        # A panel whose worst map is two orders of magnitude off the rest turns every bar a reader is
        # actually choosing between into a sliver: iterate runs from 0.85 to 178. Past `cap` the
        # choice is already made, so the axis stops there and the bars that ran past it are drawn
        # with a torn edge. Every bar carries its true number either way.
        col_cap = (cap or {}).get(ck)
        capped = col_cap is not None and vmax > col_cap
        if capped:
            vmax = col_cap
        tk = ticks(vmax)
        if capped:
            tk = [t for t in tk if t <= vmax] or [0, vmax]
            if tk[-1] < vmax:
                tk.append(vmax)
        # Room for the widest value label, so that no label ever has to be drawn inside its bar:
        # white on a bar is 3.0 to 4.1:1 against the page and unreadable at 11px, and there is no
        # ink dark enough for the inside of a mid-tone bar either. ~6.4px per glyph at 11.5px Inter.
        lblw = max(len(f"{vals.get((m, ck), 0.0):.2f}") for m in order) * 6.4 + 9
        scale = (pw - lblw) / max(tk[-1], 1e-9)
        s += f'  <text x="{x0:.0f}" y="{top - 30:.0f}" class="t" fill="#1f2937" font-weight="600">{esc(ch)}</text>\n'
        for t in tk:
            x = x0 + t * scale
            if x > x0 + pw + 1:
                continue
            s += f'  <line x1="{x:.1f}" y1="{top - 12}" x2="{x:.1f}" y2="{top + n * ROWH}" class="ax" stroke="#e5e7eb"/>\n'
            s += (f'  <text x="{x:.1f}" y="{top - 14}" class="m" fill="#4b5563" text-anchor="middle">'
                  f'{t:g}</text>\n')
        if ref_line:
            x = x0 + 1.0 * scale
            s += f'  <line x1="{x:.1f}" y1="{top - 12}" x2="{x:.1f}" y2="{top + n * ROWH}" class="ref" stroke="#4b5563" stroke-dasharray="4 3"/>\n'
        s += (f'  <line x1="{x0:.1f}" y1="{top - 12}" x2="{x0:.1f}" y2="{top + n * ROWH}" '
              f'class="base" stroke="#9ca3af"/>\n')
        for i, m in enumerate(order):
            v = vals.get((m, ck))
            if v is None:
                continue
            y = top + i * ROWH + 2
            fam = OF[m]
            over = col_cap is not None and v > col_cap
            w = (col_cap if over else v) * scale
            shape = torn(x0, y, w, ROWH - 5) if over else bar(x0, y, w, ROWH - 5)
            s += (f'  <path d="{shape}" class="f-{fam}"'
                  f' fill="{FAMILY[fam][0]}" opacity="{0.95 if m == REF else 0.8}"/>\n')
            s += (f'  <text x="{x0 + w + 6:.1f}" y="{y + ROWH - 9:.0f}" class="m" fill="#4b5563" '
                  f'text-anchor="start">{v:.2f}</text>\n')
    name_x = label - (46 if rank else 8)
    if rank:
        s += f'  <text x="{label - 8}" y="{top - 30:.0f}" class="m" fill="#4b5563" text-anchor="end">geomean</text>\n'
    for i, m in enumerate(order):
        y = top + i * ROWH + ROWH - 7
        s += (f'  <text x="{name_x}" y="{y:.0f}" class="t" fill="#1f2937" text-anchor="end"'
              f'{" font-weight=\'600\'" if m == REF else ""}>{esc(PRETTY.get(m, m))}</text>\n')
        if rank:
            s += (f'  <text x="{label - 8}" y="{y:.0f}" class="m" fill="#4b5563" text-anchor="end">'
                  f'{rank[m]:.2f}</text>\n')
    ly = top + n * ROWH + 26
    lx = label
    for fam in [f for f in ("flat", "dense", "node") if any(OF[m] == f for m in order)]:
        s += (f'  <rect x="{lx}" y="{ly - 9}" width="11" height="11" class="f-{fam}" '
              f'fill="{FAMILY[fam][0]}" opacity="0.85"/>\n')
        s += f'  <text x="{lx + 16}" y="{ly}" class="m" fill="#4b5563">{fam}</text>\n'
        lx += 74
    s += f'  <text x="{lx + 10}" y="{ly}" class="m" fill="#4b5563">{esc(unit)}</text>\n'
    s += "</svg>\n"
    open(out, "w").write(s)
    print(out)


def order_for(rows, key, base, work):
    got = {r["map"]: r for r in rows if r["key"] == key and r["base"] == base and r["work"] == work}
    return [m for m in sorted(got, key=lambda m: got[m]["b"]) if m in OF]


def panel_args(argv):
    """<csv> <key> <base> <out.svg> [--only=a,b] [--drop=a,b] [--title=..] <workload[:title]>...

    Everything both bar commands need. --only and --drop pick which maps this chart is about: the
    README draws the same CSV twice, once against the other libraries and once against this map's
    own shapes, and a row that is in both charts would say the same thing twice.
    """
    flags = {}
    rest = []
    for a in argv:
        if a.startswith("--") and "=" in a:
            k, _, v = a[2:].partition("=")
            flags[k] = v
        else:
            rest.append(a)
    csvp, key, base, out = rest[0], rest[1], int(rest[2]), rest[3]
    works = [w.split(":") for w in rest[4:]]
    only = set(flags["only"].split(",")) if "only" in flags else None
    drop = set(flags["drop"].split(",")) if "drop" in flags else set()
    vals = {}
    present = set()
    for r in read(csvp):
        if r["key"] != key or r["base"] != base:
            continue
        if (only is not None and r["map"] not in only) or r["map"] in drop:
            continue
        vals[(r["map"], r["work"])] = r["b"]
        present.add(r["map"])
    cols = [(w[0], w[1] if len(w) > 1 else w[0]) for w in works]
    return out, key, base, works, cols, vals, present, flags


def cmd_bars(argv):
    out, key, base, works, cols, vals, present, _ = panel_args(argv)
    order = sorted(present, key=lambda m: vals.get((m, works[0][0]), 9e9))
    panels(out, f"relative to unordered_dense 5.0, {key} keys",
           f"geometric mean over one octave from {base:,} entries; lower is faster",
           order, cols, vals,
           "time relative to unordered_dense 5.0, so 1.00 is level with it")


def cmd_readme(argv):
    """The README's graph: every map in its own default configuration, five panels, one key type.

    Separate from `bars` because the captions are the difference between the two questions. `bars`
    draws maps that were all handed the same hash, so its panels are about indexes; this one draws
    what a caller gets by typing the type name, hash included, and one of its panels is bytes rather
    than nanoseconds -- so "lower is faster" would be wrong on it.
    """
    out, key, base, works, cols, vals, present, flags = panel_args(argv)
    # Ordered by the geometric mean of all five panels rather than by the first one, so that the
    # chart reads top to bottom as the ranking a reader is looking for. Ordering by one panel makes
    # the four beside it look shuffled, and which panel gets to do the ordering is arbitrary.
    def overall(m):
        rs = [vals[(m, w[0])] for w in works if (m, w[0]) in vals]
        return math.exp(sum(math.log(r) for r in rs) / len(rs)) if rs else 9e9

    order = sorted(present, key=overall)
    keyname = "uint64_t" if key == "u64" else "std::string"
    what = flags.get("title", "every map in its default configuration")
    panels(out, f"relative to unordered_dense 5.0, {keyname} keys, {what}",
           f"geometric mean over one octave from {base:,} entries; lower is better, and 1.00 is level "
           f"with unordered_dense 5.0. Rows are sorted by the geomean of all five panels; a bar torn "
           f"off at the right ran past the axis.",
           order, cols, vals,
           flags.get("unit", "relative to unordered_dense 5.0: time for the first four panels, "
                                "peak bytes per entry for the last"),
           surface=True, width=1030,
           # Four times the reference is where the choice between two maps is already made, for the
           # panels whose spread is narrow. build and iterate get ten, because both carry a node map
           # far enough out that a four-times axis tears every one of them off at the same place and
           # says nothing about which is worse.
           cap={w[0]: (10.0 if w[0] in ("build", "iterate") else 4.0) for w in works},
           rank={m: overall(m) for m in order})


def cmd_memory(argv):
    csvp, key, base, out = argv[0], argv[1], int(argv[2]), argv[3]
    rows = read(csvp)
    vals = {}
    present = set()
    for r in rows:
        if r["key"] == key and r["base"] == base and r["work"] == "memory":
            vals[(r["map"], "steady")] = r["a"]
            vals[(r["map"], "churned")] = r["b"]
            present.add(r["map"])
    order = sorted(present, key=lambda m: vals[(m, "steady")])
    what = {"u64": "uint64_t key, 8 byte value", "str": "std::string key, 8 byte value",
            "big": "uint64_t key, 64 byte value"}.get(key, key)
    panels(out, f"bytes per entry, {what}",
           f"geometric mean over one octave from {base:,} entries, from a counting allocator",
           order, [("steady", "after building"), ("churned", "after churning")], vals,
           "bytes per live entry", ref_line=False)


def cmd_octave(argv):
    """The sawtooth: cost against table size across one doubling, from the run's stdout.

    Drawn from a fine sweep -- fifty sizes across the octave -- with the handful of sizes the
    reported geometric means are actually taken at marked as larger dots, so that the picture says
    both what the curve does and how coarsely it is sampled everywhere else in the post.
    """
    txt, key, base, work, out = argv[0], argv[1], int(argv[2]), argv[3], argv[4]
    stride = int(argv[5])
    want = argv[6:]
    series = defaultdict(list)
    inside = False
    for line in open(txt):
        if line.startswith("=="):
            inside = f"{key} octave starting at {base} " in line
            continue
        if not inside or not line.startswith(f"  {work} "):
            continue
        parts = line.split()
        n = int(parts[1].split("=")[1])
        for p in parts[2:]:
            m, v = p.split("=")
            if m in want:
                series[m].append((n, float(v)))
    if not series:
        print(f"no data for {work} in {txt}", file=sys.stderr)
        return
    W2, H2 = 900, 430
    L, R, T, B = 64, 210, 92, 58
    xs = sorted({n for s in series.values() for n, _ in s})
    lo, hi = xs[0], xs[-1]
    vmax = max(v for s in series.values() for _, v in s) * 1.06
    tk = ticks(vmax)
    s = head(W2, H2)
    s += (f'  <text x="8" y="22" class="hd" fill="#111827">{esc(work)} against table size, across one doubling and '
          f'a little past it, {key} keys</text>\n')
    s += (f'  <text x="8" y="42" class="sub" fill="#4b5563">the same {len(xs)} sizes for every map, {lo:,} to '
          f'{hi:,} entries; the large dots are the five sizes every ratio in this post is '
          f'averaged over</text>\n')

    def px(n):
        return L + (math.log2(n) - math.log2(lo)) / (math.log2(hi) - math.log2(lo)) * (W2 - L - R)

    def py(v):
        return T + (1 - v / tk[-1]) * (H2 - T - B)

    for t in tk:
        s += f'  <line x1="{L}" y1="{py(t):.1f}" x2="{W2 - R}" y2="{py(t):.1f}" class="ax" stroke="#e5e7eb"/>\n'
        s += f'  <text x="{L - 8}" y="{py(t) + 4:.1f}" class="m" fill="#4b5563" text-anchor="end">{t:g}</text>\n'
    s += f'  <line x1="{L}" y1="{py(0):.1f}" x2="{W2 - R}" y2="{py(0):.1f}" class="base" stroke="#9ca3af"/>\n'
    s += f'  <text x="{L}" y="{T - 14}" class="m" fill="#4b5563">nanoseconds per operation</text>\n'
    for i, n in enumerate(xs):
        if i % stride and i != len(xs) - 1:
            continue
        s += (f'  <text x="{px(n):.1f}" y="{H2 - B + 18:.0f}" class="m" fill="#4b5563" text-anchor="middle">'
              f'{n:,}</text>\n')
        s += (f'  <line x1="{px(n):.1f}" y1="{py(0):.1f}" x2="{px(n):.1f}" y2="{py(0) + 4:.1f}" '
              f'class="base" stroke="#9ca3af"/>\n')
    s += f'  <text x="{(L + W2 - R) / 2:.0f}" y="{H2 - 14}" class="m" fill="#4b5563" text-anchor="middle">entries</text>\n'

    ends = []
    for m in want:
        pts = sorted(series.get(m, []))
        if not pts:
            continue
        col = LINE[want.index(m) % len(LINE)]
        d = " ".join(("M" if i == 0 else "L") + f"{px(n):.1f},{py(v):.1f}" for i, (n, v) in enumerate(pts))
        s += f'  <path d="{d}" fill="none" stroke="{col}" stroke-width="1.8" stroke-linejoin="round"/>\n'
        for i, (n, v) in enumerate(pts):
            # only the sizes inside the octave itself; the points past it are there to show the far
            # side of the doubling and are not what any ratio is averaged over
            if i % stride == 0 and n < lo * 2:
                s += (f'  <circle cx="{px(n):.1f}" cy="{py(v):.1f}" r="5" fill="{col}" '
                      f'stroke="#ffffff" stroke-width="1.6"/>\n')
        ends.append((py(pts[-1][1]), px(pts[-1][0]), col, PRETTY.get(m, m)))
    ends.sort()
    last = -1e9
    for y, x, col, label in ends:
        y = max(y, last + 16)
        last = y
        s += (f'  <line x1="{x + 8:.1f}" y1="{y:.1f}" x2="{x + 22:.1f}" y2="{y:.1f}" '
              f'stroke="{col}" stroke-width="2.4" stroke-linecap="round"/>\n')
        s += f'  <text x="{x + 28:.1f}" y="{y + 4:.1f}" class="m" fill="#4b5563">{esc(label)}</text>\n'
    s += "</svg>\n"
    open(out, "w").write(s)
    print(out)


def cmd_hashlat(argv):
    """Latency against key length, for every string hash in the comparison.

    A hash dispatches on length, so its cost is a staircase and one number over a mix of lengths
    hides the shape. Latency rather than throughput because that is what a *map* pays: the hash's
    result is the address of the group to probe, so nothing after it can start.

    The two unordered_dense versions share a hue and differ by dash -- colour for the library,
    style for the version, the same convention the size charts use for boost's two hashes -- which
    keeps the categorical set at the four validated hues.
    """
    src, out = argv[0], argv[1]
    ymax = float(argv[2]) if len(argv) > 2 else 0.0
    series = defaultdict(list)
    for r in csv.DictReader(open(src)):
        series[r["hash"]].append((int(r["bytes"]), float(r["latency_ns"])))
    order = ["udm5", "udm4", "absl", "boost", "folly"]
    style = {
        "udm5": (FAMILY["dense"][0], "none", "unordered_dense 5.0"),
        "udm4": (FAMILY["dense"][0], "6 4", "unordered_dense 4.11.0"),
        "absl": (LINE[0], "none", "absl::Hash"),
        "boost": (LINE[2], "none", "boost::hash"),
        "folly": (LINE[3], "none", "folly::hasher"),
    }
    W2, H2 = 920, 470
    L, R, T, B = 58, 178, 96, 62
    lo, hi = 4, 1024
    # The x axis is logarithmic, because a key length is exponential over the useful range. The y
    # axis is linear and starts at zero, so that a distance on it is a number of nanoseconds rather
    # than a ratio -- which costs some resolution among the short keys, where three of the five
    # lines are within a nanosecond of each other, and the table beside this chart is where those
    # are read off.
    vals = [v for m in order for _, v in series.get(m, [])]
    tk = ticks(ymax if ymax else max(vals) * 1.04)
    if ymax:
        # A named ceiling rather than the data's own, so that the axis ends on a round number. The
        # two slowest hashes cross it in the last few dozen bytes -- boost reaches 62.8 ns at a
        # kilobyte and folly 61.1 -- and are clipped to the frame there rather than the axis being
        # stretched by 5% for two points nobody reads a length off.
        tk = [tv for tv in tk if tv <= ymax] or tk
    s = head(W2, H2)
    # The two slowest hashes cross a named ceiling in the last few dozen bytes; without this they
    # would be drawn straight through the axis and out over the legend.
    s += (f'  <clipPath id="plot"><rect x="{L}" y="{T}" width="{W2 - R - L}" '
          f'height="{H2 - T - B}"/></clipPath>\n')
    s += '  <text x="8" y="22" class="hd" fill="#111827">What a string hash costs a lookup, by key length</text>\n'
    s += ('  <text x="8" y="42" class="sub" fill="#4b5563">nanoseconds per hash, each one waiting on the one '
          'before it, which is the order a map pays them in</text>\n')
    s += ('  <text x="8" y="60" class="sub" fill="#4b5563">median of three runs, every hash interleaved in one '
          'process; about 1.5 ns of every line is the chain\'s own store and load</text>\n')

    def px(n):
        return L + (math.log2(n) - math.log2(lo)) / (math.log2(hi) - math.log2(lo)) * (W2 - L - R)

    def py(v):
        return T + (1 - v / tk[-1]) * (H2 - T - B)

    axis_y = py(0)
    s += (f'  <rect x="{px(8):.1f}" y="{T:.0f}" width="{px(135) - px(8):.1f}" '
          f'height="{py(0) - T:.1f}" fill="#f1f5f9"/>\n')
    s += (f'  <text x="{(px(8) + px(135)) / 2:.0f}" y="{T + 14:.0f}" class="m" fill="#4b5563" '
          f'text-anchor="middle">the lengths this post\'s string keys use</text>\n')
    for tv in tk:
        s += f'  <line x1="{L}" y1="{py(tv):.1f}" x2="{W2 - R}" y2="{py(tv):.1f}" class="ax" stroke="#e5e7eb"/>\n'
        s += f'  <text x="{L - 8}" y="{py(tv) + 4:.1f}" class="m" fill="#4b5563" text-anchor="end">{tv:g}</text>\n'
    s += f'  <line x1="{L}" y1="{axis_y:.1f}" x2="{W2 - R}" y2="{axis_y:.1f}" class="base" stroke="#9ca3af"/>\n'
    s += f'  <text x="{L}" y="{T - 14}" class="m" fill="#4b5563">nanoseconds per hash, chained</text>\n'
    for n in (4, 8, 16, 32, 64, 128, 256, 512, 1024):
        s += (f'  <text x="{px(n):.1f}" y="{H2 - B + 18:.0f}" class="m" fill="#4b5563" text-anchor="middle">'
              f'{n}</text>\n')
        s += (f'  <line x1="{px(n):.1f}" y1="{axis_y:.1f}" x2="{px(n):.1f}" y2="{axis_y + 4:.1f}" '
              f'class="base" stroke="#9ca3af"/>\n')
    s += (f'  <text x="{(L + W2 - R) / 2:.0f}" y="{H2 - 14}" class="m" fill="#4b5563" text-anchor="middle">'
          f'key length in bytes</text>\n')

    ends = []
    for m in order:
        pts = sorted(series.get(m, []))
        if not pts:
            continue
        col, dash, label = style[m]
        d = " ".join(("M" if i == 0 else "L") + f"{px(n):.1f},{py(v):.1f}"
                     for i, (n, v) in enumerate(pts))
        da = "" if dash == "none" else f' stroke-dasharray="{dash}"'
        s += (f'  <path d="{d}" fill="none" stroke="{col}" stroke-width="1.9" '
              f'stroke-linejoin="round" clip-path="url(#plot)"{da}/>\n')
        ends.append((max(py(pts[-1][1]), T), px(pts[-1][0]), col, dash, label))
    ends.sort()
    last = -1e9
    for y, x, col, dash, label in ends:
        y = max(y, last + 17)
        last = y
        da = "" if dash == "none" else f' stroke-dasharray="4 3"'
        s += (f'  <line x1="{x + 8:.1f}" y1="{y:.1f}" x2="{x + 24:.1f}" y2="{y:.1f}" '
              f'stroke="{col}" stroke-width="2.4" stroke-linecap="round"{da}/>\n')
        s += f'  <text x="{x + 30:.1f}" y="{y + 4:.1f}" class="m" fill="#4b5563">{esc(label)}</text>\n'
    s += "</svg>\n"
    open(out, "w").write(s)
    print(out)


def cmd_merge(argv):
    """Combine independent runs point by point, and say how far apart the worst of them was.

    A single paired run is trustworthy for a ratio and not for a *point*: read a chart for its
    shape and check a surprising point against a second run.
    """
    out, ins = argv[0], argv[1:]
    acc = {}
    for path in ins:
        for r in read(path):
            acc.setdefault((r["key"], r["work"], r["base"], r["map"]), []).append((r["a"], r["b"]))
    worst, worst_at = 1.0, None
    with open(out, "w") as f:
        for k, vs in acc.items():
            for i in (0, 1):
                xs = [v[i] for v in vs]
                if min(xs) > 0 and max(xs) / min(xs) > worst:
                    worst, worst_at = max(xs) / min(xs), (k, xs)
            a = math.exp(sum(math.log(v[0]) for v in vs) / len(vs))
            b = math.exp(sum(math.log(v[1]) for v in vs) / len(vs))
            f.write(f"{k[0]},{k[1]},{k[2]},{k[3]},{a:.4f},{b:.4f}\n")
    print(f"{out}: {len(acc)} points from {len(ins)} runs, worst spread {worst:.3f} at "
          f"{worst_at[0] if worst_at else '--'}")


WORKS = [("build", "build"), ("hit", "hit"), ("miss", "miss"), ("half", "50% hits"),
         ("iterate", "iterate"), ("churn", "churn"), ("ie", "insert/erase")]


def cmd_table(argv):
    """The full grid as a markdown table: every map, every workload, relative to the group index."""
    csvp, key, base = argv[0], argv[1], int(argv[2])
    rows = read(csvp)
    vals = {}
    present = []
    for r in rows:
        if r["key"] == key and r["base"] == base and r["work"] != "memory":
            vals[(r["map"], r["work"])] = r["b"]
            if r["map"] not in present:
                present.append(r["map"])
    print("| map | " + " | ".join(t for _, t in WORKS) + " |")
    print("|---|" + "---|" * len(WORKS))
    for m in present:
        cells = []
        for w, _ in WORKS:
            v = vals.get((m, w))
            cells.append("--" if v is None else (f"**{v:.2f}**" if v < 1.0 else f"{v:.2f}"))
        print(f"| {PRETTY.get(m, m)} | " + " | ".join(cells) + " |")


# Cell tints for the HTML tables: a diverging ramp around parity, four steps either side, and every
# one of them light enough that the ink on top stays above 4.5:1 -- measured 7.4 to 13.1:1 for
# #1f2937, which is the constraint that decides how dark the ramp may get. Blue for faster than the
# reference and amber for slower, because that pair survives every kind of colour blindness where
# red/green does not; the two sides separate by 25 to 130 units under deuteranopia, protanopia and
# tritanopia alike. Lightness carries the magnitude, hue carries the direction, and the number is in
# the cell -- so nothing here is encoded by colour alone.
TINT_FAST = ["f1", "f2", "f3", "f4"]
TINT_SLOW = ["s1", "s2", "s3", "s4"]
# |log2(ratio)| band edges: within 5% of parity is untinted, then 16%, 41%, 2x, beyond.
TINT_EDGES = [0.07, 0.22, 0.5, 1.0]


def tint(v):
    if v is None:
        return ""
    d = abs(math.log2(v)) if v > 0 else 0.0
    if d < TINT_EDGES[0]:
        return ""
    ramp = TINT_FAST if v < 1.0 else TINT_SLOW
    for i, e in enumerate(TINT_EDGES[1:]):
        if d < e:
            return ramp[i]
    return ramp[-1]


def cmd_htmltable(argv):
    """The same grid as an HTML table, each cell tinted by how far it is from parity."""
    csvp, key, base = argv[0], argv[1], int(argv[2])
    rows = read(csvp)
    vals = {}
    present = []
    for r in rows:
        if r["key"] == key and r["base"] == base and r["work"] != "memory":
            vals[(r["map"], r["work"])] = r["b"]
            if r["map"] not in present:
                present.append(r["map"])
    best = {w: min((vals[(m, w)] for m in present if (m, w) in vals), default=None)
            for w, _ in WORKS}
    out = ['<table class="grid">', '<thead><tr><th scope="col">map</th>']
    out += [f'<th scope="col">{esc(t)}</th>' for _, t in WORKS]
    out += ["</tr></thead>", "<tbody>"]
    for m in present:
        out.append(f'<tr><th scope="row">{esc(PRETTY.get(m, m))}</th>')
        for w, _ in WORKS:
            v = vals.get((m, w))
            if v is None:
                out.append('<td class="na">--</td>')
                continue
            cls = tint(v)
            body = f"<b>{v:.2f}</b>" if best[w] is not None and v <= best[w] else f"{v:.2f}"
            out.append(f'<td{f' class="{cls}"' if cls else ""}>{body}</td>')
        out.append("</tr>")
    out += ["</tbody>", "</table>"]
    print("\n".join(out))


def cmd_swing(argv):
    """Dearest point of an octave over its cheapest: the amplitude of the load-factor sawtooth."""
    txt, key, base, work = argv[0], argv[1], int(argv[2]), argv[3]
    series = defaultdict(list)
    inside = False
    for line in open(txt):
        if line.startswith("=="):
            inside = f"{key} octave starting at {base} " in line
            continue
        if not inside or not line.startswith(f"  {work} "):
            continue
        for p in line.split()[2:]:
            m, v = p.split("=")
            series[m].append(float(v))
    for m, vs in sorted(series.items(), key=lambda kv: max(kv[1]) / min(kv[1])):
        print(f"{PRETTY.get(m, m):<26} {max(vs) / min(vs):.2f}x   ({min(vs):.2f} to {max(vs):.2f} ns)")


if __name__ == "__main__":
    {"bars": cmd_bars, "readme": cmd_readme, "memory": cmd_memory, "octave": cmd_octave, "table": cmd_table,
     "htmltable": cmd_htmltable,
     "hashlat": cmd_hashlat,
     "swing": cmd_swing, "merge": cmd_merge}[sys.argv[1]](sys.argv[2:])
