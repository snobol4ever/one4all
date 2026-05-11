/*
 * prolog_lower.c — Prolog ClauseAST -> scrip-cc IR lowering
 *
 * Takes a PlProgram (list of PlClause) and produces a CODE_t* whose
 * STMT_t nodes carry the Prolog IR node kinds added to AST_e in scrip-cc.h.
 *
 * Pipeline:
 *   1. Group clauses by functor/arity key -> one AST_CHOICE per predicate
 *   2. For each PlClause -> one AST_CLAUSE child of the AST_CHOICE
 *   3. Lower each Term* in head args and body goals -> AST_t*
 *   4. Assign variable slots per clause (VarScope reused from parser)
 *   5. Emit AST_TRAIL_MARK / AST_TRAIL_UNWIND sentinels around each clause
 *
 * Term -> AST_t lowering:
 *   TT_ATOM     -> AST_QLIT  (sval = atom name)
 *   TT_INT      -> AST_ILIT  (ival = value)
 *   TT_FLOAT    -> AST_FLIT  (dval = value)
 *   TT_VAR      -> AST_VAR  (sval = "_V<slot>", ival = slot)
 *   TT_COMPOUND -> AST_FNC   (sval = functor name, children = lowered args)
 *                  special: =/2  -> AST_UNIFY
 *                           is/2 -> AST_FNC("is") with arithmetic rhs
 *                           !/0  -> AST_CUT
 *                           ,/2  -> flattened into body (done in parser)
 */

#include "prolog_lower.h"
#include "prolog_atom.h"
#include "term.h"
#include "scrip_cc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Functor/arity key for grouping clauses
 * ======================================================================= */
typedef struct {
    int functor;   /* atom_id */
    int arity;
} PredKey;

static int pred_key_eq(PredKey a, PredKey b) {
    return a.functor == b.functor && a.arity == b.arity;
}

/* Extract predicate key from a clause head Term* */
static PredKey key_of_head(Term *head) {
    PredKey k = {-1, 0};
    if (!head) return k;
    head = term_deref(head);
    if (!head) return k;
    if (head->tag == TT_ATOM) {
        k.functor = head->atom_id;
        k.arity   = 0;
    } else if (head->tag == TT_COMPOUND) {
        k.functor = head->compound.functor;
        k.arity   = head->compound.arity;
    }
    return k;
}

/* =========================================================================
 * Term -> AST_t lowering
 * ======================================================================= */

static AST_t *lower_term(Term *t);
static AST_t *lower_clause(PlClause *cl, PredKey key);

/* Walk the Term graph rooted at the head + each body goal of a clause.
 * Pass 1: find max named-var slot.  Pass 2: assign fresh distinct slots to
 * anonymous TT_VARs (saved_slot == -1).  Mutation is in place on the Term;
 * idempotent — vars whose slot is already assigned are left alone.
 * Used both by lower_clause and by the plunit prescan, which builds an
 * assertz term whose Goal arg points back at the clause body Terms; the
 * prescan must run this BEFORE lowering the assertz term so anon vars in
 * the test body emit distinct AST_VAR slots, not all aliased to slot 0. */
