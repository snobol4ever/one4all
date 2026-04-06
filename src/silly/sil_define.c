/*
 * sil_define.c — Defined functions (v311.sil §12 lines 4240–4470)
 *
 * Faithful C translation of Phil Budne's CSNOBOL4 v311.sil §12.
 *
 * DEFINE(P,E):
 *   Parses prototype string to extract function name, formal arguments,
 *   and local variables.  Builds a definition block and installs it via
 *   the function descriptor (ZCL from FINDEX).
 *   Requires STREAM/VARATB for prototype scanning — stubbed until M15+.
 *
 * DEFFNC:
 *   Invokes a defined function: saves/restores argument bindings,
 *   calls INTERP for the function body, unwinds on RETURN/FRETURN/NRETURN.
 *   Requires INTERP (M19) — stubbed.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M14
 */

#include <string.h>

#include "sil_types.h"
#include "sil_data.h"
#include "sil_define.h"
#include "sil_argval.h"
#include "sil_arena.h"
#include "sil_strings.h"
#include "sil_symtab.h"

/* External stubs */
extern Sil_result INVOKE_fn(void);
extern Sil_result PUTIN_fn(DESCR_t zptr, DESCR_t wptr);
extern Sil_result TRPHND_fn(DESCR_t atptr);
extern Sil_result FENTR2_fn(DESCR_t name);
extern Sil_result FNEXT2_fn(DESCR_t name);
extern Sil_result INTERP_fn(void);   /* M19 — not yet built */

/* STREAM/VARATB-based prototype parsing — not yet available */
extern int STREAM_varatb(SPEC_t *res, SPEC_t *src, int *stype_out);
/* GENVUP_fn declared in sil_arena.h */

#define GETDC_B(dst, base_d, off_i) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + (off_i), sizeof(DESCR_t))
#define PUTDC_B(base_d, off_i, src) \
    memcpy((char*)A2P(D_A(base_d)) + (off_i), &(src),  sizeof(DESCR_t))
#define GETD_B(dst, base_d, off_d) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + D_A(off_d), sizeof(DESCR_t))

/* ATTRIB offset */
#ifndef ATTRIB
#define ATTRIB  (2 * DESCR)
#endif

static DESCR_t def_stk[64];
static int def_top = 0;
static inline void    def_push(DESCR_t d) { def_stk[def_top++] = d; }
static inline DESCR_t def_pop(void)        { return def_stk[--def_top]; }

static inline int deql_fn(DESCR_t a, DESCR_t b)
{
    return D_A(a) == D_A(b) && D_V(a) == D_V(b);
}

/* ── DEFINE(P,E) ─────────────────────────────────────────────────────── */
/*
 * Prototype parsing requires STREAM with VARATB character-class table
 * (not yet implemented).  Until STREAM/VARATB is available, DEFINE
 * falls back to a simple C-string parser that handles the common case:
 *
 *   fname(arg1,arg2,...,argN:local1,local2,...,localM)
 *
 * This covers the vast majority of real SNOBOL4 programs.
 */
