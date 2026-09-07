#!/usr/bin/env python3
"""The byte-level index-layout figures for the hash map index post.

One script rather than twelve hand-written SVGs, because the point of the set is that they are
comparable: one byte is one cell of the same width in every figure, the same fill means the same
kind of information everywhere (teal a fingerprint, sky a distance or displacement, amber an index
into a value array, violet an overflow counter, grey a key or value that is actually there), and a
figure that is drawn rather than generated drifts from that within an hour.

Metadata rows are drawn one cell per *slot* and aligned across rows, so that slot i of the
fingerprints sits above slot i of the indices and two maps' metadata can be compared column by
column; the real byte count of each row is in its caption. That is the only place the drawing is
not to scale, and it is the one that makes the figures readable.

Stdlib only, no dependency, and the output renders in a README or a GitHub-flavoured page.

    scripts/ab/diagrams.py [outdir]
"""
import os
import sys

BYTE = 26          # one byte, one cell, in every figure
ROW = 30           # cell height
LEFT = 132         # where the diagram starts, so the row labels have room
W = 760

STYLE = """
    .mono{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:11px}
    .muted{fill:#6b7280}
    .lbl{fill:#1f2937;font-size:12px}
    .hd{fill:#1f2937;font-weight:600}
    .cell{fill:#fff;stroke:#1f2937;stroke-width:1.2}
    .fp{fill:#ccfbf1}
    .dist{fill:#e0f2fe}
    .idx{fill:#fef3c7}
    .ovf{fill:#ede9fe}
    .payload{fill:#f1f5f9}
    .sent{fill:url(#hatch);stroke:#9ca3af;stroke-width:1.2}
    .arrow{stroke:#0f766e;stroke-width:1.6;fill:none;marker-end:url(#head)}
    .thin{stroke:#9ca3af;stroke-width:1;fill:none}
"""

DEFS = """
    <pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
      <line x1="0" y1="0" x2="0" y2="6" stroke="#d1d5db" stroke-width="2"/>
    </pattern>
    <marker id="head" markerWidth="8" markerHeight="8" refX="6" refY="4" orient="auto">
      <path d="M0,0 L8,4 L0,8 z" fill="#0f766e"/>
    </marker>
"""


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def svg(height, body, width=W):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
            f'font-family="Inter, system-ui, sans-serif" font-size="13">\n'
            f'  <style>{STYLE}  </style>\n  <defs>{DEFS}  </defs>\n{body}</svg>\n')


def text(x, y, s, cls="lbl", anchor="start", weight=None):
    w = f' font-weight="{weight}"' if weight else ""
    return (f'  <text x="{x:.0f}" y="{y:.0f}" class="{cls}" text-anchor="{anchor}"{w}>'
            f'{esc(s)}</text>\n')


def row(x, y, cells, h=ROW, unit=BYTE):
    """cells: list of (label, css class, width in units). Text centred, small text if it is long."""
    out = ""
    cx = x
    for label, cls, span in cells:
        w = span * unit
        out += f'  <rect x="{cx:.0f}" y="{y:.0f}" width="{w:.0f}" height="{h}" class="cell {cls}"/>\n'
        if label:
            size = 11 if len(label) * 6.6 < w else (9 if len(label) * 5.6 < w else 8)
            out += (f'  <text x="{cx + w / 2:.0f}" y="{y + h / 2 + 4:.0f}" class="mono" '
                    f'text-anchor="middle" font-size="{size}">{esc(label)}</text>\n')
        cx += w
    return out


def brace(x0, x1, y, label, up=False):
    """A flat span marker under (or over) a run of cells, with a label."""
    d = -6 if up else 6
    ty = y + (-10 if up else 18)
    return (f'  <path d="M{x0:.0f},{y:.0f} L{x0:.0f},{y + d:.0f} L{x1:.0f},{y + d:.0f} '
            f'L{x1:.0f},{y:.0f}" class="thin"/>\n'
            + text((x0 + x1) / 2, ty, label, "muted", "middle"))


def note(y, s, x=20):
    return text(x, y, s, "muted")


