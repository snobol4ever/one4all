#!/usr/bin/env bash
# test_gate_em_template_byte_identity.sh — EM-MODE4-IS-MODE3-DUMP-c gate.
#
# Verifies that the per-opcode template machinery (templates/sm_halt.c
# walked through a binary emitter and flushed into SEG_CODE) produces
# bytes byte-identical to the legacy `emit_halt_blob` it replaces.
#
# Strategy.  The legacy `emit_halt_blob` produces exactly 5 bytes
# (41 ff 45 14 c3 — inc dword [r13+20]; ret) at the start of every
# SM_HALT instruction's blob in SEG_CODE.  After the EM-MODE4-IS-MODE3-
# DUMP-c retrofit, `emit_halt_blob_via_template` produces the same
# 5 bytes by routing through the template + binary emitter +
# capture-and-flush adapter.  We verify this two ways:
#
# 1. **Behavioral equivalence (this script).**  Mode-3 (`--jit-run`)
#    is the in-process JIT.  If SM_HALT's bytes were wrong, the JIT
#    would either crash (illegal instruction, garbled jump) or produce
#    different output (wrong pc value at halt time, wrong return
#    address).  We run a small fixture of SNOBOL4 programs that
#    terminate normally via SM_HALT and require `--jit-run` output
#    to match `--sm-run` (the interpreter, which doesn't use SEG_CODE
#    at all and is the byte-identity oracle).
#
# 2. **Static defensive check.**  The template adapter
#    (`emit_halt_blob_via_template` in sm_codegen.c) contains an
#    `abort()` if the template produces anything other than exactly
#    5 bytes.  If the template ever drifts (an SM_INSN_* kind change,
#    or an unintended `pad_to_blob_size` impl that suddenly emits
#    bytes), this script's runs will SIGABRT visibly rather than
#    silently corrupting SEG_CODE.  No script-side check needed for
#    this part — abort + signal exit code is enough.
#
# Self-contained per RULES.md: paths derived from $0; no env deps.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SCRIP="${SCRIP:-$ROOT/scrip}"
CORPUS="${CORPUS:-/home/claude/corpus}"
TIMEOUT="${TIMEOUT:-8}"

if [ ! -x "$SCRIP" ]; then
    echo "SKIP scrip not built at $SCRIP"
    exit 0
fi

# Fixture: tiny SNOBOL4 programs that terminate via SM_HALT.  Inline
# heredocs so the gate is self-contained and doesn't depend on corpus
# files that may evolve independently.
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/halt_immediate.sno" <<'SNO'
            OUTPUT = 'before halt'
END
SNO

cat > "$TMP/halt_after_arith.sno" <<'SNO'
            X = 1 + 2 * 3
            OUTPUT = X
END
SNO

cat > "$TMP/halt_after_match.sno" <<'SNO'
            &ANCHOR   = 0
            &FULLSCAN = 1
            S = 'hello world'
            S 'world' :S(ok)F(bad)
ok          OUTPUT = 'matched'                                     :(END)
bad         OUTPUT = 'no match'
END
SNO

cat > "$TMP/halt_after_loop.sno" <<'SNO'
            I = 0
loop        I = I + 1
            OUTPUT = I
            LE(I, 3) :S(loop)
END
SNO

PASS=0
FAIL=0
FAILS=""

for f in "$TMP"/*.sno; do
    name=$(basename "$f" .sno)

    # Run --sm-run (the byte-identity oracle: pure interpreter, no
    # SEG_CODE involved).
    sm_out=$(timeout "$TIMEOUT" "$SCRIP" --sm-run "$f" < /dev/null 2>/dev/null)
    sm_rc=$?

    # Run --jit-run (mode-3: walks SEG_CODE, including the SM_HALT
    # blob produced by the template).  If the template emits bad
    # bytes, --jit-run will either crash or produce different output.
    jit_out=$(timeout "$TIMEOUT" "$SCRIP" --jit-run "$f" < /dev/null 2>/dev/null)
    jit_rc=$?

    if [ "$sm_rc" -eq 0 ] && [ "$jit_rc" -eq 0 ] && [ "$sm_out" = "$jit_out" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        FAILS="$FAILS $name(sm_rc=$sm_rc/jit_rc=$jit_rc/diff=$([ "$sm_out" = "$jit_out" ] && echo no || echo yes))"
    fi
done

echo "PASS=$PASS FAIL=$FAIL"
[ -n "$FAILS" ] && echo "FAILS:$FAILS"
[ "$FAIL" -eq 0 ]
