#!/usr/bin/env bash
# scripts/test_gate_em_beauty_subsystems_mode4.sh — EM-7d-prep gate:
# every *_driver.sno in corpus/programs/snobol4/beauty/, under
#   --jit-emit --x64    (mode 4: emit .s, link to libscrip_rt.so, run binary)
# produces output byte-identical to the same driver under
#   --sm-run            (mode 2: proven SM interpreter)
#
# This is a PARITY gate, not a correctness gate.  The beauty drivers'
# own .ref files belong to GOAL-PARSER-SNOBOL4 (rung SN-7-8) and may be
# regressing independently.  What this gate measures is solely whether
# mode-4 (emit binary) tracks mode-3/sm-run (runtime interp) — i.e.
# whether the emitter pipeline + libscrip_rt.so reproduces what the SM
# interpreter does on the same input.  Mode-4 cannot be more correct
# than mode-3; the goal here is simply that they agree.
#
# Self-contained per RULES.md: paths derived from $0; no env deps.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SCRIP="${SCRIP:-$ROOT/scrip}"
RT_DIR="${RT_DIR:-$ROOT/out}"
CORPUS="${CORPUS:-/home/claude/corpus}"
BEAUTY="$CORPUS/programs/snobol4/beauty"
TIMEOUT="${TIMEOUT:-30}"

if [ ! -x "$SCRIP" ]; then
    echo "SKIP scrip not built at $SCRIP"
    exit 0
fi
if [ ! -f "$RT_DIR/libscrip_rt.so" ]; then
    echo "SKIP libscrip_rt.so not built — run: make libscrip_rt"
    exit 0
fi
if [ ! -d "$BEAUTY" ]; then
    echo "SKIP corpus not populated at $CORPUS"
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"

PASS=0
FAIL_EMIT=0
FAIL_LINK=0
FAIL_DIFF=0
FAILS=""

for sno in "$BEAUTY"/*_driver.sno; do
    [ ! -f "$sno" ] && continue
    name=$(basename "$sno" .sno)

    # mode-3 oracle: --sm-run on the same source.  Don't gate on its
    # rc; just capture what it produces.  If it segfaults, mode-4
    # parity means mode-4 should produce the same (empty) output.
    # bash -c runs the child in a fresh shell so SIGSEGV trap message
    # (printed by the parent's job-control) doesn't reach our stderr.
    bash -c "SNO_LIB='$BEAUTY' timeout '$TIMEOUT' '$SCRIP' --sm-run '$sno' < /dev/null" \
        > "$name.sm.out" 2>/dev/null || true

    # mode-4: emit -> assemble+link -> run.
    SNO_LIB="$BEAUTY" "$SCRIP" --jit-emit --x64 "$sno" \
        > "$name.s" 2> "$name.emit.err"
    if [ $? -ne 0 ] || [ ! -s "$name.s" ]; then
        FAIL_EMIT=$((FAIL_EMIT + 1))
        FAILS="$FAILS $name(emit)"
        continue
    fi
    gcc -no-pie "$name.s" -L"$RT_DIR" -lscrip_rt \
        -Wl,-rpath,"$RT_DIR" -o "$name.prog" 2> "$name.link.err"
    if [ $? -ne 0 ]; then
        FAIL_LINK=$((FAIL_LINK + 1))
        FAILS="$FAILS $name(link)"
        continue
    fi
    bash -c "SNO_LIB='$BEAUTY' timeout '$TIMEOUT' ./'$name.prog' < /dev/null" \
        > "$name.m4.out" 2>/dev/null || true

    if diff -q "$name.m4.out" "$name.sm.out" > /dev/null 2>&1; then
        PASS=$((PASS + 1))
    else
        FAIL_DIFF=$((FAIL_DIFF + 1))
        FAILS="$FAILS $name(diff)"
    fi
done

echo "PASS=$PASS FAIL=$((FAIL_EMIT + FAIL_LINK + FAIL_DIFF))  (emit=$FAIL_EMIT link=$FAIL_LINK diff=$FAIL_DIFF)"
[ -n "$FAILS" ] && echo "FAILS:$FAILS"
[ "$((FAIL_EMIT + FAIL_LINK + FAIL_DIFF))" -eq 0 ]
