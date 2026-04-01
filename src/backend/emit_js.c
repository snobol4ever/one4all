/*
 * emit_js.c — SNOBOL4 → JavaScript emitter for scrip-cc
 *
 * Peer to emit_byrd_c.c / emit_jvm.c / emit_net.c / emit_wasm.c.
 *
 * Architecture:
 *   - Every SNOBOL4 program compiles to a single .js file.
 *   - Simple statements (assign, expr) emit straight-line JS.
 *   - Pattern-match statements emit a Byrd-box dispatch loop:
 *       for(;;) switch(_pc) { case (uid<<2|SIGNAL): ... }
 *     using integer _pc = (node_uid << 2) | signal, matching engine.c.
 *   - OUTPUT via _vars Proxy (set trap writes to stdout).
 *   - Labels → JS label + goto via _pc assignment + continue dispatch.
 *
 * Dispatch encoding (decided SJ-1 — do not re-debate):
 *   const PROCEED=0, SUCCEED=1, CONCEDE=2, RECEDE=3;
 *   _pc = (uid << 2) | signal;
 *
 * Entry point: js_emit(Program*, FILE*)
 *
 * Sprint: SJ-2  Milestone: M-SJ-A01
 * Authors: Lon Jones Cherryholmes (arch), Claude Sonnet 4.6 (impl)
 */

#include "../frontend/snobol4/scrip_cc.h"
#include "../ir/ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* -----------------------------------------------------------------------
 * Output
 * ----------------------------------------------------------------------- */

static FILE *js_out;

static void J(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(js_out, fmt, ap);
    va_end(ap);
}

/* -----------------------------------------------------------------------
 * UID counter — single counter, shared across pattern and stmt emit
 * ----------------------------------------------------------------------- */

static int uid_ctr = 0;
static int js_next_uid(void) { return ++uid_ctr; }

/* -----------------------------------------------------------------------
 * JS-safe variable name mangling
 * Prepends '_', replaces non-alnum/_ with '_'.
 * ----------------------------------------------------------------------- */

static const char *jv(const char *s) {
    static char buf[520];
    int i = 0, j = 0;
    buf[j++] = '_';
    for (; s[i] && j < 510; i++) {
        unsigned char c = (unsigned char)s[i];
        buf[j++] = (isalnum(c) || c == '_') ? (char)c : '_';
    }
    buf[j] = '\0';
    return buf;
}

/* -----------------------------------------------------------------------
 * Escape a string for a JS string literal
 * ----------------------------------------------------------------------- */

static void js_escape_string(const char *s) {
    J("\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if      (c == '"')  J("\\\"");
        else if (c == '\\') J("\\\\");
        else if (c == '\n') J("\\n");
        else if (c == '\r') J("\\r");
        else if (c == '\t') J("\\t");
        else if (c < 0x20 || c > 0x7E) J("\\x%02x", c);
        else J("%c", c);
    }
    J("\"");
}

/* -----------------------------------------------------------------------
 * IO name check (OUTPUT, INPUT, PUNCH)
 * ----------------------------------------------------------------------- */

static int js_is_io(const char *n) {
    return n &&
        (strcmp(n,"OUTPUT")==0 || strcmp(n,"INPUT")==0 || strcmp(n,"PUNCH")==0);
}

/* -----------------------------------------------------------------------
 * Emit a JS expression from an EXPR_t node
 * ----------------------------------------------------------------------- */

