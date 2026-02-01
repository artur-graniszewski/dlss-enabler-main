#pragma once

#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <functional>
#include "../Includes/dlss/nvsdk_ngx.h"
#include "../Includes/dlss/nvsdk_ngx_defs.h"
#include "ProcResolver.h"     // IProcResolver
#include "NgxState.h"         // NgxRuntimeState
#include "INgxBackend.h"      // INgxLogger (interface)
#include <vulkan/vulkan_core.h>

namespace NGX
{
    class NgxProvider {
    public:
        NgxProvider(
            INgxLogger& logger,
            NgxRuntimeState& state,
            IProcResolver& resolver)
            : logger(logger), state(state), resolver(resolver) {
        }

        // Commons


        // ===== VULKAN =====
        NVSDK_NGX_Result GetFeatureInstanceExtensionRequirementsVulkan(const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, uint32_t* OutExtensionCount,
            VkExtensionProperties** OutExtensionProperties
        );
        NVSDK_NGX_Result GetFeatureRequirementsVulkan(
            const VkInstance instance,
            const VkPhysicalDevice device,
            NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo);

        NVSDK_NGX_Result GetCapabilityParametersVulkan(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result GetParametersVulkan(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result AllocateParametersVulkan(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result DestroyParametersVulkan(NVSDK_NGX_Parameter* InParameters);

        NVSDK_NGX_Result GetScratchBufferSizeVulkan(
            NVSDK_NGX_Feature featureId,
            const NVSDK_NGX_Parameter* parameters,
            size_t* outSizeInBytes);

        NVSDK_NGX_Result InitVulkan(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            VkInstance instance,                     
            VkPhysicalDevice physicalDevice,              
            VkDevice device,
            PFN_vkGetInstanceProcAddr getInstanceProcAddr,
            PFN_vkGetDeviceProcAddr getDeviceProcAddr,
            NVSDK_NGX_FeatureCommonInfo* featureInfo, 
            NVSDK_NGX_Version sdkVersion);

        NVSDK_NGX_Result InitVulkanExt(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            VkInstance instance,                  
            VkPhysicalDevice physicalDevice,                
            VkDevice device,                        
            NVSDK_NGX_Version sdkVersion,
            NVSDK_NGX_FeatureCommonInfo* featureInfo);

        NVSDK_NGX_Result InitVulkanExt2(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
            VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice,
            PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA,
            NVSDK_NGX_Version InSDKVersion,
            NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);

        NVSDK_NGX_Result ShutdownVulkan();
        NVSDK_NGX_Result ShutdownVulkan_1(::VkDevice InDevice);

        NVSDK_NGX_Result CreateVulkan(
            VkCommandBuffer CommandListr,
            NVSDK_NGX_Feature featureId,
            NVSDK_NGX_Parameter* parameters,
            NVSDK_NGX_Handle** outHandle);

        NVSDK_NGX_Result CreateVulkan1(
            const VkDevice device,
            VkCommandBuffer CommandList,
            NVSDK_NGX_Feature featureId,
            NVSDK_NGX_Parameter* parameters,
            NVSDK_NGX_Handle** outHandle);

        NVSDK_NGX_Result EvaluateVulkan(
            VkCommandBuffer cmdBuffer,
            const NVSDK_NGX_Handle* featureHandle,
            NVSDK_NGX_Parameter* parameters,
            PFN_NVSDK_NGX_ProgressCallback callback);

        NVSDK_NGX_Result ReleaseVulkan(NVSDK_NGX_Handle* instanceHandle);

        // ===== D3D11 =====
        NVSDK_NGX_Result GetScratchBufferSizeD3D11(
            NVSDK_NGX_Feature featureId,
            const NVSDK_NGX_Parameter* parameters,
            size_t* outSizeInBytes);

        NVSDK_NGX_Result GetFeatureRequirementsD3D11(IDXGIAdapter* Adapter,
            NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo);

        NVSDK_NGX_Result GetCapabilityParametersD3D11(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result GetParametersD3D11(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result AllocateParametersD3D11(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result DestroyParametersD3D11(NVSDK_NGX_Parameter* InParameters);

        NVSDK_NGX_Result InitD3D11(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D11Device* InDevice,
            NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
        NVSDK_NGX_Result InitD3D11Ext(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            ID3D11Device* device,
            NVSDK_NGX_Version sdkVersion,
            NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
        NVSDK_NGX_Result InitD3D11ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
            const wchar_t* InApplicationDataPath, ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);

        NVSDK_NGX_Result ShutdownD3D11_1(ID3D11Device *device);
        NVSDK_NGX_Result ShutdownD3D11();


        NVSDK_NGX_Result CreateD3D11(
            ID3D11DeviceContext* cmdList,
            NVSDK_NGX_Feature featureId,
            NVSDK_NGX_Parameter* parameters,
            NVSDK_NGX_Handle** outHandle);

        NVSDK_NGX_Result EvaluateD3D11(
            ID3D11DeviceContext* cmdList,
            const NVSDK_NGX_Handle* featureHandle,
            const NVSDK_NGX_Parameter* parameters,
            PFN_NVSDK_NGX_ProgressCallback callback);

        NVSDK_NGX_Result ReleaseD3D11(NVSDK_NGX_Handle* instanceHandle);

        // ===== D3D12 =====
        NVSDK_NGX_Result GetFeatureRequirementsD3D12(IDXGIAdapter* Adapter,
            NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo);

        NVSDK_NGX_Result GetCapabilityParametersD3D12(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result GetParametersD3D12(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result AllocateParametersD3D12(NVSDK_NGX_Parameter** OutParameters);
        NVSDK_NGX_Result DestroyParametersD3D12(NVSDK_NGX_Parameter* InParameters);

        NVSDK_NGX_Result GetScratchBufferSizeD3D12(
            NVSDK_NGX_Feature featureId,
            const NVSDK_NGX_Parameter* parameters,
            size_t* outSizeInBytes);

        NVSDK_NGX_Result InitD3D12(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D12Device* InDevice,
            NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
        NVSDK_NGX_Result InitD3D12Ext(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            ID3D12Device* device,
            NVSDK_NGX_Version sdkVersion,
            NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
        NVSDK_NGX_Result InitD3D12ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
            const wchar_t* InApplicationDataPath, ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);

        NVSDK_NGX_Result ShutdownD3D12_1(ID3D12Device *device);
        NVSDK_NGX_Result ShutdownD3D12();

        NVSDK_NGX_Result CreateD3D12(
            ID3D12GraphicsCommandList* cmdList,
            NVSDK_NGX_Feature featureId,
            NVSDK_NGX_Parameter* parameters,
            NVSDK_NGX_Handle** outHandle);

        NVSDK_NGX_Result EvaluateD3D12(
            ID3D12GraphicsCommandList* cmdList,
            const NVSDK_NGX_Handle* featureHandle,
            NVSDK_NGX_Parameter* parameters,
            PFN_NVSDK_NGX_ProgressCallback callback);
        NVSDK_NGX_Result ReleaseD3D12(NVSDK_NGX_Handle* featureHandle);

    private:
        // Logging helpers (prefix with exact NVSDK entry name)
        void LogInfo(const wchar_t* entry, const std::wstring& message);
        void LogWarning(const wchar_t* entry, const std::wstring& message);
        void LogError(const wchar_t* entry, const std::wstring& message);
        void LogNoBackend(const wchar_t* entry);

        NVSDK_NGX_Feature GetFeatureByHandleId(const NVSDK_NGX_Handle *inHandleId);

    private:
        static constexpr wchar_t kModule[] = L"NGX";
        INgxLogger& logger;
        NgxRuntimeState& state;
        IProcResolver& resolver;
        std::unordered_map<const NVSDK_NGX_Handle*, NVSDK_NGX_Feature, NgxPointerHash> handleRegistry;
    };
}