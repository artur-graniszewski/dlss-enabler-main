#include "Common.h"
#include <string>
#include <sstream>
#include "dxgi1_6.h"
#include "../Detours/detours.h"
#include "../Core/Context.h"
#include "DxgiProxy.h"
#include "SwapchainProxy.h"
#include <mutex>
#include <atomic>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

namespace DXGI
{
	// Master switch for the adapter/factory-level vtable hooks installed by
	// AttachToFactory / AttachToAdapter / EnsureSwapChainDetours.
	// Raised at the end of HookDxgi::Install, lowered at the start of
	// HookDxgi::Uninstall so that TeardownDxgi() can detach from a quiescent state.
	static std::atomic<bool> g_dxgiHooksActive{ false };

	//UINT dxgiVendorId = 0x10EE; // AMD
	UINT dxgiVendorId = 0x10de; // NVIDIA
	UINT dxgiDeviceId = 1;
	//UINT dxgiDeviceId = 0x2684; // RTX 4090
	//UINT dxgiDeviceId = 0x2C05; // RTX 5070 TI
	SIZE_T dxgiDedicatedVideoMemory = 0;
	std::wstring modelName = L"";
	unsigned int DxgiVersion = 0;
	static std::mutex dxgiHookMutex;
	static std::mutex dxgiFactoryMutex;
	static std::mutex dxgiAdapterHookMutex;
	static bool isDxgiInitialized = true;

	typedef HRESULT(WINAPI* PFN_GetDesc)(IDXGIAdapter* This, DXGI_ADAPTER_DESC* pDesc);
	typedef HRESULT(WINAPI* PFN_GetDesc1)(IDXGIAdapter1* This, DXGI_ADAPTER_DESC1* pDesc);
	typedef HRESULT(WINAPI* PFN_GetDesc2)(IDXGIAdapter2* This, DXGI_ADAPTER_DESC2* pDesc);
	typedef HRESULT(WINAPI* PFN_GetDesc3)(IDXGIAdapter4* This, DXGI_ADAPTER_DESC3* pDesc);
	typedef HRESULT(WINAPI* PFN_EnumAdapterByGpuPreference)(IDXGIFactory6* This, UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void** ppvAdapter);
	typedef HRESULT(WINAPI* PFN_EnumAdapterByLuid)(IDXGIFactory4* This, LUID AdapterLuid, REFIID riid, void** ppvAdapter);
	typedef HRESULT(WINAPI* PFN_EnumAdapters1)(IDXGIFactory1* This, UINT Adapter, IDXGIAdapter1** ppAdapter);
	typedef HRESULT(WINAPI* PFN_EnumAdapters)(IDXGIFactory* This, UINT Adapter, IDXGIAdapter** ppAdapter);

	extern CreateDXGIFactory_t orgCreateDXGIFactory;
	extern CreateDXGIFactory1_t orgCreateDXGIFactory1;
	extern CreateDXGIFactory2_t orgCreateDXGIFactory2;

	PFN_GetDesc orgGetDesc;
	PFN_GetDesc1 orgGetDesc1;
	PFN_GetDesc2 orgGetDesc2;
	PFN_GetDesc3 orgGetDesc3;

	PFN_EnumAdapters orgEnumAdapters;
	PFN_EnumAdapters1 orgEnumAdapters1;
	PFN_EnumAdapterByLuid orgEnumAdapterByLuid;
	PFN_EnumAdapterByGpuPreference orgEnumAdapterByGpuPreference;

	void WINAPI AttachToFactory(IUnknown* unkFactory);
	void WINAPI AttachToAdapter(IUnknown* unkAdapter);
	void DlssEnablerLogger(const char* message, unsigned int loggingLevel, const char* sourceComponent);
	UINT WINAPI DlssEnablerInit(UINT newVendorId, UINT newDeviceId, const char* newModelName, SIZE_T newDedicatedVideoMemory = 0);

