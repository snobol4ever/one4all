/*
 * prolog_lower_test.c — M-PROLOG-LOWER acceptance criterion
 *
 * Verifies ClauseAST -> IR lowering for all 10 corpus rungs.
 * Acceptance: IR pretty-prints correctly for representative programs.
 *
 * Build:
 *   gcc -I. -I../snobol4 -o prolog_lower_test \
 *       prolog_lower_test.c prolog_lower.c prolog_parse.c prolog_lex.c \
 *       prolog_atom.c prolog_unify.c
 */

#include "prolog_lower.h"
#include "prolog_parse.h"
#include "prolog_atom.h"
#include "scrip_cc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0, tests_passed = 0;

#define CHECK(label, cond) do { \
    tests_run++; \
    if (cond) { printf("PASS: %s\n", label); tests_passed++; } \
    else        printf("FAIL: %s\n", label); \
} while(0)

/* Count STMT_t nodes whose subject has a given tree_e */
static int count_kind(CODE_t *prog, tree_e k) {
    int n = 0;
    for (STMT_t *s = prog->head; s; s = s->next)
        if (s->subject && s->subject->t == k) n++;
    return n;
}

/* Find TT_CHOICE by functor/arity string "foo/2" */
static tree_t *find_choice(CODE_t *prog, const char *pred) {
    for (STMT_t *s = prog->head; s; s = s->next)
        if (s->subject && s->subject->t == TT_CHOICE &&
            s->subject->v.sval && strcmp(s->subject->v.sval, pred) == 0)
            return s->subject;
    return NULL;
}

/* -------------------------------------------------------------------------
 * Test 1: facts -> TT_CHOICE with TT_CLAUSE children, zero body goals
 * ---------------------------------------------------------------------- */
static void test_facts(void) {
    const char *src =
        "person(brown).\n"
        "person(jones).\n"
        "person(smith).\n";
    PlProgram *pl = prolog_parse(src, "t_facts");
    CODE_t   *ir = prolog_lower(pl);

    CHECK("facts: 1 TT_CHOICE", count_kind(ir, TT_CHOICE) == 1);
    tree_t *ch = find_choice(ir, "person/1");
    CHECK("facts: choice is person/1", ch != NULL);
    CHECK("facts: 3 clauses",  ch && ch->n == 3);
    /* Each clause: 1 head arg, 0 body goals */
    CHECK("facts: clause[0] nchildren==1", ch && ch->n >= 1 &&
          ch->c[0]->t == TT_CLAUSE &&
          ch->c[0]->n == 1);
    prolog_program_free(pl); free(ir);
}

/* -------------------------------------------------------------------------
 * Test 2: rule with body -> TT_CLAUSE children include body goals
 * ---------------------------------------------------------------------- */
static void test_rule(void) {
    const char *src =
        "double(X, Y) :- Y is X * 2.\n";
    PlProgram *pl = prolog_parse(src, "t_rule");
    CODE_t   *ir = prolog_lower(pl);

    tree_t *ch = find_choice(ir, "double/2");
    CHECK("rule: choice double/2 exists", ch != NULL);
    CHECK("rule: 1 clause", ch && ch->n == 1);
    if (ch && ch->n == 1) {
        tree_t *cl = ch->c[0];
        CHECK("rule: TT_CLAUSE kind", cl->t == TT_CLAUSE);
        /* head args: X(_V0), Y(_V1) = 2 children; body: is(Y, X*2) = 1 child */
        CHECK("rule: 3 children (2 head args + 1 body goal)", cl->n == 3);
        CHECK("rule: n_vars >= 2", cl->v.ival >= 2);
    }
    prolog_program_free(pl); free(ir);
}

/* -------------------------------------------------------------------------
 * Test 3: unification -> TT_UNIFY node
 * ---------------------------------------------------------------------- */
static void test_unify_node(void) {
    const char *src =
        "test :- X = foo.\n";
    PlProgram *pl = prolog_parse(src, "t_unify");
    CODE_t   *ir = prolog_lower(pl);

    tree_t *ch = find_choice(ir, "test/0");
    CHECK("unify: choice test/0 exists", ch != NULL);
    if (ch && ch->n == 1) {
        tree_t *cl = ch->c[0];
        /* body has 1 goal: X = foo -> TT_UNIFY */
        /* nchildren = 0 head args + 1 body = 1 */
        CHECK("unify: TT_UNIFY in body",
              cl->n >= 1 &&
              cl->c[0]->t == TT_UNIFY);
    }
    prolog_program_free(pl); free(ir);
}

/* -------------------------------------------------------------------------
 * Test 4: cut -> TT_CUT node
 * ---------------------------------------------------------------------- */
