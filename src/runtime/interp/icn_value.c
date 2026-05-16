#include "icn_value.h"
#include "icn_stmt.h"
#include "icn_runtime.h"
#include "../../driver/interp_private.h"
#include "coerce.h"
#include "bb_broker.h"
#include "snobol4.h"
#include <string.h>
#include <gc/gc.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t eval_node(tree_t *e);
unsigned long bb_icn_rnd_seed = 12345UL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
typedef enum { BBR_LT, BBR_LE, BBR_GT, BBR_GE, BBR_EQ, BBR_NE } bb_relop_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
typedef enum { BBS_LLT, BBS_LLE, BBS_LGT, BBS_LGE, BBS_LEQ, BBS_LNE } bb_strrelop_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_strrel(tree_t *e, bb_strrelop_t op)
{
    if (e->n < 2) return FAILDESCR;
    DESCR_t l = bb_eval_value(e->c[0]);
    DESCR_t r = bb_eval_value(e->c[1]);
    if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
    char _lb[64], _rb[64];
    const char *ls, *rs;
    if (IS_INT_fn(l))       { snprintf(_lb,sizeof _lb,"%lld",(long long)l.i); ls=_lb; }
    else if (IS_REAL_fn(l)) { real_str(l.r,_lb,sizeof _lb); ls=_lb; }
    else                    { ls=VARVAL_fn(l); if(!ls) ls=""; }
    if (IS_INT_fn(r))       { snprintf(_rb,sizeof _rb,"%lld",(long long)r.i); rs=_rb; }
    else if (IS_REAL_fn(r)) { real_str(r.r,_rb,sizeof _rb); rs=_rb; }
    else                    { rs=VARVAL_fn(r); if(!rs) rs=""; }
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
    return ok ? STRVAL(GC_strdup(rs)) : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
typedef enum { BBS_RANGE, BBS_PLUS, BBS_MINUS } bb_section_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_section(tree_t *e, bb_section_t kind)
{
    if (e->n < 3) return NULVCL;
    DESCR_t sd = bb_eval_value(e->c[0]);
    if (IS_FAIL_fn(sd)) return FAILDESCR;
    DESCR_t a1 = bb_eval_value(e->c[1]);
    DESCR_t a2 = bb_eval_value(e->c[2]);
    if (IS_FAIL_fn(a1) || IS_FAIL_fn(a2)) return FAILDESCR;
    if (sd.v == DT_DATA) {
        DESCR_t tag = FIELD_GET_fn(sd, "icn_type");
        if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
            int n = (int)FIELD_GET_fn(sd, "frame_size").i;
            DESCR_t ea = FIELD_GET_fn(sd, "frame_elems");
            DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
            int i = (int)to_int(a1), x = (int)to_int(a2);
            if (i == 0) i = n + 1; else if (i < 0) i = n + i + 1;
            int lo, hi;
            if (kind == BBS_RANGE) {
                if (x == 0) x = n + 1; else if (x < 0) x = n + x + 1;
                if (i < 1 || i > n+1 || x < 1 || x > n+1) return FAILDESCR;
                lo = i < x ? i : x; hi = i < x ? x : i;
            } else if (kind == BBS_PLUS) {
                if (x >= 0) { lo = i; hi = i + x; } else { lo = i + x; hi = i; }
                if (lo < 1 || hi > n+1) return FAILDESCR;
            } else {
                if (x >= 0) { lo = i - x; hi = i; } else { lo = i; hi = i - x; }
                if (lo < 1 || hi > n+1) return FAILDESCR;
            }
            int rlen = hi - lo;
            if (rlen <= 0) {
                static int icnlist_empty_reg2 = 0;
                if (!icnlist_empty_reg2) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_empty_reg2 = 1; }
                DESCR_t ep; ep.v = DT_DATA; ep.slen = 0; ep.ptr = NULL;
                return DATCON_fn("icnlist", ep, INTVAL(0), STRVAL("list"));
            }
            DESCR_t *rbuf = GC_malloc((size_t)rlen * sizeof(DESCR_t));
            for (int k = 0; k < rlen; k++) rbuf[k] = (elems && lo+k-1 >= 0 && lo+k-1 < n) ? elems[lo+k-1] : NULVCL;
            DESCR_t rp; rp.v = DT_DATA; rp.slen = 0; rp.ptr = (void *)rbuf;
            static int icnlist_sect_reg = 0;
            if (!icnlist_sect_reg) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_sect_reg = 1; }
            return DATCON_fn("icnlist", rp, INTVAL(rlen), STRVAL("list"));
        }
    }
    const char *s = VARVAL_fn(sd);
    if (!s) s = "";
    int slen = (int)strlen(s);
    int i = (int)to_int(a1);
    int x = (int)to_int(a2);
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
    } else {
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_augop_compute(DESCR_t lv, DESCR_t rv, AugOp_e op)
{
    if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
    int lv_real = IS_REAL_fn(lv), rv_real = IS_REAL_fn(rv);
    double ld = lv_real ? lv.r : IS_INT_fn(lv) ? (double)lv.i : strtod(lv.s ? lv.s : "0", NULL);
    double rd = rv_real ? rv.r : IS_INT_fn(rv) ? (double)rv.i : strtod(rv.s ? rv.s : "0", NULL);
    long li = (long)ld, ri = (long)rd;
    int use_real = lv_real || rv_real;
    switch (op) {
    case AUGOP_ADD:    return use_real ? REALVAL(ld+rd) : INTVAL(li+ri);
    case AUGOP_SUB:    return use_real ? REALVAL(ld-rd) : INTVAL(li-ri);
    case AUGOP_MUL:    return use_real ? REALVAL(ld*rd) : INTVAL(li*ri);
    case AUGOP_DIV:    return use_real ? (rd!=0.0?REALVAL(ld/rd):FAILDESCR) : (ri?INTVAL(li/ri):FAILDESCR);
    case AUGOP_MOD:    return ri ? INTVAL(li % ri) : FAILDESCR;
    case AUGOP_POW: {
        double result = pow(ld, rd);
        if (!lv_real && !rv_real && rd >= 0 && (double)(long long)result == result) return INTVAL((long long)result);
        return REALVAL(result);
    }
    case AUGOP_CONCAT: {
        const char *ls = VARVAL_fn(lv), *rs = VARVAL_fn(rv);
        if (!ls) ls = ""; if (!rs) rs = "";
        size_t ll = strlen(ls), rl = strlen(rs);
        char *buf = GC_malloc(ll + rl + 1);
        memcpy(buf, ls, ll); memcpy(buf + ll, rs, rl); buf[ll + rl] = '\0';
        return STRVAL(buf);
    }
    case AUGOP_EQ: return (li == ri) ? rv : FAILDESCR;
    case AUGOP_NE: return (li != ri) ? rv : FAILDESCR;
    case AUGOP_LT: return (li <  ri) ? rv : FAILDESCR;
    case AUGOP_LE: return (li <= ri) ? rv : FAILDESCR;
    case AUGOP_GT: return (li >  ri) ? rv : FAILDESCR;
    case AUGOP_GE: return (li >= ri) ? rv : FAILDESCR;
    case AUGOP_SEQ: case AUGOP_SNE:
    case AUGOP_SLT: case AUGOP_SLE: case AUGOP_SGT: case AUGOP_SGE: {
        const char *ls = VARVAL_fn(lv), *rs = VARVAL_fn(rv);
        if (!ls) ls = ""; if (!rs) rs = "";
        int cmp = strcmp(ls, rs);
        int ok = 0;
        switch (op) {
        case AUGOP_SEQ: ok = (cmp == 0); break;
        case AUGOP_SNE: ok = (cmp != 0); break;
        case AUGOP_SLT: ok = (cmp <  0); break;
        case AUGOP_SLE: ok = (cmp <= 0); break;
        case AUGOP_SGT: ok = (cmp >  0); break;
        case AUGOP_SGE: ok = (cmp >= 0); break;
        default:        break;
        }
        return ok ? rv : FAILDESCR;
    }
    default:           return INTVAL(li + ri);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void bb_augop_writeback(tree_t *lhs, DESCR_t res)
{
    if (!lhs) return;
    if (lhs->t == TT_VAR) {
        int slot = lhs->_id;
        if (frame_depth > 0 && slot >= 0 && slot < FRAME.env_n)
            FRAME.env[slot] = res;
        else if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&')
            set_and_trace(lhs->v.sval, res);
    } else if (lhs->t == TT_IDX && lhs->n >= 2) {
        DESCR_t base = bb_eval_value(lhs->c[0]);
        DESCR_t idx  = bb_eval_value(lhs->c[1]);
        if (!IS_FAIL_fn(base) && !IS_FAIL_fn(idx))
            subscript_set(base, idx, res);
    } else if (lhs->t == TT_FIELD && ICN_FIELD_NAME(lhs)) {
        DESCR_t obj = bb_eval_value(lhs->c[0]);
        if (!IS_FAIL_fn(obj)) {
            DESCR_t *cell = data_field_ptr(ICN_FIELD_NAME(lhs), obj);
            if (cell) *cell = res;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_proc_as_value(const char *name)
{
    if (!name || name[0] == '&') return FAILDESCR;
    for (int i = 0; i < proc_count; i++) {
        if (proc_table[i].name && strcmp(proc_table[i].name, name) == 0) {
            DESCR_t pv; pv.v = DT_E;
            pv.slen = (uint32_t)i;
            pv.i    = proc_table[i].entry_pc;
            return pv;
        }
    }
    static const char *builtins[] = {
        "write","writes","read","reads","close","open","remove","flush",
        "put","get","pull","push","pop","list","image","proc","type","copy",
        "string","integer","real","numeric","ord","char","reverse","sort","sortf",
        "find","match","many","any","upto","bal","move","tab","pos",
        "map","repl","trim","left","right","center","detab","entab",
        "abs","sqrt","sin","cos","tan","asin","acos","atan","exp","log",
        "dtor","rtod",
        "iand","ior","ixor","ishift","icom",
        "table","key","insert","delete","member","args","level",
        "collect","stop","exit","runerr","name","variable","seq",
        NULL
    };
    for (int i = 0; builtins[i]; i++)
        if (strcmp(builtins[i], name) == 0) return STRVAL(name);
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t bb_eval_value(tree_t *e)
{
    if (!e) return NULVCL;
    extern tree_t *icn_drive_node; extern DESCR_t icn_drive_val;
    if (icn_drive_node && e == icn_drive_node) return icn_drive_val;
    if (e->t == TT_VAR && frame_depth > 0) {
        if (e->v.sval && e->v.sval[0] == '&') {
            return icn_kw_read(e->v.sval + 1);
        }
        int slot = e->_id;
        if (slot >= 0 && slot < FRAME.env_n) {
            DESCR_t sv = FRAME.env[slot];
            if (sv.v != 0) return sv;
        }
        if (e->v.sval) {
            DESCR_t nv = NV_GET_fn(e->v.sval);
            if (!IS_FAIL_fn(nv) && (nv.v != DT_SNUL)) return nv;
            DESCR_t pv = icn_proc_as_value(e->v.sval);
            if (!IS_FAIL_fn(pv)) return pv;
            return nv;
        }
        return NULVCL;
    }
    switch (e->t) {
    case TT_ILIT:
    case TT_FLIT:
    case TT_QLIT:
    case TT_NUL:
    case TT_KEYWORD:
        return eval_node(e);
    case TT_VAR: {
        DESCR_t r = eval_node(e);
        if (!IS_FAIL_fn(r) && r.v != DT_SNUL) return r;
        if (e->v.sval) {
            DESCR_t pv = icn_proc_as_value(e->v.sval);
            if (!IS_FAIL_fn(pv)) return pv;
        }
        return r;
    }
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
            int slot = lhs->_id;
            if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; return val; }
            if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') set_and_trace(lhs->v.sval, val);
        } else if (lhs && lhs->t == TT_IDX && lhs->n >= 2) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(base)) {
                DESCR_t idx = bb_eval_value(lhs->c[1]);
                if (!IS_FAIL_fn(idx)) subscript_set(base, idx, val);
            }
        } else if (lhs && lhs->t == TT_FIELD && ICN_FIELD_NAME(lhs)) {
            DESCR_t obj = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(obj)) {
                DESCR_t *cell = data_field_ptr(ICN_FIELD_NAME(lhs), obj);
                if (cell) *cell = val;
            }
        } else if (lhs && lhs->t == TT_ITERATE && lhs->n >= 1) {
DESCR_t cv = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(cv)) {
                if (cv.v == DT_T && cv.tbl) {
                    for (int b = 0; b < TABLE_BUCKETS; b++)
                        for (TBPAIR_t *p = cv.tbl->buckets[b]; p; p = p->next)
                            p->val = val;
                } else if (IS_STR_fn(cv)) {
        const char *s = VARVAL_fn(cv);
                    int slen = s ? (int)strlen(s) : 0;
                    if (slen > 0) {
                        const char *ch = VARVAL_fn(val);
                        char *buf = (char *)GC_malloc((size_t)(slen + 1));
                        memcpy(buf, s, (size_t)(slen + 1));
                        buf[0] = (ch && *ch) ? ch[0] : '\0';
                        tree_t *lv = lhs->c[0];
                        if (lv && lv->t == TT_VAR) {
                            DESCR_t sv = STRVAL(buf);
                            if (lv->_id >= 0 && lv->_id < FRAME.env_n) FRAME.env[lv->_id] = sv;
                            else if (lv->_id < 0 && lv->v.sval && lv->v.sval[0] != '&') NV_SET_fn(lv->v.sval, sv);
                        }
                    }
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
        } else if (lhs && lhs->t == TT_RANDOM && lhs->n >= 1) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            extern unsigned long bb_icn_rnd_seed;
            if (base.v == DT_DATA) {
                DESCR_t tag = FIELD_GET_fn(base, "icn_type");
                if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                    int n = (int)FIELD_GET_fn(base, "frame_size").i;
                    if (n > 0) {
                        bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                        int fi = (int)((bb_icn_rnd_seed >> 33) % (unsigned long)n);
                        subscript_set(base, INTVAL(fi + 1), val);  /* 1-based indexing */
                    }
                } else if (base.u && base.u->type && base.u->type->nfields > 0 && base.u->fields) {
                    bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                    int fi = (int)((bb_icn_rnd_seed >> 33) % (unsigned long)base.u->type->nfields);
                    base.u->fields[fi] = val;
                }
            } else if (base.v == DT_T && base.tbl && base.tbl->size > 0) {
                bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                int target = (int)((bb_icn_rnd_seed >> 33) % (unsigned long)base.tbl->size);
                int seen = 0;
                for (int _b = 0; _b < TABLE_BUCKETS; _b++)
                    for (TBPAIR_t *p = base.tbl->buckets[_b]; p; p = p->next)
                        if (seen++ == target) { p->val = val; goto _random_lhs_done; }
                _random_lhs_done:;
            }
        }
        return val;
    }
    case TT_FNC: {
        if (e->n < 1) return NULVCL;
        int nargs = e->n - 1;
        if (!e->c[0] || e->c[0]->t != TT_VAR) {
            DESCR_t callee = e->c[0] ? bb_eval_value(e->c[0]) : FAILDESCR;
            if (IS_FAIL_fn(callee)) return FAILDESCR;
            DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
            for (int j = 0; j < nargs; j++) {
                args[j] = bb_eval_value(e->c[1+j]);
                if (IS_FAIL_fn(args[j])) return FAILDESCR;
            }
            if (callee.v == DT_E) {
                for (int i = 0; i < proc_count; i++) {
                    if (proc_table[i].entry_pc == (int)callee.i) return proc_table_call(i, args, nargs);
                }
                if (callee.slen < (uint32_t)proc_count)
                    return proc_table_call((int)callee.slen, args, nargs);
                return FAILDESCR;
            }
            if (callee.v == DT_S && callee.s) {
                DESCR_t out = FAILDESCR;
                if (icn_try_call_builtin_by_name(callee.s, args, nargs, &out)) return out;
                for (int i = 0; i < proc_count; i++) {
                    if (proc_table[i].name && strcmp(proc_table[i].name, callee.s) == 0)
                        return proc_table_call(i, args, nargs);
                }
                return FAILDESCR;
            }
            if (IS_INT_fn(callee) || IS_REAL_fn(callee)) return FAILDESCR;
            if (callee.v == DT_SNUL) return NULVCL;
            return FAILDESCR;
        }
        const char *fn = e->c[0]->v.sval;
        if (!fn) return NULVCL;
        {
            DESCR_t __rk_d;
            if (raku_try_call_builtin(e, &__rk_d)) return __rk_d;
        }
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, fn) != 0) continue;
            DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
            for (int j = 0; j < nargs; j++) {
                args[j] = bb_eval_value(e->c[1+j]);
                if (IS_FAIL_fn(args[j])) return FAILDESCR;
            }
            DESCR_t result = proc_table_call(i, args, nargs);
            return result;
        }
        DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
        for (int j = 0; j < nargs; j++) {
            args[j] = bb_eval_value(e->c[1+j]);
            if (IS_FAIL_fn(args[j])) return FAILDESCR;
        }
        {
            DESCR_t callee_val = bb_eval_value(e->c[0]);
            if (callee_val.v == DT_E) {
                for (int i = 0; i < proc_count; i++) {
                    if (proc_table[i].entry_pc == (int)callee_val.i)
                        return proc_table_call(i, args, nargs);
                }
                if (callee_val.slen < (uint32_t)proc_count)
                    return proc_table_call((int)callee_val.slen, args, nargs);
                return FAILDESCR;
            }
            if (callee_val.v == DT_S && callee_val.s) {
                DESCR_t out2 = FAILDESCR;
                if (icn_try_call_builtin_by_name(callee_val.s, args, nargs, &out2)) return out2;
            }
        }
        return icn_call_builtin(e, args, nargs);
    }
    case TT_ADD: return bb_arith(e, SM_ADD);
    case TT_SUB: return bb_arith(e, SM_SUB);
    case TT_MUL: return bb_arith(e, SM_MUL);
    case TT_DIV: return bb_arith(e, SM_DIV);
    case TT_MOD: return bb_arith(e, SM_MOD);
    case TT_POW: return bb_arith(e, SM_EXP);
    case TT_LT: return bb_numrel(e, BBR_LT);
    case TT_LE: return bb_numrel(e, BBR_LE);
    case TT_GT: return bb_numrel(e, BBR_GT);
    case TT_GE: return bb_numrel(e, BBR_GE);
    case TT_EQ: return bb_numrel(e, BBR_EQ);
    case TT_NE: return bb_numrel(e, BBR_NE);
    case TT_IDENTICAL: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = bb_eval_value(e->c[0]);
        DESCR_t r = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return icn_descr_identical(l, r) ? r : FAILDESCR;
    }
    case TT_LLT: return bb_strrel(e, BBS_LLT);
    case TT_LLE: return bb_strrel(e, BBS_LLE);
    case TT_LGT: return bb_strrel(e, BBS_LGT);
    case TT_LGE: return bb_strrel(e, BBS_LGE);
    case TT_LEQ: return bb_strrel(e, BBS_LEQ);
    case TT_LNE: return bb_strrel(e, BBS_LNE);
    case TT_CAT:
        return bb_str_concat(e);
    case TT_LCONCAT: {
        DESCR_t a = bb_eval_value(e->c[0]);
        DESCR_t b = bb_eval_value(e->c[1]);
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
                static int icnlist_lcat_bb = 0;
                if (!icnlist_lcat_bb) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_lcat_bb=1; }
                return DATCON_fn("icnlist", eptr, INTVAL(cn), STRVAL("list"));
            }
        }
        return bb_str_concat(e);
    }
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
    case TT_FIELD: {
        if (!ICN_FIELD_NAME(e)) return NULVCL;
        DESCR_t obj = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(obj)) return FAILDESCR;
        DESCR_t *cell = data_field_ptr(ICN_FIELD_NAME(e), obj);
        if (!cell) return FAILDESCR;
        return *cell;
    }
    case TT_SECTION:        return bb_section(e, BBS_RANGE);
    case TT_SECTION_PLUS:   return bb_section(e, BBS_PLUS);
    case TT_SECTION_MINUS:  return bb_section(e, BBS_MINUS);
    case TT_MAKELIST: {
        int n = e->n;
        static int icnlist_registered = 0;
        if (!icnlist_registered) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_registered = 1; }
        DESCR_t *elems = GC_malloc((n > 0 ? n : 1) * sizeof(DESCR_t));
        for (int i = 0; i < n; i++) elems[i] = bb_eval_value(e->c[i]);
        DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void*)elems;
        return DATCON_fn("icnlist", eptr, INTVAL(n), STRVAL("list"));
    }
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
    case TT_NOT: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return NULVCL;
        return FAILDESCR;
    }
    case TT_NULL: {
        if (e->n < 1) return NULVCL;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return NULVCL;
        if (v.v == DT_S && (!v.s || v.s[0] == '\0')) return NULVCL;
        return FAILDESCR;
    }
    case TT_NONNULL: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) return FAILDESCR;
        return v;
    }
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
    case TT_RANDOM: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t v = bb_eval_value(e->c[0]);
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
        AugOp_e op = (AugOp_e)e->v.ival;
        DESCR_t result = NULVCL;
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
        if (rhs && is_suspendable(rhs)) {
            bb_node_t rbox = icn_bb_build(rhs);
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
        DESCR_t lv = bb_eval_value(lhs);
        DESCR_t rv = bb_eval_value(rhs);
        DESCR_t res = bb_augop_compute(lv, rv, op);
        if (!IS_FAIL_fn(res)) {
            bb_augop_writeback(lhs, res);
            result = res;
        } else {
            return FAILDESCR;
        }
        return result;
    }
    case TT_TO:
    case TT_TO_BY: {
        long cur;
        if (icn_frame_lookup(e, &cur)) return INTVAL(cur);
        bb_node_t box = icn_bb_build(e);
        return box.fn(box.ζ, α);
    }
    case TT_ITERATE: {
        long cur; const char *sv;
        if (icn_frame_lookup_sv(e, &cur, &sv)) {
            if (sv) {
                char tmp[2]; tmp[0] = sv[cur]; tmp[1] = '\0';
                return STRVAL(GC_strdup(tmp));
            }
            return INTVAL(cur);
        }
        bb_node_t box = icn_bb_build(e);
        return box.fn(box.ζ, α);
    }
    case TT_LIMIT:
    case TT_ALTERNATE:
    case TT_SEQ_EXPR: {
        bb_node_t box = icn_bb_build(e);
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
    case TT_SCAN: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t subj_d = bb_eval_value(e->c[0]);
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
        if (i < nc) return bb_eval_value(e->c[i]);
        return NULVCL;
    }
    case TT_CSET:
        return e->v.sval ? CSETVAL(icn_cset_canonical(e->v.sval)) : NULVCL;
    case TT_CSET_COMPL: {
        DESCR_t operand = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(operand)) return FAILDESCR;
        if (IS_INT_fn(operand) || IS_REAL_fn(operand)) operand = descr_to_str_icn(operand);
        const char *cs = IS_NULL_fn(operand) ? "" : VARVAL_fn(operand);
        return CSETVAL(icn_cset_complement(cs));
    }
    case TT_CSET_UNION: {
        DESCR_t lv = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        char _lb[64], _rb[64];
        const char *a = IS_INT_fn(lv) ? (snprintf(_lb,sizeof _lb,"%lld",(long long)lv.i),_lb)
                      : IS_REAL_fn(lv) ? (real_str(lv.r,_lb,sizeof _lb),_lb) : VARVAL_fn(lv);
        const char *b = IS_INT_fn(rv) ? (snprintf(_rb,sizeof _rb,"%lld",(long long)rv.i),_rb)
                      : IS_REAL_fn(rv) ? (real_str(rv.r,_rb,sizeof _rb),_rb) : VARVAL_fn(rv);
        if (!a) a=""; if (!b) b="";
        return CSETVAL(icn_cset_canonical(icn_cset_union(a, b)));
    }
    case TT_CSET_DIFF: {
        DESCR_t lv = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        char _lb[64], _rb[64];
        const char *a = IS_INT_fn(lv) ? (snprintf(_lb,sizeof _lb,"%lld",(long long)lv.i),_lb)
                      : IS_REAL_fn(lv) ? (real_str(lv.r,_lb,sizeof _lb),_lb) : VARVAL_fn(lv);
        const char *b = IS_INT_fn(rv) ? (snprintf(_rb,sizeof _rb,"%lld",(long long)rv.i),_rb)
                      : IS_REAL_fn(rv) ? (real_str(rv.r,_rb,sizeof _rb),_rb) : VARVAL_fn(rv);
        if (!a) a=""; if (!b) b="";
        return CSETVAL(icn_cset_canonical(icn_cset_diff(a, b)));
    }
    case TT_CSET_INTER: {
        DESCR_t lv = bb_eval_value(e->c[0]);
        if (IS_FAIL_fn(lv)) return FAILDESCR;
        DESCR_t rv = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        char _lb[64], _rb[64];
        const char *a = IS_INT_fn(lv) ? (snprintf(_lb,sizeof _lb,"%lld",(long long)lv.i),_lb)
                      : IS_REAL_fn(lv) ? (real_str(lv.r,_lb,sizeof _lb),_lb) : VARVAL_fn(lv);
        const char *b = IS_INT_fn(rv) ? (snprintf(_rb,sizeof _rb,"%lld",(long long)rv.i),_rb)
                      : IS_REAL_fn(rv) ? (real_str(rv.r,_rb,sizeof _rb),_rb) : VARVAL_fn(rv);
        if (!a) a=""; if (!b) b="";
        return CSETVAL(icn_cset_canonical(icn_cset_inter(a, b)));
    }
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
    case TT_EVERY: {
        if (e->n < 1) return NULVCL;
        tree_t *gen  = e->c[0];
        tree_t *body = (e->n > 1) ? e->c[1] : NULL;
        if (gen->t == TT_ASSIGN &&
            gen->n >= 2 && is_suspendable(gen->c[1])) {
            tree_t *leaf = find_leaf_suspendable(gen->c[1]);
            if (!leaf) leaf = gen->c[1];
            bb_node_t rbox = icn_bb_build(leaf);
            DESCR_t tick = rbox.fn(rbox.ζ, α);
            while (!IS_FAIL_fn(tick) && !FRAME.returning && !FRAME.loop_break) {
                FRAME.loop_next = 0;
                tree_e saved_t = leaf->t;
                union { char *sval; long long ival; double dval; } saved_v;
                saved_v.sval = leaf->v.sval;
                if (tick.v == DT_R) { leaf->t = TT_FLIT; leaf->v.dval = tick.r; }
                else if (tick.v == DT_S || tick.v == DT_SNUL) { leaf->t = TT_QLIT; leaf->v.sval = (tick.v == DT_SNUL) ? "" : (char *)tick.s; }
                else { leaf->t = TT_ILIT; leaf->v.ival = tick.i; }
                (void)bb_eval_value(gen);
                leaf->t = saved_t; leaf->v.sval = saved_v.sval;
                if (body) bb_exec_stmt(body);
                if (FRAME.returning || FRAME.loop_break) break;
                tick = rbox.fn(rbox.ζ, β);
            }
            FRAME.loop_break = 0;
            FRAME.loop_next  = 0;
            return FAILDESCR;
        }
        if (gen->t == TT_SEQ && gen->n >= 2 && is_suspendable(gen->c[0])) {
            tree_t *filter = gen->c[0];
            bb_node_t fbox = icn_bb_build(filter);
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
            return FAILDESCR;
        }
        bb_node_t box = icn_bb_build(gen);
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
        return FAILDESCR;
    }
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
                    if (frame_depth > 0 && lhs->_id >= 0 && lhs->_id < FRAME.env_n)
                        sl->val = FRAME.env[lhs->_id];
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
                            && lhs->_id >= 0 && lhs->_id < FRAME.env_n) {
                            FRAME.env[lhs->_id] = ent->s[si].val;
                            restored = 1;
                        }
                    }
                }
                if (!restored) NV_SET_fn(ent->s[si].nm, ent->s[si].val);
            }
        }
        return NULVCL;
    }
    case TT_SWAP: {
        if (e->n < 2 || frame_depth <= 0) return NULVCL;
        tree_t *lhs = e->c[0], *rhs = e->c[1];
        DESCR_t lv = bb_eval_value(lhs), rv = bb_eval_value(rhs);
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
        if (lhs && lhs->t == TT_VAR) {
            if (lhs->v.sval && lhs->v.sval[0] == '&') {
                if (!kw_assign(lhs->v.sval + 1, rv)) return FAILDESCR;
            } else {
                int sl=lhs->_id;
                if (sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=rv;
                else if (sl<0&&lhs->v.sval) NV_SET_fn(lhs->v.sval,rv);
            }
        }
        if (rhs && rhs->t == TT_VAR) {
            if (rhs->v.sval && rhs->v.sval[0] == '&') {
                if (!kw_assign(rhs->v.sval + 1, lv)) return FAILDESCR;
            } else {
                int sl=rhs->_id;
                if (sl>=0&&sl<FRAME.env_n) FRAME.env[sl]=lv;
                else if (sl<0&&rhs->v.sval) NV_SET_fn(rhs->v.sval,lv);
            }
        }
        return rv;
    }
    case TT_IF: {
        if (e->n < 1) return NULVCL;
        tree_t *test = e->c[0];
        DESCR_t cv;
        if (is_suspendable(test)) {
            bb_node_t box = icn_bb_build(test);
            cv = box.fn(box.ζ, α);
        } else {
            cv = bb_eval_value(test);
        }
        if (!IS_FAIL_fn(cv))
            return (e->n > 1) ? bb_eval_value(e->c[1]) : cv;
        return (e->n > 2) ? bb_eval_value(e->c[2]) : FAILDESCR;
    }
    case TT_PROC_FAIL: {
        if (frame_depth > 0) {
            FRAME.return_val = FAILDESCR;
            FRAME.returning  = 1;
        }
        return FAILDESCR;
    }
    case TT_REVASSIGN: {
        if (e->n < 2) return NULVCL;
        DESCR_t val = bb_eval_value(e->c[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        tree_t *lhs = e->c[0];
        if (lhs && lhs->t == TT_VAR) {
            int slot = lhs->_id;
            if (slot >= 0 && slot < FRAME.env_n) FRAME.env[slot] = val;
            else if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') set_and_trace(lhs->v.sval, val);
        } else if (lhs && lhs->t == TT_IDX && lhs->n >= 2) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(base)) {
                DESCR_t idx = bb_eval_value(lhs->c[1]);
                if (!IS_FAIL_fn(idx)) subscript_set(base, idx, val);
            }
        } else if (lhs && lhs->t == TT_FIELD && ICN_FIELD_NAME(lhs)) {
            DESCR_t obj = bb_eval_value(lhs->c[0]);
            if (!IS_FAIL_fn(obj)) {
                DESCR_t *cell = data_field_ptr(ICN_FIELD_NAME(lhs), obj);
                if (cell) *cell = val;
            }
        }
        return val;
    }
    case TT_BANG_BINARY: {
        bb_node_t box = icn_bb_build(e);
        return box.fn(box.ζ, α);
    }
    case TT_LOOP_BREAK: {
        FRAME.loop_break = 1;
        return (e->n > 0) ? bb_eval_value(e->c[0]) : NULVCL;
    }
    case TT_RETURN: {
        if (frame_depth > 0) {
            FRAME.return_val = (e->n > 0)
                ? bb_eval_value(e->c[0]) : NULVCL;
            FRAME.returning = 1;
            return FRAME.return_val;
        }
        return (e->n > 0) ? bb_eval_value(e->c[0]) : NULVCL;
    }
    case TT_INTERROGATE:
        return bb_eval_value(e->c[0]);
    case TT_GLOBAL:
        return NULVCL;
    case TT_INDIRECT: {
        const tree_t *ch = e->c[0];
        if (ch && ch->t == TT_NAME && ch->n == 1) {
            const tree_t *inner = ch->c[0];
            if (inner && inner->t == TT_IDX && inner->n >= 2
                    && inner->c[0] && inner->c[0]->t == TT_VAR
                    && inner->c[0]->v.sval) {
                int nargs = inner->n;
                if (nargs > 64) nargs = 64;
                DESCR_t args[64];
                for (int i = 0; i < nargs; i++) args[i] = bb_eval_value(inner->c[i]);
                DESCR_t out = FAILDESCR;
                icn_try_call_builtin_by_name("IDX", args, nargs, &out);
                return out;
            }
        }
        DESCR_t arg0 = bb_eval_value(ch);
        if (IS_FAIL_fn(arg0)) return FAILDESCR;
        DESCR_t out = FAILDESCR;
        icn_try_call_builtin_by_name("INDIR_GET", &arg0, 1, &out);
        return out;
    }
    case TT_NAME: {
        const char *vname = (e->c[0] && e->c[0]->v.sval) ? e->c[0]->v.sval : "";
        DESCR_t arg = BSTRVAL((char *)vname, (uint32_t)strlen(vname));
        DESCR_t out = FAILDESCR;
        icn_try_call_builtin_by_name("NAME_PUSH", &arg, 1, &out);
        return out;
    }
    case TT_RECORD: {
        int nfields = e->n;
        DESCR_t *args = GC_malloc((size_t)(nfields + 1) * sizeof(DESCR_t));
        const char *rname = e->v.sval ? e->v.sval : "";
        args[0] = BSTRVAL((char *)rname, (uint32_t)strlen(rname));
        for (int i = 0; i < nfields; i++) {
            args[i + 1] = bb_eval_value(e->c[i]);
            if (IS_FAIL_fn(args[i + 1])) return FAILDESCR;
        }
        DESCR_t out = FAILDESCR;
        icn_try_call_builtin_by_name("RECORD_MAKE", args, nfields + 1, &out);
        return out;
    }
    case TT_VLIST: {
        if (e->n == 0) return NULVCL;
        if (e->n == 1) return bb_eval_value(e->c[0]);
        for (int i = 0; i < e->n; i++) {
            DESCR_t v = bb_eval_value(e->c[i]);
            if (!IS_FAIL_fn(v)) return v;
        }
        return FAILDESCR;
    }
    case TT_OPSYN: {
        const char *raw = e->v.sval ? e->v.sval : "&";
        char op_buf[4];
        const char *op = raw;
        const char *lp = strchr(raw, '(');
        if (lp && lp[1] && lp[2] == ')') { op_buf[0] = lp[1]; op_buf[1] = '\0'; op = op_buf; }
        else if (strcmp(raw, "BARFN")  == 0) op = "|";
        else if (strcmp(raw, "AROWFN") == 0) op = "^";
        int nargs = e->n;
        DESCR_t *args = nargs > 0 ? GC_malloc((size_t)nargs * sizeof(DESCR_t)) : NULL;
        for (int i = 0; i < nargs; i++) {
            args[i] = bb_eval_value(e->c[i]);
            if (IS_FAIL_fn(args[i])) return FAILDESCR;
        }
        DESCR_t out = FAILDESCR;
        if (icn_try_call_builtin_by_name(op, args, nargs, &out)) return out;
        return FAILDESCR;
    }
    default:
        break;
    }
    fprintf(stderr,
            "FATAL bb_eval_value: unhandled kind %d (RS-23e isolation breach)\n",
            (int)e->t);
    abort();
}