# --------------------------------------------------------------------------- the figures

def hashmap_basics():
    """What an open addressing lookup does, and where the five questions sit in it."""
    b = text(20, 24, "one lookup in an open addressing hash map", "hd")

    def mark(x, y, n):
        return (f'  <circle cx="{x}" cy="{y}" r="9" fill="#0f766e"/>\n'
                f'  <text x="{x}" y="{y + 4}" text-anchor="middle" fill="#ffffff" font-size="11" '
                f'font-weight="600">{n}</text>\n')

    # the key, the hash, and which bits of it go where
    y = 46
    b += row(20, y, [("key", "payload", 3)])
    b += f'  <path d="M{20 + 3 * BYTE + 4},{y + ROW / 2} L{20 + 3 * BYTE + 26},{y + ROW / 2}" class="arrow"/>\n'
    b += row(20 + 3 * BYTE + 30, y, [("hash", "dist", 3)])
    hx = 20 + 6 * BYTE + 60
    b += f'  <path d="M{hx - 26},{y + ROW / 2} L{hx - 4},{y + ROW / 2}" class="arrow"/>\n'
    b += row(hx, y, [("home bits", "dist", 4), ("", "", 5), ("fp", "fp", 2)])
    b += text(hx, y - 8, "the 64 bit hash", "muted")
    b += text(hx + 11 * BYTE + 8, y + 20, "kept beside the slot", "muted")

    # the metadata, with the home slot the top bits picked
    y = 150
    b += text(20, y - 22, "the metadata", "hd")
    b += text(112, y - 22, "a byte or two per slot", "muted")
    cells = [("--", "", 1), ("A2", "fp", 1), ("--", "", 1), ("7C", "fp", 1), ("11", "fp", 1),
             ("--", "", 1), ("A2", "fp", 1), ("3F", "fp", 1), ("--", "", 1), ("62", "fp", 1),
             ("A2", "fp", 1), ("--", "", 1), ("18", "fp", 1), ("--", "", 1), ("5E", "fp", 1),
             ("--", "", 1)]
    b += row(LEFT, y, cells)
    b += text(LEFT - 12, y + 20, "metadata", "muted", "end")
    home = LEFT + 6 * BYTE
    b += (f'  <rect x="{home}" y="{y - 3}" width="{BYTE}" height="{ROW + 6}" fill="none" '
          f'stroke="#0f766e" stroke-width="2"/>\n')
    # the top bits pick that slot
    b += (f'  <path d="M{hx + 2 * BYTE},{46 + ROW + 2} C{hx + 2 * BYTE},{y - 40} '
          f'{home + BYTE / 2},{y - 40} {home + BYTE / 2},{y - 8}" class="arrow"/>\n')
    b += mark(hx + 2 * BYTE + 22, 46 + ROW + 30, 1)
    b += text(hx + 2 * BYTE + 36, 46 + ROW + 34, "home: which slot the key belongs in", "muted")
    b += mark(LEFT + BYTE / 2, y + ROW + 24, 2)
    b += text(LEFT + BYTE / 2 + 14, y + ROW + 28,
              "here? one compare says whether any of these sixteen could be mine", "muted")
    b += mark(LEFT + 16 * BYTE + 22, y + ROW / 2, 4)
    b += (f'  <path d="M{LEFT + 16 * BYTE + 34},{y + ROW / 2} L{LEFT + 16 * BYTE + 56},{y + ROW / 2}" '
          f'class="arrow"/>\n')
    b += text(LEFT + 16 * BYTE + 60, y + 14, "where next:", "muted")
    b += text(LEFT + 16 * BYTE + 60, y + 28, "on to the next", "muted")
    b += mark(LEFT + BYTE / 2, y + ROW + 46, 3)
    b += text(LEFT + BYTE / 2 + 14, y + ROW + 50, "absent: an empty slot proves the key is not in the table", "muted")

    # the slots
    y = 264
    b += row(LEFT, y, [("", "" if i in (0, 2, 5, 8, 11, 13, 15) else "payload", 1) for i in range(16)])
    b += text(LEFT - 12, y + 20, "slots", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW, "the key and the value, or an index that finds them")
    b += mark(LEFT + BYTE / 2, y + ROW + 46, 5)
    b += text(LEFT + BYTE / 2 + 14, y + ROW + 50, "gone: what does erasing one of them leave behind?", "muted")

    return svg(y + 158, b)


