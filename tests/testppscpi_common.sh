#!/bin/bash

set -euo pipefail

PPSCPI_PID=""
READY_RESPONSE=""

stop_stale_ppscpi() {
  pkill -f "(^|/)ppscpi($| )" >/dev/null 2>&1 || true

  for _ in {1..20}; do
    if ! pgrep -f "(^|/)ppscpi($| )" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done

  echo "FAILURE: stale ppscpi still running" >&2
  return 1
}

cleanup_ppscpi() {
  local pid="${PPSCPI_PID}"
  PPSCPI_PID=""

  if [[ -n "${pid}" ]]; then
    printf 'TERMINATE\n' | nc localhost 5025 >/dev/null 2>&1 || true
    wait "${pid}" >/dev/null 2>&1 || true
  fi
}

trap cleanup_ppscpi EXIT

wait_for_ppscpi() {
  local response

  for _ in {1..40}; do
    if ! kill -0 "${PPSCPI_PID}" >/dev/null 2>&1; then
      wait "${PPSCPI_PID}" >/dev/null 2>&1 || true
      echo "FAILURE: ppscpi exited before becoming ready" >&2
      return 1
    fi

    response="$(printf '*IDN?\n' | nc localhost 5025 2>/dev/null || true)"
    if [[ "${response}" == *"PulsePins"* ]]; then
      READY_RESPONSE="${response}"
      return 0
    fi

    sleep 0.1
  done

  echo "FAILURE: ppscpi did not become ready" >&2
  return 1
}

start_ppscpi() {
  stop_stale_ppscpi
  ppscpi &
  PPSCPI_PID=$!
  wait_for_ppscpi
}

ppscpi_query() {
  printf '%s\n' "$1" | nc localhost 5025 2>/dev/null
}