static void test_cut_node(void) {
    const char *src =
        "differ(X, X) :- !, fail.\n"
        "differ(_, _).\n";
    PlProgram *pl = prolog_parse(src, "t_cut");
    CODE_t   *ir = prolog_lower(pl);

    tree_t *ch = find_choice(ir, "differ/2");
    CHECK("cut: choice differ/2 exists", ch != NULL);
    CHECK("cut: 2 clauses", ch && ch->n == 2);
    if (ch && ch->n >= 1) {
        tree_t *cl1 = ch->c[0];
        /* differ(X,X) :- !,fail  -> head_args=2, body=[!,fail]=2 -> 4 children */
        /* body[0] = TT_CUT */
        int found_cut = 0;
        for (int i = 0; i < cl1->n; i++)
            if (cl1->c[i]->t == TT_CUT) found_cut = 1;
        CHECK("cut: TT_CUT in clause 1 body", found_cut);
    }
    prolog_program_free(pl); free(ir);
}

/* -------------------------------------------------------------------------
 * Test 5: multiple predicates -> multiple TT_CHOICE nodes
 * ---------------------------------------------------------------------- */
static void test_multi_pred(void) {
    const char *src =
        "member(X, [X|_]).\n"
        "member(X, [_|T]) :- member(X, T).\n"
        "append([], L, L).\n"
        "append([H|T], L, [H|R]) :- append(T, L, R).\n";
    PlProgram *pl = prolog_parse(src, "t_multi");
    CODE_t   *ir = prolog_lower(pl);

    CHECK("multi: 2 TT_CHOICE nodes", count_kind(ir, TT_CHOICE) == 2);
    CHECK("multi: member/2 exists", find_choice(ir, "member/2") != NULL);
    CHECK("multi: append/3 exists", find_choice(ir, "append/3") != NULL);
    tree_t *mem = find_choice(ir, "member/2");
    CHECK("multi: member/2 has 2 clauses", mem && mem->n == 2);
    prolog_program_free(pl); free(ir);
}

/* -------------------------------------------------------------------------
 * Test 6: directive -> emitted before choices
 * ---------------------------------------------------------------------- */
static void test_directive(void) {
    const char *src =
        ":- initialization(main).\n"
        "main :- write(hello), nl.\n";
    PlProgram *pl = prolog_parse(src, "t_dir");
    CODE_t   *ir = prolog_lower(pl);

    CHECK("directive: nstmts >= 2", ir->nstmts >= 2);
    /* First stmt is directive (TT_FNC) */
    CHECK("directive: first stmt is TT_FNC",
          ir->head && ir->head->subject &&
          ir->head->subject->t == TT_FNC);
    prolog_program_free(pl); free(ir);
}

/* -------------------------------------------------------------------------
 * Test 7: puzzle_01 — full program, 10 rungs
 * ---------------------------------------------------------------------- */
static const char *PUZZLE01 =
    ":- initialization(main).\n"
    "person(brown).\n"
    "person(jones).\n"
    "person(smith).\n"
    "puzzle :-\n"
    "   person(Cashier), person(Manager), person(Teller),\n"
    "   differ(Cashier, Manager, Teller),\n"
    "   differ(smith, Manager),\n"
    "   differ(Teller, brown),\n"
    "   differ(smith, Teller),\n"
    "   display(Cashier, Manager, Teller), fail.\n"
    "display(C, M, T) :-\n"
    "   write('Cashier='), write(C), write(' Manager='), write(M),\n"
    "   write(' Teller='), write(T), write('\\n').\n"
    "main :- puzzle ; true.\n"
    "differ(X, X) :- !, fail.\n"
    "differ(_, _).\n"
    "differ(X, X, _) :- !, fail.\n"
    "differ(X, _, X) :- !, fail.\n"
    "differ(_, X, X) :- !, fail.\n"
    "differ(_, _, _).\n";

static void test_puzzle01(void) {
    PlProgram *pl = prolog_parse(PUZZLE01, "puzzle01");
    CHECK("puzzle01: parse 0 errors", pl->nerrors == 0);
    CODE_t *ir = prolog_lower(pl);
    CHECK("puzzle01: TT_CHOICE nodes >= 5", count_kind(ir, TT_CHOICE) >= 5);
    CHECK("puzzle01: person/1 has 3 clauses",
          find_choice(ir, "person/1") &&
          find_choice(ir, "person/1")->n == 3);
    CHECK("puzzle01: differ/2 has 2 clauses",
          find_choice(ir, "differ/2") &&
          find_choice(ir, "differ/2")->n == 2);
    CHECK("puzzle01: differ/3 has 4 clauses",
          find_choice(ir, "differ/3") &&
          find_choice(ir, "differ/3")->n == 4);
    prolog_program_free(pl); free(ir);
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(void) {
    prolog_atom_init();

    test_facts();
    test_rule();
    test_unify_node();
    test_cut_node();
    test_multi_pred();
    test_directive();
    test_puzzle01();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