def byte_ruler(x, y, segments, width=None):
    """The block drawn to true byte scale, with a tick per byte and the offsets labelled.

    This goes at the *top* of a layout figure, because it is the anatomy: how many bytes there are
    and how they are divided. The detail rows below expand one of these segments at a time, in the
    same colour and under the same name, and they are drawn one cell per *slot* so that two maps can
    be compared column by column -- which is why they are not to scale and this is.
    """
    total = sum(n for _, _, n in segments)
    width = width or 600
    scale = width / total
    h = 16
    out = ""
    cx = x
    for label, cls, n in segments:
        w = n * scale
        out += f'  <rect x="{cx:.1f}" y="{y}" width="{w:.1f}" height="{h}" class="cell {cls}"/>\n'
        if w > len(label) * 6.6:
            out += (f'  <text x="{cx + w / 2:.1f}" y="{y + h - 4}" class="mono" text-anchor="middle" '
                    f'font-size="9">{esc(label)}</text>\n')
        cx += w
    # one faint tick per byte, so the count is countable rather than asserted
    for i in range(total + 1):
        tx = x + i * scale
        out += (f'  <line x1="{tx:.1f}" y1="{y + h}" x2="{tx:.1f}" y2="{y + h + (5 if i % 8 else 8)}" '
                f'stroke="#9ca3af" stroke-width="{0.6 if i % 8 else 1}"/>\n')
    off = 0
    for label, _, n in segments + [("", "", 0)]:
        tx = x + off * scale
        anchor = "start" if off == 0 else ("end" if off == total else "middle")
        out += (f'  <text x="{tx:.1f}" y="{y + h + 21}" class="muted" text-anchor="{anchor}" '
                f'font-size="10">{off}</text>\n')
        off += n
    # no total label: every figure's title already says how many bytes it is
    return out


def anatomy(title, segments, y=40):
    """A figure's opening: what it is, then the bytes it is made of, to scale."""
    b = text(20, 24, title, "hd")
    b += byte_ruler(LEFT, y, segments)
    return b


ANATOMY_H = 96  # where the first detail row goes, under the byte strip


def rh_bucket():
    """unordered_dense 4.11.0: distance above fingerprint in one word."""
    b = anatomy("one bucket: 8 bytes, and no key in it",
                [("distance", "dist", 3), ("fp", "fp", 1), ("value index", "idx", 4)])
    y = ANATOMY_H + 20
    b += text(20, y - 12, "a run of buckets, and what an insert does to it", "hd")
    b += row(LEFT, y, [(t, "", 2) for t in ["1.9C>3", "2.5B>0", "3.11>5", "1.E0>1", "2.A7>4", "0"]])
    b += text(LEFT - 12, y + 20, "before", "muted", "end")
    y2 = y + 56
    b += row(LEFT, y2, [("1.9C>3", "", 2), ("2.5B>0", "", 2), ("2.33>6", "fp", 2), ("4.11>5", "", 2),
                        ("2.E0>1", "", 2), ("3.A7>4", "", 2)])
    b += text(LEFT - 12, y2 + 20, "after", "muted", "end")
    b += f'  <path d="M{LEFT + 5 * BYTE},{y + ROW + 4} L{LEFT + 5 * BYTE},{y2 - 4}" class="arrow"/>\n'
    b += text(LEFT + 12 * BYTE + 14, y + 20, "each cell reads", "muted")
    b += text(LEFT + 12 * BYTE + 14, y + 36, "distance . fp > value index", "muted")
    return svg(y2 + ROW + 24, b)


