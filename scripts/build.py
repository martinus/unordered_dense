#!/bin/env python

import os
import subprocess

from numpy import putmask

cmd_and_dir = [
    ['env', 'CXX="clang++"', 'meson', 'setup', '--buildtype', 'release', '-Dcpp_std=c++17', 'builddir/clang_cpp17_release'],
    ['env', 'CXX="clang++"', 'meson', 'setup', '--buildtype', 'debug',   '-Dcpp_std=c++17', 'builddir/clang_cpp17_debug'],
    ['env', 'CXX="g++"',     'meson', 'setup', '--buildtype', 'release', '-Dcpp_std=c++17', 'builddir/gcc_cpp17_release'],
    ['env', 'CXX="g++"',     'meson', 'setup', '--buildtype', 'debug',   '-Dcpp_std=c++17', 'builddir/gcc_cpp17_debug'],

    # 32bit. Install lib32-clang
    ['env', 'CXX="g++"',     'meson', 'setup', '--buildtype', 'debug', '-Dcpp_std=c++17', '-Dcpp_args="-m32"', '-Dcpp_link_args="-m32"', '-Dc_args="-m32"', '-Dc_link_args="-m32"', 'builddir/gcc_cpp17_debug_32'],
    ['env', 'CXX="clang++"', 'meson', 'setup', '--buildtype', 'debug', '-Dcpp_std=c++17', '-Dcpp_args="-m32"', '-Dcpp_link_args="-m32"', '-Dc_args="-m32"', '-Dc_link_args="-m32"', 'builddir/clang_cpp17_debug_32'],

    # c++20
    ['env', 'CXX="clang++"', 'meson', 'setup', '--buildtype', 'debug', '-Dcpp_std=c++20', 'builddir/clang_cpp20_debug'],
    ['env', 'CXX="g++"',     'meson', 'setup', '--buildtype', 'debug', '-Dcpp_std=c++20', 'builddir/gcc_cpp20_debug'],

    # coverage; use "ninja clean && ninja test && ninja coverage"
    ['env', 'CXX="clang++"', 'meson', 'setup', '-Db_coverage=true', 'builddir/coverage'],

    # sanitizers
    # It is not possible to combine more than one of the -fsanitize=address, -fsanitize=thread, and -fsanitize=memory checkers in the same program.
    # see https://clang.llvm.org/docs/UsersManual.html#controlling-code-generation
    #
    # can't use ccache, it doesn't work with the ignorelist.txt
    ['env', 'CXX="clang++"', 'meson', 'setup', '-Db_sanitize=address',   'builddir/clang_sanitize_address'],
    ['env', 'CXX="clang++"', 'meson', 'setup', '-Db_sanitize=thread',    'builddir/clang_sanitize_thread'],
    # ['env', 'CXX="clang++" meson setup -Db_sanitize=memory',    'builddir/clang_sanitize_memory], # doesn't work due to STL, and ignore doesn't work either :-(
    ['env', 'CXX="clang++"', 'meson', 'setup', '-Db_sanitize=undefined', 'builddir/clang_sanitize_undefined'],
]

setup_cmd = ['meson', 'compile', '--clean', '-C']
test_cmd =  ['meson', 'test', '-q', '--print-errorlogs', '-C']


def run(cmd):
    subprocess.run(cmd)

# setup
for cmd_dir in cmd_and_dir:
    workdir = cmd_dir[-1]
    if not os.path.isdir(workdir):
        out = run(cmd_dir)
    
    cmd = setup_cmd.copy()
    cmd.append(workdir)
    run(cmd)

    cmd = test_cmd.copy()
    cmd.append(workdir)
    run(cmd)

    
