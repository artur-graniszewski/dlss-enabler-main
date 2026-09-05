#pragma once
#include "HookRegistry.h"
#include "DetourApi.h"
#include <vector>

class HookManager {
public:
    HookManager(Context& ctx, IDetourApi& api) : ctx_(ctx), api_(api) {}

    bool InitializeAll() {
        api_.RestoreAfterWith();
        return runPhase(HookPhase::EARLY)
            && runPhase(HookPhase::CORE)
            && runPhase(HookPhase::LATE);
    }

    void TryInstallOnDemand() { runPhase(HookPhase::ON_DEMAND); }

    void UninstallAll() {
        runUninstallPhase(HookPhase::ON_DEMAND);
        runUninstallPhase(HookPhase::LATE);
        runUninstallPhase(HookPhase::CORE);
        runUninstallPhase(HookPhase::EARLY);
    }

private:
    void runUninstallPhase(HookPhase ph) {
        auto hooks = HookRegistry::Instance().GetSorted(ph);
        // odwrotna kolejnoœæ w ramach fazy: ni¿szy priority = póŸniej instalowane = wczeœniej odinstalowywane
        std::reverse(hooks.begin(), hooks.end());
        for (auto* h : hooks) {
            h->Uninstall(ctx_, api_);
        }
    }

    bool runPhase(HookPhase ph) {
        auto hooks = HookRegistry::Instance().GetSorted(ph);
        for (auto* h : hooks) {
            if (!h->CanInstall(ctx_, api_)) continue;
            if (!h->Install(ctx_, api_)) {
                // log error 
            }
        }
        return true;
    }

    Context& ctx_;
    IDetourApi& api_;
};