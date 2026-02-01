#pragma once
#include <string>
#include <wtypes.h>

class Validator {
public:
	static int AreDLSStoFSR3FilesPresent();
	static int GetHAGSRegistrySetting();
	static int GetNVIDIASignatureSetting();
	static std::wstring DescribeNVIDIASignatureSetting(int setting, bool isDebugOn);
	static std::wstring DescribeHAGSSetting(int hags);
	static std::wstring DescribeDLSStoFSR3FilesStatus(int status, std::wstring bundledFSR3ModVersion);
	static int IsNVNGXDLLPresent(bool includeLocalPath);
	static int Is_NVNGXDLLPresent();
	static std::wstring GetDiagnosticsReport();
	static void ValidateHAGSSetting(int hags);
	static void ValidateNvidiaSignatureSetting(int setting);
	static void ValidateDLSStoFSR3FilesStatus(int status);

private:
	static int CheckRegistryValue(LPCWSTR registryKey, LPCWSTR valueName, DWORD expectedData);
	static int CheckRegistryValueAndFile(const LPCSTR keyPath, const LPCSTR valueName, const LPCSTR appendedFileName);
	static int CheckNvapi64Presence();

};