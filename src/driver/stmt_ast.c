#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast/ast.h"
#include "frontend/snobol4/scrip_cc.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *ast_stmt_new(tree_e kind)
{
    tree_t *n = calloc(1, sizeof *n);
    n->t = kind;
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *sa_new(tree_e kind) { return ast_stmt_new(kind); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sa_add(tree_t *parent, tree_t *child)
{
    if (!child) return;
    ast_push(parent, child);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *ast_attr_leaf(const char *tag, const char *val)
{
    tree_t *n = sa_new(TT_ATTR);
    n->v.sval = strdup(tag);
    if (val && val[0]) {
        tree_t *leaf = sa_new(TT_QLIT);
        leaf->v.sval = strdup(val);
        sa_add(n, leaf);
    }
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *attr_leaf(const char *tag, const char *val) { return ast_attr_leaf(tag, val); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *ast_attr_int(const char *tag, int ival)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%d", ival);
    return ast_attr_leaf(tag, buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *attr_int(const char *tag, int ival) { return ast_attr_int(tag, ival); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *ast_attr_expr(const char *tag, tree_t *expr)
{
    tree_t *n = sa_new(TT_ATTR);
    n->v.sval = strdup(tag);
    sa_add(n, expr);
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *attr_expr(const char *tag, tree_t *e) { return ast_attr_expr(tag, e); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *make_goto_attr(const char *tag, const char *label, tree_t *expr)
{
    if (expr)                   return attr_expr(tag, expr);
    if (label && label[0])      return attr_leaf(tag, label);
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *stmt_to_ast(const STMT_t *s)
{
    if (s->is_end) {
        tree_t *node = sa_new(TT_END);
        if (s->label && s->label[0])
            sa_add(node, attr_leaf(":lbl",  s->label));
        sa_add(node, attr_int(":line", s->lineno));
        sa_add(node, attr_int(":stno", s->stno));
        return node;
    }
    tree_t *node = sa_new(TT_STMT);
    if (s->label && s->label[0])
        sa_add(node, attr_leaf(":lbl",  s->label));
    if (s->lang != 0)
        sa_add(node, attr_int(":lang", s->lang));
    sa_add(node, attr_int(":line", s->lineno));
    sa_add(node, attr_int(":stno", s->stno));
    if (s->subject)
        sa_add(node, attr_expr(":subj", s->subject));
    if (s->pattern)
        sa_add(node, attr_expr(":pat",  s->pattern));
    if (s->has_eq) {
        sa_add(node, attr_leaf(":eq", ""));
        if (s->replacement)
            sa_add(node, attr_expr(":repl", s->replacement));
        else {
            tree_t *empty = sa_new(TT_QLIT);
            empty->v.sval = strdup("");
            sa_add(node, attr_expr(":repl", empty));
        }
    }
    sa_add(node, make_goto_attr(":goS", s->goto_s, s->goto_s_expr));
    sa_add(node, make_goto_attr(":goF", s->goto_f, s->goto_f_expr));
    sa_add(node, make_goto_attr(":go",  s->goto_u, s->goto_u_expr));
    return node;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *code_to_ast(const CODE_t *prog)
{
    tree_t *root = sa_new(TT_PROGRAM);
    for (const STMT_t *s = prog->head; s; s = s->next)
        sa_add(root, stmt_to_ast(s));
    return root;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *stmt_attr_find(const tree_t *stmt, const char *tag)
{
    for (int i = 0; i < stmt->n; i++) {
        tree_t *ch = stmt->c[i];
        if (ch && ch->t == TT_ATTR && ch->v.sval && strcmp(ch->v.sval, tag) == 0)
            return ch;
    }
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *stmt_attr_expr(const tree_t *attr)
{
    if (!attr || attr->n == 0) return NULL;
    return attr->c[0];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *stmt_attr_str(const tree_t *attr)
{
    if (!attr) return NULL;
    if (attr->n > 0 && attr->c[0])
        return attr->c[0]->v.sval;
    return NULL;
}
