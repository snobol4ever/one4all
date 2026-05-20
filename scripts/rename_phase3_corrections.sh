#!/usr/bin/env bash
# rename_phase3_corrections.sh — builder/consumer correction pass.
# Rule: builders of IR data = UPPERCASE.  Consumers of IR data = lowercase.
# Phase 2 incorrectly UPPERCASED SM_templates dispatchers and dumpers; this fixes them.
# Also renames runtime broker modes BB_SCAN/PUMP/ONCE to lowercase (consumer constants).
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$HERE/.."
CORPUS="${CORPUS:-/home/claude/corpus}"
roots=("$ONE4ALL/src" "$ONE4ALL/include")
[ -d "$CORPUS" ] && roots+=("$CORPUS")
files=$(find "${roots[@]}" -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc' \) \
        ! -name '*.tab.c' ! -name '*.tab.h' ! -name '*.lex.c' ! -name 'snobol4.c' 2>/dev/null)
# Single-pass python to handle string-literal awareness for broker modes.
# For the SM_templates and dumpers, plain sed is fine (no string-literal collision).
echo "$files" | while read -r f; do
    [ -z "$f" ] && continue
    python3 -c "
import re, sys
p = '$f'
try:
    src = open(p, 'r', encoding='utf-8', errors='replace').read()
except Exception as e:
    sys.exit(0)
orig = src
# === Group 1: SM_templates dispatch functions back to lowercase (consumers) ===
tmpl = ['SM_halt','SM_jump','SM_jump_s','SM_jump_f','SM_return','SM_freturn','SM_nreturn',
        'SM_push_lit_i','SM_push_lit_s','SM_push_lit_f','SM_push_null','SM_push_var',
        'SM_store_var','SM_void_pop','SM_concat','SM_neg','SM_coerce_num','SM_exp',
        'SM_add','SM_sub','SM_mul','SM_div','SM_mod','SM_stno','SM_acomp','SM_lcomp',
        'SM_exec_stmt',
        'SM_pat_abort','SM_pat_alt','SM_pat_any','SM_pat_any_i','SM_pat_arb','SM_pat_arbno',
        'SM_pat_bal','SM_pat_break','SM_pat_capture','SM_pat_capture_fn',
        'SM_pat_capture_fn_args','SM_pat_cat','SM_pat_deref','SM_pat_eps','SM_pat_fail',
        'SM_pat_len','SM_pat_lit','SM_pat_notany','SM_pat_pos','SM_pat_refname','SM_pat_rem',
        'SM_pat_rpos','SM_pat_rtab','SM_pat_span','SM_pat_succeed','SM_pat_tab',
        'SM_pat_usercall','SM_pat_usercall_args']
for name in tmpl:
    lower = name.lower()
    src = re.sub(r'\b' + name + r'\b', lower, src)
# === Group 2: Dumpers (consumers) ===
src = re.sub(r'\bBB_print\b', 'bb_print', src)
src = re.sub(r'\bSM_seq_print\b', 'sm_seq_print', src)
# === Group 3: Broker mode constants to lowercase (runtime consumers) ===
# These appear both as identifiers AND as string literals.  Mask quoted strings,
# rename identifiers, then restore the quoted strings unchanged (the dump strings
# stay UPPERCASE because they're human-readable labels — not the identifier).
def mask_strings(line):
    stash = []
    def grab(m):
        stash.append(m.group(0))
        return f'\x00QSTR{len(stash)-1}\x01'
    return re.sub(r'\"(?:[^\"\\\\]|\\\\.)*\"', grab, line), stash
def unmask(line, stash):
    for i, q in enumerate(stash):
        line = line.replace(f'\x00QSTR{i}\x01', q)
    return line
out_lines = []
for line in src.split('\n'):
    masked, stash = mask_strings(line)
    # Broker modes BB_PUMP/BB_ONCE only ever appear as broker modes — safe to lowercase.
    # BB_SCAN has TWO meanings: broker mode (in bb_box.h, bb_broker.c, stmt_exec.c, pl_runtime.c, interp_hooks.c)
    # and IR tag (in BB.h enum, emit_core.c).  Only lowercase in broker-mode files.
    is_ir_tag_file = ('include/BB.h' in p or 'emitter/emit_core.c' in p)
    if not is_ir_tag_file:
        masked = re.sub(r'\bBB_SCAN\b', 'bb_scan', masked)
    masked = re.sub(r'\bBB_PUMP\b', 'bb_pump', masked)
    masked = re.sub(r'\bBB_ONCE\b', 'bb_once', masked)
    out_lines.append(unmask(masked, stash))
src = '\n'.join(out_lines)
if src != orig:
    open(p, 'w', encoding='utf-8').write(src)
" 2>/dev/null
done
echo "Phase 3 done."
