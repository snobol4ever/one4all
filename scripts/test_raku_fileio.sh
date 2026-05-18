#!/usr/bin/env bash
# scripts/test_raku_fileio.sh — RK-38/RK-39/RK-56 file I/O gate
# Self-contained. Run from anywhere.
# Tests: open, close, slurp, lines, spurt, $*STDIN/$*STDOUT/$*STDERR
#
# Authors: LCherryholmes · Claude Sonnet 4.6

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
SCRIP="${SCRIP:-$REPO/scrip}"
TESTDIR="$REPO/test/raku"

if [ ! -x "$SCRIP" ]; then
    echo "SKIP scrip binary not found at $SCRIP" >&2; exit 0
fi

PASS=0; FAIL=0

run_one() {
    local name="$1" raku="$2" exp="$3"
    local got want
    got=$(timeout 8 "$SCRIP" --interp "$raku" < /dev/null 2>/dev/null) || true
    want=$(printf '%s' "$exp")
    if [ "$got" = "$want" ]; then
        echo "  PASS $name"; PASS=$((PASS+1))
    else
        echo "  FAIL $name"
        echo "    want: $(echo "$want" | tr '\n' '|')"
        echo "    got:  $(echo "$got"  | tr '\n' '|')"
        FAIL=$((FAIL+1))
    fi
}

echo "=== Raku file I/O (RK-38/39/56) ==="

# RK-38 + RK-56: spurt/slurp/lines
EXP38="$(printf 'line one\nline two\nline three\n\nslurp fh ok\nline one\nline two\nline three\nrk_fileio38 ok')"
run_one "rk_fileio38" "$TESTDIR/rk_fileio38.raku" "$EXP38"

# RK-39: $*STDOUT / $*STDERR
EXP39="$(printf 'stdout ok\nsay stdout ok\nstderr ok\nrk_stdio39 ok')"
run_one "rk_stdio39" "$TESTDIR/rk_stdio39.raku" "$EXP39"

echo ""
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
