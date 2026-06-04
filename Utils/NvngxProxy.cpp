#include "Common.h"
#include "NvngxCommon.h"
#include "NvngxProxy.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include <vulkan/vulkan_core.h>
#include "../Core/Context.h"
#include "VulkanProxy.h"
#include "Validator.h"
#include <memory>
#include "BackendManager.h"
#include "NgxState.h"
#include "DlssgProxy.h"
#include "NgxProvider.h"
#include "ProcResolver.h"
#include <unordered_set>
#include "../Includes/NvParamImp.h"
#include "GpuHelpers.h"
#include "DlssgLazyHook.h"

// Global singletons for the Dlss module (kept minimal and internal)
std::unique_ptr<DLSSG::DlssgProxy> dlssgModule;
std::unique_ptr<NGX::NgxProvider> ngxProvider;
std::unique_ptr<BackendManager> ngxBackends;
extern NgxRuntimeState ngxRuntimeState;
extern DefaultLogger globalLogger;
extern DefaultBackendLoader globalNgxLoader;

#define CALL_NGX_EXPORT_IMPL(ExportName) return reinterpret_cast<decltype(&ExportName)>(GetOriginalExportCached(#ExportName))
#define HOOK_NVNGX_FUNCTION(name) \
	if (strcmp(lpProcName, #name) == 0) { \
        if (!org_##name) { \
            org_##name = reinterpret_cast<decltype(&proxy_##name)>(OriginalGetProcAddress(hModule, lpProcName)); \
        } \
        return reinterpret_cast<FARPROC>(&proxy_##name); \
	}
#define DISALLOW_NVNGX_PROXY_MODE() \
	if (ctx.ngx.isProxyEnabled) { \
		LOG_NGX_ERROR(L"feature not implemented"); \
		LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_FAIL_NotImplemented); \
	}

static GetProcAddress_t OriginalGetProcAddress = GetProcAddress;

bool isEvalFeatReported = false;

VkDevice vkDevice2;
ID3D12Device* dx12Device;
ID3D11Device* dx11Device;
static int initCallsHandled = 0;

#define NVNGX_COUNT_INIT_CALLS() \
    initCallsHandled++; \
	if (initCallsHandled > 10) { \
		LOG_NGX_ERROR(L"potential infinite initialization loop detected"); \
		LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_FAIL_Denied); \
	}

static HMODULE GetUpscalerHandle(bool isSilentMode = false)
{
	static bool errorReported = false;
	static HMODULE hModule;

	if (hModule) {
		return hModule;
	}

	if (ctx.ngx.isDlssEnabled && !ctx.ngx.isProxyEnabled) {
		if (!errorReported && !isSilentMode) {
			errorReported = true;
			LOG_INFO(L"[DLSS] DLSS upscaler enabled, hardware support detected. Defaulting to hardware backend");
		}
		return nullptr;
	}

	hModule = LoadLibraryW(L"dlss-enabler-optiscaler.dll");
	if (ctx.ngx.isEmbeddedNgxUsed && hModule == nullptr) {
		hModule = Common::GetModuleHandle();
	}
	else {
		// @todo: fixme!
		ctx.ngx.isEmbeddedNgxUsed = true;
		hModule = LoadLibraryW(L"dlss-enabler-optiscaler.dll");
	}

	if (hModule == nullptr) {
		if (errorReported) {
			return hModule;
		}

		// Handle the case where the DLL is not loaded
		// You may want to log an error or handle it in an appropriate way
		if (ctx.ngx.overrideDlssUpscalerCapability && ctx.ngx.enableDlssUpscaler) {
			if (!ctx.ngx.isDlssSupportedByHardware && !isSilentMode) {
				errorReported = true;
				LOG_ERROR(L"[DLSS] DLSS upscaler enabled, but no valid backend detected. Defaulting to native resolution: application may become unstable");
			}
		}
	}

	return hModule;
}

static HMODULE GetFrameGeneratorHandle()
{
	static bool errorReported = false;
	static HMODULE hModule;

	if (ctx.ngx.isEmbeddedDlssgUsed) {
		return Common::GetModuleHandle();
	}

	if (hModule) {
		return hModule;
	}

	if (ctx.fsr3fgVersion == 0) {
		hModule = GetModuleHandle(L"dlssg_to_fsr3_amd_is_better-3.0.dll");
	}
	else {
		hModule = GetModuleHandle(L"dlssg_to_fsr3_amd_is_better.dll");
	}

	if (hModule == nullptr) {
		if (errorReported) {
			return hModule;
		}
		//errorReported = true;
		// Handle the case where the DLL is not loaded
		// You may want to log an error or handle it in an appropriate way
		LOG_ERROR(L"[NVNGX] DLSSG to FSR3 not loaded yet");
	}

	return hModule;
}

void SetOriginalGetProcAddress(GetProcAddress_t proc)
{
	OriginalGetProcAddress = proc;
	InstallOriginalGetProcAddress(OriginalGetProcAddress);

	// Detect fake NVIDIA driver and initialize DLSSG hooks as fallback
	static bool driverChecked = false;
	if (driverChecked) return;
	driverChecked = true;

	if (Validator::IsFakeNvidiaDriverDetected()) {
		InitializeDlssgHooks();
	}
}

static void NGX_ReportCurrentGPU(IDXGIAdapter* Adapter)
{
	static bool currentGpuReported = false;

	DXGI_ADAPTER_DESC currentGpuDescription;

	if (currentGpuReported) {
		return;
	}

	currentGpuReported = true;
	Adapter->GetDesc(&currentGpuDescription);
	ctx.gpu.refreshRate = GPU::GetRefreshRateByLUID(currentGpuDescription.AdapterLuid);
	ctx.gpu.deviceName = std::wstring(currentGpuDescription.Description);
	LOG_INFO(L"Current GPU:" + ctx.gpu.deviceName + (ctx.gpu.refreshRate <= 0 ? L" " : L" (Refresh rate: " + std::to_wstring(ctx.gpu.refreshRate) + L"hz)"));

	if (ctx.reflex.isVsyncEnabled) {
		//ctx.reflex.desiredFpsLimit = ctx.gpu.refreshRate - 1;
		//LOG_INFO(L"VSYNC feature enabled, limiting FPS to: " + std::to_wstring(ctx.gpu.refreshRate));
	}
}

