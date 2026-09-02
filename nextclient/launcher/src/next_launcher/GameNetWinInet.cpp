#include "GameNetWinInet.h"

#include <wininet.h>

#include <array>

namespace
{
class InternetHandle
{
public:
    explicit InternetHandle(HINTERNET value = nullptr) : value_(value) { }
    ~InternetHandle() { if (value_) InternetCloseHandle(value_); }

    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    [[nodiscard]] HINTERNET get() const { return value_; }

private:
    HINTERNET value_{};
};

void SetTimeout(HINTERNET session, DWORD option, DWORD milliseconds)
{
    InternetSetOptionW(session, option, &milliseconds, sizeof(milliseconds));
}
}

bool DownloadWithWinInet(
    const wchar_t* url,
    size_t maximum_size,
    std::string& response,
    SYSTEMTIME* server_time)
{
    response.clear();
    if (!url || maximum_size == 0)
        return false;

    InternetHandle session(InternetOpenW(
        L"Allclient-Access/3.0", INTERNET_OPEN_TYPE_PRECONFIG,
        nullptr, nullptr, 0));
    if (!session.get())
        return false;

    SetTimeout(session.get(), INTERNET_OPTION_CONNECT_TIMEOUT, 7000);
    SetTimeout(session.get(), INTERNET_OPTION_SEND_TIMEOUT, 7000);
    SetTimeout(session.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, 12000);

    constexpr wchar_t headers[] =
        L"Accept: text/plain, application/json\r\n"
        L"Cache-Control: no-cache\r\n"
        L"Pragma: no-cache\r\n";
    DWORD flags = INTERNET_FLAG_RELOAD |
        INTERNET_FLAG_NO_CACHE_WRITE |
        INTERNET_FLAG_NO_COOKIES |
        INTERNET_FLAG_NO_UI;
    if (_wcsnicmp(url, L"https://", 8) == 0)
        flags |= INTERNET_FLAG_SECURE;
    else if (_wcsnicmp(url, L"http://", 7) != 0)
        return false;
    InternetHandle request(InternetOpenUrlW(
        session.get(), url, headers, static_cast<DWORD>(-1L), flags, 0));
    if (!request.get())
        return false;

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!HttpQueryInfoW(request.get(),
            HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
            &status_code, &status_size, nullptr) || status_code != HTTP_STATUS_OK)
    {
        return false;
    }

    if (server_time)
    {
        DWORD time_size = sizeof(*server_time);
        if (!HttpQueryInfoW(request.get(), HTTP_QUERY_DATE | HTTP_QUERY_FLAG_SYSTEMTIME,
                server_time, &time_size, nullptr))
        {
            return false;
        }
    }

    std::array<char, 4096> buffer{};
    for (;;)
    {
        DWORD bytes_read = 0;
        if (!InternetReadFile(request.get(), buffer.data(),
                static_cast<DWORD>(buffer.size()), &bytes_read))
        {
            response.clear();
            return false;
        }
        if (bytes_read == 0)
            return !response.empty();
        if (response.size() + bytes_read > maximum_size)
        {
            response.clear();
            return false;
        }
        response.append(buffer.data(), bytes_read);
    }
}