def swiss_group():
    """abseil: one control byte per slot, sixteen compared at once."""
    b = anatomy("abseil: 16 control bytes for 16 slots", [("ctrl[]", "fp", 16)])
    y = ANATOMY_H + 22
    b += text(20, y - 12, "the hash, split in two", "hd")
    b += row(LEFT, y, [("H2", "fp", 1), ("", "", 5), ("H1", "dist", 2)])
    b += text(LEFT - 12, y + 20, "64 bit hash", "muted", "end")
    b += text(LEFT + 8 * BYTE + 14, y + 13, "H2: the top 7 bits, stored as the control byte", "muted")
    b += text(LEFT + 8 * BYTE + 14, y + 29, "H1: the whole hash, masked, picks the group", "muted")

    y += 76
    tags = [("80", "", 1), ("2C", "fp", 1), ("80", "", 1), ("7A", "fp", 1),
            ("FE", "ovf", 1), ("2C", "fp", 1), ("11", "fp", 1), ("80", "", 1),
            ("4D", "fp", 1), ("80", "", 1), ("2C", "fp", 1), ("FE", "ovf", 1),
            ("63", "fp", 1), ("80", "", 1), ("09", "fp", 1), ("5E", "fp", 1)]
    b += row(LEFT, y, tags)
    b += text(LEFT - 12, y + 20, "ctrl[]", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW,
               "one 16 byte load, one compare, one movemask: sixteen verdicts")

    y += 84
    b += row(LEFT, y, [("", "payload", 1)] * 16)
    b += text(LEFT - 12, y + 20, "slots[]", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW, "the key and the value, in the slot")
    return svg(y + ROW + 40, b)


def boost_group15():
    """boost: fifteen slots and an overflow byte."""
    b = anatomy("boost: 16 metadata bytes for 15 slots",
                [("h00 .. h14", "fp", 15), ("ofw", "ovf", 1)])
    y = ANATOMY_H + 22
    tags = [("h00", "fp", 1), ("h01", "fp", 1), ("h02", "fp", 1), ("--", "", 1), ("h04", "fp", 1),
            ("h05", "fp", 1), ("--", "", 1), ("h07", "fp", 1), ("h08", "fp", 1), ("h09", "fp", 1),
            ("h10", "fp", 1), ("--", "", 1), ("h12", "fp", 1), ("h13", "fp", 1), ("h14", "fp", 1),
            ("ofw", "ovf", 1)]
    b += row(LEFT, y, tags)
    b += text(LEFT - 12, y + 20, "group15", "muted", "end")
    b += brace(LEFT, LEFT + 15 * BYTE, y + ROW, "0 available, otherwise 2..255 of the hash")

    y += 84
    b += text(20, y - 12, "the overflow byte: one bit per hash class", "hd")
    b += row(LEFT, y, [(str(i), "ovf" if i == 3 else "", 1) for i in range(8)])
    b += text(LEFT - 12, y + 20, "ofw", "muted", "end")
    b += text(LEFT + 8 * BYTE + 14, y + 13, "an insert that found this group full set bit 3,", "muted")
    b += text(LEFT + 8 * BYTE + 14, y + 29, "because its hash is 3 mod 8, and probed on", "muted")

    y += 76
    b += row(LEFT, y, [("", "payload", 1)] * 15)
    b += text(LEFT - 12, y + 20, "slots[]", "muted", "end")
    b += brace(LEFT, LEFT + 15 * BYTE, y + ROW, "the key and the value, in the slot")
    return svg(y + ROW + 40, b)


