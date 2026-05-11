/*
 * stmt_ast.c — SI-2: shim helpers bridging CODE_t/STMT_t → AST_t
 *
 * During Phase 5 (SI-1..SI-6), frontends still emit the old CODE_t/STMT_t
 * linked-list structures.  lower() and lower_stmt() will be migrated (SI-3)
 * to consume AST_PROGRAM / AST_STMT trees.  These shims translate on the
 * fly so SI-3 can switch lower()'s signature without touching any frontend.
 *
 * Once all six frontends emit AST_STMT directly (SI-4..SI-5), these shims
 * become dead code and are deleted in SI-6.
 *
 * AST_STMT encoding (see ast.h):
 *   sval          = label (NULL or "" if none)
 *   ival          = lang  (LANG_SNO / LANG_ICN / …)
 *   a[0].i        = lineno
 *   a[1].i        = stno
 *   a[2].i        = flags (bit 0 = has_eq, bit 1 = is_end)
 *   children[0]   = subject   AST_t* (NULL node if absent)
 *   children[1]   = pattern   AST_t* (NULL node if absent)
 *   children[2]   = replacement AST_t* (NULL node if absent)
 *   children[3]   = AST_GOTO_S node  (sval=goto_s label or NULL;
 *                                      child[0]=goto_s_expr or NULL)
 *   children[4]   = AST_GOTO_F node  (sval=goto_f label or NULL;
 *                                      child[0]=goto_f_expr or NULL)
 *   children[5]   = AST_GOTO_U node  (sval=goto_u label or NULL;
 *                                      child[0]=goto_u_expr or NULL)
 *
 * AST_PROGRAM encoding:
 *   children[0..nstmts-1] = AST_STMT nodes (one per STMT_t)
 *
 * Flags bit-field:
 *   STMT_FLAG_HAS_EQ   bit 0  — statement has an '=' (replacement field)
 *   STMT_FLAG_IS_END   bit 1  — END statement
 */

#include <stdlib.h>
#include <string.h>
#include "ast/ast.h"
#include "frontend/snobol4/scrip_cc.h"

/* ── flag bits ─────────────────────────────────────────────────────────── */
#define STMT_FLAG_HAS_EQ   1
#define STMT_FLAG_IS_END   2

/* ── internal helpers ───────────────────────────────────────────────────── */

/* Allocate a zero-filled AST_t of the given kind. */
static AST_t *ast_new(AST_e kind)
{
    AST_t *n = calloc(1, sizeof *n);
    n->kind = kind;
    return n;
}

/* Append one child to a node, growing children[] as needed. */
static void ast_add_child(AST_t *parent, AST_t *child)
{
    if (parent->nchildren >= parent->nalloc) {
        parent->nalloc = parent->nalloc ? parent->nalloc * 2 : 4;
        parent->children = realloc(parent->children,
                                   (size_t)parent->nalloc * sizeof(AST_t *));
    }
    parent->children[parent->nchildren++] = child;
}

/* Build one AST_GOTO_S/F/U node from a label string and optional expr. */
static AST_t *make_goto_arm(AST_e kind, const char *label, AST_t *expr)
{
    AST_t *arm = ast_new(kind);
    arm->sval = label ? strdup(label) : NULL;
    if (expr)
        ast_add_child(arm, expr);
    return arm;
}

/* ── public API ─────────────────────────────────────────────────────────── */

/*
 * stmt_to_ast — convert one STMT_t into an AST_STMT node.
 *
 * The STMT_t's AST_t child pointers (subject, pattern, replacement,
 * goto_*_expr) are moved into the new node — not copied — since they
 * are GC-allocated and the shim is a one-shot translation.
 */
AST_t *stmt_to_ast(const STMT_t *s)
{
    AST_t *node = ast_new(AST_STMT);

    /* Scalar fields */
    node->sval   = (s->label && s->label[0]) ? strdup(s->label) : NULL;
    node->ival   = (long long)s->lang;
    node->a[0].i = s->lineno;
    node->a[1].i = s->stno;
    node->a[2].i = (s->has_eq  ? STMT_FLAG_HAS_EQ : 0)
                 | (s->is_end  ? STMT_FLAG_IS_END  : 0);

    /* children[0..2]: subject / pattern / replacement (NULL means absent) */
    ast_add_child(node, s->subject);
    ast_add_child(node, s->pattern);
    ast_add_child(node, s->replacement);

    /* children[3..5]: goto arms */
    ast_add_child(node, make_goto_arm(AST_GOTO_S, s->goto_s, s->goto_s_expr));
    ast_add_child(node, make_goto_arm(AST_GOTO_F, s->goto_f, s->goto_f_expr));
    ast_add_child(node, make_goto_arm(AST_GOTO_U, s->goto_u, s->goto_u_expr));

    return node;
}

/*
 * code_to_ast — convert a full CODE_t program into an AST_PROGRAM node.
 *
 * Each STMT_t in the linked list becomes one AST_STMT child.
 * The CODE_t itself is not freed; that is the caller's responsibility.
 */
AST_t *code_to_ast(const CODE_t *prog)
{
    AST_t *root = ast_new(AST_PROGRAM);
    for (const STMT_t *s = prog->head; s; s = s->next)
        ast_add_child(root, stmt_to_ast(s));
    return root;
}
