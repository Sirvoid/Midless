#include "platform.h"
#if defined(OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include <mmsystem.h>
static int preciseTiming;
#else
#include <unistd.h>
#endif

int Platform_ProcessorCount(void) {
#if defined(OS_WINDOWS)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? info.dwNumberOfProcessors : 1;
#elif defined(PLATFORM_WEB)
    return 2;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (int)count : 1;
#endif
}

void Platform_BeginTiming(void) {
#if defined(OS_WINDOWS)
    if (!preciseTiming) preciseTiming = timeBeginPeriod(1) == TIMERR_NOERROR;
#endif
}

void Platform_EndTiming(void) {
#if defined(OS_WINDOWS)
    if (preciseTiming) timeEndPeriod(1);
    preciseTiming = 0;
#endif
}
