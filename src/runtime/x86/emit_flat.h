
#ifndef EMIT_FLAT_H
#define EMIT_FLAT_H

#include <stdio.h>
#include "bb_pool.h"
#include "snobol4.h"
#include "bb_box.h"
#include "emit.h"

/* Build a flat-globbed blob for an entire invariant PATND_t tree. */
bb_box_fn bb_build_flat(PATND_t *p);

/* EM-BB-PURGE-1 / EDP-7: Emit pattern tree as one brokered blob with C-ABI */
bb_box_fn bb_build_brokered(PATND_t *p);

/* RW-5: TEXT-mode flat-BB builder (renamed from bb_build_flat_text). */
int emit_flat_build(PATND_t *p, FILE *out, const char *prefix);

/* RW-5: Set the intern_str callback (renamed from bb_flat_set_intern_str). */
void emit_flat_set_intern_str(const char *(*fn)(const char *));

/* RW-5: Reset internal label/slot counters (renamed from bb_build_flat_text_reset). */
void emit_flat_reset(void);

/* RW-5: Write the BB macro library to path (renamed from bb_macros_write_to_path). */
int emit_flat_macros_to_path(const char *path);

/* RW-5: Install capture-fixup callback (renamed from bb_flat_set_cap_fixup_cb). */
void emit_flat_set_cap_fixup(void (*cb)(void *cap_ptr, const char *child_alpha_label));

/* EM-MODE4-IS-MODE3-DUMP-d: shared formatter helpers */
void emit_flat_banner_rule(char ch);
void emit_flat_box_banner(const char *kind,
                          const char *args, const char *label_prefix);

extern int g_flat_node_id;

/* EDP-5: flat data/text section helpers (renamed from flat_*) */
void emit_flat_label         (const char *name);
void emit_flat_data_section  (void);
void emit_flat_text_section  (void);
void emit_flat_intel_syntax  (void);
void emit_flat_data_string   (const char *s);
void emit_flat_data_quad     (const char *arg);
void emit_flat_data_quad_i   (long long v);
void emit_flat_data_long     (long long v);
void emit_flat_data_zero     (int n);
void emit_flat_globl         (const char *name);

/* RW-5: port-call emitter (renamed from flat_box_call) */
void emit_flat_box_call      (const char *rdi_load,
                              const char *fn, int mode);

/* RW-5: slot-pointer variant (renamed from flat_box_call_slot) */
void emit_flat_box_call_slot (const char *slot_lbl,
                              const char *fn, int mode);

/* RW-5: test+jne+jmp dispatch (renamed from flat_box_dispatch_jne_jmp) */
void emit_flat_dispatch_jne_jmp(bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* RW-5: cmp esi,0; je alpha; jmp beta (renamed from flat_box_entry_dispatch) */
void emit_flat_entry_dispatch(bb_label_t *lbl_alpha_body, bb_label_t *lbl_beta);

/* Callback types for box templates */
typedef void (*bb_charset_text_fn)(bb_label_t *lbl_succ,
                                   bb_label_t *lbl_fail,
                                   bb_label_t *lbl_beta,
                                   void *arg);
typedef void (*bb_intcur_text_fn)(bb_label_t *lbl_succ,
                                  bb_label_t *lbl_fail,
                                  bb_label_t *lbl_beta,
                                  void *arg);
typedef void (*bb_brkx_text_fn)(bb_label_t *lbl_succ,
                                bb_label_t *lbl_fail,
                                bb_label_t *lbl_beta,
                                void *arg);
typedef void (*bb_pos_text_fn)(int n,
                               bb_label_t *lbl_succ,
                               bb_label_t *lbl_fail,
                               bb_label_t *lbl_beta,
                               void *arg);
typedef void (*bb_nullary_text_fn)(bb_label_t *lbl_succ,
                                   bb_label_t *lbl_fail,
                                   bb_label_t *lbl_beta,
                                   void *arg);

/* RW-5: alpha/beta dispatch for pre-allocated zeta struct (renamed from emit_flat_box_call in old header) */
void emit_flat_box_call_fn(bb_box_fn fn, const char *fn_name,
                           void *z,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                           bb_label_t *lbl_beta);

/* ── Backward-compat aliases so existing callers compile without change ── */
#define bb_build_flat_text(p,out,pfx)       emit_flat_build(p,out,pfx)
#define bb_flat_set_intern_str(fn)           emit_flat_set_intern_str(fn)
#define bb_build_flat_text_reset()           emit_flat_reset()
#define bb_macros_write_to_path(path)        emit_flat_macros_to_path(path)
#define bb_flat_set_cap_fixup_cb(cb)         emit_flat_set_cap_fixup(cb)
#define flat3c_label(name)                   emit_flat_label(name)
#define flat_data_section()                  emit_flat_data_section()
#define flat_text_section()                  emit_flat_text_section()
#define flat_intel_syntax()                  emit_flat_intel_syntax()
#define flat_data_string(s)                  emit_flat_data_string(s)
#define flat_data_quad(arg)                  emit_flat_data_quad(arg)
#define flat_data_quad_int(v)                emit_flat_data_quad_i(v)
#define flat_data_long(v)                    emit_flat_data_long(v)
#define flat_data_zero(n)                    emit_flat_data_zero(n)
#define flat_globl(name)                     emit_flat_globl(name)
#define flat_box_call(rdi,fn,mode)           emit_flat_box_call(rdi,fn,mode)
#define flat_box_call_slot(slot,fn,mode)     emit_flat_box_call_slot(slot,fn,mode)
#define flat_box_dispatch_jne_jmp(s,f)       emit_flat_dispatch_jne_jmp(s,f)
#define flat_box_entry_dispatch(a,b)         emit_flat_entry_dispatch(a,b)

#endif
