#!/usr/bin/env bash
#
# Fuzz with AFL++ across every core, then fold what it found back into data/fuzz as a minimal set.
#
#   scripts/fuzz_afl.sh run                     # all four targets, one core each, until Ctrl-C
#   scripts/fuzz_afl.sh run fuzz_api            # one target on every core, until Ctrl-C
#   scripts/fuzz_afl.sh minimize                # fold the findings in and shrink, then git diff
#
# Stopping is safe at any point: afl-fuzz writes every input it keeps to disk as it finds it, so
# Ctrl-C loses nothing. Running "run" again resumes from where the last one stopped.
#
# Needs AFL++ (apt install afl++ / brew install afl++) and clang. The builds are created on demand.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

ALL_TARGETS=(fuzz_api fuzz_insert_erase fuzz_replace_map fuzz_string)
FINDINGS=${FINDINGS:-$ROOT/fuzz-findings} # gitignored, see .gitignore
BUILD_AFL=builddir/afl
BUILD_AFL_FAST=builddir/afl-fast
BUILD_LIBFUZZER=builddir/fuzz

die() {
    echo "error: $*" >&2
    exit 1
}

cores() {
    nproc 2>/dev/null || sysctl -n hw.ncpu
}

# Both builds of the same target: afl-clang-fast++ for afl-fuzz and afl-cmin, clang++ for the
# -merge=1 pass. AFL++ takes -fsanitize=fuzzer and links its own driver over the same entry point,
# so this needs nothing special from test/meson.build. Ask for targets by name: a bare ninja would
# also build udm-test, which does not link under AFL++ (it defines
# FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION, which fuzz/run.h reads as "honggfuzz is driving").
ensure_built() {
    local builddir=$1 compiler=$2 setup_args=$3
    shift 3

    # build.ninja rather than the directory: a meson setup that failed, or one interrupted part way
    # through, leaves the directory behind, and testing for the directory then treats it as ready
    # and hands ninja something it cannot build.
    if [[ ! -f $builddir/build.ninja ]]; then
        rm -rf "$builddir"
        local log
        log=$(mktemp)
        # shellcheck disable=SC2086
        if ! CXX=$compiler meson setup "$builddir" --force-fallback-for=fmt $setup_args >"$log" 2>&1; then
            cat "$log" >&2
            rm -f "$log"
            die "meson setup failed for $builddir with CXX=$compiler${setup_args:+ $setup_args}"
        fi
        rm -f "$log"
    fi

    local t targets=()
    for t in "$@"; do
        targets+=("test/$t")
    done
    # Not silenced: when a build fails this is the only thing that says why, and when it succeeds
    # it is a handful of progress lines.
    if ! ninja -C "$builddir" "${targets[@]}"; then
        die "could not build ${targets[*]} in $builddir"
    fi
}

