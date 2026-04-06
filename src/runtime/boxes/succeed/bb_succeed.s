; bb_succeed.s _XSUCF     always γ zero-width; outer loop retries
; No ζ state used. .data: Σ_ptr, Δ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_succeed

bb_succeed:
        mov     rax, [rel succ_Σ_ptr]
        mov     rax, [rax]              ; rax = Σ
        mov     r10, [rel succ_Δ_ptr]
        movsxd  rcx, dword [r10]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx               ; δ = 0
        ret

section .data
succ_Σ_ptr: dq 0
succ_Δ_ptr: dq 0