	// Forward declarations of vtable detour handlers - referenced by TeardownDxgi()
	// which sits above their definitions.
	HRESULT WINAPI GetDesc(IDXGIAdapter* This, DXGI_ADAPTER_DESC* pDesc);
	HRESULT WINAPI GetDesc1(IDXGIAdapter1* This, DXGI_ADAPTER_DESC1* pDesc);
	HRESULT WINAPI GetDesc2(IDXGIAdapter2* This, DXGI_ADAPTER_DESC2* pDesc);
	HRESULT WINAPI GetDesc3(IDXGIAdapter4* This, DXGI_ADAPTER_DESC3* pDesc);
	HRESULT WINAPI EnumAdapters(IDXGIFactory* This, UINT Adapter, IDXGIAdapter** ppAdapter);
	HRESULT WINAPI EnumAdapters1(IDXGIFactory1* This, UINT Adapter, IDXGIAdapter1** ppAdapter);
	HRESULT WINAPI EnumAdapterByLuid(IDXGIFactory4* This, LUID AdapterLuid, REFIID riid, void** ppvAdapter);
	HRESULT WINAPI EnumAdapterByGpuPreference(IDXGIFactory6* This, UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void** ppvAdapter);

	// Subsystem lifecycle - defined below, called by HookDxgi::Install / Uninstall.
	void EnableDxgiHooks();
	void DisableDxgiHooks();
	void TeardownDxgi();

	template< typename T >
	static std::wstring int_to_hex(T i)
	{
		std::wstringstream stream;
		stream << L"(0x"
			<< std::setfill(L'0')
			<< std::setw(sizeof(T) * 2)
			<< std::hex << i;
		return stream.str() + L")";
	}

