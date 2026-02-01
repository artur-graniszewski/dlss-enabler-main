#pragma once
#include "Hook.h"
#include "Common.h"
#include "Console.h"
#include "FakeNVAPI.h"

namespace NVAPI
{
    struct HookFakeNVAPI : IHook {
        const std::wstring Name() const override { return L"NVAPI hooks"; }
        HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
        int         Priority() const override { return 100; }

        bool CanInstall(Context& ctx, IDetourApi& api) override {
            return true;
        }

        bool Install(Context& ctx, IDetourApi& api) override {
            static bool isFakeLoaded = false;
            if (isFakeLoaded) {
                return false;
            }

            isFakeLoaded = true;
            
            FakeNvapiConfig cfg{};
            cfg.hideXellLibrary = ctx.nvapi.isXellEnabled == false;
            if (!FakeNvapi_Init(Common::GetModuleHandle(), &cfg)) {}

            Console::ResetLogging(); // needs to be here to reset log format after fakeNVAPI overriding it

            LOG_INFO(L"[NVAPI] FakeNVAPI initialized");
            return true;
        }
    };
}