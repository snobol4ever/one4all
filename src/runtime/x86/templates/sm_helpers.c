/* sm_helpers.c — shared helper functions called by SM template files.
 * After TC-UNSPLIT-1..11 (sess 2026-05-12g), helpers folded back into
 * their bundles as static fns where possible.  Retained here: helpers
 * still called by single-opcode template files outside the unsplit
 * scope. */
#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_rtcall(emitter_t *e,
                         const char *comment_str,
                         const char *macro_name,
                         const char *rt_sym)
{
    (void)e;
    t_comment(comment_str);
    t_macro_begin(macro_name, NULL, 0);
    t_call_sym_plt(rt_sym, 0);
    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_pat_rtcall(emitter_t *e,
                              const char *comment_str,
                              const char *macro_name,
                              const char *rt_sym)
{
    (void)e;
    t_comment(comment_str);
    t_macro_begin(macro_name, NULL, 0);
    t_call_sym_plt(rt_sym, 0);
    t_macro_end();
    t_pad_to_blob_size();
}
