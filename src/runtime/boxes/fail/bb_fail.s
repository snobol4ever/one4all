; bb_fail.s   _XFAIL      always ω — force backtrack
; ABI: rdi=ζ (ignored), esi=entry (ignored)
; No state. No .data needed. 5 bytes.

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_fail

bb_fail:
        xor     eax, eax
        xor     edx, edx
        ret
