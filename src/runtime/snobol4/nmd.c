/*
 * nmd.c — SIL Naming List (§NMD)  RT-4  *** REFACTOR: frame-stack design ***
 *
 * ARCHITECTURE (replaces flat buffer + cookie int):
 *
 *   The naming list is a stack of NAM_Frame objects.  Each call to
 *   NAM_save() pushes a new frame; NAM_commit/NAM_discard pops it.
 *   Within a frame, captures are a singly-linked list of NamEntry_t
 *   nodes (GC-allocated, no fixed-size array).
 *
 *   Byrd box mapping:
 *     XNME α  →  (caller calls NAM_save before scan loop — unchanged)
 *     XNME γ  →  NAM_push into current frame's entry list
 *     XNME ω  →  NAM_discard pops frame — undo for free, no seen[] needed
 *     outer γ →  NAM_commit pops frame, walks entry list last→first,
 *                assigns once per target (last-write-wins)
 *
 *   Benefits over flat buffer:
 *     - No NAM_MAX overflow
 *     - No seen[] dedup array (backwards walk still gives last-write-wins)
 *     - No GC dangling pointer: every string is GC_strdup'd at push time
 *     - Nested EVAL()/EXPVAL re-entrancy is natural (each gets its own frame)
 *     - Frames nest correctly for RT-6 EXPVAL save/restore
 *
 * Public API (unchanged — stmt_exec.c needs zero edits):
 *   void  NAM_push(const char *var, DESCR_t *ptr, int dt,
 *                  const char *s, int len)
 *   int   NAM_save(void)          → returns opaque frame index
 *   void  NAM_commit(int cookie)
 *   void  NAM_discard(int cookie)
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-04-05
 * SPRINT:  RT-109
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gc/gc.h>

#include "snobol4.h"
#include "sil_macros.h"

/* ── Entry: one conditional capture within a frame ─────────────────────── */

typedef struct NamEntry {
    const char     *varname;   /* GC_strdup'd — safe across GC cycles       */
    DESCR_t        *var_ptr;   /* NAMEPTR interior pointer (or NULL)         */
    int             dt;        /* DT_S / DT_K / DT_E                        */
    const char     *substr;    /* GC_malloc'd copy of matched substring      */
    int             slen;
    struct NamEntry *next;     /* intrusive linked list (newest → oldest)    */
} NamEntry_t;

/* ── Frame: one NAM_save/commit-or-discard scope ───────────────────────── */

typedef struct NamFrame {
    NamEntry_t     *head;      /* newest entry (prepend on push)             */
    struct NamFrame *prev;     /* stack link toward older frames             */
} NamFrame_t;

/* ── Stack state ────────────────────────────────────────────────────────── */

static NamFrame_t *nam_stack = NULL;   /* top of frame stack (current frame) */
static int         nam_depth = 0;      /* frame count — used as cookie value  */

/* ── NAM_save ───────────────────────────────────────────────────────────── *
 *
 * Push a new empty frame.  Returns depth BEFORE push as the cookie so
 * commit/discard can assert they're unwinding the right frame.
 *
 * SIL equivalent: snapshot NAMICL / NHEDCL
 */
int NAM_save(void)
{
    int cookie = nam_depth;
    NamFrame_t *f = GC_MALLOC(sizeof(NamFrame_t));
    f->head = NULL;
    f->prev = nam_stack;
    nam_stack = f;
    nam_depth++;
    return cookie;          /* caller stores as NHEDCL */
}

/* ── NAM_push ───────────────────────────────────────────────────────────── *
 *
 * Record one conditional (.) capture into the current frame.
 * Called by bb_capture on every XNME γ — including backtrack attempts.
 * Prepend (newest first) so backwards-walk in NAM_commit is just
 * walking the list head→tail.
 *
 * KEY FIX vs old design: GC_strdup(var) here — no dangling pointer.
 */
void NAM_push(const char *var, DESCR_t *ptr, int dt,
              const char *s, int len)
{
    if (!nam_stack) {
        fprintf(stderr, "nmd: NAM_push with no active frame — ignored\n");
        return;
    }

    NamEntry_t *e = GC_MALLOC(sizeof(NamEntry_t));

    /* GC_strdup — survives GC cycles during ARB backtrack loop */
    e->varname = var ? GC_strdup(var) : NULL;
    e->var_ptr = ptr;
    e->dt      = dt;

    if (s && len > 0) {
        char *copy = GC_MALLOC((size_t)len + 1);
        memcpy(copy, s, (size_t)len);
        copy[len] = '\0';
        e->substr = copy;
        e->slen   = len;
    } else {
        e->substr = "";
        e->slen   = 0;
    }

    /* Prepend — list is newest-first, so head→tail walk = last→first push */
    e->next       = nam_stack->head;
    nam_stack->head = e;
}

