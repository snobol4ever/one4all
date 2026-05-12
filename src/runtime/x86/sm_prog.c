/*
 * sm_prog.c — SM_Program builder (M-SCRIP-U2)
 */

#include "sm_prog.h"
#include <string.h>
#include <stdlib.h>

/* RS-9b: set by scrip.c after sm_lower; allows _usercall_hook to detect SM bodies */
SM_Program *g_current_sm_prog = NULL;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SM_Program *sm_prog_new(void)
{
    SM_Program *p = calloc(1, sizeof *p);
    p->cap    = 64;
    p->instrs = calloc((size_t)p->cap, sizeof(SM_Instr));
    /* IM-9: stno_labels[0] unused; pre-allocate 64 statement slots */
    p->stno_labels_cap = 64;
    p->stno_labels     = calloc((size_t)p->stno_labels_cap, sizeof(const char *));
    p->stno_count      = 0;
    return p;
}

void sm_prog_free(SM_Program *p)
{
    if (!p) return;
    free(p->instrs);
    free(p->stno_labels);
    free(p);
}

static int _grow(SM_Program *p)
{
    if (p->count >= p->cap) {
        p->cap *= 2;
        p->instrs = realloc(p->instrs, (size_t)p->cap * sizeof(SM_Instr));
    }
    int idx = p->count++;
    memset(&p->instrs[idx], 0, sizeof(SM_Instr));
    p->instrs[idx].op = (sm_opcode_t)0;
    return idx;
}

int sm_emit(SM_Program *p, sm_opcode_t op)
{
    int idx = _grow(p);
    p->instrs[idx].op = op;
    return idx;
}

int sm_emit_s(SM_Program *p, sm_opcode_t op, const char *s)
{
    int idx = _grow(p);
    p->instrs[idx].op   = op;
    p->instrs[idx].a[0].s = s ? strdup(s) : NULL;
    return idx;
}

int sm_emit_i(SM_Program *p, sm_opcode_t op, int64_t i)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].i  = i;
    return idx;
}

int sm_emit_ptr(SM_Program *p, sm_opcode_t op, void *ptr)
{
    int idx = _grow(p);
    p->instrs[idx].op        = op;
    p->instrs[idx].a[0].ptr  = ptr;
    return idx;
}


int sm_emit_f(SM_Program *p, sm_opcode_t op, double f)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].f  = f;
    return idx;
}

int sm_emit_si(SM_Program *p, sm_opcode_t op, const char *s, int64_t i)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].s  = s ? strdup(s) : NULL;
    p->instrs[idx].a[1].i  = i;
    return idx;
}

int sm_emit_ii(SM_Program *p, sm_opcode_t op, int64_t i0, int64_t i1)
{
    int idx = _grow(p);
    p->instrs[idx].op      = op;
    p->instrs[idx].a[0].i  = i0;
    p->instrs[idx].a[1].i  = i1;
    return idx;
}

int sm_label(SM_Program *p)
{
    int target = p->count;   /* index of *next* instruction after this label */
    int idx = _grow(p);
    p->instrs[idx].op      = SM_LABEL;
    /* CHUNKS-step03: store PC in a[1].i (matches sm_label_named layout) and
     * leave a[0].s == NULL.  Was: a[0].i = target — but that aliases a[0].s
     * to a small-integer pointer which sm_label_pc_lookup then strcmp'd,
     * segfaulting on expression-body unnamed labels emitted by TT_DEFER lowering. */
    p->instrs[idx].a[1].i  = (int64_t)target;
    return target;
}

/* RS-9a: named label — same as sm_label but also stores name in a[0].s */
int sm_label_named(SM_Program *p, const char *name)
{
    int target = p->count;
    int idx = _grow(p);
    p->instrs[idx].op     = SM_LABEL;
    p->instrs[idx].a[0].s = name ? strdup(name) : NULL;
    p->instrs[idx].a[1].i = (int64_t)target;  /* PC stored in a[1] — a[0] holds name */
    return target;
}

/* RS-9a: scan SM_Program for SM_LABEL with matching a[0].s; return PC or -1 */
int sm_label_pc_lookup(const SM_Program *p, const char *name)
{
    if (!p || !name) return -1;
    for (int i = 0; i < p->count; i++) {
        if (p->instrs[i].op == SM_LABEL && p->instrs[i].a[0].s &&
            strcmp(p->instrs[i].a[0].s, name) == 0)
            return (int)p->instrs[i].a[1].i;  /* PC stored in a[1] */
    }
    return -1;
}

void sm_patch_jump(SM_Program *p, int jump_idx, int target_label)
{
    p->instrs[jump_idx].a[0].i = (int64_t)target_label;
}

/* IM-9: record source label for statement stno (1-based).
 * label may be NULL (unlabelled statement). String is not copied — caller
 * owns the lifetime (interned in TT_STMT :lbl which lives for the program). */
