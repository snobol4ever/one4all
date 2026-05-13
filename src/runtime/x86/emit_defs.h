#ifndef EMIT_DEFS_H
#define EMIT_DEFS_H

#include <stdint.h>

/* bb_emit_mode_t — controls what every emit helper produces */
typedef enum {
    EMIT_TEXT             = 0,
    EMIT_BINARY_WIRED     = 1,
    EMIT_BINARY_BROKERED  = 2,
    EMIT_MACRO_DEF        = 3,
    EMIT_TEXT_INLINE      = 4
} bb_emit_mode_t;

/* bb_label_t — symbolic label used in both binary and text emission */
#define BB_LABEL_NAME_MAX  80
#define BB_LABEL_UNRESOLVED (-1)

typedef struct {
    char name[BB_LABEL_NAME_MAX];
    int  offset;
} bb_label_t;

#define bb_label_defined(lbl)  ((lbl)->offset != BB_LABEL_UNRESOLVED)

/* jmp_kind_t — jump condition for emit_jmp */
typedef enum {
    JMP_JMP = 0,
    JMP_JE,
    JMP_JNE,
    JMP_JL,
    JMP_JGE,
    JMP_JG,
} jmp_kind_t;

/* bb_patch_t — deferred jump target patch record */
#define BB_PATCH_MAX  512

typedef enum {
    PATCH_REL8,
    PATCH_REL32
} bb_patch_kind_t;

typedef struct {
    int              site;
    bb_label_t      *label;
    bb_patch_kind_t  kind;
} bb_patch_t;

#endif /* EMIT_DEFS_H */
