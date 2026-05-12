#ifndef TEMPLATES_SM_HELPERS_H
#define TEMPLATES_SM_HELPERS_H
#include "../emitter.h"
#include "../bb_emit.h"
#include <stdint.h>

void emit_sm_rtcall(emitter_t *e, const char *comment_str,
                         const char *macro_name, const char *rt_sym);
void emit_sm_pat_rtcall(emitter_t *e, const char *comment_str,
                              const char *macro_name, const char *rt_sym);
void emit_sm_lbl_rt(emitter_t *e, const char *comment_str,
                     const char *macro_name, const char *rt_sym,
                     const char *name_lbl, uint64_t name_ptr);
void emit_sm_arith_op(emitter_t *e, int op_enum, const char *macro_name);
#endif
