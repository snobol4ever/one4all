#ifndef ICON_GEN_H
#define ICON_GEN_H
#include <stdlib.h>
#include <string.h>
#include "bb_broker.h"
#include "snobol4.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_fail_box(void *zeta, int entry) { (void)zeta; (void)entry; return FAILDESCR; }
static const bb_node_t ICN_FAIL_GEN = { icn_fail_box, NULL, 0 };
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void *icn_gen_enter(void **pp, size_t size) {
    void *p = *pp;
    if (size) {
        if (p) memset(p, 0, size);
        else   p = *pp = calloc(1, size);
    }
    return p;
}
#define ICN_ENTER(ref, T)  ((T *)icn_gen_enter((void **)(ref), sizeof(T)))
typedef struct { long lo; long hi; long cur; }                                        icn_to_state_t;
#define ICN_TO_NESTED_MAX 256
typedef struct {
    long lo_vals[ICN_TO_NESTED_MAX];
    long hi_vals[ICN_TO_NESTED_MAX];
    int  nlo, nhi;
    int  li, hi2;
    long cur;
} icn_to_nested_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_to_nested(void *zeta, int entry);
typedef struct { long lo; long hi; long step; long cur; }                             icn_to_by_state_t;
typedef struct { double lo; double hi; double step; double cur; }                    icn_to_by_real_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_to_by_real(void *zeta, int entry);
typedef struct { const char *str; long len; long pos; char ch[2]; }                  icn_iterate_state_t;
typedef struct { TBBLK_t *tbl; int bucket; TBPAIR_t *entry; }                        icn_tbl_iterate_state_t;
typedef struct { TBBLK_t *tbl; int bucket; TBPAIR_t *entry; }                        icn_tbl_key_iterate_state_t;
typedef struct { DESCR_t list_obj; int pos; }                                         icn_list_iterate_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_list_iterate(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_tbl_key_iterate(void *zeta, int entry);
typedef struct { DESCR_t inst; int pos; }                                             icn_record_iterate_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_record_iterate(void *zeta, int entry);
typedef struct { const char *needle; const char *hay; int nlen; const char *next; }  icn_find_state_t;
typedef struct {
    bb_node_t   subj_gen;
    const char *needle;
    int         nlen;
    const char *hay;
    const char *next;
    int         subj_entry;
} icn_find_gen_subj_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_find_subj(void *zeta, int entry);
typedef struct {
    bb_node_t   subj_gen;
    const char *cset;
    const char *hay;
    int         slen;
    int         pos;
    int         subj_entry;
} icn_upto_gen_subj_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_upto_subj(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_to_by(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_iterate(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_tbl_iterate(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_find(void *zeta, int entry);
typedef struct { const char *s; const char *c1; const char *c2; const char *c3; int slen; int pos; int endp; } icn_bal_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_bal(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_binop(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_alternate(void *zeta, int entry);
typedef enum {
    ICN_BINOP_ADD, ICN_BINOP_SUB, ICN_BINOP_MUL, ICN_BINOP_DIV, ICN_BINOP_MOD,
    ICN_BINOP_LT, ICN_BINOP_LE, ICN_BINOP_GT, ICN_BINOP_GE, ICN_BINOP_EQ, ICN_BINOP_NE,
    ICN_BINOP_CONCAT,
    ICN_BINOP_SLT, ICN_BINOP_SLE, ICN_BINOP_SGT, ICN_BINOP_SGE, ICN_BINOP_SEQ, ICN_BINOP_SNE,
    ICN_BINOP_POW,
} IcnBinopKind;
typedef struct {
    bb_node_t    left;
    bb_node_t    right;
    IcnBinopKind op;
    int          is_relop;
    DESCR_t      left_val;
    DESCR_t      right_val;
    int          phase;
} icn_binop_gen_state_t;
typedef struct {
    bb_node_t gen[2];
    int       which;
} icn_alternate_state_t;
#ifndef EXPR_T_DEFINED
#define EXPR_T_DEFINED
typedef struct tree_t tree_t;
#endif
typedef struct {
    bb_node_t gen;
    long      max;
    long      count;
} icn_limit_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_limit(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_cset_compl(void *zeta, int entry);
typedef struct {
    bb_node_t  subj_gen;
    tree_t    *body;
    int        started;
    bb_node_t  body_gen;
    int        body_live;
    const char *body_subj;
    int         body_pos;
} icn_scan_gen_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_scan_gen(void *zeta, int entry);
typedef struct {
    bb_node_t  gen;
    tree_t    *gen_ast;
    tree_t    *body;
    int        started;
} icn_every_state_t;
typedef struct {
    bb_node_t  gen_a;
    bb_node_t  gen_b;
    tree_t    *ast_b;
    int        b_started;
    int        a_started;
} icn_mutual_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_mutual(void *zeta, int entry);
typedef struct {
    tree_t   *proc_expr;
    bb_node_t arg_box;
    DESCR_t   cur_arg;
} icn_bang_binary_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_bang_binary(void *zeta, int entry);
typedef struct {
    tree_t   **children;
    int        n;
    bb_node_t  last_box;
    int        started;
} icn_seq_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_seq_expr(void *zeta, int entry);
typedef struct { bb_node_t gen; struct tree_t *cat_expr; struct tree_t *leaf; } icn_cat_gen_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_cat(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void gather_trampoline(void);
typedef struct { tree_t *expr; int frame_popped; } icn_not_state_t;
typedef struct { tree_t *expr; int started; int ever_succeeded; bb_node_t inner; } icn_repalt_state_t;
typedef struct { tree_t *expr; tree_t *body; } icn_while_state_t;
typedef struct { tree_t *expr; tree_t *body; } icn_until_state_t;
typedef struct { tree_t *body; } icn_repeat_state_t;
typedef struct { bb_node_t obj_gen; const char *field; } icn_field_gen_state_t;
typedef struct { const char *kw; int fired; DESCR_t val; } icn_kw_gen_state_t;
typedef enum { ICN_SEC_RANGE, ICN_SEC_PLUS, ICN_SEC_MINUS } icn_sec_kind_t;
#define ICN_CASE_MAX     32
#define ICN_COMPOUND_MAX 32
#define ICN_LISTCON_MAX  64
typedef struct {
    DESCR_t disc; tree_t *clause_exprs[ICN_CASE_MAX]; tree_t *clause_bodies[ICN_CASE_MAX];
    int n_clauses; tree_t *dflt; int cur_clause; bb_node_t body_box; int body_started;
} icn_case_state_t;
typedef struct { tree_t *children[ICN_COMPOUND_MAX]; int n; bb_node_t last_box; int started; } icn_compound_state_t;
typedef struct {
    bb_node_t val_gen; bb_node_t left_gen; bb_node_t right_gen;
    tree_t *val_expr; tree_t *left_expr; tree_t *right_expr;
    icn_sec_kind_t kind; DESCR_t cur_val; DESCR_t cur_left;
    int val_started; int left_started; int right_started;
} icn_section_gen_state_t;
typedef struct { tree_t *children[ICN_LISTCON_MAX]; int n; int fired; } icn_listcon_state_t;
typedef struct {
    tree_t *proc; int body_start; int nbody; int stmt_idx;
    bb_node_t expr_box; int in_suspend; tree_t *suspend_body; int frame_popped;
} icn_proc_state_t;
typedef struct { icn_proc_state_t base; DESCR_t args[16]; int nargs; } icn_proc_call_state_t;
typedef struct { int dummy; }                              icn_noop_state_t;
typedef struct { long long val; }                          icn_intlit_state_t;
typedef struct { double val; }                             icn_reallit_state_t;
typedef struct { const char *s; }                          icn_strlit_state_t;
typedef struct { const char *s; }                          icn_csetlit_state_t;
typedef struct { int dummy; }                              icn_global_state_t;
typedef struct { tree_t *cond; tree_t *then_e; tree_t *else_e; } icn_if_state_t;
typedef struct { tree_t *body; }                           icn_initial_state_t;
typedef struct { int dummy; }                              icn_invocable_state_t;
typedef struct { int dummy; }                              icn_link_state_t;
typedef struct { const char *name; int nfields; }          icn_record_state_t;
typedef struct { tree_t *expr; }                           icn_return_state_t;
typedef struct { int dummy; }                              icn_fail_state_t;
typedef struct { const char *op; tree_t *operand; }        icn_unop_state_t;
typedef struct { int dummy; }                              icn_next_state_t;
typedef struct { tree_t *expr; }                           icn_break_state_t;
typedef struct { tree_t *expr; }                           icn_create_state_t;
typedef struct { int dummy; }                              icn_coexplist_state_t;
typedef struct { int dummy; }                              icn_arglist_state_t;
typedef struct { tree_t *proc; }                           icn_procdecl_state_t;
typedef struct { tree_t *body; }                           icn_procbody_state_t;
typedef struct { tree_t *init; tree_t *body; }             icn_proccode_state_t;
#endif
