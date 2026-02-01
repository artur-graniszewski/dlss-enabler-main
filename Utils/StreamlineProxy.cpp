#include <wtypes.h>
#include <cstdint>
#include <d3d12.h>

#include <string>
#include "Common.h"
#include "../Detours/detours.h"
#include "../Core/Context.h"
#include "StreamlineProxy.h"

HMODULE GetCustomInterposer()
{
	auto static result = LoadLibraryA("my.interposer.dll");

	return result;

}

// Define the pointer to the original slInit function
extern SLINIT original_slInit;
extern SLFEATINIT original_slIsFeatureLoaded;
extern SL3ARGS original_slIsFeatureSupported;
extern SL0ARGS original_slShutdown;
extern DLSSGSETFEATLOADED original_slSetFeatureLoaded;
extern SL4ARGS original_D3D12CreateDevice;
extern SL4ARGS original_slHookPresent;
extern SL5ARGS original_slHookPresent1;
extern SL4ARGS original_D3D12CreateRootSignatureDeserializer;
extern SL4ARGS original_D3D12CreateVersionedRootSignatureDeserializer;
extern SL4ARGS original_D3D12SerializeRootSignature;
extern SL3ARGS original_slSetConstants;
extern SLGETFEATREQ original_slGetFeatureRequirements;
extern SL2ARGS original_slFreeResources;
extern SL2ARGS original_D3D12GetDebugInterface;
extern SL2ARGS original_slGetFeatureVersion;
extern SL2ARGS original_slGetNewFrameToken;
extern SL2ARGS original_slGetNativeInterface;
//SL3ARGS original_slGetFeatureFunction = nullptr;
extern SLGETFEATFUNC original_slGetFeatureFunction;
extern SL3ARGS original_slAllocateResources;
extern SLFEATUREARGS original_slEvaluateFeature;
extern SL1ARG original_slUpgradeInterface;
extern SL1ARG original_slSetD3DDevice;
extern SL1ARG original_slSetVulkanInfo ;
extern SL2ARGS original_CreateDXGIFactory ;
extern SL2ARGS original_CreateDXGIFactory1;
extern SL3ARGS original_CreateDXGIFactory2;
extern SL3ARGS original_D3D12SerializeVersionedRootSignature;
extern SL3ARGS original_D3D12GetInterface;
extern SLSETTAG original_slSetTag;
extern SLSETTAGV1 original_slSetTagV1;
extern SL2ARGS original_slDLSSSetOptions;
extern DLSSGSETOPTS original_slDLSSGSetOptions;
extern SL2ARGS original_slDLSSSetOptions;
extern DLSSGSETFEATLOADED original_slSetFeatureLoaded2;
extern DEEPDVCSETOPTS original_slDeepDVCSetOptions;

static void* currentViewPort;

// Define the detoured slInit function
int detoured_slIsFeatureLoaded(void* arg, bool& loaded)
{
	//Console::Warning(std::wstring(L"========= ") + __FUNCTIONW__);
	SLFEATINIT tmp = (SLFEATINIT)GetProcAddress(GetCustomInterposer(), "slIsFeatureLoaded");

	int result = tmp(arg, loaded);

	return result;
}

// sl::Result slSetTag(const sl::ViewportHandle& viewport, const sl::ResourceTag* resources, uint32_t numResources, sl::CommandBuffer* cmdBuffer)
int detoured_slSetTag(void* arg, void* arg2, void* arg3, void* arg4)
{
	//Console::Warning(std::wstring(L"========= ") + __FUNCTIONW__);
	SL4ARGS tmp = (SL4ARGS)GetProcAddress(GetCustomInterposer(), "slSetTag");

	int result = tmp(arg, arg2, arg3, arg4);

	return result;
}

