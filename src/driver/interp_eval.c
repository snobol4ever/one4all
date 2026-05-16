#include "interp_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../ast/ast.h"
#include "../runtime/interp/icn_value.h"
unsigned long *rs24_diag_hits_ptr = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *rs24_diag_kind_name(int k);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void set_and_trace(const char *name, DESCR_t val) {
    if (shadow_has(name)) { shadow_set_cur(name, val); goto trace_hook; }
    NV_SET_fn(name, val);
trace_hook:
    if (call_depth > 0) {
        CallFrame *fr = &call_stack[call_depth - 1];
        if (name && fr->fname[0] && strcmp(name, fr->fname) == 0) {
            fr->retval_cell = val;
            fr->retval_set  = 1;
        }
    }
    if (shadow_has(name) && name && name[0] != '&' && trace_is_active(name))
        comm_var(name, val);
}
static const char *PAT_FNC_NAMES[] = {
    "ANY","NOTANY","SPAN","BREAK","BREAKX","LEN","POS","RPOS","TAB","RTAB",
    "ARB","ARBNO","REM","FAIL","SUCCEED","FENCE","ABORT","BAL","CALL", NULL
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int _is_pat_fnc_name(const char *s) {
    if (!s) return 0;
    for (int i = 0; PAT_FNC_NAMES[i]; i++)
        if (strcmp(s, PAT_FNC_NAMES[i]) == 0) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int _expr_is_pat(tree_t *e) {
    if (!e) return 0;
    switch (e->t) {
        case TT_ARB: case TT_ARBNO: case TT_CAPT_COND_ASGN:
        case TT_CAPT_IMMED_ASGN: case TT_CAPT_CURSOR: case TT_DEFER:
            return 1;
        default: break;
    }
    if (e->t == TT_FNC && _is_pat_fnc_name(e->v.sval)) return 1;
    if (e->t == TT_VAR && _is_pat_fnc_name(e->v.sval)) return 1;
    for (int i = 0; i < e->n; i++)
        if (_expr_is_pat(e->c[i])) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t *data_field_ptr(const char *fname, DESCR_t inst) {
    if (inst.v < DT_DATA || !inst.u) return NULL;
    DATBLK_t *blk = inst.u->type;
    if (!blk) return NULL;
    for (int i = 0; i < blk->nfields; i++)
        if (blk->fields[i] && strcmp(blk->fields[i], fname) == 0)
            return &inst.u->fields[i];
    return NULL;
}
#include "../runtime/interp/icn_runtime.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_string_section_assign(tree_t *lhs, DESCR_t val) {
    if (!lhs) return 0;
    int kind = lhs->t;
    if (kind != TT_SECTION && kind != TT_SECTION_PLUS &&
        kind != TT_SECTION_MINUS && kind != TT_IDX) return 0;
    if (lhs->n < 2) return 0;
    if (kind == TT_SECTION && lhs->n < 3) return 0;
    extern int     icn_frame_env_active(void);
    extern DESCR_t icn_frame_env_load(int slot);
    extern void    icn_frame_env_store(int slot, DESCR_t val);
    tree_t *bch = lhs->c[0];
    DESCR_t *cell = NULL;
    DESCR_t _icn_base_storage;
    int _icn_bb_var_slot = -1;
    if (bch && bch->t == TT_VAR && g_lang == LANG_ICN && icn_frame_env_active()) {
        _icn_bb_var_slot = (bch->v.sval) ? scope_get(&FRAME.sc, bch->v.sval) : -1;
        if (_icn_bb_var_slot >= 0) {
            _icn_base_storage = icn_frame_env_load(_icn_bb_var_slot);
            cell = &_icn_base_storage;
        }
    }
    if (!cell && bch && bch->t == TT_VAR && frame_depth > 0) {
        int sl = bch->_id;
        if (sl >= 0 && sl < FRAME.env_n) cell = &FRAME.env[sl];
    }
    if (!cell) cell = interp_eval_ref(bch);
    if (!cell) return 0;
    DESCR_t base = *cell;
    if (kind == TT_IDX) {
        if (base.v != DT_S && base.v != DT_SNUL) return 0;
    }
    const char *s = (base.v == DT_SNUL) ? "" : VARVAL_fn(base);
    if (!s) s = "";
    int slen = (int)strlen(s);
    int lo = 0, hi = 0;
    if (kind == TT_SECTION) {
        DESCR_t _i1=(g_lang==LANG_ICN)?bb_eval_value(lhs->c[1]):interp_eval(lhs->c[1]);
        DESCR_t _i2=(g_lang==LANG_ICN)?bb_eval_value(lhs->c[2]):interp_eval(lhs->c[2]);
        int i=(int)to_int(_i1), j=(int)to_int(_i2);
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (j == 0) j = slen + 1; else if (j < 0) j = slen + 1 + j;
        if (i < 1 || i > slen+1 || j < 1 || j > slen+1) return 0;
        lo = i < j ? i : j; hi = i < j ? j : i;
    } else if (kind == TT_SECTION_PLUS) {
        DESCR_t _sp1=(g_lang==LANG_ICN)?bb_eval_value(lhs->c[1]):interp_eval(lhs->c[1]);
        DESCR_t _sp2=(g_lang==LANG_ICN)?bb_eval_value(lhs->c[2]):interp_eval(lhs->c[2]);
        int i=(int)to_int(_sp1), n=(int)to_int(_sp2);
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return 0;
        if (n < 0) return 0;
        if (i + n > slen + 1) return 0;
        lo = i; hi = i + n;
    } else if (kind == TT_SECTION_MINUS) {
        DESCR_t _sm1=(g_lang==LANG_ICN)?bb_eval_value(lhs->c[1]):interp_eval(lhs->c[1]);
        DESCR_t _sm2=(g_lang==LANG_ICN)?bb_eval_value(lhs->c[2]):interp_eval(lhs->c[2]);
        int i=(int)to_int(_sm1), n=(int)to_int(_sm2);
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return 0;
        if (n < 0) return 0;
        if (i - n < 1) return 0;
        lo = i - n; hi = i;
    } else {
        extern DESCR_t bb_eval_value(tree_t *e);
        DESCR_t _idx_d = (g_lang == LANG_ICN) ? bb_eval_value(lhs->c[1]) : interp_eval(lhs->c[1]);
        int i = (int)to_int(_idx_d);
        if (i == 0) return 0;
        if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen) return 0;
        lo = i; hi = i + 1;
    }
    const char *vs = VARVAL_fn(val); if (!vs) vs = "";
    int vlen = (int)strlen(vs);
    int prefix = lo - 1;
    int suffix = slen - (hi - 1);
    int newlen = prefix + vlen + suffix;
    char *buf = (char *)GC_malloc((size_t)newlen + 1);
    if (prefix > 0) memcpy(buf, s, (size_t)prefix);
    if (vlen > 0)   memcpy(buf + prefix, vs, (size_t)vlen);
    if (suffix > 0) memcpy(buf + prefix + vlen, s + hi - 1, (size_t)suffix);
    buf[newlen] = '\0';
    tree_t *base_expr = lhs->c[0];
    if (_icn_bb_var_slot >= 0) {
        icn_frame_env_store(_icn_bb_var_slot, STRVAL(buf));
    } else if (base_expr && base_expr->t == TT_VAR && base_expr->v.sval &&
               base_expr->v.sval[0] != '&' &&
               !(frame_depth > 0 && base_expr->_id >= 0 && base_expr->_id < FRAME.env_n)) {
        set_and_trace(base_expr->v.sval, STRVAL(buf));
    } else {
        *cell = STRVAL(buf);
    }
    return 1;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *real_str(double r, char *buf, int bufsz) {
    for (int p = 15; p <= 17; p++) {
        snprintf(buf, bufsz, "%.*g", p, r);
        char *end; double back = strtod(buf, &end);
        if (back == r) break;
    }
    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E') && !strchr(buf, 'n') && !strchr(buf, 'N'))
        strncat(buf, ".0", bufsz - strlen(buf) - 1);
    return buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_try_call_builtin_by_name(const char *fn, DESCR_t *args, int nargs, DESCR_t *out)
{
    if (!fn || !out) return 0;
    if (!strcmp(fn, "FAIL"))    { *out = FAILDESCR; return 1; }
    if (!strcmp(fn, "SUCCEED")) { *out = NULVCL;    return 1; }
    if (!strcmp(fn, "write")) {
        int start = 0;
        FILE *dest = stdout;
        if (nargs > 0 && IS_FH_fn(args[0])) {
            FILE *fp = raku_fh_get((int)args[0].i);
            if (fp) dest = fp;
            start = 1;
        }
        for (int _wi = start; _wi < nargs; _wi++) {
            DESCR_t av = args[_wi];
            if (IS_FAIL_fn(av)) { *out = FAILDESCR; return 1; }
            if (av.v == DT_SNUL) continue;
            if (IS_INT_fn(av))       fprintf(dest, "%lld", (long long)av.i);
            else if (IS_REAL_fn(av)) { char _rb[64]; fprintf(dest, "%s", real_str(av.r,_rb,sizeof _rb)); }
            else if (IS_CSET_fn(av)) { if (av.s) fwrite(av.s, 1, strlen(av.s), dest); }
            else { const char *s = VARVAL_fn(av); if (s) fputs(s, dest); }
        }
        fputc('\n', dest);
        *out = nargs > start ? args[nargs-1] : (nargs > 0 ? args[0] : NULVCL);
        return 1;
    }
    if (!strcmp(fn, "writes")) {
        int start = 0;
        FILE *dest = stdout;
        if (nargs > 0 && IS_FH_fn(args[0])) {
            FILE *fp = raku_fh_get((int)args[0].i);
            if (fp) dest = fp;
            start = 1;
        }
        for (int _wi = start; _wi < nargs; _wi++) {
            DESCR_t av = args[_wi];
            if (IS_FAIL_fn(av)) { *out = FAILDESCR; return 1; }
            if (av.v == DT_SNUL) continue;
            if (IS_INT_fn(av))       fprintf(dest, "%lld", (long long)av.i);
            else if (IS_REAL_fn(av)) { char _rb[64]; fprintf(dest, "%s", real_str(av.r,_rb,sizeof _rb)); }
            else if (IS_CSET_fn(av)) { if (av.s) fwrite(av.s, 1, strlen(av.s), dest); }
            else { const char *s = VARVAL_fn(av); if (s) fputs(s, dest); }
        }
        *out = nargs > start ? args[nargs-1] : (nargs > 0 ? args[0] : NULVCL);
        return 1;
    }
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
    if (!strcmp(fn,"cset") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_CSET_fn(av)) { *out = av; return 1; }
        char _cbuf[64];
        const char *raw;
        if (IS_INT_fn(av))       { snprintf(_cbuf,sizeof _cbuf,"%lld",(long long)av.i); raw=_cbuf; }
        else if (IS_REAL_fn(av)) { real_str(av.r,_cbuf,sizeof _cbuf); raw=_cbuf; }
        else { raw = VARVAL_fn(av); if (!raw) raw = ""; }
        *out = CSETVAL(icn_cset_canonical(raw)); return 1;
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
        else if (IS_CSET_fn(av)) t="cset";
        else if (av.v==DT_SNUL)  t="null";
        else t="string";
        *out = STRVAL(t); return 1;
    }
    if (!strcmp(fn,"image") && nargs == 0) {
        *out = STRVAL("&null"); return 1;
    }
    if (!strcmp(fn,"args") && nargs == 1) {
        DESCR_t a = args[0];
        if (a.v == DT_E) {
            for (int i=0;i<proc_count;i++) {
                if (proc_table[i].entry_pc == (int)a.i) {
                    *out = INTVAL(proc_table[i].nparams <= 0 ? -2 : proc_table[i].nparams);
                    return 1;
                }
            }
            *out = INTVAL(-2); return 1;
        }
        if (IS_STR_fn(a)) {
            *out = INTVAL(-2); return 1;
        }
        *out = FAILDESCR; return 1;
    }
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
        *out = STRVAL(GC_strdup(pname)); return 1;
    }
    if (!strcmp(fn,"image") && nargs == 1) {
        DESCR_t av = args[0];
        if (IS_FAIL_fn(av)) { *out = FAILDESCR; return 1; }
        char *buf = GC_malloc(256);
        if (av.v == DT_SNUL)     { *out = STRVAL("&null"); return 1; }
        if (IS_CSET_fn(av)) {
            const char *cs = av.s ? av.s : "";
            const char *kname = icn_kw_cset_name(cs);
            if (kname) { *out = STRVAL(kname); return 1; }
            int clen = (int)strlen(cs);
            char *outs = GC_malloc(clen + 3);
            outs[0] = '\'';
            memcpy(outs+1, cs, clen);
            outs[1+clen] = '\'';
            outs[2+clen] = '\0';
            *out = STRVAL(outs); return 1;
        }
        if (IS_FH_fn(av)) {
            int idx = (int)av.i;
            if (idx >= 0 && idx < RAKU_FH_MAX && raku_fh_name[idx]) {
                snprintf(buf,256,"file(%s)",raku_fh_name[idx]);
                *out = STRVAL(buf); return 1;
            }
            if (idx == 0) { snprintf(buf,256,"file(&input)");  *out = STRVAL(buf); return 1; }
            if (idx == 1) { snprintf(buf,256,"file(&output)"); *out = STRVAL(buf); return 1; }
            if (idx == 2) { snprintf(buf,256,"file(&errout)"); *out = STRVAL(buf); return 1; }
            snprintf(buf,256,"file(?)"); *out = STRVAL(buf); return 1;
        }
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
        if (av.v==DT_E) {
            for (int i=0;i<proc_count;i++)
                if (proc_table[i].entry_pc==(int)av.i)
                    { snprintf(buf,128,"procedure %s",proc_table[i].name); *out=STRVAL(buf); return 1; }
            snprintf(buf,128,"procedure"); *out=STRVAL(buf); return 1;
        }
        if (IS_CSET_fn(av)) {
            const char *cs = av.s ? av.s : "";
            const char *kname = icn_kw_cset_name(cs);
            if (kname) { *out = STRVAL(kname); return 1; }
            int cslen = (int)strlen(cs);
            char *outs = GC_malloc(cslen * 4 + 3);
            int o = 0;
            outs[o++] = '\'';
            for (int i = 0; i < cslen; i++) {
                unsigned char c = (unsigned char)cs[i];
                if (c == '\'') { outs[o++] = '\\'; outs[o++] = '\''; }
                else if (c < 0x20 || c == 0x7f) { o += snprintf(outs+o, 5, "\\x%02x", c); }
                else outs[o++] = (char)c;
            }
            outs[o++] = '\'';
            outs[o] = '\0';
            *out = STRVAL(outs); return 1;
        }
        if (IS_STR_fn(av) && av.s) {
            extern DESCR_t icn_proc_as_value(const char *);
            DESCR_t pv = icn_proc_as_value(av.s);
            if (pv.v == DT_S) {
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
    if (!strcmp(fn,"image") && nargs == 2) {
        DESCR_t av = args[0];
        if (IS_STR_fn(av) && av.s) {
            char *buf = GC_malloc(64);
            snprintf(buf, 64, "function %s", av.s);
            *out = STRVAL(buf); return 1;
        }
        if (av.v == DT_E) {
            char *buf = GC_malloc(128);
            for (int i=0;i<proc_count;i++)
                if (proc_table[i].entry_pc==(int)av.i)
                    { snprintf(buf,128,"procedure %s",proc_table[i].name); *out=STRVAL(buf); return 1; }
            snprintf(buf,128,"procedure"); *out=STRVAL(buf); return 1;
        }
        DESCR_t one_out = FAILDESCR;
        if (icn_try_call_builtin_by_name("image", args, 1, &one_out))
            { *out = one_out; return 1; }
        *out = FAILDESCR; return 1;
    }
    if (!strcmp(fn,"repl") && nargs == 2) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        int n=(int)to_int(args[1]); if(n<0)n=0;
        int sl=(int)strlen(s); char *buf=GC_malloc(sl*n+1); buf[0]='\0';
        for(int i=0;i<n;i++) memcpy(buf+i*sl,s,sl); buf[sl*n]='\0';
        *out = STRVAL(buf); return 1;
    }
    if (!strcmp(fn,"reverse") && nargs == 1) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        int sl=(int)strlen(s); char *buf=GC_malloc(sl+1);
        for(int i=0;i<sl;i++) buf[i]=s[sl-1-i]; buf[sl]='\0';
        *out = STRVAL(buf); return 1;
    }
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
    if (!strcmp(fn,"trim") && (nargs == 1 || nargs == 2)) {
        const char *s=VARVAL_fn(args[0]); if(!s)s="";
        const char *cset = " ";
        if (nargs == 2) { DESCR_t cv = args[1]; if (cv.v != DT_SNUL) { const char *cs = VARVAL_fn(cv); if (cs) cset = cs; } }
        int sl=(int)strlen(s);
        while (sl > 0 && strchr(cset, s[sl-1])) sl--;
        char *buf=GC_malloc(sl+1); memcpy(buf,s,sl); buf[sl]='\0';
        *out = STRVAL(buf); return 1;
    }
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
    if (!strcmp(fn,"detab") && nargs >= 1) {
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
        for (int i = 0; s[i]; i++) {
            if (s[i] == '\t') {
                int next = -1;
                for (int k = 0; k < nstops; k++) if (stops[k] > col+1) { next=stops[k]; break; }
                if (next < 0) {
                    int base = stops[nstops-1];
                    int beyond = col + 1 - base;
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
#define ICN_TONUM(av) (IS_REAL_fn(av) ? (av).r : IS_INT_fn(av) ? (double)(av).i : ((av).v==DT_S && (av).s ? strtod((av).s,NULL) : 0.0))
    if (!strcmp(fn,"sqrt") && nargs >= 1) {
        DESCR_t av = args[0];
        double v = ICN_TONUM(av);
        *out = REALVAL(sqrt(v)); return 1;
    }
#define ICN_MATH1(fname, cfn) \
    if (!strcmp(fn, fname) && nargs >= 1) { double _v = ICN_TONUM(args[0]); *out = REALVAL(cfn(_v)); return 1; }
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
    if (!strcmp(fn,"dtor") && nargs >= 1) { double v=ICN_TONUM(args[0]); *out=REALVAL(v*3.14159265358979323846/180.0); return 1; }
    if (!strcmp(fn,"rtod") && nargs >= 1) { double v=ICN_TONUM(args[0]); *out=REALVAL(v*180.0/3.14159265358979323846); return 1; }
    if (!strcmp(fn,"iand")  && nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(a&b); return 1; }
    if (!strcmp(fn,"ior")   && nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(a|b); return 1; }
    if (!strcmp(fn,"ixor")  && nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(a^b); return 1; }
    if (!strcmp(fn,"ishift")&& nargs==2) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r, b=IS_INT_fn(args[1])?args[1].i:(int64_t)args[1].r; *out=INTVAL(b>=0?a<<b:a>>(-b)); return 1; }
    if (!strcmp(fn,"icom")  && nargs==1) { int64_t a=IS_INT_fn(args[0])?args[0].i:(int64_t)args[0].r; *out=INTVAL(~a); return 1; }
#undef ICN_TONUM
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
    if (!strcmp(fn,"list") && nargs >= 0) {
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
    if (!strcmp(fn,"read") && nargs == 0) {
        char buf[4096];
        if (!fgets(buf, sizeof buf, stdin)) { *out = FAILDESCR; return 1; }
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
        char *r = GC_malloc(len + 1); memcpy(r, buf, len + 1);
        *out = STRVAL(r); return 1;
    }
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
    if (!strcmp(fn,"stop")) { exit(0); }
    extern const char *scan_subj;
    extern int         scan_pos;
    extern int         scan_depth;
    extern ScanEntry   scan_stack[];
    if (!strcmp(fn,"ICN_SCAN_PUSH") && nargs == 1) {
        const char *s;
        if (IS_REAL_fn(args[0])) { char _rb[64]; real_str(args[0].r,_rb,sizeof _rb); s = GC_strdup(_rb); }
        else { s = VARVAL_fn(args[0]); if (!s) s = ""; }
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
    if (!strcmp(fn,"any") && nargs >= 1 && (scan_pos > 0 || nargs >= 2)) {
        const char *cv = VARVAL_fn(args[0]); if (!cv) { *out = FAILDESCR; return 1; }
        if (nargs >= 2) {
            const char *s = VARVAL_fn(args[1]); if (!s) s = "";
            int slen = (int)strlen(s);
            int i1 = (nargs >= 3) ? (int)args[2].i : (scan_pos > 0 ? scan_pos : 1);
            int i2 = (nargs >= 4) ? (int)args[3].i : slen + 1;
            if (i1 <= 0 || i1 > slen) { *out = FAILDESCR; return 1; }
            if (i2 <= 0) i2 = slen + 1;
            int p = i1 - 1, end = i2 - 1;
            if (p < 0 || p >= slen || p >= end || !strchr(cv, s[p])) { *out = FAILDESCR; return 1; }
            *out = INTVAL(p + 2); return 1;
        }
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj), p0 = scan_pos - 1;
        if (p0 < 0 || p0 >= slen || !strchr(cv, scan_subj[p0])) { *out = FAILDESCR; return 1; }
        *out = INTVAL(p0 + 2); return 1;
    }
    if (!strcmp(fn,"many") && nargs >= 1 && (scan_pos > 0 || nargs >= 2)) {
        const char *cv = VARVAL_fn(args[0]); if (!cv) { *out = FAILDESCR; return 1; }
        if (nargs >= 2) {
            const char *s = VARVAL_fn(args[1]); if (!s) s = "";
            int slen = (int)strlen(s);
            int i1 = (nargs >= 3) ? (int)args[2].i : (scan_pos > 0 ? scan_pos : 1);
            int i2 = (nargs >= 4) ? (int)args[3].i : slen + 1;
            if (i1 <= 0 || i1 > slen) { *out = FAILDESCR; return 1; }
            if (i2 <= 0) i2 = slen + 1;
            int p = i1 - 1, end = i2 - 1;
            if (p < 0 || p >= slen || p >= end || !strchr(cv, s[p])) { *out = FAILDESCR; return 1; }
            while (p < end && p < slen && strchr(cv, s[p])) p++;
            *out = INTVAL(p + 1); return 1;
        }
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj), p0 = scan_pos - 1;
        if (p0 < 0 || p0 >= slen || !strchr(cv, scan_subj[p0])) { *out = FAILDESCR; return 1; }
        while (p0 < slen && strchr(cv, scan_subj[p0])) p0++;
        *out = INTVAL(p0 + 1); return 1;
    }
    if (!strcmp(fn,"upto") && nargs >= 1 && (scan_pos > 0 || nargs >= 2)) {
        const char *cv = VARVAL_fn(args[0]); if (!cv) { *out = FAILDESCR; return 1; }
        if (nargs >= 2) {
            const char *s = VARVAL_fn(args[1]); if (!s) s = "";
            int slen = (int)strlen(s);
            int i1 = (nargs >= 3) ? (int)args[2].i : (scan_pos > 0 ? scan_pos : 1);
            int i2 = (nargs >= 4) ? (int)args[3].i : slen + 1;
            if (i1 <= 0 || i1 > slen) { *out = FAILDESCR; return 1; }
            if (i2 <= 0) i2 = slen + 1;
            int p = i1 - 1, end = (i2 - 1 < slen ? i2 - 1 : slen);
            while (p < end && !strchr(cv, s[p])) p++;
            if (p >= end) { *out = FAILDESCR; return 1; }
            *out = INTVAL(p + 1); return 1;
        }
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj), p0 = scan_pos - 1;
        while (p0 < slen && !strchr(cv, scan_subj[p0])) p0++;
        if (p0 >= slen) { *out = FAILDESCR; return 1; }
        *out = INTVAL(p0 + 1); return 1;
    }
    if (!strcmp(fn,"tab") && nargs == 1 && scan_pos > 0) {
        if (!scan_subj) { *out = FAILDESCR; return 1; }
        int slen = (int)strlen(scan_subj);
        int target = (int)to_int(args[0]);
        if (target <= 0) target = slen + 1 + target;
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
    if (!strcmp(fn,"SIZE") && nargs == 1) {
        DESCR_t v = args[0];
        if (IS_FAIL_fn(v)) { *out = FAILDESCR; return 1; }
        if (v.v == DT_T)   { *out = INTVAL(v.tbl ? v.tbl->size : 0); return 1; }
        if (v.v == DT_A)   { *out = INTVAL(v.arr ? (v.arr->hi - v.arr->lo + 1) : 0); return 1; }
        if (v.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(v,"icn_type");
            if (tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0) {
                *out = INTVAL((int)FIELD_GET_fn(v,"frame_size").i); return 1;
            }
        }
        if (IS_INT_fn(v)||IS_REAL_fn(v)) { *out = INTVAL(0); return 1; }
        if (IS_CSET_fn(v)) {
            int klen = icn_kw_cset_len(v.s);
            *out = INTVAL(klen >= 0 ? klen : (v.s ? (long)strlen(v.s) : 0));
            return 1;
        }
        const char *s = VARVAL_fn(v); if (!s) { *out = INTVAL(0); return 1; }
        if (strchr(s,'\x01')) {
            long n=1; for(const char *p=s;*p;p++) if(*p=='\x01') n++;
            *out = INTVAL(n); return 1;
        }
        long len = IS_CSET_fn(v) ? (long)strlen(s) : (v.slen > 0 ? v.slen : (long)strlen(s));
        *out = INTVAL(len); return 1;
    }
    if (!strcmp(fn,"NONNULL") && nargs == 1) {
        DESCR_t v = args[0];
        if (IS_FAIL_fn(v))  { *out = FAILDESCR; return 1; }
        if (v.v == DT_SNUL) { *out = FAILDESCR; return 1; }
        if (v.v == DT_S && (!v.s || v.s[0]=='\0')) { *out = FAILDESCR; return 1; }
        *out = v; return 1;
    }
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
    if (!strcmp(fn,"ICN_SWAP_TOP2") && nargs == 2) {
        *out = args[0];
        return 1;
    }
    if (!strcmp(fn,"ICN_NULL") && nargs == 1) {
        DESCR_t v = args[0];
        if (IS_FAIL_fn(v))  { *out = FAILDESCR; return 1; }
        if (v.v == DT_SNUL) { *out = NULVCL; return 1; }
        if (v.v == DT_S && (!v.s || v.s[0]=='\0')) { *out = NULVCL; return 1; }
        *out = FAILDESCR; return 1;
    }
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
    if (!strcmp(fn,"key") && nargs == 1) {
        DESCR_t td = args[0];
        if (td.v != DT_T || !td.tbl) { *out=FAILDESCR; return 1; }
        for (int _bi=0;_bi<TABLE_BUCKETS;_bi++)
            if (td.tbl->buckets[_bi]) {
                *out = td.tbl->buckets[_bi]->key_descr; return 1;
            }
        *out = FAILDESCR; return 1;
    }
    if (!strcmp(fn,"push") && nargs >= 1) {
        DESCR_t ld = args[0];
        if (ld.v != DT_DATA) return 0;
        DESCR_t tag = FIELD_GET_fn(ld,"icn_type");
        if (!(tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)) return 0;
        int _nv = (nargs > 1) ? nargs - 1 : 1;
        for (int _pi = 0; _pi < _nv; _pi++) {
            DESCR_t vd = (nargs > 1) ? args[1 + _pi] : NULVCL;
            int n=(int)FIELD_GET_fn(ld,"frame_size").i;
            DESCR_t ea=FIELD_GET_fn(ld,"frame_elems");
            DESCR_t *old=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
            DESCR_t *nb=GC_malloc((n+1)*sizeof(DESCR_t));
            nb[0]=vd;
            if(old&&n>0) memcpy(nb+1,old,n*sizeof(DESCR_t));
            FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=nb});
            FIELD_SET_fn(ld,"frame_size",INTVAL(n+1));
        }
        *out = ld; return 1;
    }
    if (!strcmp(fn,"put") && nargs >= 1) {
        DESCR_t ld = args[0];
        if (ld.v != DT_DATA) return 0;
        DESCR_t tag = FIELD_GET_fn(ld,"icn_type");
        if (!(tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)) return 0;
        int _nv = (nargs > 1) ? nargs - 1 : 1;
        for (int _pi = 0; _pi < _nv; _pi++) {
            DESCR_t vd = (nargs > 1) ? args[1 + _pi] : NULVCL;
            int n=(int)FIELD_GET_fn(ld,"frame_size").i;
            DESCR_t ea=FIELD_GET_fn(ld,"frame_elems");
            DESCR_t *old=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
            DESCR_t *nb=GC_malloc((n+1)*sizeof(DESCR_t));
            if(old&&n>0) memcpy(nb,old,n*sizeof(DESCR_t));
            nb[n]=vd;
            FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=nb});
            FIELD_SET_fn(ld,"frame_size",INTVAL(n+1));
        }
        *out = ld; return 1;
    }
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
    if (!strcmp(fn,"pop") && nargs == 1) {
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
    if (!strcmp(fn,"FIELD_GET") && nargs == 2) {
        DESCR_t obj  = args[0];
        DESCR_t fname_d = args[1];
        const char *fname = VARVAL_fn(fname_d);
        if (!fname || obj.v != DT_DATA) { *out=FAILDESCR; return 1; }
        extern DESCR_t sc_dat_field_get(const char *field, DESCR_t obj);
        *out = sc_dat_field_get(fname, obj); return 1;
    }
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
    if (!strcmp(fn,"MAKELIST")) {
        static int icnlist_reg3 = 0;
        if (!icnlist_reg3) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_reg3 = 1; }
        DESCR_t *elems = GC_malloc((nargs>0?nargs:1)*sizeof(DESCR_t));
        for (int _j=0;_j<nargs;_j++) elems[_j]=args[_j];
        DESCR_t eptr; eptr.v=DT_DATA; eptr.slen=0; eptr.ptr=(void*)elems;
        *out = DATCON_fn("icnlist", eptr, INTVAL(nargs), STRVAL("list")); return 1;
    }
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
        if (idx >= 0 && idx < RAKU_FH_MAX) raku_fh_name[idx] = GC_strdup(path);
        *out = FHVAL(idx); return 1;
    }
    if (!strcmp(fn,"close") && nargs == 1) {
        if (IS_FH_fn(args[0]) || IS_INT_fn(args[0])) {
            int idx = (int)args[0].i;
            FILE *fp = raku_fh_get(idx);
            if (fp && idx > 2) { fclose(fp); raku_fh_free(idx); }
        }
        *out = args[0]; return 1;
    }
    if (!strcmp(fn,"read") && nargs == 1) {
        FILE *fp = (IS_FH_fn(args[0]) || IS_INT_fn(args[0])) ? raku_fh_get((int)args[0].i) : NULL;
        if (!fp) { *out = FAILDESCR; return 1; }
        char buf[4096];
        if (!fgets(buf, sizeof buf, fp)) { *out = FAILDESCR; return 1; }
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
        *out = STRVAL(GC_strdup(buf)); return 1;
    }
    if (!strcmp(fn,"reads") && nargs == 2) {
        FILE *fp = (IS_FH_fn(args[0]) || IS_INT_fn(args[0])) ? raku_fh_get((int)args[0].i) : NULL;
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
    if (!strcmp(fn,"ASGN") && nargs == 2) {
        DESCR_t rhs = args[0];
        if (IS_FAIL_fn(rhs)) { *out = FAILDESCR; return 1; }
        DESCR_t lref = args[1];
        if (lref.v == DT_S && lref.s) NV_SET_fn(lref.s, rhs);
        *out = rhs; return 1;
    }
    if (!strcmp(fn,"variable") && nargs == 1) {
        const char *vname = (args[0].v == DT_S || args[0].v == DT_SNUL) ? args[0].s : NULL;
        if (!vname) { *out = FAILDESCR; return 1; }
        DESCR_t v = NV_GET_fn(vname);
        *out = IS_FAIL_fn(v) ? FAILDESCR : v; return 1;
    }
#define _OPCOERCE(d) do { \
        if (!IS_INT_fn(d) && !IS_REAL_fn(d)) { \
            const char *_s = VARVAL_fn(d); \
            if (_s && *_s) { char *_e=NULL; long long _iv=strtoll(_s,&_e,10); \
                if (_e && !*_e){(d)=INTVAL(_iv);} \
                else {double _rv=strtod(_s,&_e); \
                      if(_e && !*_e){(d)=REALVAL(_rv);}else{*out=FAILDESCR;return 1;}} \
            } else { *out=FAILDESCR; return 1; } } } while(0)
#define _NUMREL(op) do { DESCR_t _l=args[0],_r=args[1]; _OPCOERCE(_l); _OPCOERCE(_r); \
        double _lv2=IS_REAL_fn(_l)?_l.r:(double)_l.i, _rv2=IS_REAL_fn(_r)?_r.r:(double)_r.i; \
        *out=(_lv2 op _rv2)?_r:FAILDESCR; return 1; } while(0)
