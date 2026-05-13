#include "emitter.h"
#include "bb_emit.h"
#include "templates.h"
#include "sm_prog.h"

static void emit_sm_rtcall    (const char *comment, const char *macro, const char *rt_sym);
static void emit_sm_lbl_rt    (const char *comment, const char *macro, const char *rt_sym, const char *lbl, uint64_t ptr);
static void emit_sm_pat_lbl_rt(const char *comment, const char *macro, const char *rt_sym, const char *lbl, uint64_t ptr);
static void make_pc_label(bb_label_t *lbl, int target_pc);

/*---- SM_ARITH family (ADD/SUB/MUL/DIV/MOD) -----------------------------------------------------------------------*/
void emit_sm_arith_op(int op_enum, const char *macro_name)
{
    emit_comment(macro_name ? macro_name : "SM_ARITH");
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
    emit_comment("SM_ACOMP — numeric compare, op=EKind");
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
    emit_comment("SM_LCOMP — lexicographic string compare, op=EKind");
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
    emit_comment("SM_INCR — increment TOS by immediate n");
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
    emit_comment("SM_DECR — decrement TOS by immediate n");
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
    emit_comment("SM_HALT — exit sm_jit_run via ret");
    emit_bb_inc_mem_r13_disp8(20);
    bb_emit_ret();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_void_pop()
{
    emit_comment("SM_VOID_POP — pop and discard TOS");
    emit_macro_begin("VOID_POP", NULL, 0);
    bb_emit_call_sym_plt("rt_pop_void", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_return()
{
    emit_comment("SM_RETURN — native return");
    emit_macro_begin("RETURN", NULL, 0);
    bb_emit_ret();
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
void emit_sm_return_variant(int kind, int cond, int pc)
{
    emit_comment("SM_RETURN_VARIANT — conditional/typed return via rt_do_return");
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
    emit_comment("SM_JUMP — unconditional jump");
    bb_label_t tgt; make_pc_label(&tgt, target_pc);
    emit_jmp(&tgt, JMP_JMP);
}
void emit_sm_jump_s(int target_pc)
{
    emit_comment("SM_JUMP_S — jump if last_ok");
    bb_emit_call_sym_plt("rt_last_ok", 0);
    bb_emit_test_rax_rax();
    bb_label_t tgt; make_pc_label(&tgt, target_pc);
    emit_jmp(&tgt, JMP_JNE);
}
void emit_sm_jump_f(int target_pc)
{
    emit_comment("SM_JUMP_F — jump if not last_ok");
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
    emit_comment("SM_PUSH_LIT_I — push integer literal");
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
    emit_comment("SM_PUSH_LIT_F — push real literal");
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
    emit_comment("SM_PUSH_LIT_S — push string literal via rt_push_str(s, len)");
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
    emit_comment("SM_PUSH_EXPR — push frozen DT_E expression descriptor");
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
    emit_comment("SM_PUSH_EXPRESSION — push expression descriptor (entry, arity)");
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
    emit_comment("SM_CALL_EXPRESSION — call expression chunk directly");
    static const char *const params[] = { "tgt" };
    emit_macro_begin("CALL_EXPRESSION", params, 1);
    emit_call_sym_param(tgt_sym);
    emit_macro_end();
    emit_pad_to_blob_size();
}
void emit_sm_exec_stmt(const char *subj_lbl, uint64_t subj_ptr, int has_repl)
{
    emit_comment("SM_EXEC_STMT — execute pattern statement via rt_match_variant");
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
    emit_comment("SM_CALL_FN — call named function via rt_call(name, nargs)");
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
    emit_comment("SM_PAT_CAPTURE — pop child, push capture(varname, kind)");
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
    emit_comment("SM_PAT_USERCALL_ARGS — push *func(args) user-call pattern");
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
    emit_comment("SM_PAT_CAPTURE_FN — pop child, push . *func() / $ *func() capture");
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
    emit_comment("SM_PAT_CAPTURE_FN_ARGS — pop child, push *func(args) capture");
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
void emit_sm_pat_lit      (const char *lbl, uint64_t ptr) { emit_sm_pat_lbl_rt("SM_PAT_LIT — push literal-string match pattern",        "PAT_LIT",      "rt_pat_lit",      lbl, ptr); }
void emit_sm_pat_refname  (const char *lbl, uint64_t ptr) { emit_sm_pat_lbl_rt("SM_PAT_REFNAME — push *varname pattern (deref by name)", "PAT_REFNAME",  "rt_pat_refname",  lbl, ptr); }
void emit_sm_pat_usercall (const char *lbl, uint64_t ptr) { emit_sm_pat_lbl_rt("SM_PAT_USERCALL — push *func() user-call pattern",       "PAT_USERCALL", "rt_pat_usercall", lbl, ptr); }
/*====================================================================================================================*/
void emit_sm_pat_eps    () { emit_sm_rtcall("SM_PAT_EPS — push epsilon pattern",                    "PAT_EPS",     "rt_pat_eps");     }
void emit_sm_pat_arb    () { emit_sm_rtcall("SM_PAT_ARB — push ARB (greedy 0+) pattern",            "PAT_ARB",     "rt_pat_arb");     }
void emit_sm_pat_rem    () { emit_sm_rtcall("SM_PAT_REM — push REM (rest of subject) pattern",      "PAT_REM",     "rt_pat_rem");     }
void emit_sm_pat_fail   () { emit_sm_rtcall("SM_PAT_FAIL — push FAIL (always fail) pattern",        "PAT_FAIL",    "rt_pat_fail");    }
void emit_sm_pat_succeed() { emit_sm_rtcall("SM_PAT_SUCCEED — push SUCCEED (always succeed) pat",   "PAT_SUCCEED", "rt_pat_succeed"); }
void emit_sm_pat_abort  () { emit_sm_rtcall("SM_PAT_ABORT — push ABORT (terminate match) pattern",  "PAT_ABORT",   "rt_pat_abort");   }
void emit_sm_pat_bal    () { emit_sm_rtcall("SM_PAT_BAL — push BAL (balanced string) pattern",      "PAT_BAL",     "rt_pat_bal");     }
void emit_sm_pat_fence  () { emit_sm_rtcall("SM_PAT_FENCE0 — push FENCE (no-backtrack gate)",       "PAT_FENCE",   "rt_pat_fence");   }
void emit_sm_pat_fence1 () { emit_sm_rtcall("SM_PAT_FENCE1 — pop child, push FENCE(child)",         "PAT_FENCE1",  "rt_pat_fence1");  }
void emit_sm_pat_span   () { emit_sm_rtcall("SM_PAT_SPAN — pop charset, push SPAN(cs)",             "PAT_SPAN",    "rt_pat_span");    }
void emit_sm_pat_break  () { emit_sm_rtcall("SM_PAT_BREAK — pop charset, push BREAK(cs)",           "PAT_BREAK",   "rt_pat_break");   }
void emit_sm_pat_any    () { emit_sm_rtcall("SM_PAT_ANY — pop charset, push ANY(cs)",               "PAT_ANY",     "rt_pat_any");     }
void emit_sm_pat_notany () { emit_sm_rtcall("SM_PAT_NOTANY — pop charset, push NOTANY(cs)",         "PAT_NOTANY",  "rt_pat_notany");  }
void emit_sm_pat_len    () { emit_sm_rtcall("SM_PAT_LEN — pop integer n, push LEN(n)",              "PAT_LEN",     "rt_pat_len");     }
void emit_sm_pat_pos    () { emit_sm_rtcall("SM_PAT_POS — pop integer n, push POS(n)",              "PAT_POS",     "rt_pat_pos");     }
void emit_sm_pat_rpos   () { emit_sm_rtcall("SM_PAT_RPOS — pop integer n, push RPOS(n)",            "PAT_RPOS",    "rt_pat_rpos");    }
void emit_sm_pat_tab    () { emit_sm_rtcall("SM_PAT_TAB — pop integer n, push TAB(n)",              "PAT_TAB",     "rt_pat_tab");     }
void emit_sm_pat_rtab   () { emit_sm_rtcall("SM_PAT_RTAB — pop integer n, push RTAB(n)",            "PAT_RTAB",    "rt_pat_rtab");    }
void emit_sm_pat_arbno  () { emit_sm_rtcall("SM_PAT_ARBNO — pop child pattern, push ARBNO(child)",  "PAT_ARBNO",   "rt_pat_arbno");   }
void emit_sm_pat_cat    () { emit_sm_rtcall("SM_PAT_CAT — pop right+left patterns, push CAT(l,r)",  "PAT_CAT",     "rt_pat_cat");     }
void emit_sm_pat_alt    () { emit_sm_rtcall("SM_PAT_ALT — pop right+left patterns, push ALT(l,r)",  "PAT_ALT",     "rt_pat_alt");     }
void emit_sm_pat_deref  () { emit_sm_rtcall("SM_PAT_DEREF — pop value, deref to pattern",           "PAT_DEREF",   "rt_pat_deref");   }
/*====================================================================================================================*/
void emit_sm_push_var  (const char *lbl, uint64_t ptr) { emit_sm_lbl_rt("SM_PUSH_VAR — push value of named variable via rt_nv_get",   "PUSH_VAR",  "rt_nv_get", lbl, ptr); }
void emit_sm_store_var (const char *lbl, uint64_t ptr) { emit_sm_lbl_rt("SM_STORE_VAR — store TOS into named variable via rt_nv_set", "STORE_VAR", "rt_nv_set", lbl, ptr); }
/*====================================================================================================================*/
void emit_sm_concat         () { emit_sm_rtcall("SM_CONCAT — pop right+left, push concat result",         "CONCAT",         "rt_concat");          }
void emit_sm_push_null      () { emit_sm_rtcall("SM_PUSH_NULL — push null descriptor",                    "PUSH_NULL",      "rt_push_null");       }
void emit_sm_push_null_noflip(){ emit_sm_rtcall("SM_PUSH_NULL_NOFLIP — push null, preserve last_ok",      "PUSH_NULL_NOFLIP","rt_push_null_noflip");}
void emit_sm_coerce_num     () { emit_sm_rtcall("SM_COERCE_NUM — coerce TOS string to number",            "COERCE_NUM",     "rt_coerce_num");      }
void emit_sm_exp            () { emit_sm_rtcall("SM_EXP — pop 2, push base**exp",                         "EXP_NUM",        "rt_exp");             }
void emit_sm_neg            () { emit_sm_rtcall("SM_NEG — negate TOS",                                    "NEGATE",         "rt_neg");             }
void emit_sm_define         () { emit_sm_rtcall("SM_DEFINE — no-op: function definition prescan",         "DEFINE",         "rt_define");          }
void emit_sm_define_entry   () { emit_sm_rtcall("SM_DEFINE_ENTRY — no-op: function entry marker",         "DEFINE_ENTRY",   "rt_define_entry");    }
/*====================================================================================================================*/
void emit_sm_resume         () { emit_sm_rtcall("SM_RESUME — generator resume marker (M5)",               "RESUME",         "rt_unhandled_sm"); }
void emit_sm_suspend        () { emit_sm_rtcall("SM_SUSPEND — generator suspend (M5)",                    "SUSPEND",        "rt_unhandled_sm"); }
void emit_sm_suspend_value  () { emit_sm_rtcall("SM_SUSPEND_VALUE — coroutine yield (M5)",                "SUSPEND_VALUE",  "rt_unhandled_sm"); }
void emit_sm_gen_tick       () { emit_sm_rtcall("SM_GEN_TICK — drive generator one tick (M5)",            "GEN_TICK",       "rt_unhandled_sm"); }
void emit_sm_load_glocal    () { emit_sm_rtcall("SM_LOAD_GLOCAL — push gen-local slot (M5)",              "LOAD_GLOCAL",    "rt_unhandled_sm"); }
void emit_sm_store_glocal   () { emit_sm_rtcall("SM_STORE_GLOCAL — pop into gen-local slot (M5)",         "STORE_GLOCAL",   "rt_unhandled_sm"); }
void emit_sm_load_frame     () { emit_sm_rtcall("SM_LOAD_FRAME — push IcnFrame slot (M5)",                "LOAD_FRAME",     "rt_unhandled_sm"); }
void emit_sm_store_frame    () { emit_sm_rtcall("SM_STORE_FRAME — pop into IcnFrame slot (M5)",           "STORE_FRAME",    "rt_unhandled_sm"); }
void emit_sm_icmp_gt        () { emit_sm_rtcall("SM_ICMP_GT — integer compare > (M5)",                   "ICMP_GT",        "rt_unhandled_sm"); }
void emit_sm_icmp_lt        () { emit_sm_rtcall("SM_ICMP_LT — integer compare < (M5)",                   "ICMP_LT",        "rt_unhandled_sm"); }
void emit_sm_bb_once        () { emit_sm_rtcall("SM_BB_ONCE — run BB once (M5)",                          "BB_ONCE",        "rt_unhandled_sm"); }
void emit_sm_bb_once_proc   () { emit_sm_rtcall("SM_BB_ONCE_PROC — Prolog proc BB once (M5)",             "BB_ONCE_PROC",   "rt_unhandled_sm"); }
void emit_sm_bb_pump        () { emit_sm_rtcall("SM_BB_PUMP — drive BB generator (M5)",                   "BB_PUMP",        "rt_unhandled_sm"); }
void emit_sm_bb_pump_case   () { emit_sm_rtcall("SM_BB_PUMP_CASE — Raku case BB pump (M5)",               "BB_PUMP_CASE",   "rt_unhandled_sm"); }
void emit_sm_bb_pump_every  () { emit_sm_rtcall("SM_BB_PUMP_EVERY — every-generator BB pump (M5)",        "BB_PUMP_EVERY",  "rt_unhandled_sm"); }
void emit_sm_bb_pump_proc   () { emit_sm_rtcall("SM_BB_PUMP_PROC — Icon proc BB pump (M5)",               "BB_PUMP_PROC",   "rt_unhandled_sm"); }
void emit_sm_bb_pump_sm     () { emit_sm_rtcall("SM_BB_PUMP_SM — migrated SM BB pump (M5)",               "BB_PUMP_SM",     "rt_unhandled_sm"); }
void emit_sm_bb_pump_ast    () { emit_sm_rtcall("SM_BB_PUMP_AST — drive BB generator via AST pump",       "BB_PUMP_AST",    "rt_bb_pump_ast"); }
/*====================================================================================================================*/
void emit_sm_unhandled_op(int op)
{
    emit_comment("SM_UNHANDLED — trap for unimplemented opcode");
    static const char *const params[] = { "op" };
    emit_macro_begin("UNHANDLED", params, 1);
    emit_mov_edi_imm32(op);
    bb_emit_call_sym_plt("rt_unhandled_op", 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
static void emit_sm_rtcall(const char *comment, const char *macro, const char *rt_sym)
{
    emit_comment(comment);
    emit_macro_begin(macro, NULL, 0);
    bb_emit_call_sym_plt(rt_sym, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
static void emit_sm_lbl_rt(const char *comment, const char *macro, const char *rt_sym,
                             const char *name_lbl, uint64_t name_ptr)
{
    emit_comment(comment);
    static const char *const params[] = { "lbl" };
    emit_macro_begin(macro, params, 1);
    emit_lea_rdi_strtab_sym(name_lbl, name_ptr);
    bb_emit_call_sym_plt(rt_sym, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
static void emit_sm_pat_lbl_rt(const char *comment, const char *macro, const char *rt_sym,
                                 const char *name_lbl, uint64_t name_ptr)
{
    emit_comment(comment);
    static const char *const params[] = { "lbl" };
    emit_macro_begin(macro, params, 1);
    emit_lea_rdi_strtab_sym(name_lbl, name_ptr);
    bb_emit_call_sym_plt(rt_sym, 0);
    emit_macro_end();
    emit_pad_to_blob_size();
}
/*====================================================================================================================*/
