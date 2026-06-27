#!/usr/bin/env bash
# Manual live-board smoke runner for the current build artifacts.
#
# What it does:
# 1. checks that the current `pulsepins.rbf`, `pptool`, `ppscpi`, and `ppwebgui` exist locally
# 2. copies them to the target board and reloads the FPGA
# 3. runs a small finite `pptool` smoke sequence on the board
# 4. runs a real TCP smoke exchange against `ppscpi`, including basic error-queue checks
# 5. runs a small HTTP smoke exchange against `ppwebgui`, including `400` and `504` checks
#
# Typical usage:
#   ./scripts/board_smoke.sh
#   ./scripts/board_smoke.sh --verbose
#   ./scripts/board_smoke.sh my-board-host
#   TARGETHOST=my-board-host make board-smoke

set -euo pipefail

TARGETHOST="${TARGETHOST:-de10nano}"
VERBOSE=0

usage() {
  cat <<'EOF'
Usage: board_smoke.sh [--verbose] [targethost]

Runs a manual live-board smoke pass against the current build artifacts.

Arguments:
  --verbose   print full step logs after each successful step
  targethost  SSH target host (default: TARGETHOST env or de10nano)
EOF
}

while (($#)); do
  case "$1" in
    --verbose|-v)
      VERBOSE=1
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    -*)
      printf 'Unknown option: %s\n' "$1" >&2
      usage >&2
      exit 1
      ;;
    *)
      TARGETHOST="$1"
      ;;
  esac
  shift
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

RBF_PATH="${ROOT_DIR}/pulsepins.rbf"
PPTOOL_PATH="${ROOT_DIR}/c++/pptool"
PPSCPI_PATH="${ROOT_DIR}/c++/ppscpi"
PPWEBGUI_PATH="${ROOT_DIR}/c++/ppwebgui"

# `pptool` symlink front-ends expected by the on-board CLI smoke checks.
SYMLINKS=(
  pptest ppmstest ppdmatest ppfg ppreset pptrig ppdelay ppqout ppaux
  ppcounter ppts ppgpsdo pptemp ppfreq ppread ppplay ppvcd pphelloworld
)

LOG_DIR="$(mktemp -d -t pulsepins-board-smoke-XXXXXX)"
PPSCPI_SSH_PID=""
PPWEBGUI_SSH_PID=""
STEP_LOG_FILE=""
BOARD_IP=""
PPTOOL_VERSION_LINE=""
PPSCPI_VERSION_LINE=""
PPWEBGUI_VERSION_LINE=""
BOARD_BITSTREAM_LINE=""
declare -a PASSED_STEPS=()

section() {
  printf '\n== %s ==\n' "$1"
}

pass_step() {
  local label="$1"
  printf 'PASS %s\n' "$label"
  PASSED_STEPS+=("$label")
}

print_log_tail() {
  local log_file="$1"
  if [[ -f "${log_file}" ]]; then
    printf '  log: %s\n' "${log_file}" >&2
    printf '  last lines:\n' >&2
    tail -n 20 "${log_file}" | sed 's/^/    /' >&2
  fi
}

fail_step() {
  local label="$1"
  local log_file="$2"
  printf 'FAIL %s\n' "$label" >&2
  print_log_tail "$log_file"
  printf '\n== Summary ==\n' >&2
  printf 'Target host: %s\n' "$TARGETHOST" >&2
  if [[ -n "${BOARD_IP}" ]]; then
    printf 'Board IP: %s\n' "$BOARD_IP" >&2
  fi
  printf 'Logs: %s\n' "$LOG_DIR" >&2
  exit 1
}

show_verbose_log() {
  local log_file="$1"
  printf '  log: %s\n' "$log_file"
  sed 's/^/    /' "$log_file"
}

run_step() {
  local label="$1"
  local log_name="$2"
  local log_file="${LOG_DIR}/${log_name}.log"
  shift 2

  printf 'RUN  %s\n' "$label"
  STEP_LOG_FILE="$log_file"

  set +e
  "$@" >"${log_file}" 2>&1
  local rc=$?
  set -e

  if (( rc != 0 )); then
    fail_step "$label" "$log_file"
  fi

  if (( VERBOSE )); then
    show_verbose_log "$log_file"
  fi

  pass_step "$label"
}

require_file() {
  local file_path="$1"
  if [[ ! -f "${file_path}" ]]; then
    printf 'Missing required file: %s\n' "${file_path}" >&2
    exit 1
  fi
}

