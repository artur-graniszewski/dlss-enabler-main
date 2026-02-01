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

private:
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