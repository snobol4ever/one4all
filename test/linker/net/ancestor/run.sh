#!/bin/bash
# test/linker/net/ancestor/run.sh — M-LINK-NET-4 acceptance test
# Cross-language call: SNOBOL4 → Prolog ANCESTOR via .NET Byrd-box ABI.
# Hand-authored ancestor.il stands in for -pl -net compiler output.
# Expected output: ann
set -e

SNO2C=../../../../src/driver/sno2c
RUNTIME=../../../../src/runtime/net
OUT=./out
mkdir -p "$OUT"

cp "$RUNTIME"/snobol4lib.dll "$OUT"/
cp "$RUNTIME"/snobol4run.dll "$OUT"/

echo "--- assembling ancestor.dll (Prolog library) ---"
ilasm ancestor.il /dll /output:"$OUT"/ancestor.dll

echo "--- compiling ancestor_main.sno ---"
"$SNO2C" -net ancestor_main.sno > "$OUT"/ancestor_main.il

echo "--- assembling ancestor_main.exe ---"
ilasm "$OUT"/ancestor_main.il /exe /output:"$OUT"/ancestor_main.exe

echo "--- running ---"
RESULT=$(cd "$OUT" && mono ancestor_main.exe)

if [ "$RESULT" = "ann" ]; then
    echo "M-LINK-NET-4 ✅  $RESULT"
else
    echo "M-LINK-NET-4 ❌  got: '$RESULT'"
    exit 1
fi
