/*
 * nmd.c — SIL Naming List (§NMD)  RT-4
 *
 * The naming list is a LIFO buffer of (target, substring) pairs written
 * during pattern matching by the . (conditional capture) operator.
 *
 * SIL globals modelled here:
 *   NAMICL — current top of naming list (nam_top)
 *   NHEDCL — base of current statement's naming list (nam_head saved as cookie)
 *   NBSPTR — buffer base (nam_buf[])
 *   PDLPTR/PDLHED — pattern history list (backtrack stack, separate; not here)
 *
 * SIL procs implemented:
 *   NMD    — commit all entries since cookie: assign each substr to its var
 *   NMD1–5 — (specialised commit variants; we implement one generic path)
 *   NMDIC  — keyword assignment path: coerce to INTEGER via ASGNIC_fn
 *   NAMEXN — EXPRESSION variable path: EXPEVL (stub — RT-6 fills this in)
 *
 * Public API:
 *   void    NAM_push(const char *var, int dt, const char *s, int len)
 *   int     NAM_save(void)
 *   void    NAM_commit(int cookie)
 *   void    NAM_discard(int cookie)
 *
 * Wire points:
 *   bb_capture (stmt_exec.c)  — on conditional γ calls NAM_push()
 *   exec_stmt  (stmt_exec.c)  — on success: NAM_commit(cookie)
 *                             — on failure: NAM_discard(cookie)
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-04-05
 * SPRINT:  RT-108
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gc/gc.h>

#include "snobol4.h"
#include "sil_macros.h"

/* ── Naming list buffer ───────────────────────────────────────────────────── */

#define NAM_MAX 512

typedef struct {
    const char *varname;   /* target variable name (DT_S path) */
    DESCR_t    *var_ptr;   /* DT_N NAMEPTR target (interior pointer) */
    int         dt;        /* DT_S / DT_K / DT_E — dispatch selector */
    const char *substr;    /* matched substring (GC-owned copy) */
    int         slen;      /* substring length */
} NamEntry_t;

static NamEntry_t nam_buf[NAM_MAX];
static int        nam_top  = 0;   /* NAMICL — current top */

/* ── NAM_push — called by bb_capture on conditional (.) γ ─────────────────
 *
 * var  — NV variable name (DT_S target); NULL if var_ptr is used
 * ptr  — DT_N interior pointer target; NULL if varname is used
 * dt   — DT_S for ordinary var, DT_K for keyword, DT_E for expression
 * s    — matched substring start pointer (in subject buffer)
 * len  — matched substring length
 */
void NAM_push(const char *var, DESCR_t *ptr, int dt,
              const char *s, int len)
{
    if (nam_top >= NAM_MAX) {
        /* Buffer overflow — silently drop (should not arise in practice) */
        fprintf(stderr, "nmd: NAM_MAX exceeded — capture dropped\n");
        return;
    }
    NamEntry_t *e = &nam_buf[nam_top++];
    e->varname = var;
    e->var_ptr = ptr;
    e->dt      = dt;
    /* Copy substring into GC-managed storage so it survives the match */
    if (s && len > 0) {
        char *copy = (char *)GC_MALLOC((size_t)len + 1);
        memcpy(copy, s, (size_t)len);
        copy[len] = '\0';
        e->substr = copy;
        e->slen   = len;
    } else {
        e->substr = "";
        e->slen   = 0;
    }
}

/* ── NAM_save — snapshot current top; returns cookie for commit/discard ───── */
int NAM_save(void)
{
    return nam_top;   /* caller stores as NHEDCL */
}

/* ── NAM_commit — SIL NMD: assign all entries pushed since cookie ──────────
 *
 * SIL NMD semantics: for each target variable, only the LAST push since
 * cookie is assigned.  This mirrors SNOBOL4's "last write wins" behaviour
 * when a conditional capture fires multiple times during backtracking
 * (e.g. ARB . V tries δ=0,1,2,... — only the winning δ is committed).
 *
 * Algorithm: walk backwards from nam_top-1 to cookie.  For each entry,
 * assign only if this variable has not yet been seen (set-once per var).
 * After commit, restores nam_top to cookie.
 *
 * Dispatch:
 *   DT_K — NMDIC: call ASGNIC_fn (keyword coerce to INTEGER)
 *   DT_E — NAMEXN stub: log warning, skip (RT-6 fills in EXPEVL)
 *   else — NV_SET_fn or interior-pointer write
 */
void NAM_commit(int cookie)
{
    /* Walk backwards: last push for each target wins.
     * Use a seen[] parallel array to track which targets we've assigned. */
#define NAM_SEEN_MAX 64
    const char *seen_name[NAM_SEEN_MAX];
    DESCR_t    *seen_ptr[NAM_SEEN_MAX];
    int         seen_count = 0;

    for (int i = nam_top - 1; i >= cookie; i--) {
        NamEntry_t *e = &nam_buf[i];

        /* Check if we already committed this target (ptr or name) */
        int already = 0;
        for (int j = 0; j < seen_count; j++) {
            if (e->var_ptr && seen_ptr[j] == e->var_ptr) { already = 1; break; }
            if (!e->var_ptr && e->varname && seen_name[j] &&
                strcmp(seen_name[j], e->varname) == 0) { already = 1; break; }
        }
        if (already) continue;

        /* Record as seen */
        if (seen_count < NAM_SEEN_MAX) {
            seen_name[seen_count] = e->varname;
            seen_ptr[seen_count]  = e->var_ptr;
            seen_count++;
        }

        DESCR_t val;
        val.v    = DT_S;
        val.slen = (uint32_t)e->slen;
        val.s    = (char *)e->substr;

        if (e->dt == DT_K) {
            if (e->varname)
                ASGNIC_fn(e->varname, val);
        } else if (e->dt == DT_E) {
            fprintf(stderr, "nmd: NAMEXN (DT_E target) not yet implemented\n");
        } else {
            if (e->var_ptr)
                *e->var_ptr = val;
            else if (e->varname && e->varname[0])
                NV_SET_fn(e->varname, val);
        }
    }
    nam_top = cookie;
}

/* ── NAM_discard — on pattern failure: restore nam_top to cookie ───────────
 *
 * SIL: restore NAMICL to NHEDCL — nothing is assigned.
 * The substring copies in GC-managed storage will be collected.
 */
void NAM_discard(int cookie)
{
    nam_top = cookie;
}
