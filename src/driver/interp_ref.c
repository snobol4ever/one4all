#include "interp_private.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t *interp_eval_ref(tree_t *e)
{
    NO_AST_WALK_GUARD("interp_eval_ref");
    if (!e) return NULL;
    switch (e->t) {
    case TT_VAR: {
        return NV_PTR_fn(e->v.sval);
    }
    case TT_IDX: {
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
    case TT_NAME: {
        if (e->n == 1)
            return interp_eval_ref(e->c[0]);
        if (e->v.sval)
            return NV_PTR_fn(e->v.sval);
        return NULL;
    }
    case TT_FIELD: {
        if (!e->v.sval || e->n < 1) return NULL;
        DESCR_t obj = interp_eval(e->c[0]);
        if (IS_FAIL_fn(obj)) return NULL;
        return data_field_ptr(e->v.sval, obj);
    }
    case TT_CAPT_COND_ASGN: {
        if (e->n >= 1 && e->c[0]->t == TT_VAR)
            return NV_PTR_fn(e->c[0]->v.sval);
        if (e->n >= 1)
            return interp_eval_ref(e->c[0]);
        return NULL;
    }
    case TT_INDIRECT: {
        DESCR_t name_d = interp_eval(e->n >= 1 ? e->c[0] : NULL);
        const char *nm0 = IS_NAMEPTR(name_d)
            ? VARVAL_fn(NAME_DEREF_PTR(name_d))
            : VARVAL_fn(name_d);
        if (!nm0 || !*nm0) return NULL;
        char *nm = GC_strdup(nm0); sno_fold_name(nm);
        return NV_PTR_fn(nm);
    }
    default: return NULL;
    }
}
