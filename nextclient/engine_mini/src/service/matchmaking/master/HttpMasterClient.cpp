#include "HttpMasterClient.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <optick.h>
#include <utility>

#include <nitro_utils/net_utils.h>
#include <nitro_utils/string_utils.h>

using namespace cpr;
using namespace taskcoro;
using namespace concurrencpp;

HttpMasterClient::HttpMasterClient(NextClientVersion client_version, std::string url, bool plain_text) :
    url_(std::move(url)),
    plain_text_(plain_text)
{
    if (!plain_text_)
        headers_.emplace("Content-type", "application/json");
    headers_.emplace("BuildVersion", BuildNextClientVersionString(client_version));
    // headers_["UID"] = user_info_->GetClientUid();
    // headers_["Branch"] = user_info_->GetUpdateBranch();
    // headers_["LaunchGameCount"] = std::to_string(user_info_->GetLaunchGameCount());
}

result<std::vector<netadr_t>> HttpMasterClient::GetServerAddressesAsync(
    std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    Response response = co_await TaskCoro::RunIO([this, cancellation_token]
    {
        Header header;
        for (auto& [key, value] : headers_)
        {
            header.emplace(key, value);
        }

        Session session;
        session.SetUrl(url_);
        session.SetHeader(header);
        if (!plain_text_)
            session.SetBody(Body("{\"method\": \"server_list\", \"data\": \"null\"}"));
        session.SetConnectTimeout(kConnectTimeout);
        session.SetTimeout(kTimeout);
        session.SetProgressCallback(ProgressCallback([cancellation_token](cpr_pf_arg_t, cpr_pf_arg_t, cpr_pf_arg_t, cpr_pf_arg_t, intptr_t)
        {
            if (cancellation_token != nullptr && cancellation_token->IsCanceled())
            {
                return false;
            }

            return true;
        }));

        return session.Get();
    });

    cancellation_token->ThrowIfCancelled();

    if (response.error.code != cpr::ErrorCode::OK || response.status_code != 200)
        co_return std::vector<netadr_t>{};

    std::vector<netadr_t> addresses = ParseResponse(response.text);

    if (address_received_callback)
    {
        for (const netadr_t& address : addresses)
        {
            address_received_callback(address);
        }
    }

    co_return addresses;
}

std::vector<netadr_t> HttpMasterClient::ParseResponse(const std::string& data)
{
    OPTICK_EVENT();

    std::vector<netadr_t> result;

    std::istringstream input(data);
    std::string line;
    while (std::getline(input, line))
    {
        const auto first = std::find_if_not(line.begin(), line.end(), [](unsigned char ch) { return std::isspace(ch); });
        const auto last = std::find_if_not(line.rbegin(), line.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
        if (first >= last || *first == '#')
            continue;

        const std::string address_text(first, last);
        netadr_t address;
        address.SetFromString(address_text.c_str(), false);
        if (!address.IsValid())
            continue;

        const bool duplicate = std::any_of(result.begin(), result.end(), [&address](const netadr_t& existing)
        {
            return existing.CompareAdr(address);
        });
        if (!duplicate)
            result.push_back(address);
    }

    return result;
}
