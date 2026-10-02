#!/usr/bin/env python3
"""Check notes/index-design.md, the evidence log, for the things that went wrong in it.

1. The contents list between `<!-- contents -->` and `<!-- /contents -->` is generated from the
   `##` sections and `###` entries. A hand-kept index drifted from the entries it listed and once
   kept a merge-conflict line for days. `--fix` rewrites the list.
2. No merge-conflict markers anywhere.
3. Every entry (`###` heading) starts with a status line: `*<date> · ... · <status> · ...*`. The
   status field starts with one of STATUSES (`kept (merged block)` is fine). The tooling section
   is reference material and is exempt.
4. Every `](#anchor)` link names a heading that exists.
5. Every phrase the rest of the repository quotes from the notes (`notes/index-design.md, "..."`,
   or `under "..."`) still occurs in them. Code comments point into the notes by phrase, so a
   reworded entry must not orphan them.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NOTES = ROOT / "notes" / "index-design.md"
STATUSES = ("kept", "rejected", "retracted", "superseded", "open", "method", "info")
EXEMPT_SECTIONS = ("Tooling",)
BEGIN, END = "<!-- contents -->", "<!-- /contents -->"


def slug(heading: str, seen: dict) -> str:
    """GitHub's anchor for a heading: lowercase, punctuation dropped, spaces to hyphens."""
    s = heading.strip().lower()
    s = re.sub(r"[^\w\- ]", "", s)
    s = s.replace(" ", "-")
    n = seen.get(s, 0)
    seen[s] = n + 1
    return s if n == 0 else f"{s}-{n}"


def parse(lines):
    """Yield (level, title, status line or None) for every ## and ### heading outside the contents."""
    out = []
    in_contents = False
    in_code = False
    before_contents = BEGIN in "\n".join(lines)
    for i, line in enumerate(lines):
        if line.startswith("```"):
            in_code = not in_code
        if in_code:
            continue
        if line.strip() == BEGIN:
            in_contents = True
        elif line.strip() == END:
            in_contents = False
            before_contents = False
            continue
        if in_contents or before_contents:
            continue
        m = re.match(r"^(##|###) (.+)$", line)
        if not m:
            continue
        status = None
        for nxt in lines[i + 1 : i + 4]:
            if nxt.strip():
                status = nxt.strip() if nxt.strip().startswith("*") else None
                break
        out.append((len(m.group(1)), m.group(2), status))
    return out


def first_word(field: str) -> str:
    """The status field may qualify its word: `kept (merged block)`, `rejected as a default`."""
    words = field.strip(" *").split()
    return words[0].strip(",;:") if words else ""


def contents(headings):
    seen = {}
    rows = []
    for level, title, status in headings:
        anchor = slug(title, seen)
        if level == 2:
            rows.append(f"- [{title}](#{anchor})")
        else:
            meta = ""
            if status:
                parts = [p.strip(" *") for p in status.split("·")]
                date = parts[0] if re.match(r"\d{4}-\d{2}-\d{2}", parts[0]) else ""
                st = next((first_word(p) for p in parts if first_word(p) in STATUSES), "")
                meta = " · ".join(p for p in (date, st) if p)
            rows.append(f"  - [{title}](#{anchor})" + (f" · {meta}" if meta else ""))
    return rows


def quoted_phrases():
    files = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.split()
    found = []
    for f in files:
        if f.startswith(("notes/", "subprojects/", "handoff/")):
            continue
        path = ROOT / f
        try:
            text = path.read_text(errors="ignore")
        except (OSError, UnicodeDecodeError):
            continue
        if "index-design" not in text:
            continue
        flat = re.sub(r"\n\s*(?://|#)?\s*", " ", text)
        for m in re.finditer(r'index-design\.md`?\)?,? (?:under )?"([^"]{4,120})"', flat):
            found.append((f, m.group(1)))
        for m in re.finditer(r'"([^"]{4,120})" in `?notes/index-design\.md', flat):
            found.append((f, m.group(1)))
    return found


def main():
    fix = "--fix" in sys.argv
    text = NOTES.read_text()
    lines = text.split("\n")
    errors = []

    for n, line in enumerate(lines, 1):
        if re.match(r"^(<<<<<<<|=======$|>>>>>>>|\|\|\|\|\|\|\|)", line):
            errors.append(f"line {n}: merge-conflict marker")

    headings = parse(lines)
    section = ""
    for level, title, status in headings:
        if level == 2:
            section = title
            continue
        if section.startswith(EXEMPT_SECTIONS):
            continue
        if not status:
            errors.append(f'entry without a status line: "{title}"')
        elif not any(first_word(p) in STATUSES for p in status.split("·")):
            errors.append(f'status line names none of {STATUSES}: "{title}": {status}')

    if BEGIN not in text or END not in text:
        errors.append(f"no {BEGIN} ... {END} block")
    else:
        head, rest = text.split(BEGIN, 1)
        _, tail = rest.split(END, 1)
        fresh = BEGIN + "\n\n" + "\n".join(contents(headings)) + "\n\n" + END
        if BEGIN + rest.split(END, 1)[0] + END != fresh:
            if fix:
                NOTES.write_text(head + fresh + tail)
                print("notes: contents list rewritten")
            else:
                errors.append("contents list is stale: run scripts/lint/lint-notes.py --fix")

    seen = {}
    anchors = set()
    for line in lines:
        m = re.match(r"^#{1,6} (.+)$", line)
        if m:
            anchors.add(slug(m.group(1), seen))
    for n, line in enumerate(lines, 1):
        for target in re.findall(r"\]\(#([^)\s]+)\)", line):
            if target not in anchors:
                errors.append(f"line {n}: link to #{target}, which is no heading")

    flat_notes = re.sub(r"\s+", " ", text.replace("`", "")).lower()
    for f, phrase in quoted_phrases():
        if re.sub(r"\s+", " ", phrase.replace("`", "")).lower().strip(" .") not in flat_notes:
            errors.append(f'{f} quotes "{phrase}", which notes/index-design.md no longer contains')

    for e in errors:
        print(f"notes: {e}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
