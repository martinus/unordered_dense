#!/usr/bin/env python3
"""Draw scripts/ab/sweep.cpp's CSV as an SVG: lookup cost against table size.

Two panels of the same measurement -- a random find with a 50% hit rate, which is what the scored
benchmark's find workload does -- one zoomed to the sizes most programs actually build and one over
the whole range. They carry their own y scales on purpose: that is what a detail view is for, and
the full view would otherwise squeeze every small table into a few pixels.

Stdlib only, so the repository gains no dependency, and the output is an SVG that renders in a
README.
"""
import csv
import math
import sys
from collections import defaultdict

# categorical slots 1-3 of the reference palette, light and dark, validated for CVD separation
# Four slots, validated as a categorical set against both surfaces (dataviz/validate_palette.js,
# --pairs all): the magenta had to be a magenta, because a purple beside this blue is 13.6 apart to
# normal vision and 4.8 under deuteranopia, which is not a distinction anyone can make.
# Four hues, validated as a categorical set against both surfaces. The fifth series is not a fifth
# hue: it is the same boost map with its own hash instead of this one's, so it shares boost's green
# and is told apart by being dashed. That is the honest encoding -- colour for the map, style for the
# hash -- and it is also the only one available, since no fifth hue clears the CVD floor against
# these four (the best candidate is 2.7 apart from the blue under deuteranopia).
SERIES = [("this", "this map", "#2a78d6", "#3987e5", ""),
          ("main", "4.11.0", "#eb6834", "#d95926", ""),
          ("jan", "4.8.1 (January)", "#b5399e", "#c74ab0", ""),
          ("boost", "boost::unordered_flat_map", "#1baf7a", "#199e70", ""),
          ("boostdef", "boost, its own hash", "#1baf7a", "#199e70", "7 4")]

W, H = 980, 450
PAD_L, PAD_R, PAD_T, PAD_B = 52, 190, 96, 52
PANEL_GAP = 76
DETAIL_MAX = 65536


def si(n):
    for cut, suffix in ((1 << 20, "M"), (1 << 10, "K")):
        if n >= cut:
            return f"{n // cut}{suffix}"
    return str(n)


def bar_path(x, y, w, h, r=3.0):
    """A bar rounded at the two ends away from the baseline, which `rect rx` cannot do on its own."""
    r = min(r, w / 2, h if h > 0 else 0)
    return (f"M{x:.1f},{y + h:.1f} L{x:.1f},{y + r:.1f} Q{x:.1f},{y:.1f} {x + r:.1f},{y:.1f} "
            f"L{x + w - r:.1f},{y:.1f} Q{x + w:.1f},{y:.1f} {x + w:.1f},{y + r:.1f} "
            f"L{x + w:.1f},{y + h:.1f} Z")


def nice_ticks(vmax):
    """Round y ticks from 0 to at least vmax, five or six of them."""
    for step in (0.5, 1, 2, 2.5, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000):
        if vmax / step <= 6:
            return [i * step for i in range(int(vmax / step) + 2)]
    return [0, vmax]


