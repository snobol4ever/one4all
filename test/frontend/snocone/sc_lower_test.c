/*
 * sc_lower_test.c -- Sprint SC2 quick-check tests
 *
 * M-SNOC-LOWER trigger:
 *   OUTPUT = 'hello'  ->  assignment STMT_t  with E_VART(OUTPUT) subject
 *                         and E_QLIT(hello) replacement  PASS
 *
 * Build:
 *   gcc -I src/frontend/snocone -I src/frontend/snobol4 \
 *       -o /tmp/sc_lower_test \
 *       test/frontend/snocone/sc_lower_test.c \
 *       src/frontend/snocone/sc_lex.c \
 *       src/frontend/snocone/sc_parse.c \
 *       src/frontend/snocone/sc_lower.c
 *   /tmp/sc_lower_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sc_lex.h"
#include "sc_parse.h"
#include "sc_lower.h"

/* ---------------------------------------------------------------------------
 * Minimal test harness
 * ------------------------------------------------------------------------- */
static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { fprintf(stderr, "FAIL [%d]: %s\n", __LINE__, msg); g_fail++; } \
} while(0)

/* Run the full pipeline on a source string and return the Program.
 *
 * Splits tokens at SC_NEWLINE/SC_SEMICOLON boundaries, parses each segment
 * independently, appends SC_NEWLINE separators, then lowers the combined
 * postfix stream.
 *
 * IMPORTANT: all ScParseResult objects are kept alive until AFTER
 * sc_lower() returns, because combined[] holds pointers into their
 * token text buffers.  They are freed at the end.
 */
#define MAX_SEGMENTS 64
static Program *pipeline(const char *src) {
    ScTokenArray ta = sc_lex(src);

    ScParseResult segments[MAX_SEGMENTS];
    int nseg = 0;

    int cap = ta.count * 2 + 8;
    ScPToken *combined = malloc(cap * sizeof(ScPToken));
    int ccount = 0;

    int start = 0;
    for (int i = 0; i <= ta.count; i++) {
        int end_kind = (i < ta.count) ? (int)ta.tokens[i].kind : (int)SC_EOF;
        int is_sep = (end_kind == (int)SC_NEWLINE ||
                      end_kind == (int)SC_SEMICOLON ||
                      end_kind == (int)SC_EOF);
        if (is_sep) {
            int seg_len = i - start;
            if (seg_len > 0 && nseg < MAX_SEGMENTS) {
                segments[nseg] = sc_parse(ta.tokens + start, seg_len);
                ScParseResult *pr = &segments[nseg++];
                if (ccount + pr->count + 1 >= cap) {
                    cap = (ccount + pr->count + 4) * 2;
                    combined = realloc(combined, cap * sizeof(ScPToken));
                }
                for (int k = 0; k < pr->count; k++)
                    combined[ccount++] = pr->tokens[k];
            }
            /* append newline separator (not for final EOF) */
            if (end_kind != (int)SC_EOF && seg_len > 0) {
                ScPToken nl;
                memset(&nl, 0, sizeof nl);
                nl.kind = SC_NEWLINE;
                nl.text = (char *)"\n";
                nl.line = (i < ta.count) ? ta.tokens[i].line : 0;
                if (ccount >= cap) { cap *= 2; combined = realloc(combined, cap * sizeof(ScPToken)); }
                combined[ccount++] = nl;
            }
            start = i + 1;
        }
    }

    /* Lower BEFORE freeing segment parse results */
    ScLowerResult lr = sc_lower(combined, ccount, "<test>");
    free(combined);

    /* Now safe to free parse results */
    for (int i = 0; i < nseg; i++) sc_parse_free(&segments[i]);
    free(ta.tokens);

    return lr.nerrors == 0 ? lr.prog : NULL;
}

/* ---------------------------------------------------------------------------
 * Test: OUTPUT = 'hello'  (M-SNOC-LOWER trigger)
 * ------------------------------------------------------------------------- */
static void test_hello_assign(void) {
    Program *prog = pipeline("OUTPUT = 'hello'");
    ASSERT(prog != NULL,               "pipeline returned NULL");
    if (!prog) return;
    ASSERT(prog->nstmts == 1,          "nstmts == 1");
    STMT_t *st = prog->head;
    ASSERT(st != NULL,                 "head stmt not NULL");
    if (!st) return;
    ASSERT(st->has_eq == 1,            "has_eq == 1");
    ASSERT(st->subject != NULL,        "subject not NULL");
    ASSERT(st->replacement != NULL,    "replacement not NULL");
    if (st->subject)
        ASSERT(st->subject->kind == E_VART, "subject kind == E_VART");
    if (st->subject && st->subject->sval)
        ASSERT(strcmp(st->subject->sval, "OUTPUT") == 0, "subject name == OUTPUT");
    if (st->replacement)
        ASSERT(st->replacement->kind == E_QLIT, "replacement kind == E_QLIT");
    if (st->replacement && st->replacement->sval)
        ASSERT(strcmp(st->replacement->sval, "hello") == 0, "replacement value == hello");
    ASSERT(st->pattern == NULL,        "pattern == NULL");
}