static void assign_clause_anon_slots(PlClause *cl) {
    if (!cl) return;
    Term *stk[512]; int top = 0;
    #define PA_PUSH(t_) do { Term *_p = term_deref(t_); \
        if (_p && top < 512) stk[top++] = _p; } while(0)
    int max_slot = -1;
    /* Pass 1: max named slot */
    #define PA_WALK_MAX(root_) do { top = 0; PA_PUSH(root_); \
        while (top > 0) { Term *_c = stk[--top]; if (!_c) continue; \
            if (_c->tag == TT_VAR && _c->saved_slot > max_slot) \
                max_slot = _c->saved_slot; \
            if (_c->tag == TT_COMPOUND) \
                for (int _i = 0; _i < _c->compound.arity; _i++) \
                    PA_PUSH(_c->compound.args[_i]); } } while(0)
    if (cl->head) PA_WALK_MAX(cl->head);
    for (int i = 0; i < cl->nbody; i++) if (cl->body[i]) PA_WALK_MAX(cl->body[i]);
    /* Pass 2: assign fresh slots to anon vars */
    int next_anon = max_slot + 1;
    #define PA_WALK_ASSIGN(root_) do { top = 0; PA_PUSH(root_); \
        while (top > 0) { Term *_c = stk[--top]; if (!_c) continue; \
            if (_c->tag == TT_VAR && _c->saved_slot < 0) \
                _c->saved_slot = next_anon++; \
            if (_c->tag == TT_COMPOUND) \
                for (int _i = 0; _i < _c->compound.arity; _i++) \
                    PA_PUSH(_c->compound.args[_i]); } } while(0)
    if (cl->head) PA_WALK_ASSIGN(cl->head);
    for (int i = 0; i < cl->nbody; i++) if (cl->body[i]) PA_WALK_ASSIGN(cl->body[i]);
    #undef PA_PUSH
    #undef PA_WALK_MAX
    #undef PA_WALK_ASSIGN
}

/* Build functor/arity string like "foo/2" for AST_CHOICE sval */
static char *pred_str(int functor, int arity) {
    const char *fn = prolog_atom_name(functor);
    if (!fn) fn = "?";
    char buf[256];
    snprintf(buf, sizeof buf, "%s/%d", fn, arity);
    return strdup(buf);
}

