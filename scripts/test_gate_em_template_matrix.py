#!/usr/bin/env python3
"""test_gate_em_template_matrix.py — EC-UNI matrix gate (5-column).

Scans each .c file under {SM,BB}_templates/, extracts every top-level fn body
(string-literal-aware brace matching), and verifies each fn carries an arm
(`IS_<BE>`) or n/a sentinel (`<BE>: n/a`) for every cell of the 5-column
backend matrix.

Per AXIS CORRECTION (GOAL-HEADQUARTERS, 2026-05-19): text-vs-binary is a
serializer choice INSIDE each backend's output layer, NOT a matrix column.
The matrix is 5 wide: X86, JVM, JS, NET, WASM.

BB_templates fns are exempt from the X86 row (BB x86 goes through
emit_flat_body, not emit_bb_node).
"""
import pathlib
import re
import sys

BACKENDS = ["X86", "JVM", "JS", "NET", "WASM"]

def skip_string(s, i):
    quote = s[i]
    i += 1
    n = len(s)
    while i < n:
        if s[i] == "\\" and i + 1 < n:
            i += 2
            continue
        if s[i] == quote:
            return i + 1
        i += 1
    return i

def find_matching(s, open_idx):
    depth = 1
    i = open_idx + 1
    n = len(s)
    while i < n and depth > 0:
        c = s[i]
        if c in '"\'':
            i = skip_string(s, i)
            continue
        if c == "/" and i + 1 < n and s[i+1] == "/":
            nl = s.find("\n", i)
            i = n if nl == -1 else nl + 1
            continue
        if c == "/" and i + 1 < n and s[i+1] == "*":
            end = s.find("*/", i + 2)
            i = n if end == -1 else end + 2
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    return i

SIG_RE = re.compile(
    r"^(?:void|int|long)\s+([A-Za-z_][A-Za-z_0-9]*)\s*\([^)]*\)\s*\{",
    re.MULTILINE,
)

def extract_fns(text):
    """Yield (fn_name, body_text) for each top-level non-static fn in text."""
    for m in SIG_RE.finditer(text):
        name = m.group(1)
        open_brace = m.end() - 1
        close = find_matching(text, open_brace)
        body = text[open_brace + 1 : close - 1]
        yield name, body

# Static helpers in the same file are EC-UNI-16 Layer-2 extractions.  When a
# top-level fn delegates to a static helper (return helper(...) or { helper(...); }),
# the matrix gate credits the wrapper for whatever cells the helper covers.
STATIC_SIG_RE = re.compile(
    r"^static\s+(?:void|int|long)\s+([A-Za-z_][A-Za-z_0-9]*)\s*\([^)]*\)\s*\{",
    re.MULTILINE,
)

def extract_static_fns(text):
    """Yield (fn_name, body_text) for each static helper fn."""
    for m in STATIC_SIG_RE.finditer(text):
        name = m.group(1)
        open_brace = m.end() - 1
        close = find_matching(text, open_brace)
        body = text[open_brace + 1 : close - 1]
        yield name, body

def cell_present(body, be):
    """Return True if the body explicitly covers the <be> backend."""
    # Word-boundary IS_<BE> guard (must not match longer suffixes like IS_X86_TEXT).
    if re.search(rf"\bIS_{be}\b", body):
        return True
    # `<BE>: n/a ...` documented gap.
    if re.search(rf"\b{be}\s*:\s*n/a", body):
        return True
    return False

def effective_body(body, static_helpers):
    """If `body` delegates to a static helper in this file, return the union of
    its own text plus the helper's body.  Detection: any identifier from
    static_helpers appearing in `body` as a call site (word boundary + `(`).
    Multi-level transitive lookup is intentionally NOT done — EC-UNI-16 keeps
    helpers shallow."""
    extended = [body]
    for hname, hbody in static_helpers.items():
        if re.search(rf"\b{re.escape(hname)}\s*\(", body):
            extended.append(hbody)
    return "\n".join(extended)

def check_file(path, is_bb):
    misses = []
    fn_count = 0
    cell_checks = 0
    text = path.read_text()
    static_helpers = dict(extract_static_fns(text))
    for name, body in extract_fns(text):
        fn_count += 1
        effective = effective_body(body, static_helpers)
        for be in BACKENDS:
            # BB_templates fns: X86 row auto-skipped (BB x86 -> emit_flat_body)
            if is_bb and be == "X86":
                cell_checks += 1
                continue
            cell_checks += 1
            if not cell_present(effective, be):
                misses.append((path.name, name, f"IS_{be}"))
    return fn_count, cell_checks, misses

def main():
    if len(sys.argv) != 2:
        print("usage: test_gate_em_template_matrix.py <project_root>", file=sys.stderr)
        sys.exit(2)
    root = pathlib.Path(sys.argv[1])
    sm_dir = root / "src/emitter/SM_templates"
    bb_dir = root / "src/emitter/BB_templates"
    if not sm_dir.is_dir() or not bb_dir.is_dir():
        print(f"FAIL  template dirs not found under {root}", file=sys.stderr)
        sys.exit(2)

    total_files = 0
    total_fns = 0
    total_cells = 0
    all_misses = []

    for f in sorted(sm_dir.glob("sm_*.c")):
        total_files += 1
        fc, cc, ms = check_file(f, is_bb=False)
        total_fns += fc
        total_cells += cc
        all_misses.extend(ms)
    for f in sorted(bb_dir.glob("bb_*.c")):
        total_files += 1
        fc, cc, ms = check_file(f, is_bb=True)
        total_fns += fc
        total_cells += cc
        all_misses.extend(ms)

    for filename, fn_name, cell in all_misses:
        print(f"[MATRIX-MISS] {filename}::{fn_name}  {cell}")

    print()
    print(f"  Files checked:  {total_files}")
    print(f"  Fns checked:    {total_fns}")
    print(f"  Cells checked:  {total_cells}")
    print(f"  Misses:         {len(all_misses)}")

    if not all_misses:
        print("  PASS")
        sys.exit(0)
    print("  FAIL")
    sys.exit(1)

if __name__ == "__main__":
    main()
