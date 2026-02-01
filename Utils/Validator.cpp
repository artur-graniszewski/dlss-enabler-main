#include <wtypes.h>
#include <string>
#include "Validator.h"
#include <Shlwapi.h>
#include "Common.h"
#include "../Core/Context.h"

#define FSR3_FILES_NOT_CHECKED_YET -100
#define NVIDIA_REGISTRY_GUID L"{41FCC608-8496-4DEF-B43E-7D9BD675A6FF}"

int fsr3FilesStatus = FSR3_FILES_NOT_CHECKED_YET;

void Validator::ValidateHAGSSetting(int hags)
{
	if (hags == 1 || hags == -1 || !ctx.isValidationOn || (ctx.emulation.isHagsSpoofed && ctx.gpu.isHagsEnabled)) {
		return;
	}

	Common::Error(
		L"Hardware-Accelerated GPU Scheduling feature in Windows is " +
		Validator::DescribeHAGSSetting(hags) +
		L", consult DLSS enabler's readme.txt file on how to enable it or ignore this error by starting the game again with additional arguments: --dlss-skip-validation --dlss-hags=on", true);
}

void Validator::ValidateNvidiaSignatureSetting(int setting)
{
	if (!ctx.isValidationOn || setting == 1 || Validator::AreDLSStoFSR3FilesPresent() == 1) {
		return;
	}

	//Common::Error("NVIDIA driver signature setting is " + Validator::DescribeNVIDIASignatureSetting(setting, isDebugOn) + "\n\nPOTENTIAL SOLUTIONS:\n=====================================\nInstall optional DLSS Enabler's package with registry keys and use them to disable the signature checks\n\nOR\n\nGet a genuine _nvngx.dll file from official NVIDIA drivers package and save it into the game directory\n=====================================", true);
}

void Validator::ValidateDLSStoFSR3FilesStatus(int status)
{
	if (status >= 0 || !ctx.isValidationOn || ctx.ngx.isEmbeddedDlssgUsed) {
		return;
	}

	Common::Error(L"DLSS to FSR3 mod is " + Validator::DescribeDLSStoFSR3FilesStatus(status, L"Unknown") + L", consult DLSS enabler's readme.txt file on how to install it or ignore this error by starting the game again with additional argument: --dlss-skip-validation", true);
}

