#!/usr/bin/env bash
# test_rs23_diag_capture.sh — run smoke + corpus under scrip-rs23-diag,
# capturing RS23DIAG: lines that record any interp_eval call reached
# from a BB-adapter ancestor frame.
#
# Output: /tmp/rs23_diag.log (raw) + /tmp/rs23_diag_unique.log (sorted -u).
# Print summary to stdout.
#
# Strategy: temporarily swap scrip ↔ scrip-rs23-diag so the existing
# gate scripts (which reference $HERE/../scrip) pick up the diag binary
# without modification.  Restore on exit.

set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT"

SCRIP="$ROOT/scrip"
DIAG="$ROOT/scrip-rs23-diag"
BACKUP="$ROOT/.scrip.before-rs23-diag"

[ -x "$DIAG" ] || { echo "FAIL diag binary missing — run build_scrip_rs23_diag.sh first"; exit 1; }
[ -x "$SCRIP" ] || { echo "FAIL scrip missing"; exit 1; }

cp "$SCRIP" "$BACKUP"
trap 'cp "$BACKUP" "$SCRIP"; rm -f "$BACKUP"; echo "[restore] scrip restored"' EXIT
cp "$DIAG" "$SCRIP"

LOG=/tmp/rs23_diag.log
: > "$LOG"
export RS23_DIAG_LOG="$LOG"

run_gate() {
    local name="$1" cmd="$2"
    echo "=== $name ==="
    eval "$cmd" 2>&1 | tail -3 || true
}

run_gate "smoke_snobol4" "bash scripts/test_smoke_snobol4.sh"
run_gate "smoke_icon"    "bash scripts/test_smoke_icon.sh"
run_gate "smoke_prolog"  "bash scripts/test_smoke_prolog.sh"
run_gate "smoke_raku"    "bash scripts/test_smoke_raku.sh"
run_gate "unified_broker" "bash scripts/test_smoke_unified_broker.sh"

# Optional: full Icon corpus — may be slow.  Drive it manually with the
# corpus runner if it exists.
if [ -f "$ROOT/scripts/test_icon_all_rungs.sh" ]; then
    run_gate "icon_ir_all_rungs" "bash scripts/test_icon_all_rungs.sh"
fi

echo
echo "=== RS23DIAG summary ==="
grep -c '^RS23DIAG:' "$LOG" | xargs printf "raw lines: %s\n"
grep '^RS23DIAG:' "$LOG" | sort -u > /tmp/rs23_diag_unique.log
wc -l < /tmp/rs23_diag_unique.log | xargs printf "unique  : %s\n"
echo
echo "Top kinds:"
grep '^RS23DIAG:' "$LOG" | awk '{print $2}' | sort | uniq -c | sort -rn | head -20
echo
echo "Unique (kind, caller, via) tuples written to /tmp/rs23_diag_unique.log"
