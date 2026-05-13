
#ifndef EMIT_BB_H
#define EMIT_BB_H

#include "emit_core.h"
#include "bb_pool.h"
#include "snobol4.h"
#include "bb_box.h"
#include <stdio.h>

bb_box_fn bb_build_flat      (PATND_t *p);
bb_box_fn bb_build_brokered  (PATND_t *p);
int  emit_flat_build         (PATND_t *p, FILE *out, const char *prefix);
void emit_flat_set_intern_str(const char *(*fn)(const char *));
void emit_flat_reset         (void);
int  emit_flat_macros_to_path(const char *path);
void emit_flat_set_cap_fixup (void (*cb)(void *cap_ptr, const char *child_alpha_label));
void emit_flat_banner_rule   (char ch);
void emit_flat_box_banner    (const char *kind, const char *args, const char *label_prefix);
extern int g_flat_node_id;

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
void emit_flat_box_call      (const char *rdi_load, const char *fn, int mode);
void emit_flat_box_call_slot (const char *slot_lbl, const char *fn, int mode);
void emit_flat_dispatch_jne_jmp(bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_flat_entry_dispatch(bb_label_t *lbl_alpha_body, bb_label_t *lbl_beta);

typedef void (*bb_charset_text_fn)(bb_label_t *, bb_label_t *, bb_label_t *, void *);
typedef void (*bb_intcur_text_fn) (bb_label_t *, bb_label_t *, bb_label_t *, void *);
typedef void (*bb_brkx_text_fn)   (bb_label_t *, bb_label_t *, bb_label_t *, void *);
typedef void (*bb_pos_text_fn)    (int, bb_label_t *, bb_label_t *, bb_label_t *, void *);
typedef void (*bb_nullary_text_fn)(bb_label_t *, bb_label_t *, bb_label_t *, void *);

void emit_flat_box_call_fn(bb_box_fn fn, const char *fn_name, void *z,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_beta);

#define bb_build_flat_text(p,out,pfx)   emit_flat_build(p,out,pfx)
#define bb_flat_set_intern_str(fn)       emit_flat_set_intern_str(fn)
#define bb_build_flat_text_reset()       emit_flat_reset()
#define bb_macros_write_to_path(path)    emit_flat_macros_to_path(path)
#define bb_flat_set_cap_fixup_cb(cb)     emit_flat_set_cap_fixup(cb)
#define flat3c_label(name)               emit_flat_label(name)
#define flat_data_section()              emit_flat_data_section()
#define flat_text_section()              emit_flat_text_section()
#define flat_intel_syntax()              emit_flat_intel_syntax()
#define flat_data_string(s)              emit_flat_data_string(s)
#define flat_data_quad(arg)              emit_flat_data_quad(arg)
#define flat_data_quad_int(v)            emit_flat_data_quad_i(v)
#define flat_data_long(v)                emit_flat_data_long(v)
#define flat_data_zero(n)                emit_flat_data_zero(n)
#define flat_globl(name)                 emit_flat_globl(name)
#define flat_box_call(rdi,fn,mode)       emit_flat_box_call(rdi,fn,mode)
#define flat_box_call_slot(slot,fn,mode) emit_flat_box_call_slot(slot,fn,mode)
#define flat_box_dispatch_jne_jmp(s,f)   emit_flat_dispatch_jne_jmp(s,f)
#define flat_box_entry_dispatch(a,b)     emit_flat_entry_dispatch(a,b)

#define bb_build_flat_text_reset()       emit_flat_reset()

#endif 
