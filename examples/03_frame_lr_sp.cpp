// Example 3 — Frame pointer, link register, and stack pointer snapshot.
//
// At callback entry:
//   - X29 holds the caller's frame pointer (per the AAPCS64).
//   - LR  (X30) holds the return address the hooked slot would branch to.
//   - SP  points at the caller's stack (captured before the dispatcher
//     allocated the context frame).
//
// Walking X29 like `fp = *fp` yields a lightweight synchronous backtrace
// without touching libunwind. We record the top four return addresses so a
// profiler thread can later resolve them against the module layout.
//
// NB: pointer authentication is outside this abstraction. On PAC-enabled
// targets the strip of the LR authenticator must happen before the address
// is used as a text pointer. This example assumes a non-PAC build, which is
// consistent with the slot dispatch model documented in the main header.

#include "A64SlotInstrument.h"

#include <atomic>
#include <stdint.h>

namespace
{
    constexpr size_t kBacktraceDepth = 4;

    struct CallSiteRecord
    {
        uintptr_t HookAddress;
        uintptr_t ReturnAddress;
        uintptr_t StackPointer;
        uintptr_t FramePointer;
        uintptr_t Backtrace[kBacktraceDepth];
    };

    std::atomic<CallSiteRecord> LastCallSite;

    bool LooksLikeFrame(uintptr_t Fp, uintptr_t SpFloor)
    {
        if (!Fp)                      return false;
        if (Fp & 0xFu)                return false;
        if (Fp < SpFloor)             return false;
        if (Fp > SpFloor + (1u << 20)) return false;
        return true;
    }

    A64SlotInstrument::Action RecordCallSite(
        const A64SlotInstrument::RegisterContext &Context)
    {
        CallSiteRecord Record{};
        Record.HookAddress   = Context.HookAddress;
        Record.ReturnAddress = Context.LR;
        Record.StackPointer  = Context.SP;
        Record.FramePointer  = Context.X[29];

        uintptr_t Fp = Context.X[29];
        for (size_t Depth = 0; Depth < kBacktraceDepth; ++Depth)
        {
            if (!LooksLikeFrame(Fp, Context.SP))
                break;

            uintptr_t NextFp = *reinterpret_cast<uintptr_t *>(Fp);
            uintptr_t Ret    = *reinterpret_cast<uintptr_t *>(Fp + 8);
            Record.Backtrace[Depth] = Ret;
            if (NextFp <= Fp)
                break;
            Fp = NextFp;
        }

        LastCallSite.store(Record, std::memory_order_relaxed);
        return A64SlotInstrument::Action::CallOriginal;
    }
}

void InstallCallSiteRecorder(uintptr_t SlotAddress)
{
    A64SlotInstrument::Instrument(SlotAddress, RecordCallSite);
}
