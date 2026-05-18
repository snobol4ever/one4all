# RS-23 — Diagnostic Findings (session 2026-05-03 cont.)

## Method

Built `scrip-rs23-diag` with `-Wl,--wrap=interp_eval` and a wrapper
(`src/driver/rs23_diag.c`) that, on every `interp_eval` call, walks the
backtrace looking for any ancestor frame whose symbol contains
`bb_eval_value`, `bb_exec_stmt`, `coro_call`, `coro_eval`, or
`coro_drive` — the BB-adapter call surface.  When found, logs the
EXPR kind, immediate-caller symbol, and BB-ancestor symbol once per
unique tuple to `/tmp/rs23_diag.log` (env: `RS23_DIAG_LOG`).

Ran across:
- `test_smoke_snobol4.sh` (7 tests, --interp / --run / --ir-run)
- `test_smoke_icon.sh` (5 tests, --ir-run)
- `test_smoke_prolog.sh` (5 tests)
- `test_smoke_raku.sh` (5 tests)
- `test_smoke_unified_broker.sh` (49 tests, mixed modes)
- `test_icon_all_rungs.sh` (263 programs, --ir-run)

Total: 570 raw events, 18 unique (kind, caller, via) tuples.

## Top kinds (by frequency)

| Count | Kind |
|------:|------|
|  250  | E_FNC |
|  172  | E_ASSIGN |
|   89  | E_EVERY |
|   22  | E_SCAN |
|   14  | E_AUGOP |
|   11  | E_INITIAL |
|    3  | E_CASE |
|    3  | E_ALTERNATE |
|    2  | E_SWAP |
|    1  | E_WHILE |
|    1  | E_NUL |
|    1  | E_NOT |
|    1  | E_ILIT |

## Coverage matrix — what's missing where

A `case X:` in `bb_eval_value` (coro_value.c) handles value-context
arrivals; a `case X:` in `bb_exec_stmt` (coro_stmt.c) handles
statement-context arrivals.  When a kind reaches an adapter in a
context it doesn't handle, control falls through to `interp_eval`.

| Kind         | value | stmt | Gap |
|--------------|:-----:|:----:|-----|
| E_FNC        |   ✓   |  ✗   | needs stmt handler (proc-call-as-stmt) |
| E_ASSIGN     |   ✓   |  ✗   | needs stmt handler |
| E_AUGOP      |   ✓   |  ✗   | needs stmt handler |
| E_SCAN       |   ✓   |  ✗   | needs stmt handler |
| E_CASE       |   ✓   |  ✗   | needs stmt handler |
| E_NOT        |   ✓   |  ✗   | needs stmt handler |
| E_ILIT       |   ✓   |  ✗   | trivial no-op stmt handler |
| E_NUL        |   ✓   |  ✗   | trivial |
| E_ALTERNATE  |   ✓   |  ✗   | needs stmt handler |
| E_WHILE      |   ✗   |  ✓   | needs value-ctx handler |
| E_EVERY      |   ✗   |  ✗   | needs both — RS-21 missed |
| E_INITIAL    |   ✗   |  ✗   | needs both |
| E_SWAP       |   ✗   |  ✗   | needs both |

Total gaps: 9 stmt-only, 1 value-only, 3 both-missing = 13 distinct
kind/context pairs.

## Why the previous RS-23 attempt regressed

The prior probe only fired at the *direct* fallthrough lines
(`coro_value.c:1075`, `coro_stmt.c:203`).  Hardening the fallthroughs
to `fprintf+FAILDESCR` broke gates because programs in the smoke and
unified_broker corpora hit the gaps above — kinds that are present in
one adapter but missing in the other.  The probe missed these because
they reached `interp_eval` through the unhardened sibling adapter's
fallthrough, which itself was never replaced (only the harded one was
on the affected stack).

The wrap-time diagnostic catches both fallthroughs *and* any indirect
paths because it instruments `interp_eval` itself — no matter who
called it — and only logs when a BB-adapter is on the call stack.

## Mode-1 observation

Smoke_icon runs `--ir-run` (mode 1).  In mode 1, Icon programs go
through `polyglot_execute → coro_call(main)`.  `coro_call` uses
`bb_exec_stmt` for proc bodies (RS-17b).  Therefore mode-1 Icon DOES
exercise the BB adapters, and the gaps above DO fire in mode 1.  The
RS-23 work — closing these gaps — is just as critical for mode-1 as
for mode-2/3.

## Plan for RS-23 sub-rungs

| Sub-rung | Scope | Approx LOC |
|----------|-------|-----------|
| RS-23a   | Add stmt handlers for E_FNC, E_ASSIGN, E_AUGOP in bb_exec_stmt — these are the high-volume cases (250 + 172 + 14 = 436 of 570 raw events) | ~80 |
| RS-23b   | Add stmt handlers for E_SCAN, E_CASE, E_NOT, E_ALTERNATE, E_ILIT, E_NUL | ~60 |
| RS-23c   | Add E_EVERY, E_INITIAL, E_SWAP to both adapters (they are missing in both) | ~80 |
| RS-23d   | Add value-ctx handler for E_WHILE in bb_eval_value | ~15 |
| RS-23e   | Re-run wrap diagnostic; expect 0 unique tuples; harden direct fallthroughs to abort + log; add coro_value.c, coro_stmt.c to isolation gate | ~10 + script update |

After RS-23e, the IR/SM isolation invariant is enforced for the BB
adapters, the externs go away, and RS-24 (strip dead Icon-frame switch
from interp_eval.c) becomes mechanical.

## Artifacts

- `src/driver/rs23_diag.c` — diagnostic wrap (do not commit; revert
  before RS-23e merge)
- `scripts/build_scrip_rs23_diag.sh` — diag build (keep; tooling)
- `scripts/test_rs23_diag_capture.sh` — capture runner (keep; tooling)
- `/tmp/rs23_diag_unique.log` — raw output of this session's run

## Gates (baseline, with diag NOT linked)

- smoke_snobol4 7/7
- smoke_icon 5/5
- smoke_prolog 5/5
- smoke_raku 5/5
- unified_broker 49/0
- isolation gate green
- icon_ir_all_rungs 191/42/30/263 (PASS/FAIL/XFAIL/TOTAL)
