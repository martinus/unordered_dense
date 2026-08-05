#pragma once

#include <fuzz/provider.h>

#include <functional>

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
extern "C" {
void HF_ITER(const uint8_t** buf_ptr, size_t* len_ptr);
}
#endif

namespace fuzz {

namespace detail {

void evaluate_corpus(std::function<void(provider)> const& op);

} // namespace detail

/**
 * There are 2 modes how this the op() will be executed:
 *
 * Driven by honggfuzz: this is enabled when compiling with -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION.
 * This is done to fuzz a particular test.
 *
 * Otherwise, this is run in "corpus" mode, where all files in a directory named by the testname are evaluated
 * This should be done in normal unit testing. The location of the corpus base directory is determined in this order:
 * 1. Use FUZZ_CORPUS_BASE_DIR environment variable
 * 2. If this is not set, look in the working directory for a ".fuzz-corpus-base-dir" file which should contain
 *    the path to the base directory (relative to that particular file)
 */
template <typename Op>
void run(Op const& op) {
#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) && !defined(FUZZ)
    size_t len = 0;
    uint8_t const* buf = nullptr;
    while (true) {
        ::HF_ITER(&buf, &len);
        op(provider(buf, len));
    }
#else
    detail::evaluate_corpus(op);
#endif
}

} // namespace fuzz

/**
 * Declares a fuzz target, in both of the shapes it is needed in.
 *
 * Without -DFUZZ this is the doctest test case that replays the committed corpus in
 * data/fuzz/<name>, which is what the unit test suite runs on every build. With -DFUZZ the same
 * body becomes libFuzzer's entry point instead, so the target that searches for new inputs and the
 * target that guards against the old ones can never drift apart. Used as:
 *
 *     FUZZ_TEST_CASE(fuzz_api, p) {
 *         do_fuzz_api<some_map>(p.copy());
 *     }
 */
#if defined(FUZZ)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define FUZZ_TEST_CASE(name, provider_param)                                                    \
        static void fuzz_body_##name(fuzz::provider provider_param);                                \
        extern "C" auto LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size) -> int { \
            fuzz_body_##name(fuzz::provider(data, size));                                           \
            return 0;                                                                               \
        }                                                                                           \
        static void fuzz_body_##name(fuzz::provider provider_param)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define FUZZ_TEST_CASE(name, provider_param)                     \
        static void fuzz_body_##name(fuzz::provider provider_param); \
        TEST_CASE(#name* doctest::test_suite("fuzz")) {              \
            fuzz::run([](fuzz::provider p) {                         \
                fuzz_body_##name(p.copy());                          \
            });                                                      \
        }                                                            \
        static void fuzz_body_##name(fuzz::provider provider_param)
#endif
