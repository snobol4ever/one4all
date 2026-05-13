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

void    emitter_init_text       (FILE *out, int mode);
void    emitter_init_binary     (bb_buf_t buf, int size);
void    emitter_init_macro_def  (FILE *out);
FILE *  emitter_text_out        (void);
int     emitter_pos             (void);
int     emitter_end             (void);

extern int g_is_text;
extern int g_emit_text_mode;

#define TEXT_MODE_INVOCATION  0
#define TEXT_MODE_DEFINITION  1

void emit_banner                (const char *text);
void emit_bb_dispatch_jne_jmp   (bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_bb_zeta_rdi           (uint64_t ptr, const char *sym);
void emit_blank_line            (void);
void emit_data_long             (int32_t val);
void emit_data_quad             (uint64_t val);
void emit_data_quad_sym         (const char *sym);
void emit_data_string           (const char *bytes, size_t len);
void emit_directive             (const char *line);
void emit_fprintf_raw           (const char *fmt, ...);
void emit_global_sym            (const char *name);
void emit_jmp_label             (bb_label_t *target, jmp_kind_t kind);
void emit_label_name            (const char *name);
void emit_macro_param_ref       (const char *name);
void emit_minor_break           (const char *text);
void emit_pc_label              (int pc);
void emit_section               (const char *name);

#endif /* EMIT_H */
