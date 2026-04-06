; bb_atp.s    _XATP       @var — write cursor Δ as DT_I into varname; no backtrack
; atp_t: { int done @0; [pad4]; const char *varname @8 }
; .data: Σ_ptr, Δ_ptr, NV_SET_ptr
; DESCR_t layout: dword v @0, dword slen @4, qword i @8 — DT_I=6
; NV_SET_fn(const char *name, DESCR_t v): rdi=name, rsi=v[0..7], rdx=v[8..15]
; Stack used for DESCR_t temp (16 bytes) — SysV requires 16-byte alignment before call

section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global bb_atp

bb_atp:
        mov     r10, rdi               ; r10 = ζ (atp_t*)
        cmp     esi, 0
        je      ATP_α
        jmp     ATP_ω
ATP_α:
        mov     dword [r10], 1         ; ζ->done = 1
        mov     r11, [r10+8]           ; ζ->varname
        test    r11, r11
        jz      .atp_noset
        cmp     byte [r11], 0
        je      .atp_noset
        ; build DESCR_t on stack: v=DT_I=6, slen=0, i=Δ
        sub     rsp, 16
        mov     dword [rsp+0], 6       ; DT_I
        mov     dword [rsp+4], 0       ; slen
        mov     rax, [rel atp_Δ_ptr]
        movsxd  rax, dword [rax]       ; rax = Δ
        mov     qword [rsp+8], rax     ; i = Δ
        mov     rdi, r11               ; name
        mov     rsi, qword [rsp+0]     ; v low qword
        mov     rdx, qword [rsp+8]     ; v high qword
        call    qword [rel atp_NV_SET_ptr]
        add     rsp, 16
.atp_noset:
        mov     rax, [rel atp_Σ_ptr]
        mov     rax, [rax]
        mov     r11, [rel atp_Δ_ptr]
        movsxd  rcx, dword [r11]
        add     rax, rcx               ; σ = Σ+Δ
        xor     edx, edx
        ret
ATP_ω:
        xor     eax, eax
        xor     edx, edx
        ret

section .data
atp_Σ_ptr:      dq 0
atp_Δ_ptr:      dq 0
atp_NV_SET_ptr: dq 0