require_cmd() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    printf 'Missing required command: %s\n' "${cmd}" >&2
    exit 1
  fi
}

capture_common_metadata_from_log() {
  local log_file="$1"
  if [[ -z "${BOARD_BITSTREAM_LINE}" ]]; then
    BOARD_BITSTREAM_LINE="$(grep -m1 '^Bitstream timestamp:' "$log_file" | tr -d '\r' || true)"
  fi
}

capture_binary_version_from_log() {
  local tool_name="$1"
  local log_file="$2"
  local version_line
  version_line="$(grep -m1 '^Version .*commit ' "$log_file" | tr -d '\r' || true)"
  case "$tool_name" in
    pptool)
      if [[ -z "${PPTOOL_VERSION_LINE}" ]]; then
        PPTOOL_VERSION_LINE="$version_line"
      fi
      ;;
    ppscpi)
      if [[ -z "${PPSCPI_VERSION_LINE}" ]]; then
        PPSCPI_VERSION_LINE="$version_line"
      fi
      ;;
    ppwebgui)
      if [[ -z "${PPWEBGUI_VERSION_LINE}" ]]; then
        PPWEBGUI_VERSION_LINE="$version_line"
      fi
      ;;
  esac
}

cleanup() {
  # If a background service smoke is still running when the script exits, stop the
  # local SSH tunnel process so the remote server goes away with it.
  if [[ -n "${PPWEBGUI_SSH_PID}" ]]; then
    kill "${PPWEBGUI_SSH_PID}" >/dev/null 2>&1 || true
    wait "${PPWEBGUI_SSH_PID}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${PPSCPI_SSH_PID}" ]]; then
    kill "${PPSCPI_SSH_PID}" >/dev/null 2>&1 || true
    wait "${PPSCPI_SSH_PID}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

# Pre-flight: fail fast if the expected local artifacts or host-side tools are missing.
for path in "${RBF_PATH}" "${PPTOOL_PATH}" "${PPSCPI_PATH}" "${PPWEBGUI_PATH}"; do
  require_file "${path}"
done
for cmd in ssh scp python3 grep tail sed; do
  require_cmd "${cmd}"
done

deploy_artifacts() {
  scp "${RBF_PATH}" "${TARGETHOST}:pulsepins.rbf.new"
  scp "${PPTOOL_PATH}" "${TARGETHOST}:pptool.new"
  scp "${PPSCPI_PATH}" "${TARGETHOST}:ppscpi.new"
  scp "${PPWEBGUI_PATH}" "${TARGETHOST}:ppwebgui.new"

  local remote_symlink_loop='for L in'
  local link
  for link in "${SYMLINKS[@]}"; do
    remote_symlink_loop+=" ${link}"
  done
  remote_symlink_loop+='; do ln -sf pptool "$L"; done'

  ssh "${TARGETHOST}" "mv -f pulsepins.rbf.new pulsepins.rbf && mv -f pptool.new pptool && mv -f ppscpi.new ppscpi && mv -f ppwebgui.new ppwebgui && ${remote_symlink_loop} && FPGA-writeConfig -f pulsepins.rbf"
}

discover_board_ip() {
  BOARD_IP="$(ssh "${TARGETHOST}" "ip -4 addr show eth0 | awk '/inet /{print \$2}' | cut -d/ -f1")"
  if [[ -z "${BOARD_IP}" ]]; then
    printf 'Failed to determine board IP for %s\n' "${TARGETHOST}" >&2
    return 1
  fi
  export BOARD_IP
  printf 'Board IP: %s\n' "$BOARD_IP"
}

# Finite command-line smokes cover the common reset, stream, readback, and DMA flows.
smoke_ppcounter() {
  ssh "${TARGETHOST}" './ppcounter -test1 -check'
}

smoke_ppdmatest21() {
  ssh "${TARGETHOST}" './ppdmatest 21 -c 10 -v 16'
}

smoke_ppread_timeout() {
  ssh "${TARGETHOST}" './ppread -timeout 1'
  grep -q 'Caught ReadbackException: Timeout waiting for more readback data.' "${STEP_LOG_FILE}"
}

smoke_ppdmatest22() {
  ssh "${TARGETHOST}" './ppdmatest 22 -c 10 -v 16 -reps 4'
}

