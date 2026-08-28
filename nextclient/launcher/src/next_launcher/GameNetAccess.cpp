#include "GameNetAccess.h"

#include "GameNetAccessConfig.h"

#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
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

GameNetAccessStatus ResponseAccessStatus(const std::string& response, const CalendarDate& today)
{
    GameNetAccessStatus status{GameNetAccessState::TagMissing, kGameNetTag, {}};
    size_t begin = 0;
    while (begin <= response.size())
    {
        const size_t end = response.find('\n', begin);
        const auto line = Trim(std::string_view(response).substr(
            begin, end == std::string::npos ? response.size() - begin : end - begin));
        if (!line.empty() && line.front() != '#')
        {
            const size_t separator = line.find('|');
            if (separator != std::string_view::npos && EqualsTag(Trim(line.substr(0, separator))))
            {
                status.expiry_date = std::string(Trim(line.substr(separator + 1)));
                CalendarDate expiry;
                if (ParseJalaliDate(line.substr(separator + 1), expiry) &&
                    IsValidJalaliDate(expiry))
                {
                    if (DateKey(today) <= DateKey(expiry))
                        return {GameNetAccessState::Active, kGameNetTag, status.expiry_date};
                    status.state = GameNetAccessState::Expired;
                }
                else
                    status.state = GameNetAccessState::InvalidEntry;
            }
        }
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return status;
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
}

GameNetAccessStatus QueryGameNetOnlineAccess()
{
    const auto unavailable = []
    {
        return GameNetAccessStatus{GameNetAccessState::ServiceUnavailable, kGameNetTag, {}};
    };

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(kGameNetAccessUrl, 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        return unavailable();

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    WinHttpHandle session(WinHttpOpen(
        L"Allclient-Access/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.get())
        return unavailable();

    WinHttpSetTimeouts(session.get(), 3000, 3000, 5000, 5000);
    WinHttpHandle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection.get())
        return unavailable();

    WinHttpHandle request(WinHttpOpenRequest(
        connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.get() ||
        !WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
    {
        return unavailable();
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX) ||
        status_code != HTTP_STATUS_OK)
    {
        return unavailable();
    }

    CalendarDate today;
    if (!GetIranDateFromResponse(request.get(), today))
        return unavailable();

    std::string response;
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available))
            return unavailable();
        if (available == 0)
            break;
        if (response.size() + available > 64 * 1024)
            return unavailable();

        const size_t offset = response.size();
        response.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), response.data() + offset, available, &read))
            return unavailable();
        response.resize(offset + read);
    }

    return ResponseAccessStatus(response, today);
}
