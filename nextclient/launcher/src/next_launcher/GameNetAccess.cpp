#include "GameNetAccess.h"

#include "GameNetAccessConfig.h"
#include "GameNetWinInet.h"

#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

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

class GlobalMemory
{
public:
    ~GlobalMemory() { reset(); }
    GlobalMemory(const GlobalMemory&) = delete;
    GlobalMemory& operator=(const GlobalMemory&) = delete;
    GlobalMemory() = default;

    void reset(void* value = nullptr)
    {
        if (value_)
            GlobalFree(value_);
        value_ = value;
    }

private:
    void* value_{};
};

enum class ProxyMode
{
    CurrentUser,
    WinHttpDefault,
    Direct,
};

constexpr DWORD kTls12Only = 0x00000800; // WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
constexpr wchar_t kRequestHeaders[] =
    L"Accept: text/plain, application/json\r\n"
    L"Cache-Control: no-cache\r\n"
    L"Pragma: no-cache\r\n"
    L"Connection: close\r\n";

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

struct CalendarDate
{
    int year{};
    int month{};
    int day{};
};

bool ParseNumber(std::string_view value, int& number)
{
    if (value.empty())
        return false;

    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

bool ParseJalaliDate(std::string_view value, CalendarDate& date)
{
    value = Trim(value);
    const size_t first_slash = value.find('/');
    const size_t second_slash = first_slash == std::string_view::npos
        ? std::string_view::npos : value.find('/', first_slash + 1);
    if (first_slash == std::string_view::npos || second_slash == std::string_view::npos ||
        value.find('/', second_slash + 1) != std::string_view::npos)
    {
        return false;
    }

    return ParseNumber(value.substr(0, first_slash), date.year) &&
        ParseNumber(value.substr(first_slash + 1, second_slash - first_slash - 1), date.month) &&
        ParseNumber(value.substr(second_slash + 1), date.day) &&
        date.year >= 1200 && date.year <= 1700 && date.month >= 1 && date.month <= 12 &&
        date.day >= 1 && date.day <= 31;
}

CalendarDate GregorianToJalali(int gy, int gm, int gd)
{
    static constexpr std::array<int, 12> kGregorianMonthDays{
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    const int gy2 = gm > 2 ? gy + 1 : gy;
    int days = 355666 + (365 * gy) + ((gy2 + 3) / 4) - ((gy2 + 99) / 100) +
        ((gy2 + 399) / 400) + gd + kGregorianMonthDays[static_cast<size_t>(gm - 1)];
    CalendarDate result;
    result.year = -1595 + (33 * (days / 12053));
    days %= 12053;
    result.year += 4 * (days / 1461);
    days %= 1461;
    if (days > 365)
    {
        result.year += (days - 1) / 365;
        days = (days - 1) % 365;
    }
    if (days < 186)
    {
        result.month = 1 + (days / 31);
        result.day = 1 + (days % 31);
    }
    else
    {
        result.month = 7 + ((days - 186) / 30);
        result.day = 1 + ((days - 186) % 30);
    }
    return result;
}

CalendarDate JalaliToGregorian(int jy, int jm, int jd)
{
    jy += 1595;
    int days = -355668 + (365 * jy) + ((jy / 33) * 8) + (((jy % 33) + 3) / 4) + jd;
    days += jm < 7 ? (jm - 1) * 31 : ((jm - 7) * 30) + 186;

    CalendarDate result;
    result.year = 400 * (days / 146097);
    days %= 146097;
    if (days > 36524)
    {
        result.year += 100 * (--days / 36524);
        days %= 36524;
        if (days >= 365)
            ++days;
    }
    result.year += 4 * (days / 1461);
    days %= 1461;
    if (days > 365)
    {
        result.year += (days - 1) / 365;
        days = (days - 1) % 365;
    }
    static constexpr std::array<int, 12> kGregorianMonthLengths{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    auto month_lengths = kGregorianMonthLengths;
    if ((result.year % 4 == 0 && result.year % 100 != 0) || result.year % 400 == 0)
        month_lengths[1] = 29;

    result.month = 1;
    while (result.month <= 12 && days >= month_lengths[static_cast<size_t>(result.month - 1)])
    {
        days -= month_lengths[static_cast<size_t>(result.month - 1)];
        ++result.month;
    }
    result.day = days + 1;
    return result;
}

bool IsValidJalaliDate(const CalendarDate& date)
{
    if (date.year < 1200 || date.year > 1700 || date.month < 1 || date.month > 12 || date.day < 1)
        return false;
    if ((date.month <= 6 && date.day > 31) || (date.month >= 7 && date.month <= 11 && date.day > 30) ||
        (date.month == 12 && date.day > 30))
        return false;

    const auto gregorian = JalaliToGregorian(date.year, date.month, date.day);
    const auto round_trip = GregorianToJalali(gregorian.year, gregorian.month, gregorian.day);
    return round_trip.year == date.year && round_trip.month == date.month && round_trip.day == date.day;
}

int DateKey(const CalendarDate& date)
{
    return (date.year * 10000) + (date.month * 100) + date.day;
}

int64_t GregorianDayNumber(CalendarDate date)
{
    date.year -= date.month <= 2;
    const int era = date.year >= 0 ? date.year / 400 : (date.year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(date.year - era * 400);
    const unsigned month_position = static_cast<unsigned>(
        date.month + (date.month > 2 ? -3 : 9));
    const unsigned day_of_year = (153 * month_position + 2) / 5 +
        static_cast<unsigned>(date.day - 1);
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 -
        year_of_era / 100 + day_of_year;
    return static_cast<int64_t>(era) * 146097 + day_of_era;
}

int DaysRemaining(const CalendarDate& today, const CalendarDate& expiry)
{
    const auto today_gregorian = JalaliToGregorian(today.year, today.month, today.day);
    const auto expiry_gregorian = JalaliToGregorian(expiry.year, expiry.month, expiry.day);
    return static_cast<int>(GregorianDayNumber(expiry_gregorian) -
        GregorianDayNumber(today_gregorian));
}

bool IsValidPlayerNameTag(std::string_view value)
{
    value = Trim(value);
    return !value.empty() && value.size() <= 12 &&
        std::ranges::all_of(value, [](unsigned char character)
        {
            return std::isalnum(character) || character == '_' || character == '-';
        });
}

GameNetAccessStatus ResponseAccessStatus(const std::string& response, const CalendarDate& today)
{
    GameNetAccessStatus status{GameNetAccessState::TagMissing, kGameNetTag, {}, {}, -1};
    size_t begin = 0;
    while (begin <= response.size())
    {
        const size_t end = response.find('\n', begin);
        const auto line = Trim(std::string_view(response).substr(
            begin, end == std::string::npos ? response.size() - begin : end - begin));
        if (!line.empty() && line.front() != '#')
        {
            const size_t first_separator = line.find('|');
            if (first_separator != std::string_view::npos &&
                EqualsTag(Trim(line.substr(0, first_separator))))
            {
                const size_t second_separator = line.find('|', first_separator + 1);
                if (second_separator != std::string_view::npos &&
                    line.find('|', second_separator + 1) != std::string_view::npos)
                {
                    status.state = GameNetAccessState::InvalidEntry;
                }
                else
                {
                    // Accept the previous "build tag | expiry" format during
                    // migration. It enables Online without a player-name tag.
                    const auto player_name_tag = second_separator == std::string_view::npos
                        ? std::string_view{} : Trim(line.substr(
                            first_separator + 1, second_separator - first_separator - 1));
                    const auto expiry_text = second_separator == std::string_view::npos
                        ? Trim(line.substr(first_separator + 1))
                        : Trim(line.substr(second_separator + 1));
                    status.player_name_tag = std::string(player_name_tag);
                    status.expiry_date = std::string(expiry_text);
                    CalendarDate expiry;
                    if ((player_name_tag.empty() || IsValidPlayerNameTag(player_name_tag)) &&
                        ParseJalaliDate(expiry_text, expiry) && IsValidJalaliDate(expiry))
                    {
                        status.days_remaining = DaysRemaining(today, expiry);
                        if (DateKey(today) <= DateKey(expiry))
                            return {GameNetAccessState::Active, kGameNetTag, status.player_name_tag,
                                status.expiry_date, status.days_remaining};
                        status.state = GameNetAccessState::Expired;
                    }
                    else
                        status.state = GameNetAccessState::InvalidEntry;
                }
            }
        }
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return status;
}

bool GetIranDateFromUtcTime(const SYSTEMTIME& utc_time, CalendarDate& date)
{
    FILETIME file_time{};
    if (!SystemTimeToFileTime(&utc_time, &file_time))
        return false;

    ULARGE_INTEGER iran_time{};
    iran_time.LowPart = file_time.dwLowDateTime;
    iran_time.HighPart = file_time.dwHighDateTime;
    iran_time.QuadPart += 126000000000ULL; // UTC +03:30, Iran standard time.
    file_time.dwLowDateTime = iran_time.LowPart;
    file_time.dwHighDateTime = iran_time.HighPart;

    SYSTEMTIME local_time{};
    if (!FileTimeToSystemTime(&file_time, &local_time))
        return false;
    date = GregorianToJalali(local_time.wYear, local_time.wMonth, local_time.wDay);
    return true;
}

bool GetIranDateFromResponse(HINTERNET request, CalendarDate& date)
{
    SYSTEMTIME utc_time{};
    DWORD size = sizeof(utc_time);
    if (!WinHttpQueryHeaders(request,
            WINHTTP_QUERY_DATE | WINHTTP_QUERY_FLAG_SYSTEMTIME,
            WINHTTP_HEADER_NAME_BY_INDEX, &utc_time, &size, WINHTTP_NO_HEADER_INDEX))
    {
        return false;
    }
    return GetIranDateFromUtcTime(utc_time, date);
}

bool PerformHttpGet(
    const wchar_t* url,
    size_t maximum_size,
    ProxyMode proxy_mode,
    std::string& response,
    CalendarDate* server_date)
{
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url, 0, 0, &parts) ||
        (parts.nScheme != INTERNET_SCHEME_HTTPS && parts.nScheme != INTERNET_SCHEME_HTTP))
        return false;
    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_proxy{};
    GlobalMemory ie_proxy_name;
    GlobalMemory ie_proxy_bypass;
    GlobalMemory ie_auto_config;
    bool have_ie_proxy = false;
    if (proxy_mode == ProxyMode::CurrentUser && WinHttpGetIEProxyConfigForCurrentUser(&ie_proxy))
    {
        ie_proxy_name.reset(ie_proxy.lpszProxy);
        ie_proxy_bypass.reset(ie_proxy.lpszProxyBypass);
        ie_auto_config.reset(ie_proxy.lpszAutoConfigUrl);
        have_ie_proxy = true;
    }

    DWORD access_type = WINHTTP_ACCESS_TYPE_NO_PROXY;
    const wchar_t* proxy_name = WINHTTP_NO_PROXY_NAME;
    const wchar_t* proxy_bypass = WINHTTP_NO_PROXY_BYPASS;
    if (proxy_mode == ProxyMode::WinHttpDefault)
    {
        access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    }
    else if (have_ie_proxy && ie_proxy.lpszProxy)
    {
        access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        proxy_name = ie_proxy.lpszProxy;
        proxy_bypass = ie_proxy.lpszProxyBypass;
    }

    WinHttpHandle session(WinHttpOpen(
        L"Allclient-Access/2.0", access_type, proxy_name, proxy_bypass, 0));
    if (!session.get())
        return false;
    WinHttpSetTimeouts(session.get(), 5000, 7000, 7000, 12000);

    DWORD connect_retries = 2;
    WinHttpSetOption(session.get(), WINHTTP_OPTION_CONNECT_RETRIES,
        &connect_retries, sizeof(connect_retries));

    // Windows 7 WinHTTP commonly defaults to TLS 1.0 even when browsers use
    // TLS 1.2. Select TLS 1.2 per-process so no machine registry change is
    // required. Failure is allowed to fall through to the other transports.
    DWORD secure_protocols = kTls12Only;
    if (secure && !WinHttpSetOption(session.get(), WINHTTP_OPTION_SECURE_PROTOCOLS,
            &secure_protocols, sizeof(secure_protocols)))
    {
        return false;
    }

    WinHttpHandle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection.get())
        return false;
    WinHttpHandle request(WinHttpOpenRequest(
        connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
    if (!request.get())
        return false;

    WINHTTP_PROXY_INFO auto_proxy{};
    GlobalMemory auto_proxy_name;
    GlobalMemory auto_proxy_bypass;
    if (proxy_mode == ProxyMode::CurrentUser && have_ie_proxy &&
        !ie_proxy.lpszProxy && (ie_proxy.fAutoDetect || ie_proxy.lpszAutoConfigUrl))
    {
        WINHTTP_AUTOPROXY_OPTIONS options{};
        if (ie_proxy.lpszAutoConfigUrl)
        {
            options.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
            options.lpszAutoConfigUrl = ie_proxy.lpszAutoConfigUrl;
        }
        if (ie_proxy.fAutoDetect)
        {
            options.dwFlags |= WINHTTP_AUTOPROXY_AUTO_DETECT;
            options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP |
                WINHTTP_AUTO_DETECT_TYPE_DNS_A;
        }
        options.fAutoLogonIfChallenged = TRUE;
        if (WinHttpGetProxyForUrl(session.get(), url, &options, &auto_proxy))
        {
            auto_proxy_name.reset(auto_proxy.lpszProxy);
            auto_proxy_bypass.reset(auto_proxy.lpszProxyBypass);
            if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_PROXY,
                    &auto_proxy, sizeof(auto_proxy)))
            {
                return false;
            }
        }
    }

    DWORD auto_logon_policy = WINHTTP_AUTOLOGON_SECURITY_LEVEL_LOW;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_AUTOLOGON_POLICY,
        &auto_logon_policy, sizeof(auto_logon_policy));

    if (!WinHttpSendRequest(request.get(), kRequestHeaders,
            static_cast<DWORD>(-1L),
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
    {
        return false;
    }

    if (server_date && !GetIranDateFromResponse(request.get(), *server_date))
        return false;

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX) ||
        status_code != HTTP_STATUS_OK)
    {
        return false;
    }

    response.clear();
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available))
            return false;
        if (available == 0)
            return true;
        if (response.size() + available > maximum_size)
            return false;

        const size_t offset = response.size();
        response.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), response.data() + offset, available, &read))
            return false;
        response.resize(offset + read);
    }
}