cmd_run() {
    command -v afl-fuzz >/dev/null || die "afl-fuzz not found. Install AFL++ first."

    # afl-fuzz refuses to start when the kernel would hand crashes to a crash reporter instead of
    # to it, and it is right to: crashes would be missed or blamed on the wrong input.
    if [[ -r /proc/sys/kernel/core_pattern ]] && [[ $(cat /proc/sys/kernel/core_pattern) == \|* ]]; then
        die "core_pattern pipes to a crash handler. Fix it with:
    sudo sh -c 'echo core > /proc/sys/kernel/core_pattern'"
    fi

    local targets=("$@")
    [[ ${#targets[@]} -gt 0 ]] || targets=("${ALL_TARGETS[@]}")
    ensure_built "$BUILD_AFL" afl-clang-fast++ "" "${targets[@]}"
    ensure_built "$BUILD_AFL_FAST" afl-clang-fast++ "-Dfuzz_sanitizers=false" "${targets[@]}"

    local cores
    cores=$(cores)
    # One target: every core on it. Several: split the cores between them, at least one each.
    local per_target=$((cores / ${#targets[@]}))
    [[ $per_target -ge 1 ]] || per_target=1

    # The first target's main instance runs in the foreground and keeps the terminal, so there is
    # a live status screen to watch rather than four silent log files. Everything else runs in the
    # background with the UI off, because AFL's screen only makes sense one at a time.
    local watched=${targets[0]}

    # With more than one instance the foreground one runs without sanitizers, because it is the
    # number you watch and the one doing the deterministic passes; a secondary carries them. With
    # only one instance there is nothing to trade, so it keeps them.
    local sanitized_instance=1 watched_build=$BUILD_AFL_FAST
    if [[ $per_target -eq 1 ]]; then
        sanitized_instance=0
        watched_build=$BUILD_AFL
    fi

    echo "fuzzing ${targets[*]} on $cores cores ($per_target per target), Ctrl-C to stop"
    echo "showing $watched, the rest log to $FINDINGS/<target>/afl-*.log"
    echo "sanitizers on instance s$sanitized_instance of each target, off elsewhere for speed"
    echo

    local pids=() logs=() stats=()
    # shellcheck disable=SC2317
    stop() {
        trap - INT TERM
        kill "${pids[@]}" 2>/dev/null || true
        wait 2>/dev/null || true
        echo
        echo "stopped. Fold the findings in with: scripts/fuzz_afl.sh minimize ${targets[*]}"
        exit 0
    }
    trap stop INT TERM

    local t i name role
    for t in "${targets[@]}"; do
        mkdir -p "$FINDINGS/$t"
        for ((i = 0; i < per_target; ++i)); do
            # One main instance per target does the deterministic passes, the rest explore at
            # random and share whatever any of them finds through the output directory.
            if [[ $i -eq 0 ]]; then name=main role=(-M "main"); else name="s$i" role=(-S "s$i"); fi
            # this one is started last, in the foreground
            if [[ $t == "$watched" && $i -eq 0 ]]; then
                continue
            fi
            # The sanitizers cost around 7x here, so exactly one instance per target carries them
            # and the others cover ground. A crash any of them finds is saved to the same place;
            # replay it under $BUILD_AFL/test/<target> to get the ASan report.
            local build=$BUILD_AFL_FAST
            if [[ $i -eq $sanitized_instance ]]; then build=$BUILD_AFL; fi
            AFL_SKIP_CPUFREQ=1 AFL_AUTORESUME=1 AFL_NO_UI=1 \
                afl-fuzz "${role[@]}" -i "data/fuzz/$t" -o "$FINDINGS/$t" \
                -- "./$build/test/$t" >"$FINDINGS/$t/afl-$i.log" 2>&1 &
            pids+=($!)
            logs+=("$FINDINGS/$t/afl-$i.log")
            stats+=("$FINDINGS/$t/$name/fuzzer_stats")
        done
    done

    # afl-fuzz fails at startup for ordinary reasons: a target the instrumentation did not take
    # to, an unreadable corpus, a core_pattern it will not run under. These instances have their
    # UI off and their output in a log file nobody is looking at, so without this the run looks
    # healthy -- the foreground screen is healthy -- while most of the cores sit idle.
    #
    # Waiting a fixed couple of seconds and looking for corpses is not enough; measured on
    # fuzz_api, an abort takes about as long to happen (3s) as a good instance takes to start
    # fuzzing. So wait for one of the two to be true of every instance instead: fuzzer_stats
    # exists, meaning it calibrated the corpus and is running, or the process is gone. The
    # deadline is only there so a target that calibrates very slowly cannot hang the script.
    local deadline=$((SECONDS + 60)) waiting=1
    while [[ $waiting -eq 1 && $SECONDS -lt $deadline ]]; do
        waiting=0
        for i in "${!pids[@]}"; do
            if kill -0 "${pids[$i]}" 2>/dev/null && [[ ! -f ${stats[$i]} ]]; then
                waiting=1
            fi
        done
        [[ $waiting -eq 0 ]] || sleep 0.5
    done

    local dead=()
    for i in "${!pids[@]}"; do
        kill -0 "${pids[$i]}" 2>/dev/null || dead+=("${logs[$i]}")
    done
    if [[ ${#dead[@]} -gt 0 ]]; then
        local log
        for log in "${dead[@]}"; do
            echo "--- $log" >&2
            tail -n 20 "$log" >&2
        done
        kill "${pids[@]}" 2>/dev/null || true
        die "${#dead[@]} of ${#pids[@]} background instances exited at startup, see above"
    fi

    # No AFL_NO_UI and no redirect: this is the one with the screen. The status is caught rather
    # than left to set -e, which would end the run before the background instances were stopped.
    # Reaching the check at all means afl-fuzz exited on its own: a Ctrl-C goes to the whole
    # process group, and bash runs the trap -- which stops everything and exits -- as soon as the
    # foreground child is reaped, so that path never gets here.
    local status=0
    AFL_SKIP_CPUFREQ=1 AFL_AUTORESUME=1 \
        afl-fuzz -M "main" -i "data/fuzz/$watched" -o "$FINDINGS/$watched" \
        -- "./$watched_build/test/$watched" || status=$?
    if [[ $status -ne 0 ]]; then
        kill "${pids[@]}" 2>/dev/null || true
        die "afl-fuzz on $watched exited with $status"
    fi
    stop
}

cmd_minimize() {
    local targets=("$@")
    [[ ${#targets[@]} -gt 0 ]] || targets=("${ALL_TARGETS[@]}")
    ensure_built "$BUILD_AFL" afl-clang-fast++ "" "${targets[@]}"
    ensure_built "$BUILD_LIBFUZZER" clang++ "" "${targets[@]}"

    local t
    for t in "${targets[@]}"; do
        local work="$FINDINGS/.minimize/$t"
        rm -rf "$work"
        mkdir -p "$work/all"

        # Everything that exists: what is committed, plus every queue any instance produced,
        # staged under the sha1 of its contents.
        #
        # Naming by content is what the final corpus uses anyway, and here it also collapses
        # duplicates for free. Instances copy inputs from each other whenever they sync, so a good
        # input ends up sitting in several queues and afl-cmin would otherwise execute every copy:
        # measured on one four-instance run, 7960 queue files but only 5047 distinct ones.
        #
        # It is worth doing this without forking per file. Naming each file through basename,
        # dirname and tr, then copying it, is six processes per file and around 80 seconds per
        # 8000 inputs; one sha1sum over all of them and one hardlink each, made in parallel, is
        # three. Hardlinks because these files are only ever read, and the fallback is for a
        # $FINDINGS on a different filesystem, where linking cannot work.
        local n_found pairs="$work/pairs"
        n_found=$(find "$FINDINGS/$t" -path '*/queue/id:*' -type f 2>/dev/null | wc -l | tr -d ' ')
        {
            find "data/fuzz/$t" -type f -print0
            find "$FINDINGS/$t" -path '*/queue/id:*' -type f -print0 2>/dev/null
        } | xargs -0 -r sha1sum |
            awk -v d="$work/all" '{h = $1; sub(/^[0-9a-f]+  /, ""); print $0 "\n" d "/" h}' |
            tr '\n' '\0' >"$pairs"
        xargs -0 -r -P "$(cores)" -n 2 ln -f <"$pairs" 2>/dev/null ||
            xargs -0 -r -P "$(cores)" -n 2 cp <"$pairs" ||
            die "could not stage the inputs for $t"

        local before
        before=$(find "data/fuzz/$t" -type f | wc -l | tr -d ' ')

        # Two passes, because neither minimizer subsumes the other. afl-cmin keeps a set covering
        # the same AFL edges and is far more aggressive; -merge=1 onto its output adds back the
        # files carrying a libFuzzer feature it could not see. The @@ matters: afl-cmin pipes stdin
        # by default, and this driver reports no coverage for that. -T runs the executions across
        # every core -- only the trace collection parallelises and the solve stays single threaded,
        # so it is not a speedup by the core count: 39s to 30s over 5520 inputs on four cores.
        AFL_SKIP_CPUFREQ=1 afl-cmin -T all -i "$work/all" -o "$work/cmin" \
            -- "./$BUILD_AFL/test/$t" @@ >"$work/cmin.log" 2>&1 ||
            die "afl-cmin failed, see $work/cmin.log"
        cp -r "$work/cmin" "$work/min"
        "./$BUILD_LIBFUZZER/test/$t" -merge=1 "$work/min" "$work/all" >"$work/merge.log" 2>&1 ||
            die "merge failed, see $work/merge.log"

        # Name every file by the sha1 of its contents, which is what -merge=1 does and what the
        # nightly workflow uploads, so a file that was already committed keeps its name.
        rm -rf "data/fuzz/$t"
        mkdir -p "data/fuzz/$t"
        for f in "$work/min/"*; do
            [[ -f $f ]] || continue
            cp "$f" "data/fuzz/$t/$(sha1sum "$f" | cut -d' ' -f1)"
        done

        local after
        after=$(find "data/fuzz/$t" -type f | wc -l | tr -d ' ')
        printf '%-20s %5s files -> %-5s  (%s from fuzzing went in)\n' "$t" "$before" "$after" "$n_found"
        "./$BUILD_LIBFUZZER/test/$t" -runs=0 "data/fuzz/$t" 2>&1 | grep -oE 'INITED cov: [0-9]+ ft: [0-9]+' |
            sed 's/^/                     /'
    done

    echo
    echo "crashes, if any: find $FINDINGS -path '*/crashes/id:*'"
    echo "now check the suite still passes, then commit:"
    echo "    meson test -C $BUILD_LIBFUZZER"
    echo "    git add data/fuzz && git commit"
}

case "${1:-}" in
run)
    shift
    cmd_run "$@"
    ;;
minimize)
    shift
    cmd_minimize "$@"
    ;;
*)
    grep '^#' "${BASH_SOURCE[0]}" | sed -n '2,12p' | cut -c3-
    exit 1
    ;;
esac
