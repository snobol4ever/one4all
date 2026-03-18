# Artifacts

One canonical file per artifact. Git history is the archive — no numbered session copies.
Overwrite and commit when the artifact changes. `git log -p artifacts/asm/beauty_prog.s` shows full evolution.

```
artifacts/
  asm/
    beauty_prog.s        ← PRIMARY: beauty.sno compiled via -asm
    fixtures/            ← sprint oracle .s files (one per node type / milestone)
    samples/             ← sample programs (roman, wordcount)
  c/                     ← C backend generated output (.c files)
  jvm/                   ← JVM bytecode (future)
  net/                   ← .NET MSIL (future)
```

---

## asm/beauty_prog.s — PRIMARY ARTIFACT

The canonical output of `sno2c -asm` on `beauty.sno`. This is what M-ASM-BEAUTIFUL was declared on.
Every session that changes the ASM backend must regenerate and commit this file.

**Update:**
```bash
INC=/home/socrates/snobol4corpus/programs/inc
BEAUTY=/home/socrates/snobol4corpus/programs/beauty/beauty.sno
src/sno2c/sno2c -asm -I$INC $BEAUTY > artifacts/asm/beauty_prog.s
nasm -f elf64 -I src/runtime/asm/ artifacts/asm/beauty_prog.s -o /dev/null  # must be clean
git add artifacts/asm/beauty_prog.s && git commit
```

---

## asm/fixtures/ — Sprint oracle fixtures

One file per node type or milestone. Hand-written or generated during sprint work.
These are reference/regression files — not updated every session.

| File | Node/feature | Milestone |
|------|-------------|-----------|
| null.s | empty program | M-ASM-HELLO ✅ |
| lit_hello.s | LIT node | M-ASM-LIT ✅ |
| pos0_rpos0.s, cat_pos_lit_rpos.s | POS/RPOS/SEQ | M-ASM-SEQ ✅ |
| alt_first.s, alt_second.s, alt_fail.s | ALT | M-ASM-ALT ✅ |
| arbno_match.s, arbno_empty.s, arbno_alt.s | ARBNO | M-ASM-ARBNO ✅ |
| any_vowel.s, notany_consonant.s, span_digits.s, break_space.s | charset | M-ASM-CHARSET ✅ |
| assign_lit.s, assign_digits.s | $ capture | M-ASM-ASSIGN ✅ |
| ref_astar_bstar.s, anbn.s | named patterns | M-ASM-NAMED ✅ |
| multi_capture_abc.s, star_deref_capture.s | multi-capture, *VAR | M-ASM-CROSSCHECK ✅ |
| stmt_output_lit.s, stmt_goto.s | program-mode statements | M-ASM-BEAUTY work |

---

## asm/samples/ — Sample programs

| File | What | Status |
|------|------|--------|
| roman.s | roman numeral converter | assembles; output placeholder |
| wordcount.s | word count program | assembles with warning |
