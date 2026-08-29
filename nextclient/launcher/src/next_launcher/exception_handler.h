#pragma once

void ExceptionHandler(void* exception_pointers);

// Retained for command-line compatibility. Disk crash dumps are disabled.
extern bool g_SaveFullDumps;
