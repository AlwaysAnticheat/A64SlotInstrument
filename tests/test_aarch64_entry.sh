#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/tests/build-aarch64"
rm -rf "$BUILD"
mkdir -p "$BUILD"

clang --target=aarch64-none-linux-android29 -I"$ROOT/include" -c "$ROOT/src/A64SlotInstrumentEntry.S" -o "$BUILD/A64SlotInstrumentEntry.o"
llvm-objdump -d "$BUILD/A64SlotInstrumentEntry.o" > "$BUILD/disassembly.txt"

for reg in x0 x2 x4 x6 x8 x10 x12 x14 x16 x18 x19 x21 x23 x25 x27 x29 x30; do
    grep -q "\\b$reg\\b" "$BUILD/disassembly.txt"
done
for qreg in q0 q2 q4 q6 q8 q10 q12 q14 q16 q18 q20 q22 q24 q26 q28 q30; do
    grep -q "\\b$qreg\\b" "$BUILD/disassembly.txt"
done
grep -q "sub.*sp.*#0x340" "$BUILD/disassembly.txt"
grep -q "add.*sp.*#0x340" "$BUILD/disassembly.txt"
grep -q "add.*sp.*#0x10" "$BUILD/disassembly.txt"
grep -q "mrs.*NZCV" "$BUILD/disassembly.txt"
grep -q "mrs.*FPCR" "$BUILD/disassembly.txt"
grep -q "mrs.*FPSR" "$BUILD/disassembly.txt"
grep -q "msr.*NZCV" "$BUILD/disassembly.txt"
grep -q "msr.*FPCR" "$BUILD/disassembly.txt"
grep -q "msr.*FPSR" "$BUILD/disassembly.txt"
readelf -r "$BUILD/A64SlotInstrumentEntry.o" | grep -q "R_AARCH64_CALL26"
grep -q "br.*x16" "$BUILD/disassembly.txt"

cat > "$BUILD/dispatch.S" <<'ASM'
.text
.global A64SlotInstrumentDispatch
.hidden A64SlotInstrumentDispatch
.type A64SlotInstrumentDispatch,%function
A64SlotInstrumentDispatch:
    mov w0, wzr
    ret
.size A64SlotInstrumentDispatch, .-A64SlotInstrumentDispatch
.section .note.GNU-stack,"",%progbits
ASM
clang --target=aarch64-none-linux-android29 -c "$BUILD/dispatch.S" -o "$BUILD/dispatch.o"
ld.lld -shared -o "$BUILD/entry.so" "$BUILD/A64SlotInstrumentEntry.o" "$BUILD/dispatch.o"
if readelf -r "$BUILD/entry.so" | grep -q "R_AARCH64_"; then
    echo "Unexpected runtime relocations remain in dispatcher test" >&2
    readelf -r "$BUILD/entry.so" >&2
    exit 1
fi
grep -q "ldr.*x9.*#0x340" "$BUILD/disassembly.txt"
grep -q "ldr.*x9.*#0x348" "$BUILD/disassembly.txt"
printf "%s\n" "AArch64 X16/X17 spill-offset test: PASS"
printf "%s\n" "AArch64 assembly + link test: PASS"
