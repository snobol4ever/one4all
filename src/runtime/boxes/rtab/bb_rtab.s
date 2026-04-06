; bb_rtab.s   _XRTB       advance cursor TO position Ω-n
; rtab_t: { int n @0; int advance @4 }
; .data: Δ_ptr, Ω_ptr, Σ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_rtab

bb_rtab:
        mov     r10, rdi
        cmp     esi, 0
        je      RTAB_α
        jmp     RTAB_β
RTAB_α:
        mov     r11, [rel rtab_Ω_ptr]
        mov     eax, dword [r11]        ; eax = Ω
        sub     eax, dword [r10]        ; eax = Ω-n (target)
        mov     r11, [rel rtab_Δ_ptr]
        mov     ecx, dword [r11]        ; ecx = Δ
        cmp     ecx, eax               ; Δ > Ω-n ?
        jg      RTAB_ω
        mov     edx, eax
        sub     edx, ecx               ; advance = (Ω-n)-Δ
        mov     dword [r10+4], edx      ; ζ->advance
        mov     r11, [rel rtab_Σ_ptr]
        mov     rax, [r11]
        movsxd  rcx, ecx               ; old Δ
        add     rax, rcx               ; σ = Σ+old_Δ
        mov     r11, [rel rtab_Δ_ptr]
        mov     ecx, dword [r10]
        mov     r11b, byte [rel rtab_Ω_ptr] ; reload Ω properly:
        ; set Δ = Ω-n: eax still holds Ω-n
        mov     r11, [rel rtab_Δ_ptr]
        mov     eax, dword [r10+4]      ; advance
        mov     ecx, dword [r11]
        add     ecx, eax
        ; recompute: target = old_Δ + advance
        ; just write target directly
        mov     r11, [rel rtab_Ω_ptr]
        mov     eax, dword [r11]
        sub     eax, dword [r10]        ; target = Ω-n
        mov     r11, [rel rtab_Δ_ptr]
        mov     dword [r11], eax        ; Δ = Ω-n
        ret
RTAB_β:
        mov     ecx, dword [r10+4]
        mov     r11, [rel rtab_Δ_ptr]
        sub     dword [r11], ecx
        jmp     RTAB_ω
RTAB_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
rtab_Δ_ptr: dq 0
rtab_Ω_ptr: dq 0
rtab_Σ_ptr: dq 0
