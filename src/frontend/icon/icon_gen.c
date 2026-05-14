/*============================================================================================================================
 * icon_gen.c — Icon Value-Generator Byrd Box Implementations (GOAL-ICN-BROKER, GOAL-UNIFIED-BROKER U-7/U-10)
 *
 * U-7:  icn_gen_t → bb_node_t; icn_box_fn → bb_box_fn throughout.
 * U-10: icn_broker removed — all call sites use bb_broker(..., BB_PUMP, ...) directly.
 *
 * Architecture mirrors SNOBOL4's exec_stmt Phase 3 broker loop (stmt_exec.c):
 *   Phase 3 (SNOBOL4):  root.fn(ζ,α) → body → root.fn(ζ,β) → … → ω
 *   Icon generators:    gen.fn(ζ,α)   → body → gen.fn(ζ,β)  → … → ω  (bb_broker BB_PUMP)
 *
 * Value type: DESCR_t (not spec_t).  Failure sentinel: FAILDESCR / IS_FAIL_fn().
 *============================================================================================================================*/

#include "icon_gen.h"
#include "../ast/ast.h"            /* tree_t, tree_e, TT_TO, TT_TO_BY, TT_ITERATE, TT_SUSPEND, TT_FNC */
#include "../../runtime/common/coerce.h"  /* descr_to_str_icn (D-1/D-2 RS-6) */
#include "../../runtime/interp/coro_runtime.h"  /* CORO_STACK_SZ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <unistd.h>

/*============================================================================================================================
 * B-3: coro_bb_to — TT_TO Byrd box  (i to j)
 *
 * State: lo, hi, cur.
 *   α: cur = lo; if cur > hi → ω; else return integer cur (γ).
 *   β: cur++; if cur > hi → ω; else return integer cur (γ).
 *============================================================================================================================*/

DESCR_t coro_bb_to(void *zeta, int entry) {
    icn_to_state_t *z = (icn_to_state_t *)zeta;
    if (entry == α) z->cur = z->lo;
    else            z->cur++;
    if (z->cur > z->hi) return FAILDESCR;
    return (DESCR_t){ .v = DT_I, .i = z->cur };
}

/*============================================================================================================================
 * coro_bb_to_nested — (lo_gen) to (hi_gen) cross-product Byrd box
 *
 * JCON irgen.icn ir_a_To nested case: when lo or hi is itself a generator,
 * pre-collect all values from each, then iterate outer lo × hi pairs,
 * yielding each inner lo_val..hi_val range in sequence.
 *
 * State pre-populated by coro_eval before returning this box.
 * α: li=0, hi2=0, cur=lo_vals[0]; step through inner range.
 * β: cur++; if cur > hi_vals[hi2]: hi2++; if hi2 >= nhi: li++, hi2=0; reset cur.
 * ω: li >= nlo.
 *============================================================================================================================*/

DESCR_t coro_bb_to_nested(void *zeta, int entry) {
    icn_to_nested_state_t *z = (icn_to_nested_state_t *)zeta;
    if (z->nlo == 0 || z->nhi == 0) return FAILDESCR;
    if (entry == α) { z->li = 0; z->hi2 = 0; z->cur = z->lo_vals[0]; }
    else            { z->cur++; }
    /* Advance outer indices when inner range is exhausted */
    for (;;) {
        if (z->li >= z->nlo) return FAILDESCR;
        long hi_bound = z->hi_vals[z->hi2];
        if (z->cur <= hi_bound) return (DESCR_t){ .v = DT_I, .i = z->cur };
        /* exhausted this (li, hi2) pair — advance hi2, then li */
        z->hi2++;
        if (z->hi2 >= z->nhi) { z->li++; z->hi2 = 0; }
        if (z->li >= z->nlo) return FAILDESCR;
        z->cur = z->lo_vals[z->li];
    }
}

/*============================================================================================================================
 * B-4: coro_bb_to_by — TT_TO_BY Byrd box  (i to j by k)
 *
 * State: lo, hi, step, cur.
 *   α: cur = lo.
 *   β: cur += step.
 *   if step > 0: cur > hi → ω.   if step < 0: cur < hi → ω.
 *============================================================================================================================*/