	std::wstring WhoIsTheCaller(void* returnAddress)
	{
		HMODULE hModule = NULL;
		char callerPath[MAX_PATH] = { 0 };

		// Get the return address from the current function call.
		// void* returnAddress = _ReturnAddress();

		// Get the base address of the module containing the return address.
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)returnAddress, &hModule))
		{
			// Get the full path of the calling module.
			GetModuleFileNameA(hModule, callerPath, sizeof(callerPath));
			auto path = std::filesystem::path(callerPath);

			return path.filename().wstring();
		}

		return L"";
	}

	static bool EnableSpoofing(void* returnAddress)
	{
		if (ctx.streamline.isSelectiveSpoofingEnabled && WhoIsTheCaller(returnAddress) == Common::GetProcessFileName()) {
			//LOG_WARNING(L"[DXGI] Selective GPU Spoofing applied");
			//ctx.streamline.isSelectiveSpoofingEnabled = false;
			//return false;
		}

		static bool reported = false;
		if (GetModuleHandleW(L"OptiPatcher.asi") && ctx.isOptiPatcherActive) {
			if (!reported) {
				LOG_WARNING(L"[DXGI] OptiPatcher detected, disabling internal VendorID override");
				reported = true;
			}
			return false;
		}
		return true;
	}

	std::wstring LuidToHexString(LUID luid)
	{
		// Convert LUID to hexadecimal string
		std::wstringstream ss;
		ss << std::hex << std::setw(8) << std::setfill(L'0') << luid.HighPart << std::setw(8) << std::setfill(L'0') << luid.LowPart;

		return ss.str();
	}

	extern "C" __declspec(dllexport) void DlssEnablerLogger(const char* message, unsigned int loggingLevel, const char* sourceComponent)
	{
		if (loggingLevel > 1 && !ctx.logging.isUltraDebugEnabled) {
			return;
		}

		if (loggingLevel == 1 && !ctx.logging.isExtraDebugEnabled) {
			return;
		}

		std::string msg = std::string(message);
		std::string component = std::string(sourceComponent);
		LOG_INFO(L"[" + ToWideString(component) + L"] " + std::wstring(msg.begin(), msg.end()));
	}

	void InitDxgi(HMODULE dxgi)
	{
		static bool dxgiLoadReported = false;
		FARPROC pFunc = GetProcAddress(dxgi, "DlssEnablerInit");
		if (pFunc != NULL && false) {
			auto DxgiInit = reinterpret_cast<decltype(&DlssEnablerInit)>(pFunc);
			unsigned int version = DxgiInit(0, 0, ctx.gpu.desiredDeviceName, ctx.gpu.desiredDedicatedVideoMemory);
			DxgiVersion = version;

			if (version >= 14000) {
				auto DxgiLoggerSet = reinterpret_cast<decltype(&DlssEnablerLoggerSet)>(GetProcAddress(dxgi, "DlssEnablerLoggerSet"));
				if (DxgiLoggerSet) {
					DxgiLoggerSet((void*)DlssEnablerLogger);
				}
			}

			if (!dxgiLoadReported) {
				unsigned int major = version / 10000;
				unsigned int minor = (version / 100) % 100;
				unsigned int patch = version % 100;
				LOG_INFO(L"[LOADER] Bundled DXGI API initialized successfully (version " + std::to_wstring(major) + L"." + std::to_wstring(minor) + L"." + std::to_wstring(patch) + L")");
				//ctx.isLoadedByDxgi = true;
			}
		}
		else {
			if (!dxgiLoadReported) {
				WCHAR filename[MAX_PATH];
				GetModuleFileNameW(dxgi, filename, MAX_PATH);
				LOG_INFO(L"[LOADER] External DXGI API initialized successfully: " + std::wstring(filename));
			}
		}

		BundledDxgiInit(0, 0, ctx.gpu.desiredDeviceName, ctx.gpu.desiredDedicatedVideoMemory);
		dxgiLoadReported = true;
	}

	// Raise the master flag - from this point on AttachToFactory / AttachToAdapter /
	// EnsureSwapChainDetours are allowed to install hooks.
	void EnableDxgiHooks()
	{
		g_dxgiHooksActive.store(true, std::memory_order_release);
	}

	// Lower the master flag - block any further attach from the lazy attach points.
	// In-flight attaches holding a mutex will re-check the flag after acquiring it
	// and bail out. Must be called before TeardownDxgi().
	void DisableDxgiHooks()
	{
		g_dxgiHooksActive.store(false, std::memory_order_release);
	}

	// Symmetric counterpart to the detours installed by AttachToFactory and
	// AttachToAdapter. Detaches all eight vtable-level trampolines in one
	// transaction. Must be called after DisableDxgiHooks() so that no concurrent
	// AttachTo* can race the detach.
	//
	// Trampoline pointers are NOT nulled afterwards: any in-flight EnumAdapters*
	// / GetDesc* handler on another thread may still read them. Post-detach they
	// point at the original function in dxgi.dll, so invoking them is safe.
	void TeardownDxgi()
	{
		// Take both adapter and factory locks. These are never held together
		// elsewhere so the order is arbitrary; we pick factory-then-adapter and
		// keep it consistent should anyone ever need both.
		std::lock_guard<std::mutex> lockFactory(dxgiFactoryMutex);
		std::lock_guard<std::mutex> lockAdapter(dxgiAdapterHookMutex);

		bool anyInstalled = orgEnumAdapters || orgEnumAdapters1
			|| orgEnumAdapterByLuid || orgEnumAdapterByGpuPreference
			|| orgGetDesc || orgGetDesc1 || orgGetDesc2 || orgGetDesc3;
		if (!anyInstalled) {
			return;
		}

		LOG_INFO(L"[DXGI] Unhooking adapter/factory vtable entries...");

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		// Detach adapter-level hooks (installed by AttachToAdapter), reverse order.
		if (orgGetDesc3) {
			DetourDetach(&(PVOID&)orgGetDesc3, GetDesc3);
		}
		if (orgGetDesc2) {
			DetourDetach(&(PVOID&)orgGetDesc2, GetDesc2);
		}
		if (orgGetDesc1) {
			DetourDetach(&(PVOID&)orgGetDesc1, GetDesc1);
		}
		if (orgGetDesc) {
			DetourDetach(&(PVOID&)orgGetDesc, GetDesc);
		}

		// Detach factory-level hooks (installed by AttachToFactory), reverse order.
		if (orgEnumAdapterByGpuPreference) {
			DetourDetach(&(PVOID&)orgEnumAdapterByGpuPreference, EnumAdapterByGpuPreference);
		}
		if (orgEnumAdapterByLuid) {
			DetourDetach(&(PVOID&)orgEnumAdapterByLuid, EnumAdapterByLuid);
		}
		if (orgEnumAdapters1) {
			DetourDetach(&(PVOID&)orgEnumAdapters1, EnumAdapters1);
		}
		if (orgEnumAdapters) {
			DetourDetach(&(PVOID&)orgEnumAdapters, EnumAdapters);
		}

		if (DetourTransactionCommit() != NO_ERROR) {
			LOG_ERROR(L"[DXGI] Failed to detach adapter/factory vtable hooks");
			return;
		}

		LOG_INFO(L"[DXGI] Adapter/factory vtable entries unhooked");
	}

