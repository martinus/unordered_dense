#!/usr/bin/env python3
"""Regression tests for scripts/mutate/mutate.py.

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
"""

from __future__ import annotations

import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import types
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "mutate" / "mutate.py"


def load_script():
    """A fresh module object, so a test that patches a global cannot leak into the next one."""
    spec = importlib.util.spec_from_file_location("mutate_under_test", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


mutate = load_script()


def sites(src, line_filter=None):
    """Every mutation the sweep would try on `src`, as 'old -> new' strings."""
    return [s["description"]
            for s in mutate.mutation_sites(src, mutate.code_mask(src), line_filter)]


def finished(returncode, stdout="", stderr=""):
    """A stand-in for a finished subprocess, which is all parse_test_output reads."""
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
    def test_the_replacement_lands_where_the_site_says(self):
        src = "x = a + b;"
        mutants = mutate.site_mutants(
            mutate.mutation_sites(src, mutate.code_mask(src)), src)
        self.assertEqual([m["text"] for m in mutants], ["x = a - b;"])

    def test_a_multi_character_operator_is_replaced_whole(self):
        src = "if (a <= b) {}"
        texts = [m["text"] for m in mutate.site_mutants(
            mutate.mutation_sites(src, mutate.code_mask(src)), src)]
        self.assertEqual(texts, ["if (a < b) {}", "if (a >= b) {}", "if (a == b) {}"])


class TestBugFiles(unittest.TestCase):
    def parse(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bugs.txt"
            path.write_text(text)
            return mutate.parse_bug_file(str(path))

    def test_a_block_becomes_a_bug(self):
        bugs = self.parse("# the name\n<<<\nold line\n===\nnew line\n>>>\n")
        self.assertEqual(bugs, [dict(name="the name", old="old line", new="new line")])

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


class TestTestOutput(unittest.TestCase):
    def test_a_green_run_has_no_failures(self):
        self.assertEqual(mutate.parse_test_output(finished(0, "anything at all")), [])

    def test_a_failed_assertion_names_its_test_case(self):
        out = ("TEST CASE:  erase_the_last_element\n"
               "some/file.cpp:12: ERROR: CHECK( a == b ) is NOT correct!\n")
        self.assertEqual(mutate.parse_test_output(finished(1, out)),
                         ["erase_the_last_element"])

    def test_a_banner_without_an_error_is_not_a_failure(self):
        # doctest prints the banner for anything that produces output, a passing
        # MESSAGE included. Counting those would report a kill that never was.
        out = ("TEST CASE:  chatty_but_passing\n"
               "some/file.cpp:12: MESSAGE: hello\n"
               "TEST CASE:  the_real_one\n"
               "some/file.cpp:13: ERROR: CHECK( x ) is NOT correct!\n")
        self.assertEqual(mutate.parse_test_output(finished(1, out)), ["the_real_one"])

    def test_a_case_is_named_once_however_many_assertions_it_failed(self):
        out = ("TEST CASE:  noisy\n"
               "f.cpp:1: ERROR: a\n"
               "f.cpp:2: ERROR: b\n")
        self.assertEqual(mutate.parse_test_output(finished(1, out)), ["noisy"])

    def test_a_sanitizer_abort_names_the_sanitizer(self):
        # Nonzero exit with no failed assertion. Under -Db_sanitize the thing
        # that noticed is not a test at all, and which runtime complained is the
        # whole answer.
        err = "SUMMARY: AddressSanitizer: heap-buffer-overflow /src/x.h:99 in foo\n"
        self.assertEqual(mutate.parse_test_output(finished(1, "", err)),
                         ["AddressSanitizer heap-buffer-overflow /src/x.h:99 in foo"])

    def test_a_ubsan_runtime_error_is_recognised(self):
        err = "/src/x.h:12:7: runtime error: index 8 out of bounds\n"
        self.assertEqual(mutate.parse_test_output(finished(1, "", err)),
                         ["index 8 out of bounds"])

    def test_a_bare_crash_still_counts_as_caught(self):
        got = mutate.parse_test_output(finished(-11))
        self.assertEqual(got, ["exit -11, no assertion failed"])
        self.assertTrue(got, "a crash must not read as an empty failure list")


class TestCountTestCases(unittest.TestCase):
    """The guard against a filter that matches nothing.

    doctest's suites are the ones TEST_SUITE names; meson's are not. `-ts=unit` names a meson
    suite, runs zero cases, exits 0 -- and every mutant under it comes back survived."""

    def test_the_summary_is_read(self):
        out = "[doctest] test cases:     501 |     501 passed | 0 failed | 27 skipped\n"
        self.assertEqual(mutate.count_test_cases(out), 501)

    def test_a_filter_that_matched_nothing_counts_zero(self):
        out = "[doctest] test cases: 0 | 0 passed | 0 failed | 528 skipped\n"
        self.assertEqual(mutate.count_test_cases(out), 0)

    def test_output_without_a_summary_is_unknown_rather_than_zero(self):
        # A crash before the summary must not be read as "ran no tests", which
        # would abort the run instead of reporting the kill.
        self.assertIsNone(mutate.count_test_cases("Segmentation fault\n"))


class TestNameRendering(unittest.TestCase):
    """Most of this suite is TEST_CASE_TEMPLATE, so a name arrives as 300 characters of
    instantiation with the useful 25 at the front."""

    def test_a_plain_name_is_untouched(self):
        self.assertEqual(mutate.short_test_name("erase_and_shift_down"), "erase_and_shift_down")

    def test_an_instantiation_is_collapsed_but_kept_as_a_marker(self):
        self.assertEqual(
            mutate.short_test_name("bucket_micro<ankerl::unordered_dense::map<int, int>>"),
            "bucket_micro<...>")

    def test_nested_instantiations_collapse_to_one_marker(self):
        self.assertEqual(mutate.short_test_name("t<a<b<c>>>"), "t<...>")

    def test_a_long_name_is_truncated(self):
        self.assertEqual(len(mutate.short_test_name("x" * 200)), 52)

    def test_the_first_few_tests_are_shown_and_the_rest_counted(self):
        self.assertEqual(mutate.render_caught(["a", "b", "c", "d", "e"]),
                         "a, b, c (+2 more)")

    def test_exactly_the_limit_is_not_followed_by_a_count(self):
        self.assertEqual(mutate.render_caught(["a", "b", "c"]), "a, b, c")

    def test_nothing_caught_it_renders_as_nothing(self):
        self.assertEqual(mutate.render_caught([]), "")


class TestMesonSetupArgs(unittest.TestCase):
    def args(self, **kwargs):
        return types.SimpleNamespace(buildtype="debug", meson_arg=[], **kwargs)

    def test_debug_info_is_off_by_default(self):
        # -O0 without -g: a third of the compile time and two thirds of each
        # lane's build directory, and nothing here reads a symbol.
        self.assertIn("-Ddebug=false", mutate.meson_setup_args(self.args()))

    def test_an_explicit_debug_option_wins(self):
        got = mutate.meson_setup_args(
            types.SimpleNamespace(buildtype="debug", meson_arg=["-Ddebug=true"]))
        self.assertNotIn("-Ddebug=false", got)
        self.assertIn("-Ddebug=true", got)

    def test_the_buildtype_is_passed_through(self):
        got = mutate.meson_setup_args(
            types.SimpleNamespace(buildtype="release", meson_arg=[]))
        self.assertEqual(got[:2], ["--buildtype", "release"])

    def test_extra_arguments_come_last_so_they_can_override(self):
        got = mutate.meson_setup_args(
            types.SimpleNamespace(buildtype="debug", meson_arg=["-Db_sanitize=address"]))
        self.assertEqual(got[-1], "-Db_sanitize=address")


class TestFingerprint(unittest.TestCase):
    def facts(self, **kwargs):
        args = types.SimpleNamespace(buildtype="debug", meson_arg=[], test_suite=None,
                                     exclude_suite=None, test_filter=None,
                                     file=mutate.HEADER, lanes=4, jobs=2,
                                     memory_limit=2 * mutate.GIB, memory_note="")
        for key, value in kwargs.items():
            setattr(args, key, value)
        return mutate.fingerprint(args)

    def test_a_plain_build_says_it_has_no_sanitizer(self):
        # The one the report must not stay quiet about: without a sanitizer, a
        # mutant that reads one slot too far comes back survived.
        self.assertEqual(self.facts()["sanitizer"], "none")
        self.assertIn("no sanitizer", mutate.render_fingerprint(self.facts()))

    def test_a_sanitizer_build_says_which(self):
        facts = self.facts(meson_arg=["-Db_sanitize=address,undefined"])
        self.assertEqual(facts["sanitizer"], "address,undefined")
        self.assertNotIn("no sanitizer", mutate.render_fingerprint(facts))

    def test_the_test_filters_are_recorded(self):
        self.assertEqual(self.facts(exclude_suite="fuzz")["tests"], "-tse=fuzz")
        self.assertEqual(self.facts()["tests"], "all")

    def test_a_capped_run_says_how_much_and_warns_about_nothing(self):
        self.assertIn("2.0 GiB per lane", mutate.render_fingerprint(self.facts()))
        self.assertNotIn("nothing caps", mutate.render_fingerprint(self.facts()))

    def test_an_uncapped_run_says_so_and_says_why(self):
        # The one the report must not stay quiet about either: without a cap a
        # runaway mutant takes down whatever the kernel picks, and the verdicts
        # of the lanes beside it are then guesses.
        facts = self.facts(memory_limit=0, memory_note="Failed to connect to bus")
        rendered = mutate.render_fingerprint(facts)
        self.assertIn("memory          off", rendered)
        self.assertIn("nothing caps", rendered)
        self.assertIn("Failed to connect to bus", rendered)

    def test_a_cap_turned_off_on_purpose_still_warns(self):
        # --memory-limit 0 leaves no reason to print, and the warning is about
        # the consequence rather than the cause.
        self.assertIn("nothing caps",
                      mutate.render_fingerprint(self.facts(memory_limit=0)))


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
        self._repo = mutate.REPO
        mutate.REPO = str(self.src)  # lane_ignore keys the root-only skips off this
        shutil.copytree(self.src, self.dst)

    def tearDown(self):
        mutate.REPO = self._repo
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_a_changed_file_is_copied_and_stamped_now(self):
        header = self.src / "include" / "h.h"
        header.write_text("mutated\n")
        os.utime(header, (1, 1))  # an old mtime, as a checkout or a git op leaves
        mutate.sync_tree(str(self.src), str(self.dst))
        copy = self.dst / "include" / "h.h"
        self.assertEqual(copy.read_text(), "mutated\n")
        self.assertGreater(copy.stat().st_mtime, time.time() - 60)

    def test_an_unchanged_file_keeps_its_mtime(self):
        # The point of the whole function: restamping every source would cost a
        # full rebuild in every lane, which is most of what reuse saves.
        keep = self.dst / "keep.txt"
        os.utime(keep, (1000, 1000))
        mutate.sync_tree(str(self.src), str(self.dst))
        self.assertEqual(keep.stat().st_mtime, 1000)

    def test_a_file_with_the_same_size_but_different_bytes_is_copied(self):
        # filecmp with shallow=True would call these equal on a same-size,
        # same-mtime pair, and the lane would test the wrong source.
        (self.src / "keep.txt").write_text("SAME\n")
        os.utime(self.src / "keep.txt", (1000, 1000))
        os.utime(self.dst / "keep.txt", (1000, 1000))
        mutate.sync_tree(str(self.src), str(self.dst))
        self.assertEqual((self.dst / "keep.txt").read_text(), "SAME\n")

    def test_a_deleted_file_is_removed_from_the_lane(self):
        # Or it keeps being compiled, and the lane builds a test file that the
        # working tree no longer has.
        (self.src / "keep.txt").unlink()
        mutate.sync_tree(str(self.src), str(self.dst))
        self.assertFalse((self.dst / "keep.txt").exists())

    def test_a_new_file_arrives(self):
        (self.src / "new.cpp").write_text("int main() {}\n")
        mutate.sync_tree(str(self.src), str(self.dst))
        self.assertTrue((self.dst / "new.cpp").exists())

    def test_the_lane_build_directory_is_not_touched(self):
        # It is the one directory in the lane with no counterpart in the repo,
        # and deleting it would turn every reuse into a cold build.
        build = self.dst / "builddir"
        build.mkdir()
        (build / "build.ninja").write_text("rules\n")
        mutate.sync_tree(str(self.src), str(self.dst))
        self.assertTrue((build / "build.ninja").exists())

    def test_ignored_directories_are_not_copied(self):
        (self.src / ".git").mkdir()
        (self.src / ".git" / "HEAD").write_text("ref\n")
        (self.src / "builddir").mkdir()
        (self.src / "builddir" / "junk").write_text("x\n")
        mutate.sync_tree(str(self.src), str(self.dst))
        self.assertFalse((self.dst / ".git").exists())
        self.assertFalse((self.dst / "builddir" / "junk").exists())

    def test_a_build_named_file_outside_the_root_is_kept(self):
        # `build*` as a global pattern would eat scripts/build.py, which is why
        # the build directories are matched at the repo root only.
        (self.src / "scripts").mkdir()
        (self.src / "scripts" / "build.py").write_text("#!/usr/bin/env python3\n")
        mutate.sync_tree(str(self.src), str(self.dst))
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
                mutate.check_room_for(tmp, 10 ** 6, lambda message: None)
            self.assertIn("free", str(e.exception))
            self.assertIn("--lanes", str(e.exception))

    def test_room_for_one_lane_is_not_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            mutate.check_room_for(tmp, 1, lambda message: None)

    def test_reusing_every_lane_asks_for_no_room_at_all(self):
        # And so cannot fail on a workdir already holding exactly what it needs.
        mutate.check_room_for("/nonexistent", 0, lambda message: None)

    def test_a_tmpfs_workdir_is_named_as_memory(self):
        # The line about it is the difference between "the disk is full" and
        # "the machine is out of RAM", which want different answers.
        real = mutate.memory_backed
        mutate.memory_backed = lambda path: True
        try:
            said = []
            with tempfile.TemporaryDirectory() as tmp:
                mutate.check_room_for(tmp, 1, said.append)
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
        return mutate.evaluate(lane, dict(text="mutated"), args)

    def test_the_mutant_is_written_before_anything_is_run(self):
        lane = self.FakeLane(syntax=finished(1))
        mutate.evaluate(lane, dict(text="mutated source"),
                        types.SimpleNamespace(quick_reject=True, build_timeout=1,
                                              test_timeout=1))
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
        self.assertEqual(mutate.evaluate(lane, dict(text="x"), args), ("survived", []))


class TestEstimate(unittest.TestCase):
    def test_more_lanes_is_never_slower(self):
        self.assertLessEqual(
            mutate.estimate_seconds(500, lanes=32, cores=32),
            mutate.estimate_seconds(500, lanes=1, cores=32))

    def test_the_compiling_is_divided_by_the_machine_not_the_lanes(self):
        # The distinction this whole tool is shaped around: every mutant
        # rebuilds every TU, so lanes overlap the tail and nothing else.
        many_cores = mutate.estimate_seconds(500, lanes=8, cores=64)
        few_cores = mutate.estimate_seconds(500, lanes=8, cores=8)
        self.assertLess(many_cores, few_cores / 2)

    def test_it_reads_as_a_duration(self):
        self.assertTrue(mutate.estimate(1, 1, 32).endswith("s"))
        self.assertTrue(mutate.estimate(500, 32, 32).endswith("min"))
        self.assertTrue(mutate.estimate(100000, 32, 32).endswith("h"))


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