/* ---------------------------------------------------------------------------
 * Test: x = 1 + 2
 * ------------------------------------------------------------------------- */
static void test_arith_assign(void) {
    Program *prog = pipeline("x = 1 + 2");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_ADD, "replacement kind == E_ADD");
    if (st->replacement->left)
        ASSERT(st->replacement->left->kind  == E_ILIT, "add left == E_ILIT");
    if (st->replacement->right)
        ASSERT(st->replacement->right->kind == E_ILIT, "add right == E_ILIT");
    if (st->replacement->left)  ASSERT(st->replacement->left->ival  == 1, "ival 1");
    if (st->replacement->right) ASSERT(st->replacement->right->ival == 2, "ival 2");
}

/* ---------------------------------------------------------------------------
 * Test: y = GT(a, b) — function call
 * ------------------------------------------------------------------------- */
static void test_fnc_call(void) {
    Program *prog = pipeline("y = GT(a, b)");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_FNC,  "replacement kind == E_FNC");
    if (st->replacement->sval)
        ASSERT(strcmp(st->replacement->sval, "GT") == 0, "fnc name == GT");
    ASSERT(st->replacement->nargs == 2, "fnc nargs == 2");
}

/* ---------------------------------------------------------------------------
 * Test: z = a == b  -> EQ(a,b)
 * ------------------------------------------------------------------------- */
static void test_eq_op(void) {
    Program *prog = pipeline("z = a == b");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_FNC, "== maps to E_FNC");
    if (st->replacement->sval)
        ASSERT(strcmp(st->replacement->sval, "EQ") == 0, "== maps to EQ");
}

/* ---------------------------------------------------------------------------
 * Test: two statements on separate lines
 * ------------------------------------------------------------------------- */
static void test_multi_stmt(void) {
    Program *prog = pipeline("OUTPUT = 'a'\nOUTPUT = 'b'");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    ASSERT(prog->nstmts == 2, "nstmts == 2");
    STMT_t *s1 = prog->head;
    STMT_t *s2 = s1 ? s1->next : NULL;
    ASSERT(s1 != NULL, "stmt1 not NULL");
    ASSERT(s2 != NULL, "stmt2 not NULL");
    if (s1 && s1->replacement && s1->replacement->sval)
        ASSERT(strcmp(s1->replacement->sval, "a") == 0, "stmt1 value == a");
    if (s2 && s2->replacement && s2->replacement->sval)
        ASSERT(strcmp(s2->replacement->sval, "b") == 0, "stmt2 value == b");
}

/* ---------------------------------------------------------------------------
 * Test: p = 'a' || 'b'  -> E_OR
 * ------------------------------------------------------------------------- */
static void test_or_expr(void) {
    Program *prog = pipeline("p = 'a' || 'b'");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_OR, "|| maps to E_OR");
}

/* ---------------------------------------------------------------------------
 * Test: s = 'hello' && ' world'  -> E_CONC
 * ------------------------------------------------------------------------- */
static void test_concat_expr(void) {
    Program *prog = pipeline("s = 'hello' && ' world'");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_CONC, "&& maps to E_CONC");
}

/* ---------------------------------------------------------------------------
 * Test: r = a % b  -> REMDR(a,b)
 * ------------------------------------------------------------------------- */
static void test_percent_op(void) {
    Program *prog = pipeline("r = a % b");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_FNC, "% maps to E_FNC");
    if (st->replacement->sval)
        ASSERT(strcmp(st->replacement->sval, "REMDR") == 0, "% maps to REMDR");
}

/* ---------------------------------------------------------------------------
 * Test: x = a[i]  -> E_IDX
 * ------------------------------------------------------------------------- */
static void test_array_ref(void) {
    Program *prog = pipeline("x = a[i]");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_IDX,  "a[i] maps to E_IDX");
    ASSERT(st->replacement->nargs == 1,     "E_IDX nargs == 1");
}

/* ---------------------------------------------------------------------------
 * Test: x = -y  -> E_MNS
 * ------------------------------------------------------------------------- */
static void test_unary_minus(void) {
    Program *prog = pipeline("x = -y");
    ASSERT(prog != NULL, "pipeline returned NULL");
    if (!prog) return;
    STMT_t *st = prog->head;
    ASSERT(st != NULL, "head stmt not NULL");
    if (!st || !st->replacement) return;
    ASSERT(st->replacement->kind == E_MNS, "unary - maps to E_MNS");
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void) {
    test_hello_assign();   /* M-SNOC-LOWER trigger */
    test_arith_assign();
    test_fnc_call();
    test_eq_op();
    test_multi_stmt();
    test_or_expr();
    test_concat_expr();
    test_percent_op();
    test_array_ref();
    test_unary_minus();

    int total = g_pass + g_fail;
    printf("%d/%d PASS\n", g_pass, total);
    if (g_fail) {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
