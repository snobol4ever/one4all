#!/usr/bin/env bash
# test_smoke_jit_emit_x64.sh — gate for GOAL-MODE4-EMIT EM-1..EM-4
#
# Single growing gate.  Each EM-N rung extends the test set; previous
# rungs' invariants still hold.
#
# EM-1 contract (wiring + libscrip_rt.so skeleton):
#   - --jit-emit + --x64 flag pair selects asm emission to stdout.
#   - All three flag-validation paths error correctly.
#   - Bare emit (any program) produces asm with a main: label and
#     PLT calls into rt_init / rt_finalize.
#
# EM-2 contract (SM_HALT + SM_PUSH_LIT_I codegen):
#   - Synthetic SM_PUSH_LIT_I 42 + SM_HALT program emits, links,
#     runs, exits with rc=42 — proves PUSH_LIT_I + HALT codegen,
#     SM stack push/pop in libscrip_rt.so, halt-rc surfacing
#     through finalize.
#   - Synthetic program containing an opcode the emitter does not yet
#     bake (SM_CONCAT) emits, links, runs, and aborts loudly with the
#     unhandled-op trap diagnostic on stderr.
#   - Real-frontend program (`OUTPUT = "hi"`) emits successfully (the
#     emit itself returns 0); the resulting asm assembles cleanly.
#     Runtime behavior of the real-frontend binary is not checked
#     here — most ops are still unhandled in EM-2; that's expected
#     and verified by the synthetic unhandled-op test above.
#
# EM-3 contract (typed stack + arithmetic):
#   - Synthetic (2+3)*4 program emits, links, runs, exits rc=20.
#
# EM-4 contract (control flow):
#   - 6a: synthetic 15-op program exercises SM_JUMP forward (skip dead
#     code), SM_JUMP_F not-taken (last_ok=1 default), SM_JUMP_S taken
#     (last_ok=1 default).  Expected rc=42; asm shape verified
#     (jmp/jz/jnz forms all present).
#   - 6b: synthetic 6-op countdown body uses SM_JUMP_F backward.  A
#     thin C driver overrides rt_last_ok to return 0 twice then
#     1, proving the backward branch lands on its .L<top> label.
#     Expected rc=0.
#
# EM-5+ rungs add tests in this file; the runtime test for the real
# frontend grows as more opcodes leave the unhandled set.
#
# Idempotent.  Safe to run multiple times.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIP="$ROOT/scrip"
RT_SO="$ROOT/out/libscrip_rt.so"
HARNESS="$ROOT/out/sm_codegen_x64_emit_test"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$SCRIP" ]   || { echo "FAIL scrip not built — run scripts/build_scrip.sh"; exit 1; }
[ -f "$RT_SO" ]   || { echo "FAIL libscrip_rt.so not built — run: make libscrip_rt"; exit 1; }
[ -x "$HARNESS" ] || { echo "FAIL test harness not built — run: make out/sm_codegen_x64_emit_test"; exit 1; }

# ── Test 1: SM_PUSH_LIT_I 42 + SM_HALT → rc=42 ─────────────────────────
"$HARNESS" "$TMP/em2_a.s" >/dev/null
gcc -no-pie "$TMP/em2_a.s" \
    -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/em2_a" 2> "$TMP/em2_a.err" || {
    echo "FAIL em2_a link"; cat "$TMP/em2_a.err"; exit 1; }
set +e
"$TMP/em2_a"
RC=$?
set -e
if [ "$RC" -ne 42 ]; then
    echo "FAIL em2_a expected rc=42 got rc=$RC"; exit 1
