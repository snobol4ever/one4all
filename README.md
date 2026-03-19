# snobol4x — TINY compiler
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

A SNOBOL4/SPITBOL compiler targeting x86-64 native ASM, JVM bytecode, and .NET MSIL.
Part of the [snobol4ever](https://github.com/snobol4ever) organization.

---

## What This Is

`snobol4x` is the **TINY** compiler — a from-scratch SNOBOL4 front-end (`sno2c`) with
three independent back-ends sharing one IR. It compiles `.sno` source to:

| Flag | Output | Status |
|------|--------|--------|
| *(default)* | C with gotos (trampoline) | ✅ 106/106 corpus |
| `-asm` | x86-64 NASM assembly | ✅ 106/106 corpus, sample programs |
| `-jvm` | JVM Jasmin bytecode (`.j`) | ✅ patterns/ rung, working toward 106/106 |
| `-net` | .NET CIL (`.il`) | 🔧 hello/literals working |

Sister repos: [`snobol4jvm`](https://github.com/snobol4ever/snobol4jvm) (full Clojure→JVM pipeline) and [`snobol4dotnet`](https://github.com/snobol4ever/snobol4dotnet) (full C#→MSIL pipeline).

---

## The Byrd Box Model

Every SNOBOL4 statement has the form:

```
label:   subject   pattern   = replacement   :S(x) F(y)
```

Each pattern node compiles to a **Byrd box** — four labeled entry points:

| Port | Meaning |
|------|---------|
| **α** | Enter fresh — cursor at current position |
| **β** | Resume after backtrack from a child |
| **γ** | Succeed — advance cursor, pass control forward |
| **ω** | Fail — restore cursor, propagate failure back |

Sequential composition wires γ of one node to α of the next.
Alternation saves the cursor on the left-ω path and restores it before trying right.
ARBNO wires child-γ back into α until child-ω exits.

All three back-ends implement the same four-port wiring — the execution semantics
are identical whether the target is C gotos, JVM bytecode, or MSIL.

---

## Build

```bash
# Dependencies
apt-get install -y libgc-dev nasm default-jdk

# Build sno2c
make -C src

# Run a program
./sno2c -asm program.sno | nasm -f elf64 - -o prog.o && gcc prog.o -o prog && ./prog
./sno2c -jvm program.sno > prog.j && java -jar src/backend/jvm/jasmin.jar prog.j -d . && java Prog
```

---

## Validate

```bash
# C backend — 106/106 corpus
bash test/crosscheck/run_crosscheck.sh

# ASM backend — 106/106 corpus
STOP_ON_FAIL=0 bash test/crosscheck/run_crosscheck_asm.sh

# JVM backend — patterns rung (19/20)
JASMIN=src/backend/jvm/jasmin.jar
PDIR=../snobol4corpus/crosscheck/patterns
for sno in $PDIR/*.sno; do
  base=$(basename $sno .sno); TMPD=$(mktemp -d)
  ./sno2c -jvm "$sno" > $TMPD/p.j 2>/dev/null
  java -jar $JASMIN $TMPD/p.j -d $TMPD/ 2>/dev/null
  cls=$(ls $TMPD/*.class 2>/dev/null | head -1 | xargs basename 2>/dev/null | sed 's/.class//')
  got=$(java -cp $TMPD $cls 2>/dev/null); exp=$(cat "${sno%.sno}.ref" 2>/dev/null)
  rm -rf $TMPD
  [ "$got" = "$exp" ] && echo "PASS $base" || echo "FAIL $base"
done
```

---

## Corpus Ladder

All back-ends climb the same 11-rung ladder against `snobol4corpus/crosscheck/`:

```
Rung 1:  hello/output    Rung 5:  control       Rung  9: keywords
Rung 2:  assign          Rung 6:  patterns      Rung 10: functions
Rung 3:  concat          Rung 7:  capture       Rung 11: data
Rung 4:  arith           Rung 8:  strings       Rung 12: beauty.sno
```

| Backend | Rungs | Notes |
|---------|-------|-------|
| C (trampoline) | 1–11 ✅ | 106/106 |
| x86-64 ASM | 1–11 ✅ | 106/106; roman/wordcount in progress |
| JVM bytecode | 1–6 ✅ | patterns/ 19/20; capture rung next |
| .NET MSIL | 1–2 🔧 | hello + literals passing |

---

## Repository Layout

```
src/
  frontend/
    snobol4/          SNOBOL4 lexer + parser → AST + IR (EXPR_t / STMT_t)
    snocone/          Snocone frontend (SC language)
  backend/
    c/                C-with-gotos emitter (emit_byrd.c, emit_cnode.c)
    x64/              x86-64 NASM emitter  (emit_byrd_asm.c)
    jvm/              JVM Jasmin emitter   (emit_byrd_jvm.c) + jasmin.jar
    net/              .NET CIL emitter     (net_emit.c)
  driver/
    main.c            sno2c entry point — flag dispatch
  runtime/
    asm/              NASM macro library (snobol4_asm.mac) + runtime helpers
test/
  crosscheck/         106 SNOBOL4 corpus programs + .ref oracle outputs
  jvm_j3/             JVM sprint J3 smoke tests
artifacts/
  asm/                Canonical ASM outputs (beauty_prog.s, roman.s, wordcount.s)
  jvm/                Canonical JVM outputs (hello_prog.j)
  net/                Canonical NET outputs (hello_prog.il)
  c/                  Canonical C outputs
```

---

## Active Development

Session tracking and sprint state live in [snobol4ever/.github](https://github.com/snobol4ever/.github):

- **PLAN.md** — milestone dashboard, 4D feature matrix, active sprint per session
- **JVM.md** — JVM back-end sprint state (current: J5 capture rung)
- **TINY.md** — ASM back-end sprint state
- **SESSIONS_ARCHIVE.md** — full session history

**Oracle:** CSNOBOL4 2.3.3 (`snobol4 -f -P256k -I$INC file.sno`)

---

## Collaborators

- **Lon Jones Cherryholmes** — compiler architecture, ASM/JVM/NET back-ends
- **Jeffrey Cooper, M.D.** — snobol4dotnet, MSIL target
- **Claude Sonnet 4.6** — TINY co-author (JVM back-end sessions)
