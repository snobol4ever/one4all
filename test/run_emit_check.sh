#!/usr/bin/env bash
# test/run_emit_check.sh — Emit-diff invariant check (M-G-INV-EMIT)
#
# Tests emitters, not the runtime.
#
# Invariant: sno2c emitter output is byte-for-byte identical to the
# committed snapshot baseline for every corpus .sno file × 3 backends.
#
# Approach:
#   1. Generate .asm/.j/.il for each corpus file (no assemble, no run)
#   2. Diff each against the committed snapshot in test/emit_baseline/
#   3. Report mismatches; exit 1 if any
#
# Usage:
#   bash test/run_emit_check.sh            # diff mode (default)
#   bash test/run_emit_check.sh --update   # regenerate baseline snapshots
#   bash test/run_emit_check.sh --verbose  # print PASS lines too
#
# Environment:
#   SNO2C    path to sno2c binary  (default: <root>/sno2c)
#   CORPUS   path to corpus root   (default: <root>/../corpus)
#   JOBS     parallel jobs         (default: nproc)
#
# Speed: ~4s wall time for 152 files × 3 backends in parallel.
# No JVM startup. No nasm. No linker. No program execution.
#
# Produced by: Claude Sonnet 4.6 (G-8 session, 2026-03-29)
# Milestone: M-G-INV-EMIT

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SNO2C="${SNO2C:-$ROOT/sno2c}"
CORPUS="${CORPUS:-$(cd "$ROOT/../corpus" 2>/dev/null && pwd || echo "")}"
BASELINE="$ROOT/test/emit_baseline"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

UPDATE=0; VERBOSE=0
for arg in "$@"; do
  [[ "$arg" == "--update"  ]] && UPDATE=1
  [[ "$arg" == "--verbose" ]] && VERBOSE=1
done

GREEN='\033[0;32m'; RED='\033[0;31m'; BOLD='\033[1m'; RESET='\033[0m'

# ── Validate environment ───────────────────────────────────────────────────────
if [[ ! -x "$SNO2C" ]]; then
  echo "ERROR: sno2c not found at $SNO2C — build first (cd src && make)" >&2; exit 1
fi
if [[ -z "$CORPUS" || ! -d "$CORPUS/crosscheck" ]]; then
  echo "ERROR: corpus not found. Set CORPUS= or clone snobol4ever/corpus alongside snobol4x" >&2; exit 1
fi

# ── Collect corpus files ───────────────────────────────────────────────────────
mapfile -t SNO_FILES < <(find "$CORPUS/crosscheck" -name "*.sno" | sort)
TOTAL=${#SNO_FILES[@]}
if [[ $TOTAL -eq 0 ]]; then
  echo "ERROR: no .sno files found in $CORPUS/crosscheck" >&2; exit 1
fi

# ── Update mode: regenerate baseline ──────────────────────────────────────────
if [[ $UPDATE -eq 1 ]]; then
  echo "Regenerating emit baseline for $TOTAL files × 3 backends..."
  mkdir -p "$BASELINE"

  emit_one_update() {
    local sno="$1" backend="$2" ext="$3"
    local name
    name="$(basename "$sno" .sno)"
    local subdir
    subdir="$(basename "$(dirname "$sno")")"
    local outdir="$BASELINE/$backend/$subdir"
    mkdir -p "$outdir"
    "$SNO2C" "$backend" "$sno" > "$outdir/$name.$ext" 2>/dev/null
  }
  export -f emit_one_update
  export SNO2C BASELINE

  printf '%s\n' "${SNO_FILES[@]}" | xargs -P"$JOBS" -I{} bash -c 'emit_one_update "$1" -asm s'   _ {}
  printf '%s\n' "${SNO_FILES[@]}" | xargs -P"$JOBS" -I{} bash -c 'emit_one_update "$1" -jvm j'   _ {}
  printf '%s\n' "${SNO_FILES[@]}" | xargs -P"$JOBS" -I{} bash -c 'emit_one_update "$1" -net il'  _ {}

  COUNT=$(find "$BASELINE" -type f | wc -l)
  echo "Baseline written: $COUNT files in $BASELINE"
  echo "Commit with: git add test/emit_baseline && git commit -m 'G-8: M-G-INV-EMIT baseline'"
  exit 0
fi

# ── Check mode: diff against baseline ─────────────────────────────────────────
if [[ ! -d "$BASELINE" ]]; then
  echo "ERROR: no baseline at $BASELINE — run with --update first" >&2; exit 1
fi

WORK=$(mktemp -d /tmp/emit_check_XXXXXX)
trap 'rm -rf "$WORK"' EXIT

PASS=0; FAIL=0
FAIL_LOG="$WORK/failures.txt"
touch "$FAIL_LOG"

START=$(date +%s%N 2>/dev/null || date +%s)

emit_one_check() {
  local sno="$1" backend="$2" ext="$3"
  local name subdir
  name="$(basename "$sno" .sno)"
  subdir="$(basename "$(dirname "$sno")")"
  local expected="$BASELINE/$backend/$subdir/$name.$ext"
  local got
  got=$("$SNO2C" "$backend" "$sno" 2>/dev/null)
  if [[ ! -f "$expected" ]]; then
    echo "MISSING_BASELINE $backend $subdir/$name" >> "$FAIL_LOG"
    return
  fi
  if diff -q <(echo "$got") "$expected" > /dev/null 2>&1; then
    echo "PASS $backend $subdir/$name" >> "$FAIL_LOG"
  else
    echo "FAIL $backend $subdir/$name" >> "$FAIL_LOG"
    diff <(echo "$got") "$expected" | head -20 >> "$FAIL_LOG"
    echo "---" >> "$FAIL_LOG"
  fi
}
export -f emit_one_check
export SNO2C BASELINE FAIL_LOG

printf '%s\n' "${SNO_FILES[@]}" | xargs -P"$JOBS" -I{} bash -c 'emit_one_check "$1" -asm s'  _ {}
printf '%s\n' "${SNO_FILES[@]}" | xargs -P"$JOBS" -I{} bash -c 'emit_one_check "$1" -jvm j'  _ {}
printf '%s\n' "${SNO_FILES[@]}" | xargs -P"$JOBS" -I{} bash -c 'emit_one_check "$1" -net il' _ {}

END=$(date +%s%N 2>/dev/null || date +%s)
WALL_MS=$(( (END - START) / 1000000 ))

# Tally results
while IFS= read -r line; do
  if [[ "$line" == PASS* ]]; then
    PASS=$((PASS + 1))
    [[ $VERBOSE -eq 1 ]] && echo -e "${GREEN}PASS${RESET} ${line#PASS }"
  elif [[ "$line" == FAIL* || "$line" == MISSING* ]]; then
    FAIL=$((FAIL + 1))
    echo -e "${RED}${line}${RESET}"
  fi
done < "$FAIL_LOG"

echo ""
echo -e "${BOLD}Emit-diff results: ${GREEN}$PASS pass${RESET} / ${RED}$FAIL fail${RESET} — ${WALL_MS}ms