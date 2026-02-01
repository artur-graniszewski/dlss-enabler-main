// =============================================================================
// SettingsMenu.cpp - ImGui Settings Menu with Sidebar Navigation
// =============================================================================

#include "SettingsMenu.h"
#include "../Core/Context.h"
#include "FpsMonitor.h"
#include "MenuAnimations.h"
#include "SettingsPersistence.h"
#include "OverdriveController.h"
#include "UxHook.h"
#include <cmath>

// Set to 0 to disable the main center settings window (only side panel visible)
#define ENABLE_MAIN_SETTINGS_WINDOW 0

namespace SettingsMenu
{
    // =============================================================================
    // State
    // =============================================================================

    static Category g_CurrentCategory = Category::General;
    static bool g_Initialized = false;
    static HWND g_TargetWindow = nullptr;  // Store target window for mouse coords
    static bool g_MenuWasOpen = false;     // Track if menu was open last frame
    static bool g_WasLmbDown = false;      // Track LMB state for click detection

    // Colors (matching the green theme from screenshot)
    namespace Colors
    {
        // Main colors
        static UxImVec4 Background = UxImVec4(0.05f, 0.08f, 0.10f, 0.95f);  // Dark blue-gray
        static UxImVec4 SidebarBg = UxImVec4(0.04f, 0.06f, 0.08f, 1.00f);  // Darker sidebar
        static UxImVec4 ContentBg = UxImVec4(0.06f, 0.09f, 0.11f, 1.00f);  // Content area

        // Accent colors (green theme)
        static UxImVec4 AccentGreen = UxImVec4(0.20f, 0.80f, 0.40f, 1.00f);  // Bright green
        static UxImVec4 AccentGreenDark = UxImVec4(0.15f, 0.60f, 0.30f, 1.00f);  // Darker green
        static UxImVec4 AccentGreenBg = UxImVec4(0.10f, 0.30f, 0.15f, 1.00f);  // Green background

        // Text colors
        static UxImVec4 TextPrimary = UxImVec4(1.00f, 1.00f, 1.00f, 1.00f);  // White
        static UxImVec4 TextSecondary = UxImVec4(0.60f, 0.65f, 0.70f, 1.00f);  // Gray
        static UxImVec4 TextDisabled = UxImVec4(0.40f, 0.45f, 0.50f, 1.00f);  // Dark gray

        // Button colors
        static UxImVec4 ButtonNormal = UxImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        static UxImVec4 ButtonHovered = UxImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        static UxImVec4 ButtonActive = UxImVec4(0.20f, 0.80f, 0.40f, 1.00f);

        // Toggle colors
        static UxImVec4 ToggleOff = UxImVec4(0.25f, 0.28f, 0.32f, 1.00f);
        static UxImVec4 ToggleOn = UxImVec4(0.20f, 0.80f, 0.40f, 1.00f);
    }

    // Category info
    struct CategoryInfo
    {
        const char* name;
        const char* icon;  // Unicode icon or letter
    };

    static CategoryInfo g_Categories[] = {
        { "Status",      "\xef\x80\x8d" },  // Font Awesome icons (if loaded) or use letters
        { "Info Block",  "\xef\x81\x9a" },
        { "Speedometer", "\xef\x83\xb4" },
        { "Compass",     "\xef\x81\x8e" },
        { "General",     "\xef\x80\x93" },
    };

    // =============================================================================
    // Helper Functions
    // =============================================================================

