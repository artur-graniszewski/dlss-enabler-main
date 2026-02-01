#pragma once
#include "DetourApi.h"
#include "../Core/Context.h"

enum class HookPhase { EARLY, CORE, LATE, ON_DEMAND };

struct IHook {
    virtual ~IHook() = default;
    virtual const std::wstring Name() const = 0;
    virtual HookPhase Phase() const = 0;
    virtual int Priority() const { return 0; } // higher = earlier execution
    // Check conditions (eg. mod presence, platform)
    virtual bool CanInstall(Context& ctx, IDetourApi& api) { return true; }
    // perform attach operation (multiple attach ops allowed)
    virtual bool Install(Context& ctx, IDetourApi& api) = 0;
    // optional detach (np. for tests / clean-up)
    virtual void Uninstall(Context& ctx, IDetourApi& api) {}
};