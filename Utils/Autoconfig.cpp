#include "Common.h"
#include "Autoconfig.h"
#include <string>
#include "Console.h"
#include "../Core/Context.h"
#include "Validator.h"
#define RTX_4090_FULL_NAME "NVIDIA GeForce RTX 4090 Ti"

static bool IsRunningUnderWindows()
{
	static bool isWindows = false;
	static bool isOsReported = false;
	if (isOsReported) {
		return isWindows;
	}
	isOsReported = true;
	static const char* (CDECL * wineGetVersion)(void);
	static void (CDECL * wineGetHostVersion)(const char** sysname, const char** release);

	HMODULE hntdll = GetModuleHandle(L"ntdll.dll");
	if (!hntdll) {
		LOG_WARNING(L"[INIT] Running under unknown OS, some features might be broken");
		isWindows = false;
		return isWindows;
	}

	wineGetVersion = (const char* (CDECL*)(void))GetProcAddress(hntdll, "wine_get_version");
	wineGetHostVersion = (void (CDECL*)(const char**, const char**))GetProcAddress(hntdll, "wine_get_host_version");

	if (wineGetVersion) {
		isWindows = false;
		const char* sysname;
		const char* version;
		if (!wineGetHostVersion) {
			LOG_WARNING(L"[INIT] Unknown Wine version detected");
			LOG_WARNING(L"[INIT] *** Wine support is experimental, some features might be broken");
			return isWindows;
		}

		wineGetHostVersion(&sysname, &version);
		std::string sysVersion = std::string(wineGetVersion());
		std::string sysName = std::string(sysname);
		std::string ver = std::string(version);
		LOG_INFO(L"[INIT] Running Wine " + ToWideString(sysVersion)
			+ L" under " + ToWideString(sysName)
			+ L" " + ToWideString(ver)
		);

		if (sysVersion[0] == L'9') {
			LOG_WARNING(L"[INIT] *** Wine 9 support is experimental, some features might be broken");
		}
		else if (sysVersion[0] == L'8') {
			LOG_WARNING(L"[INIT] *** Wine 8 support is very limited, some features might be broken, consider upgrading to Wine 9 or better");
		}

		return isWindows;
	}

	typedef NTSTATUS(WINAPI* RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);

	RtlGetVersionFunc func = (RtlGetVersionFunc)GetProcAddress(hntdll, "RtlGetVersion");
	if (func != nullptr) {
		RTL_OSVERSIONINFOW rovi = { 0 };
		rovi.dwOSVersionInfoSize = sizeof(rovi);
		if (func(&rovi) == 0) {
			LOG_INFO(L"[INIT] Running under Windows " + std::to_wstring(rovi.dwMajorVersion) + L"." + std::to_wstring(rovi.dwMinorVersion) + L"." + std::to_wstring(rovi.dwBuildNumber));
			isWindows = true;
			return isWindows;
		}
	}

	LOG_INFO(L"[INIT] Running under Windows");
	isWindows = true;
	return isWindows;
}

