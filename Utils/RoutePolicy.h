#pragma once

#include <cstdint>
#include <memory>
#include "../Includes/dlss/nvsdk_ngx.h"

namespace DLSSG
{
    // Which backend should evaluate the current subframe.
    enum class RouteTarget : uint8_t
    {
        Fsr3Mfg,
        NativeDlssg
    };

    // Inputs the policy needs to make a per-subframe routing decision.
    struct RouteDecisionInput
    {
        uint32_t subframeIndex;   // DLSSG.MultiFrameIndex (0..count-1)
        uint32_t subframeCount;   // DLSSG.MultiFrameCount (1=x2, 2=x3, 3=x4, 5=x6)
        uint64_t globalFrameId;   // monotonic counter for hysteresis / debug
        bool     dlssgHwAvailable;
        bool     dlssgHealthy;    // false after a recent native eval failure
    };

    // Policy interface — easy to swap for ML / heuristic experiments later.
    struct IRoutePolicy
    {
        virtual ~IRoutePolicy() = default;
        virtual RouteTarget Decide(const RouteDecisionInput& in) = 0;
        virtual void NotifyResult(const RouteDecisionInput& in,
            RouteTarget chosen,
            NVSDK_NGX_Result result) = 0;
    };

    // Default policy:
    //   Native DLSSG only fires for the EXACT middle subframe of an MFG batch.
    //   A batch has an exact middle iff the number of interpolated subframes
    //   is odd; for even counts there is no single middle slot and we let
    //   FSR3 handle the entire batch.
    //
    //   IMPORTANT: Streamline uses 1-BASED subframe indexing in NGX parameter
    //   block. Confirmed empirically with Streamline 2.10.3 + nvngx_dlssg
    //   from driver 595.97 (probe log of DLSSG.MultiFrameIndex/MultiFrameCount):
    //
    //     numFramesToGenerate=1 (x2): count=1, idx in {1}
    //     numFramesToGenerate=2 (x3): count=2, idx in {1, 2}
    //     numFramesToGenerate=3 (x4): count=3, idx in {1, 2, 3}
    //     numFramesToGenerate=4 (x5): count=4, idx in {1, 2, 3, 4}
    //     numFramesToGenerate=5 (x6): count=5, idx in {1, 2, 3, 4, 5}
    //
    //   So count = number of interpolated frames, and idx ranges 1..count.
    //   The middle of an odd-sized batch is at idx = (count + 1) / 2:
    //
    //     x2 -> count=1 (odd)  -> idx 1 -> DLSSG
    //     x3 -> count=2 (even) -> all FSR3
    //     x4 -> count=3 (odd)  -> idx 2 -> DLSSG, idx 1/3 FSR3
    //     x5 -> count=4 (even) -> all FSR3
    //     x6 -> count=5 (odd)  -> idx 3 -> DLSSG, idx 1/2/4/5 FSR3
    //
    //   Earlier versions of this policy assumed 0-based indexing and never
    //   matched. The 1-based formula (count + 1) / 2 gives:
    //     count=1 -> 1, count=3 -> 2, count=5 -> 3 — all correct.
    class MidpointDlssgPolicy : public IRoutePolicy
    {
    public:
        RouteTarget Decide(const RouteDecisionInput& in) override
        {
            if (!in.dlssgHwAvailable || !in.dlssgHealthy) {
                return RouteTarget::Fsr3Mfg;
            }
            if (in.subframeCount == 0) {
                // Defensive: should never happen because the caller defaults
                // to 1 when parameters->Get fails, but if it ever does we
                // route to FSR3 as the safe choice.
                return RouteTarget::Fsr3Mfg;
            }

            // Even number of subframes -> no exact middle -> all FSR3.
            if ((in.subframeCount & 1u) == 0u) {
                return RouteTarget::Fsr3Mfg;
            }

            // Odd number of subframes -> middle is at 1-based index (count + 1) / 2.
            const uint32_t midpointIdx = (in.subframeCount + 1u) / 2u;
            return (in.subframeIndex == midpointIdx) ? RouteTarget::NativeDlssg
                : RouteTarget::Fsr3Mfg;
        }

        void NotifyResult(const RouteDecisionInput&,
            RouteTarget,
            NVSDK_NGX_Result) override
        {
            // No-op for the default policy. Subclasses can track health,
            // implement hysteresis, log decisions, etc.
        }
    };
}