def f14_chunk():
    """folly F14: fourteen tags and two counters."""
    b = anatomy("folly F14: 16 metadata bytes for 14 slots",
                [("tags_", "fp", 14), ("ctl", "ovf", 1), ("out", "ovf", 1)])
    y = ANATOMY_H + 22
    tags = [(t, "fp", 1) for t in ["3A", "--", "91", "3A", "07", "--", "C4", "5F", "--", "22",
                                   "3A", "8B", "--", "6E"]]
    b += row(LEFT, y, tags + [("ctl", "ovf", 1), ("out", "ovf", 1)])
    b += text(LEFT - 12, y + 20, "F14Chunk", "muted", "end")
    b += brace(LEFT, LEFT + 14 * BYTE, y + ROW, "tags_: 0 is empty, otherwise the top byte of the hash")

    y += 84
    b += row(LEFT, y, [("scale", "dist", 2), ("hosted", "ovf", 2)], unit=BYTE * 2)
    b += text(LEFT - 12, y + 20, "ctl", "muted", "end")
    b += text(LEFT + 8 * BYTE + 14, y + 13, "four bits of capacity scale, in chunk 0 only, and four", "muted")
    b += text(LEFT + 8 * BYTE + 14, y + 29, "bits counting the keys hosted here that belong elsewhere", "muted")

    y += 54
    b += row(LEFT, y, [("saturating count", "ovf", 8)])
    b += text(LEFT - 12, y + 20, "out", "muted", "end")
    b += text(LEFT + 8 * BYTE + 14, y + 13, "how many keys wanted this chunk and did not fit,", "muted")
    b += text(LEFT + 8 * BYTE + 14, y + 29, "including those that had already passed a full one", "muted")

    y += 60
    b += row(LEFT, y, [("", "payload", 1)] * 14)
    b += text(LEFT - 12, y + 20, "items", "muted", "end")
    b += brace(LEFT, LEFT + 14 * BYTE, y + ROW, "the key and the value, in the chunk")
    return svg(y + ROW + 40, b)


def emhash8_index():
    """emhash8: a chain through the index, and a fingerprint in the spare bits."""
    b = anatomy("emhash8: 8 index bytes per bucket, and no key",
                [("next", "dist", 4), ("slot", "idx", 4)])
    y = ANATOMY_H + 22
    b += text(20, y - 12, "the slot word, when the table has fewer than 2^32 slots", "hd")
    b += row(LEFT, y, [("hash bits", "fp", 5), ("slot", "idx", 3)])
    b += text(LEFT - 12, y + 20, "slot", "muted", "end")
    b += brace(LEFT, LEFT + 5 * BYTE, y + ROW, "everything above log2(bucket count)")
    b += text(LEFT + 8 * BYTE + 14, y + 13, "a fingerprint that costs nothing, because", "muted")
    b += text(LEFT + 8 * BYTE + 14, y + 29, "the word has to be loaded anyway", "muted")

    y += 90
    b += text(20, y - 12, "the chain, threaded through the index", "hd")
    b += row(LEFT, y, [("next 2", "dist", 2), ("--", "", 2), ("next 4", "dist", 2), ("--", "", 2),
                       ("end", "dist", 2), ("--", "", 2)])
    b += text(LEFT - 12, y + 20, "index[]", "muted", "end")
    b += (f'  <path d="M{LEFT + 1 * BYTE},{y + ROW} C{LEFT + 1 * BYTE},{y + ROW + 26} '
          f'{LEFT + 5 * BYTE},{y + ROW + 26} {LEFT + 5 * BYTE},{y + ROW + 2}" class="arrow"/>\n')
    b += (f'  <path d="M{LEFT + 5 * BYTE},{y + ROW} C{LEFT + 5 * BYTE},{y + ROW + 40} '
          f'{LEFT + 9 * BYTE},{y + ROW + 40} {LEFT + 9 * BYTE},{y + ROW + 2}" class="arrow"/>\n')
    y += 96
    b += row(LEFT, y, [(str(i), "payload", 2) for i in range(6)])
    b += text(LEFT - 12, y + 20, "values", "muted", "end")
    b += brace(LEFT, LEFT + 12 * BYTE, y + ROW, "a dense vector, in insertion order")
    return svg(y + ROW + 40, b)


