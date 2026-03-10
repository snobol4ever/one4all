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

def sno_val_to_c_literal(s: str) -> str:
    """
    Convert a Python str (holding a raw SNOBOL4 runtime value) to a valid
    C string literal including surrounding double-quotes.

    Rules (see HQ/STRING_ESCAPES.md):
      SNOBOL4 backslash is just a backslash — no escape meaning.
      In C a backslash MUST be doubled.
      In C a double-quote MUST be escaped.
      Control chars (newline, tab, etc.) MUST be escaped.

    This function is the ONLY place where SNOBOL4 values are converted to
    C literals. Never apply .replace() chains elsewhere — call this.
    """
    result = []
    for ch in s:
        if ch == '\\':
            result.append('\\\\')
        elif ch == '"':
            result.append('\\"')
        elif ch == '\n':
            result.append('\\n')
        elif ch == '\r':
            result.append('\\r')
        elif ch == '\t':
            result.append('\\t')
        elif ch == '\0':
            result.append('\\0')
        elif ch == '\f':
            result.append('\\f')
        elif ch == '\b':
            result.append('\\b')
        elif ord(ch) < 32 or ord(ch) > 126:
            result.append(f'\\x{ord(ch):02x}')
        else:
            result.append(ch)
    return '"' + ''.join(result) + '"'