std::wstring Validator::GetDiagnosticsReport()
{
	// check HAGS first
	// start with registry
	int regHAGS = Validator::GetHAGSRegistrySetting();
	//int apiHAGS = Validator::isHAGSEnabled();
	int _nvngxDLL = Validator::Is_NVNGXDLLPresent();
	int fsr3check = Validator::AreDLSStoFSR3FilesPresent();
	//int nvngxDll = CheckNvngxPresence();
	int nvngxDll = Validator::IsNVNGXDLLPresent(true);
	int nvapi64 = 1;// not needed? CheckNvapi64Presence();

	std::wstring problems = L"";
	std::wstring passed = L"";


	if (regHAGS != 1)
	{
		problems += L"- Hardware Accelerated GPU Scheduling misconfigured:\n";

		if (regHAGS == 0) {
			problems += L"   - Capability switched off in Windows Registry\n";
		}
		else if (regHAGS < 0) {
			problems += L"   - Missing Windows Registry setting\n";
		}
	}

	if (_nvngxDLL == 1 && nvapi64 == 1 && nvngxDll > 0) {
		passed += L"+ NVIDIA Runtime Environment configured correctly\n";
		if (nvapi64 == 1) {
			passed += L"   + NVAPI64 library detected in system32 directory\n";
		}

		if (nvapi64 == 2) {
			passed += L"   + NVAPI64 library detected in local directory\n";
		}

		if (nvngxDll == 1) {
			passed += L"   + NGX runtime library detected in system32 directory\n";
		}
		else if (nvngxDll == 2) {
			passed += L"   + NGX runtime library detected in local directory\n";
		}

		if (_nvngxDLL == 1) {
			passed += L"   + _NGX runtime library detected in system32 directory\n";
		}
		else if (_nvngxDLL == 2) {
			passed += L"   + _NGX runtime library detected in local directory\n";
		}
	}
	else {
		problems += L"+ NVIDIA Runtime Environment misconfigured\n";

		if (nvngxDll == 0) {
			problems += L"   - NGX runtime library is configured but file is missing\n";
		}
		else if (nvngxDll < 0) {
			problems += L"   - NGX runtime library is not configured properly\n";
		}

		if (nvapi64 < 1) {
			problems += L"   - NVAPI64 library is missing\n";
		}

		if (_nvngxDLL == 0) {
			problems += L"   - _NGX runtime library is configured but file is missing\n";
		}
		else if (_nvngxDLL < 0) {
			problems += L"   - _NGX runtime library is not configured properly\n";
		}
	}

	if (fsr3check < 0) {
		problems += L"- DLSSG to FSR3 module files are missing\n";
	}
	else {
		passed += L"+ DLSSG to FSR3 module is present (version unknown)\n";
	}

	//std::string gpuNames = std::string(gpus.c_str());
	std::wstring summary = L"SYSTEM INFORMATION:";

	summary += L"\n\nSYSTEM CHECKS PASSED : \n";
	if (!passed.empty()) {
		summary += passed;
	}
	else {
		summary += L"   [none]\n";
	}

	summary += L"\nSYSTEM CHECKS FAILED:\n";
	if (!problems.empty()) {
		summary += problems;
	}

	else {
		summary += L"   [none]\n";
	}

	summary += L"\nNOTICE:\n";
	if (nvngxDll == 2) {
		summary += L" - NGX runtime library found in local directory (shouldn't happen if using NVIDIA GPU)\n";
	}
	else if (nvngxDll == 1) {
		summary += L" - NGX runtime library is missing in local directory (ignore if using NVIDIA GPU, applicable only to AMD/Intel GPU owners)\n";
	}

	return summary;
}


int Validator::CheckNvapi64Presence() 
{
	// Get the System32 directory path
	wchar_t system32Path[MAX_PATH];
	if (GetSystemDirectoryW(system32Path, MAX_PATH) == 0) {
		// Failed to get System32 directory
		return -1;
	}

	// Append nvapi64.dll to the System32 directory path
	PathAppendW(system32Path, L"nvapi64.dll");

	// Check if nvapi64.dll is present in System32
	if (PathFileExistsW(system32Path)) {
		return 1; // Found in System32
	}

	// Check in the directory where the process is running
	auto result = Common::IsPluginPresent(L"\\nvapi64-proxy.dll");
	if (result) {
		return 2; // found in local directory
	}

	// Not found in System32 or process directory
	return 0;
}

int Validator::CheckRegistryValueAndFile(const LPCSTR keyPath, const LPCSTR valueName, const LPCSTR appendedFileName) 
{
	HKEY hKey;
	LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &hKey);

	if (result != ERROR_SUCCESS) {
		if (result == ERROR_FILE_NOT_FOUND) {
			// Key not found
			return -2;
		}
		else {
			// General failure
			return -3;
		}
	}

	// Read the value
	CHAR valueData[MAX_PATH];
	DWORD dataSize = sizeof(valueData);
	result = RegQueryValueExA(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(valueData), &dataSize);

	RegCloseKey(hKey);

	if (result != ERROR_SUCCESS) {
		if (result == ERROR_FILE_NOT_FOUND) {
			// Value not found
			return -1;
		}
		else {
			// General failure
			return -3;
		}
	}

	// Append filename to the directory path
	PathAppendA(valueData, appendedFileName);

	// Check if the file exists
	if (PathFileExistsA(valueData)) {
		// File is present
		return 1;
	}
	else {
		// File is missing
		return 0;
	}
}