int detoured_slGetFeatureRequirements(void* arg, FeatureRequirements& requirements)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	//SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "slGetFeatureRequirements");

	//HRESULT result = tmp(arg, arg2);
	HRESULT result = original_slGetFeatureRequirements(arg, requirements);

	static bool isHagsReported = false;
	static bool isVsyncReported = false;
	if (static_cast<uint32_t>(requirements.flags) & static_cast<uint32_t>(FeatureRequirementFlags::eVSyncOffRequired)) {

		if (!isVsyncReported) {
			LOG_INFO(L"[STREAMLINE] NVNGX feature requires VSYNC to be always off, invalidating this requirement");
			isVsyncReported = true;
		}
		// Cast the flags to uint32_t, disable the eVSyncOffRequired flag, and cast back
		requirements.flags = static_cast<FeatureRequirementFlags>(
			static_cast<uint32_t>(requirements.flags) & ~static_cast<uint32_t>(FeatureRequirementFlags::eVSyncOffRequired)
			);
	}

	if (static_cast<uint32_t>(requirements.flags) & static_cast<uint32_t>(FeatureRequirementFlags::eHardwareSchedulingRequired)) {

		if (!isHagsReported) {
			LOG_INFO(L"[STREAMLINE] NVNGX feature requires Hardware Accelerated GPU Scheduling to be on, invalidating this requirement");
			isHagsReported = true;
		}
		requirements.flags = static_cast<FeatureRequirementFlags>(
			static_cast<uint32_t>(requirements.flags) & ~static_cast<uint32_t>(FeatureRequirementFlags::eHardwareSchedulingRequired)
			);
	}

	return result;
}

int detoured_slSetTagForCyberpunkFixed(void* arg, const ResourceTag* resources, uint32_t numResources, void* arg4)
{
	static bool reported = false;
	for (uint32_t i = 0; i < numResources; i++) {
		auto tag = &resources[i];

		while (tag != nullptr) {
			if (tag->type == kBufferTypeHUDLessColor
				&&
				tag->resource->state == (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				) {
				if (!reported) {
					LOG_DEBUG(L"[STREAMLINE] slSetTag: Wrong resource state detected, applying fix!");
					reported = true;
				}
				tag->resource->state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			}

			tag = findStruct<ResourceTag>(tag->next);
		}
	}

	if (GetCustomInterposer()) {
		SLSETTAG tmp = (SLSETTAG)GetProcAddress(GetCustomInterposer(), "slSetTag");
		return tmp(arg, resources, numResources, arg4);
	}

	int result = original_slSetTag(arg, resources, numResources, arg4);

	return result;
}

//int detoured_slSetTagForWitcherFixed(void* arg, const ResourceTag* resources, uint32_t numResources, void* arg4)
int detoured_slSetTagForWitcherFixed(const ResourceV1* resource, BufferType tag, uint32_t id, void* arg4)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	ID3D12Resource* dxRes = static_cast<ID3D12Resource*>(resource->native);

	D3D12_RESOURCE_DESC resourceDesc = dxRes->GetDesc();

	UINT64 width = resourceDesc.Width;
	UINT height = resourceDesc.Height;
	LOG_WARNING(L"** " + std::to_wstring(tag) + L" : " + std::to_wstring(width) + L"x" + std::to_wstring(height));


	int result = original_slSetTagV1(resource, tag, id, arg4);

	return result;
}

int detoured_slHookPresent(void* arg, void* arg2, void* arg3, void* arg4)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);

	int result = original_slHookPresent(arg, arg2, arg3, arg4);

	return result;
}

int detoured_slHookPresent1(void* arg, void* arg2, void* arg3, void* arg4, void *arg5)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);

	int result = original_slHookPresent1(arg, arg2, arg3, arg4, arg5);

	return result;
}