#pragma region Adapter

	HRESULT WINAPI GetDesc3(IDXGIAdapter4* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC3* pDesc)
	{
		LOG_TRACE(L"[DXGI] IDXGIAdapter4.GetDesc3");

		auto result = orgGetDesc3(This, pDesc);

		if (SUCCEEDED(result)) {
			if (dxgiDedicatedVideoMemory > 0) {
				pDesc->DedicatedVideoMemory = dxgiDedicatedVideoMemory;
			}

			std::wstring name = modelName;
			if (name != L"") {
				const wchar_t* szName = name.c_str();
				std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
				std::memcpy(pDesc->Description, szName, 54);
			}

			if (pDesc->VendorId != 0x10de) {
				size_t size = sizeof(pDesc->Description) / sizeof(pDesc->Description[0]);
				// Shift existing characters to the right
				memmove(pDesc->Description + 1, pDesc->Description, (size - 1) * sizeof(WCHAR));

				// Prepend space character
				pDesc->Description[0] = L' ';
			}



			//LOG_WARNING(L"[DXGI] Called by " + WhoIsTheCaller(_ReturnAddress()));
			if (EnableSpoofing(_ReturnAddress())) {
				if (dxgiVendorId > 1) {
					pDesc->VendorId = dxgiVendorId;
				}

				if (dxgiDeviceId > 1) {
					pDesc->DeviceId = dxgiDeviceId;
				}
			}

			for (size_t i = 0; i < sizeof(pDesc->Description); ++i) {
				if (pDesc->Description[i] == L' ') {
					//pDesc->Description[i] = L'\u200B';
				}
			}

			LOG_TRACE(L"[DXGI] IDXGIAdapter4.GetDesc3: succeeded");
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIAdapter4.GetDesc3: failed " + int_to_hex(result));
		}

		AttachToAdapter(This);

		return result;
	}

	HRESULT WINAPI GetDesc2(IDXGIAdapter2* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC2* pDesc)
	{
		LOG_TRACE(L"[DXGI] IDXGIAdapter2.GetDesc2");

		auto result = orgGetDesc2(This, pDesc);

		if (SUCCEEDED(result)) {
			if (dxgiDedicatedVideoMemory > 0) {
				pDesc->DedicatedVideoMemory = dxgiDedicatedVideoMemory;
			}

			std::wstring name = modelName;
			if (name != L"") {
				const wchar_t* szName = name.c_str();
				std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
				std::memcpy(pDesc->Description, szName, 54);
			}

			if (pDesc->VendorId != 0x10de) {
				size_t size = sizeof(pDesc->Description) / sizeof(pDesc->Description[0]);
				// Shift existing characters to the right
				memmove(pDesc->Description + 1, pDesc->Description, (size - 1) * sizeof(WCHAR));

				// Prepend space character
				pDesc->Description[0] = L' ';
			}

			//LOG_WARNING(L"[DXGI] Called by " + WhoIsTheCaller(_ReturnAddress()));
			if (EnableSpoofing(_ReturnAddress())) {
				if (dxgiVendorId > 1) {
					pDesc->VendorId = dxgiVendorId;
				}

				if (dxgiDeviceId > 1) {
					pDesc->DeviceId = dxgiDeviceId;
				}
			}

			for (size_t i = 0; i < sizeof(pDesc->Description); ++i) {
				if (pDesc->Description[i] == L' ') {
					//pDesc->Description[i] = L'\u200B';
				}
			}

			LOG_TRACE(L"[DXGI] IDXGIAdapter2.GetDesc2: succeeded");
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIAdapter2.GetDesc2: failed " + int_to_hex(result));
		}

		AttachToAdapter(This);

		return result;
	}

	HRESULT WINAPI GetDesc1(IDXGIAdapter1* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC1* pDesc)
	{
		LOG_TRACE(L"[DXGI] IDXGIAdapter1.GetDesc1");

		auto result = orgGetDesc1(This, pDesc);

		if (SUCCEEDED(result)) {
			if (dxgiDedicatedVideoMemory > 0) {
				pDesc->DedicatedVideoMemory = dxgiDedicatedVideoMemory;
			}

			std::wstring name = modelName;
			if (name != L"") {
				const wchar_t* szName = name.c_str();
				std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
				std::memcpy(pDesc->Description, szName, 54);
			}

			if (pDesc->VendorId != 0x10de) {
				size_t size = sizeof(pDesc->Description) / sizeof(pDesc->Description[0]);
				// Shift existing characters to the right
				memmove(pDesc->Description + 1, pDesc->Description, (size - 1) * sizeof(WCHAR));

				// Prepend space character
				pDesc->Description[0] = L' ';
			}

			//LOG_WARNING(L"[DXGI] Called by " + WhoIsTheCaller(_ReturnAddress()));
			if (EnableSpoofing(_ReturnAddress())) {
				if (dxgiVendorId > 1) {
					pDesc->VendorId = dxgiVendorId;
				}

				if (dxgiDeviceId > 1) {
					pDesc->DeviceId = dxgiDeviceId;
				}
			}

			LOG_TRACE(L"[DXGI] IDXGIAdapter1.GetDesc1: succeeded");
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIAdapter1.GetDesc1: failed " + int_to_hex(result));
		}

		AttachToAdapter(This);

		return result;
	}

	HRESULT WINAPI GetDesc(IDXGIAdapter* This, /* [annotation][out] */ _Out_  DXGI_ADAPTER_DESC* pDesc)
	{
		LOG_TRACE(L"[DXGI] IDXGIAdapter.GetDesc");

		auto result = orgGetDesc(This, pDesc);

		if (SUCCEEDED(result)) {
			if (dxgiDedicatedVideoMemory > 0) {
				pDesc->DedicatedVideoMemory = dxgiDedicatedVideoMemory;
			}

			std::wstring name = modelName;
			if (name != L"") {
				const wchar_t* szName = name.c_str();
				std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
				std::memcpy(pDesc->Description, szName, 54);
			}

			if (pDesc->VendorId != 0x10de) {
				size_t size = sizeof(pDesc->Description) / sizeof(pDesc->Description[0]);
				// Shift existing characters to the right
				memmove(pDesc->Description + 1, pDesc->Description, (size - 1) * sizeof(WCHAR));

				// Prepend space character
				pDesc->Description[0] = L' ';
			}

			//LOG_WARNING(L"[DXGI] Called by " + WhoIsTheCaller(_ReturnAddress()));
			if (EnableSpoofing(_ReturnAddress())) {
				if (dxgiVendorId > 1) {
					pDesc->VendorId = dxgiVendorId;
				}

				if (dxgiDeviceId > 1) {
					pDesc->DeviceId = dxgiDeviceId;
				}
			}

			LOG_TRACE(L"[DXGI] IDXGIAdapter.GetDesc: succeeded");
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIAdapter.GetDesc: failed " + int_to_hex(result));
		}

		AttachToAdapter(This);

		return result;
	}

