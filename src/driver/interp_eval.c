/*
 * interp_eval.c — expression evaluator (interp_eval)
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     /* getpid */
#include "../ast/ast.h"   /* TT_KIND_COUNT */
#include "../runtime/interp/coro_value.h"  /* A3-seed-fix: bb_icn_rnd_seed shared RNG */

/* RS-24 diag: per-kind hit counter for the Icon-frame switch in
 * interp_eval().  See the env-gated init block inside that function for
 * the recording side.  This pointer is set on first call and dumped at
 * exit when RS24_DIAG=1. */
unsigned long *rs24_diag_hits_ptr = NULL;
static const char *rs24_diag_kind_name(int k);
void rs24_diag_dump(void) {
    if (!rs24_diag_hits_ptr) return;
    FILE *fp = fopen("/tmp/rs24_diag_hits.log", "a");
    if (!fp) return;
    fprintf(fp, "=== RS-24 Icon-frame switch hits (pid=%d) ===\n", (int)getpid());
    for (int k = 0; k < (int)TT_KIND_COUNT; k++) {
        if (rs24_diag_hits_ptr[k]) {
            fprintf(fp, "  kind=%-3d %-20s hits=%lu\n",
                    k, rs24_diag_kind_name(k), rs24_diag_hits_ptr[k]);
        }
    }
    fclose(fp);
}
/* Stringify the kinds we care about — only the labels in the Icon-frame switch.
 * For others we emit "?".  Keeps the diag focused. */
static const char *rs24_diag_kind_name(int k) {
    switch (k) {
    case TT_VAR:        return "TT_VAR";
    case TT_ASSIGN:     return "TT_ASSIGN";
    case TT_FNC:        return "TT_FNC";
    case TT_IF:         return "TT_IF";
    case TT_WHILE:      return "TT_WHILE";
    case TT_UNTIL:      return "TT_UNTIL";
    case TT_REPEAT:     return "TT_REPEAT";
    case TT_EVERY:      return "TT_EVERY";
    case TT_SEQ:        return "TT_SEQ";
    case TT_SEQ_EXPR:   return "TT_SEQ_EXPR";
    case TT_ALT:        return "TT_ALT";
    case TT_ALTERNATE:  return "TT_ALTERNATE";
    case TT_REVASSIGN:  return "TT_REVASSIGN";
    case TT_LOOP_NEXT:  return "TT_LOOP_NEXT";
    case TT_SUSPEND:    return "TT_SUSPEND";
    case TT_RETURN:     return "TT_RETURN";
    case TT_PROC_FAIL:  return "TT_PROC_FAIL";
    default:           return "?";
    }
}





/* T-0: set_and_trace — NV_SET_fn + VALUE trace hook for monitor.
 * Use at every plain-variable assignment site in the --ir-run path.
 * Keywords (&STLIMIT etc.) excluded: trace_is_active only fires on
 * names registered via TRACE(var,'VALUE'), never on &-keywords.
 * KW-RETFIX: capture return-value writes into frame->retval_cell to bypass
 * NV keyword collision when user procedure name matches a keyword (e.g. "Trim"). */
void set_and_trace(const char *name, DESCR_t val) {
    /* SN-3: if this name is in the shadow table, update shadow and skip NV_SET_fn
     * (which would be ignored for pattern-primitive names anyway). */
    if (shadow_has(name)) { shadow_set_cur(name, val); goto trace_hook; }
    NV_SET_fn(name, val);
trace_hook:
    if (call_depth > 0) {
        CallFrame *fr = &call_stack[call_depth - 1];
        /* SN-19 stage-2: names arrive canonical, plain strcmp is correct */
        if (name && fr->fname[0] && strcmp(name, fr->fname) == 0) {
            fr->retval_cell = val;
            fr->retval_set  = 1;
        }
    }
    /* SN-26-binmon-3way: comm_var now fires post-commit inside NV_SET_fn,
     * so an additional call here would double-emit on every assignment.
     * The shadow path needs comm_var because shadow_set_cur doesn't pass
     * through NV_SET_fn. */
    if (shadow_has(name) && name && name[0] != '&' && trace_is_active(name))
        comm_var(name, val);
}

/* DYN-57: TT_FNC names that always yield a pattern value.
 * Mirrors PAT_FNC_NAMES in SJ-17 (sno-interp.js ec6c0b3).
 * Scoped to _expr_is_pat only — do NOT intercept TT_VAR (breaks 210_indirect_ref)
 * and do NOT touch S=PR split/has_eq guard (breaks 062_capture_replacement). */
static const char *PAT_FNC_NAMES[] = {
    "ANY","NOTANY","SPAN","BREAK","BREAKX","LEN","POS","RPOS","TAB","RTAB",
    "ARB","ARBNO","REM","FAIL","SUCCEED","FENCE","ABORT","BAL","CALL", NULL
};
int _is_pat_fnc_name(const char *s) {
    if (!s) return 0;
    /* SN-19 stage-2: AST sval arrives canonical from lexer fold, plain strcmp correct */
    for (int i = 0; PAT_FNC_NAMES[i]; i++)
        if (strcmp(s, PAT_FNC_NAMES[i]) == 0) return 1;
    return 0;
}

/* DYN-54: returns 1 if expr tree contains any pattern-only node.
 * Mirrors is_pat() in snobol4.y but accessible at eval time. */
int _expr_is_pat(tree_t *e) {
    if (!e) return 0;
    switch (e->t) {
        case TT_ARB: case TT_ARBNO: case TT_CAPT_COND_ASGN:
        case TT_CAPT_IMMED_ASGN: case TT_CAPT_CURSOR: case TT_DEFER:
            return 1;
        default: break;
    }
    /* DYN-57: TT_FNC whose name is a pattern primitive (LEN, POS, TAB, ARB, etc.) */
    if (e->t == TT_FNC && _is_pat_fnc_name(e->v.sval)) return 1;
    /* DYN-58: TT_VAR whose name is a zero-arg pattern primitive (ARB, REM, FAIL, etc.)
     * Only in _expr_is_pat — do NOT change the general TT_VAR eval path (breaks 210). */
    if (e->t == TT_VAR && _is_pat_fnc_name(e->v.sval)) return 1;
    for (int i = 0; i < e->n; i++)
        if (_expr_is_pat(e->c[i])) return 1;
    return 0;
}

/* BP-1: return interior ptr into DATA instance field, or NULL if not found.
 * SN-19 arch fix: case-policy-neutral shared runtime. Each frontend enforces
 * its own case policy at ingest (SNOBOL4 lex-folds + _builtin_DATA pre-folds;
 * Icon/Raku preserve). Within one language, stored fields and lookup keys
 * have identical case convention — plain strcmp is correct by construction. */
DESCR_t *data_field_ptr(const char *fname, DESCR_t inst) {
    if (inst.v < DT_DATA || !inst.u) return NULL;
    DATBLK_t *blk = inst.u->type;
    if (!blk) return NULL;
    for (int i = 0; i < blk->nfields; i++)
        if (blk->fields[i] && strcmp(blk->fields[i], fname) == 0)
            return &inst.u->fields[i];
    return NULL;
}

/* IC-9: Icon string-section assignment.
 * Implements `s[i:j] := v`, `s[i+:n] := v`, `s[i-:n] := v`, and `s[i] := v`
 * when `s` is a string variable.
 *
 * Strings in Icon are immutable values — but variables holding strings can
 * have a section "replaced" by rebuilding the whole string and writing it
 * back to the underlying variable cell.
 *
 * Returns 1 on success (cell written), 0 on FAIL (OOB) or if the LHS shape
 * is not handled here.  Caller should treat 0 as FAIL of the whole assign
 * unless the LHS kind is TT_IDX (in which case caller falls back to
 * subscript_set for list/table semantics).
 *
 * Only handles the simple case: lhs->c[0] is an addressable lvalue
 * (TT_VAR / TT_FIELD / TT_NAME / TT_INDIRECT) whose current value is a string.
 * Nested patterns like `t[k][i:j] := v` are not (yet) supported. */
int icn_string_section_assign(tree_t *lhs, DESCR_t val) {
    if (!lhs) return 0;
    int kind = lhs->t;
    if (kind != TT_SECTION && kind != TT_SECTION_PLUS &&
        kind != TT_SECTION_MINUS && kind != TT_IDX) return 0;
    if (lhs->n < 2) return 0;
    if (kind == TT_SECTION && lhs->n < 3) return 0;

    /* Get a pointer to the underlying cell (so we can write back).  Prefer
     * local slot for TT_VAR (when in an icon frame), falling back to NV via
     * interp_eval_ref.  This mirrors the read-side logic in case TT_VAR. */
    tree_t *bch = lhs->c[0];
    DESCR_t *cell = NULL;
    if (bch && bch->t == TT_VAR && frame_depth > 0) {
        int sl = (int)bch->v.ival;
        if (sl >= 0 && sl < FRAME.env_n) cell = &FRAME.env[sl];
    }
    if (!cell) cell = interp_eval_ref(bch);
    if (!cell) return 0;
    DESCR_t base = *cell;
    /* For TT_IDX: only handle string base — list/table goes through subscript_set. */
    if (kind == TT_IDX) {
        if (base.v != DT_S && base.v != DT_SNUL) return 0;
    }
    const char *s = (base.v == DT_SNUL) ? "" : VARVAL_fn(base);
    if (!s) s = "";
    int slen = (int)strlen(s);

    /* Compute lo, hi (1-based section bounds, lo ≤ hi, range [lo, hi)). */
    int lo = 0, hi = 0;
    if (kind == TT_SECTION) {
        int i = (int)to_int(interp_eval(lhs->c[1]));
        int j = (int)to_int(interp_eval(lhs->c[2]));
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (j == 0) j = slen + 1; else if (j < 0) j = slen + 1 + j;
        if (i < 1 || i > slen+1 || j < 1 || j > slen+1) return 0;
        lo = i < j ? i : j; hi = i < j ? j : i;
    } else if (kind == TT_SECTION_PLUS) {
        int i = (int)to_int(interp_eval(lhs->c[1]));
        int n = (int)to_int(interp_eval(lhs->c[2]));
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return 0;
        if (n < 0) return 0;
        if (i + n > slen + 1) return 0;
        lo = i; hi = i + n;
    } else if (kind == TT_SECTION_MINUS) {
        int i = (int)to_int(interp_eval(lhs->c[1]));
        int n = (int)to_int(interp_eval(lhs->c[2]));
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return 0;
        if (n < 0) return 0;
        if (i - n < 1) return 0;
        lo = i - n; hi = i;
    } else { /* TT_IDX */
        int i = (int)to_int(interp_eval(lhs->c[1]));
        if (i == 0) return 0;
        if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen) return 0;  /* single-char index must point at a real char */
        lo = i; hi = i + 1;
    }

    /* Build new string = s[1..lo) + val + s[hi..slen+1). */
    const char *vs = VARVAL_fn(val); if (!vs) vs = "";
    int vlen = (int)strlen(vs);
    int prefix = lo - 1;             /* chars before the section */
    int suffix = slen - (hi - 1);    /* chars after the section */
    int newlen = prefix + vlen + suffix;
    char *buf = (char *)GC_malloc((size_t)newlen + 1);
    if (prefix > 0) memcpy(buf, s, (size_t)prefix);
    if (vlen > 0)   memcpy(buf + prefix, vs, (size_t)vlen);
    if (suffix > 0) memcpy(buf + prefix + vlen, s + hi - 1, (size_t)suffix);
    buf[newlen] = '\0';

    /* Write back to the cell.  If the base is a global variable, also route
     * through set_and_trace so VALUE traces fire — mirrors TT_ASSIGN. */
    tree_t *base_expr = lhs->c[0];
    if (base_expr && base_expr->t == TT_VAR && base_expr->v.sval &&
        base_expr->v.sval[0] != '&' &&
        !(frame_depth > 0 && base_expr->v.ival >= 0 && base_expr->v.ival < FRAME.env_n))
    {
        set_and_trace(base_expr->v.sval, STRVAL(buf));
    } else {
        *cell = STRVAL(buf);
    }
    return 1;
}



/* ══════════════════════════════════════════════════════════════════════════
 * eval_node_interp — thin wrapper; reuses eval_node from eval_code.c
 * via eval_expr (we call it by re-parsing for non-trivial exprs,
 * but for the common case we use NV_GET_fn / NV_SET_fn directly).
 *
 * For the interpreter we need direct tree_t evaluation, not string
 * re-parse.  We replicate the minimal logic needed here rather than
 * exposing eval_node (which is static in eval_code.c).
 * ══════════════════════════════════════════════════════════════════════════ */

/* find_leaf_suspendable — walk an expr tree and return the first generator-kind node.
 * Used by TT_EVERY special-case to find the raw TT_TO (or similar) inside
 * compound exprs like TT_ADD(TT_VAR(total), TT_TO(1,n)), so we can drive only
 * the generator and inject via coro_drive_node, letting interp_eval re-read
 * mutable variables (e.g. frame locals) fresh each tick.
 *
 * RS-23c: definition lifted to coro_runtime.c (declared in coro_runtime.h)
 * so coro_value.c / coro_stmt.c can share the single copy. */
#include "../runtime/interp/coro_runtime.h"

/* real_str — format a real for Icon output using shortest round-trip representation.
 * Tries precisions 15..17 and picks the shortest that parses back to the same double. */
const char *real_str(double r, char *buf, int bufsz) {
    for (int p = 15; p <= 17; p++) {
        snprintf(buf, bufsz, "%.*g", p, r);
        char *end; double back = strtod(buf, &end);
        if (back == r) break;   /* shortest precision that round-trips */
    }
    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E') && !strchr(buf, 'n') && !strchr(buf, 'N'))
        strncat(buf, ".0", bufsz - strlen(buf) - 1);
    return buf;
}

/* CH-17g-runtime-bridge-1 (2026-05-09): name-based tree_t-free Icon builtin
 * dispatch.  Returns 1 if the call was handled (and writes the result to
 * *out), 0 otherwise.  The caller has no tree_t handle: only the function
 * name and pre-evaluated args.
 *
 * This helper is the bridge that lets SM_CALL_FN in sm_interp.c dispatch
 * Icon builtins from inside expression bodies.  Today the expression emits e.g.
 *
 *     SM_PUSH_LIT_S "hello"
 *     SM_CALL_FN s="write" nargs=1
 *
 * and SM_CALL_FN's handler walks INVOKE_fn / APPLY_fn (the SNOBOL4 builtin
 * registry).  Icon's `write` lives in icn_call_builtin (below) on the
 * legacy IR-walker path and is never registered through register_fn, so
 * APPLY_fn returns FAIL and the expression surfaces "Error 5: Undefined
 * function or operation."  Wiring this helper into SM_CALL_FN after the
 * INVOKE_fn fallback closes that gap (CH-17g-runtime-bridge-2).
 *
 * Scope today: write, writes.  Each branch is a copy of the same logic
 * in icn_call_builtin's body — kept in lockstep by having icn_call_builtin
 * delegate here.  Future rungs may extend coverage to other tree_t-free
 * Icon builtins (integer, string, real, char, type, copy, list, table,
 * read, repl, upto, find, any, many, tab, move, match, …) — each kind
 * migrates by adding a branch here AND removing its branch from
 * icn_call_builtin's tail (or its current home in interp_eval's TT_FNC
 * switch) in the same commit.
 *
 * Builtins that need tree_t (Raku/SCAN dispatch helpers, mutators that
 * write back through children[1]'s lvalue identity, generator builtins
 * that inspect children[i] structurally) are NOT covered here and remain
 * with icn_call_builtin / interp_eval.  Those become per-kind expression
 * producer/consumer migrations under CH-17h. */
int icn_try_call_builtin_by_name(const char *fn, DESCR_t *args, int nargs, DESCR_t *out)
{
    if (!fn || !out) return 0;
    /* write(x1,...,xN) — concatenate all args, append newline.
     * Icon semantics: any FAIL arg propagates; &null arg writes empty. */
    if (!strcmp(fn, "write")) {
        for (int _wi = 0; _wi < nargs; _wi++) {
            DESCR_t av = args[_wi];
            if (IS_FAIL_fn(av)) { *out = FAILDESCR; return 1; }
            if (av.v == DT_SNUL) continue;   /* &null → empty */
            if (IS_INT_fn(av))       printf("%lld", (long long)av.i);
            else if (IS_REAL_fn(av)) { char _rb[64]; printf("%s", real_str(av.r,_rb,sizeof _rb)); }
            else { const char *s = VARVAL_fn(av); if (s) fputs(s, stdout); }
        }
        putchar('\n');
        *out = nargs > 0 ? args[nargs-1] : NULVCL;
        return 1;
    }
    /* writes(x1,...,xN) — same but no newline */
    if (!strcmp(fn, "writes")) {
        for (int _wi = 0; _wi < nargs; _wi++) {
            DESCR_t av = args[_wi];
            if (IS_FAIL_fn(av)) { *out = FAILDESCR; return 1; }
            if (av.v == DT_SNUL) continue;
            if (IS_INT_fn(av))       printf("%lld", (long long)av.i);
            else if (IS_REAL_fn(av)) { char _rb[64]; printf("%s", real_str(av.r,_rb,sizeof _rb)); }
            else { const char *s = VARVAL_fn(av); if (s) fputs(s, stdout); }
        }
        *out = nargs > 0 ? args[nargs-1] : NULVCL;
        return 1;
    }
    /* CH-17g-runtime-bridge-3 (2026-05-09): single-arg pure value-transforms.
     * Each branch is a verbatim copy of the equivalent in-eval branch, with
     * `interp_eval(e->c[i])` replaced by `args[i-1]` (already
     * pre-evaluated by the SM_CALL_FN handler).  No tree_t access.
     *
     * Builtins covered: integer, real, string, numeric, char, ord, type,
     * image (0 or 1 arg), copy, *e (size-of). */

    /* integer(x) — Icon spec: int → int; real → trunc to int; string → parse
     * (with optional radix `BASErDIGITS`).  IC-9 (2026-05-01). */
    if (!strcmp(fn,"integer") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_INT_fn(av))  { *out = av; return 1; }
        if (IS_REAL_fn(av)) { *out = INTVAL((long long)av.r); return 1; }
        const char *s = VARVAL_fn(av); if (!s) { *out = FAILDESCR; return 1; }
        {
            const char *p = s;
            while (*p == ' ' || *p == '\t') p++;
            int neg = 0; if (*p == '+') p++; else if (*p == '-') { neg = 1; p++; }
            int base = 0; const char *bstart = p;
            while (*p >= '0' && *p <= '9') { base = base * 10 + (*p - '0'); p++; }
            if (p > bstart && (*p == 'r' || *p == 'R') && base >= 2 && base <= 36) {
                p++; const char *dstart = p; long long v = 0;
                while (*p) {
                    int d = -1;
                    if (*p >= '0' && *p <= '9') d = *p - '0';
                    else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
                    else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
                    if (d < 0 || d >= base) break;
                    v = v * base + d;
                    p++;
                }
                while (*p == ' ' || *p == '\t') p++;
                if (p > dstart && *p == '\0') { *out = INTVAL(neg ? -v : v); return 1; }
            }
        }
        char *end; long long iv = strtoll(s, &end, 10);
        if (end != s && (*end=='\0'||*end==' ')) { *out = INTVAL(iv); return 1; }
        double rv = strtod(s, &end);
        if (end != s && (*end=='\0'||*end==' ')) { *out = INTVAL((long long)rv); return 1; }
        *out = FAILDESCR; return 1;
    }
    if (!strcmp(fn,"real") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_REAL_fn(av)) { *out = av; return 1; }
        if (IS_INT_fn(av))  { *out = REALVAL((double)av.i); return 1; }
        const char *s = VARVAL_fn(av); if (!s) { *out = FAILDESCR; return 1; }
        char *end; double rv = strtod(s, &end);
        if (end != s && (*end=='\0'||*end==' ')) { *out = REALVAL(rv); return 1; }
        *out = FAILDESCR; return 1;
    }
    if (!strcmp(fn,"string") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_STR_fn(av)) { *out = av; return 1; }
        char *buf = GC_malloc(64);
        if (IS_INT_fn(av))       snprintf(buf,64,"%lld",(long long)av.i);
        else if (IS_REAL_fn(av)) { real_str(av.r,buf,64); }
        else { *out = NULVCL; return 1; }
        *out = STRVAL(buf); return 1;
    }
    if (!strcmp(fn,"numeric") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_INT_fn(av)||IS_REAL_fn(av)) { *out = av; return 1; }
        const char *s = VARVAL_fn(av); if (!s||!*s) { *out = FAILDESCR; return 1; }
        {
            const char *p = s;
            while (*p == ' ' || *p == '\t') p++;
            int neg = 0; if (*p == '+') p++; else if (*p == '-') { neg = 1; p++; }
            int base = 0; const char *bstart = p;
            while (*p >= '0' && *p <= '9') { base = base * 10 + (*p - '0'); p++; }
            if (p > bstart && (*p == 'r' || *p == 'R') && base >= 2 && base <= 36) {
                p++; const char *dstart = p; long long v = 0;
                while (*p) {
                    int d = -1;
                    if (*p >= '0' && *p <= '9') d = *p - '0';
                    else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
                    else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
                    if (d < 0 || d >= base) break;
                    v = v * base + d;
                    p++;
                }
                while (*p == ' ' || *p == '\t') p++;
                if (p > dstart && *p == '\0') { *out = INTVAL(neg ? -v : v); return 1; }
            }
        }
        char *end; long long iv = strtoll(s, &end, 10);
        if (end != s && (*end=='\0'||*end==' ')) { *out = INTVAL(iv); return 1; }
        double rv = strtod(s, &end);
        if (end != s && (*end=='\0'||*end==' ')) { *out = REALVAL(rv); return 1; }
        *out = FAILDESCR; return 1;
    }
    if (!strcmp(fn,"char") && nargs == 1) {
        DESCR_t av = args[0];
        int n = (int)(IS_INT_fn(av) ? av.i : (long long)strtol(VARVAL_fn(av)?VARVAL_fn(av):"0",NULL,10));
        char *buf = GC_malloc(2); buf[0]=(char)(n&0xFF); buf[1]='\0';
        *out = STRVAL(buf); return 1;
    }
    if (!strcmp(fn,"ord") && nargs == 1) {
        DESCR_t av = args[0];
        const char *s = VARVAL_fn(av); if (!s||!*s) { *out = FAILDESCR; return 1; }
        *out = INTVAL((unsigned char)s[0]); return 1;
    }
    if (!strcmp(fn,"type") && nargs == 1) {
        DESCR_t av = args[0];
        const char *t;
        if (IS_INT_fn(av))       t="integer";
        else if (IS_REAL_fn(av)) t="real";
        else if (av.v==DT_T)     t="table";
        else if (av.v==DT_A)     t="list";
        else if (av.v==DT_DATA)  {
            DESCR_t tag = FIELD_GET_fn(av,"icn_type");
            t = (tag.v==DT_S && tag.s) ? tag.s : "record";
        }
        else t="string";
        *out = STRVAL(t); return 1;
    }
    if (!strcmp(fn,"image") && nargs == 0) {
        *out = STRVAL("&null"); return 1;
    }
    /* IJ-3: args(proc_val) — return arity of a procedure value.
     * args(f) where f is DT_E (user proc): return nparams (-1 = varargs, 0 = none).
     * args(f) where f is DT_S (builtin name): return -2 (vararg convention). */
    if (!strcmp(fn,"args") && nargs == 1) {
        DESCR_t a = args[0];
        if (a.v == DT_E) {
            /* Look up proc by entry_pc */
            for (int i=0;i<proc_count;i++) {
                if (proc_table[i].entry_pc == (int)a.i) {
                    *out = INTVAL(proc_table[i].nparams <= 0 ? -2 : proc_table[i].nparams);
                    return 1;
                }
            }
            *out = INTVAL(-2); return 1;
        }
        if (IS_STR_fn(a)) {
            /* Builtin: -2 = vararg */
            *out = INTVAL(-2); return 1;
        }
        *out = FAILDESCR; return 1;
    }
    /* IJ-3: proc(name, arity) — look up procedure by name+arity */
    if (!strcmp(fn,"proc") && nargs == 2) {
        const char *pname = VARVAL_fn(args[0]);
        int arity = (int)to_int(args[1]);
        if (!pname) { *out = FAILDESCR; return 1; }
        for (int i = 0; i < proc_count; i++) {
            if (proc_table[i].name && strcmp(proc_table[i].name, pname) == 0) {
                if (arity < 0 || proc_table[i].nparams == arity || proc_table[i].nparams <= 0) {
                    DESCR_t pv; pv.v = DT_E;
                    pv.slen = (uint32_t)(arity >= 0 ? arity : 0);
                    pv.i    = proc_table[i].entry_pc;
                    *out = pv; return 1;
                }
            }
        }
        /* Builtin proc: return DT_S with the name as a callable sentinel */
        *out = STRVAL(GC_strdup(pname)); return 1;
    }
    if (!strcmp(fn,"image") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_FAIL_fn(av)) { *out = FAILDESCR; return 1; }
        char *buf = GC_malloc(256);
        if (av.v == DT_SNUL)     { *out = STRVAL("&null"); return 1; }
        /* IJ-3: file values — integer FH index with name in raku_fh_name */
        if (IS_INT_fn(av)) {
            int idx = (int)av.i;
            if (idx >= 0 && idx < RAKU_FH_MAX && raku_fh_name[idx]) {
                snprintf(buf,256,"file(%s)",raku_fh_name[idx]);
                *out = STRVAL(buf); return 1;
            }
            snprintf(buf,256,"%lld",(long long)av.i); *out = STRVAL(buf); return 1;
        }
        if (IS_REAL_fn(av))      { real_str(av.r,buf,128); *out = STRVAL(buf); return 1; }
        if (av.v==DT_T)          { snprintf(buf,128,"table(%d)",av.tbl?av.tbl->size:0); *out = STRVAL(buf); return 1; }
        /* IJ-3: DT_DATA — distinguish icnlist from user records */
        if (av.v==DT_DATA && av.u) {
            const char *tname = av.u->type ? av.u->type->name : "record";
            if (strcmp(tname,"icnlist")==0) {
                int cnt = (av.u->type && av.u->type->nfields>=2 && av.u->fields)
                          ? (int)av.u->fields[1].i : 0;
                snprintf(buf,128,"list(%d)",cnt); *out = STRVAL(buf); return 1;
            }
            snprintf(buf,256,"record(%s)",tname); *out = STRVAL(buf); return 1;
        }
        if (av.v==DT_DATA)       { *out = STRVAL("record"); return 1; }
        /* IJ-3: DT_E = procedure value */
        if (av.v==DT_E) {
            for (int i=0;i<proc_count;i++)
                if (proc_table[i].entry_pc==(int)av.i)
                    { snprintf(buf,128,"procedure %s",proc_table[i].name); *out=STRVAL(buf); return 1; }
            snprintf(buf,128,"procedure"); *out=STRVAL(buf); return 1;
        }
        /* IJ-4: DT_S builtin proc name → "function name" */
        if (IS_STR_fn(av) && av.s) {
            /* Check if it's a known builtin name; if so emit "function name" */
            extern DESCR_t icn_proc_as_value(const char *);
            DESCR_t pv = icn_proc_as_value(av.s);
            if (pv.v == DT_S) {  /* confirmed builtin */
                snprintf(buf, 128, "function %s", av.s);
                *out = STRVAL(buf); return 1;
            }
        }
        const char *s=VARVAL_fn(av); if (!s) s = "";
        int sl = (int)strlen(s);
        char *outs = GC_malloc(sl*4 + 3);
        int o = 0;
        outs[o++] = '"';
        for (int i = 0; i < sl; i++) {
            unsigned char c = (unsigned char)s[i];
            switch (c) {
                case '"':  outs[o++]='\\'; outs[o++]='"';  break;
                case '\\': outs[o++]='\\'; outs[o++]='\\'; break;
                case '\n': outs[o++]='\\'; outs[o++]='n';  break;
                case '\t': outs[o++]='\\'; outs[o++]='t';  break;
                case '\r': outs[o++]='\\'; outs[o++]='r';  break;
                default:
                    if (c < 0x20 || c == 0x7f) {
                        o += snprintf(outs+o, 5, "\\x%02x", c);
                    } else {
                        outs[o++] = (char)c;
                    }
            }
        }
        outs[o++] = '"';
        outs[o] = '\0';
        *out = STRVAL(outs); return 1;
    }
    /* IJ-4: image(val, width) — width arg ignored per Icon 9.5 oracle.
     * Also handles DT_S builtin proc names: image(sqrt,15) → "function sqrt". */
    if (!strcmp(fn,"image") && nargs == 2) {
        DESCR_t av = args[0];
        /* DT_S builtin proc name */
        if (IS_STR_fn(av) && av.s) {
            char *buf = GC_malloc(64);
            snprintf(buf, 64, "function %s", av.s);
            *out = STRVAL(buf); return 1;
        }
        /* DT_E user proc */
        if (av.v == DT_E) {
            char *buf = GC_malloc(128);
            for (int i=0;i<proc_count;i++)
                if (proc_table[i].entry_pc==(int)av.i)
                    { snprintf(buf,128,"procedure %s",proc_table[i].name); *out=STRVAL(buf); return 1; }
            snprintf(buf,128,"procedure"); *out=STRVAL(buf); return 1;
        }
        /* Delegate to image(val) for all other types */
        DESCR_t one_out = FAILDESCR;
        if (icn_try_call_builtin_by_name("image", args, 1, &one_out))
            { *out = one_out; return 1; }
        *out = FAILDESCR; return 1;
    }
    /* CH-17g-runtime-bridge-3 BATCH 2 (2026-05-09): multi-arg pure transforms.
     * Same constraints as batch 1 — verbatim port of in-eval branches with
     * `interp_eval(e->c[i])` → `args[i-1]`.  All tree_t-free, no
     * write-back, no &pos/&subject mutation.  g_lang reads (in `trim`) are
     * safe because polyglot_execute sets g_lang=1 before any Icon proc runs,
     * regardless of --ir-run vs --sm-run path. */

    /* repl(s,n) — n-fold concat */
    if (!strcmp(fn,"repl") && nargs == 2) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        int n=(int)to_int(args[1]); if(n<0)n=0;
        int sl=(int)strlen(s); char *buf=GC_malloc(sl*n+1); buf[0]='\0';
        for(int i=0;i<n;i++) memcpy(buf+i*sl,s,sl); buf[sl*n]='\0';
        *out = STRVAL(buf); return 1;
    }
    /* reverse(s) — string reverse */
    if (!strcmp(fn,"reverse") && nargs == 1) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        int sl=(int)strlen(s); char *buf=GC_malloc(sl+1);
        for(int i=0;i<sl;i++) buf[i]=s[sl-1-i]; buf[sl]='\0';
        *out = STRVAL(buf); return 1;
    }
    /* map(s, c1, c2) — char-class mapping; defaults: c1=ucase, c2=lcase */
    if (!strcmp(fn,"map") && nargs >= 1 && nargs <= 3) {
        static const char *UCASE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        static const char *LCASE = "abcdefghijklmnopqrstuvwxyz";
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        const char *from = UCASE, *to = LCASE;
        if (nargs >= 2) {
            DESCR_t fv=args[1];
            if (fv.v != DT_SNUL) {
                const char *fs = VARVAL_fn(fv);
                if (fs) from = fs;
            }
        }
        if (nargs >= 3) {
            DESCR_t tv=args[2];
            if (tv.v != DT_SNUL) {
                const char *ts = VARVAL_fn(tv);
                if (ts) to = ts;
            }
        }
        int sl=(int)strlen(s); char *buf=GC_malloc(sl+1);
        int fl=(int)strlen(from), tl=(int)strlen(to);
        for (int i=0;i<sl;i++) {
            char c=s[i]; int hit=0;
            for (int j=fl-1;j>=0;j--) {
                if (from[j]==c) { buf[i] = (j<tl) ? to[j] : c; hit=1; break; }
            }
            if (!hit) buf[i]=c;
        }
        buf[sl]='\0'; *out = STRVAL(buf); return 1;
    }
    /* trim(s, [c]) — Icon: trailing chars in cset; Raku: both ends, whitespace */
    if (!strcmp(fn,"trim") && (nargs == 1 || nargs == 2)) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        const char *cset = " ";
        if (nargs == 2) {
            DESCR_t cv = args[1];
            if (cv.v != DT_SNUL) {
                const char *cs = VARVAL_fn(cv);
                if (cs) cset = cs;
            }
        }
        if (g_lang == 1 || nargs == 2) {
            int sl=(int)strlen(s);
            while (sl > 0 && strchr(cset, s[sl-1])) sl--;
            char *buf=GC_malloc(sl+1); memcpy(buf,s,sl); buf[sl]='\0';
            *out = STRVAL(buf); return 1;
        } else {
            while(*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
            size_t len=strlen(s);
            while(len>0&&(s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r')) len--;
            char *buf=GC_malloc(len+1); memcpy(buf,s,len); buf[len]='\0';
            *out = STRVAL(buf); return 1;
        }
    }
    /* left(s, [n], [p]) — pad/truncate string left-aligned */
    if (!strcmp(fn,"left") && nargs >= 1) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        int sl=(int)strlen(s);
        int n = 1;
        if (nargs >= 2) {
            DESCR_t nv = args[1];
            if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) n = (int)to_int(nv);
        }
        if (n < 0) n = 0;
        const char *fill=" "; int fl=1;
        if (nargs >= 3) {
            DESCR_t fd = args[2];
            if (!IS_FAIL_fn(fd) && fd.v != DT_SNUL) {
                const char *fs = VARVAL_fn(fd);
                if (fs && *fs) { fill = fs; fl = (int)strlen(fs); }
            }
        }
        char *buf=GC_malloc(n+1);
        int copy = sl < n ? sl : n;
        for (int i = 0; i < copy; i++) buf[i] = s[i];
        int rpad = n - copy;
        for (int k = 0; k < rpad; k++) {
            int idx = ((k + fl - rpad) % fl + fl) % fl;
            buf[copy + k] = fill[idx];
        }
        buf[n]='\0';
        *out = STRVAL(buf); return 1;
    }
    /* right(s, [n], [p]) — pad/truncate string right-aligned */
    if (!strcmp(fn,"right") && nargs >= 1) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        int sl=(int)strlen(s);
        int n = 1;
        if (nargs >= 2) {
            DESCR_t nv = args[1];
            if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) n = (int)to_int(nv);
        }
        if (n < 0) n = 0;
        const char *fill=" "; int fl=1;
        if (nargs >= 3) {
            DESCR_t fd = args[2];
            if (!IS_FAIL_fn(fd) && fd.v != DT_SNUL) {
                const char *fs = VARVAL_fn(fd);
                if (fs && *fs) { fill = fs; fl = (int)strlen(fs); }
            }
        }
        char *buf=GC_malloc(n+1);
        int pad = n - sl; if (pad < 0) pad = 0;
        for (int i = 0; i < pad; i++) buf[i] = fill[i % fl];
        int srcoff = (sl > n) ? (sl - n) : 0;
        int copy = sl - srcoff; if (pad + copy > n) copy = n - pad;
        for (int i = 0; i < copy; i++) buf[pad + i] = s[srcoff + i];
        buf[n]='\0';
        *out = STRVAL(buf); return 1;
    }
    /* center(s, [n], [p]) — pad/truncate centered */
    if (!strcmp(fn,"center") && nargs >= 1) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        int sl=(int)strlen(s);
        int n = 1;
        if (nargs >= 2) {
            DESCR_t nv = args[1];
            if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) n = (int)to_int(nv);
        }
        if (n < 0) n = 0;
        const char *fill=" "; int fl=1;
        if (nargs >= 3) {
            DESCR_t fd = args[2];
            if (!IS_FAIL_fn(fd) && fd.v != DT_SNUL) {
                const char *fs = VARVAL_fn(fd);
                if (fs && *fs) { fill = fs; fl = (int)strlen(fs); }
            }
        }
        char *buf=GC_malloc(n+1);
        int lpad = (n - sl) / 2; if (lpad < 0) lpad = 0;
        int srcoff = (sl > n) ? (sl - n + 1) / 2 : 0;
        int copy = sl - srcoff; if (lpad + copy > n) copy = n - lpad;
        int rpad = n - lpad - copy;
        for (int i = 0; i < lpad; i++) buf[i] = fill[i % fl];
        for (int i = 0; i < copy; i++) buf[lpad + i] = s[srcoff + i];
        for (int k = 0; k < rpad; k++) {
            int idx = ((k + fl - rpad) % fl + fl) % fl;
            buf[lpad + copy + k] = fill[idx];
        }
        buf[n]='\0';
        *out = STRVAL(buf); return 1;
    }
    /* IJ-5: detab(s,t...) — expand tabs; entab(s,t...) — compress spaces to tabs */
    if (!strcmp(fn,"detab") && nargs >= 1) {
        /* arg0 must be string/cset (coercible); tab stops must be integer/real */
        if (args[0].v == DT_I || args[0].v == DT_R) { *out = FAILDESCR; return 1; }
        const char *s = VARVAL_fn(args[0]); if (!s) s = "";
        int stops[32], nstops = 0;
        for (int j = 1; j < nargs && nstops < 32; j++) {
            if (IS_FAIL_fn(args[j]) || args[j].v == DT_SNUL) continue;
            if (!IS_INT_fn(args[j]) && !IS_REAL_fn(args[j])) { *out = FAILDESCR; return 1; }
            stops[nstops++] = (int)to_int(args[j]);
        }
        if (nstops == 0) { stops[0] = 9; nstops = 1; }
        /* gap beyond last explicit stop: if one stop, period = stop value;
         * if two+, period = last - second-to-last */
        int gap = (nstops >= 2) ? stops[nstops-1] - stops[nstops-2] : stops[0];
        if (gap < 1) gap = 1;
        int cap = 4096; char *buf = GC_malloc(cap); int bi = 0, col = 0;
        for (int i = 0; s[i]; i++) {
            if (s[i] == '\t') {
                int next = -1;
                for (int k = 0; k < nstops; k++) if (stops[k] > col+1) { next=stops[k]; break; }
                if (next < 0) {
                    int base = stops[nstops-1];
                    int beyond = col + 1 - base; /* how far past last stop (0-based) */
                    next = base + ((beyond / gap) + 1) * gap;
                }
                int sp = next - (col+1);
                while (sp-- > 0) { if (bi>=cap-1){cap*=2;buf=GC_realloc(buf,cap);} buf[bi++]=' '; col++; }
            } else { if (bi>=cap-1){cap*=2;buf=GC_realloc(buf,cap);} buf[bi++]=s[i]; col++; }
        }
        buf[bi]='\0'; *out=STRVAL(buf); return 1;
    }
    if (!strcmp(fn,"entab") && nargs >= 1) {
        if (args[0].v == DT_I || args[0].v == DT_R) { *out = FAILDESCR; return 1; }
        const char *s = VARVAL_fn(args[0]); if (!s) s = "";
        int stops[32], nstops = 0;
        for (int j = 1; j < nargs && nstops < 32; j++) {
            if (IS_FAIL_fn(args[j]) || args[j].v == DT_SNUL) continue;
            if (!IS_INT_fn(args[j]) && !IS_REAL_fn(args[j])) { *out = FAILDESCR; return 1; }
            stops[nstops++] = (int)to_int(args[j]);
        }
        if (nstops == 0) { stops[0] = 9; nstops = 1; }
        int gap = (nstops >= 2) ? stops[nstops-1] - stops[nstops-2] : stops[0];
        if (gap < 1) gap = 1;
        int cap = 4096; char *buf = GC_malloc(cap); int bi = 0, col = 0;
        int slen = (int)strlen(s);
        for (int i = 0; i <= slen; ) {
            if (i < slen && s[i] == ' ') {
                int run_start = col, j = i;
                while (j < slen && s[j]==' ') { j++; col++; }
                int sc = run_start;
                while (sc < col) {
                    int next = -1;
                    for (int k = 0; k < nstops; k++) if (stops[k] > sc+1) { next=stops[k]; break; }
                    if (next < 0) {
                        int base = stops[nstops-1];
                        int beyond = sc + 1 - base;
                        next = base + ((beyond / gap) + 1) * gap;
                    }
                    if (next-1 <= col) { if(bi>=cap-1){cap*=2;buf=GC_realloc(buf,cap);} buf[bi++]='\t'; sc=next-1; }
                    else { if(bi>=cap-1){cap*=2;buf=GC_realloc(buf,cap);} buf[bi++]=' '; sc++; }
                }
                i = j;
            } else {
                if (i < slen) { if(bi>=cap-1){cap*=2;buf=GC_realloc(buf,cap);} buf[bi++]=s[i]; col++; }
                i++;
            }
        }
        buf[bi]='\0'; *out=STRVAL(buf); return 1;
    }
    /* abs(x), max(a,b,...), min(a,b,...), sqrt(x) — math */
    if (!strcmp(fn,"abs") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_REAL_fn(av)) { *out = REALVAL(fabs(av.r)); return 1; }
        *out = INTVAL(av.i < 0 ? -av.i : av.i); return 1;
    }
    if (!strcmp(fn,"max") && nargs >= 2) {
        DESCR_t best = args[0];
        for (int _j = 1; _j < nargs; _j++) {
            DESCR_t cv = args[_j];
            int gt = (IS_REAL_fn(best)||IS_REAL_fn(cv))
                ? ((IS_REAL_fn(best)?best.r:(double)best.i) < (IS_REAL_fn(cv)?cv.r:(double)cv.i))
                : (best.i < cv.i);
            if (gt) best = cv;
        }
        *out = best; return 1;
    }
    if (!strcmp(fn,"min") && nargs >= 2) {
        DESCR_t best = args[0];
        for (int _j = 1; _j < nargs; _j++) {
            DESCR_t cv = args[_j];
            int lt = (IS_REAL_fn(best)||IS_REAL_fn(cv))
                ? ((IS_REAL_fn(best)?best.r:(double)best.i) > (IS_REAL_fn(cv)?cv.r:(double)cv.i))
                : (best.i > cv.i);
            if (lt) best = cv;
        }
        *out = best; return 1;
    }
    if (!strcmp(fn,"sqrt") && nargs == 1) {
        DESCR_t av = args[0];
        double v = IS_REAL_fn(av) ? av.r : (double)av.i;
        *out = REALVAL(sqrt(v)); return 1;
    }
    /* IJ-4: trig/exp/log/bitwise — coerce arg to numeric */
