#define DLSS_HMODULE 0x2222
#define NVNGX_HMODULE 0x1234

#include "Kernel32Proxy.h"
#include "Common.h"
#include "NvngxProxy.h"
#include "Autoconfig.h"
#include "../Core/Context.h"
#include "../Utils/Preloader.h"
#include <windows.h>
#include <string>
#include "HookManager.h"
#include "DetourApiDetours.h"
#include "ProcAliasRegistry.h"
#include "Nvapi64Dispatch.h"
#include <stdio.h>
#include <intrin.h>

#pragma intrinsic(_ReturnAddress)

static DetourApiDetours detourApi;
HookManager* hookManager;

void DetachDetours()
{
	return;
}

static void TryInstallHooksOnDemand() {
	if (hookManager)
		hookManager->TryInstallOnDemand();
}

void InitializeDetours()
{
	Common::SetProcAddress(OriginalGetProcAddress);

	static HookManager mgr(ctx, detourApi);
	hookManager = &mgr;
	mgr.InitializeAll();
	mgr.TryInstallOnDemand();
	SetOriginalGetProcAddress(OriginalGetProcAddress);
}

FARPROC WINAPI _DetourGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	std::wstring modName = Common::GetModuleFilePath(hModule).wstring();

	auto alias = ProcAliasRegistry::Instance().TryResolve(hModule, lpProcName);
	if (alias) {
		//LOG_DEBUG(L"@>> " + ToWideString(std::string(lpProcName)) + L" :: " + modName);
		return reinterpret_cast<FARPROC>(alias);
	}
	
	
	if (hModule == (HMODULE) 0x1234) {
		//LOG_DEBUG(L"!>> " + ToWideString(std::string(lpProcName)) + L" :: " + modName);
		return _OriginalGetProcAddress(GetModuleHandle(L"_nvngx.dll"), lpProcName);
	}

	return _OriginalGetProcAddress(hModule, lpProcName);
}

FARPROC WINAPI DetourGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	std::wstring modName = Common::GetModuleFilePath(hModule).wstring();
#ifdef PROCLOAD_DEBUG

	if (lpProcName) {
		LOG_DEBUG(L">>> " + ToWideString(std::string(lpProcName)) + L" :: " + modName);
	}

#endif

	auto alias = ProcAliasRegistry::Instance().TryResolve(hModule, lpProcName);
	if (alias) {
		//LOG_DEBUG(L">>> " + ToWideString(std::string(lpProcName)) + L" :: " + modName);
		return reinterpret_cast<FARPROC>(alias);
	}

	if (hModule == (HMODULE)NVNGX_HMODULE) {
		//LOG_DEBUG(L"@>> " + ToWideString(std::string(lpProcName)) + L" :: " + modName);
		auto realNgxModule = Autoconfig::GetNGXLibrary();
		if (realNgxModule) {
			//LOG_INFO(L"[INIT] Returning original NGX function pointer: " + std::wstring(funcName.begin(), funcName.end()));
			return OriginalGetProcAddress(realNgxModule, lpProcName);
		}
		else {
			auto alias = ProcAliasRegistry::Instance().TryResolve(hModule, lpProcName);
			if (alias) {
				//LOG_DEBUG(L"!>> " + ToWideString(std::string(lpProcName)) + L" :: " + modName);
				return reinterpret_cast<FARPROC>(alias);
			}

			LOG_ERROR(L"[INIT] Unable to return original NGX function pointer: (reason: missing genuine nvngx.dll file)");
			return nullptr;
		}
	}

	static bool done = false;
	FARPROC result;

	if (endsWith(modName, L"nvapi64.dll") && endsWith(ToWideString(std::string(lpProcName)), L"NvAPI_QueryInterface")) {
		return reinterpret_cast<FARPROC>(&NVAPI::NvAPI_QueryInterface);
	}
	if (endsWith(modName, L"_nvngx.dll") || (endsWith(modName, L"nvngx.dll") && modName.find(L"system32") == std::wstring::npos)) {
		//LOG_ERROR(L"[INIT] Detouring NGX in " + modName);
		//result = DetourNgx(hModule, lpProcName);
		result = GetProcAddress(Common::GetModuleHandle(), lpProcName);
		return result;
	}

	if (endsWith(modName, L"nvngx_dlssg.dll")) {
		LOG_WARNING(L"[INIT] Detected DLSSG function: " + ToWideString(std::string(lpProcName)));
		//result = DetourNgx(hModule, lpProcName);
		//result = GetProcAddress(Common::GetModuleHandle(), lpProcName);
		//return result;
	}

	result = OriginalGetProcAddress(hModule, lpProcName);

	return result;
}
 
