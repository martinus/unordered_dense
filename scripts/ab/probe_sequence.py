#!/usr/bin/env python3
"""Writes a copy of ankerl/unordered_dense.h that walks another probe sequence (#355).

    scripts/ab/probe_sequence.py <variant> <in header> <out header>
    scripts/ab/probe_sequence.py --list
    scripts/ab/probe_sequence.py --check      # every variant visits every group, 4 to 2^16 groups

Every walk over the sequence goes through one function afterwards, `advance(group_idx, delta,
word, mask)`: the lookup (probe_from, probe_after_home), the placement (place_group and the
rehash's own copy in fill_buckets_from_values), the counter walk back (uncount, from
erase_group_slot and move_home), and the two searches by value (slot_of_value, repoint_value).
`delta` is the number of steps taken, so the bound `delta == m_group_mask` keeps its meaning. A
variant that visits some group twice before it has seen all of them raises the bound by as many
steps as that (`extra`); --check proves the bound for every fingerprint and every array size.

A variant is the distance of step `delta` (1, 2, ...) as a C expression of `delta` and the
fingerprint word, and the same in Python for --check. The variants and what each one asks:

  tri     the header as it is
  trix    triangular through advance(): what the plumbing costs on its own
  dh      2*fp + 1 from the first step, folly's probeDelta. step mod 16 = 2*class + 1, so on 16
          groups or fewer every key of one counter class walks the same sequence
  dh1     home+1 first, then dh: keeps the adjacent block, which the prefetcher serves
  dh2     two triangular steps (+1, +2), then dh
  dhhi    the step from the fingerprint's five high bits, which the class (fp & 7) does not use
  dh1hi   dh1 with that step
  dhcls   the step from the class alone, which the walk has in a register anyway: 1, 3, ..., 15
  dhv     dh with the step's input behind an empty asm volatile, so it cannot be hoisted onto the
          hit path. Diagnostic only; gcc and clang
  tric1   first step 1 + 2*class, triangular after it: spreads a full group's overflow over eight
          neighbours by class, and needs no register the triangular walk does not have
  dhclsv  dhcls and tric1v tric1 with the class behind the asm volatile: the class is live
  tric1v  anyway, so the step can be made where it is used without a register of its own
  <v>peel  any variant (tri included) with the home group peeled off every walk that starts at
          home and runs per operation: probe() for a key whose compare is cheap (the shape it
          already has for a key whose compare is a call), place_group, slot_of_value and
          repoint_value. Whatever the walk needs past home is then computed after the home group
          and not in front of it, where the compiler hoists anything loop-invariant
  dhp     dh *packed*: delta becomes a std::uint64_t `steps << 9 | step` that starts at the step,
          so the step lives in the counter's register instead of its own. Clang's scalar
          evolution sees that `delta & 511` never changes and splits it out again
  dhq     packed the other way round, `step << 32 | steps`: the bound compares the low half, and
          `delta >> 32` is not provably invariant (a carry out of the low half would change it),
          so it is recomputed where it is used. Needs value_idx_type to be 32 bits
"""

import sys

FP = "2U * (word & 0xFFU) + 1U"
HI = "2U * ((word >> 3U) & 0x1FU) + 1U"
VARIANTS = {
    "tri": None,
    "trix": dict(c="delta", py=lambda d, fp: d),
    "dh": dict(c=FP, py=lambda d, fp: 2 * fp + 1),
    "dh1": dict(c=f"delta == 1U ? 1U : {FP}", py=lambda d, fp: 1 if d == 1 else 2 * fp + 1, extra=1),
    "dh2": dict(c=f"delta <= 2U ? delta : {FP}", py=lambda d, fp: d if d <= 2 else 2 * fp + 1, extra=2),
    "dhhi": dict(c=HI, py=lambda d, fp: 2 * (fp >> 3) + 1),
    "dh1hi": dict(c=f"delta == 1U ? 1U : {HI}", py=lambda d, fp: 1 if d == 1 else 2 * (fp >> 3) + 1, extra=1),
    "dhcls": dict(c="2U * (word & 7U) + 1U", py=lambda d, fp: 2 * (fp & 7) + 1),
    "dhv": dict(c="2U * (opaque(word) & 0xFFU) + 1U", py=lambda d, fp: 2 * fp + 1),
    "tric1": dict(c="delta == 1U ? 2U * (word & 7U) + 1U : delta - 1U", py=lambda d, fp: 2 * (fp & 7) + 1 if d == 1 else d - 1, extra=1),
    "dhp": dict(packed=FP, py=lambda d, fp: 2 * fp + 1),
    "dhq": dict(packed=FP, high=True, py=lambda d, fp: 2 * fp + 1),
    "dhclsv": dict(c="2U * opaque(word & 7U) + 1U", py=lambda d, fp: 2 * (fp & 7) + 1),
    "tric1v": dict(c="delta == 1U ? 2U * opaque(word & 7U) + 1U : delta - 1U", py=lambda d, fp: 2 * (fp & 7) + 1 if d == 1 else d - 1, extra=1),
}


