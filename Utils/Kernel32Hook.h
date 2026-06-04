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

// Tracks whether a full Install() completed successfully.
// Trampoline pointers above are initialized to non-null default values
// (e.g. OriginalGetProcAddress = GetProcAddress), so we cannot use a
// pointer-null check to distinguish "installed" from "never installed".
// Set to true only after the Install transaction commits.
static bool g_kernel32HooksInstalled = false;

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
            g_kernel32HooksInstalled = true;
            LOG_INFO(L"[KERNEL] Hooks applied successfully");
        }
        else {
            LOG_ERROR(L"[KERNEL] Failed to activate hooks");
        }
        return result;
    }

    void Uninstall(Context& ctx, IDetourApi& api) override {
        // Guard: if Install never committed, there is nothing installed.
        // This covers both the "Install was never called" case and the
        // "Install aborted mid-transaction" case (partial non-installed state).
        if (!g_kernel32HooksInstalled) {
            return;
        }

        LOG_INFO(L"[KERNEL] Unhooking kernel32/kernelbase");

        DetourTxn txn(api);
        bool ok = true;

        // Detach in reverse order of Install. Each detach is guarded by the
        // corresponding trampoline being non-null, mirroring the Install-time
        // guards. Note: the trampoline pointers are NOT nulled afterwards -
        // other threads may still dereference them from inside detour handlers
        // that started before Uninstall. Post-detach they point at the
        // original kernel32/kernelbase functions (Detours restored the first
        // bytes), so invoking them is safe and simply calls the real import.

        // kernelbase.dll detours
        if (_OriginalGetProcAddress) {
            ok &= txn.detach((void**)&_OriginalGetProcAddress, (void*)&_DetourGetProcAddress, "GetProcAddress");
        }
        if (_OriginalLoadLibraryW) {
            ok &= txn.detach((void**)&_OriginalLoadLibraryW, (void*)&_DetourLoadLibraryW, "LoadLibraryW");
        }
        if (_OriginalLoadLibraryExW) {
            ok &= txn.detach((void**)&_OriginalLoadLibraryExW, (void*)&_DetourLoadLibraryExW, "LoadLibraryExW");
        }

        // kernel32.dll detours
        if (OriginalLoadLibraryW) {
            ok &= txn.detach((void**)&OriginalLoadLibraryW, (void*)&DetourLoadLibraryW, "LoadLibraryW");
        }
        if (OriginalLoadLibraryExW) {
            ok &= txn.detach((void**)&OriginalLoadLibraryExW, (void*)&DetourLoadLibraryExW, "LoadLibraryExW");
        }
        if (OriginalGetProcAddress) {
            ok &= txn.detach((void**)&OriginalGetProcAddress, (void*)&DetourGetProcAddress, "GetProcAddress");
        }

        if (!ok || !txn.commit()) {
            LOG_ERROR(L"[KERNEL] Failed to detach hooks");
            // Don't clear the flag - retaining "installed" state lets a retry
            // attempt another detach rather than silently no-op'ing.
            return;
        }

        g_kernel32HooksInstalled = false;
        LOG_INFO(L"[KERNEL] Hooks removed successfully");
    }
};