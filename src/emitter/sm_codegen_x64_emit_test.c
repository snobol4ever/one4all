#include "SM.h"
#include "emit_sm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_to(const char *path, SM_sequence_t *p)
{
    FILE *out = fopen(path, "w");
    if (!out) { perror(path); return 1; }
    int rc = sm_codegen_text(p, out, NULL);
    fclose(out);
    return rc != 0 ? 1 : 0;
}
#define AUDIT_COL1_END 24
#define AUDIT_COL2_END 40
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int audit_first_unquoted(const char *s, int len, char ch)
{
    int in_str = 0;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        if (c == '"') in_str = !in_str;
        else if (c == ch && !in_str) return i;
    }
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int audit_is_banner(const char *s, int len)
{
    int i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    return (i < len && s[i] == '#') ? 1 : 0;
}
typedef struct {
    long blank;
    long semicolon;
    long tab;
    long col1_shape;
    long off_grid;
    long col2_overflow;
} AuditCounters;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int audit_col1_check(const char *s, int len,
                            int *out_trimmed_len,
                            int *out_starts_with_dot)
{
    int n = (len < AUDIT_COL1_END) ? len : AUDIT_COL1_END;
    while (n > 0 && s[n-1] == ' ') n--;
    *out_trimmed_len = n;
    *out_starts_with_dot = (n > 0 && s[0] == '.') ? 1 : 0;
    if (n == 0) return 1;
    if (s[0] == ' ' || s[0] == '\t') return 0;
    for (int i = 0; i < n; i++) {
        if (s[i] == ' ' || s[i] == '\t') return 0;
    }
    return (s[n-1] == ':') ? 1 : 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int audit_line(const char *path, long lineno,
                      const char *raw, int len,
                      AuditCounters *cnt, int *printed_so_far)
{
    int violations = 0;
    int all_ws = 1;
    for (int i = 0; i < len; i++) {
        if (raw[i] != ' ' && raw[i] != '\t') { all_ws = 0; break; }
    }
    if (len == 0 || all_ws) {
        cnt->blank++;
        if ((*printed_so_far)++ < 10)
            fprintf(stderr, "  %s:%ld [I0/blank]: blank line\n", path, lineno);
        return 1;
    }
    if (audit_is_banner(raw, len)) return 0;
    int hash_pos = audit_first_unquoted(raw, len, '#');
    int code_len = (hash_pos < 0) ? len : hash_pos;
    int semi_pos = audit_first_unquoted(raw, code_len, ';');
    if (semi_pos >= 0 && semi_pos < 40) {
        cnt->semicolon++;
        violations++;
        if ((*printed_so_far)++ < 10)
            fprintf(stderr, "  %s:%ld [I1/semicolon]: bare ';' at col %d\n",
                    path, lineno, semi_pos);
    }
    {
        int in_str = 0;
        for (int i = 0; i < len; i++) {
            char c = raw[i];
            if (c == '"') in_str = !in_str;
            else if (c == '\t' && !in_str) {
                cnt->tab++;
                violations++;
                if ((*printed_so_far)++ < 10)
                    fprintf(stderr, "  %s:%ld [I2/tab]: TAB at col %d\n",
                            path, lineno, i);
                break;
            }
        }
    }
    int c1_trim_len, c1_starts_dot;
    if (!audit_col1_check(raw, len, &c1_trim_len, &c1_starts_dot)) {
        if (c1_starts_dot || (len > 0 && (raw[0] == ' ' || raw[0] == '\t'))) {
            cnt->off_grid++;
            violations++;
            if ((*printed_so_far)++ < 10)
                fprintf(stderr, "  %s:%ld [I3a/off-grid-directive]: directive not on col-1 grid\n",
                        path, lineno);
        } else {
            cnt->col1_shape++;
            violations++;
            if ((*printed_so_far)++ < 10)
                fprintf(stderr, "  %s:%ld [I3/col1-shape]: col-1 not 'label:'\n",
                        path, lineno);
        }
        return violations;
    }
    if (len < AUDIT_COL1_END) return violations;
    int p = AUDIT_COL1_END;
    while (p < len && raw[p] == ' ') p++;
    if (p >= len) return violations;
    int tok_start = p;
    while (p < len && raw[p] != ' ' && raw[p] != '\t') p++;
    int tok_end = p;
    int tok_len = tok_end - tok_start;
    (void)tok_len;
    (void)tok_end;
    return violations;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static long audit_file(const char *path, AuditCounters *cnt)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "audit: cannot open '%s'\n", path);
        return -1;
    }
    char buf[8192];
    long lineno = 0, total = 0;
    int printed = 0;
    while (fgets(buf, sizeof(buf), f)) {
        lineno++;
        int len = (int)strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            buf[--len] = '\0';
        }
        total += audit_line(path, lineno, buf, len, cnt, &printed);
    }
    fclose(f);
    if (printed > 10)
        fprintf(stderr, "  ... (+%d more in %s)\n", printed - 10, path);
    return total;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int run_audit(int n_paths, char **paths)
{
    AuditCounters cnt = {0};
    long grand_total = 0;
    for (int i = 0; i < n_paths; i++) {
        long t = audit_file(paths[i], &cnt);
        if (t < 0) return 2;
        if (t == 0) printf("OK   %s\n", paths[i]);
        else        printf("FAIL %s (%ld violations)\n", paths[i], t);
        grand_total += t;
    }
    printf("\n--- audit summary ---\n");
    printf("  I0/blank            : %ld\n", cnt.blank);
    printf("  I1/semicolon        : %ld\n", cnt.semicolon);
    printf("  I2/tab              : %ld\n", cnt.tab);
    printf("  I3/col1-shape       : %ld\n", cnt.col1_shape);
    printf("  I3a/off-grid-directive : %ld\n", cnt.off_grid);
    printf("  TOTAL               : %ld violations across %d files\n",
           grand_total, n_paths);
    return grand_total == 0 ? 0 : 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "--audit") == 0) {
        return run_audit(argc - 2, argv + 2);
    }
    if (argc < 2 || argc > 7) {
        fprintf(stderr,
            "usage: %s <em2.s> [<em3.s>] [<em4a.s>] [<em4b.s>]"
            " [<em5.s>] [<em5b.s>]\n"
            "       %s --audit <file.s> [<file.s> ...]\n",
            argv[0], argv[0]);
        return 2;
    }
    {
        SM_sequence_t *p = SM_seq_new();
        if (!p) { fprintf(stderr, "SM_seq_new failed\n"); return 1; }
        SM_emit_i(p, SM_PUSH_LIT_I, 42);
        SM_emit(p, SM_HALT);
        int rc = emit_to(argv[1], p);
        SM_seq_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-2 program\n");
            return 1;
        }
    }
    if (argc < 3) return 0;
    {
        SM_sequence_t *p = SM_seq_new();
        if (!p) { fprintf(stderr, "SM_seq_new failed\n"); return 1; }
        SM_emit_i(p, SM_PUSH_LIT_I, 2);
        SM_emit_i(p, SM_PUSH_LIT_I, 3);
        SM_emit(p,  SM_ADD);
        SM_emit_i(p, SM_PUSH_LIT_I, 4);
        SM_emit(p,  SM_MUL);
        SM_emit(p,  SM_HALT);
        int rc = emit_to(argv[2], p);
        SM_seq_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-3 program\n");
            return 1;
        }
    }
    if (argc < 4) return 0;
    {
        SM_sequence_t *p = SM_seq_new();
        if (!p) { fprintf(stderr, "SM_seq_new failed\n"); return 1; }
        SM_emit_i(p, SM_PUSH_LIT_I, 100);
        int j_skip = SM_emit_i(p, SM_JUMP, 0);
        SM_emit_i(p, SM_PUSH_LIT_I, 99);
        SM_emit(p,   SM_HALT);
        int L_skip = SM_label(p);
        SM_emit_i(p, SM_PUSH_LIT_I, -58);
        SM_emit(p,   SM_ADD);
        int j_dead = SM_emit_i(p, SM_JUMP_F, 0);
        int j_final= SM_emit_i(p, SM_JUMP_S, 0);
        SM_emit_i(p, SM_PUSH_LIT_I, 77);
        SM_emit(p,   SM_HALT);
        int L_dead = SM_label(p);
        SM_emit(p,   SM_HALT);
        int L_final= SM_label(p);
        SM_emit(p,   SM_HALT);
        SM_patch_jump(p, j_skip,  L_skip);
        SM_patch_jump(p, j_dead,  L_dead);
        SM_patch_jump(p, j_final, L_final);
        int rc = emit_to(argv[3], p);
        SM_seq_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-4a program\n");
            return 1;
        }
    }
    if (argc < 5) return 0;
    {
        SM_sequence_t *p = SM_seq_new();
        if (!p) { fprintf(stderr, "SM_seq_new failed\n"); return 1; }
        SM_emit_i(p, SM_PUSH_LIT_I, 3);
        int L_top = SM_label(p);
        SM_emit_i(p, SM_PUSH_LIT_I, 1);
        SM_emit(p,   SM_SUB);
        int j_back = SM_emit_i(p, SM_JUMP_F, 0);
        SM_emit(p,   SM_HALT);
        SM_patch_jump(p, j_back, L_top);
        int rc = emit_to(argv[4], p);
        SM_seq_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-4b program\n");
            return 1;
        }
    }
    if (argc < 6) return 0;
    {
        SM_sequence_t *p = SM_seq_new();
        if (!p) { fprintf(stderr, "SM_seq_new failed\n"); return 1; }
        SM_emit_i(p, SM_PUSH_LIT_I, 0);
        SM_emit(p,   SM_VOID_POP);
        int j_main = SM_emit_i(p, SM_JUMP, 0);
        int L_b    = SM_label(p);
        SM_emit_i(p, SM_PUSH_LIT_I, 7);
        SM_emit(p,   SM_RETURN);
        int L_a    = SM_label(p);
        SM_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)L_b, 0);
        SM_emit_i(p, SM_PUSH_LIT_I, 6);
        SM_emit(p,   SM_ADD);
        SM_emit(p,   SM_RETURN);
        int L_main = SM_label(p);
        SM_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)L_a, 0);
        SM_emit(p,   SM_HALT);
        SM_patch_jump(p, j_main, L_main);
        int rc = emit_to(argv[5], p);
        SM_seq_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-5 program\n");
            return 1;
        }
    }
    if (argc < 7) return 0;
    {
        SM_sequence_t *p = SM_seq_new();
        if (!p) { fprintf(stderr, "SM_seq_new failed\n"); return 1; }
        SM_emit_ii(p, SM_PUSH_EXPRESSION, 99, 2);
        SM_emit(p,   SM_VOID_POP);
        SM_emit_i(p, SM_PUSH_LIT_I, 21);
        SM_emit(p,   SM_HALT);
        int rc = emit_to(argv[6], p);
        SM_seq_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-5b program\n");
            return 1;
        }
    }
    return 0;
}