def offsets(variant, fp, groups):
    """The group offsets from home a variant visits within its bound."""
    spec = VARIANTS[variant] or dict(py=lambda d, fp: d)
    g, delta, out = 0, 0, [0]
    while delta != groups - 1 + spec.get("extra", 0):
        delta += 1
        g = (g + spec["py"](delta, fp)) % groups
        out.append(g)
    return out


def check():
    for v in VARIANTS:
        for bits in range(2, 17):
            groups = 1 << bits
            for fp in range(1, 256):
                seen = set(offsets(v, fp, groups))
                assert len(seen) == groups, f"{v}: {groups} groups, fp {fp}: visits {len(seen)}"
        print(f"{v}: every group within the bound, 4 to 65536 groups, every fingerprint")


NEXT_GROUP = """    // quadratic: the triangular numbers reach every group of a power-of-two array
    [[nodiscard]] auto next_group(value_idx_type group_idx, value_idx_type& delta) const -> value_idx_type {
        return static_cast<value_idx_type>((group_idx + (++delta)) & m_group_mask);
    }
"""


PROBE_INTEGER = """        if constexpr (!detail::key_compare_is_call_v<Key>) {
            return probe_from(key, word, counter, home_idx, 0);
        } else {
"""


def peel_loop(text, anchor):
    """The first `while (true) {` loop after `anchor`, with its body copied once in front of it as a
    plain block: the first iteration, which is the home group, runs before the loop."""
    assert text.count(anchor) == 1, f"{anchor!r} changed; update scripts/ab/probe_sequence.py"
    head = "while (true) {\n"
    start = text.index(head, text.index(anchor))
    depth, i = 1, start + len(head)
    while depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    body = text[start + len(head) : i - 1]
    indent = text[text.rindex("\n", 0, start) + 1 : start]
    return text[:start] + "{\n" + body + "}\n" + indent + text[start:]


def patch(variant, text):
    if not variant.endswith("peel"):
        return patch_sequence(variant, text)
    text = patch_sequence(variant[: -len("peel")], text)
    assert text.count(PROBE_INTEGER) == 1, "probe() changed; update scripts/ab/probe_sequence.py"
    text = text.replace(PROBE_INTEGER, "        {\n")
    for anchor in (
        "place_group(std::uint32_t word, unsigned counter, value_idx_type group_idx, value_idx_type value_idx) {",
        "auto slot_of_value(std::uint64_t mh, value_idx_type value_idx) const -> group_slot {",
        "void repoint_value(std::uint64_t mh, value_idx_type value_idx, value_idx_type new_value_idx) {",
    ):
        text = peel_loop(text, anchor)
    return text


