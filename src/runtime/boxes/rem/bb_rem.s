; bb_rem.s    _XSTAR      REM — match entire remainder; β always ω
; ABI: rdi=ζ (ignored), esi=entry; r10/r11=scratch
; No mutable state. .data: Σ_ptr, Δ_ptr, Ω_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_rem

bb_rem:
        cmp     esi, 0
        jne     REM_ω
        mov     rax, [rel rem_Σ_ptr]
        mov     rax, [rax]              ; rax = Σ
        mov     r10, [rel rem_Δ_ptr]
        mov     r11d, dword [r10]       ; r11d = Δ
        movsxd  rcx, r11d
        add     rax, rcx               ; σ = Σ+Δ
        mov     r11, [rel rem_Ω_ptr]
        mov     edx, dword [r11]        ; edx = Ω
        mov     rcx, [rel rem_Δ_ptr]
        sub     edx, dword [rcx]        ; δ = Ω-Δ
        mov     rcx, [rel rem_Ω_ptr]
        mov     r11d, dword [rcx]
        mov     rcx, [rel rem_Δ_ptr]
        mov     dword [rcx], r11d       ; Δ = Ω
        ret
REM_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
rem_Σ_ptr: dq 0
rem_Δ_ptr: dq 0
rem_Ω_ptr: dq 0
