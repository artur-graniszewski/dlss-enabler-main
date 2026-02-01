#include "NgxFrontend.h"
#include "NvngxCommon.h"          // NVSDK_NGX_SUCCEED, ctx, helpers
#include "../Core/Context.h" 
#include "NgxLogHelpers.h"
#include "DlssgProxy.h"
#include "Common.h"
#include "../Includes/dlss/nvsdk_ngx_params.h"
#include "../Includes/dlss/nvsdk_ngx.h"
#include "../Includes/dlss/nvsdk_ngx_defs.h"
#include "../Includes/NvParamImp.h"
#include "NgxFeatureEvents.h"

extern std::unique_ptr<DLSSG::DlssgProxy> dlssgModule;

namespace NGX
{
	static bool IsDlssgFeature(NVSDK_NGX_Feature featureId) {
		// NVSDK_NGX_Feature_FrameGeneration = 11
		return featureId == NVSDK_NGX_Feature_FrameGeneration;
	}

	static bool IsDlssgHandle(const std::unordered_map<const NVSDK_NGX_Handle*, NVSDK_NGX_Feature>& registry, const NVSDK_NGX_Handle* handle) {
		auto it = registry.find(handle);
		if (it != registry.end()) {
			return IsDlssgFeature(it->second);
		}
		return false;
	}

	NVSDK_NGX_Result NGX_DeepDvcCallback()
	{
		return NVSDK_NGX_Result_Success;
	}

	#define NVSDK_NGX_Parameter_DeepDVC_GetStatsCallback        "DeepDVC.GetStatsCallback"
	using ::VkInstance;
	using ::VkDevice;
	using ::VkPhysicalDevice;

