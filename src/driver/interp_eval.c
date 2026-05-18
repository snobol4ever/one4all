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
        DESCR_t _i1=FAILDESCR;
        DESCR_t _i2=FAILDESCR;
        int i=(int)to_int(_i1), j=(int)to_int(_i2);
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (j == 0) j = slen + 1; else if (j < 0) j = slen + 1 + j;
        if (i < 1 || i > slen+1 || j < 1 || j > slen+1) return 0;
        lo = i < j ? i : j; hi = i < j ? j : i;
    } else if (kind == TT_SECTION_PLUS) {
        DESCR_t _sp1=FAILDESCR;
        DESCR_t _sp2=FAILDESCR;
        int i=(int)to_int(_sp1), n=(int)to_int(_sp2);
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return 0;
        if (n < 0) return 0;
        if (i + n > slen + 1) return 0;
        lo = i; hi = i + n;
    } else if (kind == TT_SECTION_MINUS) {
        DESCR_t _sm1=FAILDESCR;
        DESCR_t _sm2=FAILDESCR;
        int i=(int)to_int(_sm1), n=(int)to_int(_sm2);
        if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen+1) return 0;
        if (n < 0) return 0;
        if (i - n < 1) return 0;
        lo = i - n; hi = i;
    } else {
        DESCR_t _idx_d = FAILDESCR;
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
    /* Record constructor fallback: if fn matches a registered ScDatType, construct an instance. */
    {
        ScDatType *_rdt = sc_dat_find_type(fn);
        if (_rdt) { *out = sc_dat_construct(_rdt, args, nargs); return 1; }
    }
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
            return FAILDESCR;
        }
    }
    return FAILDESCR;
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