HMODULE WINAPI DetourLoadLibraryW(LPCWSTR lpLibFileName)
{
	return DetourLoadLibraryExW(lpLibFileName, NULL, 0);
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

BOOL WINAPI DetouredFreeLibrary(HMODULE hLibModule)
{
	if (hLibModule == Common::GetModuleHandle()) {
		//LOG_WARNING(L"Unloading of the main DLSS Enabler module prevented");
		return TRUE;
	}

	auto libName = Common::GetModuleFilePath(hLibModule).wstring();

	if (endsWith(libName, L"nvngx_dlssg.dll")) {
		//return true;
		LOG_WARNING(L"[DLSSG] NVNGX FILE UNLOADED: " + WhoIsTheCaller(_ReturnAddress()));
	}

	if (endsWith(libName, L"nvapi64.dll")) {
		//FakeNvapi_Shutdown();
	}

	if (hLibModule == (HMODULE)DLSS_HMODULE
		|| endsWith(libName, L"nvngx-wrapper.dll")
		|| endsWith(libName, L"nvapi64.dll")
		|| endsWith(libName, L"dlss-enabler-upscaler.dll")
		|| endsWith(libName, L"dlssg_to_fsr3_amd_is_better.dll")
		|| endsWith(libName, L"dlssg_to_fsr3_amd_is_better-3.0.dll")
		//|| endsWith(libName, L"sl.interposer.dll")
		//|| endsWith(libName, L"libxess.dll")
		|| endsWith(libName, L"dxgi.dll")
		) {
		//LOG_WARNING(L"Unloading of critical DLSS Enabler modules prevented: " + libName);
		return TRUE;
	}

	// Call the original FreeLibrary function
	return pOriginalFreeLibrary(hLibModule);
}

static HMODULE WINAPI WrappedLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	auto result = OriginalLoadLibraryExW(lpLibFileName, hFile, dwFlags);
	TryInstallHooksOnDemand();

	return result;
}

#include "Console.h"
#include "Optiscaler.h"

HMODULE WINAPI _DetourLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	//LOG_WARNING(L"[LOADER] Loading " + std::wstring(lpLibFileName)); 
	//LOG_ERROR(L"[DLSSG] FILE LOADED: " + std::wstring(lpLibFileName));
	if (endsWith(lpLibFileName, L"dlssg_to_fsr3_amd_is_better.dll")) {
		LOG_INFO(L"[LOADER] DLSSG TO FSR3 file requested");
	}

	if (endsWith(lpLibFileName, L"nvngx_dlssg.dll")) {
		LOG_DEBUG(L"[LOADER] NVNGX_DLSSG file requested for " + WhoIsTheCaller(_ReturnAddress()));
		if (std::wstring(lpLibFileName).find(L"system32")) {
			LOG_DEBUG(L"[NVNGX] NVNGX_DLSSG requested from System32 directory");
		}
	}

	//if (endsWith(lpLibFileName, L"dlss-enabler-ngx.dll") && !ctx.ngx.isRealNgxPresent) {
	if (endsWith(lpLibFileName, L"dlss-enabler-ngx.dll")) {
		return (HMODULE)0x1234;
	}

	auto result = _OriginalLoadLibraryExW(lpLibFileName, hFile, dwFlags);
	return result;
}

