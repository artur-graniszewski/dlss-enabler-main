#include "NgxProvider.h"
#include "NvngxCommon.h"          // NVSDK_NGX_SUCCEED, ctx, helpers
#include "../Core/Context.h" 
#include "NgxLogHelpers.h"
#include "DlssgProxy.h"
#include "Common.h"
#include "../Includes/dlss/nvsdk_ngx_params.h"
#include "../Includes/dlss/nvsdk_ngx.h"
#include "../Includes/dlss/nvsdk_ngx_defs.h"
#include "../Includes/NvParamImp.h"

extern std::unique_ptr<DLSSG::DlssgProxy> dlssgModule;

namespace NGX
{
	using ::VkInstance;
	using ::VkDevice;
	using ::VkPhysicalDevice;

	// ===== Logging helpers =====
	void NgxProvider::LogInfo(const wchar_t* entry, const std::wstring& message) { logger.Info(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxProvider::LogWarning(const wchar_t* entry, const std::wstring& message) { logger.Warning(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxProvider::LogError(const wchar_t* entry, const std::wstring& message) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }

	// ===== D3D12: CreateFeature (fully implemented) =====

	static void GetNGXFeatureRequirements(NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		*RequirementInfo = NVSDK_NGX_FeatureRequirement();

		if (FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature::NVSDK_NGX_Feature_RayReconstruction) {
			RequirementInfo->FeatureSupported = (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled) ? NVSDK_NGX_FeatureSupportResult_NotImplemented : NVSDK_NGX_FeatureSupportResult_Supported;
		}
		else {
			RequirementInfo->FeatureSupported = NVSDK_NGX_FeatureSupportResult_Supported;
		}

		if (FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature::NVSDK_NGX_Feature_SuperSampling) {
			if (ctx.ngx.overrideDlssUpscalerCapability && !ctx.ngx.enableDlssUpscaler) {
				RequirementInfo->FeatureSupported = NVSDK_NGX_FeatureSupportResult_AdapterUnsupported;
				ctx.ngx.isDlssSupportedByHardware = false;
			}
			else {
				ctx.ngx.isDlssSupportedByHardware = true;
			}
		}

		ctx.ngx.isDlssgSupportedByHardware = true;
		RequirementInfo->MinHWArchitecture = 10;
		strcpy_s(RequirementInfo->MinOSVersion, "10.0.0.0");
	}

	static void InitNGX(const wchar_t* InApplicationDataPath, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo** InFeatureInfo)
	{
		if (InFeatureInfo == nullptr || *InFeatureInfo == nullptr) {
			*InFeatureInfo = new NVSDK_NGX_FeatureCommonInfo();
			(*InFeatureInfo)->PathListInfo.Length = 0;
			(*InFeatureInfo)->PathListInfo.Path = nullptr;
			LOG_WARNING(L"[NVNGX] Application did not provide Feature Common Info");
		}

		LOG_INFO(L"[NVNGX]    SDK version: " + std::to_wstring(InSDKVersion));
		auto LoggingInfo = (*InFeatureInfo)->LoggingInfo;
		LOG_INFO(L"[NVNGX]    Logging level: " + std::to_wstring(LoggingInfo.MinimumLoggingLevel));
		(*InFeatureInfo)->LoggingInfo.LoggingCallback = NGX_Logger;
		(*InFeatureInfo)->LoggingInfo.DisableOtherLoggingSinks = false;
		(*InFeatureInfo)->LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
		(*InFeatureInfo)->LoggingInfo.DisableOtherLoggingSinks = true;

		// Allocate new memory for the path list
		unsigned int newLength = (*InFeatureInfo)->PathListInfo.Length + 1;
		wchar_t const** newPathArray = new wchar_t const* [newLength];
		std::wstring appDirPath = Common::GetProcessFilePath().parent_path().wstring() + L"\\";
		bool isAppDirPathPresent = false;
		// Copy existing paths
		for (unsigned int i = 0; i < (*InFeatureInfo)->PathListInfo.Length; ++i) {
			newPathArray[i] = (*InFeatureInfo)->PathListInfo.Path[i];
			auto path = std::wstring(newPathArray[i]);
			LOG_INFO(L"[NVNGX]    Path included: " + path);
			if (appDirPath == path) {
				isAppDirPathPresent = true;
			}
		}

		// Add the new path
		if (!isAppDirPathPresent) {
			LOG_INFO(L"[NVNGX] Adding " + appDirPath + L" to the Path List Info structure");
			wchar_t* newPath = new wchar_t[appDirPath.length() + 1];
			wcscpy_s(newPath, appDirPath.length() + 1, appDirPath.c_str());
			newPathArray[(*InFeatureInfo)->PathListInfo.Length] = newPath;

			// Update PathListInfo
			(*InFeatureInfo)->PathListInfo.Path = newPathArray;
			(*InFeatureInfo)->PathListInfo.Length = newLength;
		}

		// Try loading the nvngx files
		for (unsigned int i = 0; i < (*InFeatureInfo)->PathListInfo.Length; ++i) {
			auto dir = std::wstring((*InFeatureInfo)->PathListInfo.Path[i]) + L"\\";
			if (GetModuleHandleW(L"nvngx_dlss.dll") == nullptr && GetFileAttributesW((dir + L"nvngx_dlss.dll").c_str()) != INVALID_FILE_ATTRIBUTES) {
				LOG_INFO(L"[NVNGX] Loading DLSS module");
				LoadLibraryW((dir + L"nvngx_dlss.dll").c_str());
			}
			if (GetModuleHandleW(L"nvngx_dlssg.dll") == nullptr && GetFileAttributesW((dir + L"nvngx_dlss.dll").c_str()) != INVALID_FILE_ATTRIBUTES) {
				LOG_INFO(L"[NVNGX] Loading DLSSG module");
				LoadLibraryW((dir + L"nvngx_dlssg.dll").c_str());
			}
		}

	}

	NVSDK_NGX_Result NgxProvider::InitD3D11Ext(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		ID3D11Device* device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::ShutdownD3D11()
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_Shutdown");

		NGX_LOG_CALL;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::ShutdownD3D12()
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_Shutdown");

		NGX_LOG_CALL;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::ShutdownD3D11_1(ID3D11Device *device)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_Shutdown");

		NGX_LOG_CALL;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::ShutdownVulkan()
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_Shutdown");

		NGX_LOG_CALL;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::ShutdownVulkan_1(VkDevice InDevice)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_Shutdown1");

		NGX_LOG_CALL;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::ShutdownD3D12_1(ID3D12Device *device)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_Shutdown");

		NGX_LOG_CALL;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}


	NVSDK_NGX_Result NgxProvider::InitD3D12Ext(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		ID3D12Device* device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		//result = NVSDK_NGX_Result_Fail;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::InitD3D12ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
		const wchar_t* InApplicationDataPath, ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_Init_ProjectID");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::InitD3D11ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
		const wchar_t* InApplicationDataPath, ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_Init_ProjectID");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::InitD3D11(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D11Device* InDevice,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_Init");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::InitD3D12(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D12Device* InDevice,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_Init");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetScratchBufferSizeD3D12(
		NVSDK_NGX_Feature featureId,
		const NVSDK_NGX_Parameter* parameters,
		size_t* outSizeInBytes)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_GetScratchBufferSize");

		NGX_LOG_CALL;

		if (featureId != NVSDK_NGX_Feature_FrameGeneration) {
			LogError(kEntry, L"Unrecognized feature ID: " + std::to_wstring(featureId));
			result = NVSDK_NGX_Result_FAIL_InvalidParameter;
		}
		else {
			result = dlssgModule->GetScratchBufferSizeD3D12(featureId, parameters, outSizeInBytes);
		}
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetScratchBufferSizeVulkan(
		NVSDK_NGX_Feature featureId,
		const NVSDK_NGX_Parameter* parameters,
		size_t* outSizeInBytes)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_GetScratchBufferSize");

		NGX_LOG_CALL;

		if (featureId != NVSDK_NGX_Feature_FrameGeneration) {
			LogError(kEntry, L"Unrecognized feature ID: " + std::to_wstring(featureId));
			result = NVSDK_NGX_Result_FAIL_InvalidParameter;
		}
		else {
			result = dlssgModule->GetScratchBufferSizeVulkan(featureId, parameters, outSizeInBytes);
		}
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetScratchBufferSizeD3D11(
		NVSDK_NGX_Feature featureId,
		const NVSDK_NGX_Parameter* parameters,
		size_t* outSizeInBytes)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_GetScratchBufferSize");

		NGX_LOG_CALL;

		result = NVSDK_NGX_Result_FAIL_InvalidParameter;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetFeatureRequirementsD3D11(IDXGIAdapter* Adapter,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_GetFeatureRequirements");
		NGX_LOG_CALL;

		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);
		//RequirementInfo->FeatureSupported = NVSDK_NGX_FeatureSupportResult_AdapterUnsupported;
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::CreateD3D11(
		ID3D11DeviceContext* cmdList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_CreateFeature");

		NGX_LOG_CALL;

		result = NVSDK_NGX_Result_FAIL_FeatureNotSupported;

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::EvaluateD3D11(
		ID3D11DeviceContext* cmdList,
		const NVSDK_NGX_Handle* featureHandle,
		const NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_EvaluateFeature");

		NGX_LOG_CALL;

		result = NVSDK_NGX_Result_FAIL_InvalidParameter;

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::ReleaseD3D11(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_ReleaseFeature");

		NGX_LOG_CALL;

		result = NVSDK_NGX_Result_FAIL_InvalidParameter;

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetFeatureRequirementsD3D12(IDXGIAdapter* Adapter,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_GetFeatureRequirements");
		NGX_LOG_CALL;

		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetFeatureRequirementsVulkan(
		const VkInstance instance,
		const VkPhysicalDevice device,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_GetFeatureRequirements");
		NGX_LOG_CALL;

		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);
		result = NVSDK_NGX_Result_Success;
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::CreateD3D12(
		ID3D12GraphicsCommandList* cmdList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_CreateFeature");

		NGX_LOG_CALL;

		NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

		result = dlssgModule->CreateD3D12(cmdList, featureId, parameters, outHandle);
		if (NVSDK_NGX_SUCCEED(result)) {
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::EvaluateD3D12(
		ID3D12GraphicsCommandList* cmdList,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isFgEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		static bool isRetryTried = false;
		auto featureId = GetFeatureByHandleId(featureHandle);
		if (featureId != NVSDK_NGX_Feature_FrameGeneration) {
			LogError(kEntry, L"Unrecognized feature ID: " + std::to_wstring(featureHandle->Id));
			result = NVSDK_NGX_Result_FAIL_InvalidParameter;
		}
		else {
			result = dlssgModule->EvaluateD3D12(cmdList, featureHandle, parameters, callback);
		}
		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"DLSSG failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Feature NgxProvider::GetFeatureByHandleId(const NVSDK_NGX_Handle* inHandleId)
	{
		auto it = handleRegistry.find(inHandleId);
		if (it == handleRegistry.end()) {
			return NVSDK_NGX_Feature_Reserved_Unknown;
		}
		
		return it->second;
	}

	NVSDK_NGX_Result NgxProvider::ReleaseD3D12(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_ReleaseFeature");

		NGX_LOG_CALL;

		auto featureId = GetFeatureByHandleId(featureHandle);
		if (featureId != NVSDK_NGX_Feature_FrameGeneration) {
			LogError(kEntry, L"Unrecognized feature ID: " + std::to_wstring(featureHandle->Id));
			result = NVSDK_NGX_Result_FAIL_InvalidParameter;
		}
		else {
			result = dlssgModule->ReleaseD3D12(featureHandle);
			if (NVSDK_NGX_SUCCEED(result)) {
				handleRegistry.erase(featureHandle);
			}
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::InitVulkan(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		VkInstance instance,
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		PFN_vkGetInstanceProcAddr getInstanceProcAddr,
		PFN_vkGetDeviceProcAddr getDeviceProcAddr,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo,
		NVSDK_NGX_Version sdkVersion)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_Init");

		NGX_LOG_CALL;

		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		//result = NVSDK_NGX_Result_Fail;
		result = dlssgModule->InitVulkan(applicationId, applicationDataPath, instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr, InFeatureInfo, sdkVersion);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::InitVulkanExt(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		//result = NVSDK_NGX_Result_Fail;
		result = dlssgModule->InitVulkanExt(applicationId, applicationDataPath,
			instance, physicalDevice, device,
			sdkVersion, InFeatureInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::InitVulkanExt2(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, 
		VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, 
		PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA, 
		NVSDK_NGX_Version InSDKVersion, 
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_Init_Ext2");

		NGX_LOG_CALL;

		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		result = dlssgModule->InitVulkanExt2(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InSDKVersion, InFeatureInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::CreateVulkan(
		VkCommandBuffer CommandList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_CreateFeature");

		NGX_LOG_CALL;

		NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

		result = dlssgModule->CreateVulkan(CommandList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::CreateVulkan1(
		const VkDevice device,
		VkCommandBuffer CommandList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_CreateFeature1");

		NGX_LOG_CALL;

		NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

		result = dlssgModule->CreateVulkan1(device, CommandList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::AllocateParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParametersD3D11();

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::AllocateParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParametersVulkan();

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::AllocateParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParameters();

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::DestroyParametersD3D12(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_DestroyParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;

		delete InParameters;

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::DestroyParametersVulkan(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_DestroyParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;

		delete InParameters;

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::DestroyParametersD3D11(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_DestroyParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;

		delete InParameters;

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_GetParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParametersD3D11();

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetCapabilityParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D11_GetCapabilityParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParametersD3D11();
		
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_GetParameters!");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParameters();
		//result = NVSDK_NGX_Result_Fail;
		result = dlssgModule->PopulateParametersD3D12(*OutParameters);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetCapabilityParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_GetCapabilityParameters!");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParametersVulkan();
		result = dlssgModule->PopulateParametersVulkan(*OutParameters);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_GetCapabilityParameters!");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParametersVulkan();
		result = dlssgModule->PopulateParametersVulkan(*OutParameters);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetCapabilityParametersD3D12(NVSDK_NGX_Parameter** OutParameters) 
	{
		NGX_INIT_SHIM("NVSDK_NGX_D3D12_GetCapabilityParameters!");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		*OutParameters = getNGXParameters();
		//result = NVSDK_NGX_Result_Fail;
		result = dlssgModule->PopulateParametersD3D12(*OutParameters);
		NGX_LOG_RESULT_AND_RETURN;
		using PfnType = NVSDK_NGX_Result(*)(NVSDK_NGX_Parameter**);                              
		static PfnType proxy = nullptr;                                                 
		//if (!proxy) {
		//	proxy = reinterpret_cast<PfnType>(resolver.Resolve(Common::GetModuleHandle(), "NVSDK_NGX_D3D12_PopulateParameters_Impl"));
		//	if (!proxy) {

		//		//NGX_INIT_SHIM("NVSDK_NGX_D3D12_GetCapabilityParameters!! NO PROXY");
		//		result = NVSDK_NGX_Result_Fail;                                         
		//	}
		//	else {
		//		result = proxy(*OutParameters);
		//	}
		//}

		//if (!proxy) {

		//	proxy = reinterpret_cast<PfnType>(resolver.Resolve(Common::GetModuleHandle(), "NVSDK_NGX_D3D12_GetCapabilityParameters"));
		//	if (!proxy) {

		//		NGX_INIT_SHIM("NVSDK_NGX_D3D12_GetCapabilityParameters!! NO PROXY");
		//		result = NVSDK_NGX_Result_Fail;
		//	}
		//	else {
		//		result = proxy(OutParameters);
		//	}
		//}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxProvider::GetFeatureInstanceExtensionRequirementsVulkan(const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, uint32_t* OutExtensionCount,
		VkExtensionProperties** OutExtensionProperties
	)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_EvaluateFeature");
		result = NVSDK_NGX_Result_Success;

		if ((FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature_SuperSampling ||
			FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature_FrameGeneration) &&
			OutExtensionCount != nullptr)
		{
			if (OutExtensionProperties == nullptr) {
				*OutExtensionCount = 3;
				NGX_LOG_RESULT_AND_RETURN;
			}
			else if (*OutExtensionCount != 3) {
				result = NVSDK_NGX_Result_FAIL_InvalidParameter;
				NGX_LOG_RESULT_AND_RETURN
			}

			auto& ext0 = (*OutExtensionProperties)[0];
			std::memset(ext0.extensionName, 0, sizeof(ext0.extensionName));
			strcpy_s(
				ext0.extensionName,
				sizeof(ext0.extensionName),
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
			);
			ext0.specVersion = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION;

			auto& ext1 = (*OutExtensionProperties)[1];
			std::memset(ext1.extensionName, 0, sizeof(ext1.extensionName));
			strcpy_s(ext1.extensionName, sizeof(ext1.extensionName),
				VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
			ext1.specVersion = VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_SPEC_VERSION;

			auto& ext2 = (*OutExtensionProperties)[2];
			std::memset(ext2.extensionName, 0, sizeof(ext2.extensionName));
			strcpy_s(ext2.extensionName, sizeof(ext2.extensionName),
				VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
			ext2.specVersion = VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_SPEC_VERSION;

			NGX_LOG_RESULT_AND_RETURN;
		}

		if (OutExtensionCount != nullptr) {
			*OutExtensionCount = 0;
		}

		NGX_LOG_RESULT_AND_RETURN;
	}
	NVSDK_NGX_Result NgxProvider::EvaluateVulkan(
		VkCommandBuffer cmdBuffer,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isFgEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		static bool isRetryTried = false;
		auto featureId = GetFeatureByHandleId(featureHandle);
		if (featureId != NVSDK_NGX_Feature_FrameGeneration) {
			LogError(kEntry, L"Unrecognized feature ID: " + std::to_wstring(featureHandle->Id));
			result = NVSDK_NGX_Result_FAIL_InvalidParameter;
		}
		else {
			result = dlssgModule->EvaluateVulkan(cmdBuffer, featureHandle, parameters, callback);
		}
		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"DLSSG failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Result NgxProvider::ReleaseVulkan(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_SHIM("NVSDK_NGX_VULKAN_ReleaseFeature");

		NGX_LOG_CALL;

		auto featureId = GetFeatureByHandleId(featureHandle);
		if (featureId != NVSDK_NGX_Feature_FrameGeneration) {
			LogError(kEntry, L"Unrecognized feature ID: " + std::to_wstring(featureHandle->Id));
			result = NVSDK_NGX_Result_FAIL_InvalidParameter;
		}
		else {
			result = dlssgModule->ReleaseVulkan(featureHandle);
			if (NVSDK_NGX_SUCCEED(result)) {
				handleRegistry.erase(featureHandle);
			}
		}

		NGX_LOG_RESULT_AND_RETURN;
	}
}