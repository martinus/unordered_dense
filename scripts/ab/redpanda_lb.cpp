// Redpanda's leader balancer benchmark (`lb.random_generator` in src/v/cluster/tests/
// leader_balancer_bench.cc), without Seastar, with the container of `_current_leaders` as a template
// parameter: the six containers redpanda#17182 compared (#317).
//
// What is timed, as in Redpanda: constructing `random_reassignments` from a cluster index of 72
// nodes x 16 shards x 80 groups, 3 replicas each, then 184320 `generate_reassignment()` calls. The
// group ids run 0..79 on every shard, so `_current_leaders` ends up with 80 keys after 92160
// assignments, and every lookup hits. In a real cluster every raft group has its own id, so the map
// holds one entry per partition: -DUNIQUE_GROUPS numbers the groups 0..92159 instead, which gives
// `_current_leaders` 92160 entries and changes nothing else. Differences from Redpanda's: the index is a flat vector
// rather than absl::node_hash_map<shard, btree_map<group, replicas>> (the same content, iterated
// in shard order), the replica list is a std::vector rather than fragmented_vector, the RNG is
// std::mt19937_64 with uniform_int_distribution, and `segmented_map` uses its own segmented_vector
// rather than Redpanda's chunked_vector.
//
//   clang++ -O3 -DNDEBUG -std=c++17 -I<header dir> -I<abseil>/include scripts/ab/redpanda_lb.cpp \
//       -Wl,--start-group <abseil>/lib64/libabsl_*.a -Wl,--end-group
//   ./a.out ROUNDS     # prints: header container median_ms min_ms
#include <ankerl/unordered_dense.h>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#ifdef WITH_ID_MAP
#    include "id_map/id_map.h" // #379: -DWITH_ID_MAP adds the id_map prototype as a seventh container
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct broker_shard {
    std::int32_t node_id;
    std::uint32_t shard;
    friend auto operator==(broker_shard a, broker_shard b) -> bool {
        return a.node_id == b.node_id && a.shard == b.shard;
    }
    friend auto operator!=(broker_shard a, broker_shard b) -> bool {
        return !(a == b);
    }
};

struct reassignment {
    std::int64_t group;
    broker_shard from;
    broker_shard to;
};

struct index_entry {
    broker_shard shard;
    std::int64_t group;
    std::vector<broker_shard> replicas;
};

constexpr int node_count = 72;
constexpr unsigned shards_per_node = 16;
constexpr unsigned groups_per_shard = 80;
constexpr std::size_t replica_count = 3;
constexpr int total_reassignments = groups_per_shard * shards_per_node * node_count * (replica_count - 1);

// leader_balancer_test_utils::make_cluster_index, flattened.
auto make_cluster_index() -> std::vector<index_entry> {
    auto shards = std::vector<broker_shard>{};
    for (int n = 0; n < node_count; ++n) {
        for (unsigned s = 0; s < shards_per_node; ++s) {
            shards.push_back({n, s});
        }
    }
    auto index = std::vector<index_entry>{};
    std::size_t replica = 0;
    for (auto shard : shards) {
        for (unsigned g = 0; g < groups_per_shard; ++g) {
            auto replicas = std::vector<broker_shard>{shard};
            while (replicas.size() != replica_count) {
                if (shards[replica % shards.size()] != shard) {
                    replicas.push_back(shards[replica % shards.size()]);
                }
                ++replica;
            }
#ifdef UNIQUE_GROUPS
            auto const group = static_cast<std::int64_t>(index.size());
#else
            auto const group = static_cast<std::int64_t>(g);
#endif
            index.push_back({shard, group, std::move(replicas)});
        }
    }
    return index;
}

std::mt19937_64 gen{12345};

// cluster::leader_balancer_types::random_reassignments at redpanda#17182, with Map for its
// chunked_hash_map.
template <typename Map>
class random_reassignments {
public:
    explicit random_reassignments(std::vector<index_entry> const& si) {
        for (auto const& e : si) {
            _current_leaders[e.group] = e.shard;
            for (auto const& r : e.replicas) {
                _replicas.push_back({e.group, r});
            }
        }
    }

