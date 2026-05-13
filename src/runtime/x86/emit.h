/* emit.h — umbrella include for the emitter subsystem (RW-CONSOLIDATE).
 *
 * Three consolidated units:
 *   emit_core.h  — types, raw buffer, mode, labels, insn leaves, text, seq
 *   emit_bb.h    — BB box templates + flat-glob builder
 *   emit_sm.h    — SM opcode templates + text codegen walker
 *   emit_sm_binary.h — frozen mode-3 binary codegen (never touched)
 *
 * Old shim headers (emit_bb_gen.h, emit_buf.h, emit_form.h, emit_label.h,
 * emit_label_new.h, emit_text3c.h, emit_insn.h, insn.h, emit_text.h,
 * emit_mode.h, emit_seq.h, emit_defs.h, emit_flat.h, emit_walk.h,
 * emit_sm_shape.h, emit_bb_flat.h, emit_sm_text.h) are all deleted.
 * Their declarations live in emit_core.h / emit_bb.h / emit_sm.h.
 */

#ifndef EMIT_H
#define EMIT_H

#include "emit_core.h"
#include "emit_bb.h"
#include "emit_sm.h"
#include "emit_sm_binary.h"
#include "bb_pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* Legacy shim: emit_form.h compat (still used by emit_core.c internals) */
/* The symbols below were in emit_form.h and are still called by external  */
/* code compiled against old headers. They are implemented in emit_core.c. */
void emitter_init_binary(bb_buf_t buf, int size);
void emitter_init_text  (FILE *out, int mode);
int  emitter_end        (void);
void emitter_init_macro_def(FILE *out);
FILE *emitter_text_out(void);
int   emitter_pos(void);

extern int g_is_text;
extern int g_emit_text_mode;

#define TEXT_MODE_INVOCATION  0
#define TEXT_MODE_DEFINITION  1

#define TEXT_MODE_INVOCATION  0
#define TEXT_MODE_DEFINITION  1

/* emit_form.h data-output functions now in emit_core.c */
void emit_section       (const char *name);
void emit_directive     (const char *line);
void emit_global_sym    (const char *name);
void emit_banner        (const char *text);
void emit_minor_break   (const char *text);
void emit_blank_line    (void);
void emit_fprintf_raw   (const char *fmt, ...);
void emit_data_quad     (uint64_t val);
void emit_data_quad_sym (const char *sym);
void emit_data_string   (const char *bytes, size_t len);
void emit_data_long     (int32_t val);
void emit_macro_param_ref(const char *name);

/* emit_form.h BB-wiring functions now in emit_core.c */
void emit_bb_zeta_rdi        (uint64_t ptr, const char *sym);
void emit_bb_dispatch_jne_jmp(bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* emit_form.h label helpers now in emit_core.c */
void emit_label_name    (const char *name);
void emit_pc_label      (int pc);
void emit_jmp_label     (bb_label_t *target, jmp_kind_t kind);

#endif /* EMIT_H */
