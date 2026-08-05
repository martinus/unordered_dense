#!/bin/env bash
set -ev

# Start from a build directory, usually clang_cpp17_release
#   ../../scripts/fuzz_run.sh <testname>
#
# The target name is accepted either way, "api" or "fuzz_api". The binaries are not built by
# default, so this asks for the one it needs by name.
#
# Found a crash? Minimize it like so:
#   ./test/fuzz_replace_map -minimize_crash=1 ./crash-123abcdef

FUZZ_TARGET=${1#fuzz_}
SCRIPT_DIR=`dirname "$0"`
CORPUS_SMALL=${SCRIPT_DIR}/../data/fuzz/fuzz_${FUZZ_TARGET}
CORPUS_BIG=CORPUS_BIG/fuzz_${FUZZ_TARGET}
NUM_JOBS=$(nproc)

mkdir -p ${CORPUS_BIG}
ninja test/fuzz_${FUZZ_TARGET}
chrt -i 0 ./test/fuzz_${FUZZ_TARGET} -jobs=${NUM_JOBS} -workers=${NUM_JOBS} ${CORPUS_BIG} ${CORPUS_SMALL}
