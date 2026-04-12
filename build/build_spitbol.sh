#!/bin/bash
# build_spitbol.sh — build SPITBOL x64 oracle from snobol4ever/x64
# Usage: bash build/build_spitbol.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
X64="$(cd "$ROOT/../x64" && pwd)"
[ -d "$X64/.git" ] || { echo "ERROR: clone snobol4ever/x64 to $X64 first"; exit 1; }
cd "$X64" && make
cp "$X64/sbl" /usr/local/bin/spitbol
chmod +x /usr/local/bin/spitbol
echo "Built: /usr/local/bin/spitbol"