#pragma endregion

#pragma region Factory

	HRESULT WINAPI EnumAdapterByGpuPreference(IDXGIFactory6* This, /* [annotation] */ _In_  UINT Adapter, /* [annotation] */ _In_  DXGI_GPU_PREFERENCE GpuPreference, /* [annotation] */ _In_  REFIID riid, /* [annotation] */ _COM_Outptr_  void** ppvAdapter)
	{
		LOG_TRACE(L"[DXGI] IDXGIFactory6.EnumAdapterByGpuPreference");

		AttachToFactory(This);

		IDXGIAdapter* adapter;
		auto result = orgEnumAdapterByGpuPreference(This, Adapter, GpuPreference, riid, (void**)&adapter);

		if (SUCCEEDED(result)) {
			AttachToAdapter(adapter);
			*ppvAdapter = adapter;
			LOG_TRACE(L"[DXGI] IDXGIFactory6.EnumAdapterByGpuPreference: succeeded");
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIFactory6.EnumAdapterByGpuPreference: failed " + int_to_hex(result));
		}

		return result;
	}

	HRESULT WINAPI EnumAdapterByLuid(IDXGIFactory4* This, /* [annotation] */ _In_  LUID AdapterLuid, /* [annotation] */ _In_  REFIID riid, /* [annotation] */ _COM_Outptr_  void** ppvAdapter)
	{
		LOG_TRACE(L"[DXGI] IDXGIFactory4.EnumAdapterByLuid: LUID: " + LuidToHexString(AdapterLuid));

		AttachToFactory(This);

		IDXGIAdapter* adapter;
		auto result = orgEnumAdapterByLuid(This, AdapterLuid, riid, (void**)&adapter);

		if (SUCCEEDED(result)) {
			AttachToAdapter(adapter);
			*ppvAdapter = adapter;
			LOG_TRACE(L"[DXGI] IDXGIFactory4.EnumAdapterByLuid: succeeded");
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIFactory4.EnumAdapterByLuid: failed " + int_to_hex(result));
		}

		return result;
	}

	HRESULT WINAPI EnumAdapters1(IDXGIFactory1* This, /* [in] */ UINT Adapter, /* [annotation][out] */ _COM_Outptr_  IDXGIAdapter1** ppAdapter)
	{
		LOG_TRACE(L"[DXGI] IDXGIFactory1.EnumAdapters1: adapter: " + std::to_wstring(Adapter));

		AttachToFactory(This);

		IDXGIAdapter1* adapter;
		auto result = orgEnumAdapters1(This, Adapter, &adapter);

		if (SUCCEEDED(result)) {
			AttachToAdapter(adapter);
			*ppAdapter = adapter;
			LOG_TRACE(L"[DXGI] IDXGIFactory1.EnumAdapters1: succeeded");
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIFactory1.EnumAdapters1: failed " + int_to_hex(result));
		}

		return result;
	}

	HRESULT WINAPI EnumAdapters(IDXGIFactory* This, /* [in] */ UINT Adapter, /* [annotation][out] */ _COM_Outptr_  IDXGIAdapter** ppAdapter)
	{
		LOG_TRACE(L"[DXGI] IDXGIFactory.EnumAdapters: adapter: " + std::to_wstring(Adapter));

		AttachToFactory(This);

		IDXGIAdapter* adapter;
		auto result = orgEnumAdapters(This, Adapter, &adapter);

		if (SUCCEEDED(result)) {
			LOG_TRACE(L"[DXGI] IDXGIFactory.EnumAdapters: succeeded");
			AttachToAdapter(adapter);
			*ppAdapter = adapter;
		}
		else {
			LOG_TRACE(L"[DXGI] IDXGIFactory.EnumAdapters: failed " + int_to_hex(result));
		}

		return result;
	}

