; bb_dvar.s   _XDSAR/_XVAR  *VAR — re-resolve live value on every α
; dvar_t: { const char *name @0; bb_box_fn child_fn @8; void *child_state @16;
;            size_t child_size @24 }
; .data: NV_GET_ptr, bb_lit_ptr (for DT_S rebuild), malloc_ptr
; ABI: r10=ζ, r11/r12/r13=scratch (no push/pop)
; Note: full rebuild logic delegated — this .s handles the common path only.
;       Complex rebuild (DT_P) falls through to C stub for now.

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_deferred_var

bb_deferred_var:
        mov     r10, rdi               ; r10 = ζ (dvar_t*)
        cmp     esi, 0
        je      DVAR_α
        jmp     DVAR_β
DVAR_α:
        ; Call NV_GET_fn(ζ->name) — returns DESCR_t in rax:rdx
        mov     rdi, [r10+0]           ; ζ->name
        call    qword [rel dvar_NV_GET_ptr]
        ; rax = DESCR_t low qword (v+slen), rdx = high qword (ptr/val)
        ; Check DT_S (v==3): if string, delegate to child (already set up by C path)
        ; For .s: just delegate to existing child_fn if set
        cmp     qword [r10+8], 0       ; child_fn == NULL?
        je      DVAR_ω
        mov     rdi, [r10+16]          ; child_state
        xor     esi, esi
        call    qword [r10+8]          ; child_fn(child_state, α)
        test    rax, rax
        jz      DVAR_ω
        ret
DVAR_β:
        cmp     qword [r10+8], 0
        je      DVAR_ω
        mov     rdi, [r10+16]
        mov     esi, 1
        call    qword [r10+8]          ; child_fn(child_state, β)
        test    rax, rax
        jz      DVAR_ω
        ret
DVAR_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
dvar_NV_GET_ptr: dq 0
