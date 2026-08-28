#include <Windows.h>
#include <CommCtrl.h>
#include <windowsx.h>

#include <algorithm>
#include <compare>
#include <cstdlib>
#include <iterator>
#include <ranges>
#include <set>
#include <string>
#include <vector>

#include "../next_launcher/GameNetAccess.h"

namespace
{
constexpr wchar_t kWindowClass[] = L"NextClientLauncherWindow";
constexpr wchar_t kTitle[] = L"NextClient Launcher";
constexpr wchar_t kSettingsKey[] = L"Software\\Valve\\Half-Life\\Settings";
constexpr wchar_t kLauncherKey[] = L"Software\\Valve\\Half-Life\\nextclient\\video_launcher";
constexpr int kWindowsPointerSpeeds[] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20};

enum ControlId
{
    IdResolution = 100,
    IdFullscreen,
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
    return std::to_wstring(std::clamp(position, 1, 11)) + L" / 11";
}

HINSTANCE g_instance{};
HFONT g_font{};
HFONT g_emphasisFont{};
HWND g_resolution{};
HWND g_fullscreen{};
HWND g_hdModels{};
HWND g_highQuality{};
HWND g_pointerSpeed{};
HWND g_pointerSpeedValue{};
HWND g_enhancePointer{};
HWND g_status{};
HWND g_subscriptionState{};
HWND g_subscriptionTag{};
HWND g_subscriptionDetails{};
HWND g_subscriptionRemaining{};
std::vector<Resolution> g_resolutions;
bool g_launchRequested{};
GameNetAccessStatus g_accessStatus;
SystemMouseSettings g_mouseAtLastApply{};
bool g_mousePreviewChanged{};

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

void PopulateSubscriptionStatus()
{
    std::wstring stateText;
    std::wstring detailText;
    std::wstring remainingText;
    const std::wstring expiry = WidenAscii(g_accessStatus.expiry_date);

    switch (g_accessStatus.state)
    {
    case GameNetAccessState::Active:
        stateText = L"ACTIVE SUBSCRIPTION";
        detailText = L"Expires: " + expiry;
        if (g_accessStatus.days_remaining == 0)
            remainingText = L"Expires today";
        else if (g_accessStatus.days_remaining > 0)
            remainingText = std::to_wstring(g_accessStatus.days_remaining) +
                L" days remaining";
        break;
    case GameNetAccessState::Expired:
        stateText = L"SUBSCRIPTION EXPIRED";
        detailText = L"Expired: " + expiry;
        if (g_accessStatus.days_remaining < 0)
            remainingText = std::to_wstring(-g_accessStatus.days_remaining) +
                L" days ago";
        break;
    case GameNetAccessState::TagMissing:
        stateText = L"SUBSCRIPTION INACTIVE";
        detailText = L"No active subscription was found.";
        break;
    case GameNetAccessState::InvalidEntry:
        stateText = L"SUBSCRIPTION DATA INVALID";
        detailText = L"The subscription record has an invalid expiration date.";
        break;
    case GameNetAccessState::ServiceUnavailable:
    default:
        stateText = L"STATUS UNAVAILABLE";
        detailText = L"Subscription could not be verified. Online is disabled; LAN remains available.";
        break;
    }

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
    Button_SetCheck(g_fullscreen, value.windowed ? BST_UNCHECKED : BST_CHECKED);
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
        SetStatus(L"Mouse preview active. Launch Game to keep it, or Cancel to revert.");
    }
    else
        SetStatus(L"Windows rejected the mouse preview setting.", true);
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
    value.windowed = Button_GetCheck(g_fullscreen) == BST_CHECKED ? 0 : 1;
    value.hdModels = Button_GetCheck(g_hdModels) == BST_CHECKED ? 1 : 0;
    value.videoLevel = Button_GetCheck(g_highQuality) == BST_CHECKED ? 1 : 0;
    return true;
}

