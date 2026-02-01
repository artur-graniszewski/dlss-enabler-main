#pragma once
// =============================================================================
// OverdriveController.h - Overdrive mode controller for performance presets
// =============================================================================
//
// Manages override values for various settings based on selected Overdrive mode.
// When Overdrive is active (mode > 0), these values should be used instead of
// the user-configured values in ctx.
//
// Modes:
//   -1 = Hidden (feature disabled)
//    0 = OFF (no override, use user settings)
//    1 = Performance (max FPS, lower quality)
//    2 = Quality (balanced, adaptive FG)
//    3 = Latency (minimum input lag)
//
// =============================================================================

#ifndef OVERDRIVE_CONTROLLER_H
#define OVERDRIVE_CONTROLLER_H

namespace OverdriveController
{
    // =========================================================================
    // Overdrive Modes
    // =========================================================================

    enum class Mode
    {
        Hidden = -1,      // Feature disabled, not shown in UI
        Off = 0,          // No override, use user settings
        Performance = 1,  // Max FPS, lower quality
        Quality = 2,      // Balanced, adaptive FG
        Latency = 3       // Minimum input lag
    };

    // =========================================================================
    // Initialization
    // =========================================================================

    // Initialize the controller (call once at startup)
    void Init();

    // Update override values based on current mode (call each frame)
    void Update();

    // =========================================================================
    // Mode Control
    // =========================================================================

    // Get current overdrive mode
    Mode GetMode();

    // Check if overdrive is active (mode > 0)
    bool IsActive();

    // =========================================================================
    // Override Value Getters
    // These return the effective values to use (either override or user value)
    // =========================================================================

    // V-Sync override
    bool GetVsyncOverrideEnabled();
    bool GetVsyncEnabled();

    // Reflex settings
    bool GetReflexEnabled();
    bool GetBoostOverriden();
    bool GetBoostEnabled();

    // FPS Limit
    bool GetFpsLimitEnabled();
    int GetDesiredFpsLimit();

    // Adaptive Frame Generation
    bool GetDynamicFrameGenerationEnabled();
    int GetDynamicFrameGenerationThreshold();
    bool GetDynamicFrameGenerationStartingOnThreshold();

    // SSRTGI Quality
    int GetRayTracingQuality();

    // =========================================================================
    // Debug / Info
    // =========================================================================

    // Get mode name as string
    const char* GetModeName();
}

#endif // OVERDRIVE_CONTROLLER_H