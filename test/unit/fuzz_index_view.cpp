#include <ankerl/unordered_dense.h>
#include <fuzz/provider.h>
#include <fuzz/run.h>

#include <app/doctest.h>
#include <app/hashers.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

// An index from outside (#299), fuzzed: bytes the fuzzer chooses go into the view constructor with
// trust::checked and into the owning constructor. Either they are rejected, or a fixed sequence of
// lookups (and for the owning table inserts and erases) has to finish without a sanitizer report or
// a hang. Wrong answers are allowed: the check only promises memory safety and a free slot for every
// placement, and verify() is what finds wrong answers. An erase that runs out of probe sequence
// (on_error_key_changed()) is an accepted outcome.
//
// Two shapes of input, picked by the first byte: raw bytes for the whole index, which mostly tests
// the rejections, and a valid index with a few bytes overwritten, which mostly passes the check and
// tests what a table does on bytes that are wrong in ways the check cannot see.

namespace {

// The key is its own hash, so the fuzzer's bytes and the keys' homes are in the same coordinates.
using iv_map_t = ankerl::unordered_dense::map<std::uint64_t, std::uint64_t, test::identity_hash>;
using iv_view_t = iv_map_t::view_type;
using iv_block_t = iv_map_t::index_block;

constexpr std::size_t iv_num_values = 40;

auto iv_key(std::size_t i) -> std::uint64_t {
    return (static_cast<std::uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15)) | 1U;
}

void iv_exercise(iv_view_t const& v) {
    auto acc = std::size_t{0};
    for (std::size_t i = 0; i < 2 * iv_num_values; ++i) {
        acc += v.count(iv_key(i));
        if (auto it = v.find(iv_key(i)); it != v.end()) {
            acc += static_cast<std::size_t>(it->second);
        }
    }
    static_cast<void>(v.verify(ankerl::unordered_dense::verify_level::full));
    static_cast<void>(acc);
}

void iv_exercise(iv_map_t& m, fuzz::provider& p) {
    try {
        for (std::size_t i = 0; i < 64; ++i) {
            auto const k = iv_key(p.bounded<std::size_t>(2 * iv_num_values));
            switch (p.bounded<int>(3)) {
            case 0:
                m.try_emplace(k, i);
                break;
            case 1:
                m.erase(k);
                break;
            default:
                static_cast<void>(m.count(k));
                break;
            }
        }
    } catch (std::logic_error const&) {
        // on_error_key_changed(): the table is unusable afterwards, which the contract says
        return;
    }
}

void iv_fuzz_body(fuzz::provider p) {
    auto values = std::vector<iv_map_t::value_type>();
    for (std::size_t i = 0; i < iv_num_values; ++i) {
        values.emplace_back(iv_key(i), i);
    }

    auto blocks = std::vector<iv_block_t>();
    auto num_vals = iv_num_values;
    if (p.integral<bool>()) {
        // raw: the block count and every byte
        blocks.resize(std::size_t{1} << p.bounded<unsigned>(5));
        auto* bytes = reinterpret_cast<unsigned char*>(blocks.data());
        for (std::size_t i = 0; i < blocks.size() * sizeof(iv_block_t); ++i) {
            bytes[i] = p.integral<std::uint8_t>();
        }
        num_vals = p.bounded<std::size_t>(iv_num_values + 1);
    } else {
        // a valid index, a few bytes overwritten
        auto const source = iv_map_t(values.begin(), values.end());
        auto const idx = source.index();
        blocks.assign(idx.data(), idx.data() + idx.size());
        auto* bytes = reinterpret_cast<unsigned char*>(blocks.data());
        auto const n = p.bounded<std::size_t>(8);
        for (std::size_t i = 0; i < n; ++i) {
            bytes[p.bounded<std::size_t>(blocks.size() * sizeof(iv_block_t))] = p.integral<std::uint8_t>();
        }
    }
    values.resize(num_vals);
    auto const index = iv_map_t::index_view(blocks.data(), blocks.size());

    try {
        auto const v = iv_view_t({values.data(), values.size()}, index, ankerl::unordered_dense::trust::checked);
        iv_exercise(v);
    } catch (std::invalid_argument const&) {
        // rejected
    }
    try {
        auto copy = values;
        auto m = iv_map_t(std::move(copy), index, ankerl::unordered_dense::trust::checked);
        iv_exercise(m, p);
    } catch (std::invalid_argument const&) {
        // rejected
    }
}

} // namespace

FUZZ_TEST_CASE(fuzz_index_view, p) {
    iv_fuzz_body(p.copy());
}
