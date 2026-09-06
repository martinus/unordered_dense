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


def chart(rows, title, subtitle, panels, xlabel, unit, xlog=True, ylog=False, note=""):
    if rows is None:
        return None
    return {
        "title": title,
        "subtitle": subtitle,
        "xlabel": xlabel,
        "unit": unit,
        "xlog": xlog,
        "ylog": ylog,
        "note": note,
        "resizes": resizes(rows),
        "panels": [{"title": t, "series": series_of(rows, c)} for c, t in panels],
    }


def main():
    charts = []

    def add(*a, **k):
        c = chart(*a, **k)
        if c:
            charts.append(c)

    sawtooth = ("Nothing is reserved, so between the dotted lines each table's load factor climbs "
                "from about 0.5 to its maximum and the cost climbs with it. Read a ratio at both "
                "ends: at a power of two every table has just doubled and is at its emptiest.")

    add(read("find_hits_vs_size.csv"), "A find that hits", "nanoseconds per lookup, every one present",
        [("ns", "up to 64K entries"), ("ns", "all sizes")], "entries", "ns",
        note="The discriminating lookup view. " + sawtooth)
    add(read("find_vs_size.csv"), "A find, half of them hitting", "nanoseconds per lookup, 50% hit rate",
        [("ns", "up to 64K entries"), ("ns", "all sizes")], "entries", "ns",
        note="A 50% hit rate is the maximum-entropy point of the hit-rate curve: it adds about half a "
             "branch misprediction per lookup to every map and so compresses the differences, and it "
             "can order two maps the opposite way from both all-hits and all-misses. Read it with the "
             "chart above, never alone.")
    add(read("churn_vs_size.csv"), "Churn at a fixed size", "nanoseconds per erase-and-insert pair",
        [("ns", "up to 64K entries"), ("ns", "all sizes")], "entries", "ns",
        note="The workload that separates designs rather than constant factors. " + sawtooth)
    add(read("insert_erase_vs_size.csv"), "Insert and erase",
        "nanoseconds per operator[] and erase pair, half of each finding nothing",
        [("ns", "up to 64K entries"), ("ns", "all sizes")], "entries", "ns", note=sawtooth)
    add(read("value_size.csv") and wide_value_size(), "Build and iteration against mapped-value size",
        "nanoseconds per entry, 200000 entries",
        [("build", "build from empty"), ("iterate", "one iteration pass")],
        "sizeof(mapped_type), bytes", "ns",
        note="The axis that decides dense against flat. It stops at 64 bytes because 200000 entries of "
             "a 64 byte value is 14 MB and still in L3, where 128 bytes is 27 MB and is not; past that "
             "cliff every line bends upward together and the chart stops being about the value.")
    add(read("memory_vs_value_size.csv"), "Memory against mapped-value size",
        "megabytes held for 1000000 entries",
        [("steady", "steady state"), ("peak", "peak during growth")],
        "sizeof(mapped_type), bytes", "MB",
        note="Measured with a counting allocator and checked against a replaced global operator new; "
             "the two agree to the byte. The peak is separate because growth allocates the new array "
             "beside the old and only then frees it.")
    add(read("memory_vs_size.csv"), "Memory against table size", "megabytes held",
        [("steady", "steady state"), ("peak", "peak during growth")], "entries", "MB", ylog=True,
        note="Per entry this barely moves with the table size, which is why it is sixteen near-identical "
             "octaves and why the value-size chart above is the one that discriminates. The staircase is "
             "the doubling.")

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


def wide_value_size():
    """value_size.csv is long-form (a `what` column); the chart wants one column per workload."""
    rows = read("value_size.csv")
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
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --surface: #1a1a19; --ink: #ffffff; --ink2: #c3c2b7; --ink3: #8b8a80;
      --grid: #343431; --rule: #2b2b29; --chip: #232322;
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
  figcaption { color: var(--ink2); font-size: 12.5px; line-height: 1.6; max-width: 88ch; }
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
  const px = v => L + (chart.xlog ? (Math.log2(v) - lx0) / (lx1 - lx0) : (v - xDomain[0]) / (xDomain[1] - xDomain[0])) * (W - L - R);
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

  for (const m of maps) {
    const d = pts(m).map(p => `${px(p[0]).toFixed(1)},${py(p[1]).toFixed(1)}`).join(" ");
    g.push(`<polyline points="${d}" fill="none" stroke="var(--c-${m})" stroke-width="2" stroke-linejoin="round" stroke-linecap="round" class="ln" data-map="${m}"/>`);
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
    cross.setAttribute("x1", px(best).toFixed(1)); cross.setAttribute("x2", px(best).toFixed(1));
    cross.setAttribute("opacity", 0.45);
    const rows = [];
    dots.innerHTML = "";
    for (const m of maps) {
      const p = panel.series[m].find(q => q[0] === best);
      if (!p) continue;
      rows.push([m, p[1]]);
      dots.insertAdjacentHTML("beforeend",
        `<circle cx="${px(best).toFixed(1)}" cy="${py(p[1]).toFixed(1)}" r="3.5" fill="var(--c-${m})" stroke="var(--surface)" stroke-width="1.5"/>`);
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
      (chart.note ? `<figcaption style="margin-top:10px">${chart.note}</figcaption>` : "");
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
