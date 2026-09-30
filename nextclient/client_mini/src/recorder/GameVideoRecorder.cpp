#include "GameVideoRecorder.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <direct.h>
#include <io.h>
#include <cmath>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <gl/GL.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#define GL_PIXEL_PACK_BUFFER_ARB 0x88EB
#define GL_STREAM_READ_ARB       0x88E1
#define GL_READ_ONLY_ARB         0x88B8

typedef void (APIENTRY * PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY * PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRY * PFNGLBUFFERDATAARBPROC) (GLenum target, ptrdiff_t size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRY * PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRY * PFNGLUNMAPBUFFERARBPROC) (GLenum target);
typedef void (APIENTRY * PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);

static PFNGLGENBUFFERSARBPROC pfnGenBuffers = nullptr;
static PFNGLBINDBUFFERARBPROC pfnBindBuffer = nullptr;
static PFNGLBUFFERDATAARBPROC pfnBufferData = nullptr;
static PFNGLMAPBUFFERARBPROC pfnMapBuffer = nullptr;
static PFNGLUNMAPBUFFERARBPROC pfnUnmapBuffer = nullptr;
static PFNGLDELETEBUFFERSARBPROC pfnDeleteBuffers = nullptr;
static bool s_pboProcsLoaded = false;

static void LoadPboProcedures()
{
    if (s_pboProcsLoaded) return;
    s_pboProcsLoaded = true;

    pfnGenBuffers = (PFNGLGENBUFFERSARBPROC)wglGetProcAddress("glGenBuffersARB");
    if (!pfnGenBuffers) pfnGenBuffers = (PFNGLGENBUFFERSARBPROC)wglGetProcAddress("glGenBuffers");

    pfnBindBuffer = (PFNGLBINDBUFFERARBPROC)wglGetProcAddress("glBindBufferARB");
    if (!pfnBindBuffer) pfnBindBuffer = (PFNGLBINDBUFFERARBPROC)wglGetProcAddress("glBindBuffer");

    pfnBufferData = (PFNGLBUFFERDATAARBPROC)wglGetProcAddress("glBufferDataARB");
    if (!pfnBufferData) pfnBufferData = (PFNGLBUFFERDATAARBPROC)wglGetProcAddress("glBufferData");

    pfnMapBuffer = (PFNGLMAPBUFFERARBPROC)wglGetProcAddress("glMapBufferARB");
    if (!pfnMapBuffer) pfnMapBuffer = (PFNGLMAPBUFFERARBPROC)wglGetProcAddress("glMapBuffer");

    pfnUnmapBuffer = (PFNGLUNMAPBUFFERARBPROC)wglGetProcAddress("glUnmapBufferARB");
    if (!pfnUnmapBuffer) pfnUnmapBuffer = (PFNGLUNMAPBUFFERARBPROC)wglGetProcAddress("glUnmapBuffer");

    pfnDeleteBuffers = (PFNGLDELETEBUFFERSARBPROC)wglGetProcAddress("glDeleteBuffersARB");
    if (!pfnDeleteBuffers) pfnDeleteBuffers = (PFNGLDELETEBUFFERSARBPROC)wglGetProcAddress("glDeleteBuffers");
}

static bool IsPboAvailable()
{
    LoadPboProcedures();
    return (pfnGenBuffers && pfnBindBuffer && pfnBufferData && pfnMapBuffer && pfnUnmapBuffer && pfnDeleteBuffers);
}

