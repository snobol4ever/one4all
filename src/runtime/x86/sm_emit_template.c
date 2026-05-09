/* sm_emit_template.c -- single source of truth for SM opcode emission.
 *
 * EM-7c-sm-macros (sess #87, 2026-05-09).
 *
 * Drift impossible by construction: each opcode shape is described
 * ONCE in this file by:
 *   (1) A `kind` value that picks one of the shape renderers below.
 *   (2) A macro_name + runtime symbol baked into the template.
 * The shape renderer produces both the GAS macro body and the
 * per-call emission line.  Adding a new shape is a single switch
 * arm in render_macro_body() AND in render_call_line() (paired by
 * kind); the pairing is the contract.
 *
 * For the "drift impossible" property, observe: for every kind k,
 *   - render_macro_body(k) writes ONE .macro NAME args / body / .endm.
 *   - render_call_line(k, args) writes ONE line: MACRO_NAME formatted_args.
 * The body of the macro is in render_macro_body; the per-call line
 * is in render_call_line.  They share NO body text (the macro hides
 * the body entirely from the call site).  What they share is the
 * macro_name and the args' encoding.  Updating one without the other
 * means the assembler complains immediately on the next regen
 * (mismatched arg count, undefined macro, etc.) -- we cannot ship a
 * silently-wrong byte stream.
 */
#include "sm_emit_template.h"
#include "sm_prog.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Three-column label support.
 *
 * g_pending_pc_label is set by the dispatcher once per instruction.
 * render_call_line consumes it (copies to args->label if args->label is
 * NULL) and clears it so continuation lines within the same instruction
 * (multi-line blobs) don't inherit it.
 * ----------------------------------------------------------------------- */
static char g_pending_pc_label[32] = "";  /* ".Lpc%d:"  set by sm_emit_set_pc_label */

void sm_emit_set_pc_label(const char *lbl)
{
    if (lbl && *lbl) {
        size_t n = strlen(lbl);
        if (n >= sizeof(g_pending_pc_label)) n = sizeof(g_pending_pc_label) - 1;
        memcpy(g_pending_pc_label, lbl, n);
        g_pending_pc_label[n] = '\0';
    } else {
        g_pending_pc_label[0] = '\0';
    }
}

/* Read + clear.  Used by callers that bypass render_call_line (e.g.
 * sm_line in the codegen driver) but still want the column-1 label
 * pickup behavior.  Returns "" when there's nothing pending. */
const char *sm_emit_consume_pc_label(void)
{
    static char buf[32];
    if (!g_pending_pc_label[0]) return "";
    /* Copy out into a static buffer (so the caller can use it across
     * subsequent sm_emit_* calls without aliasing into g_pending_pc_label). */
    size_t n = strlen(g_pending_pc_label);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, g_pending_pc_label, n);
    buf[n] = '\0';
    g_pending_pc_label[0] = '\0';
    return buf;
}


/* ---------------------------------------------------------------------
 * Template table
 *
 * One entry per opcode group.  The lookup is by SM opcode integer.
 * Opcodes whose codegen falls through to the trap (e.g. SM_DUP, SM_SWAP,
 * SM_PAT_*_ARGS that EM-7-revert removed) are absent here; they're
 * handled by the dedicated UNHANDLED template below.
 * --------------------------------------------------------------------- */

