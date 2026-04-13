/*
 * prolog_interp.c -- Prolog IR interpreter
 *
 * One-to-one mirror of prolog_emit.c.
 * Each function corresponds to its emit_* counterpart:
 *   pl_term_from_expr  <-> emit_term_val
 *   pl_eval_arith      <-> emit_arith_expr
 *   pl_exec_goal       <-> emit_goal
 *   pl_exec_body       <-> emit_body
 *   pl_exec_clause     <-> emit_clause
 *   pl_exec_choice     <-> emit_choice
 *   pl_execute_program <-> pl_emit (top-level)
 */

#include "prolog_interp.h"
#include "prolog_atom.h"
#include "prolog_runtime.h"
#include "term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Predicate table
 * ========================================================================= */
#define PL_MAX_PREDS 512
typedef struct { const char *key; EXPR_t *choice; } PlPred;
static PlPred pl_preds[PL_MAX_PREDS];
static int    pl_npreds = 0;
static Trail  pl_trail;

static EXPR_t *pl_lookup(const char *key) {
    for (int i = 0; i < pl_npreds; i++)
        if (strcmp(pl_preds[i].key, key) == 0) return pl_preds[i].choice;
    return NULL;
}
static void pl_register(const char *key, EXPR_t *choice) {
    if (pl_npreds < PL_MAX_PREDS) {
        pl_preds[pl_npreds].key = key;
        pl_preds[pl_npreds].choice = choice;
        pl_npreds++;
    }
}

/* Forward declarations */
static int pl_exec_goal(EXPR_t *goal, Term **env, int n_vars);
static int pl_exec_body(EXPR_t **goals, int ngoals, Term **env, int n_vars);
static int pl_exec_choice(EXPR_t *choice, Term **call_args, int arity);

/* =========================================================================
 * pl_term_from_expr -- mirror of emit_term_val
 * Converts a lowered EXPR_t term node to a runtime Term*.
 * ========================================================================= */
static Term *pl_term_from_expr(EXPR_t *e, Term **env, int n_vars) {
    if (!e) return term_new_atom(prolog_atom_intern("[]"));
    switch (e->kind) {
        case E_QLIT:
            return term_new_atom(prolog_atom_intern(e->sval ? e->sval : ""));
        case E_ILIT:
            return term_new_int((long)e->ival);
        case E_FLIT:
            return term_new_float(e->dval);
        case E_VAR: {
            int slot = (int)e->ival;
            if (slot < 0 || slot >= n_vars) return term_new_var(-1);
            if (!env[slot]) env[slot] = term_new_var(slot);
            return env[slot];
        }
        case E_FNC: {
            int arity = e->nchildren;
            if (arity == 0)
                return term_new_atom(prolog_atom_intern(e->sval ? e->sval : "f"));
            Term **args = malloc(arity * sizeof(Term *));
            for (int i = 0; i < arity; i++)
                args[i] = pl_term_from_expr(e->children[i], env, n_vars);
            int fid = prolog_atom_intern(e->sval ? e->sval : "f");
            Term *t = term_new_compound(fid, arity, args);
            free(args);
            return t;
        }
        default:
            return term_new_atom(prolog_atom_intern("?"));
    }
}

/* =========================================================================
 * pl_eval_arith -- mirror of emit_arith_expr
 * Evaluates an arithmetic EXPR_t to a C long.
 * ========================================================================= */
