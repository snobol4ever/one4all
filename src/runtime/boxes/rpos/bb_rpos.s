; bb_rpos.s   _XRPSI      assert cursor == Ω-n (zero-width)
; rpos_t: { int n @0 }
; .data: Δ_ptr, Ω_ptr, Σ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_rpos

bb_rpos:
        mov     r10, rdi
        cmp     esi, 0
        je      RPOS_α
        jmp     RPOS_ω
RPOS_α:
        mov     r11, [rel rpos_Ω_ptr]
        mov     eax, dword [r11]        ; eax = Ω
        sub     eax, dword [r10]        ; eax = Ω - ζ->n
        mov     r11, [rel rpos_Δ_ptr]
        cmp     eax, dword [r11]        ; == Δ ?
        jne     RPOS_ω
        mov     r11, [rel rpos_Σ_ptr]
        mov     rax, [r11]
        mov     r11, [rel rpos_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx
        ret
RPOS_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
rpos_Δ_ptr: dq 0
rpos_Ω_ptr: dq 0
rpos_Σ_ptr: dq 0
