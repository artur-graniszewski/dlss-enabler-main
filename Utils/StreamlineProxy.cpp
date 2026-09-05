#include <wtypes.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include "Common.h"
#include "../Detours/detours.h"
#include "../Core/Context.h"
#include "StreamlineProxy.h"
#include "SwapChainEvents.h"
#include "OverdriveController.h"
#include "UxHook.h"

// When defined, forces Streamline to use a DXGI factory proxy instead of
// modifying the v-table of the base interface.  This reduces the risk of
// conflicts with 3rd-party overlays and our own DXGI hooks.
//#define SL_USE_DXGI_FACTORY_PROXY

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
extern SL1ARG original_slSetVulkanInfo;
extern SL2ARGS original_CreateDXGIFactory;
extern SL2ARGS original_CreateDXGIFactory1;
extern SL3ARGS original_CreateDXGIFactory2;
extern SL3ARGS original_D3D12SerializeVersionedRootSignature;
extern SL3ARGS original_D3D12GetInterface;
extern SLSETTAG original_slSetTag;
extern SLSETTAGV1 original_slSetTagV1;
extern SL2ARGS original_slDLSSSetOptions;
extern DLSSGSETOPTS original_slDLSSGSetOptions;
extern DLSSGGETSTATUS original_slDLSSGGetState;
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

// =============================================================================
// Streamline Present hooks - full present pipeline for SL-managed swapchains
// Replicates prePresentHook/postPresentHook from Wrapped_Swapchain path
// Active only when ctx.streamline.isPresentHookEnabled == true
// =============================================================================

static UINT g_SlFrameSkipCounter = 0;

static void slOnPrePresent()
{
	static uint64_t lastCycleId = 0;

	if (lastCycleId != ctx.reflex.optiFgCycle) {
		lastCycleId = ctx.reflex.optiFgCycle;
		ctx.reflex.isOptiFgEnabled = true;
	}
	else {
		ctx.reflex.optiFgCycle = 0;
		ctx.reflex.isOptiFgEnabled = 0;
	}

	if (ctx.logging.isReflexDebugEnabled) {
		LOG_WARNING(L"< PRESENT (SL)");
	}
}

int detoured_slHookPresent(void* arg, void* arg2, void* arg3, void* arg4)
{
	IDXGISwapChain* pSwapChain = reinterpret_cast<IDXGISwapChain*>(arg);
	UINT SyncInterval = static_cast<UINT>(reinterpret_cast<uintptr_t>(arg2));
	UINT PresentFlags = static_cast<UINT>(reinterpret_cast<uintptr_t>(arg3));

	// Skip test/restart frames
	if ((PresentFlags & DXGI_PRESENT_TEST) || (PresentFlags & DXGI_PRESENT_RESTART))
		return original_slHookPresent(arg, arg2, arg3, arg4);

	static bool s_FirstCall = false;
	if (!s_FirstCall) {
		s_FirstCall = true;
		LOG_INFO(L"[STREAMLINE] slHookPresent: first call on tid "
			+ std::to_wstring(GetCurrentThreadId())
			+ L" swapchain=0x" + std::to_wstring(reinterpret_cast<uintptr_t>(arg)));
	}

	// Frame skipping for dynamic FG mode when not generating
	if (ctx.ngx.isDynamicFrameGenerationEnabled &&
		!ctx.ngx.isGeneratingFrames &&
		ctx.ngx.framesGenerated >= 0)
	{
		g_SlFrameSkipCounter++;
		UINT skipCount = ctx.ngx.maxFramesGenerated + 1;
		if (g_SlFrameSkipCounter < skipCount)
		{
			PresentFlags = DXGI_PRESENT_TEST;
			return original_slHookPresent(arg, arg2,
				reinterpret_cast<void*>(static_cast<uintptr_t>(PresentFlags)), arg4);
		}
		g_SlFrameSkipCounter = 0;
	}

	// Store original SyncInterval for FPS monitoring
	UINT originalSyncInterval = SyncInterval;

	// Detect if game is using VSync natively (this frame)
	ctx.reflex.isGameVsyncEnabled = (SyncInterval > 0);

	// Detect if game is using tearing mode THIS FRAME (VRR/G-Sync/FreeSync)
	ctx.reflex.isGameTearingEnabled = (PresentFlags & DXGI_PRESENT_ALLOW_TEARING) != 0;

	// Apply VSync override only if enabled
	if (OverdriveController::GetVsyncOverrideEnabled()) {
		if (OverdriveController::GetVsyncEnabled()) {
			if (ctx.reflex.isGameTearingEnabled) {
				PresentFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
			}
			SyncInterval = 1;
		}
		else {
			SyncInterval = 0;
		}
	}

	// Store effective SyncInterval for FPS monitor
	ctx.reflex.lastSyncInterval = SyncInterval;
	ctx.reflex.lastOriginalSyncInterval = originalSyncInterval;

	slOnPrePresent();

	// Dispatch to all registered listeners
	SwapChainEvents::DispatchPrePresent(pSwapChain, SyncInterval, PresentFlags);

	// Render ImGui overlay
	UxHook::RenderOverlay(pSwapChain);

	// Call original Streamline Present with (potentially modified) args
	int result = original_slHookPresent(
		arg,
		reinterpret_cast<void*>(static_cast<uintptr_t>(SyncInterval)),
		reinterpret_cast<void*>(static_cast<uintptr_t>(PresentFlags)),
		arg4);

	// Post-present
	SwapChainEvents::DispatchPostPresent(pSwapChain, SyncInterval, PresentFlags, static_cast<HRESULT>(result));

	return result;
}

