/*
 * sm_codegen_x64_emit.c -- SM_Program -> standalone x86-64 GNU-as text
 *                         (M-JITEM-X64, GOAL-MODE4-EMIT EM-1..EM-3)
 *
 * Authors: Lon Jones Cherryholmes, Claude Sonnet
 * Date: 2026-05-06
 *
 * Architecture (settled session #67 -- see archive/EMITTER-MODE4-ARCH.md):
 *
 * TWO SEPARATE EMITTERS inside this file:
 *
 *   emit_sm_instr()  -- straight-line SM opcodes (push/pop/arith/control)
 *                       Calls GNU-as macros from sm_macros.s. One macro
 *                       per opcode group. Three-column SNOBOL4 layout:
 *                         .LpcN:   SM_ADD         ; pop r,l -> push l+r
 *                       Arithmetic and push/pop bake inline via macros.
 *                       libscrip_rt.so boundary: NV table, matcher, GC.
 *
 *   emit_bb_box()    -- BB graph boxes (called per SM_PAT_* instruction)
 *                       One proc per box. Four local labels per proc:
 *                         .α (try), .β (retry),
 *                         .γ (success exit), .ω (failure exit)
 *                       Three-column law inside each port body.
 *                       Proven precedent: bb_emit.c + snobol4_asm.mac
 *                       (106/106 SPITBOL oracle).
 *
 * Calling convention: System V AMD64. .intel_syntax noprefix.
 * sm_macros.s included at the top of every emitted file.
 *
 * Opcode coverage:
 *   EM-1: literal-zero scaffold (init+finalize only).
 *   EM-2: SM_HALT, SM_PUSH_LIT_I. SM_NOP not in opcode enum.
 *   EM-3: SM_PUSH_LIT_S, SM_PUSH_VAR, SM_STORE_VAR, SM_VOID_POP,
 *         SM_ADD/SUB/MUL/DIV/MOD via sm_macros.s.
 *         SM_DUP/SM_SWAP not in enum (honest deviation).
 *         emit_bb_box() scaffold added (no SM_PAT_* coverage yet).
 *   EM-4: SM_LABEL (no-op; .LpcN label suffices), SM_JUMP (direct jmp),
 *         SM_JUMP_S/F (call last_ok + test + conditional jmp).
 *   EM-5: SM_PUSH_CHUNK (push DT_E descriptor), SM_CALL_CHUNK (baked
 *         direct call to .Lpc<entry_pc>), SM_RETURN (native ret).
 *         Conditional return variants (SM_RETURN_S/F, SM_FRETURN[_S/_F],
 *         SM_NRETURN[_S/_F]) still trap via emit_sm_unhandled.
 *   EM-6: [REVERTED in EM-7-revert] SM_PAT_*, SM_EXEC_STMT, and the
 *         scrip_rt_pat_*@PLT helpers were removed from the emitted-code
 *         path.  Lon's correction: the brokered descriptor-tree model
 *         was the wrong architecture.  See GOAL-MODE4-EMIT.md "Design
 *         Discoveries" section for the corrected five-phase model.
 *         EM-7a/b/c will reintroduce pattern emit using bb_flat in
 *         EMIT_TEXT mode (invariant sub-trees baked into .text) plus
 *         bb_emit BINARY mode (variant nodes built into bb_pool RX
 *         memory at runtime), with Phase-3 as a direct call to the
 *         root chunk's α — no broker, no pat-stack, no descriptor tree.
 *   EM-7-pre keepers (kept after revert): SM_CALL_FN, SM_CONCAT,
 *         SM_PUSH_NULL, SM_COERCE_NUM, all 8 conditional return
 *         variants.  These are Phase 1/4/5 concerns, orthogonal to BB.
 */

#include "sm_codegen_x64_emit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "sm_prog.h"
#include "snobol4.h"         /* DESCR_t, PATND_t, pat_* constructors (EM-7a) */
#include "bb_flat.h"         /* bb_build_flat_text, bb_build_flat_text_reset (EM-7c) */
#include "sm_emit_template.h" /* SM op template table (EM-7c-sm-macros, sess #87) */
#include <string.h>

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
        fprintf(stderr, "sm_codegen_x64_emit: string table overflow\n");
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
            snprintf(buf, bufsz, ".Lstr_%d", g_strtab[i].idx);
            return;
        }
    /* Should not happen if caller always interns first. */
    snprintf(buf, bufsz, ".Lstr_ERR");
}

/* EM-7c-symbolic: intern_str callback installed on text emitters so that
 * bb_flat.c can route literal strings through the SM-side .Lstr_N strtab.
 * Uses a static buffer — safe because callers only need the label momentarily
 * to build a bb_insn_desc_t.sym before emit_insn consumes it. */
static char g_intern_str_buf[64];
static const char *codegen_intern_str(emitter_v *unused, const char *s)
{
    (void)unused;
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
 * Banner lines (# ====..., # stmt N) are NOT routed here -- they print
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

    const char *lbl = (label  && *label)  ? label  : "";
    const char *op  = (opcode && *opcode) ? opcode : "";
    return fprintf(out, "%-24s%-16s %s\n", lbl, op, c3) < 0 ? -1 : 0;
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
        snprintf(lbl, sizeof(lbl), ".Lstr_%d:", i);
        if (emit_three_column_line(out, lbl, "", "", NULL) != 0) return -1;
        if (emit_three_column_line(out, "", ".string", esc, NULL) != 0) return -1;
    }
    if (emit_three_column_line(out, "", ".text", "", NULL) != 0) return -1;
    return 0;
}



/* EM-7d-usercall-reentrant: emit .data chunk registry table.
 *
 * Walk prog for SM_LABEL instructions with a[0].s set (SNOBOL4 named function
 * entries).  Emit a .section .data table of {name_ptr, fn_ptr} pairs (using
 * the already-interned .Lstr_N for names and the existing .LpcN for fn ptrs),
 * terminated by {0, 0}.  In-file .L references across sections are resolved
 * by the assembler/linker from the same TU.
 *
 * Returns the number of entries emitted, or -1 on I/O error.
 * If no named labels exist, emits nothing (returns 0). */
