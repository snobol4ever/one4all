; bb_notany.s _XNNYC      match one char if NOT in charset
; notany_t: { const char *chars @0 }
; .data: Δ_ptr, Ω_ptr, Σ_ptr, strchr_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_notany

bb_notany:
        mov     r10, rdi
        cmp     esi, 0
        je      NOTANY_α
        jmp     NOTANY_β
NOTANY_α:
        mov     r11, [rel notany_Δ_ptr]
        mov     eax, dword [r11]
        mov     r11, [rel notany_Ω_ptr]
        cmp     eax, dword [r11]
        jge     NOTANY_ω
        mov     rax, [rel notany_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel notany_Δ_ptr]
        movsxd  rcx, dword [r11]
        movzx   edi, byte [rax+rcx]
        mov     rsi, [r10]
        xchg    rdi, rsi
        call    qword [rel notany_strchr_ptr]
        test    rax, rax
        jnz     NOTANY_ω               ; found in set → NOT match
        mov     rax, [rel notany_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel notany_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx
        mov     edx, 1
        add     dword [r11], 1
        ret
NOTANY_β:
        mov     r11, [rel notany_Δ_ptr]
        sub     dword [r11], 1
        jmp     NOTANY_ω
NOTANY_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
notany_Δ_ptr:      dq 0
notany_Ω_ptr:      dq 0
notany_Σ_ptr:      dq 0
notany_strchr_ptr: dq 0