DESCR_t coro_bb_to_by(void *zeta, int entry) {
    icn_to_by_state_t *z = (icn_to_by_state_t *)zeta;
    if (entry == α) z->cur = z->lo;
    else            z->cur += z->step;
    long step = z->step ? z->step : 1;
    if (step > 0 && z->cur > z->hi) return FAILDESCR;
    if (step < 0 && z->cur < z->hi) return FAILDESCR;
    return (DESCR_t){ .v = DT_I, .i = z->cur };
}

/* coro_bb_to_by_real — TT_TO_BY with real (float) step/bounds */
DESCR_t coro_bb_to_by_real(void *zeta, int entry) {
    icn_to_by_real_state_t *z = (icn_to_by_real_state_t *)zeta;
    if (entry == α) z->cur = z->lo;
    else            z->cur += z->step;
    double step = z->step != 0.0 ? z->step : 1.0;
    if (step > 0.0 && z->cur > z->hi + 1e-10) return FAILDESCR;
    if (step < 0.0 && z->cur < z->hi - 1e-10) return FAILDESCR;
    return (DESCR_t){ .v = DT_R, .r = z->cur };
}

/*============================================================================================================================
 * B-5: coro_bb_iterate — TT_ITERATE Byrd box  (!str, Icon char iteration)
 *
 * State: str, len, pos.
 *   α: pos = 0.  β: pos++.  ω: pos >= len.  γ: single-char string at pos.
 *============================================================================================================================*/

DESCR_t coro_bb_iterate(void *zeta, int entry) {
    icn_iterate_state_t *z = (icn_iterate_state_t *)zeta;
    if (entry == α) z->pos = 0;
    else            z->pos++;
    if (z->pos >= z->len) return FAILDESCR;
    z->ch[0] = z->str[z->pos];
    z->ch[1] = '\0';
    /* IJ-14: must not return z->ch directly — callers like put(L,!s) store
     * the DESCR_t.s pointer; all stored entries would alias the same z->ch
     * buffer and see the last character.  GC_strdup gives each tick its own
     * allocation so stored descriptors are independent. */
    return (DESCR_t){ .v = DT_S, .slen = 1, .s = GC_strdup(z->ch) };
}

/*============================================================================================================================
 * B-5b: coro_bb_tbl_iterate — TT_ITERATE Byrd box for DT_T tables  (!T yields values)
 *
 * State: tbl, bucket (0..TABLE_BUCKETS-1), entry (current TBPAIR_t*).
 *   α: bucket=0, entry=tbl->buckets[0].
 *   β: advance to next entry (or next non-empty bucket).
 *   ω: all buckets exhausted.
 *   γ: return entry->val.
 *============================================================================================================================*/

DESCR_t coro_bb_tbl_iterate(void *zeta, int entry) {
    icn_tbl_iterate_state_t *z = (icn_tbl_iterate_state_t *)zeta;
    if (!z->tbl) return FAILDESCR;
    if (entry == α) { z->bucket = 0; z->entry = z->tbl->buckets[0]; }
    else if (z->entry) { z->entry = z->entry->next; }
    /* advance past empty buckets */
    while (!z->entry && z->bucket < TABLE_BUCKETS - 1) {
        z->bucket++;
        z->entry = z->tbl->buckets[z->bucket];
    }
    if (!z->entry) return FAILDESCR;
    return z->entry->val;
}

/*============================================================================================================================
 * IC-5: coro_bb_list_iterate — TT_ITERATE Byrd box for DT_DATA icnlist  (!L yields elements)
 *   Holds the live list DT_DATA descriptor so it sees elements added by put() after box creation.
 *   α: reset pos=0, return elems[0].
 *   β: advance pos, return elems[pos].
 *   ω: pos >= n.
 *============================================================================================================================*/
DESCR_t coro_bb_list_iterate(void *zeta, int entry) {
    icn_list_iterate_state_t *z = (icn_list_iterate_state_t *)zeta;
    /* Re-read elems and size from live object each tick */
    DESCR_t ea = FIELD_GET_fn(z->list_obj, "frame_elems");
    int n = (int)FIELD_GET_fn(z->list_obj, "frame_size").i;
    DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
    if (!elems || n <= 0) return FAILDESCR;
    if (entry == α) z->pos = 0;
    else            z->pos++;
    if (z->pos >= n) return FAILDESCR;
    return elems[z->pos];
}

