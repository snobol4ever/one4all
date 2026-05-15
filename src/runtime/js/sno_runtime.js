'use strict';
/*
 * sno_runtime.js — SNOBOL4 JavaScript runtime
 *
 * Provides the primitive operations used by emit_js.c output.
 *
 * Design decisions (SJ-1, do not re-debate):
 *   - _vars: Proxy — set trap for OUTPUT writes to process.stdout
 *   - _FAIL: sentinel object (not null, which is valid SNOBOL4 null/empty)
 *   - All values are JS strings, numbers, or null (SNOBOL4 null)
 *   - Arithmetic coerces via _num(); concatenation via _str()
 *
 * Sprint: SJ-2  Milestone: M-SJ-A01
 * Authors: Lon Jones Cherryholmes (arch), Claude Sonnet 4.6 (impl)
 */

/* -----------------------------------------------------------------------
 * Failure sentinel
 * ----------------------------------------------------------------------- */
const _FAIL = Object.freeze({ _sno_fail: true });

function _is_fail(v) { return v === _FAIL; }

/* -----------------------------------------------------------------------
 * _vars — SNOBOL4 variable store with IO trapping
 * ----------------------------------------------------------------------- */
const _store = {};

const _vars = new Proxy(_store, {
    set(o, k, v) {
        /* SNOBOL4 identifiers are case-insensitive — normalize to uppercase.
         * Symbol keys (JS internals) pass through via the typeof guard. */
        if (typeof k === 'string') k = k.toUpperCase();
        o[k] = v;
        if (k === 'OUTPUT') {
            process.stdout.write(_str(v) + '\n');
        }
        return true;
    },
    get(o, k) {
        if (typeof k === 'string') k = k.toUpperCase();
        if (k in o) return o[k];
        if (k === 'INPUT') {
            /* Synchronous line-at-a-time read from stdin — Node.js only.
             * Read one byte at a time to avoid consuming beyond the newline.
             * _stdin_buf holds leftover bytes from previous reads (shouldn't
             * occur with byte-at-a-time, but kept for safety). */
            try {
                const fs = require('fs');
                const oneByte = Buffer.alloc(1);
                const chars = [];
                while (true) {
                    const n = fs.readSync(0, oneByte, 0, 1, null);
                    if (n <= 0) break;              // EOF
                    const ch = oneByte[0];
                    if (ch === 10) break;           // newline — end of line
                    if (ch !== 13) chars.push(ch);  // skip CR
                }
                if (chars.length === 0) return _FAIL;  // EOF with no data
                return Buffer.from(chars).toString();
            } catch(e) { return _FAIL; }
        }
        return null; /* unset variable = SNOBOL4 null */
    }
});

/* -----------------------------------------------------------------------
 * Type coercion
 * ----------------------------------------------------------------------- */

/** Coerce to SNOBOL4 string (null → empty string) */
function _str(v) {
    if (v === null || v === undefined) return '';
    if (v === _FAIL) return '';
    if (typeof v === 'number') {
        if (Number.isInteger(v)) return String(v);
        /* SNOBOL4 real format: always has decimal point; 1.0→"1.", 0.001→"0.001" */
        let s = String(v);
        if (s.indexOf('.') < 0 && s.indexOf('e') < 0) s += '.';
        s = s.replace(/(\.\d*?)0+$/, '$1');
        return s;
    }
    if (_is_real(v)) {
        /* tagged real — format as SNOBOL4 real: 1.0→"1.", 0.001→"0.001" */
        let s = String(v.v);
        if (s.indexOf('.') < 0 && s.indexOf('e') < 0) s += '.';
        s = s.replace(/(\.\d*?)0+$/, '$1');
        return s;
    }
    if (typeof v === 'object') return '';  /* DATA/ARRAY/TABLE objects stringify as '' */
    return String(v);  /* plain strings (including '1.0' quoted literals) unchanged */
}

/** Coerce to number; throws if not numeric */
function _num(v) {
    if (v === null || v === undefined || v === '') return 0;
    if (_is_real(v)) return v.v;
    const n = Number(v);
    if (!isFinite(n)) throw new Error('SNOBOL4 type error: not a number: ' + String(v));
    return n;
}

/* Return true if value is an integer (not real) */
function _is_int(v) {
    if (v === null || v === undefined) return true;   /* SNOBOL4 null = integer 0 */
    if (_is_real(v)) return false;                    /* tagged real object */
    if (typeof v === 'string') return true;           /* plain strings are integer-convertible */
    return typeof v === 'number' && Number.isInteger(v);
}

