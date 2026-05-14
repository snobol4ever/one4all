/*
 * sm_interp_test.c — M-SCRIP-U2 unit test
 *
 * Tests the SM_Program builder and C dispatch loop in isolation.
 * Uses a stub NV table (no full SNOBOL4 runtime required).
 *
 * Compile (from src/runtime/sm/):
 *   gcc -O0 -g -I. -I../.. -I../../runtime -I../../runtime/snobol4 \
 *       sm_prog.c sm_interp.c sm_interp_test.c -lm -o sm_interp_test
 * Run: ./sm_interp_test
 */

#include "sm_prog.h"
#include "sm_interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ── Minimal runtime stubs (no full snobol4.c needed) ─────────────── */
/* When compiled with -DFULL_RUNTIME_LINKED these stubs are omitted and
 * the linker uses the real snobol4.c implementations instead. */
#ifndef FULL_RUNTIME_LINKED

/* Simple NV store: 16 slots */
#define NV_MAX 16
static struct { char name[64]; DESCR_t val; } nv_store[NV_MAX];
static int nv_count = 0;

DESCR_t NV_GET_fn(const char *name) {
    for (int i = 0; i < nv_count; i++)
        if (strcmp(nv_store[i].name, name) == 0) return nv_store[i].val;
    return NULVCL;
}
DESCR_t NV_SET_fn(const char *name, DESCR_t val) {  /* RT-5 */
    for (int i = 0; i < nv_count; i++)
        if (strcmp(nv_store[i].name, name) == 0) { nv_store[i].val = val; return val; }
    if (nv_count < NV_MAX) {
        strncpy(nv_store[nv_count].name, name, 63);
        nv_store[nv_count].val = val;
        nv_count++;
    }
    return val;  /* RT-5 */
}
void nv_reset(void) { nv_count = 0; }

int64_t to_int(DESCR_t v) {
    if (v.v == DT_I) return v.i;
    if (v.v == DT_R) return (int64_t)v.r;
    if (v.v == DT_S && v.s) return atoll(v.s);
    return 0;
}
double to_real(DESCR_t v) {
    if (v.v == DT_R) return v.r;
    if (v.v == DT_I) return (double)v.i;
    if (v.v == DT_S && v.s) return atof(v.s);
    return 0.0;
}
DESCR_t CONCAT_fn(DESCR_t a, DESCR_t b) {
    /* minimal: concat two strings */
    const char *as = (a.v == DT_S && a.s) ? a.s : "";
    const char *bs = (b.v == DT_S && b.s) ? b.s : "";
    size_t la = strlen(as), lb = strlen(bs);
    char *buf = malloc(la + lb + 1);
    memcpy(buf, as, la); memcpy(buf + la, bs, lb); buf[la+lb] = '\0';
    return STRVAL(buf);
}
DESCR_t INVOKE_fn(const char *name, DESCR_t *args, int nargs) {
    (void)name; (void)args; (void)nargs;
    return NULVCL;
}

/* CHUNKS-step17b'' (CH-17b''): stubs for the Icon frame-env forwarders.
 * The unit-test world has no Icon frame stack; report "no frame active"
 * so SM_LOAD_FRAME / SM_STORE_FRAME push FAILDESCR and clear last_ok.
 * The frame-slot opcodes are only emitted by sm_lower for expressions
 * (which the unit tests do not exercise), so this path is never hit
 * by existing tests; the stubs exist purely to satisfy the linker. */
int icn_frame_env_active(void) { return 0; }
DESCR_t icn_frame_env_load(int slot) { (void)slot; return FAILDESCR; }
void icn_frame_env_store(int slot, DESCR_t val) { (void)slot; (void)val; }

#endif /* FULL_RUNTIME_LINKED */

/* ── Test harness ───────────────────────────────────────────────────── */

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } \
    else          { fprintf(stdout, "PASS: %s\n", (msg)); } \
} while(0)

/* ── Test 1: push_i push_i add store_var halt → X == 5 ────────────── */

static void test_add_and_store(void)
{
    nv_reset();
    SM_Program *p = sm_prog_new();
    sm_emit_i(p, SM_PUSH_LIT_I, 2);
    sm_emit_i(p, SM_PUSH_LIT_I, 3);
    sm_emit(p,   SM_ADD);
    sm_emit_s(p, SM_STORE_VAR, "X");
    sm_emit(p,   SM_HALT);

    SM_State st; sm_state_init(&st);
    int r = sm_interp_run(p, &st);
    CHECK(r == 0, "test_add_and_store: run returns 0");

    DESCR_t x = NV_GET_fn("X");
    CHECK(x.v == DT_I && x.i == 5, "test_add_and_store: X == 5");

    sm_prog_free(p);
}

