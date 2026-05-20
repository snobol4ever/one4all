#!/usr/bin/env python3
"""ec_uni_8_1_regroup.py — regroup 57 per-opcode files into 10 themed files.

Reads each per-opcode .c file, extracts the fn definition (skipping the
duplicate `#include` preamble), and writes one merged file per group.
"""
import pathlib
import re
import sys

GROUPS = {
    "sm_push_pop_lits.c": [
        "sm_push_lit_i", "sm_push_lit_s", "sm_push_lit_f",
        "sm_push_null", "sm_void_pop", "sm_push_var", "sm_store_var",
    ],
    "sm_arith.c": [
        "sm_concat", "sm_neg", "sm_coerce_num", "sm_exp",
        "sm_add", "sm_sub", "sm_mul", "sm_div", "sm_mod",
    ],
    "sm_compare.c": [
        "sm_stno", "sm_acomp", "sm_lcomp",
    ],
    "sm_jumps.c": [
        "sm_jump", "sm_jump_s", "sm_jump_f",
    ],
    "sm_halt.c": [
        "sm_halt",
    ],
    "sm_returns.c": [
        "sm_return", "sm_freturn", "sm_nreturn",
    ],
    "sm_pat_anchors.c": [
        "sm_pat_lit", "sm_pat_any", "sm_pat_any_i", "sm_pat_notany",
        "sm_pat_span", "sm_pat_break", "sm_pat_refname", "sm_pat_deref",
        "sm_pat_arb",
    ],
    "sm_pat_position.c": [
        "sm_pat_len", "sm_pat_pos", "sm_pat_rpos", "sm_pat_tab",
        "sm_pat_rtab", "sm_pat_rem", "sm_pat_bal", "sm_pat_eps",
    ],
    "sm_pat_control.c": [
        "sm_pat_fence0", "sm_pat_fence1", "sm_pat_abort",
        "sm_pat_fail", "sm_pat_succeed", "sm_pat_arbno",
    ],
    "sm_pat_combine.c": [
        "sm_pat_cat", "sm_pat_alt",
        "sm_pat_capture", "sm_pat_capture_fn", "sm_pat_capture_fn_args",
        "sm_pat_usercall", "sm_pat_usercall_args",
        "sm_exec_stmt",
    ],
}

BANNER = "/*" + "-" * 196 + "*/"

def extract_body(path):
    """Return the fn definition body (everything from the first fn signature
    line onward; strips leading #include lines + blank)."""
    text = path.read_text()
    lines = text.splitlines(keepends=True)
    # find first non-include, non-blank line that begins a fn definition
    fn_start = None
    for i, line in enumerate(lines):
        s = line.lstrip()
        if s.startswith("void ") or s.startswith("int ") or s.startswith("long "):
            if "(" in line:
                fn_start = i
                break
    if fn_start is None:
        raise RuntimeError(f"no fn found in {path}")
    return "".join(lines[fn_start:]).rstrip() + "\n"

def main():
    if len(sys.argv) != 3:
        print("usage: ec_uni_8_1_regroup.py <SM_templates_dir> <out_dir>", file=sys.stderr)
        sys.exit(1)
    src_dir = pathlib.Path(sys.argv[1])
    out_dir = pathlib.Path(sys.argv[2])
    out_dir.mkdir(exist_ok=True)

    # Sanity-check that GROUPS covers every existing file.
    existing = {p.stem for p in src_dir.glob("sm_*.c")}
    grouped = {name for fns in GROUPS.values() for name in fns}
    missing = existing - grouped
    extra = grouped - existing
    if missing:
        print(f"WARN: files exist in src_dir but NOT listed in any group: {sorted(missing)}", file=sys.stderr)
    if extra:
        print(f"WARN: groups list files that DON'T exist in src_dir: {sorted(extra)}", file=sys.stderr)

    header = (
        '#include "sm_template_common.h"\n'
        '#include "emit_sm.h"\n'
    )
    for group_file, fn_names in GROUPS.items():
        parts = [header]
        for fn in fn_names:
            p = src_dir / f"{fn}.c"
            if not p.exists():
                print(f"  WARN  missing: {p.name}", file=sys.stderr)
                continue
            parts.append(BANNER + "\n")
            parts.append(extract_body(p))
        out = out_dir / group_file
        out.write_text("".join(parts))
        print(f"  OK  {out.name}  ({len(fn_names)} fns)")

if __name__ == "__main__":
    main()
