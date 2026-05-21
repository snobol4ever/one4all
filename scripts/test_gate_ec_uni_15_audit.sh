#!/usr/bin/env bash
# test_gate_ec_uni_15_audit.sh — EC-UNI-15 close audit.
#
# Reports matrix coverage (delegated to test_gate_em_template_matrix.sh)
# and fn-size distribution across all SM/BB templates.  Size info is
# informational — EC-UNI-15 declares the structural shape complete when
# matrix coverage is 100%; remaining size deltas are EC-UNI-16 work.
#
# Self-contained per RULES.md.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
exec python3 "$HERE/test_gate_ec_uni_15_audit.py" "$ROOT"
