#!/usr/bin/env bash
# generate_demo_jvm_artifacts.sh — emit JVM Jasmin .j files for all SNOBOL4 demo programs
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")}" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
DEMO_DIR="${DEMO_DIR:-/home/claude/corpus/programs/snobol4/demo}"
JASM="${JASMIN:-$HERE/../src/backend/jasmin.jar}"

echo "Generating JVM artifacts for SNOBOL4 demo programs..."
echo "Demo dir: $DEMO_DIR"

# List of demo programs to emit
PROGRAMS=(
    "roman.sno:roman"
    "wordcount.sno:wordcount"
    "claws5.sno:claws5"
    "treebank-list.sno:treebank-list"
    "treebank-array.sno:treebank-array"
    "expression.sno:expression"
    "porter.sno:porter"
    "beauty/beauty.sno:beauty"
)

for prog in "${PROGRAMS[@]}"; do
    IFS=':' read -r sno_file base_name <<< "$prog"
    sno_path="$DEMO_DIR/$sno_file"
    j_file="$DEMO_DIR/${base_name}.j"
    
    if [ ! -f "$sno_path" ]; then
        echo "SKIP $base_name (file not found: $sno_path)"
        continue
    fi
    
    echo -n "Emit $base_name ... "
    if ! timeout 30 "$SCRIP" --sm-emit --target=jvm "$sno_path" > "$j_file" 2>/dev/null; then
        echo "FAIL (emit error)"
        rm -f "$j_file"
        continue
    fi
    
    # Filter out stderr messages (sm_lower warnings about undefined labels)
    # The file was created but may contain stderr prefix lines — clean them
    if grep -q "^sm_lower:" "$j_file"; then
        grep -v "^sm_lower:" "$j_file" > "${j_file}.tmp" && mv "${j_file}.tmp" "$j_file"
    fi
    
    lines=$(wc -l < "$j_file")
    echo "OK ($lines lines)"
done

echo "Done. All .j files written to $DEMO_DIR/"
