/*============================================================================================================================
 * coro_value.c — RS-17a: pure-BB value-context evaluator for Icon Byrd boxes.
 *
 * See coro_value.h for the contract and migration plan.
 *
 * Today this delegates to `eval_node` for kinds that are SNOBOL4/Icon-identical
 * (literals, keywords) and adds an Icon-frame-aware E_VAR shim that reads the
 * slot-indexed local from FRAME.env when frame_depth > 0.  All other kinds
 * fall through to `interp_eval` — that fallthrough is intentional scaffolding
 * for the incremental migration of coro_runtime.c call sites (RS-17a-cont).
 *
 * RS-22a (2026-05-03): E_ASSIGN and E_FNC ported out of interp_eval's Icon-frame
 * switch.  All interp_eval(child) recurses replaced with bb_eval_value(child).
 * E_FNC builtins route through icn_call_builtin (already IR-free).
 *
 * RS-22b (2026-05-03): Arithmetic + numeric-comparison binops lifted in.
 * E_ADD/E_SUB/E_MUL/E_DIV/E_MOD/E_POW dispatch through `shared_arith` in
 * runtime/common/coerce.c (mirrors sm_interp's SM_ADD..SM_EXP path —
 * FAIL propagation, DT_S → INT, DT_SNUL → INT 0, then shared_arith).
 * E_LT/E_LE/E_GT/E_GE/E_EQ/E_NE return the right operand on success,
 * FAILDESCR on fail (Icon goal-directed convention).  E_IDENTICAL routes
 * through `icn_descr_identical` (declared in coro_runtime.h).  Note: there
 * is no E_NOT_IDENTICAL kind — `~===` lowers as E_NOT(E_IDENTICAL(...)).
 *
 * RS-22c (2026-05-03): String concat + subscript read + section read +
 * field read lifted in.  E_CAT and E_LCONCAT share `bb_str_concat`
 * (numeric operands → string via `descr_to_str_icn`, then GC_malloc'd
 * concat).  E_IDX dispatches via `subscript_get`/`subscript_get2` (already
 * exposed in snobol4.h).  E_FIELD via `data_field_ptr`.  E_SECTION/
 * E_SECTION_PLUS/E_SECTION_MINUS share `bb_section` with Icon position
 * normalization (0 → slen+1, negative → slen+1+p) — three minor variants
 * of bound computation kept inline.
 *
 * RS-22d (2026-05-03): Unary + augmented-assign kinds lifted in.
 * E_MNS (unary `-` — Icon parser uses E_MNS, not the rung-text "E_NEG"),
 * E_PLS (unary `+`), E_NOT (`not`), E_NULL (`/`), E_NONNULL (`\`),
 * E_SIZE (`*`), E_RANDOM (`?`) all dispatched directly.  E_AUGOP (the
 * actual IR kind name; rung-text "E_AUGASSIGN" was a label rather than
 * the literal kind) handles all three IR-mode paths: bang-iterate lvalue
 * (`!container OP:= rhs`), generator-RHS drive (`every sum +:= (1 to n)`
 * via coro_eval + bb_node_t.fn ticks), and plain `lv OP rv` then
 * writeback.  Two helpers — `bb_augop_compute` (pure compute given lv,
 * rv, op token) and `bb_augop_writeback` (write to E_VAR slot / E_IDX /
 * E_FIELD lhs) — replace IR-mode's AUGOP_APPLY / AUGOP_CELL macros.
 * Unsupported tokens (TK_AUGPOW, TK_AUGCSET_*, TK_AUGSCAN) fall through
 * to the integer-add default — same coverage as IR-mode.
 *
 * RS-22e (2026-05-03): Fallthrough survey.  smoke_icon hits zero
 * unhandled kinds — the rung's stated gate is met.  Full Icon corpus
 * (271 programs) hits 16 distinct kinds totaling 62 fallthroughs, in
 * five categories: generators (E_TO/E_ALTERNATE/E_ITERATE/E_LIMIT/
 * E_SEQ), string relops (E_LEQ/E_LNE/E_LGE/E_LLT plus untriggered
 * E_LGT/E_LLE peers), cset arithmetic (E_CSET/E_CSET_COMPL/_DIFF/
 * _INTER), and three mid-size kinds (E_MAKELIST, E_SCAN, E_CASE).
 * Hardening the fallthrough to FAILDESCR causes 4 unified_broker FAILs
 * (notably palindrome.icn via E_LNE), so per the rung the abort is
 * reverted; full inventory in docs/RS-22e-fallthrough-survey.md.
 * RS-22f-or-RS-23 closes the remaining 16 kinds; only after that can
 * the `interp_eval` extern be dropped (RS-23) and coro_value.c
 * promoted into the isolation gate.
 *
 * RS-22f-strrel (2026-05-03): Six string lexicographic relops lifted
 * in.  E_LLT/E_LLE/E_LGT/E_LGE/E_LEQ/E_LNE share `bb_strrel` — direct
 * mirror of `bb_numrel` using strcmp(VARVAL_fn(l), VARVAL_fn(r)).
 * Right-operand-on-success (Icon goal-directed convention).  Closes
 * 17 fallthroughs (E_LEQ ×9, E_LNE ×6, E_LGE ×1, E_LLT ×1) and the
 * two untriggered peers in the survey.  Removes palindrome.icn
 * unified_broker failure path and three peers — first sub-rung of
 * RS-22f.
 *
 * RS-22f-makelist (2026-05-03): E_MAKELIST lifted in.  Mirrors
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
#include "coro_stmt.h"         /* RS-23c: bb_exec_stmt used in E_EVERY body dispatch */
#include "coro_runtime.h"   /* FRAME, frame_depth, scan_pos, scan_subj, icn_descr_identical, g_lang, is_suspendable, coro_eval */
#include "../../driver/interp_private.h"  /* RS-22a: icn_call_builtin, icn_string_section_assign, set_and_trace, data_field_ptr, kw_assign; RS-22d: IcnTkKind via icon_lex.h */
#include "../common/coerce.h"             /* RS-22b: shared_arith */
#include "../x86/bb_broker.h"             /* RS-22d: α, β, bb_node_t for E_AUGOP generator-RHS path */
#include "snobol4.h"
#include <string.h>
#include <gc/gc.h>

