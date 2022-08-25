#include <app/ui/periodic.h>     // for Periodic
#include <app/ui/progress_bar.h> // for ProgressBar

#include <fuzz/api.h>          // for api
#include <fuzz/insert_erase.h> // for insert_erase
#include <fuzz/replace.h>      // for replace
#include <fuzz/string.h>       // for string

#include <doctest.h>  // for TestCase, MessageBuilder, skip, INFO
#include <fmt/core.h> // for print

#include <chrono>      // for operator""ms, literals, steady_clock...
#include <cstdint>     // for uint8_t
#include <cstdlib>     // for getenv, size_t
#include <filesystem>  // for directory_iterator, operator<<, begin
#include <fstream>     // for operator<<, basic_istream, ifstream
#include <iterator>    // for istreambuf_iterator, operator!=, dis...
#include <sstream>     // for basic_stringbuf<>::int_type, basic_s...
#include <stdexcept>   // for runtime_error
#include <string>      // for basic_string, operator<<, allocator
#include <string_view> // for string_view

using namespace std::literals;

namespace {

auto env(char const* varname) -> std::string {
#ifdef _MSC_VER
    char* pValue = nullptr;
    size_t len = 0;
    errno_t err = _dupenv_s(&pValue, &len, varname);
    if (err || nullptr == pValue) {
        return "";
    }
    auto str = std::string(pValue);
    free(pValue);
    return str;
#else
    return std::getenv(varname); // NOLINT(concurrency-mt-unsafe,clang-analyzer-cplusplus.StringChecker)
#endif
}

template <typename Op>
void run_corpus(std::string_view name, Op op) {
    auto corpus_base_dir = env("FUZZ_CORPUS_BASE_DIR");
    if (corpus_base_dir.empty()) {
        throw std::runtime_error("Environment variable FUZZ_CORPUS_BASE_DIR not set!");
    }
    INFO("got FUZZ_CORPUS_BASE_DIR='" << corpus_base_dir << "'");
    auto path = std::filesystem::path(corpus_base_dir) / name;

    INFO("loading from '" << path << "'");
    auto num_files = size_t();
    auto periodic = ui::periodic(100ms);

    auto dir = std::filesystem::directory_iterator(path);
    auto const total_files = std::distance(begin(dir), end(dir));
    auto progressbar = ui::progress_bar(50, static_cast<size_t>(total_files));

    for (auto const& dir_entry : std::filesystem::directory_iterator(path)) {
        ++num_files;
        if (periodic) {
            fmt::print("\r|{}| {:7}/{:<7}  ", progressbar(num_files), num_files, total_files);
        }

        auto const& test_file = dir_entry.path();
        INFO("file " << test_file);

        auto f = std::ifstream(test_file);
        auto content = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        op(reinterpret_cast<uint8_t const*>(content.data()), content.size());
    }
    REQUIRE(0U != num_files);
    fmt::print("\r|{}| {:7}/{:<7} {}\n", progressbar(num_files), num_files, total_files, path.string());
}

} // namespace

TEST_CASE("fuzz_api" * doctest::test_suite("fuzz") * doctest::skip(true)) {
    run_corpus("api", fuzz::api);
}

TEST_CASE("fuzz_replace" * doctest::test_suite("fuzz") * doctest::skip(true)) {
    run_corpus("replace", fuzz::replace);
}

TEST_CASE("fuzz_insert_erase" * doctest::test_suite("fuzz") * doctest::skip(true)) {
    run_corpus("insert_erase", fuzz::insert_erase);
}

TEST_CASE("fuzz_string" * doctest::test_suite("fuzz") * doctest::skip(true)) {
    run_corpus("string", fuzz::string);
}
