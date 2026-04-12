#!/bin/bash
# build_ss_monitor.sh — build ss-monitor test harness
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/test/ss-monitor" && make
echo "Built: ss-monitor"
