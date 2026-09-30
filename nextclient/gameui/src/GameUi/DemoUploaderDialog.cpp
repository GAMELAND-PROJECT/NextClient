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
#include <set>

#ifdef MessageBox
#undef MessageBox
#endif

using namespace vgui2;
namespace fs = std::filesystem;

CDemoUploaderDialog::CDemoUploaderDialog(vgui2::Panel *parent) : Frame(parent, "DemoUploaderDialog")
{
    SetBounds(0, 0, 620, 440);
    SetSizeable(false);
    SetTitle("GameLand Match Demo Studio", true);

    m_pDemoList = new ListPanel(this, "DemoList");
    m_pDemoList->AddColumnHeader(0, "demoname", "Demo File", 270);
    m_pDemoList->AddColumnHeader(1, "map", "Map", 90);
    m_pDemoList->AddColumnHeader(2, "size", "Size", 75);
    m_pDemoList->AddColumnHeader(3, "date", "Recorded Date", 110);

    m_pPlayButton = new Button(this, "PlayButton", "Play Demo");
    m_pDeleteButton = new Button(this, "DeleteButton", "Delete Demo");
    m_pOpenFolderButton = new Button(this, "OpenFolderButton", "Demos Folder");
    m_pRefreshButton = new Button(this, "RefreshButton", "Refresh");
    m_pCloseButton = new Button(this, "CloseButton", "Close (F4)");

    m_pPlayButton->SetCommand("Play");
    m_pDeleteButton->SetCommand("Delete");
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

    m_pPlayButton->SetBounds(20, btnY, 110, btnH);
    m_pDeleteButton->SetBounds(138, btnY, 110, btnH);
    m_pOpenFolderButton->SetBounds(256, btnY, 130, btnH);
    m_pRefreshButton->SetBounds(394, btnY, 95, btnH);
    m_pCloseButton->SetBounds(497, btnY, 103, btnH);
}

struct DemoEntryItem
{
    std::string filename;
    std::string relpath;
    std::string fullpath;
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
    for (char& c : lower) c = (char)tolower(c);

    static const char* kMaps[] = {
        "de_dust2", "de_inferno", "de_mirage", "de_nuke", "de_train",
        "de_aztec", "de_cbble", "de_cpl_mill", "de_cpl_strike", "de_cpl_fire",
        "de_tuscan", "de_forge", "de_hell", "de_kabul", "de_dust",
        "cs_assault", "cs_italy", "cs_militia", "cs_office", "cs_747",
        "awp_india", "aim_map", "aim_headshot", "fy_pool_day", "fy_iceworld",
        "de_piranesi"
    };

    for (const char* m : kMaps)
    {
        if (lower.find(m) != std::string::npos)
            return m;
    }
    return "-";
}

void CDemoUploaderDialog::RefreshDemoList()
{
    m_pDemoList->RemoveAll();

    std::vector<DemoEntryItem> demos;
    std::set<std::string> seenNames;

    auto scanDir = [&](const std::string& dirPath, const std::string& prefixRel) {
        try {
            if (fs::exists(dirPath)) {
                for (const auto& entry : fs::directory_iterator(dirPath)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".dem") {
                        std::string fname = entry.path().filename().string();
                        if (seenNames.find(fname) != seenNames.end())
                            continue;
                        seenNames.insert(fname);

                        DemoEntryItem item;
                        item.filename = fname;
                        item.relpath = prefixRel.empty() ? item.filename : (prefixRel + "/" + item.filename);
                        item.fullpath = entry.path().string();
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

    // Scan main game root folder demos first (D:\Allclient\demos\)
    scanDir("demos", "demos");
    // Also scan cstrike/demos for compatibility
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
        kv->SetString("fullpath", d.fullpath.c_str());
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
    const char *szDemoName = kv->GetString("demoname", "");
    const char *szFullPath = kv->GetString("fullpath", "");

    if (!szDemoName[0]) return;

    if (engine != nullptr)
    {
        if (szFullPath[0] && g_pFullFileSystem != nullptr)
        {
            try {
                fs::path p(szFullPath);
                g_pFullFileSystem->AddSearchPathNoWrite(p.parent_path().string().c_str(), "GAME");
            } catch (...) {}
        }

        std::string cmd = std::format("viewdemo \"{}\"\n", szDemoName);
        engine->pfnClientCmd(cmd.c_str());
        OnClose();
    }
}

void CDemoUploaderDialog::DeleteSelectedDemo()
{
    if (m_pDemoList->GetSelectedItemsCount() == 0)
    {
        MessageBox *pBox = new MessageBox("Error", "Please select a demo from the list first.");
        pBox->DoModal();
        return;
    }

    int itemID = m_pDemoList->GetSelectedItem(0);
    KeyValues *kv = m_pDemoList->GetItem(itemID);
    const char *szFullPath = kv->GetString("fullpath", "");
    const char *szDemoName = kv->GetString("demoname", "");

    if (!szFullPath[0]) return;

    std::string promptMsg = std::string("Are you sure you want to delete this demo?\n\nFile: ") + szDemoName;
    int choice = MessageBoxA(
        nullptr,
        promptMsg.c_str(),
        "Delete Demo - GameLand Studio",
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST
    );

    if (choice == IDYES)
    {
        try {
            if (fs::exists(szFullPath)) {
                fs::remove(szFullPath);
            }
        } catch (...) {}

        RefreshDemoList();
    }
}

void CDemoUploaderDialog::OpenDemosFolder()
{
    CreateDirectoryA("demos", nullptr);
    WinExec("explorer.exe demos", SW_SHOW);
}

void CDemoUploaderDialog::OnCommand(const char *command)
{
    if (!strcmp(command, "Play"))
    {
        PlaySelectedDemo();
    }
    else if (!strcmp(command, "Delete"))
    {
        DeleteSelectedDemo();
    }
    else if (!strcmp(command, "OpenFolder"))
    {
        OpenDemosFolder();
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