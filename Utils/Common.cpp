#include "Common.h"

extern HMODULE hSelf;
static LoadLibraryW_t OriginalLoadLibraryW1;
static GetProcAddress_t OriginalGetProcAddress;

bool endsWith(const std::wstring& fullString, const std::wstring& ending)
{
	if (fullString.length() < ending.length()) {
		return false;
	}

	return (fullString.substr(fullString.length() - ending.length()) == ending);
}

std::wstring ToWideString(const std::string& fullstring)
{
	return std::wstring(fullstring.begin(), fullstring.end());
}

void Common::SetLoader(LoadLibraryW_t loader)
{
	OriginalLoadLibraryW1 = loader;
}
bool Common::IsPluginPresent(std::wstring libFileName)
{
	return IsPluginPresent(libFileName.c_str());
}

bool Common::IsPluginPresent(LPCWSTR libFileName)
{
	std::wstring directory = GetModuleDirectory();
	std::wstring fileName = directory + std::wstring(libFileName);

	if (GetFileAttributesW(fileName.c_str()) == INVALID_FILE_ATTRIBUTES) {
		return false;
	}

	return true;
}

std::wstring Common::GetProcessFileName()
{
	wchar_t fileName[MAX_PATH] = {};
	DWORD size = MAX_PATH;

	if (!QueryFullProcessImageNameW(GetCurrentProcess(), 0, fileName, &size))
		return {};

	std::wstring fullPath(fileName, size);

	size_t pos = fullPath.rfind(L'\\');
	if (pos != std::wstring::npos)
		return fullPath.substr(pos + 1);

	return fullPath;
}

void Common::SetProcAddress(GetProcAddress_t proc)
{
	OriginalGetProcAddress = proc;
}

FARPROC Common::GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	return OriginalGetProcAddress(hModule, lpProcName);
}

std::wstring Common::GetPluginVersion(LPCWSTR libFileName)
{
	std::wstring directory = GetModuleDirectory();
	std::wstring fileName = directory + std::wstring(libFileName);

	return Common::GetFileVersion(fileName.c_str());
}

HMODULE Common::GetModuleHandle()
{
	return hSelf;
}

