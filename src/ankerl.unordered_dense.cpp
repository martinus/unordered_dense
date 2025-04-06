module;

// see https://github.com/fmtlib/fmt/blob/master/src/fmt.cc

// Put all implementation-provided headers into the global module fragment
// to prevent attachment to this module.

#if !defined(ANKERL_UNORDERED_DENSE_USE_STD_IMPORT)
#    if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#        define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 1 // NOLINT(cppcoreguidelines-macro-usage)
#    else
#        define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 0 // NOLINT(cppcoreguidelines-macro-usage)
#    endif

#include <ankerl/stl.h>
#endif

#if defined(_MSC_VER) && defined(_M_X64)
#    include <intrin.h>
#    pragma intrinsic(_umul128)
#endif

export module ankerl.unordered_dense;

#if defined(ANKERL_UNORDERED_DENSE_USE_STD_IMPORT)
import std;
#endif

#define ANKERL_UNORDERED_DENSE_EXPORT export
#define ANKER_UNOREDERED_DENSE_DONT_INCLUDE_STL
#include <ankerl/unordered_dense.h>