// Isolated C-style SEH wrapper without any C++ object unwinding (prevents MSVC C2712)
static bool SafeGlReadPixels(int width, int height, uint8_t* pDest)
{
    __try
    {
        glPixelStorei(GL_PACK_ALIGNMENT, ((width * 3) % 4 == 0) ? 4 : 1);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pDest);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
#endif

namespace nextclient::client_mini
{
    struct WavHeader
    {
        char riff[4]{'R', 'I', 'F', 'F'};
        uint32_t fileSize{0};
        char wave[4]{'W', 'A', 'V', 'E'};
        char fmt[4]{'f', 'm', 't', ' '};
        uint32_t fmtSize{16};
        uint16_t audioFormat{1}; // PCM
        uint16_t numChannels{2};
        uint32_t sampleRate{48000};
        uint32_t byteRate{192000};
        uint16_t blockAlign{4};
        uint16_t bitsPerSample{16};
        char data[4]{'d', 'a', 't', 'a'};
        uint32_t dataSize{0};
    };

    GameVideoRecorder& GameVideoRecorder::Instance()
    {
        static GameVideoRecorder s_instance;
        return s_instance;
    }

    GameVideoRecorder::GameVideoRecorder() = default;

    void GameVideoRecorder::InitPbo(int width, int height)
    {
#ifdef _WIN32
        if (!IsPboAvailable())
        {
            m_pboSupported = false;
            m_pboInitialized = false;
            return;
        }

        if (m_pboInitialized && m_pboWidth == width && m_pboHeight == height)
            return;

        CleanupPbo();

        const size_t frameSize = static_cast<size_t>(width) * height * 3;
        pfnGenBuffers(2, m_pboIds);

        for (int i = 0; i < 2; ++i)
        {
            pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pboIds[i]);
            pfnBufferData(GL_PIXEL_PACK_BUFFER_ARB, frameSize, nullptr, GL_STREAM_READ_ARB);
        }
        pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

        m_pboIndex = 0;
        m_pboWidth = width;
        m_pboHeight = height;
        m_pboSupported = true;
        m_pboInitialized = true;
#endif
    }

    void GameVideoRecorder::CleanupPbo()
    {
#ifdef _WIN32
        if (m_pboInitialized)
        {
            if (pfnBindBuffer)
                pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
            if (pfnDeleteBuffers)
                pfnDeleteBuffers(2, m_pboIds);
            m_pboIds[0] = 0;
            m_pboIds[1] = 0;
            m_pboInitialized = false;
            m_pboSupported = false;
            m_pboWidth = 0;
            m_pboHeight = 0;
            m_pboIndex = 0;
        }
#endif
    }

    GameVideoRecorder::~GameVideoRecorder()
    {
        DiscardHighlight();
        FinishStudioRender();
        StopWorkerCapture();
        CancelConversion();
    }

    // -------------------------------------------------------------
    // In-Match Zero-Lag Demo Recording
    // -------------------------------------------------------------
    bool GameVideoRecorder::StartMatchDemo(const std::string& baseFileName)
    {
        _mkdir("demos");
        m_currentDemoFileName = baseFileName + ".dem";
        m_matchDemoStartTime = std::chrono::steady_clock::now();
        m_isMatchDemoRecording = true;
        return true;
    }

    void GameVideoRecorder::StopMatchDemo()
    {
        m_isMatchDemoRecording = false;
    }

    std::string GameVideoRecorder::GetFormattedDemoTime() const
    {
        if (!m_isMatchDemoRecording.load())
            return "00:00";

        const auto now = std::chrono::steady_clock::now();
        const int totalSeconds = static_cast<int>(std::chrono::duration<double>(now - m_matchDemoStartTime).count());
        const int minutes = totalSeconds / 60;
        const int seconds = totalSeconds % 60;

        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);
        return buf;
    }

    // -------------------------------------------------------------
    // Bookmark & HLAE Studio Render (Keys 1 & 2 only)
    // -------------------------------------------------------------
    static void* GetDemoPlayerInterface()
    {
        typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
        static void* s_pDemoPlayer = nullptr;

        if (s_pDemoPlayer == nullptr)
        {
            HMODULE hModule = GetModuleHandleA("demoplayer.dll");
            if (hModule != nullptr)
            {
                auto pfnCreate = (CreateInterfaceFn)GetProcAddress(hModule, "CreateInterface");
                if (pfnCreate != nullptr)
                {
                    int retCode = 0;
                    s_pDemoPlayer = pfnCreate("demoplayer001", &retCode);
                }
            }
        }
        return s_pDemoPlayer;
    }

    double GameVideoRecorder::GetExactDemoTime()
    {
        void* pDemoPlayer = GetDemoPlayerInterface();
        if (pDemoPlayer != nullptr)
        {
            void** vtable = *(void***)pDemoPlayer;
            if (vtable != nullptr && vtable[34] != nullptr)
            {
                typedef double (__thiscall *GetDemoTimeFn)(void* thisPtr);
                GetDemoTimeFn pfnGetDemoTime = (GetDemoTimeFn)vtable[34];
                const double t = pfnGetDemoTime(pDemoPlayer);
                if (t >= 0.0) return t;
            }
        }
        return 0.0;
    }

    double GameVideoRecorder::GetDemoStartTime()
    {
        void* pDemoPlayer = GetDemoPlayerInterface();
        if (pDemoPlayer != nullptr)
        {
            void** vtable = *(void***)pDemoPlayer;
            if (vtable != nullptr && vtable[35] != nullptr)
            {
                typedef double (__thiscall *GetStartTimeFn)(void* thisPtr);
                GetStartTimeFn pfnGetStartTime = (GetStartTimeFn)vtable[35];
                return pfnGetStartTime(pDemoPlayer);
            }
        }
        return 0.0;
    }

    double GameVideoRecorder::GetDemoEndTime()
    {
        void* pDemoPlayer = GetDemoPlayerInterface();
        if (pDemoPlayer != nullptr)
        {
            void** vtable = *(void***)pDemoPlayer;
            if (vtable != nullptr && vtable[36] != nullptr)
            {
                typedef double (__thiscall *GetEndTimeFn)(void* thisPtr);
                GetEndTimeFn pfnGetEndTime = (GetEndTimeFn)vtable[36];
                return pfnGetEndTime(pDemoPlayer);
            }
        }
        return 0.0;
    }

    bool GameVideoRecorder::SetDemoWorldTime(double time, bool relative)
    {
        void* pDemoPlayer = GetDemoPlayerInterface();
        if (pDemoPlayer != nullptr)
        {
            void** vtable = *(void***)pDemoPlayer;
            if (vtable != nullptr && vtable[22] != nullptr)
            {
                typedef void (__thiscall *SetWorldTimeFn)(void* thisPtr, double timeVal, bool isRelative);
                SetWorldTimeFn pfnSetWorldTime = (SetWorldTimeFn)vtable[22];
                pfnSetWorldTime(pDemoPlayer, time, relative);
                return true;
            }
        }
        return false;
    }

    bool GameVideoRecorder::SetDemoPaused(bool paused)
    {
        void* pDemoPlayer = GetDemoPlayerInterface();
        if (pDemoPlayer != nullptr)
        {
            void** vtable = *(void***)pDemoPlayer;
            if (vtable != nullptr && vtable[24] != nullptr)
            {
                typedef void (__thiscall *SetPausedFn)(void* thisPtr, bool state);
                SetPausedFn pfnSetPaused = (SetPausedFn)vtable[24];
                pfnSetPaused(pDemoPlayer, paused);
                return true;
            }
        }
        return false;
    }

    void GameVideoRecorder::MarkIn(float demoTime)
    {
        _mkdir("videos");

        // Stop any residual audio thread
        m_stopAudioRequested = true;
        if (m_audioRecordThread.joinable())
            m_audioRecordThread.join();

        m_markInTime = demoTime;
        m_markOutTime = demoTime;
        m_highlightClipInfo.clear();

        const DWORD pid = GetCurrentProcessId();
        const DWORD tick = GetTickCount();

        m_tempAudioPath = "videos/temp_audio_" + std::to_string(pid) + "_" + std::to_string(tick) + ".wav";
        DeleteFileA(m_tempAudioPath.c_str());

        // Start recording game audio while user watches the clip
        m_stopAudioRequested = false;
        m_audioRecordThread = std::thread(&GameVideoRecorder::AudioRecordingThread, this, m_tempAudioPath, QuerySystemAudioSampleRate());

        m_highlightState = HighlightState::Marking;
    }

    bool GameVideoRecorder::MarkOut(float demoTime)
    {
        if (m_highlightState.load() != HighlightState::Marking)
            return false;

        m_markOutTime = demoTime;
        const float dur = m_markOutTime - m_markInTime;
        if (dur < 0.5f)
        {
            DiscardHighlight();
            return false;
        }

        // Stop audio recording and finalize temp WAV file
        m_stopAudioRequested = true;
        if (m_audioRecordThread.joinable())
            m_audioRecordThread.join();

        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "Duration: %.1fs  |  Range: %s -> %s  |  1080p 60 FPS",
            dur, GetFormattedTime(m_markInTime).c_str(), GetFormattedTime(m_markOutTime).c_str());
        m_highlightClipInfo = buf;

        m_highlightState = HighlightState::AwaitingConfirm;
        return true;
    }

    void GameVideoRecorder::DiscardHighlight()
    {
        m_stopAudioRequested = true;
        if (m_audioRecordThread.joinable())
            m_audioRecordThread.join();

        if (!m_tempAudioPath.empty())
        {
            DeleteFileA(m_tempAudioPath.c_str());
        }
        if (!m_tempVideoPath.empty())
        {
            DeleteFileA(m_tempVideoPath.c_str());
        }

        m_markInTime = 0.0f;
        m_markOutTime = 0.0f;
        m_highlightClipInfo.clear();
        m_highlightState = HighlightState::Idle;
    }

    bool GameVideoRecorder::StartStudioRender(const std::string& demoOrMapName, int width, int height)
    {
        if (m_highlightState.load() == HighlightState::Rendering)
            return false;

        const float dur = m_markOutTime - m_markInTime;
        if (dur <= 0.0f)
            return false;

        _mkdir("videos");

        m_recordWidth = width;
        m_recordHeight = height;
        m_renderTargetFrames = static_cast<uint64_t>(std::ceil(dur * 60.0f));
        m_renderFramesPushed = 0;
        m_isFirstRenderFrame = true;

        std::time_t now = std::time(nullptr);
        std::tm lt{};
        localtime_s(&lt, &now);

        char stamp[64]{};
        std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d_%02d-%02d-%02d",
            lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);

        std::string safeName = demoOrMapName;
        const size_t dot = safeName.find_last_of('.');
        if (dot != std::string::npos) safeName = safeName.substr(0, dot);
        for (char& c : safeName)
        {
            if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_';
        }
        if (safeName.empty()) safeName = "Highlight";

        m_lastSavedHighlightPath = "videos/Highlight_" + safeName + "_" + stamp + "_1080p.mp4";

        const DWORD pid = GetCurrentProcessId();
        const DWORD tick = GetTickCount();

        m_tempVideoPath = "videos/temp_video_" + std::to_string(pid) + "_" + std::to_string(tick) + ".mp4";
        DeleteFileA(m_tempVideoPath.c_str());