/* Return true if value looks numeric */
function _is_numeric(v) {
    if (v === null || v === undefined || v === '') return false;
    if (_is_real(v)) return true;
    return isFinite(Number(v));
}

/* -----------------------------------------------------------------------
 * Arithmetic — match SNOBOL4 semantics (integer if both integer-valued)
 * ----------------------------------------------------------------------- */

function _int_if_possible(n) {
    return Number.isInteger(n) ? n : n;
}

/* When either operand is real, tag result as real string so _str() formats it with '.'.
 * This preserves SNOBOL4 real type through whole-number results (e.g. 2.0+3.0=5.) */
/* Real-typed value wrapper — produced only by arithmetic, never by quoted literals.
 * Keeps real type visible to _is_int/_str/_num without dot-sniffing quoted strings. */
function _mkreal(r) { return Object.freeze({ _r: 1, v: r }); }
function _is_real(v) { return v !== null && typeof v === 'object' && v._r === 1; }
function _real_result(r) { return _mkreal(typeof r === 'number' ? r : Number(r)); }

function _add(a, b) {
    if (a === _FAIL || b === _FAIL) return _FAIL;
    const an = _num(a), bn = _num(b);
    const r = an + bn;
    return (_is_int(a) && _is_int(b)) ? Math.trunc(r) : _real_result(r);
}
function _sub(a, b) {
    if (a === _FAIL || b === _FAIL) return _FAIL;
    const an = _num(a), bn = _num(b);
    const r = an - bn;
    return (_is_int(a) && _is_int(b)) ? Math.trunc(r) : _real_result(r);
}
function _mul(a, b) {
    if (a === _FAIL || b === _FAIL) return _FAIL;
    const an = _num(a), bn = _num(b);
    const r = an * bn;
    return (_is_int(a) && _is_int(b)) ? Math.trunc(r) : _real_result(r);
}
function _div(a, b) {
    if (a === _FAIL || b === _FAIL) return _FAIL;
    const an = _num(a), bn = _num(b);
    if (bn === 0) throw new Error('SNOBOL4: division by zero');
    if (_is_int(a) && _is_int(b)) return Math.trunc(an / bn);
    return _real_result(an / bn);
}
function _pow(a, b) {
    if (a === _FAIL || b === _FAIL) return _FAIL;
    const r = Math.pow(_num(a), _num(b));
    if (_is_int(a) && _is_int(b)) return r;  /* int ** int → int (may be non-integer JS num) */
    return _real_result(r);  /* real base or real exponent → real result */
}
function _uplus(a)  { return _num(a); }

/* -----------------------------------------------------------------------
 * String concatenation (n-ary)
 * ----------------------------------------------------------------------- */

function _cat(...args) {
    for (const a of args) if (a === _FAIL) return _FAIL;
    return args.map(_str).join('');
}

/* -----------------------------------------------------------------------
 * Keyword access (&STCOUNT etc.)
 * ----------------------------------------------------------------------- */

const _kw_store = {
    STCOUNT: 0,
    STLIMIT: -1,
    ALPHABET: (function() { let s=''; for(let i=0;i<256;i++) s+=String.fromCharCode(i); return s; })(),
    DIGITS: '0123456789',
    MAXLNGTH: 5000,
    TRIM: 0,
    RTNTYPE: '',
    ERRTEXT: '',
    ERRLIMIT: 0,
    FNNAME: '',
    LASTNO: 0,
    UCASE: (function() { let s=''; for(let i=65;i<=90;i++) s+=String.fromCharCode(i); return s; })(),
    LCASE: (function() { let s=''; for(let i=97;i<=122;i++) s+=String.fromCharCode(i); return s; })(),
};

function _kw(name) {
    const k = name.replace(/^&/, '');
    if (k in _kw_store) return _kw_store[k];
    return null;
}

/* -----------------------------------------------------------------------
 * Function application — builtin dispatch
 * ----------------------------------------------------------------------- */

