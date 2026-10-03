#include "A64SlotInstrument.h"

#include <assert.h>
#include <stddef.h>

int main()
{
    using namespace A64SlotInstrument;

    assert(offsetof(RegisterContext, X) == 0);
    assert(offsetof(RegisterContext, LR) == A64_CTX_LR);
    assert(offsetof(RegisterContext, SP) == A64_CTX_SP);
    assert(offsetof(RegisterContext, PC) == A64_CTX_PC);
    assert(offsetof(RegisterContext, NZCV) == A64_CTX_NZCV);
    assert(offsetof(RegisterContext, FPCR) == A64_CTX_FPCR);
    assert(offsetof(RegisterContext, FPSR) == A64_CTX_FPSR);
    assert(offsetof(RegisterContext, V) == A64_CTX_V0);
    assert(offsetof(RegisterContext, HookAddress) == A64_CTX_HOOK);
    assert(offsetof(RegisterContext, Original) == A64_CTX_ORIGINAL);
    assert(sizeof(RegisterContext) == 816);
    assert(alignof(RegisterContext) == 16);
    return 0;
}
