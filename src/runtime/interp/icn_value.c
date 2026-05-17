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
/* DESCR_t-input helpers reused by both AST-walking (bb_eval_value) and IR (ir_exec.c). */
DESCR_t icn_str_concat_d(DESCR_t a, DESCR_t b)
{
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
DESCR_t icn_lconcat_d(DESCR_t a, DESCR_t b)
{
    if (IS_FAIL_fn(a) || IS_FAIL_fn(b)) return FAILDESCR;
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
            static int icnlist_lcat_d = 0;
            if (!icnlist_lcat_d) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_lcat_d=1; }
            return DATCON_fn("icnlist", eptr, INTVAL(cn), STRVAL("list"));
        }
    }
    /* Fallback: string concat with coercion (matches TT_LCONCAT spec for non-list operands). */
    return icn_str_concat_d(a, b);
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
/* DAI (IJ-DEL-ICN-AST): bb_eval_value body removed. The Icon-specific tree_t* AST walker is being */
/* amputated. Mode-1 (--ir-run / --ast-run) is no longer a valid Icon execution path. Use          */
/* --sm-run / --jit-run / --sm-native (modes 2/3/4) for Icon programs.                             */
/* DESCR_t-input helpers above (icn_str_concat_d, icn_lconcat_d, cset_resolve, etc.) are still     */
/* used by src/lower/ir_exec.c and stay.                                                            */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t bb_eval_value(tree_t *e)
{
    fprintf(stderr, "[DAI-BOMB] bb_eval_value called (mode-1 Icon AST walker is amputated). "
                    "tree tag=%d. Use --sm-run/--jit-run/--sm-native instead.\n",
                    e ? (int)e->t : -1);
    exit(78);
}
