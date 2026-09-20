#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>

class DemoUploader {
public:
    static DemoUploader& GetInstance() {
        static DemoUploader instance;
        return instance;
    }

    void Initialize();
    void Shutdown();

private:
    DemoUploader();
    ~DemoUploader();

    void WorkerThread();
    void ScanAndUploadDemos();
    bool UploadDemo(const std::wstring& filePath, const std::string& tag);
    bool IsFileInUse(const std::wstring& filePath);
    std::wstring GetCStrikeDirectory();

    std::thread m_WorkerThread;
    std::atomic<bool> m_bRunning;
};
