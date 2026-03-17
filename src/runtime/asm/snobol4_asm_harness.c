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

/* cap_order[] is a null-terminated array of {name*, buf*, len*} triples
 * exported by the body .s when captures are present.
 * When no captures exist the body exports nothing and we fall back to
 * printing the matched span. */
typedef struct { const char *name; char *buf; uint64_t *len; } CapEntry;

/* Weak symbols so link succeeds even when body has no captures */
extern CapEntry   cap_order[]       __attribute__((weak));
extern uint64_t   cap_order_count   __attribute__((weak));

static jmp_buf scan_env;
#define JMP_FAIL 2

extern void root_alpha(void);

void match_success(void) __attribute__((noreturn));
void match_success(void) {
    /* If the body exported a cap_order table, print captures in order */
    if (&cap_order != NULL && &cap_order_count != NULL && cap_order_count > 0) {
        for (uint64_t i = 0; i < cap_order_count; i++) {
            uint64_t len = *cap_order[i].len;
            if (len == UINT64_MAX) continue;
            if (len > 0) write(STDOUT_FILENO, cap_order[i].buf, (size_t)len);
            write(STDOUT_FILENO, "\n", 1);
        }
    } else {
        /* No captures — print matched span */
        size_t start = (size_t)outer_cursor;
        size_t end   = (size_t)cursor;
        if (end > (size_t)subject_len_val) end = (size_t)subject_len_val;
        if (end > start)
            write(STDOUT_FILENO, subject_data + start, end - start);
        write(STDOUT_FILENO, "\n", 1);
    }
    exit(0);
}

void match_fail(void) __attribute__((noreturn));
void match_fail(void) { longjmp(scan_env, JMP_FAIL); }

static void __attribute__((noinline)) run_pattern(void) {
    __asm__ volatile ("jmp root_alpha\n\t" ::: "memory");
    __builtin_unreachable();
}

/* Reset all capture lengths to UINT64_MAX before each scan attempt */
static void reset_captures(void) {
    if (&cap_order == NULL || &cap_order_count == NULL) return;
    for (uint64_t i = 0; i < cap_order_count; i++)
        *cap_order[i].len = UINT64_MAX;
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
        reset_captures();
        if (setjmp(scan_env) == JMP_FAIL) continue;
        run_pattern();
        __builtin_unreachable();
    }
    exit(1);
}
