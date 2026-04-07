#!/usr/bin/env python3
"""
tools/beautify.py — snobol4ever C/H source beautifier

RULES:
  • 4 spaces per indent level, no tab characters anywhere
  • No blank lines inside a function body
  • Standalone block comments inside a function (/* ... */ alone on a line)
    are queued and attached as end-of-line comments on the next code line
  • Binary operators get exactly one space each side:
      ==  !=  <=  >=  &&  ||  <<  >>  +=  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=
  • Multiple interior spaces collapsed to one (leading indent preserved)
  • Line width target: 120 columns (configurable via --width)
  • /*===...===*/  major separator (between functions): exactly 120 chars
  • /*---...---*/  minor separator (inside large functions): exactly 120 chars

VERIFICATION (--verify):
  Compiles each file before/after with -O0 (no -g); compares .text section hashes.
  A hash match confirms the transform was semantically transparent.

USAGE:
  python3 tools/beautify.py [--verify] [--dry-run] [--width N] FILE_OR_DIR ...
"""

import re, sys, os, hashlib, subprocess, tempfile, argparse
from typing import List, Tuple, Optional

DEFAULT_WIDTH = 120
INDENT_W      = 4

SKIP_RE = [
    re.compile(r'\.tab\.[ch]$'),
    re.compile(r'\.lex\.[ch]$'),
    re.compile(r'^lex\.'),
    re.compile(r'unicode_alpha_ranges\.h$'),
]

def should_skip(path):
    base = os.path.basename(path)
    return any(p.search(base) for p in SKIP_RE)

# ── tokeniser ────────────────────────────────────────────────────────────────

def tokenize(s):
    tokens = []
    i, n = 0, len(s)
    buf = ''
    def flush():
        nonlocal buf
        if buf:
            tokens.append(('code', buf))
            buf = ''
    while i < n:
        if s[i:i+2] == '//':
            flush(); tokens.append(('lc', s[i:])); return tokens
        if s[i:i+2] == '/*':
            flush()
            end = s.find('*/', i + 2)
            if end == -1:
                tokens.append(('bc', s[i:])); return tokens
            tokens.append(('bc', s[i:end+2])); i = end + 2; continue
        if s[i] == '"':
            flush(); j = i + 1
            while j < n:
                if s[j] == '\\': j += 2; continue
                if s[j] == '"':  j += 1; break
                j += 1
            tokens.append(('str', s[i:j])); i = j; continue
        if s[i] == "'":
            flush(); j = i + 1
            while j < n:
                if s[j] == '\\': j += 2; continue
                if s[j] == "'":  j += 1; break
                j += 1
            tokens.append(('chr', s[i:j])); i = j; continue
        buf += s[i]; i += 1
    flush()
    return tokens

# ── operator normalisation ───────────────────────────────────────────────────
#
# Each operator is matched with negative lookahead/lookbehind to prevent
# a shorter op from matching inside a longer one (e.g. >> inside >>=).

def _make_op_re(op):
    """
    Build a pair of regexes (before, after) that add spaces around `op`
    without touching a longer operator that contains `op` as a prefix/suffix.
    """
    esc = re.escape(op)
    # Characters that would extend this op into a longer one:
    # For <<  don't match <<= ;  for >>  don't match >>= ;  for <  don't match <= << <<=
    # Strategy: after the op, require not followed by = or the same char
    last  = op[-1]
    first = op[0]
    # negative lookahead: not followed by = (catches += matching inside <<=, etc.)
    # We already process longest-first, so by the time we hit << the >>= is already spaced.
    # But to be safe, add explicit negative lookahead for =
    no_extend = r'(?![=\<\>])' if last in '<>' else r'(?!=)'
    re_before = r'(?<! )' + esc            # no space immediately before
    re_after  = esc + no_extend            # op not followed by extending char
    return re_before, re_after

