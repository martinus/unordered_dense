test_sources = [
    'app/counter.cpp',
    'app/doctest.cpp',
    'app/nanobench.cpp',
    'app/stacktrace.cpp',
    'app/ui/periodic.cpp',
    'app/ui/progress_bar.cpp',
    'app/unordered_dense.cpp',

    'bench/copy.cpp',
    'bench/find_random.cpp',
    'bench/game_of_life.cpp',
    'bench/precomputed_hash.cpp',
    'bench/quick_overall_map.cpp',
    'bench/replace_key.cpp',
    'bench/show_allocations.cpp',
    'bench/swap.cpp',

    'fuzz/run.cpp',

    'unit/array_of_maps.cpp',
    'unit/assign_to_move.cpp',
    'unit/assignment_combinations.cpp',
    'unit/assignment_exception_safety.cpp',
    'unit/at.cpp',
    'unit/avalanching.cpp',
    'unit/bucket.cpp',
    'unit/carried_state.cpp',
    'unit/contains.cpp',
    'unit/copy_and_assign_maps.cpp',
    'unit/copyassignment.cpp',
    'unit/count.cpp',
    'unit/ctors.cpp',
    'unit/custom_container_boost.cpp',
    'unit/custom_container.cpp',
    'unit/custom_hash.cpp',
    'unit/deduction_guides.cpp',
    'unit/diamond.cpp',
    'unit/empty.cpp',
    'unit/equal_range.cpp',
    'unit/erase_if.cpp',
    'unit/erase_range.cpp',
    'unit/erase_throwing_move.cpp',
    'unit/erase.cpp',
    'unit/explicit.cpp',
    'unit/extract.cpp',
    'unit/fuzz_api.cpp',
    'unit/fuzz_insert_erase.cpp',
    'unit/fuzz_replace_map.cpp',
    'unit/fuzz_string.cpp',
    'unit/growth_exception_safety.cpp',
    'unit/hash_scalar_types.cpp',
    'unit/hash_smart_ptr.cpp',
    'unit/hash_string_view.cpp',
    'unit/hash.cpp',
    'unit/hash_golden.cpp',
    'unit/include_only.cpp',
    'unit/initializer_list.cpp',
    'unit/insert_or_assign.cpp',
    'unit/insert.cpp',
    'unit/iterators_conversion.cpp',
    'unit/iterators_empty.cpp',
    'unit/iterators_erase.cpp',
    'unit/iterators_insert.cpp',
    'unit/lazy_bucket_allocation.cpp',
    'unit/load_factor.cpp',
    'unit/maps_of_maps.cpp',
    'unit/max.cpp',
    'unit/move_to_moved.cpp',
    'unit/multiple_apis.cpp',
    'unit/namespace.cpp',
    'unit/noexcept_contract.cpp',
    'unit/not_copyable.cpp',
    'unit/not_moveable.cpp',
    'unit/pmr_move_with_allocators.cpp',
    'unit/precomputed_hash.cpp',
    'unit/probe_wrap.cpp',
    'unit/pmr.cpp',
    'unit/reentrant.cpp',
    'unit/rehash.cpp',
    'unit/replace_key.cpp',
    'unit/replace.cpp',
    'unit/reserve_and_assign.cpp',
    'unit/reserve.cpp',
    'unit/segmented_vector.cpp',
    'unit/segmented_vector_allocators.cpp',
    'unit/set_or_map_types.cpp',
    'unit/set.cpp',
    'unit/std_hash.cpp',
    'unit/swap.cpp',
    'unit/table_allocators.cpp',
    'unit/transparent.cpp',
    'unit/try_emplace.cpp',
    'unit/tuple_hash.cpp',
    'unit/unique_ptr.cpp',
    'unit/unordered_set.cpp',
    'unit/vectorofmaps.cpp',
    'unit/windows_include.cpp',
]

# additional compile options
# see https://mesonbuild.com/Reference-tables.html
cpp_args = []
compiler = meson.get_compiler('cpp')
foreach arg : [
            # gcc
            '-Wno-stringop-overflow', # g++ error in fmtlib
            '-Warith-conversion',
            '-Wshadow=global',
            '-Wno-array-bounds', # gcc 13 gives incorrect warning

            # gcc / clang
            '-Wconversion',
            '-Wextra',
            '-Wunreachable-code',
            '-Wuninitialized',
            '-pedantic-errors',
            '-Wold-style-cast',
            '-Wno-unused-function',
            # '-Weffc++', doesn't work with fmt

            # clang 22 only, and only meaningful together with the DOCTEST_COUNTER define
            # below -- see the comment there for why one without the other does not work.
            '-Wno-c2y-extensions',

            # '-march=native',
        ]
    if compiler.has_argument(arg)
        cpp_args += [arg]
    endif
