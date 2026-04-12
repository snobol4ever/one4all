#!/bin/bash
# build_spitbol.sh — build SPITBOL x64 oracle from snobol4ever/x64
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
X64="$ROOT/x64"
[ -d "$X64" ] || { echo "Clone snobol4ever/x64 first"; exit 1; }
cd "$X64" && make
cp "$X64/sbl" /usr/local/bin/spitbol
echo "Built: /usr/local/bin/spitbol"
