#ifndef RUNTIME_SHIM_H
#define RUNTIME_SHIM_H
#include "snobol4.h"
#include <string.h>
#include <gc/gc.h>
#define IS_FAIL_fn(v)      IS_FAIL_fn(v)
#define get(v)          (v)
#define set(v, x)       ((v) = (x))
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _vint_impl(int64_t i)    { return INTVAL(i); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _real_impl(double d)    { return REALVAL(d); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _str_impl(const char *s) {
    if (!s) return NULVCL;
    char *p = GC_STRDUP(s);
    return STRVAL(p);
}
#define INTVAL_fn(i)   _vint_impl((int64_t)(i))
#define real(d)  _real_impl((double)(d))
#define STRVAL_fn(s)   _str_impl(s)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _kw_impl(const char *name) { return NV_GET_fn(name); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void   _kw_set_impl(const char *name, DESCR_t v) { NV_SET_fn(name, v); }
#define kw(name)         _kw_impl(name)
#define kw_set(name, v)  _kw_set_impl(name, v)
#ifdef CONCAT_fn
#undef CONCAT_fn
#endif
#define CONCAT_fn(a, b)    CONCAT_fn((a), (b))
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _alt_impl(DESCR_t a, DESCR_t b) { return pat_alt(a, b); }
#define alt(a, b)   _alt_impl(a, b)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _deref_impl(DESCR_t nameVal) {
    const char *name = VARVAL_fn(nameVal);
    if (!name || !*name) return NULVCL;
    return NV_GET_fn(name);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void _iset_impl(DESCR_t nameVal, DESCR_t v) {
    const char *name = VARVAL_fn(nameVal);
    if (name && *name) NV_SET_fn(name, v);
}
#define deref(nv)       _deref_impl(nv)
#define iset(nv, v)     _iset_impl(nv, v)
#define assign_expr(lvar, x)  ((lvar) = (x))
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _aref_impl(DESCR_t arr, DESCR_t *keys, int n) {
    if (n <= 0) return FAILDESCR;
    if (arr.v == DT_T) {
        const char *k = VARVAL_fn(keys[0]);
        return table_get(arr.tbl, k ? k : "");
    }
    if (arr.v == DT_A) {
        int i = (int)to_int(keys[0]);
        if (n == 1) return array_get(arr.arr, i);
        int j = (int)to_int(keys[1]);
        return array_get2(arr.arr, i, j);
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void _aset_impl(DESCR_t arr, DESCR_t *keys, int n, DESCR_t v) {
    if (n <= 0) return;
    if (arr.v == DT_T) {
        const char *k = VARVAL_fn(keys[0]);
        table_set_descr(arr.tbl, k ? k : "", keys[0], v);
        return;
    }
    if (arr.v == DT_A) {
        int i = (int)to_int(keys[0]);
        if (n == 1) { array_set(arr.arr, i, v); return; }
        int j = (int)to_int(keys[1]);
        array_set2(arr.arr, i, j, v);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _index_impl(DESCR_t base, DESCR_t *keys, int n) {
    return _aref_impl(base, keys, n);
}
#undef aref
#undef aset
#undef INDEX_fn
#define aref   _aref_impl
#define aset   _aset_impl
#define INDEX_fn  _index_impl
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _snoc_cursor_get(const char *varname) {
    (void)varname; return NULVCL;
}
#define cursor_get(n)   _snoc_cursor_get(n)
#define pat_break(chars)    pat_break_((chars))
#define pat_any(chars)      pat_any_cs((chars))
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _snoc_pat_var(const char *name) { return pat_ref(name); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _snoc_pat_val(DESCR_t v)          { return var_as_pattern(v); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _snoc_pat_deref(DESCR_t v)        { return var_as_pattern(v); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _snoc_pat_cond(DESCR_t child, const char *var) {
    return pat_assign_cond(child, STRVAL((char *)var));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t _snoc_pat_imm(DESCR_t child, const char *var) {
    return pat_assign_imm(child, STRVAL((char *)var));
}
#define pat_var(name)           _snoc_pat_var(name)
#define pat_val(v)              _snoc_pat_val(v)
#define pat_deref(v)            _snoc_pat_deref(v)
#define pat_cond(child, var)    _snoc_pat_cond(child, var)
#define pat_imm(child, var)     _snoc_pat_imm(child, var)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void INIT_fn(void)    { SNO_INIT_fn(); extern void inc_init(void); inc_init(); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void finish(void)  { }
#include <setjmp.h>
#define ABRT_STACK_INIT 16
static jmp_buf **_sno_abort_stack = NULL;
static int       _sno_abort_depth = 0;
static int       _sno_abort_cap   = 0;
static int       _sno_abort_lineno = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void push_abort_handler(jmp_buf *jb) {
    if (_sno_abort_depth >= _sno_abort_cap) {
        _sno_abort_cap = _sno_abort_cap ? _sno_abort_cap * 2 : ABRT_STACK_INIT;
        _sno_abort_stack = realloc(_sno_abort_stack, _sno_abort_cap * sizeof(jmp_buf *));
        if (!_sno_abort_stack) { fprintf(stderr, "abort stack OOM\n"); abort(); }
    }
    _sno_abort_stack[_sno_abort_depth++] = jb;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void pop_abort_handler(void) {
    if (_sno_abort_depth > 0) _sno_abort_depth--;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline void ABORT_fn(int lineno) {
    _sno_abort_lineno = lineno;
    if (_sno_abort_depth > 0)
        longjmp(*_sno_abort_stack[_sno_abort_depth-1], 1);
    fprintf(stderr, "ABORT at line %d\n", lineno);
    exit(1);
}
#define ABRT_GUARD_DECL   jmp_buf _stmt_jmp;
#define ABRT_GUARD_SET    (push_abort_handler(&_stmt_jmp), setjmp(_stmt_jmp))
#define ABRT_GUARD_POP    pop_abort_handler()
#endif