# Network smoke for the SCPI server: start it remotely, exercise both normal commands
# and one expected SCPI error path, then terminate it cleanly.
smoke_ppscpi() {
  ssh "${TARGETHOST}" 'pkill -f "(^|/)ppscpi($| )" >/dev/null 2>&1 || true'
  ssh -o ServerAliveInterval=2 -o ServerAliveCountMax=2 "${TARGETHOST}" 'exec ./ppscpi' &
  PPSCPI_SSH_PID=$!

  python3 - <<'PY'
import os
import socket
import time

host = os.environ["BOARD_IP"]
port = 5025

def connect_ready_socket():
    for _ in range(40):
        try:
            sock = socket.create_connection((host, port), timeout=1)
            sock.settimeout(5)
            sock.sendall(b"*IDN?\n")
            data = b""
            while not data.endswith(b"\n"):
                chunk = sock.recv(4096)
                if not chunk:
                    raise ConnectionResetError("connection closed before *IDN? reply")
                data += chunk
            reply = data.decode().strip()
            if reply.startswith("PulsePins,"):
                print(f"*IDN? -> {reply}")
                return sock
            sock.close()
        except OSError:
            time.sleep(0.5)
    raise SystemExit("failed to connect to ready ppscpi server")

with connect_ready_socket() as sock:
    sock.settimeout(5)

    def send_only(cmd: str) -> None:
        sock.sendall((cmd + "\n").encode())

    def ask(cmd: str) -> str:
        sock.sendall((cmd + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
        reply = data.decode().strip()
        print(f"{cmd} -> {reply}")
        return reply

    print("subcheck: built-in TEST1")
    assert ask("TEST1") == "SUCCESS"

    print("subcheck: CHECK state toggles and forced stream succeeds")
    send_only("CHECK ON")
    assert ask("CHECK?") == "TRUE"
    send_only("CHECK OFF")
    assert ask("CHECK?") == "FALSE"
    assert ask("SEQ d 1 0x1 f") == "LOADED"
    assert ask("STREAM") == "SUCCESS"
    assert ask("SYST:ERR?") == '0, "No error"'

    print("subcheck: checked non-strobed stream reports execution error")
    send_only("CHECK ON")
    assert ask("CHECK?") == "TRUE"
    assert ask("SEQ dn 1 0x1 f") == "LOADED"
    assert ask("STREAM") == "FAILURE"
    error_text = ask("SYST:ERR?")
    assert "STREAM failed with rc=" in error_text
    assert "timeout" in error_text
    assert ask("SYST:ERR?") == '0, "No error"'

    print("subcheck: malformed command reaches error queue")
    assert ask("BADCMD") == "ERROR"
    error_text = ask("SYST:ERR?")
    assert "unknown token 'BADCMD'" in error_text
    assert ask("SYST:ERR?") == '0, "No error"'

    print("subcheck: malformed query form returns ERROR")
    assert ask("STREAM?") == "ERROR"
    error_text = ask("SYST:ERR?")
    assert "not queryable" in error_text
    assert ask("SYST:ERR?") == '0, "No error"'

    ask("TERMINATE")
PY

  set +e
  wait "${PPSCPI_SSH_PID}"
  set -e
  PPSCPI_SSH_PID=""
}

# Network smoke for the web UI: start the server remotely, hit the main success flows,
# then verify one bad-request path and one timeout path before stopping it.
smoke_ppwebgui() {
  ssh "${TARGETHOST}" 'pkill -f "(^|/)ppwebgui($| )" >/dev/null 2>&1 || true'
  ssh -o ServerAliveInterval=2 -o ServerAliveCountMax=2 "${TARGETHOST}" 'exec ./ppwebgui -ip 0.0.0.0 -port 4242' &
  PPWEBGUI_SSH_PID=$!

  python3 - <<'PY'
import json
import os
import time
import urllib.parse
import urllib.error
import urllib.request

base = f"http://{os.environ['BOARD_IP']}:4242"

for _ in range(40):
    try:
        with urllib.request.urlopen(base + "/api/status", timeout=2) as resp:
            status = json.load(resp)
            break
    except Exception:
        time.sleep(0.5)
else:
    raise SystemExit("failed to connect to ppwebgui")

assert "stream" in status and "clocking" in status

def post_form(path: str, data: dict):
    body = urllib.parse.urlencode(data).encode()
    req = urllib.request.Request(
        base + path,
        data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        payload = json.load(resp)
    print(path, payload.get("message", ""))
    return payload

def post_form_expect_http_error(path: str, data: dict, expected_status: int):
    body = urllib.parse.urlencode(data).encode()
    req = urllib.request.Request(
        base + path,
        data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    try:
        urllib.request.urlopen(req, timeout=10)
    except urllib.error.HTTPError as exc:
        payload = json.loads(exc.read().decode())
        print(path, exc.code, payload.get("error", payload.get("message", "")))
        assert exc.code == expected_status
        return payload
    raise SystemExit(f"expected HTTP {expected_status} from {path}")

print("subcheck: remeasure clocks")
measure = post_form("/api/clocking/measure", {})
assert "status" in measure

print("subcheck: stream tiny valid sequence")
stream = post_form(
    "/api/stream",
    {
        "sequence_text": "d 1 0x1\n",
        "force_trigger": "1",
        "check_readback": "0",
    },
)
assert stream["ok"] is True

print("subcheck: reject malformed clock request")
bad_clocking = post_form_expect_http_error(
    "/api/clocking",
    {
        "source": "bad",
        "core_profile": "100M",
        "int_profile": "100M",
    },
    400,
)
assert bad_clocking["ok"] is False
assert "Invalid clock source" in bad_clocking["error"]

print("subcheck: reject explicit final in browser sequence text")
bad_stream = post_form_expect_http_error(
    "/api/stream",
    {
        "sequence_text": "d 1 0x1\nfinal 0x0\n",
        "force_trigger": "1",
        "check_readback": "0",
    },
    400,
)
assert bad_stream["ok"] is False
assert "explicit final output" in bad_stream["error"]

print("subcheck: report readback timeout as HTTP 504")
timed_out_stream = post_form_expect_http_error(
    "/api/stream",
    {
        "sequence_text": "dn 1 0x1\n",
        "force_trigger": "1",
        "check_readback": "1",
    },
    504,
)
assert timed_out_stream["ok"] is False
assert "timed out" in timed_out_stream["message"].lower()
PY

  kill "${PPWEBGUI_SSH_PID}"
  wait "${PPWEBGUI_SSH_PID}" >/dev/null 2>&1 || true
  PPWEBGUI_SSH_PID=""
}

section "Deploy"
run_step "Deploy bitstream and binaries" deploy deploy_artifacts
run_step "Discover board IP" discover-ip discover_board_ip

section "pptool Smoke"
run_step "ppcounter -test1 -check" ppcounter smoke_ppcounter
capture_common_metadata_from_log "${LOG_DIR}/ppcounter.log"
capture_binary_version_from_log pptool "${LOG_DIR}/ppcounter.log"
run_step "ppdmatest 21 -c 10 -v 16" ppdmatest21 smoke_ppdmatest21
run_step "ppread -timeout 1" ppread-timeout smoke_ppread_timeout
run_step "ppdmatest 22 -c 10 -v 16 -reps 4" ppdmatest22 smoke_ppdmatest22

section "Network Smoke"
run_step "ppscpi session and error handling" ppscpi smoke_ppscpi
capture_common_metadata_from_log "${LOG_DIR}/ppscpi.log"
capture_binary_version_from_log ppscpi "${LOG_DIR}/ppscpi.log"
run_step "ppwebgui API success and failure paths" ppwebgui smoke_ppwebgui
capture_common_metadata_from_log "${LOG_DIR}/ppwebgui.log"
capture_binary_version_from_log ppwebgui "${LOG_DIR}/ppwebgui.log"

printf '\n== Summary ==\n'
printf 'Target host: %s\n' "$TARGETHOST"
printf 'Board IP: %s\n' "$BOARD_IP"
if [[ -n "${PPTOOL_VERSION_LINE}" ]]; then
  printf 'pptool binary: %s\n' "$PPTOOL_VERSION_LINE"
fi
if [[ -n "${PPSCPI_VERSION_LINE}" ]]; then
  printf 'ppscpi binary: %s\n' "$PPSCPI_VERSION_LINE"
fi
if [[ -n "${PPWEBGUI_VERSION_LINE}" ]]; then
  printf 'ppwebgui binary: %s\n' "$PPWEBGUI_VERSION_LINE"
fi
if [[ -n "${BOARD_BITSTREAM_LINE}" ]]; then
  printf '%s\n' "$BOARD_BITSTREAM_LINE"
fi
printf 'Passed steps:\n'
for step in "${PASSED_STEPS[@]}"; do
  printf '  - %s\n' "$step"
done
printf 'Logs: %s\n' "$LOG_DIR"
