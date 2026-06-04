#pragma once
#include "Hook.h"
#include "Common.h"
#include "DetourTxn.h"
#include "NgxFrontend.h"
#include "NgxHookHelpers.h"
#include <vulkan/vulkan_core.h>
#include "Console.h"

namespace NGXFrontend
{
    std::unique_ptr<NGX::NgxFrontend> ngxFrontend;

    NgxRuntimeState ngxRuntimeState;

    // Tiny default logger & loader that forward to your current logger and Win32
    class DefaultLogger : public INgxLogger {
    public:
        void Info(const std::wstring msg) override { Console::Info(msg); }
        void Warning(const std::wstring msg) override { Console::Warning(msg); }
        void Error(const std::wstring msg) override { Console::Error(msg); }
    };

    class DefaultBackendLoader : public IBackendLoader {
    public:
        HMODULE Load(const std::wstring& path) override { return ::LoadLibraryW(path.c_str()); }
        FARPROC Resolve(HMODULE module, const char* name) override { return ::GetProcAddress(module, name); }

    };

    static DefaultLogger globalLogger;
    static DefaultBackendLoader globalNgxLoader;

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Init,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)) {
        return ngxFrontend->InitVulkan(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InFeatureInfo, InSDKVersion);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Init_Ext,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return ngxFrontend->InitVulkanExt(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Init_Ext2,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return ngxFrontend->InitVulkanExt2(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_CreateFeature, (VkCommandBuffer InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)) {
        return ngxFrontend->CreateVulkan(InCmdList, InFeatureID, InParameters, OutHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_CreateFeature1, (const VkDevice InDevice, VkCommandBuffer InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)) {
        return ngxFrontend->CreateVulkan1(InDevice, InCmdList, InFeatureID, InParameters, OutHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_GetFeatureRequirements, (
        IDXGIAdapter* Adapter,
        NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)) {
        return ngxFrontend->GetFeatureRequirementsD3D12(Adapter, FeatureDiscoveryInfo, RequirementInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Init_Ext,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
            ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return ngxFrontend->InitD3D12Ext(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_GetFeatureRequirements, (
        IDXGIAdapter* Adapter,
        NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)) {
        return ngxFrontend->GetFeatureRequirementsD3D11(Adapter, FeatureDiscoveryInfo, RequirementInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_GetFeatureRequirements, (
        const ::VkInstance instance,
        const ::VkPhysicalDevice device,
        NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)) {
        return ngxFrontend->GetFeatureRequirementsVulkan(instance, device, FeatureDiscoveryInfo, RequirementInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_Init, (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D11Device* InDevice,
        NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)) {
        return ngxFrontend->InitD3D11(InApplicationId, InApplicationDataPath, InDevice, InFeatureInfo, InSDKVersion);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Init, (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D12Device* InDevice,
        NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)) {
        return ngxFrontend->InitD3D12(InApplicationId, InApplicationDataPath, InDevice, InFeatureInfo, InSDKVersion);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_Init_Ext,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
            ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return ngxFrontend->InitD3D11Ext(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_Init_ProjectID,
        (const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
            const wchar_t* InApplicationDataPath, ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return ngxFrontend->InitD3D11ProjectId(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Init_ProjectID,
        (const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
            const wchar_t* InApplicationDataPath, ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return ngxFrontend->InitD3D12ProjectId(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Shutdown, ()) {
        return ngxFrontend->ShutdownD3D12();
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Shutdown, ()) {
        return ngxFrontend->ShutdownVulkan();
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Shutdown1, (::VkDevice InDevice)) {
        return ngxFrontend->ShutdownVulkan_1(InDevice);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_Shutdown, ()) {
        return ngxFrontend->ShutdownD3D11();
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Shutdown1, (ID3D12Device* D3DDevice)) {
        return ngxFrontend->ShutdownD3D12_1(D3DDevice);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_Shutdown1, (ID3D11Device* D3DDevice)) {
        return ngxFrontend->ShutdownD3D11_1(D3DDevice);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_CreateFeature, (ID3D11DeviceContext* InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)) {
        return ngxFrontend->CreateD3D11(InCmdList, InFeatureID, InParameters, OutHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_CreateFeature, (ID3D12GraphicsCommandList* InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)) {
        return ngxFrontend->CreateD3D12(InCmdList, InFeatureID, InParameters, OutHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_EvaluateFeature,
        (ID3D12GraphicsCommandList* InCmdList, const NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
            PFN_NVSDK_NGX_ProgressCallback InCallback)) {
        return ngxFrontend->EvaluateD3D12(InCmdList, InFeatureHandle, InParameters, InCallback);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements, (
        const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, uint32_t* OutExtensionCount,
        VkExtensionProperties** OutExtensionProperties)) {
        return ngxFrontend->GetFeatureInstanceExtensionRequirementsVulkan(FeatureDiscoveryInfo, OutExtensionCount, OutExtensionProperties);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_EvaluateFeature,
        (VkCommandBuffer InCmdList, const NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
            PFN_NVSDK_NGX_ProgressCallback InCallback)) {
        return ngxFrontend->EvaluateVulkan(InCmdList, InFeatureHandle, InParameters, InCallback);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_GetCapabilityParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->GetCapabilityParametersD3D12(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_GetCapabilityParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->GetCapabilityParametersVulkan(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_GetParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->GetParametersVulkan(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_AllocateParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->AllocateParametersVulkan(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_DestroyParameters, (NVSDK_NGX_Parameter* InParameters)) {
        return ngxFrontend->DestroyParametersVulkan(InParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_GetCapabilityParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->GetCapabilityParametersD3D11(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_GetParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->GetParametersD3D12(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_GetParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->GetParametersD3D11(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_AllocateParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->AllocateParametersD3D11(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_AllocateParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        return ngxFrontend->AllocateParametersD3D12(OutParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_DestroyParameters, (NVSDK_NGX_Parameter* InParameters)) {
        return ngxFrontend->DestroyParametersD3D12(InParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_DestroyParameters, (NVSDK_NGX_Parameter* InParameters)) {
        return ngxFrontend->DestroyParametersD3D11(InParameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_EvaluateFeature,
        (ID3D11DeviceContext* InCmdList, const NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
            PFN_NVSDK_NGX_ProgressCallback InCallback)) {
        return ngxFrontend->EvaluateD3D11(InCmdList, InFeatureHandle, InParameters, InCallback);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_GetScratchBufferSize, (NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)) {
        return ngxFrontend->GetScratchBufferSizeD3D11(InFeatureId, InParameters, OutSizeInBytes);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_GetScratchBufferSize, (NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)) {
        return ngxFrontend->GetScratchBufferSizeD3D12(InFeatureId, InParameters, OutSizeInBytes);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_GetScratchBufferSize, (NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)) {
        return ngxFrontend->GetScratchBufferSizeVulkan(InFeatureId, InParameters, OutSizeInBytes);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_ReleaseFeature, (NVSDK_NGX_Handle* InstanceHandle)) {
        return ngxFrontend->ReleaseD3D11(InstanceHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_ReleaseFeature, (NVSDK_NGX_Handle* InstanceHandle)) {
        return ngxFrontend->ReleaseD3D12(InstanceHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_ReleaseFeature, (NVSDK_NGX_Handle* InstanceHandle)) {
        return ngxFrontend->ReleaseVulkan(InstanceHandle);
    }

    struct HookNgxFrontend : IHook {
        const std::wstring Name() const override { return L"NGX Frontend hooks"; }
        HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
        int         Priority() const override { return 50; }

        bool CanInstall(Context& ctx, IDetourApi& api) override {
            return
                api.GetModHandleW(L"nvngx.dll") != nullptr && !ctx.ngx.isRealNgxPresent || api.GetModHandleW(L"_nvngx.dll");
        }

        bool Install(Context& ctx, IDetourApi& api) override {
            static bool isHooked = false;
            if (isHooked) {
                return false;
            }

            isHooked = true;
            LOG_INFO(L"[NVNGX] Initializing NGX Frontend");

            static DefaultLogger globalLogger;
            static DefaultBackendLoader globalNgxLoader;

            auto ngxBackends = std::make_unique<BackendManager>(globalNgxLoader, globalLogger);
            IProcResolver& resolver = GetProcResolver();
            ngxFrontend = std::make_unique<NGX::NgxFrontend>(*ngxBackends, globalLogger, ngxRuntimeState, resolver);



            auto _ngx = api.GetModHandleW(L"_nvngx.dll");
            auto __ngx = api.GetModHandleW(L"_nvngx.dll");
            auto ngx = _ngx ? _ngx : __ngx;
            ngx = Common::GetModuleHandle();

            DetourTxn txn(api);

            auto ATTACH = [&](auto& org, auto proxy, const char* name) -> bool {
                using FnPtr = std::decay_t<decltype(org)>;
                org = reinterpret_cast<FnPtr>(api.GetProc(ngx, name));
                if (org) {
                    if (!txn.attach(reinterpret_cast<void**>(&org), reinterpret_cast<void*>(proxy), name)) {
                        LOG_ERROR(L"[NVNGX] NVNGX Attach failed");
                        return false;
                    }
                }
                else {
                    LOG_ERROR(L"[NVNGX] NVNGX GetProc failed");
                }
                auto name2 = std::string(name);
                auto name3 = std::wstring(name2.begin(), name2.end());
                LOG_TRACE(L"[NVNGX] NVNGX Attach applied:" + name3);
                return true;
                };

            /*
            // Commons
            // VULKAN
            NGX_ATTACH(NVSDK_NGX_VULKAN_GetFeatureRequirements);
            NGX_ATTACH(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements);
            NGX_ATTACH(NVSDK_NGX_VULKAN_GetScratchBufferSize);

            NGX_ATTACH(NVSDK_NGX_VULKAN_Init);
            NGX_ATTACH(NVSDK_NGX_VULKAN_Init_Ext);
            NGX_ATTACH(NVSDK_NGX_VULKAN_Init_Ext2);
            NGX_ATTACH(NVSDK_NGX_VULKAN_CreateFeature);
            NGX_ATTACH(NVSDK_NGX_VULKAN_CreateFeature1);
            NGX_ATTACH(NVSDK_NGX_VULKAN_EvaluateFeature);
            NGX_ATTACH(NVSDK_NGX_VULKAN_ReleaseFeature);
            NGX_ATTACH(NVSDK_NGX_VULKAN_Shutdown1);
            NGX_ATTACH(NVSDK_NGX_VULKAN_Shutdown);

            NGX_ATTACH(NVSDK_NGX_VULKAN_GetCapabilityParameters);
            NGX_ATTACH(NVSDK_NGX_VULKAN_GetParameters);
            NGX_ATTACH(NVSDK_NGX_VULKAN_AllocateParameters);
            NGX_ATTACH(NVSDK_NGX_VULKAN_DestroyParameters);

            // DX11
            NGX_ATTACH(NVSDK_NGX_D3D11_GetFeatureRequirements);
            NGX_ATTACH(NVSDK_NGX_D3D11_GetScratchBufferSize);

            NGX_ATTACH(NVSDK_NGX_D3D11_Init);
            NGX_ATTACH(NVSDK_NGX_D3D11_Init_Ext);
            NGX_ATTACH(NVSDK_NGX_D3D11_Init_ProjectID);
            NGX_ATTACH(NVSDK_NGX_D3D11_Shutdown1);
            NGX_ATTACH(NVSDK_NGX_D3D11_Shutdown);
            NGX_ATTACH(NVSDK_NGX_D3D11_CreateFeature);
            NGX_ATTACH(NVSDK_NGX_D3D11_EvaluateFeature);
            NGX_ATTACH(NVSDK_NGX_D3D11_ReleaseFeature);

            NGX_ATTACH(NVSDK_NGX_D3D11_GetCapabilityParameters);
            NGX_ATTACH(NVSDK_NGX_D3D11_GetParameters);
            NGX_ATTACH(NVSDK_NGX_D3D11_AllocateParameters);
            NGX_ATTACH(NVSDK_NGX_D3D11_DestroyParameters);

            // DX12
            NGX_ATTACH(NVSDK_NGX_D3D12_GetFeatureRequirements);
            NGX_ATTACH(NVSDK_NGX_D3D12_GetScratchBufferSize);

            NGX_ATTACH(NVSDK_NGX_D3D12_Init);
            NGX_ATTACH(NVSDK_NGX_D3D12_Init_Ext);
            NGX_ATTACH(NVSDK_NGX_D3D11_Init_ProjectID);
            NGX_ATTACH(NVSDK_NGX_D3D12_Shutdown1);
            NGX_ATTACH(NVSDK_NGX_D3D12_Shutdown);
            NGX_ATTACH(NVSDK_NGX_D3D12_CreateFeature);
            NGX_ATTACH(NVSDK_NGX_D3D12_EvaluateFeature);
            NGX_ATTACH(NVSDK_NGX_D3D12_ReleaseFeature);

            NGX_ATTACH(NVSDK_NGX_D3D12_GetCapabilityParameters);
            NGX_ATTACH(NVSDK_NGX_D3D12_GetParameters);
            NGX_ATTACH(NVSDK_NGX_D3D12_AllocateParameters);
            NGX_ATTACH(NVSDK_NGX_D3D12_DestroyParameters);
            */

            NGX_ALIAS(NVSDK_NGX_VULKAN_GetFeatureRequirements);
            NGX_ALIAS(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements);
            NGX_ALIAS(NVSDK_NGX_VULKAN_GetScratchBufferSize);

            NGX_ALIAS(NVSDK_NGX_VULKAN_Init);
            NGX_ALIAS(NVSDK_NGX_VULKAN_Init_Ext);
            NGX_ALIAS(NVSDK_NGX_VULKAN_Init_Ext2);
            NGX_ALIAS(NVSDK_NGX_VULKAN_CreateFeature);
            NGX_ALIAS(NVSDK_NGX_VULKAN_CreateFeature1);
            NGX_ALIAS(NVSDK_NGX_VULKAN_EvaluateFeature);
            NGX_ALIAS(NVSDK_NGX_VULKAN_ReleaseFeature);
            NGX_ALIAS(NVSDK_NGX_VULKAN_Shutdown1);
            NGX_ALIAS(NVSDK_NGX_VULKAN_Shutdown);

            NGX_ALIAS(NVSDK_NGX_VULKAN_GetCapabilityParameters);
            NGX_ALIAS(NVSDK_NGX_VULKAN_GetParameters);
            NGX_ALIAS(NVSDK_NGX_VULKAN_AllocateParameters);
            NGX_ALIAS(NVSDK_NGX_VULKAN_DestroyParameters);

            // DX11
            NGX_ALIAS(NVSDK_NGX_D3D11_GetFeatureRequirements);
            NGX_ALIAS(NVSDK_NGX_D3D11_GetScratchBufferSize);

            NGX_ALIAS(NVSDK_NGX_D3D11_Init);
            NGX_ALIAS(NVSDK_NGX_D3D11_Init_Ext);
            NGX_ALIAS(NVSDK_NGX_D3D11_Init_ProjectID);
            NGX_ALIAS(NVSDK_NGX_D3D11_Shutdown1);
            NGX_ALIAS(NVSDK_NGX_D3D11_Shutdown);
            NGX_ALIAS(NVSDK_NGX_D3D11_CreateFeature);
            NGX_ALIAS(NVSDK_NGX_D3D11_EvaluateFeature);
            NGX_ALIAS(NVSDK_NGX_D3D11_ReleaseFeature);

            NGX_ALIAS(NVSDK_NGX_D3D11_GetCapabilityParameters);
            NGX_ALIAS(NVSDK_NGX_D3D11_GetParameters);
            NGX_ALIAS(NVSDK_NGX_D3D11_AllocateParameters);
            NGX_ALIAS(NVSDK_NGX_D3D11_DestroyParameters);

            // DX12
            NGX_ALIAS(NVSDK_NGX_D3D12_GetFeatureRequirements);
            NGX_ALIAS(NVSDK_NGX_D3D12_GetScratchBufferSize);

            NGX_ALIAS(NVSDK_NGX_D3D12_Init);
            NGX_ALIAS(NVSDK_NGX_D3D12_Init_Ext);
            NGX_ALIAS(NVSDK_NGX_D3D11_Init_ProjectID);
            NGX_ALIAS(NVSDK_NGX_D3D12_Shutdown1);
            NGX_ALIAS(NVSDK_NGX_D3D12_Shutdown);
            NGX_ALIAS(NVSDK_NGX_D3D12_CreateFeature);
            NGX_ALIAS(NVSDK_NGX_D3D12_EvaluateFeature);
            NGX_ALIAS(NVSDK_NGX_D3D12_ReleaseFeature);

            NGX_ALIAS(NVSDK_NGX_D3D12_GetCapabilityParameters);
            NGX_ALIAS(NVSDK_NGX_D3D12_GetParameters);
            NGX_ALIAS(NVSDK_NGX_D3D12_AllocateParameters);
            NGX_ALIAS(NVSDK_NGX_D3D12_DestroyParameters);

            //LOG_INFO(L"[NVNGX] NVNGX attaching");
            if (!txn.commit())
                return false;

            LOG_INFO(L"[NVNGX] NVNGX Frontend initialized");
            return true;
        }

        void Uninstall(Context& ctx, IDetourApi& api) override {
            // Intentionally a no-op.
            //
            // Despite the presence of a DetourTxn in Install(), this hook does
            // not actually install any Detours trampolines at runtime - all
            // NGX_ATTACH calls are commented out; only NGX_ALIAS is used, which
            // registers pointers in ProcAliasRegistry. There is therefore
            // nothing to DetourDetach here.
            //
            // We also deliberately do NOT:
            //   1) Call ProcAliasRegistry::Clear() - the registry has no mutex
            //      and a concurrent TryResolve() from a game thread that is
            //      mid-way through a hooked GetProcAddress call would race it.
            //   2) Destroy the global ngxFrontend unique_ptr - the game may
            //      already have cached proxy function pointers (returned from
            //      earlier GetProcAddress calls) in its IAT; those pointers
            //      refer into ngxFrontend's members, so destroying it would
            //      leave dangling references that crash on the next NGX call.
            //
            // Process exit handles both resources correctly:
            //   - the static ngxFrontend is destroyed during DLL unload,
            //   - ProcAliasRegistry's fixed-size array has trivial destruction.
            //
            // If a real teardown is ever needed (e.g. for hot-reload of the
            // DLL without restarting the game), it requires first:
            //   - adding a mutex to ProcAliasRegistry,
            //   - ensuring no game-side IAT entry points into ngxFrontend
            //     members (e.g. by making proxy dispatchers stateless
            //     singletons), and
            //   - fixing the dangling ngxBackends reference in Install()
            //     (local unique_ptr passed by reference to NgxFrontend that
            //     outlives its owner).
        }
    };
}