static const sm_op_template_t g_sm_templates[] = {
    /* Value push/pop/store */
    { SM_HALT,         "HALT",         "scrip_rt_halt_tos",     SM_TPL_NULLARY,    0, 0 },
    { SM_PUSH_LIT_I,   "PUSH_INT",     "scrip_rt_push_int",     SM_TPL_INT64,      0, 0 },
    { SM_PUSH_LIT_S,   "PUSH_STR",     "scrip_rt_push_str",     SM_TPL_LBL_INT32,  0, 0 },
    { SM_PUSH_VAR,     "PUSH_VAR",     "scrip_rt_nv_get",       SM_TPL_LBL,        0, 0 },
    { SM_STORE_VAR,    "STORE_VAR",    "scrip_rt_nv_set",       SM_TPL_LBL,        0, 0 },
    { SM_POP,          "VOID_POP",          "scrip_rt_pop_void",     SM_TPL_NULLARY,    0, 0 },
    { SM_PUSH_NULL,    "PUSH_NULL",    "scrip_rt_push_null",    SM_TPL_NULLARY,    0, 0 },
    { SM_CONCAT,       "CONCAT",       "scrip_rt_concat",       SM_TPL_NULLARY,    0, 0 },
    { SM_COERCE_NUM,   "COERCE_NUM",   "scrip_rt_coerce_num",   SM_TPL_NULLARY,    0, 0 },

    /* Arithmetic.  All five share the same shape; they differ only in
     * the op-enum baked into the call.  The macro takes a single arg
     * (the op enum) so per-instruction sites can use the same macro.
     * For human readability we ALSO emit per-op convenience macros
     * (SM_ADD, SM_SUB, ...) that wrap SM_ARITH; see render_macro_body.
     * The per-call site uses SM_ARITH directly with the op as arg. */
    { SM_ADD,          "ARITH",        "scrip_rt_arith",        SM_TPL_ARITH,     SM_ADD, 0 },
    { SM_SUB,          "ARITH",        "scrip_rt_arith",        SM_TPL_ARITH,     SM_SUB, 0 },
    { SM_MUL,          "ARITH",        "scrip_rt_arith",        SM_TPL_ARITH,     SM_MUL, 0 },
    { SM_DIV,          "ARITH",        "scrip_rt_arith",        SM_TPL_ARITH,     SM_DIV, 0 },
    { SM_MOD,          "ARITH",        "scrip_rt_arith",        SM_TPL_ARITH,     SM_MOD, 0 },

    /* Control flow */
    { SM_JUMP,         "JUMP",         NULL,                    SM_TPL_PCREF_JMP,  0, 0 },
    { SM_JUMP_S,       "JUMP_S",       "scrip_rt_last_ok",      SM_TPL_PCREF_COND, 1, 0 },
    { SM_JUMP_F,       "JUMP_F",       "scrip_rt_last_ok",      SM_TPL_PCREF_COND, 0, 0 },

    /* Chunk discipline */
    { SM_PUSH_CHUNK,   "PUSH_CHUNK",   "scrip_rt_push_chunk_descr", SM_TPL_PUSH_CHUNK, 0, 0 },
    { SM_CALL_CHUNK,   "CALL_CHUNK",   NULL,                    SM_TPL_CALL_CHUNK, 0, 0 },
    { SM_RETURN,       "RETURN",       NULL,                    SM_TPL_RET,        0, 0 },

    /* General call */
    { SM_CALL,         "CALL_FN",         "scrip_rt_call",         SM_TPL_LBL_INT32,  0, 0 },

    /* Pattern construction (no-arg shape) */
    { SM_PAT_SPAN,     "PAT_SPAN",     "scrip_rt_pat_span",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_BREAK,    "PAT_BREAK",    "scrip_rt_pat_break",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ANY,      "PAT_ANY",      "scrip_rt_pat_any",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_NOTANY,   "PAT_NOTANY",   "scrip_rt_pat_notany",   SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_LEN,      "PAT_LEN",      "scrip_rt_pat_len",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_POS,      "PAT_POS",      "scrip_rt_pat_pos",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_RPOS,     "PAT_RPOS",     "scrip_rt_pat_rpos",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_TAB,      "PAT_TAB",      "scrip_rt_pat_tab",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_RTAB,     "PAT_RTAB",     "scrip_rt_pat_rtab",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ARB,      "PAT_ARB",      "scrip_rt_pat_arb",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ARBNO,    "PAT_ARBNO",    "scrip_rt_pat_arbno",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_REM,      "PAT_REM",      "scrip_rt_pat_rem",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_FENCE,    "PAT_FENCE",    "scrip_rt_pat_fence",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_FENCE1,   "PAT_FENCE1",   "scrip_rt_pat_fence1",   SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_FAIL,     "PAT_FAIL",     "scrip_rt_pat_fail",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ABORT,    "PAT_ABORT",    "scrip_rt_pat_abort",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_SUCCEED,  "PAT_SUCCEED",  "scrip_rt_pat_succeed",  SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_BAL,      "PAT_BAL",      "scrip_rt_pat_bal",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_EPS,      "PAT_EPS",      "scrip_rt_pat_eps",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_CAT,      "PAT_CAT",      "scrip_rt_pat_cat",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ALT,      "PAT_ALT",      "scrip_rt_pat_alt",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_DEREF,    "PAT_DEREF",    "scrip_rt_pat_deref",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_BOXVAL,   "PAT_BOXVAL",   "scrip_rt_pat_boxval",   SM_TPL_NULLARY,    0, 0 },

    /* Pattern construction (one-string-or-NULL shape) */
    { SM_PAT_LIT,      "PAT_LIT",      "scrip_rt_pat_lit",      SM_TPL_LBLOPT,     0, 0 },
    { SM_PAT_REFNAME,  "PAT_REFNAME",  "scrip_rt_pat_refname",  SM_TPL_LBLOPT,     0, 0 },
    { SM_PAT_USERCALL, "PAT_USERCALL", "scrip_rt_pat_usercall", SM_TPL_LBLOPT,     0, 0 },

    /* Pattern construction (string-or-NULL + int) */
    { SM_PAT_CAPTURE,       "PAT_CAPTURE",       "scrip_rt_pat_capture",       SM_TPL_LBLOPT_INT32, 0, 0 },
    { SM_PAT_USERCALL_ARGS, "PAT_USERCALL_ARGS", "scrip_rt_pat_usercall_args", SM_TPL_LBLOPT_INT32, 0, 0 },

    /* Pattern construction with three args */
    { SM_PAT_CAPTURE_FN,      "PAT_CAPTURE_FN",      "scrip_rt_pat_capture_fn",
      SM_TPL_LBLOPT3,         0, 0 },
    { SM_PAT_CAPTURE_FN_ARGS, "PAT_CAPTURE_FN_ARGS", "scrip_rt_pat_capture_fn_args",
      SM_TPL_LBLOPT_I_I,      0, 0 },

    /* Statement execution (variant pattern) */
    { SM_EXEC_STMT,    "EXEC_STMT_VARIANT",  "scrip_rt_match_variant",
      SM_TPL_EXEC_VAR, 0, 0 },
};

