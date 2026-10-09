///////////////////////// ankerl::unordered_dense::mapped_view /////////////////////////

// A file mapping that owns a map_view or set_view over its bytes.
// Version 5.3.2
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

#ifndef ANKERL_UNORDERED_DENSE_MAPPED_VIEW_H
#define ANKERL_UNORDERED_DENSE_MAPPED_VIEW_H

// A separate header rather than a section of unordered_dense.h, because it needs <sys/mman.h>.
#include "unordered_dense.h" // for map_view, trust, the version namespace and ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS

#include <cerrno>       // for errno, EINTR, EIO, EOVERFLOW
#include <cstddef>      // for size_t, byte
#include <cstdint>      // for uint8_t, uintptr_t
#include <cstdlib>      // for abort
#include <stdexcept>    // for invalid_argument
#include <system_error> // for system_error, generic_category
#include <utility>      // for move, swap

#if defined(__has_include)
#    if __has_include(<sys/mman.h>) && __has_include(<unistd.h>) && __has_include(<fcntl.h>) && __has_include(<sys/stat.h>)
#        include <fcntl.h>                               // for open, O_RDONLY, O_CLOEXEC
#        include <sys/mman.h>                            // for mmap, munmap, mprotect, madvise
#        include <sys/stat.h>                            // for fstat
#        include <unistd.h>                              // for close, pread
#        define ANKERL_UNORDERED_DENSE_HAS_MAPPED_VIEW 1 // NOLINT(cppcoreguidelines-macro-usage)
#    endif
#endif
#if !defined(ANKERL_UNORDERED_DENSE_HAS_MAPPED_VIEW)
#    define ANKERL_UNORDERED_DENSE_HAS_MAPPED_VIEW 0 // NOLINT(cppcoreguidelines-macro-usage)
#endif

#if ANKERL_UNORDERED_DENSE_HAS_MAPPED_VIEW

namespace ankerl::unordered_dense {
inline namespace ANKERL_UNORDERED_DENSE_NAMESPACE {

// Where the bytes of a mapped_file live, and when they are read.
//
// `file` maps the file itself, read only and MAP_SHARED: no copy, nothing read until a lookup
// touches it, and one copy in the page cache for every process that maps the same file. (A view
// constructed with trust::checked reads the whole index once, which pages all of it in: lazy is
// trust::unchecked, over bytes the caller vouches for.) Its pages
// are the page cache's, which is 4 KB pages unless the file is on hugetlbfs, where they are 2 MB
// with nothing else to ask for.
//
// `file_populated` is the same mapping with MAP_POPULATE: the whole file is read and mapped before
// the constructor returns, in large sequential reads where a lazy mapping pays one fault per page it
// touches, at random.
//
// `huge_copy` reads the file into private anonymous memory on 2 MB pages: MAP_HUGETLB where
// /proc/sys/vm/nr_hugepages has pages reserved, else transparent huge pages through MADV_HUGEPAGE,
// else, if the kernel gives neither, 4 KB pages. It pays the read up front and holds its own copy in
// every process.
//
// Which to pick, from map<uint64_t, uint64_t> measured at 1M to 64M entries (the numbers are in
// doc/usage.md, "Mapping a file", and notes/index-design.md, "A map_view over a mapped file"):
// `file` is the default. It starts fastest when the file is in the page cache, shares one copy
// between processes, and on hugetlbfs it is also as fast as anything here. On 4 KB pages random
// lookups past the cache are about a tenth slower than on 2 MB pages. `file_populated` is for a 4 KB
// file that is likely not in the page cache, where a lazy mapping pays one random read per page it
// touches. `huge_copy` is for a process that does many lookups, does not share the file and has no
// hugetlbfs.
enum class mapping : std::uint8_t { file, file_populated, huge_copy };

namespace detail {

[[noreturn]] inline void on_error_mapping(int err, char const* what) {
#    if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()
    throw std::system_error(err, std::generic_category(), what);
#    else
    static_cast<void>(err);
    static_cast<void>(what);
    std::abort();
#    endif
}

[[noreturn]] inline void on_error_layout() {
#    if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()
    throw std::invalid_argument(
        "ankerl::unordered_dense::mapped_view: the values or the index reach past the end of the file");
#    else
    std::abort();
#    endif
}

} // namespace detail

// A whole file, mapped read only, unmapped by the destructor. Move only. An empty file maps
// nothing: data() is null and size() 0.
class mapped_file {
    static constexpr std::size_t huge_page_size = std::size_t{2} << 20U;

    std::byte* m_region = nullptr;
    std::size_t m_region_size = 0; // what munmap gets: the file's size, or rounded to 2 MB for a copy
    std::size_t m_size = 0;        // the file's size

    // Closes the descriptor on every way out of the constructor.
    struct descriptor {
        int fd;
        explicit descriptor(char const* path)
            : fd(::open(path, O_RDONLY | O_CLOEXEC)) { // NOLINT(cppcoreguidelines-pro-type-vararg)
            if (fd < 0) {
                detail::on_error_mapping(errno, "ankerl::unordered_dense::mapped_file: open");
            }
        }
        descriptor(descriptor const&) = delete;
        descriptor(descriptor&&) = delete;
        auto operator=(descriptor const&) -> descriptor& = delete;
        auto operator=(descriptor&&) -> descriptor& = delete;
        ~descriptor() {
            ::close(fd);
        }
    };

    // Private anonymous memory for the copy, on 2 MB pages if the kernel gives them. Every way to
    // get them is advice the kernel may decline, so only the final mmap failing is an error.
    [[nodiscard]] static auto map_anonymous(std::size_t len) -> std::byte* {
        if (len < huge_page_size) {
            auto* small = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (small == MAP_FAILED) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
                detail::on_error_mapping(errno, "ankerl::unordered_dense::mapped_file: mmap");
            }
            return static_cast<std::byte*>(small);
        }
#    if defined(MAP_HUGETLB)
        auto* hugetlb = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (hugetlb != MAP_FAILED) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
            return static_cast<std::byte*>(hugetlb);
        }
#    endif
        // One huge page more than asked, to cut a 2 MB-aligned range out of it, as
        // huge_page_allocator does: a transparent huge page needs an aligned 2 MB extent.
        auto const span = len + huge_page_size;
        auto* raw = ::mmap(nullptr, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (raw == MAP_FAILED) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
            detail::on_error_mapping(errno, "ankerl::unordered_dense::mapped_file: mmap");
        }
        auto const start = reinterpret_cast<std::uintptr_t>(raw); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        auto const aligned = (start + huge_page_size - 1) / huge_page_size * huge_page_size;
        auto const head = aligned - start;
        auto const tail = span - head - len;
        if (head != 0) {
            ::munmap(raw, head);
        }
        if (tail != 0) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
            ::munmap(reinterpret_cast<void*>(aligned + len), tail);
        }
        auto* region = reinterpret_cast<std::byte*>(
            aligned); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
#    if defined(MADV_HUGEPAGE)
        ::madvise(region, len, MADV_HUGEPAGE);
#    endif
        return region;
    }

    void read_into(int fd) const {
        auto done = std::size_t{0};
        while (done != m_size) {
            auto const left = m_size - done;
            auto const chunk = left < (std::size_t{1} << 30U) ? left : (std::size_t{1} << 30U);
            auto const got = ::pread(fd, m_region + done, chunk, static_cast<off_t>(done));
            if (got < 0 && errno == EINTR) {
                continue;
            }
            if (got <= 0) {
                // a file that shrank under the read is as wrong as one that cannot be read
                detail::on_error_mapping(got < 0 ? errno : EIO, "ankerl::unordered_dense::mapped_file: read");
            }
            done += static_cast<std::size_t>(got);
        }
    }

    void release() noexcept {
        if (m_region != nullptr) {
            ::munmap(m_region, m_region_size);
        }
        m_region = nullptr;
        m_region_size = 0;
        m_size = 0;
    }

