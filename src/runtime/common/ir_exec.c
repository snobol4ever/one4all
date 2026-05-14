/*
 * ir_exec.c — DCG graph-walk executor: IR_exec_once, IR_exec_pump (LR-2)
 *             IR_PAT_* cursor-walk cases + IR_exec_pat (LR-S1b)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-2, 2026-05-14; LR-S1b, 2026-05-14)
 */
#include "ir_exec.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gc/gc.h>
/* ── Cursor globals from stmt_exec.c ───────────────────────────────────────
 * Σ = subject string base, Δ = current cursor, Ω = anchor limit,
 * Σlen = true subject length (Ω may be clamped by &ANCHOR).
 * Non-static file-scope in stmt_exec.c; extern here for IR_PAT_* nodes. */
extern const char *Σ;
extern int         Δ;
extern int         Ω;
extern int         Σlen;
/* NV_SET_fn for conditional/immediate capture assignment (IR_PAT_ASSIGN_*). */
#include "../../runtime/x86/snobol4.h"
/* descr_match_span: construct DT_S match descriptor from (base, len). */
#include "../../runtime/x86/bb_box.h"
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_exec_node — evaluate nd in its current state; return next port.
 * Self-evaluating scalar kinds set nd->value and return γ or ω.
 * Generative kinds consult nd->state / nd->counter and update them.
 * Unimplemented kinds return ω (explicit, safe, detectable in tests). */
