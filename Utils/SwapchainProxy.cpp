#include <wtypes.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d12.h>
#include <mutex>
#include <atomic>
#include "Common.h"
#include "../Detours/detours.h"
#include "../Core/Context.h"
#include "Wrapped_Swapchain.h"
#include "SwapchainProxy.h"
#include "SwapChainEvents.h"
#include "OverdriveController.h"
#include "UxHook.h"

// Master switch for the swapchain hook subsystem.
// Set to true by EnableSwapchainHooks() (called from HookDxgi::Install).
// Set to false by DisableSwapchainHooks() (called from HookDxgi::Uninstall).
// While false, DetourSwapChain1 / DetourPresent / DetourInitThread must not install
// any new detours - this gives TeardownSwapchainProxy() a stable state to detach from.
static std::atomic<bool> g_swapchainHooksActive{ false };

// Serializes DetourSwapChain1 across concurrent swapchain creations.
// Without this, multi-threaded swapchain creation (rare but legal) would race
// the double-attach guard below and re-patch already-patched Present code.
static std::mutex swapchainDetourMutex;

// Original function pointers
static PresentFn originalPresent = nullptr;
static Present1Fn originalPresent1 = nullptr;
static bool isPresentDetourHijacked = false;

// Dx12 Late Binding
PFN_CreateSwapChain orgCreateSwapChain = nullptr;
PFN_CreateSwapChainForHwnd orgCreateSwapChainForHwnd = nullptr;
PFN_CreateSwapChainForComposition orgCreateSwapChainForComposition = nullptr;
PFN_CreateSwapChainForCoreWindow orgCreateSwapChainForCoreWindow = nullptr;

static unsigned int swapchainInstanceNumber = 0;
static IDXGISwapChain* pSwapChainLast;
static uint64_t lastFrameId = 0;
static int presentCalled = 0;
static int present1Called = 0;

// =============================================================================
// Helper: Safely try to get D3D12 CommandQueue from pDevice
// Returns the command queue pointer if found, nullptr otherwise.
// The returned pointer has been AddRef'd and must be Released by the caller.
// =============================================================================
static ID3D12CommandQueue* TryGetD3D12CommandQueuePtr(IUnknown* pDevice, const wchar_t* callerName)
{
	if (!pDevice)
		return nullptr;

	// Method 1: Try QueryInterface for ID3D12CommandQueue
	ID3D12CommandQueue* pQueue = nullptr;
	if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&pQueue)))) {
		LOG_DEBUG(std::wstring(L"[DXGI] ") + callerName + L": Got CommandQueue via QueryInterface");
		return pQueue; // caller owns the ref
	}

	return nullptr;
}

// Original helper that also sets UxHook � calls the new one internally
static bool TryGetD3D12CommandQueue(IUnknown* pDevice, const wchar_t* callerName)
{
	ID3D12CommandQueue* pQueue = TryGetD3D12CommandQueuePtr(pDevice, callerName);
	if (pQueue) {
		UxHook::SetSwapChainCommandQueue(pQueue);
		pQueue->Release();
		return true;
	}

	// Method 2: Try QueryInterface for ID3D12Device (indicates D3D12, but pDevice is Device not Queue)
	ID3D12Device* pD3D12Device = nullptr;
	if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&pD3D12Device)))) {
		// pDevice is a D3D12Device, not a CommandQueue - we can't get queue from it directly
		LOG_WARNING(std::wstring(L"[DXGI] ") + callerName + L": pDevice is D3D12Device, not CommandQueue");
		pD3D12Device->Release();
		return false;
	}

	// Method 3: Check if it's D3D11 device
	ID3D11Device* pD3D11Device = nullptr;
	if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&pD3D11Device)))) {
		// This is D3D11 - no CommandQueue concept
		LOG_DEBUG(std::wstring(L"[DXGI] ") + callerName + L": D3D11 device detected, skipping CommandQueue setup");
		pD3D11Device->Release();
		return false;
	}

	// Unknown device type
	LOG_DEBUG(std::wstring(L"[DXGI] ") + callerName + L": Unknown device type, skipping CommandQueue setup");
	return false;
}

// =============================================================================
// Internal pre/post handlers (core functionality)
// =============================================================================

static void onPrePresent()
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
		LOG_WARNING(L"< PRESENT");
	}
}

static void onPostPresent()
{
	if (ctx.logging.isReflexDebugEnabled) {
		// LOG_INFO(L"==== EVAL ID: " + std::to_wstring(ctx.reflex.evalId));
	}
}

// =============================================================================
// Wrapper callbacks (called by DxgiWrappedIDXGISwapChain4)
// These dispatch to the event system
// =============================================================================

// Frame skip counter for dynamic FG mode
static UINT g_FrameSkipCounter = 0;

