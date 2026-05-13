

#ifndef BB_FLAT_H
#define BB_FLAT_H

#include <stdio.h>
#include "bb_pool.h"
#include "snobol4.h"
#include "bb_box.h"
#include "emit.h"

/* Build a flat-globbed blob for an entire invariant PATND_t tree. */
bb_box_fn bb_build_flat(PATND_t *p);

/* EM-BB-PURGE-1 / EDP-7: Emit pattern tree as one brokered blob with C-ABI */
bb_box_fn bb_build_brokered(PATND_t *p);

/* EM-7b: TEXT-mode counterpart to bb_build_flat. */
int bb_build_flat_text(PATND_t *p, FILE *out, const char *prefix);

/* EM-7c-symbolic: set the intern_str callback used by bb_build_flat_text. */
void bb_flat_set_intern_str(const char *(*fn)(const char *));

/* EM-7c: reset internal label/slot counters between unrelated emit runs. */
void bb_build_flat_text_reset(void);

/* EM-7c-bb-macros: write the BB macro library to the given path. */
int bb_macros_write_to_path(const char *path);

/* EM-7c-capture: install a callback that is invoked each time emit_flat_node */
void bb_flat_set_cap_fixup_cb(void (*cb)(void *cap_ptr, const char *child_α_label));

/* EM-MODE4-IS-MODE3-DUMP-d: shared formatter helpers exposed for use */
void emit_flat_banner_rule(char ch);
void emit_flat_box_banner (const char *kind,
                           const char *args, const char *label_prefix);


extern int g_flat_node_id;

/* EDP-5: flat data/text section helpers promoted from static so BB */
void flat3c_label          (const char *name);
void flat_data_section     (void);
void flat_text_section     (void);
void flat_intel_syntax     (void);
void flat_data_string      (const char *s);
void flat_data_quad        (const char *arg);
void flat_data_quad_int    (long long v);
void flat_data_long        (long long v);
void flat_data_zero        (int n);
void flat_globl            (const char *name);
/* TEXT-mode port-call emitter: push r10, lea rdi,[rip+rdi_load], mov esi,mode, */
void flat_box_call         (const char *rdi_load,
                            const char *fn, int mode);
/* Slot-pointer variant: mov rdi,[rip+slot_lbl]; mov esi,mode; call fn@PLT. */
void flat_box_call_slot    (const char *slot_lbl,
                            const char *fn, int mode);
/* test rax,rax; jne succ; jmp fail — fused on one line. TEXT-mode only. */
void flat_box_dispatch_jne_jmp(
                               bb_label_t *lbl_succ, bb_label_t *lbl_fail);
/* cmp esi,0; je alpha_body; jmp beta — fused entry dispatch. TEXT-mode only. */
void flat_box_entry_dispatch(
                             bb_label_t *lbl_alpha_body, bb_label_t *lbl_beta);

/* Callback type for the charset-family template (bb_xspnc.c). */
typedef void (*bb_charset_text_fn)(
                                   bb_label_t *lbl_succ,
                                   bb_label_t *lbl_fail,
                                   bb_label_t *lbl_β,
                                   void *arg);

/* Callback type for the integer-cursor family template (bb_xlnth.c). */
typedef void (*bb_intcur_text_fn)(
                                  bb_label_t *lbl_succ,
                                  bb_label_t *lbl_fail,
                                  bb_label_t *lbl_β,
                                  void *arg);

/* Callback type for the XBRKX template (bb_xbrkx.c). */
typedef void (*bb_brkx_text_fn)(
                                bb_label_t *lbl_succ,
                                bb_label_t *lbl_fail,
                                bb_label_t *lbl_β,
                                void *arg);

/* Callback type for the XPOSI/XRPSI templates (bb_xposi.c). */
typedef void (*bb_pos_text_fn)(int n,
                               bb_label_t *lbl_succ,
                               bb_label_t *lbl_fail,
                               bb_label_t *lbl_β,
                               void *arg);

/* Callback type for no-operand boxes (XEPS/XFAIL/XFARB) (bb_xfarb.c). */
typedef void (*bb_nullary_text_fn)(
                                   bb_label_t *lbl_succ,
                                   bb_label_t *lbl_fail,
                                   bb_label_t *lbl_β,
                                   void *arg);

/* emit_flat_box_call — alpha/beta dispatch for a pre-allocated zeta struct. */
void emit_flat_box_call(bb_box_fn fn, const char *fn_name,
                        void *z,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                        bb_label_t *lbl_β);

#endif
