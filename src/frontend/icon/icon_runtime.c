static char icn_str_arena[65536];
static int  str_arena_pos = 0;
#define ICN_STACK_MAX 256
static long icn_stack[ICN_STACK_MAX];
static int  icn_sp = 0;
long icn_retval = 0;
int  icn_failed = 0;
static char subscript_buf[2];
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_cset_complement(const char *cs) {
    if (!cs) cs = "";
    if (str_arena_pos + 128 > 65536) str_arena_pos = 0;
    char *out = icn_str_arena + str_arena_pos;
    int n = 0;
    for (int c = 1; c < 128; c++) {
        int found = 0;
        for (int i = 0; cs[i]; i++) { if ((unsigned char)cs[i] == (unsigned)c) { found = 1; break; } }
        if (!found) out[n++] = (char)c;
    }
    out[n] = '\0';
    str_arena_pos += n + 1;
    return out;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_cset_union(const char *a, const char *b) {
    if (!a) a = ""; if (!b) b = "";
    if (str_arena_pos + 256 > 65536) str_arena_pos = 0;
    char *out = icn_str_arena + str_arena_pos;
    int n = 0;
    for (int i = 0; a[i]; i++) out[n++] = a[i];
    for (int j = 0; b[j]; j++) {
        int found = 0;
        for (int i = 0; a[i]; i++) { if (a[i] == b[j]) { found = 1; break; } }
        if (!found) out[n++] = b[j];
    }
    out[n] = '\0';
    str_arena_pos += n + 1;
    return out;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_cset_diff(const char *a, const char *b) {
    if (!a) a = ""; if (!b) b = "";
    if (str_arena_pos + 256 > 65536) str_arena_pos = 0;
    char *out = icn_str_arena + str_arena_pos;
    int n = 0;
    for (int i = 0; a[i]; i++) {
        int found = 0;
        for (int j = 0; b[j]; j++) { if (a[i] == b[j]) { found = 1; break; } }
        if (!found) out[n++] = a[i];
    }
    out[n] = '\0';
    str_arena_pos += n + 1;
    return out;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_cset_inter(const char *a, const char *b) {
    if (!a) a = ""; if (!b) b = "";
    if (str_arena_pos + 256 > 65536) str_arena_pos = 0;
    char *out = icn_str_arena + str_arena_pos;
    int n = 0;
    for (int i = 0; a[i]; i++) {
        for (int j = 0; b[j]; j++) { if (a[i] == b[j]) { out[n++] = a[i]; break; } }
    }
    out[n] = '\0';
    str_arena_pos += n + 1;
    return out;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_cset_canonical(const char *cs) {
    if (!cs || !*cs) return "";
    unsigned char present[256] = {0};
    for (const unsigned char *p = (const unsigned char *)cs; *p; p++) present[*p] = 1;
    int n = 0;
    for (int c = 0; c < 256; c++) if (present[c]) n++;
    if (str_arena_pos + n + 1 > 65536) str_arena_pos = 0;
    char *out = icn_str_arena + str_arena_pos;
    int bi = 0;
    for (int c = 0; c < 256; c++) if (present[c]) out[bi++] = (char)c;
    out[bi] = '\0';
    str_arena_pos += bi + 1;
    return out;
}
