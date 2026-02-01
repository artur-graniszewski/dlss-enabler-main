#pragma once

#include <Windows.h>

#ifdef DLL_EXPORT
#define DLL_API __declspec(dllexport)
#else
#define DLL_API __declspec(dllimport)
#endif

extern "C" DLL_API FARPROC GetFileVersionInfoAFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoByHandleFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoExAFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoExWFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoSizeAFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoSizeExAFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoSizeExWFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoSizeWFunc();
extern "C" DLL_API FARPROC GetFileVersionInfoWFunc();
extern "C" DLL_API FARPROC VerFindFileAFunc();
extern "C" DLL_API FARPROC VerFindFileWFunc();
extern "C" DLL_API FARPROC VerInstallFileAFunc();
extern "C" DLL_API FARPROC VerInstallFileWFunc();
extern "C" DLL_API FARPROC VerLanguageNameAFunc();
extern "C" DLL_API FARPROC VerLanguageNameWFunc();
extern "C" DLL_API FARPROC VerQueryValueAFunc();
extern "C" DLL_API FARPROC VerQueryValueWFunc();

class DLL_API VersionFunctions {
public:
    static void Initialize();
    static void Release();
};