static void js_emit_expr(EXPR_t *e) {
    if (!e) { J("null"); return; }
    switch (e->kind) {
    case E_NUL:
        J("null");
        break;
    case E_QLIT:
        js_escape_string(e->sval);
        break;
    case E_ILIT:
        J("%ld", e->ival);
        break;
    case E_FLIT:
        J("%g", e->dval);
        break;
    case E_VAR:
        if (js_is_io(e->sval))
            J("_vars[\"%s\"]", e->sval);
        else
            J("_vars[\"%s\"]", e->sval);
        break;
    case E_KW:
        J("_kw(\"%s\")", e->sval);
        break;
    case E_INDR: {
        EXPR_t *operand = (e->nchildren > 1 && e->children[1]) ? e->children[1] : e->children[0];
        J("_vars[_str("); js_emit_expr(operand); J(")]");
        break;
    }
    case E_NEG:
        J("(-_num("); js_emit_expr(e->children[0]); J("))");
        break;
    case E_CONCAT:
        J("_cat(");
        for (int i = 0; i < e->nchildren; i++) {
            if (i) J(", ");
            js_emit_expr(e->children[i]);
        }
        J(")");
        break;
    case E_ADD:
        J("_add("); js_emit_expr(e->children[0]); J(", "); js_emit_expr(e->children[1]); J(")");
        break;
    case E_SUB:
        J("_sub("); js_emit_expr(e->children[0]); J(", "); js_emit_expr(e->children[1]); J(")");
        break;
    case E_MPY:
        J("_mul("); js_emit_expr(e->children[0]); J(", "); js_emit_expr(e->children[1]); J(")");
        break;
    case E_DIV:
        J("_div("); js_emit_expr(e->children[0]); J(", "); js_emit_expr(e->children[1]); J(")");
        break;
    case E_POW:
        J("_pow("); js_emit_expr(e->children[0]); J(", "); js_emit_expr(e->children[1]); J(")");
        break;
    case E_FNC:
        J("_apply(\"%s\", [", e->sval);
        for (int i = 0; i < e->nchildren; i++) {
            if (i) J(", ");
            js_emit_expr(e->children[i]);
        }
        J("])");
        break;
    case E_CAPT_COND:
    case E_CAPT_IMM:
        js_emit_expr(e->children[0]);
        break;
    case E_ASSIGN: {
        const char *vname = e->children[0]->sval;
        J("(_vars[\"%s\"] = ", vname);
        js_emit_expr(e->children[1]);
        J(")");
        break;
    }
    default:
        J("/* unimpl E_%d */null", (int)e->kind);
        break;
    }
}

/* -----------------------------------------------------------------------
 * Emit goto logic for a statement's SnoGoto
 * fn  — enclosing function name (for RETURN routing)
 * ok  — JS expression string that is truthy on success, or NULL (uncond)
 * ----------------------------------------------------------------------- */

static void js_emit_goto(SnoGoto *go, int ok_uid) {
    /* ok_uid < 0 → unconditional success */
    if (!go) {
        /* fall through to next statement */
        return;
    }
    if (go->uncond) {
        J("    goto_%s();\n    return;\n", jv(go->uncond));
        return;
    }
    if (ok_uid < 0) {
        /* always success */
        if (go->onsuccess)
            J("    goto_%s();\n    return;\n", jv(go->onsuccess));
        return;
    }
    /* conditional */
    if (go->onsuccess && go->onfailure) {
        J("    if (_ok%d) { goto_%s(); return; }\n", ok_uid, jv(go->onsuccess));
        J("    else       { goto_%s(); return; }\n", jv(go->onfailure));
    } else if (go->onsuccess) {
        J("    if (_ok%d) { goto_%s(); return; }\n", ok_uid, jv(go->onsuccess));
    } else if (go->onfailure) {
        J("    if (!_ok%d) { goto_%s(); return; }\n", ok_uid, jv(go->onfailure));
    }
}

/* -----------------------------------------------------------------------
 * Collect all labels for forward-declaration of goto_ functions
 * ----------------------------------------------------------------------- */

static char **label_list = NULL;
static int    label_count = 0;
static int    label_cap   = 0;

static void label_register(const char *lbl) {
    if (!lbl) return;
    for (int i = 0; i < label_count; i++)
        if (strcmp(label_list[i], lbl) == 0) return;
    if (label_count >= label_cap) {
        label_cap = label_cap ? label_cap * 2 : 64;
        label_list = realloc(label_list, (size_t)label_cap * sizeof(char*));
    }
    label_list[label_count++] = strdup(lbl);
}

static void collect_labels(Program *prog) {
    for (STMT_t *s = prog->head; s; s = s->next) {
        if (s->label) label_register(s->label);
        if (s->go) {
            label_register(s->go->onsuccess);
            label_register(s->go->onfailure);
            label_register(s->go->uncond);
        }
    }
}

/* -----------------------------------------------------------------------
 * Emit one statement
 * ----------------------------------------------------------------------- */

