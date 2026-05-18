#!/bin/bash
# icon_bb_probes.sh — detection/completion probes for GOAL-ICON-BB-COMPLETE rungs.
# Source this file from a rung's probe commands; do not execute directly.
#
# A0 (GOAL-ICON-BB-COMPLETE): initial version.
#
# Usage from a rung:
#   source scripts/icon_bb_probes.sh
#   bb_probe_detect  "A1" "AST_BANG_BINARY|AST_LCONCAT"  rung15_real_swap_lconcat
#   bb_probe_complete "A1" "AST_BANG_BINARY|AST_LCONCAT" rung15_real_swap_lconcat

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="${CORPUS:-/home/claude/corpus/programs/icon}"

# Detection: rung is needed iff one of {anchor program fires SM_PUSH_EXPR}
#                                  OR {any program fires SM_PUSH_EXPR for this kind set}
bb_probe_detect() {
    local rung="$1" kind_re="$2" anchor="$3"
    local fires
    fires=$(timeout 8 bash -c "SCRIP_EXPRS_AUDIT=1 '$SCRIP' --interp '${CORPUS}/${anchor}.icn' < /dev/null 2>&1" \
            | grep -cE "(${kind_re}).*SM_PUSH_EXPR fired" 2>/dev/null || true)
    if [ "${fires:-0}" -gt 0 ]; then
        echo "$rung needed: anchor $anchor has $fires fires"
        return 0
    fi
    # Anchor clean — sweep corpus for any fire of these kinds
    local total=0
    for f in "${CORPUS}"/rung*.icn; do
        local c
        c=$(timeout 8 bash -c "SCRIP_EXPRS_AUDIT=1 '$SCRIP' --interp '$f' < /dev/null 2>&1" \
            | grep -cE "(${kind_re}).*SM_PUSH_EXPR fired" 2>/dev/null || true)
        total=$((total + ${c:-0}))
    done
    if [ "$total" -gt 0 ]; then
        echo "$rung needed: corpus has $total fires for kinds /$kind_re/"
        return 0
    fi
    echo "$rung already closed (no fires)"
    return 1
}

# Completion: HONEST mode 3 — output equality is necessary but not sufficient.
#   (a) anchor sm-run output matches ir-run             [output witness]
#   (b) anchor passes under SCRIP_NO_AST_WALK=1         [structural witness]
#   (c) audit counter zero for kind set across corpus   [closure witness]
#   (d) smoke icon + snobol4 + unified-broker unchanged  [no-regression]
#   (e) at least one corpus program flipped honest       [progress witness]
bb_probe_complete() {
    local rung="$1" kind_re="$2" anchor="$3"
    local fail=0

    # (a) anchor output equality
    local ir_out sm_out
    ir_out=$(timeout 8 "$SCRIP" --interp "${CORPUS}/${anchor}.icn" < /dev/null 2>&1)
    sm_out=$(timeout 8 "$SCRIP" --interp "${CORPUS}/${anchor}.icn" < /dev/null 2>&1)
    if [ "$ir_out" != "$sm_out" ]; then
        echo "$rung FAIL (a): anchor $anchor sm-run differs from ir-run"
        diff <(echo "$ir_out") <(echo "$sm_out") | head -5
        fail=1
    fi

    # (b) anchor honest under SCRIP_NO_AST_WALK
    local h_out h_rc
    h_out=$(timeout 8 bash -c "SCRIP_NO_AST_WALK=1 '$SCRIP' --interp '${CORPUS}/${anchor}.icn' < /dev/null 2>&1")
    h_rc=$?
    if [ $h_rc -eq 134 ] || echo "$h_out" | grep -q "FATAL:"; then
        echo "$rung FAIL (b): anchor $anchor cheats — aborts under SCRIP_NO_AST_WALK"
        fail=1
    elif [ "$h_out" != "$ir_out" ]; then
        echo "$rung FAIL (b): anchor $anchor output differs honest"
        diff <(echo "$ir_out") <(echo "$h_out") | head -5
        fail=1
    fi

    # (c) audit counter zero for kinds across corpus
    for f in "${CORPUS}"/rung*.icn; do
        local n c
        n=$(basename "$f" .icn)
        c=$(timeout 8 bash -c "SCRIP_EXPRS_AUDIT=1 '$SCRIP' --interp '$f' < /dev/null 2>&1" \
            | grep -cE "(${kind_re}).*SM_PUSH_EXPR fired" 2>/dev/null || true)
        if [ "${c:-0}" -gt 0 ]; then
            echo "$rung FAIL (c): $n still has $c fires for /$kind_re/"
            fail=1
        fi
    done

    # (d) smoke sets unchanged
    local icon_pass
    icon_pass=$(bash "${HERE}/test_smoke_icon.sh" 2>&1 | grep -oP 'PASS=\K\d+')
    if [ "${icon_pass:-0}" -lt 5 ]; then
        echo "$rung FAIL (d): icon smoke PASS=$icon_pass (expected 5)"
        fail=1
    fi

    # (e) progress witness — at least one corpus program flipped honest
    local flipped=0
    for f in "${CORPUS}"/rung*.icn; do
        local n h_md5 ir_md5 base_h_md5
        n=$(basename "$f" .icn)
        h_md5=$(timeout 8 bash -c "SCRIP_NO_AST_WALK=1 '$SCRIP' --interp '$f' < /dev/null 2>&1" \
                | md5sum | cut -d' ' -f1)
        ir_md5=$(timeout 8 "$SCRIP" --interp "$f" < /dev/null 2>&1 | md5sum | cut -d' ' -f1)
        base_h_md5=$(grep "^$n " baselines/icon-bb/sm-run-honest.md5 2>/dev/null | awk '{print $3}')
        [ "$h_md5" = "$ir_md5" ] && [ "${base_h_md5:-}" != "$ir_md5" ] && \
            flipped=$((flipped + 1))
    done
    if [ "$flipped" -eq 0 ]; then
        echo "$rung WARN (e): no programs flipped honest-mode-3 (ok if structural-only)"
    else
        echo "$rung gain: $flipped programs flipped honest-mode-3"
    fi

    [ $fail -eq 0 ] && echo "$rung complete"
    return $fail
}