static HRESULT WINAPI prePresentHook(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& PresentFlags)
{
	if ((PresentFlags & DXGI_PRESENT_TEST) || (PresentFlags & DXGI_PRESENT_RESTART))
		return S_OK;

	if (ctx.ngx.isDynamicFrameGenerationEnabled)
		//LOG_WARNING(L"AFG: " + std::to_wstring(ctx.ngx.isDynamicFrameGenerationEnabled) + L", GEN: " 
		//	+ std::to_wstring(ctx.ngx.isGeneratingFrames) + L", MAX:" + std::to_wstring(ctx.ngx.maxFramesGenerated));

		// Frame skipping for dynamic FG mode when not generating
		// Skip X frames where X = framesGenerated (e.g., skip 3 out of 4 for 4X mode)
		if (ctx.ngx.isDynamicFrameGenerationEnabled &&
			!ctx.ngx.isGeneratingFrames &&
			ctx.ngx.framesGenerated >= 0)
		{
			g_FrameSkipCounter++;

			// Skip this frame if not at the Nth frame
			// framesGenerated = 3 means 4X mode, so present every 4th frame (skip 3)
			UINT skipCount = ctx.ngx.maxFramesGenerated + 1;
			if (g_FrameSkipCounter < skipCount)
			{
				// Skip this present by using TEST flag
				PresentFlags = DXGI_PRESENT_TEST;
				return S_OK;
			}

			// Reset counter, allow this present through
			g_FrameSkipCounter = 0;
		}

	// Store original SyncInterval for FPS monitoring
	UINT originalSyncInterval = SyncInterval;

	// Detect if game is using VSync natively (this frame)
	ctx.reflex.isGameVsyncEnabled = (SyncInterval > 0);

	// Detect if game is using tearing mode THIS FRAME (VRR/G-Sync/FreeSync)
	ctx.reflex.isGameTearingEnabled = (PresentFlags & DXGI_PRESENT_ALLOW_TEARING) != 0;

	// Frame duplication mode: SyncInterval = 2 when duplicating and no tearing
	//if (ctx.ngx.isDuplicatingFrames && !ctx.reflex.isGameTearingEnabled) {
	//	SyncInterval = ctx.ngx.maxFramesGenerated + 1;
	//}

	// Apply VSync override only if enabled (and not in duplication mode)
	//else 
	if (OverdriveController::GetVsyncOverrideEnabled()) {
		if (OverdriveController::GetVsyncEnabled()) {
			// Force VSync ON
			// If game uses tearing, we need to remove the flag and set SyncInterval
			if (ctx.reflex.isGameTearingEnabled) {
				PresentFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
			}
			SyncInterval = 1;
		}
		else {
			// Force VSync OFF
			// If game uses VSync natively, just set SyncInterval to 0
			// (we can't add ALLOW_TEARING flag - swapchain must be created with it)
			SyncInterval = 0;
		}
	}

	// Store effective SyncInterval for FPS monitor
	ctx.reflex.lastSyncInterval = SyncInterval;
	ctx.reflex.lastOriginalSyncInterval = originalSyncInterval;

	onPrePresent();

	// Dispatch to all registered listeners
	SwapChainEvents::DispatchPrePresent(pSwapChain, SyncInterval, PresentFlags);

	// Render ImGui overlay
	UxHook::RenderOverlay(pSwapChain);

	return S_OK;
}

static HRESULT WINAPI prePresent1Hook(IDXGISwapChain1* pSwapChain, UINT& SyncInterval, UINT& PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
	if ((PresentFlags & DXGI_PRESENT_TEST) || (PresentFlags & DXGI_PRESENT_RESTART))
		return S_OK;

	// Frame skipping for dynamic FG mode when not generating
	// Skip X frames where X = framesGenerated (e.g., skip 3 out of 4 for 4X mode)

	if (ctx.ngx.isDynamicFrameGenerationEnabled &&
		!ctx.ngx.isGeneratingFrames &&
		ctx.ngx.framesGenerated >= 0)
	{
		g_FrameSkipCounter++;

		// Skip this frame if not at the Nth frame
		// framesGenerated = 3 means 4X mode, so present every 4th frame (skip 3)
		UINT skipCount = ctx.ngx.maxFramesGenerated + 1;
		if (g_FrameSkipCounter < skipCount)
		{
			// Skip this present by using TEST flag
			PresentFlags = DXGI_PRESENT_TEST;
			return S_OK;
		}

		// Reset counter, allow this present through
		g_FrameSkipCounter = 0;
	}

	// Store original SyncInterval for FPS monitoring
	UINT originalSyncInterval = SyncInterval;

	// Detect if game is using VSync natively (this frame)
	ctx.reflex.isGameVsyncEnabled = (SyncInterval > 0);

	// Detect if game is using tearing mode THIS FRAME (VRR/G-Sync/FreeSync)
	ctx.reflex.isGameTearingEnabled = (PresentFlags & DXGI_PRESENT_ALLOW_TEARING) != 0;

	// Frame duplication mode: SyncInterval = 2 when duplicating and no tearing
	//if (ctx.ngx.isDuplicatingFrames && !ctx.reflex.isGameTearingEnabled) {
	//	SyncInterval = ctx.ngx.maxFramesGenerated + 1;
	//}

	// Apply VSync override only if enabled (and not in duplication mode)
	//else 
	if (OverdriveController::GetVsyncOverrideEnabled()) {
		if (OverdriveController::GetVsyncEnabled()) {
			// Force VSync ON
			// If game uses tearing, we need to remove the flag and set SyncInterval
			if (ctx.reflex.isGameTearingEnabled) {
				PresentFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
			}
			SyncInterval = 1;
		}
		else {
			// Force VSync OFF
			// If game uses VSync natively, just set SyncInterval to 0
			// (we can't add ALLOW_TEARING flag - swapchain must be created with it)
			SyncInterval = 0;
		}
	}

	// Store effective SyncInterval for FPS monitor
	ctx.reflex.lastSyncInterval = SyncInterval;
	ctx.reflex.lastOriginalSyncInterval = originalSyncInterval;

	onPrePresent();

	// Dispatch to all registered listeners
	SwapChainEvents::DispatchPrePresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);

	// Render ImGui overlay
	UxHook::RenderOverlay(pSwapChain);

	return S_OK;
}

