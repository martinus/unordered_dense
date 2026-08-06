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
    'bench/quick_overall_map.cpp',
    'bench/replace_key.cpp',
    'bench/show_allocations.cpp',
    'bench/swap.cpp',

    'fuzz/run.cpp',

    'unit/array_of_maps.cpp',
    'unit/assign_to_move.cpp',
    'unit/assignment_combinations.cpp',
    'unit/at.cpp',
    'unit/bucket.cpp',
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
    'unit/hash_char_types.cpp',
    'unit/hash_smart_ptr.cpp',
    'unit/hash_string_view.cpp',
    'unit/hash.cpp',
    'unit/include_only.cpp',
    'unit/initializer_list.cpp',
    'unit/insert_or_assign.cpp',
    'unit/insert.cpp',
    'unit/lazy_bucket_allocation.cpp',
    'unit/iterators_empty.cpp',
    'unit/iterators_erase.cpp',
    'unit/iterators_insert.cpp',
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
            
            # '-march=native',
        ]
    if compiler.has_argument(arg)
        cpp_args += [arg]
    endif
endforeach

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
        # doctest's. Its DOCTEST_ANONYMOUS uses __COUNTER__, which C2y standardises and clang 22
        # therefore warns about under -Wc2y-extensions, and the diagnostic is reported against
        # whichever test file used INFO() or REQUIRE(). Warnings in code this project actually
        # wrote are unaffected, including expressions handed to those macros: clang suppresses
        # what a system macro expands to, not what is passed into it.
        dependency('doctest', include_type: 'system'),
        dependency('fmt', method: fmt_method, fallback: ['fmt', 'fmt_dep']),

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

test(
    'unit',
    test_exe,
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
                dependency('fmt', method: fmt_method, fallback: ['fmt', 'fmt_dep']),
            ],
            build_by_default: false)
    endforeach
endif

