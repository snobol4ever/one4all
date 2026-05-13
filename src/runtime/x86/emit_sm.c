#include "emit_sm.h"
#include "emit_templates.h"
#include "emit_form.h"
#include "../rt/rt.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include "emit_templates.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static void emit_sm_nullary_rt(const char *macro_name, const char *rt_fn)
{
    emit_macro_begin(macro_name, NULL, 0);
    emit_call_sym_plt(rt_fn, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}

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
        if (e->op == op)
            emit_sm_nullary_rt(e->macro_name, e->rt_fn);
        else
            emit_sm_unhandled_op(op);
}

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
static const struct { int op; const char *mn; } g_sm_arith[] = {
    { SM_ADD,"ADD_NUM" },{ SM_SUB,"SUB_NUM" },{ SM_MUL,"MUL_NUM" },
    { SM_DIV,"DIV_NUM" },{ SM_MOD,"MOD_NUM" },{ -1,NULL }
};

void emit_sm_arith_dispatch(int op)
{
    for (int i = 0; g_sm_arith[i].op >= 0; i++)
        if (g_sm_arith[i].op == op)
            emit_sm_arith_op(op, g_sm_arith[i].mn);
        else
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

static void emit_sm_int_arg(const char *mn, const char *rt_fn,
                              const char *param_name, int val)
{
    const char *const params[] = { param_name };
    emit_macro_begin(mn, params, 1);
    emit_seq_mov_edi_i32(val);
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

void emit_sm_halt()

{ emit_seq_inc_r13(20); emit_ret(); emit_pad_to_blob_size(); }

void emit_sm_return()

{ emit_macro_begin("RETURN",NULL,0); emit_ret(); emit_macro_end(); emit_pad_to_blob_size(); }

void emit_sm_return_variant(int kind, int cond, int pc)
{
    static const char *const params[] = { "kind","cond","pc" };
    emit_macro_begin("RETURN_VARIANT", params, 3);
    emit_seq_mov_edi_i32(kind); emit_mov_esi_imm32(cond);
    emit_call_sym_plt("rt_do_return", 0);
    emit_test_eax_eax(); emit_seq_jz_retskip(pc);
    emit_ret(); emit_seq_retskip_label(pc);
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
static void make_pc_label(bb_label_t *lbl, int pc) { emit_label_initf(lbl,".L%d",pc); }

void emit_sm_jump(int pc)

{ bb_label_t t; make_pc_label(&t,pc); emit_jmp(&t,JMP_JMP); }

void emit_sm_jump_s(int pc)
{ emit_call_sym_plt("rt_last_ok",0); emit_test_rax_rax();
  bb_label_t t; make_pc_label(&t,pc); emit_jmp(&t,JMP_JNE); }
void emit_sm_jump_f(int pc)
{ emit_call_sym_plt("rt_last_ok",0); emit_test_rax_rax();
  bb_label_t t; make_pc_label(&t,pc); emit_jmp(&t,JMP_JE); }

void emit_sm_label()                                      { emit_seq_noop_macro("LABEL"); }
void emit_sm_stno(int stno, int lineno, const char *src) { emit_text_stno_banner(stno,lineno,src); emit_seq_noop_macro("STNO"); }

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
    emit_seq_lea_rdi_sym(str_lbl,str_ptr);
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

void emit_sm_push_expression(uint64_t entry_ptr, int arity)
{
    static const char *const p[] = {"entry","arity"};
    emit_macro_begin("PUSH_EXPRESSION",p,2);
    emit_seq_movabs_rdi(entry_ptr); emit_mov_esi_imm32(arity);
    emit_call_sym_plt("rt_push_expression_descr",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_call_expression(const char *tgt_sym)
{
    static const char *const p[] = {"tgt"};
    emit_macro_begin("CALL_EXPRESSION",p,1);
    emit_seq_call_tgt(tgt_sym);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_exec_stmt(const char *subj_lbl, uint64_t subj_ptr, int has_repl)
{
    static const char *const p[] = {"has_repl","subj_lbl"};
    emit_macro_begin("EXEC_STMT_VARIANT",p,2);
    emit_seq_lea_rdi_sym(subj_lbl,subj_ptr); emit_mov_esi_imm32(has_repl);
    emit_call_sym_plt("rt_match_variant",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_call_fn(const char *name_lbl, uint64_t name_ptr, int nargs)
{
    (void)name_lbl; (void)name_ptr; (void)nargs;
    static const char *const p[] = {"lbl","n"};
    emit_macro_begin("CALL_FN",p,2);
    emit_seq_lea_rdi_sym(NULL,0); emit_mov_esi_imm32(0);
    emit_call_sym_plt("rt_call",0);
    emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_push_var(const char *lbl, uint64_t ptr)
{
    static const char *const p[] = {"lbl"};
    emit_macro_begin("PUSH_VAR",p,1); emit_seq_lea_rdi_sym(lbl,ptr);
    emit_call_sym_plt("rt_nv_get",0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_store_var(const char *lbl, uint64_t ptr)
{
    static const char *const p[] = {"lbl"};
    emit_macro_begin("STORE_VAR",p,1); emit_seq_lea_rdi_sym(lbl,ptr);
    emit_call_sym_plt("rt_nv_set",0); emit_macro_end(); emit_pad_to_blob_size();
}

static void emit_sm_pat_str(const char *mn, const char *rt_fn,
                             const char *lbl, uint64_t ptr)
{
    static const char *const p[] = {"lbl"};
    emit_macro_begin(mn,p,1); emit_seq_lea_rdi_sym(lbl,ptr);
    emit_call_sym_plt(rt_fn,0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_pat_lit     (const char *l, uint64_t p) { emit_sm_pat_str("PAT_LIT",      "rt_pat_lit",      l,p); }
void emit_sm_pat_refname (const char *l, uint64_t p) { emit_sm_pat_str("PAT_REFNAME",  "rt_pat_refname",  l,p); }
void emit_sm_pat_usercall(const char *l, uint64_t p) { emit_sm_pat_str("PAT_USERCALL", "rt_pat_usercall", l,p); }

void emit_sm_pat_capture(const char *name_lbl, uint64_t name_ptr, int kind)
{
    static const char *const p[] = {"lbl","n"};
    emit_macro_begin("PAT_CAPTURE",p,2);
    emit_seq_lea_rdi_sym(name_lbl,name_ptr); emit_mov_esi_imm32(kind);
    emit_call_sym_plt("rt_pat_capture",0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_pat_usercall_args(const char *name_lbl, uint64_t name_ptr, int nargs)
{
    static const char *const p[] = {"lbl","n"};
    emit_macro_begin("PAT_USERCALL_ARGS",p,2);
    emit_seq_lea_rdi_sym(name_lbl,name_ptr); emit_mov_esi_imm32(nargs);
    emit_call_sym_plt("rt_pat_usercall_args",0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_pat_capture_fn(const char *fname_lbl, uint64_t fname_ptr,
                             int is_imm, const char *namelist_lbl, uint64_t namelist_ptr)
{
    static const char *const p[] = {"is_imm","fname_lbl","namelist_lbl"};
    emit_macro_begin("PAT_CAPTURE_FN",p,3);
    emit_seq_lea_rdi_sym(fname_lbl,fname_ptr); emit_mov_esi_imm32(is_imm);
    emit_seq_lea_rdx_sym(namelist_lbl,namelist_ptr);
    emit_call_sym_plt("rt_pat_capture_fn",0); emit_macro_end(); emit_pad_to_blob_size();
}

void emit_sm_pat_capture_fn_args(const char *fname_lbl, uint64_t fname_ptr,
                                  int is_imm, int nargs)
{
    static const char *const p[] = {"is_imm","nargs","fname_lbl"};
    emit_macro_begin("PAT_CAPTURE_FN_ARGS",p,3);
    emit_seq_lea_rdi_sym(fname_lbl,fname_ptr); emit_mov_esi_imm32(is_imm);
    emit_seq_mov_edx_i32(nargs);
    emit_call_sym_plt("rt_pat_capture_fn_args",0); emit_macro_end(); emit_pad_to_blob_size();
}

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char g_pending_pc_label[32] = "";
void emit_sm_set_pc_label(const char *lbl)
{
    if (lbl && *lbl) {
        size_t n = strlen(lbl);
        if (n >= sizeof(g_pending_pc_label)) n = sizeof(g_pending_pc_label) - 1;
        memcpy(g_pending_pc_label, lbl, n);
        g_pending_pc_label[n] = '\0';
    } else {
        g_pending_pc_label[0] = '\0';
    }
}

const char *emit_sm_consume_pc_label(void)
{
    static char buf[32];
    if (!g_pending_pc_label[0]) return "";
    size_t n = strlen(g_pending_pc_label);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, g_pending_pc_label, n);
    buf[n] = '\0';
    g_pending_pc_label[0] = '\0';
    return buf;
}

typedef enum {
    SM_TPL_RTCALL=0, SM_TPL_INT64, SM_TPL_LBL, SM_TPL_LBLOPT,
    SM_TPL_LBL_INT32, SM_TPL_LBLOPT_INT32, SM_TPL_LBLOPT3,
    SM_TPL_LBLOPT_I_I, SM_TPL_EXEC_VAR, SM_TPL_ARITH,
    SM_TPL_PCREF_JMP, SM_TPL_PCREF_COND, SM_TPL_PUSH_EXPRESSION,
    SM_TPL_CALL_EXPRESSION, SM_TPL_RET, SM_TPL_RET_VAR,
    SM_TPL_UNHANDLED, SM_TPL_NOOP, SM_TPL__COUNT
} sm_tpl_kind_t;
typedef struct {
    int             op;
    const char     *macro_name;
    const char     *runtime;
    sm_tpl_kind_t   kind;
    int             const_a;
    int             const_b;
} sm_op_template_t;
typedef struct {
    int64_t     i64;
    int         i32_a; int i32_b;
    int         pc;
    const char *lbl; const char *lbl_b;
    const char *anno; const char *label;
} emit_sm_args_t;
static const sm_op_template_t *sm_template_lookup(int op);
static const sm_op_template_t *sm_template_unhandled(void);
static const sm_op_template_t *sm_template_ret_var(void);
static const sm_op_template_t g_sm_templates[] = {
    { SM_HALT,              "HALT",         "rt_halt_tos",     SM_TPL_RTCALL,    0, 0 },
    { SM_PUSH_LIT_I,        "PUSH_INT",     "rt_push_int",     SM_TPL_INT64,      0, 0 },
    { SM_PUSH_LIT_S,        "PUSH_STR",     "rt_push_str",     SM_TPL_LBL_INT32,  0, 0 },
    { SM_PUSH_VAR,          "PUSH_VAR",     "rt_nv_get",       SM_TPL_LBL,        0, 0 },
    { SM_STORE_VAR,         "STORE_VAR",    "rt_nv_set",       SM_TPL_LBL,        0, 0 },
    { SM_VOID_POP,          "VOID_POP",          "rt_pop_void",     SM_TPL_RTCALL,    0, 0 },
    { SM_PUSH_NULL,         "PUSH_NULL",    "rt_push_null",    SM_TPL_RTCALL,    0, 0 },
    { SM_CONCAT,            "CONCAT",       "rt_concat",       SM_TPL_RTCALL,    0, 0 },
    { SM_COERCE_NUM,        "COERCE_NUM",   "rt_coerce_num",   SM_TPL_RTCALL,    0, 0 },
    { SM_ADD,               "ADD_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_ADD, 0 },
    { SM_SUB,               "SUB_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_SUB, 0 },
    { SM_MUL,               "MUL_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_MUL, 0 },
    { SM_DIV,               "DIV_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_DIV, 0 },
    { SM_MOD,               "MOD_NUM",      "rt_arith",        SM_TPL_ARITH,     SM_MOD, 0 },
    { SM_JUMP,              "JUMP",         NULL,                    SM_TPL_PCREF_JMP,  0, 0 },
    { SM_JUMP_S,            "JUMP_S",       "rt_last_ok",      SM_TPL_PCREF_COND, 1, 0 },
    { SM_JUMP_F,            "JUMP_F",       "rt_last_ok",      SM_TPL_PCREF_COND, 0, 0 },
    { SM_PUSH_EXPRESSION,   "PUSH_EXPRESSION",   "rt_push_expression_descr", SM_TPL_PUSH_EXPRESSION, 0, 0 },
    { SM_CALL_EXPRESSION,   "CALL_EXPRESSION",   NULL,                    SM_TPL_CALL_EXPRESSION, 0, 0 },
    { SM_RETURN,       "RETURN",       NULL,                    SM_TPL_RET,        0, 0 },
    { SM_CALL_FN,         "CALL_FN",         "rt_call",         SM_TPL_LBL_INT32,  0, 0 },
    { SM_PAT_SPAN,     "PAT_SPAN",     "rt_pat_span",     SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_BREAK,    "PAT_BREAK",    "rt_pat_break",    SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_ANY,      "PAT_ANY",      "rt_pat_any",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_NOTANY,   "PAT_NOTANY",   "rt_pat_notany",   SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_LEN,      "PAT_LEN",      "rt_pat_len",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_POS,      "PAT_POS",      "rt_pat_pos",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_RPOS,     "PAT_RPOS",     "rt_pat_rpos",     SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_TAB,      "PAT_TAB",      "rt_pat_tab",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_RTAB,     "PAT_RTAB",     "rt_pat_rtab",     SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_ARB,      "PAT_ARB",      "rt_pat_arb",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_ARBNO,    "PAT_ARBNO",    "rt_pat_arbno",    SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_REM,      "PAT_REM",      "rt_pat_rem",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_FENCE0,    "PAT_FENCE",    "rt_pat_fence",    SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_FENCE1,   "PAT_FENCE1",   "rt_pat_fence1",   SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_FAIL,     "PAT_FAIL",     "rt_pat_fail",     SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_ABORT,    "PAT_ABORT",    "rt_pat_abort",    SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_SUCCEED,  "PAT_SUCCEED",  "rt_pat_succeed",  SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_BAL,      "PAT_BAL",      "rt_pat_bal",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_EPS,      "PAT_EPS",      "rt_pat_eps",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_CAT,      "PAT_CAT",      "rt_pat_cat",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_ALT,      "PAT_ALT",      "rt_pat_alt",      SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_DEREF,    "PAT_DEREF",    "rt_pat_deref",    SM_TPL_RTCALL,    0, 0 },
    { SM_PAT_LIT,      "PAT_LIT",      "rt_pat_lit",      SM_TPL_LBLOPT,     0, 0 },
    { SM_PAT_REFNAME,  "PAT_REFNAME",  "rt_pat_refname",  SM_TPL_LBLOPT,     0, 0 },
    { SM_PAT_USERCALL, "PAT_USERCALL", "rt_pat_usercall", SM_TPL_LBLOPT,     0, 0 },
    { SM_PAT_CAPTURE,       "PAT_CAPTURE",       "rt_pat_capture",       SM_TPL_LBLOPT_INT32, 0, 0 },
    { SM_PAT_USERCALL_ARGS, "PAT_USERCALL_ARGS", "rt_pat_usercall_args", SM_TPL_LBLOPT_INT32, 0, 0 },
    { SM_PAT_CAPTURE_FN,      "PAT_CAPTURE_FN",      "rt_pat_capture_fn",
      SM_TPL_LBLOPT3,         0, 0 },
    { SM_PAT_CAPTURE_FN_ARGS, "PAT_CAPTURE_FN_ARGS", "rt_pat_capture_fn_args",
      SM_TPL_LBLOPT_I_I,      0, 0 },
    { SM_EXEC_STMT,    "EXEC_STMT_VARIANT",  "rt_match_variant",
      SM_TPL_EXEC_VAR, 0, 0 },
    { SM_LABEL,        "LABEL",        NULL,                    SM_TPL_NOOP,       0, 0 },
    { SM_STNO,         "STNO",         NULL,                    SM_TPL_NOOP,       0, 0 },
    { SM_PUSH_NULL_NOFLIP, "PUSH_NULL_NOFLIP", "rt_push_null_noflip", SM_TPL_RTCALL, 0, 0 },
    { SM_EXP,          "EXP_NUM",      "rt_exp",          SM_TPL_RTCALL,    0, 0 },
    { SM_NEG,          "NEGATE",       "rt_neg",          SM_TPL_RTCALL,    0, 0 },
    { SM_DEFINE_ENTRY, "DEFINE_ENTRY", "rt_define_entry", SM_TPL_RTCALL,    0, 0 },
    { SM_DEFINE,       "DEFINE",       "rt_define",       SM_TPL_RTCALL,    0, 0 },
    { SM_SUSPEND,        "SUSPEND",        "rt_unhandled_sm", SM_TPL_ARITH, SM_SUSPEND,        0 },
    { SM_RESUME,         "RESUME",         "rt_unhandled_sm", SM_TPL_ARITH, SM_RESUME,         0 },
    { SM_SUSPEND_VALUE,  "SUSPEND_VALUE",  "rt_unhandled_sm", SM_TPL_ARITH, SM_SUSPEND_VALUE,  0 },
    { SM_GEN_TICK,       "GEN_TICK",       "rt_unhandled_sm", SM_TPL_ARITH, SM_GEN_TICK,       0 },
    { SM_BB_PUMP,        "BB_PUMP",        "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_PUMP,        0 },
    { SM_BB_ONCE,        "BB_ONCE",        "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_ONCE,        0 },
    { SM_BB_EVAL,        "BB_EVAL",        "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_EVAL,        0 },
    { SM_BB_ONCE_PROC,   "BB_ONCE_PROC",   "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_ONCE_PROC,   0 },
    { SM_BB_PUMP_PROC,   "BB_PUMP_PROC",   "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_PUMP_PROC,   0 },
    { SM_BB_PUMP_CASE,   "BB_PUMP_CASE",   "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_PUMP_CASE,   0 },
    { SM_BB_PUMP_SM,     "BB_PUMP_SM",     "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_PUMP_SM,     0 },
    { SM_BB_PUMP_EVERY,  "BB_PUMP_EVERY",  "rt_unhandled_sm", SM_TPL_ARITH, SM_BB_PUMP_EVERY,  0 },
    { SM_LOAD_GLOCAL,    "LOAD_GLOCAL",    "rt_unhandled_sm", SM_TPL_ARITH, SM_LOAD_GLOCAL,    0 },
    { SM_STORE_GLOCAL,   "STORE_GLOCAL",   "rt_unhandled_sm", SM_TPL_ARITH, SM_STORE_GLOCAL,   0 },
    { SM_ICMP_GT,        "ICMP_GT",        "rt_unhandled_sm", SM_TPL_ARITH, SM_ICMP_GT,        0 },
    { SM_ICMP_LT,        "ICMP_LT",        "rt_unhandled_sm", SM_TPL_ARITH, SM_ICMP_LT,        0 },
    { SM_LOAD_FRAME,     "LOAD_FRAME",     "rt_unhandled_sm", SM_TPL_ARITH, SM_LOAD_FRAME,     0 },
    { SM_STORE_FRAME,    "STORE_FRAME",    "rt_unhandled_sm", SM_TPL_ARITH, SM_STORE_FRAME,    0 },
    { SM_INCR,    "INCR",    "rt_incr",  SM_TPL_ARITH, 0 , 0 },
    { SM_DECR,    "DECR",    "rt_decr",  SM_TPL_ARITH, 0, 0 },
    { SM_ACOMP,   "ACOMP",   "rt_acomp", SM_TPL_ARITH, 0 , 0 },
    { SM_LCOMP,   "LCOMP",   "rt_lcomp", SM_TPL_ARITH, 0, 0 },
};

#define G_SM_TEMPLATES_N (int)(sizeof(g_sm_templates) / sizeof(g_sm_templates[0]))
static const sm_op_template_t g_tpl_unhandled = {
    -1, "UNHANDLED", "rt_unhandled_op", SM_TPL_UNHANDLED, 0, 0
};

static const sm_op_template_t g_tpl_ret_var = {
    -2, "RETURN_VARIANT", "rt_do_return", SM_TPL_RET_VAR, 0, 0
};

const sm_op_template_t *sm_template_lookup(int op)
{
    for (int i = 0; i < G_SM_TEMPLATES_N; i++) {
        if (g_sm_templates[i].op == op) return &g_sm_templates[i];
    }
    return NULL;
}

const sm_op_template_t *sm_template_unhandled(void)  { return &g_tpl_unhandled; }
const sm_op_template_t *sm_template_ret_var(void)    { return &g_tpl_ret_var; }

static int macro_line(FILE *out, const char *label, const char *opcode, const char *col3);
static int emit_optional_lbl(FILE *out, const char *macro_arg,
                             const char *register_load_dst)
{
    char ifnb_arg[32], lea_arg[64], xor_arg[16];
    snprintf(ifnb_arg, sizeof(ifnb_arg), "\\%s", macro_arg);
    snprintf(lea_arg,  sizeof(lea_arg),  "%s, [rip + \\%s]",
             register_load_dst, macro_arg);
    snprintf(xor_arg,  sizeof(xor_arg),  "e%s, e%s",
             register_load_dst + 1, register_load_dst + 1);
    if (macro_line(out, "", ".ifnb", ifnb_arg) < 0) return -1;
    if (macro_line(out, "", "lea",   lea_arg)  < 0) return -1;
    if (macro_line(out, "", ".else", "")       < 0) return -1;
    if (macro_line(out, "", "xor",   xor_arg)  < 0) return -1;
    if (macro_line(out, "", ".endif", "")      < 0) return -1;
    return 0;
}

static int macro_line(FILE *out, const char *label, const char *opcode, const char *col3)
{
    const char *lbl = (label  && *label)  ? label  : "";
    const char *op  = (opcode && *opcode) ? opcode : "";
    const char *c3  = (col3   && *col3)   ? col3   : "";
    char line[768];
    int n = snprintf(line, sizeof(line), "%-24s%-16s %s", lbl, op, c3);
    if (n < 0) return -1;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) n--;
    line[n] = '\0';
    return (fputs(line, out) < 0 || fputc('\n', out) == EOF) ? -1 : 0;
}

static int render_macro_body(FILE *out, const sm_op_template_t *t)
{
    char macro_def[64];
    switch (t->kind) {
    case SM_TPL_RTCALL:
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        if (strcmp(t->macro_name, "DEFINE_ENTRY") == 0) {
            macro_line(out, "", "push", "rbp");
            macro_line(out, "", "mov",  "rbp, rsp");
        }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_RET:
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "mov",  "rsp, rbp");
        macro_line(out, "", "pop",  "rbp");
        macro_line(out, "", "ret", "");
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_INT64:
        snprintf(macro_def, sizeof(macro_def), "%s val", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "movabs", "rdi, \\val");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_LBL:
        snprintf(macro_def, sizeof(macro_def), "%s lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "lea", "rdi, [rip + \\lbl]");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_LBLOPT:
        snprintf(macro_def, sizeof(macro_def), "%s lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "lbl", "rdi");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_LBL_INT32:
        snprintf(macro_def, sizeof(macro_def), "%s lbl, n", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "lea", "rdi, [rip + \\lbl]");
        macro_line(out, "", "mov", "esi, \\n");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_LBLOPT_INT32:
        snprintf(macro_def, sizeof(macro_def), "%s n, lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\n");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_LBLOPT3:
        snprintf(macro_def, sizeof(macro_def), "%s is_imm, fname_lbl, namelist_lbl",
                 t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "fname_lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\is_imm");
        emit_optional_lbl(out, "namelist_lbl", "rdx");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_LBLOPT_I_I:
        snprintf(macro_def, sizeof(macro_def), "%s is_imm, nargs, fname_lbl",
                 t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "fname_lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\is_imm");
        macro_line(out, "", "mov", "edx, \\nargs");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_EXEC_VAR:
        snprintf(macro_def, sizeof(macro_def), "%s has_repl, subj_lbl", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        emit_optional_lbl(out, "subj_lbl", "rdi");
        macro_line(out, "", "mov", "esi, \\has_repl");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_ARITH:
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        { char op_arg[32]; snprintf(op_arg, sizeof(op_arg), "edi, %d", t->const_a);
          macro_line(out, "", "mov", op_arg); }
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_PCREF_JMP:
        snprintf(macro_def, sizeof(macro_def), "%s tgt", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "jmp", "\\tgt");
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_PCREF_COND:
        snprintf(macro_def, sizeof(macro_def), "%s tgt", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", "test", "eax, eax");
        macro_line(out, "", t->const_a ? "jnz" : "jz", "\\tgt");
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_PUSH_EXPRESSION:
        snprintf(macro_def, sizeof(macro_def), "%s entry, arity", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "movabs", "rdi, \\entry");
        macro_line(out, "", "mov", "esi, \\arity");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_CALL_EXPRESSION:
        snprintf(macro_def, sizeof(macro_def), "%s tgt", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "call", "\\tgt");
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_RET_VAR:
        snprintf(macro_def, sizeof(macro_def), "%s kind, cond, pc", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "mov", "edi, \\kind");
        macro_line(out, "", "mov", "esi, \\cond");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", "test", "eax, eax");
        macro_line(out, "", "jz", ".Lretskip_\\pc");
        macro_line(out, "", "mov", "rsp, rbp");
        macro_line(out, "", "pop", "rbp");
        macro_line(out, "", "ret", "");
        fprintf(out, ".Lretskip_\\pc\\():\n");
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_UNHANDLED:
        snprintf(macro_def, sizeof(macro_def), "%s op", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", "mov", "edi, \\op");
        { char ct[64]; snprintf(ct, sizeof(ct), "%s@PLT", t->runtime);
          macro_line(out, "", "call", ct); }
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL_NOOP:
        snprintf(macro_def, sizeof(macro_def), "%s", t->macro_name);
        macro_line(out, "", ".macro", macro_def);
        macro_line(out, "", ".endm", "");
        return 0;
    case SM_TPL__COUNT:
        break;
    }
    fprintf(stderr, "emit_sm_template: render_macro_body: unknown kind %d\n",
            (int)t->kind);
    return -1;
}

static const char *anno_or_empty(const char *anno)
{
    return (anno && *anno) ? anno : NULL;
}

static int write_anno(FILE *out, const char *anno)
{
    const char *a = anno_or_empty(anno);
    if (!a) return fputc('\n', out) == EOF ? -1 : 0;
    if (a[0] == '#') return fprintf(out, "  %s\n", a) < 0 ? -1 : 0;
    return fprintf(out, "  # %s\n", a) < 0 ? -1 : 0;
}

static int build_args_col(char *buf, int cap, const sm_op_template_t *t,
                          const emit_sm_args_t *args)
{
    int n = 0;
    switch (t->kind) {
    case SM_TPL_RTCALL:
    case SM_TPL_RET:
    case SM_TPL_NOOP:
        n = snprintf(buf, cap, "");
        break;
    case SM_TPL_INT64:
        n = snprintf(buf, cap, "%" PRId64, args->i64);
        break;
    case SM_TPL_LBL:
        if (!args->lbl) {
            fprintf(stderr, "emit_sm_template: SM_TPL_LBL got NULL lbl for %s\n",
                    t->macro_name);
            return -1;
        }
        n = snprintf(buf, cap, "%s", args->lbl);
        break;
    case SM_TPL_LBLOPT:
        if (args->lbl)
            n = snprintf(buf, cap, "%s", args->lbl);
        else
            n = snprintf(buf, cap, "");
        break;
    case SM_TPL_LBL_INT32:
        if (!args->lbl) {
            fprintf(stderr, "emit_sm_template: SM_TPL_LBL_INT32 got NULL lbl for %s\n",
                    t->macro_name);
            return -1;
        }
        n = snprintf(buf, cap, "%s, %d", args->lbl, args->i32_a);
        break;
    case SM_TPL_LBLOPT_INT32:
        if (args->lbl)
            n = snprintf(buf, cap, "%d, %s", args->i32_a, args->lbl);
        else
            n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL_LBLOPT3:
        if (args->lbl && args->lbl_b)
            n = snprintf(buf, cap, "%d, %s, %s",
                         args->i32_a, args->lbl, args->lbl_b);
        else if (args->lbl)
            n = snprintf(buf, cap, "%d, %s", args->i32_a, args->lbl);
        else if (args->lbl_b)
            n = snprintf(buf, cap, "%d, , %s", args->i32_a, args->lbl_b);
        else
            n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL_LBLOPT_I_I:
        if (args->lbl)
            n = snprintf(buf, cap, "%d, %d, %s",
                         args->i32_a, args->i32_b, args->lbl);
        else
            n = snprintf(buf, cap, "%d, %d", args->i32_a, args->i32_b);
        break;
    case SM_TPL_EXEC_VAR:
        if (args->lbl)
            n = snprintf(buf, cap, "%d, %s", args->i32_a, args->lbl);
        else
            n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL_ARITH:
        n = snprintf(buf, cap, "");
        break;
    case SM_TPL_PCREF_JMP:
    case SM_TPL_PCREF_COND:
    case SM_TPL_CALL_EXPRESSION:
        n = snprintf(buf, cap, ".L%d", args->i32_a);
        break;
    case SM_TPL_PUSH_EXPRESSION:
        n = snprintf(buf, cap, "%" PRId64 ", %d", args->i64, args->i32_a);
        break;
    case SM_TPL_RET_VAR:
        n = snprintf(buf, cap, "%d, %d, %d",
                     args->i32_a, args->i32_b, args->pc);
        break;
    case SM_TPL_UNHANDLED:
        n = snprintf(buf, cap, "%d", args->i32_a);
        break;
    case SM_TPL__COUNT:
        break;
    default:
        fprintf(stderr, "emit_sm_template: build_args_col: unknown kind %d\n",
                (int)t->kind);
        return -1;
    }
    return (n < 0 || n >= cap) ? -1 : 0;
}

static int render_call_line(FILE *out, const sm_op_template_t *t,
                            const emit_sm_args_t *args)
{
    char argsb[128];
    if (build_args_col(argsb, sizeof(argsb), t, args) != 0) return -1;
    char lbl_buf[32];
    const char *lbl_col;
    if (args && args->label && *args->label) {
        lbl_col = args->label;
    } else if (g_pending_pc_label[0]) {
        size_t n = strlen(g_pending_pc_label);
        if (n >= sizeof(lbl_buf)) n = sizeof(lbl_buf) - 1;
        memcpy(lbl_buf, g_pending_pc_label, n);
        lbl_buf[n] = '\0';
        lbl_col = lbl_buf;
    } else {
        lbl_col = "";
    }
    g_pending_pc_label[0] = '\0';
    char col3[256];
    const char *anno = args ? args->anno : NULL;
    if (argsb[0] && anno && *anno) {
        if (anno[0] == '#')
            snprintf(col3, sizeof(col3), "%s %s", argsb, anno);
        else
            snprintf(col3, sizeof(col3), "%s # %s", argsb, anno);
    } else if (argsb[0]) {
        snprintf(col3, sizeof(col3), "%s", argsb);
    } else if (anno && *anno) {
        if (anno[0] == '#')
            snprintf(col3, sizeof(col3), "%s", anno);
        else
            snprintf(col3, sizeof(col3), "# %s", anno);
    } else {
        col3[0] = '\0';
    }
    bb3c_format(out, (lbl_col && *lbl_col) ? lbl_col : "",
                t->macro_name, col3);
    return 0;
}

int emit_sm_macro_library(FILE *out)
{
    const char *seen[256] = { 0 };
    int n_seen = 0;
    if (fputs(
        "# === BEGIN sm macro library (generated from g_sm_templates[]) ===\n"
        "# EM-7c-sm-macros: one macro per opcode group; bodies and per-call\n"
        "#   emissions share one renderer in emit_sm_template.c, so the\n"
        "#   .s and the C dispatcher cannot drift -- they are paired by\n"
        "#   shape kind in render_macro_body() / render_call_line().\n"
        "                        .intel_syntax    noprefix\n",
        out) == EOF) return -1;
    #define EMIT_IF_NEW(tpl) do {                                           \
        const sm_op_template_t *_t = (tpl);                                 \
        int already = 0;                                                    \
        for (int _i = 0; _i < n_seen; _i++) {                               \
            if (strcmp(seen[_i], _t->macro_name) == 0) { already = 1; break; } \
        }                                                                   \
        if (!already) {                                                     \
            if (n_seen >= (int)(sizeof(seen)/sizeof(seen[0]))) {            \
                fprintf(stderr, "emit_sm_macro_library: seen[] overflow\n");\
                return -1;                                                  \
            }                                                               \
            seen[n_seen++] = _t->macro_name;                                \
            if (render_macro_body(out, _t) != 0) return -1;                 \
        }                                                                   \
    } while (0)
    for (int i = 0; i < G_SM_TEMPLATES_N; i++) {
        EMIT_IF_NEW(&g_sm_templates[i]);
    }
    EMIT_IF_NEW(&g_tpl_unhandled);
    EMIT_IF_NEW(&g_tpl_ret_var);
    #undef EMIT_IF_NEW
    if (fputs(
        "# PUSH_REAL: hand-written (SM_PUSH_LIT_F special-case; param = 64-bit bits as hex)\n"
        "                        .macro           PUSH_REAL val\n"
        "                        movabs           rdi, \\val\n"
        "                        call             rt_push_real_bits@PLT\n"
        "                        .endm\n",
        out) == EOF) return -1;
    if (fputs("# === END sm macro library ===\n", out) == EOF) return -1;
    return 0;
}

int emit_sm_macro_library_to_path(const char *path)
{
    if (!path || !*path) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "emit_sm_macro_library_to_path: cannot open %s for writing\n",
                path);
        return -1;
    }
    int rc = emit_sm_macro_library(fp);
    if (fclose(fp) != 0) return -1;
    return rc;
}

int emit_sm_template(FILE *out, const sm_op_template_t *t,
                     const emit_sm_args_t *args)
{
    if (!t || !args) return -1;
    return render_call_line(out, t, args);
}

int emit_sm_rtcall(FILE *out, const sm_op_template_t *t, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_noop(FILE *out, const sm_op_template_t *t, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_int64(FILE *out, const sm_op_template_t *t,
                  int64_t v, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.i64 = v;
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_lbl(FILE *out, const sm_op_template_t *t,
                const char *lbl, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.lbl = lbl;
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_lblopt(FILE *out, const sm_op_template_t *t,
                   const char *lbl_or_null, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.lbl = (lbl_or_null && *lbl_or_null) ? lbl_or_null : NULL;
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_lbl_int32(FILE *out, const sm_op_template_t *t,
                      const char *lbl, int n, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.lbl = lbl;
    a.i32_a = n;
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_lblopt_int32(FILE *out, const sm_op_template_t *t,
                         const char *lbl_or_null, int n, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.lbl = (lbl_or_null && *lbl_or_null) ? lbl_or_null : NULL;
    a.i32_a = n;
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_arith(FILE *out, const sm_op_template_t *t)
{
    emit_sm_args_t a = { 0 };
    return emit_sm_template(out, t, &a);
}

int emit_sm_pcref_jmp(FILE *out, const sm_op_template_t *t,
                      int target_pc, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.i32_a = target_pc;
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_pcref_cond(FILE *out, const sm_op_template_t *t,
                       int target_pc, int taken_when_ok,
                       const char *anno)
{
    (void)taken_when_ok;
    emit_sm_args_t a = { 0 };
    a.i32_a = target_pc;
    a.anno = anno;
    return emit_sm_template(out, t, &a);
}

int edp4_emit_push_expression(FILE *out, const sm_op_template_t *t,
                       int64_t entry_pc, int arity)
{
    emit_sm_args_t a = { 0 };
    a.i64 = entry_pc;
    a.i32_a = arity;
    return emit_sm_template(out, t, &a);
}

int edp4_emit_call_expression(FILE *out, const sm_op_template_t *t, int target_pc)
{
    emit_sm_args_t a = { 0 };
    a.i32_a = target_pc;
    return emit_sm_template(out, t, &a);
}

int emit_sm_ret(FILE *out, const sm_op_template_t *t, const char *anno)
{
    return emit_sm_rtcall(out, t, anno);
}

int emit_sm_ret_var(FILE *out, int kind, int cond, int pc, const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.i32_a = kind;
    a.i32_b = cond;
    a.pc    = pc;
    a.anno  = anno;
    return emit_sm_template(out, sm_template_ret_var(), &a);
}

int emit_sm_unhandled(FILE *out, int op)
{
    emit_sm_args_t a = { 0 };
    a.i32_a = op;
    return emit_sm_template(out, sm_template_unhandled(), &a);
}

int emit_sm_exec_var(FILE *out, const sm_op_template_t *t,
                     const char *subj_lbl_or_null, int has_repl)
{
    emit_sm_args_t a = { 0 };
    a.lbl   = (subj_lbl_or_null && *subj_lbl_or_null) ? subj_lbl_or_null : NULL;
    a.i32_a = has_repl;
    return emit_sm_template(out, t, &a);
}

int emit_sm_capture_fn(FILE *out, const sm_op_template_t *t,
                       const char *fname_lbl_or_null,
                       int is_imm,
                       const char *namelist_lbl_or_null,
                       const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.lbl   = (fname_lbl_or_null && *fname_lbl_or_null) ? fname_lbl_or_null : NULL;
    a.lbl_b = (namelist_lbl_or_null && *namelist_lbl_or_null) ? namelist_lbl_or_null : NULL;
    a.i32_a = is_imm;
    a.anno  = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_capture_fn_args(FILE *out, const sm_op_template_t *t,
                            const char *fname_lbl_or_null,
                            int is_imm, int nargs,
                            const char *anno)
{
    emit_sm_args_t a = { 0 };
    a.lbl   = (fname_lbl_or_null && *fname_lbl_or_null) ? fname_lbl_or_null : NULL;
    a.i32_a = is_imm;
    a.i32_b = nargs;
    a.anno  = anno;
    return emit_sm_template(out, t, &a);
}

int emit_sm_template_selftest(FILE *out)
{
    int failures = 0;
    if (fprintf(out, "emit_sm_template self-test: %d templates\n",
                G_SM_TEMPLATES_N + 2) < 0) return -1;
    if (emit_sm_macro_library(out) != 0) {
        fprintf(out, "FAIL: emit_sm_macro_library returned -1\n");
        failures++;
    }
    emit_sm_args_t sentinel = {
        .i64   = 0x12345678,
        .i32_a = 7,
        .i32_b = 3,
        .pc    = 99,
        .lbl   = ".LstrSEN",
        .lbl_b = ".LstrSEN2",
        .anno  = "# self-test"
    };
    for (int i = 0; i < G_SM_TEMPLATES_N; i++) {
        if (render_call_line(out, &g_sm_templates[i], &sentinel) != 0) {
            fprintf(out, "FAIL: render_call_line(%s)\n",
                    g_sm_templates[i].macro_name);
            failures++;
        }
    }
    if (render_call_line(out, &g_tpl_unhandled, &sentinel) != 0)
        { fprintf(out, "FAIL: unhandled\n"); failures++; }
    if (render_call_line(out, &g_tpl_ret_var, &sentinel) != 0)
        { fprintf(out, "FAIL: ret_var\n"); failures++; }
    fprintf(out, "self-test: %d failures\n", failures);
    return failures ? -1 : 0;
}

#include "sm_image.h"
#include "bb_broker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
void emit_sm_op(int op);
void emit_sm_halt(void);
void emit_sm_return(void);
void emit_sm_label(void);
void emit_sm_stno(int stno, int lineno, const char *src);
void emit_sm_push_lit_i(int64_t val);
void emit_sm_push_lit_f(double val);
void emit_sm_push_lit_s(const char *str_lbl, uint64_t str_ptr, int len);
void emit_sm_push_expr(uint64_t ptr_val);
void emit_sm_push_expression(uint64_t entry_ptr, int arity);
void emit_sm_call_expression(const char *tgt_sym);
void emit_sm_push_var(const char *lbl, uint64_t ptr);
void emit_sm_store_var(const char *lbl, uint64_t ptr);
void emit_sm_exec_stmt(const char *subj_lbl, uint64_t subj_ptr, int has_repl);
void emit_sm_call_fn(const char *name_lbl, uint64_t name_ptr, int nargs);
void emit_sm_jump(int pc);
void emit_sm_jump_s(int pc);
void emit_sm_jump_f(int pc);
void emit_sm_add(void); void emit_sm_sub(void); void emit_sm_mul(void);
void emit_sm_div(void); void emit_sm_mod(void);
void emit_sm_acomp(int op); void emit_sm_lcomp(int op);
void emit_sm_incr(int64_t n); void emit_sm_decr(int64_t n);
void emit_sm_return_variant(int kind, int cond, int pc);
void emit_sm_freturn(int pc); void emit_sm_nreturn(int pc);
void emit_sm_return_s(int pc); void emit_sm_return_f(int pc);
void emit_sm_freturn_s(int pc); void emit_sm_freturn_f(int pc);
void emit_sm_nreturn_s(int pc); void emit_sm_nreturn_f(int pc);
void emit_sm_pat_lit(const char *l, uint64_t p);
void emit_sm_pat_refname(const char *l, uint64_t p);
void emit_sm_pat_usercall(const char *l, uint64_t p);
void emit_sm_pat_capture(const char *name_lbl, uint64_t name_ptr, int kind);
void emit_sm_pat_usercall_args(const char *name_lbl, uint64_t name_ptr, int nargs);
void emit_sm_pat_capture_fn(const char *fname_lbl, uint64_t fname_ptr, int is_imm, const char *namelist_lbl, uint64_t namelist_ptr);
void emit_sm_pat_capture_fn_args(const char *fname_lbl, uint64_t fname_ptr, int is_imm, int nargs);
void emit_sm_pat_span(void); void emit_sm_pat_break(void);
void emit_sm_pat_any(void);  void emit_sm_pat_notany(void);
void emit_sm_pat_len(void);  void emit_sm_pat_pos(void);
void emit_sm_pat_rpos(void); void emit_sm_pat_tab(void);
void emit_sm_pat_rtab(void); void emit_sm_pat_arb(void);
void emit_sm_pat_arbno(void);void emit_sm_pat_rem(void);
void emit_sm_pat_fence(void);void emit_sm_pat_fence1(void);
void emit_sm_pat_fail(void); void emit_sm_pat_abort(void);
void emit_sm_pat_succeed(void); void emit_sm_pat_bal(void);
void emit_sm_pat_eps(void);  void emit_sm_pat_cat(void);
void emit_sm_pat_alt(void);  void emit_sm_pat_deref(void);
void emit_sm_unhandled_op(int op);
void emit_sm_define(void); void emit_sm_define_entry(void);
void emit_sm_coerce_num(void); void emit_sm_concat(void);
void emit_sm_push_null(void); void emit_sm_push_null_noflip(void);
void emit_sm_neg(void); void emit_sm_exp(void);
void emit_sm_resume(void); void emit_sm_suspend(void);
void emit_sm_bb_pump(void); void emit_sm_bb_once(void);
void emit_sm_bb_once_proc(void); void emit_sm_bb_pump_proc(void);
void emit_sm_bb_pump_sm(void); void emit_sm_bb_pump_ast(void);
void emit_sm_bb_pump_case(void); void emit_sm_bb_pump_every(void);
void emit_sm_bb_eval(void);
int  emit_sm_macro_library(FILE *out);
int  emit_sm_macro_library_to_path(const char *path);
const sm_op_template_t *sm_template_lookup(int op);
const char *emit_sm_consume_pc_label(void);
void emit_sm_set_pc_label(const char *lbl);
int g_emit_inline = 0;
#define TEXT_MODE() (g_emit_inline ? EMIT_TEXT_INLINE : EMIT_TEXT)
typedef struct {
    char       *buf;
    char      **lines;
    int         count;
    const char *path;
} SrcLines;
static int srclines_load(SrcLines *sl, const char *src_path)
{
    memset(sl, 0, sizeof(*sl));
    if (!src_path) return -1;
    FILE *f = fopen(src_path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n < 0)                       { fclose(f); return -1; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf)                        { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    int count = 0;
    for (size_t i = 0; i < got; i++) if (buf[i] == '\n') count++;
    if (got > 0 && buf[got-1] != '\n') count++;
    char **lines = calloc((size_t)count + 2, sizeof(char *));
    if (!lines) { free(buf); return -1; }
    int    li = 1;
    char  *p  = buf;
    char  *line_start = buf;
    for (size_t i = 0; i < got; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            lines[li++] = line_start;
            line_start = &buf[i+1];
        }
    }
    if (line_start < buf + got) lines[li++] = line_start;
    sl->buf   = buf;
    sl->lines = lines;
    sl->count = li - 1;
    sl->path  = src_path;
    (void)p;
    return 0;
}

static void srclines_free(SrcLines *sl)
{
    free(sl->lines);
    free(sl->buf);
    memset(sl, 0, sizeof(*sl));
}

static const char *srclines_get(const SrcLines *sl, int lineno)
{
    if (!sl || !sl->lines || lineno < 1 || lineno > sl->count) return NULL;
    return sl->lines[lineno];
}

static void srcline_strip_cr(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    if (n > 0 && s[n-1] == '\r') s[n-1] = '\0';
}

#define STRTAB_CAP 8192
typedef struct {
    const char *s;
    int         idx;
} StrEntry;
static StrEntry g_strtab[STRTAB_CAP];
static int      g_strtab_n = 0;
static void strtab_reset(void)
{
    g_strtab_n = 0;
}

static int strtab_intern(const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0)
            return g_strtab[i].idx;
    if (g_strtab_n >= STRTAB_CAP) {
        fprintf(stderr, "sm_codegen_text: string table overflow\n");
        abort();
    }
    int idx = g_strtab_n;
    g_strtab[g_strtab_n].s   = s;
    g_strtab[g_strtab_n].idx = idx;
    g_strtab_n++;
    return idx;
}

static int strtab_lookup(const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0)
            return g_strtab[i].idx;
    return -1;
}

static void strtab_label(char *buf, size_t bufsz, const char *s)
{
    if (!s) s = "";
    for (int i = 0; i < g_strtab_n; i++)
        if (g_strtab[i].s == s || strcmp(g_strtab[i].s, s) == 0) {
            snprintf(buf, bufsz, ".S%d", g_strtab[i].idx);
            return;
        }
    snprintf(buf, bufsz, ".S_ERR");
}

static char g_intern_str_buf[64];
static const char *codegen_intern_str(const char *s)
{
    if (s) strtab_intern(s);
    strtab_label(g_intern_str_buf, sizeof(g_intern_str_buf), s ? s : "");
    return g_intern_str_buf;
}

static void strtab_escape(char *out, size_t outsz, const char *s)
{
    size_t j = 0;
    out[j++] = '"';
    for (const char *p = s; *p && j + 6 < outsz; p++) {
        unsigned char c = (unsigned char)*p;
        if      (c == '\\') { out[j++] = '\\'; out[j++] = '\\'; }
        else if (c == '"')  { out[j++] = '\\'; out[j++] = '"';  }
        else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n';  }
        else if (c == '\t') { out[j++] = '\\'; out[j++] = 't';  }
        else if (c < 0x20 || c == 0x7f) {
            j += (size_t)snprintf(out + j, outsz - j, "\\%03o", c);
        } else {
            out[j++] = (char)c;
        }
    }
    out[j++] = '"';
    if (j < outsz) out[j] = '\0';
}

static void strtab_collect(const SM_Program *prog)
{
    strtab_reset();
    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        switch (ins->op) {
        case SM_PUSH_LIT_S:
        case SM_PUSH_VAR:
        case SM_STORE_VAR:
        case SM_PAT_LIT:
        case SM_PAT_REFNAME:
        case SM_PAT_CAPTURE:
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_USERCALL:
        case SM_PAT_CAPTURE_FN_ARGS:
        case SM_PAT_USERCALL_ARGS:
        case SM_EXEC_STMT:
        case SM_CALL_FN:
        case SM_LABEL:
            if (ins->a[0].s) strtab_intern(ins->a[0].s);
            break;
        default:
            break;
        }
        switch (ins->op) {
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_USERCALL:
            if (ins->a[2].s) strtab_intern(ins->a[2].s);
            break;
        default:
            break;
        }
    }
}

static uint8_t *g_pc_used_as_target = NULL;
static int      g_pc_used_count     = 0;
static int pc_used_alloc(const SM_Program *prog)
{
    if (g_pc_used_as_target) {
        free(g_pc_used_as_target);
        g_pc_used_as_target = NULL;
        g_pc_used_count = 0;
    }
    if (!prog || prog->count <= 0) return 0;
    g_pc_used_as_target = (uint8_t *)calloc((size_t)prog->count, 1);
    if (!g_pc_used_as_target) return -1;
    g_pc_used_count = prog->count;
    g_pc_used_as_target[0] = 1;
    return 0;
}

static inline void pc_used_mark(int pc)
{
    if (g_pc_used_as_target && pc >= 0 && pc < g_pc_used_count)
        g_pc_used_as_target[pc] = 1;
}

static int pc_is_used_as_target(int pc)
{
    if (!g_pc_used_as_target) return 1;
    if (pc < 0 || pc >= g_pc_used_count) return 1;
    return g_pc_used_as_target[pc] ? 1 : 0;
}

static void release_pc_used_as_target(void)
{
    if (g_pc_used_as_target) {
        free(g_pc_used_as_target);
        g_pc_used_as_target = NULL;
        g_pc_used_count = 0;
    }
}

static int emit_three_column_line(FILE *out,
                                  const char *label,
                                  const char *opcode,
                                  const char *col3,
                                  const char *anno)
{
    char c3[512];
    if (col3 && *col3 && anno && *anno) {
        if (anno[0] == '#')
            snprintf(c3, sizeof(c3), "%s %s", col3, anno);
        else
            snprintf(c3, sizeof(c3), "%s # %s", col3, anno);
    } else if (col3 && *col3) {
        snprintf(c3, sizeof(c3), "%s", col3);
    } else if (anno && *anno) {
        if (anno[0] == '#')
            snprintf(c3, sizeof(c3), "%s", anno);
        else
            snprintf(c3, sizeof(c3), "# %s", anno);
    } else {
        c3[0] = '\0';
    }
    bb3c_format(out, label ? label : "", opcode ? opcode : "", c3);
    return 0;
}

static int strtab_emit_rodata(FILE *out)
{
    if (g_strtab_n == 0) return 0;
    if (emit_three_column_line(out, "", ".section", ".rodata", NULL) != 0) return -1;
    char esc[1024];
    char lbl[32];
    for (int i = 0; i < g_strtab_n; i++) {
        strtab_escape(esc, sizeof(esc), g_strtab[i].s);
        snprintf(lbl, sizeof(lbl), ".S%d:", i);
        if (emit_three_column_line(out, lbl, ".string", esc, NULL) != 0) return -1;
    }
    if (emit_three_column_line(out, "", ".text", "", NULL) != 0) return -1;
    return 0;
}

static int emit_expression_registry(FILE *out, const SM_Program *prog)
{
    int n = 0;
    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        if (ins->op == SM_LABEL && ins->a[0].s && *ins->a[0].s && ins->a[2].i == 1)
            n++;
    }
    if (n == 0) return 0;
    if (emit_three_column_line(out, "", ".section", ".data", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".align",   "8",     NULL) != 0) return -1;
    if (emit_three_column_line(out, ".Lexpression_registry:", "", "", NULL) != 0) return -1;
    for (int i = 0; i < prog->count; i++) {
        const SM_Instr *ins = &prog->instrs[i];
        if (ins->op != SM_LABEL || !ins->a[0].s || !*ins->a[0].s || ins->a[2].i != 1) continue;
        int str_idx = strtab_lookup(ins->a[0].s);
        if (str_idx < 0) continue;
        int entry_pc = i + 1;
        char qarg[32];
        snprintf(qarg, sizeof(qarg), ".S%d", str_idx);
        char combined[128];
        snprintf(combined, sizeof(combined), "%-16s ; .quad            .L%d", qarg, entry_pc);
        if (emit_three_column_line(out, "", ".quad", combined, NULL) != 0) return -1;
    }
    if (emit_three_column_line(out, "", ".quad", "0                ; .quad            0", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "",  NULL)        != 0) return -1;
    return n;
}

#define MAX_CAP_FIXUPS 1024
typedef struct {
    void       *cap_ptr;
    char        child_label[128];
} cap_fixup_t;
static cap_fixup_t g_cap_fixups[MAX_CAP_FIXUPS];
static int         g_cap_fixups_n = 0;

static void cap_fixups_reset(void) { g_cap_fixups_n = 0; }

static void cap_fixup_add(void *cap_ptr, const char *child_label)
{
    if (g_cap_fixups_n >= MAX_CAP_FIXUPS) return;
    g_cap_fixups[g_cap_fixups_n].cap_ptr = cap_ptr;
    snprintf(g_cap_fixups[g_cap_fixups_n].child_label,
             sizeof(g_cap_fixups[0].child_label), "%s", child_label);
    g_cap_fixups_n++;
}

static int emit_file_header(FILE *out, int count, int has_expression_registry)
{
    if (emit_three_column_line(out, "", ".intel_syntax", "noprefix", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".globl",  "main",          NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".type",   "main, @function", NULL) != 0) return -1;
    if (emit_three_column_line(out, "main:",       "push",   "rbp", NULL) != 0) return -1;
    if (emit_three_column_line(out, "",            "mov",    "rbp, rsp", NULL) != 0) return -1;
    if (has_expression_registry) {
        if (emit_three_column_line(out, "", "lea",  "rdi, [rip + .Lexpression_registry]", NULL) != 0) return -1;
        if (emit_three_column_line(out, "", "call", "rt_register_expressions@PLT", NULL) != 0) return -1;
    } else {
        if (emit_three_column_line(out, "", "xor",  "edi, edi", NULL) != 0) return -1;
        if (emit_three_column_line(out, "", "call", "rt_register_expressions@PLT", NULL) != 0) return -1;
    }
    for (int i = 0; i < g_cap_fixups_n; i++) {
        const char *α = g_cap_fixups[i].child_label;
        if ((uintptr_t)g_cap_fixups[i].cap_ptr == 1) {
            char cap_lbl[128];
            const char *p = α;
            if (*p == '_') p++;
            const char *underscore = strchr(p, '_');
            int id_len = underscore ? (int)(underscore - p) : (int)strlen(p);
            snprintf(cap_lbl, sizeof(cap_lbl), ".L%.*s_data", id_len, p);
            char rdi_arg[128], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", cap_lbl);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "lea",  rdi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",  rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call", "rt_patch_cap_fn@PLT", NULL) != 0) return -1;
        } else if ((uintptr_t)g_cap_fixups[i].cap_ptr == 2) {
            char slot_lbl[128];
            const char *p = α;
            if (*p == '_') p++;
            const char *underscore = strchr(p, '_');
            int id_len = underscore ? (int)(underscore - p) : (int)strlen(p);
            snprintf(slot_lbl, sizeof(slot_lbl), ".L%.*s_slot", id_len, p);
            char rdi_arg[128], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", slot_lbl);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "lea",  rdi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",  rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call", "rt_init_arbno@PLT", NULL) != 0) return -1;
        } else {
            char rdi_arg[64], rsi_arg[128];
            snprintf(rdi_arg, sizeof(rdi_arg), "rdi, %llu",
                     (unsigned long long)(uintptr_t)g_cap_fixups[i].cap_ptr);
            snprintf(rsi_arg, sizeof(rsi_arg), "rsi, [rip + %s]", α);
            if (emit_three_column_line(out, "", "movabs", rdi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "lea",    rsi_arg, NULL) != 0) return -1;
            if (emit_three_column_line(out, "", "call",   "rt_patch_cap_fn@PLT", NULL) != 0) return -1;
        }
    }
    if (emit_three_column_line(out, "", "call", "rt_init@PLT",
                               "rt_init(argc, argv)") != 0) return -1;
    return 0;
}

static int emit_file_footer(FILE *out)
{
    bb3c_flush_pending();
    if (emit_three_column_line(out, "", "call", "rt_finalize@PLT", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "pop",  "rbp", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "ret",  "",    NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".size", "main, .-main", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".section", ".note.GNU-stack,\"\",@progbits", NULL) != 0) return -1;
    return 0;
}

static int sm_line(FILE *out, const char *label, const char *action,
                   const char *goto_col)
{
    const char *gc = "";
    char gc_buf[128] = "";
    if (goto_col && *goto_col) {
        int is_asm = (strncmp(goto_col, "jmp", 3) == 0 ||
                      strncmp(goto_col, "je",  2) == 0 ||
                      strncmp(goto_col, "jne", 3) == 0 ||
                      strncmp(goto_col, "jz",  2) == 0 ||
                      strncmp(goto_col, "jnz", 3) == 0 ||
                      strncmp(goto_col, "ret", 3) == 0 ||
                      strncmp(goto_col, "call",4) == 0 ||
                      goto_col[0] == '#');
        if (is_asm) {
            gc = goto_col;
        } else if (goto_col[0] == ';') {
            snprintf(gc_buf, sizeof(gc_buf), "# %s", goto_col + 1);
            gc = gc_buf;
        } else {
            snprintf(gc_buf, sizeof(gc_buf), "# %s", goto_col);
            gc = gc_buf;
        }
    }
    const char *lbl;
    if (label && *label) {
        lbl = label;
    } else {
        lbl = emit_sm_consume_pc_label();
    }
    const char *act = (action && *action) ? action : "";
    bb3c_format(out, (lbl && *lbl) ? lbl : "", act, gc);
    return 0;
}

static int emit_major_break(FILE *out, int stno, int lineno,
                            const char *src_text)
{
    bb3c_flush_pending();
    if (fputs(
        "#=======================================================================================================================\n",
        out) == EOF) return -1;
    if (src_text && *src_text) {
        if (fprintf(out, "# stmt %d  (line %d):  %s\n",
                    stno, lineno, src_text) < 0) return -1;
    } else if (lineno > 0) {
        if (fprintf(out, "# stmt %d  (line %d)\n", stno, lineno) < 0) return -1;
    } else {
        if (fprintf(out, "# stmt %d\n", stno) < 0) return -1;
    }
    if (fputs(
        "#=======================================================================================================================\n",
        out) == EOF) return -1;
    return 0;
}

static int emit_sm_minor_break(FILE *out, const char *caption)
{
    bb3c_flush_pending();
    if (fputs("#-----------------------------------------------------------------------------------------------------------------------\n",
              out) == EOF) return -1;
    if (caption && *caption) {
        if (fprintf(out, "# %s\n", caption) < 0) return -1;
        if (fputs("#-----------------------------------------------------------------------------------------------------------------------\n",
                  out) == EOF) return -1;
    }
    return 0;
}

#define STR_PREVIEW_MAX  40
static void render_str_preview(char *dst, size_t cap,
                               const char *s, int slen)
{
    if (cap == 0) return;
    size_t  n = (slen > 0) ? (size_t)slen : (s ? strlen(s) : 0);
    size_t  o = 0;
    if (o < cap) dst[o++] = '"';
    for (size_t i = 0; i < n && o + 2 < cap && i < STR_PREVIEW_MAX; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            if (o + 1 < cap) dst[o++] = '\\';
            dst[o++] = (char)c;
        } else if (c < 0x20 || c == 0x7f) {
            dst[o++] = '.';
        } else {
            dst[o++] = (char)c;
        }
    }
    if (n > STR_PREVIEW_MAX && o + 4 < cap) {
        dst[o++] = '.'; dst[o++] = '.'; dst[o++] = '.';
    }
    if (o + 1 < cap) dst[o++] = '"';
    dst[o] = '\0';
}

static int emit_sm_pc_label(FILE *out, int pc)
{
    return fprintf(out, ".L%d:\n", pc) < 0 ? -1 : 0;
}

static int emit_halt_line(FILE *out, int pc)
{
    (void)pc;
    return emit_sm_rtcall(out, sm_template_lookup(SM_HALT), NULL);
}

static int emit_push_lit_i_line(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return emit_sm_int64(out, sm_template_lookup(SM_PUSH_LIT_I),
                         ins->a[0].i, NULL);
}

__attribute__((unused))
static int emit_sm_push_lit_i_legacy(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return emit_sm_int64(out, sm_template_lookup(SM_PUSH_LIT_I),
                         ins->a[0].i, NULL);
}

static int emit_sm_push_lit_s_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *s    = ins->a[0].s ? ins->a[0].s : "";
    int64_t     slen = ins->a[1].i;
    char lbl[32], anno[STR_PREVIEW_MAX + 16], preview[STR_PREVIEW_MAX + 8];
    strtab_label(lbl, sizeof(lbl), s);
    render_str_preview(preview, sizeof(preview), s, (int)slen);
    snprintf(anno, sizeof(anno), "# %s", preview);
    return emit_sm_lbl_int32(out, sm_template_lookup(SM_PUSH_LIT_S),
                             lbl, (int)slen, anno);
}

static int emit_sm_push_var_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl(out, sm_template_lookup(SM_PUSH_VAR), lbl, anno);
}

static int emit_sm_store_var_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl(out, sm_template_lookup(SM_STORE_VAR), lbl, anno);
}

static int emit_sm_pop(FILE *out, int pc)
{
    (void)pc;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_op(SM_VOID_POP);
    return 0;
}

__attribute__((unused))
static int emit_sm_pop_legacy(FILE *out, int pc)
{
    (void)pc;
    return emit_sm_rtcall(out, sm_template_lookup(SM_VOID_POP), NULL);
}

static int edp4_sm_arith(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const sm_op_template_t *t = sm_template_lookup(ins->op);
    if (!t) return -1;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_arith_op((int)ins->op, t->macro_name);
    return 0;
}

static int emit_sm_label_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)ins; (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_label();
    return 0;
}

static int emit_sm_jump_line(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    emit_sm_jump((int)ins->a[0].i);
    return 0;
}

static int emit_sm_jump_cond(FILE *out, const SM_Instr *ins, int pc,
                             int take_when_ok)
{
    (void)pc;
    int  target = (int)ins->a[0].i;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    if (take_when_ok) emit_sm_jump_s(target);
    else              emit_sm_jump_f(target);
    return 0;
}

static int emit_sm_jump_s_line(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, 1);
}

static int emit_sm_jump_f_line(FILE *out, const SM_Instr *ins, int pc)
{
    return emit_sm_jump_cond(out, ins, pc, 0);
}

static int emit_sm_push_expression_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return edp4_emit_push_expression(out, sm_template_lookup(SM_PUSH_EXPRESSION),
                              ins->a[0].i, (int)ins->a[1].i);
}

static int emit_sm_call_expression_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    return edp4_emit_call_expression(out, sm_template_lookup(SM_CALL_EXPRESSION),
                              (int)ins->a[0].i);
}

static int emit_sm_return_dispatch(FILE *out, int pc)
{
    (void)pc;
    return emit_sm_ret(out, sm_template_lookup(SM_RETURN), NULL);
}

static int emit_sm_stno_dispatch(FILE *out, const SM_Instr *ins, int pc,
                        const SrcLines *sl)
{
    (void)pc;
    int stno   = (int)ins->a[0].i;
    int lineno = (int)ins->a[1].i;
    int try_lineno = lineno;
    if (try_lineno <= 0 || (sl && try_lineno > sl->count))
        try_lineno = 0;
    char line_copy[1024];
    const char *src = NULL;
    if (sl && try_lineno > 0) {
        const char *raw = srclines_get(sl, try_lineno);
        if (raw && *raw) {
            strncpy(line_copy, raw, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';
            srcline_strip_cr(line_copy);
            src = line_copy;
        }
    }
    int banner_lineno;
    if (lineno > 0 && (!sl || lineno <= sl->count))
        banner_lineno = lineno;
    else
        banner_lineno = 0;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_stno(stno, banner_lineno, src);
    return 0;
}

static int emit_sm_concat_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_CONCAT);
    return 0;
}

static int emit_sm_push_null_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_PUSH_NULL);
    return 0;
}

static int emit_sm_coerce_num_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_coerce_num();
    return 0;
}

static int emit_sm_push_null_noflip_dispatch(FILE *out, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_op(SM_PUSH_NULL_NOFLIP);
    return 0;
}

static int emit_sm_push_lit_f_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_push_lit_f(ins->a[0].f);
    return 0;
}

static int emit_sm_push_expr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_push_expr((uint64_t)(uintptr_t)ins->a[0].ptr);
    return 0;
}

static int emit_sm_incr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_incr(ins->a[0].i);
    return 0;
}

static int emit_sm_decr_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_decr(ins->a[0].i);
    return 0;
}

static int emit_sm_acomp_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_acomp((int)ins->a[0].i);
    return 0;
}

static int emit_sm_lcomp_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    emit_mode_set(TEXT_MODE(), out);
    emit_sm_lcomp((int)ins->a[0].i);
    return 0;
}

static int emit_sm_call_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name  = ins->a[0].s ? ins->a[0].s : "";
    int         nargs = (int)ins->a[1].i;
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_lbl_int32(out, sm_template_lookup(SM_CALL_FN),
                             lbl, nargs, anno);
}

__attribute__((unused))
static int emit_sm_call_legacy(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *name  = ins->a[0].s ? ins->a[0].s : "";
    int         nargs = (int)ins->a[1].i;
    char lbl[32], anno[80];
    strtab_label(lbl, sizeof(lbl), name);
    snprintf(anno, sizeof(anno), "%s", name);
    return emit_sm_lbl_int32(out, sm_template_lookup(SM_CALL_FN),
                             lbl, nargs, anno);
}

static int emit_sm_return_variant_dispatch(FILE *out, sm_opcode_t op, int pc)
{
    int kind = 0;
    if (op == SM_FRETURN || op == SM_FRETURN_S || op == SM_FRETURN_F) kind = 1;
    if (op == SM_NRETURN || op == SM_NRETURN_S || op == SM_NRETURN_F) kind = 2;
    int cond = 0;
    if (op == SM_RETURN_S || op == SM_FRETURN_S || op == SM_NRETURN_S) cond = 1;
    if (op == SM_RETURN_F || op == SM_FRETURN_F || op == SM_NRETURN_F) cond = 2;
    return emit_sm_ret_var(out, kind, cond, pc, sm_opcode_name(op));
}

static void edp4_label_then(FILE *out, void (*fn)(emitter_t *))
{
    const char *lbl = emit_sm_consume_pc_label();
    if (lbl && *lbl) bb3c_format(out, lbl, "", "");
    emit_mode_set(TEXT_MODE(), out);
    fn(NULL);
}

static int emit_sm_define_entry_dispatch(FILE *out, const SM_Instr *ins, int pc, const SM_Program *prog) {
    (void)ins;
    const char *name = (pc > 0 && prog->instrs[pc-1].a[0].s) ? prog->instrs[pc-1].a[0].s : "";
    char anno[80]; snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_noop(out, sm_template_lookup(SM_DEFINE_ENTRY), anno);
}

static int emit_sm_define_dispatch(FILE *out, const SM_Instr *ins, int pc) {
    (void)pc;
    const char *name = ins->a[0].s ? ins->a[0].s : "";
    char anno[80]; snprintf(anno, sizeof(anno), "# %s", name);
    return emit_sm_noop(out, sm_template_lookup(SM_DEFINE), anno);
}

static int emit_sm_pat_span_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_span);     return 0; }
static int emit_sm_pat_break_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_break);    return 0; }
static int emit_sm_pat_any_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_any);      return 0; }
static int emit_sm_pat_notany_dispatch(FILE *out, int pc)   { (void)pc; edp4_label_then(out, emit_sm_pat_notany);   return 0; }
static int emit_sm_pat_len_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_len);      return 0; }
static int emit_sm_pat_pos_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_pos);      return 0; }
static int emit_sm_pat_rpos_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_rpos);     return 0; }
static int emit_sm_pat_tab_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_tab);      return 0; }
static int emit_sm_pat_rtab_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_rtab);     return 0; }
static int emit_sm_pat_arb_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_arb);      return 0; }
static int emit_sm_pat_arbno_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_arbno);    return 0; }
static int emit_sm_pat_rem_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_rem);      return 0; }
static int emit_sm_pat_fence0_dispatch(FILE *out, int pc)   { (void)pc; edp4_label_then(out, emit_sm_pat_fence);    return 0; }
static int emit_sm_pat_fence1_dispatch(FILE *out, int pc)   { (void)pc; edp4_label_then(out, emit_sm_pat_fence1);   return 0; }
static int emit_sm_pat_fail_dispatch(FILE *out, int pc)     { (void)pc; edp4_label_then(out, emit_sm_pat_fail);     return 0; }
static int emit_sm_pat_abort_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_abort);    return 0; }
static int emit_sm_pat_succeed_dispatch(FILE *out, int pc)  { (void)pc; edp4_label_then(out, emit_sm_pat_succeed);  return 0; }
static int emit_sm_pat_bal_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_bal);      return 0; }
static int emit_sm_pat_eps_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_eps);      return 0; }
static int emit_sm_pat_cat_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_cat);      return 0; }
static int emit_sm_pat_alt_dispatch(FILE *out, int pc)      { (void)pc; edp4_label_then(out, emit_sm_pat_alt);      return 0; }
static int emit_sm_pat_deref_dispatch(FILE *out, int pc)    { (void)pc; edp4_label_then(out, emit_sm_pat_deref);    return 0; }