static AST_t *lower_term(Term *t) {
    t = term_deref(t);
    if (!t) {
        AST_t *e = expr_new(AST_QLIT);
        e->v.sval = strdup("[]");
        return e;
    }

    switch (t->tag) {
        case TT_ATOM: {
            /* ! -> AST_CUT */
            if (t->atom_id == ATOM_CUT) return expr_new(AST_CUT);
            /* Atom in a goal position -> AST_FNC/0 (nl, true, fail, halt, etc.) */
            AST_t *e = expr_new(AST_FNC);
            const char *nm = prolog_atom_name(t->atom_id);
            e->v.sval = strdup(nm ? nm : "");
            return e;
        }
        case TT_INT: {
            AST_t *e = expr_new(AST_ILIT);
            e->v.ival = t->v.ival;
            return e;
        }
        case TT_FLOAT: {
            AST_t *e = expr_new(AST_FLIT);
            e->v.dval = t->fval;
            return e;
        }
        case TT_VAR: {
            AST_t *e = expr_new(AST_VAR);
            int slot = t->saved_slot;  /* -1 = anonymous wildcard */
            char buf[32];
            if (slot < 0) {
                snprintf(buf, sizeof buf, "_anon");
            } else {
                snprintf(buf, sizeof buf, "_V%d", slot);
            }
            e->v.sval = strdup(buf);
            e->v.ival = slot;  /* -1 for wildcard, >=0 for real var */
            return e;
        }
        case TT_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            int arity = t->compound.arity;

            /* =/2 -> AST_UNIFY */
            int eq_id = prolog_atom_intern("=");
            if (t->compound.functor == eq_id && arity == 2) {
                AST_t *e = expr_new(AST_UNIFY);
                expr_add_child(e, lower_term(t->compound.args[0]));
                expr_add_child(e, lower_term(t->compound.args[1]));
                return e;
            }

            /* Arithmetic operators within is/2 rhs */
            struct { const char *name; AST_e kind; } arith[] = {
                { "+", AST_ADD }, { "-", AST_SUB }, { "*", AST_MUL },
                { "/", AST_DIV }, { "//", AST_DIV }, { NULL, 0 }
            };
            if (arity == 2) {
                for (int i = 0; arith[i].name; i++) {
                    if (strcmp(fn, arith[i].name) == 0) {
                        AST_t *e = expr_new(arith[i].t);
                        expr_add_child(e, lower_term(t->compound.args[0]));
                        expr_add_child(e, lower_term(t->compound.args[1]));
                        return e;
                    }
                }
            }

            /* ,/2 — conjunction: flatten right-spine into n-ary AST_FNC(",")
             * e.g. (A,(B,(C,D))) -> AST_FNC(",") [A, B, C, D]  */
            int comma_id = prolog_atom_intern(",");
            if (t->compound.functor == comma_id && arity == 2) {
                AST_t *e = expr_new(AST_FNC);
                e->v.sval = strdup(",");
                /* walk the right spine collecting all conjuncts */
                Term *cur = t;
                while (cur && cur->tag == TT_COMPOUND &&
                       cur->compound.functor == comma_id &&
                       cur->compound.arity == 2) {
                    expr_add_child(e, lower_term(cur->compound.args[0]));
                    cur = term_deref(cur->compound.args[1]);
                }
                if (cur) expr_add_child(e, lower_term(cur));
                return e;
            }

            /* ;/2 — disjunction: flatten right-spine into n-ary AST_FNC(";")
             * e.g. (A;(B;C)) -> AST_FNC(";") [A, B, C]  */
            int semi_id = prolog_atom_intern(";");
            if (t->compound.functor == semi_id && arity == 2) {
                AST_t *e = expr_new(AST_FNC);
                e->v.sval = strdup(";");
                Term *cur = t;
                while (cur && cur->tag == TT_COMPOUND &&
                       cur->compound.functor == semi_id &&
                       cur->compound.arity == 2) {
                    expr_add_child(e, lower_term(cur->compound.args[0]));
                    cur = term_deref(cur->compound.args[1]);
                }
                if (cur) expr_add_child(e, lower_term(cur));
                return e;
            }

            /* ->/2 — if-then: flatten Then-part into n-ary children.
             * AST_FNC("->") layout: children[0] = Cond, children[1..] = Then goals.
             * (Then conjunction flattened, same as AST_CLAUSE body goals.)
             * Every then-step is visible at top level for the emitter. */
            int arrow_id = prolog_atom_intern("->");
            if (t->compound.functor == arrow_id && arity == 2) {
                AST_t *e = expr_new(AST_FNC);
                e->v.sval = strdup("->");
                /* children[0] = Cond */
                expr_add_child(e, lower_term(t->compound.args[0]));
                /* children[1..] = Then goals (flatten right-spine conjunction) */
                Term *then_part = term_deref(t->compound.args[1]);
                while (then_part && then_part->tag == TT_COMPOUND &&
                       then_part->compound.functor == comma_id &&
                       then_part->compound.arity == 2) {
                    expr_add_child(e, lower_term(then_part->compound.args[0]));
                    then_part = term_deref(then_part->compound.args[1]);
                }
                if (then_part) expr_add_child(e, lower_term(then_part));
                return e;
            }

            /* General compound / goal -> AST_FNC */
            AST_t *e = expr_new(AST_FNC);
            e->v.sval = strdup(fn);
            for (int i = 0; i < arity; i++)
                expr_add_child(e, lower_term(t->compound.args[i]));
            return e;
        }
        case TT_REF:
            return lower_term(t->ref);
        default: {
            AST_t *e = expr_new(AST_QLIT);
            e->v.sval = strdup("?");
            return e;
        }
    }
}

/* =========================================================================
 * Lower one PlClause -> AST_CLAUSE AST_t
 *
 * AST_CLAUSE layout:
 *   sval   = "functor/arity"
 *   ival   = n_vars (EnvLayout.n_vars)
 *   dval   = (double)n_args
 *   children[0..n_args-1]  = lowered head argument terms
 *   children[n_args..]     = lowered body goals
 *   (AST_TRAIL_MARK and AST_TRAIL_UNWIND are added as sentinel children
 *    around the head-unify region by the emitter; lower just records
 *    ival = trail_mark_slot = n_vars)
 * ======================================================================= */