bool Autoconfig::Initialize()
{
	auto filePath = Common::GetProcessFilePath();
	std::wstring processName = filePath.filename().wstring();
	ctx.isRunningUnderWindows = IsRunningUnderWindows();
	ctx.ngx.upscalingMethod = UPSCALING_METHOD_AUTO;
	ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_AUTO;
	ctx.ngx.overrideDlssUpscalerCapability = true;
	ctx.ngx.enableDlssUpscaler = true;

	//ctx.logging.isReflexDebugEnabled = true;
	ctx.ngx.isEmbeddedDlssgUsed = true;
	ctx.nvapi.isMockEnabled = false;
	ctx.nvapi.isProxyEnabled = true;
	ctx.nvapi.isEmbeddedNvapiUsed = Validator::Is_NVNGXDLLPresent() != 1;
	ctx.ngx.isRealNgxPresent = Validator::Is_NVNGXDLLPresent() == 1;
	//ctx.nvapi.isEmbeddedNvapiUsed = true;
	ctx.emulation.isHagsSpoofed = true;
	ctx.gpu.isHagsEnabled = true;
	ctx.ngx.isProxyEnabled = true;
	ctx.logging.isDebugEnabled = false;
	ctx.logging.isConsoleEnabled = false;

	Autoconfig::CheckIniFile();
	Autoconfig::CheckCommandLineParams();

	if (ctx.logging.isConsoleEnabled) {
		Console::Attach();
		Console::FlushBuffer();
	}

	if (GetModuleHandleW(L"vulkan-1.dll")) {
		LOG_INFO(L"[INIT] Vulkan based application detected!");
	}

	std::wstring upscalerFile = L"dlss-enabler-upscaler.dll";
	std::wstring upscalerVersion = L"";
	 
	if (Common::IsPluginPresent(upscalerFile) && !ctx.ngx.isEmbeddedNgxUsed) {
		upscalerVersion = Common::GetPluginVersion(upscalerFile.c_str());
		// this is one of the newer optiscalers (older ones did not have the product version)
		if (upscalerVersion != L"" && upscalerVersion.compare(0, 4, L"0.4.") != 0) {
			ctx.ngx.isProxyEnabled = false;
		}
	}
	else {
		ctx.ngx.isProxyEnabled = false;
	}

	if (processName == L"ROTTR.exe" || processName == L"NMS.exe") { // Rise of the Tomb Rider and No Man's Sky (experimental branch)
		LOG_WARNING(L"[INIT] Spoofing \"RTX 4090 TI\" GPU name as the game unlocks DLSS/DLSSG based on the graphic card name");
		ctx.gpu.isFakeRtxNameRequired = true; // this unlocks DLSS (detection is based on GPU Name)
	} 

	if (
		processName == L"RiftApart.exe"
		||
		processName == L"HorizonForbiddenWest.exe"
		||
		processName == L"GhostOfTsushima.exe"
		||
		processName == L"MilesMorales.exe"
		||
		processName == L"Spider-Man.exe"		
		||
		processName == L"SOTTR.exe"
		) {
		ctx.enableRegistrySpoofing = true;
		LOG_WARNING(L"[INIT] Enabling hardware spoofing on Windows Registry level");
	}

	if (processName == L"HorizonForbiddenWest.exe") {
		LOG_WARNING(L"[INIT] Enabling Reflex emulation to address game microstuttering");
		ctx.reflex.isEmulationEnabled = true;
	}

	return true;
}

bool Autoconfig::InitializeFrameGeneration()
{
	std::wstring wrapperFile;
	if (ctx.ngx.isDlssgEnabled) {
		LOG_INFO(L"[DLSSG] Loading frame generation backend: DLSSG");
		LOG_INFO(L"[DLSSG] Frame generation backend loaded successfully");
		return true;
	}

	auto filePath = Common::GetProcessFilePath();
	std::wstring processName = filePath.filename().wstring();

	if (processName == L"BrightMemoryInfinite-Win64-Shipping.exe" && (ctx.ngx.configuredFrameGenerationMethod == FRAMEGENERATION_METHOD_FSR3 || ctx.ngx.configuredFrameGenerationMethod == FRAMEGENERATION_METHOD_AUTO)) {
		LOG_WARNING(L"[DLSSG] Downgrading Frame Generation from FSR 3.1 to FSR 3.0 to fix in-game visual glitches");
		ctx.fsr3fgVersion = 0;
	}
	
	if (!ctx.ngx.isEmbeddedDlssgUsed) {
		LOG_INFO(L"[DLSSG] Loading frame generation backend: FSR 3." + std::to_wstring(ctx.fsr3fgVersion));

		// some files trigger proxy error 5 in Nukem9 mod dll....
		if (processName != L"ForzaHorizon5.exe") {
			wrapperFile = L"nvngx-wrapper.dll";
			HMODULE hModule33 = Common::LoadPlugin(wrapperFile);
		}

		if (ctx.fsr3fgVersion == 0) {
			wrapperFile = L"dlssg_to_fsr3_amd_is_better-3.0.dll";
		}
		else {
			wrapperFile = L"dlssg_to_fsr3_amd_is_better.dll";
		}
		if (!Common::IsPluginPresent(wrapperFile)) {
			LOG_ERROR(L"[DLSSG] Software backends not detected, DLSS upscaler might be unavailable if not supported by the hardware!");
			std::wstring errorMsg = L"Frame generation backend not detected: " + wrapperFile + L" file is missing";
			LOG_ERROR(L"[DLSSG] " + errorMsg);
			if (ctx.isValidationOn) {
				Common::Error(errorMsg, true);
			}

			return false;
		}

		HMODULE hModule1 = Common::LoadPlugin(wrapperFile);
		if (!hModule1) {
			DWORD lastError = GetLastError();
			std::wstring errorMsg = L"Frame generation backend failed to load: " + wrapperFile + L" file is corrupted (code: " + std::to_wstring(lastError) + L")";
			LOG_ERROR(L"[DLSSG] " + errorMsg);
			if (ctx.isValidationOn) {
				Common::Error(errorMsg, true);
			}

			return false;
		}
	}
	else {
//		wrapperFile = L"nvngx-wrapper.dll";
//		HMODULE hModule33 = Common::LoadPlugin(wrapperFile);
		LOG_INFO(L"[DLSSG] Loading built-in frame generation backend: FSR 3.1B");
	}
	
	LOG_INFO(L"[DLSSG] Frame generation backend loaded successfully");

	return true;
}

