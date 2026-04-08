#!/usr/bin/env python3
"""inject_silly.py — inject MON_ENTER/MON_EXIT into Silly SNOBOL4 *_fn() functions.

Strategy: find every "RESULT_t NAME_fn(void)" definition, insert MON_ENTER at top
and wrap each return with MON_EXIT. Handles both multi-line and one-liner functions.

One-liner pattern: RESULT_t FOO_fn(void)   { ... return BAR_fn(); }
Multi-line pattern: RESULT_t FOO_fn(void)\n{\n    ...\n    return ...\n}\n
"""
import sys, re

# Match function definition line (at column 0)
FN_DEF = re.compile(r'^(RESULT_t\s+([A-Z][A-Z0-9_]*)_fn\([^)]*\))')

def strip_name(fn_with_suffix):
    # "ARGVAL_fn" -> "ARGVAL"
    return fn_with_suffix.replace('_fn', '')

def inject(src, dst):
    with open(src) as f:
        lines = f.readlines()

    out = []
    out.append('#ifdef MON_ENABLED\n')
    out.append('#include "mon_hooks.h"\n')
    out.append('#endif\n')
    i = 0
    instrumented = 0

    while i < len(lines):
        line = lines[i]
        m = FN_DEF.match(line)
        if m:
            full_name = m.group(2) + '_fn'
            label = m.group(2)  # e.g. "ARGVAL"

            # Check for one-liner: definition and body on same line
            rest = line[m.end():]
            if '{' in rest and '}' in rest:
                # One-liner — wrap the return inside braces
                # e.g. "{ SCL.a.i = 1;  return ARITH_fn(); }"
                # Transform to multi-line with hooks
                body = rest.strip()
                # Extract inner content between first { and last }
                inner = body[body.index('{')+1 : body.rindex('}')].strip()
                # Replace bare return with MON_EXIT + return
                inner_hooked = re.sub(
                    r'\breturn\b',
                    f'MON_EXIT("{label}", "OK"); return',
                    inner
                )
                out.append(f'{m.group(1)} {{\n')
                out.append(f'    MON_ENTER("{label}");\n')
                out.append(f'    {inner_hooked}\n')
                out.append(f'}}\n')
                instrumented += 1
                i += 1
                continue

            # Multi-line function
            out.append(line)
            i += 1
            # Find opening brace (may be on same line or next)
            brace_opened = '{' in line
            if not brace_opened:
                while i < len(lines):
                    out.append(lines[i])
                    if '{' in lines[i]:
                        i += 1
                        break
                    i += 1
            # Insert MON_ENTER after opening brace
            out.append(f'    MON_ENTER("{label}");\n')
            # Now copy body, wrapping returns, until matching close brace
            depth = 1
            while i < len(lines) and depth > 0:
                ln = lines[i]
                depth += ln.count('{') - ln.count('}')
                if depth > 0:
                    # Wrap return statements
                    ln = re.sub(
                        r'\breturn\s+(OK|FAIL|[A-Z_a-z][A-Z_a-z0-9]*\(\))\s*;',
                        lambda rm: f'MON_EXIT("{label}", "{rm.group(1) if rm.group(1) in ("OK","FAIL") else "OK"}"); return {rm.group(1)};',
                        ln
                    )
                    out.append(ln)
                else:
                    # Closing brace line — insert MON_EXIT before if not already handled
                    # (catches fall-through returns)
                    out.append(f'    MON_EXIT("{label}", "OK"); /* fall-through */\n')
                    out.append(ln)
                i += 1
            instrumented += 1
            continue

        out.append(line)
        i += 1

    with open(dst, 'w') as f:
        f.writelines(out)
    print(f'inject_silly.py: {instrumented} functions instrumented -> {dst}')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} <src.c> <dst.c>')
        sys.exit(1)
    inject(sys.argv[1], sys.argv[2])
