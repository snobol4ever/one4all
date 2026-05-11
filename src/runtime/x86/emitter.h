/*
 * emitter.h — Emitter vtable + instruction-description layer
 *
 * Two levels of abstraction:
 *
 * Level 1 — vtable (emitter_t):
 *   One struct, two implementations (emitter_text, emitter_binary).
 *   The walker (bb_flat.c) takes emitter_t * and is generic over target.
 *
 * Level 2 — instruction descriptions (bb_insn_desc_t + emit_insn):
 *   Named instruction kinds with typed args.  TEXT renders real GAS
 *   mnemonics; BINARY renders the corresponding bytes.  The walker
 *   calls named inline helpers (em_load_delta, em_mov_rax_imm64, ...)
 *   which each build a bb_insn_desc_t and call e->emit_insn(e, &d).
 *   Zero byte knowledge in bb_flat.c.
 *
 * For the Milestone-3 matrix (x86/JVM/.NET/WASM/JS) and Snocone
 * bootstrap (EXPRESSION as templates): each backend column is one
 * emitter_t instance; the walker and helper layer are unchanged.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-7b'' / GOAL-MODE4-EMIT
 */

#ifndef EMITTER_H
#define EMITTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include "bb_emit.h"   /* bb_label_t, bb_patch_kind_t, bb_buf_t */

/* ── jump kind ────────────────────────────────────────────────────────────── */

typedef enum {
    JMP_JMP = 0,
    JMP_JE,
    JMP_JNE,
    JMP_JL,
    JMP_JGE,
    JMP_JG,
} jmp_kind_t;

/* ── instruction kinds ────────────────────────────────────────────────────── */
/*
 * Every distinct instruction form used by bb_flat.c.  TEXT renders a
 * readable GAS line; BINARY renders the corresponding x86-64 bytes.
 * Argument slots:  a0=imm64/addr, a1=imm32, a2=imm8, lbl=label ptr.
 */
typedef enum {
    BB_INSN_MOV_R10_IMM64,    /* mov r10, imm64        49 BA <8> */
    BB_INSN_MOV_RAX_IMM64,    /* mov rax, imm64        48 B8 <8> */
    BB_INSN_MOV_RDI_IMM64,    /* mov rdi, imm64        48 BF <8> */
    BB_INSN_MOV_RSI_IMM64,    /* mov rsi, imm64        48 BE <8> */  /* unused; kept for completeness */
    BB_INSN_MOV_RDX_IMM64,    /* mov rdx, imm64        48 BA <8> */
    BB_INSN_MOV_ESI_IMM32,    /* mov esi, imm32        BE <4> */
    BB_INSN_MOV_EAX_IMM32,    /* mov eax, imm32        B8 <4> */
    BB_INSN_ADD_EAX_IMM32,    /* add eax, imm32        05 <4> */
    BB_INSN_SUB_EAX_IMM32,    /* sub eax, imm32        2D <4> */
    BB_INSN_CMP_EAX_IMM32,    /* cmp eax, imm32        3D <4> */
    BB_INSN_CMP_ESI_IMM8,     /* cmp esi, imm8         83 FE <1> */
    BB_INSN_MOV_RCX_IMM64,    /* mov rcx, imm64        48 B9 <8> */
    BB_INSN_MOV_EAX_RCXMEM,   /* mov eax, [rcx]        8B 01 */
    BB_INSN_MOV_RAX_RCXMEM,   /* mov rax, [rcx]        48 8B 01 */
    BB_INSN_CMP_EAX_RCXMEM,   /* cmp eax, [rcx]        3B 01 */
    BB_INSN_MOV_EAX_R10MEM,   /* mov eax, [r10]        41 8B 02 */
    BB_INSN_MOV_R10MEM_EAX,   /* mov [r10], eax        41 89 02 */
    BB_INSN_MOV_ECX_EAX,      /* mov ecx, eax          89 C1 */
    BB_INSN_MOV_RDI_RAX,      /* mov rdi, rax          48 89 C7 */
    BB_INSN_MOV_RDX_RAX,      /* mov rdx, rax          48 89 C2 */
    BB_INSN_MOVSXD_RCX_R10MEM,/* movsxd rcx,[r10]      49 63 0A */
    BB_INSN_LEA_RAX_RAXRCX,   /* lea rax,[rax+rcx]     48 8D 04 08 */
    BB_INSN_CMP_EAX_ECX,      /* cmp eax, ecx          39 C8 */
    BB_INSN_TEST_EAX_EAX,     /* test eax, eax         85 C0 */
    BB_INSN_TEST_RAX_RAX,     /* test rax, rax         48 85 C0 */
    BB_INSN_XOR_EDX_EDX,      /* xor edx, edx          31 D2 */
    BB_INSN_RET,               /* ret                   C3 */
    BB_INSN_CALL_RAX,          /* call rax              FF D0 */
    /* ── symbolic kinds (EM-7c-symbolic) ──────────────────────────────────── */
    /* TEXT:   lea rcx, [rip + sym]   (RIP-relative GOT/data reference)      */
    /* BINARY: mov rcx, a0            (imm64 process-address fallback)        */
    BB_INSN_LEA_RCX_SYM,
    /* TEXT:   lea r10, [rip + sym]   (RIP-relative, used for &Δ at entry)   */
    /* BINARY: mov r10, a0            (imm64 process-address fallback)        */
    BB_INSN_LEA_R10_SYM,
    /* TEXT:   call sym@PLT           (PLT-indirect function call)            */
    /* BINARY: mov rax, a0; call rax  (imm64 + indirect-call fallback)        */
    BB_INSN_CALL_SYM_PLT,
} bb_insn_kind_t;

