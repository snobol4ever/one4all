; bb_fence.s  _XFNCE      succeed once; β cuts (no retry)
; ABI: rdi=ζ (fence_t*), esi=entry; r10/r11=scratch (no push/pop)
; fence_t: { int fired @0 }
; .data: Δ_ptr, Σ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_fence

bb_fence:
        mov     r10, rdi
        cmp     esi, 0
        je      FENCE_α
        jmp     FENCE_ω
FENCE_α:
        mov     dword [r10], 1          ; ζ->fired = 1
        mov     r11, [rel fence_Σ_ptr]
        mov     rax, [r11]              ; rax = Σ
        mov     r11, [rel fence_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx
        ret
FENCE_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
fence_Δ_ptr: dq 0
fence_Σ_ptr: dq 0
