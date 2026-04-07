/*
 * bb_poc.c — Dynamic Byrd Box Proof of Concept
 *
 * Proves the entire dynamic execution stack:
 *   mmap → write x86-64 bytes by hand → mprotect RW→RX (I-cache fence)
 *   → call via function pointer → γ/ω return
 *
 * No scrip-cc. No frontend. No runtime. Pure proof.
 *
 * The Byrd box model:
 *   α — entry port (attempt match)
 *   γ — success port (match succeeded, cursor advanced)
 *   ω — failure port (match failed)
 *   β — retry port (not used in a literal box — literals don't backtrack)
 *
 * This box matches the literal string "hello" against a subject.
 * Subject is passed as (const char *subject, int len).
 * Returns: 1 if γ fired (match), 0 if ω fired (no match).
 *
 * Calling convention: System V AMD64
 *   rdi = subject pointer
 *   rsi = subject length
 *   returns rax = 1 (γ) or 0 (ω)
 *
 * Build: gcc -o bb_poc src/runtime/asm/bb_poc.c
 * Gate:  subject="hello world" → PASS
 *        subject="goodbye"     → FAIL
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

/* The box function signature: (subject, len) -> 1=match, 0=no match */
typedef int (*box_fn)(const char *subject, int len);

/*
 * emit_byte / emit helpers — write into a buffer at offset *pos
 */
static uint8_t *buf;
static int pos;

static void eb(uint8_t b)               { buf[pos++] = b; }
static void e32(uint32_t v) {
    buf[pos++] = v & 0xff;
    buf[pos++] = (v >> 8) & 0xff;
    buf[pos++] = (v >> 16) & 0xff;
    buf[pos++] = (v >> 24) & 0xff;
}

/*
 * build_lit_box(lit, litlen)
 *
 * Emits x86-64 machine code for a Byrd box that matches a literal string.
 *
 * Strategy:
 *   - Compare subject length against literal length → ω if subject too short
 *   - Compare subject bytes against literal bytes one at a time → ω on mismatch
 *   - γ: mov rax,1 / ret
 *   - ω: mov rax,0 / ret
 *
 * Register use (callee does not save — this is a leaf):
 *   rdi = subject pointer  (arg 0)
 *   rsi = subject length   (arg 1)
 *   rcx = loop counter / byte index
 *   al  = scratch byte comparison
 *
 * Byte-by-byte approach: dead simple, correct, easy to verify.
 *
 * Layout:
 *   [α entry]
 *     cmp esi, litlen       ; subject long enough?
 *     jl  ω
 *     mov rcx, 0            ; index = 0
 *   [loop top]              ; for each byte in literal:
 *     movzx eax, byte [rdi+rcx]
 *     cmp al, lit[i]
 *     jne ω
 *     inc rcx
 *     cmp rcx, litlen
 *     jl  loop top
 *   [γ port]
 *     mov rax, 1
 *     ret
 *   [ω port]
 *     mov rax, 0
 *     ret
 */