/*============================================================================================================================
 * IC-9 (2026-05-01): coro_bb_record_iterate — TT_ITERATE Byrd box for DT_DATA records (!R yields field values).
 *   Records are DT_DATA but NOT the icnlist shape (no "icn_type" tag of "list"); their structure lives in
 *   inst.u->type->nfields and inst.u->fields[i].  Iterates fields in declaration order.  Re-reads the live
 *   instance each tick so mutations made between ticks (rare; field-iteration usually one-pass) are visible.
 *   α: pos=0, return fields[0] (or fail if nfields==0).
 *   β: pos++, return fields[pos] (or fail when pos >= nfields).
 *============================================================================================================================*/
DESCR_t coro_bb_record_iterate(void *zeta, int entry) {
    icn_record_iterate_state_t *z = (icn_record_iterate_state_t *)zeta;
    if (z->inst.v != DT_DATA || !z->inst.u || !z->inst.u->type) return FAILDESCR;
    int n = z->inst.u->type->nfields;
    if (n <= 0 || !z->inst.u->fields) return FAILDESCR;
    if (entry == α) z->pos = 0;
    else            z->pos++;
    if (z->pos >= n) return FAILDESCR;
    return z->inst.u->fields[z->pos];
}

/*============================================================================================================================
 * IC-6: coro_bb_tbl_key_iterate — key(T) generator: yields each key in table T
 *   Same bucket-walk as coro_bb_tbl_iterate but returns entry->key_descr instead of entry->val.
 *============================================================================================================================*/
DESCR_t coro_bb_tbl_key_iterate(void *zeta, int entry) {
    icn_tbl_key_iterate_state_t *z = (icn_tbl_key_iterate_state_t *)zeta;
    if (!z->tbl) return FAILDESCR;
    if (entry == α) { z->bucket = 0; z->entry = z->tbl->buckets[0]; }
    else if (z->entry) { z->entry = z->entry->next; }
    while (!z->entry && z->bucket < TABLE_BUCKETS - 1) {
        z->bucket++;
        z->entry = z->tbl->buckets[z->bucket];
    }
    if (!z->entry) return FAILDESCR;
    return z->entry->key_descr;
}



/*============================================================================================================================
 * B-6: coro_bb_suspend — TT_SUSPEND Byrd box (coroutine wrapper)
 *
 * Wraps existing ucontext coroutine machinery from scrip.c.
 * The zeta pointer carries an opaque Icn_coro_entry* already set up by the caller.
 * α: start (fresh call) — swapcontext into coroutine.
 * β: resume — swapcontext into coroutine again.
 * ω: coroutine set exhausted=1.
 *
 * This box does NOT own the coroutine — coro_eval (B-8) wires it up.
 * The zeta is cast to coro_t which the broker caller populates.
 *============================================================================================================================*/

DESCR_t coro_bb_suspend(void *zeta, int entry) {
    coro_t *z = (coro_t *)zeta;
    /* exhausted+yielded_returned: truly done, return ω */
    if (z->exhausted && z->yielded_returned) return FAILDESCR;
    if (entry == α && !z->started) {
        /* First α: set up and enter the coroutine */
        z->started = 1;
        getcontext(&z->gen_ctx);
        /* IB-10 post: use CORO_STACK_SZ (now 1MB) and install a guard page at
         * the bottom of the malloc'd stack so overflow gives a clean SIGSEGV
         * rather than silently corrupting adjacent heap/mmap regions. */
        { static long _pg = 0; if (!_pg) _pg = sysconf(_SC_PAGESIZE);
          mprotect(z->stack, (size_t)_pg, PROT_NONE); }
        z->gen_ctx.uc_stack.ss_sp   = z->stack;
        z->gen_ctx.uc_stack.ss_size = CORO_STACK_SZ;
        z->gen_ctx.uc_link          = NULL;
        /* RK-21: if using gather trampoline, pass ss via the static staging pointer
         * (makecontext cannot pass pointer args portably on x86-64). */
        if (z->trampoline == gather_trampoline)
            gather_trampoline_ss = z;
        makecontext(&z->gen_ctx, z->trampoline, 0);
        swapcontext(&z->caller_ctx, &z->gen_ctx);
    } else if (!z->exhausted) {
        /* β or α-after-started: resume only if not exhausted */
        swapcontext(&z->caller_ctx, &z->gen_ctx);
    } else {
        /* exhausted but not yet returned — fall through to return yielded */
    }
    if (z->exhausted) {
        if (IS_FAIL_fn(z->yielded)) return FAILDESCR;   /* proc failed — ω immediately */
        z->yielded_returned = 1;
        return z->yielded;
    }
    return z->yielded;
}

