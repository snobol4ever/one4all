/*
 * emitter_sm.c — SM opcode emitters (renamed from sm_templates.c, EM-UNIFY-a)
 *
 * EM-UNIFY-c: nullary and single-arg rt-call families collapsed to tables.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-UNIFY / GOAL-MODE4-EMIT
 */

#include "emitter.h"
#include "bb_emit.h"
#include "templates.h"
#include "sm_prog.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ── nullary rt-call body ─────────────────────────────────────────────────── */

static void emit_sm_nullary_rt(const char *macro_name, const char *rt_fn)
{
    emit_macro_begin(macro_name, NULL, 0);
    emit_call_sym_plt(rt_fn, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}

/* ── nullary opcode table ────────────────────────────────────────────────── */

typedef struct { int op; const char *macro_name; const char *rt_fn; } sm_nullary_entry_t;

static const sm_nullary_entry_t g_sm_nullary[] = {
    { SM_VOID_POP,        "VOID_POP",          "rt_pop_void"          },
    { SM_CONCAT,          "CONCAT",            "rt_concat"            },
    { SM_PUSH_NULL,       "PUSH_NULL",         "rt_push_null"         },
    { SM_PUSH_NULL_NOFLIP,"PUSH_NULL_NOFLIP",  "rt_push_null_noflip"  },
    { SM_COERCE_NUM,      "COERCE_NUM",        "rt_coerce_num"        },
    { SM_EXP,             "EXP_NUM",           "rt_exp"               },
    { SM_NEG,             "NEGATE",            "rt_neg"               },
    { SM_DEFINE,          "DEFINE",            "rt_define"            },
    { SM_DEFINE_ENTRY,    "DEFINE_ENTRY",      "rt_define_entry"      },
    { SM_PAT_EPS,         "PAT_EPS",           "rt_pat_eps"           },
    { SM_PAT_ARB,         "PAT_ARB",           "rt_pat_arb"           },
    { SM_PAT_REM,         "PAT_REM",           "rt_pat_rem"           },
    { SM_PAT_FAIL,        "PAT_FAIL",          "rt_pat_fail"          },
    { SM_PAT_SUCCEED,     "PAT_SUCCEED",       "rt_pat_succeed"       },
    { SM_PAT_ABORT,       "PAT_ABORT",         "rt_pat_abort"         },
    { SM_PAT_BAL,         "PAT_BAL",           "rt_pat_bal"           },
    { SM_PAT_FENCE0,      "PAT_FENCE",         "rt_pat_fence"         },
    { SM_PAT_FENCE1,      "PAT_FENCE1",        "rt_pat_fence1"        },
    { SM_PAT_SPAN,        "PAT_SPAN",          "rt_pat_span"          },
    { SM_PAT_BREAK,       "PAT_BREAK",         "rt_pat_break"         },
    { SM_PAT_ANY,         "PAT_ANY",           "rt_pat_any"           },
    { SM_PAT_NOTANY,      "PAT_NOTANY",        "rt_pat_notany"        },
    { SM_PAT_LEN,         "PAT_LEN",           "rt_pat_len"           },
    { SM_PAT_POS,         "PAT_POS",           "rt_pat_pos"           },
    { SM_PAT_RPOS,        "PAT_RPOS",          "rt_pat_rpos"          },
    { SM_PAT_TAB,         "PAT_TAB",           "rt_pat_tab"           },
    { SM_PAT_RTAB,        "PAT_RTAB",          "rt_pat_rtab"          },
    { SM_PAT_ARBNO,       "PAT_ARBNO",         "rt_pat_arbno"         },
    { SM_PAT_CAT,         "PAT_CAT",           "rt_pat_cat"           },
    { SM_PAT_ALT,         "PAT_ALT",           "rt_pat_alt"           },
    { SM_PAT_DEREF,       "PAT_DEREF",         "rt_pat_deref"         },
    /* M5 stubs */
    { SM_RESUME,          "RESUME",            "rt_unhandled_sm"      },
    { SM_SUSPEND,         "SUSPEND",           "rt_unhandled_sm"      },
    { SM_SUSPEND_VALUE,   "SUSPEND_VALUE",     "rt_unhandled_sm"      },
    { SM_GEN_TICK,        "GEN_TICK",          "rt_unhandled_sm"      },
    { SM_LOAD_GLOCAL,     "LOAD_GLOCAL",       "rt_unhandled_sm"      },
    { SM_STORE_GLOCAL,    "STORE_GLOCAL",      "rt_unhandled_sm"      },
    { SM_LOAD_FRAME,      "LOAD_FRAME",        "rt_unhandled_sm"      },
    { SM_STORE_FRAME,     "STORE_FRAME",       "rt_unhandled_sm"      },
    { SM_ICMP_GT,         "ICMP_GT",           "rt_unhandled_sm"      },
    { SM_ICMP_LT,         "ICMP_LT",           "rt_unhandled_sm"      },
    { SM_BB_ONCE,         "BB_ONCE",           "rt_unhandled_sm"      },
    { SM_BB_ONCE_PROC,    "BB_ONCE_PROC",      "rt_unhandled_sm"      },
    { SM_BB_PUMP,         "BB_PUMP",           "rt_unhandled_sm"      },
    { SM_BB_PUMP_CASE,    "BB_PUMP_CASE",      "rt_unhandled_sm"      },
    { SM_BB_PUMP_EVERY,   "BB_PUMP_EVERY",     "rt_unhandled_sm"      },
    { SM_BB_PUMP_PROC,    "BB_PUMP_PROC",      "rt_unhandled_sm"      },
    { SM_BB_PUMP_SM,      "BB_PUMP_SM",        "rt_unhandled_sm"      },
    { -1, NULL, NULL }
};

void emit_sm_op(int op)
{
    for (const sm_nullary_entry_t *e = g_sm_nullary; e->op >= 0; e++)
        if (e->op == op) { emit_sm_nullary_rt(e->macro_name, e->rt_fn); return; }
    emit_sm_unhandled_op(op);
}

/* Named wrappers — public API unchanged */
void emit_sm_coerce_num()     { emit_sm_op(SM_COERCE_NUM); }
void emit_sm_exp()            { emit_sm_op(SM_EXP); }
void emit_sm_neg()            { emit_sm_op(SM_NEG); }
void emit_sm_define()         { emit_sm_op(SM_DEFINE); }
void emit_sm_define_entry()   { emit_sm_op(SM_DEFINE_ENTRY); }
void emit_sm_pat_eps()        { emit_sm_op(SM_PAT_EPS); }
void emit_sm_pat_arb()        { emit_sm_op(SM_PAT_ARB); }
void emit_sm_pat_rem()        { emit_sm_op(SM_PAT_REM); }
void emit_sm_pat_fail()       { emit_sm_op(SM_PAT_FAIL); }
void emit_sm_pat_succeed()    { emit_sm_op(SM_PAT_SUCCEED); }
void emit_sm_pat_abort()      { emit_sm_op(SM_PAT_ABORT); }
void emit_sm_pat_bal()        { emit_sm_op(SM_PAT_BAL); }
void emit_sm_pat_fence()      { emit_sm_op(SM_PAT_FENCE0); }
void emit_sm_pat_fence1()     { emit_sm_op(SM_PAT_FENCE1); }
void emit_sm_pat_span()       { emit_sm_op(SM_PAT_SPAN); }
void emit_sm_pat_break()      { emit_sm_op(SM_PAT_BREAK); }
void emit_sm_pat_any()        { emit_sm_op(SM_PAT_ANY); }
void emit_sm_pat_notany()     { emit_sm_op(SM_PAT_NOTANY); }
void emit_sm_pat_len()        { emit_sm_op(SM_PAT_LEN); }
void emit_sm_pat_pos()        { emit_sm_op(SM_PAT_POS); }
void emit_sm_pat_rpos()       { emit_sm_op(SM_PAT_RPOS); }
void emit_sm_pat_tab()        { emit_sm_op(SM_PAT_TAB); }
void emit_sm_pat_rtab()       { emit_sm_op(SM_PAT_RTAB); }
void emit_sm_pat_arbno()      { emit_sm_op(SM_PAT_ARBNO); }
void emit_sm_pat_cat()        { emit_sm_op(SM_PAT_CAT); }
void emit_sm_pat_alt()        { emit_sm_op(SM_PAT_ALT); }
void emit_sm_pat_deref()      { emit_sm_op(SM_PAT_DEREF); }
void emit_sm_resume()         { emit_sm_op(SM_RESUME); }
void emit_sm_suspend()        { emit_sm_op(SM_SUSPEND); }
void emit_sm_suspend_value()  { emit_sm_op(SM_SUSPEND_VALUE); }
void emit_sm_gen_tick()       { emit_sm_op(SM_GEN_TICK); }
void emit_sm_load_glocal()    { emit_sm_op(SM_LOAD_GLOCAL); }
void emit_sm_store_glocal()   { emit_sm_op(SM_STORE_GLOCAL); }
void emit_sm_load_frame()     { emit_sm_op(SM_LOAD_FRAME); }
void emit_sm_store_frame()    { emit_sm_op(SM_STORE_FRAME); }
void emit_sm_icmp_gt()        { emit_sm_op(SM_ICMP_GT); }
void emit_sm_icmp_lt()        { emit_sm_op(SM_ICMP_LT); }
void emit_sm_bb_once()        { emit_sm_op(SM_BB_ONCE); }
void emit_sm_bb_once_proc()   { emit_sm_op(SM_BB_ONCE_PROC); }
void emit_sm_bb_pump()        { emit_sm_op(SM_BB_PUMP); }
void emit_sm_bb_pump_case()   { emit_sm_op(SM_BB_PUMP_CASE); }
void emit_sm_bb_pump_every()  { emit_sm_op(SM_BB_PUMP_EVERY); }
void emit_sm_bb_pump_proc()   { emit_sm_op(SM_BB_PUMP_PROC); }
void emit_sm_bb_pump_sm()     { emit_sm_op(SM_BB_PUMP_SM); }
void emit_sm_bb_pump_ast() { emit_sm_nullary_rt("BB_PUMP_AST", "rt_bb_pump_ast"); }

/* ── arith family — collapsed to emit_sm_arith(op) ──────────────────────── */

static const struct { int op; const char *mn; } g_sm_arith[] = {
    { SM_ADD,"ADD_NUM" },{ SM_SUB,"SUB_NUM" },{ SM_MUL,"MUL_NUM" },
    { SM_DIV,"DIV_NUM" },{ SM_MOD,"MOD_NUM" },{ -1,NULL }
};

void emit_sm_arith_dispatch(int op)
{
    for (int i = 0; g_sm_arith[i].op >= 0; i++)
        if (g_sm_arith[i].op == op) { emit_sm_arith_op(op, g_sm_arith[i].mn); return; }
    emit_sm_unhandled_op(op);
}

void emit_sm_arith_op(int op_enum, const char *macro_name)
{
    emit_macro_begin(macro_name ? macro_name : "ARITH", NULL, 0);
    emit_mov_rdi_imm64((uint64_t)(unsigned)op_enum);
    emit_call_sym_plt("rt_arith", 0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_add() { emit_sm_arith_dispatch(SM_ADD); }
void emit_sm_sub() { emit_sm_arith_dispatch(SM_SUB); }
void emit_sm_mul() { emit_sm_arith_dispatch(SM_MUL); }
void emit_sm_div() { emit_sm_arith_dispatch(SM_DIV); }
void emit_sm_mod() { emit_sm_arith_dispatch(SM_MOD); }

/* ── single int-arg — collapsed to emit_sm_int_arg ──────────────────────── */

static void emit_sm_int_arg(const char *mn, const char *rt_fn,
                              const char *param_name, int val)
{
    const char *const params[] = { param_name };
    emit_macro_begin(mn, params, 1);
    emit_mov_edi_imm32(val);
    emit_call_sym_plt(rt_fn, 0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_acomp(int op)       { emit_sm_int_arg("ACOMP",    "rt_acomp",       "op", op); }
void emit_sm_lcomp(int op)       { emit_sm_int_arg("LCOMP",    "rt_lcomp",       "op", op); }
void emit_sm_unhandled_op(int op){ emit_sm_int_arg("UNHANDLED","rt_unhandled_op","op", op); }

void emit_sm_incr(int64_t n)
{
    const char *const params[] = { "n" };
    emit_macro_begin("INCR", params, 1);
    emit_mov_rdi_imm64((uint64_t)n);
    emit_call_sym_plt("rt_incr", 0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_decr(int64_t n)
{
    const char *const params[] = { "n" };
    emit_macro_begin("DECR", params, 1);
    emit_mov_rdi_imm64((uint64_t)n);
    emit_call_sym_plt("rt_decr", 0);
    emit_macro_end(); emit_pad_to_blob_size();
}

/* ── halt / return / jump ────────────────────────────────────────────────── */

void emit_sm_halt()
{ emit_bb_inc_mem_r13_disp8(20); emit_ret(); emit_pad_to_blob_size(); }

void emit_sm_return()
{ emit_macro_begin("RETURN",NULL,0); emit_ret(); emit_macro_end(); emit_pad_to_blob_size(); }

void emit_sm_return_variant(int kind, int cond, int pc)
{
    static const char *const params[] = { "kind","cond","pc" };
    emit_macro_begin("RETURN_VARIANT", params, 3);
    emit_mov_edi_imm32(kind); emit_mov_esi_imm32(cond);
    emit_call_sym_plt("rt_do_return", 0);
    emit_test_eax_eax(); emit_jz_retskip(pc);
    emit_ret(); emit_retskip_label(pc);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_freturn  (int pc) { emit_sm_return_variant(1,0,pc); }
void emit_sm_nreturn  (int pc) { emit_sm_return_variant(2,0,pc); }
void emit_sm_return_s (int pc) { emit_sm_return_variant(0,1,pc); }
void emit_sm_return_f (int pc) { emit_sm_return_variant(0,2,pc); }
void emit_sm_freturn_s(int pc) { emit_sm_return_variant(1,1,pc); }
void emit_sm_freturn_f(int pc) { emit_sm_return_variant(1,2,pc); }
void emit_sm_nreturn_s(int pc) { emit_sm_return_variant(2,1,pc); }
void emit_sm_nreturn_f(int pc) { emit_sm_return_variant(2,2,pc); }

static void make_pc_label(bb_label_t *lbl, int pc) { bb_label_initf(lbl,".L%d",pc); }

void emit_sm_jump(int pc)
{ bb_label_t t; make_pc_label(&t,pc); emit_jmp(&t,JMP_JMP); }

void emit_sm_jump_s(int pc)
{ emit_call_sym_plt("rt_last_ok",0); emit_test_rax_rax();
  bb_label_t t; make_pc_label(&t,pc); emit_jmp(&t,JMP_JNE); }

void emit_sm_jump_f(int pc)
{ emit_call_sym_plt("rt_last_ok",0); emit_test_rax_rax();
  bb_label_t t; make_pc_label(&t,pc); emit_jmp(&t,JMP_JE); }

/* ── structural markers ──────────────────────────────────────────────────── */

void emit_sm_label()                                      { emit_noop_macro("LABEL"); }
void emit_sm_stno(int stno, int lineno, const char *src) { emit_banner_stno(stno,lineno,src); emit_noop_macro("STNO"); }

/* ── push literals ───────────────────────────────────────────────────────── */

void emit_sm_push_lit_i(int64_t val)
{
    static const char *const p[] = {"val"};
    emit_macro_begin("PUSH_INT",p,1);
    emit_mov_rdi_imm64((uint64_t)val);
    emit_call_sym_plt("rt_push_int",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_push_lit_f(double val)
{
    uint64_t bits; __builtin_memcpy(&bits,&val,8);
    char bs[32]; snprintf(bs,sizeof(bs),"0x%016llx",(unsigned long long)bits);
    const char *p[] = {bs};
    emit_macro_begin("PUSH_REAL",p,1);
    emit_mov_rdi_imm64(bits);
    emit_call_sym_plt("rt_push_real_bits",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_push_lit_s(const char *str_lbl, uint64_t str_ptr, int len)
{
    static const char *const p[] = {"lbl","n"};
    emit_macro_begin("PUSH_STR",p,2);
    emit_lea_rdi_strtab_sym(str_lbl,str_ptr);
    emit_mov_esi_imm32(len);
    emit_call_sym_plt("rt_push_str",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_push_expr(uint64_t ptr_val)
{
    static const char *const p[] = {"ptr"};
    emit_macro_begin("PUSH_EXPR",p,1);
    emit_mov_rdi_imm64(ptr_val);
    emit_call_sym_plt("rt_push_expr",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

/* ── expression / statement ──────────────────────────────────────────────── */

void emit_sm_push_expression(uint64_t entry_ptr, int arity)
{
    static const char *const p[] = {"entry","arity"};
    emit_macro_begin("PUSH_EXPRESSION",p,2);
    emit_movabs_rdi_entry(entry_ptr); emit_mov_esi_imm32(arity);
    emit_call_sym_plt("rt_push_expression_descr",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_call_expression(const char *tgt_sym)
{
    static const char *const p[] = {"tgt"};
    emit_macro_begin("CALL_EXPRESSION",p,1);
    emit_call_sym_param(tgt_sym);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_exec_stmt(const char *subj_lbl, uint64_t subj_ptr, int has_repl)
{
    static const char *const p[] = {"has_repl","subj_lbl"};
    emit_macro_begin("EXEC_STMT_VARIANT",p,2);
    emit_lea_rdi_strtab_sym(subj_lbl,subj_ptr); emit_mov_esi_imm32(has_repl);
    emit_call_sym_plt("rt_match_variant",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_call_fn(const char *name_lbl, uint64_t name_ptr, int nargs)
{
    (void)name_lbl; (void)name_ptr; (void)nargs;
    static const char *const p[] = {"lbl","n"};
    emit_macro_begin("CALL_FN",p,2);
    emit_lea_rdi_strtab_sym(NULL,0); emit_mov_esi_imm32(0);
    emit_call_sym_plt("rt_call",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

/* ── var / store ─────────────────────────────────────────────────────────── */

void emit_sm_push_var(const char *lbl, uint64_t ptr)
{
    static const char *const p[] = {"lbl"};
    emit_macro_begin("PUSH_VAR",p,1); emit_lea_rdi_strtab_sym(lbl,ptr);
    emit_call_sym_plt("rt_nv_get",0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_store_var(const char *lbl, uint64_t ptr)
{
    static const char *const p[] = {"lbl"};
    emit_macro_begin("STORE_VAR",p,1); emit_lea_rdi_strtab_sym(lbl,ptr);
    emit_call_sym_plt("rt_nv_set",0); emit_macro_end(); emit_pad_to_blob_size();
}

/* ── pat string-arg — collapsed to emit_sm_pat_str ──────────────────────── */

static void emit_sm_pat_str(const char *mn, const char *rt_fn,
                             const char *lbl, uint64_t ptr)
{
    static const char *const p[] = {"lbl"};
    emit_macro_begin(mn,p,1); emit_lea_rdi_strtab_sym(lbl,ptr);
    emit_call_sym_plt(rt_fn,0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_pat_lit     (const char *l, uint64_t p) { emit_sm_pat_str("PAT_LIT",      "rt_pat_lit",      l,p); }
void emit_sm_pat_refname (const char *l, uint64_t p) { emit_sm_pat_str("PAT_REFNAME",  "rt_pat_refname",  l,p); }
void emit_sm_pat_usercall(const char *l, uint64_t p) { emit_sm_pat_str("PAT_USERCALL", "rt_pat_usercall", l,p); }

/* ── pat str+int ─────────────────────────────────────────────────────────── */

void emit_sm_pat_capture(const char *name_lbl, uint64_t name_ptr, int kind)
{
    static const char *const p[] = {"lbl","n"};
    emit_macro_begin("PAT_CAPTURE",p,2);
    emit_lea_rdi_strtab_sym(name_lbl,name_ptr); emit_mov_esi_imm32(kind);
    emit_call_sym_plt("rt_pat_capture",0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_pat_usercall_args(const char *name_lbl, uint64_t name_ptr, int nargs)
{
    static const char *const p[] = {"lbl","n"};
    emit_macro_begin("PAT_USERCALL_ARGS",p,2);
    emit_lea_rdi_strtab_sym(name_lbl,name_ptr); emit_mov_esi_imm32(nargs);
    emit_call_sym_plt("rt_pat_usercall_args",0); emit_macro_end(); emit_pad_to_blob_size();
}

/* ── pat three-arg ───────────────────────────────────────────────────────── */

void emit_sm_pat_capture_fn(const char *fname_lbl, uint64_t fname_ptr,
                             int is_imm, const char *namelist_lbl, uint64_t namelist_ptr)
{
    static const char *const p[] = {"is_imm","fname_lbl","namelist_lbl"};
    emit_macro_begin("PAT_CAPTURE_FN",p,3);
    emit_lea_rdi_strtab_sym(fname_lbl,fname_ptr); emit_mov_esi_imm32(is_imm);
    emit_lea_rdx_strtab_sym(namelist_lbl,namelist_ptr);
    emit_call_sym_plt("rt_pat_capture_fn",0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_pat_capture_fn_args(const char *fname_lbl, uint64_t fname_ptr,
                                  int is_imm, int nargs)
{
    static const char *const p[] = {"is_imm","nargs","fname_lbl"};
    emit_macro_begin("PAT_CAPTURE_FN_ARGS",p,3);
    emit_lea_rdi_strtab_sym(fname_lbl,fname_ptr); emit_mov_esi_imm32(is_imm);
    emit_mov_edx_imm32(nargs);
    emit_call_sym_plt("rt_pat_capture_fn_args",0); emit_macro_end(); emit_pad_to_blob_size();
}
