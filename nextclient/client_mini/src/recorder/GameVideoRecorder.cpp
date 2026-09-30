#include "GameVideoRecorder.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <direct.h>
#include <io.h>

#ifdef _WIN32
#include <windows.h>
#include <gl/GL.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

// Isolated C-style SEH wrapper without any C++ object unwinding (prevents MSVC C2712)
static bool SafeGlReadPixels(int width, int height, uint8_t* pDest)
{
    __try
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
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
    GameVideoRecorder& GameVideoRecorder::Instance()
    {
        static GameVideoRecorder s_instance;
        return s_instance;
    }

    GameVideoRecorder::GameVideoRecorder() = default;

    GameVideoRecorder::~GameVideoRecorder()
    {
        Stop();
    }

    std::string GameVideoRecorder::FindFfmpegExecutable() const
    {
#ifdef _WIN32
        // 1. Check folder of current running game executable
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

        // 2. Check candidate working and deployment paths
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

    bool GameVideoRecorder::Start(const std::string& baseFileName, int width, int height, int targetFps)
    {
        if (m_isRecording.load() || m_isFinalizing.load())
            return false;

        _mkdir("cstrike\\videos");

        m_recordWidth = width;
        m_recordHeight = height;
        m_targetFps = (targetFps > 0) ? targetFps : 60;
        m_framesPushed = 0;

        const std::string ffmpegPath = FindFfmpegExecutable();
        m_currentVideoPath = "cstrike/videos/" + baseFileName + ".mp4";

#ifdef _WIN32
        const DWORD pid = GetCurrentProcessId();
        const DWORD randId = GetTickCount();
        const std::string videoPipeName = "\\\\.\\pipe\\gl_vid_" + std::to_string(pid) + "_" + std::to_string(randId);
        const std::string audioPipeName = "\\\\.\\pipe\\gl_aud_" + std::to_string(pid) + "_" + std::to_string(randId);

        // Pre-allocate capture buffer once on main thread
        const size_t frameBytes = static_cast<size_t>(width) * height * 3;
        m_preallocatedCaptureBuffer.resize(frameBytes, 0);

        m_stopRequested = false;
        m_readyForFrames = false;
        m_isFinalizing = false;
        m_isRecording = true;
        m_startTime = std::chrono::steady_clock::now();

        // Clear existing frame queue
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            std::queue<std::vector<uint8_t>> emptyQueue;
            std::swap(m_frameQueue, emptyQueue);
        }

        // Launch background master thread so render loop never freezes
        m_masterThread = std::thread(
            &GameVideoRecorder::MasterWorkerThread,
            this,
            ffmpegPath,
            m_currentVideoPath,
            videoPipeName,
            audioPipeName,
            width,
            height,
            m_targetFps
        );
#endif

        return true;
    }

    void GameVideoRecorder::MasterWorkerThread(
        std::string ffmpegPath,
        std::string outputPath,
        std::string videoPipeName,
        std::string audioPipeName,
        int width,
        int height,
        int fps)
    {
#ifdef _WIN32
        constexpr DWORD kVideoBufSize = 4 * 1024 * 1024;
        constexpr DWORD kAudioBufSize = 256 * 1024;

        HANDLE hVideoPipe = CreateNamedPipeA(
            videoPipeName.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            1,
            kVideoBufSize,
            kVideoBufSize,
            5000,
            nullptr
        );

        HANDLE hAudioPipe = CreateNamedPipeA(
            audioPipeName.c_str(),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            1,
            kAudioBufSize,
            kAudioBufSize,
            5000,
            nullptr
        );

        if (hVideoPipe == INVALID_HANDLE_VALUE || hAudioPipe == INVALID_HANDLE_VALUE)
        {
            if (hVideoPipe != INVALID_HANDLE_VALUE) CloseHandle(hVideoPipe);
            if (hAudioPipe != INVALID_HANDLE_VALUE) CloseHandle(hAudioPipe);
            m_isRecording = false;
            m_readyForFrames = false;
            m_isFinalizing = false;
            return;
        }

        m_hVideoPipe = hVideoPipe;
        m_hAudioPipe = hAudioPipe;

        const int audioRate = QuerySystemAudioSampleRate();

        // High-precision FFmpeg command:
        // - aresample=async=1000:min_hard_comp=0.100000:first_pts=0 locks audio PTS to video PTS with 0ms drift
        std::ostringstream cmd;
        cmd << "\"" << ffmpegPath << "\" -y -hide_banner -loglevel error"
            << " -f rawvideo -pix_fmt rgb24 -s " << width << "x" << height
            << " -r " << fps << " -i \"" << videoPipeName << "\""
            << " -f s16le -ar " << audioRate << " -ac 2 -i \"" << audioPipeName << "\""
            << " -vf vflip"
            << " -c:v libx264 -preset ultrafast -tune zerolatency -pix_fmt yuv420p -b:v 4500k"
            << " -af aresample=async=1000:min_hard_comp=0.100000:first_pts=0"
            << " -c:a aac -b:a 160k"
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
            m_isRecording = false;
            m_readyForFrames = false;
            m_isFinalizing = false;
            return;
        }

        m_hFfmpegProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        // Step 1: Connect video pipe (FFmpeg input 0)
        ConnectNamedPipe(hVideoPipe, nullptr);

        // Step 2: Send exactly 1 initial video frame for FFmpeg probe
        const size_t probeFrameSize = static_cast<size_t>(width) * height * 3;
        std::vector<uint8_t> probeFrame(probeFrameSize, 0);
        DWORD probeWritten = 0;
        WriteFile(hVideoPipe, probeFrame.data(), static_cast<DWORD>(probeFrame.size()), &probeWritten, nullptr);

        // Step 3: Connect audio pipe (FFmpeg input 1)
        ConnectNamedPipe(hAudioPipe, nullptr);

        // Step 4: Symmetrical initial audio chunk: exactly 1 frame equivalent (1/fps of a second)
        const size_t probeAudioSamples = static_cast<size_t>(audioRate / fps);
        std::vector<int16_t> probeAudio(probeAudioSamples * 2, 0);
        WriteFile(hAudioPipe, probeAudio.data(), static_cast<DWORD>(probeAudio.size() * sizeof(int16_t)), &probeWritten, nullptr);

        // Step 5: Start audio capture thread
        m_audioThread = std::thread(&GameVideoRecorder::AudioWorkerThread, this, hAudioPipe, audioRate);

        // Step 6: Mark synchronized start time t0 and open gate for game frames
        m_syncStartTime = std::chrono::steady_clock::now();
        m_framesPushed = 1; // 1 probe frame already written
        m_readyForFrames = true;

        // Video streaming loop
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

        // Flush remaining frames from queue
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

        // Disconnect and close video pipe cleanly
        DisconnectNamedPipe(hVideoPipe);
        CloseHandle(hVideoPipe);
        m_hVideoPipe = INVALID_HANDLE_VALUE;

        // Join audio worker thread
        if (m_audioThread.joinable())
            m_audioThread.join();

        // Wait for FFmpeg to finalize container with 5-second timeout
        if (m_hFfmpegProcess != nullptr)
        {
            WaitForSingleObject(m_hFfmpegProcess, 5000);
            CloseHandle(m_hFfmpegProcess);
            m_hFfmpegProcess = nullptr;
        }

        m_lastSavedVideoPath = outputPath;
        m_readyForFrames = false;
        m_isRecording = false;
        m_isFinalizing = false;
#endif
    }

    void GameVideoRecorder::CaptureFrame(int width, int height)
    {
#ifdef _WIN32
        if (!m_isRecording.load() || !m_readyForFrames.load() || m_stopRequested.load())
            return;

        if (width != m_recordWidth || height != m_recordHeight)
            return;

        // Precision wallclock timeline pacing:
        // Calculates how many 60fps frames should exist at this exact microsecond
        const auto now = std::chrono::steady_clock::now();
        const double elapsedSec = std::chrono::duration<double>(now - m_syncStartTime).count();
        const uint64_t targetFrameCount = static_cast<uint64_t>(elapsedSec * m_targetFps);

        const uint64_t currentPushed = m_framesPushed.load();
        if (currentPushed >= targetFrameCount)
            return; // Render loop is running faster than 60 FPS (e.g. 100 FPS), skip this frame

        uint64_t framesNeeded = targetFrameCount - currentPushed;
        if (framesNeeded > 3)
            framesNeeded = 3; // Clamp burst to avoid queue flooding

        const size_t frameSize = static_cast<size_t>(width) * height * 3;
        if (m_preallocatedCaptureBuffer.size() != frameSize)
            m_preallocatedCaptureBuffer.resize(frameSize, 0);

        // Crash-proof OpenGL capture protected by isolated SEH function
        if (!SafeGlReadPixels(width, height, m_preallocatedCaptureBuffer.data()))
            return;

        // Non-blocking try-lock: pushes frame and compensates for any dropped frames
        {
            std::unique_lock<std::mutex> lock(m_queueMutex, std::try_to_lock);
            if (lock.owns_lock())
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
                        // Ultra low latency buffer: 40ms (400,000 hns) instead of 1,000ms
                        hr = pAudioClient->Initialize(
                            AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_LOOPBACK,
                            400000,
                            0,
                            pwfx,
                            nullptr
                        );

                        if (SUCCEEDED(hr))
                        {
                            hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
                            if (SUCCEEDED(hr) && pCaptureClient != nullptr)
                            {
                                pAudioClient->Start();
                                captureReady = true;
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
                // Fallback silence generation if sound card or WASAPI is unavailable (Lite Windows builds)
                const size_t silenceFrames = static_cast<size_t>(sampleRate) / 100; // 10ms worth
                std::vector<int16_t> silence(silenceFrames * 2, 0);
                DWORD written = 0;
                WriteFile(hPipe, silence.data(), static_cast<DWORD>(silence.size() * sizeof(int16_t)), &written, nullptr);
            }

            // Low latency poll: 3ms instead of 10ms
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

    void GameVideoRecorder::Stop()
    {
#ifdef _WIN32
        // Atomic compare-exchange: ensures Stop() is executed exactly once
        bool expected = true;
        if (!m_isRecording.compare_exchange_strong(expected, false))
            return;

        m_isFinalizing = true;
        m_readyForFrames = false;
        m_stopRequested = true;
        m_queueCv.notify_all();

        // 100% non-blocking: detach worker thread so main game render loop NEVER stalls or freezes!
        if (m_masterThread.joinable())
            m_masterThread.detach();
#endif
    }

    double GameVideoRecorder::GetElapsedSeconds() const
    {
        if (!m_isRecording.load())
            return 0.0;

        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - m_startTime).count();
    }

    std::string GameVideoRecorder::GetFormattedTime() const
    {
        const int totalSeconds = static_cast<int>(GetElapsedSeconds());
        const int minutes = totalSeconds / 60;
        const int seconds = totalSeconds % 60;

        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);
        return buf;
    }

    std::string GameVideoRecorder::GetCurrentVideoPath() const
    {
        return m_currentVideoPath;
    }

    std::string GameVideoRecorder::GetLastSavedVideoPath() const
    {
        return m_lastSavedVideoPath;
    }
}