typedef struct {
    bb_insn_kind_t kind;
    uint64_t       a0;   /* imm64 / address (BINARY fallback for SYM kinds) */
    uint32_t       a1;   /* imm32 */
    uint8_t        a2;   /* imm8 */
    const char    *sym;  /* symbol name for BB_INSN_LEA_RCX_SYM / BB_INSN_CALL_SYM_PLT */
} bb_insn_desc_t;

/* ── vtable ───────────────────────────────────────────────────────────────── */

typedef struct emitter_t emitter_t;

struct emitter_t {
    /*
     * emit_insn — emit one instruction described by d.
     *   TEXT:   writes a single readable GAS line.
     *   BINARY: writes the corresponding x86-64 bytes.
     */
    void (*emit_insn)(emitter_t *e, const bb_insn_desc_t *d);

    /*
     * label_define — plant a label at the current position.
     *   TEXT:   emits "name:\n".
     *   BINARY: records offset; patches forward refs.
     */
    void (*label_define)(emitter_t *e, bb_label_t *lbl);

    /*
     * emit_jmp — emit a jump to target of the given kind.
     *   TEXT:   emits "    jmp/je/... name\n".
     *   BINARY: opcode + forward-ref patch.
     */
    void (*emit_jmp)(emitter_t *e, bb_label_t *target, jmp_kind_t kind);

    /*
     * global_sym — emit a .global directive.
     *   TEXT:   emits ".global name\n".
     *   BINARY: no-op.
     */
    void (*global_sym)(emitter_t *e, const char *name);

    /*
     * fprintf_raw — arbitrary formatted text (TEXT only, no-op in binary).
     */
    void (*fprintf_raw)(emitter_t *e, const char *fmt, ...);

    /*
     * pos — current emission cursor in bytes.
     */
    int (*pos)(emitter_t *e);

    /*
     * intern_str — register a literal string and return its asm label.
     *   TEXT:   adds the string to the .rodata strtab; returns ".Lstr_N".
     *   BINARY: no-op; returns NULL (caller uses raw pointer for in-process).
     * May be NULL if the emitter does not support string interning.
     */
    const char *(*intern_str)(emitter_t *e, const char *s);

    /* is_text — 1 if this is a TEXT emitter, 0 if BINARY.
     * Callers that need to know the mode at the call site use this flag
     * rather than comparing function pointers. */
    int is_text;

    void *ctx;
};

/* ── constructors / lifecycle ─────────────────────────────────────────────── */

emitter_t *emitter_text_new  (FILE *out);
emitter_t *emitter_binary_new(bb_buf_t buf, int size);
void       emitter_free      (emitter_t *e);
int        emitter_end       (emitter_t *e);

/* EM-FORMAT-BB lone-label fusion (2026-05-09):
 * Returns the FILE* that a TEXT-mode emitter writes to (for callers
 * in bb_flat.c that need to route through bb3c_format directly).
 * Returns NULL for non-text (binary) emitters. */
FILE *emitter_text_file(emitter_t *e);

/* ── convenience macros ───────────────────────────────────────────────────── */

#define EV_LABEL(e, lbl)      (e)->label_define((e), (lbl))
#define EV_JMP(e, lbl, kind)  (e)->emit_jmp((e), (lbl), (kind))
#define EV_GLOBAL(e, name)    (e)->global_sym((e), (name))
#define EV_TEXT(e, ...)       (e)->fprintf_raw((e), __VA_ARGS__)

