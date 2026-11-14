/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * vulkan.exe: does Vulkan work in this Wine build? Loads vulkan-1.dll,
 * creates an instance and lists the physical devices (on macOS: MoltenVK,
 * through the bundled loader). Prints one line per device and exits 0 if
 * an instance could be created; 1 if vulkan-1.dll or vkCreateInstance
 * fails. No Vulkan headers needed: the few types used are declared here,
 * as in the Vulkan spec.
 *
 * Built by build/smoke.sh with the toolchain's llvm-mingw:
 *   x86_64-w64-mingw32-clang -O2 -o vulkan.exe tests/probes/vulkan.c
 * (plan.md section 4: small probes for what real programs don't cover.)
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int VkResult;
typedef void *VkInstance;
typedef void *VkPhysicalDevice;
typedef void (*PFN_vkVoidFunction)(void);

typedef struct
{
    int sType;              /* VK_STRUCTURE_TYPE_APPLICATION_INFO = 0 */
    const void *pNext;
    const char *pApplicationName;
    unsigned int applicationVersion;
    const char *pEngineName;
    unsigned int engineVersion;
    unsigned int apiVersion;
} VkApplicationInfo;

typedef struct
{
    int sType;              /* VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1 */
    const void *pNext;
    unsigned int flags;
    const VkApplicationInfo *pApplicationInfo;
    unsigned int enabledLayerCount;
    const char *const *ppEnabledLayerNames;
    unsigned int enabledExtensionCount;
    const char *const *ppEnabledExtensionNames;
} VkInstanceCreateInfo;

/* VkPhysicalDeviceProperties up to deviceName; the rest is skipped. */
typedef struct
{
    unsigned int apiVersion;
    unsigned int driverVersion;
    unsigned int vendorID;
    unsigned int deviceID;
    int deviceType;
    char deviceName[256];
    unsigned char rest[1024];
} VkPhysicalDevicePropertiesPrefix;

typedef PFN_vkVoidFunction (WINAPI *PFN_vkGetInstanceProcAddr)(VkInstance, const char *);
typedef VkResult (WINAPI *PFN_vkCreateInstance)(const VkInstanceCreateInfo *, const void *, VkInstance *);
typedef VkResult (WINAPI *PFN_vkEnumeratePhysicalDevices)(VkInstance, unsigned int *, VkPhysicalDevice *);
typedef void (WINAPI *PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, VkPhysicalDevicePropertiesPrefix *);
typedef void (WINAPI *PFN_vkDestroyInstance)(VkInstance, const void *);

int main(void)
{
    VkApplicationInfo app = { 0, NULL, "wine-mac-builds probe", 1, NULL, 0, (1u << 22) | (1u << 12) /* 1.1 */ };
    VkInstanceCreateInfo info = { 1, NULL, 0, &app, 0, NULL, 0, NULL };
    PFN_vkGetInstanceProcAddr gipa;
    PFN_vkCreateInstance create;
    PFN_vkEnumeratePhysicalDevices enum_devices;
    PFN_vkGetPhysicalDeviceProperties get_props;
    PFN_vkDestroyInstance destroy;
    VkPhysicalDevice devices[16];
    unsigned int count = 16, i;
    VkInstance instance;
    VkResult res;
    HMODULE vk;

    if (!(vk = LoadLibraryA("vulkan-1.dll")))
    {
        printf("vulkan: can't load vulkan-1.dll (error %lu)\n", GetLastError());
        return 1;
    }
    gipa = (PFN_vkGetInstanceProcAddr)GetProcAddress(vk, "vkGetInstanceProcAddr");
    create = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
    if (!create || (res = create(&info, NULL, &instance)))
    {
        printf("vulkan: vkCreateInstance failed (%d)\n", create ? res : -1);
        return 1;
    }
    enum_devices = (PFN_vkEnumeratePhysicalDevices)gipa(instance, "vkEnumeratePhysicalDevices");
    get_props = (PFN_vkGetPhysicalDeviceProperties)gipa(instance, "vkGetPhysicalDeviceProperties");
    destroy = (PFN_vkDestroyInstance)gipa(instance, "vkDestroyInstance");
    res = enum_devices(instance, &count, devices);
    if (res < 0) count = 0;
    printf("vulkan: instance created, %u device(s)\n", count);
    for (i = 0; i < count; i++)
    {
        VkPhysicalDevicePropertiesPrefix props;
        memset(&props, 0, sizeof(props));
        get_props(devices[i], &props);
        printf("vulkan: device %u: %s (Vulkan %u.%u)\n", i, props.deviceName,
               props.apiVersion >> 22, (props.apiVersion >> 12) & 0x3ff);
    }
    destroy(instance, NULL);
    return 0;
}
