import ctypes
from ctypes import wintypes

winmm = ctypes.windll.winmm

num_devs = winmm.waveInGetNumDevs()
print('Number of waveIn devices:', num_devs)

class WAVEINCAPSW(ctypes.Structure):
    _fields_ = [
        ('wMid', wintypes.WORD),
        ('wPid', wintypes.WORD),
        ('vDriverVersion', wintypes.DWORD),
        ('szPname', wintypes.WCHAR * 32),
        ('dwFormats', wintypes.DWORD),
        ('wChannels', wintypes.WORD),
        ('wReserved1', wintypes.WORD),
    ]

for i in range(num_devs):
    caps = WAVEINCAPSW()
    res = winmm.waveInGetDevCapsW(i, ctypes.byref(caps), ctypes.sizeof(caps))
    print(f'Device {i}: "{caps.szPname}", channels={caps.wChannels}, formats={hex(caps.dwFormats)}')
