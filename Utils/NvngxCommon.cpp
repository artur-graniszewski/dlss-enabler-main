#include "NvngxCommon.h"
#include "Common.h"
#include <string>
#include <wtypes.h>
#include "../Includes/nvapi.h"
#include "../Core/Context.h"
#include <deque>
#include "NvngxProxy.h"
#include <unordered_set>
#include "../Includes/NvParamImp.h"
#include "NgxState.h"

std::unordered_set<NVSDK_NGX_Handle*, PointerHash> upscalerHandles;
std::unordered_set<NVSDK_NGX_Handle*, PointerHash> frameGenerationHandles;
std::unordered_set<NVSDK_NGX_Handle*, PointerHash> deepDvcHandles;

extern NgxRuntimeState ngxRuntimeState;
static bool tmpEnableDFG = false;

typedef enum DLSS_Enabler_FrameGeneration_Mode
{
	DLSS_Enabler_FrameGeneration_Disabled,
	DLSS_Enabler_FrameGeneration_Enabled,
	DLSS_Enabler_FrameGeneration_DFG_Disabled,
	DLSS_Enabler_FrameGeneration_DFG_Enabled,
} DLSS_Enabler_FrameGeneration_Mode;

typedef enum DLSS_Enabler_Result
{
	DLSS_Enabler_Result_Success = 1,
	DLSS_Enabler_Result_Fail_Unsupported = 0,
	DLSS_Enabler_Result_Fail_Bad_Argument = -1,
} DLSS_Enabler_Result;

NGXDLLEXPORT DLSS_Enabler_Result SetFrameGenerationMode(DLSS_Enabler_FrameGeneration_Mode mode)
{
	if (frameGenerationHandles.size() == 0) {
		LOG_TRACE(L"[INIT] SetFrameGenerationMode: successful (frame generation is unsupported)");
		return DLSS_Enabler_Result_Fail_Unsupported;
	}

	switch (mode) {
	case DLSS_Enabler_FrameGeneration_Disabled:
		LOG_TRACE(L"[INIT] SetFrameGenerationMode: successful (disabling frame generation)");
		ctx.ngx.isFrameGenerationEnabled = false;
		break;
	case DLSS_Enabler_FrameGeneration_Enabled:
		LOG_TRACE(L"[INIT] SetFrameGenerationMode: successful (enabling frame generation)");
		ctx.ngx.isFrameGenerationEnabled = true;
		break;
	case DLSS_Enabler_FrameGeneration_DFG_Disabled:
		LOG_TRACE(L"[INIT] SetFrameGenerationMode: successful (disabling dynamic frame generation)");

		if (ctx.deactivateDFG) {
			break;
		}

		tmpEnableDFG = ctx.ngx.isDynamicFrameGenerationEnabled;
		ctx.deactivateDFG = true;
		ctx.ngx.isDynamicFrameGenerationEnabled = false;
		break;
	case DLSS_Enabler_FrameGeneration_DFG_Enabled:
		LOG_TRACE(L"[INIT] SetFrameGenerationMode: successful (enabling dynamic frame generation)");

		if (!ctx.deactivateDFG) {
			break;
		}

		ctx.ngx.isDynamicFrameGenerationEnabled = tmpEnableDFG;
		ctx.deactivateDFG = false;

		break;
	default:
		LOG_ERROR(L"[NVNGX] SetFrameGenerationMode: failed (wrong mode requested)");
		return DLSS_Enabler_Result_Fail_Bad_Argument;
	}

	return DLSS_Enabler_Result_Success;
}

