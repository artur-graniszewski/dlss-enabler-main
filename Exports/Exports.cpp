#include "Exports.h"
#include "../Utils/Common.h"
#include <algorithm>

// Function pointer arrays
FARPROC originalFuncsPsapi[27];
FARPROC originalFuncsVersion[17];
FARPROC originalFuncsWinhttp[65];
FARPROC originalFuncsWinmm[181];
FARPROC originalFuncsDbghelp[258];
FARPROC originalFuncsDxgi[5];

namespace Exports
{
	void Configure(HINSTANCE hModule)
	{
		auto proxyPath = Common::GetModuleFilePath();
		auto filename = proxyPath.filename().wstring();
		std::transform(filename.begin(), filename.end(), filename.begin(), towlower);

		// Skip if loaded as ASI or by DLSS Enabler itself
		if (filename.ends_with(L".asi") || filename == L"dlss-enabler.dll") {
			LOG_INFO(L"[INIT] DLSS Enabler started as " + filename);
			return;
		}

		// Find matching proxy type
		size_t proxyIndex;
		if (!FindProxyIndex(filename, proxyIndex)) {
			std::wstring validNames;
			for (size_t i = 0; i < compatibleNames.size(); ++i) {
				if (i > 0) validNames += L", ";
				validNames += compatibleNames[i];
			}
			Common::Error(L"Invalid proxy filename!\nValid names: " + validNames, true);
			return;
		}

		// Load original DLL and its exports
		HMODULE originalDll = LoadOriginalDll(proxyPath);
		if (!originalDll) {
			Common::Error(L"Failed to load original " + filename, true);
			return;
		}

		LoadExports(proxyIndex, originalDll);
		LOG_INFO(L"[INIT] Proxy loaded: " + filename);
	}

	bool FindProxyIndex(const std::wstring& filename, size_t& outIndex)
	{
		for (size_t i = 0; i < compatibleNames.size(); ++i) {
			if (filename == compatibleNames[i]) {
				outIndex = i;
				return true;
			}
		}
		return false;
	}

	HMODULE LoadOriginalDll(const std::filesystem::path& proxyPath)
	{
		auto stem = proxyPath.filename().stem().wstring();

		// Try loading renamed original first (e.g., "version-original.dll")
		HMODULE dll = LoadLibraryExW((stem + L"-original.dll").c_str(), nullptr, 0);
		if (dll) return dll;

		// Fall back to System32
		wchar_t systemPath[MAX_PATH];
		if (GetSystemDirectoryW(systemPath, MAX_PATH)) {
			dll = LoadLibraryExW((std::filesystem::path(systemPath) / proxyPath.filename()).c_str(), nullptr, 0);
		}
		return dll;
	}

	void LoadExports(size_t proxyIndex, HMODULE originalDll)
	{
		const auto& info = proxyInfos[proxyIndex];
		for (size_t i = 0; i < info.count; ++i) {
			info.originalFuncs[i] = GetProcAddress(originalDll, info.exportNames[i]);
		}
	}
}