static AST_t *lower_clause(PlClause *cl, PredKey key) {
    AST_t *ec = expr_new(AST_CLAUSE);
    ec->v.sval = pred_str(key.functor, key.arity);

    /* Count distinct variable slots via full recursive Term walk */
    int max_slot = -1;

    /* Recursive helper using explicit stack to avoid C VLA issues */
    #define TERM_STACK_MAX 512
    Term *stk[TERM_STACK_MAX];
    int  stk_top = 0;

    #define PUSH_TERM(t_) do { \
        Term *_pt = term_deref(t_); \
        if (_pt && stk_top < TERM_STACK_MAX) stk[stk_top++] = _pt; \
    } while(0)

    #define WALK_ALL(root_) do { \
        stk_top = 0; \
        PUSH_TERM(root_); \
        while (stk_top > 0) { \
            Term *_cur = stk[--stk_top]; \
            if (!_cur) continue; \
            if (_cur->tag == TT_VAR && _cur->saved_slot > max_slot) \
                max_slot = _cur->saved_slot; \
            if (_cur->tag == TT_COMPOUND) \
                for (int _wi = 0; _wi < _cur->compound.arity; _wi++) \
                    PUSH_TERM(_cur->compound.args[_wi]); \
        } \
    } while(0)

    /* Walk head */
    if (cl->head) WALK_ALL(cl->head);

    /* Walk each body goal */
    for (int i = 0; i < cl->nbody; i++)
        if (cl->body[i]) WALK_ALL(cl->body[i]);

    /* Pass 2: assign fresh slots to anonymous TT_VARs (saved_slot == -1).
     * Each distinct anon Term* gets its own slot starting at max_slot+1.
     * Without this, lower_term sees saved_slot==-1 for every "_" and emits
     * a single shared AST_VAR slot — which incorrectly aliases distinct anon
     * vars in the same clause (e.g. assertz-synthesized plunit test bodies
     * containing multiple "_" placeholders). Idempotent: re-running on a
     * Term graph whose anon vars already got slots leaves them unchanged.
     * Walks via the same explicit stack pattern as Pass 1; mutation is on
     * the Term directly so lower_term picks it up via the existing
     * t->saved_slot read at line ~109. The plunit prescan calls this
     * helper directly before building its synthesized assertz term, so
     * the test body sees its anons get distinct slots BEFORE lower_term
     * walks them. The named-var max_slot is recomputed here against the
     * full clause regardless. */
    int next_anon = max_slot + 1;
    #define ASSIGN_ANON(root_) do { \
        stk_top = 0; \
        PUSH_TERM(root_); \
        while (stk_top > 0) { \
            Term *_cur = stk[--stk_top]; \
            if (!_cur) continue; \
            if (_cur->tag == TT_VAR && _cur->saved_slot < 0) \
                _cur->saved_slot = next_anon++; \
            if (_cur->tag == TT_COMPOUND) \
                for (int _ai = 0; _ai < _cur->compound.arity; _ai++) \
                    PUSH_TERM(_cur->compound.args[_ai]); \
        } \
    } while(0)

    if (cl->head) ASSIGN_ANON(cl->head);
    for (int i = 0; i < cl->nbody; i++)
        if (cl->body[i]) ASSIGN_ANON(cl->body[i]);

    int n_vars = next_anon;
    ec->v.ival = n_vars;              /* EnvLayout.n_vars */
    ec->v.dval = (double)key.arity;  /* EnvLayout.n_args */

    /* Add head argument nodes */
    if (cl->head) {
        Term *h = term_deref(cl->head);
        if (h && h->tag == TT_COMPOUND) {
            for (int i = 0; i < h->compound.arity; i++)
                expr_add_child(ec, lower_term(h->compound.args[i]));
        }
        /* arity 0: head is just an atom, no arg children */
    }

    /* Add body goal nodes */
    for (int i = 0; i < cl->nbody; i++)
        expr_add_child(ec, lower_term(cl->body[i]));

    return ec;
}

