// Example 2 — Floating point and NEON register inspection.
//
// AArch64 passes floating point and vector arguments through V0..V7 and
// exposes two status registers (FPCR, FPSR). The RegisterContext captures
// all 32 128-bit SIMD/NEON registers in full, so a callback can read both
// scalar `double`/`float` arguments and whole `float32x4_t` lanes.
//
// This callback snapshots the first vector argument into a user-provided
// buffer and clears the IOC/IDC/IXC exception bits of FPSR into a mirror
// variable so a monitoring thread can tell whether the hooked path raised
// an IEEE exception during this invocation.

#include "A64SlotInstrument.h"

#include <atomic>
#include <stdint.h>
#include <string.h>

namespace
{
    constexpr uint64_t kFPSR_IOC = 1ull << 0;
    constexpr uint64_t kFPSR_DZC = 1ull << 1;
    constexpr uint64_t kFPSR_OFC = 1ull << 2;
    constexpr uint64_t kFPSR_UFC = 1ull << 3;
    constexpr uint64_t kFPSR_IXC = 1ull << 4;
    constexpr uint64_t kFPSR_IDC = 1ull << 7;

    constexpr uint64_t kFPSR_AllExceptions =
        kFPSR_IOC | kFPSR_DZC | kFPSR_OFC | kFPSR_UFC | kFPSR_IXC | kFPSR_IDC;

    struct VectorSnapshot
    {
        uint64_t Lo;
        uint64_t Hi;
        double AsDouble0;
        double AsDouble1;
    };

    std::atomic<VectorSnapshot> LastVectorArgument;
    std::atomic<uint64_t>       LastFPCR{0};
    std::atomic<uint64_t>       LastFPExceptionMask{0};

    A64SlotInstrument::Action InspectFloatingPoint(
        const A64SlotInstrument::RegisterContext &Context)
    {
        VectorSnapshot Snapshot{};
        Snapshot.Lo = Context.V[0].Lo;
        Snapshot.Hi = Context.V[0].Hi;
        memcpy(&Snapshot.AsDouble0, &Context.V[0].Lo, sizeof(double));
        memcpy(&Snapshot.AsDouble1, &Context.V[0].Hi, sizeof(double));
        LastVectorArgument.store(Snapshot, std::memory_order_relaxed);

        LastFPCR.store(Context.FPCR, std::memory_order_relaxed);
        LastFPExceptionMask.store(Context.FPSR & kFPSR_AllExceptions,
                                  std::memory_order_relaxed);

        return A64SlotInstrument::Action::CallOriginal;
    }
}

void InstallFloatingPointInspector(uintptr_t SlotAddress)
{
    A64SlotInstrument::Instrument(SlotAddress, InspectFloatingPoint);
}
