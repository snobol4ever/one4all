/*
 * test_snocone_parse_e.c — GOAL-SNOCONE-LANG-SPACE LS-4.e acceptance test
 *
 * The LS-4.e gate: all remaining SPITBOL unary operators parse and lower to
 * the correct EKind nodes.  Operators added this rung (beyond LS-4.a's
 * T_1PLUS / T_1MINUS):
 *
 *   *expr   → AST_DEFER        (deferred evaluation / indirect pattern ref)
 *   .expr   → AST_NAME         (name reference — returns name descriptor)
 *   $expr   → AST_INDIRECT     (variable indirection)
 *   @expr   → AST_CAPT_CURSOR  (cursor position capture)
 *   ~expr   → AST_NOT          (negate success/failure)
 *   ?expr   → AST_INTERROGATE  (null if succeeds, fail if fails)
 *   &expr   → AST_OPSYN sval="&"  (bare ampersand — OPSYN slot pri 2)
 *   %expr   → AST_OPSYN sval="%"  (OPSYN slot pri 10)
 *   /expr   → AST_OPSYN sval="/"  (OPSYN slot)
 *   #expr   → AST_OPSYN sval="#"  (OPSYN slot)
 *   |expr   → AST_OPSYN sval="|"  (OPSYN slot)
 *   =expr   → AST_OPSYN sval="="  (OPSYN slot)
 *
 * All unaries apply to expr17 (atoms) — highest priority, binding tighter
 * than any binary operator.  Chains like `~.x` (tilde then dot) nest right-
 * to-left: AST_NOT(AST_NAME(x)).
 *
 * Build:
 *   cc -Wall -o test_snocone_parse_e test_snocone_parse_e.c \
 *       ../../src/frontend/snocone/snocone_parse.tab.c \
 *       ../../src/frontend/snocone/snocone_lex.c \
 *       -I ../../src/frontend/snocone \
 *       -I ../../src/frontend/snobol4 \
 *       -I ../../src
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 * Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define IR_DEFINE_NAMES
#include "scrip_cc.h"

Program *snocone_parse_program(const char *src, const char *filename);



/* ---- test harness ---- */
static int g_pass = 0, g_fail = 0;

static AST_t *parse_first_stmt(const char *src) {
    /* bare expressions go into head->subject (see snocone_parse.y:886) */
    Program *prog = snocone_parse_program(src, "<test>");
    if (!prog || !prog->head) return NULL;
    return prog->head->subject;
}

