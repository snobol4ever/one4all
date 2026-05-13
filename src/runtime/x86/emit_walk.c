

#include "emit_walk.h"
#include "sm_image.h"
#include "sm_prog.h"
#include "snobol4.h"
#include "bb_broker.h"
#include "emit_bb_gen.h"
#include "emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "emit_sm_shape.h"
#include "emit_flat.h"
#include <assert.h>


#include "emit_sm_shape.h"
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

int g_emit_inline = 0;

#define TEXT_MODE() (g_emit_inline ? EMIT_TEXT_INLINE : EMIT_TEXT)

typedef struct {
    char       *buf;
    char      **lines;
    int         count;
    const char *path;
} SrcLines;

/* Slurp src_path into sl.  Returns 0 on success, -1 on error. */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
    int count = 0;
    for (size_t i = 0; i < got; i++) if (buf[i] == '\n') count++;
    if (got > 0 && buf[got-1] != '\n') count++;
    char **lines = calloc((size_t)count + 2, sizeof(char *));
    if (!lines) { free(buf); return -1; }
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void srclines_free(SrcLines *sl)
{
    free(sl->lines);
    free(sl->buf);
    memset(sl, 0, sizeof(*sl));
}

static const char *srclines_get(const SrcLines *sl, int lineno)
{
    if (!sl || !sl->lines || lineno < 1 || lineno > sl->count) return NULL;
    return sl->lines[lineno];
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void srcline_strip_cr(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    if (n > 0 && s[n-1] == '\r') s[n-1] = '\0';
}

#define STRTAB_CAP 8192

typedef struct {
    const char *s;
    int         idx;
} StrEntry;

static StrEntry g_strtab[STRTAB_CAP];
static int      g_strtab_n = 0;

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void strtab_reset(void)
{
    g_strtab_n = 0;
}

/* Intern a string; return its label index (0-based). */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int strtab_intern(const char *s)
{
    if (!s) s = "";
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int strtab_lookup(const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0)
            return g_strtab[i].idx;
    return -1;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void strtab_label(char *buf, size_t bufsz, const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0) {
            snprintf(buf, bufsz, ".S%d", g_strtab[i].idx);
            return;
        }
    snprintf(buf, bufsz, ".S_ERR");
}

static char g_intern_str_buf[64];
static const char *codegen_intern_str(const char *s)
{
    if (s) strtab_intern(s);
    strtab_label(g_intern_str_buf, sizeof(g_intern_str_buf), s ? s : "");
    return g_intern_str_buf;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void strtab_collect(const SM_Program *prog)
{
    strtab_reset();
    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        switch (ins->op) {
        case SM_PUSH_LIT_S:
        case SM_PUSH_VAR:
        case SM_STORE_VAR:
        case SM_PAT_LIT:
        case SM_PAT_REFNAME:
        case SM_PAT_CAPTURE:
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_USERCALL:
        case SM_PAT_CAPTURE_FN_ARGS:
        case SM_PAT_USERCALL_ARGS:
        case SM_EXEC_STMT:
        case SM_CALL_FN:
        case SM_LABEL:
            if (ins->a[0].s) strtab_intern(ins->a[0].s);
            break;
        default:
            break;
        }
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


static uint8_t *g_pc_used_as_target = NULL;
static int      g_pc_used_count     = 0;

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
    g_pc_used_as_target[0] = 1;
    return 0;
}

static inline void pc_used_mark(int pc)
{
    if (g_pc_used_as_target && pc >= 0 && pc < g_pc_used_count)
        g_pc_used_as_target[pc] = 1;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int pc_is_used_as_target(int pc)
{
    if (!g_pc_used_as_target) return 1;
    if (pc < 0 || pc >= g_pc_used_count) return 1;
    return g_pc_used_as_target[pc] ? 1 : 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void release_pc_used_as_target(void)
{
    if (g_pc_used_as_target) {
        free(g_pc_used_as_target);
        g_pc_used_as_target = NULL;
        g_pc_used_count = 0;
    }
}

static int emit_three_column_line(FILE *out,
                                  const char *label,
                                  const char *opcode,
                                  const char *col3,
                                  const char *anno)
{
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
    bb3c_format(out, label ? label : "", opcode ? opcode : "", c3);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/* EM-7d-usercall-reentrant: emit .datan expression registry table. */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_expression_registry(FILE *out, const SM_Program *prog)
{
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
        if (ins->op != SM_LABEL || !ins->a[0].s || !*ins->a[0].s || ins->a[2].i != 1) continue;
        int str_idx = strtab_lookup(ins->a[0].s);
        if (str_idx < 0) continue;
        int entry_pc = i + 1;
        char qarg[32];
        snprintf(qarg, sizeof(qarg), ".S%d", str_idx);
        char combined[128];
        snprintf(combined, sizeof(combined), "%-16s ; .quad            .L%d", qarg, entry_pc);
        if (emit_three_column_line(out, "", ".quad", combined, NULL) != 0) return -1;
    }
    if (emit_three_column_line(out, "", ".quad", "0                ; .quad            0", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "",  NULL)        != 0) return -1;
    return n;
}

#define MAX_CAP_FIXUPS 1024
typedef struct {
    void       *cap_ptr;
    char        child_label[128];
} cap_fixup_t;

static cap_fixup_t g_cap_fixups[MAX_CAP_FIXUPS];
static int         g_cap_fixups_n = 0;

static void cap_fixups_reset(void) { g_cap_fixups_n = 0; }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void cap_fixup_add(void *cap_ptr, const char *child_label)
{
    if (g_cap_fixups_n >= MAX_CAP_FIXUPS) return;
    g_cap_fixups[g_cap_fixups_n].cap_ptr = cap_ptr;
    snprintf(g_cap_fixups[g_cap_fixups_n].child_label,
             sizeof(g_cap_fixups[0].child_label), "%s", child_label);
    g_cap_fixups_n++;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_file_header(FILE *out, int count, int has_expression_registry)
{
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_file_footer(FILE *out)
{
    bb3c_flush_pending();
    if (emit_three_column_line(out, "", "call", "rt_finalize@PLT", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "pop",  "rbp", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "ret",  "",    NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".size", "main, .-main", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".section", ".note.GNU-stack,\"\",@progbits", NULL) != 0) return -1;
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int sm_line(FILE *out, const char *label, const char *action,
                   const char *goto_col)
{
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
    const char *lbl;
    if (label && *label) {
        lbl = label;
    } else {
        lbl = emit_sm_consume_pc_label();
    }
    const char *act = (action && *action) ? action : "";
    bb3c_format(out, (lbl && *lbl) ? lbl : "", act, gc);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_major_break(FILE *out, int stno, int lineno,
                            const char *src_text)
{
    bb3c_flush_pending();
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_minor_break(FILE *out, const char *caption)
{
    bb3c_flush_pending();
    if (fputs("#-----------------------------------------------------------------------------------------------------------------------\n",
              out) == EOF) return -1;
    if (caption && *caption) {
        if (fprintf(out, "# %s\n", caption) < 0) return -1;
        if (fputs("#-----------------------------------------------------------------------------------------------------------------------\n",
                  out) == EOF) return -1;
    }
    return 0;
}

#define STR_PREVIEW_MAX  40
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_pc_label(FILE *out, int pc)
{
    return fprintf(out, ".L%d:\n", pc) < 0 ? -1 : 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_halt_line(FILE *out, int pc)
{
    (void)pc;
    return emit_sm_rtcall(out, sm_template_lookup(SM_HALT), NULL);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_push_lit_i_line(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return emit_sm_int64(out, sm_template_lookup(SM_PUSH_LIT_I),
                         ins->a[0].i, NULL);
}

__attribute__((unused))
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_push_lit_i_legacy(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return emit_sm_int64(out, sm_template_lookup(SM_PUSH_LIT_I),
                         ins->a[0].i, NULL);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_push_var_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl(out, sm_template_lookup(SM_PUSH_VAR), lbl, anno);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_store_var_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl(out, sm_template_lookup(SM_STORE_VAR), lbl, anno);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_pop(FILE *out, int pc)
{
    (void)pc;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_op(SM_VOID_POP);
    return 0;
}

__attribute__((unused))
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_pop_legacy(FILE *out, int pc)
{
    (void)pc;
    return emit_sm_rtcall(out, sm_template_lookup(SM_VOID_POP), NULL);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int edp4_sm_arith(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const sm_op_template_t *t = sm_template_lookup(ins->op);
    if (!t) return -1;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_arith_op((int)ins->op, t->macro_name);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_label_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)ins; (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_label();
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_jump_line(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_jump((int)ins->a[0].i);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_jump_cond(FILE *out, const SM_Instr *ins, int pc,
                             int take_when_ok)
{
    (void)pc;
    int  target = (int)ins->a[0].i;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    if (take_when_ok) emit_sm_jump_s(target);
    else              emit_sm_jump_f(target);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_jump_s_line(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, 1);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_jump_f_line(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, 0);
}

/* ----------------------------------------------------------------------- */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_push_expression_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return edp4_emit_push_expression(out, sm_template_lookup(SM_PUSH_EXPRESSION),
                              ins->a[0].i, (int)ins->a[1].i);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_call_expression_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return edp4_emit_call_expression(out, sm_template_lookup(SM_CALL_EXPRESSION),
                              (int)ins->a[0].i);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_return_dispatch(FILE *out, int pc)
{
    (void)pc;
    return emit_sm_ret(out, sm_template_lookup(SM_RETURN), NULL);
}

/* ----------------------------------------------------------------------- */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_stno_dispatch(FILE *out, const SM_Instr *ins, int pc,
                        const SrcLines *sl)
{
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

/* ----------------------------------------------------------------------- */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_concat_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_CONCAT);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_push_null_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_PUSH_NULL);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_coerce_num_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_coerce_num();
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_push_null_noflip_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_PUSH_NULL_NOFLIP);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_push_lit_f_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_push_lit_f(ins->a[0].f);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_push_expr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_push_expr((uint64_t)(uintptr_t)ins->a[0].ptr);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_incr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_incr(ins->a[0].i);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_decr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_decr(ins->a[0].i);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_acomp_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_acomp((int)ins->a[0].i);
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_lcomp_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_lcomp((int)ins->a[0].i);
    return 0;
}

/* SM_CALL_FN: general function call.  All dispatch (pseudo-calls, builtins, */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_return_variant_dispatch(FILE *out, sm_opcode_t op, int pc)
{
    int kind = 0;
    if (op == SM_FRETURN || op == SM_FRETURN_S || op == SM_FRETURN_F) kind = 1;
    if (op == SM_NRETURN || op == SM_NRETURN_S || op == SM_NRETURN_F) kind = 2;
    int cond = 0;
    if (op == SM_RETURN_S || op == SM_FRETURN_S || op == SM_NRETURN_S) cond = 1;
    if (op == SM_RETURN_F || op == SM_FRETURN_F || op == SM_NRETURN_F) cond = 2;
    return emit_sm_ret_var(out, kind, cond, pc, sm_opcode_name(op));
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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


#define PHASE2_SIM_DEPTH  128

typedef struct {
    DESCR_t val;
    int     is_pat;
    int     is_variant;
} SimVal;

typedef struct {
    SimVal slots[PHASE2_SIM_DEPTH];
    int    top;
} SimStack;

static void simstack_init(SimStack *ss) { ss->top = 0; }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void simstack_push(SimStack *ss, SimVal v)
{
    if (ss->top < PHASE2_SIM_DEPTH) ss->slots[ss->top++] = v;
}

static SimVal simstack_pop(SimStack *ss)
{
    if (ss->top > 0) return ss->slots[--ss->top];
    SimVal v; v.val = pat_epsilon(); v.is_pat = 1; v.is_variant = 1;
    return v;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void simstack_push_variant_val(SimStack *ss)
{
    SimVal v;
    v.val = pat_epsilon();
    v.is_pat = 0;
    v.is_variant = 1;
    simstack_push(ss, v);
}

static SimVal make_pat_val(DESCR_t d, int is_variant)
{
    SimVal v;
    v.val = d;
    v.is_pat = 1;
    v.is_variant = is_variant;
    return v;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
int emit_flat_eligible(const PATND_t *p)
{
    if (!p) return 1;
    return p->kind != XVAR;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
int emit_flat_invariant(const PATND_t *p)
{
    if (!p) return 1;
    if (!flat_is_eligible_node(p)) return 0;
    if (p->kind == XCAT && p->nchildren > 2) return 0;
    for (int i = 0; i < p->nchildren; i++)
        if (!patnd_is_fully_invariant(p->children[i])) return 0;
    return 1;
}

static PATND_t *patnd_of(DESCR_t d)
{
    if (d.v != DT_P || !d.s) return NULL;
    return (PATND_t *)d.s;
}

/* Walk SM instructions [phase2_start, phase2_end) and simulate Phase-2 */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t emit_walk_phase2(const SM_Program *prog,
                            int phase2_start, int phase2_end,
                            int *out_variant)
{
    SimStack ss;
    simstack_init(&ss);
    int has_variant = 0;
    for (int pc = phase2_start; pc < phase2_end; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];
        switch (ins->op) {
        case SM_PUSH_LIT_S:
            simstack_push_const_s(&ss, ins->a[0].s ? ins->a[0].s : "");
            break;
        case SM_PUSH_LIT_I:
            simstack_push_const_i(&ss, ins->a[0].i);
            break;
        case SM_PUSH_VAR:
            simstack_push_variant_val(&ss);
            has_variant = 1;
            break;
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
            simstack_push(&ss, make_pat_val(pat_fence(), 0));
            break;
        case SM_PAT_LIT: {
            const char *s = ins->a[0].s ? ins->a[0].s : "";
            simstack_push(&ss, make_pat_val(pat_lit(s), 0));
            break;
        }
        case SM_PAT_SPAN: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
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
            simstack_push(&ss, make_pat_val(pat_len(n), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_POS: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            int v = arg.is_variant;
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
        case SM_PAT_ARBNO: {
            SimVal inner = simstack_pop(&ss);
            int v = inner.is_variant;
            simstack_push(&ss, make_pat_val(pat_arbno(inner.val), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_FENCE1: {
            SimVal inner = simstack_pop(&ss);
            simstack_push(&ss, make_pat_val(pat_fence_p(inner.val), inner.is_variant));
            if (inner.is_variant) has_variant = 1;
            break;
        }
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
        case SM_PAT_DEREF: {
            SimVal arg = simstack_pop(&ss);
            DESCR_t d;
            if (arg.val.v == DT_S && arg.val.s)
                d = pat_ref(arg.val.s);
            else
                d = pat_epsilon();
            simstack_push(&ss, make_pat_val(d, 0));
            break;
        }
        case SM_PAT_REFNAME: {
            const char *name = ins->a[0].s ? ins->a[0].s : "";
            simstack_push(&ss, make_pat_val(pat_ref(name), 0));
            break;
        }
        case SM_PAT_CAPTURE: {
            SimVal child = simstack_pop(&ss);
            const char *vname = ins->a[0].s ? ins->a[0].s : "";
            int kind = (int)ins->a[1].i;
            DESCR_t var = NAME_fn(vname);
            DESCR_t d;
            if (kind == 1) d = pat_assign_imm(child.val, var);
            else           d = pat_assign_cond(child.val, var);
            simstack_push(&ss, make_pat_val(d, child.is_variant));
            if (child.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_CAPTURE_FN_ARGS:
        case SM_PAT_USERCALL:
        case SM_PAT_USERCALL_ARGS: {
            SimVal child = simstack_pop(&ss);
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            DESCR_t d = pat_assign_callcap(child.val, fname, NULL, 0);
            simstack_push(&ss, make_pat_val(d, 1));
            has_variant = 1;
            break;
        }
        default:
            simstack_push_variant_val(&ss);
            has_variant = 1;
            break;
        }
    }
    *out_variant = has_variant;
    if (ss.top == 0) return pat_epsilon();
    return ss.slots[ss.top - 1].val;
}


#define MAX_PATTERN_WINDOWS 4096

typedef struct {
    int  phase2_start;
    int  phase2_end;
    int  exec_stmt_pc;
    int  pat_id;
    int  is_invariant;
    DESCR_t root;
} pattern_window_t;

static pattern_window_t g_pat_windows[MAX_PATTERN_WINDOWS];
static int              g_pat_windows_n   = 0;
static int              g_pat_windows_id  = 0;

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void pattern_windows_reset(void)
{
    g_pat_windows_n  = 0;
    g_pat_windows_id = 0;
    cap_fixups_reset();
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int pattern_window_at_pc(int pc)
{
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (pc >= g_pat_windows[i].phase2_start &&
            pc <  g_pat_windows[i].phase2_end)
            return i;
    }
    return -1;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int pattern_window_for_exec_stmt(int pc)
{
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (g_pat_windows[i].exec_stmt_pc == pc)
            return i;
    }
    return -1;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void pattern_windows_collect(const SM_Program *prog)
{
    pattern_windows_reset();
    pc_used_alloc(prog);
    int stmt_start = 0;
    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];
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
                pc_used_mark(pc + 1);
            break;
        default:
            break;
        }
        if (ins->op == SM_STNO) {
            stmt_start = pc + 1;
            continue;
        }
        if (ins->op != SM_EXEC_STMT) continue;
        int phase2_end = pc - 2;
        if (phase2_end < stmt_start) phase2_end = stmt_start;
        int has_variant = 0;
        DESCR_t root = sm_phase2_to_patnd(prog, stmt_start, phase2_end, &has_variant);
        if (g_pat_windows_n >= MAX_PATTERN_WINDOWS) {
            continue;
        }
        pattern_window_t *w = &g_pat_windows[g_pat_windows_n++];
        w->phase2_start = stmt_start;
        w->phase2_end   = phase2_end;
        w->exec_stmt_pc = pc;
        w->pat_id       = g_pat_windows_id++;
        w->root         = root;
        PATND_t *p = (PATND_t *)root.p;
        w->is_invariant = (!has_variant && p && patnd_is_fully_invariant(p)) ? 1 : 0;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_pattern_blobs(FILE *out)
{
    int n_invariant = 0;
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (g_pat_windows[i].is_invariant) n_invariant++;
    }
    if (n_invariant == 0) return 0;
    emit_flat_set_intern_str(codegen_intern_str);
    emit_flat_set_cap_fixup(cap_fixup_add);
    emit_flat_reset();
    if (emit_three_column_line(out, "", ".intel_syntax", "noprefix", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "", NULL) != 0) return -1;
    for (int i = 0; i < g_pat_windows_n; i++) {
        pattern_window_t *w = &g_pat_windows[i];
        if (!w->is_invariant) continue;
        char prefix[64];
        snprintf(prefix, sizeof(prefix), "pat_inv_%d", w->pat_id);
        PATND_t *p = (PATND_t *)w->root.p;
        if (emit_flat_build(p, out, prefix) != 0) {
            w->is_invariant = 0;
        }
    }
    bb3c_flush_pending();
    return 0;
}

/* SM_EXEC_STMT for a registered invariant pattern: emit the runtime call. */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_exec_stmt_blob(FILE *out, const SM_Instr *ins, int pc, int win_idx)
{
    pattern_window_t *w = &g_pat_windows[win_idx];
    const char *sname = ins->a[0].s;
    int has_repl      = (int)ins->a[1].i;
    char act[160];
    snprintf(act, sizeof(act),
             "rdi, [rip + pat_inv_%d_α]", w->pat_id);
    const char *anno = NULL;
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
    if (sname && *sname) {
        char lbl_str[64];
        strtab_label(lbl_str, sizeof(lbl_str), sname);
        char act2[160];
        snprintf(act2, sizeof(act2), "rsi, [rip + %s]", lbl_str);
        if (emit_three_column_line(out, "", "lea", act2, NULL) != 0) return -1;
    } else {
        if (emit_three_column_line(out, "", "xor", "esi, esi", NULL) != 0) return -1;
    }
    char act3[80];
    snprintf(act3, sizeof(act3), "edx, %d", has_repl);
    if (emit_three_column_line(out, "", "mov", act3, NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "call", "rt_match_blob@PLT", NULL) != 0) return -1;
    (void)pc;
    return 0;
}

/* Emit a real three-column line for an SM op that was absorbed into an */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_pat_baked(FILE *out, const SM_Instr *ins, int pc, int win_idx)
{
    pattern_window_t *w = &g_pat_windows[win_idx];
    const sm_op_template_t *t = sm_template_lookup(ins->op);
    const char *opname = (t && t->macro_name) ? t->macro_name
                                              : sm_opcode_name(ins->op);
    if (!opname) opname = "?";
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

/* ----------------------------------------------------------------------- */

static const char *pat_arg_label(char *lbl_buf, size_t lbl_buf_n,
                                 const char *arg)
{
    if (!arg || !*arg) return NULL;
    strtab_label(lbl_buf, lbl_buf_n, arg);
    return lbl_buf;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/* SM_PAT_<no-arg>: dispatch via template lookup.  Each NULLARY-shape */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_pat_noarg(FILE *out, sm_opcode_t op, int pc)
{
    (void)pc;
    const sm_op_template_t *t = sm_template_lookup(op);
    if (!t) return -1;
    return emit_sm_rtcall(out, t, NULL);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_sm_exec_stmt_variant(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *sname = ins->a[0].s;
    int has_repl      = (int)ins->a[1].i;
    char lbl[64];
    const char *l = pat_arg_label(lbl, sizeof(lbl), sname);
    char anno[128];
    if (l) {
        snprintf(anno, sizeof(anno), "subj=%s", sname);
    } else {
        anno[0] = '\0';
    }
    emit_sm_args_t a = { 0 };
    a.lbl   = l;
    a.i32_a = has_repl;
    a.anno  = anno[0] ? anno : NULL;
    return emit_sm_template(out, sm_template_lookup(SM_EXEC_STMT), &a);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int edp4_sm_unhandled(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char anno[64];
    snprintf(anno, sizeof(anno), "%s", sm_opcode_name(ins->op));
    emit_sm_args_t a = { 0 };
    a.i32_a = (int)ins->op;
    a.anno  = anno;
    return emit_sm_template(out, sm_template_unhandled(), &a);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
int emit_walk_codegen(SM_Program *prog, FILE *out, const char *src_path)
{
    assert(prog != NULL);
    assert(out  != NULL);
    if (!g_emit_inline) {
        if (emit_sm_macro_library_to_path("sm_macros.s") != 0) {
            fprintf(stderr,
                    "sm_codegen_text: failed to write sm_macros.s "
                    "(working directory writable?)\n");
            return -1;
        }
        if (emit_three_column_line(out, "", ".include", "\"sm_macros.s\"", NULL) != 0) return -1;
        if (emit_flat_macros_to_path("bb_macros.s") != 0) {
            fprintf(stderr, "sm_codegen_text: failed to write bb_macros.s\n");
            return -1;
        }
        if (emit_three_column_line(out, "", ".include", "\"bb_macros.s\"", NULL) != 0) return -1;
    }
    strtab_collect(prog);
    if (strtab_emit_rodata(out) != 0) return -1;
    int expression_reg_count = emit_expression_registry(out, prog);
    if (expression_reg_count < 0) return -1;
    pattern_windows_collect(prog);
    if (emit_pattern_blobs(out) != 0) return -1;
    SrcLines sl;
    int sl_loaded = (srclines_load(&sl, src_path) == 0);
    if (emit_file_header(out, prog->count, expression_reg_count > 0) != 0) {
        if (sl_loaded) srclines_free(&sl);
        return -1;
    }
    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];
        {
            const char *leftover = emit_sm_consume_pc_label();
            if (leftover && *leftover) {
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
            case SM_HALT:         rc = emit_halt_line(out, pc);          break;
            case SM_PUSH_LIT_I:   rc = emit_push_lit_i_line(out, ins, pc); break;
            case SM_PUSH_LIT_F:   rc = emit_sm_push_lit_f_dispatch(out, ins, pc);  break;
            case SM_PUSH_EXPR:    rc = emit_sm_push_expr_dispatch(out, ins, pc);   break;
            case SM_PUSH_LIT_S:   rc = emit_sm_push_lit_s_dispatch(out, ins, pc); break;
            case SM_PUSH_VAR:     rc = emit_sm_push_var_dispatch(out, ins, pc);   break;
            case SM_STORE_VAR:    rc = emit_sm_store_var_dispatch(out, ins, pc);  break;
            case SM_VOID_POP:          rc = emit_sm_pop(out, pc);             break;
            case SM_ADD:
            case SM_SUB:
            case SM_MUL:
            case SM_DIV:
            case SM_MOD:          rc = edp4_sm_arith(out, ins, pc);      break;
            case SM_LABEL:        rc = emit_sm_label_dispatch(out, ins, pc);      break;
            case SM_JUMP:         rc = emit_sm_jump_line(out, ins, pc);   break;
            case SM_JUMP_S:       rc = emit_sm_jump_s_line(out, ins, pc); break;
            case SM_JUMP_F:       rc = emit_sm_jump_f_line(out, ins, pc); break;
            case SM_PUSH_EXPRESSION:   rc = emit_sm_push_expression_dispatch(out, ins, pc); break;
            case SM_CALL_EXPRESSION:   rc = emit_sm_call_expression_dispatch(out, ins, pc); break;
            case SM_RETURN:       rc = emit_sm_return_dispatch(out, pc);          break;
            case SM_DEFINE_ENTRY: rc = emit_sm_define_entry_dispatch(out, ins, pc, prog); break;
            case SM_DEFINE:       rc = emit_sm_define_dispatch(out, ins, pc);              break;
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
            case SM_STNO:         rc = emit_sm_stno_dispatch(out, ins, pc,
                                                    sl_loaded ? &sl : NULL); break;
            case SM_PAT_LIT:      rc = emit_sm_pat_lit_dispatch(out, ins, pc);     break;
            case SM_PAT_REFNAME:  rc = emit_sm_pat_refname_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE:      rc = emit_sm_pat_capture_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN:   rc = emit_sm_pat_capture_fn_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN_ARGS: rc = emit_sm_pat_capture_fn_args_dispatch(out, ins, pc); break;
            case SM_PAT_USERCALL:     rc = emit_sm_pat_usercall_dispatch(out, ins, pc); break;
            case SM_PAT_USERCALL_ARGS: rc = emit_sm_pat_usercall_args_dispatch(out, ins, pc); break;
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
            case SM_EXEC_STMT:    rc = emit_sm_exec_stmt_variant(out, ins, pc); break;
            default:              rc = edp4_sm_unhandled(out, ins, pc);  break;
        }
        if (rc != 0) {
            if (sl_loaded) srclines_free(&sl);
            return -1;
        }
    }
    {
        const char *leftover = emit_sm_consume_pc_label();
        if (leftover && *leftover) {
            bb3c_format(out, leftover, "", "");
        }
    }
    int frc = emit_file_footer(out);
    bb3c_flush_pending();
    if (sl_loaded) srclines_free(&sl);
    release_pc_used_as_target();
    return frc;
}
