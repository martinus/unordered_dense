#include <ankerl/unordered_dense.h>
#include <fuzz/provider.h>
#include <fuzz/run.h>

#include <app/doctest.h>

#include <unordered_map>

namespace {

// The longest key this could produce was 32 bytes, and the hash treats lengths quite differently:
// up to 16 bytes is two reads and no loop, over 48 runs three lanes, over 96 runs six. Everything
// past 48 bytes was therefore unreachable from here, which is the wrong half of wyhash to leave
// unfuzzed -- a mistake in a lane loop is silent, it just hashes wrongly. 224 reaches every path
// with room to spare; short keys stay far more likely, since the provider stops at the first
// terminator it reads.
static constexpr auto max_key_length = size_t{224};

template <typename Map>
void do_string(fuzz::provider p) {
    auto ank = Map();
    auto ref = std::unordered_map<std::string, std::string>();

    while (p.has_remaining_bytes()) {
        auto str = p.string(max_key_length);
        REQUIRE(ank.try_emplace(str, "hello!").second == ref.try_emplace(str, "hello!").second);

        str = p.string(max_key_length);
        auto it_ank = ank.find(str);
        auto it_ref = ref.find(str);
        REQUIRE((it_ank == ank.end()) == (it_ref == ref.end()));

        if (it_ank != ank.end()) {
            ank.erase(it_ank);
            ref.erase(it_ref);
        }
        REQUIRE(ank.size() == ref.size());

        str = p.string(max_key_length);
        REQUIRE(ank.try_emplace(str, "huh").second == ref.try_emplace(str, "huh").second);

        str = p.string(max_key_length);
        REQUIRE(ank.erase(str) == ref.erase(str));
    }

    REQUIRE(std::unordered_map(ank.begin(), ank.end()) == ref);
}

} // namespace

FUZZ_TEST_CASE(fuzz_string, p) {
    do_string<ankerl::unordered_dense::map<std::string, std::string>>(p.copy());
    do_string<ankerl::unordered_dense::segmented_map<std::string, std::string>>(p.copy());
    do_string<ankerl::unordered_dense::map<std::string,
                                           std::string,
                                           ankerl::unordered_dense::hash<std::string>,
                                           std::equal_to<std::string>,
                                           std::deque<std::pair<std::string, std::string>>>>(p.copy());
}
