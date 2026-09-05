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
#include "Console.h"
#include "Optiscaler.h"
#include "../Utils/Validator.h"

#pragma intrinsic(_ReturnAddress)

static DetourApiDetours detourApi;
HookManager* hookManager;

void DetachDetours()
{
	if (hookManager)
		hookManager->UninstallAll();
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

static HMODULE WINAPI WrappedLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	auto result = OriginalLoadLibraryExW(lpLibFileName, hFile, dwFlags);
	TryInstallHooksOnDemand();

	return result;
}


FARPROC WINAPI _DetourGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	std::wstring modName = Common::GetModuleFilePath(hModule).wstring();
//	if (IS_INTRESOURCE(lpProcName))
//		LOG_DEBUG(L"[_DetourGetProcAddress] Module: " + modName + L" | Ordinal: " + std::to_wstring(reinterpret_cast<uintptr_t>(lpProcName)));
//	else
//		LOG_DEBUG(L"[_DetourGetProcAddress] Module: " + modName + L" | Function: " + ToWideString(std::string(lpProcName)));

	//std::wstring modName = Common::GetModuleFilePath(hModule).wstring();

	if (modName == L"amdxc64.dll") {
		//LOG_WARNING(L"[LOADER] Blocking " + modName + L":" + ToWideString(std::string(lpProcName)) + L" due to NVIDIA GPU driver presence");
		//return nullptr;
	}

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
	//if (IS_INTRESOURCE(lpProcName))
	//	LOG_DEBUG(L"[_DetourGetProcAddress] Module: " + modName + L" | Ordinal: " + std::to_wstring(reinterpret_cast<uintptr_t>(lpProcName)));
	//else
	//	LOG_DEBUG(L"[_DetourGetProcAddress] Module: " + modName + L" | Function: " + ToWideString(std::string(lpProcName)));


	//std::wstring modName = Common::GetModuleFilePath(hModule).wstring();
#ifdef PROCLOAD_DEBUG

	if (lpProcName) {
		LOG_DEBUG(L">>> " + ToWideString(std::string(lpProcName)) + L" :: " + modName);
	}

#endif
	if (modName == L"amdxc64.dll") {
		//LOG_WARNING(L"[LOADER] Blocking " + modName + L":" + ToWideString(std::string(lpProcName)) + L" due to NVIDIA GPU driver presence");
		//return nullptr;
	}

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

		if (ToWideString(std::string(lpProcName)) == L"NVSDK_NGX_D3D12_CreateFeature1") {
			LOG_ERROR(L"[INIT] Disabling DLSSG function: " + ToWideString(std::string(lpProcName)));
			return nullptr;
		}

		if (ToWideString(std::string(lpProcName)) == L"NVSDK_NGX_D3D12_CreateFeature2") {
			LOG_ERROR(L"[INIT] Disabling DLSSG function: " + ToWideString(std::string(lpProcName)));
			return nullptr;
		}

		if (ToWideString(std::string(lpProcName)) == L"NVSDK_NGX_D3D12_Init_Ext1") {
			LOG_ERROR(L"[INIT] Disabling DLSSG function: " + ToWideString(std::string(lpProcName)));
			return nullptr;
		}

		if (ToWideString(std::string(lpProcName)) == L"NVSDK_NGX_D3D12_Init_Ext2") {
			LOG_ERROR(L"[INIT] Disabling DLSSG function: " + ToWideString(std::string(lpProcName)));
			return nullptr;
		}
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

