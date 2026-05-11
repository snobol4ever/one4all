/*
 * lower_stmt.c — SNOBOL4/Prolog statement-level lowering (SR-12)
 *
 * Provides lower_stmt(ctx, s): lowers one STMT_t into SM bytecode.
 *
 * Sub-phases handled here:
 *   1. Blank-line guard — skip empty statements
 *   2. Label emission — SM_LABEL_NAMED before SM_STNO; tag DEFINE entry
 *   3. SM_STNO / SM_HALT for is_end
 *   4. Icon lang — nothing to emit (defs registered by polyglot_init)
 *   5. Prolog lang — SM_BB_ONCE_PROC or SM_BB_ONCE
 *   6. Pattern match — lower_pat_expr + subject + replacement + SM_EXEC_STMT
 *   7. Assignment — subject lhs dispatch (VAR/KW/INDIRECT/IDX/FNC/generic)
 *   8. Bare expression — lower_expr + SM_VOID_POP (RETURN/FRETURN/NRETURN short-circuit)
 *   9. Goto emission — SM_JUMP / SM_JUMP_S / SM_JUMP_F / SM_JUMP_INDIR
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include "sm_prog.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"
#include "snobol4.h"
#include <gc/gc.h>
#include <string.h>
#include <stdio.h>

void lower_stmt(LowerCtx *c, const STMT_t *s)
{
    SM_Program *p = c->p;
    LabelTable *labtab = &c->labtab;

    /* Blank source line — emit nothing; the next non-blank stmt's SM_STNO fires. */
    if (!s->is_end
        && (!s->label || !s->label[0])
        && !s->subject && !s->pattern && !s->replacement
        && !s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) {
        return;
    }

    /* Label emitted before SM_STNO so backward branches land on the STNO. */
    if (s->label && s->label[0]) {
        int lbl_idx = sm_label_named(p, s->label);
        labtab_define(labtab, s->label, lbl_idx);
        /* Tag DEFINE'd function entry labels; mode-3 emits a call prologue for them. */
        if (FUNC_IS_ENTRY_LABEL(s->label)) {
            p->instrs[p->count - 1].a[2].i = 1;
            sm_emit(p, SM_DEFINE_ENTRY);
        }
    }

    sm_emit_ii(p, SM_STNO, (int64_t)s->stno, (int64_t)s->lineno);

    if (s->is_end) { sm_emit(p, SM_HALT); return; }

    /* Icon proc/global/record defs are registered by polyglot_init; nothing to emit per-def. */
    if (s->lang == LANG_ICN) return;

    if (s->lang == LANG_PL) {
        if (s->subject && s->subject->kind == AST_CHOICE && s->subject->sval) {
            const char *key = s->subject->sval;
            int arity = 0;
            const char *sl = strrchr(key, '/');
            if (sl) arity = atoi(sl + 1);
            sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
        } else {
            if (s->subject) lower_expr(c, s->subject);
            else            sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_BB_ONCE);
        }
        goto emit_gotos;
    }

    /*
     * Pattern match statement:  subject  pattern  [= replacement]  :(goto)
     *
     * Pattern tree is emitted first so its parameterised-op args (e.g. SM_PAT_LEN)
     * are consumed from the value stack before the subject is pushed.
     */
    if (s->pattern) {
        lower_pat_expr(c, s->pattern);
        if (s->subject) lower_expr(c, s->subject);
        else            sm_emit(p, SM_PUSH_NULL);
        if (s->has_eq && s->replacement)
            lower_expr(c, s->replacement);
        else if (s->has_eq)
            sm_emit_si(p, SM_PUSH_LIT_S, "", 0);
        else
            sm_emit_i(p, SM_PUSH_LIT_I, 0);
        /* a[0].s = subject variable name for write-back; a[1].i = has_eq.
         * GC_strdup the sval — the IR may be freed before the SM_Program is used. */
        {
            const char *sname = NULL;
            if (s->subject && (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD))
                sname = s->subject->sval;
            sm_emit_si(p, SM_EXEC_STMT, sname, (int64_t)s->has_eq);
        }
        goto emit_gotos;
    }

    /*
     * Pure assignment or expression statement:
     *   label:  expr = value   :(goto)
     *   label:  expr           :(goto)
     */
    if (s->subject) {
        if (s->has_eq) {
            if (s->replacement) lower_expr(c, s->replacement);
            else                sm_emit(p, SM_PUSH_NULL);

            if (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD) {
                sm_emit_s(p, SM_STORE_VAR, s->subject->sval ? s->subject->sval : "");
            } else if (s->subject->kind == AST_INDIRECT) {
                lower_expr(c, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                sm_emit_si(p, SM_CALL_FN, "ASGN_INDIR", 2);
            } else if (s->subject->kind == AST_IDX) {
                int nc = s->subject->nchildren;
                for (int ci = 0; ci < nc; ci++) lower_expr(c, s->subject->children[ci]);
                sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(nc + 1));
            } else if (s->subject->kind == AST_FNC && s->subject->sval) {
                if (s->subject->nchildren == 0) {
                    /* Zero-arg LHS: NRETURN path — fn returns DT_N, we write through. */
                    sm_emit_si(p, SM_CALL_FN, "NRETURN_ASGN", 1);
                    p->instrs[p->count - 1].a[1].s = GC_strdup(s->subject->sval);
                } else {
                    if (strcasecmp(s->subject->sval, "ITEM") == 0) {
                        int nc = s->subject->nchildren;
                        for (int ci = 0; ci < nc; ci++) lower_expr(c, s->subject->children[ci]);
                        sm_emit_si(p, SM_CALL_FN, "ITEM_SET", (int64_t)(nc + 1));
                    } else {
                        lower_expr(c, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                        char _setname[256];
                        snprintf(_setname, sizeof(_setname), "%s_SET", s->subject->sval);
                        sm_emit_si(p, SM_CALL_FN, _setname, 2);
                    }
                }
            } else {
                lower_expr(c, s->subject);
                sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
            }
        } else {
            /* Bare expression statement.
             * SNOBOL4 special case: bare RETURN / FRETURN / NRETURN with no
             * assignment is equivalent to :(RETURN) — emit the return opcode. */
            if (s->subject->kind == AST_VAR && s->subject->sval) {
                if (strcasecmp(s->subject->sval, "RETURN") == 0)  { sm_emit(p, SM_RETURN);  goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "FRETURN") == 0) { sm_emit(p, SM_FRETURN); goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "NRETURN") == 0) { sm_emit(p, SM_NRETURN); goto emit_gotos; }
            }
            lower_expr(c, s->subject);
            sm_emit(p, SM_VOID_POP);
        }
    }

emit_gotos: {
    if (!s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) return;
    if (s->goto_u && s->goto_u[0]) { emit_goto(c, SM_JUMP, s->goto_u); return; }
    if (s->goto_u_expr) {
        sm_emit_s(p, SM_PUSH_LIT_S, "(computed-goto)");
        sm_emit(p, SM_JUMP_INDIR);
        return;
    }
    if (s->goto_s && s->goto_s[0]) emit_goto(c, SM_JUMP_S, s->goto_s);
    if (s->goto_f && s->goto_f[0]) emit_goto(c, SM_JUMP_F, s->goto_f);
    }
}
