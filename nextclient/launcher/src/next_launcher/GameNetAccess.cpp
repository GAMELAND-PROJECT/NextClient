#include "GameNetAccess.h"

#include "GameNetAccessConfig.h"

#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace
{
class WinHttpHandle
{
public:
    explicit WinHttpHandle(HINTERNET value = nullptr) : value_(value) { }
    ~WinHttpHandle() { if (value_) WinHttpCloseHandle(value_); }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    [[nodiscard]] HINTERNET get() const { return value_; }

private:
    HINTERNET value_;
};

std::string_view Trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return value;
}

bool EqualsTag(std::string_view value)
{
    const std::string_view expected(kGameNetTag);
    return value.size() == expected.size() &&
        std::equal(value.begin(), value.end(), expected.begin(), [](unsigned char left, unsigned char right)
        {
            return std::tolower(left) == std::tolower(right);
        });
}

bool ResponseContainsActiveTag(const std::string& response)
{
    size_t begin = 0;
    while (begin <= response.size())
    {
        const size_t end = response.find('\n', begin);
        const auto line = Trim(std::string_view(response).substr(
            begin, end == std::string::npos ? response.size() - begin : end - begin));
        if (!line.empty() && line.front() != '#' && EqualsTag(line))
            return true;
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return false;
}
}

bool IsGameNetOnlineAccessAllowed()
{
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(kGameNetAccessUrl, 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        return false;

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    WinHttpHandle session(WinHttpOpen(
        L"Allclient-Access/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.get())
        return false;

    WinHttpSetTimeouts(session.get(), 3000, 3000, 5000, 5000);
    WinHttpHandle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection.get())
        return false;

    WinHttpHandle request(WinHttpOpenRequest(
        connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.get() ||
        !WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
    {
        return false;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX) ||
        status_code != HTTP_STATUS_OK)
    {
        return false;
    }

    std::string response;
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available))
            return false;
        if (available == 0)
            break;
        if (response.size() + available > 64 * 1024)
            return false;

        const size_t offset = response.size();
        response.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), response.data() + offset, available, &read))
            return false;
        response.resize(offset + read);
    }

    return ResponseContainsActiveTag(response);
}

