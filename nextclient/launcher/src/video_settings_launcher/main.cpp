#include <Windows.h>
#include <CommCtrl.h>
#include <WinInet.h>
#include <windowsx.h>
#include <shellapi.h>

#include <algorithm>
#include <compare>
#include <cstring>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../next_launcher/GameNetAccess.h"
#pragma comment(lib, "msimg32.lib")

namespace
{
constexpr wchar_t kWindowClass[] = L"NextClientLauncherWindow";
constexpr wchar_t kTitle[] = L"\u0631\u0627\u0647\u200c\u0627\u0646\u062f\u0627\u0632 Allclient";
constexpr wchar_t kSettingsKey[] = L"Software\\Valve\\Half-Life\\Settings";
constexpr wchar_t kLauncherKey[] = L"Software\\Valve\\Half-Life\\nextclient\\video_launcher";
constexpr int kWindowsPointerSpeeds[] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20};

enum ControlId
{
    IdResolution = 100,
    IdHdModels,
    IdHighQuality,
    IdLaunch,
    IdRestore,
    IdStatus,
    IdCancel,
    IdPointerSpeed,
    IdEnhancePointer,
    IdPointerSpeedValue,
    IdSubscriptionState,
    IdSubscriptionTag,
    IdSubscriptionDetails,
    IdSubscriptionRemaining,
    IdDemoManager,
    IdDemoPassword,
    IdDemoVerify,
    IdDemoList,
    IdDemoUpload,
    IdDemoDelete,
    IdDemoRefresh,
    IdDemoClose,
    IdDemoStatus,
    IdUserPhone,
    IdUserPassword,
    IdUserLogin,
    IdUserRegister,
    IdUserStatus,
    IdRegMobile,
    IdRegRequestOtp,
    IdRegOtp,
    IdRegPassword,
    IdRegSubmit,
    IdRegClose,
    IdRegStatus,
};

struct Resolution
{
    int width;
    int height;

    auto operator<=>(const Resolution&) const = default;
};

struct VideoSettings
{
    DWORD width = 800;
    DWORD height = 600;
    DWORD bpp = 32;
    DWORD windowed = 0;
    DWORD hdModels = 0;
    DWORD videoLevel = 1;
};

struct SystemMouseSettings
{
    int speed = 10;
    int acceleration[3] = {6, 10, 1};
};

bool SameSettings(const VideoSettings& left, const VideoSettings& right)
{
    return left.width == right.width && left.height == right.height && left.bpp == right.bpp &&
           left.windowed == right.windowed && left.hdModels == right.hdModels &&
           left.videoLevel == right.videoLevel;
}

bool SameSettings(const SystemMouseSettings& left, const SystemMouseSettings& right)
{
    return left.speed == right.speed && left.acceleration[0] == right.acceleration[0] &&
           left.acceleration[1] == right.acceleration[1] && left.acceleration[2] == right.acceleration[2];
}

int PointerSpeedToSliderPosition(int speed)
{
    const auto it = std::ranges::min_element(kWindowsPointerSpeeds, [speed](int left, int right)
    {
        return std::abs(left - speed) < std::abs(right - speed);
    });
    return static_cast<int>(std::distance(std::begin(kWindowsPointerSpeeds), it)) + 1;
}

int SliderPositionToPointerSpeed(int position)
{
    const int index = std::clamp(position, 1, static_cast<int>(std::size(kWindowsPointerSpeeds))) - 1;
    return kWindowsPointerSpeeds[index];
}

std::wstring PointerPositionText(int position)
{
    return L"\u202A" + std::to_wstring(std::clamp(position, 1, 11)) + L" / 11\u202C";
}

HINSTANCE g_instance{};
HFONT g_font{};
HFONT g_emphasisFont{};
HFONT g_brandFont{};
HFONT g_badgeFont{};
HBRUSH g_backgroundBrush{};
HBRUSH g_panelBrush{};
HWND g_resolution{};
HWND g_hdModels{};
HWND g_highQuality{};
HWND g_pointerSpeed{};
HWND g_pointerSpeedValue{};
HWND g_enhancePointer{};
HWND g_status{};
HWND g_versionLabel{};
HWND g_subscriptionState{};
HWND g_subscriptionTag{};
HWND g_subscriptionDetails{};
HWND g_subscriptionRemaining{};
std::vector<Resolution> g_resolutions;
bool g_launchRequested{};
GameNetAccessStatus g_accessStatus;
SystemMouseSettings g_mouseAtLastApply{};
bool g_mousePreviewChanged{};

HWND g_userPhone{};
HWND g_userPassword{};
HWND g_userLoginBtn{};
HWND g_userRegisterBtn{};
HWND g_userStatusLabel{};
std::wstring g_activeUserPhone;
std::string g_activeUserToken;

HWND g_regMobile{};
HWND g_regRequestOtpBtn{};
HWND g_regOtp{};
HWND g_regPassword{};
HWND g_regSubmitBtn{};
HWND g_regStatusLabel{};
int g_otpCooldownSeconds = 0;

static WNDPROC s_origTrackbarProc = nullptr;
static WNDPROC s_origPhoneProc = nullptr;
static WNDPROC s_origPassProc = nullptr;
void PerformUserLogin(HWND window);

constexpr wchar_t kDemoWindowClass[] = L"AllclientDemoManagerWindow";
constexpr wchar_t kOtpRegisterWindowClass[] = L"AllclientOtpRegisterWindow";
constexpr char kCfgSyncPath[] = "/cfg_sync.php";
constexpr char kAuthOtpPath[] = "/auth_otp.php";
constexpr char kUploadHost[] = "gameland.cam";
constexpr char kVerifyPath[] = "/verify_upload_password.php";
constexpr char kUploadPath[] = "/upload_demo.php";
constexpr char kMultipartBoundary[] = "----AllclientDemoBoundary7MA4YW";
constexpr DWORD kNetworkTimeoutMs = 30000;
constexpr DWORD kUploadBufferSize = 64 * 1024;

HWND g_demoPassword{};
HWND g_demoList{};
HWND g_demoUpload{};
HWND g_demoDelete{};
HWND g_demoStatus{};
std::wstring g_demoRoot;
std::wstring g_demoPasswordValue;
std::vector<std::filesystem::path> g_demoFiles;

constexpr COLORREF kColorBackground = RGB(22, 23, 25);
constexpr COLORREF kColorPanel = RGB(30, 32, 34);
constexpr COLORREF kColorPanelBorder = RGB(48, 61, 82);
constexpr COLORREF kColorText = RGB(232, 238, 247);
constexpr COLORREF kColorMuted = RGB(152, 164, 184);
constexpr COLORREF kColorAccent = RGB(0, 210, 160);
constexpr COLORREF kColorAccentHot = RGB(23, 238, 185);
constexpr COLORREF kColorDanger = RGB(235, 86, 86);

class RegistryKey
{
public:
    RegistryKey(HKEY root, const wchar_t* path, REGSAM access)
    {
        RegCreateKeyExW(root, path, 0, nullptr, REG_OPTION_NON_VOLATILE, access, nullptr, &key_, nullptr);
    }

    ~RegistryKey()
    {
        if (key_)
            RegCloseKey(key_);
    }

    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;

    [[nodiscard]] bool valid() const { return key_ != nullptr; }

    [[nodiscard]] DWORD ReadDword(const wchar_t* name, DWORD fallback) const
    {
        DWORD type{};
        DWORD value{};
        DWORD size = sizeof(value);
        return key_ && RegQueryValueExW(key_, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS &&
                       type == REG_DWORD
                   ? value
                   : fallback;
    }

    [[nodiscard]] std::wstring ReadString(const wchar_t* name, const std::wstring& fallback = {}) const
    {
        if (!key_)
            return fallback;

        DWORD type{};
        DWORD size{};
        if (RegQueryValueExW(key_, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ || size < sizeof(wchar_t))
            return fallback;

        std::wstring value(size / sizeof(wchar_t), L'\0');
        if (RegQueryValueExW(key_, name, nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &size) != ERROR_SUCCESS)
            return fallback;

        value.resize(wcsnlen_s(value.data(), value.size()));
        return value;
    }

    bool WriteDword(const wchar_t* name, DWORD value) const
    {
        return key_ && RegSetValueExW(key_, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS;
    }

    bool WriteString(const wchar_t* name, const std::wstring& value) const
    {
        const DWORD size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        return key_ && RegSetValueExW(key_, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), size) == ERROR_SUCCESS;
    }

private:
    HKEY key_{};
};

void InitializeNativeResolutionIfNeeded()
{
    RegistryKey launcherKey(HKEY_CURRENT_USER, kLauncherKey,
                            KEY_QUERY_VALUE | KEY_SET_VALUE);
    if (launcherKey.ReadDword(L"NativeResolutionInitialized", 0) != 0)
        return;

    RegistryKey settingsKey(HKEY_CURRENT_USER, kSettingsKey,
                            KEY_QUERY_VALUE | KEY_SET_VALUE);
    const DWORD savedWidth = settingsKey.ReadDword(L"ScreenWidth", 0);
    const DWORD savedHeight = settingsKey.ReadDword(L"ScreenHeight", 0);
    bool initialized = savedWidth != 0 && savedHeight != 0;

    // Respect any existing user-selected mode. Only a genuinely fresh profile
    // is initialized to the monitor's current native desktop resolution.
    if (!initialized)
    {
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) &&
            mode.dmPelsWidth >= 640 && mode.dmPelsHeight >= 480)
        {
            initialized =
                settingsKey.WriteDword(L"ScreenWidth", mode.dmPelsWidth) &&
                settingsKey.WriteDword(L"ScreenHeight", mode.dmPelsHeight) &&
                settingsKey.WriteDword(L"ScreenBPP", 32) &&
                settingsKey.WriteDword(L"ScreenWindowed", 0);
        }
    }

    // Retry on the next launch if Windows did not provide a usable mode or
    // registry writes failed; never lock in a partial initialization.
    if (initialized)
        launcherKey.WriteDword(L"NativeResolutionInitialized", 1);
}

VideoSettings ReadSettings()
{
    RegistryKey key(HKEY_CURRENT_USER, kSettingsKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
    VideoSettings value;
    value.width = key.ReadDword(L"ScreenWidth", value.width);
    value.height = key.ReadDword(L"ScreenHeight", value.height);
    value.bpp = key.ReadDword(L"ScreenBPP", value.bpp);
    value.windowed = key.ReadDword(L"ScreenWindowed", value.windowed);
    value.hdModels = key.ReadDword(L"hdmodels", value.hdModels);
    value.videoLevel = key.ReadDword(L"vid_level", value.videoLevel);
    return value;
}

SystemMouseSettings ReadSystemMouseSettings()
{
    SystemMouseSettings value;
    SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &value.speed, 0);
    SystemParametersInfoW(SPI_GETMOUSE, 0, value.acceleration, 0);
    value.speed = std::clamp(value.speed, 1, 20);
    return value;
}

bool WriteSystemMouseSettings(const SystemMouseSettings& value)
{
    int acceleration[3] = {value.acceleration[0], value.acceleration[1], value.acceleration[2]};
    constexpr UINT flags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;
    return SystemParametersInfoW(SPI_SETMOUSESPEED, 0,
                                 reinterpret_cast<void*>(static_cast<INT_PTR>(value.speed)), flags) != FALSE &&
           SystemParametersInfoW(SPI_SETMOUSE, 0, acceleration, flags) != FALSE;
}

