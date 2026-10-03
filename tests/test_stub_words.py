from pathlib import Path

source = (Path(__file__).resolve().parents[1] / "src" / "A64SlotInstrument.cpp").read_text()
expected = {
    "BTIJC": 0xD50324DF,
    "STP_X16_X17_PRE": 0xA9BF47F0,
    "LDR_X16_12": 0x58000090,
    "LDR_X17_16": 0x580000B1,
    "BR_X17": 0xD61F0220,
}
for name, value in expected.items():
    needle = f"constexpr uint32_t {name} = 0x{value:08X};"
    assert needle in source, needle

assert "A64_STUB_HOOK" in source
assert "A64_STUB_ENTRY" in source
assert "A64_STUB_SIZE" in source
print("Runtime stub encoding source test: PASS")

assert "memcpy(Code + A64_STUB_HOOK" in source
assert "memcpy(Code + A64_STUB_ENTRY" in source
assert "mprotect(Page, PageSize, PROT_READ | PROT_EXEC)" in source
assert "__builtin___clear_cache" in source
assert "__ATOMIC_ACQUIRE" in source
assert "__ATOMIC_RELEASE" in source
assert "__atomic_compare_exchange_n" in source
assert "DestroyStub" in source
print("Runtime safety/source invariants: PASS")
