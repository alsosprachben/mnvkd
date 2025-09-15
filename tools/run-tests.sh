#!/usr/bin/env bash
set -euo pipefail

# run-tests.sh — Safe test runner with timeout and capped output
#
# Usage examples:
#   tools/run-tests.sh                   # debug, target=test, 240s timeout, 400 lines
#   tools/run-tests.sh safe              # run a curated set of stable tests
#   tools/run-tests.sh --mode release --timeout 120 --lines 300 test
#   tools/run-tests.sh --target vk_test_exec.passed
#
# Env overrides:
#   TEST_TIMEOUT   default 240 (seconds)
#   MAX_LOG_LINES  default 400 (lines)
#   JOBS           default 1 (passed to make -j)

MODE=debug
TARGET="test"
TIMEOUT="${TEST_TIMEOUT:-240}"
MAX_LINES="${MAX_LOG_LINES:-400}"
JOBS="${JOBS:-1}"

die() { echo "[run-tests] $*" >&2; exit 1; }

cmd_exists() { command -v "$1" >/dev/null 2>&1; }

MAKE=make
if cmd_exists bmake; then MAKE=bmake; fi

print_usage() {
  cat <<USAGE
Usage: tools/run-tests.sh [options] [target]

Options:
  --mode [debug|release]   Build mode (default: debug)
  --timeout SECONDS        Timeout per invocation (default: ${TIMEOUT})
  --lines N                Max visible lines per invocation (default: ${MAX_LINES})
  --jobs N                 Parallel jobs for make (default: ${JOBS})
  --target NAME            Make target to run (default: ${TARGET})
  --list                   List curated safe targets

Targets:
  test     Run full test suite via Makefile (may hang in some cases)
  safe     Run curated, fast-passing test subset
  Any other Makefile target name is accepted as a direct target
USAGE
}

list_safe_targets() {
  cat <<SAFE
vk_test_signal.passed
vk_test_exec.passed
vk_test_mem.passed
vk_test_ft.passed
vk_test_ft2.passed
vk_test_ft3.passed
vk_test_err.passed
vk_test_err2.passed
vk_test_write.passed
vk_test_read.passed
vk_test_forward.passed
vk_test_pollread.passed
vk_test_isolate_thread.passed
SAFE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode) MODE="$2"; shift 2;;
    --timeout) TIMEOUT="$2"; shift 2;;
    --lines) MAX_LINES="$2"; shift 2;;
    --jobs) JOBS="$2"; shift 2;;
    --target) TARGET="$2"; shift 2;;
    --list) list_safe_targets; exit 0;;
    -h|--help) print_usage; exit 0;;
    safe|test) TARGET="$1"; shift;;
    *) TARGET="$1"; shift;;
  esac
done

run_make() {
  local target="$1"
  local mode="$2"

  local launcher=("./debug.sh")
  if [[ "$mode" == "release" ]]; then launcher=("./release.sh"); fi

  echo "[run-tests] mode=$mode timeout=${TIMEOUT}s lines=${MAX_LINES} jobs=${JOBS} target=$target"
  # Use stdbuf for line-buffering, and awk to cap visible output while still draining the pipe
  if timeout -k 10s "${TIMEOUT}"s stdbuf -oL -eL "${launcher[@]}" "$MAKE" -j"${JOBS}" "$target" 2>&1 \
      | awk -v max_lines="$MAX_LINES" '
          NR<=max_lines { print; next }
          NR==max_lines+1 { print "... [output truncated; continuing silently] ..." }
          { next }
        '
  then
    echo "[run-tests] RESULT: SUCCESS ($target)"
    return 0
  else
    ec=$?
    if [[ $ec -eq 124 ]]; then
      echo "[run-tests] RESULT: TIMEOUT (${TIMEOUT}s) ($target)" >&2
    else
      echo "[run-tests] RESULT: FAIL (exit=$ec) ($target)" >&2
    fi
    return $ec
  fi
}

case "$TARGET" in
  safe)
    rc=0
    while read -r t; do
      [[ -z "$t" ]] && continue
      run_make "$t" "$MODE" || rc=$?
    done < <(list_safe_targets)
    exit $rc
    ;;
  test|*)
    run_make "$TARGET" "$MODE"
    ;;
esac

