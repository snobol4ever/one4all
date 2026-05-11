/*
 * interp_label.c — label table and DEFINE prescan
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

/* ══════════════════════════════════════════════════════════════════════════
 * label_table — map SNOBOL4 source labels → const AST_t*
 * ══════════════════════════════════════════════════════════════════════════ */



LabelEntry label_table[LABEL_MAX];
int label_count = 0;

void label_table_build(const AST_t *prog)
{
    label_count = 0;
    if (!prog) return;
    for (int i = 0; i < prog->nchildren && label_count < LABEL_MAX; i++) {
        const AST_t *s = prog->children[i];
        if (!s || (s->kind != AST_STMT && s->kind != AST_END)) continue;
        const char *lbl = stmt_attr_str(stmt_attr_find(s, ":lbl"));
        if (lbl && *lbl) {
            label_table[label_count].name = strdup(lbl);
            label_table[label_count].stmt = s;
            label_count++;
        }
    }
}

const AST_t *label_lookup(const char *name)
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
void prescan_defines(const AST_t *prog)
{
    if (!prog) return;
    for (int i = 0; i < prog->nchildren; i++) {
        const AST_t *s = prog->children[i];
        if (!s || s->kind != AST_STMT) continue;
        AST_t *subj = stmt_attr_expr(stmt_attr_find(s, ":subj"));
        if (!subj) continue;
        const char *spec = define_spec_from_expr(subj);
        if (spec && *spec) {
            char *spec_copy = strdup(spec);
            const char *entry = define_entry_from_expr(subj);
            if (entry) DEFINE_fn_entry(spec_copy, NULL, strdup(entry));
            else       DEFINE_fn(spec_copy, NULL);
        }
    }
}

/* SI-6: label_table_clear_stmts removed — GC owns AST_t nodes; no dangling
 * pointer hazard after AST_PROGRAM is freed. */
