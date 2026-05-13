#include "emitter.h"
#include "bb_emit.h"
#include "templates.h"
#include "sm_prog.h"

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
    uint64_t bits; __builtin_memcpy(&bits, &val, 8);
    char val_str[32]; snprintf(val_str, sizeof(val_str), "%g", val);
    const char *params[] = { val_str };
    emit_macro_begin("PUSH_REAL", params, 1);
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
void emit_sm_pat_lit(const char *lbl, uint64_t ptr)      { static const char *const p[] = {"lbl"}; emit_macro_begin("PAT_LIT", p, 1); emit_lea_rdi_strtab_sym(lbl, ptr); bb_emit_call_sym_plt("rt_pat_lit", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_refname(const char *lbl, uint64_t ptr)  { static const char *const p[] = {"lbl"}; emit_macro_begin("PAT_REFNAME", p, 1); emit_lea_rdi_strtab_sym(lbl, ptr); bb_emit_call_sym_plt("rt_pat_refname", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_usercall(const char *lbl, uint64_t ptr) { static const char *const p[] = {"lbl"}; emit_macro_begin("PAT_USERCALL", p, 1); emit_lea_rdi_strtab_sym(lbl, ptr); bb_emit_call_sym_plt("rt_pat_usercall", 0); emit_macro_end(); emit_pad_to_blob_size(); }
/*====================================================================================================================*/
void emit_sm_pat_eps()    { emit_macro_begin("PAT_EPS", NULL, 0); bb_emit_call_sym_plt("rt_pat_eps", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_arb()    { emit_macro_begin("PAT_ARB", NULL, 0); bb_emit_call_sym_plt("rt_pat_arb", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_rem()    { emit_macro_begin("PAT_REM", NULL, 0); bb_emit_call_sym_plt("rt_pat_rem", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_fail()   { emit_macro_begin("PAT_FAIL", NULL, 0); bb_emit_call_sym_plt("rt_pat_fail", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_succeed(){ emit_macro_begin("PAT_SUCCEED", NULL, 0); bb_emit_call_sym_plt("rt_pat_succeed", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_abort()  { emit_macro_begin("PAT_ABORT", NULL, 0); bb_emit_call_sym_plt("rt_pat_abort", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_bal()    { emit_macro_begin("PAT_BAL", NULL, 0); bb_emit_call_sym_plt("rt_pat_bal", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_fence()  { emit_macro_begin("PAT_FENCE", NULL, 0); bb_emit_call_sym_plt("rt_pat_fence", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_fence1() { emit_macro_begin("PAT_FENCE1", NULL, 0); bb_emit_call_sym_plt("rt_pat_fence1", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_span()   { emit_macro_begin("PAT_SPAN", NULL, 0); bb_emit_call_sym_plt("rt_pat_span", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_break()  { emit_macro_begin("PAT_BREAK", NULL, 0); bb_emit_call_sym_plt("rt_pat_break", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_any()    { emit_macro_begin("PAT_ANY", NULL, 0); bb_emit_call_sym_plt("rt_pat_any", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_notany() { emit_macro_begin("PAT_NOTANY", NULL, 0); bb_emit_call_sym_plt("rt_pat_notany", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_len()    { emit_macro_begin("PAT_LEN", NULL, 0); bb_emit_call_sym_plt("rt_pat_len", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_pos()    { emit_macro_begin("PAT_POS", NULL, 0); bb_emit_call_sym_plt("rt_pat_pos", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_rpos()   { emit_macro_begin("PAT_RPOS", NULL, 0); bb_emit_call_sym_plt("rt_pat_rpos", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_tab()    { emit_macro_begin("PAT_TAB", NULL, 0); bb_emit_call_sym_plt("rt_pat_tab", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_rtab()   { emit_macro_begin("PAT_RTAB", NULL, 0); bb_emit_call_sym_plt("rt_pat_rtab", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_arbno()  { emit_macro_begin("PAT_ARBNO", NULL, 0); bb_emit_call_sym_plt("rt_pat_arbno", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_cat()    { emit_macro_begin("PAT_CAT", NULL, 0); bb_emit_call_sym_plt("rt_pat_cat", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_alt()    { emit_macro_begin("PAT_ALT", NULL, 0); bb_emit_call_sym_plt("rt_pat_alt", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_pat_deref()  { emit_macro_begin("PAT_DEREF", NULL, 0); bb_emit_call_sym_plt("rt_pat_deref", 0); emit_macro_end(); emit_pad_to_blob_size(); }
/*====================================================================================================================*/
void emit_sm_push_var(const char *lbl, uint64_t ptr)  { static const char *const p[] = {"lbl"}; emit_macro_begin("PUSH_VAR", p, 1); emit_lea_rdi_strtab_sym(lbl, ptr); bb_emit_call_sym_plt("rt_nv_get", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_store_var(const char *lbl, uint64_t ptr) { static const char *const p[] = {"lbl"}; emit_macro_begin("STORE_VAR", p, 1); emit_lea_rdi_strtab_sym(lbl, ptr); bb_emit_call_sym_plt("rt_nv_set", 0); emit_macro_end(); emit_pad_to_blob_size(); }
/*====================================================================================================================*/
void emit_sm_concat()         { emit_macro_begin("CONCAT", NULL, 0); bb_emit_call_sym_plt("rt_concat", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_push_null()      { emit_macro_begin("PUSH_NULL", NULL, 0); bb_emit_call_sym_plt("rt_push_null", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_push_null_noflip(){ emit_macro_begin("PUSH_NULL_NOFLIP", NULL, 0); bb_emit_call_sym_plt("rt_push_null_noflip", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_coerce_num()     { emit_macro_begin("COERCE_NUM", NULL, 0); bb_emit_call_sym_plt("rt_coerce_num", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_exp()            { emit_macro_begin("EXP_NUM", NULL, 0); bb_emit_call_sym_plt("rt_exp", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_neg()            { emit_macro_begin("NEGATE", NULL, 0); bb_emit_call_sym_plt("rt_neg", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_define()         { emit_macro_begin("DEFINE", NULL, 0); bb_emit_call_sym_plt("rt_define", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_define_entry()   { emit_macro_begin("DEFINE_ENTRY", NULL, 0); bb_emit_call_sym_plt("rt_define_entry", 0); emit_macro_end(); emit_pad_to_blob_size(); }
/*====================================================================================================================*/
void emit_sm_resume()         { emit_macro_begin("RESUME", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_suspend()        { emit_macro_begin("SUSPEND", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_suspend_value()  { emit_macro_begin("SUSPEND_VALUE", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_gen_tick()       { emit_macro_begin("GEN_TICK", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_load_glocal()    { emit_macro_begin("LOAD_GLOCAL", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_store_glocal()   { emit_macro_begin("STORE_GLOCAL", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_load_frame()     { emit_macro_begin("LOAD_FRAME", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_store_frame()    { emit_macro_begin("STORE_FRAME", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_icmp_gt()        { emit_macro_begin("ICMP_GT", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_icmp_lt()        { emit_macro_begin("ICMP_LT", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_once()        { emit_macro_begin("BB_ONCE", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_once_proc()   { emit_macro_begin("BB_ONCE_PROC", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_pump()        { emit_macro_begin("BB_PUMP", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_pump_case()   { emit_macro_begin("BB_PUMP_CASE", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_pump_every()  { emit_macro_begin("BB_PUMP_EVERY", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_pump_proc()   { emit_macro_begin("BB_PUMP_PROC", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_pump_sm()     { emit_macro_begin("BB_PUMP_SM", NULL, 0); bb_emit_call_sym_plt("rt_unhandled_sm", 0); emit_macro_end(); emit_pad_to_blob_size(); }
void emit_sm_bb_pump_ast()    { emit_macro_begin("BB_PUMP_AST", NULL, 0); bb_emit_call_sym_plt("rt_bb_pump_ast", 0); emit_macro_end(); emit_pad_to_blob_size(); }
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
/*====================================================================================================================*/
