#!/bin/env bash
set -ev

# Start from a build directory, usually clang_cpp17_release
# usage: fuzz_run.sh <testname>
FUZZ_TARGET=$1
SCRIPT_DIR=`dirname "$0"`
CORPUS_SMALL=${SCRIPT_DIR}/../data/fuzz/${FUZZ_TARGET}
CORPUS_BIG=CORPUS_BIG/${FUZZ_TARGET}
NUM_JOBS=$(nproc)

mkdir -p ${CORPUS_BIG}
ninja
chrt -i 0 ./test/fuzz_${FUZZ_TARGET} -jobs=${NUM_JOBS} -workers=${NUM_JOBS} ${CORPUS_BIG} ${CORPUS_SMALL}