bool Autoconfig::PreloadUpscaler(bool forceLoad)
{
	auto filePath = Common::GetProcessFilePath();
	std::wstring processName = filePath.filename().wstring();

	if (processName == L"DOOMEternalx64vk.exe" || processName == L"NMS.exe" || processName == L"bg3.exe" || processName == L"StreamlineSample.exe") {
		ctx.isVulkanApplication = true;
		if (ctx.ngx.configuredUpscalingMethod == UPSCALING_METHOD_AUTO) {
			//LOG_WARNING(L"[INIT] Delaying OptiScaler load due to Vulkan game detected: " + processName);
			//ctx.ngx.isRealNgxHidden = true;
		}
	}

	//if (processName == L"DOOMEternalx64vk.exe") {
	//	if (ctx.ngx.configuredUpscalingMethod == UPSCALING_METHOD_AUTO) {
	//		LOG_WARNING(L"[INIT] Delaying OptiScaler load due to Vulkan game detected: " + processName);
	//		//ctx.ngx.isRealNgxHidden = true;
	//		//ctx.isVulkanApplication = true;
	//	}
	//}

	LOG_INFO(L"[DLSS] Preloading upscaling backends");
	std::wstring upscalerFile = L"dlss-enabler-upscaler.dll";

	HMODULE upscalerModule = NULL; 
	ctx.ngx.isRealNgxHidden = false;

	if (!ctx.ngx.isEmbeddedNgxUsed) {
		upscalerModule = LoadLibraryW(L"dlss-enabler-upscaler.dll");

		if (upscalerModule != NULL) {
			LOG_INFO(L"[DLSS] Upscaler backend loaded successfully");
			return true;
		}

		DWORD lastError = GetLastError();
		LOG_ERROR(L"[DLSS] Failed to initialize " + ctx.ngx.upscalingMethod + L" upscaler (code: " + std::to_wstring(lastError) + L")");

		if (ctx.isValidationOn) {
			Common::Error(ctx.ngx.upscalingMethod + L" upscaler backend failed to load: " + upscalerFile + L" is corrupted\n\n(code: " + std::to_wstring(lastError) + L")", true);
		}
	}
	else {
		LOG_INFO(L"[DLSS] Built-in upscaler backend loaded successfully");
	}
	return true;
}

HMODULE Autoconfig::GetNGXLibrary()
{
	static HMODULE realNgxModule;

	if (realNgxModule) {
		return realNgxModule;
	}

	wchar_t filePath[MAX_PATH] = {};
	DWORD filePathSize = sizeof(filePath);

	HKEY key = nullptr;
	auto status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", 0, KEY_READ, &key);

	if (status == ERROR_SUCCESS) {
		status = RegGetValueW(key, nullptr, L"NGXPath", RRF_RT_ANY, nullptr, filePath, &filePathSize);
		RegCloseKey(key);
	}
	else {
		status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", 0, KEY_READ, &key);

		if (status == ERROR_SUCCESS) {
			status = RegGetValueW(key, nullptr, L"FullPath", RRF_RT_ANY, nullptr, filePath, &filePathSize);
			RegCloseKey(key);
		}
	}

	if (status == ERROR_SUCCESS) {
		wcscat_s(filePath, L"\\_nvngx.dll");
		LOG_INFO(L"[INIT] Loading genuine NGX file from " + std::wstring(filePath));
	}
	else {
		LOG_INFO(L"[INIT] No genuine NGX file found");
		return nullptr;
	}

	const auto moduleHandle = LoadLibraryW(filePath);
	if (moduleHandle) {
		LOG_INFO(L"[INIT] NGX file loaded successfully");
	}
	else {
		LOG_ERROR(L"[INIT] NGX file failed to load: " + std::to_wstring(GetLastError()));
	}

	realNgxModule = moduleHandle;
	return moduleHandle;
}

