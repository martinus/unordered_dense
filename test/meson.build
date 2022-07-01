test_sources = [
    'app/Counter.cpp',
    'app/doctest.cpp',
    'app/nanobench.cpp',
    'bench/find_random.cpp',
    'bench/quick_overall_map.cpp',
    'unit/assign_to_move.cpp',
    'unit/assignment_combinations.cpp',
    'unit/at.cpp',
    'unit/contains.cpp',
    'unit/copy_and_assign_maps.cpp',
    'unit/copyassignment.cpp',
    'unit/count.cpp',
    'unit/ctors.cpp',
    'unit/diamond.cpp',
    'unit/empty.cpp',
    'unit/equal_range.cpp',
    'unit/erase_if.cpp',
    'unit/erase_range.cpp',
    'unit/erase.cpp',
    'unit/explicit.cpp',
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
    'unordered_dense_map test',
    test_exe,
    verbose: true)