static HRESULT WINAPI postPresentHook(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& PresentFlags)
{
	if ((PresentFlags & DXGI_PRESENT_TEST) || (PresentFlags & DXGI_PRESENT_RESTART)) {
		return S_OK;
	}

	onPostPresent();

	// Dispatch to all registered listeners (result not available in wrapper callback)
	SwapChainEvents::DispatchPostPresent(pSwapChain, SyncInterval, PresentFlags, S_OK);

	return S_OK;
}

static HRESULT WINAPI postPresent1Hook(IDXGISwapChain1* pSwapChain, UINT& SyncInterval, UINT& PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
	if ((PresentFlags & DXGI_PRESENT_TEST) || (PresentFlags & DXGI_PRESENT_RESTART))
		return S_OK;

	onPostPresent();

	// Dispatch to all registered listeners
	SwapChainEvents::DispatchPostPresent1(pSwapChain, SyncInterval, PresentFlags, S_OK);

	return S_OK;
}

// =============================================================================
// ResizeBuffers callbacks (renamed from ClearTrigger)
// =============================================================================

static void preResizeBuffersHook(IDXGISwapChain* pSwapChain)
{
	// Dispatch to all registered listeners (basic info, full params in wrapper)
	SwapChainEvents::DispatchPreResizeBuffers(pSwapChain, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

	UxHook::OnResizeBuffers();
}

static void postResizeBuffersHook(IDXGISwapChain* pSwapChain)
{
	// Dispatch to all registered listeners
	SwapChainEvents::DispatchPostResizeBuffers(pSwapChain, S_OK);

	UxHook::OnResizeBuffersComplete(pSwapChain);
}

// =============================================================================
// Direct Detour hooks (for enableDirectSwapchainHooking mode)
// =============================================================================

void DetourPresent(IDXGISwapChain* pSwapChain);

// Hooked Present function
HRESULT WINAPI hookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	LOG_TRACE(L"[DXGI] Present (direct): called");
	presentCalled++;

	if ((Flags & DXGI_PRESENT_TEST) || (Flags & DXGI_PRESENT_RESTART))
		return S_OK;

	// Store original SyncInterval for FPS monitoring
	UINT originalSyncInterval = SyncInterval;

	// Detect if game is using VSync natively (this frame)
	ctx.reflex.isGameVsyncEnabled = (SyncInterval > 0);

	// Detect if game is using tearing mode THIS FRAME (VRR/G-Sync/FreeSync)
	ctx.reflex.isGameTearingEnabled = (Flags & DXGI_PRESENT_ALLOW_TEARING) != 0;

	// Frame duplication mode: SyncInterval = 2 when duplicating and no tearing
	if (ctx.ngx.isDuplicatingFrames && !ctx.reflex.isGameTearingEnabled) {
		SyncInterval = 2;
	}
	// Apply VSync override only if enabled (and not in duplication mode)
	else if (ctx.reflex.isVsyncOverrideEnabled) {
		if (ctx.reflex.isVsyncEnabled) {
			// Force VSync ON
			// If game uses tearing, we need to remove the flag and set SyncInterval
			if (ctx.reflex.isGameTearingEnabled) {
				Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
			}
			SyncInterval = 1;
		}
		else {
			// Force VSync OFF
			// If game uses VSync natively, just set SyncInterval to 0
			// (we can't add ALLOW_TEARING flag - swapchain must be created with it)
			SyncInterval = 0;
		}
	}

	// Store effective SyncInterval for FPS monitor
	ctx.reflex.lastSyncInterval = SyncInterval;
	ctx.reflex.lastOriginalSyncInterval = originalSyncInterval;

	onPrePresent();

	// Dispatch pre-present to listeners
	SwapChainEvents::DispatchPrePresent(pSwapChain, SyncInterval, Flags);

	// Render ImGui overlay (direct detour path)
	UxHook::RenderOverlay(pSwapChain);

	// original Present
	auto result = originalPresent(pSwapChain, SyncInterval, Flags);

	// Dispatch post-present to listeners
	SwapChainEvents::DispatchPostPresent(pSwapChain, SyncInterval, Flags, result);

	onPostPresent();

	// re-hook logic in case another mod detoured this call
	DetourPresent(pSwapChain);

	return result;
}

