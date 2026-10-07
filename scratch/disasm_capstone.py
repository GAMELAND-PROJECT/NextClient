import capstone
import struct

with open(r'D:\Allclient\hw.dll', 'rb') as f:
    data = f.read()

e_lfanew = struct.unpack_from('<I', data, 0x3c)[0]
opt_hdr_offset = e_lfanew + 4 + 20
image_base = struct.unpack_from('<I', data, opt_hdr_offset + 28)[0]
num_sections = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
sec_offset = opt_hdr_offset + struct.unpack_from('<H', data, e_lfanew + 4 + 16)[0]

def va_to_raw(va):
    rva = va - image_base
    for i in range(num_sections):
        sec = data[sec_offset + i*40 : sec_offset + (i+1)*40]
        vsize, vaddr, rsize, raddr = struct.unpack_from('<IIII', sec, 8)
        if vaddr <= rva < vaddr + vsize:
            return rva - vaddr + raddr
    return None

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

va_start = 0x01DC3500
raw = va_to_raw(va_start)
chunk = data[raw : raw + 0x200]

print(f"Disassembly from {hex(va_start)}:")
for insn in md.disasm(chunk, va_start):
    print(f"0x{insn.address:08X}:  {insn.mnemonic:8} {insn.op_str}")
