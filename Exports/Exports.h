#pragma once
#include <wtypes.h>
#include <string>
#include <filesystem>
#include "Resources.h"

// Original function pointers for each proxy type
extern "C" FARPROC originalFuncsPsapi[27];
extern "C" FARPROC originalFuncsVersion[17];
extern "C" FARPROC originalFuncsWinhttp[65];
extern "C" FARPROC originalFuncsWinmm[181];
extern "C" FARPROC originalFuncsDbghelp[258];
extern "C" FARPROC originalFuncsDxgi[5];

namespace Exports
{
	// Proxy configuration data - arrays must match in order and size
	inline constexpr std::array<const wchar_t*, 6> compatibleNames = {
		L"psapi.dll",
		L"version.dll",
		L"winhttp.dll",
		L"winmm.dll",
		L"dbghelp.dll",
		L"dxgi.dll"
	};

	// Export name arrays from Resources.h mapped to function pointer arrays
	struct ProxyInfo {
		const char* const* exportNames;
		FARPROC* originalFuncs;
		size_t count;
	};

	inline const std::array<ProxyInfo, 6> proxyInfos = { {
		{ ExportNames_psapi.data(),   originalFuncsPsapi,   ExportNames_psapi.size()   },
		{ ExportNames_version.data(), originalFuncsVersion, ExportNames_version.size() },
		{ ExportNames_winhttp.data(), originalFuncsWinhttp, ExportNames_winhttp.size() },
		{ ExportNames_winmm.data(),   originalFuncsWinmm,   ExportNames_winmm.size()   },
		{ ExportNames_dbghelp.data(), originalFuncsDbghelp, ExportNames_dbghelp.size() },
		{ ExportNames_dxgi.data(),    originalFuncsDxgi,    ExportNames_dxgi.size()    }
	} };

	void Configure(HINSTANCE hModule);
	bool FindProxyIndex(const std::wstring& filename, size_t& outIndex);
	HMODULE LoadOriginalDll(const std::filesystem::path& proxyPath);
	void LoadExports(size_t proxyIndex, HMODULE originalDll);
}