/*
 * stmt_ast.c — SI-2/SI-3 shim: CODE_t/STMT_t → pure AST tree
 *
 * Produces AST_PROGRAM / AST_STMT / AST_END trees whose shape matches
 * the Snocone `tree` datatype: four logical fields t/v/n/c (kind, value,
 * nchildren, children[]).  No side-channel fields (no a[], no flags).
 *
 * AST_STMT uses tagged attribute children matching parser_snobol4.sc output:
 *   tree(':lbl',  label)   — label (omitted if empty)
 *   tree(':lang', langstr) — lang (omitted if LANG_SNO=0)
 *   tree(':line', linestr) — source lineno
 *   tree(':stno', stnostr) — source stno
 *   tree(':subj', expr)    — subject (omitted if absent)
 *   tree(':pat',  expr)    — pattern (omitted if absent)
 *   tree(':eq',   '')      — presence signals has_eq=true
 *   tree(':repl', expr)    — replacement (omitted if !has_eq)
 *   tree(':goS',  label)   — success goto (omitted if absent)
 *   tree(':goF',  label)   — failure goto (omitted if absent)
 *   tree(':go',   label)   — unconditional goto (omitted if absent)
 *
 * Attribute tag nodes: kind=AST_ATTR, sval=tag_name, nchildren=0 (leaf
 * payload in sval) or nchildren=1 (expr payload in children[0]).
 *
 * lower_stmt() reads attributes by scanning for sval tag match.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast/ast.h"
#include "frontend/snobol4/scrip_cc.h"

/* ── helpers ────────────────────────────────────────────────────────────── */

/* ast_stmt_new — allocate a zeroed AST node of the given kind.
 * Public: used by snobol4.y (SI-4) to build AST_STMT/AST_END directly. */
AST_t *ast_stmt_new(AST_e kind)
{
    AST_t *n = calloc(1, sizeof *n);
    n->t = kind;
    return n;
}
static AST_t *sa_new(AST_e kind) { return ast_stmt_new(kind); }

static void sa_add(AST_t *parent, AST_t *child)
{
    if (!child) return;  /* tagged encoding: never add NULL children */
    ast_push(parent, child);
}

/*
 * ast_attr_leaf — tag node with string value in sval, no children.
 * Matches tree(':tag', string_value) in Snocone.
 * Public: used by snobol4.y (SI-4).
 */
AST_t *ast_attr_leaf(const char *tag, const char *val)
{
    AST_t *n = sa_new(AST_ATTR);
    n->v.sval = strdup(tag);
    /* Store payload as first child if val non-empty, else leaf with no child */
    if (val && val[0]) {
        AST_t *leaf = sa_new(AST_QLIT);
        leaf->v.sval = strdup(val);
        sa_add(n, leaf);
    }
    return n;
}
static AST_t *attr_leaf(const char *tag, const char *val) { return ast_attr_leaf(tag, val); }

/*
 * ast_attr_int — tag node with integer value (stored as string in child sval).
 * Public: used by snobol4.y (SI-4).
 */
AST_t *ast_attr_int(const char *tag, int ival)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%d", ival);
    return ast_attr_leaf(tag, buf);
}
static AST_t *attr_int(const char *tag, int ival) { return ast_attr_int(tag, ival); }

/*
 * ast_attr_expr — tag node with one AST_t* expression child.
 * Matches tree(':tag', '', 1, expr) in Snocone.
 * Public: used by snobol4.y (SI-4).
 */
AST_t *ast_attr_expr(const char *tag, AST_t *expr)
{
    AST_t *n = sa_new(AST_ATTR);
    n->v.sval = strdup(tag);
    sa_add(n, expr);
    return n;
}
static AST_t *attr_expr(const char *tag, AST_t *e) { return ast_attr_expr(tag, e); }

/*
 * make_goto_attr — build a goto attribute node.
 * Static label → attr_leaf(':goX', label)
 * Computed expr → attr_expr(':goX', expr)
 * Absent → NULL (caller skips adding it)
 */
static AST_t *make_goto_attr(const char *tag, const char *label, AST_t *expr)
{
    if (expr)                   return attr_expr(tag, expr);
    if (label && label[0])      return attr_leaf(tag, label);
    return NULL;  /* absent: not added */
}

/* ── public API ─────────────────────────────────────────────────────────── */

AST_t *stmt_to_ast(const STMT_t *s)
{
    if (s->is_end) {
        AST_t *node = sa_new(AST_END);
        if (s->label && s->label[0])
            sa_add(node, attr_leaf(":lbl",  s->label));
        sa_add(node, attr_int(":line", s->lineno));
        sa_add(node, attr_int(":stno", s->stno));
        return node;
    }

    AST_t *node = sa_new(AST_STMT);

    /* Label */
    if (s->label && s->label[0])
        sa_add(node, attr_leaf(":lbl",  s->label));

    /* Lang (omit for LANG_SNO=0 — default, most common) */
    if (s->lang != 0)
        sa_add(node, attr_int(":lang", s->lang));

    /* Source location */
    sa_add(node, attr_int(":line", s->lineno));
    sa_add(node, attr_int(":stno", s->stno));

    /* Subject */
    if (s->subject)
        sa_add(node, attr_expr(":subj", s->subject));

    /* Pattern */
    if (s->pattern)
        sa_add(node, attr_expr(":pat",  s->pattern));

    /* has_eq + replacement */
    if (s->has_eq) {
        sa_add(node, attr_leaf(":eq", ""));
        if (s->replacement)
            sa_add(node, attr_expr(":repl", s->replacement));
        else {
            /* has_eq=true with no replacement → empty string replacement */
            AST_t *empty = sa_new(AST_QLIT);
            empty->v.sval = strdup("");
            sa_add(node, attr_expr(":repl", empty));
        }
    }

    /* Goto arms (omitted when absent) */
    sa_add(node, make_goto_attr(":goS", s->goto_s, s->goto_s_expr));
    sa_add(node, make_goto_attr(":goF", s->goto_f, s->goto_f_expr));
    sa_add(node, make_goto_attr(":go",  s->goto_u, s->goto_u_expr));

    return node;
}

AST_t *code_to_ast(const CODE_t *prog)
{
    AST_t *root = sa_new(AST_PROGRAM);
    for (const STMT_t *s = prog->head; s; s = s->next)
        sa_add(root, stmt_to_ast(s));
    return root;
}

/*
 * stmt_attr_find — find the first attribute child with tag sval==tag.
 * Returns the AST_ATTR node, or NULL if not present.
 * Used by lower_stmt() to read tagged fields.
 */
AST_t *stmt_attr_find(const AST_t *stmt, const char *tag)
{
    for (int i = 0; i < stmt->n; i++) {
        AST_t *ch = stmt->c[i];
        if (ch && ch->t == AST_ATTR && ch->v.sval && strcmp(ch->v.sval, tag) == 0)
            return ch;
    }
    return NULL;
}

/*
 * stmt_attr_expr — get the expression child of an attribute node.
 * Returns children[0] if present, NULL otherwise.
 */
AST_t *stmt_attr_expr(const AST_t *attr)
{
    if (!attr || attr->n == 0) return NULL;
    return attr->c[0];
}

/*
 * stmt_attr_str — get the string value of an attribute leaf node.
 * For leaf attrs: children[0]->v.sval; for childless attrs: attr->v.sval payload.
 */
const char *stmt_attr_str(const AST_t *attr)
{
    if (!attr) return NULL;
    if (attr->n > 0 && attr->c[0])
        return attr->c[0]->v.sval;
    return NULL;
}
