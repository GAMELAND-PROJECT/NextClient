#pragma once

#include <string>
#include <vector>
#include <deque>
#include <nitroapi/NitroApiInterface.h>

enum class ModernChatMode
{
    Closed,
    SayAll,
    SayTeam,
    CustomCommand
};

struct LiveChatMessage
{
    int clientIndex = 0;
    std::string prefix;
    std::string sender;
    std::string text;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    double timestamp = 0.0;
    bool isTeam = false;
    bool isServer = false;
    bool isCommand = false;
};

class ModernChat
{
public:
    static ModernChat& Instance();

    void Init(nitroapi::NitroApiInterface* nitro_api);
    void VidInit();
    void Reset();

    void Open(ModernChatMode mode);
    void OpenCustom(const std::string& customCommand);
    void Close();
    void Send();
    void Cancel();
    void ToggleMode();
    [[nodiscard]] const std::string& GetCustomCommand() const { return m_customCommand; }

    void AddChatMessage(int clientIndex, const std::string& prefix, const std::string& sender, const std::string& text, float r, float g, float b, bool isTeam = false, bool isServer = false, bool isCommand = false);
    bool OnSayTextPacket(int clientIndex, const std::vector<std::string>& strings);
    bool OnTextMsgPacket(const std::vector<std::string>& strings);
    void OnSayText(int clientIndex, const std::string& str1, const std::string& str2, const std::string& str3, const std::string& str4 = "");
    void OnTextMsg(const std::string& formattedMsg);
    void OnLocalPlayerSend(ModernChatMode mode, const std::string& message);

    [[nodiscard]] bool IsOpen() const { return m_mode != ModernChatMode::Closed; }
    [[nodiscard]] ModernChatMode GetMode() const { return m_mode; }

    int HandleKey(int down, int keynum, const char* pszCurrentBinding);
    void Draw(int scrW, int scrH);
    void ScrollHistory(int delta);
    [[nodiscard]] int GetMaxVisibleMessages() const;

private:
    ModernChat() = default;

    ModernChatMode m_mode = ModernChatMode::Closed;
    std::string m_customCommand;
    std::string m_buffer;
    double m_openTime = 0.0;
    int m_scrollOffset = 0;

    std::deque<LiveChatMessage> m_messages;

    std::string m_lastLocalSentText;
    double m_lastLocalSentTime = 0.0;

    int m_barX = 0;
    int m_barY = 0;
    int m_barW = 0;
    int m_barH = 0;
};
