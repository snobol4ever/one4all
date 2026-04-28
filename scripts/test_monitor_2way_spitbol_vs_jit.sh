#!/usr/bin/env bash
# test_monitor_2way_spitbol_vs_jit.sh — SPITBOL x64 (oracle) vs scrip --jit-run.
# Sibling of test_monitor_2way_spitbol_vs_sm.sh for the JIT path.
set -uo pipefail
SNO=${1:?Usage: test_monitor_2way_spitbol_vs_jit.sh <file.sno>}
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARTICIPANTS="spl scr" SCRIP_RUN_FLAG=--jit-run \
    exec bash "$HERE/test_monitor_3way_sync_step_auto.sh" "$SNO"
