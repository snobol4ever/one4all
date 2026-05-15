#!/bin/bash
# build_silly.sh — build silly-snobol4 from one4all/silly
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/silly" && make
echo "Built: /tmp/silly-snobol4"
