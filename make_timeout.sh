#!/usr/bin/env bash
set -euo pipefail

TIME_LIMIT_SECONDS=120
MAKE_TARGETS=()

usage() {
  echo "Usage: $0 [-t seconds] [make-target ...]" >&2
}

while getopts ":t:h" opt; do
  case "${opt}" in
    t)
      if [[ -z "${OPTARG}" ]] || [[ ! ${OPTARG} =~ ^[0-9]+$ ]]; then
        echo "Invalid timeout value: '${OPTARG}'" >&2
        usage
        exit 1
      fi
      if (( OPTARG <= 0 )); then
        echo "Timeout must be greater than zero: '${OPTARG}'" >&2
        usage
        exit 1
      fi
      TIME_LIMIT_SECONDS="${OPTARG}"
      ;;
    h)
      usage
      exit 0
      ;;
    ?)
      echo "Unknown option: -${OPTARG}" >&2
      usage
      exit 1
      ;;
  esac
done

shift $((OPTIND - 1))

if [[ $# -gt 0 ]]; then
  MAKE_TARGETS=("$@")
else
  MAKE_TARGETS=("clean_all" "test")
fi

COMMAND=("./debug.sh" "bmake" "${MAKE_TARGETS[@]}")

pgid=""
watchdog_pid=""

terminate_group() {
  if [[ -n "${pgid}" ]] && kill -0 "-${pgid}" 2>/dev/null; then
    kill -KILL "-${pgid}" 2>/dev/null || true
  fi
}

stop_watchdog() {
  if [[ -n "${watchdog_pid}" ]]; then
    kill "${watchdog_pid}" 2>/dev/null || true
    wait "${watchdog_pid}" 2>/dev/null || true
    watchdog_pid=""
  fi
}

on_signal() {
  terminate_group
  stop_watchdog
  exit 130
}

trap on_signal INT TERM

setsid "${COMMAND[@]}" &
cmd_pid=$!
pgid=${cmd_pid}

watchdog() {
  sleep "${TIME_LIMIT_SECONDS}"
  if kill -0 "-${pgid}" 2>/dev/null; then
    echo "Time limit of ${TIME_LIMIT_SECONDS}s exceeded. Killing process group ${pgid}." >&2
    kill -KILL "-${pgid}" 2>/dev/null || true
  fi
}

watchdog &
watchdog_pid=$!

set +e
wait "${cmd_pid}"
cmd_status=$?
set -e

stop_watchdog
trap - INT TERM

wait "${cmd_pid}" 2>/dev/null || true
exit "${cmd_status}"
