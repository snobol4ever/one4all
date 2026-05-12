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
 *   calls named inline helpers (emit_load_delta, emit_mov_rax_imm64, ...)
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

/* ── EM-MODE4-IS-MODE3-DUMP-b ────────────────────────────────────────────── */
/*
 * Text-emitter mode (sess 2026-05-11 sub-rung -b).
 *
 * The text backend produces two distinct outputs from the same template
 * walk:
 *
 *   TEXT_MODE_INVOCATION — emit a single macro invocation line per call
 *                          site, e.g. "PUSH_INT 42".  This is what
 *                          mode-4's per-call-site `.s` emission uses.
 *
 *   TEXT_MODE_DEFINITION — emit the full body of the macro the template
 *                          describes, e.g. ".macro PUSH_INT v / mov rdi,
 *                          \\v / call rt_push_int@PLT / .endm".  This is
 *                          what `sm_macros.s` regeneration uses.
 *
 * One boolean flag on the text backend, not a fourth backend.
 * Defaults to TEXT_MODE_INVOCATION for backward compatibility.
 */
typedef enum {
    TEXT_MODE_INVOCATION = 0,    /* macro invocation per call site (default) */
    TEXT_MODE_DEFINITION = 1,    /* full macro body for sm_macros.s regen */
} emitter_text_mode_t;

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
    /* ── EM-MODE4-IS-MODE3-DUMP-c: SM-State-field manipulation ──────────── */
    /* inc dword [r13 + disp8]  — pc++ / sp++ / last_ok++ etc.
     * a2 carries the disp8.  Used by SM_HALT (pc bump via [r13+20]) and
     * future SM opcodes that touch SM_State fields directly. */
    BB_INSN_INC_MEM_R13_DISP8,
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
    /* push r10               41 52      — save flat-BB LOCAL across runtime call */
    BB_INSN_PUSH_R10,
    /* pop r10                41 5A      — restore flat-BB LOCAL after runtime call */
    BB_INSN_POP_R10,
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

    /*
     * ── EM-MODE4-IS-MODE3-DUMP-b surface extensions ──────────────────────
     *
     * The methods below are the NEW surface added for SM-side templates.
     * Per the design doc (MIGRATION-MODE4-IS-MODE3-DUMP.md §"The vtable
     * surface" and §"Three-call-site discipline"): templates call any of
     * these freely; each backend implements or no-ops per its semantics.
     *
     * Sub-rung -b adds the surface only.  No template calls them yet;
     * mode-3 and mode-4 still use their pre-existing emission paths.
     * Sub-rung -c (SM_HALT, the first template) will be the first
     * caller.
     *
     * All pointers may be NULL initially in a backend; callers MUST
     * tolerate NULL via the EMIT_OPT(...) helper macros below.
     */

    /* — structural markers — */

    /* label_name — plant a named label at the current position.
     *   TEXT:   emits "name:\n".
     *   BINARY: records (name, offset) for later resolution.
     * Simpler convenience over label_define which takes bb_label_t*. */
    void (*label_name)     (emitter_t *e, const char *name);

    /* pc_label — plant a ".L<pc>:" label.  Used by SM templates that
     *   address SM_Program pc directly. */
    void (*pc_label)       (emitter_t *e, int pc);

    /* section — switch to a named GAS section.
     *   TEXT:   emits ".section name\n" (or ".text", ".data", ".rodata"
     *           when name is one of those canonical short forms).
     *   BINARY: no-op (in-process JIT has one code region). */
    void (*section)        (emitter_t *e, const char *name);

    /* directive — arbitrary GAS directive line (single line of text).
     *   TEXT:   emits "    <line>\n" (no column shaping; this is for
     *           directives like ".align 16" / ".size foo, .-foo").
     *   BINARY: no-op. */
    void (*directive)      (emitter_t *e, const char *line);

    /* data_quad — emit ".quad <imm64>".
     *   TEXT:   emits ".quad 0x...\n" (in current section).
     *   BINARY: writes 8 bytes at current position. */
    void (*data_quad)      (emitter_t *e, uint64_t val);

    /* data_quad_sym — emit ".quad <symbol>".
     *   TEXT:   emits ".quad <sym>\n".
     *   BINARY: in-process JIT writes the resolved address (zero stub
     *           if unresolved; caller is expected to use this only in
     *           text-side aux sections, not SEG_CODE). */
    void (*data_quad_sym)  (emitter_t *e, const char *sym);

    /* data_string — emit a literal byte string (no null terminator
     *   added; use the explicit length for control).
     *   TEXT:   emits ".ascii \"...\"\n" with escaping.
     *   BINARY: writes the raw bytes at current position. */
    void (*data_string)    (emitter_t *e, const char *bytes, size_t len);

    /* data_long — emit a 32-bit integer word in the current section.
     *   TEXT:   emits ".long <val>\n".
     *   BINARY: writes 4 bytes (little-endian) at current position. */
    void (*data_long)      (emitter_t *e, int32_t val);

    /* bb_zeta_rdi — load the address of a per-box zeta struct into rdi.
     *   BINARY: emit_mov_rdi_imm64(e, ptr) — bake the in-process ptr.
     *   TEXT:   emit lea rdi, [rip + sym] — RIP-relative static ref.
     * Use instead of emit_mov_rdi_imm64 in templates that manage a zeta
     * in both binary (heap alloc) and text (.data section) modes. */
    void (*bb_zeta_rdi)    (emitter_t *e, uint64_t ptr, const char *sym);

    /* bb_dispatch_jne_jmp — after a box runtime call whose return value
     * is in rax (non-zero = success), emit the success/fail dispatch.
     *   BINARY: emit_test_rax_rax + JNE lbl_succ + JMP lbl_fail (3 insns).
     *   TEXT:   emit fused three-column "test rax,rax; jne x; jmp y" line. */
    void (*bb_dispatch_jne_jmp)(emitter_t *e,
                                bb_label_t *lbl_succ, bb_label_t *lbl_fail);

    /* pad_to_blob_size — pad current emission to the canonical blob size.
     *   TEXT:   no-op (text size has no fixed-blob constraint).
     *   BINARY: writes 0x90 (NOP) bytes until alignment is reached. */
    void (*pad_to_blob_size)(emitter_t *e);

    /* — BB-port primitives (Greek-port semantic labels α/β/γ/ω) — */

    /* bb_port_label — plant a port label for a BB box.
     *   port ∈ {'a','b','g','o'} (ASCII for α β γ ω in template source).
     *   TEXT:   emits "<box_prefix>_<greek>:\n".
     *   BINARY: records offset; patches forward refs. */
    void (*bb_port_label)  (emitter_t *e, const char *box_prefix, char port);

    /* bb_port_jmp — emit a jump to a port label on a BB box.
     *   TEXT:   emits "    jmp <box_prefix>_<greek>\n".
     *   BINARY: opcode + forward-ref patch. */
    void (*bb_port_jmp)    (emitter_t *e, const char *box_prefix, char port);

    /* bb_box_banner — emit a "# BOX <kind>(<args>) [<prefix>]" banner.
     *   TEXT:   emits 120-char #---- rule + the banner line.
     *   BINARY: no-op. */
    void (*bb_box_banner)  (emitter_t *e, const char *kind, const char *args);

    /* — formatting / readability — */

    /* comment — emit a one-line "# ..." comment.
     *   TEXT:   emits "# <text>\n".
     *   BINARY: no-op. */
    void (*comment)        (emitter_t *e, const char *text);

    /* banner — emit a major "stmt N (line L): ..." style stmt banner.
     *   TEXT:   emits a 120-char #==== rule + the text + another rule.
     *   BINARY: no-op. */
    void (*banner)         (emitter_t *e, const char *text);

    /* minor_break — emit a minor section break within a stmt.
     *   TEXT:   emits a 120-char #---- rule + the text.
     *   BINARY: no-op. */
    void (*minor_break)    (emitter_t *e, const char *text);

    /* blank_line — emit a single blank line.
     *   TEXT:   emits "\n".
     *   BINARY: no-op. */
    void (*blank_line)     (emitter_t *e);

    /* — macro_def-only hooks (binary + text invocation-mode no-op) — */

    /* macro_begin — open a .macro NAME params block.
     *   macro_def:           emits ".macro NAME p1 p2 ...\n".
     *   text-invocation:     emits "NAME p1 p2 ..." invocation line and
     *                        sets a flag so subsequent body emissions are
     *                        suppressed until macro_end.
     *   binary:              no-op.
     */
    void (*macro_begin)    (emitter_t *e, const char *name,
                            const char *const *params, int nparams);

    /* macro_param_ref — emit a "\param" reference inside a macro body.
     *   macro_def:           emits "\<param>".
     *   text-invocation:     no-op (body suppressed).
     *   binary:              no-op.
     */
    void (*macro_param_ref)(emitter_t *e, const char *name);

    /* macro_end — close a .macro block.
     *   macro_def:           emits ".endm\n".
     *   text-invocation:     clears the body-suppression flag.
     *   binary:              no-op.
     */
    void (*macro_end)      (emitter_t *e);

    /* — backend identity / mode — */

    /* text_mode — for text backends, which mode (INVOCATION/DEFINITION).
     *   Ignored by binary backend. */
    emitter_text_mode_t text_mode;

    /* is_text — 1 if this is a TEXT-family emitter (text or macro_def),
     * 0 if BINARY.  Callers that need to know the mode at the call site
     * use this flag rather than comparing function pointers. */
    int is_text;

    void *ctx;
};

