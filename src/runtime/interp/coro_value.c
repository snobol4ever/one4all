/*============================================================================================================================
 * coro_value.c — RS-17a: pure-BB value-context evaluator for Icon Byrd boxes.
 *
 * See coro_value.h for the contract and migration plan.
 *
 * Today this delegates to `eval_node` for kinds that are SNOBOL4/Icon-identical
 * (literals, keywords) and adds an Icon-frame-aware TT_VAR shim that reads the
 * slot-indexed local from FRAME.env when frame_depth > 0.  All other kinds
 * fall through to `interp_eval` — that fallthrough is intentional scaffolding
 * for the incremental migration of coro_runtime.c call sites (RS-17a-cont).
 *
 * RS-22a (2026-05-03): TT_ASSIGN and TT_FNC ported out of interp_eval's Icon-frame
 * switch.  All interp_eval(child) recurses replaced with bb_eval_value(child).
 * TT_FNC builtins route through icn_call_builtin (already IR-free).
 *
 * RS-22b (2026-05-03): Arithmetic + numeric-comparison binops lifted in.
 * TT_ADD/TT_SUB/TT_MUL/TT_DIV/TT_MOD/TT_POW dispatch through `shared_arith` in
 * runtime/common/coerce.c (mirrors sm_interp's SM_ADD..SM_EXP path —
 * FAIL propagation, DT_S → INT, DT_SNUL → INT 0, then shared_arith).
 * TT_LT/TT_LE/TT_GT/TT_GE/TT_EQ/TT_NE return the right operand on success,
 * FAILDESCR on fail (Icon goal-directed convention).  TT_IDENTICAL routes
 * through `icn_descr_identical` (declared in coro_runtime.h).  Note: there
 * is no AST_NOT_IDENTICAL kind — `~===` lowers as TT_NOT(TT_IDENTICAL(...)).
 *
 * RS-22c (2026-05-03): String concat + subscript read + section read +
 * field read lifted in.  TT_CAT and TT_LCONCAT share `bb_str_concat`
 * (numeric operands → string via `descr_to_str_icn`, then GC_malloc'd
 * concat).  TT_IDX dispatches via `subscript_get`/`subscript_get2` (already
 * exposed in snobol4.h).  TT_FIELD via `data_field_ptr`.  TT_SECTION/
 * TT_SECTION_PLUS/TT_SECTION_MINUS share `bb_section` with Icon position
 * normalization (0 → slen+1, negative → slen+1+p) — three minor variants
 * of bound computation kept inline.
 *
 * RS-22d (2026-05-03): Unary + augmented-assign kinds lifted in.
 * TT_MNS (unary `-` — Icon parser uses TT_MNS, not the rung-text "AST_NEG"),
 * TT_PLS (unary `+`), TT_NOT (`not`), TT_NULL (`/`), TT_NONNULL (`\`),
 * TT_SIZE (`*`), TT_RANDOM (`?`) all dispatched directly.  TT_AUGOP (the
 * actual IR kind name; rung-text "AST_AUGASSIGN" was a label rather than
 * the literal kind) handles all three IR-mode paths: bang-iterate lvalue
 * (`!container OP:= rhs`), generator-RHS drive (`every sum +:= (1 to n)`
 * via coro_eval + bb_node_t.fn ticks), and plain `lv OP rv` then
 * writeback.  Two helpers — `bb_augop_compute` (pure compute given lv,
 * rv, op token) and `bb_augop_writeback` (write to TT_VAR slot / TT_IDX /
 * TT_FIELD lhs) — replace IR-mode's AUGOP_APPLY / AUGOP_CELL macros.
 * Unsupported tokens (TK_AUGPOW, TK_AUGCSET_*, TK_AUGSCAN) fall through
 * to the integer-add default — same coverage as IR-mode.
 *
 * RS-22e (2026-05-03): Fallthrough survey.  smoke_icon hits zero
 * unhandled kinds — the rung's stated gate is met.  Full Icon corpus
 * (271 programs) hits 16 distinct kinds totaling 62 fallthroughs, in
 * five categories: generators (TT_TO/TT_ALTERNATE/TT_ITERATE/TT_LIMIT/
 * TT_SEQ), string relops (TT_LEQ/TT_LNE/TT_LGE/TT_LLT plus untriggered
 * TT_LGT/TT_LLE peers), cset arithmetic (TT_CSET/TT_CSET_COMPL/_DIFF/
 * _INTER), and three mid-size kinds (TT_MAKELIST, TT_SCAN, TT_CASE).
 * Hardening the fallthrough to FAILDESCR causes 4 unified_broker FAILs
 * (notably palindrome.icn via TT_LNE), so per the rung the abort is
 * reverted; full inventory in docs/RS-22e-fallthrough-survey.md.
 * RS-22f-or-RS-23 closes the remaining 16 kinds; only after that can
 * the `interp_eval` extern be dropped (RS-23) and coro_value.c
 * promoted into the isolation gate.
 *
 * RS-22f-strrel (2026-05-03): Six string lexicographic relops lifted
 * in.  TT_LLT/TT_LLE/TT_LGT/TT_LGE/TT_LEQ/TT_LNE share `bb_strrel` — direct
 * mirror of `bb_numrel` using strcmp(VARVAL_fn(l), VARVAL_fn(r)).
 * Right-operand-on-success (Icon goal-directed convention).  Closes
 * 17 fallthroughs (TT_LEQ ×9, TT_LNE ×6, TT_LGE ×1, TT_LLT ×1) and the
 * two untriggered peers in the survey.  Removes palindrome.icn
 * unified_broker failure path and three peers — first sub-rung of
 * RS-22f.
 *
 * RS-22f-makelist (2026-05-03): TT_MAKELIST lifted in.  Mirrors
 * interp_eval.c:4051-4062 verbatim — first-sighting DEFDAT_fn
 * registration of `icnlist`, GC_malloc'd elem array, bb_eval_value
 * over each child (was interp_eval in IR mode), DATCON_fn returns
 * the DT_DATA descriptor.  Single fallthrough closed.  Second
 * sub-rung of RS-22f.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 * SPRINT:  RS-17a (2026-05-03), RS-22a (2026-05-03), RS-22b (2026-05-03), RS-22c (2026-05-03), RS-22d (2026-05-03), RS-22e (2026-05-03), RS-22f-strrel (2026-05-03), RS-22f-makelist (2026-05-03), RS-22f-cset (2026-05-03), RS-22f-generators (2026-05-03), RS-22f-stmt (2026-05-03)
 *==========================================================================================================================*/

#include "coro_value.h"
#include "coro_stmt.h"         /* RS-23c: bb_exec_stmt used in TT_EVERY body dispatch */
#include "coro_runtime.h"   /* FRAME, frame_depth, scan_pos, scan_subj, icn_descr_identical, g_lang, is_suspendable, coro_eval */
#include "../../driver/interp_private.h"  /* RS-22a: icn_call_builtin, icn_string_section_assign, set_and_trace, data_field_ptr, kw_assign; RS-22d: IcnTkKind via icon_lex.h */
#include "../common/coerce.h"             /* RS-22b: shared_arith */
#include "../x86/bb_broker.h"             /* RS-22d: α, β, bb_node_t for TT_AUGOP generator-RHS path */
#include "snobol4.h"
#include <string.h>
#include <gc/gc.h>

/* eval_node lives in src/runtime/x86/eval_code.c — IR-free expression evaluator. */
extern DESCR_t eval_node(tree_t *e);

/* GOAL-ICON-BB-COMPLETE A3-seed-fix: canonical Icon ?E LCG seed.
 * Shared with src/runtime/x86/sm_interp.c (SM ICN_RANDOM dispatch) and
 * src/driver/interp_eval.c (TT_RANDOM fallback path) so the three modes
 * advance one common sequence.  Initial value 12345UL preserves any test
 * baseline that was computed by the previous static-seed sites; constants
 * (Knuth MMIX) unchanged. */
unsigned long bb_icn_rnd_seed = 12345UL;

