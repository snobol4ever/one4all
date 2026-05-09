/*
 * interp_private.h — private shared header for the interp_*.c family
 *
 * Every interp_*.c file includes this header and nothing else at the top.
 * It provides all system includes, all frontend/runtime includes, all extern
 * declarations, and the struct/type definitions that are internal to the
 * interpreter but shared across the split units.
 *
 * interp.h remains the PUBLIC interface (declarations only, no internals).
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 * PURPOSE: RS-3 — split interp.c by concern
 */

#ifndef INTERP_PRIVATE_H
#define INTERP_PRIVATE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <gc.h>

/* ── frontend ─────────────────────────────────────────────────────────── */
#include "frontend/snobol4/scrip_cc.h"
extern CODE_t *sno_parse(FILE *f, const char *filename);
#include "frontend/snocone/snocone_driver.h"
#include "frontend/prolog/prolog_driver.h"
#include "frontend/prolog/term.h"
#include "frontend/prolog/prolog_runtime.h"
#include "frontend/prolog/prolog_atom.h"
#include "frontend/raku/raku_re.h"
#include "frontend/prolog/prolog_builtin.h"
#include "frontend/prolog/pl_broker.h"
#include "frontend/icon/icon_driver.h"
#include "frontend/raku/raku_driver.h"
#include "frontend/rebus/rebus_lower.h"
#include "frontend/icon/icon_gen.h"
#include "frontend/icon/icon_lex.h"

extern void ir_print_node   (const AST_t *e, FILE *f);
extern void ir_print_node_nl(const AST_t *e, FILE *f);

/* ── runtime ──────────────────────────────────────────────────────────── */
#include "runtime/x86/snobol4.h"
#include "runtime/x86/sil_macros.h"
#include "runtime/x86/snobol4_runtime_shim.h"
#include "runtime/x86/sm_lower.h"
#include "runtime/x86/sm_interp.h"
#include "runtime/x86/sm_prog.h"
#include "runtime/x86/bb_build.h"
#include "runtime/x86/sm_codegen.h"
#include "runtime/x86/sm_image.h"

extern DESCR_t pat_at_cursor(const char *varname);

#include "runtime/interp/coro_runtime.h"
#include "runtime/interp/pl_runtime.h"

extern DESCR_t      eval_expr(const char *src);
extern const char  *exec_code(DESCR_t code_block);
extern int exec_stmt(const char *subj_name,
                     DESCR_t    *subj_var,
                     DESCR_t     pat,
                     DESCR_t    *repl,
                     int         has_repl);
extern const char *Σ;
extern int         Ω;
extern int         Δ;
extern int         Σlen;

#include "interp.h"

/* ── globals (defined in interp_globals.c) ────────────────────────────── */
extern char  g_raku_exception[512];
extern Raku_match  g_raku_match;
extern const char *g_raku_subject;
extern int   g_kw_ctx;           /* set by execute_program, read by NV_SET_fn guard */

/* ── Raku file-handle table (defined in interp_globals.c) ─────────────── */
#define RAKU_FH_MAX 64
extern FILE *raku_fh_table[RAKU_FH_MAX];
extern int   raku_fh_init;
void  raku_fh_ensure_init(void);
int   raku_fh_alloc(FILE *fp);
FILE *raku_fh_get(int idx);
void  raku_fh_free(int idx);

/* ── label table (defined in interp_label.c) ─────────────────────────── */
const char *define_spec_from_expr(AST_t *subj);
const char *define_entry_from_expr(AST_t *subj);
#define LABEL_MAX 4096
typedef struct { const char *name; STMT_t *stmt; } LabelEntry;
extern LabelEntry label_table[LABEL_MAX];
extern int        label_count;

/* ── call stack (defined in interp_call.c) ───────────────────────────── */
#define CALL_STACK_MAX 256
#define SHADOW_MAX 32
typedef struct { char name[64]; DESCR_t val; } ShadowEntry;
typedef struct {
    jmp_buf  ret_env;
    char     fname[128];
    char   **saved_names;
    DESCR_t *saved_vals;
    int      nsaved;
    DESCR_t  retval_cell;
    int      retval_set;
    ShadowEntry shadow[SHADOW_MAX];
    int         nshadow;
} CallFrame;
extern CallFrame call_stack[CALL_STACK_MAX];
extern int       call_depth;

