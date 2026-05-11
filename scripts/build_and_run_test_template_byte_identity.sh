#!/usr/bin/env bash
# build_and_run_test_template_byte_identity.sh — EM-MODE4-IS-MODE3-DUMP-c
#
# Builds and runs the unit test that drives the SM_HALT template
# through a binary emitter and verifies the emitted bytes match
# the legacy emit_halt_blob's output exactly (41 ff 45 14 c3).
#
# Self-contained per RULES.md: paths derived from $0.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SRC="$ROOT/src"
RT="$SRC/runtime"

# Build the unit test alongside its dependencies.  We only need the
# emitter machinery + the template + bb_emit (for bb_emit_buf state).
CC="gcc"
CFLAGS="-O0 -g -w -I$SRC -I$RT/x86 -I$RT -DDYN_ENGINE_LINKED"
OUT="/tmp/test_template_byte_identity"

# Minimal dependency set: just the emitter + bb_emit + template.
# bb_emit transitively pulls in some headers but no other .c files
# at the level we need for the binary backend.
$CC $CFLAGS \
    "$RT/x86/test_template_byte_identity.c" \
    "$RT/x86/emitter_binary.c" \
    "$RT/x86/emitter_text.c" \
    "$RT/x86/templates/sm_halt.c" \
    "$RT/x86/bb_emit.c" \
    -lgc -lm \
    -o "$OUT" 2>&1
rc=$?
if [ $rc -ne 0 ]; then
    echo "FAIL: build failed (rc=$rc)"
    exit 1
fi

"$OUT"
exit $?
