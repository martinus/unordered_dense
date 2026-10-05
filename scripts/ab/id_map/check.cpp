// Randomized check of id_map.h against std::map: inserts (with duplicates), operator[], erase, find,
// iteration in ID order, copy, clear. Run under ASan+UBSan:
//   clang++ -std=c++17 -g -O1 -fsanitize=address,undefined check.cpp && ./a.out
#include "id_map.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <string>
#include <vector>

template <class M, class K, class T, class Make>
void check(char const* name, std::uint64_t range, int ops, Make make) {
    std::mt19937_64 rng(42);
    M m;
    std::map<K, T> ref;
    auto fail = [&](char const* what, K k) {
        std::printf("FAIL %s %s key %llu\n", name, what, static_cast<unsigned long long>(k));
        std::exit(1);
    };
    for (int op = 0; op < ops; ++op) {
        auto const k = static_cast<K>(rng() % range);
        switch (rng() % 6) {
        case 0:
        case 1: {
            auto v = make(rng());
            auto [it, ins] = m.emplace(k, v);
            auto ins_ref = ref.emplace(k, v).second;
            if (ins != ins_ref || it->first != k || !(it->second == ref.at(k)))
                fail("emplace", k);
            break;
        }
        case 2: {
            auto v = make(rng());
            m[k] = v;
            ref[k] = v;
            break;
        }
        case 3:
            if (m.erase(k) != ref.erase(k))
                fail("erase", k);
            break;
        default: {
            auto it = m.find(k);
            auto r = ref.find(k);
            if ((it == m.end()) != (r == ref.end()))
                fail("find", k);
            if (r != ref.end() && !(it->second == r->second))
                fail("find value", k);
        }
        }
        if (m.size() != ref.size())
            fail("size", k);
    }
    auto check_order = [&](M const& mm, char const* what) {
        auto r = ref.begin();
        for (auto it = mm.begin(); it != mm.end(); ++it, ++r) {
            if (r == ref.end() || it->first != r->first || !(it->second == r->second))
                fail(what, it->first);
        }
        if (r != ref.end())
            fail(what, r->first);
    };
    check_order(m, "iterate");
    M copy(m);
    check_order(copy, "copy");
    for (auto const& [k, v] : copy) {
        (void)v;
        if (!m.contains(k))
            fail("structured binding", k);
    }
    m.clear();
    if (m.size() != 0 || m.begin() != m.end())
        fail("clear", 0);
    std::printf("ok %s range %llu size %zu\n", name, static_cast<unsigned long long>(range), ref.size());
}

// web-ifc's pattern: ascending IDs in steps of 1-3, every 50th a jump back, a few repeats; then erase
// a third. Ascending inserts are what convert pages early and grow direct pages, which uniformly
// random keys never do.
template <class M>
void check_ascending(char const* name, std::uint32_t n) {
    std::mt19937_64 rng(7);
    M m;
    std::map<std::uint32_t, int> ref;
    std::uint32_t id = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        id += 1 + static_cast<std::uint32_t>(rng() % 3);
        auto k = i % 50 == 49 ? id - static_cast<std::uint32_t>(rng() % 500 % id) : id;
        m.emplace(k, static_cast<int>(i));
        ref.emplace(k, static_cast<int>(i));
    }
    for (auto it = ref.begin(); it != ref.end();) {
        if (rng() % 3 == 0) {
            m.erase(it->first);
            it = ref.erase(it);
        } else {
            ++it;
        }
    }
    auto r = ref.begin();
    for (auto it = m.begin(); it != m.end(); ++it, ++r) {
        if (r == ref.end() || it->first != r->first || it->second != r->second || !m.contains(r->first)) {
            std::printf("FAIL %s ascending key %u\n", name, it->first);
            std::exit(1);
        }
    }
    if (r != ref.end() || m.size() != ref.size()) {
        std::printf("FAIL %s ascending size\n", name);
        std::exit(1);
    }
    std::printf("ok %s ascending %u\n", name, n);
}

