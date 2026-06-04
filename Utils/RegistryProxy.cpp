#include "Common.h"
#include <Wtypes.h>
#include <map>
#include "../Core/Context.h"
#include "RegistryProxy.h"
#define REGHOOK_ONA


std::wstring GetKeyName(HKEY hKey) {
	WCHAR keyNameBuffer[MAX_PATH];
	DWORD keyNameSize = sizeof(keyNameBuffer) / sizeof(keyNameBuffer[0]);
	LONG result = RegQueryInfoKeyW(
		hKey,
		keyNameBuffer,
		&keyNameSize,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	);
	if (result == ERROR_SUCCESS) {
		return std::wstring(keyNameBuffer);
	}
	else {
		return L"<Unknown Key>";
	}
}

// Helper function to get the predefined key name
std::wstring GetPredefinedKeyName(HKEY hKey) {
	std::map<HKEY, std::wstring> predefinedKeys = {
		{HKEY_CLASSES_ROOT, L"HKEY_CLASSES_ROOT"},
		{HKEY_CURRENT_USER, L"HKEY_CURRENT_USER"},
		{HKEY_LOCAL_MACHINE, L"HKEY_LOCAL_MACHINE"},
		{HKEY_USERS, L"HKEY_USERS"},
		{HKEY_CURRENT_CONFIG, L"HKEY_CURRENT_CONFIG"}
	};

	if (predefinedKeys.find(hKey) != predefinedKeys.end()) {
		return predefinedKeys[hKey];
	}

	return L"";
}

// Helper function to build the full registry key path
std::wstring BuildRegistryKeyPath(HKEY hKey) {
	WCHAR keyNameBuffer[MAX_PATH];
	DWORD keyNameSize = sizeof(keyNameBuffer) / sizeof(keyNameBuffer[0]);
	HKEY hParentKey = hKey;
	std::wstring fullPath;

	// Traverse the key hierarchy
	while (hParentKey) {
		LONG result = RegQueryInfoKeyW(
			hParentKey,
			keyNameBuffer,
			&keyNameSize,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL
		);

		if (result != ERROR_SUCCESS) {
			break;
		}

		std::wstring currentKeyName = keyNameBuffer;
		if (fullPath.empty()) {
			fullPath = currentKeyName;
		}
		else {
			fullPath = currentKeyName + L"\\" + fullPath;
		}

		// Move up to the parent key (this part is tricky as we need a way to get parent HKEY)
		// For simplicity, we assume we are done after one level up.
		hParentKey = NULL; // You might need a more sophisticated way to move up the hierarchy.
	}

	std::wstring predefinedKeyName = GetPredefinedKeyName(hKey);
	if (!predefinedKeyName.empty()) {
		fullPath = predefinedKeyName + L"\\" + fullPath;
	}

	return fullPath;
}