NGXDLLEXPORT DLSS_Enabler_Result GetFrameGenerationMode(DLSS_Enabler_FrameGeneration_Mode& mode)
{
	if (frameGenerationHandles.size() == 0) {
		mode = DLSS_Enabler_FrameGeneration_Disabled;
		LOG_TRACE(L"[INIT] GetFrameGenerationMode: successful (frame generation is unsupported)");
		return DLSS_Enabler_Result_Fail_Unsupported;
	}

	if (ctx.ngx.isDuplicatingFrames && ctx.ngx.isDynamicFrameGenerationEnabled) {
		mode = DLSS_Enabler_FrameGeneration_DFG_Disabled;
		LOG_TRACE(L"[INIT] GetFrameGenerationMode: successful (frame generation is disabled by DFG)");
		return DLSS_Enabler_Result_Success;
	}
	else if (ctx.ngx.isDynamicFrameGenerationEnabled) {
		mode = DLSS_Enabler_FrameGeneration_DFG_Enabled;
		LOG_TRACE(L"[INIT] GetFrameGenerationMode: successful (frame generation is enabled by DFG)");
		return DLSS_Enabler_Result_Success;
	}

	if (!ctx.ngx.isFrameGenerationEnabled) {
		mode = DLSS_Enabler_FrameGeneration_Disabled;
		LOG_TRACE(L"[INIT] GetFrameGenerationMode: successful (frame generation is disabled)");
	}
	else {
		mode = DLSS_Enabler_FrameGeneration_Enabled;
		LOG_TRACE(L"[INIT] GetFrameGenerationMode: successful (frame generation is enabled)");
	}

	return DLSS_Enabler_Result_Success;
}


std::wstring NGX_FormatLogEntry(LPCWSTR functionName, std::wstring message)
{
	std::wstring func_name = std::wstring(functionName);
	std::wstring prefix = func_name.substr(6, func_name.find(L"_NVSDK") - 6);
	if (prefix[0] == L'N') { prefix = L"NVNGX"; }
	return L"[" + prefix + L"] " + func_name.substr(func_name.find(L"NVSDK")) + L": " + message;
}

void NGX_DisableGpuSpoofing()
{
	//ctx.currentGpuArchitecture = NV_GPU_ARCHITECTURE_TU100 > ctx.realGpuArchitecture ? NV_GPU_ARCHITECTURE_TU100 : ctx.realGpuArchitecture;
}

void NGX_EnableGpuSpoofing()
{
	ctx.currentGpuArchitecture = ctx.targetGpuArchitecture;
}

bool NGX_IsFrameGenerationFeature(NVSDK_NGX_Handle* InFeatureHandle)
{
	return frameGenerationHandles.find(InFeatureHandle) != frameGenerationHandles.end();
}

bool NGX_IsSuperSamplingFeature(NVSDK_NGX_Handle* InFeatureHandle)
{
	return upscalerHandles.find(InFeatureHandle) != upscalerHandles.end();
}

bool NGX_IsDeepDvcFeature(NVSDK_NGX_Handle* InFeatureHandle)
{
	return deepDvcHandles.find(InFeatureHandle) != deepDvcHandles.end();
}

bool NGX_RegisterFeature(NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Handle* InFeatureHandle)
{
	if (InFeatureID == NVSDK_NGX_Feature_FrameGeneration) {
		frameGenerationHandles.insert(InFeatureHandle);
	}

	if (InFeatureID == NVSDK_NGX_Feature_DeepDVC) {
		deepDvcHandles.insert(InFeatureHandle);
	}

	if (InFeatureID == NVSDK_NGX_Feature_SuperSampling) {
		upscalerHandles.insert(InFeatureHandle);
	}

	return true;
}

bool NGX_UnregisterFeature(NVSDK_NGX_Handle* InFeatureHandle)
{
	auto it = frameGenerationHandles.find(InFeatureHandle);
	if (it != frameGenerationHandles.end()) {
		frameGenerationHandles.erase(it);
	}

	auto it2 = deepDvcHandles.find(InFeatureHandle);
	if (it2 != deepDvcHandles.end()) {
		deepDvcHandles.erase(it2);
	}

	auto it3 = upscalerHandles.find(InFeatureHandle);
	if (it3 != upscalerHandles.end()) {
		upscalerHandles.erase(it3);
	}

	return true;
}


std::wstring NGX_FeatureIdToString(NVSDK_NGX_Feature InFeatureID)
{
	switch (InFeatureID) {
	case NVSDK_NGX_Feature_FrameGeneration:
		return L"Frame Generation";
	case NVSDK_NGX_Feature_RayReconstruction:
		return L"Ray Reconstruction";
	case NVSDK_NGX_Feature_SuperSampling:
		return L"Super Sampling";
	default:
		return L"Unknown";
	}
}

