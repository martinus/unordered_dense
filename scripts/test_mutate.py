#!/usr/bin/env python3
"""Regression tests for scripts/mutate/mutate_core.py and the adapter beside it.

    scripts/test_mutate.py            # run them
    scripts/test_mutate.py -v         # ... naming each one

Every test here is hermetic: no compiler, no meson, no lanes, and nothing written outside a
temporary directory. What that leaves out is the half of the tool that is a build -- whether meson
configures, whether ninja rebuilds what the mutant touched, whether a hang is really a hang. What
it covers is the half that decides what a verdict *means*, and every way that half has been wrong
so far has been silent: a bug block that substitutes nothing, a filter that matches no test, a
sync that leaves yesterday's binary in place. None of those fail loudly. They come back "survived"
and read as a hole in the tests.

So the rule these encode is the one from the tool's own docstring -- when it is wrong, it must not
be wrong in the direction that flatters or defames the suite.

`mutate_core.py` is vendored: nanobench and oans each hold a byte-identical copy and drive it
through an adapter of their own. That is the reason the cmake and make backends are tested here,
in a repository that builds with meson and will never run either -- a backend covered only where
it is used is exactly as untested as it was before the tools were merged, and re-vendoring a core
whose other half is broken is how one repository's green suite would ship the others a fault. The
same goes for the minunit harness, which only oans runs: a harness that cannot see a failure
scores it `survived`, and that is the one direction this tool must never be wrong in.
"""

from __future__ import annotations

import argparse
import importlib.util
import inspect
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import types
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "mutate" / "mutate.py"
CORE = SCRIPT.parent / "mutate_core.py"