// Hooked Present1 function
HRESULT WINAPI hookedPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
	LOG_TRACE(L"[DXGI] Present1 (direct): called");
	present1Called++;

	if ((Flags & DXGI_PRESENT_TEST) || (Flags & DXGI_PRESENT_RESTART))
		return S_OK;

	// Store original SyncInterval for FPS monitoring
	UINT originalSyncInterval = SyncInterval;

	// Detect if game is using VSync natively (this frame)
	ctx.reflex.isGameVsyncEnabled = (SyncInterval > 0);

	// Detect if game is using tearing mode THIS FRAME (VRR/G-Sync/FreeSync)
	ctx.reflex.isGameTearingEnabled = (Flags & DXGI_PRESENT_ALLOW_TEARING) != 0;

	// Frame duplication mode: SyncInterval = 2 when duplicating and no tearing
	if (ctx.ngx.isDuplicatingFrames && !ctx.reflex.isGameTearingEnabled) {
		SyncInterval = 2;
	}
	// Apply VSync override only if enabled (and not in duplication mode)
	else if (ctx.reflex.isVsyncOverrideEnabled) {
		if (ctx.reflex.isVsyncEnabled) {
			// Force VSync ON
			// If game uses tearing, we need to remove the flag and set SyncInterval
			if (ctx.reflex.isGameTearingEnabled) {
				Flags &= ~DXGI_PRESENT_ALLOW_TEARING;
			}
			SyncInterval = 1;
		}
		else {
			// Force VSync OFF
			// If game uses VSync natively, just set SyncInterval to 0
			// (we can't add ALLOW_TEARING flag - swapchain must be created with it)
			SyncInterval = 0;
		}
	}

	// Store effective SyncInterval for FPS monitor
	ctx.reflex.lastSyncInterval = SyncInterval;
	ctx.reflex.lastOriginalSyncInterval = originalSyncInterval;

	onPrePresent();

	// Dispatch pre-present1 to listeners
	SwapChainEvents::DispatchPrePresent1(pSwapChain, SyncInterval, Flags, pPresentParameters);

	// Render ImGui overlay (direct detour path)
	UxHook::RenderOverlay(pSwapChain);

	auto result = originalPresent1(pSwapChain, SyncInterval, Flags, pPresentParameters);

	// Dispatch post-present1 to listeners
	SwapChainEvents::DispatchPostPresent1(pSwapChain, SyncInterval, Flags, result);

	onPostPresent();

	return result;
}

// =============================================================================
// DetourInitThread and other existing code...
// =============================================================================

DWORD WINAPI DetourInitThread(LPVOID lpParam)
{
	Sleep(100);

	// Uninstall may have fired during the sleep; bail out before touching anything.
	if (!g_swapchainHooksActive.load(std::memory_order_acquire)) {
		return 0;
	}

	// Retrieve the virtual table (vtable) for the swapchain
	IDXGISwapChain* pSwapChain = (IDXGISwapChain*)lpParam;
	void** pVTable = *reinterpret_cast<void***>(pSwapChain);

	// Save the original Present method and attach the detour
	if (!originalPresent || presentCalled == 1) {
		LOG_WARNING(L"[DXGI] Present: external detour detected, reapplying hook");
		isPresentDetourHijacked = true;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		originalPresent = (PresentFn)pVTable[8];
		DetourAttach(&(PVOID&)originalPresent, hookedPresent);
		DetourTransactionCommit();
	}
	return 0;
}

void DetourPresent(IDXGISwapChain* pSwapChain)
{
	// Do not spawn the hijack-recovery thread while the subsystem is disabled.
	if (!g_swapchainHooksActive.load(std::memory_order_acquire)) {
		return;
	}

	void** pVTable = *reinterpret_cast<void***>(pSwapChain);
	bool isHooked = (pVTable[8] == hookedPresent);

	if (isPresentDetourHijacked && !isHooked) {
		CreateThread(NULL, 0, DetourInitThread, pSwapChain, 0, NULL);
	}
}

