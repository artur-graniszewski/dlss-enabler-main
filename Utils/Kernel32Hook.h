#pragma once
#include "Hook.h"
#include "DetourTxn.h"
#include "Kernel32Proxy.h"

GetProcAddress_t OriginalGetProcAddress = GetProcAddress;
GetProcAddress_t _OriginalGetProcAddress = GetProcAddress;
LoadLibraryExW_t OriginalLoadLibraryExW = LoadLibraryExW;
LoadLibraryExW_t _OriginalLoadLibraryExW = LoadLibraryExW;
LoadLibraryW_t OriginalLoadLibraryW = LoadLibraryW;
LoadLibraryW_t _OriginalLoadLibraryW = LoadLibraryW;
PFreeLibrary pOriginalFreeLibrary = nullptr;
PFreeLibrary _pOriginalFreeLibrary = nullptr;

struct HookKernel32 : IHook {
    const std::wstring Name() const override { return L"Kernel32 hooks"; }
    HookPhase   Phase() const override { return HookPhase::EARLY; }
    int         Priority() const override { return 1000; }

    bool CanInstall(Context& ctx, IDetourApi& api) override {
        return api.GetModHandleW(L"kernel32.dll") != nullptr;
    }

    bool Install(Context& ctx, IDetourApi& api) override {
        DetourTxn txn(api);
         
        auto kernel32 = api.GetModHandleW(L"kernel32.dll");
        OriginalGetProcAddress = (GetProcAddress_t)api.GetProc(kernel32, "GetProcAddress");
        if (OriginalGetProcAddress) {
            if (!txn.attach((void**)&OriginalGetProcAddress, (void*)&DetourGetProcAddress, "GetProcAddress")) return false;
        }

        OriginalLoadLibraryExW = (LoadLibraryExW_t)api.GetProc(kernel32, "LoadLibraryExW");
        if (OriginalLoadLibraryExW) {
            if (!txn.attach((void**)&OriginalLoadLibraryExW, (void*)&DetourLoadLibraryExW, "LoadLibraryExW")) return false;
        }

        OriginalLoadLibraryW = (LoadLibraryW_t)api.GetProc(kernel32, "LoadLibraryW");
        if (OriginalLoadLibraryW) {
            if (!txn.attach((void**)&OriginalLoadLibraryW, (void*)&DetourLoadLibraryW, "LoadLibraryW")) return false;
        }


        auto kernelBase = api.GetModHandleW(L"kernelbase.dll");

        _OriginalLoadLibraryExW = (LoadLibraryExW_t)api.GetProc(kernelBase, "LoadLibraryExW");
        if (_OriginalLoadLibraryExW) {
            if (!txn.attach((void**)&_OriginalLoadLibraryExW, (void*)&_DetourLoadLibraryExW, "LoadLibraryExW")) return false;
        }

        _OriginalLoadLibraryW = (LoadLibraryW_t)api.GetProc(kernelBase, "LoadLibraryW");
        if (_OriginalLoadLibraryW) {
            if (!txn.attach((void**)&_OriginalLoadLibraryW, (void*)&_DetourLoadLibraryW, "LoadLibraryW")) return false;
        }


        _OriginalGetProcAddress = (GetProcAddress_t)api.GetProc(kernelBase, "GetProcAddress");
        if (_OriginalGetProcAddress) {
            if (!txn.attach((void**)&_OriginalGetProcAddress, (void*)&_DetourGetProcAddress, "GetProcAddress")) return false;
        }
        /*
        if (ctx.enableUnloadProtection) {
            pOriginalFreeLibrary = (PFreeLibrary)api.GetProc(kernel32, "FreeLibrary");
            if (pOriginalFreeLibrary) {
                if (!txn.attach((void**)&pOriginalFreeLibrary, (void*)&DetouredFreeLibrary, "FreeLibrary")) return false;
            }
        }*/

        auto result = txn.commit();

        if (result) {
            LOG_INFO(L"[KERNEL] Hooks applied successfully");
        }
        else {
            LOG_ERROR(L"[KERNEL] Failed to activate hooks");
        }
        return result;
    }
};