fi
# Asm-content sanity: PUSH_LIT_I and HALT emitter blocks both present.
# Post EM-7c-sm-macros: SM opcodes emit as macro calls.  The `movabs rdi, val`
# instruction lives inside the .macro SM_PUSH_INT body (emitted at top of
# every .s by sm_emit_macro_library); the call site reads `SM_PUSH_INT 42`.
# Both are verified below — body proves the encoding; call-site proves the
# dispatcher routed through the template.
grep -qE '^(\.L[0-9]+:)?[[:space:]]+PUSH_INT[[:space:]]+42\b' "$TMP/em2_a.s" || { echo "FAIL no PUSH_INT 42 macro call"; exit 1; }
grep -q "rt_push_int@PLT"       "$ROOT/sm_macros.s" || { echo "FAIL no push_int call in sm_macros.s"; exit 1; }
grep -q "rt_halt_tos@PLT"       "$ROOT/sm_macros.s" || { echo "FAIL no halt_tos call in sm_macros.s"; exit 1; }
echo "  PASS PUSH_LIT_I+HALT  (rc=42; emit shape correct)"

# ── Test 2: unhandled-op trap fires on SM_SUSPEND ─────────────────────────
# Build a tiny inline harness that emits SM_PUSH_LIT_I + SM_SUSPEND + SM_HALT.
# Run; expect non-zero rc + diagnostic on stderr.
# SM_SUSPEND has no case in the emit_walk_codegen switch → hits edp4_sm_unhandled
# which emits UNHANDLED <op> → calls rt_unhandled_op() → prints diagnostic + abort().
cat > "$TMP/unh.c" <<'CEOF'
#include "sm_prog.h"
#include "sm_codegen_x64_emit.h"
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    SM_Program *p = sm_prog_new();
    sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit(p, SM_SUSPEND);
    sm_emit(p, SM_HALT);
    FILE *f = fopen(argv[1], "w");
    sm_codegen_x64_emit(p, f, NULL);
    fclose(f);
    sm_prog_free(p);
    return 0;
}
CEOF
gcc -O0 -g -w -I"$ROOT/src" -I"$ROOT/src/runtime/x86" -I"$ROOT/src/runtime" \
    "$TMP/unh.c" \
    "$ROOT/src/runtime/x86/sm_codegen_x64_emit.c" \
    "$ROOT/src/runtime/x86/sm_emit_template.c" \
    "$ROOT/src/runtime/x86/sm_prog.c" \
    -L"$ROOT/out" -lscrip_rt -lgc -lm -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/unh_emitter" 2> "$TMP/unh_build.err" || {
    echo "FAIL unhandled-op harness build"; cat "$TMP/unh_build.err"; exit 1; }
"$TMP/unh_emitter" "$TMP/unh.s" >/dev/null
gcc -no-pie "$TMP/unh.s" \
    -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/unh_prog" 2> "$TMP/unh_link.err" || {
    echo "FAIL unhandled-op link"; cat "$TMP/unh_link.err"; exit 1; }
set +e
"$TMP/unh_prog" 2> "$TMP/unh.err"
RC=$?
set -e
if [ "$RC" -eq 0 ]; then
    echo "FAIL unhandled-op program should abort, got rc=0"; exit 1
fi
grep -q "unhandled SM opcode" "$TMP/unh.err" || {
    echo "FAIL no unhandled-op diagnostic on stderr"; cat "$TMP/unh.err"; exit 1; }
echo "  PASS UNHANDLED_OP trap (rc=$RC; diagnostic present)"

# ── Test 3: real-frontend emit returns 0 + asm assembles ───────────────
cat > "$TMP/real.sno" <<'EOF'
	OUTPUT = "hi"
END
EOF
"$SCRIP" --jit-emit --x64 "$TMP/real.sno" > "$TMP/real.s" 2> "$TMP/real_emit.err" || {
    echo "FAIL real-frontend emit"; cat "$TMP/real_emit.err"; exit 1; }
[ -s "$TMP/real.s" ] || { echo "FAIL real-frontend emit produced empty asm"; exit 1; }
# Just assemble (don't link or run — it'd hit unhandled-op as expected).
gcc -c "$TMP/real.s" -o "$TMP/real.o" 2> "$TMP/real_as.err" || {
    echo "FAIL real-frontend asm doesn't assemble"; cat "$TMP/real_as.err"; exit 1; }
echo "  PASS real frontend  (emit ok; asm assembles)"

