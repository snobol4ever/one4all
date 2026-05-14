/* sm_codegen_x64_emit.h — compatibility shim for EM-2 gate harness.
 * sm_codegen_x64_emit() is an alias for sm_codegen_text() (emit_sm.h).
 * All implementation lives in emit_sm.c / libscrip_rt.so. */
#pragma once
#include "emit_sm.h"
#define sm_codegen_x64_emit(prog, out, src) sm_codegen_text(prog, out, src)