void Autoconfig::CheckIniFile()
{
	auto filePath = Common::GetProcessFilePath();
	std::wstring processName = filePath.filename().wstring();

	WCHAR buffer[255];
	std::wstring fileName = std::wstring(Common::GetProcessFilePath().remove_filename().wstring() + L"nvngx.ini");

	LPCWSTR lpFileName = fileName.c_str();
	if (GetFileAttributesW(lpFileName) == INVALID_FILE_ATTRIBUTES) {
		const auto path = std::filesystem::path(lpFileName);
		const auto fileName = path.filename().wstring();
		const auto parentDir = path.parent_path().parent_path().wstring();
		auto newPath = std::wstring(parentDir + L"\\" + fileName);
		lpFileName = newPath.c_str();

		if (GetFileAttributesW(lpFileName) == INVALID_FILE_ATTRIBUTES) {
			LOG_INFO(L"[INIT] Loading of the config file failed (file is missing)");
			Console::EnableLogging(true);
			ctx.logging.isExtraDebugEnabled = true;
			return;
		}
	}

	DWORD dwResult;

	// logger
	dwResult = GetPrivateProfileStringW(L"Log", L"LoggingEnabled", L"auto", buffer, 255, lpFileName);
	std::wstring value = std::wstring(buffer);

	if (value != L"false") {
		Console::EnableLogging(true);
		ctx.logging.isExtraDebugEnabled = true;
	}

	dwResult = GetPrivateProfileStringW(L"Log", L"LogLevel", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"1") {
		ctx.logging.isExtraDebugEnabled = true;
	}

	dwResult = GetPrivateProfileStringW(L"Log", L"OpenConsole", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"true") {
		ctx.logging.isConsoleEnabled = true;
	}
	else {
		ctx.logging.isConsoleEnabled = false;
	}
		
	LOG_INFO(L"[INIT] Loading of the config file: " + std::wstring(lpFileName));
	
	dwResult = GetPrivateProfileStringW(L"Spoofing", L"Vulkan", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);
	ctx.enableVulkanSpoofing = false;

	if (value == L"auto") {
		if (processName == L"NMS.exe" || processName == L"StreamlineSample.exe" || processName == L"DOOMEternalx64vk.exe" || processName == L"BG3.exe") {
			// for now only few games are officially supported under Linux 
			ctx.enableVulkanSpoofing = true;
		}
		else {
			// Linux may be unstable due to DXVK issues...
			ctx.enableVulkanSpoofing = ctx.isRunningUnderWindows;
		}
	}
	else if (value == L"true") {
		ctx.enableVulkanSpoofing = true;
	}

	dwResult = GetPrivateProfileStringW(L"Spoofing", L"DeepDVC", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);
	ctx.ngx.isDeepDvcEnabled = true;

	if (value == L"false") {
		ctx.ngx.isDeepDvcEnabled = false;
	}

	if (value == L"force") {
		ctx.streamline.forceLoadDeepDvc = true;
	}

	dwResult = GetPrivateProfileStringW(L"DeepDVC", L"Enable", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);
	ctx.streamline.forceLoadDeepDvc = false;

	if (value == L"true") {
		ctx.streamline.forceLoadDeepDvc = true;
	}

	dwResult = GetPrivateProfileStringW(L"DeepDVC", L"Intensity", L"0.5", buffer, 255, lpFileName);
	ctx.deepDVC.intensity = (float) _wtof(buffer);

	dwResult = GetPrivateProfileStringW(L"DeepDVC", L"SaturationBoost", L"0.75", buffer, 255, lpFileName);
	ctx.deepDVC.saturationBoost = (float) _wtof(buffer);

	dwResult = GetPrivateProfileStringW(L"FrameGen", L"UseFGSwapChain", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);
	ctx.isOptiscalerSwapchainHookEnabled = true;

	if (value == L"false") {
		ctx.isOptiscalerSwapchainHookEnabled = false;
	}

	dwResult = GetPrivateProfileStringW(L"Hotfix", L"EnableDirectSwapchainHooking", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);
	ctx.enableDirectSwapchainHooking = false;

	if (value == L"true") { 
		ctx.enableDirectSwapchainHooking = true;
	}

	dwResult = GetPrivateProfileStringW(L"Hotfix", L"OptiscalerSwapchainAllowedForDLSSG", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"true") {
		ctx.isOptiscalerSwapchainHookEnabled = false;
	}

	// upscaler
	dwResult = GetPrivateProfileStringW(L"Upscalers", L"Dx12Upscaler", L"auto", buffer, 255, lpFileName);

	value = std::wstring(buffer);
	if (value == L"auto") {
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_AUTO;
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_AUTO;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"dlss") {
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_DLSS;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_DLSS;
		ctx.ngx.overrideDlssUpscalerCapability = false;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"xess") {
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_XESS;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_XESS;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"fsr22") {
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_FSR22;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR22;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"fsr31") {
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_FSR31;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR31;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"fsr21" || value == L"fsr") {
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_FSR;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else {
		value = L"";
	}

	// upscaler
	dwResult = GetPrivateProfileStringW(L"Upscalers", L"VulkanUpscaler", L"auto", buffer, 255, lpFileName);

	value = std::wstring(buffer);
	if (value == L"auto") {
		ctx.ngx.configuredVkUpscalingMethod = UPSCALING_METHOD_AUTO;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"dlss") {
		ctx.ngx.configuredVkUpscalingMethod = UPSCALING_METHOD_DLSS;
		ctx.ngx.overrideDlssUpscalerCapability = false;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"xess") {
		ctx.ngx.configuredVkUpscalingMethod = UPSCALING_METHOD_XESS;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"fsr22") {
		ctx.ngx.configuredVkUpscalingMethod = UPSCALING_METHOD_FSR22;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"fsr31") {
		ctx.ngx.configuredVkUpscalingMethod = UPSCALING_METHOD_FSR31;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (value == L"fsr21" || value == L"fsr") {
		ctx.ngx.configuredVkUpscalingMethod = UPSCALING_METHOD_FSR;
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else {
		value = L"";
	}

	// frame generation
	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"TranslateDLSS", L"off", buffer, 255, lpFileName);
	value = std::wstring(buffer);
	if (value == L"on") {
		ctx.streamline.forceLoadDLSSG = true;
	}

	// frame generation
	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"Generator", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	std::wstring frameGenerator = L"";
	if (value == L"auto") {
		frameGenerator = L"auto";
		ctx.fsr3fgVersion = 1;
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_AUTO;
	}
	else if (value == L"dlssg") {
		frameGenerator = L"DLSSG";
		ctx.ngx.isDlssgEnabled = true;
		ctx.ngx.isProxyEnabled = false;
		ctx.nvapi.isProxyEnabled = true;
		ctx.nvapi.isMockEnabled = false;
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_DLSSG;
	}
	else if (value == L"fsr3") {
		frameGenerator = L"FSR 3.1";
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_FSR3;
		ctx.fsr3fgVersion = 1;
	}
	else if (value == L"fsr3" || value == L"fsr31") {
		frameGenerator = L"FSR 3.1";
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_FSR31;
		ctx.fsr3fgVersion = 1;
	}
	else if (value == L"fsr30") {
		frameGenerator = L"FSR 3.0";
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_FSR30;
		ctx.fsr3fgVersion = 0;
	}

	// frame generation fps cap
	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"FramerateLimit", L"off", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"off") {
		//ctx.reflex.isFpsLimitEnabled = false;
	}
	else if (value == L"vsync") {
		//ctx.reflex.isVsyncEnabled = true;
	}
	else {
		ctx.reflex.desiredFpsLimit = _wtoi(buffer);
	}
	ctx.reflex.realFpsLimit = (double) ctx.reflex.desiredFpsLimit;

	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"Vsync", L"off", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"on") {
		ctx.reflex.isVsyncEnabled = true;
	}

	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"FramerateLimitEnabled", L"off", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"on") {
		ctx.reflex.isFpsLimitEnabled = true;
	}
	
	// frame generation fps lower cap
	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"FrameGenerationMode", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"dynamic") {   
		ctx.ngx.isDynamicFrameGenerationEnabled = true;
	}
	else {
		ctx.ngx.isDynamicFrameGenerationEnabled = false; 
	}

	// frame generation fps lower cap
	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"DoubleBuffering", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"on") {
		ctx.reflex.isDoubleBufferingEnforced = true;
	}
	else {
		ctx.reflex.isDoubleBufferingEnforced = false;
	}

	// frame generation fps lower cap
	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"NVAPIMode", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"mock") {
		ctx.nvapi.isMockEnabled = true;
		ctx.nvapi.isProxyEnabled = false;
	}
	else if (value == L"sys") {
		ctx.nvapi.isProxyEnabled = false;
	}

	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"Reflex", L"on", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"boost") {
		ctx.reflex.isBoostEnabled = true;
		LOG_DEBUG(L"[INIT] Refex: " + value);
	}
	else if (value == L"off") {
		ctx.reflex.isEnabled = false;
		LOG_DEBUG(L"[INIT] Refex: " + value);
	}
	else if (value == L"inject") {
		ctx.reflex.isEnabled = true;
		ctx.enableReflexInjection = true;
		LOG_DEBUG(L"[INIT] Refex: " + value);
	}

	dwResult = GetPrivateProfileStringW(L"FrameGeneration", L"ReflexEmulation", L"auto", buffer, 255, lpFileName);
	value = std::wstring(buffer);

	if (value == L"noop") {
		ctx.reflex.isEmulationEnabled = true;
		ctx.nvapi.isMockEnabled = true;
	}	
	
	if (value == L"on") {
		ctx.reflex.isLocalReflexUsed = true;
		ctx.nvapi.isMockEnabled = false;
		ctx.nvapi.isProxyEnabled = true;
	}

	LOG_INFO(L"[INIT] Loading of the config file succeeded");
}