int Validator::AreDLSStoFSR3FilesPresent()
{
	if (fsr3FilesStatus != FSR3_FILES_NOT_CHECKED_YET) {
		return fsr3FilesStatus;
	}

	fsr3FilesStatus = 0;
	std::wstring directory = Common::GetModuleDirectory();

	std::wstring file1 = L"nvngx.dll";
	std::wstring file2 = L"dlssg_to_fsr3_amd_is_better.dll";
	std::wstring file3 = L"nvngx-wrapper.dll";

	// Check if file1 exists
	//if (!Common::IsPluginPresent(file1)) {
		if (!Common::IsPluginPresent(file3)) {
			fsr3FilesStatus = -1;
		}
		else {
			fsr3FilesStatus = 1;
		}
	//}

	// Check if file2 exists
	if (!Common::IsPluginPresent(file2)) {
		fsr3FilesStatus = fsr3FilesStatus - 10;
	}

	return fsr3FilesStatus;
}

std::wstring Validator::DescribeDLSStoFSR3FilesStatus(int status, std::wstring bundledFSR3ModVersion)
{
	if (status == 0) {
		return L"present (version: unknown)";
	}

	if (status == 1) {
		return L"bundled (version: " + bundledFSR3ModVersion + L")";
	}

	return L"missing (error: " + std::to_wstring(status) + L")";
}

// Helper method to check registry value
int Validator::CheckRegistryValue(LPCWSTR registryKey, LPCWSTR valueName, DWORD expectedData)
{
	HKEY hKey;
	DWORD valueData;
	DWORD dataSize = sizeof(DWORD);
	int result = -5;

	// Open the registry key
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, registryKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
		// Query the value
		if (RegQueryValueEx(hKey, valueName, nullptr, nullptr, reinterpret_cast<BYTE*>(&valueData), &dataSize) == ERROR_SUCCESS) {
			// Compare the DWORD data
			result = (valueData == expectedData) ? 1 : 0;
		}
		else {
			result = -1;
		}
		RegCloseKey(hKey);
	}
	else {
		result = -2;
	}

	return result;
}

// Method to check HAGS setting
int Validator::GetHAGSRegistrySetting()
{
	LPCWSTR registryKey = L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers";
	LPCWSTR valueName = L"HwSchMode";
	DWORD expectedData = 2;
	return CheckRegistryValue(registryKey, valueName, expectedData);
}

// Method to check NVIDIA signature setting
int Validator::GetNVIDIASignatureSetting()
{
	LPCWSTR registryKey = L"SOFTWARE\\NVIDIA Corporation\\Global";
	LPCWSTR valueName = NVIDIA_REGISTRY_GUID;
	DWORD expectedData = 1;
	int result = CheckRegistryValue(registryKey, valueName, expectedData);

	if (result != 1) {
		return result;
	}

	registryKey = L"SYSTEM\\ControlSet001\\Services\\nvlddmkm";
	valueName = NVIDIA_REGISTRY_GUID;
	result = CheckRegistryValue(registryKey, valueName, expectedData);

	if (result < 0) {
		result -= 2;
	}

	return result;
}

int Validator::Is_NVNGXDLLPresent()
{
	LPCSTR keyPath = "SYSTEM\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore";
	LPCSTR valueName = "NGXPath";
	LPCSTR appendedFileName = "_nvngx.dll";

	int result = CheckRegistryValueAndFile(keyPath, valueName, appendedFileName);

	if (result < 1) {
		// Check in the directory where the process is running
		char processPath[MAX_PATH];
		if (GetModuleFileNameA(nullptr, processPath, MAX_PATH) != 0) {
			// Extract the directory from the process path
			PathRemoveFileSpecA(processPath);

			PathAppendA(processPath, appendedFileName);

			if (PathFileExistsA(processPath)) {
				return 2; // Found in the process directory
			}
		}
	}

	return result;
}

