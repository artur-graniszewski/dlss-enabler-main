#include "../Core/Context.h"
#include "../Detours/detours.h"
#include "VulkanProxy.h"
#include "../Utils/Common.h"
//#define VK_OPTIFLOW_ON

static bool foundImageViewHandle = false;
static bool foundBinaryImport = false;

extern VkDevice vkDevice;

typedef struct VkDummyProps
{
    VkStructureType    sType;
    void* pNext;
} VkDummyProps;

static uint32_t vkEnumerateInstanceExtensionPropertiesCount = 0;
static uint32_t vkEnumerateDeviceExtensionPropertiesCount = 0;

uint64_t streamlineSignalId = 0;

VkResult proxy_VkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout)
{
    static bool reported = false;
    if (streamlineSignalId > 0) {
        for (uint32_t i = 0; i < pWaitInfo->semaphoreCount; ++i) {
            LOG_INFO(L"[VULKAN] CHECK: " + std::to_wstring(streamlineSignalId) + L" : " + std::to_wstring(pWaitInfo->pValues[i]));
            if (pWaitInfo->pValues[i] == streamlineSignalId) {

                streamlineSignalId = 0;
                return VK_SUCCESS;
            }
        }
    }

    if (!reported) {
        LOG_INFO(L"[VULKAN] VkWaitSemaphores");
    }

    VkResult result = VK_SUCCESS;

    if (true) {
        result = originalVkWaitSemaphores(device, pWaitInfo, timeout);
    }
    else {
        //streamlineSignalId = 0;
    }

    if (!reported) {
        LOG_INFO(L"[VULKAN] VkWaitSemaphores: " + std::to_wstring(result));
        reported = true;
    }

    return result;
}

VkResult proxy_VkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout)
{
    static bool reported = false;
    if (streamlineSignalId > 0) {
        for (uint32_t i = 0; i < pWaitInfo->semaphoreCount; ++i) {
            //LOG_INFO(L"[VULKAN] CHECK: " + std::to_wstring(streamlineSignalId) + L" : " + std::to_wstring(pWaitInfo->pValues[i]));
            if (pWaitInfo->pValues[i] == streamlineSignalId) {

                streamlineSignalId = 0;
                return VK_SUCCESS;
            }
        }
    }
    if (!reported) {
        LOG_INFO(L"[VULKAN] VkWaitSemaphoresKHR");
    }

    VkResult result = VK_SUCCESS;
    result = originalVkWaitSemaphoresKHR(device, pWaitInfo, timeout);

    if (!reported) {
        LOG_INFO(L"[VULKAN] VkWaitSemaphoresKHR: " + std::to_wstring(result));
        reported = true;
    }

    return result;
}

VkResult proxy_VkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* info)
{
    static bool reported = false;

    if (!reported) {
        LOG_TRACE(L"[VULKAN] VkQueuePresentKHR");
    }

    auto result = originalVkQueuePresentKHR(queue, info);

    if (!reported) {
        LOG_TRACE(L"[VULKAN] VkQueuePresentKHR: " + std::to_wstring(result));
        reported = true;
    }

    return result;
}


void Vulkan_HookDeviceFunctions()
{

    if (vkDevice && !originalVkQueuePresentKHR) {
        HMODULE vulkanModule = GetModuleHandleW(L"vulkan-1.dll");
        PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr =
            reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(vulkanModule, "vkGetDeviceProcAddr"));


        originalVkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(pfnVkGetDeviceProcAddr(vkDevice, "vkQueuePresentKHR"));

        originalVkWaitSemaphores = reinterpret_cast<PFN_vkWaitSemaphores>(pfnVkGetDeviceProcAddr(vkDevice, "vkWaitSemaphores"));
        originalVkWaitSemaphoresKHR = reinterpret_cast<PFN_vkWaitSemaphoresKHR>(pfnVkGetDeviceProcAddr(vkDevice, "vkWaitSemaphoresKHR"));


        if (originalVkQueuePresentKHR) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)originalVkQueuePresentKHR, proxy_VkQueuePresentKHR);
            DetourTransactionCommit();
        }

        if (originalVkWaitSemaphores) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)originalVkWaitSemaphores, proxy_VkWaitSemaphores);
            DetourTransactionCommit();
        }

        if (originalVkWaitSemaphoresKHR) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)originalVkWaitSemaphoresKHR, proxy_VkWaitSemaphoresKHR);
            DetourTransactionCommit();
        }
    }
}


