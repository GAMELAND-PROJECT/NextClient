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

    enum class HighlightState
    {
        Idle,
        Marking,
        AwaitingConfirm,
        Seeking,
        Rendering
    };

    class GameVideoRecorder
    {
    public:
        static GameVideoRecorder& Instance();

        // In-Match Zero-Lag Demo Recording (F4 Menu)
        bool StartMatchDemo(const std::string& baseFileName);
        void StopMatchDemo();
        bool IsMatchDemoRecording() const { return m_isMatchDemoRecording.load(); }
        std::string GetFormattedDemoTime() const;
        std::string GetCurrentDemoFileName() const { return m_currentDemoFileName; }

        // Bookmark & HLAE Studio Render (Keys 1 & 2 only)
        HighlightState GetHighlightState() const { return m_highlightState.load(); }
        bool IsHighlightIdle() const { return m_highlightState.load() == HighlightState::Idle; }
        bool IsHighlightMarking() const { return m_highlightState.load() == HighlightState::Marking; }
        bool IsHighlightAwaitingConfirm() const { return m_highlightState.load() == HighlightState::AwaitingConfirm; }
        bool IsHighlightSeeking() const { return m_highlightState.load() == HighlightState::Seeking; }
        bool IsHighlightRendering() const { return m_highlightState.load() == HighlightState::Rendering; }

        // Real demo playback time directly from demoplayer.dll (IDemoPlayer001)
        double GetExactDemoTime();
        double GetDemoStartTime();
        double GetDemoEndTime();
        bool SetDemoWorldTime(double time, bool relative = false);
        bool SetDemoPaused(bool paused);

        void MarkIn(float demoTime);
        bool MarkOut(float demoTime);
        void SetHighlightSeeking() { m_highlightState = HighlightState::Seeking; }
        void DiscardHighlight();

        bool StartStudioRender(const std::string& demoOrMapName, int width, int height);
        void CaptureStudioFrame(int width, int height);
        void FinishStudioRender();

        float GetMarkInTime() const { return m_markInTime; }
        float GetMarkOutTime() const { return m_markOutTime; }
        float GetHighlightDuration() const { return (m_markOutTime > m_markInTime) ? (m_markOutTime - m_markInTime) : 0.0f; }
        std::string GetFormattedTime(float seconds) const;
        std::string GetHighlightRangeFormatted() const;
        std::string GetHighlightClipInfo() const { return m_highlightClipInfo; }
        int GetRenderProgressPercent() const;
        uint64_t GetRenderFramesPushed() const { return m_renderFramesPushed.load(); }
        uint64_t GetRenderTargetFrames() const { return m_renderTargetFrames; }
        std::string GetLastSavedHighlightPath() const { return m_lastSavedHighlightPath; }

        void SetCurrentPlayingDemoName(const std::string& name) { m_currentPlayingDemoName = name; }
        std::string GetCurrentPlayingDemoName() const { return m_currentPlayingDemoName; }

        // In-Lobby Demo Studio & Background MP4 Converter
        std::vector<DemoFileItem> RefreshDemoList();
        const std::vector<DemoFileItem>& GetCachedDemos() const { return m_cachedDemos; }
        bool StartDemoConversion(const std::string& demoFileName, int targetWidth = 1280, int targetHeight = 720, int fps = 100);
        void CancelConversion();
        bool IsConverting() const { return m_isConverting.load(); }
        int GetConversionPercent() const { return m_convertPercent.load(); }
        std::string GetConvertingDemoName() const { return m_convertingDemoName; }
        std::string GetLastConvertedVideoPath() const { return m_lastConvertedVideoPath; }

        // Dedicated Headless Conversion Engine (Invoked by background worker)
        bool StartWorkerCapture(const std::string& outputBaseName, int width, int height, int fps = 100);
        void WorkerCaptureFrame(int width, int height);
        void StopWorkerCapture();

    private:
        GameVideoRecorder();
        ~GameVideoRecorder();

        GameVideoRecorder(const GameVideoRecorder&) = delete;
        GameVideoRecorder& operator=(const GameVideoRecorder&) = delete;

        void MasterWorkerThread(std::string ffmpegPath, std::string outputPath, std::string videoPipeName, std::string audioPipeName, int width, int height, int fps);
        void AudioWorkerThread(HANDLE hPipe, int sampleRate);
        void AudioRecordingThread(std::string wavPath, int sampleRate);
        std::string BuildStudioVideoFilter() const;
        std::string FindFfmpegExecutable() const;
        int QuerySystemAudioSampleRate() const;
        void MonitorConversionThread(HANDLE hProcess, std::string outputPath);
        void InitPbo(int width, int height);
        void CleanupPbo();
        void PipeWriterWorker();

    private:
        // In-Match Demo State
        std::atomic<bool> m_isMatchDemoRecording{false};
        std::string m_currentDemoFileName;
        std::chrono::steady_clock::time_point m_matchDemoStartTime;

        // Bookmark & HLAE Studio Render State
        std::atomic<HighlightState> m_highlightState{HighlightState::Idle};
        float m_markInTime{0.0f};
        float m_markOutTime{0.0f};
        uint64_t m_renderTargetFrames{0};
        std::atomic<uint64_t> m_renderFramesPushed{0};
        bool m_isFirstRenderFrame{true};

        std::string m_currentHighlightDemoName;
        std::string m_currentPlayingDemoName{"Demo"};
        std::string m_tempAudioPath;
        std::string m_tempVideoPath;
        std::string m_lastSavedHighlightPath;
        std::string m_highlightClipInfo;

        std::atomic<bool> m_stopAudioRequested{false};
        std::thread m_audioRecordThread;

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
        std::chrono::steady_clock::time_point m_lastFrameTime;

        int m_recordWidth{1280};
        int m_recordHeight{720};
        int m_targetFps{100};

#ifdef _WIN32
        HANDLE m_hFfmpegProcess{nullptr};
        HANDLE m_hVideoPipe{INVALID_HANDLE_VALUE};
        HANDLE m_hAudioPipe{INVALID_HANDLE_VALUE};

        std::thread m_masterThread;
        std::thread m_audioThread;
        std::thread m_pipeWriterThread;

        std::mutex m_queueMutex;
        std::condition_variable m_queueCv;
        std::condition_variable m_queueSpaceCv;
        std::queue<std::vector<uint8_t>> m_frameQueue;
        std::vector<std::vector<uint8_t>> m_frameBufferPool;
        std::atomic<bool> m_stopWriterThread{false};
        static constexpr size_t kMaxQueueFrames = 150;

        std::vector<uint8_t> m_preallocatedCaptureBuffer;
        unsigned int m_pboIds[2]{0, 0};
        int m_pboIndex{0};
        bool m_pboSupported{false};
        bool m_pboInitialized{false};
        int m_pboWidth{0};
        int m_pboHeight{0};
#endif
    };
}
