/*
 * test_snocone_parse_f.c — GOAL-SNOCONE-LANG-SPACE LS-4.f acceptance test
 *
 * The LS-4.f gate: control flow `if`/`else`/`while` parses and lowers to
 * a flat SPITBOL-style statement sequence with :F/:(uncond) goto fields
 * and synthetic labels.
 *
 * Lowering shapes (verified by these tests):
 *
 *   if (C) S                   ->   subj=C  go.onfailure=L1
 *                                   <S>
 *                                   label=L1
 *
 *   if (C) S1 else S2          ->   subj=C  go.onfailure=Lelse
 *                                   <S1>
 *                                   go.uncond=Lend
 *                                   label=Lelse
 *                                   <S2>
 *                                   label=Lend
 *
 *   while (C) S                ->   label=Ltop
 *                                   subj=C  go.onfailure=Lend
 *                                   <S>
 *                                   go.uncond=Ltop
 *                                   label=Lend
 *
 * Plus dangling-else: `if (a) if (b) p; else q;` — `else` binds to the
 * inner if (Pascal/Algol balanced grammar).  Brace blocks `{ ... }`,
 * nested control flow, and bodies that are full statements all covered.
 *
 * Build:
 *   cc -Wall -o test_snocone_parse_f test_snocone_parse_f.c \
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

#define IR_DEFINE_NAMES
#include "scrip_cc.h"

Program *snocone_parse_program(const char *src, const char *filename);

/* ---- harness ---- */
static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, fmt, ...) do { \
    if (cond) { g_pass++; } \
    else { fprintf(stderr, "FAIL %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); g_fail++; } \
} while(0)

/* Walk the linked-list and collect a STMT_t* array (caller must free). */
static STMT_t **collect(Program *p, int *out_n) {
    int n = 0;
    for (STMT_t *s = p->head; s; s = s->next) n++;
    STMT_t **arr = calloc(n + 1, sizeof *arr);
    int i = 0;
    for (STMT_t *s = p->head; s; s = s->next) arr[i++] = s;
    *out_n = n;
    return arr;
}

static int is_label_only(const STMT_t *s) {
    return s && s->label && !s->subject && !s->replacement && !s->go;
}
static int is_goto_uncond(const STMT_t *s, const char *target) {
    return s && s->go && s->go->uncond && !s->subject && !s->replacement
         && (!target || strcmp(s->go->uncond, target) == 0);
}
static int is_cond_fail(const STMT_t *s) {
    return s && s->subject && s->go && s->go->onfailure && !s->go->uncond;
}

/* ---- tests ---- */

static void test_if_no_else_simple(void) {
    /* if (cond) y = 1;
     * Expect:
     *   stmt[0]: subj=AST_VAR(cond), go.onfailure="_Lend_0001"
     *   stmt[1]: subj=AST_VAR(y), repl=AST_ILIT(1), has_eq=1
     *   stmt[2]: label="_Lend_0001"
     */
    Program *p = snocone_parse_program("if (cond) y = 1;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 3, "3 stmts, got %d", n);
    if (n >= 3) {
        ASSERT(is_cond_fail(st[0]), "stmt[0] is cond+fail");
        ASSERT(st[0]->subject && st[0]->subject->kind == AST_VAR, "cond is AST_VAR");
        ASSERT(strcmp(st[0]->subject->sval, "cond") == 0, "cond.sval==cond");
        ASSERT(st[0]->go->onfailure != NULL, "onfailure set");
        ASSERT(st[1]->has_eq == 1, "stmt[1] is assign (has_eq)");
        ASSERT(st[1]->subject && st[1]->subject->kind == AST_VAR && strcmp(st[1]->subject->sval, "y") == 0, "stmt[1] LHS is y");
        ASSERT(is_label_only(st[2]), "stmt[2] is label-only");
        ASSERT(strcmp(st[2]->label, st[0]->go->onfailure) == 0, "fail target matches end label");
    }
    free(st);
}

static void test_if_else_simple(void) {
    /* if (cond) y = 1; else z = 2;
     * Expect 5 stmts: cond+F, y=1, goto Lend, label Lelse z=2 wait
     * Actually:
     *   [0] cond :F(Lelse)
     *   [1] y = 1
     *   [2] :(Lend)
     *   [3] label=Lelse
     *   [4] z = 2
     *   [5] label=Lend
     */
    Program *p = snocone_parse_program("if (cond) y = 1; else z = 2;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 6, "6 stmts, got %d", n);
    if (n >= 6) {
        ASSERT(is_cond_fail(st[0]), "[0] cond+fail");
        ASSERT(st[1]->has_eq == 1, "[1] y=1");
        ASSERT(is_goto_uncond(st[2], NULL), "[2] uncond goto");
        ASSERT(is_label_only(st[3]), "[3] is label-only (Lelse)");
        ASSERT(strcmp(st[3]->label, st[0]->go->onfailure) == 0, "Lelse label == cond fail target");
        ASSERT(st[4]->has_eq == 1, "[4] z=2");
        ASSERT(strcmp(st[4]->subject->sval, "z") == 0, "[4] LHS is z");
        ASSERT(is_label_only(st[5]), "[5] is label-only (Lend)");
        ASSERT(strcmp(st[5]->label, st[2]->go->uncond) == 0, "Lend label == uncond goto target");
    }
    free(st);
}

static void test_while_simple(void) {
    /* while (running) i = i + 1;
     * Expect:
     *   [0] label=Ltop
     *   [1] running :F(Lend)
     *   [2] i = i + 1
     *   [3] :(Ltop)
     *   [4] label=Lend
     */
    Program *p = snocone_parse_program("while (running) i = i + 1;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 5, "5 stmts, got %d", n);
    if (n >= 5) {
        ASSERT(is_label_only(st[0]), "[0] is label-only (Ltop)");
        ASSERT(is_cond_fail(st[1]), "[1] cond+fail");
        ASSERT(strcmp(st[1]->subject->sval, "running") == 0, "cond is running");
        ASSERT(st[2]->has_eq == 1, "[2] i = i+1");
        ASSERT(is_goto_uncond(st[3], NULL), "[3] uncond goto");
        ASSERT(strcmp(st[3]->go->uncond, st[0]->label) == 0, "uncond goto -> Ltop");
        ASSERT(is_label_only(st[4]), "[4] is label-only (Lend)");
        ASSERT(strcmp(st[4]->label, st[1]->go->onfailure) == 0, "Lend = cond fail target");
    }
    free(st);
}

static void test_block_body(void) {
    /* if (c) { x = 1; y = 2; }
     *   [0] c :F(Lend)
     *   [1] x = 1
     *   [2] y = 2
     *   [3] label=Lend
     */
    Program *p = snocone_parse_program("if (c) { x = 1; y = 2; }", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 4, "4 stmts, got %d", n);
    if (n >= 4) {
        ASSERT(is_cond_fail(st[0]), "[0] cond+fail");
        ASSERT(st[1]->has_eq == 1 && strcmp(st[1]->subject->sval, "x") == 0, "[1] x=1");
        ASSERT(st[2]->has_eq == 1 && strcmp(st[2]->subject->sval, "y") == 0, "[2] y=2");
        ASSERT(is_label_only(st[3]), "[3] Lend");
    }
    free(st);
}

static void test_empty_block(void) {
    /* if (c) { } — empty block as body.
     *   [0] c :F(Lend)
     *   [1] label=Lend
     */
    Program *p = snocone_parse_program("if (c) { }", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 2, "2 stmts (cond + Lend), got %d", n);
    if (n >= 2) {
        ASSERT(is_cond_fail(st[0]), "[0] cond+fail");
        ASSERT(is_label_only(st[1]), "[1] Lend");
    }
    free(st);
}

static void test_dangling_else(void) {
    /* if (a) if (b) p = 1; else q = 2;
     * Pascal/Algol balanced grammar: else binds to inner `if (b)`.
     *
     * Lowering:
     *   [0] a :F(LendA)               <- outer if, no else
     *   [1] b :F(LelseB)              <- inner if has else
     *   [2] p = 1                     <- inner then
     *   [3] :(LendB)                  <- inner uncond goto
     *   [4] label=LelseB              <- inner else label
     *   [5] q = 2                     <- inner else body
     *   [6] label=LendB               <- inner end
     *   [7] label=LendA               <- outer end
     */
    Program *p = snocone_parse_program("if (a) if (b) p = 1; else q = 2;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 8, "8 stmts, got %d", n);
    if (n >= 8) {
        ASSERT(is_cond_fail(st[0]) && strcmp(st[0]->subject->sval, "a") == 0, "[0] outer if (a)");
        ASSERT(is_cond_fail(st[1]) && strcmp(st[1]->subject->sval, "b") == 0, "[1] inner if (b)");
        ASSERT(st[2]->has_eq == 1 && strcmp(st[2]->subject->sval, "p") == 0, "[2] p = 1");
        ASSERT(is_goto_uncond(st[3], NULL), "[3] uncond goto");
        ASSERT(is_label_only(st[4]), "[4] LelseB");
        ASSERT(strcmp(st[4]->label, st[1]->go->onfailure) == 0, "LelseB matches inner-if fail target");
        ASSERT(st[5]->has_eq == 1 && strcmp(st[5]->subject->sval, "q") == 0, "[5] q = 2");
        ASSERT(is_label_only(st[6]), "[6] LendB");
        ASSERT(strcmp(st[6]->label, st[3]->go->uncond) == 0, "LendB matches uncond target");
        ASSERT(is_label_only(st[7]), "[7] LendA");
        ASSERT(strcmp(st[7]->label, st[0]->go->onfailure) == 0, "LendA matches outer fail target");
    }
    free(st);
}

static void test_nested_while_inside_if(void) {
    /* if (cond) while (i) i = i - 1;
     * Lowering:
     *   [0] cond :F(LendIf)
     *   [1] label=Ltop
     *   [2] i :F(LendW)
     *   [3] i = i - 1
     *   [4] :(Ltop)
     *   [5] label=LendW
     *   [6] label=LendIf
     */
    Program *p = snocone_parse_program("if (cond) while (i) i = i - 1;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 7, "7 stmts, got %d", n);
    if (n >= 7) {
        ASSERT(is_cond_fail(st[0]) && strcmp(st[0]->subject->sval, "cond") == 0, "[0] outer if cond");
        ASSERT(is_label_only(st[1]), "[1] Ltop");
        ASSERT(is_cond_fail(st[2]) && strcmp(st[2]->subject->sval, "i") == 0, "[2] while cond");
        ASSERT(st[3]->has_eq == 1, "[3] body assignment");
        ASSERT(is_goto_uncond(st[4], NULL), "[4] uncond goto Ltop");
        ASSERT(strcmp(st[4]->go->uncond, st[1]->label) == 0, "[4] -> Ltop");
        ASSERT(is_label_only(st[5]), "[5] LendW");
        ASSERT(strcmp(st[5]->label, st[2]->go->onfailure) == 0, "LendW matches while fail target");
        ASSERT(is_label_only(st[6]), "[6] LendIf");
        ASSERT(strcmp(st[6]->label, st[0]->go->onfailure) == 0, "LendIf matches if fail target");
    }
    free(st);
}

static void test_pre_existing_stmts(void) {
    /* x = 0;
     * if (c) y = 1;
     * Confirms splice doesn't disturb pre-existing statements.
     *
     *   [0] x = 0       <- pre-existing
     *   [1] c :F(Lend)
     *   [2] y = 1
     *   [3] label=Lend
     */
    Program *p = snocone_parse_program("x = 0; if (c) y = 1;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 4, "4 stmts, got %d", n);
    if (n >= 4) {
        ASSERT(st[0]->has_eq == 1 && strcmp(st[0]->subject->sval, "x") == 0, "[0] x = 0 preserved");
        ASSERT(is_cond_fail(st[1]), "[1] cond+fail");
        ASSERT(st[2]->has_eq == 1 && strcmp(st[2]->subject->sval, "y") == 0, "[2] y = 1");
        ASSERT(is_label_only(st[3]), "[3] Lend");
    }
    free(st);
}

static void test_post_existing_stmts(void) {
    /* if (c) y = 1; z = 2;
     * Confirms statements after the if are not pulled in.
     *   [0] c :F(Lend)
     *   [1] y = 1
     *   [2] label=Lend
     *   [3] z = 2
     */
    Program *p = snocone_parse_program("if (c) y = 1; z = 2;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 4, "4 stmts, got %d", n);
    if (n >= 4) {
        ASSERT(is_cond_fail(st[0]), "[0] cond+fail");
        ASSERT(st[1]->has_eq == 1 && strcmp(st[1]->subject->sval, "y") == 0, "[1] y=1");
        ASSERT(is_label_only(st[2]), "[2] Lend");
        ASSERT(st[3]->has_eq == 1 && strcmp(st[3]->subject->sval, "z") == 0, "[3] z=2 preserved after if");
    }
    free(st);
}

static void test_unique_labels(void) {
    /* Two adjacent ifs — labels must be distinct (counter increments per parse). */
    Program *p = snocone_parse_program("if (a) x = 1; if (b) y = 2;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 6, "6 stmts (3+3), got %d", n);
    if (n >= 6) {
        const char *L1 = st[0]->go->onfailure;
        const char *L2 = st[3]->go->onfailure;
        ASSERT(strcmp(L1, L2) != 0, "two if-Lends are distinct: %s vs %s", L1, L2);
    }
    free(st);
}

static void test_while_block(void) {
    /* while (x < 10) { x = x + 1; }
     *   [0] label=Ltop
     *   [1] LT(x, 10) :F(Lend)
     *   [2] x = x + 1
     *   [3] :(Ltop)
     *   [4] label=Lend
     */
    Program *p = snocone_parse_program("while (x < 10) { x = x + 1; }", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 5, "5 stmts, got %d", n);
    if (n >= 5) {
        ASSERT(is_label_only(st[0]), "[0] Ltop");
        ASSERT(is_cond_fail(st[1]), "[1] cond fail");
        ASSERT(st[1]->subject->kind == AST_FNC, "while cond is AST_FNC (LT)");
        ASSERT(strcmp(st[1]->subject->sval, "LT") == 0, "fname=LT");
    }
    free(st);
}

static void test_nested_if_in_while(void) {
    /* while (i) if (a) p = 1;
     *   [0] label=Ltop
     *   [1] i :F(LendW)
     *   [2] a :F(LendIf)
     *   [3] p = 1
     *   [4] label=LendIf
     *   [5] :(Ltop)
     *   [6] label=LendW
     */
    Program *p = snocone_parse_program("while (i) if (a) p = 1;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 7, "7 stmts, got %d", n);
    if (n >= 7) {
        ASSERT(is_label_only(st[0]), "[0] Ltop");
        ASSERT(is_cond_fail(st[1]), "[1] while cond");
        ASSERT(is_cond_fail(st[2]), "[2] if cond");
        ASSERT(st[3]->has_eq == 1, "[3] body");
        ASSERT(is_label_only(st[4]), "[4] LendIf");
        ASSERT(is_goto_uncond(st[5], NULL), "[5] goto Ltop");
        ASSERT(is_label_only(st[6]), "[6] LendW");
    }
    free(st);
}

static void test_if_else_with_blocks(void) {
    /* if (c) { x = 1; } else { y = 2; }
     *   [0] c :F(Lelse)
     *   [1] x = 1
     *   [2] :(Lend)
     *   [3] label=Lelse
     *   [4] y = 2
     *   [5] label=Lend
     */
    Program *p = snocone_parse_program("if (c) { x = 1; } else { y = 2; }", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 6, "6 stmts, got %d", n);
    if (n >= 6) {
        ASSERT(is_cond_fail(st[0]), "[0] cond+fail");
        ASSERT(st[1]->has_eq == 1 && strcmp(st[1]->subject->sval, "x") == 0, "[1] x=1");
        ASSERT(is_goto_uncond(st[2], NULL), "[2] uncond goto");
        ASSERT(is_label_only(st[3]), "[3] Lelse");
        ASSERT(st[4]->has_eq == 1 && strcmp(st[4]->subject->sval, "y") == 0, "[4] y=2");
        ASSERT(is_label_only(st[5]), "[5] Lend");
    }
    free(st);
}

static void test_empty_then_else(void) {
    /* if (c) ; else q = 1;  (empty then-branch)
     *   [0] c :F(Lelse)
     *   [1] :(Lend)
     *   [2] label=Lelse
     *   [3] q = 1
     *   [4] label=Lend
     */
    Program *p = snocone_parse_program("if (c) ; else q = 1;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 5, "5 stmts (cond, gotoEnd, Lelse, q, Lend), got %d", n);
    if (n >= 5) {
        ASSERT(is_cond_fail(st[0]), "[0] cond+fail");
        ASSERT(is_goto_uncond(st[1], NULL), "[1] goto Lend");
        ASSERT(is_label_only(st[2]), "[2] Lelse");
        ASSERT(st[3]->has_eq == 1, "[3] q = 1");
        ASSERT(is_label_only(st[4]), "[4] Lend");
    }
    free(st);
}

static void test_chained_else_if(void) {
    /* if (a) p; else if (b) q; else r;
     * Parses as: if (a) ... else (if (b) ... else r) — left-most outer if has matched else,
     * outer's else is itself an if-else. So:
     *   [0] a :F(LelseA)
     *   [1] p     (bare)
     *   [2] :(LendA)
     *   [3] label=LelseA
     *   [4] b :F(LelseB)
     *   [5] q     (bare)
     *   [6] :(LendB)
     *   [7] label=LelseB
     *   [8] r     (bare)
     *   [9] label=LendB
     *   [10] label=LendA
     */
    Program *p = snocone_parse_program("if (a) p; else if (b) q; else r;", "<test>");
    ASSERT(p, "parses");
    int n; STMT_t **st = collect(p, &n);
    ASSERT(n == 11, "11 stmts, got %d", n);
    if (n >= 11) {
        ASSERT(is_cond_fail(st[0]) && strcmp(st[0]->subject->sval, "a") == 0, "[0] if a");
        ASSERT(st[1]->subject && strcmp(st[1]->subject->sval, "p") == 0 && st[1]->has_eq == 0, "[1] bare p");
        ASSERT(is_goto_uncond(st[2], NULL), "[2] goto LendA");
        ASSERT(is_label_only(st[3]), "[3] LelseA");
        ASSERT(is_cond_fail(st[4]) && strcmp(st[4]->subject->sval, "b") == 0, "[4] if b");
    }
    free(st);
}

int main(void) {
    test_if_no_else_simple();
    test_if_else_simple();
    test_while_simple();
    test_block_body();
    test_empty_block();
    test_dangling_else();
    test_nested_while_inside_if();
    test_pre_existing_stmts();
    test_post_existing_stmts();
    test_unique_labels();
    test_while_block();
    test_nested_if_in_while();
    test_if_else_with_blocks();
    test_empty_then_else();
    test_chained_else_if();

    int total = g_pass + g_fail;
    printf("PASS=%d FAIL=%d TOTAL=%d\n", g_pass, g_fail, total);
    return g_fail ? 1 : 0;
}