NVSDK_NGX_Result NGX_DeepDvcCallback()
{
	return NVSDK_NGX_Result_Success;
}
//#define NVSDK_NGX_Parameter_DeepDVC_Available               "DeepDVC.Available"
//#define NVSDK_NGX_Parameter_DeepDVC_NeedsUpdatedDriver      "DeepDVC.NeedsUpdatedDriver"
//#define NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMajor   "DeepDVC.MinDriverVersionMajor"
//#define NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMinor   "DeepDVC.MinDriverVersionMinor"
//#define NVSDK_NGX_Parameter_DeepDVC_FeatureInitResult       "DeepDVC.FeatureInitResult"
//#define NVSDK_NGX_Parameter_DeepDVC_Strength                "DeepDVC.Strength"
//#define NVSDK_NGX_Parameter_DeepDVC_SaturationBoost         "DeepDVC.SaturationBoost"
#define NVSDK_NGX_Parameter_DeepDVC_GetStatsCallback        "DeepDVC.GetStatsCallback"

void NGX_PopulateNgxParameters(NVSDK_NGX_Parameter** OutParameters, bool createNew)
{
	if (createNew) {
		*OutParameters = getNGXParameters();
	}

	auto InParams = *OutParameters;
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_Available, 1);
	InParams->Set("SuperSamplingDenoising.Available", 1);
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, 10);
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, 10);
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, 0);
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, 1);
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, 10);
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, 10);
	InParams->Set("SuperSamplingDenoising.NeedsUpdatedDriver", 0);
	InParams->Set("SuperSamplingDenoising.FeatureInitResult", 1);
	InParams->Set(NVSDK_NGX_Parameter_OptLevel, 0);
	InParams->Set(NVSDK_NGX_EParameter_OptLevel, 0);
	InParams->Set(NVSDK_NGX_Parameter_IsDevSnippetBranch, 0);
	InParams->Set(NVSDK_NGX_EParameter_IsDevSnippetBranch, 0);
	InParams->Set(NVSDK_NGX_Parameter_DLSS_Exposure_Scale, 1.0f);
	InParams->Set(NVSDK_NGX_EParameter_Sharpness, 0.0f); //
	InParams->Set(NVSDK_NGX_EParameter_MV_Scale_X, 1.0f); //
	InParams->Set(NVSDK_NGX_EParameter_MV_Scale_Y, 1.0f); //
	InParams->Set(NVSDK_NGX_EParameter_MV_Offset_X, 0.0f); //
	InParams->Set(NVSDK_NGX_EParameter_MV_Offset_Y, 0.0f); //
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
	InParams->Set("DLSSEnabler.Available", 1); //


	//InParams->Set("DFG.Available", useNvapiMock || isNvapiProxyLoaded ? 1 : 0); //
	InParams->Set("DFG.Available", 1); //
	InParams->Set("DFG.Enabled", ctx.ngx.isDynamicFrameGenerationEnabled ? 1 : 0); //

	InParams->Set("DLSSG.MVecJittered", 0); //
	if (ctx.ngx.isDeepDvcEnabled && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		InParams->Set(NVSDK_NGX_Parameter_DeepDVC_Available, 1);
		InParams->Set(NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMajor, 10);
		InParams->Set(NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMinor, 10);
		InParams->Set(NVSDK_NGX_Parameter_DeepDVC_FeatureInitResult, 1);
		InParams->Set(NVSDK_NGX_Parameter_DeepDVC_GetStatsCallback, NGX_DeepDvcCallback);
		InParams->Set(NVSDK_NGX_Parameter_SizeInBytes, 1024 * 1024);
	}
	else if (ctx.ngx.isDeepDvcEnabled == false) {
		InParams->Set(NVSDK_NGX_Parameter_DeepDVC_Available, 0);
		InParams->Set(NVSDK_NGX_Parameter_DeepDVC_FeatureInitResult, 0);
	}
}

