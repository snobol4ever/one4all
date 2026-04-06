; bb_interr.s _XNOT(interr) ?X — succeed zero-width if child succeeds; β ω
; interr_t: { bb_box_fn fn @0; void *state @8; int start @16 }
; .data: Σ_ptr, Δ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_interr

bb_interr:
        mov     r10, rdi               ; r10 = ζ
        cmp     esi, 0
        je      INT_α
        jmp     INT_ω
INT_α:
        mov     r11, [rel interr_Δ_ptr]
        mov     eax, dword [r11]
        mov     dword [r10+16], eax    ; ζ->start = Δ
        mov     rdi, [r10+8]           ; ζ->state
        xor     esi, esi
        call    qword [r10]            ; ζ->fn(ζ->state, α)
        test    rax, rax
        jz      INT_ω
        mov     r11, [rel interr_Δ_ptr]
        mov     eax, dword [r10+16]
        mov     dword [r11], eax       ; Δ = start (restore)
        mov     rax, [rel interr_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel interr_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx
        ret
INT_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
interr_Σ_ptr: dq 0
interr_Δ_ptr: dq 0