/* ── Test 2: push_var push_i add store_var — uses NV_GET ───────────── */

static void test_push_var(void)
{
    nv_reset();
    NV_SET_fn("Y", INTVAL(10));
    SM_Program *p = sm_prog_new();
    sm_emit_s(p, SM_PUSH_VAR,   "Y");
    sm_emit_i(p, SM_PUSH_LIT_I, 7);
    sm_emit(p,   SM_ADD);
    sm_emit_s(p, SM_STORE_VAR,  "Z");
    sm_emit(p,   SM_HALT);

    SM_State st; sm_state_init(&st);
    sm_interp_run(p, &st);
    DESCR_t z = NV_GET_fn("Z");
    CHECK(z.v == DT_I && z.i == 17, "test_push_var: Z == 17");
    sm_prog_free(p);
}

/* ── Test 3: integer multiply ──────────────────────────────────────── */

static void test_mul(void)
{
    nv_reset();
    SM_Program *p = sm_prog_new();
    sm_emit_i(p, SM_PUSH_LIT_I, 6);
    sm_emit_i(p, SM_PUSH_LIT_I, 7);
    sm_emit(p,   SM_MUL);
    sm_emit_s(p, SM_STORE_VAR, "R");
    sm_emit(p,   SM_HALT);

    SM_State st; sm_state_init(&st);
    sm_interp_run(p, &st);
    DESCR_t r = NV_GET_fn("R");
    CHECK(r.v == DT_I && r.i == 42, "test_mul: R == 42");
    sm_prog_free(p);
}

/* ── Test 4: SM_JUMP unconditional ─────────────────────────────────── */

static void test_jump(void)
{
    nv_reset();
    SM_Program *p = sm_prog_new();
    /* jump over the "bad" store */
    int jmp = sm_emit_i(p, SM_JUMP, 0);          /* placeholder target */
    sm_emit_i(p, SM_PUSH_LIT_I, 999);
    sm_emit_s(p, SM_STORE_VAR, "BAD");
    int target = sm_label(p);                     /* label = index of next instr */
    sm_patch_jump(p, jmp, target);
    sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_s(p, SM_STORE_VAR, "GOOD");
    sm_emit(p,   SM_HALT);

    SM_State st; sm_state_init(&st);
    sm_interp_run(p, &st);
    DESCR_t bad  = NV_GET_fn("BAD");
    DESCR_t good = NV_GET_fn("GOOD");
    CHECK(bad.v  == DT_SNUL,              "test_jump: BAD not set (skipped)");
    CHECK(good.v == DT_I && good.i == 1,  "test_jump: GOOD == 1");
    sm_prog_free(p);
}

/* ── Test 5: SM_JUMP_S / SM_JUMP_F conditional ──────────────────────── */

static void test_conditional_jump(void)
{
    nv_reset();
    SM_Program *p = sm_prog_new();
    /* 2 + 3 succeeds → jump_s taken */
    sm_emit_i(p, SM_PUSH_LIT_I, 2);
    sm_emit_i(p, SM_PUSH_LIT_I, 3);
    sm_emit(p,   SM_ADD);
    sm_emit_s(p, SM_STORE_VAR, "SUM");
    int js = sm_emit_i(p, SM_JUMP_S, 0);    /* placeholder */
    sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_s(p, SM_STORE_VAR, "BRANCH");
    int skip = sm_emit_i(p, SM_JUMP, 0);
    int s_label = sm_label(p);
    sm_patch_jump(p, js, s_label);
    sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_s(p, SM_STORE_VAR, "BRANCH");
    int end_label = sm_label(p);
    sm_patch_jump(p, skip, end_label);
    sm_emit(p, SM_HALT);

    SM_State st; sm_state_init(&st);
    sm_interp_run(p, &st);
    DESCR_t br = NV_GET_fn("BRANCH");
    CHECK(br.v == DT_I && br.i == 1, "test_conditional_jump: success branch taken");
    sm_prog_free(p);
}

/* ── Test 6: string concat ──────────────────────────────────────────── */

