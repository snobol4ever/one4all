#!/usr/bin/env python3
"""ec_uni_8_promote_multiline.py — handle multi-line `if (IS_BE) { ... }` arms.

Walks each line; when it sees `if (IS_<BE>)` where BE in {JVM, JS, NET, WASM}:
  1. Renames IS_<BE> -> IS_<BE>_TEXT in place.
  2. Tracks brace depth from the opening `{` to the matching `}`.
  3. Immediately after the closing `}` line, inserts the matching _BIN/JS-n/a stub
     (with the same indent as the opening line).

Idempotent: skips if the next line already references IS_<BE>_BIN or IS_JS_BIN.
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
            # Already promoted to _TEXT in a prior pass? skip.
            if rest.lstrip().startswith("_TEXT") or rest.lstrip().startswith("_BIN"):
                out.append(line)
                i += 1
                continue
            new_line = f"{indent}if (IS_{be}_TEXT){rest}\n" if not line.endswith("\n") else f"{indent}if (IS_{be}_TEXT){rest}"
            if not new_line.endswith("\n"):
                new_line += "\n"
            out.append(new_line)
            # Determine if this is a single-line arm (closing } on same line)
            # by net brace count after the keyword.
            # Count braces in `rest`:
            opens = rest.count("{")
            closes = rest.count("}")
            depth = opens - closes
            j = i
            if depth > 0:
                # Multi-line — consume lines until depth returns to 0.
                j = i + 1
                while j < n and depth > 0:
                    out.append(lines[j])
                    depth += lines[j].count("{") - lines[j].count("}")
                    j += 1
                # j now points just past the closing line.
                # Insert stub at j unless already present.
                nxt = lines[j] if j < n else ""
            else:
                # Single-line — j stays at i+1; nxt is lines[i+1]
                j = i + 1
                nxt = lines[j] if j < n else ""
            # Check idempotency
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
        # Also inject IS_X86_BIN after any IS_X86_TEXT line (single-line form only).
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