IR_t * IR_exec_node(IR_t * nd) {
    switch (nd->t) {
    /*-- Literals: always succeed, value is the literal. ----------------------------------------------------------------*/
    case IR_LIT_I:
        nd->value = INTVAL(nd->ival);
        return nd->γ;
    case IR_LIT_F:
        nd->value = REALVAL(nd->dval);
        return nd->γ;
    case IR_LIT_S:
        nd->value = STRVAL(nd->sval ? nd->sval : "");
        return nd->γ;
    case IR_LIT_NUL:
    case IR_SUCCEED:
        nd->value = NULVCL;
        return nd->γ;
    /*-- FAIL: always fails. --------------------------------------------------------------------------------------------*/
    case IR_FAIL:
        nd->value = FAILDESCR;
        return nd->ω;
    /*-- TO_BY: integer range generator.
     * state 0 = fresh (init from children c[0]=from, c[1]=to, c[2]=by or NULL).
     * state 1 = running; nd->counter = current value; nd->value = INTVAL(counter).
     * state 2 = exhausted → ω. -------------------------------------------------------------------------------*/
    case IR_TO_BY: {
        if (nd->state == 0) {
            /* evaluate children to get from/to/by -- do not rely on pre-set values */
            int64_t from = 0, by = 1;
            if (nd->n > 0 && nd->c[0]) { IR_exec_node(nd->c[0]); from = nd->c[0]->value.i; }
            if (nd->n > 2 && nd->c[2]) { IR_exec_node(nd->c[2]); by   = nd->c[2]->value.i; }
            if (by == 0) by = 1;
            nd->counter = from;
            nd->ival    = by;   /* reuse ival for step */
            nd->state   = 1;
        }
        if (nd->state == 2) {
            nd->value = FAILDESCR;
            return nd->ω;
        }
        /* evaluate to-child (may be dynamic) */
        int64_t to_val = 0;
        if (nd->n > 1 && nd->c[1]) { IR_exec_node(nd->c[1]); to_val = nd->c[1]->value.i; }
        int64_t by = nd->ival;
        if (by >= 0 ? nd->counter > to_val : nd->counter < to_val) {
            nd->state = 2;
            nd->value = FAILDESCR;
            return nd->ω;
        }
        nd->value    = INTVAL(nd->counter);
        nd->counter += by;
        return nd->γ;
    }
    /*-- ALTERNATE(A,B): try A; on A-fail try B. Wired by lower:
     * α→A.start, A.succ→self.γ, A.fail→B.start,
     * B.succ→self.γ, B.fail→self.ω.
     * IR_exec_node is not called for ALTERNATE in the walker —
     * the port wiring handles routing.  But if somehow called, route to fail. */
    case IR_ALTERNATE:
        nd->value = FAILDESCR;
        return nd->ω;
    /*-- IR_PAT_LIT: literal string match.
     * counter = length of literal (set on first call from nd->sval).
     * On entry: try to match nd->sval at Σ+Δ.  Advance Δ on success. ------------------------------------------------*/
    case IR_PAT_LIT: {
        const char *lit = nd->sval ? nd->sval : "";
        int         len = (int)strlen(lit);
        if (nd->state == 0) {
            /* fresh: attempt match at current Δ */
            if (Δ + len > Σlen || (len > 0 && memcmp(Σ + Δ, lit, (size_t)len) != 0)) {
                nd->value = FAILDESCR;
                return nd->ω;
            }
            nd->counter = len;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, len);
            Δ += len;
            return nd->γ;
        }
        /* resume → undo and fail (literal is non-generative) */
        Δ -= (int)nd->counter;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_ANY: match one char from charset.
     * Non-generative: succeeds once or fails. -----------------------------------------------------------------------*/
    case IR_PAT_ANY: {
        const char *chars = nd->sval ? nd->sval : "";
        if (nd->state == 0) {
            if (Δ >= Σlen || !strchr(chars, Σ[Δ])) {
                nd->value = FAILDESCR;
                return nd->ω;
            }
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, 1);
            Δ++;
            return nd->γ;
        }
        /* resume → undo */
        Δ--;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_BREAK: match chars NOT in charset, up to first member.
     * Non-generative: matches 0 or more chars (may match empty). ---------------------------------------------------*/
    case IR_PAT_BREAK: {
        const char *chars = nd->sval ? nd->sval : "";
        if (nd->state == 0) {
            int i = 0;
            while (Δ + i < Σlen && !strchr(chars, Σ[Δ + i])) i++;
            nd->counter = i;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, i);
            Δ += i;
            return nd->γ;
        }
        /* resume → undo */
        Δ -= (int)nd->counter;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_SPAN: match one or more chars from charset; generative (try-shorter on resume).
     * state 0 = fresh; counter = chars consumed (max greedy match).
     * state 1 = yielded max; resume tries max-1, max-2, ... down to 1.
     * state 2 = exhausted → fail. -----------------------------------------------------------------------------------*/
    case IR_PAT_SPAN: {
        const char *chars = nd->sval ? nd->sval : "";
        if (nd->state == 0) {
            int i = 0;
            while (Δ + i < Σlen && strchr(chars, Σ[Δ + i])) i++;
            if (i == 0) { nd->value = FAILDESCR; return nd->ω; }
            nd->counter = i;   /* max match length */
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, i);
            Δ += i;
            return nd->γ;
        }
        if (nd->state == 1) {
            /* resume: try one shorter */
            Δ -= (int)nd->counter;   /* undo previous */
            nd->counter--;
            if (nd->counter < 1) { nd->state = 2; nd->value = FAILDESCR; return nd->ω; }
            nd->value = descr_match_span(Σ + Δ, (int)nd->counter);
            Δ += (int)nd->counter;
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_ARB: match any number of chars (0,1,2,...); generative (try-longer on resume).
     * state 0 = fresh; tries 0 chars first.
     * state 1 = running; counter = chars consumed last time; resume tries counter+1.
     * state 2 = exhausted (Δ+counter >= Σlen). ----------------------------------------------------------------------*/
    case IR_PAT_ARB: {
        if (nd->state == 0) {
            nd->counter = 0;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, 0);
            /* do NOT advance Δ — 0-length match */
            return nd->γ;
        }
        if (nd->state == 1) {
            /* resume: undo last, try one longer */
            Δ -= (int)nd->counter;
            nd->counter++;
            if (Δ + (int)nd->counter > Σlen) {
                nd->state = 2;
                nd->value = FAILDESCR;
                return nd->ω;
            }
            nd->value = descr_match_span(Σ + Δ, (int)nd->counter);
            Δ += (int)nd->counter;
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_REM: match everything from Δ to end of subject. Non-generative. ------------------------------------*/
    case IR_PAT_REM: {
        if (nd->state == 0) {
            int rem = Σlen - Δ;
            nd->counter = rem;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, rem);
            Δ = Σlen;
            return nd->γ;
        }
        /* resume → undo */
        Δ -= (int)nd->counter;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_FENCE: succeed once, cut on resume (prevent backtrack through). ------------------------------------*/
    case IR_PAT_FENCE: {
        if (nd->state == 0) {
            nd->state = 1;
            nd->value = NULVCL;
            return nd->γ;
        }
        /* resume → hard fail (FENCE cuts backtrack) */
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_ABORT: always fails unconditionally (no backtrack). ------------------------------------------------*/
    case IR_PAT_ABORT: {
        nd->value = FAILDESCR;
        return nd->ω;
    }
    /*-- IR_PAT_CAT / IR_PAT_ALT: wiring-only nodes — routing handled by port pointers.
     * IR_exec_node is not called on these during normal walk; if called, pass through. ---------------------------*/
    case IR_PAT_CAT:
    case IR_PAT_ALT:
        nd->value = NULVCL;
        return nd->γ;
    /*-- IR_PAT_ASSIGN_COND (P $ V): conditional capture — assign matched span to variable on pattern success.
     * The inner pattern is wired as α; we fire NV_SET_fn when succ arrives.
     * counter = Δ at entry (to compute matched span on success). ---------------------------------------------------*/
    case IR_PAT_ASSIGN_COND: {
        if (nd->state == 0) {
            nd->counter = Δ;   /* record Δ before inner pattern runs */
            nd->state   = 1;
            nd->value   = NULVCL;
            return nd->γ;   /* pass through to inner; wiring routes succ here after inner */
        }
        /* Called again on inner success — assign */
        if (nd->sval && *nd->sval) {
            DESCR_t matched = descr_match_span(Σ + (int)nd->counter, Δ - (int)nd->counter);
            NV_SET_fn(nd->sval, matched);
        }
        nd->value = NULVCL;
        return nd->γ;
    }
    /*-- IR_PAT_ASSIGN_IMM (P . V): immediate capture — assign on every match attempt (including failed ones).
     * Simpler than COND: assign matched span each time inner succeeds. Same port structure. ----------------------*/
    case IR_PAT_ASSIGN_IMM: {
        if (nd->state == 0) {
            nd->counter = Δ;
            nd->state   = 1;
            nd->value   = NULVCL;
            return nd->γ;
        }
        if (nd->sval && *nd->sval) {
            DESCR_t matched = descr_match_span(Σ + (int)nd->counter, Δ - (int)nd->counter);
            NV_SET_fn(nd->sval, matched);
        }
        nd->value = NULVCL;
        return nd->γ;
    }
    /*-- All other kinds: not yet implemented — return ω explicitly. ----------------------------------------*/
    default:
        nd->value = FAILDESCR;
        return nd->ω;
    }
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_exec_once — drive cfg from entry to first succ or fail.
 * Graph walker: pointer-chase from entry through γ/ω.
 * Back-edges (cycles) are traversed by the pointer chain; exhaustion is
 * when a node routes to ω with no further resume path. */
DESCR_t IR_exec_once(IR_prog_t * cfg) {
    if (!cfg || !cfg->entry) return FAILDESCR;
    IR_reset(cfg);
    IR_t * cur = cfg->entry;
    int safety = cfg->n * 64 + 256;   /* cycle-breaker: max steps before abort */
    while (cur && safety-- > 0) {
        IR_t * next = IR_exec_node(cur);
        if (!next) {
            /* terminal: succ path returned NULL → value is in cur->value */
            return IS_FAIL_fn(cur->value) ? FAILDESCR : cur->value;
        }
        /* If next == cur we have an infinite self-loop on a non-generative node.
         * Treat as fail to avoid spinning forever. */
        if (next == cur) return FAILDESCR;
        cur = next;
    }
    return FAILDESCR;
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_exec_pump — drive cfg to exhaustion, calling body_fn per value.
 * After IR_exec_once returns a value, resume by following β of the
 * deepest node that has one.  Implementation: simple retry loop using
 * IR_exec_once_resume which starts from β of the last-succ node. */
int IR_exec_pump(IR_prog_t * cfg, IR_body_fn body_fn, void * ctx) {
    if (!cfg || !cfg->entry) return 0;
    IR_reset(cfg);
    int ticks  = 0;
    int safety = cfg->n * 256 + 1024;
    IR_t * cur = cfg->entry;
    while (cur && safety-- > 0) {
        IR_t * next = IR_exec_node(cur);
        if (!next) {
            /* terminal node: check value */
            if (!IS_FAIL_fn(cur->value)) {
                ticks++;
                if (body_fn && body_fn(cur->value, ctx)) break;
                /* resume from this node's β — do NOT reset state */
                next = cur->β;
                if (!next) break;
            } else {
                break;
            }
        } else if (next == cur) {
            /* self-loop on a generative node: eval again without resetting */
            continue;
        }
        cur = next;
    }
    return ticks;
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* IR_exec_pat — LR-S1b: IR-path equivalent of exec_stmt() for SNOBOL4 patterns.
 *
 * Called from SM_EXEC_STMT when ins->a[2].ptr is a non-NULL IR_prog_t* (built by
 * IR_lower_pat at lower time).  Mirrors exec_stmt Phases 1+3+4+5 using the IR graph
 * walker instead of the dynamic bb_node_t broker.
 *
 * Cursor globals Σ/Δ/Ω/Σlen are set up here (Phase 1) and consumed by IR_exec_node
 * PAT_* cases.  On match success the BB_SCAN equivalent is finding the first anchor
 * position where IR_exec_once returns non-FAIL; then Phase 4/5 perform replacement.
 *
 * Returns 1 on match (:S branch), 0 on no-match (:F branch).
 */
int IR_exec_pat(IR_prog_t *cfg,
                const char *subj_name,
                DESCR_t    *subj_var,
                DESCR_t    *repl,
                int         has_repl)
{
    if (!cfg || !cfg->entry) return 0;
    /* ── Phase 1: set up subject cursor ── */
    const char *subj_str = "";
    int         subj_len = 0;
    DESCR_t subj_fetched;
    if (subj_name && *subj_name) {
        subj_fetched = NV_GET_fn(subj_name);
        subj_var     = &subj_fetched;
    }
    if (subj_var) {
        DESCR_t sv = VARVAL_d_fn(*subj_var);
        if (sv.v == DT_S || sv.v == DT_SNUL) {
            subj_str = sv.s ? sv.s : "";
            subj_len = sv.slen ? (int)sv.slen : (int)strlen(subj_str);
        }
    }
    Σ    = subj_str;
    Σlen = subj_len;
    Ω    = subj_len;
    /* ── Phase 3: scan — try each anchor position 0..Ω ── */
    int match_start = -1;
    int match_end   = -1;
    extern int64_t kw_anchor;
    int max_start = kw_anchor ? 0 : Ω;
    for (int start = 0; start <= max_start; start++) {
        Δ = start;
        IR_reset(cfg);
        DESCR_t result = IR_exec_once(cfg);
        if (!IS_FAIL_fn(result)) {
            match_start = start;
            match_end   = Δ;
            break;
        }
    }
    if (match_start < 0) return 0;   /* :F */
    /* ── Phase 4/5: replacement (mirrors exec_stmt) ── */
    if (!has_repl || !repl) return 1;   /* :S, no replacement */
    if (!subj_name && !subj_var)        return 0;   /* no lvalue — :F */
    const char *repl_str = "";
    int         repl_len = 0;
    if (repl->v == DT_S && repl->s) {
        repl_str = repl->s;
        repl_len = repl->slen ? (int)repl->slen : (int)strlen(repl->s);
    } else if (repl->v == DT_I) {
        char ibuf[32];
        snprintf(ibuf, sizeof(ibuf), "%lld", (long long)repl->i);
        char *gs = (char *)GC_MALLOC(strlen(ibuf) + 1);
        strcpy(gs, ibuf);
        repl_str = gs;
        repl_len = (int)strlen(gs);
    }
    int   new_len = match_start + repl_len + (subj_len - match_end);
    char *new_s   = (char *)GC_MALLOC((size_t)new_len + 1);
    memcpy(new_s,                          subj_str,                (size_t)match_start);
    memcpy(new_s + match_start,            repl_str,                (size_t)repl_len);
    memcpy(new_s + match_start + repl_len, subj_str + match_end,    (size_t)(subj_len - match_end));
    new_s[new_len] = '\0';
    DESCR_t new_val = { .v = DT_S, .slen = (uint32_t)new_len, .s = new_s };
    if (subj_name && *subj_name) {
        NV_SET_fn(subj_name, new_val);
    } else if (subj_var) {
        *subj_var = new_val;
    }
    return 1;
}
