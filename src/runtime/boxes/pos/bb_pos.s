; bb_pos.s    _XPOSI      assert cursor == n (zero-width)
; ABI: rdi=ζ (pos_t*), esi=entry; r10/r11=scratch
; pos_t: { int n @0 }
; .data: Δ_ptr, Σ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_pos

bb_pos:
        mov     r10, rdi               ; r10 = ζ
        cmp     esi, 0
        je      POS_α
        jmp     POS_ω
POS_α:
        mov     r11, [rel pos_Δ_ptr]
        mov     eax, dword [r11]        ; eax = Δ
        cmp     eax, dword [r10]        ; Δ == ζ->n ?
        jne     POS_ω
        mov     r11, [rel pos_Σ_ptr]
        mov     rax, [r11]              ; rax = Σ
        mov     r11, [rel pos_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx
        ret
POS_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
pos_Δ_ptr: dq 0
pos_Σ_ptr: dq 0
