#include "GamelandGuard.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cwctype>
#include <wincrypt.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

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
        L"hlr.dll",
        L"loader.dll",
        L"blackhawk-main.dll"
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
        L"extremeinjector.exe",
        L"ultimate bunny jump.exe",
        L"autobhop.exe",
        L"bhop.exe",
        L"bunnyhop.exe",
        L"bhop_script.exe",
        L"cs_bhop.exe",
        L"csbhop.exe",
        L"autohotkey.exe",
        L"autohotkey32.exe",
        L"autohotkey64.exe",
        L"autoit3.exe",
        L"aimpoint.exe"
    };

#ifndef LLKHF_INJECTED
#define LLKHF_INJECTED 0x00000010
#endif
#ifndef LLKHF_LOWER_IL_INJECTED
#define LLKHF_LOWER_IL_INJECTED 0x00000002
#endif

    static HHOOK g_hKeyboardHook = nullptr;
    static std::atomic<bool> g_inputProtectionRunning{ false };
    static std::thread g_inputProtectionThread;

    // Low-level keyboard hook callback to filter synthetic/macro keystrokes
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            auto* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            if (pKey && pKey->vkCode == VK_SPACE)
            {
                // In Windows, keystrokes injected by SendInput/keybd_event/AHK have LLKHF_INJECTED or LLKHF_LOWER_IL_INJECTED
                if (pKey->flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED))
                {
                    HWND fg = GetForegroundWindow();
                    if (fg != nullptr && g_gameWindow != nullptr && (fg == g_gameWindow || IsChild(g_gameWindow, fg)))
                    {
                        // Injected Space targeting active game window -> DROP IT!
                        // Returning 1 prevents Windows from passing this message to the target window procedure.
                        return 1;
                    }
                }
            }
        }
        return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
    }

    static void InputProtectionWorker()
    {
        // Dedicated thread for low-level hook with its own message pump
        g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);

        MSG msg;
        while (g_inputProtectionRunning.load())
        {
            BOOL bRet = GetMessageW(&msg, nullptr, 0, 0);
            if (bRet == 0 || bRet == -1)
                break;

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (g_hKeyboardHook)
        {
            UnhookWindowsHookEx(g_hKeyboardHook);
            g_hKeyboardHook = nullptr;
        }
    }

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

    static std::wstring GetGameRootDirectory()
    {
        wchar_t exePath[MAX_PATH] = { 0 };
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH))
        {
            wchar_t* lastSlash = wcsrchr(exePath, L'\\');
            if (lastSlash)
            {
                *lastSlash = L'\0';
                return exePath;
            }
        }
        wchar_t curDir[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(MAX_PATH, curDir);
        return curDir;
    }

    static std::string ComputeFileSha256Hex(const std::wstring& filePath)
    {
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return "";

        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        std::string hexResult;

        if (CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        {
            if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
            {
                BYTE buffer[65536];
                DWORD bytesRead = 0;
                bool success = true;

                while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
                {
                    if (!CryptHashData(hHash, buffer, bytesRead, 0))
                    {
                        success = false;
                        break;
                    }
                }

                if (success)
                {
                    BYTE hashBuf[32]{};
                    DWORD hashLen = sizeof(hashBuf);
                    if (CryptGetHashParam(hHash, HP_HASHVAL, hashBuf, &hashLen, 0))
                    {
                        char hex[65]{};
                        for (DWORD i = 0; i < hashLen; ++i)
                            sprintf_s(hex + (i * 2), 3, "%02x", hashBuf[i]);
                        hexResult = hex;
                    }
                }
                CryptDestroyHash(hHash);
            }
            CryptReleaseContext(hProv, 0);
        }

        CloseHandle(hFile);
        return hexResult;
    }

    struct ProtectedBinary
    {
        const wchar_t* relativePath;
        const char* expectedHash;
        const wchar_t* displayName;
    };

    static const ProtectedBinary kProtectedBinaries[] = {
        { L"cstrike\\cl_dlls\\client.dll", "ef7a0f40989cb79ba95d40f528534da36866892ee871147ca82e133b7a5edc3d", L"کلاینت بازی (client.dll)" },
        { L"hw.dll", "be45f76049a133392423679d334c69c8e1e7e82dc873eebdd229ea0341ba1b10", L"موتور گرافیکی اصلی (hw.dll)" },
        { L"cstrike\\dlls\\mp.dll", "5596780734fa40dd709537b26b9a0bf4cf970c150fcc03108ed5432b65e3c331", L"منطق سرور محلی (mp.dll)" },
        { L"SDL2.dll", "f1be4b46ac46bc9a26ae017c95711e5aedc11ed602908b62eface4e8d2b28aba", L"کتابخانه ورودی/صدا (SDL2.dll)" },
        { L"Mss32.dll", "3231d251c8aa4003b3b23196fe849b97c5ea3ac2d3549980e83bceb9078b4cf7", L"موتور صدای مایلز (Mss32.dll)" }
    };

    static bool VerifyFileIntegrity()
    {
        std::wstring gameDir = GetGameRootDirectory();

        for (const auto& bin : kProtectedBinaries)
        {
            wchar_t fullPath[MAX_PATH];
            swprintf_s(fullPath, L"%s\\%s", gameDir.c_str(), bin.relativePath);

            DWORD attrs = GetFileAttributesW(fullPath);
            if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
            {
                wchar_t desc[256];
                swprintf_s(desc, L"فایل سیستمی ضروری یافت نشد: %s", bin.displayName);
                OnViolation(L"فایل ناقص بازی", desc);
                return false;
            }

            std::string actualHash = ComputeFileSha256Hex(fullPath);
            if (actualHash.empty() || _stricmp(actualHash.c_str(), bin.expectedHash) != 0)
            {
                wchar_t desc[350];
                swprintf_s(desc, L"دستکاری غیرمجاز در %s کشف شد.\nامضای دیجیتال این فایل با نسخه رسمی مطابقت ندارد.", bin.displayName);
                OnViolation(L"دستکاری فایل‌های رسمی بازی (Anti-Tamper)", desc);
                return false;
            }
        }
        return true;
    }

    struct AuthorizedAsi
    {
        const wchar_t* fileName;
        const char* expectedHash;
    };

    static const AuthorizedAsi kWhitelistedAsiFiles[] = {
        { L"binkawin.asi", "1aba951f3d3de59aec6c3a77133241dac6949dd4b1d158a77b646ad1ec7c5371" },
        { L"mssmp3.asi", "dd69f9509a50db36ea6f69f5f572c300dead7f0054801a255feb556e00a453ec" },
        { L"mssvoice.asi", "e99de0f5e95a70b84596a66aa1af8eb7f20cb9816e1fc67dbdd8f0feab1b26ac" }
    };

    static bool CheckUnauthorizedAsiFiles()
    {
        std::wstring gameDir = GetGameRootDirectory();
        const wchar_t* checkSubDirs[] = { L"", L"\\cstrike", L"\\valve" };

        for (const auto* sub : checkSubDirs)
        {
            wchar_t searchPattern[MAX_PATH];
            swprintf_s(searchPattern, L"%s%s\\*.asi", gameDir.c_str(), sub);

            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(searchPattern, &fd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        std::wstring lowerName = ToLower(fd.cFileName);
                        bool isAuthorized = false;

                        for (const auto& auth : kWhitelistedAsiFiles)
                        {
                            if (lowerName == ToLower(auth.fileName))
                            {
                                wchar_t asiPath[MAX_PATH];
                                swprintf_s(asiPath, L"%s%s\\%s", gameDir.c_str(), sub, fd.cFileName);
                                std::string hash = ComputeFileSha256Hex(asiPath);
                                if (_stricmp(hash.c_str(), auth.expectedHash) == 0)
                                {
                                    isAuthorized = true;
                                    break;
                                }
                            }
                        }

                        if (!isAuthorized)
                        {
                            wchar_t asiPath[MAX_PATH];
                            swprintf_s(asiPath, L"%s%s\\%s", gameDir.c_str(), sub, fd.cFileName);

                            // Attempt to delete it first
                            if (!DeleteFileW(asiPath))
                            {
                                FindClose(hFind);
                                wchar_t desc[256];
                                swprintf_s(desc, L"پلاگین مشکوک و غیرمجاز شناسایی شد: %s", fd.cFileName);
                                OnViolation(L"پلاگین غیرمجاز (.asi)", desc);
                                return false;
                            }
                        }
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }
        return true;
    }

    // Check if proxy DLLs exist in the current game working directory
    static bool CheckProxyDlls()
    {
        std::wstring gameDir = GetGameRootDirectory();

        for (const auto* dllName : kBlacklistedProxyDlls)
        {
            wchar_t fullPath[MAX_PATH];
            swprintf_s(fullPath, L"%s\\%s", gameDir.c_str(), dllName);

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

    // Returns the resolved destination address of a JMP/CALL/Detour instruction,
    // following up to 4 relay/trampoline jumps if needed.
    static uintptr_t ResolveHookDestination(const unsigned char* pfn)
    {
        if (!pfn)
            return 0;

        __try
        {
            uintptr_t current = reinterpret_cast<uintptr_t>(pfn);
            for (int depth = 0; depth < 4; ++depth)
            {
                const unsigned char* b = reinterpret_cast<const unsigned char*>(current);
                if (!b)
                    break;

                // 0xE9 xx xx xx xx : JMP rel32
                if (b[0] == 0xE9)
                {
                    int32_t rel = *reinterpret_cast<const int32_t*>(b + 1);
                    current = current + 5 + rel;
                }
                // 0xEB xx : JMP rel8
                else if (b[0] == 0xEB)
                {
                    int8_t rel = *reinterpret_cast<const int8_t*>(b + 1);
                    current = current + 2 + rel;
                }
                // 0xFF 0x25 xx xx xx xx : JMP dword ptr [addr]
                else if (b[0] == 0xFF && b[1] == 0x25)
                {
                    uintptr_t* pDword = *reinterpret_cast<uintptr_t* const*>(b + 2);
                    if (pDword && !IsBadReadPtr(pDword, sizeof(uintptr_t)))
                        current = *pDword;
                    else
                        break;
                }
                // 0x68 xx xx xx xx 0xC3 : PUSH imm32; RET
                else if (b[0] == 0x68 && b[5] == 0xC3)
                {
                    current = *reinterpret_cast<const uintptr_t*>(b + 1);
                }
                else
                {
                    return current;
                }
            }
            return current;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }

    // Check if an address belongs to an authorized recording, streaming, overlay, or system module
    static bool IsAddressInWhitelistedModule(uintptr_t addr)
    {
        if (addr == 0)
            return false;

        HMODULE hMod = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(addr),
                &hMod) && hMod)
        {
            wchar_t modulePath[MAX_PATH] = { 0 };
            if (GetModuleFileNameW(hMod, modulePath, MAX_PATH))
            {
                std::wstring pathLower = ToLower(modulePath);
                const wchar_t* fileName = wcsrchr(modulePath, L'\\');
                std::wstring fileLower = ToLower(fileName ? (fileName + 1) : modulePath);

                static const wchar_t* kWhitelistedModules[] = {
                    // OBS Studio & Streamlabs
                    L"graphics-hook32.dll",
                    L"graphics-hook64.dll",
                    L"obs-hook32.dll",
                    L"obs-hook64.dll",
                    L"obs-vulkan32.dll",
                    L"obs-vulkan64.dll",
                    L"hook32.dll",
                    L"hook64.dll",
                    // Steam Client Overlay
                    L"gameoverlayrenderer.dll",
                    L"gameoverlayrenderer64.dll",
                    // Discord Overlay
                    L"discordhook.dll",
                    L"discordhook32.dll",
                    L"discordhook64.dll",
                    // RivaTuner Statistics Server (RTSS) / MSI Afterburner
                    L"rtsshooks.dll",
                    L"rtsshooks64.dll",
                    L"rtss.dll",
                    // Bandicam
                    L"bdcam32.dll",
                    L"bdcam64.dll",
                    L"bdcam.dll",
                    L"bdcap32.dll",
                    // Mirillis Action!
                    L"action_x86.dll",
                    L"action_x64.dll",
                    // Fraps
                    L"fraps32.dll",
                    L"fraps64.dll",
                    L"fraps.dll",
                    // Overwolf / Medal.tv
                    L"owclient.dll",
                    L"owexplorer.dll",
                    L"medal-hook32.dll",
                    L"medal-hook64.dll",
                    // NVIDIA ShadowPlay / GeForce Experience
                    L"nvspcap.dll",
                    L"nvspcap64.dll",
                    L"geforce_overlay.dll",
                    L"nvvideoencode.dll",
                    L"nvd3d9wrap.dll",
                    // AMD Radeon ReLive
                    L"amdfcd32.dll",
                    L"amdfcd64.dll",
                    L"amdfld32.dll",
                    // PresentMon
                    L"presentmon.dll",
                    // System graphics & core runtimes
                    L"opengl32.dll",
                    L"gdi32.dll",
                    L"user32.dll",
                    L"d3d9.dll",
                    L"dxgi.dll",
                    L"nvoglv32.dll",
                    L"atio6axx.dll",
                    L"ig9ic32.dll",
                    L"ig4ic32.dll",
                    L"kernel32.dll",
                    L"ntdll.dll",
                    // Game internal binaries
                    L"cstrike.exe",
                    L"allclient.exe",
                    L"next_engine_mini.dll",
                    L"nitro_api2.dll",
                    L"vgui2.dll",
                    L"filesystem_proxy.dll",
                    L"client_mini.dll",
                    L"gameui.dll"
                };

                for (const auto* whitelisted : kWhitelistedModules)
                {
                    if (fileLower == whitelisted)
                        return true;
                }

                // Path keyword checks for authorized software installations
                if (pathLower.find(L"obs-studio") != std::wstring::npos ||
                    pathLower.find(L"streamlabs") != std::wstring::npos ||
                    pathLower.find(L"discord") != std::wstring::npos ||
                    pathLower.find(L"steam") != std::wstring::npos ||
                    pathLower.find(L"rivatuner") != std::wstring::npos ||
                    pathLower.find(L"bandicam") != std::wstring::npos ||
                    pathLower.find(L"nvidia") != std::wstring::npos ||
                    pathLower.find(L"overwolf") != std::wstring::npos ||
                    pathLower.find(L"medal") != std::wstring::npos)
                {
                    return true;
                }
            }
        }
        else
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) != 0)
            {
                wchar_t mappedName[MAX_PATH] = { 0 };
                if (GetMappedFileNameW(GetCurrentProcess(), mbi.BaseAddress, mappedName, MAX_PATH) > 0)
                {
                    std::wstring mappedLower = ToLower(mappedName);
                    if (mappedLower.find(L"graphics-hook") != std::wstring::npos ||
                        mappedLower.find(L"obs") != std::wstring::npos ||
                        mappedLower.find(L"discord") != std::wstring::npos ||
                        mappedLower.find(L"gameoverlay") != std::wstring::npos ||
                        mappedLower.find(L"rtss") != std::wstring::npos)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
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
            // 0x68 ... 0xC3 = PUSH addr; RET
            bool isHooked = (bytes[0] == 0xE9) ||
                            (bytes[0] == 0xFF && bytes[1] == 0x25) ||
                            (bytes[0] == 0xEB) ||
                            (bytes[0] == 0x68 && bytes[5] == 0xC3);

            if (isHooked)
            {
                uintptr_t destAddr = ResolveHookDestination(bytes);

                // Check if the destination belongs to a whitelisted recording/overlay tool:
                if (IsAddressInWhitelistedModule(destAddr))
                {
                    // Authorized capture/overlay tool (e.g. OBS Studio, Steam Overlay, Discord) -> PERMITTED!
                    continue;
                }

                // For wglSwapBuffers specifically:
                // Wallhacks do NOT operate through wglSwapBuffers (they must alter geometry/depth in glBegin/glClear/glDepthFunc).
                // If wglSwapBuffers is hooked, check if it's any legitimate capture tool
                if (strcmp(fnName, "wglSwapBuffers") == 0)
                {
                    HMODULE hDestMod = nullptr;
                    if (GetModuleHandleExW(
                            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(destAddr),
                            &hDestMod) && hDestMod)
                    {
                        wchar_t destModPath[MAX_PATH] = { 0 };
                        if (GetModuleFileNameW(hDestMod, destModPath, MAX_PATH))
                        {
                            std::wstring destLower = ToLower(destModPath);
                            if (destLower.find(L"hook") != std::wstring::npos ||
                                destLower.find(L"capture") != std::wstring::npos ||
                                destLower.find(L"record") != std::wstring::npos ||
                                destLower.find(L"stream") != std::wstring::npos ||
                                destLower.find(L"overlay") != std::wstring::npos ||
                                destLower.find(L"video") != std::wstring::npos)
                            {
                                continue;
                            }
                        }
                    }
                }

                // If not whitelisted and truly an unauthorized hook -> Trigger Anti-Cheat violation!
                wchar_t desc[256];
                swprintf_s(desc, L"تغییر غیرمجاز در تابع رندرینگ %hs (OpenGL Hook/Wallhack)", fnName);
                OnViolation(L"هوک رندرینگ گرافیکی (وال‌هک)", desc);
            }
        }
    }

    // Check if an external overlay process is an authorized recording, streaming, or system tool
    static bool IsWhitelistedOverlayProcess(DWORD pid)
    {
        if (pid == 0 || pid == GetCurrentProcessId())
            return true;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc)
            return false;

        wchar_t procPath[MAX_PATH] = { 0 };
        DWORD pathLen = MAX_PATH;
        bool whitelisted = false;

        if (QueryFullProcessImageNameW(hProc, 0, procPath, &pathLen))
        {
            std::wstring pathLower = ToLower(procPath);
            const wchar_t* fileName = wcsrchr(procPath, L'\\');
            std::wstring exeLower = ToLower(fileName ? (fileName + 1) : procPath);

            static const wchar_t* kWhitelistedProcesses[] = {
                L"obs64.exe",
                L"obs32.exe",
                L"obs.exe",
                L"streamlabs obs.exe",
                L"streamlabs desktop.exe",
                L"slobs.exe",
                L"discord.exe",
                L"discordcanary.exe",
                L"discordptb.exe",
                L"steam.exe",
                L"gameoverlayui.exe",
                L"rtss.exe",
                L"msiafterburner.exe",
                L"bdcam.exe",
                L"action.exe",
                L"fraps.exe",
                L"nvidia share.exe",
                L"nvcontainer.exe",
                L"nvsphelper64.exe",
                L"radeonsoftware.exe",
                L"amdrsserv.exe",
                L"dwm.exe",
                L"explorer.exe",
                L"applicationframehost.exe",
                L"shellexperiencehost.exe",
                L"textinputhost.exe",
                L"ctfmon.exe",
                L"sharex.exe",
                L"lightshot.exe",
                L"snippingtool.exe",
                L"screentogif.exe",
                L"overwolf.exe",
                L"overwolfbrowser.exe",
                L"medal.exe"
            };

            for (const auto* target : kWhitelistedProcesses)
            {
                if (exeLower == target)
                {
                    whitelisted = true;
                    break;
                }
            }

            if (!whitelisted)
            {
                if (pathLower.find(L"obs-studio") != std::wstring::npos ||
                    pathLower.find(L"streamlabs") != std::wstring::npos ||
                    pathLower.find(L"discord") != std::wstring::npos ||
                    pathLower.find(L"steam") != std::wstring::npos ||
                    pathLower.find(L"nvidia") != std::wstring::npos ||
                    pathLower.find(L"overwolf") != std::wstring::npos ||
                    pathLower.find(L"medal") != std::wstring::npos ||
                    pathLower.find(L"bandicam") != std::wstring::npos)
                {
                    whitelisted = true;
                }
            }
        }

        CloseHandle(hProc);
        return whitelisted;
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
            // Check process ownership of this window first
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != 0 && pid != GetCurrentProcessId())
            {
                if (IsWhitelistedOverlayProcess(pid))
                {
                    // Authorized recording/overlay/system window -> Skip!
                    return TRUE;
                }
            }

            // Check window class name (e.g. Windows capture frame or tooltips)
            wchar_t className[128] = { 0 };
            GetClassNameW(hwnd, className, 127);
            std::wstring classLower = ToLower(className);
            if (classLower.find(L"graphicscapture") != std::wstring::npos ||
                classLower.find(L"corewindow") != std::wstring::npos ||
                classLower.find(L"tooltip") != std::wstring::npos ||
                classLower.find(L"dwm") != std::wstring::npos ||
                classLower.find(L"qt") != std::wstring::npos)
            {
                return TRUE;
            }

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

    // Scan for external AutoHotkey / script cheat windows
    static BOOL CALLBACK EnumAutoHotkeyWindows(HWND hwnd, LPARAM lParam)
    {
        wchar_t className[64] = { 0 };
        if (GetClassNameW(hwnd, className, 63))
        {
            if (_wcsicmp(className, L"AutoHotkey") == 0 || _wcsicmp(className, L"AutoHotkeyGUI") == 0)
            {
                wchar_t windowTitle[512] = { 0 };
                GetWindowTextW(hwnd, windowTitle, 511);
                std::wstring titleLower = ToLower(windowTitle);

                static const wchar_t* kKeywords[] = {
                    L"bhop", L"bunny", L"jump", L"cheat", L"hack", L"trigger", L"aim", L"macro", L"ultimate"
                };

                for (const auto* kw : kKeywords)
                {
                    if (titleLower.find(kw) != std::wstring::npos)
                    {
                        DWORD pid = 0;
                        GetWindowThreadProcessId(hwnd, &pid);
                        if (pid != 0 && pid != GetCurrentProcessId())
                        {
                            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                            if (hProc)
                            {
                                TerminateProcess(hProc, 0);
                                CloseHandle(hProc);
                            }
                        }

                        OnViolation(L"ابزار غیرمجاز Auto-BunnyHop",
                                    L"اسکریپت یا نرم‌افزار پرش خودکار (AutoHotkey/Auto-Bhop) در حال اجرا شناسایی شد.");
                        return FALSE;
                    }
                }
            }
        }
        return TRUE;
    }

    static void ScanAutoHotkeyCheats()
    {
        EnumWindows(EnumAutoHotkeyWindows, 0);
    }

    // Background Watchdog worker
    static void WatchdogWorker()
    {
        int watchdogCycles = 0;
        while (g_watchdogRunning)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1300));
            if (!g_watchdogRunning)
                break;

            CheckBlacklistedProcesses();
            ScanAutoHotkeyCheats();
            ScanOpenGLHooks();
            ScanUnbackedExecutableMemory();
            ClearHardwareBreakpoints();

            watchdogCycles++;
            if (watchdogCycles % 5 == 0)
            {
                VerifyFileIntegrity();
                CheckUnauthorizedAsiFiles();
            }

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

        if (!CheckUnauthorizedAsiFiles())
            return false;

        if (!VerifyFileIntegrity())
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
        if (!g_inputProtectionRunning.exchange(true))
        {
            g_inputProtectionThread = std::thread(InputProtectionWorker);
        }
        if (!g_watchdogRunning.exchange(true))
        {
            g_watchdogThread = std::thread(WatchdogWorker);
        }
    }

    void StopWatchdog()
    {
        if (g_inputProtectionRunning.exchange(false))
        {
            if (g_inputProtectionThread.joinable())
            {
                DWORD tid = GetThreadId(g_inputProtectionThread.native_handle());
                if (tid != 0)
                {
                    PostThreadMessageW(tid, WM_QUIT, 0, 0);
                }
                g_inputProtectionThread.join();
            }
        }

        if (g_watchdogRunning.exchange(false))
        {
            if (g_watchdogThread.joinable())
            {
                g_watchdogThread.join();
            }
        }
    }
}
