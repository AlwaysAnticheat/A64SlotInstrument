#include "A64SlotInstrument.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

extern "C" __attribute__((visibility("hidden"))) void A64SlotInstrumentEntry();

namespace
{
    constexpr uint32_t BTIJC = 0xD50324DF;
    constexpr uint32_t STP_X16_X17_PRE = 0xA9BF47F0;
    constexpr uint32_t LDR_X16_12 = 0x58000090;
    constexpr uint32_t LDR_X17_16 = 0x580000B1;
    constexpr uint32_t BR_X17 = 0xD61F0220;

    static_assert(A64_STUB_HOOK + sizeof(uintptr_t) <= A64_STUB_SIZE, "Stub hook literal out of bounds");
    static_assert(A64_STUB_ENTRY + sizeof(uintptr_t) <= A64_STUB_SIZE, "Stub entry literal out of bounds");
    static_assert((A64_STUB_SIZE & 3u) == 0, "Stub size must be instruction aligned");

    struct Hook
    {
        void *Original;
        A64SlotInstrument::Callback CallbackFn;
        uintptr_t Address;
        void *Stub;
        void *StubPage;
        size_t StubPageSize;
    };

    std::mutex HooksMutex;
    std::vector<std::unique_ptr<Hook>> Hooks;



    void Write32(uint8_t *Destination, uint32_t Value)
    {
        memcpy(Destination, &Value, sizeof(Value));
    }

    bool IsAligned(uintptr_t Address)
    {
        return (Address & (sizeof(void *) - 1)) == 0;
    }

    void *CreateStub(Hook *Entry)
    {
        const long PageSizeLong = sysconf(_SC_PAGESIZE);
        if (PageSizeLong <= 0)
            return nullptr;

        const size_t PageSize = (size_t)PageSizeLong;
        void *Page = mmap(nullptr,
                          PageSize,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0);

        if (Page == MAP_FAILED)
            return nullptr;

        uint8_t *Code = (uint8_t *)Page;

        Write32(Code + 0, BTIJC);
        Write32(Code + 4, STP_X16_X17_PRE);
        Write32(Code + 8, LDR_X16_12);
        Write32(Code + 12, LDR_X17_16);
        Write32(Code + 16, BR_X17);
        Write32(Code + 20, 0xD503201F);

        memcpy(Code + A64_STUB_HOOK, &Entry, sizeof(Entry));
        uintptr_t EntryAddress = (uintptr_t)&A64SlotInstrumentEntry;
        memcpy(Code + A64_STUB_ENTRY, &EntryAddress, sizeof(EntryAddress));

        __builtin___clear_cache((char *)Code, (char *)Code + A64_STUB_SIZE);

        if (mprotect(Page, PageSize, PROT_READ | PROT_EXEC) != 0)
        {
            munmap(Page, PageSize);
            return nullptr;
        }

        Entry->StubPage = Page;
        Entry->StubPageSize = PageSize;
        return Code;
    }

    void DestroyStub(Hook *Entry)
    {
        if (!Entry || !Entry->StubPage || !Entry->StubPageSize)
            return;

        munmap(Entry->StubPage, Entry->StubPageSize);
        Entry->Stub = nullptr;
        Entry->StubPage = nullptr;
        Entry->StubPageSize = 0;
    }


}

extern "C" __attribute__((visibility("hidden"), noinline))
A64SlotInstrument::Action A64SlotInstrumentDispatch(
    void *EntryPointer,
    A64SlotInstrument::RegisterContext *Context)
{
    Hook *Entry = (Hook *)EntryPointer;

    if (!Entry || !Context || !Entry->CallbackFn)
        return A64SlotInstrument::Action::CallOriginal;

    Context->HookAddress = Entry->Address;
    Context->Original = (uintptr_t)Entry->Original;
    Context->PC = (uintptr_t)Entry->Stub;

    const A64SlotInstrument::Action Result = Entry->CallbackFn(*Context);
    return Result == A64SlotInstrument::Action::ReturnZero
        ? A64SlotInstrument::Action::ReturnZero
        : A64SlotInstrument::Action::CallOriginal;
}

namespace A64SlotInstrument
{
    bool Instrument(uintptr_t Address, Callback CallbackFn)
    {
        if (!Address || !CallbackFn || !IsAligned(Address))
            return false;

        std::lock_guard<std::mutex> Lock(HooksMutex);

        for (const auto &Entry : Hooks)
        {
            if (Entry->Address == Address)
                return false;
        }

        Hooks.reserve(Hooks.size() + 1);

        void **Slot = (void **)Address;
        void *Original = __atomic_load_n(Slot, __ATOMIC_ACQUIRE);
        if (!Original || (((uintptr_t)Original) & 3u) != 0)
            return false;

        std::unique_ptr<Hook> Entry(new Hook{
            Original,
            CallbackFn,
            Address,
            nullptr,
            nullptr,
            0
        });

        void *Stub = CreateStub(Entry.get());
        if (!Stub)
            return false;

        Entry->Stub = Stub;

        void *Expected = Original;
        if (!__atomic_compare_exchange_n(Slot, &Expected, Stub, false,
                                          __ATOMIC_RELEASE,
                                          __ATOMIC_ACQUIRE))
        {
            DestroyStub(Entry.get());
            return false;
        }

        Hooks.emplace_back(std::move(Entry));
        return true;
    }
}