static void test_concat(void)
{
    nv_reset();
    SM_Program *p = sm_prog_new();
    sm_emit_s(p, SM_PUSH_LIT_S, "hello ");
    sm_emit_s(p, SM_PUSH_LIT_S, "world");
    sm_emit(p,   SM_CONCAT);
    sm_emit_s(p, SM_STORE_VAR, "MSG");
    sm_emit(p,   SM_HALT);

    SM_State st; sm_state_init(&st);
    sm_interp_run(p, &st);
    DESCR_t msg = NV_GET_fn("MSG");
    CHECK(msg.v == DT_S && msg.s && strcmp(msg.s, "hello world") == 0,
          "test_concat: MSG == 'hello world'");
    sm_prog_free(p);
}

/* ── Test 7: SM_INCR / SM_DECR ──────────────────────────────────────── */

static void test_incr_decr(void)
{
    nv_reset();
    SM_Program *p = sm_prog_new();
    sm_emit_i(p, SM_PUSH_LIT_I, 10);
    sm_emit_i(p, SM_INCR, 5);
    sm_emit_s(p, SM_STORE_VAR, "A");
    sm_emit_i(p, SM_PUSH_LIT_I, 10);
    sm_emit_i(p, SM_DECR, 3);
    sm_emit_s(p, SM_STORE_VAR, "B");
    sm_emit(p,   SM_HALT);

    SM_State st; sm_state_init(&st);
    sm_interp_run(p, &st);
    DESCR_t a = NV_GET_fn("A");
    DESCR_t b = NV_GET_fn("B");
    CHECK(a.v == DT_I && a.i == 15, "test_incr_decr: A == 15");
    CHECK(b.v == DT_I && b.i ==  7, "test_incr_decr: B == 7");
    sm_prog_free(p);
}

/* ── CHUNKS-step14: generator test ──────────────────────────────────── */
/* Hand-built SM program:
 *   SM_PUSH_LIT_I 10     ; yield value 10
 *   SM_SUSPEND
 *   SM_PUSH_LIT_I 20     ; yield value 20
 *   SM_SUSPEND
 *   SM_PUSH_LIT_I 30     ; yield value 30
 *   SM_SUSPEND
 *   SM_RETURN             ; generator exhausted — no more values
 *
 * bb_broker_drive_sm should call body_fn 3 times with 10, 20, 30.
 */

static int64_t gen_collected[8];
static int     gen_count = 0;

static void collect_gen_val(DESCR_t val, void *arg)
{
    (void)arg;
    if (gen_count < 8) gen_collected[gen_count++] = (val.v == DT_I) ? val.i : -999;
}

static void test_generator_suspend_resume(void)
{
    printf("--- test_generator_suspend_resume ---\n");
    nv_reset();
    gen_count = 0;

    /* Build the generator SM program */
    SM_Program *p = sm_prog_new();
    sm_emit_i(p, SM_PUSH_LIT_I, 10);
    sm_emit(p, SM_SUSPEND);
    sm_emit_i(p, SM_PUSH_LIT_I, 20);
    sm_emit(p, SM_SUSPEND);
    sm_emit_i(p, SM_PUSH_LIT_I, 30);
    sm_emit(p, SM_SUSPEND);
    sm_emit(p, SM_RETURN);

    /* Point the global current prog at this test program */
    SM_Program *saved_prog = g_current_sm_prog;
    g_current_sm_prog = p;

    /* Drive it */
    SmGenState *gs   = sm_gen_state_new(0);
    int         ticks = bb_broker_drive_sm(gs, collect_gen_val, NULL);

    g_current_sm_prog = saved_prog;

    CHECK(ticks == 3, "generator: tick count == 3");
    CHECK(gen_count == 3, "generator: body_fn called 3 times");
    CHECK(gen_collected[0] == 10, "generator: first yield == 10");
    CHECK(gen_collected[1] == 20, "generator: second yield == 20");
    CHECK(gen_collected[2] == 30, "generator: third yield == 30");

    /* Re-drive an exhausted generator: must return 0 */
    gen_count = 0;
    int ticks2 = bb_broker_drive_sm(gs, collect_gen_val, NULL);
    CHECK(ticks2 == 0, "generator: re-drive exhausted gen returns 0");
    CHECK(gen_count == 0, "generator: body_fn not called after exhaustion");

    sm_prog_free(p);
}

/* CHUNKS-step14b: gen-local slot survival across SUSPEND.
 * Build an expression that initializes locals[0]=100, yields, then on each resume
 * loads locals[0], increments, stores back, yields.  Three yields expected:
 * 100, 101, 102.  Proves the locals[] array in SmGenState survives across
 * SUSPEND/RESUME boundaries — the foundation for TT_TO/TT_TO_BY in CHUNKS-step15a. */
