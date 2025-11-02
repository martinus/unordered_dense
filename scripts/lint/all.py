#!/usr/bin/env python3

from pathlib import Path
from subprocess import run
from time import perf_counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import os


def _run_linter(path: Path):
    """Run a single linter and return (name, returncode, duration, stdout, stderr).

    We capture output so concurrent runs don't interleave in the terminal.
    """
    name = path.name
    start = perf_counter()
    res = run([str(path)], capture_output=True, text=True)
    duration = perf_counter() - start
    return {
        "name": name,
        "path": path,
        "returncode": res.returncode,
        "duration": duration,
        "stdout": res.stdout,
        "stderr": res.stderr,
    }


def main():
    start = perf_counter()
    linters_dir = Path(__file__).parent
    linters = sorted(
        p for p in linters_dir.iterdir() if p.is_file() and p.name.startswith("lint-")
    )

    results = []
    rc = 0

    max_workers = min(32, len(linters))
    with ThreadPoolExecutor(max_workers=max_workers) as ex:
        futures = {ex.submit(_run_linter, lint): lint for lint in linters}
        for fut in as_completed(futures):
            res = fut.result()
            results.append(res)
            rc |= res["returncode"]
            print(f"  {res['duration']:0.2f}s {"⛔" if res['returncode'] else "✅"} {res['name']}")


    # Print failures first (with captured output), then a brief timing summary.
    print()
    failures = [r for r in results if r["returncode"]]
    for r in failures:
        if r["stdout"]:
            print(r["stdout"].rstrip())
        if r["stderr"]:
            print(r["stderr"].rstrip())
        print(f"^---- failure(s) from {r['name']}\n")
        print()

    total_duration = perf_counter() - start
    print(f"{len(linters)} linters in {total_duration:0.2f}s")
    raise SystemExit(rc)


if __name__ == "__main__":
    main()
