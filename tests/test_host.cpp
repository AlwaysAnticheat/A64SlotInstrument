#include "A64SlotInstrument.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>

extern "C" void A64SlotInstrumentEntry() {}

static void OriginalFunction() {}

static bool IsReadExecuteOnly(uintptr_t Address)
{
    FILE *File = fopen("/proc/self/maps", "r");
    if (!File)
        return false;

    char Line[512];
    bool Result = false;
    while (fgets(Line, sizeof(Line), File))
    {
        unsigned long Start = 0;
        unsigned long End = 0;
        char Perms[5] = {};
        if (sscanf(Line, "%lx-%lx %4s", &Start, &End, Perms) != 3)
            continue;

        if (Address >= (uintptr_t)Start && Address < (uintptr_t)End)
        {
            Result = Perms[0] == 'r' && Perms[1] == '-' && Perms[2] == 'x';
            break;
        }
    }

    fclose(File);
    return Result;
}

static A64SlotInstrument::Action Callback(const A64SlotInstrument::RegisterContext &Context)
{
    assert(Context.HookAddress != 0);
    assert(Context.Original == (uintptr_t)&OriginalFunction);
    return A64SlotInstrument::Action::ReturnZero;
}



static void TestConcurrentInstallation()
{
    constexpr size_t Count = 16;
    std::vector<void *> Slots(Count);
    for (void *&Slot : Slots)
        Slot = (void *)&OriginalFunction;

    std::vector<bool> Results(Count, false);
    std::vector<std::thread> Threads;
    Threads.reserve(Count);

    for (size_t Index = 0; Index < Count; ++Index)
    {
        Threads.emplace_back([&, Index]() {
            Results[Index] = A64SlotInstrument::Instrument((uintptr_t)&Slots[Index], Callback);
        });
    }

    for (std::thread &Thread : Threads)
        Thread.join();

    for (bool Result : Results)
        assert(Result);
}

int main()
{
    void *Slot = (void *)&OriginalFunction;
    const uintptr_t SlotAddress = (uintptr_t)&Slot;

    assert(A64SlotInstrument::Instrument(SlotAddress, Callback));
    assert(!A64SlotInstrument::Instrument(SlotAddress, Callback));

    void *Stub = Slot;
    assert(Stub != nullptr);
    assert(IsReadExecuteOnly((uintptr_t)Stub));

    const uint32_t *Code = (const uint32_t *)Stub;
    assert(Code[0] == 0xD50324DF);
    assert(Code[1] == 0xA9BF47F0);
    assert(Code[2] == 0x58000090);
    assert(Code[3] == 0x580000B1);
    assert(Code[4] == 0xD61F0220);
    assert(Code[5] == 0xD503201F);

    uintptr_t HookPointer = 0;
    uintptr_t EntryPointer = 0;
    memcpy(&HookPointer, (const uint8_t *)Stub + 24, sizeof(HookPointer));
    memcpy(&EntryPointer, (const uint8_t *)Stub + 32, sizeof(EntryPointer));

    assert(HookPointer != 0);
    assert(EntryPointer == (uintptr_t)&A64SlotInstrumentEntry);

    void *Empty = nullptr;
    assert(!A64SlotInstrument::Instrument((uintptr_t)&Empty, Callback));

    uint8_t Buffer[16] = {};
    assert(!A64SlotInstrument::Instrument((uintptr_t)(Buffer + 1), Callback));
    assert(!A64SlotInstrument::Instrument(0, Callback));
    assert(!A64SlotInstrument::Instrument(SlotAddress + 1, Callback));
    assert(!A64SlotInstrument::Instrument(SlotAddress, nullptr));

    TestConcurrentInstallation();

    puts("Host installation + stub test: PASS");
    return 0;
}
