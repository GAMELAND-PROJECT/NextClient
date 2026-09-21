#include <windows.h>
#include <urlmon.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib")

static bool ExecuteHiddenProcess(const std::wstring& command, const std::wstring& workingDir = L"") {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuffer(command.begin(), command.end());
    cmdBuffer.push_back(L'\0');

    const wchar_t* pWorkingDir = workingDir.empty() ? NULL : workingDir.c_str();

    if (CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, pWorkingDir, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 120000); // 2 minutes max
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

static void KillGameProcesses() {
    ExecuteHiddenProcess(L"taskkill.exe /F /IM cstrike.exe /IM hl.exe /IM Allclient.exe");
    Sleep(500); // brief pause to release file handles
}

static bool IsZipFile(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    unsigned char header[4] = { 0 };
    DWORD bytesRead = 0;
    bool isZip = false;
    if (ReadFile(hFile, header, sizeof(header), &bytesRead, NULL) && bytesRead >= 4) {
        if (header[0] == 0x50 && header[1] == 0x4B && header[2] == 0x03 && header[3] == 0x04) {
            isZip = true;
        }
    }
    CloseHandle(hFile);
    return isZip;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring url;
    if (argc >= 2 && argv[1] != NULL && wcslen(argv[1]) > 0) {
        url = argv[1];
    } else if (lpCmdLine != NULL && wcslen(lpCmdLine) > 0) {
        url = lpCmdLine;
        // Trim leading and trailing quotes if any
        if (url.length() >= 2 && url.front() == L'"' && url.back() == L'"') {
            url = url.substr(1, url.length() - 2);
        }
    }

    if (argv) {
        LocalFree(argv);
    }

    if (url.empty()) {
        MessageBoxW(NULL, L"این برنامه فقط از طریق بازی و برای اعمال آپدیت کلاینت قابل اجرا است.", L"NextClient Updater", MB_ICONINFORMATION);
        return 1;
    }

    // Determine game root directory (where updater.exe is located)
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring gameDir = exePath;

    // Temporary downloaded patch path
    wchar_t tempDir[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring downloadPath = std::wstring(tempDir) + L"NextClient_Update.pkg";
    DeleteFileW(downloadPath.c_str());

    // Kill any remaining game processes so files are not locked
    KillGameProcesses();

    // Inform the user
    MessageBoxW(NULL, L"در حال دریافت فایل‌های آپدیت جدید از هاست...\nلطفاً چند لحظه شکیبا باشید.", L"NextClient Updater", MB_ICONINFORMATION | MB_OK);

    // Download update file from URL
    HRESULT hr = URLDownloadToFileW(NULL, url.c_str(), downloadPath.c_str(), 0, NULL);
    if (hr != S_OK) {
        MessageBoxW(NULL, L"خطا در دانلود فایل آپدیت از سرور!\nلطفاً اتصال اینترنت خود را بررسی نمایید.", L"خطای آپدیت", MB_ICONERROR);
        return 1;
    }

    bool updateSuccess = false;

    // Check if the downloaded package is a ZIP archive or an EXE installer
    if (IsZipFile(downloadPath) || (url.length() >= 4 && _wcsicmp(url.c_str() + url.length() - 4, L".zip") == 0)) {
        // 1. Try Windows native tar.exe first (fastest)
        std::wstring tarCmd = L"tar.exe -xf \"" + downloadPath + L"\" -C \"" + gameDir + L"\"";
        updateSuccess = ExecuteHiddenProcess(tarCmd, gameDir);

        // 2. Fallback to PowerShell Expand-Archive
        if (!updateSuccess) {
            std::wstring psCmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" +
                                 downloadPath + L"' -DestinationPath '" + gameDir + L"' -Force\"";
            updateSuccess = ExecuteHiddenProcess(psCmd, gameDir);
        }
    } else {
        // Execute installer silently with target game directory
        std::wstring installParams = L"/VERYSILENT /SUPPRESSMSGBOXES /FORCECLOSEAPPLICATIONS /DIR=\"" + gameDir + L"\"";
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";
        sei.lpFile = downloadPath.c_str();
        sei.lpParameters = installParams.c_str();
        sei.nShow = SW_HIDE;

        if (ShellExecuteExW(&sei)) {
            WaitForSingleObject(sei.hProcess, 300000); // 5 minutes max
            CloseHandle(sei.hProcess);
            updateSuccess = true;
        }
    }

    // Clean up temporary downloaded file
    DeleteFileW(downloadPath.c_str());

    if (!updateSuccess) {
        MessageBoxW(NULL, L"خطا در استخراج یا نصب فایل‌های آپدیت!", L"خطای بروزرسانی", MB_ICONERROR);
        return 1;
    }

    // Restart the game launcher
    std::wstring launcherExe = gameDir + L"\\Allclient.exe";
    if (!PathFileExistsW(launcherExe.c_str())) {
        launcherExe = gameDir + L"\\cstrike.exe";
    }

    ShellExecuteW(NULL, L"open", launcherExe.c_str(), NULL, gameDir.c_str(), SW_SHOWNORMAL);
    return 0;
}
