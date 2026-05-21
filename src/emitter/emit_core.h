/* emit_core.h — L0/L1/L2 emitter types, globals, and leaf insn declarations */
#ifndef EMIT_CORE_H
#define EMIT_CORE_H
#define TEXT_MODE_INVOCATION  0
#define TEXT_MODE_DEFINITION  1
#include "bb_pool.h"
#include "x86_opcodes.h"
#include "BB.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
/*--- emit mode -----------------------------------------------------------*/
typedef enum {
    EMIT_TEXT             = 0,   /* x86-64 GAS text   */
    EMIT_BINARY_WIRED     = 1,   /* x86-64 binary, wired BBs    */
    EMIT_BINARY_BROKERED  = 2,   /* x86-64 binary, brokered BBs */
    EMIT_MACRO_DEF        = 3,   /* GAS macro library emission  */
    EMIT_TEXT_INLINE      = 4,   /* x86-64 GAS text, inline mode */
    EMIT_JVM              = 5,   /* JVM Jasmin text   */
    EMIT_JS               = 6,   /* JavaScript text   */
    EMIT_NET              = 7,   /* MSIL .NET text    */
    EMIT_WASM             = 8    /* WASM WAT text     */
} bb_emit_mode_t;
#define EMIT_BINARY     EMIT_BINARY_WIRED
#define IS_TEXT     (bb_emit_mode != EMIT_BINARY_WIRED && bb_emit_mode != EMIT_BINARY_BROKERED)
#define IS_BIN      (bb_emit_mode == EMIT_BINARY_WIRED || bb_emit_mode == EMIT_BINARY_BROKERED)
#define IS_WIRED    (bb_emit_mode == EMIT_BINARY_WIRED)
#define IS_BROKERED (bb_emit_mode == EMIT_BINARY_BROKERED)
/* EC-UNI matrix: one IS_<BE> per backend (5 columns).  Text-vs-binary —
 * and the various x86 text sub-modes — are serializer choices INSIDE each
 * backend's output layer, NOT matrix columns.  See GOAL-HEADQUARTERS §
 * AXIS CORRECTION.
 *
 * IS_X86 covers all FIVE x86 modes uniformly:
 *   EMIT_TEXT             — GAS text (normal invocation)
 *   EMIT_TEXT_INLINE      — GAS text (inline form for hot paths)
 *   EMIT_MACRO_DEF        — GAS macro-definition pass
 *   EMIT_BINARY_WIRED     — binary machine code, wired BBs
 *   EMIT_BINARY_BROKERED  — binary machine code, brokered BBs
 * The dispatcher below the template (e.g. emit_halt_line,
 * emit_sm_<op>_dispatch) consults bb_emit_mode and TEXT_MODE() to choose
 * the actual output form.  Each template fn carries exactly ONE arm
 * per macro below. */
#define IS_X86      (bb_emit_mode == EMIT_TEXT \
                  || bb_emit_mode == EMIT_TEXT_INLINE \
                  || bb_emit_mode == EMIT_MACRO_DEF \
                  || bb_emit_mode == EMIT_BINARY_WIRED \
                  || bb_emit_mode == EMIT_BINARY_BROKERED)
