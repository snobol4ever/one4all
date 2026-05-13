#include "emitter.h"
#include "bb_emit.h"
#include "templates.h"
#include "sm_prog.h"

static void emit_sm_rtcall    (const char *macro, const char *rt_sym);
static void emit_sm_lbl_rt    (const char *macro, const char *rt_sym, const char *lbl, uint64_t ptr);
static void emit_sm_pat_lbl_rt(const char *macro, const char *rt_sym, const char *lbl, uint64_t ptr);
static void make_pc_label(bb_label_t *lbl, int target_pc);

/*---- SM_ARITH family (ADD/SUB/MUL/DIV/MOD) -----------------------------------------------------------------------*/
void emit_sm_arith_op(int op_enum, const char *macro_name)
{
    emit_macro_begin(macro_name ? macro_name : "ARITH", NULL, 0);
    bb_emit_mov_rdi_imm64((uint64_t)(unsigned)op_enum);
    bb_emit_call_sym_plt("rt_arith", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
void emit_sm_add() { emit_sm_arith_op(SM_ADD, "ADD_NUM"); }
void emit_sm_sub() { emit_sm_arith_op(SM_SUB, "SUB_NUM"); }
void emit_sm_mul() { emit_sm_arith_op(SM_MUL, "MUL_NUM"); }
void emit_sm_div() { emit_sm_arith_op(SM_DIV, "DIV_NUM"); }
void emit_sm_mod() { emit_sm_arith_op(SM_MOD, "MOD_NUM"); }
/*====================================================================================================================*/
void emit_sm_acomp(int op)
{
    static const char *const params[] = { "op" };
    emit_macro_begin("ACOMP", params, 1);
    emit_mov_edi_imm32(op);
    bb_emit_call_sym_plt("rt_acomp", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_lcomp(int op)
{
    static const char *const params[] = { "op" };
    emit_macro_begin("LCOMP", params, 1);
    emit_mov_edi_imm32(op);
    bb_emit_call_sym_plt("rt_lcomp", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_incr(int64_t n)
{
    static const char *const params[] = { "n" };
    emit_macro_begin("INCR", params, 1);
    bb_emit_mov_rdi_imm64((uint64_t)n);
    bb_emit_call_sym_plt("rt_incr", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_decr(int64_t n)
{
    static const char *const params[] = { "n" };
    emit_macro_begin("DECR", params, 1);
    bb_emit_mov_rdi_imm64((uint64_t)n);
    bb_emit_call_sym_plt("rt_decr", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_halt()
{
    emit_bb_inc_mem_r13_disp8(20);
    bb_emit_ret();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_void_pop()
{
    emit_macro_begin("VOID_POP", NULL, 0);
    bb_emit_call_sym_plt("rt_pop_void", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_return()
{
    emit_macro_begin("RETURN", NULL, 0);
    bb_emit_ret();
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_return_variant(int kind, int cond, int pc)
{
    static const char *const params[] = { "kind", "cond", "pc" };
    emit_macro_begin("RETURN_VARIANT", params, 3);
    emit_mov_edi_imm32(kind);
    bb_emit_mov_esi_imm32(cond);
    bb_emit_call_sym_plt("rt_do_return", 0);
    bb_emit_test_eax_eax();
    emit_jz_retskip(pc);
    bb_emit_ret();
    emit_retskip_label(pc);
    emit_macro_end();
    emit_pad_to_blob_size();
}
void emit_sm_freturn  (int pc) { emit_sm_return_variant(1, 0, pc); }
void emit_sm_nreturn  (int pc) { emit_sm_return_variant(2, 0, pc); }
void emit_sm_return_s (int pc) { emit_sm_return_variant(0, 1, pc); }
void emit_sm_return_f (int pc) { emit_sm_return_variant(0, 2, pc); }
void emit_sm_freturn_s(int pc) { emit_sm_return_variant(1, 1, pc); }
void emit_sm_freturn_f(int pc) { emit_sm_return_variant(1, 2, pc); }
void emit_sm_nreturn_s(int pc) { emit_sm_return_variant(2, 1, pc); }
void emit_sm_nreturn_f(int pc) { emit_sm_return_variant(2, 2, pc); }
/*====================================================================================================================*/
static void make_pc_label(bb_label_t *lbl, int target_pc) { bb_label_initf(lbl, ".L%d", target_pc); }
void emit_sm_jump  (int target_pc)
{
    bb_label_t tgt; make_pc_label(&tgt, target_pc);
    emit_jmp(&tgt, JMP_JMP);
}
void emit_sm_jump_s(int target_pc)
{
    bb_emit_call_sym_plt("rt_last_ok", 0);
    bb_emit_test_rax_rax();
    bb_label_t tgt; make_pc_label(&tgt, target_pc);
    emit_jmp(&tgt, JMP_JNE);
}
void emit_sm_jump_f(int target_pc)
{
    bb_emit_call_sym_plt("rt_last_ok", 0);
    bb_emit_test_rax_rax();
    bb_label_t tgt; make_pc_label(&tgt, target_pc);
    emit_jmp(&tgt, JMP_JE);
}
/*====================================================================================================================*/
void emit_sm_label()                                          { emit_noop_macro("LABEL"); }
void emit_sm_stno (int stno, int lineno, const char *src)   { emit_banner_stno(stno, lineno, src); emit_noop_macro("STNO"); }
/*====================================================================================================================*/
void emit_sm_push_lit_i(int64_t val)
{
    static const char *const params[] = { "val" };
    emit_macro_begin("PUSH_INT", params, 1);
    bb_emit_mov_rdi_imm64((uint64_t)val);
    bb_emit_call_sym_plt("rt_push_int", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_push_lit_f(double val)
{
    static const char *const params[] = { "val" };
    emit_macro_begin("PUSH_REAL", params, 1);
    uint64_t bits; __builtin_memcpy(&bits, &val, 8);
    bb_emit_mov_rdi_imm64(bits);
    bb_emit_call_sym_plt("rt_push_real_bits", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_push_lit_s(const char *str_lbl, uint64_t str_ptr, int len)
{
    static const char *const params[] = { "lbl", "n" };
    emit_macro_begin("PUSH_STR", params, 2);
    emit_lea_rdi_strtab_sym(str_lbl, str_ptr);
    bb_emit_mov_esi_imm32(len);
    bb_emit_call_sym_plt("rt_push_str", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_push_expr(uint64_t ptr_val)
{
    static const char *const params[] = { "ptr" };
    emit_macro_begin("PUSH_EXPR", params, 1);
    bb_emit_mov_rdi_imm64(ptr_val);
    bb_emit_call_sym_plt("rt_push_expr", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_push_expression(uint64_t entry_ptr, int arity)
{
    static const char *const params[] = { "entry", "arity" };
    emit_macro_begin("PUSH_EXPRESSION", params, 2);
    emit_movabs_rdi_entry(entry_ptr);
    bb_emit_mov_esi_imm32(arity);
    bb_emit_call_sym_plt("rt_push_expression_descr", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
void emit_sm_call_expression(const char *tgt_sym)
{
    static const char *const params[] = { "tgt" };
    emit_macro_begin("CALL_EXPRESSION", params, 1);
    emit_call_sym_param(tgt_sym);
    emit_macro_end();
    emit_pad_to_blob_size();
}
void emit_sm_exec_stmt(const char *subj_lbl, uint64_t subj_ptr, int has_repl)
{
    static const char *const params[] = { "has_repl", "subj_lbl" };
    emit_macro_begin("EXEC_STMT_VARIANT", params, 2);
    emit_lea_rdi_strtab_sym(subj_lbl, subj_ptr);
    bb_emit_mov_esi_imm32(has_repl);
    bb_emit_call_sym_plt("rt_match_variant", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_call_fn(const char *name_lbl, uint64_t name_ptr, int nargs)
{
    (void)name_lbl; (void)name_ptr; (void)nargs;
    static const char *const params[] = { "lbl", "n" };
    emit_macro_begin("CALL_FN", params, 2);
    emit_lea_rdi_strtab_sym(NULL, 0);
    bb_emit_mov_esi_imm32(0);
    bb_emit_call_sym_plt("rt_call", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_pat_capture(const char *name_lbl, uint64_t name_ptr, int kind)
{
    static const char *const params[] = { "lbl", "n" };
    emit_macro_begin("PAT_CAPTURE", params, 2);
    emit_lea_rdi_strtab_sym(name_lbl, name_ptr);
    bb_emit_mov_esi_imm32(kind);
    bb_emit_call_sym_plt("rt_pat_capture", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
void emit_sm_pat_usercall_args(const char *name_lbl, uint64_t name_ptr, int nargs)
{
    static const char *const params[] = { "lbl", "n" };
    emit_macro_begin("PAT_USERCALL_ARGS", params, 2);
    emit_lea_rdi_strtab_sym(name_lbl, name_ptr);
    bb_emit_mov_esi_imm32(nargs);
    bb_emit_call_sym_plt("rt_pat_usercall_args", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_pat_capture_fn(const char *fname_lbl, uint64_t fname_ptr,
                             int is_imm, const char *namelist_lbl, uint64_t namelist_ptr)
{
    static const char *const params[] = { "is_imm", "fname_lbl", "namelist_lbl" };
    emit_macro_begin("PAT_CAPTURE_FN", params, 3);
    emit_lea_rdi_strtab_sym(fname_lbl, fname_ptr);
    bb_emit_mov_esi_imm32(is_imm);
    emit_lea_rdx_strtab_sym(namelist_lbl, namelist_ptr);
    bb_emit_call_sym_plt("rt_pat_capture_fn", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
void emit_sm_pat_capture_fn_args(const char *fname_lbl, uint64_t fname_ptr,
                                  int is_imm, int nargs)
{
    static const char *const params[] = { "is_imm", "nargs", "fname_lbl" };
    emit_macro_begin("PAT_CAPTURE_FN_ARGS", params, 3);
    emit_lea_rdi_strtab_sym(fname_lbl, fname_ptr);
    bb_emit_mov_esi_imm32(is_imm);
    emit_mov_edx_imm32(nargs);
    bb_emit_call_sym_plt("rt_pat_capture_fn_args", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_pat_lit      (const char *lbl, uint64_t ptr) { emit_sm_pat_lbl_rt("PAT_LIT",      "rt_pat_lit",      lbl, ptr); }
void emit_sm_pat_refname  (const char *lbl, uint64_t ptr) { emit_sm_pat_lbl_rt("PAT_REFNAME",  "rt_pat_refname",  lbl, ptr); }
void emit_sm_pat_usercall (const char *lbl, uint64_t ptr) { emit_sm_pat_lbl_rt("PAT_USERCALL", "rt_pat_usercall", lbl, ptr); }
/*====================================================================================================================*/
void emit_sm_pat_eps    () { emit_sm_rtcall("PAT_EPS",     "rt_pat_eps");     }
void emit_sm_pat_arb    () { emit_sm_rtcall("PAT_ARB",     "rt_pat_arb");     }
void emit_sm_pat_rem    () { emit_sm_rtcall("PAT_REM",     "rt_pat_rem");     }
void emit_sm_pat_fail   () { emit_sm_rtcall("PAT_FAIL",    "rt_pat_fail");    }
void emit_sm_pat_succeed() { emit_sm_rtcall("PAT_SUCCEED", "rt_pat_succeed"); }
void emit_sm_pat_abort  () { emit_sm_rtcall("PAT_ABORT",   "rt_pat_abort");   }
void emit_sm_pat_bal    () { emit_sm_rtcall("PAT_BAL",     "rt_pat_bal");     }
void emit_sm_pat_fence  () { emit_sm_rtcall("PAT_FENCE",   "rt_pat_fence");   }
void emit_sm_pat_fence1 () { emit_sm_rtcall("PAT_FENCE1",  "rt_pat_fence1");  }
void emit_sm_pat_span   () { emit_sm_rtcall("PAT_SPAN",    "rt_pat_span");    }
void emit_sm_pat_break  () { emit_sm_rtcall("PAT_BREAK",   "rt_pat_break");   }
void emit_sm_pat_any    () { emit_sm_rtcall("PAT_ANY",     "rt_pat_any");     }
void emit_sm_pat_notany () { emit_sm_rtcall("PAT_NOTANY",  "rt_pat_notany");  }
void emit_sm_pat_len    () { emit_sm_rtcall("PAT_LEN",     "rt_pat_len");     }
void emit_sm_pat_pos    () { emit_sm_rtcall("PAT_POS",     "rt_pat_pos");     }
void emit_sm_pat_rpos   () { emit_sm_rtcall("PAT_RPOS",    "rt_pat_rpos");    }
void emit_sm_pat_tab    () { emit_sm_rtcall("PAT_TAB",     "rt_pat_tab");     }
void emit_sm_pat_rtab   () { emit_sm_rtcall("PAT_RTAB",    "rt_pat_rtab");    }
void emit_sm_pat_arbno  () { emit_sm_rtcall("PAT_ARBNO",   "rt_pat_arbno");   }
void emit_sm_pat_cat    () { emit_sm_rtcall("PAT_CAT",     "rt_pat_cat");     }
void emit_sm_pat_alt    () { emit_sm_rtcall("PAT_ALT",     "rt_pat_alt");     }
void emit_sm_pat_deref  () { emit_sm_rtcall("PAT_DEREF",   "rt_pat_deref");   }
/*====================================================================================================================*/
void emit_sm_push_var  (const char *lbl, uint64_t ptr) { emit_sm_lbl_rt("PUSH_VAR",  "rt_nv_get", lbl, ptr); }
void emit_sm_store_var (const char *lbl, uint64_t ptr) { emit_sm_lbl_rt("STORE_VAR", "rt_nv_set", lbl, ptr); }
/*====================================================================================================================*/
void emit_sm_concat         () { emit_sm_rtcall("CONCAT",         "rt_concat");          }
void emit_sm_push_null      () { emit_sm_rtcall("PUSH_NULL",      "rt_push_null");       }
void emit_sm_push_null_noflip(){ emit_sm_rtcall("PUSH_NULL_NOFLIP","rt_push_null_noflip");}
void emit_sm_coerce_num     () { emit_sm_rtcall("COERCE_NUM",     "rt_coerce_num");      }
void emit_sm_exp            () { emit_sm_rtcall("EXP_NUM",        "rt_exp");             }
void emit_sm_neg            () { emit_sm_rtcall("NEGATE",         "rt_neg");             }
void emit_sm_define         () { emit_sm_rtcall("DEFINE",         "rt_define");          }
void emit_sm_define_entry   () { emit_sm_rtcall("DEFINE_ENTRY",   "rt_define_entry");    }
/*====================================================================================================================*/
void emit_sm_resume         () { emit_sm_rtcall("RESUME",         "rt_unhandled_sm"); }
void emit_sm_suspend        () { emit_sm_rtcall("SUSPEND",        "rt_unhandled_sm"); }
void emit_sm_suspend_value  () { emit_sm_rtcall("SUSPEND_VALUE",  "rt_unhandled_sm"); }
void emit_sm_gen_tick       () { emit_sm_rtcall("GEN_TICK",       "rt_unhandled_sm"); }
void emit_sm_load_glocal    () { emit_sm_rtcall("LOAD_GLOCAL",    "rt_unhandled_sm"); }
void emit_sm_store_glocal   () { emit_sm_rtcall("STORE_GLOCAL",   "rt_unhandled_sm"); }
void emit_sm_load_frame     () { emit_sm_rtcall("LOAD_FRAME",     "rt_unhandled_sm"); }
void emit_sm_store_frame    () { emit_sm_rtcall("STORE_FRAME",    "rt_unhandled_sm"); }
void emit_sm_icmp_gt        () { emit_sm_rtcall("ICMP_GT",        "rt_unhandled_sm"); }
void emit_sm_icmp_lt        () { emit_sm_rtcall("ICMP_LT",        "rt_unhandled_sm"); }
void emit_sm_bb_once        () { emit_sm_rtcall("BB_ONCE",        "rt_unhandled_sm"); }
void emit_sm_bb_once_proc   () { emit_sm_rtcall("BB_ONCE_PROC",   "rt_unhandled_sm"); }
void emit_sm_bb_pump        () { emit_sm_rtcall("BB_PUMP",        "rt_unhandled_sm"); }
void emit_sm_bb_pump_case   () { emit_sm_rtcall("BB_PUMP_CASE",   "rt_unhandled_sm"); }
void emit_sm_bb_pump_every  () { emit_sm_rtcall("BB_PUMP_EVERY",  "rt_unhandled_sm"); }
void emit_sm_bb_pump_proc   () { emit_sm_rtcall("BB_PUMP_PROC",   "rt_unhandled_sm"); }
void emit_sm_bb_pump_sm     () { emit_sm_rtcall("BB_PUMP_SM",     "rt_unhandled_sm"); }
void emit_sm_bb_pump_ast    () { emit_sm_rtcall("BB_PUMP_AST",    "rt_bb_pump_ast"); }
/*====================================================================================================================*/
void emit_sm_unhandled_op(int op)
{
    static const char *const params[] = { "op" };
    emit_macro_begin("UNHANDLED", params, 1);
    emit_mov_edi_imm32(op);
    bb_emit_call_sym_plt("rt_unhandled_op", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
static void emit_sm_rtcall(const char *macro, const char *rt_sym)
{
    emit_macro_begin(macro, NULL, 0);
    bb_emit_call_sym_plt(rt_sym, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
static void emit_sm_lbl_rt(const char *macro, const char *rt_sym,
                             const char *name_lbl, uint64_t name_ptr)
{
    static const char *const params[] = { "lbl" };
    emit_macro_begin(macro, params, 1);
    emit_lea_rdi_strtab_sym(name_lbl, name_ptr);
    bb_emit_call_sym_plt(rt_sym, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
static void emit_sm_pat_lbl_rt(const char *macro, const char *rt_sym,
                                 const char *name_lbl, uint64_t name_ptr)
{
    static const char *const params[] = { "lbl" };
    emit_macro_begin(macro, params, 1);
    emit_lea_rdi_strtab_sym(name_lbl, name_ptr);
    bb_emit_call_sym_plt(rt_sym, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