#ifdef STREAMLINE_DETOUR
int detoured_D3D12CreateDevice(void* arg, void* arg2, void* arg3, void* arg4)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL4ARGS tmp = (SL4ARGS)GetProcAddress(GetCustomInterposer(), "D3D12CreateDevice");

	int result = tmp(arg, arg2, arg3, arg4);
	//LOG_WARNING(std::wstring(L"==OK===== ") + __FUNCTIONW__);
	return result;
}

int detoured_D3D12CreateRootSignatureDeserializer(void* arg, void* arg2, void* arg3, void* arg4)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL4ARGS tmp = (SL4ARGS)GetProcAddress(GetCustomInterposer(), "D3D12CreateRootSignatureDeserializer");

	int result = tmp(arg, arg2, arg3, arg4);

	return result;
}

int detoured_D3D12SerializeRootSignature(void* arg, void* arg2, void* arg3, void* arg4)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL4ARGS tmp = (SL4ARGS)GetProcAddress(GetCustomInterposer(), "D3D12SerializeRootSignature");

	int result = tmp(arg, arg2, arg3, arg4);

	return result;
}

int detoured_D3D12CreateVersionedRootSignatureDeserializer(void* arg, void* arg2, void* arg3, void* arg4)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL4ARGS tmp = (SL4ARGS)GetProcAddress(GetCustomInterposer(), "D3D12CreateVersionedRootSignatureDeserializer");

	int result = tmp(arg, arg2, arg3, arg4);

	return result;
}

int detoured_D3D12GetDebugInterface(void* arg, void* arg2)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "D3D12GetDebugInterface");

	int result = tmp(arg, arg2);

	return result;
}

int detoured_slSetConstants(void* arg, void* arg2, void* arg3)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL3ARGS tmp = (SL3ARGS)GetProcAddress(GetCustomInterposer(), "slSetConstants");

	int result = tmp(arg, arg2, arg3);

	return result;
}

int detoured_D3D12GetInterface(void* arg, void* arg2, void* arg3)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL3ARGS tmp = (SL3ARGS)GetProcAddress(GetCustomInterposer(), "D3D12GetInterface");

	int result = tmp(arg, arg2, arg3);

	return result;
}

int detoured_slAllocateResources(void* arg, void* arg2, void* arg3)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL3ARGS tmp = (SL3ARGS)GetProcAddress(GetCustomInterposer(), "slAllocateResources");

	int result = tmp(arg, arg2, arg3);

	return result;
}

//int detoured_slGetFeatureFunction(void* arg, void* arg2, void* arg3)
//{
//	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
//	SL3ARGS tmp = (SL3ARGS)GetProcAddress(GetCustomInterposer(), "slGetFeatureFunction");
//
//	int result = tmp(arg, arg2, arg3);
//
//	return result;
//}

int detoured_slGetNativeInterface(void* arg, void* arg2)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "slGetNativeInterface");

	int result = tmp(arg, arg2);

	return result;
}

int detoured_slFreeResources(void* arg, void* arg2)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "slFreeResources");

	int result = tmp(arg, arg2);

	return result;
}

int detoured_slGetFeatureVersion(void* arg, void* arg2)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "slGetFeatureVersion");

	int result = tmp(arg, arg2);

	return result;
}

int detoured_slGetNewFrameToken(void* arg, void* arg2)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "slGetNewFrameToken");

	int result = tmp(arg, arg2);

	return result;
}

int detoured_slUpgradeInterface(void* arg)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL1ARG tmp = (SL1ARG)GetProcAddress(GetCustomInterposer(), "slUpgradeInterface");

	int result = tmp(arg);

	return result;
}

int detoured_slSetVulkanInfo(void* arg)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL1ARG tmp = (SL1ARG)GetProcAddress(GetCustomInterposer(), "slSetVulkanInfo");

	int result = tmp(arg);

	return result;
}

int detoured_slSetD3DDevice(void* arg)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL1ARG tmp = (SL1ARG)GetProcAddress(GetCustomInterposer(), "slSetD3DDevice");

	HRESULT result = tmp(arg);

	return result;
}


