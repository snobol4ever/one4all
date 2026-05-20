#ifndef BB_BOX_H
#define BB_BOX_H
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "descr.h"
#include "name_t.h"
/*================================================================================================================================================================================*/
static inline DESCR_t descr_match_span(const char * σ, int δ) { DESCR_t d; d.v = DT_S; d.slen = (uint32_t)δ; d.s = (char *)σ; return d; }
static inline DESCR_t descr_match     (const char * σ, int δ) { return descr_match_span(σ, δ); }
static inline DESCR_t descr_match_cat (DESCR_t x, DESCR_t y)  { DESCR_t d; d.v = DT_S; d.slen = x.slen + y.slen; d.s = x.s; return d; }
static inline DESCR_t descr_bool(int ok) {
    if (ok) { DESCR_t d; d.v = DT_SNUL; d.slen = 0; d.s = (char *)""; return d; }
    return FAILDESCR;
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const int α = 0;
static const int β = 1;
#define BB_ALPHA_DEFINED 1
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern const char * Σ;
extern int          Δ;
extern int          Ω;
extern int          Σlen;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void * bb_enter(void ** ζζ, size_t size) {
    void * ζ = *ζζ;
    if (size) {
        if (ζ) memset(ζ, 0, size);
        else   ζ = *ζζ = calloc(1, size);
    }
    return ζ;
}
#define BB_ENTER(ref, T)  ((T *)bb_enter((void **)(ref), sizeof(T)))
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef DESCR_t (*bb_box_fn)(void * zeta, int entry);
typedef struct { bb_box_fn fn; void * ζ; size_t ζ_size; } bb_node_t;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct { int n; }                             len_t;
typedef struct { const char * chars; int δ; }        brkx_t;
typedef struct { int count; int start; }              arb_t;
typedef struct { int dummy; }                         rem_t;
typedef struct { int dummy; }                         succeed_t;
typedef struct { int n; int advance; }                tab_t;
typedef struct { int n; int advance; }                rtab_t;
typedef struct { int fired; }                         fence_t;
typedef struct { int dummy; }                         abort_t;
typedef struct { int δ; }                             bal_t;
typedef struct { int done; const char * varname; }   atp_t;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct cap_s {
    bb_box_fn  fn;
    void     * state;
    int        immediate;
    NAME_t     name;
    DESCR_t    pending;
    int        has_pending;
    int        registered;
    int        cap_start;
} cap_t;
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t  bb_cap    (void * zeta, int entry);
extern cap_t  * bb_cap_new(bb_box_fn child_fn, void * child_state, const char * varname, DESCR_t * var_ptr, int immediate);
extern cap_t  * bb_cap_new_call(bb_box_fn child_fn, void * child_state,
                                const char * fnc_name,
                                DESCR_t * fnc_args, int fnc_nargs,
                                char ** fnc_arg_names, int fnc_n_arg_names,
                                int immediate);
extern void flush_pending_captures(void);
extern void reset_capture_registry(void);
extern void clear_pending_flags(void);
extern DESCR_t  bb_atp    (void * zeta, int entry);
extern atp_t  * bb_atp_new(const char * varname);
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int exec_stmt_blob(const char * subj_name, DESCR_t * subj_var, bb_box_fn root_fn, DESCR_t * repl, int has_repl);
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef DESCR_t (*univ_box_fn)(void * zeta, int entry);
typedef enum { bb_scan, bb_pump, bb_once } BrokerMode;
void exec_stmt_pool_reset(void);
/*================================================================================================================================================================================*/
#endif