/*============================================================================================================================
 * B-8: coro_bb_bal — bal(c1,c2,c3) generator Byrd box
 *
 * Walks scan subject from current &pos, yields 1-based positions where chars in c1
 * appear at nesting depth 0 w.r.t. c2/c3 open/close delimiters.
 *   α: scan from start_pos to endp; yield first match.
 *   β: resume from one past last match; yield next.
 *   ω: no more matches.
 *============================================================================================================================*/

DESCR_t coro_bb_bal(void *zeta, int entry) {
    icn_bal_state_t *z = (icn_bal_state_t *)zeta;
    if (entry == α) z->pos = (z->pos > 0 ? z->pos : 0); /* already set at construction */
    int p = z->pos, depth = 0;
    while (p < z->endp && p < z->slen) {
        char ch = z->s[p];
        if (strchr(z->c2, ch)) depth++;
        else if (strchr(z->c3, ch) && depth > 0) depth--;
        else if (depth == 0 && strchr(z->c1, ch)) {
            z->pos = p + 1;   /* β resumes from p+1 (next char after match) */
            return INTVAL((long)(p + 1));
        }
        p++;
    }
    return FAILDESCR;
}

/*============================================================================================================================
 * B-7: coro_bb_find — find() generator Byrd box
 *
 * State: needle, haystack, pos (byte offset into haystack, 0-based).
 *   α: pos = 0, find first match.
 *   β: advance past last match, find next.
 *   returns 1-based position of match, or ω.
 *============================================================================================================================*/

DESCR_t coro_bb_find(void *zeta, int entry) {
    icn_find_state_t *z = (icn_find_state_t *)zeta;
    if (entry == α) z->next = z->hay;
    const char *hit = strstr(z->next, z->needle);
    if (!hit) return FAILDESCR;
    long pos1 = (long)(hit - z->hay) + 1;   /* 1-based */
    z->next = hit + (z->nlen > 0 ? z->nlen : 1);
    return (DESCR_t){ .v = DT_I, .i = pos1 };
}

/*============================================================================================================================
 * coro_bb_find_subj — find(needle, scan_subject): drive subject generator, exhaust
 * find positions for each subject before advancing to the next subject.
 *   α/β: advance within current subject first; when exhausted, pull next subject.
 *============================================================================================================================*/
DESCR_t coro_bb_find_subj(void *zeta, int entry) {
    icn_find_gen_subj_t *z = (icn_find_gen_subj_t *)zeta;
    for (;;) {
        if (z->hay) {
            /* Try to find next hit in current subject */
            const char *hit = strstr(z->next, z->needle);
            if (hit) {
                long pos1 = (long)(hit - z->hay) + 1;
                z->next = hit + (z->nlen > 0 ? z->nlen : 1);
                return (DESCR_t){ .v = DT_I, .i = pos1 };
            }
        }
        /* Current subject exhausted — advance subject generator */
        DESCR_t sv = z->subj_gen.fn(z->subj_gen.ζ, z->subj_entry);
        z->subj_entry = β;
        if (IS_FAIL_fn(sv)) return FAILDESCR;  /* all subjects done */
        const char *s = sv.s ? sv.s : (sv.v == DT_SNUL ? "" : "");
        z->hay  = s;
        z->next = s;
    }
}

/*============================================================================================================================
 * coro_bb_upto_subj — upto(cset, scan_subject): drive subject generator, yield
 * positions of chars in cset for each subject before advancing.
 *============================================================================================================================*/
