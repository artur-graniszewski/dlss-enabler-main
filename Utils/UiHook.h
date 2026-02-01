#pragma once
#include "Hook.h"
#include "Common.h"
#include "UxHook.h"
#include "SettingsPersistence.h"


struct HookUi : IHook {
    const std::wstring Name() const override { return L"UI hooks"; }
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
        UxInit();
        SettingsPersistence::Init();
        SettingsPersistence::Load();
        return true;
    }
};