# ── Test 4: EM-1 wiring still solid (regression guard) ────────────────
# All three flag-validation paths should still error out as in EM-1.
set +e
"$SCRIP" --jit-emit "$TMP/real.sno" >/dev/null 2>&1
[ $? -eq 1 ] || { echo "FAIL bare --jit-emit should error"; exit 1; }
"$SCRIP" --x64 "$TMP/real.sno" >/dev/null 2>&1
[ $? -eq 1 ] || { echo "FAIL bare --x64 should error"; exit 1; }
"$SCRIP" --jit-emit --x64 --sm-run "$TMP/real.sno" >/dev/null 2>&1
[ $? -eq 1 ] || { echo "FAIL mutex with --sm-run should error"; exit 1; }
set -e
echo "  PASS EM-1 errors    (flag validation regression-clean)"


# -- Test 5: EM-3 gate -- (2 + 3) * 4 = 20 ----------------------------------
# Build the EM-3 synthetic program (6-op: PUSH 2, PUSH 3, ADD, PUSH 4, MUL, HALT).
# The harness now accepts up to four asm output paths;
#   argv[1]=EM-2, argv[2]=EM-3, argv[3]=EM-4a (forward+conditional shapes),
#   argv[4]=EM-4b (backward-loop body, driven by override below).
"$HARNESS" "$TMP/em2_a.s" "$TMP/em3.s" "$TMP/em4a.s" "$TMP/em4b.s" "$TMP/em5.s" "$TMP/em5b.s" >/dev/null
gcc -no-pie "$TMP/em3.s" \
    -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/em3_prog" 2> "$TMP/em3_link.err" || {
    echo "FAIL em3 link"; cat "$TMP/em3_link.err"; exit 1; }
set +e
"$TMP/em3_prog"
RC=$?
set -e
if [ "$RC" -ne 20 ]; then
    echo "FAIL em3 expected rc=20 got rc=$RC"; exit 1
fi
# Asm-content sanity: each arithmetic op is its own named no-arg macro now
# (EM-7c-bb-three-column follow-up: ADD_NUM, SUB_NUM, MUL_NUM, DIV_NUM,
# MOD_NUM — opcode name lives in col 2 directly, no opaque "ARITH 17 #
# SM_ADD" form).  Suffix `_NUM` avoids collision with x86 add/sub/mul/div
# mnemonics (GAS macro-name match is case-insensitive).
# The em3 program is (2+3)*4 — must contain at least ADD_NUM and MUL_NUM.
grep -qE '^(\.L[0-9]+:)?[[:space:]]+ADD_NUM\b' "$TMP/em3.s" || { echo "FAIL no ADD_NUM macro call in em3 asm"; exit 1; }
grep -qE '^(\.L[0-9]+:)?[[:space:]]+MUL_NUM\b' "$TMP/em3.s" || { echo "FAIL no MUL_NUM macro call in em3 asm"; exit 1; }
grep -q "rt_arith@PLT" "$ROOT/sm_macros.s" || { echo "FAIL no arith PLT call in sm_macros.s"; exit 1; }
echo "  PASS EM-3 arithmetic  ((2+3)*4=20; emit->link->run verified)"

# -- Test 6a: EM-4 forward jump + conditional shapes -----------------------
# Synthetic 15-op program: SM_JUMP forward over dead code, then
# SM_JUMP_F (not taken since last_ok=1) and SM_JUMP_S (taken since last_ok=1)
# steer execution to the final HALT.  Expected rc=42.
gcc -no-pie "$TMP/em4a.s" \
    -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/em4a_prog" 2> "$TMP/em4a_link.err" || {
    echo "FAIL em4a link"; cat "$TMP/em4a_link.err"; exit 1; }
set +e
"$TMP/em4a_prog"
RC=$?
set -e
if [ "$RC" -ne 42 ]; then
    echo "FAIL em4a expected rc=42 got rc=$RC"; exit 1