// Fill dense IDs, erase all of them in random order with full comparisons along the way, then
// reinsert: drives direct pages back to packed (an eighth of their slots) and frees empty pages.
template <class M>
void check_drain(char const* name, std::uint32_t n) {
    std::mt19937_64 rng(11);
    M m;
    std::map<std::uint32_t, int> ref;
    std::vector<std::uint32_t> ids(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        ids[i] = i;
        m.emplace(i, static_cast<int>(i));
        ref.emplace(i, static_cast<int>(i));
    }
    std::shuffle(ids.begin(), ids.end(), rng);
    auto compare = [&](char const* what) {
        auto r = ref.begin();
        for (auto it = m.begin(); it != m.end(); ++it, ++r) {
            if (r == ref.end() || it->first != r->first || it->second != r->second || !m.contains(r->first)) {
                std::printf("FAIL %s drain %s key %u\n", name, what, it->first);
                std::exit(1);
            }
        }
        if (r != ref.end() || m.size() != ref.size()) {
            std::printf("FAIL %s drain %s size\n", name, what);
            std::exit(1);
        }
    };
    for (std::uint32_t i = 0; i < n; ++i) {
        m.erase(ids[i]);
        ref.erase(ids[i]);
        if (i % (n / 16 + 1) == 0) {
            compare("erase");
        }
    }
    compare("empty");
    for (std::uint32_t i = 0; i < n; i += 3) {
        m.emplace(i, -static_cast<int>(i));
        ref.emplace(i, -static_cast<int>(i));
    }
    compare("refill");
    std::printf("ok %s drain %u\n", name, n);
}

