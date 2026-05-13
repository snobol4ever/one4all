/*
 * emitter_sm_gen.c — SM_Program -> standalone x86-64 GNU-as text (mode 4, --jit-emit --x64)
 *
 * Entry point: sm_codegen_text(SM_Program *prog, FILE *out, const char *src_path)
 *
 * Split from sm_codegen.c (ESP-15): binary/jit-run half stays in sm_codegen.c;
 * text/jit-emit half lives here.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "sm_codegen.h"
#include "sm_image.h"
#include "sm_prog.h"
#include "snobol4.h"
#include "bb_broker.h"
#include "emitter_bb_gen.h"
#include "emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "emitter_sm_template.h" /* sm_op_template_t, emit_sm_args_t (ESP-13) */
#include "bb_flat.h"              /* bb_build_flat_text (ESP-13) */
#include <assert.h>               /* assert() used in sm_codegen_text (ESP-13) */

/* emitter_sm.c exports — no separate header exists; declare here (ESP-15) */
#include "emitter_sm_template.h"
void emit_sm_op(int op);
void emit_sm_halt(void);
void emit_sm_return(void);
void emit_sm_label(void);
void emit_sm_stno(int stno, int lineno, const char *src);
void emit_sm_push_lit_i(int64_t val);
void emit_sm_push_lit_f(double val);
void emit_sm_push_lit_s(const char *str_lbl, uint64_t str_ptr, int len);
void emit_sm_push_expr(uint64_t ptr_val);
void emit_sm_push_expression(uint64_t entry_ptr, int arity);
void emit_sm_call_expression(const char *tgt_sym);
void emit_sm_push_var(const char *lbl, uint64_t ptr);
void emit_sm_store_var(const char *lbl, uint64_t ptr);
void emit_sm_exec_stmt(const char *subj_lbl, uint64_t subj_ptr, int has_repl);
void emit_sm_call_fn(const char *name_lbl, uint64_t name_ptr, int nargs);
void emit_sm_jump(int pc);
void emit_sm_jump_s(int pc);
void emit_sm_jump_f(int pc);
void emit_sm_add(void); void emit_sm_sub(void); void emit_sm_mul(void);
void emit_sm_div(void); void emit_sm_mod(void);
void emit_sm_acomp(int op); void emit_sm_lcomp(int op);
void emit_sm_incr(int64_t n); void emit_sm_decr(int64_t n);
void emit_sm_return_variant(int kind, int cond, int pc);
void emit_sm_freturn(int pc); void emit_sm_nreturn(int pc);
void emit_sm_return_s(int pc); void emit_sm_return_f(int pc);
void emit_sm_freturn_s(int pc); void emit_sm_freturn_f(int pc);
void emit_sm_nreturn_s(int pc); void emit_sm_nreturn_f(int pc);
void emit_sm_pat_lit(const char *l, uint64_t p);
void emit_sm_pat_refname(const char *l, uint64_t p);
void emit_sm_pat_usercall(const char *l, uint64_t p);
void emit_sm_pat_capture(const char *name_lbl, uint64_t name_ptr, int kind);
void emit_sm_pat_usercall_args(const char *name_lbl, uint64_t name_ptr, int nargs);
void emit_sm_pat_capture_fn(const char *fname_lbl, uint64_t fname_ptr, int is_imm, const char *namelist_lbl, uint64_t namelist_ptr);
void emit_sm_pat_capture_fn_args(const char *fname_lbl, uint64_t fname_ptr, int is_imm, int nargs);
void emit_sm_pat_span(void); void emit_sm_pat_break(void);
void emit_sm_pat_any(void);  void emit_sm_pat_notany(void);
void emit_sm_pat_len(void);  void emit_sm_pat_pos(void);
void emit_sm_pat_rpos(void); void emit_sm_pat_tab(void);
void emit_sm_pat_rtab(void); void emit_sm_pat_arb(void);
void emit_sm_pat_arbno(void);void emit_sm_pat_rem(void);
void emit_sm_pat_fence(void);void emit_sm_pat_fence1(void);
void emit_sm_pat_fail(void); void emit_sm_pat_abort(void);
void emit_sm_pat_succeed(void); void emit_sm_pat_bal(void);
void emit_sm_pat_eps(void);  void emit_sm_pat_cat(void);
void emit_sm_pat_alt(void);  void emit_sm_pat_deref(void);
void emit_sm_unhandled_op(int op);
void emit_sm_define(void); void emit_sm_define_entry(void);
void emit_sm_coerce_num(void); void emit_sm_concat(void);
void emit_sm_push_null(void); void emit_sm_push_null_noflip(void);
void emit_sm_neg(void); void emit_sm_exp(void);
void emit_sm_resume(void); void emit_sm_suspend(void);
void emit_sm_bb_pump(void); void emit_sm_bb_once(void);
void emit_sm_bb_once_proc(void); void emit_sm_bb_pump_proc(void);
void emit_sm_bb_pump_sm(void); void emit_sm_bb_pump_ast(void);
void emit_sm_bb_pump_case(void); void emit_sm_bb_pump_every(void);
void emit_sm_bb_eval(void);
int  emit_sm_macro_library(FILE *out);
int  emit_sm_macro_library_to_path(const char *path);
const sm_op_template_t *sm_template_lookup(int op);
const char *emit_sm_consume_pc_label(void);
void emit_sm_set_pc_label(const char *lbl);

/*======================================================================================================================
 * sm_codegen_text — TEXT mode pipeline (ESP-13: moved from sm_codegen_x64_emit.c)
 *======================================================================================================================*/

/* EDP-2: inline GAS mode flag — set by --jit-emit-inline via scrip.c. */
int g_jit_emit_inline = 0;

/* TEXT_MODE: pick EMIT_TEXT_INLINE or EMIT_TEXT depending on the flag. */
#define TEXT_MODE() (g_jit_emit_inline ? EMIT_TEXT_INLINE : EMIT_TEXT)

/* -----------------------------------------------------------------------
 * Source-line cache (EM-4-readability)
 *
 * When the caller passes a non-NULL src_path, we slurp the file once,
 * split it into lines (1-based index), and use it to print a verbatim
 * source banner above each statement's asm block.
 *
 * Goal: the emitted .s file reads top-to-bottom like an annotated
 * disassembly -- each statement's source text is right above the
 * x86 it produces.  Nobody else's compiler does this.  Ours does.
 * ----------------------------------------------------------------------- */

typedef struct {
    char       *buf;       /* whole-file backing buffer (NUL-terminated)  */
    char      **lines;     /* lines[i] = pointer into buf, 1-based;
                              lines[0] is unused so lookups are direct.  */
    int         count;     /* highest 1-based line index that exists     */
    const char *path;      /* original src_path (borrowed; may be NULL)  */
} SrcLines;

/* Slurp src_path into sl.  Returns 0 on success, -1 on error.
 * On error sl is left zeroed; the emitter falls back to "no source"
 * mode and emits structural banners only. */
static int srclines_load(SrcLines *sl, const char *src_path)
{
    memset(sl, 0, sizeof(*sl));
    if (!src_path) return -1;
    FILE *f = fopen(src_path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n < 0)                       { fclose(f); return -1; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf)                        { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';

    /* First pass: count lines.  Last line w/o trailing \n still counts. */
    int count = 0;
    for (size_t i = 0; i < got; i++) if (buf[i] == '\n') count++;
    if (got > 0 && buf[got-1] != '\n') count++;

    char **lines = calloc((size_t)count + 2, sizeof(char *));
    if (!lines) { free(buf); return -1; }

    /* Second pass: split.  NUL-terminate each line (overwrite '\n').
     * lines[0] left NULL so 1-based lookups are direct. */
    int    li = 1;
    char  *p  = buf;
    char  *line_start = buf;
    for (size_t i = 0; i < got; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            lines[li++] = line_start;
            line_start = &buf[i+1];
        }
    }
    if (line_start < buf + got) lines[li++] = line_start;

    sl->buf   = buf;
    sl->lines = lines;
    sl->count = li - 1;
    sl->path  = src_path;
    (void)p;
    return 0;
}

static void srclines_free(SrcLines *sl)
{
    free(sl->lines);
    free(sl->buf);
    memset(sl, 0, sizeof(*sl));
}

/* Lookup line text by 1-based index.  Returns NULL if out of range. */
static const char *srclines_get(const SrcLines *sl, int lineno)
{
    if (!sl || !sl->lines || lineno < 1 || lineno > sl->count) return NULL;
    return sl->lines[lineno];
}

/* Strip a trailing '\r' (CRLF source files) for clean banner output. */
static void srcline_strip_cr(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    if (n > 0 && s[n-1] == '\r') s[n-1] = '\0';
}

/* -----------------------------------------------------------------------
 * String table — EM-6
 *
 * The emitter cannot use raw in-process pointers for string literals —
 * the emitted binary runs in a different process.  Instead, we collect
 * all unique C strings referenced by the SM_Program into a string table,
 * emit them in .section .rodata as .Lstr_N labels, and reference those
 * labels by name in the instruction emitters.
 *
 * Two-pass protocol:
 *   1. strtab_collect(prog) — walk the SM_Program, intern all a[].s strings.
 *   2. strtab_emit(out)     — write .section .rodata with all .string data.
 *   3. Instruction emitters call strtab_label(s) to get the label name.
 * ----------------------------------------------------------------------- */

#define STRTAB_CAP 8192  /* EM-7: beauty.sno has ~2500 unique strings; 8192 gives headroom */

typedef struct {
    const char *s;       /* original pointer from SM_Instr.a[].s */
    int         idx;     /* assigned label index (.Lstr_<idx>) */
} StrEntry;

static StrEntry g_strtab[STRTAB_CAP];
static int      g_strtab_n = 0;


static void strtab_reset(void)
{
    g_strtab_n = 0;
}

/* Intern a string; return its label index (0-based). */
static int strtab_intern(const char *s)
{
    if (!s) s = "";
    /* Linear scan — small tables; fast enough for realistic programs. */
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0)
            return g_strtab[i].idx;
    if (g_strtab_n >= STRTAB_CAP) {
        fprintf(stderr, "sm_codegen_text: string table overflow\n");
        abort();
    }
    int idx = g_strtab_n;
    g_strtab[g_strtab_n].s   = s;
    g_strtab[g_strtab_n].idx = idx;
    g_strtab_n++;
    return idx;
}

/* Look up a string; return its label index or -1 if not interned. */
static int strtab_lookup(const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0)
            return g_strtab[i].idx;
    return -1;
}

/* Return the .Lstr_N label name for string s (must be interned first). */
static void strtab_label(char *buf, size_t bufsz, const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0) {
            snprintf(buf, bufsz, ".S%d", g_strtab[i].idx);
            return;
        }
    /* Should not happen if caller always interns first. */
    snprintf(buf, bufsz, ".S_ERR");
}

/* EM-7c-symbolic: intern_str callback installed on text emitters so that
 * bb_flat.c can route literal strings through the SM-side .Lstr_N strtab.
 * Uses a static buffer — safe because callers only need the label momentarily
 * to build a bb_insn_desc_t.sym before emit_insn consumes it. */
static char g_intern_str_buf[64];
static const char *codegen_intern_str(const char *s)
{
    if (s) strtab_intern(s);          /* ensure entry exists */
    strtab_label(g_intern_str_buf, sizeof(g_intern_str_buf), s ? s : "");
    return g_intern_str_buf;
}

/* Escape a C string for GNU-as .string directive.
 * Only escapes \, ", and control chars that matter. */
static void strtab_escape(char *out, size_t outsz, const char *s)
{
    size_t j = 0;
    out[j++] = '"';
    for (const char *p = s; *p && j + 6 < outsz; p++) {
        unsigned char c = (unsigned char)*p;
        if      (c == '\\') { out[j++] = '\\'; out[j++] = '\\'; }
        else if (c == '"')  { out[j++] = '\\'; out[j++] = '"';  }
        else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n';  }
        else if (c == '\t') { out[j++] = '\\'; out[j++] = 't';  }
        else if (c < 0x20 || c == 0x7f) {
            j += (size_t)snprintf(out + j, outsz - j, "\\%03o", c);
        } else {
            out[j++] = (char)c;
        }
    }
    out[j++] = '"';
    if (j < outsz) out[j] = '\0';
}

/* Collect all strings from the SM_Program into the table.
 * Only opcodes that actually carry string payloads in a[0].s / a[2].s
 * are interned — never blindly walk all instructions (other opcodes
 * carry integer / pointer payloads in the same union fields). */
