#ifdef MODULES
#    if !defined(ANKERL_UNORDERED_DENSE_STD_MODULE)
#        if defined(__cpp_modules) && __cpp_modules >= 201907L && defined(__cpp_lib_modules) && __cpp_lib_modules >= 202207L
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#            define ANKERL_UNORDERED_DENSE_STD_MODULE 1
#        else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#            define ANKERL_UNORDERED_DENSE_STD_MODULE 0
#        endif
#    endif

#    if ANKERL_UNORDERED_DENSE_STD_MODULE
import std;
#    else
#        include <iostream>
#    endif
import ankerl.unordered_dense;
#else
#    include <ankerl/unordered_dense.h>

#    include <iostream>
#endif

auto main() -> int {
    auto map = ankerl::unordered_dense::map<int, std::string>();
    map[123] = "hello";
    map[987] = "world!";

    for (auto const& [key, val] : map) {
        std::cout << key << " => " << val << std::endl;
    }
}