VkResult proxy_VkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    LOG_INFO(L"[VULKAN] VkEnumerateDeviceExtensionProperties");

    auto count = *pPropertyCount;
    auto result = originalVkEnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);

    if (result != VK_SUCCESS) {
        return result;
    }

    if (pLayerName == nullptr && pProperties == nullptr && count == 0) {
        *pPropertyCount += 3;
        vkEnumerateDeviceExtensionPropertiesCount = *pPropertyCount;
        return result;
    }

    // Check if pProperties is not null to validate the presence of extensions
    if (pProperties != nullptr) {
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            if (strcmp(pProperties[i].extensionName, VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME) == 0) {
                foundImageViewHandle = true;
            }
            if (strcmp(pProperties[i].extensionName, VK_NVX_BINARY_IMPORT_EXTENSION_NAME) == 0) {
                foundBinaryImport = true;
            }
        }
    }

    if (pLayerName == nullptr && pProperties != nullptr && *pPropertyCount > 0)
    {
        if (count == vkEnumerateDeviceExtensionPropertiesCount) {
            *pPropertyCount = count;
        }

        VkExtensionProperties bi{ VK_NVX_BINARY_IMPORT_EXTENSION_NAME, VK_NVX_BINARY_IMPORT_SPEC_VERSION };
        memcpy(&pProperties[*pPropertyCount - 1], &bi, sizeof(VkExtensionProperties));

        VkExtensionProperties ivh{ VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME, VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION };
        memcpy(&pProperties[*pPropertyCount - 2], &ivh, sizeof(VkExtensionProperties));

        VkExtensionProperties of{ VK_NV_OPTICAL_FLOW_EXTENSION_NAME, VK_NV_OPTICAL_FLOW_SPEC_VERSION };
        memcpy(&pProperties[*pPropertyCount - 3], &of, sizeof(VkExtensionProperties));
    }

    LOG_INFO(L"[VULKAN] VkEnumerateDeviceExtensionProperties: " + std::to_wstring(result));

    return result;
}

VkResult proxy_VkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    LOG_INFO(L"[VULKAN] VkEnumerateInstanceExtensionProperties");

    auto count = *pPropertyCount;
    auto result = originalVkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);

    if (result != VK_SUCCESS)
        return result;

    if (pLayerName == nullptr && pProperties == nullptr && count == 0) {
        *pPropertyCount += 3;
        vkEnumerateInstanceExtensionPropertiesCount = *pPropertyCount;
        return result;
    }

    // Check if pProperties is not null to validate the presence of extensions
    if (pProperties != nullptr) {
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            if (strcmp(pProperties[i].extensionName, VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME) == 0) {
                foundImageViewHandle = true;
            }
            if (strcmp(pProperties[i].extensionName, VK_NVX_BINARY_IMPORT_EXTENSION_NAME) == 0) {
                foundBinaryImport = true;
            }
        }
    }

    if (pLayerName == nullptr && pProperties != nullptr && *pPropertyCount > 0) {
        if (vkEnumerateInstanceExtensionPropertiesCount == count) {
            *pPropertyCount = count;
        }

        VkExtensionProperties bi{ VK_NVX_BINARY_IMPORT_EXTENSION_NAME, VK_NVX_BINARY_IMPORT_SPEC_VERSION };
        memcpy(&pProperties[*pPropertyCount - 1], &bi, sizeof(VkExtensionProperties));

        VkExtensionProperties ivh{ VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME, VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION };
        memcpy(&pProperties[*pPropertyCount - 2], &ivh, sizeof(VkExtensionProperties));

        VkExtensionProperties of{ VK_NV_OPTICAL_FLOW_EXTENSION_NAME, VK_NV_OPTICAL_FLOW_SPEC_VERSION };
        memcpy(&pProperties[*pPropertyCount - 3], &of, sizeof(VkExtensionProperties));
    }

    LOG_INFO(L"[VULKAN] VkEnumerateInstanceExtensionProperties: " + std::to_wstring(result));

    return result;
}

