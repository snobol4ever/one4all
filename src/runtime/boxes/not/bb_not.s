; bb_not.s    _XNOT(neg)  \X — succeed iff child fails; β ω
; not_t: { bb_box_fn fn @0; void *state @8; int start @16 }
; .data: Σ_ptr, Δ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_not

bb_not:
        mov     r10, rdi
        cmp     esi, 0
        je      NOT_α
        jmp     NOT_ω
NOT_α:
        mov     r11, [rel not_Δ_ptr]
        mov     eax, dword [r11]
        mov     dword [r10+16], eax    ; ζ->start = Δ
        mov     rdi, [r10+8]
        xor     esi, esi
        call    qword [r10]            ; ζ->fn(ζ->state, α)
        test    rax, rax
        jnz     NOT_ω                  ; child succeeded → NOT fails
        mov     r11, [rel not_Δ_ptr]
        mov     eax, dword [r10+16]
        mov     dword [r11], eax       ; Δ = start
        mov     rax, [rel not_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel not_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx
        ret
NOT_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
not_Σ_ptr: dq 0
not_Δ_ptr: dq 0