public:
    mapped_file() noexcept = default;

    // Delegates, so that a throw after the mapping exists runs the destructor and unmaps it.
    explicit mapped_file(char const* path, mapping how = mapping::file)
        : mapped_file() {
        auto const d = descriptor(path);
        struct stat st{};
        if (::fstat(d.fd, &st) != 0) {
            detail::on_error_mapping(errno, "ankerl::unordered_dense::mapped_file: fstat");
        }
        if constexpr (sizeof(st.st_size) > sizeof(std::size_t)) {
            // a 32 bit process cannot map a file of 4 GB or more; truncating the size would map part of it
            if (static_cast<std::uint64_t>(st.st_size) > static_cast<std::uint64_t>(static_cast<std::size_t>(-1))) {
                detail::on_error_mapping(EOVERFLOW, "ankerl::unordered_dense::mapped_file: file too large");
            }
        }
        m_size = static_cast<std::size_t>(st.st_size);
        if (m_size == 0) {
            return;
        }
        if (how != mapping::huge_copy) {
            auto flags = MAP_SHARED;
#    if defined(MAP_POPULATE)
            if (how == mapping::file_populated) {
                flags |= MAP_POPULATE;
            }
#    endif
            auto* p = ::mmap(nullptr, m_size, PROT_READ, flags, d.fd, 0);
            if (p == MAP_FAILED) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
                detail::on_error_mapping(errno, "ankerl::unordered_dense::mapped_file: mmap");
            }
            m_region = static_cast<std::byte*>(p);
            // On hugetlbfs, munmap takes only whole huge pages; a file there always is (ftruncate
            // rejects any other size), so the file's size is the length on every file system.
            m_region_size = m_size;
            return;
        }
        // Below one huge page there is no huge page to get: map exactly the file's size.
        m_region_size = m_size < huge_page_size ? m_size : (m_size + huge_page_size - 1) / huge_page_size * huge_page_size;
        m_region = map_anonymous(m_region_size);
        read_into(d.fd);
        ::mprotect(m_region, m_region_size, PROT_READ);
    }

    mapped_file(mapped_file const&) = delete;
    auto operator=(mapped_file const&) -> mapped_file& = delete;

    mapped_file(mapped_file&& other) noexcept {
        swap(other);
    }

    auto operator=(mapped_file&& other) noexcept -> mapped_file& {
        mapped_file(std::move(other)).swap(*this);
        return *this;
    }

    void swap(mapped_file& other) noexcept {
        std::swap(m_region, other.m_region);
        std::swap(m_region_size, other.m_region_size);
        std::swap(m_size, other.m_size);
    }

    ~mapped_file() {
        release();
    }

    [[nodiscard]] auto data() const noexcept -> std::byte const* {
        return m_region;
    }
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return m_size;
    }
};