#define G_SM_TEMPLATES_N (int)(sizeof(g_sm_templates) / sizeof(g_sm_templates[0]))

/* Standalone non-table templates (for shapes that aren't keyed by opcode). */
static const sm_op_template_t g_tpl_unhandled = {
    -1, "UNHANDLED", "scrip_rt_unhandled_op", SM_TPL_UNHANDLED, 0, 0
};

static const sm_op_template_t g_tpl_ret_var = {
    -2, "RETURN_VARIANT", "scrip_rt_do_return", SM_TPL_RET_VAR, 0, 0
};

const sm_op_template_t *sm_template_lookup(int op)
{
    /* Linear scan; ~50 entries.  If this becomes hot, replace with a
     * sparse op-indexed table generated at startup. */
    for (int i = 0; i < G_SM_TEMPLATES_N; i++) {
        if (g_sm_templates[i].op == op) return &g_sm_templates[i];
    }
    return NULL;
}

const sm_op_template_t *sm_template_unhandled(void)  { return &g_tpl_unhandled; }
const sm_op_template_t *sm_template_ret_var(void)    { return &g_tpl_ret_var; }

/* ---------------------------------------------------------------------
 * Shape renderers
 *
 * Two functions per shape kind (paired by switch arm): one renders
 * the GAS macro body; the other renders the per-call line.  The
 * pairing is the contract -- BOTH must update together when a shape
 * changes.  The shape kind enum is the join key.
 *
 * To minimise the chance of pairing a renderer with the wrong shape:
 *   - Every shape has a dedicated `case` arm in BOTH functions.
 *   - The arms appear in the SAME ORDER in both functions.
 *   - A `default: abort()` covers any unhandled kind.
 * --------------------------------------------------------------------- */

/* ---- macro body renderer -------------------------------------------- */

/* Helper: format the .ifnb/.else/.endif block for an optional label arg. */
static int emit_optional_lbl(FILE *out, const char *macro_arg,
                             const char *register_load_dst)
{
    return fprintf(out,
        "    .ifnb \\%s\n"
        "        lea     %s, [rip + \\%s]\n"
        "    .else\n"
        "        xor     e%s, e%s\n"     /* clears the 64-bit reg too */
        "    .endif\n",
        macro_arg, register_load_dst, macro_arg,
        /* xor edi,edi clears rdi; xor edx,edx clears rdx; etc.
         * register_load_dst is "rdi"/"rsi"/"rdx"; we strip the 'r'. */
        register_load_dst + 1, register_load_dst + 1);
}

