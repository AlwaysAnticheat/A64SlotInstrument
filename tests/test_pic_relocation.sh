#!/bin/sh
set -eu
BUILD=$(mktemp -d)
trap "rm -rf \"$BUILD\"" EXIT
cat > "$BUILD/probe.cpp" <<'CPP'
typedef unsigned long uintptr_t;
extern "C" __attribute__((visibility("hidden"))) void A64SlotInstrumentEntry();
extern "C" __attribute__((visibility("hidden"))) uintptr_t Probe()
{
    return (uintptr_t)&A64SlotInstrumentEntry;
}
CPP
clang++ --target=aarch64-none-linux-android29 -ffreestanding -fPIC -nostdinc -c "$BUILD/probe.cpp" -o "$BUILD/probe.o"
if readelf -r "$BUILD/probe.o" | grep -q "R_AARCH64_ABS64"; then
    echo "ABS64 relocation detected in PIC probe" >&2
    exit 1
fi
llvm-objdump -dr "$BUILD/probe.o" | grep -q "R_AARCH64_ADR_PREL_PG_HI21"
llvm-objdump -dr "$BUILD/probe.o" | grep -q "R_AARCH64_ADD_ABS_LO12_NC"
printf "%s\n" "AArch64 PIC relocation test: PASS"
