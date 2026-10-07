import subprocess

with open('f:/Allclient/platform/steam/games/SmartEmu/SmartSteamEmu/SmartSteamEmu.dll', 'rb') as f:
    dll = f.read()

cl_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\cl.exe'
dumpbin_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe'

chunk = dll[0x96600:0x96b00]
code = '#pragma section(".mycode", read, execute)\n__declspec(allocate(".mycode")) const unsigned char func[] = { ' + ', '.join(f'0x{b:02x}' for b in chunk) + ' };\n'
with open('f:/NextClient-1/scratch/sse_disasm.c', 'w') as f:
    f.write(code)

res = subprocess.run([cl_path, '/c', 'f:/NextClient-1/scratch/sse_disasm.c', '/Fof:/NextClient-1/scratch/sse_disasm.obj'], capture_output=True, text=True)
if res.returncode == 0:
    dis = subprocess.check_output([dumpbin_path, '/disasm', 'f:/NextClient-1/scratch/sse_disasm.obj'], text=True)
    with open('f:/NextClient-1/scratch/sse_disasm.txt', 'w') as f:
        f.write(dis)
    print('Disassembled successfully!')
else:
    print('Failed:', res.stderr)
