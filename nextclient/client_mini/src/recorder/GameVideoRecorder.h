#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace nextclient::client_mini
{
    class GameVideoRecorder
    {
    public:
        static GameVideoRecorder& Instance();

        bool Start(const std::string& baseFileName, int width, int height, int targetFps = 60);
        void CaptureFrame(int width, int height);
        void Stop();

        bool IsRecording() const { return m_isRecording.load(); }
        bool IsFinalizing() const { return m_isFinalizing.load(); }
        double GetElapsedSeconds() const;
        std::string GetFormattedTime() const;
        std::string GetCurrentVideoPath() const;
        std::string GetLastSavedVideoPath() const;

    private:
        GameVideoRecorder();
        ~GameVideoRecorder();

        GameVideoRecorder(const GameVideoRecorder&) = delete;
        GameVideoRecorder& operator=(const GameVideoRecorder&) = delete;

        void MasterWorkerThread(std::string ffmpegPath, std::string outputPath, std::string videoPipeName, std::string audioPipeName, int width, int height, int fps);
        void AudioWorkerThread(HANDLE hPipe, int sampleRate);
        std::string FindFfmpegExecutable() const;
        int QuerySystemAudioSampleRate() const;

    private:
        std::atomic<bool> m_isRecording{false};
        std::atomic<bool> m_isFinalizing{false};
        std::atomic<bool> m_readyForFrames{false};
        std::atomic<bool> m_stopRequested{false};

        std::string m_currentVideoPath;
        std::string m_lastSavedVideoPath;
        std::chrono::steady_clock::time_point m_startTime;
        std::chrono::steady_clock::time_point m_syncStartTime;

        int m_recordWidth{0};
        int m_recordHeight{0};
        int m_targetFps{60};

        std::atomic<uint64_t> m_framesPushed{0};

#ifdef _WIN32
        HANDLE m_hFfmpegProcess{nullptr};
        HANDLE m_hVideoPipe{INVALID_HANDLE_VALUE};
        HANDLE m_hAudioPipe{INVALID_HANDLE_VALUE};

        std::thread m_masterThread;
        std::thread m_audioThread;

        std::mutex m_queueMutex;
        std::condition_variable m_queueCv;
        std::queue<std::vector<uint8_t>> m_frameQueue;
        static constexpr size_t kMaxQueueFrames = 15;

        std::vector<uint8_t> m_preallocatedCaptureBuffer;
#endif
    };
}
