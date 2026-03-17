; alt_fail.s — Sprint A4 artifact
; Pattern: LIT("cat") | LIT("dog")   Subject: "fish"
; Expected: both arms fail → exit 1 (no output)
;
; Same ALT wiring — subject "fish" matches neither arm.
;
; assemble:  nasm -f elf64 alt_fail.s -o alt_fail.o
; link:      ld alt_fail.o -o alt_fail
; run:       ./alt_fail; echo $?   # must print 1

    global _start

section .data

subject:    db "fish"
subj_len:   equ 4
lit1_str:   db "cat"
lit1_len:   equ 3
lit2_str:   db "dog"
lit2_len:   equ 3

section .bss

cursor:         resq 1
cursor_at_alt:  resq 1
lit1_saved:     resq 1
lit2_saved:     resq 1

section .text

_start:
    mov     qword [cursor], 0
    jmp     alt1_alpha

; -----------------------------------------------------------------------
; ALT α/β/γ/ω
; -----------------------------------------------------------------------
alt1_alpha:
    mov     rax, [cursor]
    mov     [cursor_at_alt], rax
    jmp     lit1_alpha

alt1_beta:
    jmp     lit2_beta

alt1_gamma:
    ; (unreachable in this test — both arms fail)
    mov     eax, 60
    xor     edi, edi
    syscall

alt1_omega:
    mov     eax, 60
    mov     edi, 1
    syscall

; -----------------------------------------------------------------------
; LIT("cat") — left arm
; -----------------------------------------------------------------------
lit1_alpha:
    mov     rax, [cursor]
    add     rax, lit1_len
    cmp     rax, subj_len
    jg      lit1_omega

    lea     rsi, [rel subject]
    mov     rcx, [cursor]
    add     rsi, rcx
    lea     rdi, [rel lit1_str]
    mov     rcx, lit1_len
    repe    cmpsb
    jne     lit1_omega

    mov     rax, [cursor]
    mov     [lit1_saved], rax
    add     rax, lit1_len
    mov     [cursor], rax
    jmp     lit1_gamma

lit1_beta:
    mov     rax, [lit1_saved]
    mov     [cursor], rax

lit1_omega:
    mov     rax, [cursor_at_alt]
    mov     [cursor], rax
    jmp     lit2_alpha

lit1_gamma:
    jmp     alt1_gamma

; -----------------------------------------------------------------------
; LIT("dog") — right arm
; -----------------------------------------------------------------------
lit2_alpha:
    mov     rax, [cursor]
    add     rax, lit2_len
    cmp     rax, subj_len
    jg      lit2_omega

    lea     rsi, [rel subject]
    mov     rcx, [cursor]
    add     rsi, rcx
    lea     rdi, [rel lit2_str]
    mov     rcx, lit2_len
    repe    cmpsb
    jne     lit2_omega

    mov     rax, [cursor]
    mov     [lit2_saved], rax
    add     rax, lit2_len
    mov     [cursor], rax
    jmp     lit2_gamma

lit2_beta:
    mov     rax, [lit2_saved]
    mov     [cursor], rax

lit2_omega:
    jmp     alt1_omega

lit2_gamma:
    jmp     alt1_gamma
