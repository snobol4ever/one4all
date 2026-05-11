/*
 * templates/bb_xchr.c — XCHR (literal-string-match) BB box template.
 *
 * The first BB-box template in the EM-MODE4-IS-MODE3-DUMP retrofit
 * (sub-rung -d, sess 2026-05-11).  ONE function describes the XCHR
 * box's emission; the `emitter_t` vtable walks it to produce either
 * native bytes (binary backend → mode-3 in-process pattern blob) or
 * GAS asm text (text backend → mode-4 `.s` output).
 *
 * UNLIKE SM_HALT (templates/sm_halt.c), this template has NO known
 * mode-3-vs-mode-4 architectural divergence.  Both modes call into
 * the same runtime symbols (`memcmp`, `bb_label_*`, etc.) and both
 * read/write the same global subject-string anchors (`Σ`, `Σlen`,
 * `Δ`).  The text and binary productions differ only in how the
 * intermediate bytes are addressed (RIP-relative symbol vs imm64
 * raw pointer) — which the emitter vtable already absorbs via
 * `BB_INSN_LEA_RCX_SYM` and `BB_INSN_CALL_SYM_PLT`.
 *
 * This template is lifted byte-for-byte from `flat_emit_lit` (and
 * the surrounding `case XCHR:` branch in `flat_emit_node`) in
 * `bb_flat.c`.  The caller in `bb_flat.c` now dispatches to
 * `emit_bb_xchr` directly.  Sub-rung -d's success criterion is that
 * the BB-box vtable machinery exercises a template end-to-end with
 * zero behavioral change.
 *
 * SUB-RUNG -d SCOPE:
 *   - Define `emit_bb_xchr(emitter_t *e, PATND_t *p, ...)`.
 *   - Cut the body out of `flat_emit_lit` and the XCHR branch of
 *     `flat_emit_node`.
 *   - The caller (`flat_emit_node`'s XCHR case) invokes this template.
 *   - Preserve byte-for-byte output (text and binary) on the 5
 *     tracked artifacts (roman.s, wordcount.s, claws5.s,
 *     treebank-list.s, treebank-array.s).
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-d / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_flat.h"     /* flat_emit_box_banner (exposed in sub-rung -d) */
#include "../bb_box.h"      /* extern const char *Σ; extern int Σlen; */
#include "../snobol4_patnd.h"  /* PATND_t, XCHR */
#include <string.h>           /* strlen, memcmp, snprintf */
#include <stdio.h>            /* snprintf */
#include <stdint.h>           /* uint64_t, uintptr_t */

/* Anchor addresses for &Σ / &Σlen, same forms used throughout
 * bb_flat.c (mirrors ADDR_SIGMA / ADDR_SIGLEN macros defined there).
 * Templates are independent translation units so the macro form
 * doesn't carry across — these inline equivalents are emitted at
 * the call sites where the vtable helpers want a uint64_t address. */
#define TEMPLATE_ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define TEMPLATE_ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)

/*
 * emit_bb_xchr — XCHR / literal-string-match box.
 *
 * Body lifted byte-for-byte from `flat_emit_lit` + the `case XCHR:`
 * branch in `flat_emit_node` (bb_flat.c).  Combines the box banner
 * (text-mode only) with the literal-match sequence.
 *
 * Ports:
 *   α (succ) — lbl_succ: jump target on match success.
 *   ω (fail) — lbl_fail: jump target on match failure / EOI.
 *   β (retry) — lbl_β:   re-entry on backtrack; un-advances Δ
 *                        and falls through to ω.
 *
 * Sequence (Σ = subject string, Δ = current cursor):
 *
 *   α: if Δ + len > Σlen           → fail
 *      if memcmp(Σ+Δ, lit, len) ≠ 0 → fail
 *      Δ += len
 *      jmp succ
 *   β: Δ -= len
 *      jmp fail
 */
void emit_bb_xchr(emitter_t *e, PATND_t *p,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                  bb_label_t *lbl_β)
{
    const char *lit = (p && p->STRVAL_fn) ? p->STRVAL_fn : "";
    int len = (int)strlen(lit);

    /* TEXT: emit the "# BOX LIT('foo')" banner above the box code.
     * BINARY: flat_emit_box_banner short-circuits on !e->is_text. */
    if (e->is_text) {
        char preview[40];
        if (len > 24) snprintf(preview, sizeof(preview), "'%.24s...'", lit);
        else          snprintf(preview, sizeof(preview), "'%s'", lit);
        flat_emit_box_banner(e, "LIT", preview, lbl_succ->name);
    }

    /* α: if Δ + len > Σlen → fail */
    emit_load_delta(e);                                       /* eax = Δ */
    emit_add_eax_imm32(e, (uint32_t)len);                     /* eax += len */
    emit_cmp_eax_siglen(e, TEMPLATE_ADDR_SIGLEN);             /* cmp eax, [Σlen] */
    EMIT_JMP(e, lbl_fail, JMP_JG);

    /* memcmp(Σ+Δ, lit, len): set up rdi=Σ+Δ, rsi=lit, rdx=len */
    emit_sigma_plus_delta(e, TEMPLATE_ADDR_SIGMA);            /* rax = Σ+Δ */
    emit_mov_rdi_rax(e);                                      /* rdi = Σ+Δ */
    emit_mov_rdx_imm64(e, (uint64_t)(uint32_t)len);           /* rdx = len */

    /* rsi = lit ptr: TEXT mode → use strtab label; BINARY → raw ptr.
     * Mirrors the original flat_emit_lit branching exactly. */
    if (e->is_text && e->intern_str) {
        const char *lbl = e->intern_str(e, lit);              /* e.g. ".SN" */
        bb_insn_desc_t d = {BB_INSN_LEA_RCX_SYM,
                            (uint64_t)(uintptr_t)lit, 0, 0, lbl};
        e->emit_insn(e, &d);                                  /* lea rcx, [rip + .SN] */
        /* mov rsi, rcx — route through rcx since we have no LEA_RSI_SYM.
         * The bb_insn_desc_t built below is unused; the actual emission
         * goes through fprintf_raw for this single one-off pair (matches
         * legacy flat_emit_lit exactly). */
        e->fprintf_raw(e, "    mov     rsi, rcx\n");
    } else {
        /* BINARY / no-strtab: bake raw pointer (in-process mode-3 valid). */
        bb_insn_desc_t d = {BB_INSN_MOV_RSI_IMM64,
                            (uint64_t)(uintptr_t)lit, 0, 0, NULL};
        e->emit_insn(e, &d);
    }

    /* call memcmp — TEXT: call memcmp@PLT; BINARY: mov rax, ptr; call rax */
    emit_call_sym_plt(e, "memcmp", (uint64_t)(uintptr_t)memcmp);
    emit_test_eax_eax(e);                                     /* test eax, eax */
    EMIT_JMP(e, lbl_fail, JMP_JNE);

    /* success: Δ += len */
    emit_add_delta_imm(e, len);
    EMIT_JMP(e, lbl_succ, JMP_JMP);

    /* β: Δ -= len; fail */
    EMIT_LABEL(e, lbl_β);
    emit_sub_delta_imm(e, len);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
