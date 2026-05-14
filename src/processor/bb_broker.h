/*============================================================================================================================
 * bb_broker.h — Unified Byrd Box Broker (GOAL-UNIFIED-BROKER U-3)
 *============================================================================================================================*/

#ifndef BB_BROKER_H
#define BB_BROKER_H

#include "bb_box.h"
/* bb_convert.h removed EST-4 */
#include "snobol4.h"      /* DESCR_t, FAILDESCR, IS_FAIL_fn, α, β */

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_broker — drive a Byrd box in one of three modes.
 *
 * root     — the box to drive (fn + state ζ)
 * mode     — BB_SCAN / BB_PUMP / BB_ONCE
 * body_fn  — called once per γ result; may be NULL (tick counting only)
 * arg      — passed through to body_fn
 *
 * Returns tick count (number of γ results produced). 0 = total failure (ω on first call).
 *----------------------------------------------------------------------------------------------------------------------------*/
int bb_broker(bb_node_t root, BrokerMode mode,
              void (*body_fn)(DESCR_t val, void *arg), void *arg);

#endif /* BB_BROKER_H */
