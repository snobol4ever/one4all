#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "snobol4.h"
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
DESCR_t (*g_eval_str_hook)(const char *s) = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static PATND_t *spat_new(XKIND_t kind) {
    PATND_t *p = (PATND_t *)GC_MALLOC(sizeof(PATND_t));
    memset(p, 0, sizeof(PATND_t));
    p->kind = kind;
    return p;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void patnd_set_children(PATND_t *p, PATND_t **ch, int n) {
    int count = 0;
    for (int i = 0; i < n; i++) if (ch[i]) count++;
    if (count == 0) return;
    p->children = (PATND_t **)GC_MALLOC((size_t)count * sizeof(PATND_t *));
    int j = 0;
    for (int i = 0; i < n; i++) if (ch[i]) p->children[j++] = ch[i];
    p->nchildren = count;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void patnd_append_child(PATND_t *p, PATND_t *ch) {
    if (!ch) return;
    p->children = (PATND_t **)GC_REALLOC(p->children,
                      (size_t)(p->nchildren + 1) * sizeof(PATND_t *));
    p->children[p->nchildren++] = ch;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t spat_val(PATND_t *p) {
    DESCR_t v;
    v.v = DT_P;
    v.p    = (struct _PATND_t *)p;
    return v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline PATND_t *spat_of(DESCR_t v) {
    if (v.v != DT_P) return NULL;
    return (PATND_t *)v.p;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_lit(const char *s) {
    PATND_t *p = spat_new(XCHR);
    p->STRVAL_fn = s ? GC_strdup(s) : "";
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_span(const char *chars) {
    PATND_t *p = spat_new(XSPNC);
    p->STRVAL_fn = chars ? GC_strdup(chars) : "";
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_break_(const char *chars) {
    PATND_t *p = spat_new(XBRKC);
    p->STRVAL_fn = chars ? GC_strdup(chars) : "";
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_breakx(const char *chars) {
    PATND_t *p = spat_new(XBRKX);
    p->STRVAL_fn = chars ? GC_strdup(chars) : "";
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_any_cs(const char *chars) {
    PATND_t *p = spat_new(XANYC);
    p->STRVAL_fn = chars ? GC_strdup(chars) : "";
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_notany(const char *chars) {
    PATND_t *p = spat_new(XNNYC);
    p->STRVAL_fn = chars ? GC_strdup(chars) : "";
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_len(int64_t n) {
    if (n < 0) { sno_runtime_error(14, NULL); return FAILDESCR; }
    PATND_t *p = spat_new(XLNTH);
    p->num = n;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_pos(int64_t n) {
    if (n < 0) { sno_runtime_error(14, NULL); return FAILDESCR; }
    PATND_t *p = spat_new(XPOSI);
    p->num = n;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_rpos(int64_t n) {
    if (n < 0) { sno_runtime_error(14, NULL); return FAILDESCR; }
    PATND_t *p = spat_new(XRPSI);
    p->num = n;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_tab(int64_t n) {
    if (n < 0) { sno_runtime_error(14, NULL); return FAILDESCR; }
    PATND_t *p = spat_new(XTB);
    p->num = n;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_rtab(int64_t n) {
    if (n < 0) { sno_runtime_error(14, NULL); return FAILDESCR; }
    PATND_t *p = spat_new(XRTB);
    p->num = n;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_arb(void) {
    return spat_val(spat_new(XFARB));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_arbno(DESCR_t inner) {
    PATND_t *p = spat_new(XARBN);
    PATND_t *ch = spat_of(inner);
    if (!ch && inner.v == DT_S) ch = spat_of(pat_lit(inner.s));
    PATND_t *arr[1] = { ch };
    patnd_set_children(p, arr, 1);
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_rem(void) {
    return spat_val(spat_new(XSTAR));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_fence_p(DESCR_t inner) {
    PATND_t *p = spat_new(XFNCE);
    PATND_t *ch = spat_of(inner);
    if (!ch && inner.v == DT_S && inner.s) ch = spat_of(pat_lit(inner.s));
    if (!ch && inner.v == DT_SNUL)         ch = spat_of(pat_lit(""));
    if (!ch && inner.v == DT_I)            { char buf[32]; snprintf(buf,sizeof buf,"%lld",(long long)inner.i); ch = spat_of(pat_lit(buf)); }
    if (!ch && inner.v == DT_R)            { char buf[64]; snprintf(buf,sizeof buf,"%.14g",inner.r); ch = spat_of(pat_lit(buf)); }
    PATND_t *arr[1] = { ch };
    patnd_set_children(p, arr, 1);
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_fence(void) {
    return spat_val(spat_new(XFNCE));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_fail(void) {
    return spat_val(spat_new(XFAIL));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_abort(void) {
    return spat_val(spat_new(XABRT));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_succeed(void) {
    return spat_val(spat_new(XSUCF));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_bal(void) {
    return spat_val(spat_new(XBAL));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_epsilon(void) {
    return spat_val(spat_new(XEPS));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
PATND_t *patnd_make_xchr(const char *lit) {
    PATND_t *p = (PATND_t *)GC_MALLOC(sizeof(PATND_t));
    memset(p, 0, sizeof(PATND_t));
    p->kind = XCHR;
    p->STRVAL_fn = lit ? GC_strdup(lit) : "";
    return p;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
PATND_t *patnd_make_eps(void) {
    PATND_t *p = (PATND_t *)GC_MALLOC(sizeof(PATND_t));
    memset(p, 0, sizeof(PATND_t));
    p->kind = XEPS;
    return p;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t eval_node(tree_t *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static PATND_t *pat_to_patnd(DESCR_t v) {
    if (v.v == DT_E) {
        if (v.slen == 1) {
            v = EXPVAL_fn(v);
            goto coerce;
        }
        tree_t *frozen = (tree_t *)v.ptr;
        if (!frozen) return NULL;
        if (frozen->t == TT_FNC) {
            int nargs = frozen->n;
            DESCR_t *args = NULL;
            if (nargs > 0) {
                args = (DESCR_t *)alloca((size_t)nargs * sizeof(DESCR_t));
                for (int i = 0; i < nargs; i++)
                    args[i] = eval_node(frozen->c[i]);
            }
            const char *fname = frozen->v.sval ? frozen->v.sval : "";
            DESCR_t pv = pat_user_call(fname, args, nargs);
            if (IS_FAIL_fn(pv)) return NULL;
            PATND_t *pp = spat_of(pv);
            if (pp) return pp;
            v = pv;
        }
        if (frozen->t == TT_VAR && frozen->v.sval) {
            DESCR_t name_d = STRVAL(frozen->v.sval);
            DESCR_t pv = var_as_pattern(name_d);
            return spat_of(pv);
        }
        v = PATVAL_fn(v);
        if (v.v == DT_FAIL) return NULL;
    }
    coerce:
    if (v.v == DT_N) {
        if (v.slen == 1 && v.ptr) v = *(DESCR_t *)v.ptr;
        else if (v.slen == 0 && v.s) v = NV_GET_fn(v.s);
        else v = NULVCL;
    }
    PATND_t *p = spat_of(v);
    if (!p && v.v == DT_S) p = (v.s && v.s[0]) ? spat_of(pat_lit(v.s)) : spat_of(pat_lit(""));
    if (!p && v.v == DT_I) { char buf[32]; snprintf(buf,sizeof buf,"%lld",(long long)v.i); p = spat_of(pat_lit(buf)); }
    if (!p && v.v == DT_R) { char buf[64]; snprintf(buf,sizeof buf,"%.14g",v.r); p = spat_of(pat_lit(buf)); }
    if (!p && v.v == DT_SNUL) p = spat_of(pat_lit(""));
    return p;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_cat(DESCR_t left, DESCR_t right) {
    PATND_t *l = pat_to_patnd(left);
    PATND_t *r = pat_to_patnd(right);
    if (!l && left.v  != DT_SNUL) {
        fprintf(stderr, "pat_cat: left is not a pattern (DT=%d) — dropping\n", left.v);
    }
    if (!r && right.v != DT_SNUL) {
        fprintf(stderr, "pat_cat: right is not a pattern (DT=%d) — dropping\n", right.v);
    }
    if (!l) return r ? spat_val(r) : pat_epsilon();
    if (!r) return spat_val(l);
    PATND_t *p = spat_new(XCAT);
    if (l->kind == XCAT) {
        for (int i = 0; i < l->nchildren; i++) patnd_append_child(p, l->children[i]);
    } else {
        patnd_append_child(p, l);
    }
    if (r->kind == XCAT) {
        for (int i = 0; i < r->nchildren; i++) patnd_append_child(p, r->children[i]);
    } else {
        patnd_append_child(p, r);
    }
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_alt(DESCR_t left, DESCR_t right) {
    PATND_t *l = pat_to_patnd(left);
    PATND_t *r = pat_to_patnd(right);
    if (!l && left.v  == DT_SNUL) l = spat_of(pat_epsilon());
    if (!r && right.v == DT_SNUL) r = spat_of(pat_epsilon());
    if (!l) return r ? spat_val(r) : pat_epsilon();
    if (!r) return spat_val(l);
    PATND_t *p = spat_new(XOR);
    if (l->kind == XOR) {
        for (int i = 0; i < l->nchildren; i++) patnd_append_child(p, l->children[i]);
    } else {
        patnd_append_child(p, l);
    }
    if (r->kind == XOR) {
        for (int i = 0; i < r->nchildren; i++) patnd_append_child(p, r->children[i]);
    } else {
        patnd_append_child(p, r);
    }
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_ref(const char *name) {
    PATND_t *p = spat_new(XDSAR);
    p->STRVAL_fn = name ? GC_strdup(name) : "";
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_ref_val(DESCR_t nameVal) {
    return pat_ref(VARVAL_fn(nameVal));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_assign_imm(DESCR_t child, DESCR_t var) {
    PATND_t *p = spat_new(XFNME);
    PATND_t *ch = pat_to_patnd(child);
    PATND_t *arr[1] = { ch };
    patnd_set_children(p, arr, 1);
    p->var  = var;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_assign_cond(DESCR_t child, DESCR_t var) {
    PATND_t *p = spat_new(XNME);
    PATND_t *ch = pat_to_patnd(child);
    PATND_t *arr[1] = { ch };
    patnd_set_children(p, arr, 1);
    p->var  = var;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_assign_callcap(DESCR_t child, const char *fnc_name, DESCR_t *args, int nargs) {
    return pat_assign_callcap_named(child, fnc_name, args, nargs, NULL, 0);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_assign_callcap_named(DESCR_t child, const char *fnc_name,
                                  DESCR_t *args, int nargs,
                                  char **arg_names, int n_arg_names) {
    PATND_t *p = spat_new(XCALLCAP);
    PATND_t *ch = pat_to_patnd(child);
    PATND_t *arr[1] = { ch };
    patnd_set_children(p, arr, 1);
    p->STRVAL_fn = fnc_name ? GC_strdup(fnc_name) : "";
    p->args  = args;
    p->nargs = nargs;
    p->arg_names   = arg_names;
    p->n_arg_names = n_arg_names;
    p->imm = 0;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_assign_callcap_named_imm(DESCR_t child, const char *fnc_name,
                                      DESCR_t *args, int nargs,
                                      char **arg_names, int n_arg_names) {
    PATND_t *p = spat_new(XCALLCAP);
    PATND_t *ch = pat_to_patnd(child);
    PATND_t *arr[1] = { ch };
    patnd_set_children(p, arr, 1);
    p->STRVAL_fn = fnc_name ? GC_strdup(fnc_name) : "";
    p->args  = args;
    p->nargs = nargs;
    p->arg_names   = arg_names;
    p->n_arg_names = n_arg_names;
    p->imm = 1;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t var_as_pattern(DESCR_t v) {
    if (v.v == DT_P) return v;
    if (v.v == DT_S || v.v == DT_SNUL) {
        return pat_lit(VARVAL_fn(v));
    }
    PATND_t *p = spat_new(XVAR);
    p->var = v;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_user_call(const char *name, DESCR_t *args, int nargs) {
    PATND_t *p = spat_new(XATP);
    p->STRVAL_fn   = name ? GC_strdup(name) : "";
    p->nargs = nargs;
    if (nargs > 0) {
        p->args = (DESCR_t *)GC_MALLOC(nargs * sizeof(DESCR_t));
        memcpy(p->args, args, nargs * sizeof(DESCR_t));
    }
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_at_cursor(const char *varname) {
    PATND_t *p = spat_new(XATP);
    p->STRVAL_fn = "@";
    p->nargs = 1;
    p->args  = (DESCR_t *)GC_MALLOC(sizeof(DESCR_t));
    p->args[0].v    = DT_S;
    p->args[0].s    = varname ? GC_strdup(varname) : "";
    p->args[0].slen = varname ? (uint32_t)strlen(varname) : 0;
    return spat_val(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t array_create(DESCR_t spec) {
    const char *s = VARVAL_fn(spec);
    int lo = 1, hi = 1;
    const char *colon = strchr(s, ':');
    int bare = 0;
    if (colon) {
        lo = atoi(s);
        hi = atoi(colon + 1);
    } else {
        hi = atoi(s);
        bare = 1;
    }
    if (hi < lo) hi = lo;
    ARBLK_t *a = array_new(lo, hi);
    a->proto_bare = bare;
    DESCR_t v;
    v.v = DT_A;
    v.arr    = a;
    return v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t subscript_get(DESCR_t arr, DESCR_t idx) {
    if (arr.v == DT_A) {
        return array_get(arr.arr, (int)to_int(idx));
    }
    if (arr.v == DT_T) {
        char kb[64]; const char *ks;
        if (IS_INT_fn(idx))       { snprintf(kb,sizeof kb,"%lld",(long long)idx.i); ks=kb; }
        else if (IS_REAL_fn(idx)) { snprintf(kb,sizeof kb,"%g",idx.r); ks=kb; }
        else                      { ks = VARVAL_fn(idx); if (!ks) ks=""; }
        if (!table_has(arr.tbl, ks)) {
            if (arr.tbl->dflt.v != DT_FAIL && arr.tbl->dflt.v != 0)
                return arr.tbl->dflt;
            return NULVCL;
        }
        return table_get(arr.tbl, ks);
    }
    if (arr.v == DT_I) {
        char ibuf[32]; snprintf(ibuf, sizeof ibuf, "%lld", (long long)arr.i);
        arr = STRVAL(GC_strdup(ibuf));
    }
    if (arr.v == DT_S || arr.v == DT_SNUL) {
        const char *s = arr.s ? arr.s : "";
        int slen = (int)strlen(s);
        int i = (int)to_int(idx);
        if (i < 0) i = slen + i + 1;
        if (i < 1 || i > slen) return FAILDESCR;
        char *buf = GC_malloc(2); buf[0] = s[i-1]; buf[1] = '\0';
        return STRVAL(buf);
    }
    if (arr.v == DT_DATA) {
        DESCR_t tag = FIELD_GET_fn(arr, "icn_type");
        if (tag.v == DT_S && tag.s && strcmp(tag.s,"list")==0) {
            int n = (int)FIELD_GET_fn(arr,"frame_size").i;
            DESCR_t ea = FIELD_GET_fn(arr,"frame_elems");
            DESCR_t *elems = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
            int i = (int)to_int(idx);
            if (i < 0) i = n + 1 + i + 1;
            if (!elems || i < 1 || i > n) return FAILDESCR;
            return elems[i-1];
        }
        if (arr.u && arr.u->type && arr.u->type->nfields > 0 && arr.u->fields) {
            DATBLK_t *blk = arr.u->type;
            if (IS_INT_fn(idx)) {
                int i = (int)idx.i;
                if (i < 1 || i > blk->nfields) return FAILDESCR;
                return arr.u->fields[i-1];
            }
            if (idx.v == DT_S || idx.v == DT_SNUL) {
                const char *k = idx.s ? idx.s : "";
                for (int i = 0; i < blk->nfields; i++)
                    if (blk->fields[i] && strcmp(blk->fields[i], k) == 0)
                        return arr.u->fields[i];
                return FAILDESCR;
            }
        }
        int i = (int)to_int(idx);
        DESCR_t children = FIELD_GET_fn(arr, "c");
        if (children.v == DT_A && children.arr)
            return array_get(children.arr, i);
        return FAILDESCR;
    }
    sno_runtime_error(3, NULL);
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int subscript_set(DESCR_t arr, DESCR_t idx, DESCR_t val) {
    if (arr.v == DT_A) {
        int i = (int)to_int(idx);
        if (i < arr.arr->lo || i > arr.arr->hi) return 0;
        array_set(arr.arr, i, val);
        return 1;
    }
    if (arr.v == DT_T) {
        const char *k = VARVAL_fn(idx);
        table_set_descr(arr.tbl, k ? k : "", idx, val);
        return 1;
    }
    if (arr.v == DT_DATA) {
        DESCR_t tag = FIELD_GET_fn(arr, "icn_type");
        if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
            int n = (int)FIELD_GET_fn(arr, "frame_size").i;
            DESCR_t ea = FIELD_GET_fn(arr, "frame_elems");
            DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
            int i = (int)to_int(idx);
            if (i < 0) i = n + 1 + i + 1;
            if (!elems || i < 1 || i > n) return 0;
            elems[i - 1] = val;
            return 1;
        }
        if (arr.u && arr.u->type && arr.u->fields) {
            DATBLK_t *blk = arr.u->type;
            if (IS_INT_fn(idx)) {
                int i = (int)idx.i;
                if (i < 1 || i > blk->nfields) return 0;
                arr.u->fields[i - 1] = val;
                return 1;
            }
            if (idx.v == DT_S || idx.v == DT_SNUL) {
                const char *k = idx.s ? idx.s : "";
                for (int i = 0; i < blk->nfields; i++)
                    if (blk->fields[i] && strcmp(blk->fields[i], k) == 0) {
                        arr.u->fields[i] = val;
                        return 1;
                    }
                return 0;
            }
        }
        return 0;
    }
    if (arr.v == DT_S && arr.s) {
        int slen = (int)strlen(arr.s);
        int i = (int)to_int(idx);
        if (i < 0) i = slen + 1 + i;
        if (i < 1 || i > slen) { sno_runtime_error(3, NULL); return 0; }
        const char *vs = VARVAL_fn(val);
        if (!vs) vs = "";
        int vlen = (int)strlen(vs);
        int newlen = slen - 1 + vlen;
        char *ns = GC_malloc(newlen + 1);
        memcpy(ns, arr.s, i - 1);
        memcpy(ns + i - 1, vs, vlen);
        memcpy(ns + i - 1 + vlen, arr.s + i, slen - i + 1);
        char *live = (char *)arr.s;
        if (vlen == 1) {
            live[i - 1] = vs[0];
        } else {
            memmove(live + i - 1 + vlen, live + i, slen - i + 1);
            memcpy(live + i - 1, vs, vlen);
        }
        return 1;
    }
    sno_runtime_error(3, NULL);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t subscript_get2(DESCR_t arr, DESCR_t i, DESCR_t j) {
    if (arr.v == DT_A)
        return array_get2(arr.arr, (int)to_int(i), (int)to_int(j));
    if (arr.v == DT_DATA) {
        DESCR_t tag = FIELD_GET_fn(arr, "icn_type");
        if (tag.v == DT_S && tag.s && strcmp(tag.s,"list")==0) {
            int n = (int)FIELD_GET_fn(arr,"frame_size").i;
            DESCR_t ea = FIELD_GET_fn(arr,"frame_elems");
            DESCR_t *elems = (ea.v==DT_DATA) ? (DESCR_t*)ea.ptr : NULL;
            int ii = (int)to_int(i), jj = (int)to_int(j);
            if (ii < 0) ii = n + 1 + ii + 1;
            if (jj < 0) jj = n + 1 + jj + 1;
            if (ii < 1) ii = 1; if (jj > n) jj = n;
            if (ii > jj) {
                static int icnlist_empty_reg = 0;
                if (!icnlist_empty_reg) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_empty_reg=1; }
                DESCR_t empty_ptr; empty_ptr.v=DT_DATA; empty_ptr.slen=0; empty_ptr.ptr=NULL;
                return DATCON_fn("icnlist", empty_ptr, INTVAL(0), STRVAL("list"));
            }
            int rlen = jj - ii + 1;
            DESCR_t *rbuf = GC_malloc(rlen * sizeof(DESCR_t));
            for (int k = 0; k < rlen; k++) rbuf[k] = (elems && ii+k-1 >= 0 && ii+k-1 < n) ? elems[ii+k-1] : NULVCL;
            DESCR_t rptr; rptr.v=DT_DATA; rptr.slen=0; rptr.ptr=(void*)rbuf;
            static int icnlist_slice_reg = 0;
            if (!icnlist_slice_reg) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_slice_reg=1; }
            return DATCON_fn("icnlist", rptr, INTVAL(rlen), STRVAL("list"));
        }
    }
    if (arr.v == DT_S || arr.v == DT_SNUL) {
        const char *s = arr.s ? arr.s : "";
        int slen = (int)strlen(s);
        int ii = (int)to_int(i), jj = (int)to_int(j);
        if (ii < 0) ii = slen + 1 + ii + 1;
        if (jj < 0) jj = slen + 1 + jj + 1;
        if (ii < 1) ii = 1; if (jj > slen+1) jj = slen+1;
        if (ii > jj) { char *e=GC_malloc(1); e[0]='\0'; return STRVAL(e); }
        int len = jj - ii;
        char *buf = GC_malloc(len+1); memcpy(buf, s+ii-1, len); buf[len]='\0';
        return STRVAL(buf);
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int subscript_set2(DESCR_t arr, DESCR_t i, DESCR_t j, DESCR_t val) {
    if (arr.v == DT_A) {
        int ii = (int)to_int(i), jj = (int)to_int(j);
        if (ii < arr.arr->lo || ii > arr.arr->hi) return 0;
        if (arr.arr->ndim >= 2 && (jj < arr.arr->lo2 || jj > arr.arr->hi2)) return 0;
        array_set2(arr.arr, ii, jj, val);
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t MAKE_TREE_fn(DESCR_t tag, DESCR_t val, DESCR_t n_children, DESCR_t children) {
    return DATCON_fn("tree", tag, val, n_children, children, (DESCR_t){0});
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t push_val(DESCR_t x) {
    PUSH_fn(x);
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pop_val(void) {
    return POP_fn();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t top_val(void) {
    return TOP_fn();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void register_fn(const char *name, DESCR_t (*fn)(DESCR_t*, int), int min_args, int max_args) {
    (void)min_args; (void)max_args;
    DEFINE_fn(name, fn);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void define_spec(DESCR_t spec) {
    DEFINE_fn(VARVAL_fn(spec), NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t apply_val(DESCR_t fnval, DESCR_t *args, int nargs) {
    const char *name = VARVAL_fn(fnval);
    return APPLY_fn(name, args, nargs);
}
typedef struct { const char *s; int pos; } SnoEvalCtx;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _ev_skip(SnoEvalCtx *e) {
    while (e->s[e->pos] == ' ' || e->s[e->pos] == '\t') e->pos++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *_ev_ident(SnoEvalCtx *e) {
    int start = e->pos;
    while (isalnum((unsigned char)e->s[e->pos]) || e->s[e->pos] == '_') e->pos++;
    int len = e->pos - start;
    if (len == 0) return NULL;
    char *nm = GC_malloc(len + 1);
    memcpy(nm, e->s + start, len);
    nm[len] = '\0';
    return nm;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *_ev_strlit(SnoEvalCtx *e) {
    char delim = e->s[e->pos]; e->pos++;
    int start = e->pos;
    while (e->s[e->pos] && e->s[e->pos] != delim) e->pos++;
    int len = e->pos - start;
    char *lit = GC_malloc(len + 1);
    memcpy(lit, e->s + start, len);
    lit[len] = '\0';
    if (e->s[e->pos] == delim) e->pos++;
    return lit;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ev_val(SnoEvalCtx *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ev_term(SnoEvalCtx *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ev_expr(SnoEvalCtx *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int _ev_args(SnoEvalCtx *e, DESCR_t *args, int maxargs) {
    int na = 0;
    _ev_skip(e);
    while (e->s[e->pos] && e->s[e->pos] != ')') {
        int pos_before = e->pos;
        if (na > 0) { if (e->s[e->pos] == ',') e->pos++; _ev_skip(e); }
        if (na < maxargs) args[na++] = _ev_val(e);
        else _ev_val(e);
        _ev_skip(e);
        if (e->pos == pos_before) break;
    }
    if (e->s[e->pos] == ')') e->pos++;
    return na;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ev_val(SnoEvalCtx *e) {
    _ev_skip(e);
    char c = e->s[e->pos];
    if (c == '\'' || c == '"') return STRVAL(_ev_strlit(e));
    if (isalpha((unsigned char)c) || c == '_') {
        char *nm = _ev_ident(e);
        _ev_skip(e);
        if (e->s[e->pos] == '(') {
            e->pos++;
            DESCR_t args[8]; int na = _ev_args(e, args, 8);
            return APPLY_fn(nm, args, na);
        }
        return NV_GET_fn(nm);
    }
    if (isdigit((unsigned char)c) || c == '-') {
        char *end;
        long long iv = strtoll(e->s + e->pos, &end, 10);
        e->pos = (int)(end - e->s);
        return INTVAL(iv);
    }
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ev_term(SnoEvalCtx *e) {
    _ev_skip(e);
    char c = e->s[e->pos];
    if (c == '*') {
        e->pos++;
        _ev_skip(e);
        char *nm = _ev_ident(e);
        if (!nm) return pat_epsilon();
        _ev_skip(e);
        if (e->s[e->pos] == '(') {
            e->pos++;
            DESCR_t args[8]; int na = _ev_args(e, args, 8);
            DESCR_t *ac = na ? GC_malloc(na * sizeof(DESCR_t)) : NULL;
            if (ac) memcpy(ac, args, na * sizeof(DESCR_t));
            return pat_user_call(nm, ac, na);
        }
        return pat_ref(nm);
    }
    if (c == '\'' || c == '"') return pat_lit(_ev_strlit(e));
    if (isalpha((unsigned char)c) || c == '_') {
        char *nm = _ev_ident(e);
        _ev_skip(e);
        if (e->s[e->pos] == '(') {
            e->pos++;
            DESCR_t args[8]; int na = _ev_args(e, args, 8);
            return APPLY_fn(nm, args, na);
        }
        return STRVAL(GC_strdup(nm));
    }
    return pat_epsilon();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ev_expr(SnoEvalCtx *e) {
    DESCR_t left = _ev_term(e);
    if (left.v == DT_S) {
        DESCR_t v = NV_GET_fn(left.s);
        if (v.v == DT_P) left = v;
        else if (v.v == DT_S && v.s && v.s[0]) left = pat_lit(v.s);
        else left = pat_epsilon();
    } else if (left.v == DT_SNUL) {
        left = pat_epsilon();
    }
    _ev_skip(e);
    while (e->s[e->pos] == '.') {
        e->pos++;
        _ev_skip(e);
        DESCR_t right = _ev_term(e);
        _ev_skip(e);
        if (right.v == DT_S) {
            left = pat_assign_cond(left, right);
        } else {
            if (right.v == DT_SNUL) right = pat_epsilon();
            left = pat_cat(left, right);
        }
    }
    return left;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t EVAL_fn(DESCR_t expr) {
    if (expr.v == DT_E) {
        return EXPVAL_fn(expr);
    }
    if (expr.v == DT_I) return expr;
    if (expr.v == DT_R) return expr;
    if (expr.v == DT_P) {
        if (g_eval_pat_hook) return g_eval_pat_hook(expr);
        return expr;
    }
    const char *s = VARVAL_fn(expr);
    if (!s || !*s) return NULVCL;
    {
        char *endp = NULL;
        int64_t iv = (int64_t)strtoll(s, &endp, 10);
        if (endp && *endp == '\0') return INTVAL(iv);
    }
    {
        char *endp = NULL;
        double rv = strtod(s, &endp);
        if (endp && *endp == '\0') return REALVAL(rv);
    }
    if (g_eval_str_hook) return g_eval_str_hook(s);
    DESCR_t compiled = CONVE_fn(expr);
    if (IS_FAIL_fn(compiled)) { fprintf(stderr, "DBG IS_FAIL true!\n"); return FAILDESCR; }
    DESCR_t _ev2 = EXPVAL_fn(compiled);
    return _ev2;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t opsyn(DESCR_t newname, DESCR_t oldname, DESCR_t type) {
    (void)type;
    const char *nm  = VARVAL_fn(newname);
    const char *old = NULL;
    if (oldname.v == DT_N) {
        if (oldname.slen == 0 && oldname.s && *oldname.s)
            old = oldname.s;
        else if (oldname.slen == 1 && oldname.ptr)
            old = NV_name_from_ptr((const DESCR_t *)oldname.ptr);
    }
    if (!old) old = VARVAL_fn(oldname);
    if (!nm || !old || !*old) return FAILDESCR;
    register_fn_alias(nm, old);
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int _sort_type_rank(DESCR_t d) {
    switch (d.v) {
        case DT_A: return 0;
        case DT_C: return 1;
        case DT_E: return 2;
        case DT_I: return 3;
        case DT_P: return 6;
        case DT_R: return 7;
        case DT_S: return 8;
        case DT_SNUL: return 8;
        case DT_T: return 9;
        default:   return 5;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int _sort_cmp_descr(DESCR_t a, DESCR_t b, const char *sa, const char *sb) {
    if (a.v == DT_I && b.v == DT_I) {
        if (a.i < b.i) return -1;
        if (a.i > b.i) return  1;
        return 0;
    }
    if ((a.v == DT_S || a.v == DT_SNUL) && (b.v == DT_S || b.v == DT_SNUL)) {
        return strcmp(sa ? sa : "", sb ? sb : "");
    }
    int ra = _sort_type_rank(a), rb = _sort_type_rank(b);
    if (ra != rb) return ra - rb;
    return strcmp(sa ? sa : "", sb ? sb : "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sort_fn(DESCR_t arr) {
    if (arr.v != DT_T) return arr;
    TBBLK_t *tbl = arr.tbl;
    if (!tbl) return FAILDESCR;
    int n = 0;
    for (int h = 0; h < TABLE_BUCKETS; h++)
        for (TBPAIR_t *e = tbl->buckets[h]; e; e = e->next) n++;
    if (n == 0) return FAILDESCR;
    const char **keys = GC_malloc(n * sizeof(char *));
    DESCR_t *key_descrs = GC_malloc(n * sizeof(DESCR_t));
    DESCR_t *vals = GC_malloc(n * sizeof(DESCR_t));
    int idx = 0;
    for (int h = 0; h < TABLE_BUCKETS; h++)
        for (TBPAIR_t *e = tbl->buckets[h]; e; e = e->next) {
            keys[idx] = e->key;
            key_descrs[idx] = e->key_descr;
            vals[idx] = e->val;
            idx++;
        }
    int *order = GC_malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) {
        int tmp = order[i];
        int j = i - 1;
        while (j >= 0 &&
               _sort_cmp_descr(key_descrs[order[j]], key_descrs[tmp],
                               keys[order[j]],      keys[tmp]) > 0) {
            order[j+1] = order[j]; j--;
        }
        order[j+1] = tmp;
    }
    ARBLK_t *a = GC_malloc(sizeof(ARBLK_t));
    a->lo         = 1;
    a->hi         = n;
    a->ndim       = 2;
    a->lo2        = 1;
    a->hi2        = 2;
    a->proto_bare = 1;
    a->data = GC_malloc(n * 2 * sizeof(DESCR_t));
    for (int i = 0; i < n; i++) {
        a->data[i * 2 + 0] = key_descrs[order[i]];
        a->data[i * 2 + 1] = vals[order[i]];
    }
    DESCR_t result = {0};
    result.v = DT_A;
    result.arr    = a;
    return result;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pat_call(const char *name, DESCR_t arg) {
    DESCR_t args[1] = { arg };
    DESCR_t result = APPLY_fn(name, args, 1);
    if (IS_FAIL_fn(result)) return pat_fail();
    return var_as_pattern(result);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t compile_to_expression(const char *src) {
    if (!src || !*src) return FAILDESCR;
    tree_t *tree = parse_expr_pat_from_str(src);
    if (!tree) return FAILDESCR;
    DESCR_t d;
    d.v    = DT_E;
    d.slen = 0;
    d.s    = NULL;
    d.ptr  = tree;
    return d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t rsort_fn(DESCR_t arr) {
    DESCR_t sorted = sort_fn(arr);
    if (sorted.v != DT_A || !sorted.arr) return sorted;
    ARBLK_t *a = sorted.arr;
    int n = a->hi - a->lo + 1;
    for (int lo = 0, hi = n - 1; lo < hi; lo++, hi--) {
        DESCR_t tmp0 = a->data[lo*2+0], tmp1 = a->data[lo*2+1];
        a->data[lo*2+0] = a->data[hi*2+0];
        a->data[lo*2+1] = a->data[hi*2+1];
        a->data[hi*2+0] = tmp0;
        a->data[hi*2+1] = tmp1;
    }
    return sorted;
}
#include <stdio.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *xkind_name(XKIND_t k) {
    switch (k) {
        case XCHR:     return "CHR";
        case XSPNC:    return "SPAN";
        case XBRKC:    return "BREAK";
        case XANYC:    return "ANY";
        case XNNYC:    return "NOTANY";
        case XLNTH:    return "LEN";
        case XPOSI:    return "POS";
        case XRPSI:    return "RPOS";
        case XTB:      return "TAB";
        case XRTB:     return "RTAB";
        case XFARB:    return "ARB";
        case XARBN:    return "ARBNO";
        case XSTAR:    return "REM";
        case XFNCE:    return "FENCE";
        case XFAIL:    return "FAIL";
        case XABRT:    return "ABORT";
        case XSUCF:    return "SUCCEED";
        case XBAL:     return "BAL";
        case XEPS:     return "EPS";
        case XCAT:     return "CAT";
        case XOR:      return "ALT";
        case XDSAR:    return "DEREF";
        case XFNME:    return "CAP_IMM";
        case XNME:     return "CAP_COND";
        case XCALLCAP: return "CALLCAP";
        case XVAR:     return "VAR";
        case XATP:     return "USERPAT";
        case XBRKX:    return "BREAKX";
        default:       return "?";
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void patnd_print_r(const PATND_t *p, FILE *out, int depth) {
    if (!p) { fprintf(out, "%*s(null)\n", depth*2, ""); return; }
    fprintf(out, "%*s(%s", depth*2, "", xkind_name(p->kind));
    if (p->STRVAL_fn) fprintf(out, " \"%s\"", p->STRVAL_fn);
    if (p->num)       fprintf(out, " %lld", (long long)p->num);
    if (p->nchildren == 0) {
        fprintf(out, ")\n");
    } else {
        fprintf(out, "\n");
        for (int i = 0; i < p->nchildren; i++)
            patnd_print_r(p->children[i], out, depth + 1);
        fprintf(out, "%*s)\n", depth*2, "");
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void patnd_print(const PATND_t *p, FILE *out) {
    patnd_print_r(p, out, 0);
}
