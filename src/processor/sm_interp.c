#include "sm_interp.h"
#include "sm_prog.h"
#include "coerce.h"
#include "ir_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gc/gc.h>
#include "../interp/icn_runtime.h"
#include "../interp/icn_value.h"
#include "snobol4.h"   /* DESCR_t, PATND_t, DT_* */
#include "sil_macros.h"
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_lit(const char *s);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_span(const char *chars);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_break_(const char *chars);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_breakx(const char *chars);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_any_cs(const char *chars);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_notany(const char *chars);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_len(int64_t n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_pos(int64_t n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_rpos(int64_t n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_tab(int64_t n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_rtab(int64_t n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_arb(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_arbno(DESCR_t inner);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_rem(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_fence(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_fail(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_abort(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_succeed(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_bal(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_epsilon(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_cat(DESCR_t left, DESCR_t right);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_alt(DESCR_t left, DESCR_t right);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_ref(const char *name);         /* deferred *var ref */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_assign_imm(DESCR_t child, DESCR_t var);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_assign_cond(DESCR_t child, DESCR_t var);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_at_cursor(const char *varname);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int exec_stmt(const char *subj_name, DESCR_t *subj_var,
                     DESCR_t pat, DESCR_t *repl, int has_repl);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern char    *VARVAL_fn(DESCR_t d);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t  NV_GET_fn(const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t  icn_frame_env_load(int slot);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void     icn_frame_env_store(int slot, DESCR_t val);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int      icn_frame_env_active(void);
#include "bb_broker.h"
#include <setjmp.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern bb_node_t icn_bb_build(tree_t *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern bb_node_t icn_bb_pump_proc_by_name(const char *name, DESCR_t *args, int nargs);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int sm_yield_to_caller(DESCR_t v);
#include "../../frontend/prolog/pl_broker.h"
#include "../../runtime/interp/pl_runtime.h"
int      g_sm_step_limit = 0;
int      g_sm_steps_done = 0;
jmp_buf  g_sm_step_jmp;
int      g_exprs_audit_push_expr  = 0;
int      g_exprs_audit_push_expression = 0;
int      g_exprs_audit_oor  = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void exprs_audit_summary(void) {
    if (getenv("SCRIP_EXPRS_AUDIT")) {
        fprintf(stderr,
                "[CHUNKS-AUDIT] summary: SM_PUSH_EXPRESSION=%d  SM_PUSH_EXPR=%d  out_of_range=%d\n",
                g_exprs_audit_push_expression,
                g_exprs_audit_push_expr,
                g_exprs_audit_oor);
    }
}
__attribute__((constructor))
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void exprs_audit_register(void) {
    atexit(exprs_audit_summary);
}
GeneratorState *g_current_generator_state = NULL;
#define EVERY_TABLE_INIT 16
static tree_t **g_every_table     = NULL;
static int     g_every_table_n   = 0;
static int     g_every_table_cap = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int every_table_register(tree_t *ast)
{
    if (g_every_table_n >= g_every_table_cap) {
        int new_cap = g_every_table_cap ? g_every_table_cap * 2 : EVERY_TABLE_INIT;
        tree_t **nt = (tree_t **)realloc(g_every_table, new_cap * sizeof(tree_t *));
        if (!nt) { fprintf(stderr, "every_table: OOM\n"); abort(); }
        g_every_table     = nt;
        g_every_table_cap = new_cap;
    }
    int id = g_every_table_n++;
    g_every_table[id] = ast;
    return id;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *every_table_lookup(int id)
{
    if (id < 0 || id >= g_every_table_n) return NULL;
    return g_every_table[id];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void every_table_reset(void)
{
    if (g_every_table) { free(g_every_table); g_every_table = NULL; }
    g_every_table_n = 0;
    g_every_table_cap = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pump_print(DESCR_t val, void *arg) {
    (void)arg;
    char *s = VARVAL_fn(val);
    if (s) printf("%s\n", s);
}
#define SM_STACK_INIT 16
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_state_init(SM_State *st)
{
    memset(st, 0, sizeof *st);
    st->stack     = malloc(SM_STACK_INIT * sizeof(DESCR_t));
    st->stack_cap = SM_STACK_INIT;
    st->sp        = 0;
    st->last_ok   = 1;
    st->pc        = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_state_free(SM_State *st)
{
    free(st->stack);
    st->stack     = NULL;
    st->stack_cap = 0;
    st->sp        = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push(SM_State *st, DESCR_t d)
{
    if (st->sp >= st->stack_cap) {
        st->stack_cap *= 2;
        st->stack = realloc(st->stack, st->stack_cap * sizeof(DESCR_t));
        if (!st->stack) { fprintf(stderr, "sm_interp: out of memory\n"); abort(); }
    }
    st->stack[st->sp++] = d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sm_pop(SM_State *st)
{
    if (st->sp <= 0) {
        fprintf(stderr, "sm_interp: stack underflow\n");
        abort();
    }
    return st->stack[--st->sp];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sm_peek(SM_State *st)
{
    if (st->sp <= 0) {
        fprintf(stderr, "sm_interp: peek on empty stack\n");
        abort();
    }
    return st->stack[st->sp - 1];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t nv_fold_get(const char *raw) {
    if (!raw || !*raw) return NULVCL;
    char *n = GC_strdup(raw); sno_fold_name(n);
    return NV_GET_fn(n);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void nv_fold_set(const char *raw, DESCR_t val) {
    if (!raw || !*raw) return;
    char *n = GC_strdup(raw); sno_fold_name(n);
    NV_SET_fn(n, val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_interp_run(SM_Program *prog, SM_State *st)
{
    int was_active = g_sm_dispatch_active;
    g_sm_dispatch_active = 1;
    int _rc = sm_interp_run_inner(prog, st);
    g_sm_dispatch_active = was_active;
    return _rc;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_interp_run_inner(SM_Program *prog, SM_State *st)
{
    while (st->pc < prog->count) {
        SM_Instr *ins = &prog->instrs[st->pc];
        st->pc++;
        switch (ins->op) {
        case SM_LABEL:
            break;
        case SM_DEFINE_ENTRY:
            break;
        case SM_HALT:
            return 0;
        case SM_STNO: {
            extern void comm_stno(int n);
            int sm_stno = (int)ins->a[0].i;
            comm_stno(sm_stno);
            {
                extern void mon_emit_label_bin(int64_t stno);
                mon_emit_label_bin((int64_t)sm_stno);
            }
            kw_stno = sm_stno;
            st->sp = 0;
            {
                static int s_trace_init = 0, s_trace_on = 0;
                if (!s_trace_init) {
                    const char *e = getenv("ONE4ALL_STEP_TRACE");
                    s_trace_on = (e && e[0] == '1');
                    s_trace_init = 1;
                }
                if (s_trace_on)
                    fprintf(stderr, "SMSTEP %d sm_stno=%d pc=%d\n",
                            g_sm_steps_done + 1, sm_stno, st->pc);
            }
            if (g_sm_step_limit > 0 && g_sm_steps_done++ >= g_sm_step_limit)
                longjmp(g_sm_step_jmp, 1);
            break;
        }
        case SM_JUMP:
            st->pc = (int)ins->a[0].i;
            break;
        case SM_JUMP_S:
            if (st->last_ok) st->pc = (int)ins->a[0].i;
            break;
        case SM_JUMP_F:
            if (!st->last_ok) st->pc = (int)ins->a[0].i;
            break;
        case SM_PUSH_LIT_S: {
            const char *s = ins->a[0].s ? ins->a[0].s : "";
            int64_t     n = ins->a[1].i;
            DESCR_t d;
            d.v    = DT_S;
            d.s    = (char *)s;
            d.slen = (n > 0) ? (uint32_t)n : 0;
            sm_push(st, d);
            break;
        }
        case SM_PUSH_LIT_CS: {
            const char *s = ins->a[0].s ? ins->a[0].s : "";
            sm_push(st, CSETVAL(s));
            break;
        }
        case SM_PUSH_LIT_I:
            sm_push(st, INTVAL(ins->a[0].i));
            break;
        case SM_PUSH_LIT_F:
            sm_push(st, REALVAL(ins->a[0].f));
            break;
        case SM_PUSH_NULL:
            sm_push(st, NULVCL);
            st->last_ok = 1;
            break;
        case SM_PUSH_NULL_NOFLIP:
            sm_push(st, NULVCL);
            break;
        case SM_PUSH_VAR: {
            const char *name = ins->a[0].s;
            DESCR_t val;
            { extern int g_lang;
              if (g_lang == LANG_ICN && name && name[0] == '&') {
                extern DESCR_t icn_kw_read(const char *kw);
                DESCR_t kv = icn_kw_read(name + 1);
                sm_push(st, kv);
                st->last_ok = IS_FAIL_fn(kv) ? 0 : 1;
                break;
              }
            }
            val = NV_GET_fn(name);
            { extern int g_lang;
            if (g_lang == LANG_ICN && (val.v == DT_S || val.v == DT_SNUL)) {
                extern int proc_count;
                extern IcnProcEntry proc_table[];
                for (int _pi = 0; _pi < proc_count; _pi++) {
                    if (proc_table[_pi].name && strcmp(proc_table[_pi].name, name) == 0) {
                        val.v    = DT_E;
                        val.slen = (uint32_t)_pi;
                        val.i    = proc_table[_pi].entry_pc;
                        break;
                    }
                }
                if (val.v == DT_SNUL || val.v == DT_S) {
                    extern DESCR_t icn_proc_as_value(const char *);
                    DESCR_t pv = icn_proc_as_value(name);
                    if (pv.v != DT_FAIL) val = pv;
                }
            } }
            sm_push(st, val);
            st->last_ok = (val.v != DT_FAIL);
            break;
        }
        case SM_PUSH_EXPR: {
            if (getenv("SCRIP_EXPRS_AUDIT")) {
                g_exprs_audit_push_expr++;
                fprintf(stderr, "[CHUNKS-AUDIT] SM_PUSH_EXPR fired at pc=%d (legacy tree_t* path)\n",
                        st->pc);
            }
            DESCR_t d;
            d.v    = DT_E;
            d.slen = 0;
            d.ptr  = ins->a[0].ptr;
            sm_push(st, d);
            st->last_ok = 1;
            break;
        }
        case SM_PUSH_EXPRESSION: {
            if (getenv("SCRIP_EXPRS_AUDIT")) {
                g_exprs_audit_push_expression++;
                int entry_pc = (int)ins->a[0].i;
                if (entry_pc < 0 || entry_pc >= prog->count) {
                    g_exprs_audit_oor++;
                    fprintf(stderr, "[CHUNKS-AUDIT] SM_PUSH_EXPRESSION at pc=%d: entry_pc=%d out of range [0,%d)\n",
                            st->pc, entry_pc, prog->count);
                }
            }
            DESCR_t d;
            d.v    = DT_E;
            d.slen = 1;
            d.i    = ins->a[0].i;
            sm_push(st, d);
            st->last_ok = 1;
            break;
        }
        case SM_CALL_EXPRESSION: {
            int entry_pc = (int)ins->a[0].i;
            if (entry_pc < 0 || entry_pc >= prog->count
                    || st->call_depth >= SM_CALL_STACK_MAX) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            SmCallFrame *fr = &st->call_stack[st->call_depth++];
            fr->ret_pc = st->pc;
            fr->ret_ok = 1;
            fr->retval_name = NULL;
            fr->nsaved = 0;
            fr->caller_sp = st->sp;
            if (st->sp > 0) {
                fr->caller_stack = GC_malloc(st->sp * sizeof(DESCR_t));
                memcpy(fr->caller_stack, st->stack, st->sp * sizeof(DESCR_t));
            } else {
                fr->caller_stack = NULL;
            }
            st->sp = 0;
            st->pc = entry_pc;
            goto sm_call_done;
        }
        case SM_STORE_VAR: {
            const char *name = ins->a[0].s;
            DESCR_t val = sm_pop(st);
            if (val.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            { extern int g_lang;
              if (g_lang == LANG_ICN && name && name[0] == '&') {
                if (!kw_assign(name + 1, val)) {
                    sm_push(st, FAILDESCR);
                    st->last_ok = 0;
                    break;
                }
                sm_push(st, val);
                st->last_ok = 1;
                break;
              }
            }
            NV_SET_fn(name, val);
            sm_push(st, val);
            st->last_ok = 1;
            break;
        }
        case SM_VOID_POP:
            sm_pop(st);
            break;
        case SM_ADD:
        case SM_SUB:
        case SM_MUL:
        case SM_DIV:
        case SM_MOD:
        case SM_EXP: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            if (l.v == DT_FAIL || r.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            if (l.v == DT_S) l = INTVAL(to_int(l));
            if (r.v == DT_S) r = INTVAL(to_int(r));
            if (l.v == DT_SNUL) l = INTVAL(0);
            if (r.v == DT_SNUL) r = INTVAL(0);
            DESCR_t result = shared_arith(l, r, ins->op);
            sm_push(st, result);
            st->last_ok = (result.v != DT_FAIL);
            break;
        }
        case SM_NEG: {
            DESCR_t v = sm_pop(st);
            if (v.v == DT_I) sm_push(st, INTVAL(-v.i));
            else              sm_push(st, REALVAL(-to_real(v)));
            break;
        }
        case SM_CONCAT: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            DESCR_t result = CONCAT_fn(l, r);
            sm_push(st, result);
            st->last_ok = (result.v != DT_FAIL);
            break;
        }
        case SM_COERCE_NUM: {
            DESCR_t v = sm_pop(st);
            if (v.v == DT_S) {
                int64_t iv = to_int(v);
                if (iv != 0 || (v.s && v.s[0] == '0')) { sm_push(st, INTVAL(iv)); }
                else { double rv = to_real(v); sm_push(st, REALVAL(rv)); }
            } else { sm_push(st, v); }
            st->last_ok = 1;
            break;
        }
        case SM_PAT_LIT: {
            sm_push(st, pat_lit(ins->a[0].s ? ins->a[0].s : ""));
            break;
        }
        case SM_PAT_ANY: {
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_any_cs(cs ? cs : ""));
            break;
        }
        case SM_PAT_NOTANY: {
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_notany(cs ? cs : ""));
            break;
        }
        case SM_PAT_SPAN: {
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_span(cs ? cs : ""));
            break;
        }
        case SM_PAT_BREAK: {
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_break_(cs ? cs : ""));
            break;
        }
        case SM_PAT_LEN: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_len(n));
            break;
        }
        case SM_PAT_POS: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_pos(n));
            break;
        }
        case SM_PAT_RPOS: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_rpos(n));
            break;
        }
        case SM_PAT_TAB: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_tab(n));
            break;
        }
        case SM_PAT_RTAB: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_rtab(n));
            break;
        }
        case SM_PAT_ARB:     sm_push(st, pat_arb());     break;
        case SM_PAT_ARBNO:   { DESCR_t _inner = sm_pop(st); sm_push(st, pat_arbno(_inner)); } break;
        case SM_PAT_REM:     sm_push(st, pat_rem());     break;
        case SM_PAT_FAIL:    sm_push(st, pat_fail());    break;
        case SM_PAT_SUCCEED: sm_push(st, pat_succeed()); break;
        case SM_PAT_EPS:     sm_push(st, pat_epsilon()); break;
        case SM_PAT_FENCE0:   sm_push(st, pat_fence());   break;
        case SM_PAT_FENCE1:  { DESCR_t _ch = sm_pop(st); sm_push(st, pat_fence_p(_ch)); } break;
        case SM_PAT_ABORT:   sm_push(st, pat_abort());   break;
        case SM_PAT_BAL:     sm_push(st, pat_bal());     break;
        case SM_PAT_CAT: {
            DESCR_t right = sm_pop(st);
            DESCR_t left  = sm_pop(st);
            sm_push(st, pat_cat(left, right));
            break;
        }
        case SM_PAT_ALT: {
            DESCR_t right = sm_pop(st);
            DESCR_t left  = sm_pop(st);
            sm_push(st, pat_alt(left, right));
            break;
        }
        case SM_PAT_DEREF: {
            DESCR_t v = sm_pop(st);
            if (v.v == DT_P) {
                sm_push(st, v);
            } else if (v.v == DT_S && v.s) {
                sm_push(st, pat_lit(v.s));
            } else {
                const char *name = VARVAL_fn(v);
                sm_push(st, pat_ref(name ? name : ""));
            }
            break;
        }
        case SM_PAT_REFNAME: {
            const char *name = ins->a[0].s ? ins->a[0].s : "";
            sm_push(st, pat_ref(name));
            break;
        }
        case SM_PAT_CAPTURE: {
            DESCR_t child = sm_pop(st);
            const char *vname = ins->a[0].s ? ins->a[0].s : "";
            DESCR_t var = NAME_fn(vname);
            int kind = (int)ins->a[1].i;
            if (kind == 1)
                sm_push(st, pat_assign_imm(child, var));
            else if (kind == 2)
                sm_push(st, pat_cat(child, pat_at_cursor(vname)));
            else
                sm_push(st, pat_assign_cond(child, var));
            break;
        }
        case SM_PAT_CAPTURE_FN: {
            DESCR_t child = sm_pop(st);
            const char *fname    = ins->a[0].s ? ins->a[0].s : "";
            const char *namelist = ins->a[2].s;
            if (namelist && namelist[0]) {
                int nnames = 1;
                for (const char *q = namelist; *q; q++) if (*q == '\t') nnames++;
                char **names = (char **)GC_MALLOC((size_t)nnames * sizeof(char *));
                int ni = 0;
                const char *start = namelist;
                for (const char *q = namelist; ; q++) {
                    if (*q == '\t' || *q == '\0') {
                        size_t len = (size_t)(q - start);
                        char *nm = (char *)GC_MALLOC(len + 1);
                        memcpy(nm, start, len);
                        nm[len] = '\0';
                        names[ni++] = nm;
                        if (*q == '\0') break;
                        start = q + 1;
                    }
                }
                int is_imm = (int)ins->a[1].i;
                sm_push(st, is_imm
                    ? pat_assign_callcap_named_imm(child, fname, NULL, 0, names, nnames)
                    : pat_assign_callcap_named(child, fname, NULL, 0, names, nnames));
            } else {
                int is_imm = (int)ins->a[1].i;
                sm_push(st, is_imm
                    ? pat_assign_callcap_named_imm(child, fname, NULL, 0, NULL, 0)
                    : pat_assign_callcap(child, fname, NULL, 0));
            }
            break;
        }
        case SM_PAT_CAPTURE_FN_ARGS: {
            int nargs = (int)ins->a[2].i;
            DESCR_t *argv = nargs > 0
                ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
                : NULL;
            for (int i = nargs - 1; i >= 0; i--) argv[i] = sm_pop(st);
            DESCR_t child = sm_pop(st);
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            int is_imm = (int)ins->a[1].i;
            sm_push(st, is_imm
                ? pat_assign_callcap_named_imm(child, fname, argv, nargs, NULL, 0)
                : pat_assign_callcap(child, fname, argv, nargs));
            break;
        }
        case SM_PAT_USERCALL: {
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            sm_push(st, pat_user_call(fname, NULL, 0));
            break;
        }
        case SM_PAT_USERCALL_ARGS: {
            int nargs = (int)ins->a[1].i;
            DESCR_t *argv = nargs > 0
                ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
                : NULL;
            for (int i = nargs - 1; i >= 0; i--) argv[i] = sm_pop(st);
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            sm_push(st, pat_user_call(fname, argv, nargs));
            break;
        }
        case SM_EXEC_STMT: {
            int has_repl = (int)ins->a[1].i;
            DESCR_t repl   = sm_pop(st);
            DESCR_t subj_d = sm_pop(st);
            DESCR_t pat_d  = sm_pop(st);
            const char *sname = ins->a[0].s;
            IR_block_t *pat_dcg = (int)ins->a[2].i >= 0 ? g_current_sm_prog->dcg_table[(int)ins->a[2].i] : NULL;
            int ok;
            if (pat_dcg) {
                ok = IR_exec_pat(pat_dcg, sname, &subj_d,
                                 has_repl ? &repl : NULL, has_repl);
            } else {
                ok = exec_stmt(sname, &subj_d, pat_d,
                               has_repl ? &repl : NULL, has_repl);
            }
            st->last_ok = ok;
            break;
        }
        case SM_BB_PUMP: {
            sm_pop(st);
            fprintf(stderr, "[NO-AST] SM_BB_PUMP stub: needs fresh SM/BB lowering\n");
            st->last_ok = 0;
            break;
        }
        case SM_BB_ONCE: {
            sm_pop(st);
            fprintf(stderr, "[NO-AST] SM_BB_ONCE stub: needs fresh SM/BB lowering\n");
            st->last_ok = 0;
            break;
        }
        case SM_BB_EVAL: {
            (void)ins;
            fprintf(stderr, "[NO-AST] SM_BB_EVAL stub: needs fresh SM/BB lowering\n");
            st->last_ok = 0;
            sm_push(st, FAILDESCR);
            break;
        }
        case SM_BB_ONCE_PROC: {
            (void)ins;
            fprintf(stderr, "[NO-AST] SM_BB_ONCE_PROC stub: needs fresh SM/BB lowering\n");
            st->last_ok = 0;
            break;
        }
        case SM_BB_PUMP_PROC: {
            const char *name  = ins->a[0].s;
            int         nargs = (int)ins->a[1].i;
            DESCR_t *args = NULL;
            if (nargs > 0) {
                args = calloc(nargs, sizeof(DESCR_t));
                for (int k = nargs - 1; k >= 0; k--) args[k] = sm_pop(st);
            }
            bb_node_t node = icn_bb_pump_proc_by_name(name, args, nargs);
            if (!node.fn) {
                if (args) free(args);
                st->last_ok = 0;
                break;
            }
            g_ast_pump_active++;
            { extern int g_lang; int saved_lang = g_lang; g_lang = LANG_ICN;
            int ticks = bb_broker(node, BB_PUMP, pump_print, NULL);
            g_lang = saved_lang;
            g_ast_pump_active--;
            st->last_ok = (ticks > 0); }
            break;
        }
        case SM_BB_PUMP_CASE: {
            int ncases      = (int)ins->a[0].i;
            int has_default = (int)ins->a[1].i;
            int default_pc = -1;
            if (has_default) {
                DESCR_t d = sm_pop(st);
                default_pc = (d.v == DT_E && d.slen == 1) ? (int)d.i : -1;
            }
            int *cmp_kinds = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
            int *val_pcs   = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
            int *body_pcs  = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
            for (int k = ncases - 1; k >= 0; k--) {
                DESCR_t b = sm_pop(st);
                DESCR_t v = sm_pop(st);
                DESCR_t c = sm_pop(st);
                body_pcs[k]  = (b.v == DT_E && b.slen == 1) ? (int)b.i : -1;
                val_pcs[k]   = (v.v == DT_E && v.slen == 1) ? (int)v.i : -1;
                cmp_kinds[k] = (c.v == DT_I) ? (int)c.i : (int)TT_EQ;
            }
            DESCR_t topic_d = sm_pop(st);
            int topic_pc = (topic_d.v == DT_E && topic_d.slen == 1) ? (int)topic_d.i : -1;
            DESCR_t topic = (topic_pc >= 0) ? sm_call_expression(topic_pc) : NULVCL;
            DESCR_t result = NULVCL;
            int matched   = 0;
            for (int k = 0; k < ncases; k++) {
                if (val_pcs[k] < 0 || body_pcs[k] < 0) continue;
                DESCR_t wval = sm_call_expression(val_pcs[k]);
                int match = 0;
                if ((tree_e)cmp_kinds[k] == TT_LEQ) {
                    const char *ts = IS_STR_fn(topic) ? topic.s : VARVAL_fn(topic);
                    const char *ws = IS_STR_fn(wval)  ? wval.s  : VARVAL_fn(wval);
                    match = (ts && ws && strcmp(ts, ws) == 0);
                } else {
                    if (IS_INT_fn(topic) && IS_INT_fn(wval)) {
                        match = (topic.i == wval.i);
                    } else {
                        const char *ts = VARVAL_fn(topic);
                        const char *ws = VARVAL_fn(wval);
                        match = (ts && ws && strcmp(ts, ws) == 0);
                    }
                }
                if (match) {
                    result = sm_call_expression(body_pcs[k]);
                    matched = 1;
                    break;
                }
            }
            if (!matched && default_pc >= 0) {
                result = sm_call_expression(default_pc);
                matched = 1;
            }
            sm_push(st, result);
            st->last_ok = matched;
            break;
        }
        case SM_BB_PUMP_SM: {
            DESCR_t d = sm_pop(st);
            if (d.v != DT_E || d.slen != 1) {
                st->last_ok = 0;
                break;
            }
            int entry_pc = (int)d.i;
            if (entry_pc < 0 || entry_pc >= prog->count) {
                st->last_ok = 0;
                break;
            }
            GeneratorState *gs = generator_state_new(entry_pc);
            int ticks = bb_broker_drive_sm(gs, pump_print, NULL);
            st->last_ok = (ticks > 0);
            break;
        }
        case SM_BB_PUMP_EVERY: {
            (void)ins;
            fprintf(stderr, "[NO-AST] SM_BB_PUMP_EVERY stub: needs fresh SM/BB lowering\n");
            st->last_ok = 0;
            sm_push(st, NULVCL);
            break;
        }
        case SM_EXEC_BB: {
            IR_block_t * cfg = g_current_sm_prog->dcg_table[(int)ins->a[0].i];
            DESCR_t _val;
            if (!cfg) { _val = FAILDESCR; }
            else if (ins->a[1].i == 0) { ins->a[1].i = 1; _val = IR_exec_once(cfg); }
            else                        { _val = IR_exec_resume(cfg); }
            if (IS_FAIL_fn(_val)) ins->a[1].i = 0;
            st->last_ok = !IS_FAIL_fn(_val);
            sm_push(st, _val);
            break;
        }
        case SM_PUMP_BB: {
            IR_block_t * cfg = g_current_sm_prog->dcg_table[(int)ins->a[0].i];
            int _ticks = cfg ? IR_exec_pump(cfg, NULL, NULL) : 0;
            st->last_ok = (_ticks > 0);
            sm_push(st, INTVAL(_ticks));
            break;
        }
        case SM_SUSPEND_VALUE: {
            DESCR_t v = sm_pop(st);
            if (sm_yield_to_caller(v)) {
                st->last_ok = 1;
            } else {
                sm_push(st, v);
                st->last_ok = !IS_FAIL_fn(v);
            }
            break;
        }
        case SM_CALL_FN: {
            const char *name  = ins->a[0].s;
            int         nargs = (int)ins->a[1].i;
            if (name && strcmp(name, "INDIR_GET") == 0) {
                DESCR_t name_d = sm_pop(st);
                DESCR_t val;
                if (IS_NAMEPTR(name_d)) {
                    val = NAME_DEREF_PTR(name_d);
                } else if (IS_NAMEVAL(name_d)) {
                    val = nv_fold_get(name_d.s);
                } else {
                    val = nv_fold_get(VARVAL_fn(name_d));
                }
                sm_push(st, val);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "NAME_PUSH") == 0) {
                DESCR_t name_d = sm_pop(st);
                const char *vname0 = VARVAL_fn(name_d);
                char *vname = GC_strdup(vname0 ? vname0 : ""); sno_fold_name(vname);
                sm_push(st, NAMEVAL(vname));
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "ASGN_INDIR") == 0) {
                DESCR_t name_d = sm_pop(st);
                DESCR_t val    = sm_pop(st);
                int ok = 0;
                if (IS_NAMEPTR(name_d)) {
                    *(DESCR_t*)name_d.ptr = val; ok = 1;
                } else if (IS_NAMEVAL(name_d)) {
                    nv_fold_set(name_d.s, val); ok = 1;
                } else {
                    const char *vname0 = VARVAL_fn(name_d);
                    if (vname0 && *vname0) { nv_fold_set(vname0, val); ok = 1; }
                }
                sm_push(st, val);
                st->last_ok = ok;
                break;
            }
            if (name && strcmp(name, "NRETURN_ASGN") == 0) {
                const char *fname = ins->a[1].s;
                DESCR_t rhs = sm_pop(st);
                DESCR_t fres = INVOKE_fn(fname, NULL, 0);
                int ok = 0;
                if (IS_NAMEPTR(fres)) { NAME_DEREF_PTR(fres) = rhs; ok = 1; }
                else if (IS_NAMEVAL(fres)) { NV_SET_fn(fres.s, rhs); ok = 1; }
                else {
                    char setname[256];
                    snprintf(setname, sizeof(setname), "%s_SET", fname ? fname : "");
                    DESCR_t sargs[2] = { rhs, fres };
                    DESCR_t sr = INVOKE_fn(setname, sargs, 2);
                    ok = (sr.v != DT_FAIL);
                }
                sm_push(st, rhs);
                st->last_ok = ok;
                break;
            }
            if (name && strcmp(name, "IDX") == 0) {
                if (nargs == 2) {
                    DESCR_t idx  = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t r = subscript_get(base, idx);
                    sm_push(st, r);
                    st->last_ok = (r.v != DT_FAIL);
                } else if (nargs == 3) {
                    DESCR_t j    = sm_pop(st); DESCR_t i = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t r = subscript_get2(base, i, j);
                    sm_push(st, r);
                    st->last_ok = (r.v != DT_FAIL);
                } else {
                    int n = nargs;
                    DESCR_t raw[32];
                    for (int k = 0; k < n; k++) raw[k] = sm_pop(st);
                    DESCR_t base = raw[n-1];
                    DESCR_t fargs[32]; fargs[0] = base;
                    for (int k = 0; k < n-1; k++) fargs[k+1] = raw[n-2-k];
                    DESCR_t r = INVOKE_fn("ITEM", fargs, n);
                    sm_push(st, r);
                    st->last_ok = (r.v != DT_FAIL);
                }
                break;
            }
            if (name && strcmp(name, "IDX_SET") == 0) {
                if (nargs == 3) {
                    DESCR_t i    = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t val  = sm_pop(st);
                    st->last_ok = subscript_set(base, i, val);
                    { extern void comm_var(const char *, DESCR_t);
                      comm_var("<lval>", val); }
                    sm_push(st, val);
                } else if (nargs == 4) {
                    DESCR_t j    = sm_pop(st); DESCR_t i = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t val  = sm_pop(st);
                    st->last_ok = subscript_set2(base, i, j, val);
                    { extern void comm_var(const char *, DESCR_t);
                      comm_var("<lval>", val); }
                    sm_push(st, val);
                } else {
                    int ndim = nargs - 2;
                    DESCR_t idx[32];
                    for (int k = ndim - 1; k >= 0; k--) idx[k] = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t val  = sm_pop(st);
                    DESCR_t fargs[32]; fargs[0] = val; fargs[1] = base;
                    for (int k = 0; k < ndim; k++) fargs[k+2] = idx[k];
                    DESCR_t r = INVOKE_fn("ITEM_SET", fargs, ndim + 2);
                    st->last_ok = (r.v != DT_FAIL);
                    { extern void comm_var(const char *, DESCR_t);
                      comm_var("<lval>", val); }
                    sm_push(st, val);
                }
                break;
            }
            if (name && strcmp(name, "ICN_RANDOM") == 0) {
                DESCR_t v = sm_pop(st);
                if (IS_FAIL_fn(v)) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                unsigned long rnd = bb_icn_rnd_seed >> 33;
                DESCR_t result = FAILDESCR;
                if (IS_INT_fn(v)) {
                    long long n = v.i;
                    if (n <= 0) goto icn_random_fail;
                    result = INTVAL((long long)(rnd % (unsigned long)n) + 1);
                } else if (v.v == DT_S || v.v == DT_SNUL) {
                    const char *s = VARVAL_fn(v);
                    if (!s || !*s) goto icn_random_fail;
                    long slen = (long)strlen(s);
                    char *out = GC_malloc(2); out[0] = s[rnd % (unsigned long)slen]; out[1] = '\0';
                    result = STRVAL(out);
                } else if (v.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(v, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        int n = (int)FIELD_GET_fn(v, "frame_size").i;
                        if (n <= 0) goto icn_random_fail;
                        DESCR_t ea = FIELD_GET_fn(v, "frame_elems");
                        if (ea.v == DT_DATA && ea.ptr) {
                            DESCR_t *elems = (DESCR_t *)ea.ptr;
                            result = elems[rnd % (unsigned long)n];
                        }
                    } else if (v.u && v.u->type && v.u->type->nfields > 0 && v.u->fields) {
                        int n = v.u->type->nfields;
                        result = v.u->fields[rnd % (unsigned long)n];
                    }
                } else if (v.v == DT_T) {
                    if (!v.tbl || v.tbl->size <= 0) goto icn_random_fail;
                    int target = (int)(rnd % (unsigned long)v.tbl->size);
                    int seen = 0;
                    for (int b = 0; b < TABLE_BUCKETS; b++) {
                        for (TBPAIR_t *bp = v.tbl->buckets[b]; bp; bp = bp->next) {
                            if (seen == target) { result = bp->val; goto icn_random_done; }
                            seen++;
                        }
                    }
                }
                goto icn_random_done;
                icn_random_fail: result = FAILDESCR;
                icn_random_done:
                sm_push(st, result);
                st->last_ok = (result.v != DT_FAIL);
                break;
            }
            if (name && strcmp(name, "ICN_RANDOM_SET") == 0) {
                DESCR_t base = sm_pop(st);
                DESCR_t val  = sm_pop(st);
                bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                unsigned long rnd = bb_icn_rnd_seed >> 33;
                int ok = 0;
                if (base.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(base, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        int n = (int)FIELD_GET_fn(base, "frame_size").i;
                        if (n > 0) {
                            int fi = (int)(rnd % (unsigned long)n);
                            ok = subscript_set(base, INTVAL(fi + 1), val);
                        }
                    } else if (base.u && base.u->type && base.u->type->nfields > 0 && base.u->fields) {
                        int fi = (int)(rnd % (unsigned long)base.u->type->nfields);
                        base.u->fields[fi] = val;
                        ok = 1;
                    }
                } else if (base.v == DT_T && base.tbl && base.tbl->size > 0) {
                    int target = (int)(rnd % (unsigned long)base.tbl->size), seen = 0;
                    for (int _b = 0; _b < TABLE_BUCKETS; _b++)
                        for (TBPAIR_t *bp = base.tbl->buckets[_b]; bp; bp = bp->next)
                            if (seen++ == target) { bp->val = val; ok = 1; goto icn_random_set_done; }
                    icn_random_set_done:;
                }
                sm_push(st, val);
                st->last_ok = ok;
                break;
            }
            if (name && strcmp(name, "ICN_ITERATE_FIRST_SET") == 0) {
                DESCR_t varname = sm_pop(st);
                DESCR_t base    = sm_pop(st);
                DESCR_t val     = sm_pop(st);
                if (base.v == DT_DATA && base.u && base.u->type && base.u->type->nfields > 0 && base.u->fields) {
                    base.u->fields[0] = val;
                } else if (base.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(base, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        DESCR_t ea = FIELD_GET_fn(base, "frame_elems");
                        DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                        if (elems) elems[0] = val;
                    }
                } else if ((base.v == DT_S || base.v == DT_SNUL) && base.s) {
                    const char *str = base.s;
                    long slen = base.slen > 0 ? (long)base.slen : (long)strlen(str);
                    if (slen > 0) {
                        const char *ch = VARVAL_fn(val);
                        char *ns = GC_malloc((size_t)(slen + 1));
                        memcpy(ns, str, (size_t)slen); ns[0] = (ch && *ch) ? ch[0] : '\0'; ns[slen] = '\0';
                        DESCR_t sv = STRVAL(ns);
                        const char *vname = (varname.v == DT_S && varname.s) ? varname.s : NULL;
                        if (vname && icn_frame_env_active()) {
                            int _sl = scope_get(&FRAME.sc, vname);
                            if (_sl >= 0) { icn_frame_env_store(_sl, sv); }
                            else set_and_trace(vname, sv);
                        } else if (vname) set_and_trace(vname, sv);
                    }
                } else if (base.v == DT_T && base.tbl) {
                    for (int _b = 0; _b < TABLE_BUCKETS; _b++)
                        for (TBPAIR_t *_bp = base.tbl->buckets[_b]; _bp; _bp = _bp->next)
                            _bp->val = val;
                }
                sm_push(st, val);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "ICN_BANG_NEXT") == 0) {
                DESCR_t result = FAILDESCR;
                if (!g_current_generator_state) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                DESCR_t container = g_current_generator_state->locals[0];
                long    pos       = g_current_generator_state->locals[1].i;
                if (container.v == DT_S || container.v == DT_SNUL) {
                    const char *s = container.s ? container.s : "";
                    long slen = IS_CSET_fn(container) ? (long)strlen(s)
                              : (container.slen > 0 && container.slen != 0xFFFFFFFFu) ? (long)container.slen
                              : (long)strlen(s);
                    if (pos < slen) {
                        char *ch = GC_malloc(2); ch[0] = s[pos]; ch[1] = '\0';
                        result = (DESCR_t){ .v = DT_S, .slen = 1, .s = ch };
                    }
                } else if (container.v == DT_DATA && container.u) {
                    DESCR_t tag = FIELD_GET_fn(container, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        DESCR_t ea = FIELD_GET_fn(container, "frame_elems");
                        int n = (int)FIELD_GET_fn(container, "frame_size").i;
                        DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                        if (elems && pos < n) result = elems[pos];
                    } else if (container.u->type && container.u->type->nfields > 0 && container.u->fields) {
                        int n = container.u->type->nfields;
                        if (pos < n) result = container.u->fields[pos];
                    }
                } else if (container.v == DT_T && container.tbl) {
                    long seen = 0;
                    for (int b = 0; b < TABLE_BUCKETS; b++) {
                        for (TBPAIR_t *bp = container.tbl->buckets[b]; bp; bp = bp->next) {
                            if (seen == pos) { result = bp->val; goto icn_bang_done; }
                            seen++;
                        }
                    }
                }
                icn_bang_done:
                if (!IS_FAIL_fn(result))
                    g_current_generator_state->locals[1].i = pos + 1;
                sm_push(st, result);
                st->last_ok = !IS_FAIL_fn(result);
                break;
            }
            if (name && (strcmp(name, "ICN_SECTION_RANGE") == 0 ||
                         strcmp(name, "ICN_SECTION_PLUS")  == 0 ||
                         strcmp(name, "ICN_SECTION_MINUS") == 0)) {
                DESCR_t hi_d = sm_pop(st);
                DESCR_t lo_d = sm_pop(st);
                DESCR_t sd   = sm_pop(st);
                if (IS_FAIL_fn(sd) || IS_FAIL_fn(lo_d) || IS_FAIL_fn(hi_d)) {
                    sm_push(st, FAILDESCR); st->last_ok = 0; break;
                }
                const char *s = (sd.v == DT_S || sd.v == DT_SNUL) ? VARVAL_fn(sd) : NULL;
                if (!s) s = "";
                int slen = (int)strlen(s);
                int i = (int)to_int(lo_d);
                int x = (int)to_int(hi_d);
                if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
                int lo, hi;
                if (strcmp(name, "ICN_SECTION_RANGE") == 0) {
                    if (x == 0) x = slen + 1; else if (x < 0) x = slen + 1 + x;
                    if (i < 1 || i > slen+1 || x < 1 || x > slen+1) {
                        sm_push(st, FAILDESCR); st->last_ok = 0; break;
                    }
                    lo = i < x ? i : x;
                    hi = i < x ? x : i;
                } else if (strcmp(name, "ICN_SECTION_PLUS") == 0) {
                    if (i < 1 || i > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                    if (x >= 0) { lo = i;     hi = i + x; }
                    else        { lo = i + x; hi = i;     }
                    if (lo < 1 || hi > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                } else {
                    if (i < 1 || i > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                    if (x >= 0) { lo = i - x; hi = i;     }
                    else        { lo = i;     hi = i - x; }
                    if (lo < 1 || hi > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                }
                int len = hi - lo;
                char *buf = GC_malloc(len + 1);
                memcpy(buf, s + lo - 1, len);
                buf[len] = '\0';
                DESCR_t r = STRVAL(buf);
                sm_push(st, r); st->last_ok = 1;
                break;
            }
            if (name && (strcmp(name, "ICN_SECTION_RANGE_SET") == 0 ||
                         strcmp(name, "ICN_SECTION_PLUS_SET")  == 0 ||
                         strcmp(name, "ICN_SECTION_MINUS_SET") == 0)) {
                DESCR_t hi_d = sm_pop(st); DESCR_t lo_d = sm_pop(st);
                DESCR_t base = sm_pop(st); DESCR_t val  = sm_pop(st);
                if (IS_FAIL_fn(base) || IS_FAIL_fn(lo_d) || IS_FAIL_fn(hi_d)) {
                    sm_push(st, FAILDESCR); st->last_ok = 0; break;
                }
                if (base.v != DT_S && base.v != DT_SNUL) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                const char *s = base.s ? base.s : "";
                int slen = (int)strlen(s);
                int i = (int)to_int(lo_d), x = (int)to_int(hi_d);
                if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
                int lo, hi;
                if (strcmp(name, "ICN_SECTION_RANGE_SET") == 0) {
                    if (x == 0) x = slen + 1; else if (x < 0) x = slen + 1 + x;
                    if (i < 1 || i > slen+1 || x < 1 || x > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                    lo = i < x ? i : x; hi = i < x ? x : i;
                } else if (strcmp(name, "ICN_SECTION_PLUS_SET") == 0) {
                    if (i < 1 || i > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                    if (x >= 0) { lo = i; hi = i + x; } else { lo = i + x; hi = i; }
                    if (lo < 1 || hi > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                } else {
                    if (i < 1 || i > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                    if (x >= 0) { lo = i - x; hi = i; } else { lo = i; hi = i - x; }
                    if (lo < 1 || hi > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                }
                const char *vs = VARVAL_fn(val); if (!vs) vs = "";
                int vlen = (int)strlen(vs);
                int prefix = lo - 1, suffix = slen - (hi - 1);
                int newlen = prefix + vlen + suffix;
                char *buf = GC_malloc(newlen + 1);
                if (prefix > 0) memcpy(buf, s, prefix);
                if (vlen > 0)   memcpy(buf + prefix, vs, vlen);
                if (suffix > 0) memcpy(buf + prefix + vlen, s + hi - 1, suffix);
                buf[newlen] = ' ';
                DESCR_t sv = STRVAL(buf);
                DESCR_t varname = sm_pop(st);
                const char *vname = (varname.v == DT_S && varname.s) ? varname.s : NULL;
                if (vname && icn_frame_env_active()) {
                    int _sl = scope_get(&FRAME.sc, vname);
                    if (_sl >= 0) icn_frame_env_store(_sl, sv);
                    else set_and_trace(vname, sv);
                } else if (vname) set_and_trace(vname, sv);
                sm_push(st, sv); st->last_ok = 1;
                break;
            }
            DESCR_t args[32];
            for (int k = nargs - 1; k >= 0; k--)
                args[k] = sm_pop(st);
            if (name && strcmp(name, "ICN_SCAN_PUSH") == 0 && nargs == 1) {
                const char *s;
                if (IS_REAL_fn(args[0])) { char _rb[64]; real_str(args[0].r,_rb,sizeof _rb); s = GC_strdup(_rb); }
                else { s = VARVAL_fn(args[0]); if (!s) s = ""; }
                if (scan_depth < SCAN_STACK_MAX) {
                    scan_stack[scan_depth].subj = scan_subj;
                    scan_stack[scan_depth].pos  = scan_pos;
                    scan_depth++;
                }
                scan_subj = GC_strdup(s); scan_pos = 1;
                sm_push(st, args[0]);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "ICN_TO_INIT") == 0) {
                icn_to_state_t *z = (icn_to_state_t *)ins->a[2].ptr;
                DESCR_t hi_d = sm_pop(st);
                DESCR_t lo_d = sm_pop(st);
                z->lo = IS_FAIL_fn(lo_d) ? 0 : lo_d.i;
                z->hi = IS_FAIL_fn(hi_d) ? 0 : hi_d.i;
                sm_push(st, NULVCL);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "ICN_TO_BY_INIT") == 0) {
                icn_to_by_state_t *z = (icn_to_by_state_t *)ins->a[2].ptr;
                DESCR_t step_d = sm_pop(st);
                DESCR_t hi_d   = sm_pop(st);
                DESCR_t lo_d   = sm_pop(st);
                z->lo   = IS_FAIL_fn(lo_d)   ? 0 : lo_d.i;
                z->hi   = IS_FAIL_fn(hi_d)   ? 0 : hi_d.i;
                z->step = IS_FAIL_fn(step_d) ? 1 : step_d.i;
                sm_push(st, NULVCL);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "ICN_SCAN_POP") == 0 && nargs == 1) {
                if (scan_depth > 0) {
                    scan_depth--;
                    scan_subj = scan_stack[scan_depth].subj;
                    scan_pos  = scan_stack[scan_depth].pos;
                }
                sm_push(st, args[0]);
                st->last_ok = (args[0].v != DT_FAIL);
                break;
            }
            if (name && strcmp(name, "ICN_KW_SWAP") == 0 && nargs == 6) {
                DESCR_t lv = args[0], rv = args[1];
                const char *kw  = (args[2].v == DT_S && args[2].s) ? args[2].s : NULL;
                const char *var = (args[3].v == DT_S && args[3].s) ? args[3].s : NULL;
                int var_slot    = (int)args[4].i;
                int kw_is_lhs   = (int)args[5].i;
                if (!kw || !var || IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) {
                    sm_push(st, FAILDESCR); st->last_ok = 0; break;
                }
                #define KW_SWAP_WRITE_VAR(val) do { \
                    if (var_slot >= 0 && icn_frame_env_active()) \
                        icn_frame_env_store(var_slot, (val)); \
                    else \
                        NV_SET_fn(var, (val)); \
                } while(0)
                DESCR_t result;
                if (kw_is_lhs) {
                    if (!icn_kw_can_assign(kw + 1, rv)) {
                        sm_push(st, FAILDESCR); st->last_ok = 0; break;
                    }
                    kw_assign(kw + 1, rv);
                    KW_SWAP_WRITE_VAR(lv);
                    result = rv;
                } else {
                    KW_SWAP_WRITE_VAR(rv);
                    if (!icn_kw_can_assign(kw + 1, lv)) {
                        sm_push(st, FAILDESCR); st->last_ok = 0; break;
                    }
                    kw_assign(kw + 1, lv);
                    result = rv;
                }
                #undef KW_SWAP_WRITE_VAR
                sm_push(st, result);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "PL_UNIFY") == 0 && nargs == 0) {
                sm_pop(st);
                fprintf(stderr, "[NO-AST] PL_UNIFY stub: needs fresh SM/BB lowering\n");
                st->last_ok = 0;
                break;
            }
            if (name && strcmp(name, "PL_CUT") == 0 && nargs == 0) {
                sm_pop(st);
                g_pl_cut_flag = 1;
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "PL_TRAIL_MARK") == 0 && nargs == 0) {
                DESCR_t d; d.v = DT_I; d.i = (int64_t)trail_mark(&g_pl_trail); d.ptr = NULL;
                sm_push(st, d);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "PL_TRAIL_UNWIND") == 0 && nargs == 0) {
                DESCR_t mark_d = sm_pop(st);
                trail_unwind(&g_pl_trail, (int)mark_d.i);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "PL_BUILTIN") == 0 && nargs == 0) {
                sm_pop(st);
                fprintf(stderr, "[NO-AST] PL_BUILTIN stub: needs fresh SM/BB lowering\n");
                st->last_ok = 0;
                break;
            }
            for (int k = 0; k < nargs; k++) {
                if (args[k].v == DT_FAIL) {
                    sm_push(st, FAILDESCR);
                    st->last_ok = 0;
                    goto sm_call_done;
                }
            }
            if (name && icn_frame_env_active()) {
                int _pv_slot = scope_get(&FRAME.sc, name);
                if (_pv_slot >= 0) {
                    DESCR_t _pv = icn_frame_env_load(_pv_slot);
                    if (_pv.v == DT_E) {
                        for (int _pi = 0; _pi < proc_count; _pi++) {
                            if (proc_table[_pi].entry_pc == (int)_pv.i) {
                                DESCR_t _pr = sm_call_proc(proc_table[_pi].entry_pc,
                                                            proc_table[_pi].nparams,
                                                            args, nargs);
                                sm_push(st, _pr);
                                st->last_ok = (_pr.v != DT_FAIL);
                                goto sm_call_done;
                            }
                        }
                        if (_pv.slen < (uint32_t)proc_count) {
                            int _pi = (int)_pv.slen;
                            DESCR_t _pr = sm_call_proc(proc_table[_pi].entry_pc,
                                                        proc_table[_pi].nparams,
                                                        args, nargs);
                            sm_push(st, _pr);
                            st->last_ok = (_pr.v != DT_FAIL);
                            goto sm_call_done;
                        }
                    }
                    if (IS_STR_fn(_pv) && _pv.s) {
                        extern int icn_try_call_builtin_by_name(const char *, DESCR_t *, int, DESCR_t *);
                        DESCR_t _br = FAILDESCR;
                        if (icn_try_call_builtin_by_name(_pv.s, args, nargs, &_br)) {
                            sm_push(st, _br);
                            st->last_ok = (_br.v != DT_FAIL);
                            goto sm_call_done;
                        }
                    }
                }
            }
            if (name && g_current_sm_prog) {
                extern int proc_count;
                extern IcnProcEntry proc_table[];
                for (int _pi = 0; _pi < proc_count; _pi++) {
                    if (proc_table[_pi].entry_pc >= 0 &&
                        proc_table[_pi].name &&
                        strcmp(proc_table[_pi].name, name) == 0) {
                        DESCR_t _pr = sm_call_proc(proc_table[_pi].entry_pc,
                                                    proc_table[_pi].nparams,
                                                    args, nargs);
                        sm_push(st, _pr);
                        st->last_ok = (_pr.v != DT_FAIL);
                        goto sm_call_done;
                    }
                }
            }
            {
            DESCR_t result = FAILDESCR;
            int _data_first = (nargs >= 1 && args[0].v == DT_DATA);
            int _data_set   = (nargs >= 2 && args[1].v == DT_DATA && name &&
                               strlen(name) > 4 &&
                               strcasecmp(name + strlen(name) - 4, "_SET") == 0);
            if (_data_first || _data_set)
                result = sc_dat_field_call(name, args, nargs);
            if (result.v == DT_FAIL || (!_data_first && !_data_set)) {
                int body_pc = -1;
                if (!_data_first && !_data_set && name) {
                    body_pc = sm_label_pc_lookup(prog, name);
                    if (body_pc < 0) {
                        char uname[128]; size_t nl = strlen(name);
                        if (nl >= sizeof(uname)) nl = sizeof(uname)-1;
                        for (size_t _i = 0; _i <= nl; _i++)
                            uname[_i] = (char)toupper((unsigned char)name[_i]);
                        body_pc = sm_label_pc_lookup(prog, uname);
                    }
                    if (body_pc < 0) {
                        const char *entry = FUNC_ENTRY_fn(name);
                        if (entry) body_pc = sm_label_pc_lookup(prog, entry);
                    }
                }
                if (body_pc >= 0 && st->call_depth < SM_CALL_STACK_MAX) {
                    SmCallFrame *fr = &st->call_stack[st->call_depth++];
                    fr->ret_pc = st->pc;
                    fr->ret_ok = 1;
                    fr->caller_sp = st->sp;
                    if (st->sp > 0) {
                        fr->caller_stack = GC_malloc(st->sp * sizeof(DESCR_t));
                        memcpy(fr->caller_stack, st->stack, st->sp * sizeof(DESCR_t));
                    } else {
                        fr->caller_stack = NULL;
                    }
                    st->sp = 0;
                    const char *entry2 = FUNC_ENTRY_fn(name);
                    const char *retname = (entry2 && strcmp(entry2, name) != 0
                                           && FNCEX_fn(entry2)) ? entry2 : name;
                    fr->retval_name = GC_strdup(retname);
                    int np = FUNC_NPARAMS_fn(name);
                    int nl2 = FUNC_NLOCALS_fn(name);
                    if (np > 64) np = 64;
                    if (nl2 > 64) nl2 = 64;
                    int ns = 0;
                    if (ns < SM_SAVED_NV_MAX) {
                        fr->saved_names[ns] = GC_strdup(retname);
                        fr->saved_vals [ns] = NV_GET_fn(retname);
                        ns++;
                    }
                    NV_SET_fn(retname, STRVAL(""));
                    for (int k = 0; k < np && ns < SM_SAVED_NV_MAX; k++) {
                        const char *pname = FUNC_PARAM_fn(name, k);
                        if (!pname) pname = "";
                        fr->saved_names[ns] = GC_strdup(pname);
                        fr->saved_vals [ns] = NV_GET_fn(pname);
                        ns++;
                        NV_SET_fn(pname, k < nargs ? args[k] : NULVCL);
                    }
                    for (int k = 0; k < nl2 && ns < SM_SAVED_NV_MAX; k++) {
                        const char *lname = FUNC_LOCAL_fn(name, k);
                        if (!lname) lname = "";
                        fr->saved_names[ns] = GC_strdup(lname);
                        fr->saved_vals [ns] = NV_GET_fn(lname);
                        ns++;
                        NV_SET_fn(lname, NULVCL);
                    }
                    fr->nsaved = ns;
                    comm_call(retname);
                    st->pc = body_pc;
                    goto sm_call_done;
                }
                if (result.v == DT_FAIL || (!_data_first && !_data_set)) {
                    if (name) {
                        extern int icn_try_call_builtin_by_name(
                            const char *fn, DESCR_t *args, int nargs, DESCR_t *out);
                        DESCR_t icn_out;
                        if (icn_try_call_builtin_by_name(name, args, nargs, &icn_out)) {
                            result = icn_out;
                            goto sm_call_invoke_done;
                        }
                    }
                    result = INVOKE_fn(name, args, nargs);
                    sm_call_invoke_done: ;
                }
            }
            if (IS_NAMEPTR(result))      result = NAME_DEREF_PTR(result);
            else if (IS_NAMEVAL(result)) result = NV_GET_fn(result.s);
            sm_push(st, result);
            st->last_ok = (result.v != DT_FAIL);
            }
            sm_call_done:
            break;
        }
        case SM_RETURN:
        case SM_FRETURN:
        case SM_NRETURN:
        case SM_RETURN_S:  case SM_RETURN_F:
        case SM_FRETURN_S: case SM_FRETURN_F:
        case SM_NRETURN_S: case SM_NRETURN_F: {
            int cond_s = (ins->op == SM_RETURN_S || ins->op == SM_FRETURN_S || ins->op == SM_NRETURN_S);
            int cond_f = (ins->op == SM_RETURN_F || ins->op == SM_FRETURN_F || ins->op == SM_NRETURN_F);
            if ((cond_s && !st->last_ok) || (cond_f && st->last_ok)) break;
            if (st->call_depth > 0) {
                SmCallFrame *fr = &st->call_stack[--st->call_depth];
                int is_fret  = (ins->op == SM_FRETURN  || ins->op == SM_FRETURN_S || ins->op == SM_FRETURN_F);
                int is_nret  = (ins->op == SM_NRETURN  || ins->op == SM_NRETURN_S || ins->op == SM_NRETURN_F);
                extern int g_lang;
                DESCR_t retval = (fr->retval_name)
                    ? NV_GET_fn(fr->retval_name)
                    : ((st->sp > 0) ? st->stack[st->sp - 1]
                       : (g_lang == LANG_ICN ? NULVCL : FAILDESCR));
                for (int k = fr->nsaved - 1; k >= 0; k--)
                    NV_SET_fn(fr->saved_names[k], fr->saved_vals[k]);
                if (fr->caller_sp > 0 && fr->caller_stack) {
                    if (fr->caller_sp > st->stack_cap) {
                        st->stack = GC_realloc(st->stack, fr->caller_sp * sizeof(DESCR_t));
                        st->stack_cap = fr->caller_sp;
                    }
                    memcpy(st->stack, fr->caller_stack, fr->caller_sp * sizeof(DESCR_t));
                }
                st->sp = fr->caller_sp;
                if (is_fret) {
                    sm_push(st, FAILDESCR);
                    st->last_ok = 0;
                    strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1);
                } else if (is_nret) {
                    DESCR_t deref = retval;
                    if (IS_NAMEPTR(deref))      deref = NAME_DEREF_PTR(deref);
                    else if (IS_NAMEVAL(deref)) deref = NV_GET_fn(deref.s);
                    sm_push(st, deref);
                    st->last_ok = 1;
                    strncpy(kw_rtntype, "NRETURN", sizeof(kw_rtntype)-1);
                } else {
                    sm_push(st, retval);
                    st->last_ok = (retval.v != DT_FAIL);
                    strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1);
                }
                st->pc = fr->ret_pc;
            } else {
                int is_fret = (ins->op == SM_FRETURN  || ins->op == SM_FRETURN_S || ins->op == SM_FRETURN_F);
                int is_nret = (ins->op == SM_NRETURN  || ins->op == SM_NRETURN_S || ins->op == SM_NRETURN_F);
                if (is_fret)      strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1);
                else if (is_nret) strncpy(kw_rtntype, "NRETURN", sizeof(kw_rtntype)-1);
                else              strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1);
                return 0;
            }
            break;
        }
        case SM_DEFINE:
            break;
        case SM_INCR: {
            DESCR_t v = sm_pop(st);
            sm_push(st, INTVAL(v.i + ins->a[0].i));
            break;
        }
        case SM_DECR: {
            DESCR_t v = sm_pop(st);
            sm_push(st, INTVAL(v.i - ins->a[0].i));
            break;
        }
        case SM_ACOMP: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            if (l.v == DT_FAIL || r.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            if (l.v == DT_SNUL) l = INTVAL(0);
            if (r.v == DT_SNUL) r = INTVAL(0);
            double lv = (l.v == DT_R) ? l.r : (double)((l.v == DT_I) ? l.i : 0);
            double rv = (r.v == DT_R) ? r.r : (double)((r.v == DT_I) ? r.i : 0);
            int op = (int)ins->a[0].i;
            int ok;
            switch (op) {
                case TT_EQ: ok = (lv == rv); break;
                case TT_NE: ok = (lv != rv); break;
                case TT_LT: ok = (lv <  rv); break;
                case TT_LE: ok = (lv <= rv); break;
                case TT_GT: ok = (lv >  rv); break;
                case TT_GE: ok = (lv >= rv); break;
                default:   ok = (lv == rv); break;
            }
            if (ok) {
                sm_push(st, r);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }
        case SM_LCOMP: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            if (l.v == DT_FAIL || r.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            const char *ls = VARVAL_fn(l); if (!ls) ls = "";
            const char *rs = VARVAL_fn(r); if (!rs) rs = "";
            int cmp = strcmp(ls, rs);
            int op = (int)ins->a[0].i;
            int ok;
            switch (op) {
                case TT_LLT: ok = (cmp <  0); break;
                case TT_LLE: ok = (cmp <= 0); break;
                case TT_LGT: ok = (cmp >  0); break;
                case TT_LGE: ok = (cmp >= 0); break;
                case TT_LEQ: ok = (cmp == 0); break;
                case TT_LNE: ok = (cmp != 0); break;
                default:    ok = (cmp == 0); break;
            }
            if (ok) {
                sm_push(st, r);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }
        case SM_SUSPEND: {
            DESCR_t yielded = (st->sp > 0) ? sm_pop(st) : FAILDESCR;
            if (g_current_generator_state) {
                GeneratorState *gs = g_current_generator_state;
                gs->yielded    = yielded;
                gs->resume_pc  = st->pc;
                gs->last_ok    = st->last_ok;
                if (st->sp > 0) {
                    if (st->sp > gs->stack_cap) {
                        gs->stack     = GC_realloc(gs->stack, st->sp * sizeof(DESCR_t));
                        gs->stack_cap = st->sp;
                    }
                    memcpy(gs->stack, st->stack, st->sp * sizeof(DESCR_t));
                }
                gs->sp = st->sp;
                return SM_INTERP_SUSPENDED;
            }
            sm_push(st, FAILDESCR);
            break;
        }
        case SM_LOAD_GLOCAL: {
            int slot = (int)ins->a[0].i;
            if (g_current_generator_state && slot >= 0 && slot < SM_GEN_LOCAL_MAX) {
                sm_push(st, g_current_generator_state->locals[slot]);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }
        case SM_STORE_GLOCAL: {
            int slot = (int)ins->a[0].i;
            DESCR_t v = sm_pop(st);
            if (g_current_generator_state && slot >= 0 && slot < SM_GEN_LOCAL_MAX) {
                g_current_generator_state->locals[slot] = v;
                sm_push(st, v);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }
        case SM_ICMP_GT: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            st->last_ok = (l.i > r.i);
            break;
        }
        case SM_ICMP_LT: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            st->last_ok = (l.i < r.i);
            break;
        }
        case SM_LOAD_FRAME: {
            int slot = (int)ins->a[0].i;
            if (icn_frame_env_active() && slot >= 0) {
                sm_push(st, icn_frame_env_load(slot));
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }
        case SM_STORE_FRAME: {
            int slot = (int)ins->a[0].i;
            DESCR_t v = sm_pop(st);
            if (v.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            if (icn_frame_env_active() && slot >= 0) {
                icn_frame_env_store(slot, v);
                sm_push(st, v);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }
        default:
            fprintf(stderr, "sm_interp: unhandled opcode %d (%s) at pc=%d\n",
                    (int)ins->op, sm_opcode_name(ins->op), st->pc - 1);
            return -1;
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_interp_run_steps(SM_Program *prog, SM_State *st, int n) {
    g_sm_step_limit = n;
    g_sm_steps_done = 0;
    int rc = 0;
    if (setjmp(g_sm_step_jmp) == 0)
        rc = sm_interp_run(prog, st);
    g_sm_step_limit = 0;
    g_sm_steps_done = 0;
    return rc;
}
extern const char *Σ;
extern int         Ω;
extern int         Δ;
extern jmp_buf     g_sno_err_jmp;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sm_call_expression(int entry_pc)
{
    SM_Program *prog = g_current_sm_prog;
    if (!prog || entry_pc < 0 || entry_pc >= prog->count) {
        fprintf(stderr, "sm_call_expression: invalid entry_pc %d\n", entry_pc);
        return FAILDESCR;
    }
    NAME_ctx_t chunk_ctx;
    NAME_ctx_enter(&chunk_ctx);
    const char *save_Σ = Σ;
    int         save_Ω = Ω;
    int         save_Δ = Δ;
    jmp_buf saved_err_jmp;
    memcpy(&saved_err_jmp, &g_sno_err_jmp, sizeof(jmp_buf));
    DESCR_t result = FAILDESCR;
    if (setjmp(g_sno_err_jmp) == 0) {
        SM_State *nested = GC_malloc(sizeof(SM_State));
        sm_state_init(nested);
        nested->pc = entry_pc;
        kw_rtntype[0] = '\0';
        sm_interp_run(prog, nested);
        if (strncmp(kw_rtntype, "FRETURN", 7) == 0) result = FAILDESCR;
        else if (nested->sp > 0) result = nested->stack[nested->sp - 1];
    }
    memcpy(&g_sno_err_jmp, &saved_err_jmp, sizeof(jmp_buf));
    Σ = save_Σ; Ω = save_Ω; Δ = save_Δ;
    NAME_ctx_leave();
    return result;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
GeneratorState *generator_state_new(int entry_pc)
{
    GeneratorState *gs = GC_malloc(sizeof(GeneratorState));
    memset(gs, 0, sizeof *gs);
    gs->entry_pc         = entry_pc;
    gs->resume_pc        = entry_pc;
    gs->started          = 0;
    gs->yielded          = FAILDESCR;
    gs->stack            = NULL;
    gs->sp               = 0;
    gs->stack_cap        = 0;
    gs->last_ok          = 1;
    gs->saved_frame_depth = 0;
    return gs;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
GeneratorState *generator_state_new_proc(int pi, DESCR_t *args, int nargs)
{
    if (pi < 0 || pi >= proc_count) return NULL;
    int entry_pc = proc_table[pi].entry_pc;
    if (entry_pc < 0 || !g_current_sm_prog) return NULL;
    if (frame_depth >= FRAME_STACK_MAX) return NULL;
    IcnFrame *f = &frame_stack[frame_depth++];
    memset(f, 0, sizeof *f);
    int nparams = proc_table[pi].nparams;
    int nslots = (nparams > 0) ? nparams : 1;
    if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;
    f->env_n = nslots;
    for (int i = 0; i < nparams && i < nargs && i < FRAME_SLOT_MAX; i++) f->env[i] = args[i];
    fprintf(stderr, "[NO-AST] sm_call_proc tree walk removed: scope must be prebuilt at lower time\n");
    f->sc = proc_table[pi].lower_sc;
    int total_slots = proc_table[pi].lower_sc.n;
    if (total_slots > f->env_n) f->env_n = total_slots;
    GeneratorState *gs = GC_malloc(sizeof(GeneratorState));
    memset(gs, 0, sizeof *gs);
    gs->entry_pc          = entry_pc;
    gs->resume_pc         = entry_pc;
    gs->started           = 0;
    gs->yielded           = FAILDESCR;
    gs->stack             = NULL;
    gs->sp                = 0;
    gs->stack_cap         = 0;
    gs->last_ok           = 1;
    gs->saved_frame_depth = frame_depth;
    return gs;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int bb_broker_drive_sm(GeneratorState *gs, void (*body_fn)(DESCR_t val, void *arg), void *arg)
{
    SM_Program *prog = g_current_sm_prog;
    if (!prog || !gs || gs->started == 2) return 0;
    int ticks = 0;
    for (;;) {
        SM_State *st = GC_malloc(sizeof(SM_State));
        sm_state_init(st);
        st->pc      = gs->resume_pc;
        st->last_ok = gs->last_ok;
        if (gs->sp > 0 && gs->stack) {
            if (gs->sp > st->stack_cap) {
                st->stack     = GC_realloc(st->stack, gs->sp * sizeof(DESCR_t));
                st->stack_cap = gs->sp;
            }
            memcpy(st->stack, gs->stack, gs->sp * sizeof(DESCR_t));
            st->sp = gs->sp;
        }
        GeneratorState *outer_gs = g_current_generator_state;
        g_current_generator_state  = gs;
        gs->started = 1;
        int rc = sm_interp_run(prog, st);
        g_current_generator_state = outer_gs;
        if (rc == SM_INTERP_SUSPENDED) {
            ticks++;
            if (body_fn) body_fn(gs->yielded, arg);
        } else {
            gs->started = 2;
            break;
        }
    }
    return ticks;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int bb_broker_drive_sm_one(GeneratorState *gs, DESCR_t *out)
{
    SM_Program *prog = g_current_sm_prog;
    if (!prog || !gs || gs->started == 2) { *out = FAILDESCR; return 0; }
    if (gs->saved_frame_depth > 0) frame_depth = gs->saved_frame_depth;
    SM_State *st = GC_malloc(sizeof(SM_State));
    sm_state_init(st);
    st->pc      = gs->resume_pc;
    st->last_ok = gs->last_ok;
    if (gs->sp > 0 && gs->stack) {
        if (gs->sp > st->stack_cap) {
            st->stack     = GC_realloc(st->stack, gs->sp * sizeof(DESCR_t));
            st->stack_cap = gs->sp;
        }
        memcpy(st->stack, gs->stack, gs->sp * sizeof(DESCR_t));
        st->sp = gs->sp;
    }
    GeneratorState *outer_gs = g_current_generator_state;
    g_current_generator_state  = gs;
    gs->started = 1;
    int rc = sm_interp_run(prog, st);
    g_current_generator_state = outer_gs;
    if (rc == SM_INTERP_SUSPENDED) {
        *out = gs->yielded;
        return 1;
    } else {
        gs->started = 2;
        if (gs->saved_frame_depth > 0) frame_depth = gs->saved_frame_depth - 1;
        *out = FAILDESCR;
        return 0;
    }
}
