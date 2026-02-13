#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[1/4] Building"
make

echo "[2/4] Materializing fixture"
./binagent materialize \
  --binary-in agent_output.binary.txt \
  --binary-out "$TMP_DIR/run.binary.txt" \
  --html-out "$TMP_DIR/run.html"

echo "[3/4] Output checks"
if [ ! -s "$TMP_DIR/run.binary.txt" ]; then
  echo "FAIL: binary output is empty" >&2
  exit 1
fi

if [ ! -s "$TMP_DIR/run.html" ]; then
  echo "FAIL: html output is empty" >&2
  exit 1
fi

echo "[4/4] HTML sanity check"
if ! rg -qi "<!doctype html>|<html" "$TMP_DIR/run.html"; then
  echo "FAIL: decoded output does not look like HTML" >&2
  exit 1
fi

echo "PASS: smoke test completed"
