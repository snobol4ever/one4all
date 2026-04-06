/*
 * bb_build_bin.h — Binary x86-64 Byrd Box Emitter (M-DYN-B1)
 */
#ifndef BB_BUILD_BIN_H
#define BB_BUILD_BIN_H

#include "../boxes/shared/bb_box.h"

/*
 * Emit the LIT box as executable x86-64 bytes into bb_pool.
 * lit and len are baked into the emitted code — no zeta needed at runtime.
 * Returns a callable bb_box_fn, or NULL on allocation failure.
 * The returned function pointer is valid until bb_free() is called.
 */
bb_box_fn bb_lit_emit_binary(const char *lit, int len);

#endif /* BB_BUILD_BIN_H */
