; bb_alt.s    _XOR        alternation: try each child on α; β retries same child
; alt_t: { int n @0; [pad4]; bb_altchild_t children[16] @8 (fn@+0,state@+8, each 16B);
;           int current @264; int position @268; spec_t result @272{σ@272,δ@280} }
; ABI: rdi=ζ, esi=entry; r10=ζ, r11/r12/r13=scratch (all caller-saved, no push/pop)
; .data: Δ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_alt

bb_alt:
        mov     r10, rdi               ; r10 = ζ (alt_t*)
        cmp     esi, 0
        je      ALT_α
        jmp     ALT_β

ALT_α:
        mov     r11, [rel alt_Δ_ptr]
        mov     eax, dword [r11]
        mov     dword [r10+268], eax   ; ζ->position = Δ
        mov     dword [r10+264], 1     ; ζ->current = 1
        mov     rdi, [r10+16]          ; children[0].state
        xor     esi, esi
        call    qword [r10+8]          ; children[0].fn
        mov     r12, rax
        mov     r13d, edx
        test    r12, r12
        jz      child_α_ω
        jmp     child_α_γ

ALT_β:
        mov     eax, dword [r10+264]   ; ζ->current
        dec     eax
        imul    eax, 16
        add     eax, 8
        movsxd  rcx, eax               ; offset of children[current-1]
        mov     rdi, [r10+rcx+8]       ; .state
        mov     esi, 1
        call    qword [r10+rcx]        ; .fn
        mov     r12, rax
        mov     r13d, edx
        test    r12, r12
        jz      ALT_ω
        jmp     child_β_γ

child_α_γ:
        mov     qword [r10+272], r12   ; ζ->result.σ
        mov     dword [r10+280], r13d  ; ζ->result.δ
        jmp     ALT_γ

child_α_ω:
        add     dword [r10+264], 1     ; ζ->current++
        mov     eax, dword [r10+264]
        cmp     eax, dword [r10+0]     ; > ζ->n?
        jg      ALT_ω
        mov     r11, [rel alt_Δ_ptr]
        mov     eax, dword [r10+268]
        mov     dword [r11], eax       ; Δ = ζ->position
        mov     eax, dword [r10+264]
        dec     eax
        imul    eax, 16
        add     eax, 8
        movsxd  rcx, eax
        mov     rdi, [r10+rcx+8]
        xor     esi, esi
        call    qword [r10+rcx]
        mov     r12, rax
        mov     r13d, edx
        test    r12, r12
        jz      child_α_ω
        jmp     child_α_γ

child_β_γ:
        mov     qword [r10+272], r12
        mov     dword [r10+280], r13d
        jmp     ALT_γ

ALT_γ:
        mov     rax, qword [r10+272]
        movsxd  rdx, dword [r10+280]
        ret

ALT_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
alt_Δ_ptr: dq 0
