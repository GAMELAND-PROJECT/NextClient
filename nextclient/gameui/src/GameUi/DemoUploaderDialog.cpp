#include "GameUi.h"
#include "DemoUploaderDialog.h"
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/MessageBox.h>
#include <vgui/ISurfaceNext.h>
#include <KeyValues.h>

#include "FileSystem.h"
#include <tier0/memdbgon.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <sstream>
#include <format>
#include <chrono>

#ifdef MessageBox
#undef MessageBox
#endif

using namespace vgui2;
namespace fs = std::filesystem;

CDemoUploaderDialog::CDemoUploaderDialog(vgui2::Panel *parent) : Frame(parent, "DemoUploaderDialog")
{
    SetBounds(0, 0, 620, 440);
    SetSizeable(false);
    SetTitle("GameLand Match Demo Studio & Video Converter", true);

    m_pDemoList = new ListPanel(this, "DemoList");
    m_pDemoList->AddColumnHeader(0, "demoname", "Demo File", 270);
    m_pDemoList->AddColumnHeader(1, "map", "Map", 90);
    m_pDemoList->AddColumnHeader(2, "size", "Size", 75);
    m_pDemoList->AddColumnHeader(3, "date", "Recorded Date", 110);

    m_pConvertButton = new Button(this, "ConvertButton", "Convert to MP4 (1080p)");
    m_pPlayButton = new Button(this, "PlayButton", "Play Demo");
    m_pOpenFolderButton = new Button(this, "OpenFolderButton", "Videos Folder");
    m_pRefreshButton = new Button(this, "RefreshButton", "Refresh");
    m_pCloseButton = new Button(this, "CloseButton", "Close (F4)");

    m_pConvertButton->SetCommand("Convert");
    m_pPlayButton->SetCommand("Play");
    m_pOpenFolderButton->SetCommand("OpenFolder");
    m_pRefreshButton->SetCommand("Refresh");
    m_pCloseButton->SetCommand("Close");

    RefreshDemoList();
}

CDemoUploaderDialog::~CDemoUploaderDialog()
{
}

void CDemoUploaderDialog::Activate()
{
    BaseClass::Activate();
    RefreshDemoList();
}

void CDemoUploaderDialog::OnKeyCodePressed(vgui2::KeyCode code)
{
    if (code == vgui2::KEY_F4 || code == vgui2::KEY_ESCAPE)
    {
        OnClose();
        return;
    }
    BaseClass::OnKeyCodePressed(code);
}

void CDemoUploaderDialog::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    // Center dialog on screen
    int screenW = 1024, screenH = 768;
    if (vgui2::surface() != nullptr)
        vgui2::surface()->GetScreenSize(screenW, screenH);
    SetPos((screenW - 620) / 2, (screenH - 440) / 2);

    m_pDemoList->SetBounds(20, 42, 580, 335);

    const int btnY = 390;
    const int btnH = 30;

    m_pConvertButton->SetBounds(20, btnY, 175, btnH);
    m_pPlayButton->SetBounds(205, btnY, 95, btnH);
    m_pOpenFolderButton->SetBounds(310, btnY, 115, btnH);
    m_pRefreshButton->SetBounds(435, btnY, 80, btnH);
    m_pCloseButton->SetBounds(525, btnY, 75, btnH);
}

struct DemoEntryItem
{
    std::string filename;
    std::string relpath;
    std::string map;
    std::string sizeStr;
    std::string dateStr;
    uint64_t timestamp{0};
};

static std::string FormatBytes(uintmax_t bytes)
{
    char buf[32]{};
    if (bytes >= 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    return buf;
}

static std::string ExtractMap(const std::string& name)
{
    std::string lower = name;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);

    for (const auto* prefix : {"de_", "cs_", "aim_", "awp_", "fy_", "surf_", "zm_"})
    {
        size_t pos = lower.find(prefix);
        if (pos != std::string::npos)
        {
            size_t endPos = lower.find_first_of("._", pos + 3);
            if (endPos != std::string::npos)
                return name.substr(pos, endPos - pos);
            return name.substr(pos);
        }
    }
    return "cstrike";
}

