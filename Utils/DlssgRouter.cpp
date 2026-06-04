#include "DlssgRouter.h"
#include "../Core/Context.h"

#include <string>

namespace DLSSG
{
    static constexpr wchar_t kModule[] = L"DlssgRouter";

    RouteTarget DlssgRouter::Decide(uint32_t subframeIdx,
        uint32_t subframeCount,
        bool     dlssgHwAvailable,
        bool     fsr3ShadowExists,
        bool     nativeHandleExists)
    {
        const RouteDecisionInput in{
            subframeIdx,
            subframeCount,
            globalFrameCounter.fetch_add(1, std::memory_order_relaxed),
            dlssgHwAvailable,
            nativeHandleExists && dlssgHealthy.load(std::memory_order_relaxed)
        };

        RouteTarget target = policy->Decide(in);

        // Capability fallback: if we picked a backend that has no live handle
        // for this entry, switch to the other one. This is the layer that
        // makes "DLSSG failed to create at startup -> route everything to FSR3"
        // work transparently without the policy needing to know.
        if (target == RouteTarget::NativeDlssg && !nativeHandleExists) {
            target = RouteTarget::Fsr3Mfg;
        }
        if (target == RouteTarget::Fsr3Mfg && !fsr3ShadowExists) {
            target = RouteTarget::NativeDlssg;
        }
        return target;
    }

    void DlssgRouter::RememberPair(NVSDK_NGX_Handle* nativeHandle,
        NVSDK_NGX_Handle* fsr3Handle,
        bool nativeOk,
        bool fsr3Ok)
    {
        // Pick the primary key. Native is preferred (it's what the game sees).
        // If native failed, FSR3 becomes the primary so the game-visible handle
        // is still in the map.
        NVSDK_NGX_Handle* primary = nativeOk ? nativeHandle : fsr3Handle;
        if (!primary) {
            return;
        }

        RouterEntry entry;
        entry.nativeHandle = nativeHandle;
        entry.fsr3Handle = fsr3Handle;
        entry.nativeOk = nativeOk;
        entry.fsr3Ok = fsr3Ok;

        // Reset health on every fresh Create. Without this, a stale "unhealthy"
        // flag from a previous session would prevent the new session from ever
        // routing to native DLSSG, even though the new native handle is fresh.
        if (nativeOk) {
            dlssgHealthy.store(true, std::memory_order_relaxed);
        }

        std::lock_guard<std::mutex> lk(mapMutex);
        registry[primary] = entry;
    }

    NVSDK_NGX_Handle* DlssgRouter::LookupShadow(const NVSDK_NGX_Handle* nativeHandle)
    {
        std::lock_guard<std::mutex> lk(mapMutex);
        auto it = registry.find(nativeHandle);
        if (it == registry.end()) {
            return nullptr;
        }
        return it->second.fsr3Ok ? it->second.fsr3Handle : nullptr;
    }

    RouterEntry DlssgRouter::LookupEntry(const NVSDK_NGX_Handle* nativeHandle)
    {
        std::lock_guard<std::mutex> lk(mapMutex);
        auto it = registry.find(nativeHandle);
        if (it == registry.end()) {
            return RouterEntry{};
        }
        return it->second;
    }

    RouterEntry DlssgRouter::ForgetPair(NVSDK_NGX_Handle* nativeHandle)
    {
        std::lock_guard<std::mutex> lk(mapMutex);
        auto it = registry.find(nativeHandle);
        if (it == registry.end()) {
            return RouterEntry{};
        }
        RouterEntry entry = it->second;
        registry.erase(it);
        return entry;
    }

    void DlssgRouter::NotifyNativeEvalResult(NVSDK_NGX_Result result)
    {
        if (NVSDK_NGX_SUCCEED(result)) {
            dlssgHealthy.store(true, std::memory_order_relaxed);
        }
        else {
            dlssgHealthy.store(false, std::memory_order_relaxed);
            logger.Warning(L"[" + std::wstring(kModule) + L"] native DLSSG eval failed, marking unhealthy");
        }
    }
}