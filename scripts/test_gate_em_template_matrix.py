#!/usr/bin/env python3
"""test_gate_em_template_matrix.py — EC-UNI-8.3 gate (driven by .sh wrapper).

Scans each .c file under {SM,BB}_templates/, extracts every top-level fn body
(string-literal-aware brace matching), and verifies each fn carries an arm
(`IS_<BE>_<MODE>`) or n/a sentinel (`<BE>_<MODE>: n/a`) for every cell of
the 5×2 backend×mode matrix.

BB_templates fns are exempt from the X86 row (BB x86 goes through
emit_flat_body).  JS_BIN is the standing n/a everywhere.
"""
import pathlib
import re
import sys

BACKENDS = ["X86", "JVM", "JS", "NET", "WASM"]
MODES = ["TEXT", "BIN"]

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

def cell_present(body, be, md):
    """Return True if the body explicitly covers the (be, md) cell."""
    if re.search(rf"IS_{be}_{md}\b", body):
        return True
    if re.search(rf"{be}_{md}: n/a", body):
        return True
    return False

def check_file(path, is_bb):
    misses = []
    fn_count = 0
    cell_checks = 0
    text = path.read_text()
    for name, body in extract_fns(text):
        fn_count += 1
        for be in BACKENDS:
            for md in MODES:
                # BB_templates fns: X86 row auto-skipped (BB x86 -> emit_flat_body)
                if is_bb and be == "X86":
                    cell_checks += 1
                    continue
                # JS_BIN: standing n/a everywhere
                if be == "JS" and md == "BIN":
                    cell_checks += 1
                    continue
                cell_checks += 1
                if not cell_present(body, be, md):
                    misses.append((path.name, name, f"IS_{be}_{md}"))
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