// Where the two arrays are in the file, in bytes from its start, and how many of each: what a
// map_view is constructed from. The library does not frame the file; the caller wrote it and says.
struct mapped_layout {
    std::size_t values_offset;
    std::size_t num_values;
    std::size_t index_offset;
    std::size_t num_blocks; // index().size() of the table that was written
};

// A mapped_file and a map_view or set_view over its bytes, held together so that the view cannot
// outlive the mapping: view() is a reference into this object, and there is none on a temporary.
// A copy of the view taken from it is valid only while this object lives. Moving it keeps the
// mapping where it is, so the view stays valid; the moved-from object holds nothing. It cannot be
// assigned, as a map_view cannot.
//
//     using view_t = ankerl::unordered_dense::map_view<std::uint64_t, std::uint64_t>;
//     auto m = ankerl::unordered_dense::mapped_view<view_t>("table.bin", layout, ankerl::unordered_dense::trust::checked);
//     auto it = m.view().find(key);
//
// What trust::checked checks holds for the bytes as they were when the view was constructed. The
// mapping is read only, but a `file` mapping shows what another process writes to the file
// afterwards, and that can make a lookup read out of bounds; truncating the file under the mapping
// raises SIGBUS on the next lookup that touches a page past the new end. A writer must write a new
// file and rename() it into place: this mapping keeps the old file's bytes, and the next one maps
// the new file. A `huge_copy` is a copy and sees neither.
//
// Not available without <sys/mman.h> (Windows): ANKERL_UNORDERED_DENSE_HAS_MAPPED_VIEW is 0 there
// and the header declares nothing.
template <class View>
class mapped_view {
    using value_type = typename View::value_type;
    using index_block = typename View::index_block;

    mapped_file m_file;
    View m_view;

    static auto make_view(mapped_file const& file,
                          mapped_layout const& layout,
                          trust t,
                          typename View::hasher const& hash,
                          typename View::key_equal const& equal) -> View {
        auto const size = file.size();
        auto const fits = [size](std::size_t offset, std::size_t count, std::size_t element_size) {
            return offset <= size && count <= (size - offset) / element_size;
        };
        if (!fits(layout.values_offset, layout.num_values, sizeof(value_type)) ||
            !fits(layout.index_offset, layout.num_blocks, sizeof(index_block))) {
            detail::on_error_layout();
        }
        // an empty array may sit at the end of the file, or the file may be empty: no pointer is formed then
        auto const* values = layout.num_values == 0 ? nullptr : file.data() + layout.values_offset;
        auto const* blocks = layout.num_blocks == 0 ? nullptr : file.data() + layout.index_offset;
        // The view constructor rejects an array that is not aligned for its type, and an index of
        // the wrong shape; the offsets decide the alignment, since the mapping starts on a page.
        return View(typename View::value_container_type(reinterpret_cast<value_type const*>(values), // NOLINT
                                                        layout.num_values),
                    typename View::index_view(reinterpret_cast<index_block const*>(blocks), layout.num_blocks), // NOLINT
                    t,
                    hash,
                    equal);
    }

public:
    using view_type = View;

    mapped_view(mapped_file file,
                mapped_layout const& layout,
                trust t,
                typename View::hasher const& hash = typename View::hasher(),
                typename View::key_equal const& equal = typename View::key_equal())
        : m_file(std::move(file))
        , m_view(make_view(m_file, layout, t, hash, equal)) {}

    mapped_view(char const* path, mapped_layout const& layout, trust t, mapping how = mapping::file)
        : mapped_view(mapped_file(path, how), layout, t) {}

    mapped_view(mapped_view const&) = delete;
    auto operator=(mapped_view const&) -> mapped_view& = delete;
    mapped_view(mapped_view&&) noexcept = default;
    auto operator=(mapped_view&&) -> mapped_view& = delete; // as a map_view: it is read only
    ~mapped_view() = default;

    [[nodiscard]] auto view() const& noexcept -> View const& {
        return m_view;
    }
    auto view() const&& -> View const& = delete; // would dangle once the temporary is gone

    [[nodiscard]] auto file() const noexcept -> mapped_file const& {
        return m_file;
    }
};

} // namespace ANKERL_UNORDERED_DENSE_NAMESPACE
} // namespace ankerl::unordered_dense

#endif // ANKERL_UNORDERED_DENSE_HAS_MAPPED_VIEW

#endif // ANKERL_UNORDERED_DENSE_MAPPED_VIEW_H