/* eval_node lives in src/runtime/x86/eval_code.c — IR-free expression evaluator. */
extern DESCR_t eval_node(EXPR_t *e);

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
static DESCR_t bb_arith(EXPR_t *e, sm_opcode_t op)
{
    if (e->nchildren < 2) return FAILDESCR;
    DESCR_t l = bb_eval_value(e->children[0]);
    DESCR_t r = bb_eval_value(e->children[1]);
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

static DESCR_t bb_numrel(EXPR_t *e, bb_relop_t op)
{
    if (e->nchildren < 2) return FAILDESCR;
    DESCR_t l = bb_eval_value(e->children[0]);
    DESCR_t r = bb_eval_value(e->children[1]);
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

static DESCR_t bb_strrel(EXPR_t *e, bb_strrelop_t op)
{
    if (e->nchildren < 2) return FAILDESCR;
    DESCR_t l = bb_eval_value(e->children[0]);
    DESCR_t r = bb_eval_value(e->children[1]);
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
 * bb_str_concat — RS-22c string-concat helper (E_CAT + E_LCONCAT).
 *
 * Icon `||` (E_CAT) and `|||` (E_LCONCAT) both reach here via the BB
 * adapter.  Mirrors the IR-mode E_LCONCAT case at interp_eval.c:4037 —
 * coerce numeric operands via descr_to_str_icn (round-trip-correct real
 * formatting), VARVAL_fn for everything else, GC_malloc'd concat.
 *
 * Pattern operands: do not occur in BB-engine call sites today (Icon
 * never produces them; SNOBOL4's pattern-context paths never reach
 * bb_eval_value).  If one ever did, descr_to_str_icn would fail-through
 * to FAILDESCR rather than producing garbage.
 *----------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_str_concat(EXPR_t *e)
{
    if (e->nchildren < 2) return NULVCL;
    DESCR_t a = bb_eval_value(e->children[0]);
    DESCR_t b = bb_eval_value(e->children[1]);
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
 * Mirrors interp_eval.c:4070-4125 for E_SECTION (s[i:j]), E_SECTION_PLUS
 * (s[i+:n]), E_SECTION_MINUS (s[i-:n]).  Icon position rules:
 *   p ≥ 1     → 1-based position (1 is before first char)
 *   p == 0    → position past last char (= slen+1)
 *   p < 0     → slen+1+p   (-1 → slen, -2 → slen-1, ...)
 * Out-of-bounds after normalization → FAILDESCR.
 *----------------------------------------------------------------------------------------------------------------------------*/
typedef enum { BBS_RANGE, BBS_PLUS, BBS_MINUS } bb_section_t;

static DESCR_t bb_section(EXPR_t *e, bb_section_t kind)
{
    if (e->nchildren < 3) return NULVCL;
    DESCR_t sd = bb_eval_value(e->children[0]);
    if (IS_FAIL_fn(sd)) return FAILDESCR;
    const char *s = VARVAL_fn(sd);
    if (!s) s = "";
    int slen = (int)strlen(s);
    DESCR_t a1 = bb_eval_value(e->children[1]);
    DESCR_t a2 = bb_eval_value(e->children[2]);
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
 * bb_augop_compute — RS-22d pure compute step for E_AUGOP.
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
 * lhs may be E_VAR (slot/global), E_IDX (subscript), or E_FIELD (record
 * field).  E_VAR&-keyword left untouched for parity with IR-mode (the
 * AUGOP_APPLY macro never wrote back to keyword lvalues).  Callers
 * already checked !IS_FAIL_fn(res).
 *----------------------------------------------------------------------------------------------------------------------------*/
static void bb_augop_writeback(EXPR_t *lhs, DESCR_t res)
{
    if (!lhs) return;
    if (lhs->kind == E_VAR) {
        int slot = (int)lhs->ival;
        if (frame_depth > 0 && slot >= 0 && slot < FRAME.env_n)
            FRAME.env[slot] = res;
        else if (slot < 0 && lhs->sval && lhs->sval[0] != '&')
            set_and_trace(lhs->sval, res);
    } else if (lhs->kind == E_IDX && lhs->nchildren >= 2) {
        DESCR_t base = bb_eval_value(lhs->children[0]);
        DESCR_t idx  = bb_eval_value(lhs->children[1]);
        if (!IS_FAIL_fn(base) && !IS_FAIL_fn(idx))
            subscript_set(base, idx, res);
    } else if (lhs->kind == E_FIELD && lhs->sval && lhs->nchildren >= 1) {
        DESCR_t obj = bb_eval_value(lhs->children[0]);
        if (!IS_FAIL_fn(obj)) {
            DESCR_t *cell = data_field_ptr(lhs->sval, obj);
            if (cell) *cell = res;
        }
    }
}

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_eval_value — evaluate e in value context.
 *
 * Value context means: produce a single DESCR_t result.  Generator-context
 * sub-expressions (E_TO/E_TO_BY/E_ALTERNATE/E_FNC user-proc/etc.) fall through
 * to interp_eval today; routing them through coro_eval + a single bb_broker
 * BB_ONCE pump is RS-17a-cont work.
 *
 * Icon-frame E_VAR shim mirrors interp_eval.c lines 353-372: when frame_depth>0
 * we read scan/letter keywords directly and use slot-indexed locals from
 * FRAME.env[ival].  Outside an Icon frame, E_VAR delegates to eval_node which
 * does the SNOBOL4 NV_GET_fn lookup.
 *----------------------------------------------------------------------------------------------------------------------------*/
DESCR_t bb_eval_value(EXPR_t *e)
{
    if (!e) return NULVCL;

    /* Icon-frame-aware E_VAR: slot read when inside an Icon procedure call. */
    if (e->kind == E_VAR && frame_depth > 0) {
        if (e->sval && e->sval[0] == '&') {
            const char *kw = e->sval + 1;
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
        int slot = (int)e->ival;
        if (slot >= 0 && slot < FRAME.env_n) return FRAME.env[slot];
        if (slot < 0 && e->sval && e->sval[0] != '&') return NV_GET_fn(e->sval);
        return NULVCL;
    }

    /* Kinds that eval_node already handles identically for SNOBOL4 and Icon
     * outside-of-frame: literals, &keywords, NULVCL.  E_VAR outside an Icon
     * frame goes through eval_node's NV_GET_fn path. */
    switch (e->kind) {
    case E_ILIT:
    case E_FLIT:
    case E_QLIT:
    case E_NUL:
    case E_KEYWORD:
        return eval_node(e);
    case E_VAR:
        /* frame_depth == 0: SNOBOL4-style lookup via eval_node */
        return eval_node(e);

    /* RS-22a: E_ASSIGN — slot store, IDX, FIELD, ITERATE, RANDOM lvalue paths.
     * Mirrors interp_eval.c lines 373-474; all interp_eval(child) replaced
     * with bb_eval_value(child). */
    case E_ASSIGN: {
        if (e->nchildren < 2) return NULVCL;
        DESCR_t val = bb_eval_value(e->children[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        EXPR_t *lhs = e->children[0];
        if (lhs && (lhs->kind == E_SECTION || lhs->kind == E_SECTION_PLUS ||
                    lhs->kind == E_SECTION_MINUS)) {
            if (icn_string_section_assign(lhs, val)) return val;
            return FAILDESCR;
        }
        if (lhs && lhs->kind == E_IDX && lhs->nchildren >= 2) {
            if (icn_string_section_assign(lhs, val)) return val;
            { DESCR_t _b = bb_eval_value(lhs->children[0]);
              if (_b.v == DT_S || _b.v == DT_SNUL) return FAILDESCR; }
        }
        if (lhs && lhs->kind == E_VAR) {
            if (lhs->sval && lhs->sval[0] == '&') {
                if (!kw_assign(lhs->sval + 1, val)) return FAILDESCR;
                return val;
            }
            int slot = (int)lhs->ival;
            if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; return val; }
            if (slot < 0 && lhs->sval && lhs->sval[0] != '&') set_and_trace(lhs->sval, val);
        } else if (lhs && lhs->kind == E_IDX && lhs->nchildren >= 2) {
            DESCR_t base = bb_eval_value(lhs->children[0]);
            if (!IS_FAIL_fn(base)) {
                DESCR_t idx = bb_eval_value(lhs->children[1]);
                if (!IS_FAIL_fn(idx)) subscript_set(base, idx, val);
            }
        } else if (lhs && lhs->kind == E_FIELD && lhs->sval && lhs->nchildren >= 1) {
            DESCR_t obj = bb_eval_value(lhs->children[0]);
            if (!IS_FAIL_fn(obj)) {
                DESCR_t *cell = data_field_ptr(lhs->sval, obj);
                if (cell) *cell = val;
            }
        } else if (lhs && lhs->kind == E_ITERATE && lhs->nchildren >= 1) {
            DESCR_t cv = bb_eval_value(lhs->children[0]);
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

    /* RS-22a: E_FNC — user-proc path dispatches through proc_table → coro_call.
     * Builtin path evaluates args through bb_eval_value then calls icn_call_builtin
     * (already IR-free).  Mirror of interp_eval.c E_FNC case but recursion-safe. */
    case E_FNC: {
        if (e->nchildren < 1) return NULVCL;
        const char *fn = e->children[0] ? e->children[0]->sval : NULL;
        if (!fn) return NULVCL;
        int nargs = e->nchildren - 1;
        /* RS-23a-raku: Raku block-receiving builtins (raku_try / raku_map /
         * raku_grep / raku_sort) need raw EXPR_t access and must NOT be subject
         * to FAIL-prop on the body argument — dispatch them here, before the
         * generic user-proc / builtin pre-eval loops below.  raku_try_call_builtin
         * returns 1 if `fn` matched a Raku builtin (and *out is set); 0 means
         * not a Raku builtin and we fall through to the existing dispatch. */
        {
            DESCR_t __rk_d;
            if (raku_try_call_builtin(e, &__rk_d)) return __rk_d;
        }
        /* User-proc path: look up in proc_table and call via proc_table_call
         * (CH-17g-call-sites: dispatches via SM chunk when entry_pc resolved). */
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, fn) != 0) continue;
            DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
            for (int j = 0; j < nargs; j++) args[j] = bb_eval_value(e->children[1+j]);
            DESCR_t result = proc_table_call(i, args, nargs);
            return result;
        }
        /* Builtin path: evaluate args through bb_eval_value, then icn_call_builtin. */
        DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
        for (int j = 0; j < nargs; j++) {
            args[j] = bb_eval_value(e->children[1+j]);
            if (IS_FAIL_fn(args[j])) return FAILDESCR;
        }
        return icn_call_builtin(e, args, nargs);
    }

    /* RS-22b: arithmetic binops — bb_arith handles FAIL prop, string→int,
     * SNUL→0, then dispatches via shared_arith (the same path SM mode uses).
     * E_POW: shared_arith maps SM_EXP → integer for non-negative int^int,
     * else REALVAL(pow(...)).  This matches Icon `^` (always real if any
     * operand is real) and SNOBOL4 `**` (int when both ints, exp >= 0). */
    case E_ADD: return bb_arith(e, SM_ADD);
    case E_SUB: return bb_arith(e, SM_SUB);
    case E_MUL: return bb_arith(e, SM_MUL);
    case E_DIV: return bb_arith(e, SM_DIV);
    case E_MOD: return bb_arith(e, SM_MOD);
    case E_POW: return bb_arith(e, SM_EXP);

    /* RS-22b: numeric relational binops — succeed → return right operand,
     * fail → FAILDESCR.  Right-operand-on-success is Icon goal-directed
     * convention (lets `2 < (1 to 4)` filter a generator chain). */
    case E_LT: return bb_numrel(e, BBR_LT);
    case E_LE: return bb_numrel(e, BBR_LE);
    case E_GT: return bb_numrel(e, BBR_GT);
    case E_GE: return bb_numrel(e, BBR_GE);
    case E_EQ: return bb_numrel(e, BBR_EQ);
    case E_NE: return bb_numrel(e, BBR_NE);

    /* RS-22b: deep identity (Icon `===`).  Right-operand-on-success again.
     * Note: `~===` does NOT lower to a distinct EXPR kind — Icon's parser
     * emits E_NOT(E_IDENTICAL(a, b)).  When E_NOT is lifted (RS-22d), the
     * full `~===` path becomes IR-free; until then E_NOT still falls through
     * to interp_eval and walks back here for its E_IDENTICAL child. */
    case E_IDENTICAL: {
        if (e->nchildren < 2) return FAILDESCR;
        DESCR_t l = bb_eval_value(e->children[0]);
        DESCR_t r = bb_eval_value(e->children[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return icn_descr_identical(l, r) ? r : FAILDESCR;
    }

    /* RS-22f-strrel: lexicographic (string) relational binops — succeed →
     * return right operand, fail → FAILDESCR.  Mirror of bb_numrel for
     * Icon string comparison operators (==, ~==, <<, <<=, >>, >>=). */
    case E_LLT: return bb_strrel(e, BBS_LLT);
    case E_LLE: return bb_strrel(e, BBS_LLE);
    case E_LGT: return bb_strrel(e, BBS_LGT);
    case E_LGE: return bb_strrel(e, BBS_LGE);
    case E_LEQ: return bb_strrel(e, BBS_LEQ);
    case E_LNE: return bb_strrel(e, BBS_LNE);

    /* RS-22c: string concat — Icon `||` (E_CAT) and `|||` (E_LCONCAT)
     * share bb_str_concat.  In Icon BB context neither produces patterns,
     * so the simple coerce-and-concat path is correct (mirrors IR-mode
     * E_LCONCAT at interp_eval.c:4037).  SNOBOL4 E_CAT in pattern context
     * does not arrive here — that path is interp_eval_pat-only. */
    case E_CAT:
    case E_LCONCAT:
        return bb_str_concat(e);

    /* RS-22c: subscript read — table/list/record/string index.
     * Two-arg form → subscript_get; three-arg form (s[i:j] lowered as
     * E_IDX with two index children) → subscript_get2.  Mirrors
     * interp_eval.c:3084. */
    case E_IDX: {
        if (e->nchildren < 2) return FAILDESCR;
        DESCR_t base = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(base)) return FAILDESCR;
        if (e->nchildren == 2) {
            DESCR_t idx = bb_eval_value(e->children[1]);
            if (IS_FAIL_fn(idx)) return FAILDESCR;
            return subscript_get(base, idx);
        }
        DESCR_t i1 = bb_eval_value(e->children[1]);
        DESCR_t i2 = bb_eval_value(e->children[2]);
        if (IS_FAIL_fn(i1) || IS_FAIL_fn(i2)) return FAILDESCR;
        return subscript_get2(base, i1, i2);
    }

    /* RS-22c: record field read.  e->sval = field name, child[0] = object. */
    case E_FIELD: {
        if (!e->sval || e->nchildren < 1) return NULVCL;
        DESCR_t obj = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(obj)) return FAILDESCR;
        DESCR_t *cell = data_field_ptr(e->sval, obj);
        if (!cell) return FAILDESCR;
        return *cell;
    }

    /* RS-22c: string section (Icon s[i:j], s[i+:n], s[i-:n]) — bb_section.
     * Three minor variants of bound computation share one helper. */
    case E_SECTION:        return bb_section(e, BBS_RANGE);
    case E_SECTION_PLUS:   return bb_section(e, BBS_PLUS);
    case E_SECTION_MINUS:  return bb_section(e, BBS_MINUS);

    /* RS-22f-makelist: Icon `[e1, e2, ...]` list constructor.  Mirrors
     * interp_eval.c:4051-4062 — register `icnlist` DATA type once on
     * first sighting, GC_malloc the elem array, evaluate each child via
     * bb_eval_value (was interp_eval in IR mode), then DATCON_fn to
     * build the DT_DATA descriptor.  The static-flag idiom matches IR
     * mode exactly: DEFDAT_fn is called at most once per process. */
    case E_MAKELIST: {
        int n = e->nchildren;
        static int icnlist_registered = 0;
        if (!icnlist_registered) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_registered = 1; }
        DESCR_t *elems = GC_malloc((n > 0 ? n : 1) * sizeof(DESCR_t));
        for (int i = 0; i < n; i++) elems[i] = bb_eval_value(e->children[i]);
        DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void*)elems;
        return DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
    }

    /* RS-22d: unary minus / plus.  Mirrors interp_eval.c:2501-2540.
     * E_PLS is more elaborate than `pos()` — try integer parse first,
     * then real, fall back to INTVAL(0).  Match exactly. */
    case E_MNS: {
        if (e->nchildren < 1) return FAILDESCR;
        return neg(bb_eval_value(e->children[0]));
    }
    case E_PLS: {
        if (e->nchildren < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->children[0]);
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
    case E_NOT: {
        if (e->nchildren < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(v)) return NULVCL;
        return FAILDESCR;
    }

    /* RS-22d: `/E` succeed-if-null. Mirrors interp_eval.c:3629. */
    case E_NULL: {
        if (e->nchildren < 1) return NULVCL;
        DESCR_t v = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return NULVCL;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return NULVCL;
        return FAILDESCR;
    }

    /* RS-22d: `\E` succeed-if-non-null. Mirrors interp_eval.c:3646. */
    case E_NONNULL: {
        if (e->nchildren < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return FAILDESCR;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return FAILDESCR;
        return v;
    }

    /* RS-22d: `*E` size — string/list/table.  Mirrors interp_eval.c:3133. */
    case E_SIZE: {
        if (e->nchildren < 1) return INTVAL(0);
        DESCR_t v = bb_eval_value(e->children[0]);
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
    case E_RANDOM: {
        if (e->nchildren < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        static unsigned long _rnd_seed = 12345UL;
        _rnd_seed = _rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
        unsigned long _rnd = _rnd_seed >> 33;
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
     *   (1) `!container OP:= rhs`  (lhs is E_ITERATE) — bang-iterate,
     *       apply OP to every cell of T/list/record in place.
     *   (2) RHS suspendable          — drive via coro_eval+bb_node_t,
     *       apply OP per tick, re-reading lhs each tick.  Implements
     *       `every sum +:= (1 to n)`.
     *   (3) Plain                     — single lv OP rv, then writeback.
     * `bb_augop_compute` is the shared compute step; `bb_augop_writeback`
     * is the shared writeback (E_VAR slot / E_IDX / E_FIELD). */
    case E_AUGOP: {
        if (e->nchildren < 2) return NULVCL;
        EXPR_t *lhs = e->children[0];
        EXPR_t *rhs = e->children[1];
        IcnTkKind op = (IcnTkKind)e->ival;
        DESCR_t result = NULVCL;

        /* (1) `!container OP:= rhs` — bang-iterate lvalue. */
        if (lhs && lhs->kind == E_ITERATE && lhs->nchildren >= 1) {
            DESCR_t cv = bb_eval_value(lhs->children[0]);
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
     * E_TO / E_TO_BY / E_ITERATE / E_LIMIT / E_ALTERNATE / E_SEQ_EXPR are
     * generators.  The contract has THREE cases that must be distinguished:
     *
     * (1) The node is currently being driven by an outer pump.  An every-loop
     *     above us pushed the E_TO onto FRAME's gen stack; the body re-reads
     *     the node to fetch the current tick value.  E.g.:
     *         every write("ICN: " || (1 to 3))
     *     drives E_FNC(write) which drives E_CAT which evaluates E_TO.  The
     *     E_TO is on FRAME's gen stack with cur = 1, 2, 3 successively.
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
     *     Mirrors RS-22d's E_AUGOP generator-RHS path (lines 784-794) and
     *     interp_eval.c's E_IF goal-directed test (line 2386-2392).
     *
     * E_SEQ is `&` conjunction in value context: evaluate children
     * left-to-right; FAILDESCR on first failure; return last child on full
     * success.  Mirrors interp_eval.c:2348 icn-frame switch case. */
    case E_TO:
    case E_TO_BY: {
        /* (2) injection check — outer driver staged a value */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* (1) frame check — outer pump is iterating this node */
        long cur;
        if (icn_frame_lookup(e, &cur)) return INTVAL(cur);
        /* (3) fresh first-value via coro_eval+α */
        bb_node_t box = coro_eval(e);
        return box.fn(box.ζ, α);
    }

    case E_ITERATE: {
        /* (2) injection check */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* (1) frame check — !L pump pushes E_ITERATE onto frame as the
         * generator node; current cur is the index, sval is the string for
         * char-iter or the list bucket.  Mirrors interp_eval.c E_ITERATE in
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

    case E_LIMIT:
    case E_ALTERNATE:
    case E_SEQ_EXPR: {
        /* (2) injection check */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* These three don't carry per-tick scalar state on FRAME.gen the way
         * E_TO/E_ITERATE do; they are pure generator combinators whose
         * internal state lives in the bb_node_t.  Outer-pump retries reach
         * them through the bb_node_t's β path, not through re-entry of
         * bb_eval_value.  So fresh α-once is correct here.
         * (3) fresh first-value via coro_eval+α */
        bb_node_t box = coro_eval(e);
        return box.fn(box.ζ, α);
    }

    case E_SEQ: {
        if (e->nchildren == 0) return NULVCL;
        DESCR_t last = NULVCL;
        for (int i = 0; i < e->nchildren; i++) {
            last = bb_eval_value(e->children[i]);
            if (IS_FAIL_fn(last)) return FAILDESCR;
            if (FRAME.returning || FRAME.loop_break || FRAME.loop_next) break;
        }
        return last;
    }

    /* RS-22f-stmt (2026-05-03): E_SCAN and E_CASE in value context.
     *
     * E_SCAN: `subj ? body` in Icon/Prolog mode.  bb_eval_value is only
     * reached from BB-engine call sites (Icon every-bodies, Prolog
     * clause-bodies), so the SNOBOL4-mode exec_stmt branch in
     * interp_eval.c:3887 is unreachable here — the Icon/Prolog branch
     * (line 3899) is what we mirror.  Push current scan state, evaluate
     * subject as string, install as new scan target with pos=1, evaluate
     * body, restore prior scan state.
     *
     * E_CASE: case-expression in value context.  Evaluate topic; walk
     * pairs (Icon) or triples (Raku) comparing topic to each value;
     * return the matching body's value, or the default body's value, or
     * NULVCL.  Mirrors interp_eval.c:3569 verbatim — only `interp_eval`
     * recursive calls are replaced with `bb_eval_value`. */
    case E_SCAN: {
        if (e->nchildren < 1) return FAILDESCR;
        DESCR_t subj_d = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(subj_d)) return FAILDESCR;
        const char *subj_s = VARVAL_fn(subj_d); if (!subj_s) subj_s = "";
        if (scan_depth < SCAN_STACK_MAX) {
            scan_stack[scan_depth].subj = scan_subj;
            scan_stack[scan_depth].pos  = scan_pos;
            scan_depth++;
        }
        scan_subj = subj_s; scan_pos = 1;
        DESCR_t r = (e->nchildren >= 2) ? bb_eval_value(e->children[1]) : NULVCL;
        if (scan_depth > 0) {
            scan_depth--;
            scan_subj = scan_stack[scan_depth].subj;
            scan_pos  = scan_stack[scan_depth].pos;
        }
        return r;
    }

    case E_CASE: {
        if (e->nchildren < 1) return NULVCL;
        DESCR_t topic = bb_eval_value(e->children[0]);
        /* Detect Raku triple layout: (nchildren-1)%3==0 AND child[1] is
         * E_ILIT or E_NUL (the comparison-kind marker). */
        int is_raku_layout = (e->nchildren >= 4 && (e->nchildren - 1) % 3 == 0 &&
            e->children[1] && (e->children[1]->kind == E_ILIT || e->children[1]->kind == E_NUL));
        if (is_raku_layout) {
            int i = 1;
            while (i + 2 < e->nchildren) {
                EXPR_t *cmpnode = e->children[i];
                EXPR_t *val     = e->children[i+1];
                EXPR_t *body    = e->children[i+2];
                i += 3;
                if (cmpnode->kind == E_NUL) return bb_eval_value(body);
                EXPR_e cmp = (EXPR_e)(cmpnode->ival);
                DESCR_t wval = bb_eval_value(val);
                int match = 0;
                if (cmp == E_LEQ) {
                    const char *ts = IS_STR_fn(topic)?topic.s:VARVAL_fn(topic);
                    const char *ws = IS_STR_fn(wval) ?wval.s :VARVAL_fn(wval);
                    match = (ts && ws && strcmp(ts, ws) == 0);
                } else {
                    if (IS_INT_fn(topic) && IS_INT_fn(wval)) match = (topic.i == wval.i);
                    else { const char *ts=VARVAL_fn(topic), *ws=VARVAL_fn(wval); match=(ts && ws && strcmp(ts,ws)==0); }
                }
                if (match) return bb_eval_value(body);
            }
            if (i+1 < e->nchildren && e->children[i]->kind == E_NUL)
                return bb_eval_value(e->children[i+1]);
            return NULVCL;
        }
        /* Icon: pairs [val, body] then optional trailing default body */
        int nc = e->nchildren;
        int i = 1;
        while (i + 1 < nc) {
            DESCR_t wval = bb_eval_value(e->children[i]);
            EXPR_t *body = e->children[i+1];
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
        if (i < nc) return bb_eval_value(e->children[i]);
        return NULVCL;
    }

    /* RS-22f-cset (2026-05-03): Cset literal + four set-arithmetic ops.
     * Icon csets are represented as NUL-terminated strings of member chars.
     * The four binary ops delegate to the icn_cset_* helpers in icon_runtime.c
     * (now declared in coro_runtime.h).  E_CSET literal mirrors interp_eval.c:3466. */
    case E_CSET:
        return e->sval ? STRVAL(e->sval) : NULVCL;

    case E_CSET_COMPL: {
        DESCR_t operand = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(operand)) return FAILDESCR;
        const char *cs = IS_NULL_fn(operand) ? "" : VARVAL_fn(operand);
        return STRVAL(icn_cset_complement(cs));
    }

    case E_CSET_UNION: {
        DESCR_t lv = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->children[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        const char *a = IS_NULL_fn(lv) ? "" : VARVAL_fn(lv);
        const char *b = IS_NULL_fn(rv) ? "" : VARVAL_fn(rv);
        return STRVAL(icn_cset_union(a, b));
    }

    case E_CSET_DIFF: {
        DESCR_t lv = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->children[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        const char *a = IS_NULL_fn(lv) ? "" : VARVAL_fn(lv);
        const char *b = IS_NULL_fn(rv) ? "" : VARVAL_fn(rv);
        return STRVAL(icn_cset_diff(a, b));
    }

    case E_CSET_INTER: {
        DESCR_t lv = bb_eval_value(e->children[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->children[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        const char *a = IS_NULL_fn(lv) ? "" : VARVAL_fn(lv);
        const char *b = IS_NULL_fn(rv) ? "" : VARVAL_fn(rv);
        return STRVAL(icn_cset_inter(a, b));
    }

    /* RS-23d: E_WHILE — Icon while loop in value context.
     * Returns NULVCL after normal exit; mirrors interp_eval.c:1719-1730 with
     * interp_eval(child) replaced by bb_eval_value (cond) and bb_exec_stmt (body). */
    case E_WHILE: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            DESCR_t cv = (e->nchildren > 0) ? bb_eval_value(e->children[0]) : FAILDESCR;
            if (IS_FAIL_fn(cv)) break;
            FRAME.loop_next = 0;
            if (e->nchildren > 1) bb_exec_stmt(e->children[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return NULVCL;
    }

    /*========================================================================
     * RS-23c: E_EVERY, E_INITIAL, E_SWAP — missing from both adapters.
     * Value-context implementations mirror interp_eval.c's icon-frame switch,
     * with interp_eval(child) replaced by bb_eval_value(child) / bb_exec_stmt.
     *======================================================================*/

    /* E_EVERY — drive a generator, run optional body per tick.  Returns NULVCL.
     * Three sub-cases mirror interp_eval.c:1639-1750:
     *   1. E_ASSIGN with generative RHS — re-evaluate assignment per tick.
     *   2. E_SEQ conjunction — drive filter, execute seq body per tick.
     *   3. Generic — drive gen via coro_eval box, run body per tick. */
    case E_EVERY: {
        if (e->nchildren < 1) return NULVCL;
        EXPR_t *gen  = e->children[0];
        EXPR_t *body = (e->nchildren > 1) ? e->children[1] : NULL;
        if (gen->kind == E_ASSIGN &&
            gen->nchildren >= 2 && is_suspendable(gen->children[1])) {
            EXPR_t *leaf = find_leaf_suspendable(gen->children[1]);
            if (!leaf) leaf = gen->children[1];
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
        if (gen->kind == E_SEQ && gen->nchildren >= 2 && is_suspendable(gen->children[0])) {
            EXPR_t *filter = gen->children[0];
            bb_node_t fbox = coro_eval(filter);
            DESCR_t tick = fbox.fn(fbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.returning && !FRAME.loop_break) {
                FRAME.loop_next = 0;
                for (int _si = 1; _si < gen->nchildren; _si++) bb_exec_stmt(gen->children[_si]);
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
            if (gen->sval && *gen->sval && caller_depth >= 1) {
                IcnFrame *cf = &frame_stack[caller_depth - 1];
                int slot = scope_get(&cf->sc, gen->sval);
                if (slot >= 0 && slot < cf->env_n) cf->env[slot] = val;
                else NV_SET_fn(gen->sval, val);
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

    /* E_INITIAL — once-only block; side-effects only, returns NULVCL.
     * Mirrors interp_eval.c:3558-3601; interp_eval(child) → bb_eval_value(child). */
    case E_INITIAL: {
        IcnInitEnt *ent = NULL;
        for (int _i = 0; _i < icn_init_n; _i++)
            if (init_tab[_i].id == e->id) { ent = &init_tab[_i]; break; }
        if (!ent) {
            for (int i = 0; i < e->nchildren; i++) (void)bb_eval_value(e->children[i]);
            if (icn_init_n < ICN_INIT_MAX) {
                ent = &init_tab[icn_init_n++];
                ent->id = e->id; ent->ns = 0;
                for (int i = 0; i < e->nchildren && ent->ns < ICN_INIT_SLOTS; i++) {
                    EXPR_t *ch = e->children[i];
                    if (!ch || ch->kind != E_ASSIGN || ch->nchildren < 1) continue;
                    EXPR_t *lhs = ch->children[0];
                    if (!lhs || lhs->kind != E_VAR || !lhs->sval) continue;
                    IcnInitSlot *sl = &ent->s[ent->ns++];
                    strncpy(sl->nm, lhs->sval, 63); sl->nm[63] = '\0';
                    if (frame_depth > 0 && lhs->ival >= 0 && lhs->ival < FRAME.env_n)
                        sl->val = FRAME.env[lhs->ival];
                    else
                        sl->val = NV_GET_fn(lhs->sval);
                }
            }
            e->ival = 1;
        } else {
            for (int si = 0; si < ent->ns; si++) {
                int restored = 0;
                if (frame_depth > 0) {
                    for (int i = 0; i < e->nchildren && !restored; i++) {
                        EXPR_t *ch = e->children[i];
                        if (!ch || ch->kind != E_ASSIGN || ch->nchildren < 1) continue;
                        EXPR_t *lhs = ch->children[0];
                        if (!lhs || lhs->kind != E_VAR || !lhs->sval) continue;
                        if (strcasecmp(lhs->sval, ent->s[si].nm) == 0
                            && lhs->ival >= 0 && lhs->ival < FRAME.env_n) {
                            FRAME.env[lhs->ival] = ent->s[si].val;
                            restored = 1;
                        }
                    }
                }
                if (!restored) NV_SET_fn(ent->s[si].nm, ent->s[si].val);
            }
        }
        return NULVCL;
    }

    /* E_SWAP — Icon :=: swap operator.  Evaluates both sides, writes cross.
     * Returns rv (the new value of lhs), or FAILDESCR if either side fails.
     * Mirrors interp_eval.c:3408-3437; interp_eval(child) → bb_eval_value(child). */
    case E_SWAP: {
        if (e->nchildren < 2 || frame_depth <= 0) return NULVCL;
        EXPR_t *lhs = e->children[0], *rhs = e->children[1];
        DESCR_t lv = bb_eval_value(lhs), rv = bb_eval_value(rhs);
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
        if (lhs && lhs->kind == E_VAR) {
            if (lhs->sval && lhs->sval[0] == '&') {
                if (!kw_assign(lhs->sval + 1, rv)) return FAILDESCR;
            } else {
                int sl=(int)lhs->ival;
                if (sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=rv;
                else if (sl<0&&lhs->sval) NV_SET_fn(lhs->sval,rv);
            }
        }
        if (rhs && rhs->kind == E_VAR) {
            if (rhs->sval && rhs->sval[0] == '&') {
                if (!kw_assign(rhs->sval + 1, lv)) return FAILDESCR;
            } else {
                int sl=(int)rhs->ival;
                if (sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=lv;
                else if (sl<0&&rhs->sval) NV_SET_fn(rhs->sval,lv);
            }
        }
        return rv;
    }

    /*========================================================================
     * RS-23-extra (session 2026-05-05): value-context handlers for the
     * remaining 5-of-6 unique tuples (`E_RETURN via coro_eval` excluded —
     * separate oneshot path).  Diag prior to this rung showed:
     *   E_BANG_BINARY  caller=bb_eval_value     via=bb_eval_value
     *   E_IF           caller=bb_eval_value     via=bb_eval_value
     *   E_IF           caller=coro_bb_seq_expr  via=bb_eval_value
     *   E_PROC_FAIL    caller=(direct)          via=bb_eval_value
     *   E_REVASSIGN    caller=bb_exec_stmt      via=bb_exec_stmt   (in coro_stmt.c)
     * Precondition: RS-23-extra-prep2 (smart fallback in icn_call_builtin)
     * unblocks the route by killing the meander double-eval regression.
     *======================================================================*/

    /* E_IF in value context — mirrors interp_eval.c:3108-3114.
     * Eval cond; if it doesn't fail, evaluate then-branch (or return cond
     * value if there is no then); if cond fails, evaluate else-branch
     * (or return FAILDESCR if there is no else).  is_suspendable check
     * mirrors the stmt-context handler at coro_stmt.c:106 — for a
     * suspendable cond, drive its first value via coro_eval+α so generator
     * semantics are preserved. */
    case E_IF: {
        if (e->nchildren < 1) return NULVCL;
        EXPR_t *test = e->children[0];
        DESCR_t cv;
        if (is_suspendable(test)) {
            bb_node_t box = coro_eval(test);
            cv = box.fn(box.ζ, α);
        } else {
            cv = bb_eval_value(test);
        }
        if (!IS_FAIL_fn(cv))
            return (e->nchildren > 1) ? bb_eval_value(e->children[1]) : cv;
        return (e->nchildren > 2) ? bb_eval_value(e->children[2]) : FAILDESCR;
    }

    /* E_PROC_FAIL in value context — mirrors interp_eval.c:2064-2070.
     * Procedure-level fail: set the frame's returning sentinel and return
     * FAILDESCR.  Note: this is the *eager* form, reached when something
     * directly calls bb_eval_value(E_PROC_FAIL).  The lazy form for
     * `expr | fail` alternation lives in coro_eval (RS-23b's icn_lazy_box
     * wrapping at coro_runtime.c:1576) and is unaffected — the lazy box
     * triggers this same case only when the alternation arm is actually
     * pumped, preserving the semantics RS-23b established. */
    case E_PROC_FAIL: {
        if (frame_depth > 0) {
            FRAME.return_val = FAILDESCR;
            FRAME.returning  = 1;
        }
        return FAILDESCR;
    }

    /* E_REVASSIGN in value context — `x <- v`, reversible assign.
     * Mirrors interp_eval.c:606-637 (the standalone path).  Outside `every`
     * no driver backtracks the operation, so we just perform the assign and
     * succeed.  The revert semantics live in coro_bb_revassign and are
     * reached only when coro_eval is asked for a box (every / alt-driven
     * contexts) — that path is unaffected.  Three lvalue shapes: E_VAR
     * (slot or NV name), E_IDX (subscript_set), E_FIELD (data_field_ptr). */
    case E_REVASSIGN: {
        if (e->nchildren < 2) return NULVCL;
        DESCR_t val = bb_eval_value(e->children[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        EXPR_t *lhs = e->children[0];
        if (lhs && lhs->kind == E_VAR) {
            int slot = (int)lhs->ival;
            if (slot >= 0 && slot < FRAME.env_n) FRAME.env[slot] = val;
            else if (slot < 0 && lhs->sval && lhs->sval[0] != '&') set_and_trace(lhs->sval, val);
        } else if (lhs && lhs->kind == E_IDX && lhs->nchildren >= 2) {
            DESCR_t base = bb_eval_value(lhs->children[0]);
            if (!IS_FAIL_fn(base)) {
                DESCR_t idx = bb_eval_value(lhs->children[1]);
                if (!IS_FAIL_fn(idx)) subscript_set(base, idx, val);
            }
        } else if (lhs && lhs->kind == E_FIELD && lhs->sval && lhs->nchildren >= 1) {
            DESCR_t obj = bb_eval_value(lhs->children[0]);
            if (!IS_FAIL_fn(obj)) {
                DESCR_t *cell = data_field_ptr(lhs->sval, obj);
                if (cell) *cell = val;
            }
        }
        return val;
    }

    /* E_BANG_BINARY in value context — `E1 ! E2`, Icon's apply-as-generator.
     * This is a generator combinator with no per-tick scalar state on
     * FRAME.gen — its state lives entirely in the bb_node_t built by
     * coro_eval.  Outer-pump retries reach it through the box's β path,
     * not through re-entry of bb_eval_value.  Mirrors the existing
     * E_LIMIT/E_ALTERNATE/E_SEQ_EXPR pattern at coro_value.c:888-901. */
    case E_BANG_BINARY: {
        /* (2) injection check — outer pump might have already produced
         * a tick we should return verbatim. */
        if (coro_drive_node && e == coro_drive_node) return coro_drive_val;
        /* (3) fresh first-value via coro_eval+α */
        bb_node_t box = coro_eval(e);
        return box.fn(box.ζ, α);
    }

    /* E_LOOP_BREAK in value context — surfaced by the diag after RS-23-extra
     * absorbed E_IF/E_PROC_FAIL/E_BANG_BINARY/E_REVASSIGN.  Pattern: `break`
     * appears as a body whose value is harvested by an enclosing expression
     * (e.g. `expr & break` in a value-context).  Mirrors interp_eval.c:3419-
     * 3422: set the frame's loop_break sentinel and return the optional
     * value child if present, else NULVCL.  The stmt-context handler in
     * coro_stmt.c:69 is the more common path. */
    case E_LOOP_BREAK: {
        FRAME.loop_break = 1;
        return (e->nchildren > 0) ? bb_eval_value(e->children[0]) : NULVCL;
    }

    /* E_RETURN in value context — surfaced by the same diag rerun.  Pattern:
     * `return expr` appears as a body whose value is harvested.  Mirrors
     * interp_eval.c:2053-2061: evaluate the optional value child, set
     * FRAME.return_val and FRAME.returning, and return the value.  At
     * frame_depth 0 we just evaluate the child (no procedure to return
     * from).  The stmt-context handler in coro_stmt.c:80 is the common
     * path; this addition handles the rare value-context arrival. */
    case E_RETURN: {
        if (frame_depth > 0) {
            FRAME.return_val = (e->nchildren > 0)
                ? bb_eval_value(e->children[0]) : NULVCL;
            FRAME.returning = 1;
            return FRAME.return_val;
        }
        return (e->nchildren > 0) ? bb_eval_value(e->children[0]) : NULVCL;
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
            (int)e->kind);
    abort();
}
