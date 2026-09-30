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
    struct DemoFileItem
    {
        std::string fileName;
        std::string mapName;
        std::string dateFormatted;
        std::string sizeFormatted;
        uint64_t timestamp{0};
    };

    class GameVideoRecorder
    {
    public:
        static GameVideoRecorder& Instance();

        // In-Match Zero-Lag Demo Recording (0% GPU flush, locked 100 FPS)
        bool StartMatchDemo(const std::string& baseFileName);
        void StopMatchDemo();
        bool IsMatchDemoRecording() const { return m_isMatchDemoRecording.load(); }
        std::string GetFormattedDemoTime() const;
        std::string GetCurrentDemoFileName() const { return m_currentDemoFileName; }

        // In-Lobby Demo Studio & Background MP4 Converter
        std::vector<DemoFileItem> RefreshDemoList();
        const std::vector<DemoFileItem>& GetCachedDemos() const { return m_cachedDemos; }
        bool StartDemoConversion(const std::string& demoFileName, int targetWidth = 1280, int targetHeight = 720, int fps = 60);
        void CancelConversion();
        bool IsConverting() const { return m_isConverting.load(); }
        int GetConversionPercent() const { return m_convertPercent.load(); }
        std::string GetConvertingDemoName() const { return m_convertingDemoName; }
        std::string GetLastConvertedVideoPath() const { return m_lastConvertedVideoPath; }

        // Dedicated Headless Conversion Engine (Invoked by background worker)
        bool StartWorkerCapture(const std::string& outputBaseName, int width, int height, int fps = 60);
        void WorkerCaptureFrame(int width, int height);
        void StopWorkerCapture();

    private:
        GameVideoRecorder();
        ~GameVideoRecorder();

        GameVideoRecorder(const GameVideoRecorder&) = delete;
        GameVideoRecorder& operator=(const GameVideoRecorder&) = delete;

        void MasterWorkerThread(std::string ffmpegPath, std::string outputPath, std::string videoPipeName, std::string audioPipeName, int width, int height, int fps);
        void AudioWorkerThread(HANDLE hPipe, int sampleRate);
        std::string FindFfmpegExecutable() const;
        int QuerySystemAudioSampleRate() const;
        void MonitorConversionThread(HANDLE hProcess, std::string outputPath);

    private:
        // In-Match Demo State
        std::atomic<bool> m_isMatchDemoRecording{false};
        std::string m_currentDemoFileName;
        std::chrono::steady_clock::time_point m_matchDemoStartTime;

        // In-Lobby Studio State
        std::vector<DemoFileItem> m_cachedDemos;
        std::atomic<bool> m_isConverting{false};
        std::atomic<int> m_convertPercent{0};
        std::string m_convertingDemoName;
        std::string m_lastConvertedVideoPath;
        std::thread m_conversionMonitorThread;
        HANDLE m_hConversionProcess{nullptr};

        // Worker Capture State (Used inside headless worker instance)
        std::atomic<bool> m_isWorkerCapturing{false};
        std::atomic<bool> m_readyForFrames{false};
        std::atomic<bool> m_stopRequested{false};
        std::atomic<uint64_t> m_framesPushed{0};
        std::chrono::steady_clock::time_point m_syncStartTime;

        int m_recordWidth{1280};
        int m_recordHeight{720};
        int m_targetFps{60};

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