bool DownloadText(
    const wchar_t* url,
    size_t maximum_size,
    std::string& response,
    CalendarDate* server_date = nullptr)
{
    // WinINet follows the exact Internet Explorer configuration that is known
    // to work on the target Windows 7 machines. WinHTTP remains a clean
    // fallback for machine proxy and direct-connect environments.
    SYSTEMTIME wininet_time{};
    std::string wininet_response;
    if (DownloadWithWinInet(url, maximum_size, wininet_response,
            server_date ? &wininet_time : nullptr))
    {
        CalendarDate wininet_date{};
        if (!server_date || GetIranDateFromUtcTime(wininet_time, wininet_date))
        {
            response = std::move(wininet_response);
            if (server_date)
                *server_date = wininet_date;
            return true;
        }
    }

    // Try current-user proxy/PAC, machine WinHTTP proxy, then direct. Each
    // attempt owns fresh handles so a failed proxy cannot poison the fallback.
    constexpr std::array modes{
        ProxyMode::CurrentUser,
        ProxyMode::WinHttpDefault,
        ProxyMode::Direct,
    };
    for (const auto mode : modes)
    {
        CalendarDate candidate_date{};
        std::string candidate_response;
        if (PerformHttpGet(url, maximum_size, mode, candidate_response,
                server_date ? &candidate_date : nullptr))
        {
            response = std::move(candidate_response);
            if (server_date)
                *server_date = candidate_date;
            return true;
        }
    }
    response.clear();
    return false;
}

bool DownloadSmallText(const wchar_t* url, size_t maximum_size, std::string& response)
{
    return DownloadText(url, maximum_size, response);
}
}

GameNetAccessStatus QueryGameNetOnlineAccess()
{
    const auto unavailable = []
    {
        return GameNetAccessStatus{
            GameNetAccessState::ServiceUnavailable, kGameNetTag, {}, {}, -1};
    };

    CalendarDate today;
    std::string response;
    if (!DownloadText(kGameNetAccessUrl, 64 * 1024, response, &today))
        return unavailable();

    return ResponseAccessStatus(response, today);
}

std::string QueryGameNetServerPassword()
{
    std::string response;
    if (!DownloadSmallText(kGameNetServerPasswordUrl, 256, response))
        return {};

    const std::string_view password = Trim(response);
    if (password.empty() || password.size() > 31)
        return {};

    // GoldSrc userinfo uses backslash as a separator. Reject control bytes and
    // command-string metacharacters even though the value is set via Cvar_Set.
    if (!std::ranges::all_of(password, [](unsigned char character)
        {
            return character >= 33 && character <= 126 &&
                character != '\\' && character != '"' && character != ';';
        }))
    {
        return {};
    }

    return std::string(password);
}
