static void write_bytes(const char *buf, long len) {
    __asm__ volatile (
        "syscall"
        : : "a"(1L), "D"(1L), "S"(buf), "d"(len)
        : "rcx", "r11", "memory"
    );
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void write_long(long v) {
    char buf[32]; int i = 0;
    if (v < 0) { write_bytes("-", 1); v = -v; }
    if (v == 0) { write_bytes("0\n", 2); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    for (int a = 0, b = i-1; a < b; a++, b--) { char t=buf[a]; buf[a]=buf[b]; buf[b]=t; }
    buf[i++] = '\n';
    write_bytes(buf, i);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static long my_strlen(const char *s) { long n=0; while(s[n]) n++; return n; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_write_int(long v) { write_long(v); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_write_str(const char *s) { if(!s) { write_bytes("\n",1); return; } long l=my_strlen(s); write_bytes(s,l); write_bytes("\n",1); }
static char icn_str_arena[65536];
static int  str_arena_pos = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_str_concat(const char *a, const char *b) {
    long la = my_strlen(a), lb = my_strlen(b);
    if (str_arena_pos + la + lb + 1 > 65536) {
        str_arena_pos = 0;
    }
    char *out = icn_str_arena + str_arena_pos;
    for (long i = 0; i < la; i++) out[i]      = a[i];
    for (long i = 0; i < lb; i++) out[la + i] = b[i];
    out[la + lb] = '\0';
    str_arena_pos += (int)(la + lb + 1);
    return out;
}
#define ICN_STACK_MAX 256
static long icn_stack[ICN_STACK_MAX];
static int  icn_sp = 0;
const char *scan_subject = (void*)0;
long        icn_pos     = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_any(const char *cset) {
    if (!scan_subject) return 0;
    long len = my_strlen(scan_subject);
    if (icn_pos >= len) return 0;
    char c = scan_subject[icn_pos];
    for (long i = 0; cset[i]; i++) {
        if (cset[i] == c) {
            icn_pos++;
            return icn_pos + 1;
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_many(const char *cset) {
    if (!scan_subject) return 0;
    long len = my_strlen(scan_subject);
    long start = icn_pos;
    while (icn_pos < len) {
        char c = scan_subject[icn_pos];
        int found = 0;
        for (long i = 0; cset[i]; i++) { if (cset[i] == c) { found = 1; break; } }
        if (!found) break;
        icn_pos++;
    }
    if (icn_pos == start) return 0;
    return icn_pos + 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_upto(const char *cset) {
    if (!scan_subject) return 0;
    long len = my_strlen(scan_subject);
    while (icn_pos < len) {
        char c = scan_subject[icn_pos];
        for (long i = 0; cset[i]; i++) {
            if (cset[i] == c) return icn_pos + 1;
        }
        icn_pos++;
    }
    return 0;
}
long icn_retval = 0;
int  icn_failed = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_push(long v)  { if (icn_sp < ICN_STACK_MAX) icn_stack[icn_sp++] = v; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_pop(void)     { return icn_sp > 0 ? icn_stack[--icn_sp] : 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_str_find(const char *s1, const char *s2, long from) {
    if (!s1 || !s2) return 0;
    long l1 = my_strlen(s1), l2 = my_strlen(s2);
    for (long i = from; i <= l2 - l1; i++) {
        long j;
        for (j = 0; j < l1; j++) if (s2[i+j] != s1[j]) break;
        if (j == l1) return i + 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_match(const char *s) {
    if (!s || !scan_subject) return 0;
    long len = my_strlen(s);
    long subj_len = my_strlen(scan_subject);
    if (icn_pos + len > subj_len) return 0;
    for (long i = 0; i < len; i++)
        if (scan_subject[icn_pos + i] != s[i]) return 0;
    icn_pos += len;
    return icn_pos + 1;
}
static char tabmove_buf[4096];
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_tab(long n) {
    if (!scan_subject) return 0;
    long subj_len = my_strlen(scan_subject);
    long new_pos = n - 1;
    if (new_pos < icn_pos || new_pos > subj_len) return 0;
    long len = new_pos - icn_pos;
    if (len >= (long)sizeof(tabmove_buf)) return 0;
    for (long i = 0; i < len; i++) tabmove_buf[i] = scan_subject[icn_pos + i];
    tabmove_buf[len] = '\0';
    icn_pos = new_pos;
    return tabmove_buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_move(long n) {
    if (!scan_subject) return 0;
    long subj_len = my_strlen(scan_subject);
    if (n < 0 || icn_pos + n > subj_len) return 0;
    if (n >= (long)sizeof(tabmove_buf)) return 0;
    for (long i = 0; i < n; i++) tabmove_buf[i] = scan_subject[icn_pos + i];
    tabmove_buf[n] = '\0';
    icn_pos += n;
    return tabmove_buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int icn_str_cmp(const char *a, const char *b) {
    while (*a && *b) {
        if ((unsigned char)*a < (unsigned char)*b) return -1;
        if ((unsigned char)*a > (unsigned char)*b) return  1;
        a++; b++;
    }
    if (*b) return -1;
    if (*a) return  1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_strlen(const char *s) { return my_strlen(s); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_pow(long base, long exp) {
    if (exp < 0) return 0;
    long result = 1;
    while (exp-- > 0) result *= base;
    return result;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_str_size(const char *s) { return s ? my_strlen(s) : 0; }
static char subscript_buf[2];
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_str_subscript(const char *s, long i) {
    if (!s) return "";
    long len = my_strlen(s);
    if (i < 0 || i >= len) return "";
    subscript_buf[0] = s[i];
    subscript_buf[1] = '\0';
    return subscript_buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_str_section(const char *s, long i, long j, long kind) {
    if (!s) return "";
    long len = my_strlen(s);
    long lo, hi;
    if (kind == 0) {
        lo = i - 1; hi = j - 1;
    } else if (kind == 1) {
        lo = i - 1; hi = lo + j;
    } else {
        hi = i - 1; lo = hi - j;
    }
    if (lo < 0) lo = 0;
    if (hi > len) hi = len;
    if (lo > hi) lo = hi;
    long slen = hi - lo;
    if (str_arena_pos + slen + 1 > 65536) str_arena_pos = 0;
    char *out = icn_str_arena + str_arena_pos;
    for (long k = 0; k < slen; k++) out[k] = s[lo + k];
    out[slen] = '\0';
    str_arena_pos += (int)(slen + 1);
    return out;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_bang_char_at(const char *s, long pos) {
    if (!s) return (void*)0;
    long len = my_strlen(s);
    if (pos < 0 || pos >= len) return (void*)0;
    subscript_buf[0] = s[pos];
    subscript_buf[1] = '\0';
    return subscript_buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
long icn_match_pat(const char *pat) {
    if (!pat || !scan_subject) return -1;
    long plen = my_strlen(pat);
    long slen = my_strlen(scan_subject);
    if (icn_pos + plen > slen) return -1;
    for (long i = 0; i < plen; i++)
        if (scan_subject[icn_pos + i] != pat[i]) return -1;
    icn_pos += plen;
    return icn_pos;
}
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
long icn_random(long n) {
    static unsigned long seed = 12345;
    seed = seed * 6364136223846793005UL + 1442695040888963407UL;
    if (n <= 0) return 0;
    return (long)((seed >> 33) % (unsigned long)n) + 1;
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
