/*
 * t2_reloc.c — Technique 2 relocation patcher
 *
 * After memcpy'ing a TEXT block to a new address, every internal
 * PC-relative reference (jmp/call near) and every absolute DATA pointer
 * embedded in the code must be adjusted by `delta = new_addr - old_addr`.
 *
 * The emitter (emit_byrd_asm.c) will emit a per-box relocation table as
 * NASM data (box_N_reloc_table / box_N_reloc_count).  At runtime we pass
 * that table here before marking the block executable.
 *
 * Correct call sequence for T2 invocation:
 *
 *   uint8_t *text = t2_alloc(text_sz);       // RW
 *   memcpy(text, original_text, text_sz);
 *   ptrdiff_t delta = text - original_text;
 *   t2_relocate(text, text_sz, delta, reloc_table, reloc_count);
 *   t2_flush_icache(text, text_sz);           // portable i$/d$ coherence
 *   t2_mprotect_rx(text, text_sz);            // RW -> RX
 *   // jump to text + entry_offset
 */

#include "t2_reloc.h"
#include <string.h>
#include <errno.h>

/*
 * t2_relocate — apply all relocation entries to a copied TEXT block.
 *
 * For T2_RELOC_REL32:
 *   The displaced value stored at text[offset..offset+3] is a signed 32-bit
 *   offset relative to the END of that field (i.e. the standard x86 encoding:
 *   target = PC_after_insn + rel32, where PC_after_insn = old_text + offset + 4).
 *
 *   After copying to new_text:
 *     new_rel32 = old_rel32 - delta
 *   because the field now sits `delta` bytes further from its old target.
 *
 *   If old_rel32 - delta overflows int32_t we return -1 (target out of range
 *   for a 32-bit displacement — caller must use an indirect trampoline).
 *
 * For T2_RELOC_ABS64:
 *   A 64-bit pointer embedded in the code (e.g. lea rax,[rip+0] + fixup, or
 *   mov rax, imm64).  Simply add delta:
 *     new_abs64 = old_abs64 + delta
 *
 * Returns 0 on success, -1 on any out-of-range condition.
 */
int t2_relocate(uint8_t *text, size_t len,
                ptrdiff_t delta,
                const t2_reloc_entry *table, size_t n)
{
    if (!text || !table || n == 0) return 0;

    for (size_t i = 0; i < n; i++) {
        size_t off  = table[i].offset;

        switch (table[i].kind) {

        case T2_RELOC_REL32: {
            /* bounds check: need 4 bytes */
            if (off + 4 > len) return -1;

            int32_t rel;
            memcpy(&rel, text + off, sizeof rel);

            /* Adjust: the field moved by delta, so subtract delta from the
             * stored displacement to keep it pointing at the same target. */
            int64_t new_rel = (int64_t)rel - (int64_t)delta;
            if (new_rel < INT32_MIN || new_rel > INT32_MAX) return -1;

            int32_t patched = (int32_t)new_rel;
            memcpy(text + off, &patched, sizeof patched);
            break;
        }

        case T2_RELOC_ABS64: {
            /* bounds check: need 8 bytes */
            if (off + 8 > len) return -1;

            uint64_t abs;
            memcpy(&abs, text + off, sizeof abs);
            abs = (uint64_t)((int64_t)abs + (int64_t)delta);
            memcpy(text + off, &abs, sizeof abs);
            break;
        }

        default:
            return -1;   /* unknown relocation kind */
        }
    }
    return 0;
}