static int emit_chunk_registry(FILE *out, const SM_Program *prog)
{
    /* Count named labels first so we can skip the section if none. */
    int n = 0;
    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        if (ins->op == SM_LABEL && ins->a[0].s && *ins->a[0].s)
            n++;
    }
    if (n == 0) return 0;

    if (emit_three_column_line(out, "", ".section", ".data", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".align",   "8",     NULL) != 0) return -1;
    if (emit_three_column_line(out, ".Lchunk_registry:", "", "", NULL) != 0) return -1;

    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        if (ins->op != SM_LABEL || !ins->a[0].s || !*ins->a[0].s) continue;

        int str_idx = strtab_lookup(ins->a[0].s);
        if (str_idx < 0) continue;  /* should not happen after strtab_collect */

        /* The function body starts at the NEXT instruction after SM_LABEL
         * (SM_LABEL itself is a no-op; .LpcI is the label's own pc, but the
         * first real op is at pc i+1 which is where .Lpc{i+1}: sits).
         * Verify: in test_define.s, ROMAN's SM_LABEL is at some pc M and
         * the first op is at M+1.  We emit .Lpc{i+1} as the fn entry. */
        int entry_pc = i + 1;

        char anno[64], qarg[32], earg[32];
        snprintf(anno, sizeof(anno), "chunk: %s -> .Lpc%d", ins->a[0].s, entry_pc);
        snprintf(qarg, sizeof(qarg), ".Lstr_%d", str_idx);
        snprintf(earg, sizeof(earg), ".Lpc%d", entry_pc);
        if (emit_three_column_line(out, "", ".quad", qarg, anno) != 0) return -1;
        if (emit_three_column_line(out, "", ".quad", earg, NULL)  != 0) return -1;
    }

    /* Sentinel: {NULL, NULL} */
    if (emit_three_column_line(out, "", ".quad", "0", "sentinel") != 0) return -1;
    if (emit_three_column_line(out, "", ".quad", "0", NULL)       != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "",  NULL)        != 0) return -1;

    return n;
}

/* EM-7c-capture: cap fixup table.  Each entry pairs a heap cap_t pointer
 * (baked as imm64 — valid in emitter process AND in the emitted binary
 * since the binary calls into libscrip_rt which allocates the same heap
 * objects) with the child's α label name.  Emitted as a sequence of
 * scrip_rt_patch_cap_fn(cap_ptr, child_fn) calls in main's preamble. */
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


static int emit_file_header(FILE *out, int count, int has_chunk_registry)
{
    /* The .rodata section (string literals) and .data chunk registry (if any)
     * were already emitted before this call.  We resume with .text + main
     * prologue.  If has_chunk_registry is non-zero, main calls
     * scrip_rt_register_chunks before scrip_rt_init so that user-defined
     * SNOBOL4 functions are dispatchable from the start. */
    if (fprintf(out,
        "# -----------------------------------------------------------------------\n"
        "# scrip --jit-emit --x64  (M-JITEM-X64 / EM-1..EM-7d)\n"
        "# %d SM instructions. Links against libscrip_rt.so.\n"
        "# Architecture: two emitters -- SM straight-line via sm_macros.s\n"
        "#   macros (inline x86); BB boxes via emit_bb_box() one-proc-per-box.\n"
        "# See archive/EMITTER-MODE4-ARCH.md for the full design.\n"
        "# -----------------------------------------------------------------------\n",
        count) < 0) return -1;
    if (emit_three_column_line(out, "", ".intel_syntax", "noprefix", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".globl",  "main",          NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".type",   "main, @function", NULL) != 0) return -1;
    if (emit_three_column_line(out, "main:",       "push",   "rbp", NULL) != 0) return -1;
    if (emit_three_column_line(out, "",            "mov",    "rbp, rsp", NULL) != 0) return -1;

    if (has_chunk_registry) {
        if (emit_three_column_line(out, "", "lea",  "rdi, [rip + .Lchunk_registry]",
                                   "EM-7d: register user-defined function chunks") != 0) return -1;
        if (emit_three_column_line(out, "", "call", "scrip_rt_register_chunks@PLT", NULL) != 0) return -1;
    } else {
        if (emit_three_column_line(out, "", "xor",  "edi, edi",
                                   "no user-defined functions") != 0) return -1;
        if (emit_three_column_line(out, "", "call", "scrip_rt_register_chunks@PLT", NULL) != 0) return -1;
    }

    /* EM-7c-capture: patch cap_t fn pointers to baked child blobs. */
    for (int i = 0; i < g_cap_fixups_n; i++) {
        const char *α = g_cap_fixups[i].child_label;
        char anno[128];
        if ((uintptr_t)g_cap_fixups[i].cap_ptr == 1) {
            char cap_lbl[128];
            const char *p = α;
            if (*p == '_') p++;
            const char *underscore = strchr(p, '_');
            int id_len = underscore ? (int)(underscore - p) : (int)strlen(p);
            snprintf(cap_lbl, sizeof(cap_lbl), ".L%.*s_data", id_len, p);
            snprintf(anno, sizeof(anno), "cap fixup %d (static): %s -> %s", i, cap_lbl, α);
            char rdi_arg[128], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", cap_lbl);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "lea",  rdi_arg, anno) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",  rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call", "scrip_rt_patch_cap_fn@PLT", NULL) != 0) return -1;
        } else if ((uintptr_t)g_cap_fixups[i].cap_ptr == 2) {
            char slot_lbl[128];
            const char *p = α;
            if (*p == '_') p++;
            const char *underscore = strchr(p, '_');
            int id_len = underscore ? (int)(underscore - p) : (int)strlen(p);
            snprintf(slot_lbl, sizeof(slot_lbl), ".L%.*s_slot", id_len, p);
            snprintf(anno, sizeof(anno), "arbno fixup %d: %s -> %s", i, slot_lbl, α);
            char rdi_arg[128], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", slot_lbl);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "lea",  rdi_arg, anno) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",  rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call", "scrip_rt_init_arbno@PLT", NULL) != 0) return -1;
        } else {
            char rdi_arg[64], rsi_arg[128];
            snprintf(anno, sizeof(anno), "cap fixup %d: cap_t@%p -> %s",
                     i, g_cap_fixups[i].cap_ptr, α);
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, %llu",
                     (unsigned long long)(uintptr_t)g_cap_fixups[i].cap_ptr);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "movabs", rdi_arg, anno) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",    rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call",   "scrip_rt_patch_cap_fn@PLT", NULL) != 0) return -1;
        }
    }

    if (emit_three_column_line(out, "", "call", "scrip_rt_init@PLT",
                               "scrip_rt_init(argc, argv)") != 0) return -1;
    return 0;
}

