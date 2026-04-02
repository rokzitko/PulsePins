#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SOURCE_FILE="$REPO_ROOT/contrib/completions/pulsepins.bash"
TARGET_DIR="/etc/profile.d"
TARGET_FILE="$TARGET_DIR/pulsepins-completion.sh"

if [[ ! -f "$SOURCE_FILE" ]]; then
  printf 'Completion source file not found: %s\n' "$SOURCE_FILE" >&2
  exit 1
fi

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  printf 'This installer must be run as root on the board.\n' >&2
  exit 1
fi

mkdir -p "$TARGET_DIR"
install -m 0644 "$SOURCE_FILE" "$TARGET_FILE"

cat <<EOF
Installed PulsePins Bash completion to:
  $TARGET_FILE

Reload your shell configuration with:
  source /etc/profile

or log out and log back in.
EOF