VkResult VKAPI_PTR proxy_VkCreateDevice(
    VkPhysicalDevice physicalDevice,
    VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    LOG_INFO(L"[VULKAN] VkCreateDevice");

    // Log the number of requested extensions
    LOG_DEBUG(L"[VULKAN] Requested " + std::to_wstring(pCreateInfo->enabledExtensionCount) + L" extensions: ");

    std::vector<std::wstring> extensionsToRemove = {};

    LOG_ERROR(L"GPU ARCH: " + std::to_wstring(ctx.realGpuArchitecture));
    LOG_ERROR(L"API INIT: " + std::to_wstring((int)ctx.nvapi.isInitialized));
    LOG_ERROR(L"IS WINDO: " + std::to_wstring((int)ctx.isRunningUnderWindows));
    if (ctx.isVulkanApplication || (ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100 && (ctx.nvapi.isInitialized || ctx.isRunningUnderWindows))) {
        if (!foundImageViewHandle) {
            extensionsToRemove.push_back(L"VK_NVX_image_view_handle");
        }

        if (!foundBinaryImport) {
            extensionsToRemove.push_back(L"VK_NVX_binary_import");
        }

        if (!foundBinaryImport || !foundImageViewHandle) {
            LOG_DEBUG(L"[VULKAN] Added DLSS extensions to the block list, reason: unsupported GPU architecture");
        }
    }

    if (!ctx.ngx.isDlssgEnabled) {
        extensionsToRemove.push_back(ToWideString(VK_NV_OPTICAL_FLOW_EXTENSION_NAME));
        LOG_DEBUG(L"[VULKAN] Added DLSSG extensions to the block list, reason: unsupported GPU architecture");
    }

    std::vector<const char*> newExtensionList;

    for (size_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
        auto wideExt = ToWideString(pCreateInfo->ppEnabledExtensionNames[i]);
        if (std::find(extensionsToRemove.begin(), extensionsToRemove.end(), wideExt) != extensionsToRemove.end()
            &&
            // remove the extensions only if the real GPU architecture is known, this is a problem with dxvk which uses all extensions before we
            // get the info about the GPU arch
            (ctx.isVulkanApplication || (ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100 && (ctx.nvapi.isInitialized || ctx.isRunningUnderWindows)))
            ) {
            LOG_DEBUG(L"[VULKAN]    " + wideExt + L" [ignored/unsupported]");
        }
        else {
            newExtensionList.push_back(pCreateInfo->ppEnabledExtensionNames[i]);
            LOG_DEBUG(L"[VULKAN]    " + wideExt);
        }
    }

    if (std::find(newExtensionList.begin(), newExtensionList.end(), VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == newExtensionList.end()) {
        newExtensionList.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    }

    pCreateInfo->enabledExtensionCount = static_cast<uint32_t>(newExtensionList.size());
    pCreateInfo->ppEnabledExtensionNames = newExtensionList.data();

    std::vector<VkStructureType> sTypesToRemove = { static_cast<VkStructureType>(1000464000), static_cast<VkStructureType>(53) };

    if (!ctx.ngx.isDlssgEnabled) {
        //vulkan_RemoveStructuresFromPNext(pCreateInfo, sTypesToRemove);
    }

    LOG_DEBUG(L"[VULKAN] Proxying");

    // Call the original function with the possibly modified parameters
    VkResult result = originalVkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);

    LOG_INFO(L"[VULKAN] VkCreateDevice: " + (result == VK_SUCCESS ? L"succeeded" : L"failed (" + std::to_wstring(result) + L")"));
    return result;
}

