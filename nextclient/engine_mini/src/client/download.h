#pragma once

#include <cstdint>
#include <resource/ResourceDescriptor.h>
#include "http_download/DownloadLoggerAggregator.h"

// Large enough for legitimate GoldSrc resources while preventing a remote
// server from forcing an unbounded allocation or decompression on the client.
inline constexpr uint32_t kMaxServerResourceBytes = 64u * 1024u * 1024u;
inline constexpr uint32_t kMaxServerDownloadBatchBytes = 512u * 1024u * 1024u;

extern std::shared_ptr<DownloadLoggerAggregator> g_DownloadFileLogger;

void CL_CreateHttpDownloadManager(IGameUI* game_ui,
                                  vgui2::ILocalize* localize,
                                  std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider);
void CL_DeleteHttpDownloadManager();
HttpDownloadManagerInterface* CL_GetHttpDownloadManager();
bool CL_AddDownloadFileLogger(DownloadFileLoggerInterface* logger);
bool CL_RemoveDonwloadFileLogger(DownloadFileLoggerInterface* logger);

void CL_HTTPSetDownloadUrl(const std::string& url);
int CL_HttpGetDownloadQueueSize();
void CL_QueueHTTPDownload(const ResourceDescriptor& file_resource);
void CL_HTTPUpdate();
void CL_HTTPCancel_f();
void CL_HTTPStop_f();
void CL_MarkMapAsUsingHTTPDownload();