# Ordered longest-first so <<=/>>=  are handled before <</>>/<=/>= etc.
_BINOPS = ['<<=', '>>=',
           '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=',
           '==', '!=', '<=', '>=', '&&', '||', '<<', '>>']

def norm_ops(s):
    """Normalise spacing in a pure-code segment (no strings, no comments)."""
    # Collapse interior multi-spaces (lookbehind \S preserves leading indent)
    s = re.sub(r'(?<=\S)  +', ' ', s)
    # Single-pass substitution: match longest op first (alternation is left-to-right greedy).
    # Replace function adds spaces on both sides atomically — avoids >> matching inside >>=.
    _PAT = r'(<<=|>>=|\+=|-=|\*=|/=|%=|&=|\|=|\^=|==|!=|<=|>=|&&|\|\||<<|>>)'
    s = re.sub(_PAT, lambda m: ' ' + m.group(0) + ' ', s)
    s = re.sub(r'(?<=\S)  +', ' ', s)
    return s

def normalize_operators(line):
    return ''.join(norm_ops(t) if k == 'code' else t for k, t in tokenize(line))

# ── rule-line helpers ────────────────────────────────────────────────────────

def is_major_rule(line):
    s = line.strip()
    return len(s) >= 6 and s.startswith('/*=') and s.endswith('=*/')

def is_minor_rule(line):
    s = line.strip()
    return len(s) >= 6 and s.startswith('/*-') and s.endswith('-*/')

def is_rule_line(line):
    return is_major_rule(line) or is_minor_rule(line)

def make_rule(ch, indent_n, width):
    prefix = ' ' * indent_n
    return prefix + '/*' + ch * (width - len(prefix) - 4) + '*/'

def regen_rule(line, width):
    ch = '=' if is_major_rule(line) else '-'
    return make_rule(ch, len(line) - len(line.lstrip()), width)

# ── brace counting ───────────────────────────────────────────────────────────

def count_braces(line):
    opens = closes = 0
    for k, t in tokenize(line):
        if k == 'code':
            opens += t.count('{'); closes += t.count('}')
    return opens, closes

# ── comment helpers ──────────────────────────────────────────────────────────

def is_pure_comment(line):
    s = line.strip()
    if not s: return False, ''
    if s.startswith('//'):  return True, s[2:].strip()
    if s.startswith('/*') and s.endswith('*/'):
        return True, s[2:-2].strip()
    return False, ''

def attach_comment(code_line, cmt, width):
    if not cmt: return code_line
    return code_line.rstrip() + ' /* ' + cmt + ' */'

# ── main transform ───────────────────────────────────────────────────────────

def process_lines(raw_lines, width=DEFAULT_WIDTH, op_norm=True):
    lines = [l.rstrip('\r\n').replace('\t', ' ' * INDENT_W) for l in raw_lines]
    out         = []
    depth       = 0
    in_func     = False
    pending_cmt = None
    in_block_cmt = False   # True when inside a /* ... */ spanning multiple lines

    just_closed = False   # True after a function's closing brace, until next non-blank
    seen_func   = False   # True once at least one function has been processed

    for line in lines:
        line = line.rstrip()

        # Track multi-line block comments: pass them through untouched
        if in_block_cmt:
            out.append(line)
            if '*/' in line:
                in_block_cmt = False
            continue
        # Detect start of unclosed block comment on this line
        if '/*' in line:
            tok = tokenize(line)
            for k, t in tok:
                if k == 'bc' and not t.endswith('*/'):
                    in_block_cmt = True
                    break

        if is_rule_line(line):
            pending_cmt = None
            just_closed = False
            out.append(regen_rule(line, width))
            continue

        stripped = line.strip()

        if not in_func:
            opens, closes = count_braces(line)
            depth += opens - closes
            if stripped == '{' and depth == 1:
                in_func = True
                just_closed = False

            # Insert major rule before first non-blank content after a function closed
            if just_closed and stripped:
                out.append(make_rule('=', 0, width))
                just_closed = False

            out.append(line)
            continue

        # inside function body
        opens, closes = count_braces(line)
        depth += opens - closes

        if depth <= 0:
            depth = 0; in_func = False; just_closed = True; seen_func = True
            if pending_cmt:
                line = attach_comment(line, pending_cmt, width)
                pending_cmt = None
            out.append(line)
            continue

        if not stripped:                        # blank line — drop
            continue

        pure, cmt_text = is_pure_comment(line)
        if pure:                                # standalone comment — queue
            pending_cmt = (pending_cmt + '  ' + cmt_text) if pending_cmt else cmt_text
            continue

        if op_norm:
            line = normalize_operators(line)
        line = line.rstrip()

        if pending_cmt:
            line = attach_comment(line, pending_cmt, width)
            pending_cmt = None

        out.append(line)

    if pending_cmt:
        out.append('/* ' + pending_cmt + ' */')
    return out