def emilib_state():
    """emilib: a state byte per slot, homes rounded to a group."""
    b = anatomy("emilib: 16 state bytes for 16 slots", [("states[]", "fp", 16)])
    y = ANATOMY_H + 22
    st = [("80", "", 1), ("A2", "fp", 1), ("80", "", 1), ("del", "ovf", 1),
          ("11", "fp", 1), ("A2", "fp", 1), ("80", "", 1), ("7C", "fp", 1),
          ("80", "", 1), ("3F", "fp", 1), ("del", "ovf", 1), ("80", "", 1),
          ("62", "fp", 1), ("80", "", 1), ("A2", "fp", 1), ("18", "fp", 1)]
    b += row(LEFT, y, st)
    b += text(LEFT - 12, y + 20, "states[]", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW,
               "empty is -128, deleted its own value, otherwise the hash mod 253")

    y += 84
    b += row(LEFT, y, [("", "payload", 1)] * 16)
    b += text(LEFT - 12, y + 20, "slots[]", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW, "the key and the value, in the slot")
    return svg(y + ROW + 40, b)


def indivi_metagroup():
    """indivi: fragments, counters an erase can undo, and distance nibbles."""
    b = anatomy("indivi flat_umap: 32 metadata bytes for 16 slots",
                [("hfrags", "fp", 16), ("oflws", "ovf", 8), ("dists", "dist", 8)])
    y = ANATOMY_H + 22
    b += row(LEFT, y, [(t, "fp", 1) for t in ["9C", "--", "41", "9C", "07", "--", "B3", "5A",
                                              "--", "22", "9C", "8E", "--", "6D", "F1", "--"]])
    b += text(LEFT - 12, y + 20, "hfrags", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW, "one byte per slot, compared with one SIMD instruction")

    y += 84
    b += row(LEFT, y, [(str(v), "ovf", 2) for v in [0, 1, 0, 0, 2, 0, 0, 0]])
    b += text(LEFT - 12, y + 20, "oflws", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW,
               "8 counters, one per hash class, drawn across the sixteen slots they cover")

    y += 84
    b += row(LEFT, y, [(str(v), "dist", 1) for v in [0, 1, 0, 0, 2, 0, 1, 0, 0, 0, 3, 0, 1, 0, 0, 0]])
    b += text(LEFT - 12, y + 20, "dists", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW,
               "a four bit distance from home per slot, two to a byte")
    return svg(y + ROW + 44, b)


def group_block():
    """unordered_dense 5.0: the 88 byte block."""
    b = anatomy("unordered_dense 5.0: an 88 byte block for 16 slots, in one allocation",
                [("fingerprints", "fp", 16), ("overflow", "ovf", 8), ("value index", "idx", 64)])
    y = ANATOMY_H + 22
    b += row(LEFT, y, [(t, "fp", 1) for t in ["9C", "--", "41", "9C", "07", "--", "B3", "5A",
                                              "--", "22", "9C", "8E", "--", "6D", "F1", "--"]])
    b += text(LEFT - 12, y + 20, "fingerprints", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW, "one byte per slot: 0 is empty, otherwise the low byte of the hash")

    y += 84
    b += row(LEFT, y, [(str(v), "ovf", 2) for v in [0, 1, 0, 0, 2, 0, 0, 0]])
    b += text(LEFT - 12, y + 20, "overflow", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW,
               "8 counters, one per fingerprint class, drawn across the sixteen slots they cover")

    y += 84
    b += row(LEFT, y, [(str(v), "idx", 1) for v in [7, 0, 3, 12, 5, 0, 1, 9, 0, 4, 2, 8, 0, 6, 11, 0]])
    b += text(LEFT - 12, y + 20, "value index", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW, "a uint32_t per slot, four bytes each")

    y += 108
    b += row(LEFT, y, [(str(i), "payload", 2) for i in range(8)])
    b += text(LEFT - 12, y + 20, "m_values", "muted", "end")
    b += brace(LEFT, LEFT + 16 * BYTE, y + ROW, "a std::vector<std::pair<Key, T>> in insertion order")
    for src, dst in ((0, 7), (2, 3), (9, 4)):
        x0 = LEFT + src * BYTE + BYTE / 2
        x1 = LEFT + dst * 2 * BYTE + BYTE
        b += f'  <path d="M{x0},{y - 46} C{x0},{y - 22} {x1},{y - 22} {x1},{y - 2}" class="arrow"/>\n'
    return svg(y + ROW + 44, b)


