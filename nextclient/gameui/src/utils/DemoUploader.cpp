#include "DemoUploader.h"
#include <windows.h>
#include <cpr/cpr.h>
#include <filesystem>
#include <iostream>

#ifndef NEXTCLIENT_TAG
#define NEXTCLIENT_TAG "default_gamenet"
#endif

namespace fs = std::filesystem;

DemoUploader::DemoUploader() : m_bRunning(false) {
}

DemoUploader::~DemoUploader() {
    Shutdown();
}

void DemoUploader::Initialize() {
    if (m_bRunning) return;
    m_bRunning = true;
    m_WorkerThread = std::thread(&DemoUploader::WorkerThread, this);
}

void DemoUploader::Shutdown() {
    if (m_bRunning) {
        m_bRunning = false;
        if (m_WorkerThread.joinable()) {
            m_WorkerThread.join();
        }
    }
}

std::wstring DemoUploader::GetCStrikeDirectory() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path p(exePath);
    // Usually it's in root/cstrike.exe, so cstrike is the directory next to it or named cstrike
    // Assuming the executable is in root directory and demos are in root/cstrike
    fs::path cstrikePath = p.parent_path() / L"cstrike";
    return cstrikePath.wstring();
}

bool DemoUploader::IsFileInUse(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_WRITE,
        0, // No sharing! If game is writing, this will fail
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return true; // File is in use or we don't have permission
    }

    CloseHandle(hFile);
    return false;
}

bool DemoUploader::UploadDemo(const std::wstring& filePath, const std::string& tag) {
    fs::path p(filePath);
    std::string filename = p.filename().string();
    
    // We convert wstring to string for the file path
    std::string filePathStr = p.string();

    cpr::Response r = cpr::Post(
        cpr::Url{"http://dl.gameland.cam/api.php"},
        cpr::Multipart{
            {"action", "upload_demo"},
            {"tag", tag},
            {"demo_file", cpr::File{filePathStr}}
        }
    );

    if (r.status_code == 200) {
        return true;
    }

    return false;
}

void DemoUploader::ScanAndUploadDemos() {
    std::wstring cstrikeDir = GetCStrikeDirectory();
    if (!fs::exists(cstrikeDir)) return;

    for (const auto& entry : fs::directory_iterator(cstrikeDir)) {
        if (!entry.is_regular_file()) continue;
        
        fs::path filePath = entry.path();
        if (filePath.extension() == L".dem") {
            // Check if file is finished recording
            if (!IsFileInUse(filePath.wstring())) {
                // Upload
                std::string tag = NEXTCLIENT_TAG;
                if (UploadDemo(filePath.wstring(), tag)) {
                    // Delete locally after successful upload to save space and avoid re-uploading
                    try {
                        fs::remove(filePath);
                    } catch (...) {
                        // ignore errors
                    }
                }
            }
        }
    }
}

void DemoUploader::WorkerThread() {
    // Wait for the game to fully start before we start uploading
    std::this_thread::sleep_for(std::chrono::seconds(15));
    
    while (m_bRunning) {
        ScanAndUploadDemos();
        
        // Check every 60 seconds, but respond to shutdown quickly
        for (int i = 0; i < 60 && m_bRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}