static void js_emit_stmt(STMT_t *s) {
    J("/* line %d */\n", s->lineno);

    /* label-only: emit the goto_ function and return */
    if (s->label) {
        J("function goto_%s() {\n", jv(s->label));
    }

    /* label-only statement (no subject) */
    if (!s->subject) {
        if (s->go) js_emit_goto(s->go, -1);
        if (s->label) J("}\n");
        return;
    }

    if (!s->label) {
        /* Inline statement — wrap in IIFE-style block */
        J("{\n");
    }

    /* ---- pure assignment ---- */
    if (!s->pattern && s->replacement) {
        int u = js_next_uid();
        J("    var _v%d = ", u);
        js_emit_expr(s->replacement);
        J(";\n");
        J("    var _ok%d = (_v%d !== _FAIL);\n", u, u);
        J("    if (_ok%d) {\n", u);
        /* assign target */
        if (s->subject && s->subject->kind == E_VAR) {
            J("        _vars[\"%s\"] = _v%d;\n", s->subject->sval, u);
        }
        J("    }\n");
        js_emit_goto(s->go, u);
        if (s->label) J("}\n"); else J("}\n");
        return;
    }

    /* ---- null assign (has_eq, no replacement) ---- */
    if (!s->pattern && !s->replacement && s->has_eq) {
        if (s->subject && s->subject->kind == E_VAR)
            J("    _vars[\"%s\"] = null;\n", s->subject->sval);
        js_emit_goto(s->go, -1);
        if (s->label) J("}\n"); else J("}\n");
        return;
    }

    /* ---- pattern match ---- */
    if (s->pattern) {
        int u = js_next_uid();
        J("    var _s%d = _str(", u);
        js_emit_expr(s->subject);
        J(");\n");
        J("    var _ok%d = _match(_s%d, ", u, u);
        js_emit_expr(s->pattern);
        J(");\n");
        if (s->replacement && s->subject && s->subject->kind == E_VAR) {
            J("    if (_ok%d) { _vars[\"%s\"] = _replace(_s%d, ", u, s->subject->sval, u);
            js_emit_expr(s->replacement);
            J("); }\n");
        }
        js_emit_goto(s->go, u);
        if (s->label) J("}\n"); else J("}\n");
        return;
    }

    /* ---- expression evaluation only ---- */
    {
        int u = js_next_uid();
        J("    var _v%d = ", u);
        js_emit_expr(s->subject);
        J(";\n");
        J("    var _ok%d = (_v%d !== _FAIL);\n", u, u);
        js_emit_goto(s->go, u);
        if (s->label) J("}\n"); else J("}\n");
    }
}

/* -----------------------------------------------------------------------
 * Emit the complete JS program
 * ----------------------------------------------------------------------- */

void js_emit(Program *prog, FILE *f) {
    js_out   = f;
    uid_ctr  = 0;
    label_count = 0;

    collect_labels(prog);

    /* --- preamble: load runtime --- */
    J("'use strict';\n");
    J("const _rt = require(process.env.SNO_RUNTIME || __dirname + '/sno_runtime.js');\n");
    J("const { _vars, _FAIL, _str, _num, _cat, _add, _sub, _mul, _div, _pow,\n");
    J("        _apply, _kw, _match, _replace } = _rt;\n");
    J("\n");

    /* --- forward declare all goto_ functions --- */
    for (int i = 0; i < label_count; i++)
        J("var goto_%s;\n", jv(label_list[i]));
    J("\n");

    /* --- emit statements --- */
    J("function _program() {\n");

    for (STMT_t *s = prog->head; s; s = s->next) {
        if (s->is_end) {
            J("    /* END */\n");
            J("    return;\n");
            break;
        }
        /* label statements become named functions; emit them before main flow */
        if (s->label) {
            /* close _program for now, emit labeled block, reopen */
            J("}\n\n");
            js_emit_stmt(s);
            J("\nfunction _program_continue_after_%s() {\n", jv(s->label));
        } else {
            js_emit_stmt(s);
        }
    }

    J("}\n\n");
    J("_program();\n");

    /* free label list */
    for (int i = 0; i < label_count; i++) free(label_list[i]);
    free(label_list);
    label_list  = NULL;
    label_count = 0;
    label_cap   = 0;
}