int detoured_slHookPresent1(void* arg, void* arg2, void* arg3, void* arg4, void* arg5)
{
	IDXGISwapChain1* pSwapChain = reinterpret_cast<IDXGISwapChain1*>(arg);
	UINT SyncInterval = static_cast<UINT>(reinterpret_cast<uintptr_t>(arg2));
	UINT PresentFlags = static_cast<UINT>(reinterpret_cast<uintptr_t>(arg3));
	const DXGI_PRESENT_PARAMETERS* pParams = reinterpret_cast<const DXGI_PRESENT_PARAMETERS*>(arg4);

	// Skip test/restart frames
	if ((PresentFlags & DXGI_PRESENT_TEST) || (PresentFlags & DXGI_PRESENT_RESTART))
		return original_slHookPresent1(arg, arg2, arg3, arg4, arg5);

	static bool s_FirstCall1 = false;
	if (!s_FirstCall1) {
		s_FirstCall1 = true;
		LOG_INFO(L"[STREAMLINE] slHookPresent1: first call on tid "
			+ std::to_wstring(GetCurrentThreadId())
			+ L" swapchain=0x" + std::to_wstring(reinterpret_cast<uintptr_t>(arg)));
	}

	// Frame skipping for dynamic FG mode
	if (ctx.ngx.isDynamicFrameGenerationEnabled &&
		!ctx.ngx.isGeneratingFrames &&
		ctx.ngx.framesGenerated >= 0)
	{
		g_SlFrameSkipCounter++;
		UINT skipCount = ctx.ngx.maxFramesGenerated + 1;
		if (g_SlFrameSkipCounter < skipCount)
		{
			PresentFlags = DXGI_PRESENT_TEST;
			return original_slHookPresent1(arg, arg2,
				reinterpret_cast<void*>(static_cast<uintptr_t>(PresentFlags)), arg4, arg5);
		}
		g_SlFrameSkipCounter = 0;
	}

	UINT originalSyncInterval = SyncInterval;
	ctx.reflex.isGameVsyncEnabled = (SyncInterval > 0);
	ctx.reflex.isGameTearingEnabled = (PresentFlags & DXGI_PRESENT_ALLOW_TEARING) != 0;

	if (OverdriveController::GetVsyncOverrideEnabled()) {
		if (OverdriveController::GetVsyncEnabled()) {
			if (ctx.reflex.isGameTearingEnabled) {
				PresentFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
			}
			SyncInterval = 1;
		}
		else {
			SyncInterval = 0;
		}
	}

	ctx.reflex.lastSyncInterval = SyncInterval;
	ctx.reflex.lastOriginalSyncInterval = originalSyncInterval;

	slOnPrePresent();

	// Dispatch to all registered listeners
	SwapChainEvents::DispatchPrePresent1(pSwapChain, SyncInterval, PresentFlags, pParams);

	// Render ImGui overlay
	UxHook::RenderOverlay(pSwapChain);

	// Call original with modified args
	int result = original_slHookPresent1(
		arg,
		reinterpret_cast<void*>(static_cast<uintptr_t>(SyncInterval)),
		reinterpret_cast<void*>(static_cast<uintptr_t>(PresentFlags)),
		arg4, arg5);

	// Post-present
	SwapChainEvents::DispatchPostPresent1(pSwapChain, SyncInterval, PresentFlags, static_cast<HRESULT>(result));

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
	LOG_WARNING(std::wstring(L"==OK===== ") + __FUNCTIONW__);

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
		//return;
	}
	//return;
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

	// Throttle spammy log messages - mute after 10 occurrences
	static const std::vector<std::wstring> throttledPrefixes = {
		L"UI buffer extent does not match color buffer size",
		L"HUD-less buffer extent does not match color buffer size",
		L"ProcessEvaluateParams Render Size:",
		L"dlfgPresent::updateStatus: eDLSSGStatusFailReflexNotDetectedAtRuntime:",
		L"Exceeded VRAM budget, various performance issues including stuttering can be expected",
		L"bCplVsyncOn",
		L"eBlockNoClientQueues",
		L"not set yet",
		L"commonInterface::set: Setting different 'common'"
	};

	static std::unordered_map<std::wstring, int> throttleCounts;
	constexpr int MAX_LOG_REPEATS = 10;

	for (const auto& prefix : throttledPrefixes) {
		if (message.find(prefix) != std::wstring::npos) {
			int& count = throttleCounts[prefix];
			count++;
			if (count == MAX_LOG_REPEATS + 1) {
				LOG_WARNING(L"[STREAMLINE] Log entry \"" + prefix + L"...\" repeated more than "
					+ std::to_wstring(MAX_LOG_REPEATS) + L" times, further occurrences will be muted");
			}
			if (count > MAX_LOG_REPEATS) {
				return;
			}
			break;
		}
	}

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

	//pref.flags = static_cast<PreferenceFlags>((uint64_t)pref.flags & ~(uint64_t)PreferenceFlags::eUseDXGIFactoryProxy);
