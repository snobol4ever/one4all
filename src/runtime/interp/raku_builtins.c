/*============================================================================================================================
 * raku_builtins.c — RS-23a-raku: lift Raku-specific builtins out of interp_eval's icn-frame TT_FNC switch.
 *
 * Why this file exists.  Before RS-23a-raku, the Raku builtins (raku_try,
 * raku_die, raku_map, raku_grep, raku_sort, raku_substr / raku_index /
 * raku_rindex, uc / lc / chars / length / trim, raku_match / raku_match_global /
 * raku_subst, raku_named_capture / raku_capture, file-I/O wrappers, raku_new /
 * raku_mcall, RK-32 raku_nfa_compile, etc.) lived inside interp_eval.c's
 * icn-frame TT_FNC case (~lines 955-1547 pre-lift).  That made them reachable
 * only via direct interp_eval recursion — when bb_eval_value's TT_FNC case
 * dispatched a Raku call through icn_call_builtin, the Raku names were not
 * recognised and control fell back to interp_eval, walking the IR tree.
 * That violates the IR/SM isolation invariant the four-mode pipeline depends
 * on (RS-20).  The first RS-23a attempt regressed unified_broker
 * rk_map_grep_sort24 / rk_try_catch25 once stmt-context TT_FNC was routed
 * through bb_eval_value, because the Raku block-receiving builtins disappeared.
 *
 * The fix: a single function `raku_try_call_builtin(call, *out)` that owns
 * the dispatch.  It returns 1 if `call` named a Raku builtin (with *out set
 * to the result) and 0 otherwise.  Every former `interp_eval(child)` is now
 * `bb_eval_value(child)`.  The function is invoked from three places:
 *   1. interp_eval's icn-frame TT_FNC case — preserves mode-1 behaviour.
 *   2. bb_eval_value's TT_FNC case — before the generic builtin arg-eval loop,
 *      because Raku block-receiving builtins (raku_try / raku_map / raku_grep /
 *      raku_sort) need tree_t access and must not be subject to FAIL-prop on
 *      the body argument.
 *   3. icn_call_builtin top — defensive coverage for the icn_bb_fnc path.
 *
 * Design notes.
 * - Internal recursions use `bb_eval_value` (not `interp_eval`), per the rung's
 *   spec.  This is safe in mode 1 because mode-1 Icon programs flow through
 *   FRAME-aware coro_call before reaching builtin dispatch — frame_depth > 0
 *   during execution, so bb_eval_value's Icon-frame TERM_VAR shim is correct.
 *   Outside an Icon frame, bb_eval_value delegates to eval_node (the SNOBOL4
 *   path), which is also correct because Raku builtins called from the SNOBOL4
 *   frontend would themselves run in eval_node territory.
 * - Out-parameter (`DESCR_t *__rk_out`) is the conventional way to keep the
 *   "tried/found" bool clean from the carried descriptor; the parameter name
 *   is intentionally collision-proof (the lifted code uses local `out` and
 *   `res` heavily).
 * - Symbols this file references are all already declared in either
 *   `interp_private.h` (g_raku_*, raku_fh_*, sc_dat_*, exec_stmt),
 *   `coro_runtime.h` (FRAME, frame_depth, proc_table, proc_count, coro_call,
 *   NV_SET_fn via snobol4.h), or `frontend/raku/raku_re.h` (Raku_nfa,
 *   raku_nfa_*).  No new declarations were added.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 * SPRINT:  RS-23a-raku (2026-05-03)
 *==========================================================================================================================*/

#include "raku_builtins.h"
#include "coro_value.h"            /* bb_eval_value */
#include "coro_runtime.h"          /* FRAME, frame_depth, proc_table, proc_count, coro_call */
#include "../../driver/interp_private.h"  /* g_raku_*, raku_fh_*, sc_dat_*, exec_stmt, ScDatType, DATINST_t via snobol4.h */
#include "../../frontend/raku/raku_re.h"  /* Raku_nfa, raku_nfa_build/exec/free, Raku_match */
#include "snobol4.h"               /* NV_SET_fn, STRVAL, INTVAL, REALVAL, DESCR_t, FAILDESCR, NULVCL, IS_*_fn, VARVAL_fn */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <gc/gc.h>

/*----------------------------------------------------------------------------------------------------------------------------
 * raku_try_call_builtin — Raku-builtin dispatch.
 *
 * Returns 1 if `call` names a Raku builtin and was handled (*__rk_out set to
 * the result, which may be FAILDESCR).  Returns 0 if the name does not match
 * any Raku builtin — caller falls through to its own dispatch.
 *
 * Internal recursions evaluate children via `bb_eval_value`, not `interp_eval`,
 * to keep this file off the IR-walker call graph (RS-20 isolation invariant).
 *--------------------------------------------------------------------------------------------------------------------------*/
