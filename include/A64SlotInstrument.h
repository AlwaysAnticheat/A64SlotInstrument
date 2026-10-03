#pragma once

#include <stddef.h>
#include <stdint.h>

#include "A64SlotInstrumentLayout.h"

namespace A64SlotInstrument
{
    enum class Action : uint32_t
    {
        CallOriginal = 0,
        ReturnZero = 1
    };

    struct alignas(16) Vector128
    {
        uint64_t Lo;
        uint64_t Hi;
    };

    struct alignas(16) RegisterContext
    {
        uint64_t X[30];
        uint64_t LR;
        uintptr_t SP;
        uintptr_t PC;
        uint64_t NZCV;
        uint64_t FPCR;
        uint64_t FPSR;
        Vector128 V[32];
        uintptr_t HookAddress;
        uintptr_t Original;
    };

    using Callback = Action (*)(const RegisterContext &Context);

    bool Instrument(uintptr_t Address, Callback CallbackFn);

    static_assert(sizeof(uintptr_t) == 8, "A64SlotInstrument requires AArch64");
    static_assert(sizeof(Vector128) == 16, "Vector128 size");
    static_assert(offsetof(RegisterContext, X) == 0, "RegisterContext::X");
    static_assert(offsetof(RegisterContext, X[29]) == A64_CTX_X29, "RegisterContext::X29");
    static_assert(offsetof(RegisterContext, LR) == A64_CTX_LR, "RegisterContext::LR");
    static_assert(offsetof(RegisterContext, SP) == A64_CTX_SP, "RegisterContext::SP");
    static_assert(offsetof(RegisterContext, PC) == A64_CTX_PC, "RegisterContext::PC");
    static_assert(offsetof(RegisterContext, NZCV) == A64_CTX_NZCV, "RegisterContext::NZCV");
    static_assert(offsetof(RegisterContext, FPCR) == A64_CTX_FPCR, "RegisterContext::FPCR");
    static_assert(offsetof(RegisterContext, FPSR) == A64_CTX_FPSR, "RegisterContext::FPSR");
    static_assert(offsetof(RegisterContext, V) == A64_CTX_V0, "RegisterContext::V");
    static_assert(offsetof(RegisterContext, HookAddress) == A64_CTX_HOOK, "RegisterContext::HookAddress");
    static_assert(offsetof(RegisterContext, Original) == A64_CTX_ORIGINAL, "RegisterContext::Original");
    static_assert(sizeof(RegisterContext) == A64_FRAME_SIZE - 16, "RegisterContext size");
    static_assert(A64_TOTAL_STACK == A64_FRAME_SIZE + 16, "Stack layout");
    static_assert(alignof(RegisterContext) == 16, "RegisterContext alignment");
}