static void test_gen_locals_survive_suspend(void)
{
    printf("--- test_gen_locals_survive_suspend ---\n");
    nv_reset();
    gen_count = 0;

    SM_Program *p = sm_prog_new();
    sm_emit_i(p, SM_PUSH_LIT_I, 100);
    sm_emit_i(p, SM_STORE_GLOCAL, 0);   /* locals[0] = 100; leaves 100 on TOS */
    sm_emit(p, SM_SUSPEND);             /* yield 100 */

    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_INCR, 1);
    sm_emit_i(p, SM_STORE_GLOCAL, 0);   /* locals[0] = 101 */
    sm_emit(p, SM_SUSPEND);             /* yield 101 */

    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_INCR, 1);
    sm_emit_i(p, SM_STORE_GLOCAL, 0);   /* locals[0] = 102 */
    sm_emit(p, SM_SUSPEND);             /* yield 102 */

    sm_emit(p, SM_RETURN);

    SM_Program *saved_prog = g_current_sm_prog;
    g_current_sm_prog = p;

    SmGenState *gs    = sm_gen_state_new(0);
    int         ticks = bb_broker_drive_sm(gs, collect_gen_val, NULL);

    g_current_sm_prog = saved_prog;

    CHECK(ticks == 3, "gen-locals: tick count == 3");
    CHECK(gen_count == 3, "gen-locals: body_fn called 3 times");
    CHECK(gen_collected[0] == 100, "gen-locals: first yield == 100");
    CHECK(gen_collected[1] == 101, "gen-locals: second yield == 101 (locals survived)");
    CHECK(gen_collected[2] == 102, "gen-locals: third yield == 102 (locals survived twice)");

    sm_prog_free(p);
}

/* CHUNKS-step14b: gen-locals are private to each SmGenState invocation.
 * Two independent generators using locals[0] must not interfere — even
 * when they share the same entry_pc.  Drive gen_a fully, then drive
 * gen_b fully on the same expression; each must see its own private 100/101/102.
 * (Sequential, not concurrent — but both allocate their own SmGenState.) */
static void test_gen_locals_isolated_per_invocation(void)
{
    printf("--- test_gen_locals_isolated_per_invocation ---\n");
    nv_reset();

    SM_Program *p = sm_prog_new();
    sm_emit_i(p, SM_PUSH_LIT_I, 100);
    sm_emit_i(p, SM_STORE_GLOCAL, 0);
    sm_emit(p, SM_SUSPEND);
    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_INCR, 1);
    sm_emit_i(p, SM_STORE_GLOCAL, 0);
    sm_emit(p, SM_SUSPEND);
    sm_emit(p, SM_RETURN);

    SM_Program *saved_prog = g_current_sm_prog;
    g_current_sm_prog = p;

    /* First invocation */
    gen_count = 0;
    SmGenState *gs_a = sm_gen_state_new(0);
    int ticks_a = bb_broker_drive_sm(gs_a, collect_gen_val, NULL);
    int64_t a_first  = gen_collected[0];
    int64_t a_second = gen_collected[1];

    /* Second invocation — fresh SmGenState, same expression */
    gen_count = 0;
    SmGenState *gs_b = sm_gen_state_new(0);
    int ticks_b = bb_broker_drive_sm(gs_b, collect_gen_val, NULL);
    int64_t b_first  = gen_collected[0];
    int64_t b_second = gen_collected[1];

    g_current_sm_prog = saved_prog;

    CHECK(ticks_a == 2, "gen-isolation: first invocation ticks == 2");
    CHECK(ticks_b == 2, "gen-isolation: second invocation ticks == 2");
    CHECK(a_first == 100,  "gen-isolation: a yields 100");
    CHECK(a_second == 101, "gen-isolation: a yields 101");
    CHECK(b_first == 100,  "gen-isolation: b also yields 100 (locals are fresh)");
    CHECK(b_second == 101, "gen-isolation: b also yields 101");

    sm_prog_free(p);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== SM_Program dispatch loop tests (M-SCRIP-U2) ===\n");
    test_add_and_store();
    test_push_var();
    test_mul();
    test_jump();
    test_conditional_jump();
    test_concat();
    test_incr_decr();
    test_generator_suspend_resume();
    test_gen_locals_survive_suspend();
    test_gen_locals_isolated_per_invocation();

    if (failures == 0)
        printf("\nAll tests PASSED.\n");
    else
        printf("\n%d test(s) FAILED.\n", failures);
    return failures ? 1 : 0;
}
