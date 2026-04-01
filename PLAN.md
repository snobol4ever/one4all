# one4all — Sprint Plan

## §START — Bootstrap (ALWAYS FIRST)

```bash
cd /home/claude/snobol4ever
bash one4all/setup.sh          # must end: 106/106 ALL PASS
ln -sfn /home/claude/snobol4ever/x64 /home/claude/x64   # if x64 missing
```

---

## Current milestone: `M-BEAUTIFY-BOOTSTRAP` (B-278)

All 19 beauty subsystems ✅. Now run `beauty.sno` on itself end-to-end through
all 3 backends and verify fixed-point output.

1. Locate `demo/beauty.sno` (the full beautifier)
2. Run CSNOBOL4 oracle: `INC=demo/inc snobol4 -f -P256k -Idemo/inc demo/beauty.sno < demo/beauty.sno > /tmp/beauty_oracle.sno`
3. Verify fixed point: oracle output == input
4. Run ASM and SPITBOL backends through the same pipeline
5. Diff all three against oracle
6. Fix any ASM divergences in `emit_byrd_asm.c`
7. On PASS: `git commit -m "B-278: M-BEAUTIFY-BOOTSTRAP ✅"` + push

**Fixed B-277:** M-BEAUTY-TRACE — T8Trace/T8Pos helpers pass 3-way monitor;
9 tests PASS (CSNOBOL4 + SPITBOL + ASM). Key: GE(t8MaxLine,621) guard, DATATYPE
case portability, TABLE var exclusion. Commit `22e291c`.

**Fixed B-276:** M-BEAUTY-OMEGA — binary `E_ATP` (`pat @txOfs`) in value context
emitted OPSYN dispatch instead of LHS+cursor-capture. Fixed `expr_has_pattern_fn`
and value-context `E_ATP` handler in `emit_byrd_asm.c`. 15 tests PASS. Commit `151a99b`.

**Fixed B-275:** M-BEAUTY-SEMANTIC — `nPush/nInc/nPop/nTop` pattern helpers pass
3-way monitor; all 8 tests PASS (CSNOBOL4 + SPITBOL + ASM). Commit `fe86477`.
---

## Beauty subsystem sequence

| #  | Subsystem   | Status |
|----|-------------|--------|
| 1  | global      | ✅ |
| 2  | is          | ✅ |
| 3  | FENCE       | ✅ |
| 4  | io          | ✅ |
| 5  | case        | ✅ |
| 6  | assign      | ✅ |
| 7  | match       | ✅ |
| 8  | counter     | ✅ |
| 9  | stack       | ✅ |
| 10 | tree        | ✅ |
| 11 | ShiftReduce | ✅ |
| 12 | TDump       | ✅ |
| 13 | Gen         | ✅ |
| 14 | Qize        | ✅ |
| 15 | ReadWrite   | ✅ |
| 16 | XDump       | ✅ |
| 17 | semantic    | ✅ |
| 18 | omega       | ✅ |
| 19 | trace       | ✅ |

**All 19 subsystems PASS → M-BEAUTIFY-BOOTSTRAP sprint begins.**

---

## Trigger phrases
- `"playing with beauty"` → B-session → beauty milestone above
- `"playing with Prolog frontend"` → F-session → M-PROLOG-R10 (puzzle_02 doesEarnMore cut bug; puzzle_01 ✅ puzzle_06 ✅)

## L2 docs
- `doc/DESIGN.md` — architecture / emitter
- `doc/BOOTSTRAP.md` — env setup
- `doc/DECISIONS.md` — decisions log
- `.github/BEAUTY.md` — beauty subsystem plan
- `.github/SESSIONS_ARCHIVE.md` — session history
- `.github/PLAN.md` — HQ dashboard
