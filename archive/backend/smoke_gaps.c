/*
 * smoke_gaps.c — Targeted smoke tests for the 3 critical gaps
 * before attempting to compile beautiful.c
 *
 * Gap 1: Pattern engine (pat_*, match_pattern, match_and_replace)
 * Gap 2: Array/Table/TREEBLK_t subscript API (array_create, subscript_get/set)
 * Gap 3: register_fn, push_val/pop_val/top_val, tree_new
 *
 * Each test is standalone. Failures pinpoint exactly what to implement.
 */

#include <stdio.h>
#include <string.h>
#include "snobol4.h"

int pass = 0, fail = 0;

#define CHECK(desc, cond) \
    do { if (cond) { printf("  PASS: %s\n", desc); pass++; } \
         else      { printf("  DT_FAIL: %s\n", desc); fail++; } } while(0)

/* =========================================================================
 * GAP 1: Pattern engine
 * ===================================================================== */

static void test_pattern_engine(void) {
    printf("\n[Gap 1: Pattern engine]\n");

    /* pat_lit + match_pattern */
    DESCR_t lit = pat_lit("hello");
    CHECK("pat_lit matches 'hello'",      match_pattern(lit, "hello"));
    CHECK("pat_lit fails on 'world'",    !match_pattern(lit, "world"));
    CHECK("pat_lit matches inside 'say hello'", match_pattern(lit, "say hello"));

    /* pat_span */
    DESCR_t sp = pat_span("abc");
    CHECK("pat_span('abc') matches 'aabbcc'", match_pattern(sp, "aabbcc"));
    CHECK("pat_span fails on 'xyz'",         !match_pattern(sp, "xyz"));

    /* pat_break_ */
    DESCR_t brk = pat_break_(".");
    CHECK("pat_break('.') matches in 'foo.bar'", match_pattern(brk, "foo.bar"));

    /* pat_len */
    DESCR_t l3 = pat_len(3);
    CHECK("pat_len(3) matches 'abc'",  match_pattern(l3, "abc"));
    CHECK("pat_len(3) matches in 'xabcy'", match_pattern(l3, "xabcy"));

    /* pat_pos + pat_rpos */
    DESCR_t p0 = pat_pos(0);
    DESCR_t rp0 = pat_rpos(0);
    DESCR_t anchored = pat_cat(p0, pat_cat(pat_lit("ab"), rp0));
    CHECK("POS(0) 'ab' RPOS(0) matches 'ab'",   match_pattern(anchored, "ab"));
    CHECK("POS(0) 'ab' RPOS(0) fails on 'xab'", !match_pattern(anchored, "xab"));

    /* pat_cat + pat_alt */
    DESCR_t cat = pat_cat(pat_lit("foo"), pat_lit("bar"));
    CHECK("cat 'foo''bar' matches 'foobar'",  match_pattern(cat, "foobar"));
    CHECK("cat 'foo''bar' fails on 'foobaz'", !match_pattern(cat, "foobaz"));

    DESCR_t alt = pat_alt(pat_lit("cat"), pat_lit("dog"));
    CHECK("alt 'cat'|'dog' matches 'cat'", match_pattern(alt, "cat"));
    CHECK("alt 'cat'|'dog' matches 'dog'", match_pattern(alt, "dog"));
    CHECK("alt 'cat'|'dog' fails on 'bird'", !match_pattern(alt, "bird"));

    /* pat_epsilon — always matches */
    DESCR_t eps = pat_epsilon();
    CHECK("epsilon always matches",  match_pattern(eps, "anything"));
    CHECK("epsilon matches empty",   match_pattern(eps, ""));

    /* pat_arbno */
    DESCR_t star = pat_arbno(pat_lit("ab"));
    CHECK("arbno('ab') matches ''",      match_pattern(star, ""));
    CHECK("arbno('ab') matches 'ababab'", match_pattern(star, "ababab"));

    /* pat_any_cs — ANY(chars) */
    DESCR_t any_vw = pat_any_cs("aeiou");
    CHECK("any('aeiou') matches 'a'", match_pattern(any_vw, "a"));
    CHECK("any('aeiou') matches 'e'", match_pattern(any_vw, "e"));
    CHECK("any('aeiou') fails on 'b'", !match_pattern(any_vw, "b"));

    /* pat_rtab */
    DESCR_t rt2 = pat_rtab(2);
    CHECK("rtab(2) on 'hello' leaves 2 chars", match_pattern(rt2, "hello"));

    /* pat_ref — deferred variable reference */
    NV_SET_fn("myPat", pat_lit("xyz"));
    DESCR_t ref = pat_ref("myPat");
    CHECK("pat_ref('myPat') matches 'xyz'",   match_pattern(ref, "xyz"));
    CHECK("pat_ref('myPat') fails on 'abc'", !match_pattern(ref, "abc"));

    /* var_as_pattern — variable holding pattern */
    DESCR_t vp = var_as_pattern(NV_GET_fn("myPat"));
    CHECK("var_as_pattern from var works", match_pattern(vp, "xyz"));

    /* pat_assign_cond — conditional capture $.var */
    DESCR_t cap_var = NULVCL;
    NV_SET_fn("captured", NULVCL);
    DESCR_t cap = pat_assign_cond(pat_span("0123456789"), NV_GET_fn("captured"));
    /* Note: assign_cond stores into the variable on MATCH_fn */
    int ok = match_pattern(
        pat_cat(pat_pos(0), pat_cat(cap, pat_rpos(0))),
        "12345"
    );
    CHECK("pat_assign_cond captures digits", ok);

    /* match_and_replace */
    DESCR_t subject = STRVAL("hello world");
    int replaced = match_and_replace(&subject, pat_lit("world"), STRVAL("there"));
    CHECK("match_and_replace replaces 'world' with 'there'", replaced);
    CHECK("match_and_replace result is 'hello there'",
          strcmp(VARVAL_fn(subject), "hello there") == 0);

    /* pat_user_call — unknown pattern function dispatched via APPLY_fn */
    /* (requires register_fn to be working) */
}