static long pl_eval_arith(EXPR_t *e, Term **env, int n_vars) {
    if (!e) return 0;
    switch (e->kind) {
        case E_ILIT: return (long)e->ival;
        case E_FLIT: return (long)e->dval;
        case E_VAR: {
            int slot = (int)e->ival;
            if (slot < 0 || slot >= n_vars || !env[slot]) return 0;
            Term *t = term_deref(env[slot]);
            return (t && t->tag == TT_INT) ? t->ival : 0;
        }
        case E_ADD: return pl_eval_arith(e->children[0],env,n_vars)
                         + pl_eval_arith(e->children[1],env,n_vars);
        case E_SUB: return pl_eval_arith(e->children[0],env,n_vars)
                         - pl_eval_arith(e->children[1],env,n_vars);
        case E_MUL: return pl_eval_arith(e->children[0],env,n_vars)
                         * pl_eval_arith(e->children[1],env,n_vars);
        case E_DIV: {
            long d = pl_eval_arith(e->children[1],env,n_vars);
            return d ? pl_eval_arith(e->children[0],env,n_vars) / d : 0;
        }
        default: return 0;
    }
}

/* =========================================================================
 * pl_write_term -- runtime write/1 (mirror of pl_write in emitter runtime)
 * ========================================================================= */
static void pl_write_term(Term *t) {
    if (!t) { printf("[]"); return; }
    t = term_deref(t);
    if (!t) { printf("_"); return; }
    switch (t->tag) {
        case TT_ATOM:     printf("%s", prolog_atom_name(t->atom_id)); break;
        case TT_INT:      printf("%ld", t->ival); break;
        case TT_FLOAT:    printf("%g", t->fval); break;
        case TT_VAR:      printf("_G%d", t->var_slot); break;
        case TT_COMPOUND:
            printf("%s(", prolog_atom_name(t->compound.functor));
            for (int i = 0; i < t->compound.arity; i++) {
                if (i) printf(",");
                pl_write_term(t->compound.args[i]);
            }
            printf(")");
            break;
        default: printf("?"); break;
    }
}

/* =========================================================================
 * pl_exec_goal -- mirror of emit_goal
 *
 * Returns: 1 = success (gamma), 0 = failure (omega), 2 = cut signal
 * ========================================================================= */
