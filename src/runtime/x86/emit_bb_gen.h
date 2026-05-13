/* emit_bb_gen.h — umbrella shim: includes all emitter subsystem headers.
 *
 * Callers that previously included emit_bb_gen.h get all L0–L3 emitter
 * symbols without any source change.  New code should include the specific
 * level header it needs.
 */

#ifndef EMITTER_BB_GEN_H
#define EMITTER_BB_GEN_H

#include "emit_defs.h"       /* L2: shared types */
#include "emit_buf.h"        /* L0: raw buffer */
#include "emit_form.h"       /* L1: x86 encoding forms */
#include "emit_label.h"      /* L2: label lifecycle (old names) */
#include "emit_label_new.h"  /* L2: label lifecycle (new names) */
#include "emit_text3c.h"     /* L2: 3-col formatter */
#include "emit_insn.h"       /* L2: single-instruction emitters (old names) */
#include "insn.h"            /* L1: single-instruction emitters (new names) */
#include "emit_text.h"       /* L2: TEXT-only helpers (new names) */
#include "emit_mode.h"       /* L2: mode globals + macro begin/end */
#include "emit_seq.h"        /* L3: compound BB helpers (new names) */
#include "bb_pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#endif /* EMITTER_BB_GEN_H */