void DetourSwapChain1(void** pVTable)
{
	if (!pVTable) {
		LOG_ERROR(L"[DXGI] DetourSwapChain: pVTable is nullptr.");
		return;
	}

	// Teardown in progress / not yet installed - do not attach.
	if (!g_swapchainHooksActive.load(std::memory_order_acquire)) {
		return;
	}

	// Fast path: hooks already installed on a previous swapchain. The vtable slots
	// for Present / Present1 resolve to the same addresses in dxgi.dll regardless
	// of the swapchain instance, so re-attaching for every new swapchain would
	// double-patch already-patched code.
	if (originalPresent) {
		return;
	}

	// Double-checked locking: game may create swapchains from several threads.
	std::lock_guard<std::mutex> lock(swapchainDetourMutex);

	// Re-check after acquiring the lock, and re-check the master flag in case
	// Uninstall raced between the first check and the lock acquisition.
	if (!g_swapchainHooksActive.load(std::memory_order_acquire)) {
		return;
	}
	if (originalPresent) {
		return;
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	originalPresent = (PresentFn)pVTable[8];
	DetourAttach(&(PVOID&)originalPresent, hookedPresent);

	originalPresent1 = (Present1Fn)pVTable[22];
	DetourAttach(&(PVOID&)originalPresent1, hookedPresent1);
	DetourTransactionCommit();
	LOG_DEBUG(L"[DXGI] DetourSwapChain1: Detour set up for Present/Present1");
}

// =============================================================================
// Helper: Create the wrapped swapchain with command queue from pDevice
// =============================================================================

static DxgiWrappedIDXGISwapChain4* CreateWrappedSwapChain(IDXGISwapChain* pSwapChain, IUnknown* pDevice)
{
	// Try to get the command queue from pDevice.
	// In D3D12, CreateSwapChainForHwnd receives the command queue as pDevice.
	// We pass it to the wrapper so REFramework's object scan finds it immediately
	// and doesn't enter the Proton/FrameGen detection path.
	ID3D12CommandQueue* pQueue = nullptr;
	if (pDevice) {
		pDevice->QueryInterface(IID_PPV_ARGS(&pQueue));
	}

	auto* wrapped = new DxgiWrappedIDXGISwapChain4(
		pSwapChain,
		prePresentHook, postPresentHook,
		prePresent1Hook, postPresent1Hook,
		preResizeBuffersHook, postResizeBuffersHook,
		pQueue
	);

	if (pQueue) {
		pQueue->Release(); // wrapper took its own AddRef
	}

	return wrapped;
}

static DxgiWrappedIDXGISwapChain4* CreateWrappedSwapChain1(IDXGISwapChain1* pSwapChain, IUnknown* pDevice)
{
	ID3D12CommandQueue* pQueue = nullptr;
	if (pDevice) {
		pDevice->QueryInterface(IID_PPV_ARGS(&pQueue));
	}

	auto* wrapped = new DxgiWrappedIDXGISwapChain4(
		pSwapChain,
		prePresentHook, postPresentHook,
		prePresent1Hook, postPresent1Hook,
		preResizeBuffersHook, postResizeBuffersHook,
		pQueue
	);

	if (pQueue) {
		pQueue->Release();
	}

	return wrapped;
}

static bool isSwapchainBeingWrapped = false;

HRESULT WINAPI proxy_CreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
{
	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChain");
	}

	DXGI_SWAP_CHAIN_DESC descCopy = *pDesc;

	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChain: using " + std::to_wstring(descCopy.BufferCount) + L" buffer(s)");
		LOG_DEBUG(L"[DXGI] CreateSwapChain: swapchain swap effect set to " + std::to_wstring(pDesc->SwapEffect));
		LOG_DEBUG(L"[DXGI] CreateSwapChain: swapchain resolution is " + std::to_wstring(pDesc->BufferDesc.Width) + L"x" + std::to_wstring(pDesc->BufferDesc.Height));
	}

	auto wasWrapped = isSwapchainBeingWrapped;
	isSwapchainBeingWrapped = true;
	auto result = orgCreateSwapChain(pFactory, pDevice, &descCopy, ppSwapChain);
	isSwapchainBeingWrapped = wasWrapped;

	// Try to get CommandQueue for D3D12 (safely handles D3D11)
	if (result == S_OK && ppSwapChain && *ppSwapChain) {
		TryGetD3D12CommandQueue(pDevice, L"CreateSwapChain");
	}

	if (!ctx.enableDirectSwapchainHooking) {
		if (true || ctx.enableReflexInjection || ctx.reflex.isVsyncEnabled) {
			if (swapchainInstanceNumber > 1) {
				LOG_WARNING(L"[DXGI] CreateSwapChain: VSYNC feature will be disabled for stability reasons");
			}
			else if (!wasWrapped) {
				void* pCustomInterface;
				if (FAILED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain4Interface), (void**)&pCustomInterface))) {
					*ppSwapChain = CreateWrappedSwapChain(*ppSwapChain, pDevice);
				}
			}
		}
	}
	else {
		if (result == S_OK && ppSwapChain && *ppSwapChain) {
			DetourSwapChain1(*reinterpret_cast<void***>(ppSwapChain));
		}
	}

	LOG_DEBUG(L"[DXGI] CreateSwapChain: succeeded");
	return result;
}