#define IS_JVM      (bb_emit_mode == EMIT_JVM)
#define IS_JS       (bb_emit_mode == EMIT_JS)
#define IS_NET      (bb_emit_mode == EMIT_NET)
#define IS_WASM     (bb_emit_mode == EMIT_WASM)
/*--- label ---------------------------------------------------------------*/
#define BB_LABEL_NAME_MAX   80
#define BB_LABEL_UNRESOLVED (-1)
#define EMIT_UNRESOLVED     BB_LABEL_UNRESOLVED
#define EMIT_LABEL_MAX      BB_LABEL_NAME_MAX
typedef struct { char name[BB_LABEL_NAME_MAX]; int offset; } bb_label_t;
#define bb_label_defined(lbl)  ((lbl)->offset != BB_LABEL_UNRESOLVED)
#define emit_label_ok(l)       bb_label_defined(l)
typedef enum { JMP_JMP = 0, JMP_JE, JMP_JNE, JMP_JL, JMP_JGE, JMP_JG } jmp_kind_t;
/*--- patch list ----------------------------------------------------------*/
#define BB_PATCH_MAX   512
#define EMIT_PATCH_MAX BB_PATCH_MAX
typedef enum { PATCH_REL8, PATCH_REL32 } bb_patch_kind_t;
typedef struct { int site; bb_label_t * label; bb_patch_kind_t kind; } bb_patch_t;
typedef int emitter_t;
/*--- globals -------------------------------------------------------------*/
extern bb_emit_mode_t  bb_emit_mode;
extern FILE          * bb_emit_out;
extern int             g_bb_emit_format;
extern int             g_in_text_macro_body;
extern bb_buf_t        bb_emit_buf;
extern int             bb_emit_pos;
extern int             bb_emit_size;
extern bb_patch_t      bb_patch_list[BB_PATCH_MAX];
extern int             bb_patch_count;
extern int             g_is_text;
extern int             g_emit_text_mode;
/*--- L0: raw buffer ------------------------------------------------------*/
void     bb_emit_begin      (bb_buf_t buf, int size);
int      bb_emit_end        (void);
void     bb_emit_patch_rel32(bb_label_t * lbl);
void     bb_emit_byte       (uint8_t b);
void     bb_emit_u32        (uint32_t v);
void     bb_emit_u64        (uint64_t v);
void     bb_emit_i32        (int32_t v);
/*--- L1: mode lifecycle --------------------------------------------------*/
void     emit_mode_set         (bb_emit_mode_t m, FILE * out);
FILE *   emit_outf             (void);
int      emit_bb_is_format_mode(void);
void     fmt_body_append       (const char * instr, const char * operands);
void     emit_pad_to_blob_size (void);
void     emit_macro_begin      (const char * name, const char * const * params, int nparams);
void     emit_macro_end        (void);
/*--- L2: 3-col text formatter --------------------------------------------*/
void     bb3c_format           (FILE * out, const char * label, const char * action, const char * goto_);
void     bb3c_emit_jmp         (FILE * out, const char * mn, const char * target);
void     bb3c_flush_pending    (void);
void     bb3c_flush_pending_cjmp_only(void);
/*--- L2: label lifecycle -------------------------------------------------*/
void     emit_jmp              (bb_label_t * target, jmp_kind_t kind);
void     emit_label_define     (bb_label_t * lbl);
void     emit_label_initf      (bb_label_t * lbl, const char * fmt, ...);
void     bb_label_define       (bb_label_t * lbl);
/*--- L2: name-taking label primitives (Snocone-shape, for BB_templates) --
   Input is the label NAME (string).  Text-mode only; binary mode is
   shape-only until a name-keyed offset table replaces bb_label_t.offset. */
