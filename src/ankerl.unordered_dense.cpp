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

// The vector probe reads four buckets at once through x86 intrinsics, and those are declared by a
// header included here, in the global module fragment. A translation unit that imports this module
// does not include that header, and clang does not carry the declarations across the module
// boundary either -- instantiating the probe in the importing translation unit then fails to find
// _mm_cmpeq_epi32. So a module is built with the scalar probe. Everything else the module offers is
// unaffected, and a consumer that includes the header directly still gets the vector probe.
#define ANKERL_UNORDERED_DENSE_HAS_SSE2 0 // NOLINT(cppcoreguidelines-macro-usage)

#include <ankerl/unordered_dense.h>

export module ankerl.unordered_dense;

export namespace ankerl::unordered_dense {
    inline namespace ANKERL_UNORDERED_DENSE_NAMESPACE {
      using ankerl::unordered_dense::hash;
      using ankerl::unordered_dense::hash_is_avalanching;
      using ankerl::unordered_dense::hash_is_avalanching_v;
      using ankerl::unordered_dense::require_avalanching;

      using ankerl::unordered_dense::map;
      using ankerl::unordered_dense::segmented_map;
      using ankerl::unordered_dense::set;
      using ankerl::unordered_dense::segmented_set;
      namespace bucket_type {
        using ankerl::unordered_dense::bucket_type::group;
        using ankerl::unordered_dense::bucket_type::group_big;
      }
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
