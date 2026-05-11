/*
 * interp_ref.c — lvalue evaluator (interp_eval_ref)
 *
 * Evaluates an AST_t in NAME context, returning a DESCR_t* interior pointer.
 * Mirrors SIL: ARYA10 (array), ASSCR (table), FIELD (DATA), GNVARS (variable).
 * Called by assignment sites that need a writable cell rather than a value.
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

DESCR_t *interp_eval_ref(AST_t *e)
{
    NO_AST_WALK_GUARD("interp_eval_ref");
    if (!e) return NULL;
    switch (e->t) {

    case AST_VAR: {
        /* Simple variable — find-or-create NV cell */
        return NV_PTR_fn(e->v.sval);
    }

    case AST_IDX: {
        /* arr[idx] or arr[i][j] — return interior cell pointer */
        if (e->n < 2) return NULL;
        DESCR_t base = interp_eval(e->c[0]);
        if (IS_FAIL_fn(base)) return NULL;
        DESCR_t idx  = interp_eval(e->c[1]);
        if (IS_FAIL_fn(idx)) return NULL;
        if (IS_ARR(base)) {
            return array_ptr(base.arr, (int)to_int(idx));
        }
        if (IS_TBL(base)) {
            return table_ptr(base.tbl, idx);
        }
        return NULL;
    }

    case AST_NAME: {
        /* .expr — dot operator: evaluate child as lvalue */
        if (e->n == 1)
            return interp_eval_ref(e->c[0]);
        /* .var plain (sval set): NV cell */
        if (e->v.sval)
            return NV_PTR_fn(e->v.sval);
        return NULL;
    }

    case AST_FIELD: {
        /* obj.fieldname as lvalue — return interior ptr to field cell */
        if (!e->v.sval || e->n < 1) return NULL;
        DESCR_t obj = interp_eval(e->c[0]);
        if (IS_FAIL_fn(obj)) return NULL;
        return data_field_ptr(e->v.sval, obj);
    }

    case AST_CAPT_COND_ASGN: {
        /* .var (parsed as AST_CAPT_COND_ASGN child AST_VAR) */
        if (e->n >= 1 && e->c[0]->t == AST_VAR)
            return NV_PTR_fn(e->c[0]->v.sval);
        if (e->n >= 1)
            return interp_eval_ref(e->c[0]);
        return NULL;
    }

    case AST_INDIRECT: {
        /* $expr — evaluate expr to get name string, then return that var's cell */
        DESCR_t name_d = interp_eval(e->n >= 1 ? e->c[0] : NULL);
        const char *nm0 = IS_NAMEPTR(name_d)
            ? VARVAL_fn(NAME_DEREF_PTR(name_d))
            : VARVAL_fn(name_d);
        if (!nm0 || !*nm0) return NULL;
        char *nm = GC_strdup(nm0); sno_fold_name(nm);  /* SN-19 */
        return NV_PTR_fn(nm);
    }

    default: return NULL;
    }
}

