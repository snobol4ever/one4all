#include "frontend/prolog/pl_broker.h"
#include "frontend/prolog/pl_interp.h"
#include "SM.h"
#include "stage2.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t pl_gamma(void) { return descr_bool(1); }
typedef struct { int fired; } pl_true_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_true_fn(void *zeta, int entry) {
    pl_true_t *ζ = zeta;
    if (entry == α) { ζ->fired = 1; return NULVCL; }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_true(void) {
    pl_true_t *ζ = calloc(1, sizeof(pl_true_t));
    return (bb_node_t){ pl_true_fn, ζ, 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_fail_fn(void *zeta, int entry) {
    (void)zeta; (void)entry;
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_fail(void) {
    return (bb_node_t){ pl_fail_fn, NULL, 0 };
}
typedef struct { tree_t *goal; Term **env; } pl_builtin_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_builtin_fn(void *zeta, int entry) {
    pl_builtin_t *ζ = zeta;
    if (entry == α) {
        int ok = interp_exec_pl_builtin(ζ->goal, ζ->env);
        return ok ? NULVCL : FAILDESCR;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_builtin(tree_t *goal, Term **env) {
    pl_builtin_t *ζ = malloc(sizeof(pl_builtin_t));
    ζ->goal = goal;
    ζ->env  = env;
    return (bb_node_t){ pl_builtin_fn, ζ, 0 };
}
typedef struct { bb_node_t left; bb_node_t right; } pl_cat_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_cat_fn(void *zeta, int entry) {
    pl_cat_t *ζ = zeta;
    DESCR_t lr, rr;
    if (entry == α)                                 goto CAT_α;
                                    goto CAT_β;
CAT_α:  lr = ζ->left.fn(ζ->left.ζ, α);
        if (IS_FAIL_fn(lr))                      goto left_ω;
                                                    goto left_γ;
CAT_β:  rr = ζ->right.fn(ζ->right.ζ, β);
        if (IS_FAIL_fn(rr))                      goto right_ω;
                                                    goto right_γ;
left_γ: rr = ζ->right.fn(ζ->right.ζ, α);
        if (IS_FAIL_fn(rr))                      goto right_ω;
                                                    goto right_γ;
left_ω:                                             return FAILDESCR;
right_γ:                                            return NULVCL;
right_ω: lr = ζ->left.fn(ζ->left.ζ, β);
        if (IS_FAIL_fn(lr))                      goto left_ω;
                                                    goto left_γ;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_cat(bb_node_t left, bb_node_t right) {
    pl_cat_t *ζ = malloc(sizeof(pl_cat_t));
    ζ->left  = left;
    ζ->right = right;
    return (bb_node_t){ pl_cat_fn, ζ, 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_cat_list(bb_node_t *goals, int n) {
    if (n == 0) return pl_box_true();
    if (n == 1) return goals[0];
    bb_node_t acc = pl_box_cat(goals[0], goals[1]);
    for (int i = 2; i < n; i++) acc = pl_box_cat(acc, goals[i]);
    return acc;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_choice_call(tree_t *goal, Term **env);
typedef struct { tree_t *node; Term **env; int mark; int fired; } pl_unify_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_unify_fn(void *zeta, int entry) {
    pl_unify_t *ζ = zeta;
    if (entry == α) {
        ζ->mark  = trail_mark(&g_pl_trail);
        ζ->fired = 0;
        if (!ζ->node || ζ->node->n < 2) return FAILDESCR;
        Term *t1 = pl_unified_term_from_expr(ζ->node->c[0], ζ->env);
        Term *t2 = pl_unified_term_from_expr(ζ->node->c[1], ζ->env);
        if (!unify(t1, t2, &g_pl_trail)) { trail_unwind(&g_pl_trail, ζ->mark); return FAILDESCR; }
        ζ->fired = 1;
        return NULVCL;
    }
    if (ζ->fired) { trail_unwind(&g_pl_trail, ζ->mark); ζ->fired = 0; }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static bb_node_t pl_box_unify(tree_t *node, Term **env) {
    pl_unify_t *ζ = calloc(1, sizeof(pl_unify_t));
    ζ->node = node; ζ->env = env;
    return (bb_node_t){ pl_unify_fn, ζ, 0 };
}
typedef struct {
    Term   **caller_args;
    int      arity;
    tree_t  *ec;
    Term   **cenv;
    int      n_vars;
    int      head_mark;
    int      fired;
} pl_head_unify_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_head_unify_fn(void *zeta, int entry) {
    pl_head_unify_t *ζ = zeta;
    if (entry == α) {
        ζ->head_mark = trail_mark(&g_pl_trail);
        ζ->cenv      = pl_env_new(ζ->n_vars);
        ζ->fired     = 0;
        for (int i = 0; i < ζ->arity && i < ζ->ec->n; i++) {
            Term *ha = pl_unified_term_from_expr(ζ->ec->c[i], ζ->cenv);
            Term *ca = (ζ->caller_args && i < ζ->arity) ? ζ->caller_args[i] : term_new_var(i);
            if (!unify(ca, ha, &g_pl_trail)) {
                trail_unwind(&g_pl_trail, ζ->head_mark);
                if (ζ->cenv) { free(ζ->cenv); ζ->cenv = NULL; }
                return FAILDESCR;
            }
        }
        ζ->fired = 1;
        return NULVCL;
    }
    if (ζ->fired) {
        trail_unwind(&g_pl_trail, ζ->head_mark);
        if (ζ->cenv) { free(ζ->cenv); ζ->cenv = NULL; }
        ζ->fired = 0;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_is_builtin_goal(tree_t *g);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static bb_node_t pl_box_alt(bb_node_t left, bb_node_t right);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_goal_from_ir(tree_t *g, Term **env) {
    if (!g) return pl_box_true();
    if (g->t == TT_CUT)   return pl_box_cut();
    if (g->t == TT_UNIFY) return pl_box_unify(g, env);
    if (g->t == TT_FNC) {
        if (g->v.sval && strcmp(g->v.sval, "true") == 0) return pl_box_true();
        if (g->v.sval && strcmp(g->v.sval, "fail") == 0) return pl_box_fail();
        if (g->v.sval && strcmp(g->v.sval, ",") == 0 && g->n >= 2) {
            bb_node_t *goals = malloc(g->n * sizeof(bb_node_t));
            for (int i = 0; i < g->n; i++)
                goals[i] = pl_box_goal_from_ir(g->c[i], env);
            bb_node_t result = pl_box_cat_list(goals, g->n);
            free(goals);
            return result;
        }
        if (g->v.sval && strcmp(g->v.sval, ";") == 0 && g->n >= 2) {
            tree_t *lc = g->c[0];
            if (lc && lc->t == TT_FNC && lc->v.sval &&
                strcmp(lc->v.sval, "->") == 0 && lc->n >= 2) {
                bb_node_t *then_goals = malloc(lc->n * sizeof(bb_node_t));
                for (int i = 0; i < lc->n; i++)
                    then_goals[i] = pl_box_goal_from_ir(lc->c[i], env);
                bb_node_t ite_left = pl_box_cat_list(then_goals, lc->n);
                free(then_goals);
                bb_node_t ite_right = pl_box_goal_from_ir(g->c[1], env);
                return pl_box_alt(ite_left, ite_right);
            }
            bb_node_t acc = pl_box_goal_from_ir(g->c[g->n - 1], env);
            for (int i = g->n - 2; i >= 0; i--)
                acc = pl_box_alt(pl_box_goal_from_ir(g->c[i], env), acc);
            return acc;
        }
        if (g->v.sval && strcmp(g->v.sval, "->") == 0 && g->n >= 2) {
            bb_node_t *goals = malloc(g->n * sizeof(bb_node_t));
            for (int i = 0; i < g->n; i++)
                goals[i] = pl_box_goal_from_ir(g->c[i], env);
            bb_node_t result = pl_box_cat_list(goals, g->n);
            free(goals);
            return result;
        }
        if (pl_is_builtin_goal(g)) return pl_box_builtin(g, env);
        return pl_box_choice_call(g, env);
    }
    return pl_box_true();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_is_builtin_goal(tree_t *g) {
    if (!g || g->t != TT_FNC || !g->v.sval) return 0;
    static const char *builtins[] = {
        "true","fail","halt","nl","write","writeln","print","writeq","write_canonical","tab","is",
        "<",">","=<",">=","=:=","=\\=","=","\\=","==","\\==",
        "@<","@>","@=<","@>=",
        "var","nonvar","atom","integer","float","compound","atomic","callable","is_list",
        "functor","arg","=..","\\+","not","once","findall",
        "assert","assertz","asserta","retract","retractall","abolish",
        "atom_length","atom_concat","atom_chars","atom_codes","atom_number","atom_to_term","sub_atom",
        "sort","msort","compare","@<","@>","@=<","@>=",
        "succ","plus","format",
        "numbervars","char_type","term_singletons",
        "nv_get","nv_set",
        "term_string","number_codes","number_chars","char_code","upcase_atom","downcase_atom",
        "copy_term","atomic_list_concat","concat_atom","string_to_atom",
        "nb_setval","nb_getval","aggregate_all","throw","catch",
        "phrase",
        "call","setup_call_cleanup",
        NULL
    };
    for (int i = 0; builtins[i]; i++)
        if (strcmp(g->v.sval, builtins[i]) == 0) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_goal_from_ir(tree_t *g, Term **env);
typedef struct {
    tree_t    *goal_node;
    Term    ***env_ptr;
    bb_node_t inner;
    int        built;
} pl_deferred_env_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_deferred_env_fn(void *zeta, int entry) {
    pl_deferred_env_t *ζ = zeta;
    if (entry == α || !ζ->built) {
        ζ->inner = pl_box_goal_from_ir(ζ->goal_node, *ζ->env_ptr);
        ζ->built  = 1;
        return ζ->inner.fn(ζ->inner.ζ, α);
    }
    return ζ->inner.fn(ζ->inner.ζ, β);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static bb_node_t pl_box_deferred_env(tree_t *goal_node, Term ***env_ptr) {
    pl_deferred_env_t *ζ = calloc(1, sizeof(pl_deferred_env_t));
    ζ->goal_node = goal_node;
    ζ->env_ptr   = env_ptr;
    return (bb_node_t){ pl_deferred_env_fn, ζ, 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_clause(tree_t *ec, Term **caller_args, int arity) {
    if (!ec || ec->t != TT_CLAUSE) return pl_box_fail();
    int n_vars = (int)ec->v.ival;
    int nbody  = ec->n - arity;
    if (nbody < 0) nbody = 0;
    pl_head_unify_t *hζ = calloc(1, sizeof(pl_head_unify_t));
    hζ->caller_args = caller_args;
    hζ->arity       = arity;
    hζ->ec          = ec;
    hζ->n_vars      = n_vars;
    bb_node_t head_box = (bb_node_t){ pl_head_unify_fn, hζ, 0 };
    if (nbody == 0) return head_box;
    int total = 1 + nbody;
    bb_node_t *boxes = malloc(total * sizeof(bb_node_t));
    boxes[0] = head_box;
    for (int i = 0; i < nbody; i++)
        boxes[1 + i] = pl_box_deferred_env(ec->c[arity + i], &hζ->cenv);
    bb_node_t result = pl_box_cat_list(boxes, total);
    free(boxes);
    return result;
}
static int *g_pl_cur_cut_flag = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_cut_fn(void *zeta, int entry) {
    (void)zeta;
    if (entry == α) {
        if (g_pl_cur_cut_flag) *g_pl_cur_cut_flag = 1;
        else                    g_pl_cut_flag = 1;
        return NULVCL;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_cut(void) {
    return (bb_node_t){ pl_cut_fn, NULL, 0 };
}
typedef struct { bb_node_t left; bb_node_t right; int phase; } pl_alt_zeta_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_alt_fn(void *zeta, int entry) {
    pl_alt_zeta_t *ζ = (pl_alt_zeta_t *)zeta;
    DESCR_t r;
    if (entry == α) {
        r = ζ->left.fn(ζ->left.ζ, α);
        if (!IS_FAIL_fn(r)) { ζ->phase = 0; return r; }
        ζ->phase = 2;
        return ζ->right.fn(ζ->right.ζ, α);
    }
    if (ζ->phase == 0) {
        r = ζ->left.fn(ζ->left.ζ, β);
        if (!IS_FAIL_fn(r)) return r;
        ζ->phase = 2;
        return ζ->right.fn(ζ->right.ζ, α);
    }
    return ζ->right.fn(ζ->right.ζ, β);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static bb_node_t pl_box_alt(bb_node_t left, bb_node_t right) {
    pl_alt_zeta_t *ζ = malloc(sizeof(pl_alt_zeta_t));
    ζ->left = left; ζ->right = right; ζ->phase = 0;
    return (bb_node_t){ pl_alt_fn, ζ, 0 };
}
typedef struct {
    tree_t    **clauses;
    int         nclause;
    int         ci;
    int         trail_mark;
    Term      **caller_args;
    int         arity;
    bb_node_t  cur;
    int         cur_active;
    int         cut;
} pl_choice_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_choice_fn(void *zeta, int entry) {
    pl_choice_t *ζ = zeta;
    DESCR_t r;
    if (entry == β && ζ->cur_active) {
        r = ζ->cur.fn(ζ->cur.ζ, β);
        if (!IS_FAIL_fn(r)) return NULVCL;
        trail_unwind(&g_pl_trail, ζ->trail_mark);
        ζ->ci++;
        ζ->cur_active = 0;
    } else if (entry == α) {
        ζ->trail_mark = trail_mark(&g_pl_trail);
        ζ->ci         = 0;
        ζ->cur_active = 0;
        ζ->cut        = 0;
    }
    int *saved_cut_ptr = g_pl_cur_cut_flag;
    g_pl_cur_cut_flag  = &ζ->cut;
    while (ζ->ci < ζ->nclause && !ζ->cut) {
        tree_t *ec = ζ->clauses[ζ->ci];
        if (!ec || ec->t != TT_CLAUSE) { ζ->ci++; continue; }
        trail_unwind(&g_pl_trail, ζ->trail_mark);
        ζ->cur        = pl_box_clause(ec, ζ->caller_args, ζ->arity);
        ζ->cur_active = 1;
        r = ζ->cur.fn(ζ->cur.ζ, α);
        if (!IS_FAIL_fn(r)) {
            g_pl_cur_cut_flag = saved_cut_ptr;
            return NULVCL;
        }
        ζ->ci++;
        ζ->cur_active = 0;
    }
    g_pl_cur_cut_flag = saved_cut_ptr;
    trail_unwind(&g_pl_trail, ζ->trail_mark);
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_choice(tree_t *choice_node, Term **caller_args, int arity) {
    if (!choice_node || choice_node->t != TT_CHOICE) return pl_box_fail();
    pl_choice_t *ζ = calloc(1, sizeof(pl_choice_t));
    ζ->clauses     = choice_node->c;
    ζ->nclause     = choice_node->n;
    ζ->caller_args = caller_args;
    ζ->arity       = arity;
    return (bb_node_t){ pl_choice_fn, ζ, 0 };
}
typedef struct { int entry_pc; int fired; } pl_expression_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t pl_chunk_fn(void *zeta, int entry) {
    extern DESCR_t sm_call_expression(int);
    pl_expression_t *z = zeta;
    if (entry == β || z->fired) return FAILDESCR;
    z->fired = 1;
    if (0) return FAILDESCR;
    DESCR_t r = sm_call_expression(z->entry_pc);
    return IS_FAIL_fn(r) ? FAILDESCR : NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_choice_pc(int entry_pc, Term **caller_args, int arity) {
    (void)caller_args; (void)arity;
    if (entry_pc < 0) return pl_box_fail();
    pl_expression_t *z = calloc(1, sizeof(pl_expression_t));
    z->entry_pc = entry_pc;
    z->fired    = 0;
    return (bb_node_t){ pl_chunk_fn, z, 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t pl_box_choice_call(tree_t *goal, Term **env) {
    if (!goal || !goal->v.sval) return pl_box_fail();
    int arity = goal->n;
    char key[256];
    snprintf(key, sizeof key, "%s/%d", goal->v.sval, arity);
    tree_t *choice = pl_pred_table_lookup_global(key);
    if (!choice) return pl_box_fail();
    Term **caller_args = arity ? malloc(arity * sizeof(Term *)) : NULL;
    static int g_wildcard_slot = 100000;
    for (int i = 0; i < arity; i++) {
        tree_t *ch = goal->c[i];
        if (ch && ch->t == TT_VAR && (int)ch->v.ival == -1)
            caller_args[i] = term_new_var(g_wildcard_slot++);
        else
            caller_args[i] = pl_unified_term_from_expr(ch, env);
    }
    return pl_box_choice(choice, caller_args, arity);
}
