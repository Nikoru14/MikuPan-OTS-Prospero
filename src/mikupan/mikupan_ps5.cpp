#include "mikupan/mikupan_ps5.h"

#include "mikupan/debug/mikupan_logging.h"
#include "mikupan/debug/mikupan_logging_c.h"

#include "platform.h"

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
extern "C" int sceSystemServiceLoadExec(const char *path, const char *const *argv);

/* Before main (and before SDL starts): standard error into klog and the
 * shell's splash screen dismissed, which otherwise covers every frame
 * (ps5/platform/platform.c). */
__attribute__((constructor)) static void MikuPan_Ps5Startup()
{
    platform_init(MIKUPAN_PS5_TITLE_NAME);
}

/* Called by the title's entry point (ps5/native/app_crt.cpp) when main
 * returns. A title gets SIGSYS when it calls exit(), which shows a crash
 * dialog, so ask the shell to close it instead, as PS5_Vulkan's titles do.
 * The shell closes it asynchronously: wait, and never return. */
extern "C" [[noreturn]] void catchReturnFromMain(int status)
{
    if (status != 0)
    {
        MikuPan_Ps5Notify("MikuPan has stopped (status %d). See "
                          "/data/MikuPan/mikupan.log",
                          status);
    }
    MikuPan_FlushLog();
    std::fflush(nullptr);
    (void)sceSystemServiceLoadExec("exit", nullptr);
    for (;;)
    {
        sceKernelUsleep(100000);
    }
}
