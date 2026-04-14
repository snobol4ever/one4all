#!/usr/bin/env bash
# build_spitbol_archive.sh — Build SPITBOL x64 as a linkable static archive.
#
# Produces: x64/libspitbol.a
#
# What this does:
#   - Compiles osint/*.c with -DENGINE=1 -Dmain=spitbol_main -fPIC
#     (ENGINE=1 disables interactive polling/break handling in syspl.c;
#      -Dmain=spitbol_main renames main() so it can coexist with scrip's main)
#   - Assembles bootstrap/sbl.asm and bootstrap/err.asm via nasm
#     (these are the pre-baked SPITBOL runtime assembly — no sbl binary needed)
#   - Archives all .o files into x64/libspitbol.a
#
# Gate: nm x64/libspitbol.a | grep spitbol_main  →  shows symbol
#
# Idempotent: skips compile if libspitbol.a is already up to date.
# Usage: bash scripts/build_spitbol_archive.sh
#
# IM-13

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
X64="${X64:-$(cd "$REPO/../x64" 2>/dev/null && pwd || echo "")}"

# ── locate x64 repo ──────────────────────────────────────────────────────────
if [ -z "$X64" ] || [ ! -d "$X64/osint" ]; then
    echo "FAIL  x64 repo not found (expected at $REPO/../x64 or \$X64)"
    exit 1
fi

# ── check nasm ───────────────────────────────────────────────────────────────
if ! command -v nasm >/dev/null 2>&1; then
    echo "FAIL  nasm not found — run: apt-get install -y nasm"
    exit 1
fi

OUT="$X64/libspitbol.a"
BUILD="$X64/_archive_build"

# ── idempotent check ─────────────────────────────────────────────────────────
if [ -f "$OUT" ]; then
    # Rebuild if any osint source is newer than the archive
    if [ -z "$(find "$X64/osint" "$X64/bootstrap" -name "*.c" -o -name "*.asm" \
               -newer "$OUT" 2>/dev/null)" ]; then
        echo "SKIP  $OUT is up to date"
        nm "$OUT" | grep -q spitbol_main && echo "OK    nm shows spitbol_main" && exit 0
    fi
fi

mkdir -p "$BUILD"
echo "BUILD libspitbol.a from $X64"

# ── compile osint/*.c ─────────────────────────────────────────────────────────
# Flags match the existing Makefile's CFLAGS, plus:
#   -DENGINE=1      suppress interactive break/poll in syspl.c
#   -Dmain=spitbol_main  rename entry so it links alongside scrip's main()
#   -I$X64/osint    headers live here (port.h, osint.h, etc.)
CFLAGS="-Dm64 -DEXTFUN=1 -m64 -no-pie -fPIC \
        -mfpmath=sse -mlong-double-64 -ffloat-store \
        -Wno-unused-command-line-argument \
        -Wno-ignored-optimization-argument \
        -DENGINE=1 \
        -Dmain=spitbol_main \
        -I$X64/osint"

OSINT_OBJS=""
for src in "$X64"/osint/*.c; do
    base="$(basename "$src" .c)"
    obj="$BUILD/${base}.o"
    echo "  CC  osint/$base.c"
    gcc $CFLAGS -c "$src" -o "$obj"
    OSINT_OBJS="$OSINT_OBJS $obj"
done

# ── assemble bootstrap/*.asm ──────────────────────────────────────────────────
# sbl.asm  — the SPITBOL minimal-language runtime (pre-translated from sbl.min)
# err.asm  — error message table
ASM_OBJS=""
for asm in sbl.asm err.asm; do
    # (int.asm assembled separately — lives in x64/ root not bootstrap/)
    src="$X64/bootstrap/$asm"
    if [ ! -f "$src" ]; then
        echo "FAIL  missing $src"
        exit 1
    fi
    base="$(basename "$asm" .asm)"
    obj="$BUILD/${base}_boot.o"
    echo "  ASM bootstrap/$asm"
    nasm -f elf64 -d m64 "$src" -o "$obj"
    ASM_OBJS="$ASM_OBJS $obj"
done

# ── assemble int.asm (register globals — lives in x64/ root) ──────────────
INT_OBJ="$BUILD/int_reg.o"
echo "  ASM int.asm"
nasm -f elf64 -d m64 "$X64/int.asm" -o "$INT_OBJ"
ASM_OBJS="$ASM_OBJS $INT_OBJ"

# ── archive ───────────────────────────────────────────────────────────────────
echo "  AR  $OUT"
ar rcs "$OUT" $OSINT_OBJS $ASM_OBJS

# ── gate ─────────────────────────────────────────────────────────────────────
if nm "$OUT" | grep -q spitbol_main; then
    echo "OK    nm shows spitbol_main in $OUT"
else
    echo "FAIL  spitbol_main not found in $OUT"
    exit 1
fi

echo "OK    $OUT built successfully"
echo "OK    IM-13 gate PASS"
