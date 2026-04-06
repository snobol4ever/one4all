/*
 * sm_lower.h — IR → SM_Program compiler pass (M-SCRIP-U3)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06
 */

#ifndef SM_LOWER_H
#define SM_LOWER_H

#include "sm_prog.h"
#include "../../frontend/snobol4/scrip_cc.h"

/*
 * sm_lower — compile a SNOBOL4 IR Program into a flat SM_Program.
 *
 * The caller owns the returned SM_Program and must free it with sm_prog_free().
 * The input Program* and all EXPR_t trees must remain valid for the duration
 * of this call (sm_lower does not deep-copy strings; it borrows sval pointers
 * that live in the GC heap or are interned).
 *
 * Returns NULL on allocation failure.
 */
SM_Program *sm_lower(const Program *prog);

#endif /* SM_LOWER_H */
