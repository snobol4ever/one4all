/* icon_runtime.c — Tiny-ICON runtime — pure syscalls, no libc */
static void write_bytes(const char *buf, long len) {
    __asm__ volatile (
        "syscall"
        : : "a"(1L), "D"(1L), "S"(buf), "d"(len)
        : "rcx", "r11", "memory"
    );
}

static void write_long(long v) {
    char buf[32]; int i = 0;
    if (v < 0) { write_bytes("-", 1); v = -v; }
    if (v == 0) { write_bytes("0\n", 2); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    for (int a = 0, b = i-1; a < b; a++, b--) { char t=buf[a]; buf[a]=buf[b]; buf[b]=t; }
    buf[i++] = '\n';
    write_bytes(buf, i);
}

static long my_strlen(const char *s) { long n=0; while(s[n]) n++; return n; }

void icn_write_int(long v) { write_long(v); }
void icn_write_str(const char *s) { long l=my_strlen(s); write_bytes(s,l); write_bytes("\n",1); }

#define ICN_STACK_MAX 256
static long icn_stack[ICN_STACK_MAX];
static int  icn_sp = 0;

long icn_retval = 0;
int  icn_failed = 0;

void icn_push(long v)  { if (icn_sp < ICN_STACK_MAX) icn_stack[icn_sp++] = v; }
long icn_pop(void)     { return icn_sp > 0 ? icn_stack[--icn_sp] : 0; }