fi
# Asm-shape sanity: forward jmp, JUMP_F (jz) and JUMP_S (jnz) all present.
# Post EM-7c-sm-three-column: the literal `jmp/jz/jnz \tgt` instructions live
# in the .macro JUMP / JUMP_F / JUMP_S bodies in the externalised sm_macros.s
# (one copy per emit run); the dispatcher emits `JUMP .LN`, `JUMP_F .LN`,
# `JUMP_S .LN` at the call sites in three-column form.  Both forms verified.
grep -qE '^(\.L[0-9]+:)?[[:space:]]+JUMP[[:space:]]+\.L' "$TMP/em4a.s" || { echo "FAIL no JUMP .LN call in em4a asm"; exit 1; }
grep -qE '^(\.L[0-9]+:)?[[:space:]]+JUMP_F[[:space:]]+\.L' "$TMP/em4a.s" || { echo "FAIL no JUMP_F .LN call in em4a asm"; exit 1; }
grep -qE '^(\.L[0-9]+:)?[[:space:]]+JUMP_S[[:space:]]+\.L' "$TMP/em4a.s" || { echo "FAIL no JUMP_S .LN call in em4a asm"; exit 1; }
grep -q "rt_last_ok@PLT" "$ROOT/sm_macros.s" || { echo "FAIL no last_ok call in sm_macros.s"; exit 1; }
# Macro-body instructions: confirm the .macro definitions actually emitted
# the conditional branches with \tgt as the target operand.
grep -qE 'jz[[:space:]]+\\tgt'  "$ROOT/sm_macros.s" || { echo "FAIL no 'jz \\\\tgt' in JUMP_F macro body"; exit 1; }
grep -qE 'jnz[[:space:]]+\\tgt' "$ROOT/sm_macros.s" || { echo "FAIL no 'jnz \\\\tgt' in JUMP_S macro body"; exit 1; }
echo "  PASS EM-4a control flow (forward JUMP + JUMP_F not-taken + JUMP_S taken; rc=42)"

# -- Test 6b: EM-4 conditional backward loop ------------------------------
# 6-op SM body: counter=3, decrement, JUMP_F backward.  The driver below
# overrides rt_last_ok to return 0 twice (loop iterates 2x: 3->2, 2->1)
# then 1 (exit; counter -=1 -> 0; HALT rc=0).  Proves the JUMP_F backward
# branch lands on the .L<top> label correctly.
cat > "$TMP/em4b_driver.c" <<'CEOF'
/* EM-4b loop driver: rt_last_ok override drives the backward loop.
 * Defined here so the linker resolves it before falling back to libscrip_rt.so. */
static int call_count = 0;
int rt_last_ok(void) {
    call_count++;
    return (call_count >= 3) ? 1 : 0;
}
CEOF
gcc -no-pie "$TMP/em4b.s" "$TMP/em4b_driver.c" \
    -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/em4b_prog" 2> "$TMP/em4b_link.err" || {
    echo "FAIL em4b link"; cat "$TMP/em4b_link.err"; exit 1; }
set +e
"$TMP/em4b_prog"
RC=$?
set -e
if [ "$RC" -ne 0 ]; then
    echo "FAIL em4b expected rc=0 got rc=$RC"; exit 1
fi
# Asm-shape sanity: the backward jz must target the loop-top label (.L1).
# Post EM-7c-sm-three-column: the call site emits `.LN: JUMP_F .L1` (the
# dispatcher's encoding of "jump-to-pc-1-on-failure"); the literal `jz \tgt`
# lives in the .macro JUMP_F body in sm_macros.s and resolves at assembly time.
grep -qE '^(\.L[0-9]+:)?[[:space:]]+JUMP_F[[:space:]]+\.L1\b' "$TMP/em4b.s" || { echo "FAIL no backward JUMP_F .L1 in em4b"; exit 1; }
echo "  PASS EM-4b backward loop (JUMP_F backward x2, fallthrough; rc=0)"

# -- Test 7a: EM-5 expression call/return -- two expressions calling each other -------
# Outer expression_A calls inner expression_B (returns 7), adds 6, returns 13.
# main calls expression_A and HALTs with the returned value.  Proves the
# baked-direct call/ret discipline composes across nested expressions.
gcc -no-pie "$TMP/em5.s" \
    -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/em5_prog" 2> "$TMP/em5_link.err" || {
    echo "FAIL em5 link"; cat "$TMP/em5_link.err"; exit 1; }