const _builtins = {
    SIZE(args)    { return _str(args[0]).length; },
    TRIM(args)    { return _str(args[0]).trimEnd(); },
    DUPL(args)    { const s=_str(args[0]), n=_num(args[1]); return s.repeat(Math.max(0,n)); },
    SUBSTR(args)  { const s=_str(args[0]),i=_num(args[1])-1,n=args[2]!=null?_num(args[2]):s.length-i; return s.substr(i,n); },
    IDENT(args)   {
        const a = args[0], b = args[1];
        const eq = (_is_numeric(a) && _is_numeric(b))
            ? _num(a) === _num(b)
            : _str(a) === _str(b);
        return eq ? '' : _FAIL;
    },
    DIFFER(args)  {
        const a = args[0], b = args[1];
        const eq = (_is_numeric(a) && _is_numeric(b))
            ? _num(a) === _num(b)
            : _str(a) === _str(b);
        return eq ? _FAIL : '';
    },
    LT(args)      { return _num(args[0])<_num(args[1])    ? '' : _FAIL; },
    LE(args)      { return _num(args[0])<=_num(args[1])   ? '' : _FAIL; },
    GT(args)      { return _num(args[0])>_num(args[1])    ? '' : _FAIL; },
    GE(args)      { return _num(args[0])>=_num(args[1])   ? '' : _FAIL; },
    EQ(args)      { return _num(args[0])===_num(args[1])  ? '' : _FAIL; },
    NE(args)      { return _num(args[0])!==_num(args[1])  ? '' : _FAIL; },
    LGT(args)     { return _str(args[0]) >  _str(args[1]) ? '' : _FAIL; },
    LLT(args)     { return _str(args[0]) <  _str(args[1]) ? '' : _FAIL; },
    LGE(args)     { return _str(args[0]) >= _str(args[1]) ? '' : _FAIL; },
    LLE(args)     { return _str(args[0]) <= _str(args[1]) ? '' : _FAIL; },
    LEQ(args)     { return _str(args[0]) === _str(args[1]) ? '' : _FAIL; },
    LNE(args)     { return _str(args[0]) !== _str(args[1]) ? '' : _FAIL; },
    INTEGER(args) { const n=Number(args[0]); return Number.isInteger(n) ? n : _FAIL; },
    REAL(args)    { const n=Number(args[0]); return isFinite(n) ? n : _FAIL; },
    CONVERT(args) { /* basic: return arg[0] */ return args[0]; },
    DATATYPE(args){ const v=args[0]; if(_is_real(v)) return 'REAL'; if(typeof v==='number'||(_is_int(v)&&v!==null&&v!=='')) return 'INTEGER'; return 'STRING'; },
    INPUT(args)   { return _vars['INPUT']; },
    OUTPUT(args)  { if(args[0]!==undefined) _vars['OUTPUT']=args[0]; return args[0]; },
    CHAR(args)    { return String.fromCharCode(_num(args[0])); },
    CODE(args)    { const s=_str(args[0]); return s.length ? s.charCodeAt(0) : _FAIL; },
    LPAD(args)    { const s=_str(args[0]),n=_num(args[1]),c=args[2]!=null?_str(args[2]):''; return s.padStart(n,c[0]||' '); },
    RPAD(args)    { const s=_str(args[0]),n=_num(args[1]),c=args[2]!=null?_str(args[2]):''; return s.padEnd(n,c[0]||' '); },
    REPLACE(args) { /* REPLACE(s, from, to) */ const s=_str(args[0]),f=_str(args[1]),t=_str(args[2]); let r=''; for(let i=0;i<s.length;i++){const fi=f.indexOf(s[i]);r+=fi>=0?(t[fi]??''):s[i];}return r; },
    REVERSE(args) { return _str(args[0]).split('').reverse().join(''); },
    UPPER(args)   { return _str(args[0]).toUpperCase(); },
    LOWER(args)   { return _str(args[0]).toLowerCase(); },
    ABORT(args)   { process.exit(1); },
    TIME(args)    { return Date.now(); },
    FENCE(args)   { return args[0] !== undefined ? args[0] : ''; },
    FAIL(args)    { return _FAIL; },
    SUCCEED(args) { return args[0] !== undefined ? args[0] : ''; },
    APPLY(args)   { return _apply(_str(args[0]), args.slice(1)); },
    REMDR(args)   { const a=_num(args[0]),b=_num(args[1]); if(b===0) throw new Error('SNOBOL4: remdr by zero'); return Math.trunc(a)%Math.trunc(b); },
    DEFINE(args)  { /* stub — user-defined functions not yet wired */ return null; },
    ARRAY(args)   { /* stub */ return []; },
    TABLE(args)   { /* stub */ return {}; },
    PROTOTYPE(args){ return null; },
    ARB(args)     { return ''; /* zero-width succeed in value context */ },
    REM(args)     { return ''; },
    ANY(args)     { return _str(args[0])[0] || _FAIL; },
    NOTANY(args)  { return _FAIL; /* stub */ },
    SPAN(args)    { return _FAIL; /* stub */ },
    BREAK(args)   { return _FAIL; /* stub */ },
    LEN(args)     { return ''; /* stub zero-width */ },
    POS(args)     { return ''; /* stub */ },
    RPOS(args)    { return ''; /* stub */ },
    TAB(args)     { return ''; /* stub */ },
    RTAB(args)    { return ''; /* stub */ },
    ARBNO(args)   { return ''; /* stub */ },
};