int Validator::IsNVNGXDLLPresent(bool includeLocalPath)
{
	//LPCSTR keyPath = "SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore";
	LPCSTR keyPath = "SYSTEM\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore";
	//LPCSTR valueName = "FullPath";
	LPCSTR valueName = "NGXPath";
	LPCSTR appendedFileName = "nvngx.dll";

	int result = CheckRegistryValueAndFile(keyPath, valueName, appendedFileName);

	if (result < 1 || includeLocalPath) {
		// Check in the directory where the process is running
		char processPath[MAX_PATH];
		if (GetModuleFileNameA(nullptr, processPath, MAX_PATH) != 0) {
			PathRemoveFileSpecA(processPath);

			PathAppendA(processPath, appendedFileName);

			if (PathFileExistsA(processPath)) {
				return 2; // Found in the process directory
			}
		}
	}

	return result;
}

std::wstring Validator::DescribeNVIDIASignatureSetting(int setting, bool isDebugOn)
{
	if (AreDLSStoFSR3FilesPresent() == 1) {
		// signature checks not impacting the functionality
		return L"not applicable";
	}

	if (!isDebugOn || setting >= 0) {
		return setting == 0 ? L"disabled" : L"enabled";
	}

	std::wstring status = L"enabled ";

	switch (setting) {
	case -1:
		return status += L"(query #1 failed)";
	case -2:
		return status += L"(key #1 not found)";
	case -3:
		return status += L"(query #2 failed)";
	case -4:
		return status += L"(key #2 not found)";
	default:
		return L"unknown (unknown status)";
	}
}

std::wstring Validator::DescribeHAGSSetting(int hags)
{
	switch (hags) {
	case 1:
		return L"enabled";
		break;
	case 0:
		return L"disabled";
		break;
	case -1:
		return L"enabled (inferred)";
		break;
	case -2:
		return L"unknown (key not found)";
		break;
	default:
		return L"unknown (unknown status)";
		break;
	}
}

std::wstring ToLowercase(const std::wstring& str)
{
	std::wstring result = str;
	std::transform(result.begin(), result.end(), result.begin(), ::towlower);
	return result;
}

extern "C" __declspec(dllexport) void InitializeASI()
{
	wchar_t moduleFileName[MAX_PATH];
	GetModuleFileNameW(NULL, moduleFileName, MAX_PATH);
	std::filesystem::path dir = std::filesystem::path(moduleFileName).parent_path();

	// List of required DLLs
	std::vector<std::wstring> requiredDlls = { L"version.dll", L"winhttp.dll", L"winmm.dll", L"d3d12.dll"};

	// ProductName to search for
	std::wstring targetProductName = L"Ultimate-ASI-Loader-x64";

	// Convert directory path to lowercase for case-insensitive comparison
	std::wstring dirLowercase = ToLowercase(dir.native());

	// Search for all DLL files in the directory
	WIN32_FIND_DATAW findFileData;
	HANDLE hFind = FindFirstFileW((dir / L"*.dll").c_str(), &findFileData);
	bool loaderFound = false;
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			// Check if the found item is a file, not a directory
			if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				// Get the filename of the DLL
				std::wstring filename = findFileData.cFileName;
				// Check if the current file is one of the required DLLs
				if (std::find(requiredDlls.begin(), requiredDlls.end(), filename) != requiredDlls.end()) {
					//if (CheckResource(dir / filename, targetProductName)) {
						LOG_INFO(L"[LOADER] ASI Loader detected: " + std::wstring(filename));
						loaderFound = true;
						break;
					//}
				}
				// Check if the DLL contains the desired resource

			}
		} while (FindNextFileW(hFind, &findFileData) != 0);
		FindClose(hFind);
	}

	if (!loaderFound) {
		LOG_ERROR(L"[LOADER] ASI Loader not found under names: version.dll, winhttp.dll, winmm.dll");
		LOG_ERROR(L"[LOADER] ASI Loader relying on any other file name will break DLSS Enabler!!!");
		if (ctx.isValidationOn) {
			Common::Error(L"ASI Loader not found under names: d3d12.dll, version.dll, winhttp.dll, winmm.dll.\n\nASI Loader relying on any other file name results in DLSS Enabler being loaded to late into the game process and breaking the module", true);
		}
	}
}