#pragma endregion

	static UINT BundledDxgiInit(UINT newVendorId, UINT newDeviceId, const char* newModelName, SIZE_T newDedicatedVideoMemory = 0)
	{
		isDxgiInitialized = true;
		if (newDedicatedVideoMemory > 0) {
			dxgiDedicatedVideoMemory = newDedicatedVideoMemory;
		}

		if (newModelName != nullptr) {
			modelName = std::wstring(newModelName, newModelName + strlen(newModelName));
		}

		if (newVendorId > 0) {
			dxgiVendorId = newVendorId;
		}
		if (newDeviceId > 0) {
			dxgiDeviceId = newDeviceId;
		}

		return 16000;
	}

	static void EnsureSwapChainDetours(IUnknown* factoryUnknown)
	{
		// Teardown in progress / subsystem disabled - do not install new hooks.
		if (!g_dxgiHooksActive.load(std::memory_order_acquire)) {
			return;
		}

		if (orgCreateSwapChain != nullptr)
			return;

		std::lock_guard<std::mutex> lock(dxgiHookMutex);

		// Re-check after acquiring the lock.
		if (!g_dxgiHooksActive.load(std::memory_order_acquire)) {
			return;
		}
		if (orgCreateSwapChain != nullptr) {
			return;
		}

		IDXGIFactory4* factory4 = nullptr;
		HRESULT hr = E_FAIL;

		if (factoryUnknown) {
			hr = factoryUnknown->QueryInterface(__uuidof(IDXGIFactory4),
				(void**)&factory4);
		}

		if (FAILED(hr) || !factory4) {
			hr = orgCreateDXGIFactory1(__uuidof(IDXGIFactory4),
				(void**)&factory4);
		}

		if (FAILED(hr) || !factory4) {
			LOG_ERROR(L"[DXGI] Failed to obtain IDXGIFactory4 for swapchain detours");
			return;
		}

		LOG_INFO(L"[DXGI] Hooking swap chain functions...");
		void** vtbl = *reinterpret_cast<void***>(factory4);

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		if (!orgCreateSwapChain) {
			orgCreateSwapChain = (PFN_CreateSwapChain)vtbl[10];
			DetourAttach(&(PVOID&)orgCreateSwapChain, proxy_CreateSwapChain);
			LOG_INFO(L"[DXGI] CreateSwapChain attached");
		}

		if (!orgCreateSwapChainForHwnd) {
			orgCreateSwapChainForHwnd = (PFN_CreateSwapChainForHwnd)vtbl[15];
			DetourAttach(&(PVOID&)orgCreateSwapChainForHwnd, proxy_CreateSwapChainForHwnd);
			LOG_INFO(L"[DXGI] CreateSwapChainForHwnd attached");
		}

		if (!orgCreateSwapChainForCoreWindow) {
			orgCreateSwapChainForCoreWindow =
				(PFN_CreateSwapChainForCoreWindow)vtbl[16];
			DetourAttach(&(PVOID&)orgCreateSwapChainForCoreWindow,
				proxy_CreateSwapChainForCoreWindow);
			LOG_INFO(L"[DXGI] CreateSwapChainForCoreWindow attached");
		}

		if (!orgCreateSwapChainForComposition) {
			orgCreateSwapChainForComposition =
				(PFN_CreateSwapChainForComposition)vtbl[24];
			DetourAttach(&(PVOID&)orgCreateSwapChainForComposition,
				proxy_CreateSwapChainForComposition);
			LOG_INFO(L"[DXGI] CreateSwapChainForComposition attached");
		}

		DetourTransactionCommit();
		factory4->Release();

		LOG_INFO(L"[DXGI] Swap chain functions hooked successfully");
	}