#ifdef _WIN32
        const std::string videoPipeName = "\\\\.\\pipe\\gl_studio_vid_" + std::to_string(pid) + "_" + std::to_string(tick);
        HANDLE hVideoPipe = CreateNamedPipeA(
            videoPipeName.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            1,
            1024 * 1024 * 4,
            1024 * 1024 * 4,
            0,
            nullptr
        );

        if (hVideoPipe == INVALID_HANDLE_VALUE)
        {
            m_highlightState = HighlightState::Idle;
            return false;
        }

        m_hVideoPipe = hVideoPipe;

        const std::string ffmpegPath = FindFfmpegExecutable();

        std::string videoFilter = "vflip";
        if (height < 1080)
        {
            videoFilter = "vflip,scale=-2:1080:flags=lanczos";
        }

        std::ostringstream cmd;
        cmd << "\"" << ffmpegPath << "\" -y -hide_banner -loglevel error"
            << " -f rawvideo -pix_fmt rgb24 -s " << width << "x" << height
            << " -r 60 -i \"" << videoPipeName << "\""
            << " -vf " << videoFilter
            << " -c:v libx264 -preset veryfast -crf 16 -pix_fmt yuv420p"
            << " -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv"
            << " -movflags +faststart \"" << m_tempVideoPath << "\"";

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};
        std::string cmdStr = cmd.str();

        BOOL created = CreateProcessA(
            nullptr,
            cmdStr.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!created)
        {
            CloseHandle(m_hVideoPipe);
            m_hVideoPipe = INVALID_HANDLE_VALUE;
            m_highlightState = HighlightState::Idle;
            return false;
        }

        m_hFfmpegProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        ConnectNamedPipe(m_hVideoPipe, nullptr);

        const size_t frameSize = static_cast<size_t>(width) * height * 3;
        m_preallocatedCaptureBuffer.resize(frameSize, 0);

        m_highlightState = HighlightState::Rendering;
        return true;
#else
        return false;
#endif
    }

    void GameVideoRecorder::CaptureStudioFrame(int width, int height)
    {
#ifdef _WIN32
        if (m_highlightState.load() != HighlightState::Rendering)
            return;

        if (m_hVideoPipe == INVALID_HANDLE_VALUE)
            return;

        if (m_renderFramesPushed.load() >= m_renderTargetFrames)
            return;

        // HLAE FS_STARTING: Drop frame 0 to let scene and camera matrix settle
        if (m_isFirstRenderFrame)
        {
            m_isFirstRenderFrame = false;
            return;
        }

        const size_t frameSize = static_cast<size_t>(width) * height * 3;
        if (m_preallocatedCaptureBuffer.size() != frameSize)
            m_preallocatedCaptureBuffer.resize(frameSize, 0);

        if (!m_pboInitialized || m_pboWidth != width || m_pboHeight != height)
        {
            InitPbo(width, height);
        }

        bool captured = false;
        if (m_pboSupported && m_pboInitialized)
        {
            int nextIndex = (m_pboIndex + 1) % 2;

            pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pboIds[nextIndex]);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, 0);

            pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pboIds[m_pboIndex]);
            void* pGpuData = pfnMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
            if (pGpuData != nullptr)
            {
                memcpy(m_preallocatedCaptureBuffer.data(), pGpuData, frameSize);
                pfnUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
                captured = true;
            }
            pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

            m_pboIndex = nextIndex;
        }

        if (!captured)
        {
            if (!SafeGlReadPixels(width, height, m_preallocatedCaptureBuffer.data()))
                return;
        }

        DWORD written = 0;
        WriteFile(m_hVideoPipe, m_preallocatedCaptureBuffer.data(), static_cast<DWORD>(frameSize), &written, nullptr);
        m_renderFramesPushed.fetch_add(1);