def patch_sequence(variant, text):
    spec = VARIANTS[variant]
    if spec is None:
        return text

    def sub(name, old, new, count=1):
        nonlocal text
        n = text.count(old)
        assert n == count, f"{name}: expected {count} matches, found {n}; update scripts/ab/probe_sequence.py"
        text = text.replace(old, new)

    if "packed" in spec:
        high = spec.get("high", False)
        start = f"std::uint64_t{{{spec['packed']}}} << 32U" if high else spec["packed"]
        step = "delta >> 32U" if high else "delta & 511U"
        count = "static_cast<std::uint32_t>(delta)" if high else "(delta >> 9U)"
        sub(
            "next_group",
            NEXT_GROUP,
            f"""    // probe_sequence.py variant {variant}: the step and the steps taken packed into delta, and
    // every walk over the sequence is advance()
    [[nodiscard]] static constexpr auto probe_start(std::uint32_t word) -> std::uint64_t {{
        return {start};
    }}
    [[nodiscard]] static constexpr auto
    advance(value_idx_type group_idx, std::uint64_t& delta, std::uint32_t word, value_idx_type mask) -> value_idx_type {{
        (void)word;
        delta += {"1U" if high else "512U"};
        return static_cast<value_idx_type>((group_idx + ({step})) & mask);
    }}
    [[nodiscard]] auto next_group(value_idx_type group_idx, std::uint64_t& delta, std::uint32_t word) const
        -> value_idx_type {{
        return advance(group_idx, delta, word, m_group_mask);
    }}
""",
        )
        sub("probe_from", "value_idx_type group_idx, value_idx_type delta) const", "value_idx_type group_idx, std::uint64_t delta) const", 2)
        sub("probe()", "probe_from(key, word, counter, home_idx, 0);", "probe_from(key, word, counter, home_idx, probe_start(word));")
        sub("starts", "value_idx_type delta = 0;", "std::uint64_t delta = probe_start(word);", 6)
        sub("bound", "delta == m_group_mask)", f"{count} == m_group_mask)", 3)
    else:
        sub(
            "next_group",
            NEXT_GROUP,
            f"""    // probe_sequence.py variant {variant}: every walk over the sequence is advance()
    static auto opaque(std::uint32_t w) -> std::uint32_t {{
        __asm__ volatile("" : "+r"(w));
        return w;
    }}
    [[nodiscard]] static auto
    advance(value_idx_type group_idx, value_idx_type& delta, std::uint32_t word, value_idx_type mask) -> value_idx_type {{
        (void)word;
        ++delta;
        return static_cast<value_idx_type>((group_idx + ({spec["c"]})) & mask);
    }}
    [[nodiscard]] auto next_group(value_idx_type group_idx, value_idx_type& delta, std::uint32_t word) const
        -> value_idx_type {{
        return advance(group_idx, delta, word, m_group_mask);
    }}
""",
        )
        if spec.get("extra"):
            sub("bound", "delta == m_group_mask)", f"delta == m_group_mask + {spec['extra']}U)", 3)

    # probe_from, place_group, slot_of_value, repoint_value: each has `word` in scope
    sub("walks", "group_idx = next_group(group_idx, delta);", "group_idx = next_group(group_idx, delta, word);", 4)
    sub("probe_after_home", "next_group(home_idx, delta);", "next_group(home_idx, delta, word);")
    # uncount and fill_buckets_from_values write the step out, with the mask in a local
    sub(
        "inline walks",
        "group_idx = static_cast<value_idx_type>((group_idx + (++delta)) & mask);",
        "group_idx = advance(group_idx, delta, word, mask);",
        2,
    )
    sub(
        "uncount",
        "uncount(Group* groups, value_idx_type mask, value_idx_type home_idx, unsigned counter, value_idx_type found_in) {",
        "uncount(Group* groups, value_idx_type mask, value_idx_type home_idx, std::uint32_t word, value_idx_type found_in) {\n"
        "        auto const counter = word & 7U;",
    )
    sub("uncount's callers", "        auto const counter = fingerprint_word(mh) & 7U;\n", "        auto const word = fingerprint_word(mh);\n", 2)
    sub("uncount's callers", "uncount(groups, mask, home_idx, counter, found_in);", "uncount(groups, mask, home_idx, word, found_in);", 2)
    for leftover in ("(++delta)", "next_group(group_idx, delta)", "next_group(home_idx, delta)"):
        assert leftover not in text, f"a triangular walk is left: {leftover}"
    return text


def main():
    if sys.argv[1:] == ["--list"]:
        print(" ".join(VARIANTS), " ".join(v + "peel" for v in VARIANTS))
    elif sys.argv[1:] == ["--check"]:
        check()
    else:
        variant, src, dst = sys.argv[1:4]
        with open(src, encoding="utf-8") as f:
            text = f.read()
        with open(dst, "w", encoding="utf-8") as f:
            f.write(patch(variant, text))


main()