HRESULT WINAPI proxy_CreateSwapChainForHwnd(IDXGIFactory* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc,
	const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
{
	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForHwnd");
		LOG_DEBUG(L"[DXGI] pFactory address: " + std::to_wstring(reinterpret_cast<uintptr_t>(pFactory)));
		LOG_DEBUG(L"[DXGI] pDevice address: " + std::to_wstring(reinterpret_cast<uintptr_t>(pDevice)));
		LOG_DEBUG(L"[DXGI] hWnd handle: " + std::to_wstring(reinterpret_cast<uintptr_t>(hWnd)));

		if (pDesc) {
			LOG_DEBUG(L"[DXGI] pDesc - BufferCount: " + std::to_wstring(pDesc->BufferCount));
			LOG_DEBUG(L"[DXGI] pDesc - Width: " + std::to_wstring(pDesc->Width));
			LOG_DEBUG(L"[DXGI] pDesc - Height: " + std::to_wstring(pDesc->Height));
			LOG_DEBUG(L"[DXGI] pDesc - Format: " + std::to_wstring(pDesc->Format));
			LOG_DEBUG(L"[DXGI] pDesc - SwapEffect: " + std::to_wstring(pDesc->SwapEffect));
			LOG_DEBUG(L"[DXGI] pDesc - SampleDesc.Count: " + std::to_wstring(pDesc->SampleDesc.Count));
			LOG_DEBUG(L"[DXGI] pDesc - SampleDesc.Quality: " + std::to_wstring(pDesc->SampleDesc.Quality));
			LOG_DEBUG(L"[DXGI] pDesc - Flags: " + std::to_wstring(pDesc->Flags));
		}
		else {
			LOG_DEBUG(L"[DXGI] pDesc is nullptr");
		}

		if (pFullscreenDesc) {
			LOG_DEBUG(L"[DXGI] pFullscreenDesc - RefreshRate.Numerator: " + std::to_wstring(pFullscreenDesc->RefreshRate.Numerator));
			LOG_DEBUG(L"[DXGI] pFullscreenDesc - RefreshRate.Denominator: " + std::to_wstring(pFullscreenDesc->RefreshRate.Denominator));
			LOG_DEBUG(L"[DXGI] pFullscreenDesc - ScanlineOrdering: " + std::to_wstring(pFullscreenDesc->ScanlineOrdering));
			LOG_DEBUG(L"[DXGI] pFullscreenDesc - Scaling: " + std::to_wstring(pFullscreenDesc->Scaling));
			LOG_DEBUG(L"[DXGI] pFullscreenDesc - Windowed: " + std::to_wstring(pFullscreenDesc->Windowed));
		}
		else {
			LOG_DEBUG(L"[DXGI] pFullscreenDesc is nullptr");
		}

		LOG_DEBUG(L"[DXGI] pRestrictToOutput address: " + std::to_wstring(reinterpret_cast<uintptr_t>(pRestrictToOutput)));
		LOG_DEBUG(L"[DXGI] ppSwapChain address: " + std::to_wstring(reinterpret_cast<uintptr_t>(ppSwapChain)));
		if (ppSwapChain && *ppSwapChain) {
			LOG_DEBUG(L"[DXGI] ppSwapChain points to valid IDXGISwapChain1 object.");
		}
		else {
			LOG_DEBUG(L"[DXGI] ppSwapChain is nullptr or points to nullptr.");
		}
	}

	UxHook::OnSwapChainAboutToBeCreated(hWnd);

	DXGI_SWAP_CHAIN_DESC1 descCopy = *pDesc;

	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForHwnd: using " + std::to_wstring(descCopy.BufferCount) + L" buffer(s)");
		LOG_DEBUG(L"[DXGI] CreateSwapChainForHwnd: swapchain swap effect set to " + std::to_wstring(pDesc->SwapEffect));
		LOG_DEBUG(L"[DXGI] CreateSwapChainForHwnd: swapchain resolution is " + std::to_wstring(pDesc->Width) + L"x" + std::to_wstring(pDesc->Height));
	}

	auto wasWrapped = isSwapchainBeingWrapped;
	isSwapchainBeingWrapped = true;

	auto result = orgCreateSwapChainForHwnd(pFactory, pDevice, hWnd, &descCopy, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	if (result == S_OK && ppSwapChain && *ppSwapChain) {
		// Try to get CommandQueue for D3D12 (safely handles D3D11)
		TryGetD3D12CommandQueue(pDevice, L"CreateSwapChainForHwnd");
	}

	isSwapchainBeingWrapped = wasWrapped;

	if (!ctx.enableDirectSwapchainHooking) {
		if (true || ctx.enableReflexInjection || ctx.reflex.isVsyncEnabled) {
			if (swapchainInstanceNumber > 1) {
				LOG_WARNING(L"[DXGI] CreateSwapChainForHwnd: VSYNC feature will be disabled for stability reasons");
			}
			else if (!wasWrapped) {
				void* pCustomInterface;
				if (FAILED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain4Interface), (void**)&pCustomInterface))) {
					*ppSwapChain = reinterpret_cast<IDXGISwapChain1*>(
						CreateWrappedSwapChain1(*ppSwapChain, pDevice)
						);
				}
			}
		}
	}
	else {
		if (result == S_OK && ppSwapChain && *ppSwapChain) {
			DetourSwapChain1(*reinterpret_cast<void***>(ppSwapChain));
		}
	}

	if (ctx.logging.isDxgiDebugEnabled) {
		if (result == S_OK) {
			LOG_DEBUG(L"[DXGI] CreateSwapChainForHwnd: succeeded");
		}
		else {
			LOG_DEBUG(L"[DXGI] CreateSwapChainForHwnd: failed (" + std::to_wstring(result) + L")");
		}
	}

	return result;
}

