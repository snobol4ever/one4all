#include "prolog_runtime.h"
#include "term.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gc.h>
#define TRAIL_INIT_CAP 64
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void trail_init(Trail *t) {
    t->stack    = GC_malloc(TRAIL_INIT_CAP * sizeof(Term *));
    t->top      = 0;
    t->capacity = TRAIL_INIT_CAP;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void trail_push(Trail *t, Term *term) {
    if (t->top >= t->capacity) {
        t->capacity *= 2;
        t->stack = GC_realloc(t->stack, t->capacity * sizeof(Term *));
    }
    t->stack[t->top++] = term;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void trail_unwind(Trail *t, int mark) {
    while (t->top > mark) {
        Term *bound = t->stack[--t->top];
        int saved_slot = bound->saved_slot;
        bound->tag      = TERM_VAR;
        bound->var_slot = saved_slot;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void bind(Term *var, Term *val, Trail *trail) {
    if (var->var_slot != -1)
        trail_push(trail, var);
    var->ref = val;
    var->tag = TERM_REF;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int unify(Term *t1, Term *t2, Trail *trail) {
    t1 = term_deref(t1);
    t2 = term_deref(t2);
    if (t1 == t2) return 1;
    if (t1 && t1->tag == TERM_VAR) { bind(t1, t2, trail); return 1; }
    if (t2 && t2->tag == TERM_VAR) { bind(t2, t1, trail); return 1; }
    if (!t1 || !t2) return 0;
    if (t1->tag == TERM_ATOM && t2->tag == TERM_ATOM)
        return t1->atom_id == t2->atom_id;
    if (t1->tag == TERM_INT && t2->tag == TERM_INT)
        return t1->ival == t2->ival;
    if (t1->tag == TERM_FLOAT && t2->tag == TERM_FLOAT)
        return t1->fval == t2->fval;
    if (t1->tag == TERM_COMPOUND && t2->tag == TERM_COMPOUND) {
        if (t1->compound.functor != t2->compound.functor) return 0;
        if (t1->compound.arity   != t2->compound.arity  ) return 0;
        int arity = t1->compound.arity;
        for (int i = 0; i < arity; i++) {
            if (!unify(t1->compound.args[i], t2->compound.args[i], trail))
                return 0;
        }
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int trail_mark_fn(const Trail *t) { return t->top; }
