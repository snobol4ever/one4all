/*
 * interp_label.c — label table and DEFINE prescan
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

/* ══════════════════════════════════════════════════════════════════════════
 * label_table — map SNOBOL4 source labels → STMT_t*
 * ══════════════════════════════════════════════════════════════════════════ */



LabelEntry label_table[LABEL_MAX];
int label_count = 0;

void label_table_build(CODE_t *prog)
{
    label_count = 0;
    for (STMT_t *s = prog->head; s; s = s->next) {
        if (s->label && *s->label && label_count < LABEL_MAX) {
            /* RS-9b: strdup so label_table survives code_free(prog).
             * s->stmt is kept for --ir-run only; --sm-run uses sm_label_pc_lookup. */
            label_table[label_count].name = strdup(s->label);
            label_table[label_count].stmt = s;
            label_count++;
        }
    }
}

STMT_t *label_lookup(const char *name)
{
    if (!name || !*name) return NULL;
    for (int i = 0; i < label_count; i++)
        if (strcmp(label_table[i].name, name) == 0)
            return label_table[i].stmt;
    return NULL;
}

/* ── Extract DEFINE spec string from AST_FNC("DEFINE",...) subject node ── */
const char *define_spec_from_expr(AST_t *subj)
{
    if (!subj || subj->kind != AST_FNC) return NULL;
    if (!subj->sval || strcmp(subj->sval, "DEFINE") != 0) return NULL;  /* SN-19 */
    if (subj->nchildren < 1 || !subj->children[0]) return NULL;
    AST_t *arg = subj->children[0];
    if (arg->kind == AST_QLIT) return arg->sval;
    if (arg->kind == AST_CAT || arg->kind == AST_SEQ) {
        static char flatbuf[1024];
        size_t pos = 0;
        flatbuf[0] = '\0';
        for (int i = 0; i < arg->nchildren && pos < sizeof(flatbuf)-1; i++) {
            AST_t *c = arg->children[i];
            if (c && c->kind == AST_QLIT && c->sval) {
                size_t clen = strlen(c->sval);
                if (pos + clen >= sizeof(flatbuf)-1) break;
                memcpy(flatbuf + pos, c->sval, clen);
                pos += clen;
            }
        }
        flatbuf[pos] = '\0';
        return pos ? flatbuf : NULL;
    }
    return NULL;
}

/* ── Extract optional entry-label string from second arg of DEFINE ── */
const char *define_entry_from_expr(AST_t *subj)
{
    if (!subj || subj->kind != AST_FNC) return NULL;
    if (!subj->sval || strcmp(subj->sval, "DEFINE") != 0) return NULL;  /* SN-19 */
    if (subj->nchildren < 2 || !subj->children[1]) return NULL;
    AST_t *arg2 = subj->children[1];
    /* .label_name → AST_NAME(AST_VAR sval="label_name") or AST_CAPT_COND_ASGN */
    if (arg2->kind == AST_NAME && arg2->nchildren == 1) {
        AST_t *inner = arg2->children[0];
        if (inner->kind == AST_VAR && inner->sval) return inner->sval;
    }
    if (arg2->kind == AST_CAPT_COND_ASGN && arg2->nchildren == 1) {
        AST_t *inner = arg2->children[0];
        if (inner->kind == AST_VAR && inner->sval) return inner->sval;
    }
    if (arg2->kind == AST_VAR && arg2->sval) return arg2->sval;
    if (arg2->kind == AST_QLIT && arg2->sval) return arg2->sval;
    return NULL;
}

/* ── Pre-scan program and register all DEFINE'd functions ── */
void prescan_defines(CODE_t *prog)
{
    for (STMT_t *s = prog->head; s; s = s->next) {
        if (!s->subject) continue;
        const char *spec = define_spec_from_expr(s->subject);
        if (spec && *spec) {
            /* RS-9b: strdup so the func registry survives code_free(prog).
             * define_spec_from_expr may return arg->sval (direct IR pointer)
             * or flatbuf (static buffer) — strdup is correct in both cases. */
            char *spec_copy = strdup(spec);
            const char *entry = define_entry_from_expr(s->subject);
            if (entry) DEFINE_fn_entry(spec_copy, NULL, strdup(entry));
            else       DEFINE_fn(spec_copy, NULL);
        }
    }
}

/* RS-9b: null out STMT_t* pointers in label_table after code_free so
 * any residual label_lookup calls return NULL rather than a dangling ptr. */
void label_table_clear_stmts(void)
{
    for (int i = 0; i < label_count; i++)
        label_table[i].stmt = NULL;
}
