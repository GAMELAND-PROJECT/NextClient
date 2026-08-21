#include "GameConsoleNext.h"
#include <strtools.h>
#include <cstdarg>
#include <nitro_utils/string_utils.h>
#include <nitro_utils/config/FileConfigProvider.h>

static CGameConsoleNext g_GameConsoleNext;

CGameConsoleNext &GameConsoleNext()
{
    return g_GameConsoleNext;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameConsole, IGameConsoleNext, GAMECONSOLE_NEXT_INTERFACE_VERSION, g_GameConsoleNext);

void CGameConsoleNext::Initialize(CGameConsoleDialog *console_dialog)
{
    if (initialized_)
        return;

    console_dialog_ = console_dialog;

    initialized_ = true;

    // This provides a 1 frame delay to display the text after the temporary buffer from the engine
    TaskCoro::RunInMainThread([this]
    {
        ExecuteTempConsoleBuffer();
    });
}

void CGameConsoleNext::ColorPrintf(uint8_t, uint8_t, uint8_t, const char *, ...)
{
}

void CGameConsoleNext::ColorPrintfWide(uint8_t, uint8_t, uint8_t, const wchar_t *, ...)
{
}

void CGameConsoleNext::PrintfEx(const char *, ...)
{
}

void CGameConsoleNext::PrintfExWide(const wchar_t *, ...)
{
}

void CGameConsoleNext::ExecuteTempConsoleBuffer()
{
    for (const auto& [color, text] : temp_console_buffer_)
    {
        console_dialog_->ColorPrint(color, text.data(), text.data() + text.size());
    }

    temp_console_buffer_.clear();
    temp_console_buffer_.shrink_to_fit();
}

std::wstring CGameConsoleNext::Utf8ToWstring(const char* str)
{
    int cch = Q_strlen(str);
    int cubDest = (cch + 1) * sizeof(wchar_t);
    wchar_t* pwch = (wchar_t*) stackalloc(cubDest);
    int cwch = Q_UTF8ToWString(str, pwch, cubDest) / sizeof(wchar_t);

    return std::wstring(pwch, pwch + cwch);
}
