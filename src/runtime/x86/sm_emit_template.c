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
    { SM_HALT,         "HALT",         "rt_halt_tos",     SM_TPL_NULLARY,    0, 0 },
    { SM_PUSH_LIT_I,   "PUSH_INT",     "rt_push_int",     SM_TPL_INT64,      0, 0 },
    { SM_PUSH_LIT_S,   "PUSH_STR",     "rt_push_str",     SM_TPL_LBL_INT32,  0, 0 },
    { SM_PUSH_VAR,     "PUSH_VAR",     "rt_nv_get",       SM_TPL_LBL,        0, 0 },
    { SM_STORE_VAR,    "STORE_VAR",    "rt_nv_set",       SM_TPL_LBL,        0, 0 },
    { SM_VOID_POP,          "VOID_POP",          "rt_pop_void",     SM_TPL_NULLARY,    0, 0 },
    { SM_PUSH_NULL,    "PUSH_NULL",    "rt_push_null",    SM_TPL_NULLARY,    0, 0 },
    { SM_CONCAT,       "CONCAT",       "rt_concat",       SM_TPL_NULLARY,    0, 0 },
    { SM_COERCE_NUM,   "COERCE_NUM",   "rt_coerce_num",   SM_TPL_NULLARY,    0, 0 },

    /* Arithmetic.  EM-7c follow-up: each op gets its own named no-arg
     * macro.  Op-enum is baked into the macro body via t->const_a, not
     * passed at the call site, so col 2 carries the human-readable name
     * directly — no opaque integer + # annotation.  All six map to one
     * runtime helper (rt_arith).
     *
     * Suffix `_NUM` disambiguates from x86 mnemonics `add`, `sub`, `mul`,
     * `div` (GAS macro-name match is case-insensitive against mnemonics).
     * Same disambiguator pattern as VOID_POP (SM POP vs x86 pop) and
     * CALL_FN (SM CALL vs x86 call) from the prior rung. */
    { SM_ADD,          "ADD_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_ADD, 0 },
    { SM_SUB,          "SUB_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_SUB, 0 },
    { SM_MUL,          "MUL_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_MUL, 0 },
    { SM_DIV,          "DIV_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_DIV, 0 },
    { SM_MOD,          "MOD_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_MOD, 0 },

    /* Control flow */
    { SM_JUMP,         "JUMP",         NULL,                    SM_TPL_PCREF_JMP,  0, 0 },
    { SM_JUMP_S,       "JUMP_S",       "rt_last_ok",      SM_TPL_PCREF_COND, 1, 0 },
    { SM_JUMP_F,       "JUMP_F",       "rt_last_ok",      SM_TPL_PCREF_COND, 0, 0 },

    /* Expression discipline */
    { SM_PUSH_EXPRESSION,   "PUSH_CHUNK",   "rt_push_expression_descr", SM_TPL_PUSH_CHUNK, 0, 0 },
    { SM_CALL_EXPRESSION,   "CALL_CHUNK",   NULL,                    SM_TPL_CALL_CHUNK, 0, 0 },
    { SM_RETURN,       "RETURN",       NULL,                    SM_TPL_RET,        0, 0 },

    /* General call */
    { SM_CALL_FN,         "CALL_FN",         "rt_call",         SM_TPL_LBL_INT32,  0, 0 },

    /* Pattern construction (no-arg shape) */
    { SM_PAT_SPAN,     "PAT_SPAN",     "rt_pat_span",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_BREAK,    "PAT_BREAK",    "rt_pat_break",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ANY,      "PAT_ANY",      "rt_pat_any",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_NOTANY,   "PAT_NOTANY",   "rt_pat_notany",   SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_LEN,      "PAT_LEN",      "rt_pat_len",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_POS,      "PAT_POS",      "rt_pat_pos",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_RPOS,     "PAT_RPOS",     "rt_pat_rpos",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_TAB,      "PAT_TAB",      "rt_pat_tab",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_RTAB,     "PAT_RTAB",     "rt_pat_rtab",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ARB,      "PAT_ARB",      "rt_pat_arb",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ARBNO,    "PAT_ARBNO",    "rt_pat_arbno",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_REM,      "PAT_REM",      "rt_pat_rem",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_FENCE,    "PAT_FENCE",    "rt_pat_fence",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_FENCE1,   "PAT_FENCE1",   "rt_pat_fence1",   SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_FAIL,     "PAT_FAIL",     "rt_pat_fail",     SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ABORT,    "PAT_ABORT",    "rt_pat_abort",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_SUCCEED,  "PAT_SUCCEED",  "rt_pat_succeed",  SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_BAL,      "PAT_BAL",      "rt_pat_bal",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_EPS,      "PAT_EPS",      "rt_pat_eps",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_CAT,      "PAT_CAT",      "rt_pat_cat",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_ALT,      "PAT_ALT",      "rt_pat_alt",      SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_DEREF,    "PAT_DEREF",    "rt_pat_deref",    SM_TPL_NULLARY,    0, 0 },
    { SM_PAT_BOXVAL,   "PAT_BOXVAL",   "rt_pat_boxval",   SM_TPL_NULLARY,    0, 0 },

    /* Pattern construction (one-string-or-NULL shape) */
    { SM_PAT_LIT,      "PAT_LIT",      "rt_pat_lit",      SM_TPL_LBLOPT,     0, 0 },
    { SM_PAT_REFNAME,  "PAT_REFNAME",  "rt_pat_refname",  SM_TPL_LBLOPT,     0, 0 },
    { SM_PAT_USERCALL, "PAT_USERCALL", "rt_pat_usercall", SM_TPL_LBLOPT,     0, 0 },

    /* Pattern construction (string-or-NULL + int) */
    { SM_PAT_CAPTURE,       "PAT_CAPTURE",       "rt_pat_capture",       SM_TPL_LBLOPT_INT32, 0, 0 },
    { SM_PAT_USERCALL_ARGS, "PAT_USERCALL_ARGS", "rt_pat_usercall_args", SM_TPL_LBLOPT_INT32, 0, 0 },

    /* Pattern construction with three args */
    { SM_PAT_CAPTURE_FN,      "PAT_CAPTURE_FN",      "rt_pat_capture_fn",
      SM_TPL_LBLOPT3,         0, 0 },
    { SM_PAT_CAPTURE_FN_ARGS, "PAT_CAPTURE_FN_ARGS", "rt_pat_capture_fn_args",
      SM_TPL_LBLOPT_I_I,      0, 0 },

    /* Statement execution (variant pattern) */
    { SM_EXEC_STMT,    "EXEC_STMT_VARIANT",  "rt_match_variant",
      SM_TPL_EXEC_VAR, 0, 0 },

    /* Statement-boundary / no-op markers (EM-7c-stmt-banner-fidelity).
     * These opcodes carry no executable code but mark a position in
     * the pc tape.  Rendering them as a real three-column line keeps
     * the .LpcN: label out of the "naked" failure mode — col 2
     * declares which marker executes here, even though the macro
     * body is empty. */
    { SM_LABEL,        "LABEL",        NULL,                    SM_TPL_NOOP,       0, 0 },
    { SM_STNO,         "STNO",         NULL,                    SM_TPL_NOOP,       0, 0 },
};

#define G_SM_TEMPLATES_N (int)(sizeof(g_sm_templates) / sizeof(g_sm_templates[0]))

/* Standalone non-table templates (for shapes that aren't keyed by opcode). */
static const sm_op_template_t g_tpl_unhandled = {
    -1, "UNHANDLED", "rt_unhandled_op", SM_TPL_UNHANDLED, 0, 0
};

static const sm_op_template_t g_tpl_ret_var = {
    -2, "RETURN_VARIANT", "rt_do_return", SM_TPL_RET_VAR, 0, 0
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

/* forward decl — emit_optional_lbl below uses macro_line, defined further down */
static int macro_line(FILE *out, const char *label, const char *opcode, const char *col3);

/* Helper: format the .ifnb/.else/.endif block for an optional label arg.
 *
 * EM-7c-sm-three-column-verify (2026-05-09): rewritten to route through
 * macro_line so every emitted line obeys the col-1/col-2/col-3 grid.
 * Was: `\t.ifnb \\lbl\\n\t\tlea rdi, [rip + \\lbl]\\n\t.else\\n...` —
 * 4-space-indented directives + 8-space-indented body, off the grid.
 * Now: each line is a macro_line call: directive token in col 2, args in
 * col 3.  Result assembles identically (GAS conditional assembly is
 * indentation-insensitive); audit test passes. */
static int emit_optional_lbl(FILE *out, const char *macro_arg,
                             const char *register_load_dst)
{
    char ifnb_arg[32], lea_arg[64], xor_arg[16];
    snprintf(ifnb_arg, sizeof(ifnb_arg), "\\%s", macro_arg);
    snprintf(lea_arg,  sizeof(lea_arg),  "%s, [rip + \\%s]",
             register_load_dst, macro_arg);
    /* xor edi,edi clears rdi; xor edx,edx clears rdx; etc.
     * register_load_dst is "rdi"/"rsi"/"rdx"; we strip the 'r'. */
    snprintf(xor_arg,  sizeof(xor_arg),  "e%s, e%s",
             register_load_dst + 1, register_load_dst + 1);
    if (macro_line(out, "", ".ifnb", ifnb_arg) < 0) return -1;
    if (macro_line(out, "", "lea",   lea_arg)  < 0) return -1;
    if (macro_line(out, "", ".else", "")       < 0) return -1;
    if (macro_line(out, "", "xor",   xor_arg)  < 0) return -1;
    if (macro_line(out, "", ".endif", "")      < 0) return -1;
    return 0;
}

/* Helper: emit one three-column line for sm_macros.s content.
 * Col 1 (label, 24-wide): label or empty.
 * Col 2 (opcode, 16-wide): directive/mnemonic.
 * Col 3 (free): operands + optional annotation.
 *
 * If opcode starts with '.' it is a directive (.macro, .endm, .ifnb, etc.).
 * Banner lines (starting with '#') are printed full-width -- NOT three-column.
 */
static int macro_line(FILE *out, const char *label, const char *opcode, const char *col3)
{
    const char *lbl = (label  && *label)  ? label  : "";
    const char *op  = (opcode && *opcode) ? opcode : "";
    const char *c3  = (col3   && *col3)   ? col3   : "";
    /* EM-7c-no-trailing-ws (2026-05-09): build + right-trim. */
    char line[768];
    int n = snprintf(line, sizeof(line), "%-24s%-16s %s", lbl, op, c3);
    if (n < 0) return -1;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) n--;
    line[n] = '\0';
    return (fputs(line, out) < 0 || fputc('\n', out) == EOF) ? -1 : 0;
}

static int render_macro_body(FILE *out, const sm_op_template_t *t)
{
    /* Each arm emits one .macro NAME args / body / .endm block.
     * All lines use the corrected three-column format:
     *   Col 1 (24-wide): label or empty
     *   Col 2 (16-wide): opcode/directive/mnemonic
     *   Col 3 (free):    args/operands */
    char macro_def[64];
    switch (t->kind) {
    case SM_TPL_NULLARY:
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_RET:
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "ret", "");
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_INT64:
        snprintf(macro_def, sizeof(macro_def), "%s val", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "movabs", "rdi, \\val");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_LBL:
        snprintf(macro_def, sizeof(macro_def), "%s lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "lea", "rdi, [rip + \\lbl]");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_LBLOPT:
        snprintf(macro_def, sizeof(macro_def), "%s lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "lbl", "rdi");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_LBL_INT32:
        snprintf(macro_def, sizeof(macro_def), "%s lbl, n", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "lea", "rdi, [rip + \\lbl]");
        macro_line(out, "", "mov", "esi, \\n");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_LBLOPT_INT32:
        snprintf(macro_def, sizeof(macro_def), "%s n, lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\n");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_LBLOPT3:
        snprintf(macro_def, sizeof(macro_def), "%s is_imm, fname_lbl, namelist_lbl",
                 t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "fname_lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\is_imm");
        emit_optional_lbl(out, "namelist_lbl", "rdx");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_LBLOPT_I_I:
        snprintf(macro_def, sizeof(macro_def), "%s is_imm, nargs, fname_lbl",
                 t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "fname_lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\is_imm");
        macro_line(out, "", "mov", "edx, \\nargs");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_EXEC_VAR:
        snprintf(macro_def, sizeof(macro_def), "%s has_repl, subj_lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "subj_lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\has_repl");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_ARITH:
        /* EM-7c-bb-three-column follow-up: each arithmetic SM op gets its
         * own named no-arg macro (ADD_NUM, SUB_NUM, MUL_NUM, DIV_NUM,
         * MOD_NUM).  Op enum is baked into the macro body via t->const_a;
         * caller writes just "ADD_NUM" in col 2 — no opaque "ARITH 17"
         * with redundant "# SM_ADD" annotation.  Suffix avoids collision
         * with x86 mnemonics `add`/`sub`/`mul`/`div`. */
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        { char op_arg[32]; snprintf(op_arg, sizeof(op_arg), "edi, %d", t->const_a);
          macro_line(out, "", "mov", op_arg); }
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_PCREF_JMP:
        snprintf(macro_def, sizeof(macro_def), "%s tgt", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "jmp", "\\tgt");
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_PCREF_COND:
        snprintf(macro_def, sizeof(macro_def), "%s tgt", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", "test", "eax, eax");
        macro_line(out, "", t->const_a ? "jnz" : "jz", "\\tgt");
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_PUSH_CHUNK:
        snprintf(macro_def, sizeof(macro_def), "%s entry, arity", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "movabs", "rdi, \\entry");
        macro_line(out, "", "mov", "esi, \\arity");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_CALL_CHUNK:
        snprintf(macro_def, sizeof(macro_def), "%s tgt", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "call", "\\tgt");
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_RET_VAR:
        snprintf(macro_def, sizeof(macro_def), "%s kind, cond, pc", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "mov", "edi, \\kind");
        macro_line(out, "", "mov", "esi, \\cond");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", "test", "eax, eax");
        macro_line(out, "", "jz", ".Lretskip_\\pc");
        macro_line(out, "", "ret", "");
        fprintf(out, ".Lretskip_\\pc\\():\n");  /* GAS local label hack: must stay as-is */
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_UNHANDLED:
        snprintf(macro_def, sizeof(macro_def), "%s op", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "mov", "edi, \\op");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;

    case SM_TPL_NOOP:
        /* NOOP shape: empty-bodied marker macro.  The macro name in col
         * 2 of the per-call line declares "what executes here" so the
         * .LpcN: label is never naked.  The macro body assembles to
         * nothing; the .LpcN remains a valid jump target on its own. */
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", ".endm", "");
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

/* Build the args-only string for column 3 into buf[cap].
 * This is everything after the macro name in the call line.
 * Returns 0 on success, -1 if buf is too small (truncation).
 *
 * Convention: if there are no args, buf[0] = '\0' (empty string).
 * The macro name itself goes into column 2 (t->macro_name directly).
 * These two together replace the old fused build_op_col. */
static int build_args_col(char *buf, int cap, const sm_op_template_t *t,
                          const sm_emit_args_t *args)
{
    int n = 0;
    switch (t->kind) {
    case SM_TPL_NULLARY:
    case SM_TPL_RET:
    case SM_TPL_NOOP:
        n = snprintf(buf, cap, "");  /* no args */
        break;
    case SM_TPL_INT64:
        n = snprintf(buf, cap, "%" PRId64, args->i64);
        break;
    case SM_TPL_LBL:
        if (!args->lbl) {
            fprintf(stderr, "sm_emit_template: SM_TPL_LBL got NULL lbl for %s\n",
                    t->macro_name);
            return -1;
        }
        n = snprintf(buf, cap, "%s", args->lbl);
        break;
    case SM_TPL_LBLOPT:
        if (args->lbl)
            n = snprintf(buf, cap, "%s", args->lbl);
        else
            n = snprintf(buf, cap, "");
        break;
    case SM_TPL_LBL_INT32:
        if (!args->lbl) {
            fprintf(stderr, "sm_emit_template: SM_TPL_LBL_INT32 got NULL lbl for %s\n",
                    t->macro_name);
            return -1;
        }
        n = snprintf(buf, cap, "%s, %d", args->lbl, args->i32_a);
        break;
    case SM_TPL_LBLOPT_INT32:
        if (args->lbl)
            n = snprintf(buf, cap, "%d, %s", args->i32_a, args->lbl);
        else
            n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL_LBLOPT3:
        if (args->lbl && args->lbl_b)
            n = snprintf(buf, cap, "%d, %s, %s",
                         args->i32_a, args->lbl, args->lbl_b);
        else if (args->lbl)
            n = snprintf(buf, cap, "%d, %s", args->i32_a, args->lbl);
        else if (args->lbl_b)
            n = snprintf(buf, cap, "%d, , %s", args->i32_a, args->lbl_b);
        else
            n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL_LBLOPT_I_I:
        if (args->lbl)
            n = snprintf(buf, cap, "%d, %d, %s",
                         args->i32_a, args->i32_b, args->lbl);
        else
            n = snprintf(buf, cap, "%d, %d", args->i32_a, args->i32_b);
        break;
    case SM_TPL_EXEC_VAR:
        if (args->lbl)
            n = snprintf(buf, cap, "%d, %s", args->i32_a, args->lbl);
        else
            n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL_ARITH:
        /* No args; op-enum is baked into the macro body. */
        n = snprintf(buf, cap, "");
        break;
    case SM_TPL_PCREF_JMP:
    case SM_TPL_PCREF_COND:
    case SM_TPL_CALL_CHUNK:
        n = snprintf(buf, cap, ".Lpc%d", args->i32_a);
        break;
    case SM_TPL_PUSH_CHUNK:
        n = snprintf(buf, cap, "%" PRId64 ", %d", args->i64, args->i32_a);
        break;
    case SM_TPL_RET_VAR:
        n = snprintf(buf, cap, "%d, %d, %d",
                     args->i32_a, args->i32_b, args->pc);
        break;
    case SM_TPL_UNHANDLED:
        n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL__COUNT:
        break;
    default:
        fprintf(stderr, "sm_emit_template: build_args_col: unknown kind %d\n",
                (int)t->kind);
        return -1;
    }
    return (n < 0 || n >= cap) ? -1 : 0;
}

static int render_call_line(FILE *out, const sm_op_template_t *t,
                            const sm_emit_args_t *args)
{
    /* Corrected three-column layout:
     *   Col 1 (label):  24 chars, left-aligned  -- from args->label or g_pending_pc_label
     *   Col 2 (opcode): 16 chars, left-aligned  -- macro name ONLY (no args)
     *   Col 3 (args + anno): free width         -- args then # comment
     *
     * g_pending_pc_label is consumed (cleared) on the first call per
     * instruction so that continuation lines (multi-line blob handlers)
     * don't inherit the label.
     */
    char argsb[128];
    if (build_args_col(argsb, sizeof(argsb), t, args) != 0) return -1;

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

    /* Build args+anno for column 3: args (if any) then annotation.
     * One-space gap between args and `#` -- no fourth-column padding. */
    char col3[256];
    const char *anno = args ? args->anno : NULL;
    if (argsb[0] && anno && *anno) {
        if (anno[0] == '#')
            snprintf(col3, sizeof(col3), "%s %s", argsb, anno);
        else
            snprintf(col3, sizeof(col3), "%s # %s", argsb, anno);
    } else if (argsb[0]) {
        snprintf(col3, sizeof(col3), "%s", argsb);
    } else if (anno && *anno) {
        if (anno[0] == '#')
            snprintf(col3, sizeof(col3), "%s", anno);
        else
            snprintf(col3, sizeof(col3), "# %s", anno);
    } else {
        col3[0] = '\0';
    }

    /* EM-7c-no-trailing-ws (2026-05-09): build + right-trim. */
    char line[768];
    int n;
    if (lbl_col && *lbl_col) {
        /* Three-column: label(24) / opcode(16) / args+anno.
         * Single space ensures gap even when opcode overflows 16 chars. */
        n = snprintf(line, sizeof(line), "%-24s%-16s %s", lbl_col, t->macro_name, col3);
    } else {
        /* No label: tab + opcode(16) + space + args+anno */
        n = snprintf(line, sizeof(line), "\t%-15s %s", t->macro_name, col3);
    }
    if (n < 0) return -1;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) n--;
    line[n] = '\0';
    if (fputs(line, out) < 0 || fputc('\n', out) == EOF) return -1;
    return 0;
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

    /* EM-7c-sm-three-column-verify (2026-05-09): trailing '\\n\\n'
     * produced one blank line at end-of-file — flagged by the audit.
     * Drop the second '\\n'. */
    if (fputs("# === END sm macro library ===\n", out) == EOF) return -1;
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

int sm_emit_noop(FILE *out, const sm_op_template_t *t, const char *anno)
{
    /* SM_TPL_NOOP: render exactly one three-column line carrying the
     * macro name in col 2.  The pending .LpcN: pc-label is consumed
     * (via render_call_line's normal label-pickup path) so the label
     * is never naked.  Macro body is empty; the .s assembles cleanly
     * because the macro expands to nothing. */
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

int sm_emit_push_expression(FILE *out, const sm_op_template_t *t,
                       int64_t entry_pc, int arity)
{
    sm_emit_args_t a = { 0 };
    a.i64 = entry_pc;
    a.i32_a = arity;
    return sm_emit_template(out, t, &a);
}

int sm_emit_call_expression(FILE *out, const sm_op_template_t *t, int target_pc)
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