#define PHASE2_SIM_DEPTH  128
typedef struct {
    DESCR_t val;
    int     is_pat;
    int     is_variant;
} SimVal;
typedef struct {
    SimVal slots[PHASE2_SIM_DEPTH];
    int    top;
} SimStack;

static void simstack_init(SimStack *ss) { ss->top = 0; }

static void simstack_push(SimStack *ss, SimVal v)
{
    if (ss->top < PHASE2_SIM_DEPTH) ss->slots[ss->top++] = v;
}

static SimVal simstack_pop(SimStack *ss)
{
    if (ss->top > 0) return ss->slots[--ss->top];
    SimVal v; v.val = pat_epsilon(); v.is_pat = 1; v.is_variant = 1;
    return v;
}

static void simstack_push_const_s(SimStack *ss, const char *s)
{
    SimVal v;
    v.val.v  = DT_S;
    v.val.s  = s;
    v.val.i  = 0;
    v.is_pat = 0;
    v.is_variant = 0;
    simstack_push(ss, v);
}

static void simstack_push_const_i(SimStack *ss, int64_t n)
{
    SimVal v;
    v.val.v  = DT_I;
    v.val.i  = n;
    v.val.s  = NULL;
    v.is_pat = 0;
    v.is_variant = 0;
    simstack_push(ss, v);
}

