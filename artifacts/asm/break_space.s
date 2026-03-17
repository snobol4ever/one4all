; break_space.s — Sprint A6 artifact
; Pattern: BREAK(" ") . V   Subject: "hello world"
; Expected: "hello\n" on stdout, exit 0
;
; BREAK(S) semantics (v311.sil BRKC):
;   Advance cursor while subject[cursor] ∉ S (i.e. stop BEFORE a char in S).
;   Must match at least 1 char.
;   Cursor stops at the break character (not consumed).
;
; Anchored: cursor starts at 0. "hello world" → scan until ' ' → 5 chars.
;
; assemble:  nasm -f elf64 break_space.s -o break_space.o
; link:      ld break_space.o -o break_space
; run:       ./break_space && echo PASS || echo FAIL

    global _start

section .data

subject:    db "hello world"
subj_len:   equ 11
charset:    db " "             ; break on space
charset_len equ 1
newline:    db 10

section .bss

cursor:      resq 1
match_start: resq 1

section .text

_start:
    mov     qword [cursor], 0
    jmp     break_alpha

; -----------------------------------------------------------------------
; BREAK α — scan while NOT in set, stop before break char
; -----------------------------------------------------------------------
break_alpha:
    mov     rax, [cursor]
    mov     [match_start], rax

break_loop:
    mov     rax, [cursor]
    cmp     rax, subj_len
    jge     break_check_progress  ; end of subject

    lea     rbx, [rel subject]
    movzx   ecx, byte [rbx + rax]

    ; is char in break set?
    lea     rsi, [rel charset]
    mov     rdx, charset_len
    xor     r8d, r8d
.scan:
    cmp     r8, rdx
    jge     .not_break           ; char NOT in set → keep scanning
    movzx   r9d, byte [rsi + r8]
    cmp     ecx, r9d
    je      break_check_progress ; char IS in set → stop here
    inc     r8
    jmp     .scan
.not_break:
    ; char not in break set → advance cursor
    inc     qword [cursor]
    jmp     break_loop

break_check_progress:
    ; must have consumed at least 1 char
    mov     rax, [cursor]
    mov     rbx, [match_start]
    cmp     rax, rbx
    je      match_fail

    jmp     break_gamma

; -----------------------------------------------------------------------
; BREAK γ — print matched span (not including break char)
; -----------------------------------------------------------------------
break_gamma:
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