VkResult VKAPI_PTR proxy_VkCreateDevice2(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    LOG_INFO(L"[VULKAN] VkCreateDevice");

    // Log the number of requested extensions
    LOG_DEBUG(L"[VULKAN] Requested " + std::to_wstring(pCreateInfo->enabledExtensionCount) + L" extensions: ");

    std::vector<std::wstring> extensionsToRemove = {
        L"VK_NV_optical_flow"
    };

    if (ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100 && (ctx.nvapi.isInitialized || ctx.isRunningUnderWindows)) {
        extensionsToRemove.push_back(L"VK_NVX_image_view_handle");
        extensionsToRemove.push_back(L"VK_NVX_binary_import");
    }

    // Create a vector to store the filtered list of extensions
    std::vector<const char*> filteredExtensions;

    // Iterate through the original extensions and add them to the filtered list unless they match one to remove
    for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
        std::string ext(pCreateInfo->ppEnabledExtensionNames[i]);
        std::wstring wideExt = ToWideString(ext);

        if (std::find(extensionsToRemove.begin(), extensionsToRemove.end(), wideExt) == extensionsToRemove.end()) {
            filteredExtensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);
            LOG_DEBUG(L"[VULKAN]    " + wideExt);
        }
        else {
            LOG_DEBUG(L"[VULKAN]    " + wideExt + L" [ignored/unsupported]");
        }
    }
    // Copy the original VkDeviceCreateInfo structure
    VkDeviceCreateInfo modifiedCreateInfo = *pCreateInfo;
    modifiedCreateInfo.enabledExtensionCount = static_cast<uint32_t>(filteredExtensions.size());
    modifiedCreateInfo.ppEnabledExtensionNames = filteredExtensions.data();



    LOG_DEBUG(L"[VULKAN] Proxying");
    // Call the original function with the possibly modified parameters
    VkResult result = originalVkCreateDevice(physicalDevice, &modifiedCreateInfo, pAllocator, pDevice);


    LOG_INFO(L"[VULKAN] VkCreateDevice: " + (result == VK_SUCCESS ? L"succeeded" : L"failed (" + std::to_wstring(result) + L")"));
    return result;
}

void VKAPI_PTR proxy_VkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties* pQueueFamilyProperties)
{
    static int numReported = 0;
    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceQueueFamilyProperties");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceQueueFamilyProperties (more than 20 entries...)");
    }

    // Call the original function first
    originalVkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);

    if (pQueueFamilyProperties != nullptr) {
        VkQueueFlags requiredCaps = VK_QUEUE_OPTICAL_FLOW_BIT_NV;
        bool foundOpticalFlowQueue = false;

        // Check if any of the queue families already have the required optical flow capability
        for (uint32_t i = 0; i < *pQueueFamilyPropertyCount; ++i) {
            if ((pQueueFamilyProperties[i].queueFlags & requiredCaps) == requiredCaps) {
                foundOpticalFlowQueue = true;
                break;
            }
        }

        // If no queue family has the optical flow capability, modify the first one to include it
        if (!foundOpticalFlowQueue && *pQueueFamilyPropertyCount > 0) {
            pQueueFamilyProperties[0].queueFlags |= requiredCaps;
        }
    }

    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceQueueFamilyProperties: succeeded");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceQueueFamilyProperties: succeeded (more than 20 entries...)");
    }

    numReported++;
}

void VKAPI_PTR proxy_VkGetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures2* pFeatures)
{
    static int numReported = 0;
    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceFeatures2");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceFeatures2 (more than 20 entries...)");
    }

    // Call the original function
    originalVkGetPhysicalDeviceFeatures2(physicalDevice, pFeatures);

    // Traverse the pNext chain to find VkPhysicalDeviceSynchronization2Features and VkPhysicalDeviceOpticalFlowFeaturesNV
    void* pNext = pFeatures->pNext;

    while (pNext) {
        VkBaseOutStructure* pBase = reinterpret_cast<VkBaseOutStructure*>(pNext);

        if (pBase->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES) {
            VkPhysicalDeviceSynchronization2Features* synchronization2Features =
                reinterpret_cast<VkPhysicalDeviceSynchronization2Features*>(pBase);

            // Set synchronization2 to VK_TRUE
#ifdef VK_OPTIFLOW_ON 
            synchronization2Features->synchronization2 = VK_TRUE;
#endif
        }
        else if (pBase->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV) {
            VkPhysicalDeviceOpticalFlowFeaturesNV* opticalFlowFeaturesNV =
                reinterpret_cast<VkPhysicalDeviceOpticalFlowFeaturesNV*>(pBase);
#ifdef VK_OPTIFLOW_ON 
            // Set opticalFlow to VK_TRUE
            opticalFlowFeaturesNV->opticalFlow = VK_TRUE;
#endif
        }

        pNext = pBase->pNext;
    }

    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceFeatures2: succeeded");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceFeatures2: succeeded (more than 20 entries...)");
    }

    numReported++;
}


