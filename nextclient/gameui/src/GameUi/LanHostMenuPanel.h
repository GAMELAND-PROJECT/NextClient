#pragma once

#include <vgui_controls/Panel.h>
#include <Windows.h>
#include <string>
#include <vector>

class CGameConsoleDialog;

class CLanHostMenuPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CLanHostMenuPanel, vgui2::Panel);

public:
    enum ButtonId
    {
        BTN_NONE = 0,
        BTN_TAB_CONSOLE,
        BTN_MODE_WARMUP,
        BTN_MODE_MATCH,
        BTN_MODE_1V1,
        BTN_RESTART,
        BTN_TOGGLE_FF,
        BTN_FREEZE_MINUS,
        BTN_FREEZE_PLUS,
        BTN_FREEZE_0,
        BTN_FREEZE_5,
        BTN_FREEZE_8,
        BTN_FREEZE_12,
        BTN_ROUND_100,
        BTN_ROUND_145,
        BTN_ROUND_200,
        BTN_ROUND_500,
        BTN_MONEY_800,
        BTN_MONEY_5000,
        BTN_MONEY_16K,
    };

    CLanHostMenuPanel(vgui2::Panel* parent, CGameConsoleDialog* pConsoleDialog);
    ~CLanHostMenuPanel() override;

    void Paint() override;
    void OnCursorMoved(int x, int y) override;
    void OnMousePressed(vgui2::MouseCode code) override;
    void OnMouseReleased(vgui2::MouseCode code) override;
    void OnCursorEntered() override;
    void OnCursorExited() override;
    void OnThink() override;

    void SyncCvarsFromEngine();
    void SetMode(const char* modeName);
    void ToggleFriendlyFire();
    void SetFreezeTime(int seconds);
    void AdjustFreezeTime(int delta);
    void SetRoundTime(float minutes);
    void SetStartMoney(int money);

private:
    struct ButtonHitbox
    {
        ButtonId id;
        RECT rect;
        std::string command;
        std::wstring title;
        std::wstring subtitle;
        bool isToggle;
        bool isActive;
        COLORREF accentColor;
    };

    CGameConsoleDialog* m_pConsoleDialog;
    int m_texture = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_needsRedraw = true;

    ButtonId m_hoveredId = BTN_NONE;
    ButtonId m_pressedId = BTN_NONE;

    // Server State
    bool m_bFriendlyFire = true;
    int m_iFreezeTime = 12;
    float m_fRoundTime = 1.75f;
    int m_iStartMoney = 800;
    std::string m_currentMode = "mix";

    std::vector<ButtonHitbox> m_hitboxes;

    void BuildHitboxes(int width, int height);
    ButtonHitbox* FindHitbox(int x, int y);
    void ExecuteAction(ButtonId id);
    std::vector<unsigned char> RenderHostMenuBitmap(int width, int height);
};
