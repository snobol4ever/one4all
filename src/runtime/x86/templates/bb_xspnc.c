/*
 * templates/bb_xspnc.c — charset-family BB box template.
 *
 * Covers four box kinds that share one runtime ABI:
 *   XSPNC  (SPAN)    → bb_span(zeta, port)
 *   XBRKC  (BREAK)   → bb_brk(zeta, port)
 *   XANYC  (ANY)     → bb_any(zeta, port)
 *   XNNYC  (NOTANY)  → bb_notany(zeta, port)
 *
 * All four compile identically except for the runtime function name
 * and the banner kind-string.  One template function parameterised
 * by (c_fn, c_fn_name, kind_name) covers the whole family.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-e (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * DESIGN (mirrors bb_xchr.c / EM-MODE4-IS-MODE3-DUMP-d):
 *
 *   NO mode-3-vs-mode-4 architectural divergence.  Both modes call
 *   into the same runtime symbols (bb_span/brk/any/notany) and both
 *   read/write the same global subject-string anchors.  The binary-
 *   vs-text difference is fully absorbed by the emitter vtable's
 *   symbolic helpers — the same pattern already proven by XCHR.
 *
 * CALLER RESPONSIBILITIES (enforced by bb_flat.c):
 *
 *   The text path needs access to state in bb_flat.c's static file
 *   scope (g_flat_node_id for unique label names; flat_data_section /
 *   flat_text_section / flat_intel_syntax / flat3c_label /
 *   flat_data_string / flat_data_quad / flat_data_long / flat3c_action /
 *   flat_box_dispatch_jne_jmp).  Rather than expose all that machinery
 *   to the linker, the text path is provided by the per-kind wrapper
 *   functions in bb_flat.c itself via bb_flat_charset_text_body().
 *   This template file owns the binary path and the dispatch logic;
 *   bb_flat.c provides the text-path body.
 *
 *   See emit_bb_charset() below for the full ABI.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-e / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

/* ── emit_bb_charset ─────────────────────────────────────────────────────── */

/*
 * emit_bb_charset — generic charset-family box template.
 *
 * Parameters:
 *   e             — emitter (binary or text backend)
 *   c_fn          — runtime C function pointer (bb_span / bb_brk / etc.)
 *   c_fn_name     — function name string for PLT call
 *   kind_name     — banner display name (SPAN/BREAK/ANY/NOTANY)
 *   chars         — charset string (NUL-terminated)
 *   lbl_succ      — gamma port (succeed) jump target
 *   lbl_fail      — omega port (fail) jump target
 *   lbl_β         — beta port (retry) entry label
 *   text_body_fn  — callback into bb_flat.c that emits the text-path
 *                   body (avoids externalizing bb_flat.c's statics).
 *                   Called only when e->is_text.  NULL = no-op on text.
 *   text_body_arg — opaque arg forwarded to text_body_fn.
 */
void emit_bb_charset(emitter_t *e,
                     bb_box_fn c_fn,
                     const char *c_fn_name,
                     const char *kind_name,
                     const char *chars,
                     bb_label_t *lbl_succ,
                     bb_label_t *lbl_fail,
                     bb_label_t *lbl_β,
                     bb_charset_text_fn text_body_fn,
                     void *text_body_arg)
{
    (void)kind_name;   /* used only by text_body_fn (passed via arg) */
    if (!e) return;

    if (e->is_text) {
        /* Text path: delegate to bb_flat.c which has access to all the
         * static helpers (flat_data_section, flat3c_action, etc.). */
        if (text_body_fn) text_body_fn(e, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }

    /* ── Binary path (mode-3 in-process JIT) ────────────────────────────── */

    /* Heap-allocate a cs_t per invocation (freed by GC). */
    typedef struct { const char *chars; int delta; } cs_t;
    cs_t *z = calloc(1, sizeof(cs_t));
    z->chars = chars;

    /* alpha port: call fn(z, 0) */
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 0);
    emit_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);

    /* beta port: call fn(z, 1) */
    EMIT_LABEL(e, lbl_β);
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 1);
    emit_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