set +e
"$TMP/em5_prog"
RC=$?
set -e
if [ "$RC" -ne 13 ]; then
    echo "FAIL em5 expected rc=13 got rc=$RC"; exit 1
fi
# Asm-shape sanity: RETURN bakes direct ret; CALL_EXPRESSION bakes
# direct call to a .LN target -- no PLT call for either opcode.
# Post EM-7c-sm-three-column: RETURN's `ret` and CALL_EXPRESSION's `call \tgt`
# live in the .macro bodies in sm_macros.s; the dispatcher emits
# `.LN: RETURN` (no args) and `.LN: CALL_EXPRESSION .LM` at the call sites.
grep -qE '^(\.L[0-9]+:)?[[:space:]]+RETURN\b'                "$TMP/em5.s" || { echo "FAIL no RETURN call in em5"; exit 1; }
grep -qE '^(\.L[0-9]+:)?[[:space:]]+CALL_EXPRESSION[[:space:]]+\.L' "$TMP/em5.s" || { echo "FAIL no CALL_EXPRESSION .LN call in em5"; exit 1; }
# Macro-body instructions: confirm the templates emitted the right bytes.
# `call \tgt` lives in the .macro CALL_EXPRESSION body; `ret` lives in RETURN.
grep -qE 'call[[:space:]]+\\tgt' "$ROOT/sm_macros.s" || { echo "FAIL no 'call \\\\tgt' in CALL_EXPRESSION macro body"; exit 1; }
grep -qE '^[[:space:]]+ret\b' "$ROOT/sm_macros.s" || { echo "FAIL no native ret in sm_macros.s"; exit 1; }
echo "  PASS EM-5a expression call/return  (expression_A -> expression_B; nested rc=13)"

# -- Test 7b: EM-5 SM_PUSH_EXPRESSION descriptor round-trip ----------------------
# PUSH_EXPRESSION (entry_pc=99, arity=2) then POP it; then PUSH_LIT_I 21 + HALT.
# Proves rt_push_expression_descr@PLT round-trips without corrupting
# the SM stack.
gcc -no-pie "$TMP/em5b.s" \
    -L"$ROOT/out" -lscrip_rt -Wl,-rpath,"$ROOT/out" \
    -o "$TMP/em5b_prog" 2> "$TMP/em5b_link.err" || {
    echo "FAIL em5b link"; cat "$TMP/em5b_link.err"; exit 1; }
set +e
"$TMP/em5b_prog"
RC=$?
set -e
if [ "$RC" -ne 21 ]; then
    echo "FAIL em5b expected rc=21 got rc=$RC"; exit 1
fi
grep -q "PUSH_EXPRESSION"                "$TMP/em5b.s" || { echo "FAIL no PUSH_EXPRESSION marker"; exit 1; }
grep -q "rt_push_expression_descr@PLT" "$ROOT/sm_macros.s" || { echo "FAIL no descriptor-push PLT call in sm_macros.s"; exit 1; }
echo "  PASS EM-5b push-expression descr   (PUSH_EXPRESSION + POP round-trip; rc=21)"

# ── Test 10: RETIRED in EM-7-revert (session #72) ─────────────────────────────
# (reserved slot — see above for rationale)

# ── Test 11: EM-7a Phase-2 simulator unit test ────────────────────────────────
SIM_TEST="$ROOT/out/sm_phase2_sim_test"
if [ ! -x "$SIM_TEST" ]; then
    echo "FAIL sm_phase2_sim_test not built — run: make out/sm_phase2_sim_test"; exit 1
fi
SIM_OUT=$("$SIM_TEST" 2>&1)
echo "$SIM_OUT" | grep -q "^PASS=25 FAIL=0" || { echo "FAIL EM-7a sim test"; echo "$SIM_OUT"; exit 1; }
echo "  PASS EM-7a Phase-2 sim       (PASS=25: PATND_t reconstruction + flat_is_eligible_node)"