/* ── constructors / lifecycle ─────────────────────────────────────────────── */

emitter_t *emitter_text_new  (FILE *out);
emitter_t *emitter_binary_new(bb_buf_t buf, int size);
void       emitter_free      (emitter_t *e);
int        emitter_end       (emitter_t *e);

/* EM-MODE4-IS-MODE3-DUMP-b: text emitter with explicit mode selection.
 *   emitter_text_new(out) — keeps prior behaviour; uses INVOCATION mode.
 *   emitter_text_new_mode(out, mode) — explicit INVOCATION or DEFINITION. */
emitter_t *emitter_text_new_mode(FILE *out, emitter_text_mode_t mode);

/* EM-MODE4-IS-MODE3-DUMP-b: macro-definition backend.  Thin wrapper that
 * constructs a text emitter in DEFINITION mode, suitable for regenerating
 * sm_macros.s from per-opcode templates.  See emitter_macro_def.c. */
emitter_t *emitter_macro_def_new(FILE *out);

/* EM-MODE4-IS-MODE3-DUMP-b: NULL-safe optional-method invoker.
 *
 * The EM-MODE4-IS-MODE3-DUMP-b surface is intentionally permissive — a
 * template may call any vtable method; each backend either implements
 * or no-ops the call.  During the staged retrofit, a backend may
 * temporarily leave a pointer NULL.  Use EMIT_OPT to invoke a method
 * only if non-NULL.
 *
 * Usage:
 *   EMIT_OPT(e, comment,  e, "hello");
 *   EMIT_OPT(e, data_quad, e, 0x1234);
 *
 * For methods that always have a definition in every backend (e.g.
 * emit_insn, label_define), invoke directly: e->emit_insn(e, &d). */
