/*
 * icon_parse_test.c — Unit tests for icon_parse.c
 *
 * M-ICON-PARSE-LIT acceptance criteria:
 *   Parser produces correct AST for all Proebsting §2 paper examples.
 */

#include "icon_lex.h"
#include "icon_ast.h"
#include "icon_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0, tests_pass = 0, tests_fail = 0;

#define PASS(name) do { tests_run++; tests_pass++; printf("  PASS  %s\n", name); } while(0)
#define FAIL(name, ...) do { tests_run++; tests_fail++; printf("  FAIL  %s: ", name); printf(__VA_ARGS__); printf("\n"); } while(0)

/* Parse a single expression from src */
static IcnNode *parse_one_expr(const char *src) {
    IcnLexer lx; icn_lex_init(&lx, src);
    IcnParser p;  icn_parse_init(&p, &lx);
    IcnNode *n = icn_parse_expr(&p);
    if (p.had_error) { fprintf(stderr, "    parse error: %s\n", p.errmsg); }
    return n;
}

/* Parse a full file, return first procedure's first statement */
static IcnNode *parse_file_first_stmt(const char *src) {
    IcnLexer lx; icn_lex_init(&lx, src);
    IcnParser p;  icn_parse_init(&p, &lx);
    int count = 0;
    IcnNode **procs = icn_parse_file(&p, &count);
    if (p.had_error || count == 0) {
        if (p.had_error) fprintf(stderr, "    parse error: %s\n", p.errmsg);
        return NULL;
    }
    IcnNode *proc = procs[0];
    if (proc->nchildren < 2) return NULL;
    return proc->children[1]; /* first stmt (child 0 is proc name) */
}

/* =========================================================================
 * AST shape checks
 * ======================================================================= */

static int is_kind(IcnNode *n, IcnKind k) { return n && n->kind == k; }
static int is_int(IcnNode *n, long v) { return n && n->kind == ICN_INT && n->val.ival == v; }
static int is_var(IcnNode *n, const char *name) {
    return n && n->kind == ICN_VAR && strcmp(n->val.sval, name) == 0;
}

/* =========================================================================
 * Tests
 * ======================================================================= */

static void test_literal_int(void) {
    printf("--- literal int ---\n");
    IcnNode *n = parse_one_expr("42");
    if (is_int(n, 42)) PASS("42"); else FAIL("42", "kind=%s", n ? icn_kind_name(n->kind) : "null");
    icn_node_free(n);
}

static void test_literal_real(void) {
    printf("--- literal real ---\n");
    IcnNode *n = parse_one_expr("3.14");
    if (n && n->kind == ICN_REAL && n->val.fval > 3.13 && n->val.fval < 3.15) PASS("3.14");
    else FAIL("3.14", "kind=%s", n ? icn_kind_name(n->kind) : "null");
    icn_node_free(n);
}

static void test_literal_string(void) {
    printf("--- literal string ---\n");
    IcnNode *n = parse_one_expr("\"hello\"");
    if (n && n->kind == ICN_STR && strcmp(n->val.sval, "hello") == 0) PASS("\"hello\"");
    else FAIL("\"hello\"", "kind=%s", n ? icn_kind_name(n->kind) : "null");
    icn_node_free(n);
}

static void test_var(void) {
    printf("--- variable ---\n");
    IcnNode *n = parse_one_expr("x");
    if (is_var(n, "x")) PASS("x");
    else FAIL("x", "kind=%s", n ? icn_kind_name(n->kind) : "null");
    icn_node_free(n);
}

