; bb_arb.s    _XFARB      ARB — match 0..n chars lazily; β extends by 1
; arb_t: { int count @0; int start @4 }   both mutable
; .data: Δ_ptr, Ω_ptr, Σ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_arb

bb_arb:
        mov     r10, rdi               ; r10 = ζ (arb_t*)
        cmp     esi, 0
        je      ARB_α
        jmp     ARB_β
ARB_α:
        mov     dword [r10], 0          ; ζ->count = 0
        mov     r11, [rel arb_Δ_ptr]
        mov     eax, dword [r11]
        mov     dword [r10+4], eax      ; ζ->start = Δ
        mov     r11, [rel arb_Σ_ptr]
        mov     rax, [r11]
        mov     r11, [rel arb_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx               ; δ = 0
        ret
ARB_β:
        mov     eax, dword [r10]
        inc     eax
        mov     dword [r10], eax        ; ζ->count++
        mov     ecx, dword [r10+4]      ; ζ->start
        add     ecx, eax               ; start + count
        mov     r11, [rel arb_Ω_ptr]
        cmp     ecx, dword [r11]        ; > Ω ?
        jg      ARB_ω
        mov     r11, [rel arb_Δ_ptr]
        mov     ecx, dword [r10+4]
        mov     dword [r11], ecx        ; Δ = start
        mov     r11, [rel arb_Σ_ptr]
        mov     rax, [r11]
        movsxd  rcx, ecx
        add     rax, rcx               ; σ = Σ+start
        mov     edx, dword [r10]        ; δ = count
        mov     r11, [rel arb_Δ_ptr]
        add     dword [r11], edx        ; Δ += count
        ret
ARB_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
arb_Δ_ptr: dq 0
arb_Ω_ptr: dq 0
arb_Σ_ptr: dq 0
