#if __GNUC__

#    include <fmt/format.h>

#    include <array>
#    include <cstdio>
#    include <cstdlib>
#    include <execinfo.h>
#    include <signal.h>
#    include <unistd.h>

namespace {

void handler(int sig) {
    fmt::print(stderr, "Error: signal {}:\n", sig);
    auto ary = std::array<void*, 50>();

    // get void*'s for all entries on the stack
    auto size = backtrace(ary.data(), static_cast<int>(ary.size()));

    // print out all the frames to stderr
    fmt::print(stderr, "Error: signal {}. See stacktrace with\n", sig);
    fmt::print(stderr, "addr2line -Cafpie ./test/udm");
    for (size_t i = 0; i < static_cast<size_t>(size); ++i) {
        fmt::print(stderr, " {}", ary[i]);
    }
    exit(1); // NOLINT(concurrency-mt-unsafe)
}

class Handler {
public:
    Handler() {
        (void)signal(SIGTERM, handler);
    }
};

auto const h = Handler();

} // namespace

#endif