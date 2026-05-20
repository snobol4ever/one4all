#include "SM.h"
#include "BB.h"
#include "stage2.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/*── The one Stage 2 build target (in .bss).  See stage2.h. ──────────────*/
stage2_t g_stage2;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Initialize the SM_sequence_t in place.  Called by SM_seq_new() and by
 * stage2_reset() for the embedded SM piece.  Allocates the dynamic arrays
 * (instrs / stno_labels / bb_table) but not the struct itself.              */
static void sm_seq_init(SM_sequence_t *p)
{
    p->cap             = 64;
    p->count           = 0;
    p->instrs          = calloc((size_t)p->cap, sizeof(SM_t));
    p->stno_labels_cap = 64;
    p->stno_labels     = calloc((size_t)p->stno_labels_cap, sizeof(const char *));
    p->stno_count      = 0;
    p->bb_cap          = 16;
    p->bb_count        = 0;
    p->bb_table        = calloc((size_t)p->bb_cap, sizeof(BB_graph_t *));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Deinitialize the SM_sequence_t in place — frees the dynamic arrays but
 * not the struct itself.  Mirror of sm_seq_init.                            */
static void sm_seq_deinit(SM_sequence_t *p)
{
    free(p->instrs);      p->instrs      = NULL;
    free(p->stno_labels); p->stno_labels = NULL;
    free(p->bb_table);    p->bb_table    = NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* SM_seq_new — heap-allocate a standalone SM_sequence_t.  Used by tests
 * that exercise the SM stream without going through lower / stage2.        */
SM_sequence_t *SM_seq_new(void)
{
    SM_sequence_t *p = calloc(1, sizeof *p);
    sm_seq_init(p);
    return p;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void SM_seq_free(SM_sequence_t *p)
{
    if (!p) return;
    sm_seq_deinit(p);
    free(p);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* stage2_reset — clear the global stage2 to a clean state.  Called by
 * lower() at the top of every pass.  Frees the previous build's dynamic SM
 * arrays (idempotent: NULL-safe inside sm_seq_deinit) and re-initializes
 * them; zeros every sidecar count and the pl_pred_table buckets; resets
 * the module_registry main_mod sentinel.
 *
 * ST2-1c (2026-05-20): label_table / proc_table are now dynamically-grown.
 * On first call they're allocated; on subsequent calls the previous storage
 * is freed and a fresh array of initial capacity is allocated.  Initial caps
 * mirror the former hard limits (STAGE2_LABEL_MAX / STAGE2_PROC_TABLE_MAX)
 * as starting hints — programs that exceed them now grow rather than
 * silently truncating.                                                       */
void stage2_reset(void)
{
    sm_seq_deinit(&g_stage2.sm);
    sm_seq_init  (&g_stage2.sm);
    free(g_stage2.label_table); g_stage2.label_table = NULL;
    free(g_stage2.proc_table);  g_stage2.proc_table  = NULL;
    g_stage2.label_cap   = STAGE2_LABEL_MAX;
    g_stage2.label_count = 0;
    g_stage2.label_table = calloc((size_t)g_stage2.label_cap, sizeof(LabelEntry));
    g_stage2.proc_cap    = STAGE2_PROC_TABLE_MAX;
    g_stage2.proc_count  = 0;
    g_stage2.proc_table  = calloc((size_t)g_stage2.proc_cap,  sizeof(IcnProcEntry));
    memset(&g_stage2.pl_pred_table,   0, sizeof g_stage2.pl_pred_table);
    memset(&g_stage2.module_registry, 0, sizeof g_stage2.module_registry);
    g_stage2.module_registry.main_mod = -1;
    g_stage2.lang = 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* stage2_label_grow — reserve a fresh slot in the dynamic label_table.  If
 * count == cap, doubles cap and reallocates.  Returns the slot index; the
 * slot is zero-initialized.  Mirror of the `_grow` pattern in sm_prog.c.  */
int stage2_label_grow(stage2_t *s2)
{
    if (s2->label_count >= s2->label_cap) {
        s2->label_cap *= 2;
        s2->label_table = realloc(s2->label_table, (size_t)s2->label_cap * sizeof(LabelEntry));
    }
    int idx = s2->label_count++;
    memset(&s2->label_table[idx], 0, sizeof(LabelEntry));
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* stage2_proc_grow — reserve a fresh slot in the dynamic proc_table.  If
 * count == cap, doubles cap and reallocates.  Returns the slot index; the
 * slot is zero-initialized.                                                */
int stage2_proc_grow(stage2_t *s2)
{
    if (s2->proc_count >= s2->proc_cap) {
        s2->proc_cap *= 2;
        s2->proc_table = realloc(s2->proc_table, (size_t)s2->proc_cap * sizeof(IcnProcEntry));
    }
    int idx = s2->proc_count++;
    memset(&s2->proc_table[idx], 0, sizeof(IcnProcEntry));
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int _grow(SM_sequence_t *p)
{
    if (p->count >= p->cap) {
        p->cap *= 2;
        p->instrs = realloc(p->instrs, (size_t)p->cap * sizeof(SM_t));
    }
    int idx = p->count++;
    memset(&p->instrs[idx], 0, sizeof(SM_t));
    p->instrs[idx].op = (SM_op_t)0;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit(SM_sequence_t *p, SM_op_t op)
{
    int idx = _grow(p);
    p->instrs[idx].op = op;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_s(SM_sequence_t *p, SM_op_t op, const char *s)
{
    int idx = _grow(p);
    p->instrs[idx].op   = op;
    p->instrs[idx].a[0].s = s ? strdup(s) : NULL;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_i(SM_sequence_t *p, SM_op_t op, int64_t i)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].i  = i;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_ptr(SM_sequence_t *p, SM_op_t op, void *ptr)
{
    int idx = _grow(p);
    p->instrs[idx].op        = op;
    p->instrs[idx].a[0].ptr  = ptr;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_f(SM_sequence_t *p, SM_op_t op, double f)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].f  = f;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_si(SM_sequence_t *p, SM_op_t op, const char *s, int64_t i)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].s  = s ? strdup(s) : NULL;
    p->instrs[idx].a[1].i  = i;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_sip(SM_sequence_t *p, SM_op_t op, const char *s, int64_t i, void *ptr)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].s  = s ? strdup(s) : NULL;
    p->instrs[idx].a[1].i  = i;
    p->instrs[idx].a[2].ptr = ptr;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_ii(SM_sequence_t *p, SM_op_t op, int64_t i0, int64_t i1)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].i  = i0;
    p->instrs[idx].a[1].i  = i1;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_label(SM_sequence_t *p)
{
    int target = p->count;   /* index of *next* instruction after this label */
    int idx = _grow(p);
    p->instrs[idx].op      = SM_LABEL;
    p->instrs[idx].a[1].i  = (int64_t)target;
    return target;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_label_named(SM_sequence_t *p, const char *name)
{
    int target = p->count;
    int idx = _grow(p);
    p->instrs[idx].op     = SM_LABEL;
    p->instrs[idx].a[0].s = name ? strdup(name) : NULL;
    p->instrs[idx].a[1].i = (int64_t)target;
    return target;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_label_pc_lookup(const SM_sequence_t *p, const char *name)
{
    if (!p || !name) return -1;
    for (int i = 0; i < p->count; i++) {
        if (p->instrs[i].op == SM_LABEL && p->instrs[i].a[0].s &&
            strcmp(p->instrs[i].a[0].s, name) == 0)
            return (int)p->instrs[i].a[1].i;
    }
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void SM_patch_jump(SM_sequence_t *p, int jump_idx, int target_label)
{
    p->instrs[jump_idx].a[0].i = (int64_t)target_label;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void SM_stno_label_record(SM_sequence_t *p, int stno, const char *label)
{
    if (stno <= 0) return;
    if (stno >= p->stno_labels_cap) {
        int newcap = p->stno_labels_cap * 2;
        while (newcap <= stno) newcap *= 2;
        p->stno_labels = realloc(p->stno_labels, (size_t)newcap * sizeof(const char *));
        for (int i = p->stno_labels_cap; i < newcap; i++) p->stno_labels[i] = NULL;
        p->stno_labels_cap = newcap;
    }
    p->stno_labels[stno] = label;
    if (stno > p->stno_count) p->stno_count = stno;
}
static const char *opnames[SM_OPCODE_COUNT] = {
    "SM_LABEL","SM_JUMP","SM_JUMP_S","SM_JUMP_F","SM_HALT",
    "SM_STNO",
    "SM_PUSH_LIT_S","SM_PUSH_LIT_CS","SM_PUSH_LIT_I","SM_PUSH_LIT_F","SM_PUSH_NULL","SM_PUSH_NULL_NOFLIP",
    "SM_PUSH_VAR","SM_PUSH_EXPR","SM_PUSH_EXPRESSION","SM_CALL_EXPRESSION","SM_STORE_VAR","SM_VOID_POP",
    "SM_ADD","SM_SUB","SM_MUL","SM_DIV","SM_EXP","SM_MOD","SM_CONCAT","SM_COERCE_NUM","SM_NEG",
    "SM_PAT_LIT","SM_PAT_ANY","SM_PAT_NOTANY","SM_PAT_SPAN","SM_PAT_BREAK",
    "SM_PAT_LEN","SM_PAT_POS","SM_PAT_RPOS","SM_PAT_TAB","SM_PAT_RTAB",
    "SM_PAT_ARB","SM_PAT_ARBNO","SM_PAT_REM","SM_PAT_BAL","SM_PAT_FENCE0","SM_PAT_FENCE1","SM_PAT_ABORT",
    "SM_PAT_FAIL","SM_PAT_SUCCEED","SM_PAT_EPS","SM_PAT_ALT","SM_PAT_CAT",
    "SM_PAT_DEREF","SM_PAT_REFNAME","SM_PAT_CAPTURE",
    "SM_PAT_CAPTURE_FN",
    "SM_PAT_CAPTURE_FN_ARGS",
    "SM_PAT_USERCALL",
    "SM_PAT_USERCALL_ARGS",
    "SM_EXEC_STMT",
    "SM_BB_PUMP","SM_BB_ONCE","SM_BB_EVAL","SM_BB_ONCE_PROC","SM_BB_PUMP_PROC","SM_BB_PUMP_CASE","SM_BB_PUMP_SM","SM_BB_PUMP_EVERY","SM_SUSPEND_VALUE",
    "SM_CALL_FN","SM_RETURN","SM_FRETURN","SM_NRETURN",
    "SM_RETURN_S","SM_RETURN_F","SM_FRETURN_S","SM_FRETURN_F","SM_NRETURN_S","SM_NRETURN_F",
    "SM_DEFINE_ENTRY",
    "SM_DEFINE",
    "SM_INCR","SM_DECR","SM_LCOMP","SM_ACOMP",
    "SM_SUSPEND","SM_LOAD_GLOCAL","SM_STORE_GLOCAL","SM_ICMP_GT","SM_ICMP_LT",
    "SM_LOAD_FRAME","SM_STORE_FRAME",
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *sm_opcode_name(SM_op_t op)
{
    if (op >= 0 && op < SM_OPCODE_COUNT) return opnames[op];
    return "SM_UNKNOWN";
}
#include <stdio.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_seq_print(const SM_sequence_t *p, FILE *out)
{
    if (!p) { fprintf(out, "(null SM_sequence_t)\n"); return; }
    fprintf(out, "; SM_sequence_t  count=%d\n", p->count);
    for (int i = 0; i < p->count; i++) {
        const SM_t *in = &p->instrs[i];
        const char *name = sm_opcode_name(in->op);
        fprintf(out, "%4d  %-20s", i, name);
        switch (in->op) {
            case SM_PUSH_LIT_S:
            case SM_PUSH_LIT_CS:
            case SM_PAT_LIT:
            case SM_PAT_ANY: case SM_PAT_NOTANY:
            case SM_PAT_SPAN: case SM_PAT_BREAK:
                fprintf(out, " s=\"%s\"", in->a[0].s ? in->a[0].s : "");
                break;
            case SM_PUSH_LIT_I:
            case SM_PAT_LEN: case SM_PAT_POS: case SM_PAT_RPOS:
            case SM_PAT_TAB: case SM_PAT_RTAB:
            case SM_INCR: case SM_DECR:
            case SM_LCOMP:
            case SM_ACOMP:
                fprintf(out, " i=%lld", (long long)in->a[0].i);
                break;
            case SM_PUSH_LIT_F:
                fprintf(out, " f=%g", in->a[0].f);
                break;
            case SM_JUMP:
            case SM_JUMP_S:
            case SM_JUMP_F:
                fprintf(out, " -> %lld", (long long)in->a[0].i);
                break;
            case SM_PUSH_VAR: case SM_STORE_VAR:
            case SM_PAT_CAPTURE: case SM_PAT_DEREF:
            case SM_PAT_REFNAME:
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                break;
            case SM_PAT_CAPTURE_FN:
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                fprintf(out, " kind=%lld", (long long)in->a[1].i);
                if (in->a[2].s) fprintf(out, " args=\"%s\"", in->a[2].s);
                break;
            case SM_PAT_CAPTURE_FN_ARGS:
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                fprintf(out, " kind=%lld nargs=%lld",
                    (long long)in->a[1].i, (long long)in->a[2].i);
                break;
            case SM_PAT_USERCALL:
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                if (in->a[2].s) fprintf(out, " args=\"%s\"", in->a[2].s);
                break;
            case SM_PAT_USERCALL_ARGS:
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                fprintf(out, " nargs=%lld", (long long)in->a[1].i);
                break;
            case SM_CALL_FN:
                fprintf(out, " s=\"%s\" nargs=%lld",
                    in->a[0].s ? in->a[0].s : "?",
                    (long long)in->a[1].i);
                break;
            case SM_DEFINE:
                fprintf(out, " s=\"%s\"", in->a[0].s ? in->a[0].s : "?");
                break;
            case SM_EXEC_STMT:
                if (in->a[0].i) fprintf(out, " has_repl=%lld", (long long)in->a[1].i);
                if (in->a[0].s) fprintf(out, " subj=\"%s\"", in->a[0].s);
                break;
            case SM_STNO:
                fprintf(out, " stmt=%lld", (long long)in->a[0].i);
                if (in->a[1].i > 0)
                    fprintf(out, " line=%lld", (long long)in->a[1].i);
                break;
            case SM_LABEL:
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                if (in->a[2].i) fprintf(out, " define_entry=1");
                break;
            default:
                break;
        }
        fprintf(out, "\n");
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_seq_bb_add(SM_sequence_t *p, struct BB_graph_t *cfg) {
    if (!cfg) return -1;
    /* IR-CONSOLIDATE-DCG step 5 (2026-05-20): lazy-init for mode-4 standalone.  In scrip
     * the embedded g_stage2.sm is initialized by stage2_reset() at the top of every lower()
     * pass.  Standalone binaries link against the runtime but never call lower(), so
     * g_stage2.sm is left zero-initialized.  Detect that case and allocate. */
    if (p->bb_cap == 0) {
        p->bb_cap   = 16;
        p->bb_count = 0;
        p->bb_table = calloc((size_t)p->bb_cap, sizeof(BB_graph_t *));
    }
    if (p->bb_count >= p->bb_cap) {
        p->bb_cap *= 2;
        p->bb_table = realloc(p->bb_table, (size_t)p->bb_cap * sizeof(BB_graph_t *));
    }
    int idx = p->bb_count++;
    p->bb_table[idx] = cfg;
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int SM_emit_sii(SM_sequence_t *p, SM_op_t op, const char *s, int64_t i0, int64_t i1) {
    int idx = _grow(p);
    p->instrs[idx].op    = op;
    p->instrs[idx].a[0].s = s;
    p->instrs[idx].a[1].i = i0;
    p->instrs[idx].a[2].i = i1;
    return idx;
}
