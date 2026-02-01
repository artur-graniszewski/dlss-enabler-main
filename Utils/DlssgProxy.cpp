#include "DlssgProxy.h"
#include "NvngxCommon.h"          // NVSDK_NGX_SUCCEED, ctx, helpers
#include "../Core/Context.h" 
#include "NgxLogHelpers.h"
#include "ScopedGpuSpoofing.h"
#include "Common.h"
#include "OverdriveController.h"

namespace DLSSG
{
    class FrameGenerationHelper
    {
    public:
        // =============================================================================
        // ShouldGenerateFrame - Determines if frame should be generated in dynamic mode
        // =============================================================================
        //
        // Uses:
        //   - ctx.ngx.dynamicFrameGenerationThreshold (FPS threshold, e.g. 60)
        //   - ctx.ngx.isDynamicFrameGenerationStartingOnThreshold (STARTS or STOPS on threshold)
        //   - ctx.reflex.potentialFps (current potential FPS)
        //
        // Logic:
        //   - STARTS (true):  Generate when FPS <= threshold (help when FPS is low)
        //   - STOPS (false):  Generate when FPS > threshold (stop when FPS is low)
        //
        // Returns:
        //   true  - should generate frame
        //   false - should NOT generate frame
        //
        static bool ShouldGenerateFrame()
        {
            if (ctx.ngx.isDynamicFrameGenerationStartingOnThreshold)
            {
                // Frame generation STARTS when FPS drops to or below threshold
                // (generate frames to boost low FPS)
                return (ctx.reflex.currentFps <= ctx.ngx.dynamicFrameGenerationThreshold);
            }
            else
            {
                // Frame generation STOPS when FPS drops to or below threshold
                // (stop generating when GPU is struggling)
                return (ctx.reflex.currentFps > ctx.ngx.dynamicFrameGenerationThreshold);
            }
        }
    };

    static constexpr wchar_t kModule[] = L"DLSSG";

    // ===== Logging helpers =====
    void DlssgProxy::LogInfo(const wchar_t* entry, const std::wstring& message) { logger.Info(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
    void DlssgProxy::LogWarning(const wchar_t* entry, const std::wstring& message) { logger.Warning(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
    void DlssgProxy::LogError(const wchar_t* entry, const std::wstring& message) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
    void DlssgProxy::LogNoBackend(const wchar_t* entry) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": backend entrypoint not found"); }

    void DlssgProxy::OnCreate()
    {
        ctx.ngx.isFrameGenerationActive = true;
        //if (ctx.reflex.desiredFpsLimit > 0) {
        //    ctx.reflex.desiredFpsLimit /= 2;
        //}
    }

    void DlssgProxy::OnRelease()
    {
        ctx.ngx.isFrameGenerationActive = false;
        //if (ctx.reflex.desiredFpsLimit > 0) {
        //    ctx.reflex.desiredFpsLimit *= 2;
        //}

        ctx.ngx.isDuplicatingFrames = false;
        ctx.ngx.framesGenerated = 0;
        ctx.ngx.maxFramesGenerated = 1;
        ctx.ngx.isGeneratingFrames = false;

        ctx.ngx.lastEvaluationTimeMsec = 0.0f;
        state.isFgEvaluated.store(false);
    }

    // ===== D3D12: CreateFeature (fully implemented) =====
    NVSDK_NGX_Result DlssgProxy::CreateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle** outHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_CreateFeature");

        NGX_LOG_CALL;

        NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

        NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);

        result = proxy(cmdList, featureId, parameters, outHandle);
        auto handle = (outHandle && *outHandle) ? *outHandle : nullptr;

        if (NVSDK_NGX_SUCCEED(result)) {
            OnCreate();
        }

        NGX_LOG_RESULT_WITH_HANDLE_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::EvaluateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* featureHandle,
        NVSDK_NGX_Parameter* parameters,
        PFN_NVSDK_NGX_ProgressCallback callback)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_EvaluateFeature");

