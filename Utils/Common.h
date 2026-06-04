#pragma once
#include <Wtypes.h>
#include <string>
#include <filesystem>
#include "Logger.h"

typedef HMODULE(WINAPI* LoadLibraryExW_t)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef HMODULE(WINAPI* LoadLibraryW_t)(LPCWSTR lpLibFileName);
typedef FARPROC(WINAPI* GetProcAddress_t)(HMODULE hModule, LPCSTR lpProcName);

bool endsWith(const std::wstring& fullString, const std::wstring& ending);
std::wstring ToWideString(const std::string& fullstring);

class Common 
{
    public:
        static double GetCurrentTimeMsec();
        static HMODULE GetModuleHandle();
        static std::wstring GetModuleDirectory();
        static std::filesystem::path GetModuleFilePath();
        static std::filesystem::path GetModuleFilePath(HMODULE);
        static std::filesystem::path GetProcessFilePath();
        static HMODULE LoadPlugin(LPCWSTR libFileName);
        static bool IsPluginPresent(LPCWSTR libFileName);
        static HMODULE LoadPlugin(std::wstring libFileName);
        static bool IsPluginPresent(std::wstring libFileName);
        static std::wstring GetPluginVersion(LPCWSTR libFileName);
        static std::wstring GetFileVersion(LPCWSTR dllPath);
        static std::wstring GetFileProductName(LPCWSTR dllPath);
        static void Initialize();
        static void SetLoader(LoadLibraryW_t loader);
        static void Info(const std::wstring& reason);
        static void Error(const std::wstring& reason, const bool shouldKill);
        static void KillProcess();
        static void SetProcAddress(GetProcAddress_t proc);
        static FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
        static std::wstring GetProcessFileName();
    private:
        static void DisplayErrorMessage(const std::wstring& reason, const bool shouldKill);
        static void CheckModConflict();
};