#!/usr/bin/env bash
# test_monitor_2way_spitbol_vs_ir.sh — SPITBOL x64 (oracle) vs scrip --interp.
#
# 2-way sync-step monitor comparing the SPITBOL x64 oracle against
# scrip's tree-walking IR interpreter.  This is the primary
# scrip-debugging path: every divergence is a scrip bug to be hunted
# in src/driver/interp.c, src/runtime/x86/snobol4_*.c, etc.
#
# Implementation: thin wrapper around test_monitor_3way_sync_step_auto.sh
# with PARTICIPANTS="spl scr" — env-var driven binary IPC, no source
# preprocessing.  Per RULES.md "Sync-step monitor — keyword catch-alls
# only, no source preprocessing".
#
# Usage:  bash test_monitor_2way_spitbol_vs_ir.sh <file.sno>
# Exit:   0 = agree   1 = diverge   2 = timeout / setup failure
#
# Requires (run once per session):
#   bash scripts/install_system_packages.sh
#   bash scripts/build_scrip.sh
#   bash scripts/build_spitbol_oracle.sh   # SN-26-spl-bridge applied
set -uo pipefail

SNO=${1:?Usage: test_monitor_2way_spitbol_vs_ir.sh <file.sno>}
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PARTICIPANTS="spl scr" exec bash "$HERE/test_monitor_3way_sync_step_auto.sh" "$SNO"