function _apply(name, args) {
    const uname = name.toUpperCase();
    if (uname in _builtins) return _builtins[uname](args);
    /* user-defined: look up in _user_fns */
    if (name in _user_fns) return _user_fns[name](args);
    throw new Error('SNOBOL4: undefined function: ' + name);
}

const _user_fns = {};

/* -----------------------------------------------------------------------
 * Pattern matching — sno_engine.js (SJ-5, M-SJ-B01)
 * ----------------------------------------------------------------------- */

const _engine = require(process.env.SNO_ENGINE ||
    require('path').join(__dirname, 'sno_engine.js'));

/* Inject vars hook so CAPT_IMM/CAPT_COND can write to _vars */
_engine._set_vars_hook((v, text) => { _vars[v] = text; });

/**
 * _match(subject, pat_node) → {matched,start,end} | null
 * pat_node is a pattern tree built by PAT_* helpers (or a string literal).
 * Used by pattern-match statements emitted by emit_js.c.
 */
function _match(subject, pat_node) {
    if (pat_node === null || pat_node === undefined || pat_node === _FAIL)
        return null;
    return _engine.sno_search(_str(subject), pat_node);
}

/**
 * _match_anchored(subject, pat_node) → {matched,start,end} | null
 * Anchored at position 0 (for &ANCHOR / POS(0) patterns).
 */
function _match_anchored(subject, pat_node) {
    if (pat_node === null || pat_node === undefined || pat_node === _FAIL)
        return null;
    return _engine.sno_match(_str(subject), pat_node);
}

/* -----------------------------------------------------------------------
 * Stack machine state (for scalar IR emission — SJ4-JS-2)
 * ----------------------------------------------------------------------- */

let _stack = [];              /* Value stack for scalar operations */
let _last_ok = true;          /* Last pattern match success flag */
let _stno = 0;                /* Statement number for debugging */

function _init() {
    _stack = [];
    _last_ok = true;
    _stno = 0;
}

function _finalize() {
    _stack = [];
    _last_ok = true;
}

/* Stack operations */
function push_int(n)         { _stack.push(n); }
function push_str(s, len)    { _stack.push(_str(s)); }
function push_real_bits(bits){ _stack.push(_mkreal(bits)); }
function push_null()         { _stack.push(null); }
function push_var(name) {
    const uname = name.toUpperCase();
    if (uname in _kw_store) {
        _stack.push(_kw(uname));
    } else {
        _stack.push(_vars[name]);
    }
}
function pop_void()          { _stack.pop(); }

function store_var(name) {
    const v = _stack[_stack.length - 1];
    _vars[name] = v;
}

function concat() {
    if (_stack.length < 2) throw new Error('SNOBOL4: concat underflow');
    const b = _stack.pop();
    const a = _stack.pop();
    _stack.push(_cat(a, b));
}

function neg() {
    const v = _stack.pop();
    _stack.push(_num(v) * -1);
}

function exp_op() {
    const e = _stack.pop();
    const b = _stack.pop();
    _stack.push(_pow(b, e));
}

function coerce_num() {
    const v = _stack.pop();
    if (_is_real(v)) {
        _stack.push(v);  /* Already a real; keep it */
    } else {
        const n = _num(v);
        if (!Number.isInteger(n)) {
            _stack.push(_mkreal(n));  /* Non-integer numeric → real */
        } else {
            _stack.push(n);  /* Integer stays as plain number */
        }
    }
}

function arith(op) {
    if (_stack.length < 2) throw new Error('SNOBOL4: arith underflow');
    const b = _stack.pop();
    const a = _stack.pop();
    let r;
    switch(op) {
        case 'add': r = _add(a, b); break;
        case 'sub': r = _sub(a, b); break;
        case 'mul': r = _mul(a, b); break;
        case 'div': r = _div(a, b); break;
        case 'mod': r = _num(a) % _num(b); break;
        default: throw new Error('SNOBOL4: unknown arith op: ' + op);
    }
    _stack.push(r);
}