DESCR_t coro_bb_upto_subj(void *zeta, int entry) {
    icn_upto_gen_subj_t *z = (icn_upto_gen_subj_t *)zeta;
    for (;;) {
        if (z->hay) {
            /* Scan forward from current pos */
            while (z->pos < z->slen) {
                unsigned char c = (unsigned char)z->hay[z->pos];
                z->pos++;
                /* Check if c is in cset — byte-by-byte scan (8-bit safe) */
                int in_cset = 0;
                if (z->cset) {
                    for (const char *p = z->cset; *p; p++) {
                        if ((unsigned char)*p == c) { in_cset = 1; break; }
                    }
                }
                if (in_cset) return INTVAL((long)z->pos);  /* 1-based, already incremented */
            }
        }
        /* Current subject exhausted — advance subject generator */
        DESCR_t sv = z->subj_gen.fn(z->subj_gen.ζ, z->subj_entry);
        z->subj_entry = β;
        if (IS_FAIL_fn(sv)) return FAILDESCR;
        const char *s = sv.s ? sv.s : "";
        z->hay  = s;
        z->slen = (int)strlen(s);
        z->pos  = 0;
    }
}

/*============================================================================================================================
 * coro_bb_binop — IC-2a: generative binary operator Byrd box
 *
 * Protocol (JCON irgen.icn §4.3, funcs-set):
 *   α: pump left α → get left_val; pump right α → get right_val; apply op.
 *   β (arithmetic):  resume right β; if right ω → resume left β, reset right α.
 *   β (relational):  resume right β; if right ω → resume left β, reset right α.
 *   On relational comparison failure: retry right β (goal-directed).
 *
 * left/right are bb_node_t generators (may be oneshot wrappers for scalar operands).
 *============================================================================================================================*/

