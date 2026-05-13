
#ifndef EMIT_SM_SHAPE_H_
#define EMIT_SM_SHAPE_H_

#include <stdint.h>
#include <stdio.h>


typedef enum {
    SM_TPL_RTCALL = 0,
    SM_TPL_INT64,
    SM_TPL_LBL,
    SM_TPL_LBLOPT,
    SM_TPL_LBL_INT32,
    SM_TPL_LBLOPT_INT32,
    SM_TPL_LBLOPT3,
    SM_TPL_LBLOPT_I_I,
    SM_TPL_EXEC_VAR,
    SM_TPL_ARITH,
    SM_TPL_PCREF_JMP,
    SM_TPL_PCREF_COND,
    SM_TPL_PUSH_EXPRESSION,
    SM_TPL_CALL_EXPRESSION,
    SM_TPL_RET,
    SM_TPL_RET_VAR,
    SM_TPL_UNHANDLED,
    SM_TPL_NOOP,
    SM_TPL__COUNT
} sm_tpl_kind_t;


typedef struct {
    int             op;
    const char     *macro_name;
    const char     *runtime;
    sm_tpl_kind_t   kind;
    int             const_a;
    int             const_b;
} sm_op_template_t;

/* The lookup function: given an SM opcode, return the matching template, */
const sm_op_template_t *sm_template_lookup(int op);

const sm_op_template_t *sm_template_unhandled(void);

const sm_op_template_t *sm_template_ret_var(void);

int emit_sm_macro_library(FILE *out);

int emit_sm_macro_library_to_path(const char *path);

typedef struct {
    int64_t     i64;
    int         i32_a;
    int         i32_b;
    int         pc;
    const char *lbl;
    const char *lbl_b;
    const char *anno;
    const char *label;
} emit_sm_args_t;

int emit_sm_template(FILE *out, const sm_op_template_t *t,
                     const emit_sm_args_t *args);

int emit_sm_rtcall    (FILE *out, const sm_op_template_t *t,
                        const char *anno);
int emit_sm_int64      (FILE *out, const sm_op_template_t *t,
                        int64_t v, const char *anno);
int emit_sm_lbl        (FILE *out, const sm_op_template_t *t,
                        const char *lbl, const char *anno);
int emit_sm_lbl_int32  (FILE *out, const sm_op_template_t *t,
                        const char *lbl, int n, const char *anno);
int emit_sm_lblopt    (FILE *out, const sm_op_template_t *t,
                       const char *lbl_or_null, const char *anno);
int emit_sm_lblopt_int32(FILE *out, const sm_op_template_t *t,
                         const char *lbl_or_null, int n, const char *anno);
int emit_sm_arith      (FILE *out, const sm_op_template_t *t);
int emit_sm_pcref_jmp  (FILE *out, const sm_op_template_t *t,
                        int target_pc, const char *anno);
int emit_sm_pcref_cond (FILE *out, const sm_op_template_t *t,
                        int target_pc, int taken_when_ok,
                        const char *anno);
int edp4_emit_push_expression (FILE *out, const sm_op_template_t *t,
                        int64_t entry_pc, int arity);
int edp4_emit_call_expression (FILE *out, const sm_op_template_t *t,
                        int target_pc);
int emit_sm_ret        (FILE *out, const sm_op_template_t *t,
                        const char *anno);
int emit_sm_ret_var    (FILE *out, int kind, int cond, int pc,
                        const char *anno);
int emit_sm_unhandled  (FILE *out, int op);

int emit_sm_noop       (FILE *out, const sm_op_template_t *t,
                        const char *anno);

/* The exec-stmt-variant + capture-fn shapes have unique arg layouts; */
int emit_sm_exec_var   (FILE *out, const sm_op_template_t *t,
                        const char *subj_lbl_or_null, int has_repl);
int emit_sm_capture_fn (FILE *out, const sm_op_template_t *t,
                        const char *fname_lbl_or_null,
                        int is_imm,
                        const char *namelist_lbl_or_null,
                        const char *anno);
int emit_sm_capture_fn_args(FILE *out, const sm_op_template_t *t,
                            const char *fname_lbl_or_null,
                            int is_imm, int nargs,
                            const char *anno);

/* --------------------------------------------------------------------- */
int emit_sm_template_selftest(FILE *out);

void        emit_sm_set_pc_label(const char *lbl);
const char *emit_sm_consume_pc_label(void);

#endif
