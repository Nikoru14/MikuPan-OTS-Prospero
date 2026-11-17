#include "mikupan/mikupan_ps5.h"

#include "mikupan/debug/mikupan_logging.h"
#include "mikupan/debug/mikupan_logging_c.h"

#include "platform.h"

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

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
    /* RADV's own messages (Mesa's log) beside MikuPan's, rather than only in
     * the console's klog. */
    static std::string mesa_log;
    mesa_log = std::string(MikuPan_GetPs5UserDirectory()) + "mesa.log";
    setenv("MESA_LOG_FILE", mesa_log.c_str(), 1);
    setenv("MESA_LOG_LEVEL", "debug", 1);
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

/* ------------------------------------------------------------ diagnostics */

#include <SDL3/SDL_log.h>

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance instance, const char *name);

namespace
{
void ForwardSdlLog(void *, int category, SDL_LogPriority priority, const char *message)
{
    static const char *const names[] = {"", "trace", "verbose", "debug", "info",
                                        "warn", "error", "critical"};
    const int index = priority >= 0 && priority < (int)(sizeof(names) / sizeof(names[0]))
                          ? (int)priority
                          : 0;
    info_log("SDL [%d %s] %s", category, names[index], message);
}
} // namespace

extern "C" void MikuPan_Ps5ForwardSdlLog(void)
{
    SDL_SetLogOutputFunction(ForwardSdlLog, nullptr);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_SYSTEM, SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_VERBOSE);
}

extern "C" void MikuPan_Ps5VulkanReport(void)
{
    info_log("Vulkan report: start");
    auto get = [](VkInstance instance, const char *name) {
        return vk_icdGetInstanceProcAddr(instance, name);
    };
#define PS5_VK(instance, name) auto name = reinterpret_cast<PFN_##name>(get(instance, #name))
    PS5_VK(VK_NULL_HANDLE, vkEnumerateInstanceExtensionProperties);
    PS5_VK(VK_NULL_HANDLE, vkEnumerateInstanceVersion);
    PS5_VK(VK_NULL_HANDLE, vkCreateInstance);
    if (!vkEnumerateInstanceExtensionProperties || !vkCreateInstance)
    {
        info_log("Vulkan report: RADV gave no global entry points");
        return;
    }
    uint32_t version = 0;
    if (vkEnumerateInstanceVersion)
    {
        vkEnumerateInstanceVersion(&version);
    }
    info_log("Vulkan report: instance version %u.%u.%u", VK_API_VERSION_MAJOR(version),
             VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));

    uint32_t count = 0;
    VkResult rc = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> instance_extensions(count);
    if (count)
    {
        rc = vkEnumerateInstanceExtensionProperties(nullptr, &count, instance_extensions.data());
    }
    std::string list;
    for (const auto &extension : instance_extensions)
    {
        list += extension.extensionName;
        list += ' ';
    }
    info_log("Vulkan report: %u instance extensions (%d): %s", count, (int)rc, list.c_str());

    const char *enabled[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME};
    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "MikuPan";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = 2;
    info.ppEnabledExtensionNames = enabled;
    VkInstance instance = VK_NULL_HANDLE;
    rc = vkCreateInstance(&info, nullptr, &instance);
    info_log("Vulkan report: vkCreateInstance(surface, display) = %d", (int)rc);
    MikuPan_FlushLog();
    if (rc != VK_SUCCESS)
    {
        return;
    }

    PS5_VK(instance, vkDestroyInstance);
    PS5_VK(instance, vkEnumeratePhysicalDevices);
    PS5_VK(instance, vkGetPhysicalDeviceProperties);
    PS5_VK(instance, vkGetPhysicalDeviceFeatures);
    PS5_VK(instance, vkEnumerateDeviceExtensionProperties);
    PS5_VK(instance, vkGetPhysicalDeviceQueueFamilyProperties);
    PS5_VK(instance, vkGetPhysicalDeviceDisplayPropertiesKHR);
    PS5_VK(instance, vkGetDisplayModePropertiesKHR);

    count = 0;
    rc = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    info_log("Vulkan report: vkEnumeratePhysicalDevices = %d, %u GPUs", (int)rc, count);
    std::vector<VkPhysicalDevice> devices(count);
    if (count)
    {
        rc = vkEnumeratePhysicalDevices(instance, &count, devices.data());
    }
    for (VkPhysicalDevice device : devices)
    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(device, &properties);
        info_log("Vulkan report: GPU '%s' API %u.%u.%u type %d", properties.deviceName,
                 VK_API_VERSION_MAJOR(properties.apiVersion),
                 VK_API_VERSION_MINOR(properties.apiVersion),
                 VK_API_VERSION_PATCH(properties.apiVersion), (int)properties.deviceType);

        VkPhysicalDeviceFeatures features = {};
        vkGetPhysicalDeviceFeatures(device, &features);
        info_log("Vulkan report: features independentBlend %u imageCubeArray %u depthClamp %u "
                 "shaderClipDistance %u drawIndirectFirstInstance %u sampleRateShading %u "
                 "samplerAnisotropy %u",
                 features.independentBlend, features.imageCubeArray, features.depthClamp,
                 features.shaderClipDistance, features.drawIndirectFirstInstance,
                 features.sampleRateShading, features.samplerAnisotropy);

        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> device_extensions(extension_count);
        if (extension_count)
        {
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                                 device_extensions.data());
        }
        bool swapchain = false, maintenance1 = false;
        for (const auto &extension : device_extensions)
        {
            swapchain |= std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            maintenance1 |= std::strcmp(extension.extensionName, VK_KHR_MAINTENANCE_1_EXTENSION_NAME) == 0;
        }
        info_log("Vulkan report: %u device extensions, swapchain %d, maintenance1 %d",
                 extension_count, swapchain, maintenance1);

        uint32_t families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &families, nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);
        if (families)
        {
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, queues.data());
        }
        for (uint32_t i = 0; i < families; i++)
        {
            info_log("Vulkan report: queue family %u flags 0x%x count %u", i,
                     (unsigned)queues[i].queueFlags, queues[i].queueCount);
        }

        if (vkGetPhysicalDeviceDisplayPropertiesKHR && vkGetDisplayModePropertiesKHR)
        {
            uint32_t displays = 0;
            rc = vkGetPhysicalDeviceDisplayPropertiesKHR(device, &displays, nullptr);
            info_log("Vulkan report: vkGetPhysicalDeviceDisplayPropertiesKHR = %d, %u displays",
                     (int)rc, displays);
            std::vector<VkDisplayPropertiesKHR> display_list(displays);
            if (displays)
            {
                vkGetPhysicalDeviceDisplayPropertiesKHR(device, &displays, display_list.data());
            }
            for (const auto &display : display_list)
            {
                uint32_t modes = 0;
                rc = vkGetDisplayModePropertiesKHR(device, display.display, &modes, nullptr);
                std::vector<VkDisplayModePropertiesKHR> mode_list(modes);
                if (modes)
                {
                    vkGetDisplayModePropertiesKHR(device, display.display, &modes, mode_list.data());
                }
                for (const auto &mode : mode_list)
                {
                    info_log("Vulkan report: display mode %ux%u %u mHz",
                             mode.parameters.visibleRegion.width,
                             mode.parameters.visibleRegion.height, mode.parameters.refreshRate);
                }
            }
        }
        MikuPan_FlushLog();
    }
    vkDestroyInstance(instance, nullptr);
    info_log("Vulkan report: done");
    MikuPan_FlushLog();
#undef PS5_VK
}
