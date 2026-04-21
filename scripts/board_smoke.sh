#!/usr/bin/env bash
set -euo pipefail

TARGETHOST="${1:-${TARGETHOST:-de10nano}}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

RBF_PATH="${ROOT_DIR}/pulsepins.rbf"
PPTOOL_PATH="${ROOT_DIR}/c++/pptool"
PPSCPI_PATH="${ROOT_DIR}/c++/ppscpi"
PPWEBGUI_PATH="${ROOT_DIR}/c++/ppwebgui"

SYMLINKS=(
  pptest ppmstest ppdmatest ppfg ppreset pptrig ppdelay ppqout ppaux
  ppcounter ppts ppgpsdo pptemp ppfreq ppread ppplay ppvcd pphelloworld
)

require_file() {
  local file_path="$1"
  if [[ ! -f "${file_path}" ]]; then
    printf 'Missing required file: %s\n' "${file_path}" >&2
    exit 1
  fi
}

for path in "${RBF_PATH}" "${PPTOOL_PATH}" "${PPSCPI_PATH}" "${PPWEBGUI_PATH}"; do
  require_file "${path}"
done

for cmd in ssh scp python3; do
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    printf 'Missing required command: %s\n' "${cmd}" >&2
    exit 1
  fi
done

PPSCPI_SSH_PID=""
PPWEBGUI_SSH_PID=""
PPSCPI_LOG="$(mktemp -t ppscpi-smoke-XXXXXX.log)"
PPWEBGUI_LOG="$(mktemp -t ppwebgui-smoke-XXXXXX.log)"

cleanup() {
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

printf 'Deploying current bitstream and host tools to %s\n' "${TARGETHOST}"
scp "${RBF_PATH}" "${TARGETHOST}:pulsepins.rbf.new"
scp "${PPTOOL_PATH}" "${TARGETHOST}:pptool.new"
scp "${PPSCPI_PATH}" "${TARGETHOST}:ppscpi.new"
scp "${PPWEBGUI_PATH}" "${TARGETHOST}:ppwebgui.new"

remote_symlink_loop='for L in'
for link in "${SYMLINKS[@]}"; do
  remote_symlink_loop+=" ${link}"
done
remote_symlink_loop+='; do ln -sf pptool "$L"; done'

ssh "${TARGETHOST}" "mv -f pulsepins.rbf.new pulsepins.rbf && mv -f pptool.new pptool && mv -f ppscpi.new ppscpi && mv -f ppwebgui.new ppwebgui && ${remote_symlink_loop} && FPGA-writeConfig -f pulsepins.rbf"

BOARD_IP="$(ssh "${TARGETHOST}" "ip -4 addr show eth0 | awk '/inet /{print \$2}' | cut -d/ -f1")"
if [[ -z "${BOARD_IP}" ]]; then
  printf 'Failed to determine board IP for %s\n' "${TARGETHOST}" >&2
  exit 1
fi
printf 'Board IP: %s\n' "${BOARD_IP}"
export BOARD_IP

printf 'Running pptool finite smoke checks\n'
ssh "${TARGETHOST}" './ppcounter -test1 -check'
ssh "${TARGETHOST}" './ppdmatest 21 -c 10 -v 16'
ssh "${TARGETHOST}" './ppread -timeout 1' | tee /tmp/ppread-board-smoke.log
grep -q 'Caught ReadbackException: Timeout waiting for more readback data.' /tmp/ppread-board-smoke.log
ssh "${TARGETHOST}" './ppdmatest 22 -c 10 -v 16 -reps 4'
rm -f /tmp/ppread-board-smoke.log

printf 'Running ppscpi network smoke check\n'
ssh -o ServerAliveInterval=2 -o ServerAliveCountMax=2 "${TARGETHOST}" './ppscpi' >"${PPSCPI_LOG}" 2>&1 &
PPSCPI_SSH_PID=$!

python3 - <<'PY'
import os
import socket
import time

HOST = os.environ["BOARD_IP"]
PORT = 5025

for _ in range(40):
    try:
        sock = socket.create_connection((HOST, PORT), timeout=1)
        break
    except OSError:
        time.sleep(0.5)
else:
    raise SystemExit("failed to connect to ppscpi")

with sock:
    sock.settimeout(5)

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

    assert ask("*IDN?").startswith("PulsePins,")
    assert ask("TEST1") == "SUCCESS"
    ask("TERMINATE")
PY

wait "${PPSCPI_SSH_PID}"
PPSCPI_SSH_PID=""

printf 'Running ppwebgui HTTP smoke check\n'
ssh -o ServerAliveInterval=2 -o ServerAliveCountMax=2 "${TARGETHOST}" './ppwebgui -ip 0.0.0.0 -port 4242' >"${PPWEBGUI_LOG}" 2>&1 &
PPWEBGUI_SSH_PID=$!

python3 - <<'PY'
import os
import json
import time
import urllib.parse
import urllib.request

BASE = f"http://{os.environ['BOARD_IP']}:4242"

for _ in range(40):
    try:
        with urllib.request.urlopen(BASE + "/api/status", timeout=2) as resp:
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
        BASE + path,
        data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        payload = json.load(resp)
    print(path, payload.get("message", ""))
    return payload

measure = post_form("/api/clocking/measure", {})
assert "status" in measure
stream = post_form(
    "/api/stream",
    {
        "sequence_text": "d 1 0x1\n",
        "force_trigger": "1",
        "check_readback": "0",
    },
)
assert stream["ok"] is True
PY

kill "${PPWEBGUI_SSH_PID}"
wait "${PPWEBGUI_SSH_PID}" >/dev/null 2>&1 || true
PPWEBGUI_SSH_PID=""

printf 'Board smoke succeeded. Logs:\n'
printf '  ppscpi:   %s\n' "${PPSCPI_LOG}"
printf '  ppwebgui: %s\n' "${PPWEBGUI_LOG}"
