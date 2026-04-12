#!/bin/bash
# build_snobol4jvm.sh — build snobol4jvm from snobol4ever/snobol4jvm
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPO="$ROOT/snobol4jvm"
[ -d "$REPO" ] || { echo "Clone snobol4ever/snobol4jvm first"; exit 1; }
cd "$REPO"
mvn package -q
echo "Built: snobol4jvm"
