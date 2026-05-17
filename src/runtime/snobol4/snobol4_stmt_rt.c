#include <string.h>
#include <gc.h>
#include "snobol4.h"
#include "snobol4_runtime_shim.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_init(void) {
    SNO_INIT_fn();
    NV_SYNC_fn();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_strval(const char *s) {
    return _str_impl(s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_intval(int64_t i) {
    return INTVAL(i);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_realval(const double *p) {
    return REALVAL(*p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_set_null(const char *name) {
    if (name && name[0] == '&') name++;
    NV_SET_fn(name, NULVCL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_set_indirect(DESCR_t name_val, DESCR_t val) {
    const char *s = VARVAL_fn(name_val);
    if (!s || !*s) return;
    NV_SET_fn(s, val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_get_indirect(DESCR_t name_val) {
    const char *s = NULL;
    if (name_val.v == DT_S) {
        s = VARVAL_fn(name_val);
    } else if (name_val.v == DT_N) {
        s = name_val.s;
    } else {
        return NULVCL;
    }
    if (!s || !*s) return NULVCL;
    return NV_GET_fn(s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_nreturn_deref(DESCR_t retval) {
    const char *s = NULL;
    if (retval.v == DT_N) {
        s = retval.s ? retval.s : NULL;
    } else if (retval.v == DT_S) {
        s = VARVAL_fn(retval);
    } else {
        return retval;
    }
    if (!s || !*s) return retval;
    DESCR_t v = NV_GET_fn(s);
    if (v.v == DT_SNUL) return NULVCL;
    return v;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_pos_var(const char *varname, int64_t cursor) {
    DESCR_t v = NV_GET_fn(varname);
    int64_t n = to_int(v);
    return (cursor == n) ? 1 : 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_rpos_var(const char *varname, int64_t cursor, int64_t subj_len) {
    DESCR_t v = NV_GET_fn(varname);
    int64_t n = to_int(v);
    return (cursor == subj_len - n) ? 1 : 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t stmt_span_var(const char *varname, int64_t cursor,
                      const char *subj, int64_t subj_len) {
    DESCR_t v = NV_GET_fn(varname);
    const char *cs = VARVAL_fn(v);
    if (!cs || cursor >= subj_len) return -1;
    int64_t pos = cursor;
    while (pos < subj_len) {
        char c = subj[pos];
        if (!strchr(cs, c)) break;
        pos++;
    }
    if (pos == cursor) return -1;
    return pos;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t stmt_break_var(const char *varname, int64_t cursor,
                       const char *subj, int64_t subj_len) {
    DESCR_t v = NV_GET_fn(varname);
    const char *cs = VARVAL_fn(v);
    if (!cs) cs = "";
    int64_t pos = cursor;
    while (pos < subj_len) {
        char c = subj[pos];
        if (strchr(cs, c)) break;
        pos++;
    }
    if (pos >= subj_len) return -1;
    return pos;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t stmt_breakx_var(const char *varname, int64_t cursor,
                        const char *subj, int64_t subj_len) {
    DESCR_t v = NV_GET_fn(varname);
    const char *cs = VARVAL_fn(v);
    if (!cs) cs = "";
    int64_t pos = cursor;
    while (pos < subj_len) {
        char c = subj[pos];
        if (strchr(cs, c)) break;
        pos++;
    }
    if (pos >= subj_len) return -1;
    if (pos == cursor)   return -1;
    return pos;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t stmt_breakx_lit(const char *cs, int64_t cursor,
                        const char *subj, int64_t subj_len) {
    if (!cs) cs = "";
    int64_t pos = cursor;
    while (pos < subj_len) {
        char c = subj[pos];
        if (strchr(cs, c)) break;
        pos++;
    }
    if (pos >= subj_len) return -1;
    if (pos == cursor)   return -1;
    return pos;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_any_var(const char *varname, int64_t cursor,
                 const char *subj, int64_t subj_len) {
    DESCR_t v = NV_GET_fn(varname);
    const char *cs = VARVAL_fn(v);
    if (!cs || cursor >= subj_len) return 0;
    return strchr(cs, subj[cursor]) ? 1 : 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_any_ptr(uint64_t vtype, void *vptr, int64_t cursor,
                 const char *subj, int64_t subj_len) {
    DESCR_t v; v.v = (int)vtype; v.p = vptr;
    const char *cs = VARVAL_fn(v);
    if (getenv("SNO_CALLDEBUG"))
        fprintf(stderr, "[stmt_any_ptr] vtype=%llu cs='%.20s' cursor=%lld subj[cur]='%c'\n",
                (unsigned long long)vtype, cs?cs:"(nil)",
                (long long)cursor,
                (cursor < subj_len) ? subj[cursor] : '?');
    if (!cs || cursor >= subj_len) return 0;
    return strchr(cs, subj[cursor]) ? 1 : 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t stmt_break_ptr(uint64_t vtype, void *vptr, int64_t cursor,
                       const char *subj, int64_t subj_len) {
    DESCR_t v; v.v = (int)vtype; v.p = vptr;
    const char *cs = VARVAL_fn(v);
    if (!cs) cs = "";
    int64_t pos = cursor;
    while (pos < subj_len) {
        char c = subj[pos];
        if (strchr(cs, c)) break;
        pos++;
    }
    if (pos >= subj_len) return -1;
    return pos;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t stmt_span_ptr(uint64_t vtype, void *vptr, int64_t cursor,
                      const char *subj, int64_t subj_len) {
    DESCR_t v; v.v = (int)vtype; v.p = vptr;
    const char *cs = VARVAL_fn(v);
    if (!cs || cursor >= subj_len) return -1;
    int64_t pos = cursor;
    while (pos < subj_len) {
        char c = subj[pos];
        if (!strchr(cs, c)) break;
        pos++;
    }
    if (pos == cursor) return -1;
    return pos;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_notany_var(const char *varname, int64_t cursor,
                    const char *subj, int64_t subj_len) {
    DESCR_t v = NV_GET_fn(varname);
    const char *cs = VARVAL_fn(v);
    if (!cs || cursor >= subj_len) return 0;
    return strchr(cs, subj[cursor]) ? 0 : 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_at_capture(const char *varname, int64_t cursor) {
    NV_SET_fn(varname, INTVAL(cursor));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_is_fail(DESCR_t v) {
    return IS_FAIL_fn(v);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_get(const char *name) {
    if (name && name[0] == '&') name++;
    DESCR_t r = NV_GET_fn(name);
    if (getenv("SNO_CALLDEBUG") && name &&
        (strcmp(name,"UCASE")==0 || strcmp(name,"LCASE")==0))
        fprintf(stderr, "[stmt_get %s] type=%d val='%.30s'\n",
                name, (int)r.v, r.v==1&&r.ptr?(const char*)r.ptr:"<non-str>");
    return r;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_set(const char *name, DESCR_t v) {
    if (name && name[0] == '&') name++;
    if (getenv("SNO_CALLDEBUG") && name && strcmp(name,"icase")==0)
        fprintf(stderr, "[SET icase] type=%d ptr=%p val=%s\n",
                (int)v.v, v.ptr, v.v==1&&v.ptr?(const char*)v.ptr:"<non-str>");
    NV_SET_fn(name, v);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_output(DESCR_t val) {
    NV_SET_fn("OUTPUT", val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_input(void) {
    return NV_GET_fn("INPUT");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_concat(DESCR_t a, DESCR_t b) {
    if (IS_FAIL_fn(a)) return FAILDESCR;
    if (IS_FAIL_fn(b)) return FAILDESCR;
    if (a.v == DT_P || b.v == DT_P || b.v == DT_N) {
        if (b.v == DT_N) {
            DESCR_t pa = (a.v == DT_P) ? a : pat_lit(VARVAL_fn(a));
            return pat_assign_cond(pa, b);
        }
        DESCR_t pa = (a.v == DT_P) ? a : pat_lit(VARVAL_fn(a));
        DESCR_t pb = (b.v == DT_P) ? b : pat_lit(VARVAL_fn(b));
        return pat_cat(pa, pb);
    }
    const char *sa = VARVAL_fn(a);
    const char *sb = VARVAL_fn(b);
    if (!sa) sa = "";
    if (!sb) sb = "";
    size_t la = strlen(sa), lb = strlen(sb);
    if (la == 0) return b;
    if (lb == 0) return a;
    char *buf = GC_MALLOC_ATOMIC(la + lb + 1);
    memcpy(buf, sa, la);
    memcpy(buf + la, sb, lb);
    buf[la + lb] = '\0';
    return STRVAL(buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_finish(void) {
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_goto_dispatch(DESCR_t label_val, const char **names, int n) {
    const char *s = VARVAL_fn(label_val);
    if (!s || !*s) return -1;
    char buf[512]; size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof(buf)-1; i++) {
        if (s[i] == '\'' || s[i] == '"') continue;
        buf[j++] = s[i];
    }
    buf[j] = '\0';
    for (int i = 0; i < n; i++) {
        if (strcmp(buf, names[i]) == 0) return i;
    }
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_add_int(DESCR_t a, DESCR_t b) {
    int64_t ia = (a.v == DT_I) ? a.i : (a.s ? atoll(a.s) : 0);
    int64_t ib = (b.v == DT_I) ? b.i : (b.s ? atoll(b.s) : 0);
    return INTVAL(ia + ib);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_gt(DESCR_t a, DESCR_t b) {
    int64_t ia = (a.v == DT_I) ? a.i : (a.s ? atoll(a.s) : 0);
    int64_t ib = (b.v == DT_I) ? b.i : (b.s ? atoll(b.s) : 0);
    return ia > ib;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_apply(const char *name, DESCR_t *args, int nargs) {
    DESCR_t r = APPLY_fn(name, args, nargs);
    if (getenv("SNO_CALLDEBUG") && name && strcmp(name,"ALT")==0)
        fprintf(stderr, "[stmt_apply ALT] nargs=%d arg0.type=%d arg1.type=%d -> result.type=%d\n",
                nargs,
                nargs>0?(int)args[0].v:-1,
                nargs>1?(int)args[1].v:-1,
                (int)r.v);
    return r;
}
extern uint64_t cursor;
extern uint64_t subject_len_val;
extern char     subject_data[65536];
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_setup_subject(DESCR_t subj) {
    if (IS_FAIL_fn(subj)) { subject_len_val = 0; return 1; }
    const char *s = VARVAL_fn(subj);
    if (!s) s = "";
    size_t len = descr_slen(subj);
    if (len == 0) len = strlen(s);
    if (len >= 65536) len = 65535;
    memcpy(subject_data, s, len);
    subject_data[len] = '\0';
    subject_len_val = (uint64_t)len;
    cursor = 0;
    return 0;
}
#define SUBJ_SAVE_DEPTH 32
static struct { char data[65536]; uint64_t len; uint64_t cur; } subj_stack[SUBJ_SAVE_DEPTH];
static int subj_stack_top = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_save_subject(void) {
    if (subj_stack_top >= SUBJ_SAVE_DEPTH) return;
    struct { char data[65536]; uint64_t len; uint64_t cur; } *s = &subj_stack[subj_stack_top++];
    memcpy(s->data, subject_data, (size_t)subject_len_val + 1);
    s->len = subject_len_val;
    s->cur = cursor;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_restore_subject(void) {
    if (subj_stack_top <= 0) return;
    struct { char data[65536]; uint64_t len; uint64_t cur; } *s = &subj_stack[--subj_stack_top];
    memcpy(subject_data, s->data, (size_t)s->len + 1);
    subject_len_val = s->len;
    cursor = s->cur;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_apply_replacement(const char *varname, DESCR_t repl) {
    if (varname && *varname)
        NV_SET_fn(varname, repl);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_apply_replacement_splice(const char *varname, DESCR_t repl,
                                   uint64_t match_start, uint64_t match_end) {
    if (!varname || !*varname) return;
    const char *rstr = "";
    size_t rlen = 0;
    if (repl.v == DT_S && repl.s) { rstr = repl.s; rlen = strlen(rstr); }
    else if (repl.v == DT_I) {
        static char ibuf[64];
        snprintf(ibuf, sizeof ibuf, "%ld", (long)repl.i);
        rstr = ibuf; rlen = strlen(rstr);
    } else if (repl.v == DT_R) {
        static char rbuf[64];
        snprintf(rbuf, sizeof rbuf, "%g", repl.r);
        rstr = rbuf; rlen = strlen(rstr);
    } else if (repl.v == DT_SNUL) {
        rstr = ""; rlen = 0;
    }
    if (match_start > subject_len_val) match_start = subject_len_val;
    if (match_end   > subject_len_val) match_end   = subject_len_val;
    if (match_end   < match_start)     match_end   = match_start;
    size_t prefix_len = (size_t)match_start;
    size_t suffix_len = (size_t)(subject_len_val - match_end);
    size_t total = prefix_len + rlen + suffix_len;
    char *out = GC_MALLOC_ATOMIC(total + 1);
    if (!out) return;
    memcpy(out, subject_data, prefix_len);
    memcpy(out + prefix_len, rstr, rlen);
    memcpy(out + prefix_len + rlen, subject_data + match_end, suffix_len);
    out[total] = '\0';
    NV_SET_fn(varname, STRVAL(out));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int match_pattern_at(DESCR_t pat, const char *subj, int subj_len, int cursor) {
    (void)pat; (void)subj; (void)subj_len; (void)cursor;
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_match_var(const char *varname) {
    DESCR_t val = NV_GET_fn(varname);
    if (val.v == DT_P) {
        int new_cur = match_pattern_at(val, subject_data,
                                       (int)subject_len_val, (int)cursor);
        if (new_cur < 0) return 0;
        cursor = (uint64_t)new_cur;
        return 1;
    }
    const char *s = VARVAL_fn(val);
    if (!s) return 0;
    size_t len = strlen(s);
    if (len == 0) return 1;
    if (cursor + len > subject_len_val) return 0;
    if (memcmp(subject_data + cursor, s, len) != 0) return 0;
    cursor += (uint64_t)len;
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int stmt_match_descr(uint64_t vtype, void *vptr) {
    DESCR_t val = { .v = (DTYPE_t)vtype, .slen = 0, .ptr = vptr };
    if (IS_FAIL_fn(val)) return 0;
    if (getenv("SNO_CALLDEBUG"))
        fprintf(stderr, "[stmt_match_descr] vtype=%llu DT_P=%d cursor=%llu subj_len=%llu\n",
                (unsigned long long)vtype, (int)DT_P,
                (unsigned long long)cursor, (unsigned long long)subject_len_val);
    if (val.v == DT_P) {
        int new_cur = match_pattern_at(val, subject_data,
                                       (int)subject_len_val, (int)cursor);
        if (new_cur < 0) return 0;
        cursor = (uint64_t)new_cur;
        return 1;
    }
    const char *s = VARVAL_fn(val);
    if (!s) return 0;
    size_t len = strlen(s);
    if (len == 0) return 1;
    if (cursor + len > subject_len_val) return 0;
    if (memcmp(subject_data + cursor, s, len) != 0) return 0;
    cursor += (uint64_t)len;
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_aref(DESCR_t arr, DESCR_t key) {
    DESCR_t keys[1] = { key };
    return _aref_impl(arr, keys, 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t stmt_aref2(DESCR_t arr, DESCR_t key1, DESCR_t key2) {
    DESCR_t keys[2] = { key1, key2 };
    return _aref_impl(arr, keys, 2);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_aset(DESCR_t arr, DESCR_t key, DESCR_t val) {
    DESCR_t keys[1] = { key };
    _aset_impl(arr, keys, 1, val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_aset2(DESCR_t arr, DESCR_t key1, DESCR_t key2, DESCR_t val) {
    DESCR_t keys[2] = { key1, key2 };
    _aset_impl(arr, keys, 2, val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_field_set(DESCR_t obj, const char *field, DESCR_t val) {
    FIELD_SET_fn(obj, field, val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void stmt_set_capture(const char *varname, const char *buf, uint64_t len) {
    if (!varname || !buf) return;
    if (len == (uint64_t)-1) return;
    char *s = GC_MALLOC_ATOMIC(len + 1);
    if (!s) return;
    memcpy(s, buf, len);
    s[len] = '\0';
    NV_SET_fn(varname, BSTRVAL(s, len));
}
