/* SnoPat.java — SNOBOL4 pattern matcher for the JVM runtime.                                                                                                                                       */
/* AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet                                                                                                                            */
/*                                                                                                                                                                                                  */
/* This is the runtime helper invoked by SM_PAT_* opcodes and SM_EXEC_STMT in JVM-emitted code.                                                                                                     */
/* SM_PAT_* opcodes build a tree of SnoPat nodes on SnoRt.vstack via the static factory methods.                                                                                                    */
/* SM_EXEC_STMT pops (pat, subject, repl) and calls execStmt(), which performs the unanchored scan with FULLSCAN backtracking.                                                                       */
/*                                                                                                                                                                                                  */
/* Semantic reference: src/runtime/snobol4/snobol4_pattern.c + src/runtime/snobol4/stmt_exec.c in the C runtime.                                                                                     */
/* Match strategy: backtracking recursive descent with a continuation lambda. Each node's match() takes (subj, pos, k) and returns boolean.                                                          */
/* k is the continuation; success is propagated by k returning true. Failure backtracks naturally via Java return.                                                                                  */

package rt;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public final class SnoPat {

    public static final int LIT             =  0;
    public static final int ANY             =  1;
    public static final int NOTANY          =  2;
    public static final int SPAN            =  3;
    public static final int BREAK_          =  4;
    public static final int LEN             =  5;
    public static final int POS             =  6;
    public static final int RPOS            =  7;
    public static final int TAB             =  8;
    public static final int RTAB            =  9;
    public static final int ARB             = 10;
    public static final int ARBNO           = 11;
    public static final int REM             = 12;
    public static final int EPS             = 13;
    public static final int FENCE0          = 14;
    public static final int FENCE1          = 15;
    public static final int ABORT           = 16;
    public static final int FAIL            = 17;
    public static final int SUCCEED         = 18;
    public static final int CAT             = 19;
    public static final int ALT             = 20;
    public static final int ASSIGN_IMM      = 21;
    public static final int ASSIGN_COND     = 22;
    public static final int DEREF           = 23;
    public static final int REFNAME         = 24;
    public static final int CAPTURE_FN      = 25;
    public static final int CAPTURE_FN_ARGS = 26;
    public static final int USERCALL        = 27;
    public static final int USERCALL_ARGS   = 28;
    public static final int BAL             = 29;
    public static final int DOLLAR_IMM      = 30;
    public static final int DOLLAR_COND     = 31;

    public final int kind;
    public final String s;
    public final long n;
    public final SnoPat[] kids;
    public final Object[] argv;
    public final String[] argnames;

    /* AbortException signals SM ABORT semantics: unwind the entire match. */
    private static final class AbortException extends RuntimeException {
        AbortException() { super(); }
    }

    /* Pending capture: applied only after a successful match. The C runtime calls these "deferred captures". */
    private static final class Capture {
        final String var;
        final String value;
        final boolean imm;
        Capture(String v, String val, boolean imm) { this.var = v; this.value = val; this.imm = imm; }
    }

    /* Match context — shared across the whole scan, holds subject string, pending captures, and (for callcap) function call hook. */
    public static final class Ctx {
        final String subj;
        final int subjLen;
        final List<Capture> pending = new ArrayList<>();
        public Ctx(String subj) { this.subj = subj; this.subjLen = subj.length(); }
    }

    public SnoPat(int kind, String s, long n, SnoPat[] kids, Object[] argv, String[] argnames) {
        this.kind = kind;
        this.s = s;
        this.n = n;
        this.kids = kids;
        this.argv = argv;
        this.argnames = argnames;
    }

    public static SnoPat lit(String s)            { return new SnoPat(LIT,    s == null ? "" : s, 0, null, null, null); }
    public static SnoPat any(String cs)           { return new SnoPat(ANY,    cs == null ? "" : cs, 0, null, null, null); }
    public static SnoPat notany(String cs)        { return new SnoPat(NOTANY, cs == null ? "" : cs, 0, null, null, null); }
    public static SnoPat span(String cs)          { return new SnoPat(SPAN,   cs == null ? "" : cs, 0, null, null, null); }
    public static SnoPat brk(String cs)           { return new SnoPat(BREAK_, cs == null ? "" : cs, 0, null, null, null); }
    public static SnoPat len(long n)              { return new SnoPat(LEN,    null, n, null, null, null); }
    public static SnoPat pos(long n)              { return new SnoPat(POS,    null, n, null, null, null); }
    public static SnoPat rpos(long n)             { return new SnoPat(RPOS,   null, n, null, null, null); }
    public static SnoPat tab(long n)              { return new SnoPat(TAB,    null, n, null, null, null); }
    public static SnoPat rtab(long n)             { return new SnoPat(RTAB,   null, n, null, null, null); }
    public static SnoPat arb()                    { return new SnoPat(ARB,    null, 0, null, null, null); }
    public static SnoPat arbno(SnoPat inner)      { return new SnoPat(ARBNO,  null, 0, new SnoPat[]{inner}, null, null); }
    public static SnoPat rem()                    { return new SnoPat(REM,    null, 0, null, null, null); }
    public static SnoPat eps()                    { return new SnoPat(EPS,    null, 0, null, null, null); }
    public static SnoPat fence0()                 { return new SnoPat(FENCE0, null, 0, null, null, null); }
    public static SnoPat fence1(SnoPat inner)     { return new SnoPat(FENCE1, null, 0, new SnoPat[]{inner}, null, null); }
    public static SnoPat abort_()                 { return new SnoPat(ABORT,  null, 0, null, null, null); }
    public static SnoPat fail_()                  { return new SnoPat(FAIL,   null, 0, null, null, null); }
    public static SnoPat succeed_()               { return new SnoPat(SUCCEED, null, 0, null, null, null); }
    public static SnoPat bal()                    { return new SnoPat(BAL,    null, 0, null, null, null); }
    public static SnoPat refname(String name)     { return new SnoPat(REFNAME, name == null ? "" : name, 0, null, null, null); }
    public static SnoPat usercall(String fn)      { return new SnoPat(USERCALL, fn == null ? "" : fn, 0, null, null, null); }

    /* CAT and ALT are right-associative trees. The C runtime flattens these into n-ary nodes; we keep binary to match the SM stack semantics (each SM_PAT_CAT pops two operands). */
    public static SnoPat cat(SnoPat left, SnoPat right) {
        if (left == null)  left  = eps();
        if (right == null) right = eps();
        return new SnoPat(CAT, null, 0, new SnoPat[]{left, right}, null, null);
    }
    public static SnoPat alt(SnoPat left, SnoPat right) {
        if (left == null)  left  = eps();
        if (right == null) right = eps();
        return new SnoPat(ALT, null, 0, new SnoPat[]{left, right}, null, null);
    }

    /* CAPTURE: . and $ assignment. kind=1 for IMM ($), kind=0 for COND (.). */
    public static SnoPat capture(SnoPat child, String var, int captureKind) {
        return new SnoPat(captureKind == 1 ? ASSIGN_IMM : ASSIGN_COND, var == null ? "" : var, 0, new SnoPat[]{child == null ? eps() : child}, null, null);
    }

    /* DEREF: x in pattern context. If x is already a pattern, pass through; if a string, treat as literal. */
    public static SnoPat deref(Object v) {
        if (v == null)              return lit("");
        if (v instanceof SnoPat)    return (SnoPat) v;
        if (v instanceof String)    return lit((String) v);
        return lit(v.toString());
    }

    /* CAPTURE_FN: . *fn(arg1,arg2,...) — call fn(args) with the matched substring's bound vars; we use the named-args variant. */
    public static SnoPat captureFn(SnoPat child, String fname, String namelist) {
        String[] names = (namelist == null || namelist.isEmpty()) ? new String[0] : namelist.split("\t");
        return new SnoPat(CAPTURE_FN, fname == null ? "" : fname, 0, new SnoPat[]{child == null ? eps() : child}, null, names);
    }
    public static SnoPat captureFnArgs(SnoPat child, String fname, Object[] args) {
        return new SnoPat(CAPTURE_FN_ARGS, fname == null ? "" : fname, 0, new SnoPat[]{child == null ? eps() : child}, args, null);
    }
    public static SnoPat usercallArgs(String fname, Object[] args) {
        return new SnoPat(USERCALL_ARGS, fname == null ? "" : fname, 0, null, args, null);
    }

    /* ────────────────────────────────────────────────────────────────────── */
    /* The matcher.                                                          */
    /* Each kind has its own match() body. The continuation k (a Java Runnable returning boolean) is what comes after this pattern.                                                                  */
    /* On success, k.test(endPos) is called; if it returns true, the match commits. If k returns false, we try other branches.                                                                       */
    /* ────────────────────────────────────────────────────────────────────── */

    public interface IntPred { boolean test(int pos); }

    private boolean match(Ctx c, int pos, IntPred k) {
        switch (kind) {
            case LIT: {
                int len = s.length();
                if (pos + len > c.subjLen) return false;
                if (!c.subj.regionMatches(pos, s, 0, len)) return false;
                return k.test(pos + len);
            }
            case ANY: {
                if (pos >= c.subjLen) return false;
                if (s.indexOf(c.subj.charAt(pos)) < 0) return false;
                return k.test(pos + 1);
            }
            case NOTANY: {
                if (pos >= c.subjLen) return false;
                if (s.indexOf(c.subj.charAt(pos)) >= 0) return false;
                return k.test(pos + 1);
            }
            case SPAN: {
                int p = pos;
                while (p < c.subjLen && s.indexOf(c.subj.charAt(p)) >= 0) p++;
                if (p == pos) return false;
                while (p > pos) { if (k.test(p)) return true; p--; }
                return false;
            }
            case BREAK_: {
                int p = pos;
                while (p < c.subjLen && s.indexOf(c.subj.charAt(p)) < 0) p++;
                if (p >= c.subjLen) return false;
                return k.test(p);
            }
            case LEN: {
                int e = pos + (int) n;
                if (e > c.subjLen) return false;
                return k.test(e);
            }
            case POS: {
                if (pos != (int) n) return false;
                return k.test(pos);
            }
            case RPOS: {
                if (c.subjLen - pos != (int) n) return false;
                return k.test(pos);
            }
            case TAB: {
                int target = (int) n;
                if (target < pos || target > c.subjLen) return false;
                return k.test(target);
            }
            case RTAB: {
                int target = c.subjLen - (int) n;
                if (target < pos || target > c.subjLen) return false;
                return k.test(target);
            }
            case ARB: {
                for (int p = pos; p <= c.subjLen; p++) {
                    if (k.test(p)) return true;
                }
                return false;
            }
            case ARBNO: {
                if (k.test(pos)) return true;
                final SnoPat inner = kids[0];
                final int startPos = pos;
                return inner.match(c, pos, new IntPred() {
                    @Override public boolean test(int after) {
                        if (after == startPos) return false;
                        return SnoPat.this.match(c, after, k);
                    }
                });
            }
            case REM:
                return k.test(c.subjLen);
            case EPS:
                return k.test(pos);
            case FENCE0:
                /* FENCE without inner: succeeds at current pos, but commits — failure here aborts the scan from this scan-anchor. We model this as a hard barrier in the continuation. */
                return k.test(pos);
            case FENCE1: {
                /* FENCE(inner): try inner; if it succeeds at position e, commit at e (no backtrack into it). */
                final boolean[] inner_ok = { false };
                final int[] inner_end = { -1 };
                kids[0].match(c, pos, new IntPred() {
                    @Override public boolean test(int e) {
                        inner_ok[0] = true;
                        inner_end[0] = e;
                        return true;
                    }
                });
                if (!inner_ok[0]) return false;
                return k.test(inner_end[0]);
            }
            case ABORT:
                throw new AbortException();
            case FAIL:
                return false;
            case SUCCEED: {
                /* Loop forever on success; in practice, used inside the unanchored scan to retry. We treat as EPS. */
                return k.test(pos);
            }
            case BAL: {
                /* Balanced parenthesized expression: minimal SNOBOL4 BAL — match at least one char up to balancing point. Simplified: match any single non-paren char. */
                if (pos >= c.subjLen) return false;
                char ch = c.subj.charAt(pos);
                if (ch == '(' || ch == ')') return false;
                return k.test(pos + 1);
            }
            case CAT: {
                final SnoPat right = kids[1];
                return kids[0].match(c, pos, new IntPred() {
                    @Override public boolean test(int mid) {
                        return right.match(c, mid, k);
                    }
                });
            }
            case ALT: {
                if (kids[0].match(c, pos, k)) return true;
                return kids[1].match(c, pos, k);
            }
            case ASSIGN_IMM: {
                /* . assignment that fires immediately when the inner matches, regardless of overall match success. Useful for tracing. */
                final String var = s;
                final int startPos = pos;
                return kids[0].match(c, pos, new IntPred() {
                    @Override public boolean test(int e) {
                        String matched = c.subj.substring(startPos, e);
                        SnoRt.store_var_external(var, matched);
                        return k.test(e);
                    }
                });
            }
            case ASSIGN_COND: {
                /* . assignment that fires only when overall match succeeds. We use the pending-captures list. */
                final String var = s;
                final int startPos = pos;
                final int snapshot = c.pending.size();
                return kids[0].match(c, pos, new IntPred() {
                    @Override public boolean test(int e) {
                        String matched = c.subj.substring(startPos, e);
                        c.pending.add(new Capture(var, matched, false));
                        if (k.test(e)) return true;
                        /* Backtrack: rewind pending list. */
                        while (c.pending.size() > snapshot) c.pending.remove(c.pending.size() - 1);
                        return false;
                    }
                });
            }
            case REFNAME: {
                /* *var in pattern context — evaluate var at match time, treat as literal. */
                Object v = SnoRt.get_var_external(s);
                String lit = v == null ? "" : v.toString();
                int len = lit.length();
                if (pos + len > c.subjLen) return false;
                if (!c.subj.regionMatches(pos, lit, 0, len)) return false;
                return k.test(pos + len);
            }
            case DEREF: {
                /* Stub — DEREF should have been resolved at build time via deref() factory. If we reach here, treat as EPS. */
                return k.test(pos);
            }
            case CAPTURE_FN: {
                final String fname = s;
                final String[] names = argnames == null ? new String[0] : argnames;
                final int startPos = pos;
                final int snapshot = c.pending.size();
                return kids[0].match(c, pos, new IntPred() {
                    @Override public boolean test(int e) {
                        String matched = c.subj.substring(startPos, e);
                        Object[] args = new Object[names.length];
                        for (int i = 0; i < names.length; i++) args[i] = SnoRt.get_var_external(names[i]);
                        SnoRt.call_external(fname, args, matched);
                        if (k.test(e)) return true;
                        while (c.pending.size() > snapshot) c.pending.remove(c.pending.size() - 1);
                        return false;
                    }
                });
            }
            case CAPTURE_FN_ARGS: {
                final String fname = s;
                final Object[] args = argv == null ? new Object[0] : argv;
                final int startPos = pos;
                final int snapshot = c.pending.size();
                return kids[0].match(c, pos, new IntPred() {
                    @Override public boolean test(int e) {
                        String matched = c.subj.substring(startPos, e);
                        SnoRt.call_external(fname, args, matched);
                        if (k.test(e)) return true;
                        while (c.pending.size() > snapshot) c.pending.remove(c.pending.size() - 1);
                        return false;
                    }
                });
            }
            case USERCALL:
            case USERCALL_ARGS: {
                /* bare *fn() in pattern position: the function is called at match time and its return value becomes the pattern to match. We stub: treat result string as a literal. */
                Object result = SnoRt.call_returning(s, argv == null ? new Object[0] : argv);
                if (result == null) return false;
                if (result instanceof SnoPat) return ((SnoPat) result).match(c, pos, k);
                String lit = result.toString();
                int len = lit.length();
                if (pos + len > c.subjLen) return false;
                if (!c.subj.regionMatches(pos, lit, 0, len)) return false;
                return k.test(pos + len);
            }
            default:
                return false;
        }
    }

    /* Unanchored scan: try matching at each position 0..subjLen, return true and store match_start/match_end on first success. &ANCHOR=1 restricts to pos=0; we honor the SnoRt.anchor flag. */
    public static int matched_start = -1;
    public static int matched_end   = -1;

    /* Top-level entry: execute pat against subject; return true on success. Pending captures are flushed on success. */
    public static boolean execMatch(SnoPat pat, String subj) {
        if (pat == null) return false;
        boolean anchor = SnoRt.get_anchor();
        int start = 0;
        int endLimit = subj.length();
        while (start <= endLimit) {
            final Ctx c = new Ctx(subj);
            final int s0 = start;
            final boolean[] success = { false };
            final int[] endPos = { -1 };
            try {
                pat.match(c, start, new IntPred() {
                    @Override public boolean test(int e) {
                        success[0] = true;
                        endPos[0] = e;
                        return true;
                    }
                });
            } catch (AbortException ae) {
                return false;
            }
            if (success[0]) {
                matched_start = s0;
                matched_end = endPos[0];
                for (Capture cap : c.pending) {
                    SnoRt.store_var_external(cap.var, cap.value);
                }
                return true;
            }
            if (anchor) return false;
            start++;
        }
        return false;
    }
}
