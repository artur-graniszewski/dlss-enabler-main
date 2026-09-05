#pragma once
#include "Hook.h"
#include "DetourTxn.h"
#include "HagsProxy.h"

PFN_D3DKMTQueryAdapterInfo gOrigQueryAdapterInfo = nullptr;
PFN_D3DKMTEnumAdapters2    gOrigEnumAdapters2 = nullptr;

struct HookGdi32Hags : IHook {
    const std::wstring Name() const override { return L"GDI32 HAGS hooks"; }
    HookPhase   Phase() const override { return HookPhase::CORE; }
    int         Priority() const override { return 50; }

    bool CanInstall(Context& ctx, IDetourApi& api) override {
        return api.GetModHandleW(L"gdi32.dll") != nullptr;
    }

    bool Install(Context& ctx, IDetourApi& api) override {
        auto gdi32 = api.GetModHandleW(L"gdi32.dll");
        auto pQA = reinterpret_cast<void**>(&gOrigQueryAdapterInfo);
        gOrigQueryAdapterInfo = (PFN_D3DKMTQueryAdapterInfo)api.GetProc(gdi32, "D3DKMTQueryAdapterInfo");
        DetourTxn txn(api);
        if (!txn.attach((void**)pQA, (void*)&MyD3DKMTQueryAdapterInfo, "D3DKMTQueryAdapterInfo")) return false;
        if (!ctx.isRunningUnderWindows) {
            gOrigEnumAdapters2 = (PFN_D3DKMTEnumAdapters2)api.GetProc(gdi32, "D3DKMTEnumAdapters2");
            if (gOrigEnumAdapters2) {
                if (!txn.attach((void**)&gOrigEnumAdapters2, (void*)&MyD3DKMTEnumAdapters2, "D3DKMTEnumAdapters2")) return false;
            }
        }
        return txn.commit();
    }

    void Uninstall(Context& ctx, IDetourApi& api) override {
        DetourTxn txn(api);
        if (gOrigEnumAdapters2) {
            txn.detach((void**)&gOrigEnumAdapters2, (void*)&MyD3DKMTEnumAdapters2, "D3DKMTEnumAdapters2");
        }
        if (gOrigQueryAdapterInfo) {
            txn.detach((void**)&gOrigQueryAdapterInfo, (void*)&MyD3DKMTQueryAdapterInfo, "D3DKMTQueryAdapterInfo");
        }
        if (txn.commit()) {
            gOrigEnumAdapters2 = nullptr;
            gOrigQueryAdapterInfo = nullptr;
        }
    }
};