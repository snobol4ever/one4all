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

#include "../../frontend/icon/icon_gen.h"
#include "../ast/ast.h"            /* tree_t, tree_e, TT_TO, TT_TO_BY, TT_ITERATE, TT_SUSPEND, TT_FNC */
#include "../../runtime/common/coerce.h"  /* descr_to_str_icn (D-1/D-2 RS-6) */
#include "../../runtime/interp/icn_runtime.h"  /* CORO_STACK_SZ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*============================================================================================================================
 * B-3: icn_bb_to — TT_TO Byrd box  (i to j)
 *
 * State: lo, hi, cur.
 *   α: cur = lo; if cur > hi → ω; else return integer cur (γ).
 *   β: cur++; if cur > hi → ω; else return integer cur (γ).
 *============================================================================================================================*/

DESCR_t icn_bb_to(void *zeta, int entry) {
    icn_to_state_t *z = (icn_to_state_t *)zeta;
    if (entry == α) z->cur = z->lo;
    else            z->cur++;
    if (z->cur > z->hi) return FAILDESCR;
    return (DESCR_t){ .v = DT_I, .i = z->cur };
}

/*============================================================================================================================
 * icn_bb_to_nested — (lo_gen) to (hi_gen) cross-product Byrd box
 *
 * JCON irgen.icn ir_a_To nested case: when lo or hi is itself a generator,
 * pre-collect all values from each, then iterate outer lo × hi pairs,
 * yielding each inner lo_val..hi_val range in sequence.
 *
 * State pre-populated by icn_bb_build before returning this box.
 * α: li=0, hi2=0, cur=lo_vals[0]; step through inner range.
 * β: cur++; if cur > hi_vals[hi2]: hi2++; if hi2 >= nhi: li++, hi2=0; reset cur.
 * ω: li >= nlo.
 *============================================================================================================================*/

