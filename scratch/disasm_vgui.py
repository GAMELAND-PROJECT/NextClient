import subprocess, struct

dumpbin_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe'
cl_path = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\cl.exe'

with open(r'F:\Allclient\cstrike\cl_dlls\client.dll', 'rb') as f:
    data = f.read()

for target_offset, name in [(0x26b39 - 0x20, 'loc1'), (0x4e35a - 0x20, 'loc2')]:
    chunk = data[target_offset : target_offset + 0xa0]
    bytes_str = ', '.join(f'0x{b:02x}' for b in chunk)
    code = f'#pragma section(".mycode", read, execute)\n__declspec(allocate(".mycode")) const unsigned char func[] = {{ {bytes_str} }};\n'
    with open(f'scratch/{name}.c', 'w') as f:
        f.write(code)
    subprocess.run([cl_path, '/c', f'scratch/{name}.c', f'/Foscratch/{name}.obj'], capture_output=True)
    dis = subprocess.check_output([dumpbin_path, '/disasm', f'scratch/{name}.obj'], text=True)
    print(f'=== {name} ===')
    for line in dis.splitlines()[8:40]:
        print(line)