    static void PushStyleColors()
    {
        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, Colors::Background);
        UxImGui::PushStyleColor(UxImGuiCol_ChildBg, Colors::SidebarBg);
        UxImGui::PushStyleColor(UxImGuiCol_Text, Colors::TextPrimary);
        UxImGui::PushStyleColor(UxImGuiCol_Button, Colors::ButtonNormal);
        UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, Colors::ButtonHovered);
        UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, Colors::ButtonActive);
        UxImGui::PushStyleColor(UxImGuiCol_FrameBg, Colors::ButtonNormal);
        UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered, Colors::ButtonHovered);
        UxImGui::PushStyleColor(UxImGuiCol_FrameBgActive, Colors::ButtonActive);
        UxImGui::PushStyleColor(UxImGuiCol_Header, Colors::AccentGreenBg);
        UxImGui::PushStyleColor(UxImGuiCol_HeaderHovered, Colors::AccentGreenDark);
        UxImGui::PushStyleColor(UxImGuiCol_HeaderActive, Colors::AccentGreen);
    }

    static void PopStyleColors()
    {
        UxImGui::PopStyleColor(12);
    }

    // Custom toggle button using public API
    static bool ToggleButton(const char* str_id, bool* v)
    {
        UxImVec2 pos = UxImGui::GetCursorScreenPos();
        UxImDrawList* draw_list = UxImGui::GetWindowDrawList();

        const float height = UxImGui::GetFrameHeight();
        const float width = height * 1.8f;
        const float radius = height * 0.5f;

        UxImGui::InvisibleButton(str_id, UxImVec2(width, height));
        bool pressed = UxImGui::IsItemClicked();
        if (pressed)
            *v = !*v;

        // Background
        UxImU32 bg_col = *v ? UxImGui::ColorConvertFloat4ToU32(Colors::ToggleOn)
            : UxImGui::ColorConvertFloat4ToU32(Colors::ToggleOff);

        draw_list->AddRectFilled(pos, UxImVec2(pos.x + width, pos.y + height), bg_col, radius);

        // Circle
        float circle_x = *v ? (pos.x + width - radius) : (pos.x + radius);
        UxImVec2 circle_pos(circle_x, pos.y + radius);
        draw_list->AddCircleFilled(circle_pos, radius - 2.0f, IM_COL32(255, 255, 255, 255));

        return pressed;
    }

    // Styled menu item button for sidebar using MANUAL hit testing
    static bool MenuItemButton(const char* label, const char* icon, bool selected)
    {
        UxImVec2 pos = UxImGui::GetCursorScreenPos();
        UxImDrawList* draw_list = UxImGui::GetWindowDrawList();

        const float width = UxImGui::GetContentRegionAvail().x;
        const float height = 40.0f;

        // Manual hit testing
        POINT mouseScreenPos;
        GetCursorPos(&mouseScreenPos);
        if (g_TargetWindow)
            ScreenToClient(g_TargetWindow, &mouseScreenPos);

        UxImVec2 mousePos((float)mouseScreenPos.x, (float)mouseScreenPos.y);
        bool lmbDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool lmbClicked = lmbDown && !g_WasLmbDown;

        bool hovered = (mousePos.x >= pos.x && mousePos.x <= pos.x + width &&
            mousePos.y >= pos.y && mousePos.y <= pos.y + height);
        bool pressed = hovered && lmbClicked;

        // Reserve space
        UxImGui::Dummy(UxImVec2(width, height));

        // Background
        UxImU32 bg_col;
        if (selected)
            bg_col = UxImGui::ColorConvertFloat4ToU32(Colors::AccentGreenBg);
        else if (hovered)
            bg_col = UxImGui::ColorConvertFloat4ToU32(Colors::ButtonHovered);
        else
            bg_col = IM_COL32(0, 0, 0, 0);  // Transparent

        draw_list->AddRectFilled(pos, UxImVec2(pos.x + width, pos.y + height), bg_col, 4.0f);

        // Left accent bar if selected
        if (selected)
        {
            UxImVec2 bar_min = pos;
            UxImVec2 bar_max(pos.x + 3.0f, pos.y + height);
            draw_list->AddRectFilled(bar_min, bar_max,
                UxImGui::ColorConvertFloat4ToU32(Colors::AccentGreen), 2.0f);
        }

        // Icon (simple circle with letter)
        float icon_size = 24.0f;
        UxImVec2 icon_pos(pos.x + 12.0f, pos.y + (height - icon_size) * 0.5f);

        // Icon background circle
        UxImVec2 icon_center(icon_pos.x + icon_size * 0.5f, icon_pos.y + icon_size * 0.5f);
        UxImU32 icon_bg = selected ? UxImGui::ColorConvertFloat4ToU32(Colors::AccentGreen)
            : UxImGui::ColorConvertFloat4ToU32(Colors::ButtonNormal);
        draw_list->AddCircleFilled(icon_center, icon_size * 0.5f, icon_bg);

        // Icon letter (first letter of label)
        char icon_letter[2] = { label[0], 0 };
        UxImVec2 text_size = UxImGui::CalcTextSize(icon_letter);
        UxImVec2 icon_text_pos(icon_center.x - text_size.x * 0.5f, icon_center.y - text_size.y * 0.5f);
        draw_list->AddText(icon_text_pos, IM_COL32(255, 255, 255, 255), icon_letter);

        // Label text
        UxImVec2 label_pos(pos.x + 48.0f, pos.y + (height - UxImGui::GetTextLineHeight()) * 0.5f);
        UxImU32 text_col = selected ? UxImGui::ColorConvertFloat4ToU32(Colors::TextPrimary)
            : UxImGui::ColorConvertFloat4ToU32(Colors::TextSecondary);
        draw_list->AddText(label_pos, text_col, label);

        // Settings icons on the right (placeholder)
        float icons_x = pos.x + width - 50.0f;
        UxImU32 icon_col = UxImGui::ColorConvertFloat4ToU32(Colors::TextDisabled);
        draw_list->AddText(UxImVec2(icons_x, label_pos.y), icon_col, "...");

        return pressed;
    }

    // Option row with label and description using public API
    static void BeginOptionRow(const char* label, const char* description, const char* icon_letter)
    {
        UxImGui::PushID(label);

        UxImVec2 pos = UxImGui::GetCursorScreenPos();
        UxImDrawList* draw_list = UxImGui::GetWindowDrawList();

        // Option background
        float width = UxImGui::GetContentRegionAvail().x;
        float height = 60.0f;

        UxImU32 bg_col = UxImGui::ColorConvertFloat4ToU32(UxImVec4(0.08f, 0.11f, 0.14f, 1.0f));
        draw_list->AddRectFilled(pos, UxImVec2(pos.x + width, pos.y + height), bg_col, 6.0f);

        // Icon
        float icon_size = 32.0f;
        UxImVec2 icon_pos(pos.x + 15.0f, pos.y + (height - icon_size) * 0.5f);
        UxImVec2 icon_center(icon_pos.x + icon_size * 0.5f, icon_pos.y + icon_size * 0.5f);

        // Colored icon background based on first letter
        UxImVec4 icon_colors[] = {
            UxImVec4(0.9f, 0.3f, 0.3f, 1.0f),  // Red
            UxImVec4(0.2f, 0.6f, 0.9f, 1.0f),  // Blue
            UxImVec4(0.9f, 0.6f, 0.2f, 1.0f),  // Orange
            UxImVec4(0.5f, 0.3f, 0.8f, 1.0f),  // Purple
            UxImVec4(0.2f, 0.8f, 0.4f, 1.0f),  // Green
        };
        int color_idx = (icon_letter[0] % 5);
        draw_list->AddCircleFilled(icon_center, icon_size * 0.5f,
            UxImGui::ColorConvertFloat4ToU32(icon_colors[color_idx]));

        // Icon letter
        UxImVec2 text_size = UxImGui::CalcTextSize(icon_letter);
        UxImVec2 icon_text_pos(icon_center.x - text_size.x * 0.5f, icon_center.y - text_size.y * 0.5f);
        draw_list->AddText(icon_text_pos, IM_COL32(255, 255, 255, 255), icon_letter);

        // Label
        UxImVec2 label_pos(pos.x + 60.0f, pos.y + 12.0f);
        draw_list->AddText(label_pos,
            UxImGui::ColorConvertFloat4ToU32(Colors::TextPrimary), label);

        // Description
        if (description && description[0])
        {
            UxImVec2 desc_pos(pos.x + 60.0f, pos.y + 32.0f);
            draw_list->AddText(desc_pos,
                UxImGui::ColorConvertFloat4ToU32(Colors::TextSecondary), description);
        }

        // Set cursor for control on the right
        UxImGui::SetCursorScreenPos(UxImVec2(pos.x + width - 120.0f, pos.y + (height - 28.0f) * 0.5f));
    }

    static void EndOptionRow()
    {
        UxImGui::PopID();
        UxImGui::Dummy(UxImVec2(0, 70.0f));  // Spacing for next row
    }

    // Two-option toggle (like Off/On or MPH/KM/H)
    static int OptionToggle(const char* option1, const char* option2, int current)
    {
        int result = current;

        UxImGui::PushStyleVar(UxImGuiStyleVar_ItemSpacing, UxImVec2(0, 0));
        UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

        // Option 1
        bool is_opt1 = (current == 0);
        if (is_opt1)
        {
            UxImGui::PushStyleColor(UxImGuiCol_Button, Colors::AccentGreen);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, Colors::AccentGreen);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, Colors::AccentGreen);
        }
        else
        {
            UxImGui::PushStyleColor(UxImGuiCol_Button, Colors::ButtonNormal);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, Colors::ButtonHovered);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, Colors::ButtonActive);
        }

        if (UxImGui::Button(option1, UxImVec2(50, 28)))
            result = 0;

        UxImGui::PopStyleColor(3);

        UxImGui::SameLine();

        // Option 2
        bool is_opt2 = (current == 1);
        if (is_opt2)
        {
            UxImGui::PushStyleColor(UxImGuiCol_Button, Colors::AccentGreen);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, Colors::AccentGreen);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, Colors::AccentGreen);
        }
        else
        {
            UxImGui::PushStyleColor(UxImGuiCol_Button, Colors::ButtonNormal);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, Colors::ButtonHovered);
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, Colors::ButtonActive);
        }

        if (UxImGui::Button(option2, UxImVec2(50, 28)))
            result = 1;

        UxImGui::PopStyleColor(3);
        UxImGui::PopStyleVar(2);

        return result;
    }

    // =============================================================================
    // Content Rendering for each category
    // =============================================================================

    static void RenderGeneralContent()
    {
        UxImDrawList* draw_list = UxImGui::GetWindowDrawList();
        UxImVec2 windowPos = UxImGui::GetWindowPos();
        UxImVec2 contentStart = UxImGui::GetCursorScreenPos();

        // Header
        UxImGui::TextColored(Colors::TextPrimary, "General");
        UxImGui::TextColored(Colors::TextSecondary, "Adjust various HUD settings.");

        // Reset buttons in top right
        UxImGui::SameLine(UxImGui::GetContentRegionAvail().x - 220);
        UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.6f, 0.2f, 0.2f, 0.3f));
        UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.9f, 0.4f, 0.4f, 1.0f));
        UxImGui::SmallButton("Hold to Reset Section");
        UxImGui::SameLine();
        UxImGui::SmallButton("Hold");
        UxImGui::PopStyleColor(2);

        UxImGui::Dummy(UxImVec2(0, 15));

        // Options 
        static int performance_mode = 0;
        static int speed_type = 1;
        static int map_style = 0;
        static int map_visibility = 1;
        static int compass_visibility = 1;
        static int cinematic_bars = 0;

        float contentWidth = UxImGui::GetContentRegionAvail().x;
        float optionWidth = (contentWidth - 15) / 2.0f;  // Two columns with gap
        float optionHeight = 65.0f;

        // Helper lambda for drawing option card with MANUAL hit testing
        auto DrawOptionCard = [&](const char* label, const char* description, const char* iconLetter,
            const char* opt1, const char* opt2, int* value, UxImVec4 iconColor)
            {
                UxImVec2 pos = UxImGui::GetCursorScreenPos();

                // Card background
                UxImU32 cardBg = UxImGui::ColorConvertFloat4ToU32(UxImVec4(0.08f, 0.11f, 0.14f, 1.0f));
                draw_list->AddRectFilled(pos, UxImVec2(pos.x + optionWidth, pos.y + optionHeight), cardBg, 6.0f);

                // Icon circle
                float iconSize = 36.0f;
                UxImVec2 iconCenter(pos.x + 25.0f, pos.y + optionHeight / 2.0f);
                draw_list->AddCircleFilled(iconCenter, iconSize / 2.0f, UxImGui::ColorConvertFloat4ToU32(iconColor));

                // Icon letter
                UxImVec2 letterSize = UxImGui::CalcTextSize(iconLetter);
                draw_list->AddText(UxImVec2(iconCenter.x - letterSize.x / 2, iconCenter.y - letterSize.y / 2),
                    IM_COL32(255, 255, 255, 255), iconLetter);

                // Label
                draw_list->AddText(UxImVec2(pos.x + 55.0f, pos.y + 15.0f),
                    UxImGui::ColorConvertFloat4ToU32(Colors::TextPrimary), label);

                // Description  
                draw_list->AddText(UxImVec2(pos.x + 55.0f, pos.y + 35.0f),
                    UxImGui::ColorConvertFloat4ToU32(Colors::TextSecondary), description);

                // Toggle buttons - MANUAL HIT TESTING
                float btnWidth = 45.0f;
                float btnHeight = 24.0f;
                float btnX = pos.x + optionWidth - btnWidth * 2 - 15.0f;
                float btnY = pos.y + (optionHeight - btnHeight) / 2.0f;

                // Get mouse state directly
                POINT mouseScreenPos;
                GetCursorPos(&mouseScreenPos);
                if (g_TargetWindow)
                    ScreenToClient(g_TargetWindow, &mouseScreenPos);

                UxImVec2 mousePos((float)mouseScreenPos.x, (float)mouseScreenPos.y);
                bool lmbDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                bool lmbClicked = lmbDown && !g_WasLmbDown;

                // Button 1 bounds
                UxImVec2 btn1Min(btnX, btnY);
                UxImVec2 btn1Max(btnX + btnWidth, btnY + btnHeight);
                bool hover1 = (mousePos.x >= btn1Min.x && mousePos.x <= btn1Max.x &&
                    mousePos.y >= btn1Min.y && mousePos.y <= btn1Max.y);

                // Button 2 bounds
                UxImVec2 btn2Min(btnX + btnWidth + 2, btnY);
                UxImVec2 btn2Max(btnX + btnWidth * 2 + 2, btnY + btnHeight);
                bool hover2 = (mousePos.x >= btn2Min.x && mousePos.x <= btn2Max.x &&
                    mousePos.y >= btn2Min.y && mousePos.y <= btn2Max.y);

                // Handle clicks
                if (lmbClicked)
                {
                    if (hover1) *value = 0;
                    if (hover2) *value = 1;
                }

                // Draw button 1
                bool isOpt1 = (*value == 0);
                UxImU32 btn1Col = isOpt1 ? UxImGui::ColorConvertFloat4ToU32(Colors::AccentGreen) :
                    hover1 ? UxImGui::ColorConvertFloat4ToU32(Colors::ButtonHovered) :
                    UxImGui::ColorConvertFloat4ToU32(Colors::ButtonNormal);
                draw_list->AddRectFilled(btn1Min, btn1Max, btn1Col, 4.0f);
                UxImVec2 txt1Size = UxImGui::CalcTextSize(opt1);
                draw_list->AddText(UxImVec2(btn1Min.x + (btnWidth - txt1Size.x) / 2, btn1Min.y + (btnHeight - txt1Size.y) / 2),
                    IM_COL32(255, 255, 255, 255), opt1);

                // Draw button 2
                bool isOpt2 = (*value == 1);
                UxImU32 btn2Col = isOpt2 ? UxImGui::ColorConvertFloat4ToU32(Colors::AccentGreen) :
                    hover2 ? UxImGui::ColorConvertFloat4ToU32(Colors::ButtonHovered) :
                    UxImGui::ColorConvertFloat4ToU32(Colors::ButtonNormal);
                draw_list->AddRectFilled(btn2Min, btn2Max, btn2Col, 4.0f);
                UxImVec2 txt2Size = UxImGui::CalcTextSize(opt2);
                draw_list->AddText(UxImVec2(btn2Min.x + (btnWidth - txt2Size.x) / 2, btn2Min.y + (btnHeight - txt2Size.y) / 2),
                    IM_COL32(255, 255, 255, 255), opt2);

                // Move cursor for next item
                UxImGui::SetCursorScreenPos(UxImVec2(pos.x, pos.y + optionHeight + 10.0f));
            };

        // Row 1
        UxImVec2 row1Start = UxImGui::GetCursorScreenPos();

        DrawOptionCard("Performance Mode", "Change HUD game performance.", "P",
            "Off", "On", &performance_mode, UxImVec4(0.9f, 0.3f, 0.3f, 1.0f));

        UxImGui::SetCursorScreenPos(UxImVec2(row1Start.x + optionWidth + 15.0f, row1Start.y));

        DrawOptionCard("Speed Type", "Choose KMH / MPH", "S",
            "MP/H", "KM/H", &speed_type, UxImVec4(0.9f, 0.3f, 0.3f, 1.0f));

        // Row 2
        UxImGui::SetCursorScreenPos(UxImVec2(row1Start.x, row1Start.y + optionHeight + 10.0f));
        UxImVec2 row2Start = UxImGui::GetCursorScreenPos();

        DrawOptionCard("Map Style", "Choose Circle / Rectangle", "M",
            "Rect", "Circle", &map_style, UxImVec4(0.2f, 0.6f, 0.9f, 1.0f));

        UxImGui::SetCursorScreenPos(UxImVec2(row2Start.x + optionWidth + 15.0f, row2Start.y));

        DrawOptionCard("Map Visibility", "Choose when to show the map.", "V",
            "Always", "In Car", &map_visibility, UxImVec4(0.5f, 0.3f, 0.8f, 1.0f));

        // Row 3
        UxImGui::SetCursorScreenPos(UxImVec2(row1Start.x, row2Start.y + optionHeight + 10.0f));
        UxImVec2 row3Start = UxImGui::GetCursorScreenPos();

        DrawOptionCard("Compass Visibility", "Choose when to show the compass.", "C",
            "Always", "In Car", &compass_visibility, UxImVec4(0.9f, 0.6f, 0.2f, 1.0f));

        UxImGui::SetCursorScreenPos(UxImVec2(row3Start.x + optionWidth + 15.0f, row3Start.y));

        DrawOptionCard("Cinematic Bars", "Toggle cinematic bars on or off.", "B",
            "Off", "On", &cinematic_bars, UxImVec4(0.5f, 0.3f, 0.8f, 1.0f));
    }

    static void RenderStatusContent()
    {
        UxImGui::TextColored(Colors::TextPrimary, "Status");
        UxImGui::TextColored(Colors::TextSecondary, "Configure status display options.");
        UxImGui::Dummy(UxImVec2(0, 20));

        UxImGui::Text("Status options will appear here...");
    }

    static void RenderInfoBlockContent()
    {
        UxImGui::TextColored(Colors::TextPrimary, "Info Block");
        UxImGui::TextColored(Colors::TextSecondary, "Configure information block settings.");
        UxImGui::Dummy(UxImVec2(0, 20));

        UxImGui::Text("Info Block options will appear here...");
    }

    static void RenderSpeedometerContent()
    {
        UxImGui::TextColored(Colors::TextPrimary, "Speedometer");
        UxImGui::TextColored(Colors::TextSecondary, "Configure speedometer display.");
        UxImGui::Dummy(UxImVec2(0, 20));

        UxImGui::Text("Speedometer options will appear here...");
    }

    static void RenderCompassContent()
    {
        UxImGui::TextColored(Colors::TextPrimary, "Compass");
        UxImGui::TextColored(Colors::TextSecondary, "Configure compass display.");
        UxImGui::Dummy(UxImVec2(0, 20));

        UxImGui::Text("Compass options will appear here...");
    }

    // =============================================================================
    // SettingsMenu.cpp - PATCH FILE
    // =============================================================================
    // This file contains the modified sections for SettingsMenu.cpp
    // Replace the corresponding sections in your original file with these versions
    // =============================================================================

    // =============================================================================
    // REPLACE: MonitoringBar namespace (around line 515)
    // =============================================================================

    namespace MonitoringBar
    {
        // Cached values for refresh (updated every 1 second)
        static int g_CachedCurrentFps = 0;
        static int g_CachedPotentialFps = 0;

        // Cached latency value (updated every 1 second)
        static double g_CachedLatencyMs = 0.0;
        static bool g_CachedHasLatencyData = false;

        // Cached status values (updated every 1 second)
        static int g_CachedFgMultiplier = 0;        // 0=OFF, 1-4 = multiplier
        static bool g_CachedFgIsAuto = false;       // Dynamic FG mode
        static char g_CachedUsStatusText[16] = "OFF";
        static UxImVec4 g_CachedUsStatusColor = UxImVec4(0.6f, 0.3f, 0.3f, 1.0f);
        static int g_CachedFramesGenerated = 0;

        static ULONGLONG g_LastUpdateTime = 0;
    }

    // =============================================================================
    // REPLACE: RenderMonitoringBar function (around line 532)
    // Replace the entire function with this version
    // =============================================================================

    static void RenderMonitoringBar()
    {
        // Track previous state to detect changes
        static bool wasMonitoringEnabled = false;
        static bool firstFrame = true;

        // Initialize on first frame - if monitoring is already enabled at startup, start animation
        if (firstFrame)
        {
            if (ctx.isMonitoringEnabled)
            {
                MenuAnimations::StartMonitoringBarOpen();
            }
            firstFrame = false;
            wasMonitoringEnabled = ctx.isMonitoringEnabled;
        }

        // Detect state change and trigger animation
        if (ctx.isMonitoringEnabled && !wasMonitoringEnabled)
        {
            // Just enabled - start open animation
            MenuAnimations::StartMonitoringBarOpen();
        }
        else if (!ctx.isMonitoringEnabled && wasMonitoringEnabled)
        {
            // Just disabled - start close animation
            MenuAnimations::StartMonitoringBarClose();
        }
        wasMonitoringEnabled = ctx.isMonitoringEnabled;

        // Get animated height (Update is called in RenderFrame)
        const float barHeight = MenuAnimations::GetMonitoringBarHeight();

        // Don't render if fully closed or too small
        if (barHeight < 1.0f && MenuAnimations::IsMonitoringBarFullyClosed())
            return;

        UxImGuiIO& io = UxImGui::GetIO();

        ULONGLONG currentTime = GetTickCount64();

        // Update all cached values every 1000ms
        if (currentTime - MonitoringBar::g_LastUpdateTime >= 1000)
        {
            MonitoringBar::g_CachedCurrentFps = FpsMonitor::GetCurrentFps();
            MonitoringBar::g_CachedPotentialFps = FpsMonitor::GetPotentialFps();

            // Cache latency data
            MonitoringBar::g_CachedHasLatencyData = FpsMonitor::HasLatencyData();
            if (MonitoringBar::g_CachedHasLatencyData)
            {
                MonitoringBar::g_CachedLatencyMs = FpsMonitor::GetAverageLatencyMs();
            }

            // Cache frames generated count
            MonitoringBar::g_CachedFramesGenerated = ctx.ngx.framesGenerated;

            // Calculate FG status: OFF, 1X, 2X, 3X, 4X (with optional "(AUTO)")
            if (!ctx.ngx.isFrameGenerationActive)
            {
                // FG completely off
                MonitoringBar::g_CachedFgMultiplier = 0;
                MonitoringBar::g_CachedFgIsAuto = false;
            }
            else if (ctx.ngx.isDynamicFrameGenerationEnabled)
            {
                // Dynamic FG mode
                MonitoringBar::g_CachedFgIsAuto = true;
                if (MonitoringBar::g_CachedFramesGenerated > 0)
                {
                    MonitoringBar::g_CachedFgMultiplier = MonitoringBar::g_CachedFramesGenerated + 1;
                }
                else
                {
                    MonitoringBar::g_CachedFgMultiplier = 1;
                }
            }
            else
            {
                // FG always on
                MonitoringBar::g_CachedFgMultiplier = MonitoringBar::g_CachedFramesGenerated + 1;
                MonitoringBar::g_CachedFgIsAuto = false;
            }

            // Calculate US status based on upscaling quality
            // NVSDK_NGX_PerfQuality_Value enum:
            // 0 = MaxPerf, 1 = Balanced, 2 = MaxQuality, 
            // 3 = UltraPerformance, 4 = UltraQuality, 5 = DLAA
            if (!ctx.ngx.isUpscalingActive)
            {
                strcpy_s(MonitoringBar::g_CachedUsStatusText, "OFF");
                MonitoringBar::g_CachedUsStatusColor = UxImVec4(0.6f, 0.3f, 0.3f, 1.0f);
            }
            else
            {
                switch (ctx.ngx.upscalingQuality)
                {
                case 0:  // MaxPerf
                    strcpy_s(MonitoringBar::g_CachedUsStatusText, "Perf");
                    break;
                case 1:  // Balanced
                    strcpy_s(MonitoringBar::g_CachedUsStatusText, "Balanced");
                    break;
                case 2:  // MaxQuality
                    strcpy_s(MonitoringBar::g_CachedUsStatusText, "Quality");
                    break;
                case 3:  // UltraPerformance
                    strcpy_s(MonitoringBar::g_CachedUsStatusText, "UltraP");
                    break;
                case 4:  // UltraQuality
                    strcpy_s(MonitoringBar::g_CachedUsStatusText, "UltraQ");
                    break;
                case 5:  // DLAA
                    strcpy_s(MonitoringBar::g_CachedUsStatusText, "AA");
                    break;
                default:
                    strcpy_s(MonitoringBar::g_CachedUsStatusText, "ON");
                    break;
                }
                MonitoringBar::g_CachedUsStatusColor = UxImVec4(0.0f, 0.8f, 0.6f, 1.0f);
            }

            MonitoringBar::g_LastUpdateTime = currentTime;
        }

        // Bar dimensions
        const float barMargin = 10.0f;
        const float barWidth = 480.0f;  // Wide enough for FPS/Latency + FG options + US + debug

        // Position: top center with 10px margin from top
        UxImVec2 barPos((io.DisplaySize.x - barWidth) * 0.5f, barMargin);

        UxImGui::SetNextWindowPos(barPos);
        UxImGui::SetNextWindowSize(UxImVec2(barWidth, barHeight));

        // Styling
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, 6.0f);
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(12, 6));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, 0.0f);

        // Semi-transparent dark background
        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0.02f, 0.05f, 0.08f, 0.85f));

        UxImGuiWindowFlags flags = UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoScrollbar |
            UxImGuiWindowFlags_NoCollapse |
            UxImGuiWindowFlags_NoInputs;

        if (UxImGui::Begin("##MonitoringBar", nullptr, flags))
        {
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // Draw accent line on bottom
            UxImU32 accentColor = IM_COL32(0, 200, 180, 255);
            drawList->AddRectFilled(
                UxImVec2(windowPos.x, windowPos.y + windowSize.y - 2.0f),
                UxImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                accentColor
            );

            // FPS display with optional latency (fixed width section)
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("FPS:");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            UxImGui::Text("%3d", MonitoringBar::g_CachedCurrentFps);
            UxImGui::PopStyleColor();

            // Latency suffix (if available): " / XXX ms" - use fixed width format
            // Only show if real NVAPI is used (not embedded/fake)
            bool showLatency = MonitoringBar::g_CachedHasLatencyData &&
                MonitoringBar::g_CachedLatencyMs > 0.0 &&
                !ctx.nvapi.isEmbeddedNvapiUsed;
            if (showLatency)
            {
                UxImGui::SameLine();
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));  // Dimmer
                UxImGui::Text("/ %4.0f ms", MonitoringBar::g_CachedLatencyMs);
                UxImGui::PopStyleColor();
            }
            else if (!ctx.nvapi.isEmbeddedNvapiUsed)
            {
                // Reserve space for latency when not showing (keeps layout stable)
                // Only if real NVAPI - no need to reserve space for embedded
                UxImGui::SameLine();
                UxImGui::Dummy(UxImVec2(70, 0));  // Approximate width of "/ XXXX ms"
            }

            UxImGui::SameLine();
            UxImGui::Dummy(UxImVec2(4, 0));
            UxImGui::SameLine();

            // Separator
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.3f, 0.35f, 0.4f, 1.0f));
            UxImGui::Text("|");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();
            UxImGui::Dummy(UxImVec2(4, 0));
            UxImGui::SameLine();

            // Frame Generation status: "FG: 2X 3X 4X AUTO"
            // Active option is highlighted, others are dimmed
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("FG:");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            // Colors
            UxImVec4 dimColor = UxImVec4(0.3f, 0.35f, 0.4f, 1.0f);      // Dimmed/inactive
            UxImVec4 activeColor = UxImVec4(0.0f, 0.8f, 0.6f, 1.0f);    // Active multiplier (green)
            UxImVec4 orangeColor = UxImVec4(0.9f, 0.6f, 0.2f, 1.0f);    // Orange (AUTO waiting)

            // 2X
            UxImGui::PushStyleColor(UxImGuiCol_Text, (MonitoringBar::g_CachedFgMultiplier == 2) ? activeColor : dimColor);
            UxImGui::Text("2X");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            // 3X
            UxImGui::PushStyleColor(UxImGuiCol_Text, (MonitoringBar::g_CachedFgMultiplier == 3) ? activeColor : dimColor);
            UxImGui::Text("3X");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            // 4X
            UxImGui::PushStyleColor(UxImGuiCol_Text, (MonitoringBar::g_CachedFgMultiplier == 4) ? activeColor : dimColor);
            UxImGui::Text("4X");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            // AUTO - color logic:
            // Gray: isDynamicFrameGenerationEnabled == false OR isFrameGenerationActive == false
            // Orange: AUTO enabled, FG active, but duplicating frames (not generating, blocking due to threshold)
            // Green: AUTO enabled, FG active, and not duplicating (generating normally)
            UxImVec4 autoColor = dimColor;
            if (ctx.ngx.isDynamicFrameGenerationEnabled && ctx.ngx.isFrameGenerationActive)
            {
                if (ctx.ngx.isDuplicatingFrames)
                {
                    autoColor = orangeColor;  // Orange - duplicating/blocking
                }
                else
                {
                    autoColor = activeColor;  // Green - generating normally
                }
            }

            UxImGui::PushStyleColor(UxImGuiCol_Text, autoColor);
            UxImGui::Text("AUTO");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();
            UxImGui::Dummy(UxImVec2(4, 0));
            UxImGui::SameLine();

            // Separator
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.3f, 0.35f, 0.4f, 1.0f));
            UxImGui::Text("|");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();
            UxImGui::Dummy(UxImVec2(4, 0));
            UxImGui::SameLine();

            // Upscaling status (uses cached values, updated every 1s)
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("US:");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            UxImGui::PushStyleColor(UxImGuiCol_Text, MonitoringBar::g_CachedUsStatusColor);
            UxImGui::Text("%-9s", MonitoringBar::g_CachedUsStatusText);
            UxImGui::PopStyleColor();
        }
        UxImGui::End();

        UxImGui::PopStyleColor();  // WindowBg
        UxImGui::PopStyleVar(3);
    }

    // =============================================================================
    // Side Panel (Graphics/Performance HUD) - Sci-Fi Style
    // Uses ctx from Context.h for all settings
    // =============================================================================

    namespace SidePanel
    {
        // Only keep UI-specific state here, all settings come from ctx
        static bool g_Enabled = true;  // Will be synced with ctx.isSideBarEnabled

        // Popup vertical slider state
        static bool g_ShowRayTracingRangeSlider = false;
        static float g_RayTracingRangeSliderY = 0.0f;
        static bool g_ShowIlluminationSlider = false;
        static float g_IlluminationSliderY = 0.0f;
        static bool g_ShowOcclusionSlider = false;
        static float g_OcclusionSliderY = 0.0f;
    }

    static void RenderSidePanel()
    {
        // Sync with context
        SidePanel::g_Enabled = ctx.isSideBarEnabled;

        if (!SidePanel::g_Enabled)
            return;

        // Get animated width
        const float panelWidth = MenuAnimations::GetSidePanelWidth();
        if (panelWidth < 1.0f)
            return;  // Don't render if too small

        UxImGuiIO& io = UxImGui::GetIO();

        // Panel dimensions - dynamic height based on screen resolution
        const float panelMargin = 10.0f;
        const float persistPromptHeight = 85.0f;  // Height reserved for "Settings Changed" prompt
        const float maxPanelHeight = io.DisplaySize.y - panelMargin * 2 - persistPromptHeight;
        const float desiredPanelHeight = 750.0f;  // Desired height for content
        const float panelHeight = (desiredPanelHeight < maxPanelHeight) ? desiredPanelHeight : maxPanelHeight;

        // Position: top-left with 10px margin
        UxImVec2 panelPos(panelMargin, panelMargin);

        UxImGui::SetNextWindowPos(panelPos);
        UxImGui::SetNextWindowSize(UxImVec2(panelWidth, panelHeight));

        // Styling
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, 8.0f);
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(0, 0));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, 0.0f);
        UxImGui::PushStyleVar(UxImGuiStyleVar_ItemSpacing, UxImVec2(8, 4));

        // Scrollbar styling
        UxImGui::PushStyleVar(UxImGuiStyleVar_ScrollbarSize, 8.0f);
        UxImGui::PushStyleVar(UxImGuiStyleVar_ScrollbarRounding, 4.0f);
        UxImGui::PushStyleColor(UxImGuiCol_ScrollbarBg, UxImVec4(0.02f, 0.05f, 0.08f, 0.5f));
        UxImGui::PushStyleColor(UxImGuiCol_ScrollbarGrab, UxImVec4(0.2f, 0.25f, 0.3f, 0.8f));
        UxImGui::PushStyleColor(UxImGuiCol_ScrollbarGrabHovered, UxImVec4(0.3f, 0.35f, 0.4f, 1.0f));
        UxImGui::PushStyleColor(UxImGuiCol_ScrollbarGrabActive, UxImVec4(0.0f, 0.6f, 0.55f, 1.0f));

        // Semi-transparent dark background with slight blue tint
        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0.02f, 0.05f, 0.08f, 0.85f));

        // Allow vertical scrolling
        UxImGuiWindowFlags flags = UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoCollapse;

        if (UxImGui::Begin("##SidePanel", nullptr, flags))
        {
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // =====================================================
            // Draw decorative left edge (cyan/teal accent line)
            // =====================================================
            UxImU32 accentColor = IM_COL32(0, 200, 180, 255);  // Teal/cyan

            // Vertical accent line on left
            drawList->AddRectFilled(
                windowPos,
                UxImVec2(windowPos.x + 3.0f, windowPos.y + windowSize.y),
                accentColor
            );

            // Subtle glow effect
            for (int i = 0; i < 3; i++)
            {
                float alpha = 0.15f - (i * 0.05f);
                drawList->AddRectFilled(
                    UxImVec2(windowPos.x + 3.0f + i * 2.0f, windowPos.y),
                    UxImVec2(windowPos.x + 5.0f + i * 2.0f, windowPos.y + windowSize.y),
                    IM_COL32(0, 200, 180, (int)(alpha * 255))
                );
            }

            // Content padding
            UxImGui::SetCursorPos(UxImVec2(15.0f, 12.0f));

            // Colors for enabled/disabled states
            UxImVec4 textEnabled = UxImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            UxImVec4 textDisabled = UxImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            UxImVec4 sliderBgDisabled = UxImVec4(0.08f, 0.08f, 0.1f, 1.0f);

            // Helper lambda for toggle button
            auto DrawToggle = [&](const char* label, bool* value, bool enabled = true) -> bool
                {
                    bool changed = false;
                    UxImGui::SetCursorPosX(15.0f);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, enabled ? textEnabled : textDisabled);
                    UxImGui::Text("%s", label);
                    UxImGui::PopStyleColor();

                    UxImGui::SameLine(panelWidth - 55.0f);

                    const char* btnLabel = *value ? "ON" : "OFF";
                    UxImVec4 bgColor;
                    if (!enabled)
                        bgColor = UxImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                    else if (*value)
                        bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f);
                    else
                        bgColor = UxImVec4(0.2f, 0.22f, 0.25f, 1.0f);

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, enabled ? ((*value) ? UxImVec4(0.0f, 0.7f, 0.65f, 1.0f) : UxImVec4(0.25f, 0.28f, 0.32f, 1.0f)) : bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, enabled ? UxImVec4(1.0f, 1.0f, 1.0f, 1.0f) : textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

                    UxImGui::PushID(label);
                    if (enabled && UxImGui::Button(btnLabel, UxImVec2(40.0f, 22.0f)))
                    {
                        *value = !*value;
                        changed = true;
                    }
                    else if (!enabled)
                    {
                        UxImGui::Button(btnLabel, UxImVec2(40.0f, 22.0f));
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                    return changed;
                };

            // Helper lambda for combo box
            auto DrawCombo = [&](const char* label, int* value, const char* items[], int itemCount, bool enabled = true)
                {
                    UxImGui::SetCursorPosX(15.0f);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, enabled ? textEnabled : textDisabled);
                    UxImGui::Text("%s", label);
                    UxImGui::PopStyleColor();

                    UxImGui::SameLine(panelWidth - 90.0f);
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBg, enabled ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered, enabled ? UxImVec4(0.15f, 0.18f, 0.22f, 1.0f) : sliderBgDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);
                    UxImGui::PushItemWidth(75.0f);

                    UxImGui::PushID(label);
                    if (enabled)
                        UxImGui::Combo("##combo", value, items, itemCount);
                    else
                    {
                        UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                        UxImGui::Combo("##combo", value, items, itemCount);
                        UxImGui::PopStyleColor();
                    }
                    UxImGui::PopID();

                    UxImGui::PopItemWidth();
                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(2);
                };

            // Helper lambda for slider
            auto DrawSlider = [&](const char* label, int* value, bool enabled = true)
                {
                    UxImGui::SetCursorPosX(15.0f);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, enabled ? textEnabled : textDisabled);
                    UxImGui::Text("%s", label);
                    UxImGui::PopStyleColor();

                    UxImGui::SetCursorPosX(15.0f);
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBg, enabled ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                    UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, enabled ? UxImVec4(0.0f, 0.75f, 0.7f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, enabled ? UxImVec4(0.0f, 0.85f, 0.8f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, 4.0f);
                    UxImGui::PushItemWidth(panelWidth - 30.0f);

                    UxImGui::PushID(label);
                    if (enabled)
                    {
                        UxImGui::SliderInt("##slider", value, 1, 100, "%d");
                    }
                    else
                    {
                        // Disabled slider - just display, no interaction
                        UxImGui::BeginDisabled(true);
                        UxImGui::SliderInt("##slider", value, 1, 100, "%d");
                        UxImGui::EndDisabled();
                    }
                    UxImGui::PopID();

                    UxImGui::PopItemWidth();
                    UxImGui::PopStyleVar(2);
                    UxImGui::PopStyleColor(3);
                };

            // =====================================================
            // GRAPHICS Section
            // =====================================================
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("GRAPHICS");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 6));

            // Screen Space Ray Tracing - only available in D3D12 with DLSS Upscaling active
            // Check if SSRT is available: must be D3D12 mode and DLSS upscaling active
            bool isD3D12Mode = (UxHook::GetDetectedAPI() == UxHook::GraphicsAPI::D3D12);
            bool ssrtAvailable = isD3D12Mode && ctx.ngx.isUpscalingActive;

            if (ssrtAvailable)
            {
                // Normal toggle when available
                DrawToggle("Screen Space Ray Tracing", &ctx.ngx.isScreenSpaceRayTracingEnabled);
            }
            else
            {
                // Show N/A with tooltip when not available
                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                UxImGui::Text("Screen Space Ray Tracing");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - 55.0f);

                UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

                UxImGui::Button("N/A", UxImVec2(40.0f, 22.0f));

                // Tooltip on hover - different message based on reason
                if (UxImGui::IsItemHovered())
                {
                    UxImGui::BeginTooltip();
                    if (!isD3D12Mode)
                        UxImGui::Text("Currently available only in D3D12 mode");
                    else
                        UxImGui::Text("Requires DLSS Upscaling to be active");
                    UxImGui::EndTooltip();
                }

                UxImGui::PopStyleVar();
                UxImGui::PopStyleColor(4);

                // NOTE: Do NOT reset isScreenSpaceRayTracingEnabled here!
                // The flag preserves INI value - SSRTGI simply won't execute when unavailable
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            bool rtEnabled = ssrtAvailable && ctx.ngx.isScreenSpaceRayTracingEnabled;

            // Ray Tracing Quality (combo) -> ctx.ngx.illuminationQuality
            // Disabled when Overdrive is active (mode controls quality)
            bool rtQualityEnabled = rtEnabled && !OverdriveController::IsActive();
            const char* qualityItems[] = { "ULTRA", "HIGH", "MEDIUM", "LOW" };
            DrawCombo("Ray Tracing quality", &ctx.ngx.rayTracingQuality, qualityItems, 4, rtQualityEnabled);
            if (OverdriveController::IsActive() && rtEnabled && UxImGui::IsItemHovered())
            {
                UxImGui::BeginTooltip();
                UxImGui::Text("Controlled by Overdrive mode");
                UxImGui::EndTooltip();
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Ray Tracing range -> ctx.ngx.rayTracingRange
            DrawSlider("Ray Tracing range", &ctx.ngx.rayTracingRange, rtEnabled);

            UxImGui::Dummy(UxImVec2(0, 4));

            // Global Illumination -> ctx.ngx.isGlobalIlluminationEnabled
            DrawToggle("Global Illumination", &ctx.ngx.isGlobalIlluminationEnabled, rtEnabled);

            UxImGui::Dummy(UxImVec2(0, 4));

            // Illumination Strength -> ctx.ngx.illuminationStrength
            bool giEnabled = rtEnabled && ctx.ngx.isGlobalIlluminationEnabled;
            DrawSlider("Illumination strength", &ctx.ngx.illuminationStrength, giEnabled);

            UxImGui::Dummy(UxImVec2(0, 6));

            // Ambient Occlusion -> ctx.ngx.isAmbientOcclusionEnabled
            // AO is independent from GI - only requires RT to be enabled
            DrawToggle("Contact shadows", &ctx.ngx.isAmbientOcclusionEnabled, rtEnabled);

            UxImGui::Dummy(UxImVec2(0, 4));

            // Occlusion Strength -> ctx.ngx.occlusionStrength
            bool aoStrengthEnabled = rtEnabled && ctx.ngx.isAmbientOcclusionEnabled;
            DrawSlider("Shadows strength", &ctx.ngx.occlusionStrength, aoStrengthEnabled);

            // =====================================================
            // Separator line
            // =====================================================
            UxImGui::Dummy(UxImVec2(0, 8));
            UxImGui::SetCursorPosX(15.0f);
            drawList->AddLine(
                UxImVec2(windowPos.x + 15.0f, UxImGui::GetCursorScreenPos().y),
                UxImVec2(windowPos.x + panelWidth - 15.0f, UxImGui::GetCursorScreenPos().y),
                IM_COL32(60, 70, 80, 255)
            );
            UxImGui::Dummy(UxImVec2(0, 12));

            // =====================================================
            // PERFORMANCE Section
            // =====================================================
            UxImGui::SetCursorPosX(15.0f);
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("PERFORMANCE");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 6));

            // V-SYNC - Three state toggle: APP (game controls) / ON / OFF
            // Works even if game uses ALLOW_TEARING - we override it
            // Disabled when Overdrive is active
            {
                bool vsyncControlEnabled = !OverdriveController::IsActive();

                UxImGui::SetCursorPosX(15.0f);

                bool isOverrideEnabled = ctx.reflex.isVsyncOverrideEnabled;
                bool isVsyncOn = ctx.reflex.isVsyncEnabled;

                UxImGui::PushStyleColor(UxImGuiCol_Text, vsyncControlEnabled ? textEnabled : textDisabled);
                UxImGui::Text("V-SYNC");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - 55.0f);

                // Determine button label and color
                const char* btnLabel;
                UxImVec4 bgColor;
                UxImVec4 hoverColor;

                if (!vsyncControlEnabled) {
                    // Overdrive active - show current override state but disabled
                    bool odVsyncOverride = OverdriveController::GetVsyncOverrideEnabled();
                    bool odVsync = OverdriveController::GetVsyncEnabled();
                    if (!odVsyncOverride) {
                        btnLabel = "APP";
                    }
                    else if (odVsync) {
                        btnLabel = "ON";
                    }
                    else {
                        btnLabel = "OFF";
                    }
                    bgColor = UxImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                    hoverColor = bgColor;
                }
                else if (!isOverrideEnabled) {
                    // APP mode - game controls VSync
                    btnLabel = "APP";
                    bgColor = UxImVec4(0.4f, 0.4f, 0.15f, 1.0f);        // Yellow/gold
                    hoverColor = UxImVec4(0.5f, 0.5f, 0.2f, 1.0f);
                }
                else if (isVsyncOn) {
                    // Override ON
                    btnLabel = "ON";
                    bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f);        // Teal/cyan
                    hoverColor = UxImVec4(0.0f, 0.7f, 0.65f, 1.0f);
                }
                else {
                    // Override OFF
                    btnLabel = "OFF";
                    bgColor = UxImVec4(0.6f, 0.3f, 0.1f, 1.0f);         // Orange
                    hoverColor = UxImVec4(0.7f, 0.35f, 0.15f, 1.0f);
                }

                UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_Text, vsyncControlEnabled ? UxImVec4(1.0f, 1.0f, 1.0f, 1.0f) : textDisabled);
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

                UxImGui::PushID("VSyncToggle");
                if (vsyncControlEnabled && UxImGui::Button(btnLabel, UxImVec2(40.0f, 22.0f)))
                {
                    // Cycle: APP -> ON -> OFF -> APP
                    if (!isOverrideEnabled) {
                        // APP -> ON
                        ctx.reflex.isVsyncOverrideEnabled = true;
                        ctx.reflex.isVsyncEnabled = true;
                    }
                    else if (isVsyncOn) {
                        // ON -> OFF
                        ctx.reflex.isVsyncEnabled = false;
                    }
                    else {
                        // OFF -> APP
                        ctx.reflex.isVsyncOverrideEnabled = false;
                    }
                }
                else if (!vsyncControlEnabled)
                {
                    UxImGui::Button(btnLabel, UxImVec2(40.0f, 22.0f));
                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        UxImGui::Text("Controlled by Overdrive mode");
                        UxImGui::EndTooltip();
                    }
                }
                UxImGui::PopID();

                UxImGui::PopStyleVar();
                UxImGui::PopStyleColor(4);
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // FPS Limit -> ctx.reflex.isFpsLimitEnabled
            // Disabled when Overdrive is active
            {
                bool fpsLimitControlEnabled = !OverdriveController::IsActive();
                DrawToggle("FPS Limit", &ctx.reflex.isFpsLimitEnabled, fpsLimitControlEnabled);
                if (!fpsLimitControlEnabled && UxImGui::IsItemHovered())
                {
                    UxImGui::BeginTooltip();
                    UxImGui::Text("Controlled by Overdrive mode");
                    UxImGui::EndTooltip();
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Limit slider (30-220) -> ctx.reflex.desiredFpsLimit
            {
                bool limitEnabled = ctx.reflex.isFpsLimitEnabled;
                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_Text, limitEnabled ? textEnabled : textDisabled);
                UxImGui::Text("Limit");
                UxImGui::PopStyleColor();

                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, limitEnabled ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, limitEnabled ? UxImVec4(0.0f, 0.75f, 0.7f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, limitEnabled ? UxImVec4(0.0f, 0.85f, 0.8f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, 4.0f);
                UxImGui::PushItemWidth(panelWidth - 30.0f);

                if (limitEnabled)
                    UxImGui::SliderInt("##FPSLimitSlider", &ctx.reflex.desiredFpsLimit, 30, 220, "%d");
                else
                {
                    UxImGui::BeginDisabled(true);
                    UxImGui::SliderInt("##FPSLimitSlider", &ctx.reflex.desiredFpsLimit, 30, 220, "%d");
                    UxImGui::EndDisabled();
                }

                UxImGui::PopItemWidth();
                UxImGui::PopStyleVar(2);
                UxImGui::PopStyleColor(3);
            }

            UxImGui::Dummy(UxImVec2(0, 6));

            // Adaptive Frame Generation -> ctx.ngx.isDynamicFrameGenerationEnabled
            // Enables/disables frame generation based on FPS threshold
            // Only available when Frame Generation is active in the game
            // Disabled when Overdrive is active
            {
                bool fgActive = ctx.ngx.isFrameGenerationActive;
                bool afgenControlEnabled = !OverdriveController::IsActive();
                bool canToggle = fgActive && afgenControlEnabled;

                if (canToggle)
                {
                    // Normal toggle when FG is active and Overdrive is off
                    DrawToggle("Adaptive Frame Generation", &ctx.ngx.isDynamicFrameGenerationEnabled);
                }
                else
                {
                    // Show disabled toggle with tooltip
                    UxImGui::SetCursorPosX(15.0f);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::Text("Adaptive Frame Generation");
                    UxImGui::PopStyleColor();

                    UxImGui::SameLine(panelWidth - 55.0f);

                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

                    // Show effective state from Overdrive if active, otherwise user state
                    bool effectiveState = OverdriveController::IsActive()
                        ? OverdriveController::GetDynamicFrameGenerationEnabled()
                        : ctx.ngx.isDynamicFrameGenerationEnabled;
                    const char* btnLabel = effectiveState ? "ON" : "OFF";
                    UxImGui::Button(btnLabel, UxImVec2(40.0f, 22.0f));

                    // Tooltip on hover
                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        if (!fgActive)
                            UxImGui::Text("Frame Generation must be enabled in game");
                        else
                            UxImGui::Text("Controlled by Overdrive mode");
                        UxImGui::EndTooltip();
                    }

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // FPS Threshold slider (30-220) -> ctx.ngx.dynamicFrameGenerationThreshold
            {
                bool dfgEnabled = ctx.ngx.isFrameGenerationActive && ctx.ngx.isDynamicFrameGenerationEnabled;
                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_Text, dfgEnabled ? textEnabled : textDisabled);
                UxImGui::Text("FPS Threshold");
                UxImGui::PopStyleColor();

                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, dfgEnabled ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, dfgEnabled ? UxImVec4(0.0f, 0.75f, 0.7f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, dfgEnabled ? UxImVec4(0.0f, 0.85f, 0.8f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, 4.0f);
                UxImGui::PushItemWidth(panelWidth - 30.0f);

                if (dfgEnabled)
                    UxImGui::SliderInt("##FPSThresholdSlider", &ctx.ngx.dynamicFrameGenerationThreshold, 30, 220, "%d");
                else
                {
                    UxImGui::BeginDisabled(true);
                    UxImGui::SliderInt("##FPSThresholdSlider", &ctx.ngx.dynamicFrameGenerationThreshold, 30, 220, "%d");
                    UxImGui::EndDisabled();
                }

                UxImGui::PopItemWidth();
                UxImGui::PopStyleVar(2);
                UxImGui::PopStyleColor(3);
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // On the threshold generation STARTS/STOPS -> ctx.ngx.isDynamicFrameGenerationStartingOnThreshold
            {
                bool dfgEnabled = ctx.ngx.isFrameGenerationActive && ctx.ngx.isDynamicFrameGenerationEnabled;
                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_Text, dfgEnabled ? textEnabled : textDisabled);
                UxImGui::Text("Below threshold generation");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - 65.0f);

                const char* btnLabel = ctx.ngx.isDynamicFrameGenerationStartingOnThreshold ? "STARTS" : "STOPS";
                UxImVec4 bgColor;
                if (!dfgEnabled)
                    bgColor = UxImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                else if (ctx.ngx.isDynamicFrameGenerationStartingOnThreshold)
                    bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f);
                else
                    bgColor = UxImVec4(0.6f, 0.3f, 0.1f, 1.0f);  // Orange for STOPS

                UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, dfgEnabled ? (ctx.ngx.isDynamicFrameGenerationStartingOnThreshold ? UxImVec4(0.0f, 0.7f, 0.65f, 1.0f) : UxImVec4(0.7f, 0.35f, 0.15f, 1.0f)) : bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_Text, dfgEnabled ? UxImVec4(1.0f, 1.0f, 1.0f, 1.0f) : textDisabled);
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

                if (dfgEnabled && UxImGui::Button(btnLabel, UxImVec2(55.0f, 22.0f)))
                    ctx.ngx.isDynamicFrameGenerationStartingOnThreshold = !ctx.ngx.isDynamicFrameGenerationStartingOnThreshold;
                else if (!dfgEnabled)
                    UxImGui::Button(btnLabel, UxImVec2(55.0f, 22.0f));

                UxImGui::PopStyleVar();
                UxImGui::PopStyleColor(4);
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Reflex Boost - Three state toggle: APP (game controls) / ON / OFF
            // APP: isBoostOverriden == false (follow game setting)
            // ON:  isBoostOverriden == true && isBoostEnabled == true
            // OFF: isBoostOverriden == true && isBoostEnabled == false
            // Only available with real NVAPI (NVIDIA GPUs)
            // Disabled when Overdrive is active
            {
                bool boostAvailable = !ctx.nvapi.isEmbeddedNvapiUsed;
                bool boostControlEnabled = boostAvailable && !OverdriveController::IsActive();

                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_Text, boostControlEnabled ? textEnabled : textDisabled);
                UxImGui::Text("Reflex Boost");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - 55.0f);

                if (boostControlEnabled)
                {
                    // Normal toggle when available and Overdrive is off
                    bool isOverriden = ctx.reflex.isBoostOverriden;
                    bool isBoostOn = ctx.reflex.isBoostEnabled;

                    const char* btnLabel;
                    UxImVec4 bgColor;
                    UxImVec4 hoverColor;

                    if (!isOverriden) {
                        // APP mode - follows game setting
                        btnLabel = "APP";
                        bgColor = UxImVec4(0.4f, 0.4f, 0.15f, 1.0f);        // Yellow/gold
                        hoverColor = UxImVec4(0.5f, 0.5f, 0.2f, 1.0f);
                    }
                    else if (isBoostOn) {
                        // Override ON
                        btnLabel = "ON";
                        bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f);        // Teal/cyan
                        hoverColor = UxImVec4(0.0f, 0.7f, 0.65f, 1.0f);
                    }
                    else {
                        // Override OFF
                        btnLabel = "OFF";
                        bgColor = UxImVec4(0.6f, 0.3f, 0.1f, 1.0f);         // Orange
                        hoverColor = UxImVec4(0.7f, 0.35f, 0.15f, 1.0f);
                    }

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

                    UxImGui::PushID("ReflexBoostToggle");
                    if (UxImGui::Button(btnLabel, UxImVec2(40.0f, 22.0f)))
                    {
                        // Cycle: APP -> ON -> OFF -> APP
                        if (!isOverriden) {
                            // APP -> ON
                            ctx.reflex.isBoostOverriden = true;
                            ctx.reflex.isBoostEnabled = true;
                        }
                        else if (isBoostOn) {
                            // ON -> OFF
                            ctx.reflex.isBoostEnabled = false;
                        }
                        else {
                            // OFF -> APP
                            ctx.reflex.isBoostOverriden = false;
                        }
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(3);
                }
                else
                {
                    // Disabled - show effective state with tooltip
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

                    const char* btnLabel;
                    if (!boostAvailable) {
                        btnLabel = "N/A";
                    }
                    else {
                        // Overdrive active - show effective state
                        bool odBoostOverride = OverdriveController::GetBoostOverriden();
                        bool odBoost = OverdriveController::GetBoostEnabled();
                        if (!odBoostOverride) {
                            btnLabel = "APP";
                        }
                        else if (odBoost) {
                            btnLabel = "ON";
                        }
                        else {
                            btnLabel = "OFF";
                        }
                    }

                    UxImGui::Button(btnLabel, UxImVec2(40.0f, 22.0f));

                    // Tooltip on hover
                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        if (!boostAvailable)
                            UxImGui::Text("Only supported on NVIDIA GPUs");
                        else
                            UxImGui::Text("Controlled by Overdrive mode");
                        UxImGui::EndTooltip();
                    }

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Game start boost -> ctx.quickBoot
            // Enables quick boot optimization
            DrawToggle("Game start boost", &ctx.quickBoot);

            UxImGui::Dummy(UxImVec2(0, 4));

            // Overdrive mode - only show if not disabled (-1)
            // -1 = hidden, 0 = OFF, 1 = Performance, 2 = Quality, 3 = Latency
            if (ctx.overdriveMode >= 0)
            {
                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_Text, textEnabled);
                UxImGui::Text("Overdrive");
                UxImGui::PopStyleColor();

                // "ALPHA" superscript in red
                UxImGui::SameLine(0, 0);
                UxImGui::SetCursorPosY(UxImGui::GetCursorPosY() - 4.0f);  // Move up for superscript
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.9f, 0.2f, 0.2f, 1.0f));  // Red
                UxImGui::SetWindowFontScale(0.7f);  // Smaller font
                UxImGui::Text("ALPHA");
                UxImGui::SetWindowFontScale(1.0f);  // Reset font scale
                UxImGui::PopStyleColor();
                UxImGui::SetCursorPosY(UxImGui::GetCursorPosY() + 4.0f);  // Reset Y position

                UxImGui::SameLine(panelWidth - 95.0f);

                const char* overdriveItems[] = { "OFF", "PERF", "QUALITY", "LATENCY" };

                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, UxImVec4(0.1f, 0.12f, 0.15f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered, UxImVec4(0.15f, 0.18f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBgActive, UxImVec4(0.1f, 0.12f, 0.15f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.18f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.2f, 0.24f, 0.28f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Header, UxImVec4(0.0f, 0.5f, 0.45f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_HeaderHovered, UxImVec4(0.0f, 0.6f, 0.55f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);
                UxImGui::PushItemWidth(80.0f);

                UxImGui::Combo("##OverdriveCombo", &ctx.overdriveMode, overdriveItems, 4);

                UxImGui::PopItemWidth();
                UxImGui::PopStyleVar();
                UxImGui::PopStyleColor(7);
            }

            // =====================================================
            // Separator line
            // =====================================================
            UxImGui::Dummy(UxImVec2(0, 8));
            UxImGui::SetCursorPosX(15.0f);
            drawList->AddLine(
                UxImVec2(windowPos.x + 15.0f, UxImGui::GetCursorScreenPos().y),
                UxImVec2(windowPos.x + panelWidth - 15.0f, UxImGui::GetCursorScreenPos().y),
                IM_COL32(60, 70, 80, 255)
            );
            UxImGui::Dummy(UxImVec2(0, 12));

            // =====================================================
            // INTERFACE Section
            // =====================================================
            UxImGui::SetCursorPosX(15.0f);
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("INTERFACE");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 6));

            // Status Bar ON/OFF -> ctx.isMonitoringEnabled
            DrawToggle("Status Bar", &ctx.isMonitoringEnabled);

            UxImGui::Dummy(UxImVec2(0, 8));

            // Debug visualization toggles - only enabled when Frame Generation is active
            // These control bits in ctx.flags
            {
                bool fgActive = ctx.ngx.isFrameGenerationActive;

                UxImGui::SetCursorPosX(15.0f);
                UxImGui::PushStyleColor(UxImGuiCol_Text, fgActive ? textEnabled : textDisabled);
                UxImGui::Text("Debug");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - 185.0f);

                // Helper lambda for debug flag toggle buttons
                auto DrawDebugToggle = [&](const char* label, uint32_t flag, float width) {
                    bool isSet = (ctx.flags & flag) != 0;

                    UxImVec4 bgColor;
                    UxImVec4 hoverColor;
                    if (!fgActive) {
                        bgColor = UxImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                        hoverColor = bgColor;
                    }
                    else if (isSet) {
                        bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f);        // Teal/cyan - ON
                        hoverColor = UxImVec4(0.0f, 0.7f, 0.65f, 1.0f);
                    }
                    else {
                        bgColor = UxImVec4(0.25f, 0.25f, 0.28f, 1.0f);      // Dark gray - OFF
                        hoverColor = UxImVec4(0.3f, 0.3f, 0.35f, 1.0f);
                    }

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, fgActive ? UxImVec4(1.0f, 1.0f, 1.0f, 1.0f) : textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 3.0f);

                    UxImGui::PushID(label);
                    if (fgActive && UxImGui::Button(label, UxImVec2(width, 20.0f)))
                    {
                        // Toggle the flag bit
                        ctx.flags ^= flag;
                    }
                    else if (!fgActive)
                    {
                        UxImGui::Button(label, UxImVec2(width, 20.0f));
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                    };

                // Debug flag constants
                const uint32_t MFG_DEBUG_FLAG_FRAME_INDEX_LINE = 0x00010000;
                const uint32_t MFG_DEBUG_FLAG_HUD_DETECTION = 0x00020000;
                const uint32_t MFG_DEBUG_FLAG_DISOCCLUSION_TINT = 0x00040000;
                const uint32_t MFG_DEBUG_FLAG_ARTIFACTS_DETECTION = 0x00080000;

                DrawDebugToggle("FRAME", MFG_DEBUG_FLAG_FRAME_INDEX_LINE, 42.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("HUD", MFG_DEBUG_FLAG_HUD_DETECTION, 32.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("DIFF", MFG_DEBUG_FLAG_DISOCCLUSION_TINT, 34.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("GLITCH", MFG_DEBUG_FLAG_ARTIFACTS_DETECTION, 48.0f);

                // Tooltip when disabled
                if (!fgActive && UxImGui::IsItemHovered())
                {
                    UxImGui::BeginTooltip();
                    UxImGui::Text("Frame Generation must be active");
                    UxImGui::EndTooltip();
                }
            }

            // Add some padding at the bottom for scrolling
            UxImGui::Dummy(UxImVec2(0, 15));
        }
        UxImGui::End();

        UxImGui::PopStyleColor(5);  // WindowBg + 4 scrollbar colors
        UxImGui::PopStyleVar(6);    // 4 original + 2 scrollbar

        // =====================================================
        // Vertical Slider Popup (rendered outside main panel)
        // Only one slider can be visible at a time
        // Slider touches the sidebar with yellow accent line connecting them
        // =====================================================
        bool showAnySlider = SidePanel::g_ShowRayTracingRangeSlider ||
            SidePanel::g_ShowIlluminationSlider ||
            SidePanel::g_ShowOcclusionSlider;
        if (showAnySlider)
        {
            const float sliderWidth = 50.0f;
            const float sliderHeight = 150.0f;

            // Determine which slider to show and get its Y position
            float sliderY;
            int* valuePtr;
            const char* sliderId;
            const char* windowId;

            if (SidePanel::g_ShowRayTracingRangeSlider) {
                sliderY = SidePanel::g_RayTracingRangeSliderY;
                valuePtr = &ctx.ngx.rayTracingRange;
                sliderId = "##RayTracingRangeVSlider";
                windowId = "##RayTracingRangeSliderPopup";
            }
            else if (SidePanel::g_ShowIlluminationSlider) {
                sliderY = SidePanel::g_IlluminationSliderY;
                valuePtr = &ctx.ngx.illuminationStrength;
                sliderId = "##IlluminationVSlider";
                windowId = "##IlluminationSliderPopup";
            }
            else {
                sliderY = SidePanel::g_OcclusionSliderY;
                valuePtr = &ctx.ngx.occlusionStrength;
                sliderId = "##OcclusionVSlider";
                windowId = "##OcclusionSliderPopup";
            }

            // Position: touching the sidebar (no margin), aligned with button Y
            UxImVec2 sliderPos(panelMargin + panelWidth, sliderY - sliderHeight / 2 + 11);

            // Clamp to screen bounds
            if (sliderPos.y < panelMargin)
                sliderPos.y = panelMargin;
            if (sliderPos.y + sliderHeight > io.DisplaySize.y - panelMargin)
                sliderPos.y = io.DisplaySize.y - panelMargin - sliderHeight;

            UxImGui::SetNextWindowPos(sliderPos);
            UxImGui::SetNextWindowSize(UxImVec2(sliderWidth, sliderHeight));

            // Transparent window - we'll draw custom background
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, 0.0f);
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(8, 12));
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, 0.0f);
            UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0.0f, 0.0f, 0.0f, 0.0f));  // Transparent

            UxImGuiWindowFlags sliderFlags = UxImGuiWindowFlags_NoTitleBar |
                UxImGuiWindowFlags_NoResize |
                UxImGuiWindowFlags_NoMove |
                UxImGuiWindowFlags_NoScrollbar |
                UxImGuiWindowFlags_NoCollapse;

            if (UxImGui::Begin(windowId, nullptr, sliderFlags))
            {
                UxImDrawList* sliderDrawList = UxImGui::GetWindowDrawList();
                UxImVec2 sliderWinPos = UxImGui::GetWindowPos();
                UxImVec2 sliderWinSize = UxImGui::GetWindowSize();

                // Draw custom background with rounding only on right side (matching sidebar's 8.0f)
                sliderDrawList->AddRectFilled(
                    sliderWinPos,
                    UxImVec2(sliderWinPos.x + sliderWinSize.x, sliderWinPos.y + sliderWinSize.y),
                    IM_COL32(5, 13, 20, 242),  // Same as sidebar bg (0.02, 0.05, 0.08, 0.95)
                    8.0f,
                    UxImDrawFlags_RoundCornersRight
                );

                // Draw dark yellow accent line on left (connecting to sidebar background)
                sliderDrawList->AddRectFilled(
                    sliderWinPos,
                    UxImVec2(sliderWinPos.x + 3.0f, sliderWinPos.y + sliderWinSize.y),
                    IM_COL32(100, 90, 20, 255)  // Same dark yellow as background strip
                );

                // Vertical slider
                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, UxImVec4(0.1f, 0.12f, 0.15f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, UxImVec4(0.6f, 0.55f, 0.15f, 1.0f));  // Dark yellow grab
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, UxImVec4(0.7f, 0.65f, 0.2f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, 4.0f);
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabMinSize, 20.0f);

                // Center the slider
                float availHeight = UxImGui::GetContentRegionAvail().y;
                UxImGui::VSliderInt(sliderId, UxImVec2(30.0f, availHeight), valuePtr, 1, 100, "%d");

                UxImGui::PopStyleVar(3);
                UxImGui::PopStyleColor(3);
            }
            UxImGui::End();

            UxImGui::PopStyleColor();
            UxImGui::PopStyleVar(3);
        }
    }

    // Public function to toggle side panel
    void SetSidePanelEnabled(bool enabled)
    {
        SidePanel::g_Enabled = enabled;
        ctx.isSideBarEnabled = enabled;
    }

    bool IsSidePanelEnabled()
    {
        return ctx.isSideBarEnabled;
    }

    // =============================================================================
    // Public API
    // =============================================================================

    void Init()
    {
        g_Initialized = true;
        g_CurrentCategory = Category::General;
    }

    void SetTargetWindow(HWND hwnd)
    {
        g_TargetWindow = hwnd;
    }

    void OnMenuClosed()
    {
        if (g_MenuWasOpen)
        {
            // Menu just closed - let game regain control
            // Game will hide cursor through its normal code path
            // (our hooks are no longer blocking it)
            g_MenuWasOpen = false;
        }
    }

    bool IsMenuOpen()
    {
        return g_MenuWasOpen;
    }

    Category GetCurrentCategory()
    {
        return g_CurrentCategory;
    }

    void SetCurrentCategory(Category cat)
    {
        g_CurrentCategory = cat;
    }

    void Render(bool* p_open)
    {
        if (!g_Initialized)
            Init();

        // =====================================================================
        // CURSOR CONTROL - Force cursor visible and unclipped each frame
        // =====================================================================
        UxImGuiIO& io = UxImGui::GetIO();

        if (!g_MenuWasOpen)
        {
            // First frame menu is open - force cursor visible
            while (ShowCursor(TRUE) < 0) {}
            g_MenuWasOpen = true;
        }

        // Continuously unclip cursor each frame (game may try to re-clip)
        // Note: We need to bypass our own hook here, so we call the function directly
        // The hook blocks game calls, but we need this call to work
        // Actually - since we ARE the ones calling it, and our hook returns TRUE when
        // menu is open, this does nothing. We need to restructure.
        // For now, rely on OnMenuOpen having done ClipCursor(nullptr) via original

        // =====================================================================
        // Render Side Panel (always visible when menu open)
        // =====================================================================
        RenderSidePanel();

        // =====================================================================
        // Render Settings Persistence Prompt (if settings changed)
        // =====================================================================
        SettingsPersistence::RenderPersistPrompt();

#if ENABLE_MAIN_SETTINGS_WINDOW
        PushStyleColors();
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, 8.0f);
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(0, 0));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, 0.0f);

        // Window size and position
        UxImVec2 window_size(750, 450);
        UxImVec2 display_size = UxImGui::GetIO().DisplaySize;
        UxImVec2 window_pos((display_size.x - window_size.x) * 0.5f, (display_size.y - window_size.y) * 0.5f);

        UxImGui::SetNextWindowSize(window_size, UxImGuiCond_Always);
        UxImGui::SetNextWindowPos(window_pos, UxImGuiCond_Always);

        UxImGuiWindowFlags window_flags =
            UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoScrollbar |
            UxImGuiWindowFlags_NoCollapse;

        if (UxImGui::Begin("##SettingsMenu", p_open, window_flags))
        {
            UxImVec2 contentRegion = UxImGui::GetContentRegionAvail();
            float sidebarWidth = 180.0f;

            // =====================================================================
            // Sidebar
            // =====================================================================
            UxImGui::PushStyleColor(UxImGuiCol_ChildBg, Colors::SidebarBg);
            UxImGui::BeginChild("##Sidebar", UxImVec2(sidebarWidth, 0), false);
            {
                // Title
                UxImGui::Dummy(UxImVec2(0, 15));
                UxImGui::SetCursorPosX(20);
                UxImGui::TextColored(Colors::AccentGreen, "SETTINGS");
                UxImGui::Dummy(UxImVec2(0, 20));

                // Menu items
                for (int i = 0; i < (int)Category::COUNT; i++)
                {
                    bool isSelected = ((int)g_CurrentCategory == i);

                    UxImGui::SetCursorPosX(10);

                    if (MenuItemButton(g_Categories[i].name, g_Categories[i].icon, isSelected))
                    {
                        g_CurrentCategory = (Category)i;
                    }

                    UxImGui::Dummy(UxImVec2(0, 2));
                }
            }
            UxImGui::EndChild();
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            // =====================================================================
            // Content area
            // =====================================================================
            UxImGui::PushStyleColor(UxImGuiCol_ChildBg, Colors::ContentBg);
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(20, 20));
            UxImGui::BeginChild("##Content", UxImVec2(0, 0), false);
            {
                switch (g_CurrentCategory)
                {
                case Category::Status:      RenderStatusContent(); break;
                case Category::InfoBlock:   RenderInfoBlockContent(); break;
                case Category::Speedometer: RenderSpeedometerContent(); break;
                case Category::Compass:     RenderCompassContent(); break;
                case Category::General:     RenderGeneralContent(); break;
                default: break;
                }
            }
            UxImGui::EndChild();
            UxImGui::PopStyleVar();
            UxImGui::PopStyleColor();
        }
        UxImGui::End();

        UxImGui::PopStyleVar(3);
        PopStyleColors();
#endif // ENABLE_MAIN_SETTINGS_WINDOW

        // Update LMB state for next frame
        g_WasLmbDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    }

    // =============================================================================
    // Public Monitoring Overlay - Call every frame, independent of menu state
    // =============================================================================
    void RenderMonitoringOverlay()
    {
        // This renders the monitoring bar based solely on ctx.isMonitoringEnabled
        // It's separate from Render() which requires F1 to be pressed
        RenderMonitoringBar();
    }
}