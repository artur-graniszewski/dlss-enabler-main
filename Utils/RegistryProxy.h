#pragma once

typedef LONG(WINAPI* RegQueryValueExW_Ptr)(
	HKEY hKey,
	LPCWSTR lpValueName,
	LPVOID lpReserved,
	LPDWORD lpType,
	LPBYTE lpData,
	LPDWORD lpcbData
	);

typedef LONG(WINAPI* RegQueryValueExA_Ptr)(
	HKEY hKey,
	LPCSTR lpValueName,
	LPVOID lpReserved,
	LPDWORD lpType,
	LPBYTE lpData,
	LPDWORD lpcbData
	);

typedef LSTATUS(WINAPI* RegGetValueW_Ptr)(
	HKEY    hkey,
	LPCWSTR lpSubKey,
	LPCWSTR lpValue,
	DWORD   dwFlags,
	LPDWORD pdwType,
	PVOID   pvData,
	LPDWORD pcbData
	);

typedef LSTATUS(WINAPI* RegGetValueA_Ptr)(
	HKEY    hkey,
	LPCSTR lpSubKey,
	LPCSTR lpValue,
	DWORD   dwFlags,
	LPDWORD pdwType,
	PVOID   pvData,
	LPDWORD pcbData
	);

typedef LSTATUS(WINAPI* PREGOPENKEYEXW)(
	HKEY hKey,
	LPCWSTR lpSubKey,
	DWORD ulOptions,
	REGSAM samDesired,
	PHKEY phkResult
	);

typedef LSTATUS(WINAPI* PREGOPENKEYEXA)(
	HKEY hKey,
	LPCSTR lpSubKey,
	DWORD ulOptions,
	REGSAM samDesired,
	PHKEY phkResult
	);

typedef LSTATUS(WINAPI* PREGOPENKEYW)(
	HKEY hKey,
	LPCWSTR lpSubKey,
	PHKEY phkResult
	);

typedef LSTATUS(WINAPI* PREGOPENKEYA)(
	HKEY hKey,
	LPCSTR lpSubKey,
	PHKEY phkResult
	);


// Function pointer type for RegQueryMultipleValues
typedef LSTATUS(WINAPI* RegQueryMultipleValuesType)(
	HKEY     hKey,
	PVALENT  val_list,
	DWORD    num_vals,
	LPWSTR   lpValueBuf,
	LPDWORD  ldwTotsize
	);

LONG WINAPI proxy_RegQueryValueExW(
	HKEY hKey,
	LPCWSTR lpValueName,
	LPVOID lpReserved,
	LPDWORD lpType,
	LPBYTE lpData,
	LPDWORD lpcbData
);

LONG WINAPI proxy_RegQueryValueExA(
	HKEY hKey,
	LPCSTR lpValueName,
	LPVOID lpReserved,
	LPDWORD lpType,
	LPBYTE lpData,
	LPDWORD lpcbData
);

LSTATUS WINAPI proxy_RegOpenKeyExW(
	HKEY hKey,
	LPCWSTR lpSubKey,
	DWORD ulOptions,
	REGSAM samDesired,
	PHKEY phkResult
);

LSTATUS WINAPI proxy_RegOpenKeyExA(
	HKEY hKey,
	LPCSTR lpSubKey,
	DWORD ulOptions,
	REGSAM samDesired,
	PHKEY phkResult
);

LSTATUS WINAPI proxy_RegOpenKeyA(
	HKEY hKey,
	LPCSTR lpSubKey,
	PHKEY phkResult
);

LSTATUS WINAPI proxy_RegQueryMultipleValues(
	HKEY     hKey,
	PVALENT  val_list,
	DWORD    num_vals,
	LPWSTR   lpValueBuf,
	LPDWORD  ldwTotsize
);

LSTATUS WINAPI proxy_RegOpenKeyW(
	HKEY hKey,
	LPCWSTR lpSubKey,
	PHKEY phkResult
);

LSTATUS WINAPI proxy_RegGetValueA(
	HKEY    hkey,
	LPCSTR lpSubKey,
	LPCSTR lpValue,
	DWORD   dwFlags,
	LPDWORD pdwType,
	PVOID   pvData,
	LPDWORD pcbData
);

LSTATUS WINAPI proxy_RegGetValueW(
	HKEY    hkey,
	LPCWSTR lpSubKey,
	LPCWSTR lpValue,
	DWORD   dwFlags,
	LPDWORD pdwType,
	PVOID   pvData,
	LPDWORD pcbData
);

extern RegGetValueW_Ptr OriginalRegGetValueW;
extern RegGetValueA_Ptr OriginalRegGetValueA;
extern RegQueryMultipleValuesType OriginalRegQueryMultipleValues;
extern PREGOPENKEYEXW OriginalRegOpenKeyExW;
extern PREGOPENKEYEXA OriginalRegOpenKeyExA;
extern PREGOPENKEYW OriginalRegOpenKeyW;
extern PREGOPENKEYA OriginalRegOpenKeyA;
extern RegQueryValueExW_Ptr originalRegQueryValueExW;
extern RegQueryValueExA_Ptr originalRegQueryValueExA;
