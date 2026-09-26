#include "GamelandGuard.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "psapi.lib")

namespace gameland_guard
{
    static std::atomic<bool> g_watchdogRunning{ false };
    static std::thread g_watchdogThread;
    static HWND g_gameWindow = nullptr;

    // List of blacklisted proxy DLLs that cheats place in CS 1.6 root directory
    static const wchar_t* kBlacklistedProxyDlls[] = {
        L"opengl32.dll",
        L"cdhack.dll",
        L"leis.dll",
        L"vermillion.dll",
        L"hack.dll",
        L"cheat.dll",
        L"aimbot.dll",
        L"kykhack.dll",
        L"badboy.dll",
        L"r-aimbot.dll",
        L"hlr.dll"
    };

    // List of blacklisted cheat tools and debuggers
    static const wchar_t* kBlacklistedProcesses[] = {
        L"cheatengine-x86_64.exe",
        L"cheatengine-i386.exe",
        L"cheatengine.exe",
        L"x64dbg.exe",
        L"x32dbg.exe",
        L"ollydbg.exe",
        L"ida.exe",
        L"ida64.exe",
        L"processhacker.exe",
        L"injector.exe",
        L"extremeinjector.exe"
    };

    static std::wstring ToLower(const std::wstring& str)
    {
        std::wstring out = str;
        std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) { return std::towlower(c); });
        return out;
    }

    void OnViolation(const wchar_t* violationType, const wchar_t* details)
    {
        wchar_t message[1024];
        swprintf_s(message, 
            L"سیستم امنیتی GAMELAND Shield\n\n"
            L"تخلف امنیتی شناسایی شد: %s\n"
            L"توضیحات: %s\n\n"
            L"جهت حفظ سلامت رقابت‌ها و عدالت بازی، اجرای بازی متوقف شد.",
            violationType, details);

        MessageBoxW(nullptr, message, L"GAMELAND Anti-Cheat Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
        ExitProcess(0x1337);
    }

    // Check if proxy DLLs exist in the current game working directory
    static bool CheckProxyDlls()
    {
        wchar_t gameDir[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(MAX_PATH, gameDir);

        for (const auto* dllName : kBlacklistedProxyDlls)
        {
            wchar_t fullPath[MAX_PATH];
            swprintf_s(fullPath, L"%s\\%s", gameDir, dllName);

            DWORD attrs = GetFileAttributesW(fullPath);
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
            {
                // Attempt to delete it first, or abort
                if (!DeleteFileW(fullPath))
                {
                    wchar_t desc[256];
                    swprintf_s(desc, L"فایل مشکوک و غیرمجاز شناسایی شد: %s", dllName);
                    OnViolation(L"فایل DLL غیرمجاز در پوشه بازی", desc);
                    return false;
                }
            }
        }
        return true;
    }

    // Check if blacklisted memory scanners or debuggers are running
    static bool CheckBlacklistedProcesses()
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return true;

        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snap, &entry))
        {
            do
            {
                std::wstring procName = ToLower(entry.szExeFile);
                for (const auto* target : kBlacklistedProcesses)
                {
                    if (procName == ToLower(target))
                    {
                        CloseHandle(snap);
                        wchar_t desc[256];
                        swprintf_s(desc, L"نرم‌افزار مشکوک یا دیباگر در حال اجرا شناسایی شد: %s", entry.szExeFile);
                        OnViolation(L"نرم‌افزار دستکاری حافظه", desc);
                        return false;
                    }
                }
            } while (Process32NextW(snap, &entry));
        }

        CloseHandle(snap);
        return true;
    }

    // Anti-debug checks
    static bool CheckDebugger()
    {
        if (IsDebuggerPresent())
        {
            OnViolation(L"اتصال دیباگر به بازی", L"دیباگر فعال در پردازش بازی شناسایی شد.");
            return false;
        }

        BOOL remoteDebugger = FALSE;
        if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger) && remoteDebugger)
        {
            OnViolation(L"اتصال دیباگر از راه دور", L"ابزار دیباگینگ متصل به پردازش شناسایی شد.");
            return false;
        }

        return true;
    }

    // Verify OpenGL export function prologues for Wallhack/Chams hooks
    static void ScanOpenGLHooks()
    {
        HMODULE hOpenGL = GetModuleHandleW(L"opengl32.dll");
        if (!hOpenGL)
            return;

        const char* kCriticalFunctions[] = {
            "glBegin",
            "glClear",
            "glDepthFunc",
            "glDrawElements",
            "wglSwapBuffers",
            "glViewport"
        };

        for (const char* fnName : kCriticalFunctions)
        {
            FARPROC pfn = GetProcAddress(hOpenGL, fnName);
            if (!pfn)
                continue;

            const unsigned char* bytes = reinterpret_cast<const unsigned char*>(pfn);

            // Hook indicators:
            // 0xE9 = relative JMP (detour)
            // 0xFF 0x25 = absolute indirect JMP
            // 0xEB = short JMP
            if (bytes[0] == 0xE9 || (bytes[0] == 0xFF && bytes[1] == 0x25) || bytes[0] == 0xEB)
            {
                wchar_t desc[256];
                swprintf_s(desc, L"تغییر غیرمجاز در تابع رندرینگ %hs (OpenGL Hook/Wallhack)", fnName);
                OnViolation(L"هوک رندرینگ گرافیکی (وال‌هک)", desc);
            }
        }
    }

    // Scan for transparent external overlay windows (External ESP)
    struct OverlaySearchContext
    {
        HWND gameHwnd;
        RECT gameRect;
    };

    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
    {
        auto* ctx = reinterpret_cast<OverlaySearchContext*>(lParam);
        if (!ctx || hwnd == ctx->gameHwnd)
            return TRUE;

        if (!IsWindowVisible(hwnd))
            return TRUE;

        DWORD exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
        // External ESP overlays typically have WS_EX_LAYERED and WS_EX_TRANSPARENT
        if ((exStyle & WS_EX_LAYERED) && (exStyle & WS_EX_TRANSPARENT))
        {
            RECT winRect;
            if (GetWindowRect(hwnd, &winRect))
            {
                // Check if it overlaps game window closely
                int overlapWidth = std::min(winRect.right, ctx->gameRect.right) - std::max(winRect.left, ctx->gameRect.left);
                int overlapHeight = std::min(winRect.bottom, ctx->gameRect.bottom) - std::max(winRect.top, ctx->gameRect.top);

                if (overlapWidth > 300 && overlapHeight > 200)
                {
                    wchar_t title[128] = { 0 };
                    GetWindowTextW(hwnd, title, 127);
                    wchar_t desc[256];
                    swprintf_s(desc, L"پنجره شفاف خارجی همپوشان بر روی بازی شناسایی شد (عنوان: %s)", title);
                    OnViolation(L"اکسترنال اورلی (External ESP)", desc);
                    return FALSE;
                }
            }
        }
        return TRUE;
    }

    static void ScanExternalOverlays(HWND hwndGame)
    {
        if (!hwndGame || !IsWindow(hwndGame))
            return;

        OverlaySearchContext ctx;
        ctx.gameHwnd = hwndGame;
        if (GetWindowRect(hwndGame, &ctx.gameRect))
        {
            EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&ctx));
        }
    }

    // Scan for unbacked executable memory pages (Manual Mapping detection)
    static void ScanUnbackedExecutableMemory()
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        unsigned char* addr = static_cast<unsigned char*>(sysInfo.lpMinimumApplicationAddress);
        unsigned char* maxAddr = static_cast<unsigned char*>(sysInfo.lpMaximumApplicationAddress);

        while (addr < maxAddr)
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0)
                break;

            // Check executable and writable private pages
            if ((mbi.State == MEM_COMMIT) && (mbi.Type == MEM_PRIVATE))
            {
                if (mbi.Protect == PAGE_EXECUTE_READWRITE || mbi.Protect == PAGE_EXECUTE_READ)
                {
                    // Check if it contains a hidden PE (MZ header)
                    if (mbi.RegionSize >= 4096)
                    {
                        const unsigned char* base = static_cast<const unsigned char*>(mbi.BaseAddress);
                        __try
                        {
                            if (base[0] == 'M' && base[1] == 'Z')
                            {
                                OnViolation(L"تزریق مخفی در حافظه (Manual Mapping)", 
                                            L"بلوک کد اجرایی مخفی در حافظه بازی کشف شد.");
                            }
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                            // Memory access protected
                        }
                    }
                }
            }

            addr += mbi.RegionSize;
        }
    }

    // Check and clear hardware breakpoints (DR0 - DR3)
    static void ClearHardwareBreakpoints()
    {
        CONTEXT ctx;
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        HANDLE hThread = GetCurrentThread();

        if (GetThreadContext(hThread, &ctx))
        {
            if (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0)
            {
                ctx.Dr0 = 0;
                ctx.Dr1 = 0;
                ctx.Dr2 = 0;
                ctx.Dr3 = 0;
                ctx.Dr7 = 0;
                SetThreadContext(hThread, &ctx);
            }
        }
    }

    // Background Watchdog worker
    static void WatchdogWorker()
    {
        while (g_watchdogRunning)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1300));
            if (!g_watchdogRunning)
                break;

            ScanOpenGLHooks();
            ScanUnbackedExecutableMemory();
            ClearHardwareBreakpoints();

            if (g_gameWindow && IsWindow(g_gameWindow))
            {
                ScanExternalOverlays(g_gameWindow);
            }
            else
            {
                // Try finding game window
                g_gameWindow = FindWindowA("Valve001", nullptr);
            }
        }
    }

    void Initialize()
    {
        // Set unhandled exception filter to catch crashes and hide threads
        typedef LONG(WINAPI* pfnNtSetInformationThread)(HANDLE, ULONG, PVOID, ULONG);
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll)
        {
            auto fn = reinterpret_cast<pfnNtSetInformationThread>(GetProcAddress(hNtdll, "NtSetInformationThread"));
            if (fn)
            {
                // ThreadHideFromDebugger = 0x11
                fn(GetCurrentThread(), 0x11, nullptr, 0);
            }
        }
    }

    bool PreLaunchScan()
    {
        Initialize();

        if (!CheckProxyDlls())
            return false;

        if (!CheckBlacklistedProcesses())
            return false;

        if (!CheckDebugger())
            return false;

        return true;
    }

    void StartWatchdog(HWND gameWindow)
    {
        g_gameWindow = gameWindow;
        if (!g_watchdogRunning.exchange(true))
        {
            g_watchdogThread = std::thread(WatchdogWorker);
        }
    }

    void StopWatchdog()
    {
        if (g_watchdogRunning.exchange(false))
        {
            if (g_watchdogThread.joinable())
            {
                g_watchdogThread.join();
            }
        }
    }
}