static void strtab_collect(const SM_Program *prog)
{
    strtab_reset();
    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        switch (ins->op) {
        /* Opcodes that carry string data in a[0].s */
        case SM_PUSH_LIT_S:
        case SM_PUSH_VAR:
        case SM_STORE_VAR:
        case SM_PAT_LIT:
        case SM_PAT_REFNAME:
        case SM_PAT_CAPTURE:
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_USERCALL:
        case SM_PAT_CAPTURE_FN_ARGS:  /* EM-7: a[0].s = fname */
        case SM_PAT_USERCALL_ARGS:    /* EM-7: a[0].s = fname */
        case SM_EXEC_STMT:
        case SM_CALL_FN:
        case SM_LABEL:       /* EM-7d: named labels (SNOBOL4 func entries) */
            if (ins->a[0].s) strtab_intern(ins->a[0].s);
            break;
        default:
            break;
        }
        /* a[2].s: only SM_PAT_CAPTURE_FN / SM_PAT_USERCALL carry name lists */
        switch (ins->op) {
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_USERCALL:
            if (ins->a[2].s) strtab_intern(ins->a[2].s);
            break;
        default:
            break;
        }
    }
}

/* -----------------------------------------------------------------------
 * EM-FORMAT-SM "labels-only-when-referenced" (sess 2026-05-09)
 *
 * Pre-pass over SM_Program building a bitset of PCs that are actually
 * referenced as control-flow / data targets.  An unreferenced PC has no
 * .LpcN: label emitted in the .s file, eliminating the long stretches
 * of one-shot labels that previously hung off every instruction.
 *
 * Sources of "PC is a target":
 *   1. SM_JUMP / SM_JUMP_S / SM_JUMP_F : a[0].i is the destination pc.
 *   2. SM_PUSH_EXPRESSION / SM_CALL_EXPRESSION : a[0].i is an entry_pc.
 *   3. SM_LABEL with named a[0].s : the expression registry emits
 *      `.quad .L{i+1}` (see emit_expression_registry); mark pc+1.
 *   4. PC 0 : conventional program entry; safe to keep marked.
 *
 * Pattern blobs (`pat_inv_<id>_α/β/γ/ω`) reference the pat_id, not
 * .LpcN -- their back-references are human-readable comments only,
 * not asm symbols.  No marking needed for blob entry pcs.
 *
 * The bitset is populated as a side effect of pattern_windows_collect()
 * so prog->instrs is walked exactly ONCE before the main dispatch loop
 * (the pattern-window walk + target-collection share the same pass).
 * ----------------------------------------------------------------------- */

static uint8_t *g_pc_used_as_target = NULL;   /* len = prog->count; 1 = referenced */
static int      g_pc_used_count     = 0;

static int pc_used_alloc(const SM_Program *prog)
{
    if (g_pc_used_as_target) {
        free(g_pc_used_as_target);
        g_pc_used_as_target = NULL;
        g_pc_used_count = 0;
    }
    if (!prog || prog->count <= 0) return 0;
    g_pc_used_as_target = (uint8_t *)calloc((size_t)prog->count, 1);
    if (!g_pc_used_as_target) return -1;
    g_pc_used_count = prog->count;
    /* Source 4: program entry pc=0 always referenced. */
    g_pc_used_as_target[0] = 1;
    return 0;
}

static inline void pc_used_mark(int pc)
{
    if (g_pc_used_as_target && pc >= 0 && pc < g_pc_used_count)
        g_pc_used_as_target[pc] = 1;
}

static int pc_is_used_as_target(int pc)
{
    if (!g_pc_used_as_target) return 1;   /* fallback: keep all labels */
    if (pc < 0 || pc >= g_pc_used_count) return 1;
    return g_pc_used_as_target[pc] ? 1 : 0;
}

static void release_pc_used_as_target(void)
{
    if (g_pc_used_as_target) {
        free(g_pc_used_as_target);
        g_pc_used_as_target = NULL;
        g_pc_used_count = 0;
    }
}

/* -----------------------------------------------------------------------
 * emit_three_column_line -- central renderer for all non-BB, non-banner lines.
 *
 * Corrected three-column shape (per EM-7c-three-column-non-bb):
 *   Col 1 (label,  24-wide): label with trailing ':' if present, else empty.
 *   Col 2 (opcode, 16-wide): directive/mnemonic name ONLY (no args).
 *   Col 3 (free):            args + optional "# comment".
 *
 * When label is NULL/"", 24 spaces precede column 2.
 * When opcode is NULL/"", column 2 is empty (useful for bare-label lines).
 * When col3 is NULL/"" and no anno, nothing follows the opcode (padded).
 * Banner lines (#====..., # stmt N) are NOT routed here -- they print
 * full-width and don't obey column rules.
 * ----------------------------------------------------------------------- */
static int emit_three_column_line(FILE *out,
                                  const char *label,   /* e.g. ".Lstr_0:" or NULL */
                                  const char *opcode,  /* e.g. ".string" or NULL */
                                  const char *col3,    /* args+comment or NULL */
                                  const char *anno)    /* optional annotation or NULL */
{
    /* Build column 3: col3 content (if any) + annotation (if any).
     * One-space gap between args and `#` -- no fourth-column padding. */
    char c3[512];
    if (col3 && *col3 && anno && *anno) {
        if (anno[0] == '#')
            snprintf(c3, sizeof(c3), "%s %s", col3, anno);
        else
            snprintf(c3, sizeof(c3), "%s # %s", col3, anno);
    } else if (col3 && *col3) {
        snprintf(c3, sizeof(c3), "%s", col3);
    } else if (anno && *anno) {
        if (anno[0] == '#')
            snprintf(c3, sizeof(c3), "%s", anno);
        else
            snprintf(c3, sizeof(c3), "# %s", anno);
    } else {
        c3[0] = '\0';
    }

    /* EM-FORMAT-BB lone-label fusion (2026-05-09): route through
     * bb3c_format so the pending-label buffer covers SM-side data
     * directives (e.g. .Lexpression_registry:) that previously emitted
     * standalone label-only lines. */
    bb3c_format(out, label ? label : "", opcode ? opcode : "", c3);
    return 0;
}

/* Emit .section .rodata with all interned strings as .Lstr_N: .string "..." */
static int strtab_emit_rodata(FILE *out)
{
    if (g_strtab_n == 0) return 0;
    if (emit_three_column_line(out, "", ".section", ".rodata", NULL) != 0) return -1;
    char esc[1024];
    char lbl[32];
    for (int i = 0; i < g_strtab_n; i++) {
        strtab_escape(esc, sizeof(esc), g_strtab[i].s);
        snprintf(lbl, sizeof(lbl), ".S%d:", i);
        if (emit_three_column_line(out, lbl, ".string", esc, NULL) != 0) return -1;
    }
    if (emit_three_column_line(out, "", ".text", "", NULL) != 0) return -1;
    return 0;
}



/* EM-7d-usercall-reentrant: emit .datan expression registry table.
 *
 * Walk prog for SM_LABEL instructions with a[0].s set (SNOBOL4 named function
 * entries).  Emit a .section .data table of {name_ptr, fn_ptr} pairs (using
 * the already-interned .Lstr_N for names and the existing .LpcN for fn ptrs),
 * terminated by {0, 0}.  In-file .L references across sections are resolved
 * by the assembler/linker from the same TU.
 *
 * Returns the number of entries emitted, or -1 on I/O error.
 * If no named labels exist, emits nothing (returns 0). */
static int emit_expression_registry(FILE *out, const SM_Program *prog)
{
    /* Count function-entry labels only (a[2].i == 1, set by lower.c when
     * FUNC_IS_ENTRY_LABEL is true).  Plain goto labels (a[2].i == 0) must
     * not appear in the expression registry — they are not callable chunks. */
    int n = 0;
    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        if (ins->op == SM_LABEL && ins->a[0].s && *ins->a[0].s && ins->a[2].i == 1)
            n++;
    }
    if (n == 0) return 0;

    if (emit_three_column_line(out, "", ".section", ".data", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".align",   "8",     NULL) != 0) return -1;
    if (emit_three_column_line(out, ".Lexpression_registry:", "", "", NULL) != 0) return -1;

    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        /* Only function-entry labels (a[2].i == 1); skip goto labels. */
        if (ins->op != SM_LABEL || !ins->a[0].s || !*ins->a[0].s || ins->a[2].i != 1) continue;

        int str_idx = strtab_lookup(ins->a[0].s);
        if (str_idx < 0) continue;  /* should not happen after strtab_collect */

        /* The function body starts at the NEXT instruction after SM_LABEL
         * (SM_LABEL itself is a no-op; .LpcI is the label's own pc, but the
         * first real op is at pc i+1 which is where .Lpc{i+1}: sits).
         * Verify: in test_define.s, ROMAN's SM_LABEL is at some pc M and
         * the first op is at M+1.  We emit .Lpc{i+1} as the fn entry. */
        int entry_pc = i + 1;

        char qarg[32];
        snprintf(qarg, sizeof(qarg), ".S%d", str_idx);
        /* Both quads on one line: ".quad .SN ; .quad .LM" */
        char combined[128];
        snprintf(combined, sizeof(combined), "%-16s ; .quad            .L%d", qarg, entry_pc);
        if (emit_three_column_line(out, "", ".quad", combined, NULL) != 0) return -1;
    }

    /* Sentinel: {NULL, NULL} */
    if (emit_three_column_line(out, "", ".quad", "0                ; .quad            0", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "",  NULL)        != 0) return -1;

    return n;
}

/* EM-7c-capture: cap fixup table.  Each entry pairs a heap cap_t pointer
 * (baked as imm64 — valid in emitter process AND in the emitted binary
 * since the binary calls into libscrip_rt which allocates the same heap
 * objects) with the child's α label name.  Emitted as a sequence of
 * rt_patch_cap_fn(cap_ptr, child_fn) calls in main's preamble. */
#define MAX_CAP_FIXUPS 1024
typedef struct {
    void       *cap_ptr;      /* heap cap_t * from bb_cap_new — baked as imm64 */
    char        child_label[128]; /* label like _pat_inv_0_cap0_child_α */
} cap_fixup_t;

static cap_fixup_t g_cap_fixups[MAX_CAP_FIXUPS];
static int         g_cap_fixups_n = 0;

static void cap_fixups_reset(void) { g_cap_fixups_n = 0; }

static void cap_fixup_add(void *cap_ptr, const char *child_label)
{
    if (g_cap_fixups_n >= MAX_CAP_FIXUPS) return;
    g_cap_fixups[g_cap_fixups_n].cap_ptr = cap_ptr;
    snprintf(g_cap_fixups[g_cap_fixups_n].child_label,
             sizeof(g_cap_fixups[0].child_label), "%s", child_label);
    g_cap_fixups_n++;
}


static int emit_file_header(FILE *out, int count, int has_expression_registry)
{
    /* The .rodata section (string literals) and expression registry (if any)
     * were already emitted before this call.  We resume with .text + main
     * prologue.  If has_expression_registry is non-zero, main calls
     * rt_register_expressions before rt_init so that user-defined
     * SNOBOL4 functions are dispatchable from the start. */
    if (emit_three_column_line(out, "", ".intel_syntax", "noprefix", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".globl",  "main",          NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".type",   "main, @function", NULL) != 0) return -1;
    if (emit_three_column_line(out, "main:",       "push",   "rbp", NULL) != 0) return -1;
    if (emit_three_column_line(out, "",            "mov",    "rbp, rsp", NULL) != 0) return -1;

    if (has_expression_registry) {
        if (emit_three_column_line(out, "", "lea",  "rdi, [rip + .Lexpression_registry]", NULL) != 0) return -1;
        if (emit_three_column_line(out, "", "call", "rt_register_expressions@PLT", NULL) != 0) return -1;
    } else {
        if (emit_three_column_line(out, "", "xor",  "edi, edi", NULL) != 0) return -1;
        if (emit_three_column_line(out, "", "call", "rt_register_expressions@PLT", NULL) != 0) return -1;
    }

    /* EM-7c-capture: patch cap_t fn pointers to baked child blobs. */
    for (int i = 0; i < g_cap_fixups_n; i++) {
        const char *α = g_cap_fixups[i].child_label;
        if ((uintptr_t)g_cap_fixups[i].cap_ptr == 1) {
            char cap_lbl[128];
            const char *p = α;
            if (*p == '_') p++;
            const char *underscore = strchr(p, '_');
            int id_len = underscore ? (int)(underscore - p) : (int)strlen(p);
            snprintf(cap_lbl, sizeof(cap_lbl), ".L%.*s_data", id_len, p);
            char rdi_arg[128], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", cap_lbl);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "lea",  rdi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",  rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call", "rt_patch_cap_fn@PLT", NULL) != 0) return -1;
        } else if ((uintptr_t)g_cap_fixups[i].cap_ptr == 2) {
            char slot_lbl[128];
            const char *p = α;
            if (*p == '_') p++;
            const char *underscore = strchr(p, '_');
            int id_len = underscore ? (int)(underscore - p) : (int)strlen(p);
            snprintf(slot_lbl, sizeof(slot_lbl), ".L%.*s_slot", id_len, p);
            char rdi_arg[128], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", slot_lbl);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "lea",  rdi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",  rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call", "rt_init_arbno@PLT", NULL) != 0) return -1;
        } else {
            char rdi_arg[64], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, %llu",
                     (unsigned long long)(uintptr_t)g_cap_fixups[i].cap_ptr);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "movabs", rdi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",    rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call",   "rt_patch_cap_fn@PLT", NULL) != 0) return -1;
        }
    }

    if (emit_three_column_line(out, "", "call", "rt_init@PLT",
                               "rt_init(argc, argv)") != 0) return -1;
    return 0;
}

