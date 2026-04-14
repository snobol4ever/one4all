#!/usr/bin/env bash
# scripts/test_scrip_demos.sh — run all scrip demo/*.md files via --ir-run and compare to .expected
#
# Self-contained. Run from anywhere with no env vars.
# Usage: bash scripts/test_scrip_demos.sh
# Exit:  0 = all PASS, 1 = any FAIL
#
# Authors: LCherryholmes · Claude Sonnet 4.6

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SCRIP="${SCRIP:-$ROOT/scrip}"
DEMO_DIR="$ROOT/demo/scrip"
TIMEOUT=8
PASS=0; FAIL=0; SKIP=0

if [ ! -x "$SCRIP" ]; then
    echo "ERROR: scrip not found at $SCRIP — run build_scrip.sh first" >&2
    exit 1
fi

check() {
    local label="$1" expected="$2" actual="$3"
    if [ "$actual" = "$expected" ]; then
        echo "  PASS $label"
        PASS=$((PASS+1))
    else
        echo "  FAIL $label"
        # Show first differing line
        diff <(printf '%s\n' "$expected") <(printf '%s\n' "$actual") | head -6 | sed 's/^/       /'
        FAIL=$((FAIL+1))
    fi
}

echo "=== scrip demo suite (--ir-run) ==="
echo ""

# demo1 — Hello World (SNO + Icon + Prolog, each prints once → 3 lines)
# demo2 — Word Count   (SNO + Icon each print 9 → 2 lines)
# Each demo's .expected reflects what --ir-run produces for the full polyglot .md.
# If .expected was written for a single-language run it will be updated here as we go.

for demo_dir in "$DEMO_DIR"/demo*/; do
    dname=$(basename "$demo_dir")
    md=$(ls "$demo_dir"*.scrip 2>/dev/null | head -1)
    exp_file=$(ls "$demo_dir"*.expected 2>/dev/null | head -1)

    if [ -z "$md" ]; then
        echo "  SKIP $dname (no .scrip file)"
        SKIP=$((SKIP+1))
        continue
    fi
    if [ -z "$exp_file" ] || [ ! -f "$exp_file" ]; then
        echo "  SKIP $dname (no .expected file)"
        SKIP=$((SKIP+1))
        continue
    fi

    expected=$(cat "$exp_file")
    actual=$(timeout "$TIMEOUT" "$SCRIP" --ir-run "$md" < /dev/null 2>/dev/null)
    label="$dname  ($(basename "$md"))"
    check "$label" "$expected" "$actual"
done

echo ""
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
[ "$FAIL" -eq 0 ]