void sm_stno_label_record(SM_Program *p, int stno, const char *label)
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
    "SM_PUSH_LIT_S","SM_PUSH_LIT_I","SM_PUSH_LIT_F","SM_PUSH_NULL","SM_PUSH_NULL_NOFLIP",
    "SM_PUSH_VAR","SM_PUSH_EXPR","SM_PUSH_EXPRESSION","SM_CALL_EXPRESSION","SM_STORE_VAR","SM_VOID_POP",
    "SM_ADD","SM_SUB","SM_MUL","SM_DIV","SM_EXP","SM_MOD","SM_CONCAT","SM_COERCE_NUM","SM_NEG",
    "SM_PAT_LIT","SM_PAT_ANY","SM_PAT_NOTANY","SM_PAT_SPAN","SM_PAT_BREAK",
    "SM_PAT_LEN","SM_PAT_POS","SM_PAT_RPOS","SM_PAT_TAB","SM_PAT_RTAB",
    "SM_PAT_ARB","SM_PAT_ARBNO","SM_PAT_REM","SM_PAT_BAL","SM_PAT_FENCE","SM_PAT_FENCE1","SM_PAT_ABORT",
    "SM_PAT_FAIL","SM_PAT_SUCCEED","SM_PAT_EPS","SM_PAT_ALT","SM_PAT_CAT",
    "SM_PAT_DEREF","SM_PAT_REFNAME","SM_PAT_CAPTURE",
    "SM_PAT_CAPTURE_FN",
    "SM_PAT_CAPTURE_FN_ARGS",
    "SM_PAT_USERCALL",
    "SM_PAT_USERCALL_ARGS",
    "SM_EXEC_STMT",
    "SM_BB_PUMP","SM_BB_ONCE","SM_BB_ONCE_PROC","SM_BB_PUMP_PROC","SM_BB_PUMP_CASE","SM_BB_PUMP_SM","SM_BB_PUMP_EVERY","SM_BB_PUMP_AST","SM_GEN_TICK","SM_SUSPEND_VALUE",
    "SM_CALL_FN","SM_RETURN","SM_FRETURN","SM_NRETURN",
    "SM_RETURN_S","SM_RETURN_F","SM_FRETURN_S","SM_FRETURN_F","SM_NRETURN_S","SM_NRETURN_F",
    "SM_DEFINE_ENTRY",
    "SM_DEFINE",
    "SM_INCR","SM_DECR","SM_LCOMP","SM_ACOMP",
    /* SM_PAT_BOXVAL removed by ME-1 */
    "SM_SUSPEND","SM_RESUME","SM_LOAD_GLOCAL","SM_STORE_GLOCAL","SM_ICMP_GT","SM_ICMP_LT",
    "SM_LOAD_FRAME","SM_STORE_FRAME",
};

const char *sm_opcode_name(sm_opcode_t op)
{
    if (op >= 0 && op < SM_OPCODE_COUNT) return opnames[op];
    return "SM_UNKNOWN";
}

/* ── sm_prog_print — --dump-sm diagnostic ──────────────────────────────── */
#include <stdio.h>

void sm_prog_print(const SM_Program *p, FILE *out)
{
    if (!p) { fprintf(out, "(null SM_Program)\n"); return; }
    fprintf(out, "; SM_Program  count=%d\n", p->count);
    for (int i = 0; i < p->count; i++) {
        const SM_Instr *in = &p->instrs[i];
        const char *name = sm_opcode_name(in->op);
        fprintf(out, "%4d  %-20s", i, name);
        /* Print operands heuristically based on opcode */
        switch (in->op) {
            /* string operands */
            case SM_PUSH_LIT_S:
            case SM_PAT_LIT:
            case SM_PAT_ANY: case SM_PAT_NOTANY:
            case SM_PAT_SPAN: case SM_PAT_BREAK:
                fprintf(out, " s=\"%s\"", in->a[0].s ? in->a[0].s : "");
                break;
            /* int operand */
            case SM_PUSH_LIT_I:
            case SM_PAT_LEN: case SM_PAT_POS: case SM_PAT_RPOS:
            case SM_PAT_TAB: case SM_PAT_RTAB:
            case SM_INCR: case SM_DECR:
            case SM_LCOMP:
            case SM_ACOMP:  /* CH-17g-runtime-bridge-acomp: a[0].i = operator EKind */
                fprintf(out, " i=%lld", (long long)in->a[0].i);
                break;
            /* float operand */
            case SM_PUSH_LIT_F:
                fprintf(out, " f=%g", in->a[0].f);
                break;
            /* jump targets */
            case SM_JUMP:
            case SM_JUMP_S:
            case SM_JUMP_F:
                fprintf(out, " -> %lld", (long long)in->a[0].i);
                break;
            /* string + int */
            case SM_PUSH_VAR: case SM_STORE_VAR:
            case SM_PAT_CAPTURE: case SM_PAT_DEREF:
            case SM_PAT_REFNAME:
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                break;
            case SM_PAT_CAPTURE_FN:
                /* TL-2: a[0].s = fname, a[1].i = kind (0=cond,1=imm),
                 *       a[2].s = optional '\t'-separated arg-var names */
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                fprintf(out, " kind=%lld", (long long)in->a[1].i);
                if (in->a[2].s) fprintf(out, " args=\"%s\"", in->a[2].s);
                break;
            case SM_PAT_CAPTURE_FN_ARGS:
                /* SN-8a: a[0].s = fname, a[1].i = kind, a[2].i = nargs (values on stack) */
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                fprintf(out, " kind=%lld nargs=%lld",
                    (long long)in->a[1].i, (long long)in->a[2].i);
                break;
            case SM_PAT_USERCALL:
                /* SN-17a: a[0].s = fname, a[2].s = optional '\t'-separated arg-var names */
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                if (in->a[2].s) fprintf(out, " args=\"%s\"", in->a[2].s);
                break;
            case SM_PAT_USERCALL_ARGS:
                /* SN-8a: a[0].s = fname, a[1].i = nargs (values on stack) */
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
                /* ME-7: a[0].s = optional label name; a[1].i = PC;
                 * a[2].i = 1 if this label is a DEFINE'd-function entry point. */
                if (in->a[0].s) fprintf(out, " s=\"%s\"", in->a[0].s);
                if (in->a[2].i) fprintf(out, " define_entry=1");
                break;
            case SM_GEN_TICK:
                fprintf(out, " entry_pc=%lld slot=%lld", (long long)in->a[0].i, (long long)in->a[1].i);
                break;
            default:
                break;
        }
        fprintf(out, "\n");
    }
}
