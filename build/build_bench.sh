#!/bin/bash
# build_bench.sh — build one4all benchmarks
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/bench" && make
echo "Built: bench"