#define ICN_TONUM(av) (IS_REAL_fn(av) ? (av).r : IS_INT_fn(av) ? (double)(av).i : ((av).v==DT_S && (av).s ? strtod((av).s,NULL) : 0.0))
#define ICN_MATH1(fname, cfn) \
    if (!strcmp(fn, fname) && nargs == 1) { double _v = ICN_TONUM(args[0]); *out = REALVAL(cfn(_v)); return 1; }
    ICN_MATH1("sin",  sin)
    ICN_MATH1("cos",  cos)
    ICN_MATH1("tan",  tan)
    ICN_MATH1("asin", asin)
    ICN_MATH1("acos", acos)
    ICN_MATH1("exp",  exp)
#undef ICN_MATH1
    if (!strcmp(fn,"atan") && nargs >= 1) {
        double v = ICN_TONUM(args[0]);
        if (nargs >= 2) { double v2 = ICN_TONUM(args[1]); *out = REALVAL(atan2(v,v2)); }
        else *out = REALVAL(atan(v));
        return 1;
    }
    if (!strcmp(fn,"log") && nargs >= 1) {
        double v = ICN_TONUM(args[0]);
        if (nargs >= 2) { double base = ICN_TONUM(args[1]); *out = REALVAL(log(v)/log(base)); }
        else *out = REALVAL(log(v));
        return 1;
    }
    if (!strcmp(fn,"dtor") && nargs == 1) { double v=ICN_TONUM(args[0]); *out=REALVAL(v*3.14159265358979323846/180.0); return 1; }
    if (!strcmp(fn,"rtod") && nargs == 1) { double v=ICN_TONUM(args[0]); *out=REALVAL(v*180.0/3.14159265358979323846); return 1; }
    if (!strcmp(fn,"iand")  && nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(a&b); return 1; }
    if (!strcmp(fn,"ior")   && nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(a|b); return 1; }
    if (!strcmp(fn,"ixor")  && nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(a^b); return 1; }
    if (!strcmp(fn,"ishift")&& nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(b>=0?a<<b:a>>(-b)); return 1; }
    if (!strcmp(fn,"icom")  && nargs==1) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r; *out=INTVAL(~a); return 1; }
