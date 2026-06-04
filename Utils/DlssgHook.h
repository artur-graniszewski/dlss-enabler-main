#pragma once
#include "Hook.h"
#include "Common.h"
#include "DetourTxn.h"
#include "DlssgProxy.h"
#include "NgxHookHelpers.h"
#include "NgxProvider.h"
#include "DlssgLazyHook.h"

extern std::unique_ptr<DLSSG::DlssgProxy> dlssgModule;
extern std::unique_ptr<NGX::NgxProvider> ngxProvider;
extern std::unique_ptr<BackendManager> ngxBackends;

namespace DLSSG
{
    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_GetFeatureRequirements, (
        IDXGIAdapter* Adapter,
        NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)) {
        return dlssgModule->GetFeatureRequirementsD3D12(Adapter, FeatureDiscoveryInfo, RequirementInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_GetGPUArchitecture, ()) {
        return dlssgModule->GetGpuArchitecture();
    }

    NGX_MAKE_PROXY(NVSDK_NGX_GetDriverVersionEx, (uint32_t* Versions, uint32_t InputVersionCount, uint32_t* TotalDriverVersionCount)) {
        return dlssgModule->GetDriverVersionEx(Versions, InputVersionCount, TotalDriverVersionCount);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_CreateFeature, (ID3D12GraphicsCommandList* InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)) {
        return dlssgModule->CreateD3D12(InCmdList, InFeatureID, InParameters, OutHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_CreateFeature, (void* InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)) {
        return dlssgModule->CreateVulkan(InCmdList, InFeatureID, InParameters, OutHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_CreateFeature1, (const VkDevice InDevice, void* InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)) {
        return dlssgModule->CreateVulkan1(InDevice, InCmdList, InFeatureID, InParameters, OutHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_ReleaseFeature, (NVSDK_NGX_Handle* InstanceHandle)) {
        return dlssgModule->ReleaseD3D12(InstanceHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_ReleaseFeature, (NVSDK_NGX_Handle* InstanceHandle)) {
        return dlssgModule->ReleaseVulkan(InstanceHandle);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_PopulateParameters_Impl, (NVSDK_NGX_Parameter* Parameters)) {
        return dlssgModule->PopulateParametersD3D12(Parameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_PopulateParameters_Impl, (NVSDK_NGX_Parameter* Parameters)) {
        return dlssgModule->PopulateParametersVulkan(Parameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_EvaluateFeature,
        (ID3D12GraphicsCommandList* InCmdList, const NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
            PFN_NVSDK_NGX_ProgressCallback InCallback)) {
        return dlssgModule->EvaluateD3D12(InCmdList, InFeatureHandle, InParameters, InCallback);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_EvaluateFeature,
        (void* InCmdList, const NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
            PFN_NVSDK_NGX_ProgressCallback InCallback)) {
        return dlssgModule->EvaluateVulkan(InCmdList, InFeatureHandle, InParameters, InCallback);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_GetScratchBufferSize, (NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)) {
        return dlssgModule->GetScratchBufferSizeD3D12(InFeatureId, InParameters, OutSizeInBytes);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_GetScratchBufferSize, (NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)) {
        return dlssgModule->GetScratchBufferSizeVulkan(InFeatureId, InParameters, OutSizeInBytes);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_GetAPIVersion, ()) {
        return dlssgModule->GetApiVersion();
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Init,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
            ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion)) {
        return dlssgModule->InitD3D12(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Init_Ext,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
            ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_Parameter* Parameters)) {
        return dlssgModule->InitD3D12Ext(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion, Parameters);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Init_Ext,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, void* InDevice, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return dlssgModule->InitVulkanExt(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Init_Ext2,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, void* InDevice, void* InGIPA, void* InGDPA, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)) {
        return dlssgModule->InitVulkanExt2(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InSDKVersion, InFeatureInfo);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Init,
        (unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, void* InDevice, void* InGIPA, void* InGDPA, const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)) {
        return dlssgModule->InitVulkan(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InFeatureInfo, InSDKVersion);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Shutdown1, (ID3D12Device* D3DDevice)) {
        return dlssgModule->ShutdownD3D12_1(D3DDevice);
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_Shutdown, ()) {
        return dlssgModule->ShutdownD3D12();
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Shutdown, ()) {
        return dlssgModule->ShutdownVulkan();
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_Shutdown1, (void* LogicalDevice)) {
        return dlssgModule->ShutdownVulkan_1(LogicalDevice);
    }

    // NGX addon
    NGX_MAKE_PROXY(NVSDK_NGX_D3D11_GetCapabilityParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        auto result = org_NVSDK_NGX_D3D11_GetCapabilityParameters(OutParameters);
        dlssgModule->PopulateParametersD3D12(*OutParameters);
        return result;
    }

    NGX_MAKE_PROXY(NVSDK_NGX_D3D12_GetCapabilityParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        auto result = org_NVSDK_NGX_D3D12_GetCapabilityParameters(OutParameters);
        dlssgModule->PopulateParametersD3D12(*OutParameters);
        return result;
    }

    NGX_MAKE_PROXY(NVSDK_NGX_VULKAN_GetCapabilityParameters, (NVSDK_NGX_Parameter** OutParameters)) {
        auto result = org_NVSDK_NGX_VULKAN_GetCapabilityParameters(OutParameters);
        dlssgModule->PopulateParametersVulkan(*OutParameters);
        return result;
    }

    inline void RegisterDlssgProxiesForAllInstances()
    {
        auto& registry = ProcAliasRegistry::Instance();

#define REG_DLSSG(name) \
        registry.RegisterAlias(L"nvngx_dlssg.dll", #name, \
            reinterpret_cast<ProcAliasRegistry::FuncPtr>(&proxy_##name))

        REG_DLSSG(NVSDK_NGX_GetGPUArchitecture);
        REG_DLSSG(NVSDK_NGX_GetDriverVersionEx);
        REG_DLSSG(NVSDK_NGX_GetAPIVersion);
        REG_DLSSG(NVSDK_NGX_D3D12_CreateFeature);
        REG_DLSSG(NVSDK_NGX_D3D12_ReleaseFeature);
        REG_DLSSG(NVSDK_NGX_D3D12_EvaluateFeature);
        REG_DLSSG(NVSDK_NGX_D3D12_PopulateParameters_Impl);
        REG_DLSSG(NVSDK_NGX_D3D12_GetScratchBufferSize);
        REG_DLSSG(NVSDK_NGX_D3D12_Init);
        REG_DLSSG(NVSDK_NGX_D3D12_Init_Ext);
        REG_DLSSG(NVSDK_NGX_D3D12_Shutdown);
        REG_DLSSG(NVSDK_NGX_D3D12_Shutdown1);
        REG_DLSSG(NVSDK_NGX_D3D12_GetFeatureRequirements);
        REG_DLSSG(NVSDK_NGX_VULKAN_CreateFeature);
        REG_DLSSG(NVSDK_NGX_VULKAN_CreateFeature1);
        REG_DLSSG(NVSDK_NGX_VULKAN_ReleaseFeature);
        REG_DLSSG(NVSDK_NGX_VULKAN_EvaluateFeature);
        REG_DLSSG(NVSDK_NGX_VULKAN_PopulateParameters_Impl);
        REG_DLSSG(NVSDK_NGX_VULKAN_GetScratchBufferSize);
        REG_DLSSG(NVSDK_NGX_VULKAN_Init);
        REG_DLSSG(NVSDK_NGX_VULKAN_Init_Ext);
        REG_DLSSG(NVSDK_NGX_VULKAN_Init_Ext2);
        REG_DLSSG(NVSDK_NGX_VULKAN_Shutdown);
        REG_DLSSG(NVSDK_NGX_VULKAN_Shutdown1);

#undef REG_DLSSG
    }

    struct HookDlssg : IHook {
        const std::wstring Name() const override { return L"DLSSG hooks"; }
        HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
        int         Priority() const override { return 50; }

        bool CanInstall(Context& ctx, IDetourApi& api) override {
            return
                api.GetModHandleW(L"nvngx_dlssg.dll") != nullptr && ctx.nvapi.isRealHardwareDetected;
        }

        bool Install(Context& ctx, IDetourApi& api) override {
            if (ctx.ngx.isDlssgSupportedByHardware && !ctx.ngx.isDlssgDisabled) {
                return true;
            }
            static auto lastDlssg = api.GetModHandleW(L"nvngx_dlssg.dll");
            auto dlssg = api.GetModHandleW(L"nvngx_dlssg.dll");

            if (lastDlssg != nullptr && lastDlssg != dlssg) {
                LOG_ERROR(L"[NVNGX] DLSSG - new instance found");
            }
            lastDlssg = dlssg;
            static bool isHooked = false;
            if (isHooked) {
                return false;
            }

            LOG_INFO(L"[NVNGX] DLSSG initializing");

            InitializeDlssgHooks();

            isHooked = true;

            DetourTxn txn(api);

            auto ATTACH = [&](auto& org, auto proxy, const char* name) -> bool {
                using FnPtr = std::decay_t<decltype(org)>;
                org = reinterpret_cast<FnPtr>(api.GetProc(dlssg, name));

                if (org) {
                    if (!txn.attach(reinterpret_cast<void**>(&org), reinterpret_cast<void*>(proxy), name)) {
                        LOG_ERROR(L"[NVNGX] DLSSG Attach failed");
                        return false;
                    }
                }
                else {
                    LOG_ERROR(L"[NVNGX] DLSSG GetProc failed");
                }
                auto name2 = std::string(name);
                auto name3 = std::wstring(name2.begin(), name2.end());
                LOG_TRACE(L"[NVNGX] DLSSG Attach applied:" + name3);
                return true;
                };


            if (!ctx.ngx.isRealNgxPresent && api.GetModHandleW(L"nvngx!!!.dll")) {
                LOG_INFO(L"[NVNGX] DLSSG Attaching to GetCapabilityParameters");
                // detour Optiscaler function to enable FG on non-AMD platforms
                using FnPtr = std::decay_t<decltype(org_NVSDK_NGX_D3D12_GetCapabilityParameters)>;
                org_NVSDK_NGX_D3D12_GetCapabilityParameters = reinterpret_cast<FnPtr>(api.GetProc(Common::GetModuleHandle(), "NVSDK_NGX_D3D12_GetCapabilityParameters"));
                if (org_NVSDK_NGX_D3D12_GetCapabilityParameters) {
                    if (!txn.attach(reinterpret_cast<void**>(&org_NVSDK_NGX_D3D12_GetCapabilityParameters), reinterpret_cast<void*>(proxy_NVSDK_NGX_D3D12_GetCapabilityParameters), "NGX_D3D12_GetCapabilityParameters")) {
                        LOG_ERROR(L"[NVNGX] DLSSG Attach failed for GetCapabilityParameters");
                    }
                    else {
                        LOG_TRACE(L"[NVNGX] DLSSG Attach succeeded for GetCapabilityParameters");
                    }
                }

                using FnPtr = std::decay_t<decltype(org_NVSDK_NGX_D3D11_GetCapabilityParameters)>;
                org_NVSDK_NGX_D3D11_GetCapabilityParameters = reinterpret_cast<FnPtr>(api.GetProc(Common::GetModuleHandle(), "NVSDK_NGX_D3D11_GetCapabilityParameters"));
                if (org_NVSDK_NGX_D3D11_GetCapabilityParameters) {
                    if (!txn.attach(reinterpret_cast<void**>(&org_NVSDK_NGX_D3D11_GetCapabilityParameters), reinterpret_cast<void*>(proxy_NVSDK_NGX_D3D11_GetCapabilityParameters), "NGX_D3D11_GetCapabilityParameters")) {
                        LOG_ERROR(L"[NVNGX] DLSSG Attach failed for GetCapabilityParameters");
                    }
                    else {
                        LOG_TRACE(L"[NVNGX] DLSSG Attach succeeded for GetCapabilityParameters");
                    }
                }

                using FnPtr = std::decay_t<decltype(org_NVSDK_NGX_VULKAN_GetCapabilityParameters)>;
                org_NVSDK_NGX_VULKAN_GetCapabilityParameters = reinterpret_cast<FnPtr>(api.GetProc(Common::GetModuleHandle(), "NVSDK_NGX_VULKAN_GetCapabilityParameters"));
                if (org_NVSDK_NGX_VULKAN_GetCapabilityParameters) {
                    if (!txn.attach(reinterpret_cast<void**>(&org_NVSDK_NGX_VULKAN_GetCapabilityParameters), reinterpret_cast<void*>(proxy_NVSDK_NGX_VULKAN_GetCapabilityParameters), "NGX_VULKAN_GetCapabilityParameters")) {
                        LOG_ERROR(L"[NVNGX] DLSSG Attach failed for GetCapabilityParameters");
                    }
                    else {
                        LOG_TRACE(L"[NVNGX] DLSSG Attach succeeded for GetCapabilityParameters");
                    }
                }
            }

            //NGX_ATTACH(NVSDK_NGX_GetGPUArchitecture);
            //NGX_ATTACH(NVSDK_NGX_GetDriverVersionEx);
            //NGX_ATTACH(NVSDK_NGX_D3D12_CreateFeature);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_CreateFeature);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_CreateFeature1);
            //NGX_ATTACH(NVSDK_NGX_D3D12_ReleaseFeature);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_ReleaseFeature);
            //NGX_ATTACH(NVSDK_NGX_D3D12_PopulateParameters_Impl);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_PopulateParameters_Impl);
            //NGX_ATTACH(NVSDK_NGX_D3D12_EvaluateFeature);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_EvaluateFeature);
            //NGX_ATTACH(NVSDK_NGX_D3D12_GetScratchBufferSize);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_GetScratchBufferSize);
            //NGX_ATTACH(NVSDK_NGX_GetAPIVersion);
            //NGX_ATTACH(NVSDK_NGX_D3D12_Init);
            //NGX_ATTACH(NVSDK_NGX_D3D12_Init_Ext);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_Init_Ext);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_Init_Ext2);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_Init);
            //NGX_ATTACH(NVSDK_NGX_D3D12_Shutdown1);
            //NGX_ATTACH(NVSDK_NGX_D3D12_Shutdown);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_Shutdown);
            //NGX_ATTACH(NVSDK_NGX_VULKAN_Shutdown1);
            //NGX_ATTACH(NVSDK_NGX_D3D12_GetFeatureRequirements);
            RegisterDlssgProxiesForAllInstances();

            LOG_INFO(L"[NVNGX] DLSSG attaching");
            if (!txn.commit())
                return false;

            LOG_INFO(L"[NVNGX] DLSSG initialized");
            return true;
        }
    };
}