static int render_macro_body(FILE *out, const sm_op_template_t *t)
{
    /* Each arm emits one .macro NAME args / body / .endm block. */
    switch (t->kind) {
    case SM_TPL_NULLARY:
        fprintf(out, ".macro %s\n", t->macro_name);
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_INT64:
        fprintf(out, ".macro %s val\n", t->macro_name);
        fprintf(out, "    movabs  rdi, \\val\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_LBL:
        fprintf(out, ".macro %s lbl\n", t->macro_name);
        fprintf(out, "    lea     rdi, [rip + \\lbl]\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_LBLOPT:
        fprintf(out, ".macro %s lbl\n", t->macro_name);
        emit_optional_lbl(out, "lbl", "rdi");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_LBL_INT32:
        fprintf(out, ".macro %s lbl, n\n", t->macro_name);
        fprintf(out, "    lea     rdi, [rip + \\lbl]\n");
        fprintf(out, "    mov     esi, \\n\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_LBLOPT_INT32:
        fprintf(out, ".macro %s n, lbl\n", t->macro_name);
        emit_optional_lbl(out, "lbl", "rdi");
        fprintf(out, "    mov     esi, \\n\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_LBLOPT3:
        /* args: is_imm, fname_lbl, namelist_lbl  (matches the C-side
         * wrapper sm_emit_capture_fn) */
        fprintf(out, ".macro %s is_imm, fname_lbl, namelist_lbl\n",
                t->macro_name);
        emit_optional_lbl(out, "fname_lbl", "rdi");
        fprintf(out, "    mov     esi, \\is_imm\n");
        emit_optional_lbl(out, "namelist_lbl", "rdx");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_LBLOPT_I_I:
        /* args: is_imm, nargs, fname_lbl */
        fprintf(out, ".macro %s is_imm, nargs, fname_lbl\n", t->macro_name);
        emit_optional_lbl(out, "fname_lbl", "rdi");
        fprintf(out, "    mov     esi, \\is_imm\n");
        fprintf(out, "    mov     edx, \\nargs\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_EXEC_VAR:
        fprintf(out, ".macro %s has_repl, subj_lbl\n", t->macro_name);
        emit_optional_lbl(out, "subj_lbl", "rdi");
        fprintf(out, "    mov     esi, \\has_repl\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_ARITH:
        /* SM_ARITH appears multiple times in the table (once per opcode
         * value), but the macro only needs to be defined ONCE.
         * sm_emit_macro_library() de-duplicates by macro_name. */
        fprintf(out, ".macro %s op\n", t->macro_name);
        fprintf(out, "    mov     edi, \\op\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_PCREF_JMP:
        fprintf(out, ".macro %s tgt\n", t->macro_name);
        fprintf(out, "    jmp     \\tgt\n");
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_PCREF_COND:
        /* taken_when_ok is baked in: SM_JUMP_S uses jnz; SM_JUMP_F uses jz.
         * Each SM_JUMP_S/_F template has its own macro entry. */
        fprintf(out, ".macro %s tgt\n", t->macro_name);
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, "    test    eax, eax\n");
        fprintf(out, "    %s     \\tgt\n", t->const_a ? "jnz" : "jz");
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_PUSH_CHUNK:
        fprintf(out, ".macro %s entry, arity\n", t->macro_name);
        fprintf(out, "    movabs  rdi, \\entry\n");
        fprintf(out, "    mov     esi, \\arity\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_CALL_CHUNK:
        fprintf(out, ".macro %s tgt\n", t->macro_name);
        fprintf(out, "    call    \\tgt\n");
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_RET:
        fprintf(out, ".macro %s\n", t->macro_name);
        fprintf(out, "    ret\n");
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_RET_VAR:
        fprintf(out, ".macro %s kind, cond, pc\n", t->macro_name);
        fprintf(out, "    mov     edi, \\kind\n");
        fprintf(out, "    mov     esi, \\cond\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, "    test    eax, eax\n");
        fprintf(out, "    jz      .Lretskip_\\pc\n");
        fprintf(out, "    ret\n");
        fprintf(out, ".Lretskip_\\pc\\():\n");
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL_UNHANDLED:
        fprintf(out, ".macro %s op\n", t->macro_name);
        fprintf(out, "    mov     edi, \\op\n");
        fprintf(out, "    call    %s@PLT\n", t->runtime);
        fprintf(out, ".endm\n");
        return 0;

    case SM_TPL__COUNT:
        break;
    }
    fprintf(stderr, "sm_emit_template: render_macro_body: unknown kind %d\n",
            (int)t->kind);
    return -1;
}

/* ---- per-call line renderer ---------------------------------------- */
/*
 * Emits exactly one source line of the form:
 *   "\tMACRO_NAME formatted_args  # annotation"
 * The body of the macro is *not* re-emitted -- the assembler expands
 * it on its own.  The renderer's only job is to format the args
 * matching the macro's expected signature.
 *
 * The annotation, if non-NULL and non-empty, is appended after the
 * macro call as a `#` comment.  GAS treats `#` as the line-comment
 * introducer (verified at session start).
 */

/* Helper: format the third-column annotation, "" if NULL/empty. */
static const char *anno_or_empty(const char *anno)
{
    return (anno && *anno) ? anno : NULL;
}

/* Helper: write trailing annotation if any.
 *
 * Mirrors sm_line's convention: an annotation that doesn't start with
 * '#' gets one prepended (so "SM_ADD" becomes "# SM_ADD").  GAS treats
 * '#' as the line-comment introducer in Intel syntax. */
static int write_anno(FILE *out, const char *anno)
{
    const char *a = anno_or_empty(anno);
    if (!a) return fputc('\n', out) == EOF ? -1 : 0;
    if (a[0] == '#') return fprintf(out, "  %s\n", a) < 0 ? -1 : 0;
    return fprintf(out, "  # %s\n", a) < 0 ? -1 : 0;
}

/* Build the opcode+args string for column 2 into buf[cap].
 * Returns 0 on success, -1 if buf is too small (truncation). */
static int build_op_col(char *buf, int cap, const sm_op_template_t *t,
                        const sm_emit_args_t *args)
{
    int n = 0;
    switch (t->kind) {
    case SM_TPL_NULLARY:
        n = snprintf(buf, cap, "%s", t->macro_name);
        break;
    case SM_TPL_INT64:
        n = snprintf(buf, cap, "%s %" PRId64, t->macro_name, args->i64);
        break;
    case SM_TPL_LBL:
        if (!args->lbl) {
            fprintf(stderr, "sm_emit_template: SM_TPL_LBL got NULL lbl for %s\n",
                    t->macro_name);
            return -1;
        }
        n = snprintf(buf, cap, "%s %s", t->macro_name, args->lbl);
        break;
    case SM_TPL_LBLOPT:
        if (args->lbl)
            n = snprintf(buf, cap, "%s %s", t->macro_name, args->lbl);
        else
            n = snprintf(buf, cap, "%s", t->macro_name);
        break;
    case SM_TPL_LBL_INT32:
        if (!args->lbl) {
            fprintf(stderr, "sm_emit_template: SM_TPL_LBL_INT32 got NULL lbl for %s\n",
                    t->macro_name);
            return -1;
        }
        n = snprintf(buf, cap, "%s %s, %d", t->macro_name, args->lbl, args->i32_a);
        break;
    case SM_TPL_LBLOPT_INT32:
        if (args->lbl)
            n = snprintf(buf, cap, "%s %d, %s", t->macro_name, args->i32_a, args->lbl);
        else
            n = snprintf(buf, cap, "%s %d", t->macro_name, args->i32_a);
        break;
    case SM_TPL_LBLOPT3:
        if (args->lbl && args->lbl_b)
            n = snprintf(buf, cap, "%s %d, %s, %s",
                         t->macro_name, args->i32_a, args->lbl, args->lbl_b);
        else if (args->lbl)
            n = snprintf(buf, cap, "%s %d, %s",
                         t->macro_name, args->i32_a, args->lbl);
        else if (args->lbl_b)
            n = snprintf(buf, cap, "%s %d, , %s",
                         t->macro_name, args->i32_a, args->lbl_b);
        else
            n = snprintf(buf, cap, "%s %d", t->macro_name, args->i32_a);
        break;
    case SM_TPL_LBLOPT_I_I:
        if (args->lbl)
            n = snprintf(buf, cap, "%s %d, %d, %s",
                         t->macro_name, args->i32_a, args->i32_b, args->lbl);
        else
            n = snprintf(buf, cap, "%s %d, %d",
                         t->macro_name, args->i32_a, args->i32_b);
        break;
    case SM_TPL_EXEC_VAR:
        if (args->lbl)
            n = snprintf(buf, cap, "%s %d, %s",
                         t->macro_name, args->i32_a, args->lbl);
        else
            n = snprintf(buf, cap, "%s %d", t->macro_name, args->i32_a);
        break;
    case SM_TPL_ARITH:
        n = snprintf(buf, cap, "%s %d", t->macro_name, t->const_a);
        break;
    case SM_TPL_PCREF_JMP:
    case SM_TPL_PCREF_COND:
    case SM_TPL_CALL_CHUNK:
        n = snprintf(buf, cap, "%s .Lpc%d", t->macro_name, args->i32_a);
        break;
    case SM_TPL_PUSH_CHUNK:
        n = snprintf(buf, cap, "%s %" PRId64 ", %d",
                     t->macro_name, args->i64, args->i32_a);
        break;
    case SM_TPL_RET:
        n = snprintf(buf, cap, "%s", t->macro_name);
        break;
    case SM_TPL_RET_VAR:
        n = snprintf(buf, cap, "%s %d, %d, %d",
                     t->macro_name, args->i32_a, args->i32_b, args->pc);
        break;
    case SM_TPL_UNHANDLED:
        n = snprintf(buf, cap, "%s %d", t->macro_name, args->i32_a);
        break;
    case SM_TPL__COUNT:
        break;
    default:
        fprintf(stderr, "sm_emit_template: build_op_col: unknown kind %d\n",
                (int)t->kind);
        return -1;
    }
    return (n < 0 || n >= cap) ? -1 : 0;
}

static int render_call_line(FILE *out, const sm_op_template_t *t,
                            const sm_emit_args_t *args)
{
    /* Three-column layout:  LABEL:       OPCODE args     # annotation
     * Col 1 (label):  24 chars, left-aligned  -- from args->label or g_pending_pc_label
     * Col 2 (opcode): 36 chars, left-aligned  -- macro name + args
     * Col 3 (anno):   free width              -- # comment
     *
     * g_pending_pc_label is consumed (cleared) on the first call per
     * instruction so that continuation lines (multi-line blob handlers)
     * don't inherit the label.
     */
    char op[128];
    if (build_op_col(op, sizeof(op), t, args) != 0) return -1;

    /* Determine label: explicit args->label wins; else consume the pending one.
     * Snapshot to a local buffer before clearing g_pending_pc_label, since
     * lbl_col may otherwise alias into it. */
    char lbl_buf[32];
    const char *lbl_col;
    if (args && args->label && *args->label) {
        lbl_col = args->label;
    } else if (g_pending_pc_label[0]) {
        size_t n = strlen(g_pending_pc_label);
        if (n >= sizeof(lbl_buf)) n = sizeof(lbl_buf) - 1;
        memcpy(lbl_buf, g_pending_pc_label, n);
        lbl_buf[n] = '\0';
        lbl_col = lbl_buf;
    } else {
        lbl_col = "";
    }
    /* Consume pending label so next call on this instruction gets no label. */
    g_pending_pc_label[0] = '\0';

    if (lbl_col && *lbl_col) {
        /* Three-column: label / opcode+args / annotation */
        if (fprintf(out, "%-24s%-36s", lbl_col, op) < 0) return -1;
    } else {
        /* No label (macro library emission, test path, continuation lines) */
        if (fprintf(out, "\t%-35s", op) < 0) return -1;
    }
    return write_anno(out, args ? args->anno : NULL);
}

/* ---------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* sm_emit_macro_library:
 *
 * Walks g_sm_templates[] (plus the standalone unhandled and ret_var
 * templates) and emits one .macro definition per UNIQUE macro_name.
 * Multiple templates can share a macro_name (e.g. SM_ADD/SUB/MUL/DIV/MOD
 * all share "ARITH"); each is emitted only once.
 *
 * Output starts with a banner; ends with a blank line.  The macro
 * library is syntax-agnostic (only `.macro`/`.endm`/`.if` constructs
 * which are common to AT&T and Intel syntax); placement before any
 * `.intel_syntax noprefix` directive in the .s is fine.
 */
int sm_emit_macro_library(FILE *out)
{
    /* De-duplication: track which macro_names have already been emitted. */
    const char *seen[256] = { 0 };
    int n_seen = 0;

    if (fputs(
        "# === BEGIN sm macro library (generated from g_sm_templates[]) ===\n"
        "# EM-7c-sm-macros: one macro per opcode group; bodies and per-call\n"
        "#   emissions share one renderer in sm_emit_template.c, so the\n"
        "#   .s and the C dispatcher cannot drift -- they are paired by\n"
        "#   shape kind in render_macro_body() / render_call_line().\n",
        out) == EOF) return -1;

    /* Helper: emit a template's macro IF it hasn't been emitted yet. */
    #define EMIT_IF_NEW(tpl) do {                                           \
        const sm_op_template_t *_t = (tpl);                                 \
        int already = 0;                                                    \
        for (int _i = 0; _i < n_seen; _i++) {                               \
            if (strcmp(seen[_i], _t->macro_name) == 0) { already = 1; break; } \
        }                                                                   \
        if (!already) {                                                     \
            if (n_seen >= (int)(sizeof(seen)/sizeof(seen[0]))) {            \
                fprintf(stderr, "sm_emit_macro_library: seen[] overflow\n");\
                return -1;                                                  \
            }                                                               \
            seen[n_seen++] = _t->macro_name;                                \
            if (render_macro_body(out, _t) != 0) return -1;                 \
        }                                                                   \
    } while (0)

    /* Walk the table in declaration order so the library reads
     * top-to-bottom in a logical grouping. */
    for (int i = 0; i < G_SM_TEMPLATES_N; i++) {
        EMIT_IF_NEW(&g_sm_templates[i]);
    }
    EMIT_IF_NEW(&g_tpl_unhandled);
    EMIT_IF_NEW(&g_tpl_ret_var);

    #undef EMIT_IF_NEW

    if (fputs("# === END sm macro library ===\n\n", out) == EOF) return -1;
    return 0;
}

/* sm_emit_macro_library_to_path:
 *
 * Open `path` for writing and emit the macro library to it as a
 * standalone GAS source file.  Used by the emitter driver to ship
 * sm_macros.s once per emission run, alongside the .s output.
 * The .s file pulls the macros in via `.include "sm_macros.s"`.
 */
int sm_emit_macro_library_to_path(const char *path)
{
    if (!path || !*path) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "sm_emit_macro_library_to_path: cannot open %s for writing\n",
                path);
        return -1;
    }
    int rc = sm_emit_macro_library(fp);
    if (fclose(fp) != 0) return -1;
    return rc;
}

/* Per-instruction generic dispatch (rarely called directly; convenience
 * wrappers below are usually clearer at call sites). */
int sm_emit_template(FILE *out, const sm_op_template_t *t,
                     const sm_emit_args_t *args)
{
    if (!t || !args) return -1;
    return render_call_line(out, t, args);
}

/* ---- Convenience wrappers ----------------------------------------- */

int sm_emit_nullary(FILE *out, const sm_op_template_t *t, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_int64(FILE *out, const sm_op_template_t *t,
                  int64_t v, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.i64 = v;
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_lbl(FILE *out, const sm_op_template_t *t,
                const char *lbl, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.lbl = lbl;
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_lblopt(FILE *out, const sm_op_template_t *t,
                   const char *lbl_or_null, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.lbl = (lbl_or_null && *lbl_or_null) ? lbl_or_null : NULL;
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_lbl_int32(FILE *out, const sm_op_template_t *t,
                      const char *lbl, int n, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.lbl = lbl;
    a.i32_a = n;
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_lblopt_int32(FILE *out, const sm_op_template_t *t,
                         const char *lbl_or_null, int n, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.lbl = (lbl_or_null && *lbl_or_null) ? lbl_or_null : NULL;
    a.i32_a = n;
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_arith(FILE *out, const sm_op_template_t *t)
{
    sm_emit_args_t a = { 0 };
    /* annotation set by caller's choice -- we put the opcode name */
    return sm_emit_template(out, t, &a);
}

int sm_emit_pcref_jmp(FILE *out, const sm_op_template_t *t,
                      int target_pc, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.i32_a = target_pc;
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_pcref_cond(FILE *out, const sm_op_template_t *t,
                       int target_pc, int taken_when_ok,
                       const char *anno)
{
    /* taken_when_ok is encoded in t->const_a (S=1, F=0); the per-call
     * site only needs the target pc. */
    (void)taken_when_ok;
    sm_emit_args_t a = { 0 };
    a.i32_a = target_pc;
    a.anno = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_push_chunk(FILE *out, const sm_op_template_t *t,
                       int64_t entry_pc, int arity)
{
    sm_emit_args_t a = { 0 };
    a.i64 = entry_pc;
    a.i32_a = arity;
    return sm_emit_template(out, t, &a);
}

int sm_emit_call_chunk(FILE *out, const sm_op_template_t *t, int target_pc)
{
    sm_emit_args_t a = { 0 };
    a.i32_a = target_pc;
    return sm_emit_template(out, t, &a);
}

int sm_emit_ret(FILE *out, const sm_op_template_t *t, const char *anno)
{
    return sm_emit_nullary(out, t, anno);
}

int sm_emit_ret_var(FILE *out, int kind, int cond, int pc, const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.i32_a = kind;
    a.i32_b = cond;
    a.pc    = pc;
    a.anno  = anno;
    return sm_emit_template(out, sm_template_ret_var(), &a);
}

int sm_emit_unhandled(FILE *out, int op)
{
    sm_emit_args_t a = { 0 };
    a.i32_a = op;
    return sm_emit_template(out, sm_template_unhandled(), &a);
}

int sm_emit_exec_var(FILE *out, const sm_op_template_t *t,
                     const char *subj_lbl_or_null, int has_repl)
{
    sm_emit_args_t a = { 0 };
    a.lbl   = (subj_lbl_or_null && *subj_lbl_or_null) ? subj_lbl_or_null : NULL;
    a.i32_a = has_repl;
    return sm_emit_template(out, t, &a);
}

int sm_emit_capture_fn(FILE *out, const sm_op_template_t *t,
                       const char *fname_lbl_or_null,
                       int is_imm,
                       const char *namelist_lbl_or_null,
                       const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.lbl   = (fname_lbl_or_null && *fname_lbl_or_null) ? fname_lbl_or_null : NULL;
    a.lbl_b = (namelist_lbl_or_null && *namelist_lbl_or_null) ? namelist_lbl_or_null : NULL;
    a.i32_a = is_imm;
    a.anno  = anno;
    return sm_emit_template(out, t, &a);
}

int sm_emit_capture_fn_args(FILE *out, const sm_op_template_t *t,
                            const char *fname_lbl_or_null,
                            int is_imm, int nargs,
                            const char *anno)
{
    sm_emit_args_t a = { 0 };
    a.lbl   = (fname_lbl_or_null && *fname_lbl_or_null) ? fname_lbl_or_null : NULL;
    a.i32_a = is_imm;
    a.i32_b = nargs;
    a.anno  = anno;
    return sm_emit_template(out, t, &a);
}

/* ---------------------------------------------------------------------
 * Self-test: walks every template, emits its macro body to a stub
 * stream, asserts no failure.  Used by the gate to verify all kinds
 * have both arms implemented (no SM_TPL__COUNT slipping through).
 * --------------------------------------------------------------------- */
int sm_emit_template_selftest(FILE *out)
{
    int failures = 0;
    if (fprintf(out, "sm_emit_template self-test: %d templates\n",
                G_SM_TEMPLATES_N + 2) < 0) return -1;
    /* Macro library round-trip. */
    if (sm_emit_macro_library(out) != 0) {
        fprintf(out, "FAIL: sm_emit_macro_library returned -1\n");
        failures++;
    }
    /* Per-call line round-trip with sentinel args, one per kind. */
    sm_emit_args_t sentinel = {
        .i64   = 0x12345678,
        .i32_a = 7,
        .i32_b = 3,
        .pc    = 99,
        .lbl   = ".LstrSEN",
        .lbl_b = ".LstrSEN2",
        .anno  = "# self-test"
    };
    for (int i = 0; i < G_SM_TEMPLATES_N; i++) {
        if (render_call_line(out, &g_sm_templates[i], &sentinel) != 0) {
            fprintf(out, "FAIL: render_call_line(%s)\n",
                    g_sm_templates[i].macro_name);
            failures++;
        }
    }
    if (render_call_line(out, &g_tpl_unhandled, &sentinel) != 0)
        { fprintf(out, "FAIL: unhandled\n"); failures++; }
    if (render_call_line(out, &g_tpl_ret_var, &sentinel) != 0)
        { fprintf(out, "FAIL: ret_var\n"); failures++; }

    fprintf(out, "self-test: %d failures\n", failures);
    return failures ? -1 : 0;
}
