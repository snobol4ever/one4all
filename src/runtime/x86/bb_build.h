/*
 * bb_build.h — Binary x86-64 Byrd Box Emitter (M-DYN-B1 / M-DYN-B2)
 */
#ifndef BB_BUILD_BIN_H
#define BB_BUILD_BIN_H

#include "bb_box.h"
#include "snobol4_patnd.h"

/*
 * M-BB-LIVE-WIRE: Byrd Box pattern mode.
 * BB_MODE_DRIVER (default): pattern matching via driver/broker (C box calls).
 * BB_MODE_LIVE:             live-wired — route phase 3 through bb_build_binary().
 */
typedef enum { BB_MODE_DRIVER = 0, BB_MODE_LIVE = 1 } bb_mode_t;
extern bb_mode_t g_bb_mode;

/*
 * M-DYN-B1: Emit the LIT box as executable x86-64 bytes into bb_pool.
 * lit and len are baked into the emitted code — no zeta needed at runtime.
 * Returns a callable bb_box_fn, or NULL on allocation failure.
 */
bb_box_fn bb_lit_emit_binary(const char *lit, int len);

/*
 * M-DYN-B2: Emit the EPS box as executable x86-64 bytes.
 * Stateless (no zeta needed): α always succeeds with δ=0, β always fails.
 * Returns a callable bb_box_fn, or NULL on allocation failure.
 */
bb_box_fn bb_eps_emit_binary(void);

/*
 * M-DYN-B2: Walk a PATND_t tree and emit binary boxes for all nodes.
 * Returns NULL if any node cannot yet be emitted as binary (caller
 * should fall back to C bb_build() for the whole tree in that case).
 */
bb_box_fn bb_build_binary(PATND_t *p);

#endif /* BB_BUILD_BIN_H */
