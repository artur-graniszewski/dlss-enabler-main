// =============================================================================
// OverdriveController.cpp - Overdrive mode controller for performance presets
// =============================================================================

#include "OverdriveController.h"
#include "SwapChainEvents.h"
#include "../Core/Context.h"

namespace OverdriveController
{
    // =========================================================================
    // Internal State - Override Values
    // =========================================================================

    struct OverrideState
    {
        // V-Sync
        bool isVsyncOverrideEnabled = false;
        bool isVsyncEnabled = false;

        // Reflex
        bool isReflexEnabled = true;
        bool isBoostOverriden = false;
        bool isBoostEnabled = false;

        // FPS Limit
        bool isFpsLimitEnabled = false;
        int desiredFpsLimit = 60;

        // Adaptive Frame Generation
        bool isDynamicFrameGenerationEnabled = false;
        int dynamicFrameGenerationThreshold = 60;
        bool isDynamicFrameGenerationStartingOnThreshold = true;

        // SSRTGI Quality (0=Ultra, 1=High, 2=Medium, 3=Low)
        int rayTracingQuality = 1;
    };

    static OverrideState g_Override;
    static bool g_Initialized = false;

    // =========================================================================
    // Forward declarations
    // =========================================================================

    static void OnPrePresent(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& Flags);
    static void OnPrePresent1(IDXGISwapChain1* pSwapChain, UINT& SyncInterval, UINT& Flags, const DXGI_PRESENT_PARAMETERS* pParams);

    // =========================================================================
    // Initialization
    // =========================================================================

    void Init()
    {
        if (g_Initialized)
            return;

        // Register for Present events to update each frame
        SwapChainEvents::RegisterPrePresent(OnPrePresent);
        SwapChainEvents::RegisterPrePresent1(OnPrePresent1);

        g_Initialized = true;
        Update();
    }

    // =========================================================================
    // Present Event Handlers
    // =========================================================================

    static void OnPrePresent(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& Flags)
    {
        Update();
    }

    static void OnPrePresent1(IDXGISwapChain1* pSwapChain, UINT& SyncInterval, UINT& Flags, const DXGI_PRESENT_PARAMETERS* pParams)
    {
        Update();
    }

    // =========================================================================
    // Strategy Functions - Set override values based on mode
    // =========================================================================

    static void ApplyPerformanceStrategy()
    {
        // Performance mode: Max FPS, lower quality
        // - Reflex disabled (less overhead)
        // - Reflex Boost ON (forced override)
        // - Adaptive FG OFF
        // - SSRTGI Quality = Low
        // - VSync OFF

        g_Override.isVsyncOverrideEnabled = true;
        g_Override.isVsyncEnabled = false;

        g_Override.isReflexEnabled = false;
        g_Override.isBoostOverriden = true;   // Force override
        g_Override.isBoostEnabled = true;     // Boost ON

        g_Override.isFpsLimitEnabled = false;
        g_Override.desiredFpsLimit = 0;

        g_Override.isDynamicFrameGenerationEnabled = false;
        g_Override.dynamicFrameGenerationThreshold = 0;
        g_Override.isDynamicFrameGenerationStartingOnThreshold = true;

        g_Override.rayTracingQuality = 3;  // Low
    }

    static void ApplyQualityStrategy()
    {
        // Quality mode: Balanced, adaptive FG for smoothness
        // - Reflex enabled
        // - Reflex Boost ON (forced override)
        // - Adaptive FG ON, threshold 30, generation STOPS below threshold
        // - SSRTGI Quality = High
        // - VSync from user setting

        g_Override.isVsyncOverrideEnabled = ctx.reflex.isVsyncOverrideEnabled;
        g_Override.isVsyncEnabled = ctx.reflex.isVsyncEnabled;

        g_Override.isReflexEnabled = true;
        g_Override.isBoostOverriden = true;   // Force override
        g_Override.isBoostEnabled = true;     // Boost ON

        g_Override.isFpsLimitEnabled = ctx.reflex.isFpsLimitEnabled;
        g_Override.desiredFpsLimit = ctx.reflex.desiredFpsLimit;

        g_Override.isDynamicFrameGenerationEnabled = true;
        g_Override.dynamicFrameGenerationThreshold = 30;
        g_Override.isDynamicFrameGenerationStartingOnThreshold = false;  // STOPS below threshold

        g_Override.rayTracingQuality = 1;  // High
    }

    static void ApplyLatencyStrategy()
    {
        // Latency mode: Minimum input lag
        // - Reflex enabled
        // - Reflex Boost ON (forced override)
        // - Adaptive FG ON, copy threshold from user setting, generation STARTS on threshold
        // - SSRTGI Quality = Medium
        // - VSync OFF

        g_Override.isVsyncOverrideEnabled = true;
        g_Override.isVsyncEnabled = false;

        g_Override.isReflexEnabled = true;
        g_Override.isBoostOverriden = true;   // Force override
        g_Override.isBoostEnabled = true;     // Boost ON

        g_Override.isFpsLimitEnabled = ctx.reflex.isFpsLimitEnabled;
        g_Override.desiredFpsLimit = ctx.reflex.desiredFpsLimit;

        g_Override.isDynamicFrameGenerationEnabled = true;
        g_Override.dynamicFrameGenerationThreshold = ctx.ngx.dynamicFrameGenerationThreshold;
        g_Override.isDynamicFrameGenerationStartingOnThreshold = true;  // STARTS on threshold

        g_Override.rayTracingQuality = 2;  // Medium
    }

