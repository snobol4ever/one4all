#!/usr/bin/env bash
# test_smoke_compile_hello_all_langs.sh — IJ-HELLO-1 baseline gate
#
# Gate for GOAL-ICON-BB-JCON / IJ-MODE4-HELLO-ALL-LANGS.  Drives the canonical
# "hello world" program in each of the six SCRIP-supported languages through
# the full mode-4 pipeline:
#
#   scrip --compile  →  gcc -c  →  gcc -lscrip_rt  →  run  →  diff stdout
#
# Plus an `nm` audit on each emitted binary asserting **no direct brokered
# imports** (bb_broker / rt_bb_once_proc / rt_bb_pump_proc).  The audit
# protects the IJ-MODE4-HELLO-ALL-LANGS architecture mandate ("wired only
# under --compile") at static link time.
#
# Per-language expected outcome (this is the IJ-HELLO-1 BASELINE, measured at
# HEAD a4fe1c21 with Opus 4.7 on 2026-05-18, sub-rung IJ-HELLO-1):
#
#   snobol4   PASS  wired-clean    OUTPUT = 'Hello, World!'
#   snocone   PASS  wired-clean    OUTPUT = 'Hello, World!';
#   rebus     PASS  wired-clean    output := "Hello, World!"
#   icon      FAIL  runtime trap  "unhandled SM opcode 61 reached" (= SM_BB_PUMP_PROC)
#   prolog    FAIL  assemble fail  no .macro PUSH_EXPR in sm_macros.s (directive form)
#   raku      PASS  wired-clean    OUTPUT = 'Hello, World!' (IJ-HELLO-2b 2026-05-18)
#
# IJ-HELLO-2b floor: PASS=4 FAIL=2.  Each subsequent sub-rung (IJ-HELLO-3/4)
# flips one FAIL row to PASS until IJ-HELLO-5 closes the matrix at 6/0.
#
# The script's exit code reports `FAIL=$ACTUAL_FAIL ≠ $EXPECTED_FAIL`, so it
# fails LOUDLY if the baseline ever drifts (either improvement or regression).
# When IJ-HELLO-2/3/4 lands, edit the EXPECTED_FAIL constant alongside the
# fix commit.
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet (Opus 4.7)
# DATE: 2026-05-18

set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SCRIP="$ROOT/scrip"
RT_SO="$ROOT/out/libscrip_rt.so"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$SCRIP" ] || { echo "SKIP scrip not built — run scripts/build_scrip.sh"; exit 0; }
[ -f "$RT_SO" ] || { echo "SKIP libscrip_rt.so not built — run: make libscrip_rt"; exit 0; }

EXPECTED_OUTPUT="Hello, World!"
HW_EXPECTED_PASS=4   # snobol4 snocone rebus raku run hello-world correctly (raku ✅ IJ-HELLO-2b)
HW_EXPECTED_FAIL=2   # icon prolog currently fail at IJ-HELLO-2b baseline
ROWS_MATCH=0; ROWS_DRIFT=0
HW_PASS=0; HW_FAIL=0

# ── Six canonical hello programs ──────────────────────────────────────────────

cat > "$TMP/hello.sno" << 'EOF'
        OUTPUT = 'Hello, World!'
END
EOF

cat > "$TMP/hello.sc" << 'EOF'
OUTPUT = 'Hello, World!';
EOF

cat > "$TMP/hello.reb" << 'EOF'
function main()
  output := "Hello, World!"
end
EOF

cat > "$TMP/hello.icn" << 'EOF'
procedure main()
    write("Hello, World!")
end
EOF

cat > "$TMP/hello.pl" << 'EOF'
:- initialization(main, main).
main :- write('Hello, World!'), nl.
EOF

cat > "$TMP/hello.raku" << 'EOF'
sub main() { say('Hello, World!'); }
EOF

# ── Pipeline runner ───────────────────────────────────────────────────────────
# Outcome buckets recorded as: PASS-wired, FAIL-compile, FAIL-link, FAIL-run,
# FAIL-wrong-output, FAIL-brokered.  Each language has an EXPECTED bucket; the
# row passes iff the actual bucket equals the expected one.
#
# Args: $1=lang  $2=src  $3=expected_bucket
#
# Buckets:
#   PASS-wired       compile ok, link ok, run rc=0, stdout=EXPECTED_OUTPUT, no brokered U
#   FAIL-compile     scrip --compile exited non-zero or wrote empty .s
#   FAIL-link        gcc -c / -ld exited non-zero
#   FAIL-run         binary exited non-zero (or no output for success-rc form)
#   FAIL-wrong-out   binary ran rc=0 but stdout != EXPECTED_OUTPUT
#   FAIL-brokered    pipeline runs correctly but `nm` shows brokered import
#
# Note: FAIL-brokered is the architecture-violation flag that this gate exists
# to enforce.  Prolog can in principle run "correctly" today via the brokered
# rt_bb_once_proc path; this gate FAILs that row anyway because brokered is
# forbidden under --compile per IJ-MODE4-HELLO-ALL-LANGS' Lon directive.

