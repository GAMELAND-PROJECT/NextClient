#include <Windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wininet.h>

#include "../next_launcher/GameNetAccess.h"
#include "../next_launcher/VideoSettingsDialog.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "wininet.lib")

namespace
{
enum ControlId : WORD
{
    IdOk = 1001,
    IdCancel,
    IdReset,
    IdDemoManager,
    IdResolutionCombo,
    IdWindowedCheck,
    IdHighQualityCheck,
    IdHdModelsCheck,
    IdSensSlider,
    IdSensValueText,
    IdEnhancePointerCheck,
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

constexpr wchar_t kWindowClassName[] = L"AllclientVideoSettingsWindow";
constexpr wchar_t kOtpRegisterWindowClass[] = L"AllclientOtpRegisterWindow";
constexpr wchar_t kRegistryKey[] = L"Software\\Valve\\Half-Life\\Settings";
constexpr wchar_t kLauncherKey[] = L"Software\\Valve\\Half-Life\\nextclient";
constexpr wchar_t kFontFamily[] = L"Segoe UI";
constexpr int kBaseDpi = 96;

constexpr char kUploadHost[] = "gameland.cam";
constexpr char kAuthOtpPath[] = "/auth_otp.php";
constexpr char kCfgSyncPath[] = "/cfg_sync.php";

HINSTANCE g_instance{};
HWND g_dialogWindow{};
HWND g_resolutionCombo{};
HWND g_windowedCheck{};
HWND g_highQualityCheck{};
HWND g_hdModelsCheck{};
HWND g_sensSlider{};
HWND g_sensValueText{};
HWND g_enhancePointerCheck{};
HWND g_demoManagerBtn{};
HWND g_statusLabel{};
HWND g_okButton{};
HWND g_cancelButton{};
HWND g_resetButton{};

HWND g_userPhone{};
HWND g_userPassword{};
HWND g_userLoginBtn{};
HWND g_userRegisterBtn{};
HWND g_userStatusLabel{};

HWND g_regMobile{};
HWND g_regRequestOtpBtn{};
HWND g_regOtp{};
HWND g_regPassword{};
HWND g_regSubmitBtn{};
HWND g_regStatusLabel{};

HFONT g_dialogFont{};
HFONT g_titleFont{};
HFONT g_titleFontSub{};
HFONT g_badgeFont{};
HFONT g_valueFont{};
HFONT g_buttonFont{};

bool g_launchRequested = false;
bool g_mousePreviewChanged = false;
GameNetAccessStatus g_accessStatus{};

std::wstring g_activeUserPhone;
std::string g_activeUserToken;

int g_otpCooldownSeconds = 0;
UINT_PTR g_otpTimerId = 0;

struct Resolution
{
    int width;
    int height;
    auto operator<=>(const Resolution&) const = default;
};

static const std::vector<Resolution> kKnownResolutions = {
    {640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1280, 960}, {1280, 1024},
    {1366, 768}, {1440, 900}, {1600, 900}, {1680, 1050}, {1920, 1080}, {2560, 1440}
};

struct VideoSettings
{
    int width = 0;
    int height = 0;
    bool windowed = false;
    bool highQuality = true;
    bool hdModels = false;
};

struct SystemMouseSettings
{
    int speed = 10;
    int threshold1 = 6;
    int threshold2 = 10;
    int acceleration = 1;
};

SystemMouseSettings g_initialMouse{};
SystemMouseSettings g_mouseAtLastApply{};

int ScaleDpi(int value, int dpi)
{
    return MulDiv(value, dpi, kBaseDpi);
}

int GetWindowDpi(HWND window)
{
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        const auto fn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (fn && window)
            return static_cast<int>(fn(window));
    }
    const HDC screenDc = GetDC(nullptr);
    const int dpi = screenDc ? GetDeviceCaps(screenDc, LOGPIXELSX) : kBaseDpi;
    if (screenDc)
        ReleaseDC(nullptr, screenDc);
    return dpi ? dpi : kBaseDpi;
}

std::wstring WidenUtf8(const std::string& str)
{
    if (str.empty())
        return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring out(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), count);
    return out;
}

std::string NarrowUtf8(const std::wstring& str)
{
    if (str.empty())
        return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
    std::string out(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), out.data(), count, nullptr, nullptr);
    return out;
}

