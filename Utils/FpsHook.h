#pragma once
#include "Hook.h"
#include "Common.h"
#include "FpsMonitor.h"
#include "OverdriveController.h"

namespace FpsMonitor
{
    struct HookFpsMonitor : IHook {
        const std::wstring Name() const override { return L"DXGI hooks"; }
        HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
        int         Priority() const override { return 50; }

        bool CanInstall(Context& ctx, IDetourApi& api) override {
            return api.GetModHandleW(L"dxgi.dll");
        }

        bool Install(Context&, IDetourApi& api)
        {
            static bool isInstalled;
            if (isInstalled) {
                return true;
            }

            isInstalled = true;
            LOG_INFO(L"[DXGI] Installing FPS Monitor");
            FpsMonitor::Init();
            OverdriveController::Init(); // @todo: refactor me!

            return true;
        }
    };
}