int raku_try_call_builtin(tree_t *call, DESCR_t *__rk_out) {
    if (!call || call->n < 1 || !call->c[0]) return 0;
    const char *fn = call->c[0]->v.sval;
    if (!fn) return 0;
    int nargs = call->n - 1;

            /* ── RK-22: Raku string op builtins ────────────────────────────
             * substr($s, $start [, $len])  — 0-based; maps to SNOBOL4 SUBSTR (1-based)
             * index($s, $needle [, $pos])  — 0-based pos of first match, -1 if not found
             * rindex($s, $needle [, $pos]) — 0-based pos of last match, -1 if not found
             * uc($s)   — uppercase via REPLACE(s, &lcase, &ucase)
             * lc($s)   — lowercase via REPLACE(s, &ucase, &lcase)
             * trim($s) — strip leading+trailing whitespace (Raku semantics)
             * chars($s) / length($s) — number of chars                     */
            if (!strcmp(fn,"raku_substr") || (!strcmp(fn,"substr") && nargs >= 2)) {
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t id = bb_eval_value(call->c[2]);
                const char *s = VARVAL_fn(sd); if (!s) s = "";
                long slen = (long)strlen(s);
                long start = IS_INT_fn(id) ? id.i : 0;
                if (start < 0) start = slen + start;
                if (start < 0) start = 0;
                if (start > slen) start = slen;
                long len = slen - start;
                if (nargs >= 3) {
                    DESCR_t ld = bb_eval_value(call->c[3]);
                    len = IS_INT_fn(ld) ? ld.i : len;
                    if (len < 0) len = 0;
                    if (start + len > slen) len = slen - start;
                }
                char *out = GC_malloc((size_t)len + 1);
                memcpy(out, s + start, (size_t)len); out[len] = '\0';
                { *__rk_out = STRVAL(out); return 1; }
            }
            if (!strcmp(fn,"raku_index") || (!strcmp(fn,"index") && nargs >= 2)) {
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t nd = bb_eval_value(call->c[2]);
                const char *s = VARVAL_fn(sd); if (!s) s = "";
                const char *needle = VARVAL_fn(nd); if (!needle) needle = "";
                long from = 0;
                if (nargs >= 3) { DESCR_t pd = bb_eval_value(call->c[3]); from = IS_INT_fn(pd)?pd.i:0; }
                if (from < 0) from = 0;
                if (*needle == '\0') { *__rk_out = INTVAL(from); return 1; }
                const char *found = strstr(s + from, needle);
                { *__rk_out = found ? INTVAL((long)(found - s)) : INTVAL(-1); return 1; }
            }
            if (!strcmp(fn,"raku_rindex") || (!strcmp(fn,"rindex") && nargs >= 2)) {
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t nd = bb_eval_value(call->c[2]);
                const char *s = VARVAL_fn(sd); if (!s) s = "";
                const char *needle = VARVAL_fn(nd); if (!needle) needle = "";
                long slen = (long)strlen(s);
                long from = slen;
                if (nargs >= 3) { DESCR_t pd = bb_eval_value(call->c[3]); from = IS_INT_fn(pd)?pd.i:slen; }
                size_t nlen = strlen(needle);
                if (nlen == 0) { *__rk_out = INTVAL(from < slen ? from : slen); return 1; }
                long best = -1;
                for (long i = 0; i <= from - (long)nlen; i++) {
                    if (memcmp(s + i, needle, nlen) == 0) best = i;
                }
                { *__rk_out = INTVAL(best); return 1; }
            }
            if (!strcmp(fn,"raku_match") && nargs == 2) {
                /* RK-23: $s ~~ /pattern/ — substring search (literal regex subset).
                 * Returns INTVAL(1) on match, FAILDESCR on no match.
                 * If pattern evaluates to DT_P, dispatch through match_pattern. */
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t pd = bb_eval_value(call->c[2]);
                const char *subj = VARVAL_fn(sd); if (!subj) subj = "";
                if (pd.v == DT_P) {
                    extern int exec_stmt(const char *, DESCR_t *, DESCR_t, DESCR_t *, int);
                    { *__rk_out = exec_stmt(NULL, &sd, pd, NULL, 0) ? INTVAL(1) : FAILDESCR; return 1; }
                }
                const char *pat = VARVAL_fn(pd); if (!pat) pat = "";
                { Raku_nfa *nfa = raku_nfa_build(pat);
                  if (!nfa) { *__rk_out = FAILDESCR; return 1; }
                  raku_nfa_exec(nfa, subj, &g_raku_match);
                  g_raku_subject = subj;
                  raku_nfa_free(nfa);
                  { *__rk_out = g_raku_match.matched ? INTVAL(1) : FAILDESCR; return 1; }
                }
            }
            if (!strcmp(fn,"raku_match_global") && nargs == 2) {
                /* RK-37: $s ~~ m:g/pat/ -- collect all non-overlapping matches */
                /* Returns SOH-delimited list of full-match strings for for-loop */
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t pd = bb_eval_value(call->c[2]);
                const char *subj = VARVAL_fn(sd); if (!subj) subj = "";
                const char *pat  = VARVAL_fn(pd); if (!pat)  pat  = "";
                Raku_nfa *nfa = raku_nfa_build(pat);
                if (!nfa) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                int slen = (int)strlen(subj);
                /* collect all matches into a SOH-delimited array string */
                char *out = GC_malloc(slen * 4 + 4); out[0] = '\0';
                int pos = 0, count = 0;
                while (pos <= slen) {
                    Raku_match m;
                    /* build a temporary subject slice via exec on offset */
                    raku_nfa_exec(nfa, subj + pos, &m);
                    if (!m.matched) break;
                    int mlen = m.full_end - m.full_start;
                    if (count > 0) { int ol=strlen(out); out[ol]='\x01'; out[ol+1]='\0'; }
                    strncat(out, subj + pos + m.full_start, (size_t)mlen);
                    /* also update g_raku_match for last match captures */
                    g_raku_match = m;
                    g_raku_match.full_start += pos;
                    g_raku_match.full_end   += pos;
                    for (int g=0;g<m.ngroups;g++) {
                        if (m.group_start[g]>=0) g_raku_match.group_start[g]+=pos;
                        if (m.group_end[g]>=0)   g_raku_match.group_end[g]+=pos;
                    }
                    g_raku_subject = subj;
                    pos += m.full_start + (mlen > 0 ? mlen : 1);
                    count++;
                }
                raku_nfa_free(nfa);
                { *__rk_out = count > 0 ? STRVAL(out) : FAILDESCR; return 1; }
            }
            if (!strcmp(fn,"raku_subst") && nargs == 2) {
                /* RK-37: $s ~~ s/pat/repl/[g] -- substitution */
                /* tok format: "pat\x01repl\x01flag" where flag=g or - */
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t td = bb_eval_value(call->c[2]);
                const char *subj = VARVAL_fn(sd); if (!subj) subj = "";
                const char *tok  = VARVAL_fn(td); if (!tok)  tok  = "";
                /* split tok on \x01 */
                const char *sep1 = strchr(tok, '\x01');
                if (!sep1) { *__rk_out = sd; return 1; }
                const char *sep2 = strchr(sep1+1, '\x01');
                if (!sep2) { *__rk_out = sd; return 1; }
                int plen = (int)(sep1-tok);
                int rlen = (int)(sep2-(sep1+1));
                char *pat  = GC_malloc(plen+1); memcpy(pat, tok, plen); pat[plen]='\0';
                char *repl = GC_malloc(rlen+1); memcpy(repl, sep1+1, rlen); repl[rlen]='\0';
                int global = (*(sep2+1)=='g');
                Raku_nfa *nfa = raku_nfa_build(pat);
                if (!nfa) { *__rk_out = sd; return 1; }
                int slen=(int)strlen(subj);
                char *res = GC_malloc(slen*4+rlen*8+4); res[0]='\0';
                int pos=0, did_one=0;
                while (pos<=slen) {
                    Raku_match m; raku_nfa_exec(nfa, subj+pos, &m);
                    if (!m.matched) { strncat(res, subj+pos, (size_t)(slen-pos)); break; }
                    /* copy pre-match */
                    strncat(res, subj+pos, (size_t)m.full_start);
                    /* copy replacement (TODO: $0/$<n> expansion in repl) */
                    strcat(res, repl);
                    g_raku_match=m; g_raku_subject=subj;
                    int advance=m.full_start+(m.full_end-m.full_start>0?m.full_end-m.full_start:1);
                    pos+=advance; did_one=1;
                    if (!global) { strncat(res, subj+pos, (size_t)(slen-pos)); break; }
                }
                raku_nfa_free(nfa);
                /* update the subject variable in the frame if it was a VAR */
                if (call->c[1]->t==TERM_VAR && call->c[1]->v.ival>=0 &&
                    call->c[1]->v.ival<FRAME.env_n && frame_depth>0)
                    FRAME.env[call->c[1]->v.ival] = STRVAL(res);
                { *__rk_out = did_one ? STRVAL(res) : sd; return 1; }
            }
            /* RK-38: file I/O builtins */
            if (!strcmp(fn,"open") && (nargs==1||nargs==2)) {
                DESCR_t pd=bb_eval_value(call->c[1]);
                const char *path=VARVAL_fn(pd); if(!path||!*path) { *__rk_out = FAILDESCR; return 1; }
                const char *mode="r";
                if(nargs==2){
                    DESCR_t md=bb_eval_value(call->c[2]);
                    const char *ms=VARVAL_fn(md); if(!ms) ms="";
                    if(strstr(ms,":w")||strstr(ms,"w")) mode="w";
                    else if(strstr(ms,":a")||strstr(ms,"a")) mode="a";
                }
                FILE *fp=fopen(path,mode);
                if(!fp) { *__rk_out = FAILDESCR; return 1; }
                int idx=raku_fh_alloc(fp);
                if(idx<0){fclose(fp);{ *__rk_out = FAILDESCR; return 1; }}
                { *__rk_out = INTVAL(idx); return 1; }
            }
            if (!strcmp(fn,"close") && nargs==1) {
                DESCR_t fd=bb_eval_value(call->c[1]);
                int idx=(int)(IS_INT_fn(fd)?fd.i:0);
                FILE *fp=raku_fh_get(idx);
                if(fp){fclose(fp);raku_fh_free(idx);}
                { *__rk_out = INTVAL(0); return 1; }
            }
            if (!strcmp(fn,"slurp") && nargs==1) {
                DESCR_t ad=bb_eval_value(call->c[1]);
                FILE *fp=NULL; int need_close=0;
                if(IS_INT_fn(ad)) {
                    fp=raku_fh_get((int)ad.i);
                } else {
                    const char *path=VARVAL_fn(ad); if(!path||!*path) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                    fp=fopen(path,"r"); need_close=1;
                }
                if(!fp) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                fseek(fp,0,SEEK_END); long sz=ftell(fp); rewind(fp);
                char *buf=GC_malloc(sz+1);
                size_t nr=fread(buf,1,(size_t)sz,fp); buf[nr]='\0';
                if(need_close) fclose(fp);
                { *__rk_out = STRVAL(buf); return 1; }
            }
            if (!strcmp(fn,"lines") && nargs==1) {
                /* lines(fh|path) -> SOH-delimited line list for for-loop */
                DESCR_t ad=bb_eval_value(call->c[1]);
                FILE *fp=NULL; int need_close=0;
                if(IS_INT_fn(ad)) {
                    fp=raku_fh_get((int)ad.i);
                } else {
                    const char *path=VARVAL_fn(ad); if(!path||!*path) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                    fp=fopen(path,"r"); need_close=1;
                }
                if(!fp) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                char *out=GC_malloc(65536); out[0]='\0'; size_t cap=65536, used=0;
                char line[4096]; int first=1;
                while(fgets(line,sizeof line,fp)){
                    size_t ll=strlen(line);
                    while(ll>0&&(line[ll-1]=='\n'||line[ll-1]=='\r')) line[--ll]='\0';
                    size_t need=used+ll+2;
                    if(need>cap){cap=need*2;char*nb=GC_malloc(cap);memcpy(nb,out,used);out=nb;}
                    if(!first){out[used++]='\x01';}
                    memcpy(out+used,line,ll); used+=ll; out[used]='\0'; first=0;
                }
                if(need_close) fclose(fp);
                { *__rk_out = STRVAL(out); return 1; }
            }
            if ((!strcmp(fn,"raku_print_fh")||!strcmp(fn,"raku_say_fh")) && nargs==2) {
                /* RK-39: print/say to file handle */
                DESCR_t fd=bb_eval_value(call->c[1]);
                DESCR_t vd=bb_eval_value(call->c[2]);
                int idx=(int)(IS_INT_fn(fd)?fd.i:1);
                FILE *fp=raku_fh_get(idx); if(!fp) fp=stdout;
                const char *s=VARVAL_fn(vd); if(!s) s="";
                fputs(s,fp);
                if(!strcmp(fn,"raku_say_fh")) fputc('\n',fp);
                { *__rk_out = INTVAL(0); return 1; }
            }
            if (!strcmp(fn,"spurt") && nargs==2) {
                /* RK-56: spurt(path, content) -- write string to file */
                DESCR_t pd=bb_eval_value(call->c[1]);
                DESCR_t cd=bb_eval_value(call->c[2]);
                const char *path=VARVAL_fn(pd); if(!path||!*path) { *__rk_out = FAILDESCR; return 1; }
                const char *content=VARVAL_fn(cd); if(!content) content="";
                FILE *fp=fopen(path,"w"); if(!fp) { *__rk_out = FAILDESCR; return 1; }
                fputs(content,fp); fclose(fp);
                { *__rk_out = INTVAL(0); return 1; }
            }
            if (!strcmp(fn,"raku_nfa_compile") && nargs == 1) {
                /* RK-32: compile pattern string -> NFA, print state count, return 0 */
                DESCR_t pd = bb_eval_value(call->c[1]);
                const char *pat = VARVAL_fn(pd); if (!pat) pat = "";
                { Raku_nfa *nfa = raku_nfa_build(pat);
                  if (!nfa) { printf("NFA:%s:ERROR\n", pat); { *__rk_out = INTVAL(0); return 1; } }
                  printf("NFA:%s:states=%d\n", pat, raku_nfa_state_count(nfa));
                  raku_nfa_free(nfa);
                }
                { *__rk_out = INTVAL(0); return 1; }
            }
            if (!strcmp(fn,"raku_named_capture") && nargs == 1) {
                /* RK-35: $<n> named capture from last ~~ match */
                DESCR_t nd = bb_eval_value(call->c[1]);
                const char *name = VARVAL_fn(nd); if (!name) name = "";
                if (!g_raku_match.matched) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                int g = -1;
                for (int i=0;i<g_raku_match.ngroups;i++)
                    if (strcmp(g_raku_match.group_name[i],name)==0){g=i;break;}
                if (g<0||g_raku_match.group_start[g]<0) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                int gs=g_raku_match.group_start[g], ge=g_raku_match.group_end[g];
                if (ge<gs) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                int len=ge-gs; char *out=GC_malloc(len+1);
                memcpy(out,g_raku_subject+gs,(size_t)len); out[len]='\0';
                { *__rk_out = STRVAL(out); return 1; }
            }
            if (!strcmp(fn,"raku_capture") && nargs == 1) {
                /* RK-34: $N positional capture from last ~~ match */
                DESCR_t nd = bb_eval_value(call->c[1]);
                int n = (int)(IS_INT_fn(nd) ? nd.i : 0);
                if (!g_raku_match.matched || n < 0 || n >= g_raku_match.ngroups
                    || g_raku_match.group_start[n] < 0) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                int gs = g_raku_match.group_start[n];
                int ge = g_raku_match.group_end[n];
                if (ge < gs) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                int len = ge - gs;
                char *out = GC_malloc(len + 1);
                memcpy(out, g_raku_subject + gs, (size_t)len);
                out[len] = '\0';
                { *__rk_out = STRVAL(out); return 1; }
            }
            if (!strcmp(fn,"raku_uc") || (!strcmp(fn,"uc") && nargs == 1)) {
                DESCR_t sd = bb_eval_value(call->c[1]);
                const char *s = VARVAL_fn(sd); if (!s) s = "";
                char *out = GC_strdup(s);
                for (char *p = out; *p; p++) *p = (char)toupper((unsigned char)*p);
                { *__rk_out = STRVAL(out); return 1; }
            }
            if (!strcmp(fn,"raku_lc") || (!strcmp(fn,"lc") && nargs == 1)) {
                DESCR_t sd = bb_eval_value(call->c[1]);
                const char *s = VARVAL_fn(sd); if (!s) s = "";
                char *out = GC_strdup(s);
                for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
                { *__rk_out = STRVAL(out); return 1; }
            }
            if (!strcmp(fn,"raku_trim")) {
                DESCR_t sd = bb_eval_value(call->c[1]);
                const char *s = VARVAL_fn(sd); if (!s) s = "";
                while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
                size_t len = strlen(s);
                while (len > 0 && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r')) len--;
                char *out = GC_malloc(len + 1); memcpy(out, s, len); out[len] = '\0';
                { *__rk_out = STRVAL(out); return 1; }
            }
            if (!strcmp(fn,"chars") || !strcmp(fn,"length")) {
                if (nargs == 1) {
                    DESCR_t sd = bb_eval_value(call->c[1]);
                    const char *s = VARVAL_fn(sd); if (!s) s = "";
                    { *__rk_out = INTVAL((long)strlen(s)); return 1; }
                }
            }

            /* ── RK-25: Raku try/CATCH/die exception handling ───────────────
             * raku_die(msg)         — store msg in g_raku_exception, return FAILDESCR
             * raku_try(body)        — eval body; if FAIL, clear exception, return NULVCL
             * raku_try(body, catch) — eval body; if FAIL, eval catch block, return result */
            if (!strcmp(fn,"raku_die") && nargs >= 1) {
                DESCR_t md = bb_eval_value(call->c[1]);
                const char *msg = VARVAL_fn(md); if (!msg) msg = "Died";
                extern char g_raku_exception[512];
                snprintf(g_raku_exception, sizeof g_raku_exception, "%s", msg);
                { *__rk_out = FAILDESCR; return 1; }
            }
            if (!strcmp(fn,"raku_try") && (nargs == 1 || nargs == 2)) {
                extern char g_raku_exception[512];
                g_raku_exception[0] = '\0';
                DESCR_t r = bb_eval_value(call->c[1]);   /* try body */
                int body_failed = IS_FAIL_fn(r);
                int real_die    = (g_raku_exception[0] != '\0'); /* only raku_die sets this */
                if (!body_failed) { g_raku_exception[0]='\0'; { *__rk_out = r; return 1; } } /* success */
                /* body failed */
                if (nargs == 2 && real_die) {
                    /* CATCH block: only fires on explicit die, not on fall-off-end */
                    tree_t *catch_blk = call->c[2];
                    int _sl2 = -1;
                    tree_t *_stk2[64]; int _sn2=0; _stk2[_sn2++]=catch_blk;
                    while (_sn2>0 && _sl2<0) {
                        tree_t *_n=_stk2[--_sn2]; if(!_n) continue;
                        if(_n->t==TERM_VAR && _n->v.sval &&
                           (strcmp(_n->v.sval,"$!")==0||strcmp(_n->v.sval,"!")==0))
                            _sl2=(int)_n->v.ival;
                        for(int _ci=0;_ci<_n->n&&_sn2<62;_ci++) _stk2[_sn2++]=_n->c[_ci];
                    }
                    DESCR_t exc_d = STRVAL(GC_strdup(g_raku_exception));
                    if (_sl2 >= 0 && _sl2 < FRAME.env_n) FRAME.env[_sl2] = exc_d;
                    else NV_SET_fn("$!", exc_d);
                    g_raku_exception[0] = '\0';
                    { *__rk_out = bb_eval_value(catch_blk); return 1; }
                }
                g_raku_exception[0] = '\0';
                { *__rk_out = NULVCL; return 1; }   /* swallow failure (no CATCH, or non-die failure) */
            }

            /* ── RK-24: Raku map/grep/sort higher-order list ops ────────────
             * raku_map(block, @arr)  — apply block to each elem, collect results
             * raku_grep(block, @arr) — collect elems where block is truthy
             * raku_sort(@arr)        — lexicographic sort
             * raku_sort(block, @arr) — sort with comparator (block uses $a/$b)
             *
             * Arrays are SOH (\x01) delimited strings.
             * Block is an AST_EXPR subtree (child[1]); array is child[2] (or child[1] for sort).
             * $_ is bound into env slot via icn_scope_set for each iteration.  */

            /* helper: split SOH string → char** of elems (GC-allocated) */
#define SOH '\x01'

            if (!strcmp(fn,"raku_map") && nargs == 2) {
                tree_t *blk = call->c[1];          /* block tree_t* */
                DESCR_t arrd = bb_eval_value(call->c[2]);
                const char *as = VARVAL_fn(arrd); if (!as) as = "";
                /* iterate elems, eval block with $_ bound, collect */
                char *out = GC_strdup("");
                const char *seg = as;
                int first = 1;
                do {
                    const char *nx = strchr(seg, SOH);
                    size_t elen = nx ? (size_t)(nx - seg) : strlen(seg);
                    char *elem = GC_malloc(elen + 1);
                    memcpy(elem, seg, elen); elem[elen] = '\0';
                    /* bind $_ — walk closure tree; $_ has sval="$_" or "_", use ival as slot */
                    { /* elem: INTVAL if numeric, else STRVAL */
                      char *_ep_ev; long _iv_ev = strtol(elem, &_ep_ev, 10);
                      DESCR_t _ev = (*_ep_ev == '\0' && _ep_ev > elem) ? INTVAL(_iv_ev) : STRVAL(elem);
                      int _sl = -1;
                      tree_t *_stk[64]; int _sn=0; _stk[_sn++]=blk;
                      while (_sn>0 && _sl<0) {
                          tree_t *_n=_stk[--_sn];
                          if (!_n) continue;
                          if (_n->t==TERM_VAR && _n->v.sval) {
                              const char *_sv = _n->v.sval;
                              /* match "$_" or "_" (sigil may be stripped) */
                              if (strcmp(_sv,"$_")==0 || strcmp(_sv,"_")==0)
                                  _sl=(int)_n->v.ival;
                          }
                          for(int _ci=0;_ci<_n->n&&_sn<62;_ci++) _stk[_sn++]=_n->c[_ci];
                      }
                      if (_sl >= 0 && _sl < FRAME.env_n) FRAME.env[_sl] = _ev;
                      else NV_SET_fn("$_", _ev); }
                    DESCR_t r = bb_eval_value(blk);
                    if (!IS_FAIL_fn(r)) {
                        const char *rv; char rb[64];
                        if (IS_INT_fn(r))       { snprintf(rb,sizeof rb,"%lld",(long long)r.i); rv=rb; }
                        else if (IS_REAL_fn(r)) { snprintf(rb,sizeof rb,"%g",r.r); rv=rb; }
                        else                    { rv = VARVAL_fn(r); if (!rv) rv = ""; }
                        size_t ol = strlen(out), rl = strlen(rv);
                        char *nout = GC_malloc(ol + rl + 2);
                        memcpy(nout, out, ol);
                        if (!first) { nout[ol] = SOH; memcpy(nout+ol+1, rv, rl); nout[ol+1+rl]='\0'; }
                        else        { memcpy(nout+ol, rv, rl); nout[ol+rl]='\0'; first=0; }
                        out = nout;
                    }
                    seg = nx ? nx + 1 : NULL;
                } while (seg);
                { *__rk_out = STRVAL(out); return 1; }
            }

            if (!strcmp(fn,"raku_grep") && nargs == 2) {
                tree_t *blk = call->c[1];
                DESCR_t arrd = bb_eval_value(call->c[2]);
                const char *as = VARVAL_fn(arrd); if (!as) as = "";
                char *out = GC_strdup("");
                const char *seg = as;
                int first = 1;
                do {
                    const char *nx = strchr(seg, SOH);
                    size_t elen = nx ? (size_t)(nx - seg) : strlen(seg);
                    char *elem = GC_malloc(elen + 1);
                    memcpy(elem, seg, elen); elem[elen] = '\0';
                    /* bind $_ — walk closure tree; $_ has sval="$_" or "_", use ival as slot */
                    { /* elem: INTVAL if numeric, else STRVAL */
                      char *_ep_ev; long _iv_ev = strtol(elem, &_ep_ev, 10);
                      DESCR_t _ev = (*_ep_ev == '\0' && _ep_ev > elem) ? INTVAL(_iv_ev) : STRVAL(elem);
                      int _sl = -1;
                      tree_t *_stk[64]; int _sn=0; _stk[_sn++]=blk;
                      while (_sn>0 && _sl<0) {
                          tree_t *_n=_stk[--_sn];
                          if (!_n) continue;
                          if (_n->t==TERM_VAR && _n->v.sval) {
                              const char *_sv = _n->v.sval;
                              /* match "$_" or "_" (sigil may be stripped) */
                              if (strcmp(_sv,"$_")==0 || strcmp(_sv,"_")==0)
                                  _sl=(int)_n->v.ival;
                          }
                          for(int _ci=0;_ci<_n->n&&_sn<62;_ci++) _stk[_sn++]=_n->c[_ci];
                      }
                      if (_sl >= 0 && _sl < FRAME.env_n) FRAME.env[_sl] = _ev;
                      else NV_SET_fn("$_", _ev); }
                    DESCR_t r = bb_eval_value(blk);
                    /* RK-24: grep truthy = block did not fail (SNOBOL4 success/fail semantics).
                     * TT_EQ/TT_LT etc return FAILDESCR on false, non-fail on true. */
                    int truthy = !IS_FAIL_fn(r);
                    if (truthy) {
                        size_t ol = strlen(out), el = strlen(elem);
                        char *nout = GC_malloc(ol + el + 2);
                        memcpy(nout, out, ol);
                        if (!first) { nout[ol] = SOH; memcpy(nout+ol+1,elem,el); nout[ol+1+el]='\0'; }
                        else        { memcpy(nout+ol,elem,el); nout[ol+el]='\0'; first=0; }
                        out = nout;
                    }
                    seg = nx ? nx + 1 : NULL;
                } while (seg);
                { *__rk_out = STRVAL(out); return 1; }
            }

            if (!strcmp(fn,"raku_sort") && (nargs == 1 || nargs == 2)) {
                /* Simple lexicographic sort; numeric if all-integer elements.
                 * With block (nargs==2): use $a/$b comparator block. */
                DESCR_t arrd = bb_eval_value(call->c[nargs == 2 ? 2 : 1]);
                const char *as = VARVAL_fn(arrd); if (!as || !*as) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                tree_t *blk = (nargs == 2) ? call->c[1] : NULL;
                /* split into array of strings */
                int cnt = 1; for (const char *p=as;*p;p++) if(*p==SOH) cnt++;
                char **elems = GC_malloc((size_t)cnt * sizeof(char*));
                int idx = 0; const char *seg = as;
                do {
                    const char *nx = strchr(seg, SOH);
                    size_t elen = nx ? (size_t)(nx-seg) : strlen(seg);
                    char *elem = GC_malloc(elen+1); memcpy(elem,seg,elen); elem[elen]='\0';
                    elems[idx++] = elem;
                    seg = nx ? nx+1 : NULL;
                } while (seg && idx < cnt);
                /* sort: comparator block or default lexicographic */
                if (blk) {
                    /* insertion sort using block with $a/$b */
                    for (int i=1;i<cnt;i++) {
                        char *key = elems[i]; int j=i-1;
                        while (j>=0) {
                            NV_SET_fn("$a", STRVAL(elems[j]));
                            NV_SET_fn("$b", STRVAL(key));
                            DESCR_t r = bb_eval_value(blk);
                            long cmp = IS_INT_fn(r) ? r.i : 0;
                            if (cmp <= 0) break;
                            elems[j+1]=elems[j]; j--;
                        }
                        elems[j+1]=key;
                    }
                } else {
                    /* check if all-integer */
                    int all_int = 1;
                    for (int i=0;i<cnt&&all_int;i++) {
                        char *ep; strtol(elems[i],&ep,10);
                        if (*ep) all_int=0;
                    }
                    /* qsort */
                    if (all_int) {
                        /* numeric sort via simple insertion */
                        for (int i=1;i<cnt;i++) {
                            char *key=elems[i]; long kv=atol(key); int j=i-1;
                            while (j>=0 && atol(elems[j])>kv) { elems[j+1]=elems[j]; j--; }
                            elems[j+1]=key;
                        }
                    } else {
                        for (int i=1;i<cnt;i++) {
                            char *key=elems[i]; int j=i-1;
                            while (j>=0 && strcmp(elems[j],key)>0) { elems[j+1]=elems[j]; j--; }
                            elems[j+1]=key;
                        }
                    }
                }
                /* rejoin */
                size_t total=0; for(int i=0;i<cnt;i++) total+=strlen(elems[i])+1;
                char *out=GC_malloc(total+1); out[0]='\0';
                for (int i=0;i<cnt;i++) {
                    if (i) { size_t ol=strlen(out); out[ol]=SOH; out[ol+1]='\0'; }
                    strcat(out,elems[i]);
                }
                { *__rk_out = STRVAL(out); return 1; }
            }
#undef SOH

            /* ── RK-26: Raku OO builtins ────────────────────────────────────
             * raku_new(classname, key1, val1, key2, val2, ...)
             *   → find registered ScDatType by name, construct instance,
             *     assign named args to matching fields.
             * raku_mcall(obj, methname, arg1, arg2, ...)
             *   → look up obj's datatype name, find "TypeName__methname" proc
             *     in proc_table, call it with (obj, arg1, arg2, ...).
             * ──────────────────────────────────────────────────────────────*/
            if (!strcmp(fn,"raku_new")) {
                /* children: [fn_name_var, classname_qlit, key1, val1, ...] */
                /* call->c[0] = TERM_VAR("raku_new") (make_call layout)
                 * call->c[1] = TT_QLIT(classname)
                 * call->c[2..] = alternating key, val */
                if (call->n < 2) { *__rk_out = NULVCL; return 1; }
                DESCR_t cnameD = bb_eval_value(call->c[1]);
                const char *cname = VARVAL_fn(cnameD);
                if (!cname || !*cname) { *__rk_out = FAILDESCR; return 1; }
                ScDatType *t = sc_dat_find_type(cname);
                if (!t) { *__rk_out = FAILDESCR; return 1; }
                /* Build field array in order matching type definition */
                DESCR_t fvals[64];
                for (int i=0;i<t->nfields && i<64;i++) fvals[i]=NULVCL;
                /* Walk named pairs: children[2],children[3] = key,val ... */
                for (int ci=2; ci+1 < call->n; ci+=2) {
                    DESCR_t kD = bb_eval_value(call->c[ci]);
                    DESCR_t vD = bb_eval_value(call->c[ci+1]);
                    const char *kname = VARVAL_fn(kD);
                    if (!kname) continue;
                    for (int fi=0;fi<t->nfields;fi++) {
                        if (strcasecmp(t->fields[fi], kname)==0) { fvals[fi]=vD; break; }
                    }
                }
                { *__rk_out = sc_dat_construct(t, fvals, t->nfields); return 1; }
            }

            if (!strcmp(fn,"raku_mcall")) {
                /* children: [fn_var, obj, methname_qlit, arg1, arg2, ...] */
                if (call->n < 3) { *__rk_out = FAILDESCR; return 1; }
                DESCR_t obj    = bb_eval_value(call->c[1]);
                DESCR_t mnameD = bb_eval_value(call->c[2]);
                const char *mname = VARVAL_fn(mnameD);
                if (!mname || !*mname) { *__rk_out = FAILDESCR; return 1; }
                /* Determine class name from obj's DT_DATA type */
                const char *cname = NULL;
                if (obj.v == DT_DATA && obj.u) {
                    DATINST_t *inst = (DATINST_t *)obj.u;
                    if (inst->type) cname = inst->type->name;
                }
                if (!cname) { *__rk_out = FAILDESCR; return 1; }
                /* Build proc name: "ClassName__methname" */
                char procname[256];
                snprintf(procname, sizeof procname, "%s__%s", cname, mname);
                /* Find in proc_table */
                int pi;
                for (pi = 0; pi < proc_count; pi++)
                    if (strcmp(proc_table[pi].name, procname) == 0) break;
                if (pi >= proc_count) { *__rk_out = FAILDESCR; return 1; }
                /* Build arg array: self=obj, then extra args */
                int nextra = call->n - 3;
                int total  = 1 + nextra;
                DESCR_t *callargs = GC_malloc((size_t)total * sizeof(DESCR_t));
                callargs[0] = obj;
                for (int i=0;i<nextra;i++) callargs[i+1] = bb_eval_value(call->c[3+i]);
                { *__rk_out = proc_table_call(pi, callargs, total); return 1; }   /* CH-17g-call-sites */
            }

    /* Not a Raku builtin — let caller handle it. */
    return 0;
}
