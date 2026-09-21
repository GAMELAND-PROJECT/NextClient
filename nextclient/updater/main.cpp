#include <windows.h>
#include <commctrl.h>
#include <urlmon.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")

// Custom window messages
#define WM_UPDATE_PROGRESS (WM_APP + 201)
#define WM_UPDATE_STATUS   (WM_APP + 202)
#define WM_UPDATE_FINISHED (WM_APP + 203)
#define WM_UPDATE_ERROR    (WM_APP + 204)

static HWND g_hProgressWnd = NULL;
static HWND g_hProgressBar = NULL;
static HWND g_hStatusText = NULL;
static HWND g_hPercentText = NULL;
static HFONT g_hFont = NULL;
static HFONT g_hFontBold = NULL;
static HBRUSH g_hBgBrush = NULL;

static std::atomic<bool> g_bCancel(false);
static std::wstring g_sTargetUrl;
static std::wstring g_sGameDir;

// Progress Callback Implementation
class CDownloadCallback : public IBindStatusCallback {
public:
    CDownloadCallback(HWND hWnd) : m_hWnd(hWnd), m_cRef(1) {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) {
        if (!ppvObject) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IBindStatusCallback) {
            *ppvObject = static_cast<IBindStatusCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&m_cRef); }
    STDMETHOD_(ULONG, Release)() {
        ULONG count = InterlockedDecrement(&m_cRef);
        if (count == 0) delete this;
        return count;
    }

    STDMETHOD(OnStartBinding)(DWORD, IBinding*) { return S_OK; }
    STDMETHOD(GetPriority)(LONG*) { return S_OK; }
    STDMETHOD(OnLowResource)(DWORD) { return S_OK; }
    STDMETHOD(OnProgress)(ULONG ulProgress, ULONG ulProgressMax, ULONG, LPCWSTR) {
        if (g_bCancel) return E_ABORT;
        if (ulProgressMax > 0 && m_hWnd) {
            int percent = static_cast<int>((static_cast<double>(ulProgress) / ulProgressMax) * 100.0);
            if (percent > 100) percent = 100;
            PostMessage(m_hWnd, WM_UPDATE_PROGRESS, static_cast<WPARAM>(percent), 0);
        }
        return S_OK;
    }
    STDMETHOD(OnStopBinding)(HRESULT, LPCWSTR) { return S_OK; }
    STDMETHOD(GetBindInfo)(DWORD*, BINDINFO*) { return S_OK; }
    STDMETHOD(OnDataAvailable)(DWORD, DWORD, FORMATETC*, STGMEDIUM*) { return S_OK; }
    STDMETHOD(OnObjectAvailable)(REFIID, IUnknown*) { return S_OK; }

private:
    HWND m_hWnd;
    LONG m_cRef;
};

static bool ExecuteHiddenProcess(const std::wstring& command, const std::wstring& workingDir = L"") {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuffer(command.begin(), command.end());
    cmdBuffer.push_back(L'\0');

    const wchar_t* pWorkingDir = workingDir.empty() ? NULL : workingDir.c_str();

    if (CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, pWorkingDir, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 180000); // 3 minutes max
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
    Sleep(500);
}

