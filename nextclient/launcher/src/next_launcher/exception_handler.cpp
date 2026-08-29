#include <Windows.h>

#include "exception_handler.h"

bool g_SaveFullDumps;

void ExceptionHandler(void* exception_pointers)
{
    EXCEPTION_POINTERS* ep = (EXCEPTION_POINTERS*)exception_pointers;
    if (ep == nullptr)
    {
        return;
    }

    // Capture once per process. An outer engine/Steam filter can swallow a fatal fault and
    // resume on a deterministically failing operation, re-entering this handler every iteration;
    // the faulting address is unreliable for dedup (may be null or vary), so guard with a flag.
    static LONG handled = 0;
    if (InterlockedExchange(&handled, 1) != 0)
    {
        return;
    }

    // Do not invoke a disk-backed crash reporter here. Terminate cleanly so neither the
    // launcher nor Steam/Crashpad can create a dump beside the game executable.
    TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : EXCEPTION_NONCONTINUABLE_EXCEPTION);
}
