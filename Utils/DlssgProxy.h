#pragma once

#include <d3d12.h>
#include <string>
#include "../Includes/dlss/nvsdk_ngx.h"
#include "ProcResolver.h"     // IProcResolver
#include "NgxState.h"         // NgxRuntimeState
#include "INgxBackend.h"      // INgxLogger (interface)
#include "BackendManager.h"   // BackendManager
#include <vulkan/vulkan_core.h>

namespace DLSSG
{
    class DlssgProxy {
    public:
        DlssgProxy(BackendManager& backends,
            INgxLogger& logger,
            NgxRuntimeState& state,
            IProcResolver& resolver)
            : backends(backends), logger(logger), state(state), resolver(resolver) {
        }

        // ===== D3D12 =====
        NVSDK_NGX_Result InitD3D12(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            ID3D12Device* device,
            NVSDK_NGX_Version sdkVersion);

        NVSDK_NGX_Result InitD3D12Ext(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            ID3D12Device* device,
            NVSDK_NGX_Version sdkVersion,
            const NVSDK_NGX_Parameter* parameters);

        NVSDK_NGX_Result ShutdownD3D12();
        NVSDK_NGX_Result ShutdownVulkan();
        NVSDK_NGX_Result ShutdownVulkan_1(void* device);
        NVSDK_NGX_Result ShutdownD3D12_1(ID3D12Device* device);

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

        NVSDK_NGX_Result ReleaseD3D12(NVSDK_NGX_Handle* instanceHandle);

        NVSDK_NGX_Result PopulateParametersD3D12(NVSDK_NGX_Parameter* parameters);
        NVSDK_NGX_Result GetScratchBufferSizeD3D12(
            NVSDK_NGX_Feature featureId,
            const NVSDK_NGX_Parameter* parameters,
            size_t* outSizeInBytes);

        NVSDK_NGX_Result GetScratchBufferSizeVulkan(
            NVSDK_NGX_Feature featureId,
            const NVSDK_NGX_Parameter* parameters,
            size_t* outSizeInBytes);

        NVSDK_NGX_Result GetFeatureRequirementsD3D12(
            IDXGIAdapter* adapter,
            NVSDK_NGX_FeatureDiscoveryInfo* discoveryInfo,
            NVSDK_NGX_FeatureRequirement* requirementInfo);

        NVSDK_NGX_Result CreateVulkan(
            void* cmdBuffer,                    // VkCommandBuffer
            NVSDK_NGX_Feature featureId,
            NVSDK_NGX_Parameter* parameters,
            NVSDK_NGX_Handle** outHandle);

        NVSDK_NGX_Result CreateVulkan1(
            const VkDevice device,                       // VkDevice
            void* cmdBuffer,                    // VkCommandBuffer
            NVSDK_NGX_Feature featureId,
            NVSDK_NGX_Parameter* parameters,
            NVSDK_NGX_Handle** outHandle);

        NVSDK_NGX_Result EvaluateVulkan(
            void* cmdBuffer,                    // VkCommandBuffer
            const NVSDK_NGX_Handle* featureHandle,
            NVSDK_NGX_Parameter* parameters,
            PFN_NVSDK_NGX_ProgressCallback callback);

        NVSDK_NGX_Result ReleaseVulkan(NVSDK_NGX_Handle* instanceHandle);

        NVSDK_NGX_Result InitVulkan(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            void* instance,                     // VkInstance
            void* physicalDevice,               // VkPhysicalDevice
            void* device,                       // VkDevice
            void* getInstanceProcAddr,          // PFN_vkGetInstanceProcAddr
            void* getDeviceProcAddr,            // PFN_vkGetDeviceProcAddr
            const NVSDK_NGX_FeatureCommonInfo* featureInfo, // zgodnie z Twoim headerem
            NVSDK_NGX_Version sdkVersion);

        NVSDK_NGX_Result InitVulkanExt(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            void* instance,                      // VkInstance
            void* physicalDevice,                // VkPhysicalDevice
            void* device,                        // VkDevice
            NVSDK_NGX_Version sdkVersion,
            const NVSDK_NGX_FeatureCommonInfo* featureInfo);

        NVSDK_NGX_Result InitVulkanExt2(
            unsigned long long applicationId,
            const wchar_t* applicationDataPath,
            void* instance, void* physicalDevice, void* device,
            void* getInstanceProcAddr, void* getDeviceProcAddr,
            NVSDK_NGX_Version sdkVersion,
            const NVSDK_NGX_FeatureCommonInfo* featureInfo);

        NVSDK_NGX_Result PopulateParametersVulkan(NVSDK_NGX_Parameter* parameters);

        uint32_t GetApiVersion();
        NVSDK_NGX_Result GetDriverVersionEx(uint32_t* versions, uint32_t inputVersionCount, uint32_t* totalDriverVersionCount);
        uint32_t GetDriverVersion();
        uint32_t GetApplicationId();
        uint32_t GetGpuArchitecture();
        uint32_t GetSnippetVersion();

    private:
        // Logging helpers (prefix with exact NVSDK entry name)
        void LogInfo(const wchar_t* entry, const std::wstring& message);
        void LogWarning(const wchar_t* entry, const std::wstring& message);
        void LogError(const wchar_t* entry, const std::wstring& message);
        void LogNoBackend(const wchar_t* entry);
        void PopulateParameters(NVSDK_NGX_Parameter* Parameters);
        void OnCreate();
        void OnRelease();
        HMODULE GetBackend();
        static NVSDK_NGX_Result EstimateVRAMCallback(uint32_t mvecDepthWidth, uint32_t mvecDepthHeight,
            uint32_t colorWidth, uint32_t colorHeight,
            uint32_t colorBufferFormat,
            uint32_t mvecBufferFormat, uint32_t depthBufferFormat,
            uint32_t hudLessBufferFormat, uint32_t uiBufferFormat, size_t* EstimatedSize);

    private:
        INgxLogger& logger;
        NgxRuntimeState& state;
        IProcResolver& resolver;
        BackendManager& backends;
    };
}