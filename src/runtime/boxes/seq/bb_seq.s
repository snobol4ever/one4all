; bb_seq.s    _XCAT       concatenation: left then right; β retries right then left
; seq_t: { bb_child_t left @0{fn@0,state@8}; bb_child_t right @16{fn@16,state@24};
;           spec_t matched @32{σ@32,δ@40} }
; ABI: rdi=ζ, esi=entry; r10=ζ scratch, r11/r12/r13/r14=intermediate results
; r10-r15 are caller-saved in SysV ABI — no push/pop needed
; .data: Σ_ptr, Δ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_seq

bb_seq:
        mov     r10, rdi               ; r10 = ζ (seq_t*)
        cmp     esi, 0
        je      SEQ_α
        jmp     SEQ_β

SEQ_α:
        mov     rax, [rel seq_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel seq_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx
        mov     qword [r10+32], rax    ; ζ->matched.σ = Σ+Δ
        mov     dword [r10+40], 0      ; ζ->matched.δ = 0
        mov     rdi, [r10+8]           ; left.state
        xor     esi, esi
        call    qword [r10+0]          ; left.fn(state, α)
        mov     r12, rax               ; lr.σ
        mov     r13d, edx              ; lr.δ
        test    r12, r12
        jz      left_ω
        jmp     left_γ

SEQ_β:
        mov     rdi, [r10+24]          ; right.state
        mov     esi, 1
        call    qword [r10+16]         ; right.fn(state, β)
        mov     r12, rax               ; rr.σ
        mov     r13d, edx              ; rr.δ
        test    r12, r12
        jz      right_ω
        jmp     right_γ

left_γ:
        mov     eax, dword [r10+40]
        add     eax, r13d              ; matched.δ += lr.δ
        mov     dword [r10+40], eax
        mov     rdi, [r10+24]
        xor     esi, esi
        call    qword [r10+16]         ; right.fn(state, α)
        mov     r12, rax
        mov     r13d, edx
        test    r12, r12
        jz      right_ω
        jmp     right_γ

left_ω:
        jmp     SEQ_ω

right_γ:
        mov     rax, qword [r10+32]    ; ζ->matched.σ
        mov     ecx, dword [r10+40]
        add     ecx, r13d              ; matched.δ + rr.δ
        movsxd  rdx, ecx
        ret                            ; return spec(matched.σ, matched.δ+rr.δ)

right_ω:
        ; lr = left.fn(state, β)
        mov     eax, dword [r10+40]
        sub     eax, r13d              ; undo lr contribution
        mov     dword [r10+40], eax
        mov     rdi, [r10+8]
        mov     esi, 1
        call    qword [r10+0]          ; left.fn(state, β)
        mov     r12, rax
        mov     r13d, edx
        test    r12, r12
        jz      left_ω
        jmp     left_γ

SEQ_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
seq_Σ_ptr: dq 0
seq_Δ_ptr: dq 0
