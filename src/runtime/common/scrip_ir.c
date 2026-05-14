/*
 * scrip_ir.c — Universal generator IR: IR_prog_t / IR_prog_t alloc/free/reset/print (LR-0)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-0, 2026-05-14)
 */
#include "scrip_ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/*------------------------------------------------------------------------------------------------------------------------------------*/
static const char * kind_names[IR_E_COUNT] = {
    "IR_LIT_I", "IR_LIT_S", "IR_LIT_F", "IR_LIT_NUL",
    "IR_VAR", "IR_ASSIGN", "IR_AUGOP", "IR_BINOP", "IR_UNOP", "IR_CALL",
    "IR_SEQ", "IR_FAIL", "IR_SUCCEED", "IR_GOTO", "IR_RETURN",
    "IR_ALTERNATE", "IR_TO_BY", "IR_EVERY", "IR_WHILE", "IR_LIMIT", "IR_SUSPEND", "IR_PROC",
    "IR_SCAN", "IR_NONNULL", "IR_INTERROGATE",
    "IR_PAT_LIT", "IR_PAT_ANY", "IR_PAT_SPAN", "IR_PAT_BREAK", "IR_PAT_ARB",
    "IR_PAT_ARBNO", "IR_PAT_CAT", "IR_PAT_ALT",
    "IR_PAT_ASSIGN_IMM", "IR_PAT_ASSIGN_COND",
    "IR_PAT_POS", "IR_PAT_TAB", "IR_PAT_REM", "IR_PAT_FENCE", "IR_PAT_ABORT", "IR_PAT_CALLOUT",
    "IR_PL_CHOICE", "IR_PL_UNIFY", "IR_PL_CUT", "IR_PL_CALL"
};
/*------------------------------------------------------------------------------------------------------------------------------------*/
const char * IR_e_name(IR_e k) {
    if (k >= 0 && k < IR_E_COUNT) return kind_names[k];
    return "IR_UNKNOWN";
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
IR_prog_t * IR_alloc(int max_nodes, int lang) {
    IR_prog_t * cfg = calloc(1, sizeof(IR_prog_t));
    if (!cfg) return NULL;
    cfg->all  = calloc((size_t)max_nodes, sizeof(IR_t *));
    if (!cfg->all) { free(cfg); return NULL; }
    cfg->n    = 0;
    cfg->lang = lang;
    cfg->entry = NULL;
    return cfg;
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
IR_t * IR_node_alloc(IR_prog_t * cfg, IR_e kind, int lang) {
    IR_t * nd = calloc(1, sizeof(IR_t));
    if (!nd) return NULL;
    nd->kind        = kind;
    nd->lang        = lang;
    nd->id          = cfg->n;
    nd->port_start  = NULL;
    nd->port_resume = NULL;
    nd->port_succ   = NULL;
    nd->port_fail   = NULL;
    nd->c           = NULL;
    nd->n           = 0;
    nd->value       = FAILDESCR;
    nd->counter     = 0;
    nd->state       = 0;
    nd->generative  = 0;
    nd->visited     = 0;
    cfg->all[cfg->n++] = nd;
    return nd;
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void IR_reset(IR_prog_t * cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->n; i++) {
        IR_t * nd = cfg->all[i];
        if (!nd) continue;
        nd->value   = FAILDESCR;
        nd->counter = 0;
        nd->state   = 0;
        nd->visited = 0;
    }
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void IR_free(IR_prog_t * cfg) {
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
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* Helper — print one port as "name=id" or "name=NULL". */
static void print_port(FILE * fp, const char * label, const IR_t * nd) {
    if (nd) fprintf(fp, " %s=%d", label, nd->id);
    else    fprintf(fp, " %s=NULL", label);
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
void IR_print(const IR_prog_t * cfg, FILE * fp) {
    if (!cfg) { fprintf(fp, "(null IR_prog_t)\n"); return; }
    static const char * lang_names[] = { "?", "SNO", "SCO", "REB", "ICN", "PL", "RKU" };
    const char * lname = (cfg->lang >= 1 && cfg->lang <= 6) ? lang_names[cfg->lang] : "?";
    fprintf(fp, "IR_prog_t lang=%s n=%d entry=%s\n", lname, cfg->n, cfg->entry ? "set" : "NULL");
    for (int i = 0; i < cfg->n; i++) {
        const IR_t * nd = cfg->all[i];
        if (!nd) continue;
        fprintf(fp, "  [%d] %s gen=%d", nd->id, IR_e_name(nd->kind), nd->generative);
        print_port(fp, "start",  nd->port_start);
        print_port(fp, "resume", nd->port_resume);
        print_port(fp, "succ",   nd->port_succ);
        print_port(fp, "fail",   nd->port_fail);
        if (nd->n > 0) {
            fprintf(fp, " children=[");
            for (int j = 0; j < nd->n; j++) fprintf(fp, "%s%d", j ? "," : "", nd->c[j] ? nd->c[j]->id : -1);
            fprintf(fp, "]");
        }
        switch (nd->kind) {
            case IR_LIT_I:  fprintf(fp, " ival=%lld", (long long)nd->ival); break;
            case IR_LIT_F:  fprintf(fp, " dval=%g",  nd->dval);             break;
            case IR_LIT_S:  fprintf(fp, " sval=\"%s\"", nd->sval ? nd->sval : ""); break;
            case IR_VAR:    fprintf(fp, " var=\"%s\"",  nd->sval ? nd->sval : ""); break;
            case IR_CALL:   fprintf(fp, " call=\"%s\" nargs=%d", nd->call.name ? nd->call.name : "", nd->call.nargs); break;
            default: break;
        }
        fprintf(fp, "\n");
    }
}