# ── .o verification ──────────────────────────────────────────────────────────
# We compare the .text section only (not .debug_line which encodes line numbers,
# and not .shstrtab which contains the source filename).

def find_repo_root(start):
    d = os.path.abspath(start)
    while d != '/':
        if os.path.exists(os.path.join(d, 'Makefile')): return d
        d = os.path.dirname(d)
    return None

def compile_flags(src, repo_root):
    S = os.path.join(repo_root, 'src')
    RT = os.path.join(S, 'runtime')
    base = os.path.basename(src)
    # Note: NO -g — debug info encodes line numbers and would give false mismatches
    CBASE = ['-O0', '-w', '-I'+S, '-I'+os.path.join(RT,'x86'), '-I'+RT]
    CRT   = CBASE + ['-I'+os.path.join(RT,'x86'), '-DDYN_ENGINE_LINKED']
    if 'emit_wasm' in base:
        return ['-O0','-w','-I'+S,
                '-I'+os.path.join(S,'frontend','snobol4'),
                '-I'+os.path.join(S,'frontend','icon'),
                '-I'+os.path.join(S,'frontend','prolog'),
                '-I'+os.path.join(S,'frontend','snocone'),
                '-I'+os.path.join(S,'frontend','rebus'),
                '-I'+os.path.join(S,'backend')]
    if 'ir_print' in base:
        return CBASE + ['-I'+os.path.join(S,'frontend','snobol4'), '-DIR_DEFINE_NAMES']
    if base in ('snobol4.lex.c', 'snobol4.tab.c'):
        return CBASE
    return CRT

def extract_text_section(obj_bytes):
    """Return the raw bytes of the .text section from an ELF .o file."""
    # Write to temp, use objcopy to extract .text
    with tempfile.NamedTemporaryFile(suffix='.o', delete=False) as tf:
        tf.write(obj_bytes); obj_path = tf.name
    with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as tf:
        bin_path = tf.name
    try:
        r = subprocess.run(
            ['objcopy', '-O', 'binary', '--only-section=.text', obj_path, bin_path],
            capture_output=True)
        if r.returncode != 0 or not os.path.exists(bin_path):
            return obj_bytes  # fallback: use whole .o
        with open(bin_path, 'rb') as f:
            return f.read()
    finally:
        for p in (obj_path, bin_path):
            if os.path.exists(p): os.unlink(p)

def compile_obj(src, repo_root):
    flags = compile_flags(src, repo_root)
    with tempfile.NamedTemporaryFile(suffix='.o', delete=False) as tf:
        obj = tf.name
    try:
        r = subprocess.run(['gcc'] + flags + ['-c', src, '-o', obj],
                           capture_output=True)
        if r.returncode != 0:
            print('    [verify] compile error:\n' +
                  r.stderr.decode(errors='replace')[:400], file=sys.stderr)
            return None
        with open(obj, 'rb') as f:
            data = f.read()
        return extract_text_section(data)
    finally:
        if os.path.exists(obj): os.unlink(obj)

