#pragma once
/* coerce.h — shared coercion and arithmetic helpers (RS-6, RS-7)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (RS-6/RS-7, 2026-05-02) */

#include "snobol4.h"     /* DESCR_t, DT_*, IS_INT_fn, IS_REAL_fn, IS_STR_fn, STRVAL, FAILDESCR — via -I$(RT)/x86 */
#include "sm_prog.h"     /* sm_opcode_t, SM_ADD .. SM_EXP — via -I$(RT)/x86 */

/* descr_to_str_icn — coerce DT_I or DT_R to DT_S (Icon semantics).
 * DT_S / DT_SNUL returned as-is.  All others return FAILDESCR. */
DESCR_t descr_to_str_icn(DESCR_t d);

/* shared_arith — unified binary arithmetic (F-1, RS-7).
 * Replaces sm_arith() and jit_arith(). Caller must pre-coerce strings. */
DESCR_t shared_arith(DESCR_t l, DESCR_t r, sm_opcode_t op);
