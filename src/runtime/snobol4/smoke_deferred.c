/*
 * smoke_deferred.c — smoke test: deferred pattern refs (*name)
 *
 * In beautiful.sno: *snoParse, *snoExpr, *snoCommand etc.
 * These are stored in the variable table as patterns, then
 * referenced via pat_ref("snoParse") at MATCH_fn time.
 *
 * This test:
 *   1. Stores a pattern in a variable
 *   2. Matches using a deferred ref to that variable
 *   3. Verifies the MATCH_fn succeeds
 */
#include <stdio.h>
#include "snobol4.h"

int main(void) {
    SNO_INIT_fn();
    int pass = 0, fail = 0;

#define CHECK(desc, cond) \
    do { if (cond) { printf("  PASS: %s\n", desc); pass++; } \
         else      { printf("  DT_FAIL: %s\n", desc); fail++; } } while(0)

    /* Store SPAN('abc') as a pattern in variable "myPat" */
    DESCR_t span_pat = pat_span("abc");
    NV_SET_fn("myPat", span_pat);

    /* Match via deferred ref *myPat */
    DESCR_t ref_pat = pat_ref("myPat");
    CHECK("deferred ref *myPat matches 'aabbcc'",
          match_pattern(ref_pat, "aabbcc"));
    CHECK("deferred ref *myPat fails on 'xyz'",
          !match_pattern(ref_pat, "xyz"));

    /* Nested: *outer = *inner, inner = LEN(3) */
    DESCR_t len3 = pat_len(3);
    NV_SET_fn("inner", len3);
    DESCR_t outer_ref = pat_ref("inner");
    NV_SET_fn("outer", outer_ref);
    DESCR_t deref2 = pat_ref("outer");
    CHECK("double deferred ref *outer → *inner → LEN(3) matches 'abc'",
          match_pattern(deref2, "abc"));
    CHECK("double deferred ref fails on 'ab' (len 2)",
          !match_pattern(deref2, "ab"));

    /* Cat of two deferred refs: *a *b */
    NV_SET_fn("a", pat_span("0123456789"));
    NV_SET_fn("b", pat_lit("x"));
    DESCR_t cat = pat_cat(pat_ref("a"), pat_ref("b"));
    CHECK("*a *b matches '123x'",   match_pattern(cat, "123x"));
    CHECK("*a *b fails on '123y'", !match_pattern(cat, "123y"));

    printf("\nDeferred ref smoke: %d pass, %d fail\n", pass, fail);
    return fail ? 1 : 0;
}
