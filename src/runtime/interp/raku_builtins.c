#include "raku_builtins.h"
#include "icn_value.h"
#include "icn_runtime.h"
#include "../../driver/interp_private.h"  /* g_raku_*, raku_fh_*, sc_dat_*, exec_stmt, ScDatType, DATINST_t via snobol4.h */
#include "../../frontend/raku/raku_re.h"
#include "snobol4.h"               /* NV_SET_fn, STRVAL, INTVAL, REALVAL, DESCR_t, FAILDESCR, NULVCL, IS_*_fn, VARVAL_fn */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <gc/gc.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int raku_try_call_builtin(tree_t *call, DESCR_t *__rk_out) {
    if (!call || call->n < 1 || !call->c[0]) return 0;
    const char *fn = call->c[0]->v.sval;
    if (!fn) return 0;
    int nargs = call->n - 1;
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
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t pd = bb_eval_value(call->c[2]);
                const char *subj = VARVAL_fn(sd); if (!subj) subj = "";
                const char *pat  = VARVAL_fn(pd); if (!pat)  pat  = "";
                Raku_nfa *nfa = raku_nfa_build(pat);
                if (!nfa) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                int slen = (int)strlen(subj);
                char *out = GC_malloc(slen * 4 + 4); out[0] = '\0';
                int pos = 0, count = 0;
                while (pos <= slen) {
                    Raku_match m;
                    raku_nfa_exec(nfa, subj + pos, &m);
                    if (!m.matched) break;
                    int mlen = m.full_end - m.full_start;
                    if (count > 0) { int ol=strlen(out); out[ol]='\x01'; out[ol+1]='\0'; }
                    strncat(out, subj + pos + m.full_start, (size_t)mlen);
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
                DESCR_t sd = bb_eval_value(call->c[1]);
                DESCR_t td = bb_eval_value(call->c[2]);
                const char *subj = VARVAL_fn(sd); if (!subj) subj = "";
                const char *tok  = VARVAL_fn(td); if (!tok)  tok  = "";
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
                    strncat(res, subj+pos, (size_t)m.full_start);
                    strcat(res, repl);
                    g_raku_match=m; g_raku_subject=subj;
                    int advance=m.full_start+(m.full_end-m.full_start>0?m.full_end-m.full_start:1);
                    pos+=advance; did_one=1;
                    if (!global) { strncat(res, subj+pos, (size_t)(slen-pos)); break; }
                }
                raku_nfa_free(nfa);
                if (call->c[1]->t==TERM_VAR && call->c[1]->v.ival>=0 &&
                    call->c[1]->v.ival<FRAME.env_n && frame_depth>0)
                    FRAME.env[call->c[1]->v.ival] = STRVAL(res);
                { *__rk_out = did_one ? STRVAL(res) : sd; return 1; }
            }
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
                DESCR_t pd=bb_eval_value(call->c[1]);
                DESCR_t cd=bb_eval_value(call->c[2]);
                const char *path=VARVAL_fn(pd); if(!path||!*path) { *__rk_out = FAILDESCR; return 1; }
                const char *content=VARVAL_fn(cd); if(!content) content="";
                FILE *fp=fopen(path,"w"); if(!fp) { *__rk_out = FAILDESCR; return 1; }
                fputs(content,fp); fclose(fp);
                { *__rk_out = INTVAL(0); return 1; }
            }
            if (!strcmp(fn,"raku_nfa_compile") && nargs == 1) {
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
                DESCR_t r = bb_eval_value(call->c[1]);
                int body_failed = IS_FAIL_fn(r);
                int real_die    = (g_raku_exception[0] != '\0');
                if (!body_failed) { g_raku_exception[0]='\0'; { *__rk_out = r; return 1; } }
                if (nargs == 2 && real_die) {
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
                { *__rk_out = NULVCL; return 1; }
            }
#define SOH '\x01'
            if (!strcmp(fn,"raku_map") && nargs == 2) {
                tree_t *blk = call->c[1];          /* block tree_t* */
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
                    {
                      char *_ep_ev; long _iv_ev = strtol(elem, &_ep_ev, 10);
                      DESCR_t _ev = (*_ep_ev == '\0' && _ep_ev > elem) ? INTVAL(_iv_ev) : STRVAL(elem);
                      int _sl = -1;
                      tree_t *_stk[64]; int _sn=0; _stk[_sn++]=blk;
                      while (_sn>0 && _sl<0) {
                          tree_t *_n=_stk[--_sn];
                          if (!_n) continue;
                          if (_n->t==TERM_VAR && _n->v.sval) {
                              const char *_sv = _n->v.sval;
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
                    {
                      char *_ep_ev; long _iv_ev = strtol(elem, &_ep_ev, 10);
                      DESCR_t _ev = (*_ep_ev == '\0' && _ep_ev > elem) ? INTVAL(_iv_ev) : STRVAL(elem);
                      int _sl = -1;
                      tree_t *_stk[64]; int _sn=0; _stk[_sn++]=blk;
                      while (_sn>0 && _sl<0) {
                          tree_t *_n=_stk[--_sn];
                          if (!_n) continue;
                          if (_n->t==TERM_VAR && _n->v.sval) {
                              const char *_sv = _n->v.sval;
                              if (strcmp(_sv,"$_")==0 || strcmp(_sv,"_")==0)
                                  _sl=(int)_n->v.ival;
                          }
                          for(int _ci=0;_ci<_n->n&&_sn<62;_ci++) _stk[_sn++]=_n->c[_ci];
                      }
                      if (_sl >= 0 && _sl < FRAME.env_n) FRAME.env[_sl] = _ev;
                      else NV_SET_fn("$_", _ev); }
                    DESCR_t r = bb_eval_value(blk);
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
                DESCR_t arrd = bb_eval_value(call->c[nargs == 2 ? 2 : 1]);
                const char *as = VARVAL_fn(arrd); if (!as || !*as) { *__rk_out = STRVAL(GC_strdup("")); return 1; }
                tree_t *blk = (nargs == 2) ? call->c[1] : NULL;
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
                if (blk) {
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
                    int all_int = 1;
                    for (int i=0;i<cnt&&all_int;i++) {
                        char *ep; strtol(elems[i],&ep,10);
                        if (*ep) all_int=0;
                    }
                    if (all_int) {
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
                size_t total=0; for(int i=0;i<cnt;i++) total+=strlen(elems[i])+1;
                char *out=GC_malloc(total+1); out[0]='\0';
                for (int i=0;i<cnt;i++) {
                    if (i) { size_t ol=strlen(out); out[ol]=SOH; out[ol+1]='\0'; }
                    strcat(out,elems[i]);
                }
                { *__rk_out = STRVAL(out); return 1; }
            }
#undef SOH
            if (!strcmp(fn,"raku_new")) {
                if (call->n < 2) { *__rk_out = NULVCL; return 1; }
                DESCR_t cnameD = bb_eval_value(call->c[1]);
                const char *cname = VARVAL_fn(cnameD);
                if (!cname || !*cname) { *__rk_out = FAILDESCR; return 1; }
                ScDatType *t = sc_dat_find_type(cname);
                if (!t) { *__rk_out = FAILDESCR; return 1; }
                DESCR_t fvals[64];
                for (int i=0;i<t->nfields && i<64;i++) fvals[i]=NULVCL;
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
                if (call->n < 3) { *__rk_out = FAILDESCR; return 1; }
                DESCR_t obj    = bb_eval_value(call->c[1]);
                DESCR_t mnameD = bb_eval_value(call->c[2]);
                const char *mname = VARVAL_fn(mnameD);
                if (!mname || !*mname) { *__rk_out = FAILDESCR; return 1; }
                const char *cname = NULL;
                if (obj.v == DT_DATA && obj.u) {
                    DATINST_t *inst = (DATINST_t *)obj.u;
                    if (inst->type) cname = inst->type->name;
                }
                if (!cname) { *__rk_out = FAILDESCR; return 1; }
                char procname[256];
                snprintf(procname, sizeof procname, "%s__%s", cname, mname);
                int pi;
                for (pi = 0; pi < proc_count; pi++)
                    if (strcmp(proc_table[pi].name, procname) == 0) break;
                if (pi >= proc_count) { *__rk_out = FAILDESCR; return 1; }
                int nextra = call->n - 3;
                int total  = 1 + nextra;
                DESCR_t *callargs = GC_malloc((size_t)total * sizeof(DESCR_t));
                callargs[0] = obj;
                for (int i=0;i<nextra;i++) callargs[i+1] = bb_eval_value(call->c[3+i]);
                { *__rk_out = proc_table_call(pi, callargs, total); return 1; }
            }
    return 0;
}
