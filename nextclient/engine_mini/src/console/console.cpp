#include "console.h"

#include <common.h>

#include "../common/filesystem.h"
#include "../common/sys_dll.h"

bool con_debuglog;
bool con_initialized;

void Con_DebugLog(const char* file, const char* format, ...)
{
    // Production clients never create diagnostic log files. Keeping the
    // symbol as a no-op preserves ABI/call sites without formatting or I/O.
    (void)file;
    (void)format;
}

void Con_Init()
{
    // Ignore -condebug: accidental command-line flags must not enable disk
    // logging in a latency-focused production build.
    con_debuglog = false;

    con_initialized = true;
}

void Con_DPrintf(ConLogType type, const char* format, ...)
{
    if (developer->value == 0.0 ||
        *p_scr_con_current != 0.0 && cls->state == ca_active)
        return;

    char text[4096];

    va_list params;
    va_start(params, format);
    V_vsnprintf(text, sizeof(text), format, params);
    va_end(params);

    char prefix[16]{};
    if (type == ConLogType::Error)
    {
        V_strcpy_safe(prefix, "Error: ");
    }
    else if (type == ConLogType::Warning)
    {
        V_strcpy_safe(prefix, "Warning: ");
    }

    Sys_Printf("%s%s", prefix, text);

    if (type == ConLogType::Info)
    {
        g_GameConsole->DPrintf("%s%s", prefix, text);
    }
    else
    {
        const Color& color = g_LogTypeColors[(int)type];
        g_GameConsoleNext->ColorPrintf(color.r(), color.g(), color.b(), "%s%s", prefix, text);
    }
}