#undef ICN_TONUM
    /* copy(X) — shallow container copy.  For tables, lists: fresh container,
     * same element descriptors.  For strings/ints/reals/sets: value semantics
     * already give a fresh descriptor; return as-is. */
    if (!strcmp(fn,"copy") && nargs == 1) {
        DESCR_t src = args[0];
        if (src.v == DT_T && src.tbl) {
            TBBLK_t *nt = table_new();
            nt->dflt = src.tbl->dflt;
            nt->init = src.tbl->init;
            nt->inc  = src.tbl->inc;
            for (int b = 0; b < TABLE_BUCKETS; b++)
                for (TBPAIR_t *p = src.tbl->buckets[b]; p; p = p->next)
                    table_set_descr(nt, p->key, p->key_descr, p->val);
            DESCR_t d; d.v = DT_T; d.slen = 0; d.tbl = nt;
            *out = d; return 1;
        }
        if (src.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(src, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                DESCR_t ea = FIELD_GET_fn(src, "frame_elems");
                int n = (int)FIELD_GET_fn(src, "frame_size").i;
                DESCR_t *src_elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                DESCR_t *new_elems = (DESCR_t *)GC_malloc((size_t)(n > 0 ? n : 1) * sizeof(DESCR_t));
                if (src_elems && n > 0) memcpy(new_elems, src_elems, (size_t)n * sizeof(DESCR_t));
                DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void *)new_elems;
                *out = DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
                return 1;
            }
        }
        *out = src; return 1;
    }
    /* list([n], [init]) — Icon list constructor.  n elements, all init. */
    if (!strcmp(fn,"list") && nargs >= 0 && nargs <= 2) {
        int n = 0;
        DESCR_t init = NULVCL;
        if (nargs >= 1) {
            DESCR_t nv = args[0];
            if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) {
                if (IS_INT_fn(nv)) n = (int)nv.i;
                else if (IS_REAL_fn(nv)) n = (int)nv.r;
                else { *out = FAILDESCR; return 1; }
                if (n < 0) { *out = FAILDESCR; return 1; }
            }
        }
        if (nargs >= 2) {
            DESCR_t iv = args[1];
            if (!IS_FAIL_fn(iv)) init = iv;
        }
        static int icnlist_reg2 = 0;
        if (!icnlist_reg2) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_reg2 = 1; }
        DESCR_t *elems = GC_malloc((n>0?n:1)*sizeof(DESCR_t));
        for (int i = 0; i < n; i++) elems[i] = init;
        DESCR_t eptr; eptr.v=DT_DATA; eptr.slen=0; eptr.ptr=(void*)elems;
        *out = DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
        return 1;
    }
    /* table([dflt]) — Icon table constructor.  Optional default value. */
    if (!strcmp(fn,"table") && nargs <= 2) {
        TBBLK_t *tbl = table_new();
        if (nargs == 1) {
            tbl->dflt = args[0];
        } else {
            tbl->dflt = NULVCL;
        }
        DESCR_t d; d.v = DT_T; d.slen = 0; d.tbl = tbl;
        *out = d; return 1;
    }
    /* read() — read one line from stdin (no newline), fail on EOF. */
    if (!strcmp(fn,"read") && nargs == 0) {
        char buf[4096];
        if (!fgets(buf, sizeof buf, stdin)) { *out = FAILDESCR; return 1; }
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
        char *r = GC_malloc(len + 1); memcpy(r, buf, len + 1);
        *out = STRVAL(r); return 1;
    }
    /* reads(n) — read n bytes from stdin, fail on EOF. */
    if (!strcmp(fn,"reads") && nargs == 1) {
        DESCR_t nd = args[0];
        int n = (int)to_int(nd);
        if (n <= 0) { *out = FAILDESCR; return 1; }
        char *buf = GC_malloc(n + 1);
        int got = (int)fread(buf, 1, (size_t)n, stdin);
        if (got <= 0) { *out = FAILDESCR; return 1; }
        buf[got] = '\0';
        DESCR_t r; r.v = DT_S; r.slen = (uint32_t)got; r.s = buf;
        *out = r; return 1;
    }
    /* stop() — terminate process with exit(0).  Matches the legacy in-eval
     * behavior verbatim (note: not Icon-spec-conformant which would write
     * args to stderr; that fidelity gap is pre-existing and tracked
     * separately). */
    if (!strcmp(fn,"stop")) { exit(0); }

    /* CH-17g-scan: Icon string-scanning context helpers.
     * ICN_SCAN_PUSH(subj): save &subject/&pos, set scan_subj=subj, scan_pos=1.
     * ICN_SCAN_POP(body_result): restore scan_subj/scan_pos, pass body_result through. */
    extern const char *scan_subj;
    extern int         scan_pos;
    extern int         scan_depth;
    extern ScanEntry   scan_stack[];
    if (!strcmp(fn,"ICN_SCAN_PUSH") && nargs == 1) {
        const char *s = VARVAL_fn(args[0]); if (!s) s = "";
        if (scan_depth < SCAN_STACK_MAX) {
            scan_stack[scan_depth].subj = scan_subj;
            scan_stack[scan_depth].pos  = scan_pos;
            scan_depth++;
        }
        scan_subj = GC_strdup(s); scan_pos = 1;
        *out = args[0]; return 1;
    }
    if (!strcmp(fn,"ICN_SCAN_POP") && nargs == 1) {
        if (scan_depth > 0) {
            scan_depth--;
            scan_subj = scan_stack[scan_depth].subj;
            scan_pos  = scan_stack[scan_depth].pos;
        }
        *out = args[0]; return 1;
    }

    /* Icon string-scanning builtins — operate on scan_subj / scan_pos globals.
     * All require scan_pos > 0 (i.e. active scanning context). */
    if (!strcmp(fn,"any") && nargs >= 1 && scan_pos > 0) {
        const char *cv = VARVAL_fn(args[0]); if (!cv) { *out = FAILDESCR; return 1; }
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj), p0 = scan_pos - 1;
        if (p0 < 0 || p0 >= slen || !strchr(cv, scan_subj[p0])) { *out = FAILDESCR; return 1; }
        scan_pos++; *out = INTVAL(scan_pos); return 1;
    }
    if (!strcmp(fn,"many") && nargs >= 1 && scan_pos > 0) {
        const char *cv = VARVAL_fn(args[0]); if (!cv) { *out = FAILDESCR; return 1; }
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj), p0 = scan_pos - 1;
        if (p0 < 0 || p0 >= slen || !strchr(cv, scan_subj[p0])) { *out = FAILDESCR; return 1; }
        while (p0 < slen && strchr(cv, scan_subj[p0])) p0++;
        scan_pos = p0 + 1; *out = INTVAL(scan_pos); return 1;
    }
    if (!strcmp(fn,"upto") && nargs >= 1 && scan_pos > 0) {
        const char *cv = VARVAL_fn(args[0]); if (!cv) { *out = FAILDESCR; return 1; }
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj), p0 = scan_pos - 1;
        while (p0 < slen && !strchr(cv, scan_subj[p0])) p0++;
        if (p0 >= slen) { *out = FAILDESCR; return 1; }
        scan_pos = p0 + 1; *out = INTVAL(scan_pos); return 1;
    }
    if (!strcmp(fn,"tab") && nargs == 1 && scan_pos > 0) {
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj);
        int target = (int)to_int(args[0]);
        if (target <= 0) target = slen + 1 + target;   /* negative: from right */
        if (target < 1 || target > slen + 1) { *out = FAILDESCR; return 1; }
        int old = scan_pos; scan_pos = target;
        int lo = old < target ? old : target, hi = old < target ? target : old;
        int len = hi - lo;
        char *buf = GC_malloc(len + 1);
        memcpy(buf, scan_subj + lo - 1, len); buf[len] = '\0';
        *out = STRVAL(buf); return 1;
    }
    if (!strcmp(fn,"move") && nargs == 1 && scan_pos > 0) {
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj);
        int n = (int)to_int(args[0]);
        int target = scan_pos + n;
        if (target < 1 || target > slen + 1) { *out = FAILDESCR; return 1; }
        int old = scan_pos; scan_pos = target;
        int lo = old < target ? old : target, hi = old < target ? target : old;
        int len = hi - lo;
        char *buf = GC_malloc(len + 1);
        memcpy(buf, scan_subj + lo - 1, len); buf[len] = '\0';
        *out = STRVAL(buf); return 1;
    }
    if (!strcmp(fn,"match") && nargs >= 1 && scan_pos > 0) {
        const char *pat = VARVAL_fn(args[0]); if (!pat) { *out = FAILDESCR; return 1; }
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int plen = (int)strlen(pat), p0 = scan_pos - 1;
        int slen = (int)strlen(scan_subj);
        if (p0 + plen > slen || strncmp(scan_subj + p0, pat, plen) != 0) { *out = FAILDESCR; return 1; }
        scan_pos += plen; *out = INTVAL(scan_pos); return 1;
    }
    if (!strcmp(fn,"find") && nargs >= 2 && scan_pos > 0) {
        const char *needle = VARVAL_fn(args[0]); if (!needle) { *out = FAILDESCR; return 1; }
        const char *hay    = VARVAL_fn(args[1]); if (!hay) hay = scan_subj ? scan_subj : "";
        int nlen = (int)strlen(needle), hlen = (int)strlen(hay);
        int start = scan_pos - 1;
        for (int i = start; i + nlen <= hlen; i++) {
            if (strncmp(hay + i, needle, nlen) == 0) { *out = INTVAL(i + 1); return 1; }
        }
        *out = FAILDESCR; return 1;
    }

    /* CH-17g-builtin-batch (2026-05-11): SM-mode Icon builtins missing from
     * icn_try_call_builtin_by_name.  All are verbatim ports of the AST-walk
     * paths in interp_eval.c / coro_value.c with interp_eval(e->c[i])
     * replaced by args[i] (pre-evaluated by SM_CALL_FN). No tree_t access. */

    /* SIZE (*E) — string/list/table size.  Mirrors coro_value.c:TT_SIZE. */
    if (!strcmp(fn,"SIZE") && nargs == 1) {
        DESCR_t v = args[0];
        if (IS_FAIL_fn(v)) { *out = FAILDESCR; return 1; }
        if (v.v == DT_T)   { *out = INTVAL(v.tbl ? v.tbl->size : 0); return 1; }
        if (v.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(v,"icn_type");
            if (tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0) {
                *out = INTVAL((int)FIELD_GET_fn(v,"frame_size").i); return 1;
            }
        }
        if (IS_INT_fn(v)||IS_REAL_fn(v)) { *out = INTVAL(0); return 1; }
        const char *s = VARVAL_fn(v); if (!s) { *out = INTVAL(0); return 1; }
        if (strchr(s,'\x01')) {
            long n=1; for(const char *p=s;*p;p++) if(*p=='\x01') n++;
            *out = INTVAL(n); return 1;
        }
        long len = v.slen > 0 ? v.slen : (long)strlen(s);
        *out = INTVAL(len); return 1;
    }

    /* NONNULL (\E) — succeed with E if E != null, else fail.
     * Mirrors coro_value.c:TT_NONNULL. */
    if (!strcmp(fn,"NONNULL") && nargs == 1) {
        DESCR_t v = args[0];
        if (IS_FAIL_fn(v))  { *out = FAILDESCR; return 1; }
        if (v.v == DT_SNUL) { *out = FAILDESCR; return 1; }
        if (v.v == DT_S && (!v.s || v.s[0]=='\0')) { *out = FAILDESCR; return 1; }
        *out = v; return 1;
    }

    /* ICN_CASE_EQ(topic, val) — type-aware equality for case expressions.
     * SM pop order: args[1]=TOS=val, args[0]=topic (topic pushed first).
     * Numeric if both numeric; string otherwise. Returns val on match, FAILDESCR on miss. */
    if (!strcmp(fn,"ICN_CASE_EQ") && nargs == 2) {
        DESCR_t topic = args[0], val = args[1];
        if (IS_FAIL_fn(topic) || IS_FAIL_fn(val)) { *out = FAILDESCR; return 1; }
        int eq = 0;
        if ((IS_INT_fn(topic) || IS_REAL_fn(topic)) &&
            (IS_INT_fn(val)   || IS_REAL_fn(val))) {
            double tv = IS_REAL_fn(topic) ? topic.r : (double)topic.i;
            double vv = IS_REAL_fn(val)   ? val.r   : (double)val.i;
            eq = (tv == vv);
        } else {
            const char *ts = VARVAL_fn(topic); if (!ts) ts = "";
            const char *vs = VARVAL_fn(val);   if (!vs) vs = "";
            eq = (strcmp(ts, vs) == 0);
        }
        *out = eq ? val : FAILDESCR; return 1;
    }

    /* ICN_SWAP_TOP2(a, b) — swap top two stack items; returns a (the lower one).
     * Used by TT_SWAP lowering to cross-store two variable values. */
    if (!strcmp(fn,"ICN_SWAP_TOP2") && nargs == 2) {
        /* args[0]=TOS (rhs_val), args[1]=below (lhs_val) after SM pop order.
         * Push lhs_val first (it goes below), then rhs_val on top.
         * But SM_CALL_FN replaces both with *out — only one value returned.
         * We need to push TWO values back. That's not possible via SM_CALL_FN.
         * Use a different approach: return lhs_val; the STORE_FRAME/VAR following
         * will store it; rhs_val is already stored by the prior STORE_FRAME/VAR. */
        /* SM pop order: args[1]=TOS=rhs_val pushed second, args[0]=lhs_val pushed first.
         * Wait — SM_CALL_FN loop: for(k=nargs-1; k>=0; k--) args[k]=sm_pop().
         * So args[1]=sm_pop()=TOS=rhs_val, args[0]=sm_pop()=lhs_val. */
        *out = args[0];  /* return lhs_val; caller stores it to rhs */
        return 1;
    }

    /* ICN_NULL (/E) — succeed with &null iff E is null, else fail.
     * Mirrors coro_value.c:TT_NULL. */
    if (!strcmp(fn,"ICN_NULL") && nargs == 1) {
        DESCR_t v = args[0];
        if (IS_FAIL_fn(v))  { *out = FAILDESCR; return 1; }
        if (v.v == DT_SNUL) { *out = NULVCL; return 1; }
        if (v.v == DT_S && (!v.s || v.s[0]=='\0')) { *out = NULVCL; return 1; }
        *out = FAILDESCR; return 1;
    }

    /* insert(T, k [, v]) — add key k with value v (or &null) to table T.
     * Mirrors interp_eval.c:1643. */
    if (!strcmp(fn,"insert") && nargs >= 2) {
        DESCR_t td = args[0];
        if (td.v != DT_T) { *out = FAILDESCR; return 1; }
        DESCR_t kd = args[1];
        DESCR_t vd = (nargs >= 3) ? args[2] : NULVCL;
        char kb[64]; const char *ks;
        if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
        else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
        else                     { ks=VARVAL_fn(kd); if(!ks) ks=""; }
        table_set_descr(td.tbl, ks, kd, vd);
        *out = td; return 1;
    }

    /* delete(T [, k]) — remove key k from table T.  Mirrors interp_eval.c:1655. */
    if (!strcmp(fn,"delete") && nargs >= 1) {
        DESCR_t td = args[0];
        if (td.v != DT_T) { *out = FAILDESCR; return 1; }
        DESCR_t kd = (nargs >= 2) ? args[1] : NULVCL;
        char kb[64]; const char *ks;
        if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
        else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
        else                     { ks=VARVAL_fn(kd); if(!ks) ks=""; }
        unsigned h=0x1505;
        { const char *p=ks; while(*p){h=(h<<5)+h^(unsigned char)*p++;} h&=0xFF; }
        TBPAIR_t **pp=&td.tbl->buckets[h];
        while(*pp) {
            if(strcmp((*pp)->key,ks)==0){TBPAIR_t *del=*pp;*pp=del->next;td.tbl->size--;break;}
            pp=&(*pp)->next;
        }
        *out = td; return 1;
    }

    /* member(T [, k]) — succeed with value if k is a key, else fail.
     * Mirrors interp_eval.c:1679. */
    if (!strcmp(fn,"member") && nargs >= 1) {
        DESCR_t td = args[0];
        if (td.v != DT_T) { *out = FAILDESCR; return 1; }
        DESCR_t kd = (nargs >= 2) ? args[1] : NULVCL;
        char kb[64]; const char *ks;
        if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
        else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
        else                     { ks=VARVAL_fn(kd); if(!ks) ks=""; }
        if (!table_has(td.tbl,ks)) { *out=FAILDESCR; return 1; }
        *out = table_get(td.tbl,ks); return 1;
    }

    /* key(T) — oneshot: return first key of table (non-generator; every uses bb).
     * Mirrors interp_eval.c:1697. */
    if (!strcmp(fn,"key") && nargs == 1) {
        DESCR_t td = args[0];
        if (td.v != DT_T || !td.tbl) { *out=FAILDESCR; return 1; }
        for (int _bi=0;_bi<TABLE_BUCKETS;_bi++)
            if (td.tbl->buckets[_bi]) {
                *out = td.tbl->buckets[_bi]->key_descr; return 1;
            }
        *out = FAILDESCR; return 1;
    }

    /* push(L, v) — prepend v to Icon list L.  Mirrors interp_eval.c:1829.
     * Only fires for DT_DATA icnlist; Raku's SOH-string push goes to INVOKE_fn. */
    if (!strcmp(fn,"push") && nargs == 2) {
        DESCR_t ld = args[0]; DESCR_t vd = args[1];
        if (ld.v != DT_DATA) return 0;  /* not an icnlist — let INVOKE_fn handle */
        DESCR_t tag = FIELD_GET_fn(ld,"icn_type");
        if (!(tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)) return 0;
        int n=(int)FIELD_GET_fn(ld,"frame_size").i;
        DESCR_t ea=FIELD_GET_fn(ld,"frame_elems");
        DESCR_t *old=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
        DESCR_t *nb=GC_malloc((n+1)*sizeof(DESCR_t));
        nb[0]=vd;
        if(old&&n>0) memcpy(nb+1,old,n*sizeof(DESCR_t));
        FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=nb});
        FIELD_SET_fn(ld,"frame_size",INTVAL(n+1));
        *out = ld; return 1;
    }

    /* put(L, v) — append v to Icon list L.  Mirrors interp_eval.c:1843. */
    if (!strcmp(fn,"put") && nargs == 2) {
        DESCR_t ld = args[0]; DESCR_t vd = args[1];
        if (ld.v != DT_DATA) return 0;
        DESCR_t tag = FIELD_GET_fn(ld,"icn_type");
        if (!(tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)) return 0;
        int n=(int)FIELD_GET_fn(ld,"frame_size").i;
        DESCR_t ea=FIELD_GET_fn(ld,"frame_elems");
        DESCR_t *old=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
        DESCR_t *nb=GC_malloc((n+1)*sizeof(DESCR_t));
        if(old&&n>0) memcpy(nb,old,n*sizeof(DESCR_t));
        nb[n]=vd;
        FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=nb});
        FIELD_SET_fn(ld,"frame_size",INTVAL(n+1));
        *out = ld; return 1;
    }

    /* get(L) — remove and return first element.  Mirrors interp_eval.c:1857. */
    if (!strcmp(fn,"get") && nargs == 1) {
        DESCR_t ld = args[0];
        if (ld.v != DT_DATA) return 0;
        DESCR_t tag = FIELD_GET_fn(ld,"icn_type");
        if (!(tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)) return 0;
        DESCR_t ea=FIELD_GET_fn(ld,"frame_elems");
        int n=(int)FIELD_GET_fn(ld,"frame_size").i;
        DESCR_t *arr=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
        if(!arr||n<=0) { *out=FAILDESCR; return 1; }
        DESCR_t ret=arr[0];
        FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=arr+1});
        FIELD_SET_fn(ld,"frame_size",INTVAL(n-1));
        *out = ret; return 1;
    }

    /* pull(L) — remove and return last element.  Mirrors interp_eval.c:1874. */
    if (!strcmp(fn,"pull") && nargs == 1) {
        DESCR_t ld = args[0];
        if (ld.v != DT_DATA) return 0;
        DESCR_t tag = FIELD_GET_fn(ld,"icn_type");
        if (!(tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)) return 0;
        DESCR_t ea=FIELD_GET_fn(ld,"frame_elems");
        int n=(int)FIELD_GET_fn(ld,"frame_size").i;
        DESCR_t *arr=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
        if(!arr||n<=0) { *out=FAILDESCR; return 1; }
        DESCR_t ret=arr[n-1];
        FIELD_SET_fn(ld,"frame_size",INTVAL(n-1));
        *out = ret; return 1;
    }

    /* sort(L) / sortf(L, i) — sort Icon list, optionally by record field i.
     * Mirrors interp_eval.c:2220.  Insertion sort — correct for all sizes. */
    if ((!strcmp(fn,"sort")&&nargs==1)||(!strcmp(fn,"sortf")&&nargs==2)) {
        DESCR_t ld = args[0];
        if (ld.v != DT_DATA) return 0;
        DESCR_t tag = FIELD_GET_fn(ld,"icn_type");
        if (!(tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)) return 0;
        DESCR_t ea=FIELD_GET_fn(ld,"frame_elems");
        int n=(int)FIELD_GET_fn(ld,"frame_size").i;
        if (n<=0) { *out=ld; return 1; }
        DESCR_t *arr=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
        if(!arr) { *out=ld; return 1; }
        DESCR_t *sorted=GC_malloc(n*sizeof(DESCR_t));
        memcpy(sorted,arr,n*sizeof(DESCR_t));
        int field_idx=(!strcmp(fn,"sortf")&&nargs==2)?(int)to_int(args[1])-1:-1;
        for(int _i=1;_i<n;_i++){
            DESCR_t key=sorted[_i]; int _j=_i-1;
            while(_j>=0){
                DESCR_t a=sorted[_j],b=key;
                if(field_idx>=0){
                    if(a.v==DT_DATA&&a.u){DATINST_t*_ia=(DATINST_t*)a.u;if(_ia->type&&field_idx<_ia->type->nfields)a=_ia->fields[field_idx];}
                    if(b.v==DT_DATA&&b.u){DATINST_t*_ib=(DATINST_t*)b.u;if(_ib->type&&field_idx<_ib->type->nfields)b=_ib->fields[field_idx];}
                }
                int cmp;
                if(IS_INT_fn(a)&&IS_INT_fn(b)) cmp=(a.i>b.i)?1:(a.i<b.i)?-1:0;
                else{const char*sa=VARVAL_fn(a),*sb=VARVAL_fn(b);cmp=strcmp(sa?sa:"",sb?sb:"");}
                if(cmp<=0) break;
                sorted[_j+1]=sorted[_j];_j--;
            }
            sorted[_j+1]=key;
        }
        DESCR_t res=ld;
        FIELD_SET_fn(res,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=sorted});
        FIELD_SET_fn(res,"frame_size",INTVAL(n));
        *out=res; return 1;
    }

    /* FIELD_GET(record, fieldname) — Icon record field read.
     * sm_lower emits: [lower obj] + SM_PUSH_LIT_S(field) + SM_CALL_FN("FIELD_GET",2).
     * Mirrors FIELD_GET_fn macro / sc_dat_field_get. */
    if (!strcmp(fn,"FIELD_GET") && nargs == 2) {
        DESCR_t obj  = args[0];
        DESCR_t fname_d = args[1];
        const char *fname = VARVAL_fn(fname_d);
        if (!fname || obj.v != DT_DATA) { *out=FAILDESCR; return 1; }
        extern DESCR_t sc_dat_field_get(const char *field, DESCR_t obj);
        *out = sc_dat_field_get(fname, obj); return 1;
    }

    /* FIELD_SET(rhs, record, fieldname) — Icon record field write.
     * Stack at call (TOS first): fieldname, record, rhs → popped into args[2],args[1],args[0].
     * So: args[0]=rhs, args[1]=record, args[2]=fieldname. */
    if (!strcmp(fn,"FIELD_SET") && nargs == 3) {
        DESCR_t val    = args[0];
        DESCR_t obj    = args[1];
        DESCR_t fname_d = args[2];
        const char *fname = VARVAL_fn(fname_d);
        if (!fname || obj.v != DT_DATA) { *out=FAILDESCR; return 1; }
        extern DESCR_t *data_field_ptr(const char *field, DESCR_t obj);
        DESCR_t *cell = data_field_ptr(fname, obj);
        if (cell) { *cell = val; *out = val; return 1; }
        *out = FAILDESCR; return 1;
    }

    /* MAKELIST(e0, e1, ...) — Icon list literal [e0, e1, ...].
     * sm_lower emits each element then SM_CALL_FN("MAKELIST", n).
     * Mirrors interp_eval.c:TT_MAKELIST. */
    if (!strcmp(fn,"MAKELIST")) {
        static int icnlist_reg3 = 0;
        if (!icnlist_reg3) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_reg3 = 1; }
        DESCR_t *elems = GC_malloc((nargs>0?nargs:1)*sizeof(DESCR_t));
        for (int _j=0;_j<nargs;_j++) elems[_j]=args[_j];
        DESCR_t eptr; eptr.v=DT_DATA; eptr.slen=0; eptr.ptr=(void*)elems;
        *out = DATCON_fn("icnlist", eptr, INTVAL(nargs), STRVAL("list")); return 1;
    }

    /* RECORD_MAKE(name, field0, field1, ...) — Icon record constructor.
     * sm_lower emits SM_PUSH_LIT_S(typename) + args + SM_CALL_FN("RECORD_MAKE", nfields+1).
     * Mirrors the sc_dat_construct path in interp_eval.c:2260. */
    if (!strcmp(fn,"RECORD_MAKE") && nargs >= 1) {
        const char *rname = VARVAL_fn(args[0]);
        if (!rname || !*rname) { *out=FAILDESCR; return 1; }
        ScDatType *_dt = sc_dat_find_type(rname);
        if (!_dt) { *out=FAILDESCR; return 1; }
        DESCR_t fargs[FRAME_SLOT_MAX];
        int nf = nargs - 1;
        for (int _j=0;_j<nf&&_j<FRAME_SLOT_MAX;_j++) fargs[_j]=args[1+_j];
        *out = sc_dat_construct(_dt, fargs, nf); return 1;
    }

    /* open(filename[, mode]) — open a file, return INTVAL(fh_idx) as file descriptor.
     * Reuses raku_fh_alloc to store FILE* by integer index.
     * Icon modes: "r"=read (default), "w"=write, "a"=append. */
    if (!strcmp(fn,"open") && (nargs == 1 || nargs == 2)) {
        const char *path = (args[0].v == DT_S || args[0].v == DT_SNUL) ? args[0].s : NULL;
        if (!path) { *out = FAILDESCR; return 1; }
        const char *mode = (nargs == 2 && (args[1].v == DT_S||args[1].v == DT_SNUL) && args[1].s)
                           ? args[1].s : "r";
        const char *cmode = "r";
        if (strstr(mode,"w")) cmode = "w";
        else if (strstr(mode,"a")) cmode = "a";
        FILE *fp = fopen(path, cmode);
        if (!fp) { *out = FAILDESCR; return 1; }
        int idx = raku_fh_alloc(fp);
        if (idx < 0) { fclose(fp); *out = FAILDESCR; return 1; }
        if (idx >= 0 && idx < RAKU_FH_MAX) raku_fh_name[idx] = GC_strdup(path);  /* IJ-3 */
        *out = INTVAL(idx); return 1;
    }

    /* close(fh) — close a file handle. */
    if (!strcmp(fn,"close") && nargs == 1) {
        if (IS_INT_fn(args[0])) {
            int idx = (int)args[0].i;
            FILE *fp = raku_fh_get(idx);
            if (fp) { fclose(fp); raku_fh_free(idx); }
        }
        *out = args[0]; return 1;
    }

    /* read(fh) — read one line from file handle. */
    if (!strcmp(fn,"read") && nargs == 1) {
        FILE *fp = IS_INT_fn(args[0]) ? raku_fh_get((int)args[0].i) : NULL;
        if (!fp) { *out = FAILDESCR; return 1; }
        char buf[4096];
        if (!fgets(buf, sizeof buf, fp)) { *out = FAILDESCR; return 1; }
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
        *out = STRVAL(GC_strdup(buf)); return 1;
    }

    /* reads(fh, n) — read n bytes from file handle. */
    if (!strcmp(fn,"reads") && nargs == 2) {
        FILE *fp = IS_INT_fn(args[0]) ? raku_fh_get((int)args[0].i) : NULL;
        if (!fp) { *out = FAILDESCR; return 1; }
        int n = (int)to_int(args[1]);
        if (n <= 0) { *out = FAILDESCR; return 1; }
        char *buf = GC_malloc(n + 1);
        int got = (int)fread(buf, 1, (size_t)n, fp);
        if (got <= 0) { *out = FAILDESCR; return 1; }
        buf[got] = '\0';
        DESCR_t r; r.v = DT_S; r.slen = (uint32_t)got; r.s = buf;
        *out = r; return 1;
    }

    /* IDENTICAL(a,b) — Icon === operator as function (a === b). */
    if (!strcmp(fn,"IDENTICAL") && nargs == 2) {
        DESCR_t a = args[0], b = args[1];
        int same = (a.v == b.v);
        if (same) {
            if      (a.v == DT_I)               same = (a.i == b.i);
            else if (a.v == DT_R)               same = (a.r == b.r);
            else if (a.v == DT_S || a.v == DT_SNUL)
                same = (a.s == b.s || (a.s && b.s && strcmp(a.s,b.s)==0));
            else                                same = (a.ptr == b.ptr);
        }
        *out = same ? b : FAILDESCR; return 1;
    }

    /* set([list]) — create a set (table with keys=members, vals=1).
     * Icon: set() creates empty set; set(L) creates from list. */
    if (!strcmp(fn,"set") && nargs <= 1) {
        TBBLK_t *tbl = table_new();
        if (nargs == 1 && args[0].v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(args[0], "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s,"list")==0) {
                DESCR_t ea = FIELD_GET_fn(args[0], "frame_elems");
                int n = (int)FIELD_GET_fn(args[0], "frame_size").i;
                DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                if (elems) for (int _i = 0; _i < n; _i++)
                    table_set_descr(tbl, NULL, elems[_i], INTVAL(1));
            }
        }
        *out = TABLE_VAL(tbl); return 1;
    }

    /* ASGN(rhs, target) — assignment as builtin (called from emit_lhs_store
     * for non-standard LHS forms).  Propagates FAILDESCR. */
    if (!strcmp(fn,"ASGN") && nargs == 2) {
        DESCR_t rhs = args[0];
        if (IS_FAIL_fn(rhs)) { *out = FAILDESCR; return 1; }
        DESCR_t lref = args[1];
        if (lref.v == DT_S && lref.s) NV_SET_fn(lref.s, rhs);
        *out = rhs; return 1;
    }

    /* variable(name) — return current value of named variable. */
    if (!strcmp(fn,"variable") && nargs == 1) {
        const char *vname = (args[0].v == DT_S || args[0].v == DT_SNUL) ? args[0].s : NULL;
        if (!vname) { *out = FAILDESCR; return 1; }
        DESCR_t v = NV_GET_fn(vname);
        *out = IS_FAIL_fn(v) ? FAILDESCR : v; return 1;
    }

    return 0;
}

/* icn_call_builtin — call a builtin TT_FNC with pre-resolved args array.
 * Used by coro_bb_fnc to avoid re-evaluating generator children.
 * Dispatches write/writes/upto/find/any/many/upto/tab/move/match by name.
 * For user procs, calls coro_call directly. */
DESCR_t icn_call_builtin(tree_t *call, DESCR_t *args, int nargs) {
    if (!call || call->n < 1 || !call->c[0]) return NULVCL;
    const char *fn = call->c[0]->v.sval;
    if (!fn) return NULVCL;
    /* RS-23a-raku: Raku-builtin dispatch — defensive coverage for the
     * coro_bb_fnc → icn_call_builtin path.  raku_try_call_builtin re-evaluates
     * its own args from call->c[] (it ignores `args`/`nargs`), so this
     * is correct even though args may already be pre-evaluated by caller. */
    {
        DESCR_t __rk_d;
        if (raku_try_call_builtin(call, &__rk_d)) return __rk_d;
    }
    /* RS-23-extra-prep: SCAN-context builtin dispatch.  scan_try_call_builtin
     * uses pre-evaluated args[] (matching the icn_call_builtin convention),
     * so unlike Raku it must come AFTER the caller has populated args.  Both
     * BB-adapter and interp_eval-resident callers pre-evaluate, so this is
     * safe in every entry path. */
    {
        DESCR_t __sc_d;
        if (scan_try_call_builtin(call, args, nargs, &__sc_d)) return __sc_d;
    }
    /* CH-17g-runtime-bridge-1 (2026-05-09): delegate write/writes (and any
     * future tree_t-free Icon builtins) to icn_try_call_builtin_by_name.
     * Pure refactor; behaviour identical to the inlined branches that lived
     * here previously. */
    {
        DESCR_t __nb_d;
        if (icn_try_call_builtin_by_name(fn, args, nargs, &__nb_d)) return __nb_d;
    }
    DESCR_t a0 = nargs > 0 ? args[0] : NULVCL;
    DESCR_t a1 = nargs > 1 ? args[1] : NULVCL;
    /* User proc — call directly with resolved args (CH-17g-call-sites: via SM expression when entry_pc resolved) */
    for (int i = 0; i < proc_count; i++) {
        if (!strcmp(proc_table[i].name, fn))
            return proc_table_call(i, args, nargs);
    }
    /* RS-23-extra-prep2 (Option B′): smart fallback for the residual
     * builtins still living in interp_eval's TT_FNC switch (~40 names:
     * integer, string, real, char, type, copy, list, table, read, repl,
     * etc.).  The naive fallback `interp_eval(call)` re-walks
     * `call->c[1..]` from scratch, and if any arg has side effects
     * (e.g. `tab(0)` advances scan_pos) it fires twice: once when the
     * BB-adapter caller pre-evaluated into args[], once here.  Meander.icn's
     * `n := integer(tab(0))` is the canonical regression.
     *
     * Fix: synthesize a shallow clone of `call` whose children[1..nargs]
     * are literal leaves (TT_QLIT/TT_ILIT/TT_FLIT/TT_NUL) carrying the
     * already-evaluated descriptors.  When interp_eval recursively
     * evaluates those leaf nodes (interp_eval.c:1850-1853), it does so
     * idempotently — no scan_pos advance, no read() consumption.
     *
     * Mutator-aware denylist: five builtins (`push`, `pop`, `arr_set`,
     * `hash_set`, `hash_delete`) write back through `e->c[1]` via
     * a `FRAME.env[children[1]->v.ival] = ...` assignment, and rely on
     * `children[1]->t == TT_VAR` to identify the lvalue slot.
     * Substituting an TT_QLIT literal there destroys the lvalue identity
     * and silently drops the writeback (Raku rk_arrays / rk_hashes
     * regression in the original Option-B prototype, session 2026-05-05).
     * For these names we keep the original `interp_eval(call)` fallback,
     * which preserves the TT_VAR child.  The Icon-list cluster
     * (push/put/get/pull at line 1180) and Icon-table mutators
     * (insert/delete) operate on heap objects via DT_DATA / DT_T and
     * mutate through the descriptor — not via children[]-writeback —
     * so they are safe to synthesize, but their first arg is non-scalar
     * (DT_DATA / DT_T) and falls through this filter anyway.
     *
     * Scope: handle the four scalar descriptor types (DT_S/DT_SNUL/
     * DT_I/DT_R) that round-trip through the existing literal kinds.
     * For DT_FAIL we propagate without calling.  For other heap types
     * (DT_A/DT_T/DT_P/DT_DATA/DT_K/DT_E/DT_C/DT_N) we keep the original
     * re-eval — those args are heap references and rarely produced by
     * generator children.
     *
     * Residual gap: a mutator on the denylist called with a side-effectful
     * value-arg (e.g. `push(@arr, tab(0))`) will still double-eval that
     * value-arg.  None observed in the corpus.  Lift to Option A for
     * those names if a real bug appears.
     */
    {
        /* Denylist: builtins whose first-arg lvalue identity would be
         * destroyed by literal substitution.  Keep this list in sync
         * with FRAME.env[children[1]->v.ival] writeback sites in this
         * file's TT_FNC switch. */
        int is_mutator =
            !strcmp(fn, "push")        ||
            !strcmp(fn, "pop")         ||
            !strcmp(fn, "arr_set")     ||
            !strcmp(fn, "hash_set")    ||
            !strcmp(fn, "hash_delete");
        int all_scalar = !is_mutator;
        for (int _i = 0; all_scalar && _i < nargs; _i++) {
            if (IS_FAIL_fn(args[_i])) return FAILDESCR;
            DTYPE_t v = args[_i].v;
            if (v != DT_S && v != DT_SNUL && v != DT_I && v != DT_R) {
                all_scalar = 0;
                break;
            }
        }
        if (all_scalar) {
            /* Stack-allocate the shallow clone and the literal-leaf children.
             * `interp_eval`'s TT_FNC case reads call->n and indexes
             * children[0]..c[nargs] (children[0] is the function-name
             * node, retained as-is).  Nothing in the current builtin
             * implementations stores call->c[i] beyond the call. */
            tree_t   leafbufs[16];               /* covers all observed arities */
            tree_t  *kidsbuf[16 + 1];            /* +1 for the name node */
            tree_t **kids = kidsbuf;
            tree_t  *leaves = leafbufs;
            if (nargs > 16) {
                kids   = (tree_t **)GC_malloc(sizeof(tree_t *) * (size_t)(nargs + 1));
                leaves = (tree_t  *)GC_malloc(sizeof(tree_t  ) * (size_t)nargs);
            }
            kids[0] = call->c[0];        /* keep the function-name node */
            for (int _i = 0; _i < nargs; _i++) {
                tree_t *L = &leaves[_i];
                memset(L, 0, sizeof *L);
                switch (args[_i].v) {
                    case DT_S:
                        L->t = TT_QLIT;
                        L->v.sval = args[_i].s;
                        break;
                    case DT_SNUL:
                        L->t = TT_NUL;
                        break;
                    case DT_I:
                        L->t = TT_ILIT;
                        L->v.ival = args[_i].i;
                        break;
                    case DT_R:
                        L->t = TT_FLIT;
                        L->v.dval = args[_i].r;
                        break;
                    default:
                        /* Unreachable: all_scalar guard above. */
                        break;
                }
                kids[_i + 1] = L;
            }
            tree_t clone;
            memset(&clone, 0, sizeof clone);
            clone.t      = call->t;
            clone.v.sval      = call->v.sval;
            clone.v.ival      = call->v.ival;
            clone.v.dval      = call->v.dval;
            clone._id        = call->_id;
            clone.c  = kids;
            clone.n = nargs + 1;
            clone._nalloc    = nargs + 1;
            return interp_eval(&clone);
        }
    }
    /* Fallback (mutator names + non-scalar args): re-evaluate whole call
     * from the original tree.  Risks double-eval of side-effectful arg
     * expressions, but preserves lvalue identity for the five denylist
     * mutators and round-trips heap-typed args (DT_A/DT_T/DT_P/DT_DATA)
     * that have no scalar literal kind to carry them. */
    return interp_eval(call);
}

/* Forward declaration (also declared above for call_user_function) */

/* IC-9 (2026-05-01): Icon-keyword write helper.
 * Returns 1 on success, 0 on FAIL (out-of-range &pos write).
 * Keywords without a write contract (e.g. &letters, &lcase) are read-only
 * and silently no-op (return 1) — Icon's own behaviour is "no error, no effect".
 *
 * &pos := N semantics:
 *   subj length = L; valid N is in [-L, L+1]; N == 0 → after-last (= L+1);
 *   N < 0 → L+1+N (so -1 → L; -2 → L-1 …).  Out of range → FAIL,
 *   pos unchanged.
 *
 * &subject := s semantics:
 *   set scan subject to string form of s; reset &pos to 1 (Icon spec). */
int kw_assign(const char *kw, DESCR_t val) {
    if (!strcmp(kw, "pos")) {
        long n = to_int(val);
        int slen = scan_subj ? (int)strlen(scan_subj) : 0;
        long norm;
        if (n == 0)      norm = slen + 1;
        else if (n < 0)  norm = slen + 1 + n;
        else             norm = n;
        if (norm < 1 || norm > slen + 1) return 0;
        scan_pos = norm;
        return 1;
    }
    if (!strcmp(kw, "subject")) {
        const char *s = VARVAL_fn(val); if (!s) s = "";
        scan_subj = GC_strdup(s);
        scan_pos = 1;
        return 1;
    }
    /* Other keywords: silently accept (no write contract here) */
    return 1;
}

/* IC-9 (session #26): probe variant of kw_assign — answers "would the
 * write succeed?" without performing it.  Used by atomic TT_SWAP / TT_REVSWAP
 * to detect OOB-on-keyword cases where neither side should be written. */
int icn_kw_can_assign(const char *kw, DESCR_t val) {
    if (!strcmp(kw, "pos")) {
        long n = to_int(val);
        int slen = scan_subj ? (int)strlen(scan_subj) : 0;
        long norm;
        if (n == 0)      norm = slen + 1;
        else if (n < 0)  norm = slen + 1 + n;
        else             norm = n;
        return (norm >= 1 && norm <= slen + 1) ? 1 : 0;
    }
    /* &subject, others: always accept */
    return 1;
}

