# A64SlotInstrument

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![Arch: AArch64](https://img.shields.io/badge/arch-AArch64-informational)](#platform-support)
[![C++: 17](https://img.shields.io/badge/C%2B%2B-17-blue)](CMakeLists.txt)

A small, allocation-free **AArch64 (ARM64) instrumentation framework** for
hooks placed at *writable function-pointer slots* — the kind you find in
`.data` dispatch tables, vtables that live in RW memory, and runtime
state-machine fan-outs.

Instead of patching code, A64SlotInstrument swaps the slot's function
pointer for a per-slot executable stub, captures the full ARM64 machine
state at the call boundary, and hands it to your callback. The callback
chooses whether to forward the call to the original target or to shortcut
with `return 0`.

If you are familiar with [Dobby](https://github.com/jmpews/Dobby) or
[bytedance/android-inline-hook](https://github.com/bytedance/android-inline-hook),
this library is **not** an inline-hook engine — it is deliberately smaller
and only solves the "writable slot" case. It is useful where you control
the dispatch table but cannot (or would rather not) rewrite target
instructions.

---

## Table of contents

- [Features](#features)
- [Motivation — obfuscated indirect-branch dispatch](#motivation--obfuscated-indirect-branch-dispatch)
- [When to use this (and when not to)](#when-to-use-this-and-when-not-to)
- [Platform support](#platform-support)
- [Build](#build)
- [Quick start](#quick-start)
- [Control flow at runtime](#control-flow-at-runtime)
- [Examples](#examples)
- [API reference](#api-reference)
- [Captured register state](#captured-register-state)
- [How it works](#how-it-works)
- [Threading model](#threading-model)
- [Limits](#limits)
- [Project layout](#project-layout)
- [Testing](#testing)
- [License](#license)

---

## Features

- **No code patching.** The target function's instructions are never
  modified; only the `.data` slot that holds the function pointer is
  updated with release semantics.
- **Full AArch64 register snapshot.** Callbacks receive X0–X29, LR, SP, PC
  of the stub, NZCV, FPCR, FPSR, and all 32 128-bit SIMD/NEON V-registers.
- **BTI-safe stubs.** Each stub starts with `BTI JC`, so slots reachable
  through indirect `BR` on BTI-enabled targets land cleanly.
- **PIC stub body.** The hook pointer and the dispatcher pointer live in a
  literal pool inside the same stub page, so stub generation needs no
  link-time relocations.
- **Allocation-free hot path.** The callback path takes no mutex and does
  no heap work.
- **Signature agnostic.** Because the full register file is forwarded, the
  framework does not need to know the C/C++ signature of the hooked
  function.
- **Shortcut-return path.** `Action::ReturnZero` restores register state,
  clears `X0`, and returns to the caller without executing the original.

## Motivation — obfuscated indirect-branch dispatch

A64SlotInstrument came out of a very specific reversing problem: **a
heavily obfuscated AArch64 binary whose "functions" aren't really
functions.** The obfuscator breaks each logical routine into dozens of
small basic blocks and glues them together through a `.data` dispatch
table and an indirect `BR Xn`. Every block ends with something like:

```asm
__text:00000000000A78D4 sub_A78D4
__text:00000000000A78D4   SUB   SP, SP, #0x70
__text:00000000000A78D8   STP   X26, X25, [SP,#0x60+var_40]
                          ; ... normal prologue + state setup ...
__text:00000000000A7940   LDR   X8, [X19,#0x60]!
__text:00000000000A7944   MOV   W9,  #0x174A0DDA
__text:00000000000A794C   ORR   W10, W9, #4
__text:00000000000A7950   EOR   W9,  W10, W9
__text:00000000000A7954   MOV   W10, #0xC3C52265
__text:00000000000A795C   ADD   W11, W10, #7
__text:00000000000A7960   STP   W11, W10, [SP,#0x60+var_48]
__text:00000000000A7964   EOR   W10, W11, W10
__text:00000000000A7968   CMP   X8,  #0
__text:00000000000A796C   CSEL  W10, W9, W10, EQ
__text:00000000000A7970   ADRL  X9,  off_296760           ; <-- .data dispatch table
__text:00000000000A7978   LDR   X10, [X9, W10, UXTW#3]    ; <-- pick next block
__text:00000000000A797C   BR    X10                       ; <-- indirect branch

__text:00000000000A7980 sub_A7980                         ; DATA XREF: __data:2967A8↓o
__text:00000000000A7980   LDR   W10, [X8,#0x18]
__text:00000000000A7984   ADD   W11, W22, #3
__text:00000000000A7988   EOR   W11, W22, W11
                          ; ... another CSEL, ADRL off_296760, BR X10 ...

__text:00000000000A79A8 sub_A79A8                         ; DATA XREF: __data:296798↓o
__text:00000000000A79A8   MOV   W10, #0x10
__text:00000000000A79AC   MOV   W11, #8
                          ; ... another CSEL, ADRL off_296760, BR X10 ...
```

And the corresponding dispatch table in `.data`:

```
__data:00000000002967A8 off_2967A8      DCQ sub_A7980   ; one BB entry
__data:0000000000296798                 DCQ sub_A79A8   ; next BB entry
__data:0000000000296788                 DCQ sub_A79DC   ; next BB entry
                                        ; ... hundreds of entries ...
```

Each "function" IDA names (`sub_A7980`, `sub_A79A8`, `sub_A79DC`, …) is a
basic block of the *same* logical routine, reached only through
`BR X10` after the dispatcher resolves `off_296760[idx]`. That has two
consequences for an instrumentation author:

1. **Classic inline hooks don't apply here.** `sub_A7980` has no normal
   prologue — the previous block already set up the frame. Patching the
   first instruction would corrupt the shared register state (`X19`,
   `X22`–`X25` are being used as rolling obfuscation keys across blocks).
2. **The only stable, writable "handle" is the `.data` slot** that holds
   the pointer the dispatcher is about to load with `LDR X10, [X9,...]`.
   That slot is in RW memory. If you swap its contents for a pointer to
   your own BTI-safe stub, you get called *in the middle of the obfuscated
   routine*, with every register `X0..X29`, `LR`, `SP`, `NZCV`, `FPCR`,
   `FPSR`, and `V0..V31` exactly as the previous block left them — and
   you can read any of them.

That is exactly what this library gives you. You pick a slot address
(for example `0x2967A8` relative to the module base, pointing at
`sub_A7980`), register a C++ callback, and the next time the dispatcher
runs `BR X10` through that slot, control enters your callback with the
full ARM64 register file captured and you can either forward to the
original block or short-circuit the whole routine with `ReturnZero`.

```cpp
#include "A64SlotInstrument.h"

// Called when the obfuscated dispatcher branches through .data:0x2967A8.
// At entry, every X-register / V-register below is the live state the
// previous basic block (sub_A78D4) left in flight.
static A64SlotInstrument::Action OnBlockA7980(
    const A64SlotInstrument::RegisterContext &Ctx)
{
    const uint64_t  ObfuscationKey = Ctx.X[22];     // rolling W22 key
    const uint64_t  StructPointer  = Ctx.X[19];     // 'this'-like ptr
    const uint64_t  LoadedWord     = Ctx.X[8];      // from LDR X8,[X19,#0x60]!

    // Inspect, log, compare against our own model, etc.
    if (LoadedWord == 0 && ObfuscationKey == 0x78FFFE3B)
        return A64SlotInstrument::Action::ReturnZero;  // bail the whole routine

    return A64SlotInstrument::Action::CallOriginal;    // let sub_A7980 run
}

void InstallBlockHook(uintptr_t ModuleBase)
{
    // 0x2967A8 is the .data slot that holds the pointer to sub_A7980.
    A64SlotInstrument::Instrument(ModuleBase + 0x2967A8, &OnBlockA7980);
}
```

The appeal of this approach for obfuscated binaries:

- You never touch `.text`. No cache coherency games, no CFI/BTI-ABI
  surprises on the hooked function, no risk of mis-decoding an
  instruction stream the obfuscator has intentionally mangled.
- You get **register-level introspection at an arbitrary point inside
  the obfuscated control flow**, chosen by picking which `.data` slot to
  swap.
- The stub is PIC and self-contained, so you can install it at runtime
  from an injected `.so` without touching the loader.

## When to use this (and when not to)

**Good fit**

- You own, or can enumerate, a dispatch table of function pointers (e.g.,
  a state-machine vtable in `.data`).
- Targets expect a normal AArch64 call-site ABI at entry.
- You want to observe or short-circuit calls at the slot boundary without
  touching code pages.

**Not a fit**

- You need to hook arbitrary already-linked code (use Dobby or
  android-inline-hook instead).
- Your target uses pointer authentication for the stored slot pointer.
- Your hooked path uses SVE / SME scalable vector state.

## Platform support

| Platform | Status |
|---|---|
| Android AArch64 (API 23+) | Primary target |
| Linux AArch64             | Supported |
| macOS arm64               | Host tests only; `mmap`/`mprotect` path works, but the slot-writable assumption rarely holds for shipping Mach-O binaries |
| iOS arm64                 | Not supported out of the box (code-signing / pointer auth) |
| 32-bit ARM, x86, x86_64   | Not supported |

An AArch64 toolchain (`aarch64-linux-gnu-*`, Android NDK, or
`clang -target aarch64-linux-android`) is required to build the library
itself; the host-side tests build on any 64-bit host and exercise the
installer logic without executing the stub.

## Build

```bash
cmake -S . -B build \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
    -DCMAKE_ANDROID_NDK=$ANDROID_NDK_HOME \
    -DCMAKE_SYSTEM_VERSION=23
cmake --build build -j
```

As a subproject:

```cmake
add_subdirectory(third_party/A64SlotInstrument)
target_link_libraries(my_module PRIVATE A64SlotInstrument::A64SlotInstrument)
```

CMake options:

| Option | Default | Meaning |
|---|---|---|
| `A64SLOTINSTRUMENT_BUILD_EXAMPLES` | `ON` | Build the snippets in `examples/` as a static library. |

## Quick start

```cpp
#include "A64SlotInstrument.h"

static A64SlotInstrument::Action MyCallback(
    const A64SlotInstrument::RegisterContext &Context)
{
    // Inspect any captured register, decide what happens next.
    if (Context.X[0] == 0)
        return A64SlotInstrument::Action::ReturnZero;

    return A64SlotInstrument::Action::CallOriginal;
}

void InstallAt(uintptr_t ModuleBase)
{
    const uintptr_t SlotAddress = ModuleBase + 0x539318;
    A64SlotInstrument::Instrument(SlotAddress, MyCallback);
}
```

`SlotAddress` is the address of the **slot** that holds the function
pointer, not the address of the function itself.

## Control flow at runtime

Here is what happens on a single call once a slot has been instrumented.
Follow the arrows top-to-bottom; the dashed box is the per-slot stub
A64SlotInstrument generates.

```
          Obfuscated dispatcher inside the target
          ───────────────────────────────────────
             ADRL  X9, off_296760
             LDR   X10, [X9, W_idx, UXTW#3]   ; loads slot .data:0x..2967A8
             BR    X10                        ; indirect branch
                     │
                     │   (slot's stored pointer no longer points at sub_A7980;
                     │    A64SlotInstrument swapped it for the stub below)
                     ▼
     ┌──────────────────────────────────────────────────────┐
     │  Per-slot stub (RX page, 40 bytes, PIC literal pool) │
     │                                                      │
     │   bti  jc                                            │
     │   stp  x16, x17, [sp, #-16]!       ; preserve x16/x17│
     │   ldr  x16, hook_literal           ; x16 = &Hook     │
     │   ldr  x17, dispatcher_literal     ; x17 = &Entry    │
     │   br   x17                                           │
     └───────────────────────────┬──────────────────────────┘
                                 │
                                 ▼
     A64SlotInstrumentEntry  (assembly, src/A64SlotInstrumentEntry.S)
       1. Reserve a RegisterContext frame on the hooked stack.
       2. Spill X0..X29, LR, NZCV, FPCR, FPSR, V0..V31.
       3. Reconstruct the pre-stub SP and store it in Context.SP.
                                 │
                                 ▼
     A64SlotInstrumentDispatch (C++, src/A64SlotInstrument.cpp)
       Context.HookAddress = slot .data:0x..2967A8
       Context.Original    = original pointer to sub_A7980
       Context.PC          = this stub's address
                                 │
                                 ▼
     YOUR CALLBACK  (A64SlotInstrument::Callback)
       Reads any X[i], LR, SP, X29, V[i], FPCR, FPSR, NZCV it needs.
       Returns Action::CallOriginal or Action::ReturnZero.
            │                                              │
    CallOriginal                                       ReturnZero
            │                                              │
            ▼                                              ▼
     Reload every reg, restore x16/x17            Reload every reg,
     from the stub's STP slot, then               set x0 = 0, then
     BR Entry->Original  (= real sub_A7980).      RET via caller's LR.
```

Two things worth calling out:

- The slot write that activates the hook is a single release-CAS from
  the slot's original value to the stub's entry address. If another
  thread raced us (the slot changed), install fails cleanly and nothing
  is swapped.
- `X16` / `X17` are the AArch64 intra-procedure-call scratch registers.
  The stub saves them before using them so the callback sees the correct
  caller values; on `CallOriginal`, `X16` necessarily carries the
  indirect-branch target during the final `BR`, so the original block
  observes the standard "X16 is scratch at call-site" ABI invariant.

## Examples

Three self-contained snippets live in [`examples/`](examples). Each one
installs a different kind of callback to illustrate what is reachable
through `RegisterContext`:

| File | What it shows |
|---|---|
| [`examples/01_filter_by_argument.cpp`](examples/01_filter_by_argument.cpp) | Reading **X0** as a C-string argument and short-circuiting the call with `Action::ReturnZero` when the name matches a block list. Demonstrates the integer-argument registers X0–X7. |
| [`examples/02_fp_neon_inspect.cpp`](examples/02_fp_neon_inspect.cpp) | Snapshotting **V0** as both a 128-bit vector and a pair of `double` lanes, plus reading **FPCR** / **FPSR** to see which IEEE floating-point exception flags the hooked call raised. |
| [`examples/03_frame_lr_sp.cpp`](examples/03_frame_lr_sp.cpp) | Recording **LR**, **SP**, and **X29** (frame pointer) and walking the frame chain to produce a lightweight synchronous backtrace without libunwind. |

Each example is also wired into the optional
`A64SlotInstrumentExamples` CMake target.

## API reference

```cpp
namespace A64SlotInstrument {

enum class Action : uint32_t {
    CallOriginal = 0,  // forward to the original target
    ReturnZero   = 1,  // skip the target, return 0 to the caller
};

struct RegisterContext { /* see below */ };
using Callback = Action (*)(const RegisterContext &);

bool Instrument(uintptr_t SlotAddress, Callback CallbackFn);

}
```

`Instrument` returns `false` for any of:

- `SlotAddress == 0` or misaligned,
- `CallbackFn == nullptr`,
- the slot already contains a null or misaligned pointer,
- the slot has already been instrumented,
- the stub page could not be mapped RX,
- another thread's write raced with the install (CAS failure).

Installation should run during initialization, before other threads can
actively execute the slot.

## Captured register state

The callback receives, by const reference, this snapshot of the AArch64
machine state at the hook boundary:

| Field | Meaning |
|---|---|
| `X[0..29]` | General-purpose registers at call entry. X16/X17 are the pre-stub values; see [How it works](#how-it-works). |
| `LR` (X30) | Return address the slot would have branched to. |
| `SP`       | Stack pointer **before** the dispatcher allocated the context frame. |
| `PC`       | Address of the generated stub (AArch64 has no architectural readable PC). |
| `NZCV`     | Condition flags. |
| `FPCR`, `FPSR` | Floating-point control and status registers. |
| `V[0..31]` | All 32 full 128-bit SIMD/NEON vector registers. |
| `HookAddress` | The slot address passed to `Instrument`. |
| `Original`    | The slot's original function pointer, restored before tail-branch. |

Offsets are frozen as macros in
[`include/A64SlotInstrumentLayout.h`](include/A64SlotInstrumentLayout.h) and
`static_assert`ed against the C++ struct in
[`include/A64SlotInstrument.h`](include/A64SlotInstrument.h), so the
assembly entry can address every field by constant offset.

## How it works

Each instrumented slot gets a private 40-byte executable stub:

```asm
bti jc
stp x16, x17, [sp, #-16]!   ; preserve caller x16/x17
ldr x16, hook_literal       ; x16 <- &Hook (literal pool, +24)
ldr x17, dispatcher_literal ; x17 <- &A64SlotInstrumentEntry (literal pool, +32)
br  x17
```

The literal pool is embedded in the same stub, so there is **no ABS64
relocation and no runtime linker involvement**.

The stub page is created `RW`, populated, cache-invalidated with
`__builtin___clear_cache`, and then flipped to `RX` via `mprotect`. The
target's `.data` page is never modified by this library; the one write
A64SlotInstrument performs is the atomic release-store that replaces the
slot's pointer with the stub's entry address, and that write is protected
by a compare-and-swap against the original value, so a lost race aborts
the install cleanly instead of overwriting someone else's change.

The dispatcher `A64SlotInstrumentEntry` is written in assembly. It:

1. Allocates a `RegisterContext`-sized frame on the hooked thread's stack.
2. Spills X0–X29, LR, NZCV, FPCR, FPSR, and V0–V31 into that frame.
3. Reconstructs the caller's SP (the stub pushed X16/X17) and writes it
   into `RegisterContext::SP`.
4. Calls `A64SlotInstrumentDispatch`, which invokes the user callback.
5. On `CallOriginal`, reloads register state and tail-branches through
   `Entry->Original`.
6. On `ReturnZero`, reloads register state, sets `X0 = 0`, and returns to
   the original caller via `LR`.

`X16` and `X17` are AArch64 **intra-procedure-call scratch** registers and
are caller-saved per the standard ABI; the stub preserves them before use,
but on the `CallOriginal` tail-branch `X16` necessarily carries the
indirect branch target, so its value at the original function's entry is
not guaranteed to match the caller's.

## Threading model

- Installation is serialized by a mutex and completes with a release-store
  CAS on the slot. Readers of the slot see either the original pointer or
  the fully-constructed, `RX`-mapped stub — never a half-built state.
- The callback path **takes no mutex and performs no dynamic allocation**.
- A callback must not throw C++ exceptions across the assembly boundary.
- Installation should happen during initialization, before other threads
  can execute the slot. A64SlotInstrument does not provide stop-the-world
  synchronization for live threads already inside the target.

## Limits

- Preserves GPRs, NZCV, FPCR, FPSR, and all 32 128-bit SIMD/NEON V-regs.
  Does **not** save/restore scalable SVE / SME state (`Z*`, `P*`, `FFR`,
  ZA). A separate SVE/SME trampoline would be needed if your hooked path
  uses those extensions.
- **Pointer authentication is out of scope.** The pointer stored in the
  slot must be directly callable with `BR`.
- Only AArch64. 32-bit ARM, x86, and x86_64 are not supported.

## Project layout

```
.
├── CMakeLists.txt
├── LICENSE                       # Apache License 2.0
├── NOTICE
├── README.md
├── include/
│   ├── A64SlotInstrument.h       # Public API, RegisterContext layout
│   └── A64SlotInstrumentLayout.h # Frozen struct offsets shared with the asm entry
├── src/
│   ├── A64SlotInstrument.cpp     # Installer, stub construction, dispatcher glue
│   └── A64SlotInstrumentEntry.S  # AArch64 register-saving entry
├── examples/
│   ├── 01_filter_by_argument.cpp # X-register argument filter
│   ├── 02_fp_neon_inspect.cpp    # FPCR / FPSR / V-register inspection
│   └── 03_frame_lr_sp.cpp        # LR / SP / FP backtrace
└── tests/
    ├── test_host.cpp             # Host-side installer / stub tests
    ├── test_layout.cpp           # Struct-offset sanity
    ├── test_aarch64_entry.sh     # Cross-assembly + link smoke test
    ├── test_pic_relocation.sh    # ABS64 relocation probe
    ├── test_register_coverage.py # Register / system-state coverage audit
    ├── test_stub_words.py        # Stub instruction byte validation
    └── run_all.sh                # Combined runner
```

## Testing

`tests/` exercises the installer and the generated stub without needing an
Android device:

- **`test_host.cpp`** — installer, stub creation, RX permissions,
  concurrency, duplicate handling, invalid-input paths, and stub-byte
  validation. Built with AddressSanitizer + UndefinedBehaviorSanitizer.
- **`test_layout.cpp`** — asserts the `RegisterContext` struct layout
  matches the macros used by the assembly entry.
- **`test_aarch64_entry.sh`** — cross-assembles and links
  `A64SlotInstrumentEntry.S` with Clang + LLD.
- **`test_pic_relocation.sh`** — scans the assembled object for ABS64
  relocation patterns and fails if any are present.
- **`test_register_coverage.py`** — audits that every register and system
  bit in the stated contract is actually saved and restored.
- **`test_stub_words.py`** — verifies the exact 32-bit words written into
  each stub.

Run everything from the project root:

```bash
cd tests && ./run_all.sh
```

Final execution inside a real Android AArch64 process still requires the
Android/ARM64 target, which the test harness does not provide.

## Inspiration

This project is scoped much more narrowly than, but was informed by, two
excellent AArch64 hooking libraries:

- [jmpews/Dobby](https://github.com/jmpews/Dobby) — general-purpose
  cross-platform inline hooking.
- [bytedance/android-inline-hook](https://github.com/bytedance/android-inline-hook)
  — production-grade inline hook for Android ARM64.

If you need code-patching inline hooks, use one of those instead.

## License

Licensed under the [Apache License, Version 2.0](LICENSE). See
[`NOTICE`](NOTICE) for attribution.