static int emit_file_footer(FILE *out)
{
    /* EM-FORMAT-BB lone-label fusion: flush any pending label before footer. */
    bb3c_flush_pending();
    if (emit_three_column_line(out, "", "call", "rt_finalize@PLT", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "pop",  "rbp", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "ret",  "",    NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".size", "main, .-main", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".section", ".note.GNU-stack,\"\",@progbits", NULL) != 0) return -1;
    return 0;
}

/* -----------------------------------------------------------------------
 * Three-column helpers
 *
 * Three-column SNOBOL4 layout (from BB-GEN-X86-TEXT.md law):
 *   Col 1 (label):  col 0, width 24
 *   Col 2 (action): col 24, macro name + args
 *   Col 3 (goto):   semicolon comment or live jmp
 *
 * SM_LINE(label, action, goto_comment) -- full three-column line
 * SM_ACT(action, goto_comment)         -- action + goto, no label
 * SM_LABEL_DEF(label)                  -- label only line
 * ----------------------------------------------------------------------- */

static int sm_line(FILE *out, const char *label, const char *action,
                   const char *goto_col)
{
    /* Three-column layout:  LABEL:  ACTION  GOTO/#comment
     * Col 1 (label):  24 chars, left-aligned
     * Col 2 (action): 36 chars, left-aligned
     * Col 3 (goto/#comment): free width
     *
     * If label is NULL/empty, consume g_cur_pc_label (set once per
     * instruction by set_cur_pc_label()) so the first sm_line call on
     * an instruction automatically gets the .LpcN: column.  Subsequent
     * calls within the same instruction have g_cur_pc_label already
     * cleared and emit with no label column.
     */
    const char *gc = "";
    char gc_buf[128] = "";
    if (goto_col && *goto_col) {
        int is_asm = (strncmp(goto_col, "jmp", 3) == 0 ||
                      strncmp(goto_col, "je",  2) == 0 ||
                      strncmp(goto_col, "jne", 3) == 0 ||
                      strncmp(goto_col, "jz",  2) == 0 ||
                      strncmp(goto_col, "jnz", 3) == 0 ||
                      strncmp(goto_col, "ret", 3) == 0 ||
                      strncmp(goto_col, "call",4) == 0 ||
                      goto_col[0] == '#');
        if (is_asm) {
            gc = goto_col;
        } else if (goto_col[0] == ';') {
            snprintf(gc_buf, sizeof(gc_buf), "# %s", goto_col + 1);
            gc = gc_buf;
        } else {
            snprintf(gc_buf, sizeof(gc_buf), "# %s", goto_col);
            gc = gc_buf;
        }
    }
    /* Determine column-1 label. Explicit arg wins; else consume pending
     * label from emit_sm_template's global (set by emit_sm_set_pc_label
     * once per instruction at the top of the dispatch loop). */
    const char *lbl;
    if (label && *label) {
        lbl = label;
    } else {
        lbl = emit_sm_consume_pc_label();   /* "" if no pending label */
    }
    const char *act = (action && *action) ? action : "";
    /* EM-FORMAT-BB lone-label fusion (2026-05-09): route through bb3c_format
     * so SM-side dispatch lines participate in the same pending-label buffer
     * as BB-side and data-section emissions.  bb3c_format applies the same
     * %-24s%-16s %s shape + right-trim. */
    bb3c_format(out, (lbl && *lbl) ? lbl : "", act, gc);
    return 0;
}

/* -----------------------------------------------------------------------
 * Page breaks (EM-4-readability)
 *
 * Major (==): printed at every statement boundary -- carries the verbatim
 *             source text of the statement (when available) plus stno/lineno.
 * Minor (--): printed between conceptual blocks within a statement, used
 *             sparingly to avoid clutter.
 * ----------------------------------------------------------------------- */

static int emit_major_break(FILE *out, int stno, int lineno,
                            const char *src_text)
{
    /* EM-FORMAT-BB lone-label fusion (2026-05-09): flush any pending label
     * before banner so it doesn't appear after the banner it should precede. */
    bb3c_flush_pending();
    /* EM-FORMAT-SM: 120-char `#=` banner per spec.  No blank lines. */
    if (fputs(
        "#=======================================================================================================================\n",
        out) == EOF) return -1;
    if (src_text && *src_text) {
        if (fprintf(out, "# stmt %d  (line %d):  %s\n",
                    stno, lineno, src_text) < 0) return -1;
    } else if (lineno > 0) {
        if (fprintf(out, "# stmt %d  (line %d)\n", stno, lineno) < 0) return -1;
    } else {
        if (fprintf(out, "# stmt %d\n", stno) < 0) return -1;
    }
    if (fputs(
        "#=======================================================================================================================\n",
        out) == EOF) return -1;
    return 0;
}

static int emit_sm_minor_break(FILE *out, const char *caption)
{
    /* EM-FORMAT-BB lone-label fusion (2026-05-09): flush any pending label
     * before banner so it doesn't appear after the banner it should precede. */
    bb3c_flush_pending();
    /* EM-FORMAT-SM: 120-char `#-` banner. */
    if (fputs("#-----------------------------------------------------------------------------------------------------------------------\n",
              out) == EOF) return -1;
    if (caption && *caption) {
        if (fprintf(out, "# %s\n", caption) < 0) return -1;
        if (fputs("#-----------------------------------------------------------------------------------------------------------------------\n",
                  out) == EOF) return -1;
    }
    return 0;
}

/* Render a printable, single-line preview of a string literal for use in
 * inline annotations (e.g. movabs rdi,<ptr>  # str="hi").  Truncates at
 * MAX_PREVIEW chars and replaces non-printable bytes with '.'. */
#define STR_PREVIEW_MAX  40
static void render_str_preview(char *dst, size_t cap,
                               const char *s, int slen)
{
    if (cap == 0) return;
    size_t  n = (slen > 0) ? (size_t)slen : (s ? strlen(s) : 0);
    size_t  o = 0;
    if (o < cap) dst[o++] = '"';
    for (size_t i = 0; i < n && o + 2 < cap && i < STR_PREVIEW_MAX; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            if (o + 1 < cap) dst[o++] = '\\';
            dst[o++] = (char)c;
        } else if (c < 0x20 || c == 0x7f) {
            dst[o++] = '.';
        } else {
            dst[o++] = (char)c;
        }
    }
    if (n > STR_PREVIEW_MAX && o + 4 < cap) {
        dst[o++] = '.'; dst[o++] = '.'; dst[o++] = '.';
    }
    if (o + 1 < cap) dst[o++] = '"';
    dst[o] = '\0';
}

/* -----------------------------------------------------------------------
 * SM straight-line opcode emitters
 * Each writes one or more three-column lines using sm_macros.s macros.
 * ----------------------------------------------------------------------- */

static int emit_sm_pc_label(FILE *out, int pc)
{
    /* EM-7c-no-trailing-ws: bare label, no padding (no trailing spaces). */
    return fprintf(out, ".L%d:\n", pc) < 0 ? -1 : 0;
}

static int emit_halt_line(FILE *out, int pc)
{
    (void)pc;
    /* SM_HALT: call rt_halt_tos() via sm_macros.s HALT macro.
     * Mode-4 form: call rt_halt_tos@PLT (Option C sanctioned exception).
     * Renamed from emit_sm_halt to avoid conflict with templates/sm_halt.c's
     * emit_sm_halt(emitter_t *) (EM-MODE4-IS-MODE3-DUMP-f). */
    return emit_sm_rtcall(out, sm_template_lookup(SM_HALT), NULL);
}

static int emit_push_lit_i_line(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* TEXT mode: emit macro invocation line via render_call_line.
     * Template path (emitter_text_new) expands body inline — wrong for TEXT. */
    return emit_sm_int64(out, sm_template_lookup(SM_PUSH_LIT_I),
                         ins->a[0].i, NULL);
}

__attribute__((unused))
static int emit_sm_push_lit_i_legacy(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* Legacy: SM_PUSH_INT macro via emit_sm_template. Kept as rollback. */
    return emit_sm_int64(out, sm_template_lookup(SM_PUSH_LIT_I),
                         ins->a[0].i, NULL);
}

/* EM-3 opcodes */

static int emit_sm_push_lit_s_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *s    = ins->a[0].s ? ins->a[0].s : "";
    int64_t     slen = ins->a[1].i;
    char lbl[32], anno[STR_PREVIEW_MAX + 16], preview[STR_PREVIEW_MAX + 8];
    strtab_label(lbl, sizeof(lbl), s);
    render_str_preview(preview, sizeof(preview), s, (int)slen);
    snprintf(anno, sizeof(anno), "# %s", preview);
    return emit_sm_lbl_int32(out, sm_template_lookup(SM_PUSH_LIT_S),
                             lbl, (int)slen, anno);
}

static int emit_sm_push_var_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl(out, sm_template_lookup(SM_PUSH_VAR), lbl, anno);
}

static int emit_sm_store_var_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl(out, sm_template_lookup(SM_STORE_VAR), lbl, anno);
}

static int emit_sm_pop(FILE *out, int pc)
{
    (void)pc;
    /* EM-MODE4-IS-MODE3-DUMP-h: routed through per-opcode template.
     * emitter_text_new(out) constructs a text emitter in INVOCATION mode;
     * emit_sm_void_pop (templates/sm_void_pop.c) emits:
     *   call rt_pop_void@PLT
     * directly to `out`.  Legacy emit_sm_rtcall path retained below
     * as __attribute__((unused)) for rollback reference. */
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_op(SM_VOID_POP);
    
    return 0;
}

__attribute__((unused))
static int emit_sm_pop_legacy(FILE *out, int pc)
{
    (void)pc;
    return emit_sm_rtcall(out, sm_template_lookup(SM_VOID_POP), NULL);
}

static int edp4_sm_arith(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* EM-MODE4-IS-MODE3-DUMP-l: routed through per-opcode template.
     * macro_name from sm_template_lookup carries ADD_NUM/SUB_NUM/etc. */
    const sm_op_template_t *t = sm_template_lookup(ins->op);
    if (!t) return -1;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_arith_op((int)ins->op, t->macro_name);
    
    return 0;
}

/* EM-4 opcodes: SM_LABEL + SM_JUMP / SM_JUMP_S / SM_JUMP_F */

static int emit_sm_label_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    /* SM_LABEL: routed through template (EM-MODE4-IS-MODE3-DUMP-o). */
    (void)ins; (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_label();
    return 0;
}

static int emit_sm_jump_line(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* EM-MODE4-IS-MODE3-DUMP-j: routed through per-opcode template. */
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_jump((int)ins->a[0].i);
    
    return 0;
}

static int emit_sm_jump_cond(FILE *out, const SM_Instr *ins, int pc,
                             int take_when_ok)
{
    /* Shared core for SM_JUMP_S (take_when_ok=1) and SM_JUMP_F (=0).
     * Each has its own template (the macro name encodes which one);
     * we pick the right one and emit the per-call line. */
    (void)pc;
    int  target = (int)ins->a[0].i;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    if (take_when_ok) emit_sm_jump_s(target);
    else              emit_sm_jump_f(target);
    
    return 0;
}

static int emit_sm_jump_s_line(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, /*take_when_ok=*/1);
}

static int emit_sm_jump_f_line(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, /*take_when_ok=*/0);
}

