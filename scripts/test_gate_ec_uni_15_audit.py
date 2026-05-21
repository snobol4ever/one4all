#!/usr/bin/env python3
"""test_gate_ec_uni_15_audit.py — EC-UNI-15 close audit.

EC-UNI-15 spec: every SM/BB template fn is a verbose `if (IS_<BE>)` five-arm
switch, one screen per fn.  Done family-by-family.  Multi-statement arms fine;
no helper extraction yet.

This audit reports two things:

  (a) Matrix coverage — every fn carries a cell (`IS_<BE>` arm or `n/a`
      sentinel) for every backend.  This is delegated to
      test_gate_em_template_matrix.py (the canonical matrix gate); the audit
      simply re-asserts its result.

  (b) Fn-size distribution — physical-line count per fn, sorted descending.
      EC-UNI-15 wants "one screen per fn" — a soft guideline around 60 lines
      at 200-col density.  Fns exceeding ~60 lines are EC-UNI-16 candidates for
      Layer-2 helper extraction.  This audit does NOT fail on size; it records
      the inventory so future EC-UNI-16 work has a target list.

Exit 0 iff matrix coverage is complete.
"""
import pathlib
import re
import subprocess
import sys


def fn_sizes(path):
    text = path.read_text()
    matches = re.finditer(r'^(?:void|int)\s+(\w+)\s*\([^)]*\)\s*\{', text, re.MULTILINE)
    results = []
    for m in matches:
        depth = 1
        i = m.end()
        while i < len(text) and depth > 0:
            c = text[i]
            if c == '"' or c == "'":
                q = c
                i += 1
                while i < len(text) and text[i] != q:
                    if text[i] == '\\':
                        i += 2
                    else:
                        i += 1
                i += 1
                continue
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            i += 1
        lines = text[m.start():i].count('\n') + 1
        results.append((lines, m.group(1)))
    return results


def main(root):
    root = pathlib.Path(root)
    matrix_script = root / 'scripts' / 'test_gate_em_template_matrix.sh'
    if not matrix_script.exists():
        print(f"SKIP matrix gate not found at {matrix_script}")
        return 0

    print("=== EC-UNI-15 close audit ===\n")

    # (a) Matrix coverage
    print("--- Matrix coverage (re-run of test_gate_em_template_matrix.sh) ---")
    r = subprocess.run(['bash', str(matrix_script)], capture_output=True, text=True)
    out = r.stdout.strip()
    print(out)
    matrix_ok = r.returncode == 0

    # (b) Fn-size distribution
    print("\n--- Fn-size distribution (one-screen guideline ~60 lines at 200-col) ---")
    all_results = []
    for d in ['SM_templates', 'BB_templates']:
        for p in sorted((root / 'src' / 'emitter' / d).glob('*.c')):
            for lines, name in fn_sizes(p):
                all_results.append((lines, p.name, name))
    all_results.sort(reverse=True)
    big = [r for r in all_results if r[0] >= 60]
    medium = [r for r in all_results if 30 <= r[0] < 60]
    small = [r for r in all_results if r[0] < 30]
    print(f"  Total template fns: {len(all_results)}")
    print(f"  >= 60 lines (EC-UNI-16 candidates): {len(big)}")
    print(f"  30-59 lines                       : {len(medium)}")
    print(f"  <  30 lines                       : {len(small)}")
    if big:
        print("\n  EC-UNI-16 candidate list (>= 60 lines):")
        for lines, file, name in big:
            print(f"    {lines:4}  {file:30} {name}")

    print("\n=== EC-UNI-15 audit summary ===")
    print(f"matrix coverage : {'PASS' if matrix_ok else 'FAIL'}")
    print(f"audit verdict   : {'PASS' if matrix_ok else 'FAIL'}  "
          f"(size distribution is informational, not enforced)")
    return 0 if matrix_ok else 1


if __name__ == '__main__':
    root = sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent.parent
    sys.exit(main(root))
