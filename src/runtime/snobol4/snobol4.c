#include "snobol4.h"
#include "sil_macros.h"
#include "snobol4_utf8.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#define MON_RS "\x1e"
#define MON_US "\x1f"
#include <sys/uio.h>
#include "../../../scripts/monitor/monitor_wire.h"
int monitor_fd  = -1;
int monitor_ack_fd = -1;
int monitor_ready = 0;
int monitor_quiet_depth = 0;
static int   monitor_bin_mode = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void  mon_at_exit(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static uint32_t intern_name_bin(const char *p, int len);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void  mon_send_bin(uint32_t kind, uint32_t name_id, uint8_t type,
                          const void *value, uint32_t value_len);
#define TRACE_SET_CAP 256
static const char *trace_set[TRACE_SET_CAP];
static const char *trace_callback[TRACE_SET_CAP];
static int trace_recursion_depth = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int trace_slot_lookup(const char *name) {
    if (!name || !*name) return -1;
    unsigned h = 5381;
    for (const char *p = name; *p; p++) h = h * 33 ^ (unsigned char)*p;
    for (int i = 0; i < TRACE_SET_CAP; i++) {
        int slot = (h + i) & (TRACE_SET_CAP - 1);
        if (!trace_set[slot]) return -1;
        if (strcmp(trace_set[slot], name) == 0) return slot;
    }
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void trace_register(const char *name) {
    if (!name || !*name) return;
    unsigned h = 5381;
    for (const char *p = name; *p; p++) h = h * 33 ^ (unsigned char)*p;
    for (int i = 0; i < TRACE_SET_CAP; i++) {
        int slot = (h + i) & (TRACE_SET_CAP - 1);
        if (!trace_set[slot]) {
            trace_set[slot] = GC_strdup(name);
            trace_callback[slot] = NULL;
            return;
        }
        if (strcmp(trace_set[slot], name) == 0) return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void trace_register_callback(const char *name, const char *cbfn) {
    if (!name || !*name) return;
    unsigned h = 5381;
    for (const char *p = name; *p; p++) h = h * 33 ^ (unsigned char)*p;
    for (int i = 0; i < TRACE_SET_CAP; i++) {
        int slot = (h + i) & (TRACE_SET_CAP - 1);
        if (!trace_set[slot]) {
            trace_set[slot] = GC_strdup(name);
            trace_callback[slot] = (cbfn && *cbfn) ? GC_strdup(cbfn) : NULL;
            return;
        }
        if (strcmp(trace_set[slot], name) == 0) {
            trace_callback[slot] = (cbfn && *cbfn) ? GC_strdup(cbfn) : NULL;
            return;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void trace_unregister(const char *name) {
    if (!name || !*name) return;
    unsigned h = 5381;
    for (const char *p = name; *p; p++) h = h * 33 ^ (unsigned char)*p;
    for (int i = 0; i < TRACE_SET_CAP; i++) {
        int slot = (h + i) & (TRACE_SET_CAP - 1);
        if (!trace_set[slot]) return;
        if (strcmp(trace_set[slot], name) == 0) {
            trace_set[slot] = NULL;
            trace_callback[slot] = NULL;
            return;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int trace_registered(const char *name) {
    return trace_slot_lookup(name) >= 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *trace_get_callback(const char *name) {
    int slot = trace_slot_lookup(name);
    if (slot < 0) return NULL;
    return trace_callback[slot];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int trace_is_active(const char *name) { return trace_registered(name); }
int64_t kw_stcount = 0;
int64_t kw_stno    = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void mon_send(const char *kind, const char *name, const char *value) {
    if (monitor_fd < 0) return;
    if (!value) value = "";
    struct iovec iov[6];
    iov[0].iov_base = (void*)kind;   iov[0].iov_len = strlen(kind);
    iov[1].iov_base = MON_RS;        iov[1].iov_len = 1;
    iov[2].iov_base = (void*)name;   iov[2].iov_len = strlen(name);
    iov[3].iov_base = MON_US;        iov[3].iov_len = 1;
    iov[4].iov_base = (void*)value;  iov[4].iov_len = strlen(value);
    iov[5].iov_base = MON_RS;        iov[5].iov_len = 1;
    writev(monitor_fd, iov, 6);
    if (monitor_ack_fd >= 0) {
        char ack[1];
        ssize_t r = read(monitor_ack_fd, ack, 1);
        if (r != 1 || ack[0] == 'S') exit(0);
    }
}
static char  **g_bin_names      = NULL;
static int    *g_bin_name_lens  = NULL;
static int     g_bin_n_names    = 0;
static int     g_bin_names_cap  = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int load_names_file_bin(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int   cap = 64;
    char **names = (char **)malloc(cap * sizeof(char *));
    int   *lens  = (int  *)malloc(cap * sizeof(int));
    if (!names || !lens) { fclose(f); free(names); free(lens); return -1; }
    int n = 0;
    char *line = NULL; size_t lcap = 0;
    ssize_t got;
    while ((got = getline(&line, &lcap, f)) >= 0) {
        if (got > 0 && line[got-1] == '\n') { line[got-1] = '\0'; got--; }
        if (got > 0 && line[got-1] == '\r') { line[got-1] = '\0'; got--; }
        if (n == cap) {
            cap *= 2;
            names = (char **)realloc(names, cap * sizeof(char *));
            lens  = (int  *)realloc(lens,  cap * sizeof(int));
            if (!names || !lens) { fclose(f); free(line); return -1; }
        }
        char *copy = (char *)malloc((size_t)got + 1);
        if (!copy) { fclose(f); free(line); return -1; }
        memcpy(copy, line, (size_t)got + 1);
        names[n] = copy;
        lens[n]  = (int)got;
        n++;
    }
    free(line);
    fclose(f);
    g_bin_names     = names;
    g_bin_name_lens = lens;
    g_bin_n_names   = n;
    g_bin_names_cap = cap;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static uint32_t lookup_name_id_bin(const char *p, int len) {
    if (!g_bin_names || !p) return MW_NAME_ID_NONE;
    for (int i = 0; i < g_bin_n_names; i++) {
        if (g_bin_name_lens[i] == len && memcmp(g_bin_names[i], p, (size_t)len) == 0)
            return (uint32_t)i;
    }
    return MW_NAME_ID_NONE;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static uint32_t intern_name_bin(const char *p, int len) {
    if (!p || len < 0) return MW_NAME_ID_NONE;
    for (int i = 0; i < g_bin_n_names; i++) {
        if (g_bin_name_lens[i] == len && memcmp(g_bin_names[i], p, (size_t)len) == 0)
            return (uint32_t)i;
    }
    if (g_bin_n_names == g_bin_names_cap) {
        int new_cap = g_bin_names_cap ? g_bin_names_cap * 2 : 64;
        char **nn = (char **)realloc(g_bin_names, (size_t)new_cap * sizeof(char *));
        int   *nl = (int  *)realloc(g_bin_name_lens, (size_t)new_cap * sizeof(int));
        if (!nn || !nl) {
            if (nn) g_bin_names = nn;
            if (nl) g_bin_name_lens = nl;
            return MW_NAME_ID_NONE;
        }
        g_bin_names     = nn;
        g_bin_name_lens = nl;
        g_bin_names_cap = new_cap;
    }
    char *copy = (char *)malloc((size_t)len + 1);
    if (!copy) return MW_NAME_ID_NONE;
    if (len > 0) memcpy(copy, p, (size_t)len);
    copy[len] = '\0';
    int id = g_bin_n_names;
    g_bin_names[id]     = copy;
    g_bin_name_lens[id] = len;
    g_bin_n_names       = id + 1;
    if (monitor_bin_mode && monitor_fd >= 0) {
        mon_send_bin(MWK_NAME_DEF, (uint32_t)id, MWT_STRING,
                     (len > 0) ? (const void *)copy : NULL, (uint32_t)len);
    }
    return (uint32_t)id;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void mon_at_exit(void) {
    static int already = 0;
    if (already) return;
    already = 1;
    if (monitor_fd >= 0 && monitor_bin_mode) {
        unsigned char hdr[MW_HDR_BYTES];
        mw_pack_hdr(hdr, MWK_END, MW_NAME_ID_NONE, MWT_NULL, 0);
        ssize_t w = write(monitor_fd, hdr, MW_HDR_BYTES);
        (void)w;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static uint8_t scrip_tag_to_wire(int v) {
    switch (v) {
        case DT_SNUL:  return MWT_STRING;
        case DT_S:     return MWT_STRING;
        case DT_I:     return MWT_INTEGER;
        case DT_R:     return MWT_REAL;
        case DT_P:     return MWT_PATTERN;
        case DT_A:     return MWT_ARRAY;
        case DT_T:     return MWT_TABLE;
        case DT_C:     return MWT_CODE;
        case DT_E:     return MWT_EXPRESSION;
        case DT_DATA:  return MWT_DATA;
        default:       return MWT_UNKNOWN;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void mon_send_bin(uint32_t kind, uint32_t name_id, uint8_t type,
                         const void *value, uint32_t value_len) {
    if (monitor_fd < 0) return;
    unsigned char hdr[MW_HDR_BYTES];
    mw_pack_hdr(hdr, kind, name_id, type, value_len);
    struct iovec iov[2];
    int niov = 1;
    iov[0].iov_base = hdr;
    iov[0].iov_len  = MW_HDR_BYTES;
    if (value_len > 0 && value) {
        iov[1].iov_base = (void *)value;
        iov[1].iov_len  = (size_t)value_len;
        niov = 2;
    }
    ssize_t total = (ssize_t)MW_HDR_BYTES + (ssize_t)value_len;
    ssize_t got   = writev(monitor_fd, iov, niov);
    if (got != total) return;
    if (monitor_ack_fd >= 0) {
        char ack[1];
        ssize_t r = read(monitor_ack_fd, ack, 1);
        if (r != 1 || ack[0] == 'S') exit(0);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void mon_emit_label_bin(int64_t stno) {
    if (monitor_fd < 0 || !monitor_bin_mode) return;
    unsigned char buf[8];
    for (int k = 0; k < 8; k++) buf[k] = (unsigned char)(((uint64_t)stno >> (k*8)) & 0xff);
    mon_send_bin(MWK_LABEL, MW_NAME_ID_NONE, MWT_INTEGER, buf, 8);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _arg_str(DESCR_t a, const char **out_p, int *out_len) {
    if (a.v == DT_S && a.s) {
        *out_p   = a.s;
        *out_len = a.slen ? (int)a.slen : (int)strlen(a.s);
    } else {
        *out_p = NULL; *out_len = 0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _mon_put_helper(DESCR_t *args, int nargs, uint32_t kind, int opaque) {
    if (nargs < 2) return FAILDESCR;
    const char *np; int nlen;
    _arg_str(args[0], &np, &nlen);
    if (!np) return FAILDESCR;
    uint32_t name_id = lookup_name_id_bin(np, nlen);
    if (name_id == MW_NAME_ID_NONE) return FAILDESCR;
    uint8_t type;
    const void *vp = NULL;
    uint32_t    vlen = 0;
    int64_t i_buf;
    double  r_buf;
    if (opaque) {
        const char *tp; int tlen;
        _arg_str(args[1], &tp, &tlen);
        type = MWT_DATA;
        if (tp) {
            if      (tlen == 7  && memcmp(tp, "PATTERN", 7) == 0) type = MWT_PATTERN;
            else if (tlen == 5  && memcmp(tp, "ARRAY",   5) == 0) type = MWT_ARRAY;
            else if (tlen == 5  && memcmp(tp, "TABLE",   5) == 0) type = MWT_TABLE;
            else if (tlen == 4  && memcmp(tp, "CODE",    4) == 0) type = MWT_CODE;
            else if (tlen == 4  && memcmp(tp, "FILE",    4) == 0) type = MWT_FILE;
            else if (tlen == 10 && memcmp(tp, "EXPRESSION", 10) == 0) type = MWT_EXPRESSION;
            else if (tlen == 4  && memcmp(tp, "NAME",    4) == 0) type = MWT_NAME;
            else if (tlen == 0)                                    type = MWT_NULL;
        }
    } else {
        type = scrip_tag_to_wire(args[1].v);
        switch (type) {
            case MWT_STRING:
            case MWT_NAME:
                if (args[1].s) {
                    vlen = args[1].slen ? (uint32_t)args[1].slen : (uint32_t)strlen(args[1].s);
                    vp   = (vlen > 0) ? (const void *)args[1].s : NULL;
                }
                break;
            case MWT_INTEGER: {
                int64_t iv = args[1].i;
                unsigned char *p = (unsigned char *)&i_buf;
                for (int k = 0; k < 8; k++) p[k] = (unsigned char)((iv >> (k*8)) & 0xff);
                vp = &i_buf; vlen = 8;
                break;
            }
            case MWT_REAL: {
                double rv = args[1].r;
                memcpy(&r_buf, &rv, sizeof(r_buf));
                vp = &r_buf; vlen = 8;
                break;
            }
            default: break;
        }
    }
    mon_send_bin(kind, name_id, type, vp, vlen);
    return (DESCR_t){ .v = DT_I, .i = 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_OPEN(DESCR_t *args, int nargs) {
    if (nargs < 3) return FAILDESCR;
    if (monitor_fd < 0) return FAILDESCR;
    if (g_bin_names == NULL) {
        const char *np; int nl;
        _arg_str(args[2], &np, &nl);
        if (np && nl > 0) {
            char buf[4096];
            int  cp = nl < (int)sizeof(buf)-1 ? nl : (int)sizeof(buf)-1;
            memcpy(buf, np, cp); buf[cp] = '\0';
            if (load_names_file_bin(buf) < 0) return FAILDESCR;
        }
    }
    return (DESCR_t){ .v = DT_I, .i = 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_S_VALUE (DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_VALUE,  0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_I_VALUE (DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_VALUE,  0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_R_VALUE (DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_VALUE,  0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_O_VALUE (DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_VALUE,  1); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_S_RETURN(DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_RETURN, 0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_I_RETURN(DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_RETURN, 0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_R_RETURN(DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_RETURN, 0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_O_RETURN(DESCR_t *a, int n) { return _mon_put_helper(a, n, MWK_RETURN, 1); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_PUT_CALL(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    const char *np; int nlen;
    _arg_str(args[0], &np, &nlen);
    if (!np) return FAILDESCR;
    uint32_t name_id = lookup_name_id_bin(np, nlen);
    if (name_id == MW_NAME_ID_NONE) return FAILDESCR;
    mon_send_bin(MWK_CALL, name_id, MWT_NULL, NULL, 0);
    return (DESCR_t){ .v = DT_I, .i = 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_MON_CLOSE(DESCR_t *args, int nargs) {
    (void)args; (void)nargs;
    if (monitor_fd >= 0) {
        mon_send_bin(MWK_END, MW_NAME_ID_NONE, MWT_NULL, NULL, 0);
    }
    return (DESCR_t){ .v = DT_I, .i = 0 };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_LOAD_stub(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    const char *p; int len;
    _arg_str(args[0], &p, &len);
    if (!p || len < 4) return FAILDESCR;
    if (memcmp(p, "MON_", 4) == 0) {
        return (DESCR_t){ .v = DT_I, .i = 0 };
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void comm_stno(int n) {
    ++kw_stcount;
    g_sno_err_stmt = n;
    if (kw_stlimit >= 0 && kw_stcount > kw_stlimit)
        sno_runtime_error(22, NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void comm_var(const char *name, DESCR_t val) {
    if (!name || name[0] == '_') return;
    if (monitor_quiet_depth > 0) return;
    const char *cbfn = trace_get_callback(name);
    if (getenv("SCRIP_DEBUG_TRACE"))
        fprintf(stderr, "[scrip-trace] comm_var name=%s cb=%s recur=%d\n",
                name, cbfn ? cbfn : "(none)", trace_recursion_depth);
    if (cbfn && trace_recursion_depth == 0) {
        trace_recursion_depth++;
        DESCR_t cbargs[2];
        cbargs[0] = STRVAL(GC_strdup(name));
        cbargs[1] = STRVAL("");
        (void)APPLY_fn(cbfn, cbargs, 2);
        trace_recursion_depth--;
        return;
    }
    if (monitor_fd < 0) return;
    if (!monitor_ready) return;
    if (kw_trace <= 0 && !trace_registered(name)) return;
    if (monitor_bin_mode) {
        uint32_t name_id = intern_name_bin(name, (int)strlen(name));
        if (name_id == MW_NAME_ID_NONE) return;
        uint8_t type = scrip_tag_to_wire(val.v);
        const void *vp = NULL;
        uint32_t    vlen = 0;
        int64_t i_buf;
        double  r_buf;
        switch (type) {
            case MWT_STRING:
            case MWT_NAME:
                if (val.s) {
                    vlen = val.slen ? (uint32_t)val.slen : (uint32_t)strlen(val.s);
                    vp   = (vlen > 0) ? (const void *)val.s : NULL;
                }
                break;
            case MWT_INTEGER: {
                int64_t iv = val.i;
                unsigned char *p = (unsigned char *)&i_buf;
                for (int k = 0; k < 8; k++) p[k] = (unsigned char)((iv >> (k*8)) & 0xff);
                vp = &i_buf; vlen = 8;
                break;
            }
            case MWT_REAL: {
                double rv = val.r;
                memcpy(&r_buf, &rv, sizeof(r_buf));
                vp = &r_buf; vlen = 8;
                break;
            }
            default: break;
        }
        mon_send_bin(MWK_VALUE, name_id, type, vp, vlen);
        return;
    }
    const char *s = VARVAL_fn(val);
    mon_send("VALUE", name, s ? s : "(undef)");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void comm_call(const char *fname) {
    if (!fname || !*fname) return;
    if (kw_ftrace > 0) {
        fprintf(stdout, "****%-7lld  %s()\n",
                (long long)kw_stcount, fname);
        fflush(stdout);
    }
    if (monitor_fd < 0) return;
    if (!monitor_ready) return;
    if (kw_ftrace <= 0 && !trace_registered(fname)) return;
    if (monitor_bin_mode) {
        uint32_t name_id = intern_name_bin(fname, (int)strlen(fname));
        if (name_id == MW_NAME_ID_NONE) return;
        mon_send_bin(MWK_CALL, name_id, MWT_NULL, NULL, 0);
        return;
    }
    mon_send("CALL", fname, "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void comm_return(const char *fname, DESCR_t retval) {
    if (!fname || !*fname) return;
    if (kw_ftrace > 0) {
        const char *s = VARVAL_fn(retval);
        fprintf(stdout, "****%-7lld  RETURN %s = '%s'\n",
                (long long)kw_stcount, fname, s ? s : "");
        fflush(stdout);
    }
    if (monitor_fd < 0) return;
    if (!monitor_ready) return;
    if (kw_ftrace <= 0 && !trace_registered(fname)) return;
    if (monitor_bin_mode) {
        uint32_t name_id = intern_name_bin(fname, (int)strlen(fname));
        if (name_id == MW_NAME_ID_NONE) return;
        const char *rt = kw_rtntype[0] ? kw_rtntype : "RETURN";
        uint32_t rtlen = (uint32_t)strlen(rt);
        mon_send_bin(MWK_RETURN, name_id, MWT_STRING,
                     rtlen ? (const void *)rt : NULL, rtlen);
        (void)retval;
        return;
    }
    const char *s = VARVAL_fn(retval);
    mon_send("RETURN", fname, s ? s : "(fail)");
}
int64_t kw_fullscan = 0;
int64_t kw_maxlngth = 524288;
int64_t kw_anchor   = 0;
int64_t kw_trim     = 1;
int64_t kw_stlimit  = -1;
int64_t kw_ftrace   = 0;
int64_t kw_trace    = 0;
int64_t kw_errlimit = 0;
int64_t kw_code     = 0;
int64_t kw_fnclevel = 0;
char    kw_rtntype[16] = "";
char ucase[27]    = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char lcase[27]    = "abcdefghijklmnopqrstuvwxyz";
char digits[11]   = "0123456789";
char alphabet[257];
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int is_numeric_like(DESCR_t d) {
    if (IS_INT(d) || IS_REAL(d) || IS_NULL(d)) return 1;
    if (IS_STR(d)) {
        const char *s = d.s ? d.s : "";
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) return 1;
        char *end;
        strtod(s, &end);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t') end++;
        return (*end == '\0');
    }
    if (IS_KW(d)) return is_numeric_like(NV_GET_fn(d.s));
    return 0;
}
#define NUM_GUARD(fn)                                                      \
    do {                                                                   \
        if (!is_numeric_like(a[0])) {                                      \
            sno_runtime_error(1, fn " first argument is not numeric");    \
            return FAILDESCR;                                              \
        }                                                                  \
        if (!is_numeric_like(a[1])) {                                      \
            sno_runtime_error(1, fn " second argument is not numeric");   \
            return FAILDESCR;                                              \
        }                                                                  \
    } while (0)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _GT_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    NUM_GUARD("GT");
    return gt(a[0], a[1]) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LT_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    NUM_GUARD("LT");
    return lt(a[0], a[1]) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _GE_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    NUM_GUARD("GE");
    return ge(a[0], a[1]) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LE_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    NUM_GUARD("LE");
    return le(a[0], a[1]) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _EQ_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    NUM_GUARD("EQ");
    if (IS_INT(a[0]) && IS_INT(a[1]))
        return (a[0].i == a[1].i) ? NULVCL : FAILDESCR;
    return (to_real(a[0]) == to_real(a[1])) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _NE_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    NUM_GUARD("NE");
    if (IS_INT(a[0]) && IS_INT(a[1]))
        return (a[0].i != a[1].i) ? NULVCL : FAILDESCR;
    return (to_real(a[0]) != to_real(a[1])) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_add(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    return add(a[0], a[1]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_sub(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    return sub(a[0], a[1]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_mul(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    return mul(a[0], a[1]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_div(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    return DIVIDE_fn(a[0], a[1]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_pow(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    return POWER_fn(a[0], a[1]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_neg(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    return neg(a[0]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_pos(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    return pos(a[0]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _INTEGER_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    if (IS_INT(a[0])) return a[0];
    if (IS_STR(a[0]) && a[0].s) {
        char *end;
        long long v = strtoll(a[0].s, &end, 10);
        if (end != a[0].s && *end == '\0') return INTVAL(v);
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _REAL_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    if (IS_REAL(a[0])) return a[0];
    if (IS_INT(a[0]))  return REALVAL((double)a[0].i);
    if (IS_STR(a[0]) && a[0].s) {
        char *end;
        double v = strtod(a[0].s, &end);
        if (end != a[0].s && *end == '\0') return REALVAL(v);
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _SIZE_(DESCR_t *a, int n) {
    if (n < 1) return INTVAL(0);
    if (IS_STR(a[0]) && a[0].slen) return INTVAL((int64_t)a[0].slen);
    const char *s = VARVAL_fn(a[0]);
    return INTVAL((int64_t)(s ? utf8_strlen(s) : 0));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _IDENT_(DESCR_t *a, int n) {
    DESCR_t x = (n > 0) ? a[0] : NULVCL;
    DESCR_t y = (n > 1) ? a[1] : NULVCL;
    return ident(x, y) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DIFFER_(DESCR_t *a, int n) {
    DESCR_t x = (n > 0) ? a[0] : NULVCL;
    DESCR_t y = (n > 1) ? a[1] : NULVCL;
    return differ(x, y) ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _VDIFFER_(DESCR_t *a, int n) {
    if (n < 2) return (n == 1) ? a[0] : FAILDESCR;
    DESCR_t x = a[0], y = a[1];
    if (x.v == DT_SNUL) { x.v = DT_S; x.s = ""; }
    if (y.v == DT_SNUL) { y.v = DT_S; y.s = ""; }
    int equal;
    if (x.v != y.v) {
        equal = 0;
    } else {
        switch (x.v) {
            case DT_I: equal = (x.i == y.i); break;
            case DT_R: equal = (x.r == y.r); break;
            case DT_S: {
                           const char *xs = x.s ? x.s : "";
                           const char *ys = y.s ? y.s : "";
                           equal = (strcmp(xs, ys) == 0); break; }
            default:   equal = (x.s == y.s); break;
        }
    }
    return equal ? FAILDESCR : a[0];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _NUMERIC_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    DESCR_t val = a[0];
    if (IS_INT(val))  return val;
    if (IS_REAL(val)) return val;
    if (IS_STR(val) || val.v == DT_SNUL) {
        const char *s = val.s ? val.s : "";
        while (*s == ' ') s++;
        if (!*s) return INTVAL(0);
        char *end = NULL;
        long long iv = strtoll(s, &end, 10);
        while (*end == ' ') end++;
        if (*end == '\0') return INTVAL((int64_t)iv);
        double rv = strtod(s, &end);
        while (*end == ' ') end++;
        if (*end == '\0') return REALVAL(rv);
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _NAME_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    DESCR_t val = a[0];
    if (IS_NAME(val)) {
        const char *nm = val.s ? val.s : "";
        return STRVAL(GC_strdup(nm));
    }
    const char *s = VARVAL_fn(val);
    return STRVAL(GC_strdup(s ? s : ""));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LGT_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *x = VARVAL_fn(a[0]); const char *y = VARVAL_fn(a[1]);
    if (!x) x = ""; if (!y) y = "";
    return strcmp(x, y) > 0 ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LLT_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *x = VARVAL_fn(a[0]); const char *y = VARVAL_fn(a[1]);
    if (!x) x = ""; if (!y) y = "";
    return strcmp(x, y) < 0 ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LGE_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *x = VARVAL_fn(a[0]); const char *y = VARVAL_fn(a[1]);
    if (!x) x = ""; if (!y) y = "";
    return strcmp(x, y) >= 0 ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LLE_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *x = VARVAL_fn(a[0]); const char *y = VARVAL_fn(a[1]);
    if (!x) x = ""; if (!y) y = "";
    return strcmp(x, y) <= 0 ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LEQ_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *x = VARVAL_fn(a[0]); const char *y = VARVAL_fn(a[1]);
    if (!x) x = ""; if (!y) y = "";
    return strcmp(x, y) == 0 ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LNE_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *x = VARVAL_fn(a[0]); const char *y = VARVAL_fn(a[1]);
    if (!x) x = ""; if (!y) y = "";
    return strcmp(x, y) != 0 ? NULVCL : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _HOST_(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    int64_t selector = to_int(a[0]);
    if (selector == 0) return STRVAL(GC_strdup(""));
    if (selector == 1) {
        char buf[32]; snprintf(buf, sizeof(buf), "%d", (int)getpid());
        return STRVAL(GC_strdup(buf));
    }
    if (selector == 3) return INTVAL(0);
    if (selector == 4 && n >= 2) {
        const char *envname = VARVAL_fn(a[1]);
        if (!envname || !*envname) return NULVCL;
        const char *val = getenv(envname);
        if (!val) return NULVCL;
        return STRVAL(GC_strdup(val));
    }
    return NULVCL;
}
#define IO_CHAN_MAX 32
typedef struct {
    FILE  *fp;
    char  *varname;
    int    is_output;
    char  *buf;
    size_t cap;
} io_chan_t;
static io_chan_t _io_chan[IO_CHAN_MAX];
static int _io_chan_init = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _io_chan_setup(void) {
    if (_io_chan_init) return;
    memset(_io_chan, 0, sizeof(_io_chan));
    _io_chan_init = 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int _io_chan_find_by_var(const char *name) {
    _io_chan_setup();
    for (int i = 0; i < IO_CHAN_MAX; i++)
        if (_io_chan[i].varname && strcmp(_io_chan[i].varname, name) == 0) return i;
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _io_chan_close(int ch) {
    _io_chan_setup();
    if (ch < 0 || ch >= IO_CHAN_MAX) return;
    if (_io_chan[ch].fp) { fclose(_io_chan[ch].fp); _io_chan[ch].fp = NULL; }
    if (_io_chan[ch].varname) { free(_io_chan[ch].varname); _io_chan[ch].varname = NULL; }
    if (_io_chan[ch].buf)  { free(_io_chan[ch].buf); _io_chan[ch].buf = NULL; }
    _io_chan[ch].cap = 0;
    _io_chan[ch].is_output = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ENDFILE_(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    int ch = (int)a[0].i;
    if (ch >= 0 && ch < IO_CHAN_MAX) _io_chan_close(ch);
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _APPLY_(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    const char *fname = NULL;
    if (a[0].v == DT_N) {
        if (a[0].slen == 0 && a[0].s && *a[0].s)
            fname = a[0].s;
        else if (a[0].slen == 1 && a[0].ptr)
            fname = NV_name_from_ptr((const DESCR_t *)a[0].ptr);
    }
    if (!fname) fname = VARVAL_fn(a[0]);
    return APPLY_fn(fname, a + 1, n - 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ARG_(DESCR_t *a, int n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DEFINE_(DESCR_t *a, int n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _FIELD_(DESCR_t *a, int n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LOCAL_(DESCR_t *a, int n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LPAD_(DESCR_t *a, int n) {
    if (n < 2) return n > 0 ? a[0] : NULVCL;
    return lpad_fn(a[0], a[1], n > 2 ? a[2] : STRVAL(" "));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _RPAD_(DESCR_t *a, int n) {
    if (n < 2) return n > 0 ? a[0] : NULVCL;
    return rpad_fn(a[0], a[1], n > 2 ? a[2] : STRVAL(" "));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _CHAR_(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return BCHAR_fn(a[0]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DUPL_(DESCR_t *a, int n) {
    if (n < 2) return NULVCL;
    return DUPL_fn(a[0], a[1]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _REMDR_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    int64_t x = to_int(a[0]), y = to_int(a[1]);
    if (y == 0) return FAILDESCR;
    return INTVAL(x % y);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _REPLACE_(DESCR_t *a, int n) {
    if (n < 3) return NULVCL;
    return REPLACE_fn(a[0], a[1], a[2]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _TRIM_(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return TRIM_fn(a[0]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _SUBSTR_(DESCR_t *a, int n) {
    if (n < 2) return NULVCL;
    if (n < 3) {
        DESCR_t big = { .v = DT_I, .slen = 0, .i = 999999999 };
        return SUBSTR_fn(a[0], a[1], big);
    }
    return SUBSTR_fn(a[0], a[1], a[2]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _REVERSE_(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return REVERS_fn(a[0]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DATATYPE_(DESCR_t *a, int n) {
    if (n < 1) return STRVAL("STRING");
    return STRVAL((char*)datatype(a[0]));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LCASE_(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    const char *s = VARVAL_fn(a[0]);
    if (!s) return NULVCL;
    char *r = GC_strdup(s);
    for (int i = 0; r[i]; i++) r[i] = (char)tolower((unsigned char)r[i]);
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _UCASE__fn(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    const char *s = VARVAL_fn(a[0]);
    if (!s) return NULVCL;
    char *r = GC_strdup(s);
    for (int i = 0; r[i]; i++) r[i] = (char)toupper((unsigned char)r[i]);
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t EVAL_fn(DESCR_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t code(const char *src);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t opsyn(DESCR_t, DESCR_t, DESCR_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t sort_fn(DESCR_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _EVAL_(DESCR_t *a, int n)  { return EVAL_fn(n>0?a[0]:NULVCL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _CODE_(DESCR_t *a, int n)  { return code(n>0?VARVAL_fn(a[0]):""); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _OPSYN_(DESCR_t *a, int n) {
    return opsyn(n>0?a[0]:NULVCL,n>1?a[1]:NULVCL,n>2?a[2]:NULVCL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _SORT_(DESCR_t *a, int n)  { return sort_fn(n>0?a[0]:NULVCL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DATE_(DESCR_t *a, int n) {
    (void)a; (void)n;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *buf = GC_malloc(20);
    strftime(buf, 20, "%m/%d/%Y %H:%M:%S", tm);
    return STRVAL(buf);
}
static int64_t _g_start_ms = -1;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _TIME_(DESCR_t *a, int n) {
    (void)a; (void)n;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if (_g_start_ms < 0) _g_start_ms = now_ms;
    return REALVAL((double)(now_ms - _g_start_ms));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _INPUT_(DESCR_t *a, int n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _OUTPUT_(DESCR_t *a, int n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ARRAY_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    const char *proto = VARVAL_fn(a[0]);
    if (proto && strchr(proto, ':')) {
        int lo = 1, hi = 1, lo2 = 1, hi2 = 1;
        const char *comma = strchr(proto, ',');
        if (comma) {
            sscanf(proto,  "%d:%d", &lo, &hi);
            if (strchr(comma+1, ':')) {
                sscanf(comma+1, "%d:%d", &lo2, &hi2);
            } else {
                lo2 = 1; hi2 = atoi(comma+1);
                if (hi2 < 1) hi2 = 1;
            }
            ARBLK_t *arr2d = array_new2d(lo, hi, lo2, hi2);
            arr2d->proto_bare = 0;
            return ARRAY_VAL(arr2d);
        }
        sscanf(proto, "%d:%d", &lo, &hi);
        ARBLK_t *arr1d = array_new(lo, hi);
        arr1d->proto_bare = 0;
        return ARRAY_VAL(arr1d);
    }
    if (proto && strchr(proto, ',')) {
        int r = 1, c = 1;
        sscanf(proto, "%d,%d", &r, &c);
        if (r < 1) r = 1;
        if (c < 1) c = 1;
        ARBLK_t *arrrc = array_new2d(1, r, 1, c);
        arrrc->proto_bare = 1;
        return ARRAY_VAL(arrrc);
    }
    int sz = (int)to_int(a[0]);
    if (sz < 1) return FAILDESCR;
    ARBLK_t *arr = array_new(1, sz);
    arr->proto_bare = 1;
    if (n >= 2) {
        for (int i = 0; i < sz; i++) arr->data[i] = a[1];
    }
    return ARRAY_VAL(arr);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _TABLE_(DESCR_t *a, int n) {
    int init = (n >= 1) ? (int)to_int(a[0]) : 10;
    int inc  = (n >= 2) ? (int)to_int(a[1]) : 10;
    return TABLE_VAL(table_new_args(init, inc));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _CONVERT_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    DESCR_t val  = a[0];
    const char *type = VARVAL_fn(a[1]);
    if (!type) return FAILDESCR;
    if (strcasecmp(type, "STRING")  == 0) {
        const char *s = VARVAL_fn(val);
        return s ? STRVAL(GC_strdup(s)) : NULVCL;
    }
    if (strcasecmp(type, "INTEGER") == 0) {
        if (!IS_STR(val) && !IS_INT(val) && !IS_REAL(val)) return FAILDESCR;
        return INTVAL((int64_t)to_int(val));
    }
    if (strcasecmp(type, "REAL")    == 0) {
        if (!IS_STR(val) && !IS_INT(val) && !IS_REAL(val)) return FAILDESCR;
        return REALVAL(to_real(val));
    }
    if (strcasecmp(type, "ARRAY")   == 0) {
        if (IS_ARR(val)) return val;
        if (IS_TBL(val) && val.tbl) {
            TBBLK_t *tbl = val.tbl;
            int n = tbl->size;
            if (n == 0) return FAILDESCR;
            ARBLK_t *a = array_new2d(1, n, 1, 2);
            a->proto_bare = 1;
            int row = 1;
            for (int b = 0; b < TABLE_BUCKETS && row <= n; b++) {
                for (TBPAIR_t *e = tbl->buckets[b]; e && row <= n; e = e->next) {
                    DESCR_t kd = (e->key_descr.v != DT_SNUL)
                                 ? e->key_descr
                                 : STRVAL(e->key ? e->key : "");
                    array_set2(a, row, 1, kd);
                    array_set2(a, row, 2, e->val);
                    row++;
                }
            }
            return ARRAY_VAL(a);
        }
        return FAILDESCR;
    }
    if (strcasecmp(type, "TABLE")   == 0) {
        if (IS_TBL(val)) return val;
        if (IS_ARR(val) && val.arr) {
            ARBLK_t *a = val.arr;
            int rows = a->hi - a->lo + 1;
            int cols = a->hi2 - a->lo2 + 1;
            if (cols != 2) return FAILDESCR;
            TBBLK_t *tbl = table_new_args(rows > 0 ? rows : 10, 10);
            for (int i = a->lo; i <= a->hi; i++) {
                DESCR_t kd = array_get2(a, i, a->lo2);
                DESCR_t vd = array_get2(a, i, a->lo2 + 1);
                const char *key = VARVAL_fn(kd);
                if (!key) continue;
                table_set_descr(tbl, key, kd, vd);
            }
            return TABLE_VAL(tbl);
        }
        return FAILDESCR;
    }
    if (strcasecmp(type, "PATTERN") == 0) {
        if (IS_PAT(val)) return val;
        if (IS_STR(val) || val.v == DT_SNUL) {
            const char *s = VARVAL_fn(val);
            return s ? pat_lit(s) : FAILDESCR;
        }
        return FAILDESCR;
    }
    if (strcasecmp(type, "CODE")       == 0) {
        const char *s = VARVAL_fn(val);
        if (!s || !*s) return FAILDESCR;
        return code(s);
    }
    if (strcasecmp(type, "EXPRESSION") == 0) {
        const char *s = VARVAL_fn(val);
        if (!s || !*s) return FAILDESCR;
        return compile_to_expression(s);
    }
    if (strcasecmp(type, "NAME") == 0) {
        const char *s = VARVAL_fn(val);
        if (!s || !*s) return FAILDESCR;
        return NAMEVAL(GC_strdup(s));
    }
    if (strcasecmp(type, "NUMERIC")    == 0) {
        if (IS_INT(val)) return val;
        if (IS_REAL(val)) return val;
        if (IS_STR(val) || val.v == DT_SNUL) {
            const char *s = val.s ? val.s : "";
            while (*s == ' ') s++;
            if (!*s) return INTVAL(0);
            char *end = NULL;
            long long iv = strtoll(s, &end, 10);
            while (*end == ' ') end++;
            if (*end == ' ') return INTVAL((int64_t)iv);
            double rv = strtod(s, &end);
            while (*end == ' ') end++;
            if (*end == ' ') return REALVAL(rv);
        }
        return FAILDESCR;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _COPY_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    DESCR_t v = a[0];
    if (IS_ARR(v)) {
        if (!v.arr) return v;
        int sz = v.arr->hi - v.arr->lo + 1;
        ARBLK_t *copy = array_new(v.arr->lo, v.arr->hi);
        copy->proto_bare = v.arr->proto_bare;
        for (int i = 0; i < sz; i++) copy->data[i] = v.arr->data[i];
        return ARRAY_VAL(copy);
    }
    return v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_nPush(DESCR_t *a, int n) {
    (void)a; (void)n;
    NPUSH_fn();
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_nInc(DESCR_t *a, int n) {
    (void)a; (void)n;
    NINC_fn();
    return INTVAL(ntop());
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_nDec(DESCR_t *a, int n) {
    (void)a; (void)n;
    NDEC_fn();
    return INTVAL(ntop());
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_nTop(DESCR_t *a, int n) {
    (void)a; (void)n;
    return INTVAL(ntop());
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_nPop(DESCR_t *a, int n) {
    (void)a; (void)n;
    int64_t val = ntop();
    NPOP_fn();
    return INTVAL(val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_tree_n(DESCR_t *a, int n) {
    if (n < 1) return INTVAL(0);
    return FIELD_GET_fn(a[0], "n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_tree_t(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return FIELD_GET_fn(a[0], "t");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_tree_v(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return FIELD_GET_fn(a[0], "v");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_tree_c(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return FIELD_GET_fn(a[0], "c");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_field_value(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return FIELD_GET_fn(a[0], "value");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _b_field_next(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    return FIELD_GET_fn(a[0], "next");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t rsort_fn(DESCR_t t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _RSORT_(DESCR_t *a, int n) { return rsort_fn(n>0?a[0]:NULVCL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _CLEAR_(DESCR_t *a, int n) {
    (void)a; (void)n;
    NV_CLEAR_fn();
    return NULVCL;
}
static char _setexit_label[256];
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _SETEXIT_(DESCR_t *a, int n) {
    if (n < 1 || a[0].v == DT_FAIL) {
        _setexit_label[0] = '\0';
        return NULVCL;
    }
    const char *lbl = VARVAL_fn(a[0]);
    if (lbl) strncpy(_setexit_label, lbl, sizeof(_setexit_label)-1);
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *setexit_label_get(void) {
    return _setexit_label[0] ? _setexit_label : NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _FUNCTION_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    const char *name = VARVAL_fn(a[0]);
    if (!name || !*name) return FAILDESCR;
    return FNCEX_fn(name) ? STRVAL(GC_strdup(name)) : FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int (*_label_exists_hook)(const char *) = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_set_label_exists_hook(int (*fn)(const char *)) { _label_exists_hook = fn; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LABEL_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    const char *name = VARVAL_fn(a[0]);
    if (!name || !*name) return FAILDESCR;
    if (_label_exists_hook && _label_exists_hook(name))
        return STRVAL(GC_strdup(name));
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _COLLECT_(DESCR_t *a, int n) {
    (void)a; (void)n;
    GC_gcollect();
    return INTVAL((int64_t)GC_get_free_bytes());
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void var_dump(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DUMP_(DESCR_t *a, int n) {
    (void)a; (void)n;
    var_dump();
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _TRACE_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    const char *varname = VARVAL_fn(a[0]);
    if (!varname || !*varname) return FAILDESCR;
    if (getenv("SCRIP_DEBUG_TRACE"))
        fprintf(stderr, "[scrip-trace] _TRACE_ entry n=%d varname=%s\n", n, varname);
    const char *type = (n >= 2) ? VARVAL_fn(a[1]) : "VALUE";
    if (!type || !*type) type = "VALUE";
    if (getenv("SCRIP_DEBUG_TRACE"))
        fprintf(stderr, "[scrip-trace] _TRACE_ type=%s\n", type);
    const char *cbfn = (n >= 4) ? VARVAL_fn(a[3]) : "";
    if (type && (strcmp(type,"VALUE")==0 || strcmp(type,"value")==0)) {
        if (cbfn && *cbfn) {
            trace_register_callback(varname, cbfn);
        } else {
            trace_register(varname);
        }
    }
    return STRVAL(GC_strdup(varname));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _STOPTR_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    const char *varname = VARVAL_fn(a[0]);
    if (!varname || !*varname) return FAILDESCR;
    trace_unregister(varname);
    return STRVAL(GC_strdup(varname));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DATBLK_t *_udef_lookup(const char *name);
typedef struct { char *typename; int nfields; char **fields; } DataClosure;
typedef struct { char *typename; char *fieldname; } FieldClosure;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _data_ctor_fn(DESCR_t *args, int nargs) {
    (void)args; (void)nargs;
    return NULVCL;
}
#define DATA_MAX_TYPES 64
#define DATA_MAX_FIELDS 16
static struct {
    char *typename;
    int   nfields;
    char *fields[DATA_MAX_FIELDS];
} _data_types[DATA_MAX_TYPES];
static int _data_ntypes = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _make_ctor(int tidx, DESCR_t *args, int nargs) {
    if (tidx < 0 || tidx >= _data_ntypes) return NULVCL;
    DATBLK_t *t = _udef_lookup(_data_types[tidx].typename);
    if (!t) return NULVCL;
    DATINST_t *u = GC_malloc(sizeof(DATINST_t));
    u->type   = t;
    u->fields = GC_malloc(t->nfields * sizeof(DESCR_t));
    for (int i = 0; i < t->nfields; i++)
        u->fields[i] = (i < nargs) ? args[i] : NULVCL;
    return (DESCR_t){ .v = DT_DATA, .u = u };
}
#define CTOR_FN(idx) \
static DESCR_t _ctor_##idx(DESCR_t *a, int n) { return _make_ctor(idx, a, n); }
CTOR_FN(0)  CTOR_FN(1)  CTOR_FN(2)  CTOR_FN(3)
CTOR_FN(4)  CTOR_FN(5)  CTOR_FN(6)  CTOR_FN(7)
CTOR_FN(8)  CTOR_FN(9)  CTOR_FN(10) CTOR_FN(11)
CTOR_FN(12) CTOR_FN(13) CTOR_FN(14) CTOR_FN(15)
CTOR_FN(16) CTOR_FN(17) CTOR_FN(18) CTOR_FN(19)
CTOR_FN(20) CTOR_FN(21) CTOR_FN(22) CTOR_FN(23)
CTOR_FN(24) CTOR_FN(25) CTOR_FN(26) CTOR_FN(27)
CTOR_FN(28) CTOR_FN(29) CTOR_FN(30) CTOR_FN(31)
CTOR_FN(32) CTOR_FN(33) CTOR_FN(34) CTOR_FN(35)
CTOR_FN(36) CTOR_FN(37) CTOR_FN(38) CTOR_FN(39)
CTOR_FN(40) CTOR_FN(41) CTOR_FN(42) CTOR_FN(43)
CTOR_FN(44) CTOR_FN(45) CTOR_FN(46) CTOR_FN(47)
CTOR_FN(48) CTOR_FN(49) CTOR_FN(50) CTOR_FN(51)
CTOR_FN(52) CTOR_FN(53) CTOR_FN(54) CTOR_FN(55)
CTOR_FN(56) CTOR_FN(57) CTOR_FN(58) CTOR_FN(59)
CTOR_FN(60) CTOR_FN(61) CTOR_FN(62) CTOR_FN(63)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t (*_ctor_fns[DATA_MAX_TYPES])(DESCR_t *, int) = {
    _ctor_0,  _ctor_1,  _ctor_2,  _ctor_3,
    _ctor_4,  _ctor_5,  _ctor_6,  _ctor_7,
    _ctor_8,  _ctor_9,  _ctor_10, _ctor_11,
    _ctor_12, _ctor_13, _ctor_14, _ctor_15,
    _ctor_16, _ctor_17, _ctor_18, _ctor_19,
    _ctor_20, _ctor_21, _ctor_22, _ctor_23,
    _ctor_24, _ctor_25, _ctor_26, _ctor_27,
    _ctor_28, _ctor_29, _ctor_30, _ctor_31,
    _ctor_32, _ctor_33, _ctor_34, _ctor_35,
    _ctor_36, _ctor_37, _ctor_38, _ctor_39,
    _ctor_40, _ctor_41, _ctor_42, _ctor_43,
    _ctor_44, _ctor_45, _ctor_46, _ctor_47,
    _ctor_48, _ctor_49, _ctor_50, _ctor_51,
    _ctor_52, _ctor_53, _ctor_54, _ctor_55,
    _ctor_56, _ctor_57, _ctor_58, _ctor_59,
    _ctor_60, _ctor_61, _ctor_62, _ctor_63,
};
#define FIELD_ACCESSOR_MAX (DATA_MAX_TYPES * DATA_MAX_FIELDS)
static struct { int tidx; int fidx; } _facc_slots[FIELD_ACCESSOR_MAX];
static int _facc_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _make_fget(int slot, DESCR_t obj) {
    if (slot < 0 || slot >= _facc_n) return FAILDESCR;
    int tidx = _facc_slots[slot].tidx;
    int fidx = _facc_slots[slot].fidx;
    if (!IS_DATA(obj) || !obj.u) return FAILDESCR;
    if (fidx < 0 || fidx >= obj.u->type->nfields) return FAILDESCR;
    return obj.u->fields[fidx];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _make_fset(int slot, DESCR_t obj, DESCR_t val) {
    if (slot < 0 || slot >= _facc_n) return;
    int fidx = _facc_slots[slot].fidx;
    if (!IS_DATA(obj) || !obj.u) return;
    if (fidx < 0 || fidx >= obj.u->type->nfields) return;
    obj.u->fields[fidx] = val;
}
#define FACC_FN(idx) \
static DESCR_t _facc_get_##idx(DESCR_t *a, int n) { \
    return n>=1 ? _make_fget(idx, a[0]) : NULVCL; }
#define FACC_SET_FN(idx) \
static DESCR_t _facc_set_##idx(DESCR_t *a, int n) { \
    if (n>=2) _make_fset(idx, a[1], a[0]); \
    return n>=1 ? a[0] : NULVCL; }
FACC_FN(0)   FACC_FN(1)   FACC_FN(2)   FACC_FN(3)
FACC_FN(4)   FACC_FN(5)   FACC_FN(6)   FACC_FN(7)
FACC_FN(8)   FACC_FN(9)   FACC_FN(10)  FACC_FN(11)
FACC_FN(12)  FACC_FN(13)  FACC_FN(14)  FACC_FN(15)
FACC_FN(16)  FACC_FN(17)  FACC_FN(18)  FACC_FN(19)
FACC_FN(20)  FACC_FN(21)  FACC_FN(22)  FACC_FN(23)
FACC_FN(24)  FACC_FN(25)  FACC_FN(26)  FACC_FN(27)
FACC_FN(28)  FACC_FN(29)  FACC_FN(30)  FACC_FN(31)
FACC_FN(32)  FACC_FN(33)  FACC_FN(34)  FACC_FN(35)
FACC_FN(36)  FACC_FN(37)  FACC_FN(38)  FACC_FN(39)
FACC_FN(40)  FACC_FN(41)  FACC_FN(42)  FACC_FN(43)
FACC_FN(44)  FACC_FN(45)  FACC_FN(46)  FACC_FN(47)
FACC_FN(48)  FACC_FN(49)  FACC_FN(50)  FACC_FN(51)
FACC_FN(52)  FACC_FN(53)  FACC_FN(54)  FACC_FN(55)
FACC_FN(56)  FACC_FN(57)  FACC_FN(58)  FACC_FN(59)
FACC_FN(60)  FACC_FN(61)  FACC_FN(62)  FACC_FN(63)
FACC_FN(64)  FACC_FN(65)  FACC_FN(66)  FACC_FN(67)
FACC_FN(68)  FACC_FN(69)  FACC_FN(70)  FACC_FN(71)
FACC_FN(72)  FACC_FN(73)  FACC_FN(74)  FACC_FN(75)
FACC_FN(76)  FACC_FN(77)  FACC_FN(78)  FACC_FN(79)
FACC_FN(80)  FACC_FN(81)  FACC_FN(82)  FACC_FN(83)
FACC_FN(84)  FACC_FN(85)  FACC_FN(86)  FACC_FN(87)
FACC_FN(88)  FACC_FN(89)  FACC_FN(90)  FACC_FN(91)
FACC_FN(92)  FACC_FN(93)  FACC_FN(94)  FACC_FN(95)
FACC_FN(96)  FACC_FN(97)  FACC_FN(98)  FACC_FN(99)
FACC_FN(100) FACC_FN(101) FACC_FN(102) FACC_FN(103)
FACC_FN(104) FACC_FN(105) FACC_FN(106) FACC_FN(107)
FACC_FN(108) FACC_FN(109) FACC_FN(110) FACC_FN(111)
FACC_FN(112) FACC_FN(113) FACC_FN(114) FACC_FN(115)
FACC_FN(116) FACC_FN(117) FACC_FN(118) FACC_FN(119)
FACC_FN(120) FACC_FN(121) FACC_FN(122) FACC_FN(123)
FACC_FN(124) FACC_FN(125) FACC_FN(126) FACC_FN(127)
FACC_SET_FN(0)   FACC_SET_FN(1)   FACC_SET_FN(2)   FACC_SET_FN(3)
FACC_SET_FN(4)   FACC_SET_FN(5)   FACC_SET_FN(6)   FACC_SET_FN(7)
FACC_SET_FN(8)   FACC_SET_FN(9)   FACC_SET_FN(10)  FACC_SET_FN(11)
FACC_SET_FN(12)  FACC_SET_FN(13)  FACC_SET_FN(14)  FACC_SET_FN(15)
FACC_SET_FN(16)  FACC_SET_FN(17)  FACC_SET_FN(18)  FACC_SET_FN(19)
FACC_SET_FN(20)  FACC_SET_FN(21)  FACC_SET_FN(22)  FACC_SET_FN(23)
FACC_SET_FN(24)  FACC_SET_FN(25)  FACC_SET_FN(26)  FACC_SET_FN(27)
FACC_SET_FN(28)  FACC_SET_FN(29)  FACC_SET_FN(30)  FACC_SET_FN(31)
FACC_SET_FN(32)  FACC_SET_FN(33)  FACC_SET_FN(34)  FACC_SET_FN(35)
FACC_SET_FN(36)  FACC_SET_FN(37)  FACC_SET_FN(38)  FACC_SET_FN(39)
FACC_SET_FN(40)  FACC_SET_FN(41)  FACC_SET_FN(42)  FACC_SET_FN(43)
FACC_SET_FN(44)  FACC_SET_FN(45)  FACC_SET_FN(46)  FACC_SET_FN(47)
FACC_SET_FN(48)  FACC_SET_FN(49)  FACC_SET_FN(50)  FACC_SET_FN(51)
FACC_SET_FN(52)  FACC_SET_FN(53)  FACC_SET_FN(54)  FACC_SET_FN(55)
FACC_SET_FN(56)  FACC_SET_FN(57)  FACC_SET_FN(58)  FACC_SET_FN(59)
FACC_SET_FN(60)  FACC_SET_FN(61)  FACC_SET_FN(62)  FACC_SET_FN(63)
FACC_SET_FN(64)  FACC_SET_FN(65)  FACC_SET_FN(66)  FACC_SET_FN(67)
FACC_SET_FN(68)  FACC_SET_FN(69)  FACC_SET_FN(70)  FACC_SET_FN(71)
FACC_SET_FN(72)  FACC_SET_FN(73)  FACC_SET_FN(74)  FACC_SET_FN(75)
FACC_SET_FN(76)  FACC_SET_FN(77)  FACC_SET_FN(78)  FACC_SET_FN(79)
FACC_SET_FN(80)  FACC_SET_FN(81)  FACC_SET_FN(82)  FACC_SET_FN(83)
FACC_SET_FN(84)  FACC_SET_FN(85)  FACC_SET_FN(86)  FACC_SET_FN(87)
FACC_SET_FN(88)  FACC_SET_FN(89)  FACC_SET_FN(90)  FACC_SET_FN(91)
FACC_SET_FN(92)  FACC_SET_FN(93)  FACC_SET_FN(94)  FACC_SET_FN(95)
FACC_SET_FN(96)  FACC_SET_FN(97)  FACC_SET_FN(98)  FACC_SET_FN(99)
FACC_SET_FN(100) FACC_SET_FN(101) FACC_SET_FN(102) FACC_SET_FN(103)
FACC_SET_FN(104) FACC_SET_FN(105) FACC_SET_FN(106) FACC_SET_FN(107)
FACC_SET_FN(108) FACC_SET_FN(109) FACC_SET_FN(110) FACC_SET_FN(111)
FACC_SET_FN(112) FACC_SET_FN(113) FACC_SET_FN(114) FACC_SET_FN(115)
FACC_SET_FN(116) FACC_SET_FN(117) FACC_SET_FN(118) FACC_SET_FN(119)
FACC_SET_FN(120) FACC_SET_FN(121) FACC_SET_FN(122) FACC_SET_FN(123)
FACC_SET_FN(124) FACC_SET_FN(125) FACC_SET_FN(126) FACC_SET_FN(127)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t (*_facc_set_fns[FIELD_ACCESSOR_MAX])(DESCR_t *, int) = {
    _facc_set_0,   _facc_set_1,   _facc_set_2,   _facc_set_3,
    _facc_set_4,   _facc_set_5,   _facc_set_6,   _facc_set_7,
    _facc_set_8,   _facc_set_9,   _facc_set_10,  _facc_set_11,
    _facc_set_12,  _facc_set_13,  _facc_set_14,  _facc_set_15,
    _facc_set_16,  _facc_set_17,  _facc_set_18,  _facc_set_19,
    _facc_set_20,  _facc_set_21,  _facc_set_22,  _facc_set_23,
    _facc_set_24,  _facc_set_25,  _facc_set_26,  _facc_set_27,
    _facc_set_28,  _facc_set_29,  _facc_set_30,  _facc_set_31,
    _facc_set_32,  _facc_set_33,  _facc_set_34,  _facc_set_35,
    _facc_set_36,  _facc_set_37,  _facc_set_38,  _facc_set_39,
    _facc_set_40,  _facc_set_41,  _facc_set_42,  _facc_set_43,
    _facc_set_44,  _facc_set_45,  _facc_set_46,  _facc_set_47,
    _facc_set_48,  _facc_set_49,  _facc_set_50,  _facc_set_51,
    _facc_set_52,  _facc_set_53,  _facc_set_54,  _facc_set_55,
    _facc_set_56,  _facc_set_57,  _facc_set_58,  _facc_set_59,
    _facc_set_60,  _facc_set_61,  _facc_set_62,  _facc_set_63,
    _facc_set_64,  _facc_set_65,  _facc_set_66,  _facc_set_67,
    _facc_set_68,  _facc_set_69,  _facc_set_70,  _facc_set_71,
    _facc_set_72,  _facc_set_73,  _facc_set_74,  _facc_set_75,
    _facc_set_76,  _facc_set_77,  _facc_set_78,  _facc_set_79,
    _facc_set_80,  _facc_set_81,  _facc_set_82,  _facc_set_83,
    _facc_set_84,  _facc_set_85,  _facc_set_86,  _facc_set_87,
    _facc_set_88,  _facc_set_89,  _facc_set_90,  _facc_set_91,
    _facc_set_92,  _facc_set_93,  _facc_set_94,  _facc_set_95,
    _facc_set_96,  _facc_set_97,  _facc_set_98,  _facc_set_99,
    _facc_set_100, _facc_set_101, _facc_set_102, _facc_set_103,
    _facc_set_104, _facc_set_105, _facc_set_106, _facc_set_107,
    _facc_set_108, _facc_set_109, _facc_set_110, _facc_set_111,
    _facc_set_112, _facc_set_113, _facc_set_114, _facc_set_115,
    _facc_set_116, _facc_set_117, _facc_set_118, _facc_set_119,
    _facc_set_120, _facc_set_121, _facc_set_122, _facc_set_123,
    _facc_set_124, _facc_set_125, _facc_set_126, _facc_set_127,
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t (*_facc_fns[FIELD_ACCESSOR_MAX])(DESCR_t *, int) = {
    _facc_get_0,   _facc_get_1,   _facc_get_2,   _facc_get_3,
    _facc_get_4,   _facc_get_5,   _facc_get_6,   _facc_get_7,
    _facc_get_8,   _facc_get_9,   _facc_get_10,  _facc_get_11,
    _facc_get_12,  _facc_get_13,  _facc_get_14,  _facc_get_15,
    _facc_get_16,  _facc_get_17,  _facc_get_18,  _facc_get_19,
    _facc_get_20,  _facc_get_21,  _facc_get_22,  _facc_get_23,
    _facc_get_24,  _facc_get_25,  _facc_get_26,  _facc_get_27,
    _facc_get_28,  _facc_get_29,  _facc_get_30,  _facc_get_31,
    _facc_get_32,  _facc_get_33,  _facc_get_34,  _facc_get_35,
    _facc_get_36,  _facc_get_37,  _facc_get_38,  _facc_get_39,
    _facc_get_40,  _facc_get_41,  _facc_get_42,  _facc_get_43,
    _facc_get_44,  _facc_get_45,  _facc_get_46,  _facc_get_47,
    _facc_get_48,  _facc_get_49,  _facc_get_50,  _facc_get_51,
    _facc_get_52,  _facc_get_53,  _facc_get_54,  _facc_get_55,
    _facc_get_56,  _facc_get_57,  _facc_get_58,  _facc_get_59,
    _facc_get_60,  _facc_get_61,  _facc_get_62,  _facc_get_63,
    _facc_get_64,  _facc_get_65,  _facc_get_66,  _facc_get_67,
    _facc_get_68,  _facc_get_69,  _facc_get_70,  _facc_get_71,
    _facc_get_72,  _facc_get_73,  _facc_get_74,  _facc_get_75,
    _facc_get_76,  _facc_get_77,  _facc_get_78,  _facc_get_79,
    _facc_get_80,  _facc_get_81,  _facc_get_82,  _facc_get_83,
    _facc_get_84,  _facc_get_85,  _facc_get_86,  _facc_get_87,
    _facc_get_88,  _facc_get_89,  _facc_get_90,  _facc_get_91,
    _facc_get_92,  _facc_get_93,  _facc_get_94,  _facc_get_95,
    _facc_get_96,  _facc_get_97,  _facc_get_98,  _facc_get_99,
    _facc_get_100, _facc_get_101, _facc_get_102, _facc_get_103,
    _facc_get_104, _facc_get_105, _facc_get_106, _facc_get_107,
    _facc_get_108, _facc_get_109, _facc_get_110, _facc_get_111,
    _facc_get_112, _facc_get_113, _facc_get_114, _facc_get_115,
    _facc_get_116, _facc_get_117, _facc_get_118, _facc_get_119,
    _facc_get_120, _facc_get_121, _facc_get_122, _facc_get_123,
    _facc_get_124, _facc_get_125, _facc_get_126, _facc_get_127,
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int fn_has_builtin(const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _func_init(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sno_DATA_register(DESCR_t *a, int n);   /* exported below as the non-static body */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DATA_(DESCR_t *a, int n) { return sno_DATA_register(a, n); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sno_DATA_register(DESCR_t *a, int n) {
    if (n < 1) return NULVCL;
    const char *raw_spec = VARVAL_fn(a[0]);
    if (!raw_spec || !*raw_spec) return NULVCL;
    char *spec = GC_strdup(raw_spec);
    DEFDAT_fn(spec);
    char *s = GC_strdup(spec);
    char *paren = strchr(s, '(');
    if (!paren) return NULVCL;
    *paren = '\0';
    char *tname = s;
    char *fstr = paren + 1;
    char *close = strchr(fstr, ')');
    if (close) *close = '\0';
    if (_data_ntypes >= DATA_MAX_TYPES) return NULVCL;
    int tidx = _data_ntypes++;
    char *uname = GC_strdup(tname);
    _data_types[tidx].typename = uname;
    int nf = 0;
    char *tmp = GC_strdup(fstr);
    char *tok = strtok(tmp, ",");
    while (tok && nf < DATA_MAX_FIELDS) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
        char *fld = GC_strdup(tok);
        _data_types[tidx].fields[nf] = fld;
        nf++;
        tok = strtok(NULL, ",");
    }
    _data_types[tidx].nfields = nf;
    extern void register_fn(const char *, DESCR_t (*)(DESCR_t*, int), int, int);
    register_fn(uname, _ctor_fns[tidx], 0, nf);
    for (int fi = 0; fi < nf; fi++) {
        if (_facc_n >= FIELD_ACCESSOR_MAX) break;
        int slot = _facc_n++;
        _facc_slots[slot].tidx = tidx;
        _facc_slots[slot].fidx = fi;
        const char *fname = _data_types[tidx].fields[fi];
        register_fn(fname, _facc_fns[slot], 1, 1);
        char setname[256];
        snprintf(setname, sizeof(setname), "%s_SET", fname);
        register_fn(setname, _facc_set_fns[slot], 2, 2);
    }
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_span(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_break_(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_breakx(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_any_cs(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_notany(const char *);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_len(int64_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_pos(int64_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_rpos(int64_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_tab(int64_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_rtab(int64_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_arb(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_rem(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_fail(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_abort(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_succeed(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_bal(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_arbno(DESCR_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_fence(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_fence_p(DESCR_t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_SPAN_(DESCR_t *a, int n)    { return n>=1 ? pat_span(VARVAL_fn(a[0]))    : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_BREAK_(DESCR_t *a, int n)   { return n>=1 ? pat_break_(VARVAL_fn(a[0]))  : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_BREAKX_(DESCR_t *a, int n)  { return n>=1 ? pat_breakx(VARVAL_fn(a[0]))  : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_ANY_(DESCR_t *a, int n)     { return n>=1 ? pat_any_cs(VARVAL_fn(a[0]))  : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_NOTANY_(DESCR_t *a, int n)  { return n>=1 ? pat_notany(VARVAL_fn(a[0]))  : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_LEN_(DESCR_t *a, int n)     { return n>=1 ? pat_len(to_int(a[0]))   : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_POS_(DESCR_t *a, int n)     { return n>=1 ? pat_pos(to_int(a[0]))   : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_RPOS_(DESCR_t *a, int n)    { return n>=1 ? pat_rpos(to_int(a[0]))  : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_TAB_(DESCR_t *a, int n)     { return n>=1 ? pat_tab(to_int(a[0]))   : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_RTAB_(DESCR_t *a, int n)    { return n>=1 ? pat_rtab(to_int(a[0]))  : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_ARB_(DESCR_t *a, int n)     { (void)a;(void)n; return pat_arb();     }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_REM_(DESCR_t *a, int n)     { (void)a;(void)n; return pat_rem();     }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_FAIL_(DESCR_t *a, int n)    { (void)a;(void)n; return pat_fail();    }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_ABORT_(DESCR_t *a, int n)   { (void)a;(void)n; return pat_abort();   }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_SUCCEED_(DESCR_t *a, int n) { (void)a;(void)n; return pat_succeed(); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_BAL_(DESCR_t *a, int n)     { (void)a;(void)n; return pat_bal();     }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_ARBNO_(DESCR_t *a, int n)   { return n>=1 ? pat_arbno(a[0])  : FAILDESCR; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_FENCE_(DESCR_t *a, int n)   { return n>=1 ? pat_fence_p(a[0]) : pat_fence(); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_ALT_(DESCR_t *a, int n)     { return n>=2 ? pat_alt(a[0], a[1])  : (n>=1 ? a[0] : FAILDESCR); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PAT_CONCAT_(DESCR_t *a, int n)  { return n>=2 ? pat_cat(a[0], a[1])  : (n>=1 ? a[0] : FAILDESCR); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _PROTOTYPE_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    DESCR_t v = a[0];
    if (IS_ARR(v) && v.arr) {
        ARBLK_t *arr = v.arr;
        char buf[128];
        if (arr->ndim > 1) {
            int cols = arr->hi2 - arr->lo2 + 1;
            if (arr->proto_bare)
                snprintf(buf, sizeof(buf), "%d,%d",
                         arr->hi - arr->lo + 1, cols);
            else
                snprintf(buf, sizeof(buf), "%d:%d,%d:%d",
                         arr->lo, arr->hi, arr->lo2, arr->hi2);
        } else {
            if (arr->proto_bare)
                snprintf(buf, sizeof(buf), "%d", arr->hi);
            else
                snprintf(buf, sizeof(buf), "%d:%d", arr->lo, arr->hi);
        }
        return STRVAL(GC_strdup(buf));
    }
    if (IS_TBL(v)) {
        return STRVAL("");
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ITEM_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    DESCR_t arr = a[0];
    if (IS_TBL(arr)) {
        const char *k = VARVAL_fn(a[1]);
        return table_get(arr.tbl, k ? k : "");
    }
    if (IS_ARR(arr)) {
        int i = (int)to_int(a[1]);
        if (n == 2) return array_get(arr.arr, i);
        int j = (int)to_int(a[2]);
        return array_get2(arr.arr, i, j);
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _VALUE_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    const char *name = VARVAL_fn(a[0]);
    if (!name) return FAILDESCR;
    char *fname = GC_strdup(name);
    return NV_GET_fn(fname);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void SNO_INIT_fn(void) {
    GC_INIT();
    for (int i = 0; i < 256; i++) alphabet[i] = (char)i;
    alphabet[256] = '\0';
    NV_SET_fn("ALPHABET", BSTRVAL(alphabet, 256));
    { struct timespec _ts; clock_gettime(CLOCK_MONOTONIC, &_ts);
      _g_start_ms = (int64_t)_ts.tv_sec * 1000 + _ts.tv_nsec / 1000000; }
    const char *mon_fifo = getenv("MONITOR_READY_PIPE");
    if (mon_fifo && mon_fifo[0]) {
        monitor_fd = open(mon_fifo, O_WRONLY | O_NONBLOCK);
        if (monitor_fd < 0) monitor_fd = open(mon_fifo, O_WRONLY);
        const char *go_pipe = getenv("MONITOR_GO_PIPE");
        if (go_pipe && go_pipe[0])
            monitor_ack_fd = open(go_pipe, O_RDONLY);
        const char *names_path = getenv("MONITOR_NAMES_FILE");
        if (names_path && names_path[0])
            (void)load_names_file_bin(names_path);
    } else {
        const char *mon = getenv("MONITOR");
        if (mon && mon[0] == '1') monitor_fd = 2;
    }
    {
        const char *ev_ftr = getenv("SCRIP_FTRACE");
        if (ev_ftr && ev_ftr[0]) {
            int64_t v = (int64_t)strtoll(ev_ftr, NULL, 10);
            if (v > 0) kw_ftrace = v;
        }
        const char *ev_tr = getenv("SCRIP_TRACE");
        if (ev_tr && ev_tr[0]) {
            int64_t v = (int64_t)strtoll(ev_tr, NULL, 10);
            if (v > 0) kw_trace = v;
        }
    }
    {
        const char *ev_bin = getenv("MONITOR_BIN");
        if (ev_bin && ev_bin[0] && ev_bin[0] != '0') {
            monitor_bin_mode = 1;
            atexit(mon_at_exit);
        }
    }
    extern void register_fn(const char *, DESCR_t (*)(DESCR_t*, int), int, int);
    register_fn("GT",       _GT_,       2, 2);
    register_fn("LT",       _LT_,       2, 2);
    register_fn("GE",       _GE_,       2, 2);
    register_fn("LE",       _LE_,       2, 2);
    register_fn("EQ",       _EQ_,       2, 2);
    register_fn("NE",       _NE_,       2, 2);
    register_fn("add",      _b_add,      2, 2);
    register_fn("sub",      _b_sub,      2, 2);
    register_fn("mul",      _b_mul,      2, 2);
    register_fn("DIVIDE_fn",_b_div,      2, 2);
    register_fn("POWER_fn", _b_pow,      2, 2);
    register_fn("neg",      _b_neg,      1, 1);
    register_fn("__num_pos", _b_pos,      1, 1);
    register_fn("PLS",       _b_pos,      1, 1);
    register_fn("INTEGER",  _INTEGER_,  1, 1);
    register_fn("REAL",     _REAL_,     1, 1);
    register_fn("SIZE",        _SIZE_,     1, 1);
    register_fn("IDENT",    _IDENT_,    0, 2);
    register_fn("DIFFER",   _DIFFER_,   0, 2);
    register_fn("VDIFFER",  _VDIFFER_,  0, 2);
    register_fn("NUMERIC",  _NUMERIC_,  1, 1);
    register_fn("NAME",     _NAME_,     1, 1);
    register_fn("LGT",      _LGT_,      2, 2);
    register_fn("LLT",      _LLT_,      2, 2);
    register_fn("LGE",      _LGE_,      2, 2);
    register_fn("LLE",      _LLE_,      2, 2);
    register_fn("LEQ",      _LEQ_,      2, 2);
    register_fn("LNE",      _LNE_,      2, 2);
    register_fn("HOST",     _HOST_,     1, 4);
    register_fn("ENDFILE",  _ENDFILE_,  1, 1);
    register_fn("APPLY",    _APPLY_,    1, 9);
    register_fn("LPAD",     _LPAD_,     2, 3);
    register_fn("RPAD",     _RPAD_,     2, 3);
    register_fn("CHAR",     _CHAR_,     1, 1);
    register_fn("DUPL",        _DUPL_,     2, 2);
    register_fn("REPLACE",  _REPLACE_,  3, 3);
    register_fn("REMDR",    _REMDR_,    2, 2);
    register_fn("TRIM",        _TRIM_,     1, 1);
    register_fn("SUBSTR",      _SUBSTR_,   2, 3);
    register_fn("REVERSE",  _REVERSE_,  1, 1);
    register_fn("DATATYPE", _DATATYPE_, 1, 1);
    register_fn("LCASE",    _LCASE_,    1, 1);
    register_fn("UCASE",    _UCASE__fn, 1, 1);
    register_fn("DATA",        _DATA_,     1, 1);
    register_fn("ARRAY",   _ARRAY_,   1, 2);
    register_fn("TABLE",   _TABLE_,   0, 2);
    register_fn("CONVERT", _CONVERT_, 2, 2);
    register_fn("PROTOTYPE", _PROTOTYPE_, 1, 1);
    register_fn("ITEM",    _ITEM_,    2, 9);
    register_fn("VALUE",   _VALUE_,   1, 1);
    register_fn("COPY",    _COPY_,    1, 1);
    register_fn("EVAL",  _EVAL_,  1, 1);
    register_fn("CODE",  _CODE_,  1, 1);
    register_fn("DEFINE", _DEFINE_, 1, 2);
    register_fn("FIELD",  _FIELD_,  2, 2);
    register_fn("OPSYN", _OPSYN_, 2, 3);
    register_fn("ARG",   _ARG_,   2, 2);
    register_fn("LOCAL", _LOCAL_, 2, 2);
    register_fn("SORT",  _SORT_,  1, 1);
    register_fn("INPUT",  _INPUT_,  1, 4);
    register_fn("OUTPUT", _OUTPUT_, 1, 4);
    register_fn("nPush",    _b_nPush,    0, 0);
    register_fn("nInc",     _b_nInc,     0, 0);
    register_fn("nDec",     _b_nDec,     0, 0);
    register_fn("nTop",     _b_nTop,     0, 0);
    register_fn("nPop",     _b_nPop,     0, 0);
    register_fn("n",        _b_tree_n,      1, 1);
    register_fn("t",        _b_tree_t,      1, 1);
    register_fn("v",        _b_tree_v,      1, 1);
    register_fn("c",        _b_tree_c,      1, 1);
    register_fn("DATE",     _DATE_,        0, 0);
    register_fn("TIME",     _TIME_,        0, 0);
    register_fn("DUMP",     _DUMP_,        0, 1);
    register_fn("TRACE",    _TRACE_,       1, 4);
    register_fn("STOPTR",   _STOPTR_,      1, 2);
    register_fn("LOAD",            _b_LOAD_stub,        1, 2);
    register_fn("MON_OPEN",        _b_MON_OPEN,         3, 3);
    register_fn("MON_PUT_S_VALUE", _b_MON_PUT_S_VALUE,  2, 2);
    register_fn("MON_PUT_I_VALUE", _b_MON_PUT_I_VALUE,  2, 2);
    register_fn("MON_PUT_R_VALUE", _b_MON_PUT_R_VALUE,  2, 2);
    register_fn("MON_PUT_O_VALUE", _b_MON_PUT_O_VALUE,  2, 2);
    register_fn("MON_PUT_S_RETURN",_b_MON_PUT_S_RETURN, 2, 2);
    register_fn("MON_PUT_I_RETURN",_b_MON_PUT_I_RETURN, 2, 2);
    register_fn("MON_PUT_R_RETURN",_b_MON_PUT_R_RETURN, 2, 2);
    register_fn("MON_PUT_O_RETURN",_b_MON_PUT_O_RETURN, 2, 2);
    register_fn("MON_PUT_CALL",    _b_MON_PUT_CALL,     1, 1);
    register_fn("MON_CLOSE",       _b_MON_CLOSE,        0, 0);
    register_fn("DATE",     _DATE_,        0, 0);
    register_fn("TIME",     _TIME_,        0, 0);
    register_fn("RSORT",    _RSORT_,       1, 1);
    register_fn("CLEAR",    _CLEAR_,       0, 0);
    register_fn("SETEXIT",  _SETEXIT_,     0, 1);
    register_fn("FUNCTION", _FUNCTION_,    1, 1);
    register_fn("LABEL",    _LABEL_,       1, 1);
    register_fn("COLLECT",  _COLLECT_,     0, 1);
    register_fn("SPAN",    _PAT_SPAN_,    1, 1);
    register_fn("BREAK",   _PAT_BREAK_,   1, 1);
    register_fn("BREAKX",  _PAT_BREAKX_,  1, 1);
    register_fn("ANY",     _PAT_ANY_,     1, 1);
    register_fn("NOTANY",  _PAT_NOTANY_,  1, 1);
    register_fn("LEN",     _PAT_LEN_,     1, 1);
    register_fn("POS",     _PAT_POS_,     1, 1);
    register_fn("RPOS",    _PAT_RPOS_,    1, 1);
    register_fn("TAB",     _PAT_TAB_,     1, 1);
    register_fn("RTAB",    _PAT_RTAB_,    1, 1);
    register_fn("ARB",     _PAT_ARB_,     0, 0);
    register_fn("REM",     _PAT_REM_,     0, 0);
    register_fn("FAIL",       _PAT_FAIL_,    0, 0);
    register_fn("ABORT",   _PAT_ABORT_,   0, 0);
    register_fn("SUCCEED", _PAT_SUCCEED_, 0, 0);
    register_fn("BAL",     _PAT_BAL_,     0, 0);
    register_fn("ARBNO",   _PAT_ARBNO_,   1, 1);
    register_fn("FENCE",   _PAT_FENCE_,   0, 1);
    register_fn("ALT",     _PAT_ALT_,     2, 2);
    register_fn("CONCAT",  _PAT_CONCAT_,  2, 2);
    {
        char *_ch = GC_malloc_atomic(2);
        _ch[0] = (char)9;  _ch[1] = '\0'; NV_SET_fn("tab", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)9;  _ch[1] = '\0'; NV_SET_fn("ht", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)10; _ch[1] = '\0'; NV_SET_fn("nl", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)10; _ch[1] = '\0'; NV_SET_fn("lf", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)13; _ch[1] = '\0'; NV_SET_fn("cr", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)12; _ch[1] = '\0'; NV_SET_fn("ff", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)11; _ch[1] = '\0'; NV_SET_fn("vt", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)8;  _ch[1] = '\0'; NV_SET_fn("bs", STRVAL(_ch));
        { char *_nul = GC_malloc_atomic(2); _nul[0] = '\0'; _nul[1] = '\0';
          NV_SET_fn("nul", BSTRVAL(_nul, 1)); }
        NV_SET_fn("epsilon", pat_epsilon());
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)47; _ch[1] = '\0'; NV_SET_fn("fSlash", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)92; _ch[1] = '\0'; NV_SET_fn("bSlash", STRVAL(_ch));
        _ch = GC_malloc_atomic(2);
        _ch[0] = (char)59; _ch[1] = '\0'; NV_SET_fn("semicolon", STRVAL(_ch));
        NV_SET_fn("UCASE",  STRVAL(ucase));
        NV_SET_fn("LCASE",  STRVAL(lcase));
        NV_SET_fn("digits", STRVAL("0123456789"));
    }
    NV_SET_fn("ARB",     pat_arb());
    NV_SET_fn("BAL",     pat_bal());
    NV_SET_fn("FENCE",   pat_fence());
    NV_SET_fn("ABORT",   pat_abort());
    NV_SET_fn("FAIL",    pat_fail());
    NV_SET_fn("REM",     pat_rem());
    NV_SET_fn("SUCCEED", pat_succeed());
    monitor_ready = 1;
    DEFDAT_fn("tree(t,v,n,c)");
    register_fn("c", _b_tree_c, 1, 1);
    register_fn("t", _b_tree_t, 1, 1);
    register_fn("v", _b_tree_v, 1, 1);
    register_fn("n", _b_tree_n, 1, 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
char *STRDUP_fn(const char *s) {
    if (!s) return GC_strdup("");
    return GC_strdup(s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
char *STRCONCAT_fn(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *r = GC_malloc(la + lb + 1);
    memcpy(r, a, la);
    memcpy(r + la, b, lb);
    r[la + lb] = '\0';
    return r;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t CONCAT_fn(DESCR_t a, DESCR_t b) {
    if (IS_FAIL(a)) return FAILDESCR;
    if (IS_FAIL(b)) return FAILDESCR;
    if (IS_PAT(a) || IS_PAT(b))
        return pat_cat(a, b);
    int a_null = IS_NULL_fn(a);
    int b_null = IS_NULL_fn(b);
    if (a_null && b_null) return NULVCL;
    if (a_null)            return b;
    if (b_null)            return a;
    const char *sa = VARVAL_fn(a);
    const char *sb = VARVAL_fn(b);
    return STRVAL(STRCONCAT_fn(sa, sb));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t size(const char *s) {
    return s ? (int64_t)strlen(s) : 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
char *VARVAL_fn(DESCR_t v) {
    char buf[64];
    switch (v.v) {
        case DT_SNUL:    return GC_strdup("");
        case DT_S:     return v.s ? v.s : GC_strdup("");
        case DT_I:
            snprintf(buf, sizeof(buf), "%" PRId64, v.i);
            return GC_strdup(buf);
        case DT_R: {
            snprintf(buf, sizeof(buf), "%.15g", v.r);
            if (!strchr(buf, '.') && !strchr(buf, 'e'))
                strncat(buf, ".", sizeof(buf) - strlen(buf) - 1);
            return GC_strdup(buf);
        }
        case DT_DATA:
            return v.u ? GC_strdup(v.u->type->name) : GC_strdup("");
        case DT_P:
            return GC_strdup("PATTERN");
        case DT_A: {
            if (!v.arr) return GC_strdup("ARRAY");
            ARBLK_t *arr = v.arr;
            char buf[128];
            if (arr->ndim > 1) {
                int cols = arr->hi2 - arr->lo2 + 1;
                if (arr->proto_bare)
                    snprintf(buf, sizeof(buf), "ARRAY('%d,%d')",
                             arr->hi - arr->lo + 1, cols);
                else
                    snprintf(buf, sizeof(buf), "ARRAY('%d:%d,%d:%d')",
                             arr->lo, arr->hi, arr->lo2, arr->hi2);
            } else {
                if (arr->proto_bare)
                    snprintf(buf, sizeof(buf), "ARRAY('%d')", arr->hi);
                else
                    snprintf(buf, sizeof(buf), "ARRAY('%d:%d')", arr->lo, arr->hi);
            }
            return GC_strdup(buf);
        }
        case DT_T: {
            if (!v.tbl) return GC_strdup("TABLE");
            char buf[64];
            snprintf(buf, sizeof(buf), "TABLE(%d,%d)", v.tbl->init, v.tbl->inc);
            return GC_strdup(buf);
        }
        case DT_N:
            if (v.s) return GC_strdup(v.s);
            if (v.ptr) {
                const char *nm = NV_name_from_ptr((const DESCR_t *)v.ptr);
                if (nm) return GC_strdup(nm);
                return VARVAL_fn(*(DESCR_t *)v.ptr);
            }
            return GC_strdup("");
        case DT_K:
            if (v.s) return VARVAL_fn(NV_GET_fn(v.s));
            return GC_strdup("");
        case DT_E:
            return GC_strdup("EXPRESSION");
        case DT_C:
            return GC_strdup("");
        default:
            return GC_strdup("");
    }
}
#include <setjmp.h>
jmp_buf g_sno_err_jmp;
int     g_sno_err_active = 0;
int     g_sno_err_stmt   = 0;
int     g_kw_ctx         = 0;
static const char *sno_err_msgs[40] = {
     NULL,
     "Illegal data type",
     "Error in arithmetic operation",
     "Erroneous array or table reference",
     "Null string in illegal context",
     "Undefined function or operation",
     "Erroneous prototype",
     "Unknown keyword",
     "Variable not present where required",
     "Entry point of function not label",
     "Illegal argument to primitive function",
     "Reading error",
     "Illegal i/o unit",
     "Limit on defined data types exceeded",
     "Negative number in illegal context",
     "String overflow",
     "Overflow during pattern matching",
     "Error in SNOBOL4 system",
     "Return from level zero",
     "Failure during goto evaluation",
     "Insufficient storage to continue",
     "Stack overflow",
     "Limit on statement execution exceeded",
     "Object exceeds size limit",
     "Undefined or erroneous goto",
     "Incorrect number of arguments",
     "Limit on compilation errors exceeded",
     "Erroneous END statement",
     "Execution of statement with compilation error",
     "Erroneous INCLUDE statement",
     "Cannot open INCLUDE file",
     "Erroneous LINE statement",
     "Missing END statement",
     "Output error",
     "User interrupt",
     "Not in a SETEXIT handler",
     "Error in BLOCKS",
     "Too many warnings in BLOCKS",
     "Mystery error in BLOCKS",
     "Cannot CONTINUE from FATAL error",
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sno_runtime_error(int code, const char *msg) {
    if (!msg && code >= 1 && code <= 39)
        msg = sno_err_msgs[code];
    fprintf(stderr, "\n** Error %d in statement %d\n   %s\n",
            code, g_sno_err_stmt, msg ? msg : "");
    if (sno_err_is_terminal(code)) exit(1);
    if (sno_err_is_fatal(code))    exit(1);
    if (g_sno_err_active) longjmp(g_sno_err_jmp, code);
    exit(1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t to_int(DESCR_t v) {
    switch (v.v) {
        case DT_I:  return v.i;
        case DT_R: return (int64_t)v.r;
        case DT_S:
        case DT_SNUL: {
            const char *s = v.s ? v.s : "";
            while (*s == ' ') s++;
            if (!*s) return 0;
            return (int64_t)strtoll(s, NULL, 10);
        }
        case DT_K:
            return to_int(NV_GET_fn(v.s));
        default:
            sno_runtime_error(1, NULL);
            return 0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
double to_real(DESCR_t v) {
    switch (v.v) {
        case DT_R: return v.r;
        case DT_I:  return (double)v.i;
        case DT_S:
        case DT_SNUL: {
            const char *s = v.s ? v.s : "";
            return strtod(s, NULL);
        }
        case DT_K:
            return to_real(NV_GET_fn(v.s));
        default:
            sno_runtime_error(1, NULL);
            return 0.0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *datatype(DESCR_t v) {
    switch (v.v) {
        case DT_SNUL:    return "STRING";
        case DT_S:       return "STRING";
        case DT_I:       return "INTEGER";
        case DT_R:       return "REAL";
        case DT_DATA:    return v.u ? v.u->type->name : "DATA";
        case DT_P:       return "PATTERN";
        case DT_A:       return "ARRAY";
        case DT_T:       return "TABLE";
        case DT_C:       return "CODE";
        case DT_E:       return "EXPRESSION";
        case DT_N:       return "NAME";
        case DT_K:       return "NAME";
        default:         return "STRING";
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
TREEBLK_t *expr_new(const char *tag, DESCR_t val) {
    TREEBLK_t *t = GC_malloc(sizeof(TREEBLK_t));
    t->tag = GC_strdup(tag ? tag : "");
    t->val = val;
    t->n   = 0;
    t->cap = 0;
    t->c   = NULL;
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
TREEBLK_t *tree_new0(const char *tag) {
    return expr_new(tag, NULVCL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _tree_ensure_cap(TREEBLK_t *x, int needed) {
    if (x->cap >= needed) return;
    int newcap = x->cap ? x->cap * 2 : 4;
    while (newcap < needed) newcap *= 2;
    TREEBLK_t **nc = GC_malloc(newcap * sizeof(TREEBLK_t *));
    if (x->c) memcpy(nc, x->c, x->n * sizeof(TREEBLK_t *));
    x->c   = nc;
    x->cap = newcap;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void tree_append(TREEBLK_t *x, TREEBLK_t *y) {
    _tree_ensure_cap(x, x->n + 1);
    x->c[x->n++] = y;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void tree_prepend(TREEBLK_t *x, TREEBLK_t *y) {
    _tree_ensure_cap(x, x->n + 1);
    memmove(x->c + 1, x->c, x->n * sizeof(TREEBLK_t *));
    x->c[0] = y;
    x->n++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void tree_insert(TREEBLK_t *x, TREEBLK_t *y, int place) {
    if (place < 1) place = 1;
    if (place > x->n + 1) place = x->n + 1;
    _tree_ensure_cap(x, x->n + 1);
    int idx = place - 1;
    memmove(x->c + idx + 1, x->c + idx, (x->n - idx) * sizeof(TREEBLK_t *));
    x->c[idx] = y;
    x->n++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
TREEBLK_t *tree_remove(TREEBLK_t *x, int place) {
    if (!x || place < 1 || place > x->n) return NULL;
    int idx = place - 1;
    TREEBLK_t *removed = x->c[idx];
    memmove(x->c + idx, x->c + idx + 1, (x->n - idx - 1) * sizeof(TREEBLK_t *));
    x->n--;
    return removed;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
ARBLK_t *array_new(int lo, int hi) {
    ARBLK_t *a = GC_malloc(sizeof(ARBLK_t));
    a->lo   = lo;
    a->hi   = hi;
    a->ndim = 1;
    int sz  = hi - lo + 1;
    if (sz < 1) sz = 1;
    a->data = GC_malloc(sz * sizeof(DESCR_t));
    for (int i = 0; i < sz; i++) a->data[i] = NULVCL;
    return a;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
ARBLK_t *array_new2d(int lo1, int hi1, int lo2, int hi2) {
    ARBLK_t *a = GC_malloc(sizeof(ARBLK_t));
    a->lo   = lo1;
    a->hi   = hi1;
    a->lo2  = lo2;
    a->hi2  = hi2;
    a->ndim = 2;
    int rows = hi1 - lo1 + 1;
    int cols = hi2 - lo2 + 1;
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    a->data = GC_malloc(rows * cols * sizeof(DESCR_t));
    for (int i = 0; i < rows * cols; i++) a->data[i] = NULVCL;
    return a;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t array_get(ARBLK_t *a, int i) {
    if (!a) return FAILDESCR;
    int idx = i - a->lo;
    if (idx < 0 || idx >= (a->hi - a->lo + 1)) return FAILDESCR;
    return a->data[idx];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void array_set(ARBLK_t *a, int i, DESCR_t v) {
    if (!a) return;
    int idx = i - a->lo;
    if (idx < 0 || idx >= (a->hi - a->lo + 1)) return;
    a->data[idx] = v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t array_get2(ARBLK_t *a, int i, int j) {
    if (!a) return FAILDESCR;
    int cols = a->hi2 - a->lo2 + 1;
    int row  = i - a->lo;
    int col  = j - a->lo2;
    int idx  = row * cols + col;
    int total = (a->hi - a->lo + 1) * cols;
    if (row < 0 || row >= (a->hi - a->lo + 1) || col < 0 || col >= cols || idx < 0 || idx >= total)
        return FAILDESCR;
    return a->data[idx];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void array_set2(ARBLK_t *a, int i, int j, DESCR_t v) {
    if (!a) return;
    int cols = a->hi2 - a->lo2 + 1;
    int row  = i - a->lo;
    int col  = j - a->lo2;
    int idx  = row * cols + col;
    a->data[idx] = v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t *array_ptr(ARBLK_t *a, int i) {
    if (!a) return NULL;
    int idx = i - a->lo;
    if (idx < 0 || idx >= (a->hi - a->lo + 1)) return NULL;
    return &a->data[idx];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static unsigned _tbl_hash(const char *key) {
    unsigned h = 5381;
    while (*key) h = h * 33 ^ (unsigned char)*key++;
    return h % TABLE_BUCKETS;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
TBBLK_t *table_new(void) {
    TBBLK_t *t = GC_malloc(sizeof(TBBLK_t));
    memset(t->buckets, 0, sizeof(t->buckets));
    t->size = 0;
    t->init = 10;
    t->inc  = 10;
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
TBBLK_t *table_new_args(int init, int inc) {
    TBBLK_t *t = table_new();
    t->init = (init > 0) ? init : 10;
    t->inc  = (inc  > 0) ? inc  : 10;
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t table_get(TBBLK_t *tbl, const char *key) {
    if (!tbl || !key) return NULVCL;
    unsigned h = _tbl_hash(key);
    for (TBPAIR_t *e = tbl->buckets[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e->val;
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void table_set(TBBLK_t *tbl, const char *key, DESCR_t val) {
    if (!tbl || !key) return;
    unsigned h = _tbl_hash(key);
    for (TBPAIR_t *e = tbl->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) { e->val = val; return; }
    }
    TBPAIR_t *e = GC_malloc(sizeof(TBPAIR_t));
    e->key       = GC_strdup(key);
    e->key_descr = STRVAL(e->key);
    e->val  = val;
    e->next = tbl->buckets[h];
    tbl->buckets[h] = e;
    tbl->size++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void table_set_descr(TBBLK_t *tbl, const char *key, DESCR_t key_d, DESCR_t val) {
    if (!tbl || !key) return;
    unsigned h = _tbl_hash(key);
    for (TBPAIR_t *e = tbl->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) { e->val = val; e->key_descr = key_d; return; }
    }
    TBPAIR_t *e = GC_malloc(sizeof(TBPAIR_t));
    e->key       = GC_strdup(key);
    e->key_descr = key_d;
    e->val  = val;
    e->next = tbl->buckets[h];
    tbl->buckets[h] = e;
    tbl->size++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int table_has(TBBLK_t *tbl, const char *key) {
    if (!tbl || !key) return 0;
    unsigned h = _tbl_hash(key);
    for (TBPAIR_t *e = tbl->buckets[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t *table_ptr(TBBLK_t *tbl, DESCR_t key_d) {
    if (!tbl) return NULL;
    const char *key = VARVAL_fn(key_d);
    if (!key) key = "";
    unsigned h = _tbl_hash(key);
    for (TBPAIR_t *e = tbl->buckets[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return &e->val;
    TBPAIR_t *e = GC_malloc(sizeof(TBPAIR_t));
    e->key       = GC_strdup(key);
    e->key_descr = key_d;
    e->val       = NULVCL;
    e->next      = tbl->buckets[h];
    tbl->buckets[h] = e;
    tbl->size++;
    return &e->val;
}
static DATBLK_t *_udef_types = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void DEFDAT_fn(const char *spec) {
    char *s = GC_strdup(spec);
    char *paren = strchr(s, '(');
    if (!paren) return;
    *paren = '\0';
    char *name = s;
    char *fields_str = paren + 1;
    char *close = strchr(fields_str, ')');
    if (close) *close = '\0';
    DATBLK_t *t = GC_malloc(sizeof(DATBLK_t));
    t->name = GC_strdup(name);
    int nfields = 0;
    char *tmp = GC_strdup(fields_str);
    char *tok = strtok(tmp, ",");
    while (tok) { nfields++; tok = strtok(NULL, ","); }
    t->nfields = nfields;
    t->fields  = GC_malloc(nfields * sizeof(char *));
    tmp = GC_strdup(fields_str);
    tok = strtok(tmp, ",");
    for (int i = 0; i < nfields && tok; i++) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
        char *fld = GC_strdup(tok);
        t->fields[i] = fld;
        tok = strtok(NULL, ",");
    }
    t->next    = _udef_types;
    _udef_types = t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DATBLK_t *_udef_lookup(const char *name) {
    for (DATBLK_t *t = _udef_types; t; t = t->next)
        if (strcasecmp(t->name, name) == 0) return t;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t DATCON_fn(const char *typename, ...) {
    DATBLK_t *t = _udef_lookup(typename);
    if (!t) return NULVCL;
    DATINST_t *u = GC_malloc(sizeof(DATINST_t));
    u->type   = t;
    u->fields = GC_malloc(t->nfields * sizeof(DESCR_t));
    for (int i = 0; i < t->nfields; i++) u->fields[i] = NULVCL;
    va_list ap;
    va_start(ap, typename);
    for (int i = 0; i < t->nfields; i++) {
        DESCR_t v = va_arg(ap, DESCR_t);
        if (IS_NULL(v) && v.s == NULL) break;
        u->fields[i] = v;
    }
    va_end(ap);
    return (DESCR_t){ .v = DT_DATA, .u = u };
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t FIELD_GET_fn(DESCR_t obj, const char *field) {
    if (!IS_DATA(obj) || !obj.u) return NULVCL;
    DATBLK_t *t = obj.u->type;
    for (int i = 0; i < t->nfields; i++)
        if (strcasecmp(t->fields[i], field) == 0)
            return obj.u->fields[i];
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void FIELD_SET_fn(DESCR_t obj, const char *field, DESCR_t val) {
    if (!IS_DATA(obj) || !obj.u) return;
    DATBLK_t *t = obj.u->type;
    for (int i = 0; i < t->nfields; i++)
        if (strcasecmp(t->fields[i], field) == 0) {
            obj.u->fields[i] = val;
            return;
        }
}
#define VAR_BUCKETS 512
typedef struct _VarEntry {
    char   *name;
    DESCR_t  val;
    struct _VarEntry *next;
} NV_t;
static NV_t *_var_buckets[VAR_BUCKETS];
static int _var_init_done = 0;
#define VAR_REG_MAX 1024
typedef struct { const char *name; DESCR_t *ptr; } VarReg;
static VarReg _var_reg[VAR_REG_MAX];
static int    _var_reg_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NV_REG_fn(const char *name, DESCR_t *ptr) {
    if (_var_reg_n < VAR_REG_MAX) {
        _var_reg[_var_reg_n].name = name;
        _var_reg[_var_reg_n].ptr  = ptr;
        _var_reg_n++;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _var_init(void) {
    if (_var_init_done) return;
    memset(_var_buckets, 0, sizeof(_var_buckets));
    _var_init_done = 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static unsigned _var_hash(const char *name) {
    unsigned h = 5381;
    while (*name) h = h * 33 ^ (unsigned char)*name++;
    return h % VAR_BUCKETS;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t NV_GET_fn(const char *name) {
    _var_init();
    if (!name) return NULVCL;
    if (strcmp(name, "INPUT") == 0) return input_read();
    _io_chan_setup();
    int ch = _io_chan_find_by_var(name);
    if (ch >= 0 && !_io_chan[ch].is_output && _io_chan[ch].fp) {
        ssize_t nread = getline(&_io_chan[ch].buf, &_io_chan[ch].cap, _io_chan[ch].fp);
        if (nread < 0) return FAILDESCR;
        if (nread > 0 && _io_chan[ch].buf[nread-1] == '\n') { _io_chan[ch].buf[nread-1] = '\0'; nread--; }
        if (kw_trim) {
            while (nread > 0 && (_io_chan[ch].buf[nread-1] == ' ' || _io_chan[ch].buf[nread-1] == '\t')) {
                _io_chan[ch].buf[--nread] = '\0';
            }
        }
        return STRVAL(GC_strdup(_io_chan[ch].buf));
    }
    if (strcmp(name, "STCOUNT")  == 0) return INTVAL(kw_stcount);
    if (strcmp(name, "STNO")     == 0) return INTVAL(kw_stno);
    if (strcmp(name, "STLIMIT")  == 0) return INTVAL(kw_stlimit);
    if (strcmp(name, "ANCHOR")   == 0) return INTVAL(kw_anchor);
    if (strcmp(name, "TRIM")     == 0) return INTVAL(kw_trim);
    if (strcmp(name, "FULLSCAN") == 0) return INTVAL(kw_fullscan);
    if (strcmp(name, "CASE")     == 0) return INTVAL(0);
    if (strcmp(name, "MAXLNGTH") == 0) return INTVAL(kw_maxlngth);
    if (strcmp(name, "FTRACE")   == 0) return INTVAL(kw_ftrace);
    if (strcmp(name, "TRACE")    == 0) return INTVAL(kw_trace);
    if (strcmp(name, "ERRLIMIT") == 0) return INTVAL(kw_errlimit);
    if (strcmp(name, "CODE")     == 0) return INTVAL(kw_code);
    if (strcmp(name, "FNCLEVEL") == 0) return INTVAL(kw_fnclevel);
    if (strcmp(name, "RTNTYPE")  == 0) return STRVAL(kw_rtntype);
    if (strcmp(name, "ALPHABET") == 0) return BSTRVAL(alphabet, 256);
    if (strcmp   (name, "&subject") == 0) {
        extern const char *scan_subj;
        return scan_subj ? STRVAL(scan_subj) : NULVCL;
    }
    if (strcmp   (name, "&pos") == 0) {
        extern int scan_pos;
        return INTVAL(scan_pos);
    }
    unsigned h = _var_hash(name);
    for (NV_t *e = _var_buckets[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e->val;
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t NV_SET_fn(const char *name, DESCR_t val) {
    _var_init();
    if (!name) return val;
    _io_chan_setup();
    int ch = _io_chan_find_by_var(name);
    if (ch >= 0 && _io_chan[ch].is_output && _io_chan[ch].fp) {
        char *s = VARVAL_fn(val);
        fprintf(_io_chan[ch].fp, "%s\n", s ? s : "");
        return val;
    }
    if (strcmp(name, "OUTPUT") == 0) { output_val(val); return val; }
    if (strcmp(name, "TERMINAL") == 0) {
        const char *s = IS_STR(val) ? val.s : "";
        fprintf(stderr, "%s\n", s);
        return val;
    }
    if (strcmp(name, "STLIMIT")  == 0) { kw_stlimit  = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "ANCHOR")   == 0) { kw_anchor   = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "TRIM")     == 0) { kw_trim     = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "FULLSCAN") == 0) { kw_fullscan = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "CASE")     == 0) {
        sno_runtime_error(10, "&CASE is read-only; SCRIP is case-sensitive only");
        return val;
    }
    if (strcmp(name, "MAXLNGTH") == 0) { kw_maxlngth = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "FTRACE")   == 0) { kw_ftrace   = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "TRACE")    == 0) { kw_trace    = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "ERRLIMIT") == 0) { kw_errlimit = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp(name, "CODE")     == 0) { kw_code     = (val.v==DT_I)?val.i:(int64_t)to_real(val); return val; }
    if (strcmp   (name, "&subject") == 0) {
        extern const char *scan_subj;
        const char *s = (val.v == DT_S) ? val.s : VARVAL_fn(val);
        scan_subj = s ? GC_strdup(s) : ""; return val;
    }
    if (strcmp   (name, "&pos") == 0) {
        extern int scan_pos;
        scan_pos = (int)((val.v==DT_I) ? val.i : (int64_t)to_real(val)); return val;
    }
    if (g_kw_ctx) {
        static const char *known_kw[] = {
            "STLIMIT","ANCHOR","TRIM","FULLSCAN","CASE","MAXLNGTH",
            "FTRACE","ERRLIMIT","CODE","FNCLEVEL","RTNTYPE",
            "ALPHABET","UCASE","LCASE","DIGITS","PI","PARM",
            "STEXEC","STCOUNT","STNO","DUMP","ABEND",
            "TRACE","GTRACE","FATALLIMIT","ERRLIMIT","ERRTYPE","ERRTEXT",
            "INPUT","OUTPUT","TERMINAL","PUNCHAR",
            NULL
        };
        int found = 0;
        for (int _ki = 0; known_kw[_ki]; _ki++)
            if (strcmp(name, known_kw[_ki]) == 0) { found = 1; break; }
        if (!found) {
            sno_runtime_error(7, NULL);
            return FAILDESCR;
        }
    }
    unsigned h = _var_hash(name);
    for (NV_t *e = _var_buckets[h]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            e->val = val;
            for (int _ri = 0; _ri < _var_reg_n; _ri++)
                if (strcmp(_var_reg[_ri].name, name) == 0) { *_var_reg[_ri].ptr = val; break; }
            comm_var(name, val);
            return;
        }
    }
    NV_t *e = GC_malloc(sizeof(NV_t));
    e->name = GC_strdup(name);
    e->val  = val;
    e->next = _var_buckets[h];
    _var_buckets[h] = e;
    for (int _ri = 0; _ri < _var_reg_n; _ri++)
        if (strcmp(_var_reg[_ri].name, name) == 0) { *_var_reg[_ri].ptr = val; break; }
    comm_var(name, val);
    return val;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t *NV_PTR_fn(const char *name) {
    _var_init();
    if (!name) return NULL;
    if (strcmp(name, "INPUT")  == 0) return NULL;
    if (strcmp(name, "OUTPUT") == 0) return NULL;
    if (strcmp(name, "STLIMIT")  == 0) return NULL;
    if (strcmp(name, "ANCHOR")   == 0) return NULL;
    if (strcmp(name, "TRIM")     == 0) return NULL;
    if (strcmp(name, "FULLSCAN") == 0) return NULL;
    if (strcmp(name, "CASE")     == 0) return NULL;
    if (strcmp(name, "MAXLNGTH") == 0) return NULL;
    if (strcmp(name, "FTRACE")   == 0) return NULL;
    if (strcmp(name, "TRACE")    == 0) return NULL;
    if (strcmp(name, "ERRLIMIT") == 0) return NULL;
    if (strcmp(name, "CODE")     == 0) return NULL;
    if (strcmp(name, "FNCLEVEL") == 0) return NULL;
    if (strcmp(name, "RTNTYPE")  == 0) return NULL;
    unsigned h = _var_hash(name);
    for (NV_t *e = _var_buckets[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return &e->val;
    NV_t *e = GC_malloc(sizeof(NV_t));
    e->name = GC_strdup(name);
    e->val  = NULVCL;
    e->next = _var_buckets[h];
    _var_buckets[h] = e;
    return &e->val;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *NV_name_from_ptr(const DESCR_t *ptr) {
    if (!ptr) return NULL;
    for (int i = 0; i < VAR_BUCKETS; i++)
        for (NV_t *e = _var_buckets[i]; e; e = e->next)
            if (&e->val == ptr) return e->name;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NV_CLEAR_fn(void) {
    _var_init();
    for (int _i = 0; _i < VAR_BUCKETS; _i++) {
        for (NV_t *_e = _var_buckets[_i]; _e; _e = _e->next)
            _e->val = NULVCL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void nv_reset(void) { NV_CLEAR_fn(); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int nv_snapshot(NvPair **out) {
    _var_init();
    int count = 0;
    for (int i = 0; i < VAR_BUCKETS; i++)
        for (NV_t *e = _var_buckets[i]; e; e = e->next)
            if (!IS_NULL(e->val)) count++;
    if (count == 0) { *out = NULL; return 0; }
    NvPair *pairs = GC_MALLOC((size_t)count * sizeof(NvPair));
    if (!pairs) { *out = NULL; return 0; }
    int idx = 0;
    for (int i = 0; i < VAR_BUCKETS; i++)
        for (NV_t *e = _var_buckets[i]; e; e = e->next)
            if (!IS_NULL(e->val)) { pairs[idx].name = e->name; pairs[idx].val = e->val; idx++; }
    *out = pairs;
    return count;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void nv_restore(const NvPair *pairs, int n) {
    nv_reset();
    for (int i = 0; i < n; i++) NV_SET_fn(pairs[i].name, pairs[i].val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NV_SYNC_fn(void) {
    for (int _ri = 0; _ri < _var_reg_n; _ri++) {
        DESCR_t v = NV_GET_fn(_var_reg[_ri].name);
        if (!IS_NULL(v) && v.v != 0)
            *_var_reg[_ri].ptr = v;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t INDR_GET_fn(const char *name) {
    DESCR_t indirect_name = NV_GET_fn(name);
    const char *target = VARVAL_fn(indirect_name);
    return NV_GET_fn(target);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void INDR_SET_fn(const char *name, DESCR_t val) {
    DESCR_t indirect_name = NV_GET_fn(name);
    const char *target = VARVAL_fn(indirect_name);
    NV_SET_fn(target, val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t NAME_fn(const char *varname) {
    if (!varname || !*varname) return FAILDESCR;
    if (strcmp(varname, "STLIMIT")  == 0 ||
        strcmp(varname, "ANCHOR")   == 0 ||
        strcmp(varname, "TRIM")     == 0 ||
        strcmp(varname, "FULLSCAN") == 0 ||
        strcmp(varname, "STCOUNT")  == 0 ||
        strcmp(varname, "STNO")     == 0 ||
        strcmp(varname, "ALPHABET") == 0 ||
        strcmp(varname, "CASE")     == 0 ||
        strcmp(varname, "MAXLNGTH") == 0 ||
        strcmp(varname, "FTRACE")   == 0 ||
        strcmp(varname, "ERRLIMIT") == 0 ||
        strcmp(varname, "CODE")     == 0 ||
        strcmp(varname, "FNCLEVEL") == 0 ||
        strcmp(varname, "RTNTYPE")  == 0 ||
        strcmp(varname, "INPUT")    == 0 ||
        strcmp(varname, "OUTPUT")   == 0)
        return NAMEVAL(GC_strdup(varname));
    DESCR_t *cell = NV_PTR_fn(varname);
    if (cell) return NAMEPTR(cell);
    return NAMEVAL(GC_strdup(varname));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ASGNIC_fn(const char *kw_name, DESCR_t val) {
    if (!kw_name) return 0;
    int64_t iv = IS_INT(val) ? val.i : (int64_t)to_real(val);
    if (strcmp(kw_name, "STLIMIT")  == 0) { kw_stlimit  = iv; return 1; }
    if (strcmp(kw_name, "ANCHOR")   == 0) { kw_anchor   = iv; return 1; }
    if (strcmp(kw_name, "TRIM")     == 0) { kw_trim     = iv; return 1; }
    if (strcmp(kw_name, "FULLSCAN") == 0) { kw_fullscan = iv; return 1; }
    if (strcmp(kw_name, "CASE")     == 0) {
        (void)iv;
        sno_runtime_error(10, "&CASE is read-only; SCRIP is case-sensitive only");
        return 1;
    }
    if (strcmp(kw_name, "MAXLNGTH") == 0) { kw_maxlngth = iv; return 1; }
    if (strcmp(kw_name, "FTRACE")   == 0) { kw_ftrace   = iv; return 1; }
    if (strcmp(kw_name, "ERRLIMIT") == 0) { kw_errlimit = iv; return 1; }
    if (strcmp(kw_name, "CODE")     == 0) { kw_code     = iv; return 1; }
    if (strcmp(kw_name, "STCOUNT")  == 0) return 1;
    if (strcmp(kw_name, "STNO")     == 0) return 1;
    if (strcmp(kw_name, "ALPHABET") == 0) return 1;
    if (strcmp(kw_name, "FNCLEVEL") == 0) return 1;
    if (strcmp(kw_name, "RTNTYPE")  == 0) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void var_dump(void) {
    fprintf(stderr, "[DUMP start]\n");
    for (int i = 0; i < VAR_BUCKETS; i++) {
        for (NV_t *e = _var_buckets[i]; e; e = e->next) {
            const char *tname;
            switch(e->val.v) {
                case 0: tname="NULL"; break;
                case 1: tname="STR"; break;
                case 2: tname="INT"; break;
                case 3: tname="REAL"; break;
                case 5: tname="PATTERN"; break;
                case 6: tname="DT_A"; break;
                case 7: tname="TABLE"; break;
                case 8: tname="DT_DATA"; break;
                case 9: tname="DT_FAIL"; break;
                default: tname="OTHER"; break;
            }
            if (IS_STR(e->val)) {
                const char *s = e->val.s ? e->val.s : "(null)";
                int len = (int)strlen(s);
                fprintf(stderr, "  %s = STR(%.*s)\n", e->name, len > 40 ? 40 : len, s);
            } else {
                fprintf(stderr, "  %s = %s\n", e->name, tname);
            }
        }
    }
    fprintf(stderr, "[DUMP end]\n");
}
#define NSTACK_MAX 256
static int64_t _nstack[NSTACK_MAX];
static int      _ntop = -1;
#define NHOME_MAX 256
static int _nhome[NHOME_MAX];
static int _nhome_top = -1;
int _nseq = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NPUSH_fn(void) {
    if (_ntop < NSTACK_MAX - 1) {
        ++_ntop;
        _nstack[_ntop] = 0;
        if (_nhome_top < NHOME_MAX - 1) {
            _nhome[++_nhome_top] = _ntop;
        }
    }
    fprintf(stderr, "SEQ%04d NPUSH depth=%d top=%lld\n",
            ++_nseq, _ntop, (long long)(_ntop >= 0 ? _nstack[_ntop] : 0));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int NHAS_FRAME_fn(void) { return _ntop >= 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NINC_fn(void) {
    if (_ntop >= 0) _nstack[_ntop]++;
    fprintf(stderr, "SEQ%04d NINC  depth=%d top=%lld\n",
            ++_nseq, _ntop, (long long)(_ntop >= 0 ? _nstack[_ntop] : 0));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NINC_AT_fn(int frame) {
    if (frame >= 0 && frame <= _ntop) _nstack[frame]++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NDEC_fn(void) { if (_ntop >= 0) _nstack[_ntop]--; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t ntop(void) { return (_ntop >= 0) ? _nstack[_ntop] : 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void NPOP_fn(void) {
    fprintf(stderr, "SEQ%04d NPOP  depth=%d top=%lld\n",
            ++_nseq, _ntop, (long long)(_ntop >= 0 ? _nstack[_ntop] : 0));
    if (_ntop >= 0) _ntop--;
}
#define VSTACK_MAX 1024
static DESCR_t _vstack[VSTACK_MAX];
static int    _vstop = -1;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void PUSH_fn(DESCR_t v) {
    if (_vstop < VSTACK_MAX - 1) _vstack[++_vstop] = v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t POP_fn(void) {
    if (_vstop >= 0) return _vstack[_vstop--];
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t TOP_fn(void) {
    if (_vstop >= 0) return _vstack[_vstop];
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int STACK_DEPTH_fn(void) {
    return _vstop + 1;
}
#define FUNC_BUCKETS 128
typedef struct _FNCBLK_t {
    char   *name;
    char   *spec;
    char   *entry_label;
    FNCPTR_t fn;
    int     nparams;
    char  **params;
    int     nlocals;
    char  **locals;
    struct _FuncEntry *next;
} FNCBLK_t;
static FNCBLK_t *_func_buckets[FUNC_BUCKETS];
static int        _func_init_done = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static unsigned _func_hash(const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int fn_has_builtin(const char *name) {
    if (!name) return 0;
    _func_init();
    unsigned h = _func_hash(name);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0 && e->fn != NULL) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void _func_init(void) {
    if (_func_init_done) return;
    memset(_func_buckets, 0, sizeof(_func_buckets));
    _func_init_done = 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static unsigned _func_hash(const char *name) {
    unsigned h = 5381;
    while (*name) h = h * 33 ^ (unsigned char)*name++;
    return h % FUNC_BUCKETS;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static FNCBLK_t *_parse_define_spec(const char *spec) {
    FNCBLK_t *fe = GC_malloc(sizeof(FNCBLK_t));
    char *s = GC_strdup(spec);
    fe->spec = GC_strdup(spec);
    char *paren = strchr(s, '(');
    if (!paren) {
        char *comma = strchr(s, ',');
        if (comma) {
            *comma = '\0';
            fe->name = GC_strdup(s);
            fe->entry_label = fe->name;
            char *lstr = GC_strdup(comma + 1);
            int nl = 0;
            char *tok = strtok(lstr, ",");
            while (tok) { nl++; tok = strtok(NULL, ","); }
            fe->nlocals = nl;
            fe->locals  = GC_malloc(nl * sizeof(char *));
            lstr = GC_strdup(comma + 1);
            tok  = strtok(lstr, ",");
            for (int i = 0; i < nl && tok; i++) {
                while (*tok == ' ') tok++;
                fe->locals[i] = GC_strdup(tok);
                tok = strtok(NULL, ",");
            }
        } else {
            fe->name = GC_strdup(s);
            fe->entry_label = fe->name;
        }
        fe->nparams = 0;
        fe->params  = NULL;
        return fe;
    }
    *paren = '\0';
    fe->name = GC_strdup(s);
    fe->entry_label = fe->name;
    char *close = strchr(paren + 1, ')');
    char *locals_str = NULL;
    if (close) {
        locals_str = close + 1;
        if (*locals_str == ',') locals_str++;
        *close = '\0';
    }
    char *pstr = GC_strdup(paren + 1);
    int np = 0;
    if (*pstr) {
        char *tok = strtok(pstr, ",");
        while (tok) { np++; tok = strtok(NULL, ","); }
    }
    fe->nparams = np;
    fe->params  = np ? GC_malloc(np * sizeof(char *)) : NULL;
    if (np) {
        pstr = GC_strdup(paren + 1);
        char *tok = strtok(pstr, ",");
        for (int i = 0; i < np && tok; i++) {
            while (*tok == ' ') tok++;
            fe->params[i] = GC_strdup(tok);
            tok = strtok(NULL, ",");
        }
    }
    int nl = 0;
    fe->nlocals = 0;
    fe->locals  = NULL;
    if (locals_str && *locals_str) {
        char *lstr = GC_strdup(locals_str);
        char *tok  = strtok(lstr, ",");
        while (tok) { nl++; tok = strtok(NULL, ","); }
        fe->nlocals = nl;
        fe->locals  = GC_malloc(nl * sizeof(char *));
        lstr = GC_strdup(locals_str);
        tok  = strtok(lstr, ",");
        for (int i = 0; i < nl && tok; i++) {
            while (*tok == ' ') tok++;
            fe->locals[i] = GC_strdup(tok);
            tok = strtok(NULL, ",");
        }
    }
    return fe;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void DEFINE_fn(const char *spec, FNCPTR_t fn) {
    _func_init();
    FNCBLK_t *fe = _parse_define_spec(spec);
    fe->fn = fn;
    unsigned h = _func_hash(fe->name);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next) {
        if (strcmp(e->name, fe->name) == 0) {
            e->spec    = fe->spec;
            e->fn      = fe->fn;
            e->nparams = fe->nparams;
            e->params  = fe->params;
            e->nlocals = fe->nlocals;
            e->locals  = fe->locals;
            return;
        }
    }
    fe->next = _func_buckets[h];
    _func_buckets[h] = fe;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void DEFINE_fn_entry(const char *spec, FNCPTR_t fn, const char *entry_label) {
    DEFINE_fn(spec, fn);
    if (!entry_label) return;
    _func_init();
    FNCBLK_t *fe = _parse_define_spec(spec);
    unsigned h = _func_hash(fe->name);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next) {
        if (strcmp(e->name, fe->name) == 0) {
            char *el = GC_strdup(entry_label);
            e->entry_label = el;
            return;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void register_fn_alias(const char *newname, const char *oldname) {
    _func_init();
    char *nn = GC_strdup(newname);
    char *on = GC_strdup(oldname);
    newname = nn; oldname = on;
    FNCBLK_t *old_entry = NULL;
    unsigned ho = _func_hash(oldname);
    for (FNCBLK_t *e = _func_buckets[ho]; e; e = e->next) {
        if (strcmp(e->name, oldname) == 0) { old_entry = e; break; }
    }
    FNCBLK_t *fe = GC_malloc(sizeof(FNCBLK_t));
    fe->name    = GC_strdup(newname);
    if (old_entry) {
        fe->spec        = old_entry->spec;
        fe->entry_label = old_entry->entry_label;
        fe->fn          = old_entry->fn;
        fe->nparams = old_entry->nparams;
        fe->params  = old_entry->params;
        fe->nlocals = old_entry->nlocals;
        fe->locals  = old_entry->locals;
    } else {
        fe->spec = GC_strdup(newname); fe->fn = NULL;
        fe->entry_label = fe->name;
        fe->nparams = 0; fe->params = NULL;
        fe->nlocals = 0; fe->locals = NULL;
    }
    fe->next = NULL;
    unsigned hn = _func_hash(newname);
    for (FNCBLK_t *e = _func_buckets[hn]; e; e = e->next) {
        if (strcmp(e->name, newname) == 0) {
            e->spec = fe->spec; e->fn = fe->fn;
            e->entry_label = fe->entry_label;
            e->nparams = fe->nparams; e->params = fe->params;
            e->nlocals = fe->nlocals; e->locals = fe->locals;
            return;
        }
    }
    fe->next = _func_buckets[hn];
    _func_buckets[hn] = fe;
}
DESCR_t (*g_user_call_hook)(const char *name, DESCR_t *args, int nargs) = NULL;
DESCR_t (*g_eval_pat_hook)(DESCR_t pat) = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t APPLY_fn(const char *name, DESCR_t *args, int nargs) {
    _func_init();
    if (!name) return NULVCL;
    unsigned h = _func_hash(name);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            if (e->fn) {
                return e->fn(args, nargs);
            }
            if (g_user_call_hook) return g_user_call_hook(name, args, nargs);
            return NULVCL;
        }
    }
    if (g_user_call_hook) {
        DESCR_t r = g_user_call_hook(name, args, nargs);
        if (!IS_FAIL_fn(r)) return r;
    }
    /* PST-RB-5i diagnostic: name the unresolved symbol when SCRIP_DEBUG_APPLY
       is set. Without this, "Undefined function or operation" gives no clue
       which SCRIP helper is missing when running parser_*.sc. Print BEFORE
       sno_runtime_error because that call longjmps/exits. */
    if (getenv("SCRIP_DEBUG_APPLY"))
        fprintf(stderr, "[apply-err5] unresolved '%s' (nargs=%d)\n", name ? name : "(null)", nargs);
    sno_runtime_error(5, NULL);
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _ARG_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *fname = VARVAL_fn(a[0]);
    if (!fname) return FAILDESCR;
    int64_t idx = to_int(a[1]);
    _func_init();
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next) {
        if (strcmp(e->name, fname) == 0) {
            if (idx < 1 || idx > (int64_t)e->nparams) return FAILDESCR;
            return STRVAL(GC_strdup(e->params[idx - 1]));
        }
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _LOCAL_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *fname = VARVAL_fn(a[0]);
    if (!fname) return FAILDESCR;
    int64_t idx = to_int(a[1]);
    _func_init();
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next) {
        if (strcmp(e->name, fname) == 0) {
            if (idx < 1 || idx > (int64_t)e->nlocals) return FAILDESCR;
            return STRVAL(GC_strdup(e->locals[idx - 1]));
        }
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _DEFINE_(DESCR_t *a, int n) {
    if (n < 1) return FAILDESCR;
    const char *proto = VARVAL_fn(a[0]);
    if (!proto || !*proto) return FAILDESCR;
    const char *entry = (n >= 2) ? VARVAL_fn(a[1]) : NULL;
    if (entry && !*entry) entry = NULL;
    if (entry)
        DEFINE_fn_entry(proto, NULL, entry);
    else
        DEFINE_fn(proto, NULL);
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _FIELD_(DESCR_t *a, int n) {
    if (n < 2) return FAILDESCR;
    const char *fname = VARVAL_fn(a[0]);
    if (!fname) return FAILDESCR;
    int64_t idx = to_int(a[1]);
    _func_init();
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next) {
        if (strcmp(e->name, fname) == 0) {
            if (idx < 1 || idx > (int64_t)e->nparams) return FAILDESCR;
            return STRVAL(GC_strdup(e->params[idx - 1]));
        }
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int FNCEX_fn(const char *name) {
    _func_init();
    if (!name) return 0;
    unsigned h = _func_hash(name);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int FUNC_NPARAMS_fn(const char *fname) {
    _func_init();
    if (!fname) return 0;
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next)
        if (strcmp(e->name, fname) == 0) return e->nparams;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int FUNC_NLOCALS_fn(const char *fname) {
    _func_init();
    if (!fname) return 0;
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next)
        if (strcmp(e->name, fname) == 0) return e->nlocals;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *FUNC_PARAM_fn(const char *fname, int i) {
    _func_init();
    if (!fname) return NULL;
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next)
        if (strcmp(e->name, fname) == 0)
            return (i >= 0 && i < e->nparams) ? e->params[i] : NULL;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *FUNC_LOCAL_fn(const char *fname, int i) {
    _func_init();
    if (!fname) return NULL;
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next)
        if (strcmp(e->name, fname) == 0)
            return (i >= 0 && i < e->nlocals) ? e->locals[i] : NULL;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *FUNC_ENTRY_fn(const char *fname) {
    _func_init();
    if (!fname) return NULL;
    unsigned h = _func_hash(fname);
    for (FNCBLK_t *e = _func_buckets[h]; e; e = e->next)
        if (strcmp(e->name, fname) == 0)
            return e->entry_label ? e->entry_label : e->name;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int FUNC_IS_ENTRY_LABEL(const char *label) {
    _func_init();
    if (!label || !*label) return 0;
    for (int b = 0; b < FUNC_BUCKETS; b++) {
        for (FNCBLK_t *e = _func_buckets[b]; e; e = e->next) {
            const char *el = e->entry_label ? e->entry_label : e->name;
            if (el && strcmp(el, label) == 0) return 1;
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t SIZE_fn(DESCR_t s) {
    const char *STRVAL_fn = VARVAL_fn(s);
    return INTVAL((int64_t)utf8_strlen(STRVAL_fn));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t DUPL_fn(DESCR_t s, DESCR_t n) {
    const char *STRVAL_fn = VARVAL_fn(s);
    int64_t times   = to_int(n);
    if (times < 0) return FAILDESCR;
    if (times == 0 || !STRVAL_fn || !*STRVAL_fn) return STRVAL(GC_strdup(""));
    size_t slen = strlen(STRVAL_fn);
    char *r = GC_malloc(slen * (size_t)times + 1);
    r[0] = '\0';
    for (int64_t i = 0; i < times; i++) memcpy(r + i * slen, STRVAL_fn, slen);
    r[slen * times] = '\0';
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t REPLACE_fn(DESCR_t s, DESCR_t from, DESCR_t to) {
    const char *sp   = IS_STR(s)    ? s.s    : VARVAL_fn(s);
    const char *fp   = IS_STR(from) ? from.s : VARVAL_fn(from);
    const char *tp   = IS_STR(to)   ? to.s   : VARVAL_fn(to);
    size_t slen_val  = descr_slen(s);
    unsigned char xlat[256];
    for (int i = 0; i < 256; i++) xlat[i] = (unsigned char)i;
    size_t flen = descr_slen(from), tlen = descr_slen(to);
    for (size_t i = 0; i < flen; i++) {
        unsigned char fc = (unsigned char)fp[i];
        unsigned char tc = (i < tlen) ? (unsigned char)tp[i] : 0;
        xlat[fc] = tc;
    }
    int binary_mode = (IS_STR(from) && from.slen) || (IS_STR(to) && to.slen)
                   || (IS_STR(s) && s.slen);
    char *r = GC_malloc(slen_val + 1);
    size_t rlen = 0;
    for (size_t i = 0; i < slen_val; i++) {
        unsigned char c = xlat[(unsigned char)sp[i]];
        if (binary_mode || c) r[rlen++] = (char)c;
    }
    r[rlen] = '\0';
    return binary_mode ? BSTRVAL(r, rlen) : STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t SUBSTR_fn(DESCR_t s, DESCR_t i, DESCR_t n) {
    const char *STRVAL_fn = VARVAL_fn(s);
    int64_t start   = to_int(i);
    int64_t len_    = to_int(n);
    size_t blen     = strlen(STRVAL_fn);
    size_t ncpts    = utf8_strlen(STRVAL_fn);
    if (start < 1) return FAILDESCR;
    if ((size_t)start > ncpts + 1) return STRVAL(GC_strdup(""));
    if (len_ < 0) len_ = 0;
    if ((size_t)(start - 1 + len_) > ncpts) len_ = (int64_t)(ncpts - (size_t)start + 1);
    size_t boff  = utf8_char_offset(STRVAL_fn, blen, (size_t)start);
    size_t bspan = utf8_char_bytes(STRVAL_fn, blen, boff, (size_t)len_);
    char *r = GC_malloc(bspan + 1);
    memcpy(r, STRVAL_fn + boff, bspan);
    r[bspan] = '\0';
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t TRIM_fn(DESCR_t s) {
    const char *STRVAL_fn = VARVAL_fn(s);
    int len = (int)strlen(STRVAL_fn);
    while (len > 0 && STRVAL_fn[len-1] == ' ') len--;
    char *r = GC_malloc((size_t)len + 1);
    memcpy(r, STRVAL_fn, (size_t)len);
    r[len] = '\0';
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t lpad_fn(DESCR_t s, DESCR_t n, DESCR_t pad) {
    const char *STRVAL_fn = VARVAL_fn(s);
    int64_t width   = to_int(n);
    const char *p   = VARVAL_fn(pad);
    char padch      = (p && *p) ? p[0] : ' ';
    int64_t slen    = (int64_t)strlen(STRVAL_fn);
    if (width <= slen) return STRVAL(GC_strdup(STRVAL_fn));
    int64_t npad = width - slen;
    char *r = GC_malloc((size_t)width + 1);
    memset(r, padch, (size_t)npad);
    memcpy(r + npad, STRVAL_fn, (size_t)slen);
    r[width] = '\0';
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t rpad_fn(DESCR_t s, DESCR_t n, DESCR_t pad) {
    const char *STRVAL_fn = VARVAL_fn(s);
    int64_t width   = to_int(n);
    const char *p   = VARVAL_fn(pad);
    char padch      = (p && *p) ? p[0] : ' ';
    int64_t slen    = (int64_t)strlen(STRVAL_fn);
    if (width <= slen) return STRVAL(GC_strdup(STRVAL_fn));
    char *r = GC_malloc((size_t)width + 1);
    memcpy(r, STRVAL_fn, (size_t)slen);
    memset(r + slen, padch, (size_t)(width - slen));
    r[width] = '\0';
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t REVERS_fn(DESCR_t s) {
    const char *STRVAL_fn = VARVAL_fn(s);
    int len = (int)strlen(STRVAL_fn);
    char *r = GC_malloc((size_t)len + 1);
    for (int i = 0; i < len; i++) r[i] = STRVAL_fn[len - 1 - i];
    r[len] = '\0';
    return STRVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t BCHAR_fn(DESCR_t n) {
    int64_t code = to_int(n);
    if (code < 0 || code >= 256) return FAILDESCR;
    char *buf = GC_malloc_atomic(2);
    buf[0] = (char)(code & 0xFF);
    buf[1] = '\0';
    return BSTRVAL(buf, 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t INTGER_fn(DESCR_t v) {
    if (IS_INT(v))  return v;
    if (IS_REAL(v)) return INTVAL((int64_t)v.r);
    if (IS_STR(v)) {
        const char *s = v.s ? v.s : "";
        while (*s == ' ') s++;
        if (!*s) return NULVCL;
        char *end;
        long long iv = strtoll(s, &end, 10);
        while (*end == ' ') end++;
        if (*end) return NULVCL;
        return INTVAL((int64_t)iv);
    }
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t real_fn(DESCR_t v) {
    if (IS_REAL(v)) return v;
    if (IS_INT(v))  return REALVAL((double)v.i);
    if (IS_STR(v)) {
        const char *s = v.s ? v.s : "";
        while (*s == ' ') s++;
        if (!*s) return NULVCL;
        char *end;
        double rv = strtod(s, &end);
        while (*end == ' ') end++;
        if (*end) return NULVCL;
        return REALVAL(rv);
    }
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t string_fn(DESCR_t v) {
    return STRVAL(VARVAL_fn(v));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t coerce_numeric(DESCR_t v) {
    if (IS_STR(v)) {
        const char *s = v.s ? v.s : "";
        while (*s == ' ') s++;
        if (*s == '+' || *s == '-') s++;
        if (!*s) return INTVAL(0);
        const char *p = s;
        while (*p >= '0' && *p <= '9') p++;
        while (*p == ' ') p++;
        if (*p == '\0' && p > s)
            return INTVAL((int64_t)strtoll(v.s ? v.s : "", NULL, 10));
    }
    return v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t add(DESCR_t a, DESCR_t b) {
    if (IS_FAIL(a) || IS_FAIL(b)) return FAILDESCR;
    if (IS_NULL(a)) a = INTVAL(0);
    if (IS_NULL(b)) b = INTVAL(0);
    a = coerce_numeric(a); b = coerce_numeric(b);
    if (IS_INT(a) && IS_INT(b))
        return INTVAL(a.i + b.i);
    return REALVAL(to_real(a) + to_real(b));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sub(DESCR_t a, DESCR_t b) {
    if (IS_FAIL(a) || IS_FAIL(b)) return FAILDESCR;
    if (IS_NULL(a)) a = INTVAL(0);
    if (IS_NULL(b)) b = INTVAL(0);
    a = coerce_numeric(a); b = coerce_numeric(b);
    if (IS_INT(a) && IS_INT(b))
        return INTVAL(a.i - b.i);
    return REALVAL(to_real(a) - to_real(b));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t mul(DESCR_t a, DESCR_t b) {
    if (IS_FAIL(a) || IS_FAIL(b)) return FAILDESCR;
    a = coerce_numeric(a); b = coerce_numeric(b);
    if (IS_INT(a) && IS_INT(b))
        return INTVAL(a.i * b.i);
    return REALVAL(to_real(a) * to_real(b));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t DIVIDE_fn(DESCR_t a, DESCR_t b) {
    if (IS_FAIL(a) || IS_FAIL(b)) return FAILDESCR;
    if (IS_INT(a) && IS_INT(b)) {
        if (b.i == 0) { sno_runtime_error(2, NULL); return FAILDESCR; }
        return INTVAL(a.i / b.i);
    }
    double denom = to_real(b);
    if (denom == 0.0) { sno_runtime_error(2, NULL); return FAILDESCR; }
    return REALVAL(to_real(a) / denom);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t POWER_fn(DESCR_t a, DESCR_t b) {
    if (IS_FAIL(a) || IS_FAIL(b)) return FAILDESCR;
    if (IS_INT(a) && IS_INT(b)) {
        int64_t ix = a.i, iy = b.i;
        if (ix == 0 && iy < 0) { sno_runtime_error(2, NULL); return FAILDESCR; }
        if (iy < 0) return INTVAL(0);
        int64_t p = 1;
        for (;;) {
            if (iy & 1) p *= ix;
            iy >>= 1;
            if (iy == 0) break;
            ix *= ix;
        }
        return INTVAL(p);
    }
    double r = pow(to_real(a), to_real(b));
    if (isinf(r) || isnan(r)) { sno_runtime_error(2, NULL); return FAILDESCR; }
    return REALVAL(r);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t neg(DESCR_t a) {
    if (IS_FAIL(a)) return FAILDESCR;
    if (IS_INT(a))  return INTVAL(-a.i);
    if (IS_REAL(a)) return REALVAL(-a.r);
    return INTVAL(-to_int(a));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pos(DESCR_t a) {
    if (IS_FAIL(a))  return FAILDESCR;
    if (IS_INT(a))   return a;
    if (IS_REAL(a))  return a;
    return INTVAL(to_int(a));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int eq(DESCR_t a, DESCR_t b) {
    if (IS_INT(a) && IS_INT(b)) return a.i == b.i;
    return to_real(a) == to_real(b);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ne(DESCR_t a, DESCR_t b) { return !eq(a, b); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int lt(DESCR_t a, DESCR_t b) {
    if (IS_INT(a) && IS_INT(b)) return a.i < b.i;
    return to_real(a) < to_real(b);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int le(DESCR_t a, DESCR_t b) {
    if (IS_INT(a) && IS_INT(b)) return a.i <= b.i;
    return to_real(a) <= to_real(b);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int gt(DESCR_t a, DESCR_t b) {
    if (IS_INT(a) && IS_INT(b)) return a.i > b.i;
    return to_real(a) > to_real(b);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ge(DESCR_t a, DESCR_t b) {
    if (IS_INT(a) && IS_INT(b)) return a.i >= b.i;
    return to_real(a) >= to_real(b);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int ident(DESCR_t a, DESCR_t b) {
    if (a.v != b.v) {
        int a_null = (IS_NULL(a) || (IS_STR(a) && descr_slen(a) == 0));
        int b_null = (IS_NULL(b) || (IS_STR(b) && descr_slen(b) == 0));
        if (a_null && b_null) return 1;
        return 0;
    }
    switch (a.v) {
        case DT_SNUL: return 1;
        case DT_S: {
            size_t la = descr_slen(a), lb = descr_slen(b);
            const char *sa = a.s ? a.s : "", *sb = b.s ? b.s : "";
            return la == lb && memcmp(sa, sb, la) == 0;
        }
        case DT_I:  return a.i == b.i;
        case DT_R: return a.r == b.r;
        case DT_DATA: return a.u == b.u;
        default:       return a.ptr == b.ptr;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int differ(DESCR_t a, DESCR_t b) { return !ident(a, b); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void output_val(DESCR_t v) {
    char *s = VARVAL_fn(v);
    printf("%s\n", s ? s : "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void output_str(const char *s) {
    printf("%s\n", s ? s : "");
}
static FILE *_input_fp = NULL;
static char *_input_buf = NULL;
static size_t _input_cap = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t input_read(void) {
    if (!_input_fp) _input_fp = stdin;
    ssize_t nread = getline(&_input_buf, &_input_cap, _input_fp);
    if (nread < 0) return FAILDESCR;
    if (nread > 0 && _input_buf[nread-1] == '\n') { _input_buf[nread-1] = '\0'; nread--; }
    if (kw_trim) {
        while (nread > 0 && (_input_buf[nread-1] == ' ' || _input_buf[nread-1] == '\t')) {
            _input_buf[--nread] = '\0';
        }
    }
    return STRVAL(GC_strdup(_input_buf));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *_io_extract_fname(const char *opts_str, char *buf, size_t bufsz) {
    if (!opts_str || !opts_str[0]) return NULL;
    const char *bracket = strchr(opts_str, '[');
    if (bracket) {
        size_t len = (size_t)(bracket - opts_str);
        while (len > 0 && opts_str[len-1] == ' ') len--;
        if (len > 0 && len < bufsz) {
            memcpy(buf, opts_str, len);
            buf[len] = '\0';
            return buf;
        }
        return NULL;
    }
    return opts_str;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *_io_varname(DESCR_t d) {
    if (IS_STR(d)) return d.s;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _INPUT_(DESCR_t *a, int n) {
    _io_chan_setup();
    char fname_buf[4096];
    const char *fname = NULL;
    if (n >= 4) {
        fname = VARVAL_fn(a[3]);
    } else if (n >= 3) {
        fname = _io_extract_fname(VARVAL_fn(a[2]), fname_buf, sizeof(fname_buf));
    }
    int ch = (n >= 2 && IS_INT(a[1])) ? (int)a[1].i : -1;
    if (!fname || !fname[0]) {
        if (_input_fp && _input_fp != stdin) fclose(_input_fp);
        _input_fp = stdin;
        return NULVCL;
    }
    FILE *f = fopen(fname, "r");
    if (!f) return FAILDESCR;
    if (ch >= 0 && ch < IO_CHAN_MAX) {
        _io_chan_close(ch);
        _io_chan[ch].fp = f;
        _io_chan[ch].is_output = 0;
        const char *vn = (n >= 1) ? _io_varname(a[0]) : NULL;
        _io_chan[ch].varname = vn ? strdup(vn) : NULL;
    } else {
        if (_input_fp && _input_fp != stdin) fclose(_input_fp);
        _input_fp = f;
    }
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t _OUTPUT_(DESCR_t *a, int n) {
    _io_chan_setup();
    char fname_buf[4096];
    const char *fname = NULL;
    if (n >= 4) {
        fname = VARVAL_fn(a[3]);
    } else if (n >= 3) {
        fname = _io_extract_fname(VARVAL_fn(a[2]), fname_buf, sizeof(fname_buf));
    } else if (n >= 1) {
        return NULVCL;
    }
    int ch = (n >= 2 && IS_INT(a[1])) ? (int)a[1].i : -1;
    if (!fname || !fname[0]) return FAILDESCR;
    FILE *f = fopen(fname, "w");
    if (!f) return FAILDESCR;
    if (ch >= 0 && ch < IO_CHAN_MAX) {
        _io_chan_close(ch);
        _io_chan[ch].fp = f;
        _io_chan[ch].is_output = 1;
        const char *vn = (n >= 1) ? _io_varname(a[0]) : NULL;
        _io_chan[ch].varname = vn ? strdup(vn) : NULL;
    } else {
        fclose(f);
        return FAILDESCR;
    }
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void indirect_goto(const char *varname) {
    DESCR_t v = NV_GET_fn(varname);
    const char *lbl = IS_STR(v) ? v.s : "(nil)";
    fprintf(stderr, "indirect_goto: var=%s label=%s (not implemented)\n",
            varname, lbl);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int nhome_info(void) { return (_nhome_top >= 0) ? _nhome[_nhome_top] : -1; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int NTOP_INDEX_fn(void) { return _ntop; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t NSTACK_AT_fn(int frame) { return (frame>=0 && frame<NSTACK_MAX) ? _nstack[frame] : 0; }
int _x4_pending_parent_frame = -1;
int _command_pending_parent_frame = -1;