bool ApplySettings()
{
    VideoSettings requested;
    if (!SettingsFromControls(requested))
    {
        SetStatus(L"Please select a valid resolution.", true);
        return false;
    }

    const VideoSettings previous = ReadSettings();
    const SystemMouseSettings previousMouse = g_mouseAtLastApply;
    const SystemMouseSettings requestedMouse = MouseSettingsFromControls(previousMouse);
    if (SameSettings(previous, requested) && SameSettings(previousMouse, requestedMouse))
    {
        SetStatus(L"Video settings are already up to date.");
        return true;
    }

    if (!SaveBackup(previous, previousMouse))
    {
        SetStatus(L"Could not create a settings backup. No changes were made.", true);
        return false;
    }
    if (!WriteSettings(requested) || !WriteSystemMouseSettings(requestedMouse))
    {
        WriteSettings(previous);
        WriteSystemMouseSettings(previousMouse);
        SetStatus(L"Could not apply settings; the previous values were restored.", true);
        return false;
    }

    g_mouseAtLastApply = requestedMouse;
    g_mousePreviewChanged = false;
    SetStatus(L"Video and Windows mouse settings applied successfully.");
    return true;
}

HWND AddControl(HWND parent, const wchar_t* type, const wchar_t* text, DWORD style,
                int x, int y, int width, int height, int id = 0, DWORD exStyle = 0)
{
    HWND control = CreateWindowExW(exStyle, type, text, WS_CHILD | WS_VISIBLE | style,
                                   x, y, width, height, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    return control;
}

void CreateControls(HWND window)
{
    InitializeNativeResolutionIfNeeded();

    AddControl(window, L"STATIC", L"Configure restart-sensitive video options, then launch NextClient.",
               SS_LEFT, 24, 18, 572, 22);
    AddControl(window, L"BUTTON", L"Display", BS_GROUPBOX, 18, 48, 578, 166);
    AddControl(window, L"STATIC", L"Resolution", SS_LEFT, 38, 78, 120, 22);
    g_resolution = AddControl(window, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                              176, 74, 210, 220, IdResolution);
    g_fullscreen = AddControl(window, L"BUTTON", L"Fullscreen", BS_AUTOCHECKBOX | WS_TABSTOP,
                              38, 116, 220, 24, IdFullscreen);
    g_hdModels = AddControl(window, L"BUTTON", L"HD player models", BS_AUTOCHECKBOX | WS_TABSTOP,
                            38, 148, 220, 24, IdHdModels);
    g_highQuality = AddControl(window, L"BUTTON", L"High video quality", BS_AUTOCHECKBOX | WS_TABSTOP,
                               294, 148, 240, 24, IdHighQuality);
    AddControl(window, L"STATIC", L"OpenGL renderer, 32-bit color", SS_LEFT, 294, 116, 244, 24);

    AddControl(window, L"BUTTON", L"Windows mouse", BS_GROUPBOX, 18, 224, 578, 96);
    AddControl(window, L"STATIC", L"Pointer speed", SS_LEFT, 38, 252, 112, 22);
    g_pointerSpeed = AddControl(window, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                150, 244, 280, 34, IdPointerSpeed);
    SendMessageW(g_pointerSpeed, TBM_SETRANGE, TRUE, MAKELPARAM(1, 11));
    SendMessageW(g_pointerSpeed, TBM_SETTICFREQ, 1, 0);
    g_pointerSpeedValue = AddControl(window, L"STATIC", L"6 / 11", SS_CENTER, 438, 250, 54, 22, IdPointerSpeedValue);
    g_enhancePointer = AddControl(window, L"BUTTON", L"Enhance pointer precision",
                                  BS_AUTOCHECKBOX | WS_TABSTOP, 38, 282, 250, 24, IdEnhancePointer);
    AddControl(window, L"STATIC", L"This changes the current Windows user setting.",
               SS_LEFT, 310, 284, 260, 22);

    AddControl(window, L"BUTTON", L"Online subscription", BS_GROUPBOX, 18, 330, 578, 104);
    g_subscriptionState = AddControl(window, L"STATIC", L"Checking subscription...",
                                     SS_LEFT, 38, 354, 536, 22, IdSubscriptionState);
    g_subscriptionTag = AddControl(window, L"STATIC", L"",
                                   SS_LEFT, 38, 382, 188, 24, IdSubscriptionTag);
    SendMessageW(g_subscriptionTag, WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);
    g_subscriptionDetails = AddControl(window, L"STATIC", L"",
                                       SS_LEFT, 238, 378, 336, 22, IdSubscriptionDetails);
    g_subscriptionRemaining = AddControl(window, L"STATIC", L"",
                                         SS_LEFT, 238, 402, 336, 22, IdSubscriptionRemaining);
    SendMessageW(g_subscriptionRemaining, WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_emphasisFont), TRUE);
    PopulateSubscriptionStatus();

    AddControl(window, L"BUTTON", L"Restore", BS_PUSHBUTTON | WS_TABSTOP, 18, 452, 112, 32, IdRestore);
    AddControl(window, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 140, 452, 100, 32, IdCancel);
    AddControl(window, L"BUTTON", L"Launch Game", BS_DEFPUSHBUTTON | WS_TABSTOP,
               460, 452, 136, 32, IdLaunch);
    g_status = AddControl(window, L"STATIC", L"Ready", SS_LEFT, 20, 502, 576, 36, IdStatus);

    const VideoSettings current = ReadSettings();
    PopulateResolutions(current);
    SetControls(current);
    g_mouseAtLastApply = ReadSystemMouseSettings();
    g_mousePreviewChanged = false;
    SetMouseControls(g_mouseAtLastApply);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateControls(window);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IdLaunch:
            if (ApplySettings())
            {
                g_launchRequested = true;
                DestroyWindow(window);
            }
            return 0;
        case IdCancel:
            RevertMousePreview();
            DestroyWindow(window);
            return 0;
        case IdEnhancePointer:
            if (HIWORD(wParam) == BN_CLICKED)
                ApplyMousePreview();
            return 0;
        case IdRestore:
        {
            // Resolution is intentionally not touched. Restore only resets the
            // non-resolution defaults and preserves the user's current choice.
            Button_SetCheck(g_fullscreen, BST_CHECKED);
            Button_SetCheck(g_hdModels, BST_UNCHECKED);
            Button_SetCheck(g_highQuality, BST_CHECKED);

            SystemMouseSettings mouseDefaults = g_mouseAtLastApply;
            mouseDefaults.speed = SliderPositionToPointerSpeed(4);
            mouseDefaults.acceleration[0] = 6;
            mouseDefaults.acceleration[1] = 10;
            mouseDefaults.acceleration[2] = 1;
            SetMouseControls(mouseDefaults);
            ApplyMousePreview();
            SetStatus(L"Defaults selected; resolution preserved. Fullscreen and high quality on, HD off, pointer speed 4 / 11, precision on.");
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
            ApplyMousePreview();
            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        if (reinterpret_cast<HWND>(lParam) == g_subscriptionState)
        {
            const bool active = g_accessStatus.state == GameNetAccessState::Active;
            SetTextColor(reinterpret_cast<HDC>(wParam), active ? RGB(24, 132, 76) : RGB(190, 48, 48));
        }
        else if (reinterpret_cast<HWND>(lParam) == g_subscriptionTag)
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(28, 73, 128));
        else if (reinterpret_cast<HWND>(lParam) == g_subscriptionDetails)
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(80, 80, 80));
        else if (reinterpret_cast<HWND>(lParam) == g_subscriptionRemaining)
        {
            const bool active = g_accessStatus.state == GameNetAccessState::Active;
            SetTextColor(reinterpret_cast<HDC>(wParam), active ? RGB(24, 132, 76) : RGB(190, 48, 48));
        }
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));

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

bool ShowVideoSettingsDialog(HINSTANCE instance, const GameNetAccessStatus& access_status)
{
    g_instance = instance;
    g_launchRequested = false;
    g_accessStatus = access_status;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    g_font = CreateFontIndirectW(&metrics.lfMessageFont);
    LOGFONTW emphasisFont = metrics.lfMessageFont;
    emphasisFont.lfWeight = FW_BOLD;
    g_emphasisFont = CreateFontIndirectW(&emphasisFont);

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    windowClass.lpszClassName = kWindowClass;
    windowClass.hIconSm = windowClass.hIcon;
    if (!RegisterClassExW(&windowClass))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    RECT rect{0, 0, 616, 558};
    AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND window = CreateWindowExW(0, kWindowClass, kTitle,
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!window)
        return false;

    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_font)
        DeleteObject(g_font);
    if (g_emphasisFont)
        DeleteObject(g_emphasisFont);
    return g_launchRequested;
}
