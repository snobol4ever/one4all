; bb_bal.s    _XBAL       BAL — balanced parens (stub → ω)
; No state needed for stub.

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_bal

bb_bal:
        xor     eax, eax
        xor     edx, edx
        ret
