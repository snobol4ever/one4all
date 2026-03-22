# Prolog Frontend Corpus

Ten-rung ladder for the Tiny-Prolog frontend (`-pl` flag).
Mirrors the SNOBOL4 10-rung crosscheck ladder.

Each rung is a directory. Programs in a rung test exactly the features
introduced at that rung — nothing from later rungs.

## Ladder

| Rung | Dir | Feature | Milestone |
|------|-----|---------|-----------|
| 1 | rung01_hello     | `write/1`, `nl/0`, `halt/0` | M-PROLOG-R1 |
| 2 | rung02_facts     | facts, deterministic lookup | M-PROLOG-R1 |
| 3 | rung03_unify     | head unification, compound terms | M-PROLOG-R3 |
| 4 | rung04_arith     | `is/2`, `+/-/*///mod`, comparisons | M-PROLOG-R4 |
| 5 | rung05_backtrack | `member/2`, `fail`, multiple solutions | M-PROLOG-R5 |
| 6 | rung06_lists     | `append/3`, `length/2`, `reverse/2` | M-PROLOG-R6 |
| 7 | rung07_cut       | `!`, `differ/N`, closed-world negation | M-PROLOG-R7 |
| 8 | rung08_recursion | `fibonacci/2`, `factorial/2` | M-PROLOG-R8 |
| 9 | rung09_builtins  | `functor/3`, `arg/3`, `=../2`, type tests | M-PROLOG-R9 |
| 10 | rung10_programs | Lon's word-puzzle constraint solvers | M-PROLOG-R10 |

## Rung 10 — Word Puzzle Programs

Logic puzzles in the Smullyan / Dell genre.
Generate-and-test with `differ/N` permutation guard and
closed-world negation via `!, fail`.

| File | Puzzle | Expected output |
|------|--------|-----------------|
| `puzzle_01.pro` | Bank positions: Brown/Jones/Smith | `Cashier=smith Manager=brown Teller=jones` |
| `puzzle_02.pro` | Trades: Clark/Daw/Fuller | `Carpenter=clark Painter=daw Plumber=fuller` |
| `puzzle_05.pro` | Bank chess: Brown/Clark/Jones/Smith | (constraints partially commented — WIP) |
| `puzzle_06.pro` | Occupations: Clark/Jones/Morgan/Smith | `Clark=druggist Jones=grocer Morgan=butcher Smith=policeman` |
| `puzzles.pro`   | Collection: puzzles 3–20 stubs | varies per puzzle |

## Prolog features used in Rung 10

All within the practical subset — no `assert/retract`, no `setof/bagof`,
no modules, no arithmetic beyond integer `is/2`.

- Horn clauses, facts
- Head unification
- `!, fail` closed-world negation
- `differ/2,3,4` with cut (canonical permutation guard)
- `write/1`, `nl/0`, `fail/0` (exhaustive search driver)
- `:- initialization(main)` directive
- Transitive closure predicates (`doesEarnMore`, `doesLiveNear`)