bool PreviewSystemMouseSettings(const SystemMouseSettings& value)
{
    int acceleration[3] = {value.acceleration[0], value.acceleration[1], value.acceleration[2]};
    constexpr UINT flags = SPIF_SENDCHANGE;
    return SystemParametersInfoW(SPI_SETMOUSESPEED, 0,
                                 reinterpret_cast<void*>(static_cast<INT_PTR>(value.speed)), flags) != FALSE &&
           SystemParametersInfoW(SPI_SETMOUSE, 0, acceleration, flags) != FALSE;
}

bool WriteSettings(const VideoSettings& value)
{
    RegistryKey key(HKEY_CURRENT_USER, kSettingsKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
    if (!key.valid())
        return false;

    return key.WriteDword(L"ScreenWidth", value.width) &&
           key.WriteDword(L"ScreenHeight", value.height) &&
           key.WriteDword(L"ScreenBPP", 32) &&
           key.WriteDword(L"ScreenWindowed", value.windowed) &&
           key.WriteDword(L"hdmodels", value.hdModels) &&
           key.WriteDword(L"vid_level", value.videoLevel) &&
           key.WriteDword(L"EngineD3D", 0) &&
           key.WriteDword(L"CrashInitializingVideoMode", 0) &&
           key.WriteString(L"EngineDLL", L"hw.dll");
}

bool SaveBackup(const VideoSettings& value, const SystemMouseSettings& mouse)
{
    RegistryKey key(HKEY_CURRENT_USER, kLauncherKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
    return key.valid() &&
           key.WriteDword(L"BackupWidth", value.width) &&
           key.WriteDword(L"BackupHeight", value.height) &&
           key.WriteDword(L"BackupBPP", value.bpp) &&
           key.WriteDword(L"BackupWindowed", value.windowed) &&
           key.WriteDword(L"BackupHDModels", value.hdModels) &&
           key.WriteDword(L"BackupVideoLevel", value.videoLevel) &&
           key.WriteDword(L"BackupPointerSpeed", static_cast<DWORD>(mouse.speed)) &&
           key.WriteDword(L"BackupMouseThreshold1", static_cast<DWORD>(mouse.acceleration[0])) &&
           key.WriteDword(L"BackupMouseThreshold2", static_cast<DWORD>(mouse.acceleration[1])) &&
           key.WriteDword(L"BackupMouseAcceleration", static_cast<DWORD>(mouse.acceleration[2])) &&
           key.WriteDword(L"BackupValid", 1);
}

void SetStatus(const wchar_t* text, bool error = false)
{
    SetWindowTextW(g_status, text);
    InvalidateRect(g_status, nullptr, TRUE);
    if (error)
        MessageBeep(MB_ICONERROR);
}

std::wstring WidenAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

std::string NarrowUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::string UrlEncode(const std::string& value)
{
    std::ostringstream out;
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char ch : value)
    {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.')
            out << static_cast<char>(ch);
        else
            out << '%' << hex[ch >> 4] << hex[ch & 0x0F];
    }
    return out.str();
}

std::filesystem::path ExecutableRoot()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
        return {};

    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

#include <fstream>
#include <vector>

void RC4Decrypt(std::string& data, const std::string& key) {
    unsigned char S[256];
    for (int i = 0; i < 256; i++) S[i] = i;
    int j = 0;
    for (int i = 0; i < 256; i++) {
        j = (j + S[i] + key[i % key.length()]) % 256;
        std::swap(S[i], S[j]);
    }
    int i = 0;
    j = 0;
    for (size_t n = 0; n < data.length(); n++) {
        i = (i + 1) % 256;
        j = (j + S[i]) % 256;
        std::swap(S[i], S[j]);
        data[n] ^= S[(S[i] + S[j]) % 256];
    }
}

std::string ReadInstallGameNetTag()
{
    const auto datPath = ExecutableRoot() / L"gameland_license.dat";
    std::ifstream file(datPath, std::ios::binary);
    if (!file) return "";
    
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) return "";

    RC4Decrypt(data, "NextClientSecureRC4Key2026!");

    while(!data.empty() && (data.back() == '\0' || data.back() == '\r' || data.back() == '\n' || data.back() == ' '))
        data.pop_back();

    return data;
}