HRESULT WINAPI proxy_CreateSwapChainForCoreWindow(IDXGIFactory* pFactory, IUnknown* pDevice, IUnknown* pWindow, const DXGI_SWAP_CHAIN_DESC1* pDesc,
	IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
{
	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForCoreWindow");
	}

	DXGI_SWAP_CHAIN_DESC1 descCopy = *pDesc;

	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForCoreWindow: " + std::to_wstring(descCopy.Width) + L"x" + std::to_wstring(descCopy.Height));
		LOG_DEBUG(L"[DXGI] CreateSwapChainForCoreWindow: using " + std::to_wstring(descCopy.BufferCount) + L" buffer(s)");
		LOG_DEBUG(L"[DXGI] CreateSwapChainForCoreWindow: swapchain swap effect set to " + std::to_wstring(pDesc->SwapEffect));
		LOG_DEBUG(L"[DXGI] CreateSwapChainForCoreWindow: swapchain resolution is " + std::to_wstring(descCopy.Width) + L"x" + std::to_wstring(descCopy.Height));
	}

	auto wasWrapped = isSwapchainBeingWrapped;
	isSwapchainBeingWrapped = true;
	auto result = orgCreateSwapChainForCoreWindow(pFactory, pDevice, pWindow, &descCopy, pRestrictToOutput, ppSwapChain);
	isSwapchainBeingWrapped = wasWrapped;
	if (result == S_OK && ppSwapChain && *ppSwapChain) {
		// Try to get CommandQueue for D3D12 (safely handles D3D11)
		TryGetD3D12CommandQueue(pDevice, L"CreateSwapChainForCoreWindow");
	}

	if (!ctx.enableDirectSwapchainHooking) {
		if (true || ctx.enableReflexInjection || ctx.reflex.isVsyncEnabled) {
			if (swapchainInstanceNumber > 1) {
				LOG_WARNING(L"[DXGI] CreateSwapChainForCoreWindow: VSYNC feature will be disabled for stability reasons");
			}
			else if (!wasWrapped) {
				void* pCustomInterface;
				if (FAILED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain4Interface), (void**)&pCustomInterface))) {
					*ppSwapChain = reinterpret_cast<IDXGISwapChain1*>(
						CreateWrappedSwapChain1(*ppSwapChain, pDevice)
						);
				}
			}
		}
	}
	else {
		if (result == S_OK && ppSwapChain && *ppSwapChain) {
			DetourSwapChain1(*reinterpret_cast<void***>(ppSwapChain));
		}
	}

	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForCoreWindow: succeeded");
	}
	return result;
}

