; bb_breakx.s _XBRKX      BREAKX: scan to set char; fail if zero advance or hit end
; brkx_t: { const char *chars @0; int δ @8 }
; .data: Δ_ptr, Ω_ptr, Σ_ptr, strchr_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_breakx

bb_breakx:
        mov     r10, rdi
        cmp     esi, 0
        je      BRKX_α
        jmp     BRKX_β
BRKX_α:
        mov     dword [r10+8], 0
        mov     r11, [rel brkx_Δ_ptr]
        mov     eax, dword [r11]
BRKX_loop:
        mov     ecx, dword [r10+8]
        mov     edx, eax
        add     edx, ecx
        mov     r11, [rel brkx_Ω_ptr]
        cmp     edx, dword [r11]
        jge     BRKX_ω                 ; hit end without finding set char → ω
        mov     rsi, [rel brkx_Σ_ptr]
        mov     rsi, [rsi]
        movsxd  rcx, edx
        movzx   edi, byte [rsi+rcx]
        mov     rsi, [r10]
        xchg    rdi, rsi
        call    qword [rel brkx_strchr_ptr]
        test    rax, rax
        jnz     BRKX_found
        add     dword [r10+8], 1
        jmp     BRKX_loop
BRKX_found:
        cmp     dword [r10+8], 0
        je      BRKX_ω                 ; zero advance → ω
        mov     rax, [rel brkx_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel brkx_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx
        mov     edx, dword [r10+8]
        add     dword [r11], edx
        ret
BRKX_β:
        mov     ecx, dword [r10+8]
        mov     r11, [rel brkx_Δ_ptr]
        sub     dword [r11], ecx
        jmp     BRKX_ω
BRKX_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
brkx_Δ_ptr:      dq 0
brkx_Ω_ptr:      dq 0
brkx_Σ_ptr:      dq 0
brkx_strchr_ptr: dq 0
