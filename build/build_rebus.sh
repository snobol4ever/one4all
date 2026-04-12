#!/bin/bash
# build_rebus.sh — build rebus frontend
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/src/frontend/rebus" && make
echo "Built: rebus"
