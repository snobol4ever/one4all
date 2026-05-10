/* sm_emit_template.h -- single source of truth for SM opcode emission.
 *
 * EM-7c-sm-macros (sess #87, 2026-05-09).
 *
 * Each SM opcode binds to ONE template.  A template names an arg-shape
 * (NULLARY, INT64, LBL_INT32, …) plus a `runtime` symbol (the libscrip_rt.so
 * entry that the macro calls into) plus a unique `macro` name.  ONE
 * renderer per shape produces:
 *   (a) the GAS macro body that goes into the macro library at the top
 *       of every emitted .s file (sm_emit_macro_library); and
 *   (b) the per-instruction emission line at every dispatch site
 *       (sm_emit_template).
 *
 * Both renderings come from ONE format string per shape.  The macro
 * body and the per-call expansion cannot drift — they share one
 * source of truth, by construction.
 *
 * Adding a new SM opcode group: declare a new template (or reuse an
 * existing arg-shape), add an entry to g_sm_templates[], and call
 * sm_emit_template() from the dispatcher.  No change anywhere else.
 *
 * Author: Claude Sonnet (with Lon Jones Cherryholmes).
 */
#ifndef SM_EMIT_TEMPLATE_H_
#define SM_EMIT_TEMPLATE_H_

#include <stdint.h>
#include <stdio.h>

/* ---------------------------------------------------------------------
 * Arg-shape kinds.
 *
 * Each kind names a fixed asm sequence parameterised over typed args.
 * The renderer lives in sm_emit_template.c and produces both macro
 * body and per-call line from the same body string.
 *
 * NULLARY    -- no args; body is "    call \rt@PLT"
 * INT64      -- i64 v;    body loads v into rdi (movabs), calls runtime
 * LBL        -- str L;    body loads .Lstr_N RIP-relative into rdi
 * LBL_INT32  -- str L, i32 n; body sets rdi=L, esi=n
 * LBLOPT_INT32 -- optstr L, i32 n; rdi=L|0, esi=n  (PAT_CAPTURE)
 * LBLOPT3    -- optstr L1, i32 n, optstr L2; PAT_CAPTURE_FN
 * LBLOPT_I_I -- optstr L,  i32 n1, i32 n2;  PAT_CAPTURE_FN_ARGS
 * EXEC_VAR   -- optstr L,  i32 has_repl;    SM_EXEC_STMT (variant)
 * ARITH      -- i32 op_enum;   SM_ADD..SM_MOD
 * PCREF_JMP  -- int target_pc; SM_JUMP
 * PCREF_COND -- int target_pc, int taken_when_ok; SM_JUMP_S / SM_JUMP_F
 * PUSH_EXPRESSION -- i64 entry, i32 arity
 * CALL_EXPRESSION -- int target_pc
 * RET        -- no args; emits `ret`
 * RET_VAR    -- i32 kind, i32 cond, int pc; conditional return shape
 * UNHANDLED  -- i32 op_enum; trap shape
 * --------------------------------------------------------------------- */
typedef enum {
    SM_TPL_NULLARY = 0,
    SM_TPL_INT64,
    SM_TPL_LBL,
    SM_TPL_LBLOPT,           /* label-or-null, no extra int */
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
    SM_TPL_NOOP,             /* no-arg, no-body marker (e.g. SM_LABEL, SM_STNO).
                                 macro body is `.macro N\n.endm`; the per-call
                                 line is one three-column line carrying the
                                 macro name in col 2 so the .LpcN: prefix is
                                 never naked. */
    SM_TPL__COUNT
} sm_tpl_kind_t;

/* One template per opcode group.  A single g_sm_templates[] entry
 * binds an opcode integer to a {macro_name, runtime_symbol, kind}
 * triple; the runtime_symbol is the libscrip_rt.so PLT entry the
 * macro calls.  For shapes that don't take a runtime arg (RET,
 * PCREF_JMP, PCREF_COND, CALL_EXPRESSION, RET_VAR), runtime is NULL.
 */
typedef struct {
    int             op;            /* sm_opcode_t value (or -1 for the
                                     trap template, indexed separately) */
    const char     *macro_name;    /* "SM_PUSH_INT" etc. */
    const char     *runtime;       /* "rt_push_int" or NULL */
    sm_tpl_kind_t   kind;
    /* Optional per-opcode integer constant baked into the call (e.g.
     * SM_ARITH op-enum, SM_RETURN_VARIANT kind/cond, UNHANDLED op). */
    int             const_a;
    int             const_b;
} sm_op_template_t;

/* The lookup function: given an SM opcode, return the matching template,
 * or NULL if unknown.  Constant time (linear scan; small N). */
const sm_op_template_t *sm_template_lookup(int op);

/* The trap template (used for opcodes without their own entry). */
const sm_op_template_t *sm_template_unhandled(void);

/* The RET_VAR template (used for SM_RETURN_S/F, SM_FRETURN[_S/_F],
 * SM_NRETURN[_S/_F] -- their kind/cond is computed from the opcode
 * by the caller and passed as args, not via const_a/const_b on the
 * template itself). */
const sm_op_template_t *sm_template_ret_var(void);

/* ---------------------------------------------------------------------
 * Renderers.
 *
 * Each shape has ONE renderer that produces both the macro body and
 * the per-call emission line.  They share one printf-style format
 * string per shape (defined in sm_emit_template.c).
 * --------------------------------------------------------------------- */

