#include "../engine.h"
#include "http_download/DownloadLoggerAggregator.h"

std::shared_ptr<DownloadLoggerAggregator> g_DownloadFileLogger;

static std::unique_ptr<HttpDownloadManager> g_HttpDownloadManager;

void CL_CreateHttpDownloadManager(IGameUI* game_ui,
                                  vgui2::ILocalize* localize,
                                  std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider)
{
    if (g_DownloadFileLogger == nullptr)
        g_DownloadFileLogger = std::make_shared<DownloadLoggerAggregator>();

    g_HttpDownloadManager = std::make_unique<HttpDownloadManager>(game_ui, localize, g_DownloadFileLogger, config_provider);
}

void CL_DeleteHttpDownloadManager()
{
    if (g_HttpDownloadManager)
    {
        g_HttpDownloadManager->Stop();
        g_HttpDownloadManager.reset();
    }

    g_DownloadFileLogger.reset();
}

HttpDownloadManagerInterface* CL_GetHttpDownloadManager()
{
    return g_HttpDownloadManager.get();
}

bool CL_AddDownloadFileLogger(DownloadFileLoggerInterface* logger)
{
    if (g_DownloadFileLogger == nullptr)
        g_DownloadFileLogger = std::make_shared<DownloadLoggerAggregator>();

    return g_DownloadFileLogger->AddLogger(logger);
}

bool CL_RemoveDonwloadFileLogger(DownloadFileLoggerInterface* logger)
{
    if (g_DownloadFileLogger == nullptr)
        g_DownloadFileLogger = std::make_shared<DownloadLoggerAggregator>();

    return g_DownloadFileLogger->RemoveLogger(logger);
}

void CL_HTTPSetDownloadUrl(const std::string& url)
{
    if (g_HttpDownloadManager == nullptr || cls == nullptr || cls->state == cactive_t::ca_active)
        return;

    g_HttpDownloadManager->SetUrl(url);
}

int CL_HttpGetDownloadQueueSize()
{
    return g_HttpDownloadManager ? g_HttpDownloadManager->GetDownloadQueueSize() : 0;
}

void CL_QueueHTTPDownload(const ResourceDescriptor& file_resource)
{
    if (g_HttpDownloadManager)
        g_HttpDownloadManager->Queue(file_resource);
}

void CL_HTTPUpdate()
{
    if (g_HttpDownloadManager)
        g_HttpDownloadManager->Update();
}

void CL_HTTPCancel_f()
{
    if (g_HttpDownloadManager)
        g_HttpDownloadManager->Stop();
}

void CL_HTTPStop_f()
{
    if (g_HttpDownloadManager)
        g_HttpDownloadManager->Stop();
}

void CL_MarkMapAsUsingHTTPDownload()
{
    eng()->CL_MarkMapAsUsingHTTPDownload();
}