void Autoconfig::CheckCommandLineParams()
{
	if (CMD_EXISTS("--dlss-off")) {
		ctx.isDlssEnablerOn = false;
	}

	if (CMD_EXISTS("--dlss-boost=on")) {
		//ctx.directX.isTrilinearForced = true;
		ctx.directX.isBilinearForced = true;
		ctx.directX.skipTopMips = 1;
	}

	if (CMD_EXISTS("--dlss-reflex=boost")) {
		ctx.reflex.isBoostEnabled = true;
	}
	else if (CMD_EXISTS("--dlss-reflex=off")) {
		ctx.reflex.isEnabled = false;
		ctx.reflex.isBoostEnabled = false;
	}	
	
	if (CMD_EXISTS("--dlss-reflex=inject")) {
		ctx.enableReflexInjection = true;
	}

	if (CMD_EXISTS("--dlss-debug")) {
		ctx.logging.isConsoleEnabled = true;
		ctx.logging.isDebugEnabled = true;
	}

	if (CMD_EXISTS("--dlss-debug=extra")) {
		ctx.logging.isExtraDebugEnabled = true;
	}

	if (CMD_EXISTS("--dlss-debug=ultra")) {
		ctx.logging.isExtraDebugEnabled = true;
		ctx.logging.isUltraDebugEnabled = true;
	}

	if (CMD_EXISTS("--dlss-debug=nvngx")) {
		ctx.logging.isExtraDebugEnabled = true;
		ctx.logging.isUltraDebugEnabled = true;
		ctx.logging.isNvngxDebugEnabled = true;
		ctx.logging.isNvapiDebugEnabled = false;
		ctx.logging.isStreamlineDebugEnabled = false;
		ctx.logging.isDxgiDebugEnabled = false; 
	}

	if (CMD_EXISTS("--dlss-upscaler=on")) {
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
	}
	else if (CMD_EXISTS("--dlss-upscaler=off")) {
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = false;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_FSR;
	}
	else if (CMD_EXISTS("--dlss-upscaler=xess")) {
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_XESS;
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_XESS;
	}
	else if (CMD_EXISTS("--dlss-upscaler=fsr")) {
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR;
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_FSR;
	}
	else if (CMD_EXISTS("--dlss-upscaler=fsr22")) {
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR22;
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_FSR22;
	}
	else if (CMD_EXISTS("--dlss-upscaler=fsr31")) {
		ctx.ngx.overrideDlssUpscalerCapability = true;
		ctx.ngx.enableDlssUpscaler = true;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_FSR31;
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_FSR31;
	}
	else if (CMD_EXISTS("--dlss-upscaler=dlss")) {
		ctx.ngx.overrideDlssUpscalerCapability = false;
		ctx.ngx.enableDlssUpscaler = true;
		ctx.ngx.upscalingMethod = UPSCALING_METHOD_DLSS;
		ctx.ngx.configuredUpscalingMethod = UPSCALING_METHOD_DLSS;
	}

	if (CMD_EXISTS("--dlss-upscaler-quality=ultra")) {
		ctx.enableNgxNativeResolution = true;
	}

	if (CMD_EXISTS("--dlss-logging=on")) {
		Console::EnableLogging(true);
		ctx.logging.isExtraDebugEnabled = true;
	}

	if (CMD_EXISTS("--dlss-fg=dlssg")) {
		ctx.ngx.isDlssgEnabled = true;
		ctx.ngx.isProxyEnabled = false;
		ctx.nvapi.isProxyEnabled = true;
		ctx.nvapi.isMockEnabled = false;
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_DLSSG;
	}

	if (CMD_EXISTS("--dlss-fg=fsr31")) {
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_FSR31;
		ctx.fsr3fgVersion = 1;
	}
	else if (CMD_EXISTS("--dlss-fg=fsr30")) {
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_FSR30;
		ctx.fsr3fgVersion = 0;
	}
	else if(CMD_EXISTS("--dlss-fg=fsr3")) {
		ctx.ngx.configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_FSR3;
		ctx.fsr3fgVersion = 1;
	}

	if (CMD_EXISTS("--dlss-arch=turing")) {
		ctx.currentGpuArchitecture = NV_GPU_ARCHITECTURE_TU100;
		ctx.targetGpuArchitecture = NV_GPU_ARCHITECTURE_TU100;
	}
	else if (CMD_EXISTS("--dlss-arch=ampere")) {
		ctx.currentGpuArchitecture = NV_GPU_ARCHITECTURE_GA100;
		ctx.targetGpuArchitecture = NV_GPU_ARCHITECTURE_GA100;
	}
	else if (CMD_EXISTS("--dlss-arch=ada")) {
		ctx.currentGpuArchitecture = NV_GPU_ARCHITECTURE_AD100;
		ctx.targetGpuArchitecture = NV_GPU_ARCHITECTURE_AD100;
	}

	if (CMD_EXISTS("--dlss-highest-arch")) {
		ctx.currentGpuArchitecture = NV_GPU_ARCHITECTURE_AD100;
		ctx.targetGpuArchitecture = NV_GPU_ARCHITECTURE_AD100;
	}

	if (CMD_EXISTS("--dlss-hags=on")) {
		ctx.emulation.isHagsSpoofed = true;
		ctx.gpu.isHagsEnabled = true;
	}
	else if (CMD_EXISTS("--dlss-hags=off")) {
		ctx.emulation.isHagsSpoofed = true;
		ctx.gpu.isHagsEnabled = false; 
	}
	else if (CMD_EXISTS("--dlss-hags=sys")) {
		ctx.emulation.isHagsSpoofed = false;
	}

	if (CMD_EXISTS("--dlss-nvapi=sys")) {
		ctx.nvapi.isProxyEnabled = false;
	}
	else if (CMD_EXISTS("--dlss-nvapi=mock")) {
		ctx.nvapi.isMockEnabled = true;
		ctx.nvapi.isProxyEnabled = false;
	}

	if (CMD_EXISTS("--dlss-nvngx=proxy")) {
		ctx.ngx.isProxyEnabled = true;
	}

	if (CMD_EXISTS("--dlss-nvngx=sys")) {
		ctx.ngx.isProxyEnabled = false;
	}

	if (CMD_EXISTS("--dlss-skip-validation")) {
		ctx.isValidationOn = false;
	}	
	
	if (CMD_EXISTS("--dlss-fg-mode=dynamic")) {
		ctx.ngx.isDynamicFrameGenerationEnabled = true;
	}

	if (CMD_EXISTS("--dlss-nvngx=embedded")) {
		ctx.ngx.isEmbeddedNgxUsed = true;
	}

	if (CMD_EXISTS("--dlss-nvngx=native")) {
		ctx.ngx.isEmbeddedNgxUsed = false;
	}

	if (CMD_EXISTS("--dlss-dlssg=embedded")) {
		ctx.ngx.isEmbeddedDlssgUsed = true;
	}

	if (CMD_EXISTS("--dlss-dlssg=native")) {
		ctx.ngx.isEmbeddedDlssgUsed = false;
	}

	if (CMD_EXISTS("--dlss-xell=off")) {
		ctx.nvapi.isXellEnabled = false;
	}

	if (CMD_EXISTS("--dlss-xell=on")) {
		ctx.nvapi.isXellEnabled = true;
	}

	if (CMD_EXISTS("--dlss-nvapi=native")) {
		ctx.nvapi.isEmbeddedNvapiUsed = false;
	}

	if (CMD_EXISTS("--dlss-nvapi=embedded")) {
		ctx.nvapi.isEmbeddedNvapiUsed = true;
	}

	if (CMD_EXISTS("--dlss-reflex-emulation=on")) {
		ctx.reflex.isEmulationEnabled = true;
	}

	LPSTR lpCmdLine = GetCommandLineA(); // Using GetCommandLineA for narrow-character strings

	// Convert LPSTR to std::string
	std::string commandLine(lpCmdLine);
	std::string gpuName = "";

	// Check if the command line contains "--dlss-gpu-name="
	size_t gpuNamePos = commandLine.find("--dlss-gpu-name=");
	if (gpuNamePos != std::string::npos) {
		// Find the position of the first double quote character after "--dlss-gpu-name="
		size_t start_pos = commandLine.find("\"", gpuNamePos);
		if (start_pos != std::string::npos) {
			// Find the position of the last double quote character in the argument string
			size_t end_pos = commandLine.find("\"", start_pos + 1);
			if (end_pos != std::string::npos) {
				// Extract the GPU name part
				gpuName = commandLine.substr(start_pos + 1, end_pos - start_pos - 1);
			}
		}
	}
	

	if (ctx.gpu.isFakeRtxNameRequired) {
		if (ctx.gpu.desiredDeviceName == nullptr || strlen(ctx.gpu.desiredDeviceName) == 0) {
			gpuName = RTX_4090_FULL_NAME;
		}
	}
	// Convert the std::string to char*
	ctx.gpu.desiredDeviceName = new char[gpuName.length() + 1];

	// Copy the contents of the string to the char array
	std::copy(gpuName.begin(), gpuName.end(), ctx.gpu.desiredDeviceName);
	ctx.gpu.desiredDeviceName[gpuName.length()] = '\0'; // Add null terminator

	// GPU memory ================
	LPWSTR commandLine2 = GetCommandLineW();

	int argc;
	LPWSTR* argv = CommandLineToArgvW(commandLine2, &argc);


	for (int i = 1; i < argc; ++i) {
		std::wstring arg = argv[i];

		if (arg.find(L"--dlss-gpu-vram=") != std::wstring::npos) {
			size_t pos = arg.find(L"=");
			if (pos != std::wstring::npos) {
				std::wstring numPart = arg.substr(pos + 1);
				// Remove the trailing 'g' if present
				if (numPart.back() == L'g' || numPart.back() == L'G') {
					numPart.pop_back();
				}
				ctx.gpu.desiredDedicatedVideoMemory = _wtoi64(numPart.c_str()) * 1024 * 1024 * 1024; // Convert string to SIZE_T
			}
			else if (ctx.isValidationOn) {
				Common::Error(L"Invalid argument format. Please use --dlss-gpu-vram=[number]g", true);
			}
		}
	}

	// isReflexVsyncOn
	if (CMD_EXISTS("--dlss-reflex-fps=vsync")) {
		ctx.reflex.isVsyncEnabled = true;
		ctx.reflex.desiredFpsLimit = 0;
	}
	else {
		for (int i = 1; i < argc; ++i) {
			std::wstring arg = argv[i];

			if (arg.find(L"--dlss-reflex-fps=") != std::wstring::npos) {
				size_t pos = arg.find(L"=");
				if (pos != std::wstring::npos) {
					std::wstring numPart = arg.substr(pos + 1);
					ctx.reflex.desiredFpsLimit = _wtoi(numPart.c_str());
					ctx.reflex.realFpsLimit = (double) ctx.reflex.desiredFpsLimit;
				}
				else if (ctx.isValidationOn) {
					Common::Error(L"Invalid argument format. Please use --dlss-reflex-fps=[number]", true);
				}
			}
		}
	}

	if (ctx.ngx.isDynamicFrameGenerationEnabled && ctx.reflex.desiredFpsLimit == 0) {
		ctx.ngx.isDynamicFrameGenerationEnabled = false;
		LOG_ERROR(L"[INIT] Cannot enable Dynamic Frame Generation: FPS target not provided!");
	}

	if (ctx.reflex.desiredFpsLimit > 0) {
		LOG_INFO(L"[INIT] Configured to use limit framerate to: " + std::to_wstring(ctx.reflex.desiredFpsLimit) + L" FPS");
	}
}
