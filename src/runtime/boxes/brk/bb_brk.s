; bb_brk.s    _XBRKC      scan to first char in set (zero-width match OK)
; brk_t: { const char *chars @0; int δ @8 }
; .data: Δ_ptr, Ω_ptr, Σ_ptr, strchr_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_brk

bb_brk:
        mov     r10, rdi
        cmp     esi, 0
        je      BRK_α
        jmp     BRK_β
BRK_α:
        mov     dword [r10+8], 0
        mov     r11, [rel brk_Δ_ptr]
        mov     eax, dword [r11]
BRK_loop:
        mov     ecx, dword [r10+8]
        mov     edx, eax
        add     edx, ecx
        mov     r11, [rel brk_Ω_ptr]
        cmp     edx, dword [r11]
        jge     BRK_ω
        mov     rsi, [rel brk_Σ_ptr]
        mov     rsi, [rsi]
        movsxd  rcx, edx
        movzx   edi, byte [rsi+rcx]
        mov     rsi, [r10]
        xchg    rdi, rsi
        call    qword [rel brk_strchr_ptr]
        test    rax, rax
        jnz     BRK_found
        add     dword [r10+8], 1
        jmp     BRK_loop
BRK_found:
        mov     rax, [rel brk_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel brk_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx
        mov     edx, dword [r10+8]
        add     dword [r11], edx
        ret
BRK_β:
        mov     ecx, dword [r10+8]
        mov     r11, [rel brk_Δ_ptr]
        sub     dword [r11], ecx
        jmp     BRK_ω
BRK_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
brk_Δ_ptr:      dq 0
brk_Ω_ptr:      dq 0
brk_Σ_ptr:      dq 0
brk_strchr_ptr: dq 0