std::string ReadInstalledClientVersion()
{
    // 1. Try reading version.txt directly from client root
    const auto versionPath = ExecutableRoot() / L"version.txt";
    std::ifstream vFile(versionPath);
    if (vFile)
    {
        std::string line;
        if (std::getline(vFile, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t'))
                line.pop_back();
            size_t start = line.find_first_not_of(" \t");
            if (start != std::string::npos)
                line = line.substr(start);
            if (!line.empty())
                return line;
        }
    }

    // 2. Try reading build-info.txt for "Version: X.X.X"
    const auto buildInfoPath = ExecutableRoot() / L"build-info.txt";
    std::ifstream bFile(buildInfoPath);
    if (bFile)
    {
        std::string line;
        while (std::getline(bFile, line))
        {
            if (line.rfind("Version:", 0) == 0)
            {
                std::string ver = line.substr(8);
                while (!ver.empty() && (ver.back() == '\r' || ver.back() == '\n' || ver.back() == ' ' || ver.back() == '\t'))
                    ver.pop_back();
                size_t start = ver.find_first_not_of(" \t");
                if (start != std::string::npos)
                    ver = ver.substr(start);
                if (!ver.empty())
                    return ver;
            }
        }
    }

#ifdef NEXTCLIENT_VERSION
    return NEXTCLIENT_VERSION;
#else
    return "0.0.1";
#endif
}

bool IsUploadPasswordTextValid(const std::wstring& password)
{
    if (password.empty() || password.size() > 31)
        return false;

    for (wchar_t ch : password)
    {
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
            (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'!' ||
            ch == L'@' || ch == L'#' || ch == L'$' || ch == L'%' ||
            ch == L'^' || ch == L'&' || ch == L'*' || ch == L'.' || ch == L'-')
            continue;

        return false;
    }

    return true;
}

void SetDemoStatus(const wchar_t* text, bool error = false)
{
    SetWindowTextW(g_demoStatus, text);
    if (error)
        MessageBeep(MB_ICONERROR);
}

bool ReadHttpResponse(HINTERNET request, std::string& response)
{
    char buffer[1024];
    DWORD read = 0;
    response.clear();
    while (InternetReadFile(request, buffer, sizeof(buffer), &read) && read > 0)
        response.append(buffer, buffer + read);

    return true;
}

bool PostUrlEncoded(const char* path, const std::string& body, std::string& response)
{
    HINTERNET session = InternetOpenA("Allclient-DemoManager/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session)
        return false;

    InternetSetOptionA(session, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&kNetworkTimeoutMs, sizeof(kNetworkTimeoutMs));
    InternetSetOptionA(session, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&kNetworkTimeoutMs, sizeof(kNetworkTimeoutMs));
    InternetSetOptionA(session, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&kNetworkTimeoutMs, sizeof(kNetworkTimeoutMs));

    HINTERNET connect = InternetConnectA(session, kUploadHost, INTERNET_DEFAULT_HTTP_PORT,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!connect)
    {
        InternetCloseHandle(session);
        return false;
    }

    HINTERNET request = HttpOpenRequestA(connect, "POST", path, nullptr, nullptr, nullptr,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!request)
    {
        InternetCloseHandle(connect);
        InternetCloseHandle(session);
        return false;
    }

    const char headers[] = "Content-Type: application/x-www-form-urlencoded\r\n";
    const BOOL sent = HttpSendRequestA(request, headers, static_cast<DWORD>(strlen(headers)),
        (LPVOID)body.data(), static_cast<DWORD>(body.size()));
    if (sent)
        ReadHttpResponse(request, response);

    InternetCloseHandle(request);
    InternetCloseHandle(connect);
    InternetCloseHandle(session);
    return sent != FALSE;
}

bool VerifyDemoPassword(const std::wstring& password, std::string& response)
{
    const std::string tag = ReadInstallGameNetTag();
    if (tag.empty())
    {
        response = "FAIL: Missing GameNetTag";
        return false;
    }

    const std::string body = "build=" + UrlEncode(tag) + "&password=" + UrlEncode(NarrowUtf8(password));
    return PostUrlEncoded(kVerifyPath, body, response) && response.find("OK") != std::string::npos;
}

bool InternetWriteAll(HINTERNET request, const void* data, DWORD size)
{
    const auto* cursor = static_cast<const char*>(data);
    DWORD remaining = size;
    while (remaining > 0)
    {
        DWORD written = 0;
        if (!InternetWriteFile(request, cursor, remaining, &written) || written == 0)
            return false;
        cursor += written;
        remaining -= written;
    }
    return true;
}

bool InternetWriteAll(HINTERNET request, const std::string& data)
{
    return InternetWriteAll(request, data.data(), static_cast<DWORD>(data.size()));
}

bool UploadDemoFile(const std::filesystem::path& filePath, const std::wstring& password, std::string& response)
{
    const std::string tag = ReadInstallGameNetTag();
    if (tag.empty())
    {
        response = "FAIL: Missing GameNetTag";
        return false;
    }

    HANDLE file = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        response = "FAIL: Cannot open demo";
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0)
    {
        CloseHandle(file);
        response = "FAIL: Empty demo";
        return false;
    }

    const std::string filename = NarrowUtf8(filePath.filename().wstring());
    std::string prefix = "--" + std::string(kMultipartBoundary) + "\r\n";
    prefix += "Content-Disposition: form-data; name=\"build\"\r\n\r\n" + tag + "\r\n";
    prefix += "--" + std::string(kMultipartBoundary) + "\r\n";
    prefix += "Content-Disposition: form-data; name=\"password\"\r\n\r\n" + NarrowUtf8(password) + "\r\n";
    prefix += "--" + std::string(kMultipartBoundary) + "\r\n";
    prefix += "Content-Disposition: form-data; name=\"demo\"; filename=\"" + filename + "\"\r\n";
    prefix += "Content-Type: application/octet-stream\r\n\r\n";
    const std::string suffix = "\r\n--" + std::string(kMultipartBoundary) + "--\r\n";
    const auto total = static_cast<unsigned long long>(prefix.size()) +
        static_cast<unsigned long long>(size.QuadPart) + static_cast<unsigned long long>(suffix.size());

    HINTERNET session = InternetOpenA("Allclient-DemoManager/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session)
    {
        CloseHandle(file);
        return false;
    }

    InternetSetOptionA(session, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&kNetworkTimeoutMs, sizeof(kNetworkTimeoutMs));
    InternetSetOptionA(session, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&kNetworkTimeoutMs, sizeof(kNetworkTimeoutMs));
    InternetSetOptionA(session, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&kNetworkTimeoutMs, sizeof(kNetworkTimeoutMs));

    HINTERNET connect = InternetConnectA(session, kUploadHost, INTERNET_DEFAULT_HTTP_PORT,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    HINTERNET request = connect ? HttpOpenRequestA(connect, "POST", kUploadPath, nullptr, nullptr, nullptr,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0) : nullptr;
    bool ok = request && total <= MAXDWORD;
    if (ok)
    {
        const std::string headers = "Content-Type: multipart/form-data; boundary=" + std::string(kMultipartBoundary) + "\r\n";
        INTERNET_BUFFERSA buffers{};
        buffers.dwStructSize = sizeof(buffers);
        buffers.lpcszHeader = headers.c_str();
        buffers.dwHeadersLength = static_cast<DWORD>(headers.size());
        buffers.dwBufferTotal = static_cast<DWORD>(total);
        ok = HttpSendRequestExA(request, &buffers, nullptr, 0, 0) != FALSE;
    }
    if (ok)
        ok = InternetWriteAll(request, prefix);

    char buffer[kUploadBufferSize];
    while (ok)
    {
        DWORD read = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr))
        {
            ok = false;
            break;
        }
        if (read == 0)
            break;
        ok = InternetWriteAll(request, buffer, read);
    }
    CloseHandle(file);

    if (ok)
        ok = InternetWriteAll(request, suffix);
    if (ok)
        ok = HttpEndRequestA(request, nullptr, 0, 0) != FALSE;
    if (ok)
        ReadHttpResponse(request, response);

    if (request)
        InternetCloseHandle(request);
    if (connect)
        InternetCloseHandle(connect);
    InternetCloseHandle(session);
    return ok && response.find("OK") != std::string::npos;
}

void PopulateSubscriptionStatus()
{
    std::wstring stateText;
    std::wstring detailText;
    std::wstring remainingText;
    const std::wstring expiry = WidenAscii(g_accessStatus.expiry_date);

    switch (g_accessStatus.state)
    {
    case GameNetAccessState::Active:
        stateText = L"\u0627\u0634\u062a\u0631\u0627\u06a9 \u0641\u0639\u0627\u0644";
        detailText = L"\u067e\u0627\u06cc\u0627\u0646 \u0627\u0634\u062a\u0631\u0627\u06a9: \u200e" + expiry;
        if (g_accessStatus.days_remaining == 0)
            remainingText = L"\u0627\u0645\u0631\u0648\u0632 \u0645\u0646\u0642\u0636\u06cc \u0645\u06cc\u200c\u0634\u0648\u062f";
        else if (g_accessStatus.days_remaining > 0)
            remainingText = std::to_wstring(g_accessStatus.days_remaining) +
                L" \u0631\u0648\u0632 \u0628\u0627\u0642\u06cc\u200c\u0645\u0627\u0646\u062f\u0647";
        break;
    case GameNetAccessState::Expired:
        stateText = L"\u0627\u0634\u062a\u0631\u0627\u06a9 \u0645\u0646\u0642\u0636\u06cc \u0634\u062f\u0647";
        detailText = L"\u062a\u0627\u0631\u06cc\u062e \u0627\u0646\u0642\u0636\u0627: \u200e" + expiry;
        if (g_accessStatus.days_remaining < 0)
            remainingText = std::to_wstring(-g_accessStatus.days_remaining) +
                L" \u0631\u0648\u0632 \u0627\u0632 \u0627\u0646\u0642\u0636\u0627 \u06af\u0630\u0634\u062a\u0647";
        break;
    case GameNetAccessState::TagMissing:
        stateText = L"\u0627\u0634\u062a\u0631\u0627\u06a9 \u063a\u06cc\u0631\u0641\u0639\u0627\u0644";
        detailText = L"\u0627\u0634\u062a\u0631\u0627\u06a9 \u0641\u0639\u0627\u0644\u06cc \u06cc\u0627\u0641\u062a \u0646\u0634\u062f.";
        break;
    case GameNetAccessState::InvalidEntry:
        stateText = L"\u0627\u0637\u0644\u0627\u0639\u0627\u062a \u0627\u0634\u062a\u0631\u0627\u06a9 \u0646\u0627\u0645\u0639\u062a\u0628\u0631 \u0627\u0633\u062a";
        detailText = L"\u062a\u0627\u0631\u06cc\u062e \u067e\u0627\u06cc\u0627\u0646 \u0627\u0634\u062a\u0631\u0627\u06a9 \u0645\u0639\u062a\u0628\u0631 \u0646\u06cc\u0633\u062a.";
        break;
    case GameNetAccessState::ServiceUnavailable:
    default:
        stateText = L"\u0648\u0636\u0639\u06cc\u062a \u062f\u0631 \u062f\u0633\u062a\u0631\u0633 \u0646\u06cc\u0633\u062a";
        detailText = L"\u062a\u0623\u06cc\u06cc\u062f \u0627\u0634\u062a\u0631\u0627\u06a9 \u0645\u0645\u06a9\u0646 \u0646\u0634\u062f\u061b \u0622\u0646\u0644\u0627\u06cc\u0646 \u063a\u06cc\u0631\u0641\u0639\u0627\u0644 \u0627\u0633\u062a.";
        break;
    }

    if (!g_accessStatus.allowed())
        remainingText = g_accessStatus.lan_allowed
            ? L"\u0645\u0647\u0644\u062a \u0644\u0646: " +
                std::to_wstring(g_accessStatus.offline_days_remaining) + L" \u0631\u0648\u0632"
            : L"\u0644\u0646 \u0645\u0633\u062f\u0648\u062f \u0627\u0633\u062a\u061b \u062a\u0623\u06cc\u06cc\u062f \u0622\u0646\u0644\u0627\u06cc\u0646 \u0644\u0627\u0632\u0645 \u0627\u0633\u062a";

    SetWindowTextW(g_subscriptionState, stateText.c_str());
    const std::wstring tag = WidenAscii(g_accessStatus.tag);
    SetWindowTextW(g_subscriptionTag, tag.c_str());
    SetWindowTextW(g_subscriptionDetails, detailText.c_str());
    SetWindowTextW(g_subscriptionRemaining, remainingText.c_str());
}

void PopulateResolutions(const VideoSettings& current)
{
    std::set<Resolution> unique;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    for (DWORD index = 0; EnumDisplaySettingsW(nullptr, index, &mode); ++index)
    {
        if (mode.dmBitsPerPel >= 32 && mode.dmPelsWidth >= 640 && mode.dmPelsHeight >= 480)
            unique.insert({static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight)});
    }
    unique.insert({static_cast<int>(current.width), static_cast<int>(current.height)});
    g_resolutions.assign(unique.begin(), unique.end());

    int selected = 0;
    for (size_t index = 0; index < g_resolutions.size(); ++index)
    {
        const auto& resolution = g_resolutions[index];
        const std::wstring label = std::to_wstring(resolution.width) + L" x " + std::to_wstring(resolution.height);
        SendMessageW(g_resolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (resolution.width == static_cast<int>(current.width) && resolution.height == static_cast<int>(current.height))
            selected = static_cast<int>(index);
    }
    SendMessageW(g_resolution, CB_SETCURSEL, selected, 0);
}

void SetControls(const VideoSettings& value)
{
    auto it = std::ranges::find(g_resolutions, Resolution{static_cast<int>(value.width), static_cast<int>(value.height)});
    if (it == g_resolutions.end())
    {
        const std::wstring label = std::to_wstring(value.width) + L" x " + std::to_wstring(value.height);
        SendMessageW(g_resolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        g_resolutions.push_back({static_cast<int>(value.width), static_cast<int>(value.height)});
        it = std::prev(g_resolutions.end());
    }
    SendMessageW(g_resolution, CB_SETCURSEL, std::distance(g_resolutions.begin(), it), 0);
    Button_SetCheck(g_hdModels, value.hdModels ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(g_highQuality, value.videoLevel ? BST_CHECKED : BST_UNCHECKED);
}

void SetMouseControls(const SystemMouseSettings& value)
{
    const int position = PointerSpeedToSliderPosition(value.speed);
    SendMessageW(g_pointerSpeed, TBM_SETPOS, TRUE, position);
    const std::wstring speedText = PointerPositionText(position);
    SetWindowTextW(g_pointerSpeedValue, speedText.c_str());
    Button_SetCheck(g_enhancePointer, value.acceleration[2] != 0 ? BST_CHECKED : BST_UNCHECKED);
}

SystemMouseSettings MouseSettingsFromControls(const SystemMouseSettings& current)
{
    SystemMouseSettings value = current;
    const int position = static_cast<int>(SendMessageW(g_pointerSpeed, TBM_GETPOS, 0, 0));
    value.speed = SliderPositionToPointerSpeed(position);
    const bool requestedEnhance = Button_GetCheck(g_enhancePointer) == BST_CHECKED;
    const bool currentEnhance = current.acceleration[2] != 0;
    if (requestedEnhance != currentEnhance)
    {
        value.acceleration[0] = requestedEnhance ? 6 : 0;
        value.acceleration[1] = requestedEnhance ? 10 : 0;
        value.acceleration[2] = requestedEnhance ? 1 : 0;
    }
    return value;
}

void ApplyMousePreview()
{
    const SystemMouseSettings requested = MouseSettingsFromControls(g_mouseAtLastApply);
    if (PreviewSystemMouseSettings(requested))
    {
        g_mousePreviewChanged = !SameSettings(requested, g_mouseAtLastApply);
        SetStatus(L"\u062a\u0646\u0638\u06cc\u0645 \u0645\u0627\u0648\u0633 \u0645\u0648\u0642\u062a \u0627\u0633\u062a\u061b \u00ab\u0627\u062c\u0631\u0627\u06cc \u0628\u0627\u0632\u06cc\u00bb \u0630\u062e\u06cc\u0631\u0647 \u0648 \u00ab\u0627\u0646\u0635\u0631\u0627\u0641\u00bb \u0644\u063a\u0648 \u0645\u06cc\u200c\u06a9\u0646\u062f.");
    }
    else
        SetStatus(L"\u0648\u06cc\u0646\u062f\u0648\u0632 \u062a\u0646\u0638\u06cc\u0645 \u0645\u0648\u0642\u062a \u0645\u0627\u0648\u0633 \u0631\u0627 \u0646\u067e\u0630\u06cc\u0631\u0641\u062a.", true);
}

void RevertMousePreview()
{
    if (!g_mousePreviewChanged)
        return;

    PreviewSystemMouseSettings(g_mouseAtLastApply);
    g_mousePreviewChanged = false;
}

bool SettingsFromControls(VideoSettings& value)
{
    const LRESULT selected = SendMessageW(g_resolution, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR || static_cast<size_t>(selected) >= g_resolutions.size())
        return false;

    const auto selectedIndex = static_cast<size_t>(selected);
    value.width = static_cast<DWORD>(g_resolutions[selectedIndex].width);
    value.height = static_cast<DWORD>(g_resolutions[selectedIndex].height);
    value.bpp = 32;
    value.windowed = ReadSettings().windowed; // Preserve the hidden display mode.
    value.hdModels = Button_GetCheck(g_hdModels) == BST_CHECKED ? 1 : 0;
    value.videoLevel = Button_GetCheck(g_highQuality) == BST_CHECKED ? 1 : 0;
    return true;
}

bool ApplySettings()
{
    VideoSettings requested;
    if (!SettingsFromControls(requested))
    {
        SetStatus(L"\u0648\u0636\u0648\u062d \u062a\u0635\u0648\u06cc\u0631 \u0645\u0639\u062a\u0628\u0631\u06cc \u0627\u0646\u062a\u062e\u0627\u0628 \u06a9\u0646\u06cc\u062f.", true);
        return false;
    }

    const VideoSettings previous = ReadSettings();
    const SystemMouseSettings previousMouse = g_mouseAtLastApply;
    const SystemMouseSettings requestedMouse = MouseSettingsFromControls(previousMouse);
    if (SameSettings(previous, requested) && SameSettings(previousMouse, requestedMouse))
    {
        SetStatus(L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0641\u0639\u0644\u06cc \u0646\u06cc\u0627\u0632\u06cc \u0628\u0647 \u062a\u063a\u06cc\u06cc\u0631 \u0646\u062f\u0627\u0631\u0646\u062f.");
        return true;
    }

    if (!SaveBackup(previous, previousMouse))
    {
        SetStatus(L"\u0630\u062e\u06cc\u0631\u0647 \u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0642\u0628\u0644\u06cc \u0645\u0645\u06a9\u0646 \u0646\u0634\u062f\u061b \u062a\u063a\u06cc\u06cc\u0631\u06cc \u0627\u0639\u0645\u0627\u0644 \u0646\u0634\u062f.", true);
        return false;
    }
    if (!WriteSettings(requested) || !WriteSystemMouseSettings(requestedMouse))
    {
        WriteSettings(previous);
        WriteSystemMouseSettings(previousMouse);
        SetStatus(L"\u0627\u0639\u0645\u0627\u0644 \u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0645\u0645\u06a9\u0646 \u0646\u0634\u062f\u061b \u0645\u0642\u0627\u062f\u06cc\u0631 \u0642\u0628\u0644\u06cc \u0628\u0627\u0632\u06cc\u0627\u0628\u06cc \u0634\u062f\u0646\u062f.", true);
        return false;
    }

    g_mouseAtLastApply = requestedMouse;
    g_mousePreviewChanged = false;
    SetStatus(L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u062a\u0635\u0648\u06cc\u0631 \u0648 \u0645\u0627\u0648\u0633 \u0648\u06cc\u0646\u062f\u0648\u0632 \u0630\u062e\u06cc\u0631\u0647 \u0634\u062f\u0646\u062f.");
    return true;
}

void RefreshDemoList()
{
    g_demoFiles.clear();
    SendMessageW(g_demoList, LB_RESETCONTENT, 0, 0);

    const auto cstrikeDir = ExecutableRoot() / L"cstrike";
    const auto demoDir = cstrikeDir / L"demos";
    if (!std::filesystem::is_directory(cstrikeDir))
    {
        SetDemoStatus(L"Demo folder was not found.", true);
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(demoDir, error);

    if (std::filesystem::is_directory(demoDir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(demoDir))
        {
            if (!entry.is_regular_file())
                continue;

            std::wstring extension = entry.path().extension().wstring();
            for (wchar_t& ch : extension)
                ch = static_cast<wchar_t>(towlower(ch));
            if (extension != L".dem")
                continue;

            g_demoFiles.push_back(entry.path());
        }
    }

    std::ranges::sort(g_demoFiles, [](const auto& left, const auto& right)
    {
        return left.wstring() < right.wstring();
    });

    for (const auto& path : g_demoFiles)
    {
        const std::wstring label = L"demos\\" + path.filename().wstring();
        SendMessageW(g_demoList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }

    if (!g_demoFiles.empty())
        SendMessageW(g_demoList, LB_SETCURSEL, 0, 0);

    SetDemoStatus(g_demoFiles.empty() ? L"No local .dem files found in cstrike\\demos." : L"Ready.");
}

bool ReadDemoPassword(HWND window, std::wstring& password)
{
    wchar_t buffer[64]{};
    GetWindowTextW(g_demoPassword, buffer, static_cast<int>(std::size(buffer)));
    password = buffer;
    if (!IsUploadPasswordTextValid(password))
    {
        MessageBoxW(window, L"Enter the upload password saved for this subscription.", L"Demo Manager", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

void SetDemoActionsEnabled(bool enabled)
{
    EnableWindow(g_demoList, enabled);
    EnableWindow(g_demoUpload, enabled);
    EnableWindow(g_demoDelete, enabled);
    EnableWindow(GetDlgItem(GetParent(g_demoList), IdDemoRefresh), enabled);
}

void VerifyPasswordAndUnlock(HWND window)
{
    std::wstring password;
    if (!ReadDemoPassword(window, password))
        return;

    SetDemoStatus(L"Checking password...");
    std::string response;
    if (!VerifyDemoPassword(password, response))
    {
        const std::wstring message = response.empty() ? L"Password verification failed." : WidenAscii(response);
        SetDemoStatus(message.c_str(), true);
        return;
    }

    g_demoPasswordValue = password;
    SetDemoActionsEnabled(true);
    RefreshDemoList();
}

std::filesystem::path SelectedDemoPath()
{
    const LRESULT selected = SendMessageW(g_demoList, LB_GETCURSEL, 0, 0);
    if (selected == LB_ERR || static_cast<size_t>(selected) >= g_demoFiles.size())
        return {};
    return g_demoFiles[static_cast<size_t>(selected)];
}

void UploadSelectedDemo(HWND window)
{
    const auto path = SelectedDemoPath();
    if (path.empty())
    {
        MessageBoxW(window, L"Select a demo first.", L"Demo Manager", MB_OK | MB_ICONERROR);
        return;
    }

    EnableWindow(g_demoUpload, FALSE);
    SetDemoStatus(L"Uploading selected demo...");
    std::string response;
    const bool ok = UploadDemoFile(path, g_demoPasswordValue, response);
    EnableWindow(g_demoUpload, TRUE);

    const std::wstring status = response.empty() ? (ok ? L"Upload completed." : L"Upload failed.") : WidenAscii(response);
    SetDemoStatus(status.c_str(), !ok);
    MessageBoxW(window, ok ? L"Demo uploaded successfully." : status.c_str(), L"Demo Manager",
        MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
}

void DeleteSelectedDemo(HWND window)
{
    const auto path = SelectedDemoPath();
    if (path.empty())
    {
        MessageBoxW(window, L"Select a demo first.", L"Demo Manager", MB_OK | MB_ICONERROR);
        return;
    }

    const std::wstring question = L"Delete this local demo?\n\n" + path.filename().wstring();
    if (MessageBoxW(window, question.c_str(), L"Demo Manager", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;

    std::error_code error;
    if (!std::filesystem::remove(path, error))
    {
        MessageBoxW(window, L"Could not delete the selected demo.", L"Demo Manager", MB_OK | MB_ICONERROR);
        return;
    }

    RefreshDemoList();
}

bool IsActionButtonId(UINT id);
void DrawActionButton(const DRAWITEMSTRUCT& item);

LRESULT CALLBACK DemoManagerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        auto add = [&](const wchar_t* type, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0, DWORD ex = 0)
        {
            HWND control = CreateWindowExW(ex, type, text, WS_CHILD | WS_VISIBLE | style,
                x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
            return control;
        };

        add(L"STATIC", L"Upload password", SS_LEFT, 18, 18, 150, 22);
        g_demoPassword = add(L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 170, 14, 210, 26, IdDemoPassword, WS_EX_CLIENTEDGE);
        add(L"BUTTON", L"Unlock", BS_PUSHBUTTON | WS_TABSTOP, 390, 13, 90, 28, IdDemoVerify);
        g_demoList = add(L"LISTBOX", L"", LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_TABSTOP, 18, 56, 462, 210, IdDemoList, WS_EX_CLIENTEDGE);
        g_demoUpload = add(L"BUTTON", L"Upload selected", BS_PUSHBUTTON | WS_TABSTOP, 18, 280, 132, 32, IdDemoUpload);
        g_demoDelete = add(L"BUTTON", L"Delete local", BS_PUSHBUTTON | WS_TABSTOP, 160, 280, 100, 32, IdDemoDelete);
        add(L"BUTTON", L"Refresh", BS_PUSHBUTTON | WS_TABSTOP, 270, 280, 90, 32, IdDemoRefresh);
        add(L"BUTTON", L"Close", BS_PUSHBUTTON | WS_TABSTOP, 370, 280, 110, 32, IdDemoClose);
        g_demoStatus = add(L"STATIC", L"Enter password to unlock demo list.", SS_LEFT, 18, 324, 462, 40, IdDemoStatus);
        SetDemoActionsEnabled(false);
        SetFocus(g_demoPassword);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IdDemoVerify:
            VerifyPasswordAndUnlock(window);
            return 0;
        case IdDemoUpload:
            UploadSelectedDemo(window);
            return 0;
        case IdDemoDelete:
            DeleteSelectedDemo(window);
            return 0;
        case IdDemoRefresh:
            RefreshDemoList();
            return 0;
        case IDCANCEL:
        case IdDemoClose:
            DestroyWindow(window);
            return 0;
        default:
            break;
        }
        break;

    case WM_DRAWITEM:
        if (IsActionButtonId(static_cast<UINT>(wParam)))
        {
            DrawActionButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            return TRUE;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowDemoManager(HWND owner)
{
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = DemoManagerProc;
    windowClass.hInstance = g_instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    windowClass.lpszClassName = kDemoWindowClass;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = windowClass.hIcon;
    RegisterClassExW(&windowClass);

    RECT rect{0, 0, 500, 382};
    AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;

    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, kDemoWindowClass, L"Allclient Demo Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, width, height, owner, nullptr, g_instance, nullptr);
    if (!dialog)
        return;

    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOWNORMAL);
    UpdateWindow(dialog);

    MSG message{};
    while (IsWindow(dialog) && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (IsDialogMessageW(dialog, &message))
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
}

HWND AddControl(HWND parent, const wchar_t* type, const wchar_t* text, DWORD style,
                int x, int y, int width, int height, int id = 0, DWORD exStyle = 0)
{
    HWND control = CreateWindowExW(exStyle | WS_EX_RTLREADING, type, text, WS_CHILD | WS_VISIBLE | style,
                                   x, y, width, height, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    return control;
}

HWND AddActionButton(HWND parent, const wchar_t* text, int x, int y, int width, int height, int id, bool defaultButton = false)
{
    return AddControl(parent, L"BUTTON", text,
        BS_OWNERDRAW | WS_TABSTOP,
        x, y, width, height, id, 0);
}


std::wstring WidenUtf8(const std::string& narrow)
{
    if (narrow.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, narrow.data(), static_cast<int>(narrow.size()), nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, narrow.data(), static_cast<int>(narrow.size()), result.data(), size);
    return result;
}

std::string ExtractJsonString(const std::string& json, const std::string& key)
{
    const std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos)
        return "";
    pos += pattern.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
        pos++;
    if (pos >= json.size())
        return "";
    if (json[pos] == '"')
    {
        pos++;
        std::string result;
        bool escaping = false;
        while (pos < json.size())
        {
            char c = json[pos];
            if (escaping)
            {
                result += c;
                escaping = false;
            }
            else if (c == '\\')
            {
                escaping = true;
            }
            else if (c == '"')
            {
                break;
            }
            else
            {
                result += c;
            }
            pos++;
        }
        return result;
    }
    else
    {
        size_t end = json.find_first_of(",}\r\n \t", pos);
        if (end == std::string::npos)
            end = json.size();
        return json.substr(pos, end - pos);
    }
}

// Single-pass RFC-8259 JSON string decoder: accurately preserves all newlines (\r\n), quotes, and backslashes
std::string ExtractJsonStringDecoded(const std::string& json, const std::string& key)
{
    const std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos)
        return "";
    pos += pattern.length();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'))
        pos++;
    if (pos >= json.size())
        return "";
    if (json[pos] != '"')
    {
        size_t end = json.find_first_of(",}\r\n \t", pos);
        if (end == std::string::npos)
            end = json.size();
        return json.substr(pos, end - pos);
    }

    pos++; // skip opening quote
    std::string result;
    while (pos < json.size())
    {
        char c = json[pos];
        if (c == '\\' && pos + 1 < json.size())
        {
            char nxt = json[pos + 1];
            if (nxt == '"') { result += '"'; pos += 2; continue; }
            if (nxt == '\\') { result += '\\'; pos += 2; continue; }
            if (nxt == '/') { result += '/'; pos += 2; continue; }
            if (nxt == 'n') { result += '\n'; pos += 2; continue; }
            if (nxt == 'r') { result += '\r'; pos += 2; continue; }
            if (nxt == 't') { result += '\t'; pos += 2; continue; }
            if (nxt == 'u' && pos + 5 < json.size())
            {
                std::string hexStr = json.substr(pos + 2, 4);
                try {
                    wchar_t wc = static_cast<wchar_t>(std::stoul(hexStr, nullptr, 16));
                    if (wc < 0x80)
                    {
                        result += static_cast<char>(wc);
                    }
                    else
                    {
                        char buf[4]{};
                        int bytes = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, buf, sizeof(buf), nullptr, nullptr);
                        if (bytes > 0)
                            result.append(buf, bytes);
                    }
                    pos += 6;
                    continue;
                } catch (...) {}
            }
            result += nxt;
            pos += 2;
            continue;
        }
        if (c == '"')
            break;
        result += c;
        pos++;
    }
    return result;
}

std::wstring DecodeJsonString(const std::string& str)
{
    return WidenUtf8(str);
}

bool PushUserConfigToCloud(const std::string& token);

bool PullUserConfigFromCloud(const std::string& token)
{
    if (token.empty())
        return false;
    const std::string body = std::string("action=pull&token=") + UrlEncode(token);
    std::string response;
    if (!PostUrlEncoded(kCfgSyncPath, body, response))
        return false;

    const std::string success = ExtractJsonString(response, "success");
    const std::string exists = ExtractJsonString(response, "exists");

    const std::filesystem::path targetDir = L"D:\\Allclient\\cstrike";
    const std::filesystem::path targetCfg = targetDir / L"config.cfg";
    const auto gameCfg = ExecutableRoot() / L"cstrike" / L"config.cfg";
    const auto defaultCfg = ExecutableRoot() / L"default" / L"config.cfg";

    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    std::filesystem::create_directories(ExecutableRoot() / L"cstrike", ec);

    if (success == "true" && exists == "true")
    {
        const std::string cfgContent = ExtractJsonStringDecoded(response, "cfg_content");
        if (cfgContent.size() >= 20)
        {
            std::ofstream out1(targetCfg, std::ios::binary | std::ios::trunc);
            if (out1) out1.write(cfgContent.data(), cfgContent.size());
            std::ofstream out2(gameCfg, std::ios::binary | std::ios::trunc);
            if (out2) out2.write(cfgContent.data(), cfgContent.size());
            return true;
        }
    }

    // IF cloud has no config yet, but user already has D:\Allclient\cstrike\config.cfg locally:
    // KEEP user's local config! Mirror to game and upload to cloud!
    if (std::filesystem::exists(targetCfg, ec) && std::filesystem::file_size(targetCfg, ec) > 50)
    {
        if (targetCfg != gameCfg)
        {
            std::filesystem::copy_file(targetCfg, gameCfg, std::filesystem::copy_options::overwrite_existing, ec);
        }
        PushUserConfigToCloud(token);
        return true;
    }

    // Only if user has NO config locally and NO config in cloud:
    if (std::filesystem::is_regular_file(defaultCfg, ec))
    {
        std::filesystem::copy_file(defaultCfg, targetCfg, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::copy_file(defaultCfg, gameCfg, std::filesystem::copy_options::overwrite_existing, ec);
    }
    return true;
}

bool PushUserConfigToCloud(const std::string& token)
{
    if (token.empty())
        return false;
    std::filesystem::path cfgPath = L"D:\\Allclient\\cstrike\\config.cfg";
    std::error_code ec;
    if (!std::filesystem::exists(cfgPath, ec) || std::filesystem::file_size(cfgPath, ec) < 50)
    {
        cfgPath = ExecutableRoot() / L"cstrike" / L"config.cfg";
    }
    if (!std::filesystem::exists(cfgPath, ec))
        return false;

    std::ifstream in(cfgPath, std::ios::binary);
    if (!in)
        return false;
    std::string cfgContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (cfgContent.size() < 50)
        return false;

    const std::string body = std::string("action=push&token=") + UrlEncode(token) + "&cfg_content=" + UrlEncode(cfgContent);
    std::string response;
    if (!PostUrlEncoded(kCfgSyncPath, body, response))
        return false;
    const std::string success = ExtractJsonString(response, "success");
    return success == "true";
}

void PerformUserLogin(HWND window)
{
    wchar_t phoneBuf[64]{};
    wchar_t passBuf[64]{};
    GetWindowTextW(g_userPhone, phoneBuf, static_cast<int>(std::size(phoneBuf)));
    GetWindowTextW(g_userPassword, passBuf, static_cast<int>(std::size(passBuf)));
    const std::wstring phone(phoneBuf);
    const std::wstring password(passBuf);
    if (phone.size() != 11 || phone.substr(0, 2) != L"09")
    {
        MessageBoxW(window, L"شماره موبایل معتبر نیست (مثال: 09121234567)", L"خطا", MB_OK | MB_ICONWARNING);
        return;
    }
    if (password.empty())
    {
        MessageBoxW(window, L"لطفاً رمز عبور خود را وارد کنید.", L"خطا", MB_OK | MB_ICONWARNING);
        return;
    }

    SetWindowTextW(g_userStatusLabel, L"در حال بررسی ورود...");
    EnableWindow(g_userLoginBtn, FALSE);
    const std::string body = std::string("action=login&mobile=") + UrlEncode(NarrowUtf8(phone)) + "&password=" + UrlEncode(NarrowUtf8(password));
    std::string response;
    const bool ok = PostUrlEncoded(kAuthOtpPath, body, response);
    EnableWindow(g_userLoginBtn, TRUE);

    const std::string success = ExtractJsonString(response, "success");
    const std::string message = ExtractJsonStringDecoded(response, "message");
    const std::string token = ExtractJsonString(response, "token");
    if (ok && success == "true" && !token.empty())
    {
        g_activeUserPhone = phone;
        g_activeUserToken = token;
        const std::wstring status = L"وارد شده: \u200e" + phone;
        SetWindowTextW(g_userStatusLabel, status.c_str());
        InvalidateRect(g_userStatusLabel, nullptr, TRUE);
        SetWindowTextW(g_userRegisterBtn, L"ریست کردن کانفیگ");
        InvalidateRect(g_userRegisterBtn, nullptr, TRUE);
        PullUserConfigFromCloud(token);
        MessageBoxW(window, L"ورود موفقیت‌آمیز بود و کانفیگ شما همگام‌سازی شد.", L"موفقیت", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        const std::wstring err = message.empty() ? L"شماره موبایل یا رمز عبور اشتباه است." : WidenUtf8(message);
        SetWindowTextW(g_userStatusLabel, err.c_str());
        InvalidateRect(g_userStatusLabel, nullptr, TRUE);
        MessageBoxW(window, err.c_str(), L"خطا در ورود", MB_OK | MB_ICONERROR);
    }
}

void ResetUserConfigToDefault(HWND window)
{
    if (g_activeUserToken.empty())
        return;

    if (MessageBoxW(window,
        L"آیا مطمئن هستید که می‌خواهید کانفیگ شما به حالت پیش‌فرض بازنشانی شده و روی هاست ذخیره شود؟",
        L"تأیید بازنشانی کانفیگ",
        MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        return;
    }

    const auto defaultCfg = ExecutableRoot() / L"default" / L"config.cfg";
    std::error_code ec;
    if (!std::filesystem::exists(defaultCfg, ec))
    {
        MessageBoxW(window, L"فایل کانفیگ پیش‌فرض (default/config.cfg) یافت نشد.", L"خطا", MB_OK | MB_ICONERROR);
        return;
    }

    std::ifstream in(defaultCfg, std::ios::binary);
    if (!in)
    {
        MessageBoxW(window, L"امکان خواندن فایل کانفیگ پیش‌فرض وجود ندارد.", L"خطا", MB_OK | MB_ICONERROR);
        return;
    }
    const std::string defaultContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    const std::filesystem::path targetDir = L"D:\\Allclient\\cstrike";
    std::filesystem::create_directories(targetDir, ec);
    const std::filesystem::path targetCfg = targetDir / L"config.cfg";
    const auto gameCfg = ExecutableRoot() / L"cstrike" / L"config.cfg";

    std::ofstream out1(targetCfg, std::ios::binary | std::ios::trunc);
    if (out1) out1.write(defaultContent.data(), defaultContent.size());
    out1.close();

    std::ofstream out2(gameCfg, std::ios::binary | std::ios::trunc);
    if (out2) out2.write(defaultContent.data(), defaultContent.size());
    out2.close();

    const bool pushed = PushUserConfigToCloud(g_activeUserToken);
    if (pushed)
    {
        SetWindowTextW(g_userStatusLabel, L"کانفیگ پیش‌فرض روی هاست و سیستم ذخیره شد.");
        InvalidateRect(g_userStatusLabel, nullptr, TRUE);
        MessageBoxW(window, L"کانفیگ شما با موفقیت به حالت پیش‌فرض بازنشانی و در هاست ذخیره شد.", L"موفقیت", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxW(window, L"کانفیگ به صورت محلی ریست شد اما در ذخیره روی هاست خطایی رخ داد.", L"هشدار", MB_OK | MB_ICONWARNING);
    }
}

// Subclass for Trackbar: allows clicking anywhere on the bar to jump directly to any notch (1 to 11)
LRESULT CALLBACK TrackbarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN)
    {
        RECT rc{};
        SendMessageW(hwnd, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&rc));
        const int x = GET_X_LPARAM(lParam);
        const int w = static_cast<int>(rc.right - rc.left);
        if (w > 0)
        {
            int notch = 1 + MulDiv(std::clamp(static_cast<int>(x - rc.left), 0, w), 10, w);
            notch = std::clamp(notch, 1, 11);
            SendMessageW(hwnd, TBM_SETPOS, TRUE, notch);
            SendMessageW(GetParent(hwnd), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, notch), reinterpret_cast<LPARAM>(hwnd));
            SetCapture(hwnd);
            return 0;
        }
    }
    else if (msg == WM_MOUSEMOVE && (wParam & MK_LBUTTON))
    {
        RECT rc{};
        SendMessageW(hwnd, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&rc));
        const int x = GET_X_LPARAM(lParam);
        const int w = static_cast<int>(rc.right - rc.left);
        if (w > 0)
        {
            int notch = 1 + MulDiv(std::clamp(static_cast<int>(x - rc.left), 0, w), 10, w);
            notch = std::clamp(notch, 1, 11);
            SendMessageW(hwnd, TBM_SETPOS, TRUE, notch);
            SendMessageW(GetParent(hwnd), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, notch), reinterpret_cast<LPARAM>(hwnd));
            return 0;
        }
    }
    else if (msg == WM_LBUTTONUP)
    {
        if (GetCapture() == hwnd)
            ReleaseCapture();
    }
    return CallWindowProcW(s_origTrackbarProc, hwnd, msg, wParam, lParam);
}

// Subclasses for Edit Controls: Tab switches between Phone -> Pass -> Login, Enter executes Login
LRESULT CALLBACK PhoneEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN)
    {
        if (wParam == VK_TAB)
        {
            SetFocus(g_userPassword);
            SendMessageW(g_userPassword, EM_SETSEL, 0, -1);
            return 0;
        }
        if (wParam == VK_RETURN)
        {
            PerformUserLogin(GetParent(hwnd));
            return 0;
        }
    }
    return CallWindowProcW(s_origPhoneProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK PassEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN)
    {
        if (wParam == VK_TAB)
        {
            SetFocus(g_userLoginBtn);
            return 0;
        }
        if (wParam == VK_RETURN)
        {
            PerformUserLogin(GetParent(hwnd));
            return 0;
        }
    }
    return CallWindowProcW(s_origPassProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK OtpRegisterProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        auto addCtrl = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, WORD id, DWORD ex = 0) {
            HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
            return c;
        };

        HWND titleLbl = addCtrl(L"STATIC", L"\u062b\u0628\u062a\u200c\u0646\u0627\u0645 \u0648 \u0641\u0639\u0627\u0644\u200c\u0633\u0627\u0632\u06cc \u062d\u0633\u0627\u0628 \u06a9\u0627\u0631\u0628\u0631\u06cc \u0627\u0628\u0631\u06cc", SS_CENTER, 20, 14, 440, 24, 0);
        SendMessageW(titleLbl, WM_SETFONT, reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);

        addCtrl(L"STATIC", L"\u0634\u0645\u0627\u0631\u0647 \u0645\u0648\u0628\u0627\u06cc\u0644:", SS_RIGHT, 330, 56, 120, 20, 0);
        g_regMobile = addCtrl(L"EDIT", L"", ES_AUTOHSCROLL | ES_CENTER | WS_BORDER | WS_TABSTOP, 175, 52, 148, 28, IdRegMobile);
        g_regRequestOtpBtn = addCtrl(L"BUTTON", L"\u062f\u0631\u06cc\u0627\u0641\u062a \u06a9\u062f \u067e\u06cc\u0627\u0645\u06a9\u06cc", BS_OWNERDRAW | WS_TABSTOP, 24, 51, 140, 30, IdRegRequestOtp);

        addCtrl(L"STATIC", L"\u06a9\u062f \u062a\u0623\u06cc\u06cc\u062f \u067e\u06cc\u0627\u0645\u06a9:", SS_RIGHT, 330, 100, 120, 20, 0);
        g_regOtp = addCtrl(L"EDIT", L"", ES_AUTOHSCROLL | ES_CENTER | WS_BORDER | WS_TABSTOP, 175, 96, 148, 28, IdRegOtp);

        addCtrl(L"STATIC", L"\u0631\u0645\u0632 \u0639\u0628\u0648\u0631 \u062f\u0644\u062e\u0648\u0627\u0647:", SS_RIGHT, 330, 144, 120, 20, 0);
        g_regPassword = addCtrl(L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | ES_CENTER | WS_BORDER | WS_TABSTOP, 175, 140, 148, 28, IdRegPassword);

        g_regSubmitBtn = addCtrl(L"BUTTON", L"\u062b\u0628\u062a\u200c\u0646\u0627\u0645 \u0648 \u062a\u0623\u06cc\u06cc\u062f \u0646\u0647\u0627\u06cc\u06cc", BS_OWNERDRAW | WS_TABSTOP, 244, 190, 206, 36, IdRegSubmit);
        addCtrl(L"BUTTON", L"\u0628\u0633\u062a\u0646", BS_OWNERDRAW | WS_TABSTOP, 24, 190, 206, 36, IdRegClose);
        g_regStatusLabel = addCtrl(L"STATIC", L"", SS_CENTER, 20, 242, 440, 44, IdRegStatus);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND hwndCtl = reinterpret_cast<HWND>(lParam);
        SetBkMode(hdc, TRANSPARENT);
        if (hwndCtl == g_regStatusLabel)
            SetTextColor(hdc, RGB(0, 210, 160));
        else
            SetTextColor(hdc, RGB(220, 228, 240));
        static HBRUSH s_otpBgBrush = CreateSolidBrush(RGB(26, 27, 30));
        return reinterpret_cast<INT_PTR>(s_otpBgBrush);
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdc, RGB(18, 19, 21));
        SetTextColor(hdc, RGB(245, 245, 245));
        static HBRUSH s_otpEditBrush = CreateSolidBrush(RGB(18, 19, 21));
        return reinterpret_cast<INT_PTR>(s_otpEditBrush);
    }
    case WM_DRAWITEM:
    {
        const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        const bool pressed = (item.itemState & ODS_SELECTED) != 0;
        const bool focused = (item.itemState & ODS_FOCUS) != 0;

        COLORREF fill = RGB(49, 51, 54);
        COLORREF border = RGB(75, 78, 84);
        COLORREF text = RGB(220, 226, 235);

        if (item.CtlID == IdRegSubmit)
        {
            fill = pressed ? RGB(0, 146, 112) : RGB(0, 178, 136);
            border = focused ? RGB(0, 240, 180) : RGB(0, 210, 160);
            text = RGB(5, 15, 22);
        }
        else if (item.CtlID == IdRegRequestOtp)
        {
            fill = pressed ? RGB(28, 58, 90) : RGB(36, 76, 118);
            border = focused ? RGB(80, 160, 240) : RGB(52, 108, 168);
            text = RGB(230, 240, 255);
        }
        else if (item.CtlID == IdRegClose)
        {
            fill = pressed ? RGB(35, 37, 40) : RGB(45, 47, 50);
            border = RGB(65, 68, 72);
            text = RGB(200, 208, 220);
        }

        HBRUSH fillBrush = CreateSolidBrush(fill);
        FillRect(item.hDC, &item.rcItem, fillBrush);
        DeleteObject(fillBrush);

        HPEN borderPen = CreatePen(PS_SOLID, focused ? 2 : 1, border);
        HGDIOBJ oldPen = SelectObject(item.hDC, borderPen);
        HGDIOBJ oldBrush = SelectObject(item.hDC, GetStockObject(NULL_BRUSH));
        RoundRect(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom, 6, 6);
        SelectObject(item.hDC, oldBrush);
        SelectObject(item.hDC, oldPen);
        DeleteObject(borderPen);

        wchar_t caption[128]{};
        GetWindowTextW(item.hwndItem, caption, static_cast<int>(std::size(caption)));
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, text);
        HGDIOBJ oldFont = SelectObject(item.hDC, g_font);
        RECT textRect = item.rcItem;
        if (pressed)
            OffsetRect(&textRect, 1, 1);
        DrawTextW(item.hDC, caption, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(item.hDC, oldFont);
        return TRUE;
    }
    case WM_TIMER:
    {
        if (wParam == 999)
        {
            if (g_otpCooldownSeconds > 0)
            {
                g_otpCooldownSeconds--;
                const std::wstring btnTxt = std::to_wstring(g_otpCooldownSeconds) + L" ثانیه صبرا...";
                SetWindowTextW(g_regRequestOtpBtn, btnTxt.c_str());
            }
            else
            {
                KillTimer(window, 999);
                EnableWindow(g_regRequestOtpBtn, TRUE);
                SetWindowTextW(g_regRequestOtpBtn, L"دریافت کد پیامکی");
            }
        }
        return 0;
    }
    case WM_COMMAND:
    {
        const WORD id = LOWORD(wParam);
        if (id == IdRegClose)
        {
            DestroyWindow(window);
            return 0;
        }
        if (id == IdRegRequestOtp)
        {
            wchar_t mobileBuf[64]{};
            GetWindowTextW(g_regMobile, mobileBuf, static_cast<int>(std::size(mobileBuf)));
            const std::wstring mobile(mobileBuf);
            if (mobile.size() != 11 || mobile.substr(0, 2) != L"09")
            {
                MessageBoxW(window, L"شماره موبایل معتبر نیست.", L"خطا", MB_OK | MB_ICONWARNING);
                return 0;
            }
            SetWindowTextW(g_regStatusLabel, L"در حال ارتباط با سامانه پیامکی...");
            EnableWindow(g_regRequestOtpBtn, FALSE);
            const std::string body = std::string("action=request_otp&mobile=") + UrlEncode(NarrowUtf8(mobile));
            std::string response;
            const bool ok = PostUrlEncoded(kAuthOtpPath, body, response);

            const std::string success = ExtractJsonString(response, "success");
            const std::string msgStr = ExtractJsonStringDecoded(response, "message");
            if (ok && success == "true")
            {
                g_otpCooldownSeconds = 120;
                SetTimer(window, 999, 1000, nullptr);
                const std::wstring btnTxt = std::to_wstring(g_otpCooldownSeconds) + L" ثانیه صبرا...";
                SetWindowTextW(g_regRequestOtpBtn, btnTxt.c_str());

                const std::wstring msgWide = msgStr.empty() ? L"کد پیامکی ارسال شد. لطفاً آن را وارد نمایید." : WidenUtf8(msgStr);
                SetWindowTextW(g_regStatusLabel, msgWide.c_str());
                SetFocus(g_regOtp);
                MessageBoxW(window, msgWide.c_str(), L"پیامک تأیید", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                EnableWindow(g_regRequestOtpBtn, TRUE);
                const std::wstring err = msgStr.empty() ? L"خطا در ارسال پیامک." : WidenUtf8(msgStr);
                SetWindowTextW(g_regStatusLabel, err.c_str());
                MessageBoxW(window, err.c_str(), L"خطا", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        if (id == IdRegSubmit)
        {
            wchar_t mobileBuf[64]{};
            wchar_t otpBuf[64]{};
            wchar_t passBuf[64]{};
            GetWindowTextW(g_regMobile, mobileBuf, static_cast<int>(std::size(mobileBuf)));
            GetWindowTextW(g_regOtp, otpBuf, static_cast<int>(std::size(otpBuf)));
            GetWindowTextW(g_regPassword, passBuf, static_cast<int>(std::size(passBuf)));
            const std::wstring mobile(mobileBuf);
            const std::wstring otp(otpBuf);
            const std::wstring password(passBuf);

            if (mobile.size() != 11 || otp.empty() || password.empty())
            {
                MessageBoxW(window, L"لطفاً تمام فیلدها را کامل نمایید.", L"خطا", MB_OK | MB_ICONWARNING);
                return 0;
            }

            SetWindowTextW(g_regStatusLabel, L"در حال ثبت‌نام و تأیید...");
            EnableWindow(g_regSubmitBtn, FALSE);
            const std::string body = std::string("action=verify_otp&mobile=") + UrlEncode(NarrowUtf8(mobile)) +
                                     "&otp=" + UrlEncode(NarrowUtf8(otp)) +
                                     "&password=" + UrlEncode(NarrowUtf8(password));
            std::string response;
            const bool ok = PostUrlEncoded(kAuthOtpPath, body, response);
            EnableWindow(g_regSubmitBtn, TRUE);

            const std::string success = ExtractJsonString(response, "success");
            const std::string msgStr = ExtractJsonStringDecoded(response, "message");
            const std::string token = ExtractJsonString(response, "token");
            if (ok && success == "true" && !token.empty())
            {
                g_activeUserPhone = mobile;
                g_activeUserToken = token;
                if (g_userPhone)
                    SetWindowTextW(g_userPhone, mobile.c_str());
                if (g_userPassword)
                    SetWindowTextW(g_userPassword, password.c_str());
                if (g_userStatusLabel)
                {
                    const std::wstring status = L"وارد شده: \u200e" + mobile;
                    SetWindowTextW(g_userStatusLabel, status.c_str());
                    InvalidateRect(g_userStatusLabel, nullptr, TRUE);
                }
                PullUserConfigFromCloud(token);
                MessageBoxW(window, L"ثبت‌نام با موفقیت انجام شد و وارد شدید.", L"موفقیت", MB_OK | MB_ICONINFORMATION);
                DestroyWindow(window);
            }
            else
            {
                const std::wstring err = msgStr.empty() ? L"کد تأیید اشتباه یا منقضی شده است." : WidenUtf8(msgStr);
                SetWindowTextW(g_regStatusLabel, err.c_str());
                MessageBoxW(window, err.c_str(), L"خطا در ثبت‌نام", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowOtpRegisterDialog(HWND parent)
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = OtpRegisterProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = kOtpRegisterWindowClass;
        wc.hbrBackground = CreateSolidBrush(RGB(26, 27, 30));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        registered = true;
    }

    RECT pRc{};
    GetWindowRect(parent, &pRc);
    const int dlgW = 480;
    const int dlgH = 340;
    const int dlgX = pRc.left + ((pRc.right - pRc.left) - dlgW) / 2;
    const int dlgY = pRc.top + ((pRc.bottom - pRc.top) - dlgH) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kOtpRegisterWindowClass,
                               L"\u062b\u0628\u062a\u200c\u0646\u0627\u0645 \u062d\u0633\u0627\u0628 \u06a9\u0627\u0631\u0628\u0631\u06cc (SMS OTP)",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                               dlgX, dlgY, dlgW, dlgH, parent, nullptr, g_instance, nullptr);
    typedef HRESULT(WINAPI* PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    if (HMODULE hDwm = LoadLibraryW(L"dwmapi.dll"); hDwm != nullptr)
    {
        if (auto pfn = reinterpret_cast<PFN_DwmSetWindowAttribute>(GetProcAddress(hDwm, "DwmSetWindowAttribute")))
        {
            BOOL dark = TRUE;
            pfn(dlg, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
        }
    }

    EnableWindow(parent, FALSE);
    MSG msg{};
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
}

bool IsActionButtonId(UINT id)
{
    return id == IdLaunch || id == IdDemoManager || id == IdRestore || id == IdCancel || id == IdUserLogin || id == IdUserRegister;
}

void DrawActionButton(const DRAWITEMSTRUCT& item)
{
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;

    COLORREF fill = RGB(39, 42, 45);
    COLORREF border = RGB(70, 74, 78);
    COLORREF text = disabled ? RGB(110, 120, 138) : kColorText;

    if (item.CtlID == IdLaunch)
    {
        fill = pressed ? RGB(0, 146, 112) : RGB(0, 178, 136);
        border = focused ? kColorAccentHot : kColorAccent;
        text = RGB(5, 15, 22);
    }
    else if (item.CtlID == IdDemoManager)
    {
        fill = pressed ? RGB(38, 66, 60) : RGB(31, 48, 44);
        border = focused ? kColorAccentHot : RGB(69, 113, 98);
        text = RGB(0, 210, 160);
    }
    else if (item.CtlID == IdUserLogin)
    {
        fill = pressed ? RGB(0, 130, 95) : RGB(0, 160, 120);
        border = focused ? kColorAccentHot : RGB(0, 200, 150);
        text = RGB(255, 255, 255);
    }
    else if (item.CtlID == IdUserRegister)
    {
        if (!g_activeUserToken.empty())
        {
            fill = pressed ? RGB(160, 80, 20) : RGB(190, 95, 25);
            border = focused ? RGB(255, 170, 70) : RGB(210, 120, 40);
            text = RGB(255, 255, 255);
        }
        else
        {
            fill = pressed ? RGB(28, 58, 90) : RGB(36, 76, 118);
            border = focused ? RGB(80, 160, 240) : RGB(52, 108, 168);
            text = RGB(230, 240, 255);
        }
    }
    else if (item.CtlID == IdCancel)
    {
        fill = pressed ? RGB(49, 51, 54) : kColorBackground;
        border = RGB(65, 68, 72);
        text = RGB(200, 208, 220);
    }
    else if (item.CtlID == IdRestore)
    {
        fill = pressed ? RGB(49, 51, 54) : kColorBackground;
        border = RGB(65, 68, 72);
        text = RGB(200, 208, 220);
    }

    HBRUSH fillBrush = CreateSolidBrush(fill);
    FillRect(item.hDC, &item.rcItem, fillBrush);
    DeleteObject(fillBrush);

    HPEN borderPen = CreatePen(PS_SOLID, focused ? 2 : 1, border);
    HGDIOBJ oldPen = SelectObject(item.hDC, borderPen);
    HGDIOBJ oldBrush = SelectObject(item.hDC, GetStockObject(NULL_BRUSH));
    Rectangle(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom);
    SelectObject(item.hDC, oldBrush);
    SelectObject(item.hDC, oldPen);
    DeleteObject(borderPen);

    wchar_t caption[128]{};
    GetWindowTextW(item.hwndItem, caption, static_cast<int>(std::size(caption)));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, text);
    HGDIOBJ oldFont = SelectObject(item.hDC, g_emphasisFont);
    RECT textRect = item.rcItem;
    if (pressed)
        OffsetRect(&textRect, 1, 1);
    DrawTextW(item.hDC, caption, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(item.hDC, oldFont);
    if (focused)
    {
        RECT focusRect = item.rcItem;
        InflateRect(&focusRect, -5, -5);
        DrawFocusRect(item.hDC, &focusRect);
    }
}

void CreateControls(HWND window)
{
    InitializeNativeResolutionIfNeeded();

    auto label = [&](const wchar_t* text, int x, int y, int width, int height = 24)
    {
        return AddControl(window, L"STATIC", text, SS_RIGHT, x, y, width, height);
    };

    // ─── Header (Width = 660) ───
    AddActionButton(window, L"Demo Manager", 28, 17, 136, 48, IdDemoManager);

    // Subscription Pill Badge in Header - Dual line with Subscription Name + Remaining Days
    int remainingDays = 0;
    if (g_accessStatus.state == GameNetAccessState::Active && g_accessStatus.days_remaining >= 0)
        remainingDays = g_accessStatus.days_remaining;
    else if (g_accessStatus.days_remaining > 0)
        remainingDays = g_accessStatus.days_remaining;
    else
        remainingDays = 1893;

    const std::wstring subName = g_accessStatus.tag.empty() ? L"GAMELAND" : WidenUtf8(g_accessStatus.tag);
    const std::wstring tagText = L"\u0646\u0627\u0645 \u0627\u0634\u062a\u0631\u0627\u06a9: " + subName;
    const std::wstring remainingText = L"\u0627\u0639\u062a\u0628\u0627\u0631: " + std::to_wstring(remainingDays) + L" \u0631\u0648\u0632 \u0628\u0627\u0642\u06cc\u200c\u0645\u0627\u0646\u062f\u0647";

    g_subscriptionTag = AddControl(window, L"STATIC", tagText.c_str(), SS_CENTER | SS_CENTERIMAGE, 172, 17, 234, 22);
    SendMessageW(g_subscriptionTag, WM_SETFONT, reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);

    g_subscriptionRemaining = AddControl(window, L"STATIC", remainingText.c_str(), SS_CENTER | SS_CENTERIMAGE, 172, 40, 234, 22);
    SendMessageW(g_subscriptionRemaining, WM_SETFONT, reinterpret_cast<WPARAM>(g_badgeFont), TRUE);

    HWND heading = label(L"ALLCLIENT", 420, 16, 212, 34);
    SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(g_brandFont), TRUE);
    HWND subHeading = label(L"COUNTER-STRIKE  /  1.6", 420, 52, 212, 18);
    SendMessageW(subHeading, WM_SETFONT, reinterpret_cast<WPARAM>(g_badgeFont), TRUE);

    // ─── Section 1: Video Settings ───
    HWND videoTitle = label(L"تنظیمات تصویر", 450, 96, 170, 22);
    SendMessageW(videoTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);

    label(L"وضوح تصویر:", 450, 128, 170, 24);
    g_resolution = AddControl(window, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                              60, 124, 270, 220, IdResolution);
    SetWindowLongPtrW(g_resolution, GWL_EXSTYLE,
        GetWindowLongPtrW(g_resolution, GWL_EXSTYLE) & ~WS_EX_RTLREADING);

    g_highQuality = AddControl(window, L"BUTTON", L"کیفیت بالای تصویر",
        BS_AUTOCHECKBOX | BS_RIGHT | BS_LEFTTEXT | WS_TABSTOP, 360, 160, 260, 28, IdHighQuality);
    g_hdModels = AddControl(window, L"BUTTON", L"مدل‌های HD بازیکنان",
        BS_AUTOCHECKBOX | BS_RIGHT | BS_LEFTTEXT | WS_TABSTOP, 60, 160, 260, 28, IdHdModels);

    // ─── Section 2: Mouse Settings ───
    HWND mouseTitle = label(L"تنظیمات ماوس", 450, 210, 170, 22);
    SendMessageW(mouseTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);

    label(L"سرعت نشانگر:", 450, 242, 170, 24);
    g_pointerSpeed = AddControl(window, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                140, 238, 330, 30, IdPointerSpeed);
    SetWindowLongPtrW(g_pointerSpeed, GWL_EXSTYLE,
        GetWindowLongPtrW(g_pointerSpeed, GWL_EXSTYLE) & ~WS_EX_RTLREADING);
    SendMessageW(g_pointerSpeed, TBM_SETRANGE, TRUE, MAKELPARAM(1, 11));
    SendMessageW(g_pointerSpeed, TBM_SETTICFREQ, 1, 0);
    SendMessageW(g_pointerSpeed, TBM_SETPAGESIZE, 0, 1);

    // Subclass Trackbar so direct clicks select any notch (1 to 11) immediately
    s_origTrackbarProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_pointerSpeed, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TrackbarProc)));

    g_pointerSpeedValue = AddControl(window, L"STATIC", L"\u202A4 / 11\u202C", SS_CENTER | SS_CENTERIMAGE, 50, 240, 80, 28, IdPointerSpeedValue);
    SendMessageW(g_pointerSpeedValue, WM_SETFONT, reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);

    g_enhancePointer = AddControl(window, L"BUTTON", L"افزایش دقت نشانگر (شتاب ویندوز)",
        BS_AUTOCHECKBOX | BS_RIGHT | BS_LEFTTEXT | WS_TABSTOP, 360, 274, 260, 28, IdEnhancePointer);
    label(L"اعمال زنده روی ماوس کاربر فعلی ویندوز", 60, 277, 270, 24);

    // ─── Section 3: Cloud User Account ───
    HWND cloudTitle = label(L"حساب کاربری ابری", 450, 322, 170, 22);
    SendMessageW(cloudTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);

    label(L"شماره موبایل:", 480, 354, 140, 24);
    g_userPhone = AddControl(window, L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP,
                             320, 352, 150, 28, IdUserPhone, WS_EX_CLIENTEDGE);
    s_origPhoneProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_userPhone, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PhoneEditProc)));

    label(L"رمز عبور:", 210, 354, 90, 24);
    g_userPassword = AddControl(window, L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP,
                                60, 352, 140, 28, IdUserPassword, WS_EX_CLIENTEDGE);
    s_origPassProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_userPassword, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PassEditProc)));

    g_userLoginBtn = AddActionButton(window, L"ورود", 480, 392, 140, 32, IdUserLogin);
    g_userRegisterBtn = AddActionButton(window, L"ثبت‌نام (پیامکی)", 320, 392, 150, 32, IdUserRegister);

    // Unclipped full status message
    g_userStatusLabel = label(L"وارد نشده‌اید (مهمان: کانفیگ پیش‌فرض لود می‌شود)", 50, 396, 260, 24);
    SendMessageW(g_userStatusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_badgeFont), TRUE);

    // ─── Section 4: Action Buttons ───
    AddActionButton(window, L"اجرای بازی", 400, 452, 220, 48, IdLaunch, true);
    AddActionButton(window, L"بازنشانی", 210, 452, 160, 48, IdRestore);
    AddActionButton(window, L"انصراف", 60, 452, 130, 48, IdCancel);

    // Status Message at bottom
    g_status = label(L"آماده", 30, 514, 600, 28);

    const VideoSettings current = ReadSettings();
    PopulateResolutions(current);
    SetControls(current);
    g_mouseAtLastApply = ReadSystemMouseSettings();
    g_mousePreviewChanged = false;
    SetMouseControls(g_mouseAtLastApply);
}

void CheckLauncherUpdates(HWND window)
{
    SetStatus(L"در حال بررسی بروزرسانی کلاینت...");

    std::string tag = NEXTCLIENT_TAG;
    std::string version = ReadInstalledClientVersion();
    std::string url = "http://gameland.cam/update_api.php?tag=" + tag + "&version=" + version;

    HINTERNET hInternet = InternetOpenA("AllclientLauncher", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
    {
        std::wstring fallbackMsg = L"آماده بازی  •  نسخه " + WidenAscii(version);
        SetStatus(fallbackMsg.c_str());
        return;
    }

    DWORD timeout = 2500;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect)
    {
        InternetCloseHandle(hInternet);
        std::wstring fallbackMsg = L"آماده بازی  •  نسخه " + WidenAscii(version);
        SetStatus(fallbackMsg.c_str());
        return;
    }

    char buffer[1024];
    DWORD bytesRead = 0;
    std::string response;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        response += buffer;
    }
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    bool hasUpdate = (response.find("\"update_available\":true") != std::string::npos ||
                      response.find("\"update_available\": true") != std::string::npos ||
                      response.find("\"has_update\":true") != std::string::npos ||
                      response.find("\"has_update\": true") != std::string::npos);

    if (hasUpdate)
    {
        size_t urlPos = response.find("\"download_url\":\"");
        if (urlPos == std::string::npos)
            urlPos = response.find("\"download_url\": \"");

        if (urlPos != std::string::npos)
        {
            urlPos = response.find("\"", urlPos + 15);
            if (urlPos != std::string::npos)
            {
                urlPos++;
                size_t urlEnd = response.find("\"", urlPos);
                if (urlEnd != std::string::npos)
                {
                    std::string rawUrl = response.substr(urlPos, urlEnd - urlPos);
                    std::string downloadUrl;
                    for (size_t i = 0; i < rawUrl.length(); ++i)
                    {
                        if (rawUrl[i] == '\\' && i + 1 < rawUrl.length() && rawUrl[i + 1] == '/')
                        {
                            downloadUrl += '/';
                            ++i;
                        }
                        else
                        {
                            downloadUrl += rawUrl[i];
                        }
                    }

                    std::string latestVersion = "جدید";
                    size_t verPos = response.find("\"latest_version\":\"");
                    if (verPos != std::string::npos)
                    {
                        verPos += 18;
                        size_t verEnd = response.find("\"", verPos);
                        if (verEnd != std::string::npos)
                            latestVersion = response.substr(verPos, verEnd - verPos);
                    }

                    SetStatus(L"آپدیت جدید آماده دریافت است.");

                    std::wstring promptMsg = L"آپدیت جدید نسخه " + WidenAscii(latestVersion) +
                        L" برای کلاینت شما منتشر شده است.\n\n"
                        L"برای ادامه استفاده، دریافت آپدیت الزامی است.\n"
                        L"آیا مایلید هم‌اکنون آپدیت دانلود و نصب شود؟";

                    int userChoice = MessageBoxW(window, promptMsg.c_str(), L"بروزرسانی Allclient",
                        MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);

                    if (userChoice == IDYES || userChoice == IDOK)
                    {
                        SetStatus(L"در حال فراخوانی ابزار بروزرسانی...");
                        std::string execParams = "\"" + downloadUrl + "\"";
                        SHELLEXECUTEINFOA sei = { sizeof(sei) };
                        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                        sei.lpVerb = "open";
                        sei.lpFile = "updater.exe";
                        sei.lpParameters = execParams.c_str();
                        sei.nShow = SW_SHOWNORMAL;

                        if (ShellExecuteExA(&sei))
                        {
                            ExitProcess(0);
                        }
                        else
                        {
                            SetStatus(L"خطا در اجرای updater.exe", true);
                            MessageBoxW(window, L"خطا در اجرای updater.exe. لطفاً اتصال اینترنت خود را بررسی کنید.", L"خطای بروزرسانی", MB_ICONERROR);
                        }
                    }
                    else
                    {
                        SetStatus(L"برای اجرای بازی، نصب بروزرسانی الزامی است.", true);
                    }
                    return;
                }
            }
        }
    }

    std::wstring readyMsg = L"آماده بازی  •  کلاینت نسخه " + WidenAscii(version) + L" (بروزرسانی شده)";
    SetStatus(readyMsg.c_str());
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateControls(window);
        PostMessageW(window, WM_APP + 101, 0, 0); // Check updates
        PostMessageW(window, WM_APP + 102, 0, 0); // Initial focus on phone
        return 0;

    case WM_APP + 101:
        CheckLauncherUpdates(window);
        return 0;

    case WM_APP + 102:
        if (g_userPhone)
        {
            SetFocus(g_userPhone);
            SendMessageW(g_userPhone, EM_SETSEL, 0, -1);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IdLaunch:
            if (ApplySettings())
            {
                g_launchRequested = true;
                DestroyWindow(window);
            }
            return 0;
        case IDCANCEL:
        case IdCancel:
            RevertMousePreview();
            DestroyWindow(window);
            return 0;
        case IdEnhancePointer:
            if (HIWORD(wParam) == BN_CLICKED)
                ApplyMousePreview();
            return 0;
        case IdDemoManager:
            ShowDemoManager(window);
            return 0;
        case IdUserLogin:
            PerformUserLogin(window);
            return 0;
        case IdUserRegister:
            if (!g_activeUserToken.empty())
            {
                ResetUserConfigToDefault(window);
            }
            else
            {
                ShowOtpRegisterDialog(window);
            }
            return 0;
        case IdRestore:
        {
            Button_SetCheck(g_hdModels, BST_UNCHECKED);
            Button_SetCheck(g_highQuality, BST_CHECKED);

            SystemMouseSettings mouseDefaults = g_mouseAtLastApply;
            mouseDefaults.speed = SliderPositionToPointerSpeed(4); // Default Notch 4 of 11!
            mouseDefaults.acceleration[0] = 6;
            mouseDefaults.acceleration[1] = 10;
            mouseDefaults.acceleration[2] = 1;
            SetMouseControls(mouseDefaults);
            ApplyMousePreview();
            SetStatus(L"پیش‌فرض‌ها انتخاب شدند؛ وضوح تصویر حفظ شد. مدل HD خاموش و سرعت ماوس ۴ از ۱۱ است.");
            return 0;
        }
        default:
            break;
        }
        break;

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == g_pointerSpeed)
        {
            const int speed = static_cast<int>(SendMessageW(g_pointerSpeed, TBM_GETPOS, 0, 0));
            const std::wstring speedText = PointerPositionText(speed);
            SetWindowTextW(g_pointerSpeedValue, speedText.c_str());
            InvalidateRect(g_pointerSpeedValue, nullptr, TRUE);
            UpdateWindow(g_pointerSpeedValue);
            ApplyMousePreview();
            return 0;
        }
        break;

    case WM_DRAWITEM:
        if (IsActionButtonId(static_cast<UINT>(wParam)))
        {
            DrawActionButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
            return TRUE;
        }
        break;

    case WM_CTLCOLORSTATIC:
    {
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        HWND ctl = reinterpret_cast<HWND>(lParam);
        if (ctl == g_subscriptionTag)
        {
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(235, 245, 255));
        }
        else if (ctl == g_subscriptionRemaining)
        {
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(0, 220, 165));
        }
        else if (ctl == g_pointerSpeedValue)
        {
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(0, 210, 160));
        }
        else if (ctl == g_userStatusLabel)
        {
            if (!g_activeUserToken.empty())
                SetTextColor(reinterpret_cast<HDC>(wParam), RGB(0, 210, 160));
            else
                SetTextColor(reinterpret_cast<HDC>(wParam), RGB(130, 170, 220));
        }
        else if (ctl == g_versionLabel || ctl == g_status)
        {
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(0, 210, 160));
        }
        else
        {
            SetTextColor(reinterpret_cast<HDC>(wParam), kColorText);
        }
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    }

    case WM_CTLCOLOREDIT:
    {
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(30, 32, 36));
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(240, 244, 250));
        static HBRUSH s_editBrush = CreateSolidBrush(RGB(30, 32, 36));
        return reinterpret_cast<LRESULT>(s_editBrush);
    }

    case WM_CTLCOLORBTN:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(reinterpret_cast<HDC>(wParam), kColorText);
        return reinterpret_cast<LRESULT>(g_backgroundBrush);

    case WM_CTLCOLORDLG:
        return reinterpret_cast<LRESULT>(g_backgroundBrush);

    case WM_ERASEBKGND:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(dc, &rect, g_backgroundBrush);

        HPEN accentPen = CreatePen(PS_SOLID, 1, RGB(48, 52, 58));
        HGDIOBJ oldPen = SelectObject(dc, accentPen);
        for (int y : {82, 198, 310, 436})
        {
            MoveToEx(dc, 28, y, nullptr);
            LineTo(dc, rect.right - 28, y);
        }
        SelectObject(dc, oldPen);
        DeleteObject(accentPen);

        // Subscription badge container pill in header - 238px wide, 54px high
        RECT badgeRect{170, 14, 408, 68};
        HBRUSH badgeBrush = CreateSolidBrush(RGB(22, 34, 32));
        HPEN badgePen = CreatePen(PS_SOLID, 1, RGB(0, 160, 120));
        HGDIOBJ prevBrush = SelectObject(dc, badgeBrush);
        HGDIOBJ prevPen = SelectObject(dc, badgePen);
        RoundRect(dc, badgeRect.left, badgeRect.top, badgeRect.right, badgeRect.bottom, 10, 10);
        SelectObject(dc, prevBrush);
        SelectObject(dc, prevPen);
        DeleteObject(badgeBrush);
        DeleteObject(badgePen);

        // Top accent line / bar on top right
        HBRUSH accentBrush = CreateSolidBrush(kColorAccent);
        RECT accent{rect.right - 8, 20, rect.right - 4, 68};
        FillRect(dc, &accent, accentBrush);
        DeleteObject(accentBrush);
        return TRUE;
    }

    case WM_DESTROY:
        if (!g_launchRequested)
            RevertMousePreview();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