bool NGX_HandleUnsupportedFeature(NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Handle** OutHandle)
{
	if (InFeatureID == NVSDK_NGX_Feature_DeepDVC && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		const auto handle = NGXHandle::Allocate(11);
		*OutHandle = (NVSDK_NGX_Handle*)handle;
		NGX_RegisterFeature(InFeatureID, *OutHandle);
		return true;
	}

	return false;
}

void NGX_CreateFeature(NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters)
{
	ctx.reflex.isReset = true;

	if (ctx.reflex.desiredFpsLimit > 0) {
		int frameRate = 0;
		InParameters->Get("FramerateLimit", &frameRate);

		if (frameRate == 0) {
			InParameters->Set("FramerateLimit", (int)ctx.reflex.desiredFpsLimit);
			InParameters->Set("DLSSEnabler.InternalFramerateLimit", (int)ctx.reflex.desiredFpsLimit);
		}
	}

	InParameters->Set("DLSSEnabler.InternalDFG.Enabled", (int)ctx.ngx.isDynamicFrameGenerationEnabled);

	int upscalerBackend = 10;
	InParameters->Get("DLSSEnabler.Dx12Backend", &upscalerBackend);

	if (upscalerBackend == 10) {
		// 0 = auto (XeSS)
		// 1 = FSR 2.2
		// 2 = FSR 2.1
		// 3 = DLSS
		// 4 = FSR 3.1
		if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_DLSS) {
			InParameters->Set("DLSSEnabler.Dx12Backend", 3);
		}
		else {
			InParameters->Set("DLSSEnabler.Dx12Backend", ctx.ngx.upscalingMethod == UPSCALING_METHOD_FSR ? 2 : (ctx.ngx.upscalingMethod == UPSCALING_METHOD_FSR22 ? 1 : (ctx.ngx.upscalingMethod == UPSCALING_METHOD_FSR31 ? 4 : 0)));
		}
	}

	upscalerBackend = 10;
	InParameters->Get("DLSSEnabler.VkBackend", &upscalerBackend);
	if (upscalerBackend == 10) {
		// 0 = auto (FSR 2.1)
		// 1 = FSR 2.2
		// 2 = DLSS
		// 3 = FSR 3.1
		if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_DLSS) {
			InParameters->Set("DLSSEnabler.VkBackend", 2);
		}
		else {
			InParameters->Set("DLSSEnabler.VkBackend", ctx.ngx.upscalingMethod == UPSCALING_METHOD_FSR || ctx.ngx.upscalingMethod == UPSCALING_METHOD_XESS ? 0 : (ctx.ngx.upscalingMethod == UPSCALING_METHOD_FSR22 ? 1 : (ctx.ngx.upscalingMethod == UPSCALING_METHOD_FSR31 ? 3 : 0)));
		}
	}

	if ((InFeatureID == 1 || InFeatureID == 3)) {
		LOG_INFO(L"[DLSS] Requesting OptiScaler to use " + ctx.ngx.upscalingMethod + L" upscaling backend");
	}

	if (!ctx.isProjectIdReported && ctx.ngx.upscalingMethod != UPSCALING_METHOD_DLSS
		&& ctx.engineType != NVSDK_NGX_ENGINE_TYPE_CUSTOM
		&& ctx.engineVersion.length() > 0) {
		// pass the project data to Optiscaler
		NVSDK_NGX_Application_Identifier appId;
		appId.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;

		// Initialize the ProjectIdDescription
		NVSDK_NGX_ProjectIdDescription projectDesc;
		projectDesc.ProjectId = ctx.projectId.c_str();
		projectDesc.EngineType = ctx.engineType;
		projectDesc.EngineVersion = ctx.engineVersion.c_str();

		// Assign the ProjectIdDescription to the union
		appId.v.ProjectDesc = projectDesc;
		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"Sending project information to the OptiScaler" + std::wstring());
		proxy_NVSDK_NGX_UpdateFeature(&appId, InFeatureID);
	}
}

