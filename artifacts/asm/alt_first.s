; alt_first.s — Sprint A4 artifact
; Pattern: LIT("cat") | LIT("dog")   Subject: "cat"
; Expected: first arm matches → "cat\n" on stdout, exit 0
;
; ALT Byrd Box wiring (Proebsting §4.5 ifstmt, restricted to pattern match):
;
;   alt_α:    save cursor_at_alt; jump left_α
;   left_γ:   → alt_γ           (first arm succeeded)
;   left_ω:   restore cursor_at_alt; jump right_α
;   right_γ:  → alt_γ           (second arm succeeded)
;   right_ω:  → alt_ω           (both arms failed)
;   alt_β:    → right_β          (backtrack into whichever arm last succeeded;
;                                  left already cannot retry a LIT, so goes right)
;
; .bss layout:
;   cursor          — current match cursor (int64)
;   cursor_at_alt   — cursor saved when alt_α entered (for left_ω restore)
;   lit1_saved      — cursor saved before lit1 advance (for lit1_β restore)
;   lit2_saved      — cursor saved before lit2 advance (for lit2_β restore)
;
; assemble:  nasm -f elf64 alt_first.s -o alt_first.o
; link:      ld alt_first.o -o alt_first
; run:       ./alt_first && echo PASS || echo FAIL
;
; M-ASM-ALT fires when alt_first + alt_second + alt_fail all PASS.

    global _start

section .data

subject:    db "cat"
subj_len:   equ 3
lit1_str:   db "cat"
lit1_len:   equ 3
lit2_str:   db "dog"
lit2_len:   equ 3
newline:    db 10

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
; ALT α — save cursor, try left arm
; -----------------------------------------------------------------------
alt1_alpha:
    mov     rax, [cursor]
    mov     [cursor_at_alt], rax   ; save cursor at alt entry
    jmp     lit1_alpha             ; → left arm α

; ALT β — backtrack: left LIT can't retry, go to right arm β
alt1_beta:
    jmp     lit2_beta

; ALT γ — either arm succeeded: print match, exit 0
alt1_gamma:
    ; determine which arm matched and where the span starts
    ; cursor_at_alt = start of match, cursor = end of match
    mov     rax, 1                 ; sys_write
    mov     rdi, 1                 ; stdout
    lea     rsi, [rel subject]
    mov     rcx, [cursor_at_alt]
    add     rsi, rcx               ; subject + match_start
    mov     rdx, [cursor]
    mov     rcx, [cursor_at_alt]
    sub     rdx, rcx               ; length = cursor - match_start
    syscall

    mov     rax, 1
    mov     rdi, 1
    lea     rsi, [rel newline]
    mov     rdx, 1
    syscall

    mov     eax, 60
    xor     edi, edi
    syscall

; ALT ω — both arms failed
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
    ; fall through to omega

lit1_omega:
    ; left arm failed: restore cursor to alt entry, try right arm
    mov     rax, [cursor_at_alt]
    mov     [cursor], rax
    jmp     lit2_alpha             ; CAT: left_ω → right_α

lit1_gamma:
    jmp     alt1_gamma             ; left arm succeeded → alt_γ

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
    ; fall through to omega

lit2_omega:
    jmp     alt1_omega             ; both arms failed → alt_ω

lit2_gamma:
    jmp     alt1_gamma             ; right arm succeeded → alt_γ