HRESULT WINAPI detoured_CreateDXGIFactory(void* arg, void* arg2)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "CreateDXGIFactory");

	HRESULT result = tmp(arg, arg2);

	return result;
}

HRESULT WINAPI detoured_CreateDXGIFactory1(void* arg, void* arg2)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL2ARGS tmp = (SL2ARGS)GetProcAddress(GetCustomInterposer(), "CreateDXGIFactory1");

	HRESULT result = tmp(arg, arg2);

	return result;
}

HRESULT WINAPI detoured_CreateDXGIFactory2(void* arg, void* arg2, void* arg3)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL3ARGS tmp = (SL3ARGS)GetProcAddress(GetCustomInterposer(), "CreateDXGIFactory2");

	HRESULT result = tmp(arg, arg2, arg3);
	//LOG_WARNING(std::wstring(L"==OK===== ") + __FUNCTIONW__);

	return result;
}

HRESULT WINAPI detoured_D3D12SerializeVersionedRootSignature(void* arg, void* arg2, void* arg3)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL3ARGS tmp = (SL3ARGS)GetProcAddress(GetCustomInterposer(), "D3D12SerializeVersionedRootSignature");

	HRESULT result = tmp(arg, arg2, arg3);
	//LOG_WARNING(std::wstring(L"==OK===== ") + __FUNCTIONW__);

	return result;
}

int detoured_slShutdown()
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	SL0ARGS tmp = (SL0ARGS)GetProcAddress(GetCustomInterposer(), "slShutdown");

	int result = tmp();

	return result;
}

int detoured_slIsFeatureSupported(void* arg, void* arg2, void* arg3)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);

	// Call the original slInit function
	SL3ARGS tmp = (SL3ARGS)GetProcAddress(GetCustomInterposer(), "slIsFeatureSupported");

	//int result = original_slInit(arg, sdkVersion);
	int result = tmp(arg, arg2, arg3);

	// Additional code you want to execute before or after the original function

	return result;
}

#endif

void LogMessageCallback(LogType type, const char* msg)
{
	if (!ctx.logging.isStreamlineDebugEnabled) {
		return;
	}
	return;
	std::wstring logEntry = ToWideString(std::string(msg));

	// find the last bracket (surrounding function name)
	size_t pos1 = logEntry.find(L"] ");
	
	// get the actual message after function name
	auto message = logEntry.substr(pos1 + 2);
	size_t pos0 = logEntry.rfind(L"[", pos1);
	auto functionName = logEntry.substr(pos0 + 1, pos1 - pos0 - 1);
	logEntry = logEntry.substr(0, pos0);
	pos0 = logEntry.rfind(L"]", pos0);
	auto fileName = logEntry.substr(pos0 + 1, pos1 - pos0);
	pos0 = fileName.find(L":");
	auto fileLineNumber = fileName.substr(pos0 + 1);
	fileName = fileName.substr(0, pos0);
	pos0 = fileName.find(L".");
	auto fileBaseName = fileName.substr(0, pos0);

	// remove newlines
	message.erase(message.find_last_not_of(L"\n\r") + 1);
	message.erase(message.find_last_not_of(L"\n") + 1);

	if (ctx.logging.isUltraDebugEnabled) {
		logEntry = fileName + L")::" + functionName + L": " + message;
	}
	else {
		logEntry = fileBaseName + L"::" +
			functionName + L": " + message;
	}

	logEntry = L"[STREAMLINE] " + logEntry;
	switch (type) {
	case LogType::eError:
		LOG_ERROR(logEntry);
		break;
	case LogType::eWarn:
		LOG_WARNING(logEntry);
		break;
	case LogType::eInfo:
		LOG_INFO(logEntry);
		break;
	}
}

