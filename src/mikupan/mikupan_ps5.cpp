#include "mikupan/mikupan_ps5.h"

#include "mikupan/debug/mikupan_logging.h"

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
