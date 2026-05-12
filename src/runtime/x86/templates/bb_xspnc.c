#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_bb_charset(emitter_t *e,
                     bb_box_fn c_fn,
                     const char *c_fn_name,
                     const char *kind_name,
                     const char *chars,
                     bb_label_t *lbl_succ,
                     bb_label_t *lbl_fail,
                     bb_label_t *lbl_β,
                     bb_charset_text_fn text_body_fn,
                     void *text_body_arg)
{
    (void)kind_name;
    if (!e) return;

    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }

    typedef struct { const char *chars; int delta; } cs_t;
    cs_t *z = calloc(1, sizeof(cs_t));
    z->chars = chars;

    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 0);
    emit_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);

    EMIT_LABEL(e, lbl_β);
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 1);
    emit_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
