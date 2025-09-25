#!/usr/bin/env bash
set -euo pipefail

TIME_LIMIT_SECONDS=120
COMMAND=("./debug.sh" "bmake" "test" "test" "servers")

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
