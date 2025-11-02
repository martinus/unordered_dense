module;

#if !defined(ANKERL_UNORDERED_DENSE_STD_MODULE)
#    if defined(__cpp_modules) && __cpp_modules >= 201907L && defined(__cpp_lib_modules) && __cpp_lib_modules >= 202207L
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#        define ANKERL_UNORDERED_DENSE_STD_MODULE 1
#    else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#        define ANKERL_UNORDERED_DENSE_STD_MODULE 0
#    endif
#endif

#if ANKERL_UNORDERED_DENSE_STD_MODULE
#    include <cstdint> // for UINT64_C
import std;
#endif

#include <ankerl/unordered_dense.h>

export module ankerl.unordered_dense;

export namespace ankerl::unordered_dense {
    inline namespace ANKERL_UNORDERED_DENSE_NAMESPACE {
      using ankerl::unordered_dense::hash;

      using ankerl::unordered_dense::map;
      using ankerl::unordered_dense::segmented_map;
      using ankerl::unordered_dense::set;
      using ankerl::unordered_dense::segmented_set;
#if defined(ANKERL_UNORDERED_DENSE_PMR)
      namespace pmr {
        using ankerl::unordered_dense::pmr::map;
        using ankerl::unordered_dense::pmr::segmented_map;
        using ankerl::unordered_dense::pmr::set;
        using ankerl::unordered_dense::pmr::segmented_set;
      }
#endif
  }
}

export namespace std {
  using ::std::erase_if;
}
