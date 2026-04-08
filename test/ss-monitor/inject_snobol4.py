#!/usr/bin/env python3
"""inject_snobol4.py — inject mon_enter/mon_exit into snobol4.c in-place.

Strategy: for each function definition (word at col 0 followed by '(' then '{'),
insert mon_enter("NAME") after the opening brace, and insert mon_exit("NAME", _r)
before each `return` statement within that function body.

This avoids redeclaration problems — functions keep their original signatures.
"""
import sys
import re

FUNC_START = re.compile(r'^([A-Z][A-Z0-9_]*)\s*\(ret_t\s+\w+\)\s*\{')
RETURN_STMT = re.compile(r'^(\s*)(RETURN\s*\()(\d+)\)')

def load_procs(proc_h_path):
    """Load the whitelist of top-level SIL procedure names from proc.h."""
    import re
    procs = set()
    try:
        with open(proc_h_path) as f:
            for line in f:
                m = re.match(r'^extern int ([A-Z][A-Z0-9_]*)\(', line)
                if m:
                    procs.add(m.group(1))
    except FileNotFoundError:
        pass
    return procs

def inject(src_path, dst_path, proc_h_path=None):
    with open(src_path) as f:
        lines = f.readlines()

    whitelist = load_procs(proc_h_path) if proc_h_path else None

    out = []
    out.append('/* injected by inject_snobol4.py */\n')
    out.append('#include "mon_hooks.h"\n')
    out.append('#include <stdlib.h>\n')
    out.append('static void __attribute__((constructor)) mon_init(void) {\n')
    out.append('    mon_open(getenv("MON_EVT"), getenv("MON_ACK"));\n')
    out.append('}\n\n')

    i = 0
    cur_fn = None
    depth = 0
    injected = 0

    while i < len(lines):
        line = lines[i]

        # Detect function start
        if cur_fn is None and depth == 0:
            m = FUNC_START.match(line)
            if m:
                cur_fn = m.group(1)
                # Whitelist check: only instrument top-level SIL procedures
                if whitelist and cur_fn not in whitelist:
                    cur_fn = None  # treat as non-function, copy verbatim
                    out.append(line)
                    i += 1
                    continue
                out.append(line)
                depth = line.count('{') - line.count('}')
                injected += 1
                i += 1
                # Scan forward past the ENTRY(name) line, then insert mon_enter
                while i < len(lines):
                    entry_line = lines[i]
                    out.append(entry_line)
                    depth += entry_line.count('{') - entry_line.count('}')
                    i += 1
                    if re.match(r'\s*ENTRY\s*\(', entry_line):
                        out.append(f'    mon_enter("{cur_fn}");\n')
                        break
                continue

        if cur_fn is not None:
            # Track brace depth
            depth += line.count('{') - line.count('}')

            # Inject mon_exit before RETURN() macro calls
            rm = RETURN_STMT.match(line)
            if rm:
                indent = rm.group(1)
                retval = rm.group(3)
                out.append(f'{indent}mon_exit("{cur_fn}", "{retval}");\n')

            out.append(line)

            if depth <= 0:
                cur_fn = None
                depth = 0
            i += 1
            continue

        out.append(line)
        i += 1

    with open(dst_path, 'w') as f:
        f.writelines(out)
    print(f"inject_snobol4.py: {injected} functions instrumented -> {dst_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3 or len(sys.argv) > 4:
        print(f"Usage: {sys.argv[0]} <snobol4.c> <snobol4-injected.c> [proc.h]")
        sys.exit(1)
    proc_h = sys.argv[3] if len(sys.argv) > 3 else None
    inject(sys.argv[1], sys.argv[2], proc_h)
