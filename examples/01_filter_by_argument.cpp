// Example 1 — Argument filtering through general-purpose registers.
//
// The AArch64 PCS passes the first eight integer / pointer arguments in
// X0..X7. This callback inspects X0 as a `const char *` and short-circuits
// the original function when the string matches a block list, otherwise it
// forwards the call unchanged.

#include "A64SlotInstrument.h"

#include <string.h>

namespace
{
    const char *const kBlockedNames[] = {
        "mrpcsGR",
        "diag_dump",
        "crash_report_submit",
    };

    bool IsBlocked(const char *Name)
    {
        if (!Name)
            return false;

        for (const char *Needle : kBlockedNames)
        {
            if (strstr(Name, Needle))
                return true;
        }
        return false;
    }

    A64SlotInstrument::Action FilterByArgument(
        const A64SlotInstrument::RegisterContext &Context)
    {
        const char *Argument0 = reinterpret_cast<const char *>(Context.X[0]);
        return IsBlocked(Argument0)
                   ? A64SlotInstrument::Action::ReturnZero
                   : A64SlotInstrument::Action::CallOriginal;
    }
}

void InstallArgumentFilter(uintptr_t SlotAddress)
{
    A64SlotInstrument::Instrument(SlotAddress, FilterByArgument);
}
