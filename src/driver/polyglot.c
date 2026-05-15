#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gc.h>
#include "frontend/snobol4/scrip_cc.h"
extern int64_t kw_case;
#include "frontend/prolog/prolog_driver.h"
#include "frontend/prolog/prolog_atom.h"
#include "frontend/icon/icon_driver.h"
#include "frontend/raku/raku_driver.h"
#include "frontend/rebus/rebus_lower.h"
#include "runtime/interp/icn_runtime.h"
#include "runtime/interp/pl_runtime.h"
#include "driver/interp.h"
#include "driver/polyglot.h"
#include "lower.h"
#include "sm_prog.h"
ScripModuleRegistry g_registry;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline int           s_int(const tree_t *s, const char *tag) {
    const char *v = stmt_attr_str(stmt_attr_find(s, tag)); return v ? atoi(v) : 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline tree_t        *s_expr(const tree_t *s, const char *tag) {
    return stmt_attr_expr(stmt_attr_find(s, tag)); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
uint32_t polyglot_lang_mask(const tree_t *prog)
{
    uint32_t mask = 0;
    if (!prog) return mask;
    for (int i = 0; i < prog->n; i++) {
        const tree_t *s = prog->c[i];
        if (!s) continue;
        int lang = s_int(s, ":lang");
        if (lang >= 0 && lang < 32)
            mask |= (1u << lang);
    }
    mask |= (1u << LANG_SNO);
    return mask;
}
int g_fi8_icn_init_count = 0;
int g_fi8_pl_init_count  = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void polyglot_init(const tree_t *prog, uint32_t lang_mask)
{
    if (!prog) return;
    label_table_build(prog);
    prescan_defines(prog);
    kw_case = 1;
    if (lang_mask & ((1u << LANG_ICN) | (1u << LANG_RAKU))) {
        g_fi8_icn_init_count++;
        proc_count = 0; global_count = 0;
        frame_depth = 0;
        memset(frame_stack, 0, sizeof frame_stack);
        scan_subj = ""; scan_pos = 1; scan_depth = 0;
        g_icn_root = NULL;
    }
    if (lang_mask & (1u << LANG_PL)) {
        g_fi8_pl_init_count++;
        prolog_atom_init();
        memset(&g_pl_pred_table, 0, sizeof g_pl_pred_table);
        trail_init(&g_pl_trail);
        g_pl_cut_flag = 0;
        g_pl_env      = NULL;
        g_pl_active   = 0;
    }
    memset(&g_registry, 0, sizeof g_registry);
    g_registry.main_mod = -1;
    int cur_lang = -1;
    int mod_idx  = -1;
    for (int _ci = 0; _ci < prog->n; _ci++) {
        const tree_t *s = prog->c[_ci];
        if (!s || (s->t != TT_STMT && s->t != TT_END)) continue;
        int s_lang = s_int(s, ":lang");
        if (s_lang != cur_lang) {
            if (g_registry.nmod < SCRIP_MOD_MAX) {
                cur_lang = s_lang;
                mod_idx  = g_registry.nmod++;
                ScripModule *m = &g_registry.mods[mod_idx];
                m->lang             = s_lang;
                m->name             = NULL;
                m->first            = s;
                m->last             = s;
                m->nstmts           = 0;
                m->sno_label_start  = label_count;
                m->sno_label_count  = 0;
                m->icn_proc_start   = proc_count;
                m->proc_count       = 0;
            }
        }
        if (mod_idx >= 0) {
            g_registry.mods[mod_idx].last = s;
            g_registry.mods[mod_idx].nstmts++;
        }
        tree_t *subj = s_expr(s, ":subj");
        if (!subj) continue;
        if (s_lang == LANG_ICN || s_lang == LANG_RAKU) {
            tree_t *proc = subj;
            if (proc->t == TT_GLOBAL) {
                for (int _gi = 0; _gi < proc->n; _gi++)
                    if (proc->c[_gi] && proc->c[_gi]->v.sval)
                        global_register(proc->c[_gi]->v.sval);
            }
            if (proc->t == TT_RECORD && proc->v.sval && *proc->v.sval) {
                char spec[256]; int pos = 0;
                pos += snprintf(spec+pos, sizeof(spec)-pos, "%s(", proc->v.sval);
                for (int _ri = 0; _ri < proc->n && pos < (int)sizeof(spec)-2; _ri++) {
                    if (_ri > 0) spec[pos++] = ',';
                    const char *fn2 = (proc->c[_ri] && proc->c[_ri]->v.sval)
                                      ? proc->c[_ri]->v.sval : "";
                    pos += snprintf(spec+pos, sizeof(spec)-pos, "%s", fn2);
                }
                if (pos < (int)sizeof(spec)-1) spec[pos++] = ')';
                spec[pos] = '\0';
                icn_record_register(spec);
            }
            if (proc->t == TT_FNC && proc->v.sval && *proc->v.sval) {
                const char *name = proc->v.sval;
                if (proc_count < PROC_TABLE_MAX) {
                    proc_table[proc_count].name     = name;
                    proc_table[proc_count].proc     = proc;
                    proc_table[proc_count].entry_pc = -1;
                    proc_table[proc_count].nparams  = (s_lang == LANG_ICN)
                        ? proc->_id
                        : (int)proc->v.ival;
                    proc_count++;
                    if (mod_idx >= 0) g_registry.mods[mod_idx].proc_count++;
                    if (strcmp(name, "main") == 0 && g_registry.main_mod < 0)
                        g_registry.main_mod = mod_idx;
                }
            }
            if (proc->t == TT_RECORD) {
                interp_eval(proc);
            }
        } else if (s_lang == LANG_PL) {
            tree_t *sub = subj;
            if ((sub->t == TT_CHOICE || sub->t == TT_CLAUSE) && sub->v.sval) {
                pl_pred_table_insert(&g_pl_pred_table, sub->v.sval, sub);
                g_pl_active = 1;
                if (strcmp(sub->v.sval, "main/0") == 0 && g_registry.main_mod < 0)
                    g_registry.main_mod = mod_idx;
            }
        } else if (s_lang == LANG_SNO) {
            const char *lbl = stmt_attr_str(stmt_attr_find(s, ":lbl"));
            if (mod_idx >= 0 && lbl && *lbl)
                g_registry.mods[mod_idx].sno_label_count++;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pl_directive_max_var_slot(tree_t *root)
{
    if (!root) return -1;
    int max_slot = -1;
    enum { CAP = 512 };
    tree_t *stk[CAP];
    int top = 0;
    stk[top++] = root;
    while (top > 0) {
        tree_t *e = stk[--top];
        if (!e) continue;
        if (e->t == TT_VAR && (int)e->v.ival > max_slot) max_slot = (int)e->v.ival;
        for (int i = 0; i < e->n && top < CAP; i++)
            if (e->c[i]) stk[top++] = e->c[i];
    }
    return max_slot;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void polyglot_execute(const tree_t *prog) {
    if (!prog) return;
    if (g_polyglot) {
        execute_program(prog);
        return;
    }
    int slang = LANG_SNO;
    for (int _i = 0; _i < prog->n; _i++) {
        const tree_t *ch = prog->c[_i];
        if (ch && (ch->t == TT_STMT || ch->t == TT_END)) {
            slang = s_int(ch, ":lang"); break;
        }
    }
    uint32_t mask = polyglot_lang_mask(prog);
    polyglot_init(prog, mask);
    if (slang == LANG_ICN) {
        g_lang = 1;
        for (int i = 0; i < proc_count; i++) {
            if (strcmp(proc_table[i].name, "main") == 0) {
                proc_table_call(i, NULL, 0);
                return;
            }
        }
        fprintf(stderr, "icon: no main procedure\n");
    } else if (slang == LANG_PL) {
        g_pl_active = 1;
        for (int _ci = 0; _ci < prog->n; _ci++) {
            const tree_t *_s = prog->c[_ci];
            if (!_s || _s->t != TT_STMT) continue;
            if (s_int(_s, ":lang") != LANG_PL) continue;
            tree_t *_subj = s_expr(_s, ":subj");
            if (!_subj) continue;
            if (_subj->t == TT_CHOICE || _subj->t == TT_CLAUSE) continue;
            int _max_slot = pl_directive_max_var_slot(_subj);
            Term **_dir_env = (_max_slot >= 0) ? pl_env_new(_max_slot + 1) : NULL;
            Term **_saved   = g_pl_env;
            if (_dir_env) g_pl_env = _dir_env;
            interp_exec_pl_builtin(_subj, _dir_env);
            g_pl_env = _saved;
            if (_dir_env) free(_dir_env);
        }
        Pl_PredEntry *_main_pe = pl_pred_entry_lookup("main/0");
        tree_t *main_choice = pl_pred_table_lookup(&g_pl_pred_table, "main/0");
        extern SM_Program *g_current_sm_prog;
        if (_main_pe && _main_pe->entry_pc >= 0 && g_current_sm_prog != NULL) {
            extern DESCR_t sm_call_expression(int);
            sm_call_expression(_main_pe->entry_pc);
        } else if (main_choice) interp_eval(main_choice);
        else fprintf(stderr, "prolog: no main/0 predicate\n");
        g_pl_active = 0;
    } else if (slang == LANG_REB) {
        execute_program(prog);
    } else {
        execute_program(prog);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern tree_t *sno_parse_string_ast(const char *src, CODE_t **code_out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *parse_scrip_polyglot(const char *src, const char *filename)
{
    tree_t *result = calloc(1, sizeof(tree_t));
    if (!result) return NULL;
    result->t = TT_PROGRAM;
    const char *p = src;
    while (*p) {
        const char *fence = strstr(p, "```");
        if (!fence) break;
        const char *tag_start = fence + 3;
        const char *tag_end   = tag_start;
        while (*tag_end && *tag_end != '\n' && *tag_end != '\r') tag_end++;
        while (tag_end > tag_start && (tag_end[-1] == ' ' || tag_end[-1] == '\t')) tag_end--;
        int tag_len = (int)(tag_end - tag_start);
        p = tag_end;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        int lang = -1;
        if      (tag_len == 7 && strncasecmp(tag_start, "SNOBOL4", 7) == 0) lang = LANG_SNO;
        else if (tag_len == 4 && strncasecmp(tag_start, "Icon",    4) == 0) lang = LANG_ICN;
        else if (tag_len == 6 && strncasecmp(tag_start, "Prolog",  6) == 0) lang = LANG_PL;
        else if (tag_len == 4 && strncasecmp(tag_start, "Raku",    4) == 0) lang = LANG_RAKU;
        else if (tag_len == 5 && strncasecmp(tag_start, "Scrip",   5) == 0) lang = LANG_SCRIP;
        else if (tag_len == 6 && strncasecmp(tag_start, "SCRIP",   5) == 0) lang = LANG_SCRIP;
        else if (tag_len == 5 && strncasecmp(tag_start, "Rebus",   5) == 0) lang = LANG_REB;
        const char *block_start = p;
        const char *close = strstr(p, "```");
        if (!close) break;
        int   blen = (int)(close - block_start);
        char *block = malloc(blen + 1);
        if (!block) { p = close + 3; continue; }
        memcpy(block, block_start, blen);
        block[blen] = '\0';
        p = close + 3;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        if (lang < 0) { free(block); continue; }
        tree_t *sub_ast = NULL;
        if (lang == LANG_SNO || lang == LANG_SCRIP) {
            sub_ast = sno_parse_string_ast(block, NULL);
        } else if (lang == LANG_ICN) {
            icon_compile(block, filename, &sub_ast);
        } else if (lang == LANG_PL) {
            prolog_compile(block, filename, &sub_ast);
        } else if (lang == LANG_RAKU) {
            raku_compile(block, filename, &sub_ast);
        } else if (lang == LANG_REB) {
            rebus_compile(block, filename, &sub_ast);
        }
        free(block);
        if (!sub_ast || sub_ast->n == 0) { free(sub_ast); continue; }
        for (int _i = 0; _i < sub_ast->n; _i++) {
            tree_t *ch = sub_ast->c[_i];
            if (!ch) continue;
            ast_push(result, ch);
        }
        free(sub_ast->c); free(sub_ast);
    }
    return result;
}
