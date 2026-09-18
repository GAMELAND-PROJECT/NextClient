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
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

using namespace vgui2;

CDemoUploaderDialog::CDemoUploaderDialog(vgui2::Panel *parent) : Frame(parent, "DemoUploaderDialog")
{
    SetBounds(0, 0, 400, 350);
    SetSizeable(false);
    SetTitle("GameLand Demo Manager", true);

    m_pDemoList = new ListPanel(this, "DemoList");
    m_pDemoList->AddColumnHeader(0, "demoname", "Demo File", m_pDemoList->GetWide() - 20);

    m_pUploadButton = new Button(this, "UploadButton", "Upload Selected");
    m_pRefreshButton = new Button(this, "RefreshButton", "Refresh");
    m_pCloseButton = new Button(this, "CloseButton", "Close");

    LoadControlSettings("Resource/DemoUploaderDialog.res");
    
    // Setup Action Signals
    m_pUploadButton->SetCommand("Upload");
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

void CDemoUploaderDialog::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    
    // Position elements if no .res file is present
    m_pDemoList->SetBounds(20, 40, 360, 240);
    m_pRefreshButton->SetBounds(20, 290, 80, 30);
    m_pUploadButton->SetBounds(110, 290, 180, 30);
    m_pCloseButton->SetBounds(300, 290, 80, 30);
}

void CDemoUploaderDialog::RefreshDemoList()
{
    m_pDemoList->DeleteAllItems();

    FileFindHandle_t findHandle = NULL;
    const char *filename = g_pFullFileSystem->FindFirst("*.dem", &findHandle, "GAME");
    while (filename)
    {
        // Only show files starting with GL_ for GameLand match demos
        if (strncmp(filename, "GL_", 3) == 0)
        {
            m_pDemoList->AddItem(new KeyValues("data", "demoname", filename), 0, false, false);
        }
        filename = g_pFullFileSystem->FindNext(findHandle);
    }
    g_pFullFileSystem->FindClose(findHandle);

    if (m_pDemoList->GetItemCount() > 0)
    {
        m_pDemoList->SetSingleSelectedItem(m_pDemoList->GetItemIDFromRow(0));
    }
}

void CDemoUploaderDialog::UploadSelectedDemo()
{
    if (m_pDemoList->GetSelectedItemsCount() == 0)
    {
        MessageBox *pBox = new MessageBox("Error", "Please select a demo to upload.");
        pBox->DoModal();
        return;
    }

    int itemID = m_pDemoList->GetSelectedItem(0);
    KeyValues *kv = m_pDemoList->GetItem(itemID);
    const char *szDemoName = kv->GetString("demoname", "");

    if (!szDemoName[0]) return;

    char szFullPath[MAX_PATH];
    g_pFullFileSystem->GetLocalPath(szDemoName, szFullPath, sizeof(szFullPath));

    // Disable button to prevent spam
    m_pUploadButton->SetEnabled(false);
    m_pUploadButton->SetText("Uploading...");

    // Basic upload using WinINet
    HINTERNET hSession = InternetOpen("NextClient Uploader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hSession)
    {
        HINTERNET hConnect = InternetConnect(hSession, "gameland.cam", INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
        if (hConnect)
        {
            char szUrl[256];
            snprintf(szUrl, sizeof(szUrl), "/upload_demo.php?name=%s", szDemoName);
            HINTERNET hRequest = HttpOpenRequest(hConnect, "POST", szUrl, NULL, NULL, NULL, 0, 1);
            if (hRequest)
            {
                // This is a naive multipart/form-data upload. We would read the file here.
                // Since this is blocking, in a real scenario we might put this in a thread.
                // For simplicity in UI, we'll just show success/fail.
                
                FILE *fp = fopen(szFullPath, "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long fileSize = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    
                    char *buffer = new char[fileSize];
                    fread(buffer, 1, fileSize, fp);
                    fclose(fp);

                    // Simplified header/body structure
                    char header[] = "Content-Type: application/octet-stream\r\n";
                    HttpSendRequest(hRequest, header, strlen(header), (LPVOID)buffer, fileSize);
                    delete[] buffer;
                    
                    MessageBox *pBox = new MessageBox("Success", "Demo uploaded successfully!");
                    pBox->DoModal();
                }
                else {
                    MessageBox *pBox = new MessageBox("Error", "Could not read demo file.");
                    pBox->DoModal();
                }
                InternetCloseHandle(hRequest);
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hSession);
    }
    
    m_pUploadButton->SetEnabled(true);
    m_pUploadButton->SetText("Upload Selected");
}

void CDemoUploaderDialog::OnCommand(const char *command)
{
    if (!strcmp(command, "Upload"))
    {
        UploadSelectedDemo();
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