static int pl_exec_goal(EXPR_t *goal, Term **env, int n_vars) {
    if (!goal) return 1;

    /* E_UNIFY -- mirror of emit_goal E_UNIFY case */
    if (goal->kind == E_UNIFY && goal->nchildren == 2) {
        Term *t1 = pl_term_from_expr(goal->children[0], env, n_vars);
        Term *t2 = pl_term_from_expr(goal->children[1], env, n_vars);
        int mark = trail_mark(&pl_trail);
        if (!unify(t1, t2, &pl_trail)) { trail_unwind(&pl_trail, mark); return 0; }
        return 1;
    }

    /* E_CUT -- seal beta, signal cut upward (return 2) */
    if (goal->kind == E_CUT)          return 2;
    if (goal->kind == E_TRAIL_MARK)   return 1;
    if (goal->kind == E_TRAIL_UNWIND) return 1;

    if (goal->kind != E_FNC) return 1;

    const char *fn = goal->sval ? goal->sval : "true";
    int arity = goal->nchildren;

    /* Builtins -- mirror of emit_goal E_FNC cases in order */

    if (strcmp(fn,"true")==0 && arity==0) return 1;
    if (strcmp(fn,"fail")==0 && arity==0) return 0;
    if (strcmp(fn,"halt")==0 && arity==0) { exit(0); }
    if (strcmp(fn,"halt")==0 && arity==1) {
        exit((int)pl_eval_arith(goal->children[0], env, n_vars));
    }
    if (strcmp(fn,"nl")==0 && arity==0) { putchar('\n'); return 1; }
    if (strcmp(fn,"write")==0 && arity==1) {
        pl_write_term(pl_term_from_expr(goal->children[0], env, n_vars));
        return 1;
    }
    if (strcmp(fn,"writeln")==0 && arity==1) {
        pl_write_term(pl_term_from_expr(goal->children[0], env, n_vars));
        putchar('\n'); return 1;
    }

    /* is/2 -- arithmetic evaluation, mirror of emit_goal is/2 */
    if (strcmp(fn,"is")==0 && arity==2) {
        Term *result = term_new_int(pl_eval_arith(goal->children[1], env, n_vars));
        Term *lhs    = pl_term_from_expr(goal->children[0], env, n_vars);
        int mark = trail_mark(&pl_trail);
        if (!unify(lhs, result, &pl_trail)) { trail_unwind(&pl_trail, mark); return 0; }
        return 1;
    }

    /* Comparison operators -- mirror of emit_goal cmp table */
    {
        struct { const char *name; int op; } cmps[] = {
            {"<",1},{">",2},{"=<",3},{">=",4},{"=:=",5},{"=\\=",6},{NULL,0}
        };
        for (int i = 0; cmps[i].name; i++) {
            if (strcmp(fn, cmps[i].name)==0 && arity==2) {
                long a = pl_eval_arith(goal->children[0], env, n_vars);
                long b = pl_eval_arith(goal->children[1], env, n_vars);
                switch (cmps[i].op) {
                    case 1: return a <  b;
                    case 2: return a >  b;
                    case 3: return a <= b;
                    case 4: return a >= b;
                    case 5: return a == b;
                    case 6: return a != b;
                }
            }
        }
    }

    /* =/2 structural unification */
    if (strcmp(fn,"=")==0 && arity==2) {
        Term *t1 = pl_term_from_expr(goal->children[0], env, n_vars);
        Term *t2 = pl_term_from_expr(goal->children[1], env, n_vars);
        int mark = trail_mark(&pl_trail);
        if (!unify(t1, t2, &pl_trail)) { trail_unwind(&pl_trail, mark); return 0; }
        return 1;
    }

    /* \=/2 not unifiable */
    if (strcmp(fn,"\\=")==0 && arity==2) {
        Term *t1 = pl_term_from_expr(goal->children[0], env, n_vars);
        Term *t2 = pl_term_from_expr(goal->children[1], env, n_vars);
        int mark = trail_mark(&pl_trail);
        int r = unify(t1, t2, &pl_trail);
        trail_unwind(&pl_trail, mark);
        return !r;
    }

    /* Type tests -- mirror of emit_goal tests table */
    {
        struct { const char *name; TermTag tag; int invert; } tests[] = {
            {"var",      TT_VAR,      0},
            {"nonvar",   TT_VAR,      1},
            {"atom",     TT_ATOM,     0},
            {"integer",  TT_INT,      0},
            {"float",    TT_FLOAT,    0},
            {"compound", TT_COMPOUND, 0},
            {NULL,       TT_ATOM,     0}
        };
        for (int i = 0; tests[i].name; i++) {
            if (strcmp(fn, tests[i].name)==0 && arity==1) {
                Term *t = term_deref(pl_term_from_expr(goal->children[0], env, n_vars));
                int match = t && t->tag == tests[i].tag;
                return tests[i].invert ? !match : match;
            }
        }
    }

    /* ,/2 conjunction -- mirror of emit_goal comma case */
    if (strcmp(fn,",")==0 && arity==2) {
        int r = pl_exec_goal(goal->children[0], env, n_vars);
        if (r == 0) return 0;
        if (r == 2) return 2;
        return pl_exec_goal(goal->children[1], env, n_vars);
    }

    /* not/1, \+/1 -- negation as failure */
    if ((strcmp(fn,"not")==0 || strcmp(fn,"\\+")==0) && arity==1) {
        int mark = trail_mark(&pl_trail);
        int r = pl_exec_goal(goal->children[0], env, n_vars);
        trail_unwind(&pl_trail, mark);
        return (r == 0) ? 1 : 0;
    }

    /* User-defined predicate */
    char key[128];
    snprintf(key, sizeof key, "%s/%d", fn, arity);
    EXPR_t *ch = pl_lookup(key);
    if (ch) {
        Term **gargs = malloc((arity + 1) * sizeof(Term *));
        for (int i = 0; i < arity; i++)
            gargs[i] = pl_term_from_expr(goal->children[i], env, n_vars);
        int r = pl_exec_choice(ch, gargs, arity);
        free(gargs);
        return r;
    }

    fprintf(stderr, "prolog: undefined predicate %s/%d\n", fn, arity);
    return 0;
}