/* -----------------------------------------------------------------------
 * EM-5: SM_PUSH_EXPRESSION / SM_CALL_EXPRESSION / SM_RETURN
 *
 * SM_PUSH_EXPRESSION pushes a DT_E expression descriptor onto the SM value stack.
 *   a[0].i = entry_pc   a[1].i = arity
 * Codegen: rt_push_expression_descr(entry_pc, arity).  The runtime
 * stores it as a DESCR_t { v=DT_E, slen=arity, i=entry_pc }
 * so a downstream SM_CALL_FN "EVAL" / sm_call_expression path can find it.
 *
 * SM_CALL_EXPRESSION is a baked direct call.  a[0].i = entry_pc resolves at
 * emit-time to the .Lpc<entry_pc> label that emit_pc_label has already
 * planted at every PC.  The native CALL pushes the return address on
 * the host stack; SM_RETURN's RET pops it.  The SM value stack lives
 * inside libscrip_rt.so and is shared across the call -- the expression
 * pushes its result onto the same stack the caller will read from.
 *
 * Honest deviation from the interpreter:  sm_interp.c's SM_CALL_EXPRESSION
 * snapshots the caller's value stack to a heap buffer, runs the expression
 * on an empty stack, then restores + appends the result.  The mode-4
 * emitter does NOT do this.  Rationale:
 *   (1) For EM-5's gate program (single expression pushing a single int and
 *       returning), shared-stack call/ret is byte-correct.
 *   (2) Stack-discipline violations in expression bodies are bugs in the
 *       lowerer, not the emitter; if the lowerer gets it right we
 *       don't need to defensively snapshot.
 *   (3) When SM_SUSPEND/SM_RESUME land in EM-10 we'll need a full
 *       coexpression record anyway -- the snapshot machinery moves
 *       there, not here.
 * If a future rung surfaces a real test case that needs the
 * snapshot-and-restore semantics, we revisit.
 *
 * SM_RETURN_S / SM_RETURN_F / SM_FRETURN[_S/_F] / SM_NRETURN[_S/_F]
 * are NOT yet handled by the emitter.  They fall through to
 * edp4_sm_unhandled and produce a runtime trap if executed.  The
 * tracked .s files for the demo programs assemble cleanly (the
 * unhandled stub is a real call instruction); they will not RUN
 * correctly until a near-future rung adds the conditional-return
 * shapes.  EM-5's gate doesn't exercise them.
 * ----------------------------------------------------------------------- */

static int emit_sm_push_expression_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return edp4_emit_push_expression(out, sm_template_lookup(SM_PUSH_EXPRESSION),
                              ins->a[0].i, (int)ins->a[1].i);
}

static int emit_sm_call_expression_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return edp4_emit_call_expression(out, sm_template_lookup(SM_CALL_EXPRESSION),
                              (int)ins->a[0].i);
}

static int emit_sm_return_dispatch(FILE *out, int pc)
{
    (void)pc;
    /* SM_RETURN: native return.  TEXT dispatch uses emit_sm_ret (proven path).
     * Template emit_sm_return (templates/sm_return.c) is MACRO_DEF source of truth. */
    return emit_sm_ret(out, sm_template_lookup(SM_RETURN), NULL);
}

/* -----------------------------------------------------------------------
 * SM_STNO -- statement boundary
 *
 * EM-4-readability:  the SM_STNO opcode marks a source-statement
 * boundary.  We use it to emit a major page-break banner showing the
 * verbatim source line, plus the runtime tick into &STNO/&STCOUNT.
 *
 * Operand layout (from lower.c, EM-4):
 *   a[0].i  = source statement number (1-based)
 *   a[1].i  = source line number (added EM-4-readability; safe for
 *             interp because sm_interp.c reads only a[0].i)
 * ----------------------------------------------------------------------- */

static int emit_sm_stno_dispatch(FILE *out, const SM_Instr *ins, int pc,
                        const SrcLines *sl)
{
    /* SM_STNO: routed through template (EM-MODE4-IS-MODE3-DUMP-o).
     * Compute stno/lineno/src_text here (caller context needed), then
     * delegate banner + NOOP marker to emit_sm_stno(). */
    (void)pc;
    int stno   = (int)ins->a[0].i;
    int lineno = (int)ins->a[1].i;

    int try_lineno = lineno;
    if (try_lineno <= 0 || (sl && try_lineno > sl->count))
        try_lineno = 0;

    char line_copy[1024];
    const char *src = NULL;
    if (sl && try_lineno > 0) {
        const char *raw = srclines_get(sl, try_lineno);
        if (raw && *raw) {
            strncpy(line_copy, raw, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';
            srcline_strip_cr(line_copy);
            src = line_copy;
        }
    }

    int banner_lineno;
    if (lineno > 0 && (!sl || lineno <= sl->count))
        banner_lineno = lineno;
    else
        banner_lineno = 0;

    emit_mode_set(TEXT_MODE(), out);
    emit_sm_stno(stno, banner_lineno, src);
    return 0;
}

/* -----------------------------------------------------------------------
 * EM-7-revert (session #72, 2026-05-07): the EM-6 emit_pat_call_*
 * helpers and emit_bb_box (the brokered Phase-3 dispatcher) are
 * REMOVED.  Lon's correction: the brokered runtime descriptor-tree
 * model — rt_pat_*@PLT building a runtime pat-stack, then
 * rt_exec_stmt → exec_stmt → bb_broker — was the wrong
 * architecture for emitted code.  See GOAL-MODE4-EMIT.md "Design
 * Discoveries" section for the corrected five-phase model.  EM-7a/b/c
 * will reintroduce pattern-side emit using the proven dual-mode
 * bb_emit infrastructure (bb_flat in EMIT_TEXT mode for invariant
 * sub-trees baked into .text; bb_emit BINARY mode for variant nodes
 * emitted into bb_pool RX memory at runtime; direct-call Phase-3 to
 * the root expression's α — no broker, no pat-stack, no descriptor tree).
 * Until those rungs land, all SM_PAT_* opcodes fall through to
 * edp4_sm_unhandled in the dispatch switch.
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * EM-7-pre keepers: SM_CALL_FN, SM_CONCAT, SM_PUSH_NULL, SM_COERCE_NUM,
 *                   SM_RETURN_S/F, SM_FRETURN[_S/_F], SM_NRETURN[_S/_F].
 * These are Phase 1/4/5 concerns, orthogonal to BB / pattern-matching.
 * ----------------------------------------------------------------------- */

/* SM_CONCAT / SM_PUSH_NULL / SM_COERCE_NUM: now routed through templates
 * (sm_nullary_rt.c, EM-MODE4-IS-MODE3-DUMP-n).  The static wrappers below
 * bridge the old FILE*-based dispatcher into emit_mode_set + template call. */
static int emit_sm_concat_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_CONCAT);
    return 0;
}

static int emit_sm_push_null_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_PUSH_NULL);
    return 0;
}

static int emit_sm_coerce_num_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_coerce_num();
    return 0;
}

static int emit_sm_push_null_noflip_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_PUSH_NULL_NOFLIP);
    return 0;
}

static int emit_sm_push_lit_f_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_push_lit_f(ins->a[0].f);
    return 0;
}

static int emit_sm_push_expr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_push_expr((uint64_t)(uintptr_t)ins->a[0].ptr);
    return 0;
}

static int emit_sm_incr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_incr(ins->a[0].i);
    return 0;
}

static int emit_sm_decr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_decr(ins->a[0].i);
    return 0;
}

static int emit_sm_acomp_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_acomp((int)ins->a[0].i);
    return 0;
}

static int emit_sm_lcomp_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_lcomp((int)ins->a[0].i);
    return 0;
}

/* SM_CALL_FN: general function call.  All dispatch (pseudo-calls, builtins,
 * user-defined) lives in rt_call(name, nargs).
 *   a[0].s = function name (interned in strtab)
 *   a[1].i = nargs
 * EM-MODE4-IS-MODE3-DUMP-p: TEXT invocation uses emit_sm_lbl_int32 (proven
 * path — same as PUSH_STR, STORE_VAR, etc.).  The per-opcode template
 * emit_sm_call_fn covers MACRO_DEF (sm_macros.s regen) only; it is the
 * source of truth for the CALL_FN macro body.  The legacy static path is
 * preserved as _legacy for rollback reference. */
static int emit_sm_call_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name  = ins->a[0].s ? ins->a[0].s : "";
    int         nargs = (int)ins->a[1].i;
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl_int32(out, sm_template_lookup(SM_CALL_FN),
                             lbl, nargs, anno);
}

__attribute__((unused))
static int emit_sm_call_legacy(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name  = ins->a[0].s ? ins->a[0].s : "";
    int         nargs = (int)ins->a[1].i;
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "%s", name);
    return emit_sm_lbl_int32(out, sm_template_lookup(SM_CALL_FN),
                             lbl, nargs, anno);
}

/* SM_RETURN_S / SM_RETURN_F / SM_FRETURN[_S/_F] / SM_NRETURN[_S/_F].
 *
 * TEXT dispatch uses emit_sm_ret_var (proven path).
 * Template emit_sm_return_variant (templates/sm_return.c) is MACRO_DEF source of truth. */
static int emit_sm_return_variant_dispatch(FILE *out, sm_opcode_t op, int pc)
{
    int kind = 0;  /* RETURN */
    if (op == SM_FRETURN || op == SM_FRETURN_S || op == SM_FRETURN_F) kind = 1;
    if (op == SM_NRETURN || op == SM_NRETURN_S || op == SM_NRETURN_F) kind = 2;

    int cond = 0;  /* unconditional */
    if (op == SM_RETURN_S || op == SM_FRETURN_S || op == SM_NRETURN_S) cond = 1;
    if (op == SM_RETURN_F || op == SM_FRETURN_F || op == SM_NRETURN_F) cond = 2;

    return emit_sm_ret_var(out, kind, cond, pc, sm_opcode_name(op));
}

/* EDP-4: label-consuming dispatch wrappers for opcodes that previously used
 * emit_sm_rtcall(out, sm_template_lookup(SM_X), NULL) directly in the main
 * switch.  That path consumed g_pending_pc_label via render_call_line.  The
 * new path (emit_mode_set + template call) does not go through render_call_line,
 * so pending labels would be orphaned at branch targets.  Each wrapper must:
 *   1. emit_sm_consume_pc_label() — drain the pending .LpcN: label
 *   2. bb3c_format(out, lbl, "", "") — emit it as a lone label line if non-empty
 *   3. emit_mode_set(TEXT_MODE(), out) + emit_sm_X(NULL) — dispatch to template */

/* Helper: consume pending label + emit it, then dispatch template (nullary). */
static void edp4_label_then(FILE *out, void (*fn)(emitter_t *))
{
    const char *lbl = emit_sm_consume_pc_label();
    if (lbl && *lbl) bb3c_format(out, lbl, "", "");
    emit_mode_set(TEXT_MODE(), out);
    fn(NULL);
}

static int emit_sm_define_entry_dispatch(FILE *out, const SM_Instr *ins, int pc, const SM_Program *prog) {
    (void)ins;
    const char *name = (pc > 0 && prog->instrs[pc-1].a[0].s) ? prog->instrs[pc-1].a[0].s : "";
    char anno[80]; snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_noop(out, sm_template_lookup(SM_DEFINE_ENTRY), anno);
}
static int emit_sm_define_dispatch(FILE *out, const SM_Instr *ins, int pc) {
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char anno[80]; snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_noop(out, sm_template_lookup(SM_DEFINE), anno);
}
static int emit_sm_pat_span_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_span);     return 0; }
static int emit_sm_pat_break_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_break);    return 0; }
static int emit_sm_pat_any_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_any);      return 0; }
static int emit_sm_pat_notany_dispatch(FILE *out, int pc)   { (void)pc; edp4_label_then(out, emit_sm_pat_notany);   return 0; }
static int emit_sm_pat_len_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_len);      return 0; }
static int emit_sm_pat_pos_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_pos);      return 0; }
static int emit_sm_pat_rpos_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_rpos);     return 0; }
static int emit_sm_pat_tab_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_tab);      return 0; }
static int emit_sm_pat_rtab_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_rtab);     return 0; }
static int emit_sm_pat_arb_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_arb);      return 0; }
static int emit_sm_pat_arbno_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_arbno);    return 0; }
static int emit_sm_pat_rem_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_rem);      return 0; }
static int emit_sm_pat_fence0_dispatch(FILE *out, int pc)   { (void)pc; edp4_label_then(out, emit_sm_pat_fence);    return 0; }
static int emit_sm_pat_fence1_dispatch(FILE *out, int pc)   { (void)pc; edp4_label_then(out, emit_sm_pat_fence1);   return 0; }
static int emit_sm_pat_fail_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_fail);     return 0; }
static int emit_sm_pat_abort_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_abort);    return 0; }
static int emit_sm_pat_succeed_dispatch(FILE *out, int pc)  { (void)pc; edp4_label_then(out, emit_sm_pat_succeed);  return 0; }
static int emit_sm_pat_bal_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_bal);      return 0; }
static int emit_sm_pat_eps_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_eps);      return 0; }
static int emit_sm_pat_cat_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_cat);      return 0; }
static int emit_sm_pat_alt_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_alt);      return 0; }
static int emit_sm_pat_deref_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_deref);    return 0; }

/* SM_PAT_CAPTURE_FN_ARGS / SM_PAT_USERCALL_ARGS emitters were removed
 * in EM-7-revert (session #72) along with the rest of the brokered
 * Phase-3 path.  See the EM-7-revert banner above emit_sm_concat for
 * rationale.  These opcodes fall through to edp4_sm_unhandled until
 * EM-7c reintroduces pattern-side emit using the corrected
 * bb_flat / bb_emit infrastructure. */