static int emit_file_footer(FILE *out)
{
    if (fprintf(out, "# -- epilogue -------------------------------------------\n") < 0) return -1;
    if (emit_three_column_line(out, "", "call", "scrip_rt_finalize@PLT", NULL) != 0) return -1;
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
     * label from sm_emit_template's global (set by sm_emit_set_pc_label
     * once per instruction at the top of the dispatch loop). */
    const char *lbl;
    if (label && *label) {
        lbl = label;
    } else {
        lbl = sm_emit_consume_pc_label();   /* "" if no pending label */
    }
    const char *act = (action && *action) ? action : "";
    if (lbl && *lbl)
        return fprintf(out, "%-24s%-16s %s\n", lbl, act, gc) < 0 ? -1 : 0;
    else
        return fprintf(out, "\t%-15s %s\n", act, gc) < 0 ? -1 : 0;
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
    /* Bracket bar at column 0, full width 78 (matches GNU-as
     * convention; '#' is the line-comment introducer).  No leading
     * '\n' — blank lines are not emitted. */
    if (fputs(
        "# ============================================================================\n",
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
        "# ============================================================================\n",
        out) == EOF) return -1;
    return 0;
}

static int emit_minor_break(FILE *out, const char *caption)
{
    if (fputs("# ----------------------------------------------------------------------------\n",
              out) == EOF) return -1;
    if (caption && *caption) {
        if (fprintf(out, "# %s\n", caption) < 0) return -1;
        if (fputs("# ----------------------------------------------------------------------------\n",
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

static int emit_pc_label(FILE *out, int pc)
{
    char lbl[32];
    snprintf(lbl, sizeof(lbl), ".Lpc%d:", pc);
    return fprintf(out, "%-24s\n", lbl) < 0 ? -1 : 0;
}

static int emit_sm_halt(FILE *out, int pc)
{
    (void)pc;
    /* SM_HALT: call scrip_rt_halt_tos() which safe-pops TOS as rc
     * if it's DT_I, else uses 0.  Driven by the SM_HALT template
     * (one source of truth with sm_macros.s). */
    return sm_emit_nullary(out, sm_template_lookup(SM_HALT), NULL);
}

static int emit_sm_push_lit_i(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* SM_PUSH_INT macro: movabs rdi,val / call scrip_rt_push_int.
     * Template-driven; macro body in sm_emit_template.c shares ONE
     * renderer with this per-call site (drift impossible). */
    return sm_emit_int64(out, sm_template_lookup(SM_PUSH_LIT_I),
                         ins->a[0].i, NULL);
}

/* EM-3 opcodes */

static int emit_sm_push_lit_s(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *s    = ins->a[0].s ? ins->a[0].s : "";
    int64_t     slen = ins->a[1].i;
    char lbl[32], anno[STR_PREVIEW_MAX + 16], preview[STR_PREVIEW_MAX + 8];
    strtab_label(lbl, sizeof(lbl), s);
    render_str_preview(preview, sizeof(preview), s, (int)slen);
    snprintf(anno, sizeof(anno), "# str=%s", preview);
    return sm_emit_lbl_int32(out, sm_template_lookup(SM_PUSH_LIT_S),
                             lbl, (int)slen, anno);
}

static int emit_sm_push_var(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# var=%s", name);
    return sm_emit_lbl(out, sm_template_lookup(SM_PUSH_VAR), lbl, anno);
}

static int emit_sm_store_var(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# store -> %s", name);
    return sm_emit_lbl(out, sm_template_lookup(SM_STORE_VAR), lbl, anno);
}

static int emit_sm_pop(FILE *out, int pc)
{
    (void)pc;
    return sm_emit_nullary(out, sm_template_lookup(SM_VOID_POP), NULL);
}

static int emit_sm_arith(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* EM-7c-bb-three-column follow-up: the per-op macros (ADD, SUB,
     * MUL, DIV, MOD, EXP) carry the op name directly in col 2.  No
     * # SM_ADD annotation needed — that would duplicate col 2. */
    const sm_op_template_t *t = sm_template_lookup(ins->op);
    if (!t) return -1;
    sm_emit_args_t a = { 0 };
    return sm_emit_template(out, t, &a);
}

/* EM-4 opcodes: SM_LABEL + SM_JUMP / SM_JUMP_S / SM_JUMP_F */

static int emit_sm_label(FILE *out, const SM_Instr *ins, int pc)
{
    /* SM_LABEL is a no-op control-flow marker.  The .LpcN label emitted
     * at every PC already serves as the jump target — but we render a
     * three-column line carrying the LABEL macro name in col 2 so the
     * pending .LpcN: pc-label is consumed and the line is never naked.
     * The macro body is empty (.macro LABEL\n.endm), so this line
     * assembles to nothing while keeping the .LpcN: a valid jump
     * target on its own.  EM-7c-stmt-banner-fidelity. */
    (void)ins; (void)pc;
    return sm_emit_noop(out, sm_template_lookup(SM_LABEL), NULL);
}

static int emit_sm_jump(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    int target = (int)ins->a[0].i;
    return sm_emit_pcref_jmp(out, sm_template_lookup(SM_JUMP),
                             target, NULL);
}

static int emit_sm_jump_cond(FILE *out, const SM_Instr *ins, int pc,
                             int take_when_ok)
{
    /* Shared core for SM_JUMP_S (take_when_ok=1) and SM_JUMP_F (=0).
     * Each has its own template (the macro name encodes which one);
     * we pick the right one and emit the per-call line. */
    (void)pc;
    int  target = (int)ins->a[0].i;
    int  op_id  = take_when_ok ? SM_JUMP_S : SM_JUMP_F;
    return sm_emit_pcref_cond(out, sm_template_lookup(op_id),
                              target, take_when_ok, NULL);
}

static int emit_sm_jump_s(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, /*take_when_ok=*/1);
}

static int emit_sm_jump_f(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, /*take_when_ok=*/0);
}

/* -----------------------------------------------------------------------
 * EM-5: SM_PUSH_CHUNK / SM_CALL_CHUNK / SM_RETURN
 *
 * SM_PUSH_CHUNK pushes a DT_E chunk descriptor onto the SM value stack.
 *   a[0].i = entry_pc   a[1].i = arity
 * Codegen: scrip_rt_push_chunk_descr(entry_pc, arity).  The runtime
 * stores it as a DESCR_t { v=DT_E, slen=arity, i=entry_pc }
 * so a downstream SM_CALL_FN "EVAL" / sm_call_chunk path can find it.
 *
 * SM_CALL_CHUNK is a baked direct call.  a[0].i = entry_pc resolves at
 * emit-time to the .Lpc<entry_pc> label that emit_pc_label has already
 * planted at every PC.  The native CALL pushes the return address on
 * the host stack; SM_RETURN's RET pops it.  The SM value stack lives
 * inside libscrip_rt.so and is shared across the call -- the chunk
 * pushes its result onto the same stack the caller will read from.
 *
 * Honest deviation from the interpreter:  sm_interp.c's SM_CALL_CHUNK
 * snapshots the caller's value stack to a heap buffer, runs the chunk
 * on an empty stack, then restores + appends the result.  The mode-4
 * emitter does NOT do this.  Rationale:
 *   (1) For EM-5's gate program (single chunk pushing a single int and
 *       returning), shared-stack call/ret is byte-correct.
 *   (2) Stack-discipline violations in chunk bodies are bugs in the
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
 * emit_sm_unhandled and produce a runtime trap if executed.  The
 * tracked .s files for the demo programs assemble cleanly (the
 * unhandled stub is a real call instruction); they will not RUN
 * correctly until a near-future rung adds the conditional-return
 * shapes.  EM-5's gate doesn't exercise them.
 * ----------------------------------------------------------------------- */

static int emit_sm_push_chunk(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return sm_emit_push_chunk(out, sm_template_lookup(SM_PUSH_CHUNK),
                              ins->a[0].i, (int)ins->a[1].i);
}

static int emit_sm_call_chunk(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return sm_emit_call_chunk(out, sm_template_lookup(SM_CALL_CHUNK),
                              (int)ins->a[0].i);
}

static int emit_sm_return(FILE *out, int pc)
{
    (void)pc;
    /* SM_RETURN: native return.  The chunk's last push left the result
     * on the SM value stack inside libscrip_rt.so. */
    return sm_emit_ret(out, sm_template_lookup(SM_RETURN), NULL);
}

/* -----------------------------------------------------------------------
 * SM_STNO -- statement boundary
 *
 * EM-4-readability:  the SM_STNO opcode marks a source-statement
 * boundary.  We use it to emit a major page-break banner showing the
 * verbatim source line, plus the runtime tick into &STNO/&STCOUNT.
 *
 * Operand layout (from sm_lower.c, EM-4):
 *   a[0].i  = source statement number (1-based)
 *   a[1].i  = source line number (added EM-4-readability; safe for
 *             interp because sm_interp.c reads only a[0].i)
 * ----------------------------------------------------------------------- */

static int emit_sm_stno(FILE *out, const SM_Instr *ins, int pc,
                        const SrcLines *sl)
{
    (void)pc;
    int stno   = (int)ins->a[0].i;
    int lineno = (int)ins->a[1].i;

    /* Lineno fallback: the parser only records lineno for *labeled*
     * statements (s->lineno = lbl.lineno).  For unlabeled statements
     * lineno comes through as 0.  In typical SNOBOL4 source one
     * statement occupies one line, so stno is an accurate estimator.
     * Any lookup that produces a non-blank source line is then a win;
     * mismatches produce blank-source banners (graceful degradation).
     * Future rung: have the parser record lineno on every statement
     * (one-line .y change) and remove this fallback. */
    int try_lineno = lineno;
    if (try_lineno <= 0 || (sl && try_lineno > sl->count))
        try_lineno = stno;

    char line_copy[1024];
    const char *src = NULL;
    if (sl) {
        const char *raw = srclines_get(sl, try_lineno);
        if (raw && *raw) {
            /* Copy and strip trailing CR if present (CRLF-friendly). */
            strncpy(line_copy, raw, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';
            srcline_strip_cr(line_copy);
            src = line_copy;
        }
    }

    /* If lineno was authoritatively recorded but out of range, suppress
     * the misleading "(line N)" suffix in the banner.  If we used the
     * stno-based fallback successfully, present try_lineno as the line. */
    int banner_lineno;
    if (lineno > 0 && (!sl || lineno <= sl->count)) {
        banner_lineno = lineno;
    } else if (src) {
        banner_lineno = try_lineno;   /* fallback hit something printable */
    } else {
        banner_lineno = 0;            /* truly unknown */
    }

    if (emit_major_break(out, stno, banner_lineno, src) != 0) return -1;

    /* SM_STNO is a source-statement boundary marker.  We render a
     * three-column line carrying the STNO macro name in col 2 so the
     * pending .LpcN: pc-label is consumed (and not left naked).  The
     * macro body is empty (.macro STNO\n.endm), so this line
     * assembles to nothing — &STNO / &STCOUNT runtime support is
     * deferred to a later rung.  EM-7c-stmt-banner-fidelity. */
    return sm_emit_noop(out, sm_template_lookup(SM_STNO), NULL);
}

/* -----------------------------------------------------------------------
 * EM-7-revert (session #72, 2026-05-07): the EM-6 emit_pat_call_*
 * helpers and emit_bb_box (the brokered Phase-3 dispatcher) are
 * REMOVED.  Lon's correction: the brokered runtime descriptor-tree
 * model — scrip_rt_pat_*@PLT building a runtime pat-stack, then
 * scrip_rt_exec_stmt → exec_stmt → bb_broker — was the wrong
 * architecture for emitted code.  See GOAL-MODE4-EMIT.md "Design
 * Discoveries" section for the corrected five-phase model.  EM-7a/b/c
 * will reintroduce pattern-side emit using the proven dual-mode
 * bb_emit infrastructure (bb_flat in EMIT_TEXT mode for invariant
 * sub-trees baked into .text; bb_emit BINARY mode for variant nodes
 * emitted into bb_pool RX memory at runtime; direct-call Phase-3 to
 * the root chunk's α — no broker, no pat-stack, no descriptor tree).
 * Until those rungs land, all SM_PAT_* opcodes fall through to
 * emit_sm_unhandled in the dispatch switch.
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * EM-7-pre keepers: SM_CALL_FN, SM_CONCAT, SM_PUSH_NULL, SM_COERCE_NUM,
 *                   SM_RETURN_S/F, SM_FRETURN[_S/_F], SM_NRETURN[_S/_F].
 * These are Phase 1/4/5 concerns, orthogonal to BB / pattern-matching.
 * ----------------------------------------------------------------------- */

/* SM_CONCAT: pop right then left; push CONCAT result.  All in libscrip_rt. */
static int emit_sm_concat(FILE *out, int pc)
{
    (void)pc;
    return sm_emit_nullary(out, sm_template_lookup(SM_CONCAT), NULL);
}

/* SM_PUSH_NULL: push null (empty-string) descriptor; sets last_ok=1. */
static int emit_sm_push_null(FILE *out, int pc)
{
    (void)pc;
    return sm_emit_nullary(out, sm_template_lookup(SM_PUSH_NULL), NULL);
}

/* SM_COERCE_NUM: unary +; coerce string→int/real if needed. */
static int emit_sm_coerce_num(FILE *out, int pc)
{
    (void)pc;
    return sm_emit_nullary(out, sm_template_lookup(SM_COERCE_NUM), NULL);
}

/* SM_CALL_FN: general function call.  All dispatch (pseudo-calls, builtins,
 * user-defined) lives in scrip_rt_call(name, nargs).
 *   a[0].s = function name (interned in strtab)
 *   a[1].i = nargs                                                       */
static int emit_sm_call(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name  = ins->a[0].s ? ins->a[0].s : "";
    int         nargs = (int)ins->a[1].i;
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    /* Annotation: show the unmangled fname (the args column shows the .Lstr_N
     * label which is opaque); CALL_FN macro name is in col 2. */
    snprintf(anno, sizeof(anno), "fname=\"%s\"", name);
    return sm_emit_lbl_int32(out, sm_template_lookup(SM_CALL_FN),
                             lbl, nargs, anno);
}

/* SM_RETURN_S / SM_RETURN_F / SM_FRETURN[_S/_F] / SM_NRETURN[_S/_F].
 *
 * Driven by the SM_RETURN_VARIANT template (one source of truth with
 * sm_macros' SM_RETURN_VARIANT macro).  kind/cond computed from opcode.
 * For unconditional plain SM_RETURN, emit_sm_return uses SM_RETURN. */
static int emit_sm_return_variant(FILE *out, sm_opcode_t op, int pc)
{
    int kind = 0;  /* RETURN */
    if (op == SM_FRETURN || op == SM_FRETURN_S || op == SM_FRETURN_F) kind = 1;
    if (op == SM_NRETURN || op == SM_NRETURN_S || op == SM_NRETURN_F) kind = 2;

    int cond = 0;  /* unconditional */
    if (op == SM_RETURN_S || op == SM_FRETURN_S || op == SM_NRETURN_S) cond = 1;
    if (op == SM_RETURN_F || op == SM_FRETURN_F || op == SM_NRETURN_F) cond = 2;

    return sm_emit_ret_var(out, kind, cond, pc, sm_opcode_name(op));
}

/* SM_PAT_CAPTURE_FN_ARGS / SM_PAT_USERCALL_ARGS emitters were removed
 * in EM-7-revert (session #72) along with the rest of the brokered
 * Phase-3 path.  See the EM-7-revert banner above emit_sm_concat for
 * rationale.  These opcodes fall through to emit_sm_unhandled until
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
 * pushes (SM_PUSH_LIT_S, SM_PUSH_LIT_I, SM_PUSH_VAR, SM_PAT_BOXVAL, etc.).
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
        case SM_PAT_BOXVAL:
            /* Moves top of pat-stack to value-stack side (used when
             * a pattern is stored in a variable for later DEREF).
             * The SimVal keeps its is_variant flag and is_pat flips. */
            if (ss.top > 0) ss.slots[ss.top - 1].is_pat = 0;
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
        case SM_PAT_FENCE:
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
 *    bb_build_flat_text(root, out, "_pat_inv_<id>") to bake a flat
 *    .text chunk with externally-visible α/β/γ/ω entry symbols.
 *
 * 3. Main dispatch:
 *      - SM_PAT_* and value-stack pushes inside an invariant Phase-2
 *        window: emit a NOP comment (the pattern is already baked).
 *      - SM_EXEC_STMT for an invariant statement: emit a call to
 *        scrip_rt_match_blob(_pat_inv_<id>_α, sname, has_repl).
 *      - Variant patterns / non-pattern statements: unchanged
 *        (variant patterns fall through to emit_sm_unhandled until
 *        the variant-runtime-emitter rung lands).
 *
 * Phase-1 (subject push) and Phase-4 (replacement push) are emitted
 * normally — they leave the value stack at [subj][repl_or_zero] just
 * before SM_EXEC_STMT, which is exactly the contract scrip_rt_match_blob
 * expects.
 * ======================================================================= */

#define MAX_PATTERN_WINDOWS 4096

typedef struct {
    int  phase2_start;       /* first SM pc inside Phase-2 (inclusive) */
    int  phase2_end;          /* one past last Phase-2 pc (exclusive) */
    int  exec_stmt_pc;        /* pc of the SM_EXEC_STMT instruction */
    int  pat_id;              /* unique id for the _pat_inv_<id>_* labels */
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
 * to emit_sm_unhandled in the main dispatch (variant rung is next). */
static void pattern_windows_collect(const SM_Program *prog)
{
    pattern_windows_reset();

    int stmt_start = 0;   /* PC of the first instruction in the current statement */

    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];

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
             * will handle them via emit_sm_unhandled.  Beauty.sno is
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
 * Each invariant pattern gets one labeled chunk with externally-visible
 * α/β/γ/ω labels (`_pat_inv_<id>_α` etc.). */
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

    if (fputs(
        "# ============================================================================\n"
        "# EM-7c: invariant pattern blobs (baked from sm_phase2_to_patnd → bb_build_flat_text)\n"
        "# Each block exposes _pat_inv_<id>_α / _β / _γ / _ω.\n"
        "# scrip_rt_match_blob(blob_α, ...) drives Phase-3 against these blobs.\n"
        "# ============================================================================\n",
        out) == EOF) return -1;
    if (emit_three_column_line(out, "", ".intel_syntax", "noprefix", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "", NULL) != 0) return -1;

    for (int i = 0; i < g_pat_windows_n; i++) {
        pattern_window_t *w = &g_pat_windows[i];
        if (!w->is_invariant) continue;

        char prefix[64];
        snprintf(prefix, sizeof(prefix), "_pat_inv_%d", w->pat_id);

        if (fprintf(out,
            "# ---- pattern blob %d (Phase-2 window pc=%d..%d, SM_EXEC_STMT pc=%d) ----\n",
            w->pat_id, w->phase2_start, w->phase2_end - 1,
            w->exec_stmt_pc) < 0) return -1;

        PATND_t *p = (PATND_t *)w->root.p;
        if (bb_build_flat_text(p, out, prefix) != 0) {
            fprintf(out, "# (bb_build_flat_text returned non-zero — pattern not baked)\n");
            /* If the bake fails despite the pre-pass marking it invariant,
             * downgrade — at SM_EXEC_STMT we'll fall back to UNHANDLED. */
            w->is_invariant = 0;
        }
    }
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

    /* Arg 0 (rdi) = blob_α = address of `_pat_inv_<id>_α`.
     * GAS Intel syntax: `lea rdi, [rip + symbol]`.  This first line
     * consumes the pending .LpcN: pc-label (sm_line routes through it). */
    char act[160];
    snprintf(act, sizeof(act),
             "rdi, [rip + _pat_inv_%d_α]", w->pat_id);
    char anno[80];
    snprintf(anno, sizeof(anno),
             "# blob entry α  (Phase-2 pc=%d..%d)",
             w->phase2_start, w->phase2_end - 1);
    /* Use emit_three_column_line to keep col 1 = pending .LpcN, col 2 = lea,
     * col 3 = args + comment.  The pending label is consumed via the
     * sm_emit_consume_pc_label path below. */
    char lbl[32];
    const char *pending = sm_emit_consume_pc_label();
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
        char ann2[80];
        snprintf(ann2, sizeof(ann2), "# subj_name=%s", sname);
        if (emit_three_column_line(out, "", "lea", act2, ann2) != 0) return -1;
    } else {
        if (emit_three_column_line(out, "", "xor", "esi, esi", "# subj_name=NULL") != 0) return -1;
    }

    /* Arg 2 (edx) = has_repl flag */
    char act3[80];
    snprintf(act3, sizeof(act3), "edx, %d", has_repl);
    char ann3[80];
    snprintf(ann3, sizeof(ann3), "# has_repl=%d", has_repl);
    if (emit_three_column_line(out, "", "mov", act3, ann3) != 0) return -1;

    if (emit_three_column_line(out, "", "call", "scrip_rt_match_blob@PLT",
                               "# EM-7c: Phase-3+5 against baked invariant blob") != 0) return -1;

    (void)pc;
    return 0;
}

/* Emit a real three-column line for an SM op that was absorbed into an
 * invariant pattern blob.  EM-7c-stmt-banner-fidelity: replaces the
 * earlier disembodied `# (baked into _pat_inv_<id> at .text — SM_*)`
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
 *   .LpcN:                  # PAT_RPOS       baked  _pat_inv_0 pc=7..12
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
    const char *pending = sm_emit_consume_pc_label();
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
             "baked  _pat_inv_%d pc=%d..%d",
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
 * as a thin call to the matching scrip_rt_pat_*() function.  The runtime
 * builds a PATND_t fragment on its pat-stack; SM_PAT_BOXVAL bridges to
 * the value stack as DT_P; SM_EXEC_STMT for variant patterns calls
 * scrip_rt_match_variant which delegates to exec_stmt.
 *
 * This is path β from the EM-7c-variant rung's design space (see
 * GOAL-MODE4-EMIT.md): runtime PATND_t reconstruction + bb_build_*-driven
 * Phase-3, distinct from EM-7-pre's reverted bb_broker route by virtue
 * of scrip_rt_init setting g_bb_mode = BB_MODE_LIVE.  The architectural
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
static int emit_sm_pat_lit(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "arg=\"%.40s\"%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return sm_emit_lblopt(out, sm_template_lookup(SM_PAT_LIT), l, anno);
    }
    return sm_emit_lblopt(out, sm_template_lookup(SM_PAT_LIT), l, NULL);
}

/* SM_PAT_REFNAME: a[0].s = var name. */
static int emit_sm_pat_refname(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "var=\"%.40s\"%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return sm_emit_lblopt(out, sm_template_lookup(SM_PAT_REFNAME), l, anno);
    }
    return sm_emit_lblopt(out, sm_template_lookup(SM_PAT_REFNAME), l, NULL);
}

/* SM_PAT_CAPTURE: a[0].s = varname, a[1].i = kind (0/1/2). */
static int emit_sm_pat_capture(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[80];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    int kind = (int)ins->a[1].i;
    if (l) {
        snprintf(anno, sizeof(anno), "var=%s kind=%d", ins->a[0].s, kind);
    } else {
        snprintf(anno, sizeof(anno), "kind=%d", kind);
    }
    return sm_emit_lblopt_int32(out, sm_template_lookup(SM_PAT_CAPTURE),
                                l, kind, anno);
}

/* SM_PAT_CAPTURE_FN: . *func() / $ *func() — a[0].s=fname, a[1].i=is_imm,
 *                    a[2].s=namelist.  LBLOPT3 shape. */
static int emit_sm_pat_capture_fn(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char fname_lbl[64], nl_lbl[64], anno[160];
    const char *fl = pat_arg_label(fname_lbl, sizeof(fname_lbl), ins->a[0].s);
    const char *nl = pat_arg_label(nl_lbl,    sizeof(nl_lbl),    ins->a[2].s);
    int is_imm = (int)ins->a[1].i;
    snprintf(anno, sizeof(anno),
             "fname=%s namelist=%s",
             fl ? ins->a[0].s : "(NULL)",
             nl ? ins->a[2].s : "(NULL)");
    return sm_emit_capture_fn(out, sm_template_lookup(SM_PAT_CAPTURE_FN),
                              fl, is_imm, nl, anno);
}

/* SM_PAT_CAPTURE_FN_ARGS: a[0].s=fname, a[1].i=is_imm, a[2].i=nargs. */
static int emit_sm_pat_capture_fn_args(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char fname_lbl[64], anno[128];
    const char *fl = pat_arg_label(fname_lbl, sizeof(fname_lbl), ins->a[0].s);
    int is_imm = (int)ins->a[1].i;
    int nargs  = (int)ins->a[2].i;
    snprintf(anno, sizeof(anno), "fname=%s",
             fl ? ins->a[0].s : "(NULL)");
    return sm_emit_capture_fn_args(out,
                                   sm_template_lookup(SM_PAT_CAPTURE_FN_ARGS),
                                   fl, is_imm, nargs, anno);
}

/* SM_PAT_USERCALL: bare *func() — a[0].s=fname. */
static int emit_sm_pat_usercall(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "fname=\"%.40s\"%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return sm_emit_lblopt(out, sm_template_lookup(SM_PAT_USERCALL), l, anno);
    }
    return sm_emit_lblopt(out, sm_template_lookup(SM_PAT_USERCALL), l, NULL);
}

