test_sources = [
    'app/Counter.cpp',
    'app/doctest.cpp',
    'app/nanobench.cpp',
    'bench/find_random.cpp',
    'bench/quick_overall_map.cpp',
    'unit/assign_to_move.cpp',
    'unit/assignment_combinations.cpp',
    'unit/at.cpp',
    'unit/copy_and_assign_maps.cpp',
    'unit/copyassignment.cpp',
    'unit/count.cpp',
    'unit/diamond.cpp',
    'unit/empty.cpp',
    'unit/explicit.cpp',
    'unit/hash_char_types.cpp',
    'unit/hash_smart_ptr.cpp',
    'unit/hash_string_view.cpp',
    'unit/include_only.cpp',
    'unit/iterators_insert.cpp',
    'unit/initializer_list_insert.cpp',
    'unit/initializerlist.cpp',
    'unit/insert_or_assign.cpp',
    'unit/insert.cpp',
    'unit/iterators_empty.cpp',
    'unit/iterators_erase.cpp',
    'unit/move_to_moved.cpp',
    'unit/swap.cpp',
]

# additional compile options
cpp_args = ''
compiler = meson.get_compiler('cpp')
if compiler.has_argument('-Wno-stringop-overflow')
    cpp_args += '-Wno-stringop-overflow' # g++ error in fmtlib
endif

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
        dependency('boost'),
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
    'unordered_dense_map test',
    test_exe,
    verbose: true)

