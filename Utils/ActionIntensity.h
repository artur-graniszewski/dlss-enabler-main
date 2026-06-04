// =============================================================================
// ActionIntensity.h - Scene Action Intensity Detector for DFG MFG Mode Control
// =============================================================================
//
// Quantifies scene "action intensity" on a 0.0-1.0 scale by tracking per-frame
// camera motion data from DLSSG Evaluate parameters. Designed to feed into the
// Dynamic Frame Generation module for adaptive MFG multiplier decisions.
//
// DATA SOURCE:
//   Hooks into DLSSG PreEvaluate events via NgxFeatureEvents. Extracts camera
//   position, orientation, FOV, and reset flag from NVSDK_NGX_Parameter each
//   frame. These parameters are optional - if a game doesn't supply them, the
//   detector disables itself gracefully (isAvailable = false).
//
// USAGE:
//   1. Call ActionIntensity::Initialize() once at startup (registers events)
//   2. Call ActionIntensity::GetSnapshot() from DFG decision logic
//   3. Call ActionIntensity::Shutdown() on teardown
//
// =============================================================================

#pragma once

#include <cstdint>

namespace ActionIntensity
{
    // =========================================================================
    // Output structure - consumed by DFG module to decide MFG mode
    // =========================================================================
    struct Snapshot
    {
        // --- Core metric ---
        float intensity;            // Smoothed action intensity, 0.0 (idle) .. 1.0 (peak action)

        // --- Component breakdown (all 0.0 .. 1.0, pre-smoothing) ---
        float translationSpeed;     // Normalized camera translation speed
        float rotationSpeed;        // Normalized camera rotation speed (mouse look / aim)
        float fovDelta;             // Normalized FOV change rate (ADS / zoom)

        // --- Raw values (world-space, for debug overlay) ---
        float rawTranslationUnitsPerSec;   // length(deltaPos) / dt
        float rawRotationRadPerSec;        // acos(dot(fwd, prevFwd)) / dt
        float rawFovRadPerSec;             // |deltaFOV| / dt
        float rawYawRadPerSec;             // horizontal rotation component
        float rawPitchRadPerSec;           // vertical rotation component

        // --- MFG context ---
        int   mfgMultiplier;        // Current MFG mode: 1=no MFG, 2=2X, 3=3X, 4=4X
        float mfgSensitivityScale;  // Applied sensitivity multiplier from MFG mode

        // --- Boost ---
        float appliedBoost;         // The boost multiplier that was applied this frame (from ctx.streamline.actionIntensityBoost)

        // --- State ---
        bool  isAvailable;          // false if camera data not supplied by game
        bool  isReset;              // true on the frame Reset=1 was seen (teleport/cutscene)
        uint64_t frameIndex;        // Monotonic frame counter since Initialize()
    };

    // =========================================================================
    // Tuning parameters - can be adjusted at runtime via ImGui
    // =========================================================================
    struct Config
    {
        // EMA smoothing factor (0..1). Lower = more smoothing, less flicker.
        // 0.75 gives snappy response: a sudden max rotation from idle
        // pushes intensity to ~68% in a single frame.
        float smoothingAlpha = 0.75f;

        // Decay alpha range — adaptive: slow at high intensity, fast at low.
        // effectiveDecay = decayAlphaMin + (decayAlphaMax - decayAlphaMin) * (1 - intensity)
        // This holds high intensity longer (combat feel) while quickly
        // draining residual low values back to zero (clean idle).
        float decayAlphaMin = 0.08f;   // at intensity ~1.0: very slow decay
        float decayAlphaMax = 0.50f;   // at intensity ~0.0: fast drain to zero

        // Max expected values for normalization (pre-MFG-scaling).
        // These define what "1.0" means at 1X (no MFG). MFG modes divide
        // these by mfgSensitivity, making detection much more aggressive.
        float maxTranslationSpeed = 8.0f;    // units/sec - walking speed

        // Translation deadzone: ignore translation below this rate (filters head bob).
        // Sprint head bob generates ~2-4 units/sec of lateral/vertical sway.
        float translationDeadzoneUnitsPerSec = 4.0f;
        float maxRotationSpeed = 0.375f;   // rad/sec   - ~21 deg/s, 2x more sensitive than before

        // Rotation deadzone: ignore rotation below this rate (filters head bob).
        // Sprint head bob generates ~0.3-0.5 rad/s of micro-sway.
        // 25 deg/s = ~0.44 rad/s
        float rotationDeadzoneRadPerSec = 0.44f;
        float maxFovChangeRate = 0.5f;    // rad/sec   - moderate ADS transition

        // Component weights. Rotation alone should be able to push
        // intensity near 1.0. Weights intentionally sum above 1.0 -
        // final intensity is clamped to 0..1 via Saturate().
        float weightTranslation = 0.20f;
        float weightRotation = 0.90f;
        float weightFov = 0.20f;

        // MFG sensitivity scaling: sensitivity = mfgSensitivityBase ^ (mfgMultiplier - 2)
        // 2X = 1.0x (base), 3X = 2.0x, 4X = 4.0x
        // Implemented as: scale = mfgSensitivityLut[multiplier]
        // (lookup is faster and more explicit than pow())

        // After Reset flag, force intensity to this value for one frame
        float resetIntensity = 1.0f;

        // Minimum consecutive frames with data before isAvailable = true
        // (avoids false positive on first few frames with partial data)
        uint32_t warmupFrames = 3;
    };

    // =========================================================================
    // API
    // =========================================================================

    // Register NgxFeatureEvents listeners. Safe to call multiple times (no-op after first).
    void Initialize();

    // Unregister listeners and reset state.
    void Shutdown();

    // Get current action intensity snapshot. Thread-safe (atomic copy).
    // Returns a snapshot with isAvailable=false if not yet initialized or
    // if the game doesn't supply camera parameters.
    Snapshot GetSnapshot();

    // Access config for runtime tuning (e.g. from ImGui).
    // Not thread-safe for writes - only mutate from the main/UI thread
    // between frames, never during Evaluate callbacks.
    Config& GetConfig();

    // Draw ImGui debug overlay showing all snapshot components.
    // Call from your ImGui render loop.
    void DrawDebugUI();
}