# ── Test 12: EM-7b bb_build_flat_text — TEXT-mode flat emit + external labels ──
FLAT_TEXT_TEST="$ROOT/out/bb_flat_text_test"
if [ ! -x "$FLAT_TEXT_TEST" ]; then
    echo "FAIL bb_flat_text_test not built — run: make out/bb_flat_text_test"; exit 1
fi
# Run the unit test (writes .s, verifies labels, checks 15 internal asserts)
"$FLAT_TEXT_TEST" "$TMP/em7b.s" 2> "$TMP/em7b.err" || {
    echo "FAIL EM-7b unit test"; cat "$TMP/em7b.err"; exit 1; }
grep -q "^PASS=18 FAIL=0" "$TMP/em7b.err" || {
    echo "FAIL EM-7b internal pass count"; cat "$TMP/em7b.err"; exit 1; }
# Independently verify the .s assembles cleanly and produces the four
# externally-visible α/β/γ/ω globals.
gcc -c "$TMP/em7b.s" -o "$TMP/em7b.o" 2> "$TMP/em7b.as_err" || {
    echo "FAIL EM-7b .s does not assemble"; cat "$TMP/em7b.as_err"; exit 1; }
SYMS=$(objdump -t "$TMP/em7b.o" 2>/dev/null | awk '/pat_inv_42_0_/{print $NF}' | grep -v '_α_body$' | sort)
EXPECT=$(printf "pat_inv_42_0_α\npat_inv_42_0_β\npat_inv_42_0_γ\npat_inv_42_0_ω")
[ "$SYMS" = "$EXPECT" ] || {
    echo "FAIL EM-7b external labels missing or extra"; echo "got: $SYMS"; echo "expect: $EXPECT"; exit 1; }
# Verify all four entry labels are GLOBAL (not local) — `g` flag in objdump column 2.
# (`_α_body` is internal, may be local; we filter it before counting.)
GLOBAL_COUNT=$(objdump -t "$TMP/em7b.o" 2>/dev/null | awk '/pat_inv_42_0_/ && !/_α_body/ && $2 ~ /g/ {n++} END{print n+0}')
[ "$GLOBAL_COUNT" = "4" ] || {
    echo "FAIL EM-7b only $GLOBAL_COUNT/4 entry labels are global"; exit 1; }
echo "  PASS EM-7b bb_flat TEXT mode (PASS=18 unit + .s assembles + 4/4 external α/β/γ/ω)"

# ── Test 13: EM-7c invariant-pattern blob emit shape ───────────────────────────
# Verifies that an invariant pattern statement produces:
#   1. a `pat_inv_<id>_α/_β/_γ/_ω` block in the emitted `.s`,
#   2. a `rt_match_blob@PLT` call at the SM_EXEC_STMT site,
#   3. an `.s` that assembles AND links cleanly against libscrip_rt.so.
# Runtime correctness of the linked binary is NOT checked here — bb_flat.c
# bakes process-address imm64s into its leaf instructions (Σ/Δ/Σlen/lit/memcmp),
# valid only inside the in-process JIT (mode 3); a follow-up rung
# (EM-7c-symbolic) routes those through symbolic references that the
# dynamic linker resolves, at which point this test grows a runtime check.
cat > "$TMP/em7c_inv.sno" <<'EOF'
        S = 'abc'
        S 'b' = 'X'
        OUTPUT = S
END
EOF
"$SCRIP" --jit-emit --x64 "$TMP/em7c_inv.sno" > "$TMP/em7c.s" 2> "$TMP/em7c.err" || {
    echo "FAIL EM-7c emit"; cat "$TMP/em7c.err"; exit 1; }
grep -q "^pat_inv_0_α:"     "$TMP/em7c.s" || {
    echo "FAIL EM-7c no pat_inv_0_α label"; exit 1; }
grep -q "^pat_inv_0_β:"      "$TMP/em7c.s" || {
    echo "FAIL EM-7c no pat_inv_0_β label"; exit 1; }
grep -q "rt_match_blob@PLT" "$TMP/em7c.s" || {
    echo "FAIL EM-7c no rt_match_blob@PLT call"; exit 1; }