void CDemoUploaderDialog::RefreshDemoList()
{
    m_pDemoList->DeleteAllItems();

    std::vector<DemoEntryItem> demos;

    auto scanDir = [&](const std::string& dirPath, const std::string& prefixRel) {
        try {
            if (fs::exists(dirPath)) {
                for (const auto& entry : fs::directory_iterator(dirPath)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".dem") {
                        DemoEntryItem item;
                        item.filename = entry.path().filename().string();
                        item.relpath = prefixRel.empty() ? item.filename : (prefixRel + "/" + item.filename);
                        item.map = ExtractMap(item.filename);

                        auto fsize = entry.file_size();
                        item.sizeStr = FormatBytes(fsize);

                        auto ftime = entry.last_write_time();
                        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
                        std::tm tm{};
                        localtime_s(&tm, &cftime);

                        char dtBuf[32]{};
                        std::strftime(dtBuf, sizeof(dtBuf), "%Y-%m-%d %H:%M", &tm);
                        item.dateStr = dtBuf;
                        item.timestamp = static_cast<uint64_t>(cftime);

                        demos.push_back(item);
                    }
                }
            }
        } catch (...) {}
    };

    scanDir("cstrike/demos", "demos");
    scanDir("cstrike", "");

    // Sort newest first
    std::sort(demos.begin(), demos.end(), [](const DemoEntryItem& a, const DemoEntryItem& b) {
        return a.timestamp > b.timestamp;
    });

    for (const auto& d : demos)
    {
        KeyValues *kv = new KeyValues("data");
        kv->SetString("demoname", d.filename.c_str());
        kv->SetString("relpath", d.relpath.c_str());
        kv->SetString("map", d.map.c_str());
        kv->SetString("size", d.sizeStr.c_str());
        kv->SetString("date", d.dateStr.c_str());

        m_pDemoList->AddItem(kv, 0, false, false);
    }

    if (m_pDemoList->GetItemCount() > 0)
    {
        m_pDemoList->SetSingleSelectedItem(m_pDemoList->GetItemIDFromRow(0));
    }
}

void CDemoUploaderDialog::ConvertSelectedDemo()
{
    if (m_pDemoList->GetSelectedItemsCount() == 0)
    {
        MessageBox *pBox = new MessageBox("Error", "Please select a demo from the list first.");
        pBox->DoModal();
        return;
    }

    int itemID = m_pDemoList->GetSelectedItem(0);
    KeyValues *kv = m_pDemoList->GetItem(itemID);
    const char *szRelPath = kv->GetString("relpath", "");
    const char *szDemoName = kv->GetString("demoname", "");

    if (!szRelPath[0]) return;

    CreateDirectoryA("cstrike\\videos", nullptr);

    char exePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    std::ostringstream cmd;
    cmd << "\"" << exePath << "\" -game cstrike -sw -noborder -windowed -width 1280 -height 720"
        << " -novid +viewdemo \"" << szRelPath << "\" -democonvert";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::string cmdStr = cmd.str();

    BOOL created = CreateProcessA(
        nullptr,
        cmdStr.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (created)
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        std::string msg = "Demo conversion to 1080p 60FPS MP4 has started in the background!\n\nDemo: " + std::string(szDemoName) + "\nOutput: cstrike/videos/\n\nYou can keep playing or close this window.";
        MessageBox *pBox = new MessageBox("GameLand Studio", msg.c_str());
        pBox->DoModal();
    }
    else
    {
        MessageBox *pBox = new MessageBox("Error", "Failed to start background video converter.");
        pBox->DoModal();
    }
}

void CDemoUploaderDialog::PlaySelectedDemo()
{
    if (m_pDemoList->GetSelectedItemsCount() == 0)
    {
        MessageBox *pBox = new MessageBox("Error", "Please select a demo from the list first.");
        pBox->DoModal();
        return;
    }

    int itemID = m_pDemoList->GetSelectedItem(0);
    KeyValues *kv = m_pDemoList->GetItem(itemID);
    const char *szRelPath = kv->GetString("relpath", "");

    if (!szRelPath[0]) return;

    if (engine != nullptr)
    {
        std::string cmd = std::format("viewdemo \"{}\"\n", szRelPath);
        engine->pfnClientCmd(cmd.c_str());
        OnClose();
    }
}

void CDemoUploaderDialog::OpenVideosFolder()
{
    CreateDirectoryA("cstrike\\videos", nullptr);
    WinExec("explorer.exe cstrike\\videos", SW_SHOW);
}

void CDemoUploaderDialog::OnCommand(const char *command)
{
    if (!strcmp(command, "Convert"))
    {
        ConvertSelectedDemo();
    }
    else if (!strcmp(command, "Play"))
    {
        PlaySelectedDemo();
    }
    else if (!strcmp(command, "OpenFolder"))
    {
        OpenVideosFolder();
    }
    else if (!strcmp(command, "Refresh"))
    {
        RefreshDemoList();
    }
    else if (!strcmp(command, "Close"))
    {
        OnClose();
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}
