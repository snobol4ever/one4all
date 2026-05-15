#include "bb_box.h"
#include "snobol4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*================================================================================================================================================================================*/
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define ARBNO_INIT  8
#define ARBNO_MAGIC 0xA2B20000u
typedef struct { DESCR_t matched; int start; }                                        arbno_frame_t;
typedef struct { bb_box_fn fn; void * state; int depth; int cap; arbno_frame_t * stack; uint32_t magic; } arbno_t;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
arbno_t * bb_arbno_new(bb_box_fn fn, void * state) {
    arbno_t * ζ = calloc(1, sizeof(arbno_t));
    ζ->fn = fn; ζ->state = state; ζ->cap = ARBNO_INIT;
    ζ->stack = malloc(ζ->cap * sizeof(arbno_frame_t));
    ζ->magic = ARBNO_MAGIC;
    return ζ;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define MAX_CAPTURES 256
static cap_t * g_capture_list[MAX_CAPTURES];
static int     g_capture_count = 0;
static void register_capture(cap_t * c) {
    int i;
    for (i = 0; i < g_capture_count; i++) if (g_capture_list[i] == c) return;
    if (g_capture_count < MAX_CAPTURES) g_capture_list[g_capture_count++] = c;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void flush_pending_captures(void) { int i; for (i=0;i<g_capture_count;i++) g_capture_list[i]->has_pending=0; g_capture_count=0; }
void reset_capture_registry (void) { g_capture_count = 0; }
void clear_pending_flags    (void) { int i; for (i=0;i<g_capture_count;i++) g_capture_list[i]->has_pending=0; }
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
cap_t * bb_cap_new(bb_box_fn child_fn, void * child_state, const char * varname, DESCR_t * var_ptr, int immediate) {
    cap_t * ζ = calloc(1, sizeof(cap_t));
    if (!ζ) return NULL;
    ζ->fn = child_fn; ζ->state = child_state; ζ->immediate = immediate;
    if (var_ptr)       name_init_as_ptr(&ζ->name, var_ptr);
    else if (varname)  name_init_as_var(&ζ->name, varname);
    return ζ;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
cap_t * bb_cap_new_call(bb_box_fn child_fn, void * child_state,
                        const char * fnc_name,
                        DESCR_t * fnc_args, int fnc_nargs,
                        char ** fnc_arg_names, int fnc_n_arg_names,
                        int immediate) {
    cap_t * ζ = calloc(1, sizeof(cap_t));
    if (!ζ) return NULL;
    ζ->fn = child_fn; ζ->state = child_state; ζ->immediate = immediate;
    name_init_as_call(&ζ->name, fnc_name, fnc_args, fnc_nargs, fnc_arg_names, fnc_n_arg_names);
    return ζ;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
abort_t   * bb_abort_new  (void) { return calloc(1, sizeof(abort_t));   }
fence_t   * bb_fence_new  (void) { return calloc(1, sizeof(fence_t));   }
succeed_t * bb_succeed_new(void) { return calloc(1, sizeof(succeed_t)); }
atp_t     * bb_atp_new    (const char * varname) { atp_t * ζ = calloc(1, sizeof(atp_t)); ζ->varname = varname; return ζ; }
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#include "../../frontend/icon/icon_gen.h"
icn_list_iterate_state_t   * icon_list_iterate_new  (void) { return calloc(1, sizeof(icn_list_iterate_state_t));   }
icn_tbl_iterate_state_t    * icon_tbl_iterate_new   (void) { return calloc(1, sizeof(icn_tbl_iterate_state_t));    }
icn_record_iterate_state_t * icon_record_iterate_new(void) { return calloc(1, sizeof(icn_record_iterate_state_t)); }
