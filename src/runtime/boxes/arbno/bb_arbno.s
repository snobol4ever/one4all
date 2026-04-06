; bb_arbno.s  _XARBN      zero-or-more greedy; zero-advance guard; β unwinds stack
; arbno_t: { bb_box_fn fn @0; void *state @8; int depth @16; [pad4];
;             arbno_frame_t stack[64] @24 }
; arbno_frame_t: { spec_t matched @0{σ@0,δ@8}; int start @16 }  sizeof=24
; stack[i] offset = 24 + i*24
; ABI: r10=ζ, r11/r12/r13/r14/r15=scratch (all caller-saved, no push/pop)
; .data: Σ_ptr, Δ_ptr

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_arbno

; FRAME_PTR macro: compute offset of ζ->stack[ζ->depth] into r11
%macro FRAME_OFF 0
        mov     eax, dword [r10+16]    ; ζ->depth
        imul    eax, 24
        add     eax, 24
        movsxd  r11, eax               ; r11 = frame byte offset from ζ
%endmacro

bb_arbno:
        mov     r10, rdi               ; r10 = ζ
        cmp     esi, 0
        je      ARBNO_α
        jmp     ARBNO_β

ARBNO_α:
        mov     dword [r10+16], 0      ; ζ->depth = 0
        mov     rax, [rel arbn_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel arbn_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx
        mov     qword [r10+24], rax    ; stack[0].matched.σ = Σ+Δ
        mov     dword [r10+32], 0      ; stack[0].matched.δ = 0
        mov     r11, [rel arbn_Δ_ptr]
        mov     eax, dword [r11]
        mov     dword [r10+40], eax    ; stack[0].start = Δ
        jmp     ARBNO_try

ARBNO_try:
        mov     rdi, [r10+8]           ; ζ->state
        xor     esi, esi
        call    qword [r10+0]          ; ζ->fn(state, α)
        mov     r12, rax               ; br.σ
        mov     r13d, edx              ; br.δ
        test    r12, r12
        jz      body_ω
        jmp     body_γ

ARBNO_β:
        cmp     dword [r10+16], 0
        jle     ARBNO_ω
        sub     dword [r10+16], 1
        FRAME_OFF                      ; r11 = frame offset
        mov     eax, dword [r10+r11+16] ; fr->start
        mov     r14, [rel arbn_Δ_ptr]
        mov     dword [r14], eax       ; Δ = fr->start
        jmp     ARBNO_γ

body_γ:
        FRAME_OFF
        mov     r14, [rel arbn_Δ_ptr]
        mov     eax, dword [r14]       ; Δ
        cmp     eax, dword [r10+r11+16] ; == fr->start?
        je      ARBNO_γ_now
        ; ARBNO = spec_cat(fr->matched, br)
        mov     r14, qword [r10+r11+0]  ; fr->matched.σ
        mov     eax, dword [r10+r11+8]  ; fr->matched.δ
        add     eax, r13d              ; + br.δ
        mov     r15d, eax
        ; push new frame if depth+1 < 64
        mov     eax, dword [r10+16]
        inc     eax
        cmp     eax, 64
        jge     .no_push
        mov     dword [r10+16], eax
        FRAME_OFF
        mov     qword [r10+r11+0], r14
        mov     dword [r10+r11+8], r15d
        mov     r14, [rel arbn_Δ_ptr]
        mov     eax, dword [r14]
        mov     dword [r10+r11+16], eax
.no_push:
        jmp     ARBNO_try

body_ω:
        FRAME_OFF
        mov     rax, qword [r10+r11+0]
        movsxd  rdx, dword [r10+r11+8]
        jmp     ARBNO_γ

ARBNO_γ_now:
        FRAME_OFF
        mov     rax, qword [r10+r11+0]
        movsxd  rdx, dword [r10+r11+8]

ARBNO_γ:
        ret

ARBNO_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
arbn_Σ_ptr: dq 0
arbn_Δ_ptr: dq 0
