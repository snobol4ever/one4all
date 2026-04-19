/*
 * snobol4_nmd.c — SIL Naming List (§NMD)  SN-21b: flat NAME_t[] stack
 *
 * ARCHITECTURE:
 *
 *   One flat array of NAM_entry_t (a NAME_t + the captured substring),
 *   grown on demand.  A single int `g_top` tracks the high-water slot.
 *
 *     stack: [ 0 | 1 | 2 | ... | top-1 ]    grows right
 *
 *   Two primary operations drive all pattern-time capture bookkeeping:
 *
 *     NAME_push(nm, substr, slen)  — append slot; return handle (index).
 *     NAME_pop(handle)              — drop that slot (LIFO from top).
 *
 *   Two bracket operations frame statement-match and EVAL contexts:
 *
 *     NAME_mark()                   — snapshot current top.
 *     NAME_commit_above(mark)       — walk stack[mark..top), fire each
 *                                     through name_commit_value; top = mark.
 *     NAME_discard_above(mark)      — top = mark (drop, don't fire).
 *
 * EVERY box γ push has a matching β/ω pop — boxes own and self-unwind
 * their own slots.  There is never "leftover" pushed state once a box
 * has backtracked or failed out.  Commit at the statement-match bracket
 * walks entries in push order (oldest → newest) to preserve the
 * capture-before-callcap semantics fixed in SC-26.
 *
 * LEGACY SHIMS:
 *
 *   NAM_save / NAM_push / NAM_push_callcap* / NAM_pop_one / NAM_commit /
 *   NAM_discard / NAM_pop / NAM_mark / NAM_rollback_to remain as thin
 *   wrappers mapping to the new API for the duration of SN-21b..SN-21d,
 *   so stmt_exec.c, bb_boxes.c, and eval_code.c still compile.  They
 *   are deleted in SN-21e.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Opus 4.7
 * DATE:    2026-04-19
 * SPRINT:  SN-21b
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <gc/gc.h>

#include "snobol4.h"
#include "sil_macros.h"
#include "name_t.h"

/*===========================================================================*/
/* NAM_entry_t — one slot in the flat stack                                  */
/*===========================================================================*/

typedef struct {
    int         live;        /* 1 = occupied, 0 = tombstone                     */
    NAME_t      name;        /* full unified lvalue descriptor                  */
    const char *substr;      /* GC_malloc'd captured text (epsilon → "")        */
    int         slen;
    int         legacy_dt;   /* DT_S normally; DT_K / DT_E via NAM_push shim    */
} NAM_entry_t;

/*===========================================================================*/
/* The stack                                                                  */
/*===========================================================================*/

static NAM_entry_t *g_stack = NULL;
static int          g_cap   = 0;
static int          g_top   = 0;

static void ensure_capacity(int need)
{
    if (need <= g_cap) return;
    int newcap = g_cap ? g_cap * 2 : 64;
    while (newcap < need) newcap *= 2;
    NAM_entry_t *fresh = (NAM_entry_t *)GC_MALLOC((size_t)newcap * sizeof(NAM_entry_t));
    if (g_stack && g_top > 0)
        memcpy(fresh, g_stack, (size_t)g_top * sizeof(NAM_entry_t));
    g_stack = fresh;
    g_cap   = newcap;
}

static const char *dup_substr(const char *s, int len)
{
    if (!s || len <= 0) return "";
    char *copy = (char *)GC_MALLOC((size_t)len + 1);
    memcpy(copy, s, (size_t)len);
    copy[len] = '\0';
    return copy;
}

/* Handle encoding: slot-index + 1, so NULL ≠ slot 0. */
static inline void *idx_to_handle(int i) { return (void *)(intptr_t)(i + 1); }
static inline int   handle_to_idx(void *h) { return h ? (int)(intptr_t)h - 1 : -1; }

/*===========================================================================*/
/* NAME_push — primary push                                                   */
/*===========================================================================*/

void *NAME_push(const NAME_t *nm, const char *substr, int slen)
{
    if (!nm) return NULL;

    ensure_capacity(g_top + 1);

    int idx = g_top;
    NAM_entry_t *e = &g_stack[idx];
    e->live      = 1;
    e->name      = *nm;
    e->substr    = dup_substr(substr, slen);
    e->slen      = (slen > 0) ? slen : 0;
    e->legacy_dt = DT_S;

    g_top++;
    return idx_to_handle(idx);
}

/*===========================================================================*/
/* NAME_pop — primary pop                                                     */
/*                                                                            */
/* Drop the slot.  LIFO case (handle is top-1) is O(1).  Non-LIFO pops mark  */
/* a tombstone; trailing tombstones are collapsed on every pop so g_top      */
/* always refers to a live slot.                                              */
/*===========================================================================*/

void NAME_pop(void *handle)
{
    int idx = handle_to_idx(handle);
    if (idx < 0 || idx >= g_top) return;

    NAM_entry_t *e = &g_stack[idx];
    if (!e->live) return;
    e->live = 0;

    while (g_top > 0 && !g_stack[g_top - 1].live) g_top--;
}

/*===========================================================================*/
/* NAME_mark — snapshot top                                                   */
/*===========================================================================*/

int NAME_mark(void)
{
    return g_top;
}

/*===========================================================================*/
/* NAME_commit_above — fire entries [mark..top) in push order; top = mark     */
/*===========================================================================*/

