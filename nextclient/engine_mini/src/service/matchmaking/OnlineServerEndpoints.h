#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace service::matchmaking
{
// A managed query endpoint can advertise a different game port on the same IP.
// Keep that association across browser closure, but revoke it with its pin.
class OnlineServerEndpoints
{
public:
    static uint64_t Key(uint32_t ip, uint16_t port)
    {
        return (static_cast<uint64_t>(ip) << 16) | port;
    }

    void UpdatePins(const std::unordered_set<uint64_t>& pins)
    {
        pins_ = pins;
        std::erase_if(connection_ports_, [this](const auto& entry) {
            return !pins_.contains(entry.first);
        });
    }

    void Observe(uint32_t ip, uint16_t query_port, uint16_t connection_port)
    {
        const auto query = Key(ip, query_port);
        if (connection_port && pins_.contains(query))
            connection_ports_[query] = Key(ip, connection_port);
    }

    bool Contains(uint32_t ip, uint16_t port) const
    {
        const auto endpoint = Key(ip, port);
        if (pins_.contains(endpoint))
            return true;
        for (const auto& [query, connection] : connection_ports_)
            if (connection == endpoint)
                return true;
        return false;
    }

private:
    std::unordered_set<uint64_t> pins_;
    std::unordered_map<uint64_t, uint64_t> connection_ports_;
};
}
