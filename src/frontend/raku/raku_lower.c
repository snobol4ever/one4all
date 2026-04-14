/*
 * raku_lower.c — Tiny-Raku AST (RakuNode) → unified IR (EXPR_t) lowering pass
 *
 * Walks a RakuNode tree produced by raku_parse.c and produces an EXPR_t tree
 * using canonical EKind names from ir/ir.h.  After this pass the RakuNode tree
 * is no longer needed.
 *
 * Mapping policy (RK-3, 2026-04-14):
 *   RK_INT        → E_ILIT
 *   RK_FLOAT      → E_FLIT
 *   RK_STR        → E_QLIT
 *   RK_VAR_SCALAR → E_VAR  (strip leading $)
 *   RK_VAR_ARRAY  → E_VAR  (strip leading @)
 *   RK_ADD        → E_ADD, RK_SUBTRACT→E_SUB, RK_MUL→E_MUL
 *   RK_DIV        → E_DIV, RK_MOD→E_MOD
 *   RK_NEG        → E_MNS
 *   RK_STRCAT     → E_CAT  (string concatenation ~)
 *   RK_EQ→E_EQ, RK_NE→E_NE, RK_LT→E_LT, RK_GT→E_GT, RK_LE→E_LE, RK_GE→E_GE
 *   RK_SEQ→E_LEQ, RK_SNE→E_LNE  (string eq/ne)
 *   RK_AND        → E_SEQ (short-circuit: seq of two goal-directed exprs)
 *   RK_OR         → E_ALT
 *   RK_NOT        → E_NOT
 *   RK_MY_SCALAR  → E_ASSIGN(E_VAR(name), rhs)
 *   RK_ASSIGN     → E_ASSIGN(E_VAR(name), rhs)
 *   RK_SAY        → E_FNC("write",  [expr])   (reuse Icon write builtin)
 *   RK_PRINT      → E_FNC("writes", [expr])   (no newline variant)
 *   RK_TAKE       → E_SUSPEND(expr)
 *   RK_GATHER     → E_ITERATE(body_seq)        (BB_PUMP)
 *   RK_FOR range  → E_EVERY(E_TO(lo,hi), body)
 *   RK_FOR array  → E_EVERY(E_ITERATE(E_VAR(arr)), body)
 *   RK_IF         → E_IF(cond, then [,else])
 *   RK_WHILE      → E_WHILE(cond, body)
 *   RK_BLOCK      → E_SEQ_EXPR(stmts...)
 *   RK_SUBDEF     → E_FNC(name, params_ignored, body)
 *   RK_CALL       → E_FNC(name, args...)
 *   RK_RETURN     → E_RETURN([expr])
 *   RK_RANGE      → E_TO(lo, hi)   (inclusive)
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "raku_lower.h"
#include "raku_ast.h"
#include "../../ir/ir.h"
#include "../snobol4/scrip_cc.h"   /* expr_new, expr_add_child, expr_unary,
                                      expr_binary, intern; EXPR_T_DEFINED */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*========================================================================
 * Forward declaration
 *========================================================================*/
static EXPR_t *lower_node(const RakuNode *n);

/*========================================================================
 * Helpers
 *========================================================================*/

/* strip_sigil — return pointer past leading $ or @ in a var name.
 * If no sigil, return s unchanged.  Result points into s (no alloc). */
static const char *strip_sigil(const char *s) {
    if (s && (s[0] == '$' || s[0] == '@')) return s + 1;
    return s;
}

/* leaf_sval — allocate a leaf node with a string payload. */
static EXPR_t *leaf_sval(EKind k, const char *s) {
    EXPR_t *e = expr_new(k);
    e->sval = intern(s);
    return e;
}

/* var_node — E_VAR with sigil stripped. */
static EXPR_t *var_node(const char *name) {
    return leaf_sval(E_VAR, strip_sigil(name));
}