void SyncPlayerConfig()
{
    if (!g_activeUserToken.empty())
    {
        PushUserConfigToCloud(g_activeUserToken);
    }
}

bool ShowVideoSettingsDialog(HINSTANCE instance, const GameNetAccessStatus& access_status)
{
    g_instance = instance;
    g_launchRequested = false;
    g_accessStatus = access_status;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    wcscpy_s(metrics.lfMessageFont.lfFaceName, L"Segoe UI");
    metrics.lfMessageFont.lfHeight = -15;
    g_font = CreateFontIndirectW(&metrics.lfMessageFont);
    LOGFONTW emphasisFont = metrics.lfMessageFont;
    emphasisFont.lfWeight = FW_BOLD;
    g_emphasisFont = CreateFontIndirectW(&emphasisFont);
    emphasisFont.lfHeight = -30;
    g_brandFont = CreateFontIndirectW(&emphasisFont);
    LOGFONTW badgeFont = metrics.lfMessageFont;
    badgeFont.lfWeight = FW_SEMIBOLD;
    badgeFont.lfHeight = -13;
    g_badgeFont = CreateFontIndirectW(&badgeFont);
    g_backgroundBrush = CreateSolidBrush(kColorBackground);
    g_panelBrush = CreateSolidBrush(kColorPanel);

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = g_backgroundBrush;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hIconSm = windowClass.hIcon;
    if (!RegisterClassExW(&windowClass))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    RECT rect{0, 0, 660, 560};
    AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND window = CreateWindowExW(WS_EX_RTLREADING, kWindowClass, kTitle,
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!window)
        return false;

    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (IsDialogMessageW(window, &message))
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_font)
        DeleteObject(g_font);
    if (g_emphasisFont)
        DeleteObject(g_emphasisFont);
    if (g_brandFont)
        DeleteObject(g_brandFont);
    if (g_badgeFont)
        DeleteObject(g_badgeFont);
    if (g_backgroundBrush)
        DeleteObject(g_backgroundBrush);
    if (g_panelBrush)
        DeleteObject(g_panelBrush);
    g_font = nullptr;
    g_emphasisFont = nullptr;
    g_brandFont = nullptr;
    g_badgeFont = nullptr;
    g_backgroundBrush = nullptr;
    g_panelBrush = nullptr;
    return g_launchRequested;
}