    auto generate_reassignment() -> std::optional<reassignment> {
        while (_replicas_begin < _replicas.size()) {
            auto ri = std::uniform_int_distribution<std::size_t>(_replicas_begin, _replicas.size() - 1)(gen);
            std::swap(_replicas[_replicas_begin], _replicas[ri]);
            auto const& replica = _replicas[_replicas_begin];
            _replicas_begin += 1;
            auto it = _current_leaders.find(replica.group_id);
            if (it == _current_leaders.end()) {
                std::abort();
            }
            auto const& leader = it->second;
            if (leader == replica.shard) {
                continue;
            }
            return reassignment{replica.group_id, leader, replica.shard};
        }
        return std::nullopt;
    }

private:
    struct replica_t {
        std::int64_t group_id;
        broker_shard shard;
    };
    Map _current_leaders;
    std::vector<replica_t> _replicas;
    std::size_t _replicas_begin = 0;
};

template <typename T>
void do_not_optimize(T const& v) {
    asm volatile("" : : "r,m"(v) : "memory"); // NOLINT(hicpp-no-assembler)
}

template <typename Map>
auto one_run(std::vector<index_entry> const& index) -> double {
    auto t0 = std::chrono::steady_clock::now();
    random_reassignments<Map> rt{index};
    do_not_optimize(rt);
    for (int i = 0; i < total_reassignments; ++i) {
        auto r = rt.generate_reassignment();
        if (!r) {
            std::abort();
        }
        do_not_optimize(r);
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

using K = std::int64_t;
using V = broker_shard;
using std_map = std::map<K, V>;
using absl_flat = absl::flat_hash_map<K, V>;
using absl_node = absl::node_hash_map<K, V>;
using absl_btree = absl::btree_map<K, V>;
using ud_map = ankerl::unordered_dense::map<K, V>;
using ud_segmented = ankerl::unordered_dense::segmented_map<K, V>;
#ifdef WITH_ID_MAP
using id_map = idm::id_map<K, V>;
#endif

} // namespace

int main(int argc, char** argv) {
    int const rounds = argc > 1 ? std::atoi(argv[1]) : 50;
    // A second argument runs that container only, for perf stat.
    std::string const only = argc > 2 ? argv[2] : "";
    auto const index = make_cluster_index();
    using fn = double (*)(std::vector<index_entry> const&);
    struct variant {
        char const* name;
        fn run;
        std::vector<double> ms;
    };
    auto variants = std::vector<variant>{
        {"std_map", &one_run<std_map>, {}},
        {"absl_flat", &one_run<absl_flat>, {}},
        {"absl_node", &one_run<absl_node>, {}},
        {"absl_btree", &one_run<absl_btree>, {}},
        {"unordered_dense", &one_run<ud_map>, {}},
        {"unordered_dense_segmented", &one_run<ud_segmented>, {}},
#ifdef WITH_ID_MAP
        {"id_map", &one_run<id_map>, {}},
#endif
    };
    if (!only.empty()) {
        variants.erase(std::remove_if(variants.begin(),
                                      variants.end(),
                                      [&](variant const& v) {
                                          return only != v.name;
                                      }),
                       variants.end());
        if (variants.empty()) {
            std::fprintf(stderr, "no container named %s\n", only.c_str());
            return 1;
        }
    }
    // One untimed pass over every variant, then rounds with the order rotated per round.
    for (auto& v : variants) {
        v.run(index);
    }
    for (int r = 0; r < rounds; ++r) {
        for (std::size_t i = 0; i < variants.size(); ++i) {
            auto& v = variants[(i + static_cast<std::size_t>(r)) % variants.size()];
            v.ms.push_back(v.run(index));
        }
    }
    for (auto& v : variants) {
        std::sort(v.ms.begin(), v.ms.end());
        std::printf("%d.%d.%d %s %.3f %.3f\n",
                    ANKERL_UNORDERED_DENSE_VERSION_MAJOR,
                    ANKERL_UNORDERED_DENSE_VERSION_MINOR,
                    ANKERL_UNORDERED_DENSE_VERSION_PATCH,
                    v.name,
                    v.ms[v.ms.size() / 2],
                    v.ms.front());
    }
    return 0;
}