bool CheckStreamlineOverride(LPCWSTR lpLibFileName, std::wstring& outPath)
{
	if (!lpLibFileName)
		return false;

	std::filesystem::path filePath(lpLibFileName);
	std::wstring filename = filePath.filename().wstring();
	std::wstring normalizedPath = std::filesystem::path(lpLibFileName).lexically_normal().wstring();

	// first try capturing overrides
	// C:\ProgramData/NVIDIA/NGX/models/sl_dlss_0/versions/133120/files/190_E658703.dll
	if (normalizedPath.find(L"\\NGX\\models\\sl_", 0) != wstring::npos) {
		auto pos1 = normalizedPath.find(L"\\NGX\\models\\sl_", 0);
		if (pos1 != std::wstring::npos) {
			auto pos2 = normalizedPath.find(L"\\versions\\", pos1);
			if (pos2 != std::wstring::npos) {
				auto pos3 = normalizedPath.rfind(L"_", pos2);
				if (pos3 != std::wstring::npos) {
					filename = L"sl." + normalizedPath.substr(pos1 + 15, pos3 - pos1 - 15) + L".dll";
					LOG_INFO(L"[LOADER] Detected Streamline OTA plugin " + normalizedPath + L" for " + filename);

					auto pluginName = filePath.filename().wstring();
					if (filename == L"sl.common.dll") {
						ctx.streamline.commonName = pluginName;
					}

					if (filename == L"sl.interposer.dll") {
						ctx.streamline.interposerName = pluginName;
					}

					if (filename == L"sl.pcl.dll") {
						ctx.streamline.pclName = pluginName;
					}

					if (filename == L"sl.reflex.dll") {
						ctx.streamline.reflexName = pluginName;
					}

					if (filename == L"sl.dlss.dll") {
						ctx.streamline.dlssName = pluginName;
					}

					if (filename == L"sl.dlss_g.dll") {
						ctx.streamline.dlssgName = pluginName;
					}

					if (filename == L"sl.dlss_d.dll") {
						ctx.streamline.dlssdName = pluginName;
					}

					if (filename == L"sl.deepdvc.dll") {
						ctx.streamline.deepDvcName = pluginName;
					}
				}
			}
		}
	}


	if (filename.size() < 7) // minimum "sl.X.dll"
		return false;

	if (filename.substr(0, 3) != L"sl." ||
		filename.substr(filename.size() - 4) != L".dll")
		return false;

	if (GetModuleHandleW(filename.c_str())) {
		LOG_WARNING(L"[LOADER] Cannot substitute Streamline file: " + filename + L" (file already loaded)");
		return false;
	}

	std::filesystem::path localPath = Common::GetModuleDirectory() + L"plugins\\";
	localPath /= filename;

	if (std::filesystem::exists(localPath)) {
		outPath = localPath.wstring();
		return true;
	}
	//LOG_WARNING(L"[LOADER] Cannot substitute Streamline file: " + filename + L" (file not found)");

	return false;
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

	std::wstring overridePath;
	if (CheckStreamlineOverride(lpLibFileName, overridePath)) {
		LOG_INFO(L"[LOADER] Substituting Streamline file: " + (overridePath));
		return WrappedLoadLibraryExW(overridePath.c_str(), hFile, dwFlags);
	}

	if (endsWith(lpLibFileName, L"dlss-enabler-ngx.dll") && !ctx.ngx.isRealNgxPresent) {
		LOG_INFO(L"[LOADER] Providing NVNGX file to Optiscaler");
		return (HMODULE)NVNGX_HMODULE;
	}

	if (endsWith(lpLibFileName, L"nvngx.dll") || endsWith(lpLibFileName, L"nvngx_dlssg.dll")) {
		static bool isOptiActive = false;
		if (!isOptiActive) {
			OptiScalerConfig cfg{};
			cfg.spoof_as_enabler = false;
			//cfg.isFrs4UpdateOff = Validator::Is_NVNGXDLLPresent() == 1;
			//LOG_INFO(L"[LOADER] Disabling FSR4 update check!");
			HMODULE opti = nullptr;
			if (!GetModuleHandleW(L"dlss-enabler-upscaler.dll") && Common::IsPluginPresent(L"dlss-enabler-upscaler.dll")) {
				opti = Common::LoadPlugin(L"dlss-enabler-upscaler.dll");
			}

			if (opti != nullptr) {
				// @todo: fixme!
				ctx.ngx.isEmbeddedNgxUsed = false;
				ctx.isOptiscalerInitialized = true;
			}
			else if (ctx.ngx.isEmbeddedNgxUsed && !ctx.isOptiscalerInitialized) {
				LOG_INFO(L"[LOADER] Initializing Optiscaler");
				if (!OptiScaler_Init(Common::GetModuleHandle(), &cfg)) {}
				LOG_INFO(L"[LOADER] Initializing Optiscaler: Successful");
				ctx.isOptiscalerInitialized = true;
				Console::ResetLogging();
			}

			isOptiActive = true;
		}
	}
	if (endsWith(lpLibFileName, L"nvngx.dll")) {
		if (!endsWith(lpLibFileName, L"_nvngx.dll")) {
			result = (HMODULE)NVNGX_HMODULE;
		}

		LOG_INFO(L"[LOADER] Loading " + std::wstring(lpLibFileName));


		
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