def _c_label(label_name):
    """Convert a SNOBOL4 label to a valid C label — every special char gets a unique name."""
    char_map = {
        "'": "_q_",     "#": "_H_",     "@": "_A_",    "-": "_minus_",
        ".": "_dot_",   ":": "_col_",   "(": "_lp_",   ")": "_rp_",
        "<": "_lt_",    ">": "_gt_",    "!": "_bang_", "$": "_dol_",
        "?": "_q2_",    "&": "_amp_",   "*": "_star_", "^": "_hat_",
        "~": "_til_",   "%": "_pct_",   "/": "_sl_",   "|": "_bar_",
        "+": "_plus_",  "=": "_eq_",    ",": "_com_",  " ": "_sp_",
        "[": "_lb_",    "]": "_rb_",    "{": "_lc_",   "}": "_rc_",
        ";": "_sc_",    "\\": "_bs_",
    }
    s = label_name
    for ch, rep in char_map.items():
        s = s.replace(ch, rep)
    # Remove any remaining non-identifier chars
    import re as _re
    s = _re.sub(r'[^a-zA-Z0-9_]', '_x_', s)
    # Collapse multiple underscores
    s = _re.sub(r'_+', '_', s).strip('_')
    # Prefix if starts with digit
    if s and s[0].isdigit():
        s = 'L_' + s
    if not s:
        s = 'empty'
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
        return f'SNO_STR_VAL({sno_val_to_c_literal(e.val or "")})'

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
        return f'sno_concat_sv({l}, {r})'  # P003: propagates SNO_FAIL_VAL

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
            'IDENT':    lambda a: f'(sno_ident({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'DIFFER':   lambda a: f'(sno_differ({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'EQ':       lambda a: f'(sno_eq({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'NE':       lambda a: f'(sno_ne({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'LT':       lambda a: f'(sno_lt({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'LE':       lambda a: f'(sno_le({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'GT':       lambda a: f'(sno_gt({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'GE':       lambda a: f'(sno_ge({a[0]},{a[1]}) ? SNO_NULL_VAL : SNO_FAIL_VAL)',
            'ARRAY':    lambda a: f'sno_array_create({a[0]})',
            'TABLE':    lambda a: f'SNO_TABLE_VAL(sno_table_new())',
            'SORT':     lambda a: f'sno_sort_fn({a[0]})',
            'DEFINE':   lambda a: f'(sno_define_spec({a[0]}), SNO_NULL_VAL)',
            'DATA':     lambda a: f'(sno_data_define(sno_to_str({a[0]})), SNO_NULL_VAL)',
            'APPLY':    lambda a: f'sno_apply_val({a[0]}, (SnoVal[]){{ {", ".join(a[1:])} }}, {len(a)-1})',
            'EVAL':     lambda a: f'sno_eval({a[0]})',
            'OPSYN':    lambda a: f'(sno_opsyn({a[0]},{a[1]},{len(a)>2 and a[2] or "SNO_INT_VAL(2)"}), SNO_NULL_VAL)',
            'CODE':     lambda a: f'sno_code({a[0]})',
            # Pattern-returning builtins — when called from Expr context
            'NOTANY':   lambda a: f'sno_pat_notany(sno_to_str({a[0]}))',
            'ANY':      lambda a: f'sno_pat_any_cs(sno_to_str({a[0]}))',
            'SPAN':     lambda a: f'sno_pat_span(sno_to_str({a[0]}))',
            'BREAK':    lambda a: f'sno_pat_break_(sno_to_str({a[0]}))',
            'LEN':      lambda a: f'sno_pat_len(sno_to_int({a[0]}))',
            'POS':      lambda a: f'sno_pat_pos(sno_to_int({a[0]}))',
            'RPOS':     lambda a: f'sno_pat_rpos(sno_to_int({a[0]}))',
            'TAB':      lambda a: f'sno_pat_tab(sno_to_int({a[0]}))',
            'RTAB':     lambda a: f'sno_pat_rtab(sno_to_int({a[0]}))',
            'ARB':      lambda a: 'sno_pat_arb()',
            'ARBNO':    lambda a: f'sno_pat_arbno(sno_var_as_pattern({a[0]}))',
            'FENCE':    lambda a: f'sno_pat_fence_p(sno_var_as_pattern({a[0]}))',
            'BAL':      lambda a: 'sno_pat_bal()',
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
        return f'sno_pat_lit({sno_val_to_c_literal(p.val or "")})'

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
            'LEN':     lambda a: f'sno_pat_len(sno_to_int({a[0]}))',
            'POS':     lambda a: f'sno_pat_pos(sno_to_int({a[0]}))',
            'RPOS':    lambda a: f'sno_pat_rpos(sno_to_int({a[0]}))',
            'TAB':     lambda a: f'sno_pat_tab(sno_to_int({a[0]}))',
            'RTAB':    lambda a: f'sno_pat_rtab(sno_to_int({a[0]}))',
            'ARB':     lambda a: 'sno_pat_arb()',
            'REM':     lambda a: 'sno_pat_rem()',
            # a[0] is already an emitted C expression — use directly
            'FENCE':   lambda a: f'sno_pat_fence_p({a[0] if a else "sno_pat_epsilon()"})',
            'ARBNO':   lambda a: f'sno_pat_arbno({a[0]})',
            'BAL':     lambda a: 'sno_pat_bal()',
            'FAIL':    lambda a: 'sno_pat_fail()',
            'ABORT':   lambda a: 'sno_pat_abort()',
            'SUCCEED': lambda a: 'sno_pat_succeed()',
        }

        # Build C args — for string-taking builtins, emit args as string expressions
        str_builtins = {'SPAN', 'BREAK', 'ANY', 'NOTANY'}

        def emit_as_str(a):
            """Emit a pattern or expr arg as a C string expression."""
            from ir import PatExpr as _PatExpr, Expr as _Expr
            if isinstance(a, _Expr):
                return f'sno_to_str({emit_expr(a)})'
            if isinstance(a, _PatExpr):
                pk = a.kind
                if pk == 'lit':
                    # val is the raw SNOBOL4 character — convert once via canonical function
                    return sno_val_to_c_literal(a.val or '')
                if pk == 'var':
                    return f'sno_to_str(sno_var_get("{a.val}"))'
                if pk == 'cat':
                    l = emit_as_str(a.left)
                    r = emit_as_str(a.right)
                    return f'sno_concat({l}, {r})'
                # Fallback — emit as pattern and convert
                return f'sno_to_str({emit_pattern_expr(a)})'
            return 'sno_to_str(SNO_NULL_VAL)'

        if name in str_builtins:
            arg_str = emit_as_str(args[0]) if args else '""'
            str_map = {
                'SPAN':   f'sno_pat_span({arg_str})',
                'BREAK':  f'sno_pat_break_({arg_str})',
                'ANY':    f'sno_pat_any_cs({arg_str})',
                'NOTANY': f'sno_pat_notany({arg_str})',
            }
            return str_map[name]

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

import re as _re

class FuncInfo:
    """Metadata for a SNOBOL4-defined function."""
    def __init__(self, name, params, locals_, entry_label):
        self.name        = name
        self.params      = params   # list of str
        self.locals_     = locals_  # list of str
        self.entry_label = entry_label
        self.stmts       = []       # Stmt objects for this function's body
        self.first_idx   = 0        # stmt index of entry label


class StmtEmitter:
    def __init__(self, prog: Program):
        self.prog   = prog
        self.stmts  = prog.stmts
        self.lines  = []
        # Set during per-function emit so _map_special_label knows the suffix
        self._func_suffix = ''

    # ------------------------------------------------------------------
    # Top-level entry
    # ------------------------------------------------------------------

    def emit(self):
        self.lines = []
        funcs, main_stmts, main_offset = self._collect_and_partition()
        # Build the complete "main flow" as interleaved segments:
        # [main_stmts] + [residuals from each function in order]
        # Each residual is a (start_idx, stmts_list) pair
        main_segments = [(0, main_stmts)]
        for fi in funcs:
            if fi.residual_start is not None:
                seg = self.stmts[fi.residual_start:fi.residual_end]
                if seg:
                    main_segments.append((fi.residual_start, seg))
        self._emit_header(funcs)
        self._emit_main_body_segments(main_segments)
        self._emit_main_footer()
        for fi in funcs:
            self._emit_function(fi)
        self._emit_main_c(funcs)
        return '\n'.join(self.lines)

    # ------------------------------------------------------------------
    # Function discovery and partitioning
    # ------------------------------------------------------------------

    def _collect_and_partition(self):
        """
        Scan all DEFINE statements, collect function metadata, then partition
        self.stmts into (main_stmts, list_of_FuncInfo_with_stmts).

        Returns: (funcs, main_stmts, main_offset)
        """
        # Step 1: build label→idx map (case-insensitive)
        label_to_idx = {}
        for i, s in enumerate(self.stmts):
            if s.label:
                label_to_idx[s.label.upper()] = i

        # Step 2: collect DEFINE specs
        func_map = {}  # entry_label_upper → FuncInfo
        for s in self.stmts:
            if not (s.subject and s.subject.kind == 'call' and
                    s.subject.name and s.subject.name.upper() == 'DEFINE'):
                continue
            args = s.subject.args or []
            if not args:
                continue
            spec_val = args[0].val if hasattr(args[0], 'val') else None
            if not isinstance(spec_val, str):
                continue
            entry_val = (args[1].val if len(args) > 1 and
                         hasattr(args[1], 'val') else None)
            if not isinstance(entry_val, str):
                entry_val = None

            m = _re.match(r"(\w+)\(([^)]*)\)(.*)", spec_val)
            if m:
                fname   = m.group(1)
                params  = [p.strip() for p in m.group(2).split(',') if p.strip()]
                locals_ = [l.strip() for l in m.group(3).split(',') if l.strip()]
            else:
                fname   = spec_val
                params  = []
                locals_ = []

            entry = entry_val if entry_val else fname
            eu = entry.upper()
            if eu not in func_map and eu in label_to_idx:
                fi = FuncInfo(fname, params, locals_, entry)
                fi.end_label = None
                func_map[eu] = fi

        # Step 3: sort function entries by stmt index
        funcs_ordered = sorted(
            func_map.values(),
            key=lambda fi: label_to_idx[fi.entry_label.upper()]
        )
        for fi in funcs_ordered:
            fi.first_idx = label_to_idx[fi.entry_label.upper()]

        if not funcs_ordered:
            for fi in funcs_ordered:
                fi.stmts = []
                fi.residual_start = None
                fi.residual_end = None
            return funcs_ordered, self.stmts, 0

        # Step 4: collect ALL "skip" gotos
        first_func_idx = funcs_ordered[0].first_idx
        skip_targets = set()
        func_entry_idxs = set(fi.first_idx for fi in funcs_ordered)
        for s in self.stmts:
            if s.goto and s.goto.unconditional:
                tgt = s.goto.unconditional.upper()
                if tgt in label_to_idx:
                    tgt_idx = label_to_idx[tgt]
                    # It's a skip target if it's after any function entry
                    if any(tgt_idx > entry for entry in func_entry_idxs):
                        skip_targets.add(tgt)

        # Step 5: compute function body boundaries
        # A function section = [fi.first_idx .. end)
        # end = the skip target label's idx if it falls within this function's
        #       natural range, otherwise the next function's first_idx
        main_stmts = self.stmts[:first_func_idx]

        for k, fi in enumerate(funcs_ordered):
            start = fi.first_idx
            default_end = (funcs_ordered[k+1].first_idx
                           if k+1 < len(funcs_ordered) else len(self.stmts))

            # Look for a skip target within [start..default_end)
            best_end = default_end
            for tgt_upper in skip_targets:
                tgt_idx = label_to_idx.get(tgt_upper)
                if tgt_idx is not None and start < tgt_idx <= default_end:
                    if tgt_idx < best_end:
                        best_end = tgt_idx

            fi.stmts = self.stmts[start:best_end]
            if best_end < default_end:
                fi.residual_start = best_end
                fi.residual_end   = default_end
            else:
                fi.residual_start = None
                fi.residual_end   = None

        return funcs_ordered, main_stmts, 0

    # ------------------------------------------------------------------
    # Header: includes + forward declarations
    # ------------------------------------------------------------------

    def _emit_header(self, funcs):
        self._w('/* Generated by emit_c_stmt.py — Sprint 20 */')
        self._w('#include "snobol4.h"')
        self._w('#include "snobol4_inc.h"')
        self._w('#include <stdio.h>')
        self._w('')
        # Forward-declare each user function
        for fi in funcs:
            safe = _safe_c_name(fi.name)
            self._w(f'static SnoVal sno_uf_{safe}(SnoVal *_args, int _nargs);')
        self._w('')

    # ------------------------------------------------------------------
    # Main program body
    # ------------------------------------------------------------------

    def _emit_main_body_segments(self, segments):
        """Emit main program: interleaved segments (main stmts + residuals)."""
        self._func_suffix = '_MAIN'
        self._w('int sno_program(void) {')
        self._w('    /* Jump to first statement */')
        # Find first stmt across all segments
        for offset, stmts in segments:
            if stmts:
                first_lbl = _stmt_label(offset, stmts[0])
                self._w(f'    goto {first_lbl};')
                break
        self._w('')
        for offset, stmts in segments:
            self._emit_stmts(stmts, offset, is_main=True)

    def _emit_main_footer(self):
        self._w('/* --- program exit labels --- */')
        self._w('_SNO_PROG_END:')
        self._w('    return 0;')
        self._w('SNO_RETURN_LABEL_MAIN:')
        self._w('    return 0;')
        self._w('SNO_FRETURN_LABEL_MAIN:')
        self._w('    return 1;')
        self._w('SNO_NRETURN_LABEL_MAIN:')
        self._w('    return 0;')
        self._w('SNO_CONTINUE_LABEL_MAIN:')
        self._w('    return 0;')
        self._w('SNO_error:')
        self._w('    fprintf(stderr, "error label reached\\n");')
        self._w('    return 2;')
        self._w('SNO_err:')
        self._w('    fprintf(stderr, "err label reached\\n");')
        self._w('    return 2;')
        self._w('}')
        self._w('')

    # ------------------------------------------------------------------
    # Per-function C function
    # ------------------------------------------------------------------

    def _emit_function(self, fi):
        safe = _safe_c_name(fi.name)
        self._func_suffix = f'_{safe}'
        all_vars = fi.params + fi.locals_
        self._w(f'/* SNOBOL4 function: {fi.name}({", ".join(fi.params)}) locals={fi.locals_} */')
        self._w(f'static SnoVal sno_uf_{safe}(SnoVal *_args, int _nargs) {{')
        # Save existing values of params+locals, bind params from args
        if all_vars:
            self._w(f'    /* Save and bind params/locals */')
            for v in all_vars:
                sv = _safe_c_name(v)
                self._w(f'    SnoVal _save_{sv} = sno_var_get("{v}");')
            for i, p in enumerate(fi.params):
                self._w(f'    sno_var_set("{p}", (_nargs > {i}) ? _args[{i}] : SNO_NULL_VAL);')
            for l in fi.locals_:
                self._w(f'    sno_var_set("{l}", SNO_NULL_VAL);')
            self._w('')

        # Jump to entry label
        entry_c = _c_label(fi.entry_label)
        self._w(f'    goto {entry_c};')
        self._w('')

        # Emit function body stmts
        self._emit_stmts(fi.stmts, fi.first_idx, is_main=False)

        # Return labels — per function
        ret_lbl  = f'SNO_RETURN_LABEL_{safe}'
        fret_lbl = f'SNO_FRETURN_LABEL_{safe}'
        nret_lbl = f'SNO_NRETURN_LABEL_{safe}'
        cont_lbl = f'SNO_CONTINUE_LABEL_{safe}'
        self._w(f'{ret_lbl}:')
        if all_vars:
            for v in all_vars:
                sv = _safe_c_name(v)
                self._w(f'    sno_var_set("{v}", _save_{sv});')
        self._w(f'    return SNO_NULL_VAL;')
        self._w(f'{fret_lbl}:')
        if all_vars:
            for v in all_vars:
                sv = _safe_c_name(v)
                self._w(f'    sno_var_set("{v}", _save_{sv});')
        self._w(f'    return SNO_FAIL_VAL;')
        self._w(f'{nret_lbl}:')
        if all_vars:
            for v in all_vars:
                sv = _safe_c_name(v)
                self._w(f'    sno_var_set("{v}", _save_{sv});')
        self._w(f'    return SNO_NULL_VAL;  /* NRETURN */')
        self._w(f'{cont_lbl}:')
        if all_vars:
            for v in all_vars:
                sv = _safe_c_name(v)
                self._w(f'    sno_var_set("{v}", _save_{sv});')
        self._w(f'    return SNO_NULL_VAL;  /* CONTINUE */')
        self._w(f'}}')
        self._w('')

    # ------------------------------------------------------------------
    # main() + function registration
    # ------------------------------------------------------------------

    def _emit_main_c(self, funcs):
        self._w('int main(void) {')
        self._w('    sno_runtime_init();')
        self._w('    sno_inc_init();')
        for fi in funcs:
            safe = _safe_c_name(fi.name)
            spec = f'{fi.name}({",".join(fi.params)}){",".join(fi.locals_)}'
            self._w(f'    sno_define("{spec}", sno_uf_{safe});')
        self._w('    return sno_program();')
        self._w('}')

    # ------------------------------------------------------------------
    # Statement list emitter (shared by main and functions)
    # ------------------------------------------------------------------

    def _emit_stmts(self, stmts, offset, is_main):
        n = len(stmts)
        for i, stmt in enumerate(stmts):
            abs_i = offset + i
            lbl = _stmt_label(abs_i, stmt)
            next_lbl = (_stmt_label(offset + i + 1, stmts[i+1])
                        if i+1 < n else ('_SNO_PROG_END' if is_main else
                                         f'SNO_RETURN_LABEL{self._func_suffix}'))

            self._w(f'{lbl}: {{  /* L{stmt.lineno} */')
            self._w(f'    sno_comm_stno({stmt.lineno});')

            succ_lbl = next_lbl
            fail_lbl = next_lbl

            if stmt.goto:
                g = stmt.goto
                if g.unconditional:
                    cl = _c_label(g.unconditional)
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

            self._emit_stmt_body(stmt, succ_lbl, fail_lbl, next_lbl)
            self._w(f'}}')
            self._w()

    def _emit_stmt_body(self, stmt, succ_lbl, fail_lbl, next_lbl):
        subj  = stmt.subject
        pat   = stmt.pattern
        repl  = stmt.replacement

        # Case 1: pure function call (no subject, no pattern)
        if subj is not None and subj.kind == 'call' and pat is None and repl is None:
            name = subj.name.upper() if subj.name else ''
            ca = [emit_expr(a) for a in (subj.args or [])]
            if name == 'DEFINE':
                self._w(f'    sno_define_spec({ca[0] if ca else "SNO_NULL_VAL"});')
            elif name == 'DATA':
                self._w(f'    sno_data_define(sno_to_str({ca[0] if ca else "SNO_NULL_VAL"}));')
            elif name == 'OPSYN':
                if len(ca) >= 3:
                    self._w(f'    sno_opsyn({", ".join(ca)});')
                else:
                    self._w(f'    sno_opsyn2({", ".join(ca)});')
            else:
                args_c = ', '.join(ca)
                n_args = len(ca)
                if n_args:
                    self._w(f'    {{')
                    self._w(f'        SnoVal _ret = sno_apply("{subj.name}", (SnoVal[{n_args}]){{{args_c}}}, {n_args});')
                    self._w(f'        if (sno_is_fail(_ret)) goto {fail_lbl};')
                    self._w(f'        goto {succ_lbl};')
                    self._w(f'    }}')
                    return
                else:
                    self._w(f'    {{')
                    self._w(f'        SnoVal _ret = sno_apply("{subj.name}", NULL, 0);')
                    self._w(f'        if (sno_is_fail(_ret)) goto {fail_lbl};')
                    self._w(f'        goto {succ_lbl};')
                    self._w(f'    }}')
                    return
            self._w(f'    goto {succ_lbl};')

        # Case 2: subject = replacement (assignment, no pattern)
        elif subj is not None and repl is not None and pat is None:
            rhs_c = emit_expr(repl)
            self._w(f'    {{')
            self._w(f'        SnoVal _rhs = {rhs_c};')
            self._w(f'        if (sno_is_fail(_rhs)) goto {fail_lbl};')
            self._w(f'        {emit_assign_target(subj, "_rhs")[4:]}')
            self._w(f'        goto {succ_lbl};')
            self._w(f'    }}')

        # Case 3: subject pattern = replacement (pattern match + optional replacement)
        elif subj is not None and pat is not None:
            subj_c  = emit_expr(subj)
            pat_c   = emit_pattern_expr(pat)
            repl_c  = emit_expr(repl) if repl else None

            if repl_c:
                self._w(f'    {{')
                self._w(f'        SnoVal _subj = {subj_c};')
                self._w(f'        int _ok = sno_match_and_replace(&_subj, {pat_c}, {repl_c});')
                self._w(f'        {emit_assign_target(subj, "_subj")[4:]}')
                self._w(f'        if (_ok) goto {succ_lbl};')
                self._w(f'        else goto {fail_lbl};')
                self._w(f'    }}')
            else:
                self._w(f'    {{')
                self._w(f'        SnoVal _subj = {subj_c};')
                self._w(f'        int _ok = sno_match_pattern({pat_c}, sno_to_str(_subj));')
                self._w(f'        if (_ok) goto {succ_lbl};')
                self._w(f'        else goto {fail_lbl};')
                self._w(f'    }}')

        # Case 4: bare goto only
        elif subj is None and pat is None and repl is None:
            self._w(f'    goto {succ_lbl};')

        # Case 5: OUTPUT = expr (no subject, has replacement)
        elif subj is None and repl is not None:
            rhs_c = emit_expr(repl)
            self._w(f'    {{')
            self._w(f'        SnoVal _rhs = {rhs_c};')
            self._w(f'        if (sno_is_fail(_rhs)) goto {fail_lbl};')
            self._w(f'        sno_output_val(_rhs);')
            self._w(f'        goto {succ_lbl};')
            self._w(f'    }}')

        else:
            self._w(f'    /* unhandled stmt shape */')
            self._w(f'    goto {next_lbl};')

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _map_special_label(self, cl, orig):
        upper = orig.upper()
        sfx = self._func_suffix  # e.g. '_findRefs' or '_MAIN'
        if upper == 'END':
            return '_SNO_PROG_END'
        if upper == 'RETURN':
            return f'SNO_RETURN_LABEL{sfx}'
        if upper == 'FRETURN':
            return f'SNO_FRETURN_LABEL{sfx}'
        if upper == 'NRETURN':
            return f'SNO_NRETURN_LABEL{sfx}'
        if upper == 'CONTINUE':
            return f'SNO_CONTINUE_LABEL{sfx}'
        return cl

    def _w(self, line=''):
        self.lines.append(line)


def _safe_c_name(s):
    """Convert a SNOBOL4 identifier to a safe C identifier."""
    r = _re.sub(r'[^a-zA-Z0-9]', '_', s)
    if r and r[0].isdigit():
        r = 'f_' + r
    return r or 'anon'


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