#define EMIT_OPT(e, method, ...) \
    do { if ((e) && (e)->method) (e)->method(__VA_ARGS__); } while (0)

/* EM-FORMAT-BB lone-label fusion (2026-05-09):
 * Returns the FILE* that a TEXT-mode emitter writes to (for callers
 * in bb_flat.c that need to route through bb3c_format directly).
 * Returns NULL for non-text (binary) emitters. */
FILE *emitter_text_file(emitter_t *e);

/* ── convenience macros ───────────────────────────────────────────────────── */

#define EV_LABEL(e, lbl)      (e)->label_define((e), (lbl))
#define EV_JMP(e, lbl, kind)  (e)->emit_jmp((e), (lbl), (kind))
/* Preferred names for template code (EV_* kept for non-template callers). */
#define EMIT_LABEL(e, lbl)    (e)->label_define((e), (lbl))
#define EMIT_JMP(e, lbl, kind) (e)->emit_jmp((e), (lbl), (kind))
#define EV_GLOBAL(e, name)    (e)->global_sym((e), (name))
#define EV_TEXT(e, ...)       (e)->fprintf_raw((e), __VA_ARGS__)

/* ── EM-7c-bb-three-column: shared formatters ───────────────────────────────
 *
 *   LABEL:                   ; ACTION           ; GOTO
 *   col 1 (24 wide)          ; col 2 (16 wide)  ; col 3 (free)
 *
 * The `ev3c*` family that previously lived here (one base function + three
 * convenience wrappers) was DELETED in the _v survivors purge (sess
 * 2026-05-11 final pass), per Lon's fourth-pass directive "Scrap the use
 * of the character V or v for this concept here. ... Eradicate it from
 * docs and source. I do not want to see it ever again."  The family
 * carried the `ev` prefix (emitter-vtable, banned) and two of the four
 * also carried the `_v` suffix (banned).  Static-analysis at handoff
 * confirmed zero external callers — pure dead code — so eradication
 * via deletion is cleaner than rename.
 *
 * The bb_flat.c three-column emissions use `bb3c_format` (in bb_emit.c)
 * directly, with no detour through the deleted family.  If a future
 * emitter wants generic three-column formatting it should add named
 * helpers (`emit3c_label`, etc.) to this header at that time.
 */

