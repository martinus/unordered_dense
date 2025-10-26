module;

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#    define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 1 // NOLINT(cppcoreguidelines-macro-usage)
#else
#    define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 0 // NOLINT(cppcoreguidelines-macro-usage)
#endif

#if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() == 0
#    include <cstdlib> // for abort and UINT64_C
#endif

#if !defined(ANKERL_UNORDERED_DENSE_STD_MODULE)
#    if defined(__cpp_modules) && __cpp_modules >= 201907L && defined(__cpp_lib_modules) && __cpp_lib_modules >= 202207L
#        define ANKERL_UNORDERED_DENSE_STD_MODULE 1
#    else
#        define ANKERL_UNORDERED_DENSE_STD_MODULE 0
#    endif
#else
#error "BBBBBBBBBBBBB"
#endif

#if ANKERL_UNORDERED_DENSE_STD_MODULE
import std;
#endif

#include <ankerl/unordered_dense.h>

export module ankerl.unordered_dense;

export namespace ankerl::unordered_dense {
  using ankerl::unordered_dense::hash;

  using ankerl::unordered_dense::map;
  using ankerl::unordered_dense::segmented_map;
  using ankerl::unordered_dense::set;
  using ankerl::unordered_dense::segmented_set;

  namespace pmr {
    using ankerl::unordered_dense::pmr::map;
    using ankerl::unordered_dense::pmr::segmented_map;
    using ankerl::unordered_dense::pmr::set;
    using ankerl::unordered_dense::pmr::segmented_set;
  }
}

export namespace std {
  using std::erase_if;
}