#endif
    }

    void GameVideoRecorder::FinishStudioRender()
    {
#ifdef _WIN32
        if (m_highlightState.load() != HighlightState::Rendering)
            return;

        // Close video pipe so FFmpeg finishes the temp video
        if (m_hVideoPipe != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(m_hVideoPipe);
            DisconnectNamedPipe(m_hVideoPipe);
            CloseHandle(m_hVideoPipe);
            m_hVideoPipe = INVALID_HANDLE_VALUE;
        }

        if (m_hFfmpegProcess != nullptr)
        {
            WaitForSingleObject(m_hFfmpegProcess, 10000);
            CloseHandle(m_hFfmpegProcess);
            m_hFfmpegProcess = nullptr;
        }

        CleanupPbo();

        // Mux video and audio into final output with stream copy (~150ms)
        const std::string ffmpegPath = FindFfmpegExecutable();
        bool hasAudio = false;
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExA(m_tempAudioPath.c_str(), GetFileExInfoStandard, &fad) && fad.nFileSizeLow > 44)
        {
            hasAudio = true;
        }

        std::ostringstream muxCmd;
        muxCmd << "\"" << ffmpegPath << "\" -y -hide_banner -loglevel error"
               << " -i \"" << m_tempVideoPath << "\"";

        if (hasAudio)
        {
            muxCmd << " -i \"" << m_tempAudioPath << "\" -c:v copy -c:a aac -b:a 192k -shortest";
        }
        else
        {
            muxCmd << " -c:v copy";
        }

        muxCmd << " -movflags +faststart \"" << m_lastSavedHighlightPath << "\"";

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};
        std::string cmdStr = muxCmd.str();

        if (CreateProcessA(nullptr, cmdStr.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 15000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        DeleteFileA(m_tempVideoPath.c_str());
        DeleteFileA(m_tempAudioPath.c_str());

        m_highlightState = HighlightState::Idle;
        m_markInTime = 0.0f;
        m_markOutTime = 0.0f;
#endif
    }

    void GameVideoRecorder::AudioRecordingThread(std::string wavPath, int sampleRate)
    {
#ifdef _WIN32
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool comInit = SUCCEEDED(hr);

        IMMDeviceEnumerator* pEnumerator = nullptr;
        IMMDevice* pDevice = nullptr;
        IAudioClient* pAudioClient = nullptr;
        IAudioCaptureClient* pCaptureClient = nullptr;
        WAVEFORMATEX* pwfx = nullptr;
        bool captureReady = false;

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (SUCCEEDED(hr) && pEnumerator != nullptr)
        {
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (SUCCEEDED(hr) && pDevice != nullptr)
            {
                hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
                if (SUCCEEDED(hr) && pAudioClient != nullptr)
                {
                    hr = pAudioClient->GetMixFormat(&pwfx);
                    if (SUCCEEDED(hr) && pwfx != nullptr)
                    {
                        hr = pAudioClient->Initialize(
                            AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_LOOPBACK,
                            10000000,
                            0,
                            pwfx,
                            nullptr
                        );

                        if (SUCCEEDED(hr))
                        {
                            hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
                            if (SUCCEEDED(hr) && pCaptureClient != nullptr)
                            {
                                hr = pAudioClient->Start();
                                if (SUCCEEDED(hr))
                                {
                                    captureReady = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        std::ofstream wav(wavPath, std::ios::binary);
        WavHeader hdr;
        hdr.sampleRate = static_cast<uint32_t>(sampleRate);
        hdr.byteRate = hdr.sampleRate * 2 * sizeof(int16_t);
        hdr.blockAlign = 2 * sizeof(int16_t);
        wav.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

        uint32_t totalBytesWritten = 0;
        std::vector<int16_t> pcmConversionBuffer;

        while (!m_stopAudioRequested.load())
        {
            if (captureReady && pCaptureClient != nullptr)
            {
                UINT32 packetLength = 0;
                hr = pCaptureClient->GetNextPacketSize(&packetLength);
                if (SUCCEEDED(hr) && packetLength > 0)
                {
                    BYTE* pData = nullptr;
                    UINT32 numFramesRead = 0;
                    DWORD flags = 0;

                    hr = pCaptureClient->GetBuffer(&pData, &numFramesRead, &flags, nullptr, nullptr);
                    if (SUCCEEDED(hr) && pData != nullptr && numFramesRead > 0)
                    {
                        const int channels = (pwfx != nullptr && pwfx->nChannels > 0) ? pwfx->nChannels : 2;
                        const size_t totalSamples = static_cast<size_t>(numFramesRead) * channels;
                        pcmConversionBuffer.resize(totalSamples);

                        if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                        {
                            std::fill(pcmConversionBuffer.begin(), pcmConversionBuffer.end(), 0);
                        }
                        else if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                                 (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pwfx->wBitsPerSample == 32))
                        {
                            const float* fData = reinterpret_cast<const float*>(pData);
                            for (size_t i = 0; i < totalSamples; ++i)
                            {
                                const float sample = std::clamp(fData[i], -1.0f, 1.0f);
                                pcmConversionBuffer[i] = static_cast<int16_t>(sample * 32767.0f);
                            }
                        }
                        else if (pwfx->wBitsPerSample == 16)
                        {
                            std::memcpy(pcmConversionBuffer.data(), pData, totalSamples * sizeof(int16_t));
                        }

                        const uint32_t bytesToWrite = static_cast<uint32_t>(totalSamples * sizeof(int16_t));
                        wav.write(reinterpret_cast<const char*>(pcmConversionBuffer.data()), bytesToWrite);
                        totalBytesWritten += bytesToWrite;

                        pCaptureClient->ReleaseBuffer(numFramesRead);
                        continue;
                    }
                }
            }
            else
            {
                const size_t silenceFrames = static_cast<size_t>(sampleRate) / 100;
                std::vector<int16_t> silence(silenceFrames * 2, 0);
                const uint32_t bytesToWrite = static_cast<uint32_t>(silence.size() * sizeof(int16_t));
                wav.write(reinterpret_cast<const char*>(silence.data()), bytesToWrite);
                totalBytesWritten += bytesToWrite;
            }

            Sleep(3);
        }

        if (captureReady && pAudioClient != nullptr)
            pAudioClient->Stop();

        if (pwfx != nullptr) CoTaskMemFree(pwfx);
        if (pCaptureClient != nullptr) pCaptureClient->Release();
        if (pAudioClient != nullptr) pAudioClient->Release();
        if (pDevice != nullptr) pDevice->Release();
        if (pEnumerator != nullptr) pEnumerator->Release();

        if (comInit)
            CoUninitialize();

        hdr.fileSize = totalBytesWritten + 36;
        hdr.dataSize = totalBytesWritten;
        wav.seekp(0, std::ios::beg);
        wav.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        wav.close();
#endif
    }

    std::string GameVideoRecorder::GetFormattedTime(float seconds) const
    {
        if (seconds < 0.0f) seconds = 0.0f;
        const int totalSec = static_cast<int>(seconds);
        const int mins = totalSec / 60;
        const int secs = totalSec % 60;
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        return buf;
    }

    std::string GameVideoRecorder::GetHighlightRangeFormatted() const
    {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "%s -> %s (%.1fs)",
            GetFormattedTime(m_markInTime).c_str(),
            GetFormattedTime(m_markOutTime).c_str(),
            GetHighlightDuration());
        return buf;
    }

    int GameVideoRecorder::GetRenderProgressPercent() const
    {
        if (m_renderTargetFrames == 0) return 0;
        const int pct = static_cast<int>((m_renderFramesPushed.load() * 100) / m_renderTargetFrames);
        return std::clamp(pct, 0, 100);
    }
// -------------------------------------------------------------
    // In-Lobby Match Demo Studio
    // -------------------------------------------------------------
    std::vector<DemoFileItem> GameVideoRecorder::RefreshDemoList()
    {
        m_cachedDemos.clear();

#ifdef _WIN32
        WIN32_FIND_DATAA fd{};
        HANDLE hFind = FindFirstFileA("demos\\*.dem", &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    DemoFileItem item;
                    item.fileName = fd.cFileName;

                    std::string f(fd.cFileName);
                    const size_t firstUnderscore = f.find('_');
                    if (firstUnderscore != std::string::npos)
                    {
                        const size_t secondUnderscore = f.find('_', firstUnderscore + 1);
                        if (secondUnderscore != std::string::npos)
                        {
                            item.mapName = f.substr(firstUnderscore + 1, secondUnderscore - firstUnderscore - 1);
                        }
                    }
                    if (item.mapName.empty())
                    {
                        item.mapName = f.substr(0, f.find_last_of('.'));
                    }

                    const uint64_t sizeBytes = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                    if (sizeBytes >= 1024 * 1024)
                    {
                        const double mb = static_cast<double>(sizeBytes) / (1024.0 * 1024.0);
                        char sbuf[32]{};
                        std::snprintf(sbuf, sizeof(sbuf), "%.1f MB", mb);
                        item.sizeFormatted = sbuf;
                    }
                    else
                    {
                        const double kb = static_cast<double>(sizeBytes) / 1024.0;
                        char sbuf[32]{};
                        std::snprintf(sbuf, sizeof(sbuf), "%.0f KB", kb);
                        item.sizeFormatted = sbuf;
                    }

                    FILETIME ft = fd.ftLastWriteTime;
                    SYSTEMTIME st{};
                    FileTimeToSystemTime(&ft, &st);
                    char dbuf[32]{};
                    std::snprintf(dbuf, sizeof(dbuf), "%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
                    item.dateFormatted = dbuf;

                    item.timestamp = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;

                    m_cachedDemos.push_back(std::move(item));
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }

        std::sort(m_cachedDemos.begin(), m_cachedDemos.end(), [](const DemoFileItem& a, const DemoFileItem& b) {
            return a.timestamp > b.timestamp;
        });
#endif
        return m_cachedDemos;
    }

    std::string GameVideoRecorder::FindFfmpegExecutable() const
    {
#ifdef _WIN32
        char exePath[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0)
        {
            char* lastSlash = strrchr(exePath, '\\');
            if (lastSlash != nullptr)
            {
                *(lastSlash + 1) = '\0';
                std::string localCandidate = std::string(exePath) + "ffmpeg.exe";
                if (_access(localCandidate.c_str(), 0) == 0)
                    return localCandidate;
            }
        }
#endif
        const char* candidatePaths[] = {
            "ffmpeg.exe",
            "..\\ffmpeg.exe",
            "cstrike\\..\\ffmpeg.exe",
            "F:\\Allclient\\ffmpeg.exe",
            "D:\\Allclient\\ffmpeg.exe"
        };
        for (const auto* path : candidatePaths)
        {
            if (_access(path, 0) == 0)
                return path;
        }
        return "ffmpeg.exe";
    }

    int GameVideoRecorder::QuerySystemAudioSampleRate() const
    {
#ifdef _WIN32
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool comInit = SUCCEEDED(hr);

        IMMDeviceEnumerator* pEnumerator = nullptr;
        IMMDevice* pDevice = nullptr;
        IAudioClient* pAudioClient = nullptr;
        WAVEFORMATEX* pwfx = nullptr;
        int detectedRate = 48000;

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (SUCCEEDED(hr) && pEnumerator != nullptr)
        {
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (SUCCEEDED(hr) && pDevice != nullptr)
            {
                hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
                if (SUCCEEDED(hr) && pAudioClient != nullptr)
                {
                    hr = pAudioClient->GetMixFormat(&pwfx);
                    if (SUCCEEDED(hr) && pwfx != nullptr)
                    {
                        if (pwfx->nSamplesPerSec >= 22050 && pwfx->nSamplesPerSec <= 192000)
                            detectedRate = static_cast<int>(pwfx->nSamplesPerSec);
                        CoTaskMemFree(pwfx);
                    }
                    pAudioClient->Release();
                }
                pDevice->Release();
            }
            pEnumerator->Release();
        }

        if (comInit)
            CoUninitialize();

        return detectedRate;
#else
        return 48000;
#endif
    }

    bool GameVideoRecorder::StartDemoConversion(const std::string& demoFileName, int targetWidth, int targetHeight, int fps)
    {
        if (m_isConverting.load())
            return false;

        _mkdir("videos");

        std::string baseName = demoFileName;
        const size_t dot = baseName.find_last_of('.');
        if (dot != std::string::npos)
            baseName = baseName.substr(0, dot);

        m_convertingDemoName = demoFileName;
        m_lastConvertedVideoPath = "videos/" + baseName + ".mp4";
        m_convertPercent = 5;
        m_isConverting = true;

#ifdef _WIN32
        char exePath[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        std::string runnerPath(exePath);
        const size_t lastSlash = runnerPath.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            std::string folder = runnerPath.substr(0, lastSlash + 1);
            std::string csExe = folder + "cstrike.exe";
            DWORD attr = GetFileAttributesA(csExe.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                runnerPath = csExe;
        }

        std::ostringstream cmd;
        cmd << "\"" << runnerPath << "\" -game cstrike -sw -noborder -windowed -width " << targetWidth << " -height " << targetHeight
            << " -novid -novideosettings +viewdemo \"demos/" << demoFileName << "\" -democonvert";

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};
        std::string cmdStr = cmd.str();

        BOOL created = CreateProcessA(
            nullptr,
            cmdStr.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!created)
        {
            m_isConverting = false;
            m_convertPercent = 0;
            return false;
        }

        m_hConversionProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        if (m_conversionMonitorThread.joinable())
            m_conversionMonitorThread.detach();

        m_conversionMonitorThread = std::thread(&GameVideoRecorder::MonitorConversionThread, this, pi.hProcess, m_lastConvertedVideoPath);
#endif
        return true;
    }

    void GameVideoRecorder::MonitorConversionThread(HANDLE hProcess, std::string outputPath)
    {
#ifdef _WIN32
        int fakeProgress = 10;
        while (true)
        {
            const DWORD waitRes = WaitForSingleObject(hProcess, 500);
            if (waitRes == WAIT_OBJECT_0)
            {
                break;
            }

            if (fakeProgress < 95)
            {
                fakeProgress += 3;
                m_convertPercent = fakeProgress;
            }
        }

        CloseHandle(hProcess);
        m_hConversionProcess = nullptr;
        m_convertPercent = 100;
        m_isConverting = false;
#endif
    }

    void GameVideoRecorder::CancelConversion()
    {
#ifdef _WIN32
        if (m_isConverting.load() && m_hConversionProcess != nullptr)
        {
            TerminateProcess(m_hConversionProcess, 0);
            CloseHandle(m_hConversionProcess);
            m_hConversionProcess = nullptr;
            m_isConverting = false;
            m_convertPercent = 0;
        }
#endif
    }

    // -------------------------------------------------------------
    // Dedicated Worker Capture Engine (Used inside -democonvert process)
    // -------------------------------------------------------------
    bool GameVideoRecorder::StartWorkerCapture(const std::string& outputBaseName, int width, int height, int fps)
    {
        if (m_isWorkerCapturing.load())
            return false;

        _mkdir("videos");

        m_recordWidth = width;
        m_recordHeight = height;
        m_targetFps = (fps > 0) ? fps : 60;
        m_framesPushed = 0;

        const std::string ffmpegPath = FindFfmpegExecutable();
        std::string outputPath = "videos/" + outputBaseName + ".mp4";

#ifdef _WIN32
        const DWORD pid = GetCurrentProcessId();
        const std::string videoPipeName = "\\\\.\\pipe\\gl_worker_vid_" + std::to_string(pid);
        const std::string audioPipeName = "\\\\.\\pipe\\gl_worker_aud_" + std::to_string(pid);

        HANDLE hVideoPipe = CreateNamedPipeA(
            videoPipeName.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            1,
            1024 * 1024 * 4,
            1024 * 1024 * 4,
            0,
            nullptr
        );

        HANDLE hAudioPipe = CreateNamedPipeA(
            audioPipeName.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            1,
            1024 * 1024,
            1024 * 1024,
            0,
            nullptr
        );

        if (hVideoPipe == INVALID_HANDLE_VALUE || hAudioPipe == INVALID_HANDLE_VALUE)
        {
            if (hVideoPipe != INVALID_HANDLE_VALUE) CloseHandle(hVideoPipe);
            if (hAudioPipe != INVALID_HANDLE_VALUE) CloseHandle(hAudioPipe);
            return false;
        }

        const size_t frameBytes = static_cast<size_t>(width) * height * 3;
        m_preallocatedCaptureBuffer.resize(frameBytes, 0);

        m_stopRequested = false;
        m_readyForFrames = false;
        m_isWorkerCapturing = true;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            std::queue<std::vector<uint8_t>> emptyQueue;
            std::swap(m_frameQueue, emptyQueue);
        }

        m_masterThread = std::thread(
            &GameVideoRecorder::MasterWorkerThread,
            this,
            ffmpegPath,
            outputPath,
            videoPipeName,
            audioPipeName,
            width,
            height,
            m_targetFps
        );

        return true;
#else
        return false;
#endif
    }

    void GameVideoRecorder::MasterWorkerThread(std::string ffmpegPath, std::string outputPath, std::string videoPipeName, std::string audioPipeName, int width, int height, int fps)
    {
#ifdef _WIN32
        HANDLE hVideoPipe = CreateFileA(
            videoPipeName.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        HANDLE hAudioPipe = CreateFileA(
            audioPipeName.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        m_hVideoPipe = hVideoPipe;
        m_hAudioPipe = hAudioPipe;

        const int audioRate = QuerySystemAudioSampleRate();

        std::string videoFilter = "vflip";
        if (height < 1080)
        {
            videoFilter = "vflip,scale=-2:1080:flags=lanczos";
        }

        std::ostringstream cmd;
        cmd << "\"" << ffmpegPath << "\" -y -hide_banner -loglevel error"
            << " -f rawvideo -pix_fmt rgb24 -s " << width << "x" << height
            << " -r " << fps << " -i \"" << videoPipeName << "\""
            << " -f s16le -ar " << audioRate << " -ac 2 -i \"" << audioPipeName << "\""
            << " -vf " << videoFilter
            << " -c:v libx264 -preset veryfast -crf 16 -pix_fmt yuv420p"
            << " -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv"
            << " -af aresample=async=1000:min_hard_comp=0.100000:first_pts=0"
            << " -c:a aac -b:a 192k"
            << " -movflags +faststart+frag_keyframe+empty_moov"
            << " \"" << outputPath << "\"";

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};
        std::string cmdStr = cmd.str();

        BOOL created = CreateProcessA(
            nullptr,
            cmdStr.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!created)
        {
            CloseHandle(hVideoPipe);
            CloseHandle(hAudioPipe);
            m_hVideoPipe = INVALID_HANDLE_VALUE;
            m_hAudioPipe = INVALID_HANDLE_VALUE;
            m_isWorkerCapturing = false;
            m_readyForFrames = false;
            return;
        }

        m_hFfmpegProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        ConnectNamedPipe(hVideoPipe, nullptr);

        const size_t probeFrameSize = static_cast<size_t>(width) * height * 3;
        std::vector<uint8_t> probeFrame(probeFrameSize, 0);
        DWORD probeWritten = 0;
        WriteFile(hVideoPipe, probeFrame.data(), static_cast<DWORD>(probeFrame.size()), &probeWritten, nullptr);

        ConnectNamedPipe(hAudioPipe, nullptr);

        const size_t probeAudioSamples = static_cast<size_t>(audioRate / fps);
        std::vector<int16_t> probeAudio(probeAudioSamples * 2, 0);
        WriteFile(hAudioPipe, probeAudio.data(), static_cast<DWORD>(probeAudio.size() * sizeof(int16_t)), &probeWritten, nullptr);

        m_audioThread = std::thread(&GameVideoRecorder::AudioWorkerThread, this, hAudioPipe, audioRate);

        m_syncStartTime = std::chrono::steady_clock::now();
        m_lastFrameTime = m_syncStartTime;
        m_framesPushed = 1;
        m_readyForFrames = true;

        while (!m_stopRequested.load())
        {
            std::vector<uint8_t> frame;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueCv.wait_for(lock, std::chrono::milliseconds(50), [this]() {
                    return !m_frameQueue.empty() || m_stopRequested.load();
                });

                if (m_frameQueue.empty())
                    continue;

                frame = std::move(m_frameQueue.front());
                m_frameQueue.pop();
            }

            if (!frame.empty())
            {
                DWORD written = 0;
                WriteFile(hVideoPipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr);
            }
        }

        while (true)
        {
            std::vector<uint8_t> frame;
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                if (m_frameQueue.empty())
                    break;
                frame = std::move(m_frameQueue.front());
                m_frameQueue.pop();
            }
            if (!frame.empty())
            {
                DWORD written = 0;
                WriteFile(hVideoPipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr);
            }
        }

        DisconnectNamedPipe(hVideoPipe);
        CloseHandle(hVideoPipe);
        m_hVideoPipe = INVALID_HANDLE_VALUE;

        if (m_audioThread.joinable())
            m_audioThread.join();

        if (m_hFfmpegProcess != nullptr)
        {
            WaitForSingleObject(m_hFfmpegProcess, 5000);
            CloseHandle(m_hFfmpegProcess);
            m_hFfmpegProcess = nullptr;
        }

        m_readyForFrames = false;
        m_isWorkerCapturing = false;
#endif
    }

    void GameVideoRecorder::WorkerCaptureFrame(int width, int height)
    {
#ifdef _WIN32
        if (!m_isWorkerCapturing.load() || !m_readyForFrames.load() || m_stopRequested.load())
            return;

        if (width != m_recordWidth || height != m_recordHeight)
            return;

        const auto now = std::chrono::steady_clock::now();
        const double elapsedSec = std::chrono::duration<double>(now - m_syncStartTime).count();
        const uint64_t targetTotalFrames = static_cast<uint64_t>(elapsedSec * m_targetFps);

        const uint64_t currentPushed = m_framesPushed.load();
        if (currentPushed >= targetTotalFrames)
            return;

        uint64_t framesNeeded = targetTotalFrames - currentPushed;
        if (framesNeeded > 5)
            framesNeeded = 5;

        const size_t frameSize = static_cast<size_t>(width) * height * 3;
        if (m_preallocatedCaptureBuffer.size() != frameSize)
            m_preallocatedCaptureBuffer.resize(frameSize, 0);

        if (!m_pboInitialized || m_pboWidth != width || m_pboHeight != height)
        {
            InitPbo(width, height);
        }

        bool captured = false;
        if (m_pboSupported && m_pboInitialized)
        {
            int nextIndex = (m_pboIndex + 1) % 2;

            pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pboIds[nextIndex]);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, 0);

            pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, m_pboIds[m_pboIndex]);
            void* pGpuData = pfnMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
            if (pGpuData != nullptr)
            {
                memcpy(m_preallocatedCaptureBuffer.data(), pGpuData, frameSize);
                pfnUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
                captured = true;
            }
            pfnBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

            m_pboIndex = nextIndex;
        }

        if (!captured)
        {
            if (!SafeGlReadPixels(width, height, m_preallocatedCaptureBuffer.data()))
                return;
        }

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_frameQueue.size() < kMaxQueueFrames)
            {
                for (uint64_t i = 0; i < framesNeeded && m_frameQueue.size() < kMaxQueueFrames; ++i)
                {
                    m_frameQueue.push(m_preallocatedCaptureBuffer);
                    m_framesPushed.fetch_add(1);
                }
                m_queueCv.notify_one();
            }
        }
#endif
    }

    void GameVideoRecorder::AudioWorkerThread(HANDLE hPipe, int sampleRate)
    {
#ifdef _WIN32
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool comInit = SUCCEEDED(hr);

        IMMDeviceEnumerator* pEnumerator = nullptr;
        IMMDevice* pDevice = nullptr;
        IAudioClient* pAudioClient = nullptr;
        IAudioCaptureClient* pCaptureClient = nullptr;
        WAVEFORMATEX* pwfx = nullptr;
        bool captureReady = false;

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (SUCCEEDED(hr) && pEnumerator != nullptr)
        {
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (SUCCEEDED(hr) && pDevice != nullptr)
            {
                hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
                if (SUCCEEDED(hr) && pAudioClient != nullptr)
                {
                    hr = pAudioClient->GetMixFormat(&pwfx);
                    if (SUCCEEDED(hr) && pwfx != nullptr)
                    {
                        hr = pAudioClient->Initialize(
                            AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_LOOPBACK,
                            10000000,
                            0,
                            pwfx,
                            nullptr
                        );

                        if (SUCCEEDED(hr))
                        {
                            hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
                            if (SUCCEEDED(hr) && pCaptureClient != nullptr)
                            {
                                hr = pAudioClient->Start();
                                if (SUCCEEDED(hr))
                                {
                                    captureReady = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        std::vector<int16_t> pcmConversionBuffer;

        while (!m_stopRequested.load())
        {
            if (captureReady && pCaptureClient != nullptr)
            {
                UINT32 packetLength = 0;
                hr = pCaptureClient->GetNextPacketSize(&packetLength);
                if (SUCCEEDED(hr) && packetLength > 0)
                {
                    BYTE* pData = nullptr;
                    UINT32 numFramesRead = 0;
                    DWORD flags = 0;

                    hr = pCaptureClient->GetBuffer(&pData, &numFramesRead, &flags, nullptr, nullptr);
                    if (SUCCEEDED(hr) && pData != nullptr && numFramesRead > 0)
                    {
                        const int channels = (pwfx != nullptr && pwfx->nChannels > 0) ? pwfx->nChannels : 2;
                        const size_t totalSamples = static_cast<size_t>(numFramesRead) * channels;
                        pcmConversionBuffer.resize(totalSamples);

                        if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                        {
                            std::fill(pcmConversionBuffer.begin(), pcmConversionBuffer.end(), 0);
                        }
                        else if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                                 (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pwfx->wBitsPerSample == 32))
                        {
                            const float* fData = reinterpret_cast<const float*>(pData);
                            for (size_t i = 0; i < totalSamples; ++i)
                            {
                                const float sample = std::clamp(fData[i], -1.0f, 1.0f);
                                pcmConversionBuffer[i] = static_cast<int16_t>(sample * 32767.0f);
                            }
                        }
                        else if (pwfx->wBitsPerSample == 16)
                        {
                            std::memcpy(pcmConversionBuffer.data(), pData, totalSamples * sizeof(int16_t));
                        }

                        DWORD written = 0;
                        const DWORD bytesToWrite = static_cast<DWORD>(totalSamples * sizeof(int16_t));
                        WriteFile(hPipe, pcmConversionBuffer.data(), bytesToWrite, &written, nullptr);

                        pCaptureClient->ReleaseBuffer(numFramesRead);
                        continue;
                    }
                }
            }
            else
            {
                const size_t silenceFrames = static_cast<size_t>(sampleRate) / 100;
                std::vector<int16_t> silence(silenceFrames * 2, 0);
                DWORD written = 0;
                WriteFile(hPipe, silence.data(), static_cast<DWORD>(silence.size() * sizeof(int16_t)), &written, nullptr);
            }

            Sleep(3);
        }

        if (captureReady && pAudioClient != nullptr)
            pAudioClient->Stop();

        if (pwfx != nullptr) CoTaskMemFree(pwfx);
        if (pCaptureClient != nullptr) pCaptureClient->Release();
        if (pAudioClient != nullptr) pAudioClient->Release();
        if (pDevice != nullptr) pDevice->Release();
        if (pEnumerator != nullptr) pEnumerator->Release();

        if (comInit)
            CoUninitialize();

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        m_hAudioPipe = INVALID_HANDLE_VALUE;
#endif
    }

    void GameVideoRecorder::StopWorkerCapture()
    {
#ifdef _WIN32
        bool expected = true;
        if (!m_isWorkerCapturing.compare_exchange_strong(expected, false))
            return;

        m_readyForFrames = false;
        m_stopRequested = true;
        m_queueCv.notify_all();
        CleanupPbo();

        if (m_masterThread.joinable())
            m_masterThread.detach();
#endif
    }
}
