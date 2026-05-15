#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gc.h>
#include "name_t.h"
#include "snobol4.h"    /* NV_SET_fn, NV_GET_fn, g_user_call_hook, DT_*       */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t EVAL_fn(DESCR_t expr);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int name_commit_value(const NAME_t *nm, DESCR_t value)
{
    if (!nm) return 0;
    if (value.v == DT_E) value = EVAL_fn(value);
    switch (nm->kind) {
    case NM_VAR:
        if (nm->var_name && nm->var_name[0])
            NV_SET_fn(nm->var_name, value);
        return 0;
    case NM_PTR:
        if (nm->var_ptr) {
            *nm->var_ptr = value;
            const char *recovered = NV_name_from_ptr(nm->var_ptr);
            comm_var(recovered ? recovered : "<lval>", value);
        }
        return 0;
    case NM_IDX:
        return 0;
    case NM_CALL:
        if (!g_user_call_hook || !nm->fnc_name || !nm->fnc_name[0]) return 0;
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
        } else {
            int have_dte = 0;
            for (int k = 0; k < call_n; k++) {
                if (nm->fnc_args[k].v == DT_E) { have_dte = 1; break; }
            }
            if (have_dte && call_n > 0) {
                resolved = (call_n <= 8) ? resolved_buf
                                         : (DESCR_t *)GC_MALLOC((size_t)call_n * sizeof(DESCR_t));
                for (int k = 0; k < call_n; k++) {
                    resolved[k] = (nm->fnc_args[k].v == DT_E)
                                  ? EVAL_fn(nm->fnc_args[k])
                                  : nm->fnc_args[k];
                }
                call_args = resolved;
            }
        }
        DESCR_t name_d = g_user_call_hook(nm->fnc_name, call_args, call_n);
        if (getenv("ONE4ALL_USERCALL_TRACE")) {
            fprintf(stderr, "NM_CALL name=%s nargs=%d arg_names=%s\n",
                    nm->fnc_name ? nm->fnc_name : "(null)",
                    call_n,
                    (nm->fnc_arg_names && nm->fnc_n_arg_names > 0) ? "yes" : "no");
            for (int k = 0; k < call_n; k++) {
                const char *kind = "?";
                switch ((int)call_args[k].v) {
                    case DT_SNUL: kind = "DT_SNUL"; break;
                    case DT_S:    kind = "DT_S";    break;
                    case DT_E:    kind = "DT_E";    break;
                    case DT_I:    kind = "DT_I";    break;
                    case DT_R:    kind = "DT_R";    break;
                    case DT_N:    kind = "DT_N";    break;
                    case DT_P:    kind = "DT_P";    break;
                    case DT_FAIL: kind = "DT_FAIL"; break;
                }
                const char *str = (call_args[k].v == DT_S && call_args[k].s) ? call_args[k].s : "";
                char numbuf[32]; numbuf[0] = '\0';
                if (call_args[k].v == DT_I) snprintf(numbuf, sizeof numbuf, "%ld", (long)call_args[k].i);
                else if (call_args[k].v == DT_R) snprintf(numbuf, sizeof numbuf, "%g", call_args[k].r);
                const char *raw_kind = nm->fnc_args ? "?" : "(name)";
                if (nm->fnc_args) { switch ((int)nm->fnc_args[k].v) {
                    case DT_SNUL: raw_kind = "DT_SNUL"; break;
                    case DT_S:    raw_kind = "DT_S";    break;
                    case DT_E:    raw_kind = "DT_E";    break;
                    case DT_I:    raw_kind = "DT_I";    break;
                    case DT_R:    raw_kind = "DT_R";    break;
                    case DT_N:    raw_kind = "DT_N";    break;
                    case DT_P:    raw_kind = "DT_P";    break;
                    case DT_FAIL: raw_kind = "DT_FAIL"; break;
                } }
                fprintf(stderr, "  arg[%d] raw v=%s   eff v=%s s=\"%s\" num=%s\n",
                        k, raw_kind, kind, str, numbuf);
            }
        }
        DESCR_t *cell  = (name_d.v == DT_N && name_d.ptr)
                         ? (DESCR_t *)name_d.ptr : NULL;
        if (!cell) return -1;
        *cell = value;
        {
            const char *recovered = NV_name_from_ptr(cell);
            comm_var(recovered ? recovered : "<lval>", value);
        }
        return 0;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void name_init_as_var(NAME_t *nm, const char *var_name)
{
    if (!nm) return;
    memset(nm, 0, sizeof(*nm));
    nm->kind     = NM_VAR;
    nm->var_name = var_name;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void name_init_as_ptr(NAME_t *nm, DESCR_t *var_ptr)
{
    if (!nm) return;
    memset(nm, 0, sizeof(*nm));
    nm->kind    = NM_PTR;
    nm->var_ptr = var_ptr;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