static void LoadFeature(Feature* featuresToLoad, size_t& numFeatures, const Feature* newFeatureList, size_t newListSize, size_t maxFeatures, Feature featureToAdd) 
{
	// Ensure we don't exceed the maxFeatures limit
	size_t featuresToCopy = min(newListSize, maxFeatures);

	// Copy the new features into the featuresToLoad array
	std::copy(newFeatureList, newFeatureList + featuresToCopy, featuresToLoad);

	// Update the number of features
	numFeatures = featuresToCopy;

	// Check if kFeatureDeepDVC is present in the new list, if not, replace the last element with it
	if (std::find(featuresToLoad, featuresToLoad + numFeatures, featureToAdd) == (featuresToLoad + numFeatures) && numFeatures < maxFeatures) {
		// If kFeatureDeepDVC is missing, replace the last element with it
		featuresToLoad[numFeatures] = featureToAdd;
		//featuresToLoad[numFeatures] = kFeatureDLSS_G;
	}
}

// Define the detoured slInit function
int detoured_slInit(Preferences& pref, uint64_t sdkVersion)
{
	LOG_INFO(L"[STREAMLINE] Initializing Streamline library");

	//pref.flags = static_cast<PreferenceFlags>((uint64_t)pref.flags | (uint64_t)PreferenceFlags::eUseDXGIFactoryProxy);
	//pref.flags = static_cast<PreferenceFlags>((uint64_t)pref.flags & ~(uint64_t)PreferenceFlags::eUseDXGIFactoryProxy);
	pref.flags = static_cast<PreferenceFlags>((uint64_t)pref.flags & ~(uint64_t)PreferenceFlags::eAllowOTA);
	pref.flags = static_cast<PreferenceFlags>((uint64_t)pref.flags & ~(uint64_t)PreferenceFlags::eLoadDownloadedPlugins);
	//pref.showConsole = true;
	pref.logLevel = LogLevel::eDefault;
	//pref.logLevel = LogLevel::eVerbose;
	pref.logMessageCallback = LogMessageCallback;

	if (ctx.streamline.forceLoadDeepDvc) {
		LOG_WARNING(L"[STREAMLINE] Forcefull load of DeepDVC");
		const size_t maxFeatures = 10;
		Feature featuresToLoad[maxFeatures];

		size_t numFeatures = 0; // Current number of features in the array

		// Call the function to replace the feature list
		LoadFeature(featuresToLoad, numFeatures, pref.featuresToLoad, pref.numFeaturesToLoad, maxFeatures, kFeatureDeepDVC);
		pref.featuresToLoad = featuresToLoad;
		pref.numFeaturesToLoad++;

		for (size_t i = 0; i < pref.numFeaturesToLoad; ++i) {
			LOG_WARNING(L"[STREAMLINE] Loading " + std::to_wstring(pref.featuresToLoad[i]));
		}
	}

	if (ctx.streamline.forceLoadDeepDvc) {
		LOG_WARNING(L"[STREAMLINE] Forcefull load of DLSSG");
		const size_t maxFeatures = 10;
		Feature featuresToLoad[maxFeatures];

		size_t numFeatures = 0; // Current number of features in the array

		// Call the function to replace the feature list
		LoadFeature(featuresToLoad, numFeatures, pref.featuresToLoad, pref.numFeaturesToLoad, maxFeatures, kFeatureDLSS_G);
		pref.featuresToLoad = featuresToLoad;
		pref.numFeaturesToLoad++;

		for (size_t i = 0; i < pref.numFeaturesToLoad; ++i) {
			LOG_WARNING(L"[STREAMLINE] Loading " + std::to_wstring(pref.featuresToLoad[i]));
		}
	}

	int result = original_slInit(pref, sdkVersion);

	return result;
}

