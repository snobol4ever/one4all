; bb_abort.s  _XABRT      always ω — same as FAIL

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_abort

bb_abort:
        xor     eax, eax
        xor     edx, edx
        ret
