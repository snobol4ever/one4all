/*
 * interp_data.c — DATA registry and builtin_print/DATA
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

DESCR_t _builtin_print(DESCR_t *args, int nargs) {
    if (nargs == 0) { output_str(""); return NULVCL; }
    for (int i = 0; i < nargs; i++) output_val(args[i]);
    return NULVCL;
}

/* ── SC-1: DATA registry — constructor/accessor dispatch for --ir-run ─────────
 * DEFDAT_fn registers in the SPITBOL binary runtime table; APPLY_fn does not
 * expose DATA-defined names via FNCEX_fn.  We maintain our own registry so
 * interp_eval TT_FNC can dispatch constructors and field accessors directly. */

#define SC_DAT_MAX_FIELDS 64
#define SC_DAT_MAX_TYPES  128

static ScDatType sc_dat_types[SC_DAT_MAX_TYPES];
static int       sc_dat_ntypes = 0;

/* Parse "name(f1,f2,...)" spec and register in our table + DEFDAT_fn */
ScDatType *sc_dat_register(const char *spec) {
    if (sc_dat_ntypes >= SC_DAT_MAX_TYPES) return NULL;
    ScDatType *t = &sc_dat_types[sc_dat_ntypes];
    memset(t, 0, sizeof *t);
    /* SN-19 arch fix: sc_dat_register is case-policy-neutral shared runtime
     * serving SNOBOL4 (case-insensitive, caller pre-folds), Icon/Raku
     * (case-sensitive, caller passes verbatim). Store exactly as given. */
    /* parse name */
    const char *p = spec;
    int ni = 0;
    while (*p && *p != '(' && ni < 63) t->name[ni++] = *p++;
    t->name[ni] = '\0';
    if (*p == '(') p++; /* skip '(' */
    /* parse fields */
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

/* Public wrapper: register an Icon/SNOBOL4 record type spec from polyglot_init.
 * Also calls DEFDAT_fn so the SPITBOL runtime knows the type. */
void icn_record_register(const char *spec) {
    if (!spec || !*spec) return;
    /* Skip if already registered (polyglot_init may see the same file twice) */
    const char *p = spec;
    char name[64]; int ni = 0;
    while (*p && *p != '(' && ni < 63) name[ni++] = *p++;
    name[ni] = '\0';
    if (sc_dat_find_type(name)) return;   /* already registered */
    DEFDAT_fn(spec);
    sc_dat_register(spec);
}

/* Look up a DATA type by constructor name (case-insensitive) */
ScDatType *sc_dat_find_type(const char *name) {
    for (int i = 0; i < sc_dat_ntypes; i++)
        if (strcasecmp(sc_dat_types[i].name, name) == 0) return &sc_dat_types[i];
    return NULL;
}

/* Look up which type owns a field accessor name (case-insensitive) */
ScDatType *sc_dat_find_field(const char *name, int *fidx) {
    for (int i = 0; i < sc_dat_ntypes; i++)
        for (int j = 0; j < sc_dat_types[i].nfields; j++)
            if (strcasecmp(sc_dat_types[i].fields[j], name) == 0) {
                if (fidx) *fidx = j;
                return &sc_dat_types[i];
            }
    return NULL;
}

/* Construct a new DT_DATA instance: name(f0,f1,...) */
DESCR_t sc_dat_construct(ScDatType *t, DESCR_t *args, int nargs) {
    DATINST_t *inst = GC_malloc(sizeof(DATINST_t));
    /* Find the DATBLK_t — allocated by DEFDAT_fn in the SPITBOL runtime.
     * We locate it by calling DATCON_fn with zero args to get a prototype,
     * then steal its type pointer — or we build our own DATBLK_t. */
    /* Build a minimal DATBLK_t owned by us */
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
    r.u    = inst;   /* must be last union write — .s/.u share storage */
    return r;
}

/* Field accessor: name(obj) → obj.name ; with one arg assumed to be the instance */
DESCR_t sc_dat_field_get(const char *fname, DESCR_t obj) {
    DESCR_t *cell = data_field_ptr(fname, obj);
    if (!cell) return FAILDESCR;
    return *cell;
}

/* sc_dat_field_call — public entry for SM-run DATA dispatch.
 * Called from sm_interp SM_CALL_FN handler when args[0] is DT_DATA, to give
 * DATA field accessors/mutators/constructors priority over same-named builtins
 * (e.g. DATA field named 'real' must win over REAL() builtin when arg is DT_DATA).
 * Returns FAILDESCR if name is not a DATA constructor/field — caller falls through
 * to normal INVOKE_fn dispatch.
 * Convention for mutators: name ends in _SET, args[0]=rhs, args[1]=obj. */
DESCR_t sc_dat_field_call(const char *name, DESCR_t *args, int nargs) {
    if (!name || nargs < 1) return FAILDESCR;
    /* Constructor: name matches a DATA type name */
    ScDatType *_dt = sc_dat_find_type(name);
    if (_dt) return sc_dat_construct(_dt, args, nargs);
    /* Field accessor: name matches a field, arg is DT_DATA instance */
    int _fi = 0;
    ScDatType *_ft = sc_dat_find_field(name, &_fi);
    if (_ft) return sc_dat_field_get(name, args[0]);
    /* Field mutator: name ends in _SET — strip suffix, check field */
    size_t _nlen = strlen(name);
    if (_nlen > 4 && strcasecmp(name + _nlen - 4, "_SET") == 0 && nargs >= 2) {
        char _fname[128];
        size_t _flen = _nlen - 4;
        if (_flen >= sizeof(_fname)) _flen = sizeof(_fname) - 1;
        memcpy(_fname, name, _flen); _fname[_flen] = '\0';
        DESCR_t *_cell = data_field_ptr(_fname, args[1]);
        if (_cell) { *_cell = args[0]; return args[0]; }
    }
    return FAILDESCR;
}

/* ── DATA() builtin ─────────────────────────────────────────────────────── */
DESCR_t _builtin_DATA(DESCR_t *args, int nargs) {
    if (nargs < 1) return FAILDESCR;
    const char *raw_spec = VARVAL_fn(args[0]);
    if (!raw_spec || !*raw_spec) return FAILDESCR;
    /* SN-19 arch: _builtin_DATA is SNOBOL4's ingest boundary for DATA(). The
     * underlying DEFDAT_fn/sc_dat_register are case-policy-neutral; SNOBOL4
     * pre-folds here so its case-insensitive semantics reach the runtime. */
    char *spec = GC_strdup(raw_spec);
    sno_fold_name(spec);
    DEFDAT_fn(spec);              /* register in SPITBOL runtime (for DATATYPE() etc.) */
    sc_dat_register(spec);        /* register in our dispatch table */
    return NULVCL;
}