static bool IsZipFile(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
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

// Background Worker Thread
static void UpdateWorkerThread(HWND hWnd) {
    PostMessage(hWnd, WM_UPDATE_STATUS, 0, (LPARAM)L"در حال بستن فرآیندهای باز بازی...");
    KillGameProcesses();
    Sleep(1000);

    wchar_t tempDir[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring downloadPath = std::wstring(tempDir) + L"NextClient_Update.zip";
    DeleteFileW(downloadPath.c_str());

    PostMessage(hWnd, WM_UPDATE_STATUS, 0, (LPARAM)L"در حال برقراری ارتباط و دانلود پکیج جدید...");
    CDownloadCallback* pCallback = new CDownloadCallback(hWnd);

    HRESULT hr = URLDownloadToFileW(NULL, g_sTargetUrl.c_str(), downloadPath.c_str(), 0, pCallback);
    pCallback->Release();

    if (hr != S_OK || g_bCancel) {
        PostMessage(hWnd, WM_UPDATE_ERROR, 0, (LPARAM)L"خطا در دریافت فایل آپدیت از سرور! لطفاً اتصال اینترنت خود را بررسی کنید.");
        return;
    }

    PostMessage(hWnd, WM_UPDATE_STATUS, 0, (LPARAM)L"در حال استخراج و نصب فایل‌های جدید کلاینت...");
    PostMessage(hWnd, WM_UPDATE_PROGRESS, 95, 0);

    // Double check game processes are closed before overwriting
    KillGameProcesses();
    Sleep(500);

    bool updateSuccess = false;
    if (IsZipFile(downloadPath) || (g_sTargetUrl.length() >= 4 && _wcsicmp(g_sTargetUrl.c_str() + g_sTargetUrl.length() - 4, L".zip") == 0)) {
        // Strategy 1: Windows tar.exe (fast, native in Windows 10/11)
        std::wstring tarCmd = L"tar.exe -xf \"" + downloadPath + L"\" -C \"" + g_sGameDir + L"\"";
        updateSuccess = ExecuteHiddenProcess(tarCmd, g_sGameDir);

        // Strategy 2: PowerShell Expand-Archive (fallback)
        if (!updateSuccess) {
            std::wstring psCmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" +
                                 downloadPath + L"' -DestinationPath '" + g_sGameDir + L"' -Force\"";
            updateSuccess = ExecuteHiddenProcess(psCmd, g_sGameDir);
        }

        // Strategy 3: .NET ZipFile extraction script via powershell
        if (!updateSuccess) {
            std::wstring netCmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('" +
                                  downloadPath + L"', '" + g_sGameDir + L"', $true)\"";
            updateSuccess = ExecuteHiddenProcess(netCmd, g_sGameDir);
        }
    } else {
        std::wstring installParams = L"/VERYSILENT /SUPPRESSMSGBOXES /FORCECLOSEAPPLICATIONS /DIR=\"" + g_sGameDir + L"\"";
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";
        sei.lpFile = downloadPath.c_str();
        sei.lpParameters = installParams.c_str();
        sei.nShow = SW_HIDE;

        if (ShellExecuteExW(&sei)) {
            WaitForSingleObject(sei.hProcess, 300000);
            CloseHandle(sei.hProcess);
            updateSuccess = true;
        }
    }

    DeleteFileW(downloadPath.c_str());

    if (!updateSuccess) {
        PostMessage(hWnd, WM_UPDATE_ERROR, 0, (LPARAM)L"خطا در استخراج و جایگزینی فایل‌های آپدیت!");
        return;
    }

    PostMessage(hWnd, WM_UPDATE_PROGRESS, 100, 0);
    PostMessage(hWnd, WM_UPDATE_STATUS, 0, (LPARAM)L"بروزرسانی با موفقیت کامل شد! در حال راه‌اندازی کلاینت...");
    Sleep(1200);

    PostMessage(hWnd, WM_UPDATE_FINISHED, 0, 0);
}

static LRESULT CALLBACK ProgressWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        NONCLIENTMETRICSW ncm = { sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        wcscpy_s(ncm.lfMessageFont.lfFaceName, L"Segoe UI");
        ncm.lfMessageFont.lfHeight = -14;
        g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);

        ncm.lfMessageFont.lfWeight = FW_BOLD;
        ncm.lfMessageFont.lfHeight = -16;
        g_hFontBold = CreateFontIndirectW(&ncm.lfMessageFont);

        g_hBgBrush = CreateSolidBrush(RGB(24, 26, 28));

        HWND hTitle = CreateWindowExW(WS_EX_RTLREADING, L"STATIC", L"سیستم بروزرسانی هوشمند Allclient",
            WS_CHILD | WS_VISIBLE | SS_RIGHT, 24, 20, 440, 26, hWnd, NULL, NULL, NULL);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        g_hStatusText = CreateWindowExW(WS_EX_RTLREADING, L"STATIC", L"آماده‌سازی برای بروزرسانی...",
            WS_CHILD | WS_VISIBLE | SS_RIGHT, 24, 60, 440, 24, hWnd, NULL, NULL, NULL);
        SendMessageW(g_hStatusText, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        g_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 24, 94, 440, 26, hWnd, NULL, NULL, NULL);
        SendMessageW(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_hProgressBar, PBM_SETPOS, 0, 0);

        g_hPercentText = CreateWindowExW(0, L"STATIC", L"0%",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 24, 130, 440, 22, hWnd, NULL, NULL, NULL);
        SendMessageW(g_hPercentText, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        std::thread(UpdateWorkerThread, hWnd).detach();
        return 0;
    }
    case WM_CTLCOLORDLG: {
        return (LRESULT)g_hBgBrush;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_hBgBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(225, 230, 235));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)g_hBgBrush;
    }
    case WM_UPDATE_STATUS: {
        const wchar_t* pMsg = (const wchar_t*)lParam;
        if (pMsg && g_hStatusText) {
            SetWindowTextW(g_hStatusText, pMsg);
        }
        return 0;
    }
    case WM_UPDATE_PROGRESS: {
        int percent = (int)wParam;
        if (g_hProgressBar) {
            SendMessageW(g_hProgressBar, PBM_SETPOS, percent, 0);
        }
        if (g_hPercentText) {
            std::wstring sPct = std::to_wstring(percent) + L"%";
            SetWindowTextW(g_hPercentText, sPct.c_str());
        }
        return 0;
    }
    case WM_UPDATE_FINISHED: {
        std::wstring launcherExe = g_sGameDir + L"\\Allclient.exe";
        if (!PathFileExistsW(launcherExe.c_str())) {
            launcherExe = g_sGameDir + L"\\cstrike.exe";
        }
        ShellExecuteW(NULL, L"open", launcherExe.c_str(), NULL, g_sGameDir.c_str(), SW_SHOWNORMAL);
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_UPDATE_ERROR: {
        const wchar_t* pErr = (const wchar_t*)lParam;
        MessageBoxW(hWnd, pErr ? pErr : L"خطا در دریافت بروزرسانی.", L"خطای بروزرسانی", MB_ICONERROR);
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_CLOSE: {
        g_bCancel = true;
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_DESTROY: {
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hBgBrush) DeleteObject(g_hBgBrush);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int) {
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc >= 2 && argv[1] != NULL && wcslen(argv[1]) > 0) {
        g_sTargetUrl = argv[1];
    } else if (lpCmdLine != NULL && wcslen(lpCmdLine) > 0) {
        g_sTargetUrl = lpCmdLine;
        if (g_sTargetUrl.length() >= 2 && g_sTargetUrl.front() == L'"' && g_sTargetUrl.back() == L'"') {
            g_sTargetUrl = g_sTargetUrl.substr(1, g_sTargetUrl.length() - 2);
        }
    }
    if (argv) LocalFree(argv);

    if (g_sTargetUrl.empty()) {
        MessageBoxW(NULL, L"این برنامه فقط از طریق لانچر و برای دریافت خودکار آپدیت کلاینت اجرا می‌شود.", L"NextClient Updater", MB_ICONINFORMATION);
        return 1;
    }

    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    g_sGameDir = exePath;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = ProgressWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(24, 26, 28));
    wc.lpszClassName = L"AllclientUpdateProgressWindow";
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    int w = 500;
    int h = 210;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hProgressWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_RTLREADING, wc.lpszClassName,
        L"در حال دریافت بروزرسانی Allclient",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h, NULL, NULL, hInstance, NULL);

    if (!g_hProgressWnd) return 1;

    ShowWindow(g_hProgressWnd, SW_SHOWNORMAL);
    UpdateWindow(g_hProgressWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
