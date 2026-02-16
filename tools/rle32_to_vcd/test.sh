#!/usr/bin/env bash
set -euo pipefail

# Assumes ./rle32_to_vcd is already built and executable.

BIN="${1:-./rle32_to_vcd}"
OUT="test_out.vcd"

if [[ ! -x "$BIN" ]]; then
  echo "ERROR: binary not found or not executable: $BIN" >&2
  exit 2
fi

echo "[1/3] Generate RLE stimulus and convert to VCD"
cat > test_rle.txt <<'EOF'
# value runlen
0x00000000 3
0xFFFFFFFF 2
0x12345678 4
0x12345678 1   # same value again (still should emit at boundary)
0x00000001 5
EOF

# dt=1, timescale=1ns
"$BIN" "$OUT" 1 1ns < test_rle.txt

echo "[2/3] Sanity checks"
grep -q '^\$timescale 1ns \$end$' "$OUT"
grep -q '^\$var wire 32 b bus32 \$end$' "$OUT"
grep -q '^#0$' "$OUT"

# Expected time markers:
# 0, 3, 5, 9, 10, 15
for t in 0 3 5 9 10 15; do
  grep -q "^#${t}$" "$OUT"
done

# Value lines
grep -q '^b00000000000000000000000000000000 b$' "$OUT"
grep -q '^b11111111111111111111111111111111 b$' "$OUT"
grep -q '^b00010010001101000101011001111000 b$' "$OUT"   # 0x12345678
grep -q '^b00000000000000000000000000000001 b$' "$OUT"

echo "[3/3] OK"
echo "Generated: $OUT"
echo "Tip: open with GTKWave: gtkwave $OUT"
