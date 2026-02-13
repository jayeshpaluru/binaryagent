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
    :root { --a:#09f; --b:#ff5ea7; --bg:#0b1020; }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      font-family: "Trebuchet MS", "Avenir Next", sans-serif;
      letter-spacing: .02em;
      background: radial-gradient(circle at 20% 20%, #1a2a4a, transparent 35%), linear-gradient(135deg, var(--bg), #1d2340);
      color: #f4f8ff;
    }
    .card {
      width: min(760px, 92vw);
      padding: 28px;
      border-radius: 22px;
      backdrop-filter: blur(8px);
      background: rgba(255,255,255,.08);
      box-shadow: 0 24px 70px rgba(0,0,0,.45);
      animation: floatIn .7s ease-out both;
      transition: transform .25s ease;
    }
    .card:hover { transform: translateY(-4px) scale(1.01); }
    h1 { margin: 0 0 8px; font-size: clamp(1.6rem, 3vw, 2.4rem); text-transform: uppercase; }
    .row { display: flex; gap: 12px; align-items: center; margin-top: 14px; }
    button { border: 0; border-radius: 999px; padding: 10px 14px; background: linear-gradient(90deg, var(--a), var(--b)); color: white; cursor: pointer; }
    @keyframes floatIn { from { opacity: 0; transform: translateY(12px); } to { opacity: 1; transform: translateY(0); } }
  </style>
</head>
<body>
  <main class="card">
    <h1>Smoke Test</h1>
    <p id="label">Styled + motion + interaction fixture.</p>
    <section class="row">
      <button id="btn">Shuffle Accent</button>
    </section>
  </main>
  <script>
    const btn = document.getElementById('btn');
    const label = document.getElementById('label');
    btn.addEventListener('click', () => {
      document.documentElement.style.setProperty('--a', '#' + Math.floor(Math.random()*16777215).toString(16).padStart(6, '0'));
      label.textContent = 'Updated at ' + new Date().toLocaleTimeString();
    });
  </script>
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