HMODULE WINAPI _DetourLoadLibraryW(LPCWSTR lpLibFileName)
{
	if (endsWith(lpLibFileName, L"dlss-enabler-ngx.dll")) {
		LOG_INFO(L"[LOADER] Providing NVNGX file to Optiscaler");
		return (HMODULE)0x1234;
	}

	return _OriginalLoadLibraryW(lpLibFileName);
}

HMODULE WINAPI DetourLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE result = nullptr;

	//LOG_WARNING(L"[LOADER] Loading " + std::wstring(lpLibFileName));
	
	bool shouldExit = false;

	Common::SetLoader(OriginalLoadLibraryW);
	result = Preloader::OnLibraryLoad(lpLibFileName, shouldExit);
	if (shouldExit) {
		return result;
	}

	if (endsWith(lpLibFileName, L"dlss-enabler-ngx.dll") && !ctx.ngx.isRealNgxPresent) {
		LOG_INFO(L"[LOADER] Providing NVNGX file to Optiscaler");
		return (HMODULE)NVNGX_HMODULE;
	}

	if (endsWith(lpLibFileName, L"nvngx.dll")) {
		if (!endsWith(lpLibFileName, L"_nvngx.dll")) {
			result = (HMODULE)NVNGX_HMODULE;
		}

		LOG_INFO(L"[LOADER] Loading " + std::wstring(lpLibFileName));
		OptiScalerConfig cfg{};

		static bool isOptiActive = false;

		if (!isOptiActive) {
			cfg.spoof_as_enabler = false;
			if (ctx.ngx.isEmbeddedNgxUsed) {
				if (!OptiScaler_Init(Common::GetModuleHandle(), &cfg)) {}
				ctx.isOptiscalerInitialized = true;
				Console::ResetLogging();
			}
			isOptiActive = true;
		}
		if (!ctx.ngx.isRealNgxPresent) {
			return WrappedLoadLibraryExW(L"nvngx.dll", hFile, dwFlags);
		}

		if (ctx.ngx.isRealNgxHidden) {
			return nullptr;
		}
	}

	if (endsWith(lpLibFileName, L"dlssg_to_fsr3_amd_is_better.dll")) {
		if (ctx.fsr3fgVersion == 0) {
			result = Common::LoadPlugin(L"dlssg_to_fsr3_amd_is_better-3.0.dll");
		}
		else {
			result = Common::LoadPlugin(L"dlssg_to_fsr3_amd_is_better.dll");
		}
		
		if (result == nullptr && ctx.ngx.isEmbeddedDlssgUsed) {
			return Common::GetModuleHandle();
		}
		
		return result;
	}

	std::wstring proxyDllPath = L"nvapi64.dll";

	if (!endsWith(lpLibFileName, proxyDllPath)) {
		return WrappedLoadLibraryExW(lpLibFileName, hFile, dwFlags);
	}

	if (!ctx.nvapi.isEmbeddedNvapiUsed) {
		if (ctx.nvapi.isProxyEnabled && Common::IsPluginPresent(proxyDllPath)) {
			result = Common::LoadPlugin(proxyDllPath);

			if (result) {
				ctx.nvapi.isProxyLoaded = true;
				auto version = Common::GetFileVersion(proxyDllPath.c_str());
				LOG_INFO(L"[LOADER] Loaded NVAPI from local directory (version: " + (version != L"" ? version : L"unknown") + L")");
			}
		}
		else {
			result = OriginalLoadLibraryExW(lpLibFileName, hFile, dwFlags);
			if (result) {
				ctx.nvapi.isGenuineFileLoaded = true;
				LOG_INFO(L"[LOADER] Loaded NVAPI file");
			}
		}
	}

	ctx.nvapi.isProxyLoaded = true;
	TryInstallHooksOnDemand();
	return Common::GetModuleHandle();
}