/* ── EM-7c-bb-three-column: shared formatters ───────────────────────────────
 *
 *   LABEL:                   ; ACTION           ; GOTO
 *   col 1 (24 wide)          ; col 2 (16 wide)  ; col 3 (free)
 *
 * For TEXT-mode emitters; no-op (well, still routes through fprintf_raw)
 * when called from BB-mode contexts.  The GAS `;` is a statement
 * separator on x86 — empty fields parse as empty statements (legal).
 */

/* EM-7c-s-file-beautify (2026-05-09): removed the literal `;` separators
 * that the prior PARTIAL rung introduced.  Shape matches SM-side
 * `emit_three_column_line` — one printf format shared across the
 * entire `.s` file. */
static inline void ev3c(emitter_t *e, const char *lbl, const char *act, const char *got)
{
    /* EM-7c-no-trailing-ws (2026-05-09): build + right-trim before write. */
    char line[768];
    int n = snprintf(line, sizeof(line), "%-24s%-16s %s",
                     lbl ? lbl : "", act ? act : "", got ? got : "");
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) n--;
    line[n] = '\0';
    e->fprintf_raw(e, "%s\n", line);
}

/* Action-only line (cols 1+3 empty). */
static inline void ev3c_action_v(emitter_t *e, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ev3c(e, "", buf, "");
}

/* Label-only line (cols 2+3 empty); appends `:` to the name. */
static inline void ev3c_label(emitter_t *e, const char *name)
{
    char buf[256]; snprintf(buf, sizeof(buf), "%s:", name);
    ev3c(e, buf, "", "");
}

/* Goto-only line (cols 1+2 empty). */
static inline void ev3c_goto_v(emitter_t *e, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ev3c(e, "", "", buf);
}

/* ── inline named helpers — the only API bb_flat.c uses ──────────────────── */
/*
 * Each helper builds a bb_insn_desc_t and calls e->emit_insn.
 * bb_flat.c (and future walkers) call these by name, never by byte.
 */

static inline void em_insn0(emitter_t *e, bb_insn_kind_t k)
{ bb_insn_desc_t d = {k,0,0,0,NULL}; e->emit_insn(e,&d); }

static inline void em_insn_a0(emitter_t *e, bb_insn_kind_t k, uint64_t a0)
{ bb_insn_desc_t d = {k,a0,0,0,NULL}; e->emit_insn(e,&d); }

static inline void em_insn_a1(emitter_t *e, bb_insn_kind_t k, uint32_t a1)
{ bb_insn_desc_t d = {k,0,a1,0,NULL}; e->emit_insn(e,&d); }

static inline void em_insn_a2(emitter_t *e, bb_insn_kind_t k, uint8_t a2)
{ bb_insn_desc_t d = {k,0,0,a2,NULL}; e->emit_insn(e,&d); }


/* ── EM-7c-symbolic: symbolic load/call helpers ───────────────────────────── */
/*
 * em_lea_rcx_sym — load address of a named symbol into rcx.
 *   TEXT:   lea rcx, [rip + sym]
 *   BINARY: mov rcx, imm64  (imm64 = process address for in-process JIT)
 */
static inline void em_lea_rcx_sym(emitter_t *e, const char *sym, uint64_t addr_fallback)
{
    bb_insn_desc_t d = {BB_INSN_LEA_RCX_SYM, addr_fallback, 0, 0, sym};
    e->emit_insn(e, &d);
}

/*
 * em_call_sym_plt — call a named function via PLT.
 *   TEXT:   call sym@PLT
 *   BINARY: mov rax, imm64; call rax  (imm64 = function pointer for in-process JIT)
 */
static inline void em_call_sym_plt(emitter_t *e, const char *sym, uint64_t fn_fallback)
{
    bb_insn_desc_t d = {BB_INSN_CALL_SYM_PLT, fn_fallback, 0, 0, sym};
    e->emit_insn(e, &d);
}

/* r10 = &Δ */
static inline void em_load_r10_delta_ptr(emitter_t *e, uint64_t addr)
{ em_insn_a0(e, BB_INSN_MOV_R10_IMM64, addr); }

/* eax = Δ  (via [r10]) */
static inline void em_load_delta(emitter_t *e)
{ em_insn0(e, BB_INSN_MOV_EAX_R10MEM); }

/* Δ = eax  (via [r10]) */
static inline void em_store_delta(emitter_t *e)
{ em_insn0(e, BB_INSN_MOV_R10MEM_EAX); }

/* rcx = imm64; rax = [rcx]  (load Σ ptr) */
static inline void em_load_sigma(emitter_t *e, uint64_t sigma_addr) {
    em_lea_rcx_sym(e, "\xCE\xA3", sigma_addr);      /* lea/mov rcx, &Σ */
    em_insn0      (e, BB_INSN_MOV_RAX_RCXMEM);      /* rax = [rcx] = Σ */
}

