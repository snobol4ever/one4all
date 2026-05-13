/* emit.h — RW-6: umbrella include for the rewritten emitter subsystem.
 *
 * Replaces emit_bb_gen.h. Include this in new code.
 * emit_bb_gen.h is kept as a shim that includes this file.
 */

#ifndef EMIT_H
#define EMIT_H

#include "emit_defs.h"       /* L2: shared types */
#include "emit_buf.h"        /* L0: raw buffer (bb_emit_byte, bb_emit_u32...) */
#include "emit_form.h"       /* L1: x86 encoding forms (old names, still needed) */
#include "emit_label.h"      /* L2: label lifecycle (old names) */
#include "emit_label_new.h"  /* L2: label lifecycle (new names) */
#include "emit_text3c.h"     /* L2: 3-col formatter */
#include "insn.h"            /* L1: single-instruction emitters (new names) */
#include "emit_insn.h"       /* L1: single-instruction emitters (old names) */
#include "emit_text.h"       /* L2: TEXT-only helpers */
#include "emit_mode.h"       /* L2: mode globals + macro begin/end */
#include "emit_seq.h"        /* L3: compound BB helpers */
#include "bb_pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#endif /* EMIT_H */
