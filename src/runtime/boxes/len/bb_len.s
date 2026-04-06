; bb_len.s    _XLNTH      match exactly n characters (byte count)
; len_t: { int n @0; int bspan @4 }   bspan = mutable, computed on α
; .data: Δ_ptr, Ω_ptr, Σ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_len

bb_len:
        mov     r10, rdi               ; r10 = ζ (len_t*)
        cmp     esi, 0
        je      LEN_α
        jmp     LEN_β
LEN_α:
        mov     r11, [rel len_Δ_ptr]
        mov     eax, dword [r11]        ; eax = Δ
        add     eax, dword [r10]        ; eax = Δ + ζ->n
        mov     r11, [rel len_Ω_ptr]
        cmp     eax, dword [r11]        ; > Ω ?
        jg      LEN_ω
        mov     edx, dword [r10]        ; edx = ζ->n (= bspan for ASCII)
        mov     dword [r10+4], edx      ; ζ->bspan = n
        mov     r11, [rel len_Σ_ptr]
        mov     rax, [r11]              ; rax = Σ
        mov     r11, [rel len_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        add     dword [r11], edx        ; Δ += n
        ret
LEN_β:
        mov     edx, dword [r10+4]      ; ζ->bspan
        mov     r11, [rel len_Δ_ptr]
        sub     dword [r11], edx        ; Δ -= bspan
        jmp     LEN_ω
LEN_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
len_Δ_ptr: dq 0
len_Ω_ptr: dq 0
len_Σ_ptr: dq 0
