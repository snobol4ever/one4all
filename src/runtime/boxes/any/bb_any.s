; bb_any.s    _XANYC      match one char if in charset
; any_t: { const char *chars @0 }   chars = immutable ptr, kept in ζ
; .data: Δ_ptr, Ω_ptr, Σ_ptr, strchr_ptr
; α: Δ>=Ω → ω; !strchr(ζ->chars,Σ[Δ]) → ω; σ=Σ+Δ,δ=1,Δ++
; β: Δ--; ω

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_any

bb_any:
        mov     r10, rdi               ; r10 = ζ (any_t*)
        cmp     esi, 0
        je      ANY_α
        jmp     ANY_β
ANY_α:
        mov     r11, [rel any_Ω_ptr]
        mov     r11, [r11]              ; r11 = &Ω ... wait: Ω is int global
        ; correct: load ptr-to-int, then deref
        mov     r11, [rel any_Δ_ptr]
        mov     eax, dword [r11]        ; eax = Δ
        mov     r11, [rel any_Ω_ptr]
        cmp     eax, dword [r11]        ; Δ >= Ω ?
        jge     ANY_ω
        mov     r11, [rel any_Σ_ptr]
        mov     r11, [r11]              ; r11 = Σ (ptr)
        mov     r11, [rel any_Σ_ptr]
        ; reload cleanly:
        mov     rax, [rel any_Σ_ptr]
        mov     rax, [rax]              ; rax = Σ
        mov     r11, [rel any_Δ_ptr]
        movsxd  rcx, dword [r11]
        movzx   edi, byte [rax+rcx]     ; edi = Σ[Δ]  (char arg)
        mov     rsi, [r10]              ; rsi = ζ->chars
        xchg    rdi, rsi               ; rdi=chars, rsi=char (strchr ABI: (chars,c))
        call    qword [rel any_strchr_ptr]
        test    rax, rax
        jz      ANY_ω
        mov     rax, [rel any_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel any_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        mov     edx, 1                 ; δ = 1
        add     dword [r11], 1          ; Δ++
        ret
ANY_β:
        mov     r11, [rel any_Δ_ptr]
        sub     dword [r11], 1          ; Δ--
        jmp     ANY_ω
ANY_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
any_Δ_ptr:      dq 0
any_Ω_ptr:      dq 0
any_Σ_ptr:      dq 0
any_strchr_ptr: dq 0
