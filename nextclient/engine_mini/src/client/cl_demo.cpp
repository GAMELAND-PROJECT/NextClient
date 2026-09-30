#include "engine.h"
#include "common/common.h"
#include "common/cmd.h"
#include <optick.h>

void CL_Stop_f()
{
    OPTICK_EVENT();

    eng()->CL_Stop_f.InvokeChained();

    if (COM_CheckParm("-demorender"))
    {
        Cbuf_InsertText("endmovie\nquit\n");
    }
}
