#pragma once
#include "Hook.h"
#include "Common.h"
#include "SsrtgiListener.h"


struct HookSsrtgi : IHook {
    const std::wstring Name() const override { return L"SSRTGI hooks"; }
    HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
    int         Priority() const override { return 5000; }

    bool CanInstall(Context& ctx, IDetourApi& api) override {
        return ctx.isOptiscalerInitialized;
    }

    bool Install(Context& ctx, IDetourApi& api) override {
        static bool done = false;
        if (done) {
            return false;
        }

        done = true;
        SsrtgiListener::SetCsoDirectory(L"C:\\Users\\Admin\\Desktop\\Stary pulpit\\DLSS\\DLSS Enabler - master \u2014 2.90.810.0 \u2014 kopia\\x64\\Debug\\");
        SsrtgiListener::SetHotReloadEnabled(true);
        SsrtgiListener::Register();
        return true;
    }
};
