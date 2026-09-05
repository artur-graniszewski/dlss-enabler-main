#include "Exports.h"
#include "../Utils/Common.h"

FARPROC OriginalFunctions_psapi[27];
FARPROC OriginalFunctions_version[17];
FARPROC OriginalFunctions_winhttp[65];
FARPROC OriginalFunctions_winmm[181];
FARPROC OriginalFunctions_dbghelp[258];
FARPROC OriginalFunctions_dxgi[5];
static LoadLibraryExW_t OriginalLoadLibraryExW = LoadLibraryExW;

void Exports::ConfigureProxy(HINSTANCE hModule)
{
	// Get execution path
	std::vector<char> pathBuf;
	DWORD copied = 0;
	do {
		pathBuf.resize(pathBuf.size() + MAX_PATH);
		copied = GetModuleFileNameA(nullptr, pathBuf.data(), static_cast<DWORD>(pathBuf.size()));
	} while (copied >= pathBuf.size());

	pathBuf.resize(copied);

	const std::filesystem::path filepath(pathBuf.begin(), pathBuf.end());

	// Get file path of proxy, tolowercase the file name
	const auto proxyFilepath = Common::GetModuleFilePath();
	auto ProxyFilename = proxyFilepath.filename().wstring();
	std::transform(ProxyFilename.begin(), ProxyFilename.end(), ProxyFilename.begin(), towlower);

	if (endsWith(ProxyFilename, L".asi")) {
		LOG_INFO(L"[INIT] DLSS Enabler started as an ASI module");
		return;
	}

	if (endsWith(ProxyFilename, L"dlss-enabler.dll")) {
		LOG_INFO(L"[INIT] DLSS Enabler started by DLSS Enabler's DXGI or EXE");
		return;
	}

	if (endsWith(ProxyFilename, L"dlss-enabler-headless.dll")) {
		LOG_INFO(L"[INIT] DLSS Enabler started by DLSS Enabler's DXGI or EXE");
		return;
	}

	// Make proxy name list
	std::wstring names;
	bool _1 = true;
	for (auto name : Exports::CompatibleFileNames) {
		if (_1) {
			_1 = false;
			names += name;
		}
		else {
			names += L", ";
			names += name;
		}
	}

	// Check if is compatible proxy
	std::size_t index = -1;
	if (!Exports::IsFileNameCompatible(ProxyFilename, &index)) {
		Common::Error(L"Proxy has an incompatible file name!\nValid names are: " + names + L"\n", true);
		return;
	}


	// Load original libs
	const HMODULE originalDll = LoadOriginalProxy(proxyFilepath, proxyFilepath.filename().stem().wstring());
	if (!originalDll) {
		Common::Error(L"Failed to load original " + proxyFilepath.wstring() + L" file!", true);
		return;
	}

	// Load original lib exports
	Exports::Load(index, originalDll);
	LOG_INFO(L"[INIT] DLSS Enabler's DLL proxy loaded: " + ProxyFilename);
}

HMODULE Exports::LoadOriginalProxy(const std::filesystem::path& proxyFilepath, const std::wstring& proxyFilepathNoExt)
{
	HMODULE originalDll = OriginalLoadLibraryExW((proxyFilepathNoExt + L"-original.dll").c_str(), NULL, 0);

	if (!originalDll) {
		wchar_t system32_path[MAX_PATH];

		if (GetSystemDirectoryW(system32_path, MAX_PATH) == NULL) {
			Common::Error(L"Failed to find System32 directory!", true);
			return nullptr;
		}

		const auto path = std::filesystem::path(system32_path);
		originalDll = OriginalLoadLibraryExW((path / proxyFilepath.filename()).c_str(), NULL, 0);
	}

	return originalDll;
}


bool Exports::IsFileNameCompatible(const std::wstring& proxyFilename, std::size_t* index)
{
	for (std::size_t i = 0; i < CompatibleFileNames.size(); ++i) {
		if (proxyFilename == CompatibleFileNames[i]) {
			*index = i;
			return true;
		}
	}

	return false;
}

void Exports::Load(HMODULE originalDll, const char* const* exportNames, FARPROC* originalFuncs, std::size_t arraySize)
{
	for (std::size_t i = 0; i < arraySize; i++) {
		originalFuncs[i] = GetProcAddress(originalDll, exportNames[i]);
	}
}