/* RS-23e (closes RS-23 arc): the `interp_eval` extern is gone.  Diag
 * (`scrip-rs23-diag` with `-Wl,--wrap=interp_eval`) verified zero IR
 * fallthrough from any BB-adapter ancestor across smoke +
 * unified_broker + full Icon corpus 263.  Any kind not handled by an
 * explicit case below is a four-mode isolation violation and aborts
 * with a diagnostic at the end of the function. */

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_arith — RS-22b arithmetic dispatch helper.
 *
 * Mirrors the SM_ADD..SM_EXP path in sm_interp.c (lines 327-353): propagate
 * DT_FAIL operands, coerce DT_S → DT_I via to_int, coerce DT_SNUL → INT(0),
 * then delegate to `shared_arith` in runtime/common/coerce.c.  One helper
 * instead of six near-identical cases — and the same code path SM mode uses.
 *----------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_arith(tree_t *e, sm_opcode_t op)
{
    if (e->n < 2) return FAILDESCR;
    DESCR_t l = bb_eval_value(e->c[0]);
    DESCR_t r = bb_eval_value(e->c[1]);
    if (l.v == DT_FAIL || r.v == DT_FAIL) return FAILDESCR;
    if (l.v == DT_S)    l = INTVAL(to_int(l));
    if (r.v == DT_S)    r = INTVAL(to_int(r));
    if (l.v == DT_SNUL) l = INTVAL(0);
    if (r.v == DT_SNUL) r = INTVAL(0);
    return shared_arith(l, r, op);
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_numrel — RS-22b numeric relational dispatch helper.
 *
 * Mirrors NUMREL macro in interp_eval.c (lines 3375-3384): both operands
 * coerce to double (DT_R direct, DT_I cast, anything else → 0); compare;
 * succeed → return RIGHT operand (Icon goal-directed convention; right
 * operand survives so `2 < (1 to 4)` filters generators); fail → FAILDESCR.
 *----------------------------------------------------------------------------------------------------------------------------*/
typedef enum { BBR_LT, BBR_LE, BBR_GT, BBR_GE, BBR_EQ, BBR_NE } bb_relop_t;

static DESCR_t bb_numrel(tree_t *e, bb_relop_t op)
{
    if (e->n < 2) return FAILDESCR;
    DESCR_t l = bb_eval_value(e->c[0]);
    DESCR_t r = bb_eval_value(e->c[1]);
    if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
    double lv = (l.v == DT_R) ? l.r : (double)(l.v == DT_I ? l.i : 0);
    double rv = (r.v == DT_R) ? r.r : (double)(r.v == DT_I ? r.i : 0);
    int ok;
    switch (op) {
    case BBR_LT: ok = (lv <  rv); break;
    case BBR_LE: ok = (lv <= rv); break;
    case BBR_GT: ok = (lv >  rv); break;
    case BBR_GE: ok = (lv >= rv); break;
    case BBR_EQ: ok = (lv == rv); break;
    case BBR_NE: ok = (lv != rv); break;
    default:     ok = 0;          break;
    }
    return ok ? r : FAILDESCR;
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_strrel — RS-22f-strrel string lexicographic relational dispatch helper.
 *
 * Mirrors STRREL macro in interp_eval.c (lines 3397-3407): both operands
 * resolved via VARVAL_fn (NULL → empty string), strcmp, succeed → return
 * RIGHT operand (Icon goal-directed convention; right operand survives so
 * generators can filter), fail → FAILDESCR.
 *----------------------------------------------------------------------------------------------------------------------------*/
typedef enum { BBS_LLT, BBS_LLE, BBS_LGT, BBS_LGE, BBS_LEQ, BBS_LNE } bb_strrelop_t;

static DESCR_t bb_strrel(tree_t *e, bb_strrelop_t op)
{
    if (e->n < 2) return FAILDESCR;
    DESCR_t l = bb_eval_value(e->c[0]);
    DESCR_t r = bb_eval_value(e->c[1]);
    if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
    const char *ls = VARVAL_fn(l); if (!ls) ls = "";
    const char *rs = VARVAL_fn(r); if (!rs) rs = "";
    int cmp = strcmp(ls, rs);
    int ok;
    switch (op) {
    case BBS_LLT: ok = (cmp <  0); break;
    case BBS_LLE: ok = (cmp <= 0); break;
    case BBS_LGT: ok = (cmp >  0); break;
    case BBS_LGE: ok = (cmp >= 0); break;
    case BBS_LEQ: ok = (cmp == 0); break;
    case BBS_LNE: ok = (cmp != 0); break;
    default:      ok = 0;          break;
    }
    return ok ? r : FAILDESCR;
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_str_concat — RS-22c string-concat helper (TT_CAT + TT_LCONCAT).
 *
 * Icon `||` (TT_CAT) and `|||` (TT_LCONCAT) both reach here via the BB
 * adapter.  Mirrors the IR-mode TT_LCONCAT case at interp_eval.c:4037 —
 * coerce numeric operands via descr_to_str_icn (round-trip-correct real
 * formatting), VARVAL_fn for everything else, GC_malloc'd concat.
 *
 * Pattern operands: do not occur in BB-engine call sites today (Icon
 * never produces them; SNOBOL4's pattern-context paths never reach
 * bb_eval_value).  If one ever did, descr_to_str_icn would fail-through
 * to FAILDESCR rather than producing garbage.
 *----------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_str_concat(tree_t *e)
{
    if (e->n < 2) return NULVCL;
    DESCR_t a = bb_eval_value(e->c[0]);
    DESCR_t b = bb_eval_value(e->c[1]);
    if (IS_FAIL_fn(a) || IS_FAIL_fn(b)) return FAILDESCR;
    DESCR_t as = descr_to_str_icn(a);
    DESCR_t bs = descr_to_str_icn(b);
    const char *asp = (as.v == DT_S || as.v == DT_SNUL) ? VARVAL_fn(as) : NULL;
    const char *bsp = (bs.v == DT_S || bs.v == DT_SNUL) ? VARVAL_fn(bs) : NULL;
    if (!asp) asp = "";
    if (!bsp) bsp = "";
    size_t al = strlen(asp), bl = strlen(bsp);
    char *buf = GC_malloc(al + bl + 1);
    memcpy(buf, asp, al);
    memcpy(buf + al, bsp, bl);
    buf[al + bl] = '\0';
    return STRVAL(buf);
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_section — RS-22c string section helper.
 *
 * Mirrors interp_eval.c:4070-4125 for TT_SECTION (s[i:j]), TT_SECTION_PLUS
 * (s[i+:n]), TT_SECTION_MINUS (s[i-:n]).  Icon position rules:
 *   p ≥ 1     → 1-based position (1 is before first char)
 *   p == 0    → position past last char (= slen+1)
 *   p < 0     → slen+1+p   (-1 → slen, -2 → slen-1, ...)
 * Out-of-bounds after normalization → FAILDESCR.
 *----------------------------------------------------------------------------------------------------------------------------*/
typedef enum { BBS_RANGE, BBS_PLUS, BBS_MINUS } bb_section_t;

static DESCR_t bb_section(tree_t *e, bb_section_t kind)
{
    if (e->n < 3) return NULVCL;
    DESCR_t sd = bb_eval_value(e->c[0]);
    if (IS_FAIL_fn(sd)) return FAILDESCR;
    const char *s = VARVAL_fn(sd);
    if (!s) s = "";
    int slen = (int)strlen(s);
    DESCR_t a1 = bb_eval_value(e->c[1]);
    DESCR_t a2 = bb_eval_value(e->c[2]);
    if (IS_FAIL_fn(a1) || IS_FAIL_fn(a2)) return FAILDESCR;
    int i = (int)to_int(a1);
    int x = (int)to_int(a2);   /* j for RANGE, n for PLUS/MINUS */
    if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
    int lo, hi;
    if (kind == BBS_RANGE) {
        if (x == 0) x = slen + 1; else if (x < 0) x = slen + 1 + x;
        if (i < 1 || i > slen+1 || x < 1 || x > slen+1) return FAILDESCR;
        lo = i < x ? i : x;
        hi = i < x ? x : i;
    } else if (kind == BBS_PLUS) {
        if (i < 1 || i > slen+1) return FAILDESCR;
        if (x >= 0) { lo = i;     hi = i + x; }
        else        { lo = i + x; hi = i;     }
        if (lo < 1 || hi > slen+1) return FAILDESCR;
    } else /* BBS_MINUS */ {
        if (i < 1 || i > slen+1) return FAILDESCR;
        if (x >= 0) { lo = i - x; hi = i;     }
        else        { lo = i;     hi = i - x; }
        if (lo < 1 || hi > slen+1) return FAILDESCR;
    }
    int len = hi - lo;
    char *buf = GC_malloc(len + 1);
    memcpy(buf, s + lo - 1, len);
    buf[len] = '\0';
    return STRVAL(buf);
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_augop_compute — RS-22d pure compute step for TT_AUGOP.
 *
 * Given (lv, rv, op_token), produce the augop result (FAILDESCR if
 * the op fails — e.g. division by zero, or a comparison-augop whose
 * predicate is false).  Mirrors the AUGOP_APPLY inner switch in
 * interp_eval.c:3740.  No writeback here — separate helper.
 *
 * Coverage: TK_AUGPLUS/MINUS/STAR/SLASH/MOD, TK_AUGCONCAT, the numeric
 * comparison-augops (=:=, ~=:=, <:=, <=:=, >:=, >=:=), the string
 * comparison-augops (==:=, ~==:=, <<:=, <<=:=, >>:=, >>=:=).
 * Unsupported tokens (TK_AUGPOW, TK_AUGCSET_*, TK_AUGSCAN) fall through
 * to integer-add default — same coverage as IR-mode.
 *----------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_augop_compute(DESCR_t lv, DESCR_t rv, IcnTkKind op)
{
    if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
    long li = IS_INT_fn(lv) ? lv.i : (long)lv.r;
    long ri = IS_INT_fn(rv) ? rv.i : (long)rv.r;
    switch (op) {
    case TK_AUGPLUS:   return INTVAL(li + ri);
    case TK_AUGMINUS:  return INTVAL(li - ri);
    case TK_AUGSTAR:   return INTVAL(li * ri);
    case TK_AUGSLASH:  return ri ? INTVAL(li / ri) : FAILDESCR;
    case TK_AUGMOD:    return ri ? INTVAL(li % ri) : FAILDESCR;
    case TK_AUGCONCAT: {
        const char *ls = VARVAL_fn(lv), *rs = VARVAL_fn(rv);
        if (!ls) ls = ""; if (!rs) rs = "";
        size_t ll = strlen(ls), rl = strlen(rs);
        char *buf = GC_malloc(ll + rl + 1);
        memcpy(buf, ls, ll); memcpy(buf + ll, rs, rl); buf[ll + rl] = '\0';
        return STRVAL(buf);
    }
    /* Numeric comparison-augops — `lv OP rv` evaluates the relation;
     * on success the augop result is rv (and writeback stores it),
     * on failure the augop fails (alternation `| fallback` runs). */
    case TK_AUGEQ: return (li == ri) ? rv : FAILDESCR;
    case TK_AUGNE: return (li != ri) ? rv : FAILDESCR;
    case TK_AUGLT: return (li <  ri) ? rv : FAILDESCR;
    case TK_AUGLE: return (li <= ri) ? rv : FAILDESCR;
    case TK_AUGGT: return (li >  ri) ? rv : FAILDESCR;
    case TK_AUGGE: return (li >= ri) ? rv : FAILDESCR;
    /* String comparison-augops — strcmp on string values. */
    case TK_AUGSEQ: case TK_AUGSNE:
    case TK_AUGSLT: case TK_AUGSLE: case TK_AUGSGT: case TK_AUGSGE: {
        const char *ls = VARVAL_fn(lv), *rs = VARVAL_fn(rv);
        if (!ls) ls = ""; if (!rs) rs = "";
        int cmp = strcmp(ls, rs);
        int ok = 0;
        switch (op) {
        case TK_AUGSEQ: ok = (cmp == 0); break;
        case TK_AUGSNE: ok = (cmp != 0); break;
        case TK_AUGSLT: ok = (cmp <  0); break;
        case TK_AUGSLE: ok = (cmp <= 0); break;
        case TK_AUGSGT: ok = (cmp >  0); break;
        case TK_AUGSGE: ok = (cmp >= 0); break;
        default:        break;
        }
        return ok ? rv : FAILDESCR;
    }
    default:           return INTVAL(li + ri);   /* same default as IR-mode */
    }
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_augop_writeback — RS-22d write augop result back to lhs.
 *
 * lhs may be TT_VAR (slot/global), TT_IDX (subscript), or TT_FIELD (record
 * field).  TT_VAR&-keyword left untouched for parity with IR-mode (the
 * AUGOP_APPLY macro never wrote back to keyword lvalues).  Callers
 * already checked !IS_FAIL_fn(res).
 *----------------------------------------------------------------------------------------------------------------------------*/
static void bb_augop_writeback(tree_t *lhs, DESCR_t res)
{
    if (!lhs) return;
    if (lhs->t == TT_VAR) {
        int slot = (int)lhs->v.ival;
        if (frame_depth > 0 && slot >= 0 && slot < FRAME.env_n)
            FRAME.env[slot] = res;
        else if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&')
            set_and_trace(lhs->v.sval, res);
    } else if (lhs->t == TT_IDX && lhs->n >= 2) {
        DESCR_t base = bb_eval_value(lhs->c[0]);
        DESCR_t idx  = bb_eval_value(lhs->c[1]);
        if (!IS_FAIL_fn(base) && !IS_FAIL_fn(idx))
            subscript_set(base, idx, res);
    } else if (lhs->t == TT_FIELD && lhs->v.sval && lhs->n >= 1) {
        DESCR_t obj = bb_eval_value(lhs->c[0]);
        if (!IS_FAIL_fn(obj)) {
            DESCR_t *cell = data_field_ptr(lhs->v.sval, obj);
            if (cell) *cell = res;
        }
    }
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_eval_value — evaluate e in value context.
 *
 * Value context means: produce a single DESCR_t result.  Generator-context
 * sub-expressions (TT_TO/TT_TO_BY/TT_ALTERNATE/TT_FNC user-proc/etc.) fall through
 * to interp_eval today; routing them through coro_eval + a single bb_broker
 * BB_ONCE pump is RS-17a-cont work.
 *
 * Icon-frame TT_VAR shim mirrors interp_eval.c lines 353-372: when frame_depth>0
 * we read scan/letter keywords directly and use slot-indexed locals from
 * FRAME.env[ival].  Outside an Icon frame, TT_VAR delegates to eval_node which
 * does the SNOBOL4 NV_GET_fn lookup.
 *----------------------------------------------------------------------------------------------------------------------------*/
DESCR_t bb_eval_value(tree_t *e)
{
    if (!e) return NULVCL;

    /* Icon-frame-aware TT_VAR: slot read when inside an Icon procedure call. */
    if (e->t == TT_VAR && frame_depth > 0) {
        if (e->v.sval && e->v.sval[0] == '&') {
            const char *kw = e->v.sval + 1;
            if (!strcmp(kw, "subject")) return scan_subj ? STRVAL(scan_subj) : NULVCL;
            if (!strcmp(kw, "pos"))     return INTVAL(scan_pos);
            if (!strcmp(kw, "letters")) return STRVAL("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
            if (!strcmp(kw, "ucase"))   return STRVAL("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
            if (!strcmp(kw, "lcase"))   return STRVAL("abcdefghijklmnopqrstuvwxyz");
            if (!strcmp(kw, "digits"))  return STRVAL("0123456789");
            if (!strcmp(kw, "null"))    return NULVCL;
            if (!strcmp(kw, "fail"))    return FAILDESCR;
            return NULVCL;
        }
        int slot = (int)e->v.ival;
        if (slot >= 0 && slot < FRAME.env_n) return FRAME.env[slot];
        if (slot < 0 && e->v.sval && e->v.sval[0] != '&') return NV_GET_fn(e->v.sval);
        return NULVCL;
    }

    /* Kinds that eval_node already handles identically for SNOBOL4 and Icon
     * outside-of-frame: literals, &keywords, NULVCL.  TT_VAR outside an Icon
     * frame goes through eval_node's NV_GET_fn path. */
    switch (e->t) {
    case TT_ILIT:
    case TT_FLIT:
    case TT_QLIT:
    case TT_NUL:
    case TT_KEYWORD:
        return eval_node(e);
    case TT_VAR:
        /* frame_depth == 0: SNOBOL4-style lookup via eval_node */
        return eval_node(e);

    /* RS-22a: TT_ASSIGN — slot store, IDX, FIELD, ITERATE, RANDOM lvalue paths.
     * Mirrors interp_eval.c lines 373-474; all interp_eval(child) replaced
     * with bb_eval_value(child). */
    case TT_ASSIGN: {
        if (e->n < 2) return NULVCL;
        DESCR_t val = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        tree_t *lhs = e->c[0];
        if (lhs && (lhs->t == TT_SECTION || lhs->t == TT_SECTION_PLUS ||
                    lhs->t == TT_SECTION_MINUS)) {
            if (icn_string_section_assign(lhs, val)) return val;
            return FAILDESCR;
        }
        if (lhs && lhs->t == TT_IDX && lhs->n >= 2) {
            if (icn_string_section_assign(lhs, val)) return val;
            { DESCR_t _b = bb_eval_value(lhs->c[0]);
              if (_b.v == DT_S || _b.v == DT_SNUL) return FAILDESCR; }
        }
        if (lhs && lhs->t == TT_VAR) {
            if (lhs->v.sval && lhs->v.sval[0] == '&') {
                if (!kw_assign(lhs->v.sval + 1, val)) return FAILDESCR;
                return val;
            }
            int slot = (int)lhs->v.ival;
            if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; return val; }
            if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') set_and_trace(lhs->v.sval, val);
        } else if (lhs && lhs->t == TT_IDX && lhs->n >= 2) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(base)) {
                DESCR_t idx = bb_eval_value(lhs->c[1]);
                if (!IS_FAIL_fn(idx)) subscript_set(base, idx, val);
            }
        } else if (lhs && lhs->t == TT_FIELD && lhs->v.sval && lhs->n >= 1) {
            DESCR_t obj = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(obj)) {
                DESCR_t *cell = data_field_ptr(lhs->v.sval, obj);
                if (cell) *cell = val;
            }
        } else if (lhs && lhs->t == TT_ITERATE && lhs->n >= 1) {
            DESCR_t cv = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(cv)) {
                if (cv.v == DT_T && cv.tbl) {
                    for (int b = 0; b < TABLE_BUCKETS; b++)
                        for (TBPAIR_t *p = cv.tbl->buckets[b]; p; p = p->next)
                            p->val = val;
                } else if (cv.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(cv, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        DESCR_t ea = FIELD_GET_fn(cv, "frame_elems");
                        int n = (int)FIELD_GET_fn(cv, "frame_size").i;
                        DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                        if (elems && n > 0) for (int i = 0; i < n; i++) elems[i] = val;
                    } else if (cv.u && cv.u->type && cv.u->type->nfields > 0 && cv.u->fields) {
                        for (int i = 0; i < cv.u->type->nfields; i++) cv.u->fields[i] = val;
                    }
                }
            }
        }
        return val;
    }

    /* RS-22a: TT_FNC — user-proc path dispatches through proc_table → coro_call.
     * Builtin path evaluates args through bb_eval_value then calls icn_call_builtin
     * (already IR-free).  Mirror of interp_eval.c TT_FNC case but recursion-safe. */
    case TT_FNC: {
        if (e->n < 1) return NULVCL;
        const char *fn = e->c[0] ? e->c[0]->v.sval : NULL;
        if (!fn) return NULVCL;
        int nargs = e->n - 1;
        /* RS-23a-raku: Raku block-receiving builtins (raku_try / raku_map /
         * raku_grep / raku_sort) need raw tree_t access and must NOT be subject
         * to FAIL-prop on the body argument — dispatch them here, before the
         * generic user-proc / builtin pre-eval loops below.  raku_try_call_builtin
         * returns 1 if `fn` matched a Raku builtin (and *out is set); 0 means
         * not a Raku builtin and we fall through to the existing dispatch. */
        {
            DESCR_t __rk_d;
            if (raku_try_call_builtin(e, &__rk_d)) return __rk_d;
        }
        /* User-proc path: look up in proc_table and call via proc_table_call
         * (CH-17g-call-sites: dispatches via SM expression when entry_pc resolved). */
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, fn) != 0) continue;
            DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
            for (int j = 0; j < nargs; j++) args[j] = bb_eval_value(e->c[1+j]);
            DESCR_t result = proc_table_call(i, args, nargs);
            return result;
        }
        /* Builtin path: evaluate args through bb_eval_value, then icn_call_builtin. */
        DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
        for (int j = 0; j < nargs; j++) {
            args[j] = bb_eval_value(e->c[1+j]);
            if (IS_FAIL_fn(args[j])) return FAILDESCR;
        }
        return icn_call_builtin(e, args, nargs);
    }

    /* RS-22b: arithmetic binops — bb_arith handles FAIL prop, string→int,
     * SNUL→0, then dispatches via shared_arith (the same path SM mode uses).
     * TT_POW: shared_arith maps SM_EXP → integer for non-negative int^int,
     * else REALVAL(pow(...)).  This matches Icon `^` (always real if any
     * operand is real) and SNOBOL4 `**` (int when both ints, exp >= 0). */
    case TT_ADD: return bb_arith(e, SM_ADD);
    case TT_SUB: return bb_arith(e, SM_SUB);
    case TT_MUL: return bb_arith(e, SM_MUL);
    case TT_DIV: return bb_arith(e, SM_DIV);
    case TT_MOD: return bb_arith(e, SM_MOD);
    case TT_POW: return bb_arith(e, SM_EXP);

    /* RS-22b: numeric relational binops — succeed → return right operand,
     * fail → FAILDESCR.  Right-operand-on-success is Icon goal-directed
     * convention (lets `2 < (1 to 4)` filter a generator chain). */
    case TT_LT: return bb_numrel(e, BBR_LT);
    case TT_LE: return bb_numrel(e, BBR_LE);
    case TT_GT: return bb_numrel(e, BBR_GT);
    case TT_GE: return bb_numrel(e, BBR_GE);
    case TT_EQ: return bb_numrel(e, BBR_EQ);
    case TT_NE: return bb_numrel(e, BBR_NE);

    /* RS-22b: deep identity (Icon `===`).  Right-operand-on-success again.
     * Note: `~===` does NOT lower to a distinct EXPR kind — Icon's parser
     * emits TT_NOT(TT_IDENTICAL(a, b)).  When TT_NOT is lifted (RS-22d), the
     * full `~===` path becomes IR-free; until then TT_NOT still falls through
     * to interp_eval and walks back here for its TT_IDENTICAL child. */
    case TT_IDENTICAL: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = bb_eval_value(e->c[0]);
        DESCR_t r = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return icn_descr_identical(l, r) ? r : FAILDESCR;
    }

    /* RS-22f-strrel: lexicographic (string) relational binops — succeed →
     * return right operand, fail → FAILDESCR.  Mirror of bb_numrel for
     * Icon string comparison operators (==, ~==, <<, <<=, >>, >>=). */
    case TT_LLT: return bb_strrel(e, BBS_LLT);
    case TT_LLE: return bb_strrel(e, BBS_LLE);
    case TT_LGT: return bb_strrel(e, BBS_LGT);
    case TT_LGE: return bb_strrel(e, BBS_LGE);
    case TT_LEQ: return bb_strrel(e, BBS_LEQ);
    case TT_LNE: return bb_strrel(e, BBS_LNE);

    /* RS-22c: string concat — Icon `||` (TT_CAT) and `|||` (TT_LCONCAT)
     * share bb_str_concat.  In Icon BB context neither produces patterns,
     * so the simple coerce-and-concat path is correct (mirrors IR-mode
     * TT_LCONCAT at interp_eval.c:4037).  SNOBOL4 TT_CAT in pattern context
     * does not arrive here — that path is interp_eval_pat-only. */
    case TT_CAT:
    case TT_LCONCAT:
        return bb_str_concat(e);

    /* RS-22c: subscript read — table/list/record/string index.
     * Two-arg form → subscript_get; three-arg form (s[i:j] lowered as
     * TT_IDX with two index children) → subscript_get2.  Mirrors
     * interp_eval.c:3084. */
    case TT_IDX: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t base = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(base)) return FAILDESCR;
        if (e->n == 2) {
            DESCR_t idx = bb_eval_value(e->c[1]);
            if (IS_FAIL_fn(idx)) return FAILDESCR;
            return subscript_get(base, idx);
        }
        DESCR_t i1 = bb_eval_value(e->c[1]);
        DESCR_t i2 = bb_eval_value(e->c[2]);
        if (IS_FAIL_fn(i1) || IS_FAIL_fn(i2)) return FAILDESCR;
        return subscript_get2(base, i1, i2);
    }

    /* RS-22c: record field read.  e->v.sval = field name, child[0] = object. */
    case TT_FIELD: {
        if (!e->v.sval || e->n < 1) return NULVCL;
        DESCR_t obj = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(obj)) return FAILDESCR;
        DESCR_t *cell = data_field_ptr(e->v.sval, obj);
        if (!cell) return FAILDESCR;
        return *cell;
    }

    /* RS-22c: string section (Icon s[i:j], s[i+:n], s[i-:n]) — bb_section.
     * Three minor variants of bound computation share one helper. */
    case TT_SECTION:        return bb_section(e, BBS_RANGE);
    case TT_SECTION_PLUS:   return bb_section(e, BBS_PLUS);
    case TT_SECTION_MINUS:  return bb_section(e, BBS_MINUS);

    /* RS-22f-makelist: Icon `[e1, e2, ...]` list constructor.  Mirrors
     * interp_eval.c:4051-4062 — register `icnlist` DATA type once on
     * first sighting, GC_malloc the elem array, evaluate each child via
     * bb_eval_value (was interp_eval in IR mode), then DATCON_fn to
     * build the DT_DATA descriptor.  The static-flag idiom matches IR
     * mode exactly: DEFDAT_fn is called at most once per process. */
    case TT_MAKELIST: {
        int n = e->n;
        static int icnlist_registered = 0;
        if (!icnlist_registered) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_registered = 1; }
        DESCR_t *elems = GC_malloc((n > 0 ? n : 1) * sizeof(DESCR_t));
        for (int i = 0; i < n; i++) elems[i] = bb_eval_value(e->c[i]);
        DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void*)elems;
        return DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
    }

    /* RS-22d: unary minus / plus.  Mirrors interp_eval.c:2501-2540.
     * TT_PLS is more elaborate than `pos()` — try integer parse first,
     * then real, fall back to INTVAL(0).  Match exactly. */
    case TT_MNS: {
        if (e->n < 1) return FAILDESCR;
        return neg(bb_eval_value(e->c[0]));
    }
    case TT_PLS: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (IS_INT_fn(v) || IS_REAL_fn(v)) return v;
        const char *s = VARVAL_fn(v);
        if (!s || !*s) return INTVAL(0);
        char *end = NULL;
        long long iv = strtoll(s, &end, 10);
        if (end && *end == '\0') return INTVAL(iv);
        double dv = strtod(s, &end);
        if (end && *end == '\0') return REALVAL(dv);
        return INTVAL(0);
    }

    /* RS-22d: boolean not.  `not E` succeeds with &null iff E fails;
     * fails iff E succeeds.  Mirrors interp_eval.c:3124. */
    case TT_NOT: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return NULVCL;
        return FAILDESCR;
    }

    /* RS-22d: `/E` succeed-if-null. Mirrors interp_eval.c:3629. */
    case TT_NULL: {
        if (e->n < 1) return NULVCL;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return NULVCL;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return NULVCL;
        return FAILDESCR;
    }

    /* RS-22d: `\E` succeed-if-non-null. Mirrors interp_eval.c:3646. */
    case TT_NONNULL: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return FAILDESCR;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return FAILDESCR;
        return v;
    }

    /* RS-22d: `*E` size — string/list/table.  Mirrors interp_eval.c:3133. */
    case TT_SIZE: {
        if (e->n < 1) return INTVAL(0);
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_T) return INTVAL(v.tbl ? v.tbl->size : 0);
        if (v.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(v, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0)
                return INTVAL((int)FIELD_GET_fn(v, "frame_size").i);
        }
        if (IS_INT_fn(v)) return INTVAL(0);
        if (IS_REAL_fn(v)) return INTVAL(0);
        const char *s = VARVAL_fn(v);
        if (!s) return INTVAL(0);
        if (strchr(s, '\x01')) {     /* SOH-delimited Raku/Icon array */
            long n = 1;
            for (const char *p = s; *p; p++) if (*p == '\x01') n++;
            return INTVAL(n);
        }
        long len = v.slen > 0 ? v.slen : (long)strlen(s);
        return INTVAL(len);
    }

    /* RS-22d: `?E` random selector. Mirrors interp_eval.c:3659.
     * Uses local static LCG (Knuth MMIX constants).  NOTE: this RNG
     * state is independent from interp_eval's — if a program runs
     * partly through bb_eval_value and partly through interp_eval,
     * the two RNGs interleave separately.  In pure BB (post-RS-22e)
     * this becomes the single source. */
    case TT_RANDOM: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        /* GOAL-ICON-BB-COMPLETE A3-seed-fix: single canonical RNG seed shared
         * with sm_interp.c::ICN_RANDOM and interp_eval.c::TT_RANDOM so the
         * three modes (ir-run, sm-run, interp_eval fallback) produce
         * identical sequences for random programs. */
        bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
        unsigned long _rnd = bb_icn_rnd_seed >> 33;
        if (v.v == DT_T) {
            if (!v.tbl || v.tbl->size <= 0) return FAILDESCR;
            int target = (int)(_rnd % (unsigned long)v.tbl->size);
            int seen = 0;
            for (int b = 0; b < TABLE_BUCKETS; b++) {
                for (TBPAIR_t *p = v.tbl->buckets[b]; p; p = p->next) {
                    if (seen == target) return p->val;
                    seen++;
                }
            }
            return FAILDESCR;
        }
        if (v.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(v, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                int n = (int)FIELD_GET_fn(v, "frame_size").i;
                if (n <= 0) return FAILDESCR;
                DESCR_t ea = FIELD_GET_fn(v, "frame_elems");
                if (ea.v != DT_DATA || !ea.ptr) return FAILDESCR;
                DESCR_t *elems = (DESCR_t *)ea.ptr;
                return elems[_rnd % (unsigned long)n];
            }
            if (v.u && v.u->type && v.u->type->nfields > 0 && v.u->fields) {
                int n = v.u->type->nfields;
                return v.u->fields[_rnd % (unsigned long)n];
            }
            return FAILDESCR;
        }
        if (IS_INT_fn(v)) {
            long long n = v.i;
            if (n <= 0) return FAILDESCR;
            return INTVAL((long long)((_rnd % (unsigned long)n) + 1));
        }
        if (v.v == DT_SNUL) return FAILDESCR;
        const char *s = VARVAL_fn(v);
        if (!s || !*s) return FAILDESCR;
        long slen = v.slen > 0 ? v.slen : (long)strlen(s);
        if (slen <= 0) return FAILDESCR;
        char *out = (char *)GC_malloc(2);
        out[0] = s[_rnd % (unsigned long)slen];
        out[1] = '\0';
        return STRVAL(out);
    }

    /* RS-22d: augmented assignment.  Three execution paths mirroring
     * interp_eval.c:3729-3870:
     *   (1) `!container OP:= rhs`  (lhs is TT_ITERATE) — bang-iterate,
     *       apply OP to every cell of T/list/record in place.
     *   (2) RHS suspendable          — drive via coro_eval+bb_node_t,
     *       apply OP per tick, re-reading lhs each tick.  Implements
     *       `every sum +:= (1 to n)`.
     *   (3) Plain                     — single lv OP rv, then writeback.
     * `bb_augop_compute` is the shared compute step; `bb_augop_writeback`
     * is the shared writeback (TT_VAR slot / TT_IDX / TT_FIELD). */
    case TT_AUGOP: {
        if (e->n < 2) return NULVCL;
        tree_t *lhs = e->c[0];
        tree_t *rhs = e->c[1];
        IcnTkKind op = (IcnTkKind)e->v.ival;
        DESCR_t result = NULVCL;

        /* (1) `!container OP:= rhs` — bang-iterate lvalue. */
        if (lhs && lhs->t == TT_ITERATE && lhs->n >= 1) {
            DESCR_t cv = bb_eval_value(lhs->c[0]);
            DESCR_t rv = bb_eval_value(rhs);
            if (IS_FAIL_fn(cv) || IS_FAIL_fn(rv)) return FAILDESCR;
            #define BB_AUGOP_CELL(cell_) do { \
                DESCR_t _r = bb_augop_compute((cell_), rv, op); \
                if (!IS_FAIL_fn(_r)) { (cell_) = _r; result = _r; } \
            } while (0)
            if (cv.v == DT_T && cv.tbl) {
                for (int b = 0; b < TABLE_BUCKETS; b++)
                    for (TBPAIR_t *p = cv.tbl->buckets[b]; p; p = p->next)
                        BB_AUGOP_CELL(p->val);
            } else if (cv.v == DT_DATA) {
                DESCR_t tag = FIELD_GET_fn(cv, "icn_type");
                if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                    DESCR_t ea = FIELD_GET_fn(cv, "frame_elems");
                    int n = (int)FIELD_GET_fn(cv, "frame_size").i;
                    DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                    if (elems && n > 0)
                        for (int i = 0; i < n; i++) BB_AUGOP_CELL(elems[i]);
                } else if (cv.u && cv.u->type && cv.u->type->nfields > 0 && cv.u->fields) {
                    for (int i = 0; i < cv.u->type->nfields; i++)
                        BB_AUGOP_CELL(cv.u->fields[i]);
                }
            }
            #undef BB_AUGOP_CELL
            return result;
        }

        /* (2) RHS suspendable — drive generator, apply OP per tick. */
        if (rhs && is_suspendable(rhs)) {
            bb_node_t rbox = coro_eval(rhs);
            DESCR_t tick = rbox.fn(rbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.loop_break && !FRAME.returning) {
                DESCR_t cur_lv = bb_eval_value(lhs);
                DESCR_t res    = bb_augop_compute(cur_lv, tick, op);
                if (!IS_FAIL_fn(res)) {
                    bb_augop_writeback(lhs, res);
                    result = res;
                }
                tick = rbox.fn(rbox.ζ, β);
            }
            return result;
        }

        /* (3) Plain — single compute + writeback. */
        DESCR_t lv = bb_eval_value(lhs);
        DESCR_t rv = bb_eval_value(rhs);
        DESCR_t res = bb_augop_compute(lv, rv, op);
        if (!IS_FAIL_fn(res)) {
            bb_augop_writeback(lhs, res);
            result = res;
        }
        return result;
    }

    /* RS-22f-generators (2026-05-03): Generator kinds in value context.
     *
     * TT_TO / TT_TO_BY / TT_ITERATE / TT_LIMIT / TT_ALTERNATE / TT_SEQ_EXPR are
     * generators.  The contract has THREE cases that must be distinguished:
     *
     * (1) The node is currently being driven by an outer pump.  An every-loop
     *     above us pushed the TT_TO onto FRAME's gen stack; the body re-reads
     *     the node to fetch the current tick value.  E.g.:
     *         every write("ICN: " || (1 to 3))
     *     drives TT_FNC(write) which drives TT_CAT which evaluates TT_TO.  The
     *     TT_TO is on FRAME's gen stack with cur = 1, 2, 3 successively.
     *     Mirrors interp_eval.c:3470 — return INTVAL(cur) from the frame.
     *
     * (2) coro_drive_node injection: an outer driver staged a value for this
     *     exact node (find_leaf_suspendable + coro_drive_val).  Return the
     *     staged value directly.  Mirrors interp_eval.c:348.
     *
     * (3) Fresh evaluation in value context: build a bb_node_t via coro_eval
     *     and call fn(ζ, α) once.  This is **first-value semantics**: if the
     *     generator is empty, FAILDESCR; otherwise the first value it yields.
     *     Used for stand-alone first-value contexts like `if (1 to n) > k`
     *     where the test is generative but no outer every is pumping.
     *     Mirrors RS-22d's TT_AUGOP generator-RHS path (lines 784-794) and
     *     interp_eval.c's TT_IF goal-directed test (line 2386-2392).
     *
     * TT_SEQ is `&` conjunction in value context: evaluate children
     * left-to-right; FAILDESCR on first failure; return last child on full
     * success.  Mirrors interp_eval.c:2348 icn-frame switch case. */
    case TT_TO:
    case TT_TO_BY: {
        /* (2) injection check — outer driver staged a value */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* (1) frame check — outer pump is iterating this node */
        long cur;
        if (icn_frame_lookup(e, &cur)) return INTVAL(cur);
        /* (3) fresh first-value via coro_eval+α */
        bb_node_t box = coro_eval(e);
        return box.fn(box.ζ, α);
    }

    case TT_ITERATE: {
        /* (2) injection check */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* (1) frame check — !L pump pushes TT_ITERATE onto frame as the
         * generator node; current cur is the index, sval is the string for
         * char-iter or the list bucket.  Mirrors interp_eval.c TT_ITERATE in
         * the icn-frame switch. */
        long cur; const char *sv;
        if (icn_frame_lookup_sv(e, &cur, &sv)) {
            if (sv) {
                /* char-iteration: return single-char string at cur */
                static char buf[2];
                buf[0] = sv[cur];
                buf[1] = '\0';
                return STRVAL(buf);
            }
            /* numeric iteration */
            return INTVAL(cur);
        }
        /* (3) fresh first-value */
        bb_node_t box = coro_eval(e);
        return box.fn(box.ζ, α);
    }

    case TT_LIMIT:
    case TT_ALTERNATE:
    case TT_SEQ_EXPR: {
        /* (2) injection check */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* These three don't carry per-tick scalar state on FRAME.gen the way
         * TT_TO/TT_ITERATE do; they are pure generator combinators whose
         * internal state lives in the bb_node_t.  Outer-pump retries reach
         * them through the bb_node_t's β path, not through re-entry of
         * bb_eval_value.  So fresh α-once is correct here.
         * (3) fresh first-value via coro_eval+α */
        bb_node_t box = coro_eval(e);
        return box.fn(box.ζ, α);
    }

    case TT_SEQ: {
        if (e->n == 0) return NULVCL;
        DESCR_t last = NULVCL;
        for (int i = 0; i < e->n; i++) {
            last = bb_eval_value(e->c[i]);
            if (IS_FAIL_fn(last)) return FAILDESCR;
            if (FRAME.returning || FRAME.loop_break || FRAME.loop_next) break;
        }
        return last;
    }

    /* RS-22f-stmt (2026-05-03): TT_SCAN and TT_CASE in value context.
     *
     * TT_SCAN: `subj ? body` in Icon/Prolog mode.  bb_eval_value is only
     * reached from BB-engine call sites (Icon every-bodies, Prolog
     * clause-bodies), so the SNOBOL4-mode exec_stmt branch in
     * interp_eval.c:3887 is unreachable here — the Icon/Prolog branch
     * (line 3899) is what we mirror.  Push current scan state, evaluate
     * subject as string, install as new scan target with pos=1, evaluate
     * body, restore prior scan state.
     *
     * TT_CASE: case-expression in value context.  Evaluate topic; walk
     * pairs (Icon) or triples (Raku) comparing topic to each value;
     * return the matching body's value, or the default body's value, or
     * NULVCL.  Mirrors interp_eval.c:3569 verbatim — only `interp_eval`
     * recursive calls are replaced with `bb_eval_value`. */
    case TT_SCAN: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t subj_d = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(subj_d)) return FAILDESCR;
        const char *subj_s = VARVAL_fn(subj_d); if (!subj_s) subj_s = "";
        if (scan_depth < SCAN_STACK_MAX) {
            scan_stack[scan_depth].subj = scan_subj;
            scan_stack[scan_depth].pos  = scan_pos;
            scan_depth++;
        }
        scan_subj = subj_s; scan_pos = 1;
        DESCR_t r = (e->n >= 2) ? bb_eval_value(e->c[1]) : NULVCL;
        if (scan_depth > 0) {
            scan_depth--;
            scan_subj = scan_stack[scan_depth].subj;
            scan_pos  = scan_stack[scan_depth].pos;
        }
        return r;
    }

    case TT_CASE: {
        if (e->n < 1) return NULVCL;
        DESCR_t topic = bb_eval_value(e->c[0]);
        /* Detect Raku triple layout: (nchildren-1)%3==0 AND child[1] is
         * TT_ILIT or TT_NUL (the comparison-kind marker). */
        int is_raku_layout = (e->n >= 4 && (e->n - 1) % 3 == 0 &&
            e->c[1] && (e->c[1]->t == TT_ILIT || e->c[1]->t == TT_NUL));
        if (is_raku_layout) {
            int i = 1;
            while (i + 2 < e->n) {
                tree_t *cmpnode = e->c[i];
                tree_t *val     = e->c[i+1];
                tree_t *body    = e->c[i+2];
                i += 3;
                if (cmpnode->t == TT_NUL) return bb_eval_value(body);
                tree_e cmp = (tree_e)(cmpnode->v.ival);
                DESCR_t wval = bb_eval_value(val);
                int match = 0;
                if (cmp == TT_LEQ) {
                    const char *ts = IS_STR_fn(topic)?topic.s:VARVAL_fn(topic);
                    const char *ws = IS_STR_fn(wval) ?wval.s :VARVAL_fn(wval);
                    match = (ts && ws && strcmp(ts, ws) == 0);
                } else {
                    if (IS_INT_fn(topic) && IS_INT_fn(wval)) match = (topic.i == wval.i);
                    else { const char *ts=VARVAL_fn(topic), *ws=VARVAL_fn(wval); match=(ts && ws && strcmp(ts,ws)==0); }
                }
                if (match) return bb_eval_value(body);
            }
            if (i+1 < e->n && e->c[i]->t == TT_NUL)
                return bb_eval_value(e->c[i+1]);
            return NULVCL;
        }
        /* Icon: pairs [val, body] then optional trailing default body */
        int nc = e->n;
        int i = 1;
        while (i + 1 < nc) {
            DESCR_t wval = bb_eval_value(e->c[i]);
            tree_t *body = e->c[i+1];
            i += 2;
            int match;
            if (IS_INT_fn(topic) && IS_INT_fn(wval)) match = (topic.i == wval.i);
            else {
                const char *ts = VARVAL_fn(topic), *ws = VARVAL_fn(wval);
                match = (ts && ws && strcmp(ts, ws) == 0);
            }
            if (match) return bb_eval_value(body);
        }
        /* trailing default body (odd remaining child count) */
        if (i < nc) return bb_eval_value(e->c[i]);
        return NULVCL;
    }

    /* RS-22f-cset (2026-05-03): Cset literal + four set-arithmetic ops.
     * Icon csets are represented as NUL-terminated strings of member chars.
     * The four binary ops delegate to the icn_cset_* helpers in icon_runtime.c
     * (now declared in coro_runtime.h).  TT_CSET literal mirrors interp_eval.c:3466. */
    case TT_CSET:
        return e->v.sval ? STRVAL(e->v.sval) : NULVCL;

    case TT_CSET_COMPL: {
        DESCR_t operand = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(operand)) return FAILDESCR;
        const char *cs = IS_NULL_fn(operand) ? "" : VARVAL_fn(operand);
        return STRVAL(icn_cset_complement(cs));
    }

    case TT_CSET_UNION: {
        DESCR_t lv = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        const char *a = IS_NULL_fn(lv) ? "" : VARVAL_fn(lv);
        const char *b = IS_NULL_fn(rv) ? "" : VARVAL_fn(rv);
        return STRVAL(icn_cset_union(a, b));
    }

    case TT_CSET_DIFF: {
        DESCR_t lv = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        const char *a = IS_NULL_fn(lv) ? "" : VARVAL_fn(lv);
        const char *b = IS_NULL_fn(rv) ? "" : VARVAL_fn(rv);
        return STRVAL(icn_cset_diff(a, b));
    }

    case TT_CSET_INTER: {
        DESCR_t lv = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        const char *a = IS_NULL_fn(lv) ? "" : VARVAL_fn(lv);
        const char *b = IS_NULL_fn(rv) ? "" : VARVAL_fn(rv);
        return STRVAL(icn_cset_inter(a, b));
    }

    /* RS-23d: TT_WHILE — Icon while loop in value context.
     * Returns NULVCL after normal exit; mirrors interp_eval.c:1719-1730 with
     * interp_eval(child) replaced by bb_eval_value (cond) and bb_exec_stmt (body). */
    case TT_WHILE: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            DESCR_t cv = (e->n > 0) ? bb_eval_value(e->c[0]) : FAILDESCR;
            if (IS_FAIL_fn(cv)) break;
            FRAME.loop_next = 0;
            if (e->n > 1) bb_exec_stmt(e->c[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return NULVCL;
    }

    /*========================================================================
     * RS-23c: TT_EVERY, TT_INITIAL, TT_SWAP — missing from both adapters.
     * Value-context implementations mirror interp_eval.c's icon-frame switch,
     * with interp_eval(child) replaced by bb_eval_value(child) / bb_exec_stmt.
     *======================================================================*/

    /* TT_EVERY — drive a generator, run optional body per tick.  Returns NULVCL.
     * Three sub-cases mirror interp_eval.c:1639-1750:
     *   1. TT_ASSIGN with generative RHS — re-evaluate assignment per tick.
     *   2. TT_SEQ conjunction — drive filter, execute seq body per tick.
     *   3. Generic — drive gen via coro_eval box, run body per tick. */
    case TT_EVERY: {
        if (e->n < 1) return NULVCL;
        tree_t *gen  = e->c[0];
        tree_t *body = (e->n > 1) ? e->c[1] : NULL;
        if (gen->t == TT_ASSIGN &&
            gen->n >= 2 && is_suspendable(gen->c[1])) {
            tree_t *leaf = find_leaf_suspendable(gen->c[1]);
            if (!leaf) leaf = gen->c[1];
            bb_node_t rbox = coro_eval(leaf);
            DESCR_t tick = rbox.fn(rbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.returning && !FRAME.loop_break) {
                FRAME.loop_next = 0;
                coro_drive_node = leaf;
                coro_drive_val  = tick;
                (void)bb_eval_value(gen);
                coro_drive_node = NULL;
                if (body) bb_exec_stmt(body);
                if (FRAME.returning || FRAME.loop_break) break;
                tick = rbox.fn(rbox.ζ, β);
            }
            FRAME.loop_break = 0;
            FRAME.loop_next  = 0;
            return NULVCL;
        }
        if (gen->t == TT_SEQ && gen->n >= 2 && is_suspendable(gen->c[0])) {
            tree_t *filter = gen->c[0];
            bb_node_t fbox = coro_eval(filter);
            DESCR_t tick = fbox.fn(fbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.returning && !FRAME.loop_break) {
                FRAME.loop_next = 0;
                for (int _si = 1; _si < gen->n; _si++) bb_exec_stmt(gen->c[_si]);
                if (body) bb_exec_stmt(body);
                if (FRAME.returning || FRAME.loop_break) break;
                tick = fbox.fn(fbox.ζ, β);
            }
            FRAME.loop_break = 0;
            FRAME.loop_next  = 0;
            return NULVCL;
        }
        bb_node_t box = coro_eval(gen);
        int caller_depth = frame_depth;
        DESCR_t val = box.fn(box.ζ, α);
        while (!IS_FAIL_fn(val) && !FRAME.returning && !FRAME.loop_break) {
            FRAME.loop_next = 0;
            if (gen->v.sval && *gen->v.sval && caller_depth >= 1) {
                IcnFrame *cf = &frame_stack[caller_depth - 1];
                int slot = scope_get(&cf->sc, gen->v.sval);
                if (slot >= 0 && slot < cf->env_n) cf->env[slot] = val;
                else NV_SET_fn(gen->v.sval, val);
            }
            if (body) {
                frame_push(gen, val.v == DT_I ? val.i : 0, val.v == DT_I ? NULL : val.s);
                int saved_depth = frame_depth;
                frame_depth = caller_depth;
                bb_exec_stmt(body);
                frame_depth = saved_depth;
                frame_pop();
            }
            if (FRAME.returning || FRAME.loop_break) break;
            val = box.fn(box.ζ, β);
        }
        FRAME.loop_break = 0;
        FRAME.loop_next  = 0;
        return NULVCL;
    }

    /* TT_INITIAL — once-only block; side-effects only, returns NULVCL.
     * Mirrors interp_eval.c:3558-3601; interp_eval(child) → bb_eval_value(child). */
    case TT_INITIAL: {
        IcnInitEnt *ent = NULL;
        for (int _i = 0; _i < icn_init_n; _i++)
            if (init_tab[_i].id == e->_id) { ent = &init_tab[_i]; break; }
        if (!ent) {
            for (int i = 0; i < e->n; i++) (void)bb_eval_value(e->c[i]);
            if (icn_init_n < ICN_INIT_MAX) {
                ent = &init_tab[icn_init_n++];
                ent->id = e->_id; ent->ns = 0;
                for (int i = 0; i < e->n && ent->ns < ICN_INIT_SLOTS; i++) {
                    tree_t *ch = e->c[i];
                    if (!ch || ch->t != TT_ASSIGN || ch->n < 1) continue;
                    tree_t *lhs = ch->c[0];
                    if (!lhs || lhs->t != TT_VAR || !lhs->v.sval) continue;
                    IcnInitSlot *sl = &ent->s[ent->ns++];
                    strncpy(sl->nm, lhs->v.sval, 63); sl->nm[63] = '\0';
                    if (frame_depth > 0 && lhs->v.ival >= 0 && lhs->v.ival < FRAME.env_n)
                        sl->val = FRAME.env[lhs->v.ival];
                    else
                        sl->val = NV_GET_fn(lhs->v.sval);
                }
            }
            e->v.ival = 1;
        } else {
            for (int si = 0; si < ent->ns; si++) {
                int restored = 0;
                if (frame_depth > 0) {
                    for (int i = 0; i < e->n && !restored; i++) {
                        tree_t *ch = e->c[i];
                        if (!ch || ch->t != TT_ASSIGN || ch->n < 1) continue;
                        tree_t *lhs = ch->c[0];
                        if (!lhs || lhs->t != TT_VAR || !lhs->v.sval) continue;
                        if (strcasecmp(lhs->v.sval, ent->s[si].nm) == 0
                            && lhs->v.ival >= 0 && lhs->v.ival < FRAME.env_n) {
                            FRAME.env[lhs->v.ival] = ent->s[si].val;
                            restored = 1;
                        }
                    }
                }
                if (!restored) NV_SET_fn(ent->s[si].nm, ent->s[si].val);
            }
        }
        return NULVCL;
    }

    /* TT_SWAP — Icon :=: swap operator.  Evaluates both sides, writes cross.
     * Returns rv (the new value of lhs), or FAILDESCR if either side fails.
     * Mirrors interp_eval.c:3408-3437; interp_eval(child) → bb_eval_value(child). */
    case TT_SWAP: {
        if (e->n < 2 || frame_depth <= 0) return NULVCL;
        tree_t *lhs = e->c[0], *rhs = e->c[1];
        DESCR_t lv = bb_eval_value(lhs), rv = bb_eval_value(rhs);
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
        if (lhs && lhs->t == TT_VAR) {
            if (lhs->v.sval && lhs->v.sval[0] == '&') {
                if (!kw_assign(lhs->v.sval + 1, rv)) return FAILDESCR;
            } else {
                int sl=(int)lhs->v.ival;
                if (sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=rv;
                else if (sl<0&&lhs->v.sval) NV_SET_fn(lhs->v.sval,rv);
            }
        }
        if (rhs && rhs->t == TT_VAR) {
            if (rhs->v.sval && rhs->v.sval[0] == '&') {
                if (!kw_assign(rhs->v.sval + 1, lv)) return FAILDESCR;
            } else {
                int sl=(int)rhs->v.ival;
                if (sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=lv;
                else if (sl<0&&rhs->v.sval) NV_SET_fn(rhs->v.sval,lv);
            }
        }
        return rv;
    }

    /*========================================================================
     * RS-23-extra (session 2026-05-05): value-context handlers for the
     * remaining 5-of-6 unique tuples (`TT_RETURN via coro_eval` excluded —
     * separate oneshot path).  Diag prior to this rung showed:
     *   TT_BANG_BINARY  caller=bb_eval_value     via=bb_eval_value
     *   TT_IF           caller=bb_eval_value     via=bb_eval_value
     *   TT_IF           caller=coro_bb_seq_expr  via=bb_eval_value
     *   TT_PROC_FAIL    caller=(direct)          via=bb_eval_value
     *   TT_REVASSIGN    caller=bb_exec_stmt      via=bb_exec_stmt   (in coro_stmt.c)
     * Precondition: RS-23-extra-prep2 (smart fallback in icn_call_builtin)
     * unblocks the route by killing the meander double-eval regression.
     *======================================================================*/

    /* TT_IF in value context — mirrors interp_eval.c:3108-3114.
     * Eval cond; if it doesn't fail, evaluate then-branch (or return cond
     * value if there is no then); if cond fails, evaluate else-branch
     * (or return FAILDESCR if there is no else).  is_suspendable check
     * mirrors the stmt-context handler at coro_stmt.c:106 — for a
     * suspendable cond, drive its first value via coro_eval+α so generator
     * semantics are preserved. */
    case TT_IF: {
        if (e->n < 1) return NULVCL;
        tree_t *test = e->c[0];
        DESCR_t cv;
        if (is_suspendable(test)) {
            bb_node_t box = coro_eval(test);
            cv = box.fn(box.ζ, α);
        } else {
            cv = bb_eval_value(test);
        }
        if (!IS_FAIL_fn(cv))
            return (e->n > 1) ? bb_eval_value(e->c[1]) : cv;
        return (e->n > 2) ? bb_eval_value(e->c[2]) : FAILDESCR;
    }

    /* TT_PROC_FAIL in value context — mirrors interp_eval.c:2064-2070.
     * Procedure-level fail: set the frame's returning sentinel and return
     * FAILDESCR.  Note: this is the *eager* form, reached when something
     * directly calls bb_eval_value(TT_PROC_FAIL).  The lazy form for
     * `expr | fail` alternation lives in coro_eval (RS-23b's icn_lazy_box
     * wrapping at coro_runtime.c:1576) and is unaffected — the lazy box
     * triggers this same case only when the alternation arm is actually
     * pumped, preserving the semantics RS-23b established. */
    case TT_PROC_FAIL: {
        if (frame_depth > 0) {
            FRAME.return_val = FAILDESCR;
            FRAME.returning  = 1;
        }
        return FAILDESCR;
    }

    /* TT_REVASSIGN in value context — `x <- v`, reversible assign.
     * Mirrors interp_eval.c:606-637 (the standalone path).  Outside `every`
     * no driver backtracks the operation, so we just perform the assign and
     * succeed.  The revert semantics live in coro_bb_revassign and are
     * reached only when coro_eval is asked for a box (every / alt-driven
     * contexts) — that path is unaffected.  Three lvalue shapes: TT_VAR
     * (slot or NV name), TT_IDX (subscript_set), TT_FIELD (data_field_ptr). */
    case TT_REVASSIGN: {
        if (e->n < 2) return NULVCL;
        DESCR_t val = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        tree_t *lhs = e->c[0];
        if (lhs && lhs->t == TT_VAR) {
            int slot = (int)lhs->v.ival;
            if (slot >= 0 && slot < FRAME.env_n) FRAME.env[slot] = val;
            else if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') set_and_trace(lhs->v.sval, val);
        } else if (lhs && lhs->t == TT_IDX && lhs->n >= 2) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(base)) {
                DESCR_t idx = bb_eval_value(lhs->c[1]);
                if (!IS_FAIL_fn(idx)) subscript_set(base, idx, val);
            }
        } else if (lhs && lhs->t == TT_FIELD && lhs->v.sval && lhs->n >= 1) {
            DESCR_t obj = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(obj)) {
                DESCR_t *cell = data_field_ptr(lhs->v.sval, obj);
                if (cell) *cell = val;
            }
        }
        return val;
    }

    /* TT_BANG_BINARY in value context — `E1 ! E2`, Icon's apply-as-generator.
     * This is a generator combinator with no per-tick scalar state on
     * FRAME.gen — its state lives entirely in the bb_node_t built by
     * coro_eval.  Outer-pump retries reach it through the box's β path,
     * not through re-entry of bb_eval_value.  Mirrors the existing
     * TT_LIMIT/TT_ALTERNATE/TT_SEQ_EXPR pattern at coro_value.c:888-901. */
    case TT_BANG_BINARY: {
        /* (2) injection check — outer pump might have already produced
         * a tick we should return verbatim. */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* (3) fresh first-value via coro_eval+α */
        bb_node_t box = coro_eval(e);
        return box.fn(box.ζ, α);
    }

    /* TT_LOOP_BREAK in value context — surfaced by the diag after RS-23-extra
     * absorbed TT_IF/TT_PROC_FAIL/TT_BANG_BINARY/TT_REVASSIGN.  Pattern: `break`
     * appears as a body whose value is harvested by an enclosing expression
     * (e.g. `expr & break` in a value-context).  Mirrors interp_eval.c:3419-
     * 3422: set the frame's loop_break sentinel and return the optional
     * value child if present, else NULVCL.  The stmt-context handler in
     * coro_stmt.c:69 is the more common path. */
    case TT_LOOP_BREAK: {
        FRAME.loop_break = 1;
        return (e->n > 0) ? bb_eval_value(e->c[0]) : NULVCL;
    }

    /* TT_RETURN in value context — surfaced by the same diag rerun.  Pattern:
     * `return expr` appears as a body whose value is harvested.  Mirrors
     * interp_eval.c:2053-2061: evaluate the optional value child, set
     * FRAME.return_val and FRAME.returning, and return the value.  At
     * frame_depth 0 we just evaluate the child (no procedure to return
     * from).  The stmt-context handler in coro_stmt.c:80 is the common
     * path; this addition handles the rare value-context arrival. */
    case TT_RETURN: {
        if (frame_depth > 0) {
            FRAME.return_val = (e->n > 0)
                ? bb_eval_value(e->c[0]) : NULVCL;
            FRAME.returning = 1;
            return FRAME.return_val;
        }
        return (e->n > 0) ? bb_eval_value(e->c[0]) : NULVCL;
    }

    default:
        break;
    }

    /* RS-23e (closes the RS-23 arc, session 2026-05-05).  Diag verified
     * zero `interp_eval` calls reach this fallthrough from any BB-adapter
     * ancestor across smoke + unified_broker + full Icon corpus 263.
     * Every kind that exercises the test surface is now handled by an
     * explicit case above.  Anything reaching this point is a four-mode
     * isolation violation: a kind that arrived in BB-adapter context
     * without a native value-context handler.  Abort with a diagnostic
     * naming the kind so the gap can be lifted into bb_eval_value. */
    fprintf(stderr,
            "FATAL bb_eval_value: unhandled kind %d (RS-23e isolation breach)\n",
            (int)e->t);
    abort();
}
