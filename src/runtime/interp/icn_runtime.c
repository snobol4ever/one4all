#include "icn_runtime.h"
#include "icn_value.h"
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
unsigned long bb_icn_rnd_seed = 12345UL;
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
                if (!st || st->t != TT_STATIC_DECL) continue;
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
            if (!st || st->t != TT_STATIC_DECL) continue;
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_node_t icn_bb_pump_proc_by_name(const char *name, DESCR_t *args, int nargs) {
    if (!name) return (bb_node_t){ NULL, NULL, 0 };
    for (int i = 0; i < proc_count; i++) {
        if (strcmp(proc_table[i].name, name) != 0) continue;
        /* AST → IR → BB path: when an IR_block_t body exists, drive it via icn_bb_dcg.   */
        /* This bypasses SM entirely: no proc_table_call, no sm_call_expression.            */
        if (proc_table[i].ir_body) {
            if (frame_depth < FRAME_STACK_MAX) {
                IcnFrame *f = &frame_stack[frame_depth++];
                memset(f, 0, sizeof *f);
                IcnScope sc_local = proc_table[i].lower_sc;
                f->sc       = sc_local;
                int nslots = sc_local.n > 0 ? sc_local.n : 1;
                if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;
                f->env_n    = nslots;
                for (int k = 0; k < proc_table[i].nparams && k < nargs && k < FRAME_SLOT_MAX; k++)
                    f->env[k] = args[k];
            }
            icn_dcg_state_t *dz = calloc(1, sizeof(*dz));
            dz->cfg = proc_table[i].ir_body;
            dz->first = 1;
            return (bb_node_t){ icn_bb_dcg, dz, 0 };
        }
        /* IJ-SUSPEND-PUMP-WIRE: generator proc (TT_SUSPEND present) without ir_body — route through */
        /* GeneratorState + IR_ICN_PROC_GEN, the same mechanism icn_bb_build uses on the AST path.   */
        /* is_generator was set at lower time (lower.c lower_proc_skeletons) so no AST walk here.    */
        if (proc_table[i].is_generator && proc_table[i].entry_pc >= 0 && g_current_sm_prog) {
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
        icn_bb_oneshot_state_t *oshot1 = calloc(1, sizeof(*oshot1));
        oshot1->val = proc_table_call(i, args, nargs);
        return (bb_node_t){ icn_bb_oneshot, oshot1, 0 };
    }
    return (bb_node_t){ NULL, NULL, 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* DESCR_t-input helpers relocated from icn_value.c during DAI-5b. Used externally by ir_exec.c       */
/* (icn_lconcat_d → IR_ICN_LCONCAT executor), interp_eval.c (icn_proc_as_value → coercion at TT_FNC), */
/* and lower.c (icn_proc_as_value → lower-time literal folding). Internal helper icn_str_concat_d is  */
/* the string-concat fallback used by icn_lconcat_d when operands are not both lists.                 */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_str_concat_d(DESCR_t a, DESCR_t b) {
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_lconcat_d(DESCR_t a, DESCR_t b) {
    if (IS_FAIL_fn(a) || IS_FAIL_fn(b)) return FAILDESCR;
    if (a.v == DT_DATA && b.v == DT_DATA) {
        DESCR_t atag = FIELD_GET_fn(a, "icn_type");
        DESCR_t btag = FIELD_GET_fn(b, "icn_type");
        if (atag.v == DT_S && atag.s && strcmp(atag.s, "list") == 0 &&
            btag.v == DT_S && btag.s && strcmp(btag.s, "list") == 0) {
            DESCR_t asz_d = FIELD_GET_fn(a, "frame_size");
            DESCR_t bsz_d = FIELD_GET_fn(b, "frame_size");
            int an = (int)(IS_INT_fn(asz_d) ? asz_d.i : 0);
            int bn = (int)(IS_INT_fn(bsz_d) ? bsz_d.i : 0);
            int cn = an + bn;
            DESCR_t *celems = GC_malloc((cn > 0 ? cn : 1) * sizeof(DESCR_t));
            DESCR_t aptr = FIELD_GET_fn(a, "frame_elems");
            DESCR_t bptr = FIELD_GET_fn(b, "frame_elems");
            DESCR_t *ae = (aptr.v == DT_DATA) ? (DESCR_t *)aptr.ptr : NULL;
            DESCR_t *be = (bptr.v == DT_DATA) ? (DESCR_t *)bptr.ptr : NULL;
            for (int i = 0; i < an; i++) celems[i]      = ae ? ae[i] : NULVCL;
            for (int i = 0; i < bn; i++) celems[an + i] = be ? be[i] : NULVCL;
            DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void *)celems;
            static int icnlist_lcat_d = 0;
            if (!icnlist_lcat_d) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); icnlist_lcat_d = 1; }
            return DATCON_fn("icnlist", eptr, INTVAL(cn), STRVAL("list"));
        }
    }
    return icn_str_concat_d(a, b);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_proc_as_value(const char *name) {
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
    for (int i = 0; builtins[i]; i++) if (strcmp(builtins[i], name) == 0) return STRVAL(name);
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* DAI (IJ-DEL-ICN-AST, DAI-5b-2): icn_bb_build deleted. The Icon-specific tree_t* AST-walking BB     */
/* builder is gone. Mode-1 Icon executes via the universal interp_eval walker which routes Icon kinds */
/* through ir_exec.c (IR_block_t walker); the tree_t* path through the Icon walker was vestigial.     */
/* icn_bb_dcg, icn_bb_pump_proc_by_name, proc_table machinery above remain (used by modes 2/3/4).     */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
