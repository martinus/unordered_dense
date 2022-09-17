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
    'bench/quick_overall_map.cpp',
    'bench/swap.cpp',

    'fuzz/api.cpp',
    'fuzz/insert_erase.cpp',
    'fuzz/replace.cpp',
    'fuzz/string.cpp',

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
    'unit/erase.cpp',
    'unit/explicit.cpp',
    'unit/extract.cpp',
    'unit/fuzz_corpus.cpp',
    'unit/hash_char_types.cpp',
    'unit/hash_smart_ptr.cpp',
    'unit/hash_string_view.cpp',
    'unit/hash.cpp',
    'unit/include_only.cpp',
    'unit/initializer_list.cpp',
    'unit/insert_or_assign.cpp',
    'unit/insert.cpp',
    'unit/iterators_empty.cpp',
    'unit/iterators_erase.cpp',
    'unit/iterators_insert.cpp',
    'unit/load_factor.cpp',
    'unit/maps_of_maps.cpp',
    'unit/max.cpp',
    'unit/move_to_moved.cpp',
    'unit/multiple_apis.cpp',
    'unit/namespace.cpp',
    'unit/not_copyable.cpp',
    'unit/not_moveable.cpp',
    'unit/pmr.cpp',
    'unit/rehash.cpp',
    'unit/replace.cpp',
    'unit/reserve_and_assign.cpp',
    'unit/reserve.cpp',
    'unit/set.cpp',
    'unit/std_hash.cpp',
    'unit/swap.cpp',
    'unit/transparent.cpp',
    'unit/try_emplace.cpp',
    'unit/unique_ptr.cpp',
    'unit/unordered_set.cpp',
    'unit/vectorofmaps.cpp',
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

            # gcc / clang
            '-Wconversion',
            '-Wextra',
            '-Wunreachable-code',
            '-Wuninitialized',
            '-pedantic-errors',
            '-Wold-style-cast',
            # '-Weffc++', doesn't work with fmt
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

opt_boost = dependency('boost', required: false)
link_args = []
if opt_boost.found()
    add_global_arguments('-DANKERL_UNORDERED_DENSE_HAS_BOOST=1', language: 'cpp')
    link_args += ['-lrt']
else
    add_global_arguments('-DANKERL_UNORDERED_DENSE_HAS_BOOST=0', language: 'cpp')
endif

test_exe = executable(
    'udm',
    test_sources,
    include_directories: incdir,
    cpp_args: cpp_args,
    link_args: link_args,
    dependencies: [
        dependency('threads'), # add dependency for threads (-lpthread, see https://mesonbuild.com/howtox.html),

        # see what's in the [provide] sections for the dependency names
        dependency('doctest'),
        dependency('fmt', method: fmt_method),
        opt_boost, # boost might not be found
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

test(
    'fuzz',
    test_exe,
    args: ['-ns', '-ts=fuzz'],
    env: 'FUZZ_CORPUS_BASE_DIR=' + meson.source_root() / 'data' / 'fuzz',
    verbose: true)

###################

if compiler.get_id() == 'clang'
    fuzz_deps = [
            dependency('doctest'),
            dependency('threads'),
            dependency('fmt', method: fmt_method),
        ]
    fuzz_cpp_args = [
        '-DFUZZ',
        '-fsanitize-undefined-trap-on-error',
        '-fsanitize=undefined,address,fuzzer',
        '-fno-sanitize=thread',
        '-g',
        '-isystem', '/usr/lib64/clang/14.0.0/include/',
    ]
    fuzz_link_args = [
        '-fsanitize=undefined,address,fuzzer',
        '-fno-sanitize=thread',
    ]
    fuzz_sources = [
        'app/counter.cpp',
    ]

    executable(
        'fuzz_api',
        fuzz_sources + ['fuzz/api.cpp'],
        include_directories: incdir,
        cpp_args : fuzz_cpp_args,
        link_args: fuzz_link_args,
        dependencies: fuzz_deps,
    )
    executable(
        'fuzz_replace',
        fuzz_sources + ['fuzz/replace.cpp'],
        include_directories: incdir,
        cpp_args : fuzz_cpp_args,
        link_args: fuzz_link_args,
        dependencies: fuzz_deps,
    )    
    executable(
        'fuzz_insert_erase',
        fuzz_sources + ['fuzz/insert_erase.cpp'],
        include_directories: incdir,
        cpp_args : fuzz_cpp_args,
        link_args: fuzz_link_args,
        dependencies: fuzz_deps,
    )
    executable(
        'fuzz_string',
        fuzz_sources + ['fuzz/string.cpp'],
        include_directories: incdir,
        cpp_args : fuzz_cpp_args,
        link_args: fuzz_link_args,
        dependencies: fuzz_deps,
    )
endif
