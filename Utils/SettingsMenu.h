// =============================================================================
// SettingsMenu.h - ImGui Settings Menu with Sidebar Navigation
// =============================================================================

#pragma once

#include <Windows.h>

// UxImGui - Our isolated ImGui
#include "UxImGui/imgui.h"

#include <string>
#include <vector>
#include <functional>

namespace SettingsMenu
{
    // Menu categories
    enum class Category
    {
        Status,
        InfoBlock,
        Speedometer,
        Compass,
        General,
        COUNT
    };

    // Initialize the menu (call once)
    void Init();

    // Set target window for mouse coordinate conversion
    void SetTargetWindow(HWND hwnd);

    // Call when menu closes to restore cursor state
    void OnMenuClosed();

    // Check if menu is currently open
    bool IsMenuOpen();

    // Render the menu (call every frame when menu should be visible)
    void Render(bool* p_open = nullptr);

    // Render the monitoring bar (call every frame, independent of menu)
    // Shows only when ctx.isMonitoringEnabled == true
    void RenderMonitoringOverlay();

    // Get/Set current category
    Category GetCurrentCategory();
    void SetCurrentCategory(Category cat);

    // Side Panel (Graphics/Performance HUD)
    void SetSidePanelEnabled(bool enabled);
    bool IsSidePanelEnabled();
}

