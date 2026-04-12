#!/bin/bash
# build_silly.sh — build silly-snobol4 from one4all/src/silly
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/src/silly" && make
echo "Built: /tmp/silly-snobol4"
