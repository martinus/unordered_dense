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

#define ANKERL_UNORDERED_DENSE_EXPORT export

export module ankerl.unordered_dense;

#if defined(ANKERL_UNORDERED_DENSE_USE_STD_IMPORT)
import std;
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#include <ankerl/unordered_dense.h>
#pragma clang diagnostic pop
