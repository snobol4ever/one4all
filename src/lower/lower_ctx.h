#ifndef LOWER_CTX_H
#define LOWER_CTX_H
#include "sm_prog.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"
#include "../../runtime/interp/icn_runtime.h"
typedef struct { char *name; int instr_idx; } LabelEntry;
typedef struct { int jump_instr_idx; char *target_name; } PatchEntry;
typedef struct {
    LabelEntry *labels;  int nlabels,  labels_cap;
    PatchEntry *patches; int npatches, patches_cap;
} LabelTable;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void labtab_init       (LabelTable *lt);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void labtab_free       (LabelTable *lt);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void labtab_define     (LabelTable *lt, const char *name, int instr_idx);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  labtab_find       (const LabelTable *lt, const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void labtab_patch_later(LabelTable *lt, int jump_instr_idx, const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  labtab_resolve    (LabelTable *lt, SM_Program *p);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
char *kw_canonicalize(const char *raw);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void expression_scope_walk(IcnScope *sc, tree_t *e);
#include "ast_clone.h"
#define T0(t) ((t)->n > 0 ? (t)->c[0] : NULL)
#define T1(t) ((t)->n > 1 ? (t)->c[1] : NULL)
#define T2(t) ((t)->n > 2 ? (t)->c[2] : NULL)
#define LOWER2(op)     do { lower_expr(T0(t)); lower_expr(T1(t)); sm_emit(g_p,(op)); return; } while(0)
#define LOWER1_VAL(op) do { lower_expr(T0(t));                    sm_emit(g_p,(op)); return; } while(0)
#define LOWER1_PAT(op) do { lower_pat_expr(T0(t));                sm_emit(g_p,(op)); return; } while(0)
#endif
