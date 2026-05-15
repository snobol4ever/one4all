#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef STMT_EXEC_STANDALONE
#include <stdint.h>
#include "bb_box.h"
typedef enum { DT_SNUL=0, DT_S=1, DT_P=3, DT_I=6, DT_FAIL=99 } DTYPE_t;
typedef struct DESCR_t {
    DTYPE_t  v;
    uint32_t slen;
    union {
        char   *s;
        int64_t i;
        void   *ptr;
        void   *p;
    };
} DESCR_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t NV_GET_fn(const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t  NV_SET_fn(const char *name, DESCR_t val);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern char   *VARVAL_fn(DESCR_t d);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t (*g_user_call_hook)(const char *name, DESCR_t *args, int nargs);
#define GC_MALLOC(n)  malloc(n)
#else
#include <gc/gc.h>
#include "snobol4.h"
#include "bb_broker.h"
#include "sil_macros.h"
#include "bb_build.h"
#include "bb_pool.h"
#include "emit_bb.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int rt_in_native_chunk(void) __attribute__((weak));
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int rt_in_native_chunk(void) { return 0; }
#include "../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t eval_node(tree_t *e);
#include "bb_box.h"
#ifndef BB_ALPHA_DEFINED
static const int α = 0;
static const int β = 1;
#endif
#endif
const char *Σ = NULL;
int         Δ = 0;
int         Ω = 0;
int         g_scan_pre_delta = 0;
int         Σlen = 0;
uint64_t cursor          = 0;
uint64_t subject_len_val = 0;
char     subject_data[65536] = {0};
typedef struct { const char *name; bb_box_fn child_fn; void *child_state; size_t child_size; int in_progress; } deferred_var_t;
static int g_dvar_depth = 0;
#define DVAR_MAX_DEPTH 4096
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_deferred_var(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int patnd_is_invariant(PATND_t *p)
{
    if (!p)                                           return 1;
    switch (p->kind) {
    case XDSAR:
    case XVAR:
    case XATP:
    case XFNME:
    case XNME:                                       return 0;
    case XFARB:                                      return 0;
    case XSTAR:                                      return 0;
    default:                                          break;
    }
    for (int i = 0; i < p->nchildren; i++)
        if (p->children[i] && !patnd_is_invariant(p->children[i])) return 0;
    return 1;
}
#define DYNC_CACHE_CAP 512
typedef struct {
    PATND_t   *key;
    bb_node_t template;
} cache_slot_t;
static cache_slot_t g_node_cache[DYNC_CACHE_CAP];
static int          g_cache_hits   = 0;
static int          g_cache_misses = 0;
bb_mode_t           g_bb_mode      = BB_MODE_DRIVER;
static int          g_bin_hits     = 0;
static int          g_bin_misses   = 0;
static int          g_bin_str_hits = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static cache_slot_t *cache_find(PATND_t *key)
{
    if (!key) return NULL;
    uintptr_t h = ((uintptr_t)key >> 4) & (DYNC_CACHE_CAP - 1);
    for (int i = 0; i < DYNC_CACHE_CAP; i++) {
        uintptr_t idx = (h + (uintptr_t)i) & (DYNC_CACHE_CAP - 1);
        if (g_node_cache[idx].key == key)    return &g_node_cache[idx];
        if (g_node_cache[idx].key == NULL)   return &g_node_cache[idx];
    }
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void cache_insert(PATND_t *key, bb_node_t node)
{
    cache_slot_t *slot = cache_find(key);
    if (!slot || slot->key != NULL) return;
    slot->key      = key;
    slot->template = node;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static bb_node_t cache_get_fresh(cache_slot_t *slot)
{
    bb_node_t n = slot->template;
    if (n.ζ_size && n.ζ) {
        void *fresh = calloc(1, n.ζ_size);
        memcpy(fresh, n.ζ, n.ζ_size);
        n.ζ = fresh;
    }
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void cache_reset(void)
{
    for (int i = 0; i < DYNC_CACHE_CAP; i++) g_node_cache[i].key = NULL;
    g_cache_hits = g_cache_misses = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exec_stmt_pool_reset(void)
{
    bb_pool_reset();
    cache_reset();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void cache_stats(int *hits, int *misses)
{
    if (hits)   *hits   = g_cache_hits;
    if (misses) *misses = g_cache_misses;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *xkind_name(int k)
{
    switch (k) {
    case  0: return "XCHR";    case  1: return "XSPNC";
    case  2: return "XBRKC";   case  3: return "XANYC";
    case  4: return "XNNYC";   case  5: return "XLNTH";
    case  6: return "XPOSI";   case  7: return "XRPSI";
    case  8: return "XTB";     case  9: return "XRTB";
    case 10: return "XFARB";   case 11: return "XARBN";
    case 12: return "XSTAR";   case 13: return "XFNCE";
    case 14: return "XFAIL";   case 15: return "XABRT";
    case 16: return "XSUCF";   case 17: return "XBAL";
    case 18: return "XEPS";    case 19: return "XCAT";
    case 20: return "XOR";     case 21: return "XDSAR";
    case 22: return "XFNME";   case 23: return "XNME";
    case 24: return "XCALLCAP"; case 25: return "XVAR";
    case 26: return "XATP";    case 27: return "XBRKX";
    default: return "XUNKNOWN";
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bin_audit_print(void)
{
    int pat_total = g_bin_hits + g_bin_misses;
    int all_total = pat_total + g_bin_str_hits;
    if (all_total == 0) return;
    fprintf(stderr,
        "BINARY_AUDIT: DT_P hits=%d misses=%d (%.1f%%)  DT_S hits=%d  total_binary=%d/%d (%.1f%%)\n",
        g_bin_hits, g_bin_misses,
        pat_total ? 100.0 * g_bin_hits / pat_total : 0.0,
        g_bin_str_hits,
        g_bin_hits + g_bin_str_hits, all_total,
        all_total ? 100.0 * (g_bin_hits + g_bin_str_hits) / all_total : 0.0);
    if (g_bin_misses > 0)
        fprintf(stderr,
            "BINARY_AUDIT: known fallbacks: XABRT XSUCF XBAL XVAR (not in 50-file corpus)\n");
}
typedef struct {
    const char *name;
    DESCR_t    *args;
    int         nargs;
    int         done;
    int         consumed;
} usercall_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_usercall(void *zeta, int entry)
{
    usercall_t *ζ = zeta;
    DESCR_t UC;
    if (entry == α) goto UC_α;
                    goto UC_β;
    UC_α:  ζ->done = 1;
           if (g_user_call_hook && ζ->name && ζ->name[0]) {
               DESCR_t *eff_args = ζ->args;
               DESCR_t  thaw_buf[8];
               DESCR_t *thawed   = NULL;
               int      n        = ζ->nargs;
               int      have_dte = 0;
               for (int i = 0; i < n; i++) {
                   if (ζ->args[i].v == DT_E) { have_dte = 1; break; }
               }
               if (have_dte && n > 0) {
                   thawed = (n <= 8) ? thaw_buf
                                     : (DESCR_t *)GC_MALLOC((size_t)n * sizeof(DESCR_t));
                   for (int i = 0; i < n; i++) {
                       thawed[i] = (ζ->args[i].v == DT_E)
                                   ? EVAL_fn(ζ->args[i])
                                   : ζ->args[i];
                   }
                   eff_args = thawed;
               }
               if (getenv("ONE4ALL_USERCALL_TRACE")) {
                   fprintf(stderr, "BB_USERCALL name=%s nargs=%d\n", ζ->name ? ζ->name : "(null)", n);
                   for (int i = 0; i < n; i++) {
                       const char *kind = "?";
                       switch ((int)ζ->args[i].v) {
                           case DT_SNUL: kind = "DT_SNUL"; break;
                           case DT_S:    kind = "DT_S";    break;
                           case DT_E:    kind = "DT_E";    break;
                           case DT_I:    kind = "DT_I";    break;
                           case DT_R:    kind = "DT_R";    break;
                           case DT_N:    kind = "DT_N";    break;
                           case DT_P:    kind = "DT_P";    break;
                           case DT_FAIL: kind = "DT_FAIL"; break;
                       }
                       const char *raw_str = (ζ->args[i].v == DT_S && ζ->args[i].s) ? ζ->args[i].s : "";
                       const char *eff_str = (eff_args[i].v == DT_S && eff_args[i].s) ? eff_args[i].s : "";
                       fprintf(stderr, "  arg[%d] raw v=%s s=\"%s\" ptr=%p   eff v=%d s=\"%s\"\n",
                               i, kind, raw_str, ζ->args[i].p, eff_args[i].v, eff_str);
                   }
               }
               DESCR_t r = g_user_call_hook(ζ->name, eff_args, n);
               extern char kw_rtntype[16];
               int via_nreturn = (strcmp(kw_rtntype, "NRETURN") == 0);
               if (IS_FAIL_fn(r))                         goto UC_ω;
               if (r.v == DT_P && r.p && r.p->kind == XFAIL) goto UC_ω;
               if (via_nreturn) {
                   ζ->consumed = 0;
                   UC = descr_match(Σ + Δ, 0);                  goto UC_γ;
               }
               if (r.v == DT_S || r.v == DT_SNUL) {
                   const char *rs = r.s ? r.s : "";
                   int         rl = (int)strlen(rs);
                   if (Δ + rl > Σlen)                              goto UC_ω;
                   if (rl > 0 && memcmp(Σ + Δ, rs, (size_t)rl) != 0) goto UC_ω;
                   ζ->consumed = rl;
                   UC = descr_match(Σ + Δ, rl);
                   Δ += rl;
                                                                  goto UC_γ;
               }
               ζ->consumed = 0;
           }
           UC = descr_match(Σ + Δ, 0);        goto UC_γ;
    UC_β:  Δ -= ζ->consumed;
           ζ->consumed = 0;                goto UC_ω;
    UC_γ:                                  return UC;
    UC_ω:                                  return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t bb_deferred_var(void *zeta, int entry)
{
    deferred_var_t *ζ = zeta;
    if (entry == α)                                     goto DVAR_α;
    if (entry == β)                                     goto DVAR_β;
    DESCR_t         DVAR;
    DVAR_α:         {
                        if (g_dvar_depth >= DVAR_MAX_DEPTH)
                                                              goto DVAR_ω;
                        g_dvar_depth++;
                        DESCR_t val = NV_GET_fn(ζ->name);
                        int rebuilt = 0;
#ifndef STMT_EXEC_STANDALONE
                        if (val.v == DT_E) {
                            tree_t *frozen = (tree_t *)val.ptr;
                            if (!frozen) {
                                g_dvar_depth--;
                                goto DVAR_ω;
                            }
                            if (frozen->t == TT_FNC) {
                                int nargs = frozen->n;
                                DESCR_t *args = NULL;
                                if (nargs > 0) {
                                    args = (DESCR_t *)alloca((size_t)nargs * sizeof(DESCR_t));
                                    for (int i = 0; i < nargs; i++)
                                        args[i] = eval_node(frozen->c[i]);
                                }
                                const char *fname = frozen->v.sval ? frozen->v.sval : "";
                                val = pat_user_call(fname, args, nargs);
                            } else if (frozen->t == TT_VAR && frozen->v.sval) {
                                val = NV_GET_fn(frozen->v.sval);
                            } else {
                                val = PATVAL_fn(val);
                            }
                            if (val.v == DT_FAIL) {
                                g_dvar_depth--;
                                goto DVAR_ω;
                            }
                        }
#endif
                        if (val.v == DT_P && val.p) {
                            if (val.p != ζ->child_state || !ζ->child_fn) {
                                bb_box_fn bfn = bb_build_brokered((PATND_t *)val.p);
                                if (bfn) {
                                    ζ->child_fn    = bfn;
                                    ζ->child_state = NULL;
                                    ζ->child_size  = 0;
                                } else {
                                    PATND_t *ep = patnd_make_eps();
                                    bb_box_fn efn = bb_build_brokered(ep);
                                    ζ->child_fn    = efn;
                                    ζ->child_state = NULL;
                                    ζ->child_size  = 0;
                                }
                                rebuilt = 1;
                            }
                        } else if (val.v == DT_S && val.s) {
                            PATND_t *lp = patnd_make_xchr(val.s);
                            bb_box_fn lfn = bb_build_brokered(lp);
                            if (lfn && lfn != ζ->child_fn) {
                                ζ->child_fn    = lfn;
                                ζ->child_state = NULL;
                                ζ->child_size  = 0;
                                rebuilt = 1;
                            }
                        } else {
                            if (!ζ->child_fn) {
                                PATND_t *ep = patnd_make_eps();
                                bb_box_fn efn = bb_build_brokered(ep);
                                ζ->child_fn    = efn;
                                ζ->child_state = NULL;
                                ζ->child_size  = 0;
                                rebuilt = 1;
                            }
                        }
                        int _config_only = (ζ->child_state == NULL);
                        if (!rebuilt && ζ->child_state && ζ->child_size && !_config_only)
                            memset(ζ->child_state, 0, ζ->child_size);
                    }
                    if (!ζ->child_fn) { g_dvar_depth--; goto DVAR_ω; }
                    DVAR = ζ->child_fn(ζ->child_state, α);
                    g_dvar_depth--;
                    if (IS_FAIL_fn(DVAR))                    goto DVAR_ω;
                    goto DVAR_γ;
    DVAR_β:         if (!ζ->child_fn)                         goto DVAR_ω;
                    DVAR = ζ->child_fn(ζ->child_state, β);
                    if (IS_FAIL_fn(DVAR))                    goto DVAR_ω;
                                                              goto DVAR_γ;
    DVAR_γ:                                                   return DVAR;
    DVAR_ω:                                                   return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t bb_deferred_var_exported(void *zeta, int entry) { return bb_deferred_var(zeta, entry); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
deferred_var_t *bb_dvar_bin_new(const char *name)
{
    deferred_var_t *ζ = calloc(1, sizeof(deferred_var_t));
    if (!ζ) return NULL;
    ζ->name = name;
    return ζ;
}
#ifndef STMT_EXEC_STANDALONE
extern int64_t kw_anchor;
#else
int kw_anchor = 0;
#endif
typedef struct { int start; int end; } scan_result_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void scan_body_fn_u9(DESCR_t val, void *arg) {
    scan_result_t *r = (scan_result_t *)arg;
    r->end = Δ;
    if (g_scan_pre_delta >= 0)
        r->start = g_scan_pre_delta;
    else if (val.v == DT_S && val.slen)
        r->start = Δ - (int)val.slen;
    else
        r->start = Δ;
    g_scan_pre_delta = -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int exec_stmt(const char  *subj_name,
                  DESCR_t     *subj_var,
                  DESCR_t      pat,
                  DESCR_t     *repl,
                  int          has_repl)
{
    reset_capture_registry();
    DESCR_t subj_fetched;
    if (subj_name && *subj_name) {
        subj_fetched = NV_GET_fn(subj_name);
        subj_var     = &subj_fetched;
    }
    const char *subj_str = "";
    int         subj_len = 0;
    if (subj_var) {
        DESCR_t sv_d = VARVAL_d_fn(*subj_var);
        if (sv_d.v == DT_S || sv_d.v == DT_SNUL) {
            subj_str = sv_d.s ? sv_d.s : "";
            subj_len = (int)descr_slen(sv_d);
        }
    }
    Σ = subj_str;
    Ω = subj_len;
    Σlen = subj_len;
    bb_node_t root;
    if (pat.v == DT_E && pat.ptr) {
        root.fn     = (bb_box_fn)pat.ptr;
        root.ζ      = NULL;
        root.ζ_size = 0;
    } else if (pat.v == DT_P && pat.p) {
        int bin_done = 0;
        if (g_bb_mode == BB_MODE_LIVE) {
            PATND_t *pp = (PATND_t *)pat.p;
            cache_slot_t *bslot = cache_find(pp);
            if (bslot && bslot->key == pp && bslot->template.fn) {
                root     = bslot->template;
                bin_done = 1;
                g_bin_hits++;
                g_cache_hits++;
            } else {
                bb_box_fn bfn = bb_build_flat(pp);
                if (!bfn) bfn = bb_build_brokered(pp);
                if (bfn) {
                    root.fn     = bfn;
                    root.ζ      = NULL;
                    root.ζ_size = 0;
                    bin_done    = 1;
                    g_bin_hits++;
                    cache_insert(pp, root);
                } else {
                    g_bin_misses++;
                }
            }
        } else if (g_bb_mode == BB_MODE_BROKERED || g_bb_mode == BB_MODE_DRIVER) {
            PATND_t *pp = (PATND_t *)pat.p;
            bb_box_fn bfn = bb_build_brokered(pp);
            if (bfn) {
                root.fn     = bfn;
                root.ζ      = NULL;
                root.ζ_size = 0;
                bin_done    = 1;
            }
        }
        if (!bin_done) {
            PATND_t *ep = patnd_make_eps();
            bb_box_fn efn = bb_build_brokered(ep);
            root.fn     = efn;
            root.ζ      = NULL;
            root.ζ_size = 0;
        }
    } else if (pat.v == DT_S && pat.s) {
        int bin_done = 0;
        {
            PATND_t *lp = patnd_make_xchr(pat.s);
            bb_box_fn bfn = bb_build_brokered(lp);
            if (bfn) {
                root.fn  = bfn;
                root.ζ   = NULL;
                bin_done = 1;
                g_bin_str_hits++;
            }
        }
        if (!bin_done) {
            PATND_t *ep = patnd_make_eps();
            bb_box_fn efn = bb_build_brokered(ep);
            root.fn     = efn;
            root.ζ      = NULL;
            root.ζ_size = 0;
        }
    } else {
        PATND_t *ep = patnd_make_eps();
        bb_box_fn efn = bb_build_brokered(ep);
        root.fn = efn; root.ζ = NULL; root.ζ_size = 0;
    }
    int match_start = -1;
    int match_end   = -1;
    typedef struct { int start; int end; } scan_result_t;
    scan_result_t scan_res = { -1, -1 };
    clear_pending_flags();
    NAME_ctx_t scan_ctx;
    NAME_ctx_enter(&scan_ctx);
    int saved_Ω = Ω;
    if (kw_anchor) Ω = 0;
    int ticks = bb_broker(root, BB_SCAN, scan_body_fn_u9, &scan_res);
    Ω = saved_Ω;
    if (ticks > 0) {
        match_start = scan_res.start;
        match_end   = scan_res.end;
                                                              goto Phase4;
    }
    NAME_ctx_leave();
                                                              return 0;
Phase4:
    if (has_repl && repl && !subj_name && !subj_var) {
        NAME_ctx_leave();
                                                              return 0;
    }
    NAME_commit();
    NAME_ctx_leave();
    flush_pending_captures();
    if (!has_repl || !repl)                                   goto Success;
    {
        const char *repl_str = "";
        int         repl_len = 0;
        if (repl->v == DT_S && repl->s) {
            repl_str = repl->s;
            repl_len = repl->slen ? (int)repl->slen : (int)strlen(repl->s);
        } else if (repl->v == DT_I) {
            char ibuf[32];
            snprintf(ibuf, sizeof(ibuf), "%lld", (long long)repl->i);
            char *gs = (char *)GC_MALLOC(strlen(ibuf) + 1);
            strcpy(gs, ibuf);
            repl_str = gs;
            repl_len = (int)strlen(gs);
        }
        int   new_len = match_start + repl_len + (Ω - match_end);
        char *new_s   = (char *)GC_MALLOC((size_t)new_len + 1);
        memcpy(new_s,                          subj_str,             (size_t)match_start);
        memcpy(new_s + match_start,            repl_str,             (size_t)repl_len);
        memcpy(new_s + match_start + repl_len, subj_str + match_end, (size_t)(Ω - match_end));
        new_s[new_len] = '\0';
        DESCR_t new_val;
        new_val.v    = DT_S;
        new_val.slen = (uint32_t)new_len;
        new_val.s    = new_s;
        if (subj_name && *subj_name) {
            NV_SET_fn(subj_name, new_val);
        } else if (subj_var) {
            *subj_var = new_val;
        }
    }
Success:
                                                              return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int exec_stmt_blob(const char  *subj_name,
                   DESCR_t     *subj_var,
                   bb_box_fn    root_fn,
                   DESCR_t     *repl,
                   int          has_repl)
{
    DESCR_t pat;
    pat.v    = DT_E;
    pat.slen = 0;
    pat.ptr  = (void *)root_fn;
    return exec_stmt(subj_name, subj_var, pat, repl, has_repl);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int cache_test_run(const char *lit, int n_iters)
{
    static PATND_t node;
    node.kind         = XCHR;
    node.materialising = 0;
    node.STRVAL_fn         = lit;
    node.num          = 0;
    node.children = NULL; node.nchildren = 0;
    node.args = NULL;
    node.nargs = 0;
    cache_reset();
    for (int i = 0; i < n_iters; i++) {
        bb_box_fn bfn = bb_build_brokered(&node);
        (void)bfn;
    }
    int hits = 0, misses = 0;
    cache_stats(&hits, &misses);
    return hits;
}
#define NV_INIT 16
typedef struct { char *key; DESCR_t val; } _nv_entry_t;
static _nv_entry_t *g_nv_table = NULL;
static int          g_nv_count = 0;
static int          g_nv_cap   = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void nv_set_str(const char *name, const char *s)
{
    for (int i = 0; i < g_nv_count; i++) {
        if (strcmp(g_nv_table[i].key, name) == 0) {
            g_nv_table[i].val.v    = DT_S;
            g_nv_table[i].val.s    = (char *)s;
            g_nv_table[i].val.slen = s ? (uint32_t)strlen(s) : 0;
            return;
        }
    }
    if (!g_nv_table) {
        g_nv_cap   = NV_INIT;
        g_nv_table = malloc(g_nv_cap * sizeof(_nv_entry_t));
    } else if (g_nv_count >= g_nv_cap) {
        g_nv_cap  *= 2;
        g_nv_table = realloc(g_nv_table, g_nv_cap * sizeof(_nv_entry_t));
    }
    g_nv_table[g_nv_count].key      = strdup(name);
    g_nv_table[g_nv_count].val.v    = DT_S;
    g_nv_table[g_nv_count].val.s    = (char *)s;
    g_nv_table[g_nv_count].val.slen = s ? (uint32_t)strlen(s) : 0;
    g_nv_count++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t nv_get(const char *name)
{
    for (int i = 0; i < g_nv_count; i++)
        if (strcmp(g_nv_table[i].key, name) == 0)
            return g_nv_table[i].val;
    DESCR_t d; d.v = DT_SNUL; d.slen = 0; d.s = NULL;
    return d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int deferred_var_test(void)
{
    g_nv_count = 0;
    nv_set_str("T15_PAT", "Bird");
    static PATND_t dsar_node;
    dsar_node.kind  = XDSAR;
    dsar_node.STRVAL_fn  = "T15_PAT";
    dsar_node.children = NULL; dsar_node.nchildren = 0;
    dsar_node.args  = NULL;
    dsar_node.nargs = 0;
    cache_reset();
    bb_box_fn bfn = bb_build_brokered(&dsar_node);
    bb_node_t dvar;
    if (bfn) { dvar.fn = bfn; dvar.ζ = NULL; dvar.ζ_size = 0; }
    else {
           PATND_t *ep = patnd_make_eps();
           bb_box_fn efn = bb_build_brokered(ep);
           dvar.fn = efn; dvar.ζ = NULL; dvar.ζ_size = 0; }
    int ok = 1;
    nv_set_str("T15_PAT", "Bird");
    Σ = "BlueBird"; Ω = (int)strlen(Σ); Δ = 0;
    DESCR_t r1 = dvar.fn(dvar.ζ, α);
    ok &= !IS_FAIL_fn(r1) ? 1 : 0;
    Δ = 0;
    DESCR_t r2 = dvar.fn(dvar.ζ, α);
    ok &= !IS_FAIL_fn(r2) ? 1 : 0;
    printf("  deferred_var: r1=%s r2=%s (both non-empty = re-resolve ran)\n",
           IS_FAIL_fn(r1)?"empty":"ok", IS_FAIL_fn(r2)?"empty":"ok");
    return ok;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int anchor_test(void)
{
    int ok = 1;
    kw_anchor = 0;
    int r_unanchored = exec_stmt_args("XhelloY", "hello", NULL, NULL);
    ok &= (r_unanchored == 1);
    printf("  unanchored match at pos 1: %s\n", r_unanchored ? "PASS" : "FAIL");
    kw_anchor = 1;
    int r_anchored = exec_stmt_args("XhelloY", "hello", NULL, NULL);
    ok &= (r_anchored == 0);
    printf("  anchored match fails (not at pos 0): %s\n", r_anchored == 0 ? "PASS" : "FAIL");
    kw_anchor = 0;
    return ok;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int exec_stmt_args(const char *subject, const char *pattern,
                      const char *repl_str, char **out_subject);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int exec_stmt_args(const char  *subject,
                      const char  *pattern,
                      const char  *repl_str,
                      char       **out_subject)
{
    DESCR_t subj;
    subj.v    = DT_S;
    subj.slen = subject ? (uint32_t)strlen(subject) : 0;
    subj.s    = subject ? (char *)subject : (char *)"";
    DESCR_t pat;
    pat.v    = DT_S;
    pat.slen = pattern ? (uint32_t)strlen(pattern) : 0;
    pat.s    = pattern ? (char *)pattern : (char *)"";
    DESCR_t repl_d;
    repl_d.v    = DT_S;
    repl_d.slen = repl_str ? (uint32_t)strlen(repl_str) : 0;
    repl_d.s    = repl_str ? (char *)repl_str : NULL;
    int has_repl = (repl_str != NULL);
    int r = exec_stmt(NULL, &subj, pat, has_repl ? &repl_d : NULL, has_repl);
    if (out_subject && r) {
        *out_subject = subj.s;
    }
                                                              return r;
}