#pragma region DXGI methods

	HRESULT WINAPI CreateDXGIFactory(REFIID riid, _COM_Outptr_ void** ppFactory)
	{
		LOG_TRACE(L"[DXGI] CreateDXGIFactory: called");
		IDXGIFactory* factory;
		HRESULT result = orgCreateDXGIFactory(riid, (void**)&factory);

		if (SUCCEEDED(result) && factory) {
			AttachToFactory(factory);
			EnsureSwapChainDetours(factory);
			*ppFactory = factory;
		}

		return result;
	}

	HRESULT WINAPI CreateDXGIFactory1(REFIID riid, _COM_Outptr_ void** ppFactory)
	{
		LOG_TRACE(L"[DXGI] CreateDXGIFactory1: called");

		IDXGIFactory1* factory;
		HRESULT result = orgCreateDXGIFactory1(riid, (void**)&factory);

		if (SUCCEEDED(result) && factory) {
			AttachToFactory(factory);
			EnsureSwapChainDetours(factory);
			*ppFactory = factory;
		}

		return result;
	}

	HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, _COM_Outptr_ void** ppFactory)
	{
		LOG_TRACE(L"[DXGI] CreateDXGIFactory2: called");

		IDXGIFactory* factory;
		HRESULT result = orgCreateDXGIFactory2(Flags, riid, (void**)&factory);

		if (SUCCEEDED(result) && factory) {
			AttachToFactory(factory);
			EnsureSwapChainDetours(factory);
			*ppFactory = factory;
		}

		return result;
	}

#pragma endregion