#ifdef SL_USE_DXGI_FACTORY_PROXY
	pref.flags = static_cast<PreferenceFlags>((uint64_t)pref.flags | (uint64_t)PreferenceFlags::eUseDXGIFactoryProxy);
	LOG_INFO(L"[STREAMLINE] eUseDXGIFactoryProxy enabled - using DXGI factory proxy to reduce hook conflicts");
#endif
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

	if (ctx.streamline.forceLoadDLSSG) {
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

	if (ctx.nvapi.mfgEnforcedMode > 0 || ctx.streamline.forceLoadDLSSG) {
		LOG_WARNING(L"[STREAMLINE] Forcefull load of PCL");
		const size_t maxFeatures = 10;
		Feature featuresToLoad[maxFeatures];

		size_t numFeatures = 0; // Current number of features in the array

		// Call the function to replace the feature list
		LoadFeature(featuresToLoad, numFeatures, pref.featuresToLoad, pref.numFeaturesToLoad, maxFeatures, kFeaturePCL);
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

// Last DLSSGOptions received from the game - preserved so we only override numFramesToGenerate
static DLSSGOptions g_LastGameDLSSGOptions{};
static bool g_HasGameDLSSGOptions = false;
static bool g_IsInternalCall = false; // Guard: true when our mod calls slDLSSGSetOptions

// Mutex protecting all access to original_slDLSSGSetOptions and the state above.
// Serializes game-initiated calls (slDLSSGSetOptionsWrapper), DFG-initiated calls
// (ForceApplyMfgMode), and restore calls (RestoreGameDLSSGOptions) so they never
// race against each other or Streamline internals.
static std::mutex g_DlssgSetOptionsMutex;

// -----------------------------------------------------------------------------
// MFG override encoding (ctx.nvapi.mfgEnforcedMode)
// -----------------------------------------------------------------------------
//  Value  UI label    Semantics
//    0    OFF         No override - game fully controls FG
//    1    x1          Force FG OFF (mode=eOff, numFrames=1)
//    2    x2          Force FG ON, mode=eOn, numFrames=1 (2X total)
//    3    x3          Force FG ON, mode=eOn, numFrames=2 (3X total)
//    4    x4          Force FG ON, mode=eOn, numFrames=3 (4X total)
//    5    x5          Force FG ON, mode=eOn, numFrames=4 (5X total, needs sl.dlss_g>=2.11)
//    6    x6          Force FG ON, mode=eOn, numFrames=5 (6X total, needs sl.dlss_g>=2.11)
//
// NOTE: ctx.streamline.mfgEnforcedMode (DFG dynamic) still uses RAW numFramesToGenerate
//       value (0=off, 1=2X, 2=3X, ...). Only the nvapi override uses the new
//       UI-mnemonic encoding above. Translation happens only at the boundary below.
// -----------------------------------------------------------------------------

// Translate nvapi override value (UI mnemonic) to raw numFramesToGenerate.
// Returns 0 when nvapi override is inactive (0=OFF) or when value means "force off" (1=x1).
// For x1, callers must ALSO set DLSSGMode::eOff - this function only returns the frame count.
static inline uint32_t NvapiOverrideToNumFrames(int nvapiMode)
{
	// 0=OFF, 1=x1 -> no frames (caller handles mode=eOff for x1)
	// 2=x2 -> 1 frame generated, 3=x3 -> 2, ..., 6=x6 -> 5
	if (nvapiMode <= 1) return 1;  // x1 still needs numFrames=1 per API ("Must be 1")
	return static_cast<uint32_t>(nvapiMode - 1);
}

// Returns true if nvapi override x1 is active (force FG disabled)
static inline bool NvapiOverrideIsForceOff(int nvapiMode)
{
	return nvapiMode == 1;
}

// Returns true if nvapi override is any "force ON" mode (x2..x6)
static inline bool NvapiOverrideIsForceOn(int nvapiMode)
{
	return nvapiMode >= 2;
}

// Helper: sends final DLSSG options to Streamline and keeps ctx.ngx.framesGenerated
// in sync when FG is being turned off. NGX EvaluateFeature only fires while FG is
// actually generating frames, so if Streamline switches to eOff (either because the
// game requested it or because we forced it via x1/passthrough of eOff), the status
// bar needs an explicit zero otherwise it keeps showing the last pre-off value.
// Caller must hold g_DlssgSetOptionsMutex.
static int SubmitDLSSGOptionsLocked(void* viewport, const DLSSGOptions& options)
{
	if (options.mode == DLSSGMode::eOff)
	{
		ctx.ngx.framesGenerated = 0;
	}

	// enableUserInterfaceRecomposition was added to DLSSGOptions in kStructVersion4.
	// Override it with the mod's HUD-interpolation state, but ONLY when the caller
	// actually passed a v4+ struct. On older structs the field does not exist in the
	// caller's layout and Streamline won't read it, so we forward untouched.
	// This is the single choke-point for original_slDLSSGSetOptions, so every path
	// (game wrapper, startup, ForceApply, Restore) is covered by this one guard.
	if (options.structVersion >= kStructVersion4)
	{
		DLSSGOptions modifiedOptions = options;
		modifiedOptions.queueParallelismMode = DLSSGQueueParallelismMode::eBlockPresentingClientQueue; // 0 = eBlockPresentingClientQueue
		modifiedOptions.enableUserInterfaceRecomposition =
			ctx.streamline.isHudInterpolationEnabled ? Boolean::eTrue : Boolean::eFalse;
		return original_slDLSSGSetOptions(viewport, modifiedOptions);
	}

	return original_slDLSSGSetOptions(viewport, options);
}

// Wrapper for slDLSSGSetOptions - intercepts game's calls to apply MFG override.
//
// Rules (per nvapi override):
//   OFF:            pass through untouched (game fully controls)
//   x1 (force off): force mode=eOff regardless of what game requested;
//                   cache game's requested state so toggling back to OFF restores it
//   x2..x6 (on):    HONOR game's mode - if game says eOff, pass eOff through;
//                   if game says eOn/eAuto, override numFramesToGenerate to our target.
//
// DFG dynamic (ctx.streamline.mfgEnforcedMode) still uses raw numFramesToGenerate
// and only applies when nvapi override is OFF.
int slDLSSGSetOptionsWrapper(void* viewport, const DLSSGOptions& options)
{
	std::lock_guard<std::mutex> lock(g_DlssgSetOptionsMutex);

	if (!g_IsInternalCall) {
		// Always cache what the game requested - we need it for restore/mode-honoring
		g_LastGameDLSSGOptions = options;
		g_HasGameDLSSGOptions = true;
	}

	const int nvapiMode = ctx.nvapi.mfgEnforcedMode;

	// Case: nvapi override = x1 (force FG disabled)
	if (NvapiOverrideIsForceOff(nvapiMode))
	{
		DLSSGOptions modifiedOptions = options;
		modifiedOptions.mode = DLSSGMode::eOff;
		modifiedOptions.numFramesToGenerate = 1; // API: "Must be 1"
		return SubmitDLSSGOptionsLocked(viewport, modifiedOptions);
	}

	// Case: nvapi override = x2..x6 (force FG on with specific mnemonic)
	if (NvapiOverrideIsForceOn(nvapiMode))
	{
		// Honor game's decision to disable FG - if game says eOff, pass through untouched.
		if (options.mode == DLSSGMode::eOff)
			return SubmitDLSSGOptionsLocked(viewport, options);

		// Game wants FG on (eOn or eAuto) - override multiplier to our target.
		DLSSGOptions modifiedOptions = options;
		modifiedOptions.numFramesToGenerate = NvapiOverrideToNumFrames(nvapiMode);
		return SubmitDLSSGOptionsLocked(viewport, modifiedOptions);
	}

	// Case: nvapi override = OFF - check DFG dynamic fallback
	if (ctx.streamline.mfgEnforcedMode > 0)
	{
		// DFG: only modifies numFramesToGenerate when game has FG enabled;
		// don't force-enable FG when game disabled it.
		if (options.mode == DLSSGMode::eOff)
			return SubmitDLSSGOptionsLocked(viewport, options);

		DLSSGOptions modifiedOptions = options;
		modifiedOptions.numFramesToGenerate = ctx.streamline.mfgEnforcedMode;
		return SubmitDLSSGOptionsLocked(viewport, modifiedOptions);
	}

	// No override active - full passthrough
	return SubmitDLSSGOptionsLocked(viewport, options);
}

// slDLSSGSetOptions(const sl::ViewportHandle& viewport, const sl::DLSSGOptions& options)

int slDLSSSetOptions(/*ViewportHandle& viewport*/ void* viewport, void* options)
{
	currentViewPort = viewport;
	int result = original_slDLSSSetOptions(currentViewPort, options);

	if (!original_slDLSSGSetOptions && ctx.streamline.forceLoadDLSSG) {
		LOG_ERROR(L"[STREAMLINE] Wrapping slDLSSGSetOptions");
		void* ptr;
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
		optionsg.mode = DLSSGMode::eOn;
		optionsg.numFramesToGenerate = 1; // Default 2X

		// Apply MFG override if active at startup (nvapi uses UI-mnemonic encoding)
		const int nvapiMode = ctx.nvapi.mfgEnforcedMode;
		if (NvapiOverrideIsForceOff(nvapiMode))
		{
			// x1: force FG disabled at startup
			optionsg.mode = DLSSGMode::eOff;
			optionsg.numFramesToGenerate = 1;
		}
		else if (NvapiOverrideIsForceOn(nvapiMode))
		{
			// x2..x6: force FG on with specific multiplier
			optionsg.mode = DLSSGMode::eOn;
			optionsg.numFramesToGenerate = NvapiOverrideToNumFrames(nvapiMode);
		}
		else if (ctx.streamline.mfgEnforcedMode > 0)
		{
			// DFG dynamic (raw numFramesToGenerate encoding)
			optionsg.numFramesToGenerate = ctx.streamline.mfgEnforcedMode;
		}

		{
			std::lock_guard<std::mutex> lock(g_DlssgSetOptionsMutex);
			g_IsInternalCall = true;
			auto result2 = SubmitDLSSGOptionsLocked(viewport, optionsg);
			g_IsInternalCall = false;
			// Cache as initial options so ForceApply preserves them
			g_LastGameDLSSGOptions = optionsg;
			g_HasGameDLSSGOptions = true;
		}
	}
	return result;
}

void* GetCurrentViewPort() { return currentViewPort; }

// Exported helper: force Streamline to apply current MFG override state.
// Called from:
//   - Sidebar dropdown handler when user changes override
//   - FpsMonitor::UpdateDynamicMfg when DFG dynamic mode changes at runtime
//
// Respects the new semantics:
//   nvapi x1       -> mode=eOff (force FG disabled)
//   nvapi x2..x6   -> mode honored from cached game state; if game had eOn, apply our multiplier;
//                     if game had eOff, keep eOff (don't force-enable over game's decision)
//   nvapi OFF + DFG -> mode honored from cache; numFramesToGenerate=dfgMode when game had eOn
void StreamlineProxy_ForceApplyMfgMode()
{
	if (!original_slDLSSGSetOptions || !currentViewPort || !g_HasGameDLSSGOptions)
		return;

	const int nvapiMode = ctx.nvapi.mfgEnforcedMode;
	const int dfgMode = ctx.streamline.mfgEnforcedMode;

	// Nothing active and nothing to restore - bail
	if (nvapiMode == 0 && dfgMode <= 0)
		return;

	std::lock_guard<std::mutex> lock(g_DlssgSetOptionsMutex);

	// Start from game's last cached options to preserve flags, formats, buffer sizes, etc.
	DLSSGOptions optionsg = g_LastGameDLSSGOptions;

	if (NvapiOverrideIsForceOff(nvapiMode))
	{
		// x1: force disable regardless of game's last state
		optionsg.mode = DLSSGMode::eOff;
		optionsg.numFramesToGenerate = 1;
	}
	else if (NvapiOverrideIsForceOn(nvapiMode))
	{
		// x2..x6: honor game's mode - only apply multiplier when game had FG enabled.
		// If game had FG off, leave mode=eOff untouched; wrapper will apply override when
		// game next enables FG.
		if (optionsg.mode != DLSSGMode::eOff)
			optionsg.numFramesToGenerate = NvapiOverrideToNumFrames(nvapiMode);
	}
	else if (dfgMode > 0)
	{
		// DFG dynamic (raw frame count encoding)
		if (optionsg.mode != DLSSGMode::eOff)
			optionsg.numFramesToGenerate = dfgMode;
	}

	LOG_DEBUG(L"[STREAMLINE] ForceApplyMfgMode: mode=" + std::to_wstring((uint32_t)optionsg.mode)
		+ L" numFramesToGenerate=" + std::to_wstring(optionsg.numFramesToGenerate)
		+ L" (nvapi=" + std::to_wstring(nvapiMode)
		+ L" dfg=" + std::to_wstring(dfgMode) + L")"
		+ L" flags=" + std::to_wstring((uint32_t)optionsg.flags)
		+ L" cachedFromGame=" + std::to_wstring(g_HasGameDLSSGOptions));

	g_IsInternalCall = true;
	int result = SubmitDLSSGOptionsLocked(currentViewPort, optionsg);
	g_IsInternalCall = false;

	LOG_DEBUG(L"[STREAMLINE] ForceApplyMfgMode: returned " + std::to_wstring(result));
}

// Exported helper: restore the full cached game DLSSG options to Streamline
// Called when DFG is disabled OR when sidebar override cycles back to OFF -
// gives back full control to the game's settings.
// Skips restore if any nvapi override is active (including x1), since wrapper
// will enforce override on next game call anyway.
void StreamlineProxy_RestoreGameDLSSGOptions()
{
	if (!original_slDLSSGSetOptions || !currentViewPort || !g_HasGameDLSSGOptions)
		return;

	// Any nvapi override active (x1..x6) means we shouldn't restore - wrapper handles it
	if (ctx.nvapi.mfgEnforcedMode != 0)
	{
		LOG_DEBUG(L"[STREAMLINE] RestoreGameDLSSGOptions: skipped, nvapi override active (mode="
			+ std::to_wstring(ctx.nvapi.mfgEnforcedMode) + L")");
		return;
	}

	std::lock_guard<std::mutex> lock(g_DlssgSetOptionsMutex);

	LOG_DEBUG(L"[STREAMLINE] RestoreGameDLSSGOptions: mode=" + std::to_wstring((uint32_t)g_LastGameDLSSGOptions.mode)
		+ L" numFramesToGenerate=" + std::to_wstring(g_LastGameDLSSGOptions.numFramesToGenerate)
		+ L" flags=" + std::to_wstring((uint32_t)g_LastGameDLSSGOptions.flags));

	g_IsInternalCall = true;
	int result = SubmitDLSSGOptionsLocked(currentViewPort, g_LastGameDLSSGOptions);
	g_IsInternalCall = false;

	LOG_DEBUG(L"[STREAMLINE] RestoreGameDLSSGOptions: returned " + std::to_wstring(result));
}

int detoured_slDLSSGGetState(void *viewport, DLSSGState& state, const DLSSGOptions* options)
{
	auto result = original_slDLSSGGetState(viewport, state, options);
	if (result == 0) {
		static bool reported = false;
		if (!reported) {
			LOG_WARNING(L"[STREAMLINE] Overriding Reflex presence");
			reported = true;
		}
		state.status = static_cast<DLSSGStatus>(
			static_cast<uint32_t>(state.status) & ~static_cast<uint32_t>(DLSSGStatus::eFailReflexNotDetectedAtRuntime)
			);
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
		original_slDLSSSetOptions = (SL2ARGS)function;
		function = slDLSSSetOptions;
	}

	if (funcName == "slDLSSGSetOptions" && result == 0) {
		original_slDLSSGSetOptions = reinterpret_cast<DLSSGSETOPTS>(function);
		function = reinterpret_cast<void*>(slDLSSGSetOptionsWrapper);
	}

	//if (funcName == "slDLSSGGetState" && result == 0) {
	//	original_slDLSSGGetState = reinterpret_cast<DLSSGGETSTATUS>(function);
	//	function = reinterpret_cast<void*>(detoured_slDLSSGGetState);
	//}


	return result;
}

// =============================================================================
// Lazy attach for SL Present hooks
// Called from DetourStreamline() and also from proxy_CreateSwapChain* if
// isPresentHookEnabled became true after DetourStreamline() already ran.
// =============================================================================
static bool s_SlPresentHookAttempted = false;

void TryAttachSlPresentHooks()
{
	if (s_SlPresentHookAttempted)
		return;

	if (!ctx.streamline.isPresentHookEnabled)
		return;

	if (!GetModuleHandle(TEXT("sl.interposer.dll")))
		return;

	s_SlPresentHookAttempted = true;

	bool hooked = false;

	original_slHookPresent = (SL4ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slHookPresent");
	if (original_slHookPresent) {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)original_slHookPresent, detoured_slHookPresent);
		DetourTransactionCommit();
		LOG_INFO(L"[STREAMLINE] slHookPresent detour attached for UI overlay");
		hooked = true;
	}

	original_slHookPresent1 = (SL5ARGS)GetProcAddress(GetModuleHandle(TEXT("sl.interposer.dll")), "slHookPresent1");
	if (original_slHookPresent1) {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)original_slHookPresent1, detoured_slHookPresent1);
		DetourTransactionCommit();
		LOG_INFO(L"[STREAMLINE] slHookPresent1 detour attached for UI overlay");
		hooked = true;
	}

	if (!hooked) {
		ctx.streamline.isPresentHookEnabled = false;
		LOG_WARNING(L"[STREAMLINE] slHookPresent not found in sl.interposer.dll - falling back to swapchain wrapper for UI overlay");
	}
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

	// =========================================================================
	// Hook slHookPresent/Present1 for UI overlay on Streamline-managed swapchains
	// When SL >=2.10 destroys and recreates swapchains internally, the wrapper
	// path never receives Present calls. This hooks the interposer's Present
	// entry point directly, which is always in the call chain.
	//
	// If the export doesn't exist (SL <2.10), we disable the flag so the
	// wrapper path handles overlay rendering as before.
	//
	// NOTE: This may also be called lazily from TryAttachSlPresentHooks()
	// if isPresentHookEnabled was not yet set when DetourStreamline() ran.
	// =========================================================================
	TryAttachSlPresentHooks();
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