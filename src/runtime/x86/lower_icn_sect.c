/*
 * lower_icn_sect.c — Icon section and bang-binary handlers (SR-10)
 *
 * AST kinds handled:
 *   AST_SECTION        s[i:j]    range section
 *   AST_SECTION_PLUS   s[i+:n]   plus-section (offset + length)
 *   AST_SECTION_MINUS  s[i-:n]   minus-section (offset - length)
 *   AST_BANG_BINARY    !E        generative application / list bang
 *
 * All three section variants require exactly 3 children (base, lo, hi/len);
 * they fall back to SM_PUSH_NULL for malformed nodes.
 *
 * AST_BANG_BINARY is generative and routes through SM_BB_PUMP_AST.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include "sm_interp.h"

/* ── AST_SECTION ─────────────────────────────────────────────── */

static void lower_section(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren >= 3) {
        lower_expr(c, e->children[0]);
        lower_expr(c, e->children[1]);
        lower_expr(c, e->children[2]);
        sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_RANGE", 3);
    } else {
        sm_emit(p, SM_PUSH_NULL);
    }
}

/* ── AST_SECTION_PLUS ────────────────────────────────────────── */

static void lower_section_plus(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren >= 3) {
        lower_expr(c, e->children[0]);
        lower_expr(c, e->children[1]);
        lower_expr(c, e->children[2]);
        sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_PLUS", 3);
    } else {
        sm_emit(p, SM_PUSH_NULL);
    }
}

/* ── AST_SECTION_MINUS ───────────────────────────────────────── */

static void lower_section_minus(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren >= 3) {
        lower_expr(c, e->children[0]);
        lower_expr(c, e->children[1]);
        lower_expr(c, e->children[2]);
        sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_MINUS", 3);
    } else {
        sm_emit(p, SM_PUSH_NULL);
    }
}

/* ── AST_BANG_BINARY ─────────────────────────────────────────── */

static void lower_bang_binary(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
}

/* ── Registration ─────────────────────────────────────────────── */

void lower_icn_sect_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_SECTION]       = lower_section;
    tbl[AST_SECTION_PLUS]  = lower_section_plus;
    tbl[AST_SECTION_MINUS] = lower_section_minus;
    tbl[AST_BANG_BINARY]   = lower_bang_binary;
}
