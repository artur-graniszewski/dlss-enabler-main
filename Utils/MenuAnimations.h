// =============================================================================
// MenuAnimations.h - Smooth Menu Animation System for DLSS Enabler
// =============================================================================
//
// Provides smooth expand/collapse animations for:
// - SidePanel: expands horizontally (left to right)
// - MonitoringBar: expands vertically (top to bottom)
//
// Animation duration: 1 second max, uses ease-out curve for smooth feel
//
// USAGE:
//   1. Call MenuAnimations::Init() at startup
//   2. Call MenuAnimations::Update() every frame (before rendering menus)
//   3. Use MenuAnimations::GetSidePanelWidth() instead of fixed width
//   4. Use MenuAnimations::GetMonitoringBarHeight() instead of fixed height
//   5. Call MenuAnimations::StartOpen/StartClose when menu state changes
//   6. Check MenuAnimations::IsFullyClosed() before returning control to game
// =============================================================================

#pragma once

#include <Windows.h>

namespace MenuAnimations
{
    // =============================================================================
    // Animation State
    // =============================================================================

    enum class AnimationState
    {
        Closed,         // Fully closed (width/height = 0)
        Opening,        // Animating from closed to open
        Open,           // Fully open (full width/height)
        Closing         // Animating from open to closed
    };

    // =============================================================================
    // Initialization
    // =============================================================================

    // Initialize animation system
    void Init();

    // =============================================================================
    // Per-Frame Update
    // =============================================================================

    // Update animations - call every frame before rendering
    // deltaTime in seconds (use ImGui's io.DeltaTime)
    void Update(float deltaTime);

    // =============================================================================
    // SidePanel Animation
    // =============================================================================

    // Start opening animation for sidebar
    void StartSidePanelOpen();

    // Start closing animation for sidebar
    void StartSidePanelClose();

    // Get current animated width (0 to targetWidth)
    // Use this instead of fixed panelWidth in RenderSidePanel
    float GetSidePanelWidth();

    // Get animation progress (0.0 = closed, 1.0 = fully open)
    float GetSidePanelProgress();

    // Get current state
    AnimationState GetSidePanelState();

    // Check if sidebar is fully closed (safe to return control to game)
    bool IsSidePanelFullyClosed();

    // Check if sidebar is fully open
    bool IsSidePanelFullyOpen();

    // Check if sidebar is animating (opening or closing)
    bool IsSidePanelAnimating();

    // =============================================================================
    // MonitoringBar Animation
    // =============================================================================

    // Start opening animation for monitoring bar
    void StartMonitoringBarOpen();

    // Start closing animation for monitoring bar
    void StartMonitoringBarClose();

    // Get current animated height (0 to targetHeight)
    // Use this instead of fixed barHeight in RenderMonitoringBar
    float GetMonitoringBarHeight();

    // Get animation progress (0.0 = closed, 1.0 = fully open)
    float GetMonitoringBarProgress();

    // Get current state
    AnimationState GetMonitoringBarState();

    // Check if bar is fully closed
    bool IsMonitoringBarFullyClosed();

    // Check if bar is fully open
    bool IsMonitoringBarFullyOpen();

    // Check if bar is animating
    bool IsMonitoringBarAnimating();

    // =============================================================================
    // Combined State Helpers
    // =============================================================================

    // Returns true if ALL animated elements are fully closed
    // Use this to determine when to return control to game
    bool AreAllMenusFullyClosed();

    // Returns true if ANY element is currently animating
    bool IsAnyMenuAnimating();

    // =============================================================================
    // Configuration
    // =============================================================================

    // Set animation duration in seconds (default: 0.3s for snappy feel)
    void SetAnimationDuration(float seconds);

    // Get current animation duration
    float GetAnimationDuration();

    // Set target dimensions (if they differ from defaults)
    void SetSidePanelTargetWidth(float width);
    void SetMonitoringBarTargetHeight(float height);
}