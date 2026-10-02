#!/usr/bin/env python3
"""map_view and set_view are read only (#299), and saying so is a compile error.

The table puts `static_assert(!is_view_v, ...)` into the private functions every write passes
through, not into each public writer, so a writer added later gets the message without anyone
remembering it. A static_assert in a body cannot be detected with SFINAE, so this compiles each
writer on its own and requires the message; and compiles the reads, so that the test cannot pass
because nothing compiles at all.

    test_view_readonly.py <include dir> <cpp_std or none> <compiler command...>

Registered in test/meson.build for gcc and clang.
"""

import concurrent.futures
import os
import subprocess
import sys
import tempfile

PRELUDE = """#include <ankerl/unordered_dense.h>
#include <utility>
using map_view = ankerl::unordered_dense::map_view<int, int>;
using set_view = ankerl::unordered_dense::set_view<int>;
void f(map_view& v, set_view& s, map_view const& other) {
    (void)v; (void)s; (void)other;
    %s;
}
"""

READS = [
    "auto it = v.find(1); (void)it",
    "(void)v.contains(1); (void)v.count(1); (void)v.at(1); (void)v.equal_range(1)",
    "(void)s.contains(1)",
    "for (auto const& kv : v) { (void)kv; }",
    "(void)v.verify(); (void)v.index(); (void)v.values(); (void)v.size(); (void)v.bucket_count()",
    "map_view copy(other); map_view moved(std::move(copy)); (void)moved",
    "map_view def; (void)def",
]

WRITES = [
    "v.insert({1, 1})",
    "v.emplace(1, 1)",
    "v.try_emplace(1, 1)",
    "v.insert_or_assign(1, 1)",
    "v[1]",
    "v.erase(1)",
    "v.erase(v.begin())",
    "v.clear()",
    "v.rehash(64)",
    "v.reserve(64)",
    "v.max_load_factor(0.5F)",
    "v.swap(v)",
    "v = other",
    "v = map_view()",
    "(void)std::move(v).extract()",
    "s.insert(1)",
    "s.erase(1)",
]


def compile_snippet(cmd, include, std, body):
    with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False) as f:
        f.write(PRELUDE % body)
        name = f.name
    try:
        args = cmd + ["-fsyntax-only", "-I", include, name]
        if std != "none":
            args.insert(len(cmd), "-std=" + std)
        r = subprocess.run(args, capture_output=True, text=True)
        return r.returncode, r.stdout + r.stderr
    finally:
        os.unlink(name)


def main():
    include, std, cmd = sys.argv[1], sys.argv[2], sys.argv[3:]
    failed = 0
    # the reads in one file, one block per line, so an error still names which; the writes one per file,
    # in parallel, because each must fail on its own
    code, out = compile_snippet(cmd, include, std, "{ " + "; }\n    { ".join(READS) + "; }")
    if code != 0:
        print(f"FAIL: a read does not compile\n{out[:2000]}")
        failed += 1
    with concurrent.futures.ThreadPoolExecutor() as pool:
        results = list(pool.map(lambda body: compile_snippet(cmd, include, std, body), WRITES))
    for body, (code, out) in zip(WRITES, results):
        if code == 0 or "read only" not in out:
            print(f"FAIL: a write {'compiles' if code == 0 else 'fails without the message'}: {body}\n{out[:2000]}")
            failed += 1
    print(f"{len(READS)} reads, {len(WRITES)} writes, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
