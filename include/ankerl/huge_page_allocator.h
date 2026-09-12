///////////////////////// ankerl::unordered_dense::huge_page_allocator /////////////////////////

// An opt-in allocator that puts large blocks on transparent huge pages.
// Version 5.0.0
// https://github.com/martinus/unordered_dense
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Martin Leitner-Ankerl <martin.ankerl@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef ANKERL_UNORDERED_DENSE_HUGE_PAGE_ALLOCATOR_H
#define ANKERL_UNORDERED_DENSE_HUGE_PAGE_ALLOCATOR_H

// A separate header rather than a section of unordered_dense.h, because it needs <sys/mman.h>.
#include "unordered_dense.h" // for the version namespace and ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS

#include <cstddef>     // for size_t, ptrdiff_t
#include <cstdint>     // for uintptr_t
#include <cstdlib>     // for abort
#include <functional>  // for equal_to
#include <memory>      // for allocator
#include <new>         // for bad_alloc
#include <type_traits> // for true_type
#include <utility>     // for pair

#if defined(__linux__) && defined(__has_include)
#    if __has_include(<sys/mman.h>)
#        include <sys/mman.h>                              // for mmap, munmap, madvise, MADV_HUGEPAGE
#        define ANKERL_UNORDERED_DENSE_HAS_MADV_HUGEPAGE 1 // NOLINT(cppcoreguidelines-macro-usage)
#    endif
#endif
#if !defined(ANKERL_UNORDERED_DENSE_HAS_MADV_HUGEPAGE)
#    define ANKERL_UNORDERED_DENSE_HAS_MADV_HUGEPAGE 0 // NOLINT(cppcoreguidelines-macro-usage)
#endif