Sil_result DEFINE_fn(void)
{
    /* Get prototype */
    if (VARVAL_fn() == FAIL) return FAIL;
    def_push(XPTR);
    /* Get entry point (may be NULVCL if omitted) */
    if (VARVUP_fn() == FAIL) { def_top--; return FAIL; }
    MOVD(YPTR, XPTR);
    XPTR = def_pop();

    LOCSP_fn(&XSP, &XPTR);
    const char *src  = (const char*)A2P(XSP.a) + XSP.o;
    int32_t     slen = XSP.l;

    /* ── Parse function name ──────────────────────────────────────────── */
    int32_t ni = 0;
    while (ni < slen && src[ni] != '(') ni++;
    if (ni == 0 || ni >= slen) return FAIL;

    SPEC_t name_sp; name_sp.a = XSP.a; name_sp.o = XSP.o;
    name_sp.l = ni; name_sp.v = S; name_sp.f = 0;

    int32_t fn_off = GENVUP_fn(&name_sp);
    if (!fn_off) return FAIL;
    SETAC(XPTR, fn_off); SETVC(XPTR, S);

    /* FINDEX — get/create function descriptor */
    int32_t zcl_off = FINDEX_fn(&XPTR);
    if (!zcl_off) return FAIL;
    SETAC(ZCL, zcl_off);

    /* If entry point omitted, use function name */
    if (deql_fn(YPTR, NULVCL)) MOVD(YPTR, XPTR);
    def_push(YPTR);           /* save entry point */

    /* ── Parse formals and locals ────────────────────────────────────── */
    int32_t pos = ni + 1;  /* skip '(' */
    DESCR_t YCL_d; MOVD(YCL_d, ZEROCL);
    int in_locals = 0;     /* 0=formals, 1=locals */
    int nformals  = 0;
    def_push(XPTR);        /* save function name descriptor */

    while (pos < slen) {
        /* skip whitespace */
        while (pos < slen && src[pos] == ' ') pos++;
        if (pos >= slen) break;
        char c = src[pos];
        if (c == ')') { pos++; break; }
        if (c == ':') { in_locals = 1; pos++; continue; }
        if (c == ',') { pos++; continue; }

        /* read identifier */
        int32_t id_start = pos;
        while (pos < slen && src[pos] != ',' && src[pos] != ':' &&
               src[pos] != ')' && src[pos] != ' ') pos++;
        int32_t id_len = pos - id_start;
        if (id_len == 0) continue;

        SPEC_t id_sp; id_sp.a = XSP.a; id_sp.o = XSP.o + id_start;
        id_sp.l = id_len; id_sp.v = S; id_sp.f = 0;
        int32_t id_off = GENVUP_fn(&id_sp);
        if (!id_off) { def_top -= 2; return FAIL; }
        SETAC(XPTR, id_off); SETVC(XPTR, S);
        def_push(XPTR);
        INCRA(YCL_d, 1);
        if (!in_locals) nformals++;
    }

    /* SETVA DEFCL,YCL */
    D_V(DEFCL) = D_A(YCL_d);

    /* DEF11: INCRA YCL,2 for name and entry label */
    INCRA(YCL_d, 2);
    int32_t blk_bytes = D_A(YCL_d) * DESCR;
    SETVC(XCL, B);
    SETAC(XCL, blk_bytes);
    int32_t blk = BLOCK_fn(blk_bytes, B);
    if (!blk) { def_top -= (int)(YCL_d.a.i - 2 + 2); return FAIL; }
    SETAC(XPTR, blk);

    /* Update function descriptor ZCL */
    PUTDC_B(ZCL, 0, DEFCL);
    PUTDC_B(ZCL, DESCR, XPTR);

    /* Fill definition block from stack (in reverse) */
    /* Block layout: [0]=title, [1]=entry label, [2]=fn name, [3..]=args+locals */
    int32_t fill_idx = D_A(YCL_d) - 1;
    /* Pop all args+locals+name */
    while (fill_idx >= 2) {
        DESCR_t v = def_pop();
        PUTDC_B(XPTR, fill_idx * DESCR, v);
        fill_idx--;
    }
    /* [1] = entry point */
    DESCR_t entry_pt = def_pop();
    PUTDC_B(XPTR, DESCR, entry_pt);

    MOVD(XPTR, NULVCL); return OK;
}

/* ── DEFFNC — invoke defined function ───────────────────────────────── */
/*
 * Full implementation requires INTERP (M19) and the PUSH/POP operand-
 * stack discipline for saving/restoring argument bindings at runtime.
 *
 * Stubbed: returns FAIL until INTERP is available.
 * When M19 lands, replace this stub with the full translation following
 * v311.sil DEFFNC lines 4305–4470.
 */
Sil_result DEFFNC_fn(void)
{
    /* TODO M19: full argument-binding save/restore + INTERP call */
    return FAIL;
}