def verstable_word():
    """Verstable: a chain in sixteen bits."""
    b = text(20, 24, "Verstable: two metadata bytes per bucket, used as sixteen bits", "hd")
    y = 44
    # to scale: one cell is one bit
    u = 30
    b += row(LEFT, y, [("fragment", "fp", 4), ("home", "ovf", 1), ("displacement", "dist", 11)], unit=u)
    b += text(LEFT - 12, y + 20, "uint16_t", "muted", "end")
    b += text(LEFT + 16 * u + 10, y + 20, "16 bits", "muted")
    for i, (lab, n) in enumerate((("4 bits", 0), ("1", 4), ("11 bits", 5))):
        b += text(LEFT + n * u, y + ROW + 18, lab, "muted")

    y = 150
    b += text(20, y - 12, "the chain: a lookup visits only buckets holding keys that belong to it", "hd")
    bu = BYTE
    b += row(LEFT, y, [("", "", 1), ("home", "fp", 1), ("", "", 1), ("", "", 1), ("+5", "fp", 1),
                       ("", "", 1), ("", "", 1), ("", "", 1), ("end", "fp", 1), ("", "", 1)], unit=bu)
    b += text(LEFT - 12, y + 20, "buckets", "muted", "end")
    b += (f'  <path d="M{LEFT + 1.5 * bu},{y + ROW} C{LEFT + 1.5 * bu},{y + ROW + 26} '
          f'{LEFT + 4.5 * bu},{y + ROW + 26} {LEFT + 4.5 * bu},{y + ROW + 2}" class="arrow"/>\n')
    b += (f'  <path d="M{LEFT + 4.5 * bu},{y + ROW} C{LEFT + 4.5 * bu},{y + ROW + 42} '
          f'{LEFT + 8.5 * bu},{y + ROW + 42} {LEFT + 8.5 * bu},{y + ROW + 2}" class="arrow"/>\n')
    return svg(y + ROW + 66, b)


def ihtab_group():
    """ihtab: eight slots, half load, and a dense element array."""
    b = anatomy("ihtab: a 40 byte group for 8 slots",
                [("tags", "fp", 8), ("indices", "idx", 32)])
    y = ANATOMY_H + 22
    b += row(LEFT, y, [(t, "fp", 1) for t in ["c0", "5A", "80", "11", "c0", "7F", "3C", "c0"]])
    b += text(LEFT - 12, y + 20, "tags", "muted", "end")
    b += brace(LEFT, LEFT + 8 * BYTE, y + ROW, "0xc0 empty, 0x80 deleted, otherwise the top 7 bits")
    b += text(LEFT + 8 * BYTE + 14, y + 13, "both markers have the top two bits set, so one", "muted")
    b += text(LEFT + 8 * BYTE + 14, y + 29, "and-shift-movemask finds every empty slot", "muted")

    y += 84
    b += row(LEFT, y, [(str(v), "idx", 1) for v in [0, 3, 0, 7, 0, 1, 5, 0]])
    b += text(LEFT - 12, y + 20, "indices", "muted", "end")
    b += brace(LEFT, LEFT + 8 * BYTE, y + ROW, "a uint32_t per slot, four bytes each")

    y += 84
    b += row(LEFT, y, [(str(i), "payload" if i not in (2, 4) else "sent", 1) for i in range(8)])
    b += text(LEFT - 12, y + 20, "els", "muted", "end")
    b += brace(LEFT, LEFT + 8 * BYTE, y + ROW, "appended in order, never compacted: the hatched two are erased")
    return svg(y + ROW + 44, b)