/* rcx = imm64; eax = [rcx]  (load Σlen) */
static inline void em_load_siglen(emitter_t *e, uint64_t siglen_addr) {
    em_lea_rcx_sym(e, "\xCE\xA3""len", siglen_addr); /* lea/mov rcx, &Σlen */
    em_insn0      (e, BB_INSN_MOV_EAX_RCXMEM);       /* eax = [rcx] = Σlen */
}

/* rax = Σ+Δ  (Σ ptr + cursor) */
static inline void em_sigma_plus_delta(emitter_t *e,
                                       uint64_t sigma_addr)
{
    em_load_sigma(e, sigma_addr);               /* rax = Σ */
    em_insn0(e, BB_INSN_MOVSXD_RCX_R10MEM);    /* rcx = (int64)Δ */
    em_insn0(e, BB_INSN_LEA_RAX_RAXRCX);       /* rax = rax+rcx */
}

/* cmp eax, [rcx] where rcx=siglen_addr */
static inline void em_cmp_eax_siglen(emitter_t *e, uint64_t siglen_addr) {
    em_lea_rcx_sym(e, "\xCE\xA3""len", siglen_addr); /* lea/mov rcx, &Σlen */
    em_insn0      (e, BB_INSN_CMP_EAX_RCXMEM);
}
static inline void em_add_delta_imm(emitter_t *e, int32_t v) {
    em_load_delta(e);
    em_insn_a1(e, BB_INSN_ADD_EAX_IMM32, (uint32_t)v);
    em_store_delta(e);
}

/* eax = Δ - v; store back */
static inline void em_sub_delta_imm(emitter_t *e, int32_t v) {
    em_load_delta(e);
    em_insn_a1(e, BB_INSN_SUB_EAX_IMM32, (uint32_t)v);
    em_store_delta(e);
}

/* eax += imm32 */
static inline void em_add_eax_imm32(emitter_t *e, uint32_t v)
{ em_insn_a1(e, BB_INSN_ADD_EAX_IMM32, v); }

static inline void em_mov_rax_imm64(emitter_t *e, uint64_t v)
{ em_insn_a0(e, BB_INSN_MOV_RAX_IMM64, v); }

static inline void em_mov_rdi_imm64(emitter_t *e, uint64_t v)
{ em_insn_a0(e, BB_INSN_MOV_RDI_IMM64, v); }

static inline void em_mov_rdx_imm64(emitter_t *e, uint64_t v)
{ em_insn_a0(e, BB_INSN_MOV_RDX_IMM64, v); }

static inline void em_mov_esi_imm32(emitter_t *e, uint32_t v)
{ em_insn_a1(e, BB_INSN_MOV_ESI_IMM32, v); }

static inline void em_mov_eax_imm32(emitter_t *e, uint32_t v)
{ em_insn_a1(e, BB_INSN_MOV_EAX_IMM32, v); }

static inline void em_cmp_eax_imm32(emitter_t *e, uint32_t v)
{ em_insn_a1(e, BB_INSN_CMP_EAX_IMM32, v); }

static inline void em_sub_eax_imm32(emitter_t *e, uint32_t v)
{ em_insn_a1(e, BB_INSN_SUB_EAX_IMM32, v); }

static inline void em_cmp_esi_imm8(emitter_t *e, uint8_t v)
{ em_insn_a2(e, BB_INSN_CMP_ESI_IMM8, v); }

static inline void em_mov_ecx_eax(emitter_t *e)  { em_insn0(e, BB_INSN_MOV_ECX_EAX); }
static inline void em_mov_rdi_rax(emitter_t *e)  { em_insn0(e, BB_INSN_MOV_RDI_RAX); }
static inline void em_mov_rdx_rax(emitter_t *e)  { em_insn0(e, BB_INSN_MOV_RDX_RAX); }
static inline void em_cmp_eax_ecx(emitter_t *e)  { em_insn0(e, BB_INSN_CMP_EAX_ECX); }
static inline void em_test_eax_eax(emitter_t *e) { em_insn0(e, BB_INSN_TEST_EAX_EAX); }
static inline void em_test_rax_rax(emitter_t *e) { em_insn0(e, BB_INSN_TEST_RAX_RAX); }
static inline void em_xor_edx_edx(emitter_t *e)  { em_insn0(e, BB_INSN_XOR_EDX_EDX); }
static inline void em_ret(emitter_t *e)           { em_insn0(e, BB_INSN_RET); }
static inline void em_call_rax(emitter_t *e)      { em_insn0(e, BB_INSN_CALL_RAX); }


#endif /* EMITTER_H */
