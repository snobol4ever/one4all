#!/bin/bash
# build_snobol4_frontend.sh — build snobol4 frontend
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/src/frontend/snobol4" && make
echo "Built: snobol4 frontend"