/* Emit the entire macro library (one .macro/.endm block per
 * template) at the top of an emitted .s file.  Generated from
 * g_sm_templates[].  Called once by sm_codegen_x64_emit at the
 * very start of emission, before .rodata / .data / .text. */
int sm_emit_macro_library(FILE *out);

/* Emit the SM macro library to a standalone file at `path`.
 * The emitted .s files use `.include "sm_macros.s"` to pick up the
 * macros from this header file at assembly time, rather than each
 * .s carrying its own ~200-line copy.  Returns 0 on success, -1 on
 * I/O error.  Overwrites any existing file. */
int sm_emit_macro_library_to_path(const char *path);

/* Emit one macro-call line for a given template at the dispatch site.
 * The `args` carry the per-instruction values.  Returns 0 on
 * success, -1 on I/O error. */
typedef struct {
    int64_t     i64;          /* INT64 / PUSH_EXPRESSION entry */
    int         i32_a;        /* arity / slen / nargs / target_pc / taken / kind */
    int         i32_b;        /* nargs / cond / 2nd int arg */
    int         pc;           /* RET_VAR uses for unique skip-label */
    const char *lbl;          /* primary strtab label (NULL = use xor edi,edi) */
    const char *lbl_b;        /* secondary strtab label */
    const char *anno;         /* optional annotation (may be NULL or "") */
    const char *label;        /* column-1 PC label, e.g. ".Lpc13:" (NULL = no label) */
} sm_emit_args_t;

int sm_emit_template(FILE *out, const sm_op_template_t *t,
                     const sm_emit_args_t *args);

/* Convenience: fill-and-emit for the hottest shapes, so the dispatcher
 * site doesn't have to construct sm_emit_args_t every time. */
int sm_emit_nullary    (FILE *out, const sm_op_template_t *t,
                        const char *anno);
int sm_emit_int64      (FILE *out, const sm_op_template_t *t,
                        int64_t v, const char *anno);
int sm_emit_lbl        (FILE *out, const sm_op_template_t *t,
                        const char *lbl, const char *anno);
int sm_emit_lbl_int32  (FILE *out, const sm_op_template_t *t,
                        const char *lbl, int n, const char *anno);
int sm_emit_lblopt    (FILE *out, const sm_op_template_t *t,
                       const char *lbl_or_null, const char *anno);
int sm_emit_lblopt_int32(FILE *out, const sm_op_template_t *t,
                         const char *lbl_or_null, int n, const char *anno);
int sm_emit_arith      (FILE *out, const sm_op_template_t *t);
int sm_emit_pcref_jmp  (FILE *out, const sm_op_template_t *t,
                        int target_pc, const char *anno);
int sm_emit_pcref_cond (FILE *out, const sm_op_template_t *t,
                        int target_pc, int taken_when_ok,
                        const char *anno);
int sm_emit_push_expression (FILE *out, const sm_op_template_t *t,
                        int64_t entry_pc, int arity);
int sm_emit_call_expression (FILE *out, const sm_op_template_t *t,
                        int target_pc);
int sm_emit_ret        (FILE *out, const sm_op_template_t *t,
                        const char *anno);
int sm_emit_ret_var    (FILE *out, int kind, int cond, int pc,
                        const char *anno);
int sm_emit_unhandled  (FILE *out, int op);

/* SM_TPL_NOOP — emit a labelled three-column line carrying the macro
 * name in col 2 with empty col 3.  Used by SM_LABEL and SM_STNO so
 * the pending .LpcN: pc-label is consumed and never appears naked. */
int sm_emit_noop       (FILE *out, const sm_op_template_t *t,
                        const char *anno);

/* The exec-stmt-variant + capture-fn shapes have unique arg layouts;
 * exposed as their own thin entries for dispatcher clarity. */
int sm_emit_exec_var   (FILE *out, const sm_op_template_t *t,
                        const char *subj_lbl_or_null, int has_repl);
int sm_emit_capture_fn (FILE *out, const sm_op_template_t *t,
                        const char *fname_lbl_or_null,
                        int is_imm,
                        const char *namelist_lbl_or_null,
                        const char *anno);
int sm_emit_capture_fn_args(FILE *out, const sm_op_template_t *t,
                            const char *fname_lbl_or_null,
                            int is_imm, int nargs,
                            const char *anno);

/* ---------------------------------------------------------------------
 * Diagnostic / self-test.
 *
 * Walks every template, emits its macro body, AND emits a per-call
 * expansion using sentinel arg values; asserts both texts share the
 * same call signature.  Used by test_smoke_jit_emit_x64.sh.
 * --------------------------------------------------------------------- */
int sm_emit_template_selftest(FILE *out);

/* ---------------------------------------------------------------------
 * Three-column label support.
 *
 * The dispatcher calls sm_emit_set_pc_label(".Lpc13:") once per
 * instruction before calling any sm_emit_* function.  The label is
 * consumed as column 1 by the first sm_emit_* call, then cleared.
 * Subsequent calls (multi-line blobs, raw sm_line calls) get no label.
 *
 * sm_emit_consume_pc_label() returns the pending label (or "") and
 * clears it — used by external emitters (e.g. sm_line in the codegen
 * driver) that bypass sm_emit_template but still want the column-1
 * label pickup for the first line they emit per instruction.
 * --------------------------------------------------------------------- */
void        sm_emit_set_pc_label(const char *lbl);   /* lbl copied; pass NULL to clear */
const char *sm_emit_consume_pc_label(void);          /* read+clear; returns "" if none */

#endif /* SM_EMIT_TEMPLATE_H_ */
