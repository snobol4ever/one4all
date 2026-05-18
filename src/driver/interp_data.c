#include "interp_private.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_print(DESCR_t *args, int nargs) {
    if (nargs == 0) { output_str(""); return NULVCL; }
    for (int i = 0; i < nargs; i++) output_val(args[i]);
    return NULVCL;
}
#define SC_DAT_MAX_FIELDS 64
#define SC_DAT_MAX_TYPES  128
static ScDatType sc_dat_types[SC_DAT_MAX_TYPES];
static int       sc_dat_ntypes = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
ScDatType *sc_dat_register(const char *spec) {
    if (sc_dat_ntypes >= SC_DAT_MAX_TYPES) return NULL;
    ScDatType *t = &sc_dat_types[sc_dat_ntypes];
    memset(t, 0, sizeof *t);
    const char *p = spec;
    int ni = 0;
    while (*p && *p != '(' && ni < 63) t->name[ni++] = *p++;
    t->name[ni] = '\0';
    if (*p == '(') p++;
    while (*p && *p != ')') {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == ')') break;
        int fi = 0;
        while (*p && *p != ',' && *p != ')' && fi < 63) t->fields[t->nfields][fi++] = *p++;
        t->fields[t->nfields][fi] = '\0';
        if (t->nfields < SC_DAT_MAX_FIELDS - 1) t->nfields++;
        if (*p == ',') p++;
    }
    sc_dat_ntypes++;
    return t;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_record_register(const char *spec) {
    if (!spec || !*spec) return;
    const char *p = spec;
    char name[64]; int ni = 0;
    while (*p && *p != '(' && ni < 63) name[ni++] = *p++;
    name[ni] = '\0';
    if (sc_dat_find_type(name)) return;
    DEFDAT_fn(spec);
    sc_dat_register(spec);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
ScDatType *sc_dat_find_type(const char *name) {
    for (int i = 0; i < sc_dat_ntypes; i++)
        if (strcmp(sc_dat_types[i].name, name) == 0) return &sc_dat_types[i];
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
ScDatType *sc_dat_find_field(const char *name, int *fidx) {
    for (int i = 0; i < sc_dat_ntypes; i++)
        for (int j = 0; j < sc_dat_types[i].nfields; j++)
            if (strcmp(sc_dat_types[i].fields[j], name) == 0) {
                if (fidx) *fidx = j;
                return &sc_dat_types[i];
            }
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sc_dat_construct(ScDatType *t, DESCR_t *args, int nargs) {
    DATINST_t *inst = GC_malloc(sizeof(DATINST_t));
    DATBLK_t *blk = GC_malloc(sizeof(DATBLK_t));
    blk->name    = GC_strdup(t->name);
    blk->nfields = t->nfields;
    blk->fields  = GC_malloc(t->nfields * sizeof(char *));
    for (int i = 0; i < t->nfields; i++) blk->fields[i] = GC_strdup(t->fields[i]);
    blk->next    = NULL;
    inst->type   = blk;
    inst->fields = GC_malloc(t->nfields * sizeof(DESCR_t));
    for (int i = 0; i < t->nfields; i++)
        inst->fields[i] = (i < nargs) ? args[i] : NULVCL;
    DESCR_t r;
    r.v    = DT_DATA;
    r.slen = 0;
    r.u    = inst;
    return r;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sc_dat_field_get(const char *fname, DESCR_t obj) {
    DESCR_t *cell = data_field_ptr(fname, obj);
    if (!cell) return FAILDESCR;
    return *cell;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t sc_dat_field_call(const char *name, DESCR_t *args, int nargs) {
    if (!name || nargs < 1) return FAILDESCR;
    ScDatType *_dt = sc_dat_find_type(name);
    if (_dt) return sc_dat_construct(_dt, args, nargs);
    int _fi = 0;
    ScDatType *_ft = sc_dat_find_field(name, &_fi);
    if (_ft) return sc_dat_field_get(name, args[0]);
    size_t _nlen = strlen(name);
    if (_nlen > 4 && strcmp(name + _nlen - 4, "_SET") == 0 && nargs >= 2) {
        char _fname[128];
        size_t _flen = _nlen - 4;
        if (_flen >= sizeof(_fname)) _flen = sizeof(_fname) - 1;
        memcpy(_fname, name, _flen); _fname[_flen] = '\0';
        DESCR_t *_cell = data_field_ptr(_fname, args[1]);
        if (_cell) { *_cell = args[0]; return args[0]; }
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t _builtin_DATA(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    const char *raw_spec = VARVAL_fn(args[0]);
    if (!raw_spec || !*raw_spec) return FAILDESCR;
    char *spec = GC_strdup(raw_spec);
    /* PST-RB-5i (per RULES "casing belongs at the ingress layer"):
       do not fold here. The spec arrives already canonicalized from the
       parser/lex layer; folding at this lookup-side helper would couple
       policy with mechanism and breaks case-sensitive runs of parser_*.sc. */
    DEFDAT_fn(spec);
    sc_dat_register(spec);
    /* PST-RB-5i: also register field-accessor functions in the SNOBOL4
       function table. The legacy split between _builtin_DATA (interp
       driver, registers in sc_dat_types[]) and _DATA_ (snobol4.c,
       registers in _func_buckets[]) meant calls like `value(x)` from
       SCRIP-hosted parsers failed: _builtin_DATA wins the register_fn
       race in scrip.c so _DATA_'s field-accessor registrations never
       executed. Chain through here so both tables are populated. */
    extern DESCR_t sno_DATA_register(DESCR_t *a, int n);
    sno_DATA_register(args, nargs);
    return NULVCL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t *data_field_ptr(const char *fname, DESCR_t inst) {
    if (inst.v < DT_DATA || !inst.u) return NULL;
    DATBLK_t *blk = inst.u->type;
    if (!blk) return NULL;
    for (int i = 0; i < blk->nfields; i++)
        if (blk->fields[i] && strcmp(blk->fields[i], fname) == 0)
            return &inst.u->fields[i];
    return NULL;
}
#include "../runtime/interp/icn_runtime.h"
