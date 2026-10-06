import subprocess

data = open('f:/NextClient-1/scratch/ticket_func.bin', 'rb').read()
bytes_str = ', '.join(f'0x{b:02x}' for b in data)
code = f'#pragma section(".mycode", read, execute)\n__declspec(allocate(".mycode")) const unsigned char func[] = {{ {bytes_str} }};\n'
with open('f:/NextClient-1/scratch/disasm_test.c', 'w') as f:
    f.write(code)

cl_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\cl.exe'
dumpbin_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe'

res = subprocess.run([cl_path, '/c', 'f:/NextClient-1/scratch/disasm_test.c', '/Fof:/NextClient-1/scratch/disasm_test.obj'], capture_output=True, text=True)
print(res.stdout, res.stderr)
if res.returncode == 0:
    dis = subprocess.check_output([dumpbin_path, '/disasm', 'f:/NextClient-1/scratch/disasm_test.obj'], text=True)
    with open('f:/NextClient-1/scratch/disasm_out.txt', 'w') as f:
        f.write(dis)
    print('Disassembled successfully! Lines:', len(dis.splitlines()))
