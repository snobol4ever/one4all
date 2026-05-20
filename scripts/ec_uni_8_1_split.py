#!/usr/bin/env python3
"""ec_uni_8_1_split.py — split SM_templates family files into one-file-per-opcode.

Walks each family file's content; for every top-level function (whose definition
starts at column 0), copies it — plus any preceding comment-banner block — into
SM_templates/<fn_name>.c with the same shared #includes preamble as the family
file's first lines (down through the last #include / static helper block).

We treat the family file as:
  [PROLOGUE]               // shared #includes + file-level static helpers
  [FN1] [FN2] ... [FNN]    // one or more top-level fns

Each output file gets: PROLOGUE + [FN_k]. The static helpers in the prologue
remain accessible only via internal linkage; since each split file is now its
own translation unit, helpers must move from `static` to file-scope `static` is
fine when they live in the prologue copy.  If a helper is referenced from more
than one split file the unchanged `static` keyword would cause an undef linker
error in only one of them, but since each split file gets its OWN copy of the
prologue, each TU has its own copy of every helper — no link error, just code
duplication.  That's acceptable for the split rung; EC-UNI-8.x followups can
promote shared helpers to a shared `_priv.c`.

Comment banners (`/*--------*/` lines) that immediately precede a fn definition
are bundled with that fn.
"""
import re
import sys
import pathlib

# Match a top-level function-definition opener: a `<type> <name>(args) {` at
# column 0, possibly with `void`, `int`, `static` modifiers but we only want
# NON-static defs (those are the public template fns).
# We look for: returntype name(...) {   on a line starting at col 0,
# where the line doesn't begin with `static`.
FN_OPENER = re.compile(
    r"^(?P<ret>(?:int|void|long|const\s+\S+))"
    r"\s+(?P<name>[A-Za-z_][A-Za-z_0-9]*)\s*\(.*\)\s*\{(?P<after>.*)$"
)
STATIC_OPENER = re.compile(r"^\s*static\b")
BANNER = re.compile(r"^/\*-+\*/\s*$")
COMMENT_BLOCK_START = re.compile(r"^/\*")
COMMENT_BLOCK_END = re.compile(r"\*/\s*$")

def code_brace_delta(line: str) -> int:
    """Net brace change skipping string/char literals and // comments."""
    delta = 0
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if c == '/' and i + 1 < n and line[i+1] == '/':
            break
        if c == '/' and i + 1 < n and line[i+1] == '*':
            end = line.find('*/', i + 2)
            if end == -1:
                # multiline block comment — skip rest of line
                break
            i = end + 2
            continue
        if c == '"':
            i += 1
            while i < n:
                if line[i] == '\\' and i + 1 < n:
                    i += 2; continue
                if line[i] == '"':
                    i += 1; break
                i += 1
            continue
        if c == "'":
            i += 1
            while i < n:
                if line[i] == '\\' and i + 1 < n:
                    i += 2; continue
                if line[i] == "'":
                    i += 1; break
                i += 1
            continue
        if c == '{':
            delta += 1
        elif c == '}':
            delta -= 1
        i += 1
    return delta

def split_file(path: pathlib.Path, out_dir: pathlib.Path):
    """Returns list of (fn_name, output_path) tuples."""
    lines = path.read_text().splitlines(keepends=True)
    n = len(lines)

    # Phase 1: find the prologue end. Prologue runs from line 0 until the first
    # line that BEGINS a top-level non-static function (and isn't preceded by a
    # comment banner attached to it). We walk forward and detect the first fn.
    fn_starts = []
    in_block_comment = False
    for i, line in enumerate(lines):
        # crude block-comment tracking for this scan
        if in_block_comment:
            if "*/" in line:
                in_block_comment = False
            continue
        if line.lstrip().startswith("/*") and "*/" not in line:
            in_block_comment = True
            continue
        if STATIC_OPENER.match(line):
            continue
        m = FN_OPENER.match(line)
        if m:
            fn_starts.append((i, m.group("name")))

    if not fn_starts:
        print(f"  (no fns found in {path.name})", file=sys.stderr)
        return []

    # Determine for each fn its [preamble_start, body_end] range. The preamble
    # start = first line of any comment banner block immediately preceding the
    # opener line; body_end is found by brace-matching from the opener.
    fns = []
    for k, (open_idx, name) in enumerate(fn_starts):
        # Find banner preceding fn opener — scan backward over consecutive
        # comment lines and banner lines until we hit either a blank line, a
        # `}` (end of previous fn), or the prologue boundary.
        pre = open_idx
        # Look backward for an immediately preceding /* ... */ banner block.
        j = open_idx - 1
        while j >= 0:
            stripped = lines[j].rstrip("\n")
            # accept banner-comment line that closes a /* ... */ block
            if BANNER.match(lines[j]):
                pre = j
                j -= 1
                continue
            # multi-line comment block: walk up while inside it
            if stripped.endswith("*/"):
                # find the matching /*
                pre = j
                m_j = j - 1
                while m_j >= 0:
                    if lines[m_j].lstrip().startswith("/*"):
                        pre = m_j
                        break
                    m_j -= 1
                if m_j < 0:
                    break
                j = m_j - 1
                continue
            break

        # Find body end via brace matching starting at open_idx.
        depth = code_brace_delta(lines[open_idx])
        end = open_idx
        if depth > 0:
            end = open_idx + 1
            while end < n and depth > 0:
                depth += code_brace_delta(lines[end])
                end += 1
            # end now points just past the closing `}` line; subtract 1 for inclusive.
            end -= 1

        fns.append((name, pre, end))

    # Prologue: from line 0 up to (but not including) the first fn's preamble start.
    # But we should also skip any banner line that belongs to that fn (already part of `pre`).
    prologue_end = fns[0][1]  # exclusive
    prologue = "".join(lines[:prologue_end])
    # Strip trailing blank / banner lines from prologue so it ends cleanly.
    prologue = prologue.rstrip() + "\n"

    out_paths = []
    for (name, pre, end) in fns:
        body = "".join(lines[pre:end+1])
        out_path = out_dir / f"{name}.c"
        out_text = prologue + "\n" + body
        # Ensure trailing newline
        if not out_text.endswith("\n"):
            out_text += "\n"
        out_path.write_text(out_text)
        out_paths.append((name, out_path))
        print(f"  -> {out_path.name}  ({end - pre + 1} lines)")
    return out_paths

def main():
    if len(sys.argv) < 3:
        print("usage: ec_uni_8_1_split.py <out_dir> <family.c> [<family.c>...]", file=sys.stderr)
        sys.exit(1)
    out_dir = pathlib.Path(sys.argv[1])
    out_dir.mkdir(exist_ok=True)
    all_fns = []
    for fam in sys.argv[2:]:
        p = pathlib.Path(fam)
        print(f"Splitting {p.name}:")
        all_fns.extend([(name, op) for (name, op) in split_file(p, out_dir)])
    print(f"\nTotal: {len(all_fns)} fns -> {out_dir}/")

if __name__ == "__main__":
    main()
