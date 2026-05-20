#!/usr/bin/env python3
"""ec_uni_9a_collapse.py — collapse IS_<BE>_TEXT / IS_<BE>_BIN arms into IS_<BE>.

The text-vs-binary axis was a false matrix dimension (see GOAL-HEADQUARTERS
AXIS CORRECTION block). For every template fn in SM_templates/ and
BB_templates/ this script:

  1. Rewrites every `IS_<BE>_TEXT` guard to `IS_<BE>`  (BE ∈ {X86, JVM, JS, NET, WASM}).
  2. Deletes every `IS_<BE>_BIN` arm (whether real-code stub like
     `if (IS_X86_BIN) { /* EC-UNI-6 owed: ... */ return 0; }` or a
     `/* IS_<BE>_BIN: n/a — ... */` comment line).
  3. Leaves bare `IS_BIN`, `IS_X86`, `IS_TEXT`, `IS_JVM`, `IS_JS`, `IS_NET`,
     `IS_WASM` guards alone — those are pre-existing macros used as early
     returns (e.g. bb_lit.c line 5: `if (IS_BIN) return;`).

Brace-matched, string-literal-aware. Run from project root:

    python3 scripts/ec_uni_9a_collapse.py

Reports each transformation per file. Idempotent: re-running on already-
collapsed files makes no changes.
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


def find_matching_brace(s, open_idx):
    """Given s[open_idx] == '{', return index just past matching '}'."""
    depth = 1
    i = open_idx + 1
    n = len(s)
    while i < n and depth > 0:
        c = s[i]
        if c in '"\'':
            i = skip_string(s, i)
            continue
        if c == "/" and i + 1 < n and s[i + 1] == "/":
            nl = s.find("\n", i)
            i = n if nl == -1 else nl + 1
            continue
        if c == "/" and i + 1 < n and s[i + 1] == "*":
            end = s.find("*/", i + 2)
            i = n if end == -1 else end + 2
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    return i


def delete_bin_arms(text):
    """Delete every `if (IS_<BE>_BIN) {...}` arm and every
    `/* IS_<BE>_BIN: n/a ... */` line.  Preserves the rest verbatim."""
    out = []
    i = 0
    n = len(text)
    changes = 0
    while i < n:
        # Look for `if (IS_<BE>_BIN)` — possibly preceded by whitespace.
        m = re.match(
            r"([ \t]*)if\s*\(\s*IS_(X86|JVM|JS|NET|WASM)_BIN\s*\)\s*",
            text[i:],
        )
        if m:
            # Find the body. After the `)` we expect either `{` or a single-statement.
            arm_start = i
            indent = m.group(1)
            after = i + m.end()
            if after < n and text[after] == "{":
                close = find_matching_brace(text, after)
                arm_end = close
            else:
                # single-statement form: consume to end of line (look for ; then newline)
                semi = text.find(";", after)
                if semi == -1:
                    out.append(text[i])
                    i += 1
                    continue
                arm_end = semi + 1
            # Consume trailing newline (and any trailing whitespace) so we don't
            # leave a blank gap where the arm was.
            if arm_end < n and text[arm_end] == "\n":
                arm_end += 1
            changes += 1
            # If the next non-blank line is another arm or a comment, keep
            # indentation consistent — we simply skip the arm entirely.
            i = arm_end
            # Also remove the leading indent we matched (it's part of arm_start..arm_end now via m.start()).
            # Trim any trailing whitespace on the previously-emitted line if we just removed an arm
            # that occupied its own line(s). Actually simpler: out already has the prior content; we just continue.
            continue
        # Look for `/* IS_<BE>_BIN: n/a ... */` comment as own line.
        m2 = re.match(
            r"[ \t]*/\*\s*IS_(X86|JVM|JS|NET|WASM)_BIN:\s*n/a[^*]*\*/[ \t]*\n",
            text[i:],
        )
        if m2:
            changes += 1
            i += m2.end()
            continue
        out.append(text[i])
        i += 1
    return "".join(out), changes


def collapse_text_to_be(text):
    """Replace IS_<BE>_TEXT identifiers with IS_<BE>."""
    pattern = re.compile(r"\bIS_(X86|JVM|JS|NET|WASM)_TEXT\b")
    count = [0]

    def repl(m):
        count[0] += 1
        return f"IS_{m.group(1)}"

    new = pattern.sub(repl, text)
    return new, count[0]


def process_file(path):
    orig = path.read_text()
    text = orig
    text, n_bin = delete_bin_arms(text)
    text, n_text = collapse_text_to_be(text)
    if text != orig:
        path.write_text(text)
        return n_bin, n_text
    return 0, 0


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    sm_dir = root / "src/emitter/SM_templates"
    bb_dir = root / "src/emitter/BB_templates"

    total_bin = 0
    total_text = 0
    files_changed = 0

    for d in (sm_dir, bb_dir):
        for f in sorted(d.glob("*.c")):
            n_bin, n_text = process_file(f)
            if n_bin or n_text:
                files_changed += 1
                print(f"  {f.relative_to(root)}: -{n_bin} BIN arms, {n_text} TEXT→BE")
                total_bin += n_bin
                total_text += n_text

    print()
    print(f"Files changed: {files_changed}")
    print(f"BIN arms removed: {total_bin}")
    print(f"TEXT→BE rewrites: {total_text}")


if __name__ == "__main__":
    main()