int main() {
    for (std::uint32_t n : {300U, 20000U}) {
        check_drain<idm::id_map<std::uint32_t, int>>("id_map", n);
        check_drain<idm::id_map<std::uint32_t, int, 8, true>>("id_map pairs p8", n);
        check_drain<idm::id_map<std::uint32_t, int, 12, false, idm::tune<true, 2, 4>>>("id_map tuned", n);
        check_drain<idm::id_map<std::uint32_t, int, 12, false, idm::tune<false, 1, 8, true>>>("id_map tagged", n);
        check_drain<idm::id_map<std::uint32_t, int, 12, false, idm::tune<false, 1, 8, false, true>>>("id_map slim", n);
        check_drain<idm::id_map<std::uint32_t, int, 10, true, idm::tune<true, 1, 8, false, true>>>(
            "id_map p10 pairs slim one_alloc", n);
        check_drain<idm::id_map<std::uint32_t, int, 8, true, idm::tune<true, 1, 8, true>>>("id_map pairs p8 tagged one_alloc",
                                                                                           n);
        check_drain<idm::id_map<std::uint32_t, int, 8, true, idm::tune<true, 1, 8>>>("id_map pairs p8 one_alloc", n);
    }
    {
        // a page converts at 64 entries; IDs 0..62 and 64 put its highest slot one past a power of two
        idm::id_map<std::uint32_t, int> m;
        for (std::uint32_t k = 0; k < 63; ++k)
            m.emplace(k, static_cast<int>(k));
        m.emplace(64U, 64);
        int sum = 0;
        for (auto const& [k, v] : m)
            sum += v - static_cast<int>(k);
        std::printf("%s id_map conversion at the top slot\n", sum == 0 && m.size() == 64 && m.contains(64U) ? "ok" : "FAIL");
    }
    for (std::uint32_t n : {50U, 1000U, 100000U}) {
        check_ascending<idm::id_map<std::uint32_t, int>>("id_map", n);
        check_ascending<idm::id_map<std::uint32_t, int, 8>>("id_map p8", n);
        check_ascending<idm::id_map<std::uint32_t, int, 8, true>>("id_map pairs p8", n);
        check_ascending<idm::id_map<std::uint32_t, int, 12, false, idm::tune<true, 2, 4>>>("id_map tuned", n);
        check_ascending<idm::id_map<std::uint32_t, int, 12, false, idm::tune<false, 1, 8, true>>>("id_map tagged", n);
        check_ascending<idm::id_map<std::uint32_t, int, 10, false, idm::tune<false, 1, 8, false, true>>>("id_map p10 slim", n);
        check_ascending<idm::paged_map<std::uint32_t, int>>("paged", n);
        check_ascending<idm::packed_map<std::uint32_t, int>>("hybrid", n);
    }
    auto num = [](std::uint64_t r) {
        return static_cast<int>(r);
    };
    auto str = [](std::uint64_t r) {
        return std::string(r % 40, static_cast<char>('a' + r % 26));
    };
    for (std::uint64_t range : {300ULL, 5000ULL, 200000ULL}) {
        check<idm::paged_map<std::uint32_t, int>, std::uint32_t, int>("paged u32 int", range, 300000, num);
        check<idm::packed_map<std::uint32_t, int, idm::layout::packed>, std::uint32_t, int>(
            "packed u32 int", range, 300000, num);
        check<idm::packed_map<std::uint32_t, int, idm::layout::direct>, std::uint32_t, int>(
            "direct u32 int", range, 300000, num);
        check<idm::packed_map<std::uint32_t, int, idm::layout::hybrid>, std::uint32_t, int>(
            "hybrid u32 int", range, 300000, num);
        check<idm::paged_map<std::uint64_t, std::string, 6>, std::uint64_t, std::string>(
            "paged u64 str b6", range, 100000, str);
        check<idm::id_map<std::uint32_t, int>, std::uint32_t, int>("id_map u32 int", range, 300000, num);
        check<idm::id_map<std::uint32_t, int, 6>, std::uint32_t, int>("id_map u32 int p6", range, 300000, num);
        check<idm::id_map<std::int64_t, std::string, 8>, std::int64_t, std::string>("id_map i64 str p8", range, 100000, str);
        check<idm::id_map<std::uint32_t, int, 12, true>, std::uint32_t, int>("id_map pairs u32 int", range, 300000, num);
        check<idm::id_map<std::int64_t, std::string, 8, true>, std::int64_t, std::string>(
            "id_map pairs i64 str p8", range, 100000, str);
        check<idm::id_map<std::int64_t, std::string, 8, true, idm::tune<true, 2, 4>>, std::int64_t, std::string>(
            "id_map pairs i64 str p8 tuned", range, 100000, str);
        check<idm::id_map<std::int64_t, std::string, 8, true, idm::tune<false, 1, 8, true>>, std::int64_t, std::string>(
            "id_map pairs i64 str p8 tagged", range, 100000, str);
        check<idm::id_map<std::uint32_t, int, 12, false, idm::tune<false, 1, 8, true>>, std::uint32_t, int>(
            "id_map u32 int tagged", range, 300000, num);
        check<idm::id_map<std::uint32_t, int, 12, false, idm::tune<false, 1, 8, false, true>>, std::uint32_t, int>(
            "id_map u32 int slim", range, 300000, num);
        check<idm::id_map<std::int64_t, std::string, 10, true, idm::tune<false, 1, 8, false, true>>,
              std::int64_t,
              std::string>("id_map pairs i64 str p10 slim", range, 100000, str);
        check<idm::packed_map<std::int64_t, std::string, idm::layout::packed>, std::int64_t, std::string>(
            "packed i64 str", range, 100000, str);
        check<idm::packed_map<std::int64_t, std::string, idm::layout::direct>, std::int64_t, std::string>(
            "direct i64 str", range, 100000, str);
        check<idm::packed_map<std::int64_t, std::string, idm::layout::hybrid>, std::int64_t, std::string>(
            "hybrid i64 str", range, 100000, str);
    }
    // the largest key a 32 bit table holds, and key 0
    idm::paged_map<std::uint8_t, int, 4> small;
    for (int k = 0; k < 256; k += 5)
        small[static_cast<std::uint8_t>(k)] = k;
    small[255] = 255;
    int n = 0;
    for (auto it = small.begin(); it != small.end(); ++it)
        ++n;
    std::printf("%s u8 boundary count %d\n", n == 52 ? "ok" : "FAIL", n);
}