def ohash(data): return hashlib.sha256(data).hexdigest()[:16]

# ── file entry point ─────────────────────────────────────────────────────────

def process_file(path, width, op_norm, dry_run, verify):
    if should_skip(path):
        print(f'  skip (generated): {path}'); return True

    with open(path, 'r', errors='replace') as f:
        orig_lines = f.readlines()

    new_lines = process_lines(orig_lines, width=width, op_norm=op_norm)
    orig_text = ''.join(orig_lines)
    new_text  = '\n'.join(new_lines) + '\n'

    if orig_text == new_text:
        print(f'  unchanged: {path}'); return True

    if dry_run:
        orig_split = orig_text.splitlines()
        diffs = [(i+1, orig_split[i] if i < len(orig_split) else '',
                  new_lines[i] if i < len(new_lines) else '')
                 for i in range(max(len(orig_split), len(new_lines)))
                 if (orig_split[i] if i < len(orig_split) else '') !=
                    (new_lines[i] if i < len(new_lines) else '')]
        print(f'  would change: {path}  ({len(diffs)} lines differ)')
        for ln, a, b in diffs[:6]:
            print(f'    L{ln:4d}  was: {repr(a)[:70]}')
            print(f'           now: {repr(b)[:70]}')
        if len(diffs) > 6: print(f'    … {len(diffs)-6} more')
        return True

    before_obj = None; repo_root = None
    if verify and path.endswith('.c'):  # .h files produce PCH, not ELF — skip verify
        repo_root = find_repo_root(path)
        if repo_root:
            before_obj = compile_obj(path, repo_root)
            if before_obj is None:
                print(f'  [verify] skip (compile failed before): {path}'); return True

    with open(path, 'w') as f: f.write(new_text)
    print(f'  beautified: {path}')

    if verify and repo_root and before_obj:
        after_obj = compile_obj(path, repo_root)
        if after_obj is None:
            print(f'  [verify] FAIL — does not compile after: {path}', file=sys.stderr)
            return False
        h1, h2 = ohash(before_obj), ohash(after_obj)
        if h1 == h2:
            print(f'  [verify] OK  .text identical ({h1})')
        else:
            print(f'  [verify] MISMATCH  before={h1}  after={h2}', file=sys.stderr)
            return False
    return True

# ── CLI ──────────────────────────────────────────────────────────────────────

def collect_files(targets):
    files = []
    for t in targets:
        if os.path.isfile(t):
            if t.endswith(('.c', '.h')): files.append(t)
        elif os.path.isdir(t):
            for root, dirs, fnames in os.walk(t):
                dirs[:] = sorted(d for d in dirs if d not in ('archive', '.git'))
                for fn in sorted(fnames):
                    if fn.endswith(('.c', '.h')):
                        files.append(os.path.join(root, fn))
    return files

def main():
    ap = argparse.ArgumentParser(description='snobol4ever C/H beautifier')
    ap.add_argument('targets', nargs='+', metavar='PATH')
    ap.add_argument('--verify',     action='store_true',
                    help='compare .text section hashes before/after')
    ap.add_argument('--dry-run',    action='store_true',
                    help='show diffs, write nothing')
    ap.add_argument('--width',      type=int, default=DEFAULT_WIDTH)
    ap.add_argument('--no-op-norm', action='store_true',
                    help='skip operator spacing normalisation')
    args = ap.parse_args()

    files = collect_files(args.targets)
    all_ok = True
    for path in files:
        ok = process_file(path, width=args.width, op_norm=not args.no_op_norm,
                          dry_run=args.dry_run, verify=args.verify)
        if not ok: all_ok = False

    print(f'\n{len(files)} file(s) processed.')
    sys.exit(0 if all_ok else 1)

if __name__ == '__main__':
    main()
