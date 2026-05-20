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
                                    ζ->child_state = val.p;  /* cache key: pattern ptr */
                                    ζ->child_size  = 0;
                                } else {
                                    PATND_t *ep = patnd_make_eps();
                                    bb_box_fn efn = bb_build_brokered(ep);
                                    ζ->child_fn    = efn;
                                    ζ->child_state = val.p;  /* cache key even for fallback */
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
    int ticks = bb_broker(root, bb_scan, scan_body_fn_u9, &scan_res);
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
