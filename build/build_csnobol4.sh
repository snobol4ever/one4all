#!/bin/bash
# build_csnobol4.sh — build CSNOBOL4 from snobol4ever/csnobol4
# Usage: bash build/build_csnobol4.sh
# Requires: gcc, make, m4 (installed automatically)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPO="$(cd "$ROOT/csnobol4" && pwd)"
[ -d "$REPO/.git" ] || { echo "ERROR: clone snobol4ever/csnobol4 to $REPO first"; exit 1; }
apt-get install -y m4 -qq 2>/dev/null || true
cd "$REPO"
make -f Makefile2 xsnobol4 -j4
cp "$REPO/xsnobol4" "$REPO/snobol4"
echo "Built: $REPO/snobol4"
