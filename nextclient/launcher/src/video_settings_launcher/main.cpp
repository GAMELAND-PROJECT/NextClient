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

namespace
{
constexpr wchar_t kWindowClass[] = L"NextClientLauncherWindow";
constexpr wchar_t kTitle[] = L"NextClient Launcher";
constexpr wchar_t kSettingsKey[] = L"Software\\Valve\\Half-Life\\Settings";
constexpr wchar_t kLauncherKey[] = L"Software\\Valve\\Half-Life\\nextclient\\video_launcher";

enum ControlId
{
    IdResolution = 100,
    IdFullscreen,
    IdHdModels,
    IdHighQuality,
    IdApply,
    IdLaunch,
    IdRestore,
    IdSafeMode,
    IdStatus,
    IdCancel,
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

bool SameSettings(const VideoSettings& left, const VideoSettings& right)
{
    return left.width == right.width && left.height == right.height && left.bpp == right.bpp &&
           left.windowed == right.windowed && left.hdModels == right.hdModels &&
           left.videoLevel == right.videoLevel;
}

HINSTANCE g_instance{};
HFONT g_font{};
HWND g_resolution{};
HWND g_fullscreen{};
HWND g_hdModels{};
HWND g_highQuality{};
HWND g_status{};
std::vector<Resolution> g_resolutions;
bool g_launchRequested{};

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

bool SaveBackup(const VideoSettings& value)
{
    RegistryKey key(HKEY_CURRENT_USER, kLauncherKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
    return key.valid() &&
           key.WriteDword(L"BackupWidth", value.width) &&
           key.WriteDword(L"BackupHeight", value.height) &&
           key.WriteDword(L"BackupBPP", value.bpp) &&
           key.WriteDword(L"BackupWindowed", value.windowed) &&
           key.WriteDword(L"BackupHDModels", value.hdModels) &&
           key.WriteDword(L"BackupVideoLevel", value.videoLevel) &&
           key.WriteDword(L"BackupValid", 1);
}

bool LoadBackup(VideoSettings& value)
{
    RegistryKey key(HKEY_CURRENT_USER, kLauncherKey, KEY_QUERY_VALUE | KEY_SET_VALUE);
    if (!key.valid() || key.ReadDword(L"BackupValid", 0) != 1)
        return false;

    value.width = key.ReadDword(L"BackupWidth", 800);
    value.height = key.ReadDword(L"BackupHeight", 600);
    value.bpp = key.ReadDword(L"BackupBPP", 32);
    value.windowed = key.ReadDword(L"BackupWindowed", 0);
    value.hdModels = key.ReadDword(L"BackupHDModels", 0);
    value.videoLevel = key.ReadDword(L"BackupVideoLevel", 1);
    return true;
}

void SetStatus(const wchar_t* text, bool error = false)
{
    SetWindowTextW(g_status, text);
    InvalidateRect(g_status, nullptr, TRUE);
    if (error)
        MessageBeep(MB_ICONERROR);
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
    if (SameSettings(previous, requested))
    {
        SetStatus(L"Video settings are already up to date.");
        return true;
    }

    if (!SaveBackup(previous))
    {
        SetStatus(L"Could not create a settings backup. No changes were made.", true);
        return false;
    }
    if (!WriteSettings(requested))
    {
        WriteSettings(previous);
        SetStatus(L"Could not write video settings; the previous values were restored.", true);
        return false;
    }

    SetStatus(L"Settings applied. They will be used on the next game launch.");
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

    AddControl(window, L"BUTTON", L"Safe mode", BS_PUSHBUTTON | WS_TABSTOP, 18, 232, 112, 32, IdSafeMode);
    AddControl(window, L"BUTTON", L"Restore", BS_PUSHBUTTON | WS_TABSTOP, 140, 232, 100, 32, IdRestore);
    AddControl(window, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 250, 232, 90, 32, IdCancel);
    AddControl(window, L"BUTTON", L"Apply", BS_PUSHBUTTON | WS_TABSTOP, 350, 232, 100, 32, IdApply);
    AddControl(window, L"BUTTON", L"Launch Game", BS_DEFPUSHBUTTON | WS_TABSTOP,
               460, 232, 136, 32, IdLaunch);
    g_status = AddControl(window, L"STATIC", L"Ready", SS_LEFT, 20, 282, 576, 36, IdStatus);

    const VideoSettings current = ReadSettings();
    PopulateResolutions(current);
    SetControls(current);
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
        case IdApply:
            ApplySettings();
            return 0;
        case IdLaunch:
            if (ApplySettings())
            {
                g_launchRequested = true;
                DestroyWindow(window);
            }
            return 0;
        case IdCancel:
            DestroyWindow(window);
            return 0;
        case IdSafeMode:
        {
            const VideoSettings safe{800, 600, 32, 1, 0, 1};
            const VideoSettings previous = ReadSettings();
            if (SaveBackup(previous) && WriteSettings(safe))
            {
                SetControls(safe);
                SetStatus(L"Safe mode applied: 800 x 600 windowed, OpenGL, 32-bit.");
            }
            else
            {
                WriteSettings(previous);
                SetStatus(L"Could not apply safe mode.", true);
            }
            return 0;
        }
        case IdRestore:
        {
            VideoSettings backup;
            if (LoadBackup(backup) && WriteSettings(backup))
            {
                SetControls(backup);
                SetStatus(L"Previous video settings restored.");
            }
            else
                SetStatus(L"No valid backup is available.", true);
            return 0;
        }
        default:
            break;
        }
        break;

    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

bool ShowVideoSettingsDialog(HINSTANCE instance)
{
    g_instance = instance;
    g_launchRequested = false;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    g_font = CreateFontIndirectW(&metrics.lfMessageFont);

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

    RECT rect{0, 0, 616, 338};
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
    return g_launchRequested;
}