/* fnc_node — E_FNC leaf with name; caller adds children. */
static EXPR_t *fnc_node(const char *name) {
    return leaf_sval(E_FNC, name);
}

/* lower_block_children — lower every statement in an RK_BLOCK into dst. */
static void lower_block_children(EXPR_t *dst, const RakuNode *block) {
    if (!block || block->kind != RK_BLOCK || !block->children) return;
    for (int i = 0; i < block->children->count; i++)
        expr_add_child(dst, lower_node(block->children->items[i]));
}

/* lower_block — lower an RK_BLOCK node → E_SEQ_EXPR containing its stmts. */
static EXPR_t *lower_block(const RakuNode *block) {
    EXPR_t *seq = expr_new(E_SEQ_EXPR);
    lower_block_children(seq, block);
    return seq;
}

/*========================================================================
 * Main lowering dispatch
 *========================================================================*/

static EXPR_t *lower_node(const RakuNode *n) {
    if (!n) return NULL;
    EXPR_t *e;

    switch (n->kind) {

    /*-- Literals ----------------------------------------------------------*/
    case RK_INT:
        e = expr_new(E_ILIT);
        e->ival = n->ival;
        return e;

    case RK_FLOAT:
        e = expr_new(E_FLIT);
        e->dval = n->dval;
        return e;

    case RK_STR:
        return leaf_sval(E_QLIT, n->sval ? n->sval : "");

    /*-- Variables ---------------------------------------------------------*/
    case RK_VAR_SCALAR:
    case RK_VAR_ARRAY:
    case RK_IDENT:
        return var_node(n->sval);

    /*-- Arithmetic --------------------------------------------------------*/
    case RK_ADD:      return expr_binary(E_ADD, lower_node(n->left), lower_node(n->right));
    case RK_SUBTRACT: return expr_binary(E_SUB, lower_node(n->left), lower_node(n->right));
    case RK_MUL:      return expr_binary(E_MUL, lower_node(n->left), lower_node(n->right));
    case RK_DIV:      return expr_binary(E_DIV, lower_node(n->left), lower_node(n->right));
    case RK_MOD:      return expr_binary(E_MOD, lower_node(n->left), lower_node(n->right));
    case RK_NEG:      return expr_unary (E_MNS, lower_node(n->left));
    case RK_NOT:      return expr_unary (E_NOT, lower_node(n->left));

    /*-- String concat (Raku ~) -------------------------------------------*/
    case RK_STRCAT:
        return expr_binary(E_CAT, lower_node(n->left), lower_node(n->right));

    /*-- Numeric comparisons -----------------------------------------------*/
    case RK_EQ: return expr_binary(E_EQ, lower_node(n->left), lower_node(n->right));
    case RK_NE: return expr_binary(E_NE, lower_node(n->left), lower_node(n->right));
    case RK_LT: return expr_binary(E_LT, lower_node(n->left), lower_node(n->right));
    case RK_GT: return expr_binary(E_GT, lower_node(n->left), lower_node(n->right));
    case RK_LE: return expr_binary(E_LE, lower_node(n->left), lower_node(n->right));
    case RK_GE: return expr_binary(E_GE, lower_node(n->left), lower_node(n->right));

    /*-- String comparisons (eq / ne) --------------------------------------*/
    case RK_SEQ: return expr_binary(E_LEQ, lower_node(n->left), lower_node(n->right));
    case RK_SNE: return expr_binary(E_LNE, lower_node(n->left), lower_node(n->right));

    /*-- Logic (short-circuit) ---------------------------------------------*/
    case RK_AND: return expr_binary(E_SEQ, lower_node(n->left), lower_node(n->right));
    case RK_OR:  return expr_binary(E_ALT, lower_node(n->left), lower_node(n->right));

    /*-- Range (standalone) ------------------------------------------------*/
    case RK_RANGE:
    case RK_RANGE_EX:
        /* inclusive range; RK_RANGE_EX (..^) is future — treat as inclusive */
        return expr_binary(E_TO, lower_node(n->left), lower_node(n->right));

    /*-- Assignment: my $x = expr  /  $x = expr ---------------------------*/
    case RK_MY_SCALAR:
    case RK_MY_ARRAY:
    case RK_ASSIGN: {
        EXPR_t *lhs = var_node(n->sval);
        EXPR_t *rhs = lower_node(n->left);   /* rhs expression */
        return expr_binary(E_ASSIGN, lhs, rhs);
    }

    /*-- say / print -------------------------------------------------------*/
    case RK_SAY: {
        /* say expr → E_FNC("write", [expr]) — reuses Icon write builtin */
        e = fnc_node("write");
        expr_add_child(e, lower_node(n->left));
        return e;
    }
    case RK_PRINT: {
        /* print expr → E_FNC("writes", [expr]) — no newline */
        e = fnc_node("writes");
        expr_add_child(e, lower_node(n->left));
        return e;
    }

    /*-- take expr  →  E_SUSPEND(expr) ------------------------------------*/
    case RK_TAKE:
        return expr_unary(E_SUSPEND, lower_node(n->left));

    /*-- return [expr]  →  E_RETURN([expr]) --------------------------------*/
    case RK_RETURN:
        e = expr_new(E_RETURN);
        if (n->left) expr_add_child(e, lower_node(n->left));
        return e;

    /*-- gather { block }  →  E_ITERATE(E_SEQ_EXPR(body)) -----------------
     * gather maps to BB_PUMP — same broker mode as Icon every/generator.
     * The body of gather is lowered as a sequence; E_ITERATE drives it.   */
    case RK_GATHER: {
        EXPR_t *body = lower_block(n->left);   /* n->left = body block */
        return expr_unary(E_ITERATE, body);
    }

    /*-- for RANGE -> $v body  →  E_EVERY(E_TO(lo,hi), body) --------------
     * for @arr -> $v body     →  E_EVERY(E_ITERATE(E_VAR(arr)), body)
     *
     * The pointy-block variable binding is expressed as an E_ASSIGN of
     * $_ / $v around the body — we wrap body in an E_SEQ_EXPR that first
     * assigns the iteration variable then runs the original body stmts.   */
    case RK_FOR: {
        const RakuNode *iter = n->left;        /* range or array expr */
        const char     *var  = n->sval;        /* "$v" or NULL → "$_" */
        const RakuNode *body = n->extra;       /* body block */
        if (!var) var = "$_";

        /* Build the generator expression */
        EXPR_t *gen;
        if (iter && (iter->kind == RK_RANGE || iter->kind == RK_RANGE_EX)) {
            gen = expr_binary(E_TO, lower_node(iter->left), lower_node(iter->right));
        } else if (iter && (iter->kind == RK_VAR_ARRAY || iter->kind == RK_VAR_SCALAR)) {
            gen = expr_unary(E_ITERATE, var_node(iter->sval));
        } else {
            gen = lower_node(iter);
        }

        /* Build loop body: assign iter var from $_, then execute stmts.
         * We wrap in E_SEQ_EXPR: [assign($v, $_), ...body stmts...] */
        EXPR_t *loop_body = expr_new(E_SEQ_EXPR);
        /* bind the iteration variable — the runtime sets $_ each iteration;
         * we bind the named var from $_ so the body can use it.           */
        if (strcmp(var, "$_") != 0) {
            EXPR_t *bind = expr_binary(E_ASSIGN,
                                       var_node(var),
                                       var_node("$_"));
            expr_add_child(loop_body, bind);
        }
        lower_block_children(loop_body, body);

        /* E_EVERY drives the generator to exhaustion (BB_PUMP semantics) */
        return expr_binary(E_EVERY, gen, loop_body);
    }

    /*-- if cond then [else] -----------------------------------------------*/
    case RK_IF: {
        e = expr_new(E_IF);
        expr_add_child(e, lower_node(n->left));    /* condition */
        expr_add_child(e, lower_block(n->right));  /* then block */
        if (n->extra) expr_add_child(e, lower_block(n->extra)); /* else */
        return e;
    }

    /*-- while cond body ---------------------------------------------------*/
    case RK_WHILE:
        return expr_binary(E_WHILE, lower_node(n->left), lower_block(n->right));

    /*-- block → E_SEQ_EXPR ------------------------------------------------*/
    case RK_BLOCK:
        return lower_block(n);

    /*-- expr; statement wrapper -------------------------------------------*/
    case RK_EXPR_STMT:
        return lower_node(n->left);

    /*-- sub name(params) body  →  E_FNC(name, body) ----------------------
     * Parameters are not yet wired (Phase 1 scope); body lowered as-is.  */
    case RK_SUBDEF: {
        e = fnc_node(n->sval ? n->sval : "<anon>");
        expr_add_child(e, lower_block(n->right));   /* body */
        return e;
    }

    /*-- name(args)  →  E_FNC(name, args...) ------------------------------*/
    case RK_CALL: {
        e = fnc_node(n->sval ? n->sval : "<unknown>");
        if (n->children) {
            for (int i = 0; i < n->children->count; i++)
                expr_add_child(e, lower_node(n->children->items[i]));
        }
        return e;
    }

    /*-- IDIV (integer division) — lower as DIV for now -------------------*/
    case RK_IDIV:
        return expr_binary(E_DIV, lower_node(n->left), lower_node(n->right));

    default:
        fprintf(stderr, "raku_lower: unhandled RakuKind %d at line %d\n",
                (int)n->kind, n->lineno);
        return expr_new(E_NUL);
    }
}

