#!/usr/bin/env bash
# util_regen_demo_s_artifacts.sh — regenerate x64 demo .s artifacts and commit to corpus.
# Run before handoff on any session touching bb_emit.c, bb_templates.c,
# sm_templates.c, sm_codegen_x64_emit.c, or rt.c.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/scrip}"
CORPUS="${CORPUS:-/home/claude/corpus}"
DEMO="$CORPUS/programs/snobol4/demo"

if [ ! -x "$SCRIP" ]; then echo "SKIP  scrip not found: $SCRIP"; exit 0; fi
if [ ! -d "$DEMO" ]; then echo "SKIP  corpus demo dir not found: $DEMO"; exit 0; fi

cd "$DEMO"

echo "Emitting .s artifacts..."
for f in roman wordcount claws5 treebank-list treebank-array; do
    timeout 30 "$SCRIP" --compile "$f.sno" > "$f.s" 2>/dev/null \
        && echo "  emit OK   $f.s" \
        || { echo "  emit FAIL $f.s"; exit 1; }
done

echo "Verifying gcc -c clean..."
for s in roman.s wordcount.s claws5.s treebank-list.s treebank-array.s sm_macros.s bb_macros.s; do
    gcc -c "$s" -o /tmp/$(basename "$s" .s).o 2>/tmp/as_err.txt \
        && echo "  gcc  OK   $s" \
        || { echo "  gcc  FAIL $s"; cat /tmp/as_err.txt; exit 1; }
done

echo "Committing to corpus..."
cd "$CORPUS"
git add programs/snobol4/demo/{roman,wordcount,claws5,treebank-list,treebank-array,sm_macros,bb_macros}.s
if git diff --cached --quiet; then
    echo "  No changes — artifacts already current."
else
    RUNG="${1:-regen}"
    git commit -m "x64 artifacts: $RUNG"
    echo "  Committed."
fi
echo "Done."