endforeach

# doctest names the function behind every TEST_CASE after a counter, and from 2.5.3 that counter
# is __LINE__ rather than __COUNTER__ on clang 22+, because C2y standardises __COUNTER__ and clang
# started warning about it as an extension. __LINE__ is only unique within one file, so two test
# cases that happen to sit on the same line collide as soon as a unity build puts their files in
# one translation unit -- which unit/insert.cpp and unit/insert_or_assign.cpp did, both opening
# with TEST_CASE_MAP on line 9. That is not a hypothetical: the unity CI leg builds this way, and
# so does every lane of scripts/mutate/mutate.py, where the failure would look like every mutant
# being rejected by the compiler.
#
# DOCTEST_COUNTER is doctest's own customisation point (it only picks a default `#if !defined`),
# so this takes __COUNTER__ back and the -Wno-c2y-extensions above silences what doctest was
# avoiding. Both are needed: the flag alone leaves doctest choosing __LINE__, and the define alone
# reintroduces the warning as an error. Neither has any effect on compilers that do not warn.
cpp_args += ['-DDOCTEST_COUNTER=__COUNTER__']

if compiler.get_id() == 'msvc'
    add_global_arguments(
        '/wd4189', # fmt: 'zero': local variable is initialized but not referenced, fixed in https://github.com/fmtlib/fmt/issues/2891
        '/wd4251', # 'fmt::v8::ostream::file_': class 'fmt::v8::file' needs to have dll-interface to be used by clients of class 'fmt::v8::ostream'
        language: 'cpp')
endif

# for include-what-you-use
#cpp_args += '-isystem'
#cpp_args += '/usr/lib64/clang/14.0.0/include/'

fmt_method = 'auto'
if get_option('cpp_args').contains('-m32')
    # use builtin so we can compile it for 32bit.
    # Can't use it as a default or sanitizer doesn't work...
    fmt_method = 'builtin'
endif

# use e.g.
# CXX='ccache clang++' BOOST_ROOT=/home/martinus/dev/boost_1_81_0/ meson setup --buildtype release -Dcpp_std=c++17 build
opt_boost = dependency('boost', required: false)
link_args = []
if opt_boost.found()
    add_global_arguments('-DANKERL_UNORDERED_DENSE_HAS_BOOST=1', language: 'cpp')
    link_args += ['-lrt']
else
    add_global_arguments('-DANKERL_UNORDERED_DENSE_HAS_BOOST=0', language: 'cpp')
endif

#opt_absl = dependency('absl_container', required: true, )
#if opt_boost.found()
#    add_global_arguments('-DANKERL_UNORDERED_DENSE_HAS_ABSL=1', language: 'cpp')
#else
#    add_global_arguments('-DANKERL_UNORDERED_DENSE_HAS_ABSL=0', language: 'cpp')
#endif

# One definition of the fmt dependency, because the fuzz targets need the same one and the two
# used to be able to drift apart. It has to come after the add_global_arguments calls above:
# resolving it declares the fmt subproject's target, and meson refuses a global argument once any
# target exists.
#
# warning_level and werror are this project's, and a subproject inherits them unless it is told
# otherwise -- so `werror=true` was being applied to fmt's own sources, and any warning a new fmt
# release happens to emit becomes this project's build failure. fmt 11.2.0 was clean under MSVC's
# /W4 /WX and 12.0.0 is not (format.h:2509, and only when targeting 32 bit), which is a fact about
# fmt rather than about anything here. Same reasoning as the system include for doctest above:
# -Werror is for code this project wrote. The commented-out absl line above already carried this.
fmt_dependency = dependency(
    'fmt',
    method: fmt_method,
    fallback: ['fmt', 'fmt_dep'],
    default_options: ['warning_level=0', 'werror=false'])

cpp_args += [
    #'-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION',
    #'-fsanitize-undefined-trap-on-error',
    #'-fsanitize=undefined,address',
    #'-fno-sanitize=thread',
    #'-ftrivial-auto-var-init=pattern',
    #'-g',
]

link_args += [
    #'-fsanitize=undefined,address',
    #'-fno-sanitize=thread'
    #'-Wl,-shuffle-sections' # for benchmarking with mold linker
]

test_exe = executable(
    'udm-test',
    test_sources,
    include_directories: incdir,
    cpp_args: cpp_args,
    link_args: link_args,
    dependencies: [
        dependency('threads'), # add dependency for threads (-lpthread, see https://mesonbuild.com/howtox.html),

        # see what's in the [provide] sections for the dependency names
        # As a system include, so that -Werror applies to this project's code and not to
        # doctest's. Note that this does not cover everything a header ships: a diagnostic about
        # what a macro expands to is reported against the line that *used* the macro, so it
        # counts as this project's code however the header was included. -Wc2y-extensions was
        # exactly that case, and is handled where cpp_args is built instead.
        dependency('doctest', include_type: 'system'),
        fmt_dependency,

        # disable these two if you don't want them
        #dependency('boost'),
        #dependency('absl_container', default_options: ['warning_level=0', 'werror=false'])
        # dependency('absl_hash', method: 'builtin', default_options: ['warning_level=0', 'werror=false'])
    ],
)