/* shadow table helpers (defined in interp_call.c) */
int  shadow_get(const char *name, DESCR_t *out);
void shadow_set_cur(const char *name, DESCR_t val);
int  shadow_has(const char *name);

/* IC-5: icn_init persistence (defined in interp_call.c) */
#define ICN_INIT_MAX   64
#define ICN_INIT_SLOTS  8
typedef struct { char nm[64]; DESCR_t val; } IcnInitSlot;
typedef struct { int id; int ns; IcnInitSlot s[ICN_INIT_SLOTS]; } IcnInitEnt;
extern IcnInitEnt init_tab[ICN_INIT_MAX];
extern int        icn_init_n;
void icn_init_update_snapshot(char **snames, DESCR_t *svals, int nsaved);

/* ── pattern helpers (defined in interp_eval.c) ──────────────────────── */
int _is_pat_fnc_name(const char *s);
int _expr_is_pat(AST_t *e);

/* ── set_and_trace (defined in interp_eval.c, used widely) ──────────── */
void set_and_trace(const char *name, DESCR_t val);

/* ── NAME_DEREF / NAME_SET (inline, defined here) ───────────────────── */
static inline DESCR_t NAME_DEREF(DESCR_t d) {
    if (IS_NAME(d)) {
        if (IS_NAMEPTR(d)) return NAME_DEREF_PTR(d);
        if (IS_NAMEVAL(d)) return NV_GET_fn(d.s);
    }
    return d;
}
static inline int NAME_SET(DESCR_t nd, DESCR_t val) {
    if (IS_NAME(nd)) {
        if (IS_NAMEPTR(nd)) { NAME_DEREF_PTR(nd) = val; return 1; }
        if (IS_NAMEVAL(nd)) { set_and_trace(nd.s, val); return 1; }
    }
    return 0;
}

/* ── lvalue helpers (defined in interp_lvalue.c) ───────────────────── */
DESCR_t *interp_eval_ref(AST_t *e);

/* ── DATA field interior ptr (defined in interp_eval.c) ─────────────── */
DESCR_t *data_field_ptr(const char *fname, DESCR_t inst);

/* ── Icon string-section assign (defined in interp_eval.c) ──────────── */
int icn_string_section_assign(AST_t *lhs, DESCR_t val);

/* ── DATA registry (defined in interp_data.c) ───────────────────────── */
typedef struct { char name[64]; int nfields; char fields[64][64]; } ScDatType;
ScDatType *sc_dat_register(const char *spec);
ScDatType *sc_dat_find_type(const char *name);
ScDatType *sc_dat_find_field(const char *name, int *fidx);
DESCR_t    sc_dat_construct(ScDatType *t, DESCR_t *args, int nargs);
DESCR_t    sc_dat_field_get(const char *fname, DESCR_t obj);

/* ── call_user_function (defined in interp_call.c) ───────────────────── */
DESCR_t call_user_function(const char *fname, DESCR_t *args, int nargs);

/* ── icn helpers needed across eval/call (defined in interp_eval.c) ──── */
DESCR_t icn_call_builtin(AST_t *call, DESCR_t *args, int nargs);
/* CH-17g-runtime-bridge-1: name-based dispatch for AST_t-free Icon builtins.
 * Returns 1 if handled (and writes result to *out), 0 otherwise.
 * Lets SM_CALL_FN (sm_interp.c) reach Icon `write`/`writes` from chunk
 * bodies without needing an IR call node. */
int icn_try_call_builtin_by_name(const char *fn, DESCR_t *args, int nargs, DESCR_t *out);
const char *real_str(double r, char *buf, int bufsz);

/* ── RS-23a-raku: Raku-builtin dispatch (defined in runtime/interp/raku_builtins.c) ── */
int raku_try_call_builtin(AST_t *call, DESCR_t *out);

/* ── RS-23-extra-prep: SCAN-context builtin dispatch (defined in runtime/interp/scan_builtins.c) ── */
int scan_try_call_builtin(AST_t *call, DESCR_t *args, int nargs, DESCR_t *out);

/* ── Prolog pred table size (used by execute_program) ───────────────── */
#define PL_PRED_TABLE_SIZE PL_PRED_TABLE_SIZE_FWD

#endif /* INTERP_PRIVATE_H */
