; bb_lit.s    _XCHR       literal string match — no C call, memcmp via baked ptr
; lit_t: { const char *lit @0; int len @8 }   both immutable (set at build time)
; .data: Δ_ptr, Ω_ptr, Σ_ptr, memcmp_ptr  (baked by emitter)
; α: Δ+len>Ω → ω; memcmp(Σ+Δ,lit,len)≠0 → ω; σ=Σ+Δ,δ=len,Δ+=len
; β: Δ-=len; ω

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_lit

bb_lit:
        mov     r10, rdi               ; r10 = ζ (lit_t*)
        cmp     esi, 0
        je      LIT_α
        jmp     LIT_β
LIT_α:
        mov     r11, [rel lit_Δ_ptr]
        mov     eax, dword [r11]        ; eax = Δ
        add     eax, dword [r10+8]      ; eax = Δ + len
        mov     r11, [rel lit_Ω_ptr]
        cmp     eax, dword [r11]        ; > Ω ?
        jg      LIT_ω
        ; memcmp(Σ+Δ, ζ->lit, len)
        mov     rdi, [rel lit_Σ_ptr]
        mov     rdi, [rdi]              ; rdi = Σ
        mov     r11, [rel lit_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rdi, rcx               ; rdi = Σ+Δ  (arg1)
        mov     rsi, [r10]             ; rsi = ζ->lit  (arg2)
        movsxd  rdx, dword [r10+8]     ; rdx = len  (arg3)
        call    qword [rel lit_memcmp_ptr]
        test    eax, eax
        jne     LIT_ω
        mov     rax, [rel lit_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel lit_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        mov     edx, dword [r10+8]     ; δ = len
        add     dword [r11], edx        ; Δ += len
        ret
LIT_β:
        mov     ecx, dword [r10+8]     ; len
        mov     r11, [rel lit_Δ_ptr]
        sub     dword [r11], ecx        ; Δ -= len
        jmp     LIT_ω
LIT_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
lit_Δ_ptr:      dq 0
lit_Ω_ptr:      dq 0
lit_Σ_ptr:      dq 0
lit_memcmp_ptr: dq 0
