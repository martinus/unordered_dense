module;

// see https://github.com/fmtlib/fmt/blob/master/src/fmt.cc

// Put all implementation-provided headers into the global module fragment
// to prevent attachment to this module.

#if !defined(ANKERL_UNORDERED_DENSE_USE_STD_IMPORT)
#    include <array>            // for array
#    include <cstdint>          // for uint64_t, uint32_t, uint8_t, UINT64_C
#    include <cstring>          // for size_t, memcpy, memset
#    include <functional>       // for equal_to, hash
#    include <initializer_list> // for initializer_list
#    include <iterator>         // for pair, distance
#    include <limits>           // for numeric_limits
#    include <optional>         // for optional
#    include <memory>           // for allocator, allocator_traits, shared_ptr
#    include <optional>         // for optional
#    include <stdexcept>        // for out_of_range
#    include <string>           // for basic_string
#    include <string_view>      // for basic_string_view, hash
#    include <tuple>            // for forward_as_tuple
#    include <type_traits>      // for enable_if_t, declval, conditional_t, ena...
#    include <utility>          // for forward, exchange, pair, as_const, piece...
#    include <vector>           // for vector

#    if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#        define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 1 // NOLINT(cppcoreguidelines-macro-usage)
#    else
#        define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 0 // NOLINT(cppcoreguidelines-macro-usage)
#    endif

#    if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() == 0
#        include <cstdlib> // for abort
#    endif

#    undef ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS

#    if defined(__has_include)
#        if __has_include(<memory_resource>)
#            include <memory_resource> // for polymorphic_allocator
#        elif __has_include(<experimental/memory_resource>)
#            include <experimental/memory_resource> // for polymorphic_allocator
#        endif
#    endif
#else 
#    include <stdint.h> // for UINT64_C
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
#define ANKER_UNOREDERED_DENSE_DONT_INCLUDE
#include "ankerl/unordered_dense.h"
