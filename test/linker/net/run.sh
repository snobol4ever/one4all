#!/bin/bash
# test/linker/net/run.sh — LP-4 acceptance test: M-LINK-NET-3
# Ref: SESSION-linker-net.md §Step 6
#
# Expected output: Hello, World
# Pass condition:  mono greet_main.exe prints exactly "Hello, World"
set -e

SNO2C=../../../src/driver/sno2c
RUNTIME=../../../src/runtime/net
OUT=./out
mkdir -p "$OUT"

# Copy runtime DLLs so ilasm/mono can find them
cp "$RUNTIME"/snobol4lib.dll "$OUT"/
cp "$RUNTIME"/snobol4run.dll "$OUT"/

echo "--- compiling greet_lib.sno ---"
"$SNO2C" -net greet_lib.sno > "$OUT"/SNOBOL4_greet_lib.il

echo "--- compiling greet_main.sno ---"
"$SNO2C" -net greet_main.sno > "$OUT"/SNOBOL4_greet_main.il

echo "--- assembling greet_lib.dll ---"
ilasm "$OUT"/SNOBOL4_greet_lib.il /dll /output:"$OUT"/SNOBOL4_greet_lib.dll

echo "--- assembling greet_main.exe ---"
ilasm "$OUT"/SNOBOL4_greet_main.il /exe /output:"$OUT"/greet_main.exe

echo "--- running ---"
RESULT=$(cd "$OUT" && mono greet_main.exe)

if [ "$RESULT" = "Hello, World" ]; then
    echo "M-LINK-NET-3 ✅  $RESULT"
else
    echo "M-LINK-NET-3 ❌  got: '$RESULT'"
    exit 1
fi
