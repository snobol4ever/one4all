; bb_capture.s _XNME/_XFNME  conditional/immediate capture
; capture_t: { bb_box_fn fn @0; void *state @8; const char *varname @16;
;              int immediate @24; spec_t pending @32{σ@32,δ@40}; int has_pending @48 }
; .data: Σ_ptr, Δ_ptr, NV_SET_ptr, GC_malloc_ptr
; ABI: r10=ζ, r11/r12/r13=scratch (no push/pop)

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_capture

bb_capture:
        mov     r10, rdi
        cmp     esi, 0
        je      CAP_α
        jmp     CAP_β
CAP_α:
        mov     rdi, [r10+8]           ; child.state
        xor     esi, esi
        call    qword [r10+0]          ; child.fn(state, α)
        jmp     CAP_γ_core
CAP_β:
        mov     rdi, [r10+8]
        mov     esi, 1
        call    qword [r10+0]          ; child.fn(state, β)
CAP_γ_core:
        mov     r12, rax               ; cr.σ
        mov     r13d, edx              ; cr.δ
        test    r12, r12
        jz      CAP_ω
        cmp     qword [r10+16], 0      ; varname == NULL?
        je      .cap_done
        mov     r11, [r10+16]
        cmp     byte [r11], 0          ; *varname == 0?
        je      .cap_done
        cmp     dword [r10+24], 0      ; immediate?
        je      .cap_deferred
        ; immediate ($): alloc string, NV_SET_fn
        movsxd  rdi, r13d
        add     rdi, 1                 ; len+1
        call    qword [rel cap_GC_malloc_ptr]
        mov     r11, rax               ; r11 = new string buf
        mov     rdi, r11
        mov     rsi, r12               ; cr.σ
        movsxd  rdx, r13d
        ; memcpy via rep movsb
        push    rcx
        mov     rcx, rdx
        rep movsb
        pop     rcx
        mov     byte [r11+r13], 0      ; NUL terminate
        ; build DESCR_t{DT_S,slen=δ,s=buf} on stack
        sub     rsp, 16
        mov     dword [rsp+0], 3       ; DT_S=3
        mov     dword [rsp+4], r13d    ; slen
        mov     qword [rsp+8], r11     ; s
        mov     rdi, [r10+16]          ; varname
        mov     rsi, qword [rsp+0]
        mov     rdx, qword [rsp+8]
        call    qword [rel cap_NV_SET_ptr]
        add     rsp, 16
        jmp     .cap_done
.cap_deferred:
        ; deferred (.): store pending spec
        mov     qword [r10+32], r12    ; pending.σ
        mov     dword [r10+40], r13d   ; pending.δ
        mov     dword [r10+48], 1      ; has_pending
.cap_done:
        mov     rax, r12
        movsxd  rdx, r13d
        ret
CAP_ω:
        mov     dword [r10+48], 0      ; has_pending = 0
        xor     eax, eax
        xor     edx, edx
        ret

section .data
cap_Σ_ptr:        dq 0
cap_Δ_ptr:        dq 0
cap_NV_SET_ptr:   dq 0
cap_GC_malloc_ptr: dq 0
