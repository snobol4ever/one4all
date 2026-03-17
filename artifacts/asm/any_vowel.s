; any_vowel.s — Sprint A6 artifact
; Pattern: ANY("aeiou") . V   Subject: "hello"
; Expected: "e\n" on stdout, exit 0
; (ANY scans subject from cursor for first char in set; our implicit cursor
;  starts at 0 but SNOBOL4 ANY scans for the FIRST matching char anywhere —
;  actually: ANY matches exactly one char at cursor position if it's in S.
;  "hello" at cursor 0 = 'h' not in "aeiou" → fail. But the statement-level
;  match tries all cursor positions (implicit anchor off).
;  For our standalone ASM oracle we'll start cursor at 0 and advance until
;  ANY succeeds — mirroring the unanchored match loop.)
;
; Unanchored match loop: try ANY at cursor 0,1,2,... until success or end.
;
; ANY(S) semantics (v311.sil ANYC):
;   Check subject[cursor] ∈ S. If yes: advance cursor by 1, succeed.
;   If no: fail (no cursor saved — ANY has no β state).
;
; assemble:  nasm -f elf64 any_vowel.s -o any_vowel.o
; link:      ld any_vowel.o -o any_vowel
; run:       ./any_vowel && echo PASS || echo FAIL

    global _start

section .data

subject:    db "hello"
subj_len:   equ 5
charset:    db "aeiou"
charset_len equ 5
newline:    db 10

section .bss

cursor:         resq 1      ; current match position
match_start:    resq 1      ; cursor when ANY α entered (span start for . V)

section .text

_start:
    mov     qword [cursor], 0
    jmp     outer_loop

; -----------------------------------------------------------------------
; Outer unanchored loop: try ANY at each cursor position
; -----------------------------------------------------------------------
outer_loop:
    mov     rax, [cursor]
    cmp     rax, subj_len
    jge     match_fail          ; exhausted subject → fail

    mov     [match_start], rax  ; save span start

    ; load subject[cursor]
    lea     rbx, [rel subject]
    movzx   ecx, byte [rbx + rax]   ; ecx = subject[cursor]

    ; scan charset for this character
    lea     rsi, [rel charset]
    mov     rdx, charset_len
    xor     r8d, r8d             ; i = 0
.scan:
    cmp     r8, rdx
    jge     .not_found
    movzx   r9d, byte [rsi + r8]
    cmp     ecx, r9d
    je      .found
    inc     r8
    jmp     .scan
.not_found:
    ; char not in set → advance outer cursor, retry
    mov     rax, [cursor]
    inc     rax
    mov     [cursor], rax
    jmp     outer_loop

.found:
    ; char in set → advance match cursor by 1, succeed
    mov     rax, [cursor]
    inc     rax
    mov     [cursor], rax
    jmp     any_gamma

; -----------------------------------------------------------------------
; ANY γ — print matched char (subject[match_start .. cursor))
; -----------------------------------------------------------------------
any_gamma:
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