/* ── inline named helpers — the only API bb_flat.c uses ──────────────────── */
/*
 * Each helper builds a bb_insn_desc_t and calls e->emit_insn.
 * bb_flat.c (and future walkers) call these by name, never by byte.
 */

static inline void emit_insn0(emitter_t *e, bb_insn_kind_t k)
{ bb_insn_desc_t d = {k,0,0,0,NULL}; e->emit_insn(e,&d); }

static inline void emit_insn_a0(emitter_t *e, bb_insn_kind_t k, uint64_t a0)
{ bb_insn_desc_t d = {k,a0,0,0,NULL}; e->emit_insn(e,&d); }

static inline void emit_insn_a1(emitter_t *e, bb_insn_kind_t k, uint32_t a1)
{ bb_insn_desc_t d = {k,0,a1,0,NULL}; e->emit_insn(e,&d); }

static inline void emit_insn_a2(emitter_t *e, bb_insn_kind_t k, uint8_t a2)
{ bb_insn_desc_t d = {k,0,0,a2,NULL}; e->emit_insn(e,&d); }


/* ── EM-7c-symbolic: symbolic load/call helpers ───────────────────────────── */
/*
 * emit_lea_rcx_sym — load address of a named symbol into rcx.
 *   TEXT:   lea rcx, [rip + sym]
 *   BINARY: mov rcx, imm64  (imm64 = process address for in-process JIT)
 */
static inline void emit_lea_rcx_sym(emitter_t *e, const char *sym, uint64_t addr_fallback)
{
    bb_insn_desc_t d = {BB_INSN_LEA_RCX_SYM, addr_fallback, 0, 0, sym};
    e->emit_insn(e, &d);
}

/*
 * emit_call_sym_plt — call a named function via PLT.
 *   TEXT:   call sym@PLT
 *   BINARY: mov rax, imm64; call rax  (imm64 = function pointer for in-process JIT)
 */
static inline void emit_call_sym_plt(emitter_t *e, const char *sym, uint64_t fn_fallback)
{
    bb_insn_desc_t d = {BB_INSN_CALL_SYM_PLT, fn_fallback, 0, 0, sym};
    e->emit_insn(e, &d);
}

static inline void emit_push_r10(emitter_t *e)
{ emit_insn0(e, BB_INSN_PUSH_R10); }

static inline void emit_pop_r10(emitter_t *e)
{ emit_insn0(e, BB_INSN_POP_R10); }

/* r10 = &Δ */
static inline void emit_load_r10_delta_ptr(emitter_t *e, uint64_t addr)
{ emit_insn_a0(e, BB_INSN_MOV_R10_IMM64, addr); }

/* eax = Δ  (via [r10]) */
static inline void emit_load_delta(emitter_t *e)
{ emit_insn0(e, BB_INSN_MOV_EAX_R10MEM); }

/* Δ = eax  (via [r10]) */
static inline void emit_store_delta(emitter_t *e)
{ emit_insn0(e, BB_INSN_MOV_R10MEM_EAX); }

/* rcx = imm64; rax = [rcx]  (load Σ ptr) */
static inline void emit_load_sigma(emitter_t *e, uint64_t sigma_addr) {
    emit_lea_rcx_sym(e, "\xCE\xA3", sigma_addr);      /* lea/mov rcx, &Σ */
    emit_insn0      (e, BB_INSN_MOV_RAX_RCXMEM);      /* rax = [rcx] = Σ */
}

/* rcx = imm64; eax = [rcx]  (load Σlen) */
static inline void emit_load_siglen(emitter_t *e, uint64_t siglen_addr) {
    emit_lea_rcx_sym(e, "\xCE\xA3""len", siglen_addr); /* lea/mov rcx, &Σlen */
    emit_insn0      (e, BB_INSN_MOV_EAX_RCXMEM);       /* eax = [rcx] = Σlen */
}

/* rax = Σ+Δ  (Σ ptr + cursor) */
static inline void emit_sigma_plus_delta(emitter_t *e,
                                       uint64_t sigma_addr)
{
    emit_load_sigma(e, sigma_addr);               /* rax = Σ */
    emit_insn0(e, BB_INSN_MOVSXD_RCX_R10MEM);    /* rcx = (int64)Δ */
    emit_insn0(e, BB_INSN_LEA_RAX_RAXRCX);       /* rax = rax+rcx */
}

