/*
 * interp_label.c — label table and DEFINE prescan
 *
 * Split from interp.c by RS-3 (GOAL-REWRITE-SCRIP).
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * DATE:    2026-05-02
 */

#include "interp_private.h"

/* ══════════════════════════════════════════════════════════════════════════
 * label_table — map SNOBOL4 source labels → const tree_t*
 * ══════════════════════════════════════════════════════════════════════════ */



LabelEntry label_table[LABEL_MAX];
int label_count = 0;

void label_table_build(const tree_t *prog)
{
    label_count = 0;
    if (!prog) return;
    for (int i = 0; i < prog->n && label_count < LABEL_MAX; i++) {
        const tree_t *s = prog->c[i];
        if (!s || (s->t != TT_STMT && s->t != TT_END)) continue;
        const char *lbl = stmt_attr_str(stmt_attr_find(s, ":lbl"));
        if (lbl && *lbl) {
            label_table[label_count].name = strdup(lbl);
            label_table[label_count].stmt = s;
            label_count++;
        }
    }
}

const tree_t *label_lookup(const char *name)
{
    if (!name || !*name) return NULL;
    for (int i = 0; i < label_count; i++)
        if (strcmp(label_table[i].name, name) == 0)
            return label_table[i].stmt;
    return NULL;
}

/* ── Extract DEFINE spec string from TT_FNC("DEFINE",...) subject node ── */
const char *define_spec_from_expr(tree_t *subj)
{
    if (!subj || subj->t != TT_FNC) return NULL;
    if (!subj->v.sval || strcmp(subj->v.sval, "DEFINE") != 0) return NULL;  /* SN-19 */
    if (subj->n < 1 || !subj->c[0]) return NULL;
    tree_t *arg = subj->c[0];
    if (arg->t == TT_QLIT) return arg->v.sval;
    if (arg->t == TT_CAT || arg->t == TT_SEQ) {
        static char flatbuf[1024];
        size_t pos = 0;
        flatbuf[0] = '\0';
        for (int i = 0; i < arg->n && pos < sizeof(flatbuf)-1; i++) {
            tree_t *c = arg->c[i];
            if (c && c->t == TT_QLIT && c->v.sval) {
                size_t clen = strlen(c->v.sval);
                if (pos + clen >= sizeof(flatbuf)-1) break;
                memcpy(flatbuf + pos, c->v.sval, clen);
                pos += clen;
            }
        }
        flatbuf[pos] = '\0';
        return pos ? flatbuf : NULL;
    }
    return NULL;
}

/* ── Extract optional entry-label string from second arg of DEFINE ── */
const char *define_entry_from_expr(tree_t *subj)
{
    if (!subj || subj->t != TT_FNC) return NULL;
    if (!subj->v.sval || strcmp(subj->v.sval, "DEFINE") != 0) return NULL;  /* SN-19 */
    if (subj->n < 2 || !subj->c[1]) return NULL;
    tree_t *arg2 = subj->c[1];
    /* .label_name → TT_NAME(TT_VAR sval="label_name") or TT_CAPT_COND_ASGN */
    if (arg2->t == TT_NAME && arg2->n == 1) {
        tree_t *inner = arg2->c[0];
        if (inner->t == TT_VAR && inner->v.sval) return inner->v.sval;
    }
    if (arg2->t == TT_CAPT_COND_ASGN && arg2->n == 1) {
        tree_t *inner = arg2->c[0];
        if (inner->t == TT_VAR && inner->v.sval) return inner->v.sval;
    }
    if (arg2->t == TT_VAR && arg2->v.sval) return arg2->v.sval;
    if (arg2->t == TT_QLIT && arg2->v.sval) return arg2->v.sval;
    return NULL;
}

/* ── Pre-scan program and register all DEFINE'd functions ── */
void prescan_defines(const tree_t *prog)
{
    if (!prog) return;
    for (int i = 0; i < prog->n; i++) {
        const tree_t *s = prog->c[i];
        if (!s || s->t != TT_STMT) continue;
        tree_t *subj = stmt_attr_expr(stmt_attr_find(s, ":subj"));
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

/* SI-6: label_table_clear_stmts removed — GC owns tree_t nodes; no dangling
 * pointer hazard after TT_PROGRAM is freed. */