/* =========================================================================
 * prolog_lower — main entry point
 * ======================================================================= */
CODE_t *prolog_lower(PlProgram *pl_prog) {
    CODE_t *prog = calloc(1, sizeof(CODE_t));

    /* ---- Pass 0: plunit prescan — track begin_tests/end_tests directives
     * and record which suite each test/1,2 clause belongs to.
     * Builds a parallel suite[] array indexed by clause position. */
    #define PL_MAX_CLAUSES 2048
    char plunit_suite[PL_MAX_CLAUSES][64];  /* suite name per clause, "" if none */
    {
        char cur_suite[64] = "";
        int ci = 0;
        for (PlClause *cl = pl_prog->head; cl && ci < PL_MAX_CLAUSES; cl = cl->next, ci++) {
            plunit_suite[ci][0] = '\0';
            if (!cl->head && cl->nbody > 0) {
                /* directive — check for begin_tests/end_tests */
                Term *d = term_deref(cl->body[0]);
                if (d && d->tag == TT_COMPOUND && d->compound.arity >= 1) {
                    const char *fn = prolog_atom_name(d->compound.functor);
                    if (fn && strcmp(fn, "begin_tests") == 0) {
                        Term *a = term_deref(d->compound.args[0]);
                        const char *sn = NULL;
                        if (a && a->tag == TT_ATOM)     sn = prolog_atom_name(a->atom_id);
                        if (a && a->tag == TT_COMPOUND) sn = prolog_atom_name(a->compound.functor);
                        if (sn) strncpy(cur_suite, sn, 63);
                    } else if (fn && strcmp(fn, "end_tests") == 0) {
                        cur_suite[0] = '\0';
                    }
                }
            } else if (cl->head && cur_suite[0]) {
                /* clause inside a begin_tests block — record suite */
                strncpy(plunit_suite[ci], cur_suite, 63);
            }
        }
    }

    /* ---- Pass 1: collect all predicate keys in order of first appearance */
    #define MAX_PREDS 512
    PredKey  keys[MAX_PREDS];
    AST_t  *choices[MAX_PREDS];   /* one AST_CHOICE per predicate */
    int      nkeys = 0;
    int      clause_idx = 0;  /* tracks position for plunit_suite[] lookup */

    for (PlClause *cl = pl_prog->head; cl; cl = cl->next, clause_idx++) {
        if (!cl->head) continue; /* skip directives for now */
        PredKey k = key_of_head(cl->head);
        if (k.functor < 0) continue;

        /* plunit: if this clause is test/1 or test/2 inside a begin_tests block,
         * emit a synthetic assertz(pj_test(Suite,Name,Opts,Goal)) directive stmt
         * so the runtime can enumerate tests without clause/2. */
        if (clause_idx < PL_MAX_CLAUSES && plunit_suite[clause_idx][0] != '\0') {
            const char *fn = prolog_atom_name(k.functor);
            if (fn && strcmp(fn, "test") == 0 && (k.arity == 1 || k.arity == 2)) {
                /* Assign distinct slots to anon vars in the original clause's
                 * head/body BEFORE we splice the body Terms into the synthetic
                 * assertz term — without this, multiple "_" in the test body
                 * (e.g. test_term `term_singletons(X+X+_Y,[_,_])`) would all
                 * lower to the same slot and alias at runtime, causing the
                 * `(expected fail, succeeded)` failure mode. Idempotent;
                 * lower_clause(cl,k) below will see the slots already set
                 * and leave them. */
                assign_clause_anon_slots(cl);
                /* Extract Name and Opts from head */
                Term *hd = term_deref(cl->head);
                Term *name_term = (hd->tag == TT_COMPOUND) ? term_deref(hd->compound.args[0]) : hd;
                Term *opts_term = (k.arity == 2) ? term_deref(hd->compound.args[1]) : NULL;
                /* Build Goal term = conjunction of body goals */
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
                /* Build assertz(pj_test(Suite, Name, Opts, Goal)) term */
                int suite_id   = prolog_atom_intern(plunit_suite[clause_idx]);
                int pjtest_id  = prolog_atom_intern("pj_test");
                int assertz_id = prolog_atom_intern("assertz");
                Term *opts_arg = opts_term ? opts_term : term_new_atom(prolog_atom_intern("[]"));
                Term *pjargs[4] = { term_new_atom(suite_id), name_term, opts_arg, goal_term };
                Term *pjtest    = term_new_compound(pjtest_id, 4, pjargs);
                Term *azargs[1] = { pjtest };
                Term *azterm    = term_new_compound(assertz_id, 1, azargs);
                /* Emit as a directive stmt */
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

        /* Find or create entry */
        int found = -1;
        for (int i = 0; i < nkeys; i++)
            if (pred_key_eq(keys[i], k)) { found = i; break; }

        if (found < 0) {
            if (nkeys >= MAX_PREDS) {
                fprintf(stderr, "prolog_lower: too many predicates\n");
                continue;
            }
            keys[nkeys] = k;
            choices[nkeys] = expr_new(AST_CHOICE);
            choices[nkeys]->v.sval = pred_str(k.functor, k.arity);
            found = nkeys++;
        }

        /* Lower this clause and append to the choice */
        AST_t *ec = lower_clause(cl, k);
        expr_add_child(choices[found], ec);
    }

    /* ---- Pass 2: handle directives.
     * :- export(name/arity) or :- export(name) → prog->exports
     * All other directives → plain AST_FNC STMT_t nodes. */
    for (PlClause *cl = pl_prog->head; cl; cl = cl->next) {
        if (!cl->head && cl->nbody > 0) {
            Term *goal = cl->body[0];
            Term *dg   = term_deref(goal);
            /* Check for :- export(Name/Arity) or :- export(Name) */
            int is_export = 0;
            if (dg && dg->tag == TT_COMPOUND
                && dg->compound.arity == 1) {
                int fn = dg->compound.functor;
                const char *fname = prolog_atom_name(fn);
                if (fname && strcasecmp(fname, "export") == 0) {
                    is_export = 1;
                    Term *arg = term_deref(dg->compound.args[0]);
                    /* arg is Name/Arity compound or bare atom */
                    const char *ename = NULL;
                    if (arg && arg->tag == TT_COMPOUND
                        && arg->compound.arity == 2) {
                        /* Name/Arity — extract Name */
                        Term *n = term_deref(arg->compound.args[0]);
                        if (n && n->tag == TT_ATOM)
                            ename = prolog_atom_name(n->atom_id);
                    } else if (arg && arg->tag == TT_ATOM) {
                        ename = prolog_atom_name(arg->atom_id);
                    }
                    if (ename) {
                        ExportEntry *e = calloc(1, sizeof *e);
                        e->name = strdup(ename);
                        e->next = prog->exports;
                        prog->exports = e;
                    }
                }
            }
            if (!is_export) {
                /* Other directive → plain stmt */
                STMT_t *s = stmt_new();
                s->subject = lower_term(goal);
                s->lineno  = cl->lineno;
                s->lang    = LANG_PL;  /* U-12 */
                if (!prog->head) prog->head = s;
                else             prog->tail->next = s;
                prog->tail = s;
                prog->nstmts++;
            }
        }
    }

    /* ---- Pass 3: emit one STMT_t per AST_CHOICE */
    for (int i = 0; i < nkeys; i++) {
        STMT_t *s = stmt_new();
        s->subject = choices[i];
        s->lineno  = 0;
        s->lang    = LANG_PL;  /* U-12 */
        if (!prog->head) prog->head = s;
        else             prog->tail->next = s;
        prog->tail = s;
        prog->nstmts++;
    }

    return prog;
}

/* =========================================================================
 * prolog_lower_pretty — IR dump for diagnostics
 * ======================================================================= */
static void ast_dump(AST_t *e, int indent, FILE *out) {
    if (!e) { fprintf(out, "%*s<null>\n", indent, ""); return; }
    const char *kname = "?";
    switch (e->t) {
        case AST_CHOICE:      kname = "AST_CHOICE";      break;
        case AST_CLAUSE:      kname = "AST_CLAUSE";      break;
        case AST_UNIFY:       kname = "AST_UNIFY";       break;
        case AST_CUT:         kname = "AST_CUT";         break;
        case AST_TRAIL_MARK:  kname = "AST_TRAIL_MARK";  break;
        case AST_TRAIL_UNWIND:kname = "AST_TRAIL_UNWIND";break;
        case AST_FNC:         kname = "AST_FNC";         break;
        case AST_QLIT:        kname = "AST_QLIT";        break;
        case AST_ILIT:        kname = "AST_ILIT";        break;
        case AST_FLIT:        kname = "AST_FLIT";        break;
        case AST_VAR:        kname = "AST_VAR";        break;
        case AST_ADD:         kname = "AST_ADD";         break;
        case AST_SUB:         kname = "AST_SUB";         break;
        case AST_MUL:         kname = "AST_MUL";         break;
        case AST_DIV:         kname = "AST_DIV";         break;
        default: break;
    }
    fprintf(out, "%*s%s", indent, "", kname);
    if (e->v.sval) fprintf(out, "  sval=%s", e->v.sval);
    if (e->v.ival) fprintf(out, "  ival=%ld", e->v.ival);
    if (e->t == AST_CLAUSE)
        fprintf(out, "  n_vars=%ld  n_args=%.0f", e->v.ival, e->v.dval);
    fprintf(out, "\n");
    for (int i = 0; i < e->n; i++)
        ast_dump(e->c[i], indent + 2, out);
}

void prolog_lower_pretty(CODE_t *prog, FILE *out) {
    for (STMT_t *s = prog->head; s; s = s->next) {
        fprintf(out, "--- stmt (line %d) ---\n", s->lineno);
        ast_dump(s->subject, 2, out);
    }
}

/*============================================================================================================================
 * pl_assert_term — build an AST_CLAUSE from a runtime Term* (for assertz/asserta).
 *
 * Accepts:
 *   atom or compound f(a1..an)          → fact clause, no body
 *   ':-'(head, body)                    → rule clause
 *
 * Returns a heap-allocated AST_CLAUSE ready to append to an AST_CHOICE node.
 * Also sets *key_out to the predicate key (functor atom_id, arity).
 * Returns NULL on error.
 *============================================================================================================================*/
AST_t *pl_assert_term(Term *t, int *functor_out, int *arity_out) {
    if (!t) return NULL;
    t = term_deref(t);
    if (!t) return NULL;

    Term *head = NULL;
    Term *body = NULL;  /* NULL means fact (body = true) */

    /* Detect :-(Head, Body) rule form */
    if (t->tag == TT_COMPOUND && t->compound.arity == 2) {
        const char *fn = prolog_atom_name(t->compound.functor);
        if (fn && strcmp(fn, ":-") == 0) {
            head = term_deref(t->compound.args[0]);
            body = term_deref(t->compound.args[1]);
        }
    }
    if (!head) head = t;  /* fact */

    /* Extract functor/arity from head */
    PredKey k = key_of_head(head);
    if (k.functor < 0) return NULL;

    if (functor_out) *functor_out = k.functor;
    if (arity_out)   *arity_out   = k.arity;

    /* Build a synthetic PlClause so we can reuse lower_clause */
    PlClause cl_buf;
    memset(&cl_buf, 0, sizeof cl_buf);
    cl_buf.head   = head;
    cl_buf.lineno = 0;

    /* Body: flatten conjunction into body array */
    Term *body_goals[256];
    int nbody = 0;
    if (body) {
        /* Flatten comma-conjunction */
        Term *cur = body;
        while (cur && cur->tag == TT_COMPOUND && cur->compound.arity == 2) {
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
