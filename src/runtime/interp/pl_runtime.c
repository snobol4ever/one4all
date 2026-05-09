/*
 * pl_runtime.c — Prolog interpreter runtime
 *
 * FI-5: extracted from src/driver/scrip.c.
 * Contains: pl_pred_table_*, pl_cp_stack, pl_trail_*, pl_unify_*,
 *           pl_box_choice, pl_ helpers, interp_exec_pl_builtin.
 *
 * g_pl_trail / g_pl_cut_flag remain non-static (used by pl_broker.c).
 *
 * RS-18 (2026-05-03): the 3 user-predicate clause-body invocation sites
 * route through bb_eval_value (coro_value.c), the shared Icon/Prolog
 * value-context evaluator introduced by RS-17.  No direct interp_eval
 * call site or extern declaration remains here.  bb_eval_value falls
 * through to interp_eval for the IR shapes a Prolog clause body
 * contains today (AST_BLOCK / AST_FNC / AST_CHOICE / AST_UNIFY / AST_CUT / etc.) —
 * each gets handled inside the existing interp_eval Prolog dispatch,
 * which reads g_pl_env directly for variable resolution (clause-body
 * AST_VAR is never reached because Prolog uses pl_unified_term_from_expr
 * for variable resolution at goal evaluation, not AST_VAR-eval).
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (FI-5, 2026-04-14)
 */
#include "pl_runtime.h"
#include <math.h>
#include <limits.h>
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../../frontend/prolog/prolog_driver.h"
#include "../../frontend/prolog/term.h"
#include "../../frontend/prolog/prolog_runtime.h"
#include "../../frontend/prolog/prolog_atom.h"
#include "../../frontend/prolog/prolog_builtin.h"

/* pl_assert_term declared in prolog_lower.h but included here via forward decl
 * to avoid scrip_cc.h path issues from runtime/interp context. */
extern AST_t *pl_assert_term(Term *t, int *functor_out, int *arity_out);
#include "../../frontend/prolog/pl_broker.h"
#include "../../runtime/x86/bb_broker.h"
#include "coro_value.h"   /* RS-18: bb_eval_value — shared Icon/Prolog value-context evaluator */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

/* Globals declared in pl_runtime.h */
Pl_PredTable  g_pl_pred_table;
Trail         g_pl_trail;
int           g_pl_cut_flag = 0;
Term        **g_pl_env      = NULL;
int           g_pl_active   = 0;

/* =========================================================================
 * nb_setval / nb_getval — global non-backtrackable store (PL-10)
 * ======================================================================= */
#define PL_NB_STORE_SIZE 64
typedef struct { char *key; Term *val; } Pl_NbEntry;
static Pl_NbEntry g_pl_nb_store[PL_NB_STORE_SIZE];
static int        g_pl_nb_count = 0;

static void pl_nb_setval(const char *key, Term *val) {
    for (int i = 0; i < g_pl_nb_count; i++) {
        if (strcmp(g_pl_nb_store[i].key, key) == 0) {
            g_pl_nb_store[i].val = val; return;
        }
    }
    if (g_pl_nb_count < PL_NB_STORE_SIZE) {
        g_pl_nb_store[g_pl_nb_count].key = strdup(key);
        g_pl_nb_store[g_pl_nb_count].val = val;
        g_pl_nb_count++;
    }
}

static Term *pl_nb_getval(const char *key) {
    for (int i = 0; i < g_pl_nb_count; i++)
        if (strcmp(g_pl_nb_store[i].key, key) == 0)
            return g_pl_nb_store[i].val;
    return NULL;
}

/* =========================================================================
 * throw/catch — exception mechanism (PL-10)
 * A stack of catch frames; throw() longjmps to the innermost matching one.
 * ======================================================================= */
#define PL_CATCH_STACK_MAX 64
typedef struct {
    jmp_buf  jb;
    Term    *catcher;   /* the Catcher pattern term */
    Term   **env;       /* env at catch site */
    int      trail_mark;
} Pl_CatchFrame;

static Pl_CatchFrame g_pl_catch_stack[PL_CATCH_STACK_MAX];
static int           g_pl_catch_top  = 0;
static Term         *g_pl_exception  = NULL; /* pending exception term */

/* pl_throw_iso_error: construct error(ErrorTerm, context) and throw it.
 * Returns 0 (failure) if no catch frame matches — caller should return 0. */
static int pl_throw_iso_error(Term *err_term) {
    /* Wrap in error/2: error(err_term, context) */
    Term *args2[2]; args2[0] = err_term; args2[1] = term_new_atom(prolog_atom_intern("context"));
    Term *err = term_new_compound(prolog_atom_intern("error"), 2, args2);
    g_pl_exception = err;
    for (int i = g_pl_catch_top - 1; i >= 0; i--) {
        Pl_CatchFrame *cf = &g_pl_catch_stack[i];
        Trail tmptrail; trail_init(&tmptrail);
        int tmmark = trail_mark(&tmptrail);
        int matched = unify(cf->catcher, err, &tmptrail);
        trail_unwind(&tmptrail, tmmark);
        if (matched) { g_pl_catch_top = i + 1; longjmp(cf->jb, 1); }
    }
    fprintf(stderr, "ERROR: Unhandled exception: ");
    pl_write(err); fprintf(stderr, "\n");
    exit(1);
}

/* Convenience: throw instantiation_error */
static int pl_throw_instantiation_error(void) {
    Term *e = term_new_atom(prolog_atom_intern("instantiation_error"));
    return pl_throw_iso_error(e);
}

/* Convenience: throw type_error(evaluable, Name/Arity) */
static int pl_throw_type_error_evaluable(const char *name, int arity) {
    Term *na_args[2]; na_args[0] = term_new_atom(prolog_atom_intern(name)); na_args[1] = term_new_int(arity);
    Term *na = term_new_compound(prolog_atom_intern("/"), 2, na_args);
    Term *te_args[2]; te_args[0] = term_new_atom(prolog_atom_intern("evaluable")); te_args[1] = na;
    Term *te = term_new_compound(prolog_atom_intern("type_error"), 2, te_args);
    return pl_throw_iso_error(te);
}

/* Convenience: throw existence_error(procedure, Name/Arity) — non-static so pl_broker.c can call it */
int pl_throw_existence_error_procedure(const char *name, int arity) {
    Term *na_args[2]; na_args[0] = term_new_atom(prolog_atom_intern(name)); na_args[1] = term_new_int(arity);
    Term *na = term_new_compound(prolog_atom_intern("/"), 2, na_args);
    Term *te_args[2]; te_args[0] = term_new_atom(prolog_atom_intern("procedure")); te_args[1] = na;
    Term *te = term_new_compound(prolog_atom_intern("existence_error"), 2, te_args);
    return pl_throw_iso_error(te);
}


#define PL_PRED_TABLE_SIZE PL_PRED_TABLE_SIZE_FWD