def families():
    """flat, dense, node: what a lookup has to touch."""
    col = [20, 268, 516]
    wide = 224
    titles = ["flat", "dense", "node"]
    subs = [["SwissTable, boost,", "F14, indivi, emilib"],
            ["unordered_dense,", "emhash8, F14Vector, ihtab"],
            ["std::unordered_map,", "the node maps"]]
    b = text(20, 24, "three ways to hold the keys, and what one lookup touches", "hd")
    for x, t, ss in zip(col, titles, subs):
        b += text(x, 54, t, "hd")
        for i, line in enumerate(ss):
            b += text(x, 72 + i * 15, line, "muted")

    def column(x, rows):
        out = ""
        yy = 104
        for label, cls, h in rows:
            out += f'  <rect x="{x}" y="{yy}" width="{wide}" height="{h}" class="cell {cls}"/>\n'
            out += text(x + wide / 2, yy + h / 2 + 4, label, "mono", "middle")
            yy += h + 14
        return out, yy

    c0, e0 = column(col[0], [("metadata bytes", "fp", 30), ("key and value here", "payload", 44)])
    c1, e1 = column(col[1], [("metadata bytes", "fp", 30), ("index into the values", "idx", 30),
                             ("key and value, in a vector", "payload", 44)])
    c2, e2 = column(col[2], [("metadata bytes", "fp", 30), ("pointer to a node", "idx", 30),
                             ("key and value, on the heap", "payload", 44)])
    b += c0 + c1 + c2
    ymax = max(e0, e1, e2)
    for x, s2 in ((col[0], "2 loads, 1 region"), (col[1], "3 loads, 2 regions"),
                  (col[2], "3 loads, a node each")):
        b += text(x + wide / 2, ymax + 6, s2, "muted", "middle")
    y = ymax + 34
    return svg(y + 78, b)


def lookup_touches():
    """One hit, per design: what is on the dependent chain of loads."""
    steps = [
        ("robin hood (4.11.0)", ["hash", "bucket word", "value"], ""),
        ("SwissTable, boost, F14", ["hash", "control bytes", "key in the slot"], ""),
        ("indivi, emilib", ["hash", "metadata group", "key in the slot"], ""),
        ("group index (5.0)", ["hash", "16 fingerprints", "index, same block", "value"], ""),
        ("ihtab", ["hash", "8 tags", "index, same group", "element"], ""),
        ("emhash8", ["hash", "index word", "value"], "+1 per chain step"),
        ("Verstable", ["hash", "16 bit word", "key in the bucket"], "+1 per chain step"),
        ("std::unordered_map", ["hash", "bucket pointer", "node"], "+1 per chain step"),
    ]
    b = text(20, 24, "what a hit waits for: the chain of loads, left to right", "hd")
    y = 48
    x0 = 176
    bw = 116
    for name, chain, extra in steps:
        b += text(x0 - 12, y + 20, name, "lbl", "end")
        x = x0
        for i, s2 in enumerate(chain):
            cls = "dist" if i == 0 else ("fp" if i == 1 else
                                         ("idx" if i < len(chain) - 1 else "payload"))
            b += f'  <rect x="{x}" y="{y}" width="{bw}" height="{ROW}" class="cell {cls}"/>\n'
            size = 11 if len(s2) * 6.6 < bw else (9 if len(s2) * 5.6 < bw else 8)
            b += (f'  <text x="{x + bw / 2}" y="{y + 20}" class="mono" text-anchor="middle" '
                  f'font-size="{size}">{esc(s2)}</text>\n')
            if i + 1 < len(chain):
                b += f'  <path d="M{x + bw},{y + ROW / 2} L{x + bw + 12},{y + ROW / 2}" class="arrow"/>\n'
            x += bw + 12
        if extra:
            b += text(x + 4, y + 20, extra, "muted")
        y += ROW + 14
    return svg(y + 96, b)


FIGURES = {
    "hashmap-basics": hashmap_basics,
    "rh-bucket": rh_bucket,
    "swiss-group": swiss_group,
    "boost-group15": boost_group15,
    "f14-chunk": f14_chunk,
    "emhash8-index": emhash8_index,
    "emilib-state": emilib_state,
    "indivi-metagroup": indivi_metagroup,
    "group-block": group_block,
    "verstable-word": verstable_word,
    "ihtab-group": ihtab_group,
    "families": families,
    "lookup-touches": lookup_touches,
}


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "doc/hashmap-index"
    os.makedirs(outdir, exist_ok=True)
    for name, fn in FIGURES.items():
        path = os.path.join(outdir, name + ".svg")
        with open(path, "w") as f:
            f.write(fn())
        print(path)


if __name__ == "__main__":
    main()