/* =======================================================================
 * EM-7a — Phase-2 SM simulator: reconstruct PATND_t tree at emit time
 *
 * The SM_PAT_* opcodes are a post-order serialisation of the pattern tree.
 * Simulating them at emit time (path 1 from GOAL-MODE4-EMIT.md) reconstructs
 * the PATND_t tree using the same pat_* constructors the interpreter uses,
 * so bb_build_flat / bb_build_binary_node can be called directly on the
 * result.  The simulator runs inside the scrip process; the pat_* functions
 * are already linked.
 *
 * INVARIANT vs VARIANT:
 *   A pattern subtree is INVARIANT if every leaf value is known at emit time
 *   (compile-time constant literal or argument-free constructor).  VARIANT
 *   means at least one leaf comes from a runtime value-stack argument (a
 *   variable load or other runtime expression).
 *
 *   Invariant subtrees → baked flat into .text via bb_build_flat (EM-7b/c).
 *   Variant nodes      → emit Phase-2 SM ops at runtime to build bb_pool RX
 *                        slots and patch γ/ω to children (EM-7c).
 *
 * Sim-stack:
 *   Each slot is a SimVal: either a pat DESCR_t (is_pat=1) or a value-stack
 *   argument (is_pat=0).  Both carry an is_variant flag that propagates
 *   upward: a PAT node is variant if any of its inputs was variant.
 *
 * flat_is_eligible_node(p):
 *   Local, per-node invariance check — does NOT recurse into children.
 *   Matches the node-kind exclusion list in bb_flat.c:flat_is_eligible(),
 *   but only for the single node.  The recursive whole-tree check is now
 *   `patnd_is_fully_invariant()` below (replaces the original).
 *
 * sm_phase2_to_patnd(prog, start, end, out_variant):
 *   Walk SM instructions [start, end) that form Phase-2.
 *   Returns the root PATND_t * via the DESCR_t wrapper (DT_P).
 *   Sets *out_variant = 1 if any node is variant, 0 otherwise.
 *   Returns an XEPS node if the window is empty (pure-string match).
 * ======================================================================= */

/* Maximum sim-stack depth.  Patterns rarely exceed 64 levels. */
#define PHASE2_SIM_DEPTH  128

typedef struct {
    DESCR_t val;        /* pat DESCR_t if is_pat, else value-stack DESCR_t */
    int     is_pat;     /* 1 = pattern on the sim pat-stack */
    int     is_variant; /* 1 = this value came from a runtime variable */
} SimVal;

typedef struct {
    SimVal slots[PHASE2_SIM_DEPTH];
    int    top;      /* next free slot index (stack grows up) */
} SimStack;

static void simstack_init(SimStack *ss) { ss->top = 0; }

static void simstack_push(SimStack *ss, SimVal v)
{
    if (ss->top < PHASE2_SIM_DEPTH) ss->slots[ss->top++] = v;
}

static SimVal simstack_pop(SimStack *ss)
{
    if (ss->top > 0) return ss->slots[--ss->top];
    /* Underflow: return a variant epsilon so downstream builds still run */
    SimVal v; v.val = pat_epsilon(); v.is_pat = 1; v.is_variant = 1;
    return v;
}

/* Push a compile-time-constant (invariant) string/int onto the VALUE side */
static void simstack_push_const_s(SimStack *ss, const char *s)
{
    SimVal v;
    v.val.v  = DT_S;
    v.val.s  = s;
    v.val.i  = 0;
    v.is_pat = 0;
    v.is_variant = 0;
    simstack_push(ss, v);
}

static void simstack_push_const_i(SimStack *ss, int64_t n)
{
    SimVal v;
    v.val.v  = DT_I;
    v.val.i  = n;
    v.val.s  = NULL;
    v.is_pat = 0;
    v.is_variant = 0;
    simstack_push(ss, v);
}

/* Push a runtime (variant) placeholder onto the VALUE side */
static void simstack_push_variant_val(SimStack *ss)
{
    SimVal v;
    v.val = pat_epsilon(); /* placeholder — actual value not known at emit time */
    v.is_pat = 0;
    v.is_variant = 1;
    simstack_push(ss, v);
}

/* Wrap a PATND_t (as DESCR_t) into a SimVal with given variant flag */
static SimVal make_pat_val(DESCR_t d, int is_variant)
{
    SimVal v;
    v.val = d;
    v.is_pat = 1;
    v.is_variant = is_variant;
    return v;
}

/* ── flat_is_eligible_node: single-node check (no child recursion) ───── */
int flat_is_eligible_node(const PATND_t *p)
{
    if (!p) return 1;
    /* XVAR: runtime DESCR_t as pattern node — graph unknown at emit time.
     * All other kinds (XARBN, XNME, XFNME, XDSAR, XSPNC, etc.) are invariant:
     * the BB graph structure is fixed at build time. */
    return p->kind != XVAR;
}

/* ── patnd_is_fully_invariant: whole-tree check ──────────────────────── */
/* Returns 0 only if the tree contains an XVAR node — the only kind
 * whose BB graph cannot be baked at emit time (runtime DESCR_t as pattern). */
int patnd_is_fully_invariant(const PATND_t *p)
{
    if (!p) return 1;
    if (!flat_is_eligible_node(p)) return 0;
    if (p->kind == XCAT && p->nchildren > 2) return 0;
    for (int i = 0; i < p->nchildren; i++)
        if (!patnd_is_fully_invariant(p->children[i])) return 0;
    return 1;
}

/* ── PATND_t accessor: extract PATND_t * from a DT_P DESCR_t ─────────── */
static PATND_t *patnd_of(DESCR_t d)
{
    if (d.v != DT_P || !d.s) return NULL;
    return (PATND_t *)d.s;
}

/* ── sm_phase2_to_patnd ──────────────────────────────────────────────── */
/*
 * Walk SM instructions [phase2_start, phase2_end) and simulate Phase-2
 * semantics to reconstruct the pattern tree.
 *
 * Instructions in this window are SM_PAT_* ops plus supporting value-stack
 * SM_PUSH_VAR produces a variant value; everything else may be invariant.
 *
 * Returns: root DESCR_t (DT_P) wrapping the reconstructed PATND_t.
 *          If the window is empty, returns pat_epsilon() (invariant).
 * Sets:    *out_variant = 1 if any node in the tree is variant.
 *
 * The returned PATND_t is GC-allocated (pat_* constructors use GC_MALLOC).
 * It is valid for the duration of the emit call.
 */