int detoured_slEvaluateFeature(Feature feature, void* arg2, void* arg3, void* arg4, void* arg5)
{
	//LOG_WARNING(std::wstring(L"========= ") + __FUNCTIONW__);
	//SL5ARGS tmp = (SL5ARGS)GetProcAddress(GetCustomInterposer(), "slEvaluateFeature");

	int result = original_slEvaluateFeature(feature, arg2, arg3, arg4, arg5);

	if (feature == kFeatureDLSS && ctx.streamline.forceLoadDeepDvc) {
		auto result2 = original_slEvaluateFeature(kFeatureDeepDVC, arg2, arg3, arg4, arg5);
	}
	return result;
}

// slDLSSGSetOptions(const sl::ViewportHandle& viewport, const sl::DLSSGOptions& options)

int slDLSSSetOptions(/*ViewportHandle& viewport*/ void *viewport, void* options)
{
	currentViewPort = viewport;
	int result = original_slDLSSSetOptions(currentViewPort, options);

	if (!original_slDLSSGSetOptions && ctx.streamline.forceLoadDLSSG) {
		LOG_ERROR(L"[STREAMLINE] Wrapping slDLSSGSetOptions");
		void *ptr;
		original_slGetFeatureFunction(kFeatureDLSS_G, "slDLSSGSetOptions", ptr);
		original_slDLSSGSetOptions = reinterpret_cast<DLSSGSETOPTS>(ptr);				
		
		original_slGetFeatureFunction(kFeatureDeepDVC, "slDeepDVCSetOptions", ptr);
		original_slDeepDVCSetOptions = reinterpret_cast<DEEPDVCSETOPTS>(ptr);
		
		if (ctx.streamline.forceLoadDLSSG) {
			auto slSetFeatureLoaded = (DLSSGSETFEATLOADED)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetFeatureLoaded");
			slSetFeatureLoaded(kFeatureDLSS_G, true);
		}
	}
	
	if (ctx.streamline.forceLoadDeepDvc) {
		DeepDVCOptions deepDVCOptions = {};
		deepDVCOptions.mode = DeepDVCMode::eOn; 
		deepDVCOptions.intensity = ctx.deepDVC.intensity; 
		deepDVCOptions.saturationBoost = ctx.deepDVC.saturationBoost;
		original_slDeepDVCSetOptions(viewport, deepDVCOptions);
	}

	if (ctx.streamline.forceLoadDLSSG) {
		DLSSGOptions optionsg{};
		// These are populated based on user selection in the UI
		optionsg.mode = DLSSGMode::eOn; // e.g. sl::DLSSGMode::eOn;
		auto result2 = original_slDLSSGSetOptions(viewport, optionsg);
		//LOG_WARNING(L"==== " + std::to_wstring(result2));
	}
	return result;
}

int detoured_slSetFeatureLoaded(Feature feature, bool loaded)
{
	//LOG_WARNING(std::wstring(L"========= ") + std::to_wstring(feature) + L" : " + std::to_wstring(loaded)  + __FUNCTIONW__);
	
	if (feature == kFeatureDLSS_G) {
		loaded = true;
	}
	int result = original_slSetFeatureLoaded(feature, loaded);

	return result;
}

int detoured_slGetFeatureFunction(Feature feature, const char* functionName, void*& function)
{
	LOG_DEBUG(L"[STREAMLINE] Getting " + ToWideString(std::string(functionName)) + L" for " + std::to_wstring(feature));

	auto funcName = std::string(functionName);

	int result = original_slGetFeatureFunction(feature, functionName, function);

	if (funcName == "slDLSSSetOptions" && result == 0) {
		original_slDLSSSetOptions = (SL2ARGS) function;
		function = slDLSSSetOptions;
	}


	return result;
}

