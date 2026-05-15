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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern tree_t *pl_assert_term(Term *t, int *functor_out, int *arity_out);
#include "../../frontend/prolog/pl_broker.h"
#include "bb_broker.h"
#include "icn_value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
Pl_PredTable  g_pl_pred_table;
Trail         g_pl_trail;
int           g_pl_cut_flag = 0;
Term        **g_pl_env      = NULL;
int           g_pl_active   = 0;
#define PL_NB_STORE_SIZE 64
typedef struct { char *key; Term *val; } Pl_NbEntry;
static Pl_NbEntry g_pl_nb_store[PL_NB_STORE_SIZE];
static int        g_pl_nb_count = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *pl_nb_getval(const char *key) {
    for (int i = 0; i < g_pl_nb_count; i++)
        if (strcmp(g_pl_nb_store[i].key, key) == 0)
            return g_pl_nb_store[i].val;
    return NULL;
}
#define PL_CATCH_STACK_MAX 64
typedef struct {
    jmp_buf  jb;
    Term    *catcher;
    Term   **env;
    int      trail_mark;
} Pl_CatchFrame;
static Pl_CatchFrame g_pl_catch_stack[PL_CATCH_STACK_MAX];
static int           g_pl_catch_top  = 0;
static Term         *g_pl_exception  = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_throw_iso_error(Term *err_term) {
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_throw_instantiation_error(void) {
    Term *e = term_new_atom(prolog_atom_intern("instantiation_error"));
    return pl_throw_iso_error(e);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_throw_type_error_evaluable(const char *name, int arity) {
    Term *na_args[2]; na_args[0] = term_new_atom(prolog_atom_intern(name)); na_args[1] = term_new_int(arity);
    Term *na = term_new_compound(prolog_atom_intern("/"), 2, na_args);
    Term *te_args[2]; te_args[0] = term_new_atom(prolog_atom_intern("evaluable")); te_args[1] = na;
    Term *te = term_new_compound(prolog_atom_intern("type_error"), 2, te_args);
    return pl_throw_iso_error(te);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int pl_throw_existence_error_procedure(const char *name, int arity) {
    Term *na_args[2]; na_args[0] = term_new_atom(prolog_atom_intern(name)); na_args[1] = term_new_int(arity);
    Term *na = term_new_compound(prolog_atom_intern("/"), 2, na_args);
    Term *te_args[2]; te_args[0] = term_new_atom(prolog_atom_intern("procedure")); te_args[1] = na;
    Term *te = term_new_compound(prolog_atom_intern("existence_error"), 2, te_args);
    return pl_throw_iso_error(te);
}
#define PL_PRED_TABLE_SIZE PL_PRED_TABLE_SIZE_FWD
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
unsigned pl_pred_hash(const char *s) {
    unsigned h = 5381;
    while (*s) h = h * 33 ^ (unsigned char)*s++;
    return h % PL_PRED_TABLE_SIZE;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void pl_pred_table_insert(Pl_PredTable *pt, const char *key, tree_t *choice) {
    unsigned h = pl_pred_hash(key);
    Pl_PredEntry *e = malloc(sizeof(Pl_PredEntry));
    e->key = key; e->choice = choice; e->entry_pc = -1; e->next = pt->buckets[h]; pt->buckets[h] = e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *pl_pred_table_lookup(Pl_PredTable *pt, const char *key) {
    for (Pl_PredEntry *e = pt->buckets[pl_pred_hash(key)]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e->choice;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *pl_pred_table_lookup_global(const char *key) {
    return pl_pred_table_lookup(&g_pl_pred_table, key);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Pl_PredEntry *pl_pred_entry_lookup(const char *key) {
    for (Pl_PredEntry *e = g_pl_pred_table.buckets[pl_pred_hash(key)]; e; e = e->next)
        if (e->key && strcmp(e->key, key) == 0) return e;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *pl_pred_table_get_or_create_choice(const char *key) {
    tree_t *ch = pl_pred_table_lookup(&g_pl_pred_table, key);
    if (ch) return ch;
    ch = ast_node_new(TT_CHOICE);
    ch->v.sval = strdup(key);
    pl_pred_table_insert(&g_pl_pred_table, ch->v.sval, ch);
    return ch;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_assert_clause(Term *t, int end) {
    int functor_id = -1, arity = 0;
    tree_t *ec = pl_assert_term(t, &functor_id, &arity);
    if (!ec) return 0;
    const char *fname = prolog_atom_name(functor_id);
    if (!fname) return 0;
    char key[256];
    snprintf(key, sizeof key, "%s/%d", fname, arity);
    tree_t *ch = pl_pred_table_get_or_create_choice(key);
    if (end) {
        expr_add_child(ch, ec);
    } else {
        expr_add_child(ch, ec);
        if (ch->n > 1) {
            memmove(&ch->c[1], &ch->c[0], (ch->n - 1) * sizeof(tree_t *));
            ch->c[0] = ec;
        }
    }
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_retract_clause(Term *t) {
    if (!t) return 0;
    t = term_deref(t);
    Term *head = t;
    if (t->tag == TERM_COMPOUND && t->compound.arity == 2) {
        const char *fn = prolog_atom_name(t->compound.functor);
        if (fn && strcmp(fn, ":-") == 0) head = term_deref(t->compound.args[0]);
    }
    const char *fname = NULL;
    int arity = 0;
    if (head->tag == TERM_ATOM) {
        fname = prolog_atom_name(head->atom_id); arity = 0;
    } else if (head->tag == TERM_COMPOUND) {
        fname = prolog_atom_name(head->compound.functor); arity = head->compound.arity;
    }
    if (!fname) return 0;
    char key[256]; snprintf(key, sizeof key, "%s/%d", fname, arity);
    tree_t *ch = pl_pred_table_lookup(&g_pl_pred_table, key);
    if (!ch || ch->n == 0) return 0;
    free(ch->c[0]);
    memmove(&ch->c[0], &ch->c[1], (ch->n - 1) * sizeof(tree_t *));
    ch->n--;
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_abolish_pred(Term *t) {
    if (!t) return 0;
    t = term_deref(t);
    const char *fname = NULL; int arity = 0;
    if (t->tag == TERM_COMPOUND && t->compound.arity == 2) {
        const char *fn = prolog_atom_name(t->compound.functor);
        if (fn && strcmp(fn, "/") == 0) {
            Term *na = term_deref(t->compound.args[0]);
            Term *ar = term_deref(t->compound.args[1]);
            if (na && na->tag == TERM_ATOM) fname = prolog_atom_name(na->atom_id);
            if (ar && ar->tag == TERM_INT)  arity = (int)ar->ival;
        }
    } else if (t->tag == TERM_ATOM) {
        fname = prolog_atom_name(t->atom_id); arity = 0;
    }
    if (!fname) return 0;
    char key[256]; snprintf(key, sizeof key, "%s/%d", fname, arity);
    tree_t *ch = pl_pred_table_lookup(&g_pl_pred_table, key);
    if (!ch) return 1;
    ch->n = 0;
    return 1;
}
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term **pl_env_new(int n) {
    if (n <= 0) return NULL;
    Term **env = malloc(n * sizeof(Term *));
    for (int i = 0; i < n; i++) env[i] = term_new_var(i);
    return env;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term *pl_unified_term_from_expr(tree_t *e, Term **env);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *pl_unified_deep_copy(Term *t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int          interp_exec_pl_builtin(tree_t *goal, Term **env);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term *pl_unified_term_from_expr(tree_t *e, Term **env) {
    if (!e) return term_new_atom(prolog_atom_intern("[]"));
    switch (e->t) {
        case TT_QLIT: return term_new_atom(prolog_atom_intern(e->v.sval ? e->v.sval : ""));
        case TT_ILIT: return term_new_int((long)e->v.ival);
        case TT_FLIT: return term_new_float(e->v.dval);
        case TT_VAR:  return (env && e->v.ival >= 0) ? env[e->v.ival] : term_new_var(e->v.ival);
        case TT_ADD: case TT_SUB: case TT_MUL: case TT_DIV: case TT_MOD: {
            const char *op = e->t==TT_ADD?"+":e->t==TT_SUB?"-":e->t==TT_MUL?"*":e->t==TT_DIV?"/":"%";
            int atom = prolog_atom_intern(op);
            Term *args2[2]; args2[0]=pl_unified_term_from_expr(e->c[0],env); args2[1]=pl_unified_term_from_expr(e->c[1],env);
            return term_new_compound(atom, 2, args2);
        }
        case TT_UNIFY: {
            int atom = prolog_atom_intern("=");
            Term *args2[2];
            args2[0] = e->n > 0 ? pl_unified_term_from_expr(e->c[0], env) : term_new_atom(atom);
            args2[1] = e->n > 1 ? pl_unified_term_from_expr(e->c[1], env) : term_new_atom(atom);
            return term_new_compound(atom, 2, args2);
        }
        case TT_CUT:  return term_new_atom(prolog_atom_intern("!"));
        case TT_NUL:  return term_new_atom(prolog_atom_intern("[]"));
        case TT_FNC: {
            int arity = e->n;
            int atom  = prolog_atom_intern(e->v.sval ? e->v.sval : "f");
            if (arity == 0) return term_new_atom(atom);
            Term **args = malloc(arity * sizeof(Term *));
            for (int i = 0; i < arity; i++) args[i] = pl_unified_term_from_expr(e->c[i], env);
            Term *t = term_new_compound(atom, arity, args);
            free(args);
            return t;
        }
        default: return term_new_atom(prolog_atom_intern("?"));
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *pl_unified_deep_copy(Term *t) {
    t = term_deref(t);
    if (!t || t->tag == TERM_VAR) return term_new_atom(prolog_atom_intern("_"));
    if (t->tag == TERM_ATOM)  return term_new_atom(t->atom_id);
    if (t->tag == TERM_INT)   return term_new_int(t->ival);
    if (t->tag == TERM_FLOAT) return term_new_float(t->fval);
    if (t->tag == TERM_COMPOUND) {
        Term **args = malloc(t->compound.arity * sizeof(Term *));
        for (int i = 0; i < t->compound.arity; i++) args[i] = pl_unified_deep_copy(t->compound.args[i]);
        Term *r = term_new_compound(t->compound.functor, t->compound.arity, args);
        free(args);
        return r;
    }
    return term_new_atom(prolog_atom_intern("_"));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline long pl_iso_mod(long n, long d) {
    if (!d) return 0;
    if (n == LONG_MIN && d == -1) return 0;
    long r = n % d;
    if (r != 0 && (r < 0) != (d < 0)) r += d;
    return r;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *pl_unified_eval_arith_term(tree_t *e, Term **env) {
    if (!e) return term_new_int(0);
#define _EI(x) ({ Term *_t = pl_unified_eval_arith_term(x,env); (_t&&_t->tag==TERM_INT)?_t->ival:(_t&&_t->tag==TERM_FLOAT)?(long)_t->fval:0L; })
#define _ED(x) ({ Term *_t = pl_unified_eval_arith_term(x,env); (_t&&_t->tag==TERM_FLOAT)?_t->fval:(_t&&_t->tag==TERM_INT)?(double)_t->ival:0.0; })
#define _EIS(x) (pl_unified_eval_arith_term(x,env)->tag==TERM_FLOAT)
#define _EF(op,a,b) ({ Term *_la=pl_unified_eval_arith_term(a,env),*_lb=pl_unified_eval_arith_term(b,env); \
                       int _fl=(_la&&_la->tag==TERM_FLOAT)||(_lb&&_lb->tag==TERM_FLOAT); \
                       _fl?term_new_float(_ED(a) op _ED(b)):term_new_int(_EI(a) op _EI(b)); })
    switch (e->t) {
        case TT_ILIT: return term_new_int((long)e->v.ival);
        case TT_FLIT: return term_new_float(e->v.dval);
        case TT_VAR: {
            Term *t = term_deref(env && e->v.ival >= 0 ? env[e->v.ival] : NULL);
            if (!t || t->tag == TERM_VAR) { pl_throw_instantiation_error(); return NULL; }
            return t;
        }
        case TT_ADD: {
            Term *la=pl_unified_eval_arith_term(e->c[0],env);
            Term *lb=pl_unified_eval_arith_term(e->c[1],env);
            int fl=(la&&la->tag==TERM_FLOAT)||(lb&&lb->tag==TERM_FLOAT);
            if (fl) return term_new_float(_ED(e->c[0]) + _ED(e->c[1]));
            long a=_EI(e->c[0]), b=_EI(e->c[1]), r;
            if (__builtin_add_overflow(a, b, &r))
                return term_new_float((double)a + (double)b);
            return term_new_int(r);
        }
        case TT_SUB: {
            Term *la=pl_unified_eval_arith_term(e->c[0],env);
            Term *lb=pl_unified_eval_arith_term(e->c[1],env);
            int fl=(la&&la->tag==TERM_FLOAT)||(lb&&lb->tag==TERM_FLOAT);
            if (fl) return term_new_float(_ED(e->c[0]) - _ED(e->c[1]));
            long a=_EI(e->c[0]), b=_EI(e->c[1]), r;
            if (__builtin_sub_overflow(a, b, &r))
                return term_new_float((double)a - (double)b);
            return term_new_int(r);
        }
        case TT_MUL: {
            Term *la=pl_unified_eval_arith_term(e->c[0],env);
            Term *lb=pl_unified_eval_arith_term(e->c[1],env);
            int fl=(la&&la->tag==TERM_FLOAT)||(lb&&lb->tag==TERM_FLOAT);
            if (fl) return term_new_float(_ED(e->c[0]) * _ED(e->c[1]));
            long a=_EI(e->c[0]), b=_EI(e->c[1]), r;
            if (__builtin_mul_overflow(a, b, &r))
                return term_new_float((double)a * (double)b);
            return term_new_int(r);
        }
        case TT_DIV: {
            Term *la=pl_unified_eval_arith_term(e->c[0],env);
            Term *lb=pl_unified_eval_arith_term(e->c[1],env);
            int fl=(la&&la->tag==TERM_FLOAT)||(lb&&lb->tag==TERM_FLOAT);
            if (fl) { double d=_ED(e->c[1]); return term_new_float(d?_ED(e->c[0])/d:0.0); }
            else    {
                long  n=_EI(e->c[0]);
                long  d=_EI(e->c[1]);
                if (!d) return term_new_int(0);
                if (n == LONG_MIN && d == -1) return term_new_int(LONG_MIN);
                return term_new_int(n/d);
            }
        }
        case TT_MOD: return term_new_int(pl_iso_mod(_EI(e->c[0]), _EI(e->c[1])));
        case TT_FNC: {
            const char *fn = e->v.sval ? e->v.sval : "";
            if (strcmp(fn,"/\\")==0&&e->n==2) return term_new_int(_EI(e->c[0])&_EI(e->c[1]));
            if (strcmp(fn,"\\/")==0&&e->n==2) return term_new_int(_EI(e->c[0])|_EI(e->c[1]));
            if (strcmp(fn,"xor")==0&&e->n==2) return term_new_int(_EI(e->c[0])^_EI(e->c[1]));
            if (strcmp(fn,"<<")==0&&e->n==2)  return term_new_int(_EI(e->c[0])<<_EI(e->c[1]));
            if (strcmp(fn,">>")==0&&e->n==2)  return term_new_int(_EI(e->c[0])>>_EI(e->c[1]));
            if (strcmp(fn,"\\")==0&&e->n==1)  return term_new_int(~_EI(e->c[0]));
            if (strcmp(fn,"mod")==0&&e->n==2) return term_new_int(pl_iso_mod(_EI(e->c[0]),_EI(e->c[1])));
            if (strcmp(fn,"rem")==0&&e->n==2) {
                long n=_EI(e->c[0]); long d=_EI(e->c[1]);
                if (!d) return term_new_int(0);
                if (n == LONG_MIN && d == -1) return term_new_int(0);
                return term_new_int(n%d);
            }
            if (strcmp(fn,"**")==0&&e->n==2) return term_new_float(pow(_ED(e->c[0]),_ED(e->c[1])));
            if (strcmp(fn,"^")==0&&e->n==2) {
                Term *la=pl_unified_eval_arith_term(e->c[0],env);
                Term *lb=pl_unified_eval_arith_term(e->c[1],env);
                if (la&&lb&&la->tag==TERM_INT&&lb->tag==TERM_INT&&lb->ival>=0) {
                    long base=la->ival, exp=lb->ival, acc=1;
                    while (exp-- > 0) acc *= base;
                    return term_new_int(acc);
                }
                return term_new_float(pow(_ED(e->c[0]),_ED(e->c[1])));
            }
            if (strcmp(fn,"max")==0&&e->n==2) { Term *la=pl_unified_eval_arith_term(e->c[0],env),*lb=pl_unified_eval_arith_term(e->c[1],env); int fl=(la&&la->tag==TERM_FLOAT)||(lb&&lb->tag==TERM_FLOAT); return fl?(_ED(e->c[0])>=_ED(e->c[1])?la:lb):(_EI(e->c[0])>=_EI(e->c[1])?la:lb); }
            if (strcmp(fn,"min")==0&&e->n==2) { Term *la=pl_unified_eval_arith_term(e->c[0],env),*lb=pl_unified_eval_arith_term(e->c[1],env); int fl=(la&&la->tag==TERM_FLOAT)||(lb&&lb->tag==TERM_FLOAT); return fl?(_ED(e->c[0])<=_ED(e->c[1])?la:lb):(_EI(e->c[0])<=_EI(e->c[1])?la:lb); }
            if (strcmp(fn,"gcd")==0&&e->n==2) { long a=labs(_EI(e->c[0])),b=labs(_EI(e->c[1])); while(b){long r=a%b;a=b;b=r;} return term_new_int(a); }
            if (e->n==1) {
                double d = _ED(e->c[0]);
                long   i = _EI(e->c[0]);
                int    isf = _EIS(e->c[0]);
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
            if (strcmp(fn,"pi")==0&&e->n==0) return term_new_float(M_PI);
            if (strcmp(fn,"e")==0&&e->n==0)  return term_new_float(M_E);
            Term *t=term_deref(pl_unified_term_from_expr(e,env));
            if (!t || t->tag == TERM_VAR) { pl_throw_instantiation_error(); return NULL; }
            if (t->tag == TERM_INT || t->tag == TERM_FLOAT) return t;
            pl_throw_type_error_evaluable(fn ? fn : "", e->n);
            return NULL;
        }
        default: return term_new_int(0);
    }
#undef _EI
#undef _ED
#undef _EIS
#undef _EF
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static long pl_unified_eval_arith(tree_t *e, Term **env) {
    Term *t = pl_unified_eval_arith_term(e, env);
    if (!t) return 0;
    if (t->tag == TERM_FLOAT) return (long)t->fval;
    if (t->tag == TERM_INT)   return t->ival;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int is_pl_user_call(tree_t *goal) {
    if (!goal || goal->t != TT_FNC || !goal->v.sval) return 0;
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
    for (int i = 0; builtins[i]; i++) if (strcmp(goal->v.sval, builtins[i]) == 0) return 0;
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int term_order_cmp(Term *a, Term *b) {
    a = term_deref(a); b = term_deref(b);
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return  1;
    int ra = (a->tag==TERM_VAR)?0:(a->tag==TERM_INT||a->tag==TERM_FLOAT)?1:(a->tag==TERM_ATOM)?2:3;
    int rb = (b->tag==TERM_VAR)?0:(b->tag==TERM_INT||b->tag==TERM_FLOAT)?1:(b->tag==TERM_ATOM)?2:3;
    if (ra != rb) return ra - rb;
    switch (a->tag) {
        case TT_VAR:   return (int)(a - b);
        case TERM_INT:   return (a->ival < b->ival) ? -1 : (a->ival > b->ival) ? 1 : 0;
        case TERM_FLOAT: return (a->fval < b->fval) ? -1 : (a->fval > b->fval) ? 1 : 0;
        case TERM_ATOM: {
            const char *sa = prolog_atom_name(a->atom_id);
            const char *sb = prolog_atom_name(b->atom_id);
            return strcmp(sa ? sa : "", sb ? sb : "");
        }
        case TERM_COMPOUND: {
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
#define COPY_TERM_MAX_VARS 256
typedef struct { Term *orig; Term *copy; } CopyVarMap;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *copy_term_rec(Term *t, CopyVarMap *map, int *nmap) {
    t = term_deref(t);
    if (!t) {
        Term *fresh = term_new_var((1 << 20) + *nmap);
        return fresh;
    }
    switch (t->tag) {
        case TT_VAR: {
            for (int i = 0; i < *nmap; i++)
                if (map[i].orig == t) return map[i].copy;
            Term *fresh = term_new_var((1 << 20) + *nmap);
            if (*nmap < COPY_TERM_MAX_VARS) {
                map[*nmap].orig = t; map[*nmap].copy = fresh; (*nmap)++;
            }
            return fresh;
        }
        case TERM_ATOM:  return term_new_atom(t->atom_id);
        case TERM_INT:   return term_new_int(t->ival);
        case TERM_FLOAT: return term_new_float(t->fval);
        case TERM_COMPOUND: {
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *pl_copy_term(Term *t) {
    CopyVarMap map[COPY_TERM_MAX_VARS];
    int nmap = 0;
    return copy_term_rec(t, map, &nmap);
}
#define PL_SYNTH_TENV_MAX 64
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *pl_synth_new(tree_e k) {
    tree_t *e = (tree_t *)calloc(1, sizeof(tree_t));
    e->t = k;
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pl_synth_add_child(tree_t *e, tree_t *child) {
    ast_push(e, child);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pl_synth_free(tree_t *e) {
    if (!e) return;
    for (int i = 0; i < e->n; i++) pl_synth_free(e->c[i]);
    free(e->c);
    if (e->v.sval) free(e->v.sval);
    free(e);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_tenv_add_dedup(Term **tenv, int *pn, Term *v) {
    for (int i = 0; i < *pn; i++) if (tenv[i] == v) return i;
    if (*pn >= PL_SYNTH_TENV_MAX) return -1;
    tenv[*pn] = v;
    return (*pn)++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *pl_term_to_synth_expr(Term *t, Term **tenv, int *pn) {
    t = term_deref(t);
    if (!t) {
        tree_t *e = pl_synth_new(TT_FNC);
        e->v.sval = strdup("[]");
        return e;
    }
    switch (t->tag) {
        case TT_VAR: {
            int slot = pl_tenv_add_dedup(tenv, pn, t);
            tree_t *e = pl_synth_new(TERM_VAR);
            e->v.ival = slot >= 0 ? slot : 0;
            return e;
        }
        case TERM_INT: {
            tree_t *e = pl_synth_new(TT_ILIT);
            e->v.ival = t->ival;
            return e;
        }
        case TERM_FLOAT: {
            tree_t *e = pl_synth_new(TT_FLIT);
            e->v.dval = t->fval;
            return e;
        }
        case TERM_ATOM: {
            const char *nm = prolog_atom_name(t->atom_id);
            tree_t *e = pl_synth_new(TT_FNC);
            e->v.sval = strdup(nm ? nm : "");
            return e;
        }
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            int arity = t->compound.arity;
            if (arity == 2 && strcmp(fn, "=") == 0) {
                tree_t *e = pl_synth_new(TT_UNIFY);
                pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[0], tenv, pn));
                pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[1], tenv, pn));
                return e;
            }
            if (arity == 2) {
                tree_e ak = TT_KIND_COUNT;
                if      (!strcmp(fn,"+"))   ak = TT_ADD;
                else if (!strcmp(fn,"-"))   ak = TT_SUB;
                else if (!strcmp(fn,"*"))   ak = TT_MUL;
                else if (!strcmp(fn,"/"))   ak = TT_DIV;
                else if (!strcmp(fn,"mod")) ak = TT_MOD;
                if (ak != TT_KIND_COUNT) {
                    tree_t *e = pl_synth_new(ak);
                    pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[0], tenv, pn));
                    pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[1], tenv, pn));
                    return e;
                }
            }
            tree_t *e = pl_synth_new(TT_FNC);
            e->v.sval = strdup(fn);
            for (int i = 0; i < arity; i++)
                pl_synth_add_child(e, pl_term_to_synth_expr(t->compound.args[i], tenv, pn));
            return e;
        }
    }
    return pl_synth_new(TT_FNC);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_invoke_var_goal(Term *gt, Term **caller_env) {
    (void)caller_env;
    gt = term_deref(gt);
    if (!gt) return 0;
    if (gt->tag == TERM_VAR)   return 0;
    if (gt->tag == TERM_INT)   return 0;
    if (gt->tag == TERM_FLOAT) return 0;
    Term *tenv[PL_SYNTH_TENV_MAX];
    for (int i = 0; i < PL_SYNTH_TENV_MAX; i++) tenv[i] = NULL;
    int    tn = 0;
    tree_t *synth = pl_term_to_synth_expr(gt, tenv, &tn);
    int ok = interp_exec_pl_builtin(synth, tenv);
    pl_synth_free(synth);
    return ok;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int interp_exec_pl_builtin(tree_t *goal, Term **env) {
    if (!goal) return 1;
    Trail *trail = &g_pl_trail;
    int *cut_flag = &g_pl_cut_flag;
    switch (goal->t) {
        case TT_UNIFY: {
            Term *t1=pl_unified_term_from_expr(goal->c[0],env);
            Term *t2=pl_unified_term_from_expr(goal->c[1],env);
            int mark=trail_mark(trail);
            if (!unify(t1,t2,trail)){trail_unwind(trail,mark);return 0;}
            return 1;
        }
        case TT_CUT: if (cut_flag) *cut_flag=1; return 1;
        case TT_TRAIL_MARK: case TT_TRAIL_UNWIND: return 1;
        case TT_FNC: {
            const char *fn = goal->v.sval ? goal->v.sval : "true";
            int arity = goal->n;
            if (is_pl_user_call(goal)) {
                char ukey[256]; snprintf(ukey,sizeof ukey,"%s/%d",fn,arity);
                tree_t *uch = pl_pred_table_lookup(&g_pl_pred_table, ukey);
                if (uch) {
                    int ua = arity;
                    Term **uenv = ua ? pl_env_new(ua) : NULL;
                    for (int ui = 0; ui < ua; ui++)
                        uenv[ui] = pl_unified_term_from_expr(goal->c[ui], env);
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
            if (strcmp(fn,"halt")==0&&arity==1){Term *t=term_deref(pl_unified_term_from_expr(goal->c[0],env));exit(t&&t->tag==TERM_INT?(int)t->ival:0);}
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
            if (strcmp(fn,"include")==0) return 1;
            if (strcmp(fn,"nl")==0&&arity==0){putchar('\n');return 1;}
            if (strcmp(fn,"write")==0&&arity==1){pl_write(pl_unified_term_from_expr(goal->c[0],env));return 1;}
            if (strcmp(fn,"writeln")==0&&arity==1){pl_write(pl_unified_term_from_expr(goal->c[0],env));putchar('\n');return 1;}
            if (strcmp(fn,"print")==0&&arity==1){pl_write(pl_unified_term_from_expr(goal->c[0],env));return 1;}
            if (strcmp(fn,"writeq")==0&&arity==1){pl_writeq(pl_unified_term_from_expr(goal->c[0],env));return 1;}
            if (strcmp(fn,"write_canonical")==0&&arity==1){pl_write_canonical(pl_unified_term_from_expr(goal->c[0],env));return 1;}
            if (strcmp(fn,"tab")==0&&arity==1){
                Term *t=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                long n=(t&&t->tag==TERM_INT)?t->ival:0;
                for(long i=0;i<n;i++) putchar(' ');
                return 1;
            }
            if (strcmp(fn,"is")==0&&arity==2){
                Term *val=pl_unified_eval_arith_term(goal->c[1],env);
                Term *lhs=pl_unified_term_from_expr(goal->c[0],env);
                int mark=trail_mark(trail);
                if(!unify(lhs,val,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            { struct{const char *n;int op;}cmps[]={{"<",0},{">",1},{"=<",2},{">=",3},{"=:=",4},{"=\\=",5},{NULL,0}};
              for(int ci=0;cmps[ci].n;ci++) if(strcmp(fn,cmps[ci].n)==0&&arity==2){
                  long a=pl_unified_eval_arith(goal->c[0],env),b=pl_unified_eval_arith(goal->c[1],env);
                  switch(cmps[ci].op){case 0:return a<b;case 1:return a>b;case 2:return a<=b;case 3:return a>=b;case 4:return a==b;case 5:return a!=b;}
              }
            }
            if (strcmp(fn,"=")==0&&arity==2){
                int mark=trail_mark(trail);
                if(!unify(pl_unified_term_from_expr(goal->c[0],env),pl_unified_term_from_expr(goal->c[1],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"\\=")==0&&arity==2){
                int mark=trail_mark(trail);
                int ok=unify(pl_unified_term_from_expr(goal->c[0],env),pl_unified_term_from_expr(goal->c[1],env),trail);
                trail_unwind(trail,mark);return !ok;
            }
            if (strcmp(fn,"==")==0&&arity==2){
                Term *t1=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *t2=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                if(!t1||!t2)return t1==t2;
                if(t1->tag!=t2->tag)return 0;
                if(t1->tag==TERM_ATOM)return t1->atom_id==t2->atom_id;
                if(t1->tag==TERM_INT) return t1->ival==t2->ival;
                if(t1->tag==TERM_VAR) return t1==t2;
                return 0;
            }
            if (strcmp(fn,"\\==")==0&&arity==2){
                Term *t1=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *t2=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                if(!t1||!t2)return t1!=t2;
                if(t1->tag!=t2->tag)return 1;
                if(t1->tag==TERM_ATOM)return t1->atom_id!=t2->atom_id;
                if(t1->tag==TERM_INT) return t1->ival!=t2->ival;
                if(t1->tag==TERM_VAR) return t1!=t2;
                return 1;
            }
            if (arity==1){
                Term *t=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                if(strcmp(fn,"var"     )==0)return !t||t->tag==TERM_VAR;
                if(strcmp(fn,"nonvar"  )==0)return  t&&t->tag!=TERM_VAR;
                if(strcmp(fn,"atom"    )==0)return  t&&t->tag==TERM_ATOM;
                if(strcmp(fn,"integer" )==0)return  t&&t->tag==TERM_INT;
                if(strcmp(fn,"float"   )==0)return  t&&t->tag==TERM_FLOAT;
                if(strcmp(fn,"compound")==0)return  t&&t->tag==TERM_COMPOUND;
                if(strcmp(fn,"atomic"  )==0)return  t&&(t->tag==TERM_ATOM||t->tag==TERM_INT||t->tag==TERM_FLOAT);
                if(strcmp(fn,"callable")==0)return  t&&(t->tag==TERM_ATOM||t->tag==TERM_COMPOUND);
                if(strcmp(fn,"is_list" )==0){
                    int nil=prolog_atom_intern("[]"),dot=prolog_atom_intern(".");
                    for(Term *c=t;;){c=term_deref(c);if(!c)return 0;if(c->tag==TERM_ATOM&&c->atom_id==nil)return 1;if(c->tag!=TERM_COMPOUND||c->compound.arity!=2||c->compound.functor!=dot)return 0;c=c->compound.args[1];}
                }
            }
            if (strcmp(fn,",")==0){
                for(int i=0;i<goal->n;i++){
                    tree_t *g=goal->c[i];
                    if(!g) continue;
                    int ok = is_pl_user_call(g) ? ({
                        char key[256]; snprintf(key,sizeof key,"%s/%d",g->v.sval?g->v.sval:"",g->n);
                        tree_t *ch=pl_pred_table_lookup(&g_pl_pred_table,key);
                        int r=0;
                        if(ch){ int ca=g->n; Term **cargs=ca?malloc(ca*sizeof(Term*)):NULL;
                                 for(int a=0;a<ca;a++) cargs[a]=pl_unified_term_from_expr(g->c[a],env);
                                 Term **sv=g_pl_env; g_pl_env=cargs;
                                 DESCR_t rd=bb_eval_value(ch); g_pl_env=sv; if(cargs)free(cargs);
                                 r=!IS_FAIL_fn(rd); }
                        r; }) : interp_exec_pl_builtin(g, env);
                    if(!ok) return 0;
                }
                return 1;
            }
            if (strcmp(fn,";")==0&&arity>=2){
                tree_t *left=goal->c[0],*right=goal->c[1];
                if(left&&left->t==TT_FNC&&left->v.sval&&strcmp(left->v.sval,"->")==0&&left->n>=2){
                    int mark=trail_mark(trail); int cut2=0;
                    if(interp_exec_pl_builtin(left->c[0],env)){
                        for(int i=1;i<left->n;i++) if(!interp_exec_pl_builtin(left->c[i],env)) return 0;
                        return 1;
                    }
                    trail_unwind(trail,mark);
                    return interp_exec_pl_builtin(right,env);
                }
                {int mark=trail_mark(trail);
                 if(interp_exec_pl_builtin(left,env)) return 1;
                 trail_unwind(trail,mark);
                 return interp_exec_pl_builtin(right,env);}
            }
            if (strcmp(fn,"->")==0&&arity>=2){
                if(!interp_exec_pl_builtin(goal->c[0],env)) return 0;
                for(int i=1;i<goal->n;i++) if(!interp_exec_pl_builtin(goal->c[i],env)) return 0;
                return 1;
            }
            if ((strcmp(fn,"\\+")==0||strcmp(fn,"not")==0)&&arity==1){
                int mark=trail_mark(trail);
                int ok;
                if (goal->c[0] && goal->c[0]->t == TERM_VAR) {
                    Term *gt = pl_unified_term_from_expr(goal->c[0], env);
                    ok = pl_invoke_var_goal(gt, env);
                } else {
                    ok = interp_exec_pl_builtin(goal->c[0],env);
                }
                trail_unwind(trail,mark);return !ok;
            }
            if (strcmp(fn,"once")==0&&arity==1){
                int mark=trail_mark(trail);
                int ok;
                if (goal->c[0] && goal->c[0]->t == TERM_VAR) {
                    Term *gt = pl_unified_term_from_expr(goal->c[0], env);
                    ok = pl_invoke_var_goal(gt, env);
                } else {
                    ok = interp_exec_pl_builtin(goal->c[0],env);
                }
                if(!ok) trail_unwind(trail,mark);
                return ok;
            }
            if (strcmp(fn,"call")==0 && arity>=1) {
                tree_t *g_expr = goal->c[0];
                Term *g_term = pl_unified_term_from_expr(g_expr, env);
                g_term = term_deref(g_term);
                if (!g_term) return 0;
                int n_extra = arity - 1;
                if (n_extra == 0) {
                    return pl_invoke_var_goal(g_term, env);
                }
                const char *cfn = NULL;
                int carity_base = 0;
                Term **cargs_base = NULL;
                if (g_term->tag == TERM_ATOM) {
                    cfn = prolog_atom_name(g_term->atom_id);
                    carity_base = 0; cargs_base = NULL;
                } else if (g_term->tag == TERM_COMPOUND) {
                    cfn = prolog_atom_name(g_term->compound.functor);
                    carity_base = g_term->compound.arity;
                    cargs_base = g_term->compound.args;
                } else {
                    return 0;
                }
                if (!cfn) return 0;
                int total_arity = carity_base + n_extra;
                Term **all_args = (Term **)malloc(total_arity * sizeof(Term *));
                for (int i = 0; i < carity_base; i++)
                    all_args[i] = term_deref(cargs_base[i]);
                for (int i = 0; i < n_extra; i++)
                    all_args[carity_base + i] =
                        pl_unified_term_from_expr(goal->c[1 + i], env);
                int fn_id = prolog_atom_intern(cfn);
                Term **rargs2 = (Term **)malloc(total_arity * sizeof(Term *));
                for (int i = 0; i < total_arity; i++) rargs2[i] = all_args[i];
                free(all_args);
                Term *reconstructed = term_new_compound(fn_id, total_arity, rargs2);
                free(rargs2);
                return pl_invoke_var_goal(reconstructed, env);
            }
            if (strcmp(fn,"functor")==0&&arity==3){
                int mark=trail_mark(trail);
                if(!pl_functor(pl_unified_term_from_expr(goal->c[0],env),pl_unified_term_from_expr(goal->c[1],env),pl_unified_term_from_expr(goal->c[2],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"arg")==0&&arity==3){
                int mark=trail_mark(trail);
                if(!pl_arg(pl_unified_term_from_expr(goal->c[0],env),pl_unified_term_from_expr(goal->c[1],env),pl_unified_term_from_expr(goal->c[2],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"=..")==0&&arity==2){
                int mark=trail_mark(trail);
                if(!pl_univ(pl_unified_term_from_expr(goal->c[0],env),pl_unified_term_from_expr(goal->c[1],env),trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if ((strcmp(fn,"assert")==0||strcmp(fn,"assertz")==0)&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->c[0],env);
                return pl_assert_clause(arg,1);
            }
            if (strcmp(fn,"asserta")==0&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->c[0],env);
                return pl_assert_clause(arg,0);
            }
            if ((strcmp(fn,"retract")==0||strcmp(fn,"retractall")==0)&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->c[0],env);
                return pl_retract_clause(arg);
            }
            if (strcmp(fn,"abolish")==0&&arity==1) {
                Term *arg=pl_unified_term_from_expr(goal->c[0],env);
                return pl_abolish_pred(arg);
            }
            if (strcmp(fn,"atom_length")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *l=pl_unified_term_from_expr(goal->c[1],env);
                const char *s=NULL;
                if (a&&a->tag==TERM_ATOM) s=prolog_atom_name(a->atom_id);
                else if (a&&a->tag==TERM_INT) { char buf[32]; snprintf(buf,sizeof buf,"%ld",a->ival); s=buf; }
                if (!s) return 0;
                Term *len=term_new_int((long)strlen(s));
                int mark=trail_mark(trail);
                if (!unify(l,len,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"atom_concat")==0&&arity==3) {
                Term *a1=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *a2=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                Term *a3=pl_unified_term_from_expr(goal->c[2],env);
                if (a1&&a1->tag==TERM_ATOM&&a2&&a2->tag==TERM_ATOM) {
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
                Term *a3d=term_deref(a3);
                if (a3d&&a3d->tag==TERM_ATOM) {
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
            if (strcmp(fn,"atom_chars")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *cl=pl_unified_term_from_expr(goal->c[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (a&&a->tag==TERM_ATOM) {
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
                    Term *cur=term_deref(cl); char buf[1024]; int pos=0;
                    while(cur&&cur->tag==TERM_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TERM_ATOM) return 0;
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
            if (strcmp(fn,"atom_codes")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *cl=pl_unified_term_from_expr(goal->c[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (a&&a->tag==TERM_ATOM) {
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
                    while(cur&&cur->tag==TERM_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TERM_INT) return 0;
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
            if (arity==2&&(strcmp(fn,"@<")==0||strcmp(fn,"@>")==0||strcmp(fn,"@=<")==0||strcmp(fn,"@>=")==0)) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                int c=term_order_cmp(a,b);
                if (strcmp(fn,"@<")==0)  return c<0;
                if (strcmp(fn,"@>")==0)  return c>0;
                if (strcmp(fn,"@=<")==0) return c<=0;
                if (strcmp(fn,"@>=")==0) return c>=0;
            }
            if (strcmp(fn,"compare")==0&&arity==3) {
                Term *order=pl_unified_term_from_expr(goal->c[0],env);
                Term *a=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->c[2],env));
                int c=term_order_cmp(a,b);
                const char *os=c<0?"<":c>0?">":"=";
                Term *ot=term_new_atom(prolog_atom_intern(os));
                int mark=trail_mark(trail);
                if(!unify(order,ot,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if ((strcmp(fn,"sort")==0||strcmp(fn,"msort")==0)&&arity==2) {
                int do_dedup=(strcmp(fn,"sort")==0);
                Term *lst=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *out=pl_unified_term_from_expr(goal->c[1],env);
                Term *elems[4096]; int n=0;
                Term *cur=lst;
                int nil_id=prolog_atom_intern("[]"),dot_id=prolog_atom_intern(".");
                while(cur&&cur->tag==TERM_COMPOUND&&cur->compound.arity==2) {
                    if(n<4096) elems[n++]=term_deref(cur->compound.args[0]);
                    cur=term_deref(cur->compound.args[1]);
                }
                for(int i=1;i<n;i++){
                    Term *key=elems[i]; int j=i-1;
                    while(j>=0&&term_order_cmp(elems[j],key)>0){elems[j+1]=elems[j];j--;}
                    elems[j+1]=key;
                }
                if(do_dedup&&n>0){
                    int w=0;
                    for(int i=0;i<n;i++)
                        if(i==0||term_order_cmp(elems[i-1],elems[i])!=0) elems[w++]=elems[i];
                    n=w;
                }
                Term *res=term_new_atom(nil_id);
                for(int i=n-1;i>=0;i--){Term *a2[2];a2[0]=elems[i];a2[1]=res;res=term_new_compound(dot_id,2,a2);}
                int mark=trail_mark(trail);
                if(!unify(out,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"succ")==0&&arity==2) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                int mark=trail_mark(trail);
                if (a&&a->tag==TERM_INT) {
                    if (a->ival < 0) return 0;
                    Term *r=term_new_int(a->ival+1);
                    if(!unify(b,r,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else if (b&&b->tag==TERM_INT) {
                    if (b->ival <= 0) return 0;
                    Term *r=term_new_int(b->ival-1);
                    if(!unify(a,r,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            if (strcmp(fn,"plus")==0&&arity==3) {
                Term *a=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *b=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                Term *c=term_deref(pl_unified_term_from_expr(goal->c[2],env));
                int mark=trail_mark(trail);
                int ai=(a&&a->tag==TERM_INT),bi=(b&&b->tag==TERM_INT),ci=(c&&c->tag==TERM_INT);
                Term *r=NULL;
                Term *tgt=NULL;
                if (ai&&bi)        { r=term_new_int(a->ival+b->ival); tgt=c; }
                else if (ai&&ci)   { r=term_new_int(c->ival-a->ival); tgt=b; }
                else if (bi&&ci)   { r=term_new_int(c->ival-b->ival); tgt=a; }
                else return 0;
                if(!unify(tgt,r,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"format")==0&&(arity==1||arity==2)) {
                Term *fmt_t=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                const char *fmt=NULL;
                if (fmt_t&&fmt_t->tag==TERM_ATOM) fmt=prolog_atom_name(fmt_t->atom_id);
                if (!fmt) return 0;
                Term *args_list=(arity==2)?term_deref(pl_unified_term_from_expr(goal->c[1],env)):NULL;
                int nil_id=prolog_atom_intern("[]");
                for (const char *p=fmt; *p; p++) {
                    if (*p=='~') {
                        p++;
                        if (*p=='w'||*p=='a'||*p=='p') {
                            if (args_list&&args_list->tag==TERM_COMPOUND&&args_list->compound.arity==2) {
                                pl_write(term_deref(args_list->compound.args[0]));
                                args_list=term_deref(args_list->compound.args[1]);
                            }
                        } else if (*p=='d') {
                            if (args_list&&args_list->tag==TERM_COMPOUND&&args_list->compound.arity==2) {
                                Term *h=term_deref(args_list->compound.args[0]);
                                if (h&&h->tag==TERM_INT) printf("%ld",h->ival);
                                args_list=term_deref(args_list->compound.args[1]);
                            }
                        } else if (*p=='i') {
                            if (args_list&&args_list->tag==TERM_COMPOUND&&args_list->compound.arity==2)
                                args_list=term_deref(args_list->compound.args[1]);
                        } else if (*p=='n') { putchar('\n');
                        } else if (*p=='N') { putchar('\n');
                        } else if (*p=='~') { putchar('~');
                        } else if (*p=='t') { putchar('\t');
                        } else if (*p=='r') { }
                    } else {
                        putchar(*p);
                    }
                }
                return 1;
            }
            if (strcmp(fn,"numbervars")==0&&arity==3) {
                Term *t   = pl_unified_term_from_expr(goal->c[0],env);
                Term *s   = term_deref(pl_unified_term_from_expr(goal->c[1],env));
                Term *end = pl_unified_term_from_expr(goal->c[2],env);
                if (!s||s->tag!=TERM_INT) return 0;
                long n = s->ival;
                int var_id = prolog_atom_intern("$VAR");
                Term *stk[4096]; int top=0;
                stk[top++]=term_deref(t);
                while (top>0) {
                    Term *cur=term_deref(stk[--top]);
                    if (!cur) continue;
                    if (cur->tag==TERM_VAR) {
                        Term *narg=term_new_int(n++);
                        Term *binding=term_new_compound(var_id,1,&narg);
                        cur->ref=binding; cur->tag=TERM_REF;
                    } else if (cur->tag==TERM_COMPOUND) {
                        for (int i=cur->compound.arity-1;i>=0;i--)
                            if (top<4095) stk[top++]=term_deref(cur->compound.args[i]);
                    }
                }
                Term *end_t=term_new_int(n);
                int mark=trail_mark(trail);
                if(!unify(end,end_t,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"term_singletons")==0&&arity==2) {
                Term *t   = pl_unified_term_from_expr(goal->c[0],env);
                Term *out = pl_unified_term_from_expr(goal->c[1],env);
                Term *vars[1024]; int counts[1024]; int nv=0;
                Term *stk[4096]; int top=0;
                stk[top++]=term_deref(t);
                while (top>0) {
                    Term *cur=term_deref(stk[--top]);
                    if (!cur) continue;
                    if (cur->tag==TERM_VAR) {
                        int found=-1;
                        for (int i=0;i<nv;i++) if (vars[i]==cur) { found=i; break; }
                        if (found>=0) counts[found]++;
                        else if (nv<1024) { vars[nv]=cur; counts[nv]=1; nv++; }
                    } else if (cur->tag==TERM_COMPOUND) {
                        for (int i=cur->compound.arity-1;i>=0;i--)
                            if (top<4095) stk[top++]=term_deref(cur->compound.args[i]);
                    }
                }
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
            if (strcmp(fn,"char_type")==0&&arity==2) {
                Term *ch=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *ty=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                if (!ch||ch->tag!=TERM_ATOM) return 0;
                const char *cs=prolog_atom_name(ch->atom_id);
                if (!cs||!cs[0]) return 0;
                unsigned char c=(unsigned char)cs[0];
                if (!ty||ty->tag==TERM_VAR) return 0;
                const char *tname=NULL;
                int tarity=0;
                if (ty->tag==TERM_ATOM) { tname=prolog_atom_name(ty->atom_id); tarity=0; }
                else if (ty->tag==TERM_COMPOUND) { tname=prolog_atom_name(ty->compound.functor); tarity=ty->compound.arity; }
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
            if (strcmp(fn,"nv_get")==0&&arity==2) {
                Term *nm=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                if (!nm || nm->tag != TERM_ATOM) return 0;
                const char *nm_str = prolog_atom_name(nm->atom_id);
                DESCR_t dv = NV_GET_fn(nm_str);
                Term *val_t = IS_FAIL_fn(dv) ? term_new_atom(ATOM_NIL) :
                              (dv.v==DT_I) ? term_new_int(dv.i) :
                              term_new_atom(prolog_atom_intern(dv.s ? dv.s : ""));
                Term *lhs=pl_unified_term_from_expr(goal->c[1],env);
                int mark=trail_mark(trail);
                if(!unify(lhs,val_t,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"nv_set")==0&&arity==2) {
                Term *nm=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *vl=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                if (!nm || nm->tag != TERM_ATOM) return 0;
                const char *nm_str = prolog_atom_name(nm->atom_id);
                const char *vl_str = (vl && vl->tag==TERM_ATOM) ? prolog_atom_name(vl->atom_id) : NULL;
                DESCR_t dv = (vl && vl->tag==TERM_INT) ? INTVAL(vl->ival) :
                             vl_str                   ? STRVAL(vl_str) : NULVCL;
                NV_SET_fn(nm_str, dv);
                return 1;
            }
            if (strcmp(fn,"term_string")==0&&arity==2) {
                Term *ta=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *ts=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                if (ta && ta->tag!=TERM_VAR) {
                    char *buf = pl_term_to_string(ta);
                    Term *str = term_new_atom(prolog_atom_intern(buf));
                    free(buf);
                    int mark=trail_mark(trail);
                    if(!unify(ts,str,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else if (ts && ts->tag==TERM_ATOM) {
                    const char *s = prolog_atom_name(ts->atom_id);
                    if (!s) return 0;
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
            if (strcmp(fn,"number_codes")==0&&arity==2) {
                Term *tn=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tc=pl_unified_term_from_expr(goal->c[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (tn && (tn->tag==TERM_INT||tn->tag==TERM_FLOAT)) {
                    char buf[64];
                    if (tn->tag==TERM_INT) snprintf(buf,sizeof buf,"%ld",tn->ival);
                    else snprintf(buf,sizeof buf,"%g",tn->fval);
                    int n=(int)strlen(buf);
                    Term *lst=term_new_atom(nil_id);
                    for(int i=n-1;i>=0;i--){Term *a2[2];a2[0]=term_new_int((unsigned char)buf[i]);a2[1]=lst;lst=term_new_compound(dot_id,2,a2);}
                    int mark=trail_mark(trail);
                    if(!unify(tc,lst,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else {
                    Term *cur=term_deref(tc); char buf[64]; int pos=0;
                    while(cur&&cur->tag==TERM_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TERM_INT) return 0;
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
            if (strcmp(fn,"number_chars")==0&&arity==2) {
                Term *tn=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tc=pl_unified_term_from_expr(goal->c[1],env);
                int nil_id=prolog_atom_intern("[]"), dot_id=prolog_atom_intern(".");
                if (tn && (tn->tag==TERM_INT||tn->tag==TERM_FLOAT)) {
                    char buf[64];
                    if (tn->tag==TERM_INT) snprintf(buf,sizeof buf,"%ld",tn->ival);
                    else snprintf(buf,sizeof buf,"%g",tn->fval);
                    int n=(int)strlen(buf);
                    Term *lst=term_new_atom(nil_id);
                    for(int i=n-1;i>=0;i--){char ch[2]={buf[i],'\0'};Term *a2[2];a2[0]=term_new_atom(prolog_atom_intern(ch));a2[1]=lst;lst=term_new_compound(dot_id,2,a2);}
                    int mark=trail_mark(trail);
                    if(!unify(tc,lst,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else {
                    Term *cur=term_deref(tc); char buf[64]; int pos=0;
                    while(cur&&cur->tag==TERM_COMPOUND&&cur->compound.arity==2){
                        Term *hd=term_deref(cur->compound.args[0]);
                        if(!hd||hd->tag!=TERM_ATOM) return 0;
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
            if (strcmp(fn,"char_code")==0&&arity==2) {
                Term *tch=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tco=term_deref(pl_unified_term_from_expr(goal->c[1],env));
                if (tch && tch->tag==TERM_ATOM) {
                    const char *s=prolog_atom_name(tch->atom_id);
                    Term *code=term_new_int(s?(unsigned char)s[0]:0);
                    int mark=trail_mark(trail);
                    if(!unify(tco,code,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                } else if (tco && tco->tag==TERM_INT) {
                    char ch[2]={(char)tco->ival,'\0'};
                    Term *cat=term_new_atom(prolog_atom_intern(ch));
                    int mark=trail_mark(trail);
                    if(!unify(tch,cat,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            if (strcmp(fn,"upcase_atom")==0&&arity==2) {
                Term *ta=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tr=pl_unified_term_from_expr(goal->c[1],env);
                if (!ta||ta->tag!=TERM_ATOM) return 0;
                const char *s=prolog_atom_name(ta->atom_id); if(!s) return 0;
                char *buf=malloc(strlen(s)+1);
                for(int i=0;s[i];i++) buf[i]=(char)toupper((unsigned char)s[i]); buf[strlen(s)]='\0';
                Term *res=term_new_atom(prolog_atom_intern(buf)); free(buf);
                int mark=trail_mark(trail);
                if(!unify(tr,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"downcase_atom")==0&&arity==2) {
                Term *ta=term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tr=pl_unified_term_from_expr(goal->c[1],env);
                if (!ta||ta->tag!=TERM_ATOM) return 0;
                const char *s=prolog_atom_name(ta->atom_id); if(!s) return 0;
                char *buf=malloc(strlen(s)+1);
                for(int i=0;s[i];i++) buf[i]=(char)tolower((unsigned char)s[i]); buf[strlen(s)]='\0';
                Term *res=term_new_atom(prolog_atom_intern(buf)); free(buf);
                int mark=trail_mark(trail);
                if(!unify(tr,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"sub_atom")==0&&arity==5) {
                Term *ta  = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tb  = pl_unified_term_from_expr(goal->c[1],env);
                Term *tl  = pl_unified_term_from_expr(goal->c[2],env);
                Term *taf = pl_unified_term_from_expr(goal->c[3],env);
                Term *ts  = pl_unified_term_from_expr(goal->c[4],env);
                if (!ta || ta->tag != TERM_ATOM) return 0;
                const char *s = prolog_atom_name(ta->atom_id); if (!s) return 0;
                int slen = (int)strlen(s);
                int bef = -1, len = -1;
                Term *tbv = term_deref(tb); if (tbv && tbv->tag == TERM_INT) bef = (int)tbv->ival;
                Term *tlv = term_deref(tl); if (tlv && tlv->tag == TERM_INT) len = (int)tlv->ival;
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
            if (strcmp(fn,"atom_number")==0&&arity==2) {
                Term *ta = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tn = pl_unified_term_from_expr(goal->c[1],env);
                Term *tnv = term_deref(tn);
                if (ta && ta->tag == TERM_ATOM) {
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
                } else if (tnv && (tnv->tag == TERM_INT || tnv->tag == TERM_FLOAT)) {
                    char buf[64];
                    if (tnv->tag == TERM_INT) snprintf(buf, sizeof buf, "%ld", tnv->ival);
                    else snprintf(buf, sizeof buf, "%g", tnv->fval);
                    Term *rat = term_new_atom(prolog_atom_intern(buf));
                    int mark = trail_mark(trail);
                    if (!unify(ta, rat, trail)) { trail_unwind(trail,mark); return 0; }
                    return 1;
                }
                return 0;
            }
            if (strcmp(fn,"atom_to_term")==0&&arity==3) {
                Term *ta   = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *tt   = pl_unified_term_from_expr(goal->c[1],env);
                Term *tbnd = pl_unified_term_from_expr(goal->c[2],env);
                if (ta && ta->tag == TERM_ATOM) {
                    const char *src = prolog_atom_name(ta->atom_id);
                    if (!src) return 0;
                    char *prog = malloc(strlen(src)+32);
                    snprintf(prog, strlen(src)+32, ":- X = (%s).\n", src);
                    free(prog);
                    const char *p = src;
                    while (*p && *p != '(' && *p != ',') p++;
                    Term *parsed = NULL;
                    if (*p == '\0') {
                        char *end; long iv = strtol(src, &end, 10);
                        if (*end=='\0') parsed = term_new_int(iv);
                        else { double dv = strtod(src,&end); if(*end=='\0') parsed=term_new_float(dv); }
                        if (!parsed) parsed = term_new_atom(prolog_atom_intern(src));
                    } else {
                        int flen = (int)(strchr(src,'(') - src);
                        char *fname2 = malloc(flen+1); strncpy(fname2,src,flen); fname2[flen]='\0';
                        int fatom = prolog_atom_intern(fname2); free(fname2);
                        int depth=0, argc=1; const char *q=strchr(src,'(')+1;
                        const char *qend = src+strlen(src)-1;
                        for (const char *r=q; r<qend; r++) {
                            if (*r=='(') depth++;
                            else if (*r==')') depth--;
                            else if (*r==',' && depth==0) argc++;
                        }
                        Term **args = malloc(argc*sizeof(Term*));
                        int ai=0; const char *astart=q;
                        depth=0;
                        for (const char *r=q; r<=qend; r++) {
                            if (*r=='(') depth++;
                            else if (*r==')') depth--;
                            int sep = (r==qend) || (*r==',' && depth==0);
                            if (sep) {
                                int alen=(int)(r-astart);
                                char *abuf=malloc(alen+1); strncpy(abuf,astart,alen); abuf[alen]='\0';
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
                    Term *tv = term_deref(tt);
                    if (!tv) return 0;
                    char *buf = NULL; size_t bsz = 0;
                    FILE *mf = open_memstream(&buf, &bsz);
                    if (!mf) return 0;
                    FILE *old_stdout = stdout;
                    fflush(stdout);
                    int pipefd[2]; pipe(pipefd);
                    int saved_fd = dup(1); dup2(pipefd[1], 1); close(pipefd[1]);
                    pl_write(tv);
                    fflush(stdout);
                    dup2(saved_fd, 1); close(saved_fd);
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
            if ((strcmp(fn,"phrase")==0) && (arity==2||arity==3)) {
                Term *rule = term_deref(pl_unified_term_from_expr(goal->c[0], env));
                Term *s0   = pl_unified_term_from_expr(goal->c[1], env);
                Term *s1   = (arity==3) ? pl_unified_term_from_expr(goal->c[2], env)
                                        : term_new_atom(prolog_atom_intern("[]"));
                if (!rule) return 0;
                const char *rfn = NULL;
                int rarity = 0;
                Term **rargs = NULL;
                if (rule->tag == TERM_ATOM) {
                    rfn = prolog_atom_name(rule->atom_id);
                    rarity = 0; rargs = NULL;
                } else if (rule->tag == TERM_COMPOUND) {
                    rfn = prolog_atom_name(rule->compound.functor);
                    rarity = rule->compound.arity;
                    rargs = rule->compound.args;
                }
                if (!rfn) return 0;
                int call_arity = rarity + 2;
                char ukey[256]; snprintf(ukey, sizeof ukey, "%s/%d", rfn, call_arity);
                tree_t *uch = pl_pred_table_lookup(&g_pl_pred_table, ukey);
                if (!uch) return 0;
                Term **uargs = pl_env_new(call_arity);
                for (int ui = 0; ui < rarity; ui++) uargs[ui] = term_deref(rargs[ui]);
                uargs[rarity]   = s0;
                uargs[rarity+1] = s1;
                Trail *utrail = &g_pl_trail;
                int umark = trail_mark(utrail);
                Term **saved_env = g_pl_env;
                g_pl_env = uargs;
                Pl_PredEntry *_upe1 = pl_pred_entry_lookup(ukey);
                extern void *g_current_sm_prog;
                bb_node_t uroot = (_upe1 && _upe1->entry_pc >= 0 && g_current_sm_prog != NULL)
                    ? pl_box_choice_pc(_upe1->entry_pc, g_pl_env, call_arity)
                    : pl_box_choice(uch, g_pl_env, call_arity);
                int uok = bb_broker(uroot, BB_ONCE, NULL, NULL);
                g_pl_env = saved_env;
                if (!uok) trail_unwind(utrail, umark);
                if (uargs) free(uargs);
                return uok;
            }
            if (strcmp(fn,"findall")==0&&arity==3){
                tree_t *tmpl_expr=goal->c[0];
                tree_t *goal_expr=goal->c[1];
                tree_t *list_expr=goal->c[2];
                Term **solutions=NULL; int nsol=0,sol_cap=0;
                Trail fa_trail; trail_init(&fa_trail);
                Trail saved_global_trail=g_pl_trail;
                g_pl_trail=fa_trail;
                tree_t *fa_synth = NULL; Term **fa_tenv = NULL;
                Term **outer_env = env;
                if (goal_expr && goal_expr->t == TERM_VAR) {
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
                    Term *snap=pl_copy_term(pl_unified_term_from_expr(tmpl_expr,outer_env));
                    if(nsol>=sol_cap){sol_cap=sol_cap?sol_cap*2:8;solutions=realloc(solutions,sol_cap*sizeof(Term*));}
                    solutions[nsol++]=snap;
                    fa_r=goal_box.fn(goal_box.ζ,β);
                }
                g_pl_trail=saved_global_trail;
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
            {
                char ukey[256]; snprintf(ukey, sizeof ukey, "%s/%d", fn, arity);
                tree_t *uch = pl_pred_table_lookup(&g_pl_pred_table, ukey);
                if (uch) {
                    Term **uargs = (arity > 0) ? pl_env_new(arity) : NULL;
                    Trail *utrail = &g_pl_trail;
                    int umark = trail_mark(utrail);
                    int uok = 1;
                    for (int ui = 0; ui < arity && uok; ui++) {
                        Term *actual = pl_unified_term_from_expr(goal->c[ui], env);
                        if (!unify(uargs[ui], actual, utrail)) { uok = 0; }
                    }
                    if (uok) {
                        Term **saved_env = g_pl_env;
                        g_pl_env = uargs;
                        Pl_PredEntry *_upe2 = pl_pred_entry_lookup(ukey);
                        extern void *g_current_sm_prog;
                        bb_node_t uroot = (_upe2 && _upe2->entry_pc >= 0 && g_current_sm_prog != NULL)
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
            if (strcmp(fn,"copy_term")==0&&arity==2) {
                Term *orig = pl_unified_term_from_expr(goal->c[0],env);
                Term *dest = pl_unified_term_from_expr(goal->c[1],env);
                Term *copy = pl_copy_term(orig);
                int mark = trail_mark(trail);
                if (!unify(dest, copy, trail)) { trail_unwind(trail,mark); return 0; }
                return 1;
            }
            if ((strcmp(fn,"atomic_list_concat")==0||strcmp(fn,"concat_atom")==0)&&(arity==2||arity==3)) {
                Term *lst = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                int nil_id = prolog_atom_intern("[]"), dot_id = prolog_atom_intern(".");
                const char *sep = "";
                char sepbuf[64]; sepbuf[0]='\0';
                if (arity==3) {
                    Term *sv = term_deref(pl_unified_term_from_expr(goal->c[1],env));
                    if (sv && sv->tag==TERM_ATOM) sep=prolog_atom_name(sv->atom_id);
                    else if (sv && sv->tag==TERM_INT) { snprintf(sepbuf,sizeof sepbuf,"%ld",sv->ival); sep=sepbuf; }
                }
                char buf[4096]; int pos=0; int first=1;
                for (Term *cur=lst; cur; cur=term_deref(cur->compound.args[1])) {
                    cur = term_deref(cur);
                    if (!cur) break;
                    if (cur->tag==TERM_ATOM && cur->atom_id==nil_id) break;
                    if (cur->tag!=TERM_COMPOUND || cur->compound.arity!=2) break;
                    Term *hd = term_deref(cur->compound.args[0]);
                    const char *s = NULL; char tmp[64];
                    if (hd && hd->tag==TERM_ATOM) s=prolog_atom_name(hd->atom_id);
                    else if (hd && hd->tag==TERM_INT) { snprintf(tmp,sizeof tmp,"%ld",hd->ival); s=tmp; }
                    else if (hd && hd->tag==TERM_FLOAT) { snprintf(tmp,sizeof tmp,"%g",hd->fval); s=tmp; }
                    if (!s) s="";
                    if (!first && sep[0]) { int sl=(int)strlen(sep); if(pos+sl<(int)sizeof buf-1){memcpy(buf+pos,sep,sl);pos+=sl;} }
                    first=0;
                    int sl=(int)strlen(s); if(pos+sl<(int)sizeof buf-1){memcpy(buf+pos,s,sl);pos+=sl;}
                }
                buf[pos]='\0';
                Term *res = term_new_atom(prolog_atom_intern(buf));
                Term *out = pl_unified_term_from_expr(goal->c[arity==3?2:1],env);
                int mark = trail_mark(trail);
                if (!unify(out,res,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"string_to_atom")==0&&arity==2) {
                Term *a0 = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *a1 = pl_unified_term_from_expr(goal->c[1],env);
                if (a0 && (a0->tag==TERM_ATOM||a0->tag==TERM_INT||a0->tag==TERM_FLOAT)) {
                    const char *s=NULL; char tmp[64];
                    if(a0->tag==TERM_ATOM) s=prolog_atom_name(a0->atom_id);
                    else if(a0->tag==TERM_INT){snprintf(tmp,sizeof tmp,"%ld",a0->ival);s=tmp;}
                    else{snprintf(tmp,sizeof tmp,"%g",a0->fval);s=tmp;}
                    Term *res=term_new_atom(prolog_atom_intern(s));
                    int mark=trail_mark(trail);
                    if(!unify(a1,res,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                Term *a1d = term_deref(a1);
                if (a1d && a1d->tag==TERM_ATOM) {
                    int mark=trail_mark(trail);
                    if(!unify(a0,a1d,trail)){trail_unwind(trail,mark);return 0;}
                    return 1;
                }
                return 0;
            }
            if (strcmp(fn,"nb_setval")==0&&arity==2) {
                Term *key = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *val = pl_unified_term_from_expr(goal->c[1],env);
                if (!key || key->tag!=TERM_ATOM) return 0;
                pl_nb_setval(prolog_atom_name(key->atom_id), val);
                return 1;
            }
            if (strcmp(fn,"nb_getval")==0&&arity==2) {
                Term *key = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                Term *out = pl_unified_term_from_expr(goal->c[1],env);
                if (!key || key->tag!=TERM_ATOM) return 0;
                Term *val = pl_nb_getval(prolog_atom_name(key->atom_id));
                if (!val) return 0;
                int mark=trail_mark(trail);
                if(!unify(out,val,trail)){trail_unwind(trail,mark);return 0;}
                return 1;
            }
            if (strcmp(fn,"aggregate_all")==0&&arity==3) {
                Term   *tmpl_t    = term_deref(pl_unified_term_from_expr(goal->c[0],env));
                tree_t *goal_expr = goal->c[1];
                Term   *result_out= pl_unified_term_from_expr(goal->c[2],env);
                int is_count=0, is_sum=0, is_max=0, is_min=0;
                tree_t *val_expr = NULL;
                if (tmpl_t && tmpl_t->tag==TERM_ATOM) {
                    const char *tn=prolog_atom_name(tmpl_t->atom_id);
                    if (strcmp(tn,"count")==0) is_count=1;
                } else if (tmpl_t && tmpl_t->tag==TERM_COMPOUND && tmpl_t->compound.arity==1) {
                    const char *fn2=prolog_atom_name(tmpl_t->compound.functor);
                    if (goal->c[0] && goal->c[0]->n>0)
                        val_expr = goal->c[0]->c[0];
                    if (strcmp(fn2,"sum")==0) is_sum=1;
                    else if (strcmp(fn2,"max")==0) is_max=1;
                    else if (strcmp(fn2,"min")==0) is_min=1;
                }
                Trail ag_trail; trail_init(&ag_trail);
                Trail saved_global_trail = g_pl_trail;
                g_pl_trail = ag_trail;
                long ag_count=0, ag_sum=0, ag_best=0; int ag_best_set=0;
                tree_t *snap_expr = (is_count || !val_expr) ? NULL : val_expr;
                bb_node_t goal_box = pl_box_goal_from_ir(goal_expr, env);
                DESCR_t ag_r = goal_box.fn(goal_box.ζ, α);
                while (!IS_FAIL_fn(ag_r)) {
                    ag_count++;
                    if ((is_sum||is_max||is_min) && snap_expr) {
                        Term *vt = term_deref(pl_unified_term_from_expr(snap_expr, env));
                        long v=0;
                        if (vt && vt->tag==TERM_INT)   v=vt->ival;
                        else if (vt && vt->tag==TERM_FLOAT) v=(long)vt->fval;
                        if (is_sum) ag_sum += v;
                        if (!ag_best_set || (is_max && v>ag_best) || (is_min && v<ag_best)) {
                            ag_best=v; ag_best_set=1;
                        }
                    }
                    ag_r = goal_box.fn(goal_box.ζ, β);
                }
                g_pl_trail = saved_global_trail;
                Term *res = NULL;
                if (is_count) res = term_new_int(ag_count);
                else if (is_sum) res = term_new_int(ag_sum);
                else if ((is_max||is_min) && ag_best_set) res = term_new_int(ag_best);
                else if (is_max||is_min) return 0;
                else res = term_new_int(ag_count);
                int mark = trail_mark(trail);
                if (!unify(result_out, res, trail)) { trail_unwind(trail,mark); return 0; }
                return 1;
            }
            if (strcmp(fn,"throw")==0&&arity==1) {
                Term *exc = pl_unified_term_from_expr(goal->c[0],env);
                g_pl_exception = exc;
                while (g_pl_catch_top > 0) {
                    int ci = g_pl_catch_top - 1;
                    Pl_CatchFrame *cf = &g_pl_catch_stack[ci];
                    Trail tmptrail; trail_init(&tmptrail);
                    int tmmark = trail_mark(&tmptrail);
                    int matched = unify(cf->catcher, exc, &tmptrail);
                    trail_unwind(&tmptrail, tmmark);
                    if (matched) {
                        longjmp(cf->jb, 1);
                    }
                    g_pl_catch_top--;
                }
                fprintf(stderr,"ERROR: Unhandled exception: ");
                pl_write(exc); fprintf(stderr,"\n");
                exit(1);
            }
            if (strcmp(fn,"catch")==0&&arity==3) {
                tree_t *goal_e   = goal->c[0];
                Term   *catcher  = pl_unified_term_from_expr(goal->c[1],env);
                tree_t *recovery = goal->c[2];
                if (g_pl_catch_top >= PL_CATCH_STACK_MAX) return 0;
                Pl_CatchFrame *cf = &g_pl_catch_stack[g_pl_catch_top];
                cf->catcher    = catcher;
                cf->env        = env;
                cf->trail_mark = trail_mark(trail);
                g_pl_catch_top++;
                int threw = setjmp(cf->jb);
                if (!threw) {
                    int ok;
                    if (goal_e && goal_e->t == TERM_VAR) {
                        Term *gt = pl_unified_term_from_expr(goal_e, env);
                        ok = pl_invoke_var_goal(gt, env);
                    } else if (is_pl_user_call(goal_e)) {
                        char ukey[256];
                        snprintf(ukey,sizeof ukey,"%s/%d",
                                 goal_e->v.sval?goal_e->v.sval:"",goal_e->n);
                        tree_t *uch=pl_pred_table_lookup(&g_pl_pred_table,ukey);
                        if (uch && uch->n > 0) {
                            int ua=goal_e->n;
                            Term **uenv=ua?pl_env_new(ua):NULL;
                            for(int ui=0;ui<ua;ui++)
                                uenv[ui]=pl_unified_term_from_expr(goal_e->c[ui],env);
                            Term **sv=g_pl_env; g_pl_env=uenv;
                            DESCR_t rd=bb_eval_value(uch); g_pl_env=sv;
                            if(uenv)free(uenv);
                            ok=!IS_FAIL_fn(rd);
                        } else {
                            ok = pl_throw_existence_error_procedure(
                                goal_e->v.sval ? goal_e->v.sval : "", goal_e->n);
                        }
                    } else {
                        ok = interp_exec_pl_builtin(goal_e, env);
                    }
                    if (g_pl_catch_top>0 && &g_pl_catch_stack[g_pl_catch_top-1]==cf)
                        g_pl_catch_top--;
                    return ok;
                } else {
                    trail_unwind(trail, cf->trail_mark);
                    Term *exc = g_pl_exception;
                    g_pl_exception = NULL;
                    int mark2=trail_mark(trail);
                    unify(catcher, exc, trail);
                    int rok = interp_exec_pl_builtin(recovery, env);
                    return rok;
                }
            }
            if (strcmp(fn,"setup_call_cleanup")==0&&arity==3) {
                tree_t *setup_e    = goal->c[0];
                tree_t *scc_goal_e = goal->c[1];
                tree_t *cleanup_e  = goal->c[2];
                tree_t *s_synth=NULL; Term **s_tenv=NULL;
                tree_t *g_synth=NULL; Term **g_tenv=NULL;
                tree_t *c_synth=NULL; Term **c_tenv=NULL;
                if (setup_e && setup_e->t==TERM_VAR) {
                    Term *gt=term_deref(pl_unified_term_from_expr(setup_e,env));
                    s_tenv=calloc(PL_SYNTH_TENV_MAX,sizeof(Term*)); int n=0;
                    s_synth=pl_term_to_synth_expr(gt,s_tenv,&n); setup_e=s_synth;
                }
                if (scc_goal_e && scc_goal_e->t==TERM_VAR) {
                    Term *gt=term_deref(pl_unified_term_from_expr(scc_goal_e,env));
                    g_tenv=calloc(PL_SYNTH_TENV_MAX,sizeof(Term*)); int n=0;
                    g_synth=pl_term_to_synth_expr(gt,g_tenv,&n); scc_goal_e=g_synth;
                }
                if (cleanup_e && cleanup_e->t==TERM_VAR) {
                    Term *gt=term_deref(pl_unified_term_from_expr(cleanup_e,env));
                    c_tenv=calloc(PL_SYNTH_TENV_MAX,sizeof(Term*)); int n=0;
                    c_synth=pl_term_to_synth_expr(gt,c_tenv,&n); cleanup_e=c_synth;
                }
                int sok = interp_exec_pl_builtin(setup_e, s_synth ? s_tenv : env);
                if (!sok) {
                    if (s_synth){pl_synth_free(s_synth);free(s_tenv);}
                    if (g_synth){pl_synth_free(g_synth);free(g_tenv);}
                    if (c_synth){pl_synth_free(c_synth);free(c_tenv);}
                    return 0;
                }
                int gok = interp_exec_pl_builtin(scc_goal_e, g_synth ? g_tenv : env);
                interp_exec_pl_builtin(cleanup_e, c_synth ? c_tenv : env);
                if (s_synth){pl_synth_free(s_synth);free(s_tenv);}
                if (g_synth){pl_synth_free(g_synth);free(g_tenv);}
                if (c_synth){pl_synth_free(c_synth);free(c_tenv);}
                return gok;
            }
            return 1;
        }
        default: return 1;
    }
}