static void CheckProcessNameForUpscalerAndFrameGeneration()
{
	typedef std::unordered_set<std::wstring> StringSet;
	StringSet xessBrokenGames;
	StringSet fsrBrokenGames;
	auto filePath = Common::GetProcessFilePath();
	std::wstring processName = filePath.filename().wstring();

	if (processName == L"BrightMemoryInfinite-Win64-Shipping.exe") {
		ctx.engineType = NVSDK_NGX_ENGINE_TYPE_UNREAL;
		ctx.engineVersion = "4.00";
	}

	if (processName == L"DH-Win64-Shipping.exe") {
		ctx.engineType = NVSDK_NGX_ENGINE_TYPE_UNREAL;
		ctx.engineVersion = "4.27";
	}

	if (ctx.ngx.configuredUpscalingMethod != UPSCALING_METHOD_AUTO) {
		return;
	}

	xessBrokenGames.insert(L"AlanWake2.exe");
	xessBrokenGames.insert(L"Starfield.exe");
	xessBrokenGames.insert(L"Banishers-Win64-Shipping.exe");
	fsrBrokenGames.insert(L"DD2.exe"); // Dragons Dogma 2

	if (processName == L"JediSurvivor.exe" && ctx.ngx.configuredFrameGenerationMethod == FRAMEGENERATION_METHOD_FSR3) {
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
		LOG_WARNING(L"[DLSS] Switching to FSR 3.0 frame generation due to incompatibility with " + processName);
	}

	if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_XESS && (xessBrokenGames.find(processName) != xessBrokenGames.end() || ctx.isVulkanApplication)) {
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
		LOG_WARNING(L"[DLSS] Switching from XeSS to FSR upscaler due to incompatibility with " + processName);

		return;
	}

	if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_FSR && fsrBrokenGames.find(processName) != fsrBrokenGames.end()) {
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_XESS;
		LOG_WARNING(L"[DLSS] Switching from FSR to XeSS upscaler due to incompatibility with " + processName);

		return;
	}

	if (ctx.ngx.configuredUpscalingMethod == UPSCALING_METHOD_AUTO) {
		if (processName == L"ImmortalsOfAveum-Win64-Shipping.exe" && ctx.ngx.upscalingMethod != UPSCALING_METHOD_FSR22 && ctx.ngx.upscalingMethod != UPSCALING_METHOD_DLSS) {
			ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR22;
			LOG_WARNING(L"[DLSS] Switching to FSR 2.2 upscaler due to incompatibility with " + processName);

			return;
		}

		if (processName == L"witcher3.exe" && ctx.ngx.upscalingMethod != UPSCALING_METHOD_DLSS) {
			ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR31;
			LOG_WARNING(L"[DLSS] Switching to FSR 3.1 upscaler due to incompatibility with " + processName);

			return;
		}

		if (processName == L"ReadyOrNot-Win64-Shipping.exe" && ctx.ngx.upscalingMethod != UPSCALING_METHOD_FSR22 && ctx.ngx.upscalingMethod != UPSCALING_METHOD_DLSS) {
			ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR22;
			LOG_WARNING(L"[DLSS] Switching to FSR 2.2 upscaler due to incompatibility with " + processName);

			return;
		}

		if (processName == L"DOOMEternalx64vk.exe" || processName == L"NMS.exe" || processName == L"bg3.exe" || processName == L"StreamlineSample.exe") {
			if (ctx.ngx.configuredUpscalingMethod == UPSCALING_METHOD_AUTO && ctx.currentGpuArchitecture <= NV_GPU_ARCHITECTURE_GP100) {
				ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
				ctx.ngx.isRealNgxHidden = true;
				ctx.isVulkanApplication = true;
				LOG_WARNING(L"[DLSS] Switching to FSR 2.1 upscaler due to incompatibility with " + processName);

				return;
			}
		}
	}
}

