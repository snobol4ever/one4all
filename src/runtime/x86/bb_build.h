/*
 * bb_build.h — Binary x86-64 Byrd Box Emitter
 */
#ifndef BB_BUILD_BIN_H
#define BB_BUILD_BIN_H

#include "bb_box.h"
#include "snobol4_patnd.h"

/*
 * M-BB-LIVE-WIRE: Byrd Box pattern mode.
 * BB_MODE_DRIVER (default): pattern matching via driver/broker (C box calls).
 * BB_MODE_LIVE:             live-wired — route phase 3 through bb_build_flat().
 */
typedef enum { BB_MODE_DRIVER = 0, BB_MODE_LIVE = 1, BB_MODE_BROKERED = 2 } bb_mode_t;
extern bb_mode_t g_bb_mode;

#endif /* BB_BUILD_BIN_H */
