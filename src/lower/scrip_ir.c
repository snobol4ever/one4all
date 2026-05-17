#include "IR.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static const char * kind_names[IR_E_COUNT] = {
    "IR_LIT_I", "IR_LIT_S", "IR_LIT_F", "IR_LIT_NUL",
    "IR_VAR", "IR_ASSIGN", "IR_AUGOP", "IR_BINOP", "IR_UNOP", "IR_CALL",
    "IR_SEQ", "IR_FAIL", "IR_SUCCEED", "IR_GOTO", "IR_RETURN", "IR_IF",
    "IR_ALTERNATE", "IR_TO_BY", "IR_EVERY", "IR_WHILE", "IR_UNTIL", "IR_LIMIT", "IR_SUSPEND", "IR_PROC",
    "IR_SCAN", "IR_NONNULL", "IR_INTERROGATE",
    "IR_PAT_LIT", "IR_PAT_ANY", "IR_PAT_SPAN", "IR_PAT_BREAK", "IR_PAT_ARB",
    "IR_PAT_ARBNO", "IR_PAT_CAT", "IR_PAT_ALT",
    "IR_PAT_ASSIGN_IMM", "IR_PAT_ASSIGN_COND",
    "IR_PAT_LEN", "IR_PAT_NOTANY",
    "IR_PAT_POS", "IR_PAT_TAB", "IR_PAT_REM", "IR_PAT_FENCE", "IR_PAT_ABORT", "IR_PAT_CALLOUT",
    "IR_PL_CHOICE", "IR_PL_UNIFY", "IR_PL_CUT", "IR_PL_CALL",
    [IR_SWAP] = "IR_SWAP",
    [IR_SEQ_EXPR] = "IR_SEQ_EXPR",
    [IR_INITIAL] = "IR_INITIAL",
    [IR_ICN_LCONCAT] = "IR_ICN_LCONCAT"
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char * IR_e_name(IR_e k) {
    if (k >= 0 && k < IR_E_COUNT) return kind_names[k];
    return "IR_UNKNOWN";
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t * IR_alloc(int max_nodes, int lang) {
    IR_block_t * cfg = calloc(1, sizeof(IR_block_t));
    if (!cfg) return NULL;
    cfg->all  = calloc((size_t)max_nodes, sizeof(IR_t *));
    if (!cfg->all) { free(cfg); return NULL; }
    cfg->n    = 0;
    cfg->max  = max_nodes;
    cfg->lang = lang;
    cfg->entry = NULL;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_t * IR_node_alloc(IR_block_t * cfg, IR_e t) {
    IR_t * nd = calloc(1, sizeof(IR_t));
    if (!nd) return NULL;
    nd->t       = t;
    nd->α       = nd;
    nd->β       = nd;
    nd->γ       = NULL;
    nd->ω       = NULL;
    nd->c       = NULL;
    nd->n       = 0;
    nd->value   = FAILDESCR;
    nd->counter = 0;
    nd->state   = 0;
    if (cfg->n >= cfg->max) { free(nd); return NULL; }
    cfg->all[cfg->n++] = nd;
    return nd;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void IR_reset(IR_block_t * cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->n; i++) {
        IR_t * nd = cfg->all[i];
        if (!nd) continue;
        nd->value   = FAILDESCR;
        nd->counter = 0;
        nd->state   = 0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Snapshot the mutable per-node state (value, counter, state) of every node in cfg. Used by IR_CALL to preserve the caller's activation across a recursive call into the same ir_body, since IR_reset+IR_exec_once on the callee wipes shared graph state. Caller frees with free(). */
IR_node_state_t * IR_snapshot_state(IR_block_t * cfg) {
    if (!cfg || cfg->n <= 0) return NULL;
    IR_node_state_t * snap = (IR_node_state_t *)malloc((size_t)cfg->n * sizeof(IR_node_state_t));
    if (!snap) return NULL;
    for (int i = 0; i < cfg->n; i++) {
        IR_t * nd = cfg->all[i];
        if (!nd) { snap[i].value = FAILDESCR; snap[i].counter = 0; snap[i].state = 0; continue; }
        snap[i].value   = nd->value;
        snap[i].counter = nd->counter;
        snap[i].state   = nd->state;
    }
    return snap;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Restore the per-node state snapshot taken by IR_snapshot_state and free the snapshot buffer. Safe to call with snap==NULL (no-op). */
void IR_restore_state(IR_block_t * cfg, IR_node_state_t * snap) {
    if (!cfg || !snap) { free(snap); return; }
    for (int i = 0; i < cfg->n; i++) {
        IR_t * nd = cfg->all[i];
        if (!nd) continue;
        nd->value   = snap[i].value;
        nd->counter = snap[i].counter;
        nd->state   = snap[i].state;
    }
    free(snap);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void IR_free(IR_block_t * cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->n; i++) {
        IR_t * nd = cfg->all[i];
        if (!nd) continue;
        free(nd->c);
        free(nd);
    }
    free(cfg->all);
    free(cfg);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_port(FILE * fp, const char * label, const IR_t * nd) {
    fprintf(fp, " %s=%s", label, nd ? "set" : "NULL");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void IR_print(const IR_block_t * cfg, FILE * fp) {
    if (!cfg) { fprintf(fp, "(null IR_block_t)\n"); return; }
    static const char * lang_names[] = { "?", "SNO", "SCO", "REB", "ICN", "PL", "RKU" };
    const char * lname = (cfg->lang >= 1 && cfg->lang <= 6) ? lang_names[cfg->lang] : "?";
    fprintf(fp, "IR_block_t lang=%s n=%d entry=%s\n", lname, cfg->n, cfg->entry ? "set" : "NULL");
    for (int i = 0; i < cfg->n; i++) {
        const IR_t * nd = cfg->all[i];
        if (!nd) continue;
        fprintf(fp, "  [%d] %s", i, IR_e_name(nd->t));
        print_port(fp, "α", nd->α);
        print_port(fp, "β", nd->β);
        print_port(fp, "γ", nd->γ);
        print_port(fp, "ω", nd->ω);
        if (nd->n > 0) {
            fprintf(fp, " children=[");
            for (int j = 0; j < nd->n; j++) fprintf(fp, "%s%d", j ? "," : "", nd->c[j] ? j : -1);
            fprintf(fp, "]");
        }
        switch (nd->t) {
            case IR_LIT_I: fprintf(fp, " ival=%lld", (long long)nd->ival); break;
            case IR_LIT_F: fprintf(fp, " dval=%g",   nd->dval);             break;
            case IR_LIT_S: fprintf(fp, " sval=\"%s\"", nd->sval ? nd->sval : ""); break;
            case IR_VAR:   fprintf(fp, " var=\"%s\"",  nd->sval ? nd->sval : ""); break;
            default: break;
        }
        fprintf(fp, "\n");
    }
}