LSTATUS WINAPI proxy_RegGetValueW(
	HKEY    hkey,
	LPCWSTR lpSubKey,
	LPCWSTR lpValue,
	DWORD   dwFlags,
	LPDWORD pdwType,
	PVOID   pvData,
	LPDWORD pcbData
) {
	// lpValue can be NULL per WinAPI spec - passthrough safely
	if (lpValue != NULL) {
		std::wstring valueName = std::wstring(lpValue);
#ifdef REGHOOK_ON
		LOG_WARNING(L"[REG]     |___(RegGetValueW) VALUE: " + valueName);
#endif
	}
	// Call the original function
	return OriginalRegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LSTATUS WINAPI proxy_RegGetValueA(
	HKEY    hkey,
	LPCSTR lpSubKey,
	LPCSTR lpValue,
	DWORD   dwFlags,
	LPDWORD pdwType,
	PVOID   pvData,
	LPDWORD pcbData
) {
	if (lpValue != NULL) {
		std::string valueName = std::string(lpValue);
#ifdef REGHOOK_ON
		LOG_WARNING(L"[REG]     |___(RegGetValueA) VALUE: " + std::wstring(valueName.begin(), valueName.end()));
#endif
	}
	// Call the original function
	return OriginalRegGetValueA(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}


LONG WINAPI proxy_RegQueryValueExW(
	HKEY hKey,
	LPCWSTR lpValueName,
	LPVOID lpReserved,
	LPDWORD lpType,
	LPBYTE lpData,
	LPDWORD lpcbData
) {
	std::wstring valueName = L"";

	if (lpValueName != NULL) {
		valueName = std::wstring(lpValueName);
#ifdef REGHOOK_ON
		LOG_WARNING(L"[REG]     |___(RegQueryValueExW) VALUE: " + valueName);
#endif
	}
	static bool isDriverVersionSpoofingReported = false;
	static bool isHardwareIdSpoofingReported = false;
	static bool isHwSchModeSpoofingReported = false;

	if (valueName == L"HwSchMode") {
		if (!isHwSchModeSpoofingReported) {
			isHwSchModeSpoofingReported = true;
			LOG_INFO(L"[INIT] HwSchMode spoofed in system registry");
		}

		// Check if lpcbData is not NULL
		if (lpcbData != nullptr) {
			// If lpData is NULL, we're being asked for the required size
			if (lpData == nullptr) {
				static bool isLogged = false;
				if (!isLogged) {
					LOG_INFO(L"[INIT] HwSchMode spoofed in system registry: returning data size only");
					isLogged = true;
				}
				*lpcbData = sizeof(DWORD); // Indicate the required size

				// Set the type to REG_DWORD
				if (lpType) {
					*lpType = REG_DWORD;
				}
				return ERROR_SUCCESS;
			}

			// Check if the buffer is large enough
			if (*lpcbData >= sizeof(DWORD)) {
				*(DWORD*)lpData = 2;

				// Set the type to REG_DWORD
				if (lpType) {
					*lpType = REG_DWORD;
				}

				// Set the size of the data returned
				*lpcbData = sizeof(DWORD);

				// Return success
				LOG_INFO(L"[INIT] HwSchMode spoofed in system registry: success");
				return ERROR_SUCCESS;
			}
			else {
				// Buffer is too small, return required size
				*lpcbData = sizeof(DWORD);
				LOG_INFO(L"[INIT] HwSchMode spoofed in system registry: more data");
				return ERROR_MORE_DATA;
			}
		}

		LOG_INFO(L"[INIT] HwSchMode spoofed in system registry: invalid params");
		// If lpcbData is NULL, return an error
		return ERROR_INVALID_PARAMETER;
	}

	// Call the original function
	auto result = originalRegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);

	// Check the result of the query and if the valueName matches
	if (result == ERROR_SUCCESS && (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled)) {
		if (valueName == L"DriverVersion") {
			// Spoofed value to be written
			if (!isDriverVersionSpoofingReported) {
				isDriverVersionSpoofingReported = true;
				LOG_INFO(L"[INIT] Driver version spoofed in system registry");
			}

			const std::wstring spoofedValue = L"31.0.15.5244";
			size_t spoofedValueSize = (spoofedValue.size() + 1) * sizeof(wchar_t); // Size in bytes including null terminator

			if (lpData != nullptr && lpcbData != nullptr) {
				// Check if buffer size is sufficient
				if (*lpcbData >= spoofedValueSize) {
					// Copy the spoofed value into lpData
					std::memcpy(lpData, spoofedValue.c_str(), spoofedValueSize);
					// Update lpcbData with the size of the spoofed value
					*lpcbData = static_cast<DWORD>(spoofedValueSize);
				}
				else {
					// If buffer is too small, set lpcbData to the required size
					*lpcbData = static_cast<DWORD>(spoofedValueSize);
					result = ERROR_MORE_DATA; // Indicate that buffer was too small
				}
			}
		}

		if (valueName == L"HardwareID") {
			if (!isHardwareIdSpoofingReported) {
				isHardwareIdSpoofingReported = true;
				LOG_INFO(L"[INIT] Hardware ID spoofed in system registry");
			}

			// Validate pointers before accessing data
			if (lpData != nullptr && lpcbData != nullptr && *lpcbData >= sizeof(wchar_t)) {
				// Handle REG_SZ type
				std::wstring data(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t));
				std::wstring newData = data;
				size_t pos = 0;

				// Replace VEN_1002 and VEN_8086 with VEN_10DE in REG_SZ
				while ((pos = newData.find(L"VEN_1002", pos)) != std::wstring::npos) {
					newData.replace(pos, 8, L"VEN_10DE");
					pos += 8; // Move past the replacement
				}
				pos = 0;
				while ((pos = newData.find(L"VEN_8086", pos)) != std::wstring::npos) {
					newData.replace(pos, 8, L"VEN_10DE");
					pos += 8; // Move past the replacement
				}

				LOG_ERROR(L"NEW PAYLOAD: " + newData);
				// Copy the new data back
				wcscpy_s(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t), newData.c_str());
			}
		}
	}

	return result;
}