static box_fn build_lit_box(const char *lit, int litlen)
{
    /* allocate RW page */
    buf = mmap(NULL, 4096,
               PROT_READ | PROT_WRITE,
               MAP_ANON | MAP_PRIVATE,
               -1, 0);
    if (buf == MAP_FAILED) { perror("mmap"); return NULL; }
    pos = 0;

    /*
     * α entry:
     *   cmp esi, litlen   — 83 FE imm8  (litlen fits in imm8 for short strings)
     */
    eb(0x83); eb(0xFE); eb((uint8_t)litlen);   /* cmp esi, litlen */

    /*
     *   jl ω  — 7C rel8  (patch offset later)
     * We don't know ω's address yet. Emit a placeholder; patch after we know.
     */
    int jl_ω_pos = pos;   /* save position of the jl instruction */
    eb(0x7C); eb(0x00);   /* jl +0 (placeholder) */

    /*
     * mov rcx, 0  — 48 C7 C1 00000000
     */
    eb(0x48); eb(0xC7); eb(0xC1); e32(0x00000000);

    /*
     * Loop top: index in rcx
     *
     *   movzx eax, byte ptr [rdi + rcx]  — 0F B6 04 0F
     */
    int loop_top = pos;
    eb(0x0F); eb(0xB6); eb(0x04); eb(0x0F);

    /*
     * We emit one cmp+jne per literal byte.
     * Since litlen is small we unroll: each iteration compares lit[i] using
     * the current byte already in al, then increments rcx.
     *
     * Actually cleaner: the loop already loaded byte[rdi+rcx] into al.
     * We need to compare it against lit[rcx]. But rcx changes each iteration
     * and we can't index a compile-time array via rcx in position-independent
     * code without a data section. Simplest correct approach: unroll fully.
     *
     * Full unroll: for each i in [0..litlen):
     *   movzx eax, byte [rdi + i]   (0F B6 47 imm8)
     *   cmp al, lit[i]              (3C imm8)
     *   jne ω                       (75 rel8, patch later)
     *
     * Then γ, then ω.
     */

    /* We already emitted the loop_top movzx as dead code — back up */
    pos = loop_top;   /* rewind — we'll unroll instead */

    /* Save jne-to-ω patch sites */
    int jne_ω_sites[64];
    int jne_count = 0;

    for (int i = 0; i < litlen; i++) {
        /* movzx eax, byte ptr [rdi + i]  — 0F B6 47 imm8 */
        eb(0x0F); eb(0xB6); eb(0x47); eb((uint8_t)i);
        /* cmp al, lit[i]  — 3C imm8 */
        eb(0x3C); eb((uint8_t)(unsigned char)lit[i]);
        /* jne ω  — 75 rel8 (placeholder) */
        jne_ω_sites[jne_count++] = pos;
        eb(0x75); eb(0x00);
    }

    /*
     * γ port:
     *   mov eax, 1   — B8 01000000
     *   ret          — C3
     */
    int γ_pos = pos;
    (void)γ_pos;
    eb(0xB8); e32(0x00000001);
    eb(0xC3);

    /*
     * ω port:
     *   mov eax, 0   — B8 00000000
     *   ret          — C3
     */
    int ω_pos = pos;
    eb(0xB8); e32(0x00000000);
    eb(0xC3);

    /*
     * Patch all forward jumps to ω.
     * Each jump instruction is at site-1 (opcode) and site (rel8 operand).
     * rel8 = ω_pos - (site + 1)   [rel8 is relative to end of jmp instruction]
     */

    /* patch jl ω (the length check at entry) */
    buf[jl_ω_pos + 1] = (uint8_t)(ω_pos - (jl_ω_pos + 2));

    /* patch each jne ω */
    for (int i = 0; i < jne_count; i++) {
        int site = jne_ω_sites[i];
        buf[site + 1] = (uint8_t)(ω_pos - (site + 2));
    }

    /*
     * I-cache fence: mprotect RW → RX.
     * On x86-64 the data cache and instruction cache are separate.
     * The OS serializes at mprotect time. After this call the bytes
     * we wrote as data are visible to the instruction fetch unit.
     */
    if (mprotect(buf, 4096, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect");
        munmap(buf, 4096);
        return NULL;
    }

    return (box_fn)buf;
}

int main(void)
{
    int all_pass = 1;

    /* Build a Byrd box that matches the literal "hello" */
    box_fn hello_box = build_lit_box("hello", 5);
    if (!hello_box) { fprintf(stderr, "FATAL: box construction failed\n"); return 1; }

    /* Test cases */
    struct { const char *subject; int expect; } cases[] = {
        { "hello world", 1 },   /* γ: prefix matches */
        { "hello",       1 },   /* γ: exact match */
        { "goodbye",     0 },   /* ω: no match */
        { "hell",        0 },   /* ω: subject too short */
        { "HELLO",       0 },   /* ω: case mismatch */
        { "say hello",   0 },   /* ω: match not at position 0 */
    };
    int n = sizeof(cases) / sizeof(cases[0]);

    printf("bb_poc — Dynamic Byrd Box Proof of Concept\n");
    printf("Literal: \"hello\"  |  box @ %p\n\n", (void *)hello_box);

    for (int i = 0; i < n; i++) {
        const char *subj = cases[i].subject;
        int expect = cases[i].expect;
        int got = hello_box(subj, (int)strlen(subj));
        int ok = (got == expect);
        if (!ok) all_pass = 0;
        printf("  subject=%-14s  expect=%s  got=%s  %s\n",
               subj,
               expect ? "γ(MATCH)" : "ω(FAIL) ",
               got    ? "γ(MATCH)" : "ω(FAIL) ",
               ok     ? "✓" : "✗ WRONG");
    }

    printf("\n%s\n", all_pass ? "PASS" : "FAIL");

    munmap(buf, 4096);
    return all_pass ? 0 : 1;
}
