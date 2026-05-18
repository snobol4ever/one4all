#include "scan_builtins.h"
#include "icn_runtime.h"
#include "snobol4.h"
#include <gc/gc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Resolve a cset-or-string argument to (ptr, len) honoring keyword-cset storage.                                                                                                                       */
/* Keyword csets (&cset, &ascii, &lcase, ...) are null-prefixed buffers; their true length is tracked by icn_kw_cset_len(ptr). Regular cset values and plain strings use strlen.                          */
/* Returns 1 on success (ptr/len written), 0 on null/unresolvable.                                                                                                                                       */
static int cset_resolve(DESCR_t arg, const char **out_ptr, int *out_len) {
    const char *cv;
    int clen;
    if (IS_CSET_fn(arg)) {
        cv = arg.s;
        if (!cv) return 0;
        int klen = icn_kw_cset_len(cv);
        clen = (klen >= 0) ? klen : (int)strlen(cv);
    } else {
        cv = VARVAL_fn(arg);
        if (!cv) return 0;
        clen = (int)strlen(cv);
    }
    *out_ptr = cv;
    *out_len = clen;
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* \0-safe cset membership: returns 1 iff ch appears in cv[0..clen-1].                                                                                                                                  */
static inline int cset_has(const char *cv, int clen, unsigned char ch) {
    return cv && clen > 0 && memchr(cv, ch, (size_t)clen) != NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int scan_try_call_builtin(tree_t *call, DESCR_t *args, int nargs, DESCR_t *out)
{
    if (!call || call->n < 1 || !call->c[0]) return 0;
    const char *fn = call->c[0]->v.sval;
    if (!fn) return 0;
    if (!strcmp(fn, "any") && nargs >= 1 && (scan_pos > 0 || nargs >= 2)) {
        const char *cv; int clen;
        if (!cset_resolve(args[0], &cv, &clen)) { *out = FAILDESCR; return 1; }
        const char *s; int p, slen, end;
        if (nargs >= 2) {
            s = VARVAL_fn(args[1]); if (!s) s = "";
            slen = (int)strlen(s);
            int i1 = (nargs >= 3) ? (int)args[2].i : (scan_pos > 0 ? scan_pos : 1);
            int i2 = (nargs >= 4) ? (int)args[3].i : slen + 1;
            if (i1 <= 0 || i1 > slen) { *out = FAILDESCR; return 1; }
            if (i2 <= 0) i2 = slen + 1;
            p = i1 - 1; end = i2 - 1;
        } else {
            s = scan_subj; if (!s) { *out = FAILDESCR; return 1; }
            slen = (int)strlen(s); p = scan_pos - 1; end = slen;
        }
        if (p < 0 || p >= slen || p >= end || !cset_has(cv, clen, (unsigned char)s[p])) { *out = FAILDESCR; return 1; }
        if (nargs < 2) { *out = INTVAL(p + 2); return 1; }
        *out = INTVAL(p + 2);
        return 1;
    }
    if (!strcmp(fn, "many") && nargs >= 1 && (scan_pos > 0 || nargs >= 2)) {
        const char *cv; int clen;
        if (!cset_resolve(args[0], &cv, &clen)) { *out = FAILDESCR; return 1; }
        const char *s; int p, slen, end;
        if (nargs >= 2) {
            s = VARVAL_fn(args[1]); if (!s) s = "";
            slen = (int)strlen(s);
            int i1 = (nargs >= 3) ? (int)args[2].i : (scan_pos > 0 ? scan_pos : 1);
            int i2 = (nargs >= 4) ? (int)args[3].i : slen + 1;
            if (i1 <= 0 || i1 > slen) { *out = FAILDESCR; return 1; }
            if (i2 <= 0) i2 = slen + 1;
            p = i1 - 1; end = i2 - 1;
        } else {
            s = scan_subj; if (!s) { *out = FAILDESCR; return 1; }
            slen = (int)strlen(s); p = scan_pos - 1; end = slen;
        }
        if (p < 0 || p >= slen || p >= end || !cset_has(cv, clen, (unsigned char)s[p])) { *out = FAILDESCR; return 1; }
        while (p < end && p < slen && cset_has(cv, clen, (unsigned char)s[p])) p++;
        *out = INTVAL(p + 1);
        return 1;
    }
    if (!strcmp(fn, "upto") && nargs >= 1 && (scan_pos > 0 || nargs >= 2)) {
        const char *cv; int clen;
        if (!cset_resolve(args[0], &cv, &clen)) { *out = FAILDESCR; return 1; }
        const char *s; int p, slen, end;
        if (nargs >= 2) {
            s = VARVAL_fn(args[1]); if (!s) s = "";
            slen = (int)strlen(s);
            int i1 = (nargs >= 3) ? (int)args[2].i : (scan_pos > 0 ? scan_pos : 1);
            int i2 = (nargs >= 4) ? (int)args[3].i : slen + 1;
            if (i1 <= 0) i1 = 1;
            if (i2 <= 0) i2 = slen + 1;
            p = i1 - 1; end = i2 - 1;
        } else {
            s = scan_subj; if (!s) { *out = FAILDESCR; return 1; }
            slen = (int)strlen(s); p = scan_pos - 1; end = slen;
        }
        while (p < end && p < slen && !cset_has(cv, clen, (unsigned char)s[p])) p++;
        if (p >= end || p >= slen) { *out = FAILDESCR; return 1; }
        *out = INTVAL(p + 1);
        return 1;
    }
    if (!strcmp(fn, "move") && nargs >= 1 && scan_pos > 0) {
        int n = (int)args[0].i;
        int newp = scan_pos + n;
        if (!scan_subj || newp < 1 || newp > (int)strlen(scan_subj) + 1) { *out = FAILDESCR; return 1; }
        int old = scan_pos; scan_pos = newp;
        size_t len = (size_t)(n >= 0 ? n : -n); int start = (n >= 0 ? old : newp);
        char *buf = GC_malloc(len + 1); memcpy(buf, scan_subj + start - 1, len); buf[len] = '\0';
        *out = STRVAL(buf);
        return 1;
    }
    if (!strcmp(fn, "tab") && nargs >= 1 && scan_pos > 0) {
        if (IS_FAIL_fn(args[0])) { *out = FAILDESCR; return 1; }
        int slen = scan_subj ? (int)strlen(scan_subj) : 0;
        int newp = (int)args[0].i;
        if (newp == 0) newp = slen + 1;
        else if (newp < 0) newp = slen + 1 + newp;
        if (!scan_subj || newp < scan_pos || newp < 1 || newp > slen + 1) { *out = FAILDESCR; return 1; }
        int old = scan_pos; scan_pos = newp; size_t len = (size_t)(newp - old);
        char *buf = GC_malloc(len + 1); memcpy(buf, scan_subj + old - 1, len); buf[len] = '\0';
        *out = STRVAL(buf);
        return 1;
    }
    if (!strcmp(fn, "pos") && nargs >= 1 && scan_pos > 0) {
        if (IS_FAIL_fn(args[0])) { *out = FAILDESCR; return 1; }
        int slen = scan_subj ? (int)strlen(scan_subj) : 0;
        int p = (int)args[0].i;
        if (p == 0) p = slen + 1;
        else if (p < 0) p = slen + 1 + p;
        if (p < 1 || p > slen + 1) { *out = FAILDESCR; return 1; }
        *out = (scan_pos == p) ? INTVAL(scan_pos) : FAILDESCR;
        return 1;
    }
    if (!strcmp(fn, "rpos") && nargs >= 1 && scan_pos > 0) {
        if (IS_FAIL_fn(args[0])) { *out = FAILDESCR; return 1; }
        int slen = scan_subj ? (int)strlen(scan_subj) : 0;
        int p = slen + 1 - (int)args[0].i;
        if (p < 1 || p > slen + 1) { *out = FAILDESCR; return 1; }
        *out = (scan_pos == p) ? INTVAL(scan_pos) : FAILDESCR;
        return 1;
    }
    if (!strcmp(fn, "match") && nargs >= 1 && scan_pos > 0) {
        const char *needle = VARVAL_fn(args[0]);
        const char *hay = scan_subj ? scan_subj : "";
        if (!needle) { *out = FAILDESCR; return 1; }
        int p = scan_pos - 1, nl = (int)strlen(needle);
        if (strncmp(hay + p, needle, nl) != 0) { *out = FAILDESCR; return 1; }
        scan_pos += nl;
        *out = INTVAL(scan_pos);
        return 1;
    }
    if (!strcmp(fn, "bal") && nargs >= 1) {
        const char *c1; int c1len;
        if (!cset_resolve(args[0], &c1, &c1len)) { *out = FAILDESCR; return 1; }
        const char *c2 = "(", *c3 = ")";
        int c2len = 1, c3len = 1;
        if (nargs >= 2) {
            const char *v; int vlen;
            if (cset_resolve(args[1], &v, &vlen) && vlen > 0) { c2 = v; c2len = vlen; }
        }
        if (nargs >= 3) {
            const char *v; int vlen;
            if (cset_resolve(args[2], &v, &vlen) && vlen > 0) { c3 = v; c3len = vlen; }
        }
        const char *s; int slen, p, end;
        if (nargs >= 4) {
            s = VARVAL_fn(args[3]); if (!s) s = "";
            slen = (int)strlen(s);
            int i1 = (nargs >= 5) ? (int)args[4].i : 1;
            int i2 = (nargs >= 6) ? (int)args[5].i : slen + 1;
            if (i1 <= 0) i1 = 1; if (i2 <= 0) i2 = slen + 1;
            p = i1 - 1; end = i2 - 1;
        } else {
            s = scan_subj; if (!s) { *out = FAILDESCR; return 1; }
            slen = (int)strlen(s); p = scan_pos - 1; end = slen;
        }
        int depth = 0;
        while (p < end && p < slen) {
            unsigned char ch = (unsigned char)s[p];
            if (cset_has(c2, c2len, ch)) depth++;
            else if (cset_has(c3, c3len, ch) && depth > 0) depth--;
            else if (depth == 0 && cset_has(c1, c1len, ch)) { *out = INTVAL(p + 1); return 1; }
            p++;
        }
        *out = FAILDESCR;
        return 1;
    }
    if (!strcmp(fn, "find") && nargs >= 2) {
        long pos1; if (icn_frame_lookup(call, &pos1)) { *out = INTVAL(pos1); return 1; }
        const char *needle = VARVAL_fn(args[0]);
        const char *hay = VARVAL_fn(args[1]);
        if (!needle || !hay) { *out = FAILDESCR; return 1; }
        char *p = strstr(hay, needle);
        *out = p ? INTVAL((long long)(p - hay) + 1) : FAILDESCR;
        return 1;
    }
    return 0;
}