gcc -c "$TMP/em7c.s" -o "$TMP/em7c.o" 2> "$TMP/em7c.as_err" || {
    echo "FAIL EM-7c .s does not assemble"; cat "$TMP/em7c.as_err"; exit 1; }
gcc -no-pie "$TMP/em7c.o" -L"$ROOT/out" -lscrip_rt -lgc -lm \
    -Wl,-rpath,"$ROOT/out" -o "$TMP/em7c_bin" 2> "$TMP/em7c.ld_err" || {
    echo "FAIL EM-7c link"; cat "$TMP/em7c.ld_err"; exit 1; }
# EM-7c-symbolic-runtime-correctness: run the linked binary and verify
# output matches --jit-run (the mode-3 oracle for mode-4).
EM7C_GOT=$("$TMP/em7c_bin" < /dev/null 2>/dev/null)
EM7C_WANT=$("$SCRIP" --jit-run "$TMP/em7c_inv.sno" < /dev/null 2>/dev/null)
[ "$EM7C_GOT" = "$EM7C_WANT" ] || {
    echo "FAIL EM-7c runtime: got='$EM7C_GOT' want='$EM7C_WANT' (mode-3 oracle)"; exit 1; }
echo "  PASS EM-7c invariant blob   (Phase-2 → bb_build_flat_text → match_blob; .s assembles + links + runtime output='$EM7C_GOT')"

# ── Test 14: EM-7c-sm-three-column-verify audit ──────────────────────────────────
# Validates the three-column shape invariants (I1 no bare ';', I2 no stray
# TAB, I3 col-1 'label:' shape, I5 col-2 token <=16 w/col-3) on every line
# of every tracked artifact in corpus/programs/snobol4/demo/.
#
# Uses the --audit mode of the same harness binary.  The harness exits 0
# only if every line in every file is clean.  Tracked artifacts are
# regenerated by this test (writes through scrip --jit-emit --x64) so the
# audit always reflects the current emitter, not a stale checkout copy.
DEMO="/home/claude/corpus/programs/snobol4/demo"
if [ -d "$DEMO" ]; then
    # Regenerate from $DEMO as cwd so the emitter's "source-file: ..."
    # banner records the relative path (e.g. 'roman.sno') and not the
    # absolute path.  The corpus baseline was built from cwd-relative
    # invocations; preserving that here keeps the diff to source-side
    # rung work, never introducing path-noise into corpus.
    (cd "$DEMO" && for f in roman wordcount claws5 treebank-list treebank-array; do
        "$SCRIP" --jit-emit --x64 "$f.sno" > "$f.s" 2>/dev/null || {
            echo "FAIL EM-7c-audit: regen $f.s failed"; exit 1; }
    done)
    set +e
    "$HARNESS" --audit \
        "$DEMO/roman.s" \
        "$DEMO/wordcount.s" \
        "$DEMO/claws5.s" \
        "$DEMO/treebank-list.s" \
        "$DEMO/treebank-array.s" \
        "$DEMO/sm_macros.s" > "$TMP/audit.out" 2> "$TMP/audit.err"
    AUDIT_RC=$?
    set -e
    if [ "$AUDIT_RC" -ne 0 ]; then
        echo "FAIL EM-7c-audit: three-column shape violations found"
        head -30 "$TMP/audit.err"
        echo "---"
        tail -10 "$TMP/audit.out"
        exit 1
    fi
    echo "  PASS EM-7c-audit  (three-column shape clean across 6 tracked artifacts)"
else
    echo "  SKIP EM-7c-audit  (corpus not present at $DEMO)"
fi

echo
echo "PASS=13 FAIL=0  (EM-1 wiring + EM-2 HALT/PUSH_LIT_I + EM-3 stack ops + arithmetic + EM-4 control flow + EM-5 expressions; EM-6 retired; EM-7a Phase-2 sim; EM-7b bb_flat TEXT mode; EM-7c invariant blob emit + runtime correctness; EM-7c-sm-three-column-verify audit)"
