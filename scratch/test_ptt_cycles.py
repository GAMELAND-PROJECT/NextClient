import ctypes
from ctypes import wintypes
import time

winmm = ctypes.windll.winmm

class WAVEFORMATEX(ctypes.Structure):
    _fields_ = [
        ('wFormatTag', wintypes.WORD),
        ('nChannels', wintypes.WORD),
        ('nSamplesPerSec', wintypes.DWORD),
        ('nAvgBytesPerSec', wintypes.DWORD),
        ('nBlockAlign', wintypes.WORD),
        ('wBitsPerSample', wintypes.WORD),
        ('cbSize', wintypes.WORD),
    ]

class WAVEHDR(ctypes.Structure):
    _fields_ = [
        ('lpData', ctypes.c_char_p),
        ('dwBufferLength', wintypes.DWORD),
        ('dwBytesRecorded', wintypes.DWORD),
        ('dwUser', ctypes.c_void_p),
        ('dwFlags', wintypes.DWORD),
        ('dwLoops', wintypes.DWORD),
        ('lpNext', ctypes.c_void_p),
        ('reserved', ctypes.c_void_p),
    ]

wfx = WAVEFORMATEX(1, 1, 8000, 16000, 2, 16, 0)
hWaveIn = wintypes.HANDLE()
winmm.waveInOpen(ctypes.byref(hWaveIn), -1, ctypes.byref(wfx), 0, 0, 0)

NUM_BUFS = 4
BUF_SIZE = 320
raw_buffers = [ctypes.create_string_buffer(BUF_SIZE) for _ in range(NUM_BUFS)]
hdrs = [WAVEHDR() for _ in range(NUM_BUFS)]

for i in range(NUM_BUFS):
    hdrs[i].lpData = ctypes.cast(raw_buffers[i], ctypes.c_char_p)
    hdrs[i].dwBufferLength = BUF_SIZE
    hdrs[i].dwFlags = 0
    winmm.waveInPrepareHeader(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))
    winmm.waveInAddBuffer(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))

for cycle in range(3):
    winmm.waveInStart(hWaveIn)
    rec_count = 0
    start = time.time()
    while time.time() - start < 0.1: # 100 ms
        for i in range(NUM_BUFS):
            if hdrs[i].dwFlags & 1:
                rec_count += 1
                winmm.waveInAddBuffer(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))
        time.sleep(0.01)
    winmm.waveInStop(hWaveIn)
    print(f'Cycle {cycle+1}: recorded {rec_count} buffers in 100 ms')
    time.sleep(0.05)

winmm.waveInReset(hWaveIn)
for i in range(NUM_BUFS):
    winmm.waveInUnprepareHeader(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))
winmm.waveInClose(hWaveIn)
print('PTT cycle test passed successfully!')
