#!/usr/bin/env python3
"""Write one header per shape of the insert path into OUT/shapes/<name>/ankerl/.

    gen.py HEADER OUT

A shape is four switches, each fixed for every compiler:

  hit   do_find_or_place, the lookup in the home group: inlined into the caller (i) or called (c)
  miss  find_or_place_miss, everything after a home-group miss: inlined (i) or called (c)
  far   find_or_place_far, the walk past home: inlined into the miss path (i) or called (c)
  flat  flatten on append_value, the value container's emplace_back: on (F) or off (n)

The switches are applied by rewriting the attribute in front of those three functions and the
ANKERL_UNORDERED_DENSE_FLATTEN define, so this works on any header that still has them.
"""
import itertools
import os
import re
import sys

header, out = sys.argv[1], sys.argv[2]
base = open(header).read()
stl = open(os.path.join(os.path.dirname(header), "stl.h")).read()


def set_attr(s, fn, attr):
    pat = re.compile(r"ANKERL_UNORDERED_DENSE_(?:FORCEINLINE|NOINLINE)(\s+auto\s+" + fn + r"\()")
    s, n = pat.subn("ANKERL_UNORDERED_DENSE_" + attr + r"\1", s)
    assert n == 1, f"{fn}: {n} matches"
    return s


names = []
for hit, miss, far, flat in itertools.product("ic", "ic", "ic", "Fn"):
    s = base
    for fn, v in (("do_find_or_place", hit), ("find_or_place_miss", miss), ("find_or_place_far", far)):
        s = set_attr(s, fn, "NOINLINE" if v == "c" else "FORCEINLINE")
    if flat == "n":
        s, n = re.subn(r"(#    define ANKERL_UNORDERED_DENSE_FLATTEN) __attribute__\(\(flatten\)\)", r"\1", s)
        assert n == 1, "flatten"
    name = f"h{hit}_m{miss}_f{far}_{flat}"
    d = os.path.join(out, "shapes", name, "ankerl")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "unordered_dense.h"), "w").write(s)
    open(os.path.join(d, "stl.h"), "w").write(stl)
    names.append(name)
open(os.path.join(out, "names.txt"), "w").write("\n".join(names) + "\n")
print(len(names), "shapes")