static DESCR_t icn_binop_apply(IcnBinopKind op, DESCR_t lv, DESCR_t rv, int *rel_fail) {
    *rel_fail = 0;
    if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
    /* For relops and concat, use real-aware comparison when either operand is real */
    int either_real = (IS_REAL_fn(lv) || IS_REAL_fn(rv));
    double ld = IS_REAL_fn(lv) ? lv.r : (double)(IS_INT_fn(lv) ? lv.i : 0);
    double rd = IS_REAL_fn(rv) ? rv.r : (double)(IS_INT_fn(rv) ? rv.i : 0);
    long   li = IS_INT_fn(lv) ? lv.i : (long)lv.r;
    long   ri = IS_INT_fn(rv) ? rv.i : (long)rv.r;
    switch (op) {
        case ICN_BINOP_ADD:
            return either_real ? (DESCR_t){.v=DT_R,.r=ld+rd} : INTVAL(li + ri);
        case ICN_BINOP_SUB:
            return either_real ? (DESCR_t){.v=DT_R,.r=ld-rd} : INTVAL(li - ri);
        case ICN_BINOP_MUL:
            return either_real ? (DESCR_t){.v=DT_R,.r=ld*rd} : INTVAL(li * ri);
        case ICN_BINOP_DIV:
            if (either_real) return rd != 0.0 ? (DESCR_t){.v=DT_R,.r=ld/rd} : FAILDESCR;
            return ri ? INTVAL(li / ri) : FAILDESCR;
        case ICN_BINOP_MOD:    return ri ? INTVAL(li % ri) : FAILDESCR;
        /* Relops: return the right operand as-is (Icon goal-directed semantics) */
        case ICN_BINOP_LT:  *rel_fail = !(either_real ? ld <  rd : li <  ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_LE:  *rel_fail = !(either_real ? ld <= rd : li <= ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_GT:  *rel_fail = !(either_real ? ld >  rd : li >  ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_GE:  *rel_fail = !(either_real ? ld >= rd : li >= ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_EQ:  *rel_fail = !(either_real ? ld == rd : li == ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_NE:  *rel_fail = !(either_real ? ld != rd : li != ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_CONCAT: {
            /* D-2/D-3 RS-6: coerce via descr_to_str_icn() — fixes %.15g real
             * formatting (was not round-trip for 16/17-digit values) and
             * eliminates code duplication with icn_runtime.c coerce sites. */
            DESCR_t ls_d = descr_to_str_icn(lv);
            DESCR_t rs_d = descr_to_str_icn(rv);
            if (IS_FAIL_fn(ls_d) || IS_FAIL_fn(rs_d)) return FAILDESCR;
            const char *ls = ls_d.s ? ls_d.s : "";
            const char *rs = rs_d.s ? rs_d.s : "";
            size_t ll = ls_d.slen > 0 ? (size_t)ls_d.slen : strlen(ls);
            size_t rl = rs_d.slen > 0 ? (size_t)rs_d.slen : strlen(rs);
            char *buf = GC_malloc(ll + rl + 1);
            memcpy(buf, ls, ll); memcpy(buf + ll, rs, rl); buf[ll + rl] = '\0';
            return (DESCR_t){ .v = DT_S, .slen = (int)(ll + rl), .s = buf };
        }
        default: return FAILDESCR;
    }
}

DESCR_t coro_bb_binop(void *zeta, int entry) {
    icn_binop_gen_state_t *z = (icn_binop_gen_state_t *)zeta;

    if (entry == α) {
        /* Fresh: pump left α, then right α */
        z->left_val  = z->left.fn(z->left.ζ, α);
        if (IS_FAIL_fn(z->left_val)) return FAILDESCR;
        z->right_val = z->right.fn(z->right.ζ, α);
        if (IS_FAIL_fn(z->right_val)) return FAILDESCR;
        z->phase = 2;
    } else {
        /* β: try to advance right first */
        for (;;) {
            DESCR_t rv = z->right.fn(z->right.ζ, β);
            if (!IS_FAIL_fn(rv)) { z->right_val = rv; break; }
            /* right exhausted — advance left, reset right */
            DESCR_t lv = z->left.fn(z->left.ζ, β);
            if (IS_FAIL_fn(lv)) return FAILDESCR;   /* both exhausted */
            z->left_val  = lv;
            z->right_val = z->right.fn(z->right.ζ, α);
            if (!IS_FAIL_fn(z->right_val)) break;
            /* right empty on reset — try next left */
        }
    }

    /* Apply op; for relational failure, retry right (goal-directed) */
    for (;;) {
        int rel_fail = 0;
        DESCR_t result = icn_binop_apply(z->op, z->left_val, z->right_val, &rel_fail);
        if (!IS_FAIL_fn(result)) return result;
        if (!rel_fail) return FAILDESCR;   /* arithmetic error (div by zero etc.) */
        /* Relational failure: retry right β (JCON §4.3 goal-directed) */
        DESCR_t rv = z->right.fn(z->right.ζ, β);
        if (!IS_FAIL_fn(rv)) { z->right_val = rv; continue; }
        /* right exhausted — advance left, reset right */
        DESCR_t lv = z->left.fn(z->left.ζ, β);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        z->left_val  = lv;
        z->right_val = z->right.fn(z->right.ζ, α);
        if (IS_FAIL_fn(z->right_val)) return FAILDESCR;
    }
}

/*============================================================================================================================
 * coro_bb_alternate — IC-2a: TT_ALTERNATE Byrd box
 *
 * JCON irgen.icn ir_a_Alt (binary case):
 *   α: pump gen[0] α; if γ → return. If ω → switch which=1, pump gen[1] α.
 *   β: pump current gen β; if ω and which==0 → switch to gen[1] α.
 *============================================================================================================================*/

DESCR_t coro_bb_alternate(void *zeta, int entry) {
    icn_alternate_state_t *z = (icn_alternate_state_t *)zeta;
    if (entry == α) {
        z->which = 0;
        DESCR_t v = z->gen[0].fn(z->gen[0].ζ, α);
        if (!IS_FAIL_fn(v)) return v;
        z->which = 1;
        return z->gen[1].fn(z->gen[1].ζ, α);
    }
    /* β */
    DESCR_t v = z->gen[z->which].fn(z->gen[z->which].ζ, β);
    if (!IS_FAIL_fn(v)) return v;
    if (z->which == 0) {
        z->which = 1;
        return z->gen[1].fn(z->gen[1].ζ, α);
    }
    return FAILDESCR;
}

/* coro_eval — implemented in scrip.c where interp_eval and proc tables are visible. */

/*============================================================================================================================
 * Unit tests: B-2 constant box, B-3 coro_bb_to, B-4 coro_bb_to_by, B-5 coro_bb_iterate, B-7 coro_bb_find
 *============================================================================================================================*/
#ifdef ICON_GEN_UNIT_TEST

typedef struct { DESCR_t value; int fired; } const_box_state_t;

static DESCR_t const_box_fn(void *zeta, int entry) {
    const_box_state_t *z = (const_box_state_t *)zeta;
    if (entry == α && !z->fired) { z->fired = 1; return z->value; }
    return FAILDESCR;
}

typedef struct { long *vals; int n; int cap; } collector_t;
static void collect_int(DESCR_t val, void *arg) {
    collector_t *c = (collector_t *)arg;
    if (c->n < c->cap) c->vals[c->n++] = val.i;
}

typedef struct { char *buf; int idx; int cap; } str_collector_t;
static void collect_str(DESCR_t val, void *arg) {
    str_collector_t *c = (str_collector_t *)arg;
    if (c->idx < c->cap && val.s) c->buf[c->idx++] = val.s[0];
}

static int test_fail = 0;
#define ASSERT(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); test_fail=1; } } while(0)

int main(void) {
    /* B-2: constant box → 1 tick, value=42 */
    {
        const_box_state_t *s = calloc(1, sizeof(*s));
        s->value = (DESCR_t){ .v = DT_I, .i = 42 };
        bb_node_t gen = { const_box_fn, s, 0 };
        long vals[8]; collector_t c = { vals, 0, 8 };
        int ticks = bb_broker(gen, BB_PUMP, collect_int, &c);
        ASSERT(ticks == 1, "B-2: ticks==1");
        ASSERT(c.n == 1 && vals[0] == 42, "B-2: value==42");
        free(s);
    }

    /* B-3: coro_bb_to (1 to 5) → 1,2,3,4,5 */
    {
        icn_to_state_t *s = calloc(1, sizeof(*s));
        s->lo = 1; s->hi = 5;
        bb_node_t gen = { coro_bb_to, s, 0 };
        long vals[8]; collector_t c = { vals, 0, 8 };
        int ticks = bb_broker(gen, BB_PUMP, collect_int, &c);
        ASSERT(ticks == 5, "B-3: ticks==5");
        ASSERT(c.n==5 && vals[0]==1 && vals[4]==5, "B-3: values 1..5");
        free(s);
    }

    /* B-4: coro_bb_to_by (1 to 10 by 2) → 1,3,5,7,9 */
    {
        icn_to_by_state_t *s = calloc(1, sizeof(*s));
        s->lo = 1; s->hi = 10; s->step = 2;
        bb_node_t gen = { coro_bb_to_by, s, 0 };
        long vals[8]; collector_t c = { vals, 0, 8 };
        int ticks = bb_broker(gen, BB_PUMP, collect_int, &c);
        ASSERT(ticks == 5, "B-4: ticks==5");
        ASSERT(vals[0]==1 && vals[1]==3 && vals[4]==9, "B-4: values 1,3,5,7,9");
        free(s);
    }

    /* B-5: coro_bb_iterate !("abc") → 'a','b','c' */
    {
        icn_iterate_state_t *s = calloc(1, sizeof(*s));
        s->str = "abc"; s->len = 3;
        bb_node_t gen = { coro_bb_iterate, s, 0 };
        char got[4] = {0};
        str_collector_t sc = { got, 0, 3 };
        bb_broker(gen, BB_PUMP, collect_str, &sc);
        ASSERT(strcmp(got, "abc") == 0, "B-5: iterate abc");
        free(s);
    }

    /* B-7: coro_bb_find find("is","this is it") → 3,6 */
    {
        icn_find_state_t *s = calloc(1, sizeof(*s));
        s->needle = "is"; s->hay = "this is it"; s->nlen = 2; s->next = s->hay;
        bb_node_t gen = { coro_bb_find, s, 0 };
        long vals[8]; collector_t c = { vals, 0, 8 };
        int ticks = bb_broker(gen, BB_PUMP, collect_int, &c);
        ASSERT(ticks == 2, "B-7: find ticks==2");
        ASSERT(vals[0]==3 && vals[1]==6, "B-7: find positions 3,6");
        free(s);
    }

    if (!test_fail) printf("PASS: all box gates\n");
    return test_fail;
}

#endif /* ICON_GEN_UNIT_TEST */

icn_scan_gen_state_t *icon_scan_gen_new(void) { return calloc(1, sizeof(icn_scan_gen_state_t)); }
coro_t               *icon_suspend_new(void)  { return calloc(1, sizeof(coro_t)); }
