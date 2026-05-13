#ifndef EMIT_BUF_H
#define EMIT_BUF_H

#include "emit_defs.h"
#include "bb_pool.h"
#include <stdint.h>
#include <stddef.h>

extern bb_emit_mode_t bb_emit_mode;

/* Raw buffer globals */
extern bb_buf_t  bb_emit_buf;
extern int       bb_emit_pos;
extern int       bb_emit_size;

/* Patch list */
extern bb_patch_t bb_patch_list[BB_PATCH_MAX];
extern int        bb_patch_count;

/* Buffer lifecycle */
void bb_emit_begin(bb_buf_t buf, int size);
int  bb_emit_end(void);

/* Jump-target patch helpers */
void bb_emit_patch_rel8 (bb_label_t *lbl);
void bb_emit_patch_rel32(bb_label_t *lbl);

/* Raw byte writers */
void bb_emit_byte(uint8_t b);
void bb_emit_u16 (uint16_t v);
void bb_emit_u32 (uint32_t v);
void bb_emit_u64 (uint64_t v);
void bb_emit_i8  (int8_t   v);
void bb_emit_i32 (int32_t  v);

#endif /* EMIT_BUF_H */