std::string UrlEncode(const std::string& value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : value)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            escaped << c;
        else
            escaped << '%' << std::setw(2) << std::uppercase << int(c);
    }
    return escaped.str();
}

std::wstring DecodeJsonString(const std::string& raw)
{
    std::wstring out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); i++)
    {
        if (raw[i] == '\\' && i + 5 < raw.size() && (raw[i + 1] == 'u' || raw[i + 1] == 'U'))
        {
            std::string hexPart = raw.substr(i + 2, 4);
            wchar_t ch = static_cast<wchar_t>(strtol(hexPart.c_str(), nullptr, 16));
            out += ch;
            i += 5;
        }
        else if (raw[i] == '\\' && i + 1 < raw.size())
        {
            i++;
            if (raw[i] == 'n') out += L'\n';
            else if (raw[i] == 'r') out += L'\r';
            else if (raw[i] == 't') out += L'\t';
            else out += static_cast<wchar_t>(static_cast<unsigned char>(raw[i]));
        }
        else
        {
            out += static_cast<wchar_t>(static_cast<unsigned char>(raw[i]));
        }
    }
    return out;
}

std::string ExtractJsonString(const std::string& json, const std::string& key)
{
    const std::string quotedKey = "\"" + key + "\"";
    size_t pos = json.find(quotedKey);
    if (pos == std::string::npos)
        return {};
    pos += quotedKey.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ':'))
        pos++;
    if (pos >= json.size())
        return {};
    if (json[pos] == '\"')
    {
        pos++;
        std::string val;
        while (pos < json.size() && json[pos] != '\"')
        {
            if (json[pos] == '\\' && pos + 1 < json.size())
            {
                val += json[pos];
                val += json[pos + 1];
                pos += 2;
            }
            else
            {
                val += json[pos++];
            }
        }
        return val;
    }
    size_t end = json.find_first_of(",}\r\n \t", pos);
    return (end == std::string::npos) ? json.substr(pos) : json.substr(pos, end - pos);
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

bool ReadHttpResponse(HINTERNET request, std::string& response)
{
    response.clear();
    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
    {
        response.append(buffer, bytesRead);
    }
    return !response.empty();
}

bool PostUrlEncoded(const char* path, const std::string& body, std::string& response)
{
    HINTERNET session = InternetOpenA("AllclientLauncher", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session)
        return false;
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
                                       const_cast<char*>(body.data()), static_cast<DWORD>(body.size()));
    bool ok = false;
    if (sent)
    {
        ReadHttpResponse(request, response);
        ok = true;
    }
    InternetCloseHandle(request);
    InternetCloseHandle(connect);
    InternetCloseHandle(session);
    return ok;
}

class RegistryKey
{
public:
    RegistryKey(HKEY root, const wchar_t* subKey, REGSAM access)
    {
        if (RegCreateKeyExW(root, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, access, nullptr, &m_key, nullptr) != ERROR_SUCCESS)
            m_key = nullptr;
    }
    ~RegistryKey()
    {
        if (m_key)
            RegCloseKey(m_key);
    }
    bool valid() const { return m_key != nullptr; }
    int ReadInt(const wchar_t* name, int fallback) const
    {
        DWORD value = 0;
        DWORD size = sizeof(value);
        DWORD type = 0;
        if (RegQueryValueExW(m_key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS && type == REG_DWORD)
            return static_cast<int>(value);
        return fallback;
    }
    bool WriteInt(const wchar_t* name, int value)
    {
        const DWORD dword = static_cast<DWORD>(value);
        return RegSetValueExW(m_key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dword), sizeof(dword)) == ERROR_SUCCESS;
    }
private:
    HKEY m_key{};
};

VideoSettings ReadSettings()
{
    RegistryKey key(HKEY_CURRENT_USER, kRegistryKey, KEY_QUERY_VALUE);
    VideoSettings s;
    s.width = key.valid() ? key.ReadInt(L"ScreenWidth", 800) : 800;
    s.height = key.valid() ? key.ReadInt(L"ScreenHeight", 600) : 600;
    s.windowed = key.valid() && (key.ReadInt(L"ScreenWindowed", 0) != 0);
    RegistryKey launcherKey(HKEY_CURRENT_USER, kLauncherKey, KEY_QUERY_VALUE);
    s.highQuality = !launcherKey.valid() || (launcherKey.ReadInt(L"HighQuality", 1) != 0);
    s.hdModels = launcherKey.valid() && (launcherKey.ReadInt(L"HdModels", 0) != 0);
    return s;
}

