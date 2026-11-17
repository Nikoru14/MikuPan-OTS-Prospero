#include "mikupan/mikupan_ps5.h"

#include "mikupan/debug/mikupan_logging.h"
#include "mikupan/debug/mikupan_logging_c.h"

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace
{
/* Layout used by the ps5-payload-sdk's notification samples. */
struct NotifyRequest
{
    char reserved[45];
    char message[3075];
};
} // namespace

extern "C" int sceKernelSendNotificationRequest(int device, NotifyRequest *request,
                                                size_t size, int blocking);

extern "C" void MikuPan_Ps5Notify(const char *fmt, ...)
{
    NotifyRequest request;
    std::memset(&request, 0, sizeof(request));

    va_list args;
    va_start(args, fmt);
    std::vsnprintf(request.message, sizeof(request.message), fmt, args);
    va_end(args);

    info_log("Notification: %s", request.message);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

extern "C" int sceKernelUsleep(unsigned int microseconds);

/* Called by the title's entry point (ps5/native/app_crt.cpp) when main
 * returns. A title started from the home screen gets SIGSYS when it calls
 * exit(), which shows a crash dialog, so stay idle instead; the player
 * closes the title from the PS button menu, as with the OpenLF2 port. */
extern "C" [[noreturn]] void catchReturnFromMain(int status)
{
    MikuPan_Ps5Notify("MikuPan has stopped (status %d). Close it from the PS "
                      "button menu.",
                      status);
    MikuPan_FlushLog();
    for (;;)
    {
        sceKernelUsleep(1000000);
    }
}