DESCR_t interp_eval(tree_t *e)
{
    NO_AST_WALK_GUARD("interp_eval");
    if (!e) return NULVCL;
    /* coro_drive_node injection: if this exact node is being driven as a generator
     * (set by TT_EVERY leaf-gen injection or coro_drive_fnc), return the staged value
     * directly without recursing into children.  Covers TT_TO, TT_FNC, and any other
     * node kind that find_leaf_suspendable or coro_drive_fnc selects as the leaf. */
    if (coro_drive_node && e == coro_drive_node) return coro_drive_val;

    /* OE-5: Icon frame dispatch — TT_VAR/TT_ASSIGN/TT_FNC differ between SNO and ICN.
     * All other EKinds fall through to the shared switch (already has Icon cases
     * from OE-3/OE-4). Guard: only active inside an Icon call frame. */
    if (frame_depth > 0) {
        /* RS-24 diag: env-gated per-kind hit counter for the Icon-frame switch.
         * Set RS24_DIAG=1 in the environment to enable.  At process exit,
         * /tmp/rs24_diag_hits.log is written with one line per fired case
         * label.  Used to enumerate which Icon-frame switch cases are actually
         * reachable in mode 1; dead cases can then be deleted. */
        {
            static int rs24_diag_init = 0;
            static int rs24_diag_on = 0;
            static unsigned long rs24_diag_hits[TT_KIND_COUNT];
            if (!rs24_diag_init) {
                rs24_diag_init = 1;
                rs24_diag_on = (getenv("RS24_DIAG") != NULL);
                if (rs24_diag_on) {
                    extern void rs24_diag_dump(void);  /* defined below */
                    atexit(rs24_diag_dump);
                }
            }
            if (rs24_diag_on && (unsigned)e->t < (unsigned)TT_KIND_COUNT) {
                rs24_diag_hits[e->t]++;
            }
            /* Expose pointer for the dumper. */
            extern unsigned long *rs24_diag_hits_ptr;
            rs24_diag_hits_ptr = rs24_diag_hits;
        }
        switch (e->t) {
        case TT_VAR: {
            if (e->v.sval && e->v.sval[0] == '&') {
                const char *kw = e->v.sval + 1;
                if (!strcmp(kw,"subject")) return scan_subj ? STRVAL(scan_subj) : NULVCL;
                if (!strcmp(kw,"pos"))     return INTVAL(scan_pos);
                if (!strcmp(kw,"letters")) return STRVAL("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
                if (!strcmp(kw,"ucase"))   return STRVAL("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
                if (!strcmp(kw,"lcase"))   return STRVAL("abcdefghijklmnopqrstuvwxyz");
                if (!strcmp(kw,"digits"))  return STRVAL("0123456789");
                if (!strcmp(kw,"null"))    return NULVCL;
                if (!strcmp(kw,"fail"))    return FAILDESCR;
                return NULVCL;
            }
            int slot = (int)e->v.ival;
            if (slot >= 0 && slot < FRAME.env_n) return FRAME.env[slot];
            if (slot < 0 && e->v.sval && e->v.sval[0] != '&') return NV_GET_fn(e->v.sval);
            return NULVCL;
        }
        case TT_ASSIGN:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_ASSIGN unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_REVASSIGN:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_REVASSIGN unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_FNC: {
            /* Icon call nodes: sval=NULL, name in children[0]->v.sval */
            if (e->n < 1) return NULVCL;
            /* IJ-3: indirect call — callee is not a bare TT_VAR (e.g. (!plist)()).
             * Evaluate callee to get a proc value, dispatch by type. */
            if (!e->c[0] || e->c[0]->t != TT_VAR) {
                DESCR_t callee = e->c[0] ? interp_eval(e->c[0]) : FAILDESCR;
                if (IS_FAIL_fn(callee)) return FAILDESCR;
                int _na = e->n - 1;
                DESCR_t _args[FRAME_SLOT_MAX];
                for (int _j = 0; _j < _na && _j < FRAME_SLOT_MAX; _j++)
                    _args[_j] = interp_eval(e->c[1+_j]);
                if (callee.v == DT_E) {
                    for (int _i = 0; _i < proc_count; _i++)
                        if (proc_table[_i].entry_pc == (int)callee.i)
                            return proc_table_call(_i, _args, _na);
                    if (callee.slen < (uint32_t)proc_count)
                        return proc_table_call((int)callee.slen, _args, _na);
                    return FAILDESCR;
                }
                if (callee.v == DT_S && callee.s) {
                    DESCR_t _out = FAILDESCR;
                    if (icn_try_call_builtin_by_name(callee.s, _args, _na, &_out)) return _out;
                }
                return FAILDESCR;
            }
            const char *fn = e->c[0]->v.sval;
            if (!fn) return NULVCL;
            int nargs = e->n - 1;
            if (!strcmp(fn,"write")) {
                if (nargs == 0) { printf("\n"); return NULVCL; }
                /* Icon write/writes: evaluate ALL args first; fail (no output) if any fails.
                 * IC-9: pre-fix evaluated and printed incrementally, so writes("x",fail())
                 * would print "x" before failing — wrong.  Now buffers args first. */
                DESCR_t *vals = (DESCR_t *)GC_malloc((size_t)nargs * sizeof(DESCR_t));
                for (int _wi = 0; _wi < nargs; _wi++) {
                    vals[_wi] = interp_eval(e->c[1+_wi]);
                    if (IS_FAIL_fn(vals[_wi])) return FAILDESCR;
                }
                DESCR_t last = NULVCL;
                for (int _wi = 0; _wi < nargs; _wi++) {
                    DESCR_t a = vals[_wi];
                    last = a;
                    if (a.v == DT_SNUL) continue;
                    if (IS_INT_fn(a)) printf("%lld",(long long)a.i);
                    else if (IS_REAL_fn(a)) { char _rb[64]; printf("%s",real_str(a.r,_rb,sizeof _rb)); }
                    else { const char *s=VARVAL_fn(a); if (s) fputs(s, stdout); }
                }
                putchar('\n');
                return last;
            }
            if (!strcmp(fn,"writes")) {
                if (nargs == 0) return NULVCL;
                DESCR_t *vals = (DESCR_t *)GC_malloc((size_t)nargs * sizeof(DESCR_t));
                for (int _wi = 0; _wi < nargs; _wi++) {
                    vals[_wi] = interp_eval(e->c[1+_wi]);
                    if (IS_FAIL_fn(vals[_wi])) return FAILDESCR;
                }
                DESCR_t last = NULVCL;
                for (int _wi = 0; _wi < nargs; _wi++) {
                    DESCR_t a = vals[_wi];
                    last = a;
                    if (a.v == DT_SNUL) continue;
                    if (IS_INT_fn(a)) printf("%lld",(long long)a.i);
                    else if (IS_REAL_fn(a)) { char _rb[64]; printf("%s",real_str(a.r,_rb,sizeof _rb)); }
                    else { const char *s=VARVAL_fn(a); if (s) fputs(s, stdout); }
                }
                return last;
            }
            if (!strcmp(fn,"read") && nargs == 0) {
                /* Icon read() — read one line from stdin, strip trailing newline.
                 * Fails on EOF. */
                char buf[4096];
                if (!fgets(buf, sizeof buf, stdin)) return FAILDESCR;
                size_t len = strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
                if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
                char *r = GC_malloc(len + 1); memcpy(r, buf, len + 1);
                return STRVAL(r);
            }
            if (!strcmp(fn,"reads") && nargs == 1) {
                /* Icon reads(n) — read n bytes from stdin, fail on EOF. */
                DESCR_t nd = interp_eval(e->c[1]);
                int n = (int)to_int(nd);
                if (n <= 0) return FAILDESCR;
                char *buf = GC_malloc(n + 1);
                int got = (int)fread(buf, 1, (size_t)n, stdin);
                if (got <= 0) return FAILDESCR;
                buf[got] = '\0';
                DESCR_t r; r.v = DT_S; r.slen = (uint32_t)got; r.s = buf;
                return r;
            }
            if (!strcmp(fn,"stop"))  { exit(0); }
            if (!strcmp(fn,"any") && nargs>=1 && (scan_pos>0||nargs>=2)) {
                DESCR_t cs=interp_eval(e->c[1]);
                const char *cv=VARVAL_fn(cs);
                if(!cv) return FAILDESCR;
                const char *s; int p, slen, end;
                if(nargs>=2) {
                    DESCR_t sv=interp_eval(e->c[2]); s=VARVAL_fn(sv); if(!s) s="";
                    slen=(int)strlen(s);
                    int i1=(nargs>=3)?(int)interp_eval(e->c[3]).i:scan_pos;
                    int i2=(nargs>=4)?(int)interp_eval(e->c[4]).i:slen+1;
                    if(i1<=0||i1>slen) return FAILDESCR;
                    if(i2<=0) i2=slen+1;
                    p=i1-1; end=i2-1;
                } else { s=scan_subj; if(!s) return FAILDESCR; slen=(int)strlen(s); p=scan_pos-1; end=slen; }
                if(p<0||p>=slen||p>=end||!strchr(cv,s[p])) return FAILDESCR;
                if(nargs<2) { scan_pos++; return INTVAL(scan_pos); }
                return INTVAL(p+2);
            }
            if (!strcmp(fn,"many") && nargs>=1 && (scan_pos>0||nargs>=2)) {
                DESCR_t cs=interp_eval(e->c[1]);
                const char *cv=VARVAL_fn(cs);
                if(!cv) return FAILDESCR;
                const char *s; int p, slen, end;
                if(nargs>=2) {
                    DESCR_t sv=interp_eval(e->c[2]); s=VARVAL_fn(sv); if(!s) s="";
                    slen=(int)strlen(s);
                    int i1=(nargs>=3)?(int)interp_eval(e->c[3]).i:scan_pos;
                    int i2=(nargs>=4)?(int)interp_eval(e->c[4]).i:slen+1;
                    if(i1<=0||i1>slen) return FAILDESCR;
                    if(i2<=0) i2=slen+1;
                    p=i1-1; end=i2-1;
                } else { s=scan_subj; if(!s) return FAILDESCR; slen=(int)strlen(s); p=scan_pos-1; end=slen; }
                if(p<0||p>=slen||p>=end||!strchr(cv,s[p])) return FAILDESCR;
                while(p<end&&p<slen&&strchr(cv,s[p])) p++;
                if(nargs<2) { scan_pos=p+1; return INTVAL(scan_pos); }
                return INTVAL(p+1);
            }
            if (!strcmp(fn,"upto") && nargs>=1 && (scan_pos>0||nargs>=2)) {
                DESCR_t cs=interp_eval(e->c[1]);
                const char *cv=VARVAL_fn(cs);
                if(!cv) return FAILDESCR;
                const char *s; int p, slen, end;
                if(nargs>=2) {
                    DESCR_t sv=interp_eval(e->c[2]); s=VARVAL_fn(sv); if(!s) s="";
                    slen=(int)strlen(s);
                    int i1=(nargs>=3)?(int)interp_eval(e->c[3]).i:scan_pos;
                    int i2=(nargs>=4)?(int)interp_eval(e->c[4]).i:slen+1;
                    if(i1<=0) i1=1; if(i2<=0) i2=slen+1;
                    p=i1-1; end=i2-1;
                } else { s=scan_subj; if(!s) return FAILDESCR; slen=(int)strlen(s); p=scan_pos-1; end=slen; }
                while(p<end&&p<slen&&!strchr(cv,s[p])) p++;
                if(p>=end||p>=slen) return FAILDESCR;
                if(nargs<2) { /* scan context: upto does NOT advance pos, returns current matching pos */
                    return INTVAL(p+1);
                }
                return INTVAL(p+1);
            }
            if (!strcmp(fn,"move") && nargs>=1 && scan_pos>0) {
                DESCR_t nv=interp_eval(e->c[1]); int n=(int)nv.i;
                int newp=scan_pos+n;
                if(!scan_subj||newp<1||newp>(int)strlen(scan_subj)+1) return FAILDESCR;
                int old=scan_pos; scan_pos=newp;
                size_t len=(size_t)(n>=0?n:-n); int start=(n>=0?old:newp);
                char *buf=GC_malloc(len+1); memcpy(buf,scan_subj+start-1,len); buf[len]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"tab") && nargs>=1 && scan_pos>0) {
                DESCR_t nv=interp_eval(e->c[1]); if(IS_FAIL_fn(nv)) return FAILDESCR;
                int slen=scan_subj?(int)strlen(scan_subj):0;
                int newp=(int)nv.i;
                if(newp==0) newp=slen+1;
                else if(newp<0) newp=slen+1+newp;
                if(!scan_subj||newp<scan_pos||newp<1||newp>slen+1) return FAILDESCR;
                int old=scan_pos; scan_pos=newp; size_t len=(size_t)(newp-old);
                char *buf=GC_malloc(len+1); memcpy(buf,scan_subj+old-1,len); buf[len]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"pos") && nargs>=1 && scan_pos>0) {
                DESCR_t nv=interp_eval(e->c[1]); if(IS_FAIL_fn(nv)) return FAILDESCR;
                int slen=scan_subj?(int)strlen(scan_subj):0;
                int p=(int)nv.i;
                if(p==0) p=slen+1;
                else if(p<0) p=slen+1+p;
                if(p<1||p>slen+1) return FAILDESCR;
                return (scan_pos==p) ? INTVAL(scan_pos) : FAILDESCR;
            }
            if (!strcmp(fn,"rpos") && nargs>=1 && scan_pos>0) {
                DESCR_t nv=interp_eval(e->c[1]); if(IS_FAIL_fn(nv)) return FAILDESCR;
                int slen=scan_subj?(int)strlen(scan_subj):0;
                int p=slen+1-(int)nv.i;
                if(p<1||p>slen+1) return FAILDESCR;
                return (scan_pos==p) ? INTVAL(scan_pos) : FAILDESCR;
            }
            if (!strcmp(fn,"match") && nargs>=1 && scan_pos>0) {
                DESCR_t sv=interp_eval(e->c[1]);
                const char *needle=VARVAL_fn(sv),*hay=scan_subj?scan_subj:"";
                if(!needle) return FAILDESCR;
                int p=scan_pos-1,nl=(int)strlen(needle);
                if(strncmp(hay+p,needle,nl)!=0) return FAILDESCR;
                scan_pos+=nl; return INTVAL(scan_pos);
            }
            if (!strcmp(fn,"bal") && nargs>=1) {
                /* bal(c1, c2, c3, s, i1, i2) — find first position where c1-chars appear
                   at nesting depth 0 w.r.t. c2/c3 open/close delimiters.
                   Scalar path; generator path is in coro_eval via coro_bb_bal. */
                DESCR_t cd=interp_eval(e->c[1]);
                const char *c1=VARVAL_fn(cd); if(!c1) return FAILDESCR;
                const char *c2="(", *c3=")";
                if(nargs>=2){DESCR_t t=interp_eval(e->c[2]);const char*v=VARVAL_fn(t);if(v&&v[0])c2=v;}
                if(nargs>=3){DESCR_t t=interp_eval(e->c[3]);const char*v=VARVAL_fn(t);if(v&&v[0])c3=v;}
                const char *s; int slen, p, end;
                if(nargs>=4){
                    DESCR_t sv=interp_eval(e->c[4]);s=VARVAL_fn(sv);if(!s)s="";
                    slen=(int)strlen(s);
                    int i1=(nargs>=5)?(int)interp_eval(e->c[5]).i:1;
                    int i2=(nargs>=6)?(int)interp_eval(e->c[6]).i:slen+1;
                    if(i1<=0)i1=1; if(i2<=0)i2=slen+1;
                    p=i1-1; end=i2-1;
                } else {
                    s=scan_subj; if(!s) return FAILDESCR;
                    slen=(int)strlen(s); p=scan_pos-1; end=slen;
                }
                int depth=0;
                while(p<end&&p<slen){
                    char ch=s[p];
                    if(strchr(c2,ch)) depth++;
                    else if(strchr(c3,ch)&&depth>0) depth--;
                    else if(depth==0&&strchr(c1,ch)) return INTVAL(p+1);
                    p++;
                }
                return FAILDESCR;
            }
            if (!strcmp(fn,"find") && nargs>=2) {
                long pos1; if(icn_frame_lookup(e,&pos1)) return INTVAL(pos1);
                DESCR_t s1=interp_eval(e->c[1]),s2=interp_eval(e->c[2]);
                const char *needle=VARVAL_fn(s1),*hay=VARVAL_fn(s2);
                if(!needle||!hay) return FAILDESCR;
                char *p=strstr(hay,needle);
                return p?INTVAL((long long)(p-hay)+1):FAILDESCR;
            }
            /* coro_drive_fnc passthrough: if this TT_FNC node is currently being
             * driven by coro_drive_fnc, return the suspended value directly
             * instead of re-calling the procedure (which would recurse). */
            if (e == coro_drive_node) return coro_drive_val;
            for (int i=0; i<proc_count; i++) {
                if (!strcmp(proc_table[i].name,fn)) {
                    DESCR_t args[FRAME_SLOT_MAX];
                    for (int j=0; j<nargs&&j<FRAME_SLOT_MAX; j++)
                        args[j]=interp_eval(e->c[1+j]);
                    return proc_table_call(i,args,nargs);   /* CH-17g-call-sites */
                }
            }
            /* RK-14: array builtins — arrays stored as \x01-separated strings */
            if (!strcmp(fn,"push") && nargs == 2) {
                DESCR_t arr = interp_eval(e->c[1]);
                DESCR_t val = interp_eval(e->c[2]);
                char vbuf[64]; const char *vs;
                if (IS_INT_fn(val))       { snprintf(vbuf,sizeof vbuf,"%lld",(long long)val.i); vs=vbuf; }
                else if (IS_REAL_fn(val)) { snprintf(vbuf,sizeof vbuf,"%g",val.r); vs=vbuf; }
                else vs = (val.s && *val.s) ? val.s : "";
                const char *as = (arr.v==DT_S||arr.v==DT_SNUL) ? (arr.s?arr.s:"") : "";
                size_t al=strlen(as), vl=strlen(vs);
                char *buf;
                if (al == 0) { buf=GC_malloc(vl+1); memcpy(buf,vs,vl+1); }
                else { buf=GC_malloc(al+1+vl+1); memcpy(buf,as,al); buf[al]='\x01'; memcpy(buf+al+1,vs,vl+1); }
                if (e->c[1]->t==TT_VAR && e->c[1]->v.ival>=0 &&
                    e->c[1]->v.ival<FRAME.env_n && frame_depth>0)
                    FRAME.env[e->c[1]->v.ival] = STRVAL(buf);
                return STRVAL(buf);
            }
            if (!strcmp(fn,"elems") && nargs == 1) {
                DESCR_t arr = interp_eval(e->c[1]);
                const char *as = (arr.v==DT_S||arr.v==DT_SNUL) ? (arr.s?arr.s:"") : "";
                if (!*as) return INTVAL(0);
                long cnt = 1;
                for (const char *p=as; *p; p++) if (*p=='\x01') cnt++;
                return INTVAL(cnt);
            }
            if (!strcmp(fn,"pop") && nargs == 1) {
                DESCR_t arr = interp_eval(e->c[1]);
                const char *as = (arr.v==DT_S||arr.v==DT_SNUL) ? (arr.s?arr.s:"") : "";
                if (!*as) return FAILDESCR;
                char *buf = GC_malloc(strlen(as)+1); strcpy(buf, as);
                char *last = strrchr(buf, '\x01');
                char *popped;
                if (last) { popped=GC_malloc(strlen(last+1)+1); strcpy(popped,last+1); *last='\0'; }
                else       { popped=GC_malloc(strlen(buf)+1);   strcpy(popped,buf);    buf[0]='\0'; }
                if (e->c[1]->t==TT_VAR && e->c[1]->v.ival>=0 &&
                    e->c[1]->v.ival<FRAME.env_n && frame_depth>0)
                    FRAME.env[e->c[1]->v.ival] = STRVAL(buf);
                return STRVAL(popped);
            }
            if (!strcmp(fn,"arr_get") && nargs == 2) {
                DESCR_t arr = interp_eval(e->c[1]);
                DESCR_t idx = interp_eval(e->c[2]);
                const char *as = (arr.v==DT_S||arr.v==DT_SNUL) ? (arr.s?arr.s:"") : "";
                long i = IS_INT_fn(idx) ? idx.i : 0;
                long cur = 0; const char *seg = as;
                while (cur < i) {
                    const char *nx = strchr(seg, '\x01');
                    if (!nx) return FAILDESCR;
                    seg = nx+1; cur++;
                }
                const char *end = strchr(seg, '\x01');
                size_t len = end ? (size_t)(end-seg) : strlen(seg);
                char *out = GC_malloc(len+1); memcpy(out,seg,len); out[len]='\0';
                return STRVAL(out);
            }
            if (!strcmp(fn,"arr_set") && nargs == 3) {
                DESCR_t arr = interp_eval(e->c[1]);
                DESCR_t idx = interp_eval(e->c[2]);
                DESCR_t val = interp_eval(e->c[3]);
                const char *as = (arr.v==DT_S||arr.v==DT_SNUL) ? (arr.s?arr.s:"") : "";
                long target = IS_INT_fn(idx) ? idx.i : 0;
                char vbuf[64]; const char *vs;
                if (IS_INT_fn(val)) { snprintf(vbuf,sizeof vbuf,"%lld",(long long)val.i); vs=vbuf; }
                else vs = (val.s && *val.s) ? val.s : "";
                char *out = GC_malloc(strlen(as)+strlen(vs)+64);
                out[0]='\0'; long cur=0; const char *seg=as;
                while (*seg || cur <= target) {
                    const char *end2 = strchr(seg, '\x01');
                    size_t slen = end2 ? (size_t)(end2-seg) : strlen(seg);
                    if (out[0]) strcat(out,"\x01");
                    if (cur==target) strcat(out,vs);
                    else             strncat(out,seg,slen);
                    seg = end2 ? end2+1 : seg+slen;
                    cur++;
                    if (!end2 && cur > target) break;
                }
                if (e->c[1]->t==TT_VAR && e->c[1]->v.ival>=0 &&
                    e->c[1]->v.ival<FRAME.env_n && frame_depth>0)
                    FRAME.env[e->c[1]->v.ival] = STRVAL(out);
                return STRVAL(out);
            }

            /* ── RK-15: Hash builtins ───────────────────────────────────────
             * Hashes stored as \x02-separated "key\x03value" pair strings.
             * hash_set(h,k,v): upsert; hash_get(h,k): lookup or NULVCL;
             * hash_exists(h,k): 1/0; hash_keys(h): \x01-sep key list;
             * hash_values(h): \x01-sep value list.                        */
#define HS '\x02'   /* pair separator */
#define HK '\x03'   /* key/value separator within a pair */
            if ((!strcmp(fn,"hash_set") && nargs == 3) ||
                (!strcmp(fn,"hash_get") && nargs == 2) ||
                (!strcmp(fn,"hash_exists") && nargs == 2) ||
                (!strcmp(fn,"hash_delete") && nargs == 2) ||
                (!strcmp(fn,"hash_keys") && nargs == 1) ||
                (!strcmp(fn,"hash_values") && nargs == 1) ||
                (!strcmp(fn,"hash_pairs") && nargs == 1)) {
                DESCR_t hd = interp_eval(e->c[1]);
                const char *hs = (hd.v==DT_S||hd.v==DT_SNUL) ? (hd.s?hd.s:"") : "";
                if (!strcmp(fn,"hash_set")) {
                    DESCR_t kd = interp_eval(e->c[2]);
                    DESCR_t vd = interp_eval(e->c[3]);
                    char kb[64], vb[64];
                    const char *ks = IS_INT_fn(kd)  ? (snprintf(kb,sizeof kb,"%lld",(long long)kd.i),kb)
                                   : IS_REAL_fn(kd) ? (snprintf(kb,sizeof kb,"%g",kd.r),kb)
                                   : (kd.s&&*kd.s?kd.s:"");
                    const char *vs = IS_INT_fn(vd)  ? (snprintf(vb,sizeof vb,"%lld",(long long)vd.i),vb)
                                   : IS_REAL_fn(vd) ? (snprintf(vb,sizeof vb,"%g",vd.r),vb)
                                   : (vd.s&&*vd.s?vd.s:"");
                    size_t kl=strlen(ks);
                    char *out = GC_malloc(strlen(hs)+kl+strlen(vs)+4); out[0]='\0';
                    const char *p = hs;
                    while (*p) {
                        const char *sep = strchr(p, HK); const char *end = strchr(p, HS);
                        if (!sep) break;
                        size_t pkl=(size_t)(sep-p);
                        if (pkl!=kl || memcmp(p,ks,kl)!=0) {
                            if (out[0]) { size_t ol=strlen(out); out[ol]=HS; out[ol+1]='\0'; }
                            size_t plen=end?(size_t)(end-p):strlen(p); strncat(out,p,plen);
                        }
                        if (!end) break; p=end+1;
                    }
                    if (out[0]) { size_t ol=strlen(out); out[ol]=HS; out[ol+1]='\0'; }
                    strcat(out,ks); { size_t ol=strlen(out); out[ol]=HK; out[ol+1]='\0'; } strcat(out,vs);
                    if (e->c[1]->t==TT_VAR && e->c[1]->v.ival>=0 &&
                        e->c[1]->v.ival<FRAME.env_n && frame_depth>0)
                        FRAME.env[e->c[1]->v.ival] = STRVAL(out);
                    return STRVAL(out);
                }
                if (!strcmp(fn,"hash_get") || !strcmp(fn,"hash_exists")) {
                    DESCR_t kd = interp_eval(e->c[2]);
                    char kb[64];
                    const char *ks = IS_INT_fn(kd)  ? (snprintf(kb,sizeof kb,"%lld",(long long)kd.i),kb)
                                   : IS_REAL_fn(kd) ? (snprintf(kb,sizeof kb,"%g",kd.r),kb)
                                   : (kd.s&&*kd.s?kd.s:"");
                    size_t kl=strlen(ks);
                    const char *p=hs;
                    while (*p) {
                        const char *sep=strchr(p,HK); const char *end=strchr(p,HS);
                        if (!sep) break;
                        size_t pkl=(size_t)(sep-p);
                        if (pkl==kl && memcmp(p,ks,kl)==0) {
                            if (!strcmp(fn,"hash_exists")) return INTVAL(1);
                            const char *vs=sep+1;
                            size_t vl=end?(size_t)(end-vs):strlen(vs);
                            char *out=GC_malloc(vl+1); memcpy(out,vs,vl); out[vl]='\0';
                            return STRVAL(out);
                        }
                        if (!end) break; p=end+1;
                    }
                    return !strcmp(fn,"hash_exists") ? INTVAL(0) : NULVCL;
                }
                if (!strcmp(fn,"hash_keys") || !strcmp(fn,"hash_values")) {
                    if (!*hs) { char *e2=GC_malloc(1); e2[0]='\0'; return STRVAL(e2); }
                    char *out=GC_malloc(strlen(hs)+2); out[0]='\0';
                    const char *p=hs;
                    while (*p) {
                        const char *sep=strchr(p,HK); const char *end=strchr(p,HS);
                        if (!sep) break;
                        if (out[0]) { size_t ol=strlen(out); out[ol]='\x01'; out[ol+1]='\0'; }
                        if (!strcmp(fn,"hash_keys")) {
                            strncat(out,p,(size_t)(sep-p));
                        } else {
                            const char *vs=sep+1;
                            size_t vl=end?(size_t)(end-vs):strlen(vs);
                            strncat(out,vs,vl);
                        }
                        if (!end) break; p=end+1;
                    }
                    return STRVAL(out);
                }
                if (!strcmp(fn,"hash_pairs")) {
                    if (!*hs) { char *e2=GC_malloc(1); e2[0]='\0'; return STRVAL(e2); }
                    char *out=GC_malloc(strlen(hs)*2+4); out[0]='\0';
                    const char *p=hs;
                    while (*p) {
                        const char *sep=strchr(p,HK); const char *end=strchr(p,HS);
                        if (!sep) break;
                        if (out[0]) { size_t ol=strlen(out); out[ol]='\x01'; out[ol+1]='\0'; }
                        size_t kl=(size_t)(sep-p);
                        const char *vs=sep+1;
                        size_t vl=end?(size_t)(end-vs):strlen(vs);
                        size_t ol=strlen(out);
                        memcpy(out+ol,p,kl); ol+=kl;
                        out[ol++]=':';
                        memcpy(out+ol,vs,vl); ol+=vl;
                        out[ol]='\0';
                        if (!end) break; p=end+1;
                    }
                    return STRVAL(out);
                }
                if (!strcmp(fn,"hash_delete")) {
                    DESCR_t kd = interp_eval(e->c[2]);
                    char kb[64];
                    const char *ks = IS_INT_fn(kd)  ? (snprintf(kb,sizeof kb,"%lld",(long long)kd.i),kb)
                                   : IS_REAL_fn(kd) ? (snprintf(kb,sizeof kb,"%g",kd.r),kb)
                                   : (kd.s&&*kd.s?kd.s:"");
                    size_t kl=strlen(ks);
                    char *out=GC_malloc(strlen(hs)+2); out[0]='\0';
                    const char *p=hs;
                    while (*p) {
                        const char *sep=strchr(p,HK); const char *end=strchr(p,HS);
                        if (!sep) break;
                        size_t pkl=(size_t)(sep-p);
                        if (pkl!=kl || memcmp(p,ks,kl)!=0) {
                            if (out[0]) { size_t ol=strlen(out); out[ol]=HS; out[ol+1]='\0'; }
                            size_t plen=end?(size_t)(end-p):strlen(p);
                            strncat(out,p,plen);
                        }
                        if (!end) break; p=end+1;
                    }
                    if (e->c[1]->t==TT_VAR && e->c[1]->v.ival>=0 &&
                        e->c[1]->v.ival<FRAME.env_n && frame_depth>0)
                        FRAME.env[e->c[1]->v.ival] = STRVAL(out);
                    return STRVAL(out);
                }
            }
#undef HS
#undef HK

            /* ── RS-23a-raku: Raku-builtin dispatch ─────────────────────────
             * The Raku-specific builtins (raku_substr/index/rindex/uc/lc/trim/
             * chars/length, raku_match/match_global/subst, raku_die/try, raku_map/
             * grep/sort, raku_named_capture/capture, raku_nfa_compile, file-I/O
             * wrappers, raku_new/mcall) used to live here directly.  RS-23a-raku
             * lifted them into src/runtime/interp/raku_builtins.c so bb_eval_value
             * can reach them via icn_call_builtin without falling back to
             * interp_eval.  This dispatch call preserves mode-1 behaviour
             * exactly — the same names dispatch to the same code, just from a
             * single shared site. */
            {
                DESCR_t __rk_d;
                if (raku_try_call_builtin(e, &__rk_d)) return __rk_d;
            }


            /* ── IC-3: Icon table builtins (DT_T native hash table) ────────
             * table()         → new empty table (default value = &null)
             * insert(T,k,v)   → set T[k]=v, return T
             * delete(T,k)     → remove T[k], return T
             * member(T,k)     → return T[k] if present, else fail
             * key(T)          → generator: yields each key (via every)     */
            if (!strcmp(fn,"table") && nargs <= 2) {
                /* Icon: table()      → empty table, default=&null
                 *       table(x)     → empty table, default=x
                 *       table(n,inc) → SNOBOL4 compat (ignored for Icon) */
                TBBLK_t *tbl = table_new();
                if (nargs == 1) {
                    /* Icon table(dflt) — one arg is the default value */
                    tbl->dflt = interp_eval(e->c[1]);
                } else {
                    tbl->dflt = NULVCL;
                }
                DESCR_t d; d.v = DT_T; d.slen = 0; d.tbl = tbl;
                return d;
            }
            if (!strcmp(fn,"insert") && nargs >= 2) {
                DESCR_t td = interp_eval(e->c[1]);
                if (td.v != DT_T) return FAILDESCR;
                DESCR_t kd = interp_eval(e->c[2]);
                DESCR_t vd = (nargs >= 3) ? interp_eval(e->c[3]) : NULVCL;
                char kb[64]; const char *ks;
                if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
                else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
                else                     { ks = VARVAL_fn(kd); if (!ks) ks=""; }
                table_set_descr(td.tbl, ks, kd, vd);
                return td;
            }
            if (!strcmp(fn,"delete") && nargs >= 1) {
                /* IC-9: delete arity fix — Icon delete(T, k, …) takes one key;
                 * extra args are ignored.  delete(T) with no key uses &null
                 * as the key (Icon's null-arg padding semantics).            */
                DESCR_t td = interp_eval(e->c[1]);
                if (td.v != DT_T) return FAILDESCR;
                DESCR_t kd;
                if (nargs >= 2) kd = interp_eval(e->c[2]);
                else            kd = NULVCL;       /* delete(T) → key=&null */
                char kb[64]; const char *ks;
                if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
                else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
                else                     { ks = VARVAL_fn(kd); if (!ks) ks=""; }
                /* walk bucket using same djb2 hash as _tbl_hash in binary:
                 * init=0x1505, hash = hash*33 ^ c, result & 0xFF */
                unsigned h = 0x1505;
                { const char *p=ks; while(*p) { h=(h<<5)+h^(unsigned char)*p++; } h&=0xFF; }
                TBPAIR_t **pp = &td.tbl->buckets[h];
                while (*pp) {
                    if (strcmp((*pp)->key, ks)==0) { TBPAIR_t *del=*pp; *pp=del->next; td.tbl->size--; break; }
                    pp = &(*pp)->next;
                }
                return td;
            }
            if (!strcmp(fn,"member") && nargs >= 1) {
                /* IC-9: Icon member(T, k, …) tests whether k is a key.
                 * 1-arg member(T) uses &null as the key (Icon null-arg padding).
                 * Extra args ignored.                                          */
                DESCR_t td = interp_eval(e->c[1]);
                if (td.v != DT_T) return FAILDESCR;
                DESCR_t kd;
                if (nargs >= 2) kd = interp_eval(e->c[2]);
                else            kd = NULVCL;       /* member(T) → key=&null */
                char kb[64]; const char *ks;
                if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
                else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
                else                     { ks = VARVAL_fn(kd); if (!ks) ks=""; }
                if (!table_has(td.tbl, ks)) return FAILDESCR;
                return table_get(td.tbl, ks);
            }

            /* ── IC-5: key(T) — generator yielding each key of a table ───── */
            if (!strcmp(fn,"key") && nargs == 1) {
                DESCR_t td = interp_eval(e->c[1]);
                if (td.v != DT_T || !td.tbl) return FAILDESCR;
                /* oneshot: return first key; every uses coro_bb_tbl_iterate for keys */
                for (int _bi = 0; _bi < TABLE_BUCKETS; _bi++)
                    if (td.tbl->buckets[_bi])
                        return td.tbl->buckets[_bi]->key_descr;
                return FAILDESCR;
            }

            /* ── IC-5: integer(x), real(x), string(x), numeric(x) ──────────*/
            if (!strcmp(fn,"integer") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_INT_fn(av)) return av;
                if (IS_REAL_fn(av)) return INTVAL((long long)av.r);
                const char *s = VARVAL_fn(av); if (!s) return FAILDESCR;
                /* IC-9 (2026-05-01): Icon radix prefix `BASErDIGITS` —
                 * 2..36, case-insensitive 'r'/'R', digits 0-9 + a-z (10..35). */
                {
                    const char *p = s;
                    while (*p == ' ' || *p == '\t') p++;
                    int neg = 0; if (*p == '+') p++; else if (*p == '-') { neg = 1; p++; }
                    int base = 0; const char *bstart = p;
                    while (*p >= '0' && *p <= '9') { base = base * 10 + (*p - '0'); p++; }
                    if (p > bstart && (*p == 'r' || *p == 'R') && base >= 2 && base <= 36) {
                        p++; const char *dstart = p; long long v = 0;
                        while (*p) {
                            int d = -1;
                            if (*p >= '0' && *p <= '9') d = *p - '0';
                            else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
                            else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
                            if (d < 0 || d >= base) break;
                            v = v * base + d;
                            p++;
                        }
                        while (*p == ' ' || *p == '\t') p++;
                        if (p > dstart && *p == '\0') return INTVAL(neg ? -v : v);
                    }
                }
                char *end; long long iv = strtoll(s, &end, 10);
                if (end != s && (*end=='\0'||*end==' ')) return INTVAL(iv);
                /* try real→int */
                double rv = strtod(s, &end);
                if (end != s && (*end=='\0'||*end==' ')) return INTVAL((long long)rv);
                return FAILDESCR;
            }
            if (!strcmp(fn,"real") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_REAL_fn(av)) return av;
                if (IS_INT_fn(av)) return REALVAL((double)av.i);
                const char *s = VARVAL_fn(av); if (!s) return FAILDESCR;
                char *end; double rv = strtod(s, &end);
                if (end != s && (*end=='\0'||*end==' ')) return REALVAL(rv);
                return FAILDESCR;
            }
            if (!strcmp(fn,"string") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_STR_fn(av)) return av;
                char *buf = GC_malloc(64);
                if (IS_INT_fn(av))       snprintf(buf,64,"%lld",(long long)av.i);
                else if (IS_REAL_fn(av)) { real_str(av.r,buf,64); }
                else return NULVCL;
                return STRVAL(buf);
            }
            if (!strcmp(fn,"numeric") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_INT_fn(av)||IS_REAL_fn(av)) return av;
                const char *s = VARVAL_fn(av); if (!s||!*s) return FAILDESCR;
                /* IC-9: Icon radix prefix `BASErDIGITS` (numeric also accepts radix). */
                {
                    const char *p = s;
                    while (*p == ' ' || *p == '\t') p++;
                    int neg = 0; if (*p == '+') p++; else if (*p == '-') { neg = 1; p++; }
                    int base = 0; const char *bstart = p;
                    while (*p >= '0' && *p <= '9') { base = base * 10 + (*p - '0'); p++; }
                    if (p > bstart && (*p == 'r' || *p == 'R') && base >= 2 && base <= 36) {
                        p++; const char *dstart = p; long long v = 0;
                        while (*p) {
                            int d = -1;
                            if (*p >= '0' && *p <= '9') d = *p - '0';
                            else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
                            else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
                            if (d < 0 || d >= base) break;
                            v = v * base + d;
                            p++;
                        }
                        while (*p == ' ' || *p == '\t') p++;
                        if (p > dstart && *p == '\0') return INTVAL(neg ? -v : v);
                    }
                }
                char *end; long long iv = strtoll(s, &end, 10);
                if (end!=s && (*end=='\0'||*end==' ')) return INTVAL(iv);
                double rv = strtod(s, &end);
                if (end!=s && (*end=='\0'||*end==' ')) return REALVAL(rv);
                return FAILDESCR;
            }

            /* IC-9 (2026-05-01): Icon `list(n, x)` constructor.
             *   list()       — empty list
             *   list(n)      — n elements all &null
             *   list(n, x)   — n elements all = x
             * Omitted/&null `n` defaults to 0.  Negative or non-integer n fails. */
            if (!strcmp(fn,"list") && nargs >= 0) {
                int n = 0;
                DESCR_t init = NULVCL;
                if (nargs >= 1) {
                    DESCR_t nv = interp_eval(e->c[1]);
                    if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) {
                        if (IS_INT_fn(nv)) n = (int)nv.i;
                        else if (IS_REAL_fn(nv)) n = (int)nv.r;
                        else return FAILDESCR;
                        if (n < 0) return FAILDESCR;
                    }
                }
                if (nargs >= 2) {
                    DESCR_t iv = interp_eval(e->c[2]);
                    if (!IS_FAIL_fn(iv)) init = iv;
                }
                static int icnlist_reg2 = 0;
                if (!icnlist_reg2) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_reg2 = 1; }
                DESCR_t *elems = GC_malloc((n>0?n:1)*sizeof(DESCR_t));
                for (int i = 0; i < n; i++) elems[i] = init;
                DESCR_t eptr; eptr.v=DT_DATA; eptr.slen=0; eptr.ptr=(void*)elems;
                return DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
            }

            /* ── IC-5: Icon list builtins: push/pull/put/get ───────────────
             * Icon lists stored as DT_DATA with type name "icnlist" and a
             * DT_A array field "elems".  We use a simple GC array of DESCR_t. */
            if ((!strcmp(fn,"push")||!strcmp(fn,"put")||!strcmp(fn,"get")||!strcmp(fn,"pull")) && nargs >= 1) {
                DESCR_t ld = interp_eval(e->c[1]);
                /* push(L, v) — prepend */
                if (!strcmp(fn,"push") && nargs == 2) {
                    DESCR_t vd = interp_eval(e->c[2]);
                    if (ld.v != DT_DATA) return FAILDESCR;
                    int n = (int)FIELD_GET_fn(ld,"frame_size").i;
                    DESCR_t ea = FIELD_GET_fn(ld,"frame_elems");
                    DESCR_t *old = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
                    DESCR_t *nb = GC_malloc((n+1)*sizeof(DESCR_t));
                    nb[0] = vd;
                    if (old && n > 0) memcpy(nb+1,old,n*sizeof(DESCR_t));
                    FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=nb});
                    FIELD_SET_fn(ld,"frame_size",INTVAL(n+1));
                    return ld;
                }
                /* put(L, v) — append */
                if (!strcmp(fn,"put") && nargs == 2) {
                    DESCR_t vd = interp_eval(e->c[2]);
                    if (ld.v != DT_DATA) return FAILDESCR;
                    int n = (int)FIELD_GET_fn(ld,"frame_size").i;
                    DESCR_t ea = FIELD_GET_fn(ld,"frame_elems");
                    DESCR_t *old = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
                    DESCR_t *nb = GC_malloc((n+1)*sizeof(DESCR_t));
                    if (old && n > 0) memcpy(nb,old,n*sizeof(DESCR_t));
                    nb[n] = vd;
                    FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=nb});
                    FIELD_SET_fn(ld,"frame_size",INTVAL(n+1));
                    return ld;
                }
                /* get(L) — remove and return first element */
                if (!strcmp(fn,"get") && nargs == 1) {
                    if (ld.v != DT_DATA) return FAILDESCR;
                    DESCR_t ea = FIELD_GET_fn(ld,"frame_elems");
                    int n = (int)FIELD_GET_fn(ld,"frame_size").i;
                    DESCR_t *arr = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
                    if (!arr || n <= 0) return FAILDESCR;
                    DESCR_t ret = arr[0];
                    FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=arr+1});
                    FIELD_SET_fn(ld,"frame_size",INTVAL(n-1));
                    /* write back to var if possible */
                    if (e->c[1]->t==TT_VAR) {
                        int sl=(int)e->c[1]->v.ival;
                        if(sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=ld;
                    }
                    return ret;
                }
                /* pull(L) — remove and return last element */
                if (!strcmp(fn,"pull") && nargs == 1) {
                    if (ld.v != DT_DATA) return FAILDESCR;
                    DESCR_t ea = FIELD_GET_fn(ld,"frame_elems");
                    int n = (int)FIELD_GET_fn(ld,"frame_size").i;
                    DESCR_t *arr = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
                    if (!arr || n <= 0) return FAILDESCR;
                    DESCR_t ret = arr[n-1];
                    FIELD_SET_fn(ld,"frame_size",INTVAL(n-1));
                    if (e->c[1]->t==TT_VAR) {
                        int sl=(int)e->c[1]->v.ival;
                        if(sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=ld;
                    }
                    return ret;
                }
            }

            /* ── IC-5: char(n), ord(s) ──────────────────────────────────── */
            if (!strcmp(fn,"char") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                int n = (int)(IS_INT_fn(av) ? av.i : (long long)strtol(VARVAL_fn(av)?VARVAL_fn(av):"0",NULL,10));
                char *buf = GC_malloc(2); buf[0]=(char)(n&0xFF); buf[1]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"ord") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                const char *s = VARVAL_fn(av); if (!s||!*s) return FAILDESCR;
                return INTVAL((unsigned char)s[0]);
            }

            /* ── IC-5: left/right/center/repl/reverse/map/trim ─────────── */
            /* Icon spec: left(s, i, p) — i defaults to 1, p defaults to " ".
             * Omitted args (`left(s, , p)` or `left(s, &null, p)`) take the default.
             * Pad rule (verified against JCON test corpus):
             *   - LEFT-pads (right's left, center's left): fill[i % fl] from start of pad.
             *   - RIGHT-pads (left's right, center's right): pad ends at fill[fl-1] —
             *     pad[i] = fill[(i + fl - padlen) % fl] so the last pad char is fill[fl-1].
             */
            if (!strcmp(fn,"left") && nargs >= 1) {
                DESCR_t sv=interp_eval(e->c[1]); const char *s=VARVAL_fn(sv); if(!s)s="";
                int sl=(int)strlen(s);
                int n = 1;
                if (nargs >= 2) {
                    DESCR_t nv = interp_eval(e->c[2]);
                    if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) n = (int)to_int(nv);
                }
                if (n < 0) n = 0;
                const char *fill=" "; int fl=1;
                if (nargs >= 3) {
                    DESCR_t fd = interp_eval(e->c[3]);
                    if (!IS_FAIL_fn(fd) && fd.v != DT_SNUL) {
                        const char *fs = VARVAL_fn(fd);
                        if (fs && *fs) { fill = fs; fl = (int)strlen(fs); }
                    }
                }
                char *buf=GC_malloc(n+1);
                int copy = sl < n ? sl : n;
                for (int i = 0; i < copy; i++) buf[i] = s[i];
                int rpad = n - copy;
                /* Right-pad: pad ends at fill[fl-1].  Padlen rpad, pad index k=0..rpad-1,
                 * pad char = fill[(k + fl - rpad) % fl] (clamped non-negative). */
                for (int k = 0; k < rpad; k++) {
                    int idx = ((k + fl - rpad) % fl + fl) % fl;
                    buf[copy + k] = fill[idx];
                }
                buf[n]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"right") && nargs >= 1) {
                DESCR_t sv=interp_eval(e->c[1]); const char *s=VARVAL_fn(sv); if(!s)s="";
                int sl=(int)strlen(s);
                int n = 1;
                if (nargs >= 2) {
                    DESCR_t nv = interp_eval(e->c[2]);
                    if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) n = (int)to_int(nv);
                }
                if (n < 0) n = 0;
                const char *fill=" "; int fl=1;
                if (nargs >= 3) {
                    DESCR_t fd = interp_eval(e->c[3]);
                    if (!IS_FAIL_fn(fd) && fd.v != DT_SNUL) {
                        const char *fs = VARVAL_fn(fd);
                        if (fs && *fs) { fill = fs; fl = (int)strlen(fs); }
                    }
                }
                char *buf=GC_malloc(n+1);
                int pad = n - sl; if (pad < 0) pad = 0;
                /* Left-pad: fill cycles starting at fill[0]. */
                for (int i = 0; i < pad; i++) buf[i] = fill[i % fl];
                int srcoff = (sl > n) ? (sl - n) : 0;
                int copy = sl - srcoff; if (pad + copy > n) copy = n - pad;
                for (int i = 0; i < copy; i++) buf[pad + i] = s[srcoff + i];
                buf[n]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"center") && nargs >= 1) {
                DESCR_t sv=interp_eval(e->c[1]); const char *s=VARVAL_fn(sv); if(!s)s="";
                int sl=(int)strlen(s);
                int n = 1;
                if (nargs >= 2) {
                    DESCR_t nv = interp_eval(e->c[2]);
                    if (!IS_FAIL_fn(nv) && nv.v != DT_SNUL) n = (int)to_int(nv);
                }
                if (n < 0) n = 0;
                const char *fill=" "; int fl=1;
                if (nargs >= 3) {
                    DESCR_t fd = interp_eval(e->c[3]);
                    if (!IS_FAIL_fn(fd) && fd.v != DT_SNUL) {
                        const char *fs = VARVAL_fn(fd);
                        if (fs && *fs) { fill = fs; fl = (int)strlen(fs); }
                    }
                }
                char *buf=GC_malloc(n+1);
                int lpad = (n - sl) / 2; if (lpad < 0) lpad = 0;
                /* When source longer than n, take middle n chars; offset rounds UP
                 * (right-bias) per Icon spec.  Verified against JCON center test. */
                int srcoff = (sl > n) ? (sl - n + 1) / 2 : 0;
                int copy = sl - srcoff; if (lpad + copy > n) copy = n - lpad;
                int rpad = n - lpad - copy;
                /* lpad — left-aligned cycle starting at fill[0] */
                for (int i = 0; i < lpad; i++) buf[i] = fill[i % fl];
                /* source */
                for (int i = 0; i < copy; i++) buf[lpad + i] = s[srcoff + i];
                /* rpad — pad ends at fill[fl-1] */
                for (int k = 0; k < rpad; k++) {
                    int idx = ((k + fl - rpad) % fl + fl) % fl;
                    buf[lpad + copy + k] = fill[idx];
                }
                buf[n]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"repl") && nargs == 2) {
                DESCR_t sv=interp_eval(e->c[1]); const char *s=VARVAL_fn(sv); if(!s)s="";
                int n=(int)to_int(interp_eval(e->c[2])); if(n<0)n=0;
                int sl=(int)strlen(s); char *buf=GC_malloc(sl*n+1); buf[0]='\0';
                for(int i=0;i<n;i++) memcpy(buf+i*sl,s,sl); buf[sl*n]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"reverse") && nargs == 1) {
                DESCR_t sv=interp_eval(e->c[1]); const char *s=VARVAL_fn(sv); if(!s)s="";
                int sl=(int)strlen(s); char *buf=GC_malloc(sl+1);
                for(int i=0;i<sl;i++) buf[i]=s[sl-1-i]; buf[sl]='\0';
                return STRVAL(buf);
            }
            if (!strcmp(fn,"map") && nargs >= 1 && nargs <= 3) {
                /* Icon map(s, c1, c2): c1 defaults &ucase, c2 defaults &lcase.
                 * Each char of s — if it appears in c1 at index k, replaced by c2[k];
                 * else passed through. c1 and c2 must be same length (truncated).
                 * &null arg (omitted) → use the default. */
                static const char *UCASE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                static const char *LCASE = "abcdefghijklmnopqrstuvwxyz";
                DESCR_t sv=interp_eval(e->c[1]); const char *s=VARVAL_fn(sv); if(!s)s="";
                const char *from = UCASE, *to = LCASE;
                if (nargs >= 2) {
                    DESCR_t fv=interp_eval(e->c[2]);
                    if (fv.v != DT_SNUL) {
                        const char *fs = VARVAL_fn(fv);
                        if (fs) from = fs;
                    }
                }
                if (nargs >= 3) {
                    DESCR_t tv=interp_eval(e->c[3]);
                    if (tv.v != DT_SNUL) {
                        const char *ts = VARVAL_fn(tv);
                        if (ts) to = ts;
                    }
                }
                int sl=(int)strlen(s); char *buf=GC_malloc(sl+1);
                int fl=(int)strlen(from), tl=(int)strlen(to);
                /* Icon spec: c1 and c2 same length — fail otherwise.
                 * Walk c1 RIGHT-TO-LEFT so duplicate keys: last definition wins
                 * (Icon: map("abcdef","aa","bc") → "cbcdef" — second 'a'→'c'). */
                for (int i=0;i<sl;i++) {
                    char c=s[i]; int hit=0;
                    for (int j=fl-1;j>=0;j--) {
                        if (from[j]==c) { buf[i] = (j<tl) ? to[j] : c; hit=1; break; }
                    }
                    if (!hit) buf[i]=c;
                }
                buf[sl]='\0'; return STRVAL(buf);
            }
            if (!strcmp(fn,"trim") && (nargs == 1 || nargs == 2)) {
                DESCR_t sv=interp_eval(e->c[1]); const char *s=VARVAL_fn(sv); if(!s)s="";
                /* Determine cset of trim chars. Default: space.
                 * 2-arg: c arg may be string or cset (we treat both as char list).
                 * &null arg → use default. */
                const char *cset = " ";
                if (nargs == 2) {
                    DESCR_t cv = interp_eval(e->c[2]);
                    if (cv.v != DT_SNUL) {
                        const char *cs = VARVAL_fn(cv);
                        if (cs) cset = cs;
                    }
                }
                if (g_lang == 1 || nargs == 2) {
                    /* Icon: trim trailing chars in cset */
                    int sl=(int)strlen(s);
                    while (sl > 0 && strchr(cset, s[sl-1])) sl--;
                    char *buf=GC_malloc(sl+1); memcpy(buf,s,sl); buf[sl]='\0';
                    return STRVAL(buf);
                } else {
                    /* Raku trim — both ends, whitespace only */
                    while(*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
                    size_t len=strlen(s);
                    while(len>0&&(s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r')) len--;
                    char *buf=GC_malloc(len+1); memcpy(buf,s,len); buf[len]='\0';
                    return STRVAL(buf);
                }
            }
            /* ── IC-5: type(x), image(x), copy(x) ──────────────────────── */
            if (!strcmp(fn,"type") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                const char *t;
                if (IS_INT_fn(av))       t="integer";
                else if (IS_REAL_fn(av)) t="real";
                else if (av.v==DT_T)     t="table";
                else if (av.v==DT_A)     t="list";
                else if (av.v==DT_DATA)  {
                    /* check if icnlist tag */
                    DESCR_t tag = FIELD_GET_fn(av,"icn_type");
                    t = (tag.v==DT_S && tag.s) ? tag.s : "record";
                }
                else t="string";
                return STRVAL(t);
            }
            if (!strcmp(fn,"image") && nargs == 0) {
                /* IC-9: image() with no args — Icon spec: missing arg defaults to &null */
                return STRVAL("&null");
            }
            if (!strcmp(fn,"image") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_FAIL_fn(av)) return FAILDESCR;  /* IC-9: image(?T_empty) etc must fail */
                char *buf = GC_malloc(256);
                if (av.v == DT_SNUL)     return STRVAL("&null");
                /* IJ-3: file values */
                if (IS_INT_fn(av)) {
                    int idx=(int)av.i;
                    if(idx>=0&&idx<RAKU_FH_MAX&&raku_fh_name[idx]){snprintf(buf,256,"file(%s)",raku_fh_name[idx]);return STRVAL(buf);}
                    snprintf(buf,256,"%lld",(long long)av.i); return STRVAL(buf);
                }
                if (IS_REAL_fn(av))      { real_str(av.r,buf,128); return STRVAL(buf); }
                if (av.v==DT_T)          { snprintf(buf,128,"table(%d)",av.tbl?av.tbl->size:0); return STRVAL(buf); }
                /* IJ-3: DT_DATA — list vs record */
                if (av.v==DT_DATA && av.u) {
                    const char *tname=av.u->type?av.u->type->name:"record";
                    if(strcmp(tname,"icnlist")==0){int cnt=(av.u->type&&av.u->type->nfields>=2&&av.u->fields)?(int)av.u->fields[1].i:0;snprintf(buf,128,"list(%d)",cnt);return STRVAL(buf);}
                    snprintf(buf,256,"record(%s)",tname); return STRVAL(buf);
                }
                if (av.v==DT_DATA)       { return STRVAL("record"); }
                /* IJ-3: DT_E = procedure */
                if (av.v==DT_E) {
                    for(int i=0;i<proc_count;i++) if(proc_table[i].entry_pc==(int)av.i){snprintf(buf,128,"procedure %s",proc_table[i].name);return STRVAL(buf);}
                    return STRVAL("procedure");
                }
                /* String: produce "abc" with C-style escapes for control chars and " and \. */
                const char *s=VARVAL_fn(av); if (!s) s = "";
                int sl = (int)strlen(s);
                /* Worst-case expansion: each byte → \xNN (4 chars) + 2 quotes + NUL */
                char *out = GC_malloc(sl*4 + 3);
                int o = 0;
                out[o++] = '"';
                for (int i = 0; i < sl; i++) {
                    unsigned char c = (unsigned char)s[i];
                    switch (c) {
                        case '"':  out[o++]='\\'; out[o++]='"';  break;
                        case '\\': out[o++]='\\'; out[o++]='\\'; break;
                        case '\n': out[o++]='\\'; out[o++]='n';  break;
                        case '\t': out[o++]='\\'; out[o++]='t';  break;
                        case '\r': out[o++]='\\'; out[o++]='r';  break;
                        default:
                            if (c < 0x20 || c == 0x7f) {
                                o += snprintf(out+o, 5, "\\x%02x", c);
                            } else {
                                out[o++] = (char)c;
                            }
                    }
                }
                out[o++] = '"';
                out[o] = '\0';
                return STRVAL(out);
            }
            if (!strcmp(fn,"copy") && nargs == 1) {
                /* IC-9: shallow copy — Icon `copy(X)` returns a new container
                 * whose elements are the same descriptors (no deep copy of
                 * referenced values).  Previously a no-op (returned the same
                 * DESCR_t), which aliased the original — every !y +:= V
                 * mutated x as well.  Now allocates a fresh TBBLK_t / icnlist
                 * and copies entries.                                       */
                DESCR_t src = interp_eval(e->c[1]);
                if (src.v == DT_T && src.tbl) {
                    TBBLK_t *nt = table_new();
                    nt->dflt = src.tbl->dflt;
                    nt->init = src.tbl->init;
                    nt->inc  = src.tbl->inc;
                    for (int b = 0; b < TABLE_BUCKETS; b++)
                        for (TBPAIR_t *p = src.tbl->buckets[b]; p; p = p->next)
                            table_set_descr(nt, p->key, p->key_descr, p->val);
                    DESCR_t d; d.v = DT_T; d.slen = 0; d.tbl = nt;
                    return d;
                }
                if (src.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(src, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        DESCR_t ea = FIELD_GET_fn(src, "frame_elems");
                        int n = (int)FIELD_GET_fn(src, "frame_size").i;
                        DESCR_t *src_elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                        /* Build a fresh icnlist mirroring the TT_MAKELIST shape. */
                        DESCR_t *new_elems = (DESCR_t *)GC_malloc((size_t)(n > 0 ? n : 1) * sizeof(DESCR_t));
                        if (src_elems && n > 0) memcpy(new_elems, src_elems, (size_t)n * sizeof(DESCR_t));
                        DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void *)new_elems;
                        return DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
                    }
                }
                /* For strings, integers, reals, sets — value semantics already
                 * give a fresh descriptor; return src directly.              */
                return src;
            }

            /* ── IC-5: swap(L, k) is actually handled as TT_SWAP op ─────── */
            /* ── IC-5: size *L for DT_DATA lists ──────────────────────────
             * TT_SIZE is handled below; nothing to add in TT_FNC.           */

            /* ── IC-7: math builtins: abs, max, min, sqrt ──────────────── */
            if (!strcmp(fn,"abs") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_REAL_fn(av)) return REALVAL(fabs(av.r));
                return INTVAL(av.i < 0 ? -av.i : av.i);
            }
            if (!strcmp(fn,"max") && nargs >= 2) {
                DESCR_t best = interp_eval(e->c[1]);
                for (int _j = 2; _j <= nargs; _j++) {
                    DESCR_t cv = interp_eval(e->c[_j]);
                    int gt = (IS_REAL_fn(best)||IS_REAL_fn(cv))
                        ? ((IS_REAL_fn(best)?best.r:(double)best.i) < (IS_REAL_fn(cv)?cv.r:(double)cv.i))
                        : (best.i < cv.i);
                    if (gt) best = cv;
                }
                return best;
            }
            if (!strcmp(fn,"min") && nargs >= 2) {
                DESCR_t best = interp_eval(e->c[1]);
                for (int _j = 2; _j <= nargs; _j++) {
                    DESCR_t cv = interp_eval(e->c[_j]);
                    int lt = (IS_REAL_fn(best)||IS_REAL_fn(cv))
                        ? ((IS_REAL_fn(best)?best.r:(double)best.i) > (IS_REAL_fn(cv)?cv.r:(double)cv.i))
                        : (best.i > cv.i);
                    if (lt) best = cv;
                }
                return best;
            }
            if (!strcmp(fn,"sqrt") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                double v = IS_REAL_fn(av) ? av.r : (double)av.i;
                return REALVAL(sqrt(v));
            }
            /* seq(i) / seq(i,j) — generator: i, i+1, i+2, ... (up to j if given).
             * Returns first value here; coro_eval handles TT_FNC "seq" as a box. */
            if (!strcmp(fn,"seq") && nargs >= 1) {
                DESCR_t start = interp_eval(e->c[1]);
                return IS_INT_fn(start) ? start : INTVAL(1);
            }

            /* ── IC-7: sort(L) / sortf(L, n) ───────────────────────────── */
            if ((!strcmp(fn,"sort") && nargs == 1) || (!strcmp(fn,"sortf") && nargs == 2)) {
                DESCR_t ld = interp_eval(e->c[1]);
                if (ld.v != DT_DATA) return FAILDESCR;
                DESCR_t ea = FIELD_GET_fn(ld,"frame_elems");
                int n = (int)FIELD_GET_fn(ld,"frame_size").i;
                if (n <= 0) return ld;
                DESCR_t *arr = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
                if (!arr) return ld;
                /* copy into new array for sort */
                DESCR_t *sorted = GC_malloc(n * sizeof(DESCR_t));
                memcpy(sorted, arr, n * sizeof(DESCR_t));
                int field_idx = (!strcmp(fn,"sortf") && nargs == 2)
                    ? (int)to_int(interp_eval(e->c[2])) - 1 : -1;
                /* insertion sort — small lists only; correct semantics */
                for (int _i = 1; _i < n; _i++) {
                    DESCR_t key = sorted[_i]; int _j = _i - 1;
                    while (_j >= 0) {
                        DESCR_t a = sorted[_j], b = key;
                        if (field_idx >= 0) {
                            /* sortf: compare field field_idx of record via DATINST_t */
                            if (a.v==DT_DATA && a.u) { DATINST_t *_ia=(DATINST_t*)a.u; if(_ia->type&&field_idx<_ia->type->nfields) a=_ia->fields[field_idx]; }
                            if (b.v==DT_DATA && b.u) { DATINST_t *_ib=(DATINST_t*)b.u; if(_ib->type&&field_idx<_ib->type->nfields) b=_ib->fields[field_idx]; }
                        }
                        int cmp;
                        if (IS_INT_fn(a) && IS_INT_fn(b)) cmp = (a.i > b.i) ? 1 : (a.i < b.i) ? -1 : 0;
                        else { const char *sa=VARVAL_fn(a),*sb=VARVAL_fn(b); cmp=strcmp(sa?sa:"",sb?sb:""); }
                        if (cmp <= 0) break;
                        sorted[_j+1] = sorted[_j]; _j--;
                    }
                    sorted[_j+1] = key;
                }
                /* build new icnlist with sorted elements */
                DESCR_t res = ld; /* same type tag */
                FIELD_SET_fn(res,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=sorted});
                FIELD_SET_fn(res,"frame_size",INTVAL(n));
                return res;
            }

            /* ── IC-5: record constructor — Icon puts name in children[0]->v.sval,
             * not in e->v.sval, so the shared TT_FNC handler misses it.
             * Look up fn in sc_dat registry; if found, construct instance. */
            {
                ScDatType *_dt = sc_dat_find_type(fn);
                if (_dt) {
                    DESCR_t _args[FRAME_SLOT_MAX];
                    for (int _j = 0; _j < nargs && _j < FRAME_SLOT_MAX; _j++)
                        _args[_j] = interp_eval(e->c[1+_j]);
                    return sc_dat_construct(_dt, _args, nargs);
                }
            }

            /* IJ-3: proc value stored in a variable — e.g. `pv := p0; pv()`.
             * Name "fn" not found in proc_table or builtins above; evaluate
             * the callee node to get its stored DT_E / DT_S descriptor. */
            {
                DESCR_t _callee = interp_eval(e->c[0]);
                if (_callee.v == DT_E) {
                    DESCR_t _args[FRAME_SLOT_MAX];
                    for (int _j = 0; _j < nargs && _j < FRAME_SLOT_MAX; _j++)
                        _args[_j] = interp_eval(e->c[1+_j]);
                    for (int _i = 0; _i < proc_count; _i++)
                        if (proc_table[_i].entry_pc == (int)_callee.i)
                            return proc_table_call(_i, _args, nargs);
                    if (_callee.slen < (uint32_t)proc_count)
                        return proc_table_call((int)_callee.slen, _args, nargs);
                    return FAILDESCR;
                }
                if (_callee.v == DT_S && _callee.s) {
                    DESCR_t _args[FRAME_SLOT_MAX];
                    for (int _j = 0; _j < nargs && _j < FRAME_SLOT_MAX; _j++)
                        _args[_j] = interp_eval(e->c[1+_j]);
                    DESCR_t _out = FAILDESCR;
                    if (icn_try_call_builtin_by_name(_callee.s, _args, nargs, &_out)) return _out;
                }
            }
            return NULVCL;
        }
        case TT_ALT:
        case TT_ALTERNATE:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_ALTERNATE unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_EVERY:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_EVERY unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_WHILE:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_WHILE unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_UNTIL:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_UNTIL unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_REPEAT:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_REPEAT unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_SUSPEND:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_SUSPEND unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_SEQ:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_SEQ unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_SEQ_EXPR:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_SEQ_EXPR unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_IF:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_IF unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_LOOP_NEXT:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_LOOP_NEXT unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_RETURN:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_RETURN unreachable in mode 1 (RS-24b)\n"); abort();
        case TT_PROC_FAIL:
            fprintf(stderr, "FATAL interp_eval icon-frame: TT_PROC_FAIL unreachable in mode 1 (RS-24b)\n"); abort();
        default: break;
        }
    }

    switch (e->t) {
    case TT_ILIT:   return INTVAL(e->v.ival);
    case TT_FLIT:   return REALVAL(e->v.dval);
    case TT_QLIT:   return e->v.sval ? STRVAL(e->v.sval) : NULVCL;
    case TT_NUL:    return NULVCL;

    case TT_VAR:
        if (e->v.sval && *e->v.sval) {
            /* IC-9: Icon scan-state keywords read in scan body outside any Icon proc frame.
             * When scan_depth > 0 and frame_depth == 0 (e.g. "str" ? body in main()),
             * the icon-frame switch above is skipped, so &pos and &subject must be handled here.
             * Only fires for Icon scan context; SNOBOL4 mode uses NV_GET_fn for &pos etc. */
            if (scan_depth > 0 && !frame_depth && e->v.sval[0] == '&') {
                const char *kw = e->v.sval + 1;
                if (!strcmp(kw,"pos"))     return INTVAL(scan_pos);
                if (!strcmp(kw,"subject")) return scan_subj ? STRVAL(scan_subj) : NULVCL;
            }
            /* SN-3: shadow table takes priority — param/local named after a pattern
             * primitive (LEN, ANY, SPAN, …) is invisible to NV_GET_fn. */
            { DESCR_t _sv; if (shadow_get(e->v.sval, &_sv)) return _sv; }
            DESCR_t _vr = NV_GET_fn(e->v.sval);
            if (!IS_NULL(_vr)) return _vr;
            /* Zero-arg builtin (ARB, REM, FAIL, SUCCEED, etc.) stored as
               function, not variable — only try if name is a registered fn.
               Guard prevents unset ordinary variables from spuriously calling
               APPLY_fn and triggering Error 5.
               SN-26-binmon-3way: if APPLY_fn returns FAIL (e.g. caller is
               a >0-arity builtin like VALUE invoked with 0 args), don't
               propagate the FAIL up — fall back to the unset-var value
               (NULVCL).  This matches CSNOBOL4 / SPITBOL semantics where
               an unbound identifier passed as an argument is the empty
               string, not a failure that aborts the enclosing call. */
            if (FNCEX_fn(e->v.sval)) {
                DESCR_t _fr = APPLY_fn(e->v.sval, NULL, 0);
                if (!IS_FAIL_fn(_fr) && !IS_NULL(_fr)) return _fr;
            }
            return _vr; /* unset variable */
        }
        return NULVCL;

    case TT_KEYWORD: {
        if (!e->v.sval || !*e->v.sval) return NULVCL;
        /* Keywords are case-insensitive; NV stores them uppercase (e.g. "LCASE","UCASE").
         * Lexer strips '&' and preserves original case — uppercase before lookup. */
        char uc[64]; int _ki;
        for (_ki = 0; e->v.sval[_ki] && _ki < 63; _ki++)
            uc[_ki] = toupper((unsigned char)e->v.sval[_ki]);
        uc[_ki] = '\0';
        return NV_GET_fn(uc);
    }

    case TT_INTERROGATE: {
        /* ?X — o$int: null string if X succeeds; fail if X fails */
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        return NULVCL;
    }

    case TT_NAME: {
        /* .X — dot operator: delegate to NAME_fn (snobol4.c export).
         * NAME_fn returns NAMEVAL for keywords/IO vars (not addressable by ptr)
         * and NAMEPTR (interior ptr) for ordinary NV cells.
         * BP-1: .field(x) — TT_FNC child with one arg — must return NAMEPTR into
         * the DATA struct field cell, not a name-table lookup. */
        if (e->n < 1) return FAILDESCR;
        tree_t *child = e->c[0];
        if (child->t == TT_FNC && child->v.sval && child->n == 1) {
            DESCR_t inst = interp_eval(child->c[0]);
            DESCR_t *cell = data_field_ptr(child->v.sval, inst);
            if (cell) return NAMEPTR(cell);
        }
        if ((child->t == TT_VAR || child->t == TT_KEYWORD)
                && child->v.sval)
            return NAME_fn(child->v.sval);
        DESCR_t *cell = interp_eval_ref(child);
        if (cell) return NAMEPTR(cell);
        return FAILDESCR;
    }

    case TT_MNS:
        if (e->n < 1) return FAILDESCR;
        return neg(interp_eval(e->c[0]));

    /* OE-5: TT_RETURN for Icon/Raku return statements */
    case TT_RETURN: {
        if (frame_depth > 0) {
            FRAME.return_val = (e->n > 0)
                ? interp_eval(e->c[0]) : NULVCL;
            FRAME.returning = 1;
            return FRAME.return_val;
        }
        return (e->n > 0) ? interp_eval(e->c[0]) : NULVCL;
    }

    /* Icon/Raku fail-return — distinct from TT_FAIL (SNOBOL4 FAIL pattern). */
    case TT_PROC_FAIL: {
        if (frame_depth > 0) {
            FRAME.return_val = FAILDESCR;
            FRAME.returning  = 1;
        }
        return FAILDESCR;
    }

    case TT_PLS: {
        /* Unary + coerces operand to numeric (int or real) */
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (IS_INT(v) || IS_REAL(v)) return v;
        /* String → try integer, then real */
        const char *s = VARVAL_fn(v);
        if (!s || !*s) return INTVAL(0);
        char *end = NULL;
        long long iv = strtoll(s, &end, 10);
        if (end && *end == '\0') return INTVAL(iv);
        double dv = strtod(s, &end);
        if (end && *end == '\0') return REALVAL(dv);
        return INTVAL(0);
    }

    case TT_OPSYN: {
        /* OPSYN operator: sval holds the operator symbol ("@", "&").
         * Dispatch via APPLY_fn — OPSYN registration aliased the symbol
         * to the target function via register_fn_alias. */
        if (!e->v.sval) return FAILDESCR;
        if (e->n == 2) {
            DESCR_t l = interp_eval(e->c[0]);
            DESCR_t r = interp_eval(e->c[1]);
            if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
            DESCR_t args[2] = { l, r };
            return APPLY_fn(e->v.sval, args, 2);
        } else if (e->n == 1) {
            DESCR_t v = interp_eval(e->c[0]);
            if (IS_FAIL_fn(v)) return FAILDESCR;
            DESCR_t args[1] = { v };
            return APPLY_fn(e->v.sval, args, 1);
        }
        return FAILDESCR;
    }

    case TT_ADD: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return add(l, r);
    }
    case TT_SUB: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return sub(l, r);
    }
    case TT_MUL: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return mul(l, r);
    }
    case TT_DIV: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return DIVIDE_fn(l, r);
    }
    case TT_MOD: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        long li = IS_INT_fn(l) ? l.i : (long)l.r;
        long ri = IS_INT_fn(r) ? r.i : (long)r.r;
        return ri ? INTVAL(li % ri) : FAILDESCR;
    }
    case TT_POW: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        /* SNOBOL4: int**int with non-negative exponent → integer; any real operand → real.
         * Icon (g_lang==1): `^` always produces real, even with integer operands.    */
        if (g_lang != 1 && IS_INT_fn(l) && IS_INT_fn(r) && r.i >= 0) {
            long base = l.i, result = 1; int exp = (int)r.i;
            for (int k = 0; k < exp; k++) result *= base;
            return INTVAL(result);
        }
        double base = IS_REAL_fn(l) ? l.r : (double)l.i;
        double exp  = IS_REAL_fn(r) ? r.r : (double)r.i;
        return (DESCR_t){ .v = DT_R, .r = pow(base, exp) };
    }

    case TT_CAT:
    case TT_SEQ: {
        if (e->n == 0) return NULVCL;
        /* IC-9 (2026-05-02): In Icon mode, TT_SEQ is the & (conjunction) operator —
         * evaluate children left to right; if any fails, return FAILDESCR; return
         * the last child's value.  This is completely different from SNOBOL4's
         * TT_SEQ which is string/pattern concatenation.  Gate on g_lang==1 AND
         * e->t==TT_SEQ so the TT_CAT fall-through (||) is unaffected. */
        if (g_lang == 1 && e->t == TT_SEQ) {
            DESCR_t last = NULVCL;
            for (int ci = 0; ci < e->n; ci++) {
                last = interp_eval(e->c[ci]);
                if (IS_FAIL_fn(last)) return FAILDESCR;
            }
            return last;
        }
        /* DYN-59: interp_eval is STRING context by default; pattern context
         * uses interp_eval_pat() which calls pat_cat unconditionally.
         * DYN-68: mixed-mode: if the accumulated value is DT_P (pattern),
         * switch to pat_cat so that pattern-building expressions like
         * "icase = icase (upr(c) | lwr(c))" work correctly in value context.
         * SNOBOL4 rule: concatenation of pattern with anything yields pattern.
         * RT-112: once we detect a pattern operand, re-evaluate ALL remaining
         * children via interp_eval_pat so *var/*func become XDSAR/XATP nodes
         * rather than frozen DT_E (which pat_cat cannot handle).
         * SN-26c-parseerr-g: pre-scan children for TT_DEFER (i.e. *X).  In
         * value context, interp_eval(TT_DEFER) returns DT_E (frozen
         * expression), which is NOT detected by IS_PAT(acc).  If TT_DEFER is
         * the FIRST child, the in_pat_mode promotion from the loop below
         * never fires, and CONCAT_fn(DT_E, "B") produces garbage.  Beauty's
         *   pat = *snoFunction $'(' *snoExprList $')'
         * lowered to TT_SEQ(TT_DEFER, TT_VAR, TT_DEFER, TT_VAR) is the canonical
         * victim.  Fix: if any child is TT_DEFER, we are building a pattern;
         * use interp_eval_pat for child[0] and pat_cat for the rest. */
        int has_defer = 0;
        for (int j = 0; j < e->n; j++) {
            tree_t *cj = e->c[j];
            if (cj && cj->t == TT_DEFER) { has_defer = 1; break; }
        }
        DESCR_t acc = has_defer ? interp_eval_pat(e->c[0])
                                : interp_eval(e->c[0]);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        int in_pat_mode = has_defer ? 1 : IS_PAT(acc);
        for (int i = 1; i < e->n; i++) {
            DESCR_t nxt;
            if (in_pat_mode) {
                nxt = interp_eval_pat(e->c[i]);
            } else {
                nxt = interp_eval(e->c[i]);
            }
            if (IS_FAIL_fn(nxt)) return FAILDESCR;
            if (in_pat_mode || IS_PAT(nxt)) {
                if (!in_pat_mode) {
                    /* SN-26-bridge-coverage-u: First pattern seen mid-concat.
                     * Do NOT re-evaluate `nxt` — interp_eval on pattern-shaped
                     * children (TT_ALT etc.) already returns DT_P with all inner
                     * function calls fired exactly once.  Re-evaluating would
                     * call those functions a second time (e.g. the canonical
                     * `leader (upr(letter) | lwr(letter))` would call upr/lwr
                     * twice each).  Keep `nxt` as-is and pat_cat will coerce
                     * any non-pattern operands via pat_to_patnd.
                     *
                     * Earlier children (0..i-1) likewise don't need re-eval:
                     * any TT_DEFER among them is already caught by the has_defer
                     * pre-scan above (which would have made in_pat_mode=1 from
                     * the start), so all prior `acc` values are valid scalars
                     * that pat_to_patnd will coerce to pat_lit.  pat_cat with
                     * a string LHS and pattern RHS produces an XCAT correctly. */
                    in_pat_mode = 1;
                }
                acc = pat_cat(acc, nxt);
            } else {
                /* SPITBOL rule: if either operand is null string, return the
                 * other operand UNCHANGED (no type coercion). Spec §concat:
                 * "if either operand is the null string, the other operand is
                 *  returned unchanged. It is not coerced into the string type."
                 * IC-9 (2026-05-01): Icon mode is different — `""` is a real
                 * (empty) string, not `&null`. `"" || ""` must yield `""`
                 * (DT_S, slen=0), not `&null` (DT_SNUL). Force string concat
                 * via CONCAT_fn unconditionally in Icon mode, AND post-coerce
                 * a DT_SNUL/null result back to a real empty DT_S so image()
                 * etc. distinguish empty-string from &null. */
                if (g_lang != 1 && acc.v == DT_SNUL)
                    acc = nxt;
                else if (g_lang != 1 && nxt.v == DT_SNUL)
                    { /* acc unchanged */ }
                else
                    acc = CONCAT_fn(acc, nxt);
                if (g_lang == 1 && (acc.v == DT_SNUL || (acc.v == DT_S && !acc.s))) {
                    DESCR_t empty; empty.v = DT_S; empty.s = GC_strdup(""); empty.slen = 0;
                    acc = empty;
                }
            }
            if (IS_FAIL_fn(acc)) return FAILDESCR;
        }
        return acc;
    }

    case TT_ASSIGN: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t val = interp_eval(e->c[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        tree_t *lv = e->c[0];
        /* IC-9: string-section / string-index lvalue. */
        if (lv && (lv->t == TT_SECTION || lv->t == TT_SECTION_PLUS ||
                   lv->t == TT_SECTION_MINUS)) {
            if (icn_string_section_assign(lv, val)) return val;
            return FAILDESCR;
        }
        if (lv && lv->t == TT_IDX && lv->n == 2) {
            /* Try string-index first; if base is non-string, fall through to subscript_set. */
            if (icn_string_section_assign(lv, val)) return val;
            DESCR_t _b = interp_eval(lv->c[0]);
            if (_b.v == DT_S || _b.v == DT_SNUL) return FAILDESCR;
        }
        if (lv && lv->t == TT_VAR && lv->v.sval && lv->v.sval[0] == '&' &&
                scan_depth > 0 && !frame_depth) {
            /* IC-9: Icon scan-state keyword write in scan body outside any Icon proc frame. */
            if (!kw_assign(lv->v.sval + 1, val)) return FAILDESCR;
        } else if (lv && lv->t == TT_VAR && lv->v.sval)
            NV_SET_fn(lv->v.sval, val);  /* inner expr assign: no trace (stmt-level already traced) */
        else if (lv && lv->t == TT_IDX && lv->n >= 2) {
            /* arr<i> = val  or  arr<i,j> = val */
            DESCR_t base = interp_eval(lv->c[0]);
            if (!IS_FAIL_fn(base)) {
                DESCR_t idx = interp_eval(lv->c[1]);
                if (!IS_FAIL_fn(idx)) {
                    if (lv->n >= 3) {
                        DESCR_t idx2 = interp_eval(lv->c[2]);
                        if (!IS_FAIL_fn(idx2))
                            subscript_set2(base, idx, idx2, val);
                    } else {
                        subscript_set(base, idx, val);
                    }
                }
            }
        }
        else if (lv && lv->t == TT_FNC && lv->v.sval && lv->n >= 1) {
            if (strcmp(lv->v.sval, "ITEM") == 0 && lv->n >= 2) {  /* SN-19 */
                /* ITEM(arr, i [,j]) = val — programmatic subscript setter */
                DESCR_t base = interp_eval(lv->c[0]);
                if (!IS_FAIL_fn(base)) {
                    DESCR_t idx = interp_eval(lv->c[1]);
                    if (!IS_FAIL_fn(idx)) {
                        if (lv->n >= 3) {
                            DESCR_t idx2 = interp_eval(lv->c[2]);
                            if (!IS_FAIL_fn(idx2))
                                subscript_set2(base, idx, idx2, val);
                        } else {
                            subscript_set(base, idx, val);
                        }
                    }
                }
            } else {
                /* DATA field setter: fname(obj) = val
                 * Evaluate the first argument; if it's a DT_DATA instance,
                 * dispatch through FIELD_SET_fn using the function name as field name. */
                DESCR_t obj = interp_eval(lv->c[0]);
                if (!IS_FAIL_fn(obj))
                    FIELD_SET_fn(obj, lv->v.sval, val);
            }
        }
        else if (lv && lv->t == TT_FIELD && lv->v.sval && lv->n >= 1) {
            /* IC-5: record field lvalue:  obj.fieldname := val */
            DESCR_t obj = interp_eval(lv->c[0]);
            if (!IS_FAIL_fn(obj)) {
                DESCR_t *cell = data_field_ptr(lv->v.sval, obj);
                if (cell) *cell = val;
            }
        }
        else if (lv && lv->t == TT_INDIRECT && lv->n > 0) {
            tree_t *ichild = lv->c[0];
            const char *nm = NULL;
            if (ichild->t == TT_CAPT_COND_ASGN && ichild->n == 1
                    && ichild->c[0]->t == TT_VAR && ichild->c[0]->v.sval)
                nm = ichild->c[0]->v.sval;
            else { DESCR_t nd = interp_eval(ichild); nm = VARVAL_fn(nd); }
            if (nm && *nm) {
                char *fn = GC_strdup(nm); sno_fold_name(fn);  /* SN-19 */
                NV_SET_fn(fn, val);  /* inner expr: no trace */
            }
        }
        return val;
    }

    case TT_INDIRECT: {
        if (e->n < 1) return FAILDESCR;
        tree_t *child = e->c[0];
        /* $.var parses as TT_INDIRECT(TT_NAME(TT_CAPT_COND_ASGN(TT_VAR)))
         * $.var<idx> parses as TT_INDIRECT(TT_NAME(TT_CAPT_COND_ASGN(TT_VAR, idx)))
         * The TT_NAME wrapper (from the dot-prefix parse) is unwrapped first.
         * Semantics: the identifier name is used literally (not its value).
         *   $.var      => NV_GET_fn("var")
         *   $.var<idx> => subscript( NV_GET_fn("var"), idx ) */
        /* $.var<idx> parses as TT_INDIRECT(TT_NAME(TT_CAPT_COND_ASGN(TT_VAR,idx)))
         * — the TT_NAME wrapper is from the dot-prefix parse. Unwrap it.
         * $X (no dot) parses as TT_INDIRECT(TT_VAR("X")) — no TT_NAME wrapper.
         * Track whether we unwrapped an TT_NAME to distinguish:
         *   $.var = literal name lookup (return var's value directly)
         *   $X    = runtime indirect (evaluate X, use its value as lookup name) */
        int had_name_wrap = 0;
        if (child->t == TT_NAME && child->n == 1) {
            child = child->c[0];
            had_name_wrap = 1;
        }

        /* TT_VAR child: $.var (had_name_wrap=1) vs $X (had_name_wrap=0) */
        if (child->t == TT_VAR && child->v.sval) {
            if (had_name_wrap)
                return NV_GET_fn(child->v.sval);          /* $.var — literal name */
            /* $X — evaluate X's runtime value, use it as the variable name.
             * IS_NAMEPTR (slen=1, .ptr = live DESCR_t*) vs IS_NAMEVAL (slen=0, .s = name).
             * Do NOT use ptr!=NULL — for NAMEVAL .s and .ptr alias the same union. */
            DESCR_t _xv = NV_GET_fn(child->v.sval);
            if (IS_NAMEPTR(_xv)) return NAME_DEREF_PTR(_xv);
            if (IS_NAMEVAL(_xv)) return NV_GET_fn(_xv.s);
            const char *_xnm0 = VARVAL_fn(_xv);
            if (!_xnm0 || !*_xnm0) return NULVCL;
            char *_xnm = GC_strdup(_xnm0); sno_fold_name(_xnm);  /* SN-19 */
            DESCR_t _xnamed = NV_GET_fn(_xnm);
            if (IS_NAMEPTR(_xnamed)) return NAME_DEREF_PTR(_xnamed);
            if (IS_NAMEVAL(_xnamed)) return NV_GET_fn(_xnamed.s);
            return _xnamed;
        }

        /* TT_IDX after TT_NAME unwrap: $.var<idx> subscript form
         * children[0]=TT_VAR "name", children[1]=index expr */
        if (child->t == TT_IDX && child->n >= 2
                && child->c[0]->t == TT_VAR && child->c[0]->v.sval) {
            const char *nm = child->c[0]->v.sval;
            DESCR_t base = NV_GET_fn(nm);
            if (IS_FAIL_fn(base)) return FAILDESCR;
            if (child->n == 2) {
                DESCR_t idx = interp_eval(child->c[1]);
                if (IS_FAIL_fn(idx)) return FAILDESCR;
                return subscript_get(base, idx);
            }
            DESCR_t i1 = interp_eval(child->c[1]);
            DESCR_t i2 = interp_eval(child->c[2]);
            if (IS_FAIL_fn(i1) || IS_FAIL_fn(i2)) return FAILDESCR;
            return subscript_get2(base, i1, i2);
        }

        if (child->t == TT_CAPT_COND_ASGN && child->n == 1) {
            tree_t *inner = child->c[0];
            /* $.var<idx> case: dot child is TT_IDX whose base is TT_VAR */
            if (inner->t == TT_IDX && inner->n >= 2
                    && inner->c[0]->t == TT_VAR
                    && inner->c[0]->v.sval) {
                const char *nm = inner->c[0]->v.sval;
                DESCR_t base = NV_GET_fn(nm);
                if (IS_FAIL_fn(base)) return FAILDESCR;
                if (inner->n == 2) {
                    DESCR_t idx = interp_eval(inner->c[1]);
                    if (IS_FAIL_fn(idx)) return FAILDESCR;
                    return subscript_get(base, idx);
                }
                DESCR_t i1 = interp_eval(inner->c[1]);
                DESCR_t i2 = interp_eval(inner->c[2]);
                if (IS_FAIL_fn(i1) || IS_FAIL_fn(i2)) return FAILDESCR;
                return subscript_get2(base, i1, i2);
            }
            /* $.var case: dot child is plain TT_VAR — but only if this is a
             * literal $.var (direct name lookup). For $X (runtime indirect),
             * evaluate X's value and use THAT as the variable name.
             * Distinction: $.var uses the identifier literally; $X uses X's value.
             * Since parser wraps both as TT_CAPT_COND_ASGN(TT_VAR), we must
             * evaluate the inner var and use its string value as the lookup key. */
            if (inner->t == TT_VAR && inner->v.sval) {
                DESCR_t xval = NV_GET_fn(inner->v.sval);
                if (IS_NAMEPTR(xval)) return NAME_DEREF_PTR(xval);
                if (IS_NAMEVAL(xval)) return NV_GET_fn(xval.s);
                const char *nm2_0 = VARVAL_fn(xval);
                if (!nm2_0 || !*nm2_0) return NULVCL;
                char *nm2 = GC_strdup(nm2_0); sno_fold_name(nm2);  /* SN-19 */
                DESCR_t named = NV_GET_fn(nm2);
                if (IS_NAMEPTR(named)) return NAME_DEREF_PTR(named);
                if (IS_NAMEVAL(named)) return NV_GET_fn(named.s);
                return named;
            }
            /* fallback: evaluate inner directly */
            DESCR_t nd = interp_eval(inner);
            const char *nm2_0 = VARVAL_fn(nd);
            if (!nm2_0 || !*nm2_0) return NULVCL;
            char *nm2 = GC_strdup(nm2_0); sno_fold_name(nm2);  /* SN-19 */
            DESCR_t named2 = NV_GET_fn(nm2);
            if (IS_NAMEPTR(named2)) return NAME_DEREF_PTR(named2);
            if (IS_NAMEVAL(named2)) return NV_GET_fn(named2.s);
            return named2;
        }
        /* $expr — indirect through runtime string/name value */
        DESCR_t nd = interp_eval(child);
        if (IS_NAMEPTR(nd)) return NAME_DEREF_PTR(nd);
        if (IS_NAMEVAL(nd)) return NV_GET_fn(nd.s);
        const char *nm0 = VARVAL_fn(nd);
        if (!nm0 || !*nm0) return NULVCL;
        char *nm = GC_strdup(nm0); sno_fold_name(nm);  /* SN-19 */
        /* The named variable might also be a DT_N — dereference one more level */
        DESCR_t named = NV_GET_fn(nm);
        if (IS_NAMEPTR(named)) return NAME_DEREF_PTR(named);
        if (IS_NAMEVAL(named)) return NV_GET_fn(named.s);
        return named;
    }

    case TT_FNC: {
        if (!e->v.sval || !*e->v.sval) return FAILDESCR;

        /* SB-6.X: ARBNO(P)/FENCE(P) take a *pattern* arg.  When evaluated in
         * value context (e.g. RHS of  Pat = ARBNO(*Cmd) ), the default
         * arg-eval path below uses interp_eval (value context) on each child,
         * which turns TT_DEFER(TT_VAR) into DT_E (frozen value-form expr).
         * APPLY_fn(ARBNO, DT_E, ...) then materialises the DT_E by thawing
         * Cmd's *current* value as a literal — losing deferred-ref semantics
         * (XDSAR), so subsequent re-binds of Cmd are not seen and the
         * pattern even fails to match its own initial value.
         *
         * Mirror the pat-context fix at interp_eval_pat TT_FNC (above): when
         * the function name is ARBNO or FENCE, evaluate the first child in
         * pattern context so TT_DEFER(TT_VAR) becomes a proper XDSAR ref node,
         * then call pat_arbno/pat_fence_p directly.  All other TT_FNC names
         * fall through to the value-context arg eval as before. */
        if (e->n > 0) {
            if (strcmp(e->v.sval, "ARBNO") == 0) {
                DESCR_t _inner = interp_eval_pat(e->c[0]);
                if (IS_FAIL_fn(_inner)) return FAILDESCR;
                return pat_arbno(_inner);
            }
            if (strcmp(e->v.sval, "FENCE") == 0) {
                DESCR_t _inner = interp_eval_pat(e->c[0]);
                if (IS_FAIL_fn(_inner)) return FAILDESCR;
                return pat_fence_p(_inner);
            }
        }

        /* DEFINE('spec'[,'entry']) — register user function.
         * SIL DEFIFN returns the function name string on success (DIFFER-able).
         * Extract name = everything before '(' or ',' in spec. */
        if (strcmp(e->v.sval, "DEFINE") == 0) {  /* SN-19: AST token, canonical */
            const char *spec = define_spec_from_expr(e);
            if (spec && *spec) {
                const char *entry = define_entry_from_expr(e);
                if (entry) DEFINE_fn_entry(spec, NULL, entry);
                else       DEFINE_fn(spec, NULL);
                /* SNOBOL4 spec: DEFINE returns null string on success */
                return NULVCL;
            }
            return FAILDESCR;   /* malformed spec → FAIL per SIL */
        }

        int nargs = e->n;
        DESCR_t *args = nargs > 0
            ? (DESCR_t *)alloca((size_t)nargs * sizeof(DESCR_t))
            : NULL;
        for (int i = 0; i < nargs; i++) {
            args[i] = interp_eval(e->c[i]);
            if (IS_FAIL_fn(args[i])) return FAILDESCR;
        }

        /* DYN-70 fix: check for user-defined body label BEFORE calling APPLY_fn.
         * APPLY_fn internally dispatches user functions via call_user_function,
         * so calling APPLY_fn then call_user_function again causes double execution.
         * Rule: if a body label exists in the program → user-defined → skip APPLY_fn.
         * Builtins never have a body label; user functions always do (prescan_defines). */
        {
            /* Resolve body: try as-is, uppercase, then entry_label (OPSYN aliases) */
            const tree_t *body = label_lookup(e->v.sval);
            if (!body) {
                char ufn[128];
                size_t fl = strlen(e->v.sval);
                if (fl >= sizeof(ufn)) fl = sizeof(ufn)-1;
                for (size_t i = 0; i <= fl; i++) ufn[i] = (char)toupper((unsigned char)e->v.sval[i]);
                body = label_lookup(ufn);
            }
            if (!body) {
                const char *el = FUNC_ENTRY_fn(e->v.sval);
                if (el) body = label_lookup(el);
            }
            if (body) {
                /* User-defined function — call interpreter directly, never via APPLY_fn */
                DESCR_t r = call_user_function(e->v.sval, args, nargs);
                if (IS_NAME(r)) return NAME_DEREF(r);
                return r;
            }
        }
        /* ── U-22: cross-language fallback in value-context TT_FNC ────────────
         * SNO body lookup above found nothing.  Try Icon proc table, then
         * Prolog pred table, before falling through to builtins/APPLY_fn. */
        {
            /* Try Icon proc table (case-sensitive) — CH-17g-call-sites: via SM expression when entry_pc resolved */
            for (int _ci = 0; _ci < proc_count; _ci++) {
                if (strcmp(proc_table[_ci].name, e->v.sval) == 0)
                    return proc_table_call(_ci, args, nargs);
            }
            /* Try Prolog pred table: "name/arity" key */
            if (g_pl_active) {
                char _pk[256];
                snprintf(_pk, sizeof _pk, "%s/%d", e->v.sval, nargs);
                tree_t *_choice = pl_pred_table_lookup(&g_pl_pred_table, _pk);
                if (_choice) {
                    Term **_pl_args = (nargs > 0) ? pl_env_new(nargs) : NULL;
                    Term **_saved   = g_pl_env;
                    g_pl_env = _pl_args;
                    /* CH-17e: use SM-expression path when entry_pc is resolved */
                    Pl_PredEntry *_pe = pl_pred_entry_lookup(_pk);
                    extern SM_Program *g_current_sm_prog;
                    bb_node_t _root = (_pe && _pe->entry_pc >= 0 && g_current_sm_prog != NULL)
                        ? pl_box_choice_pc(_pe->entry_pc, g_pl_env, nargs)
                        : pl_box_choice(_choice, g_pl_env, nargs);
                    int _ok = bb_broker(_root, BB_ONCE, NULL, NULL);
                    g_pl_env = _saved;
                    return _ok ? INTVAL(1) : FAILDESCR;
                }
            }
        }
        /* IDENT/DIFFER: per SPITBOL spec, arguments must have SAME data type AND value.
         * IDENT(3, '3') FAILS (integer vs string). IDENT(S) succeeds iff S is null string.
         * DIFFER(S,T) succeeds iff they differ in type OR value. DIFFER(S) succeeds iff S != ''.
         * Both return NULVCL on success. */
        /* EVAL/CODE: binary _EVAL_/_CODE_ are stubs; route through our full impl. */
        if (strcmp(e->v.sval, "EVAL") == 0) {  /* SN-19 */
            if (nargs < 1) return FAILDESCR;
            extern DESCR_t EVAL_fn(DESCR_t);
            DESCR_t _er = EVAL_fn(args[0]);
            return _er;
        }
        if (strcmp(e->v.sval, "CODE") == 0) {  /* SN-19 */
            if (nargs < 1) return FAILDESCR;
            const char *_cs = VARVAL_fn(args[0]);
            extern DESCR_t code(const char *);
            return (_cs && *_cs) ? code(_cs) : FAILDESCR;
        }
        if (strcmp(e->v.sval, "IDENT") == 0) {  /* SN-19 */
            if (nargs == 1) {
                /* IDENT(S) — succeed if S is null string */
                return IS_NULL_fn(args[0]) ? NULVCL : FAILDESCR;
            }
            if (nargs >= 2) {
                /* Normalize: treat DT_SNUL and DT_S("") as same null type for comparison */
                int a_null = IS_NULL_fn(args[0]), b_null = IS_NULL_fn(args[1]);
                if (a_null && b_null) return NULVCL;   /* both null → identical */
                if (a_null || b_null) return FAILDESCR; /* one null, one not → differ */
                /* Same non-null type AND same string value */
                if (args[0].v != args[1].v) return FAILDESCR;
                const char *sa = VARVAL_fn(args[0]);
                const char *sb = VARVAL_fn(args[1]);
                if (!sa) sa = ""; if (!sb) sb = "";
                return strcmp(sa, sb) == 0 ? NULVCL : FAILDESCR;
            }
        }
        if (strcmp(e->v.sval, "DIFFER") == 0) {  /* SN-19 */
            if (nargs == 1) {
                return IS_NULL_fn(args[0]) ? FAILDESCR : NULVCL;
            }
            if (nargs >= 2) {
                int a_null = IS_NULL_fn(args[0]), b_null = IS_NULL_fn(args[1]);
                if (a_null && b_null) return FAILDESCR;  /* both null → identical → DIFFER fails */
                if (a_null || b_null) return NULVCL;     /* one null, one not → differ */
                if (args[0].v != args[1].v) return NULVCL;
                const char *sa = VARVAL_fn(args[0]);
                const char *sb = VARVAL_fn(args[1]);
                if (!sa) sa = ""; if (!sb) sb = "";
                return strcmp(sa, sb) != 0 ? NULVCL : FAILDESCR;
            }
        }

        /* SC-1: DATA constructor/field-accessor dispatch via our registry */
        {
            ScDatType *_dt = sc_dat_find_type(e->v.sval);
            if (_dt) return sc_dat_construct(_dt, args, nargs);
            int _fi = 0;
            ScDatType *_ft = sc_dat_find_field(e->v.sval, &_fi);
            if (_ft && nargs >= 1) return sc_dat_field_get(e->v.sval, args[0]);
        }
        /* No body label → builtin or unknown. APPLY_fn handles both. */
        if (FNCEX_fn(e->v.sval)) {
            DESCR_t bres = APPLY_fn(e->v.sval, args, nargs);
            return bres;
        }
        return APPLY_fn(e->v.sval, args, nargs);
    }

    case TT_IDX: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t base = interp_eval(e->c[0]);
        if (IS_FAIL_fn(base)) return FAILDESCR;
        if (e->n == 2) {
            DESCR_t idx = interp_eval(e->c[1]);
            if (IS_FAIL_fn(idx)) return FAILDESCR;
            return subscript_get(base, idx);
        }
        DESCR_t i1 = interp_eval(e->c[1]);
        DESCR_t i2 = interp_eval(e->c[2]);
        if (IS_FAIL_fn(i1) || IS_FAIL_fn(i2)) return FAILDESCR;
        return subscript_get2(base, i1, i2);
    }

    case TT_DEFER: {
        /* *expr — SIL *X operator. Three sub-cases:
         *
         * *var  (TT_VAR): deferred variable reference — fetch value NOW.
         *   "term = *factor" stores factor's current pattern.
         *
         * *func(args) (TT_FNC): always builds a deferred T_FUNC/XATP pattern
         *   node via pat_user_call — fires the function at match time.
         *   This applies in ALL contexts (RHS assignment, pattern expr, etc.).
         *   "addop = ANY('+-') . *Push()" — *Push() must be a pattern node.
         *
         * *complex_expr: freeze as DT_E for EVAL() to thaw later. */
        if (e->n < 1) return NULVCL;
        tree_t *child = e->c[0];
        /* RUNTIME-6: *X in value context ALWAYS produces DT_E (EXPRESSION).
         * The child tree_t* is frozen; EVAL() thaws and executes it.
         * interp_eval_pat handles the pattern-context path (*var, *func). */
        DESCR_t d;
        d.v    = DT_E;
        d.slen = 0;
        d.s    = NULL;    /* clear union first... */
        d.ptr  = child;   /* ...then store ptr last (ptr and s share union) */
        return d;
    }

    case TT_NOT: {
        /* \X — o$nta/b/c: succeed (null) iff X fails; fail if X succeeds.
         * Expression-context version; pattern-context uses bb_not. */
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return NULVCL;
        return FAILDESCR;
    }

    case TT_SIZE: {
        /* *E — size of string, list, or table.
         * String: number of characters.  List/table (SOH-delimited): element count.
         * DT_T: native table → tbl->size. */
        if (e->n < 1) return INTVAL(0);
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_T) return INTVAL(v.tbl ? v.tbl->size : 0);
        /* IC-5: DT_DATA icnlist */
        if (v.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(v,"icn_type");
            if (tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)
                return INTVAL((int)FIELD_GET_fn(v,"frame_size").i);
        }
        if (IS_INT_fn(v)) return INTVAL(0);   /* integer has no size */
        if (IS_REAL_fn(v)) return INTVAL(0);
        /* String: count chars, or SOH-delimited elements for arrays */
        const char *s = VARVAL_fn(v);
        if (!s) return INTVAL(0);
        /* If string contains SOH (\x01) it is a Raku/Icon array — count elements */
        if (strchr(s, '\x01')) {
            long n = 1;
            for (const char *p = s; *p; p++) if (*p == '\x01') n++;
            return INTVAL(n);
        }
        long len = v.slen > 0 ? v.slen : (long)strlen(s);
        return INTVAL(len);
    }

    case TT_ALT: {
        /* child[0] | child[1] | ... — build pat_alt chain left-to-right.
         * Pattern alternation is inherently a pattern operation, so every
         * child should be evaluated in pattern context so that *var/*func
         * children become XDSAR/XATP nodes rather than frozen DT_E (which
         * silently coerces to pat_lit of the NAME string — wrong and
         * confusing). */
        if (e->n == 0) return NULVCL;
        DESCR_t acc = interp_eval_pat(e->c[0]);
        for (int i = 1; i < e->n; i++)
            acc = pat_alt(acc, interp_eval_pat(e->c[i]));
        return acc;
    }
    case TT_VLIST: {
        /* Goal-directed value-context disjunction.  Try children
         * left-to-right; return first non-failing value; fail if all fail.
         * SPITBOL `(a, b, c)` paren-list and Snocone `||`.
         * Distinct from TT_ALT (pattern alt is lazy/backtracking).
         * Side effects of arm k happen iff arms 0..k-1 all failed. */
        if (e->n == 0) return FAILDESCR;
        for (int i = 0; i < e->n; i++) {
            DESCR_t v = interp_eval(e->c[i]);
            if (!IS_FAIL_fn(v)) return v;
        }
        return FAILDESCR;
    }
    case TT_CAPT_COND_ASGN: {
        /* pat . target — conditional assignment on match success.
         * target may be:
         *   TT_VAR "name"         → XNME node, assign to named var at flush
         *   TT_DEFER(TT_FNC(...))  → XCALLCAP node: call func at match time to
         *                          get DT_N lvalue, write matched text at flush.
         *                          Function must NOT be called at build time. */
        if (e->n < 2) return NULVCL;
        DESCR_t pat = interp_eval_pat(e->c[0]);
        tree_t *tgt = e->c[1];
        if (tgt->t == TT_DEFER && tgt->n == 1
                && tgt->c[0]->t == TT_FNC && tgt->c[0]->v.sval) {
            /* Deferred-function target — build XCALLCAP, don't call now */
            tree_t *fnc = tgt->c[0];
            int na = fnc->n;
            /* TL-2: when every arg is a plain TT_VAR, store *names* and defer
             * lookup to flush time (NAME_commit).  This matches oracle semantics
             * where args are resolved AFTER earlier . captures in the same
             * pattern have written their variables. */
            int all_vars = (na > 0);
            for (int i = 0; i < na; i++) {
                tree_t *c = fnc->c[i];
                if (!c || c->t != TT_VAR || !c->v.sval) { all_vars = 0; break; }
            }
            if (all_vars) {
                char **names = (char **)GC_malloc(na * sizeof(char *));
                for (int i = 0; i < na; i++) names[i] = (char *)fnc->c[i]->v.sval;
                return pat_assign_callcap_named(pat, fnc->v.sval, NULL, 0, names, na);
            }
            /* SN-26c-parseerr-c: defer TT_FNC sub-args via DT_E wrapping;
             * thaw at match time in name_commit_value(NM_CALL).
             * SN-26c-parseerr-d: also defer TT_VAR — when args are mixed
             * (e.g. TT_QLIT + TT_VAR) the all_vars fast path above doesn't
             * fire, and an TT_VAR set by an earlier capture in the same
             * pattern would otherwise be eagerly read here at build time.
             * SN-26-bridge-coverage-t: also defer compound expressions —
             * e.g. `*Reduce('[]', nTop() + 1)` — where the arg's top kind
             * is TT_ADD/TT_SUB/etc but contains an TT_FNC inside.  Eager
             * interp_eval at build time would call nTop() before the
             * pattern matches, violating SPITBOL's deferred-pattern
             * semantics.  Wrap all args as DT_E; thaw at match time
             * via EVAL_fn → EXPVAL_fn (name_t.c:97). */
            DESCR_t *av = na > 0 ? GC_malloc(na * sizeof(DESCR_t)) : NULL;
            for (int i = 0; i < na; i++) {
                tree_t *arg = fnc->c[i];
                if (arg && arg->t == TT_QLIT) {
                    /* Pure string literal — safe to evaluate eagerly,
                     * idempotent under EVAL.  Avoids needless DT_E wrap. */
                    av[i] = interp_eval(arg);
                } else if (arg) {
                    /* Any other expression kind — defer.  EXPVAL_fn at
                     * thaw time handles all tree_t shapes. */
                    av[i].v = DT_E;
                    av[i].ptr = arg;
                    av[i].slen = 0;
                } else {
                    av[i] = NULVCL;
                }
            }
            return pat_assign_callcap(pat, fnc->v.sval, av, na);
        }
        /* Snocone *fn() lowers as TT_INDIRECT(TT_FNC(...)) — same semantics as
         * TT_DEFER(TT_FNC(...)): call fn at flush time to get lvalue, assign then.
         * SC-26: route this through pat_assign_callcap so it joins the unified
         * NAM list and fires in left-to-right order after preceding captures. */
        if (tgt->t == TT_INDIRECT && tgt->n == 1
                && tgt->c[0]->t == TT_FNC && tgt->c[0]->v.sval) {
            tree_t *fnc = tgt->c[0];
            int na = fnc->n;
            int all_vars = (na > 0);
            for (int i = 0; i < na; i++) {
                tree_t *c = fnc->c[i];
                if (!c || c->t != TT_VAR || !c->v.sval) { all_vars = 0; break; }
            }
            if (all_vars) {
                char **names = (char **)GC_malloc(na * sizeof(char *));
                for (int i = 0; i < na; i++) names[i] = (char *)fnc->c[i]->v.sval;
                return pat_assign_callcap_named(pat, fnc->v.sval, NULL, 0, names, na);
            }
            /* SN-26c-parseerr-c: defer TT_FNC sub-args.
             * SN-26c-parseerr-d: also defer TT_VAR (see twin site above).
             * SN-26-bridge-coverage-t: defer compound expressions too —
             * see twin site above for full rationale. */
            DESCR_t *av = na > 0 ? GC_malloc(na * sizeof(DESCR_t)) : NULL;
            for (int i = 0; i < na; i++) {
                tree_t *arg = fnc->c[i];
                if (arg && arg->t == TT_QLIT) {
                    av[i] = interp_eval(arg);
                } else if (arg) {
                    av[i].v = DT_E;
                    av[i].ptr = arg;
                    av[i].slen = 0;
                } else {
                    av[i] = NULVCL;
                }
            }
            return pat_assign_callcap(pat, fnc->v.sval, av, na);
        }
        const char *nm = tgt->v.sval;
        if (!nm && tgt->t == TT_INDIRECT && tgt->n > 0) {
            /* REM . $'$B' — target is TT_INDIRECT(TT_QLIT "$B").
             * We need the *variable name* ("$B"), not the value of $'$B'.
             * The name is the evaluated string of the child expression. */
            tree_t *ichild = tgt->c[0];
            if (ichild->t == TT_QLIT || ichild->t == TT_VAR)
                nm = ichild->v.sval;                     /* literal name: $'$B' or $.X */
            else {
                DESCR_t nd = interp_eval(ichild);      /* runtime indirect: $X */
                nm = VARVAL_fn(nd);
            }
        }
        return nm ? pat_assign_cond(pat, STRVAL((char *)nm)) : pat;
    }
    case TT_CAPT_IMMED_ASGN: {
        /* pat $ target — immediate assignment during match */
        if (e->n < 2) return NULVCL;
        DESCR_t pat = interp_eval_pat(e->c[0]);
        tree_t *tgt = e->c[1];
        if (tgt->t == TT_DEFER && tgt->n == 1
                && tgt->c[0]->t == TT_FNC && tgt->c[0]->v.sval) {
            /* SN-26c-parseerr-f: "pat $ *fn(args)" — deferred-call immediate capture.
             * Mirror TT_CAPT_COND_ASGN logic: build XCALLCAP with imm=1 so the
             * function is called at match time (not build time) with current arg
             * values.  This is critical when args are set by prior $ captures in
             * the same pattern (e.g. beauty's "SPAN $ tx $ *match(list, pattern)").
             * Calling fn eagerly at build time reads empty vars → fn returns fail
             * → match guard silently vanishes. */
            tree_t *fnc = tgt->c[0];
            int na = fnc->n;
            int all_vars = (na > 0);
            for (int i = 0; i < na; i++) {
                tree_t *c = fnc->c[i];
                if (!c || c->t != TT_VAR || !c->v.sval) { all_vars = 0; break; }
            }
            if (all_vars) {
                char **names = (char **)GC_malloc(na * sizeof(char *));
                for (int i = 0; i < na; i++) names[i] = (char *)fnc->c[i]->v.sval;
                return pat_assign_callcap_named_imm(pat, fnc->v.sval, NULL, 0, names, na);
            }
            /* Mixed args: defer TT_VAR and TT_FNC; evaluate literals eagerly. */
            DESCR_t *av = na > 0 ? GC_malloc(na * sizeof(DESCR_t)) : NULL;
            for (int i = 0; i < na; i++) {
                tree_t *arg = fnc->c[i];
                if (arg && (arg->t == TT_FNC || arg->t == TT_VAR)) {
                    av[i].v = DT_E;
                    av[i].ptr = arg;
                    av[i].slen = 0;
                } else {
                    av[i] = interp_eval(arg);
                }
            }
            return pat_assign_callcap_named_imm(pat, fnc->v.sval, av, na, NULL, 0);
        }
        tree_t *tgt2 = e->c[1];
        const char *nm = tgt2->v.sval;
        if (!nm && tgt2->t == TT_INDIRECT && tgt2->n > 0) {
            tree_t *ichild = tgt2->c[0];
            if (ichild->t == TT_QLIT || ichild->t == TT_VAR)
                nm = ichild->v.sval;
            else { DESCR_t nd = interp_eval(ichild); nm = VARVAL_fn(nd); }
        }
        return nm ? pat_assign_imm(pat, STRVAL((char *)nm)) : pat;
    }
    case TT_CAPT_CURSOR: {
        /* Two forms:
         *   unary:  @var         — TT_CAPT_CURSOR(TT_VAR)         nchildren==1
         *   binary: pat @ var    — TT_CAPT_CURSOR(pat, TT_VAR)    nchildren==2
         * Both write the cursor position into var as DT_I at match time. */
        if (e->n == 1) {
            /* unary @var: epsilon left, cursor capture into var */
            const char *nm = e->c[0]->v.sval;
            if (!nm) return NULVCL;
            return pat_at_cursor(nm);
        }
        if (e->n < 2) return NULVCL;
        DESCR_t left_pat = interp_eval_pat(e->c[0]);
        const char *nm   = e->c[1]->v.sval;
        if (!nm) return left_pat;
        DESCR_t atp = pat_at_cursor(nm);
        return pat_cat(left_pat, atp);
    }

    /* ── Numeric relational operators ─────────────────────────────────────
     * Each compares two numeric operands; succeeds (returns rhs) or fails.
     * Icon relops return the RIGHT operand on success — this lets them act
     * as filters in generator chains: every write(2 < (1 to 4)) → 3, 4.
     * (SNOBOL4 uses a separate path; these cases are Icon-only.) */