#pragma region DXGI Attach methods

	static void AttachToAdapter(IUnknown* unkAdapter)
	{
		if (!unkAdapter) {
			return;
		}

		// Teardown in progress / subsystem disabled - do not install new hooks.
		if (!g_dxgiHooksActive.load(std::memory_order_acquire)) {
			return;
		}

		if (orgGetDesc && orgGetDesc1 && orgGetDesc2 && orgGetDesc3) {
			return;
		}

		std::lock_guard<std::mutex> lock(dxgiAdapterHookMutex);

		// Re-check after acquiring the lock.
		if (!g_dxgiHooksActive.load(std::memory_order_acquire)) {
			return;
		}
		if (orgGetDesc && orgGetDesc1 && orgGetDesc2 && orgGetDesc3) {
			return;
		}

		IDXGIAdapter* adapter = nullptr;
		IDXGIAdapter1* adapter1 = nullptr;
		IDXGIAdapter2* adapter2 = nullptr;
		IDXGIAdapter4* adapter4 = nullptr;

		unkAdapter->QueryInterface(__uuidof(IDXGIAdapter), (void**)&adapter);
		unkAdapter->QueryInterface(__uuidof(IDXGIAdapter1), (void**)&adapter1);
		unkAdapter->QueryInterface(__uuidof(IDXGIAdapter2), (void**)&adapter2);
		unkAdapter->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&adapter4);

		if (!adapter && !adapter1 && !adapter2 && !adapter4) {
			return;
		}

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		if (adapter && !orgGetDesc) {
			void** vtbl = *reinterpret_cast<void***>(adapter);
			orgGetDesc = (PFN_GetDesc)vtbl[8];
			DetourAttach(&(PVOID&)orgGetDesc, GetDesc);
			LOG_INFO(L"[DXGI] IDXGIAdapter::GetDesc attached");
		}

		if (adapter1 && !orgGetDesc1) {
			void** vtbl = *reinterpret_cast<void***>(adapter1);
			orgGetDesc1 = (PFN_GetDesc1)vtbl[10];
			DetourAttach(&(PVOID&)orgGetDesc1, GetDesc1);
			LOG_INFO(L"[DXGI] IDXGIAdapter1::GetDesc1 attached");
		}

		if (adapter2 && !orgGetDesc2) {
			void** vtbl = *reinterpret_cast<void***>(adapter2);
			orgGetDesc2 = (PFN_GetDesc2)vtbl[11];
			DetourAttach(&(PVOID&)orgGetDesc2, GetDesc2);
			LOG_INFO(L"[DXGI] IDXGIAdapter2::GetDesc2 attached");
		}

		if (adapter4 && !orgGetDesc3) {
			void** vtbl = *reinterpret_cast<void***>(adapter4);
			orgGetDesc3 = (PFN_GetDesc3)vtbl[18]; // index zweryfikuj u siebie
			DetourAttach(&(PVOID&)orgGetDesc3, GetDesc3);
			LOG_INFO(L"[DXGI] IDXGIAdapter4::GetDesc3 attached");
		}

		DetourTransactionCommitEx(nullptr);

		if (adapter)  adapter->Release();
		if (adapter1) adapter1->Release();
		if (adapter2) adapter2->Release();
		if (adapter4) adapter4->Release();
	}

	static void AttachToFactory(IUnknown* unkFactory)
	{
		// Teardown in progress / subsystem disabled - do not install new hooks.
		if (!g_dxgiHooksActive.load(std::memory_order_acquire)) {
			return;
		}

		if (orgEnumAdapters &&
			orgEnumAdapters1 &&
			orgEnumAdapterByLuid &&
			orgEnumAdapterByGpuPreference) {
			return;
		}

		std::lock_guard<std::mutex> lock(dxgiFactoryMutex);

		// Re-check after acquiring the lock.
		if (!g_dxgiHooksActive.load(std::memory_order_acquire)) {
			return;
		}
		if (orgEnumAdapters &&
			orgEnumAdapters1 &&
			orgEnumAdapterByLuid &&
			orgEnumAdapterByGpuPreference) {
			return;
		}

		IDXGIFactory* factory = nullptr;
		IDXGIFactory1* factory1 = nullptr;
		IDXGIFactory4* factory4 = nullptr;
		IDXGIFactory6* factory6 = nullptr;

		unkFactory->QueryInterface(__uuidof(IDXGIFactory), (void**)&factory);
		unkFactory->QueryInterface(__uuidof(IDXGIFactory1), (void**)&factory1);
		unkFactory->QueryInterface(__uuidof(IDXGIFactory4), (void**)&factory4);
		unkFactory->QueryInterface(__uuidof(IDXGIFactory6), (void**)&factory6);

		if (!factory && !factory1 && !factory4 && !factory6) {
			return;
		}

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());

		// IDXGIFactory vtable
		if (factory && !orgEnumAdapters) {
			void** vtbl = *reinterpret_cast<void***>(factory);
			orgEnumAdapters = (PFN_EnumAdapters)vtbl[7];
			DetourAttach(&(PVOID&)orgEnumAdapters, EnumAdapters);
			LOG_INFO(L"[DXGI] EnumAdapters attached");
		}

		// IDXGIFactory1 vtable
		if (factory1 && !orgEnumAdapters1) {
			void** vtbl = *reinterpret_cast<void***>(factory1);
			orgEnumAdapters1 = (PFN_EnumAdapters1)vtbl[12];
			DetourAttach(&(PVOID&)orgEnumAdapters1, EnumAdapters1);
			LOG_INFO(L"[DXGI] EnumAdapters1 attached");
		}

		// IDXGIFactory4 vtable
		if (factory4 && !orgEnumAdapterByLuid) {
			void** vtbl = *reinterpret_cast<void***>(factory4);
			orgEnumAdapterByLuid =
				(PFN_EnumAdapterByLuid)vtbl[26];
			DetourAttach(&(PVOID&)orgEnumAdapterByLuid, EnumAdapterByLuid);
			LOG_INFO(L"[DXGI] EnumAdapterByLuid attached");
		}

		// IDXGIFactory6 vtable
		if (factory6 && !orgEnumAdapterByGpuPreference) {
			void** vtbl = *reinterpret_cast<void***>(factory6);
			orgEnumAdapterByGpuPreference =
				(PFN_EnumAdapterByGpuPreference)vtbl[29];
			DetourAttach(&(PVOID&)orgEnumAdapterByGpuPreference,
				EnumAdapterByGpuPreference);
			LOG_INFO(L"[DXGI] EnumAdapterByGpuPreference attached");
		}

		DetourTransactionCommit();

		if (factory)  factory->Release();
		if (factory1) factory1->Release();
		if (factory4) factory4->Release();
		if (factory6) factory6->Release();
	}
}