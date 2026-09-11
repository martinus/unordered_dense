#!/usr/bin/env python3
"""Check the LLDB data formatters against ground truth the fixture prints itself.

Skips (exit 77, meson's "skipped") when no lldb is installed, so it costs
nothing on a machine or CI leg without one.

    scripts/test_lldb_formatters.py [--lldb lldb-18] [--cxx g++]
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SKIP = 77


def find_lldb(explicit):
    for name in ([explicit] if explicit else []) + ["lldb"] + ["lldb-%d" % v for v in range(22, 14, -1)]:
        found = shutil.which(name)
        if found:
            return found
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lldb")
    ap.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    ap.add_argument("--formatter", default=os.path.join(ROOT, "lldb", "unordered_dense.py"))
    ap.add_argument("--fixture", default=os.path.join(ROOT, "test", "lldb", "fixture.cpp"))
    ap.add_argument("--include", default=os.path.join(ROOT, "include"))
    args = ap.parse_args()

    lldb = find_lldb(args.lldb)
    if lldb is None:
        print("SKIP: no lldb found")
        return SKIP
    # An lldb on PATH is not the same as an lldb that can debug what this platform builds, and the
    # two differ exactly on Windows: the runners there have one, so the check above does not fire,
    # the fixture is built by whatever `c++` is on PATH rather than by the leg's MSVC, and the
    # session then produces nothing at all. All four Windows legs failed on that. Everywhere else
    # this runs for real -- every Linux variant including ARM64 and libc++, and macOS arm64.
    if os.name == "nt":
        print("SKIP: no lldb here that can debug what this platform builds")
        return SKIP
    if not os.path.exists(args.formatter):
        print("SKIP: no formatter at %s" % args.formatter)
        return SKIP

    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "fixture")
        build = [args.cxx, "-std=c++17", "-g", "-O0", "-I", args.include, args.fixture, "-o", exe]
        proc = subprocess.run(build, capture_output=True, text=True)
        if proc.returncode != 0:
            print("FAIL: fixture did not build\n%s" % proc.stderr[-3000:])
            return 1

        # Ground truth, straight from the containers.
        run = subprocess.run([exe], capture_output=True, text=True, timeout=120)
        if run.returncode != 0:
            print("FAIL: fixture did not run\n%s" % run.stderr[-2000:])
            return 1
        expect_summary, expect_elem = {}, {}
        for line in run.stdout.splitlines():
            parts = line.split()
            if parts[:1] == ["GT"]:
                expect_summary[parts[1]] = (parts[2], parts[3])
            elif parts[:1] == ["ELEM"]:
                expect_elem[(parts[1], int(parts[2]))] = int(parts[3])

        if not expect_summary:
            print("FAIL: fixture printed no ground truth")
            return 1

        # Ask LLDB the same questions.
        probe = os.path.join(tmp, "probe.py")
        with open(probe, "w") as fh:
            fh.write(PROBE % {"names": repr(sorted(expect_summary)), "elems": repr(sorted(expect_elem))})
        commands = os.path.join(tmp, "cmds")
        with open(commands, "w") as fh:
            fh.write(
                "command script import %s\n"
                "breakpoint set --file fixture.cpp --source-pattern-regexp BREAKPOINT\n"
                "run\n"
                "command script import %s\n"
                "quit\n" % (args.formatter, probe)
            )
        out = subprocess.run([lldb, "-b", "-s", commands, exe], capture_output=True, text=True, timeout=600)
        text = out.stdout + out.stderr

    got_summary = dict(re.findall(r"^SUMMARY (\S+) (.*)$", text, re.M))
    got_elem = {(m[0], int(m[1])): m[2] for m in re.findall(r"^ELEMENT (\S+) (\d+) (.*)$", text, re.M)}
    if not got_summary:
        print("FAIL: the formatter produced no output. lldb said:\n%s" % text[-3000:])
        return 1

    failures = []
    for name, (size, buckets) in sorted(expect_summary.items()):
        want = "size=%s" % size + ("" if buckets == "-" else " bucket_count=%s" % buckets)
        have = got_summary.get(name, "<missing>")
        if have != want:
            failures.append("%s: summary is %r, expected %r" % (name, have, want))
    for (name, index), value in sorted(expect_elem.items()):
        have = got_elem.get((name, index), "<missing>")
        if have != str(value):
            failures.append("%s[%d]: is %r, expected %r" % (name, index, have, str(value)))

    for line in failures:
        print("FAIL: %s" % line)
    checked = len(expect_summary) + len(expect_elem)
    print("%s: %d checks, %d failed" % ("FAIL" if failures else "PASS", checked, len(failures)))
    return 1 if failures else 0


PROBE = '''
import lldb

def _second_or_self(v):
    s = v.GetNonSyntheticValue().GetChildMemberWithName("second")
    return s if s.IsValid() else v

def __lldb_init_module(debugger, internal_dict):
    frame = debugger.GetSelectedTarget().GetProcess().GetSelectedThread().GetFrameAtIndex(0)
    for name in %(names)s:
        v = frame.FindVariable(name)
        print("SUMMARY %%s %%s" %% (name, v.GetSummary() if v.IsValid() else "<no such variable>"))
    for name, index in %(elems)s:
        v = frame.FindVariable(name)
        if not v.IsValid() or index >= v.GetNumChildren():
            print("ELEMENT %%s %%d <out of range>" %% (name, index))
            continue
        print("ELEMENT %%s %%d %%s" %% (name, index, _second_or_self(v.GetChildAtIndex(index)).GetValueAsSigned()))
'''

if __name__ == "__main__":
    sys.exit(main())