static void test_binop(void) {
    printf("--- binary ops ---\n");
    {
        IcnNode *n = parse_one_expr("1 + 2");
        if (is_kind(n, ICN_ADD) && is_int(n->children[0], 1) && is_int(n->children[1], 2)) PASS("1+2");
        else FAIL("1+2", "bad");
        icn_node_free(n);
    }
    {
        IcnNode *n = parse_one_expr("3 * 4");
        if (is_kind(n, ICN_MUL) && is_int(n->children[0], 3) && is_int(n->children[1], 4)) PASS("3*4");
        else FAIL("3*4", "bad");
        icn_node_free(n);
    }
    {
        /* Left-associativity: 1 + 2 + 3 = (1+2)+3 */
        IcnNode *n = parse_one_expr("1 + 2 + 3");
        if (is_kind(n, ICN_ADD) && is_kind(n->children[0], ICN_ADD)) PASS("left-assoc add");
        else FAIL("left-assoc add", "bad");
        icn_node_free(n);
    }
    {
        /* Precedence: 1 + 2 * 3 = 1 + (2*3) */
        IcnNode *n = parse_one_expr("1 + 2 * 3");
        if (is_kind(n, ICN_ADD) && is_kind(n->children[1], ICN_MUL)) PASS("prec * over +");
        else FAIL("prec * over +", "bad");
        icn_node_free(n);
    }
}

static void test_to_generator(void) {
    printf("--- to generator ---\n");
    {
        /* 1 to 5 → ICN_TO(INT(1), INT(5)) */
        IcnNode *n = parse_one_expr("1 to 5");
        if (is_kind(n, ICN_TO) && n->nchildren == 2 &&
            is_int(n->children[0], 1) && is_int(n->children[1], 5))
            PASS("1 to 5");
        else FAIL("1 to 5", "kind=%s nc=%d", n ? icn_kind_name(n->kind) : "null", n ? n->nchildren : -1);
        icn_node_free(n);
    }
    {
        /* 1 to 10 by 2 → ICN_TO_BY */
        IcnNode *n = parse_one_expr("1 to 10 by 2");
        if (is_kind(n, ICN_TO_BY) && n->nchildren == 3) PASS("1 to 10 by 2");
        else FAIL("1 to 10 by 2", "kind=%s", n ? icn_kind_name(n->kind) : "null");
        icn_node_free(n);
    }
}

static void test_relational(void) {
    printf("--- relational ---\n");
    {
        IcnNode *n = parse_one_expr("2 < 4");
        if (is_kind(n, ICN_LT) && is_int(n->children[0], 2) && is_int(n->children[1], 4)) PASS("2 < 4");
        else FAIL("2 < 4", "bad");
        icn_node_free(n);
    }
    {
        /* goal-directed: 2 < (1 to 4) */
        IcnNode *n = parse_one_expr("2 < (1 to 4)");
        if (is_kind(n, ICN_LT) && is_kind(n->children[1], ICN_TO)) PASS("2 < (1 to 4)");
        else FAIL("2 < (1 to 4)", "bad");
        icn_node_free(n);
    }
}

static void test_call(void) {
    printf("--- function call ---\n");
    {
        /* write(42) → ICN_CALL(VAR(write), INT(42)) */
        IcnNode *n = parse_one_expr("write(42)");
        if (is_kind(n, ICN_CALL) && n->nchildren == 2 &&
            is_var(n->children[0], "write") && is_int(n->children[1], 42))
            PASS("write(42)");
        else FAIL("write(42)", "kind=%s nc=%d", n ? icn_kind_name(n->kind) : "null", n ? n->nchildren : -1);
        icn_node_free(n);
    }
    {
        /* write(1 to 5) — arg is ICN_TO */
        IcnNode *n = parse_one_expr("write(1 to 5)");
        if (is_kind(n, ICN_CALL) && n->nchildren == 2 && is_kind(n->children[1], ICN_TO)) PASS("write(1 to 5)");
        else FAIL("write(1 to 5)", "bad");
        icn_node_free(n);
    }
}

static void test_every(void) {
    printf("--- every ---\n");
    /* every write(1 to 5); → ICN_EVERY(ICN_CALL(...)) */
    const char *src = "procedure main();\n  every write(1 to 5);\nend";
    IcnNode *stmt = parse_file_first_stmt(src);
    if (stmt && is_kind(stmt, ICN_EVERY) && stmt->nchildren == 1 &&
        is_kind(stmt->children[0], ICN_CALL))
        PASS("every write(1 to 5)");
    else
        FAIL("every write(1 to 5)", "stmt=%s", stmt ? icn_kind_name(stmt->kind) : "null");
}

