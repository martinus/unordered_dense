module;

// see https://github.com/fmtlib/fmt/blob/master/src/fmt.cc

// Put all implementation-provided headers into the global module fragment
// to prevent attachment to this module.

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#    define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 1 // NOLINT(cppcoreguidelines-macro-usage)
#else
#    define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 0 // NOLINT(cppcoreguidelines-macro-usage)
#endif

#if !defined(ANKERL_UNORDERED_DENSE_USE_STD_IMPORT)
#include <ankerl/stl.h>
#else
#include <cstdint>
#endif

#if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() == 0
#include <cstdlib> // for abort and UINT64_C
#endif

#define ANKERL_UNORDERED_DENSE_EXPORT export

export module ankerl.unordered_dense;

#if defined(ANKERL_UNORDERED_DENSE_USE_STD_IMPORT)
import std;
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:5244)
#endif
#include <ankerl/unordered_dense.h>
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
