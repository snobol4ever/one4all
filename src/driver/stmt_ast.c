/*
 * stmt_ast.c — SI-2 (revised): shim helpers bridging CODE_t/STMT_t → AST_t
 *
 * During Phase 5 (SI-1..SI-6) frontends still emit CODE_t/STMT_t.  These
 * shims translate to AST_PROGRAM / AST_STMT / AST_END so that lower() and
 * lower_stmt() can be migrated (SI-3) without touching any frontend.
 *
 * Once all six frontends emit AST_STMT directly (SI-4..SI-5) these shims
 * become dead code and are deleted in SI-6.
 *
 * ── Pure AST encoding ──────────────────────────────────────────────────────
 *
 * Every node is characterised solely by:
 *   kind  — what the node IS
 *   sval  — string value  (name / label / literal text)
 *   ival  — integer value (lang for STMT, literal for ILIT, etc.)
 *   dval  — float value   (float literals only)
 *   nchildren / children[] — structural children
 *   a[0..1] — small integer metadata (lineno, stno); not tree structure
 *
 * No boolean flags in a[].  Structural distinctions are expressed as kinds
 * or as the presence/absence of a children[] slot.
 *
 * AST_PROGRAM   children[0..n-1] = AST_STMT or AST_END nodes
 *
 * AST_STMT      sval     = label (NULL if none)
 *               ival     = lang  (LANG_SNO / LANG_ICN / …)
 *               a[0].i   = lineno
 *               a[1].i   = stno
 *               children[0] = subject      (NULL slot if absent)
 *               children[1] = pattern      (NULL slot if absent)
 *               children[2] = replacement  NULL slot → no '=' (has_eq false)
 *                                          non-NULL  → has '=' (has_eq true)
 *                                          AST_NUL   → '=' with empty repl
 *               children[3] = AST_GOTO_S   arm
 *               children[4] = AST_GOTO_F   arm
 *               children[5] = AST_GOTO_U   arm
 *
 * AST_END       sval     = label (NULL if none)
 *               a[0].i   = lineno
 *               a[1].i   = stno
 *               nchildren = 0
 *
 * AST_GOTO_S/F/U sval   = static label (NULL if arm absent)
 *                nchildren = 0   static label (or absent)
 *                nchildren = 1   computed goto: children[0] = expr
 * ──────────────────────────────────────────────────────────────────────────
 */

#include <stdlib.h>
#include <string.h>
#include "ast/ast.h"
#include "frontend/snobol4/scrip_cc.h"

/* ── internal helpers ───────────────────────────────────────────────────── */

/* Allocate a zero-filled AST_t of the given kind. */
static AST_t *sa_new(AST_e kind)
{
    AST_t *n = calloc(1, sizeof *n);
    n->kind = kind;
    return n;
}

/*
 * Append one child slot to a node.  child may be NULL — a NULL slot is a
 * valid, meaningful "absent" entry in the fixed-layout children[] of AST_STMT.
 * The nchildren count is always incremented so slot indices are stable.
 */
static void sa_add(AST_t *parent, AST_t *child)
{
    if (parent->nchildren >= parent->nalloc) {
        parent->nalloc = parent->nalloc ? parent->nalloc * 2 : 8;
        parent->children = realloc(parent->children,
                                   (size_t)parent->nalloc * sizeof(AST_t *));
    }
    parent->children[parent->nchildren++] = child;
}

/*
 * Build one goto-arm node.
 *   label  — static target label string, or NULL if arm is absent
 *   expr   — computed-goto expression, or NULL for static/absent arm
 * A NULL label AND NULL expr → arm is absent (sval=NULL, nchildren=0).
 * A non-empty label         → sval=label, nchildren=0.
 * A non-NULL expr           → sval=NULL, nchildren=1, children[0]=expr.
 */
static AST_t *make_goto_arm(AST_e kind, const char *label, AST_t *expr)
{
    AST_t *arm = sa_new(kind);
    if (expr) {
        /* computed goto — expr takes precedence; label is ignored */
        sa_add(arm, expr);
    } else if (label && label[0]) {
        arm->sval = strdup(label);
    }
    /* else: absent arm — sval=NULL, nchildren=0 */
    return arm;
}

/* ── public API ─────────────────────────────────────────────────────────── */

/*
 * stmt_to_ast — convert one STMT_t into AST_STMT or AST_END.
 *
 * END statements become AST_END (structurally distinct kind — not a flag).
 * All other statements become AST_STMT with exactly 6 children.
 *
 * The STMT_t's AST_t child pointers (subject, pattern, replacement,
 * goto_*_expr) are moved into the new node — not copied.
 */
AST_t *stmt_to_ast(const STMT_t *s)
{
    if (s->is_end) {
        AST_t *node = sa_new(AST_END);
        node->sval   = (s->label && s->label[0]) ? strdup(s->label) : NULL;
        node->a[0].i = s->lineno;
        node->a[1].i = s->stno;
        return node;
    }

    AST_t *node = sa_new(AST_STMT);

    /* Scalar fields — no boolean flags */
    node->sval   = (s->label && s->label[0]) ? strdup(s->label) : NULL;
    node->ival   = (long long)s->lang;
    node->a[0].i = s->lineno;
    node->a[1].i = s->stno;

    /* children[0]: subject (NULL slot if absent) */
    sa_add(node, s->subject);

    /* children[1]: pattern (NULL slot if absent) */
    sa_add(node, s->pattern);

    /*
     * children[2]: replacement
     *   NULL slot → has_eq false (no '=' in source)
     *   non-NULL  → has_eq true  (AST_NUL means '=' with empty replacement)
     */
    if (s->has_eq)
        sa_add(node, s->replacement ? s->replacement : sa_new(AST_NUL));
    else
        sa_add(node, (AST_t *)NULL);

    /* children[3..5]: goto arms */
    sa_add(node, make_goto_arm(AST_GOTO_S, s->goto_s, s->goto_s_expr));
    sa_add(node, make_goto_arm(AST_GOTO_F, s->goto_f, s->goto_f_expr));
    sa_add(node, make_goto_arm(AST_GOTO_U, s->goto_u, s->goto_u_expr));

    return node;
}

/*
 * code_to_ast — convert a full CODE_t program into an AST_PROGRAM node.
 *
 * Each STMT_t in the linked list becomes one child (AST_STMT or AST_END).
 * The CODE_t itself is not freed; caller's responsibility.
 */
AST_t *code_to_ast(const CODE_t *prog)
{
    AST_t *root = sa_new(AST_PROGRAM);
    for (const STMT_t *s = prog->head; s; s = s->next)
        sa_add(root, stmt_to_ast(s));
    return root;
}