void     emit_text_jmp         (const char * target_name, jmp_kind_t kind);
void     emit_text_label       (const char * lbl_name);
/*--- L2: text-only helpers -----------------------------------------------*/
void     emit_text_3col        (FILE * out, const char * label, const char * action, const char * goto_);
void     emit_text_rawf        (const char * fmt, ...);
void     emit_text_global      (const char * name);
void     emit_text_stno_banner (int stno, int lineno, const char * src_text);
void     emit_bb_box_banner    (const char * kind, const char * args);
void     emit_banner_stno      (int stno, int lineno, const char * src_text);
/*--- L2: sym/load helpers ------------------------------------------------*/
void     emit_sym_lea_rcx      (const char * sym, uint64_t addr);
void     emit_sym_lea_r10      (const char * sym, uint64_t addr);
void     emit_store_delta      (void);
void     emit_load_sigma       (uint64_t a);
void     emit_sigma_plus_delta (uint64_t a);
void     emit_label_define_bb  (bb_label_t * lbl);
void     emit_jmp_label        (bb_label_t * target, jmp_kind_t kind);
/*--- L1: insn leaf functions (one per x86 instruction) -------------------*/
void insn_mov_eax_i32       (uint32_t v);          void insn_mov_rax_i64       (uint64_t v);
void insn_mov_rcx_i64       (uint64_t v);          void insn_mov_rdx_i64       (uint64_t v);
void insn_mov_rsi_i64       (uint64_t v);          void insn_mov_rdi_i64       (uint64_t v);
void insn_mov_edx_i32       (uint32_t v);          void insn_mov_edi_i32       (uint32_t v);
void insn_mov_esi_i32       (int v);
void insn_mov_rbp_rsp       (void);                void insn_mov_rsp_rbp       (void);
void insn_mov_ecx_eax       (void);                void insn_mov_rdi_rax       (void);
void insn_mov_eax_r10mem    (void);                void insn_mov_eax_rcxmem    (void);
void insn_mov_rax_rcxmem    (void);
void insn_lea_rax_rax_rcx   (void);
void insn_movzx_eax_rdi_off8(uint8_t off);         void insn_movsxd_rcx_r10mem (void);
void insn_cmp_esi_i8        (uint8_t v);           void insn_cmp_esi_i32       (uint32_t v);
void insn_cmp_al_i8         (uint8_t v);           void insn_cmp_eax_i32       (uint32_t v);
void insn_cmp_eax_ecx       (void);                void insn_cmp_eax_rcxmem    (void);
void insn_sub_rsp_i8        (uint8_t v);           void insn_add_rsp_i8        (uint8_t v);
void insn_sub_eax_i32       (uint32_t v);          void insn_add_eax_i32       (uint32_t v);
void insn_add_delta_i       (int v);               void insn_sub_delta_i       (int v);
void insn_test_rax_rax      (void);                void insn_test_eax_eax      (void);
void insn_jmp_r8            (bb_label_t * t);      void insn_jmp_r32           (bb_label_t * t);
void insn_je_r8             (bb_label_t * t);      void insn_je_r32            (bb_label_t * t);
void insn_jne_r8            (bb_label_t * t);      void insn_jne_r32           (bb_label_t * t);
void insn_jl_r8             (bb_label_t * t);      void insn_jl_r32            (bb_label_t * t);
void insn_jge_r8            (bb_label_t * t);      void insn_jge_r32           (bb_label_t * t);
void insn_jg_r32            (bb_label_t * t);
void insn_push_rbp          (void);                void insn_pop_rbp           (void);
void insn_push_r10          (void);                void insn_pop_r10           (void);
void insn_push_r12          (void);                void insn_pop_r12           (void);
void insn_ret               (void);                void insn_nop               (void);
void insn_call_plt          (const char * sym, uint64_t fn_fallback);
/*--- L3: compound emit_seq helpers ---------------------------------------*/
void emit_seq_frame_enter      (void);             void emit_seq_frame_leave      (void);
void emit_seq_brokered_enter   (void);             void emit_seq_brokered_leave   (int result);
void emit_seq_lea_rsi_sym      (const char * sym, uint64_t ptr);
void emit_seq_mov_edx_i32      (int val);          void emit_seq_mov_edi_i32      (int val);
void emit_seq_cmp_delta_i      (int n, bb_label_t * lbl_succ, bb_label_t * lbl_fail);
void emit_seq_cmp_siglen_delta (int n, uint64_t siglen_addr, bb_label_t * lbl_succ, bb_label_t * lbl_fail);
void emit_seq_sigma_delta_rdi  (uint64_t sigma_addr, uint64_t siglen_addr);
void emit_seq_bounds_len       (int len, uint64_t siglen_addr, bb_label_t * lbl_fail);
void emit_seq_noop_macro       (const char * macro_name);
void emit_seq_port_call        (uint64_t zeta_ptr, const char * fn_name, uint64_t fn_fallback, int port, bb_label_t * lbl_succ, bb_label_t * lbl_fail);
void emit_seq_port_call_rip    (uint64_t zeta_ptr, const char * zeta_label, const char * fn_name, uint64_t fn_fallback, int port, bb_label_t * lbl_succ, bb_label_t * lbl_fail);
/*--- higher-level emit helpers -------------------------------------------*/
void emit_ret          (void);
void emit_push_r10     (void);                     void emit_pop_r10      (void);
void emit_test_rax_rax (void);                     void emit_test_eax_eax (void);
void emit_mov_rdi_imm64(uint64_t v);
void emit_call_sym_plt (const char * sym, uint64_t fn_fallback);
void emit_add_delta_imm(int v);                    void emit_sub_delta_imm(int v);
/*--- legacy bb_insn_* aliases (kept for callers not yet migrated) --------*/
void bb_insn_mov_eax_imm32    (uint32_t imm);      void bb_insn_mov_rax_imm64    (uint64_t imm);
void bb_insn_ret(void);  void bb_insn_nop(void);   void bb_insn_call_rax(void);
void bb_insn_jmp_rel8  (bb_label_t * t);           void bb_insn_jmp_rel32 (bb_label_t * t);
void bb_insn_jl_rel8   (bb_label_t * t);           void bb_insn_jge_rel8  (bb_label_t * t);
void bb_insn_je_rel8   (bb_label_t * t);           void bb_insn_jne_rel8  (bb_label_t * t);
void bb_insn_jne_rel32 (bb_label_t * t);           void bb_insn_je_rel32  (bb_label_t * t);
void bb_insn_jl_rel32  (bb_label_t * t);           void bb_insn_jge_rel32 (bb_label_t * t);
void bb_insn_cmp_esi_imm8     (uint8_t v);         void bb_insn_cmp_esi_imm32    (uint32_t v);
void bb_insn_movzx_eax_rdi_off8(uint8_t off);      void bb_insn_cmp_al_imm8      (uint8_t v);
void bb_insn_push_rbp(void);  void bb_insn_pop_rbp(void);
void bb_insn_mov_rbp_rsp(void); void bb_insn_mov_rsp_rbp(void);
void bb_insn_sub_rsp_imm8     (uint8_t v);         void bb_insn_add_rsp_imm8     (uint8_t v);
void bb_insn_mov_rdx_imm64    (uint64_t v);        void bb_insn_mov_edx_imm32    (uint32_t v);
void bb_insn_mov_edi_imm32    (uint32_t v);        void bb_insn_mov_rsi_imm64    (uint64_t v);
void bb_insn_push_r12(void);  void bb_insn_pop_r12(void);
void bb_insn_inc_r13_disp8    (uint8_t d);         void bb_insn_mov_rcx_imm64    (uint64_t v);
void bb_insn_mov_eax_r10mem   (void);              void bb_insn_cmp_eax_imm32    (uint32_t v);
void bb_insn_mov_eax_mem_rcx  (void);              void bb_insn_sub_eax_imm32    (uint32_t v);
void bb_insn_mov_ecx_eax      (void);              void bb_insn_cmp_eax_ecx      (void);
void bb_insn_mov_rax_mem_rcx  (void);              void bb_insn_movsxd_rcx_r10mem(void);
void bb_insn_lea_rax_rax_rcx  (void);              void bb_insn_mov_rdi_rax      (void);
void bb_insn_add_eax_imm32    (uint32_t v);        void bb_insn_cmp_eax_mem_rcx  (void);
/*--- EC unified BB node emitter (EC-2+) ------------------------------------*/
struct BB_t;
int emit_bb_node(struct BB_t * nd, FILE * out);
/*--- EC-3 JVM scalar helpers (promoted from static in emit_jvm.c) ----------*/
void jvm_push_int2(FILE * out, long v);
void jvm_emit_ldc_string(FILE * out, const char * s);
/*--- EC-3 SM push/pop literal templates (SM_templates/sm_push_pop_lits.c) — EC-UNI-10(c): parameterless ---*/
#include "SM.h"
void sm_push_lit_i(void);
void sm_push_lit_s(void);
void sm_push_lit_f(void);
void sm_push_null (void);
void sm_void_pop  (void);
void sm_push_var  (void);
void sm_store_var (void);
/*--- EC-3 SM arithmetic templates (SM_templates/sm_arith.c) — EC-UNI-10(c): parameterless ---*/
void sm_concat    (void);
void sm_neg       (void);
void sm_coerce_num(void);
void sm_exp       (void);
void sm_add       (void);
void sm_sub       (void);
void sm_mul       (void);
void sm_div       (void);
void sm_mod       (void);
/*--- EC-3c SM compare/stno templates (SM_templates/sm_compare.c) — EC-UNI-10(c): parameterless ---*/
void sm_stno      (void);
void sm_acomp     (void);
void sm_lcomp     (void);
/*--- EC-3d SM control-flow templates (SM_templates/sm_control.c) — EC-UNI-10(b): parameterless, read from g_emit ---*/
int  sm_jump      (void);
int  sm_jump_s    (void);
int  sm_jump_f    (void);
int  sm_halt      (void);
int  sm_return    (void);
int  sm_freturn   (void);
int  sm_nreturn   (void);
/*--- EC-3f SM_PAT_* templates (SM_templates/sm_pat.c) — EC-UNI-10(c): parameterless ---*/
void sm_pat_lit          (void);
void sm_pat_any_i        (void);
void sm_pat_notany       (void);
void sm_pat_span         (void);
void sm_pat_break        (void);
void sm_pat_len          (void);
void sm_pat_pos          (void);
void sm_pat_rpos         (void);
void sm_pat_tab          (void);
void sm_pat_rtab         (void);
void sm_pat_arb          (void);
void sm_pat_rem          (void);
void sm_pat_bal          (void);
void sm_pat_fence0       (void);
void sm_pat_abort        (void);
void sm_pat_fail         (void);
void sm_pat_succeed      (void);
void sm_pat_eps          (void);
void sm_pat_deref        (void);
void sm_pat_arbno        (void);
void sm_pat_fence1       (void);
void sm_pat_cat          (void);
void sm_pat_alt          (void);
void sm_pat_refname      (void);
void sm_pat_capture      (void);
void sm_pat_capture_fn   (void);
void sm_pat_capture_fn_args(void);
void sm_pat_usercall     (void);
void sm_pat_usercall_args(void);
void sm_exec_stmt        (void);
/*--- EC-4: unified prologue/epilogue (emit_core.c) — replaces per-silo statics ----------*/
int  emit_prologue(BB_graph_t * cfg, FILE * out);
int  emit_epilogue(BB_graph_t * cfg, FILE * out);
struct tree_t;
int  emit_program(const struct tree_t * ast_prog, FILE * out, bb_emit_mode_t mode);
/* EC-UNI-14(a): emit_sm_dispatch — shared opcode→template dispatcher used by WASM/JS/NET
 * silo walkers.  Reads g_emit.instr->op, calls the matching sm_<name>() template.  Returns
 * 1 if the template emitted its own PC transfer, 0 otherwise.  Backend-specific overrides
 * (NET's SM_LABEL/SM_EXEC_STMT, JS's SM_PUSH_EXPRESSION, WASM's SM_INCR/SM_DECR) must be
 * handled by the caller BEFORE invoking.
 *
 * Supersedes the EC-UNI-0 scaffold (the old (SM_sequence_t *, FILE *, mode) signature that
 * always returned -1).  The EC-UNI ladder (10..14) shifted to per-instruction g_emit context
 * rather than passing the program around; the new signature matches that direction.
 * EC-UNI-14 proper deletes the silo walkers entirely. */
int  emit_sm_dispatch(void);
/* EC-UNI-14(a): sm_op_is_dispatched — does emit_sm_dispatch handle this opcode?  Silo walkers
 * use this to annotate truly unhandled opcodes differently from dispatcher-handled no-ops
 * (e.g., SM_LABEL on WASM gets a clean fall-through; SM_PUSH_EXPRESSION on WASM gets an
 * `;; unhandled` comment because WASM has no template for it). */
int  sm_op_is_dispatched(SM_op_t op);
#endif
