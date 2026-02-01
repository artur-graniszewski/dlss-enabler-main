// =============================================================================
// MenuAnimations.cpp - Smooth Menu Animation System for DLSS Enabler
// =============================================================================
//
// INTEGRATION INSTRUCTIONS:
// -------------------------
// 1. In SettingsMenu.cpp, add at the top:
//    #include "MenuAnimations.h"
//
// 2. In SettingsMenu::Init(), add:
//    MenuAnimations::Init();
//
// 3. In SettingsMenu::Render(), at the very beginning:
//    MenuAnimations::Update(UxImGui::GetIO().DeltaTime);
//
// 4. When menu opens (F1 pressed), call:
//    MenuAnimations::StartSidePanelOpen();
//
// 5. When menu closes (F1 pressed again), call:
//    MenuAnimations::StartSidePanelClose();
//    // Don't return control to game until IsSidePanelFullyClosed()
//
// 6. In RenderSidePanel(), replace fixed panelWidth with:
//    const float panelWidth = MenuAnimations::GetSidePanelWidth();
//    if (panelWidth < 1.0f) return;  // Don't render if too small
//
// 7. Similarly for RenderMonitoringBar():
//    const float barHeight = MenuAnimations::GetMonitoringBarHeight();
//    if (barHeight < 1.0f) return;
//
// =============================================================================

#include "MenuAnimations.h"

// Prevent Windows.h min/max macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>

namespace MenuAnimations
{
    // =============================================================================
    // Constants & Defaults
    // =============================================================================

    static const float DEFAULT_ANIMATION_DURATION = 0.3f;  // 300ms for snappy feel
    static const float DEFAULT_SIDEBAR_WIDTH = 280.0f;
    static const float DEFAULT_MONITORING_BAR_HEIGHT = 32.0f;

    // =============================================================================
    // Internal State
    // =============================================================================

    struct AnimationData
    {
        AnimationState state = AnimationState::Closed;
        float progress = 0.0f;      // 0.0 = closed, 1.0 = open
        float targetValue = 0.0f;   // Target width/height
        float currentValue = 0.0f;  // Current animated value
    };

    static bool g_Initialized = false;
    static float g_AnimationDuration = DEFAULT_ANIMATION_DURATION;

    static AnimationData g_SidePanel;
    static AnimationData g_MonitoringBar;

    // =============================================================================
    // Easing Functions
    // =============================================================================

    // Ease-out cubic: fast start, smooth slow end
    static float EaseOutCubic(float t)
    {
        t = t - 1.0f;
        return t * t * t + 1.0f;
    }

    // Ease-in cubic: slow start, fast end (for closing)
    static float EaseInCubic(float t)
    {
        return t * t * t;
    }

    // Ease-out quart: even smoother
    static float EaseOutQuart(float t)
    {
        t = t - 1.0f;
        return 1.0f - t * t * t * t;
    }

    // Ease-in-out for balanced feel
    static float EaseInOutCubic(float t)
    {
        if (t < 0.5f)
            return 4.0f * t * t * t;
        else
        {
            float f = 2.0f * t - 2.0f;
            return 0.5f * f * f * f + 1.0f;
        }
    }

    // =============================================================================
    // Internal Helpers
    // =============================================================================

    static void UpdateAnimation(AnimationData& anim, float deltaTime, bool isOpening)
    {
        if (anim.state == AnimationState::Closed || anim.state == AnimationState::Open)
            return;

        // Calculate progress delta
        float progressDelta = deltaTime / g_AnimationDuration;

        if (isOpening)
        {
            anim.progress += progressDelta;
            if (anim.progress >= 1.0f)
            {
                anim.progress = 1.0f;
                anim.state = AnimationState::Open;
            }
            // Use ease-out for opening (fast start, smooth end)
            anim.currentValue = anim.targetValue * EaseOutCubic(anim.progress);
        }
        else
        {
            anim.progress -= progressDelta;
            if (anim.progress <= 0.0f)
            {
                anim.progress = 0.0f;
                anim.state = AnimationState::Closed;
            }
            // Use ease-out for closing too (smooth feel)
            anim.currentValue = anim.targetValue * EaseOutCubic(anim.progress);
        }
    }

    // =============================================================================
    // Initialization
    // =============================================================================

    void Init()
    {
        if (g_Initialized) return;

        g_AnimationDuration = DEFAULT_ANIMATION_DURATION;

        // SidePanel - starts closed
        g_SidePanel.state = AnimationState::Closed;
        g_SidePanel.progress = 0.0f;
        g_SidePanel.targetValue = DEFAULT_SIDEBAR_WIDTH;
        g_SidePanel.currentValue = 0.0f;

        // MonitoringBar - starts closed
        g_MonitoringBar.state = AnimationState::Closed;
        g_MonitoringBar.progress = 0.0f;
        g_MonitoringBar.targetValue = DEFAULT_MONITORING_BAR_HEIGHT;
        g_MonitoringBar.currentValue = 0.0f;

        g_Initialized = true;
    }