	// ===== Logging helpers =====
	void NgxFrontend::LogInfo(const wchar_t* entry, const std::wstring& message) { logger.Info(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxFrontend::LogWarning(const wchar_t* entry, const std::wstring& message) { logger.Warning(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxFrontend::LogError(const wchar_t* entry, const std::wstring& message) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxFrontend::LogNoBackend(const wchar_t* entry) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": backend entrypoint not found"); }

	// ===== D3D12: CreateFeature (fully implemented) =====

	void NgxFrontend::OnEvaluateFeature(const NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters)
	{
		static int isThrottlingFg = 0;
		static std::deque<float> fpsHistory;

		if (fpsHistory.size() >= 10) {
			fpsHistory.pop_back();
		}

		/*
		int isDfgEnabled = 0;
		InParameters->Get("DFG.Enabled", &isDfgEnabled);

		if (ctx.ngx.isDynamicFrameGenerationEnabled != (bool)isDfgEnabled) {
			int orgDynamicFrameGenerationEnabled = 0;
			InParameters->Get("DLSSEnabler.InternalDFG.Enabled", &orgDynamicFrameGenerationEnabled);

			if (orgDynamicFrameGenerationEnabled == isDfgEnabled) {
				// its a duplicated NGX param, synchronize it with the source of truth...
				InParameters->Set("DLSSEnabler.InternalDFG.Enabled", (int)ctx.ngx.isDynamicFrameGenerationEnabled);
				InParameters->Set("DFG.Enabled", (int)ctx.ngx.isDynamicFrameGenerationEnabled);
			}
			else {
				LOG_INFO(std::wstring(L"Dynamic Frame Generation will be ") + (isDfgEnabled == 0 ? L"disabled" : L"enabled"));
				//ctx.ngx.isDynamicFrameGenerationEnabled = (bool)isDfgEnabled;
				if (!ctx.deactivateDFG) {
					ctx.ngx.isDynamicFrameGenerationEnabled = isDfgEnabled > 0;
				}
				else {
					tmpEnableDFG = (bool)isDfgEnabled;
				}
			}
		}
		*/
		int frameRate = 0;
		InParameters->Get("FramerateLimit", &frameRate);

		if (frameRate != ctx.reflex.desiredFpsLimit) {
			int orgFrameLimit = 0;
			InParameters->Get("DLSSEnabler.InternalFramerateLimit", &orgFrameLimit);

			if (orgFrameLimit == frameRate) {
				// its a duplicated NGX param, synchronize it with the source of truth...
				InParameters->Set("DLSSEnabler.InternalFramerateLimit", (int)ctx.reflex.desiredFpsLimit);
				InParameters->Set("FramerateLimit", (int)ctx.reflex.desiredFpsLimit);
			}
			else {
				LogInfo(L"OnEvaluateFeature", L"Adjusting FPS limit to: " + std::to_wstring(frameRate));
				//ctx.reflex.desiredFpsLimit = frameRate;
				//ctx.reflex.realFpsLimit = (double)frameRate;
			}
		}
		/*
		NGX_ReportUpscalerStats(InParameters);

		float ngxDelta = 0.0f;

		InParameters->Get(NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, &ngxDelta);
		if (ngxDelta == 0.0f) {
			InParameters->Set(NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, (float)ctx.reflex.timeFrameDeltaMsec);
		}

		if (frameGenerationHandles.find(InFeatureHandle) != frameGenerationHandles.end()) {
			if (!ctx.ngx.isFrameGenerationEnabled || ctx.ngx.isNextFrameSkippable) {
				InParameters->Set("DLSSG.Reset", 1);
			}

			ctx.ngx.lastEvaluationTimeMsec = Common::GetCurrentTimeMsec();

			if (ctx.reflex.desiredFpsLimit && ctx.ngx.isDynamicFrameGenerationEnabled) {
				static int threshold = 2;
				// if the frame has been duplicated previously, we need to adjust GPU potential
				if (isThrottlingFg > threshold) {
					isThrottlingFg = threshold;
				}
				else if (isThrottlingFg < -threshold) {
					isThrottlingFg = -threshold;
				}

				float potentialReflexFPStmp = (float)(isThrottlingFg < 0 ? ctx.reflex.potentialFps / 2 : ctx.reflex.potentialFps);
				fpsHistory.push_front(potentialReflexFPStmp);

				int frames = 0;
				float sum = 0;
				for (float value : fpsHistory) {
					frames++;
					sum += value;
				}

				potentialReflexFPStmp = sum / frames;


				potentialReflexFPStmp = GetPotentialFPS();

				if (potentialReflexFPStmp > ctx.reflex.desiredFpsLimit * 0.9f) {
					//if (potentialReflexFPS >= ctx.reflex.desiredFpsLimit - 2) {
					isThrottlingFg--;
				}
				else {
					isThrottlingFg++;
					if (isThrottlingFg == 0) {
						//
					}
				}

				if (isThrottlingFg <= -threshold) {
					//potentialReflexFPS /= 2;
					ctx.reflex.realFpsLimit = ctx.reflex.desiredFpsLimit;
					if (ctx.logging.isUltraDebugEnabled) {
						Console::ShowStatus(L">> Potential FPS: " + std::to_wstring(potentialReflexFPStmp)
							+ L"(" + std::to_wstring(potentialReflexFPStmp) + L") "
							+ L"/" + std::to_wstring(ctx.reflex.desiredFpsLimit)
							+ L", frametime is : " + std::to_wstring(ctx.reflex.timeFrameDeltaMsec)
							+ L": disabling FG      ");
					}
					InParameters->Set("DLSSG.Reset", 1);
					ctx.ngx.isDuplicatingFrames = true;
				}
				else {
					ctx.reflex.realFpsLimit = ctx.reflex.desiredFpsLimit * 1.5;
					if (ctx.logging.isUltraDebugEnabled) {
						Console::ShowStatus(L">> Potential FPS: " + std::to_wstring(potentialReflexFPStmp)
							+ L"(" + std::to_wstring(potentialReflexFPStmp) + L") "
							+ L"/" + std::to_wstring(ctx.reflex.desiredFpsLimit)
							+ L", frametime is : " + std::to_wstring(ctx.reflex.timeFrameDeltaMsec)
							+ L"                       ");
					}
					ctx.ngx.isDuplicatingFrames = false;
				}
			}
		}
		*/
	}


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

	NVSDK_NGX_Result NgxFrontend::InitD3D11Ext(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		ID3D11Device* device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long applicationId,
			const wchar_t*,
			ID3D11Device*,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(applicationId, applicationDataPath, device, sdkVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D11()
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE();
		result = proxy();
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D12()
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE();
		result = proxy();
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D11_1(ID3D11Device* device)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(ID3D11Device*);
		result = proxy(device);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownVulkan()
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE();
		result = proxy();
		NGX_LOG_RESULT_AND_RETURN;
	}

	// @todo, check why its not a pointer....
	NVSDK_NGX_Result NgxFrontend::ShutdownVulkan_1(VkDevice InDevice)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown1");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(VkDevice);
		result = proxy(InDevice);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D12_1(ID3D12Device* device)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(ID3D12Device*);
		result = proxy(device);
		NGX_LOG_RESULT_AND_RETURN;
	}


	NVSDK_NGX_Result NgxFrontend::InitD3D12Ext(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		ID3D12Device* device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long applicationId,
			const wchar_t*,
			ID3D12Device*,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(applicationId, applicationDataPath, device, sdkVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D12ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
		const wchar_t* InApplicationDataPath, ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Init_ProjectID");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(const char*, NVSDK_NGX_EngineType, const char*,
			const wchar_t*, ID3D12Device*, NVSDK_NGX_Version, NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D11ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
		const wchar_t* InApplicationDataPath, ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Init_ProjectID");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(const char*, NVSDK_NGX_EngineType, const char*,
			const wchar_t*, ID3D11Device*, NVSDK_NGX_Version, NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D11(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D11Device* InDevice,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Init");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

		NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, ID3D11Device*, NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);
		result = proxy(InApplicationId, InApplicationDataPath, InDevice, InFeatureInfo, InSDKVersion);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D12(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D12Device* InDevice,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Init");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

		NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, ID3D12Device*, NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);
		result = proxy(InApplicationId, InApplicationDataPath, InDevice, InFeatureInfo, InSDKVersion);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetScratchBufferSizeD3D12(
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

	NVSDK_NGX_Result NgxFrontend::GetScratchBufferSizeVulkan(
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

	NVSDK_NGX_Result NgxFrontend::GetScratchBufferSizeD3D11(
		NVSDK_NGX_Feature featureId,
		const NVSDK_NGX_Parameter* parameters,
		size_t* outSizeInBytes)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetScratchBufferSize");

		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);
		result = proxy(featureId, parameters, outSizeInBytes);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureRequirementsD3D11(IDXGIAdapter* Adapter,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetFeatureRequirements");
		NGX_LOG_CALL;

		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

		NGX_RESOLVE_PROXY_ONCE(IDXGIAdapter*, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);
		result = proxy(Adapter, FeatureDiscoveryInfo, RequirementInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::CreateD3D11(
		ID3D11DeviceContext* cmdList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_CreateFeature");

		NGX_LOG_CALL;

		// Dispatch PRE-CREATE event
		NgxFeatureEvents::DispatchPreCreateD3D11(cmdList, featureId, parameters);

		NGX_RESOLVE_PROXY_ONCE(ID3D11DeviceContext*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
		result = proxy(cmdList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			if (featureId == NVSDK_NGX_Feature_SuperSampling) {
				ctx.ngx.isUpscalingActive = true;
				parameters->Get("PerfQualityValue", &ctx.ngx.upscalingQuality);
			}
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			// Dispatch POST-CREATE event
			NgxFeatureEvents::DispatchPostCreateD3D11(cmdList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::EvaluateD3D11(
		ID3D11DeviceContext* cmdList,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isNgxEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		// Get feature type for event dispatching
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = true;
		}

		// Dispatch PRE-EVALUATE event
		NgxFeatureEvents::DispatchPreEvaluateD3D11(cmdList, featureHandle, parameters, featureId);

		OnEvaluateFeature(featureHandle, parameters);
		NGX_RESOLVE_PROXY_ONCE(ID3D11DeviceContext*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
		result = proxy(cmdList, featureHandle, parameters, callback);

		// Dispatch POST-EVALUATE event
		NgxFeatureEvents::DispatchPostEvaluateD3D11(cmdList, featureHandle, parameters, featureId, result);

		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"NGX failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Result NgxFrontend::ReleaseD3D11(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_ReleaseFeature");
		NGX_LOG_CALL;

		// Get feature type before release
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		// Dispatch PRE-RELEASE event
		NgxFeatureEvents::DispatchPreReleaseD3D11(featureHandle, featureId);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = false;
		}

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
		result = proxy(featureHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			handleRegistry.erase(featureHandle);
		}

		// Dispatch POST-RELEASE event
		NgxFeatureEvents::DispatchPostReleaseD3D11(featureHandle, featureId, result);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureRequirementsD3D12(IDXGIAdapter* Adapter,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_GetFeatureRequirements");
		NGX_LOG_CALL;

		bool isDlssgDetected = true;
		if (!GetModuleHandleW(L"nvngx_dlssg.dll")) {
			LogWarning(kEntry, L"NGX did not detect NVNGX_DLSSG.DLL file");
			isDlssgDetected = false;
		}
		else {
			LogInfo(kEntry, L"NGX detected NVNGX_DLSSG.DLL file");
		}
		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

		if (!isDlssgDetected && FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature::NVSDK_NGX_Feature_FrameGeneration) {
			result = NVSDK_NGX_Result_Success;
		}
		else
		{
			NGX_RESOLVE_PROXY_ONCE(IDXGIAdapter*, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);
			result = proxy(Adapter, FeatureDiscoveryInfo, RequirementInfo);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureRequirementsVulkan(
		const VkInstance instance,
		const VkPhysicalDevice device,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetFeatureRequirements");
		NGX_LOG_CALL;

		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

		NGX_RESOLVE_PROXY_ONCE(const VkInstance, const VkPhysicalDevice, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);
		result = proxy(instance, device, FeatureDiscoveryInfo, RequirementInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}


	NVSDK_NGX_Result NgxFrontend::CreateD3D12(
		ID3D12GraphicsCommandList* cmdList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_CreateFeature");

		NGX_LOG_CALL;

		// Dispatch PRE-CREATE event (listeners can modify parameters)
		NgxFeatureEvents::DispatchPreCreateD3D12(cmdList, featureId, parameters);

		// === DLSSG REDIRECT ===
		if (IsDlssgFeature(featureId) && dlssgModule) {
			LogInfo(kEntry, L"[DLSSG-REDIRECT] CreateFeature -> dlssgModule");
			result = dlssgModule->CreateD3D12(cmdList, featureId, parameters, outHandle);
		}
		else {
			NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
			result = proxy(cmdList, featureId, parameters, outHandle);
		}
		// === END DLSSG REDIRECT ===

		if (NVSDK_NGX_SUCCEED(result)) {
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			if (featureId == NVSDK_NGX_Feature_SuperSampling) {
				ctx.ngx.isUpscalingActive = true;
				parameters->Get("PerfQualityValue", &ctx.ngx.upscalingQuality);
			}

			// Dispatch POST-CREATE event (for initialization like SSRTGI)
			NgxFeatureEvents::DispatchPostCreateD3D12(cmdList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::EvaluateD3D12(
		ID3D12GraphicsCommandList* cmdList,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isNgxEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		// Get feature type for event dispatching
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = true;
		}

		// Dispatch PRE-EVALUATE event (for effects like SSRTGI)
		parameters->Set("DLSSG.DispatchFlags", ctx.flags);
		NgxFeatureEvents::DispatchPreEvaluateD3D12(cmdList, featureHandle, parameters, featureId);

		OnEvaluateFeature(featureHandle, parameters);

		// === DLSSG REDIRECT ===
		if (IsDlssgFeature(featureId) && dlssgModule) {
			if (isFirstCall) {
				LogInfo(kEntry, L"[DLSSG-REDIRECT] EvaluateFeature -> dlssgModule");
			}
			result = dlssgModule->EvaluateD3D12(cmdList, featureHandle, parameters, callback);
		}
		else {
			NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
			result = proxy(cmdList, featureHandle, parameters, callback);
		}
		// === END DLSSG REDIRECT ===

		// Dispatch POST-EVALUATE event
		NgxFeatureEvents::DispatchPostEvaluateD3D12(cmdList, featureHandle, parameters, featureId, result);

		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"NGX failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Feature NgxFrontend::GetFeatureByHandleId(const NVSDK_NGX_Handle* inHandleId)
	{
		auto it = handleRegistry.find(inHandleId);
		if (it == handleRegistry.end()) {
			return NVSDK_NGX_Feature_Reserved_Unknown;
		}

		return it->second;
	}

	NVSDK_NGX_Result NgxFrontend::ReleaseD3D12(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_ReleaseFeature");

		NGX_LOG_CALL;

		// Get feature type before release
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		// Dispatch PRE-RELEASE event (for cleanup like SSRTGI)
		NgxFeatureEvents::DispatchPreReleaseD3D12(featureHandle, featureId);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = false;
		}

		// === DLSSG REDIRECT ===
		if (IsDlssgFeature(featureId) && dlssgModule) {
			LogInfo(kEntry, L"[DLSSG-REDIRECT] ReleaseFeature -> dlssgModule");
			result = dlssgModule->ReleaseD3D12(featureHandle);
		}
		else {
			NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
			result = proxy(featureHandle);
		}
		// === END DLSSG REDIRECT ===

		if (NVSDK_NGX_SUCCEED(result)) {
			handleRegistry.erase(featureHandle);
		}

		// Dispatch POST-RELEASE event
		NgxFeatureEvents::DispatchPostReleaseD3D12(featureHandle, featureId, result);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitVulkan(
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
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init");

		NGX_LOG_CALL;

		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long,
			const wchar_t*,
			VkInstance,
			VkPhysicalDevice,
			VkDevice,
			PFN_vkGetInstanceProcAddr,
			PFN_vkGetDeviceProcAddr,
			NVSDK_NGX_FeatureCommonInfo*,
			NVSDK_NGX_Version);
		result = proxy(applicationId, applicationDataPath, instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr, InFeatureInfo, sdkVersion);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitVulkanExt(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long,
			const wchar_t*,
			VkInstance, VkPhysicalDevice, VkDevice,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);

		result = proxy(applicationId, applicationDataPath,
			instance, physicalDevice, device,
			sdkVersion, InFeatureInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitVulkanExt2(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
		VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice,
		PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA,
		NVSDK_NGX_Version InSDKVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext2");

		NGX_LOG_CALL;

		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*,
			VkInstance, VkPhysicalDevice, VkDevice,
			PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InSDKVersion, InFeatureInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::CreateVulkan(
		VkCommandBuffer CommandList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature");

		NGX_LOG_CALL;

		// Dispatch PRE-CREATE event
		NgxFeatureEvents::DispatchPreCreateVulkan(CommandList, featureId, parameters);

		NGX_RESOLVE_PROXY_ONCE(VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
		result = proxy(CommandList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			if (featureId == NVSDK_NGX_Feature_SuperSampling) {
				ctx.ngx.isUpscalingActive = true;
				parameters->Get("PerfQualityValue", &ctx.ngx.upscalingQuality);
			}

			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			// Dispatch POST-CREATE event
			NgxFeatureEvents::DispatchPostCreateVulkan(CommandList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::CreateVulkan1(
		const VkDevice device,
		VkCommandBuffer CommandList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature1");

		NGX_LOG_CALL;

		// Dispatch PRE-CREATE event
		NgxFeatureEvents::DispatchPreCreateVulkan(CommandList, featureId, parameters);

		NGX_RESOLVE_PROXY_ONCE(const VkDevice, VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
		result = proxy(device, CommandList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			if (featureId == NVSDK_NGX_Feature_SuperSampling) {
				ctx.ngx.isUpscalingActive = true;
			}
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			// Dispatch POST-CREATE event
			NgxFeatureEvents::DispatchPostCreateVulkan(CommandList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::AllocateParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::AllocateParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::AllocateParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::DestroyParametersD3D12(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_DestroyParameters");
		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);
		result = proxy(InParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::DestroyParametersVulkan(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_DestroyParameters");
		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);
		result = proxy(InParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::DestroyParametersD3D11(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_DestroyParameters");
		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);
		result = proxy(InParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetCapabilityParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetCapabilityParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		//if (ctx.ngx.isDeepDvcEnabled && (ctx.nvapi.isEmbeddedNvapiUsed || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_Available, 1);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMajor, 10);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMinor, 10);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_FeatureInitResult, 1);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_GetStatsCallback, NGX_DeepDvcCallback);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_SizeInBytes, 1024 * 1024);
		//}
		//else if (ctx.ngx.isDeepDvcEnabled == false) {
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_Available, 0);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_FeatureInitResult, 0);
		//}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_GetParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetCapabilityParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetCapabilityParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetCapabilityParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetCapabilityParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_GetCapabilityParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		// === DLSSG POPULATE ===
		if (NVSDK_NGX_SUCCEED(result) && dlssgModule && *OutParameters) {
			LogInfo(kEntry, L"[DLSSG] Populating capability parameters");
			dlssgModule->PopulateParametersD3D12(*OutParameters);
		}
		// === END DLSSG POPULATE ===

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureInstanceExtensionRequirementsVulkan(const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, uint32_t* OutExtensionCount,
		VkExtensionProperties** OutExtensionProperties
	)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_EvaluateFeature");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(const NVSDK_NGX_FeatureDiscoveryInfo*, uint32_t*, VkExtensionProperties**);
		result = proxy(FeatureDiscoveryInfo, OutExtensionCount, OutExtensionProperties);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::EvaluateVulkan(
		VkCommandBuffer cmdBuffer,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isNgxEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		// Get feature type for event dispatching
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = true;
		}

		// Dispatch PRE-EVALUATE event
		NgxFeatureEvents::DispatchPreEvaluateVulkan(cmdBuffer, featureHandle, parameters, featureId);

		OnEvaluateFeature(featureHandle, parameters);
		NGX_RESOLVE_PROXY_ONCE(VkCommandBuffer, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
		result = proxy(cmdBuffer, featureHandle, parameters, callback);

		// Dispatch POST-EVALUATE event
		NgxFeatureEvents::DispatchPostEvaluateVulkan(cmdBuffer, featureHandle, parameters, featureId, result);

		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"NGX failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Result NgxFrontend::ReleaseVulkan(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_ReleaseFeature");

		NGX_LOG_CALL;

		// Get feature type before release
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		// Dispatch PRE-RELEASE event
		NgxFeatureEvents::DispatchPreReleaseVulkan(featureHandle, featureId);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = false;
		}

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
		result = proxy(featureHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			handleRegistry.erase(featureHandle);
		}

		// Dispatch POST-RELEASE event
		NgxFeatureEvents::DispatchPostReleaseVulkan(featureHandle, featureId, result);

		NGX_LOG_RESULT_AND_RETURN;
	}

	HMODULE NgxFrontend::GetBackend()
	{
		return backends.GetUpscaler();
	}
}