bool WriteSettings(const VideoSettings& s)
{
    RegistryKey key(HKEY_CURRENT_USER, kRegistryKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
    if (!key.valid() || !key.WriteInt(L"ScreenWidth", s.width) ||
        !key.WriteInt(L"ScreenHeight", s.height) ||
        !key.WriteInt(L"ScreenWindowed", s.windowed ? 1 : 0))
        return false;
    RegistryKey launcherKey(HKEY_CURRENT_USER, kLauncherKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
    return launcherKey.valid() &&
           launcherKey.WriteInt(L"HighQuality", s.highQuality ? 1 : 0) &&
           launcherKey.WriteInt(L"HdModels", s.hdModels ? 1 : 0);
}

SystemMouseSettings ReadSystemMouseSettings()
{
    SystemMouseSettings s;
    int speed = 10;
    if (SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &speed, 0) && speed >= 1 && speed <= 20)
        s.speed = speed;
    int mouseParams[3]{6, 10, 1};
    if (SystemParametersInfoW(SPI_GETMOUSE, 0, mouseParams, 0))
    {
        s.threshold1 = mouseParams[0];
        s.threshold2 = mouseParams[1];
        s.acceleration = mouseParams[2];
    }
    return s;
}

bool WriteSystemMouseSettings(const SystemMouseSettings& s)
{
    int speed = std::clamp(s.speed, 1, 20);
    SystemParametersInfoW(SPI_SETMOUSESPEED, 0, reinterpret_cast<void*>(static_cast<INT_PTR>(speed)), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    int mouseParams[3]{s.threshold1, s.threshold2, s.acceleration ? 1 : 0};
    SystemParametersInfoW(SPI_SETMOUSE, 0, mouseParams, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    return true;
}

void SetStatus(const wchar_t* text, bool isError = false)
{
    if (g_statusLabel)
        SetWindowTextW(g_statusLabel, text);
}

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
    const auto cstrikeDir = ExecutableRoot() / L"cstrike";
    const auto defaultCfg = ExecutableRoot() / L"default" / L"config.cfg";
    const auto userCfg = cstrikeDir / L"userconfig.cfg";
    const auto gameCfg = cstrikeDir / L"config.cfg";

    std::error_code ec;
    std::filesystem::create_directories(cstrikeDir, ec);

    if (success == "true" && exists == "true")
    {
        const std::string cfgRaw = ExtractJsonString(response, "cfg_content");
        const std::wstring cfgWide = DecodeJsonString(cfgRaw);
        const std::string cfgUtf8 = NarrowUtf8(cfgWide);
        if (!cfgUtf8.empty())
        {
            std::ofstream out(gameCfg, std::ios::binary | std::ios::trunc);
            if (out)
            {
                out.write(cfgUtf8.data(), cfgUtf8.size());
                return true;
            }
        }
    }

    // New user with no cloud config: load clean default config
    if (std::filesystem::is_regular_file(defaultCfg, ec))
    {
        std::filesystem::copy_file(defaultCfg, gameCfg, std::filesystem::copy_options::overwrite_existing, ec);
    }
    return true;
}

bool PushUserConfigToCloud(const std::string& token)
{
    if (token.empty())
        return false;
    const auto cfgPath = ExecutableRoot() / L"cstrike" / L"config.cfg";
    std::ifstream in(cfgPath, std::ios::binary);
    if (!in)
        return false;
    std::string cfgContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (cfgContent.empty())
        return false;
    const std::string body = std::string("action=push&token=") + UrlEncode(token) + "&cfg_content=" + UrlEncode(cfgContent);
    std::string response;
    if (!PostUrlEncoded(kCfgSyncPath, body, response))
        return false;
    const std::string success = ExtractJsonString(response, "success");
    return success == "true";
}

void UpdateSensDisplay()
{
    if (!g_sensSlider || !g_sensValueText)
        return;
    const int val = static_cast<int>(SendMessageW(g_sensSlider, TBM_GETPOS, 0, 0));
    const std::wstring txt = std::to_wstring(val) + L" / 20";
    SetWindowTextW(g_sensValueText, txt.c_str());
}

SystemMouseSettings MouseSettingsFromControls(const SystemMouseSettings& base)
{
    SystemMouseSettings s = base;
    if (g_sensSlider)
        s.speed = static_cast<int>(SendMessageW(g_sensSlider, TBM_GETPOS, 0, 0));
    if (g_enhancePointerCheck)
    {
        const bool accel = (SendMessageW(g_enhancePointerCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
        s.acceleration = accel ? 1 : 0;
        s.threshold1 = accel ? 6 : 0;
        s.threshold2 = accel ? 10 : 0;
    }
    return s;
}

bool SettingsFromControls(VideoSettings& s)
{
    const int index = static_cast<int>(SendMessageW(g_resolutionCombo, CB_GETCURSEL, 0, 0));
    if (index < 0 || index >= static_cast<int>(kKnownResolutions.size()))
        return false;
    s.width = kKnownResolutions[index].width;
    s.height = kKnownResolutions[index].height;
    s.windowed = (SendMessageW(g_windowedCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.highQuality = (SendMessageW(g_highQualityCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.hdModels = (SendMessageW(g_hdModelsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    return true;
}

bool ApplySettings()
{
    VideoSettings requested;
    if (!SettingsFromControls(requested))
    {
        SetStatus(L"وضوح تصویر معتبری انتخاب کنید.", true);
        return false;
    }
    const VideoSettings previous = ReadSettings();
    const SystemMouseSettings previousMouse = g_mouseAtLastApply;
    const SystemMouseSettings requestedMouse = MouseSettingsFromControls(previousMouse);

    WriteSettings(requested);
    WriteSystemMouseSettings(requestedMouse);
    g_mouseAtLastApply = requestedMouse;
    g_mousePreviewChanged = false;

    if (!g_activeUserToken.empty())
    {
        PullUserConfigFromCloud(g_activeUserToken);
    }
    else
    {
        // Guest mode (without account): restore clean default config
        std::error_code ec;
        const auto defaultCfg = ExecutableRoot() / L"default" / L"config.cfg";
        const auto gameCfg = ExecutableRoot() / L"cstrike" / L"config.cfg";
        if (std::filesystem::is_regular_file(defaultCfg, ec))
        {
            std::filesystem::copy_file(defaultCfg, gameCfg, std::filesystem::copy_options::overwrite_existing, ec);
        }
    }

    SetStatus(L"تنظیمات اعمال شدند.");
    return true;
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
    const std::string message = ExtractJsonString(response, "message");
    const std::string token = ExtractJsonString(response, "token");
    if (ok && success == "true" && !token.empty())
    {
        g_activeUserPhone = phone;
        g_activeUserToken = token;
        const std::wstring status = L"وارد شده: \u200e" + phone;
        SetWindowTextW(g_userStatusLabel, status.c_str());
        MessageBoxW(window, L"ورود با موفقیت انجام شد. کانفیگ ابری شما فعال گردید.", L"حساب کاربری", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        const std::wstring err = message.empty() ? L"خطا در ارتباط یا رمز نادرست." : DecodeJsonString(message);
        SetWindowTextW(g_userStatusLabel, L"خطا در ورود.");
        MessageBoxW(window, err.c_str(), L"خطای ورود", MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK OtpRegisterProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        const int dpi = GetWindowDpi(window);
        const auto addCtrl = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                                 int x, int y, int w, int h, WORD id, DWORD exStyle = 0) {
            return CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                                   ScaleDpi(x, dpi), ScaleDpi(y, dpi), ScaleDpi(w, dpi), ScaleDpi(h, dpi),
                                   window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
        };

        HWND l1 = addCtrl(L"STATIC", L":شماره موبایل", SS_RIGHT, 220, 20, 190, 20, 0);
        g_regMobile = addCtrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 200, 44, 210, 28, IdRegMobile, WS_EX_CLIENTEDGE);
        g_regRequestOtpBtn = addCtrl(L"BUTTON", L"دریافت کد پیامکی", BS_PUSHBUTTON | WS_TABSTOP, 40, 43, 150, 30, IdRegRequestOtp);

        HWND l2 = addCtrl(L"STATIC", L":کد تأیید پیامک", SS_RIGHT, 260, 90, 150, 20, 0);
        g_regOtp = addCtrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 240, 114, 170, 28, IdRegOtp, WS_EX_CLIENTEDGE);

        HWND l3 = addCtrl(L"STATIC", L":رمز عبور دلخواه", SS_RIGHT, 50, 90, 170, 20, 0);
        g_regPassword = addCtrl(L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 40, 114, 180, 28, IdRegPassword, WS_EX_CLIENTEDGE);

        g_regSubmitBtn = addCtrl(L"BUTTON", L"ثبت‌نام و تأیید نهایی", BS_DEFPUSHBUTTON | WS_TABSTOP, 220, 165, 190, 34, IdRegSubmit);
        HWND closeBtn = addCtrl(L"BUTTON", L"بستن", BS_PUSHBUTTON | WS_TABSTOP, 40, 165, 160, 34, IdRegClose);

        g_regStatusLabel = addCtrl(L"STATIC", L"", SS_RIGHT, 24, 210, 390, 40, IdRegStatus);

        for (HWND h : {l1, l2, l3, g_regMobile, g_regRequestOtpBtn, g_regOtp, g_regPassword, g_regSubmitBtn, closeBtn, g_regStatusLabel})
        {
            if (h)
                SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g_dialogFont), TRUE);
        }
        return 0;
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
                g_otpTimerId = 0;
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
            const std::string message = ExtractJsonString(response, "message");
            if (ok && success == "true")
            {
                // Start 120s cooldown
                g_otpCooldownSeconds = 120;
                SetTimer(window, 999, 1000, nullptr);
                const std::wstring btnTxt = std::to_wstring(g_otpCooldownSeconds) + L" ثانیه صبرا...";
                SetWindowTextW(g_regRequestOtpBtn, btnTxt.c_str());

                const std::wstring msgWide = message.empty() ? L"کد پیامکی ارسال شد. لطفاً آن را وارد نمایید." : DecodeJsonString(message);
                SetWindowTextW(g_regStatusLabel, msgWide.c_str());
                SetFocus(g_regOtp);
                MessageBoxW(window, msgWide.c_str(), L"پیامک تأیید", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                EnableWindow(g_regRequestOtpBtn, TRUE);
                const std::wstring err = message.empty() ? L"خطا در ارسال پیامک." : DecodeJsonString(message);
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
            const std::string message = ExtractJsonString(response, "message");
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
                }
                MessageBoxW(window, L"ثبت‌نام با موفقیت انجام شد و وارد شدید.", L"موفقیت", MB_OK | MB_ICONINFORMATION);
                DestroyWindow(window);
            }
            else
            {
                const std::wstring err = message.empty() ? L"کد تأیید اشتباه یا منقضی شده است." : DecodeJsonString(message);
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
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        registered = true;
    }

    const int dpi = GetWindowDpi(parent);
    const int w = ScaleDpi(450, dpi);
    const int h = ScaleDpi(290, dpi);

    RECT parentRect{};
    GetWindowRect(parent, &parentRect);
    const int x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
    const int y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kOtpRegisterWindowClass,
                               L"ثبت‌نام حساب کاربری (SMS OTP)",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                               x, y, w, h, parent, nullptr, g_instance, nullptr);
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

void CreateFonts(int dpi)
{
    g_dialogFont = CreateFontW(-ScaleDpi(13, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, kFontFamily);
    g_titleFont = CreateFontW(-ScaleDpi(24, dpi), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, kFontFamily);
    g_titleFontSub = CreateFontW(-ScaleDpi(11, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, kFontFamily);
    g_badgeFont = CreateFontW(-ScaleDpi(11, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, kFontFamily);
    g_valueFont = CreateFontW(-ScaleDpi(12, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, kFontFamily);
    g_buttonFont = CreateFontW(-ScaleDpi(13, dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, kFontFamily);
}

void DestroyFonts()
{
    for (HFONT* font : {&g_dialogFont, &g_titleFont, &g_titleFontSub, &g_badgeFont, &g_valueFont, &g_buttonFont})
    {
        if (*font)
        {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}

bool IsActionButtonId(WORD id)
{
    return id == IdOk || id == IdCancel || id == IdReset || id == IdDemoManager ||
           id == IdUserLogin || id == IdUserRegister;
}

void DrawActionButton(LPDRAWITEMSTRUCT item)
{
    const WORD id = static_cast<WORD>(item->CtlID);
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool isOk = (id == IdOk);
    const bool isLogin = (id == IdUserLogin);
    const bool isRegister = (id == IdUserRegister);

    COLORREF fill = RGB(36, 42, 47);
    COLORREF border = RGB(68, 77, 86);
    COLORREF text = RGB(232, 236, 241);

    if (isOk)
    {
        fill = pressed ? RGB(0, 150, 115) : RGB(0, 184, 140);
        border = RGB(0, 210, 160);
        text = RGB(255, 255, 255);
    }
    else if (isLogin)
    {
        fill = pressed ? RGB(0, 120, 95) : RGB(0, 150, 115);
        border = RGB(0, 184, 140);
        text = RGB(255, 255, 255);
    }
    else if (isRegister)
    {
        fill = pressed ? RGB(30, 70, 110) : RGB(40, 90, 140);
        border = RGB(70, 130, 200);
        text = RGB(230, 240, 255);
    }
    else if (pressed)
    {
        fill = RGB(28, 33, 37);
    }

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(item->hDC, brush);
    HGDIOBJ oldPen = SelectObject(item->hDC, pen);
    RoundRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom, 6, 6);
    SelectObject(item->hDC, oldPen);
    SelectObject(item->hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t btnText[64]{};
    GetWindowTextW(item->hwndItem, btnText, static_cast<int>(std::size(btnText)));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text);
    HGDIOBJ oldFont = SelectObject(item->hDC, g_buttonFont ? g_buttonFont : g_dialogFont);
    RECT tr = item->rcItem;
    DrawTextW(item->hDC, btnText, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item->hDC, oldFont);
}

void PopulateControls(const VideoSettings& s)
{
    SendMessageW(g_resolutionCombo, CB_RESETCONTENT, 0, 0);
    int selected = -1;
    for (size_t i = 0; i < kKnownResolutions.size(); i++)
    {
        const auto& r = kKnownResolutions[i];
        const std::wstring label = std::to_wstring(r.width) + L" x " + std::to_wstring(r.height);
        SendMessageW(g_resolutionCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (r.width == s.width && r.height == s.height)
            selected = static_cast<int>(i);
    }
    if (selected >= 0)
        SendMessageW(g_resolutionCombo, CB_SETCURSEL, selected, 0);
    SendMessageW(g_windowedCheck, BM_SETCHECK, s.windowed ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_highQualityCheck, BM_SETCHECK, s.highQuality ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_hdModelsCheck, BM_SETCHECK, s.hdModels ? BST_CHECKED : BST_UNCHECKED, 0);
}

void PopulateMouseControls(const SystemMouseSettings& s)
{
    if (g_sensSlider)
        SendMessageW(g_sensSlider, TBM_SETPOS, TRUE, std::clamp(s.speed, 1, 20));
    if (g_enhancePointerCheck)
        SendMessageW(g_enhancePointerCheck, BM_SETCHECK, s.acceleration ? BST_CHECKED : BST_UNCHECKED, 0);
    UpdateSensDisplay();
}

void CreateControls(HWND window, int dpi)
{
    const auto addCtrl = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                             int x, int y, int w, int h, WORD id, DWORD exStyle = 0) {
        return CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                               ScaleDpi(x, dpi), ScaleDpi(y, dpi), ScaleDpi(w, dpi), ScaleDpi(h, dpi),
                               window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
    };

    g_demoManagerBtn = addCtrl(L"BUTTON", L"Demo Manager", BS_OWNERDRAW | WS_TABSTOP, 24, 20, 110, 36, IdDemoManager);

    // Section 1: Video
    addCtrl(L"STATIC", L"تصویر", SS_RIGHT, 450, 80, 150, 20, 0);
    addCtrl(L"STATIC", L":وضوح تصویر", SS_RIGHT, 480, 106, 120, 20, 0);
    g_resolutionCombo = addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 310, 102, 160, 200, IdResolutionCombo);
    g_windowedCheck = addCtrl(L"BUTTON", L"اجرا در پنجره", BS_AUTOCHECKBOX | BS_RIGHTBUTTON | WS_TABSTOP, 170, 104, 120, 24, IdWindowedCheck);

    g_highQualityCheck = addCtrl(L"BUTTON", L"کیفیت بالای تصویر", BS_AUTOCHECKBOX | BS_RIGHTBUTTON | WS_TABSTOP, 420, 140, 180, 24, IdHighQualityCheck);
    g_hdModelsCheck = addCtrl(L"BUTTON", L"مدل‌های باکیفیت (HD)", BS_AUTOCHECKBOX | BS_RIGHTBUTTON | WS_TABSTOP, 170, 140, 180, 24, IdHdModelsCheck);

    // Section 2: Mouse
    addCtrl(L"STATIC", L"ماوس و نشانه", SS_RIGHT, 450, 185, 150, 20, 0);
    addCtrl(L"STATIC", L":سرعت نشانگر ویندوز", SS_RIGHT, 460, 212, 140, 20, 0);
    g_sensSlider = addCtrl(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, 200, 210, 250, 28, IdSensSlider);
    SendMessageW(g_sensSlider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));
    g_sensValueText = addCtrl(L"STATIC", L"10 / 20", SS_CENTER, 140, 214, 55, 20, IdSensValueText);

    g_enhancePointerCheck = addCtrl(L"BUTTON", L"افزایش دقت نشانگر (شتاب ویندوز)", BS_AUTOCHECKBOX | BS_RIGHTBUTTON | WS_TABSTOP, 350, 248, 250, 24, IdEnhancePointerCheck);

    // Section 3: User Account & Cloud Sync
    addCtrl(L"STATIC", L"حساب کاربری و سابسکریپشن", SS_RIGHT, 420, 290, 180, 20, 0);

    addCtrl(L"STATIC", L":شماره موبایل", SS_RIGHT, 500, 318, 100, 20, 0);
    g_userPhone = addCtrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 320, 315, 175, 26, IdUserPhone, WS_EX_CLIENTEDGE);

    addCtrl(L"STATIC", L":رمز عبور", SS_RIGHT, 240, 318, 70, 20, 0);
    g_userPassword = addCtrl(L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 120, 315, 115, 26, IdUserPassword, WS_EX_CLIENTEDGE);

    g_userLoginBtn = addCtrl(L"BUTTON", L"ورود", BS_OWNERDRAW | WS_TABSTOP, 24, 314, 85, 28, IdUserLogin);
    g_userRegisterBtn = addCtrl(L"BUTTON", L"ثبت‌نام", BS_OWNERDRAW | WS_TABSTOP, 510, 350, 90, 26, IdUserRegister);

    g_userStatusLabel = addCtrl(L"STATIC", L"وارد نشده‌اید (مهمان: کانفیگ پیش‌فرض بازی لود می‌شود)", SS_RIGHT, 120, 354, 380, 20, IdUserStatus);

    // Section 4: Footer
    g_statusLabel = addCtrl(L"STATIC", L"آماده بازی • کلاینت نسخه 0.0.1", SS_RIGHT, 24, 400, 576, 20, 0);
    g_cancelButton = addCtrl(L"BUTTON", L"انصراف", BS_OWNERDRAW | WS_TABSTOP, 24, 435, 100, 42, IdCancel);
    g_resetButton = addCtrl(L"BUTTON", L"بازنشانی", BS_OWNERDRAW | WS_TABSTOP, 135, 435, 110, 42, IdReset);
    g_okButton = addCtrl(L"BUTTON", L"اجرای بازی", BS_OWNERDRAW | WS_TABSTOP, 390, 435, 210, 42, IdOk);

    for (HWND h : {g_resolutionCombo, g_windowedCheck, g_highQualityCheck, g_hdModelsCheck,
                   g_sensValueText, g_enhancePointerCheck, g_statusLabel,
                   g_userPhone, g_userPassword, g_userStatusLabel})
    {
        if (h)
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g_dialogFont), TRUE);
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_dialogWindow = window;
        const int dpi = GetWindowDpi(window);
        CreateFonts(dpi);
        CreateControls(window, dpi);
        const VideoSettings s = ReadSettings();
        PopulateControls(s);
        g_initialMouse = ReadSystemMouseSettings();
        g_mouseAtLastApply = g_initialMouse;
        PopulateMouseControls(g_initialMouse);
        return 0;
    }
    case WM_DRAWITEM:
    {
        auto item = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (item && IsActionButtonId(static_cast<WORD>(item->CtlID)))
        {
            DrawActionButton(item);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND hwndCtl = reinterpret_cast<HWND>(lParam);
        SetBkMode(hdc, TRANSPARENT);
        if (hwndCtl == g_statusLabel)
        {
            SetTextColor(hdc, RGB(0, 210, 160));
        }
        else if (hwndCtl == g_userStatusLabel)
        {
            SetTextColor(hdc, RGB(0, 190, 240));
        }
        else
        {
            SetTextColor(hdc, RGB(220, 225, 230));
        }
        static HBRUSH bgBrush = CreateSolidBrush(RGB(18, 22, 25));
        return reinterpret_cast<LRESULT>(bgBrush);
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(window, &ps);
        RECT rc{};
        GetClientRect(window, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(18, 22, 25));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(hdc, g_titleFont);
        SetTextColor(hdc, RGB(245, 247, 250));
        RECT titleRc{rc.right - 260, 16, rc.right - 24, 48};
        DrawTextW(hdc, L"ALLCLIENT", -1, &titleRc, DT_RIGHT | DT_SINGLELINE);

        SelectObject(hdc, g_titleFontSub);
        SetTextColor(hdc, RGB(180, 190, 200));
        RECT subRc{rc.right - 260, 48, rc.right - 24, 70};
        DrawTextW(hdc, L"COUNTER-STRIKE / 1.6", -1, &subRc, DT_RIGHT | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_HSCROLL:
    {
        if (reinterpret_cast<HWND>(lParam) == g_sensSlider)
        {
            UpdateSensDisplay();
            g_mousePreviewChanged = true;
        }
        return 0;
    }
    case WM_COMMAND:
    {
        const WORD id = LOWORD(wParam);
        if (id == IdUserLogin)
        {
            PerformUserLogin(window);
            return 0;
        }
        if (id == IdUserRegister)
        {
            ShowOtpRegisterDialog(window);
            return 0;
        }
        if (id == IdDemoManager)
        {
            const auto exeDir = ExecutableRoot();
            const auto demoExe = exeDir / L"demomanager.exe";
            ShellExecuteW(window, L"open", demoExe.c_str(), nullptr, exeDir.c_str(), SW_SHOW);
            return 0;
        }
        if (id == IdReset)
        {
            VideoSettings def;
            def.width = 800;
            def.height = 600;
            def.windowed = false;
            def.highQuality = true;
            def.hdModels = false;
            PopulateControls(def);
            SystemMouseSettings defMouse;
            defMouse.speed = 10;
            defMouse.threshold1 = 6;
            defMouse.threshold2 = 10;
            defMouse.acceleration = 1;
            PopulateMouseControls(defMouse);
            SetStatus(L"تنظیمات به حالت پیش‌فرض بازگشتند.");
            return 0;
        }
        if (id == IdCancel)
        {
            if (g_mousePreviewChanged)
                WriteSystemMouseSettings(g_initialMouse);
            DestroyWindow(window);
            return 0;
        }
        if (id == IdOk)
        {
            if (ApplySettings())
            {
                g_launchRequested = true;
                ShowWindow(window, SW_HIDE);
                PostQuitMessage(0);
            }
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        DestroyFonts();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

void SyncPlayerConfig()
{
    if (!g_activeUserToken.empty())
    {
        // Only authenticated users sync back to host
        PushUserConfigToCloud(g_activeUserToken);
    }
    // Guest users will never upload anything
}

bool ShowVideoSettingsDialog(HINSTANCE instance, const GameNetAccessStatus& access_status)
{
    g_instance = instance;
    g_launchRequested = false;
    g_accessStatus = access_status;
    g_activeUserPhone.clear();
    g_activeUserToken.clear();

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClassName;
    wc.hbrBackground = nullptr;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    const int dpi = GetWindowDpi(nullptr);
    const int width = ScaleDpi(640, dpi);
    const int height = ScaleDpi(530, dpi);

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int x = std::max(0, (screenW - width) / 2);
    const int y = std::max(0, (screenH - height) / 2);

    HWND window = CreateWindowExW(WS_EX_APPWINDOW, kWindowClassName, L"راه‌انداز Allclient",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
                                  x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!window)
        return false;

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return g_launchRequested;
}
