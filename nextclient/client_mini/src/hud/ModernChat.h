#pragma once

#include <string>
#include <nitroapi/NitroApiInterface.h>

enum class ModernChatMode
{
    Closed,
    SayAll,
    SayTeam
};

class ModernChat
{
public:
    static ModernChat& Instance();

    void Init(nitroapi::NitroApiInterface* nitro_api);
    void VidInit();
    void Reset();

    void Open(ModernChatMode mode);
    void Close();
    void Send();
    void Cancel();
    void ToggleMode();

    [[nodiscard]] bool IsOpen() const { return m_mode != ModernChatMode::Closed; }
    [[nodiscard]] ModernChatMode GetMode() const { return m_mode; }

    int HandleKey(int down, int keynum, const char* pszCurrentBinding);
    void Draw(int scrW, int scrH);

private:
    ModernChat() = default;

    ModernChatMode m_mode = ModernChatMode::Closed;
    std::string m_buffer;
    double m_openTime = 0.0;

    int m_barX = 0;
    int m_barY = 0;
    int m_barW = 0;
    int m_barH = 0;

    int m_sendBtnX = 0;
    int m_sendBtnY = 0;
    int m_sendBtnW = 0;
    int m_sendBtnH = 0;

    int m_cancelBtnX = 0;
    int m_cancelBtnY = 0;
    int m_cancelBtnW = 0;
    int m_cancelBtnH = 0;
};