/* SM_PAT_USERCALL_ARGS: a[0].s=fname, a[1].i=nargs. */
static int emit_sm_pat_usercall_args(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    int nargs = (int)ins->a[1].i;
    if (l) {
        snprintf(anno, sizeof(anno), "fname=\"%.40s\"", ins->a[0].s);
        return sm_emit_lblopt_int32(out, sm_template_lookup(SM_PAT_USERCALL_ARGS),
                                    l, nargs, anno);
    }
    return sm_emit_lblopt_int32(out, sm_template_lookup(SM_PAT_USERCALL_ARGS),
                                l, nargs, NULL);
}

/* SM_PAT_<no-arg>: dispatch via template lookup.  Each NULLARY-shape
 * pattern opcode (SPAN/BREAK/ANY/NOTANY/LEN/POS/RPOS/TAB/RTAB/ARB/
 * ARBNO/REM/FENCE/FENCE1/FAIL/ABORT/SUCCEED/BAL/EPS/CAT/ALT/DEREF/
 * BOXVAL) has its own template entry; sm_template_lookup picks the
 * right one and sm_emit_nullary writes the macro call.  No annotation
 * needed — the macro name in col 2 is self-describing. */
static int emit_sm_pat_noarg(FILE *out, sm_opcode_t op, int pc)
{
    (void)pc;
    const sm_op_template_t *t = sm_template_lookup(op);
    if (!t) return -1;
    return sm_emit_nullary(out, t, NULL);
}