LONG WINAPI proxy_RegQueryValueExA(
	HKEY hKey,
	LPCSTR lpValueName,
	LPVOID lpReserved,
	LPDWORD lpType,
	LPBYTE lpData,
	LPDWORD lpcbData
) {
	std::string valueName = "";
	if (lpValueName != NULL) {
		valueName = std::string(lpValueName);
#ifdef REGHOOK_ON
		LOG_WARNING(L"[REG]     |___(RegQueryValueExA) VALUE: " + std::wstring(valueName.begin(), valueName.end()));
#endif
	}

	// Call the original function
	auto result = originalRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
	// Check the result of the query and if the valueName matches
	if (result == ERROR_SUCCESS && (valueName == "HardwareID")) {
		LOG_ERROR(L"=[RegQueryValueExA]=SPOOFED HARDWAREID======================================");
		// Check the type of the data
		if (lpType && *lpType == REG_SZ) {
			// Validate pointers before accessing data
			if (lpData != nullptr && lpcbData != nullptr && *lpcbData > 0) {
				// Handle REG_SZ type
				std::string data(reinterpret_cast<char*>(lpData), *lpcbData);
				std::string newData = data;

				// Replace VEN_1002 and VEN_8086 with VEN_10DE in REG_SZ
				size_t pos = 0;
				while ((pos = newData.find("VEN_1002", pos)) != std::string::npos) {
					newData.replace(pos, 8, "VEN_10DE");
					pos += 8; // Move past the replacement
				}
				pos = 0;
				while ((pos = newData.find("VEN_8086", pos)) != std::string::npos) {
					newData.replace(pos, 8, "VEN_10DE");
					pos += 8; // Move past the replacement
				}

				// Copy the new data back
				memcpy(lpData, newData.c_str(), newData.size() + 1); // +1 for null terminator
			}
		}
		else if (lpType && *lpType == REG_MULTI_SZ) {
			// Validate pointers before accessing data
			if (lpData != nullptr && lpcbData != nullptr && *lpcbData > 0) {
				// Handle REG_MULTI_SZ type
				std::vector<char> data(*lpcbData);
				memcpy(data.data(), lpData, *lpcbData);

				std::string newData;
				size_t start = 0;
				size_t end;

				// Iterate through each string in the multi-string data
				while (start < data.size()) {
					end = start;
					// Find the next null terminator
					while (end < data.size() && data[end] != '\0') {
						++end;
					}

					// Extract the string entry
					std::string entry(data.data() + start, end - start);
					size_t pos = 0;

					// Replace VEN_1002 and VEN_8086 in each entry
					while ((pos = entry.find("VEN_1002", pos)) != std::string::npos) {
						entry.replace(pos, 8, "VEN_10DE");
						pos += 8;
					}
					pos = 0;
					while ((pos = entry.find("VEN_8086", pos)) != std::string::npos) {
						entry.replace(pos, 8, "VEN_10DE");
						pos += 8;
					}

					newData += entry + '\0'; // Append the modified entry and a null terminator
					start = end + 1; // Move to the next string
				}
				newData += '\0'; // Add the final null terminator

				// Copy the new data back
				memcpy(lpData, newData.c_str(), newData.size() + 1); // +1 for null terminator
			}
		}
	}
	return result;
}

LSTATUS WINAPI proxy_RegOpenKeyExW(
	HKEY hKey,
	LPCWSTR lpSubKey,
	DWORD ulOptions,
	REGSAM samDesired,
	PHKEY phkResult
) {
	// lpSubKey can be NULL per WinAPI spec (means open the same key again)
	if (!lpSubKey) {
		return OriginalRegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	}

	std::wstring subKey = std::wstring(lpSubKey);

	if (subKey == L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers" && !ctx.isRunningUnderWindows) {
		LOG_INFO(L"[INIT] GraphicsDrivers spoofed in System Registry");

		HKEY magicKey = (HKEY)2137;
		*phkResult = magicKey;

		return ERROR_SUCCESS;
	}
	/*
	if (
		subKey.find(L"VEN_10DE") != std::wstring::npos
		||
		subKey.find(L"VEN_1002") != std::wstring::npos
		||
		subKey.find(L"VEN_8086") != std::wstring::npos
		) {
		LOG_ERROR(L"=[RegOpenKeyExW] KEY SPOOFED==================================");
		HKEY magicKey = (HKEY)2137;
		*phkResult = magicKey;

		return ERROR_SUCCESS;
	}
	*/

	// Call the original function using its pointer
	auto result = OriginalRegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);

#ifdef REGHOOK_ON
	LOG_WARNING(L"[REG]");
	LOG_WARNING(L"[REG] |___ (RegOpenKeyExW) KEY: " + subKey + L" (" + (result == ERROR_SUCCESS ? L"present" : L"missing") + L")");
#endif

	return result;
}

