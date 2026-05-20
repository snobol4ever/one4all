#!/usr/bin/env python3
"""ec_uni_8_promote_v2.py — string-literal-aware promoter.

Same job as ec_uni_8_promote_multiline.py but counts braces only outside of
C string and character literals (and outside line comments). This avoids the
bug where `fprintf(out, "  }\n")` was mis-detected as a code `}`.
"""
import re
import sys
import pathlib

ARM_RE = re.compile(
    r"^(?P<indent> *)if \(IS_(?P<be>JVM|JS|NET|WASM)\)(?P<rest>.*)$"
)
X86_TEXT_RE = re.compile(r"^(?P<indent> *)if \(IS_X86_TEXT\)\b")

BIN_STUB = {
    "JVM":  "if (IS_JVM_BIN)  {{ /* EC-UNI-7 owed: binary .class bytes */ return; }}",
    "NET":  "if (IS_NET_BIN)  {{ /* EC-UNI-7 owed: binary .NET IL bytes */ return; }}",
    "WASM": "if (IS_WASM_BIN) {{ /* EC-UNI-7 owed: binary WASM bytes */ return; }}",
    "JS":   "/* IS_JS_BIN: n/a — JS has no binary form */",
}

def code_brace_delta(line: str) -> int:
    """Net brace change after skipping string/char literals and // comments.

    Tracks state across a single line only (multi-line strings are not used in C
    string literals; backslash-newline continuation is exceedingly rare in these
    files and would still be balanced across the joined line)."""
    delta = 0
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        # line comment kills the rest of the line
        if c == '/' and i + 1 < n and line[i+1] == '/':
            break
        # block comment /* ... */ on the same line
        if c == '/' and i + 1 < n and line[i+1] == '*':
            end = line.find('*/', i + 2)
            if end == -1:
                break
            i = end + 2
            continue
        # string literal
        if c == '"':
            i += 1
            while i < n:
                if line[i] == '\\' and i + 1 < n:
                    i += 2
                    continue
                if line[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        # char literal
        if c == "'":
            i += 1
            while i < n:
                if line[i] == '\\' and i + 1 < n:
                    i += 2
                    continue
                if line[i] == "'":
                    i += 1
                    break
                i += 1
            continue
        if c == '{':
            delta += 1
        elif c == '}':
            delta -= 1
        i += 1
    return delta

def transform(lines):
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        m = ARM_RE.match(line)
        if m:
            indent = m.group("indent")
            be = m.group("be")
            rest = m.group("rest")
            if rest.lstrip().startswith("_TEXT") or rest.lstrip().startswith("_BIN"):
                out.append(line)
                i += 1
                continue
            new_line = f"{indent}if (IS_{be}_TEXT){rest}"
            if not new_line.endswith("\n"):
                new_line += "\n"
            out.append(new_line)
            depth = code_brace_delta(rest)
            j = i
            if depth > 0:
                j = i + 1
                while j < n and depth > 0:
                    out.append(lines[j])
                    depth += code_brace_delta(lines[j])
                    j += 1
                nxt = lines[j] if j < n else ""
            else:
                j = i + 1
                nxt = lines[j] if j < n else ""
            already = False
            if be == "JS":
                if "IS_JS_BIN" in nxt:
                    already = True
            else:
                if f"IS_{be}_BIN" in nxt:
                    already = True
            if not already:
                stub = BIN_STUB[be].format()
                out.append(f"{indent}{stub}\n")
            i = j
            continue
        mx = X86_TEXT_RE.match(line)
        if mx:
            out.append(line)
            indent = mx.group("indent")
            nxt = lines[i+1] if i+1 < n else ""
            if "IS_X86_BIN" not in nxt:
                out.append(f"{indent}if (IS_X86_BIN)  {{ /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }}\n")
            i += 1
            continue
        out.append(line)
        i += 1
    return out

def main():
    for path in sys.argv[1:]:
        p = pathlib.Path(path)
        src = p.read_text().splitlines(keepends=True)
        dst = transform(src)
        p.write_text("".join(dst))
        print(f"OK  {path}  ({len(src)} -> {len(dst)} lines)")

if __name__ == "__main__":
    main()
