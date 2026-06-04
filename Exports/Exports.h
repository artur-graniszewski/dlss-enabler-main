#pragma once
#include <wtypes.h>
#include <string>
#include "Resources.h"
#include <filesystem>

extern "C" FARPROC OriginalFunctions_psapi[27];
extern "C" FARPROC OriginalFunctions_version[17];
extern "C" FARPROC OriginalFunctions_dbghelp[258];
extern "C" FARPROC OriginalFunctions_winhttp[65];
extern "C" FARPROC OriginalFunctions_winmm[181];
extern "C" FARPROC OriginalFunctions_dxgi[5];

namespace Exports {
	inline constexpr std::array<const wchar_t*, 6> CompatibleFileNames = {
		L"psapi.dll",
		L"version.dll",
		L"winhttp.dll",
		L"winmm.dll",
		L"dbghelp.dll",
		L"dxgi.dll"
	};

	void Load(HMODULE originalDll, const char* const* exportNames, FARPROC* originalFuncs, std::size_t arraySize);

	inline void Load_psapi(const HMODULE originalDll) { Load(originalDll, ExportNames_psapi.data(), OriginalFunctions_psapi, ExportNames_psapi.size()); }
	inline void Load_version(const HMODULE originalDll) { Load(originalDll, ExportNames_version.data(), OriginalFunctions_version, ExportNames_version.size()); }
	inline void Load_winhttp(const HMODULE originalDll) { Load(originalDll, ExportNames_winhttp.data(), OriginalFunctions_winhttp, ExportNames_winhttp.size()); }
	inline void Load_winmm(const HMODULE originalDll) { Load(originalDll, ExportNames_winmm.data(), OriginalFunctions_winmm, ExportNames_winmm.size()); }
	inline void Load_dbghelp(const HMODULE originalDll) { Load(originalDll, ExportNames_dbghelp.data(), OriginalFunctions_dbghelp, ExportNames_dbghelp.size()); }
	inline void Load_dxgi(const HMODULE originalDll) { Load(originalDll, ExportNames_dxgi.data(), OriginalFunctions_dxgi, ExportNames_dxgi.size()); }

	using load_exports_func = decltype(&Load_psapi);
	inline constexpr std::array<load_exports_func, 6> load_funcs = {
		Load_psapi,
		Load_version,
		Load_winhttp,
		Load_winmm,
		Load_dbghelp,
		Load_dxgi
	};

	constexpr void Load(const std::size_t index, const HMODULE originalDll) {

		load_funcs[index](originalDll);
	}
	bool IsFileNameCompatible(const std::wstring& proxyFilename, std::size_t* index);
	void ConfigureProxy(HINSTANCE hModule);
	HMODULE LoadOriginalProxy(const std::filesystem::path& proxyFilepath, const std::wstring& proxyFilepathNoExt);
};