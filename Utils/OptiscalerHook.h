#pragma once
#include "Hook.h"
#include "Common.h"
#include "Console.h"
#include "Optiscaler.h"
#include "NvngxProxy.h"
#include "DetourTxn.h"
#include "ProcAliasRegistry.h";

decltype(&proxy_NVSDK_NGX_UpdateFeature) org_NVSDK_NGX_UpdateFeature = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_Shutdown1) org_NVSDK_NGX_D3D11_Shutdown1 = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_Shutdown1) org_NVSDK_NGX_D3D12_Shutdown1 = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_Shutdown1) org_NVSDK_NGX_VULKAN_Shutdown1 = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_Shutdown) org_NVSDK_NGX_D3D11_Shutdown = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_Shutdown) org_NVSDK_NGX_D3D12_Shutdown = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_Shutdown) org_NVSDK_NGX_VULKAN_Shutdown = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_Init_Ext) org_NVSDK_NGX_D3D11_Init_Ext = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_Init_Ext) org_NVSDK_NGX_D3D12_Init_Ext = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_Init) org_NVSDK_NGX_D3D11_Init = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_Init) org_NVSDK_NGX_D3D12_Init = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_Init) org_NVSDK_NGX_VULKAN_Init = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_Init_Ext) org_NVSDK_NGX_VULKAN_Init_Ext = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_Init_Ext2) org_NVSDK_NGX_VULKAN_Init_Ext2 = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_GetScratchBufferSize) org_NVSDK_NGX_D3D11_GetScratchBufferSize = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_GetScratchBufferSize) org_NVSDK_NGX_D3D12_GetScratchBufferSize = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_GetScratchBufferSize) org_NVSDK_NGX_VULKAN_GetScratchBufferSize = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_Init_ProjectID) org_NVSDK_NGX_D3D11_Init_ProjectID = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_Init_ProjectID) org_NVSDK_NGX_D3D12_Init_ProjectID = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_Init_ProjectID) org_NVSDK_NGX_VULKAN_Init_ProjectID = nullptr;

decltype(&proxy_NVSDK_NGX_VULKAN_RequiredExtensions) org_NVSDK_NGX_VULKAN_RequiredExtensions = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_GetParameters) org_NVSDK_NGX_D3D11_GetParameters = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_GetParameters) org_NVSDK_NGX_D3D12_GetParameters = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_GetParameters) org_NVSDK_NGX_VULKAN_GetParameters = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_EvaluateFeature) org_NVSDK_NGX_D3D11_EvaluateFeature = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_EvaluateFeature) org_NVSDK_NGX_D3D12_EvaluateFeature = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_EvaluateFeature) org_NVSDK_NGX_VULKAN_EvaluateFeature = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_GetCapabilityParameters) org_NVSDK_NGX_D3D11_GetCapabilityParameters = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_GetCapabilityParameters) org_NVSDK_NGX_D3D12_GetCapabilityParameters = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_GetCapabilityParameters) org_NVSDK_NGX_VULKAN_GetCapabilityParameters = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_ReleaseFeature) org_NVSDK_NGX_D3D11_ReleaseFeature = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_ReleaseFeature) org_NVSDK_NGX_D3D12_ReleaseFeature = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_ReleaseFeature) org_NVSDK_NGX_VULKAN_ReleaseFeature = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_CreateFeature) org_NVSDK_NGX_D3D11_CreateFeature = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_CreateFeature) org_NVSDK_NGX_D3D12_CreateFeature = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_CreateFeature) org_NVSDK_NGX_VULKAN_CreateFeature = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_CreateFeature1) org_NVSDK_NGX_VULKAN_CreateFeature1 = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_DestroyParameters) org_NVSDK_NGX_D3D11_DestroyParameters = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_DestroyParameters) org_NVSDK_NGX_D3D12_DestroyParameters = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_DestroyParameters) org_NVSDK_NGX_VULKAN_DestroyParameters = nullptr;

decltype(&proxy_NVSDK_NGX_D3D11_AllocateParameters) org_NVSDK_NGX_D3D11_AllocateParameters = nullptr;
decltype(&proxy_NVSDK_NGX_D3D12_AllocateParameters) org_NVSDK_NGX_D3D12_AllocateParameters = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_AllocateParameters) org_NVSDK_NGX_VULKAN_AllocateParameters = nullptr;

decltype(&proxy_NVSDK_NGX_D3D12_GetFeatureRequirements) org_NVSDK_NGX_D3D12_GetFeatureRequirements = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_GetFeatureRequirements) org_NVSDK_NGX_VULKAN_GetFeatureRequirements = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements) org_NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements = nullptr;
decltype(&proxy_NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements) org_NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements = nullptr;


struct HookOptiscaler : IHook {
    const std::wstring Name() const override { return L"Optiscaler hooks"; }
    HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
    int         Priority() const override { return 1; }

    bool CanInstall(Context& ctx, IDetourApi& api) override {
        // reacts on two events
        // 1. when _nvngx.dll loads (both DE and Opti rely on convention that _nvngx.dll comes from NVIDIA driver and is not spoofed as a physical file)
        // 2. when nvngx.dll loads (which usually happens when no _nvngx.dll can be located - possibly due to the lack of NVIDIA driver present)
        //    in second case additional failsafe is used (ctx.ngx.isRealNgxPresent, which is based on registry settings for file locations)
        return
            api.GetModHandleW(L"_nvngx.dll") != nullptr || (!ctx.ngx.isRealNgxPresent && api.GetModHandleW(L"nvngx.dll") != nullptr);
    }

    bool Install(Context& ctx, IDetourApi& api) override {
        static bool optiStarted = false;
        if (optiStarted) {
            return false;
        }
        optiStarted = true;
        LOG_INFO(L"[NVNGX] Optiscaler initialized");
        std::wstring modulePath = Common::GetModuleFilePath();
        
        return true;
    }
};