/*========================================================================
 * Public API
 *========================================================================*/

/* raku_lower_stmt — lower one top-level statement/sub node → EXPR_t. */
EXPR_t *raku_lower_stmt(const RakuNode *n) {
    return lower_node(n);
}

/* raku_lower_file — lower a top-level program block.
 *
 * A Tiny-Raku file is an RK_BLOCK of statements.  Sub definitions become
 * individual E_FNC entries.  All remaining top-level statements are wrapped
 * in a synthetic "main" E_FNC so the driver can call them uniformly.
 *
 * stmts[0..count-1] are the individual statements from the top-level block.
 * Returns malloc'd EXPR_t** of length *out_count.  Caller owns result.      */
EXPR_t **raku_lower_file(RakuNode **stmts, int count, int *out_count) {
    /* First pass: count subs vs top-level stmts */
    int nsubs = 0;
    for (int i = 0; i < count; i++)
        if (stmts[i] && stmts[i]->kind == RK_SUBDEF) nsubs++;
    int has_main = (count - nsubs) > 0;

    int total = nsubs + (has_main ? 1 : 0);
    EXPR_t **result = malloc(sizeof(EXPR_t *) * (total + 1));
    if (!result) { *out_count = 0; return NULL; }

    int out = 0;

    /* Emit subs first */
    for (int i = 0; i < count; i++) {
        if (!stmts[i] || stmts[i]->kind != RK_SUBDEF) continue;
        result[out++] = lower_node(stmts[i]);
    }

    /* Wrap non-sub statements in synthetic "main" */
    if (has_main) {
        EXPR_t *main_fnc = fnc_node("main");
        EXPR_t *body = expr_new(E_SEQ_EXPR);
        for (int i = 0; i < count; i++) {
            if (!stmts[i] || stmts[i]->kind == RK_SUBDEF) continue;
            expr_add_child(body, lower_node(stmts[i]));
        }
        expr_add_child(main_fnc, body);
        result[out++] = main_fnc;
    }

    result[out] = NULL;   /* sentinel */
    *out_count = out;
    return result;
}
