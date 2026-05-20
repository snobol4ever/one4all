#!/usr/bin/env python3
"""ec_uni_8_promote.py — EC-UNI-8.2 mechanical promotion.

Transforms each SM_template / BB_template file so every function carries the full
backend × mode matrix:

  IS_X86_TEXT (untouched)
  IS_X86_BIN  -> insert stub immediately after the IS_X86_TEXT line if missing
  IS_JVM      -> rename to IS_JVM_TEXT; insert IS_JVM_BIN stub after
  IS_JS       -> rename to IS_JS_TEXT;  insert /* IS_JS_BIN: n/a */ after
  IS_NET      -> rename to IS_NET_TEXT; insert IS_NET_BIN stub after
  IS_WASM     -> rename to IS_WASM_TEXT; insert IS_WASM_BIN stub after

Byte-identity property: the renamed macros (IS_JVM_TEXT etc.) are #defined as
exactly the same predicates the bare ones evaluated to, because the corresponding
EMIT_* enum values denote text-mode by definition. The inserted _BIN stubs are
unreachable today (no caller sets EMIT_BIN_JVM / EMIT_BIN_NET / EMIT_BIN_WASM),
so they're dead code until EC-UNI-7 wires the binary backends.
"""
import re
import sys
import pathlib

# Match an existing arm like:
#     if (IS_JVM)  { ... }
# capturing the leading indent, the backend name, and the trailing newline.
ARM_RE = re.compile(
    r"^(?P<indent> *)if \(IS_(?P<be>JVM|JS|NET|WASM)\)(?P<gap> +)\{(?P<body>.*)\}\s*$"
)

# After IS_X86_TEXT line, we want to ensure an IS_X86_BIN stub follows.
X86_TEXT_RE = re.compile(r"^(?P<indent> *)if \(IS_X86_TEXT\) \{.*\} *$")

BIN_STUB = {
    "JVM":  "if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }",
    "NET":  "if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }",
    "WASM": "if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }",
    "JS":   "/* IS_JS_BIN: n/a — JS has no binary form */",
}

def promote_lines(lines):
    out = []
    saw_x86_bin = set()  # set of function indices already carrying x86 bin stub
    # First pass: track which functions already have IS_X86_BIN.
    # Simpler: walk linearly and stitch.
    i = 0
    while i < len(lines):
        line = lines[i]
        m = ARM_RE.match(line)
        if m:
            indent = m.group("indent")
            be = m.group("be")
            gap = m.group("gap")
            body = m.group("body")
            # Rename bare IS_BE -> IS_BE_TEXT.  Preserve original gap width so the
            # `{` column doesn't shift for byte-clean diffs of the text arm itself.
            # The macro grows by 5 characters ("_TEXT"); we absorb that by trimming
            # the gap.  If gap can't absorb, we keep one space.
            new_gap = gap[:-5] if len(gap) >= 6 else " "
            new_line = f"{indent}if (IS_{be}_TEXT){new_gap}{{{body}}}\n"
            out.append(new_line)
            # Insert the matching BIN/JS-n/a line immediately after, using the
            # same indent. Skip if next line already supplies it (idempotent).
            stub = BIN_STUB[be]
            nxt = lines[i+1] if i+1 < len(lines) else ""
            if f"IS_{be}_BIN" not in nxt and ("IS_JS_BIN" not in nxt or be != "JS"):
                out.append(f"{indent}{stub}\n")
            i += 1
            continue
        # Inject IS_X86_BIN stub right after a bare IS_X86_TEXT line if missing.
        mx = X86_TEXT_RE.match(line)
        if mx:
            out.append(line)
            indent = mx.group("indent")
            nxt = lines[i+1] if i+1 < len(lines) else ""
            if "IS_X86_BIN" not in nxt:
                out.append(f"{indent}if (IS_X86_BIN)  {{ /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }}\n")
            i += 1
            continue
        out.append(line)
        i += 1
    return out

def main():
    if len(sys.argv) < 2:
        print("usage: ec_uni_8_promote.py <file> [<file>...]", file=sys.stderr)
        sys.exit(1)
    for path in sys.argv[1:]:
        p = pathlib.Path(path)
        src = p.read_text().splitlines(keepends=True)
        dst = promote_lines(src)
        p.write_text("".join(dst))
        print(f"OK  {path}  ({len(src)} -> {len(dst)} lines)")

if __name__ == "__main__":
    main()