static bool InitializeUpscaler(NVSDK_NGX_EngineType InEngineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM, const char* InEngineVersion = nullptr)
{
	static bool isInitCompleted = false;

	if (isInitCompleted) {
		return false;
	}

	// if its Vulkan, unpack different settings
	if (ctx.isVulkanApplication) {
		ctx.ngx.configuredUpscalingMethod = ctx.ngx.configuredVkUpscalingMethod;
		ctx.ngx.upscalingMethod = ctx.ngx.configuredVkUpscalingMethod;
	}

	isInitCompleted = true;

	std::wstring upscalerFile = L"dlss-enabler-optiscaler.dll";
	std::wstring libXessFile = L"libxess.dll";
	std::wstring nvApiFile = L"nvapi64.dll";
	std::wstring upscalerVersion = L"";

	int detectedUpscalers = 0;
	if (Common::IsPluginPresent(upscalerFile) || ctx.ngx.isEmbeddedNgxUsed) {
		LOG_INFO(L"[DLSS] Upscaler backends detected: FSR 2.1, FSR 2.2, FSR 3.1" + std::wstring(ctx.isVulkanApplication ? L"" : L", XeSS 1.3") + std::wstring(ctx.realGpuArchitecture >= NV_GPU_ARCHITECTURE_GV100 && !ctx.nvapi.isProxyLoaded && !ctx.nvapi.isMockEnabled ? L", DLSS" : L" "));

		upscalerVersion = Common::GetPluginVersion(upscalerFile.c_str());

		detectedUpscalers += 3;
	}

	if ((Validator::Is_NVNGXDLLPresent() != 0 && Common::IsPluginPresent(nvApiFile)) || ctx.nvapi.isMockEnabled) {
		if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_DLSS) {
			LOG_WARNING(L"[DLSS] NVAPI proxy file detected - DLSS upscaler might be broken!");
		}
		else if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_AUTO) {
			auto deviceName = ctx.gpu.deviceName;
			if (deviceName.find(L"Radeon RX 7") != std::wstring::npos
				|| deviceName.find(L"Radeon RX 8") != std::wstring::npos
				|| deviceName.find(L"Radeon RX 9") != std::wstring::npos
				|| deviceName.find(L"Arc") != std::wstring::npos) {
				if (ctx.isVulkanApplication) {
					LOG_INFO(L"[DLSS] DP4a/XMX compatible GPU architecture detected, defaulting to FSR upscaler (XeSS unavailable)");
					ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
				}
				else {
					LOG_INFO(L"[DLSS] DP4a/XMX compatible GPU architecture detected, defaulting to XeSS upscaler");
					ctx.ngx.upscalingMethod = UPSCALING_METHOD_XESS;
				}
			}
		}

		if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_AUTO) {
			LOG_INFO(L"[DLSS] Unknown GPU architecture detected, defaulting to FSR upscaler");
			ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
		}
	}

	if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_DLSS) {
		LOG_INFO(L"[DLSS] Assuming that RTX card is present: native DLSS upscaler selected by the user");
	}

	if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_AUTO) {
		if (ctx.realGpuArchitecture == NV_GPU_ARCHITECTURE_TU100) {
			if (ctx.gpu.deviceName.find(L"GTX") != std::wstring::npos) {
				if (ctx.isVulkanApplication) {
					LOG_INFO(L"[DLSS] Turing GTX architecture detected: defaulting to FSR upscaler");
					ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
				}
				else {
					LOG_INFO(L"[DLSS] Turing GTX architecture detected: defaulting to XeSS upscaler");
					ctx.ngx.upscalingMethod = UPSCALING_METHOD_XESS;
				}
			}
			else {
				LOG_INFO(L"[DLSS] Turing or better RTX architecture detected: defaulting to native DLSS upscaler");
				ctx.ngx.upscalingMethod = UPSCALING_METHOD_DLSS;
			}
		}
		else if (ctx.realGpuArchitecture >= NV_GPU_ARCHITECTURE_GV100) {
			LOG_INFO(L"[DLSS] Turing or better RTX architecture detected: defaulting to native DLSS upscaler");
			ctx.ngx.upscalingMethod = UPSCALING_METHOD_DLSS;
		}
		else {
			if (ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_GP100) {
				LOG_INFO(L"[DLSS] Maxwell or older GPU architecture detected, defaulting to FSR upscaler");
				ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
			}
			else if (ctx.realGpuArchitecture == NV_GPU_ARCHITECTURE_GP100) {
				if (ctx.isVulkanApplication) {
					LOG_INFO(L"[DLSS] Pascal GTX architecture detected: defaulting to FSR upscaler (XeSS unavailable)");
					ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
				}
				else {
					LOG_INFO(L"[DLSS] Pascal GPU architecture detected, defaulting to XeSS upscaler");
					ctx.ngx.upscalingMethod = UPSCALING_METHOD_XESS;
				}
			}
			else if (ctx.nvapi.isMockEnabled) {
				LOG_INFO(L"[DLSS] NVAPI mock in use, defaulting to FSR upscaler");
				ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
			}
			else {
				LOG_INFO(L"[DLSS] Unknown GPU architecture detected, defaulting to FSR upscaler");
				ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
			}
		}
	}

	CheckProcessNameForUpscalerAndFrameGeneration();
	// check game incompatibilities
	if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_XESS && ctx.ngx.configuredUpscalingMethod == UPSCALING_METHOD_AUTO) {
		if (InEngineType == NVSDK_NGX_ENGINE_TYPE_UNREAL && InEngineVersion != nullptr && InEngineVersion[0] == '4') {
			// Unreal Engine 5 has some issues with XeSS
			ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
			LOG_WARNING(L"[DLSS] Switching from XeSS to FSR upscaler due to incompatibility with Unreal Engine 5");
		}
	}

	CheckProcessNameForUpscalerAndFrameGeneration();

	LOG_INFO(L"[DLSS] Loading upscaler backend: " + ctx.ngx.upscalingMethod + L" (Optiscaler version: " + upscalerVersion + L")");

	if (ctx.ngx.upscalingMethod == UPSCALING_METHOD_DLSS) {
		ctx.ngx.overrideDlssUpscalerCapability = false;
		ctx.ngx.enableDlssUpscaler = true;
	}

	if (detectedUpscalers == 0) {
		LOG_ERROR(L"[DLSS] Software upscaler failed to initialize: " + upscalerFile + L" file is missing!");
		LOG_ERROR(L"[DLSS] No software upscaler backend detected, upscaling unavailable!");
		if (ctx.isValidationOn) {
			Common::Error(L"Software upscaler failed to initialize: " + upscalerFile + L" file is missing", true);
		}
		return false;
	}

	return true;
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D11_GetCapabilityParameters(OutParameters);
	if (NVSDK_NGX_FAILED(result)) {
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}
	//*OutParameters = getNGXParameters();

	NGX_PopulateNgxParameters(OutParameters, false);

	NGX_GetCapabilityParameters(OutParameters);
	auto InParams = *OutParameters;
	InParams->Set("FrameGeneration.Available", 0);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_VULKAN_GetCapabilityParameters(OutParameters);
	if (NVSDK_NGX_FAILED(result)) {
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}
	//*OutParameters = getNGXParameters();

	LOG_NGX_INFO(L"Populating NGX parameters");
	NGX_PopulateNgxParameters(OutParameters, false);
	HMODULE fgHandle = GetFrameGeneratorHandle();

	if (fgHandle) {
		dlssgModule->PopulateParametersVulkan(*OutParameters);
	}

	NGX_GetCapabilityParameters(OutParameters);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D12_GetCapabilityParameters(OutParameters);
	if (NVSDK_NGX_FAILED(result)) {
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}
	//*OutParameters = getNGXParameters();

	LOG_NGX_INFO(L"Populating NGX parameters");
	NGX_PopulateNgxParameters(OutParameters, false);
	HMODULE fgHandle = GetFrameGeneratorHandle();

	if (fgHandle) {
		dlssgModule->PopulateParametersD3D12(*OutParameters);
	}

	NGX_GetCapabilityParameters(OutParameters);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D11_GetParameters(OutParameters);

	NGX_PopulateNgxParameters(OutParameters, false);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D12_GetParameters(OutParameters);
	//*OutParameters = getNGXParameters();

	NGX_PopulateNgxParameters(OutParameters, false);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_VULKAN_GetParameters(OutParameters);
	//*OutParameters = getNGXParameters();

	NGX_PopulateNgxParameters(OutParameters, false);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_AllocateParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D11_AllocateParameters(OutParameters);
	//*OutParameters = getNGXParameters();

	NGX_PopulateNgxParameters(OutParameters, false);

	NGX_GetCapabilityParameters(OutParameters);
	auto InParams = *OutParameters;
	InParams->Set("FrameGeneration.Available", 0);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_AllocateParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D12_AllocateParameters(OutParameters);
	//*OutParameters = getNGXParameters();

	NGX_PopulateNgxParameters(OutParameters, false);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_AllocateParameters(NVSDK_NGX_Parameter** OutParameters)
{
	LOG_NGX_FUNCTION_CALL();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_VULKAN_AllocateParameters(OutParameters);
	//*OutParameters = getNGXParameters();

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_DestroyParameters(NVSDK_NGX_Parameter* InParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D11_DestroyParameters(InParameters);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_DestroyParameters(NVSDK_NGX_Parameter* InParameters)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	result = org_NVSDK_NGX_D3D12_DestroyParameters(InParameters);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_DestroyParameters(NVSDK_NGX_Parameter* InParameters)
{
	LOG_NGX_FUNCTION_CALL();
	DISALLOW_NVNGX_PROXY_MODE();

	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_DestroyParameters(InParameters);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_CreateFeature(ID3D11DeviceContext* InCmdList, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(InFeatureID) + L" (" + NGX_FeatureIdToString(InFeatureID) + L")");

	if (NGX_HandleUnsupportedFeature(InFeatureID, OutHandle)) {
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	result = org_NVSDK_NGX_D3D11_CreateFeature(InCmdList, InFeatureID, InParameters, OutHandle);
	if (NVSDK_NGX_SUCCEED(result)) {
		ngxRuntimeState.isUpscalerResolutionReported = false;

		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring((*OutHandle)->Id));
	}

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_CreateFeature(void* InCmdBuffer, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(InFeatureID) + L" (" + NGX_FeatureIdToString(InFeatureID) + L")");

	NGX_CreateFeature(InFeatureID, InParameters);

	if (NGX_HandleUnsupportedFeature(InFeatureID, OutHandle)) {
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	if (InFeatureID == NVSDK_NGX_Feature_FrameGeneration && !ctx.ngx.isDlssgEnabled) {
		result = dlssgModule->CreateVulkan(InCmdBuffer, InFeatureID, InParameters, OutHandle);
	}
	else {
		LOG_NGX_INFO(L"proxied");
		result = org_NVSDK_NGX_VULKAN_CreateFeature(InCmdBuffer, InFeatureID, InParameters, OutHandle);
	}

	if (NVSDK_NGX_SUCCEED(result)) {
		NGX_RegisterFeature(InFeatureID, *OutHandle);

		if (InFeatureID == NVSDK_NGX_Feature_SuperSampling) {
			ngxRuntimeState.isUpscalerResolutionReported = false;
			//upscalerHandles.insert(*OutHandle);
			//upHandle = *OutHandle;
		}
		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring((*OutHandle)->Id));
	}

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_CreateFeature1(const VkDevice InDevice, void* InCmdBuffer, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(InFeatureID) + L" (" + NGX_FeatureIdToString(InFeatureID) + L")");

	NGX_CreateFeature(InFeatureID, InParameters);

	if (NGX_HandleUnsupportedFeature(InFeatureID, OutHandle)) {
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}


	if (InFeatureID == NVSDK_NGX_Feature_FrameGeneration && !ctx.ngx.isDlssgEnabled) {
		result = dlssgModule->CreateVulkan1(InDevice, InCmdBuffer, InFeatureID, InParameters, OutHandle);
	}
	else {
		LOG_NGX_INFO(L"proxied");
		result = org_NVSDK_NGX_VULKAN_CreateFeature1(InDevice, InCmdBuffer, InFeatureID, InParameters, OutHandle);
	}

	if (NVSDK_NGX_SUCCEED(result)) {
		NGX_RegisterFeature(InFeatureID, *OutHandle);

		if (InFeatureID == NVSDK_NGX_Feature_SuperSampling) {
			ngxRuntimeState.isUpscalerResolutionReported = false;
		}
		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring((*OutHandle)->Id));
	}

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_CreateFeature(ID3D12GraphicsCommandList* InCmdList, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	ctx.emulation.forceHighestArch = ctx.emulation.isHighestArch;
	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(InFeatureID) + L" (" + NGX_FeatureIdToString(InFeatureID) + L")");

	NGX_CreateFeature(InFeatureID, InParameters);

	if (NGX_HandleUnsupportedFeature(InFeatureID, OutHandle)) {
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	if (InFeatureID == NVSDK_NGX_Feature_FrameGeneration && !ctx.ngx.isDlssgEnabled) {
		result = dlssgModule->CreateD3D12(InCmdList, InFeatureID, InParameters, OutHandle);
	}
	else {

		LOG_NGX_INFO(L"proxied");
		result = org_NVSDK_NGX_D3D12_CreateFeature(InCmdList, InFeatureID, InParameters, OutHandle);
	}

	if (NVSDK_NGX_SUCCEED(result)) {
		NGX_RegisterFeature(InFeatureID, *OutHandle);
		if (InFeatureID == NVSDK_NGX_Feature_SuperSampling) {
			ngxRuntimeState.isUpscalerResolutionReported = false;
		}
		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring((*OutHandle)->Id));
	}

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_EvaluateFeature(ID3D11DeviceContext* InCmdList, NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback = NULL)
{
	static bool isDvcEvalFeatReported = false;

	// no ML for you...
	if (NGX_IsDeepDvcFeature(InFeatureHandle) && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		if (!isDvcEvalFeatReported) {
			LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring(InFeatureHandle->Id));
			isDvcEvalFeatReported = true;
			LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
		}

		return NVSDK_NGX_Result_Success;
	}

	if (!isEvalFeatReported) {
		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring(InFeatureHandle->Id));
		NGX_ReportUpscalerStats(InParameters);
		LOG_NGX_INFO(L"proxied");
	}

	if (NGX_IsSuperSamplingFeature(InFeatureHandle)) {

		if (ctx.enableReflexInjection) {
			//NvAPI_Sleep();
			ctx.reflex.evalId++;
		}
	}

	NVSDK_NGX_Result result = org_NVSDK_NGX_D3D11_EvaluateFeature(InCmdList, InFeatureHandle, InParameters, InCallback);
	if (!isEvalFeatReported) {
		isEvalFeatReported = true;
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	return result;
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_EvaluateFeature(void* InCmdList, NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback = NULL)
{
	NVSDK_NGX_Result result;
	static bool isDvcEvalFeatReported = false;

	// no ML for you...
	if (NGX_IsDeepDvcFeature(InFeatureHandle) && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		if (!isDvcEvalFeatReported) {
			LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring(InFeatureHandle->Id));
			isDvcEvalFeatReported = true;
			LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
		}

		return NVSDK_NGX_Result_Success;
	}

	if (!isEvalFeatReported) {
		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring(InFeatureHandle->Id));
	}

	if (NGX_IsSuperSamplingFeature(InFeatureHandle)) {
		if (ctx.enableReflexInjection) {
			//NvAPI_Sleep();
			ctx.reflex.evalId++;
		}
	}

	NGX_EvaluateFeature(InFeatureHandle, InParameters);

	if (NGX_IsFrameGenerationFeature(InFeatureHandle) && !ctx.ngx.isDlssgEnabled) {
		result = dlssgModule->EvaluateVulkan(InCmdList, InFeatureHandle, InParameters, InCallback);
	}
	else {
		if (!isEvalFeatReported) {
			LOG_NGX_INFO(L"proxied");
		}
		result = org_NVSDK_NGX_VULKAN_EvaluateFeature(InCmdList, InFeatureHandle, InParameters, InCallback);
	}

	if (!isEvalFeatReported) {
		isEvalFeatReported = true;
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	return result;
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList* InCmdList, NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback = NULL)
{
	NVSDK_NGX_Result result;
	static bool isDvcEvalFeatReported = false;

	// no ML for you...
	if (NGX_IsDeepDvcFeature(InFeatureHandle) && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		if (!isDvcEvalFeatReported) {
			LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring(InFeatureHandle->Id));
			isDvcEvalFeatReported = true;
			LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
		}

		return NVSDK_NGX_Result_Success;
	}

	if (!isEvalFeatReported) {
		LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring(InFeatureHandle->Id));
	}

	NGX_EvaluateFeature(InFeatureHandle, InParameters);

	if (!ctx.ngx.isDlssgEnabled && NGX_IsFrameGenerationFeature(InFeatureHandle)) {
		result = dlssgModule->EvaluateD3D12(InCmdList, InFeatureHandle, InParameters, InCallback);
	}
	else {
		if (NGX_IsSuperSamplingFeature(InFeatureHandle)) {
			if (ctx.enableReflexInjection) {
				//NvAPI_Sleep();
				ctx.reflex.evalId++;
			}
		}

		if (!isEvalFeatReported) {
			LOG_NGX_INFO(L"proxied");
		}
		result = org_NVSDK_NGX_D3D12_EvaluateFeature(InCmdList, InFeatureHandle, InParameters, InCallback);
	}

	if (!isEvalFeatReported) {
		isEvalFeatReported = true;
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	return result;
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_ReleaseFeature(NVSDK_NGX_Handle* InstanceHandle)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	isEvalFeatReported = false;

	// no ML for you...
	if (NGX_IsDeepDvcFeature(InstanceHandle) && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		NGX_UnregisterFeature(InstanceHandle);
		LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
	}

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring((InstanceHandle)->Id));
	result = org_NVSDK_NGX_D3D11_ReleaseFeature(InstanceHandle);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_ReleaseFeature(NVSDK_NGX_Handle* InstanceHandle)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	ctx.reflex.isReset = true;

	isEvalFeatReported = false;

	// no ML for you...
	if (NGX_IsDeepDvcFeature(InstanceHandle) && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		NGX_UnregisterFeature(InstanceHandle);
		LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
	}

	if (NGX_IsFrameGenerationFeature(InstanceHandle) && !ctx.ngx.isDlssgEnabled) {
		NGX_UnregisterFeature(InstanceHandle);
		result = dlssgModule->ReleaseVulkan(InstanceHandle);
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring((InstanceHandle)->Id));

	result = org_NVSDK_NGX_VULKAN_ReleaseFeature(InstanceHandle);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_ReleaseFeature(NVSDK_NGX_Handle* InstanceHandle)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	ctx.reflex.isReset = true;

	isEvalFeatReported = false;

	// no ML for you...
	if (NGX_IsDeepDvcFeature(InstanceHandle) && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		NGX_UnregisterFeature(InstanceHandle);
		LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
	}

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"ID: " + std::to_wstring((InstanceHandle)->Id));

	if (NGX_IsFrameGenerationFeature(InstanceHandle)) {
		ctx.reflex.realFpsLimit = (double)ctx.reflex.desiredFpsLimit;
		NGX_UnregisterFeature(InstanceHandle);

		if (!ctx.ngx.isDlssgEnabled) {
			result = dlssgModule->ReleaseD3D12(InstanceHandle);
			LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
		}
	}

	result = org_NVSDK_NGX_D3D12_ReleaseFeature(InstanceHandle);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Init_ProjectID(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
	const wchar_t* InApplicationDataPath, ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo = nullptr)
{
	NVNGX_COUNT_INIT_CALLS();
	dx11Device = InDevice; dx11Device->AddRef();
	InitializeUpscaler(InEngineType, InEngineVersion);

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	LOG_NGX_FUNCTION_CALL();

	NGX_InitReport(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
	NGX_InitProjectReport(InProjectId, InEngineType, InEngineVersion);

	ctx.isProjectIdReported = true;

	LOG_NGX_INFO(L"proxied");
	NVAPI_DISABLE_GPU_SPOOFING();
	result = org_NVSDK_NGX_D3D11_Init_ProjectID(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);

	NGX_ReportDlssVersions();

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init_ProjectID(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, void* InGIPA = nullptr, void* InGDPA = nullptr, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API)
{
	NVNGX_COUNT_INIT_CALLS();
	ctx.isVulkanApplication = true;
	InitializeUpscaler(InEngineType, InEngineVersion);
	vkDevice2 = InDevice;
	Vulkan_HookDeviceFunctions();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();

	ctx.isProjectIdReported = true;

	NGX_InitReport(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
	NGX_InitProjectReport(InProjectId, InEngineType, InEngineVersion);

	LOG_NGX_INFO(L"proxied");
	NVAPI_DISABLE_GPU_SPOOFING();
	result = org_NVSDK_NGX_VULKAN_Init_ProjectID(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InFeatureInfo, InSDKVersion);

	if (!ctx.ngx.isDlssgEnabled && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled)) {
		HMODULE fgHandle = GetFrameGeneratorHandle();
		if (fgHandle) {
			auto InParams = getNGXParameters();
			LOG_DEBUG(L"proxied to DLSSG-to-FSR3 mod");
			result = dlssgModule->InitVulkan(0x1227, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InFeatureInfo, InSDKVersion);
			if (NVSDK_NGX_FAILED(result)) {
				LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
			}
		}
	}

	NGX_ReportDlssVersions();

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Init_ProjectID(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
	const wchar_t* InApplicationDataPath, ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo = nullptr)
{
	NVNGX_COUNT_INIT_CALLS();
	dx12Device = InDevice; InDevice->AddRef();
	InitializeUpscaler(InEngineType, InEngineVersion);

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	LOG_NGX_FUNCTION_CALL();

	static bool isEngineTypeStored = false;

	ctx.isProjectIdReported = true;

	NGX_InitReport(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
	NGX_InitProjectReport(InProjectId, InEngineType, InEngineVersion);
	HMODULE fgHandle = GetFrameGeneratorHandle();

	if (fgHandle) {
		LOG_NGX_INFO(L"rerouted to NVSDK_NGX_D3D12_Init_Ext");
		auto InParams = getNGXParameters();
		result = dlssgModule->InitD3D12Ext(0x1337, InApplicationDataPath, InDevice, InSDKVersion, InParams);
		if (NVSDK_NGX_FAILED(result)) {
			LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
		}
	}

	LOG_NGX_INFO(L"proxied");
	NVAPI_DISABLE_GPU_SPOOFING();
	result = org_NVSDK_NGX_D3D12_Init_ProjectID(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
	NGX_ReportDlssVersions();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)
{
	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(InFeatureId));
	LOG_NGX_INFO(L"proxied");

	NVSDK_NGX_Result result = org_NVSDK_NGX_D3D11_GetScratchBufferSize(InFeatureId, InParameters, OutSizeInBytes);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)
{
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	HMODULE fgHandle = GetFrameGeneratorHandle();

	if (InFeatureId == NVSDK_NGX_Feature_FrameGeneration) {
		if (fgHandle) {
			result = dlssgModule->GetScratchBufferSizeD3D12(InFeatureId, InParameters, OutSizeInBytes);
			return result;
		}
	}

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(InFeatureId));
	result = org_NVSDK_NGX_D3D12_GetScratchBufferSize(InFeatureId, InParameters, OutSizeInBytes);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes)
{
	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(InFeatureId));

	DISALLOW_NVNGX_PROXY_MODE();

	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_GetScratchBufferSize(InFeatureId, InParameters, OutSizeInBytes);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Init(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API)
{
	NVNGX_COUNT_INIT_CALLS();
	dx11Device = InDevice; dx11Device->AddRef();
	InitializeUpscaler();

	LOG_NGX_FUNCTION_CALL();
	//NGX_InitReport(InApplicationDataPath, InSDKVersion, nullptr);

	LOG_NGX_INFO(L"proxied");
	NVAPI_DISABLE_GPU_SPOOFING();
	NVSDK_NGX_Result result = org_NVSDK_NGX_D3D11_Init(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion);
	NGX_ReportDlssVersions();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init_Ext(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
{
	NVNGX_COUNT_INIT_CALLS();
	ctx.isVulkanApplication = true;
	InitializeUpscaler();
	vkDevice2 = InDevice;
	Vulkan_HookDeviceFunctions();

	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();

	NGX_InitReport(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

	NVAPI_DISABLE_GPU_SPOOFING();
	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_Init_Ext(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InSDKVersion, InFeatureInfo);
	if (!ctx.ngx.isDlssgEnabled && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled)) {
		HMODULE fgHandle = GetFrameGeneratorHandle();
		if (fgHandle) {
			auto InParams = getNGXParameters();
			LOG_DEBUG(L"proxied to DLSSG-to-FSR3 mod");
			result = dlssgModule->InitVulkanExt(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InSDKVersion, InFeatureInfo);
			if (NVSDK_NGX_FAILED(result)) {
				LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
			}
		}
	}

	NGX_ReportDlssVersions();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init_Ext2(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, void* InGIPA, void* InGDPA, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
{
	NVNGX_COUNT_INIT_CALLS();
	ctx.isVulkanApplication = true;
	InitializeUpscaler();
	vkDevice2 = InDevice;
	Vulkan_HookDeviceFunctions();

	LOG_NGX_FUNCTION_CALL();

	NGX_InitReport(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

	NVAPI_DISABLE_GPU_SPOOFING();
	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_Init_Ext2(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InSDKVersion, InFeatureInfo);

	if (!ctx.ngx.isDlssgEnabled) {
		HMODULE fgHandle = GetFrameGeneratorHandle();
		if (fgHandle) {
			auto InParams = getNGXParameters();
			LOG_DEBUG(L"proxied to DLSSG-to-FSR3 mod");
			result = dlssgModule->InitVulkanExt2(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InSDKVersion, InFeatureInfo);
			if (NVSDK_NGX_FAILED(result)) {
				LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
			}
		}
	}

	NGX_ReportDlssVersions();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, void* InGIPA = nullptr, void* InGDPA = nullptr, const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API)
{
	NVNGX_COUNT_INIT_CALLS();
	ctx.isVulkanApplication = true;
	InitializeUpscaler();
	vkDevice2 = InDevice;
	Vulkan_HookDeviceFunctions();

	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();

	NGX_InitReport(InApplicationDataPath, InSDKVersion, nullptr);

	NVAPI_DISABLE_GPU_SPOOFING();
	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_Init(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InFeatureInfo, InSDKVersion);

	if (!ctx.ngx.isDlssgEnabled && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled)) {
		HMODULE fgHandle = GetFrameGeneratorHandle();
		if (fgHandle) {
			auto InParams = getNGXParameters();
			LOG_DEBUG(L"proxied to DLSSG-to-FSR3 mod");
			result = dlssgModule->InitVulkan(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InFeatureInfo, InSDKVersion);
			if (NVSDK_NGX_FAILED(result)) {
				LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
			}
		}
	}

	NGX_ReportDlssVersions();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Init(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API)
{
	//forceHighestArch = false;
	NVNGX_COUNT_INIT_CALLS();
	dx12Device = InDevice; InDevice->AddRef();
	InitializeUpscaler();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	LOG_NGX_FUNCTION_CALL();
	NGX_InitReport(InApplicationDataPath, InSDKVersion, nullptr);

	HMODULE fgHandle = GetFrameGeneratorHandle();
	NGX_ReportDlssVersions();

	if (fgHandle) {
		result = dlssgModule->InitD3D12(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion);
		if (NVSDK_NGX_FAILED(result)) {
			LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
		}
	}

	NVAPI_DISABLE_GPU_SPOOFING();
	result = org_NVSDK_NGX_D3D12_Init(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion);
	NGX_ReportDlssVersions();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Init_Ext(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo = nullptr)
{
	NVNGX_COUNT_INIT_CALLS();
	dx11Device = InDevice; dx11Device->AddRef();
	InitializeUpscaler();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	LOG_NGX_FUNCTION_CALL();
	NGX_InitReport(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

	LOG_NGX_INFO(L"proxied");
	NVAPI_DISABLE_GPU_SPOOFING();
	if (!org_NVSDK_NGX_D3D11_Init_Ext) {
		LOG_NGX_ERROR(L"[DLSS] No D3D11 proxy available!");
		result = NVSDK_NGX_Result_FAIL_NotImplemented;
	}
	else {
		result = org_NVSDK_NGX_D3D11_Init_Ext(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
	}
	NGX_ReportDlssVersions();

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_RequiredExtensions(unsigned int* OutInstanceExtCount, const char*** OutInstanceExts, unsigned int* OutDeviceExtCount, const char*** OutDeviceExts)
{
	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();

	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_RequiredExtensions(OutInstanceExtCount, OutInstanceExts, OutDeviceExtCount, OutDeviceExts);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Init_Ext(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo = nullptr)
{
	NVNGX_COUNT_INIT_CALLS();
	dx12Device = InDevice; InDevice->AddRef();
	InitializeUpscaler();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	LOG_NGX_FUNCTION_CALL();
	NGX_InitReport(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

	HMODULE fgHandle = GetFrameGeneratorHandle();
	NGX_ReportDlssVersions();

	ctx.applicationId = InApplicationId;

	if (false) {
		LOG_NGX_INFO(L"rerouted to DLSS/DLSSG");

		auto InParams = getNGXParameters();
		result = dlssgModule->InitD3D12Ext(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion, InParams);
		//forceHighestArch = true;
		if (NVSDK_NGX_FAILED(result)) {
			LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
		}
	}

	//NVAPI_DISABLE_GPU_SPOOFING();
	ctx.emulation.forceHighestArch = ctx.emulation.isHighestArch;

	result = org_NVSDK_NGX_D3D12_Init_Ext(InApplicationId, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
	NGX_ReportDlssVersions();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Shutdown1(ID3D11Device* D3DDevice)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	LOG_NGX_INFO(L"proxied");
	result = org_NVSDK_NGX_D3D11_Shutdown1(D3DDevice);

	if (dx11Device == D3DDevice) {
		dx11Device->Release();
		dx11Device = nullptr;
	}

	NVAPI_ENABLE_GPU_SPOOFING();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Shutdown1(ID3D12Device* D3DDevice)
{
	LOG_NGX_FUNCTION_CALL();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	if (GetFrameGeneratorHandle()) {
		result = dlssgModule->ShutdownD3D12_1(D3DDevice);
	}

	LOG_NGX_INFO(L"proxied");
	result = org_NVSDK_NGX_D3D12_Shutdown1(D3DDevice);

	if (dx12Device == D3DDevice) {
		dx12Device->Release();
		dx12Device = nullptr;
	}

	NVAPI_ENABLE_GPU_SPOOFING();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Shutdown1(void* Device)
{
	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();

	LOG_NGX_INFO(L"proxied");
	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_Shutdown1(Device);

	vkDevice2 = nullptr;

	NVAPI_ENABLE_GPU_SPOOFING();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Shutdown()
{
	LOG_NGX_FUNCTION_CALL();

	LOG_NGX_INFO(L"proxied");
	NVSDK_NGX_Result result = org_NVSDK_NGX_D3D11_Shutdown();

	if (dx11Device) {
		dx11Device->Release();
		dx11Device = nullptr;
	}

	NVAPI_ENABLE_GPU_SPOOFING();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Shutdown()
{
	LOG_NGX_FUNCTION_CALL();

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	if (GetFrameGeneratorHandle()) {
		result = dlssgModule->ShutdownD3D12();
	}

	LOG_NGX_INFO(L"proxied");
	result = org_NVSDK_NGX_D3D12_Shutdown();
	if (dx12Device) {
		dx12Device->Release();
		dx12Device = nullptr;
	}

	NVAPI_ENABLE_GPU_SPOOFING();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Shutdown()
{
	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	if (GetFrameGeneratorHandle()) {
		result = dlssgModule->ShutdownVulkan();
	}

	LOG_NGX_INFO(L"proxied");
	result = org_NVSDK_NGX_VULKAN_Shutdown();
	vkDevice2 = nullptr;
	NVAPI_ENABLE_GPU_SPOOFING();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(void* Instance,
	void* PhysicalDevice,
	const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo,
	uint32_t* OutExtensionCount,
	void** OutExtensionProperties)
{
	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();

	if (FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature_FrameGeneration && !ctx.ngx.isDlssgEnabled) {
		*OutExtensionCount = 0;
		LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
	}

	LOG_NGX_INFO(L"proxied");
	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(Instance, PhysicalDevice, FeatureDiscoveryInfo, OutExtensionCount, OutExtensionProperties);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo,
	uint32_t* OutExtensionCount,
	void** OutExtensionProperties)
{
	LOG_NGX_FUNCTION_CALL();

	DISALLOW_NVNGX_PROXY_MODE();

	if (FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature_FrameGeneration && !ctx.ngx.isDlssgEnabled) {
		*OutExtensionCount = 0;
		LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
	}

	LOG_NGX_INFO(L"proxied");
	NVSDK_NGX_Result result = org_NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(FeatureDiscoveryInfo, OutExtensionCount, OutExtensionProperties);
	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_UpdateFeature(const NVSDK_NGX_Application_Identifier* ApplicationId, const NVSDK_NGX_Feature FeatureID)
{
	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(FeatureID) + L" (" + NGX_FeatureIdToString(FeatureID) + L")");

	if (!ctx.ngx.isProxyEnabled) {
		LOG_NGX_INFO(L"proxied");
		org_NVSDK_NGX_UpdateFeature(ApplicationId, FeatureID);
	}

	LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetFeatureRequirements(
	IDXGIAdapter* Adapter,
	NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
{
	NGX_ReportCurrentGPU(Adapter);

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(FeatureDiscoveryInfo->FeatureID) + L" (" + NGX_FeatureIdToString(FeatureDiscoveryInfo->FeatureID) + L")");
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	NGX_GetFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

	if (FeatureDiscoveryInfo->FeatureID != NVSDK_NGX_Feature::NVSDK_NGX_Feature_SuperSampling) {
		RequirementInfo->FeatureSupported = NVSDK_NGX_FeatureSupportResult_AdapterUnsupported;
		LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
	}

	RequirementInfo->FeatureSupported = NVSDK_NGX_FeatureSupportResult_Supported;
	RequirementInfo->MinHWArchitecture = 10;
	strcpy_s(RequirementInfo->MinOSVersion, "10.0.0.0");

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetFeatureRequirements(
	IDXGIAdapter* Adapter,
	NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
{
	NGX_ReportCurrentGPU(Adapter);

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(FeatureDiscoveryInfo->FeatureID) + L" (" + NGX_FeatureIdToString(FeatureDiscoveryInfo->FeatureID) + L")");
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	NGX_GetFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetFeatureRequirements(
	void* Arg1,
	void* Arg2,
	NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
{
	//ReportCurrentGPU(Adapter);

	LOG_NGX_FUNCTION_CALL_WITH_ARG(L"FeatureID: " + std::to_wstring(FeatureDiscoveryInfo->FeatureID) + L" (" + NGX_FeatureIdToString(FeatureDiscoveryInfo->FeatureID) + L")");
	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;

	NGX_GetFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

	LOG_NGX_FUNCTION_CALL_AND_RETURN(result);
}

NVSDK_NGX_Result proxy_NVSDK_NGX_DummyCall()
{
	LOG_NGX_FUNCTION_CALL();
	LOG_NGX_FUNCTION_CALL_AND_RETURN(NVSDK_NGX_Result_Success);
}

FARPROC WINAPI DetourNgx(HMODULE hModule, LPCSTR lpProcName)
{
	GPU::ListGPUs();
	std::string procName = "";
	procName = std::string(lpProcName);

	if (!GetUpscalerHandle(true)) {
		LoadLibraryW(L"dlss-enabler-upscaler.dll");
	}
	hModule = GetUpscalerHandle();
	if (!hModule) {
		LOG_ERROR(L"[NVNGX] Failed to load NVNGX upscaler");
	}

	if (!org_NVSDK_NGX_UpdateFeature) {
		org_NVSDK_NGX_UpdateFeature = reinterpret_cast<decltype(&proxy_NVSDK_NGX_UpdateFeature)>(OriginalGetProcAddress(hModule, "NVSDK_NGX_UpdateFeature"));
	}

	if (ctx.logging.isExtraDebugEnabled) {
		LOG_WARNING(L"[NVNGX] " + ToWideString(procName) + L": call forwarded");
	}

	if (procName == "NVSDK_NGX_D3D12_GetFeatureRequirements") {
		//LOG_WARNING(L"[NVNGX] " + ToWideString(procName) + L": call hijacked");
		//return reinterpret_cast<FARPROC>(&proxy_NVSDK_NGX_D3D12_GetFeatureRequirements);
	}

	return OriginalGetProcAddress(hModule, lpProcName);
}