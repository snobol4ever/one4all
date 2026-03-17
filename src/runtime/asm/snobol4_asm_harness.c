#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <setjmp.h>

uint64_t  cursor          = 0;
uint64_t  subject_len_val = 0;
char      subject_data[65536];
uint64_t  outer_cursor    = 0;
uint64_t  cap_len         = UINT64_MAX;
char      cap_buf[65536];

static jmp_buf scan_env;
#define JMP_FAIL 2

extern void root_alpha(void);

void match_success(void) __attribute__((noreturn));
void match_success(void) {
    if (cap_len != UINT64_MAX) {
        if (cap_len > 0)
            write(STDOUT_FILENO, cap_buf, (size_t)cap_len);
    } else {
        size_t start = (size_t)outer_cursor;
        size_t end   = (size_t)cursor;
        if (end > (size_t)subject_len_val) end = (size_t)subject_len_val;
        if (end > start)
            write(STDOUT_FILENO, subject_data + start, end - start);
    }
    write(STDOUT_FILENO, "\n", 1);
    exit(0);
}

void match_fail(void) __attribute__((noreturn));
void match_fail(void) { longjmp(scan_env, JMP_FAIL); }

static void __attribute__((noinline)) run_pattern(void) {
    __asm__ volatile ("jmp root_alpha\n\t" ::: "memory");
    __builtin_unreachable();
}

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: %s <subject>\n", argv[0]); return 1; }
    size_t slen = strlen(argv[1]);
    if (slen >= sizeof(subject_data) - 1) slen = sizeof(subject_data) - 1;
    memcpy(subject_data, argv[1], slen);
    subject_data[slen] = '\0';
    subject_len_val = (uint64_t)slen;
    for (uint64_t start = 0; start <= subject_len_val; start++) {
        cursor = start; outer_cursor = start;
        cap_len = UINT64_MAX;
        memset(cap_buf, 0, 256);
        if (setjmp(scan_env) == JMP_FAIL) continue;
        run_pattern();
        __builtin_unreachable();
    }
    exit(1);
}
