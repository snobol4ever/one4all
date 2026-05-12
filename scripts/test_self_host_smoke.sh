#!/usr/bin/env bash
# test_self_host_smoke.sh — SI-5 cross-check: self-hosted pipeline output must
# byte-match native scrip output on the same logical program.
#
# For each (hosted, native) pair below, run both via `scrip --ir-run` and diff
# stdout.  The hosted invocation chains tree.sc + lower.sc + lower_driver.sc +
# sm_interp.sc + the *_interp.sc test driver; the native invocation runs the
# corresponding *_native.sc source directly.  Both must produce identical bytes.
#
# Idempotent.  No prerequisites beyond a built scrip and a corpus checkout.
# Self-contained per RULES.md — paths derived from $0, hardcoded corpus path,
# explicit timeout on every scrip call, < /dev/null for safety.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$(cd "$HERE/.." && pwd)"
SCRIP="${SCRIP:-$ONE4ALL/scrip}"
CORPUS="/home/claude/corpus"
SCRIP_DIR="$CORPUS/SCRIP"

[ -x "$SCRIP" ] || { echo "SKIP scrip not built at $SCRIP"; exit 0; }
[ -d "$SCRIP_DIR" ] || { echo "SKIP corpus SCRIP dir missing: $SCRIP_DIR"; exit 0; }

pass=0
fail=0
for case in smoke_interp sm_interp_test ; do
    hosted_sc="$SCRIP_DIR/${case}.sc"
    native_sc="$SCRIP_DIR/${case}_native.sc"
    [ -f "$hosted_sc" ] || { echo "SKIP $case — hosted source missing"; continue; }
    [ -f "$native_sc" ] || { echo "SKIP $case — native source missing"; continue; }
    hosted_out="$(timeout 8 "$SCRIP" --ir-run \
        "$SCRIP_DIR/tree.sc" \
        "$SCRIP_DIR/lower.sc" \
        "$SCRIP_DIR/lower_driver.sc" \
        "$SCRIP_DIR/sm_interp.sc" \
        "$hosted_sc" \
        < /dev/null)"
    native_out="$(timeout 8 "$SCRIP" --ir-run "$native_sc" < /dev/null)"
    if [ "$hosted_out" = "$native_out" ] ; then
        echo "PASS $case  (hosted == native, $(echo "$hosted_out" | wc -l) lines)"
        pass=$((pass + 1))
    else
        echo "FAIL $case"
        echo "  --- hosted ---"
        echo "$hosted_out" | sed 's/^/    /'
        echo "  --- native ---"
        echo "$native_out" | sed 's/^/    /'
        fail=$((fail + 1))
    fi
done
echo "PASS=$pass FAIL=$fail"
[ $fail -eq 0 ] || exit 1
