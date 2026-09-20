#include "AutoUpdate.h"
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "wininet.lib")

#ifndef NEXTCLIENT_VERSION
#define NEXTCLIENT_VERSION "0.0.0"
#endif

#ifndef NEXTCLIENT_TAG
#define NEXTCLIENT_TAG "unknown"
#endif

namespace NextClient {

void AutoUpdate::CheckForUpdatesAndExitIfForced() {
    HINTERNET hInternet = InternetOpenA("NextClientAutoUpdater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return;

    DWORD timeout = 2000; // 2 seconds max to prevent startup lag
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    std::string url = std::string("http://gameland.cam/update_api.php?tag=") + NEXTCLIENT_TAG + "&version=" + NEXTCLIENT_VERSION;
    
    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (hConnect) {
        char buffer[1024];
        DWORD bytesRead;
        std::string response;
        while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
        }
        InternetCloseHandle(hConnect);

        // Very basic JSON parse
        if (response.find("\"update_available\":true") != std::string::npos) {
            size_t urlPos = response.find("\"download_url\":\"");
            if (urlPos != std::string::npos) {
                urlPos += 16;
                size_t urlEnd = response.find("\"", urlPos);
                if (urlEnd != std::string::npos) {
                    std::string downloadUrl = response.substr(urlPos, urlEnd - urlPos);
                    
                    MessageBoxW(NULL, 
                        L"آپدیت جدیدی برای سیستم شما منتشر شده است.\nبرای ادامه حتماً باید کلاینت بروزرسانی شود.", 
                        L"NextClient Auto-Update", MB_ICONINFORMATION | MB_TOPMOST);

                    // Execute updater
                    SHELLEXECUTEINFOA sei = { sizeof(sei) };
                    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                    sei.lpVerb = "open";
                    sei.lpFile = "updater.exe";
                    sei.lpParameters = downloadUrl.c_str();
                    sei.nShow = SW_SHOWNORMAL;
                    
                    if (ShellExecuteExA(&sei)) {
                        ExitProcess(0);
                    } else {
                        MessageBoxW(NULL, L"خطا در اجرای updater.exe. لطفاً نصب‌کننده اصلی را مجدداً دانلود کنید.", L"NextClient Auto-Update", MB_ICONERROR);
                        ExitProcess(1);
                    }
                }
            }
        }
    }
    InternetCloseHandle(hInternet);
}

}
