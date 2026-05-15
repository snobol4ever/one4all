#include "icn_runtime.h"
#include "icn_value.h"
#include "icn_stmt.h"
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "bb_broker.h"
#include "../../frontend/icon/icon_gen.h"
#include "coerce.h"
#include "scan_builtins.h"
#include "../../lower/ir_exec.h"
#include "../../lower/lower_icn.h"
#include "../../processor/sm_interp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gc/gc.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern bb_node_t icn_bb_make_proc_box(tree_t *proc, DESCR_t *args, int nargs);
#define NO_AST_WALK_GUARD(fn_name) \
    do { if (g_sm_dispatch_active && !g_ast_pump_active && g_lang == LANG_ICN) { \
        fprintf(stderr, "FATAL: " fn_name " reached from SM dispatch (Icon BB incomplete)\n"); \
        abort(); \
    } } while (0)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t NV_SET_fn(const char *name, DESCR_t val);
IcnProcEntry proc_table[PROC_TABLE_MAX];
int          proc_count = 0;
int          g_lang         = 0;
tree_t      *g_icn_root     = NULL;
int g_sm_dispatch_active = 0;
int g_ast_pump_active = 0;
IcnFrame frame_stack[FRAME_STACK_MAX];
int      frame_depth = 0;
tree_t  *icn_drive_node = NULL;
DESCR_t  icn_drive_val;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void frame_push(tree_t *n, long v, const char *sv) {
    IcnFrame *f = &FRAME;
    if (f->gen_depth < FRAME_DEPTH_MAX) { f->gen[f->gen_depth].node=n; f->gen[f->gen_depth].cur=v; f->gen[f->gen_depth].sval=sv; f->gen_depth++; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void frame_pop(void) { if (FRAME.gen_depth > 0) FRAME.gen_depth--; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  icn_frame_lookup(tree_t *n, long *out) {
    IcnFrame *f = &FRAME;
    for (int i=f->gen_depth-1;i>=0;i--) if(f->gen[i].node==n){*out=f->gen[i].cur;return 1;} return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  icn_frame_lookup_sv(tree_t *n, long *out, const char **sv) {
    IcnFrame *f = &FRAME;
    for (int i=f->gen_depth-1;i>=0;i--) if(f->gen[i].node==n){*out=f->gen[i].cur;*sv=f->gen[i].sval;return 1;} return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  frame_active(tree_t *n) {
    IcnFrame *f = &FRAME;
    for (int i=0;i<f->gen_depth;i++) if(f->gen[i].node==n) return 1; return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_frame_env_active(void) {
    return frame_depth > 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_frame_env_load(int slot) {
    if (frame_depth <= 0) return FAILDESCR;
    IcnFrame *f = &FRAME;
    if (slot < 0 || slot >= f->env_n) return FAILDESCR;
    return f->env[slot];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_frame_env_store(int slot, DESCR_t val) {
    if (frame_depth <= 0) return;
    IcnFrame *f = &FRAME;
    if (slot < 0 || slot >= FRAME_SLOT_MAX) return;
    if (slot >= f->env_n) f->env_n = slot + 1;
    f->env[slot] = val;
}
const char *scan_subj  = "";
int         scan_pos   = 1;
ScanEntry scan_stack[SCAN_STACK_MAX];
int         scan_depth = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_yield_to_caller(DESCR_t v) { (void)v; return 0; }
const char *global_names[GLOBAL_MAX];
int         global_count = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int is_global(const char *name) {
    for (int i = 0; i < global_count; i++)
        if (global_names[i] && strcmp(global_names[i], name) == 0) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void global_register(const char *name) {
    if (!name || is_global(name) || global_count >= GLOBAL_MAX) return;
    global_names[global_count++] = name;
}
typedef struct {
    int         entry_pc;
    const char *proc_name;
    const char *name;
    DESCR_t     val;
} static_ent_t;
#define STATIC_MAX 256
static static_ent_t static_tab[STATIC_MAX];
static int              static_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int static_proc_entry_pc(const char *proc_name) {
    if (!proc_name) return -1;
    for (int i = 0; i < proc_count; i++)
        if (proc_table[i].name && strcmp(proc_table[i].name, proc_name) == 0)
            return proc_table[i].entry_pc;
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int static_entry_matches(const static_ent_t *e, int epc,
                                const char *pname, const char *vname) {
    if (!e->name || !vname || strcmp(e->name, vname) != 0) return 0;
    if (epc >= 0 && e->entry_pc >= 0) return e->entry_pc == epc;
    return e->proc_name && pname && strcmp(e->proc_name, pname) == 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int static_get(tree_t *proc, const char *name, DESCR_t *out) {
    if (!proc || !name || !out) return 0;
    const char *pname = proc->v.sval;
    int epc = static_proc_entry_pc(pname);
    for (int i = 0; i < static_n; i++) {
        if (static_entry_matches(&static_tab[i], epc, pname, name)) {
            *out = static_tab[i].val;
            return 1;
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void static_set(tree_t *proc, const char *name, DESCR_t val) {
    if (!proc || !name) return;
    const char *pname = proc->v.sval;
    int epc = static_proc_entry_pc(pname);
    for (int i = 0; i < static_n; i++) {
        if (static_entry_matches(&static_tab[i], epc, pname, name)) {
            if (epc >= 0 && static_tab[i].entry_pc < 0)
                static_tab[i].entry_pc = epc;
            static_tab[i].val = val;
            return;
        }
    }
    if (static_n >= STATIC_MAX) return;
    static_tab[static_n].entry_pc  = epc;
    static_tab[static_n].proc_name = pname;
    static_tab[static_n].name      = name;
    static_tab[static_n].val       = val;
    static_n++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int scope_add(IcnScope *sc, const char *name) {
    if (!name) return -1;
    for (int i=0;i<sc->n;i++) if(strcmp(sc->e[i].name,name)==0) return sc->e[i].slot;
    if (sc->n >= FRAME_SLOT_MAX) return -1;
    int slot = sc->n;
    sc->e[sc->n].name=name; sc->e[sc->n].slot=slot; sc->n++;
    return slot;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int scope_get(IcnScope *sc, const char *name) {
    if (!name) return -1;
    for (int i=0;i<sc->n;i++) if(strcmp(sc->e[i].name,name)==0) return sc->e[i].slot;
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_scope_patch(IcnScope *sc, tree_t *e) {
    if (!e) return;
    if (e->t == TT_GLOBAL) {
        for (int i=0;i<e->n;i++)
            if(e->c[i]&&e->c[i]->v.sval) scope_add(sc, e->c[i]->v.sval);
        return;
    }
    if (e->t == TT_VAR && e->v.sval) {
        if (e->v.sval[0] == '&') {
            e->_id = -1;
        } else if (is_global(e->v.sval)) {
            e->_id = -1;
        } else {
            int s = scope_add(sc, e->v.sval);
            e->_id = (s >= 0) ? s : -1;
        }
    }
    int child_start = (e->t == TT_FNC) ? 1 : 0;
    for (int i=child_start;i<e->n;i++) icn_scope_patch(sc, e->c[i]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sm_call_proc(int entry_pc, int nparams, DESCR_t *args, int nargs)
{
    extern DESCR_t sm_call_expression(int epc);
    if (entry_pc < 0) return FAILDESCR;
    if (frame_depth >= FRAME_STACK_MAX) return FAILDESCR;
    IcnFrame *f = &frame_stack[frame_depth++];
    memset(f, 0, sizeof *f);
    int nslots = (nparams > 0) ? nparams : 1;
    if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;
    f->env_n = nslots;
    for (int i = 0; i < nparams && i < nargs && i < FRAME_SLOT_MAX; i++)
        f->env[i] = args[i];
    int found_pi = -1;
    tree_t *found_proc = NULL;
    {
        for (int i = 0; i < proc_count; i++) {
            if (proc_table[i].entry_pc == entry_pc) { found_pi = i; break; }
        }
        if (found_pi >= 0 && proc_table[found_pi].proc) {
            found_proc = proc_table[found_pi].proc;
            int nparams_p = found_proc->_id;
            int body_start = 1 + nparams_p;
            for (int bi = body_start; bi < found_proc->n; bi++)
                icn_scope_patch(&proc_table[found_pi].lower_sc, found_proc->c[bi]);
            int total_slots = proc_table[found_pi].lower_sc.n;
            if (total_slots > f->env_n) f->env_n = total_slots;
            f->sc = proc_table[found_pi].lower_sc;
            IcnScope *sc = &proc_table[found_pi].lower_sc;
            IcnFrame *parent_f = (frame_depth >= 2) ? &frame_stack[frame_depth - 2] : NULL;
            for (int bi = body_start; bi < found_proc->n; bi++) {
                tree_t *st = found_proc->c[bi];
                if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
                for (int j = 0; j < st->n; j++) {
                    tree_t *vn = st->c[j];
                    if (!vn || !vn->v.sval) continue;
                    int slot = scope_get(sc, vn->v.sval);
                    if (slot < 0 || slot >= f->env_n) continue;
                    DESCR_t saved;
                    if (static_get(found_proc, vn->v.sval, &saved)) {
                        f->env[slot] = saved;
                    } else if (parent_f && slot < parent_f->env_n
                               && parent_f->env[slot].v != 0) {
                        f->env[slot] = parent_f->env[slot];
                    }
                }
            }
        }
    }
    DESCR_t result = sm_call_expression(entry_pc);
    if (found_pi >= 0 && found_proc) {
        IcnScope *sc = &proc_table[found_pi].lower_sc;
        int nparams_p = found_proc->_id;
        int body_start = 1 + nparams_p;
        for (int bi = body_start; bi < found_proc->n; bi++) {
            tree_t *st = found_proc->c[bi];
            if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
            for (int j = 0; j < st->n; j++) {
                tree_t *vn = st->c[j];
                if (!vn || !vn->v.sval) continue;
                int slot = scope_get(sc, vn->v.sval);
                if (slot < 0 || slot >= f->env_n) continue;
                static_set(found_proc, vn->v.sval, f->env[slot]);
            }
        }
    }
    icn_init_save_frame();
    frame_depth--;
    return result;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t proc_table_call(int pi, DESCR_t *args, int nargs)
{
    if (pi < 0 || pi >= proc_count) return FAILDESCR;
    extern SM_Program *g_current_sm_prog;
    if (proc_table[pi].entry_pc >= 0 && g_current_sm_prog != NULL)
        return sm_call_proc(proc_table[pi].entry_pc, proc_table[pi].nparams, args, nargs);
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_descr_identical(DESCR_t a, DESCR_t b) {
    if (IS_FAIL_fn(a) || IS_FAIL_fn(b)) return 0;
    int an = (a.v == DT_SNUL) || (a.v == DT_S && (!a.s || !*a.s));
    int bn = (b.v == DT_SNUL) || (b.v == DT_S && (!b.s || !*b.s));
    if (an && bn) return 1;
    if (an != bn) return 0;
    int as_str = (a.v == DT_S || a.v == DT_SNUL);
    int bs_str = (b.v == DT_S || b.v == DT_SNUL);
    if (as_str && bs_str) {
        const char *s1 = a.s ? a.s : ""; size_t l1 = (a.slen > 0 && a.slen != 0xFFFFFFFFu) ? (size_t)a.slen : strlen(s1);
        const char *s2 = b.s ? b.s : ""; size_t l2 = (b.slen > 0 && b.slen != 0xFFFFFFFFu) ? (size_t)b.slen : strlen(s2);
        return (l1 == l2 && memcmp(s1, s2, l1) == 0);
    }
    int a_cset = (a.v == DT_S && a.slen == 0xFFFFFFFFu);
    int b_cset = (b.v == DT_S && b.slen == 0xFFFFFFFFu);
    if (a_cset != b_cset) return 0;
    if (a.v != b.v) return 0;
    if (a.v == DT_I) return a.i == b.i;
    if (a.v == DT_R) return a.r == b.r;
    if (a.v == DT_T) return a.tbl == b.tbl;
    if (a.v == DT_DATA) return a.ptr == b.ptr;
    return memcmp(&a, &b, sizeof(DESCR_t)) == 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int proc_has_suspend(tree_t *t) {
    if (!t) return 0;
    if (t->t == TT_SUSPEND) return 1;
    for (int i = 0; i < t->n; i++) if (proc_has_suspend(t->c[i])) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int is_suspendable(tree_t *e) {
    if (!e) return 0;
    switch (e->t) {
        case TT_TO: case TT_TO_BY: case TT_ITERATE: case TT_ALTERNATE:
        case TT_SUSPEND: case TT_LIMIT:
        case TT_BANG_BINARY: case TT_SEQ_EXPR:
            return 1;
        case TT_FNC:
            return 1;
        case TT_IDX:
            for (int i = 1; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        case TT_ASSIGN:
            if (e->n >= 2 && e->c[0] && e->c[0]->t == TT_ITERATE) return 1;
            return (e->n >= 2 && is_suspendable(e->c[1])) ? 1 : 0;
        case TT_REVASSIGN:
            return 1;
        case TT_REVSWAP:
            return 1;
        case TT_ADD: case TT_SUB: case TT_MUL: case TT_DIV: case TT_MOD:
        case TT_LT:  case TT_LE:  case TT_GT:  case TT_GE:
        case TT_EQ:  case TT_NE:
        case TT_IDENTICAL:
        case TT_LCONCAT: case TT_CAT:
                           for (int i = 0; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        case TT_CSET_COMPL: case TT_CSET_UNION: case TT_CSET_DIFF: case TT_CSET_INTER:
            for (int i = 0; i < e->n; i++)
                if (is_suspendable(e->c[i])) return 1;
            return 0;
        case TT_NONNULL:
            return is_suspendable(e->n > 0 ? e->c[0] : NULL);
        case TT_SCAN:
            return is_suspendable(e->n > 0 ? e->c[0] : NULL)
                || is_suspendable(e->n > 1 ? e->c[1] : NULL);
        case TT_NULL:
            return 0;
        default:
            return 0;
    }
}
typedef struct { DESCR_t val; int fired; } icn_bb_oneshot_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_oneshot(void *zeta, int entry) {
    icn_bb_oneshot_state_t *z = (icn_bb_oneshot_state_t *)zeta;
    if (entry == α) { z->fired = 0; }
    if (!z->fired && !IS_FAIL_fn(z->val)) { z->fired = 1; return z->val; }
    return FAILDESCR;
}
typedef struct { tree_t *expr; } icn_lazy_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_lazy_box(void *zeta, int entry) {
    if (entry != α) return FAILDESCR;
    icn_lazy_state_t *z = (icn_lazy_state_t *)zeta;
    DESCR_t v = bb_eval_value(z->expr);
    return IS_FAIL_fn(v) ? FAILDESCR : v;
}
typedef struct { IR_block_t *cfg; int first; } icn_dcg_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_every_body_pre(void) { if (frame_depth > 0) { FRAME.loop_next = 0; } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  icn_every_body_broke(void) { if (frame_depth <= 0) return 0; int b = FRAME.loop_break; FRAME.loop_next = 0; FRAME.loop_break = 0; return b; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_dcg(void *zeta, int entry) {
    icn_dcg_state_t *z = (icn_dcg_state_t *)zeta;
    if (!z || !z->cfg) return FAILDESCR;
    if (entry == α) { z->first = 1; }
    DESCR_t v = z->first ? (z->first=0, IR_exec_once(z->cfg)) : IR_exec_resume(z->cfg);
    return v;
}
#define ICN_FNC_GEN_ARGS 8
typedef struct {
    bb_node_t   arg_box;
    tree_t     *call;
    int         gen_idx;
    int         nargs;
    DESCR_t     args[ICN_FNC_GEN_ARGS];
} icn_fnc_gen_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_call_builtin(tree_t *call, DESCR_t *args, int nargs);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_fnc(void *zeta, int entry) {
    icn_fnc_gen_state_t *z = (icn_fnc_gen_state_t *)zeta;
    const char *fn = (z->call && z->call->n >= 1 && z->call->c[0]) ? z->call->c[0]->v.sval : NULL;
    int is_scan_bltn = is_scan_builtin_name(fn);
    int tick = entry;
    for (;;) {
        DESCR_t v = z->arg_box.fn(z->arg_box.ζ, tick);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        z->args[z->gen_idx] = v;
        DESCR_t r = icn_call_builtin(z->call, z->args, z->nargs);
        if (!IS_FAIL_fn(r)) return r;
        if (!is_scan_bltn) return FAILDESCR;
        tick = β;
    }
}
typedef struct {
    tree_t    *call;
    int        nargs;
    tree_t    *arg_trees[ICN_FNC_GEN_ARGS];
    bb_node_t  gen_boxes[ICN_FNC_GEN_ARGS];
    int        is_gen[ICN_FNC_GEN_ARGS];
    DESCR_t    cur_vals[ICN_FNC_GEN_ARGS];
    int        ngen;
    int        gen_idxs[ICN_FNC_GEN_ARGS];
    int        started;
} icn_fnc_multi_gen_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_fnc_multi(void *zeta, int entry) {
    icn_fnc_multi_gen_state_t *z = (icn_fnc_multi_gen_state_t *)zeta;
    if (!z->started) {
        for (int d = 0; d < z->ngen; d++) {
            int gi = z->gen_idxs[d];
            DESCR_t v = z->gen_boxes[d].fn(z->gen_boxes[d].ζ, 0);
            if (IS_FAIL_fn(v)) return FAILDESCR;
            z->cur_vals[gi] = v;
        }
        z->started = 1;
    } else {
        int advanced = 0;
        for (int d = z->ngen - 1; d >= 0; d--) {
            int gi = z->gen_idxs[d];
            DESCR_t v = z->gen_boxes[d].fn(z->gen_boxes[d].ζ, 1);
            if (!IS_FAIL_fn(v)) {
                z->cur_vals[gi] = v;
                for (int d2 = d + 1; d2 < z->ngen; d2++) {
                    int gi2 = z->gen_idxs[d2];
                    z->gen_boxes[d2] = icn_bb_build(z->arg_trees[gi2]);
                    DESCR_t v2 = z->gen_boxes[d2].fn(z->gen_boxes[d2].ζ, 0);
                    z->cur_vals[gi2] = IS_FAIL_fn(v2) ? NULVCL : v2;
                }
                advanced = 1; break;
            }
        }
        if (!advanced) return FAILDESCR;
    }
    for (;;) {
        const char *fn = z->call->c[0] ? z->call->c[0]->v.sval : NULL;
        if (fn) {
            for (int _i = 0; _i < proc_count; _i++) {
                if (proc_table[_i].name && strcmp(proc_table[_i].name, fn) == 0) {
                    DESCR_t r = proc_table_call(_i, z->cur_vals, z->nargs);
                    if (!IS_FAIL_fn(r)) return r;
                    break;
                }
            }
        }
        int advanced = 0;
        for (int d = z->ngen - 1; d >= 0; d--) {
            int gi = z->gen_idxs[d];
            DESCR_t v = z->gen_boxes[d].fn(z->gen_boxes[d].ζ, 1);
            if (!IS_FAIL_fn(v)) {
                z->cur_vals[gi] = v;
                for (int d2 = d + 1; d2 < z->ngen; d2++) {
                    int gi2 = z->gen_idxs[d2];
                    z->gen_boxes[d2] = icn_bb_build(z->arg_trees[gi2]);
                    DESCR_t v2 = z->gen_boxes[d2].fn(z->gen_boxes[d2].ζ, 0);
                    z->cur_vals[gi2] = IS_FAIL_fn(v2) ? NULVCL : v2;
                }
                advanced = 1; break;
            }
        }
        if (!advanced) return FAILDESCR;
    }
}
typedef struct {
    tree_t    *call;
    int        nargs;
    tree_t    *arg_trees[ICN_FNC_GEN_ARGS];
    bb_node_t  gen_boxes[ICN_FNC_GEN_ARGS];
    int        is_gen[ICN_FNC_GEN_ARGS];
} icn_fnc_multi_frag_t;
#define ICN_RAKU_ARRAY_MAX 1024
typedef struct {
    char       *elems[ICN_RAKU_ARRAY_MAX];
    int         nelem;
    int         elem_idx;
    const char *loopvar;
} icn_raku_array_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_raku_array(void *zeta, int entry) {
    icn_raku_array_state_t *z = (icn_raku_array_state_t *)zeta;
    if (entry == α) z->elem_idx = 0;
    else            z->elem_idx++;
    if (z->elem_idx >= z->nelem) return FAILDESCR;
    const char *p = z->elems[z->elem_idx];
    char *end;
    long iv = strtol(p, &end, 10);
    DESCR_t val = (end != p && *end == '\0') ? INTVAL(iv) : STRVAL(p);
    if (z->loopvar && *z->loopvar) {
        int slot = scope_get(&FRAME.sc, z->loopvar);
        if (slot >= 0 && slot < FRAME.env_n)
            FRAME.env[slot] = val;
        else
            NV_SET_fn(z->loopvar, val);
    }
    return val;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *find_leaf_suspendable(tree_t *e) {
    if (!e) return NULL;
    switch (e->t) {
        case TT_TO: case TT_TO_BY: case TT_ITERATE: case TT_ALTERNATE:
        case TT_SUSPEND: case TT_LIMIT: case TT_EVERY: case TT_BANG_BINARY: case TT_SEQ_EXPR:
            return e;
        case TT_FNC: return e;
        default: break;
    }
    for (int i = 0; i < e->n; i++) {
        tree_t *found = find_leaf_suspendable(e->c[i]);
        if (found) return found;
    }
    return NULL;
}
typedef struct { bb_node_t rhs_gen; tree_t *lhs; } icn_assign_gen_state_t;
typedef struct { tree_t *str_var; tree_t *rhs_expr; int pos; int len; char *buf; DESCR_t rec; int is_rec; int is_tbl; int is_list; TBBLK_t *tbl; int tbl_bucket; TBPAIR_t *tbl_entry; DESCR_t *list_elems; } icn_assign_lhs_iter_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_assign_write(tree_t *lhs, DESCR_t val);
static DESCR_t icn_bb_assign_lhs_iter(void *zeta, int entry) {
    icn_assign_lhs_iter_state_t *z = (icn_assign_lhs_iter_state_t *)zeta;
    if (entry == α) {
        DESCR_t cv = bb_eval_value(z->str_var);
        z->is_tbl = 0; z->is_list = 0; z->is_rec = 0;
        if (cv.v == DT_T && cv.tbl) {
            z->is_tbl = 1; z->tbl = cv.tbl; z->tbl_bucket = 0; z->tbl_entry = NULL;
            while (z->tbl_bucket < TABLE_BUCKETS && !z->tbl->buckets[z->tbl_bucket]) z->tbl_bucket++;
            z->tbl_entry = (z->tbl_bucket < TABLE_BUCKETS) ? z->tbl->buckets[z->tbl_bucket] : NULL;
        } else if (cv.v == DT_DATA && cv.u) {
            DESCR_t tag = FIELD_GET_fn(cv, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                DESCR_t ea = FIELD_GET_fn(cv, "frame_elems");
                z->len = (int)FIELD_GET_fn(cv, "frame_size").i;
                z->list_elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                z->is_list = (z->list_elems && z->len > 0) ? 1 : 0;
                z->pos = 0;
                if (!z->is_list) return FAILDESCR;
            } else if (cv.u->type && cv.u->type->nfields > 0 && cv.u->fields) {
                z->is_rec = 1; z->rec = cv; z->pos = 0; z->len = cv.u->type->nfields;
            } else return FAILDESCR;
        } else {
            const char *s = VARVAL_fn(cv);
            if (!s) return FAILDESCR;
            z->len = (int)strlen(s);
            if (z->len <= 0) return FAILDESCR;
            z->buf = (char *)GC_malloc((size_t)(z->len + 1));
            memcpy(z->buf, s, (size_t)(z->len + 1));
            z->pos = 0;
        }
    }
    if (z->is_tbl) {
        if (!z->tbl_entry) return FAILDESCR;
        DESCR_t val = bb_eval_value(z->rhs_expr);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        z->tbl_entry->val = val;
        z->tbl_entry = z->tbl_entry->next;
        while (!z->tbl_entry && z->tbl_bucket < TABLE_BUCKETS - 1) {
            z->tbl_bucket++;
            z->tbl_entry = z->tbl->buckets[z->tbl_bucket];
        }
        return val;
    }
    if (z->is_list) {
        if (z->pos >= z->len) return FAILDESCR;
        DESCR_t val = bb_eval_value(z->rhs_expr);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        z->list_elems[z->pos++] = val;
        return val;
    }
    if (z->pos >= z->len) return FAILDESCR;
    DESCR_t val = bb_eval_value(z->rhs_expr);
    if (IS_FAIL_fn(val)) return FAILDESCR;
    if (z->is_rec) {
        z->rec.u->fields[z->pos++] = val;
    } else {
        const char *ch = VARVAL_fn(val);
        z->buf[z->pos++] = (ch && *ch) ? ch[0] : '\0';
        icn_assign_write(z->str_var, STRVAL(z->buf));
    }
    return val;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_assign_write(tree_t *lhs, DESCR_t val) {
    if (lhs && lhs->t == TT_VAR) {
        int slot = lhs->_id;
        if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; }
        else if (slot < 0 && lhs->v.sval && lhs->v.sval[0] != '&') NV_SET_fn(lhs->v.sval, val);
    } else if (lhs && lhs->t == TT_FIELD && lhs->v.sval && lhs->n >= 1) {
        DESCR_t obj = bb_eval_value(lhs->c[0]);
        if (!IS_FAIL_fn(obj)) FIELD_SET_fn(obj, lhs->v.sval, val);
    }
    return val;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_assign_gen(void *zeta, int entry) {
    icn_assign_gen_state_t *z = (icn_assign_gen_state_t *)zeta;
    DESCR_t val = z->rhs_gen.fn(z->rhs_gen.ζ, entry);
    if (IS_FAIL_fn(val)) return FAILDESCR;
    return icn_assign_write(z->lhs, val);
}
typedef struct { bb_node_t leaf_gen; tree_t *rhs_expr; tree_t *leaf; tree_t *lhs; } icn_assign_cat_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_assign_cat(void *zeta, int entry) {
    icn_assign_cat_state_t *z = (icn_assign_cat_state_t *)zeta;
    int e2 = entry;
    for (;;) {
        DESCR_t tick = z->leaf_gen.fn(z->leaf_gen.ζ, e2);
        if (IS_FAIL_fn(tick)) return FAILDESCR;
        icn_drive_node = z->leaf;
        icn_drive_val  = tick;
        DESCR_t val = bb_eval_value(z->rhs_expr);
        icn_drive_node = NULL;
        if (!IS_FAIL_fn(val)) return icn_assign_write(z->lhs, val);
        e2 = β;
    }
}
typedef struct {
    tree_t  *lhs_expr;
    tree_t  *rhs_expr;
    DESCR_t *cell;
    DESCR_t  base_d;
    DESCR_t  idx_d;
    int      have_subscript;
    int      var_slot;
    char    *var_name;
    DESCR_t  saved;
    int      have_saved;
    bb_node_t rhs_gen;
    int       use_rhs_gen;
} icn_revassign_state_t;
typedef struct {
    bb_node_t  gen_idx;
    tree_t    *lhs_base_expr;
    tree_t    *rhs_expr;
    DESCR_t   *cell;
    DESCR_t    base_d;
    DESCR_t    idx_d;
    int        have_subscript;
    DESCR_t    saved;
    int        have_saved;
} icn_revassign_lhs_gen_state_t;
typedef struct {
    bb_node_t  gen_idx;
    tree_t    *lhs_base_expr;
    tree_t    *rhs_expr;
    DESCR_t   *cell;
    DESCR_t    base_d;
    DESCR_t    idx_d;
    int        have_subscript;
    DESCR_t    saved;
    int        have_saved;
} icn_assign_lhs_gen_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_assign_lhs_gen(void *zeta, int entry) {
    icn_assign_lhs_gen_state_t *z = (icn_assign_lhs_gen_state_t *)zeta;
    if (entry != α && z->have_saved) {
        if (z->cell) *z->cell = z->saved;
        else if (z->have_subscript) subscript_set(z->base_d, z->idx_d, z->saved);
        z->have_saved = 0; z->cell = NULL; z->have_subscript = 0;
    }
    DESCR_t idx = (entry == α) ? z->gen_idx.fn(z->gen_idx.ζ, α) : z->gen_idx.fn(z->gen_idx.ζ, β);
    if (IS_FAIL_fn(idx)) return FAILDESCR;
    DESCR_t base = bb_eval_value(z->lhs_base_expr);
    if (IS_FAIL_fn(base)) return FAILDESCR;
    DESCR_t rv = bb_eval_value(z->rhs_expr);
    if (IS_FAIL_fn(rv)) return FAILDESCR;
    z->base_d = base; z->idx_d = idx; z->saved = subscript_get(base, idx); z->have_saved = 1;
    if (base.v == DT_A) {
        DESCR_t *cell = array_ptr(base.arr, (int)to_int(idx));
        if (cell) { z->cell = cell; *cell = rv; } else { z->have_subscript = 1; subscript_set(base, idx, rv); }
    } else if (base.v == DT_T) {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    } else if (base.v == DT_DATA) {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    } else {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    }
    return rv;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_revassign_lhs_gen(void *zeta, int entry) {
    icn_revassign_lhs_gen_state_t *z = (icn_revassign_lhs_gen_state_t *)zeta;
    if (entry != α && z->have_saved) {
        if (z->cell) *z->cell = z->saved;
        else if (z->have_subscript) subscript_set(z->base_d, z->idx_d, z->saved);
        z->have_saved = 0; z->cell = NULL; z->have_subscript = 0;
    }
    DESCR_t idx = (entry == α)
        ? z->gen_idx.fn(z->gen_idx.ζ, α)
        : z->gen_idx.fn(z->gen_idx.ζ, β);
    if (IS_FAIL_fn(idx)) return FAILDESCR;
    DESCR_t base = bb_eval_value(z->lhs_base_expr);
    if (IS_FAIL_fn(base)) return FAILDESCR;
    DESCR_t rv = bb_eval_value(z->rhs_expr);
    if (IS_FAIL_fn(rv)) return FAILDESCR;
    z->base_d = base; z->idx_d = idx;
    z->saved = subscript_get(base, idx);
    z->have_saved = 1;
    if (base.v == DT_A) {
        DESCR_t *cell = array_ptr(base.arr, (int)to_int(idx));
        if (cell) { z->cell = cell; *cell = rv; }
        else { z->have_subscript = 1; subscript_set(base, idx, rv); }
    } else if (base.v == DT_T) {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    } else if (base.v == DT_DATA) {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    } else {
        z->have_subscript = 1; subscript_set(base, idx, rv);
    }
    return rv;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_revassign(void *zeta, int entry) {
    icn_revassign_state_t *z = (icn_revassign_state_t *)zeta;
    if (entry == α) {
        DESCR_t rv;
        if (z->use_rhs_gen) {
            rv = z->rhs_gen.fn(z->rhs_gen.ζ, α);
        } else {
            rv = bb_eval_value(z->rhs_expr);
        }
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        tree_t *lhs = z->lhs_expr;
        if (lhs && lhs->t == TT_VAR) {
            int slot = lhs->_id;
            if (slot >= 0 && slot < FRAME.env_n) {
                z->saved      = FRAME.env[slot];
                z->var_slot   = slot;
                z->have_saved = 1;
                FRAME.env[slot] = rv;
            } else if (lhs->v.sval && lhs->v.sval[0] != '&') {
                z->saved      = NV_GET_fn(lhs->v.sval);
                z->var_slot   = -1;
                z->var_name   = lhs->v.sval;
                z->have_saved = 1;
                NV_SET_fn(lhs->v.sval, rv);
            }
        } else if (lhs && lhs->t == TT_IDX && lhs->n >= 2) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            DESCR_t idx  = bb_eval_value(lhs->c[1]);
            if (!IS_FAIL_fn(base) && !IS_FAIL_fn(idx)) {
                z->saved      = subscript_get(base, idx);
                z->have_saved = 1;
                if (base.v == DT_T) {
                    DESCR_t *cell = table_ptr(base.tbl, idx);
                    if (cell) { z->cell = cell; *cell = rv; }
                    else { z->base_d = base; z->idx_d = idx; z->have_subscript = 1; subscript_set(base, idx, rv); }
                } else if (base.v == DT_A) {
                    DESCR_t *cell = array_ptr(base.arr, (int)to_int(idx));
                    if (cell) { z->cell = cell; *cell = rv; }
                    else { z->base_d = base; z->idx_d = idx; z->have_subscript = 1; subscript_set(base, idx, rv); }
                } else {
                    z->base_d = base; z->idx_d = idx; z->have_subscript = 1;
                    subscript_set(base, idx, rv);
                }
            }
        } else if (lhs && lhs->t == TT_RANDOM && lhs->n >= 1) {
            DESCR_t base = bb_eval_value(lhs->c[0]);
            if (base.v == DT_DATA && base.u && base.u->type && base.u->type->nfields > 0 && base.u->fields) {
                extern unsigned long bb_icn_rnd_seed;
                bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                int fi = (int)((bb_icn_rnd_seed >> 33) % (unsigned long)base.u->type->nfields);
                z->saved = base.u->fields[fi]; z->have_saved = 1;
                z->cell = &base.u->fields[fi];
                base.u->fields[fi] = rv;
            }
        }
        return rv;
    }
    if (z->use_rhs_gen) {
        z->rhs_gen.fn(z->rhs_gen.ζ, β);
    }
    if (z->have_saved) {
        if (z->cell) {
            *z->cell = z->saved;
        } else if (z->have_subscript) {
            subscript_set(z->base_d, z->idx_d, z->saved);
        } else if (z->var_slot >= 0 && z->var_slot < FRAME.env_n) {
            FRAME.env[z->var_slot] = z->saved;
        } else if (z->var_name) {
            NV_SET_fn(z->var_name, z->saved);
        }
        z->have_saved = 0;
    }
    return FAILDESCR;
}
typedef struct {
    tree_t  *lhs_expr;
    tree_t  *rhs_expr;
    DESCR_t  saved_lhs;
    DESCR_t  saved_rhs;
    int      lhs_written;
    int      rhs_written;
} icn_revswap_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int icn_revswap_write(tree_t *lv_expr, DESCR_t val) {
    if (!lv_expr || lv_expr->t != TT_VAR) return 0;
    if (lv_expr->v.sval && lv_expr->v.sval[0] == '&') {
        return kw_assign(lv_expr->v.sval + 1, val);
    }
    int slot = lv_expr->_id;
    if (slot >= 0 && slot < FRAME.env_n) { FRAME.env[slot] = val; return 1; }
    if (slot < 0 && lv_expr->v.sval) { NV_SET_fn(lv_expr->v.sval, val); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_revswap_read(tree_t *lv_expr) {
    if (!lv_expr || lv_expr->t != TT_VAR) return FAILDESCR;
    if (lv_expr->v.sval && lv_expr->v.sval[0] == '&') {
        if (!strcmp(lv_expr->v.sval + 1, "pos")) return INTVAL(scan_pos);
        if (!strcmp(lv_expr->v.sval + 1, "subject")) return scan_subj ? STRVAL(scan_subj) : NULVCL;
        return NULVCL;
    }
    int slot = lv_expr->_id;
    if (slot >= 0 && slot < FRAME.env_n) return FRAME.env[slot];
    if (slot < 0 && lv_expr->v.sval) return NV_GET_fn(lv_expr->v.sval);
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_revswap(void *zeta, int entry) {
    icn_revswap_state_t *z = (icn_revswap_state_t *)zeta;
    if (entry == α) {
        tree_t *lhs = z->lhs_expr, *rhs = z->rhs_expr;
        DESCR_t lv = bb_eval_value(lhs);
        DESCR_t rv = bb_eval_value(rhs);
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
        z->saved_lhs = lv;
        z->saved_rhs = rv;
        if (!icn_revswap_write(lhs, rv)) return FAILDESCR;
        z->lhs_written = 1;
        if (!icn_revswap_write(rhs, lv)) return FAILDESCR;
        z->rhs_written = 1;
        return rv;
    }
    if (z->lhs_written) {
        if (icn_revswap_write(z->lhs_expr, z->saved_lhs)) {
            if (z->rhs_written) {
                icn_revswap_write(z->rhs_expr, z->saved_rhs);
            }
        }
        z->lhs_written = 0;
        z->rhs_written = 0;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_mutual(void *zeta, int entry) {
    icn_mutual_state_t *z = (icn_mutual_state_t *)zeta;
    if (!z || !z->ast_b) return FAILDESCR;
    for (;;) {
        if (!z->a_started) {
            DESCR_t va = z->gen_a.fn(z->gen_a.ζ, α);
            if (IS_FAIL_fn(va)) return FAILDESCR;
            z->a_started = 1;
            z->gen_b = icn_bb_build(z->ast_b);
            z->b_started = 0;
        }
        int b_tick = z->b_started ? β : α;
        z->b_started = 1;
        DESCR_t vb = z->gen_b.fn(z->gen_b.ζ, b_tick);
        if (!IS_FAIL_fn(vb)) return vb;
        DESCR_t va2 = z->gen_a.fn(z->gen_a.ζ, β);
        if (IS_FAIL_fn(va2)) return FAILDESCR;
        z->gen_b = icn_bb_build(z->ast_b);
        z->b_started = 0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_cat(void *zeta, int entry) {
    icn_cat_gen_state_t *z = (icn_cat_gen_state_t *)zeta;
    int e2 = entry;
    for (;;) {
        DESCR_t tick = z->gen.fn(z->gen.ζ, e2);
        if (IS_FAIL_fn(tick)) return FAILDESCR;
        icn_drive_node = z->leaf;
        icn_drive_val  = tick;
        DESCR_t result = bb_eval_value(z->cat_expr);
        icn_drive_node = NULL;
        if (!IS_FAIL_fn(result)) return result;
        e2 = β;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_scan_gen(void *zeta, int entry) {
    icn_scan_gen_state_t *z = (icn_scan_gen_state_t *)zeta;
    if (!z->body) return FAILDESCR;
    if (!z->started || entry == α) {
        DESCR_t sv = z->subj_gen.fn(z->subj_gen.ζ, (z->started ? β : α));
        if (IS_FAIL_fn(sv)) return FAILDESCR;
        z->started = 1;
        if (sv.v == DT_I || sv.v == DT_R) sv = descr_to_str_icn(sv);
        const char *s = (sv.v == DT_S && sv.s) ? sv.s : (sv.v == DT_SNUL ? "" : "");
        z->body_subj = s;
        const char *saved_subj = scan_subj;
        int saved_pos = scan_pos;
        scan_subj = s;
        scan_pos  = 1;
        z->body_gen  = icn_bb_build(z->body);
        z->body_live = 1;
        z->body_pos  = scan_pos;
        DESCR_t val = z->body_gen.fn(z->body_gen.ζ, α);
        z->body_pos = scan_pos;
        scan_subj = saved_subj;
        scan_pos  = saved_pos;
        if (!IS_FAIL_fn(val)) return val;
        z->body_live = 0;
        return FAILDESCR;
    }
    if (z->body_live) {
        const char *saved_subj = scan_subj;
        int saved_pos = scan_pos;
        scan_subj = z->body_subj;
        scan_pos  = z->body_pos;
        DESCR_t val = z->body_gen.fn(z->body_gen.ζ, β);
        z->body_pos = scan_pos;
        scan_subj = saved_subj;
        scan_pos  = saved_pos;
        if (!IS_FAIL_fn(val)) return val;
        z->body_live = 0;
    }
    DESCR_t sv = z->subj_gen.fn(z->subj_gen.ζ, β);
    if (IS_FAIL_fn(sv)) return FAILDESCR;
    if (sv.v == DT_I || sv.v == DT_R) sv = descr_to_str_icn(sv);
    const char *s = (sv.v == DT_S && sv.s) ? sv.s : "";
    z->body_subj = s;
    const char *saved_subj2 = scan_subj;
    int saved_pos2 = scan_pos;
    scan_subj = s;
    scan_pos  = 1;
    z->body_gen  = icn_bb_build(z->body);
    z->body_live = 1;
    z->body_pos  = scan_pos;
    DESCR_t val2 = z->body_gen.fn(z->body_gen.ζ, α);
    z->body_pos = scan_pos;
    scan_subj = saved_subj2;
    scan_pos  = saved_pos2;
    if (!IS_FAIL_fn(val2)) return val2;
    z->body_live = 0;
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_to_by_real(void *zeta, int entry) {
    icn_to_by_real_state_t *z = (icn_to_by_real_state_t *)zeta;
    if (entry == α) { z->cur = z->lo; }
    else            { z->cur += z->step; }
    int exhausted = (z->step >= 0) ? (z->cur > z->hi + 1e-12)
                                   : (z->cur < z->hi - 1e-12);
    if (exhausted) return FAILDESCR;
    return REALVAL(z->cur);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_find(void *zeta, int entry) {
    icn_find_state_t *z = (icn_find_state_t *)zeta;
    if (entry == α) z->next = z->hay;
    const char *hit = strstr(z->next, z->needle);
    if (!hit) return FAILDESCR;
    long pos1 = (long)(hit - z->hay) + 1;
    z->next = hit + (z->nlen > 0 ? z->nlen : 1);
    return INTVAL(pos1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_bal(void *zeta, int entry) {
    icn_bal_state_t *z = (icn_bal_state_t *)zeta;
    if (entry == α) { }
    int p = z->pos, depth = 0;
    while (p < z->endp && p < z->slen) {
        char ch = z->s[p];
        if (strchr(z->c2, ch)) depth++;
        else if (strchr(z->c3, ch) && depth > 0) depth--;
        else if (depth == 0 && strchr(z->c1, ch)) {
            z->pos = p + 1;
            return INTVAL((long)(p + 1));
        }
        p++;
    }
    return FAILDESCR;
}
typedef struct { DESCR_t base; bb_node_t idx_gen; } icn_idx_gen_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_idx_gen(void *zeta, int entry) {
    icn_idx_gen_state_t *z = (icn_idx_gen_state_t *)zeta;
    int e2 = entry;
    for (;;) {
        DESCR_t tick = z->idx_gen.fn(z->idx_gen.ζ, e2);
        if (IS_FAIL_fn(tick)) return FAILDESCR;
        DESCR_t result = subscript_get(z->base, tick);
        if (!IS_FAIL_fn(result)) return result;
        e2 = β;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_list_iterate(void *zeta, int entry) {
    icn_list_iterate_state_t *z = (icn_list_iterate_state_t *)zeta;
    DESCR_t ea = FIELD_GET_fn(z->list_obj, "frame_elems");
    int n = (int)FIELD_GET_fn(z->list_obj, "frame_size").i;
    DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
    if (!elems || n <= 0) return FAILDESCR;
    if (entry == α) z->pos = 0;
    else            z->pos++;
    if (z->pos >= n) return FAILDESCR;
    return elems[z->pos];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_tbl_iterate(void *zeta, int entry) {
    icn_tbl_iterate_state_t *z = (icn_tbl_iterate_state_t *)zeta;
    if (!z->tbl) return FAILDESCR;
    if (entry == α) { z->bucket = 0; z->entry = z->tbl->buckets[0]; }
    else if (z->entry) { z->entry = z->entry->next; }
    while (!z->entry && z->bucket < TABLE_BUCKETS - 1) {
        z->bucket++;
        z->entry = z->tbl->buckets[z->bucket];
    }
    if (!z->entry) return FAILDESCR;
    return z->entry->val;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_tbl_key_iterate(void *zeta, int entry) {
    icn_tbl_key_iterate_state_t *z = (icn_tbl_key_iterate_state_t *)zeta;
    if (!z->tbl) return FAILDESCR;
    if (entry == α) { z->bucket = 0; z->entry = z->tbl->buckets[0]; }
    else if (z->entry) { z->entry = z->entry->next; }
    while (!z->entry && z->bucket < TABLE_BUCKETS - 1) {
        z->bucket++;
        z->entry = z->tbl->buckets[z->bucket];
    }
    if (!z->entry) return FAILDESCR;
    return z->entry->key_descr;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_record_iterate(void *zeta, int entry) {
    icn_record_iterate_state_t *z = (icn_record_iterate_state_t *)zeta;
    if (z->inst.v != DT_DATA || !z->inst.u || !z->inst.u->type) return FAILDESCR;
    int n = z->inst.u->type->nfields;
    if (n <= 0 || !z->inst.u->fields) return FAILDESCR;
    if (entry == α) z->pos = 0;
    else            z->pos++;
    if (z->pos >= n) return FAILDESCR;
    return z->inst.u->fields[z->pos];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_bang_binary(void *zeta, int entry) {
    icn_bang_binary_state_t *z = (icn_bang_binary_state_t *)zeta;
    if (!z || !z->proc_expr) return FAILDESCR;
    for (;;) {
        DESCR_t arg = z->arg_box.fn(z->arg_box.ζ, entry);
        if (IS_FAIL_fn(arg)) return FAILDESCR;
        entry = β;
        const char *fname = z->proc_expr->v.sval;
        if (fname) {
            for (int pi = 0; pi < proc_count; pi++) {
                if (strcmp(proc_table[pi].name, fname) != 0) continue;
                DESCR_t args1[1]; args1[0] = arg;
                DESCR_t result = proc_table_call(pi, args1, 1);
                if (!IS_FAIL_fn(result)) return result;
                goto next_arg;
            }
        }
        {
            DESCR_t fv = bb_eval_value(z->proc_expr);
            if (!IS_FAIL_fn(fv) && fv.v == DT_E) {
                for (int pi = 0; pi < proc_count; pi++) {
                    if (proc_table[pi].entry_pc != (int)fv.i) continue;
                    DESCR_t args1[1]; args1[0] = arg;
                    DESCR_t result = proc_table_call(pi, args1, 1);
                    if (!IS_FAIL_fn(result)) return result;
                    goto next_arg;
                }
            }
        }
        next_arg:;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_bb_seq_expr(void *zeta, int entry) {
    icn_seq_state_t *z = (icn_seq_state_t *)zeta;
    if (!z || z->n < 1) return FAILDESCR;
    if (!z->started || entry == α) {
        for (int i = 0; i < z->n - 1; i++) bb_exec_stmt(z->children[i]);
        z->last_box = icn_bb_build(z->children[z->n - 1]);
        z->started  = 1;
        return z->last_box.fn(z->last_box.ζ, α);
    }
    return z->last_box.fn(z->last_box.ζ, β);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_nonnull_filter(void *zeta, int entry) {
    icn_limit_state_t *z = (icn_limit_state_t *)zeta;
    int e2 = entry;
    for (;;) {
        DESCR_t v = z->gen.fn(z->gen.ζ, e2);
        if (IS_FAIL_fn(v)) return FAILDESCR;
        if (v.v == DT_SNUL) { e2 = β; continue; }
        return v;
    }
}
typedef struct { bb_node_t r_gen; tree_t *lhs_expr; } icn_identical_gen_state_t;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t icn_bb_identical_gen(void *zeta, int entry) {
    icn_identical_gen_state_t *z = (icn_identical_gen_state_t *)zeta;
    DESCR_t lv = bb_eval_value(z->lhs_expr);
    if (IS_FAIL_fn(lv)) return FAILDESCR;
    int e2 = entry;
    while (1) {
        DESCR_t rv = z->r_gen.fn(z->r_gen.ζ, e2);
        if (IS_FAIL_fn(rv)) return FAILDESCR;
        if (icn_descr_identical(lv, rv)) return rv;
        e2 = β;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t icn_bb_pump_proc_by_name(const char *name, DESCR_t *args, int nargs) {
    if (!name) return (bb_node_t){ NULL, NULL, 0 };
    for (int i = 0; i < proc_count; i++) {
        if (strcmp(proc_table[i].name, name) != 0) continue;
        icn_bb_oneshot_state_t *oshot1 = calloc(1, sizeof(*oshot1));
        oshot1->val = proc_table_call(i, args, nargs);
        return (bb_node_t){ icn_bb_oneshot, oshot1, 0 };
    }
    return (bb_node_t){ NULL, NULL, 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t icn_bb_build(tree_t *e) {
    NO_AST_WALK_GUARD("icn_bb_build");
    if (!e) {
        icn_bb_oneshot_state_t *z = calloc(1, sizeof(*z));
        z->val = FAILDESCR; z->fired = 1;
        return (bb_node_t){ icn_bb_oneshot, z, 0 };
    }
    if (e->t == TT_TO && e->n >= 2) {
        tree_t *lo_expr = e->c[0];
        tree_t *hi_expr = e->c[1];
        int lo_gen = is_suspendable(lo_expr);
        int hi_gen = is_suspendable(hi_expr);
        if (lo_gen || hi_gen) {
            icn_to_nested_state_t *z = calloc(1, sizeof(*z));
            if (!lo_gen) {
                DESCR_t d = bb_eval_value(lo_expr);
                if (!IS_FAIL_fn(d)) z->lo_vals[z->nlo++] = d.i;
            } else {
                bb_node_t lb = icn_bb_build(lo_expr);
                DESCR_t v = lb.fn(lb.ζ, α);
                while (!IS_FAIL_fn(v) && z->nlo < ICN_TO_NESTED_MAX) { z->lo_vals[z->nlo++] = v.i; v = lb.fn(lb.ζ, β); }
            }
            if (!hi_gen) {
                DESCR_t d = bb_eval_value(hi_expr);
                if (!IS_FAIL_fn(d)) z->hi_vals[z->nhi++] = d.i;
            } else {
                bb_node_t hb = icn_bb_build(hi_expr);
                DESCR_t v = hb.fn(hb.ζ, α);
                while (!IS_FAIL_fn(v) && z->nhi < ICN_TO_NESTED_MAX) { z->hi_vals[z->nhi++] = v.i; v = hb.fn(hb.ζ, β); }
            }
            IR_block_t *ncfg = lower_icn_to_nested(z);
            icn_dcg_state_t *ndz = calloc(1, sizeof(*ndz));
            ndz->cfg = ncfg; ndz->first = 1;
            return (bb_node_t){ icn_bb_dcg, ndz, 0 };
        }
        DESCR_t lo_d = bb_eval_value(lo_expr);
        DESCR_t hi_d = bb_eval_value(hi_expr);
        int64_t lo = IS_FAIL_fn(lo_d) ? 0 : lo_d.i;
        int64_t hi = IS_FAIL_fn(hi_d) ? 0 : hi_d.i;
        IR_block_t *cfg = lower_icn_to(lo, hi);
        icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
        dz->cfg = cfg; dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, dz, 0 };
    }
    if (e->t == TT_TO_BY && e->n >= 3) {
        DESCR_t lo_d   = bb_eval_value(e->c[0]);
        DESCR_t hi_d   = bb_eval_value(e->c[1]);
        DESCR_t step_d = bb_eval_value(e->c[2]);
        int was_str_lo = (!IS_REAL_fn(lo_d) && !IS_INT_fn(lo_d) && !IS_FAIL_fn(lo_d));
        int was_str_hi = (!IS_REAL_fn(hi_d) && !IS_INT_fn(hi_d) && !IS_FAIL_fn(hi_d));
        int was_str_st = (!IS_REAL_fn(step_d) && !IS_INT_fn(step_d) && !IS_FAIL_fn(step_d));
        int any_str = was_str_lo || was_str_hi || was_str_st;
#define _TO_COERCE(d) do { \
        if (!IS_REAL_fn(d) && !IS_INT_fn(d) && !IS_FAIL_fn(d)) { \
            const char *_s = (d).s ? (d).s : ""; \
            char *_e = NULL; double _rv = strtod(_s, &_e); \
            if (_e && !*_e) { (d) = REALVAL(_rv); } else { (d) = FAILDESCR; } \
        } } while(0)
        _TO_COERCE(lo_d); _TO_COERCE(hi_d); _TO_COERCE(step_d);
#undef _TO_COERCE
        if (!any_str && IS_REAL_fn(lo_d) && IS_REAL_fn(hi_d) && IS_REAL_fn(step_d)) {
            icn_to_by_real_state_t *rz = calloc(1, sizeof(*rz));
            rz->lo   = lo_d.r;
            rz->hi   = hi_d.r;
            rz->step = step_d.r ? step_d.r : 1.0;
            rz->cur  = rz->lo;
            return (bb_node_t){ icn_bb_to_by_real, rz, 0 };
        }
#define _TO_INT(d, def) (IS_REAL_fn(d) ? (long)(d).r : (IS_FAIL_fn(d) ? (def) : (d).i))
        int64_t to_by_lo   = _TO_INT(lo_d,   0);
        int64_t to_by_hi   = _TO_INT(hi_d,   0);
        int64_t to_by_step = _TO_INT(step_d, 1); if (!to_by_step) to_by_step = 1;
#undef _TO_INT
        IR_block_t *to_by_cfg = lower_icn_to_by(to_by_lo, to_by_hi, to_by_step);
        icn_dcg_state_t *to_by_dz = calloc(1, sizeof(*to_by_dz));
        to_by_dz->cfg = to_by_cfg; to_by_dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, to_by_dz, 0 };
    }
    if (e->t == TT_ITERATE && e->n >= 1) {
        tree_t *child = e->c[0];
        if (child && child->t == TT_FNC && child->n >= 1 && child->c[0]) {
            const char *fn = child->c[0]->v.sval;
            if (fn) {
                int pi;
                for (pi = 0; pi < proc_count; pi++)
                    if (strcmp(proc_table[pi].name, fn) == 0) break;
                if (pi < proc_count) {
                    icn_bb_oneshot_state_t *oshot2 = calloc(1, sizeof(*oshot2));
                    oshot2->val = proc_table_call(pi, NULL, 0);
                    return (bb_node_t){ icn_bb_oneshot, oshot2, 0 };
                }
            }
        }
        DESCR_t sv = bb_eval_value(e->c[0]);
        const char *loopvar = e->v.sval;
        if (sv.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(sv, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                icn_list_iterate_state_t *lz = calloc(1, sizeof(*lz));
                lz->list_obj = sv;
                lz->pos      = 0;
                return (bb_node_t){ icn_bb_list_iterate, lz, 0 };
            }
            if (sv.u && sv.u->type && sv.u->type->nfields > 0) {
                icn_record_iterate_state_t *rz = calloc(1, sizeof(*rz));
                rz->inst = sv;
                rz->pos  = 0;
                return (bb_node_t){ icn_bb_record_iterate, rz, 0 };
            }
        }
        if (sv.v == DT_T) {
            icn_tbl_iterate_state_t *z = calloc(1, sizeof(*z));
            z->tbl    = sv.tbl;
            z->bucket = 0;
            z->entry  = NULL;
            return (bb_node_t){ icn_bb_tbl_iterate, z, 0 };
        }
        sv = descr_to_str_icn(sv);
        if (!IS_FAIL_fn(sv) && sv.s && (loopvar || strchr(sv.s, '\x01'))) {
            icn_raku_array_state_t *z = calloc(1, sizeof(*z));
            z->loopvar = loopvar;
            char *copy = GC_malloc(strlen(sv.s) + 1);
            strcpy(copy, sv.s);
            char *p = copy;
            while (z->nelem < ICN_RAKU_ARRAY_MAX) {
                z->elems[z->nelem++] = p;
                char *sep = strchr(p, '\x01');
                if (!sep) break;
                *sep = '\0';
                p = sep + 1;
            }
            return (bb_node_t){ icn_bb_raku_array, z, 0 };
        }
        if (!IS_FAIL_fn(sv) && sv.s) {
            const char *istr = sv.s;
            int64_t ilen = IS_CSET_fn(sv) ? (int64_t)strlen(sv.s)
                         : (sv.slen > 0 && sv.slen != 0xFFFFFFFFu) ? (int64_t)sv.slen
                         : (int64_t)strlen(sv.s);
            IR_block_t *icfg = lower_icn_iterate(istr, ilen);
            icn_dcg_state_t *idz = calloc(1, sizeof(*idz));
            idz->cfg = icfg; idz->first = 1;
            return (bb_node_t){ icn_bb_dcg, idz, 0 };
        }
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }
    if (e->t == TT_IDENTICAL && e->n >= 2) {
        tree_t *lc = e->c[0], *rc = e->c[1];
        int l_gen = is_suspendable(lc);
        int r_gen = is_suspendable(rc);
        if (l_gen || r_gen) {
            icn_identical_gen_state_t *z = calloc(1, sizeof(*z));
            if (r_gen) {
                z->lhs_expr = lc;
                z->r_gen    = icn_bb_build(rc);
            } else {
                z->lhs_expr = rc;
                z->r_gen    = icn_bb_build(lc);
            }
            return (bb_node_t){ icn_bb_identical_gen, z, 0 };
        }
    }
    if (e->t == TT_ALTERNATE && e->n >= 2) {
        bb_node_t acc;
        {
            bb_node_t left  = icn_bb_build(e->c[0]);
            bb_node_t right = icn_bb_build(e->c[1]);
            IR_block_t *cfg = lower_icn_alternate(left, right);
            icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
            dz->cfg = cfg; dz->first = 1;
            acc = (bb_node_t){ icn_bb_dcg, dz, 0 };
        }
        for (int _ai = 2; _ai < e->n; _ai++) {
            bb_node_t right2 = icn_bb_build(e->c[_ai]);
            IR_block_t *cfg2 = lower_icn_alternate(acc, right2);
            icn_dcg_state_t *dz2 = calloc(1, sizeof(*dz2));
            dz2->cfg = cfg2; dz2->first = 1;
            acc = (bb_node_t){ icn_bb_dcg, dz2, 0 };
        }
        return acc;
    }
    {
        static const struct { tree_e ek; IcnBinopKind bk; int is_rel; } binop_map[] = {
            { TT_ADD, ICN_BINOP_ADD, 0 }, { TT_SUB, ICN_BINOP_SUB, 0 },
            { TT_MUL, ICN_BINOP_MUL, 0 }, { TT_DIV, ICN_BINOP_DIV, 0 },
            { TT_MOD, ICN_BINOP_MOD, 0 },
            { TT_LT,  ICN_BINOP_LT,  1 }, { TT_LE,  ICN_BINOP_LE,  1 },
            { TT_GT,  ICN_BINOP_GT,  1 }, { TT_GE,  ICN_BINOP_GE,  1 },
            { TT_EQ,  ICN_BINOP_EQ,  1 }, { TT_NE,  ICN_BINOP_NE,  1 },
            { TT_LCONCAT, ICN_BINOP_CONCAT, 0 },
        };
        for (int mi = 0; mi < (int)(sizeof binop_map/sizeof binop_map[0]); mi++) {
            if (e->t != binop_map[mi].ek) continue;
            if (e->n < 2) break;
            tree_t *lc = e->c[0], *rc = e->c[1];
            int l_gen = is_suspendable(lc);
            int r_gen = is_suspendable(rc);
            if (!l_gen && !r_gen) break;
            bb_node_t bo_left = icn_bb_build(lc);
            bb_node_t bo_right = icn_bb_build(rc);
            IR_block_t *bo_cfg = lower_icn_binop(bo_left, bo_right, binop_map[mi].bk, binop_map[mi].is_rel);
            icn_dcg_state_t *bo_dz = calloc(1, sizeof(*bo_dz));
            bo_dz->cfg = bo_cfg; bo_dz->first = 1;
            return (bb_node_t){ icn_bb_dcg, bo_dz, 0 };
        }
    }
    if (e->t == TT_CAT && e->n >= 2) {
        int l_gen = is_suspendable(e->c[0]);
        int r_gen = is_suspendable(e->c[1]);
        if (l_gen && r_gen) {
            bb_node_t cat_left = icn_bb_build(e->c[0]);
            bb_node_t cat_right = icn_bb_build(e->c[1]);
            IR_block_t *cat_cfg = lower_icn_binop(cat_left, cat_right, ICN_BINOP_CONCAT, 0);
            icn_dcg_state_t *cat_dz = calloc(1, sizeof(*cat_dz));
            cat_dz->cfg = cat_cfg; cat_dz->first = 1;
            return (bb_node_t){ icn_bb_dcg, cat_dz, 0 };
        }
        if (l_gen || r_gen) {
            int gi = l_gen ? 0 : 1;
            tree_t *leaf = find_leaf_suspendable(e->c[gi]);
            if (!leaf) leaf = e->c[gi];
            icn_cat_gen_state_t *z = calloc(1, sizeof(*z));
            z->gen      = icn_bb_build(leaf);
            z->cat_expr = e;
            z->leaf     = leaf;
            return (bb_node_t){ icn_bb_cat, z, 0 };
        }
    }
    if (e->t == TT_IDX && e->n >= 2) {
        for (int _ci = 1; _ci < e->n; _ci++) {
            if (is_suspendable(e->c[_ci])) {
                DESCR_t base = bb_eval_value(e->c[0]);
                icn_idx_gen_state_t *z = calloc(1, sizeof(*z));
                z->base    = base;
                z->idx_gen = icn_bb_build(e->c[_ci]);
                return (bb_node_t){ icn_bb_idx_gen, z, 0 };
            }
        }
    }
    if (e->t == TT_FNC && e->n >= 3 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "find") == 0) {
        DESCR_t s1 = bb_eval_value(e->c[1]);
        if (!IS_FAIL_fn(s1)) {
            if (is_suspendable(e->c[2])) {
                icn_find_gen_subj_t *z = calloc(1, sizeof(*z));
                z->subj_gen   = icn_bb_build(e->c[2]);
                z->needle     = s1.s ? s1.s : "";
                z->nlen       = (int)strlen(z->needle);
                z->subj_entry = α;
                z->hay        = NULL;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
            DESCR_t s2 = bb_eval_value(e->c[2]);
            if (!IS_FAIL_fn(s2)) {
                icn_find_state_t *z = calloc(1, sizeof(*z));
                z->needle = s1.s ? s1.s : "";
                z->hay    = s2.s ? s2.s : "";
                z->nlen   = (int)strlen(z->needle);
                z->next   = z->hay;
                return (bb_node_t){ icn_bb_find, z, 0 };
            }
        }
    }
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "bal") == 0) {
        int nargs = e->n - 1;
        DESCR_t cd = bb_eval_value(e->c[1]);
        const char *c1 = VARVAL_fn(cd); if (!c1) goto bal_skip;
        const char *c2 = "(", *c3 = ")";
        if (nargs >= 2) { DESCR_t t = bb_eval_value(e->c[2]); const char *v = VARVAL_fn(t); if (v && v[0]) c2 = v; }
        if (nargs >= 3) { DESCR_t t = bb_eval_value(e->c[3]); const char *v = VARVAL_fn(t); if (v && v[0]) c3 = v; }
        const char *s; int slen, p, end;
        if (nargs >= 4) {
            DESCR_t sv = bb_eval_value(e->c[4]); s = VARVAL_fn(sv); if (!s) s = "";
            slen = (int)strlen(s);
            int i1 = (nargs >= 5) ? (int)bb_eval_value(e->c[5]).i : 1;
            int i2 = (nargs >= 6) ? (int)bb_eval_value(e->c[6]).i : slen + 1;
            if (i1 <= 0) i1 = 1; if (i2 <= 0) i2 = slen + 1;
            p = i1 - 1; end = i2 - 1;
        } else {
            s = scan_subj; if (!s) goto bal_skip;
            slen = (int)strlen(s); p = scan_pos - 1; end = slen;
        }
        {
            icn_bal_state_t *z = calloc(1, sizeof(*z));
            z->s = s; z->c1 = c1; z->c2 = c2; z->c3 = c3;
            z->slen = slen; z->pos = p; z->endp = end;
            return (bb_node_t){ icn_bb_bal, z, 0 };
        }
        bal_skip:;
    }
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "key") == 0) {
        DESCR_t td = bb_eval_value(e->c[1]);
        if (td.v == DT_T && td.tbl) {
            icn_tbl_key_iterate_state_t *z = calloc(1, sizeof(*z));
            z->tbl    = td.tbl;
            z->bucket = 0;
            z->entry  = NULL;
            return (bb_node_t){ icn_bb_tbl_key_iterate, z, 0 };
        }
    }
    if (e->t == TT_FNC && e->n >= 1 && e->c[0]) {
        if (e->c[0]->t != TT_VAR && is_suspendable(e->c[0])) {
            (void)0;
        }
    }
    if (e->t == TT_FNC && e->n >= 1 && e->c[0] && e->c[0]->v.sval) {
        const char *fn = e->c[0]->v.sval;
        int nargs = e->n - 1;
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, fn) != 0) continue;
            {
                int ngen_args = 0;
                int gen_idxs[ICN_FNC_GEN_ARGS];
                for (int j = 0; j < nargs && j < ICN_FNC_GEN_ARGS; j++)
                    if (e->c[1+j] && is_suspendable(e->c[1+j]))
                        gen_idxs[ngen_args++] = j;
                if (ngen_args >= 1) {
                    (void)ngen_args;
                }
            }
            DESCR_t *args = nargs > 0 ? calloc(nargs, sizeof(DESCR_t)) : NULL;
            int arg_failed = 0;
            for (int j = 0; j < nargs; j++) {
                args[j] = bb_eval_value(e->c[1+j]);
                if (IS_FAIL_fn(args[j])) { arg_failed = 1; break; }
            }
            if (arg_failed) {
                icn_bb_oneshot_state_t *oshot_fail = calloc(1, sizeof(*oshot_fail));
                oshot_fail->val = FAILDESCR;
                return (bb_node_t){ icn_bb_oneshot, oshot_fail, 0 };
            }
            if (proc_has_suspend(proc_table[i].proc) && proc_table[i].entry_pc >= 0 && g_current_sm_prog) {
                GeneratorState *pgs = generator_state_new_proc(i, args, nargs);
                if (pgs) {
                    IR_block_t *pcfg = lower_icn_proc_gen(pgs);
                    if (pcfg) {
                        icn_dcg_state_t *pdz = calloc(1, sizeof(*pdz));
                        pdz->cfg = pcfg; pdz->first = 1;
                        return (bb_node_t){ icn_bb_dcg, pdz, 0 };
                    }
                }
            }
            icn_bb_oneshot_state_t *oshot3 = calloc(1, sizeof(*oshot3));
            oshot3->val = proc_table_call(i, args, nargs);
            return (bb_node_t){ icn_bb_oneshot, oshot3, 0 };
        }
        if (fn && strcmp(fn, "upto") == 0 && nargs >= 2 && is_suspendable(e->c[2])) {
            DESCR_t cd = bb_eval_value(e->c[1]);
            const char *cset = VARVAL_fn(cd);
            if (cset) {
                icn_upto_gen_subj_t *z = calloc(1, sizeof(*z));
                z->subj_gen   = icn_bb_build(e->c[2]);
                z->cset       = cset;
                z->subj_entry = α;
                z->hay        = NULL;
                z->slen       = 0;
                z->pos        = 0;
                return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
            }
        }
        if (fn && strcmp(fn, "upto") == 0 && nargs >= 1) {
            DESCR_t cd = bb_eval_value(e->c[1]);
            const char *cset = VARVAL_fn(cd);
            const char *hay = NULL;
            if (nargs >= 2 && !is_suspendable(e->c[2])) {
                DESCR_t sd = bb_eval_value(e->c[2]);
                hay = sd.s ? sd.s : (sd.v == DT_SNUL ? "" : NULL);
            } else if (nargs == 1) {
                hay = scan_subj ? scan_subj : "";
            }
            if (cset && hay) {
                extern IR_block_t *lower_icn_upto(const char *cset, const char *hay);
                const char *hay_from = (nargs == 1 && scan_pos > 1 && scan_pos <= (int)strlen(hay)+1)
                                       ? hay + scan_pos - 1 : hay;
                IR_block_t *cfg = lower_icn_upto(cset, hay_from);
                if (cfg) {
                    icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
                    dz->cfg = cfg;
                    dz->first = 1;
                    return (bb_node_t){ icn_bb_dcg, dz, 0 };
                }
            }
        }
        for (int j = 0; j < nargs && j < ICN_FNC_GEN_ARGS; j++) {
            tree_t *arg = e->c[1+j];
            if (!arg) continue;
            if (is_suspendable(arg)) {
                icn_fnc_gen_state_t *fg = calloc(1, sizeof(*fg));
                fg->arg_box = icn_bb_build(arg);
                fg->call    = e;
                fg->gen_idx = j;
                fg->nargs   = nargs;
                for (int k2 = 0; k2 < nargs && k2 < ICN_FNC_GEN_ARGS; k2++) {
                    if (k2 == j) continue;
                    fg->args[k2] = bb_eval_value(e->c[1+k2]);
                }
                return (bb_node_t){ icn_bb_fnc, fg, 0 };
            }
        }
    }
    if (e->t == TT_LIMIT && e->n >= 2) {
        bb_node_t lim_gen = icn_bb_build(e->c[0]);
        DESCR_t lim_nd = bb_eval_value(e->c[1]);
        int64_t lim_max = IS_INT_fn(lim_nd) ? (int64_t)lim_nd.i : 0;
        IR_block_t *lim_cfg = lower_icn_limit(lim_gen, lim_max);
        icn_dcg_state_t *lim_dz = calloc(1, sizeof(*lim_dz));
        lim_dz->cfg = lim_cfg; lim_dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, lim_dz, 0 };
    }
    if (e->t == TT_EVERY && e->n >= 1) {
        bb_node_t *gen = calloc(1, sizeof(*gen));
        *gen = icn_bb_build(e->c[0]);
        tree_t *body = (e->n >= 2) ? e->c[1] : NULL;
        IR_block_t *cfg = lower_icn_every(gen, body);
        icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
        dz->cfg = cfg; dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, dz, 0 };
    }
    if (e->t == TT_BANG_BINARY && e->n >= 2) {
        icn_bang_binary_state_t *z = calloc(1, sizeof(*z));
        z->proc_expr = e->c[0];
        DESCR_t listval = bb_eval_value(e->c[1]);
        if (!IS_FAIL_fn(listval)) {
            icn_list_iterate_state_t *lz = calloc(1, sizeof(*lz));
            lz->list_obj = listval;
            lz->pos      = 0;
            z->arg_box   = (bb_node_t){ icn_bb_list_iterate, lz, 0 };
        } else {
            z->arg_box = icn_bb_build(e->c[1]);
        }
        return (bb_node_t){ icn_bb_bang_binary, z, 0 };
    }
    if (e->t == TT_SEQ_EXPR && e->n >= 1) {
        icn_seq_state_t *z = calloc(1, sizeof(*z));
        z->children = e->c;
        z->n        = e->n;
        z->started  = 0;
        return (bb_node_t){ icn_bb_seq_expr, z, 0 };
    }
    if (e->t == TT_SEQ && e->n >= 2 && is_suspendable(e->c[0])) {
        if (is_suspendable(e->c[1]) && e->c[1]->t != TT_FNC) {
            icn_mutual_state_t *z = calloc(1, sizeof(*z));
            z->gen_a    = icn_bb_build(e->c[0]);
            z->ast_b    = e->c[1];
            z->b_started = 0;
            z->a_started = 0;
            return (bb_node_t){ icn_bb_mutual, z, 0 };
        }
        bb_node_t *gen = calloc(1, sizeof(*gen));
        *gen = icn_bb_build(e->c[0]);
        IR_block_t *cfg = lower_icn_every(gen, e->c[1]);
        icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
        dz->cfg = cfg; dz->first = 1;
        return (bb_node_t){ icn_bb_dcg, dz, 0 };
    }
    if (e->t == TT_CSET_COMPL && e->n >= 1 && is_suspendable(e->c[0])) {
        icn_limit_state_t *z = calloc(1, sizeof(*z));
        z->gen = icn_bb_build(e->c[0]);
        return (bb_node_t){ icn_lazy_box, (icn_lazy_state_t*)calloc(1,sizeof(icn_lazy_state_t)), 0 };
    }
    if (e->t == TT_SCAN && e->n >= 1 &&
        (is_suspendable(e->c[0]) || (e->n >= 2 && is_suspendable(e->c[1])))) {
        icn_scan_gen_state_t *z = calloc(1, sizeof(*z));
        z->subj_gen = icn_bb_build(e->c[0]);
        z->body     = (e->n >= 2) ? e->c[1] : NULL;
        z->started  = 0;
        return (bb_node_t){ icn_bb_scan_gen, z, 0 };
    }
    if (e->t == TT_NONNULL && e->n >= 1 && is_suspendable(e->c[0])) {
        icn_limit_state_t *z = calloc(1, sizeof(*z));
        z->gen = icn_bb_build(e->c[0]);
        return (bb_node_t){ icn_bb_nonnull_filter, z, 0 };
    }
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval
        && strcmp(e->c[0]->v.sval, "seq") == 0) {
        icn_to_by_state_t *z = calloc(1, sizeof(*z));
        DESCR_t start = bb_eval_value(e->c[1]);
        z->lo   = IS_INT_fn(start) ? start.i : 1;
        z->hi   = (long long)9e18;
        z->step = (e->n >= 3) ? (long long)to_int(bb_eval_value(e->c[2])) : 1;
        z->cur  = z->lo;
        IR_block_t *seq_cfg = lower_icn_to_by(z->lo, (int64_t)9e18, z->step);
        icn_dcg_state_t *sdz = calloc(1, sizeof(*sdz));
        sdz->cfg = seq_cfg; sdz->first = 1;
        free(z);
        return (bb_node_t){ icn_bb_dcg, sdz, 0 };
    }
    if (e->t == TT_FNC && e->n >= 2 && e->c[0] && e->c[0]->v.sval) {
        const char *fn2 = e->c[0]->v.sval;
        int nargs2 = e->n - 1;
        int is_proc = 0;
        for (int _p = 0; _p < proc_count; _p++)
            if (strcmp(proc_table[_p].name, fn2) == 0) { is_proc = 1; break; }
        if (is_proc) {
            for (int j = 0; j < nargs2 && j < ICN_FNC_GEN_ARGS; j++) {
                tree_t *arg = e->c[1+j];
                if (!arg || !is_suspendable(arg)) continue;
                icn_fnc_gen_state_t *fg = calloc(1, sizeof(*fg));
                fg->arg_box = icn_bb_build(arg);
                fg->call    = e;
                fg->gen_idx = j;
                fg->nargs   = nargs2;
                for (int k2 = 0; k2 < nargs2 && k2 < ICN_FNC_GEN_ARGS; k2++) {
                    if (k2 == j) continue;
                    fg->args[k2] = bb_eval_value(e->c[1+k2]);
                }
                return (bb_node_t){ icn_bb_fnc, fg, 0 };
            }
        }
    }
    if (e->t == TT_ASSIGN && e->n >= 2 && e->c[0] && e->c[0]->t == TT_ITERATE && e->c[0]->n >= 1) {
        tree_t *iter_child = e->c[0]->c[0];
        if (iter_child && (iter_child->t == TT_VAR || iter_child->t == TT_QLIT || iter_child->t == TT_ILIT)) {
            icn_assign_lhs_iter_state_t *z = calloc(1, sizeof(*z));
            z->str_var  = iter_child;
            z->rhs_expr = e->c[1];
            z->pos = 0; z->len = 0; z->buf = NULL;
            return (bb_node_t){ icn_bb_assign_lhs_iter, z, 0 };
        }
    }
    if (e->t == TT_ASSIGN && e->n >= 2 && e->c[0] && e->c[0]->t == TT_IDX && e->c[0]->n >= 2) {
        tree_t *lhs = e->c[0];
        if (is_suspendable(lhs->c[1])) {
            icn_assign_lhs_gen_state_t *z = calloc(1, sizeof(*z));
            z->gen_idx       = icn_bb_build(lhs->c[1]);
            z->lhs_base_expr = lhs->c[0];
            z->rhs_expr      = e->c[1];
            return (bb_node_t){ icn_bb_assign_lhs_gen, z, 0 };
        }
    }
    if (e->t == TT_ASSIGN && e->n >= 2 && is_suspendable(e->c[1])) {
        tree_t *rhs = e->c[1];
        tree_t *leaf = find_leaf_suspendable(rhs);
        int has_var = 0;
        if (leaf && leaf != rhs) {
            for (int _ci = 0; _ci < rhs->n && !has_var; _ci++)
                if (rhs->c[_ci] && rhs->c[_ci]->t == TT_VAR
                    && rhs->c[_ci] != leaf) has_var = 1;
        }
        if (has_var && leaf) {
            icn_assign_cat_state_t *zc = calloc(1, sizeof(*zc));
            zc->leaf_gen = icn_bb_build(leaf);
            zc->rhs_expr = rhs;
            zc->leaf     = leaf;
            zc->lhs      = e->c[0];
            return (bb_node_t){ icn_bb_assign_cat, zc, 0 };
        }
        icn_assign_gen_state_t *z = calloc(1, sizeof(*z));
        z->rhs_gen = icn_bb_build(rhs);
        z->lhs     = e->c[0];
        return (bb_node_t){ icn_bb_assign_gen, z, 0 };
    }
    if (e->t == TT_REVASSIGN && e->n >= 2) {
        tree_t *lhs = e->c[0];
        if (lhs && lhs->t == TT_IDX && lhs->n >= 2 && is_suspendable(lhs->c[1])) {
            icn_revassign_lhs_gen_state_t *z = calloc(1, sizeof(*z));
            z->gen_idx      = icn_bb_build(lhs->c[1]);
            z->lhs_base_expr = lhs->c[0];
            z->rhs_expr      = e->c[1];
            return (bb_node_t){ icn_bb_revassign_lhs_gen, z, 0 };
        }
        icn_revassign_state_t *z = calloc(1, sizeof(*z));
        z->lhs_expr = e->c[0];
        z->rhs_expr = e->c[1];
        z->var_slot = -2;
        if (e->c[1] && is_suspendable(e->c[1])) {
            z->rhs_gen     = icn_bb_build(e->c[1]);
            z->use_rhs_gen = 1;
        }
        return (bb_node_t){ icn_bb_revassign, z, 0 };
    }
    if (e->t == TT_REVSWAP && e->n >= 2) {
        icn_revswap_state_t *z = calloc(1, sizeof(*z));
        z->lhs_expr = e->c[0];
        z->rhs_expr = e->c[1];
        return (bb_node_t){ icn_bb_revswap, z, 0 };
    }
    if (e->t == TT_VAR || e->t == TT_ILIT || e->t == TT_FLIT || e->t == TT_QLIT) {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }
    if (e->t == TT_PROC_FAIL) {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }
    {
        icn_lazy_state_t *z = calloc(1, sizeof(*z));
        z->expr = e;
        return (bb_node_t){ icn_lazy_box, z, 0 };
    }
}