    static void ApplyOffStrategy()
    {
        // Off mode: Synchronize ALL values from user settings (ctx)
        // This ensures that when user changes settings in UI, they're immediately reflected
        g_Override.isVsyncOverrideEnabled = ctx.reflex.isVsyncOverrideEnabled;
        g_Override.isVsyncEnabled = ctx.reflex.isVsyncEnabled;

        g_Override.isReflexEnabled = true;  // Default when not overriding
        g_Override.isBoostOverriden = ctx.reflex.isBoostOverriden;
        g_Override.isBoostEnabled = ctx.reflex.isBoostEnabled;

        g_Override.isFpsLimitEnabled = ctx.reflex.isFpsLimitEnabled;
        g_Override.desiredFpsLimit = ctx.reflex.desiredFpsLimit;

        g_Override.isDynamicFrameGenerationEnabled = ctx.ngx.isDynamicFrameGenerationEnabled;
        g_Override.dynamicFrameGenerationThreshold = ctx.ngx.dynamicFrameGenerationThreshold;
        g_Override.isDynamicFrameGenerationStartingOnThreshold = ctx.ngx.isDynamicFrameGenerationStartingOnThreshold;

        g_Override.rayTracingQuality = ctx.ngx.rayTracingQuality;
    }

    // =========================================================================
    // Update
    // =========================================================================

    void Update()
    {
        Mode mode = GetMode();

        switch (mode)
        {
        case Mode::Performance:
            ApplyPerformanceStrategy();
            break;
        case Mode::Quality:
            ApplyQualityStrategy();
            break;
        case Mode::Latency:
            ApplyLatencyStrategy();
            break;
        case Mode::Off:
        case Mode::Hidden:
        default:
            ApplyOffStrategy();
            break;
        }
    }

    // =========================================================================
    // Mode Control
    // =========================================================================

    Mode GetMode()
    {
        return static_cast<Mode>(ctx.overdriveMode);
    }

    bool IsActive()
    {
        return ctx.overdriveMode > 0;
    }

    // =========================================================================
    // Override Value Getters
    // =========================================================================

    bool GetVsyncOverrideEnabled()
    {
        if (!IsActive())
            return ctx.reflex.isVsyncOverrideEnabled;
        return g_Override.isVsyncOverrideEnabled;
    }

    bool GetVsyncEnabled()
    {
        if (!IsActive())
            return ctx.reflex.isVsyncEnabled;
        return g_Override.isVsyncEnabled;
    }

    bool GetReflexEnabled()
    {
        if (!IsActive())
            return ctx.reflex.isOriginallyEnabled;
        return g_Override.isReflexEnabled;
    }

    bool GetBoostOverriden()
    {
        if (!IsActive())
            return ctx.reflex.isBoostOverriden;
        return g_Override.isBoostOverriden;
    }

    bool GetBoostEnabled()
    {
        if (!IsActive())
            return ctx.reflex.isBoostEnabled;
        return g_Override.isBoostEnabled;
    }

    bool GetFpsLimitEnabled()
    {
        if (!IsActive())
            return ctx.reflex.isFpsLimitEnabled;
        return g_Override.isFpsLimitEnabled;
    }

    int GetDesiredFpsLimit()
    {
        if (!IsActive())
            return ctx.reflex.desiredFpsLimit;
        return g_Override.desiredFpsLimit;
    }

    bool GetDynamicFrameGenerationEnabled()
    {
        if (!IsActive())
            return ctx.ngx.isDynamicFrameGenerationEnabled;
        return g_Override.isDynamicFrameGenerationEnabled;
    }

    int GetDynamicFrameGenerationThreshold()
    {
        if (!IsActive())
            return ctx.ngx.dynamicFrameGenerationThreshold;
        return g_Override.dynamicFrameGenerationThreshold;
    }

    bool GetDynamicFrameGenerationStartingOnThreshold()
    {
        if (!IsActive())
            return ctx.ngx.isDynamicFrameGenerationStartingOnThreshold;
        return g_Override.isDynamicFrameGenerationStartingOnThreshold;
    }

    int GetRayTracingQuality()
    {
        if (!IsActive())
            return ctx.ngx.rayTracingQuality;
        return g_Override.rayTracingQuality;
    }

    // =========================================================================
    // Debug / Info
    // =========================================================================

    const char* GetModeName()
    {
        switch (GetMode())
        {
        case Mode::Hidden:      return "Hidden";
        case Mode::Off:         return "Off";
        case Mode::Performance: return "Performance";
        case Mode::Quality:     return "Quality";
        case Mode::Latency:     return "Latency";
        default:                return "Unknown";
        }
    }
}