#define NUMREL(op) do { \
        if (e->n < 2) return FAILDESCR; \
        DESCR_t l = interp_eval(e->c[0]); \
        DESCR_t r = interp_eval(e->c[1]); \
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR; \
        double lv = (l.v==DT_R) ? l.r : (double)(l.v==DT_I ? l.i : 0); \
        double rv = (r.v==DT_R) ? r.r : (double)(r.v==DT_I ? r.i : 0); \
        if (!(lv op rv)) return FAILDESCR; \
        return r; \
    } while(0)
    case TT_LT: NUMREL(<);
    case TT_LE: NUMREL(<=);
    case TT_GT: NUMREL(>);
    case TT_GE: NUMREL(>=);
    case TT_EQ: NUMREL(==);
    case TT_NE: NUMREL(!=);
#undef NUMREL

    /* ── Lexicographic (string) relational operators ───────────────────────
     * Each compares two string operands; succeeds (returns rhs) or fails.
     * Icon string relops return the RIGHT operand on success — oracle:
     * ocomp.r StrComp macro: "Return y as the result of the comparison." */
#define STRREL(cmpop) do { \
        if (e->n < 2) return FAILDESCR; \
        DESCR_t l = interp_eval(e->c[0]); \
        DESCR_t r = interp_eval(e->c[1]); \
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR; \
        const char *ls = VARVAL_fn(l); if (!ls) ls = ""; \
        const char *rs = VARVAL_fn(r); if (!rs) rs = ""; \
        int cmp = strcmp(ls, rs); \
        if (!(cmp cmpop 0)) return FAILDESCR; \
        return r; \
    } while(0)
    case TT_LLT: STRREL(<);
    case TT_LLE: STRREL(<=);
    case TT_LGT: STRREL(>);
    case TT_LGE: STRREL(>=);
    case TT_LEQ: STRREL(==);
    case TT_LNE: STRREL(!=);
