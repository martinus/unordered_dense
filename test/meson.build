test_sources = [
    'app/Counter.cpp',
    'app/doctest.cpp',
    'app/nanobench.cpp',
    'app/ui/Periodic.cpp',
    'app/ui/ProgressBar.cpp',
    'app/unordered_dense.cpp',
    'bench/copy.cpp',
    'bench/find_random.cpp',
    'bench/quick_overall_map.cpp',
    'bench/swap.cpp',
    'fuzz/api.cpp',
    'fuzz/insert_erase.cpp',
    'fuzz/string.cpp',
    'unit/assign_to_move.cpp',
    'unit/assignment_combinations.cpp',
    'unit/at.cpp',
    'unit/contains.cpp',
    'unit/copy_and_assign_maps.cpp',
    'unit/copyassignment.cpp',
    'unit/count.cpp',
    'unit/ctors.cpp',
    'unit/deduction_guides.cpp',
    'unit/diamond.cpp',
    'unit/empty.cpp',
    'unit/equal_range.cpp',
    'unit/erase_if.cpp',
    'unit/erase_range.cpp',
    'unit/erase.cpp',
    'unit/explicit.cpp',
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
    'unit/not_copyable.cpp',
    'unit/not_moveable.cpp',
    'unit/pmr.cpp',
    'unit/rehash.cpp',
    'unit/reserve_and_assign.cpp',
    'unit/set.cpp',
    'unit/swap.cpp',
    'unit/transparent.cpp',
    'unit/try_emplace.cpp',
    'unit/unique_ptr.cpp',
    'unit/unordered_set.cpp',
    'unit/vectorofmaps.cpp',
]

# additional compile options
cpp_args = ''
compiler = meson.get_compiler('cpp')
if compiler.has_argument('-Wno-stringop-overflow')
    cpp_args += '-Wno-stringop-overflow' # g++ error in fmtlib
endif
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

test_exe = executable(
    'udm',
    test_sources,
    include_directories: incdir,
    cpp_args: cpp_args,
    dependencies: [
        dependency('threads'), # add dependency for threads (-lpthread, see https://mesonbuild.com/howtox.html),

        # see what's in the [provide] sections for the dependency names
        dependency('doctest'),
        dependency('fmt', method: fmt_method),
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
        '-g',
        '-isystem', '/usr/lib64/clang/14.0.0/include/',
    ]
    fuzz_link_args = ['-fsanitize=undefined,address,fuzzer']
    fuzz_sources = [
        'app/Counter.cpp',
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
