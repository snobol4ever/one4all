#!/usr/bin/env bash
# test_monitor_2way_spitbol_vs_sm.sh — SPITBOL x64 (oracle) vs scrip --sm-run.
#
# Sibling of test_monitor_2way_spitbol_vs_ir.sh that drives the scrip
# side with --sm-run (stack-machine interpreter) instead of --ir-run.
# Used by SN-32 (SM/JIT beauty self-host) to surface SM-runtime bugs
# on the IPC sync-step wire.  The default test_monitor_3way_sync_step_auto.sh
# hardcodes --ir-run on the scrip line; this script overrides the
# scrip launch by exporting SCRIP_RUN_FLAG=--sm-run, which the *_auto*
# script consults if set, else falls back to --ir-run.
#
# Usage:  bash test_monitor_2way_spitbol_vs_sm.sh <file.sno>
set -uo pipefail
SNO=${1:?Usage: test_monitor_2way_spitbol_vs_sm.sh <file.sno>}
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARTICIPANTS="spl scr" SCRIP_RUN_FLAG=--sm-run \
    exec bash "$HERE/test_monitor_3way_sync_step_auto.sh" "$SNO"