check_lang() {
    local lang="$1" src="$2" expected="$3"
    local s="$TMP/${lang}.s"
    local bin="$TMP/${lang}.bin"
    local compile_err="$TMP/${lang}.compile.err"
    local ld_err="$TMP/${lang}.ld.err"
    local run_out="$TMP/${lang}.run.out"
    local run_err="$TMP/${lang}.run.err"
    local actual=""

    timeout 8 "$SCRIP" --compile "$src" > "$s" 2> "$compile_err"
    local c_rc=$?
    if [ "$c_rc" -ne 0 ] || [ ! -s "$s" ]; then
        actual="FAIL-compile"
    else
        gcc -no-pie "$s" -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
            -o "$bin" 2> "$ld_err"
        local l_rc=$?
        if [ "$l_rc" -ne 0 ] || [ ! -x "$bin" ]; then
            actual="FAIL-link"
        else
            bash -c '
                timeout 5 "$1" > "$2" 2> "$3"
                exit $?
            ' _ "$bin" "$run_out" "$run_err" 2>/dev/null
            local r_rc=$?
            local got; got=$(cat "$run_out" 2>/dev/null)
            if [ "$r_rc" -ne 0 ]; then
                actual="FAIL-run"
            elif [ "$got" != "$EXPECTED_OUTPUT" ]; then
                actual="FAIL-wrong-out"
            else
                # Output is correct — now audit brokered imports.
                local brokered
                brokered=$(nm "$bin" 2>/dev/null | \
                    grep -E " U (bb_broker|rt_bb_once_proc|rt_bb_pump_proc)\b" \
                    || true)
                if [ -n "$brokered" ]; then
                    actual="FAIL-brokered"
                else
                    actual="PASS-wired"
                fi
            fi
        fi
    fi

    if [ "$actual" = "$expected" ]; then
        echo "  ROW-MATCH $lang  ($actual — matches baseline)"
        ROWS_MATCH=$((ROWS_MATCH+1))
    else
        echo "  ROW-DRIFT $lang  (expected=$expected got=$actual)"
        ROWS_DRIFT=$((ROWS_DRIFT+1))
        case "$actual" in
            FAIL-compile)   head -3 "$compile_err" | sed 's/^/    /' ;;
            FAIL-link)      head -3 "$ld_err"      | sed 's/^/    /' ;;
            FAIL-run)       head -1 "$run_err"     | sed 's/^/    /' ;;
            FAIL-wrong-out) echo "    got=[$(cat $run_out)] stderr=[$(head -1 $run_err 2>/dev/null)]" ;;
            FAIL-brokered)  echo "    $(echo "$brokered" | tr '\n' ' ')" ;;
        esac
    fi

    # Also tally the underlying hello-world pass/fail (the matrix the goal cares
    # about), independent of whether the row matched the baseline.
    if [ "$actual" = "PASS-wired" ]; then
        HW_PASS=$((HW_PASS+1))
    else
        HW_FAIL=$((HW_FAIL+1))
    fi
}

echo "=== IJ-HELLO-1 — --compile hello-world × 6 languages × wired-only audit ==="

check_lang snobol4 "$TMP/hello.sno"  "PASS-wired"
check_lang snocone "$TMP/hello.sc"   "PASS-wired"
check_lang rebus   "$TMP/hello.reb"  "PASS-wired"
check_lang icon    "$TMP/hello.icn"  "FAIL-run"          # SM_BB_PUMP_PROC unhandled at runtime
check_lang prolog  "$TMP/hello.pl"   "FAIL-link"         # no .macro PUSH_EXPR in sm_macros.s
check_lang raku    "$TMP/hello.raku" "PASS-wired"        # IJ-HELLO-2b: SUB_TAG_ID match in lower_stmt skips spurious CALL_FN main wrapper

echo
echo "PASS=$HW_PASS FAIL=$HW_FAIL  (hello-world matrix; baseline: PASS=$HW_EXPECTED_PASS FAIL=$HW_EXPECTED_FAIL)"
echo "ROWS_MATCH=$ROWS_MATCH ROWS_DRIFT=$ROWS_DRIFT  (gate exits 0 iff ROWS_DRIFT=0)"
if [ "$ROWS_DRIFT" -eq 0 ]; then
    exit 0
fi
exit 1
