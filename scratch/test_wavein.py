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

wfx = WAVEFORMATEX()
wfx.wFormatTag = 1 # PCM
wfx.nChannels = 1
wfx.nSamplesPerSec = 8000
wfx.nAvgBytesPerSec = 16000
wfx.nBlockAlign = 2
wfx.wBitsPerSample = 16
wfx.cbSize = 0

hWaveIn = wintypes.HANDLE()
res = winmm.waveInOpen(ctypes.byref(hWaveIn), -1, ctypes.byref(wfx), 0, 0, 0)
print('waveInOpen result:', res)

NUM_BUFS = 4
BUF_SIZE = 320 # 20 ms at 8000 Hz 16-bit mono

raw_buffers = [ctypes.create_string_buffer(BUF_SIZE) for _ in range(NUM_BUFS)]
hdrs = [WAVEHDR() for _ in range(NUM_BUFS)]

for i in range(NUM_BUFS):
    hdrs[i].lpData = ctypes.cast(raw_buffers[i], ctypes.c_char_p)
    hdrs[i].dwBufferLength = BUF_SIZE
    hdrs[i].dwFlags = 0
    res = winmm.waveInPrepareHeader(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))
    res = winmm.waveInAddBuffer(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))

res = winmm.waveInStart(hWaveIn)
print('waveInStart result:', res)

start = time.time()
buffers_received = 0
while time.time() - start < 0.5: # record for 500 ms
    for i in range(NUM_BUFS):
        if hdrs[i].dwFlags & 1: # WHDR_DONE
            buffers_received += 1
            # re-add
            winmm.waveInAddBuffer(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))
    time.sleep(0.01)

winmm.waveInStop(hWaveIn)
winmm.waveInReset(hWaveIn)
for i in range(NUM_BUFS):
    winmm.waveInUnprepareHeader(hWaveIn, ctypes.byref(hdrs[i]), ctypes.sizeof(WAVEHDR))
winmm.waveInClose(hWaveIn)

print(f'Done! Received {buffers_received} buffers in 500 ms (expected ~25 buffers)')
