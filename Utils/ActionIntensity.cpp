// =============================================================================
// ActionIntensity.cpp - Scene Action Intensity Detector for DFG MFG Mode Control
// =============================================================================

#include "ActionIntensity.h"
#include "NgxFeatureEvents.h"
#include "Common.h"
#include "../Core/Context.h"

#include <cmath>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <chrono>

namespace ActionIntensity
{
    // =========================================================================
    // Helpers
    // =========================================================================

    struct Vec3 { float x, y, z; };

    static float Length(Vec3 v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    static float Dot(Vec3 a, Vec3 b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static float Saturate(float x)
    {
        return (std::max)(0.0f, (std::min)(1.0f, x));
    }

    // =========================================================================
    // Internal state
    // =========================================================================

    // Previous frame camera data
    struct CameraFrame
    {
        Vec3  pos = { 0.f, 0.f, 0.f };
        Vec3  fwd = { 0.f, 0.f, 1.f };
        Vec3  up = { 0.f, 1.f, 0.f };
        float fov = 0.f;
        bool  valid = false;
        std::chrono::steady_clock::time_point timestamp = {};
    };

    static Config          g_Config;
    static CameraFrame     g_PrevFrame;
    static Snapshot        g_CurrentSnapshot = {};
    static std::mutex      g_SnapshotMutex;
    static std::atomic<bool> g_Initialized{ false };
    static uint64_t        g_FrameIndex = 0;
    static uint32_t        g_ConsecutiveValidFrames = 0;
    static bool            g_LoggedMissingData = false;   // Log missing params only once
    static bool            g_LoggedActivation = false;    // Log successful activation only once
    static bool            g_LoggedCallbackHit = false;   // Log first callback hit

    // Rotation direction continuity tracking (signed yaw/pitch from previous frame)
    static float           g_PrevYawRate = 0.f;
    static float           g_PrevPitchRate = 0.f;

    // =========================================================================
    // Parameter extraction from NVSDK_NGX_Parameter
    // =========================================================================
    // Returns false if the key is missing or the read fails.

    static bool TryGetFloat(NVSDK_NGX_Parameter* params, const char* key, float& outVal)
    {
        // NVSDK_NGX_Parameter::Get returns NVSDK_NGX_Result
        // On failure (key not set), it returns a non-success code.
        // We treat any failure as "not available".
        if (!params) return false;

        float tmp = 0.f;
        NVSDK_NGX_Result res = params->Get(key, &tmp);
        if (NVSDK_NGX_SUCCEED(res))
        {
            outVal = tmp;
            return true;
        }
        return false;
    }

    static bool TryGetUInt(NVSDK_NGX_Parameter* params, const char* key, unsigned int& outVal)
    {
        if (!params) return false;

        unsigned int tmp = 0;
        NVSDK_NGX_Result res = params->Get(key, &tmp);
        if (NVSDK_NGX_SUCCEED(res))
        {
            outVal = tmp;
            return true;
        }
        return false;
    }

    // =========================================================================
    // Core update - called from PreEvaluate DLSSG event
    // =========================================================================

    static void OnDlssgPreEvaluate(
        ID3D12GraphicsCommandList* /*cmdList*/,
        const NVSDK_NGX_Handle*    /*handle*/,
        NVSDK_NGX_Parameter* params,
        NVSDK_NGX_Feature          /*featureId*/)
    {
        if (!g_LoggedCallbackHit)
        {
            g_LoggedCallbackHit = true;
            LOG_WARNING(L"[ActionIntensity] PreEvaluate callback fired");
        }

        // -----------------------------------------------------------------
        // 1. Extract camera parameters from NGX
        // -----------------------------------------------------------------
        Vec3  pos = {};
        Vec3  fwd = {};
        Vec3  up = {};
        float fov = 0.f;

        bool hasPos = TryGetFloat(params, "DLSSG.CameraPosX", pos.x)
            && TryGetFloat(params, "DLSSG.CameraPosY", pos.y)
            && TryGetFloat(params, "DLSSG.CameraPosZ", pos.z);

        bool hasFwd = TryGetFloat(params, "DLSSG.CameraFwdX", fwd.x)
            && TryGetFloat(params, "DLSSG.CameraFwdY", fwd.y)
            && TryGetFloat(params, "DLSSG.CameraFwdZ", fwd.z);

        bool hasUp = TryGetFloat(params, "DLSSG.CameraUpX", up.x)
            && TryGetFloat(params, "DLSSG.CameraUpY", up.y)
            && TryGetFloat(params, "DLSSG.CameraUpZ", up.z);

        bool hasFov = TryGetFloat(params, "DLSSG.CameraFOV", fov);

        // Aspect ratio for pitch sensitivity scaling (vertical motion is more
        // disruptive on wider screens). Falls back to 16:9 if not available.
        float aspectRatio = 16.0f / 9.0f;
        TryGetFloat(params, "DLSSG.CameraAspectRatio", aspectRatio);
        if (aspectRatio < 0.5f || aspectRatio > 4.0f)
            aspectRatio = 16.0f / 9.0f;  // Sanity clamp

        // MFG multiplier: MultiFrameCount = 0 means 1 interpolated frame (= 2X output)
        // So actual multiplier = MultiFrameCount + 1
        unsigned int multiFrameCount = 0;
        TryGetUInt(params, "DLSSG.MultiFrameCount", multiFrameCount);
        int mfgMultiplier = static_cast<int>(multiFrameCount) + 1;

        // MFG subframe index: Streamline calls PreEvaluate once per subframe.
        // Camera data is identical across all subframes of the same game frame,
        // so we only compute on the first subframe (index 1).
        // Subsequent subframes would see deltaPos=0, deltaFwd=0 and dilute intensity.
        unsigned int multiFrameIndex = 1;
        TryGetUInt(params, "DLSSG.MultiFrameIndex", multiFrameIndex);

        if (multiFrameIndex > 1)
            return;

        // Check Reset flag
        unsigned int resetVal = 0;
        bool hasReset = TryGetUInt(params, "DLSSG.Reset", resetVal);
        bool isReset = hasReset && (resetVal != 0);

        // Need at least position OR orientation to be useful
        bool hasAnyData = hasPos || hasFwd;

        auto now = std::chrono::steady_clock::now();

        // -----------------------------------------------------------------
        // 2. If no camera data at all, mark unavailable
        // -----------------------------------------------------------------
        if (!hasAnyData)
        {
            if (!g_LoggedMissingData)
            {
                g_LoggedMissingData = true;
                LOG_WARNING(L"[ActionIntensity] Scene intensity disabled - no camera data in DLSSG params");
                if (!hasPos)   LOG_WARNING(L"[ActionIntensity]   MISSING: DLSSG.CameraPosX/Y/Z");
                if (!hasFwd)   LOG_WARNING(L"[ActionIntensity]   MISSING: DLSSG.CameraFwdX/Y/Z");
                if (!hasFov)   LOG_WARNING(L"[ActionIntensity]   MISSING: DLSSG.CameraFOV");
                if (!hasReset) LOG_WARNING(L"[ActionIntensity]   MISSING: DLSSG.Reset");
            }

            g_ConsecutiveValidFrames = 0;

            std::lock_guard<std::mutex> lock(g_SnapshotMutex);
            g_CurrentSnapshot = {};
            g_CurrentSnapshot.isAvailable = false;
            g_CurrentSnapshot.frameIndex = g_FrameIndex++;
            return;
        }

        // Log successful activation once
        if (!g_LoggedActivation && g_ConsecutiveValidFrames + 1 >= g_Config.warmupFrames)
        {
            g_LoggedActivation = true;
            LOG_WARNING(L"[ActionIntensity] Activated - scene intensity detection online");
            if (hasPos) LOG_WARNING(L"[ActionIntensity]   Using: CameraPos (translation speed)");
            if (hasFwd) LOG_WARNING(L"[ActionIntensity]   Using: CameraFwd (rotation speed)");
            if (hasFov) LOG_WARNING(L"[ActionIntensity]   Using: CameraFOV (zoom/ADS)");
        }

        // -----------------------------------------------------------------
        // 3. Compute deltas if we have a valid previous frame
        // -----------------------------------------------------------------
        float rawTranslation = 0.f;
        float rawRotation = 0.f;
        float rawYaw = 0.f;
        float rawPitch = 0.f;
        float rawFovChange = 0.f;
        float dt = 0.f;

        bool canCompute = g_PrevFrame.valid && !isReset;

        if (canCompute)
        {
            // Delta time from wall clock (steady_clock - immune to NTP jumps)
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                now - g_PrevFrame.timestamp);
            dt = static_cast<float>(elapsed.count()) * 1e-6f;

            // Clamp dt to avoid division by tiny values on frame spikes
            // and absurd values on pauses/alt-tabs
            dt = (std::max)(dt, 0.0001f);
            if (dt > 0.5f) // >500ms gap = probably paused/loading
            {
                canCompute = false;
            }
        }

        if (canCompute)
        {
            // Translation speed decomposed into forward/back vs strafe
            if (hasPos && g_PrevFrame.valid)
            {
                Vec3 delta = {
                    pos.x - g_PrevFrame.pos.x,
                    pos.y - g_PrevFrame.pos.y,
                    pos.z - g_PrevFrame.pos.z
                };

                float deltaLen = Length(delta);
                if (deltaLen > 0.0001f && hasFwd)
                {
                    // Project delta onto forward direction
                    float fwdLen = Length(fwd);
                    if (fwdLen > 0.001f)
                    {
                        Vec3 fwdN = { fwd.x / fwdLen, fwd.y / fwdLen, fwd.z / fwdLen };
                        float forwardComponent = std::abs(Dot(delta, fwdN));
                        float strafeComponent = std::sqrt((std::max)(deltaLen * deltaLen - forwardComponent * forwardComponent, 0.0f));

                        // Strafe deadzone: head bob generates lateral/vertical micro-sway.
                        // Suppress strafe component below threshold (units per frame, not per sec).
                        float strafePerSec = strafeComponent / dt;
                        if (strafePerSec < g_Config.translationDeadzoneUnitsPerSec)
                            strafeComponent = 0.f;

                        // Forward/back: quarter sensitivity (0.25x)
                        // Strafe (lateral): boosted sensitivity (1.5x)
                        rawTranslation = (forwardComponent * 0.25f + strafeComponent * 1.5f) / dt;
                    }
                    else
                    {
                        rawTranslation = deltaLen / dt;
                    }
                }
                else
                {
                    rawTranslation = deltaLen / dt;
                }
            }

            // Rotation speed decomposed into yaw (horizontal) and pitch (vertical)
            // Rotation overall boosted by 1.5x
            if (hasFwd && g_PrevFrame.valid)
            {
                float fwdLen = Length(fwd);
                float prevFwdLen = Length(g_PrevFrame.fwd);

                if (fwdLen > 0.001f && prevFwdLen > 0.001f)
                {
                    // Total rotation angle
                    Vec3 fwdN = { fwd.x / fwdLen, fwd.y / fwdLen, fwd.z / fwdLen };
                    Vec3 prevFwdN = { g_PrevFrame.fwd.x / prevFwdLen, g_PrevFrame.fwd.y / prevFwdLen, g_PrevFrame.fwd.z / prevFwdLen };

                    float cosAngle = Dot(fwdN, prevFwdN);
                    cosAngle = (std::max)(-1.0f, (std::min)(1.0f, cosAngle));
                    float totalAngle = std::acos(cosAngle);

                    // Decompose into yaw/pitch using Up vector
                    // Pitch = rotation component along Up axis
                    // Yaw = remainder (rotation in the horizontal plane)
                    float signedYawRate = 0.f;
                    float signedPitchRate = 0.f;

                    if (hasUp)
                    {
                        float upLen = Length(up);
                        if (upLen > 0.001f)
                        {
                            Vec3 upN = { up.x / upLen, up.y / upLen, up.z / upLen };

                            // Pitch: difference in elevation (dot with up) - signed
                            float pitchCurr = Dot(fwdN, upN);
                            float pitchPrev = Dot(prevFwdN, upN);
                            float pitchDelta = pitchCurr - pitchPrev;
                            float pitchAngle = std::asin(Saturate(std::abs(pitchDelta)));

                            // Yaw: remainder via Pythagorean on the rotation sphere
                            float yawAngle = 0.f;
                            if (totalAngle > pitchAngle)
                                yawAngle = std::sqrt(totalAngle * totalAngle - pitchAngle * pitchAngle);

                            // Yaw sign: cross product of prevFwd x fwd projected onto up
                            // Positive = turning left, negative = turning right (or vice versa - sign is consistent)
                            Vec3 cross = {
                                prevFwdN.y * fwdN.z - prevFwdN.z * fwdN.y,
                                prevFwdN.z * fwdN.x - prevFwdN.x * fwdN.z,
                                prevFwdN.x * fwdN.y - prevFwdN.y * fwdN.x
                            };
                            float yawSign = (Dot(cross, upN) >= 0.f) ? 1.f : -1.f;
                            float pitchSign = (pitchDelta >= 0.f) ? 1.f : -1.f;

                            signedYawRate = yawSign * yawAngle / dt;
                            signedPitchRate = pitchSign * pitchAngle / dt;

                            rawYaw = yawAngle / dt;
                            rawPitch = pitchAngle / dt;
                        }
                        else
                        {
                            // Up vector degenerate, fallback to total rotation as yaw
                            rawYaw = totalAngle / dt;
                            signedYawRate = rawYaw; // can't determine sign, assume positive
                        }
                    }
                    else
                    {
                        // No Up vector available, treat all rotation as yaw
                        rawYaw = totalAngle / dt;
                        signedYawRate = rawYaw;
                    }

                    // ---------------------------------------------------------
                    // Rotation direction continuity gate:
                    // If rotation continues in the same direction as previous
                    // frame AND speed hasn't changed much, suppress it.
                    // This filters out steady panning (mouse sweeps, cinematic
                    // camera) while still detecting flicks, reversals, and
                    // erratic combat mouse movement.
                    // Threshold 3.0x: only suppress if speed is within 3x of
                    // previous frame (very smooth sweep). Combat mouse-look
                    // has enough frame-to-frame variance to pass through.
                    // ---------------------------------------------------------
                    auto isSteadyAxis = [](float curr, float prev) -> bool
                        {
                            // Same sign = same direction
                            if (curr * prev <= 0.f)
                                return false; // direction changed or one is zero

                            // Ignore micro-jitter near zero (below ~5 deg/s)
                            float absCurr = std::abs(curr);
                            float absPrev = std::abs(prev);
                            if (absCurr < 0.09f && absPrev < 0.09f)
                                return true; // both near-zero, treat as steady (suppress)

                            // Check if speed changed by >= 200% (tripled or thirded)
                            float slower = (std::min)(absCurr, absPrev);
                            float faster = (std::max)(absCurr, absPrev);

                            // faster/slower >= 3.0 means >=200% change -> not steady
                            return (faster < slower * 3.0f);
                        };

                    if (isSteadyAxis(signedYawRate, g_PrevYawRate))
                        rawYaw = 0.f;
                    if (isSteadyAxis(signedPitchRate, g_PrevPitchRate))
                        rawPitch = 0.f;

                    g_PrevYawRate = signedYawRate;
                    g_PrevPitchRate = signedPitchRate;

                    // Combined rotation: pitch boosted by aspect ratio
                    // Vertical motion is more perceptually disruptive on wider screens
                    // Overall rotation boosted 1.5x vs translation
                    rawRotation = (rawYaw + rawPitch * aspectRatio) * 1.5f;

                    // Deadzone: suppress head bob / micro-sway
                    // Compare pre-boost raw rate against threshold
                    float rawRotUnboosted = rawYaw + rawPitch;
                    if (rawRotUnboosted < g_Config.rotationDeadzoneRadPerSec)
                        rawRotation = 0.f;
                }
            }

            // FOV change rate (rad/sec)
            if (hasFov && g_PrevFrame.fov > 0.f)
            {
                rawFovChange = std::abs(fov - g_PrevFrame.fov) / dt;
            }
        }

        // -----------------------------------------------------------------
        // 4. MFG context - stored in snapshot for consumers (DFG module)
        //    Sensitivity scaling is no longer applied here because we only
        //    compute on the first subframe (MultiFrameIndex==1), so dt and
        //    deltas are per-game-frame and already correct.
        // -----------------------------------------------------------------
        float mfgSensitivity = static_cast<float>(mfgMultiplier);

        // -----------------------------------------------------------------
        // 5. Normalize components to 0..1
        // -----------------------------------------------------------------
        const Config& cfg = g_Config;

        float nTranslation = Saturate(rawTranslation / (std::max)(cfg.maxTranslationSpeed, 0.001f));
        float nRotation = Saturate(rawRotation / (std::max)(cfg.maxRotationSpeed, 0.001f));
        float nFov = Saturate(rawFovChange / (std::max)(cfg.maxFovChangeRate, 0.001f));

        // -----------------------------------------------------------------
        // 6. Weighted combination -> raw intensity
        // -----------------------------------------------------------------
        float rawIntensity;

        if (isReset)
        {
            // Reset frame (teleport/cutscene/scene change) = spike
            rawIntensity = cfg.resetIntensity;
        }
        else if (!canCompute)
        {
            // First frame or after long pause - no delta, keep previous smoothed value
            rawIntensity = g_CurrentSnapshot.intensity;
        }
        else
        {
            rawIntensity = nTranslation * cfg.weightTranslation
                + nRotation * cfg.weightRotation
                + nFov * cfg.weightFov;
        }

        // -----------------------------------------------------------------
        // 6a. External boost from ctx.streamline.actionIntensityBoost
        // -----------------------------------------------------------------
        // Applied to raw intensity BEFORE EMA so that idle frames (raw=0)
        // still feed zero into the smoother and intensity decays naturally.
        // boost > 1.0 amplifies, < 1.0 dampens, 0.0 or 1.0 = no-op.
        float boost = ctx.streamline.actionIntensityBoost;
        if (boost > 0.0f && boost != 1.0f)
        {
            rawIntensity = Saturate(rawIntensity * boost);
        }

        // -----------------------------------------------------------------
        // 6b. EMA smoothing
        // -----------------------------------------------------------------
        float smoothed;

        if (g_ConsecutiveValidFrames == 0)
        {
            // Cold start - seed directly, don't smooth from zero
            smoothed = rawIntensity;
        }
        else
        {
            // Asymmetric EMA:
            //   Rising  -> fast ramp (smoothingAlpha=0.60, snappy response)
            //   Falling -> adaptive decay: slow at high intensity (holds combat feel),
            //              fast at low intensity (quickly drains to clean idle)
            float prev = g_CurrentSnapshot.intensity;
            float alpha;
            if (rawIntensity < prev)
            {
                // Decay: interpolate between min (slow, at high) and max (fast, at low)
                float t = 1.0f - prev;  // 0 at peak, 1 at idle
                alpha = cfg.decayAlphaMin + (cfg.decayAlphaMax - cfg.decayAlphaMin) * t;
            }
            else
            {
                alpha = cfg.smoothingAlpha;  // rising -> snappy ramp
            }
            alpha = Saturate(alpha);
            smoothed = prev * (1.0f - alpha) + rawIntensity * alpha;
        }

        // -----------------------------------------------------------------
        // 7. Track warmup
        // -----------------------------------------------------------------
        g_ConsecutiveValidFrames++;
        bool available = (g_ConsecutiveValidFrames >= cfg.warmupFrames);

        // -----------------------------------------------------------------
        // 8. Store previous frame for next iteration
        // -----------------------------------------------------------------
        g_PrevFrame.pos = pos;
        g_PrevFrame.fwd = fwd;
        g_PrevFrame.up = up;
        g_PrevFrame.fov = fov;
        g_PrevFrame.valid = hasAnyData;
        g_PrevFrame.timestamp = now;

        // On Reset, invalidate prev so next frame doesn't compute bogus delta
        if (isReset)
        {
            g_PrevFrame.valid = false;
            g_PrevYawRate = 0.f;
            g_PrevPitchRate = 0.f;
        }

        // -----------------------------------------------------------------
        // 9. Publish snapshot
        // -----------------------------------------------------------------

        // Diagnostic: log component breakdown every ~60 frames when intensity is notable
        if (smoothed > 0.1f && (g_FrameIndex % 60) == 0)
        {
            wchar_t dbg[256];
            swprintf_s(dbg, L"[ActionIntensity] I=%.3f | trans=%.3f(%.2f) rot=%.3f(%.2f) fov=%.3f(%.2f) | rawT=%.2f rawR=%.2f rawF=%.2f | boost=%.2f",
                smoothed, nTranslation, nTranslation * cfg.weightTranslation,
                nRotation, nRotation * cfg.weightRotation,
                nFov, nFov * cfg.weightFov,
                rawTranslation, rawRotation, rawFovChange, boost);
            LOG_WARNING(dbg);
        }

        Snapshot snap = {};
        snap.intensity = smoothed;
        snap.translationSpeed = nTranslation;
        snap.rotationSpeed = nRotation;
        snap.fovDelta = nFov;
        snap.rawTranslationUnitsPerSec = rawTranslation;
        snap.rawRotationRadPerSec = rawRotation;
        snap.rawFovRadPerSec = rawFovChange;
        snap.rawYawRadPerSec = rawYaw;
        snap.rawPitchRadPerSec = rawPitch;
        snap.mfgMultiplier = mfgMultiplier;
        snap.mfgSensitivityScale = mfgSensitivity;
        snap.isAvailable = available;
        snap.isReset = isReset;
        snap.frameIndex = g_FrameIndex++;

        {
            std::lock_guard<std::mutex> lock(g_SnapshotMutex);
            g_CurrentSnapshot = snap;
        }
    }

    // =========================================================================
    // D3D11 variant - same logic, different command list type
    // =========================================================================

    static void OnDlssgPreEvaluateD3D11(
        ID3D11DeviceContext*    /*cmdList*/,
        const NVSDK_NGX_Handle* /*handle*/,
        NVSDK_NGX_Parameter* params,
        NVSDK_NGX_Feature       /*featureId*/)
    {
        // Reuse D3D12 logic - the parameter extraction is API-agnostic.
        // We pass nullptr for cmdList since we don't use it.
        OnDlssgPreEvaluate(nullptr, nullptr, params, NVSDK_NGX_Feature_FrameGeneration);
    }

    // =========================================================================
    // Vulkan variant
    // =========================================================================

    static void OnDlssgPreEvaluateVulkan(
        VkCommandBuffer         /*cmdBuffer*/,
        const NVSDK_NGX_Handle* /*handle*/,
        NVSDK_NGX_Parameter* params,
        NVSDK_NGX_Feature       /*featureId*/)
    {
        OnDlssgPreEvaluate(nullptr, nullptr, params, NVSDK_NGX_Feature_FrameGeneration);
    }

    // =========================================================================
    // Public API
    // =========================================================================

    void Initialize()
    {
        bool expected = false;
        if (!g_Initialized.compare_exchange_strong(expected, true))
            return; // Already initialized

        LOG_WARNING(L"[ActionIntensity] Initializing - registering DLSSG PreEvaluate listeners");

        // Reset state
        g_PrevFrame = {};
        g_FrameIndex = 0;
        g_ConsecutiveValidFrames = 0;
        g_PrevYawRate = 0.f;
        g_PrevPitchRate = 0.f;
        g_LoggedMissingData = false;
        g_LoggedActivation = false;
        g_LoggedCallbackHit = false;

        {
            std::lock_guard<std::mutex> lock(g_SnapshotMutex);
            g_CurrentSnapshot = {};
        }

        // Register for DLSSG PreEvaluate on all graphics APIs.
        // Filter = FrameGeneration feature only (DLSSG dispatches).
        // We hook PreEvaluate (not Post) so data is available before
        // the interpolation dispatch - in case we want to modify
        // MFG parameters in a future PreEvaluate hook.

        NgxFeatureEvents::RegisterPreEvaluateD3D12(
            OnDlssgPreEvaluate,
            NVSDK_NGX_Feature_FrameGeneration);

        NgxFeatureEvents::RegisterPreEvaluateD3D11(
            OnDlssgPreEvaluateD3D11,
            NVSDK_NGX_Feature_FrameGeneration);

        NgxFeatureEvents::RegisterPreEvaluateVulkan(
            OnDlssgPreEvaluateVulkan,
            NVSDK_NGX_Feature_FrameGeneration);
    }

    void Shutdown()
    {
        LOG_WARNING(L"[ActionIntensity] Shutting down");
        g_Initialized.store(false);
        g_PrevFrame = {};
        g_ConsecutiveValidFrames = 0;
        g_PrevYawRate = 0.f;
        g_PrevPitchRate = 0.f;

        {
            std::lock_guard<std::mutex> lock(g_SnapshotMutex);
            g_CurrentSnapshot = {};
        }
    }

    Snapshot GetSnapshot()
    {
        std::lock_guard<std::mutex> lock(g_SnapshotMutex);
        return g_CurrentSnapshot;
    }

    Config& GetConfig()
    {
        return g_Config;
    }
}