#define _STRREL(op) do { DESCR_t _l=args[0],_r=args[1]; \
        const char *_ls=VARVAL_fn(_l); if(!_ls)_ls=""; \
        const char *_rs=VARVAL_fn(_r); if(!_rs)_rs=""; \
        int _cmp=strcmp(_ls,_rs); *out=(_cmp op 0)?_r:FAILDESCR; return 1; } while(0)
    if (nargs == 1) {
        DESCR_t a = args[0];
        if (IS_FAIL_fn(a)) { *out = FAILDESCR; return 1; }
        if (fn[0]=='+' && fn[1]=='\0') {
            if (IS_INT_fn(a)||IS_REAL_fn(a)) { *out=a; return 1; }
            const char *s=VARVAL_fn(a); if(!s||!*s){*out=FAILDESCR;return 1;}
            char *e=NULL; long long iv=strtoll(s,&e,10); if(e&&*e=='\0'){*out=INTVAL(iv);return 1;}
            double dv=strtod(s,&e); if(e&&*e=='\0'){*out=REALVAL(dv);return 1;}
            *out=FAILDESCR; return 1;
        }
        if (fn[0]=='-' && fn[1]=='\0') {
            _OPCOERCE(a);
            *out = IS_REAL_fn(a) ? REALVAL(-a.r) : INTVAL(-a.i); return 1;
        }
        if (fn[0]=='*' && fn[1]=='\0') {
            if (IS_INT_fn(a)||IS_REAL_fn(a)) { *out=INTVAL(1); return 1; }
            if (a.v==DT_DATA && a.u && a.u->type) { *out=INTVAL(a.u->type->nfields); return 1; }
            const char *s=VARVAL_fn(a); *out=INTVAL(s?(long long)strlen(s):0LL); return 1;
        }
        if (fn[0]=='!' && fn[1]=='\0') {
            if (IS_INT_fn(a)||IS_REAL_fn(a)) { *out=a; return 1; }
            const char *s=VARVAL_fn(a);
            if (s&&*s) { char *ch=GC_malloc(2); ch[0]=s[0]; ch[1]='\0'; *out=STRVAL(ch); return 1; }
            *out=FAILDESCR; return 1;
        }
        if (fn[0]=='/' && fn[1]=='\0') {
            if (IS_INT_fn(a))  { *out=(a.i==0)?a:FAILDESCR; return 1; }
            if (IS_REAL_fn(a)) { *out=(a.r==0.0)?a:FAILDESCR; return 1; }
            *out=FAILDESCR; return 1;
        }
        if (fn[0]=='\\' && fn[1]=='\0') {
            *out=(a.v==DT_SNUL)?FAILDESCR:a; return 1;
        }
        if (fn[0]=='~' && fn[1]=='\0') {
            const char *s=NULL;
            if (IS_INT_fn(a)) { char *nb=GC_malloc(32); snprintf(nb,32,"%lld",(long long)a.i); s=nb; }
            else if (IS_REAL_fn(a)) { char *nb=GC_malloc(64); real_str(a.r,nb,64); s=nb; }
            else { s=VARVAL_fn(a); }
            if(!s) s="";
            unsigned char in_set[256]={0}; for(const char *p=s;*p;p++) in_set[(unsigned char)*p]=1;
            char *buf=GC_malloc(256); int n=0;
            for(int c=1;c<256;c++) if(!in_set[c]) buf[n++]=(char)c; buf[n]='\0';
            *out=STRVAL(buf); return 1;
        }
        if (fn[0]=='?' && fn[1]=='\0') {
            if (IS_INT_fn(a)) { *out=(a.i>0)?INTVAL((long long)(rand()%(int)a.i)+1):FAILDESCR; return 1; }
            if (IS_REAL_fn(a)) { *out=REALVAL((double)rand()/RAND_MAX*a.r); return 1; }
            const char *s=VARVAL_fn(a);
            if (s&&*s) { int n=(int)strlen(s); char *ch=GC_malloc(2); ch[0]=s[rand()%n]; ch[1]='\0'; *out=STRVAL(ch); return 1; }
            *out=FAILDESCR; return 1;
        }
    }
    if (nargs == 2) {
        DESCR_t l=args[0], r=args[1];
        if (IS_FAIL_fn(l)||IS_FAIL_fn(r)) { *out=FAILDESCR; return 1; }
        if (fn[0]=='+' && fn[1]=='\0') { _OPCOERCE(l); _OPCOERCE(r); *out=add(l,r); return 1; }
        if (fn[0]=='-' && fn[1]=='\0') { _OPCOERCE(l); _OPCOERCE(r); *out=sub(l,r); return 1; }
        if (fn[0]=='*' && fn[1]=='\0') { _OPCOERCE(l); _OPCOERCE(r); *out=mul(l,r); return 1; }
        if (fn[0]=='/' && fn[1]=='\0') { _OPCOERCE(l); _OPCOERCE(r); *out=DIVIDE_fn(l,r); return 1; }
        if (fn[0]=='%' && fn[1]=='\0') {
            _OPCOERCE(l); _OPCOERCE(r);
            long li=IS_INT_fn(l)?l.i:(long)l.r, ri=IS_INT_fn(r)?r.i:(long)r.r;
            *out=ri?INTVAL(li%ri):FAILDESCR; return 1;
        }
        if (fn[0]=='^' && fn[1]=='\0') {
            _OPCOERCE(l); _OPCOERCE(r);
            if (IS_INT_fn(l)&&IS_INT_fn(r)&&r.i>=0) {
                long long base=l.i, res=1; for(int k=0;k<(int)r.i;k++) res*=base;
                *out=INTVAL(res); return 1;
            }
            if (IS_INT_fn(l)&&IS_INT_fn(r)&&r.i<0) {
                double rv=pow((double)l.i,(double)r.i);
                *out=INTVAL((long long)rv); return 1;
            }
            double base=IS_REAL_fn(l)?l.r:(double)l.i, exp=IS_REAL_fn(r)?r.r:(double)r.i;
            *out=(DESCR_t){.v=DT_R,.r=pow(base,exp)}; return 1;
        }
        if (!strcmp(fn,"<"))  _NUMREL(<);
        if (!strcmp(fn,"<=")) _NUMREL(<=);
        if (!strcmp(fn,">"))  _NUMREL(>);
        if (!strcmp(fn,">=")) _NUMREL(>=);
        if (!strcmp(fn,"="))  _NUMREL(==);
        if (!strcmp(fn,"~=")) _NUMREL(!=);
        if (!strcmp(fn,"<<"))  _STRREL(<);
        if (!strcmp(fn,"<<=")) _STRREL(<=);
        if (!strcmp(fn,">>"))  _STRREL(>);
        if (!strcmp(fn,">>=")) _STRREL(>=);
        if (!strcmp(fn,"=="))  _STRREL(==);
        if (!strcmp(fn,"~==")) _STRREL(!=);
        if (!strcmp(fn,"===")) {
            extern int icn_descr_identical(DESCR_t, DESCR_t);
            *out=icn_descr_identical(l,r)?r:FAILDESCR; return 1;
        }
        if (!strcmp(fn,"~===")) {
            extern int icn_descr_identical(DESCR_t, DESCR_t);
            *out=icn_descr_identical(l,r)?FAILDESCR:r; return 1;
        }
        if (!strcmp(fn,"[]")) {
            *out=subscript_get(l,r); return 1;
        }
        if (!strcmp(fn,"++") || !strcmp(fn,"--") || !strcmp(fn,"**")) {
            char _lbuf[64], _rbuf[64];
            const char *la, *ra;
            if (IS_INT_fn(l))       { snprintf(_lbuf,sizeof _lbuf,"%lld",(long long)l.i); la=_lbuf; }
            else if (IS_REAL_fn(l)) { real_str(l.r,_lbuf,sizeof _lbuf); la=_lbuf; }
            else                    { la=VARVAL_fn(l); if(!la) la=""; }
            if (IS_INT_fn(r))       { snprintf(_rbuf,sizeof _rbuf,"%lld",(long long)r.i); ra=_rbuf; }
            else if (IS_REAL_fn(r)) { real_str(r.r,_rbuf,sizeof _rbuf); ra=_rbuf; }
            else                    { ra=VARVAL_fn(r); if(!ra) ra=""; }
            if (fn[0]=='+') *out=CSETVAL(icn_cset_canonical(icn_cset_union(la,ra)));
            else if (fn[1]=='-') *out=CSETVAL(icn_cset_canonical(icn_cset_diff(la,ra)));
            else *out=CSETVAL(icn_cset_canonical(icn_cset_inter(la,ra)));
            return 1;
        }
    }