static int same_var_target(const NAM_entry_t *a, const NAM_entry_t *b)
{
    if (a->name.kind != b->name.kind) return 0;
    if (a->name.kind == NM_PTR)
        return a->name.var_ptr && a->name.var_ptr == b->name.var_ptr;
    if (a->name.kind == NM_VAR)
        return a->name.var_name && b->name.var_name
               && strcmp(a->name.var_name, b->name.var_name) == 0;
    return 0;
}

void NAME_commit_above(int mark)
{
    if (mark < 0) mark = 0;
    if (mark > g_top) mark = g_top;

    for (int i = mark; i < g_top; i++) {
        NAM_entry_t *e = &g_stack[i];
        if (!e->live) continue;

        /* Last-write-wins dedup for NM_VAR / NM_PTR — stop at intervening NM_CALL. */
        if (e->name.kind == NM_VAR || e->name.kind == NM_PTR) {
            int superseded = 0;
            for (int j = i + 1; j < g_top; j++) {
                NAM_entry_t *f = &g_stack[j];
                if (!f->live) continue;
                if (f->name.kind == NM_CALL) break;
                if (same_var_target(e, f)) { superseded = 1; break; }
            }
            if (superseded) continue;
        }

        DESCR_t val = { .v = DT_S, .slen = (uint32_t)e->slen,
                        .s = (char *)e->substr };
        name_commit_value(&e->name, val);
    }

    g_top = mark;
}

/*===========================================================================*/
/* NAME_discard_above — drop [mark..top) without firing                       */
/*===========================================================================*/

void NAME_discard_above(int mark)
{
    if (mark < 0) mark = 0;
    if (mark > g_top) mark = g_top;
    g_top = mark;
}

/*===========================================================================*/
/* ─── LEGACY SHIMS (deleted in SN-21e) ──────────────────────────────────── */
/*===========================================================================*/

/* NAM_save — "push frame" becomes "read current top". */
int NAM_save(void)
{
    return NAME_mark();
}

/* NAM_pop — old "pop frame" is a no-op in the flat model. */
void NAM_pop(int cookie)
{
    (void)cookie;
}

/* NAM_push — old CAPTURE push carrying DT_S / DT_K / DT_E dispatch. */
void *NAM_push(const char *var, DESCR_t *ptr, int dt,
               const char *s, int len)
{
    NAME_t nm;
    if (ptr) name_init_as_ptr(&nm, ptr);
    else     name_init_as_var(&nm, var ? GC_strdup(var) : NULL);

    void *h = NAME_push(&nm, s, len);
    int idx = handle_to_idx(h);
    if (idx >= 0) g_stack[idx].legacy_dt = dt;
    return h;
}

/* NAM_push_callcap / NAM_push_callcap_named — NM_CALL entry. */
void *NAM_push_callcap(const char *fnc_name, DESCR_t *fnc_args, int fnc_nargs,
                       const char *matched_text, int matched_len)
{
    return NAM_push_callcap_named(fnc_name, fnc_args, fnc_nargs,
                                   NULL, 0, matched_text, matched_len);
}

void *NAM_push_callcap_named(const char *fnc_name,
                              DESCR_t *fnc_args, int fnc_nargs,
                              char **fnc_arg_names, int fnc_n_arg_names,
                              const char *matched_text, int matched_len)
{
    NAME_t nm;
    name_init_as_call(&nm, fnc_name, fnc_args, fnc_nargs,
                      fnc_arg_names, fnc_n_arg_names);
    return NAME_push(&nm, matched_text, matched_len);
}

/* NAM_pop_one — alias for NAME_pop. */
void NAM_pop_one(void *handle)
{
    NAME_pop(handle);
}

/* NAM_commit — honours legacy DT_K / DT_E then DT_S commit pass. */
void NAM_commit(int cookie)
{
    extern DESCR_t EVAL_fn(DESCR_t);

    int mark = cookie;
    if (mark < 0) mark = 0;
    if (mark > g_top) mark = g_top;

    /* Pass 1: legacy non-DT_S dispatch.  Handle them and tombstone the slot
     * so the DT_S pass skips it. */
    for (int i = mark; i < g_top; i++) {
        NAM_entry_t *e = &g_stack[i];
        if (!e->live) continue;
        if (e->name.kind == NM_CALL) continue;    /* always DT_S */
        int dt = e->legacy_dt;
        if (dt == DT_S) continue;

        if (dt == DT_K) {
            DESCR_t val = { .v = DT_S, .slen = (uint32_t)e->slen,
                            .s = (char *)e->substr };
            if (e->name.kind == NM_VAR && e->name.var_name)
                ASGNIC_fn(e->name.var_name, val);
            e->live = 0;
            continue;
        }

        if (dt == DT_E) {
            DESCR_t expr_d = { .v = DT_E, .ptr = e->name.var_ptr,
                               .slen = 0, .s = NULL };
            DESCR_t evval = EVAL_fn(expr_d);
            if (!IS_FAIL_fn(evval)) {
                if (e->name.kind == NM_VAR && e->name.var_name
                    && e->name.var_name[0])
                    NV_SET_fn(e->name.var_name, evval);
            }
            e->live = 0;
            continue;
        }
    }

    /* Pass 2: unified DT_S commit via NAME_commit_above. */
    NAME_commit_above(mark);
}

/* NAM_discard — clear current frame entries. */
void NAM_discard(int cookie)
{
    NAME_discard_above(cookie);
}

/* NAM_mark — opaque pointer-cookie wrapping NAME_mark(). */
void *NAM_mark(void)
{
    return (void *)(intptr_t)(NAME_mark() + 1);
}

/* NAM_rollback_to — SN-20: no-op by design (boxes self-unwind). */
void NAM_rollback_to(void *mark)
{
    (void)mark;
}
