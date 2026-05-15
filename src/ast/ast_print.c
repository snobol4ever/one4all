#define IR_DEFINE_NAMES
#include "scrip_cc.h"
/*================================================================================================================================================================================*/
#define AST_PRINT_MAX_DEPTH 64
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_escaped(const char * s, FILE * f) {
    if (!s) { fputs("(null)", f); return; }
    fputc('"', f);
    for (const char * p = s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n",  f); break;
        case '\r': fputs("\\r",  f); break;
        case '\t': fputs("\\t",  f); break;
        default:
            if ((unsigned char)*p < 0x20) fprintf(f, "\\x%02x", (unsigned char)*p);
            else fputc(*p, f);
        }
    }
    fputc('"', f);
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_indent(int depth, FILE * f) {
    for (int i = 0; i < depth * 2; i++) fputc(' ', f);
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void print_node(const tree_t * e, FILE * f, int depth) {
    const char * kname;
    int i;
    if (!e)                          { fputs("(null)", f); return; }
    if (depth > AST_PRINT_MAX_DEPTH) { fputs("(...)", f);  return; }
    kname = (e->t >= 0 && e->t < TT_KIND_COUNT) ? tt_e_name[e->t] : "E_???";
    switch (e->t) {
    case TT_QLIT:
    case TT_CSET:
        fputc('(', f); fputs(kname, f); fputc(' ', f); print_escaped(e->v.sval, f); fputc(')', f);
        return;
    case TT_ILIT:
        fprintf(f, "(%s %lld)", kname, (long long)e->v.ival);
        return;
    case TT_FLIT:
        fprintf(f, "(%s %g)", kname, e->v.dval);
        return;
    case TT_NUL:
        fprintf(f, "(%s)", kname);
        return;
    case TT_VAR: case TT_KEYWORD: case TT_FNC: case TT_IDX:
    case TT_CAPT_COND_ASGN: case TT_CAPT_IMMED_ASGN: case TT_CAPT_CURSOR:
        if (e->n == 0) {
            fputc('(', f); fputs(kname, f);
            if (e->v.sval) { fputc(' ', f); fputs(e->v.sval, f); }
            fputc(')', f);
            return;
        }
        break;
    case TT_ARB: case TT_REM: case TT_FAIL: case TT_SUCCEED:
    case TT_FENCE: case TT_ABORT: case TT_BAL:
        if (e->n == 0) { fprintf(f, "(%s)", kname); return; }
        break;
    default:
        break;
    }
    fputc('(', f);
    fputs(kname, f);
    if (e->v.sval && e->t != TT_QLIT && e->t != TT_CSET) { fputc(' ', f); fputs(e->v.sval, f); }
    if (e->n == 0) { fputc(')', f); return; }
    if (e->n == 1) {
        fputc(' ', f);
        print_node(e->c[0], f, depth + 1);
        fputc(')', f);
    } else {
        for (i = 0; i < e->n; i++) {
            fputc('\n', f);
            print_indent(depth + 1, f);
            print_node(e->c[i], f, depth + 1);
        }
        fputc('\n', f);
        print_indent(depth, f);
        fputc(')', f);
    }
}
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void ir_print_node(const tree_t * e, FILE * f)    { print_node(e, f, 0); }
void ir_print_node_nl(const tree_t * e, FILE * f)  { print_node(e, f, 0); fputc('\n', f); }
/*================================================================================================================================================================================*/
#ifdef AST_PRINT_TEST
#include <stdlib.h>
static tree_t * mk(tree_e k)                            { tree_t * e = calloc(1, sizeof *e); e->t = k; return e; }
static void add_child(tree_t * parent, tree_t * child)  { parent->c = realloc(parent->c, (size_t)(parent->n + 1) * sizeof(tree_t *)); parent->c[parent->n++] = child; }
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(void) {
    tree_t * root   = mk(TT_SEQ);
    tree_t * lit    = mk(TT_QLIT);   lit->v.sval  = "hello";
    tree_t * var    = mk(TT_VAR);    var->v.sval  = "x";
    tree_t * num    = mk(TT_ILIT);   num->v.ival  = 42;
    tree_t * assign = mk(TT_ASSIGN);
    tree_t * lhs    = mk(TT_VAR);    lhs->v.sval  = "result";
    tree_t * add    = mk(TT_ADD);
    tree_t * one    = mk(TT_ILIT);   one->v.ival  = 1;
    tree_t * two    = mk(TT_ILIT);   two->v.ival  = 2;
    tree_t * fnc    = mk(TT_FNC);    fnc->v.sval  = "LENGTH";
    tree_t * arg    = mk(TT_VAR);    arg->v.sval  = "s";
    tree_t * alt    = mk(TT_ALT);
    tree_t * foo    = mk(TT_QLIT);   foo->v.sval  = "foo";
    tree_t * span   = mk(TT_SPAN);   span->v.sval = "abc";
    add_child(root, lit); add_child(root, var); add_child(root, num);
    add_child(add, one);  add_child(add, two);
    add_child(assign, lhs); add_child(assign, add);
    add_child(fnc, arg);
    add_child(alt, foo); add_child(alt, span);
    fputs("=== ast_print unit test ===\n\n", stdout);
    fputs("1. TT_SEQ:\n",         stdout); ir_print_node_nl(root,       stdout);
    fputs("\n2. TT_ASSIGN:\n",    stdout); ir_print_node_nl(assign,     stdout);
    fputs("\n3. TT_FNC:\n",       stdout); ir_print_node_nl(fnc,        stdout);
    fputs("\n4. TT_ALT:\n",       stdout); ir_print_node_nl(alt,        stdout);
    fputs("\n5. TT_NUL leaf:\n",  stdout); ir_print_node_nl(mk(TT_NUL), stdout);
    fputs("\n6. TT_FAIL leaf:\n", stdout); ir_print_node_nl(mk(TT_FAIL),stdout);
    fputs("\n=== PASS ===\n",     stdout);
    return 0;
}
#endif
