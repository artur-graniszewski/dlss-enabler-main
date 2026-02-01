#pragma once
#include "Hook.h"
#include "DetourTxn.h"
#include "RegistryProxy.h"

RegGetValueW_Ptr OriginalRegGetValueW;
RegGetValueA_Ptr OriginalRegGetValueA;
RegQueryMultipleValuesType OriginalRegQueryMultipleValues;
PREGOPENKEYEXW OriginalRegOpenKeyExW;
PREGOPENKEYEXA OriginalRegOpenKeyExA;
PREGOPENKEYW OriginalRegOpenKeyW;
PREGOPENKEYA OriginalRegOpenKeyA;
RegQueryValueExW_Ptr originalRegQueryValueExW;
RegQueryValueExA_Ptr originalRegQueryValueExA;

struct RegistryHook : IHook {
    const std::wstring Name() const override { return L"Registry hooks"; }
    HookPhase   Phase() const override { return HookPhase::CORE; }
    int         Priority() const override { return 500000; }

    bool CanInstall(Context& ctx, IDetourApi& api) override {
        // ctx.enableRegistrySpoofing || !ctx.isRunningUnderWindow
        return api.GetModHandleW(L"advapi32.dll") != nullptr;
    }

    bool Install(Context& ctx, IDetourApi& api) override {
        auto advapi32 = api.GetModHandleW(L"advapi32.dll");
        DetourTxn txn(api);
        originalRegQueryValueExW = (RegQueryValueExW_Ptr)api.GetProc(advapi32, "RegQueryValueExW");
        if (!txn.attach((void**)&originalRegQueryValueExW, (void*)&proxy_RegQueryValueExW, "RegQueryValueExW")) return false;


        originalRegQueryValueExA = (RegQueryValueExA_Ptr)api.GetProc(advapi32, "RegQueryValueExA");
        if (!txn.attach((void**)&originalRegQueryValueExA, (void*)&proxy_RegQueryValueExA, "RegQueryValueExA")) return false;


        OriginalRegOpenKeyExW = (PREGOPENKEYEXW)api.GetProc(advapi32, "RegOpenKeyExW");
        if (!txn.attach((void**)&OriginalRegOpenKeyExW, (void*)&proxy_RegOpenKeyExW, "RegOpenKeyExW")) return false;

        OriginalRegOpenKeyExA = (PREGOPENKEYEXA)api.GetProc(advapi32, "RegOpenKeyExA");
        if (!txn.attach((void**)&OriginalRegOpenKeyExA, (void*)&proxy_RegOpenKeyExA, "RegOpenKeyExA")) return false;

        if (true) {
            OriginalRegGetValueW = (RegGetValueW_Ptr)api.GetProc(advapi32, "RegGetValueW");
            if (!txn.attach((void**)&OriginalRegGetValueW, (void*)&proxy_RegGetValueW, "RegGetValueW")) return false;

            OriginalRegGetValueA = (RegGetValueA_Ptr)api.GetProc(advapi32, "RegGetValueA");
            if (!txn.attach((void**)&OriginalRegGetValueA, (void*)&proxy_RegGetValueA, "RegGetValueA")) return false;

            OriginalRegOpenKeyA = (PREGOPENKEYA)api.GetProc(advapi32, "RegOpenKeyA");
            if (!txn.attach((void**)&OriginalRegOpenKeyA, (void*)&proxy_RegOpenKeyA, "RegOpenKeyA")) return false;

            OriginalRegOpenKeyW = (PREGOPENKEYW)api.GetProc(advapi32, "RegOpenKeyW");
            if (!txn.attach((void**)&OriginalRegOpenKeyW, (void*)&proxy_RegOpenKeyW, "RegOpenKeyW")) return false;

            OriginalRegQueryMultipleValues = (RegQueryMultipleValuesType)api.GetProc(advapi32, "RegQueryMultipleValuesW");
            if (!txn.attach((void**)&OriginalRegQueryMultipleValues, (void*)&proxy_RegQueryMultipleValues, "RegQueryMultipleValuesW")) return false;
        }

        return txn.commit();
    }
};