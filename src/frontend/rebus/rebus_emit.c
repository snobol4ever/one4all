#include "rebus.h"
#include "../../ast/ast.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-RB-5d: rebus_emit rewritten to walk tree_t via body_tree/initial_tree. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int rb_label      = 0;
static int rb_loop_top[64];
static int rb_loop_end[64];
static int rb_loop_depth = 0;
static const char *rb_current_func = "";
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int  next_label(void) { return ++rb_label; }
static void push_loop(int top, int end) { if (rb_loop_depth < 64) { rb_loop_top[rb_loop_depth] = top; rb_loop_end[rb_loop_depth] = end; rb_loop_depth++; } }
static void pop_loop(void) { if (rb_loop_depth > 0) rb_loop_depth--; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_tree_expr(tree_t *e, FILE *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_tree_expr(tree_t *e, FILE *out) {
    if (!e) return;
    switch (e->t) {
    case TT_NUL:     fprintf(out, "''"); break;
    case TT_ILIT:    fprintf(out, "%ld", e->v.ival); break;
    case TT_FLIT:    fprintf(out, "%g",  e->v.dval); break;
    case TT_VAR:     fprintf(out, "%s",  e->v.sval ? e->v.sval : "?"); break;
    case TT_KEYWORD: fprintf(out, "&%s", e->v.sval ? e->v.sval : ""); break;
    case TT_QLIT:
        fprintf(out, "'");
        for (const char *p = e->v.sval ? e->v.sval : ""; *p; p++) {
            if (*p == '\'') fprintf(out, "''");
            else            fputc(*p, out);
        }
        fprintf(out, "'");
        break;
    case TT_MNS:     fprintf(out, "-("); emit_tree_expr(e->c[0], out); fprintf(out, ")"); break;
    case TT_NOT:     fprintf(out, "\\("); emit_tree_expr(e->c[0], out); fprintf(out, ")"); break;
    case TT_NONNULL: fprintf(out, "/("); emit_tree_expr(e->c[0], out); fprintf(out, ")"); break;
    case TT_ITERATE: fprintf(out, "!("); emit_tree_expr(e->c[0], out); fprintf(out, ")"); break;
    case TT_INDIRECT:fprintf(out, "$("); emit_tree_expr(e->c[0], out); fprintf(out, ")"); break;
    case TT_ARBNO:   fprintf(out, "~("); emit_tree_expr(e->c[0], out); fprintf(out, ")"); break;
    case TT_CAPT_CURSOR: fprintf(out, "@%s", e->v.sval ? e->v.sval : "?"); break;
    case TT_ADD:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," + ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_SUB:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," - ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_MUL:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," * ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_DIV:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," / ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_MOD:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," %% ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_POW:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," ** ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_CAT:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," || ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_ALT:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," | ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_EQ:      fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," = ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_NE:      fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," ~= ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LT:      fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," < ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LE:      fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," <= ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_GT:      fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," > ");   emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_GE:      fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," >= ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LEQ:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," == ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LNE:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," ~== "); emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LLT:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," << ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LLE:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," <<= "); emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LGT:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," >> ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_LGE:     fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," >>= "); emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_ASSIGN:  fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," := ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_SWAP:    fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," :=: "); emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_CAPT_COND_ASGN:  fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," . ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_CAPT_IMMED_ASGN: fprintf(out, "("); emit_tree_expr(e->c[0],out); fprintf(out," $ ");  emit_tree_expr(e->c[1],out); fprintf(out,")"); break;
    case TT_IDX:
        emit_tree_expr(e->c[0], out);
        fprintf(out, "[");
        for (int i = 1; i < e->n; i++) { if (i > 1) fprintf(out, ","); emit_tree_expr(e->c[i], out); }
        fprintf(out, "]");
        break;
    case TT_FNC:
        if (e->v.sval) fprintf(out, "%s", e->v.sval);
        fprintf(out, "(");
        for (int i = 0; i < e->n; i++) { if (i) fprintf(out, ","); emit_tree_expr(e->c[i], out); }
        fprintf(out, ")");
        break;
    default:
        fprintf(out, "<expr:%d>", e->t);
        break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_tree_stmt(tree_t *s, FILE *out, int depth);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void ind(int depth, FILE *out) { for (int i = 0; i < depth; i++) fprintf(out, "    "); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_tree_stmt(tree_t *s, FILE *out, int depth) {
    if (!s) return;
    switch (s->t) {
    case TT_PROGRAM:
        for (int i = 0; i < s->n; i++) emit_tree_stmt(s->c[i], out, depth);
        break;
    case TT_SCAN: {
        ind(depth, out);
        emit_tree_expr(s->c[0], out);
        fprintf(out, " ? ");
        emit_tree_expr(s->c[1], out);
        if (s->n > 2) { fprintf(out, " := "); emit_tree_expr(s->c[2], out); }
        fprintf(out, "\n");
        break;
    }
    case TT_IF: {
        int L_else = next_label();
        int L_end  = next_label();
        ind(depth, out); fprintf(out, "    :(%d)\n", L_else);
        ind(depth, out); emit_tree_expr(s->c[0], out); fprintf(out, "     :S(%d) :F(%d)\n", L_end, L_else);
        if (s->n > 1) emit_tree_stmt(s->c[1], out, depth);
        if (s->n > 2) {
            ind(depth, out); fprintf(out, "     :(%d)\n", L_end);
            ind(depth, out); fprintf(out, ":(%d)\n", L_else);
            emit_tree_stmt(s->c[2], out, depth);
        } else {
            ind(depth, out); fprintf(out, ":(%d) :(%d)\n", L_end, L_else);
        }
        break;
    }
    case TT_WHILE: {
        int L_top = next_label();
        int L_end = next_label();
        push_loop(L_top, L_end);
        ind(depth, out); fprintf(out, ":(%d)\n", L_top);
        ind(depth, out); emit_tree_expr(s->c[0], out); fprintf(out, "     :F(%d)\n", L_end);
        if (s->n > 1) emit_tree_stmt(s->c[1], out, depth);
        ind(depth, out); fprintf(out, "     :(%d)\n", L_top);
        ind(depth, out); fprintf(out, ":(%d)\n", L_end);
        pop_loop();
        break;
    }
    case TT_UNTIL: {
        int L_top = next_label();
        int L_end = next_label();
        push_loop(L_top, L_end);
        ind(depth, out); fprintf(out, ":(%d)\n", L_top);
        ind(depth, out); emit_tree_expr(s->c[0], out); fprintf(out, "     :S(%d)\n", L_end);
        if (s->n > 1) emit_tree_stmt(s->c[1], out, depth);
        ind(depth, out); fprintf(out, "     :(%d)\n", L_top);
        ind(depth, out); fprintf(out, ":(%d)\n", L_end);
        pop_loop();
        break;
    }
    case TT_REPEAT: {
        int L_top = next_label();
        int L_end = next_label();
        push_loop(L_top, L_end);
        ind(depth, out); fprintf(out, ":(%d)\n", L_top);
        if (s->n > 0) emit_tree_stmt(s->c[0], out, depth);
        ind(depth, out); fprintf(out, "     :(%d)\n", L_top);
        ind(depth, out); fprintf(out, ":(%d)\n", L_end);
        pop_loop();
        break;
    }
    case TT_FOR: {
        int L_top = next_label();
        int L_end = next_label();
        push_loop(L_top, L_end);
        const char *var = s->v.sval ? s->v.sval : "_i";
        ind(depth, out); fprintf(out, "%s = ", var); emit_tree_expr(s->c[0], out); fprintf(out, "\n");
        ind(depth, out); fprintf(out, ":(%d)\n", L_top);
        ind(depth, out); fprintf(out, "GT(%s,", var); emit_tree_expr(s->c[1], out); fprintf(out, ")  :S(%d)\n", L_end);
        if (s->n > 3) emit_tree_stmt(s->c[3], out, depth);
        ind(depth, out);
        if (s->n > 2 && s->c[2] && s->c[2]->t != TT_NUL) {
            fprintf(out, "%s = %s + ", var, var); emit_tree_expr(s->c[2], out); fprintf(out, "  :(%d)\n", L_top);
        } else {
            fprintf(out, "%s = %s + 1  :(%d)\n", var, var, L_top);
        }
        ind(depth, out); fprintf(out, ":(%d)\n", L_end);
        pop_loop();
        break;
    }
    case TT_CASE: {
        int L_end = next_label();
        for (int i = 1; i < s->n; i++) {
            tree_t *cl = s->c[i];
            if (cl->t != TT_IF) continue;
            if (cl->c[0]->t == TT_NUL) {
                if (cl->n > 1) emit_tree_stmt(cl->c[1], out, depth);
                ind(depth, out); fprintf(out, "     :(%d)\n", L_end);
            } else {
                int L_next = next_label();
                ind(depth, out);
                fprintf(out, "IDENT("); emit_tree_expr(s->c[0], out); fprintf(out, ","); emit_tree_expr(cl->c[0], out); fprintf(out, ")  :F(%d)\n", L_next);
                if (cl->n > 1) emit_tree_stmt(cl->c[1], out, depth);
                ind(depth, out); fprintf(out, "     :(%d)\n", L_end);
                ind(depth, out); fprintf(out, ":(%d)\n", L_next);
            }
        }
        ind(depth, out); fprintf(out, ":(%d)\n", L_end);
        break;
    }
    case TT_RETURN: {
        if (s->n > 0 && rb_current_func && *rb_current_func) {
            ind(depth, out);
            fprintf(out, "%s = ", rb_current_func);
            emit_tree_expr(s->c[0], out);
            fprintf(out, "\n");
        }
        ind(depth, out); fprintf(out, "     :(RETURN)\n");
        break;
    }
    case TT_LOOP_BREAK:
        ind(depth, out);
        if (rb_loop_depth > 0) fprintf(out, "     :(%d)\n", rb_loop_end[rb_loop_depth - 1]);
        else                   fprintf(out, "END\n");
        break;
    case TT_LOOP_NEXT:
        ind(depth, out);
        if (rb_loop_depth > 0) fprintf(out, "     :(%d)\n", rb_loop_top[rb_loop_depth - 1]);
        break;
    case TT_PROC_FAIL:
        ind(depth, out); fprintf(out, "     :(FRETURN)\n");
        break;
    case TT_END:
        ind(depth, out); fprintf(out, "END\n");
        break;
    default: {
        ind(depth, out);
        if (s->t == TT_ASSIGN && s->n >= 2) {
            emit_tree_expr(s->c[0], out); fprintf(out, " = "); emit_tree_expr(s->c[1], out);
        } else {
            emit_tree_expr(s, out);
        }
        fprintf(out, "\n");
        break;
    }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_decl(RDecl *d, FILE *out) {
    if (!d) return;
    switch (d->kind) {
    case RD_RECORD: {
        fprintf(out, "        DATA('%s(", d->name ? d->name : "?");
        for (int i = 0; i < d->nfields; i++) { if (i) fprintf(out, ","); fprintf(out, "%s", d->fields[i]); }
        fprintf(out, ")')\n");
        break;
    }
    case RD_FUNCTION: {
        fprintf(out, "        DEFINE('%s(", d->name ? d->name : "?");
        for (int i = 0; i < d->nparams; i++) { if (i) fprintf(out, ","); fprintf(out, "%s", d->params[i]); }
        fprintf(out, ")");
        if (d->nlocals > 0) {
            fprintf(out, "/");
            for (int i = 0; i < d->nlocals; i++) { if (i) fprintf(out, ","); fprintf(out, "%s", d->locals[i]); }
        }
        fprintf(out, "')\n");
        int skip = next_label();
        fprintf(out, "     :(%d)\n", skip);
        fprintf(out, "%s\n", d->name ? d->name : "?");
        const char *saved = rb_current_func;
        rb_current_func = d->name ? d->name : "";
        if (d->initial_tree) {
            int flag_lab = next_label();
            int done_lab = next_label();
            fprintf(out, "        _INIT_%s_%d  :S(%d)\n", d->name ? d->name : "f", flag_lab, done_lab);
            emit_tree_stmt(d->initial_tree, out, 2);
            fprintf(out, "        _INIT_%s_%d = 1\n", d->name ? d->name : "f", flag_lab);
            fprintf(out, ":(%d)\n", done_lab);
        }
        if (d->body_tree) emit_tree_stmt(d->body_tree, out, 2);
        fprintf(out, "     :(RETURN)\n");
        fprintf(out, ":(%d)\n", skip);
        rb_current_func = saved;
        break;
    }
    }
    if (d->next) emit_decl(d->next, out);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_emit(RProgram *prog, FILE *out) {
    if (!prog) return;
    rb_label = 0;
    rb_loop_depth = 0;
    rb_current_func = "";
    emit_decl(prog->decls, out);
    fprintf(out, "END\n");
}
