#include "Core/Core.h"
#include "Exports/Exports.h"
#include <Wtypes.h>
#include "Utils/Common.h"
#include "Utils/Preloader.h"
#include <unordered_set>
#include "../pch.h"

bool isInitialized = false;
HMODULE hSelf;

static bool IsConsoleApplication()
{
    if (GetConsoleWindow() != NULL) {
        return true;
    }

    typedef std::unordered_set<std::wstring> StringSet;
    StringSet blockedApps;
    auto filePath = Common::GetProcessFilePath();
    std::wstring processName = filePath.filename().wstring();

    blockedApps.insert(L"crashpad_handler.exe");
    blockedApps.insert(L"CrashReport.exe");
    blockedApps.insert(L"CrashReporter.exe");
    blockedApps.insert(L"crs-handler.exe");
    blockedApps.insert(L"UnityCrashHandler64.exe");
    blockedApps.insert(L"idTechLauncher.exe");

    if (blockedApps.find(processName) != blockedApps.end()) {
        return true;
    }

    return false;
}

static DWORD WINAPI HardKillThread(LPVOID param)
{
    Sleep(reinterpret_cast<DWORD>(param));
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}

static void HardKillWatchdog(DWORD delayMs)
{
    HANDLE h = CreateThread(nullptr, 0, HardKillThread,
        reinterpret_cast<LPVOID>(delayMs), 0, nullptr);
    if (h) CloseHandle(h);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reasonForCall, LPVOID lpReserved)
{
    hSelf = hModule;
    dllModule = hModule;
    switch (reasonForCall) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        Exports::ConfigureProxy(hModule);
        if (!isInitialized) {
            if (!IsConsoleApplication()) {
                isInitialized = true;
                Common::Initialize();
                Core::Initialize(hModule);

                // Pin this DLL as hooks can't be removed once they're enabled.
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, nullptr, &hModule);
                Preloader::OnModuleLoad();


                LOG_DEBUG(L"[MAIN] DLL Loaded");

            }
        }

        break;
    case DLL_PROCESS_DETACH:
        if (!isInitialized) {
            return TRUE;
        }

        HardKillWatchdog(4000);
        Core::Finish(hModule);
        TerminateProcess(GetCurrentProcess(), 0);
        break;
    }
    return TRUE;
}