//---------------
void proxy_VkGetPhysicalDeviceProperties(VkPhysicalDevice physical_device, VkPhysicalDeviceProperties* properties)
{
    static int numReported = 0;
    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties (more than 20 entries...)");
    }
    originalVkGetPhysicalDeviceProperties(physical_device, properties);

    if (ctx.gpu.desiredDeviceName != nullptr && strlen(ctx.gpu.desiredDeviceName) > 0) {
        strncpy_s(properties->deviceName, sizeof(properties->deviceName), ctx.gpu.desiredDeviceName, _TRUNCATE);
    }
    properties->vendorID = 0x10de;
    //properties->deviceID = 0x2684;
    properties->driverVersion = VK_MAKE_API_VERSION(566, 0, 0, 0);

    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties: succeeded");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties: succeeded (more than 20 entries...)");
    }

    numReported++;
}

void proxy_VkGetPhysicalDeviceProperties2(VkPhysicalDevice phys_dev, VkPhysicalDeviceProperties2* properties2)
{
    static int numReported = 0;
    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties2");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties2 (more than 20 entries...)");
    }

    originalVkGetPhysicalDeviceProperties2(phys_dev, properties2);
    if (ctx.gpu.desiredDeviceName != nullptr && strlen(ctx.gpu.desiredDeviceName) > 0) {
        strncpy_s(properties2->properties.deviceName, sizeof(properties2->properties.deviceName), ctx.gpu.desiredDeviceName, _TRUNCATE);
    }
    properties2->properties.vendorID = 0x10de;
    //properties2->properties.deviceID = 0x2684;
    properties2->properties.driverVersion = VK_MAKE_API_VERSION(566, 0, 0, 0);

    auto next = (VkDummyProps*)properties2->pNext;

    while (next != nullptr)
    {
        if (next->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES)
        {
            auto ddp = (VkPhysicalDeviceDriverProperties*)(void*)next;
            ddp->driverID = VK_DRIVER_ID_NVIDIA_PROPRIETARY;
            strncpy_s(ddp->driverName, sizeof(ddp->driverName), "NVIDIA", _TRUNCATE);
            strncpy_s(ddp->driverInfo, sizeof(ddp->driverInfo), "566.0", _TRUNCATE);
        }

        next = (VkDummyProps*)next->pNext;
    }

    if (numReported < 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties2: succeeded");
    }
    else if (numReported == 20) {
        LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties2: succeeded (more than 20 entries...)");
    }

    numReported++;
}

void proxy_VkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice phys_dev, VkPhysicalDeviceProperties2* properties2)
{
    LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties2KHR");
    originalVkGetPhysicalDeviceProperties2KHR(phys_dev, properties2);
    if (ctx.gpu.desiredDeviceName != nullptr && strlen(ctx.gpu.desiredDeviceName) > 0) {
        strncpy_s(properties2->properties.deviceName, sizeof(properties2->properties.deviceName), ctx.gpu.desiredDeviceName, _TRUNCATE);
    }
    properties2->properties.vendorID = 0x10de;
    //properties2->properties.deviceID = 0x2684;
    properties2->properties.driverVersion = VK_MAKE_API_VERSION(566, 0, 0, 0);

    auto next = (VkDummyProps*)properties2->pNext;

    while (next != nullptr)
    {
        if (next->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES)
        {
            auto ddp = (VkPhysicalDeviceDriverProperties*)(void*)next;
            ddp->driverID = VK_DRIVER_ID_NVIDIA_PROPRIETARY;
            strncpy_s(ddp->driverName, sizeof(ddp->driverName), "NVIDIA", _TRUNCATE);
            strncpy_s(ddp->driverInfo, sizeof(ddp->driverInfo), "566.0", _TRUNCATE);
        }

        next = (VkDummyProps*)next->pNext;
    }

    LOG_INFO(L"[VULKAN] VkGetPhysicalDeviceProperties2KHR: succeeded");
}
