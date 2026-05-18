#include "prolog_builtin.h"
#include "prolog_atom.h"
#include "prolog_runtime.h"
#include "term.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_op_prec(const char *name, int arity) {
    struct { const char *n; int a; int p; } tbl[] = {
        {":-",2,1200},{";",2,1100},{"->",2,1050},{",",2,1000},
        {"=",2,700},{"\\=",2,700},{"is",2,700},{"=:=",2,700},{"=\\=",2,700},
        {"<",2,700},{">",2,700},{"=<",2,700},{">=",2,700},
        {"==",2,700},{"\\==",2,700},{"@<",2,700},{"@>",2,700},
        {"@=<",2,700},{"@>=",2,700},{"=..",2,700},
        {"+",2,500},{"-",2,500},
        {"*",2,400},{"/",2,400},{"//",2,400},{"mod",2,400},
        {"rem",2,400},{"<<",2,400},{">>",2,400},
        {"**",2,200},{"^",2,200},
        {"-",1,200},{"\\+",1,900},{"not",1,900},
        {NULL,0,0}
    };
    for (int i = 0; tbl[i].n; i++)
        if (tbl[i].a == arity && strcmp(tbl[i].n, name) == 0) return tbl[i].p;
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void pl_write(Term *t) {
    t = term_deref(t);
    if (!t) { printf("[]"); return; }
    switch (t->tag) {
        case TERM_ATOM: {
            const char *name = prolog_atom_name(t->atom_id);
            if (!name) name = "?";
            int needs_quote = 0;
            if (name[0] && (isupper((unsigned char)name[0]) || name[0] == '_'))
                needs_quote = 1;
            for (const char *p = name; *p && !needs_quote; p++)
                if (!isalnum((unsigned char)*p) && *p != '_')
                    needs_quote = 1;
            if (needs_quote && name[0] != '[') {
            }
            printf("%s", name);
            break;
        }
        case TERM_VAR:
            printf("_G%d", t->var_slot);
            break;
        case TERM_INT:
            printf("%ld", t->ival);
            break;
        case TERM_FLOAT: {
            double fv = t->fval;
            if (fv == (long)fv && fv >= -1e15 && fv <= 1e15)
                printf("%.1f", fv);
            else
                printf("%g", fv);
            break;
        }
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            if (strcmp(fn, "$VAR") == 0 && t->compound.arity == 1) {
                Term *n = term_deref(t->compound.args[0]);
                if (n && n->tag == TERM_INT) {
                    long num = n->ival;
                    int letter = (int)(num % 26);
                    long suffix = num / 26;
                    if (suffix == 0) printf("%c", 'A' + letter);
                    else            printf("%c%ld", 'A' + letter, suffix);
                    break;
                }
            }
            if (t->compound.functor == ATOM_DOT && t->compound.arity == 2) {
                printf("[");
                pl_write(t->compound.args[0]);
                Term *tail = term_deref(t->compound.args[1]);
                while (tail && tail->tag == TERM_COMPOUND &&
                       tail->compound.functor == ATOM_DOT &&
                       tail->compound.arity == 2) {
                    printf(",");
                    pl_write(tail->compound.args[0]);
                    tail = term_deref(tail->compound.args[1]);
                }
                if (tail && tail->tag == TERM_ATOM && tail->atom_id == ATOM_NIL) {
                } else {
                    printf("|");
                    pl_write(tail);
                }
                printf("]");
                break;
            }
            struct { const char *name; int arity; int prec; int right_assoc; } ops[] = {
                {":-",2,1200,1}, {";",2,1100,1}, {"->",2,1050,1},
                {",",2,1000,1},
                {"=",2,700,0},{"\\=",2,700,0},{"is",2,700,0},
                {"=:=",2,700,0},{"=\\=",2,700,0},
                {"<",2,700,0},{">",2,700,0},{"=<",2,700,0},{">=",2,700,0},
                {"==",2,700,0},{"\\==",2,700,0},
                {"@<",2,700,0},{"@>",2,700,0},{"@=<",2,700,0},{"@>=",2,700,0},
                {"=..",2,700,0},
                {"+",2,500,0},{"-",2,500,0},
                {"*",2,400,0},{"/",2,400,0},{"//",2,400,0},{"mod",2,400,0},
                {"rem",2,400,0},{"<<",2,400,0},{">>",2,400,0},
                {"**",2,200,1},{"^",2,200,1},
                {"-",1,200,0},{"\\+",1,900,0},{"not",1,900,0},
                {NULL,0,0,0}
            };
            int is_op = 0;
            for (int i = 0; ops[i].name; i++) {
                if (strcmp(fn, ops[i].name) == 0 && t->compound.arity == ops[i].arity) {
                    is_op = 1;
                    if (ops[i].arity == 2) {
                        Term *larg = term_deref(t->compound.args[0]);
                        Term *rarg = term_deref(t->compound.args[1]);
                        int lp = -1, rp = -1;
                        if (larg && larg->tag == TERM_COMPOUND) {
                            const char *lfn = prolog_atom_name(larg->compound.functor);
                            if (lfn) lp = pl_op_prec(lfn, larg->compound.arity);
                        }
                        if (rarg && rarg->tag == TERM_COMPOUND) {
                            const char *rfn = prolog_atom_name(rarg->compound.functor);
                            if (rfn) rp = pl_op_prec(rfn, rarg->compound.arity);
                        }
                        int my_prec = ops[i].prec;
                        int lneed = (lp > my_prec) || (lp == my_prec && ops[i].right_assoc);
                        int rneed = (rp > my_prec) || (rp == my_prec && !ops[i].right_assoc);
                        if (lneed) printf("(");
                        pl_write(t->compound.args[0]);
                        if (lneed) printf(")");
                        if (isalpha((unsigned char)fn[0])) printf(" %s ", fn);
                        else printf("%s", fn);
                        if (rneed) printf("(");
                        pl_write(t->compound.args[1]);
                        if (rneed) printf(")");
                    } else {
                        Term *arg = term_deref(t->compound.args[0]);
                        int ap = -1;
                        if (arg && arg->tag == TERM_COMPOUND) {
                            const char *afn = prolog_atom_name(arg->compound.functor);
                            if (afn) ap = pl_op_prec(afn, arg->compound.arity);
                        }
                        int aneed = (ap >= ops[i].prec);
                        if (isalpha((unsigned char)fn[0])) printf("%s ", fn);
                        else printf("%s", fn);
                        if (aneed) printf("(");
                        pl_write(t->compound.args[0]);
                        if (aneed) printf(")");
                    }
                    break;
                }
            }
            if (!is_op) {
                printf("%s(", fn);
                for (int i = 0; i < t->compound.arity; i++) {
                    if (i) printf(",");
                    pl_write(t->compound.args[i]);
                }
                printf(")");
            }
            break;
        }
        case TERM_REF:
            pl_write(t->ref);
            break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int atom_needs_quoting(const char *name) {
    if (!name || !name[0]) return 1;
    if (name[0] == '[' && strcmp(name,"[]")==0) return 0;
    if (name[0] == '{' && strcmp(name,"{}")==0) return 0;
    if (isupper((unsigned char)name[0]) || name[0] == '_') return 1;
    int all_graphic = 1;
    static const char *graphic = "#&*+-./:<=>?@\\^~";
    for (const char *p = name; *p; p++)
        if (!strchr(graphic, *p)) { all_graphic = 0; break; }
    if (all_graphic) return 0;
    if (islower((unsigned char)name[0])) {
        for (const char *p = name+1; *p; p++)
            if (!isalnum((unsigned char)*p) && *p != '_') return 1;
        return 0;
    }
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pl_writeq_term(Term *t) {
    t = term_deref(t);
    if (!t) { printf("'[]'"); return; }
    switch (t->tag) {
        case TERM_ATOM: {
            const char *name = prolog_atom_name(t->atom_id);
            if (!name) name = "?";
            if (atom_needs_quoting(name)) {
                putchar('\'');
                for (const char *p = name; *p; p++) {
                    if (*p == '\'') putchar('\'');
                    putchar(*p);
                }
                putchar('\'');
            } else {
                printf("%s", name);
            }
            break;
        }
        case TERM_VAR:
            printf("_G%d", t->var_slot);
            break;
        case TERM_INT:
            printf("%ld", t->ival);
            break;
        case TERM_FLOAT: {
            double fv = t->fval;
            if (fv == (long)fv && fv >= -1e15 && fv <= 1e15) printf("%.1f", fv);
            else printf("%g", fv);
            break;
        }
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            if (strcmp(fn,"$VAR")==0 && t->compound.arity==1) {
                Term *n = term_deref(t->compound.args[0]);
                if (n && n->tag == TERM_INT) {
                    long num = n->ival; int letter=(int)(num%26); long suf=num/26;
                    if (suf==0) printf("%c",'A'+letter); else printf("%c%ld",'A'+letter,suf);
                    break;
                }
            }
            if (t->compound.functor == ATOM_DOT && t->compound.arity == 2) {
                printf("["); pl_writeq_term(t->compound.args[0]);
                Term *tail = term_deref(t->compound.args[1]);
                while (tail && tail->tag==TERM_COMPOUND && tail->compound.functor==ATOM_DOT && tail->compound.arity==2) {
                    printf(","); pl_writeq_term(tail->compound.args[0]);
                    tail = term_deref(tail->compound.args[1]);
                }
                if (!(tail && tail->tag==TERM_ATOM && tail->atom_id==ATOM_NIL)) { printf("|"); pl_writeq_term(tail); }
                printf("]"); break;
            }
            struct { const char *name; int arity; int prec; int right_assoc; } ops[] = {
                {":-",2,1200,1},{";",2,1100,1},{"->",2,1050,1},{",",2,1000,1},
                {"=",2,700,0},{"\\=",2,700,0},{"is",2,700,0},
                {"=:=",2,700,0},{"=\\=",2,700,0},
                {"<",2,700,0},{">",2,700,0},{"=<",2,700,0},{">=",2,700,0},
                {"==",2,700,0},{"\\==",2,700,0},
                {"@<",2,700,0},{"@>",2,700,0},{"@=<",2,700,0},{"@>=",2,700,0},
                {"=..",2,700,0},{"+",2,500,0},{"-",2,500,0},
                {"*",2,400,0},{"/",2,400,0},{"//",2,400,0},{"mod",2,400,0},
                {"rem",2,400,0},{"<<",2,400,0},{">>",2,400,0},
                {"**",2,200,1},{"^",2,200,1},
                {"-",1,200,0},{"\\+",1,900,0},{"not",1,900,0},
                {NULL,0,0,0}
            };
            int is_op = 0;
            for (int i = 0; ops[i].name; i++) {
                if (strcmp(fn,ops[i].name)==0 && t->compound.arity==ops[i].arity) {
                    is_op = 1;
                    if (ops[i].arity==2) {
                        Term *la=term_deref(t->compound.args[0]),*ra=term_deref(t->compound.args[1]);
                        int lp=-1,rp=-1;
                        if(la&&la->tag==TERM_COMPOUND){const char*lf=prolog_atom_name(la->compound.functor);if(lf)lp=pl_op_prec(lf,la->compound.arity);}
                        if(ra&&ra->tag==TERM_COMPOUND){const char*rf=prolog_atom_name(ra->compound.functor);if(rf)rp=pl_op_prec(rf,ra->compound.arity);}
                        int my=ops[i].prec;
                        if((lp>my)||(lp==my&&ops[i].right_assoc)) { printf("("); pl_writeq_term(t->compound.args[0]); printf(")"); }
                        else pl_writeq_term(t->compound.args[0]);
                        if(isalpha((unsigned char)fn[0])) printf(" %s ",fn); else printf("%s",fn);
                        if((rp>my)||(rp==my&&!ops[i].right_assoc)) { printf("("); pl_writeq_term(t->compound.args[1]); printf(")"); }
                        else pl_writeq_term(t->compound.args[1]);
                    } else {
                        Term *arg=term_deref(t->compound.args[0]); int ap=-1;
                        if(arg&&arg->tag==TERM_COMPOUND){const char*af=prolog_atom_name(arg->compound.functor);if(af)ap=pl_op_prec(af,arg->compound.arity);}
                        if(isalpha((unsigned char)fn[0])) printf("%s ",fn); else printf("%s",fn);
                        if(ap>=ops[i].prec){printf("(");pl_writeq_term(t->compound.args[0]);printf(")");}
                        else pl_writeq_term(t->compound.args[0]);
                    }
                    break;
                }
            }
            if (!is_op) {
                if (atom_needs_quoting(fn)) {
                    putchar('\'');
                    for (const char *p=fn;*p;p++){if(*p=='\'')putchar('\'');putchar(*p);}
                    putchar('\'');
                } else { printf("%s",fn); }
                printf("(");
                for (int i=0;i<t->compound.arity;i++){if(i)printf(",");pl_writeq_term(t->compound.args[i]);}
                printf(")");
            }
            break;
        }
        default: break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void pl_writeq(Term *t) { pl_writeq_term(t); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pl_write_canonical_term(Term *t) {
    t = term_deref(t);
    if (!t) { printf("'[]'"); return; }
    switch (t->tag) {
        case TERM_ATOM: {
            const char *name = prolog_atom_name(t->atom_id);
            if (!name) name = "?";
            if (atom_needs_quoting(name)) {
                putchar('\'');
                for (const char *p=name;*p;p++){if(*p=='\'')putchar('\'');putchar(*p);}
                putchar('\'');
            } else printf("%s",name);
            break;
        }
        case TERM_VAR:  printf("_G%d",t->var_slot); break;
        case TERM_INT:  printf("%ld",t->ival); break;
        case TERM_FLOAT: {
            double fv=t->fval;
            if(fv==(long)fv&&fv>=-1e15&&fv<=1e15) printf("%.1f",fv);
            else printf("%g",fv);
            break;
        }
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            if (atom_needs_quoting(fn)) {
                putchar('\'');
                for(const char *p=fn;*p;p++){if(*p=='\'')putchar('\'');putchar(*p);}
                putchar('\'');
            } else printf("%s",fn);
            printf("(");
            for(int i=0;i<t->compound.arity;i++){
                if(i) printf(",");
                pl_write_canonical_term(t->compound.args[i]);
            }
            printf(")");
            break;
        }
        default: break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void pl_write_canonical(Term *t) { pl_write_canonical_term(t); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pl_write_to_file(Term *t, FILE *out) {
    t = term_deref(t);
    if (!t) { fprintf(out, "[]"); return; }
    switch (t->tag) {
        case TERM_ATOM: {
            const char *name = prolog_atom_name(t->atom_id);
            fprintf(out, "%s", name ? name : "?");
            break;
        }
        case TERM_VAR:   fprintf(out, "_G%d", t->var_slot); break;
        case TERM_INT:   fprintf(out, "%ld", t->ival); break;
        case TERM_FLOAT: {
            double fv = t->fval;
            if (fv == (long)fv && fv >= -1e15 && fv <= 1e15) fprintf(out, "%.1f", fv);
            else fprintf(out, "%g", fv);
            break;
        }
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            if (t->compound.functor == ATOM_DOT && t->compound.arity == 2) {
                fprintf(out, "["); pl_write_to_file(t->compound.args[0], out);
                Term *tail = term_deref(t->compound.args[1]);
                while (tail && tail->tag == TERM_COMPOUND &&
                       tail->compound.functor == ATOM_DOT && tail->compound.arity == 2) {
                    fprintf(out, ","); pl_write_to_file(tail->compound.args[0], out);
                    tail = term_deref(tail->compound.args[1]);
                }
                if (!(tail && tail->tag == TERM_ATOM && tail->atom_id == ATOM_NIL))
                    { fprintf(out, "|"); pl_write_to_file(tail, out); }
                fprintf(out, "]"); break;
            }
            fprintf(out, "%s(", fn);
            for (int i = 0; i < t->compound.arity; i++) {
                if (i) fprintf(out, ",");
                pl_write_to_file(t->compound.args[i], out);
            }
            fprintf(out, ")"); break;
        }
        default: break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
char *pl_term_to_string(Term *t) {
    char *buf = NULL; size_t sz = 0;
    FILE *f = open_memstream(&buf, &sz);
    if (!f) return strdup("?");
    pl_write_to_file(t, f);
    fclose(f);
    return buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int pl_functor(Term *t, Term *name, Term *arity, Trail *tr) {
    t     = term_deref(t);
    name  = term_deref(name);
    arity = term_deref(arity);
    if (t && t->tag != TERM_VAR) {
        Term *name_term  = NULL;
        Term *arity_term = NULL;
        switch (t->tag) {
            case TERM_ATOM:
                name_term  = term_new_atom(t->atom_id);
                arity_term = term_new_int(0);
                break;
            case TERM_INT:
                name_term  = term_new_int(t->ival);
                arity_term = term_new_int(0);
                break;
            case TERM_FLOAT:
                name_term  = term_new_float(t->fval);
                arity_term = term_new_int(0);
                break;
            case TERM_COMPOUND:
                name_term  = term_new_atom(t->compound.functor);
                arity_term = term_new_int(t->compound.arity);
                break;
            default:
                return 0;
        }
        int mark = trail_mark(tr);
        if (!unify(name, name_term, tr) || !unify(arity, arity_term, tr)) {
            trail_unwind(tr, mark);
            return 0;
        }
        return 1;
    } else {
        if (!name || name->tag == TERM_VAR) return 0;
        if (!arity || arity->tag != TERM_INT) return 0;
        long ar = arity->ival;
        Term *new_t;
        if (ar == 0) {
            if (name->tag == TERM_ATOM) new_t = term_new_atom(name->atom_id);
            else if (name->tag == TERM_INT) new_t = term_new_int(name->ival);
            else return 0;
        } else {
            if (name->tag != TERM_ATOM) return 0;
            Term **args = malloc(ar * sizeof(Term *));
            for (int i = 0; i < ar; i++) args[i] = term_new_var(i);
            new_t = term_new_compound(name->atom_id, (int)ar, args);
            free(args);
        }
        int mark = trail_mark(tr);
        if (!unify(t, new_t, tr)) {
            trail_unwind(tr, mark);
            return 0;
        }
        return 1;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int pl_arg(Term *n, Term *compound, Term *arg, Trail *tr) {
    n        = term_deref(n);
    compound = term_deref(compound);
    if (!n || n->tag != TERM_INT) return 0;
    if (!compound || compound->tag != TERM_COMPOUND) return 0;
    long idx = n->ival;
    if (idx < 1 || idx > compound->compound.arity) return 0;
    int mark = trail_mark(tr);
    if (!unify(arg, compound->compound.args[idx - 1], tr)) {
        trail_unwind(tr, mark);
        return 0;
    }
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *make_list(int n, Term **items) {
    Term *tail = term_new_atom(ATOM_NIL);
    for (int i = n - 1; i >= 0; i--) {
        Term *args[2] = { items[i], tail };
        tail = term_new_compound(ATOM_DOT, 2, args);
    }
    return tail;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int list_len(Term *t) {
    int n = 0;
    while (1) {
        t = term_deref(t);
        if (!t) return -1;
        if (t->tag == TERM_ATOM && t->atom_id == ATOM_NIL) return n;
        if (t->tag != TERM_COMPOUND || t->compound.functor != ATOM_DOT ||
            t->compound.arity != 2) return -1;
        n++;
        t = t->compound.args[1];
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int pl_univ(Term *t, Term *list, Trail *tr) {
    t    = term_deref(t);
    list = term_deref(list);
    if (t && t->tag != TERM_VAR) {
        Term *result;
        if (t->tag == TERM_ATOM) {
            Term *items[1] = { term_new_atom(t->atom_id) };
            result = make_list(1, items);
        } else if (t->tag == TERM_INT) {
            Term *items[1] = { term_new_int(t->ival) };
            result = make_list(1, items);
        } else if (t->tag == TERM_COMPOUND) {
            int arity = t->compound.arity;
            Term **items = malloc((arity + 1) * sizeof(Term *));
            items[0] = term_new_atom(t->compound.functor);
            for (int i = 0; i < arity; i++) items[i + 1] = t->compound.args[i];
            result = make_list(arity + 1, items);
            free(items);
        } else return 0;
        int mark = trail_mark(tr);
        if (!unify(list, result, tr)) { trail_unwind(tr, mark); return 0; }
        return 1;
    } else {
        if (!list || list->tag != TERM_COMPOUND || list->compound.functor != ATOM_DOT)
            return 0;
        int len = list_len(list);
        if (len < 1) return 0;
        Term *head_item = term_deref(list->compound.args[0]);
        if (!head_item || head_item->tag != TERM_ATOM) return 0;
        int functor_id = head_item->atom_id;
        int arity = len - 1;
        Term *new_t;
        if (arity == 0) {
            new_t = term_new_atom(functor_id);
        } else {
            Term **args = malloc(arity * sizeof(Term *));
            Term *cur = term_deref(list->compound.args[1]);
            for (int i = 0; i < arity; i++) {
                args[i] = term_deref(cur->compound.args[0]);
                cur = term_deref(cur->compound.args[1]);
            }
            new_t = term_new_compound(functor_id, arity, args);
            free(args);
        }
        int mark = trail_mark(tr);
        if (!unify(t, new_t, tr)) { trail_unwind(tr, mark); return 0; }
        return 1;
    }
}
static int _aid_plus=-1, _aid_minus=-1, _aid_times=-1, _aid_div=-1, _aid_mod=-1;