DESCR_t sm_phase2_to_patnd(const SM_Program *prog,
                            int phase2_start, int phase2_end,
                            int *out_variant)
{
    SimStack ss;
    simstack_init(&ss);
    int has_variant = 0;

    for (int pc = phase2_start; pc < phase2_end; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];
        switch (ins->op) {

        /* ── value-stack pushes (Phase-2 argument sources) ───────── */
        case SM_PUSH_LIT_S:
            simstack_push_const_s(&ss, ins->a[0].s ? ins->a[0].s : "");
            break;
        case SM_PUSH_LIT_I:
            simstack_push_const_i(&ss, ins->a[0].i);
            break;
        case SM_PUSH_VAR:
            /* Runtime variable load — variant */
            simstack_push_variant_val(&ss);
            has_variant = 1;
            break;

        /* ── leaf pat constructors that need no value-stack arg ──── */
        case SM_PAT_EPS:
            simstack_push(&ss, make_pat_val(pat_epsilon(), 0));
            break;
        case SM_PAT_ARB:
            simstack_push(&ss, make_pat_val(pat_arb(), 0));
            break;
        case SM_PAT_REM:
            simstack_push(&ss, make_pat_val(pat_rem(), 0));
            break;
        case SM_PAT_FAIL:
            simstack_push(&ss, make_pat_val(pat_fail(), 0));
            break;
        case SM_PAT_SUCCEED:
            simstack_push(&ss, make_pat_val(pat_succeed(), 0));
            break;
        case SM_PAT_ABORT:
            simstack_push(&ss, make_pat_val(pat_abort(), 0));
            break;
        case SM_PAT_BAL:
            simstack_push(&ss, make_pat_val(pat_bal(), 0));
            break;
        case SM_PAT_FENCE0:
            /* FENCE graph is fixed — not variant */
            simstack_push(&ss, make_pat_val(pat_fence(), 0));
            break;

        /* ── leaf constructors: invariant literal ────────────────── */
        case SM_PAT_LIT: {
            const char *s = ins->a[0].s ? ins->a[0].s : "";
            simstack_push(&ss, make_pat_val(pat_lit(s), 0));
            break;
        }

        /* ── constructors that pop one value-stack arg ───────────── */
        case SM_PAT_SPAN: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            /* invariant iff charset arg was a literal (not a variable) */
            simstack_push(&ss, make_pat_val(pat_span(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_BREAK: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            simstack_push(&ss, make_pat_val(pat_break_(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_ANY: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            simstack_push(&ss, make_pat_val(pat_any_cs(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_NOTANY: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            simstack_push(&ss, make_pat_val(pat_notany(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_LEN: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            /* invariant iff n was a literal */
            simstack_push(&ss, make_pat_val(pat_len(n), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_POS: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            int v = arg.is_variant;
            /* XPOSI is invariant per flat_is_eligible_node */
            simstack_push(&ss, make_pat_val(pat_pos(n), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_RPOS: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            int v = arg.is_variant;
            simstack_push(&ss, make_pat_val(pat_rpos(n), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_TAB: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            simstack_push(&ss, make_pat_val(pat_tab(n), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_RTAB: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            simstack_push(&ss, make_pat_val(pat_rtab(n), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }

        /* ── one-child pat constructors ──────────────────────────── */
        case SM_PAT_ARBNO: {
            SimVal inner = simstack_pop(&ss);
            int v = inner.is_variant;
            /* XARBN is variant if child is variant (mutable ζ on inner) */
            simstack_push(&ss, make_pat_val(pat_arbno(inner.val), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_FENCE1: {
            SimVal inner = simstack_pop(&ss);
            /* FENCE graph is fixed — invariant iff child is invariant */
            simstack_push(&ss, make_pat_val(pat_fence_p(inner.val), inner.is_variant));
            if (inner.is_variant) has_variant = 1;
            break;
        }

        /* ── two-child combinators ───────────────────────────────── */
        case SM_PAT_CAT: {
            SimVal right = simstack_pop(&ss);
            SimVal left  = simstack_pop(&ss);
            int v = left.is_variant | right.is_variant;
            simstack_push(&ss, make_pat_val(pat_cat(left.val, right.val), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_ALT: {
            SimVal right = simstack_pop(&ss);
            SimVal left  = simstack_pop(&ss);
            int v = left.is_variant | right.is_variant;
            simstack_push(&ss, make_pat_val(pat_alt(left.val, right.val), v));
            if (v) has_variant = 1;
            break;
        }

        /* ── deref: runtime pattern variable lookup → always variant */
        case SM_PAT_DEREF: {
            SimVal arg = simstack_pop(&ss);
            /* XDSAR: *varname — graph is invariant; name baked into node.
             * The variable's VALUE is read at match time, but the graph
             * structure (one XDSAR node with a fixed name) is constant. */
            DESCR_t d;
            if (arg.val.v == DT_S && arg.val.s)
                d = pat_ref(arg.val.s);
            else
                d = pat_epsilon();   /* degenerate: no name */
            simstack_push(&ss, make_pat_val(d, 0));  /* invariant */
            break;
        }
        case SM_PAT_REFNAME: {
            /* *varname — XDSAR node; graph is invariant (name baked in) */
            const char *name = ins->a[0].s ? ins->a[0].s : "";
            simstack_push(&ss, make_pat_val(pat_ref(name), 0));
            break;
        }

        /* ── capture wrappers ────────────────────────────────────── */
        case SM_PAT_CAPTURE: {
            SimVal child = simstack_pop(&ss);
            const char *vname = ins->a[0].s ? ins->a[0].s : "";
            int kind = (int)ins->a[1].i;
            DESCR_t var = NAME_fn(vname);
            DESCR_t d;
            if (kind == 1) d = pat_assign_imm(child.val, var);
            else           d = pat_assign_cond(child.val, var);
            /* XFNME/XNME are invariant per flat_is_eligible_node;
             * variant propagates from child only */
            simstack_push(&ss, make_pat_val(d, child.is_variant));
            if (child.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_CAPTURE_FN_ARGS:
        case SM_PAT_USERCALL:
        case SM_PAT_USERCALL_ARGS: {
            /* XCALLCAP / XATP — always variant (runtime function call) */
            /* Pop child if CAPTURE_FN* (it has a child pat), else no child */
            SimVal child = simstack_pop(&ss);
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            DESCR_t d = pat_assign_callcap(child.val, fname, NULL, 0);
            simstack_push(&ss, make_pat_val(d, 1));
            has_variant = 1;
            break;
        }

        /* ── opcodes that are NOT Phase-2 — skip silently ────────── */
        /* SM_PUSH_VAR handled above; SM_STORE_VAR, SM_STNO, etc.     */
        /* are straight-line and never appear inside a Phase-2 window */
        default:
            /* Unknown opcode in Phase-2 window: treat as variant     */
            simstack_push_variant_val(&ss);
            has_variant = 1;
            break;
        }
    }

    *out_variant = has_variant;
    if (ss.top == 0) return pat_epsilon();   /* empty window */
    return ss.slots[ss.top - 1].val;         /* root pattern */
}

/* =======================================================================
 * EM-7c: Pattern window registry + invariant-blob emission
 * =======================================================================
 *
 * Strategy (pure-invariant first; variant nodes deferred to next rung):
 *
 * 1. Pre-pass: walk SM_Program, locate every SM_EXEC_STMT.  For each, the
 *    Phase-2 window is [stmt_start, exec_pc - 2):
 *      - exec_pc - 1  → Phase-4 push (replacement, real or dummy zero)
 *      - exec_pc - 2  → Phase-1 push (subject)
 *      - stmt_start   → instruction immediately after the preceding
 *                       SM_STNO (or 0 if none)
 *    Run sm_phase2_to_patnd() on the window.
 *    If patnd_is_fully_invariant(root): allocate a unique pattern id,
 *    record [phase2_start, phase2_end, exec_pc, pat_id].
 *
 * 2. Emit pre-text section: for each invariant pattern, call
 *    bb_build_flat_text(root, out, "pat_inv_<id>") to bake a flat
 *    .text expression with externally-visible α/β/γ/ω entry symbols.
 *
 * 3. Main dispatch:
 *      - SM_PAT_* and value-stack pushes inside an invariant Phase-2
 *        window: emit a NOP comment (the pattern is already baked).
 *      - SM_EXEC_STMT for an invariant statement: emit a call to
 *        rt_match_blob(pat_inv_<id>_α, sname, has_repl).
 *      - Variant patterns / non-pattern statements: unchanged
 *        (variant patterns fall through to edp4_sm_unhandled until
 *        the variant-runtime-emitter rung lands).
 *
 * Phase-1 (subject push) and Phase-4 (replacement push) are emitted
 * normally — they leave the value stack at [subj][repl_or_zero] just
 * before SM_EXEC_STMT, which is exactly the contract rt_match_blob
 * expects.
 * ======================================================================= */

#define MAX_PATTERN_WINDOWS 4096

typedef struct {
    int  phase2_start;       /* first SM pc inside Phase-2 (inclusive) */
    int  phase2_end;          /* one past last Phase-2 pc (exclusive) */
    int  exec_stmt_pc;        /* pc of the SM_EXEC_STMT instruction */
    int  pat_id;              /* unique id for the pat_inv_<id>_* labels */
    int  is_invariant;        /* 1 = baked in .text via bb_build_flat_text */
    DESCR_t root;             /* PATND_t root from sm_phase2_to_patnd (DT_P) */
} pattern_window_t;

static pattern_window_t g_pat_windows[MAX_PATTERN_WINDOWS];
static int              g_pat_windows_n   = 0;
static int              g_pat_windows_id  = 0;



static void pattern_windows_reset(void)
{
    g_pat_windows_n  = 0;
    g_pat_windows_id = 0;
    cap_fixups_reset();
}

/* Find the index of the pattern window covering pc, or -1 if pc is not
 * inside any registered Phase-2 window.  Linear scan — fine because
 * patterns/program is small (worst case beauty.sno ~hundreds). */
static int pattern_window_at_pc(int pc)
{
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (pc >= g_pat_windows[i].phase2_start &&
            pc <  g_pat_windows[i].phase2_end)
            return i;
    }
    return -1;
}

/* Find the pattern window whose SM_EXEC_STMT is at this pc, or -1. */
static int pattern_window_for_exec_stmt(int pc)
{
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (g_pat_windows[i].exec_stmt_pc == pc)
            return i;
    }
    return -1;
}

/* Pre-pass: locate every SM_EXEC_STMT, compute its Phase-2 window,
 * run the simulator, and register fully-invariant ones.  Variant
 * windows are NOT registered — their SM_EXEC_STMT will fall through
 * to edp4_sm_unhandled in the main dispatch (variant rung is next). */
static void pattern_windows_collect(const SM_Program *prog)
{
    pattern_windows_reset();

    /* EM-FORMAT-SM (sess 2026-05-09): allocate the target bitset for the
     * "labels-only-when-referenced" rule.  We populate it as a side effect
     * of this same walk over prog->instrs -- one pass, two outputs. */
    pc_used_alloc(prog);

    int stmt_start = 0;   /* PC of the first instruction in the current statement */

    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];

        /* EM-FORMAT-SM: target-collection side pass.  Mark PCs that are
         * jump / call / chunk-registry / data-quad targets so the dispatch
         * loop later can skip emitting unreferenced .LpcN: labels. */
        switch (ins->op) {
        case SM_JUMP:
        case SM_JUMP_S:
        case SM_JUMP_F:
        case SM_PUSH_EXPRESSION:
        case SM_CALL_EXPRESSION:
            pc_used_mark((int)ins->a[0].i);
            break;
        case SM_LABEL:
            if (ins->a[0].s && *ins->a[0].s)
                pc_used_mark(pc + 1);   /* chunk registry references pc+1 */
            break;
        default:
            break;
        }

        if (ins->op == SM_STNO) {
            stmt_start = pc + 1;
            continue;
        }

        if (ins->op != SM_EXEC_STMT) continue;

        /* Phase-2 window:
         *   [stmt_start, pc - 2)  — the two instructions at pc-1 and pc-2
         *                           are the replacement and subject pushes. */
        int phase2_end = pc - 2;
        if (phase2_end < stmt_start) phase2_end = stmt_start;  /* defensive */

        int has_variant = 0;
        DESCR_t root = sm_phase2_to_patnd(prog, stmt_start, phase2_end, &has_variant);

        if (g_pat_windows_n >= MAX_PATTERN_WINDOWS) {
            /* Too many patterns — silently drop; variant fallback path
             * will handle them via edp4_sm_unhandled.  Beauty.sno is
             * well under this cap. */
            continue;
        }

        pattern_window_t *w = &g_pat_windows[g_pat_windows_n++];
        w->phase2_start = stmt_start;
        w->phase2_end   = phase2_end;
        w->exec_stmt_pc = pc;
        w->pat_id       = g_pat_windows_id++;
        w->root         = root;

        /* Is the entire reconstructed tree invariant AND eligible for
         * bb_build_flat?  The pure-invariant case is the EM-7c gate;
         * variant patterns are deferred. */
        PATND_t *p = (PATND_t *)root.p;
        w->is_invariant = (!has_variant && p && patnd_is_fully_invariant(p)) ? 1 : 0;
    }
}

/* Emit the .text-resident invariant pattern blobs.  Called after the
 * .rodata section and before the main `.text` instruction stream.
 * Each invariant pattern gets one labeled expression with externally-visible
 * α/β/γ/ω labels (`pat_inv_<id>_α` etc.). */
static int emit_pattern_blobs(FILE *out)
{
    int n_invariant = 0;
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (g_pat_windows[i].is_invariant) n_invariant++;
    }
    if (n_invariant == 0) return 0;

    /* Reset internal label counters at the start of this emit run.
     * Within the run, IDs accumulate across patterns (no collisions). */
    bb_flat_set_intern_str(codegen_intern_str);  /* EM-7c-symbolic: route lits through strtab */
    bb_flat_set_cap_fixup_cb(cap_fixup_add);     /* EM-7c-capture: collect XNME/XFNME fixups */
    bb_build_flat_text_reset();

    if (emit_three_column_line(out, "", ".intel_syntax", "noprefix", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "", NULL) != 0) return -1;

    for (int i = 0; i < g_pat_windows_n; i++) {
        pattern_window_t *w = &g_pat_windows[i];
        if (!w->is_invariant) continue;

        char prefix[64];
        snprintf(prefix, sizeof(prefix), "pat_inv_%d", w->pat_id);

        PATND_t *p = (PATND_t *)w->root.p;
        if (bb_build_flat_text(p, out, prefix) != 0) {
            /* If the bake fails despite the pre-pass marking it invariant,
             * downgrade — at SM_EXEC_STMT we'll fall back to UNHANDLED. */
            w->is_invariant = 0;
        }
    }
    /* EM-FORMAT-BB lone-label fusion (2026-05-09): flush any pending label
     * before the SM-side dispatch begins, so an unfused trailing blob
     * label doesn't cross the section boundary into `main:` content. */
    bb3c_flush_pending();
    return 0;
}

/* SM_EXEC_STMT for a registered invariant pattern: emit the runtime call.
 * The value stack at this point holds [subj][repl_or_zero] (top = repl).
 *
 * EM-7c-s-file-beautify (2026-05-09): the first line consumes the pending
 * .LpcN: pc-label via sm_line; the three follow-on lines route through
 * emit_three_column_line so they render with 24-space col-1 padding
 * (matching SM-side dispatch shape) rather than the tab-indented no-label
 * fallback in sm_line. */
static int emit_sm_exec_stmt_blob(FILE *out, const SM_Instr *ins, int pc, int win_idx)
{
    pattern_window_t *w = &g_pat_windows[win_idx];
    const char *sname = ins->a[0].s;     /* subject NV name (or NULL) */
    int has_repl      = (int)ins->a[1].i;

    /* Arg 0 (rdi) = blob_α = address of `pat_inv_<id>_α`.
     * GAS Intel syntax: `lea rdi, [rip + symbol]`.  This first line
     * consumes the pending .LpcN: pc-label (sm_line routes through it). */
    char act[160];
    snprintf(act, sizeof(act),
             "rdi, [rip + pat_inv_%d_α]", w->pat_id);
    const char *anno = NULL;
    /* Use emit_three_column_line to keep col 1 = pending .LpcN, col 2 = lea,
     * col 3 = args + comment.  The pending label is consumed via the
     * emit_sm_consume_pc_label path below. */
    char lbl[32];
    const char *pending = emit_sm_consume_pc_label();
    if (pending && *pending) {
        size_t n = strlen(pending);
        if (n >= sizeof(lbl)) n = sizeof(lbl) - 1;
        memcpy(lbl, pending, n);
        lbl[n] = '\0';
    } else {
        lbl[0] = '\0';
    }
    if (emit_three_column_line(out, lbl, "lea", act, anno) != 0) return -1;

    /* Arg 1 (rsi) = subj_name — same .Lstr_N convention as SM_PUSH_VAR. */
    if (sname && *sname) {
        char lbl_str[64];
        strtab_label(lbl_str, sizeof(lbl_str), sname);
        char act2[160];
        snprintf(act2, sizeof(act2), "rsi, [rip + %s]", lbl_str);
        if (emit_three_column_line(out, "", "lea", act2, NULL) != 0) return -1;
    } else {
        if (emit_three_column_line(out, "", "xor", "esi, esi", NULL) != 0) return -1;
    }

    /* Arg 2 (edx) = has_repl flag */
    char act3[80];
    snprintf(act3, sizeof(act3), "edx, %d", has_repl);
    if (emit_three_column_line(out, "", "mov", act3, NULL) != 0) return -1;

    if (emit_three_column_line(out, "", "call", "rt_match_blob@PLT", NULL) != 0) return -1;

    (void)pc;
    return 0;
}

/* Emit a real three-column line for an SM op that was absorbed into an
 * invariant pattern blob.  EM-7c-stmt-banner-fidelity: replaces the
 * earlier disembodied `# (baked into pat_inv_<id> at .text — SM_*)`
 * comment.  Shape:
 *   .LpcN:                  PAT_RPOS         baked  # _pat_inv_0 pc=7..12
 * Col 1 = pending .LpcN: label (consumed); col 2 = SM op name; col 3 =
 * `baked` token; trailing comment back-references the blob.  The line
 * assembles to nothing (col-2 names are macro names — PAT_RPOS, etc. —
 * but each macro body is a real PLT call; here, those PLT calls are
 * unreachable because they sit inside the blob-absorbed PC range.
 * That's fine for assembly, but for runtime correctness we MUST NOT
 * actually expand the macro.  Solution: prefix with a comment so the
 * assembler treats the line as a comment.  But col 2 still needs to
 * READ as the op name to a human.  Resolution: render `# PAT_RPOS
 * baked  # _pat_inv_0 pc=7..12` — col 2 is empty in the assembled
 * sense, but the line carries the op name in a way the eye can
 * scan.  This preserves both correctness and readability.
 *
 * Final chosen shape (col 1 still meaningful as a jump target):
 *   .LpcN:                  # PAT_RPOS       baked  pat_inv_0 pc=7..12
 * Col 2 starts with `#` so GAS treats the rest of the line as a
 * comment; the .LpcN: still defines the label so jumps targeting
 * this PC continue to work. */
static int emit_sm_pat_baked(FILE *out, const SM_Instr *ins, int pc, int win_idx)
{
    pattern_window_t *w = &g_pat_windows[win_idx];

    /* Resolve op name to its MACRO spelling (PUSH_INT, PAT_RPOS, ...)
     * for visual consistency with col-2 tokens elsewhere in the file.
     * Fall back to the SM enum name if the op has no template entry
     * (defensive — shouldn't happen on real corpora). */
    const sm_op_template_t *t = sm_template_lookup(ins->op);
    const char *opname = (t && t->macro_name) ? t->macro_name
                                              : sm_opcode_name(ins->op);
    if (!opname) opname = "?";

    /* Snapshot the pending pc-label (consume it so the next line
     * doesn't inherit it). */
    char lbl[32];
    const char *pending = emit_sm_consume_pc_label();
    if (pending && *pending) {
        size_t n = strlen(pending);
        if (n >= sizeof(lbl)) n = sizeof(lbl) - 1;
        memcpy(lbl, pending, n);
        lbl[n] = '\0';
    } else {
        lbl[0] = '\0';
    }

    /* Render via the three-column primitive with a leading-`#` op so
     * the macro is NOT expanded by the assembler — col 2 still reads
     * as the op name to the human eye, col 3 carries `baked` plus a
     * back-reference to the blob. */
    char op_col[24];
    snprintf(op_col, sizeof(op_col), "# %s", opname);

    char col3[160];
    snprintf(col3, sizeof(col3),
             "baked  pat_inv_%d pc=%d..%d",
             w->pat_id, w->phase2_start, w->phase2_end - 1);

    if (emit_three_column_line(out, lbl, op_col, col3, NULL) != 0) return -1;
    (void)pc;
    return 0;
}

/* -----------------------------------------------------------------------
 * EM-7c-variant (session #80, 2026-05-07) — pattern-construction emitters
 *
 * Each SM_PAT_* opcode that's not absorbed into an invariant blob (i.e.,
 * the pattern is variant, or there's no SM_EXEC_STMT in this region —
 * pattern-as-rvalue case like `WPAT = BREAK(WORD) SPAN(WORD)`) is emitted
 * as a thin call to the matching rt_pat_*() function.  The runtime
 * the value stack as DT_P; SM_EXEC_STMT for variant patterns calls
 * rt_match_variant which delegates to exec_stmt.
 *
 * This is path β from the EM-7c-variant rung's design space (see
 * GOAL-MODE4-EMIT.md): runtime PATND_t reconstruction + bb_build_*-driven
 * Phase-3, distinct from EM-7-pre's reverted bb_broker route by virtue
 * of rt_init setting g_bb_mode = BB_MODE_LIVE.  The architectural
 * ideal (path α — per-variant-node bb_pool emit with linker-resolved
 * invariant child labels) is filed as a follow-up rung
 * EM-7c-variant-bb-pool-emit.
 * ----------------------------------------------------------------------- */

/* Helper: emit a no-arg PLT call that pushes a constant pattern node. */
/* Helper: look up the strtab label for a string arg, or return NULL.
 * Mirrors the EM-7-pre helpers' contract for empty/NULL strings:
 * caller-side .Lstr_<n> if arg is non-empty, NULL otherwise. */
static const char *pat_arg_label(char *lbl_buf, size_t lbl_buf_n,
                                 const char *arg)
{
    if (!arg || !*arg) return NULL;
    strtab_label(lbl_buf, lbl_buf_n, arg);
    return lbl_buf;
}

/* SM_PAT_LIT: a[0].s = literal string.  Driven by SM_PAT_LIT template
 * (LBLOPT shape — single source of truth with sm_macros). */
static int emit_sm_pat_lit_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "arg=\"%.40s\"%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_LIT), l, anno);
    }
    return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_LIT), l, NULL);
}

/* SM_PAT_REFNAME: a[0].s = var name. */
static int emit_sm_pat_refname_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "%.40s%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_REFNAME), l, anno);
    }
    return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_REFNAME), l, NULL);
}

/* SM_PAT_CAPTURE: a[0].s = varname, a[1].i = kind (0/1/2). */
static int emit_sm_pat_capture_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[80];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    int kind = (int)ins->a[1].i;
    if (l) {
        snprintf(anno, sizeof(anno), "%s kind=%d", ins->a[0].s, kind);
    } else {
        snprintf(anno, sizeof(anno), "kind=%d", kind);
    }
    return emit_sm_lblopt_int32(out, sm_template_lookup(SM_PAT_CAPTURE),
                                l, kind, anno);
}

/* SM_PAT_CAPTURE_FN: . *func() / $ *func() — a[0].s=fname, a[1].i=is_imm,
 *                    a[2].s=namelist.  LBLOPT3 shape. */
static int emit_sm_pat_capture_fn_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char fname_lbl[64], nl_lbl[64], anno[160];
    const char *fl = pat_arg_label(fname_lbl, sizeof(fname_lbl), ins->a[0].s);
    const char *nl = pat_arg_label(nl_lbl,    sizeof(nl_lbl),    ins->a[2].s);
    int is_imm = (int)ins->a[1].i;
    snprintf(anno, sizeof(anno),
             "%s, %s",
             fl ? ins->a[0].s : "(NULL)",
             nl ? ins->a[2].s : "(NULL)");
    return emit_sm_capture_fn(out, sm_template_lookup(SM_PAT_CAPTURE_FN),
                              fl, is_imm, nl, anno);
}

/* SM_PAT_CAPTURE_FN_ARGS: a[0].s=fname, a[1].i=is_imm, a[2].i=nargs. */
static int emit_sm_pat_capture_fn_args_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char fname_lbl[64], anno[128];
    const char *fl = pat_arg_label(fname_lbl, sizeof(fname_lbl), ins->a[0].s);
    int is_imm = (int)ins->a[1].i;
    int nargs  = (int)ins->a[2].i;
    snprintf(anno, sizeof(anno), "%s",
             fl ? ins->a[0].s : "(NULL)");
    return emit_sm_capture_fn_args(out,
                                   sm_template_lookup(SM_PAT_CAPTURE_FN_ARGS),
                                   fl, is_imm, nargs, anno);
}

/* SM_PAT_USERCALL: bare *func() — a[0].s=fname. */
static int emit_sm_pat_usercall_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "%.40s%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_USERCALL), l, anno);
    }
    return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_USERCALL), l, NULL);
}

/* SM_PAT_USERCALL_ARGS: a[0].s=fname, a[1].i=nargs. */
static int emit_sm_pat_usercall_args_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    int nargs = (int)ins->a[1].i;
    if (l) {
        snprintf(anno, sizeof(anno), "%.40s", ins->a[0].s);
        return emit_sm_lblopt_int32(out, sm_template_lookup(SM_PAT_USERCALL_ARGS),
                                    l, nargs, anno);
    }
    return emit_sm_lblopt_int32(out, sm_template_lookup(SM_PAT_USERCALL_ARGS),
                                l, nargs, NULL);
}

/* SM_PAT_<no-arg>: dispatch via template lookup.  Each NULLARY-shape
 * pattern opcode (SPAN/BREAK/ANY/NOTANY/LEN/POS/RPOS/TAB/RTAB/ARB/
 * ARBNO/REM/FENCE/FENCE1/FAIL/ABORT/SUCCEED/BAL/EPS/CAT/ALT/DEREF/
 * BOXVAL) has its own template entry; sm_template_lookup picks the
 * right one and emit_sm_rtcall writes the macro call.  No annotation
 * needed — the macro name in col 2 is self-describing. */
static int emit_sm_pat_noarg(FILE *out, sm_opcode_t op, int pc)
{
    (void)pc;
    const sm_op_template_t *t = sm_template_lookup(op);
    if (!t) return -1;
    return emit_sm_rtcall(out, t, NULL);
}

/* SM_EXEC_STMT for a variant pattern: emit a rt_match_variant call.
 * Mirrors emit_sm_exec_stmt_blob's parameter shape (subj_name in rdi,
 * has_repl in esi). */
static int emit_sm_exec_stmt_variant(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *sname = ins->a[0].s;
    int has_repl      = (int)ins->a[1].i;
    char lbl[64];
    const char *l = pat_arg_label(lbl, sizeof(lbl), sname);
    /* Annotation captures both the (optional) subject name and the
     * has_repl flag for human readers.  The macro itself encodes the
     * shape (LBLOPT for subj + int for has_repl). */
    char anno[128];
    if (l) {
        snprintf(anno, sizeof(anno), "subj=%s", sname);
    } else {
        anno[0] = '\0';
    }
    /* Order: lbl then has_repl in template; emit_sm_exec_var packs
     * them into args->lbl and args->i32_a.  But we need the
     * annotation too, so go through emit_sm_template directly. */
    emit_sm_args_t a = { 0 };
    a.lbl   = l;
    a.i32_a = has_repl;
    a.anno  = anno[0] ? anno : NULL;
    return emit_sm_template(out, sm_template_lookup(SM_EXEC_STMT), &a);
}


/* -----------------------------------------------------------------------
 * Unhandled opcode trap
 * ----------------------------------------------------------------------- */

static int edp4_sm_unhandled(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* SM_UNHANDLED template: trap with the opcode int as edi.  Annotation
     * names the actual opcode -- the integer in args is opaque otherwise. */
    char anno[64];
    snprintf(anno, sizeof(anno), "%s", sm_opcode_name(ins->op));
    emit_sm_args_t a = { 0 };
    a.i32_a = (int)ins->op;
    a.anno  = anno;
    return emit_sm_template(out, sm_template_unhandled(), &a);
}

/* -----------------------------------------------------------------------
 * Top-level entry
 * ----------------------------------------------------------------------- */

int sm_codegen_text(SM_Program *prog, FILE *out, const char *src_path)
{
    assert(prog != NULL);
    assert(out  != NULL);

    /* EM-7c-sm-macros (sess #87): the SM opcode macro library lives in
     * a separate header file sm_macros.s; the emitted .s pulls it in via
     * `.include "sm_macros.s"`.  This keeps every .s small (was ~200 lines
     * of macro defs at the top of every file).  The header is generated
     * from g_sm_templates[] -- the same table that drives every per-call
     * emit later in this file -- so the macro definition and the
     * per-instruction emission cannot drift; they share one renderer.
     * See emit_sm_template.{h,c}.
     *
     * The header is written to the current working directory by default
     * (GAS searches `.` for `.include` paths).  Callers that redirect the
     * .s into a specific output directory should arrange to have
     * sm_macros.s alongside it (or pre-write it once via
     * emit_sm_macro_library_to_path()).  We always (re)write it here so a
     * fresh emit run cannot pick up a stale header, and the
     * write-then-include pair is atomic from the caller's perspective. */
    if (!g_jit_emit_inline) {
        if (emit_sm_macro_library_to_path("sm_macros.s") != 0) {
            fprintf(stderr,
                    "sm_codegen_text: failed to write sm_macros.s "
                    "(working directory writable?)\n");
            return -1;
        }
        if (emit_three_column_line(out, "", ".include", "\"sm_macros.s\"", NULL) != 0) return -1;
        /* EM-7c-bb-macros: write BB macro library alongside sm_macros.s. */
        if (bb_macros_write_to_path("bb_macros.s") != 0) {
            fprintf(stderr, "sm_codegen_text: failed to write bb_macros.s\n");
            return -1;
        }
        if (emit_three_column_line(out, "", ".include", "\"bb_macros.s\"", NULL) != 0) return -1;
    }

    /* EM-6: collect all string literals and variable names into the string
     * table, then emit them in .section .rodata before .text.  This makes
     * every string pointer in the emitted binary a RIP-relative reference
     * to a .Lstr_N label rather than an in-process pointer from the emitter. */
    strtab_collect(prog);
    if (strtab_emit_rodata(out) != 0) return -1;

    /* EM-7d-usercall-reentrant: emit .data expression registry table for
     * user-defined SNOBOL4 functions (SM_LABEL instructions with a[0].s set).
     * This must come after strtab_emit_rodata (so .S* labels are defined)
     * and before .text (so .L* forward references resolve in the same TU). */
    int expression_reg_count = emit_expression_registry(out, prog);
    if (expression_reg_count < 0) return -1;

    /* EM-7c: collect Phase-2 windows (one per SM_EXEC_STMT) and run the
     * Phase-2 simulator to reconstruct each pattern's PATND_t tree.
     * Fully-invariant patterns are emitted as flat .text expressions below;
     * variant patterns will be handled by a follow-up rung (their
     * SM_EXEC_STMT falls through to edp4_sm_unhandled).
     *
     * EM-FORMAT-SM (sess 2026-05-09): this call ALSO populates the
     * pc_used_as_target bitset as a side effect -- one pass over
     * prog->instrs, two outputs. */
    pattern_windows_collect(prog);
    if (emit_pattern_blobs(out) != 0) return -1;

    /* EM-4-readability: load the source file once if a path was given. */
    SrcLines sl;
    int sl_loaded = (srclines_load(&sl, src_path) == 0);

    if (emit_file_header(out, prog->count, expression_reg_count > 0) != 0) {
        if (sl_loaded) srclines_free(&sl);
        return -1;
    }

    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];

        /* If the previous instruction did not consume its pending label
         * (e.g. an SM_LABEL or SM_STNO that emits no opcode line), flush
         * it now as a standalone label line so it remains a valid jump
         * target.  Then set the label for this instruction -- but ONLY
         * if this PC is actually referenced (EM-FORMAT-SM
         * labels-only-when-referenced).  Unreferenced PCs emit no label;
         * the dispatch line renders with col-1 empty (still uniform
         * three-column shape via render_call_line / emit_three_column_line). */
        {
            const char *leftover = emit_sm_consume_pc_label();
            if (leftover && *leftover) {
                /* EM-FORMAT-BB lone-label fusion (2026-05-09): route through
                 * bb3c_format so the pending-label buffer participates.  The
                 * leftover string is "LABEL:" form already; treat as a label-
                 * only emission. */
                bb3c_format(out, leftover, "", "");
            }
            if (pc_is_used_as_target(pc)) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), ".L%d:", pc);
                emit_sm_set_pc_label(lbl);
            } else {
                emit_sm_set_pc_label("");
            }
        }

        /* EM-7c: pattern-window dispatch hook.  Two cases handled here
         * before the regular SM dispatch:
         *
         *   1. pc inside an invariant Phase-2 window → emit a comment
         *      placeholder.  Phase-2 ops were the source for the baked
         *      blob; their runtime effect (building a PATND_t on a
         *      pat-stack) is not needed because the blob is already in
         *      .text.
         *
         *   2. pc IS an SM_EXEC_STMT for an invariant statement →
         *      emit a call to rt_match_blob with the blob entry,
         *      subj name, and has_repl flag.  Phase-1 (subject) and
         *      Phase-4 (replacement) pushes immediately preceding this
         *      pc emit normally and leave the value stack at
         *      [subj][repl_or_zero] just before this call. */
        {
            int win_at  = pattern_window_at_pc(pc);
            int win_exec = (ins->op == SM_EXEC_STMT) ? pattern_window_for_exec_stmt(pc) : -1;

            if (win_at >= 0 && g_pat_windows[win_at].is_invariant) {
                int rc = emit_sm_pat_baked(out, ins, pc, win_at);
                if (rc != 0) {
                    if (sl_loaded) srclines_free(&sl);
                    return -1;
                }
                continue;
            }
            if (win_exec >= 0 && g_pat_windows[win_exec].is_invariant) {
                int rc = emit_sm_exec_stmt_blob(out, ins, pc, win_exec);
                if (rc != 0) {
                    if (sl_loaded) srclines_free(&sl);
                    return -1;
                }
                continue;
            }
        }

        int rc;
        switch (ins->op) {
            /* EM-2: halt + integer push */
            case SM_HALT:         rc = emit_halt_line(out, pc);          break;
            case SM_PUSH_LIT_I:   rc = emit_push_lit_i_line(out, ins, pc); break;
            case SM_PUSH_LIT_F:   rc = emit_sm_push_lit_f_dispatch(out, ins, pc);  break;
            case SM_PUSH_EXPR:    rc = emit_sm_push_expr_dispatch(out, ins, pc);   break;

            /* EM-3: string push, var load/store, pop, arithmetic */
            case SM_PUSH_LIT_S:   rc = emit_sm_push_lit_s_dispatch(out, ins, pc); break;
            case SM_PUSH_VAR:     rc = emit_sm_push_var_dispatch(out, ins, pc);   break;
            case SM_STORE_VAR:    rc = emit_sm_store_var_dispatch(out, ins, pc);  break;
            case SM_VOID_POP:          rc = emit_sm_pop(out, pc);             break;
            case SM_ADD:
            case SM_SUB:
            case SM_MUL:
            case SM_DIV:
            case SM_MOD:          rc = edp4_sm_arith(out, ins, pc);      break;

            /* EM-4: control flow.  SM_LABEL is a no-op (the .LpcN label at
             * every PC already serves as the target); SM_JUMP/S/F resolve
             * targets to baked-at-emit-time .Lpc<a[0].i>. */
            case SM_LABEL:        rc = emit_sm_label_dispatch(out, ins, pc);      break;
            case SM_JUMP:         rc = emit_sm_jump_line(out, ins, pc);   break;
            case SM_JUMP_S:       rc = emit_sm_jump_s_line(out, ins, pc); break;
            case SM_JUMP_F:       rc = emit_sm_jump_f_line(out, ins, pc); break;

            /* EM-5: expression descriptor push expression call, return. */
            case SM_PUSH_EXPRESSION:   rc = emit_sm_push_expression_dispatch(out, ins, pc); break;
            case SM_CALL_EXPRESSION:   rc = emit_sm_call_expression_dispatch(out, ins, pc); break;
            case SM_RETURN:       rc = emit_sm_return_dispatch(out, pc);          break;

            /* SM_DEFINE_ENTRY / SM_DEFINE: function entry markers and definition
             * stubs.  SM_DEFINE_ENTRY is emitted as a no-op rt_define_entry call
             * which registers the expression chunk in the native registry.
             * SM_DEFINE is a no-op macro (function definition already handled by
             * the expression registry at emit time). */
            case SM_DEFINE_ENTRY: rc = emit_sm_define_entry_dispatch(out, ins, pc, prog); break;
            case SM_DEFINE:       rc = emit_sm_define_dispatch(out, ins, pc);              break;

            /* EM-7-pre keepers: SM_CALL_FN (general) + SM_CONCAT + SM_PUSH_NULL +
             * SM_COERCE_NUM + conditional return variants. */
            case SM_CALL_FN:         rc = emit_sm_call_dispatch(out, ins, pc); break;
            case SM_CONCAT:       rc = emit_sm_concat_dispatch(out, pc);      break;
            case SM_PUSH_NULL:    rc = emit_sm_push_null_dispatch(out, pc);   break;
            case SM_PUSH_NULL_NOFLIP: rc = emit_sm_push_null_noflip_dispatch(out, pc); break;
            case SM_COERCE_NUM:   rc = emit_sm_coerce_num_dispatch(out, pc);  break;
            case SM_INCR:         rc = emit_sm_incr_dispatch(out, ins, pc);   break;
            case SM_DECR:         rc = emit_sm_decr_dispatch(out, ins, pc);   break;
            case SM_ACOMP:        rc = emit_sm_acomp_dispatch(out, ins, pc);  break;
            case SM_LCOMP:        rc = emit_sm_lcomp_dispatch(out, ins, pc);  break;
            case SM_FRETURN:
            case SM_NRETURN:
            case SM_RETURN_S:
            case SM_RETURN_F:
            case SM_FRETURN_S:
            case SM_FRETURN_F:
            case SM_NRETURN_S:
            case SM_NRETURN_F:    rc = emit_sm_return_variant_dispatch(out, ins->op, pc); break;

            /* SM_STNO -- statement boundary; emits major page break w/ source. */
            case SM_STNO:         rc = emit_sm_stno_dispatch(out, ins, pc,
                                                    sl_loaded ? &sl : NULL); break;

            /* EM-7c-variant (session #80, 2026-05-07): pattern-construction
             * opcodes.  Patterns inside an invariant Phase-2 window are
             * absorbed into the .text-baked pat_inv_<id>_* blob (handled
             * by the pattern_window_at_pc hook above this switch).  Patterns
             * outside any window — either variant (the runtime Phase-2
             * window built a tree with at least one variant node) or pattern-
             * as-rvalue (e.g., `WPAT = BREAK(WORD) SPAN(WORD)` builds a
             * pattern but does not exec one) — are emitted as PLT calls to
             * the libscrip_rt pat-construction ABI.
             *
             * NB: the brokered Phase-3 path that EM-7-revert tore out is
             * NOT what this rung restores.  The architectural distinction
             * is in rt_init's setting g_bb_mode = BB_MODE_LIVE so that
             * exec_stmt's Phase-3 routes through bb_build_flat / bb_build_binary
             * → direct bb_box_fn call, not through bb_broker.  See
             * GOAL-MODE4-EMIT.md "Design Discoveries" section; the bb_pool-
             * per-variant-node ideal is a follow-up rung. */
            case SM_PAT_LIT:      rc = emit_sm_pat_lit_dispatch(out, ins, pc);     break;
            case SM_PAT_REFNAME:  rc = emit_sm_pat_refname_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE:      rc = emit_sm_pat_capture_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN:   rc = emit_sm_pat_capture_fn_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN_ARGS: rc = emit_sm_pat_capture_fn_args_dispatch(out, ins, pc); break;
            case SM_PAT_USERCALL:     rc = emit_sm_pat_usercall_dispatch(out, ins, pc); break;
            case SM_PAT_USERCALL_ARGS: rc = emit_sm_pat_usercall_args_dispatch(out, ins, pc); break;
            /* Variant-path PAT opcodes: each emits its rt_pat_*@PLT call via
             * emit_sm_rtcall.  These are all registered as SM_TPL_RTCALL in
             * g_sm_templates[].  They must NOT fall through to SM_EXEC_STMT. */
            case SM_PAT_SPAN:    rc = emit_sm_pat_span_dispatch(out, pc);    break;
            case SM_PAT_BREAK:   rc = emit_sm_pat_break_dispatch(out, pc);   break;
            case SM_PAT_ANY:     rc = emit_sm_pat_any_dispatch(out, pc);     break;
            case SM_PAT_NOTANY:  rc = emit_sm_pat_notany_dispatch(out, pc);  break;
            case SM_PAT_LEN:     rc = emit_sm_pat_len_dispatch(out, pc);     break;
            case SM_PAT_POS:     rc = emit_sm_pat_pos_dispatch(out, pc);     break;
            case SM_PAT_RPOS:    rc = emit_sm_pat_rpos_dispatch(out, pc);    break;
            case SM_PAT_TAB:     rc = emit_sm_pat_tab_dispatch(out, pc);     break;
            case SM_PAT_RTAB:    rc = emit_sm_pat_rtab_dispatch(out, pc);    break;
            case SM_PAT_ARB:     rc = emit_sm_pat_arb_dispatch(out, pc);     break;
            case SM_PAT_ARBNO:   rc = emit_sm_pat_arbno_dispatch(out, pc);   break;
            case SM_PAT_REM:     rc = emit_sm_pat_rem_dispatch(out, pc);     break;
            case SM_PAT_FENCE0:  rc = emit_sm_pat_fence0_dispatch(out, pc);  break;
            case SM_PAT_FENCE1:  rc = emit_sm_pat_fence1_dispatch(out, pc);  break;
            case SM_PAT_FAIL:    rc = emit_sm_pat_fail_dispatch(out, pc);    break;
            case SM_PAT_ABORT:   rc = emit_sm_pat_abort_dispatch(out, pc);   break;
            case SM_PAT_SUCCEED: rc = emit_sm_pat_succeed_dispatch(out, pc); break;
            case SM_PAT_BAL:     rc = emit_sm_pat_bal_dispatch(out, pc);     break;
            case SM_PAT_EPS:     rc = emit_sm_pat_eps_dispatch(out, pc);     break;
            case SM_PAT_CAT:     rc = emit_sm_pat_cat_dispatch(out, pc);     break;
            case SM_PAT_ALT:     rc = emit_sm_pat_alt_dispatch(out, pc);     break;
            case SM_PAT_DEREF:   rc = emit_sm_pat_deref_dispatch(out, pc);   break;

            /* SM_EXEC_STMT for a variant pattern (invariant patterns are
             * already handled above by the pattern-window hook → blob call). */
            case SM_EXEC_STMT:    rc = emit_sm_exec_stmt_variant(out, ins, pc); break;

            default:              rc = edp4_sm_unhandled(out, ins, pc);  break;
        }
        if (rc != 0) {
            if (sl_loaded) srclines_free(&sl);
            return -1;
        }
    }

    /* Flush any final unused pending label (e.g. SM_LABEL at the very end). */
    {
        const char *leftover = emit_sm_consume_pc_label();
        if (leftover && *leftover) {
            /* EM-FORMAT-BB lone-label fusion: route through bb3c_format. */
            bb3c_format(out, leftover, "", "");
        }
    }

    int frc = emit_file_footer(out);
    /* EM-FORMAT-BB lone-label fusion (2026-05-09): flush any pending label
     * held by bb3c_format's fusion buffer before closing.  No-op if empty. */
    bb3c_flush_pending();
    if (sl_loaded) srclines_free(&sl);
    release_pc_used_as_target();
    return frc;
}