/* =========================================================================
 * GAP 2: Array/Table/TREEBLK_t subscript via DESCR_t
 * ===================================================================== */

static void test_collections(void) {
    printf("\n[Gap 2: Collections — array_create, subscript_get/set, table_new, tree_new]\n");

    /* array_create("1:4") */
    DESCR_t arr = array_create(STRVAL("1:4"));
    CHECK("array_create returns non-null", arr.v != DT_SNUL);

    subscript_set(arr, INTVAL(1), INTVAL(18));
    subscript_set(arr, INTVAL(2), INTVAL(33));
    subscript_set(arr, INTVAL(3), INTVAL(36));
    subscript_set(arr, INTVAL(4), INTVAL(81));

    DESCR_t v1 = subscript_get(arr, INTVAL(1));
    DESCR_t v4 = subscript_get(arr, INTVAL(4));
    CHECK("arr[1] == 18", to_int(v1) == 18);
    CHECK("arr[4] == 81", to_int(v4) == 81);

    /* table_new */
    DESCR_t tbl = TABLE_VAL(table_new());
    subscript_set(tbl, STRVAL("key"), STRVAL("value"));
    DESCR_t got = subscript_get(tbl, STRVAL("key"));
    CHECK("table set/get 'key'='value'", strcmp(VARVAL_fn(got), "value") == 0);

    /* tree_new — the tree DT_DATA type */
    DESCR_t t = MAKE_TREE_fn(STRVAL("snoId"),
                              STRVAL("hello"),
                              INTVAL(0),
                              NULVCL);
    CHECK("tree_new returns non-null", t.v != DT_SNUL);
    DESCR_t ttype = FIELD_GET_fn(t, "t");
    DESCR_t tval  = FIELD_GET_fn(t, "v");
    CHECK("tree t field = 'snoId'",  strcmp(VARVAL_fn(ttype), "snoId") == 0);
    CHECK("tree v field = 'hello'",  strcmp(VARVAL_fn(tval),  "hello") == 0);
}

/* =========================================================================
 * GAP 3: register_fn, push_val/pop_val/top_val
 * ===================================================================== */

static DESCR_t _test_fn(DESCR_t *args, int n) {
    if (n < 1) return INTVAL(0);
    return add(args[0], INTVAL(1));
}

static void test_registration(void) {
    printf("\n[Gap 3: register_fn + push_val/pop_val/top_val]\n");

    /* register_fn */
    register_fn("addOne", _test_fn, 1, 1);
    DESCR_t result = APPLY_fn("addOne", (DESCR_t[]){INTVAL(41)}, 1);
    CHECK("registered fn 'addOne(41)' returns 42", to_int(result) == 42);

    /* push_val / pop_val / top_val */
    DESCR_t a = STRVAL("alpha");
    DESCR_t b = STRVAL("beta");
    push_val(a);
    push_val(b);
    DESCR_t TOP_fn = top_val();
    CHECK("top_val() is 'beta'", strcmp(VARVAL_fn(TOP_fn), "beta") == 0);
    DESCR_t popped = pop_val();
    CHECK("pop_val() returns 'beta'", strcmp(VARVAL_fn(popped), "beta") == 0);
    DESCR_t popped2 = pop_val();
    CHECK("second pop_val() returns 'alpha'", strcmp(VARVAL_fn(popped2), "alpha") == 0);
}

/* =========================================================================
 * Main
 * ===================================================================== */

int main(void) {
    SNO_INIT_fn();
    printf("=== Sprint 20 Gap Smoke Tests ===\n");

    test_pattern_engine();
    test_collections();
    test_registration();

    printf("\n================================================\n");
    printf("Results: %d pass, %d fail\n", pass, fail);
    if (fail == 0) printf("ALL PASS — gaps are closed.\n");
    return fail ? 1 : 0;
}