/* cmp eax, [rcx] where rcx=siglen_addr */
static inline void emit_cmp_eax_siglen(emitter_t *e, uint64_t siglen_addr) {
    emit_lea_rcx_sym(e, "\xCE\xA3""len", siglen_addr); /* lea/mov rcx, &Σlen */
    emit_insn0      (e, BB_INSN_CMP_EAX_RCXMEM);
}
static inline void emit_add_delta_imm(emitter_t *e, int32_t v) {
    emit_load_delta(e);
    emit_insn_a1(e, BB_INSN_ADD_EAX_IMM32, (uint32_t)v);
    emit_store_delta(e);
}

/* eax = Δ - v; store back */
static inline void emit_sub_delta_imm(emitter_t *e, int32_t v) {
    emit_load_delta(e);
    emit_insn_a1(e, BB_INSN_SUB_EAX_IMM32, (uint32_t)v);
    emit_store_delta(e);
}

/* eax += imm32 */
static inline void emit_add_eax_imm32(emitter_t *e, uint32_t v)
{ emit_insn_a1(e, BB_INSN_ADD_EAX_IMM32, v); }

static inline void emit_mov_rax_imm64(emitter_t *e, uint64_t v)
{ emit_insn_a0(e, BB_INSN_MOV_RAX_IMM64, v); }

static inline void emit_mov_rdi_imm64(emitter_t *e, uint64_t v)
{ emit_insn_a0(e, BB_INSN_MOV_RDI_IMM64, v); }

static inline void emit_mov_rdx_imm64(emitter_t *e, uint64_t v)
{ emit_insn_a0(e, BB_INSN_MOV_RDX_IMM64, v); }

static inline void emit_mov_esi_imm32(emitter_t *e, uint32_t v)
{ emit_insn_a1(e, BB_INSN_MOV_ESI_IMM32, v); }

static inline void emit_mov_eax_imm32(emitter_t *e, uint32_t v)
{ emit_insn_a1(e, BB_INSN_MOV_EAX_IMM32, v); }

static inline void emit_cmp_eax_imm32(emitter_t *e, uint32_t v)
{ emit_insn_a1(e, BB_INSN_CMP_EAX_IMM32, v); }

static inline void emit_sub_eax_imm32(emitter_t *e, uint32_t v)
{ emit_insn_a1(e, BB_INSN_SUB_EAX_IMM32, v); }

static inline void emit_cmp_esi_imm8(emitter_t *e, uint8_t v)
{ emit_insn_a2(e, BB_INSN_CMP_ESI_IMM8, v); }

static inline void emit_mov_ecx_eax(emitter_t *e)  { emit_insn0(e, BB_INSN_MOV_ECX_EAX); }
static inline void emit_mov_rdi_rax(emitter_t *e)  { emit_insn0(e, BB_INSN_MOV_RDI_RAX); }
static inline void emit_mov_rdx_rax(emitter_t *e)  { emit_insn0(e, BB_INSN_MOV_RDX_RAX); }
static inline void emit_cmp_eax_ecx(emitter_t *e)  { emit_insn0(e, BB_INSN_CMP_EAX_ECX); }
static inline void emit_test_eax_eax(emitter_t *e) { emit_insn0(e, BB_INSN_TEST_EAX_EAX); }
static inline void emit_test_rax_rax(emitter_t *e) { emit_insn0(e, BB_INSN_TEST_RAX_RAX); }
static inline void emit_xor_edx_edx(emitter_t *e)  { emit_insn0(e, BB_INSN_XOR_EDX_EDX); }
static inline void emit_ret(emitter_t *e)           { emit_insn0(e, BB_INSN_RET); }
static inline void emit_call_rax(emitter_t *e)      { emit_insn0(e, BB_INSN_CALL_RAX); }

/* EM-MODE4-IS-MODE3-DUMP-c: increment an SM_State field by 1.
 *   inc dword [r13 + disp8]   — 4 bytes: 41 ff 45 <disp8>
 * Used by SM_HALT (pc bump via [r13+20]) and future SM opcodes that
 * touch SM_State integer fields. */
static inline void emit_inc_mem_r13_disp8(emitter_t *e, uint8_t disp)
{ emit_insn_a2(e, BB_INSN_INC_MEM_R13_DISP8, disp); }


#endif /* EMITTER_H */
