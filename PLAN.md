# one4all/PLAN.md — ⛔ NOT HQ

## ⛔ STOP — THIS IS NOT THE SPRINT PLAN

This file is the one4all repo README stub. It is **not** the authoritative
sprint plan and **must not** be used to determine the current milestone.

**HQ is:** `/home/claude/.github/PLAN.md`

## Session start — always go to HQ first

```
tail -120 /home/claude/.github/SESSIONS_ARCHIVE.md   # ← FIRST
cat /home/claude/.github/PLAN.md                      # ← current milestone + routing
```

The NOW table in `.github/PLAN.md` is the single source of truth for:
- current milestone per session
- HEAD commits
- gate status

## L2 docs (read only when routed here from HQ)
- `doc/DESIGN.md` — architecture / emitter
- `doc/BOOTSTRAP.md` — env setup
- `doc/DECISIONS.md` — decisions log
- `.github/BEAUTY.md` — beauty subsystem plan
- `.github/SESSIONS_ARCHIVE.md` — session history