std::wstring Common::GetFileVersion(LPCWSTR dllPath)
{
	typedef DWORD(WINAPI* LPFN_GetFileVersionInfoSizeW)(LPCWSTR, LPDWORD);
	typedef BOOL(WINAPI* LPFN_GetFileVersionInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
	typedef BOOL(WINAPI* LPFN_VerQueryValueW)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

	// Construct the full path to version.dll in the system32 directory
	wchar_t systemDirectory[MAX_PATH];
	UINT size = GetSystemDirectory(systemDirectory, MAX_PATH);
	if (size == 0 || size > MAX_PATH) {
		//std::wcerr << L"Failed to get system directory." << std::endl; 
		return L""; 
	}
	std::wstring versionDllPath = std::wstring(systemDirectory) + L"\\version.dll";

	// Load version.dll from the system32 directory
	HMODULE hVersionDll = OriginalLoadLibraryW1(versionDllPath.c_str());
	if (hVersionDll == NULL) {
		//std::wcerr << L"Failed to load version.dll" << std::endl;
		return L"";
	}

	// Get the addresses of the functions
	LPFN_GetFileVersionInfoSizeW pGetFileVersionInfoSize = (LPFN_GetFileVersionInfoSizeW)::GetProcAddress(hVersionDll, "GetFileVersionInfoSizeW");
	LPFN_GetFileVersionInfoW pGetFileVersionInfo = (LPFN_GetFileVersionInfoW)::GetProcAddress(hVersionDll, "GetFileVersionInfoW");
	LPFN_VerQueryValueW pVerQueryValue = (LPFN_VerQueryValueW)::GetProcAddress(hVersionDll, "VerQueryValueW");

	if (!pGetFileVersionInfoSize || !pGetFileVersionInfo || !pVerQueryValue) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	// Get the size of the version information
	DWORD verHandle = 0;
	DWORD verSize = pGetFileVersionInfoSize(dllPath, &verHandle);
	if (verSize == 0) {
		FreeLibrary(hVersionDll); 
		return L""; 
	}

	std::vector<char> verData(verSize);

	// Get the version information
	if (!pGetFileVersionInfo(dllPath, verHandle, verSize, verData.data())) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	// Query the version information
	VS_FIXEDFILEINFO* fileInfo = nullptr;
	size = 0;
	if (!pVerQueryValue(verData.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &size) || size == 0) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	// Check if the product version is present
	if (fileInfo->dwProductVersionMS == 0 && fileInfo->dwProductVersionLS == 0) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	// Extract version information
	std::wostringstream versionStream;
	versionStream << HIWORD(fileInfo->dwProductVersionMS) << L'.'
		<< LOWORD(fileInfo->dwProductVersionMS) << L'.'
		<< HIWORD(fileInfo->dwProductVersionLS) << L'.'
		<< LOWORD(fileInfo->dwProductVersionLS);

	// Free the loaded library
	FreeLibrary(hVersionDll);

	return std::wstring(versionStream.str());
}

std::wstring Common::GetFileProductName(LPCWSTR dllPath)
{
	typedef DWORD(WINAPI* LPFN_GetFileVersionInfoSizeW)(LPCWSTR, LPDWORD);
	typedef BOOL(WINAPI* LPFN_GetFileVersionInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
	typedef BOOL(WINAPI* LPFN_VerQueryValueW)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

	// Construct the full path to version.dll in the system32 directory
	wchar_t systemDirectory[MAX_PATH];
	UINT size = GetSystemDirectory(systemDirectory, MAX_PATH);
	if (size == 0 || size > MAX_PATH) {
		return L"";
	}
	std::wstring versionDllPath = std::wstring(systemDirectory) + L"\\version.dll";

	// Load version.dll from the system32 directory
	HMODULE hVersionDll = LoadLibraryW(versionDllPath.c_str());
	if (hVersionDll == NULL) {
		return L"";
	}

	// Get the addresses of the functions
	LPFN_GetFileVersionInfoSizeW pGetFileVersionInfoSize = (LPFN_GetFileVersionInfoSizeW)::GetProcAddress(hVersionDll, "GetFileVersionInfoSizeW");
	LPFN_GetFileVersionInfoW pGetFileVersionInfo = (LPFN_GetFileVersionInfoW)::GetProcAddress(hVersionDll, "GetFileVersionInfoW");
	LPFN_VerQueryValueW pVerQueryValue = (LPFN_VerQueryValueW)::GetProcAddress(hVersionDll, "VerQueryValueW");

	if (!pGetFileVersionInfoSize || !pGetFileVersionInfo || !pVerQueryValue) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	// Get the size of the version information
	DWORD verHandle = 0;
	DWORD verSize = pGetFileVersionInfoSize(dllPath, &verHandle);
	if (verSize == 0) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	std::vector<char> verData(verSize);

	// Get the version information
	if (!pGetFileVersionInfo(dllPath, verHandle, verSize, verData.data())) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	// Query the product name information
	LPVOID productNamePtr = nullptr;
	UINT productNameSize = 0;
	if (!pVerQueryValue(verData.data(), L"\\StringFileInfo\\040904b0\\ProductName", &productNamePtr, &productNameSize)) {
		FreeLibrary(hVersionDll);
		return L"";
	}

	// Free the loaded library
	FreeLibrary(hVersionDll);

	// Return the product name as a wstring
	return std::wstring(static_cast<wchar_t*>(productNamePtr), productNameSize - 1);
}

HMODULE Common::LoadPlugin(LPCWSTR libFileName)
{
	std::wstring directory = GetModuleDirectory();

	std::wstring actualFileName = directory + std::wstring(libFileName);

	if (GetFileAttributesW(actualFileName.c_str()) == INVALID_FILE_ATTRIBUTES) {
		LOG_ERROR(L"[LOADER] Loading " + actualFileName + L": failed (file is missing)");
		return NULL;
	}
	
	LOG_DEBUG(L"[LOADER] Loading " + std::wstring(actualFileName));

	auto result = OriginalLoadLibraryW1(actualFileName.c_str());
	if (!result) {
		auto errorCode = GetLastError();
		LOG_ERROR(L"[LOADER] Loading " + std::wstring(actualFileName) + L": failed (error code: " + std::to_wstring(errorCode) + L")");
	}
	else {
		LOG_INFO(L"[LOADER] Loading " + std::wstring(actualFileName) + L": succeeded");
	}

	return result;
}

HMODULE Common::LoadPlugin(std::wstring libFileName)
{
	return LoadPlugin(libFileName.c_str());
}

std::wstring Common::GetModuleDirectory()
{
	std::wstring fileName = GetModuleFilePath().filename().wstring();
	std::filesystem::path filePath;
	if (fileName == L"dlss-enabler.asi") {
		filePath = GetProcessFilePath();
	}
	else {
		filePath = GetModuleFilePath();
	}

	std::wstring directory = filePath.parent_path().wstring() + L"\\";

	return directory;
}

std::filesystem::path Common::GetModuleFilePath()
{
	wchar_t fileName[MAX_PATH];
	GetModuleFileNameW(hSelf, fileName, MAX_PATH);

	std::filesystem::path filePath(fileName);

	return filePath;
}

std::filesystem::path Common::GetModuleFilePath(HMODULE module)
{
	wchar_t fileName[MAX_PATH];
	GetModuleFileNameW(module, fileName, MAX_PATH);

	std::filesystem::path filePath(fileName);

	return filePath;
}

std::filesystem::path Common::GetProcessFilePath()
{
	wchar_t fileName[MAX_PATH];
	GetModuleFileNameW(NULL, fileName, MAX_PATH);

	std::filesystem::path filePath(fileName);

	return filePath;
}

void Common::Error(const std::wstring& reason, const bool shouldKill)
{
	DisplayErrorMessage(reason, shouldKill);
}

void Common::DisplayErrorMessage(const std::wstring& reason, const bool shouldKill)
{
	std::wstring title = L"DLSS Enabler";
	std::wstring message = reason + L" " + (shouldKill ? L"\n\nDLSS enabler cannot start" : L"\n\nContinuing without DLSS enabler") + L"...";

	MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);

	if (shouldKill) {
		KillProcess();
	}
}

double Common::GetCurrentTimeMsec()
{
	double currentTimeMs = 0.0f;
	static LARGE_INTEGER frequency;
	static BOOL useQpc = QueryPerformanceFrequency(&frequency);

	if (useQpc) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		currentTimeMs = double(1000.0 * now.QuadPart) / frequency.QuadPart;
	}
	else {
		currentTimeMs = double(GetTickCount64());
	}

	return currentTimeMs;
}