void NGX_GetFeatureRequirements(NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
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

void NGX_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters)
{
	auto InParams = *OutParameters;
	if (ctx.ngx.overrideDlssUpscalerCapability) {
		InParams->Set(NVSDK_NGX_Parameter_SuperSampling_Available, (ctx.ngx.enableDlssUpscaler ? 1 : 0));
	}

	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, 10);
	InParams->Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, 10);
	InParams->Set("FrameGeneration.Available", 1);
}

static float GetPotentialFPS()
{
	if (ctx.ngx.frametimeHistory.size() == 0) {
		return 0;
	}

	// Calculate the index of the 70th percentile value
	std::deque<double> sortedQueue = ctx.ngx.frametimeHistory;

	std::sort(sortedQueue.begin(), sortedQueue.end());

	size_t index = static_cast<size_t>(0.90 * sortedQueue.size());

	auto latency = sortedQueue[index];

	if (latency == 0.0f) {
		return 0;
	}
	return 1000.0f / (float)latency;
}

std::wstring NGX_EngineToString(NVSDK_NGX_EngineType InEngineType)
{
	switch (InEngineType) {
	case NVSDK_NGX_ENGINE_TYPE_UNREAL:
		return L"Unreal";
	case NVSDK_NGX_ENGINE_TYPE_UNITY:
		return L"Unity";
	case NVSDK_NGX_ENGINE_TYPE_OMNIVERSE:
		return L"Omniverse";
	default:
		return L"Custom";
	}
}

void NGX_ReportUpscalerStats(NVSDK_NGX_Parameter* InParameters)
{
	if (!ngxRuntimeState.isUpscalerResolutionReported) {
		ngxRuntimeState.isUpscalerResolutionReported = true;
		unsigned int width = 0, height = 0, owidth = 0, oheight = 0, swidth = 0, sheight = 0;
		InParameters->Get(NVSDK_NGX_Parameter_Width, &width);
		InParameters->Get(NVSDK_NGX_Parameter_OutWidth, &owidth);
		InParameters->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, &swidth);
		InParameters->Get(NVSDK_NGX_Parameter_Height, &height);
		InParameters->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, &sheight);
		InParameters->Get(NVSDK_NGX_Parameter_OutHeight, &oheight);

		if (owidth > 0 || swidth > 0) {
			LOG_INFO(L"[DLSS] Initial upscaler resolution: " + std::to_wstring(width) + L"x" + std::to_wstring(height)
				+ L" => "
				+ std::to_wstring(swidth > owidth ? swidth : owidth) + L"x" + std::to_wstring(sheight > oheight ? sheight : oheight));
		}
		else {
			LOG_INFO(L"[DLSS] Using native resolution: " + std::to_wstring(width) + L"x" + std::to_wstring(height));
		}
	}
}

void NGX_EvaluateFeature(NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters)
{
	static int isThrottlingFg = 0;
	static std::deque<float> fpsHistory;

	if (fpsHistory.size() >= 10) {
		fpsHistory.pop_back();
	}

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
			LOG_INFO(L"Adjusting FPS limit to: " + std::to_wstring(frameRate));
			ctx.reflex.desiredFpsLimit = frameRate;
			ctx.reflex.realFpsLimit = (double)frameRate;
		}
	}

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
}

std::wstring NGX_FormatLogEntry(const std::string& message)
{
	std::string logEntry = message;
	// real NGX logs come like this:
	// [2024-06-21 17:32:07] [LaunchNGXUpdater:283] process for NGX Updater created successfully
	// so let's keep it simple...
	if (logEntry.starts_with("[20")) { // should do for the next 76 years...
		if (!ctx.logging.isUltraDebugEnabled) {
			return L"";
		}
		logEntry = logEntry.substr(logEntry.find_first_of("[", 1) + 1);
		logEntry = logEntry.substr(0, logEntry.find_first_of(":", 1) + 1) + logEntry.substr(logEntry.find_first_of("]", 1) + 1);

		// now this particular case...
		// [2024-06-21 18:16:54] [tid:40404][NGXCubinGeneric::SetGPUArch:421] m_gpuArch = 0x160
		if (logEntry.starts_with("tid:")) {
			logEntry = logEntry.substr(logEntry.find_first_of("[", 1) + 1);
			logEntry = logEntry.substr(0, logEntry.find_first_of("0123456789", 1) - 1) + ":" + logEntry.substr(logEntry.find_first_of(" ", logEntry.find_first_of("0123456789", 1) + 1));
		}

		if (logEntry.back() == '\n') {
			logEntry.pop_back();
		}

		if (logEntry.back() == '\r') {
			logEntry.pop_back();
		}

		return L"[NVNGX] " + ToWideString(logEntry);
	}

	return L"[OPTI] " + ToWideString(logEntry);
}

