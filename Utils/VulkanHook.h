#pragma once
#include "Hook.h"
#include "DetourTxn.h"
#include "VulkanProxy.h"
#include "Common.h"

PFN_vkGetPhysicalDeviceProperties originalVkGetPhysicalDeviceProperties = nullptr;
PFN_vkGetPhysicalDeviceProperties2 originalVkGetPhysicalDeviceProperties2 = nullptr;
PFN_vkGetPhysicalDeviceProperties2KHR originalVkGetPhysicalDeviceProperties2KHR = nullptr;
PFN_vkGetPhysicalDeviceFeatures2 originalVkGetPhysicalDeviceFeatures2 = nullptr;
PFN_vkGetPhysicalDeviceQueueFamilyProperties originalVkGetPhysicalDeviceQueueFamilyProperties = nullptr;
PFN_vkEnumerateInstanceExtensionProperties originalVkEnumerateInstanceExtensionProperties = nullptr;
PFN_vkEnumerateDeviceExtensionProperties originalVkEnumerateDeviceExtensionProperties = nullptr;
PFN_vkCreateDevice originalVkCreateDevice = nullptr;
PFN_vkQueuePresentKHR originalVkQueuePresentKHR = nullptr;
PFN_vkWaitSemaphores originalVkWaitSemaphores = nullptr;
PFN_vkWaitSemaphoresKHR originalVkWaitSemaphoresKHR = nullptr;

struct HookVulkan : IHook {
    const std::wstring Name() const override { return L"Vulkan hooks"; }
    HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
    int         Priority() const override { return 5000; }

    bool CanInstall(Context& ctx, IDetourApi& api) override {
        return api.GetModHandleW(L"vulkan-1.dll") != nullptr;
    }

    bool Install(Context& ctx, IDetourApi& api) override {
        static bool done = false;
        if (done) {
            return false;
        }

        done = true;
        if (!ctx.enableVulkanSpoofing) {
            LOG_INFO(L"[VULKAN] Built-in GPU spoofing disabled");
            return false;
        }

        auto vk = api.GetModHandleW(L"vulkan-1.dll");

        DetourTxn txn(api);
        originalVkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)api.GetProc(vk, "vkGetPhysicalDeviceProperties");
        if (originalVkGetPhysicalDeviceProperties) {
            if (!txn.attach((void**)&originalVkGetPhysicalDeviceProperties, (void*)&proxy_VkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties")) return false;
        }

        originalVkGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)api.GetProc(vk, "vkGetPhysicalDeviceProperties2");
        if (originalVkGetPhysicalDeviceProperties2) {
            if (!txn.attach((void**)&originalVkGetPhysicalDeviceProperties2, (void*)&proxy_VkGetPhysicalDeviceProperties2, "vkGetPhysicalDeviceProperties2")) return false;
        }

        originalVkGetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)api.GetProc(vk, "vkGetPhysicalDeviceProperties2KHR");
        if (originalVkGetPhysicalDeviceProperties2KHR) {
            if (!txn.attach((void**)&originalVkGetPhysicalDeviceProperties2KHR, (void*)&proxy_VkGetPhysicalDeviceProperties2KHR, "vkGetPhysicalDeviceProperties2KHR")) return false;
        }

        originalVkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)api.GetProc(vk, "vkGetPhysicalDeviceFeatures2");
        if (originalVkGetPhysicalDeviceFeatures2) {
            if (!txn.attach((void**)&originalVkGetPhysicalDeviceFeatures2, (void*)&proxy_VkGetPhysicalDeviceFeatures2, "vkGetPhysicalDeviceFeatures2")) return false;
        }

        originalVkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)api.GetProc(vk, "vkGetPhysicalDeviceQueueFamilyProperties");
        if (originalVkGetPhysicalDeviceQueueFamilyProperties) {
            if (!txn.attach((void**)&originalVkGetPhysicalDeviceQueueFamilyProperties, (void*)&proxy_VkGetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties")) return false;
        }

        originalVkCreateDevice = (PFN_vkCreateDevice)api.GetProc(vk, "vkCreateDevice");
        if (originalVkCreateDevice) {
            if (!txn.attach((void**)&originalVkCreateDevice, (void*)&proxy_VkCreateDevice, "vkCreateDevice")) return false;
        }
 
        originalVkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)api.GetProc(vk, "vkEnumerateInstanceExtensionProperties");
        if (originalVkEnumerateInstanceExtensionProperties) {
            if (!txn.attach((void**)&originalVkEnumerateInstanceExtensionProperties, (void*)&proxy_VkEnumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties")) return false;
        }

        originalVkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)api.GetProc(vk, "vkEnumerateDeviceExtensionProperties");
        if (originalVkEnumerateDeviceExtensionProperties) {
            if (!txn.attach((void**)&originalVkEnumerateDeviceExtensionProperties, (void*)&proxy_VkEnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties")) return false;
        }

        auto result = txn.commit();

        if (result) {
            LOG_INFO(L"[VULKAN] Built-in GPU spoofing enabled");
        }
        else {
            LOG_ERROR(L"[VULKAN] Detours failed");
        }
        return txn.commit();
    }
};
