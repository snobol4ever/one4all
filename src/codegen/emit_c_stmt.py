"""
emit_c_stmt.py — Sprint 20 SNOBOL4 → C statement emitter

Translates a Program (list of Stmts) into a C function:

    int sno_program(void)

Each statement compiles to:
  - A C label  (STMT_NNN or the SNOBOL4 label name)
  - The statement body (assignment, pattern match, function call)
  - A goto to the next statement (or conditional :S/:F branches)

Pattern matching is handled by calling into the runtime pattern engine.
The inc-file functions (counter, stack, tree, etc.) are hardcoded in C
in snobol4_inc.c — they are called by name directly.

Memory model: Boehm GC throughout (D1).
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'ir'))
from ir import Expr, PatExpr, Goto, Stmt, Program


# -----------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------

def _c_label(label_name):
    """Convert a SNOBOL4 label to a valid C label."""
    # Replace chars that aren't valid in C identifiers
    s = label_name.replace("'", "_q_").replace("#", "_H_").replace("@", "_A_")
    s = s.replace("-", "_").replace(".", "_dot_")
    # Prefix if starts with digit
    if s and s[0].isdigit():
        s = 'L_' + s
    return 'SNO_' + s


def _stmt_label(i, stmt):
    """Return the C label for statement i."""
    if stmt.label:
        return _c_label(stmt.label)
    return f'_stmt_{i}'


# -----------------------------------------------------------------------
# Expression emitter → C expression string
# -----------------------------------------------------------------------

def emit_expr(e):
    """Emit a C expression string for a SNOBOL4 Expr node."""
    if e is None:
        return 'SNO_NULL_VAL'

    k = e.kind

    if k == 'null':
        return 'SNO_NULL_VAL'

    if k == 'str':
        # Escape the string for C
        s = e.val.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
        return f'SNO_STR_VAL("{s}")'

    if k == 'int':
        return f'SNO_INT_VAL({e.val}LL)'

    if k == 'real':
        return f'SNO_REAL_VAL({e.val})'

    if k == 'var':
        return f'sno_var_get("{e.val}")'

    if k == 'keyword':
        kw = e.val.upper()
        # Map known keywords to their C globals
        kw_map = {
            'FULLSCAN':  '(SNO_INT_VAL(sno_kw_fullscan))',
            'MAXLNGTH':  '(SNO_INT_VAL(sno_kw_maxlngth))',
            'ANCHOR':    '(SNO_INT_VAL(sno_kw_anchor))',
            'TRIM':      '(SNO_INT_VAL(sno_kw_trim))',
            'STLIMIT':   '(SNO_INT_VAL(sno_kw_stlimit))',
            'UCASE':     'SNO_STR_VAL(sno_ucase)',
            'LCASE':     'SNO_STR_VAL(sno_lcase)',
            'ALPHABET':  'SNO_STR_VAL(sno_alphabet)',
        }
        return kw_map.get(kw, f'sno_var_get("&{kw}")')

    if k == 'indirect':
        inner = emit_expr(e.child)
        return f'sno_var_get(sno_to_str({inner}))'

    if k == 'concat':
        l = emit_expr(e.left)
        r = emit_expr(e.right)
        return f'SNO_STR_VAL(sno_concat(sno_to_str({l}), sno_to_str({r})))'

    if k == 'add':
        return f'sno_add({emit_expr(e.left)}, {emit_expr(e.right)})'
    if k == 'sub':
        return f'sno_sub({emit_expr(e.left)}, {emit_expr(e.right)})'
    if k == 'mul':
        return f'sno_mul({emit_expr(e.left)}, {emit_expr(e.right)})'
    if k == 'div':
        return f'sno_div({emit_expr(e.left)}, {emit_expr(e.right)})'
    if k == 'pow':
        return f'sno_pow({emit_expr(e.left)}, {emit_expr(e.right)})'
    if k == 'neg':
        return f'sno_neg({emit_expr(e.child)})'

    if k == 'call':
        name = e.name.upper()
        args = e.args or []

        # Built-in functions that map directly to runtime calls
        builtins = {
            'SIZE':     lambda a: f'sno_size_fn({a[0]})',
            'DUPL':     lambda a: f'sno_dupl_fn({a[0]}, {a[1]})',
            'REPLACE':  lambda a: f'sno_replace_fn({a[0]}, {a[1]}, {a[2]})',
            'SUBSTR':   lambda a: f'sno_substr_fn({a[0]}, {a[1]}, {a[2]})',
            'TRIM':     lambda a: f'sno_trim_fn({a[0]})',
            'LPAD':     lambda a: f'sno_lpad_fn({a[0]}, {a[1]}, {len(a)>2 and a[2] or "SNO_STR_VAL(\" \")"})',
            'RPAD':     lambda a: f'sno_rpad_fn({a[0]}, {a[1]}, {len(a)>2 and a[2] or "SNO_STR_VAL(\" \")"})',
            'REVERSE':  lambda a: f'sno_reverse_fn({a[0]})',
            'CHAR':     lambda a: f'sno_char_fn({a[0]})',
            'INTEGER':  lambda a: f'sno_integer_fn({a[0]})',
            'REAL':     lambda a: f'sno_real_fn({a[0]})',
            'STRING':   lambda a: f'sno_string_fn({a[0]})',
            'DATATYPE': lambda a: f'SNO_STR_VAL(sno_datatype({a[0]}))',
            'IDENT':    lambda a: f'(sno_ident({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'DIFFER':   lambda a: f'(sno_differ({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'EQ':       lambda a: f'(sno_eq({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'NE':       lambda a: f'(sno_ne({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'LT':       lambda a: f'(sno_lt({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'LE':       lambda a: f'(sno_le({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'GT':       lambda a: f'(sno_gt({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'GE':       lambda a: f'(sno_ge({a[0]},{a[1]}) ? {a[0]} : SNO_NULL_VAL)',
            'ARRAY':    lambda a: f'sno_array_create({a[0]})',
            'TABLE':    lambda a: f'SNO_TABLE_VAL(sno_table_new())',
            'SORT':     lambda a: f'sno_sort_fn({a[0]})',
            'DEFINE':   lambda a: f'(sno_define_spec({a[0]}), SNO_NULL_VAL)',
            'DATA':     lambda a: f'(sno_data_define(sno_to_str({a[0]})), SNO_NULL_VAL)',
            'APPLY':    lambda a: f'sno_apply_val({a[0]}, (SnoVal[]){{ {", ".join(a[1:])} }}, {len(a)-1})',
            'EVAL':     lambda a: f'sno_eval({a[0]})',
            'OPSYN':    lambda a: f'(sno_opsyn({a[0]},{a[1]},{len(a)>2 and a[2] or "SNO_INT_VAL(2)"}), SNO_NULL_VAL)',
            'CODE':     lambda a: f'sno_code({a[0]})',
        }

        ca = [emit_expr(a) for a in args]

        if name in builtins:
            try:
                return builtins[name](ca)
            except (IndexError, TypeError):
                pass

        # Unknown call — dispatch through function table
        args_c = ', '.join(ca)
        nargs  = len(ca)
        if nargs == 0:
            return f'sno_apply("{e.name}", NULL, 0)'
        return (f'sno_apply("{e.name}", '
                f'(SnoVal[{nargs}]){{{args_c}}}, {nargs})')

    if k == 'field':
        # f(x) — field accessor
        child = emit_expr(e.child)
        return f'sno_field_get({child}, "{e.name}")'

    if k == 'array':
        obj  = emit_expr(e.obj)
        subs = [emit_expr(s) for s in (e.subscripts or [])]
        if len(subs) == 1:
            return f'sno_subscript_get({obj}, {subs[0]})'
        if len(subs) == 2:
            return f'sno_subscript_get2({obj}, {subs[0]}, {subs[1]})'
        return f'SNO_NULL_VAL /* array subscript */'

    return f'SNO_NULL_VAL /* unhandled expr kind={k} */'


# -----------------------------------------------------------------------
# Assignment target emitter → C statement string
# -----------------------------------------------------------------------

def emit_assign_target(lhs, rhs_c):
    """Emit C code to assign rhs_c to the lhs subject."""
    if lhs is None:
        return f'    (void)({rhs_c});'

    k = lhs.kind

    if k == 'var':
        name = lhs.val
        # Check for OUTPUT / INPUT / TERMINAL special vars
        if name.upper() == 'OUTPUT':
            return f'    sno_output_val({rhs_c});'
        # Keywords
        kw_assigns = {
            'FULLSCAN': 'sno_kw_fullscan',
            'MAXLNGTH':  'sno_kw_maxlngth',
            'ANCHOR':    'sno_kw_anchor',
            'TRIM':      'sno_kw_trim',
            'STLIMIT':   'sno_kw_stlimit',
        }
        return f'    sno_var_set("{name}", {rhs_c});'

    if k == 'keyword':
        kw = lhs.val.upper()
        kw_assigns = {
            'FULLSCAN': 'sno_kw_fullscan',
            'MAXLNGTH':  'sno_kw_maxlngth',
            'ANCHOR':    'sno_kw_anchor',
            'TRIM':      'sno_kw_trim',
            'STLIMIT':   'sno_kw_stlimit',
        }
        if kw in kw_assigns:
            return f'    {kw_assigns[kw]} = sno_to_int({rhs_c});'
        return f'    sno_var_set("&{kw}", {rhs_c});'

    if k == 'indirect':
        inner = emit_expr(lhs.child)
        return f'    sno_var_set(sno_to_str({inner}), {rhs_c});'

    if k == 'array':
        obj  = emit_expr(lhs.obj)
        subs = [emit_expr(s) for s in (lhs.subscripts or [])]
        if len(subs) == 1:
            return f'    sno_subscript_set({obj}, {subs[0]}, {rhs_c});'
        if len(subs) == 2:
            return f'    sno_subscript_set2({obj}, {subs[0]}, {subs[1]}, {rhs_c});'

    if k == 'field':
        # value($'#N') = ... → sno_field_set(sno_indirect..., "value", rhs)
        child = emit_expr(lhs.child)
        return f'    sno_field_set({child}, "{lhs.name}", {rhs_c});'

    if k == 'call':
        # e.g. value(x) = rhs → sno_field_set(x, "value", rhs)
        name = lhs.name
        args = [emit_expr(a) for a in (lhs.args or [])]
        if args:
            return f'    sno_field_set({args[0]}, "{name}", {rhs_c});'

    # Fallback
    return f'    /* unhandled assignment target kind={k} */;'


# -----------------------------------------------------------------------
# Pattern emitter → C match call
# -----------------------------------------------------------------------

def emit_pattern_match(subject_c, pat, success_label, fail_label):
    """Emit C code to match pat against subject_c.
    Jumps to success_label on match, fail_label on no-match.
    Returns list of C lines.
    """
    lines = []
    pat_c = emit_pattern_expr(pat)
    lines.append(f'    {{')
    lines.append(f'        SnoVal _subj = {subject_c};')
    lines.append(f'        int _matched = sno_match_pattern({pat_c}, sno_to_str(_subj));')
    lines.append(f'        if (_matched) goto {success_label};')
    lines.append(f'        else goto {fail_label};')
    lines.append(f'    }}')
    return lines


def emit_pat_or_expr(p):
    """Emit either a pattern or an expression as a pattern."""
    from ir import Expr, PatExpr
    if isinstance(p, PatExpr):
        return emit_pattern_expr(p)
    elif isinstance(p, Expr):
        return f'sno_var_as_pattern({emit_expr(p)})'
    elif isinstance(p, str):
        s = p.replace('"', '\\"')
        return f'sno_pat_lit("{s}")'
    return 'sno_pat_epsilon()'


def emit_pattern_expr(p):
    """Emit a C expression that constructs a runtime pattern for p."""
    if p is None:
        return 'sno_pat_any()'

    k = p.kind

    if k == 'lit':
        s = (p.val or '').replace('\\', '\\\\').replace('"', '\\"')
        return f'sno_pat_lit("{s}")'

    if k == 'var':
        if isinstance(p.val, str):
            return f'sno_var_as_pattern(sno_var_get("{p.val}"))'
        # Expr object
        return f'sno_var_as_pattern({emit_expr(p.val)})'

    if k == 'ref':
        return f'sno_pat_ref("{p.name}")'

    if k == 'epsilon':
        return 'sno_pat_epsilon()'

    if k == 'arb':
        return 'sno_pat_arb()'

    if k == 'rem':
        return 'sno_pat_rem()'

    if k == 'fail':
        return 'sno_pat_fail()'

    if k == 'abort':
        return 'sno_pat_abort()'

    if k == 'fence':
        return 'sno_pat_fence()'

    if k == 'succeed':
        return 'sno_pat_succeed()'

    if k == 'bal':
        return 'sno_pat_bal()'

    if k == 'cat':
        l = emit_pattern_expr(p.left)
        r = emit_pattern_expr(p.right)
        return f'sno_pat_cat({l}, {r})'

    if k == 'alt':
        l = emit_pattern_expr(p.left)
        r = emit_pattern_expr(p.right)
        return f'sno_pat_alt({l}, {r})'

    if k == 'assign_imm':
        child = emit_pattern_expr(p.child)
        var   = emit_expr(p.var) if p.var else 'SNO_NULL_VAL'
        return f'sno_pat_assign_imm({child}, {var})'

    if k == 'assign_cond':
        child = emit_pattern_expr(p.child)
        var   = emit_expr(p.var) if p.var else 'SNO_NULL_VAL'
        return f'sno_pat_assign_cond({child}, {var})'

    if k == 'call':
        name = (p.name or '').upper()
        args = p.args or []

        # Pattern builtins that return patterns
        pat_builtins = {
            'SPAN':    lambda a: f'sno_pat_span(sno_to_str({a[0]}))',
            'BREAK':   lambda a: f'sno_pat_break_(sno_to_str({a[0]}))',
            'ANY':     lambda a: f'sno_pat_any_cs(sno_to_str({a[0]}))',
            'NOTANY':  lambda a: f'sno_pat_notany(sno_to_str({a[0]}))',
            'LEN':     lambda a: f'sno_pat_len(sno_to_int({a[0]}))',
            'POS':     lambda a: f'sno_pat_pos(sno_to_int({a[0]}))',
            'RPOS':    lambda a: f'sno_pat_rpos(sno_to_int({a[0]}))',
            'TAB':     lambda a: f'sno_pat_tab(sno_to_int({a[0]}))',
            'RTAB':    lambda a: f'sno_pat_rtab(sno_to_int({a[0]}))',
            'ARB':     lambda a: 'sno_pat_arb()',
            'REM':     lambda a: 'sno_pat_rem()',
            'FENCE':   lambda a: f'sno_pat_fence_p({emit_pat_or_expr(a[0]) if a else "sno_pat_epsilon()"})',
            'ARBNO':   lambda a: f'sno_pat_arbno({emit_pat_or_expr(a[0])})',
            'BAL':     lambda a: 'sno_pat_bal()',
            'FAIL':    lambda a: 'sno_pat_fail()',
            'ABORT':   lambda a: 'sno_pat_abort()',
            'SUCCEED': lambda a: 'sno_pat_succeed()',
        }

        # Build C args
        ca_expr = [emit_expr(a) if isinstance(a, Expr) else emit_pattern_expr(a)
                   for a in args]

        if name in pat_builtins:
            try:
                return pat_builtins[name](ca_expr)
            except (IndexError, TypeError):
                pass

        # Unknown — call through pattern table or user function as pattern
        args_c = ', '.join(ca_expr)
        nargs  = len(ca_expr)
        return (f'sno_pat_user_call("{p.name}", '
                f'(SnoVal[{max(nargs,1)}]){{{args_c}}}, {nargs})')

    return f'sno_pat_epsilon() /* unhandled pattern kind={k} */'


# -----------------------------------------------------------------------
# Full program emitter
# -----------------------------------------------------------------------

class StmtEmitter:
    def __init__(self, prog: Program):
        self.prog   = prog
        self.stmts  = prog.stmts
        self.lines  = []

    def emit(self):
        self.lines = []
        self._emit_header()
        self._emit_body()
        self._emit_footer()
        return '\n'.join(self.lines)

    def _w(self, line=''):
        self.lines.append(line)

    def _emit_header(self):
        self._w('/* Generated by emit_c_stmt.py — Sprint 20 */')
        self._w('#include "snobol4.h"')
        self._w('#include "snobol4_inc.h"')
        self._w('')
        self._w('int sno_program(void) {')
        self._w('    /* Jump to first statement */')
        if self.stmts:
            first_lbl = _stmt_label(0, self.stmts[0])
            self._w(f'    goto {first_lbl};')
        self._w('')

    def _emit_body(self):
        n = len(self.stmts)
        for i, stmt in enumerate(self.stmts):
            lbl = _stmt_label(i, stmt)
            next_lbl = _stmt_label(i+1, self.stmts[i+1]) if i+1 < n else 'SNO_END'

            # Emit the C label
            self._w(f'{lbl}: {{  /* L{stmt.lineno} */')

            # Determine success/failure labels from goto field
            succ_lbl = next_lbl
            fail_lbl = next_lbl

            if stmt.goto:
                g = stmt.goto
                if g.unconditional:
                    cl = _c_label(g.unconditional)
                    # Map SNOBOL4 special labels
                    cl = self._map_special_label(cl, g.unconditional)
                    succ_lbl = cl
                    fail_lbl = cl
                else:
                    if g.on_success:
                        cl = _c_label(g.on_success)
                        cl = self._map_special_label(cl, g.on_success)
                        succ_lbl = cl
                    if g.on_failure:
                        cl = _c_label(g.on_failure)
                        cl = self._map_special_label(cl, g.on_failure)
                        fail_lbl = cl

            # ---- Emit statement body ----

            subj   = stmt.subject
            pat    = stmt.pattern
            repl   = stmt.replacement

            # Case 1: pure function call (no subject, no pattern)
            # e.g.  DEFINE('foo(a,b)')  or  DATA('tree(t,v,n,c)')
            if subj is not None and subj.kind == 'call' and pat is None and repl is None:
                name = subj.name.upper() if subj.name else ''
                # DEFINE / DATA — executed for side effects
                ca = [emit_expr(a) for a in (subj.args or [])]
                if name == 'DEFINE':
                    self._w(f'    sno_define_spec({ca[0] if ca else "SNO_NULL_VAL"});')
                elif name == 'DATA':
                    self._w(f'    sno_data_define(sno_to_str({ca[0] if ca else "SNO_NULL_VAL"}));')
                elif name == 'OPSYN':
                    self._w(f'    sno_opsyn({", ".join(ca)});')
                else:
                    args_c = ', '.join(ca)
                    n_args = len(ca)
                    if n_args:
                        self._w(f'    sno_apply("{subj.name}", (SnoVal[{n_args}]){{{args_c}}}, {n_args});')
                    else:
                        self._w(f'    sno_apply("{subj.name}", NULL, 0);')
                # Goto handling: if S/F specified, they apply to the call result
                self._w(f'    goto {succ_lbl};')

            # Case 2: subject = replacement (assignment, no pattern)
            elif subj is not None and repl is not None and pat is None:
                rhs_c = emit_expr(repl)
                self._w(emit_assign_target(subj, rhs_c))
                self._w(f'    goto {succ_lbl};')

            # Case 3: subject pattern = replacement (pattern match + optional replacement)
            elif subj is not None and pat is not None:
                subj_c  = emit_expr(subj)
                pat_c   = emit_pattern_expr(pat)
                repl_c  = emit_expr(repl) if repl else None

                if repl_c:
                    # Pattern match with replacement
                    self._w(f'    {{')
                    self._w(f'        SnoVal _subj = {subj_c};')
                    self._w(f'        int _ok = sno_match_and_replace(&_subj, {pat_c}, {repl_c});')
                    self._w(f'        {emit_assign_target(subj, "_subj")[4:]}')  # strip leading 4 spaces
                    self._w(f'        if (_ok) goto {succ_lbl};')
                    self._w(f'        else goto {fail_lbl};')
                    self._w(f'    }}')
                else:
                    # Pattern match only (no replacement)
                    self._w(f'    {{')
                    self._w(f'        SnoVal _subj = {subj_c};')
                    self._w(f'        int _ok = sno_match_pattern({pat_c}, sno_to_str(_subj));')
                    self._w(f'        if (_ok) goto {succ_lbl};')
                    self._w(f'        else goto {fail_lbl};')
                    self._w(f'    }}')

            # Case 4: bare goto only (no subject, no pattern, no replacement)
            elif subj is None and pat is None and repl is None:
                self._w(f'    goto {succ_lbl};')

            # Case 5: only replacement (OUTPUT = ... without subject)
            elif subj is None and repl is not None:
                rhs_c = emit_expr(repl)
                self._w(f'    sno_output_val({rhs_c});')
                self._w(f'    goto {succ_lbl};')

            else:
                self._w(f'    /* unhandled stmt shape */')
                self._w(f'    goto {next_lbl};')

            self._w(f'}}')
            self._w()

    def _map_special_label(self, cl, orig):
        """Map SNOBOL4 special labels to C equivalents."""
        upper = orig.upper()
        if upper == 'END':
            return 'SNO_END'
        if upper == 'RETURN':
            return 'SNO_RETURN_LABEL'
        if upper == 'FRETURN':
            return 'SNO_FRETURN_LABEL'
        if upper == 'NRETURN':
            return 'SNO_NRETURN_LABEL'
        if upper == 'CONTINUE':
            return 'SNO_CONTINUE_LABEL'
        return cl

    def _emit_footer(self):
        self._w('SNO_END:')
        self._w('    return SNO_SUCCESS;')
        self._w('SNO_RETURN_LABEL:')
        self._w('    return SNO_SUCCESS;')
        self._w('SNO_FRETURN_LABEL:')
        self._w('    return SNO_FAILURE;')
        self._w('SNO_NRETURN_LABEL:')
        self._w('    return SNO_SUCCESS;')
        self._w('SNO_CONTINUE_LABEL:')
        self._w('    return SNO_SUCCESS;')
        self._w('}')
        self._w('')
        self._w('int main(void) {')
        self._w('    sno_runtime_init();')
        self._w('    sno_inc_init();   /* initialize all hardcoded inc functions */')
        self._w('    return sno_program();')
        self._w('}')


def emit_program(prog: Program) -> str:
    """Emit a complete C source file from a parsed SNOBOL4 program."""
    return StmtEmitter(prog).emit()


# -----------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------

if __name__ == '__main__':
    import sys
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'parser'))
    from sno_parser import parse_file

    if len(sys.argv) < 2:
        print('Usage: emit_c_stmt.py <file.sno>', file=sys.stderr)
        sys.exit(1)

    prog = parse_file(sys.argv[1])
    print(f'/* Parsed {len(prog.stmts)} statements */', file=sys.stderr)
    print(emit_program(prog))
