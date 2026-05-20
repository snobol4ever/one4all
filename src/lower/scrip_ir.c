#include "BB.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static const char * kind_names[IR_E_COUNT] = {
    "BB_LIT_I", "BB_LIT_S", "BB_LIT_F", "BB_LIT_NUL",
    "BB_VAR", "BB_ASSIGN", "BB_AUGOP", "BB_BINOP", "BB_UNOP", "BB_CALL",
    "BB_SEQ", "BB_FAIL", "BB_SUCCEED", "BB_GOTO", "BB_RETURN", "BB_IF",
    "BB_ALTERNATE", "BB_TO_BY", "BB_EVERY", "BB_WHILE", "BB_UNTIL", "BB_LIMIT", "BB_SUSPEND", "BB_PROC",
    "BB_SCAN", "BB_NONNULL", "BB_INTERROGATE",
    "BB_PAT_LIT", "BB_PAT_ANY", "BB_PAT_SPAN", "BB_PAT_BREAK", "BB_PAT_ARB",
    "BB_PAT_ARBNO", "BB_PAT_CAT", "BB_PAT_ALT",
    "BB_PAT_ASSIGN_IMM", "BB_PAT_ASSIGN_COND",
    "BB_PAT_LEN", "BB_PAT_NOTANY",
    "BB_PAT_POS", "BB_PAT_TAB", "BB_PAT_REM", "BB_PAT_FENCE", "BB_PAT_ABORT", "BB_PAT_CALLOUT",
    "BB_PL_CHOICE", "BB_PL_UNIFY", "BB_PL_CUT", "BB_PL_CALL",
    [BB_SWAP] = "BB_SWAP",
    [BB_SEQ_EXPR] = "BB_SEQ_EXPR",
    [BB_INITIAL] = "BB_INITIAL",
    [BB_ICN_LCONCAT] = "BB_ICN_LCONCAT",
    [BB_ICN_FIND_GEN] = "BB_ICN_FIND_GEN",
    [BB_ICN_SEQ_GEN] = "BB_ICN_SEQ_GEN"
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char * IR_e_name(BB_op_t k) {
    if (k >= 0 && k < IR_E_COUNT) return kind_names[k];
    return "BB_UNKNOWN";
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
BB_graph_t * BB_alloc(int max_nodes, int lang) {
    BB_graph_t * cfg = calloc(1, sizeof(BB_graph_t));
    if (!cfg) return NULL;
    cfg->all  = calloc((size_t)max_nodes, sizeof(BB_t *));
    if (!cfg->all) { free(cfg); return NULL; }
    cfg->n    = 0;
    cfg->max  = max_nodes;
    cfg->lang = lang;
    cfg->entry = NULL;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
BB_t * BB_node_alloc(BB_graph_t * cfg, BB_op_t t) {
    BB_t * nd = calloc(1, sizeof(BB_t));
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
void BB_reset(BB_graph_t * cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->n; i++) {
        BB_t * nd = cfg->all[i];
        if (!nd) continue;
        nd->value   = FAILDESCR;
        nd->counter = 0;
        nd->state   = 0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Snapshot the mutable per-node state (value, counter, state) of every node in cfg. Used by BB_CALL to preserve the caller's activation across a recursive call into the same ir_body, since BB_reset+bb_exec_once on the callee wipes shared graph state. Caller frees with free(). */
IR_node_state_t * IR_snapshot_state(BB_graph_t * cfg) {
    if (!cfg || cfg->n <= 0) return NULL;
    IR_node_state_t * snap = (IR_node_state_t *)malloc((size_t)cfg->n * sizeof(IR_node_state_t));
    if (!snap) return NULL;
    for (int i = 0; i < cfg->n; i++) {
        BB_t * nd = cfg->all[i];
        if (!nd) { snap[i].value = FAILDESCR; snap[i].counter = 0; snap[i].state = 0; continue; }
        snap[i].value   = nd->value;
        snap[i].counter = nd->counter;
        snap[i].state   = nd->state;
    }
    return snap;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Restore the per-node state snapshot taken by IR_snapshot_state and free the snapshot buffer. Safe to call with snap==NULL (no-op). */
void IR_restore_state(BB_graph_t * cfg, IR_node_state_t * snap) {
    if (!cfg || !snap) { free(snap); return; }
    for (int i = 0; i < cfg->n; i++) {
        BB_t * nd = cfg->all[i];
        if (!nd) continue;
        nd->value   = snap[i].value;
        nd->counter = snap[i].counter;
        nd->state   = snap[i].state;
    }
    free(snap);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void BB_free(BB_graph_t * cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->n; i++) {
        BB_t * nd = cfg->all[i];
        if (!nd) continue;
        free(nd->c);
        free(nd);
    }
    free(cfg->all);
    free(cfg);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_port(FILE * fp, const char * label, const BB_t * nd) {
    fprintf(fp, " %s=%s", label, nd ? "set" : "NULL");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_print(const BB_graph_t * cfg, FILE * fp) {
    if (!cfg) { fprintf(fp, "(null BB_graph_t)\n"); return; }
    static const char * lang_names[] = { "?", "SNO", "SCO", "REB", "ICN", "PL", "RKU" };
    const char * lname = (cfg->lang >= 1 && cfg->lang <= 6) ? lang_names[cfg->lang] : "?";
    fprintf(fp, "BB_graph_t lang=%s n=%d entry=%s\n", lname, cfg->n, cfg->entry ? "set" : "NULL");
    for (int i = 0; i < cfg->n; i++) {
        const BB_t * nd = cfg->all[i];
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
            case BB_LIT_I: fprintf(fp, " ival=%lld", (long long)nd->ival); break;
            case BB_LIT_F: fprintf(fp, " dval=%g",   nd->dval);             break;
            case BB_LIT_S: fprintf(fp, " sval=\"%s\"", nd->sval ? nd->sval : ""); break;
            case BB_VAR:   fprintf(fp, " var=\"%s\"",  nd->sval ? nd->sval : ""); break;
            default: break;
        }
        fprintf(fp, "\n");
    }
}