namespace ankerl::unordered_dense {
inline namespace ANKERL_UNORDERED_DENSE_NAMESPACE {

// What it is for. This map touches two or three regions per operation -- the group block, the
// value it points at, and a string's body -- and every one of them is a random address, so on 4 KB
// pages every one is an address translation. Zen 4's first-level data TLB holds 72 entries: 288 KB.
// Any table larger than that pays an L2 TLB lookup per access, and on a dependent chain -- probe,
// then value, then an erase's second walk -- that lookup is latency. Measured on the scored
// benchmark by running the same binary with the heap on 2 MB pages: 2.6% under gcc and 3.6% under
// clang over all fifteen workloads, 5-8% on churn and on the 50% find at 50000 entries, and 22% of
// a lookup past the last-level cache (notes/index-design.md, "The score runs on 4 KB pages").
//
// What it does. An allocation of at least `Threshold` bytes is `mmap`ed on its own, aligned to 2 MB
// and rounded up to a multiple of it, and `madvise(MADV_HUGEPAGE)`d, which is what asks the kernel
// for huge pages when `/sys/kernel/mm/transparent_hugepage/enabled` is `madvise` -- the default on
// most distributions, and the mode under which a map on `std::allocator` never sees one. Smaller
// allocations go to `std::allocator<T>`. Hand it to the map as the allocator and both regions get
// it, since the index rebinds the value allocator:
//
//     ankerl::unordered_dense::map<K, V, ankerl::unordered_dense::hash<K>, std::equal_to<K>,
//                                  ankerl::unordered_dense::huge_page_allocator<std::pair<K, V>>>
//
// What it cannot do, and it is the reason the threshold cannot go below 2 MB. Huge pages are 2 MB
// each, whole: a 360 KB index cannot be on one by itself. The score's 50000-entry tables got theirs
// from glibc, which `madvise`s the *heap* so that neighbouring small blocks share an extent
// (`GLIBC_TUNABLES=glibc.malloc.hugetlb=1`, glibc 2.35 and later) -- an allocator that owns only its
// own blocks has no neighbour to share with. So this pays off once the blocks themselves are 2 MB:
// an index of about 370000 entries and up, a `std::vector` of values from 2 MB / sizeof(value_type)
// entries and up. Below that, the environment route is the one that works, and README says so.
//
// What it costs. Every block is rounded up to 2 MB, and under `MADV_HUGEPAGE` touching one byte of
// an aligned 2 MB extent populates all of it, so the rounding is resident memory, not just address
// space: a 2.1 MB block occupies 4 MB. The map doubles both of its regions, so at most half of one
// doubling step is lost per region, and only while that region is between doublings. It also means
// a block below the threshold is never rounded, which is what the threshold is for.
//
// Where it is a plain std::allocator. Anywhere without <sys/mman.h> and MADV_HUGEPAGE, which is
// Windows and macOS: the class exists with the same interface, `uses_huge_pages` is false, and
// everything is forwarded, so code written against it compiles everywhere and is only faster where
// the kernel can help. Windows has VirtualAlloc with MEM_LARGE_PAGES, which needs the
// SeLockMemoryPrivilege that an ordinary process does not have, and macOS has superpages through
// its own mmap flags; neither is asked for here.
//
// The allocator is stateless, so instances compare equal, a container copy takes its own, and
// propagation is never a question. The threshold is a template parameter for that reason -- a
// runtime member would make instances differ and make every propagation trait matter.
template <class T, std::size_t Threshold = (std::size_t{2} << 20U)>
class huge_page_allocator {
public:
    static constexpr std::size_t huge_page_size = std::size_t{2} << 20U;
    static constexpr std::size_t threshold = Threshold;
    static constexpr bool uses_huge_pages = ANKERL_UNORDERED_DENSE_HAS_MADV_HUGEPAGE != 0;
    static_assert(Threshold >= huge_page_size, "a block below one huge page cannot be on a huge page of its own");

    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    template <class U>
    struct rebind {
        using other = huge_page_allocator<U, Threshold>;
    };

    constexpr huge_page_allocator() noexcept = default;

    template <class U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    constexpr huge_page_allocator(huge_page_allocator<U, Threshold> const& /*other*/) noexcept {}

    [[nodiscard]] auto allocate(std::size_t n) -> T* {
        if (!is_huge(n)) {
            return std::allocator<T>{}.allocate(n);
        }
#if ANKERL_UNORDERED_DENSE_HAS_MADV_HUGEPAGE
        return static_cast<T*>(map_huge(rounded_bytes(n)));
#else
        return std::allocator<T>{}.allocate(n);
#endif
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (!is_huge(n)) {
            std::allocator<T>{}.deallocate(p, n);
            return;
        }
#if ANKERL_UNORDERED_DENSE_HAS_MADV_HUGEPAGE
        ::munmap(static_cast<void*>(p), rounded_bytes(n));
#else
        std::allocator<T>{}.deallocate(p, n);
#endif
    }

    template <class U, std::size_t Th>
    [[nodiscard]] friend constexpr auto operator==(huge_page_allocator const& /*a*/,
                                                   huge_page_allocator<U, Th> const& /*b*/) noexcept -> bool {
        return Threshold == Th;
    }
    template <class U, std::size_t Th>
    [[nodiscard]] friend constexpr auto operator!=(huge_page_allocator const& a, huge_page_allocator<U, Th> const& b) noexcept
        -> bool {
        return !(a == b);
    }

    // Whether a request for n objects takes the huge page path. Deallocate has to make exactly the
    // same decision from the same n, which is why it is a pure function of n and the types.
    [[nodiscard]] static constexpr auto is_huge(std::size_t n) noexcept -> bool {
        return uses_huge_pages && n >= Threshold / sizeof(T) && n * sizeof(T) >= Threshold;
    }

private:
    [[nodiscard]] static constexpr auto rounded_bytes(std::size_t n) noexcept -> std::size_t {
        auto const bytes = n * sizeof(T);
        return (bytes + huge_page_size - 1) / huge_page_size * huge_page_size;
    }

#if ANKERL_UNORDERED_DENSE_HAS_MADV_HUGEPAGE
    [[noreturn]] static void out_of_memory() {
#    if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()
        throw std::bad_alloc();
#    else
        std::abort();
#    endif
    }

    // mmap gives page alignment, not 2 MB alignment, so this maps one huge page too many, unmaps
    // the misaligned head and the tail it no longer needs, and is left holding exactly
    // [aligned, aligned + len). deallocate then unmaps that range and nothing else needs to be
    // remembered. The madvise is advice: if the kernel cannot or will not, the block is ordinary
    // 4 KB pages and still correct, which is why its result is not checked.
    [[nodiscard]] static auto map_huge(std::size_t len) -> void* {
        auto const span = len + huge_page_size;
        auto* raw = ::mmap(nullptr, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (raw == MAP_FAILED) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
            out_of_memory();
        }
        auto const start = reinterpret_cast<std::uintptr_t>(raw); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        auto const aligned = (start + huge_page_size - 1) / huge_page_size * huge_page_size;
        auto const head = aligned - start;
        auto const tail = span - head - len;
        if (head != 0) {
            ::munmap(raw, head);
        }
        if (tail != 0) {
            ::munmap(reinterpret_cast<void*>(aligned + len),
                     tail); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
        }
        auto* block =
            reinterpret_cast<void*>(aligned); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
        ::madvise(block, len, MADV_HUGEPAGE);
        return block;
    }
#endif
};

// The four containers with this allocator already in the allocator slot, so that opting in is one
// word rather than five template arguments -- the shape `pmr::` has in unordered_dense.h. The
// threshold stays at its default here; a different one is still the allocator written out.
namespace huge_page {

template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket = bucket_type::group>
using map = detail::table<Key, T, Hash, KeyEqual, huge_page_allocator<std::pair<Key, T>>, Bucket, false>;

// The segment size is worth setting here: a segment of 16 MB is at least one whole huge page for any
// element size, where the 4096 byte default is far below the allocator's threshold and gets none.
template <class Key,
          class T,
          class Hash = hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Bucket = bucket_type::group,
          std::size_t MaxSegmentSizeBytes = default_segment_size_bytes>
using segmented_map = detail::table<
    Key,
    T,
    Hash,
    KeyEqual,
    detail::segmented_container_for<std::pair<Key, T>, huge_page_allocator<std::pair<Key, T>>, MaxSegmentSizeBytes>,
    Bucket,
    true>;

template <class Key, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket = bucket_type::group>
using set = detail::table<Key, void, Hash, KeyEqual, huge_page_allocator<Key>, Bucket, false>;

template <class Key,
          class Hash = hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Bucket = bucket_type::group,
          std::size_t MaxSegmentSizeBytes = default_segment_size_bytes>
using segmented_set = detail::table<Key,
                                    void,
                                    Hash,
                                    KeyEqual,
                                    detail::segmented_container_for<Key, huge_page_allocator<Key>, MaxSegmentSizeBytes>,
                                    Bucket,
                                    true>;

} // namespace huge_page

} // namespace ANKERL_UNORDERED_DENSE_NAMESPACE
} // namespace ankerl::unordered_dense

#endif
