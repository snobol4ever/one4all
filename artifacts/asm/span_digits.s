; span_digits.s — Sprint A6 artifact
; Pattern: SPAN("0123456789") . V   Subject: "12345abc"
; Expected: "12345\n" on stdout, exit 0
;
; SPAN(S) semantics (v311.sil SPNC):
;   Advance cursor while subject[cursor] ∈ S.
;   Must match at least 1 char (fail if zero chars consumed).
;   No backtrack state — SPAN always matches the maximum greedy run.
;
; Anchored: cursor starts at 0. "12345abc" → digits run = 5 chars.
;
; assemble:  nasm -f elf64 span_digits.s -o span_digits.o
; link:      ld span_digits.o -o span_digits
; run:       ./span_digits && echo PASS || echo FAIL

    global _start

section .data

subject:    db "12345abc"
subj_len:   equ 8
charset:    db "0123456789"
charset_len equ 10
newline:    db 10

section .bss

cursor:      resq 1
match_start: resq 1

section .text

_start:
    mov     qword [cursor], 0
    jmp     span_alpha

; -----------------------------------------------------------------------
; SPAN α — scan while in set, require at least 1 char
; -----------------------------------------------------------------------
span_alpha:
    mov     rax, [cursor]
    mov     [match_start], rax   ; save span start

span_loop:
    mov     rax, [cursor]
    cmp     rax, subj_len
    jge     span_check_progress  ; hit end of subject

    lea     rbx, [rel subject]
    movzx   ecx, byte [rbx + rax]

    ; is char in charset?
    lea     rsi, [rel charset]
    mov     rdx, charset_len
    xor     r8d, r8d
.scan:
    cmp     r8, rdx
    jge     span_check_progress  ; char not in set → stop scan
    movzx   r9d, byte [rsi + r8]
    cmp     ecx, r9d
    je      .in_set
    inc     r8
    jmp     .scan
.in_set:
    ; char in set → advance cursor
    inc     qword [cursor]
    jmp     span_loop

span_check_progress:
    ; must have consumed at least 1 char
    mov     rax, [cursor]
    mov     rbx, [match_start]
    cmp     rax, rbx
    je      match_fail           ; zero chars → fail

    jmp     span_gamma

; -----------------------------------------------------------------------
; SPAN γ — print matched span
; -----------------------------------------------------------------------
span_gamma:
    mov     rax, 1
    mov     rdi, 1
    lea     rsi, [rel subject]
    mov     rcx, [match_start]
    add     rsi, rcx
    mov     rdx, [cursor]
    mov     rcx, [match_start]
    sub     rdx, rcx
    syscall

    mov     rax, 1
    mov     rdi, 1
    lea     rsi, [rel newline]
    mov     rdx, 1
    syscall

    mov     eax, 60
    xor     edi, edi
    syscall

match_fail:
    mov     eax, 60
    mov     edi, 1
    syscall