benchmark(
    'bench',
    test_exe,
    args: ['-ns', '-ts=bench'],
    verbose: true)

# Meson's default is 30 seconds, which a sanitizer build of this suite no longer fits into: address
# and undefined together, with detect_stack_use_after_return, cost it well over an order of
# magnitude, and a Valgrind run costs more again. The timeout is here to catch a hang, so it only
# has to be shorter than a CI job.
test(
    'unit',
    test_exe,
    timeout: 600,
    verbose: true)

# libFuzzer targets. The unit suite replays data/fuzz/<name> on every build, which by construction
# only finds what has already been found; these are the binaries that go looking. They are clang
# only (-fsanitize=fuzzer), and not built by default because nothing in a normal build or test run
# needs them -- ask for one by name:
#
#     ninja -C builddir test/fuzz_api
#     ./builddir/test/fuzz_api -max_total_time=60 corpus-scratch data/fuzz/fuzz_api
#
# or use scripts/fuzz_run.sh, which drives them across all cores.
if compiler.get_id() == 'clang'
    # -fsanitize=fuzzer is a link-time thing: it asks for the driver, the main() that feeds
    # LLVMFuzzerTestOneInput. The instrumentation it needs is a separate, compile-time request,
    # which is what fuzzer-no-link is for. Passing plain -fsanitize=fuzzer to the compile step as
    # well is what AFL++ reads as "add libAFLDriver.a to this command", and it duly adds an archive
    # to a -c command line, where -Werror turns clang's complaint about it into a failed build.
    #
    # Under afl-clang-fast++ the compile-time half has to go entirely. It instruments on its own,
    # and its runtime implements only the trace-pc-guard half of SanitizerCoverage; asking for
    # libFuzzer's instrumentation on top leaves references to __sanitizer_cov_8bit_counters_init,
    # __sanitizer_cov_pcs_init and __sancov_lowest_stack that nothing on the link line defines.
    # Whether it happens depends on the clang version -- AFL's own pass rewrites the coverage it
    # can, and what it leaves behind differs -- so this links on some machines and not others.
    # The link-time -fsanitize=fuzzer stays: AFL++ substitutes libAFLDriver.a for it.
    is_afl = compiler.get_define('__AFL_COMPILER') != ''

    fuzz_compile_sanitizers = is_afl ? [] : ['-fsanitize=fuzzer-no-link']
    fuzz_link_sanitizers = ['-fsanitize=fuzzer']
    if get_option('fuzz_sanitizers')
        fuzz_compile_sanitizers += '-fsanitize=address,undefined'
        fuzz_link_sanitizers += '-fsanitize=address,undefined'
    endif
    foreach fuzz_target : ['fuzz_api', 'fuzz_insert_erase', 'fuzz_replace_map', 'fuzz_string']
        executable(
            fuzz_target,
            ['unit/' + fuzz_target + '.cpp', 'app/counter.cpp'],
            include_directories: incdir,
            # FUZZ swaps the doctest test case for libFuzzer's entry point, see fuzz/run.h.
            # FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION is what turns REQUIRE into an abort the
            # fuzzer can see, see app/doctest.h.
            # DOCTEST_CONFIG_DISABLE keeps doctest's per-TU registration out of a binary that has
            # no doctest main to link against. The only assertion these targets use is REQUIRE,
            # which app/doctest.h turns into an abort() the fuzzer can see.
            cpp_args: cpp_args + [
                '-DFUZZ',
                '-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION',
                '-DDOCTEST_CONFIG_DISABLE',
                '-g',
                # Optimized whatever the buildtype says, because these targets exist to be run
                # millions of times. meson's default buildtype is debug, so a plain `meson setup`
                # gave -O0, where every one-line accessor in the header and in std::deque is a
                # real call: measured on fuzz_api, 28 executions per second against 136 at -O2.
                # Target arguments come after the buildtype's, so this wins. Debugging a crash
                # does not need this build -- the unit suite replays the corpus at -O0.
                '-O2',
            ] + fuzz_compile_sanitizers,
            link_args: link_args + fuzz_link_sanitizers,
            dependencies: [
                dependency('threads'),
                dependency('doctest', include_type: 'system'), # see above
                fmt_dependency,
            ],
            build_by_default: false)
    endforeach
endif