void Common::KillProcess()
{
	const HANDLE current_process = GetCurrentProcess();
	TerminateProcess(current_process, NULL);
	CloseHandle(current_process);
}

void Common::Info(const std::wstring& reason)
{
	MessageBoxW(nullptr, reason.c_str(), L"DLSS Enabler", MB_ICONINFORMATION | MB_OK);
}

void Common::CheckModConflict()
{
	// List of files to check for conflicts
	std::vector<std::string> fileNames = { "dxgi.dll", "psapi.dll", "winhttp.dll", "version.dll", "winmm.dll", "dbghelp.dll", "dlss-enabler.asi", "dlss-enabler-headless.dll" };
	auto reportedFileName = GetModuleFilePath().filename().string();
	std::string currentFileName;
	currentFileName.reserve(reportedFileName.length());

	for (char c : reportedFileName) {
		currentFileName += std::tolower(c);
	}

	bool isConflicting = false;

	for (const std::string& fileName : fileNames) {
		if (fileName.compare(currentFileName) == 0) {
			continue;
		}

		// check if ASI module is present...
		if (fileName == "dlss-enabler.asi") {
			auto path = Common::GetProcessFilePath().parent_path();
			std::wstring pluginPath = std::wstring(path) + L"\\plugins\\dlss-enabler.asi";
			if (GetFileAttributesW(pluginPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
				isConflicting = true;
			}
		}

		if (GetFileAttributesA(fileName.c_str()) != INVALID_FILE_ATTRIBUTES) {
			// file exists, check metadata
			std::wstring prodName = GetFileProductName(ToWideString(fileName).c_str());
			isConflicting = prodName == L"DLSS Enabler";
		}

		if (isConflicting) {
			Common::Error(L"Conflicting installation of DLSS Enabler detected, please uninstall previous version of the module first\n\nConflicting file: " + ToWideString(fileName), true);
		}
	}
}

void Common::Initialize()
{
	OriginalLoadLibraryW1 = ::LoadLibraryW;
	CheckModConflict();
}