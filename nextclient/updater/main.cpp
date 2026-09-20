#include <windows.h>
#include <urlmon.h>
#include <shellapi.h>
#include <iostream>
#include <string>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    int argc;
    LPWSTR *argv = CommandLineToArgvW(lpCmdLine, &argc);

    if (argc < 2) {
        MessageBoxW(NULL, L"این برنامه فقط از طریق خود کلاینت قابل اجرا است.", L"NextClient Updater Error", MB_ICONERROR);
        if (argv) LocalFree(argv);
        return 1;
    }

    std::wstring url = argv[0];
    
    // Create a temporary file path for the downloaded patch installer
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring installerPath = std::wstring(tempPath) + L"NextClient_Update.exe";

    // Inform the user
    MessageBoxW(NULL, L"در حال دریافت فایل‌های آپدیت...\nلطفاً صبور باشید.", L"NextClient Updater", MB_ICONINFORMATION | MB_OK);

    // Download the installer
    HRESULT hr = URLDownloadToFileW(NULL, url.c_str(), installerPath.c_str(), 0, NULL);
    if (hr != S_OK) {
        MessageBoxW(NULL, L"خطا در دانلود آپدیت! بررسی کنید اینترنت متصل باشد.", L"NextClient Updater Error", MB_ICONERROR);
        LocalFree(argv);
        return 1;
    }

    // Execute the downloaded installer silently
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas"; 
    sei.lpFile = installerPath.c_str();
    // Use Inno Setup silent flags
    sei.lpParameters = L"/VERYSILENT /SUPPRESSMSGBOXES /FORCECLOSEAPPLICATIONS";
    sei.nShow = SW_HIDE;

    if (ShellExecuteExW(&sei)) {
        // Wait for installer to finish updating the files
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
        
        // Restart the game using the launcher (assumes updater.exe is in the game root alongside Allclient.exe or cstrike.exe)
        // Since we are running from a temporary location or the game root, we will try to start Allclient.exe.
        // Actually updater is best placed in the root, so Allclient.exe is right next to it.
        ShellExecuteW(NULL, L"open", L"Allclient.exe", NULL, NULL, SW_SHOWNORMAL);
    } else {
        MessageBoxW(NULL, L"خطا در اجرای فایل نصب کننده آپدیت.", L"NextClient Updater Error", MB_ICONERROR);
    }

    LocalFree(argv);
    return 0;
}
