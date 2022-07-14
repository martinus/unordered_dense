#include <app/ui/Periodic.h>
#include <app/ui/ProgressBar.h>
#include <fuzz/api.h>
#include <fuzz/insert_erase.h>

#include <doctest.h>
#include <fmt/format.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

using namespace std::literals;

namespace {

std::string env(char const* varname) {
#ifdef _MSC_VER
    char* pValue;
    size_t len;
    errno_t err = _dupenv_s(&pValue, &len, varname);
    if (err || nullptr == pValue) {
        return "";
    }
    auto str = std::string(pValue, len);
    free(pValue);
    return str;
#else
    return std::getenv(varname);
#endif
}

template <typename Op>
void run_corpus(std::string_view name, Op op) {
    auto corpus_base_dir = env("FUZZ_CORPUS_BASE_DIR"); // NOLINT(concurrency-mt-unsafe)
    if (corpus_base_dir.empty()) {
        throw std::runtime_error("Environment variable FUZZ_CORPUS_BASE_DIR not set!");
    }

    auto path = std::filesystem::path(corpus_base_dir) / name;

    INFO("loading from " << path);
    auto num_files = size_t();
    auto periodic = ui::Periodic(100ms);

    auto dir = std::filesystem::directory_iterator(path);
    auto const total_files = std::distance(begin(dir), end(dir));
    auto progressbar = ui::ProgressBar(50, total_files);

    for (auto const& dir_entry : std::filesystem::directory_iterator(path)) {
        ++num_files;
        if (periodic) {
            fmt::print("\r|{}| {:7}/{}  ", progressbar(num_files), num_files, total_files);
        }

        auto const& path = dir_entry.path();
        INFO("file " << path);

        auto f = std::ifstream(path);
        auto content = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        op(reinterpret_cast<uint8_t const*>(content.data()), content.size());
    }
    REQUIRE(0U != num_files);
    fmt::print("\r|{}| {:7}/{} {}\n", progressbar(num_files), num_files, total_files, path.string());
}

} // namespace

TEST_CASE("fuzz_api" * doctest::test_suite("fuzz") * doctest::skip(true)) {
    run_corpus("api", fuzz::api);
}

TEST_CASE("fuzz_insert_erase" * doctest::test_suite("fuzz") * doctest::skip(true)) {
    run_corpus("insert_erase", fuzz::insert_erase);
}