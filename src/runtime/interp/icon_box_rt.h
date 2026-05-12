#ifndef ICON_BOX_RT_H
#define ICON_BOX_RT_H

/*============================================================================================================================
 * icon_box_rt.h — Icon generator runtime helpers for flat BB templates (GOAL-ICON-BB-NATIVE)
 *
 * State allocators and tick functions called by emit_bb_icon_* template blobs at runtime.
 * Zeta is allocated at runtime (not emit time) because Icon generator args are runtime values.
 *
 * Naming: icon_* (not icn_*) per session convention.
 *============================================================================================================================*/

#include <stdint.h>
#include "../../frontend/icon/icon_gen.h"   /* icn_to_state_t, icn_to_by_state_t, DESCR_t, FAILDESCR, α/β */

/*----------------------------------------------------------------------------------------------------------------------------
 * IB-1: integer range  (lo to hi)  and  (lo to hi by step)
 *--------------------------------------------------------------------------------------------------------------------------*/
icn_to_state_t    *icon_to_make(long lo, long hi);
icn_to_by_state_t *icon_to_by_make(long lo, long hi, long step);
/* Tick functions: same signature as bb_box_fn — called by BROKERED blob with rdi=zeta, esi=port */
DESCR_t icon_to_tick   (void *zeta, int port);
DESCR_t icon_to_by_tick(void *zeta, int port);

#endif /* ICON_BOX_RT_H */