unsigned pl_pred_hash(const char *s) {
    unsigned h = 5381;
    while (*s) h = h * 33 ^ (unsigned char)*s++;
    return h % PL_PRED_TABLE_SIZE;
}
void pl_pred_table_insert(Pl_PredTable *pt, const char *key, AST_t *choice) {
    unsigned h = pl_pred_hash(key);
    Pl_PredEntry *e = malloc(sizeof(Pl_PredEntry));
    e->key = key; e->choice = choice; e->entry_pc = -1; e->next = pt->buckets[h]; pt->buckets[h] = e;
}
AST_t *pl_pred_table_lookup(Pl_PredTable *pt, const char *key) {
    for (Pl_PredEntry *e = pt->buckets[pl_pred_hash(key)]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e->choice;
    return NULL;
}

/* pl_pred_table_lookup_global — non-static wrapper for pl_broker.c (pl_interp.h) */
AST_t *pl_pred_table_lookup_global(const char *key) {
    return pl_pred_table_lookup(&g_pl_pred_table, key);
}
/* CH-17e: return full Pl_PredEntry* for entry_pc access */
Pl_PredEntry *pl_pred_entry_lookup(const char *key) {
    for (Pl_PredEntry *e = g_pl_pred_table.buckets[pl_pred_hash(key)]; e; e = e->next)
        if (e->key && strcmp(e->key, key) == 0) return e;
    return NULL;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * pl_pred_table_get_or_create_choice — find or create the AST_CHOICE node for key in the global pred table.
 * key is "functor/arity" string (caller owns; we strdup internally if creating).
 *----------------------------------------------------------------------------------------------------------------------------*/
static AST_t *pl_pred_table_get_or_create_choice(const char *key) {
    AST_t *ch = pl_pred_table_lookup(&g_pl_pred_table, key);
    if (ch) return ch;
    ch = expr_new(AST_CHOICE);
    ch->sval = strdup(key);
    pl_pred_table_insert(&g_pl_pred_table, ch->sval, ch);
    return ch;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * pl_assert_clause — assertz (end=1) or asserta (end=0) a Term* into the global pred table.
 * Returns 1 on success, 0 on failure.
 *----------------------------------------------------------------------------------------------------------------------------*/
static int pl_assert_clause(Term *t, int end) {
    int functor_id = -1, arity = 0;
    AST_t *ec = pl_assert_term(t, &functor_id, &arity);
    if (!ec) return 0;
    /* Build key string "functor/arity" */
    const char *fname = prolog_atom_name(functor_id);
    if (!fname) return 0;
    char key[256];
    snprintf(key, sizeof key, "%s/%d", fname, arity);
    AST_t *ch = pl_pred_table_get_or_create_choice(key);
    if (end) {
        /* assertz — append at end */
        expr_add_child(ch, ec);
    } else {
        /* asserta — prepend: shift existing children up, insert at [0] */
        expr_add_child(ch, ec);  /* grow array first */
        if (ch->nchildren > 1) {
            memmove(&ch->children[1], &ch->children[0], (ch->nchildren - 1) * sizeof(AST_t *));
            ch->children[0] = ec;
        }
    }
    return 1;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * pl_retract_clause — retract first matching clause for head Term*.
 * Matches on functor/arity only (structural unification of head args not yet implemented — simple name match).
 * Returns 1 if a clause was removed, 0 if none found.
 *----------------------------------------------------------------------------------------------------------------------------*/
static int pl_retract_clause(Term *t) {
    if (!t) return 0;
    t = term_deref(t);
    /* Extract head from :-(Head,Body) or plain head */
    Term *head = t;
    if (t->tag == TT_COMPOUND && t->compound.arity == 2) {
        const char *fn = prolog_atom_name(t->compound.functor);
        if (fn && strcmp(fn, ":-") == 0) head = term_deref(t->compound.args[0]);
    }
    /* Get functor/arity key */
    const char *fname = NULL;
    int arity = 0;
    if (head->tag == TT_ATOM) {
        fname = prolog_atom_name(head->atom_id); arity = 0;
    } else if (head->tag == TT_COMPOUND) {
        fname = prolog_atom_name(head->compound.functor); arity = head->compound.arity;
    }
    if (!fname) return 0;
    char key[256]; snprintf(key, sizeof key, "%s/%d", fname, arity);
    AST_t *ch = pl_pred_table_lookup(&g_pl_pred_table, key);
    if (!ch || ch->nchildren == 0) return 0;
    /* Remove first clause */
    free(ch->children[0]);
    memmove(&ch->children[0], &ch->children[1], (ch->nchildren - 1) * sizeof(AST_t *));
    ch->nchildren--;
    return 1;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * pl_abolish_pred — remove all clauses for functor/arity.
 *----------------------------------------------------------------------------------------------------------------------------*/
static int pl_abolish_pred(Term *t) {
    if (!t) return 0;
    t = term_deref(t);
    const char *fname = NULL; int arity = 0;
    /* Accept functor/arity compound or plain atom */
    if (t->tag == TT_COMPOUND && t->compound.arity == 2) {
        const char *fn = prolog_atom_name(t->compound.functor);
        if (fn && strcmp(fn, "/") == 0) {
            Term *na = term_deref(t->compound.args[0]);
            Term *ar = term_deref(t->compound.args[1]);
            if (na && na->tag == TT_ATOM) fname = prolog_atom_name(na->atom_id);
            if (ar && ar->tag == TT_INT)  arity = (int)ar->ival;
        }
    } else if (t->tag == TT_ATOM) {
        fname = prolog_atom_name(t->atom_id); arity = 0;
    }
    if (!fname) return 0;
    char key[256]; snprintf(key, sizeof key, "%s/%d", fname, arity);
    AST_t *ch = pl_pred_table_lookup(&g_pl_pred_table, key);
    if (!ch) return 1;  /* already gone — succeed */
    ch->nchildren = 0;  /* remove all clauses */
    return 1;
}

/*---- Choice point stack ----*/
#define PL_CP_STACK_MAX 4096
typedef struct {
    jmp_buf     jb;
    Pl_PredTable *pt;
    const char *key;
    int         arity;
    Trail      *trail;
    int         trail_mark;
    int         next_clause;
    int         cut;
} Pl_ChoicePoint;
static Pl_ChoicePoint pl_cp_stack[PL_CP_STACK_MAX];
static int            pl_cp_top = 0;

Term **pl_env_new(int n) {
    if (n <= 0) return NULL;
    Term **env = malloc(n * sizeof(Term *));
    for (int i = 0; i < n; i++) env[i] = term_new_var(i);
    return env;
}

/*---- Continuation type ----*/
/*---- Forward declarations ----*/
Term *pl_unified_term_from_expr(AST_t *e, Term **env);
static Term *pl_unified_deep_copy(Term *t);
int          interp_exec_pl_builtin(AST_t *goal, Term **env);



/*---- pl_unified_term_from_expr ----*/
Term *pl_unified_term_from_expr(AST_t *e, Term **env) {
    if (!e) return term_new_atom(prolog_atom_intern("[]"));
    switch (e->kind) {
        case AST_QLIT: return term_new_atom(prolog_atom_intern(e->sval ? e->sval : ""));
        case AST_ILIT: return term_new_int((long)e->ival);
        case AST_FLIT: return term_new_float(e->dval);
        case AST_VAR:  return (env && e->ival >= 0) ? env[e->ival] : term_new_var(e->ival);
        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD: {
            /* arithmetic ops used as terms (e.g. K-V): wrap as compound */
            const char *op = e->kind==AST_ADD?"+":e->kind==AST_SUB?"-":e->kind==AST_MUL?"*":e->kind==AST_DIV?"/":"%";
            int atom = prolog_atom_intern(op);
            Term *args2[2]; args2[0]=pl_unified_term_from_expr(e->children[0],env); args2[1]=pl_unified_term_from_expr(e->children[1],env);
            return term_new_compound(atom, 2, args2);
        }
        case AST_UNIFY: {
            /* =/2 used as a term (e.g. G = (X = 5), assertz(p(X = 5))): wrap as
             * compound. Without this, the default arm below silently produced
             * atom("?") and any later call(G) / catch(G,_,_) saw a literal
             * `?` instead of the unification goal. (PL-12 latent bug.) */
            int atom = prolog_atom_intern("=");
            Term *args2[2];
            args2[0] = e->nchildren > 0 ? pl_unified_term_from_expr(e->children[0], env) : term_new_atom(atom);
            args2[1] = e->nchildren > 1 ? pl_unified_term_from_expr(e->children[1], env) : term_new_atom(atom);
            return term_new_compound(atom, 2, args2);
        }
        case AST_CUT:  return term_new_atom(prolog_atom_intern("!"));
        case AST_NUL:  return term_new_atom(prolog_atom_intern("[]"));
        case AST_FNC: {
            int arity = e->nchildren;
            int atom  = prolog_atom_intern(e->sval ? e->sval : "f");
            if (arity == 0) return term_new_atom(atom);
            Term **args = malloc(arity * sizeof(Term *));
            for (int i = 0; i < arity; i++) args[i] = pl_unified_term_from_expr(e->children[i], env);
            Term *t = term_new_compound(atom, arity, args);
            free(args);
            return t;
        }
        default: return term_new_atom(prolog_atom_intern("?"));
    }
}

/*---- pl_unified_deep_copy ----*/
static Term *pl_unified_deep_copy(Term *t) {
    t = term_deref(t);
    if (!t || t->tag == TT_VAR) return term_new_atom(prolog_atom_intern("_"));
    if (t->tag == TT_ATOM)  return term_new_atom(t->atom_id);
    if (t->tag == TT_INT)   return term_new_int(t->ival);
    if (t->tag == TT_FLOAT) return term_new_float(t->fval);
    if (t->tag == TT_COMPOUND) {
        Term **args = malloc(t->compound.arity * sizeof(Term *));
        for (int i = 0; i < t->compound.arity; i++) args[i] = pl_unified_deep_copy(t->compound.args[i]);
        Term *r = term_new_compound(t->compound.functor, t->compound.arity, args);
        free(args);
        return r;
    }
    return term_new_atom(prolog_atom_intern("_"));
}

/* F-3 RS-7: ISO mod — sign of divisor (floor division remainder).
 * Appears in AST_MOD case and AST_FNC "mod" case — unified here. */
static inline long pl_iso_mod(long n, long d) {
    if (!d) return 0;
    if (n == LONG_MIN && d == -1) return 0;
    long r = n % d;
    if (r != 0 && (r < 0) != (d < 0)) r += d;
    return r;
}

/*---- pl_unified_eval_arith_term — float-aware, returns Term* ----*/
static Term *pl_unified_eval_arith_term(AST_t *e, Term **env) {
    if (!e) return term_new_int(0);
    /* helper macros */
#define _EI(x) ({ Term *_t = pl_unified_eval_arith_term(x,env); (_t&&_t->tag==TT_INT)?_t->ival:(_t&&_t->tag==TT_FLOAT)?(long)_t->fval:0L; })
#define _ED(x) ({ Term *_t = pl_unified_eval_arith_term(x,env); (_t&&_t->tag==TT_FLOAT)?_t->fval:(_t&&_t->tag==TT_INT)?(double)_t->ival:0.0; })
#define _EIS(x) (pl_unified_eval_arith_term(x,env)->tag==TT_FLOAT)
#define _EF(op,a,b) ({ Term *_la=pl_unified_eval_arith_term(a,env),*_lb=pl_unified_eval_arith_term(b,env); \
                       int _fl=(_la&&_la->tag==TT_FLOAT)||(_lb&&_lb->tag==TT_FLOAT); \
                       _fl?term_new_float(_ED(a) op _ED(b)):term_new_int(_EI(a) op _EI(b)); })
    switch (e->kind) {
        case AST_ILIT: return term_new_int((long)e->ival);
        case AST_FLIT: return term_new_float(e->dval);
        case AST_VAR: {
            Term *t = term_deref(env && e->ival >= 0 ? env[e->ival] : NULL);
            if (!t || t->tag == TT_VAR) { pl_throw_instantiation_error(); return NULL; }
            return t;
        }
        case AST_ADD: {
            Term *la=pl_unified_eval_arith_term(e->children[0],env);
            Term *lb=pl_unified_eval_arith_term(e->children[1],env);
            int fl=(la&&la->tag==TT_FLOAT)||(lb&&lb->tag==TT_FLOAT);
            if (fl) return term_new_float(_ED(e->children[0]) + _ED(e->children[1]));
            /* signed-overflow detection — promote to float on wrap (matches SWI
             * bounded=true semantics: minint_promotion expects a float result). */
            long a=_EI(e->children[0]), b=_EI(e->children[1]), r;
            if (__builtin_add_overflow(a, b, &r))
                return term_new_float((double)a + (double)b);
            return term_new_int(r);
        }
        case AST_SUB: {
            Term *la=pl_unified_eval_arith_term(e->children[0],env);
            Term *lb=pl_unified_eval_arith_term(e->children[1],env);
            int fl=(la&&la->tag==TT_FLOAT)||(lb&&lb->tag==TT_FLOAT);
            if (fl) return term_new_float(_ED(e->children[0]) - _ED(e->children[1]));
            long a=_EI(e->children[0]), b=_EI(e->children[1]), r;
            if (__builtin_sub_overflow(a, b, &r))
                return term_new_float((double)a - (double)b);
            return term_new_int(r);
        }
        case AST_MUL: {
            Term *la=pl_unified_eval_arith_term(e->children[0],env);
            Term *lb=pl_unified_eval_arith_term(e->children[1],env);
            int fl=(la&&la->tag==TT_FLOAT)||(lb&&lb->tag==TT_FLOAT);
            if (fl) return term_new_float(_ED(e->children[0]) * _ED(e->children[1]));
            long a=_EI(e->children[0]), b=_EI(e->children[1]), r;
            if (__builtin_mul_overflow(a, b, &r))
                return term_new_float((double)a * (double)b);
            return term_new_int(r);
        }
        case AST_DIV: {
            Term *la=pl_unified_eval_arith_term(e->children[0],env);
            Term *lb=pl_unified_eval_arith_term(e->children[1],env);
            int fl=(la&&la->tag==TT_FLOAT)||(lb&&lb->tag==TT_FLOAT);
            if (fl) { double d=_ED(e->children[1]); return term_new_float(d?_ED(e->children[0])/d:0.0); }
            else    {
                long  n=_EI(e->children[0]);
                long  d=_EI(e->children[1]);
                if (!d) return term_new_int(0);
                /* INT_MIN/-1 overflows on x86 — would raise SIGFPE.
                 * Standard Prolog: throw evaluation_error(int_overflow);
                 * we return LONG_MIN to avoid the crash (matches GNU Prolog
                 * unbounded fallback shape; refined when bigint lands). */
                if (n == LONG_MIN && d == -1) return term_new_int(LONG_MIN);
                return term_new_int(n/d);
            }
        }
        case AST_MOD: return term_new_int(pl_iso_mod(_EI(e->children[0]), _EI(e->children[1])));
        case AST_FNC: {
            const char *fn = e->sval ? e->sval : "";
            /* two-arg integer bitwise / misc */
            if (strcmp(fn,"/\\")==0&&e->nchildren==2) return term_new_int(_EI(e->children[0])&_EI(e->children[1]));
            if (strcmp(fn,"\\/")==0&&e->nchildren==2) return term_new_int(_EI(e->children[0])|_EI(e->children[1]));
            if (strcmp(fn,"xor")==0&&e->nchildren==2) return term_new_int(_EI(e->children[0])^_EI(e->children[1]));
            if (strcmp(fn,"<<")==0&&e->nchildren==2)  return term_new_int(_EI(e->children[0])<<_EI(e->children[1]));
            if (strcmp(fn,">>")==0&&e->nchildren==2)  return term_new_int(_EI(e->children[0])>>_EI(e->children[1]));
            if (strcmp(fn,"\\")==0&&e->nchildren==1)  return term_new_int(~_EI(e->children[0]));
            if (strcmp(fn,"mod")==0&&e->nchildren==2) return term_new_int(pl_iso_mod(_EI(e->children[0]),_EI(e->children[1])));
            if (strcmp(fn,"rem")==0&&e->nchildren==2) {
                long n=_EI(e->children[0]); long d=_EI(e->children[1]);
                if (!d) return term_new_int(0);
                if (n == LONG_MIN && d == -1) return term_new_int(0);
                return term_new_int(n%d);  /* rem: truncating, sign of dividend */
            }
            /* two-arg float-aware */
            if (strcmp(fn,"**")==0&&e->nchildren==2) return term_new_float(pow(_ED(e->children[0]),_ED(e->children[1])));
            if (strcmp(fn,"^")==0&&e->nchildren==2) {
                /* ^ returns integer for non-negative integer exponents, else float */
                Term *la=pl_unified_eval_arith_term(e->children[0],env);
                Term *lb=pl_unified_eval_arith_term(e->children[1],env);
                if (la&&lb&&la->tag==TT_INT&&lb->tag==TT_INT&&lb->ival>=0) {
                    long base=la->ival, exp=lb->ival, acc=1;
                    while (exp-- > 0) acc *= base;
                    return term_new_int(acc);
                }
                return term_new_float(pow(_ED(e->children[0]),_ED(e->children[1])));
            }
            if (strcmp(fn,"max")==0&&e->nchildren==2) { Term *la=pl_unified_eval_arith_term(e->children[0],env),*lb=pl_unified_eval_arith_term(e->children[1],env); int fl=(la&&la->tag==TT_FLOAT)||(lb&&lb->tag==TT_FLOAT); return fl?(_ED(e->children[0])>=_ED(e->children[1])?la:lb):(_EI(e->children[0])>=_EI(e->children[1])?la:lb); }
            if (strcmp(fn,"min")==0&&e->nchildren==2) { Term *la=pl_unified_eval_arith_term(e->children[0],env),*lb=pl_unified_eval_arith_term(e->children[1],env); int fl=(la&&la->tag==TT_FLOAT)||(lb&&lb->tag==TT_FLOAT); return fl?(_ED(e->children[0])<=_ED(e->children[1])?la:lb):(_EI(e->children[0])<=_EI(e->children[1])?la:lb); }
            if (strcmp(fn,"gcd")==0&&e->nchildren==2) { long a=labs(_EI(e->children[0])),b=labs(_EI(e->children[1])); while(b){long r=a%b;a=b;b=r;} return term_new_int(a); }
            /* one-arg float ops */
            if (e->nchildren==1) {
                double d = _ED(e->children[0]);
                long   i = _EI(e->children[0]);
                int    isf = _EIS(e->children[0]);
                if (strcmp(fn,"sqrt")==0)               return term_new_float(sqrt(d));
                if (strcmp(fn,"sin")==0)                return term_new_float(sin(d));
                if (strcmp(fn,"cos")==0)                return term_new_float(cos(d));
                if (strcmp(fn,"tan")==0)                return term_new_float(tan(d));
                if (strcmp(fn,"exp")==0)                return term_new_float(exp(d));
                if (strcmp(fn,"log")==0)                return term_new_float(log(d));
                if (strcmp(fn,"float")==0)              return term_new_float(d);
                if (strcmp(fn,"float_integer_part")==0) return term_new_float(trunc(d));
                if (strcmp(fn,"float_fractional_part")==0) return term_new_float(d - trunc(d));
                if (strcmp(fn,"truncate")==0)           return term_new_int((long)d);
                if (strcmp(fn,"round")==0)              return term_new_int((long)round(d));
                if (strcmp(fn,"ceiling")==0)            return term_new_int((long)ceil(d));
                if (strcmp(fn,"floor")==0)              return term_new_int((long)floor(d));
                if (strcmp(fn,"abs")==0)                return isf ? term_new_float(fabs(d)) : term_new_int(i<0?-i:i);
                if (strcmp(fn,"sign")==0)               return isf ? term_new_float(d>0?1.0:d<0?-1.0:0.0) : term_new_int(i>0?1:i<0?-1:0);
                if (strcmp(fn,"-")==0)                  return isf ? term_new_float(-d) : term_new_int(-i);
            }
            /* atom constants */
            if (strcmp(fn,"pi")==0&&e->nchildren==0) return term_new_float(M_PI);
            if (strcmp(fn,"e")==0&&e->nchildren==0)  return term_new_float(M_E);
            /* unknown evaluable: resolve via env (e.g. TT_INT/TT_FLOAT atom from copy_term) */
            Term *t=term_deref(pl_unified_term_from_expr(e,env));
            if (!t || t->tag == TT_VAR) { pl_throw_instantiation_error(); return NULL; }
            if (t->tag == TT_INT || t->tag == TT_FLOAT) return t;
            /* truly non-evaluable: throw type_error(evaluable, Name/Arity) */
            pl_throw_type_error_evaluable(fn ? fn : "", e->nchildren);
            return NULL;
        }
        default: return term_new_int(0);
    }
#undef _EI
#undef _ED
#undef _EIS
#undef _EF
}

/*---- pl_unified_eval_arith — integer wrapper (kept for comparison callers) ----*/
static long pl_unified_eval_arith(AST_t *e, Term **env) {
    Term *t = pl_unified_eval_arith_term(e, env);
    if (!t) return 0;
    if (t->tag == TT_FLOAT) return (long)t->fval;
    if (t->tag == TT_INT)   return t->ival;
    return 0;
}

/*---- is_pl_user_call ----*/
int is_pl_user_call(AST_t *goal) {
    if (!goal || goal->kind != AST_FNC || !goal->sval) return 0;
    static const char *builtins[] = {
        "true","fail","halt","nl","write","writeln","print","writeq","write_canonical","tab","is",
        "<",">","=<",">=","=:=","=\\=","=","\\=","==","\\==",
        "@<","@>","@=<","@>=",
        "var","nonvar","atom","integer","float","compound","atomic","callable","is_list",
        "functor","arg","=..","\\+","not","once",",",";","->","findall",
        "assert","assertz","asserta","retract","retractall","abolish",
        "nv_get","nv_set",
        "term_string","number_codes","number_chars","char_code","upcase_atom","downcase_atom",
        "copy_term","atomic_list_concat","concat_atom","string_to_atom",
        "nb_setval","nb_getval","aggregate_all","throw","catch",
        "phrase",
        "dynamic","discontiguous","module","use_module","ensure_loaded","style_check",
        "set_prolog_flag","current_prolog_flag","module_info","if","else","endif",
        "meta_predicate","module_transparent","multifile","include",
        "$clausable","public","volatile","thread_local","table","set_test_options","encoding",
        "format","succ","plus","number_vars","numbervars","char_type","term_singletons",
        "atom_length","atom_chars","atom_codes","atom_concat","atom_string",
        "number_string","string_length","string_concat","string_codes","string_chars",
        "string_to_atom","sub_atom","atom_number","atom_to_term","msort","sort","compare",
        "between","succ_or_zero","forall","aggregate_all","length",
        "read_term","write_term","with_output_to","initialization","call","setup_call_cleanup",
        "@<","@>","@=<","@>=",
        NULL
    };
    for (int i = 0; builtins[i]; i++) if (strcmp(goal->sval, builtins[i]) == 0) return 0;
    return 1;
}

/*---- term_order_cmp — ISO standard order: var < number < atom < compound ----*/
static int term_order_cmp(Term *a, Term *b) {
    a = term_deref(a); b = term_deref(b);
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return  1;
    /* Type rank: var=0, number=1, atom=2, compound=3 */
    int ra = (a->tag==TT_VAR)?0:(a->tag==TT_INT||a->tag==TT_FLOAT)?1:(a->tag==TT_ATOM)?2:3;
    int rb = (b->tag==TT_VAR)?0:(b->tag==TT_INT||b->tag==TT_FLOAT)?1:(b->tag==TT_ATOM)?2:3;
    if (ra != rb) return ra - rb;
    switch (a->tag) {
        case TT_VAR:   return (int)(a - b);  /* address order for unbound vars */
        case TT_INT:   return (a->ival < b->ival) ? -1 : (a->ival > b->ival) ? 1 : 0;
        case TT_FLOAT: return (a->fval < b->fval) ? -1 : (a->fval > b->fval) ? 1 : 0;
        case TT_ATOM: {
            const char *sa = prolog_atom_name(a->atom_id);
            const char *sb = prolog_atom_name(b->atom_id);
            return strcmp(sa ? sa : "", sb ? sb : "");
        }
        case TT_COMPOUND: {
            /* arity first, then functor name, then args left-to-right */
            if (a->compound.arity != b->compound.arity) return a->compound.arity - b->compound.arity;
            const char *fa = prolog_atom_name(a->compound.functor);
            const char *fb = prolog_atom_name(b->compound.functor);
            int c = strcmp(fa ? fa : "", fb ? fb : "");
            if (c) return c;
            for (int i = 0; i < a->compound.arity; i++) {
                c = term_order_cmp(a->compound.args[i], b->compound.args[i]);
                if (c) return c;
            }
            return 0;
        }
        default: return 0;
    }
}

/* =========================================================================
 * copy_term helper — deep-copy a term, replacing variables with fresh ones
 * Uses a simple var-mapping array: old_var[i] → new_var[i]
 * ======================================================================= */
#define COPY_TERM_MAX_VARS 256
typedef struct { Term *orig; Term *copy; } CopyVarMap;

static Term *copy_term_rec(Term *t, CopyVarMap *map, int *nmap) {
    t = term_deref(t);
    if (!t) {
        /* unbound variable — create or reuse fresh var.
         * NOTE: var_slot must be non-negative so bind() trails this var.
         * -1 means "anonymous wildcard, don't trail" — wrong for copy_term.
         * PL-12 session #7: use synthetic slot 1<<20 + nmap to avoid env
         * collisions (real env slots are 0..arity, typically <64). */
        Term *fresh = term_new_var((1 << 20) + *nmap);
        return fresh;
    }
    switch (t->tag) {
        case TT_VAR: {
            /* unbound — look up or create a fresh copy */
            for (int i = 0; i < *nmap; i++)
                if (map[i].orig == t) return map[i].copy;
            /* PL-12 session #7: see comment above — slot != -1 needed. */
            Term *fresh = term_new_var((1 << 20) + *nmap);
            if (*nmap < COPY_TERM_MAX_VARS) {
                map[*nmap].orig = t; map[*nmap].copy = fresh; (*nmap)++;
            }
            return fresh;
        }
        case TT_ATOM:  return term_new_atom(t->atom_id);
        case TT_INT:   return term_new_int(t->ival);
        case TT_FLOAT: return term_new_float(t->fval);
        case TT_COMPOUND: {
            int ar = t->compound.arity;
            Term **args = malloc(ar * sizeof(Term *));
            for (int i = 0; i < ar; i++)
                args[i] = copy_term_rec(t->compound.args[i], map, nmap);
            Term *r = term_new_compound(t->compound.functor, ar, args);
            free(args);
            return r;
        }
        default: return t;
    }
}

static Term *pl_copy_term(Term *t) {
    CopyVarMap map[COPY_TERM_MAX_VARS];
    int nmap = 0;
    return copy_term_rec(t, map, &nmap);
}

/*============================================================================
 * pl_term_to_synth_expr / pl_invoke_var_goal — Term→EXPR bridge for var-bound
 * goal dispatch (catch/3, \+/1, once/1, not/1).
 *
 * Defect background (PL-12 fix #2 v3):
 * When `catch(G,_,_)`'s G is a runtime variable (e.g. plunit asserts test
 * goals into a clause and dispatches them via catch), the EXPR at the catch
 * call site has goal_e->kind == AST_VAR. The previous fall-through dispatched
 * `interp_exec_pl_builtin(AST_VAR, env)` which had no AST_VAR case and returned
 * 1 (silent success) via the `default:` arm. The Var-bound goal never ran.
 * Same defect afflicts \+/1, once/1, not/1 (lines 681/689).
 *
 * Lifecycle / env-sharing strategy:
 * Session #4 (2026-04-30 #4) attempted a bridge that allocated a separate
 * tenv decoupled from the caller's env, then dispatched via pl_box_goal_from_ir
 * + bb_broker. That collapsed the SWI suite 43→7. Root cause hypothesis: the
 * bb_broker's choice-point machinery interacted badly with the tenv slot
 * Term*s.
 *
 * v3 simplification: the synth EXPR's AST_VAR slots store **the original Term**
 * (deref'd) directly. Dispatch goes through `interp_exec_pl_builtin(synth, tenv)`
 * — same routine the surrounding catch code already uses for non-Var goals.
 * No new bb_broker entry. When the synth AST_VAR is resolved by
 * `pl_unified_term_from_expr(child=AST_VAR, env=tenv)`, it returns `tenv[k]`
 * which IS the original Term* from the goal. Bindings made on that Term
 * (TT_REF chained back to the caller's env vars) propagate naturally; on
 * caller's unify-stage uargs[k] is bound TT_REF -> tenv[k] which itself is
 * TT_REF -> caller_env[m], so the chain flows correctly.
 *
 * Only the kinds plunit-style goal Terms actually carry are handled:
 *   TT_VAR    -> AST_VAR slot (deduped by pointer identity, X+X = one slot)
 *   TT_INT    -> AST_ILIT
 *   TT_FLOAT  -> AST_FLIT
 *   TT_ATOM   -> AST_FNC nchildren=0 sval=name (true/fail/etc. handled there)
 *   TT_COMPOUND with functor f arity n:
 *     `=` arity 2  -> AST_UNIFY
 *     `+`/`-`/`*`/`/`/`mod` arity 2 -> AST_ADD/AST_SUB/AST_MUL/AST_DIV/AST_MOD
 *       (only when used inside is/2 RHS or similar arith-eval context;
 *       at goal level these aren't valid Prolog goals — but interp_exec_pl_builtin
 *       still recurses on children, so the kinds match what lower_term emits)
 *     anything else -> AST_FNC sval=fn nchildren=n
 *
 * Memory: synth EXPR allocated on heap, freed on exit. Var dedup via
 * pointer-identity scan of tenv (linear; fine for typical goal sizes <16).
 *==========================================================================*/

#define PL_SYNTH_TENV_MAX 64

static AST_t *pl_synth_new(AST_e k) {
    AST_t *e = (AST_t *)calloc(1, sizeof(AST_t));
    e->kind = k;
    return e;
}

static void pl_synth_add_child(AST_t *e, AST_t *c) {
    if (e->nchildren >= e->nalloc) {
        e->nalloc = e->nalloc ? e->nalloc * 2 : 4;
        e->children = (AST_t **)realloc(e->children, e->nalloc * sizeof(AST_t *));
    }
    e->children[e->nchildren++] = c;
}

static void pl_synth_free(AST_t *e) {
    if (!e) return;
    for (int i = 0; i < e->nchildren; i++) pl_synth_free(e->children[i]);
    free(e->children);
    if (e->sval) free(e->sval);
    free(e);
}

static int pl_tenv_add_dedup(Term **tenv, int *pn, Term *v) {
    /* Dedup by pointer identity — same Term twice -> same slot */
    for (int i = 0; i < *pn; i++) if (tenv[i] == v) return i;
    if (*pn >= PL_SYNTH_TENV_MAX) return -1;
    tenv[*pn] = v;
    return (*pn)++;
}

static AST_t *pl_term_to_synth_expr(Term *t, Term **tenv, int *pn) {
    t = term_deref(t);
    if (!t) {
        AST_t *e = pl_synth_new(AST_FNC);
        e->sval = strdup("[]");
        return e;
    }
    switch (t->tag) {
        case TT_VAR: {
            int slot = pl_tenv_add_dedup(tenv, pn, t);
            AST_t *e = pl_synth_new(AST_VAR);
            e->ival = slot >= 0 ? slot : 0;
            return e;
        }
        case TT_INT: {
            AST_t *e = pl_synth_new(AST_ILIT);
            e->ival = t->ival;
            return e;
        }
        case TT_FLOAT: {
            AST_t *e = pl_synth_new(AST_FLIT);
            e->dval = t->fval;
            return e;
        }
        case TT_ATOM: {
            const char *nm = prolog_atom_name(t->atom_id);
            AST_t *e = pl_synth_new(AST_FNC);
            e->sval = strdup(nm ? nm : "");
            return e;
        }
        case TT_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            int arity = t->compound.arity;
            /* =/2 -> AST_UNIFY */
            if (arity == 2 && strcmp(fn, "=") == 0) {
                AST_t *e = pl_synth_new(AST_UNIFY);
                pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[0], tenv, pn));
                pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[1], tenv, pn));
                return e;
            }
            /* arith ops -> AST_ADD etc. only meaningful inside is/2 RHS, but
             * interp_exec_pl_builtin only recurses on children of recognised
             * top-level goals — this is fine because is/2 calls
             * pl_unified_eval_arith_term which knows these kinds. */
            if (arity == 2) {
                AST_e ak = AST_KIND_COUNT;
                if      (!strcmp(fn,"+"))   ak = AST_ADD;
                else if (!strcmp(fn,"-"))   ak = AST_SUB;
                else if (!strcmp(fn,"*"))   ak = AST_MUL;
                else if (!strcmp(fn,"/"))   ak = AST_DIV;
                else if (!strcmp(fn,"mod")) ak = AST_MOD;
                if (ak != AST_KIND_COUNT) {
                    AST_t *e = pl_synth_new(ak);
                    pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[0], tenv, pn));
                    pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[1], tenv, pn));
                    return e;
                }
            }
            /* General compound -> AST_FNC sval=fn */
            AST_t *e = pl_synth_new(AST_FNC);
            e->sval = strdup(fn);
            for (int i = 0; i < arity; i++)
                pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[i], tenv, pn));
            return e;
        }
    }
    /* unreachable */
    return pl_synth_new(AST_FNC);
}

