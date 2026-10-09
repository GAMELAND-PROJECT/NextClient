#pragma once

#include <string>

enum class GameNetAccessState
{
    Active,
    Expired,
    TagMissing,
    InvalidEntry,
    ServiceUnavailable,
};

struct GameNetAccessStatus
{
    GameNetAccessState state = GameNetAccessState::ServiceUnavailable;
    std::string tag;
    std::string player_name_tag;
    std::string expiry_date;
    int days_remaining = -1;
    bool lan_allowed = false;
    int offline_days_remaining = 0;
    bool is_home_client = false;
    std::string device_hash;
    std::string phone_number;

    [[nodiscard]] bool allowed() const { return state == GameNetAccessState::Active; }
};

// Checks access at startup. LAN requires verification within the configured grace period.
GameNetAccessStatus QueryGameNetOnlineAccess();

// Helper to compute 24-character hardware hash matching Inno Setup
std::string Compute24CharDeviceHash();

struct CalendarDate
{
    int year{};
    int month{};
    int day{};
};

// Resilient DNS resolution falling back to public DNS (8.8.8.8, 1.1.1.1) and hardcoded IPs
std::string ResolveHostResilient(const std::string& host);

// Direct socket HTTP GET bypassing system DNS and WinINet
bool DownloadWithDirectSocket(
    const std::string& host,
    uint16_t port,
    const std::string& path,
    const std::string& virtual_host,
    size_t maximum_size,
    std::string& response,
    CalendarDate* server_date = nullptr);

// Direct socket HTTP POST bypassing system DNS and WinINet
bool PostWithDirectSocket(
    const std::string& host,
    uint16_t port,
    const std::string& path,
    const std::string& virtual_host,
    const std::string& body,
    std::string& response);

