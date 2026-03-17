; notany_consonant.s — Sprint A6 artifact
; Pattern: NOTANY("aeiou") . V   Subject: "hello"
; Expected: "h\n" on stdout, exit 0
; NOTANY at cursor 0: 'h' not in "aeiou" → match, advance cursor, succeed.
;
; Anchored at cursor 0 (first char 'h' already satisfies NOTANY).
;
; assemble:  nasm -f elf64 notany_consonant.s -o notany_consonant.o
; link:      ld notany_consonant.o -o notany_consonant
; run:       ./notany_consonant && echo PASS || echo FAIL

    global _start

section .data

subject:    db "hello"
subj_len:   equ 5
charset:    db "aeiou"
charset_len equ 5
newline:    db 10

section .bss

cursor:     resq 1
match_start: resq 1

section .text

_start:
    mov     qword [cursor], 0
    jmp     outer_loop

outer_loop:
    mov     rax, [cursor]
    cmp     rax, subj_len
    jge     match_fail

    mov     [match_start], rax

    lea     rbx, [rel subject]
    movzx   ecx, byte [rbx + rax]

    ; scan charset
    lea     rsi, [rel charset]
    mov     rdx, charset_len
    xor     r8d, r8d
.scan:
    cmp     r8, rdx
    jge     .not_in_set         ; char NOT in set → NOTANY succeeds
    movzx   r9d, byte [rsi + r8]
    cmp     ecx, r9d
    je      .in_set             ; char IS in set → NOTANY fails
    inc     r8
    jmp     .scan

.in_set:
    ; NOTANY fails at this position → advance outer cursor, retry
    mov     rax, [cursor]
    inc     rax
    mov     [cursor], rax
    jmp     outer_loop

.not_in_set:
    ; NOTANY succeeds → advance cursor by 1
    mov     rax, [cursor]
    inc     rax
    mov     [cursor], rax
    jmp     notany_gamma

notany_gamma:
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