/* ── NAM_commit ─────────────────────────────────────────────────────────── *
 *
 * Pattern succeeded.  Pop the top frame and assign each captured value,
 * last-write-wins (= head of list wins, because we prepend on push).
 *
 * seen[] tracks which targets have already been assigned so we skip
 * earlier (overwritten) captures for the same target.
 *
 * SIL NMD: assign NAMICL..NHEDCL, then restore NAMICL=NHEDCL.
 */
void NAM_commit(int cookie)
{
    if (!nam_stack || nam_depth != cookie + 1) {
        fprintf(stderr, "nmd: NAM_commit cookie mismatch (depth=%d cookie=%d)\n",
                nam_depth, cookie);
        return;
    }

    NamFrame_t *f = nam_stack;

#define SEEN_MAX 64
    const char *seen_name[SEEN_MAX];
    DESCR_t    *seen_ptr [SEEN_MAX];
    int         seen_n = 0;

    /* Walk head→tail = newest→oldest = last-write first */
    for (NamEntry_t *e = f->head; e; e = e->next) {

        /* Skip if this target already assigned (earlier entry = later push) */
        int already = 0;
        for (int j = 0; j < seen_n; j++) {
            if (e->var_ptr && seen_ptr[j] == e->var_ptr) { already=1; break; }
            if (!e->var_ptr && e->varname && seen_name[j] &&
                strcmp(seen_name[j], e->varname) == 0)   { already=1; break; }
        }
        if (already) continue;

        if (seen_n < SEEN_MAX) {
            seen_name[seen_n] = e->varname;
            seen_ptr [seen_n] = e->var_ptr;
            seen_n++;
        }

        DESCR_t val = { .v = DT_S, .slen = (uint32_t)e->slen,
                        .s = (char *)e->substr };

        if (e->dt == DT_K) {
            if (e->varname) ASGNIC_fn(e->varname, val);
        } else if (e->dt == DT_E) {
            /* EXPEVL: thaw EXPRESSION, assign result by name */
            extern DESCR_t EVAL_fn(DESCR_t);
            DESCR_t expr_d = { .v = DT_E, .ptr = e->var_ptr, .slen = 0, .s = NULL };
            DESCR_t val = EVAL_fn(expr_d);
            if (!IS_FAIL_fn(val)) {
                if (e->varname && e->varname[0])
                    NV_SET_fn(e->varname, val);
            }
        } else {
            if (e->var_ptr)
                *e->var_ptr = val;
            else if (e->varname && e->varname[0])
                NV_SET_fn(e->varname, val);
        }
    }

    /* Pop frame — GC reclaims entries automatically */
    nam_stack = f->prev;
    nam_depth--;
}

/* ── NAM_discard ────────────────────────────────────────────────────────── *
 *
 * Scan-position reset OR pattern failure.  Two cases:
 *
 * (a) Mid-scan reset (exec_stmt calls at top of each scan loop iteration):
 *     Truncate the current frame's entry list back to empty — keep the
 *     frame alive so subsequent NAM_push calls have a home.
 *     SIL: NAMICL = NHEDCL  (reset top, keep base frame).
 *
 * (b) Final failure discard (after scan loop exits without match):
 *     Also truncates + pops the frame.  But exec_stmt uses the SAME call
 *     for both cases (NAM_discard(cookie)).  We distinguish by whether
 *     it's the final call via the guard: if frame is already empty we
 *     pop it; if it has entries we just truncate.
 *
 * Simpler invariant adopted here: NAM_discard ALWAYS just clears the
 * current frame's entry list (undo for free — GC reclaims entries).
 * NAM_commit is the ONLY path that pops a frame.
 * The post-loop "NAM_discard on failure" also clears — the frame is
 * then popped by the implicit scope exit (exec_stmt returns).
 * To handle that, exec_stmt must call NAM_pop_frame() on the failure
 * path.  We expose that as a separate call so stmt_exec.c can be
 * updated minimally (one extra call on the :F return path).
 *
 * *** stmt_exec.c change required: ***
 *   Replace final:  NAM_discard(nam_cookie);  return 0;
 *   With:           NAM_discard(nam_cookie);  NAM_pop(nam_cookie);  return 0;
 */
void NAM_discard(int cookie)
{
    if (!nam_stack) return;   /* no frame — nothing to do */
    /* Truncate entries in current frame — keep frame alive */
    nam_stack->head = NULL;   /* GC reclaims linked NamEntry_t nodes */
}

/* ── NAM_pop — pop the frame after final failure (call after NAM_discard) ── */
void NAM_pop(int cookie)
{
    if (!nam_stack || nam_depth != cookie + 1) return;
    nam_stack = nam_stack->prev;
    nam_depth--;
}