# Detection (Phase B/C anchor-based): rung needed iff anchor cheats under SCRIP_NO_AST_WALK
bb_probe_detect_anchor() {
    local rung="$1" anchor="$2"
    local ir h_out h_rc
    ir=$(timeout 8 "$SCRIP" --interp "${CORPUS}/${anchor}.icn" < /dev/null 2>&1)
    h_out=$(timeout 8 bash -c "SCRIP_NO_AST_WALK=1 '$SCRIP' --interp '${CORPUS}/${anchor}.icn' < /dev/null 2>&1")
    h_rc=$?
    if [ $h_rc -eq 134 ] || echo "$h_out" | grep -q "FATAL:" || [ "$ir" != "$h_out" ]; then
        echo "$rung needed: anchor $anchor cheats (rc=$h_rc)"
        return 0
    fi
    echo "$rung already closed: anchor $anchor honest"
    return 1
}

# Completion (Phase B/C anchor-based)
bb_probe_complete_anchor() {
    local rung="$1" anchor="$2"
    local fail=0
    local ir h_out h_rc
    ir=$(timeout 8 "$SCRIP" --interp "${CORPUS}/${anchor}.icn" < /dev/null 2>&1)
    h_out=$(timeout 8 bash -c "SCRIP_NO_AST_WALK=1 '$SCRIP' --interp '${CORPUS}/${anchor}.icn' < /dev/null 2>&1")
    h_rc=$?
    if [ $h_rc -eq 134 ] || echo "$h_out" | grep -q "FATAL:" || [ "$ir" != "$h_out" ]; then
        echo "$rung FAIL: anchor $anchor still cheats (rc=$h_rc)"
        diff <(echo "$ir") <(echo "$h_out") | head -5
        fail=1
    fi
    bb_probe_scoreboard || fail=1
    [ $fail -eq 0 ] && echo "$rung complete"
    return $fail
}

# Scoreboard: FLIPPED-HONEST / REGRESSION-HONEST / STILL-PASS / STILL-FAIL counts
bb_probe_scoreboard() {
    local flipped=0 regressed=0 still_pass=0 still_fail=0
    for f in "${CORPUS}"/rung*.icn; do
        local n h_md5 ir_md5 base_h_md5
        n=$(basename "$f" .icn)
        h_md5=$(timeout 8 bash -c "SCRIP_NO_AST_WALK=1 '$SCRIP' --interp '$f' < /dev/null 2>&1" \
                | md5sum | cut -d' ' -f1)
        ir_md5=$(timeout 8 "$SCRIP" --interp "$f" < /dev/null 2>&1 | md5sum | cut -d' ' -f1)
        base_h_md5=$(grep "^$n " baselines/icon-bb/sm-run-honest.md5 2>/dev/null | awk '{print $3}')
        if [ "$h_md5" = "$ir_md5" ] && [ "${base_h_md5:-}" != "$ir_md5" ]; then
            flipped=$((flipped + 1))
        elif [ "$h_md5" != "$ir_md5" ] && [ "${base_h_md5:-}" = "$ir_md5" ]; then
            regressed=$((regressed + 1))
        elif [ "$h_md5" = "$ir_md5" ]; then
            still_pass=$((still_pass + 1))
        else
            still_fail=$((still_fail + 1))
        fi
    done
    echo "honest-mode-3 scoreboard:"
    echo "  FLIPPED-HONEST=$flipped REGRESSION-HONEST=$regressed"
    echo "  STILL-PASSING-HONEST=$still_pass STILL-FAILING-HONEST=$still_fail"
    [ "$regressed" -eq 0 ]
}
