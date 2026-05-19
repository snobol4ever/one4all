#include "prolog_lower.h"
#include "prolog_atom.h"
#include "term.h"
#include "scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int functor;
    int arity;
} PredKey;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pred_key_eq(PredKey a, PredKey b) {
    return a.functor == b.functor && a.arity == b.arity;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static PredKey key_of_head(Term *head) {
    PredKey k = {-1, 0};
    if (!head) return k;
    head = term_deref(head);
    if (!head) return k;
    if (head->tag == TERM_ATOM) {
        k.functor = head->atom_id;
        k.arity   = 0;
    } else if (head->tag == TERM_COMPOUND) {
        k.functor = head->compound.functor;
        k.arity   = head->compound.arity;
    }
    return k;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *lower_term(Term *t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *lower_clause(PlClause *cl, PredKey key);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void assign_clause_anon_slots(PlClause *cl) {
    if (!cl) return;
    int next_slot = 0;
    for (int _vi = 0; _vi < cl->nvar; _vi++) {
        Term *v = cl->var_terms[_vi];
        if (v && v->tag == TERM_VAR && v->saved_slot < 0)
            v->saved_slot = next_slot++;
    }
    Term *stk[512]; int top = 0;
    #define PA_PUSH(t_) do { Term *_p = term_deref(t_); \
        if (_p && top < 512) stk[top++] = _p; } while(0)
    int next_anon = next_slot;
    #define PA_WALK_ASSIGN(root_) do { top = 0; PA_PUSH(root_); \
        while (top > 0) { Term *_c = stk[--top]; if (!_c) continue; \
            if (_c->tag == TERM_VAR && _c->saved_slot < 0) \
                _c->saved_slot = next_anon++; \
            if (_c->tag == TERM_COMPOUND) \
                for (int _i = 0; _i < _c->compound.arity; _i++) \
                    PA_PUSH(_c->compound.args[_i]); } } while(0)
    if (cl->head) PA_WALK_ASSIGN(cl->head);
    for (int i = 0; i < cl->nbody; i++) if (cl->body[i]) PA_WALK_ASSIGN(cl->body[i]);
    #undef PA_PUSH
    #undef PA_WALK_ASSIGN
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *pred_str(int functor, int arity) {
    const char *fn = prolog_atom_name(functor);
    if (!fn) fn = "?";
    char buf[256];
    snprintf(buf, sizeof buf, "%s/%d", fn, arity);
    return strdup(buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *lower_term(Term *t) {
    t = term_deref(t);
    if (!t) {
        tree_t *e = ast_node_new(TT_QLIT);
        e->v.sval = strdup("[]");
        return e;
    }
    switch (t->tag) {
        case TERM_ATOM: {
            if (t->atom_id == ATOM_CUT) return ast_node_new(TT_CUT);
            tree_t *e = ast_node_new(TT_FNC);
            const char *nm = prolog_atom_name(t->atom_id);
            e->v.sval = strdup(nm ? nm : "");
            return e;
        }
        case TERM_INT: {
            tree_t *e = ast_node_new(TT_ILIT);
            e->v.ival = t->ival;
            return e;
        }
        case TERM_FLOAT: {
            tree_t *e = ast_node_new(TT_FLIT);
            e->v.dval = t->fval;
            return e;
        }
        case TERM_VAR: {
            tree_t *e = ast_node_new(TT_VAR);
            int slot = t->saved_slot;
            char buf[32];
            if (slot < 0) {
                snprintf(buf, sizeof buf, "_anon");
            } else {
                snprintf(buf, sizeof buf, "_V%d", slot);
            }
            e->v.sval = strdup(buf);
            e->v.ival = slot;
            return e;
        }
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            int arity = t->compound.arity;
            int eq_id = prolog_atom_intern("=");
            if (t->compound.functor == eq_id && arity == 2) {
                tree_t *e = ast_node_new(TT_UNIFY);
                expr_add_child(e, lower_term(t->compound.args[0]));
                expr_add_child(e, lower_term(t->compound.args[1]));
                return e;
            }
            struct { const char *name; tree_e kind; } arith[] = {
                { "+", TT_ADD }, { "-", TT_SUB }, { "*", TT_MUL },
                { "/", TT_DIV }, { "//", TT_DIV }, { NULL, 0 }
            };
            if (arity == 2) {
                for (int i = 0; arith[i].name; i++) {
                    if (strcmp(fn, arith[i].name) == 0) {
                        tree_t *e = ast_node_new(arith[i].kind);
                        expr_add_child(e, lower_term(t->compound.args[0]));
                        expr_add_child(e, lower_term(t->compound.args[1]));
                        return e;
                    }
                }
            }
            int comma_id = prolog_atom_intern(",");
            if (t->compound.functor == comma_id && arity == 2) {
                tree_t *e = ast_node_new(TT_FNC);
                e->v.sval = strdup(",");
                Term *cur = t;
                while (cur && cur->tag == TERM_COMPOUND &&
                       cur->compound.functor == comma_id &&
                       cur->compound.arity == 2) {
                    expr_add_child(e, lower_term(cur->compound.args[0]));
                    cur = term_deref(cur->compound.args[1]);
                }
                if (cur) expr_add_child(e, lower_term(cur));
                return e;
            }
            int semi_id = prolog_atom_intern(";");
            if (t->compound.functor == semi_id && arity == 2) {
                tree_t *e = ast_node_new(TT_FNC);
                e->v.sval = strdup(";");
                Term *cur = t;
                while (cur && cur->tag == TERM_COMPOUND &&
                       cur->compound.functor == semi_id &&
                       cur->compound.arity == 2) {
                    expr_add_child(e, lower_term(cur->compound.args[0]));
                    cur = term_deref(cur->compound.args[1]);
                }
                if (cur) expr_add_child(e, lower_term(cur));
                return e;
            }
            int arrow_id = prolog_atom_intern("->");
            if (t->compound.functor == arrow_id && arity == 2) {
                tree_t *e = ast_node_new(TT_FNC);
                e->v.sval = strdup("->");
                expr_add_child(e, lower_term(t->compound.args[0]));
                Term *then_part = term_deref(t->compound.args[1]);
                while (then_part && then_part->tag == TERM_COMPOUND &&
                       then_part->compound.functor == comma_id &&
                       then_part->compound.arity == 2) {
                    expr_add_child(e, lower_term(then_part->compound.args[0]));
                    then_part = term_deref(then_part->compound.args[1]);
                }
                if (then_part) expr_add_child(e, lower_term(then_part));
                return e;
            }
            tree_t *e = ast_node_new(TT_FNC);
            e->v.sval = strdup(fn);
            for (int i = 0; i < arity; i++)
                expr_add_child(e, lower_term(t->compound.args[i]));
            return e;
        }
        case TERM_REF:
            return lower_term(t->ref);
        default: {
            tree_t *e = ast_node_new(TT_QLIT);
            e->v.sval = strdup("?");
            return e;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *lower_clause(PlClause *cl, PredKey key) {
    tree_t *ec = ast_node_new(TT_CLAUSE);
    ec->v.sval = pred_str(key.functor, key.arity);
    int next_slot = 0;
    for (int _vi = 0; _vi < cl->nvar; _vi++) {
        Term *v = cl->var_terms[_vi];
        if (v && v->tag == TERM_VAR && v->saved_slot < 0)
            v->saved_slot = next_slot++;
    }
    #define TERM_STACK_MAX 512
    Term *stk[TERM_STACK_MAX];
    int  stk_top = 0;
    #define PUSH_TERM(t_) do { \
        Term *_pt = term_deref(t_); \
        if (_pt && stk_top < TERM_STACK_MAX) stk[stk_top++] = _pt; \
    } while(0)
    int next_anon = next_slot;
    #define ASSIGN_ANON(root_) do { \
        stk_top = 0; \
        PUSH_TERM(root_); \
        while (stk_top > 0) { \
            Term *_cur = stk[--stk_top]; \
            if (!_cur) continue; \
            if (_cur->tag == TERM_VAR && _cur->saved_slot < 0) \
                _cur->saved_slot = next_anon++; \
            if (_cur->tag == TERM_COMPOUND) \
                for (int _ai = 0; _ai < _cur->compound.arity; _ai++) \
                    PUSH_TERM(_cur->compound.args[_ai]); \
        } \
    } while(0)
    if (cl->head) ASSIGN_ANON(cl->head);
    for (int i = 0; i < cl->nbody; i++)
        if (cl->body[i]) ASSIGN_ANON(cl->body[i]);
    int n_vars = next_anon;
    ec->v.dval = (double)key.arity;
    ec->v.ival = n_vars;
    if (cl->head) {
        Term *h = term_deref(cl->head);
        if (h && h->tag == TERM_COMPOUND)
            for (int i = 0; i < h->compound.arity; i++)
                expr_add_child(ec, lower_term(h->compound.args[i]));
    }
    for (int i = 0; i < cl->nbody; i++)
        expr_add_child(ec, lower_term(cl->body[i]));
    return ec;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-PL-6d — lower_clause_from_tree: consume cl->tr (tree_t from parser)
 * directly instead of converting Term* via lower_term.
 *
 * Includes the pre-lower slot-assignment pass (6e preview):
 * walk all TT_VAR nodes, assign sequential v.ival by name.
 * Anonymous vars ("_") each get a unique slot.
 */
/* PST-PL-6h — pre-lower helpers moved from prolog_parse.c.
 * Parser now emits raw TT_FNC(",", ...) and TT_FNC(";", ...) nodes.
 * Lower interprets structure; parser just reflects token order. */

/* Flatten TT_FNC(",") chain into flat TT_PROGRAM children (n-ary). */
static void pl_flatten_conj(tree_t *t, tree_t *prog) {
    if (!t) return;
    if (t->t == TT_FNC && t->v.sval && strcmp(t->v.sval, ",") == 0) {
        for (int i = 0; i < t->n; i++)
            pl_flatten_conj(t->c[i], prog);
        return;
    }
    ast_push(prog, t);
}

/* Detect ;(->(Cond,Then),Else) and collapse to TT_IF(cond,then,else). */
static tree_t *pl_maybe_ifthenelse(tree_t *semi_node) {
    if (semi_node->n != 2) return semi_node;
    tree_t *left  = semi_node->c[0];
    tree_t *right = semi_node->c[1];
    if (!left || left->t != TT_FNC || !left->v.sval) return semi_node;
    if (strcmp(left->v.sval, "->") != 0 || left->n < 2) return semi_node;
    tree_t *then_prog = ast_node_new(TT_PROGRAM);
    pl_flatten_conj(left->c[1], then_prog);
    tree_t *else_prog = ast_node_new(TT_PROGRAM);
    pl_flatten_conj(right, else_prog);
    tree_t *iff = ast_node_new(TT_IF);
    ast_push(iff, left->c[0]);
    ast_push(iff, then_prog->n == 1 ? then_prog->c[0] : then_prog);
    ast_push(iff, else_prog->n == 1 ? else_prog->c[0] : else_prog);
    return iff;
}

/* Build TT_CLAUSE[head_or_NUL, TT_PROGRAM(body)] from raw parser tree.
 * Parser emits raw body (TT_FNC(",") chain or single goal); lower wraps here. */
static tree_t *pl_make_clause(tree_t *head_tr, tree_t *body_tr) {
    tree_t *cl = ast_node_new(TT_CLAUSE);
    if (head_tr) {
        ast_push(cl, head_tr);
    } else {
        ast_push(cl, ast_node_new(TT_NUL));
    }
    tree_t *prog = ast_node_new(TT_PROGRAM);
    if (body_tr) pl_flatten_conj(body_tr, prog);
    ast_push(cl, prog);
    return cl;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define TR_SLOT_MAX 256
typedef struct { const char *name; int slot; } TRSlot;
typedef struct { TRSlot e[TR_SLOT_MAX]; int n; int next; } TRSlotMap;

static void trslot_reset(TRSlotMap *m) { m->n = 0; m->next = 0; }

static int trslot_get(TRSlotMap *m, const char *name) {
    if (!name || strcmp(name, "_") == 0)
        return m->next++;           /* anonymous: always fresh */
    for (int i = 0; i < m->n; i++)
        if (strcmp(m->e[i].name, name) == 0)
            return m->e[i].slot;
    if (m->n >= TR_SLOT_MAX) return m->next++;
    int s = m->next++;
    m->e[m->n].name = name;
    m->e[m->n].slot = s;
    m->n++;
    return s;
}

/* Walk tree_t and assign v.ival slot to every TT_VAR node. */
static void tr_assign_slots(tree_t *t, TRSlotMap *m) {
    if (!t) return;
    if (t->t == TT_VAR) {
        t->v.ival = trslot_get(m, t->v.sval);
        return;
    }
    for (int i = 0; i < t->n; i++)
        tr_assign_slots(t->c[i], m);
}

/* Extract functor name and arity from a TT_CLAUSE head child (c[0]). */
static void tr_head_key(tree_t *head, const char **fn_out, int *arity_out) {
    *fn_out    = NULL;
    *arity_out = 0;
    if (!head) return;
    if (head->t == TT_FNC) {
        *fn_out    = head->v.sval;
        *arity_out = head->n;
    } else if (head->t == TT_QLIT) {
        *fn_out    = head->v.sval;
        *arity_out = 0;
    }
}

/* Build the lowered TT_CLAUSE from the parser's tree_t.
 * Mirrors lower_clause() output structure exactly. */
static tree_t *lower_clause_from_tree(tree_t *tr, PredKey key) {
    /* Slot assignment pre-pass */
    TRSlotMap sm; trslot_reset(&sm);
    tr_assign_slots(tr, &sm);
    int n_vars = sm.next;

    tree_t *ec = ast_node_new(TT_CLAUSE);
    ec->v.sval = pred_str(key.functor, key.arity);
    ec->v.dval = (double)key.arity;
    ec->v.ival = n_vars;

    /* c[0] of tr = head; c[1] = raw body (TT_FNC(",") chain or single goal)
     * PST-PL-6h: parser no longer pre-wraps body in TT_PROGRAM; lower does it. */
    tree_t *head = (tr->n > 0) ? tr->c[0] : NULL;
    tree_t *raw_body = (tr->n > 1) ? tr->c[1] : NULL;

    /* Add head arguments as direct children of ec (not the head node itself) */
    if (head && head->t == TT_FNC) {
        for (int i = 0; i < head->n; i++)
            expr_add_child(ec, head->c[i]);
    }
    /* else: 0-arity head (TT_QLIT) — no args to add */

    /* PST-PL-6h: build body TT_PROGRAM in lower, not in parser.
     * If the parser already wrapped in TT_PROGRAM (pre-6h clauses in-flight),
     * fall back gracefully. */
    tree_t *body_prog;
    if (raw_body && raw_body->t == TT_PROGRAM) {
        /* Compat path: parser pre-built TT_PROGRAM (should not occur post-6h) */
        body_prog = raw_body;
    } else {
        body_prog = ast_node_new(TT_PROGRAM);
        if (raw_body) pl_flatten_conj(raw_body, body_prog);
    }

    /* Walk flattened goals: apply pl_maybe_ifthenelse to any TT_FNC(";") */
    for (int i = 0; i < body_prog->n; i++) {
        tree_t *g = body_prog->c[i];
        if (g && g->t == TT_FNC && g->v.sval && strcmp(g->v.sval, ";") == 0)
            body_prog->c[i] = pl_maybe_ifthenelse(g);
    }

    /* Add body goals to emitted clause */
    for (int i = 0; i < body_prog->n; i++)
        expr_add_child(ec, body_prog->c[i]);

    return ec;
}

/* Derive PredKey from tree_t head (c[0] of TT_CLAUSE). */
static PredKey key_of_head_tree(tree_t *head) {
    PredKey k = {-1, 0};
    if (!head) return k;
    const char *fn = NULL;
    int arity = 0;
    tr_head_key(head, &fn, &arity);
    if (!fn) return k;
    k.functor = prolog_atom_intern(fn);
    k.arity   = arity;
    return k;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *prolog_lower(PlProgram *pl_prog) {
    CODE_t *prog = calloc(1, sizeof(CODE_t));
    #define PL_MAX_CLAUSES 2048
    char plunit_suite[PL_MAX_CLAUSES][64];
    {
        char cur_suite[64] = "";
        int ci = 0;
        for (PlClause *cl = pl_prog->head; cl && ci < PL_MAX_CLAUSES; cl = cl->next, ci++) {
            plunit_suite[ci][0] = '\0';
            if (!cl->head && cl->tr && cl->tr->n > 0 && cl->tr->c[0] && cl->tr->c[0]->t == TT_NUL) {
                /* Directive: read begin_tests/end_tests from tree_t */
                tree_t *bp = cl->tr->c[1];
                /* PST-PL-6h compat: body may be raw (TT_FNC etc.) or pre-wrapped TT_PROGRAM */
                tree_t *d = NULL;
                if (bp && bp->t == TT_PROGRAM && bp->n > 0) {
                    d = bp->c[0];
                } else if (bp) {
                    /* raw single goal or TT_FNC(",") chain — first goal is the directive */
                    d = (bp->t == TT_FNC && bp->v.sval && strcmp(bp->v.sval, ",") == 0 && bp->n > 0)
                        ? bp->c[0] : bp;
                }
                if (d && d->t == TT_FNC && d->v.sval && d->n >= 1) {
                        if (strcmp(d->v.sval, "begin_tests") == 0) {
                            tree_t *a = d->c[0];
                            const char *sn = NULL;
                            if (a && a->t == TT_QLIT) sn = a->v.sval;
                            if (a && a->t == TT_FNC)  sn = a->v.sval;
                            if (sn) strncpy(cur_suite, sn, 63);
                        } else if (strcmp(d->v.sval, "end_tests") == 0) {
                            cur_suite[0] = '\0';
                        }
                }
            } else if (cl->head && cur_suite[0]) {
                strncpy(plunit_suite[ci], cur_suite, 63);
            }
        }
    }
    #define MAX_PREDS 512
    PredKey  keys[MAX_PREDS];
    tree_t  *choices[MAX_PREDS];
    int      nkeys = 0;
    int      clause_idx = 0;
    for (PlClause *cl = pl_prog->head; cl; cl = cl->next, clause_idx++) {
        /* PST-PL-6f: non-DCG rules have cl->head==NULL; use tree to detect directives.
         * A clause is a directive if: cl->head==NULL and (cl->tr==NULL or tr->c[0]->t==TT_NUL).
         * A clause is a rule/fact if: cl->tr != NULL and tr->c[0]->t != TT_NUL. */
        int is_dcg  = (cl->head != NULL && cl->tr == NULL);  /* DCG: Term* head, no tree */
        int is_rule = (cl->tr != NULL && cl->tr->n > 0 &&
                       cl->tr->c[0] && cl->tr->c[0]->t != TT_NUL);
        if (!is_rule && !is_dcg) continue;   /* directive or empty */
        /* PST-PL-6f: use tree_t path for non-DCG; Term* for DCG (cl->tr==NULL). */
        PredKey k;
        if (cl->tr) {
            tree_t *tr_head = cl->tr->c[0];
            k = key_of_head_tree(tr_head);
        } else {
            k = key_of_head(cl->head);
        }
        if (k.functor < 0) continue;
        if (clause_idx < PL_MAX_CLAUSES && plunit_suite[clause_idx][0] != '\0') {
            const char *fn = prolog_atom_name(k.functor);
            if (fn && strcmp(fn, "test") == 0 && (k.arity == 1 || k.arity == 2)) {
                assign_clause_anon_slots(cl);
                Term *hd = term_deref(cl->head);
                Term *name_term = (hd->tag == TERM_COMPOUND) ? term_deref(hd->compound.args[0]) : hd;
                Term *opts_term = (k.arity == 2) ? term_deref(hd->compound.args[1]) : NULL;
                Term *goal_term = NULL;
                if (cl->nbody == 0) {
                    goal_term = term_new_atom(prolog_atom_intern("true"));
                } else if (cl->nbody == 1) {
                    goal_term = cl->body[0];
                } else {
                    int comma_id = prolog_atom_intern(",");
                    goal_term = cl->body[cl->nbody - 1];
                    for (int bi = cl->nbody - 2; bi >= 0; bi--) {
                        Term *a2[2] = { cl->body[bi], goal_term };
                        goal_term = term_new_compound(comma_id, 2, a2);
                    }
                }
                int suite_id   = prolog_atom_intern(plunit_suite[clause_idx]);
                int pjtest_id  = prolog_atom_intern("pj_test");
                int assertz_id = prolog_atom_intern("assertz");
                Term *opts_arg = opts_term ? opts_term : term_new_atom(prolog_atom_intern("[]"));
                Term *pjargs[4] = { term_new_atom(suite_id), name_term, opts_arg, goal_term };
                Term *pjtest    = term_new_compound(pjtest_id, 4, pjargs);
                Term *azargs[1] = { pjtest };
                Term *azterm    = term_new_compound(assertz_id, 1, azargs);
                STMT_t *rs = stmt_new();
                rs->subject = lower_term(azterm);
                rs->lineno  = cl->lineno;
                rs->lang    = LANG_PL;
                if (!prog->head) prog->head = rs;
                else             prog->tail->next = rs;
                prog->tail = rs;
                prog->nstmts++;
            }
        }
        int found = -1;
        for (int i = 0; i < nkeys; i++)
            if (pred_key_eq(keys[i], k)) { found = i; break; }
        if (found < 0) {
            if (nkeys >= MAX_PREDS) {
                fprintf(stderr, "prolog_lower: too many predicates\n");
                continue;
            }
            keys[nkeys] = k;
            choices[nkeys] = ast_node_new(TT_CHOICE);
            choices[nkeys]->v.sval = pred_str(k.functor, k.arity);
            found = nkeys++;
        }
        tree_t *ec = cl->tr ? lower_clause_from_tree(cl->tr, k)
                            : lower_clause(cl, k);
        expr_add_child(choices[found], ec);
    }
    for (PlClause *cl = pl_prog->head; cl; cl = cl->next) {
        if (cl->head) continue;   /* skip DCG rules (have Term* head) */
        /* PST-PL-6f: non-DCG rules have cl->head==NULL but cl->tr->c[0]->t != TT_NUL.
         * Only process as directive when head is TT_NUL. */
        if (cl->tr && cl->tr->n > 0 && cl->tr->c[0] && cl->tr->c[0]->t != TT_NUL) continue;
        /* Directive: cl->tr is always set post-6f (non-DCG).
         * Extract first body goal from the tree. */
        if (!cl->tr || cl->tr->n < 2) continue;
        tree_t *raw_body = cl->tr->c[1];
        /* PST-PL-6h: body may be raw (TT_FNC chain) or pre-built TT_PROGRAM */
        tree_t *goal_tr = NULL;
        if (raw_body && raw_body->t == TT_PROGRAM && raw_body->n > 0) {
            goal_tr = raw_body->c[0];
        } else if (raw_body) {
            /* Raw: single goal or TT_FNC(",") — first goal is what we want */
            if (raw_body->t == TT_FNC && raw_body->v.sval &&
                strcmp(raw_body->v.sval, ",") == 0 && raw_body->n > 0)
                goal_tr = raw_body->c[0];
            else
                goal_tr = raw_body;
        }
        if (!goal_tr) continue;
        /* Detect :- export(...) directives from tree_t */
        int is_export = 0;
        if (goal_tr && goal_tr->t == TT_FNC && goal_tr->v.sval &&
            strcmp(goal_tr->v.sval, "export") == 0 && goal_tr->n == 1) {
            is_export = 1;
            tree_t *arg = goal_tr->c[0];
            const char *ename = NULL;
            if (arg && arg->t == TT_FNC && arg->v.sval &&
                strcmp(arg->v.sval, "/") == 0 && arg->n >= 1 &&
                arg->c[0] && arg->c[0]->t == TT_QLIT) {
                ename = arg->c[0]->v.sval;
            } else if (arg && arg->t == TT_QLIT) {
                ename = arg->v.sval;
            }
            if (ename) {
                ExportEntry *e = calloc(1, sizeof *e);
                e->name = strdup(ename);
                e->next = prog->exports;
                prog->exports = e;
            }
        }
        if (!is_export) {
            STMT_t *s = stmt_new();
            s->subject = goal_tr;
            s->lineno  = cl->lineno;
            s->lang    = LANG_PL;
            if (!prog->head) prog->head = s;
            else             prog->tail->next = s;
            prog->tail = s;
            prog->nstmts++;
        }
    }
    for (int i = 0; i < nkeys; i++) {
        STMT_t *s = stmt_new();
        s->subject = choices[i];
        s->lineno  = 0;
        s->lang    = LANG_PL;
        if (!prog->head) prog->head = s;
        else             prog->tail->next = s;
        prog->tail = s;
        prog->nstmts++;
    }
    return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *pl_assert_term(Term *t, int *functor_out, int *arity_out) {
    if (!t) return NULL;
    t = term_deref(t);
    if (!t) return NULL;
    Term *head = NULL;
    Term *body = NULL;
    if (t->tag == TERM_COMPOUND && t->compound.arity == 2) {
        const char *fn = prolog_atom_name(t->compound.functor);
        if (fn && strcmp(fn, ":-") == 0) {
            head = term_deref(t->compound.args[0]);
            body = term_deref(t->compound.args[1]);
        }
    }
    if (!head) head = t;
    PredKey k = key_of_head(head);
    if (k.functor < 0) return NULL;
    if (functor_out) *functor_out = k.functor;
    if (arity_out)   *arity_out   = k.arity;
    PlClause cl_buf;
    memset(&cl_buf, 0, sizeof cl_buf);
    cl_buf.head   = head;
    cl_buf.lineno = 0;
    Term *body_goals[256];
    int nbody = 0;
    if (body) {
        Term *cur = body;
        while (cur && cur->tag == TERM_COMPOUND && cur->compound.arity == 2) {
            const char *cfn = prolog_atom_name(cur->compound.functor);
            if (!cfn || strcmp(cfn, ",") != 0) break;
            if (nbody < 256) body_goals[nbody++] = term_deref(cur->compound.args[0]);
            cur = term_deref(cur->compound.args[1]);
        }
        if (nbody < 256) body_goals[nbody++] = cur;
    }
    cl_buf.body  = (nbody > 0) ? body_goals : NULL;
    cl_buf.nbody = nbody;
    return lower_clause(&cl_buf, k);
}