/* pl_invoke_var_goal — entry point.
 * Caller passes a Term that should be invoked as a goal. Returns 1 = success,
 * 0 = failure. Throws are propagated via the existing catch-frame longjmp
 * mechanism; this routine itself does not catch them.
 *
 * NOTE: caller_env is unused by this function — the bridge owns its tenv —
 * but is kept in the signature for future use (e.g. if we ever need to alias
 * synth AST_VAR slots to specific caller_env[k] indices instead of via tenv
 * Term*s). Today it's unused; tenv-by-Term*-identity already preserves the
 * caller's variable bindings via the deref chains. */
static int pl_invoke_var_goal(Term *gt, Term **caller_env) {
    (void)caller_env;
    gt = term_deref(gt);
    if (!gt) return 0;
    /* Unbound var as goal: instantiation_error in standard Prolog; we just fail. */
    if (gt->tag == TT_VAR)   return 0;
    /* Numbers aren't callable */
    if (gt->tag == TT_INT)   return 0;
    if (gt->tag == TT_FLOAT) return 0;

    Term *tenv[PL_SYNTH_TENV_MAX];
    for (int i = 0; i < PL_SYNTH_TENV_MAX; i++) tenv[i] = NULL;
    int    tn = 0;
    AST_t *synth = pl_term_to_synth_expr(gt, tenv, &tn);
    int ok = interp_exec_pl_builtin(synth, tenv);
    pl_synth_free(synth);
    return ok;
}

/*---- interp_exec_pl_builtin — execute one Prolog builtin goal ----*/
/* Uses file-scope globals g_pl_trail, g_pl_cut_flag, g_pl_pred_table, g_pl_env.
 * Returns 1=success, 0=fail. Called by pl_box_builtin in pl_broker.c. */
