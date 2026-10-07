import capstone

with open('f:/NextClient-1/scratch/reunion/bin/Windows/reunion_mm.dll', 'rb') as f:
    data = f.read()

def va_to_off(va):
    return 0x400 + (va - 0x10001000)

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
va = 0x10003F40
off = va_to_off(va)
chunk = data[off:off+100]
print(f'=== Disasm {hex(va)} ===')
for insn in md.disasm(chunk, va):
    print(f'0x{insn.address:08X}:  {insn.mnemonic:8} {insn.op_str}')
