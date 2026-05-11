/*
 * templates/bb_xlnth.c — integer-cursor family BB box template.
 *
 * Covers three box kinds with the same ABI (integer operand n):
 *   XLNTH  (LEN n)    → bb_len(zeta, port);  zeta = len_t  {int n}
 *   XTB    (TAB n)    → bb_tab(zeta, port);  zeta = tab_t  {int n, int pad}
 *   XRTB   (RTAB n)   → bb_rtab(zeta, port); zeta = rtab_t {int n, int pad}
 *
 * All three: load integer argument n into a static/heap struct, pass
 * struct ptr as rdi to the runtime function, dispatch on return value.
 *
 * Text path: provided by bb_flat.c (intcur_text_body callback), which
 * has access to flat_data_section / flat3c_label / flat_data_long etc.
 *
 * Binary path: this file.  Uses bb_len_new / bb_tab_new / bb_rtab_new
 * to heap-allocate the zeta struct, then flat_emit_box_call for the
 * alpha/beta dispatch.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-g (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-g / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_flat.h"
#include "../bb_emit.h"
#include "templates.h"

extern len_t  *bb_len_new (int n);
extern tab_t  *bb_tab_new (int n);
extern rtab_t *bb_rtab_new(int n);

extern DESCR_t bb_len  (void *zeta, int entry);
extern DESCR_t bb_tab  (void *zeta, int entry);
extern DESCR_t bb_rtab (void *zeta, int entry);

/* ── emit_bb_intcur ──────────────────────────────────────────────────────── */

/*
 * emit_bb_intcur — generic integer-cursor box template.
 *
 * Parameters:
 *   e            — emitter
 *   c_fn         — runtime box fn (bb_len / bb_tab / bb_rtab)
 *   c_fn_name    — fn name string
 *   kind_name    — banner name (LEN / TAB / RTAB)
 *   num          — integer argument n
 *   lbl_succ / lbl_fail / lbl_β — port labels
 *   text_body_fn — callback into bb_flat.c for the text path
 *   text_body_arg — opaque forwarded to text_body_fn
 */
void emit_bb_intcur(emitter_t *e,
                    bb_box_fn c_fn,
                    const char *c_fn_name,
                    const char *kind_name,
                    long long num,
                    bb_label_t *lbl_succ,
                    bb_label_t *lbl_fail,
                    bb_label_t *lbl_β,
                    bb_intcur_text_fn text_body_fn,
                    void *text_body_arg)
{
    (void)kind_name;
    if (!e) return;

    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }

    /* ── Binary path ─────────────────────────────────────────────────────── */

    /* Allocate the per-match zeta struct via the appropriate constructor.
     * The constructors are chosen by the per-kind wrappers below. */
    void *z;
    if      (c_fn == bb_len)  z = bb_len_new ((int)num);
    else if (c_fn == bb_tab)  z = bb_tab_new ((int)num);
    else if (c_fn == bb_rtab) z = bb_rtab_new((int)num);
    else {
        /* Unknown fn — fall back to a raw malloc of an int-sized struct. */
        int *raw = calloc(2, sizeof(int));
        raw[0] = (int)num;
        z = raw;
    }

    flat_emit_box_call(e, c_fn, c_fn_name, z, lbl_succ, lbl_fail, lbl_β);
}

/* ── Per-kind wrappers ───────────────────────────────────────────────────── */

void emit_bb_xlnth(emitter_t *e, long long num,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                   bb_intcur_text_fn text_fn, void *text_arg)
{
    emit_bb_intcur(e, bb_len,  "bb_len",  "LEN",  num, lbl_succ, lbl_fail, lbl_β, text_fn, text_arg);
}

void emit_bb_xtb(emitter_t *e, long long num,
                 bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                 bb_intcur_text_fn text_fn, void *text_arg)
{
    emit_bb_intcur(e, bb_tab,  "bb_tab",  "TAB",  num, lbl_succ, lbl_fail, lbl_β, text_fn, text_arg);
}

void emit_bb_xrtb(emitter_t *e, long long num,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                  bb_intcur_text_fn text_fn, void *text_arg)
{
    emit_bb_intcur(e, bb_rtab, "bb_rtab", "RTAB", num, lbl_succ, lbl_fail, lbl_β, text_fn, text_arg);
}
