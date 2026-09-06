#!/usr/bin/env python3
"""Draw scripts/ab/sweep.cpp's CSV as an SVG: lookup cost against table size.

Two panels sharing one y scale, hits and misses, three lines each. Log x, because the sizes are
powers of two and the interesting part is where the index outgrows a cache level. Stdlib only, so
the repository gains no dependency, and the output is an SVG that renders in a README.
"""
import csv
import sys
from collections import defaultdict

# categorical slots 1-3 of the reference palette, light and dark, validated for CVD separation
SERIES = [("this", "this map", "#2a78d6", "#3987e5"),
          ("main", "robin hood (main)", "#eb6834", "#d95926"),
          ("boost", "boost::unordered_flat_map", "#1baf7a", "#199e70")]

W, H = 980, 440
PAD_L, PAD_R, PAD_T, PAD_B = 58, 190, 96, 52
PANEL_GAP = 44


def si(n):
    for cut, suffix in ((1 << 20, "M"), (1 << 10, "K")):
        if n >= cut:
            return f"{n // cut}{suffix}"
    return str(n)


def main():
    rows = list(csv.DictReader(open(sys.argv[1])))
    out = sys.argv[2] if len(sys.argv) > 2 else "lookup_vs_size.svg"
    data = defaultdict(dict)  # data[map][n] = (hit, miss)
    buckets = {}                # entries -> bucket count of this map, for the resize markers
    for r in rows:
        data[r["map"]][int(r["entries"])] = (float(r["hit_ns"]), float(r["miss_ns"]))
        if r["map"] == "this" and "buckets" in r:
            buckets[int(r["entries"])] = int(r["buckets"])
    present = [s for s in SERIES if s[0] in data]
    sizes = sorted(next(iter(data.values())))
    lo, hi = sizes[0], sizes[-1]

    # every eighth power of two, snapped to the nearest sample that exists
    ticks, target = [], lo
    while target <= hi:
        ticks.append(min(sizes, key=lambda n, t=target: abs(n - t)))
        target *= 8
    ticks = sorted(set(ticks))

    # Log y, because the interesting range spans a factor of thirty: on a linear axis every line
    # below about 10 ns is squeezed into a few pixels and the whole small-table half of the picture
    # -- which is most tables -- says nothing.
    vals = [v[i] for m in data.values() for v in m.values() for i in (0, 1)]
    ymin, ymax = min(vals), max(vals)
    yticks = [t for t in (1, 2, 3, 5, 10, 20, 30, 50, 100) if ymin / 1.3 <= t <= ymax * 1.3]
    # the ticks are round numbers, so the range has to be widened to whatever the data actually
    # reaches -- otherwise a line above the last tick is silently drawn outside the panel
    ylo = min(yticks[0] / 1.15, ymin / 1.05)
    yhi = max(yticks[-1] * 1.15, ymax * 1.05)

    panel_w = (W - PAD_L - PAD_R - PANEL_GAP) / 2
    panel_h = H - PAD_T - PAD_B

    def px(n, panel):
        import math
        frac = (math.log2(n) - math.log2(lo)) / (math.log2(hi) - math.log2(lo))
        return PAD_L + panel * (panel_w + PANEL_GAP) + frac * panel_w

    def py(v):
        import math
        frac = (math.log(v) - math.log(ylo)) / (math.log(yhi) - math.log(ylo))
        return PAD_T + panel_h * (1 - frac)

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
         f'font-family="system-ui, -apple-system, Segoe UI, Roboto, sans-serif" role="img" '
         f'aria-label="Lookup cost against table size for three hash maps">']
    # Light values are written as ordinary attributes so that any renderer shows the chart, and the
    # stylesheet only overrides them for dark mode. Doing it the other way round -- var() in the
    # attributes -- renders as a black rectangle anywhere custom properties are not supported.
    dark = ["@media (prefers-color-scheme: dark){",
            ".surface{fill:#1a1a19}.t{fill:#ffffff}.t2{fill:#c3c2b7}.g{stroke:#343431}",
            ".ring{stroke:#1a1a19}"]
    for i, (_, _, _, d) in enumerate(present):
        dark.append(f".ln{i}{{stroke:{d}}}.dot{i}{{fill:{d}}}")
    dark.append("}")
    s.append("<style>" + "".join(dark) + "</style>")
    s.append(f'<rect class="surface" width="{W}" height="{H}" fill="#fcfcfb"/>')
    s.append(f'<text x="{PAD_L}" y="26" class="t" fill="#0b0b0b" font-size="16" font-weight="600">'
             f'Lookup cost against table size</text>')
    s.append(f'<text x="{PAD_L}" y="44" class="t2" fill="#52514e" font-size="12">'
             f'nanoseconds per lookup, {si(lo)} to {si(hi)} entries, map&lt;uint64_t, size_t&gt;, lower is better</text>')
    s.append(f'<text x="{PAD_L}" y="61" class="t2" fill="#52514e" font-size="11">'
             f'dotted lines are where this map doubles its index: nothing is reserved, so between them the '
             f'load factor climbs to the maximum and the cost climbs with it</text>')

    for panel, (title, idx) in enumerate((("found", 0), ("not found", 1))):
        x0, x1 = px(lo, panel), px(hi, panel)
        s.append(f'<text x="{x0:.1f}" y="{PAD_T - 14}" class="t2" fill="#52514e" font-size="12" '
                 f'font-weight="600">{title}</text>')
        for v in yticks:
            y = py(v)
            s.append(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x1:.1f}" y2="{y:.1f}" class="g" '
                     f'stroke="#e6e5e1" stroke-width="1"/>')
            if panel == 0:
                s.append(f'<text x="{PAD_L - 10}" y="{y + 4:.1f}" class="t2" fill="#52514e" font-size="11" '
                         f'text-anchor="end">{v}</text>')
        for n in ticks:  # a handful of round sizes; dense sampling must not become a smear of labels
            s.append(f'<text x="{px(n, panel):.1f}" y="{PAD_T + panel_h + 18:.1f}" class="t2" fill="#52514e" '
                     f'font-size="11" text-anchor="middle">{si(n)}</text>')
        s.append(f'<text x="{(x0 + x1) / 2:.1f}" y="{H - 14}" class="t2" fill="#52514e" font-size="11" '
                 f'text-anchor="middle">entries</text>')
        # where this map doubled its index: the sawtooth in every line is the load factor falling
        # back at each of these and climbing again to the maximum
        for a, b in zip(sizes, sizes[1:]):
            if buckets.get(a) and buckets.get(b) and buckets[b] != buckets[a]:
                x = px(b, panel)
                s.append(f'<line x1="{x:.1f}" y1="{PAD_T:.1f}" x2="{x:.1f}" y2="{PAD_T + panel_h:.1f}" '
                         f'class="g" stroke="#e6e5e1" stroke-width="1" stroke-dasharray="2 3"/>')
        for i, (key, label, light, _) in enumerate(present):
            pts = " ".join(f"{px(n, panel):.1f},{py(data[key][n][idx]):.1f}" for n in sizes)
            s.append(f'<polyline points="{pts}" class="ln{i}" fill="none" stroke="{light}" stroke-width="2" '
                     f'stroke-linejoin="round" stroke-linecap="round"/>')
            if len(sizes) <= 40:  # markers help a sparse line and only clutter a dense one
                for n in sizes:
                    s.append(f'<circle cx="{px(n, panel):.1f}" cy="{py(data[key][n][idx]):.1f}" r="3.2" '
                             f'class="dot{i} ring" fill="{light}" stroke="#fcfcfb" stroke-width="1.5"/>')
            if panel == 1:  # direct labels, which is also the relief the contrast check asks for
                y = py(data[key][sizes[-1]][idx])
                s.append(f'<circle cx="{x1 + 12:.1f}" cy="{y:.1f}" r="4" class="dot{i}" fill="{light}"/>')
                s.append(f'<text x="{x1 + 21:.1f}" y="{y + 4:.1f}" class="t" fill="#0b0b0b" font-size="11">{label}</text>')
    s.append(f'<text x="{PAD_L - 10}" y="{PAD_T - 14}" class="t2" fill="#52514e" font-size="11" '
             f'text-anchor="end">ns</text>')
    s.append("</svg>")
    open(out, "w").write("\n".join(s) + "\n")
    print(f"wrote {out}: {len(sizes)} sizes, {len(present)} maps")


if __name__ == "__main__":
    main()
