#ifndef ICON_GEN_MISSING_H
#define ICON_GEN_MISSING_H
/*============================================================================================================================
 * icon_gen_missing.h -- state types and allocators for the missing JCON BBs (IJ-18..28)
 *
 * Exposes the coro_bb_* semantic functions and their *_new() allocators
 * so emit_bb.c can wire them into the x86 emitter template system.
 *============================================================================================================================*/

#include "../../frontend/icon/icon_gen.h"  /* bb_node_t, DESCR_t, alpha, beta */
#include <stdlib.h>

/* --- State structs --- */
typedef struct { tree_t *expr; int frame_popped; } icn_not_state_t;

typedef struct { tree_t *expr; int started; int ever_succeeded; bb_node_t inner; } icn_repalt_state_t;

typedef struct { tree_t *expr; tree_t *body; } icn_while_state_t;
typedef struct { tree_t *expr; tree_t *body; } icn_until_state_t;
typedef struct { tree_t *body; } icn_repeat_state_t;

#define ICN_CASE_MAX 32
typedef struct {
    DESCR_t   disc;
    tree_t   *clause_exprs[ICN_CASE_MAX];
    tree_t   *clause_bodies[ICN_CASE_MAX];
    int       n_clauses;
    tree_t   *dflt;
    int       cur_clause;
    bb_node_t body_box;
    int       body_started;
} icn_case_state_t;

#define ICN_COMPOUND_MAX 32
typedef struct {
    tree_t   *children[ICN_COMPOUND_MAX];
    int       n;
    bb_node_t last_box;
    int       started;
} icn_compound_state_t;

typedef struct { bb_node_t obj_gen; const char *field; } icn_field_gen_state_t;

typedef enum { ICN_SEC_RANGE, ICN_SEC_PLUS, ICN_SEC_MINUS } icn_sec_kind_t;
typedef struct {
    bb_node_t    val_gen; bb_node_t left_gen; bb_node_t right_gen;
    tree_t      *val_expr; tree_t *left_expr; tree_t *right_expr;
    icn_sec_kind_t kind;
    DESCR_t      cur_val; DESCR_t cur_left;
    int          val_started; int left_started; int right_started;
} icn_section_gen_state_t;

typedef struct { const char *kw; int fired; DESCR_t val; } icn_kw_gen_state_t;

#define ICN_LISTCON_MAX 64
typedef struct { tree_t *children[ICN_LISTCON_MAX]; int n; int fired; } icn_listcon_state_t;

/* icn_proc_state_t and icn_proc_call_state_t defined in icon_gen_missing.c */
struct icn_proc_state;
typedef struct icn_proc_call_state icn_proc_call_state_t;

/* --- Semantic BB functions --- */
DESCR_t coro_bb_not         (void *zeta, int entry);
DESCR_t coro_bb_repalt      (void *zeta, int entry);
DESCR_t coro_bb_while_gen   (void *zeta, int entry);
DESCR_t coro_bb_until_gen   (void *zeta, int entry);
DESCR_t coro_bb_repeat_gen  (void *zeta, int entry);
DESCR_t coro_bb_case_gen    (void *zeta, int entry);
DESCR_t coro_bb_compound_gen(void *zeta, int entry);
DESCR_t coro_bb_field_gen   (void *zeta, int entry);
DESCR_t coro_bb_section_gen (void *zeta, int entry);
DESCR_t coro_bb_key_gen     (void *zeta, int entry);
DESCR_t coro_bb_listcon_gen (void *zeta, int entry);
DESCR_t icn_bb_proc_call    (void *zeta, int entry);

/* --- State allocators (for emit_bb.c wiring) --- */
icn_not_state_t         *icon_not_new(void);
icn_repalt_state_t      *icon_repalt_new(void);
icn_while_state_t       *icon_while_gen_new(void);
icn_until_state_t       *icon_until_gen_new(void);
icn_repeat_state_t      *icon_repeat_gen_new(void);
icn_case_state_t        *icon_case_gen_new(void);
icn_compound_state_t    *icon_compound_gen_new(void);
icn_field_gen_state_t   *icon_field_gen_new(void);
icn_section_gen_state_t *icon_section_gen_new(void);
icn_kw_gen_state_t      *icon_kw_gen_new(void);
icn_listcon_state_t     *icon_listcon_gen_new(void);
icn_proc_call_state_t   *icon_proc_call_new(void);

/* --- emit_bb.c entry points (declared here for reference) --- */
void emit_bb_icon_not        (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_repalt     (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_while_gen  (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_until_gen  (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_repeat_gen (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_case_gen   (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_compound_gen(bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_field_gen  (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_section_gen(bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_kw_gen     (bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_listcon_gen(bb_label_t *s, bb_label_t *f, bb_label_t *b);
void emit_bb_icon_proc_call  (bb_label_t *s, bb_label_t *f, bb_label_t *b);

#endif /* ICON_GEN_MISSING_H */