static void simstack_push_variant_val(SimStack *ss)
{
    SimVal v;
    v.val = pat_epsilon();
    v.is_pat = 0;
    v.is_variant = 1;
    simstack_push(ss, v);
}

static SimVal make_pat_val(DESCR_t d, int is_variant)
{
    SimVal v;
    v.val = d;
    v.is_pat = 1;
    v.is_variant = is_variant;
    return v;
}

int emit_flat_eligible(const PATND_t *p)
{
    if (!p) return 1;
    return p->kind != XVAR;
}

int emit_flat_invariant(const PATND_t *p)
{
    if (!p) return 1;
    if (!flat_is_eligible_node(p)) return 0;
    if (p->kind == XCAT && p->nchildren > 2) return 0;
    for (int i = 0; i < p->nchildren; i++)
        if (!patnd_is_fully_invariant(p->children[i])) return 0;
    return 1;
}

static PATND_t *patnd_of(DESCR_t d)
{
    if (d.v != DT_P || !d.s) return NULL;
    return (PATND_t *)d.s;
}

DESCR_t emit_walk_phase2(const SM_Program *prog,
                            int phase2_start, int phase2_end,
                            int *out_variant)
{
    SimStack ss;
    simstack_init(&ss);
    int has_variant = 0;
    for (int pc = phase2_start; pc < phase2_end; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];
        switch (ins->op) {
        case SM_PUSH_LIT_S:
            simstack_push_const_s(&ss, ins->a[0].s ? ins->a[0].s : "");
            break;
        case SM_PUSH_LIT_I:
            simstack_push_const_i(&ss, ins->a[0].i);
            break;
        case SM_PUSH_VAR:
            simstack_push_variant_val(&ss);
            has_variant = 1;
            break;
        case SM_PAT_EPS:
            simstack_push(&ss, make_pat_val(pat_epsilon(), 0));
            break;
        case SM_PAT_ARB:
            simstack_push(&ss, make_pat_val(pat_arb(), 0));
            break;
        case SM_PAT_REM:
            simstack_push(&ss, make_pat_val(pat_rem(), 0));
            break;
        case SM_PAT_FAIL:
            simstack_push(&ss, make_pat_val(pat_fail(), 0));
            break;
        case SM_PAT_SUCCEED:
            simstack_push(&ss, make_pat_val(pat_succeed(), 0));
            break;
        case SM_PAT_ABORT:
            simstack_push(&ss, make_pat_val(pat_abort(), 0));
            break;
        case SM_PAT_BAL:
            simstack_push(&ss, make_pat_val(pat_bal(), 0));
            break;
        case SM_PAT_FENCE0:
            simstack_push(&ss, make_pat_val(pat_fence(), 0));
            break;
        case SM_PAT_LIT: {
            const char *s = ins->a[0].s ? ins->a[0].s : "";
            simstack_push(&ss, make_pat_val(pat_lit(s), 0));
            break;
        }
        case SM_PAT_SPAN: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            simstack_push(&ss, make_pat_val(pat_span(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_BREAK: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            simstack_push(&ss, make_pat_val(pat_break_(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_ANY: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            simstack_push(&ss, make_pat_val(pat_any_cs(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_NOTANY: {
            SimVal arg = simstack_pop(&ss);
            const char *cs = (arg.val.v == DT_S && arg.val.s) ? arg.val.s : "";
            simstack_push(&ss, make_pat_val(pat_notany(cs), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_LEN: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            simstack_push(&ss, make_pat_val(pat_len(n), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_POS: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            int v = arg.is_variant;
            simstack_push(&ss, make_pat_val(pat_pos(n), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_RPOS: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            int v = arg.is_variant;
            simstack_push(&ss, make_pat_val(pat_rpos(n), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_TAB: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            simstack_push(&ss, make_pat_val(pat_tab(n), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_RTAB: {
            SimVal arg = simstack_pop(&ss);
            int64_t n = (arg.val.v == DT_I) ? arg.val.i : 0;
            simstack_push(&ss, make_pat_val(pat_rtab(n), arg.is_variant));
            if (arg.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_ARBNO: {
            SimVal inner = simstack_pop(&ss);
            int v = inner.is_variant;
            simstack_push(&ss, make_pat_val(pat_arbno(inner.val), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_FENCE1: {
            SimVal inner = simstack_pop(&ss);
            simstack_push(&ss, make_pat_val(pat_fence_p(inner.val), inner.is_variant));
            if (inner.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_CAT: {
            SimVal right = simstack_pop(&ss);
            SimVal left  = simstack_pop(&ss);
            int v = left.is_variant | right.is_variant;
            simstack_push(&ss, make_pat_val(pat_cat(left.val, right.val), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_ALT: {
            SimVal right = simstack_pop(&ss);
            SimVal left  = simstack_pop(&ss);
            int v = left.is_variant | right.is_variant;
            simstack_push(&ss, make_pat_val(pat_alt(left.val, right.val), v));
            if (v) has_variant = 1;
            break;
        }
        case SM_PAT_DEREF: {
            SimVal arg = simstack_pop(&ss);
            DESCR_t d;
            if (arg.val.v == DT_S && arg.val.s)
                d = pat_ref(arg.val.s);
            else
                d = pat_epsilon();
            simstack_push(&ss, make_pat_val(d, 0));
            break;
        }
        case SM_PAT_REFNAME: {
            const char *name = ins->a[0].s ? ins->a[0].s : "";
            simstack_push(&ss, make_pat_val(pat_ref(name), 0));
            break;
        }
        case SM_PAT_CAPTURE: {
            SimVal child = simstack_pop(&ss);
            const char *vname = ins->a[0].s ? ins->a[0].s : "";
            int kind = (int)ins->a[1].i;
            DESCR_t var = NAME_fn(vname);
            DESCR_t d;
            if (kind == 1) d = pat_assign_imm(child.val, var);
            else           d = pat_assign_cond(child.val, var);
            simstack_push(&ss, make_pat_val(d, child.is_variant));
            if (child.is_variant) has_variant = 1;
            break;
        }
        case SM_PAT_CAPTURE_FN:
        case SM_PAT_CAPTURE_FN_ARGS:
        case SM_PAT_USERCALL:
        case SM_PAT_USERCALL_ARGS: {
            SimVal child = simstack_pop(&ss);
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            DESCR_t d = pat_assign_callcap(child.val, fname, NULL, 0);
            simstack_push(&ss, make_pat_val(d, 1));
            has_variant = 1;
            break;
        }
        default:
            simstack_push_variant_val(&ss);
            has_variant = 1;
            break;
        }
    }
    *out_variant = has_variant;
    if (ss.top == 0) return pat_epsilon();
    return ss.slots[ss.top - 1].val;
}

#define MAX_PATTERN_WINDOWS 4096
typedef struct {
    int  phase2_start;
    int  phase2_end;
    int  exec_stmt_pc;
    int  pat_id;
    int  is_invariant;
    DESCR_t root;
} pattern_window_t;
static pattern_window_t g_pat_windows[MAX_PATTERN_WINDOWS];
static int              g_pat_windows_n   = 0;
static int              g_pat_windows_id  = 0;
static void pattern_windows_reset(void)
{
    g_pat_windows_n  = 0;
    g_pat_windows_id = 0;
    cap_fixups_reset();
}

static int pattern_window_at_pc(int pc)
{
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (pc >= g_pat_windows[i].phase2_start &&
            pc <  g_pat_windows[i].phase2_end)
            return i;
    }
    return -1;
}

static int pattern_window_for_exec_stmt(int pc)
{
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (g_pat_windows[i].exec_stmt_pc == pc)
            return i;
    }
    return -1;
}

static void pattern_windows_collect(const SM_Program *prog)
{
    pattern_windows_reset();
    pc_used_alloc(prog);
    int stmt_start = 0;
    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];
        switch (ins->op) {
        case SM_JUMP:
        case SM_JUMP_S:
        case SM_JUMP_F:
        case SM_PUSH_EXPRESSION:
        case SM_CALL_EXPRESSION:
            pc_used_mark((int)ins->a[0].i);
            break;
        case SM_LABEL:
            if (ins->a[0].s && *ins->a[0].s)
                pc_used_mark(pc + 1);
            break;
        default:
            break;
        }
        if (ins->op == SM_STNO) {
            stmt_start = pc + 1;
            continue;
        }
        if (ins->op != SM_EXEC_STMT) continue;
        int phase2_end = pc - 2;
        if (phase2_end < stmt_start) phase2_end = stmt_start;
        int has_variant = 0;
        DESCR_t root = sm_phase2_to_patnd(prog, stmt_start, phase2_end, &has_variant);
        if (g_pat_windows_n >= MAX_PATTERN_WINDOWS) {
            continue;
        }
        pattern_window_t *w = &g_pat_windows[g_pat_windows_n++];
        w->phase2_start = stmt_start;
        w->phase2_end   = phase2_end;
        w->exec_stmt_pc = pc;
        w->pat_id       = g_pat_windows_id++;
        w->root         = root;
        PATND_t *p = (PATND_t *)root.p;
        w->is_invariant = (!has_variant && p && patnd_is_fully_invariant(p)) ? 1 : 0;
    }
}

static int emit_pattern_blobs(FILE *out)
{
    int n_invariant = 0;
    for (int i = 0; i < g_pat_windows_n; i++) {
        if (g_pat_windows[i].is_invariant) n_invariant++;
    }
    if (n_invariant == 0) return 0;
    emit_flat_set_intern_str(codegen_intern_str);
    emit_flat_set_cap_fixup(cap_fixup_add);
    emit_flat_reset();
    if (emit_three_column_line(out, "", ".intel_syntax", "noprefix", NULL) != 0) return -1;
    if (emit_three_column_line(out, "", ".text", "", NULL) != 0) return -1;
    for (int i = 0; i < g_pat_windows_n; i++) {
        pattern_window_t *w = &g_pat_windows[i];
        if (!w->is_invariant) continue;
        char prefix[64];
        snprintf(prefix, sizeof(prefix), "pat_inv_%d", w->pat_id);
        PATND_t *p = (PATND_t *)w->root.p;
        if (emit_flat_build(p, out, prefix) != 0) {
            w->is_invariant = 0;
        }
    }
    bb3c_flush_pending();
    return 0;
}

static int emit_sm_exec_stmt_blob(FILE *out, const SM_Instr *ins, int pc, int win_idx)
{
    pattern_window_t *w = &g_pat_windows[win_idx];
    const char *sname = ins->a[0].s;
    int has_repl      = (int)ins->a[1].i;
    char act[160];
    snprintf(act, sizeof(act),
             "rdi, [rip + pat_inv_%d_α]", w->pat_id);
    const char *anno = NULL;
    char lbl[32];
    const char *pending = emit_sm_consume_pc_label();
    if (pending && *pending) {
        size_t n = strlen(pending);
        if (n >= sizeof(lbl)) n = sizeof(lbl) - 1;
        memcpy(lbl, pending, n);
        lbl[n] = '\0';
    } else {
        lbl[0] = '\0';
    }
    if (emit_three_column_line(out, lbl, "lea", act, anno) != 0) return -1;
    if (sname && *sname) {
        char lbl_str[64];
        strtab_label(lbl_str, sizeof(lbl_str), sname);
        char act2[160];
        snprintf(act2, sizeof(act2), "rsi, [rip + %s]", lbl_str);
        if (emit_three_column_line(out, "", "lea", act2, NULL) != 0) return -1;
    } else {
        if (emit_three_column_line(out, "", "xor", "esi, esi", NULL) != 0) return -1;
    }
    char act3[80];
    snprintf(act3, sizeof(act3), "edx, %d", has_repl);
    if (emit_three_column_line(out, "", "mov", act3, NULL) != 0) return -1;
    if (emit_three_column_line(out, "", "call", "rt_match_blob@PLT", NULL) != 0) return -1;
    (void)pc;
    return 0;
}

static int emit_sm_pat_baked(FILE *out, const SM_Instr *ins, int pc, int win_idx)
{
    pattern_window_t *w = &g_pat_windows[win_idx];
    const sm_op_template_t *t = sm_template_lookup(ins->op);
    const char *opname = (t && t->macro_name) ? t->macro_name
                                              : sm_opcode_name(ins->op);
    if (!opname) opname = "?";
    char lbl[32];
    const char *pending = emit_sm_consume_pc_label();
    if (pending && *pending) {
        size_t n = strlen(pending);
        if (n >= sizeof(lbl)) n = sizeof(lbl) - 1;
        memcpy(lbl, pending, n);
        lbl[n] = '\0';
    } else {
        lbl[0] = '\0';
    }
    char op_col[24];
    snprintf(op_col, sizeof(op_col), "# %s", opname);
    char col3[160];
    snprintf(col3, sizeof(col3),
             "baked  pat_inv_%d pc=%d..%d",
             w->pat_id, w->phase2_start, w->phase2_end - 1);
    if (emit_three_column_line(out, lbl, op_col, col3, NULL) != 0) return -1;
    (void)pc;
    return 0;
}

static const char *pat_arg_label(char *lbl_buf, size_t lbl_buf_n,
                                 const char *arg)
{
    if (!arg || !*arg) return NULL;
    strtab_label(lbl_buf, lbl_buf_n, arg);
    return lbl_buf;
}

static int emit_sm_pat_lit_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "arg=\"%.40s\"%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_LIT), l, anno);
    }
    return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_LIT), l, NULL);
}

static int emit_sm_pat_refname_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "%.40s%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_REFNAME), l, anno);
    }
    return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_REFNAME), l, NULL);
}

static int emit_sm_pat_capture_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[80];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    int kind = (int)ins->a[1].i;
    if (l) {
        snprintf(anno, sizeof(anno), "%s kind=%d", ins->a[0].s, kind);
    } else {
        snprintf(anno, sizeof(anno), "kind=%d", kind);
    }
    return emit_sm_lblopt_int32(out, sm_template_lookup(SM_PAT_CAPTURE),
                                l, kind, anno);
}

static int emit_sm_pat_capture_fn_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char fname_lbl[64], nl_lbl[64], anno[160];
    const char *fl = pat_arg_label(fname_lbl, sizeof(fname_lbl), ins->a[0].s);
    const char *nl = pat_arg_label(nl_lbl,    sizeof(nl_lbl),    ins->a[2].s);
    int is_imm = (int)ins->a[1].i;
    snprintf(anno, sizeof(anno),
             "%s, %s",
             fl ? ins->a[0].s : "(NULL)",
             nl ? ins->a[2].s : "(NULL)");
    return emit_sm_capture_fn(out, sm_template_lookup(SM_PAT_CAPTURE_FN),
                              fl, is_imm, nl, anno);
}

static int emit_sm_pat_capture_fn_args_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char fname_lbl[64], anno[128];
    const char *fl = pat_arg_label(fname_lbl, sizeof(fname_lbl), ins->a[0].s);
    int is_imm = (int)ins->a[1].i;
    int nargs  = (int)ins->a[2].i;
    snprintf(anno, sizeof(anno), "%s",
             fl ? ins->a[0].s : "(NULL)");
    return emit_sm_capture_fn_args(out,
                                   sm_template_lookup(SM_PAT_CAPTURE_FN_ARGS),
                                   fl, is_imm, nargs, anno);
}

static int emit_sm_pat_usercall_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    if (l) {
        snprintf(anno, sizeof(anno), "%.40s%s",
                 ins->a[0].s, (strlen(ins->a[0].s) > 40) ? "..." : "");
        return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_USERCALL), l, anno);
    }
    return emit_sm_lblopt(out, sm_template_lookup(SM_PAT_USERCALL), l, NULL);
}

static int emit_sm_pat_usercall_args_dispatch(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char lbl[64], anno[128];
    const char *l = pat_arg_label(lbl, sizeof(lbl), ins->a[0].s);
    int nargs = (int)ins->a[1].i;
    if (l) {
        snprintf(anno, sizeof(anno), "%.40s", ins->a[0].s);
        return emit_sm_lblopt_int32(out, sm_template_lookup(SM_PAT_USERCALL_ARGS),
                                    l, nargs, anno);
    }
    return emit_sm_lblopt_int32(out, sm_template_lookup(SM_PAT_USERCALL_ARGS),
                                l, nargs, NULL);
}

static int emit_sm_pat_noarg(FILE *out, sm_opcode_t op, int pc)
{
    (void)pc;
    const sm_op_template_t *t = sm_template_lookup(op);
    if (!t) return -1;
    return emit_sm_rtcall(out, t, NULL);
}

static int emit_sm_exec_stmt_variant(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    const char *sname = ins->a[0].s;
    int has_repl      = (int)ins->a[1].i;
    char lbl[64];
    const char *l = pat_arg_label(lbl, sizeof(lbl), sname);
    char anno[128];
    if (l) {
        snprintf(anno, sizeof(anno), "subj=%s", sname);
    } else {
        anno[0] = '\0';
    }
    emit_sm_args_t a = { 0 };
    a.lbl   = l;
    a.i32_a = has_repl;
    a.anno  = anno[0] ? anno : NULL;
    return emit_sm_template(out, sm_template_lookup(SM_EXEC_STMT), &a);
}

static int edp4_sm_unhandled(FILE *out, const SM_Instr *ins, int pc)
{
    (void)pc;
    char anno[64];
    snprintf(anno, sizeof(anno), "%s", sm_opcode_name(ins->op));
    emit_sm_args_t a = { 0 };
    a.i32_a = (int)ins->op;
    a.anno  = anno;
    return emit_sm_template(out, sm_template_unhandled(), &a);
}

int emit_walk_codegen(SM_Program *prog, FILE *out, const char *src_path)
{
    assert(prog != NULL);
    assert(out  != NULL);
    if (!g_emit_inline) {
        if (emit_sm_macro_library_to_path("sm_macros.s") != 0) {
            fprintf(stderr,
                    "sm_codegen_text: failed to write sm_macros.s "
                    "(working directory writable?)\n");
            return -1;
        }
        if (emit_three_column_line(out, "", ".include", "\"sm_macros.s\"", NULL) != 0) return -1;
        if (emit_bb_macro_library_to_path("bb_macros.s") != 0) {
            fprintf(stderr, "sm_codegen_text: failed to write bb_macros.s\n");
            return -1;
        }
        if (emit_three_column_line(out, "", ".include", "\"bb_macros.s\"", NULL) != 0) return -1;
    }
    strtab_collect(prog);
    if (strtab_emit_rodata(out) != 0) return -1;
    int expression_reg_count = emit_expression_registry(out, prog);
    if (expression_reg_count < 0) return -1;
    pattern_windows_collect(prog);
    if (emit_pattern_blobs(out) != 0) return -1;
    SrcLines sl;
    int sl_loaded = (srclines_load(&sl, src_path) == 0);
    if (emit_file_header(out, prog->count, expression_reg_count > 0) != 0) {
        if (sl_loaded) srclines_free(&sl);
        return -1;
    }
    for (int pc = 0; pc < prog->count; pc++) {
        const SM_Instr *ins = &prog->instrs[pc];
        {
            const char *leftover = emit_sm_consume_pc_label();
            if (leftover && *leftover) {
                bb3c_format(out, leftover, "", "");
            }
            if (pc_is_used_as_target(pc)) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), ".L%d:", pc);
                emit_sm_set_pc_label(lbl);
            } else {
                emit_sm_set_pc_label("");
            }
        }
        {
            int win_at  = pattern_window_at_pc(pc);
            int win_exec = (ins->op == SM_EXEC_STMT) ? pattern_window_for_exec_stmt(pc) : -1;
            if (win_at >= 0 && g_pat_windows[win_at].is_invariant) {
                int rc = emit_sm_pat_baked(out, ins, pc, win_at);
                if (rc != 0) {
                    if (sl_loaded) srclines_free(&sl);
                    return -1;
                }
                continue;
            }
            if (win_exec >= 0 && g_pat_windows[win_exec].is_invariant) {
                int rc = emit_sm_exec_stmt_blob(out, ins, pc, win_exec);
                if (rc != 0) {
                    if (sl_loaded) srclines_free(&sl);
                    return -1;
                }
                continue;
            }
        }
        int rc;
        switch (ins->op) {
            case SM_HALT:         rc = emit_halt_line(out, pc);          break;
            case SM_PUSH_LIT_I:   rc = emit_push_lit_i_line(out, ins, pc); break;
            case SM_PUSH_LIT_F:   rc = emit_sm_push_lit_f_dispatch(out, ins, pc);  break;
            case SM_PUSH_EXPR:    rc = emit_sm_push_expr_dispatch(out, ins, pc);   break;
            case SM_PUSH_LIT_S:   rc = emit_sm_push_lit_s_dispatch(out, ins, pc); break;
            case SM_PUSH_VAR:     rc = emit_sm_push_var_dispatch(out, ins, pc);   break;
            case SM_STORE_VAR:    rc = emit_sm_store_var_dispatch(out, ins, pc);  break;
            case SM_VOID_POP:          rc = emit_sm_pop(out, pc);             break;
            case SM_ADD:
            case SM_SUB:
            case SM_MUL:
            case SM_DIV:
            case SM_MOD:          rc = edp4_sm_arith(out, ins, pc);      break;
            case SM_LABEL:        rc = emit_sm_label_dispatch(out, ins, pc);      break;
            case SM_JUMP:         rc = emit_sm_jump_line(out, ins, pc);   break;
            case SM_JUMP_S:       rc = emit_sm_jump_s_line(out, ins, pc); break;
            case SM_JUMP_F:       rc = emit_sm_jump_f_line(out, ins, pc); break;
            case SM_PUSH_EXPRESSION:   rc = emit_sm_push_expression_dispatch(out, ins, pc); break;
            case SM_CALL_EXPRESSION:   rc = emit_sm_call_expression_dispatch(out, ins, pc); break;
            case SM_RETURN:       rc = emit_sm_return_dispatch(out, pc);          break;
            case SM_DEFINE_ENTRY: rc = emit_sm_define_entry_dispatch(out, ins, pc, prog); break;
            case SM_DEFINE:       rc = emit_sm_define_dispatch(out, ins, pc);              break;
            case SM_CALL_FN:         rc = emit_sm_call_dispatch(out, ins, pc); break;
            case SM_CONCAT:       rc = emit_sm_concat_dispatch(out, pc);      break;
            case SM_PUSH_NULL:    rc = emit_sm_push_null_dispatch(out, pc);   break;
            case SM_PUSH_NULL_NOFLIP: rc = emit_sm_push_null_noflip_dispatch(out, pc); break;
            case SM_COERCE_NUM:   rc = emit_sm_coerce_num_dispatch(out, pc);  break;
            case SM_INCR:         rc = emit_sm_incr_dispatch(out, ins, pc);   break;
            case SM_DECR:         rc = emit_sm_decr_dispatch(out, ins, pc);   break;
            case SM_ACOMP:        rc = emit_sm_acomp_dispatch(out, ins, pc);  break;
            case SM_LCOMP:        rc = emit_sm_lcomp_dispatch(out, ins, pc);  break;
            case SM_FRETURN:
            case SM_NRETURN:
            case SM_RETURN_S:
            case SM_RETURN_F:
            case SM_FRETURN_S:
            case SM_FRETURN_F:
            case SM_NRETURN_S:
            case SM_NRETURN_F:    rc = emit_sm_return_variant_dispatch(out, ins->op, pc); break;
            case SM_STNO:         rc = emit_sm_stno_dispatch(out, ins, pc,
                                                    sl_loaded ? &sl : NULL); break;
            case SM_PAT_LIT:      rc = emit_sm_pat_lit_dispatch(out, ins, pc);     break;
            case SM_PAT_REFNAME:  rc = emit_sm_pat_refname_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE:      rc = emit_sm_pat_capture_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN:   rc = emit_sm_pat_capture_fn_dispatch(out, ins, pc); break;
            case SM_PAT_CAPTURE_FN_ARGS: rc = emit_sm_pat_capture_fn_args_dispatch(out, ins, pc); break;
            case SM_PAT_USERCALL:     rc = emit_sm_pat_usercall_dispatch(out, ins, pc); break;
            case SM_PAT_USERCALL_ARGS: rc = emit_sm_pat_usercall_args_dispatch(out, ins, pc); break;
            case SM_PAT_SPAN:    rc = emit_sm_pat_span_dispatch(out, pc);    break;
            case SM_PAT_BREAK:   rc = emit_sm_pat_break_dispatch(out, pc);   break;
            case SM_PAT_ANY:     rc = emit_sm_pat_any_dispatch(out, pc);     break;
            case SM_PAT_NOTANY:  rc = emit_sm_pat_notany_dispatch(out, pc);  break;
            case SM_PAT_LEN:     rc = emit_sm_pat_len_dispatch(out, pc);     break;
            case SM_PAT_POS:     rc = emit_sm_pat_pos_dispatch(out, pc);     break;
            case SM_PAT_RPOS:    rc = emit_sm_pat_rpos_dispatch(out, pc);    break;
            case SM_PAT_TAB:     rc = emit_sm_pat_tab_dispatch(out, pc);     break;
            case SM_PAT_RTAB:    rc = emit_sm_pat_rtab_dispatch(out, pc);    break;
            case SM_PAT_ARB:     rc = emit_sm_pat_arb_dispatch(out, pc);     break;
            case SM_PAT_ARBNO:   rc = emit_sm_pat_arbno_dispatch(out, pc);   break;
            case SM_PAT_REM:     rc = emit_sm_pat_rem_dispatch(out, pc);     break;
            case SM_PAT_FENCE0:  rc = emit_sm_pat_fence0_dispatch(out, pc);  break;
            case SM_PAT_FENCE1:  rc = emit_sm_pat_fence1_dispatch(out, pc);  break;
            case SM_PAT_FAIL:    rc = emit_sm_pat_fail_dispatch(out, pc);    break;
            case SM_PAT_ABORT:   rc = emit_sm_pat_abort_dispatch(out, pc);   break;
            case SM_PAT_SUCCEED: rc = emit_sm_pat_succeed_dispatch(out, pc); break;
            case SM_PAT_BAL:     rc = emit_sm_pat_bal_dispatch(out, pc);     break;
            case SM_PAT_EPS:     rc = emit_sm_pat_eps_dispatch(out, pc);     break;
            case SM_PAT_CAT:     rc = emit_sm_pat_cat_dispatch(out, pc);     break;
            case SM_PAT_ALT:     rc = emit_sm_pat_alt_dispatch(out, pc);     break;
            case SM_PAT_DEREF:   rc = emit_sm_pat_deref_dispatch(out, pc);   break;
            case SM_EXEC_STMT:    rc = emit_sm_exec_stmt_variant(out, ins, pc); break;
            default:              rc = edp4_sm_unhandled(out, ins, pc);  break;
        }
        if (rc != 0) {
            if (sl_loaded) srclines_free(&sl);
            return -1;
        }
    }
    {
        const char *leftover = emit_sm_consume_pc_label();
        if (leftover && *leftover) {
            bb3c_format(out, leftover, "", "");
        }
    }
    int frc = emit_file_footer(out);
    bb3c_flush_pending();
    if (sl_loaded) srclines_free(&sl);
    release_pc_used_as_target();
    return frc;
}
