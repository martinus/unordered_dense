#!/usr/bin/env python3
"""The figures of DESIGN.md, from the result files in data/, as light and dark SVGs in img/.

    python3 plots.py

Colors are the validated reference palette, one fixed slot per container in every figure: id_map
blue, this repository's map orange, the vector aqua, std yellow, id_map with pairs magenta. Light
mode puts three of them under 3:1 against the surface, so every figure carries direct labels or a
legend and DESIGN.md repeats the numbers in tables.
"""
import re
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("svg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyBboxPatch, Rectangle  # noqa: E402

HERE = Path(__file__).parent
DATA = HERE / "data"
IMG = HERE / "img"

THEMES = {
    "light": dict(
        surface="#fcfcfb", ink="#0b0b0b", ink2="#52514e", muted="#898781", grid="#e1e0d9", axis="#c3c2b7",
        series={"id_map": "#2a78d6", "map": "#eb6834", "vector": "#1baf7a", "std": "#eda100", "id_map_pairs": "#e87ba4"},
    ),
    "dark": dict(
        surface="#1a1a19", ink="#ffffff", ink2="#c3c2b7", muted="#898781", grid="#2c2c2a", axis="#383835",
        series={"id_map": "#3987e5", "map": "#d95926", "vector": "#199e70", "std": "#c98500", "id_map_pairs": "#d55181"},
    ),
}
LABEL = {
    "id_map": "id_map",
    "map": "unordered_dense::map 5.3.1",
    "vector": "vector indexed by ID",
    "std": "std::unordered_map",
    "id_map_pairs": "id_map, std::pair API",
}


def style(fig, axes, t):
    fig.patch.set_facecolor(t["surface"])
    for ax in axes:
        ax.set_facecolor(t["surface"])
        for side in ("top", "right", "left"):
            ax.spines[side].set_visible(False)
        ax.spines["bottom"].set_color(t["axis"])
        ax.spines["bottom"].set_linewidth(1)
        ax.tick_params(colors=t["muted"], labelsize=9, length=0)
        ax.xaxis.label.set_color(t["ink2"])
        ax.yaxis.label.set_color(t["ink2"])
        ax.title.set_color(t["ink"])
        ax.grid(True, color=t["grid"], linewidth=1, linestyle="-")
        ax.set_axisbelow(True)


def legend(fig, keys, t, y=1.0):
    handles = [plt.Line2D([0], [0], color=t["series"][k], lw=6, solid_capstyle="round") for k in keys]
    leg = fig.legend(handles, [LABEL[k] for k in keys], loc="upper center", bbox_to_anchor=(0.5, y),
                     ncol=len(keys), frameon=False, fontsize=9, handlelength=1.2, columnspacing=1.6)
    for text in leg.get_texts():
        text.set_color(t["ink2"])


def hbar(ax, y, width, height, color, t, radius_px=4):
    """A horizontal bar from x=0 with a 4px rounded data end and a square baseline end."""
    if width <= 0:
        return
    fig = ax.figure
    fig.canvas.draw()
    (x0, y0), (x1, y1) = ax.transData.transform([(0, 0), (1, 1)])
    px_per_x = abs(x1 - x0)
    px_per_y = abs(y1 - y0)
    r = min(radius_px / px_per_x, width / 2)
    # a square body up to the rounding, and a short rounded cap overlapping it: no seam at either end
    ax.add_patch(Rectangle((0, y - height / 2), width - r, height, facecolor=color, edgecolor="none", lw=0,
                           zorder=3))
    cap_start = max(0.0, width - 2 * r)
    ax.add_patch(FancyBboxPatch((cap_start, y - height / 2), width - cap_start, height,
                                boxstyle=f"round,pad=0,rounding_size={r}", mutation_aspect=px_per_x / px_per_y,
                                facecolor=color, edgecolor="none", lw=0, zorder=3))


def save(fig, name, mode):
    fig.savefig(IMG / f"{name}-{mode}.svg", facecolor=fig.get_facecolor(), bbox_inches="tight")
    plt.close(fig)


def read_idbench():
    """data/idbench.txt -> {(compiler, pattern, n, container): (find, build, bytes)} and churn rows."""
    cells, churn, comp = {}, {}, None
    for line in open(DATA / "idbench.txt"):
        f = line.split()
        if not f:
            continue
        if f[0] == "==":
            comp = f[1]
        elif f[0] == "churn":
            churn[(comp, int(f[1]), f[2])] = (float(f[4]), float(f[6]))
        else:
            cells[(comp, f[0], int(f[1]), f[2])] = (float(f[7]), float(f[4]), float(f[9]))
    return cells, churn


def fig_lookups(cells, t, mode):
    keys = ["std", "map", "vector", "id_map"]
    pats = [("dense", "dense IDs: i"), ("ifc", "web-ifc-like IDs: 1.75 i"), ("sparse", "sparse IDs: 1/16 of the range")]
    fig, axes = plt.subplots(1, 3, figsize=(11, 3.6))
    style(fig, axes, t)
    for ax, (p, title) in zip(axes, pats):
        sizes = sorted({k[2] for k in cells if k[0] == "clang++" and k[1] == p})
        for k in keys:
            ys = [cells[("clang++", p, n, k)][0] for n in sizes]
            ax.plot(sizes, ys, color=t["series"][k], lw=2, solid_capstyle="round", solid_joinstyle="round", zorder=3)
            ax.plot(sizes[-1], ys[-1], "o", ms=8, color=t["series"][k], mec=t["surface"], mew=2, zorder=4)
        ax.set_xscale("log")
        ax.set_ylim(bottom=0)
        ax.set_title(title, fontsize=10, loc="left", color=t["ink"])
        ax.set_xlabel("entries")
        idm = cells[("clang++", p, sizes[-1], "id_map")][0]
        ax.annotate(f"{idm:.1f} ns", (sizes[-1], idm), xytext=(6, 0), textcoords="offset points", va="center",
                    fontsize=9, color=t["ink"])
    axes[0].set_ylabel("ns per lookup (lower is better)")
    legend(fig, keys, t, y=1.08)
    save(fig, "lookups", mode)


def fig_memory(cells, t, mode):
    keys = ["std", "map", "vector", "id_map", "id_map_pairs"]
    pats = [("dense", "dense"), ("ifc", "web-ifc-like"), ("sparse", "sparse 1/16")]
    cap = 50.0
    fig, ax = plt.subplots(figsize=(8, 5.2))
    style(fig, [ax], t)
    ax.grid(axis="y", visible=False)
    band, h = 1.0, 0.14
    ax.set_xlim(0, cap + 9)
    ax.set_ylim(-0.6, len(pats) - 0.3)
    yticks = []
    for i, (p, name) in enumerate(pats):
        base = len(pats) - 1 - i
        yticks.append((base, name))
        for j, k in enumerate(keys):
            y = base + (len(keys) / 2 - 0.5 - j) * (h + 0.03)
            v = cells[("clang++", p, 1000000, k)][2]
            hbar(ax, y, min(v, cap), h, t["series"][k], t)
            text = f"{v:.0f} B (off scale)" if v > cap else f"{v:.1f}"
            ax.text(min(v, cap) + 0.6, y, text, va="center", fontsize=8, color=t["ink2"])
    ax.set_yticks([b for b, _ in yticks], [n for _, n in yticks], color=t["ink2"], fontsize=10)
    ax.set_xlabel("bytes per entry at 1M entries, 8 byte values (lower is better)")
    legend(fig, keys, t, y=1.02)
    save(fig, "memory", mode)


def read_webifc():
    rows = defaultdict(lambda: defaultdict(list))
    for line in open(DATA / "webifc.txt"):
        m = re.match(r"round \d+ bin (\S+) model (\S+) (.*)", line)
        if not m:
            continue
        f = m.group(3).split()
        kv = dict(zip(f[0::2], f[1::2]))
        rows[m.group(2)][m.group(1)].append(float(kv["total_s"]))
    return rows


MODELS = [
    ("ISSUE_053_20181220Holter_Tower_10", "Holter Tower, 177 MB"),
    ("LTU_A-House_redesign", "LTU A-House, 181 MB"),
    ("ISSUE_098_R8_F1_MAB_AR_M3_XX_XXX_MO_7000", "ISSUE_098, 73 MB"),
    ("ISSUE_068_ARK_NUS_skolebygg", "ISSUE_068, 57 MB"),
]


def fig_webifc(rows, t, mode):
    keys = [("map", "v5.3.1"), ("vector", "vec"), ("id_map", "idmap"), ("id_map_pairs", "idmapp")]
    fig, ax = plt.subplots(figsize=(8, 5.6))
    style(fig, [ax], t)
    ax.grid(axis="y", visible=False)
    h = 0.17
    ax.set_xlim(0, 1.25)
    ax.set_ylim(-0.6, len(MODELS) - 0.3)
    for i, (model, name) in enumerate(MODELS):
        base = len(MODELS) - 1 - i
        std = statistics.median(rows[model]["b-clang-std"])
        for j, (k, b) in enumerate(keys):
            y = base + (len(keys) / 2 - 0.5 - j) * (h + 0.03)
            v = statistics.median(rows[model][f"b-clang-{b}"]) / std
            hbar(ax, y, v, h, t["series"][k], t)
            ax.text(v + 0.01, y, f"{v:.3f}", va="center", fontsize=8, color=t["ink2"], zorder=5,
                    bbox=dict(facecolor=t["surface"], edgecolor="none", pad=0.5))
    ax.axvline(1.0, color=t["ink2"], lw=1, zorder=4)
    ax.text(1.0, len(MODELS) - 0.35, " std::unordered_map = 1", fontsize=8, color=t["ink2"], va="bottom")
    ax.set_yticks([len(MODELS) - 1 - i for i in range(len(MODELS))], [n for _, n in MODELS], color=t["ink2"],
                  fontsize=10)
    ax.set_xlabel("web-ifc load time, ratio to std::unordered_map, clang, median of 11 rounds (lower is better)")
    legend(fig, [k for k, _ in keys], t, y=1.02)
    save(fig, "webifc", mode)


def fig_churn(churn, t, mode):
    keys = ["std", "map", "vector", "id_map", "id_map_pairs"]
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.4))
    style(fig, axes, t)
    for ax, (idx, cap, xlabel) in zip(axes, [(0, 160.0, "ns per erase + insert (lower is better)"),
                                             (1, 50.0, "bytes per live entry (lower is better)")]):
        ax.grid(axis="y", visible=False)
        ax.set_xlim(0, cap * 1.25)
        ax.set_ylim(-0.6, len(keys) - 0.4)
        for j, k in enumerate(keys):
            y = len(keys) - 1 - j
            v = churn[("clang++", 1000000, k)][idx]
            hbar(ax, y, min(v, cap), 0.4, t["series"][k], t)
            ax.text(min(v, cap) + cap * 0.015, y, f"{v:.0f} (off scale)" if v > cap else f"{v:.1f}", va="center",
                    fontsize=8, color=t["ink2"])
        ax.set_yticks([], [])
        ax.set_xlabel(xlabel)
    axes[0].set_title("churn, 1M live IDs drifting upward", fontsize=10, loc="left", color=t["ink"])
    legend(fig, keys, t, y=1.1)
    save(fig, "churn", mode)


def main():
    IMG.mkdir(exist_ok=True)
    plt.rcParams["font.family"] = ["DejaVu Sans"]
    plt.rcParams["svg.fonttype"] = "none"
    cells, churn = read_idbench()
    web = read_webifc()
    for mode, t in THEMES.items():
        fig_lookups(cells, t, mode)
        fig_memory(cells, t, mode)
        fig_webifc(web, t, mode)
        fig_churn(churn, t, mode)


if __name__ == "__main__":
    main()