DESCR_t icn_bb_to_nested(void *zeta, int entry) {
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
 * B-4: icn_bb_to_by — TT_TO_BY Byrd box  (i to j by k)
 *
 * State: lo, hi, step, cur.
 *   α: cur = lo.
 *   β: cur += step.
 *   if step > 0: cur > hi → ω.   if step < 0: cur < hi → ω.
 *============================================================================================================================*/

DESCR_t icn_bb_to_by(void *zeta, int entry) {
    icn_to_by_state_t *z = (icn_to_by_state_t *)zeta;
    if (entry == α) z->cur = z->lo;
    else            z->cur += z->step;
    long step = z->step ? z->step : 1;
    if (step > 0 && z->cur > z->hi) return FAILDESCR;
    if (step < 0 && z->cur < z->hi) return FAILDESCR;
    return (DESCR_t){ .v = DT_I, .i = z->cur };
}

/* icn_bb_to_by_real — TT_TO_BY with real (float) step/bounds */
DESCR_t icn_bb_to_by_real(void *zeta, int entry) {
    icn_to_by_real_state_t *z = (icn_to_by_real_state_t *)zeta;
    if (entry == α) z->cur = z->lo;
    else            z->cur += z->step;
    double step = z->step != 0.0 ? z->step : 1.0;
    if (step > 0.0 && z->cur > z->hi + 1e-10) return FAILDESCR;
    if (step < 0.0 && z->cur < z->hi - 1e-10) return FAILDESCR;
    return (DESCR_t){ .v = DT_R, .r = z->cur };
}

/*============================================================================================================================
 * B-5: icn_bb_iterate — TT_ITERATE Byrd box  (!str, Icon char iteration)
 *
 * State: str, len, pos.
 *   α: pos = 0.  β: pos++.  ω: pos >= len.  γ: single-char string at pos.
 *============================================================================================================================*/

DESCR_t icn_bb_iterate(void *zeta, int entry) {
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
 * B-5b: icn_bb_tbl_iterate — TT_ITERATE Byrd box for DT_T tables  (!T yields values)
 *
 * State: tbl, bucket (0..TABLE_BUCKETS-1), entry (current TBPAIR_t*).
 *   α: bucket=0, entry=tbl->buckets[0].
 *   β: advance to next entry (or next non-empty bucket).
 *   ω: all buckets exhausted.
 *   γ: return entry->val.
 *============================================================================================================================*/

DESCR_t icn_bb_tbl_iterate(void *zeta, int entry) {
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
 * IC-5: icn_bb_list_iterate — TT_ITERATE Byrd box for DT_DATA icnlist  (!L yields elements)
 *   Holds the live list DT_DATA descriptor so it sees elements added by put() after box creation.
 *   α: reset pos=0, return elems[0].
 *   β: advance pos, return elems[pos].
 *   ω: pos >= n.
 *============================================================================================================================*/
DESCR_t icn_bb_list_iterate(void *zeta, int entry) {
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
 * IC-9 (2026-05-01): icn_bb_record_iterate — TT_ITERATE Byrd box for DT_DATA records (!R yields field values).
 *   Records are DT_DATA but NOT the icnlist shape (no "icn_type" tag of "list"); their structure lives in
 *   inst.u->type->nfields and inst.u->fields[i].  Iterates fields in declaration order.  Re-reads the live
 *   instance each tick so mutations made between ticks (rare; field-iteration usually one-pass) are visible.
 *   α: pos=0, return fields[0] (or fail if nfields==0).
 *   β: pos++, return fields[pos] (or fail when pos >= nfields).
 *============================================================================================================================*/
DESCR_t icn_bb_record_iterate(void *zeta, int entry) {
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
 * IC-6: icn_bb_tbl_key_iterate — key(T) generator: yields each key in table T
 *   Same bucket-walk as icn_bb_tbl_iterate but returns entry->key_descr instead of entry->val.
 *============================================================================================================================*/
DESCR_t icn_bb_tbl_key_iterate(void *zeta, int entry) {
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
 * B-8: icn_bb_bal — bal(c1,c2,c3) generator Byrd box
 *
 * Walks scan subject from current &pos, yields 1-based positions where chars in c1
 * appear at nesting depth 0 w.r.t. c2/c3 open/close delimiters.
 *   α: scan from start_pos to endp; yield first match.
 *   β: resume from one past last match; yield next.
 *   ω: no more matches.
 *============================================================================================================================*/

DESCR_t icn_bb_bal(void *zeta, int entry) {
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
 * B-7: icn_bb_find — find() generator Byrd box
 *
 * State: needle, haystack, pos (byte offset into haystack, 0-based).
 *   α: pos = 0, find first match.
 *   β: advance past last match, find next.
 *   returns 1-based position of match, or ω.
 *============================================================================================================================*/

DESCR_t icn_bb_find(void *zeta, int entry) {
    icn_find_state_t *z = (icn_find_state_t *)zeta;
    if (entry == α) z->next = z->hay;
    const char *hit = strstr(z->next, z->needle);
    if (!hit) return FAILDESCR;
    long pos1 = (long)(hit - z->hay) + 1;   /* 1-based */
    z->next = hit + (z->nlen > 0 ? z->nlen : 1);
    return (DESCR_t){ .v = DT_I, .i = pos1 };
}

/*============================================================================================================================
 * icn_bb_find_subj — find(needle, scan_subject): drive subject generator, exhaust
 * find positions for each subject before advancing to the next subject.
 *   α/β: advance within current subject first; when exhausted, pull next subject.
 *============================================================================================================================*/
DESCR_t icn_bb_find_subj(void *zeta, int entry) {
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
 * icn_bb_upto_subj — upto(cset, scan_subject): drive subject generator, yield
 * positions of chars in cset for each subject before advancing.
 *============================================================================================================================*/
DESCR_t icn_bb_upto_subj(void *zeta, int entry) {
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
 * icn_bb_binop — IC-2a: generative binary operator Byrd box
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

DESCR_t icn_bb_binop(void *zeta, int entry) {
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
 * icn_bb_alternate — IC-2a: TT_ALTERNATE Byrd box
 *
 * JCON irgen.icn ir_a_Alt (binary case):
 *   α: pump gen[0] α; if γ → return. If ω → switch which=1, pump gen[1] α.
 *   β: pump current gen β; if ω and which==0 → switch to gen[1] α.
 *============================================================================================================================*/

DESCR_t icn_bb_alternate(void *zeta, int entry) {
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

/* icn_bb_build — implemented in scrip.c where interp_eval and proc tables are visible. */

/*============================================================================================================================
 * Unit tests: B-2 constant box, B-3 icn_bb_to, B-4 icn_bb_to_by, B-5 icn_bb_iterate, B-7 icn_bb_find
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

    /* B-3: icn_bb_to (1 to 5) → 1,2,3,4,5 */
    {
        icn_to_state_t *s = calloc(1, sizeof(*s));
        s->lo = 1; s->hi = 5;
        bb_node_t gen = { icn_bb_to, s, 0 };
        long vals[8]; collector_t c = { vals, 0, 8 };
        int ticks = bb_broker(gen, BB_PUMP, collect_int, &c);
        ASSERT(ticks == 5, "B-3: ticks==5");
        ASSERT(c.n==5 && vals[0]==1 && vals[4]==5, "B-3: values 1..5");
        free(s);
    }

    /* B-4: icn_bb_to_by (1 to 10 by 2) → 1,3,5,7,9 */
    {
        icn_to_by_state_t *s = calloc(1, sizeof(*s));
        s->lo = 1; s->hi = 10; s->step = 2;
        bb_node_t gen = { icn_bb_to_by, s, 0 };
        long vals[8]; collector_t c = { vals, 0, 8 };
        int ticks = bb_broker(gen, BB_PUMP, collect_int, &c);
        ASSERT(ticks == 5, "B-4: ticks==5");
        ASSERT(vals[0]==1 && vals[1]==3 && vals[4]==9, "B-4: values 1,3,5,7,9");
        free(s);
    }

    /* B-5: icn_bb_iterate !("abc") → 'a','b','c' */
    {
        icn_iterate_state_t *s = calloc(1, sizeof(*s));
        s->str = "abc"; s->len = 3;
        bb_node_t gen = { icn_bb_iterate, s, 0 };
        char got[4] = {0};
        str_collector_t sc = { got, 0, 3 };
        bb_broker(gen, BB_PUMP, collect_str, &sc);
        ASSERT(strcmp(got, "abc") == 0, "B-5: iterate abc");
        free(s);
    }

    /* B-7: icn_bb_find find("is","this is it") → 3,6 */
    {
        icn_find_state_t *s = calloc(1, sizeof(*s));
        s->needle = "is"; s->hay = "this is it"; s->nlen = 2; s->next = s->hay;
        bb_node_t gen = { icn_bb_find, s, 0 };
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
#include "../../runtime/interp/icn_value.h"
#include "../../runtime/interp/icn_stmt.h"
#include "../../runtime/x86/snobol4.h"

/* Forward declarations from icn_runtime.c */
typedef struct { tree_t *expr; } icn_lazy_state_t;
extern DESCR_t icn_lazy_box(void *zeta, int entry);
extern int is_suspendable(tree_t *e);


/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-18: icn_bb_not -- ir_a_Not
 *
 * JCON wiring:
 *   start -> expr.start
 *   expr.success -> failure   (if E succeeded, not-E fails)
 *   expr.failure -> push null, success  (if E failed, not-E succeeds with null)
 *   resume -> failure (not-E is one-shot, bounded="always bounded" in JCON)
 *
 * State: expr to evaluate (one-shot, non-generative child).
 * alpha: evaluate child; if child fails -> NULVCL; if child succeeds -> FAILDESCR.
 * beta:  FAILDESCR (not-E is always bounded/one-shot).
 *--------------------------------------------------------------------------------------------------------------------------*/

DESCR_t icn_bb_not(void *zeta, int entry) {
    if (entry != 0) return FAILDESCR;  /* one-shot: resume always fails */
    icn_not_state_t *z = (icn_not_state_t *)zeta;
    if (!z->expr) return NULVCL;
    DESCR_t v = bb_eval_value(z->expr);
    /* expr succeeded -> not fails; expr failed -> not succeeds with null */
    return IS_FAIL_fn(v) ? NULVCL : FAILDESCR;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-18: icn_bb_repalt -- ir_a_RepAlt  (|E  repeated alternation)
 *
 * JCON wiring (unbounded):
 *   start: save failure label = ir.failure; goto expr.start
 *   expr.success: save failure label = ir.start; goto ir.success
 *   expr.failure: IndirectGoto(t)  -- i.e. if expr exhausted first time -> omega;
 *                                     after first success, restart from ir.start
 *   resume -> expr.resume
 *
 * Simplified: pump sub-box; on exhaustion restart from alpha.
 * Semantics: keep generating E over and over, never exhausting until...
 * Actually |E generates all of E then wraps: this is an infinite generator.
 * For our purposes: restart inner box on exhaustion.
 *
 * State: inner box, started flag.
 *--------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_repalt(void *zeta, int entry) {
    icn_repalt_state_t *z = (icn_repalt_state_t *)zeta;
    for (;;) {
        int port = (z->started) ? 1 : 0;
        if (!z->started) {
            z->inner   = icn_bb_build(z->expr);
            z->started = 1;
        }
        DESCR_t v = z->inner.fn(z->inner.ζ, port);
        if (!IS_FAIL_fn(v)) { z->ever_succeeded = 1; return v; }
        /* inner exhausted */
        if (!z->ever_succeeded) return FAILDESCR;  /* never succeeded -> omega */
        /* restart inner from alpha */
        z->inner   = icn_bb_build(z->expr);
        z->started = 1;  /* but pump with alpha next iteration */
        /* rebuild and pump alpha */
        v = z->inner.fn(z->inner.ζ, 0);
        if (!IS_FAIL_fn(v)) return v;
        return FAILDESCR;  /* restarted and immediately exhausted */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-20: icn_bb_while_gen -- ir_a_While  (while E do B, used as generator)
 *
 * JCON wiring:
 *   start -> expr.start
 *   expr.success -> body.start
 *   expr.failure -> ir.failure (omega)
 *   body.success -> expr.start  (next iteration)
 *   body.failure -> expr.start  (body failing just continues loop)
 *   resume -> IndirectGoto(continue)  (for break-driven resume)
 *
 * As a BB: generates the value of body on each iteration where expr succeeds.
 * State: expr_node, body_node. On each pump: test expr, run body.
 *--------------------------------------------------------------------------------------------------------------------------*/

DESCR_t icn_bb_while_gen(void *zeta, int entry) {
    icn_while_state_t *z = (icn_while_state_t *)zeta;
    /* Both alpha and beta: test expr, if succeeds run body, return body value.
     * Loop control (next/break) is handled at statement level. */
    for (;;) {
        DESCR_t test = z->expr ? bb_eval_value(z->expr) : FAILDESCR;
        if (IS_FAIL_fn(test)) return FAILDESCR;  /* expr failed -> loop done */
        DESCR_t bval = z->body ? bb_eval_value(z->body) : NULVCL;
        /* Body value (success or fail) — yield and continue */
        if (!IS_FAIL_fn(bval)) return bval;
        /* body failed -> continue to next iteration (not omega) */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-20: icn_bb_until_gen -- ir_a_Until  (until E do B, used as generator)
 *
 * JCON wiring:
 *   expr.success -> ir.failure  (expr true -> stop)
 *   expr.failure -> body.start  (expr false -> run body)
 *   body.success/failure -> expr.start  (keep looping)
 *--------------------------------------------------------------------------------------------------------------------------*/

DESCR_t icn_bb_until_gen(void *zeta, int entry) {
    icn_until_state_t *z = (icn_until_state_t *)zeta;
    for (;;) {
        DESCR_t test = z->expr ? bb_eval_value(z->expr) : NULVCL;
        if (!IS_FAIL_fn(test)) return FAILDESCR;  /* expr succeeded -> loop done */
        DESCR_t bval = z->body ? bb_eval_value(z->body) : NULVCL;
        if (!IS_FAIL_fn(bval)) return bval;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-20: icn_bb_repeat_gen -- ir_a_Repeat  (repeat B, used as generator)
 *
 * JCON wiring: unconditional loop. body.success/failure both -> ir.start.
 * As a BB: infinite generator yielding body values.
 *--------------------------------------------------------------------------------------------------------------------------*/

DESCR_t icn_bb_repeat_gen(void *zeta, int entry) {
    icn_repeat_state_t *z = (icn_repeat_state_t *)zeta;
    for (;;) {
        DESCR_t bval = z->body ? bb_eval_value(z->body) : NULVCL;
        if (!IS_FAIL_fn(bval)) return bval;
        /* body failed -> continue (repeat never stops on body failure) */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-21: icn_bb_case_gen -- ir_a_Case  (case E of { C: B ... } used as generator)
 *
 * JCON wiring:
 *   eval E (bounded); compare === each clause expr; on match pump clause body.
 *   On body exhaustion try next clause. Default if no clause matches.
 *
 * State: discriminant value, current clause index, body_box, n_clauses.
 *--------------------------------------------------------------------------------------------------------------------------*/

/* descr equality: mirrors === (TT_IDENTICAL) */
static int descr_identical(DESCR_t a, DESCR_t b) {
    if (a.v != b.v) return 0;
    if (a.v == DT_I) return a.i == b.i;
    if (a.v == DT_R) return a.r == b.r;
    if (a.v == DT_S || a.v == DT_SNUL) {
        const char *as = VARVAL_fn(a), *bs = VARVAL_fn(b);
        if (!as && !bs) return 1;
        if (!as || !bs) return 0;
        return strcmp(as, bs) == 0;
    }
    return a.ptr == b.ptr;
}

DESCR_t icn_bb_case_gen(void *zeta, int entry) {
    icn_case_state_t *z = (icn_case_state_t *)zeta;
    /* If already in a body, pump beta first */
    if (z->body_started && entry == 1) {
        DESCR_t v = z->body_box.fn(z->body_box.ζ, 1);
        if (!IS_FAIL_fn(v)) return v;
        z->body_started = 0;
        z->cur_clause++;  /* try next clause */
    }
    /* Find matching clause */
    for (; z->cur_clause < z->n_clauses; z->cur_clause++) {
        tree_t *ce = z->clause_exprs[z->cur_clause];
        if (!ce) continue;
        DESCR_t cv = bb_eval_value(ce);
        if (IS_FAIL_fn(cv)) continue;
        if (!descr_identical(z->disc, cv)) continue;
        /* Match -- pump body */
        tree_t *cb = z->clause_bodies[z->cur_clause];
        if (!cb) return NULVCL;
        z->body_box     = icn_bb_build(cb);
        z->body_started = 1;
        DESCR_t v = z->body_box.fn(z->body_box.ζ, 0);
        if (!IS_FAIL_fn(v)) return v;
        z->body_started = 0;
        /* body immediately exhausted, try next clause */
    }
    /* default */
    if (z->dflt) {
        if (!z->body_started) {
            z->body_box     = icn_bb_build(z->dflt);
            z->body_started = 1;
            DESCR_t v = z->body_box.fn(z->body_box.ζ, 0);
            if (!IS_FAIL_fn(v)) return v;
        }
    }
    return FAILDESCR;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-22: LOOP_NEXT / LOOP_BREAK in BB context
 *
 * next -> sets FRAME.loop_next=1; break E -> sets FRAME.loop_break=1, stashes E.
 * These are handled at statement level by bb_exec_stmt's default clause.
 * No separate BB needed -- they propagate via FRAME flags.
 *--------------------------------------------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-26: icn_bb_makelist_gen -- ir_a_ListConstructor with generative elements
 *
 * JCON wiring: evaluate each element left-to-right (all bounded for list construction).
 * The list constructor itself is one-shot (bounded) -- it produces one list.
 * Elements that are generators produce the cross-product, but for simplicity
 * and fidelity with ir_a_ListConstructor: we build the list eagerly, elements
 * are evaluated left-to-right, each one-shot.
 *
 * alpha: eval all children, build list, return it.
 * beta:  FAILDESCR (one-shot).
 *--------------------------------------------------------------------------------------------------------------------------*/
/* Note: non-generative list constructor is already handled in bb_eval_value
 * (TT_MAKELIST case). This BB is wired for when is_suspendable detects a
 * generative child -- we drive the cross-product via existing icn_bb_build
 * machinery by pumping the generative elements. For now: eager eval, one list. */

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-27: icn_bb_compound_gen -- ir_a_Compound  ((E1; E2; ...; EN) generative)
 *
 * JCON wiring:
 *   E1..E(N-1): evaluate for side effects (bounded); if any fail, continue.
 *   EN: pump as generator, yield its values.
 *   start -> L[1].start; resume -> L[N].resume
 *   L[i].success -> L[i+1].start  (for i < N)
 *   L[i].failure -> L[i+1].start  (failure of non-last also continues)
 *   L[N].success -> ir.success; L[N].failure -> ir.failure
 *--------------------------------------------------------------------------------------------------------------------------*/

DESCR_t icn_bb_compound_gen(void *zeta, int entry) {
    icn_compound_state_t *z = (icn_compound_state_t *)zeta;
    if (entry == 0 || !z->started) {
        /* eval all but last for side effects */
        for (int i = 0; i < z->n - 1; i++)
            if (z->children[i]) bb_eval_value(z->children[i]);
        if (z->n <= 0 || !z->children[z->n - 1]) return FAILDESCR;
        z->last_box = icn_bb_build(z->children[z->n - 1]);
        z->started  = 1;
        return z->last_box.fn(z->last_box.ζ, 0);
    }
    return z->last_box.fn(z->last_box.ζ, 1);
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-28: icn_bb_field_gen -- ir_a_Field with generative object
 *
 * JCON: val = eval(object generator each tick); result = val.field
 * alpha/beta: pump object_gen, evaluate field on each result.
 *--------------------------------------------------------------------------------------------------------------------------*/

DESCR_t icn_bb_field_gen(void *zeta, int entry) {
    icn_field_gen_state_t *z = (icn_field_gen_state_t *)zeta;
    DESCR_t obj = z->obj_gen.fn(z->obj_gen.ζ, entry);
    if (IS_FAIL_fn(obj)) return FAILDESCR;
    if (!z->field) return obj;
    return FIELD_GET_fn(obj, z->field);
}

/*============================================================================================================================
 * icn_bb_build additions -- wire new BBs into the dispatch.
 * Call icn_bb_build_missing(e) from icn_bb_build's fallback.
 *============================================================================================================================*/

bb_node_t icn_bb_build_missing_base(tree_t *e) {
    if (!e) { icn_lazy_state_t *z = calloc(1, sizeof(*z)); return (bb_node_t){ icn_lazy_box, z, 0 }; }

    /* ir_a_Not */
    if (e->t == TT_NOT) {
        icn_not_state_t *z = calloc(1, sizeof(*z));
        z->expr = (e->n >= 1) ? e->c[0] : NULL;
        return (bb_node_t){ icn_bb_not, z, 0 };
    }

    /* ir_a_While (as generator) */
    if (e->t == TT_WHILE) {
        icn_while_state_t *z = calloc(1, sizeof(*z));
        z->expr = (e->n >= 1) ? e->c[0] : NULL;
        z->body = (e->n >= 2) ? e->c[1] : NULL;
        return (bb_node_t){ icn_bb_while_gen, z, 0 };
    }

    /* ir_a_Until (as generator) */
    if (e->t == TT_UNTIL) {
        icn_until_state_t *z = calloc(1, sizeof(*z));
        z->expr = (e->n >= 1) ? e->c[0] : NULL;
        z->body = (e->n >= 2) ? e->c[1] : NULL;
        return (bb_node_t){ icn_bb_until_gen, z, 0 };
    }

    /* ir_a_Repeat (as generator) */
    if (e->t == TT_REPEAT) {
        icn_repeat_state_t *z = calloc(1, sizeof(*z));
        z->body = (e->n >= 1) ? e->c[0] : NULL;
        return (bb_node_t){ icn_bb_repeat_gen, z, 0 };
    }

    /* ir_a_Case (as generator) */
    if (e->t == TT_CASE) {
        icn_case_state_t *z = calloc(1, sizeof(*z));
        /* c[0] = discriminant; c[1..2n] = clause_expr, clause_body pairs; last = default */
        z->disc = e->n >= 1 ? bb_eval_value(e->c[0]) : NULVCL;
        z->cur_clause   = 0;
        z->body_started = 0;
        int i = 1;
        while (i + 1 < e->n && z->n_clauses < ICN_CASE_MAX) {
            z->clause_exprs[z->n_clauses]  = e->c[i];
            z->clause_bodies[z->n_clauses] = e->c[i + 1];
            z->n_clauses++;
            i += 2;
        }
        z->dflt = (i < e->n) ? e->c[i] : NULL;
        return (bb_node_t){ icn_bb_case_gen, z, 0 };
    }

    /* ir_a_Compound (generative -- last child is generator) */
    if (e->t == TT_SEQ_EXPR) {
        icn_compound_state_t *z = calloc(1, sizeof(*z));
        z->n = 0;
        for (int i = 0; i < e->n && z->n < ICN_COMPOUND_MAX; i++)
            z->children[z->n++] = e->c[i];
        return (bb_node_t){ icn_bb_compound_gen, z, 0 };
    }

    /* ir_a_Field with generative object */
    if (e->t == TT_FIELD && e->n >= 1 && is_suspendable(e->c[0])) {
        icn_field_gen_state_t *z = calloc(1, sizeof(*z));
        z->obj_gen = icn_bb_build(e->c[0]);
        z->field   = e->v.sval;
        return (bb_node_t){ icn_bb_field_gen, z, 0 };
    }

    /* ir_a_RepAlt -- TT_ALTERNATE with single child wrapping repeat semantics.
     * In our AST, |E is represented as TT_ALTERNATE with one child.
     * Standard TT_ALTERNATE with 2+ children is handled by icn_bb_alternate. */
    /* (covered by existing icn_bb_alternate for n>=2; n==1 is |E) */
    if (e->t == TT_ALTERNATE && e->n == 1) {
        icn_repalt_state_t *z = calloc(1, sizeof(*z));
        z->expr          = e->c[0];
        z->started       = 0;
        z->ever_succeeded = 0;
        return (bb_node_t){ icn_bb_repalt, z, 0 };
    }

    /* fallback: lazy box */
    icn_lazy_state_t *z = calloc(1, sizeof(*z));
    z->expr = e;
    return (bb_node_t){ icn_lazy_box, z, 0 };
}

/*============================================================================================================================
 * icn_bb_proc_call -- pure BB Icon procedure executor (replaces icn_bb_suspend + swapcontext)
 *
 * ir_a_Suspend wiring (JCON):
 *   start  → expr.start   (pump the expr generator α)
 *   expr.γ → yield value; β comes back → expr.β (resume generator)
 *   expr.ω → proc failure (no more values from this suspend)
 *
 * For a proc body with N statements:
 *   - Non-suspend statements: execute eagerly via bb_exec_stmt, advance to next.
 *   - Suspend statements: build bb_node_t from expr; pump α/β until ω; advance.
 *   - Return E: evaluate E, set return_val, stop.
 *   - Fall off end: proc fails.
 *
 * State: stmt index, current expr_box (if pumping a suspend), proc tree_t*.
 *============================================================================================================================*/



/* Forward: frame push/pop -- same as coro_call */
extern int frame_depth;
extern IcnFrame frame_stack[];
/* extern declarations from icn_runtime.h */

static void icn_bb_proc_push_frame(tree_t *proc, DESCR_t *args, int nargs,
                                    IcnScope *sc_out, int *nslots_out) {
    int nparams    = proc->_id;
    int body_start = 1 + nparams;
    int nbody      = proc->n - body_start;
    IcnScope sc;  sc.n = 0;
    for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
        tree_t *pn = proc->c[1+i];
        if (pn && pn->v.sval) scope_add(&sc, pn->v.sval);
    }
    for (int i = 0; i < nbody; i++) {
        tree_t *st = proc->c[body_start+i];
        if (st && st->t == TT_GLOBAL)
            for (int j = 0; j < st->n; j++)
                if (st->c[j] && st->c[j]->v.sval)
                    scope_add(&sc, st->c[j]->v.sval);
    }
    for (int i = 0; i < nbody; i++)
        icn_scope_patch(&sc, proc->c[body_start+i]);
    int nslots = sc.n > 0 ? sc.n : (nparams > 0 ? nparams : FRAME_SLOT_MAX);
    if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;
    IcnFrame *f = &frame_stack[frame_depth++];
    memset(f, 0, sizeof *f);
    f->env_n = nslots;
    f->sc    = sc;
    for (int i = 0; i < nparams && i < nargs && i < FRAME_SLOT_MAX; i++)
        f->env[i] = args[i];
    /* restore statics */
    for (int i = 0; i < nbody; i++) {
        tree_t *st = proc->c[body_start+i];
        if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
        for (int j = 0; j < st->n; j++) {
            tree_t *vn = st->c[j];
            if (!vn || !vn->v.sval) continue;
            int slot = scope_get(&sc, vn->v.sval);
            if (slot < 0 || slot >= nslots) continue;
            DESCR_t saved;
            if (static_get(proc, vn->v.sval, &saved)) f->env[slot] = saved;
        }
    }
    if (sc_out)      *sc_out      = sc;
    if (nslots_out)  *nslots_out  = nslots;
}

static void icn_bb_proc_pop_frame(tree_t *proc) {
    int nparams    = proc->_id;
    int body_start = 1 + nparams;
    int nbody      = proc->n - body_start;
    IcnScope *sc   = &FRAME.sc;
    int nslots     = FRAME.env_n;
    for (int i = 0; i < nbody; i++) {
        tree_t *st = proc->c[body_start+i];
        if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
        for (int j = 0; j < st->n; j++) {
            tree_t *vn = st->c[j];
            if (!vn || !vn->v.sval) continue;
            int slot = scope_get(sc, vn->v.sval);
            if (slot < 0 || slot >= nslots) continue;
            static_set(proc, vn->v.sval, FRAME.env[slot]);
        }
    }
    icn_init_save_frame();
    frame_depth--;
}


DESCR_t icn_bb_proc_call(void *zeta, int entry) {
    icn_proc_state_t *z = (icn_proc_state_t *)zeta;
    tree_t *proc       = z->proc;
    int body_start     = z->body_start;
    int nbody          = z->nbody;

    /* alpha: push fresh frame with args from extended state */
    if (entry == 0) {
        if (frame_depth >= FRAME_STACK_MAX) return FAILDESCR;
        icn_proc_call_state_t *zz = (icn_proc_call_state_t *)zeta;
        icn_bb_proc_push_frame(proc, zz->args, zz->nargs, NULL, NULL);
        z->stmt_idx  = 0;
        z->in_suspend = 0;
    }

    /* Drive statements */
    for (;;) {
        /* If currently pumping a suspend expr, pump beta */
        if (z->in_suspend) {
            DESCR_t v = z->expr_box.fn(z->expr_box.ζ, 1);
            if (!IS_FAIL_fn(v)) {
                if (z->suspend_body) bb_eval_value(z->suspend_body);
                return v;  /* yield next value */
            }
            /* expr exhausted -- advance past this suspend stmt */
            z->in_suspend = 0;
            z->stmt_idx++;
        }

        /* Advance through statements */
        while (z->stmt_idx < nbody) {
            tree_t *st = proc->c[body_start + z->stmt_idx];
            if (!st || st->t == TT_GLOBAL) { z->stmt_idx++; continue; }

            /* TT_RETURN or TT_PROC_FAIL: evaluate, pop frame, done */
            if (st->t == TT_RETURN) {
                DESCR_t rv = (st->n >= 1 && st->c[0]) ? bb_eval_value(st->c[0]) : NULVCL;
                icn_bb_proc_pop_frame(proc);
                return rv;  /* one value, then omega on next beta */
            }
            if (st->t == TT_PROC_FAIL) {
                icn_bb_proc_pop_frame(proc);
                return FAILDESCR;
            }

            /* TT_SUSPEND: build expr generator, pump alpha for first value */
            if (st->t == TT_SUSPEND) {
                tree_t *expr_node = (st->n >= 1) ? st->c[0] : NULL;
                tree_t *body_node = (st->n >= 2) ? st->c[1] : NULL;
                if (!expr_node) { z->stmt_idx++; continue; }
                z->expr_box    = icn_bb_build(expr_node);
                z->suspend_body = body_node;
                z->in_suspend   = 1;
                DESCR_t v = z->expr_box.fn(z->expr_box.ζ, 0);
                if (!IS_FAIL_fn(v)) {
                    if (body_node) bb_eval_value(body_node);
                    return v;  /* first yield */
                }
                /* expr immediately exhausted */
                z->in_suspend = 0;
                z->stmt_idx++;
                continue;
            }

            /* All other statements: execute for side effects, advance */
            bb_exec_stmt(st);
            if (FRAME.returning) {
                DESCR_t rv = FRAME.return_val;
                icn_bb_proc_pop_frame(proc);
                return rv;
            }
            if (FRAME.loop_break) { z->stmt_idx++; continue; }
            z->stmt_idx++;
        }

        /* Fell off end of body -- proc fails */
        icn_bb_proc_pop_frame(proc);
        return FAILDESCR;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_make_proc_box -- build an icn_bb_proc_call box for a proc node.
 * Called from icn_bb_build TT_FNC user-proc path to replace icn_bb_suspend.
 *--------------------------------------------------------------------------------------------------------------------------*/
bb_node_t icn_bb_make_proc_box(tree_t *proc, DESCR_t *args, int nargs) {
    icn_proc_call_state_t *zz = calloc(1, sizeof(*zz));
    zz->base.proc       = proc;
    zz->base.body_start = 1 + proc->_id;
    zz->base.nbody      = proc->n - zz->base.body_start;
    zz->base.stmt_idx   = 0;
    zz->base.in_suspend = 0;
    zz->nargs = nargs < 16 ? nargs : 16;
    for (int i = 0; i < zz->nargs; i++) zz->args[i] = args ? args[i] : NULVCL;
    return (bb_node_t){ icn_bb_proc_call, zz, 0 };
}

/*============================================================================================================================
 * icn_bb_section_gen -- ir_a_Sectionop with generative operands
 *
 * JCON wiring: val (object), left (i), right (j) all potentially generative.
 * Drive val first; on each val tick drive left; on each left tick drive right;
 * on each right tick compute val[left:right] (or +: or -:) and yield.
 * On right exhaustion resume left; on left exhaustion resume val.
 *
 * State: val_gen, left_gen, right_gen, section op kind, cached val/left values.
 *============================================================================================================================*/

static DESCR_t icn_apply_section(DESCR_t val, DESCR_t left, DESCR_t right, icn_sec_kind_t kind) {
    /* Mirrors bb_section in icn_value.c */
    extern DESCR_t bb_eval_value(tree_t *e);
    const char *s = VARVAL_fn(val); if (!s) s = "";
    long slen = (long)strlen(s);
    long i = IS_INT_fn(left)  ? left.i  : 0;
    long j = IS_INT_fn(right) ? right.i : 0;
    /* Normalize positions: 0->slen+1, negative->from end */
    if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
    if (kind == ICN_SEC_RANGE) {
        if (j == 0) j = slen + 1; else if (j < 0) j = slen + 1 + j;
    } else if (kind == ICN_SEC_PLUS)  { j = i + j; }
      else                            { j = i - j; }
    if (i > j) { long t = i; i = j; j = t; }
    i--; /* 0-based start */
    if (i < 0 || j > slen + 1 || i > j) return FAILDESCR;
    long len = j - i;
    char *buf = GC_malloc(len + 1);
    memcpy(buf, s + i, len); buf[len] = '\0';
    return STRVAL(buf);
}

DESCR_t icn_bb_section_gen(void *zeta, int entry) {
    icn_section_gen_state_t *z = (icn_section_gen_state_t *)zeta;
    int vport = (z->val_started   && entry == 1) ? 1 : 0;
    int lport = (z->left_started  && entry == 1) ? 1 : 0;
    int rport = (z->right_started && entry == 1) ? 1 : 0;

    for (;;) {
        /* Drive val */
        if (!z->val_started || rport == 0 && lport == 0) {
            z->cur_val = z->val_gen.fn(z->val_gen.ζ, z->val_started ? 1 : 0);
            z->val_started = 1;
            if (IS_FAIL_fn(z->cur_val)) return FAILDESCR;
            /* Reset left/right for new val */
            z->left_gen    = icn_bb_build(z->left_expr);
            z->left_started = 0;
            z->right_started = 0;
        }
        /* Drive left */
        z->cur_left = z->left_gen.fn(z->left_gen.ζ, z->left_started ? 1 : 0);
        z->left_started = 1;
        if (IS_FAIL_fn(z->cur_left)) {
            /* Left exhausted: resume val */
            z->left_started = 0; z->right_started = 0;
            z->cur_val = z->val_gen.fn(z->val_gen.ζ, 1);
            if (IS_FAIL_fn(z->cur_val)) return FAILDESCR;
            z->left_gen = icn_bb_build(z->left_expr);
            z->left_started = 0;
            continue;
        }
        /* Drive right */
        z->right_gen = icn_bb_build(z->right_expr);
        z->right_started = 0;
        DESCR_t rv = z->right_gen.fn(z->right_gen.ζ, 0);
        z->right_started = 1;
        if (IS_FAIL_fn(rv)) continue; /* right immediately exhausted, try next left */
        return icn_apply_section(z->cur_val, z->cur_left, rv, z->kind);
    }
}

/*============================================================================================================================
 * icn_bb_key_gen -- ir_a_Key for generative keywords (&features, &allocated, etc.)
 *
 * JCON: generative keywords emit ir_ResumeValue; one-shot keywords emit ir_Key once.
 * Generative keywords in Icon: &features (generates feature strings), &allocated (3 ints),
 * &collections (4 ints). For now: fallback to icn_kw_read and wrap as one-shot.
 * Actual generators added per keyword as needed.
 *============================================================================================================================*/

DESCR_t icn_bb_key_gen(void *zeta, int entry) {
    icn_kw_gen_state_t *z = (icn_kw_gen_state_t *)zeta;
    if (entry == 1 || z->fired) return FAILDESCR;
    z->fired = 1;
    extern DESCR_t icn_kw_read(const char *kw);
    return icn_kw_read(z->kw);
}

/*============================================================================================================================
 * icn_bb_listcon_gen -- ir_a_ListConstructor with generative elements
 *
 * JCON: evaluate each element (bounded); collect into a list; return it once.
 * The ListConstructor itself is one-shot (always bounded in JCON).
 * Generative elements are evaluated eagerly — the list gets the first value of each.
 * State: child exprs, count.
 *============================================================================================================================*/

DESCR_t icn_bb_listcon_gen(void *zeta, int entry) {
    icn_listcon_state_t *z = (icn_listcon_state_t *)zeta;
    if (entry == 1 || z->fired) return FAILDESCR;
    z->fired = 1;
    DESCR_t elems[ICN_LISTCON_MAX];
    for (int i = 0; i < z->n; i++) {
        elems[i] = z->children[i] ? bb_eval_value(z->children[i]) : NULVCL;
        if (IS_FAIL_fn(elems[i])) return FAILDESCR;
    }
    /* Build icnlist descriptor -- mirrors MAKELIST in icn_try_call_builtin_by_name */
    static int reg = 0;
    if (!reg) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); reg = 1; }
    DESCR_t *ep = GC_malloc((z->n > 0 ? z->n : 1) * sizeof(DESCR_t));
    for (int i = 0; i < z->n; i++) ep[i] = elems[i];
    DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void*)ep;
    return DATCON_fn("icnlist", eptr, INTVAL(z->n), STRVAL("list"));
}

/*============================================================================================================================
 * Wire new BBs into icn_bb_build_missing
 *============================================================================================================================*/
/* Override: replace the old icn_bb_build_missing with extended version */
/* Note: we append here; the linker will use the last definition if both
 * are non-static. Instead, we rename the old one and call from new. */

/*============================================================================================================================
 * icn_bb_build_missing -- top-level dispatch for all missing JCON BBs.
 * Called from icn_bb_build fallback. Handles new constructs then delegates.
 *============================================================================================================================*/
bb_node_t icn_bb_build_missing(tree_t *e) {
    if (!e) goto fallback;

    /* ir_a_Sectionop with generative operands */
    if ((e->t == TT_SECTION || e->t == TT_SECTION_PLUS || e->t == TT_SECTION_MINUS)
        && e->n >= 3
        && (is_suspendable(e->c[0]) || is_suspendable(e->c[1]) || is_suspendable(e->c[2]))) {
        icn_section_gen_state_t *z = calloc(1, sizeof(*z));
        z->val_expr   = e->c[0];
        z->left_expr  = e->c[1];
        z->right_expr = e->c[2];
        z->val_gen    = icn_bb_build(e->c[0]);
        z->left_gen   = icn_bb_build(e->c[1]);
        z->kind       = (e->t == TT_SECTION_PLUS) ? ICN_SEC_PLUS
                      : (e->t == TT_SECTION_MINUS) ? ICN_SEC_MINUS
                      : ICN_SEC_RANGE;
        return (bb_node_t){ icn_bb_section_gen, z, 0 };
    }

    /* ir_a_Key -- generative keyword */
    if (e->t == TT_KEYWORD && e->v.sval) {
        icn_kw_gen_state_t *z = calloc(1, sizeof(*z));
        z->kw    = e->v.sval;
        z->fired = 0;
        return (bb_node_t){ icn_bb_key_gen, z, 0 };
    }

    /* ir_a_ListConstructor -- generative list [e1, e2, ...] */
    if (e->t == TT_MAKELIST) {
        icn_listcon_state_t *z = calloc(1, sizeof(*z));
        z->n     = 0;
        z->fired = 0;
        for (int i = 0; i < e->n && z->n < ICN_LISTCON_MAX; i++)
            z->children[z->n++] = e->c[i];
        return (bb_node_t){ icn_bb_listcon_gen, z, 0 };
    }

    fallback:
    return icn_bb_build_missing_base(e);
}

/*============================================================================================================================
 * State allocators for emit_bb.c wiring -- one per new BB
 *============================================================================================================================*/
icn_not_state_t        *icon_not_new(void)          { return calloc(1, sizeof(icn_not_state_t)); }
icn_repalt_state_t     *icon_repalt_new(void)        { return calloc(1, sizeof(icn_repalt_state_t)); }
icn_while_state_t      *icon_while_gen_new(void)     { return calloc(1, sizeof(icn_while_state_t)); }
icn_until_state_t      *icon_until_gen_new(void)     { return calloc(1, sizeof(icn_until_state_t)); }
icn_repeat_state_t     *icon_repeat_gen_new(void)    { return calloc(1, sizeof(icn_repeat_state_t)); }
icn_case_state_t       *icon_case_gen_new(void)      { return calloc(1, sizeof(icn_case_state_t)); }
icn_compound_state_t   *icon_compound_gen_new(void)  { return calloc(1, sizeof(icn_compound_state_t)); }
icn_field_gen_state_t  *icon_field_gen_new(void)     { return calloc(1, sizeof(icn_field_gen_state_t)); }
icn_section_gen_state_t *icon_section_gen_new(void)  { return calloc(1, sizeof(icn_section_gen_state_t)); }
icn_kw_gen_state_t     *icon_kw_gen_new(void)        { return calloc(1, sizeof(icn_kw_gen_state_t)); }
icn_listcon_state_t    *icon_listcon_gen_new(void)   { return calloc(1, sizeof(icn_listcon_state_t)); }
icn_proc_call_state_t  *icon_proc_call_new(void)     { return calloc(1, sizeof(icn_proc_call_state_t)); }

/*============================================================================================================================
 * Non-generative JCON BBs: semantic functions + allocators (IJ-43 group)
 * Each succeeds once on alpha with evaluated value, fails on beta.
 *============================================================================================================================*/
icn_noop_state_t      *icon_noop_new(void)     { return calloc(1,sizeof(icn_noop_state_t)); }
icn_intlit_state_t    *icon_intlit_new(void)   { return calloc(1,sizeof(icn_intlit_state_t)); }
icn_reallit_state_t   *icon_reallit_new(void)  { return calloc(1,sizeof(icn_reallit_state_t)); }
icn_strlit_state_t    *icon_strlit_new(void)   { return calloc(1,sizeof(icn_strlit_state_t)); }
icn_csetlit_state_t   *icon_csetlit_new(void)  { return calloc(1,sizeof(icn_csetlit_state_t)); }
icn_global_state_t    *icon_global_new(void)   { return calloc(1,sizeof(icn_global_state_t)); }
icn_if_state_t        *icon_if_new(void)       { return calloc(1,sizeof(icn_if_state_t)); }
icn_initial_state_t   *icon_initial_new(void)  { return calloc(1,sizeof(icn_initial_state_t)); }
icn_invocable_state_t *icon_invocable_new(void){ return calloc(1,sizeof(icn_invocable_state_t)); }
icn_link_state_t      *icon_link_new(void)     { return calloc(1,sizeof(icn_link_state_t)); }
icn_record_state_t    *icon_record_new(void)   { return calloc(1,sizeof(icn_record_state_t)); }
icn_return_state_t    *icon_return_new(void)   { return calloc(1,sizeof(icn_return_state_t)); }
icn_fail_state_t      *icon_fail_new(void)     { return calloc(1,sizeof(icn_fail_state_t)); }
icn_unop_state_t      *icon_unop_new(void)     { return calloc(1,sizeof(icn_unop_state_t)); }
icn_next_state_t      *icon_next_new(void)     { return calloc(1,sizeof(icn_next_state_t)); }
icn_break_state_t     *icon_break_new(void)    { return calloc(1,sizeof(icn_break_state_t)); }
icn_create_state_t    *icon_create_new(void)   { return calloc(1,sizeof(icn_create_state_t)); }
icn_coexplist_state_t *icon_coexplist_new(void){ return calloc(1,sizeof(icn_coexplist_state_t)); }
icn_arglist_state_t   *icon_arglist_new(void)  { return calloc(1,sizeof(icn_arglist_state_t)); }
icn_procdecl_state_t  *icon_procdecl_new(void) { return calloc(1,sizeof(icn_procdecl_state_t)); }
icn_procbody_state_t  *icon_procbody_new(void) { return calloc(1,sizeof(icn_procbody_state_t)); }
icn_proccode_state_t  *icon_proccode_new(void) { return calloc(1,sizeof(icn_proccode_state_t)); }

DESCR_t icn_bb_noop     (void *z,int p){if(p)return FAILDESCR;return NULVCL;}
DESCR_t icn_bb_intlit   (void *z,int p){if(p)return FAILDESCR;return INTVAL(((icn_intlit_state_t*)z)->val);}
DESCR_t icn_bb_reallit  (void *z,int p){if(p)return FAILDESCR;return REALVAL(((icn_reallit_state_t*)z)->val);}
DESCR_t icn_bb_strlit   (void *z,int p){if(p)return FAILDESCR;const char *s=((icn_strlit_state_t*)z)->s;return s?STRVAL(s):NULVCL;}
DESCR_t icn_bb_csetlit  (void *z,int p){if(p)return FAILDESCR;const char *s=((icn_csetlit_state_t*)z)->s;return s?CSETVAL(s):NULVCL;}
DESCR_t icn_bb_global   (void *z,int p){if(p)return FAILDESCR;return NULVCL;}
DESCR_t icn_bb_if_bb    (void *z,int p){if(p)return FAILDESCR;icn_if_state_t *s=(icn_if_state_t*)z;DESCR_t c=s->cond?bb_eval_value(s->cond):FAILDESCR;return bb_eval_value(IS_FAIL_fn(c)?s->else_e:s->then_e);}
DESCR_t icn_bb_initial  (void *z,int p){if(p)return FAILDESCR;icn_initial_state_t *s=(icn_initial_state_t*)z;return s->body?bb_eval_value(s->body):NULVCL;}
DESCR_t icn_bb_invocable(void *z,int p){if(p)return FAILDESCR;return NULVCL;}
DESCR_t icn_bb_link     (void *z,int p){if(p)return FAILDESCR;return NULVCL;}
DESCR_t icn_bb_record_bb(void *z,int p){if(p)return FAILDESCR;return NULVCL;}
DESCR_t icn_bb_return_bb(void *z,int p){if(p)return FAILDESCR;icn_return_state_t *s=(icn_return_state_t*)z;DESCR_t v=s->expr?bb_eval_value(s->expr):NULVCL;if(frame_depth>0){FRAME.return_val=v;FRAME.returning=1;}return v;}
DESCR_t icn_bb_fail_bb  (void *z,int p){if(p)return FAILDESCR;if(frame_depth>0){FRAME.return_val=FAILDESCR;FRAME.returning=1;}return FAILDESCR;}
DESCR_t icn_bb_unop     (void *z,int p){if(p)return FAILDESCR;icn_unop_state_t *s=(icn_unop_state_t*)z;DESCR_t v=s->operand?bb_eval_value(s->operand):NULVCL;DESCR_t out;if(s->op&&icn_try_call_builtin_by_name(s->op,&v,1,&out))return out;return FAILDESCR;}
DESCR_t icn_bb_next_bb  (void *z,int p){if(p)return FAILDESCR;if(frame_depth>0)FRAME.loop_next=1;return NULVCL;}
DESCR_t icn_bb_break_bb (void *z,int p){if(p)return FAILDESCR;icn_break_state_t *s=(icn_break_state_t*)z;if(frame_depth>0)FRAME.loop_break=1;return s->expr?bb_eval_value(s->expr):NULVCL;}
DESCR_t icn_bb_create   (void *z,int p){if(p)return FAILDESCR;return NULVCL;}
DESCR_t icn_bb_coexplist(void *z,int p){(void)z;(void)p;return FAILDESCR;}
DESCR_t icn_bb_arglist  (void *z,int p){(void)z;(void)p;return FAILDESCR;}
DESCR_t icn_bb_procdecl (void *z,int p){if(p)return FAILDESCR;return NULVCL;}
DESCR_t icn_bb_procbody (void *z,int p){if(p)return FAILDESCR;icn_procbody_state_t *s=(icn_procbody_state_t*)z;return s->body?bb_eval_value(s->body):NULVCL;}
DESCR_t icn_bb_proccode (void *z,int p){if(p)return FAILDESCR;icn_proccode_state_t *s=(icn_proccode_state_t*)z;if(s->init)bb_eval_value(s->init);return s->body?bb_eval_value(s->body):NULVCL;}