#undef _OPCOERCE
#undef _NUMREL
#undef _STRREL
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_call_builtin(tree_t *call, DESCR_t *args, int nargs) {
    if (!call || call->n < 1 || !call->c[0]) return NULVCL;
    const char *fn = call->c[0]->v.sval;
    if (!fn) return NULVCL;
    {
        DESCR_t __rk_d;
        if (raku_try_call_builtin(call, &__rk_d)) return __rk_d;
    }
    {
        DESCR_t __sc_d;
        if (scan_try_call_builtin(call, args, nargs, &__sc_d)) return __sc_d;
    }
    {
        DESCR_t __nb_d;
        if (icn_try_call_builtin_by_name(fn, args, nargs, &__nb_d)) return __nb_d;
    }
    DESCR_t a0 = nargs > 0 ? args[0] : NULVCL;
    DESCR_t a1 = nargs > 1 ? args[1] : NULVCL;
    for (int i = 0; i < proc_count; i++) {
        if (!strcmp(proc_table[i].name, fn))
            return proc_table_call(i, args, nargs);
    }
    {
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
            tree_t   leafbufs[16];
            tree_t  *kidsbuf[16 + 1];
            tree_t **kids = kidsbuf;
            tree_t  *leaves = leafbufs;
            if (nargs > 16) {
                kids   = (tree_t **)GC_malloc(sizeof(tree_t *) * (size_t)(nargs + 1));
                leaves = (tree_t  *)GC_malloc(sizeof(tree_t  ) * (size_t)nargs);
            }
            kids[0] = call->c[0];
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
    return interp_eval(call);
}
long g_icn_error  = 0;
long g_icn_trace  = 0;
long g_icn_dump   = 0;
long g_icn_random = 0;
int  g_icn_jcon   = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
    if (!strcmp(kw,"error"))  { g_icn_error  = to_int(val); return 1; }
    if (!strcmp(kw,"trace"))  { g_icn_trace  = to_int(val); return 1; }
    if (!strcmp(kw,"dump"))   { g_icn_dump   = to_int(val); return 1; }
    if (!strcmp(kw,"random")) {
        g_icn_random = to_int(val);
        bb_icn_rnd_seed = (unsigned long)g_icn_random;
        return 1;
    }
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
    return 1;
}
#define ICN_KW_CSET_MAX 16
static struct { const char *ptr; const char *name; int len; } g_kw_cset_names[ICN_KW_CSET_MAX];
static int g_kw_cset_count = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t make_kw_cset(const char *chars, const char *kw_name) {
    for (int i = 0; i < g_kw_cset_count; i++)
        if (!strcmp(g_kw_cset_names[i].name, kw_name))
            return CSETVAL(g_kw_cset_names[i].ptr);
    const char *arena = icn_cset_canonical(chars);
    char *stable = GC_strdup(arena);
    int clen = (int)strlen(stable);
    if (g_kw_cset_count < ICN_KW_CSET_MAX) {
        g_kw_cset_names[g_kw_cset_count].ptr  = stable;
        g_kw_cset_names[g_kw_cset_count].name = kw_name;
        g_kw_cset_names[g_kw_cset_count].len  = clen;
        g_kw_cset_count++;
    }
    return CSETVAL(stable);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_kw_cset_name(const char *ptr) {
    for (int i = 0; i < g_kw_cset_count; i++)
        if (g_kw_cset_names[i].ptr == ptr) return g_kw_cset_names[i].name;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_kw_cset_len(const char *ptr) {
    for (int i = 0; i < g_kw_cset_count; i++)
        if (g_kw_cset_names[i].ptr == ptr) return g_kw_cset_names[i].len;
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_kw_read(const char *kw) {
    if (!kw) return FAILDESCR;
    if (!strcmp(kw,"pos"))     return INTVAL(scan_pos);
    if (!strcmp(kw,"subject")) return scan_subj ? STRVAL(scan_subj) : STRVAL("");
    if (!strcmp(kw,"e"))   return REALVAL(2.718281828459045);
    if (!strcmp(kw,"pi"))  return REALVAL(3.141592653589793);
    if (!strcmp(kw,"phi")) return REALVAL(1.618033988749895);
    if (!strcmp(kw,"lcase"))   return make_kw_cset("abcdefghijklmnopqrstuvwxyz","&lcase");
    if (!strcmp(kw,"ucase"))   return make_kw_cset("ABCDEFGHIJKLMNOPQRSTUVWXYZ","&ucase");
    if (!strcmp(kw,"letters")) return make_kw_cset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz","&letters");
    if (!strcmp(kw,"digits"))  return make_kw_cset("0123456789","&digits");
    if (!strcmp(kw,"ascii")) {
        static const char *cs = NULL;
        if (!cs) {
            static char ascii_str[128];
            for (int c=1;c<128;c++) ascii_str[c-1]=(char)c; ascii_str[127]='\0';
            const char *tmp = icn_cset_canonical(ascii_str);
            int tlen = (int)strlen(tmp);
            char *stable = GC_malloc(tlen + 2);
            stable[0] = '\0'; memcpy(stable+1, tmp, tlen+1);
            if (g_kw_cset_count < ICN_KW_CSET_MAX) {
                g_kw_cset_names[g_kw_cset_count].ptr  = stable;
                g_kw_cset_names[g_kw_cset_count].name = "&ascii";
                g_kw_cset_names[g_kw_cset_count].len  = 128;
                g_kw_cset_count++;
            }
            cs = stable;
        }
        return CSETVAL(cs);
    }
    if (!strcmp(kw,"cset")) {
        static const char *cs = NULL;
        if (!cs) {
            static char cset_str[256];
            for (int c=1;c<256;c++) cset_str[c-1]=(char)c; cset_str[255]='\0';
            const char *tmp = icn_cset_canonical(cset_str);
            int tlen = (int)strlen(tmp);
            char *stable = GC_malloc(tlen + 2);
            stable[0] = '\0'; memcpy(stable+1, tmp, tlen+1);
            if (g_kw_cset_count < ICN_KW_CSET_MAX) {
                g_kw_cset_names[g_kw_cset_count].ptr  = stable;
                g_kw_cset_names[g_kw_cset_count].name = "&cset";
                g_kw_cset_names[g_kw_cset_count].len  = 256;
                g_kw_cset_count++;
            }
            cs = stable;
        }
        return CSETVAL(cs);
    }
    { extern long g_icn_error, g_icn_trace, g_icn_dump, g_icn_random;
      if (!strcmp(kw,"error"))  return INTVAL(g_icn_error);
      if (!strcmp(kw,"trace"))  return INTVAL(g_icn_trace);
      if (!strcmp(kw,"dump"))   return INTVAL(g_icn_dump);
      if (!strcmp(kw,"random")) return INTVAL(g_icn_random);
    }
    if (!strcmp(kw,"col"))     return INTVAL(0);
    if (!strcmp(kw,"row"))     return INTVAL(0);
    if (!strcmp(kw,"x"))       return INTVAL(0);
    if (!strcmp(kw,"y"))       return INTVAL(0);
    { extern int frame_depth; if (!strcmp(kw,"level")) return INTVAL(frame_depth); }
    if (!strcmp(kw,"lpress"))   return INTVAL(-1);
    if (!strcmp(kw,"mpress"))   return INTVAL(-2);
    if (!strcmp(kw,"rpress"))   return INTVAL(-3);
    if (!strcmp(kw,"lrelease")) return INTVAL(-4);
    if (!strcmp(kw,"mrelease")) return INTVAL(-5);
    if (!strcmp(kw,"rrelease")) return INTVAL(-6);
    if (!strcmp(kw,"ldrag"))    return INTVAL(-7);
    if (!strcmp(kw,"mdrag"))    return INTVAL(-8);
    if (!strcmp(kw,"rdrag"))    return INTVAL(-9);
    if (!strcmp(kw,"resize"))   return INTVAL(-10);
    if (!strcmp(kw,"null"))    return NULVCL;
    if (!strcmp(kw,"fail"))    return FAILDESCR;
    if (!strcmp(kw,"window"))  return NULVCL;
    if (!strcmp(kw,"input"))   { raku_fh_ensure_init(); return FHVAL(0); }
    if (!strcmp(kw,"output"))  { raku_fh_ensure_init(); return FHVAL(1); }
    if (!strcmp(kw,"errout"))  { raku_fh_ensure_init(); return FHVAL(2); }
    if (!strcmp(kw,"current")) return STRVAL("co-expression_1(0)");
    if (!strcmp(kw,"main"))    return STRVAL("co-expression_1(0)");
    if (!strcmp(kw,"source"))  return STRVAL("co-expression_1(0)");
    { time_t t = time(NULL); struct tm *tm = localtime(&t);
      if (!strcmp(kw,"date")) {
          char *buf = GC_malloc(16);
          snprintf(buf,16,"%04d/%02d/%02d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday);
          return STRVAL(buf);
      }
      if (!strcmp(kw,"dateline")) {
          char *buf = GC_malloc(64);
          strftime(buf,64,"%A, %B %e, %Y  %H:%M:%S",tm);
          return STRVAL(buf);
      }
      if (!strcmp(kw,"clock")) {
          char *buf = GC_malloc(16);
          snprintf(buf,16,"%02d:%02d:%02d",tm->tm_hour,tm->tm_min,tm->tm_sec);
          return STRVAL(buf);
      }
      if (!strcmp(kw,"time")) {
          char *buf = GC_malloc(16);
          snprintf(buf,16,"%d",(int)(clock()*1000/CLOCKS_PER_SEC));
          return STRVAL(buf);
      }
    }
    if (!strcmp(kw,"version")) return STRVAL("Jcon Version 2.2");
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t interp_eval(tree_t *e)
{
    NO_AST_WALK_GUARD("interp_eval");
    if (!e) return NULVCL;
    if (frame_depth > 0) {
        {
            static int rs24_diag_init = 0;
            static int rs24_diag_on = 0;
            static unsigned long rs24_diag_hits[TT_KIND_COUNT];
            if (!rs24_diag_init) {
                rs24_diag_init = 1;
                rs24_diag_on = (getenv("RS24_DIAG") != NULL);
                if (rs24_diag_on) {
                    extern void rs24_diag_dump(void);
                    atexit(rs24_diag_dump);
                }
            }
            if (rs24_diag_on && (unsigned)e->t < (unsigned)TT_KIND_COUNT) {
                rs24_diag_hits[e->t]++;
            }
            extern unsigned long *rs24_diag_hits_ptr;
            rs24_diag_hits_ptr = rs24_diag_hits;
        }
        switch (e->t) {
        case TT_VAR: {
            if (e->v.sval && e->v.sval[0] == '&') {
                return icn_kw_read(e->v.sval + 1);
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
            if (e->n < 1) return NULVCL;
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
                DESCR_t *vals = (DESCR_t *)GC_malloc((size_t)nargs * sizeof(DESCR_t));
                for (int _wi = 0; _wi < nargs; _wi++) {
                    vals[_wi] = interp_eval(e->c[1+_wi]);
                    if (IS_FAIL_fn(vals[_wi])) return FAILDESCR;
                }
                int start = 0;
                FILE *dest = stdout;
                if (nargs > 0 && IS_FH_fn(vals[0])) {
                    FILE *fp = raku_fh_get((int)vals[0].i);
                    if (fp) dest = fp;
                    start = 1;
                }
                DESCR_t last = start > 0 ? vals[0] : NULVCL;
                for (int _wi = start; _wi < nargs; _wi++) {
                    DESCR_t a = vals[_wi];
                    last = a;
                    if (a.v == DT_SNUL) continue;
                    if (IS_INT_fn(a)) fprintf(dest, "%lld",(long long)a.i);
                    else if (IS_REAL_fn(a)) { char _rb[64]; fprintf(dest, "%s",real_str(a.r,_rb,sizeof _rb)); }
                    else if (IS_CSET_fn(a)) { if (a.s) fwrite(a.s, 1, strlen(a.s), dest); }
                    else { const char *s=VARVAL_fn(a); if (s) fputs(s, dest); }
                }
                fputc('\n', dest);
                return last;
            }
            if (!strcmp(fn,"read") && nargs == 0) {
                char buf[4096];
                if (!fgets(buf, sizeof buf, stdin)) return FAILDESCR;
                size_t len = strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
                if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
                char *r = GC_malloc(len + 1); memcpy(r, buf, len + 1);
                return STRVAL(r);
            }
            if (!strcmp(fn,"reads") && nargs == 1) {
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
                if(nargs<2) {
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
            for (int i=0; i<proc_count; i++) {
                if (!strcmp(proc_table[i].name,fn)) {
                    DESCR_t args[FRAME_SLOT_MAX];
                    for (int j=0; j<nargs&&j<FRAME_SLOT_MAX; j++)
                        args[j]=interp_eval(e->c[1+j]);
                    return proc_table_call(i,args,nargs);
                }
            }
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
#define HS '\x02'
#define HK '\x03'
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
            {
                DESCR_t __rk_d;
                if (raku_try_call_builtin(e, &__rk_d)) return __rk_d;
            }
            if (!strcmp(fn,"table") && nargs <= 2) {
                TBBLK_t *tbl = table_new();
                if (nargs == 1) {
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
                DESCR_t td = interp_eval(e->c[1]);
                if (td.v != DT_T) return FAILDESCR;
                DESCR_t kd;
                if (nargs >= 2) kd = interp_eval(e->c[2]);
                else            kd = NULVCL;
                char kb[64]; const char *ks;
                if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
                else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
                else                     { ks = VARVAL_fn(kd); if (!ks) ks=""; }
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
                DESCR_t td = interp_eval(e->c[1]);
                if (td.v != DT_T) return FAILDESCR;
                DESCR_t kd;
                if (nargs >= 2) kd = interp_eval(e->c[2]);
                else            kd = NULVCL;
                char kb[64]; const char *ks;
                if (IS_INT_fn(kd))       { snprintf(kb,sizeof kb,"%lld",(long long)kd.i); ks=kb; }
                else if (IS_REAL_fn(kd)) { snprintf(kb,sizeof kb,"%g",kd.r); ks=kb; }
                else                     { ks = VARVAL_fn(kd); if (!ks) ks=""; }
                if (!table_has(td.tbl, ks)) return FAILDESCR;
                return table_get(td.tbl, ks);
            }
            if (!strcmp(fn,"key") && nargs == 1) {
                DESCR_t td = interp_eval(e->c[1]);
                if (td.v != DT_T || !td.tbl) return FAILDESCR;
                for (int _bi = 0; _bi < TABLE_BUCKETS; _bi++)
                    if (td.tbl->buckets[_bi])
                        return td.tbl->buckets[_bi]->key_descr;
                return FAILDESCR;
            }
            if (!strcmp(fn,"integer") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_INT_fn(av)) return av;
                if (IS_REAL_fn(av)) return INTVAL((long long)av.r);
                const char *s = VARVAL_fn(av); if (!s) return FAILDESCR;
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
            if ((!strcmp(fn,"push")||!strcmp(fn,"put")||!strcmp(fn,"get")||!strcmp(fn,"pull")) && nargs >= 1) {
                DESCR_t ld = interp_eval(e->c[1]);
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
                if (!strcmp(fn,"get") && nargs == 1) {
                    if (ld.v != DT_DATA) return FAILDESCR;
                    DESCR_t ea = FIELD_GET_fn(ld,"frame_elems");
                    int n = (int)FIELD_GET_fn(ld,"frame_size").i;
                    DESCR_t *arr = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
                    if (!arr || n <= 0) return FAILDESCR;
                    DESCR_t ret = arr[0];
                    FIELD_SET_fn(ld,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=arr+1});
                    FIELD_SET_fn(ld,"frame_size",INTVAL(n-1));
                    if (e->c[1]->t==TT_VAR) {
                        int sl=(int)e->c[1]->v.ival;
                        if(sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=ld;
                    }
                    return ret;
                }
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
                const char *cset = " ";
                if (nargs == 2) {
                    DESCR_t cv = interp_eval(e->c[2]);
                    if (cv.v != DT_SNUL) {
                        const char *cs = VARVAL_fn(cv);
                        if (cs) cset = cs;
                    }
                }
                if (g_lang == 1 || nargs == 2) {
                    int sl=(int)strlen(s);
                    while (sl > 0 && strchr(cset, s[sl-1])) sl--;
                    char *buf=GC_malloc(sl+1); memcpy(buf,s,sl); buf[sl]='\0';
                    return STRVAL(buf);
                } else {
                    while(*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
                    size_t len=strlen(s);
                    while(len>0&&(s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r')) len--;
                    char *buf=GC_malloc(len+1); memcpy(buf,s,len); buf[len]='\0';
                    return STRVAL(buf);
                }
            }
            if (!strcmp(fn,"type") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                const char *t;
                if (IS_INT_fn(av))       t="integer";
                else if (IS_REAL_fn(av)) t="real";
                else if (av.v==DT_T)     t="table";
                else if (av.v==DT_A)     t="list";
                else if (av.v==DT_DATA)  {
                    DESCR_t tag = FIELD_GET_fn(av,"icn_type");
                    t = (tag.v==DT_S && tag.s) ? tag.s : "record";
                }
                else if (IS_CSET_fn(av)) t="cset";
                else if (av.v==DT_SNUL)  t="null";
                else t="string";
                return STRVAL(t);
            }
            if (!strcmp(fn,"image") && nargs == 0) {
                return STRVAL("&null");
            }
            if (!strcmp(fn,"image") && nargs == 1) {
                DESCR_t av = interp_eval(e->c[1]);
                if (IS_FAIL_fn(av)) return FAILDESCR;
                char *buf = GC_malloc(256);
                if (av.v == DT_SNUL)     return STRVAL("&null");
                if (IS_INT_fn(av)) {
                    int idx=(int)av.i;
                    if(idx>=0&&idx<RAKU_FH_MAX&&raku_fh_name[idx]){snprintf(buf,256,"file(%s)",raku_fh_name[idx]);return STRVAL(buf);}
                    snprintf(buf,256,"%lld",(long long)av.i); return STRVAL(buf);
                }
                if (IS_REAL_fn(av))      { real_str(av.r,buf,128); return STRVAL(buf); }
                if (av.v==DT_T)          { snprintf(buf,128,"table(%d)",av.tbl?av.tbl->size:0); return STRVAL(buf); }
                if (av.v==DT_DATA && av.u) {
                    const char *tname=av.u->type?av.u->type->name:"record";
                    if(strcmp(tname,"icnlist")==0){int cnt=(av.u->type&&av.u->type->nfields>=2&&av.u->fields)?(int)av.u->fields[1].i:0;snprintf(buf,128,"list(%d)",cnt);return STRVAL(buf);}
                    snprintf(buf,256,"record(%s)",tname); return STRVAL(buf);
                }
                if (av.v==DT_DATA)       { return STRVAL("record"); }
                if (av.v==DT_E) {
                    for(int i=0;i<proc_count;i++) if(proc_table[i].entry_pc==(int)av.i){snprintf(buf,128,"procedure %s",proc_table[i].name);return STRVAL(buf);}
                    return STRVAL("procedure");
                }
                const char *s=VARVAL_fn(av); if (!s) s = "";
                int sl = (int)strlen(s);
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
                        DESCR_t *new_elems = (DESCR_t *)GC_malloc((size_t)(n > 0 ? n : 1) * sizeof(DESCR_t));
                        if (src_elems && n > 0) memcpy(new_elems, src_elems, (size_t)n * sizeof(DESCR_t));
                        DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void *)new_elems;
                        return DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
                    }
                }
                return src;
            }
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
            if (!strcmp(fn,"seq") && nargs >= 1) {
                DESCR_t start = interp_eval(e->c[1]);
                return IS_INT_fn(start) ? start : INTVAL(1);
            }
            if ((!strcmp(fn,"sort") && nargs == 1) || (!strcmp(fn,"sortf") && nargs == 2)) {
                DESCR_t ld = interp_eval(e->c[1]);
                if (ld.v != DT_DATA) return FAILDESCR;
                DESCR_t ea = FIELD_GET_fn(ld,"frame_elems");
                int n = (int)FIELD_GET_fn(ld,"frame_size").i;
                if (n <= 0) return ld;
                DESCR_t *arr = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
                if (!arr) return ld;
                DESCR_t *sorted = GC_malloc(n * sizeof(DESCR_t));
                memcpy(sorted, arr, n * sizeof(DESCR_t));
                int field_idx = (!strcmp(fn,"sortf") && nargs == 2)
                    ? (int)to_int(interp_eval(e->c[2])) - 1 : -1;
                for (int _i = 1; _i < n; _i++) {
                    DESCR_t key = sorted[_i]; int _j = _i - 1;
                    while (_j >= 0) {
                        DESCR_t a = sorted[_j], b = key;
                        if (field_idx >= 0) {
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
                DESCR_t res = ld;
                FIELD_SET_fn(res,"frame_elems",(DESCR_t){.v=DT_DATA,.ptr=sorted});
                FIELD_SET_fn(res,"frame_size",INTVAL(n));
                return res;
            }
            {
                ScDatType *_dt = sc_dat_find_type(fn);
                if (_dt) {
                    DESCR_t _args[FRAME_SLOT_MAX];
                    for (int _j = 0; _j < nargs && _j < FRAME_SLOT_MAX; _j++)
                        _args[_j] = interp_eval(e->c[1+_j]);
                    return sc_dat_construct(_dt, _args, nargs);
                }
            }
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
            if (scan_depth > 0 && !frame_depth && e->v.sval[0] == '&') {
                const char *kw = e->v.sval + 1;
                if (!strcmp(kw,"pos"))     return INTVAL(scan_pos);
                if (!strcmp(kw,"subject")) return scan_subj ? STRVAL(scan_subj) : NULVCL;
            }
            { DESCR_t _sv; if (shadow_get(e->v.sval, &_sv)) return _sv; }
            DESCR_t _vr = NV_GET_fn(e->v.sval);
            if (!IS_NULL(_vr)) return _vr;
            if (FNCEX_fn(e->v.sval)) {
                DESCR_t _fr = APPLY_fn(e->v.sval, NULL, 0);
                if (!IS_FAIL_fn(_fr) && !IS_NULL(_fr)) return _fr;
            }
            return _vr;
        }
        return NULVCL;
    case TT_KEYWORD: {
        if (!e->v.sval || !*e->v.sval) return NULVCL;
        char uc[64]; int _ki;
        for (_ki = 0; e->v.sval[_ki] && _ki < 63; _ki++)
            uc[_ki] = toupper((unsigned char)e->v.sval[_ki]);
        uc[_ki] = '\0';
        return NV_GET_fn(uc);
    }
    case TT_INTERROGATE: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        return NULVCL;
    }
    case TT_NAME: {
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
    case TT_RETURN: {
        if (frame_depth > 0) {
            FRAME.return_val = (e->n > 0)
                ? interp_eval(e->c[0]) : NULVCL;
            FRAME.returning = 1;
            return FRAME.return_val;
        }
        return (e->n > 0) ? interp_eval(e->c[0]) : NULVCL;
    }
    case TT_PROC_FAIL: {
        if (frame_depth > 0) {
            FRAME.return_val = FAILDESCR;
            FRAME.returning  = 1;
        }
        return FAILDESCR;
    }
    case TT_PLS: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (IS_INT(v) || IS_REAL(v)) return v;
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
        if (g_lang == 1 && e->t == TT_SEQ) {
            DESCR_t last = NULVCL;
            for (int ci = 0; ci < e->n; ci++) {
                last = interp_eval(e->c[ci]);
                if (IS_FAIL_fn(last)) return FAILDESCR;
            }
            return last;
        }
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
                    in_pat_mode = 1;
                }
                acc = pat_cat(acc, nxt);
            } else {
                if (g_lang != 1 && acc.v == DT_SNUL)
                    acc = nxt;
                else if (g_lang != 1 && nxt.v == DT_SNUL)
                    { }
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
        if (lv && (lv->t == TT_SECTION || lv->t == TT_SECTION_PLUS ||
                   lv->t == TT_SECTION_MINUS)) {
            if (icn_string_section_assign(lv, val)) return val;
            return FAILDESCR;
        }
        if (lv && lv->t == TT_IDX && lv->n == 2) {
            if (icn_string_section_assign(lv, val)) return val;
            DESCR_t _b = interp_eval(lv->c[0]);
            if (_b.v == DT_S || _b.v == DT_SNUL) return FAILDESCR;
        }
        if (lv && lv->t == TT_VAR && lv->v.sval && lv->v.sval[0] == '&' &&
                scan_depth > 0 && !frame_depth) {
            if (!kw_assign(lv->v.sval + 1, val)) return FAILDESCR;
        } else if (lv && lv->t == TT_VAR && lv->v.sval)
            NV_SET_fn(lv->v.sval, val);
        else if (lv && lv->t == TT_IDX && lv->n >= 2) {
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
            if (strcmp(lv->v.sval, "ITEM") == 0 && lv->n >= 2) {
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
                DESCR_t obj = interp_eval(lv->c[0]);
                if (!IS_FAIL_fn(obj))
                    FIELD_SET_fn(obj, lv->v.sval, val);
            }
        }
        else if (lv && lv->t == TT_FIELD && lv->v.sval && lv->n >= 1) {
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
                char *fn = GC_strdup(nm); sno_fold_name(fn);
                NV_SET_fn(fn, val);
            }
        }
        else if (lv && lv->t == TT_RANDOM && lv->n >= 1) {
            DESCR_t cv = interp_eval(lv->c[0]);
            if (!IS_FAIL_fn(cv)) {
                bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                unsigned long _rnd = bb_icn_rnd_seed >> 33;
                if (cv.v == DT_DATA && cv.u && cv.u->type && cv.u->type->nfields > 0 && cv.u->fields) {
                    int n = cv.u->type->nfields;
                    cv.u->fields[_rnd % (unsigned long)n] = val;
                } else if (cv.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(cv, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        int n = (int)FIELD_GET_fn(cv, "frame_size").i;
                        DESCR_t ea = FIELD_GET_fn(cv, "frame_elems");
                        DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                        if (elems && n > 0) elems[_rnd % (unsigned long)n] = val;
                    }
                } else if (lv->c[0]->t == TT_VAR && lv->c[0]->v.sval) {
                    const char *str = IS_STR_fn(cv) ? cv.s : VARVAL_fn(cv);
                    if (str) {
                        long slen = cv.slen > 0 ? cv.slen : (long)strlen(str);
                        if (slen > 0) {
                            const char *ch = VARVAL_fn(val);
                            char c = (ch && *ch) ? ch[0] : '\0';
                            char *ns = GC_malloc((size_t)(slen + 1));
                            memcpy(ns, str, (size_t)slen);
                            ns[_rnd % (unsigned long)slen] = c;
                            ns[slen] = '\0';
                            NV_SET_fn(lv->c[0]->v.sval, STRVAL(ns));
                        }
                    }
                }
            }
        }
        else if (lv && lv->t == TT_ITERATE && lv->n >= 1) {
            DESCR_t cv = interp_eval(lv->c[0]);
            if (!IS_FAIL_fn(cv)) {
                if (cv.v == DT_DATA && cv.u && cv.u->type && cv.u->type->nfields > 0 && cv.u->fields) {
                    cv.u->fields[0] = val;
                } else if (cv.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(cv, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        DESCR_t ea = FIELD_GET_fn(cv, "frame_elems");
                        DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                        if (elems) elems[0] = val;
                    }
                } else if (lv->c[0]->t == TT_VAR && lv->c[0]->v.sval) {
                    const char *str = VARVAL_fn(cv);
                    if (str) {
                        long slen = cv.slen > 0 ? cv.slen : (long)strlen(str);
                        if (slen > 0) {
                            const char *ch = VARVAL_fn(val);
                            char c = (ch && *ch) ? ch[0] : '\0';
                            char *ns = GC_malloc((size_t)(slen + 1));
                            memcpy(ns, str, (size_t)slen);
                            ns[0] = c;
                            ns[slen] = '\0';
                            DESCR_t sv = STRVAL(ns);
                            int _slot = lv->c[0]->_id;
                            if (_slot >= 0 && _slot < FRAME.env_n) FRAME.env[_slot] = sv;
                            else NV_SET_fn(lv->c[0]->v.sval, sv);
                        }
                    }
                }
            }
        }
        return val;
    }
    case TT_INDIRECT: {
        if (e->n < 1) return FAILDESCR;
        tree_t *child = e->c[0];
        int had_name_wrap = 0;
        if (child->t == TT_NAME && child->n == 1) {
            child = child->c[0];
            had_name_wrap = 1;
        }
        if (child->t == TT_VAR && child->v.sval) {
            if (had_name_wrap)
                return NV_GET_fn(child->v.sval);
            DESCR_t _xv = NV_GET_fn(child->v.sval);
            if (IS_NAMEPTR(_xv)) return NAME_DEREF_PTR(_xv);
            if (IS_NAMEVAL(_xv)) return NV_GET_fn(_xv.s);
            const char *_xnm0 = VARVAL_fn(_xv);
            if (!_xnm0 || !*_xnm0) return NULVCL;
            char *_xnm = GC_strdup(_xnm0); sno_fold_name(_xnm);
            DESCR_t _xnamed = NV_GET_fn(_xnm);
            if (IS_NAMEPTR(_xnamed)) return NAME_DEREF_PTR(_xnamed);
            if (IS_NAMEVAL(_xnamed)) return NV_GET_fn(_xnamed.s);
            return _xnamed;
        }
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
            if (inner->t == TT_VAR && inner->v.sval) {
                DESCR_t xval = NV_GET_fn(inner->v.sval);
                if (IS_NAMEPTR(xval)) return NAME_DEREF_PTR(xval);
                if (IS_NAMEVAL(xval)) return NV_GET_fn(xval.s);
                const char *nm2_0 = VARVAL_fn(xval);
                if (!nm2_0 || !*nm2_0) return NULVCL;
                char *nm2 = GC_strdup(nm2_0); sno_fold_name(nm2);
                DESCR_t named = NV_GET_fn(nm2);
                if (IS_NAMEPTR(named)) return NAME_DEREF_PTR(named);
                if (IS_NAMEVAL(named)) return NV_GET_fn(named.s);
                return named;
            }
            DESCR_t nd = interp_eval(inner);
            const char *nm2_0 = VARVAL_fn(nd);
            if (!nm2_0 || !*nm2_0) return NULVCL;
            char *nm2 = GC_strdup(nm2_0); sno_fold_name(nm2);
            DESCR_t named2 = NV_GET_fn(nm2);
            if (IS_NAMEPTR(named2)) return NAME_DEREF_PTR(named2);
            if (IS_NAMEVAL(named2)) return NV_GET_fn(named2.s);
            return named2;
        }
        DESCR_t nd = interp_eval(child);
        if (IS_NAMEPTR(nd)) return NAME_DEREF_PTR(nd);
        if (IS_NAMEVAL(nd)) return NV_GET_fn(nd.s);
        const char *nm0 = VARVAL_fn(nd);
        if (!nm0 || !*nm0) return NULVCL;
        char *nm = GC_strdup(nm0); sno_fold_name(nm);
        DESCR_t named = NV_GET_fn(nm);
        if (IS_NAMEPTR(named)) return NAME_DEREF_PTR(named);
        if (IS_NAMEVAL(named)) return NV_GET_fn(named.s);
        return named;
    }
    case TT_FNC: {
        if (!e->v.sval || !*e->v.sval) return FAILDESCR;
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
        if (strcmp(e->v.sval, "DEFINE") == 0) {
            const char *spec = define_spec_from_expr(e);
            if (spec && *spec) {
                const char *entry = define_entry_from_expr(e);
                if (entry) DEFINE_fn_entry(spec, NULL, entry);
                else       DEFINE_fn(spec, NULL);
                return NULVCL;
            }
            return FAILDESCR;
        }
        int nargs = e->n;
        DESCR_t *args = nargs > 0
            ? (DESCR_t *)alloca((size_t)nargs * sizeof(DESCR_t))
            : NULL;
        for (int i = 0; i < nargs; i++) {
            args[i] = interp_eval(e->c[i]);
            if (IS_FAIL_fn(args[i])) return FAILDESCR;
        }
        {
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
                DESCR_t r = call_user_function(e->v.sval, args, nargs);
                if (IS_NAME(r)) return NAME_DEREF(r);
                return r;
            }
        }
        {
            for (int _ci = 0; _ci < proc_count; _ci++) {
                if (strcmp(proc_table[_ci].name, e->v.sval) == 0)
                    return proc_table_call(_ci, args, nargs);
            }
            if (g_pl_active) {
                char _pk[256];
                snprintf(_pk, sizeof _pk, "%s/%d", e->v.sval, nargs);
                tree_t *_choice = pl_pred_table_lookup(&g_pl_pred_table, _pk);
                if (_choice) {
                    Term **_pl_args = (nargs > 0) ? pl_env_new(nargs) : NULL;
                    Term **_saved   = g_pl_env;
                    g_pl_env = _pl_args;
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
        if (strcmp(e->v.sval, "EVAL") == 0) {
            if (nargs < 1) return FAILDESCR;
            extern DESCR_t EVAL_fn(DESCR_t);
            DESCR_t _er = EVAL_fn(args[0]);
            return _er;
        }
        if (strcmp(e->v.sval, "CODE") == 0) {
            if (nargs < 1) return FAILDESCR;
            const char *_cs = VARVAL_fn(args[0]);
            extern DESCR_t code(const char *);
            return (_cs && *_cs) ? code(_cs) : FAILDESCR;
        }
        if (strcmp(e->v.sval, "IDENT") == 0) {
            if (nargs == 1) {
                return IS_NULL_fn(args[0]) ? NULVCL : FAILDESCR;
            }
            if (nargs >= 2) {
                int a_null = IS_NULL_fn(args[0]), b_null = IS_NULL_fn(args[1]);
                if (a_null && b_null) return NULVCL;
                if (a_null || b_null) return FAILDESCR;
                if (args[0].v != args[1].v) return FAILDESCR;
                const char *sa = VARVAL_fn(args[0]);
                const char *sb = VARVAL_fn(args[1]);
                if (!sa) sa = ""; if (!sb) sb = "";
                return strcmp(sa, sb) == 0 ? NULVCL : FAILDESCR;
            }
        }
        if (strcmp(e->v.sval, "DIFFER") == 0) {
            if (nargs == 1) {
                return IS_NULL_fn(args[0]) ? FAILDESCR : NULVCL;
            }
            if (nargs >= 2) {
                int a_null = IS_NULL_fn(args[0]), b_null = IS_NULL_fn(args[1]);
                if (a_null && b_null) return FAILDESCR;
                if (a_null || b_null) return NULVCL;
                if (args[0].v != args[1].v) return NULVCL;
                const char *sa = VARVAL_fn(args[0]);
                const char *sb = VARVAL_fn(args[1]);
                if (!sa) sa = ""; if (!sb) sb = "";
                return strcmp(sa, sb) != 0 ? NULVCL : FAILDESCR;
            }
        }
        {
            ScDatType *_dt = sc_dat_find_type(e->v.sval);
            if (_dt) return sc_dat_construct(_dt, args, nargs);
            int _fi = 0;
            ScDatType *_ft = sc_dat_find_field(e->v.sval, &_fi);
            if (_ft && nargs >= 1) return sc_dat_field_get(e->v.sval, args[0]);
        }
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
        if (e->n < 1) return NULVCL;
        tree_t *child = e->c[0];
        DESCR_t d;
        d.v    = DT_E;
        d.slen = 0;
        d.s    = NULL;
        d.ptr  = child;
        return d;
    }
    case TT_NOT: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return NULVCL;
        return FAILDESCR;
    }
    case TT_SIZE: {
        if (e->n < 1) return INTVAL(0);
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_T) return INTVAL(v.tbl ? v.tbl->size : 0);
        if (v.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(v,"icn_type");
            if (tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0)
                return INTVAL((int)FIELD_GET_fn(v,"frame_size").i);
        }
        if (IS_INT_fn(v)) return INTVAL(0);
        if (IS_REAL_fn(v)) return INTVAL(0);
        const char *s = VARVAL_fn(v);
        if (!s) return INTVAL(0);
        if (strchr(s, '\x01')) {
            long n = 1;
            for (const char *p = s; *p; p++) if (*p == '\x01') n++;
            return INTVAL(n);
        }
        long len;
        if (IS_CSET_fn(v)) {
            int klen = icn_kw_cset_len(v.s);
            len = klen >= 0 ? klen : (v.s ? (long)strlen(v.s) : 0);
        } else len = v.slen > 0 ? v.slen : (long)strlen(s);
        return INTVAL(len);
    }
    case TT_ALT: {
        if (e->n == 0) return NULVCL;
        DESCR_t acc = interp_eval_pat(e->c[0]);
        for (int i = 1; i < e->n; i++)
            acc = pat_alt(acc, interp_eval_pat(e->c[i]));
        return acc;
    }
    case TT_VLIST: {
        if (e->n == 0) return FAILDESCR;
        for (int i = 0; i < e->n; i++) {
            DESCR_t v = interp_eval(e->c[i]);
            if (!IS_FAIL_fn(v)) return v;
        }
        return FAILDESCR;
    }
    case TT_CAPT_COND_ASGN: {
        if (e->n < 2) return NULVCL;
        DESCR_t pat = interp_eval_pat(e->c[0]);
        tree_t *tgt = e->c[1];
        if (tgt->t == TT_DEFER && tgt->n == 1
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
            tree_t *ichild = tgt->c[0];
            if (ichild->t == TT_QLIT || ichild->t == TT_VAR)
                nm = ichild->v.sval;
            else {
                DESCR_t nd = interp_eval(ichild);
                nm = VARVAL_fn(nd);
            }
        }
        return nm ? pat_assign_cond(pat, STRVAL((char *)nm)) : pat;
    }
    case TT_CAPT_IMMED_ASGN: {
        if (e->n < 2) return NULVCL;
        DESCR_t pat = interp_eval_pat(e->c[0]);
        tree_t *tgt = e->c[1];
        if (tgt->t == TT_DEFER && tgt->n == 1
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
                return pat_assign_callcap_named_imm(pat, fnc->v.sval, NULL, 0, names, na);
            }
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
        if (e->n == 1) {
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
    case TT_IDENTICAL: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = interp_eval(e->c[0]);
        DESCR_t r = interp_eval(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return icn_descr_identical(l, r) ? r : FAILDESCR;
    }
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
        return NULVCL;
    case TT_CHOICE: {
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
    case TT_CSET: return e->v.sval ? CSETVAL(icn_cset_canonical(e->v.sval)) : NULVCL;
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
        if ((gen->t == TT_ASSIGN) &&
            gen->n >= 2 && is_suspendable(gen->c[1])) {
            tree_t *leaf = find_leaf_suspendable(gen->c[1]);
            if (!leaf) leaf = gen->c[1];
            bb_node_t rbox = icn_bb_build(leaf);
            DESCR_t tick = rbox.fn(rbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.returning && !FRAME.loop_break) {
                interp_eval(gen);
                if (body) interp_eval(body);
                if (FRAME.returning || FRAME.loop_break) break;
                tick = rbox.fn(rbox.ζ, β);
            }
            FRAME.loop_break = 0;
            return NULVCL;
        }
        tree_t *do_expr = body ? body : gen;
        bb_node_t box = icn_bb_build(gen);
        DESCR_t val = box.fn(box.ζ, α);
        while (!IS_FAIL_fn(val) && !FRAME.returning && !FRAME.loop_break) {
            frame_push(gen, val.v == DT_I ? val.i : 0, val.v == DT_I ? NULL : val.s);
            if (do_expr != gen) interp_eval(do_expr);
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
        if (e->n < 1) return NULVCL;
        DESCR_t topic = interp_eval(e->c[0]);
        int is_raku_layout = (e->n >= 4 && (e->n - 1) % 3 == 0 &&
            e->c[1] && (e->c[1]->t == TT_ILIT || e->c[1]->t == TT_NUL));
        if (is_raku_layout) {
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
        if (i < nc) return interp_eval(e->c[i]);
        return NULVCL;
    }
    case TT_NULL: {
        if (e->n < 1) return NULVCL;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return NULVCL;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return NULVCL;
        return FAILDESCR;
    }
    case TT_NONNULL: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return FAILDESCR;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return FAILDESCR;
        return v;
    }
    case TT_RANDOM: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = interp_eval(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
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
        long slen = IS_CSET_fn(v) ? (long)strlen(s) : (v.slen > 0 ? v.slen : (long)strlen(s));
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
        #define AUGOP_APPLY(lv_, rv_) do { \
            DESCR_t _lv = (lv_), _rv = (rv_); \
            if (IS_FAIL_fn(_lv)||IS_FAIL_fn(_rv)) break; \
            long _li = IS_INT_fn(_lv)?_lv.i:(long)_lv.r; \
            long _ri = IS_INT_fn(_rv)?_rv.i:(long)_rv.r; \
            DESCR_t _res = NULVCL; \
            switch((AugOp_e)e->v.ival){ \
                case AUGOP_ADD:    _res=INTVAL(_li+_ri); break; \
                case AUGOP_SUB:    _res=INTVAL(_li-_ri); break; \
                case AUGOP_MUL:    _res=INTVAL(_li*_ri); break; \
                case AUGOP_DIV:    _res=_ri?INTVAL(_li/_ri):FAILDESCR; break; \
                case AUGOP_MOD:    _res=_ri?INTVAL(_li%_ri):FAILDESCR; break; \
                case AUGOP_CONCAT: { \
                    const char *_ls=VARVAL_fn(_lv),*_rs=VARVAL_fn(_rv); \
                    if(!_ls)_ls="";if(!_rs)_rs=""; \
                    size_t _ll=strlen(_ls),_rl=strlen(_rs); \
                    char *_buf=GC_malloc(_ll+_rl+1); \
                    memcpy(_buf,_ls,_ll);memcpy(_buf+_ll,_rs,_rl);_buf[_ll+_rl]='\0'; \
                    _res=STRVAL(_buf); break; \
                } \
                case AUGOP_EQ:  _res = (_li == _ri) ? _rv : FAILDESCR; break; \
                case AUGOP_NE:  _res = (_li != _ri) ? _rv : FAILDESCR; break; \
                case AUGOP_LT:  _res = (_li <  _ri) ? _rv : FAILDESCR; break; \
                case AUGOP_LE:  _res = (_li <= _ri) ? _rv : FAILDESCR; break; \
                case AUGOP_GT:  _res = (_li >  _ri) ? _rv : FAILDESCR; break; \
                case AUGOP_GE:  _res = (_li >= _ri) ? _rv : FAILDESCR; break; \
                case AUGOP_SEQ: case AUGOP_SNE: \
                case AUGOP_SLT: case AUGOP_SLE: case AUGOP_SGT: case AUGOP_SGE: { \
                    const char *_lcs=VARVAL_fn(_lv),*_rcs=VARVAL_fn(_rv); \
                    if(!_lcs)_lcs="";if(!_rcs)_rcs=""; \
                    int _cmp = strcmp(_lcs, _rcs); int _ok = 0; \
                    switch ((AugOp_e)e->v.ival) { \
                        case AUGOP_SEQ: _ok = (_cmp == 0); break; \
                        case AUGOP_SNE: _ok = (_cmp != 0); break; \
                        case AUGOP_SLT: _ok = (_cmp <  0); break; \
                        case AUGOP_SLE: _ok = (_cmp <= 0); break; \
                        case AUGOP_SGT: _ok = (_cmp >  0); break; \
                        case AUGOP_SGE: _ok = (_cmp >= 0); break; \
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
        DESCR_t _augop_result = NULVCL;
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
                        for (int i = 0; i < cv.u->type->nfields; i++) AUGOP_CELL(cv.u->fields[i]);
                    }
                }
                #undef AUGOP_CELL
            }
        } else if (rhs && is_suspendable(rhs)) {
            bb_node_t rbox = icn_bb_build(rhs);
            DESCR_t tick = rbox.fn(rbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.loop_break && !FRAME.returning) {
                DESCR_t cur_lv = interp_eval(lhs);
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
        if (!frame_depth && !g_pl_active) {
            DESCR_t subj_d = interp_eval(e->c[0]);
            if (IS_FAIL_fn(subj_d)) return FAILDESCR;
            const char *sname = (e->c[0]->t == TT_VAR) ? e->c[0]->v.sval : NULL;
            DESCR_t pat_d = (e->n >= 2) ? interp_eval_pat(e->c[1]) : pat_epsilon();
            if (IS_FAIL_fn(pat_d)) return FAILDESCR;
            int ok = exec_stmt(sname, sname ? NULL : &subj_d, pat_d, NULL, 0);
            return ok ? NULVCL : FAILDESCR;
        }
        DESCR_t subj_d = interp_eval(e->c[0]);
        if (IS_FAIL_fn(subj_d)) return FAILDESCR;
        const char *subj_s;
        if (IS_REAL_fn(subj_d)) { char _rb[64]; real_str(subj_d.r,_rb,sizeof _rb); subj_s = GC_strdup(_rb); }
        else { subj_s = VARVAL_fn(subj_d); if (!subj_s) subj_s = ""; }
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
        if (e->n >= 1) {
            DESCR_t sv = interp_eval(e->c[0]);
            if (sv.v == DT_T && sv.tbl) {
                for (int bi = 0; bi < TABLE_BUCKETS; bi++) {
                    if (sv.tbl->buckets[bi]) return sv.tbl->buckets[bi]->val;
                }
                return FAILDESCR;
            }
            if (sv.v == DT_DATA) {
                DESCR_t tag = FIELD_GET_fn(sv,"icn_type");
                if (tag.v==DT_S && tag.s && strcmp(tag.s,"list")==0) {
                    int n=(int)FIELD_GET_fn(sv,"frame_size").i;
                    DESCR_t ea=FIELD_GET_fn(sv,"frame_elems");
                    DESCR_t *elems=(ea.v==DT_DATA)?(DESCR_t*)ea.ptr:NULL;
                    if(!elems||n<=0) return FAILDESCR;
                    return elems[0];
                }
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
        DESCR_t val = (e->n > 0) ? interp_eval(e->c[0]) : NULVCL;
        if (frame_depth > 0) {
            FRAME.suspending  = 1;
            FRAME.suspend_val = val;
            FRAME.suspend_do  = (e->n > 1) ? e->c[1] : NULL;
        }
        return val;
    }
    case TT_SWAP: {
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
    case TT_LCONCAT: {
        if (e->n < 2) return NULVCL;
        DESCR_t a = interp_eval(e->c[0]);
        DESCR_t b = interp_eval(e->c[1]);
        if (a.v == DT_DATA && b.v == DT_DATA) {
            DESCR_t atag = FIELD_GET_fn(a, "icn_type");
            DESCR_t btag = FIELD_GET_fn(b, "icn_type");
            if (atag.v == DT_S && atag.s && strcmp(atag.s,"list")==0 &&
                btag.v == DT_S && btag.s && strcmp(btag.s,"list")==0) {
                DESCR_t asz_d = FIELD_GET_fn(a, "frame_size");
                DESCR_t bsz_d = FIELD_GET_fn(b, "frame_size");
                int an = (int)(IS_INT_fn(asz_d)?asz_d.i:0);
                int bn = (int)(IS_INT_fn(bsz_d)?bsz_d.i:0);
                int cn = an + bn;
                DESCR_t *celems = GC_malloc((cn>0?cn:1)*sizeof(DESCR_t));
                DESCR_t aptr = FIELD_GET_fn(a, "frame_elems");
                DESCR_t bptr = FIELD_GET_fn(b, "frame_elems");
                DESCR_t *ae = (aptr.v == DT_DATA) ? (DESCR_t*)aptr.ptr : NULL;
                DESCR_t *be = (bptr.v == DT_DATA) ? (DESCR_t*)bptr.ptr : NULL;
                for (int i=0;i<an;i++) celems[i] = ae ? ae[i] : NULVCL;
                for (int i=0;i<bn;i++) celems[an+i] = be ? be[i] : NULVCL;
                DESCR_t eptr; eptr.v=DT_DATA; eptr.slen=0; eptr.ptr=(void*)celems;
                static int icnlist_lcat = 0;
                if (!icnlist_lcat) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_lcat=1; }
                return DATCON_fn("icnlist", eptr, INTVAL(cn), STRVAL("list"));
            }
        }
        char ab[64], bb[64];
        const char *as = IS_INT_fn(a)?(snprintf(ab,64,"%lld",(long long)a.i),ab):IS_REAL_fn(a)?(real_str(a.r,ab,64),ab):VARVAL_fn(a);
        const char *bs = IS_INT_fn(b)?(snprintf(bb,64,"%lld",(long long)b.i),bb):IS_REAL_fn(b)?(real_str(b.r,bb,64),bb):VARVAL_fn(b);
        if (!as) as=""; if (!bs) bs="";
        size_t al=strlen(as),bl=strlen(bs);
        char *buf=GC_malloc(al+bl+1); memcpy(buf,as,al); memcpy(buf+al,bs,bl); buf[al+bl]='\0';
        return STRVAL(buf);
    }
    case TT_MAKELIST: {
        int n = e->n;
        static int icnlist_registered = 0;
        if (!icnlist_registered) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_registered=1; }
        DESCR_t *elems = GC_malloc((n>0?n:1)*sizeof(DESCR_t));
        for (int i = 0; i < n; i++) elems[i] = interp_eval(e->c[i]);
        DESCR_t eptr; eptr.v=DT_DATA; eptr.slen=0; eptr.ptr=(void*)elems;
        DESCR_t ld = DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
        return ld;
    }
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
    case TT_SECTION_PLUS: {
        if (e->n < 3) return NULVCL;
        DESCR_t sd = interp_eval(e->c[0]);
        const char *s = VARVAL_fn(sd); if (!s) s = "";
        int slen = (int)strlen(s);
        int i = (int)to_int(interp_eval(e->c[1]));
        int n = (int)to_int(interp_eval(e->c[2]));
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return FAILDESCR;
        int lo, hi;
        if (n >= 0) { lo = i; hi = i + n; }
        else        { lo = i + n; hi = i; }
        if (lo < 1 || hi > slen+1) return FAILDESCR;
        int len = hi - lo;
        char *buf = GC_malloc(len+1); memcpy(buf, s+lo-1, len); buf[len]='\0';
        return STRVAL(buf);
    }
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
    case TT_INITIAL: {
        IcnInitEnt *ent = NULL;
        for (int _i = 0; _i < icn_init_n; _i++)
            if (init_tab[_i].id == e->_id) { ent = &init_tab[_i]; break; }
        if (!ent) {
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
    case TT_RECORD: {
        if (!e->v.sval) return NULVCL;
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
        sc_dat_register(spec);
        return NULVCL;
    }
    case TT_FIELD: {
        if (!e->v.sval || e->n < 1) return NULVCL;
        DESCR_t obj = interp_eval(e->c[0]);
        if (IS_FAIL_fn(obj)) return FAILDESCR;
        DESCR_t *cell = data_field_ptr(e->v.sval, obj);
        if (!cell) return FAILDESCR;
        return *cell;
    }
    case TT_GLOBAL: case TT_LOCAL: case TT_STATIC_DECL:
        return NULVCL;
    default:
        return NULVCL;
    }
}