def load_script(path):
    """A fresh module object, so a test that patches a global cannot leak into the next one."""
    spec = importlib.util.spec_from_file_location("mutate_under_test_%s" % path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# The adapter is loaded first and the core is taken from *it*, rather than loaded again from the
# same path. Two module objects off one file share no class identity -- `isinstance(PROJECT,
# separately_loaded.Project)` is False -- so the second copy would make every test here pass by
# duck typing while the first `isinstance` check added to the core failed for a reason that has
# nothing to do with the code. This is also how a real run sees the two halves.
adapter = load_script(SCRIPT)
mutate = adapter.mutate_core
PROJECT = adapter.UnorderedDense()


def sites(src, line_filter=None):
    """Every mutation the sweep would try on `src`, as 'old -> new' strings."""
    return [s["description"]
            for s in mutate.mutation_sites(src, mutate.code_mask(src), line_filter)]


def finished(returncode, stdout="", stderr=""):
    """A stand-in for a finished subprocess, which is all parse_output reads."""
    return types.SimpleNamespace(returncode=returncode, stdout=stdout, stderr=stderr)


class TestCodeMask(unittest.TestCase):
    """What the sweep is allowed to touch. Everything here is budget: a mutant in a comment
    costs a full rebuild of ~90 translation units to prove that a comment does not matter."""

    def masked(self, src):
        mask = mutate.code_mask(src)
        return "".join(c if mask[i] else " " for i, c in enumerate(src))

    def test_line_comment_is_not_code(self):
        self.assertEqual(self.masked("a + b // c + d"), "a + b         ")

    def test_block_comment_is_not_code(self):
        self.assertEqual(self.masked("a /* + */ b"), "a         b")

    def test_string_literal_is_not_code(self):
        self.assertEqual(self.masked('f("a+b") + c'), 'f(     ) + c')

    def test_char_literal_is_not_code(self):
        self.assertEqual(self.masked("c == '+'"), "c ==    ")

    def test_escaped_quote_does_not_end_the_string(self):
        self.assertEqual(self.masked(r'"a\"+b" + c'), '        + c')

    def test_unterminated_string_stops_at_the_newline(self):
        # Not valid C++, but a mutant is allowed to produce it, and running away
        # to the end of the header would blank everything after it.
        self.assertEqual(self.masked('"oops\nx + y'), '     \nx + y')

    def test_raw_string_is_not_code(self):
        self.assertEqual(self.masked('R"x(a + b)x" + c'), '             + c')

    def test_preprocessor_line_is_not_code(self):
        # The version macros live on these lines and a lint job checks them.
        self.assertEqual(self.masked("#define X 1\ny + 1"), "           \ny + 1")

    def test_preprocessor_continuation_is_not_code(self):
        # The whole logical line, its embedded newline included -- a `#define`
        # is one directive however many source lines it is spread over.
        self.assertEqual(self.masked("#define X \\\n  1 + 2\ny"), "                   \ny")

    def test_a_hash_that_is_not_at_the_line_start_is_code(self):
        self.assertEqual(self.masked("a + b # c"), "a + b # c")


class TestMutationSites(unittest.TestCase):
    def test_comparison_operators(self):
        self.assertEqual(sites("a <= b"), ["<= -> <", "<= -> >=", "<= -> =="])

    def test_longest_operator_wins(self):
        # `<=` split into `<` would offer `< -> <=`, which substitutes nothing.
        self.assertNotIn("< -> <=", sites("a <= b"))

    def test_shifts_are_left_alone(self):
        self.assertEqual(sites("a << b"), [])
        self.assertEqual(sites("a >>= b"), [])

    def test_template_brackets_are_left_alone(self):
        # The header is clang-formatted: a comparison has spaces, a template
        # argument list does not. Half the `<` in this file are the latter.
        self.assertEqual(sites("static_cast<size_t>(x)"), [])
        self.assertEqual(sites("std::conditional<a, b, c>::type"), [])

    def test_arrow_and_scope_are_left_alone(self):
        self.assertEqual(sites("a->b"), [])
        self.assertEqual(sites("a::b"), [])

    def test_integers_move_by_one_in_both_directions(self):
        self.assertEqual(sites("x = 7"), ["7 -> 8", "7 -> 6"])

    def test_a_suffix_is_kept(self):
        self.assertEqual(sites("x = 8U"), ["8U -> 9U", "8U -> 7U"])

    def test_zero_does_not_go_negative(self):
        self.assertEqual(sites("x = 0"), ["0 -> 1"])

    def test_a_float_is_not_picked_apart(self):
        self.assertEqual(sites("x = 0.8"), [])
        self.assertEqual(sites("x = 1e9"), [])

    def test_a_number_with_no_neighbour_is_still_a_number(self):
        # The float guard asks what is on either side of the digits, and at the
        # start or the end of the file there is nothing there. `"" in ".0123"`
        # is True, so an empty neighbour reads as "part of a float" and drops
        # the mutation -- silently, which is the way this tool must not be wrong.
        self.assertEqual(sites("7"), ["7 -> 8", "7 -> 6"])
        self.assertEqual(sites("x + 7"), ["+ -> -", "7 -> 8", "7 -> 6"])

    def test_booleans_flip(self):
        self.assertEqual(sites("x = true"), ["true -> false"])

    def test_an_identifier_holding_a_keyword_is_not_a_keyword(self):
        self.assertEqual(sites("truexyz"), [])

    def test_line_filter_keeps_only_the_lines_asked_for(self):
        src = "a + b\nc + d\ne + f\n"
        self.assertEqual(sites(src, line_filter={2}), ["+ -> -"])

    def test_a_site_carries_the_line_it_is_on(self):
        found = mutate.mutation_sites("a\nb\nc + d", mutate.code_mask("a\nb\nc + d"))
        self.assertEqual([s["line"] for s in found], [3])


class TestLineFilter(unittest.TestCase):
    """What --lines accepts. The list form is what lets a second pass -- under a sanitizer, say --
    ask about exactly the survivors of the first, which land scattered rather than in a range."""

    def test_a_single_line(self):
        self.assertEqual(mutate.parse_lines("1290"), {1290})

    def test_a_range_includes_both_ends(self):
        self.assertEqual(mutate.parse_lines("10-13"), {10, 11, 12, 13})

    def test_lines_and_ranges_mix(self):
        self.assertEqual(mutate.parse_lines("12, 40-42 ,900"), {12, 40, 41, 42, 900})

    def test_overlapping_parts_are_not_counted_twice(self):
        # Or the same mutant is built and run once per part that names its line.
        self.assertEqual(mutate.parse_lines("10-12,11-13"), {10, 11, 12, 13})

    def test_nonsense_is_refused_with_the_part_that_was_wrong(self):
        # A silently dropped part would sweep less than was asked for and report
        # a clean result for lines nobody looked at.
        for text in ("ten", "1-", "-", "1--2", "1-2-3"):
            with self.assertRaises(Exception):
                mutate.parse_lines(text)


class TestTemplateDefaults(unittest.TestCase):
    """`std::enable_if_t<..., bool> = true>` -- the SFINAE idiom. The parameter exists so the
    substitution has somewhere to fail and nothing ever reads its value, so flipping it is a mutant
    nothing can kill. There are 47 in this header and each costs a full rebuild to prove nothing."""

    def flips(self, src):
        """Whether the sweep would try to flip the true/false in `src`."""
        word = "true" if "true" in src else "false"
        return not mutate.is_template_default(src, src.index(word), word)

    def test_the_sfinae_idiom_is_left_alone(self):
        self.assertFalse(self.flips("std::enable_if_t<is_map_v<Q>, bool> = true>"))
        self.assertFalse(self.flips("template <bool> = false>"))

    def test_an_ordinary_assignment_is_still_mutated(self):
        self.assertTrue(self.flips("bool key_found = false;"))
        self.assertTrue(self.flips("return true;"))
        self.assertTrue(self.flips("using iterator = iter_t<false>;"))

    def test_a_comparison_is_not_read_as_a_default(self):
        # The `=` that ends `==`, `<=` or `>=` is not an assignment, and the
        # value being compared against is very much live.
        self.assertTrue(self.flips("if (a == true>b) {"))
        self.assertTrue(self.flips("if (a <= true>b) {"))

    def test_the_bracket_that_closes_the_parameter_is_not_a_comparison(self):
        # `bool> = true>` has a `>` before the `=`, one space away. Reading that
        # as the tail of `>=` rejects every real instance of the idiom.
        self.assertFalse(self.flips("typename T, bool> = true>"))


class TestOperators(unittest.TestCase):
    def test_one_name_parses_to_one_operator(self):
        self.assertEqual(mutate.parse_operators("tokens"), {"tokens"})

    def test_deletions_can_be_asked_for_alone(self):
        # Which is the point of naming them rather than having a flag that adds
        # to the other: a survey of deletions costs less than the token sweep,
        # and mixing the two makes the report harder to read for no gain.
        self.assertEqual(mutate.parse_operators("deletions"), {"deletions"})

    def test_both(self):
        self.assertEqual(mutate.parse_operators("tokens,deletions"), {"tokens", "deletions"})
        self.assertEqual(mutate.parse_operators(" deletions , tokens "), {"tokens", "deletions"})

    def test_bitwise_can_be_asked_for_alone(self):
        # The point of naming the bitwise table separately from `tokens`: sweeping a
        # header of masks for ^ and | should not re-answer all 841 token sites.
        self.assertEqual(mutate.parse_operators("bitwise"), {"bitwise"})

    def test_every_named_operator_is_one_the_runner_acts_on(self):
        # Read off main() rather than restated as a literal. A name parse_operators
        # accepts but nothing dispatches on sweeps nothing and reports a clean run,
        # which is the failure this whole file exists to prevent -- and a test that
        # asserts the tuple against a copy of itself cannot catch it, because adding
        # the fourth name and updating the literal is one edit.
        body = inspect.getsource(mutate.collect_mutants)
        for name in mutate.OPERATORS:
            self.assertIn('"%s" in args.operators' % name, body)

    def test_the_default_is_every_operator_and_is_derived(self):
        # Derived from OPERATORS rather than spelled out, so an operator added later
        # is in the default by construction. A restated list is the same trap as the
        # test above: it can only ever agree with whatever was typed beside it.
        m = re.search(r"--operators.*?default=([^,]+),",
                      inspect.getsource(mutate.build_parser), re.S)
        self.assertIsNotNone(m, "could not find the --operators default in build_parser()")
        self.assertEqual(m.group(1).strip(), "set(OPERATORS)")

    def test_an_unknown_operator_is_refused_by_name(self):
        with self.assertRaises(Exception) as e:
            mutate.parse_operators("tokens,statements")
        self.assertIn("statements", str(e.exception))

    def test_asking_for_nothing_is_refused(self):
        # An empty set would sweep nothing and report a clean run.
        with self.assertRaises(Exception):
            mutate.parse_operators(",")


class TestBitwiseSites(unittest.TestCase):
    """^ and | , which the token table leaves alone. Named separately so a header of masks and
    fingerprints can be swept for them without re-answering every comparison in the file."""

    def bitwise(self, src):
        return [s["description"] for s in mutate.bitwise_sites(src, mutate.code_mask(src))]

    def test_xor_and_or_are_mutated(self):
        self.assertEqual(self.bitwise("a ^ b;\n"), ["^ -> &", "^ -> |"])
        self.assertEqual(self.bitwise("a | b;\n"), ["| -> &", "| -> ^"])

    def test_logical_or_is_consumed_rather_than_split(self):
        # The bug this guards: matching the first `|` of `||` turns `a || b` into
        # `a &| b`, which is a rebuild spent reaching a syntax error.
        self.assertEqual(self.bitwise("a || b;\n"), [])

    def test_compound_assignment_is_mutated_whole(self):
        self.assertEqual(self.bitwise("a |= b;\n"), ["|= -> &=", "|= -> ^="])
        self.assertEqual(self.bitwise("a ^= b;\n"), ["^= -> &=", "^= -> |="])

    def test_bitwise_and_is_left_alone(self):
        # Three operators share the spelling -- bitwise and, address-of, and the
        # reference declarator -- and only a parser can tell them apart.
        self.assertEqual(self.bitwise("a & b;\n"), [])
        self.assertEqual(self.bitwise("auto& x = y;\n"), [])

    def test_words_and_numbers_are_not_touched(self):
        # Otherwise asking for bitwise alone would re-answer the token sweep.
        self.assertEqual(self.bitwise("return true;\n"), [])
        self.assertEqual(self.bitwise("auto x = 8;\n"), [])

    def test_the_two_tables_never_claim_the_same_spelling(self):
        # What makes `tokens,bitwise` produce no duplicate site.
        tokens = {op for op, _ in mutate.OPERATOR_MUTATIONS}
        bitwise = {op for op, reps in mutate.BITWISE_MUTATIONS if reps}
        self.assertEqual(tokens & bitwise, set())


class TestDeletionSites(unittest.TestCase):
    """Whole statements removed -- the operator the hand-written bugs turned out to live in.
    Nearly every one of them is "the code forgot to do this", which is not one token."""

    def deletions(self, src):
        return [s["description"] for s in mutate.deletion_sites(src, mutate.code_mask(src))]

    def test_a_statement_is_a_deletion_site(self):
        self.assertEqual(self.deletions("    m_values.pop_back();\n"),
                         ["delete: m_values.pop_back();"])

    def test_the_whole_statement_goes_including_its_comment(self):
        src = "    foo();  // why\n"
        site = mutate.deletion_sites(src, mutate.code_mask(src))[0]
        self.assertEqual(src[:site["offset"]] + site["replacement"], "    ")

    def test_a_closing_brace_is_not_a_statement(self):
        self.assertEqual(self.deletions("};\n"), [])
        self.assertEqual(self.deletions("    } while (x);\n"), [])

    def test_a_declaration_of_the_scope_is_not_a_statement(self):
        for src in ("using foo = bar;\n", "template <class T> struct x;\n",
                    "static_assert(sizeof(T) == 8);\n", "public:\n"):
            self.assertEqual(self.deletions(src), [], src)

    def test_half_a_statement_is_not_deleted(self):
        # A call spread over two lines would leave the other half behind, which
        # is a syntax error rather than a question. Not parsing C++ means these
        # are simply out of reach.
        self.assertEqual(self.deletions("    foo(a,\n        b);\n"), [])

    def test_a_line_of_a_comment_is_not_a_statement(self):
        self.assertEqual(self.deletions("    // foo();\n"), [])

    def test_the_line_filter_applies(self):
        src = "a();\nb();\nc();\n"
        self.assertEqual(self.deletions(src), ["delete: a();", "delete: b();", "delete: c();"])
        got = mutate.deletion_sites(src, mutate.code_mask(src), line_filter={2})
        self.assertEqual([s["description"] for s in got], ["delete: b();"])


class TestDropUncompiled(unittest.TestCase):
    """Mutants in a branch this build does not compile. `mum()` picks between __uint128_t, an MSVC
    intrinsic and a long-hand multiply, so two thirds of it is never seen -- 35 mutants of pure
    noise in one function, each costing a full rebuild to come back `survived`."""

    class FakeLane:
        def __init__(self, compiled):
            self.compiled = compiled

        def compiled_lines(self, timeout):
            return self.compiled

    def drop(self, compiled, mutants):
        said = []
        kept = mutate.drop_uncompiled(self.FakeLane(compiled), mutants,
                                      types.SimpleNamespace(build_timeout=1), said.append)
        return [m["name"] for m in kept], " ".join(said)

    def test_a_mutant_on_a_line_that_is_not_compiled_is_dropped(self):
        kept, said = self.drop({1, 2}, [dict(name="live", line=1), dict(name="dead", line=99)])
        self.assertEqual(kept, ["live"])
        self.assertIn("99", said)
        self.assertIn("nothing could catch them", said)

    def test_a_named_bug_has_no_line_and_is_always_kept(self):
        # A bug file block can span the whole file; there is no one line to ask
        # about, and refusing to run it would be the tool declining the request.
        kept, _ = self.drop({1}, [dict(name="a bug")])
        self.assertEqual(kept, ["a bug"])

    def test_nothing_is_dropped_when_the_answer_is_unknown(self):
        # No pre-filter TU, so nothing was asked. Filtering on that would drop
        # every mutant there is.
        kept, said = self.drop(None, [dict(name="x", line=1), dict(name="y", line=99)])
        self.assertEqual(kept, ["x", "y"])
        self.assertEqual(said, "")


class TestSiteMutants(unittest.TestCase):
    def texts(self, src):
        """What each mutant's file looks like, spliced the way a lane splices it."""
        mutants = mutate.site_mutants(mutate.mutation_sites(src, mutate.code_mask(src)))
        return [mutate.mutant_text(m, src) for m in mutants]

    def test_the_replacement_lands_where_the_site_says(self):
        self.assertEqual(self.texts("x = a + b;"), ["x = a - b;"])

    def test_a_multi_character_operator_is_replaced_whole(self):
        self.assertEqual(self.texts("if (a <= b) {}"),
                         ["if (a < b) {}", "if (a >= b) {}", "if (a == b) {}"])

    def test_a_mutant_carries_the_edit_and_not_a_copy_of_the_file(self):
        # 130 KB per site here, ~1600 sites for a sweep of all three operators -- built before --limit or
        # the uncompiled-line filter can throw most of them away.
        mutants = mutate.site_mutants(mutate.mutation_sites("x = a + b;", mutate.code_mask("x = a + b;")))
        self.assertNotIn("text", mutants[0])

    def test_a_named_bug_keeps_the_whole_text_it_was_built_from(self):
        # Those are built by substitution over the file and there are dozens, not thousands.
        got = mutate.bug_mutants([dict(name="n", old="a + b", new="a - b")], "x = a + b;")
        self.assertEqual(mutate.mutant_text(got[0], "x = a + b;"), "x = a - b;")

    def test_a_deletion_splices_the_statement_out(self):
        src = "    foo();\n    bar();\n"
        mutants = mutate.site_mutants(mutate.deletion_sites(src, mutate.code_mask(src)))
        self.assertEqual(mutate.mutant_text(mutants[0], src), "    \n    bar();\n")


class TestBugFiles(unittest.TestCase):
    def parse(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bugs.txt"
            path.write_text(text)
            return mutate.parse_bug_file(str(path))

    def test_a_block_becomes_a_bug(self):
        bugs = self.parse("# the name\n<<<\nold line\n===\nnew line\n>>>\n")
        self.assertEqual(bugs, [dict(name="the name", old="old line", new="new line",
                                     additive=False)])

    def test_the_name_is_the_first_comment_line_of_a_run(self):
        # The lines after it are room to explain the bug without any of it
        # reaching the report.
        bugs = self.parse("# the name\n# why it matters\n<<<\na\n===\nb\n>>>\n")
        self.assertEqual(bugs[0]["name"], "the name")

    def test_a_blank_line_starts_a_new_run_of_comments(self):
        # Which is what lets a file open with a header of its own.
        bugs = self.parse("# file header\n\n# the name\n<<<\na\n===\nb\n>>>\n")
        self.assertEqual(bugs[0]["name"], "the name")

    def test_multi_line_bodies_need_no_escaping(self):
        bugs = self.parse("# n\n<<<\nline 1\nline 2\n===\nline 3\n>>>\n")
        self.assertEqual(bugs[0]["old"], "line 1\nline 2")

    def test_an_empty_side_is_a_deletion(self):
        bugs = self.parse("# n\n<<<\ngone\n===\n>>>\n")
        self.assertEqual(bugs[0]["new"], "")

    def test_an_unnamed_block_still_gets_a_name(self):
        self.assertEqual(self.parse("<<<\na\n===\nb\n>>>\n")[0]["name"], "bug 1")

    def test_an_unterminated_block_is_an_error(self):
        with self.assertRaises(RuntimeError):
            self.parse("# n\n<<<\na\n===\nb\n")

    def test_stray_text_outside_a_block_is_an_error(self):
        with self.assertRaises(RuntimeError):
            self.parse("this is not a comment\n<<<\na\n===\nb\n>>>\n")


class TestBugMutants(unittest.TestCase):
    """The guard that keeps this tool from blaming the tests for its own typo.

    A block whose `old` text does not appear substitutes nothing; the suite then stays green and
    the report says the bug survived. Every one of these is that failure mode."""

    def test_a_bug_that_applies_produces_the_mutated_source(self):
        got = mutate.bug_mutants([dict(name="n", old="a + b", new="a - b")], "x = a + b;")
        self.assertEqual(got[0]["text"], "x = a - b;")

    def test_a_bug_that_does_not_apply_is_refused(self):
        with self.assertRaises(RuntimeError) as e:
            mutate.bug_mutants([dict(name="n", old="nowhere", new="x")], "x = a + b;")
        self.assertIn("matches 0 times", str(e.exception))

    def test_an_ambiguous_bug_is_refused(self):
        with self.assertRaises(RuntimeError) as e:
            mutate.bug_mutants([dict(name="n", old="a", new="b")], "a + a")
        self.assertIn("matches 2 times", str(e.exception))

    def test_a_no_op_bug_is_refused(self):
        with self.assertRaises(RuntimeError) as e:
            mutate.bug_mutants([dict(name="n", old="a", new="a")], "a")
        self.assertIn("no-op", str(e.exception))

    def test_every_problem_is_reported_at_once(self):
        with self.assertRaises(RuntimeError) as e:
            mutate.bug_mutants([dict(name="first", old="nowhere", new="x"),
                                dict(name="second", old="elsewhere", new="y")], "src")
        self.assertIn("first", str(e.exception))
        self.assertIn("second", str(e.exception))


class TestAdditiveBugs(unittest.TestCase):
    """A replacement that still contains what it replaced.

    Almost always a mistake -- the way a *move* gets written wrongly, with the whole span in `old`
    including the line that stays -- and it is the mistake that cannot be seen in the output: the
    block applies, the suite runs, and `caught` or `SURVIVED` is reported about a file that never
    changed. woswoar shipped three of these in one session before it started refusing them.

    The exception is real, so there is a way to say it, and saying it wrong has to be an error
    rather than a shrug: a misspelled flag that quietly meant "not additive" would put the trap
    back with a marker beside it claiming otherwise."""

    def parse(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bugs.txt"
            path.write_text(text)
            return mutate.parse_bug_file(str(path))

    def test_an_ordinary_fence_is_not_additive(self):
        self.assertFalse(self.parse("<<<\na\n===\nb\n>>>\n")[0]["additive"])

    def test_the_fence_carries_the_flag(self):
        self.assertTrue(self.parse("<<< additive\na\n===\nx\na\n>>>\n")[0]["additive"])

    def test_an_unknown_flag_is_refused_by_name(self):
        with self.assertRaises(RuntimeError) as e:
            self.parse("<<< addative\na\n===\nb\n>>>\n")
        self.assertIn("addative", str(e.exception))
        self.assertIn("additive", str(e.exception))

    def test_the_flag_does_not_leak_into_the_next_block(self):
        # Two blocks in one file, and only the first says it. State that
        # survives a block would let one marker excuse every later mistake.
        bugs = self.parse("<<< additive\na\n===\nxa\n>>>\n\n<<<\nb\n===\nc\n>>>\n")
        self.assertEqual([b["additive"] for b in bugs], [True, False])

    def test_a_replacement_containing_its_original_is_refused(self):
        with self.assertRaises(RuntimeError) as e:
            mutate.bug_mutants([dict(name="a move written wrongly", old="keep();",
                                     new="added();\nkeep();")], "keep();\n")
        self.assertIn("does not change", str(e.exception))
        self.assertIn("additive", str(e.exception))  # names the way out

    def test_an_additive_block_is_allowed_through(self):
        # nanobench has one: a `-` sign accepted in front of a digit check that
        # stays, which is how "-1" becomes 18446744073709551615.
        got = mutate.bug_mutants([dict(name="sign accepted", old="if (digit) {",
                                       new="if (sign) { }\nif (digit) {",
                                       additive=True)], "if (digit) {\n")
        self.assertEqual(got[0]["text"], "if (sign) { }\nif (digit) {\n")

    def test_a_no_op_is_still_refused_even_when_additive(self):
        # `additive` says the new text may *contain* the old, not that a block
        # which changes nothing at all is now acceptable.
        with self.assertRaises(RuntimeError) as e:
            mutate.bug_mutants([dict(name="says nothing", old="x;", new="x;",
                                     additive=True)], "x;\n")
        self.assertIn("no-op", str(e.exception))


class TestProjectSeam(unittest.TestCase):
    """What a project has to answer, and what it gets for free.

    The half of the vendored core that only shows up when there are two projects: a default that
    is wrong for the other one is invisible here until nanobench runs it."""

    def lane(self):
        return types.SimpleNamespace(dir="/lane", build="/lane/builddir")

    def test_the_default_target_gets_the_projects_prefilter_tu(self):
        self.assertEqual(PROJECT.default_syntax_tu(PROJECT.target), PROJECT.syntax_tu)

    def test_mutating_a_tu_checks_that_tu(self):
        self.assertEqual(PROJECT.default_syntax_tu("test/unit/counter.cpp"),
                         "test/unit/counter.cpp")

    def test_mutating_anything_else_turns_the_prefilter_off(self):
        # Against a file the pre-filter TU does not include, the check compiles
        # something the mutation cannot reach, passes every time, and filters
        # nothing -- which is worse than not having one, because it looks like a
        # working filter.
        self.assertIsNone(PROJECT.default_syntax_tu("meson.build"))

    def test_the_suite_runs_in_the_lane_by_default(self):
        self.assertEqual(PROJECT.test_cwd(self.lane()), "/lane")

    def test_the_suite_runs_where_the_project_says(self):
        # Through Lane.run_tests rather than by calling the hook and asserting
        # what it returned, which would only be a test that python dispatches
        # overrides. nanobench needs this one: `nb` writes its example artifacts
        # into the working directory.
        class FromTheBuildDir(adapter.UnorderedDense):
            def test_cwd(self, lane):
                return lane.build

        lane = mutate.Lane.__new__(mutate.Lane)
        lane.project, lane.dir, lane.build = FromTheBuildDir(), "/lane", "/lane/builddir"
        lane.memory, lane.env, lane.test_args = 0, {}, []
        seen = []
        lane._run = lambda cmd, timeout, cwd=None, env=None, memory=None: seen.append(cwd)
        lane.run_tests(timeout=1)
        self.assertEqual(seen, ["/lane/builddir"])

    def test_this_projects_environment_points_the_corpus_at_the_lane(self):
        # The one input the suite reads from outside the binary. A lane silently
        # replaying nothing would show up as a wall of survivors in exactly the
        # code the fuzzers cover best.
        self.assertEqual(PROJECT.lane_env(self.lane())["FUZZ_CORPUS_BASE_DIR"],
                         os.path.join("/lane", "data", "fuzz"))

    def test_a_project_that_says_nothing_gets_an_empty_environment(self):
        self.assertEqual(mutate.Project().lane_env(self.lane()), {})


class TestSanitizerOptions(unittest.TestCase):
    """UBSAN_OPTIONS, which is the core's to own.

    `halt_on_error=1` is what makes UBSan fail a run rather than print and exit 0. Without it a
    mutant only UBSan can see is reported as a survivor -- the one direction this tool must never
    be wrong in -- so a project adding a suppressions file must not be able to drop it."""

    def env_for(self, project):
        lane = mutate.Lane.__new__(mutate.Lane)
        lane.dir, lane.build = "/lane", "/lane/builddir"
        env = dict(os.environ)
        env.pop("UBSAN_OPTIONS", None)
        for key, value in project.lane_env(lane).items():
            env.setdefault(key, value)
        ubsan = ["print_stacktrace=1", "halt_on_error=1"]
        suppressions = project.ubsan_suppressions(lane)
        if suppressions:
            ubsan.append("suppressions=" + suppressions)
        env.setdefault("UBSAN_OPTIONS", ":".join(ubsan))
        return env

    def test_the_core_options_are_there_without_a_project_saying_anything(self):
        got = self.env_for(mutate.Project())["UBSAN_OPTIONS"]
        self.assertIn("halt_on_error=1", got)
        self.assertIn("print_stacktrace=1", got)

    def test_a_projects_suppressions_are_added_rather_than_substituted(self):
        class WithSuppressions(mutate.Project):
            def ubsan_suppressions(self, lane):
                return os.path.join(lane.dir, "ubsan.supp")

        got = self.env_for(WithSuppressions())["UBSAN_OPTIONS"]
        self.assertIn("suppressions=/lane/ubsan.supp", got)
        # The half that matters: a project that had been handed the whole
        # variable would have dropped these two without anything noticing.
        self.assertIn("halt_on_error=1", got)
        self.assertIn("print_stacktrace=1", got)

    def test_a_project_cannot_reach_the_variable_through_lane_env(self):
        # Not a style rule: lane_env is applied with setdefault *before* the
        # core's own value, so a project setting it here would win silently.
        # Both adapters go through ubsan_suppressions instead.
        class Wrong(mutate.Project):
            def lane_env(self, lane):
                return {"UBSAN_OPTIONS": "print_stacktrace=1"}

        self.assertNotIn("halt_on_error", self.env_for(Wrong())["UBSAN_OPTIONS"])


class TestProjectProblems(unittest.TestCase):
    """The adapter checked against the core, which every run does before copying a lane.

    Each of these is silent at runtime: a `syntax_tu` naming a file that has moved makes the
    pre-filter fail for every mutant, and the whole run comes back `compiler` -- a verdict in the
    flattering direction."""

    def test_this_repositorys_adapter_is_well_formed(self):
        self.assertEqual(PROJECT.problems(), [])

    def test_a_bare_project_is_missing_everything_it_needs(self):
        problems = " ".join(mutate.Project().problems(repo=str(SCRIPT.parent)))
        # `slug` is not in here: it only names temporary directories, so the base
        # class can and does supply one.
        for attribute in ("repo", "target", "test_binary", "backend"):
            self.assertIn(attribute, problems)

    def test_a_project_with_no_backend_is_refused_by_name(self):
        class NoBackend(adapter.UnorderedDense):
            backend = None

        self.assertTrue(any("backend" in p for p in NoBackend().problems()))

    def test_a_target_that_has_moved_is_named(self):
        class Moved(adapter.UnorderedDense):
            target = "include/ankerl/moved.h"

        problems = Moved().problems()
        self.assertTrue(any("moved.h" in p for p in problems), problems)

    def test_a_prefilter_tu_that_has_moved_is_named(self):
        class Moved(adapter.UnorderedDense):
            syntax_tu = "test/unit/moved.cpp"

        problems = Moved().problems()
        self.assertTrue(any("moved.cpp" in p for p in problems), problems)

    def test_a_project_with_no_prefilter_tu_at_all_is_fine(self):
        # None means "do not pre-filter", which is a choice rather than a fault.
        class NoPrefilter(adapter.UnorderedDense):
            syntax_tu = None

        self.assertEqual(NoPrefilter().problems(), [])


class TestRootIgnore(unittest.TestCase):
    """Build directories are matched at the repository root only, and as patterns.

    Both halves have cost something. `build*` everywhere eats `scripts/build.sh`, and `docs`
    everywhere eats the `src/docs` nanobench's unit_templates reads its expected output from -- a
    lane without it fails the baseline for a reason that has nothing to do with any mutant."""

    def ignore_for(self, root_ignore):
        class Rooted(mutate.Project):
            repo = "/repo"
        Rooted.root_ignore = root_ignore
        return mutate.lane_ignore_for(Rooted())

    def test_a_pattern_matches_at_the_root(self):
        ignore = self.ignore_for(("build*",))
        self.assertIn("build-san", ignore("/repo", ["build-san", "src"]))

    def test_the_same_pattern_does_not_match_deeper(self):
        ignore = self.ignore_for(("build*",))
        self.assertNotIn("build.sh", ignore("/repo/src/scripts", ["build.sh"]))

    def test_an_exact_name_still_works(self):
        ignore = self.ignore_for(("docs",))
        self.assertIn("docs", ignore("/repo", ["docs", "src"]))
        self.assertNotIn("docs", ignore("/repo/src", ["docs"]))

    def test_the_ordinary_patterns_apply_everywhere(self):
        ignore = self.ignore_for(())
        self.assertIn("__pycache__", ignore("/repo/scripts", ["__pycache__"]))


class TestTestOutput(unittest.TestCase):
    def test_a_green_run_has_no_failures(self):
        self.assertEqual(mutate.DoctestHarness().parse_output(finished(0, "anything at all")), [])

    def test_a_failed_assertion_names_its_test_case(self):
        out = ("TEST CASE:  erase_the_last_element\n"
               "some/file.cpp:12: ERROR: CHECK( a == b ) is NOT correct!\n")
        self.assertEqual(mutate.DoctestHarness().parse_output(finished(1, out)),
                         ["erase_the_last_element"])

    def test_a_banner_without_an_error_is_not_a_failure(self):
        # doctest prints the banner for anything that produces output, a passing
        # MESSAGE included. Counting those would report a kill that never was.
        out = ("TEST CASE:  chatty_but_passing\n"
               "some/file.cpp:12: MESSAGE: hello\n"
               "TEST CASE:  the_real_one\n"
               "some/file.cpp:13: ERROR: CHECK( x ) is NOT correct!\n")
        self.assertEqual(mutate.DoctestHarness().parse_output(finished(1, out)), ["the_real_one"])

    def test_a_case_is_named_once_however_many_assertions_it_failed(self):
        out = ("TEST CASE:  noisy\n"
               "f.cpp:1: ERROR: a\n"
               "f.cpp:2: ERROR: b\n")
        self.assertEqual(mutate.DoctestHarness().parse_output(finished(1, out)), ["noisy"])

    def test_a_sanitizer_abort_names_the_sanitizer(self):
        # Nonzero exit with no failed assertion. Under -Db_sanitize the thing
        # that noticed is not a test at all, and which runtime complained is the
        # whole answer.
        err = "SUMMARY: AddressSanitizer: heap-buffer-overflow /src/x.h:99 in foo\n"
        self.assertEqual(mutate.DoctestHarness().parse_output(finished(1, "", err)),
                         ["AddressSanitizer heap-buffer-overflow /src/x.h:99 in foo"])

    def test_a_ubsan_runtime_error_is_recognised(self):
        err = "/src/x.h:12:7: runtime error: index 8 out of bounds\n"
        self.assertEqual(mutate.DoctestHarness().parse_output(finished(1, "", err)),
                         ["index 8 out of bounds"])

    def test_a_bare_crash_still_counts_as_caught(self):
        got = mutate.DoctestHarness().parse_output(finished(-11))
        self.assertEqual(got, ["exit -11, no assertion failed"])
        self.assertTrue(got, "a crash must not read as an empty failure list")


class TestCountTestCases(unittest.TestCase):
    """The guard against a filter that matches nothing.

    doctest's suites are the ones TEST_SUITE names; meson's are not. `-ts=unit` names a meson
    suite, runs zero cases, exits 0 -- and every mutant under it comes back survived."""

    def test_the_summary_is_read(self):
        out = "[doctest] test cases:     501 |     501 passed | 0 failed | 27 skipped\n"
        self.assertEqual(mutate.DoctestHarness().count_cases(out), 501)

    def test_a_filter_that_matched_nothing_counts_zero(self):
        out = "[doctest] test cases: 0 | 0 passed | 0 failed | 528 skipped\n"
        self.assertEqual(mutate.DoctestHarness().count_cases(out), 0)

    def test_output_without_a_summary_is_unknown_rather_than_zero(self):
        # A crash before the summary must not be read as "ran no tests", which
        # would abort the run instead of reporting the kill.
        self.assertIsNone(mutate.DoctestHarness().count_cases("Segmentation fault\n"))


class TestNameRendering(unittest.TestCase):
    """Most of this suite is TEST_CASE_TEMPLATE, so a name arrives as 300 characters of
    instantiation with the useful 25 at the front."""

    def test_a_plain_name_is_untouched(self):
        self.assertEqual(mutate.DoctestHarness().short_name("erase_and_shift_down"), "erase_and_shift_down")

    def test_an_instantiation_is_collapsed_but_kept_as_a_marker(self):
        self.assertEqual(
            mutate.DoctestHarness().short_name("bucket_micro<ankerl::unordered_dense::map<int, int>>"),
            "bucket_micro<...>")

    def test_nested_instantiations_collapse_to_one_marker(self):
        self.assertEqual(mutate.DoctestHarness().short_name("t<a<b<c>>>"), "t<...>")

    def test_a_long_name_is_truncated(self):
        self.assertEqual(len(mutate.DoctestHarness().short_name("x" * 200)), 52)

    def test_the_first_few_tests_are_shown_and_the_rest_counted(self):
        self.assertEqual(mutate.DoctestHarness().render_caught(["a", "b", "c", "d", "e"]),
                         "a, b, c (+2 more)")

    def test_exactly_the_limit_is_not_followed_by_a_count(self):
        self.assertEqual(mutate.DoctestHarness().render_caught(["a", "b", "c"]), "a, b, c")

    def test_nothing_caught_it_renders_as_nothing(self):
        self.assertEqual(mutate.DoctestHarness().render_caught([]), "")


class TestMesonSetupArgs(unittest.TestCase):
    def setUp(self):
        self.backend = mutate.MesonBackend()

    def args(self, buildtype="debug", configure_arg=(), unity=True):
        return types.SimpleNamespace(buildtype=buildtype, configure_arg=list(configure_arg),
                                     unity=unity)

    def test_debug_info_is_off_by_default(self):
        # -O0 without -g: a third of the compile time and two thirds of each
        # lane's build directory, and nothing here reads a symbol.
        self.assertIn("-Ddebug=false", self.backend.setup_args(self.args()))

    def test_an_explicit_debug_option_wins(self):
        got = self.backend.setup_args(self.args(configure_arg=["-Ddebug=true"]))
        self.assertNotIn("-Ddebug=false", got)
        self.assertIn("-Ddebug=true", got)

    def test_the_buildtype_is_passed_through(self):
        got = self.backend.setup_args(self.args(buildtype="release"))
        self.assertEqual(got[:2], ["--buildtype", "release"])

    def test_the_lanes_build_as_one_unity(self):
        # 2.5x less compiling for the same work, and the usual objection --
        # touching one file recompiles its whole chunk -- cannot apply to a
        # mutant, which recompiles every file anyway.
        self.assertIn("--unity=on", self.backend.setup_args(self.args()))

    def test_an_explicit_unity_setting_wins(self):
        got = self.backend.setup_args(self.args(configure_arg=["--unity=off"]))
        self.assertNotIn("--unity=on", got)

    def test_a_project_that_has_not_proved_its_tree_survives_unity_does_not_get_it(self):
        # Whether a tree *builds* as a unity is not a general fact: an anonymous
        # namespace stops isolating a file once its neighbours share the chunk.
        # Forced on, such a project gets a baseline that will not compile and the
        # message "fix the tree first".
        self.assertNotIn("--unity=on", self.backend.setup_args(self.args(unity=False)))

    def test_extra_arguments_come_last_so_they_can_override(self):
        got = self.backend.setup_args(self.args(configure_arg=["-Db_sanitize=address"]))
        self.assertEqual(got[-1], "-Db_sanitize=address")

    def test_an_existing_build_directory_is_reconfigured_rather_than_refused(self):
        # meson setup on a directory it has already configured is an error
        # unless it is told, and every --reuse run is exactly that.
        got = self.backend.configure_argv(self.args(), "/src", "/src/builddir", True)
        self.assertIn("--reconfigure", got)
        self.assertEqual(got[-2:], ["/src/builddir", "/src"])

    def test_a_fresh_lane_is_not_told_to_reconfigure(self):
        self.assertNotIn("--reconfigure",
                         self.backend.configure_argv(self.args(), "/src", "/src/builddir", False))


class TestCMakeBackend(unittest.TestCase):
    """The other half of the vendored core, which this repository never runs.

    Tested here because that is the point of sharing the file: nanobench drives these lines, and a
    backend covered only where it is used is exactly as untested as it was before."""

    def setUp(self):
        self.backend = mutate.CMakeBackend()

    def args(self, buildtype="release", configure_arg=()):
        return types.SimpleNamespace(buildtype=buildtype, configure_arg=list(configure_arg),
                                     unity=False)

    def test_an_untranslatable_buildtype_is_refused_rather_than_passed_through(self):
        # cmake accepts an unknown CMAKE_BUILD_TYPE *silently* and drops every
        # per-configuration flag with it, so meson's `debugoptimized` would
        # configure a lane with no optimisation at all -- and nanobench's suite
        # asserts on measured time, so that is a wall of verdicts about the
        # wrong binary rather than an error anyone would see.
        with self.assertRaises(RuntimeError) as e:
            self.backend.configure_argv(self.args("plain"), "/s", "/b", False)
        self.assertIn("plain", str(e.exception))

    def test_the_buildtypes_that_do_translate(self):
        self.assertEqual(self.backend.build_type("debugoptimized"), "RelWithDebInfo")
        self.assertEqual(self.backend.build_type("minsize"), "MinSizeRel")

    def test_a_cmake_project_that_has_not_said_does_not_claim_to_be_unsanitized(self):
        # The fingerprint prints "no sanitizer in this build" off this answer,
        # and that is a claim about what the run could observe. meson has one
        # spelling for every project; a cmake project has whatever it called its
        # own switch, so the honest answer here is that this cannot tell.
        self.assertEqual(self.backend.sanitizer(["-DWHATEVER=ON"]), "unknown")

    def test_meson_reads_its_own_spelling(self):
        self.assertEqual(mutate.MesonBackend().sanitizer(["-Db_sanitize=address"]), "address")
        self.assertEqual(mutate.MesonBackend().sanitizer([]), "none")

    def test_the_source_and_build_directories_are_named(self):
        got = self.backend.configure_argv(self.args(), "/src", "/src/build", False)
        self.assertEqual(got[:5], ["cmake", "-S", "/src", "-B", "/src/build"])

    def test_an_existing_build_directory_needs_nothing_extra(self):
        # cmake re-runs in place, which is the difference from meson - and the
        # reason a --reuse lane does not have to be told anything.
        fresh = self.backend.configure_argv(self.args(), "/src", "/src/build", False)
        again = self.backend.configure_argv(self.args(), "/src", "/src/build", True)
        self.assertEqual(fresh, again)

    def test_the_buildtype_reaches_cmakes_spelling_of_it(self):
        # The shared flag is one word in lower case; CMAKE_BUILD_TYPE is
        # capitalised, and `-DCMAKE_BUILD_TYPE=release` silently configures a
        # build with no optimisation flags at all rather than failing.
        self.assertIn("-DCMAKE_BUILD_TYPE=Release",
                      self.backend.configure_argv(self.args(), "/s", "/b", False))
        self.assertIn("-DCMAKE_BUILD_TYPE=Debug",
                      self.backend.configure_argv(self.args("debug"), "/s", "/b", False))

    def test_the_compile_database_is_asked_for(self):
        # Without it there is no compile_commands.json, the pre-filter turns
        # itself off, and every mutant that is not valid C++ costs a full
        # rebuild instead of half a second.
        self.assertIn("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                      self.backend.configure_argv(self.args(), "/s", "/b", False))

    def test_extra_arguments_come_last_so_they_can_override(self):
        got = self.backend.setup_args(self.args(configure_arg=["-DNB_sanitizer=ON"]))
        self.assertEqual(got[-1], "-DNB_sanitizer=ON")

    def test_the_pass_through_flag_is_named_after_the_tool(self):
        # It is in both projects' CLAUDE.md by name, and a renamed flag breaks
        # the documented invocation rather than anything a test would see.
        self.assertEqual(mutate.CMakeBackend().arg_flag, "--cmake-arg")
        self.assertEqual(mutate.MesonBackend().arg_flag, "--meson-arg")


class TestMakeBackend(unittest.TestCase):
    """make: no configure step, and everything else different because of it.

    Not run in this repository, and that is the point of testing it here. The two generators
    remember what they were configured with; make remembers nothing, so a pass-through argument
    that lands in the wrong place is silently dropped -- a lane asked for a sanitizer build and
    given a plain one scores every memory-safety mutant `survived`."""

    def args(self, configure_arg=()):
        return types.SimpleNamespace(buildtype="release", configure_arg=list(configure_arg),
                                     unity=False)

    def test_the_pass_through_flag_is_named_after_the_tool(self):
        self.assertEqual(mutate.MakeBackend().arg_flag, "--make-arg")

    def test_the_target_is_built_and_not_the_default_goal(self):
        # A project whose suite is one binary should not pay for the rest of the
        # tree on every mutant, and `all` is what make builds when asked for
        # nothing.
        got = mutate.MakeBackend("test").build_argv("/lane", 4, self.args())
        self.assertEqual(got, ["make", "-C", "/lane", "-j", "4", "test"])

    def test_no_target_leaves_make_to_its_default_goal(self):
        got = mutate.MakeBackend().build_argv("/lane", 1, self.args())
        self.assertEqual(got, ["make", "-C", "/lane", "-j", "1"])

    def test_pass_through_arguments_ride_on_the_build_line(self):
        # The difference that matters. meson and cmake put `--make-arg`'s
        # equivalent into a configured build directory once; make has no such
        # place, so an argument that only reached a configure step would apply
        # to nothing at all and the fingerprint would still print it.
        got = mutate.MakeBackend("test").build_argv(
            "/lane", 2, self.args(["CC=clang", "SANITIZE=address"]))
        self.assertEqual(got, ["make", "-C", "/lane", "-j", "2",
                               "CC=clang", "SANITIZE=address", "test"])

    def test_it_inherits_nothing_ninja_shaped(self):
        # The base class is deliberately not a ninja backend: a default of
        # `ninja -C` and a build.ninja probe would be inherited silently by the
        # next backend and show up as a slow reconfigure loop, not an error.
        self.assertNotIsInstance(mutate.MakeBackend(), mutate.NinjaBackend)
        self.assertIsInstance(mutate.MesonBackend(), mutate.NinjaBackend)
        self.assertIsInstance(mutate.CMakeBackend(), mutate.NinjaBackend)
        self.assertFalse(hasattr(mutate.MakeBackend(), "configure_argv"))

    def test_the_compiler_comes_off_the_build_line_not_the_environment(self):
        # `--make-arg CC=clang` never reaches the environment, so the base
        # class's answer would name the system compiler while the lanes built
        # with another one -- and the fingerprint is what a survivor is judged
        # against later.
        backend = mutate.MakeBackend("test")
        project = types.SimpleNamespace(compiler_env="CC", compiler_probe="cc")
        self.assertEqual(backend.compiler(project, self.args(["CC=clang"])), "clang")
        self.assertEqual(
            backend.compiler(project, self.args(["CC=gcc", "CC=clang-18"])),
            "clang-18")  # make takes the last assignment

    def test_a_run_that_says_nothing_about_the_compiler_falls_back(self):
        project = types.SimpleNamespace(compiler_env="CC", compiler_probe="cc")
        got = mutate.MakeBackend().compiler(project, self.args(["SANITIZE=address"]))
        self.assertNotEqual(got, "address")

    def test_it_offers_no_buildtype(self):
        # make has no portable spelling for one, and a flag accepted and then
        # dropped is printed in the fingerprint as though it had applied.
        parser = argparse.ArgumentParser()
        mutate.MakeBackend().add_arguments(parser, mutate.Project())
        self.assertEqual(parser.parse_known_args(["--buildtype", "release"])[1],
                         ["--buildtype", "release"])

    def test_a_make_project_does_not_claim_to_be_unsanitized(self):
        # There is no portable spelling, so the honest answer is that this
        # cannot tell -- the fingerprint's "no sanitizer in this build" is a
        # claim about what the run could observe.
        self.assertEqual(mutate.MakeBackend().sanitizer(["SANITIZE=address"]), "unknown")

    def test_configure_writes_a_compilation_database_from_what_make_would_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            lane = types.SimpleNamespace(dir=tmp, build=tmp, env=dict(os.environ))
            printed = "cc -O2 -Isrc -c -o src/x.o src/x.c\n"
            real = mutate.subprocess.run
            mutate.subprocess.run = lambda *a, **k: finished(0, printed)
            try:
                mutate.MakeBackend("test").configure(lane, self.args())
            finally:
                mutate.subprocess.run = real
            with open(os.path.join(tmp, "compile_commands.json")) as f:
                entries = json.load(f)
        self.assertEqual([e["file"] for e in entries], ["src/x.c"])

    def test_a_dry_run_that_fails_stops_the_lane_rather_than_writing_nothing(self):
        # An empty database reads as "no pre-filter", which is a survivable
        # answer -- so a broken makefile would cost every mutant a full rebuild
        # and never say why.
        with tempfile.TemporaryDirectory() as tmp:
            lane = types.SimpleNamespace(dir=tmp, build=tmp, env=dict(os.environ))
            real = mutate.subprocess.run
            mutate.subprocess.run = lambda *a, **k: finished(2, "", "no rule to make target")
            try:
                with self.assertRaises(RuntimeError) as e:
                    mutate.MakeBackend().configure(lane, self.args())
            finally:
                mutate.subprocess.run = real
        self.assertIn("no rule to make target", str(e.exception))


class TestCompileCommandsFromMake(unittest.TestCase):
    """`make --dry-run`, read as a compilation database.

    This is where the syntax pre-filter's flags come from under make, and a wrong reading is
    silent both ways: a line mistaken for a compile makes the pre-filter check the wrong thing,
    and a real compile missed turns the filter off and costs a full rebuild per mutant."""

    def files(self, output, directory="/lane"):
        return [e["file"] for e in mutate.compile_commands_from_make(output, directory)]

    def command(self, output, directory="/lane"):
        return mutate.compile_commands_from_make(output, directory)[0]["command"]

    def test_a_plain_compile_line_is_read(self):
        # `-c` stays, the way a generator's own database keeps it: this is a
        # compile_commands entry, and it is `_syntax_command` that strips the
        # arguments a syntax check cannot have.
        got = mutate.compile_commands_from_make("cc -O2 -c -o src/x.o src/x.c\n", "/lane")
        self.assertEqual(got, [dict(directory="/lane", file="src/x.c",
                                    command="cc -O2 -c src/x.c")])

    def test_a_line_that_is_not_a_compile_is_skipped(self):
        # `make -n` prints everything it would run: the binary itself, an echo,
        # an install. None of them is a compile, and one read as one would have
        # the pre-filter running the test suite.
        output = ("make: Entering directory '/lane'\n"
                  "./test\n"
                  "echo building src/x.c\n"
                  "install -D -m 0755 oans /usr/local/bin/oans\n")
        self.assertEqual(self.files(output), [])

    def test_a_wrapped_compiler_is_still_a_compile_line(self):
        # `CC = ccache gcc` is an ordinary thing for a makefile to say, and it
        # puts the wrapper in argv[0]. Read as "not a compile", the database
        # comes back empty, MakeBackend.configure reads that as "no pre-filter",
        # and every invalid mutant costs a full rebuild with nothing saying why.
        self.assertEqual(self.files("ccache gcc -O2 -c -o x.o src/x.c\n"), ["src/x.c"])
        self.assertEqual(self.files("env distcc cc -c -o x.o src/y.c\n"), ["src/y.c"])
        # ... and the wrapper is not left on the command, so the syntax check
        # does not run through it.
        self.assertEqual(self.command("ccache gcc -O2 -c -o x.o src/x.c\n"),
                         "gcc -O2 -c src/x.c")

    def test_a_line_of_only_wrappers_is_not_a_compile(self):
        self.assertEqual(self.files("ccache --show-stats\n"), [])

    def test_the_argument_classifier_is_reachable_on_its_own(self):
        # Lifted out of the line loop so the table that grows an entry when a
        # compiler does can be tested without a whole dry-run transcript.
        kept, sources = mutate.split_compile_line(
            ["cc", "-O2", "-MF", "dep.d", "-o", "out.c", "-c", "a.c", "b.cpp"])
        self.assertEqual(sources, ["a.c", "b.cpp"])
        self.assertEqual(kept, ["cc", "-O2", "-c", "a.c", "b.cpp"])

    def test_a_compiler_is_recognised_by_name_and_not_by_substring(self):
        self.assertTrue(mutate.looks_like_compiler("cc"))
        self.assertTrue(mutate.looks_like_compiler("/usr/lib/ccache/gcc"))
        self.assertTrue(mutate.looks_like_compiler("clang-18"))
        self.assertTrue(mutate.looks_like_compiler("x86_64-linux-gnu-gcc"))
        self.assertTrue(mutate.looks_like_compiler("g++-14"))
        # These mention a compiler without being one, and a substring test on
        # "cc" alone matches most of them.
        self.assertFalse(mutate.looks_like_compiler("ccache"))
        self.assertFalse(mutate.looks_like_compiler("echo"))
        self.assertFalse(mutate.looks_like_compiler("install"))

    def test_link_only_arguments_are_kept_in_the_database(self):
        # The database records what make would run; it is Lane._syntax_command
        # that drops what a compile producing no object cannot have. Stripping
        # here would make this reader lie about the build *and* leave the
        # generator backends unprotected against the same hazard.
        got = self.command("cc -O2 -c -o x.o x.c -Wl,-z,relro -lm -L/opt/lib\n")
        self.assertEqual(got, "cc -O2 -c x.c -Wl,-z,relro -lm -L/opt/lib")

    def test_an_output_that_looks_like_a_source_is_not_read_as_one(self):
        # `-o` takes the next word whatever it is. A makefile writing
        # `-o generated.c` would otherwise put its own output in the database.
        self.assertEqual(self.files("cc -c -o build/generated.c src/real.c\n"),
                         ["src/real.c"])

    def test_the_first_entry_for_a_file_wins(self):
        # `--always-make` over a tree with several targets compiles the same
        # file more than once, exactly as a generator's own database resolves it.
        output = ("cc -DFIRST -c -o a.o src/a.c\n"
                  "cc -DSECOND -c -o b.o src/a.c\n")
        got = mutate.compile_commands_from_make(output, "/lane")
        self.assertEqual(len(got), 1)
        self.assertIn("-DFIRST", got[0]["command"])

    def test_a_link_and_compile_in_one_line_is_still_a_compile(self):
        # The shape a single-TU test binary is built with, and the one the
        # pre-filter needs most: there is no `-c` anywhere on it.
        got = mutate.compile_commands_from_make(
            "cc -Wall -std=gnu11 src/tests.c -o test -lm\n", "/lane")
        self.assertEqual(got[0]["file"], "src/tests.c")
        self.assertEqual(got[0]["command"], "cc -Wall -std=gnu11 src/tests.c -lm")

    def test_an_unbalanced_quote_is_skipped_rather_than_raising(self):
        # A makefile echoing a shell fragment is not a compile, and one bad line
        # must not take the lane's whole setup with it.
        self.assertEqual(self.files("echo \"unterminated\ncc -c -o x.o x.c\n"), ["x.c"])

    def test_a_flag_carrying_a_source_suffix_is_not_a_source(self):
        self.assertEqual(self.files("cc -Wa,--defsym=x.c -c -o x.o real.c\n"), ["real.c"])


class TestMinunitHarness(unittest.TestCase):
    """The C runner oans uses. Only its failing path names anything, so this is where a
    misread costs a `caught`."""

    def setUp(self):
        self.harness = mutate.MinunitHarness()

    def test_a_green_run_has_no_failures(self):
        out = "......\n\n31 tests, 5910 assertions, 0 failures\n"
        self.assertEqual(self.harness.parse_output(finished(0, out)), [])

    def test_a_failed_check_names_its_test_function(self):
        out = ("...F\n"
               "test_glob_basename failed:\n"
               "\tsrc/tests.c:1061: glob_matches(set, \"a\", false)\n"
               "\n\n31 tests, 12 assertions, 1 failures\n")
        self.assertEqual(self.harness.parse_output(finished(1, out)),
                         ["test_glob_basename"])

    def test_a_test_is_named_once_however_many_ways_it_failed(self):
        out = ("F\ntest_a failed:\n\tx\nF\ntest_a failed:\n\ty\n")
        self.assertEqual(self.harness.parse_output(finished(1, out)), ["test_a"])

    def test_the_word_failed_inside_a_message_does_not_invent_a_failure(self):
        # minunit prints the asserted expression verbatim, so a test *about*
        # failure quotes the word. Anchoring on the whole line is what keeps a
        # `compiler` or an assertion message from being read as a test name.
        out = ("F\n"
               "test_real failed:\n"
               "\tsrc/tests.c:9: restore_from(bad) failed:\n")
        self.assertEqual(self.harness.parse_output(finished(1, out)), ["test_real"])

    def test_a_crash_with_no_failed_assertion_still_counts_as_caught(self):
        got = self.harness.parse_output(finished(-11))
        self.assertEqual(got, ["exit -11, no assertion failed"])
        self.assertTrue(got, "a crash must not read as an empty failure list")

    def test_a_sanitizer_abort_names_the_sanitizer(self):
        err = "SUMMARY: AddressSanitizer: heap-buffer-overflow src/x.c:9 in foo\n"
        self.assertEqual(self.harness.parse_output(finished(1, "", err)),
                         ["AddressSanitizer heap-buffer-overflow src/x.c:9 in foo"])

    def test_the_tally_says_how_many_ran(self):
        self.assertEqual(self.harness.count_cases("\n\n31 tests, 5910 assertions, 0 failures\n"),
                         31)

    def test_a_suite_that_ran_nothing_counts_zero_rather_than_unknown(self):
        # The guard the baseline refuses on. Zero cases is green, and every
        # mutant under it survives.
        self.assertEqual(self.harness.count_cases("\n\n0 tests, 0 assertions, 0 failures\n"), 0)

    def test_output_without_a_tally_is_unknown_rather_than_zero(self):
        # A crash before the report must not be read as "ran no tests", which
        # would abort the run instead of reporting the kill.
        self.assertIsNone(self.harness.count_cases("Segmentation fault\n"))

    def test_a_single_test_is_read_despite_the_singular(self):
        self.assertEqual(self.harness.count_cases("\n\n1 test, 1 assertion, 0 failures\n"), 1)

    def test_it_takes_no_filter_arguments(self):
        # minunit's binary accepts none. Passing one would be accepted by the
        # binary, ignored, and printed in the fingerprint as though it applied.
        parser = argparse.ArgumentParser()
        self.harness.add_arguments(parser)
        self.assertEqual(parser.parse_known_args(["-tc=x"])[1], ["-tc=x"])
        self.assertEqual(self.harness.test_args(types.SimpleNamespace()), [])

    def test_a_crash_is_reported_once_however_the_subclass_parses(self):
        # The contract the template method exists to hold: a nonzero exit that
        # names no failed assertion is still a kill. A subclass writing its own
        # parse_output and forgetting it would return [], scored `survived`.
        for harness in (mutate.MinunitHarness(), mutate.DoctestHarness()):
            self.assertTrue(harness.parse_output(finished(-11)))

    def test_a_name_is_shortened_but_not_treated_as_a_template(self):
        self.assertEqual(self.harness.short_name("test_fiemap_maps_share"),
                         "test_fiemap_maps_share")
        self.assertEqual(len(self.harness.short_name("t" * 200)), 52)


class TestHarnessSeam(unittest.TestCase):
    """Which runner a project gets, and what the parser offers because of it."""

    def make_project(self, harness):
        class WithHarness(adapter.UnorderedDense):
            pass
        WithHarness.harness = harness
        return WithHarness()

    def test_the_fingerprint_names_the_configure_step_a_backend_actually_has(self):
        # make has none -- that is why `configure` is a method rather than an
        # argv -- so a "make setup" row would name a step that does not exist.
        self.assertEqual(mutate.MesonBackend().setup_label, "setup")
        self.assertEqual(mutate.MakeBackend().setup_label, "variables")

    def test_a_project_that_says_nothing_is_refused_rather_than_defaulted(self):
        # Not defaulted to doctest, because a wrong harness does not announce
        # itself: DoctestHarness.count_cases finds no `[doctest] test cases:`
        # line in minunit output and returns None, which the baseline logs as
        # "? cases" and accepts -- so the run would be scored by a parser that
        # cannot read its runner, in the flattering direction.
        self.assertIsNone(mutate.Project.harness)
        self.assertTrue(any("harness" in p for p in mutate.Project().problems()))

    def test_a_doctest_harness_cannot_count_minunit_output(self):
        # The measurement behind the previous test.
        tally = "\n\n31 tests, 5910 assertions, 0 failures\n"
        self.assertIsNone(mutate.DoctestHarness().count_cases(tally))
        self.assertEqual(mutate.MinunitHarness().count_cases(tally), 31)

    def test_a_runner_without_filters_is_not_offered_the_filter_flags(self):
        # Offering them would be worse than useless: the binary takes no
        # arguments, so the filter applies to nothing while the fingerprint
        # prints it as though it had.
        parser = mutate.build_parser(self.make_project(mutate.MinunitHarness()), "")
        self.assertEqual(parser.parse_known_args(["--test-filter", "whatever"])[1],
                         ["--test-filter", "whatever"])

    def test_a_runner_with_filters_still_has_them(self):
        parser = mutate.build_parser(self.make_project(mutate.DoctestHarness()), "")
        self.assertEqual(parser.parse_args(["--test-filter", "x"]).test_filter, "x")

    def test_the_two_seams_add_their_flags_the_same_way(self):
        # One mechanism, not a boolean at one seam and nothing at the other.
        for owner in (mutate.MinunitHarness(), mutate.DoctestHarness()):
            self.assertTrue(hasattr(owner, "add_arguments"))
        parser = argparse.ArgumentParser()
        mutate.NinjaBackend().add_arguments(parser, mutate.Project())
        self.assertEqual(parser.parse_args(["--buildtype", "release"]).buildtype,
                         "release")

    def test_the_fingerprint_names_the_runner(self):
        project = self.make_project(mutate.MinunitHarness())
        args = types.SimpleNamespace(buildtype="debug", configure_arg=[], test_suite=None,
                                     exclude_suite=None, test_filter=None,
                                     exclude_filter=None, unity=project.unity,
                                     file=project.target, lanes=1, jobs=1,
                                     memory_limit=mutate.GIB, memory_note="")
        self.assertIn("minunit", mutate.render_fingerprint(project, mutate.fingerprint(project, args)))

    def test_a_c_project_probes_its_own_compiler_variable(self):
        # CXX on a C project names something that may not even be installed, and
        # the fingerprint is what a survivor is judged against later.
        self.assertEqual(mutate.Project.compiler_env, "CXX")

        class CProject(mutate.Project):
            compiler_env = "CC"
            compiler_probe = "cc"
        self.assertEqual(CProject.compiler_probe, "cc")

    def test_the_file_a_run_mutates_is_the_projects_last_word(self):
        # Default: whatever --file said. A project whose bug files name their
        # own target overrides this, so the two cannot be given inconsistently.
        args = types.SimpleNamespace(file="include/x.h", bugs=None)
        self.assertEqual(mutate.Project().resolve_file(args), "include/x.h")

    def test_a_c_source_is_its_own_prefilter_tu(self):
        self.assertEqual(mutate.Project().default_syntax_tu("src/csum.c"), "src/csum.c")


class TestFingerprint(unittest.TestCase):
    def facts(self, project=PROJECT, **kwargs):
        args = types.SimpleNamespace(buildtype="debug", configure_arg=[], test_suite=None,
                                     exclude_suite=None, test_filter=None,
                                     exclude_filter=None, unity=project.unity,
                                     file=project.target, lanes=4, jobs=2,
                                     memory_limit=2 * mutate.GIB, memory_note="")
        for key, value in kwargs.items():
            setattr(args, key, value)
        return mutate.fingerprint(project, args)

    def rendered(self, project=PROJECT, **kwargs):
        return mutate.render_fingerprint(project, self.facts(project, **kwargs))

    def test_a_plain_build_says_it_has_no_sanitizer(self):
        # The one the report must not stay quiet about: without a sanitizer, a
        # mutant that reads one slot too far comes back survived.
        self.assertEqual(self.facts()["sanitizer"], "none")
        self.assertIn("no sanitizer", self.rendered())

    def test_a_sanitizer_build_says_which(self):
        facts = self.facts(configure_arg=["-Db_sanitize=address,undefined"])
        self.assertEqual(facts["sanitizer"], "address,undefined")
        self.assertNotIn("no sanitizer", mutate.render_fingerprint(PROJECT, facts))

    def test_the_test_filters_are_recorded(self):
        self.assertEqual(self.facts(exclude_suite="fuzz")["tests"], "-tse=fuzz")
        self.assertEqual(self.facts(exclude_filter="flaky_one")["tests"], "-tce=flaky_one")
        self.assertEqual(self.facts()["tests"], "all")

    def test_a_capped_run_says_how_much_and_warns_about_nothing(self):
        self.assertIn("2.0 GiB per lane", self.rendered())
        self.assertNotIn("nothing caps", self.rendered())

    def test_an_uncapped_run_says_so_and_says_why(self):
        # The one the report must not stay quiet about either: without a cap a
        # runaway mutant takes down whatever the kernel picks, and the verdicts
        # of the lanes beside it are then guesses.
        rendered = self.rendered(memory_limit=0, memory_note="Failed to connect to bus")
        self.assertIn("memory          off", rendered)
        self.assertIn("nothing caps", rendered)
        self.assertIn("Failed to connect to bus", rendered)

    def test_a_cap_turned_off_on_purpose_still_warns(self):
        # --memory-limit 0 leaves no reason to print, and the warning is about
        # the consequence rather than the cause.
        self.assertIn("nothing caps", self.rendered(memory_limit=0))

    def test_the_build_system_is_named_in_the_line_that_shows_its_arguments(self):
        # Two projects share this file and configure with different tools, so
        # "setup" alone would leave the reader guessing which one produced the
        # flags underneath it.
        self.assertIn("meson setup", self.rendered())

    def test_a_project_can_add_what_only_it_can_observe(self):
        # nanobench's performance counters are the case: where the kernel
        # refuses them the counter columns come back survived however good the
        # tests are, and only that project knows to ask.
        class WithCounters(adapter.UnorderedDense):
            def extra_facts(self, args):
                return dict(perf_counters=False)

            def extra_fingerprint(self, facts):
                return ["perf counters   no"], ["NOTE: those verdicts mean nothing here."]

        rendered = self.rendered(WithCounters())
        self.assertIn("perf counters   no", rendered)
        self.assertIn("mean nothing here", rendered)


class TestSyncTree(unittest.TestCase):
    """Reuse, which is where being wrong is silent.

    Comparing mtimes would call the previous run's mutated header a change on every file; copying
    the repo's older mtime onto a lane would leave the object files looking newer than their
    source, ninja would build nothing, and the suite would run against the last mutant."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.src = self.tmp / "src"
        self.dst = self.tmp / "dst"
        (self.src / "include").mkdir(parents=True)
        (self.src / "include" / "h.h").write_text("original\n")
        (self.src / "keep.txt").write_text("same\n")
        # A project rooted at the fake tree, because the root-only skips are
        # keyed off where the repository is.
        class Rooted(adapter.UnorderedDense):
            repo = str(self.src)
        self.ignore = mutate.lane_ignore_for(Rooted())
        shutil.copytree(self.src, self.dst)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def sync(self):
        mutate.sync_tree(str(self.src), str(self.dst), self.ignore)

    def test_a_changed_file_is_copied_and_stamped_now(self):
        header = self.src / "include" / "h.h"
        header.write_text("mutated\n")
        os.utime(header, (1, 1))  # an old mtime, as a checkout or a git op leaves
        self.sync()
        copy = self.dst / "include" / "h.h"
        self.assertEqual(copy.read_text(), "mutated\n")
        self.assertGreater(copy.stat().st_mtime, time.time() - 60)

    def test_an_unchanged_file_keeps_its_mtime(self):
        # The point of the whole function: restamping every source would cost a
        # full rebuild in every lane, which is most of what reuse saves.
        keep = self.dst / "keep.txt"
        os.utime(keep, (1000, 1000))
        self.sync()
        self.assertEqual(keep.stat().st_mtime, 1000)

    def test_a_file_with_the_same_size_but_different_bytes_is_copied(self):
        # filecmp with shallow=True would call these equal on a same-size,
        # same-mtime pair, and the lane would test the wrong source.
        (self.src / "keep.txt").write_text("SAME\n")
        os.utime(self.src / "keep.txt", (1000, 1000))
        os.utime(self.dst / "keep.txt", (1000, 1000))
        self.sync()
        self.assertEqual((self.dst / "keep.txt").read_text(), "SAME\n")

    def test_a_deleted_file_is_removed_from_the_lane(self):
        # Or it keeps being compiled, and the lane builds a test file that the
        # working tree no longer has.
        (self.src / "keep.txt").unlink()
        self.sync()
        self.assertFalse((self.dst / "keep.txt").exists())

    def test_a_new_file_arrives(self):
        (self.src / "new.cpp").write_text("int main() {}\n")
        self.sync()
        self.assertTrue((self.dst / "new.cpp").exists())

    def test_the_lane_build_directory_is_not_touched(self):
        # It is the one directory in the lane with no counterpart in the repo,
        # and deleting it would turn every reuse into a cold build.
        build = self.dst / "builddir"
        build.mkdir()
        (build / "build.ninja").write_text("rules\n")
        self.sync()
        self.assertTrue((build / "build.ninja").exists())

    def test_ignored_directories_are_not_copied(self):
        (self.src / ".git").mkdir()
        (self.src / ".git" / "HEAD").write_text("ref\n")
        (self.src / "builddir").mkdir()
        (self.src / "builddir" / "junk").write_text("x\n")
        self.sync()
        self.assertFalse((self.dst / ".git").exists())
        self.assertFalse((self.dst / "builddir" / "junk").exists())

    def test_a_build_named_file_outside_the_root_is_kept(self):
        # `build*` as a global pattern would eat scripts/build.py, which is why
        # the build directories are matched at the repo root only.
        (self.src / "scripts").mkdir()
        (self.src / "scripts" / "build.py").write_text("#!/usr/bin/env python3\n")
        self.sync()
        self.assertTrue((self.dst / "scripts" / "build.py").exists())


class TestSyntaxCommand(unittest.TestCase):
    """The pre-filter's compile line, taken from compile_commands.json rather than reassembled.

    Two things have to come off it: -o, or a syntax check writes an object file; and the -M flags,
    or it overwrites ninja's dependency file for a compile that produced nothing -- after which
    ninja's idea of what depends on the header is a fiction."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.build = self.tmp / "builddir"
        self.build.mkdir()
        (self.tmp / "test" / "unit").mkdir(parents=True)
        (self.tmp / "test" / "unit" / "fuzz_api.cpp").write_text("int main() {}\n")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def command_for(self, command):
        (self.build / "compile_commands.json").write_text(json.dumps([
            dict(directory=str(self.build), command=command,
                 file="../test/unit/fuzz_api.cpp")]))
        lane = mutate.Lane.__new__(mutate.Lane)
        lane.dir = str(self.tmp)
        lane.build = str(self.build)
        lane.syntax_file = os.path.join("test", "unit", "fuzz_api.cpp")
        return lane._syntax_command()

    def test_the_check_is_asked_for(self):
        got = self.command_for("/usr/bin/g++ -std=c++17 -c ../test/unit/fuzz_api.cpp")
        self.assertIn("-fsyntax-only", got["argv"])
        self.assertNotIn("-c", got["argv"])

    def test_no_object_is_written(self):
        got = self.command_for("/usr/bin/g++ -o out.o -c ../test/unit/fuzz_api.cpp")
        self.assertNotIn("-o", got["argv"])
        self.assertNotIn("out.o", got["argv"])

    def test_ninjas_depfile_is_not_clobbered(self):
        got = self.command_for("/usr/bin/g++ -MD -MQ out.o -MF out.o.d "
                               "-c ../test/unit/fuzz_api.cpp")
        for flag in ("-MD", "-MQ", "-MF", "out.o.d"):
            self.assertNotIn(flag, got["argv"])

    def test_the_ccache_prefix_is_dropped(self):
        # ccache refuses -fsyntax-only outright, and there is nothing to cache
        # in a compile that produces no object.
        got = self.command_for("/usr/bin/ccache g++ -std=c++17 -c ../test/unit/fuzz_api.cpp")
        self.assertEqual(got["argv"][0], "g++")

    def test_the_real_flags_are_kept(self):
        # -Werror above all: a mutant that only produces a warning is refused by
        # the real build, so the pre-filter has to refuse it too.
        got = self.command_for("/usr/bin/g++ -Werror -Wall -I../include -std=c++17 "
                               "-c ../test/unit/fuzz_api.cpp")
        for flag in ("-Werror", "-Wall", "-I../include", "-std=c++17"):
            self.assertIn(flag, got["argv"])

    def test_it_runs_where_the_build_does(self):
        # The flags are relative to the build directory; running from anywhere
        # else turns every include path into a miss.
        got = self.command_for("/usr/bin/g++ -c ../test/unit/fuzz_api.cpp")
        self.assertEqual(got["cwd"], str(self.build))

    def test_a_file_that_is_not_in_the_build_has_no_check(self):
        # Rather than silently checking some other TU and passing every time.
        (self.build / "compile_commands.json").write_text(json.dumps([]))
        lane = mutate.Lane.__new__(mutate.Lane)
        lane.dir, lane.build = str(self.tmp), str(self.build)
        lane.syntax_file = os.path.join("test", "unit", "fuzz_api.cpp")
        self.assertIsNone(lane._syntax_command())


class TestMemorySizes(unittest.TestCase):
    """What one lane is allowed, which is the number standing between one runaway mutant and
    every verdict running beside it."""

    def test_the_usual_spellings_of_a_size(self):
        self.assertEqual(mutate.parse_size("4G"), 4 * mutate.GIB)
        self.assertEqual(mutate.parse_size("512M"), 512 * mutate.MIB)
        self.assertEqual(mutate.parse_size("1.5G"), mutate.GIB + mutate.GIB // 2)
        self.assertEqual(mutate.parse_size("2GiB"), 2 * mutate.GIB)
        self.assertEqual(mutate.parse_size("4096"), 4096)

    def test_zero_is_a_size_and_means_off(self):
        # Not falsy-by-accident: it is how the cap is turned off on a machine
        # where it misfires, so it has to parse rather than be refused.
        self.assertEqual(mutate.parse_size("0"), 0)
        self.assertEqual(mutate.human(0), "off")

    def test_nonsense_is_refused_rather_than_read_as_bytes(self):
        # "4GB of RAM" silently becoming 4 bytes would kill every mutant.
        for text in ("lots", "4 gigs", "-1", "G4", ""):
            with self.assertRaises(Exception):
                mutate.parse_size(text)

    def test_the_lanes_together_fit_in_the_machine(self):
        # The rule the default exists for: 32 lanes on a 64 GiB machine must not
        # each be allowed 64 GiB.
        total = 64 * mutate.GIB
        got = mutate.default_memory_limit(lanes=32, jobs=1, total=total)
        self.assertLessEqual(got * 32, total)

    def test_a_lane_is_not_given_more_than_it_could_want(self):
        # A single lane on a huge machine gets what its jobs can use, not the
        # machine -- a cap far above any honest need catches no runaway.
        got = mutate.default_memory_limit(lanes=1, jobs=4, total=1024 * mutate.GIB)
        self.assertEqual(got, 4 * mutate.GIB)

    def test_the_cap_scales_with_the_jobs_in_the_lane(self):
        # Each ninja job is a compiler, and a translation unit peaks at ~185 MB.
        many = mutate.default_memory_limit(lanes=1, jobs=32, total=1024 * mutate.GIB)
        few = mutate.default_memory_limit(lanes=1, jobs=2, total=1024 * mutate.GIB)
        self.assertGreater(many, few)

    def test_a_tiny_machine_still_leaves_a_lane_something_to_build_with(self):
        # Squeezing the share to nothing would fail every build instead of
        # catching anything; below the floor the arithmetic stops.
        self.assertEqual(mutate.default_memory_limit(lanes=64, jobs=1,
                                                     total=2 * mutate.GIB),
                         mutate.GIB)

    def test_a_machine_that_will_not_say_how_much_it_has_still_gets_a_cap(self):
        self.assertEqual(mutate.default_memory_limit(lanes=8, jobs=1, total=0),
                         2 * mutate.GIB)

    def test_a_build_is_never_capped_below_what_its_jobs_need(self):
        # Otherwise an explicit --memory-limit reports `oom` for every mutant --
        # a build stopped by this tool's own arithmetic, read as a growth policy
        # asking for the machine.
        self.assertEqual(mutate.build_memory_limit(mutate.GIB, jobs=32),
                         32 * mutate.MEMORY_PER_JOB)

    def test_a_cap_above_what_the_jobs_need_is_left_alone(self):
        # The cap is about the mutant, and a build that fits under it already
        # has no claim on being given more.
        self.assertEqual(mutate.build_memory_limit(8 * mutate.GIB, jobs=1),
                         8 * mutate.GIB)

    def test_turning_the_cap_off_turns_it_off_for_the_build_too(self):
        self.assertEqual(mutate.build_memory_limit(0, jobs=32), 0)


class TestLaneRoom(unittest.TestCase):
    """A lane is a copy of the tree plus a build directory, and the count of them comes off the
    core count without anyone being asked. Where that lands is usually /tmp, which is usually a
    tmpfs -- so running out of room there is the machine running out of memory."""

    def test_a_workdir_that_cannot_hold_the_lanes_is_refused_before_copying(self):
        # Half a copy leaves meson failing for a reason that never says "disk".
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(RuntimeError) as e:
                mutate.check_room_for(PROJECT, tmp, 10 ** 6, lambda message: None)
            self.assertIn("free", str(e.exception))
            self.assertIn("--lanes", str(e.exception))

    def test_room_for_one_lane_is_not_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            mutate.check_room_for(PROJECT, tmp, 1, lambda message: None)

    def test_reusing_every_lane_asks_for_no_room_at_all(self):
        # And so cannot fail on a workdir already holding exactly what it needs.
        mutate.check_room_for(PROJECT, "/nonexistent", 0, lambda message: None)

    def test_a_tmpfs_workdir_is_named_as_memory(self):
        # The line about it is the difference between "the disk is full" and
        # "the machine is out of RAM", which want different answers.
        real = mutate.memory_backed
        mutate.memory_backed = lambda path: True
        try:
            said = []
            with tempfile.TemporaryDirectory() as tmp:
                mutate.check_room_for(PROJECT, tmp, 1, said.append)
            self.assertIn("memory", said[0])
        finally:
            mutate.memory_backed = real

    @unittest.skipUnless(os.path.isdir("/dev/shm") and os.path.exists("/proc/mounts"),
                         "no /proc/mounts to read a filesystem type from")
    def test_the_deepest_mount_point_decides(self):
        # "/" is a prefix of every path, so a shallow match must not win over
        # the mount the directory is actually on.
        self.assertTrue(mutate.memory_backed("/dev/shm"))
        self.assertFalse(mutate.memory_backed("/proc/mounts"))


class TestMemoryCap(unittest.TestCase):
    """The wrapper, which has to be transparent: the command's stdout, exit status, environment
    and working directory are all read afterwards as if it had been run directly."""

    def test_a_capped_command_is_the_command_inside_a_scope(self):
        got = mutate.memory_capped(["ninja", "-C", "b"], 2 * mutate.GIB)
        self.assertEqual(got[:3], ["systemd-run", "--user", "--scope"])
        self.assertIn("MemoryMax=%d" % (2 * mutate.GIB), got)
        self.assertEqual(got[got.index("--") + 1:], ["ninja", "-C", "b"])

    def test_swap_is_pinned_to_zero(self):
        # Or a runaway thrashes through whatever swap the machine has first,
        # slowing every other lane down for minutes before anything is killed.
        self.assertIn("MemorySwapMax=0", mutate.memory_capped(["x"], mutate.GIB))

    def test_no_cap_means_no_wrapper_at_all(self):
        self.assertEqual(mutate.memory_capped(["ninja"], 0), ["ninja"])

    def test_a_kill_at_the_cap_is_told_apart_from_a_crash(self):
        # -9 is the kernel killing the offender; -15 is systemd stopping the
        # rest of the scope when the offender was not the process we waited on.
        self.assertTrue(mutate.killed_for_memory(finished(-9)))
        self.assertTrue(mutate.killed_for_memory(finished(-15)))
        self.assertFalse(mutate.killed_for_memory(finished(-11)))  # a segfault
        self.assertFalse(mutate.killed_for_memory(finished(1)))
        self.assertFalse(mutate.killed_for_memory(finished(0)))

    def test_a_machine_with_no_systemd_run_says_which_piece_is_missing(self):
        # The reason reaches the fingerprint, so "FileNotFoundError" would be
        # the run telling someone to go and find out what it meant.
        self.assertIn("PATH", self.reason_for(["definitely-not-a-real-binary"]))

    def test_a_systemd_run_that_refuses_is_quoted_rather_than_summarised(self):
        # Its own complaint names the missing piece -- no session bus, an
        # undelegated container -- better than anything invented here.
        reason = self.reason_for(["sh", "-c", "echo 'Failed to connect to bus' >&2; exit 1"])
        self.assertIn("Failed to connect to bus", reason)

    def test_a_cap_that_works_has_no_reason_to_give(self):
        self.assertEqual(self.reason_for(["true"]), "")

    def reason_for(self, argv):
        """`memory_cap_reason` against a stand-in for systemd-run."""
        real = mutate.memory_capped
        mutate.memory_capped = lambda cmd, limit: argv
        try:
            return mutate.memory_cap_reason(mutate.GIB)
        finally:
            mutate.memory_capped = real

    def test_a_lane_raises_the_cap_for_its_build_and_not_for_its_suite(self):
        # The wiring, because the two halves are only useful together: the build
        # gets what its jobs need, the suite gets the lane's cap as it stands,
        # and it is the suite where a runaway mutant shows up.
        lane = mutate.Lane.__new__(mutate.Lane)
        lane.dir, lane.build, lane.jobs = "/lane", "/lane/builddir", 8
        lane.memory, lane.env, lane.test_args = mutate.GIB, {}, []
        lane.project = PROJECT
        # A make lane carries its pass-through arguments on the build line, so
        # the build command is assembled from `args` and not from the lane alone.
        lane.args = types.SimpleNamespace(configure_arg=[])
        seen = []
        lane._run = lambda cmd, timeout, cwd=None, env=None, memory=None: seen.append(memory)
        lane.run_build(timeout=1)
        lane.run_tests(timeout=1)
        self.assertEqual(seen, [8 * mutate.MEMORY_PER_JOB, None])  # None: the lane's own

    def test_a_run_that_timed_out_was_not_killed_for_memory(self):
        # `None` is a hang, and reading it as an oom would report the wrong
        # cause for the one verdict that has a cause.
        self.assertFalse(mutate.killed_for_memory(None))


class TestEvaluate(unittest.TestCase):
    """Which verdict a mutant gets, given how its build and its suite ended.

    Every one of these is a way of being wrong that reads as something else: a build stopped at
    the memory cap looks exactly like a build the compiler refused, and calling it `compiler`
    would credit the tests with protection they are not providing."""

    class FakeLane:
        """A lane that returns canned outcomes instead of building anything."""

        def __init__(self, syntax=None, build=None, tests=None, has_syntax_cmd=True):
            self.syntax_cmd = dict(argv=["cc"], cwd=".") if has_syntax_cmd else None
            self.canned = dict(syntax=syntax, build=build, tests=tests)
            self.written = None
            # `evaluate` reads the verdict out of the project's harness, so a
            # lane without one would answer every mutant `survived`.
            self.project = PROJECT

        def write_target(self, text):
            self.written = text

        def run_syntax_check(self, timeout):
            return self.canned["syntax"]

        def run_build(self, timeout, jobs=None):
            return self.canned["build"]

        def run_tests(self, timeout):
            return self.canned["tests"]

    def verdict(self, **canned):
        args = types.SimpleNamespace(quick_reject=True, build_timeout=1, test_timeout=1)
        lane = self.FakeLane(**canned)
        return mutate.evaluate(lane, dict(text="mutated"), args, "original")

    def test_the_mutant_is_written_before_anything_is_run(self):
        lane = self.FakeLane(syntax=finished(1))
        mutate.evaluate(lane, dict(text="mutated source"),
                        types.SimpleNamespace(quick_reject=True, build_timeout=1,
                                              test_timeout=1), "original")
        self.assertEqual(lane.written, "mutated source")

    def test_nothing_noticed_is_survived(self):
        self.assertEqual(self.verdict(syntax=finished(0), build=finished(0),
                                      tests=finished(0)), ("survived", []))

    def test_a_failed_assertion_is_caught_and_names_the_test(self):
        out = "TEST CASE:  erase_shifts_down\nf.cpp:1: ERROR: CHECK( x ) is NOT correct!\n"
        self.assertEqual(self.verdict(syntax=finished(0), build=finished(0),
                                      tests=finished(1, out)),
                         ("caught", ["erase_shifts_down"]))

    def test_a_mutant_the_prefilter_rejects_never_reaches_the_build(self):
        self.assertEqual(self.verdict(syntax=finished(1), build=None, tests=None),
                         ("compiler", []))

    def test_a_build_that_failed_is_the_compiler(self):
        self.assertEqual(self.verdict(syntax=finished(0), build=finished(1)),
                         ("compiler", []))

    def test_a_suite_that_never_returned_is_a_hang(self):
        self.assertEqual(self.verdict(syntax=finished(0), build=finished(0), tests=None),
                         ("hang", []))

    def test_a_suite_stopped_at_the_cap_is_an_oom(self):
        # The verdict this cap was added for: a mutated growth policy asking for
        # more memory than the machine has.
        self.assertEqual(self.verdict(syntax=finished(0), build=finished(0),
                                      tests=finished(-9)), ("oom", []))

    def test_a_build_stopped_at_the_cap_is_an_oom_and_not_the_compiler(self):
        # A stopped ninja exits nonzero exactly as a refused one does. Reading it
        # as `compiler` would report protection that is not there, and hide that
        # the cap wants raising.
        self.assertEqual(self.verdict(syntax=finished(0), build=finished(-9)),
                         ("oom", []))

    def test_a_prefilter_stopped_at_the_cap_is_an_oom_too(self):
        # A mutated constant can send the compiler itself off allocating.
        self.assertEqual(self.verdict(syntax=finished(-15)), ("oom", []))

    def test_without_a_prefilter_the_build_is_what_decides(self):
        args = types.SimpleNamespace(quick_reject=False, build_timeout=1, test_timeout=1)
        lane = self.FakeLane(build=finished(0), tests=finished(0), has_syntax_cmd=False)
        self.assertEqual(mutate.evaluate(lane, dict(text="x"), args, "original"), ("survived", []))


class TestEstimate(unittest.TestCase):
    def test_more_lanes_is_never_slower(self):
        self.assertLessEqual(
            mutate.estimate_seconds(PROJECT, 500, lanes=32, cores=32),
            mutate.estimate_seconds(PROJECT, 500, lanes=1, cores=32))

    def test_the_compiling_is_divided_by_the_machine_not_the_lanes(self):
        # The distinction this whole tool is shaped around: every mutant
        # rebuilds every TU, so lanes overlap the tail and nothing else.
        many_cores = mutate.estimate_seconds(PROJECT, 500, lanes=8, cores=64)
        few_cores = mutate.estimate_seconds(PROJECT, 500, lanes=8, cores=8)
        self.assertLess(many_cores, few_cores / 2)

    def test_it_reads_as_a_duration(self):
        self.assertTrue(mutate.estimate(PROJECT, 1, 1, 32).endswith("s"))
        self.assertTrue(mutate.estimate(PROJECT, 500, 32, 32).endswith("min"))
        self.assertTrue(mutate.estimate(PROJECT, 100000, 32, 32).endswith("h"))


class TestCommandLine(unittest.TestCase):
    """The argument rules, checked by running the script -- these are the paths a typo reaches."""

    def run_script(self, *argv):
        return subprocess.run([sys.executable, str(SCRIPT)] + list(argv),
                              capture_output=True, text=True,
                              cwd=str(SCRIPT.parent.parent.parent))

    def test_the_three_bug_modes_are_exclusive(self):
        r = self.run_script("--replace", "a", "b", "--reverse", "HEAD")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("pick one", r.stderr)

    def test_a_missing_file_is_refused(self):
        r = self.run_script("--file", "include/nope.h", "--dry-run")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("no such file", r.stderr)

    def test_a_bug_that_does_not_apply_is_a_message_not_a_traceback(self):
        r = self.run_script("--replace", "this text is not in the header", "x", "--dry-run")
        self.assertEqual(r.returncode, 2)
        self.assertNotIn("Traceback", r.stderr)
        self.assertIn("do not apply", r.stderr)

    def test_a_dry_run_of_the_whole_header_lists_mutants_and_costs_nothing(self):
        r = self.run_script("--dry-run")
        self.assertEqual(r.returncode, 0)
        self.assertIn("mutants over", r.stdout)

    def test_a_dry_run_says_what_each_lane_may_allocate(self):
        # The number is worth seeing before an hour of lanes rather than after,
        # and this is also where "no cgroup here" surfaces -- which is why the
        # assertion is the word and not the phrasing: a machine that cannot cap
        # anything has to say that instead of a size, and CI is such a machine.
        r = self.run_script("--dry-run")
        self.assertEqual(r.returncode, 0)
        self.assertIn("memory", r.stdout)

    def test_a_memory_limit_that_is_not_a_size_is_refused(self):
        r = self.run_script("--memory-limit", "lots", "--dry-run")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("not a size", r.stderr)

    def test_a_sweep_of_lines_holding_no_code_says_so(self):
        # A comment reflow or a version bump lands here, and "nothing to
        # mutate" without a reason reads as a broken tool.
        r = self.run_script("--lines", "1-3", "--dry-run")
        self.assertEqual(r.returncode, 0)
        self.assertIn("nothing to mutate", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=1)
