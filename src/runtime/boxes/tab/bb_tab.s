; bb_tab.s    _XTB        advance cursor TO absolute position n
; tab_t: { int n @0; int advance @4 }   advance = mutable
; .data: Δ_ptr, Σ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_tab

bb_tab:
        mov     r10, rdi
        cmp     esi, 0
        je      TAB_α
        jmp     TAB_β
TAB_α:
        mov     r11, [rel tab_Δ_ptr]
        mov     eax, dword [r11]        ; eax = Δ
        cmp     eax, dword [r10]        ; Δ > ζ->n ?
        jg      TAB_ω
        mov     ecx, dword [r10]        ; ecx = n
        sub     ecx, eax               ; ecx = advance = n-Δ
        mov     dword [r10+4], ecx      ; ζ->advance = n-Δ
        mov     r11, [rel tab_Σ_ptr]
        mov     rax, [r11]
        movsxd  rcx, eax               ; old Δ
        add     rax, rcx               ; σ = Σ+old_Δ
        mov     edx, dword [r10+4]      ; δ = advance
        mov     r11, [rel tab_Δ_ptr]
        mov     ecx, dword [r10]
        mov     dword [r11], ecx        ; Δ = n
        ret
TAB_β:
        mov     ecx, dword [r10+4]      ; ζ->advance
        mov     r11, [rel tab_Δ_ptr]
        sub     dword [r11], ecx        ; Δ -= advance
        jmp     TAB_ω
TAB_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
tab_Δ_ptr: dq 0
tab_Σ_ptr: dq 0
