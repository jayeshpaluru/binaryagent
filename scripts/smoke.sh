#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[1/4] Building"
make

echo "[2/4] Generating binary"
./binagent generate \
  --prompt "smoke test prompt" \
  --style neon \
  --binary-out "$TMP_DIR/generated.binary.txt"

echo "[3/4] Validating binary"
./binagent validate --binary-in "$TMP_DIR/generated.binary.txt"

echo "[4/4] Decoding + sanity checks"
./binagent decode --binary-in "$TMP_DIR/generated.binary.txt" --program-out "$TMP_DIR/generated.html"

if [ ! -s "$TMP_DIR/generated.html" ]; then
  echo "FAIL: decoded HTML is empty" >&2
  exit 1
fi

if ! rg -q "<!doctype html>" "$TMP_DIR/generated.html"; then
  echo "FAIL: decoded output does not look like HTML" >&2
  exit 1
fi

if ! rg -q "Binary-Coded Visual Program" "$TMP_DIR/generated.html"; then
  echo "FAIL: expected program marker not found in decoded HTML" >&2
  exit 1
fi

echo "PASS: smoke test completed"
