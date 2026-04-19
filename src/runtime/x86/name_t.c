/*
 * name_t.c — SN-21: name_commit_value() dispatcher.
 *
 * One entry point, one switch on NameKind_t, one correct commit per kind.
 * This file intentionally contains no other logic — boxes and NAM commit
 * both call through here so there is exactly one place where an lvalue
 * actually becomes a store.
 */

#include <string.h>
#include <gc.h>

#include "name_t.h"
#include "snobol4.h"    /* NV_SET_fn, NV_GET_fn, g_user_call_hook, DT_*       */

/*---------------------------------------------------------------------------*/
/* name_commit_value — commit `value` into the location described by *nm     */
/*---------------------------------------------------------------------------*/

void name_commit_value(const NAME_t *nm, DESCR_t value)
{
    if (!nm) return;

    switch (nm->kind) {

    case NM_VAR:
        if (nm->var_name && nm->var_name[0])
            NV_SET_fn(nm->var_name, value);
        return;

    case NM_PTR:
        if (nm->var_ptr)
            *nm->var_ptr = value;
        return;

    case NM_IDX:
        /* Reserved — A[i,j] path is still ad-hoc; SN-21c+ will land real
         * evaluation + commit here.  No-op for now so accidental use is
         * visible rather than crashing. */
        return;

    case NM_CALL:
        /* Indirect call (pat . *fn()): invoke fn, obtain DT_N return, then
         * write value into the cell that DT_N points at.  Arg-name-deferred
         * resolution (TL-2) happens here if fnc_arg_names is set. */
        if (!g_user_call_hook || !nm->fnc_name || !nm->fnc_name[0]) return;

        DESCR_t *call_args = nm->fnc_args;
        int      call_n    = nm->fnc_nargs;
        DESCR_t  resolved_buf[8];
        DESCR_t *resolved  = NULL;

        if (nm->fnc_arg_names && nm->fnc_n_arg_names > 0) {
            call_n   = nm->fnc_n_arg_names;
            resolved = (call_n <= 8) ? resolved_buf
                                     : (DESCR_t *)GC_MALLOC((size_t)call_n * sizeof(DESCR_t));
            for (int k = 0; k < call_n; k++) {
                resolved[k] = NV_GET_fn(nm->fnc_arg_names[k]
                                        ? nm->fnc_arg_names[k] : "");
            }
            call_args = resolved;
        }

        DESCR_t name_d = g_user_call_hook(nm->fnc_name, call_args, call_n);
        DESCR_t *cell  = (name_d.v == DT_N && name_d.ptr)
                         ? (DESCR_t *)name_d.ptr : NULL;
        if (cell) *cell = value;
        return;
    }
}

/*---------------------------------------------------------------------------*/
/* Convenience initialisers                                                   */
/*---------------------------------------------------------------------------*/

void name_init_as_var(NAME_t *nm, const char *var_name)
{
    if (!nm) return;
    memset(nm, 0, sizeof(*nm));
    nm->kind     = NM_VAR;
    nm->var_name = var_name;
}

void name_init_as_ptr(NAME_t *nm, DESCR_t *var_ptr)
{
    if (!nm) return;
    memset(nm, 0, sizeof(*nm));
    nm->kind    = NM_PTR;
    nm->var_ptr = var_ptr;
}

void name_init_as_call(NAME_t *nm,
                       const char *fnc_name,
                       DESCR_t *fnc_args, int fnc_nargs,
                       char **fnc_arg_names, int fnc_n_arg_names)
{
    if (!nm) return;
    memset(nm, 0, sizeof(*nm));
    nm->kind            = NM_CALL;
    nm->fnc_name        = fnc_name;
    nm->fnc_args        = fnc_args;
    nm->fnc_nargs       = fnc_nargs;
    nm->fnc_arg_names   = fnc_arg_names;
    nm->fnc_n_arg_names = fnc_n_arg_names;
}