HRESULT WINAPI proxy_CreateSwapChainForComposition(IDXGIFactory* pFactory, IUnknown* pDevice, const DXGI_SWAP_CHAIN_DESC1* pDesc,
	IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
{
	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForComposition");
	}
	DXGI_SWAP_CHAIN_DESC1 descCopy = *pDesc;

	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForComposition: " + std::to_wstring(descCopy.Width) + L"x" + std::to_wstring(descCopy.Height));
		LOG_DEBUG(L"[DXGI] CreateSwapChainForComposition: using " + std::to_wstring(descCopy.BufferCount) + L" buffer(s)");
		LOG_DEBUG(L"[DXGI] CreateSwapChainForComposition: swapchain swap effect set to " + std::to_wstring(pDesc->SwapEffect));
		LOG_DEBUG(L"[DXGI] CreateSwapChainForComposition: swapchain resolution is " + std::to_wstring(descCopy.Width) + L"x" + std::to_wstring(descCopy.Height));
	}

	auto wasWrapped = isSwapchainBeingWrapped;
	isSwapchainBeingWrapped = true;
	auto result = orgCreateSwapChainForComposition(pFactory, pDevice, &descCopy, pRestrictToOutput, ppSwapChain);
	isSwapchainBeingWrapped = wasWrapped;
	if (result == S_OK && ppSwapChain && *ppSwapChain) {
		// Try to get CommandQueue for D3D12 (safely handles D3D11)
		TryGetD3D12CommandQueue(pDevice, L"CreateSwapChainForComposition");
	}

	if (!ctx.enableDirectSwapchainHooking) {
		if (true || ctx.enableReflexInjection || ctx.reflex.isVsyncEnabled) {
			if (swapchainInstanceNumber > 1) {
				LOG_WARNING(L"[DXGI] CreateSwapChainForComposition: VSYNC feature will be disabled for stability reasons");
			}
			else if (!wasWrapped) {
				void* pCustomInterface;
				if (FAILED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain4Interface), (void**)&pCustomInterface))) {
					*ppSwapChain = reinterpret_cast<IDXGISwapChain1*>(
						CreateWrappedSwapChain1(*ppSwapChain, pDevice)
						);
				}
			}
		}
	}
	else {
		if (result == S_OK && ppSwapChain && *ppSwapChain) {
			DetourSwapChain1(*reinterpret_cast<void***>(ppSwapChain));
		}
	}

	if (ctx.logging.isDxgiDebugEnabled) {
		LOG_DEBUG(L"[DXGI] CreateSwapChainForComposition: succeeded");
	}
	return result;
}

// =============================================================================
// Subsystem lifecycle - called by HookDxgi::Install / Uninstall
// =============================================================================

void EnableSwapchainHooks()
{
	g_swapchainHooksActive.store(true, std::memory_order_release);
}

void DisableSwapchainHooks()
{
	// Block any new attach attempts from DetourSwapChain1 / DetourInitThread /
	// DetourPresent. Existing in-flight calls holding swapchainDetourMutex will
	// re-check the flag after acquiring the lock and bail out.
	g_swapchainHooksActive.store(false, std::memory_order_release);
}

void TeardownSwapchainProxy()
{
	// Serialize against any DetourSwapChain1 call that already passed the flag
	// check but not yet reached the transaction. DisableSwapchainHooks() must
	// have been called before this; the mutex just ensures we wait out any
	// in-flight attach that started before the flag was lowered.
	std::lock_guard<std::mutex> lock(swapchainDetourMutex);

	// Nothing to do if no hooks were ever installed.
	bool anyInstalled = originalPresent || originalPresent1
		|| orgCreateSwapChain || orgCreateSwapChainForHwnd
		|| orgCreateSwapChainForCoreWindow || orgCreateSwapChainForComposition;
	if (!anyInstalled) {
		return;
	}

	LOG_INFO(L"[DXGI] Unhooking swap chain functions...");

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	// Detach in reverse order of attach. Trampoline pointers are intentionally
	// NOT nulled afterwards: any in-flight hookedPresent / proxy_CreateSwapChain*
	// handler on another thread may still read them. Post-detach they point at
	// the original function in dxgi.dll (Detours restored the first bytes), so
	// invoking them is safe and simply calls through to the real implementation.
	if (originalPresent1) {
		DetourDetach(&(PVOID&)originalPresent1, hookedPresent1);
	}
	if (originalPresent) {
		DetourDetach(&(PVOID&)originalPresent, hookedPresent);
	}
	if (orgCreateSwapChainForComposition) {
		DetourDetach(&(PVOID&)orgCreateSwapChainForComposition, proxy_CreateSwapChainForComposition);
	}
	if (orgCreateSwapChainForCoreWindow) {
		DetourDetach(&(PVOID&)orgCreateSwapChainForCoreWindow, proxy_CreateSwapChainForCoreWindow);
	}
	if (orgCreateSwapChainForHwnd) {
		DetourDetach(&(PVOID&)orgCreateSwapChainForHwnd, proxy_CreateSwapChainForHwnd);
	}
	if (orgCreateSwapChain) {
		DetourDetach(&(PVOID&)orgCreateSwapChain, proxy_CreateSwapChain);
	}

	if (DetourTransactionCommit() != NO_ERROR) {
		LOG_ERROR(L"[DXGI] Failed to detach swap chain hooks");
		return;
	}

	LOG_INFO(L"[DXGI] Swap chain functions unhooked");
}