    // =============================================================================
    // Per-Frame Update
    // =============================================================================

    void Update(float deltaTime)
    {
        if (!g_Initialized) Init();

        // Clamp deltaTime to avoid huge jumps
        deltaTime = (std::min)(deltaTime, 0.1f);

        // Update SidePanel
        if (g_SidePanel.state == AnimationState::Opening)
        {
            UpdateAnimation(g_SidePanel, deltaTime, true);
        }
        else if (g_SidePanel.state == AnimationState::Closing)
        {
            UpdateAnimation(g_SidePanel, deltaTime, false);
        }

        // Update MonitoringBar
        if (g_MonitoringBar.state == AnimationState::Opening)
        {
            UpdateAnimation(g_MonitoringBar, deltaTime, true);
        }
        else if (g_MonitoringBar.state == AnimationState::Closing)
        {
            UpdateAnimation(g_MonitoringBar, deltaTime, false);
        }
    }

    // =============================================================================
    // SidePanel Animation
    // =============================================================================

    void StartSidePanelOpen()
    {
        if (!g_Initialized) Init();

        if (g_SidePanel.state == AnimationState::Open)
            return;  // Already open

        g_SidePanel.state = AnimationState::Opening;
        // If we were closing, continue from current progress
        // If we were closed, start from 0
        if (g_SidePanel.progress <= 0.0f)
            g_SidePanel.progress = 0.0f;
    }

    void StartSidePanelClose()
    {
        if (!g_Initialized) Init();

        if (g_SidePanel.state == AnimationState::Closed)
            return;  // Already closed

        g_SidePanel.state = AnimationState::Closing;
        // Continue from current progress
    }

    float GetSidePanelWidth()
    {
        if (!g_Initialized) return 0.0f;
        return g_SidePanel.currentValue;
    }

    float GetSidePanelProgress()
    {
        if (!g_Initialized) return 0.0f;
        return g_SidePanel.progress;
    }

    AnimationState GetSidePanelState()
    {
        return g_SidePanel.state;
    }

    bool IsSidePanelFullyClosed()
    {
        return g_SidePanel.state == AnimationState::Closed;
    }

    bool IsSidePanelFullyOpen()
    {
        return g_SidePanel.state == AnimationState::Open;
    }

    bool IsSidePanelAnimating()
    {
        return g_SidePanel.state == AnimationState::Opening ||
            g_SidePanel.state == AnimationState::Closing;
    }

    // =============================================================================
    // MonitoringBar Animation
    // =============================================================================

    void StartMonitoringBarOpen()
    {
        if (!g_Initialized) Init();

        if (g_MonitoringBar.state == AnimationState::Open)
            return;

        g_MonitoringBar.state = AnimationState::Opening;
        if (g_MonitoringBar.progress <= 0.0f)
            g_MonitoringBar.progress = 0.0f;
    }

    void StartMonitoringBarClose()
    {
        if (!g_Initialized) Init();

        if (g_MonitoringBar.state == AnimationState::Closed)
            return;

        g_MonitoringBar.state = AnimationState::Closing;
    }

    float GetMonitoringBarHeight()
    {
        if (!g_Initialized) return 0.0f;
        return g_MonitoringBar.currentValue;
    }

    float GetMonitoringBarProgress()
    {
        if (!g_Initialized) return 0.0f;
        return g_MonitoringBar.progress;
    }

    AnimationState GetMonitoringBarState()
    {
        return g_MonitoringBar.state;
    }

    bool IsMonitoringBarFullyClosed()
    {
        return g_MonitoringBar.state == AnimationState::Closed;
    }

    bool IsMonitoringBarFullyOpen()
    {
        return g_MonitoringBar.state == AnimationState::Open;
    }

    bool IsMonitoringBarAnimating()
    {
        return g_MonitoringBar.state == AnimationState::Opening ||
            g_MonitoringBar.state == AnimationState::Closing;
    }

    // =============================================================================
    // Combined State Helpers
    // =============================================================================

    bool AreAllMenusFullyClosed()
    {
        return IsSidePanelFullyClosed() && IsMonitoringBarFullyClosed();
    }

    bool IsAnyMenuAnimating()
    {
        return IsSidePanelAnimating() || IsMonitoringBarAnimating();
    }

    // =============================================================================
    // Configuration
    // =============================================================================

    void SetAnimationDuration(float seconds)
    {
        // Clamp to reasonable range (0.1s to 2.0s)
        g_AnimationDuration = (std::max)(0.1f, (std::min)(seconds, 2.0f));
    }

    float GetAnimationDuration()
    {
        return g_AnimationDuration;
    }

    void SetSidePanelTargetWidth(float width)
    {
        g_SidePanel.targetValue = width;
    }

    void SetMonitoringBarTargetHeight(float height)
    {
        g_MonitoringBar.targetValue = height;
    }
}