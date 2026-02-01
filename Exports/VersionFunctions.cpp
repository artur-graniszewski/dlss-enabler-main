#include "VersionFunctions.h"

HMODULE hOriginalVersionDll = nullptr;

FARPROC GetFileVersionInfoAFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoA");
}

FARPROC GetFileVersionInfoByHandleFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoByHandle");
}

FARPROC GetFileVersionInfoExAFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoExA");
}

FARPROC GetFileVersionInfoExWFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoExW");
}

FARPROC GetFileVersionInfoSizeAFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoSizeA");
}

FARPROC GetFileVersionInfoSizeExAFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoSizeExA");
}

FARPROC GetFileVersionInfoSizeExWFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoSizeExW");
}

FARPROC GetFileVersionInfoSizeWFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoSizeW");
}

FARPROC GetFileVersionInfoWFunc() {
    return GetProcAddress(hOriginalVersionDll, "GetFileVersionInfoW");
}

FARPROC VerFindFileAFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerFindFileA");
}

FARPROC VerFindFileWFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerFindFileW");
}

FARPROC VerInstallFileAFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerInstallFileA");
}

FARPROC VerInstallFileWFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerInstallFileW");
}

FARPROC VerLanguageNameAFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerLanguageNameA");
}

FARPROC VerLanguageNameWFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerLanguageNameW");
}

FARPROC VerQueryValueAFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerQueryValueA");
}

FARPROC VerQueryValueWFunc() {
    return GetProcAddress(hOriginalVersionDll, "VerQueryValueW");
}

// Implement other functions similarly...

void VersionFunctions::Initialize() {
    hOriginalVersionDll = LoadLibrary(L"version.dll");
    if (!hOriginalVersionDll) {
        // Handle the error (e.g., log it, throw an exception, etc.)
    }
}

void VersionFunctions::Release() {
    if (hOriginalVersionDll) {
        FreeLibrary(hOriginalVersionDll);
        hOriginalVersionDll = nullptr;
    }
}