/* =========================================================================
 * pl_exec_body -- mirror of emit_body
 * Execute goals left-to-right; propagate cut (return 2).
 * ========================================================================= */
static int pl_exec_body(EXPR_t **goals, int ngoals, Term **env, int n_vars) {
    for (int i = 0; i < ngoals; i++) {
        int r = pl_exec_goal(goals[i], env, n_vars);
        if (r != 1) return r;   /* 0=fail, 2=cut */
    }
    return 1;
}

/* =========================================================================
 * pl_exec_clause -- mirror of emit_clause
 *
 * Unifies call args with head args, then executes body.
 * cut_out is set to 1 if body contained a cut.
 * Returns 1=success, 0=fail.
 * ========================================================================= */
static int pl_exec_clause(EXPR_t *ec, Term **call_args, int arity, int *cut_out) {
    int n_vars = (int)ec->ival;
    int n_args = (int)ec->dval;

    Term **env = calloc(n_vars, sizeof(Term *));
    for (int i = 0; i < n_vars; i++)
        env[i] = term_new_var(i);

    int clause_mark = trail_mark(&pl_trail);

    /* Unify head args -- mirror of emit_clause head unification loop */
    for (int i = 0; i < arity && i < n_args; i++) {
        int mu = trail_mark(&pl_trail);
        Term *head = pl_term_from_expr(ec->children[i], env, n_vars);
        if (!unify(call_args[i], head, &pl_trail)) {
            trail_unwind(&pl_trail, mu);
            trail_unwind(&pl_trail, clause_mark);
            free(env);
            return 0;
        }
    }

    /* Execute body */
    int nbody = ec->nchildren - n_args;
    int r = (nbody > 0)
        ? pl_exec_body(ec->children + n_args, nbody, env, n_vars)
        : 1;

    if (r == 2) { *cut_out = 1; r = 1; }   /* cut: succeed, seal beta */
    if (!r) trail_unwind(&pl_trail, clause_mark);
    free(env);
    return r;
}

/* =========================================================================
 * pl_exec_choice -- mirror of emit_choice
 *
 * Tries each E_CLAUSE child in order (alpha/beta).
 * Returns 1 on first success (gamma), 0 if all exhausted (omega).
 * Cut seals beta immediately.
 * ========================================================================= */
static int pl_exec_choice(EXPR_t *choice, Term **call_args, int arity) {
    int cut = 0;
    for (int ci = 0; ci < choice->nchildren && !cut; ci++) {
        EXPR_t *clause = choice->children[ci];
        if (clause->kind != E_CLAUSE) continue;
        int mark = trail_mark(&pl_trail);
        int r = pl_exec_clause(clause, call_args, arity, &cut);
        if (r) return 1;                    /* gamma */
        trail_unwind(&pl_trail, mark);      /* beta: restore, try next */
    }
    return 0;                               /* omega */
}

/* =========================================================================
 * pl_execute_program -- top-level entry point (mirror of pl_emit top level)
 * ========================================================================= */
void pl_execute_program(Program *prog) {
    prolog_atom_init();
    trail_init(&pl_trail);
    pl_npreds = 0;

    /* Build predicate table: one E_CHOICE per STMT_t subject */
    for (STMT_t *st = prog->head; st; st = st->next)
        if (st->subject && st->subject->kind == E_CHOICE)
            pl_register(st->subject->sval, st->subject);

    /* Call main/0 */
    EXPR_t *main_ch = pl_lookup("main/0");
    if (main_ch) { pl_exec_choice(main_ch, NULL, 0); return; }
    fprintf(stderr, "prolog: no main/0 predicate\n");
}
