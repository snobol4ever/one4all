#include "rebus.h"
#include "../../ast/ast.h"
#include <stdio.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-RB-5c: rebus_print now walks tree_t via body_tree/initial_tree. RExpr/RStmt deleted. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void indent(FILE *out, int depth) {
    for (int i = 0; i < depth; i++) fprintf(out, "  ");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_tree(tree_t *t, FILE *out, int depth);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_tree(tree_t *t, FILE *out, int depth) {
    if (!t) { fprintf(out, "<null>"); return; }
    switch (t->t) {
    case TT_QLIT:    fprintf(out, "\"%s\"", t->v.sval ? t->v.sval : ""); break;
    case TT_ILIT:    fprintf(out, "%ld", t->v.ival); break;
    case TT_FLIT:    fprintf(out, "%g", t->v.dval); break;
    case TT_NUL:     fprintf(out, "\"\""); break;
    case TT_VAR:     fprintf(out, "%s", t->v.sval ? t->v.sval : "?"); break;
    case TT_KEYWORD: fprintf(out, "&%s", t->v.sval ? t->v.sval : ""); break;
    case TT_MNS:     fprintf(out, "-("); print_tree(t->c[0], out, 0); fprintf(out, ")"); break;
    case TT_NOT:     fprintf(out, "~("); print_tree(t->c[0], out, 0); fprintf(out, ")"); break;
    case TT_NONNULL: fprintf(out, "/("); print_tree(t->c[0], out, 0); fprintf(out, ")"); break;
    case TT_ITERATE: fprintf(out, "!("); print_tree(t->c[0], out, 0); fprintf(out, ")"); break;
    case TT_INDIRECT:fprintf(out, "$("); print_tree(t->c[0], out, 0); fprintf(out, ")"); break;
    case TT_ARBNO:   fprintf(out, "~("); print_tree(t->c[0], out, 0); fprintf(out, ")"); break;
    case TT_CAPT_CURSOR: fprintf(out, "@%s", t->v.sval ? t->v.sval : "?"); break;
    case TT_ADD: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," + "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_SUB: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," - "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_MUL: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," * "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_DIV: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," / "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_MOD: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," %% "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_POW: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," ^ "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_CAT: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," || "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_ALT: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," | "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_EQ:  fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," = "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_NE:  fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," ~= "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LT:  fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," < "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LE:  fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," <= "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_GT:  fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," > "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_GE:  fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," >= "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LEQ: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," == "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LNE: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," ~== "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LLT: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," << "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LLE: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," <<= "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LGT: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," >> "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_LGE: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," >>= "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_ASSIGN: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," := "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_SWAP:   fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," :=: "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_CAPT_COND_ASGN:  fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," . "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_CAPT_IMMED_ASGN: fprintf(out, "("); print_tree(t->c[0],out,0); fprintf(out," $ "); print_tree(t->c[1],out,0); fprintf(out,")"); break;
    case TT_IDX: {
        print_tree(t->c[0], out, 0);
        fprintf(out, "[");
        for (int i = 1; i < t->n; i++) { if (i > 1) fprintf(out, ", "); print_tree(t->c[i], out, 0); }
        fprintf(out, "]");
        break;
    }
    case TT_FNC: {
        if (t->v.sval) fprintf(out, "%s", t->v.sval);
        fprintf(out, "(");
        for (int i = 0; i < t->n; i++) { if (i) fprintf(out, ", "); print_tree(t->c[i], out, 0); }
        fprintf(out, ")");
        break;
    }
    case TT_SCAN: {
        print_tree(t->c[0], out, 0);
        fprintf(out, " ? ");
        print_tree(t->c[1], out, 0);
        if (t->n > 2) { fprintf(out, " <- "); print_tree(t->c[2], out, 0); }
        break;
    }
    case TT_PROGRAM: {
        for (int i = 0; i < t->n; i++) {
            indent(out, depth);
            print_tree(t->c[i], out, depth);
            fprintf(out, ";\n");
        }
        break;
    }
    case TT_IF: {
        indent(out, depth);
        fprintf(out, "if ");
        print_tree(t->c[0], out, 0);
        fprintf(out, " then\n");
        if (t->n > 1) print_tree(t->c[1], out, depth + 1);
        if (t->n > 2) { indent(out, depth); fprintf(out, "else\n"); print_tree(t->c[2], out, depth + 1); }
        break;
    }
    case TT_WHILE: {
        indent(out, depth);
        fprintf(out, "while ");
        print_tree(t->c[0], out, 0);
        fprintf(out, " do\n");
        if (t->n > 1) print_tree(t->c[1], out, depth + 1);
        break;
    }
    case TT_UNTIL: {
        indent(out, depth);
        fprintf(out, "until ");
        print_tree(t->c[0], out, 0);
        fprintf(out, " do\n");
        if (t->n > 1) print_tree(t->c[1], out, depth + 1);
        break;
    }
    case TT_REPEAT: {
        indent(out, depth);
        fprintf(out, "repeat\n");
        if (t->n > 0) print_tree(t->c[0], out, depth + 1);
        break;
    }
    case TT_FOR: {
        indent(out, depth);
        fprintf(out, "for %s from ", t->v.sval ? t->v.sval : "?");
        if (t->n > 0) print_tree(t->c[0], out, 0);
        fprintf(out, " to ");
        if (t->n > 1) print_tree(t->c[1], out, 0);
        if (t->n > 2 && t->c[2] && t->c[2]->t != TT_NUL) { fprintf(out, " by "); print_tree(t->c[2], out, 0); }
        fprintf(out, " do\n");
        if (t->n > 3) print_tree(t->c[3], out, depth + 1);
        break;
    }
    case TT_CASE: {
        indent(out, depth);
        fprintf(out, "case ");
        print_tree(t->c[0], out, 0);
        fprintf(out, " of {\n");
        for (int i = 1; i < t->n; i++) {
            tree_t *cl = t->c[i];
            indent(out, depth + 1);
            if (cl->t == TT_IF) {
                if (cl->c[0]->t == TT_NUL) fprintf(out, "default");
                else print_tree(cl->c[0], out, 0);
                fprintf(out, " :\n");
                if (cl->n > 1) print_tree(cl->c[1], out, depth + 2);
            }
        }
        indent(out, depth);
        fprintf(out, "}\n");
        break;
    }
    case TT_RETURN: {
        indent(out, depth);
        fprintf(out, "return");
        if (t->n > 0) { fprintf(out, " "); print_tree(t->c[0], out, 0); }
        fprintf(out, "\n");
        break;
    }
    case TT_LOOP_BREAK: indent(out, depth); fprintf(out, "exit\n"); break;
    case TT_LOOP_NEXT:  indent(out, depth); fprintf(out, "next\n"); break;
    case TT_PROC_FAIL:  indent(out, depth); fprintf(out, "fail\n"); break;
    case TT_END:        indent(out, depth); fprintf(out, "stop\n"); break;
    default:
        fprintf(out, "<tree:%d>", t->t);
        break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_decl(RDecl *d, FILE *out) {
    if (!d) return;
    switch (d->kind) {
    case RD_RECORD:
        fprintf(out, "record %s(", d->name ? d->name : "?");
        for (int i = 0; i < d->nfields; i++) { if (i) fprintf(out, ", "); fprintf(out, "%s", d->fields[i] ? d->fields[i] : "?"); }
        fprintf(out, ")\n\n");
        break;
    case RD_FUNCTION:
        fprintf(out, "function %s(", d->name ? d->name : "?");
        for (int i = 0; i < d->nparams; i++) { if (i) fprintf(out, ", "); fprintf(out, "%s", d->params[i] ? d->params[i] : "?"); }
        fprintf(out, ")");
        if (d->nlocals > 0) {
            fprintf(out, "\n  local ");
            for (int i = 0; i < d->nlocals; i++) { if (i) fprintf(out, ", "); fprintf(out, "%s", d->locals[i] ? d->locals[i] : "?"); }
        }
        if (d->initial_tree) { fprintf(out, "\n  initial "); print_tree(d->initial_tree, out, 0); }
        fprintf(out, "\n");
        if (d->body_tree) print_tree(d->body_tree, out, 1);
        fprintf(out, "end\n\n");
        break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_print(RProgram *prog, FILE *out) {
    if (!prog) { fprintf(out, "<null program>\n"); return; }
    for (RDecl *d = prog->decls; d; d = d->next)
        print_decl(d, out);
}