function acomp(op) {
    if (_stack.length < 2) throw new Error('SNOBOL4: acomp underflow');
    const b = _stack.pop();
    const a = _stack.pop();
    let r;
    switch(op) {
        case 'lt': r = _num(a) < _num(b); break;
        case 'le': r = _num(a) <= _num(b); break;
        case 'eq': r = _num(a) === _num(b); break;
        case 'ne': r = _num(a) !== _num(b); break;
        case 'ge': r = _num(a) >= _num(b); break;
        case 'gt': r = _num(a) > _num(b); break;
        default: throw new Error('SNOBOL4: unknown acomp op: ' + op);
    }
    _last_ok = r ? true : false;
}

function lcomp(op) {
    if (_stack.length < 2) throw new Error('SNOBOL4: lcomp underflow');
    const b = _stack.pop();
    const a = _stack.pop();
    let r;
    switch(op) {
        case 'lt': r = _str(a) < _str(b); break;
        case 'le': r = _str(a) <= _str(b); break;
        case 'eq': r = _str(a) === _str(b); break;
        case 'ne': r = _str(a) !== _str(b); break;
        case 'ge': r = _str(a) >= _str(b); break;
        case 'gt': r = _str(a) > _str(b); break;
        default: throw new Error('SNOBOL4: unknown lcomp op: ' + op);
    }
    _last_ok = r ? true : false;
}

function last_ok()    { return _last_ok; }
function set_last_ok(v) { _last_ok = v ? true : false; }
function set_stno(n)  { _stno = n; }
function halt_tos()   { if (_stack.length > 0) { const v = _stack.pop(); if (v !== _FAIL) process.stdout.write(_str(v) + '\n'); } }

function call(name, nargs) {
    if (_stack.length < nargs) throw new Error('SNOBOL4: call underflow');
    const args = _stack.splice(_stack.length - nargs, nargs);
    const result = _apply(name, args);
    _stack.push(result);
}

function do_return(kind, cond) {
    /* Stub — full return semantics deferred to SJ4-JS-3 */
}

/* -----------------------------------------------------------------------
 * MatchState factory for pattern matching (for emitted pattern factories)
 * ----------------------------------------------------------------------- */

function MatchState(subject) {
    return {
        sigma: subject,
        delta: 0,
        omega: subject.length,
        _do_capture(varname, text, immediate) {
            if (immediate) {
                _vars[varname] = text;
            } else {
                if (!this._pending) this._pending = [];
                this._pending.push({ varname, text });
            }
        },
        _commit_caps() {
            if (this._pending) {
                for (const cap of this._pending) {
                    _vars[cap.varname] = cap.text;
                }
                this._pending = [];
            }
        },
        _discard_caps() {
            if (this._pending) this._pending = [];
        },
        _pending: []
    };
}

/* -----------------------------------------------------------------------
 * Exports
 * ----------------------------------------------------------------------- */

/* Re-export PAT_* builders so emitted code can use them */
const {
    PAT_lit, PAT_alt, PAT_seq, PAT_any, PAT_notany,
    PAT_span, PAT_break, PAT_arb, PAT_rem,
    PAT_len, PAT_pos, PAT_rpos, PAT_tab, PAT_rtab,
    PAT_fence, PAT_succeed, PAT_fail, PAT_abort, PAT_bal,
    PAT_arbno, PAT_capt_imm, PAT_capt_cond,
} = _engine;

function _peek() { return _stack.length > 0 ? _stack[_stack.length - 1] : null; }

module.exports = {
    /* Core runtime */
    _vars, _FAIL, _is_fail, _str, _num, _cat,
    _add, _sub, _mul, _div, _pow, _apply, _kw, _is_int, _is_real, _real_result,
    _match, _match_anchored, _user_fns, _peek,
    /* Pattern builders */
    PAT_lit, PAT_alt, PAT_seq, PAT_any, PAT_notany,
    PAT_span, PAT_break, PAT_arb, PAT_rem,
    PAT_len, PAT_pos, PAT_rpos, PAT_tab, PAT_rtab,
    PAT_fence, PAT_succeed, PAT_fail, PAT_abort, PAT_bal,
    PAT_arbno, PAT_capt_imm, PAT_capt_cond,
    /* Stack machine API (SJ4-JS-2) */
    _init, _finalize,
    push_int, push_str, push_real_bits, push_null, push_var,
    store_var, pop_void, concat, neg, exp_op, coerce_num,
    arith, acomp, lcomp, last_ok, set_last_ok, set_stno,
    halt_tos, call, do_return,
    MatchState,
};
