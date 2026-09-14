// Qt's qobjectdefs.h defines `slots`, `signals` and `emit` as macros unless the build sets
// QT_NO_KEYWORDS, so an identifier of any of those names in a header included after a Qt header is
// rewritten by the preprocessor. `detail::group_storage::slots` was one, and 5.0.0 therefore did
// not compile in a Qt translation unit (#289). This file is that translation unit.
//
// The std headers come first, as they do in a real Qt source: Qt has pulled most of them in long
// before it defines its keywords, and what libstdc++ does with a macro named `signals` is not this
// project's to answer. The map header then has to be the first thing the macros reach, which is
// why it comes before <app/doctest.h> -- that header includes the map itself, and going through it
// would leave the macros with nothing to eat. A unity build, where an earlier file in the chunk
// has already included the map, passes this file vacuously for the same reason; every other leg
// compiles it for real.
#include <ankerl/stl.h>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define slots
#define signals public
#define emit
// NOLINTEND(cppcoreguidelines-macro-usage)

#include <ankerl/unordered_dense.h>

#undef slots
#undef signals
#undef emit

#include <app/doctest.h>

#include <cstddef> // for size_t
#include <string>  // for string

TEST_CASE("qt_keywords") {
    auto map = ankerl::unordered_dense::map<std::string, size_t>();
    map["a"] = 1;
    map["b"] = 2;
    REQUIRE(map.size() == 2U);
    REQUIRE(map.find("a")->second == 1U);
}