def main():
    # Optional flags, after the positional arguments, so every existing call still works:
    #   --panels=colA:Title A|colB:Title B   two panels of different columns over the whole x range,
    #                                        instead of the default two size ranges of one column
    #   --x=Label                            what the x axis counts, if not entries
    #   --unit=ns                            what the y axis counts
    #   --xlin                               a linear x axis. A table size is exponential and belongs
    #                                        on log2; a key length is not, and on a log axis three
    #                                        quarters of the ink lands in the last octave.
    #   --xmax=N                             draw only up to N. The CSV keeps whatever was measured;
    #                                        this is for a range whose tail is a straight line and
    #                                        would otherwise set the y scale for everything.
    argv = [a for a in sys.argv if not a.startswith("--")]
    # `--logy` is a bare switch; the rest take a value
    flags = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv if a.startswith("--"))
    rows = list(csv.DictReader(open(argv[1])))
    out = argv[2] if len(argv) > 2 else "lookup_vs_size.svg"
    sys.argv = argv
    # "ratio" draws the paired ratio and its confidence band, which is the quantity a paired
    # comparison actually measures; "ns" draws the absolute times, which are what a reader can
    # reason about. On a quiet machine both hold up -- two runs agreed to 0.78% and 0.92% -- but the
    # absolute one is the first to go when the machine is not quiet: an early pair of runs, made
    # while other work was going on, read 7.97 and 12.29 ns at 392K entries with their ratios still
    # agreeing to three digits.
    mode = "ratio" if ("relative" in rows[0] and len(sys.argv) > 5 and sys.argv[5] == "ratio") else "ns"
    # Which absolute estimator to draw depends on whether the run recorded an interval for it.
    # Without one, the minimum epoch: a machine that drifts slower can only push a measurement up,
    # never below the work's actual cost, so the floor is the steadier estimator -- measured over
    # two runs of this sweep it agrees to 0.49% median and 6.9% worst where the median epoch is
    # 0.78% and 9.3%. With one, the median, because the interval is *about* the median and a band
    # drawn around a different statistic than the one plotted would be a lie about the line.
    has_abs_band = "ns_low" in rows[0] and "ns" in rows[0]
    col = "ns" if has_abs_band else ("ns_min" if "ns_min" in rows[0] else "ns")
    title = sys.argv[3] if len(sys.argv) > 3 else "Cost of a random find against table size"
    subtitle = sys.argv[4] if len(sys.argv) > 4 else "nanoseconds per lookup, 50% of them hits"
    data = defaultdict(dict)
    buckets = {}
    band = defaultdict(dict)
    xmax = float(flags["xmax"]) if flags.get("xmax") else None
    if xmax is not None:
        rows = [r for r in rows if float(r["entries"]) <= xmax]
    for r in rows:
        n = int(float(r["entries"]))
        if "panels" in flags:
            # the columns come from --panels; this pass only needs to learn the x values and the maps
            data[r["map"]][n] = 0.0
            continue
        data[r["map"]][n] = float(r["relative"]) if mode == "ratio" else float(r[col])
        if mode == "ratio" and "rel_low" in r:
            band[r["map"]][n] = (float(r["rel_low"]), float(r["rel_high"]))
        elif mode == "ns" and has_abs_band:
            band[r["map"]][n] = (float(r["ns_low"]), float(r["ns_high"]))
        if r["map"] == "this" and "buckets" in r:
            buckets[n] = int(r["buckets"])
    present = [s for s in SERIES if s[0] in data]
    if mode == "ratio":
        present = [s for s in present if s[0] != "main"]
    all_sizes = sorted(next(iter(data.values())))

    if "panels" in flags:
        # Two workloads sharing one x axis. Each panel gets its own column and its own y scale,
        # which is the point: a build and an iteration are not comparable in magnitude.
        by_col = {}
        for spec in flags["panels"].split("|"):
            colname, ptitle = spec.split(":", 1)
            per_map = defaultdict(dict)
            for r in rows:
                per_map[r["map"]][int(float(r["entries"]))] = float(r[colname])
            by_col[ptitle] = per_map
        panels = [(t, all_sizes, d) for t, d in by_col.items()]
        band = defaultdict(dict)
    else:
        panels = [("up to 64K entries", [n for n in all_sizes if n <= DETAIL_MAX], data),
                  (f"all sizes, to {si(all_sizes[-1])}", all_sizes, data)]
    panel_w = (W - PAD_L - PAD_R - PANEL_GAP) / 2
    panel_h = H - PAD_T - PAD_B

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
         f'font-family="system-ui, -apple-system, Segoe UI, Roboto, sans-serif" role="img" '
         f'aria-label="{title}, for three hash maps">']
    # Light values are ordinary attributes so any renderer shows the chart; the stylesheet only
    # overrides them for dark mode. var() in the attributes renders as a black rectangle wherever
    # custom properties are unsupported.
    dark = ["@media (prefers-color-scheme: dark){",
            ".surface{fill:#1a1a19}.t{fill:#ffffff}.t2{fill:#c3c2b7}.g{stroke:#343431}.ring{stroke:#1a1a19}"]
    for i, (_, _, _, d, _dash) in enumerate(present):
        dark.append(f".ln{i}{{stroke:{d}}}.dot{i}{{fill:{d}}}")
    dark.append("}")
    s.append("<style>" + "".join(dark) + "</style>")
    # A diagonal hatch per dashed series, so a bar can carry the same distinction its line does.
    defs = []
    for i, (_k, _lab, light, _d, dash) in enumerate(present):
        if dash:
            defs.append(f'<pattern id="hatch{i}" width="6" height="6" patternUnits="userSpaceOnUse" '
                        f'patternTransform="rotate(45)"><rect width="6" height="6" fill="{light}"/>'
                        f'<rect width="2.4" height="6" fill="#fcfcfb" fill-opacity="0.85"/></pattern>')
    if defs:
        s.append("<defs>" + "".join(defs) + "</defs>")
    s.append(f'<rect class="surface" width="{W}" height="{H}" fill="#fcfcfb"/>')
    s.append(f'<text x="{PAD_L}" y="26" class="t" fill="#0b0b0b" font-size="16" font-weight="600">'
             f'{title}</text>')
    s.append(f'<text x="{PAD_L}" y="44" class="t2" fill="#52514e" font-size="12">'
             f'{subtitle}, {flags.get("of", "map&lt;uint64_t, size_t&gt;")}, '
             f'{"above 1 is faster than the baseline" if mode == "ratio" else "lower is better"}</text>')
    # What the band means differs by mode, and saying so on the chart matters: the ratio's interval
    # is about a quantity from which machine drift cancels, the absolute one is not -- it says how
    # tightly this run pinned its own median, not how well the number reproduces on another day.
    # The qualifier on the absolute one is not pedantry. Typically the interval is the conservative
    # number -- 1.5% of the median against the 0.78% that median moves between two runs -- but the
    # tail is not bounded by it at all: two runs put one point 9.3% apart. A within-run interval
    # measures the epochs of one run and says nothing about what differs between two.
    dotted_note = "dotted: the index doubles there, so the load factor and the cost climb between them"
    band_note = ("shaded: 95% interval on the ratio" if mode == "ratio" else
                 "shaded: 95% interval on this run's median, which is within-run precision only") if band else ""
    # One line, and it has to fit: at 11px in a 980px canvas there is room for about 165 characters
    # before it runs off the right edge, which the first version of this did.
    s.append(f'<text x="{PAD_L}" y="61" class="t2" fill="#52514e" font-size="11">'
             f'{dotted_note if buckets else ""}'
             f'{("; " if buckets else "") + band_note if band_note else ""}</text>')

    for panel, (title, sizes, data) in enumerate(panels):
        lo, hi = sizes[0], sizes[-1]
        vmax = max(max(data[k][n], band[k][n][1] if k in band else 0.0)
                   for k, _, _, _, _ in present for n in sizes)
        if mode == "ratio":
            # 1.0 is where "no difference" sits, so that is the floor worth showing; zero would put
            # every line in the top half and say nothing.
            vmin = min(1.0, min(data[k][n] for k, _, _, _, _ in present for n in sizes))
            ybot = math.floor(vmin * 10) / 10
            ytop = math.ceil(vmax * 10) / 10
            step = 0.1 if ytop - ybot <= 0.8 else 0.2
            yticks = [ybot + i * step for i in range(int(round((ytop - ybot) / step)) + 1)]
        elif "logy" in flags:
            # A total against size spans five decades, where every other chart here spans one; a
            # linear axis would put four of those decades on the baseline. Powers of ten, and the
            # bottom is the smallest value rather than zero, which a log axis cannot show.
            vmin = min(data[k][n] for k, _, _, _, _ in present for n in sizes)
            ybot = math.floor(math.log10(vmin))
            ytop = math.ceil(math.log10(vmax))
            yticks = [ybot + i for i in range(int(ytop - ybot) + 1)]
        else:
            yticks = nice_ticks(vmax)
            ybot = 0.0
            ytop = yticks[-1]
        left = PAD_L + panel * (panel_w + PANEL_GAP)

        bars = "bars" in flags

        def px(n, left=left, lo=lo, hi=hi, sizes=sizes, xlin="xlin" in flags):
            if bars:  # band scale: the categories are evenly spaced whatever their values
                return left + (sizes.index(n) + 0.5) / len(sizes) * panel_w
            if xlin:
                return left + (n - lo) / (hi - lo) * panel_w
            return left + (math.log2(n) - math.log2(lo)) / (math.log2(hi) - math.log2(lo)) * panel_w

        def py(v, ybot=ybot, ytop=ytop, logy="logy" in flags):
            u = math.log10(v) if logy and v > 0 else v
            return PAD_T + panel_h * (1 - (u - ybot) / (ytop - ybot))

        s.append(f'<text x="{left:.1f}" y="{PAD_T - 14}" class="t2" fill="#52514e" font-size="12" '
                 f'font-weight="600">{title}</text>')
        s.append(f'<text x="{left - 10:.1f}" y="{PAD_T - 14}" class="t2" fill="#52514e" font-size="11" '
                 f'text-anchor="end">{flags.get("unit", "x" if mode == "ratio" else "ns")}</text>')
        for v in yticks:
            y = PAD_T + panel_h * (1 - (v - ybot) / (ytop - ybot)) if "logy" in flags else py(v)
            s.append(f'<line x1="{left:.1f}" y1="{y:.1f}" x2="{left + panel_w:.1f}" y2="{y:.1f}" class="g" '
                     f'stroke="#e6e5e1" stroke-width="1"/>')
            label = f"{10 ** v:.4g}" if "logy" in flags else f"{v:.4g}"
            s.append(f'<text x="{left - 10:.1f}" y="{y + 4:.1f}" class="t2" fill="#52514e" font-size="11" '
                     f'text-anchor="end">{label}</text>')
        # a handful of round sizes; dense sampling must not become a smear of labels. With few
        # points -- a value-size axis has six -- every one of them is a label instead.
        if bars or len(sizes) <= 8:
            ticks = list(sizes)
        elif "xlin" in flags:
            # Round multiples of a step, not five even divisions of the range: a length axis whose
            # labels read 1, 52, 103, 154 tells a reader nothing about where 128 is.
            step = next(v for v in (8, 16, 32, 64, 128, 256, 512, 1024) if (hi - lo) / v <= 6)
            ticks = [lo] + [v for v in range(step, int(hi) + 1, step) if v - lo > step / 2]
            ticks = [min(sizes, key=lambda n, t=t: abs(n - t)) for t in ticks]
        else:
            ticks, target = [], lo
            while target <= hi:
                ticks.append(min(sizes, key=lambda n, t=target: abs(n - t)))
                target *= 8
        for n in sorted(set(ticks)):
            s.append(f'<text x="{px(n):.1f}" y="{PAD_T + panel_h + 18:.1f}" class="t2" fill="#52514e" '
                     f'font-size="11" text-anchor="middle">{si(n)}</text>')
        s.append(f'<text x="{left + panel_w / 2:.1f}" y="{H - 14}" class="t2" fill="#52514e" font-size="11" '
                 f'text-anchor="middle">{flags.get("x", "entries")}</text>')
        # where this map doubled its index
        for a, b in zip(sizes, sizes[1:]):
            if buckets.get(a) and buckets.get(b) and buckets[b] != buckets[a]:
                s.append(f'<line x1="{px(b):.1f}" y1="{PAD_T:.1f}" x2="{px(b):.1f}" y2="{PAD_T + panel_h:.1f}" '
                         f'class="g" stroke="#e6e5e1" stroke-width="1" stroke-dasharray="2 3"/>')
        if bars:
            # Grouped bars, 2px of surface between neighbours so two of them never read as one, and
            # anchored on zero because a bar whose baseline is not zero is a lie about its length.
            group = panel_w / len(sizes)
            bw = (group * 0.78) / len(present)
            for gi, n in enumerate(sizes):
                x0 = left + (gi + 0.5) * group - (bw * len(present)) / 2
                for i, (key, _, light, _dark, dash) in enumerate(present):
                    v = data[key][n]
                    y = py(v)
                    fill = f"url(#hatch{i})" if dash else light
                    s.append(f'<path d="{bar_path(x0 + i * bw + 1, y, max(bw - 2, 1), PAD_T + panel_h - y)}" '
                             f'class="ln{i}" fill="{fill}"/>')
        for i, (key, label, light, _, dash) in enumerate(present):
            if bars:
                break
            if key in band:
                # the confidence band, drawn under the line: down one edge and back along the other.
                # Clamped to the panel, because the y range is chosen from the lines -- in ratio mode
                # it is floored at 1.0 -- and a band edge outside it would otherwise be drawn over
                # the axis labels.
                def pyc(v, py=py):
                    return min(max(py(v), PAD_T), PAD_T + panel_h)

                up = " ".join(f"{px(n):.1f},{pyc(band[key][n][1]):.1f}" for n in sizes)
                down = " ".join(f"{px(n):.1f},{pyc(band[key][n][0]):.1f}" for n in reversed(sizes))
                # 0.3 rather than something lighter because the absolute interval is about 1.5% of
                # the value, which is thinner than the 2px line it surrounds; at a lower opacity it
                # would not be there at all. The ratio's band is three times wider and reads fine
                # either way.
                s.append(f'<polygon points="{up} {down}" fill="{light}" fill-opacity="0.3" stroke="none"/>')
            pts = " ".join(f"{px(n):.1f},{py(data[key][n]):.1f}" for n in sizes)
            s.append(f'<polyline points="{pts}" class="ln{i}" fill="none" stroke="{light}" stroke-width="2" '
                     f'stroke-linejoin="round" stroke-linecap="round"'
                     f'{f" stroke-dasharray=" + chr(34) + dash + chr(34) if dash else ""}/>')
        if panel == len(panels) - 1:  # direct labels, also the relief the contrast check asks for
            # Two lines that end close together would otherwise print their labels on top of each
            # other, which happened the first time this drew insert and erase.
            ends = sorted(((py(data[k][hi]), i, lab, c) for i, (k, lab, c, _, _d) in enumerate(present)))
            placed = []
            for y, i, lab, light in ends:
                if placed and y - placed[-1][0] < 15:
                    y = placed[-1][0] + 15
                placed.append((y, i, lab, light))
            # A line ends at its last x, so its label belongs there; a bar group is centred in a
            # band and its label would sit on top of the bars, so that one hangs off the panel edge.
            anchor = left + panel_w + 4 if bars else px(hi)
            for y, i, lab, light in placed:
                y0 = py(data[present[i][0]][hi])
                s.append(f'<circle cx="{anchor + 12:.1f}" cy="{y0:.1f}" r="4" class="dot{i}" fill="{light}"/>')
                if abs(y - y0) > 1:  # a short leader, so a nudged label still points at its line
                    s.append(f'<line x1="{anchor + 16:.1f}" y1="{y0:.1f}" x2="{anchor + 21:.1f}" y2="{y:.1f}" '
                             f'class="ln{i}" stroke="{light}" stroke-width="1"/>')
                s.append(f'<text x="{anchor + 24:.1f}" y="{y + 4:.1f}" class="t" fill="#0b0b0b" '
                         f'font-size="11">{lab}</text>')
    s.append("</svg>")
    open(out, "w").write("\n".join(s) + "\n")
    print(f"wrote {out}: {len(all_sizes)} sizes, {len(present)} maps, column {col}")


if __name__ == "__main__":
    main()