void DetourStreamline()
{
	auto filePath = Common::GetProcessFilePath();
	std::wstring processName = filePath.filename().wstring();

	if (processName == L"Cyberpunk2077.exe" && GetModuleHandle(TEXT("sl.interposer.dll"))) {
		LOG_WARNING(L"[STREAMLINE] slInit: Cyberpunk 2077 detected, enabling UI anti-glitch hook");
		original_slSetTag = (SLSETTAG)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetTag");
		//original_slSetTag = (SLSETTAG)DetourFindFunction("sl.interposer.dll", "slSetTag");
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)original_slSetTag, detoured_slSetTagForCyberpunkFixed);
		DetourTransactionCommitEx(NULL);
	}

	if (GetModuleHandle(TEXT("sl.interposer.dll")) && ctx.streamline.interposerVersion[0] == L'2') {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slInit = (SLINIT)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slInit");
		//original_slInit = (SLINIT)DetourFindFunction("sl.interposer.dll", "slInit");
		DetourAttach(&(PVOID&)original_slInit, detoured_slInit);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slEvaluateFeature = (SLFEATUREARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slEvaluateFeature");
		DetourAttach(&(PVOID&)original_slEvaluateFeature, detoured_slEvaluateFeature);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slGetFeatureFunction = (SLGETFEATFUNC)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slGetFeatureFunction");
		DetourAttach(&(PVOID&)original_slGetFeatureFunction, detoured_slGetFeatureFunction);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slSetFeatureLoaded = (DLSSGSETFEATLOADED)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetFeatureLoaded");
		//DetourAttach(&(PVOID&)original_slSetFeatureLoaded, detoured_slSetFeatureLoaded);
		DetourTransactionCommit();
	}

	//DetourTransactionBegin();
	//DetourUpdateThread(GetCurrentThread());
	//original_slGetFeatureRequirements = (SLGETFEATREQ)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slGetFeatureRequirements");
	//DetourAttach(&(PVOID&)original_slGetFeatureRequirements, detoured_slGetFeatureRequirements);
	//DetourTransactionCommit();	
	
	/*
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	original_slHookPresent = (SL4ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slHookPresent");
	DetourAttach(&(PVOID&)original_slHookPresent, detoured_slHookPresent);
	DetourTransactionCommit();

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	original_slHookPresent1 = (SL5ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slHookPresent1");
	DetourAttach(&(PVOID&)original_slHookPresent1, detoured_slHookPresent1);
	DetourTransactionCommit();
	*/
	#ifdef STREAMLINE_DETOUR
	if (processName == L"witcher3!.exe" && GetModuleHandle(TEXT("sl.interposer.dll"))) {
		Console::Warning(L"[STREAMLINE] slInit: Witcher 3 detected, enabling aspect ratio anti-glitch hook");
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slSetTagV1 = (SLSETTAGV1)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetTag");
		DetourAttach(&(PVOID&)original_slSetTagV1, detoured_slSetTagForWitcherFixed);
		DetourTransactionCommit();
	}

	if (GetModuleHandle(TEXT("sl.interposer.dll")) && GetCustomInterposer()) {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slInit = (SLINIT)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slInit");
		DetourAttach(&(PVOID&)original_slInit, detoured_slInit);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slIsFeatureLoaded = (SLFEATINIT)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slIsFeatureLoaded");
		DetourAttach(&(PVOID&)original_slIsFeatureLoaded, detoured_slIsFeatureLoaded);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slIsFeatureSupported = (SL3ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slIsFeatureSupported");
		DetourAttach(&(PVOID&)original_slIsFeatureSupported, detoured_slIsFeatureSupported);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slAllocateResources = (SL3ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slAllocateResources");
		DetourAttach(&(PVOID&)original_slAllocateResources, detoured_slAllocateResources);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slShutdown = (SL0ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slShutdown");
		DetourAttach(&(PVOID&)original_slShutdown, detoured_slShutdown);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slGetNewFrameToken = (SL2ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slGetNewFrameToken");
		DetourAttach(&(PVOID&)original_slGetNewFrameToken, detoured_slGetNewFrameToken);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slFreeResources = (SL2ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slFreeResources");
		DetourAttach(&(PVOID&)original_slFreeResources, detoured_slFreeResources);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slGetNativeInterface = (SL2ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slGetNativeInterface");
		DetourAttach(&(PVOID&)original_slGetNativeInterface, detoured_slGetNativeInterface);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slGetFeatureVersion = (SL2ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slGetFeatureVersion");
		DetourAttach(&(PVOID&)original_slGetFeatureVersion, detoured_slGetFeatureVersion);
		DetourTransactionCommit();

		if (processName != L"Cyberpunk2077.exe") {
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			original_slSetTag = (SLSETTAG)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetTag");
			DetourAttach(&(PVOID&)original_slSetTag, detoured_slSetTag);
			DetourTransactionCommit();
		}

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_D3D12GetDebugInterface = (SL2ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "D3D12GetDebugInterface");
		DetourAttach(&(PVOID&)original_D3D12GetDebugInterface, detoured_D3D12GetDebugInterface);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_CreateDXGIFactory2 = (SL3ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "CreateDXGIFactory2");
		DetourAttach(&(PVOID&)original_CreateDXGIFactory2, detoured_CreateDXGIFactory2);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_D3D12CreateDevice = (SL4ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "D3D12CreateDevice");
		DetourAttach(&(PVOID&)original_D3D12CreateDevice, detoured_D3D12CreateDevice);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_D3D12CreateRootSignatureDeserializer = (SL4ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "D3D12CreateRootSignatureDeserializer");
		DetourAttach(&(PVOID&)original_D3D12CreateRootSignatureDeserializer, detoured_D3D12CreateRootSignatureDeserializer);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_D3D12SerializeRootSignature = (SL4ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "D3D12SerializeRootSignature");
		DetourAttach(&(PVOID&)original_D3D12SerializeRootSignature, detoured_D3D12SerializeRootSignature);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_D3D12CreateVersionedRootSignatureDeserializer = (SL4ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "D3D12CreateVersionedRootSignatureDeserializer");
		DetourAttach(&(PVOID&)original_D3D12CreateVersionedRootSignatureDeserializer, detoured_D3D12CreateVersionedRootSignatureDeserializer);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_D3D12SerializeVersionedRootSignature = (SL3ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "D3D12SerializeVersionedRootSignature");
		DetourAttach(&(PVOID&)original_D3D12SerializeVersionedRootSignature, detoured_D3D12SerializeVersionedRootSignature);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_D3D12GetInterface = (SL3ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "D3D12GetInterface");
		DetourAttach(&(PVOID&)original_D3D12GetInterface, detoured_D3D12GetInterface);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slSetConstants = (SL3ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetConstants");
		DetourAttach(&(PVOID&)original_slSetConstants, detoured_slSetConstants);
		DetourTransactionCommit();


		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slUpgradeInterface = (SL1ARG)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slUpgradeInterface");
		DetourAttach(&(PVOID&)original_slUpgradeInterface, detoured_slUpgradeInterface);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slSetD3DDevice = (SL1ARG)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetD3DDevice");
		DetourAttach(&(PVOID&)original_slSetD3DDevice, detoured_slSetD3DDevice);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slSetVulkanInfo = (SL1ARG)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slSetVulkanInfo");
		DetourAttach(&(PVOID&)original_slSetVulkanInfo, detoured_slSetVulkanInfo);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_slGetFeatureFunction = (SL3ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slGetFeatureFunction");
		DetourAttach(&(PVOID&)original_slGetFeatureFunction, detoured_slGetFeatureFunction);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_CreateDXGIFactory1 = (SL2ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "CreateDXGIFactory1");
		DetourAttach(&(PVOID&)original_CreateDXGIFactory1, detoured_CreateDXGIFactory1);
		DetourTransactionCommit();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		original_CreateDXGIFactory = (SL2ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "CreateDXGIFactory");
		DetourAttach(&(PVOID&)original_CreateDXGIFactory, detoured_CreateDXGIFactory);
		DetourTransactionCommit();
	}
#endif
}