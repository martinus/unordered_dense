#ifdef MODULES
#    ifdef ANKERL_UNORDERED_DENSE_STD_MODULE
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