LSTATUS WINAPI proxy_RegOpenKeyA(
	HKEY hKey,
	LPCSTR lpSubKey,
	PHKEY phkResult
) {
	// lpSubKey can be NULL per WinAPI spec
	if (!lpSubKey) {
		return OriginalRegOpenKeyA(hKey, lpSubKey, phkResult);
	}

	std::string subKey = std::string(lpSubKey);

	/*
	if (
		subKey.find("VEN_10DE") != std::string::npos
		||
		subKey.find("VEN_1002") != std::string::npos
		||
		subKey.find("VEN_8086") != std::string::npos
		) {
		LOG_ERROR(L"=[RegOpenKeyA] KEY SPOOFED==================================");

		// Hardcoded magic key value
		HKEY magicKey = (HKEY)2137;
		*phkResult = magicKey;

		return ERROR_SUCCESS;
	}
	*/

	// Call the original function using its pointer
	auto result = OriginalRegOpenKeyA(hKey, lpSubKey, phkResult);

#ifdef REGHOOK_ON
	LOG_WARNING(L"[REG]");
	LOG_WARNING(L"[REG] |___ (RegOpenKeyA) KEY: " + std::wstring(subKey.begin(), subKey.end()) + L" (" + (result == ERROR_SUCCESS ? L"present" : L"missing") + L")");
#endif

	return result;
}

LSTATUS WINAPI proxy_RegOpenKeyW(
	HKEY hKey,
	LPCWSTR lpSubKey,
	PHKEY phkResult
) {
	// lpSubKey can be NULL per WinAPI spec
	if (!lpSubKey) {
		return OriginalRegOpenKeyW(hKey, lpSubKey, phkResult);
	}

	std::wstring subKey = std::wstring(lpSubKey);

	/*
	if (
		subKey.find(L"VEN_10DE") != std::wstring::npos
		||
		subKey.find(L"VEN_1002") != std::wstring::npos
		||
		subKey.find(L"VEN_8086") != std::wstring::npos
		) {
		LOG_ERROR(L"=[RegOpenKeyW] KEY SPOOFED==================================");
		// Hardcoded magic key value
		HKEY magicKey = (HKEY)2137;
		*phkResult = magicKey;

		return ERROR_SUCCESS;
	}
	*/

	// Call the original function using its pointer
	auto result = OriginalRegOpenKeyW(hKey, lpSubKey, phkResult);

#ifdef REGHOOK_ON
	LOG_WARNING(L"[REG]");
	LOG_WARNING(L"[REG] |___ (RegOpenKeyW) KEY: " + subKey + L" (" + (result == ERROR_SUCCESS ? L"present" : L"missing") + L")");
#endif
	return result;
}

// Detour function
LSTATUS WINAPI proxy_RegOpenKeyExA(
	HKEY hKey,
	LPCSTR lpSubKey,
	DWORD ulOptions,
	REGSAM samDesired,
	PHKEY phkResult
) {
	// lpSubKey can be NULL per WinAPI spec
	if (!lpSubKey) {
		return OriginalRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	}

	std::string subKey = std::string(lpSubKey);

	/*
	if (
		subKey.find("VEN_10DE") != std::string::npos
		||
		subKey.find("VEN_1002") != std::string::npos
		||
		subKey.find("VEN_8086") != std::string::npos
		) {
		LOG_ERROR(L"=[RegOpenKeyExA] KEY SPOOFED==================================");
		HKEY magicKey = (HKEY)2137;
		*phkResult = magicKey;

		return ERROR_SUCCESS;
	}
	*/

	// Call the original function using its pointer
	auto result = OriginalRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
#ifdef REGHOOK_ON
	LOG_WARNING(L"[REG]");
	LOG_WARNING(L"[REG] |___ (RegOpenKeyExA) KEY: " + std::wstring(subKey.begin(), subKey.end()) + L" (" + (result == ERROR_SUCCESS ? L"present" : L"missing") + L")");
#endif

	return result;
}

// Detour function for RegQueryMultipleValues
LSTATUS WINAPI proxy_RegQueryMultipleValues(
	HKEY     hKey,
	PVALENT  val_list,
	DWORD    num_vals,
	LPWSTR   lpValueBuf,
	LPDWORD  ldwTotsize
) {
	// Custom behavior (e.g., logging)
#ifdef REGHOOK_ON
	//LOG_WARNING(L"[REG] ========== DetourRegQueryMultipleValues called");
#endif
	// Call the original function
	return OriginalRegQueryMultipleValues(hKey, val_list, num_vals, lpValueBuf, ldwTotsize);
}