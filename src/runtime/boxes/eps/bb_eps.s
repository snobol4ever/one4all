; bb_eps.s    _XEPS       zero-width success once; done flag prevents double-γ
; ABI: rdi=ζ (eps_t*), esi=entry; r10/r11=scratch (no push/pop)
; eps_t: { int done @0 }
; .data: Δ_ptr (qword &Δ), Σ_ptr (qword &Σ) — baked by emitter, rip-relative

section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global bb_eps

bb_eps:
        mov     r10, rdi                ; r10 = ζ (eps_t*)
        cmp     esi, 0
        je      EPS_α
        jmp     EPS_ω
EPS_α:
        mov     dword [r10], 0          ; ζ->done = 0  (reset on α entry)
        cmp     dword [r10], 0
        jne     EPS_ω
        mov     dword [r10], 1          ; ζ->done = 1
        mov     r11, [rel eps_Σ_ptr]    ; r11 = &Σ (baked ptr-to-ptr)
        mov     rax, [r11]              ; rax = Σ
        mov     r11, [rel eps_Δ_ptr]    ; r11 = &Δ
        movsxd  rcx, dword [r11]       ; rcx = Δ
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx               ; δ = 0
        ret
EPS_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
eps_Δ_ptr: dq 0          ; emitter bakes: absolute address of Δ global
eps_Σ_ptr: dq 0          ; emitter bakes: absolute address of Σ global
