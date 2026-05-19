#ifndef EMIT_BB_H
#define EMIT_BB_H
#include "emit_core.h"
#include "bb_pool.h"
#include "snobol4.h"
#include "bb_box.h"
#include "IR.h"
#include <stdio.h>
/*---- flat-glob builder API -----------------------------------------------*/
bb_box_fn bb_build_flat    (IR_t * nd);
bb_box_fn bb_build_brokered(IR_t * nd);
int  emit_flat_build        (IR_t * nd, FILE * out, const char * prefix);
void emit_flat_set_intern_str(const char * (*fn)(const char *));
void emit_flat_reset        (void);
int  emit_bb_macro_library_to_path(const char * path);
void emit_flat_set_cap_fixup(void (*cb)(void * cap_ptr, const char * child_alpha_label));
void emit_bb_register_child_label(IR_t * nd, const char * alpha_label);
void emit_flat_banner_rule  (char ch);
extern int g_flat_node_id;
/*---- section / data emitters ---------------------------------------------*/
void emit_flat_label        (const char * name);
void emit_flat_data_section (void);           void emit_flat_text_section (void);
void emit_flat_intel_syntax (void);
void emit_flat_data_string  (const char * s);
void emit_flat_data_quad    (const char * arg);
void emit_flat_data_long    (long long v);
/*---- box-call helpers ----------------------------------------------------*/
void emit_flat_entry_dispatch  (bb_label_t * lbl_alpha_body, bb_label_t * lbl_beta);
/*---- compat macros -------------------------------------------------------*/
#define bb_build_flat_text(p,out,pfx)    emit_flat_build(p,out,pfx)
#define bb_flat_set_intern_str(fn)        emit_flat_set_intern_str(fn)
#define bb_build_flat_text_reset()        emit_flat_reset()
#define bb_macros_write_to_path(path)     emit_bb_macro_library_to_path(path)
#define bb_flat_set_cap_fixup_cb(cb)      emit_flat_set_cap_fixup(cb)
#define flat3c_label(name)                emit_flat_label(name)
#define flat_data_section()               emit_flat_data_section()
#define flat_text_section()               emit_flat_text_section()
#define flat_intel_syntax()               emit_flat_intel_syntax()
#define flat_data_string(s)               emit_flat_data_string(s)
#define flat_data_quad(arg)               emit_flat_data_quad(arg)
#define flat_data_long(v)                 emit_flat_data_long(v)
#define flat_box_entry_dispatch(a,b)      emit_flat_entry_dispatch(a,b)
#endif