#define ASSERT(cond, fmt, ...) do { \
    if (cond) { g_pass++; } \
    else { fprintf(stderr, "FAIL %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); g_fail++; } \
} while(0)

/* ---- individual tests ---- */

static void test_defer_star(void) {
    /* *x  → AST_DEFER(AST_VAR("x")) */
    AST_t *e = parse_first_stmt("*x ;");
    ASSERT(e, "parsed *x");
    ASSERT(e->kind == AST_DEFER, "kind AST_DEFER, got %s", ast_e_name[e->kind]);
    ASSERT(e->nchildren == 1, "one child");
    ASSERT(e->children[0]->kind == AST_VAR, "child is AST_VAR");
    ASSERT(strcmp(e->children[0]->sval, "x") == 0, "child sval=x");
}

static void test_name_dot(void) {
    /* .v  → AST_NAME(AST_VAR("v")) */
    AST_t *e = parse_first_stmt(".v ;");
    ASSERT(e, "parsed .v");
    ASSERT(e->kind == AST_NAME, "kind AST_NAME, got %s", ast_e_name[e->kind]);
    ASSERT(e->nchildren == 1, "one child");
    ASSERT(e->children[0]->kind == AST_VAR, "child is AST_VAR");
}

static void test_indirect_dollar(void) {
    /* $x  → AST_INDIRECT(AST_VAR("x")) */
    AST_t *e = parse_first_stmt("$x ;");
    ASSERT(e, "parsed $x");
    ASSERT(e->kind == AST_INDIRECT, "kind AST_INDIRECT, got %s", ast_e_name[e->kind]);
    ASSERT(e->nchildren == 1, "one child");
}

static void test_cursor_at(void) {
    /* @pos  → AST_CAPT_CURSOR(AST_VAR("pos")) */
    AST_t *e = parse_first_stmt("@pos ;");
    ASSERT(e, "parsed @pos");
    ASSERT(e->kind == AST_CAPT_CURSOR, "kind AST_CAPT_CURSOR, got %s", ast_e_name[e->kind]);
    ASSERT(e->nchildren == 1, "one child");
    ASSERT(e->children[0]->kind == AST_VAR, "child is AST_VAR");
}

static void test_not_tilde(void) {
    /* ~cond  → AST_NOT(AST_VAR("cond")) */
    AST_t *e = parse_first_stmt("~cond ;");
    ASSERT(e, "parsed ~cond");
    ASSERT(e->kind == AST_NOT, "kind AST_NOT, got %s", ast_e_name[e->kind]);
    ASSERT(e->nchildren == 1, "one child");
}

static void test_interrogate_quest(void) {
    /* ?x  → AST_INTERROGATE(AST_VAR("x")) */
    AST_t *e = parse_first_stmt("?x ;");
    ASSERT(e, "parsed ?x");
    ASSERT(e->kind == AST_INTERROGATE, "kind AST_INTERROGATE, got %s", ast_e_name[e->kind]);
    ASSERT(e->nchildren == 1, "one child");
}

static void test_opsyn_amp(void) {
    /* &x  → AST_OPSYN("&", AST_VAR("x")) */
    AST_t *e = parse_first_stmt("& x ;");   /* note space — no-space is T_KEYWORD */
    ASSERT(e, "parsed & x");
    ASSERT(e->kind == AST_OPSYN, "kind AST_OPSYN, got %s", ast_e_name[e->kind]);
    ASSERT(e->sval && strcmp(e->sval, "&") == 0, "sval=&");
    ASSERT(e->nchildren == 1, "one child");
}

static void test_opsyn_percent(void) {
    /* %x  → AST_OPSYN("%", AST_VAR("x")) */
    AST_t *e = parse_first_stmt("%x ;");
    ASSERT(e, "parsed %%x");
    ASSERT(e->kind == AST_OPSYN, "kind AST_OPSYN, got %s", ast_e_name[e->kind]);
    ASSERT(e->sval && strcmp(e->sval, "%") == 0, "sval=%%");
}

static void test_opsyn_slash(void) {
    /* /x  → AST_OPSYN("/", ...) */
    AST_t *e = parse_first_stmt("/x ;");
    ASSERT(e, "parsed /x");
    ASSERT(e->kind == AST_OPSYN, "kind AST_OPSYN");
    ASSERT(e->sval && strcmp(e->sval, "/") == 0, "sval=/");
}

static void test_opsyn_pound(void) {
    AST_t *e = parse_first_stmt("#x ;");
    ASSERT(e, "parsed #x");
    ASSERT(e->kind == AST_OPSYN, "kind AST_OPSYN");
    ASSERT(e->sval && strcmp(e->sval, "#") == 0, "sval=#");
}

static void test_opsyn_pipe(void) {
    AST_t *e = parse_first_stmt("|x ;");
    ASSERT(e, "parsed |x");
    ASSERT(e->kind == AST_OPSYN, "kind AST_OPSYN");
    ASSERT(e->sval && strcmp(e->sval, "|") == 0, "sval=|");
}

static void test_opsyn_equal(void) {
    AST_t *e = parse_first_stmt("=x ;");
    ASSERT(e, "parsed =x");
    ASSERT(e->kind == AST_OPSYN, "kind AST_OPSYN");
    ASSERT(e->sval && strcmp(e->sval, "=") == 0, "sval==");
}

static void test_chain_not_dot(void) {
    /* ~.x  → AST_NOT(AST_NAME(AST_VAR("x"))) — right-to-left nesting */
    AST_t *e = parse_first_stmt("~.x ;");
    ASSERT(e, "parsed ~.x");
    ASSERT(e->kind == AST_NOT, "outer AST_NOT");
    ASSERT(e->nchildren == 1, "one child");
    ASSERT(e->children[0]->kind == AST_NAME, "inner AST_NAME");
    ASSERT(e->children[0]->nchildren == 1, "AST_NAME has child");
    ASSERT(e->children[0]->children[0]->kind == AST_VAR, "innermost AST_VAR");
}

static void test_chain_defer_indirect(void) {
    /* *$x  → AST_DEFER(AST_INDIRECT(AST_VAR("x"))) */
    AST_t *e = parse_first_stmt("*$x ;");
    ASSERT(e, "parsed *$x");
    ASSERT(e->kind == AST_DEFER, "outer AST_DEFER");
    ASSERT(e->children[0]->kind == AST_INDIRECT, "inner AST_INDIRECT");
}

static void test_unary_on_literal(void) {
    /* .'hello'  → AST_NAME(AST_QLIT("hello")) */
    AST_t *e = parse_first_stmt(".'hello' ;");
    ASSERT(e, "parsed .'hello'");
    ASSERT(e->kind == AST_NAME, "kind AST_NAME");
    ASSERT(e->children[0]->kind == AST_QLIT, "child AST_QLIT");
    ASSERT(strcmp(e->children[0]->sval, "hello") == 0, "sval=hello");
}

static void test_unary_on_call(void) {
    /* *f(x)  — AST_DEFER applied to an AST_FNC node                    */
    /* Lexer sees: T_1STAR T_CALL LPAREN T_IDENT RPAREN SEMI    */
    AST_t *e = parse_first_stmt("*f(x) ;");
    ASSERT(e, "parsed *f(x)");
    ASSERT(e->kind == AST_DEFER, "outer AST_DEFER");
    ASSERT(e->children[0]->kind == AST_FNC, "child is AST_FNC");
}

static void test_unary_in_binary_context(void) {
    /* a + *b  — unary * binds tighter than binary +               */
    AST_t *e = parse_first_stmt("a + *b ;");
    /* Should parse as a + (*b), i.e. AST_ADD(a, AST_DEFER(b)) */
    ASSERT(e, "parsed a + *b");
    ASSERT(e->kind == AST_ADD, "top AST_ADD");
    ASSERT(e->nchildren == 2, "two children");
    ASSERT(e->children[0]->kind == AST_VAR, "left AST_VAR(a)");
    ASSERT(e->children[1]->kind == AST_DEFER, "right AST_DEFER");
}

static void test_not_in_if_cond(void) {
    /* ~HOST(2) is a valid unary — tests unary on a function call    */
    AST_t *e = parse_first_stmt("~HOST(2) ;");
    ASSERT(e, "parsed ~HOST(2)");
    ASSERT(e->kind == AST_NOT, "kind AST_NOT");
    ASSERT(e->children[0]->kind == AST_FNC, "child AST_FNC");
    ASSERT(strcmp(e->children[0]->sval, "HOST") == 0, "sval=HOST");
}

static void test_interrogate_on_call(void) {
    /* ?EQ(x, y) → AST_INTERROGATE(AST_FNC("EQ", x, y))               */
    AST_t *e = parse_first_stmt("?EQ(x, y) ;");
    ASSERT(e, "parsed ?EQ(x, y)");
    ASSERT(e->kind == AST_INTERROGATE, "kind AST_INTERROGATE");
    ASSERT(e->children[0]->kind == AST_FNC, "child AST_FNC");
    ASSERT(strcmp(e->children[0]->sval, "EQ") == 0, "sval=EQ");
    ASSERT(e->children[0]->nchildren == 2, "EQ has 2 children");
}

int main(void) {
    test_defer_star();
    test_name_dot();
    test_indirect_dollar();
    test_cursor_at();
    test_not_tilde();
    test_interrogate_quest();
    test_opsyn_amp();
    test_opsyn_percent();
    test_opsyn_slash();
    test_opsyn_pound();
    test_opsyn_pipe();
    test_opsyn_equal();
    test_chain_not_dot();
    test_chain_defer_indirect();
    test_unary_on_literal();
    test_unary_on_call();
    test_unary_in_binary_context();
    test_not_in_if_cond();
    test_interrogate_on_call();

    int total = g_pass + g_fail;
    printf("PASS=%d FAIL=%d TOTAL=%d\n", g_pass, g_fail, total);
    return g_fail ? 1 : 0;
}
