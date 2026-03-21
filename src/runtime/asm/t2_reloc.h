/*
 * t2_reloc.h — Technique 2 relocation table types and API
 *
 * A relocation entry describes one reference inside a copied TEXT block
 * that must be patched after the block is placed at a new address.
 *
 * Two kinds:
 *   T2_RELOC_REL32  — 32-bit PC-relative offset (e.g. jmp/call near)
 *                     Patch: *(int32_t*)(text+offset) += delta
 *   T2_RELOC_ABS64  — 64-bit absolute pointer (e.g. lea rax,[DATA+off])
 *                     Patch: *(uint64_t*)(text+offset) += delta
 */
#ifndef T2_RELOC_H
#define T2_RELOC_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    T2_RELOC_REL32 = 1,   /* 32-bit signed PC-relative displacement */
    T2_RELOC_ABS64 = 2    /* 64-bit absolute address */
} t2_reloc_kind;

typedef struct {
    size_t          offset;  /* byte offset inside the TEXT block */
    t2_reloc_kind   kind;
} t2_reloc_entry;

/*
 * t2_relocate — patch all relocations in a copied TEXT block.
 *
 *   text   — pointer to the NEW (destination) TEXT block (RW)
 *   len    — byte length of the block
 *   delta  — new_address - old_address  (signed displacement)
 *   table  — array of relocation entries
 *   n      — number of entries
 *
 * Returns 0 on success, -1 if any entry is out of range.
 * Call BEFORE t2_flush_icache + t2_mprotect_rx.
 */
int t2_relocate(uint8_t *text, size_t len,
                ptrdiff_t delta,
                const t2_reloc_entry *table, size_t n);

#endif /* T2_RELOC_H */
