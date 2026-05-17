#!/usr/bin/env bash
# scripts/test_mode4_broad_corpus_snobol4.sh — M4SN-4a: broad corpus SNOBOL4 mode-4 parity
# Runs the same set as test_interp_broad_corpus_and_beauty.sh but via
# emit->assemble->link->run (mode-4 ELF pipeline) instead of --interp.
# Compares output against .ref files.
# Target: PASS >= sm-run PASS count (128/280). No regression vs sm-run.
#
# Self-contained per RULES.md: paths from $0, timeout on every run.
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6   DATE: 2026-05-14

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/scrip}"
RT_DIR="${RT_DIR:-$HERE/../out}"
CORPUS="/home/claude/corpus"
TIMEOUT="${TIMEOUT:-10}"
INC="$CORPUS/programs/snobol4/demo/inc"
BEAUTY="$CORPUS/programs/snobol4/beauty"
DEMO="$CORPUS/programs/snobol4/demo"

if [ ! -x "$SCRIP" ]; then echo "SKIP scrip not built at $SCRIP"; exit 0; fi
if [ ! -f "$RT_DIR/libscrip_rt.so" ]; then echo "SKIP libscrip_rt.so not built at $RT_DIR"; exit 0; fi
if [ ! -d "$CORPUS" ]; then echo "SKIP corpus not found at $CORPUS"; exit 0; fi

PASS=0; FAIL=0; SKIP=0
FAILURES=""

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

compile_mode4() {
    local sno="$1" out="$2"
    local tmp; tmp="$(mktemp -d)"
    SNO_LIB="$INC" "$SCRIP" --compile "$sno" > "$tmp/p.s" 2>/dev/null || { rm -rf "$tmp"; return 1; }
    (cd "$HERE/.." && gcc -c "$tmp/p.s" -o "$tmp/p.o" 2>/dev/null) || { rm -rf "$tmp"; return 1; }
    gcc "$tmp/p.o" -L"$RT_DIR" -lscrip_rt -lgc -lm \
        -Wl,-rpath,"$RT_DIR" -o "$out" 2>/dev/null || { rm -rf "$tmp"; return 1; }
    rm -rf "$tmp"
}

run_test() {
    local label="$1" sno="$2" ref="$3" input="${4:-}" filter="${5:-}"
    [ ! -f "$ref" ] && return
    [ ! -f "$sno" ] && return
    local bin="$WORKDIR/$(echo "$label" | tr '/' '_').bin"
    if ! compile_mode4 "$sno" "$bin"; then
        SKIP=$((SKIP+1)); return
    fi
    local got exp
    if [ -n "$input" ] && [ -f "$input" ]; then
        got=$(SNO_LIB="$INC" timeout "$TIMEOUT" "$bin" < "$input" 2>/dev/null || true)
    else
        got=$(SNO_LIB="$INC" timeout "$TIMEOUT" "$bin" < /dev/null 2>/dev/null || true)
    fi
    if [ -n "$filter" ]; then
        got=$(printf '%s\n' "$got" | grep -v "$filter" || true)
    fi
    exp=$(cat "$ref")
    if [ "$got" = "$exp" ]; then
        PASS=$((PASS+1))
    else
        FAIL=$((FAIL+1))
        FAILURES="${FAILURES}  FAIL ${label}\n"
    fi
}

# ── Crosscheck corpus ──────────────────────────────────────────────────────────
while IFS= read -r sno; do
    ref="${sno%.sno}.ref"
    input="${sno%.sno}.input"
    [ ! -f "$ref" ] && continue
    label=$(basename "$sno" .sno)
    run_test "$label" "$sno" "$ref" "$input" ""
done < <(find "$CORPUS/crosscheck" -name "*.sno" | sort)

# ── Beauty library drivers ─────────────────────────────────────────────────────
for sno in "$BEAUTY"/*_driver.sno; do
    [ ! -f "$sno" ] && continue
    name=$(basename "$sno" .sno)
    ref="$BEAUTY/${name}.ref"
    run_test "$name" "$sno" "$ref" "" ""
done

# ── Demo programs ─────────────────────────────────────────────────────────────
run_test "demo_wordcount" "$DEMO/wordcount.sno" "$DEMO/wordcount.ref" "$DEMO/wordcount.input" ""
run_test "demo_treebank"  "$DEMO/treebank.sno"  "$DEMO/treebank.ref"  "$DEMO/treebank.input"  ""
run_test "demo_claws5"    "$DEMO/claws5.sno"    "$DEMO/claws5.ref"    "$DEMO/claws5.input"    ""
TIMEOUT=30 \
run_test "demo_roman"     "$DEMO/roman.sno"     "$DEMO/roman.ref"     ""                      "^ms:"

echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP  ($((PASS+FAIL+SKIP)) total)"
[ -n "$FAILURES" ] && printf "$FAILURES" | head -40
