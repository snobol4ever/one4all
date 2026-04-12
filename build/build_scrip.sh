#!/bin/bash
# build_scrip.sh — build the scrip compiler (top-level one4all)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" && make scrip
echo "Built: $ROOT/scrip"