/* =========================================================================
 * Rung 1 corpus parse tests — verify shape of AST for all 6 programs
 * ======================================================================= */

static IcnNode **parse_corpus_file(const char *path, int *count) {
    FILE *f = fopen(path, "r");
    if (!f) { *count = 0; return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *src = malloc(sz + 1);
    fread(src, 1, sz, f); src[sz] = '\0'; fclose(f);

    IcnLexer lx; icn_lex_init(&lx, src);
    IcnParser p;  icn_parse_init(&p, &lx);
    IcnNode **procs = icn_parse_file(&p, count);
    if (p.had_error) { fprintf(stderr, "    parse error: %s\n", p.errmsg); }
    free(src);
    return procs;
}

static void test_rung1_parse(void) {
    printf("--- rung1 corpus parse ---\n");
    const char *corpus = "test/frontend/icon/corpus/rung01_paper";
    struct { const char *file; IcnKind stmt_kind; IcnKind arg_kind; } cases[] = {
        /* t01: every write(1 to 5)  → EVERY(CALL(...TO...)) */
        {"t01_to5.icn",        ICN_EVERY, ICN_CALL},
        /* t02: every write((1 to 3) * (1 to 2)) → EVERY(CALL(...MUL...)) */
        {"t02_mult.icn",       ICN_EVERY, ICN_CALL},
        /* t03: every write((1 to 2) to (2 to 3)) → EVERY(CALL(...TO...)) */
        {"t03_nested_to.icn",  ICN_EVERY, ICN_CALL},
        /* t04: every write(2 < (1 to 4)) → EVERY(CALL(...LT...)) */
        {"t04_lt.icn",         ICN_EVERY, ICN_CALL},
        /* t05: every write(3 < ((1 to 3)*(1 to 2))) → EVERY(CALL(...LT...)) */
        {"t05_compound.icn",   ICN_EVERY, ICN_CALL},
        /* t06: every write(5 > (...)) → EVERY(CALL(...)) + write("done") */
        {"t06_paper_expr.icn", ICN_EVERY, ICN_CALL},
        {NULL, 0, 0}
    };
    for (int i = 0; cases[i].file; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", corpus, cases[i].file);
        int count = 0;
        IcnNode **procs = parse_corpus_file(path, &count);
        char name[128]; snprintf(name, sizeof(name), "parse %s", cases[i].file);
        if (!procs || count == 0) { FAIL(name, "no procedures parsed"); continue; }
        IcnNode *proc = procs[0];
        if (proc->nchildren < 2) { FAIL(name, "procedure has no statements"); continue; }
        IcnNode *first_stmt = proc->children[1];
        if (!is_kind(first_stmt, cases[i].stmt_kind)) {
            FAIL(name, "first stmt is %s, want %s",
                 icn_kind_name(first_stmt->kind), icn_kind_name(cases[i].stmt_kind));
            continue;
        }
        if (!is_kind(first_stmt->children[0], cases[i].arg_kind)) {
            FAIL(name, "stmt child is %s, want %s",
                 icn_kind_name(first_stmt->children[0]->kind), icn_kind_name(cases[i].arg_kind));
            continue;
        }
        PASS(name);
        /* free */
        for (int j = 0; j < count; j++) icn_node_free(procs[j]);
        free(procs);
    }
}

/* =========================================================================
 * main
 * ======================================================================= */

int main(void) {
    printf("=== icon_parse_test ===\n");
    test_literal_int();
    test_literal_real();
    test_literal_string();
    test_var();
    test_binop();
    test_to_generator();
    test_relational();
    test_call();
    test_every();
    test_rung1_parse();

    printf("\n=== RESULTS: %d/%d PASS ===\n", tests_pass, tests_run);
    return tests_fail > 0 ? 1 : 0;
}