/* SM_EXEC_STMT for a variant pattern: emit a scrip_rt_match_variant call.
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
    /* Order: lbl then has_repl in template; sm_emit_exec_var packs
     * them into args->lbl and args->i32_a.  But we need the
     * annotation too, so go through sm_emit_template directly. */
    sm_emit_args_t a = { 0 };
    a.lbl   = l;
    a.i32_a = has_repl;
    a.anno  = anno[0] ? anno : NULL;
    return sm_emit_template(out, sm_template_lookup(SM_EXEC_STMT), &a);
}


/* -----------------------------------------------------------------------
 * Unhandled opcode trap
 * ----------------------------------------------------------------------- */

static int emit_sm_unhandled(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    /* SM_UNHANDLED template: trap with the opcode int as edi.  Annotation
     * names the actual opcode -- the integer in args is opaque otherwise. */
    char anno[64];
    snprintf(anno, sizeof(anno), "%s", sm_opcode_name(ins->op));
    sm_emit_args_t a = { 0 };
    a.i32_a = (int)ins->op;
    a.anno  = anno;
    return sm_emit_template(out, sm_template_unhandled(), &a);
}

/* -----------------------------------------------------------------------
 * Top-level entry
 * ----------------------------------------------------------------------- */

int sm_codegen_x64_emit(SM_Program *prog, FILE *out, const char *src_path)
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
     * See sm_emit_template.{h,c}.
     *
     * The header is written to the current working directory by default
     * (GAS searches `.` for `.include` paths).  Callers that redirect the
     * .s into a specific output directory should arrange to have
     * sm_macros.s alongside it (or pre-write it once via
     * sm_emit_macro_library_to_path()).  We always (re)write it here so a
     * fresh emit run cannot pick up a stale header, and the
     * write-then-include pair is atomic from the caller's perspective. */
    if (sm_emit_macro_library_to_path("sm_macros.s") != 0) {
        fprintf(stderr,
                "sm_codegen_x64_emit: failed to write sm_macros.s "
                "(working directory writable?)\n");
        return -1;
    }
    if (emit_three_column_line(out, "", ".include", "\"sm_macros.s\"", NULL) != 0) return -1;

    /* EM-6: collect all string literals and variable names into the string
     * table, then emit them in .section .rodata before .text.  This makes
     * every string pointer in the emitted binary a RIP-relative reference
     * to a .Lstr_N label rather than an in-process pointer from the emitter. */
    strtab_collect(prog);
    if (strtab_emit_rodata(out) != 0) return -1;

    /* EM-7d-usercall-reentrant: emit .data chunk registry table for
     * user-defined SNOBOL4 functions (SM_LABEL instructions with a[0].s set).
     * This must come after strtab_emit_rodata (so .Lstr_N labels are defined)
     * and before .text (so .LpcN forward references resolve in the same TU). */
    int chunk_reg_count = emit_chunk_registry(out, prog);
    if (chunk_reg_count < 0) return -1;

    /* EM-7c: collect Phase-2 windows (one per SM_EXEC_STMT) and run the
     * Phase-2 simulator to reconstruct each pattern's PATND_t tree.
     * Fully-invariant patterns are emitted as flat .text chunks below;
     * variant patterns will be handled by a follow-up rung (their
     * SM_EXEC_STMT falls through to emit_sm_unhandled). */
    pattern_windows_collect(prog);
    if (emit_pattern_blobs(out) != 0) return -1;

    /* EM-4-readability: load the source file once if a path was given. */
    SrcLines sl;
    int sl_loaded = (srclines_load(&sl, src_path) == 0);

    if (emit_file_header(out, prog->count, chunk_reg_count > 0) != 0) {
        if (sl_loaded) srclines_free(&sl);
        return -1;
    }

    /* If we have source, print a header banner naming the file and stmt
     * count -- a one-line "table of contents" for the human reader. */
    if (sl_loaded) {
        fprintf(out,
            "# source-file: %s  (%d lines)\n"
            "# Each statement appears below as a major banner ('====') above\n"
            "# the asm it produced.  Inline annotations on the right column\n"
            "# show the source-level object referenced by each macro call.\n",
            sl.path, sl.count);
    }

    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];

        /* If the previous instruction did not consume its pending label
         * (e.g. an SM_LABEL or SM_STNO that emits no opcode line), flush
         * it now as a standalone label line so it remains a valid jump
         * target.  Then set the label for this instruction. */
        {
            const char *leftover = sm_emit_consume_pc_label();
            if (leftover && *leftover) {
                if (fprintf(out, "%s\n", leftover) < 0) {
                    if (sl_loaded) srclines_free(&sl);
                    return -1;
                }
            }
            char lbl[32];
            snprintf(lbl, sizeof(lbl), ".Lpc%d:", pc);
            sm_emit_set_pc_label(lbl);
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
         *      emit a call to scrip_rt_match_blob with the blob entry,
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
            case SM_HALT:         rc = emit_sm_halt(out, pc);            break;
            case SM_PUSH_LIT_I:   rc = emit_sm_push_lit_i(out, ins, pc); break;

            /* EM-3: string push, var load/store, pop, arithmetic */
            case SM_PUSH_LIT_S:   rc = emit_sm_push_lit_s(out, ins, pc); break;
            case SM_PUSH_VAR:     rc = emit_sm_push_var(out, ins, pc);   break;
            case SM_STORE_VAR:    rc = emit_sm_store_var(out, ins, pc);  break;
            case SM_VOID_POP:          rc = emit_sm_pop(out, pc);             break;
            case SM_ADD:
            case SM_SUB:
            case SM_MUL:
            case SM_DIV:
            case SM_MOD:          rc = emit_sm_arith(out, ins, pc);      break;

            /* EM-4: control flow.  SM_LABEL is a no-op (the .LpcN label at
             * every PC already serves as the target); SM_JUMP/S/F resolve
             * targets to baked-at-emit-time .Lpc<a[0].i>. */
            case SM_LABEL:        rc = emit_sm_label(out, ins, pc);      break;
            case SM_JUMP:         rc = emit_sm_jump(out, ins, pc);       break;
            case SM_JUMP_S:       rc = emit_sm_jump_s(out, ins, pc);     break;
            case SM_JUMP_F:       rc = emit_sm_jump_f(out, ins, pc);     break;

            /* EM-5: chunk descriptor push, baked-direct chunk call, return. */
            case SM_PUSH_CHUNK:   rc = emit_sm_push_chunk(out, ins, pc); break;
            case SM_CALL_CHUNK:   rc = emit_sm_call_chunk(out, ins, pc); break;
            case SM_RETURN:       rc = emit_sm_return(out, pc);          break;

            /* EM-7-pre keepers: SM_CALL_FN (general) + SM_CONCAT + SM_PUSH_NULL +
             * SM_COERCE_NUM + conditional return variants. */
            case SM_CALL_FN:         rc = emit_sm_call(out, ins, pc);       break;
            case SM_CONCAT:       rc = emit_sm_concat(out, pc);          break;
            case SM_PUSH_NULL:    rc = emit_sm_push_null(out, pc);       break;
            case SM_COERCE_NUM:   rc = emit_sm_coerce_num(out, pc);      break;
            case SM_FRETURN:
            case SM_NRETURN:
            case SM_RETURN_S:
            case SM_RETURN_F:
            case SM_FRETURN_S:
            case SM_FRETURN_F:
            case SM_NRETURN_S:
            case SM_NRETURN_F:    rc = emit_sm_return_variant(out, ins->op, pc); break;

            /* SM_STNO -- statement boundary; emits major page break w/ source. */
            case SM_STNO:         rc = emit_sm_stno(out, ins, pc,
                                                    sl_loaded ? &sl : NULL); break;

            /* EM-7c-variant (session #80, 2026-05-07): pattern-construction
             * opcodes.  Patterns inside an invariant Phase-2 window are
             * absorbed into the .text-baked _pat_inv_<id>_* blob (handled
             * by the pattern_window_at_pc hook above this switch).  Patterns
             * outside any window — either variant (the runtime Phase-2
             * window built a tree with at least one variant node) or pattern-
             * as-rvalue (e.g., `WPAT = BREAK(WORD) SPAN(WORD)` builds a
             * pattern but does not exec one) — are emitted as PLT calls to
             * the libscrip_rt pat-construction ABI.
             *
             * NB: the brokered Phase-3 path that EM-7-revert tore out is
             * NOT what this rung restores.  The architectural distinction
             * is in scrip_rt_init's setting g_bb_mode = BB_MODE_LIVE so that
             * exec_stmt's Phase-3 routes through bb_build_flat / bb_build_binary
             * → direct bb_box_fn call, not through bb_broker.  See
             * GOAL-MODE4-EMIT.md "Design Discoveries" section; the bb_pool-
             * per-variant-node ideal is a follow-up rung. */
            case SM_PAT_LIT:      rc = emit_sm_pat_lit(out, ins, pc);     break;
            case SM_PAT_REFNAME:  rc = emit_sm_pat_refname(out, ins, pc); break;
            case SM_PAT_CAPTURE:      rc = emit_sm_pat_capture(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN:   rc = emit_sm_pat_capture_fn(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN_ARGS: rc = emit_sm_pat_capture_fn_args(out, ins, pc); break;
            case SM_PAT_USERCALL:     rc = emit_sm_pat_usercall(out, ins, pc); break;
            case SM_PAT_USERCALL_ARGS: rc = emit_sm_pat_usercall_args(out, ins, pc); break;
            case SM_PAT_SPAN:
            case SM_PAT_BREAK:
            case SM_PAT_ANY:
            case SM_PAT_NOTANY:
            case SM_PAT_LEN:
            case SM_PAT_POS:
            case SM_PAT_RPOS:
            case SM_PAT_TAB:
            case SM_PAT_RTAB:
            case SM_PAT_ARB:
            case SM_PAT_ARBNO:
            case SM_PAT_REM:
            case SM_PAT_FENCE:
            case SM_PAT_FENCE1:
            case SM_PAT_FAIL:
            case SM_PAT_ABORT:
            case SM_PAT_SUCCEED:
            case SM_PAT_BAL:
            case SM_PAT_EPS:
            case SM_PAT_CAT:
            case SM_PAT_ALT:
            case SM_PAT_DEREF:
            case SM_PAT_BOXVAL:   rc = emit_sm_pat_noarg(out, ins->op, pc); break;

            /* SM_EXEC_STMT for a variant pattern (invariant patterns are
             * already handled above by the pattern-window hook → blob call). */
            case SM_EXEC_STMT:    rc = emit_sm_exec_stmt_variant(out, ins, pc); break;

            default:              rc = emit_sm_unhandled(out, ins, pc);  break;
        }
        if (rc != 0) {
            if (sl_loaded) srclines_free(&sl);
            return -1;
        }
    }

    /* Flush any final unused pending label (e.g. SM_LABEL at the very end). */
    {
        const char *leftover = sm_emit_consume_pc_label();
        if (leftover && *leftover) {
            if (fprintf(out, "%s\n", leftover) < 0) {
                if (sl_loaded) srclines_free(&sl);
                return -1;
            }
        }
    }

    int frc = emit_file_footer(out);
    if (sl_loaded) srclines_free(&sl);
    return frc;
}