int interp_exec_pl_builtin(AST_t *goal, Term **env) {
    if (!goal) return 1;
    Trail *trail = &g_pl_trail;
    int *cut_flag = &g_pl_cut_flag;
    switch (goal->kind) {
        case AST_UNIFY: {
            Term *t1=pl_unified_term_from_expr(goal->children[0],env);
            Term *t2=pl_unified_term_from_expr(goal->children[1],env);
            int mark=trail_mark(trail);
            if (!unify(t1,t2,trail)){trail_unwind(trail,mark);return 0;}
            return 1;
        }
        case AST_CUT: if (cut_flag) *cut_flag=1; return 1;
        case AST_TRAIL_MARK: case AST_TRAIL_UNWIND: return 1;
        case AST_FNC: {
            const char *fn = goal->sval ? goal->sval : "true";
            int arity = goal->nchildren;
            /* ---- user-defined predicate dispatch (must come before builtin checks) ---- */
            if (is_pl_user_call(goal)) {
                char ukey[256]; snprintf(ukey,sizeof ukey,"%s/%d",fn,arity);
                AST_t *uch = pl_pred_table_lookup(&g_pl_pred_table, ukey);
                if (uch) {
                    int ua = arity;
                    Term **uenv = ua ? pl_env_new(ua) : NULL;
                    for (int ui = 0; ui < ua; ui++)
                        uenv[ui] = pl_unified_term_from_expr(goal->children[ui], env);
                    Term **sv = g_pl_env; g_pl_env = uenv;
                    DESCR_t rd = bb_eval_value(uch); g_pl_env = sv;
                    if (uenv) free(uenv);
                    return !IS_FAIL_fn(rd);
                }
                fprintf(stderr,"prolog: undefined predicate %s/%d\n",fn,arity);
                return pl_throw_existence_error_procedure(fn ? fn : "", arity);
            }
            if (strcmp(fn,"true")==0&&arity==0) return 1;
            if (strcmp(fn,"fail")==0&&arity==0) return 0;
            if (strcmp(fn,"halt")==0&&arity==0) exit(0);
            if (strcmp(fn,"halt")==0&&arity==1){Term *t=term_deref(pl_unified_term_from_expr(goal->children[0],env));exit(t&&t->tag==TT_INT?(int)t->ival:0);}
            /* ---- directive no-ops: dynamic, discontiguous, module, use_module, include, etc. ---- */
            if ((strcmp(fn,"dynamic")==0||strcmp(fn,"discontiguous")==0||
                 strcmp(fn,"module")==0||strcmp(fn,"use_module")==0||
                 strcmp(fn,"ensure_loaded")==0||strcmp(fn,"style_check")==0||
                 strcmp(fn,"set_prolog_flag")==0||strcmp(fn,"module_info")==0||
                 strcmp(fn,"if")==0||strcmp(fn,"else")==0||strcmp(fn,"endif")==0||
                 strcmp(fn,"meta_predicate")==0||strcmp(fn,"module_transparent")==0||
                 strcmp(fn,"multifile")==0||strcmp(fn,"$clausable")==0||
                 strcmp(fn,"public")==0||strcmp(fn,"volatile")==0||
                 strcmp(fn,"thread_local")==0||strcmp(fn,"table")==0||
                 strcmp(fn,"set_test_options")==0||strcmp(fn,"encoding")==0)) return 1;
            /* include/1: silently succeed (file already parsed by prolog_compile) */
            if (strcmp(fn,"include")==0) return 1;
            if (strcmp(fn,"nl")==0&&arity==0){putchar('\n');return 1;}
            if (strcmp(fn,"write")==0&&arity==1){pl_write(pl_unified_term_from_expr(goal->children[0],env));return 1;}
            if (strcmp(fn,"writeln")==0&&arity==1){pl_write(pl_unified_term_from_expr(goal->children[0],env));putchar('\n');return 1;}
            if (strcmp(fn,"print")==0&&arity==1){pl_write(pl_unified_term_from_expr(goal->children[0],env));return 1;}
            if (strcmp(fn,"writeq")==0&&arity==1){pl_writeq(pl_unified_term_from_expr(goal->children[0],env));return 1;}
            if (strcmp(fn,"write_canonical")==0&&arity==1){pl_write_canonical(pl_unified_term_from_expr(goal->children[0],env));return 1;}
            if (strcmp(fn,"tab")==0&&arity==1){
                Term *t=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                long n=(t&&t->tag==TT_INT)?t->ival:0;
                for(long i=0;i<n;i++) putchar(' ');
                return 1;
            }
            if (strcmp(fn,"is")==0&&arity==2){
                Term *val=pl_unified_eval_arith_term(goal->children[1],env);
                Term *lhs=pl_unified_term_from_expr(goal->children[0],env);
                int mark=trail_mark(trail);
                if(!unify(lhs,val,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* arithmetic comparisons */
            { struct{const char *n;int op;}cmps[]={{"<",0},{">",1},{"=<",2},{">=",3},{"=:=",4},{"=\\=",5},{NULL,0}};
              for(int ci=0;cmps[ci].n;ci++) if(strcmp(fn,cmps[ci].n)==0&&arity==2){
                  long a=pl_unified_eval_arith(goal->children[0],env),b=pl_unified_eval_arith(goal->children[1],env);
                  switch(cmps[ci].op){case 0:return a<b;case 1:return a>b;case 2:return a<=b;case 3:return a>=b;case 4:return a==b;case 5:return a!=b;}
              }
            }
            if (strcmp(fn,"=")==0&&arity==2){
                int mark=trail_mark(trail);
                if(!unify(pl_unified_term_from_expr(goal->children[0],env),pl_unified_term_from_expr(goal->children[1],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"\\=")==0&&arity==2){
                int mark=trail_mark(trail);
                int ok=unify(pl_unified_term_from_expr(goal->children[0],env),pl_unified_term_from_expr(goal->children[1],env),trail);
                trail_unwind(trail,mark);return !ok;
            }
            if (strcmp(fn,"==")==0&&arity==2){
                Term *t1=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *t2=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                if(!t1||!t2)return t1==t2;
                if(t1->tag!=t2->tag)return 0;
                if(t1->tag==TT_ATOM)return t1->atom_id==t2->atom_id;
                if(t1->tag==TT_INT) return t1->ival==t2->ival;
                if(t1->tag==TT_VAR) return t1==t2;
                return 0;
            }
            if (strcmp(fn,"\\==")==0&&arity==2){
                Term *t1=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *t2=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                if(!t1||!t2)return t1!=t2;
                if(t1->tag!=t2->tag)return 1;
                if(t1->tag==TT_ATOM)return t1->atom_id!=t2->atom_id;
                if(t1->tag==TT_INT) return t1->ival!=t2->ival;
                if(t1->tag==TT_VAR) return t1!=t2;
                return 1;
            }
            /* type tests */
            if (arity==1){
                Term *t=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                if(strcmp(fn,"var"     )==0)return !t||t->tag==TT_VAR;
                if(strcmp(fn,"nonvar"  )==0)return  t&&t->tag!=TT_VAR;
                if(strcmp(fn,"atom"    )==0)return  t&&t->tag==TT_ATOM;
                if(strcmp(fn,"integer" )==0)return  t&&t->tag==TT_INT;
                if(strcmp(fn,"float"   )==0)return  t&&t->tag==TT_FLOAT;
                if(strcmp(fn,"compound")==0)return  t&&t->tag==TT_COMPOUND;
                if(strcmp(fn,"atomic"  )==0)return  t&&(t->tag==TT_ATOM||t->tag==TT_INT||t->tag==TT_FLOAT);
                if(strcmp(fn,"callable")==0)return  t&&(t->tag==TT_ATOM||t->tag==TT_COMPOUND);
                if(strcmp(fn,"is_list" )==0){
                    int nil=prolog_atom_intern("[]"),dot=prolog_atom_intern(".");
                    for(Term *c=t;;){c=term_deref(c);if(!c)return 0;if(c->tag==TT_ATOM&&c->atom_id==nil)return 1;if(c->tag!=TT_COMPOUND||c->compound.arity!=2||c->compound.functor!=dot)return 0;c=c->compound.args[1];}
                }
            }
            /* ,/N conjunction — run each child goal in sequence */
            if (strcmp(fn,",")==0){
                for(int i=0;i<goal->nchildren;i++){
                    AST_t *g=goal->children[i];
                    if(!g) continue;
                    int ok = is_pl_user_call(g) ? ({
                        char key[256]; snprintf(key,sizeof key,"%s/%d",g->sval?g->sval:"",g->nchildren);
                        AST_t *ch=pl_pred_table_lookup(&g_pl_pred_table,key);
                        int r=0;
                        if(ch){ int ca=g->nchildren; Term **cargs=ca?malloc(ca*sizeof(Term*)):NULL;
                                 for(int a=0;a<ca;a++) cargs[a]=pl_unified_term_from_expr(g->children[a],env);
                                 Term **sv=g_pl_env; g_pl_env=cargs;
                                 DESCR_t rd=bb_eval_value(ch); g_pl_env=sv; if(cargs)free(cargs);
                                 r=!IS_FAIL_fn(rd); }
                        r; }) : interp_exec_pl_builtin(g, env);
                    if(!ok) return 0;
                }
                return 1;
            }
            /* ;/N disjunction */
            if (strcmp(fn,";")==0&&arity>=2){
                AST_t *left=goal->children[0],*right=goal->children[1];
                /* if-then-else: (Cond -> Then ; Else) */
                if(left&&left->kind==AST_FNC&&left->sval&&strcmp(left->sval,"->")==0&&left->nchildren>=2){
                    int mark=trail_mark(trail); int cut2=0;
                    if(interp_exec_pl_builtin(left->children[0],env)){
                        for(int i=1;i<left->nchildren;i++) if(!interp_exec_pl_builtin(left->children[i],env)) return 0;
                        return 1;
                    }
                    trail_unwind(trail,mark);
                    return interp_exec_pl_builtin(right,env);
                }
                /* plain disjunction */
                {int mark=trail_mark(trail);
                 if(interp_exec_pl_builtin(left,env)) return 1;
                 trail_unwind(trail,mark);
                 return interp_exec_pl_builtin(right,env);}
            }
            /* ->/N if-then */
            if (strcmp(fn,"->")==0&&arity>=2){
                if(!interp_exec_pl_builtin(goal->children[0],env)) return 0;
                for(int i=1;i<goal->nchildren;i++) if(!interp_exec_pl_builtin(goal->children[i],env)) return 0;
                return 1;
            }
            /* \+/not — PR-19b: var-goal arg dispatches via Term→EXPR bridge.
             * Without this, child[0]->kind == AST_VAR fell through to the
             * `default: return 1;` arm of interp_exec_pl_builtin (silent
             * success), so `\+ Var` always returned 0 (negation of fake
             * success) and the inner goal never ran. Same defect as the
             * catch/3 fix in PR-19a, applied here for \+/not. */
            if ((strcmp(fn,"\\+")==0||strcmp(fn,"not")==0)&&arity==1){
                int mark=trail_mark(trail);
                int ok;
                if (goal->children[0] && goal->children[0]->kind == AST_VAR) {
                    Term *gt = pl_unified_term_from_expr(goal->children[0], env);
                    ok = pl_invoke_var_goal(gt, env);
                } else {
                    ok = interp_exec_pl_builtin(goal->children[0],env);
                }
                trail_unwind(trail,mark);return !ok;
            }
            /* once/1 — succeed/fail like the goal; tree-walker has no
             * choice points to discard, so semantically equivalent to
             * a plain call here. Trail rolled back on failure.
             * PR-19b: var-goal arg routed through Term→EXPR bridge —
             * same silent-success defect as catch/3 / \+/not. */
            if (strcmp(fn,"once")==0&&arity==1){
                int mark=trail_mark(trail);
                int ok;
                if (goal->children[0] && goal->children[0]->kind == AST_VAR) {
                    Term *gt = pl_unified_term_from_expr(goal->children[0], env);
                    ok = pl_invoke_var_goal(gt, env);
                } else {
                    ok = interp_exec_pl_builtin(goal->children[0],env);
                }
                if(!ok) trail_unwind(trail,mark);
                return ok;
            }
            /* call/N — PR-19c: dispatch goal-as-variable or reconstruct compound.
             *
             * call(G)          — G is a callable Term; dispatch via bridge.
             * call(G, A1, ...) — G is an atom or compound; build G(G_args..., A1...)
             *                    and dispatch the reconstructed compound.
             *
             * When call's first child is AST_VAR (goal_e->kind == AST_VAR after
             * resolving child[0]), the deref'd Term becomes the goal or its
             * base.  Extra args (call/2, call/3, ...) are resolved and appended.
             *
             * For call/1 with no extra args: straightforward bridge dispatch.
             * For call/N (N>1): reconstruct a fresh TT_COMPOUND Term whose
             * functor = G's functor and whose args = G's existing args ++ extras.
             * Then dispatch via pl_invoke_var_goal on the reconstructed Term.
             */
            if (strcmp(fn,"call")==0 && arity>=1) {
                AST_t *g_expr = goal->children[0];
                /* Resolve the goal arg to a Term (handles AST_VAR and direct AST_FNC) */
                Term *g_term = pl_unified_term_from_expr(g_expr, env);
                g_term = term_deref(g_term);
                if (!g_term) return 0;

                int n_extra = arity - 1; /* number of extra args (call/2 → 1, etc.) */

                if (n_extra == 0) {
                    /* call(G) — simple dispatch via bridge */
                    return pl_invoke_var_goal(g_term, env);
                }

                /* call(G, A1, ...) — build reconstructed compound */
                /* Extract base functor name and existing args from g_term */
                const char *cfn = NULL;
                int carity_base = 0;
                Term **cargs_base = NULL;
                if (g_term->tag == TT_ATOM) {
                    cfn = prolog_atom_name(g_term->atom_id);
                    carity_base = 0; cargs_base = NULL;
                } else if (g_term->tag == TT_COMPOUND) {
                    cfn = prolog_atom_name(g_term->compound.functor);
                    carity_base = g_term->compound.arity;
                    cargs_base = g_term->compound.args;
                } else {
                    return 0; /* not callable */
                }
                if (!cfn) return 0;

                /* Resolve extra args from the call/N expression */
                int total_arity = carity_base + n_extra;
                Term **all_args = (Term **)malloc(total_arity * sizeof(Term *));
                for (int i = 0; i < carity_base; i++)
                    all_args[i] = term_deref(cargs_base[i]);
                for (int i = 0; i < n_extra; i++)
                    all_args[carity_base + i] =
                        pl_unified_term_from_expr(goal->children[1 + i], env);

                /* Build a fresh TT_COMPOUND for the reconstructed call */
                int fn_id = prolog_atom_intern(cfn);
                Term **rargs2 = (Term **)malloc(total_arity * sizeof(Term *));
                for (int i = 0; i < total_arity; i++) rargs2[i] = all_args[i];
                free(all_args);
                Term *reconstructed = term_new_compound(fn_id, total_arity, rargs2);
                free(rargs2);

                /* Dispatch via bridge (reconstructed is a concrete Term, not AST_VAR,
                 * so pl_invoke_var_goal handles it correctly via the TT_COMPOUND path) */
                return pl_invoke_var_goal(reconstructed, env);
            }
            /* functor/3 */
            if (strcmp(fn,"functor")==0&&arity==3){
                int mark=trail_mark(trail);
                if(!pl_functor(pl_unified_term_from_expr(goal->children[0],env),pl_unified_term_from_expr(goal->children[1],env),pl_unified_term_from_expr(goal->children[2],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* arg/3 */
            if (strcmp(fn,"arg")==0&&arity==3){
                int mark=trail_mark(trail);
                if(!pl_arg(pl_unified_term_from_expr(goal->children[0],env),pl_unified_term_from_expr(goal->children[1],env),pl_unified_term_from_expr(goal->children[2],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* =../2 */
            if (strcmp(fn,"=..")==0&&arity==2){
                int mark=trail_mark(trail);
                if(!pl_univ(pl_unified_term_from_expr(goal->children[0],env),pl_unified_term_from_expr(goal->children[1],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* assert/assertz/asserta/retract/retractall/abolish */
            if ((strcmp(fn,"assert")==0||strcmp(fn,"assertz")==0)&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->children[0],env);
                return pl_assert_clause(arg,1);
            }
            if (strcmp(fn,"asserta")==0&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->children[0],env);
                return pl_assert_clause(arg,0);
            }
            if ((strcmp(fn,"retract")==0||strcmp(fn,"retractall")==0)&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->children[0],env);
                return pl_retract_clause(arg);
            }
            if (strcmp(fn,"abolish")==0&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->children[0],env);
                return pl_abolish_pred(arg);
            }
            /*---- atom builtins (PL-4) ----*/
            /* atom_length(+Atom, ?Len) */
            if (strcmp(fn,"atom_length")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *l=pl_unified_term_from_expr(goal->children[1],env);
                const char *s=NULL;
                if (a&&a->tag==TT_ATOM) s=prolog_atom_name(a->atom_id);
                else if (a&&a->tag==TT_INT) { char buf[32]; snprintf(buf,sizeof buf,"%ld",a->ival); s=buf; }
                if (!s) return 0;
                Term *len=term_new_int((long)strlen(s));
                int mark=trail_mark(trail);
                if (!unify(l,len,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* atom_concat(+A1, +A2, ?A3)  or  atom_concat(?A1, ?A2, +A3) */
            if (strcmp(fn,"atom_concat")==0&&arity==3) {
                Term *a1=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *a2=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                Term *a3=pl_unified_term_from_expr(goal->children[2],env);
                /* mode: +,+,? */
                if (a1&&a1->tag==TT_ATOM&&a2&&a2->tag==TT_ATOM) {
                    const char *s1=prolog_atom_name(a1->atom_id);
                    const char *s2=prolog_atom_name(a2->atom_id);
                    size_t l1=strlen(s1),l2=strlen(s2);
                    char *buf=malloc(l1+l2+1);
                    memcpy(buf,s1,l1); memcpy(buf+l1,s2,l2); buf[l1+l2]='\0';
                    Term *res=term_new_atom(prolog_atom_intern(buf)); free(buf);
                    int mark=trail_mark(trail);
                    if(!unify(a3,res,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                /* mode: ?,?,+ — split */
                Term *a3d=term_deref(a3);
                if (a3d&&a3d->tag==TT_ATOM) {
                    const char *s3=prolog_atom_name(a3d->atom_id);
                    size_t n=strlen(s3);
                    for (size_t i=0;i<=n;i++) {
                        char *left=malloc(i+1); memcpy(left,s3,i); left[i]='\0';
                        char *right=malloc(n-i+1); memcpy(right,s3+i,n-i); right[n-i]='\0';
                        Term *tl=term_new_atom(prolog_atom_intern(left));
                        Term *tr=term_new_atom(prolog_atom_intern(right));
                        free(left); free(right);
                        int mark=trail_mark(trail);
                        if (unify(a1,tl,trail)&&unify(a2,tr,trail)) return 1;
                        trail_unwind(trail,mark);
                    }
                    return 0;
                }
                return 0;
            }
            /* atom_chars(+Atom, ?Chars)  or  atom_chars(?Atom, +Chars) */
            if (strcmp(fn,"atom_chars")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *cl=pl_unified_term_from_expr(goal->children[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (a&&a->tag==TT_ATOM) {
                    /* +Atom → build char list */
                    const char *s=prolog_atom_name(a->atom_id);
                    int n=(int)strlen(s);
                    Term *lst=term_new_atom(nil_id);
                    for(int i=n-1;i>=0;i--){
                        char ch[2]={s[i],'\0'};
                        Term *args2[2]; args2[0]=term_new_atom(prolog_atom_intern(ch)); args2[1]=lst;
                        lst=term_new_compound(dot_id,2,args2);
                    }
                    int mark=trail_mark(trail);
                    if(!unify(cl,lst,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else {
                    /* +Chars → build atom */
                    Term *cur=term_deref(cl); char buf[1024]; int pos=0;
                    while(cur&&cur->tag==TT_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TT_ATOM) return 0;
                        const char *cs=prolog_atom_name(hd->atom_id);
                        if(pos<1023) buf[pos++]=cs[0];
                        cur=term_deref(cur->compound.args[1]);
                    }
                    buf[pos]='\0';
                    Term *res=term_new_atom(prolog_atom_intern(buf));
                    int mark=trail_mark(trail);
                    if(!unify(a,res,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
            }
            /* atom_codes(+Atom, ?Codes)  or  atom_codes(?Atom, +Codes) */
            if (strcmp(fn,"atom_codes")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *cl=pl_unified_term_from_expr(goal->children[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (a&&a->tag==TT_ATOM) {
                    const char *s=prolog_atom_name(a->atom_id);
                    int n=(int)strlen(s);
                    Term *lst=term_new_atom(nil_id);
                    for(int i=n-1;i>=0;i--){
                        Term *args2[2]; args2[0]=term_new_int((unsigned char)s[i]); args2[1]=lst;
                        lst=term_new_compound(dot_id,2,args2);
                    }
                    int mark=trail_mark(trail);
                    if(!unify(cl,lst,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else {
                    Term *cur=term_deref(cl); char buf[1024]; int pos=0;
                    while(cur&&cur->tag==TT_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TT_INT) return 0;
                        if(pos<1023) buf[pos++]=(char)hd->ival;
                        cur=term_deref(cur->compound.args[1]);
                    }
                    buf[pos]='\0';
                    Term *res=term_new_atom(prolog_atom_intern(buf));
                    int mark=trail_mark(trail);
                    if(!unify(a,res,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
            }
            /*---- term ordering: @</2, @>/2, @=</2, @>=/2, compare/3 (PL-5) ----*/
            if (arity==2&&(strcmp(fn,"@<")==0||strcmp(fn,"@>")==0||strcmp(fn,"@=<")==0||strcmp(fn,"@>=")==0)) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                int c=term_order_cmp(a,b);
                if (strcmp(fn,"@<")==0)  return c<0;
                if (strcmp(fn,"@>")==0)  return c>0;
                if (strcmp(fn,"@=<")==0) return c<=0;
                if (strcmp(fn,"@>=")==0) return c>=0;
            }
            if (strcmp(fn,"compare")==0&&arity==3) {
                Term *order=pl_unified_term_from_expr(goal->children[0],env);
                Term *a=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->children[2],env));
                int c=term_order_cmp(a,b);
                const char *os=c<0?"<":c>0?">":"=";
                Term *ot=term_new_atom(prolog_atom_intern(os));
                int mark=trail_mark(trail);
                if(!unify(order,ot,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /*---- sort/2 and msort/2 (PL-5) ----*/
            if ((strcmp(fn,"sort")==0||strcmp(fn,"msort")==0)&&arity==2) {
                int do_dedup=(strcmp(fn,"sort")==0);
                Term *lst=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *out=pl_unified_term_from_expr(goal->children[1],env);
                /* collect list into array */
                Term *elems[4096]; int n=0;
                Term *cur=lst;
                int nil_id=prolog_atom_intern("[]"),dot_id=prolog_atom_intern(".");
                while(cur&&cur->tag==TT_COMPOUND&&cur->compound.arity==2) {
                    if(n<4096) elems[n++]=term_deref(cur->compound.args[0]);
                    cur=term_deref(cur->compound.args[1]);
                }
                /* insertion sort (small lists; good enough for ladder) */
                for(int i=1;i<n;i++){
                    Term *key=elems[i]; int j=i-1;
                    while(j>=0&&term_order_cmp(elems[j],key)>0){elems[j+1]=elems[j];j--;}
                    elems[j+1]=key;
                }
                /* dedup for sort/2 */
                if(do_dedup&&n>0){
                    int w=0;
                    for(int i=0;i<n;i++)
                        if(i==0||term_order_cmp(elems[i-1],elems[i])!=0) elems[w++]=elems[i];
                    n=w;
                }
                /* build result list */
                Term *res=term_new_atom(nil_id);
                for(int i=n-1;i>=0;i--){Term *a2[2];a2[0]=elems[i];a2[1]=res;res=term_new_compound(dot_id,2,a2);}
                int mark=trail_mark(trail);
                if(!unify(out,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /*---- succ/2, plus/3, format/2 (PL-6) ----*/
            /* succ(?X, ?Y)  — Y = X+1, at least one must be bound */
            if (strcmp(fn,"succ")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                int mark=trail_mark(trail);
                if (a&&a->tag==TT_INT) {
                    if (a->ival < 0) return 0;
                    Term *r=term_new_int(a->ival+1);
                    if(!unify(b,r,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else if (b&&b->tag==TT_INT) {
                    if (b->ival <= 0) return 0;
                    Term *r=term_new_int(b->ival-1);
                    if(!unify(a,r,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            /* plus(?X, ?Y, ?Z)  — Z = X+Y, at least two must be bound */
            if (strcmp(fn,"plus")==0&&arity==3) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                Term *c=term_deref(pl_unified_term_from_expr(goal->children[2],env));
                int mark=trail_mark(trail);
                int ai=(a&&a->tag==TT_INT),bi=(b&&b->tag==TT_INT),ci=(c&&c->tag==TT_INT);
                Term *r=NULL;
                Term *tgt=NULL;
                if (ai&&bi)        { r=term_new_int(a->ival+b->ival); tgt=c; }
                else if (ai&&ci)   { r=term_new_int(c->ival-a->ival); tgt=b; }
                else if (bi&&ci)   { r=term_new_int(c->ival-b->ival); tgt=a; }
                else return 0;
                if(!unify(tgt,r,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* format(+Fmt, +Args) / format(+Fmt) — ~w ~a ~d ~i ~n ~N ~t~| ~` */
            if (strcmp(fn,"format")==0&&(arity==1||arity==2)) {
                Term *fmt_t=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                const char *fmt=NULL;
                if (fmt_t&&fmt_t->tag==TT_ATOM) fmt=prolog_atom_name(fmt_t->atom_id);
                if (!fmt) return 0;
                /* Build args list */
                Term *args_list=(arity==2)?term_deref(pl_unified_term_from_expr(goal->children[1],env)):NULL;
                int nil_id=prolog_atom_intern("[]");
                /* Process format string */
                for (const char *p=fmt; *p; p++) {
                    if (*p=='~') {
                        p++;
                        if (*p=='w'||*p=='a'||*p=='p') {
                            /* ~w/~a/~p: write next arg */
                            if (args_list&&args_list->tag==TT_COMPOUND&&args_list->compound.arity==2) {
                                pl_write(term_deref(args_list->compound.args[0]));
                                args_list=term_deref(args_list->compound.args[1]);
                            }
                        } else if (*p=='d') {
                            if (args_list&&args_list->tag==TT_COMPOUND&&args_list->compound.arity==2) {
                                Term *h=term_deref(args_list->compound.args[0]);
                                if (h&&h->tag==TT_INT) printf("%ld",h->ival);
                                args_list=term_deref(args_list->compound.args[1]);
                            }
                        } else if (*p=='i') {
                            /* ~i: ignore next arg */
                            if (args_list&&args_list->tag==TT_COMPOUND&&args_list->compound.arity==2)
                                args_list=term_deref(args_list->compound.args[1]);
                        } else if (*p=='n') { putchar('\n');
                        } else if (*p=='N') { /* newline if not at start of line — approx */ putchar('\n');
                        } else if (*p=='~') { putchar('~');
                        } else if (*p=='t') { putchar('\t');
                        } else if (*p=='r') { /* radix — skip for now */ }
                    } else {
                        putchar(*p);
                    }
                }
                return 1;
            }
            /*---- numbervars/3 and char_type/2 (PL-7) ----*/
            /* numbervars(+Term, +Start, -End) — bind unbound vars to '$VAR'(N) */
            if (strcmp(fn,"numbervars")==0&&arity==3) {
                Term *t   = pl_unified_term_from_expr(goal->children[0],env);
                Term *s   = term_deref(pl_unified_term_from_expr(goal->children[1],env));
                Term *end = pl_unified_term_from_expr(goal->children[2],env);
                if (!s||s->tag!=TT_INT) return 0;
                long n = s->ival;
                int var_id = prolog_atom_intern("$VAR");
                /* Iterative DFS to number all unbound vars */
                Term *stk[4096]; int top=0;
                stk[top++]=term_deref(t);
                while (top>0) {
                    Term *cur=term_deref(stk[--top]);
                    if (!cur) continue;
                    if (cur->tag==TT_VAR) {
                        Term *narg=term_new_int(n++);
                        Term *binding=term_new_compound(var_id,1,&narg);
                        cur->ref=binding; cur->tag=TT_REF;
                    } else if (cur->tag==TT_COMPOUND) {
                        for (int i=cur->compound.arity-1;i>=0;i--)
                            if (top<4095) stk[top++]=term_deref(cur->compound.args[i]);
                    }
                }
                Term *end_t=term_new_int(n);
                int mark=trail_mark(trail);
                if(!unify(end,end_t,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* term_singletons(+Term, -Singletons) — list of variables that
             * occur exactly once in Term. We walk the term, gather all
             * unbound TT_VAR pointers, count occurrences, and unify Singletons
             * with the list of those that appear once.  Order: leftmost-first
             * (matches SWI). */
            if (strcmp(fn,"term_singletons")==0&&arity==2) {
                Term *t   = pl_unified_term_from_expr(goal->children[0],env);
                Term *out = pl_unified_term_from_expr(goal->children[1],env);
                /* Two passes: collect unique vars in order, then count */
                Term *vars[1024]; int counts[1024]; int nv=0;
                Term *stk[4096]; int top=0;
                stk[top++]=term_deref(t);
                while (top>0) {
                    Term *cur=term_deref(stk[--top]);
                    if (!cur) continue;
                    if (cur->tag==TT_VAR) {
                        int found=-1;
                        for (int i=0;i<nv;i++) if (vars[i]==cur) { found=i; break; }
                        if (found>=0) counts[found]++;
                        else if (nv<1024) { vars[nv]=cur; counts[nv]=1; nv++; }
                    } else if (cur->tag==TT_COMPOUND) {
                        for (int i=cur->compound.arity-1;i>=0;i--)
                            if (top<4095) stk[top++]=term_deref(cur->compound.args[i]);
                    }
                }
                /* Build list of singletons (count==1) in collection order */
                int dot=prolog_atom_intern("."), nil=prolog_atom_intern("[]");
                Term *list = term_new_atom(nil);
                for (int i=nv-1;i>=0;i--) {
                    if (counts[i]==1) {
                        Term *args[2] = { vars[i], list };
                        list = term_new_compound(dot, 2, args);
                    }
                }
                int mark=trail_mark(trail);
                if(!unify(out,list,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* char_type(+Char, ?Type) */
            if (strcmp(fn,"char_type")==0&&arity==2) {
                Term *ch=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *ty=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                if (!ch||ch->tag!=TT_ATOM) return 0;
                const char *cs=prolog_atom_name(ch->atom_id);
                if (!cs||!cs[0]) return 0;
                unsigned char c=(unsigned char)cs[0];
                if (!ty||ty->tag==TT_VAR) return 0; /* need bound type for now */
                const char *tname=NULL;
                int tarity=0;
                if (ty->tag==TT_ATOM) { tname=prolog_atom_name(ty->atom_id); tarity=0; }
                else if (ty->tag==TT_COMPOUND) { tname=prolog_atom_name(ty->compound.functor); tarity=ty->compound.arity; }
                if (!tname) return 0;
                if (strcmp(tname,"alpha")==0)   return isalpha(c)?1:0;
                if (strcmp(tname,"alnum")==0)   return isalnum(c)?1:0;
                if (strcmp(tname,"space")==0||strcmp(tname,"white")==0) return isspace(c)?1:0;
                if (strcmp(tname,"upper")==0&&tarity==0) return isupper(c)?1:0;
                if (strcmp(tname,"lower")==0&&tarity==0) return islower(c)?1:0;
                if (strcmp(tname,"punct")==0)   return ispunct(c)?1:0;
                if (strcmp(tname,"ascii")==0&&tarity==0) return (c<128)?1:0;
                if (strcmp(tname,"end_of_line")==0) return (c=='\n'||c=='\r')?1:0;
                if (strcmp(tname,"digit")==0&&tarity==0) return isdigit(c)?1:0;
                if (strcmp(tname,"digit")==0&&tarity==1) {
                    if (!isdigit(c)) return 0;
                    Term *d=term_new_int(c-'0');
                    int mark=trail_mark(trail);
                    if(!unify(ty->compound.args[0],d,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                if (strcmp(tname,"upper")==0&&tarity==1) {
                    if (!isupper(c)) return 0;
                    char low[2]={(char)tolower(c),'\0'};
                    Term *d=term_new_atom(prolog_atom_intern(low));
                    int mark=trail_mark(trail);
                    if(!unify(ty->compound.args[0],d,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                if (strcmp(tname,"lower")==0&&tarity==1) {
                    if (!islower(c)) return 0;
                    char up[2]={(char)toupper(c),'\0'};
                    Term *d=term_new_atom(prolog_atom_intern(up));
                    int mark=trail_mark(trail);
                    if(!unify(ty->compound.args[0],d,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                if (strcmp(tname,"to_upper")==0&&tarity==1) {
                    char up[2]={(char)toupper(c),'\0'};
                    Term *d=term_new_atom(prolog_atom_intern(up));
                    int mark=trail_mark(trail);
                    if(!unify(ty->compound.args[0],d,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                if (strcmp(tname,"to_lower")==0&&tarity==1) {
                    char low[2]={(char)tolower(c),'\0'};
                    Term *d=term_new_atom(prolog_atom_intern(low));
                    int mark=trail_mark(trail);
                    if(!unify(ty->compound.args[0],d,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            /* U-23: nv_get(+Name, -Val) / nv_set(+Name, +Val) -- SNO NV store bridge */
            if (strcmp(fn,"nv_get")==0&&arity==2) {
                Term *nm=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                if (!nm || nm->tag != TT_ATOM) return 0;
                const char *nm_str = prolog_atom_name(nm->atom_id);
                DESCR_t dv = NV_GET_fn(nm_str);
                Term *val_t = IS_FAIL_fn(dv) ? term_new_atom(ATOM_NIL) :
                              (dv.v==DT_I) ? term_new_int(dv.i) :
                              term_new_atom(prolog_atom_intern(dv.s ? dv.s : ""));
                Term *lhs=pl_unified_term_from_expr(goal->children[1],env);
                int mark=trail_mark(trail);
                if(!unify(lhs,val_t,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"nv_set")==0&&arity==2) {
                Term *nm=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *vl=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                if (!nm || nm->tag != TT_ATOM) return 0;
                const char *nm_str = prolog_atom_name(nm->atom_id);
                const char *vl_str = (vl && vl->tag==TT_ATOM) ? prolog_atom_name(vl->atom_id) : NULL;
                DESCR_t dv = (vl && vl->tag==TT_INT) ? INTVAL(vl->ival) :
                             vl_str                   ? STRVAL(vl_str) : NULVCL;
                NV_SET_fn(nm_str, dv);
                return 1;
            }
            /*---- PL-9: string/IO builtins ----*/
            /* term_string(+Term, -String) or term_string(-Term, +String) */
            if (strcmp(fn,"term_string")==0&&arity==2) {
                Term *ta=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *ts=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                if (ta && ta->tag!=TT_VAR) {
                    /* mode: +Term, -String — render term as atom */
                    char *buf = pl_term_to_string(ta);
                    Term *str = term_new_atom(prolog_atom_intern(buf));
                    free(buf);
                    int mark=trail_mark(trail);
                    if(!unify(ts,str,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else if (ts && ts->tag==TT_ATOM) {
                    /* mode: -Term, +String — parse atom as term (simple: just intern as atom) */
                    const char *s = prolog_atom_name(ts->atom_id);
                    if (!s) return 0;
                    /* Try to parse as integer */
                    char *end; long iv = strtol(s, &end, 10);
                    Term *parsed;
                    if (*end=='\0') parsed = term_new_int(iv);
                    else            parsed = term_new_atom(prolog_atom_intern(s));
                    int mark=trail_mark(trail);
                    if(!unify(ta,parsed,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            /* number_codes(+Number, ?Codes) or number_codes(?Number, +Codes) */
            if (strcmp(fn,"number_codes")==0&&arity==2) {
                Term *tn=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tc=pl_unified_term_from_expr(goal->children[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (tn && (tn->tag==TT_INT||tn->tag==TT_FLOAT)) {
                    char buf[64];
                    if (tn->tag==TT_INT) snprintf(buf,sizeof buf,"%ld",tn->ival);
                    else snprintf(buf,sizeof buf,"%g",tn->fval);
                    int n=(int)strlen(buf);
                    Term *lst=term_new_atom(nil_id);
                    for(int i=n-1;i>=0;i--){Term *a2[2];a2[0]=term_new_int((unsigned char)buf[i]);a2[1]=lst;lst=term_new_compound(dot_id,2,a2);}
                    int mark=trail_mark(trail);
                    if(!unify(tc,lst,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else {
                    /* +Codes → Number */
                    Term *cur=term_deref(tc); char buf[64]; int pos=0;
                    while(cur&&cur->tag==TT_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TT_INT) return 0;
                        if(pos<63) buf[pos++]=(char)hd->ival;
                        cur=term_deref(cur->compound.args[1]);
                    }
                    buf[pos]='\0';
                    char *end; long iv=strtol(buf,&end,10);
                    Term *num = (*end=='\0') ? term_new_int(iv) : term_new_atom(prolog_atom_intern(buf));
                    int mark=trail_mark(trail);
                    if(!unify(tn,num,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
            }
            /* number_chars(+Number, ?Chars) or number_chars(?Number, +Chars) */
            if (strcmp(fn,"number_chars")==0&&arity==2) {
                Term *tn=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tc=pl_unified_term_from_expr(goal->children[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (tn && (tn->tag==TT_INT||tn->tag==TT_FLOAT)) {
                    char buf[64];
                    if (tn->tag==TT_INT) snprintf(buf,sizeof buf,"%ld",tn->ival);
                    else snprintf(buf,sizeof buf,"%g",tn->fval);
                    int n=(int)strlen(buf);
                    Term *lst=term_new_atom(nil_id);
                    for(int i=n-1;i>=0;i--){char ch[2]={buf[i],'\0'};Term *a2[2];a2[0]=term_new_atom(prolog_atom_intern(ch));a2[1]=lst;lst=term_new_compound(dot_id,2,a2);}
                    int mark=trail_mark(trail);
                    if(!unify(tc,lst,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else {
                    Term *cur=term_deref(tc); char buf[64]; int pos=0;
                    while(cur&&cur->tag==TT_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TT_ATOM) return 0;
                        const char *cs=prolog_atom_name(hd->atom_id);
                        if(pos<63) buf[pos++]=cs?cs[0]:0;
                        cur=term_deref(cur->compound.args[1]);
                    }
                    buf[pos]='\0';
                    char *end; long iv=strtol(buf,&end,10);
                    Term *num = (*end=='\0') ? term_new_int(iv) : term_new_atom(prolog_atom_intern(buf));
                    int mark=trail_mark(trail);
                    if(!unify(tn,num,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
            }
            /* char_code(+Char, ?Code) or char_code(?Char, +Code) */
            if (strcmp(fn,"char_code")==0&&arity==2) {
                Term *tch=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tco=term_deref(pl_unified_term_from_expr(goal->children[1],env));
                if (tch && tch->tag==TT_ATOM) {
                    const char *s=prolog_atom_name(tch->atom_id);
                    Term *code=term_new_int(s?(unsigned char)s[0]:0);
                    int mark=trail_mark(trail);
                    if(!unify(tco,code,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else if (tco && tco->tag==TT_INT) {
                    char ch[2]={(char)tco->ival,'\0'};
                    Term *cat=term_new_atom(prolog_atom_intern(ch));
                    int mark=trail_mark(trail);
                    if(!unify(tch,cat,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            /* upcase_atom(+Atom, -Upper) */
            if (strcmp(fn,"upcase_atom")==0&&arity==2) {
                Term *ta=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tr=pl_unified_term_from_expr(goal->children[1],env);
                if (!ta||ta->tag!=TT_ATOM) return 0;
                const char *s=prolog_atom_name(ta->atom_id); if(!s) return 0;
                char *buf=malloc(strlen(s)+1);
                for(int i=0;s[i];i++) buf[i]=(char)toupper((unsigned char)s[i]); buf[strlen(s)]='\0';
                Term *res=term_new_atom(prolog_atom_intern(buf)); free(buf);
                int mark=trail_mark(trail);
                if(!unify(tr,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* downcase_atom(+Atom, -Lower) */
            if (strcmp(fn,"downcase_atom")==0&&arity==2) {
                Term *ta=term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tr=pl_unified_term_from_expr(goal->children[1],env);
                if (!ta||ta->tag!=TT_ATOM) return 0;
                const char *s=prolog_atom_name(ta->atom_id); if(!s) return 0;
                char *buf=malloc(strlen(s)+1);
                for(int i=0;s[i];i++) buf[i]=(char)tolower((unsigned char)s[i]); buf[strlen(s)]='\0';
                Term *res=term_new_atom(prolog_atom_intern(buf)); free(buf);
                int mark=trail_mark(trail);
                if(!unify(tr,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* sub_atom/5: sub_atom(+Atom, ?Before, ?Length, ?After, ?SubAtom)
             * Unifies SubAtom with a sub-atom of Atom starting at Before (0-based),
             * with Length chars, and After = len(Atom)-Before-Length.
             * On backtrack (via findall), yields all solutions. */
            if (strcmp(fn,"sub_atom")==0&&arity==5) {
                Term *ta  = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tb  = pl_unified_term_from_expr(goal->children[1],env); /* Before */
                Term *tl  = pl_unified_term_from_expr(goal->children[2],env); /* Length */
                Term *taf = pl_unified_term_from_expr(goal->children[3],env); /* After  */
                Term *ts  = pl_unified_term_from_expr(goal->children[4],env); /* SubAtom */
                if (!ta || ta->tag != TT_ATOM) return 0;
                const char *s = prolog_atom_name(ta->atom_id); if (!s) return 0;
                int slen = (int)strlen(s);
                /* Determine Before and Length if bound */
                int bef = -1, len = -1;
                Term *tbv = term_deref(tb); if (tbv && tbv->tag == TT_INT) bef = (int)tbv->ival;
                Term *tlv = term_deref(tl); if (tlv && tlv->tag == TT_INT) len = (int)tlv->ival;
                /* If both bound, single deterministic solution */
                if (bef >= 0 && len >= 0) {
                    if (bef + len > slen) return 0;
                    int aft = slen - bef - len;
                    char *sub = malloc(len+1); memcpy(sub, s+bef, len); sub[len]='\0';
                    Term *rsub = term_new_atom(prolog_atom_intern(sub)); free(sub);
                    int mark = trail_mark(trail);
                    if (!unify(tb, term_new_int(bef), trail)) { trail_unwind(trail,mark); return 0; }
                    if (!unify(tl, term_new_int(len), trail)) { trail_unwind(trail,mark); return 0; }
                    if (!unify(taf, term_new_int(aft), trail)) { trail_unwind(trail,mark); return 0; }
                    if (!unify(ts, rsub, trail)) { trail_unwind(trail,mark); return 0; }
                    return 1;
                }
                /* If Before bound but Length free: try all lengths at that position */
                if (bef >= 0) {
                    for (int l2 = 0; l2 <= slen - bef; l2++) {
                        int aft = slen - bef - l2;
                        char *sub = malloc(l2+1); memcpy(sub, s+bef, l2); sub[l2]='\0';
                        Term *rsub = term_new_atom(prolog_atom_intern(sub)); free(sub);
                        int mark = trail_mark(trail);
                        int ok = unify(tl, term_new_int(l2), trail) &&
                                 unify(taf, term_new_int(aft), trail) &&
                                 unify(ts, rsub, trail);
                        if (ok) return 1;
                        trail_unwind(trail, mark);
                    }
                    return 0;
                }
                /* General: iterate all (bef, len) pairs — used by findall */
                for (int b2 = 0; b2 <= slen; b2++) {
                    for (int l2 = 0; l2 <= slen - b2; l2++) {
                        int aft = slen - b2 - l2;
                        char *sub = malloc(l2+1); memcpy(sub, s+b2, l2); sub[l2]='\0';
                        Term *rsub = term_new_atom(prolog_atom_intern(sub)); free(sub);
                        int mark = trail_mark(trail);
                        int ok = unify(tb,  term_new_int(b2),  trail) &&
                                 unify(tl,  term_new_int(l2),  trail) &&
                                 unify(taf, term_new_int(aft), trail) &&
                                 unify(ts,  rsub,              trail);
                        if (ok) return 1;
                        trail_unwind(trail, mark);
                    }
                }
                return 0;
            }
            /* atom_number/2: atom_number(?Atom, ?Number)
             * atom_number('+3.14', N) -> N=3.14; atom_number(A, 42) -> A='42' */
            if (strcmp(fn,"atom_number")==0&&arity==2) {
                Term *ta = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tn = pl_unified_term_from_expr(goal->children[1],env);
                Term *tnv = term_deref(tn);
                if (ta && ta->tag == TT_ATOM) {
                    const char *s = prolog_atom_name(ta->atom_id); if (!s) return 0;
                    char *end = NULL;
                    long iv = strtol(s, &end, 10);
                    Term *num = NULL;
                    if (end && *end == '\0') num = term_new_int(iv);
                    else { double dv = strtod(s, &end); if (end && *end == '\0') num = term_new_float(dv); }
                    if (!num) return 0;
                    int mark = trail_mark(trail);
                    if (!unify(tn, num, trail)) { trail_unwind(trail,mark); return 0; }
                    return 1;
                } else if (tnv && (tnv->tag == TT_INT || tnv->tag == TT_FLOAT)) {
                    char buf[64];
                    if (tnv->tag == TT_INT) snprintf(buf, sizeof buf, "%ld", tnv->ival);
                    else snprintf(buf, sizeof buf, "%g", tnv->fval);
                    Term *rat = term_new_atom(prolog_atom_intern(buf));
                    int mark = trail_mark(trail);
                    if (!unify(ta, rat, trail)) { trail_unwind(trail,mark); return 0; }
                    return 1;
                }
                return 0;
            }
            /* atom_to_term/3: atom_to_term(+Atom, -Term, -Bindings)
             * or atom_to_term(-Atom, +Term, +Bindings)
             * Parse atom as a Prolog term; Bindings is list of 'Name'=Var pairs. */
            if (strcmp(fn,"atom_to_term")==0&&arity==3) {
                Term *ta   = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *tt   = pl_unified_term_from_expr(goal->children[1],env);
                Term *tbnd = pl_unified_term_from_expr(goal->children[2],env);
                if (ta && ta->tag == TT_ATOM) {
                    /* Parse mode: atom -> term + bindings */
                    const char *src = prolog_atom_name(ta->atom_id);
                    if (!src) return 0;
                    /* Build a temporary program string "tmp :- " + src + "." */
                    /* Simple: use atom_chars to build a term from parsing */
                    /* For the test cases we handle: atoms and simple compounds */
                    /* Full parser reuse is complex; implement naive approach:
                     * call prolog_compile on a temporary directive and extract */
                    /* Minimal: treat atom as-is for atomic terms, else parse */
                    /* For test 04: 'foo(1,2)' -> foo(1,2), [] */
                    /* We use the existing prolog frontend parser */
                    char *prog = malloc(strlen(src)+32);
                    snprintf(prog, strlen(src)+32, ":- X = (%s).\n", src);
                    /* We can't easily invoke the parser here without a reentrant API.
                     * Implement a simple recursive-descent term reader for common cases. */
                    free(prog);
                    /* Fallback: treat atom as itself if atomic */
                    /* For compound terms in atom form, use term_string path */
                    /* Check if it's a simple atom (no parens/commas) */
                    const char *p = src;
                    while (*p && *p != '(' && *p != ',') p++;
                    Term *parsed = NULL;
                    if (*p == '\0') {
                        /* simple atom or number */
                        char *end; long iv = strtol(src, &end, 10);
                        if (*end=='\0') parsed = term_new_int(iv);
                        else { double dv = strtod(src,&end); if(*end=='\0') parsed=term_new_float(dv); }
                        if (!parsed) parsed = term_new_atom(prolog_atom_intern(src));
                    } else {
                        /* compound: parse functor(args) — delegate to term_string */
                        /* Use existing number_codes/atom_codes + term_string if available */
                        /* For now: reconstruct via prolog_atom for the functor name */
                        /* Extract functor name up to '(' */
                        int flen = (int)(strchr(src,'(') - src);
                        char *fname2 = malloc(flen+1); strncpy(fname2,src,flen); fname2[flen]='\0';
                        int fatom = prolog_atom_intern(fname2); free(fname2);
                        /* Count args (naive: count commas at depth 1) */
                        int depth=0, argc=1; const char *q=strchr(src,'(')+1;
                        const char *qend = src+strlen(src)-1; /* points at ')' */
                        for (const char *r=q; r<qend; r++) {
                            if (*r=='(') depth++;
                            else if (*r==')') depth--;
                            else if (*r==',' && depth==0) argc++;
                        }
                        Term **args = malloc(argc*sizeof(Term*));
                        /* parse each arg as integer/float/atom (naive) */
                        int ai=0; const char *astart=q;
                        depth=0;
                        for (const char *r=q; r<=qend; r++) {
                            if (*r=='(') depth++;
                            else if (*r==')') depth--;
                            int sep = (r==qend) || (*r==',' && depth==0);
                            if (sep) {
                                int alen=(int)(r-astart);
                                char *abuf=malloc(alen+1); strncpy(abuf,astart,alen); abuf[alen]='\0';
                                /* trim spaces */
                                char *at=abuf; while(*at==' ')at++;
                                char *end2; long iv2=strtol(at,&end2,10);
                                if (*end2=='\0') args[ai]=term_new_int(iv2);
                                else { double dv2=strtod(at,&end2); if(*end2=='\0') args[ai]=term_new_float(dv2); else args[ai]=term_new_atom(prolog_atom_intern(at)); }
                                free(abuf); ai++;
                                astart=r+1;
                            }
                        }
                        parsed = term_new_compound(fatom, argc, args); free(args);
                    }
                    int mark = trail_mark(trail);
                    Term *empty_bindings = term_new_atom(prolog_atom_intern("[]"));
                    if (!unify(tt, parsed, trail)) { trail_unwind(trail,mark); return 0; }
                    if (!unify(tbnd, empty_bindings, trail)) { trail_unwind(trail,mark); return 0; }
                    return 1;
                } else {
                    /* Write mode: term -> atom */
                    Term *tv = term_deref(tt);
                    if (!tv) return 0;
                    /* Convert term to atom string via pl_write to buffer */
                    char *buf = NULL; size_t bsz = 0;
                    FILE *mf = open_memstream(&buf, &bsz);
                    if (!mf) return 0;
                    FILE *old_stdout = stdout; /* redirect pl_write */
                    /* pl_write goes to stdout; use dup2 trick or buffer */
                    fflush(stdout);
                    int pipefd[2]; pipe(pipefd);
                    int saved_fd = dup(1); dup2(pipefd[1], 1); close(pipefd[1]);
                    pl_write(tv);
                    fflush(stdout);
                    dup2(saved_fd, 1); close(saved_fd);
                    /* read from pipe */
                    char rbuf[4096]; int nr = (int)read(pipefd[0], rbuf, sizeof(rbuf)-1);
                    close(pipefd[0]);
                    fclose(mf); free(buf);
                    if (nr < 0) nr = 0; rbuf[nr] = '\0';
                    Term *rat2 = term_new_atom(prolog_atom_intern(rbuf));
                    int mark2 = trail_mark(trail);
                    if (!unify(ta, rat2, trail)) { trail_unwind(trail,mark2); return 0; }
                    return 1;
                }
            }
            /* phrase/2,3 — call a DCG rule: phrase(Rule, List) or phrase(Rule, List, Rest) */
            /* phrase/2,3 — call a DCG rule: phrase(Rule, List) or phrase(Rule, List, Rest) */
            if ((strcmp(fn,"phrase")==0) && (arity==2||arity==3)) {
                Term *rule = term_deref(pl_unified_term_from_expr(goal->children[0], env));
                Term *s0   = pl_unified_term_from_expr(goal->children[1], env);
                Term *s1   = (arity==3) ? pl_unified_term_from_expr(goal->children[2], env)
                                        : term_new_atom(prolog_atom_intern("[]"));
                if (!rule) return 0;
                /* Extract functor name and existing args from rule term */
                const char *rfn = NULL;
                int rarity = 0;
                Term **rargs = NULL;
                if (rule->tag == TT_ATOM) {
                    rfn = prolog_atom_name(rule->atom_id);
                    rarity = 0; rargs = NULL;
                } else if (rule->tag == TT_COMPOUND) {
                    rfn = prolog_atom_name(rule->compound.functor);
                    rarity = rule->compound.arity;
                    rargs = rule->compound.args;
                }
                if (!rfn) return 0;
                /* Build call: rfn(rargs..., s0, s1) — arity = rarity+2 */
                int call_arity = rarity + 2;
                char ukey[256]; snprintf(ukey, sizeof ukey, "%s/%d", rfn, call_arity);
                AST_t *uch = pl_pred_table_lookup(&g_pl_pred_table, ukey);
                if (!uch) return 0;
                Term **uargs = pl_env_new(call_arity);
                for (int ui = 0; ui < rarity; ui++) uargs[ui] = term_deref(rargs[ui]);
                uargs[rarity]   = s0;
                uargs[rarity+1] = s1;
                Trail *utrail = &g_pl_trail;
                int umark = trail_mark(utrail);
                Term **saved_env = g_pl_env;
                g_pl_env = uargs;
                /* CH-17e: prefer SM-expression path when entry_pc resolved */
                Pl_PredEntry *_upe1 = pl_pred_entry_lookup(ukey);
                bb_node_t uroot = (_upe1 && _upe1->entry_pc >= 0)
                    ? pl_box_choice_pc(_upe1->entry_pc, g_pl_env, call_arity)
                    : pl_box_choice(uch, g_pl_env, call_arity);
                int uok = bb_broker(uroot, BB_ONCE, NULL, NULL);
                g_pl_env = saved_env;
                if (!uok) trail_unwind(utrail, umark);
                if (uargs) free(uargs);
                return uok;
            }
            /* findall/3 — collect ALL solutions via bb_broker retry loop */
            if (strcmp(fn,"findall")==0&&arity==3){
                AST_t *tmpl_expr=goal->children[0];
                AST_t *goal_expr=goal->children[1];
                AST_t *list_expr=goal->children[2];
                Term **solutions=NULL; int nsol=0,sol_cap=0;
                /* Isolate in sub-trail so bindings don't leak to parent */
                Trail fa_trail; trail_init(&fa_trail);
                Trail saved_global_trail=g_pl_trail;  /* save by value — NOT pointer (self-alias bug) */
                g_pl_trail=fa_trail;
                /* Build a box for the goal and drive α/β to exhaustion.
                 * PR-19d bridge: if goal_expr is AST_VAR, deref to a Term and
                 * build a synth EXPR so the goal can be retried across solutions.
                 * outer_env is kept for tmpl_expr/list_expr which belong to the
                 * static IR and must still resolve against the caller's env. */
                AST_t *fa_synth = NULL; Term **fa_tenv = NULL;
                Term **outer_env = env;
                if (goal_expr && goal_expr->kind == AST_VAR) {
                    Term *gt = term_deref(pl_unified_term_from_expr(goal_expr, env));
                    fa_tenv = (Term **)calloc(PL_SYNTH_TENV_MAX, sizeof(Term *));
                    int fa_tn = 0;
                    fa_synth = pl_term_to_synth_expr(gt, fa_tenv, &fa_tn);
                    goal_expr = fa_synth;
                    env = fa_tenv;
                }
                bb_node_t goal_box=pl_box_goal_from_ir(goal_expr,env);
                DESCR_t fa_r=goal_box.fn(goal_box.ζ,α);
                while(!IS_FAIL_fn(fa_r)){
                    /* PL-12 session #7: use pl_copy_term (preserves var
                     * sharing within snapshot via CopyVarMap) instead of
                     * pl_unified_deep_copy (collapsed every TT_VAR to atom `_`,
                     * which destroyed test goal vars carried through findall).
                     * plunit's pj_run_suite stores test bodies as findall
                     * snapshots; without this fix the goals would lose their
                     * var bindings even before reaching catch's bridge. */
                    Term *snap=pl_copy_term(pl_unified_term_from_expr(tmpl_expr,outer_env));
                    if(nsol>=sol_cap){sol_cap=sol_cap?sol_cap*2:8;solutions=realloc(solutions,sol_cap*sizeof(Term*));}
                    solutions[nsol++]=snap;
                    fa_r=goal_box.fn(goal_box.ζ,β);
                }
                g_pl_trail=saved_global_trail;  /* restore parent trail */
                if (fa_synth) { pl_synth_free(fa_synth); free(fa_tenv); }
                int nil_id=prolog_atom_intern("[]"),dot_id=prolog_atom_intern(".");
                Term *lst=term_new_atom(nil_id);
                for(int i=nsol-1;i>=0;i--){Term *a2[2];a2[0]=solutions[i];a2[1]=lst;lst=term_new_compound(dot_id,2,a2);}
                free(solutions);
                Term *list_term=pl_unified_term_from_expr(list_expr,outer_env);
                int u_mark=trail_mark(trail);
                if(!unify(list_term,lst,trail)){trail_unwind(trail,u_mark);return 0;}
                return 1;
            }
            /* Look up user-defined predicate in global pred table (assertz/asserta support) */
            {
                char ukey[256]; snprintf(ukey, sizeof ukey, "%s/%d", fn, arity);
                AST_t *uch = pl_pred_table_lookup(&g_pl_pred_table, ukey);
                if (uch) {
                    Term **uargs = (arity > 0) ? pl_env_new(arity) : NULL;
                    /* Unify call arguments into fresh env */
                    Trail *utrail = &g_pl_trail;
                    int umark = trail_mark(utrail);
                    int uok = 1;
                    for (int ui = 0; ui < arity && uok; ui++) {
                        Term *actual = pl_unified_term_from_expr(goal->children[ui], env);
                        if (!unify(uargs[ui], actual, utrail)) { uok = 0; }
                    }
                    if (uok) {
                        Term **saved_env = g_pl_env;
                        g_pl_env = uargs;
                        /* CH-17e: prefer SM-expression path when entry_pc resolved */
                        Pl_PredEntry *_upe2 = pl_pred_entry_lookup(ukey);
                        bb_node_t uroot = (_upe2 && _upe2->entry_pc >= 0)
                            ? pl_box_choice_pc(_upe2->entry_pc, g_pl_env, arity)
                            : pl_box_choice(uch, g_pl_env, arity);
                        uok = bb_broker(uroot, BB_ONCE, NULL, NULL);
                        g_pl_env = saved_env;
                    }
                    if (!uok) trail_unwind(utrail, umark);
                    if (uargs) free(uargs);
                    return uok;
                }
            }
            /* ---- copy_term/2 (PL-10) ---- */
            if (strcmp(fn,"copy_term")==0&&arity==2) {
                Term *orig = pl_unified_term_from_expr(goal->children[0],env);
                Term *dest = pl_unified_term_from_expr(goal->children[1],env);
                Term *copy = pl_copy_term(orig);
                int mark = trail_mark(trail);
                if (!unify(dest, copy, trail)) { trail_unwind(trail,mark); return 0; }
                return 1;
            }
            /* ---- atomic_list_concat/2 and /3 (PL-10) ---- */
            if ((strcmp(fn,"atomic_list_concat")==0||strcmp(fn,"concat_atom")==0)&&(arity==2||arity==3)) {
                Term *lst = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                int nil_id = prolog_atom_intern("[]"), dot_id = prolog_atom_intern(".");
                const char *sep = "";
                char sepbuf[64]; sepbuf[0]='\0';
                if (arity==3) {
                    Term *sv = term_deref(pl_unified_term_from_expr(goal->children[1],env));
                    if (sv && sv->tag==TT_ATOM) sep=prolog_atom_name(sv->atom_id);
                    else if (sv && sv->tag==TT_INT) { snprintf(sepbuf,sizeof sepbuf,"%ld",sv->ival); sep=sepbuf; }
                }
                /* build result string from list */
                char buf[4096]; int pos=0; int first=1;
                for (Term *cur=lst; cur; cur=term_deref(cur->compound.args[1])) {
                    cur = term_deref(cur);
                    if (!cur) break;
                    if (cur->tag==TT_ATOM && cur->atom_id==nil_id) break;
                    if (cur->tag!=TT_COMPOUND || cur->compound.arity!=2) break;
                    Term *hd = term_deref(cur->compound.args[0]);
                    const char *s = NULL; char tmp[64];
                    if (hd && hd->tag==TT_ATOM) s=prolog_atom_name(hd->atom_id);
                    else if (hd && hd->tag==TT_INT) { snprintf(tmp,sizeof tmp,"%ld",hd->ival); s=tmp; }
                    else if (hd && hd->tag==TT_FLOAT) { snprintf(tmp,sizeof tmp,"%g",hd->fval); s=tmp; }
                    if (!s) s="";
                    if (!first && sep[0]) { int sl=(int)strlen(sep); if(pos+sl<(int)sizeof buf-1){memcpy(buf+pos,sep,sl);pos+=sl;} }
                    first=0;
                    int sl=(int)strlen(s); if(pos+sl<(int)sizeof buf-1){memcpy(buf+pos,s,sl);pos+=sl;}
                }
                buf[pos]='\0';
                Term *res = term_new_atom(prolog_atom_intern(buf));
                Term *out = pl_unified_term_from_expr(goal->children[arity==3?2:1],env);
                int mark = trail_mark(trail);
                if (!unify(out,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* ---- string_to_atom/2 (PL-10) ---- */
            if (strcmp(fn,"string_to_atom")==0&&arity==2) {
                Term *a0 = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *a1 = pl_unified_term_from_expr(goal->children[1],env);
                /* mode +,? : convert string/atom arg0 to atom arg1 */
                if (a0 && (a0->tag==TT_ATOM||a0->tag==TT_INT||a0->tag==TT_FLOAT)) {
                    const char *s=NULL; char tmp[64];
                    if(a0->tag==TT_ATOM) s=prolog_atom_name(a0->atom_id);
                    else if(a0->tag==TT_INT){snprintf(tmp,sizeof tmp,"%ld",a0->ival);s=tmp;}
                    else{snprintf(tmp,sizeof tmp,"%g",a0->fval);s=tmp;}
                    Term *res=term_new_atom(prolog_atom_intern(s));
                    int mark=trail_mark(trail);
                    if(!unify(a1,res,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                /* mode ?,+ : arg1 is the atom, unify with arg0 */
                Term *a1d = term_deref(a1);
                if (a1d && a1d->tag==TT_ATOM) {
                    int mark=trail_mark(trail);
                    if(!unify(a0,a1d,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            /* ---- nb_setval/2, nb_getval/2 (PL-10) ---- */
            if (strcmp(fn,"nb_setval")==0&&arity==2) {
                Term *key = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *val = pl_unified_term_from_expr(goal->children[1],env);
                if (!key || key->tag!=TT_ATOM) return 0;
                pl_nb_setval(prolog_atom_name(key->atom_id), val);
                return 1;
            }
            if (strcmp(fn,"nb_getval")==0&&arity==2) {
                Term *key = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                Term *out = pl_unified_term_from_expr(goal->children[1],env);
                if (!key || key->tag!=TT_ATOM) return 0;
                Term *val = pl_nb_getval(prolog_atom_name(key->atom_id));
                if (!val) return 0;
                int mark=trail_mark(trail);
                if(!unify(out,val,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            /* ---- aggregate_all/3 (PL-10) ---- */
            /* aggregate_all(+Template, :Goal, -Result)
             * Template: count | sum(Var) | max(Var) | min(Var)
             * Uses same α/β broker loop as findall. */
            if (strcmp(fn,"aggregate_all")==0&&arity==3) {
                Term   *tmpl_t    = term_deref(pl_unified_term_from_expr(goal->children[0],env));
                AST_t *goal_expr = goal->children[1];
                Term   *result_out= pl_unified_term_from_expr(goal->children[2],env);
                /* classify template */
                int is_count=0, is_sum=0, is_max=0, is_min=0;
                AST_t *val_expr = NULL;
                if (tmpl_t && tmpl_t->tag==TT_ATOM) {
                    const char *tn=prolog_atom_name(tmpl_t->atom_id);
                    if (strcmp(tn,"count")==0) is_count=1;
                } else if (tmpl_t && tmpl_t->tag==TT_COMPOUND && tmpl_t->compound.arity==1) {
                    const char *fn2=prolog_atom_name(tmpl_t->compound.functor);
                    /* val_expr is the IR child of sum(Expr)/max(Expr)/min(Expr) */
                    if (goal->children[0] && goal->children[0]->nchildren>0)
                        val_expr = goal->children[0]->children[0];
                    if (strcmp(fn2,"sum")==0) is_sum=1;
                    else if (strcmp(fn2,"max")==0) is_max=1;
                    else if (strcmp(fn2,"min")==0) is_min=1;
                }
                /* Use findall machinery: collect all snapshots, then aggregate.
                 * This correctly handles anonymous _ vars in the goal. */
                Trail ag_trail; trail_init(&ag_trail);
                Trail saved_global_trail = g_pl_trail;
                g_pl_trail = ag_trail;
                long ag_count=0, ag_sum=0, ag_best=0; int ag_best_set=0;
                /* Use goal children[0] (the template IR expr) as snapshot template.
                 * For count, snapshot a dummy atom; for sum/max/min, snapshot val_expr. */
                AST_t *snap_expr = (is_count || !val_expr) ? NULL : val_expr;
                bb_node_t goal_box = pl_box_goal_from_ir(goal_expr, env);
                DESCR_t ag_r = goal_box.fn(goal_box.ζ, α);
                while (!IS_FAIL_fn(ag_r)) {
                    ag_count++;
                    if ((is_sum||is_max||is_min) && snap_expr) {
                        Term *vt = term_deref(pl_unified_term_from_expr(snap_expr, env));
                        long v=0;
                        if (vt && vt->tag==TT_INT)   v=vt->ival;
                        else if (vt && vt->tag==TT_FLOAT) v=(long)vt->fval;
                        if (is_sum) ag_sum += v;
                        if (!ag_best_set || (is_max && v>ag_best) || (is_min && v<ag_best)) {
                            ag_best=v; ag_best_set=1;
                        }
                    }
                    ag_r = goal_box.fn(goal_box.ζ, β);
                }
                g_pl_trail = saved_global_trail;
                /* build result term */
                Term *res = NULL;
                if (is_count) res = term_new_int(ag_count);
                else if (is_sum) res = term_new_int(ag_sum);
                else if ((is_max||is_min) && ag_best_set) res = term_new_int(ag_best);
                else if (is_max||is_min) return 0; /* no solutions */
                else res = term_new_int(ag_count); /* fallback */
                int mark = trail_mark(trail);
                if (!unify(result_out, res, trail)) { trail_unwind(trail,mark); return 0; }
                return 1;
            }
            /* ---- throw/1 (PL-10) ---- */
            if (strcmp(fn,"throw")==0&&arity==1) {
                Term *exc = pl_unified_term_from_expr(goal->children[0],env);
                g_pl_exception = exc;
                /* unwind to innermost catch frame that matches */
                while (g_pl_catch_top > 0) {
                    int ci = g_pl_catch_top - 1;
                    Pl_CatchFrame *cf = &g_pl_catch_stack[ci];
                    /* try to unify catcher pattern with exception */
                    Trail tmptrail; trail_init(&tmptrail);
                    int tmmark = trail_mark(&tmptrail);
                    int matched = unify(cf->catcher, exc, &tmptrail);
                    trail_unwind(&tmptrail, tmmark);
                    if (matched) {
                        longjmp(cf->jb, 1);
                    }
                    g_pl_catch_top--;
                }
                /* uncaught exception — print and exit */
                fprintf(stderr,"ERROR: Unhandled exception: ");
                pl_write(exc); fprintf(stderr,"\n");
                exit(1);
            }
            /* ---- catch/3 (PL-10) ---- */
            if (strcmp(fn,"catch")==0&&arity==3) {
                AST_t *goal_e   = goal->children[0];
                Term   *catcher  = pl_unified_term_from_expr(goal->children[1],env);
                AST_t *recovery = goal->children[2];
                if (g_pl_catch_top >= PL_CATCH_STACK_MAX) return 0;
                Pl_CatchFrame *cf = &g_pl_catch_stack[g_pl_catch_top];
                cf->catcher    = catcher;
                cf->env        = env;
                cf->trail_mark = trail_mark(trail);
                g_pl_catch_top++;
                int threw = setjmp(cf->jb);
                if (!threw) {
                    /* run the goal */
                    int ok;
                    /* PL-12 fix #2 v3: AST_VAR-shaped goal (catch's first arg is a
                     * runtime variable bound to a callable Term, e.g. plunit
                     * pj_do_succeed(...) calling catch(Goal,_,...)). Without
                     * this branch the else-arm dispatched
                     * interp_exec_pl_builtin(AST_VAR, env) which fell through to
                     * `default: return 1;` — the silent-success defect that
                     * gates ~3-4 plunit suites. Bridge derefs the env-resolved
                     * Term and dispatches via Term->synth EXPR with tenv slots
                     * holding the original Term*s (preserving binding chains
                     * back to caller env). */
                    if (goal_e && goal_e->kind == AST_VAR) {
                        Term *gt = pl_unified_term_from_expr(goal_e, env);
                        ok = pl_invoke_var_goal(gt, env);
                    } else if (is_pl_user_call(goal_e)) {
                        char ukey[256];
                        snprintf(ukey,sizeof ukey,"%s/%d",
                                 goal_e->sval?goal_e->sval:"",goal_e->nchildren);
                        AST_t *uch=pl_pred_table_lookup(&g_pl_pred_table,ukey);
                        if (uch && uch->nchildren > 0) {
                            int ua=goal_e->nchildren;
                            Term **uenv=ua?pl_env_new(ua):NULL;
                            for(int ui=0;ui<ua;ui++)
                                uenv[ui]=pl_unified_term_from_expr(goal_e->children[ui],env);
                            Term **sv=g_pl_env; g_pl_env=uenv;
                            DESCR_t rd=bb_eval_value(uch); g_pl_env=sv;
                            if(uenv)free(uenv);
                            ok=!IS_FAIL_fn(rd);
                        } else {
                            /* undefined predicate (no table entry or zero clauses)
                             * — throw existence_error(procedure, Name/Arity) */
                            ok = pl_throw_existence_error_procedure(
                                goal_e->sval ? goal_e->sval : "", goal_e->nchildren);
                        }
                    } else {
                        ok = interp_exec_pl_builtin(goal_e, env);
                    }
                    /* goal completed without throw — pop catch frame */
                    if (g_pl_catch_top>0 && &g_pl_catch_stack[g_pl_catch_top-1]==cf)
                        g_pl_catch_top--;
                    return ok;
                } else {
                    /* exception was thrown and matched this frame */
                    /* g_pl_catch_top already decremented by throw */
                    trail_unwind(trail, cf->trail_mark);
                    /* unify exception with catcher var in current env */
                    Term *exc = g_pl_exception;
                    g_pl_exception = NULL;
                    /* bind catcher pattern */
                    int mark2=trail_mark(trail);
                    unify(catcher, exc, trail);
                    /* run recovery goal */
                    int rok = interp_exec_pl_builtin(recovery, env);
                    return rok;
                }
            }
            /* ---- setup_call_cleanup/3 (PR-19e) ---- */
            /* setup_call_cleanup(+Setup, :Goal, +Cleanup)
             * Setup runs once. Goal runs (BB_ONCE here). Cleanup runs once
             * after Goal finishes (success or failure) or throws.
             * AST_VAR bridge applied to all three positions. */
            if (strcmp(fn,"setup_call_cleanup")==0&&arity==3) {
                AST_t *setup_e    = goal->children[0];
                AST_t *scc_goal_e = goal->children[1];
                AST_t *cleanup_e  = goal->children[2];
                /* Resolve AST_VAR arms to synth EXPRs */
                AST_t *s_synth=NULL; Term **s_tenv=NULL;
                AST_t *g_synth=NULL; Term **g_tenv=NULL;
                AST_t *c_synth=NULL; Term **c_tenv=NULL;
                if (setup_e && setup_e->kind==AST_VAR) {
                    Term *gt=term_deref(pl_unified_term_from_expr(setup_e,env));
                    s_tenv=calloc(PL_SYNTH_TENV_MAX,sizeof(Term*)); int n=0;
                    s_synth=pl_term_to_synth_expr(gt,s_tenv,&n); setup_e=s_synth;
                }
                if (scc_goal_e && scc_goal_e->kind==AST_VAR) {
                    Term *gt=term_deref(pl_unified_term_from_expr(scc_goal_e,env));
                    g_tenv=calloc(PL_SYNTH_TENV_MAX,sizeof(Term*)); int n=0;
                    g_synth=pl_term_to_synth_expr(gt,g_tenv,&n); scc_goal_e=g_synth;
                }
                if (cleanup_e && cleanup_e->kind==AST_VAR) {
                    Term *gt=term_deref(pl_unified_term_from_expr(cleanup_e,env));
                    c_tenv=calloc(PL_SYNTH_TENV_MAX,sizeof(Term*)); int n=0;
                    c_synth=pl_term_to_synth_expr(gt,c_tenv,&n); cleanup_e=c_synth;
                }
                /* Run Setup */
                int sok = interp_exec_pl_builtin(setup_e, s_synth ? s_tenv : env);
                if (!sok) {
                    if (s_synth){pl_synth_free(s_synth);free(s_tenv);}
                    if (g_synth){pl_synth_free(g_synth);free(g_tenv);}
                    if (c_synth){pl_synth_free(c_synth);free(c_tenv);}
                    return 0;
                }
                /* Run Goal */
                int gok = interp_exec_pl_builtin(scc_goal_e, g_synth ? g_tenv : env);
                /* Run Cleanup unconditionally */
                interp_exec_pl_builtin(cleanup_e, c_synth ? c_tenv : env);
                if (s_synth){pl_synth_free(s_synth);free(s_tenv);}
                if (g_synth){pl_synth_free(g_synth);free(g_tenv);}
                if (c_synth){pl_synth_free(c_synth);free(c_tenv);}
                return gok;
            }
            /* Unknown functor — treat as no-op (directive or future builtin). */
            return 1;
        }
        default: return 1;
    }
}