#undef STRREL

    /* ── IC-8: Icon `===` deep-identity (non-generator path) ─────────────────
     * The icon-frame TT_IF at L2607 routes to coro_eval when is_suspendable(test)
     * is true (e.g. `if x === key(T)` where key(T) is a generator). The path
     * here handles plain `a === b` and `&null === x` evaluation in any context.
     * Returns rhs on identity (Icon goal-directed convention), FAILDESCR else. */
    case TT_IDENTICAL: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return icn_descr_identical(l, r) ? r : FAILDESCR;
    }

    /* ── Prolog IR nodes — S-1C-2/3 ──────────────────────────────────────────
     * Only reached when g_pl_active is set (Prolog program running).
     * TT_CHOICE drives clause selection via the Byrd box broker (pl_broker.c).
     * TT_UNIFY/TT_CUT/TT_TRAIL_* are leaf goal nodes evaluated inline. */
    case TT_UNIFY: {
        if (!g_pl_active) return NULVCL;
        Term *t1 = pl_unified_term_from_expr(e->c[0], g_pl_env);
        Term *t2 = pl_unified_term_from_expr(e->c[1], g_pl_env);
        int mark = trail_mark(&g_pl_trail);
        if (!unify(t1, t2, &g_pl_trail)) { trail_unwind(&g_pl_trail, mark); return FAILDESCR; }
        return INTVAL(1);
    }
    case TT_CUT:
        if (g_pl_active) g_pl_cut_flag = 1;
        return INTVAL(1);
    case TT_TRAIL_MARK:
    case TT_TRAIL_UNWIND:
        return NULVCL;
    case TT_CLAUSE:
        /* Clauses are iterated by TT_CHOICE — never dispatched standalone. */
        return NULVCL;
    case TT_CHOICE: {
        /* Drive clause selection via the Byrd box broker.
         * CH-17e: when entry_pc >= 0 (SM expression lowered by CH-17d/f), use
         * pl_box_choice_pc; otherwise fall back to IR-walk pl_box_choice. */
        if (!g_pl_active) return NULVCL;
        int arity = 0;
        if (e->v.sval) { const char *sl = strrchr(e->v.sval, '/'); if (sl) arity = atoi(sl+1); }
        Pl_PredEntry *_pe2 = e->v.sval ? pl_pred_entry_lookup(e->v.sval) : NULL;
        extern SM_Program *g_current_sm_prog;
        bb_node_t root = (_pe2 && _pe2->entry_pc >= 0 && g_current_sm_prog != NULL)
            ? pl_box_choice_pc(_pe2->entry_pc, g_pl_env, arity)
            : pl_box_choice(e, g_pl_env, arity);
        int ok = bb_broker(root, BB_ONCE, NULL, NULL);
        return ok ? INTVAL(1) : FAILDESCR;
    }

    /* ── Icon EKinds — OE-3 ─── */

    case TT_CSET: return e->v.sval ? STRVAL(icn_cset_canonical(e->v.sval)) : NULVCL;

    case TT_TO: case TT_TO_BY: {
        long cur;
        if (icn_frame_lookup(e, &cur)) return INTVAL(cur);
        if (e->n < 1) return NULVCL;
        return interp_eval(e->c[0]);
    }

    case TT_EVERY: {
        if (e->n < 1) return NULVCL;
        tree_t *gen  = e->c[0];
        tree_t *body = (e->n > 1) ? e->c[1] : NULL;
        /* IC-2a: coro_eval + BB_PUMP — all goal-directed ops through Byrd boxes.
         * Special case: if gen is TT_ASSIGN or TT_AUGOP with a generative RHS,
         * drive the LEAF generator inside the RHS and re-evaluate gen each tick so
         * that mutable frame locals (e.g. `total`) are read fresh.  e.g.:
         *   every total := total + (1 to n)   -- TT_ASSIGN(TT_VAR(total), TT_ADD(TT_VAR(total), TT_TO(1,n)))
         *   every total +:= (1 to n)           -- TT_AUGOP with TT_TO rhs
         * We find the leaf generator node (e.g. TT_TO), drive only that via coro_eval,
         * and inject each raw tick value via coro_drive_node passthrough.  interp_eval(gen)
         * then re-reads the current value of `total` from the frame slot each iteration. */
        if ((gen->t == TT_ASSIGN) &&
            gen->n >= 2 && is_suspendable(gen->c[1])) {
            tree_t *leaf = find_leaf_suspendable(gen->c[1]);
            if (!leaf) leaf = gen->c[1];   /* fallback: treat whole RHS as gen */
            bb_node_t rbox = coro_eval(leaf);
            DESCR_t tick = rbox.fn(rbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.returning && !FRAME.loop_break) {
                /* Inject the raw generator tick value via drive passthrough,
                 * then re-evaluate gen (the assign/augop) — interp_eval re-reads
                 * the current frame value of any TT_VAR in the expression. */
                coro_drive_node = leaf;
                coro_drive_val  = tick;
                interp_eval(gen);
                coro_drive_node = NULL;
                if (body) interp_eval(body);
                if (FRAME.returning || FRAME.loop_break) break;
                tick = rbox.fn(rbox.ζ, β);
            }
            FRAME.loop_break = 0;
            return NULVCL;
        }
        tree_t *do_expr = body ? body : gen;
        bb_node_t box = coro_eval(gen);
        DESCR_t val = box.fn(box.ζ, α);
        while (!IS_FAIL_fn(val) && !FRAME.returning && !FRAME.loop_break) {
            frame_push(gen, val.v == DT_I ? val.i : 0, val.v == DT_I ? NULL : val.s);
            interp_eval(do_expr);
            frame_pop();
            if (FRAME.returning || FRAME.loop_break) break;
            val = box.fn(box.ζ, β);
        }
        FRAME.loop_break = 0;
        return NULVCL;
    }

    case TT_WHILE: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending &&
               !IS_FAIL_fn(interp_eval(e->c[0]))) {
            if (e->n > 1) interp_eval(e->c[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        return NULVCL;
    }

    case TT_UNTIL: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            DESCR_t cv = (e->n > 0) ? interp_eval(e->c[0]) : FAILDESCR;
            if (!IS_FAIL_fn(cv)) break;
            if (e->n > 1) interp_eval(e->c[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        return NULVCL;
    }

    case TT_REPEAT: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending)
            if (e->n > 0) { interp_eval(e->c[0]); if (FRAME.suspending) break; }
        FRAME.loop_break = saved_brk;
        return NULVCL;
    }

    case TT_SEQ_EXPR: {
        DESCR_t v = NULVCL;
        for (int i = 0; i < e->n && !FRAME.returning; i++)
            v = interp_eval(e->c[i]);
        return v;
    }

    case TT_IF: {
        if (e->n < 1) return NULVCL;
        DESCR_t cv = interp_eval(e->c[0]);
        if (!IS_FAIL_fn(cv))
            return (e->n > 1) ? interp_eval(e->c[1]) : cv;
        return (e->n > 2) ? interp_eval(e->c[2]) : FAILDESCR;
    }

    case TT_CASE: {
        /* Two layouts:
         *   Icon:  child[0]=topic, then pairs [val, body]..., optional trailing default_body
         *   Raku:  child[0]=topic, then triples [cmpnode(TT_ILIT|TT_NUL), val, body]
         * Detect by child[1]: if it's TT_ILIT or TT_NUL it's a Raku cmpnode (triples).
         * Icon case values are always full expressions — never bare TT_ILIT/TT_NUL. */
        if (e->n < 1) return NULVCL;
        DESCR_t topic = interp_eval(e->c[0]);
        /* Detect layout by child count:
         *   Raku triples: nchildren = 1 + 3*N  (topic + N×[cmpnode,val,body])  → (nchildren-1) % 3 == 0
         *   Icon pairs:   nchildren = 1 + 2*N  or  1 + 2*N + 1 (with default)  → (nchildren-1) % 2 == 0 or 1
         * Additionally verify child[1] is TT_ILIT or TT_NUL for Raku (cmpnode marker). */
        int is_raku_layout = (e->n >= 4 && (e->n - 1) % 3 == 0 &&
            e->c[1] && (e->c[1]->t == TT_ILIT || e->c[1]->t == TT_NUL));
        if (is_raku_layout) {
            /* Raku: triples [cmpnode(TT_ILIT), val, body] */
            int i = 1;
            while (i + 2 < e->n) {
                tree_t *cmpnode = e->c[i];
                tree_t *val     = e->c[i+1];
                tree_t *body    = e->c[i+2];
                i += 3;
                if (cmpnode->t == TT_NUL) return interp_eval(body);
                tree_e cmp = (tree_e)(cmpnode->v.ival);
                DESCR_t wval = interp_eval(val);
                int match = 0;
                if (cmp == TT_LEQ) {
                    const char *ts = IS_STR_fn(topic)?topic.s:VARVAL_fn(topic);
                    const char *ws = IS_STR_fn(wval)?wval.s:VARVAL_fn(wval);
                    match = (ts && ws && strcmp(ts,ws)==0);
                } else {
                    if (IS_INT_fn(topic) && IS_INT_fn(wval)) match = (topic.i == wval.i);
                    else { const char *ts=VARVAL_fn(topic),*ws=VARVAL_fn(wval); match=(ts&&ws&&strcmp(ts,ws)==0); }
                }
                if (match) return interp_eval(body);
            }
            if (i+1 < e->n && e->c[i]->t==TT_NUL)
                return interp_eval(e->c[i+1]);
            return NULVCL;
        }
        /* Icon: pairs [val, body] then optional trailing default body */
        int nc = e->n;
        int i = 1;
        while (i + 1 < nc) {
            DESCR_t wval = interp_eval(e->c[i]);
            tree_t *body = e->c[i+1];
            i += 2;
            int match;
            if (IS_INT_fn(topic) && IS_INT_fn(wval)) match = (topic.i == wval.i);
            else {
                const char *ts = VARVAL_fn(topic), *ws = VARVAL_fn(wval);
                match = (ts && ws && strcmp(ts, ws) == 0);
            }
            if (match) return interp_eval(body);
        }
        /* trailing default body (odd child count) */
        if (i < nc) return interp_eval(e->c[i]);
        return NULVCL;
    }

    case TT_NULL: {
        /* /E — succeeds (yields &null) iff E succeeds with the null value;
         * fails if E fails OR if E yields any non-null value.
         * Pre-IC-9 this was wrong: it returned NULVCL iff IS_FAIL_fn(v),
         * conflating "E failed" with "E was null".  /x[k] for a missing
         * table key (returns NULVCL, which is success-with-null) reported
         * fail; /x[k] for a present non-null value also reported fail; the
         * two contradictory bugs cancelled in some tests, but rung36_jcon_*
         * exposed them via probes like `/x[1] | write("/1")`. */
        if (e->n < 1) return NULVCL;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return NULVCL;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return NULVCL;
        return FAILDESCR;
    }

    case TT_NONNULL: {
        /* \E — succeeds (yields E) iff E succeeds with a non-null value;
         * fails if E fails OR if E yields the null value.  Mirror of
         * TT_NULL above.  Pre-IC-9 this missed the DT_SNUL case entirely
         * — \&null returned &null (success) instead of failing. */
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return FAILDESCR;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return FAILDESCR;
        return v;
    }

    case TT_RANDOM: {
        /* ?E — Icon random selector.  IC-9 (session 2026-04-30 #20):
         *   ?n  (integer)  → random integer in [1,n]; fails if n ≤ 0
         *   ?s  (string)   → random character of s; fails if s is empty
         *   ?&null         → fails (treated as empty string)
         *   ?T  (table)    → random entry value; fails if T is empty
         *   ?L  (list)     → random element; fails if L is empty
         * Pre-fix this was unhandled (default→NULVCL), so ?T_empty returned
         * &null rather than failing — surfaced in rung36_jcon_table line
         * `should fail &null` and rung36_jcon_evalx `?table() ----> &null`.
         *
         * RNG: shared canonical bb_icn_rnd_seed (defined in coro_value.c)
         * — GOAL-ICON-BB-COMPLETE A3-seed-fix unifies ir-run / sm-run /
         * interp_eval fallback RNG state so all three advance one sequence. */
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
        unsigned long _rnd = bb_icn_rnd_seed >> 33;

        /* DT_T table — pick random entry value, fail if empty */
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
            return FAILDESCR;  /* unreachable when size matches reality */
        }
        /* DT_DATA icnlist — pick random element, fail if empty */
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
            /* IC-9 (2026-05-01): DT_DATA record — pick random field value, fail if no fields */
            if (v.u && v.u->type && v.u->type->nfields > 0 && v.u->fields) {
                int n = v.u->type->nfields;
                return v.u->fields[_rnd % (unsigned long)n];
            }
            return FAILDESCR;
        }
        /* Integer — random in [1,n]; fail if n ≤ 0 */
        if (IS_INT_fn(v)) {
            long long n = v.i;
            if (n <= 0) return FAILDESCR;
            return INTVAL((long long)((_rnd % (unsigned long)n) + 1));
        }
        /* &null — fail */
        if (v.v == DT_SNUL) return FAILDESCR;
        /* String — random character, fail if empty */
        const char *s = VARVAL_fn(v);
        if (!s || !*s) return FAILDESCR;
        long slen = v.slen > 0 ? v.slen : (long)strlen(s);
        if (slen <= 0) return FAILDESCR;
        char *out = (char *)GC_malloc(2);
        out[0] = s[_rnd % (unsigned long)slen];
        out[1] = '\0';
        return STRVAL(out);
    }

    case TT_AUGOP: {
        if (e->n < 2) return NULVCL;
        tree_t *lhs = e->c[0];
        tree_t *rhs = e->c[1];
        /* Helper lambda: apply augop to (lv, rv), write back to lhs slot, return result */
        #define AUGOP_APPLY(lv_, rv_) do { \
            DESCR_t _lv = (lv_), _rv = (rv_); \
            if (IS_FAIL_fn(_lv)||IS_FAIL_fn(_rv)) break; \
            long _li = IS_INT_fn(_lv)?_lv.i:(long)_lv.r; \
            long _ri = IS_INT_fn(_rv)?_rv.i:(long)_rv.r; \
            DESCR_t _res = NULVCL; \
            switch((IcnTkKind)e->v.ival){ \
                case TK_AUGPLUS:   _res=INTVAL(_li+_ri); break; \
                case TK_AUGMINUS:  _res=INTVAL(_li-_ri); break; \
                case TK_AUGSTAR:   _res=INTVAL(_li*_ri); break; \
                case TK_AUGSLASH:  _res=_ri?INTVAL(_li/_ri):FAILDESCR; break; \
                case TK_AUGMOD:    _res=_ri?INTVAL(_li%_ri):FAILDESCR; break; \
                case TK_AUGCONCAT: { \
                    const char *_ls=VARVAL_fn(_lv),*_rs=VARVAL_fn(_rv); \
                    if(!_ls)_ls="";if(!_rs)_rs=""; \
                    size_t _ll=strlen(_ls),_rl=strlen(_rs); \
                    char *_buf=GC_malloc(_ll+_rl+1); \
                    memcpy(_buf,_ls,_ll);memcpy(_buf+_ll,_rs,_rl);_buf[_ll+_rl]='\0'; \
                    _res=STRVAL(_buf); break; \
                } \
                /* IC-9: comparison-augops — `lv OP:= rv` evaluates `lv OP rv`; \
                 * on success the result is rv (which is then written back to lhs), \
                 * on failure the augop fails (alternation `| "none"` then runs). */ \
                case TK_AUGEQ:   _res = (_li == _ri) ? _rv : FAILDESCR; break; \
                case TK_AUGNE:   _res = (_li != _ri) ? _rv : FAILDESCR; break; \
                case TK_AUGLT:   _res = (_li <  _ri) ? _rv : FAILDESCR; break; \
                case TK_AUGLE:   _res = (_li <= _ri) ? _rv : FAILDESCR; break; \
                case TK_AUGGT:   _res = (_li >  _ri) ? _rv : FAILDESCR; break; \
                case TK_AUGGE:   _res = (_li >= _ri) ? _rv : FAILDESCR; break; \
                case TK_AUGSEQ: case TK_AUGSNE: \
                case TK_AUGSLT: case TK_AUGSLE: case TK_AUGSGT: case TK_AUGSGE: { \
                    const char *_lcs=VARVAL_fn(_lv),*_rcs=VARVAL_fn(_rv); \
                    if(!_lcs)_lcs="";if(!_rcs)_rcs=""; \
                    int _cmp = strcmp(_lcs, _rcs); int _ok = 0; \
                    switch ((IcnTkKind)e->v.ival) { \
                        case TK_AUGSEQ: _ok = (_cmp == 0); break; \
                        case TK_AUGSNE: _ok = (_cmp != 0); break; \
                        case TK_AUGSLT: _ok = (_cmp <  0); break; \
                        case TK_AUGSLE: _ok = (_cmp <= 0); break; \
                        case TK_AUGSGT: _ok = (_cmp >  0); break; \
                        case TK_AUGSGE: _ok = (_cmp >= 0); break; \
                        default: break; \
                    } \
                    _res = _ok ? _rv : FAILDESCR; break; \
                } \
                default: _res=INTVAL(_li+_ri); break; \
            } \
            if (!IS_FAIL_fn(_res) && lhs->t == TT_VAR) { \
                int _slot=(int)lhs->v.ival; \
                if (frame_depth > 0 && _slot>=0 && _slot<FRAME.env_n) \
                    FRAME.env[_slot]=_res; \
                else if (_slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') \
                    set_and_trace(lhs->v.sval, _res); \
            } else if (!IS_FAIL_fn(_res) && lhs->t == TT_IDX && lhs->n >= 2) { \
                DESCR_t _base = interp_eval(lhs->c[0]); \
                DESCR_t _idx  = interp_eval(lhs->c[1]); \
                if (!IS_FAIL_fn(_base) && !IS_FAIL_fn(_idx)) subscript_set(_base, _idx, _res); \
            } else if (!IS_FAIL_fn(_res) && lhs->t == TT_FIELD && lhs->v.sval && lhs->n >= 1) { \
                DESCR_t _obj = interp_eval(lhs->c[0]); \
                if (!IS_FAIL_fn(_obj)) { DESCR_t *_cell = data_field_ptr(lhs->v.sval, _obj); if (_cell) *_cell = _res; } \
            } \
            _augop_result = _res; \
        } while(0)

        /* If RHS is a generator: apply augop once per tick, re-reading lhs each time.
         * This implements  every sum +:= (1 to 5)  →  sum=1,3,6,10,15
         * and  every result ||:= !s  →  result="x","xy"  */
        DESCR_t _augop_result = NULVCL;
        /* IC-9: !container OP:= rhs  — bang-iterate as augmented-assign lvalue.
         * Walks every cell of the container, applies the augop in place, in one pass.
         * Mirrors the TT_ITERATE LHS branch in TT_ASSIGN.  rhs evaluated once.
         *   !T +:= v  (table) — buckets walk, modify TBPAIR_t::val
         *   !L +:= v  (list)  — array walk, modify frame_elems[i]
         */
        if (lhs && lhs->t == TT_ITERATE && lhs->n >= 1) {
            DESCR_t cv = interp_eval(lhs->c[0]);
            DESCR_t rv = interp_eval(rhs);
            if (!IS_FAIL_fn(cv) && !IS_FAIL_fn(rv)) {
                #define AUGOP_CELL(cell_) do { \
                    DESCR_t _lv = (cell_); \
                    if (IS_FAIL_fn(_lv)) break; \
                    long _li = IS_INT_fn(_lv)?_lv.i:(long)_lv.r; \
                    long _ri = IS_INT_fn(rv)?rv.i:(long)rv.r; \
                    DESCR_t _res = NULVCL; \
                    switch((IcnTkKind)e->v.ival){ \
                        case TK_AUGPLUS:   _res=INTVAL(_li+_ri); break; \
                        case TK_AUGMINUS:  _res=INTVAL(_li-_ri); break; \
                        case TK_AUGSTAR:   _res=INTVAL(_li*_ri); break; \
                        case TK_AUGSLASH:  _res=_ri?INTVAL(_li/_ri):FAILDESCR; break; \
                        case TK_AUGMOD:    _res=_ri?INTVAL(_li%_ri):FAILDESCR; break; \
                        case TK_AUGCONCAT: { \
                            const char *_ls=VARVAL_fn(_lv),*_rs=VARVAL_fn(rv); \
                            if(!_ls)_ls="";if(!_rs)_rs=""; \
                            size_t _ll=strlen(_ls),_rl=strlen(_rs); \
                            char *_buf=GC_malloc(_ll+_rl+1); \
                            memcpy(_buf,_ls,_ll);memcpy(_buf+_ll,_rs,_rl);_buf[_ll+_rl]='\0'; \
                            _res=STRVAL(_buf); break; \
                        } \
                        default: _res=INTVAL(_li+_ri); break; \
                    } \
                    if (!IS_FAIL_fn(_res)) { (cell_) = _res; _augop_result = _res; } \
                } while(0)
                if (cv.v == DT_T && cv.tbl) {
                    for (int b = 0; b < TABLE_BUCKETS; b++)
                        for (TBPAIR_t *p = cv.tbl->buckets[b]; p; p = p->next)
                            AUGOP_CELL(p->val);
                } else if (cv.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(cv, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        DESCR_t ea = FIELD_GET_fn(cv, "frame_elems");
                        int n = (int)FIELD_GET_fn(cv, "frame_size").i;
                        DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                        if (elems && n > 0) for (int i = 0; i < n; i++) AUGOP_CELL(elems[i]);
                    } else if (cv.u && cv.u->type && cv.u->type->nfields > 0 && cv.u->fields) {
                        /* IC-9 (2026-05-01): every !record OP:= V — apply OP to each field cell.
                         * Mirrors the table/list branches above; same AUGOP_CELL macro semantics. */
                        for (int i = 0; i < cv.u->type->nfields; i++) AUGOP_CELL(cv.u->fields[i]);
                    }
                }
                #undef AUGOP_CELL
            }
        } else if (rhs && is_suspendable(rhs)) {
            bb_node_t rbox = coro_eval(rhs);
            DESCR_t tick = rbox.fn(rbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.loop_break && !FRAME.returning) {
                DESCR_t cur_lv = interp_eval(lhs);   /* re-read lhs each tick */
                AUGOP_APPLY(cur_lv, tick);
                tick = rbox.fn(rbox.ζ, β);
            }
        } else {
            DESCR_t lv = interp_eval(lhs);
            DESCR_t rv = interp_eval(rhs);
            AUGOP_APPLY(lv, rv);
        }
        #undef AUGOP_APPLY
        return _augop_result;
    }

    case TT_LOOP_BREAK: {
        FRAME.loop_break = 1;
        return (e->n > 0) ? interp_eval(e->c[0]) : NULVCL;
    }

    case TT_SCAN: {
        if (e->n < 1) return FAILDESCR;
        /* ── SNOBOL4 context: `subj ? pat` as expression — run exec_stmt ──
         * When used inside ~(), EVAL(), or another expression in SNOBOL4 mode,
         * TT_SCAN must perform the actual pattern match (not just build a pattern
         * descriptor as the Icon path does).  Without this, ~(s ? pat) always
         * fails because interp_eval_pat(pat) returns a DT_P descriptor which is
         * non-fail, so TT_NOT sees success and returns FAILDESCR unconditionally.
         * Fix: evaluate subject as string, evaluate pattern in pat context,
         * run exec_stmt, return NULVCL on success or FAILDESCR on failure. */
        if (!frame_depth && !g_pl_active) {
            /* SNOBOL4 mode — perform the match */
            DESCR_t subj_d = interp_eval(e->c[0]);
            if (IS_FAIL_fn(subj_d)) return FAILDESCR;
            /* subject name for write-back */
            const char *sname = (e->c[0]->t == TT_VAR) ? e->c[0]->v.sval : NULL;
            DESCR_t pat_d = (e->n >= 2) ? interp_eval_pat(e->c[1]) : pat_epsilon();
            if (IS_FAIL_fn(pat_d)) return FAILDESCR;
            int ok = exec_stmt(sname, sname ? NULL : &subj_d, pat_d, NULL, 0);
            return ok ? NULVCL : FAILDESCR;
        }
        /* ── Icon / Prolog context: generator scan ── */
        DESCR_t subj_d = interp_eval(e->c[0]);
        if (IS_FAIL_fn(subj_d)) return FAILDESCR;
        const char *subj_s = VARVAL_fn(subj_d); if (!subj_s) subj_s = "";
        if (scan_depth < SCAN_STACK_MAX) {
            scan_stack[scan_depth].subj = scan_subj;
            scan_stack[scan_depth].pos  = scan_pos;
            scan_depth++;
        }
        scan_subj = subj_s; scan_pos = 1;
        DESCR_t r = (e->n >= 2) ? interp_eval(e->c[1]) : NULVCL;
        if (scan_depth > 0) {
            scan_depth--;
            scan_subj = scan_stack[scan_depth].subj;
            scan_pos  = scan_stack[scan_depth].pos;
        }
        return r;
    }

    case TT_ITERATE: {
        /* IC-3: DT_T table — return first value (oneshot; every uses coro_bb_tbl_iterate) */
        if (e->n >= 1) {
            DESCR_t sv = interp_eval(e->c[0]);
            if (sv.v == DT_T && sv.tbl) {
                for (int bi = 0; bi < TABLE_BUCKETS; bi++) {
                    if (sv.tbl->buckets[bi]) return sv.tbl->buckets[bi]->val;
                }
                return FAILDESCR;
            }
            /* IC-5: DT_DATA icnlist — !L returns first element (every drives the rest) */
            if (sv.v == DT_DATA) {
                DESCR_t tag = FIELD_GET_fn(sv,"icn_type");
                if (tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0) {
                    int n=(int)FIELD_GET_fn(sv,"frame_size").i;
                    DESCR_t ea=FIELD_GET_fn(sv,"frame_elems");
                    DESCR_t *elems=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
                    if(!elems||n<=0) return FAILDESCR;
                    return elems[0];
                }
                /* IC-9 (2026-05-01): DT_DATA record — !R returns first field value.
                 * Mirrors the icnlist branch above: scalar context yields child 0,
                 * every-context drives the rest via coro_bb_record_iterate.  See
                 * coro_eval TT_ITERATE for the box. */
                if (sv.u && sv.u->type && sv.u->type->nfields > 0 && sv.u->fields) {
                    return sv.u->fields[0];
                }
            }
        }
        long cur; const char *str;
        if (icn_frame_lookup_sv(e, &cur, &str) && str) {
            char *ch = GC_malloc(2); ch[0] = str[cur]; ch[1] = '\0';
            return STRVAL(ch);
        }
        return FAILDESCR;
    }

    case TT_SUSPEND: {
        /* Icon suspend: yield a value to the driving coro_drive TT_FNC loop.
         * Signal by setting FRAME.suspending=1; coro_drive sees the flag,
         * runs the every-body, executes the do-clause, clears the flag, and
         * re-enters the procedure body loop.  For non-generator (bare call)
         * contexts there is no driver, so we just return the value. */
        DESCR_t val = (e->n > 0) ? interp_eval(e->c[0]) : NULVCL;
        if (frame_depth > 0) {
            FRAME.suspending  = 1;
            FRAME.suspend_val = val;
            FRAME.suspend_do  = (e->n > 1) ? e->c[1] : NULL;
        }
        return val;
    }

    /* ── IC-5: TT_SWAP — x :=: y  (swap two lvalues) ─────────────────────
     * IC-9 (session #26): left-to-right halt-on-keyword-OOB semantics.
     * Swap writes rv → lhs first, then lv → rhs.  If a keyword side
     * OOB-fails, stop immediately — don't perform later writes.
     * Pre-fix: both writes always attempted, leaving state half-swapped
     * in shapes the JCON corpus's subjpos test specifically exercises.
     * (My first attempt at "atomic all-or-nothing" was wrong; tracing
     * subjpos.expected lines 57/58 shows x :=: &pos with `x:=9, &pos:=3`
     * → x=3 (lhs write committed) &pos=3 (rhs write OOB-aborted).)        */
    case TT_SWAP: {
        if (e->n < 2 || frame_depth <= 0) return NULVCL;
        tree_t *lhs = e->c[0], *rhs = e->c[1];
        DESCR_t lv = interp_eval(lhs), rv = interp_eval(rhs);
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
        /* Step 1: write rv → lhs.  Halt on keyword-OOB. */
        if (lhs && lhs->t == TT_VAR) {
            if (lhs->v.sval && lhs->v.sval[0] == '&') {
                if (!kw_assign(lhs->v.sval + 1, rv)) return FAILDESCR;
            } else {
                int sl=(int)lhs->v.ival;
                if (sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=rv;
                else if (sl<0&&lhs->v.sval) NV_SET_fn(lhs->v.sval,rv);
            }
        }
        /* Step 2: write lv → rhs. */
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

    /* ── IC-9 session #26: TT_REVSWAP — x <-> y  reversible value swap ────
     * Outside `every`, no driver backtracks; behaves identically to
     * left-to-right halt-on-fail TT_SWAP.  Inside `every`, coro_eval
     * routes to coro_bb_revswap for snapshot + revert on β.                */
    case TT_REVSWAP: {
        if (e->n < 2 || frame_depth <= 0) return NULVCL;
        tree_t *lhs = e->c[0], *rhs = e->c[1];
        DESCR_t lv = interp_eval(lhs), rv = interp_eval(rhs);
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

    /* ── IC-5: TT_LCONCAT — s1 ||| s2  (list concatenation = string concat alias) */
    case TT_LCONCAT: {
        if (e->n < 2) return NULVCL;
        DESCR_t a = interp_eval(e->c[0]);
        DESCR_t b = interp_eval(e->c[1]);
        /* For string operands, behave like string concat */
        char ab[64], bb[64];
        const char *as = IS_INT_fn(a)?(snprintf(ab,64,"%lld",(long long)a.i),ab):IS_REAL_fn(a)?(real_str(a.r,ab,64),ab):VARVAL_fn(a);
        const char *bs = IS_INT_fn(b)?(snprintf(bb,64,"%lld",(long long)b.i),bb):IS_REAL_fn(b)?(real_str(b.r,bb,64),bb):VARVAL_fn(b);
        if (!as) as=""; if (!bs) bs="";
        size_t al=strlen(as),bl=strlen(bs);
        char *buf=GC_malloc(al+bl+1); memcpy(buf,as,al); memcpy(buf+al,bs,bl); buf[al+bl]='\0';
        return STRVAL(buf);
    }

    /* ── IC-5: TT_MAKELIST — [e1,e2,...] list constructor ───────────────── */
    case TT_MAKELIST: {
        int n = e->n;
        /* Register icnlist type once if needed, using a global flag */
        static int icnlist_registered = 0;
        if (!icnlist_registered) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_registered=1; }
        DESCR_t *elems = GC_malloc((n>0?n:1)*sizeof(DESCR_t));
        for (int i = 0; i < n; i++) elems[i] = interp_eval(e->c[i]);
        DESCR_t eptr; eptr.v=DT_DATA; eptr.slen=0; eptr.ptr=(void*)elems;
        DESCR_t ld = DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
        return ld;
    }

    /* ── IC-5: TT_SECTION — s[i:j] string section ─────────────────────────
     * Icon position rules:
     *   p ≥ 1     → position p (1-based; position 1 is before first char)
     *   p == 0    → position past last char (= slen+1)
     *   p < 0     → position slen+1+p   (-1 → slen, -2 → slen-1, …)
     * Out-of-bounds (after normalization, p < 1 or p > slen+1) → fail. */
    case TT_SECTION: {
        if (e->n < 3) return NULVCL;
        DESCR_t sd = interp_eval(e->c[0]);
        const char *s = VARVAL_fn(sd); if (!s) s="";
        int slen = (int)strlen(s);
        int i = (int)to_int(interp_eval(e->c[1]));
        int j = (int)to_int(interp_eval(e->c[2]));
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (j == 0) j = slen + 1; else if (j < 0) j = slen + 1 + j;
        if (i < 1 || i > slen+1 || j < 1 || j > slen+1) return FAILDESCR;
        int lo = i < j ? i : j, hi = i < j ? j : i;
        int len = hi - lo;
        char *buf = GC_malloc(len+1); memcpy(buf, s+lo-1, len); buf[len]='\0';
        return STRVAL(buf);
    }

    /* IC-9 (2026-05-01): TT_SECTION_PLUS — s[i+:n] read.  Position i, length n.
     * Equivalent to s[i : i+n] with negative-position normalization on i. */
    case TT_SECTION_PLUS: {
        if (e->n < 3) return NULVCL;
        DESCR_t sd = interp_eval(e->c[0]);
        const char *s = VARVAL_fn(sd); if (!s) s = "";
        int slen = (int)strlen(s);
        int i = (int)to_int(interp_eval(e->c[1]));
        int n = (int)to_int(interp_eval(e->c[2]));
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return FAILDESCR;
        /* n may be negative — equivalent to extending leftward. */
        int lo, hi;
        if (n >= 0) { lo = i; hi = i + n; }
        else        { lo = i + n; hi = i; }
        if (lo < 1 || hi > slen+1) return FAILDESCR;
        int len = hi - lo;
        char *buf = GC_malloc(len+1); memcpy(buf, s+lo-1, len); buf[len]='\0';
        return STRVAL(buf);
    }

    /* IC-9 (2026-05-01): TT_SECTION_MINUS — s[i-:n] read.  Position i, span n
     * to the LEFT of i.  Equivalent to s[i-n : i] with negative-n flipping. */
    case TT_SECTION_MINUS: {
        if (e->n < 3) return NULVCL;
        DESCR_t sd = interp_eval(e->c[0]);
        const char *s = VARVAL_fn(sd); if (!s) s = "";
        int slen = (int)strlen(s);
        int i = (int)to_int(interp_eval(e->c[1]));
        int n = (int)to_int(interp_eval(e->c[2]));
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return FAILDESCR;
        int lo, hi;
        if (n >= 0) { lo = i - n; hi = i; }
        else        { lo = i;     hi = i - n; }
        if (lo < 1 || hi > slen+1) return FAILDESCR;
        int len = hi - lo;
        char *buf = GC_malloc(len+1); memcpy(buf, s+lo-1, len); buf[len]='\0';
        return STRVAL(buf);
    }

    /* ── IC-5: TT_INITIAL — once-only block; persists local values across calls ─ */
    case TT_INITIAL: {
        /* Uses file-scope init_tab[] keyed on e->_id.
         * First call: run block, snapshot assigned locals.
         * Subsequent calls: restore snapshot (updated at call exit by icn_init_update_snapshot). */
        IcnInitEnt *ent = NULL;
        for (int _i = 0; _i < icn_init_n; _i++)
            if (init_tab[_i].id == e->_id) { ent = &init_tab[_i]; break; }

        if (!ent) {
            /* ── First call: run the block ── */
            for (int i = 0; i < e->n; i++) interp_eval(e->c[i]);
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
            /* ── Subsequent calls: restore snapshot into frame/NV ── */
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

    /* ── IC-5: TT_RECORD — register record type ──────────────────────────── */
    case TT_RECORD: {
        /* e->v.sval = type name; children = field name TT_VAR nodes.
         * Build spec string "typename(f1,f2,...)" and call DEFDAT_fn + sc_dat_register. */
        if (!e->v.sval) return NULVCL;
        /* Only register once (tree_t node persists across calls) */
        if (e->v.ival != 0) return NULVCL;
        e->v.ival = 1;
        char spec[256]; int pos=0;
        pos += snprintf(spec+pos, sizeof(spec)-pos, "%s(", e->v.sval);
        for (int i = 0; i < e->n && pos < (int)sizeof(spec)-2; i++) {
            if (i > 0) spec[pos++]=',';
            const char *fn2 = (e->c[i] && e->c[i]->v.sval) ? e->c[i]->v.sval : "";
            pos += snprintf(spec+pos, sizeof(spec)-pos, "%s", fn2);
        }
        if (pos < (int)sizeof(spec)-1) spec[pos++]=')';
        spec[pos]='\0';
        DEFDAT_fn(spec);
        sc_dat_register(spec);   /* IC-5: also register in our dispatch table */
        return NULVCL;
    }

    /* ── IC-5: TT_FIELD — field access: obj.fieldname ────────────────────── */
    case TT_FIELD: {
        /* e->v.sval = field name; child[0] = object expression */
        if (!e->v.sval || e->n < 1) return NULVCL;
        DESCR_t obj = interp_eval(e->c[0]);
        if (IS_FAIL_fn(obj)) return FAILDESCR;
        DESCR_t *cell = data_field_ptr(e->v.sval, obj);
        if (!cell) return FAILDESCR;
        return *cell;
    }

    /* ── IC-5: TT_GLOBAL (declaration, skip at eval time) ───────────────── */
    case TT_GLOBAL:
        return NULVCL;

    default:
        return NULVCL;
    }
}


/* -- interp_eval_ref -- lvalue evaluator → DESCR_t* (SIL NAME semantics) -----
 * Returns a pointer to the live descriptor cell for the given lvalue expression.
 * Mirrors SIL: ARYA10 (array), ASSCR (table), FIELD (DATA), GNVARS (variable).
 * Returns NULL for non-addressable positions (OOB, I/O vars, etc.).
 * The caller wraps the result as NAMEPTR(ptr) to create a DT_N descriptor.
 * --------------------------------------------------------------------- */