void NGX_Logger(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent)
{
	std::wstring msg = NGX_FormatLogEntry(std::string(message));
	if (msg != L"") {
		Console::Info(msg);
	}
}

void NGX_ReportDlssVersions()
{
	HMODULE nvngxDlssg = GetModuleHandleW(L"nvngx_dlssg.dll");
	HMODULE nvngxDlssd = GetModuleHandleW(L"nvngx_dlssd.dll");
	HMODULE nvngxDlss = GetModuleHandleW(L"nvngx_dlss.dll");

	if (nvngxDlss || nvngxDlssd || nvngxDlssg) {
		LOG_INFO(L"[NVNGX] NVNGX files detected:");
	}

	if (nvngxDlssg) {
		wchar_t buffer[MAX_PATH];
		DWORD length = GetModuleFileNameW(nvngxDlssg, buffer, MAX_PATH);
		std::wstring version = Common::GetFileVersion(buffer);

		if (length > 0) {
			LOG_INFO(L"[NVNGX]    DLSSG library version: " + version);
		}
	}

	if (nvngxDlssd) {
		wchar_t buffer[MAX_PATH];
		DWORD length = GetModuleFileNameW(nvngxDlssd, buffer, MAX_PATH);
		std::wstring version = Common::GetFileVersion(buffer);

		if (length > 0) {
			LOG_INFO(L"[NVNGX]    DLSSD library version: " + version);
		}
	}

	if (nvngxDlss) {
		wchar_t buffer[MAX_PATH];
		DWORD length = GetModuleFileNameW(nvngxDlss, buffer, MAX_PATH);
		std::wstring version = Common::GetFileVersion(buffer);

		if (length > 0) {
			LOG_INFO(L"[NVNGX]    DLSS library version: " + version);
		}
	}
}

void NGX_InitProjectReport(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion)
{
	static bool isEngineTypeStored = false;

	if (!isEngineTypeStored) {
		ctx.engineType = InEngineType;
		size_t length = std::strlen(InEngineVersion);
		ctx.engineVersion = std::string(InEngineVersion);
		isEngineTypeStored = true;
	}

	if (InProjectId) {
		std::string tmp = std::string(InProjectId);
		LOG_INFO(L"[NVNGX]    ProjectID: " + ToWideString(tmp));
		ctx.projectId = tmp.c_str();
	}

	LOG_INFO(L"[NVNGX]    Engine: " + NGX_EngineToString(InEngineType) + L" version " + ToWideString(ctx.engineVersion));
}

void NGX_InitReport(const wchar_t* InApplicationDataPath, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo** InFeatureInfo)
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

	// Copy existing paths
	for (unsigned int i = 0; i < (*InFeatureInfo)->PathListInfo.Length; ++i) {
		newPathArray[i] = (*InFeatureInfo)->PathListInfo.Path[i];
		LOG_INFO(L"[NVNGX]    Path included: " + std::wstring(newPathArray[i]));
	}

	// Add the new path
	std::wstring appDirPath = Common::GetProcessFilePath().parent_path().wstring() + L"\\";
	LOG_INFO(L"[NVNGX] Adding " + appDirPath + L" to the Path List Info structure");
	wchar_t* newPath = new wchar_t[appDirPath.length() + 1];
	wcscpy_s(newPath, appDirPath.length() + 1, appDirPath.c_str());
	newPathArray[(*InFeatureInfo)->PathListInfo.Length] = newPath;

	// Update PathListInfo
	(*InFeatureInfo)->PathListInfo.Path = newPathArray;
	(*InFeatureInfo)->PathListInfo.Length = newLength;

}
