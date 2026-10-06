import subprocess
import struct

dumpbin_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe'
cl_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\cl.exe'

with open(r'D:\Allclient\hw.dll', 'rb') as f:
    data = f.read()

e_lfanew = struct.unpack_from('<I', data, 0x3c)[0]
opt_hdr_offset = e_lfanew + 4 + 20
image_base = struct.unpack_from('<I', data, opt_hdr_offset + 28)[0]
num_sections = struct.unpack_from('<H', data, e_lfanew + 4 + 2)[0]
sec_offset = opt_hdr_offset + struct.unpack_from('<H', data, e_lfanew + 4 + 16)[0]

va = 0x01DC3520
rva = va - image_base
raw_offset = None
for i in range(num_sections):
    sec = data[sec_offset + i*40 : sec_offset + (i+1)*40]
    name = sec[:8].rstrip(b'\x00').decode('latin1')
    vsize, vaddr, rsize, raddr = struct.unpack_from('<IIII', sec, 8)
    if vaddr <= rva < vaddr + vsize:
        raw_offset = rva - vaddr + raddr
        break

print(f"image_base={hex(image_base)}, raw_offset={hex(raw_offset)}")

chunk = data[raw_offset : raw_offset + 0x160]
bytes_str = ', '.join(f'0x{b:02x}' for b in chunk)
code = '#pragma section(".mycode", read, execute)\n__declspec(allocate(".mycode")) const unsigned char func[] = { ' + bytes_str + ' };\n'
with open('f:/NextClient-1/scratch/voice_record.c', 'w') as f:
    f.write(code)

res = subprocess.run([cl_path, '/c', 'f:/NextClient-1/scratch/voice_record.c', '/Fof:/NextClient-1/scratch/voice_record.obj'], capture_output=True, text=True)
if res.returncode == 0:
    dis = subprocess.check_output([dumpbin_path, '/disasm', 'f:/NextClient-1/scratch/voice_record.obj'], text=True)
    with open('f:/NextClient-1/scratch/voice_disasm.txt', 'w') as f:
        f.write(dis)
    print("Disassembled lines:", len(dis.splitlines()))
else:
    print("cl error:", res.stderr)
