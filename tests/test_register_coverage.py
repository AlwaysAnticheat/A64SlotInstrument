from pathlib import Path

asm = (Path(__file__).resolve().parents[1] / "src" / "A64SlotInstrumentEntry.S").read_text()
assert ".inst 0xD50324DF" in asm

save_pairs = [(0,1),(2,3),(4,5),(6,7),(8,9),(10,11),(12,13),(14,15),(19,20),(21,22),(23,24),(25,26),(27,28)]
restore_pairs = save_pairs
for a,b in save_pairs:
    assert f"stp x{a}, x{b}" in asm
    assert f"ldp x{a}, x{b}" in asm

assert "ldr x9, [sp, #A64_FRAME_SIZE]" in asm
assert "str x9, [sp, #A64_CTX_X16]" in asm
assert "ldr x9, [sp, #(A64_FRAME_SIZE + 8)]" in asm
assert "str x9, [sp, #A64_CTX_X17]" in asm
for r in (18,29,30):
    assert f"str x{r}" in asm
    assert f"ldr x{r}" in asm

for r in range(32):
    assert f"q{r}" in asm

for token in ("mrs x9, nzcv", "mrs x9, fpcr", "mrs x9, fpsr", "msr nzcv, x9", "msr fpcr, x9", "msr fpsr, x9"):
    assert token in asm

assert "sub sp, sp, #A64_FRAME_SIZE" in asm
assert "add sp, sp, #A64_FRAME_SIZE" in asm
assert "add sp, sp, #16" in asm
assert "bl A64SlotInstrumentDispatch" in asm
assert "br x16" in asm
assert "ret" in asm
print("Register/system-state coverage audit: PASS")
