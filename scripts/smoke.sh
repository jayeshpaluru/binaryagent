#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[1/4] Building"
make

echo "[2/4] Creating styled HTML fixture and binary stream"
cat > "$TMP_DIR/seed.html" <<'HTML'
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <style>
    body { margin: 0; font-family: system-ui; background: linear-gradient(135deg, #111827, #1f2937); color: #fff; }
    .card { margin: 48px auto; max-width: 640px; padding: 24px; border-radius: 16px; box-shadow: 0 20px 50px rgba(0,0,0,.35); }
  </style>
</head>
<body>
  <main class="card"><h1>Smoke Test</h1><p>Styled and aesthetic fixture.</p></main>
</body>
</html>
HTML

od -An -t u1 -v "$TMP_DIR/seed.html" | awk '
  {
    for (i = 1; i <= NF; i++) {
      n = $i;
      b = "";
      for (j = 7; j >= 0; j--) {
        bit = int(n / (2 ^ j)) % 2;
        b = b bit;
      }
      printf "%s%s", (count++ ? " " : ""), b;
    }
  }
  END { printf "\n"; }
' > "$TMP_DIR/input.binary.txt"

echo "[3/4] Materializing fixture"
./binagent materialize \
  --binary-in "$TMP_DIR/input.binary.txt" \
  --binary-out "$TMP_DIR/run.binary.txt" \
  --html-out "$TMP_DIR/run.html"

echo "[4/4] Output checks"
if [ ! -s "$TMP_DIR/run.binary.txt" ]; then
  echo "FAIL: binary output is empty" >&2
  exit 1
fi

if [ ! -s "$TMP_DIR/run.html" ]; then
  echo "FAIL: html output is empty" >&2
  exit 1
fi

echo "[5/5] HTML sanity check"
if ! rg -qi "<!doctype html>|<html" "$TMP_DIR/run.html"; then
  echo "FAIL: decoded output does not look like HTML" >&2
  exit 1
fi

if ! rg -qi "<style|style=" "$TMP_DIR/run.html"; then
  echo "FAIL: decoded output does not contain CSS styling" >&2
  exit 1
fi

if ! rg -qi "gradient|animation|@keyframes|box-shadow|transition|transform|border-radius" "$TMP_DIR/run.html"; then
  echo "FAIL: decoded output does not contain an aesthetic visual cue" >&2
  exit 1
fi

echo "PASS: smoke test completed"
