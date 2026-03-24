# snobol4x — Sprint Plan

## §START — Bootstrap (ALWAYS FIRST)

```bash
cd /home/claude/snobol4ever
bash snobol4x/setup.sh          # must end: 106/106 ALL PASS
ln -sfn /home/claude/snobol4ever/x64 /home/claude/x64   # if x64 missing
```

---

## Current milestone: `M-BEAUTY-OMEGA` (B-276)

1. Check `demo/inc/omega.sno` exists ✅
2. `INC=demo/inc bash test/beauty/run_beauty_subsystem.sh omega`
3. Fix any ASM divergences
4. On PASS: `git commit -m "B-276: M-BEAUTY-OMEGA ✅"` + push
5. Update PLAN.md, advance to `M-BEAUTY-TRACE` (final subsystem before bootstrap)

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
| 18 | omega       | ← now |

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
