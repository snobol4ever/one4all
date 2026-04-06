; bb_span.s   _XSPNC      longest prefix in charset (≥1 required)
; span_t: { const char *chars @0; int δ @8 }   δ=mutable scan count
; .data: Δ_ptr, Ω_ptr, Σ_ptr, strchr_ptr

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_span

bb_span:
        mov     r10, rdi
        cmp     esi, 0
        je      SPAN_α
        jmp     SPAN_β
SPAN_α:
        mov     dword [r10+8], 0        ; ζ->δ = 0
        mov     r11, [rel span_Δ_ptr]
        mov     eax, dword [r11]        ; eax = Δ (scan base)
SPAN_loop:
        mov     ecx, dword [r10+8]      ; ecx = ζ->δ
        mov     edx, eax
        add     edx, ecx               ; Δ+δ
        mov     r11, [rel span_Ω_ptr]
        cmp     edx, dword [r11]
        jge     SPAN_done
        mov     rsi, [rel span_Σ_ptr]
        mov     rsi, [rsi]
        movsxd  rcx, edx
        movzx   edi, byte [rsi+rcx]    ; Σ[Δ+δ]
        mov     rsi, [r10]             ; ζ->chars
        xchg    rdi, rsi
        call    qword [rel span_strchr_ptr]
        test    rax, rax
        jz      SPAN_done
        add     dword [r10+8], 1        ; ζ->δ++
        jmp     SPAN_loop
SPAN_done:
        cmp     dword [r10+8], 0
        jle     SPAN_ω
        mov     rax, [rel span_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel span_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        mov     edx, dword [r10+8]     ; δ
        add     dword [r11], edx        ; Δ += δ
        ret
SPAN_β:
        mov     ecx, dword [r10+8]
        mov     r11, [rel span_Δ_ptr]
        sub     dword [r11], ecx
        jmp     SPAN_ω
SPAN_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
span_Δ_ptr:      dq 0
span_Ω_ptr:      dq 0
span_Σ_ptr:      dq 0
span_strchr_ptr: dq 0
