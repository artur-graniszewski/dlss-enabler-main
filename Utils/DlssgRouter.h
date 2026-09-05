#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "../Includes/dlss/nvsdk_ngx.h"
#include "INgxBackend.h"      // INgxLogger
#include "RoutePolicy.h"

namespace DLSSG
{
    // Lightweight per-handle bookkeeping.
    // The native handle is what we hand back to the game (it is the primary
    // key in the registry). The FSR3 shadow handle hangs off it.
    struct RouterEntry
    {
        NVSDK_NGX_Handle* nativeHandle = nullptr;
        NVSDK_NGX_Handle* fsr3Handle = nullptr;
        bool nativeOk = false;
        bool fsr3Ok = false;
    };

    // Hybrid MFG router. Lives next to dlssgModule and ngxFrontend as a global.
    //
    // This class deliberately does NOT call into NgxFrontend or DlssgProxy
    // itself — it only stores state and runs the policy. The actual fanout
    // (calling both backends in CreateFeature, picking one in EvaluateFeature,
    // releasing both in ReleaseFeature) lives in NgxFrontend's existing
    // CreateD3D12 / EvaluateD3D12 / ReleaseD3D12 functions, in the same place
    // where the current "DLSSG REDIRECT" if-branches are. NgxFrontend already
    // has the resolver wiring it needs (NGX_RESOLVE_PROXY_ONCE) and a pointer
    // to dlssgModule, so all that's missing is the policy decision.
    //
    // The router exposes:
    //   - Decide()           : ask the policy who should run this subframe.
    //   - RememberPair()     : called by NgxFrontend after a successful create.
    //   - LookupShadow()     : retrieve the FSR3 handle for a game-visible native handle.
    //   - ForgetPair()       : called by NgxFrontend on release.
    //   - NotifyHealth()     : NgxFrontend tells us when native eval failed.
    class DlssgRouter
    {
    public:
        DlssgRouter(INgxLogger& logger, std::unique_ptr<IRoutePolicy> policy)
            : logger(logger), policy(std::move(policy)) {
        }

        bool IsActive() const { return enabled; }
        void Disable() { enabled = false; }

        // Decide which backend should run the next subframe.
        // Caller must read DLSSG.MultiFrameIndex / MultiFrameCount from
        // parameters and pass them in here, plus whether the FSR3 shadow
        // exists for this handle (so we can fall back if not).
        RouteTarget Decide(uint32_t subframeIdx,
            uint32_t subframeCount,
            bool     dlssgHwAvailable,
            bool     fsr3ShadowExists,
            bool     nativeHandleExists);

        // Record a (native, fsr3) pair after both backends successfully created.
        // Either pointer may be nullptr — record whatever we got, the eval path
        // will fall back to the surviving one.
        void RememberPair(NVSDK_NGX_Handle* nativeHandle,
            NVSDK_NGX_Handle* fsr3Handle,
            bool nativeOk,
            bool fsr3Ok);

        // Returns the FSR3 shadow handle for a given native (game-visible) handle,
        // or nullptr if we don't know about it.
        NVSDK_NGX_Handle* LookupShadow(const NVSDK_NGX_Handle* nativeHandle);

        // Returns the entry for a given native handle, or empty entry if unknown.
        RouterEntry LookupEntry(const NVSDK_NGX_Handle* nativeHandle);

        // Forget a pair on release. Returns the entry that was forgotten so the
        // caller can release whatever's in it.
        RouterEntry ForgetPair(NVSDK_NGX_Handle* nativeHandle);

        // NgxFrontend tells us how the last native eval went so we can keep the
        // health flag honest across calls.
        void NotifyNativeEvalResult(NVSDK_NGX_Result result);

        // Public so NgxFrontend can log it for diagnostics if it wants.
        bool IsDlssgHealthy() const { return dlssgHealthy.load(std::memory_order_relaxed); }

    private:
        INgxLogger& logger;
        std::unique_ptr<IRoutePolicy> policy;

        std::mutex mapMutex;
        // Keyed on the NATIVE handle (the one the game holds).
        std::unordered_map<const NVSDK_NGX_Handle*, RouterEntry> registry;

        std::atomic<uint64_t> globalFrameCounter{ 0 };
        std::atomic<bool>     dlssgHealthy{ true };
        bool enabled = true;
    };
}