        // Latch first-call; we reset this later in Destroy/Release path.
        const bool isFirstCall = !state.isFgEvaluated.exchange(true);

        NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*,
            const NVSDK_NGX_Handle*,
            const NVSDK_NGX_Parameter*,
            PFN_NVSDK_NGX_ProgressCallback);

        if (isFirstCall) {
            NGX_LOG_CALL;
        }

        if (ctx.ngx.isDynamicFrameGenerationEnabled) {
            ctx.ngx.isGeneratingFrames = FrameGenerationHelper::ShouldGenerateFrame();
        }
        else {
            ctx.ngx.isGeneratingFrames = true;
        }

        int frameIndex = 1;
        int frameMax = 1;

        parameters->Get("DLSSG.MultiFrameIndex", &frameIndex);
        parameters->Get("DLSSG.MultiFrameCount", &frameMax);

        static bool isHudTested = false;
        if (!isHudTested) {
            isHudTested = true;
            ID3D12Resource* hud = nullptr;
            auto result = parameters->Get("DLSSG.UI", &hud);
            if (result == NVSDK_NGX_Result_Success && hud != nullptr) {
                LogInfo(kEntry, L"DLSSG provides HUD resource");
            }
            else {
                LogWarning(kEntry, L"DLSSG is missing HUD resource");
            }
        }
        //LogWarning(kEntry, L"DLSSG frame #" + std::to_wstring(frameIndex) + L" out of " + std::to_wstring(frameMax));

        if (!ctx.ngx.isGeneratingFrames) {
            parameters->Set("DLSSG.Reset", 1);
            ctx.ngx.isDuplicatingFrames = true;
            ctx.ngx.framesGenerated = 0;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }
        else {
            ctx.ngx.framesGenerated = max(frameMax, 1);
            ctx.ngx.isDuplicatingFrames = false;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }

        ctx.ngx.isFrameGenerationActive = true;

        static bool isRetryTried = false;
        result = proxy(cmdList, featureHandle, parameters, callback);
        ctx.ngx.lastEvaluationTimeMsec = Common::GetCurrentTimeMsec();

        if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
            // Only log failures after the first call
            if (!NVSDK_NGX_SUCCEED(result)) {
                LogWarning(kEntry, L"DLSSG failed for " + std::to_wstring(featureHandle->Id));

                // try once more
                if (!isRetryTried && !isFirstCall) {
                    isRetryTried = true;
                    result = proxy(cmdList, featureHandle, parameters, callback);
                }
                if (NVSDK_NGX_SUCCEED(result)) {
                    isRetryTried = false;
                    LogWarning(kEntry, L"DLSSG retry succeeded for " + std::to_wstring(featureHandle->Id));
                }
            }

            NGX_LOG_RESULT_AND_RETURN;
        }

        return result;
    }

    NVSDK_NGX_Result DlssgProxy::GetFeatureRequirementsD3D12(
        IDXGIAdapter* adapter,
        NVSDK_NGX_FeatureDiscoveryInfo* discoveryInfo,
        NVSDK_NGX_FeatureRequirement* requirementInfo)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_GetFeatureRequirements");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(IDXGIAdapter*, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);

        result = proxy(adapter, discoveryInfo, requirementInfo);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::PopulateParametersD3D12(NVSDK_NGX_Parameter* InParams)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_PopulateParameters_Impl");

        NGX_LOG_CALL;
        NVAPI_DISABLE_GPU_SPOOFING();

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);

        result = proxy(InParams);
        InParams->Set("FrameGeneration.Available", 1);
        InParams->Set("FrameGeneration.FeatureInitResult", 1);
        InParams->Set("FrameGeneration.MinDriverVersionMajor", 10);
        InParams->Set("FrameGeneration.MinDriverVersionMinor", 10);
        InParams->Set("FrameGeneration.NeedsUpdatedDriver", 0);
        InParams->Set("FrameInterpolation.Available", 1);
        InParams->Set("FrameInterpolation.FeatureInitResult", 1);
        InParams->Set("FrameInterpolation.MinDriverVersionMajor", 10);
        InParams->Set("FrameInterpolation.MinDriverVersionMinor", 10);
        InParams->Set("FrameInterpolation.NeedsUpdatedDriver", 0);
        InParams->Set("DLSSG.MultiFrameCountMax", 4); 
        //InParams->Set("DLSSEnabler.Available", 1); //

        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ReleaseD3D12(NVSDK_NGX_Handle* instanceHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_ReleaseFeature");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);

        result = proxy(instanceHandle);
        if (NVSDK_NGX_SUCCEED(result)) {
            OnRelease();
        }
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitD3D12Ext(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        ID3D12Device* device,
        NVSDK_NGX_Version sdkVersion,
        const NVSDK_NGX_Parameter* parameters)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Init_Ext");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long,
            const wchar_t*,
            ID3D12Device*,
            NVSDK_NGX_Version,
            const NVSDK_NGX_Parameter*);

        // @todo: according to preliminary research, this fails at init under linux and does nothing good here, so we disable the call
        // @todo: fixme
        //result = proxy(applicationId, applicationDataPath, device, sdkVersion, parameters);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitD3D12(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        ID3D12Device* device,
        NVSDK_NGX_Version sdkVersion)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Init");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long,
            const wchar_t*,
            ID3D12Device*,
            NVSDK_NGX_Version);

        // @todo: according to preliminary research, this fails at init under linux and does nothing good here, so we disable the call
        // @todo: fixme
        //result = proxy(applicationId, applicationDataPath, device, sdkVersion);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownD3D12()
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE();

        result = proxy();
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownVulkan()
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE();

        result = proxy();
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownVulkan_1(void* LogicalDevice)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown1");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(void*);

        result = proxy(LogicalDevice);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownD3D12_1(ID3D12Device* device)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown1");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(ID3D12Device*);

        result = proxy(device);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::GetDriverVersionEx(
        uint32_t* versions,
        uint32_t inputVersionCount,
        uint32_t* totalDriverVersionCount)
    {
        NGX_INIT_CALL("NVSDK_NGX_GetDriverVersionEx");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(uint32_t*, uint32_t, uint32_t*);

        result = proxy(versions, inputVersionCount, totalDriverVersionCount);
        NGX_LOG_RESULT_AND_RETURN;
    }

    uint32_t DlssgProxy::GetApplicationId()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetApplicationId");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    uint32_t DlssgProxy::GetApiVersion()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetAPIVersion");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    uint32_t DlssgProxy::GetDriverVersion()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetDriverVersion");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    NVSDK_NGX_Result DlssgProxy::GetScratchBufferSizeD3D12(
        NVSDK_NGX_Feature featureId,
        const NVSDK_NGX_Parameter* parameters,
        size_t* outSizeInBytes)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_GetScratchBufferSize");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);

        result = proxy(featureId, parameters, outSizeInBytes);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::GetScratchBufferSizeVulkan(
        NVSDK_NGX_Feature featureId,
        const NVSDK_NGX_Parameter* parameters,
        size_t* outSizeInBytes)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetScratchBufferSize");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);

        result = proxy(featureId, parameters, outSizeInBytes);
        NGX_LOG_RESULT_AND_RETURN;
    }


    uint32_t DlssgProxy::GetGpuArchitecture()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetGPUArchitecture");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = 10;// proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    uint32_t DlssgProxy::GetSnippetVersion()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetSnippetVersion");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    NVSDK_NGX_Result DlssgProxy::CreateVulkan(
        void* cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle** outHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature");

        NGX_LOG_CALL;

        NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

        NGX_RESOLVE_PROXY_ONCE(void*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
        result = proxy(cmdBuffer, featureId, parameters, outHandle);
        auto handle = (outHandle && *outHandle) ? *outHandle : nullptr;

        if (NVSDK_NGX_SUCCEED(result)) {
            OnCreate();
        }

        NGX_LOG_RESULT_WITH_HANDLE_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::CreateVulkan1(
        const VkDevice device,
        void* cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle** outHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature1");

        NGX_LOG_CALL;

        NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

        NGX_RESOLVE_PROXY_ONCE(const VkDevice, void*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);

        result = proxy(device, cmdBuffer, featureId, parameters, outHandle);
        auto handle = (outHandle && *outHandle) ? *outHandle : nullptr;

        if (NVSDK_NGX_SUCCEED(result)) {
            OnCreate();
        }
        NGX_LOG_RESULT_WITH_HANDLE_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::EvaluateVulkan(
        void* cmdBuffer,
        const NVSDK_NGX_Handle* featureHandle,
        NVSDK_NGX_Parameter* parameters,
        PFN_NVSDK_NGX_ProgressCallback callback)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_EvaluateFeature");

        const bool isFirstCall = !state.isFgEvaluated.exchange(true);

        NGX_RESOLVE_PROXY_ONCE(void*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);

        int frameIndex = 1;
        int frameMax = 1;

        parameters->Get("DLSSG.MultiFrameIndex", &frameIndex);
        parameters->Get("DLSSG.MultiFrameCount", &frameMax);

        if (OverdriveController::GetDynamicFrameGenerationEnabled()) {
            ctx.ngx.isGeneratingFrames = FrameGenerationHelper::ShouldGenerateFrame();
        }
        else {
            ctx.ngx.isGeneratingFrames = true;
        }

        if (!ctx.ngx.isGeneratingFrames) {
            parameters->Set("DLSSG.Reset", 1);
            ctx.ngx.isDuplicatingFrames = true;
            ctx.ngx.framesGenerated = 0;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }
        else {
            ctx.ngx.framesGenerated = max(frameMax, 1);
            ctx.ngx.isDuplicatingFrames = false;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }

        ctx.ngx.isFrameGenerationActive = true;

        if (isFirstCall) { NGX_LOG_CALL; }
        result = proxy(cmdBuffer, featureHandle, parameters, callback);
        ctx.ngx.lastEvaluationTimeMsec = Common::GetCurrentTimeMsec();

        if (!NVSDK_NGX_SUCCEED(result)) {
            LogWarning(kEntry, L"DLSSG failed for " + std::to_wstring(featureHandle->Id));
        }

        if (isFirstCall) { NGX_LOG_RESULT_AND_RETURN; }
        else if (!NVSDK_NGX_SUCCEED(result)) { NGX_LOG_RESULT_AND_RETURN; }

        return result;
    }

    NVSDK_NGX_Result DlssgProxy::ReleaseVulkan(NVSDK_NGX_Handle* instanceHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_ReleaseFeature");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);

        result = proxy(instanceHandle);

        if (NVSDK_NGX_SUCCEED(result)) {
            OnRelease();
        }
        NGX_LOG_RESULT_AND_RETURN;
    }
    

    NVSDK_NGX_Result DlssgProxy::PopulateParametersVulkan(NVSDK_NGX_Parameter* InParams)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_PopulateParameters_Impl");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);

        result = proxy(InParams);
        InParams->Set("FrameGeneration.Available", 1);
        InParams->Set("FrameGeneration.FeatureInitResult", 1);
        InParams->Set("FrameGeneration.MinDriverVersionMajor", 10);
        InParams->Set("FrameGeneration.MinDriverVersionMinor", 10);
        InParams->Set("FrameGeneration.NeedsUpdatedDriver", 0);
        InParams->Set("FrameInterpolation.Available", 1);
        InParams->Set("FrameInterpolation.FeatureInitResult", 1);
        InParams->Set("FrameInterpolation.MinDriverVersionMajor", 10);
        InParams->Set("FrameInterpolation.MinDriverVersionMinor", 10);
        InParams->Set("FrameInterpolation.NeedsUpdatedDriver", 0);
        InParams->Set("DLSSG.MultiFrameCountMax", 1);
        //InParams->Set("DLSSEnabler.Available", 1); //

        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitVulkan(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        void* instance,
        void* physicalDevice,
        void* device,
        void* getInstanceProcAddr,
        void* getDeviceProcAddr,
        const NVSDK_NGX_FeatureCommonInfo* featureInfo,
        NVSDK_NGX_Version sdkVersion)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init");

        NGX_LOG_CALL;
        ScopedGpuSpoofing guard;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, void*, void*, void*, void*, void*, const NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);

        result = proxy(applicationId, applicationDataPath, instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr, featureInfo, sdkVersion);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitVulkanExt(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        void* instance, void* physicalDevice, void* device,
        NVSDK_NGX_Version sdkVersion,
        const NVSDK_NGX_FeatureCommonInfo* featureInfo)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext");

        NGX_LOG_CALL;
        ScopedGpuSpoofing guard;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long,
            const wchar_t*,
            void*, void*, void*,
            NVSDK_NGX_Version,
            const NVSDK_NGX_FeatureCommonInfo*);

        result = proxy(applicationId, applicationDataPath,
            instance, physicalDevice, device,
            sdkVersion, featureInfo);

        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitVulkanExt2(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        void* instance, void* physicalDevice, void* device,
        void* getInstanceProcAddr, void* getDeviceProcAddr,
        NVSDK_NGX_Version sdkVersion,
        const NVSDK_NGX_FeatureCommonInfo* featureInfo)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext2");

        NGX_LOG_CALL;
        ScopedGpuSpoofing guard;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, void*, void*, void*, void*, void*, NVSDK_NGX_Version, const NVSDK_NGX_FeatureCommonInfo*);

        result = proxy(applicationId, applicationDataPath, instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr, sdkVersion, featureInfo);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::EstimateVRAMCallback(uint32_t mvecDepthWidth, uint32_t mvecDepthHeight,
        uint32_t colorWidth, uint32_t colorHeight,
        uint32_t colorBufferFormat,
        uint32_t mvecBufferFormat, uint32_t depthBufferFormat,
        uint32_t hudLessBufferFormat, uint32_t uiBufferFormat, size_t* EstimatedSize)
    {
        if (EstimatedSize) {
            *EstimatedSize =
                mvecDepthWidth * mvecDepthHeight * 4
                +
                colorWidth * colorHeight * 4
                +
                colorWidth * colorHeight * 4 // for depth
                +
                colorWidth * colorHeight * 4 // for hudless
                +
                colorWidth * colorHeight * 4 // for UI
                +
                colorWidth * colorHeight * 4 // for output interpolated
                +
                colorWidth * colorHeight * 4; // for output real
        }

        return NVSDK_NGX_Result_Success;
    }

    void DlssgProxy::PopulateParameters(NVSDK_NGX_Parameter* Parameters)
    {
        Parameters->Set("Enable.OFA", 1);
        Parameters->Set("DLSSG.EnableInterp", 1);
        Parameters->Set("SynchronousInit", 1);
        Parameters->Set("DLSSG.CameraPinholeOffsetX", 0.0f);
        Parameters->Set("DLSSG.CameraPinholeOffsetY", 0.0f);
        Parameters->Set("DLSSG.EstimateVRAMCallback", &DlssgProxy::EstimateVRAMCallback);
        Parameters->Set(NVSDK_NGX_Parameter_FrameInterpolation_NeedsUpdatedDriver, 0);
        Parameters->Set(NVSDK_NGX_Parameter_FrameInterpolation_FeatureInitResult, 1);
        Parameters->Set(NVSDK_NGX_Parameter_FrameInterpolation_MinDriverVersionMajor, 10);
    }

    HMODULE DlssgProxy::GetBackend()
    {
        return backends.GetFrameGen();
    }
}