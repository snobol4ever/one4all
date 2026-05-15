#include "term.h"
#include "prolog_atom.h"
#include "prolog_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int tests_run    = 0;
static int tests_passed = 0;
#define CHECK(label, cond) do {                                 \
    tests_run++;                                                \
    if (cond) {                                                 \
        printf("PASS: %s\n", label);                            \
        tests_passed++;                                         \
    } else {                                                    \
        printf("FAIL: %s\n", label);                            \
    }                                                           \
} while(0)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(void) {
    prolog_atom_init();
    int atom_f = prolog_atom_intern("f");
    int atom_a = prolog_atom_intern("a");
    int atom_b = prolog_atom_intern("b");
    Trail trail;
    trail_init(&trail);
    Term *X    = term_new_var(0);
    Term *atom_a_term = term_new_atom(atom_a);
    Term *args1[2] = { X, atom_a_term };
    Term *fXa  = term_new_compound(atom_f, 2, args1);
    Term *atom_b_term = term_new_atom(atom_b);
    Term *Y    = term_new_var(1);
    Term *args2[2] = { atom_b_term, Y };
    Term *fbY  = term_new_compound(atom_f, 2, args2);
    int mark = trail_mark(&trail);
    int ok = unify(fXa, fbY, &trail);
    CHECK("unify(f(X,a), f(b,Y)) succeeds", ok == 1);
    Term *Xval = term_deref(X);
    CHECK("X = b",
          Xval && Xval->tag == TERM_ATOM && Xval->atom_id == atom_b);
    Term *Yval = term_deref(Y);
    CHECK("Y = a",
          Yval && Yval->tag == TERM_ATOM && Yval->atom_id == atom_a);
    trail_unwind(&trail, mark);
    CHECK("trail_unwind restores X to unbound",
          X->tag == TERM_VAR && X->var_slot == 0);
    CHECK("trail_unwind restores Y to unbound",
          Y->tag == TERM_VAR && Y->var_slot == 1);
    printf("%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
