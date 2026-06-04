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
#include "ActionIntensity.h"
#include "Common.h"
#include <cmath>

// External: force Streamline to apply current mfgEnforcedMode (defined in StreamlineProxy.cpp)
extern void StreamlineProxy_ForceApplyMfgMode();
// External: restore game's original DLSSG options (defined in StreamlineProxy.cpp)
extern void StreamlineProxy_RestoreGameDLSSGOptions();

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
    static int  g_MenuToggleKey = VK_OEM_3; // Menu toggle key (default: tilde/backtick)

    // =============================================================================
    // UI Scaling - proportional to 1080p reference, never smaller
    // =============================================================================

    static float g_UiScale = 1.0f;       // Cached scale factor
    static float g_UiScaleOverride = 0.0f; // Override: 0 = auto, >0 = forced scale

    // Compute UI scale based on display size
    // Reference: 1920x1080 = scale 1.0
    // Uses height for landscape, width for portrait, never below 1.0
    static float ComputeUiScale(const UxImVec2& displaySize)
    {
        // If override is set, use it directly
        if (g_UiScaleOverride > 0.0f)
            return g_UiScaleOverride;

        if (displaySize.x <= 0 || displaySize.y <= 0)
            return 1.0f;

        float scale;
        if (displaySize.x >= displaySize.y)
        {
            // Landscape or square - scale by height (reference: 1080)
            scale = displaySize.y / 1080.0f;
        }
        else
        {
            // Portrait - scale by width (reference: 1920)
            scale = displaySize.x / 1920.0f;
        }

        // Never smaller than 1.0 (FullHD is the minimum)
        if (scale < 1.0f)
            scale = 1.0f;

        return scale;
    }

    // Scale a pixel value from 1080p reference
    static inline float S(float px) { return px * g_UiScale; }

    // RAII helper: make Combo popup (and any nested popup) inherit g_UiScale font size.
    // ImGui's SetWindowFontScale is per-window and popups are separate windows, so they
    // default to FontGlobalScale=1.0 and show at base font size - unreadable in 4K.
    // Usage: wrap Combo calls in a block with this guard on top of the stack.
    struct ComboFontScaleGuard
    {
        float savedGlobalScale;
        ComboFontScaleGuard()
        {
            UxImGuiIO& io = UxImGui::GetIO();
            savedGlobalScale = io.FontGlobalScale;
            // Push scale into FontGlobalScale so the popup window inherits it,
            // and reset current window to 1.0 so the effective scale in the parent
            // (FontGlobalScale * WindowFontScale) stays equal to g_UiScale.
            io.FontGlobalScale = g_UiScale;
            UxImGui::SetWindowFontScale(1.0f);
        }
        ~ComboFontScaleGuard()
        {
            UxImGuiIO& io = UxImGui::GetIO();
            io.FontGlobalScale = savedGlobalScale;
            UxImGui::SetWindowFontScale(g_UiScale);
        }
    };

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
        const float height = S(40.0f);

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
            UxImVec2 bar_max(pos.x + S(3.0f), pos.y + height);
            draw_list->AddRectFilled(bar_min, bar_max,
                UxImGui::ColorConvertFloat4ToU32(Colors::AccentGreen), 2.0f);
        }

        // Icon (simple circle with letter)
        float icon_size = S(24.0f);
        UxImVec2 icon_pos(pos.x + S(12.0f), pos.y + (height - icon_size) * 0.5f);

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
        UxImVec2 label_pos(pos.x + S(48.0f), pos.y + (height - UxImGui::GetTextLineHeight()) * 0.5f);
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
        float height = S(60.0f);

        UxImU32 bg_col = UxImGui::ColorConvertFloat4ToU32(UxImVec4(0.08f, 0.11f, 0.14f, 1.0f));
        draw_list->AddRectFilled(pos, UxImVec2(pos.x + width, pos.y + height), bg_col, 6.0f);

        // Icon
        float icon_size = S(32.0f);
        UxImVec2 icon_pos(pos.x + S(15.0f), pos.y + (height - icon_size) * 0.5f);
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
        UxImVec2 label_pos(pos.x + S(60.0f), pos.y + S(12.0f));
        draw_list->AddText(label_pos,
            UxImGui::ColorConvertFloat4ToU32(Colors::TextPrimary), label);

        // Description
        if (description && description[0])
        {
            UxImVec2 desc_pos(pos.x + S(60.0f), pos.y + S(32.0f));
            draw_list->AddText(desc_pos,
                UxImGui::ColorConvertFloat4ToU32(Colors::TextSecondary), description);
        }

        // Set cursor for control on the right
        UxImGui::SetCursorScreenPos(UxImVec2(pos.x + width - 120.0f, pos.y + (height - 28.0f) * 0.5f));
    }

    static void EndOptionRow()
    {
        UxImGui::PopID();
        UxImGui::Dummy(UxImVec2(0, S(70.0f)));  // Spacing for next row
    }

    // Two-option toggle (like Off/On or MPH/KM/H)
    static int OptionToggle(const char* option1, const char* option2, int current)
    {
        int result = current;

        UxImGui::PushStyleVar(UxImGuiStyleVar_ItemSpacing, UxImVec2(S(0), S(0)));
        UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

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

        if (UxImGui::Button(option1, UxImVec2(S(50.0f), S(28.0f))))
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

        if (UxImGui::Button(option2, UxImVec2(S(50.0f), S(28.0f))))
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
        float optionHeight = S(65.0f);

        // Helper lambda for drawing option card with MANUAL hit testing
        auto DrawOptionCard = [&](const char* label, const char* description, const char* iconLetter,
            const char* opt1, const char* opt2, int* value, UxImVec4 iconColor)
            {
                UxImVec2 pos = UxImGui::GetCursorScreenPos();

                // Card background
                UxImU32 cardBg = UxImGui::ColorConvertFloat4ToU32(UxImVec4(0.08f, 0.11f, 0.14f, 1.0f));
                draw_list->AddRectFilled(pos, UxImVec2(pos.x + optionWidth, pos.y + optionHeight), cardBg, 6.0f);

                // Icon circle
                float iconSize = S(36.0f);
                UxImVec2 iconCenter(pos.x + S(25.0f), pos.y + optionHeight / 2.0f);
                draw_list->AddCircleFilled(iconCenter, iconSize / 2.0f, UxImGui::ColorConvertFloat4ToU32(iconColor));

                // Icon letter
                UxImVec2 letterSize = UxImGui::CalcTextSize(iconLetter);
                draw_list->AddText(UxImVec2(iconCenter.x - letterSize.x / 2, iconCenter.y - letterSize.y / 2),
                    IM_COL32(255, 255, 255, 255), iconLetter);

                // Label
                draw_list->AddText(UxImVec2(pos.x + S(55.0f), pos.y + S(15.0f)),
                    UxImGui::ColorConvertFloat4ToU32(Colors::TextPrimary), label);

                // Description  
                draw_list->AddText(UxImVec2(pos.x + S(55.0f), pos.y + S(35.0f)),
                    UxImGui::ColorConvertFloat4ToU32(Colors::TextSecondary), description);

                // Toggle buttons - MANUAL HIT TESTING
                float btnWidth = S(45.0f);
                float btnHeight = S(24.0f);
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
        static int g_CachedBaseFps = 0;              // Base FPS (without FG multiplier)
        static int g_CachedDynamicMfgTarget = 0;     // Dynamic MFG current enforced mode (0=off, 1=2X, 2=3X, 3=4X, 4=5X, 5=6X)
        static int g_CachedDynamicMfgSuggested = 0;  // Dynamic MFG suggested mode (what logic wants)
        static int g_CachedFramesGen = 0;             // ctx.ngx.framesGenerated (0=noFG, 1=2X, 2=3X, 3=4X, 4=5X, 5=6X)

        // Cached latency value (updated every 1 second)
        static double g_CachedLatencyMs = 0.0;
        static bool g_CachedHasLatencyData = false;

        // Cached status values (updated every 1 second)
        static int g_CachedFgMultiplier = 0;        // 0=OFF, 1-6 = multiplier
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
        // Get animated height and apply UI scale
        const float barHeight = MenuAnimations::GetMonitoringBarHeight() * g_UiScale;

        // Don't render if fully closed or too small
        if (barHeight < 1.0f && MenuAnimations::IsMonitoringBarFullyClosed())
            return;

        UxImGuiIO& io = UxImGui::GetIO();

        ULONGLONG currentTime = GetTickCount64();

        // MFG mode status: update every frame for instant response to mode changes
        {
            MonitoringBar::g_CachedDynamicMfgTarget = ctx.streamline.mfgEnforcedMode;
            MonitoringBar::g_CachedDynamicMfgSuggested = FpsMonitor::GetDynamicMfgSuggestedMode();
            MonitoringBar::g_CachedFramesGen = ctx.ngx.framesGenerated;
            MonitoringBar::g_CachedFramesGenerated = ctx.ngx.framesGenerated;

            // Calculate FG status: OFF, 1X, 2X, 3X, 4X (with optional "(AUTO)")
            // Dynamic MFG operates at Streamline level, independent of NGX isFrameGenerationActive
            if (ctx.streamline.isDynamicMfgEnabled)
            {
                // Dynamic MFG mode - always AUTO, works independently of NGX FG state
                MonitoringBar::g_CachedFgIsAuto = true;
                MonitoringBar::g_CachedFgMultiplier = (MonitoringBar::g_CachedFramesGenerated > 0)
                    ? MonitoringBar::g_CachedFramesGenerated + 1 : 1;
            }
            else if (!ctx.ngx.isFrameGenerationActive)
            {
                // FG completely off
                MonitoringBar::g_CachedFgMultiplier = 0;
                MonitoringBar::g_CachedFgIsAuto = false;
            }
            else if (ctx.ngx.isDynamicFrameGenerationEnabled)
            {
                // Dynamic FG mode (Adaptive FG or Dynamic MFG)
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
        }

        // Update remaining cached values every 1000ms (FPS, latency, upscaling status)
        if (currentTime - MonitoringBar::g_LastUpdateTime >= 1000)
        {
            MonitoringBar::g_CachedCurrentFps = FpsMonitor::GetCurrentFps();
            MonitoringBar::g_CachedPotentialFps = FpsMonitor::GetPotentialFps();
            MonitoringBar::g_CachedBaseFps = FpsMonitor::GetBaseFps();

            // Cache latency data
            MonitoringBar::g_CachedHasLatencyData = FpsMonitor::HasLatencyData();
            if (MonitoringBar::g_CachedHasLatencyData)
            {
                MonitoringBar::g_CachedLatencyMs = FpsMonitor::GetAverageLatencyMs();
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
        const float barMargin = S(10.0f);
        const bool showDmfg = ctx.streamline.isDynamicMfgEnabled;
        float barWidth = showDmfg ? S(610.0f) : S(510.0f);  // +20px for base FPS display

        // Extend bar width by 20px when latency is actually displayed
        const bool barShowsLatency = MonitoringBar::g_CachedHasLatencyData &&
            MonitoringBar::g_CachedLatencyMs > 0.0 &&
            !ctx.nvapi.isEmbeddedNvapiUsed;
        if (barShowsLatency)
            barWidth += S(20.0f);

        // Position: top center with 10px margin from top
        UxImVec2 barPos((io.DisplaySize.x - barWidth) * 0.5f, barMargin);

        UxImGui::SetNextWindowPos(barPos);
        UxImGui::SetNextWindowSize(UxImVec2(barWidth, barHeight));

        // Styling
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(6.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(12), S(6)));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, S(0.0f));

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
            // Scale font for monitoring bar
            UxImGui::SetWindowFontScale(g_UiScale);
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // Draw accent line on bottom
            UxImU32 accentColor = IM_COL32(0, 200, 180, 255);
            drawList->AddRectFilled(
                UxImVec2(windowPos.x, windowPos.y + windowSize.y - S(2.0f)),
                UxImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                accentColor
            );

            // FPS display with optional latency (fixed width section)
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("FPS:");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            // Display current FPS with base FPS in parentheses when FG multiplier >= 2
            if (MonitoringBar::g_CachedFgMultiplier >= 2)
            {
                int baseFps = MonitoringBar::g_CachedCurrentFps / MonitoringBar::g_CachedFgMultiplier;
                UxImGui::Text("%3d (%d)", MonitoringBar::g_CachedCurrentFps, baseFps);
            }
            else
            {
                UxImGui::Text("%3d", MonitoringBar::g_CachedCurrentFps);
            }
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

            // 5X
            UxImGui::PushStyleColor(UxImGuiCol_Text, (MonitoringBar::g_CachedFgMultiplier == 5) ? activeColor : dimColor);
            UxImGui::Text("5X");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            // 6X
            UxImGui::PushStyleColor(UxImGuiCol_Text, (MonitoringBar::g_CachedFgMultiplier == 6) ? activeColor : dimColor);
            UxImGui::Text("6X");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            // AUTO - color logic:
            // Gray: no dynamic mode enabled
            // Orange: Adaptive FG enabled + FG active but duplicating frames
            // Green: any dynamic mode active and generating normally
            UxImVec4 autoColor = dimColor;
            if (ctx.streamline.isDynamicMfgEnabled)
            {
                // Dynamic MFG is active - always show as active (green)
                // isDuplicatingFrames only applies to Adaptive FG, not Dynamic MFG
                autoColor = activeColor;
            }
            else if (ctx.ngx.isDynamicFrameGenerationEnabled && ctx.ngx.isFrameGenerationActive)
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

            // Dynamic MFG info (only show when isDynamicMfgEnabled)
            if (ctx.streamline.isDynamicMfgEnabled)
            {
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

                // Compact: "B:45 S:x3"
                auto modeStr = [](int m) -> const char* {
                switch (m) { case 1: return "2X"; case 2: return "3X"; case 3: return "4X"; case 4: return "5X"; case 5: return "6X"; default: return "--"; }
                    };
                auto modeCol = [](int m) -> UxImVec4 {
                    switch (m) {
                    case 1: return UxImVec4(0.2f, 0.6f, 1.0f, 1.0f);   // Blue - 2X
                    case 2: return UxImVec4(0.0f, 0.8f, 0.6f, 1.0f);   // Teal - 3X
                    case 3: return UxImVec4(0.5f, 0.3f, 0.7f, 1.0f);   // Purple - 4X
                    case 4: return UxImVec4(0.9f, 0.5f, 0.2f, 1.0f);   // Orange - 5X
                    case 5: return UxImVec4(0.9f, 0.2f, 0.3f, 1.0f);   // Red - 6X
                    default: return UxImVec4(0.6f, 0.3f, 0.3f, 1.0f);
                    }
                    };

                int sug = MonitoringBar::g_CachedDynamicMfgSuggested;
                UxImVec4 dim(0.5f, 0.55f, 0.6f, 1.0f);

                UxImGui::PushStyleColor(UxImGuiCol_Text, dim);
                UxImGui::Text("B:");
                UxImGui::PopStyleColor();
                UxImGui::SameLine(0, S(3));
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                UxImGui::Text("%d", MonitoringBar::g_CachedBaseFps);
                UxImGui::PopStyleColor();

                UxImGui::SameLine(0, S(4));
                UxImGui::PushStyleColor(UxImGuiCol_Text, dim);
                UxImGui::Text("S:");
                UxImGui::PopStyleColor();
                UxImGui::SameLine(0, 0);
                UxImGui::PushStyleColor(UxImGuiCol_Text, modeCol(sug));
                UxImGui::Text("%s", modeStr(sug));
                UxImGui::PopStyleColor();
            }
        }
        UxImGui::End();

        UxImGui::PopStyleColor();  // WindowBg
        UxImGui::PopStyleVar(3);
    }

    // =============================================================================
    // Scene Intensity Bar - Thin bar below Monitoring Bar
    // Shows ActionIntensity 0..1 as a colored fill (blue->red gradient)
    // =============================================================================

    namespace IntensityBar
    {
        static DWORD  g_LastUpdateTime = 0;
        static float  g_CachedIntensity = 0.f;
        static bool   g_CachedAvailable = false;
    }

    static void RenderSceneIntensityBar()
    {
        // Only render when monitoring bar is visible and DFG Instinct is active
        if (!ctx.isMonitoringEnabled || !ctx.streamline.isMinDynamicFpsActive)
            return;

        // Don't render if monitoring bar animation hasn't opened yet
        const float monitoringBarHeight = MenuAnimations::GetMonitoringBarHeight() * g_UiScale;
        if (monitoringBarHeight < 1.0f)
            return;

        // Throttle update to ~10ms
        DWORD currentTime = GetTickCount();
        if (currentTime - IntensityBar::g_LastUpdateTime >= 10)
        {
            ActionIntensity::Snapshot snap = ActionIntensity::GetSnapshot();
            IntensityBar::g_CachedAvailable = snap.isAvailable;
            IntensityBar::g_CachedIntensity = snap.intensity;
            IntensityBar::g_LastUpdateTime = currentTime;
        }

        // Dimensions
        UxImGuiIO& io = UxImGui::GetIO();
        const float barHeight = S(4.0f);
        const float barWidth = io.DisplaySize.x * 0.25f;
        const float monitoringBarMargin = S(10.0f);
        const float gap = S(4.0f);

        // Position: centered horizontally, just below monitoring bar
        float posX = (io.DisplaySize.x - barWidth) * 0.5f;
        float posY = monitoringBarMargin + monitoringBarHeight + gap;

        UxImGui::SetNextWindowPos(UxImVec2(posX, posY));
        UxImGui::SetNextWindowSize(UxImVec2(barWidth, barHeight));

        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(2.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(0, 0));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, 0.0f);

        // Transparent window - we draw everything via DrawList
        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0, 0, 0, 0));

        UxImGuiWindowFlags flags = UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoScrollbar |
            UxImGuiWindowFlags_NoCollapse |
            UxImGuiWindowFlags_NoInputs |
            UxImGuiWindowFlags_NoSavedSettings;

        if (UxImGui::Begin("##SceneIntensityBar", nullptr, flags))
        {
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 wPos = UxImGui::GetWindowPos();

            // Background track (dark gray) - always visible
            drawList->AddRectFilled(
                wPos,
                UxImVec2(wPos.x + barWidth, wPos.y + barHeight),
                IM_COL32(40, 42, 48, 180),
                S(2.0f));

            // Fill bar - only when intensity data is available
            float t = IntensityBar::g_CachedAvailable ? IntensityBar::g_CachedIntensity : 0.f;
            if (t > 0.001f)
            {
                float fillWidth = barWidth * t;

                // Color gradient: light blue (t=0) -> light red (t=1)
                //   t=0: (100, 160, 220)  soft blue
                //   t=1: (220,  80,  80)  soft red
                uint8_t r = static_cast<uint8_t>(100.f + 120.f * t);
                uint8_t g = static_cast<uint8_t>(160.f - 80.f * t);
                uint8_t b = static_cast<uint8_t>(220.f - 140.f * t);

                // Centered horizontally within the track
                float fillX = wPos.x + (barWidth - fillWidth) * 0.5f;
                drawList->AddRectFilled(
                    UxImVec2(fillX, wPos.y),
                    UxImVec2(fillX + fillWidth, wPos.y + barHeight),
                    IM_COL32(r, g, b, 220),
                    S(2.0f));
            }
        }
        UxImGui::End();

        UxImGui::PopStyleColor();
        UxImGui::PopStyleVar(3);
    }
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

        // MFG Mode Override restart warning state
        static bool g_MfgRestartWarningDismissed = false;
        static int g_MfgModeAtLoad = 0;      // Value at process start - never changes
        static int g_MfgModeLastSaved = 0;    // Value last persisted to disk - updates on Save

        // Frame Generation backend (FSR3/DLSSG) restart warning state
        static bool g_FgBackendRestartWarningDismissed = false;
        static bool g_DlssgDisabledAtLoad = true;       // Value at process start - never changes
        static bool g_DlssgDisabledLastSaved = true;    // Value last persisted to disk - updates on Save

        // Hybrid MFG (force) restart warning state
        static bool g_HybridMfgRestartWarningDismissed = false;
        static bool g_HybridMfgForcedAtLoad = false;      // Value at process start - never changes
        static bool g_HybridMfgForcedLastSaved = false;   // Value last persisted to disk - updates on Save
    }

    static void RenderSidePanel()
    {
        // Sync with context
        SidePanel::g_Enabled = ctx.isSideBarEnabled;

        if (!SidePanel::g_Enabled)
            return;

        // Detect when user clicks Persist � update g_MfgModeLastSaved
        {
            static bool hadUnsavedLastFrame = false;
            bool hasUnsaved = SettingsPersistence::HasUnsavedChanges();
            if (hadUnsavedLastFrame && !hasUnsaved)
            {
                SidePanel::g_MfgModeLastSaved = ctx.nvapi.mfgEnforcedMode;
                SidePanel::g_DlssgDisabledLastSaved = ctx.ngx.isDlssgDisabled;
                SidePanel::g_HybridMfgForcedLastSaved = ctx.ngx.isHybridMfgForced;
            }
            hadUnsavedLastFrame = hasUnsaved;
        }

        // Get animated width
        // Get animated width and apply UI scale (MenuAnimations targets 280px at 1080p)
        const float panelWidth = MenuAnimations::GetSidePanelWidth() * g_UiScale;
        if (panelWidth < 1.0f)
            return;  // Don't render if too small

        UxImGuiIO& io = UxImGui::GetIO();

        // Panel dimensions - dynamic height based on screen resolution
        const float panelMargin = S(10.0f);
        const float persistPromptHeight = S(85.0f);  // Height reserved for "Settings Changed" prompt
        const float maxPanelHeight = io.DisplaySize.y - panelMargin * 2 - persistPromptHeight;
        float desiredPanelHeight = S(750.0f);  // Desired height for content

        // Taller sidebar for high resolution (render height >= 1070)
        if (io.DisplaySize.y >= 1070.0f)
            desiredPanelHeight += S(200.0f);

        const float panelHeight = (desiredPanelHeight < maxPanelHeight) ? desiredPanelHeight : maxPanelHeight;

        // Position: top-left with 10px margin
        UxImVec2 panelPos(panelMargin, panelMargin);

        UxImGui::SetNextWindowPos(panelPos);
        UxImGui::SetNextWindowSize(UxImVec2(panelWidth, panelHeight));

        // Styling
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(8.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(0), S(0)));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, S(0.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_ItemSpacing, UxImVec2(S(8), S(4)));

        // Scrollbar styling
        UxImGui::PushStyleVar(UxImGuiStyleVar_ScrollbarSize, S(8.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_ScrollbarRounding, S(4.0f));
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
            // Scale font for entire side panel
            UxImGui::SetWindowFontScale(g_UiScale);
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
                UxImVec2(windowPos.x + S(3.0f), windowPos.y + windowSize.y),
                accentColor
            );

            // Subtle glow effect
            for (int i = 0; i < 3; i++)
            {
                float alpha = 0.15f - (i * 0.05f);
                drawList->AddRectFilled(
                    UxImVec2(windowPos.x + S(3.0f) + i * S(2.0f), windowPos.y),
                    UxImVec2(windowPos.x + S(5.0f) + i * S(2.0f), windowPos.y + windowSize.y),
                    IM_COL32(0, 200, 180, (int)(alpha * 255))
                );
            }

            // Content padding
            UxImGui::SetCursorPos(UxImVec2(S(15.0f), S(12.0f)));

            // Colors for enabled/disabled states
            UxImVec4 textEnabled = UxImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            UxImVec4 textDisabled = UxImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            UxImVec4 sliderBgDisabled = UxImVec4(0.08f, 0.08f, 0.1f, 1.0f);

            // Helper lambda for toggle button
            auto DrawToggle = [&](const char* label, bool* value, bool enabled = true) -> bool
                {
                    bool changed = false;
                    UxImGui::SetCursorPosX(S(15.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, enabled ? textEnabled : textDisabled);
                    UxImGui::Text("%s", label);
                    UxImGui::PopStyleColor();

                    UxImGui::SameLine(panelWidth - S(55.0f));

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
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID(label);
                    if (enabled && UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f))))
                    {
                        *value = !*value;
                        changed = true;
                    }
                    else if (!enabled)
                    {
                        UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f)));
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                    return changed;
                };

            // Helper lambda for combo box
            auto DrawCombo = [&](const char* label, int* value, const char* items[], int itemCount, bool enabled = true)
                {
                    UxImGui::SetCursorPosX(S(15.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, enabled ? textEnabled : textDisabled);
                    UxImGui::Text("%s", label);
                    UxImGui::PopStyleColor();

                    UxImGui::SameLine(panelWidth - S(90.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBg, enabled ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered, enabled ? UxImVec4(0.15f, 0.18f, 0.22f, 1.0f) : sliderBgDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                    UxImGui::PushItemWidth(S(75.0f));

                    UxImGui::PushID(label);
                    {
                        ComboFontScaleGuard fontGuard; // Ensure popup inherits UI scale
                        if (enabled)
                            UxImGui::Combo("##combo", value, items, itemCount);
                        else
                        {
                            UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                            UxImGui::Combo("##combo", value, items, itemCount);
                            UxImGui::PopStyleColor();
                        }
                    }
                    UxImGui::PopID();

                    UxImGui::PopItemWidth();
                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(2);
                };

            // Helper lambda for slider
            auto DrawSlider = [&](const char* label, int* value, bool enabled = true)
                {
                    UxImGui::SetCursorPosX(S(15.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, enabled ? textEnabled : textDisabled);
                    UxImGui::Text("%s", label);
                    UxImGui::PopStyleColor();

                    UxImGui::SetCursorPosX(S(15.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBg, enabled ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                    UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, enabled ? UxImVec4(0.0f, 0.75f, 0.7f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, enabled ? UxImVec4(0.0f, 0.85f, 0.8f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, S(4.0f));
                    UxImGui::PushItemWidth(panelWidth - S(30.0f));

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
                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                UxImGui::Text("Screen Space Ray Tracing");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(55.0f));

                UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                UxImGui::PushID("SSRTDisabled");
                UxImGui::Button("N/A", UxImVec2(S(40.0f), S(22.0f)));

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
                UxImGui::PopID();

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

            UxImGui::Dummy(UxImVec2(0, 6));

            // GhostBuster - Anti-ghosting for Frame Generation
            // Only available when Frame Generation is active
            {
                bool fgActive = ctx.ngx.isFrameGenerationActive;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, fgActive ? textEnabled : textDisabled);
                UxImGui::Text("GhostBuster");
                UxImGui::PopStyleColor();

                // "WIP" superscript in yellow
                UxImGui::SameLine(0, 0);
                UxImGui::SetCursorPosY(UxImGui::GetCursorPosY() - 4.0f);  // Move up for superscript
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.9f, 0.8f, 0.2f, 1.0f));  // Yellow
                UxImGui::SetWindowFontScale(0.7f * g_UiScale);  // Smaller font
                UxImGui::Text("WIP");
                UxImGui::SetWindowFontScale(g_UiScale);  // Reset font scale
                UxImGui::PopStyleColor();
                UxImGui::SetCursorPosY(UxImGui::GetCursorPosY() + 4.0f);  // Reset Y position

                UxImGui::SameLine(panelWidth - S(55.0f));

                if (fgActive)
                {
                    // Normal toggle when FG is active
                    const char* btnLabel = ctx.ngx.isGhostBustingEnabled ? "ON" : "OFF";
                    UxImVec4 bgColor = ctx.ngx.isGhostBustingEnabled
                        ? UxImVec4(0.0f, 0.6f, 0.55f, 1.0f)   // Teal/cyan - ON
                        : UxImVec4(0.2f, 0.22f, 0.25f, 1.0f); // Dark gray - OFF
                    UxImVec4 hoverColor = ctx.ngx.isGhostBustingEnabled
                        ? UxImVec4(0.0f, 0.7f, 0.65f, 1.0f)
                        : UxImVec4(0.25f, 0.28f, 0.32f, 1.0f);

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("GhostBusterToggle");
                    if (UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f))))
                    {
                        ctx.ngx.isGhostBustingEnabled = !ctx.ngx.isGhostBustingEnabled;
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
                else
                {
                    // Show N/A when FG not active
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("GhostBusterDisabled");
                    UxImGui::Button("N/A", UxImVec2(S(40.0f), S(22.0f)));

                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        UxImGui::Text("Frame Generation must be enabled in game");
                        UxImGui::EndTooltip();
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            // =====================================================
            // Separator line
            // =====================================================
            UxImGui::Dummy(UxImVec2(0, 8));
            UxImGui::SetCursorPosX(S(15.0f));
            drawList->AddLine(
                UxImVec2(windowPos.x + S(15.0f), UxImGui::GetCursorScreenPos().y),
                UxImVec2(windowPos.x + panelWidth - S(15.0f), UxImGui::GetCursorScreenPos().y),
                IM_COL32(60, 70, 80, 255)
            );
            UxImGui::Dummy(UxImVec2(0, 12));

            // =====================================================
            // PERFORMANCE Section
            // =====================================================
            UxImGui::SetCursorPosX(S(15.0f));
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("PERFORMANCE");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 6));

            // V-SYNC - Three state toggle: APP (game controls) / ON / OFF
            // Works even if game uses ALLOW_TEARING - we override it
            // Disabled when Overdrive is active
            {
                bool vsyncControlEnabled = !OverdriveController::IsActive();

                UxImGui::SetCursorPosX(S(15.0f));

                bool isOverrideEnabled = ctx.reflex.isVsyncOverrideEnabled;
                bool isVsyncOn = ctx.reflex.isVsyncEnabled;

                UxImGui::PushStyleColor(UxImGuiCol_Text, vsyncControlEnabled ? textEnabled : textDisabled);
                UxImGui::Text("V-SYNC");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(55.0f));

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
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                UxImGui::PushID("VSyncToggle");
                if (vsyncControlEnabled && UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f))))
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
                    UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f)));
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

            // Limit slider (30-360) -> ctx.reflex.desiredFpsLimit
            {
                bool limitEnabled = ctx.reflex.isFpsLimitEnabled;
                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, limitEnabled ? textEnabled : textDisabled);
                UxImGui::Text("Limit");
                UxImGui::PopStyleColor();

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, limitEnabled ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, limitEnabled ? UxImVec4(0.0f, 0.75f, 0.7f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, limitEnabled ? UxImVec4(0.0f, 0.85f, 0.8f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, S(4.0f));
                UxImGui::PushItemWidth(panelWidth - S(30.0f));

                if (limitEnabled)
                    UxImGui::SliderInt("##FPSLimitSlider", &ctx.reflex.desiredFpsLimit, 30, 360, "%d");
                else
                {
                    UxImGui::BeginDisabled(true);
                    UxImGui::SliderInt("##FPSLimitSlider", &ctx.reflex.desiredFpsLimit, 30, 360, "%d");
                    UxImGui::EndDisabled();
                }

                UxImGui::PopItemWidth();
                UxImGui::PopStyleVar(2);
                UxImGui::PopStyleColor(3);
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

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, boostControlEnabled ? textEnabled : textDisabled);
                UxImGui::Text("Reflex Boost");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(55.0f));

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
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("ReflexBoostToggle");
                    if (UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f))))
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
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

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

                    UxImGui::PushID("ReflexBoostDisabled");
                    UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f)));

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
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Overdrive mode - only show if not disabled (-1)
            // -1 = hidden, 0 = OFF, 1 = Performance, 2 = Quality, 3 = Latency
            if (ctx.overdriveMode >= 0)
            {
                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, textEnabled);
                UxImGui::Text("Overdrive");
                UxImGui::PopStyleColor();

                // "ALPHA" superscript in red
                UxImGui::SameLine(0, 0);
                UxImGui::SetCursorPosY(UxImGui::GetCursorPosY() - 4.0f);  // Move up for superscript
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.9f, 0.2f, 0.2f, 1.0f));  // Red
                UxImGui::SetWindowFontScale(0.7f * g_UiScale);  // Smaller font
                UxImGui::Text("ALPHA");
                UxImGui::SetWindowFontScale(g_UiScale);  // Reset font scale
                UxImGui::PopStyleColor();
                UxImGui::SetCursorPosY(UxImGui::GetCursorPosY() + 4.0f);  // Reset Y position

                UxImGui::SameLine(panelWidth - S(95.0f));

                const char* overdriveItems[] = { "OFF", "PERF", "QUALITY", "LATENCY" };

                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, UxImVec4(0.1f, 0.12f, 0.15f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered, UxImVec4(0.15f, 0.18f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBgActive, UxImVec4(0.1f, 0.12f, 0.15f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.18f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.2f, 0.24f, 0.28f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Header, UxImVec4(0.0f, 0.5f, 0.45f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_HeaderHovered, UxImVec4(0.0f, 0.6f, 0.55f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                UxImGui::PushItemWidth(S(80.0f));

                {
                    ComboFontScaleGuard fontGuard; // Ensure popup inherits UI scale
                    UxImGui::Combo("##OverdriveCombo", &ctx.overdriveMode, overdriveItems, 4);
                }

                UxImGui::PopItemWidth();
                UxImGui::PopStyleVar();
                UxImGui::PopStyleColor(7);
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // ---- DFG (Dynamic Frame Generation) section ----
            // Whole DFG block is force-disabled when MFG Mode Override is active
            // (ctx.nvapi.mfgEnforcedMode != 0), because a hard NVAPI override of the
            // FG multiplier makes runtime DFG decisions meaningless / conflicting.
            const bool mfgOverrideActive = (ctx.nvapi.mfgEnforcedMode != 0);

            if (mfgOverrideActive)
            {
                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.85f, 0.65f, 0.20f, 1.0f));
                UxImGui::TextWrapped("DFG disabled while MFG Mode Override is set.");
                UxImGui::PopStyleColor();
                UxImGui::Dummy(UxImVec2(0, 2));
            }

            UxImGui::BeginDisabled(mfgOverrideActive);

            // Dynamic Frame Generation toggle -> ctx.streamline.isDynamicMfgEnabled
            // Available when Streamline is loaded with version >= 2.7
            {
                bool streamlineLoaded = false;
                bool streamlineVersionOk = false;
                {
                    const std::wstring& ver = ctx.streamline.interposerVersion;
                    int major = 0, minor = 0;
                    if (swscanf_s(ver.c_str(), L"%d.%d", &major, &minor) >= 2)
                    {
                        streamlineLoaded = (major > 0 || minor > 0);
                        streamlineVersionOk = (major > 2) || (major == 2 && minor >= 7);
                    }
                }
                bool dmfgAvailable = streamlineLoaded && streamlineVersionOk && !mfgOverrideActive;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, dmfgAvailable ? textEnabled : textDisabled);
                UxImGui::Text("Dynamic Frame Generation");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(55.0f));

                if (dmfgAvailable)
                {
                    const char* btnLabel = ctx.streamline.isDynamicMfgEnabled ? "ON" : "OFF";
                    UxImVec4 bgColor = ctx.streamline.isDynamicMfgEnabled
                        ? UxImVec4(0.0f, 0.6f, 0.55f, 1.0f)
                        : UxImVec4(0.2f, 0.22f, 0.25f, 1.0f);
                    UxImVec4 hoverColor = ctx.streamline.isDynamicMfgEnabled
                        ? UxImVec4(0.0f, 0.7f, 0.65f, 1.0f)
                        : UxImVec4(0.25f, 0.28f, 0.32f, 1.0f);

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("DynamicFrameGenToggle");
                    if (UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f))))
                    {
                        ctx.streamline.isDynamicMfgEnabled = !ctx.streamline.isDynamicMfgEnabled;
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
                else
                {
                    // Disabled - show N/A with tooltip
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("DynamicFrameGenDisabled");
                    UxImGui::Button("N/A", UxImVec2(S(40.0f), S(22.0f)));

                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        if (!streamlineLoaded)
                            UxImGui::Text("Streamline not detected");
                        else
                            UxImGui::Text("Requires Streamline 2.7.0 or newer");
                        UxImGui::EndTooltip();
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // DFG Mode toggle: AUTO (0) / CUSTOM (1) -> ctx.streamline.dfgMode
            // In AUTO mode, user sets Target FPS and thresholds are computed automatically
            // In CUSTOM mode, user manually adjusts individual threshold sliders
            {
                bool dmfgAvailableForMode = ctx.streamline.isDynamicMfgEnabled && !mfgOverrideActive;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, dmfgAvailableForMode ? textEnabled : textDisabled);
                UxImGui::Text("DFG Mode");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(85.0f));

                if (dmfgAvailableForMode)
                {
                    const char* modeLabel = (ctx.streamline.dfgMode == 0) ? "AUTO" : "CUSTOM";
                    UxImVec4 bgColor = (ctx.streamline.dfgMode == 0)
                        ? UxImVec4(0.0f, 0.6f, 0.55f, 1.0f)
                        : UxImVec4(0.55f, 0.35f, 0.0f, 1.0f);
                    UxImVec4 hoverColor = (ctx.streamline.dfgMode == 0)
                        ? UxImVec4(0.0f, 0.7f, 0.65f, 1.0f)
                        : UxImVec4(0.65f, 0.45f, 0.0f, 1.0f);

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("DfgModeToggle");
                    if (UxImGui::Button(modeLabel, UxImVec2(S(70.0f), S(22.0f))))
                    {
                        ctx.streamline.dfgMode = (ctx.streamline.dfgMode == 0) ? 1 : 0;

                        // When switching to AUTO, recalculate thresholds from current target FPS
                        if (ctx.streamline.dfgMode == 0)
                        {
                            int target = ctx.streamline.dfgTargetFps;
                            ctx.streamline.dynamicMfgThreshold2 = (target + 1) / 2;  // ceil(target/2)
                            ctx.streamline.dynamicMfgThreshold3 = (target + 2) / 3;  // ceil(target/3)
                            ctx.streamline.dynamicMfgThreshold4 = (target + 3) / 4;  // ceil(target/4)
                        }
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
                else
                {
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("DfgModeDisabled");
                    UxImGui::Button("N/A", UxImVec2(S(70.0f), S(22.0f)));
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // DFG Instinct toggle -> ctx.streamline.isMinDynamicFpsActive
            // Enables action-intensity-based latency reduction within DFG.
            // Only available when DFG is enabled.
            {
                bool instinctAvailable = ctx.streamline.isDynamicMfgEnabled && !mfgOverrideActive;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, instinctAvailable ? textEnabled : textDisabled);
                UxImGui::Text("DFG Instinct");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(55.0f));

                if (instinctAvailable)
                {
                    const char* btnLabel = ctx.streamline.isMinDynamicFpsActive ? "ON" : "OFF";
                    UxImVec4 bgColor = ctx.streamline.isMinDynamicFpsActive
                        ? UxImVec4(0.55f, 0.35f, 0.0f, 1.0f)   // Amber ON
                        : UxImVec4(0.2f, 0.22f, 0.25f, 1.0f);
                    UxImVec4 hoverColor = ctx.streamline.isMinDynamicFpsActive
                        ? UxImVec4(0.65f, 0.45f, 0.0f, 1.0f)
                        : UxImVec4(0.25f, 0.28f, 0.32f, 1.0f);

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("DfgInstinctToggle");
                    if (UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f))))
                    {
                        ctx.streamline.isMinDynamicFpsActive = !ctx.streamline.isMinDynamicFpsActive;
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
                else
                {
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("DfgInstinctDisabled");
                    UxImGui::Button("N/A", UxImVec2(S(40.0f), S(22.0f)));
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Minimum FPS slider -> ctx.streamline.minDynamicFps
            // When DFG Instinct is active, latency reduction won't lower the
            // MFG multiplier below the point where output FPS would drop under this value.
            // Only active when DFG Instinct is ON (and DFG itself is enabled).
            {
                bool dmfgEnabled = ctx.streamline.isDynamicMfgEnabled && !mfgOverrideActive;
                bool instinctOn = ctx.streamline.isMinDynamicFpsActive;
                bool sliderActive = dmfgEnabled && instinctOn;

                const int minFpsMin = 30;
                const int minFpsMax = 240;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, sliderActive ? UxImVec4(0.65f, 0.7f, 0.75f, 1.0f) : textDisabled);
                UxImGui::Text("Minimum FPS");
                UxImGui::PopStyleColor();

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, sliderActive ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, sliderActive ? UxImVec4(0.55f, 0.35f, 0.0f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, sliderActive ? UxImVec4(0.65f, 0.45f, 0.0f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, S(4.0f));
                UxImGui::PushItemWidth(panelWidth - S(30.0f));

                int minFpsInt = (int)ctx.streamline.minDynamicFps;

                if (sliderActive)
                    UxImGui::SliderInt("##DfgMinFps", &minFpsInt, minFpsMin, minFpsMax, "%d FPS");
                else
                {
                    UxImGui::BeginDisabled(true);
                    UxImGui::SliderInt("##DfgMinFps", &minFpsInt, minFpsMin, minFpsMax, "%d FPS");
                    UxImGui::EndDisabled();
                }

                ctx.streamline.minDynamicFps = (float)minFpsInt;

                UxImGui::PopItemWidth();
                UxImGui::PopStyleVar(2);
                UxImGui::PopStyleColor(3);

                UxImGui::Dummy(UxImVec2(0, 2));
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Target FPS slider - only active when DFG Mode = AUTO (dfgMode == 0)
            // Changes here auto-compute thresholds: threshold_N = ceil(targetFPS / N)
            {
                bool dmfgEnabled = ctx.streamline.isDynamicMfgEnabled && !mfgOverrideActive;
                bool isAutoMode = (ctx.streamline.dfgMode == 0);
                bool sliderActive = dmfgEnabled && isAutoMode;

                const int targetMin = 30;
                const int targetMax = 300;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, sliderActive ? UxImVec4(0.65f, 0.7f, 0.75f, 1.0f) : textDisabled);
                UxImGui::Text("Target FPS (auto thresholds)");
                UxImGui::PopStyleColor();

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, sliderActive ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, sliderActive ? UxImVec4(0.0f, 0.6f, 0.55f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, sliderActive ? UxImVec4(0.0f, 0.7f, 0.65f, 1.0f) : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, S(4.0f));
                UxImGui::PushItemWidth(panelWidth - S(30.0f));

                int prevTarget = ctx.streamline.dfgTargetFps;

                if (sliderActive)
                    UxImGui::SliderInt("##DfgTargetFps", &ctx.streamline.dfgTargetFps, targetMin, targetMax, "%d FPS");
                else
                {
                    UxImGui::BeginDisabled(true);
                    UxImGui::SliderInt("##DfgTargetFps", &ctx.streamline.dfgTargetFps, targetMin, targetMax, "%d FPS");
                    UxImGui::EndDisabled();
                }

                UxImGui::PopItemWidth();
                UxImGui::PopStyleVar(2);
                UxImGui::PopStyleColor(3);

                // Auto-compute thresholds when target FPS changes in AUTO mode
                if (sliderActive && ctx.streamline.dfgTargetFps != prevTarget)
                {
                    int target = ctx.streamline.dfgTargetFps;
                    ctx.streamline.dynamicMfgThreshold2 = (target + 1) / 2;  // ceil(target/2)
                    ctx.streamline.dynamicMfgThreshold3 = (target + 2) / 3;  // ceil(target/3)
                    ctx.streamline.dynamicMfgThreshold4 = (target + 3) / 4;  // ceil(target/4)
                    ctx.streamline.dynamicMfgThreshold5 = (target + 4) / 5;  // ceil(target/5)
                    ctx.streamline.dynamicMfgThreshold6 = (target + 5) / 6;  // ceil(target/6)
                }

                UxImGui::Dummy(UxImVec2(0, 2));
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Multiplier Thresholds - 5 individual sliders
            // x6 threshold <= x5 threshold <= x4 threshold <= x3 threshold <= x2 threshold
            // ctx.streamline.dynamicMfgThreshold6 (default 20)
            // ctx.streamline.dynamicMfgThreshold5 (default 24)
            // ctx.streamline.dynamicMfgThreshold4 (default 30)
            // ctx.streamline.dynamicMfgThreshold3 (default 60)
            // ctx.streamline.dynamicMfgThreshold2 (default 90)
            // In AUTO mode (dfgMode == 0), these are grayed out (read-only, computed from target FPS)
            // In CUSTOM mode (dfgMode == 1), these are manually adjustable
            {
                bool dmfgEnabled = ctx.streamline.isDynamicMfgEnabled && !mfgOverrideActive;
                bool isCustomMode = (ctx.streamline.dfgMode == 1);
                bool slidersActive = dmfgEnabled && isCustomMode;
                const int sliderMin = 10;
                const int sliderMax = 200;
                const int handleGap = 5; // minimum FPS gap between thresholds

                // Slider style colors per mode
                struct ThresholdSlider {
                    const char* label;
                    const char* sliderId;
                    int* value;
                    UxImVec4 grabColor;
                    UxImVec4 grabActiveColor;
                };

                ThresholdSlider sliders[5] = {
                    { "x6 threshold (below = 6X)", "##MfgThresh6", &ctx.streamline.dynamicMfgThreshold6,
                      UxImVec4(0.90f, 0.20f, 0.30f, 1.0f), UxImVec4(1.00f, 0.30f, 0.40f, 1.0f) },  // Red
                    { "x5 threshold (below = 5X)", "##MfgThresh5", &ctx.streamline.dynamicMfgThreshold5,
                      UxImVec4(0.90f, 0.50f, 0.20f, 1.0f), UxImVec4(1.00f, 0.60f, 0.30f, 1.0f) },  // Orange
                    { "x4 threshold (below = 4X)", "##MfgThresh4", &ctx.streamline.dynamicMfgThreshold4,
                      UxImVec4(0.50f, 0.30f, 0.70f, 1.0f), UxImVec4(0.60f, 0.40f, 0.80f, 1.0f) },  // Purple
                    { "x3 threshold (below = 3X)", "##MfgThresh3", &ctx.streamline.dynamicMfgThreshold3,
                      UxImVec4(0.00f, 0.75f, 0.68f, 1.0f), UxImVec4(0.00f, 0.85f, 0.78f, 1.0f) },  // Teal
                    { "x2 threshold (below = 2X)", "##MfgThresh2", &ctx.streamline.dynamicMfgThreshold2,
                      UxImVec4(0.25f, 0.55f, 1.00f, 1.0f), UxImVec4(0.35f, 0.65f, 1.00f, 1.0f) },  // Blue
                };

                for (int si = 0; si < 5; si++)
                {
                    UxImGui::SetCursorPosX(S(15.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, slidersActive ? UxImVec4(0.65f, 0.7f, 0.75f, 1.0f) : textDisabled);
                    UxImGui::Text("%s", sliders[si].label);
                    UxImGui::PopStyleColor();

                    UxImGui::SetCursorPosX(S(15.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBg, slidersActive ? UxImVec4(0.1f, 0.12f, 0.15f, 1.0f) : sliderBgDisabled);
                    UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, slidersActive ? sliders[si].grabColor : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, slidersActive ? sliders[si].grabActiveColor : UxImVec4(0.2f, 0.2f, 0.22f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, S(4.0f));
                    UxImGui::PushItemWidth(panelWidth - S(30.0f));

                    int prevVal = *sliders[si].value;

                    if (slidersActive)
                        UxImGui::SliderInt(sliders[si].sliderId, sliders[si].value, sliderMin, sliderMax, "%d FPS");
                    else
                    {
                        // Disabled - both when DFG is off AND when in AUTO mode
                        UxImGui::BeginDisabled(true);
                        UxImGui::SliderInt(sliders[si].sliderId, sliders[si].value, sliderMin, sliderMax, "%d FPS");
                        UxImGui::EndDisabled();
                    }

                    UxImGui::PopItemWidth();
                    UxImGui::PopStyleVar(2);
                    UxImGui::PopStyleColor(3);

                    // Enforce ordering: x6 <= x5 <= x4 <= x3 <= x2 with minimum gap (only in CUSTOM mode)
                    if (slidersActive && *sliders[si].value != prevVal)
                    {
                        // Clamp x6 <= x5 - gap
                        if (ctx.streamline.dynamicMfgThreshold6 > ctx.streamline.dynamicMfgThreshold5 - handleGap)
                            ctx.streamline.dynamicMfgThreshold6 = ctx.streamline.dynamicMfgThreshold5 - handleGap;
                        // Clamp x5 <= x4 - gap
                        if (ctx.streamline.dynamicMfgThreshold5 > ctx.streamline.dynamicMfgThreshold4 - handleGap)
                            ctx.streamline.dynamicMfgThreshold5 = ctx.streamline.dynamicMfgThreshold4 - handleGap;
                        // Clamp x4 <= x3 - gap
                        if (ctx.streamline.dynamicMfgThreshold4 > ctx.streamline.dynamicMfgThreshold3 - handleGap)
                            ctx.streamline.dynamicMfgThreshold4 = ctx.streamline.dynamicMfgThreshold3 - handleGap;
                        // Clamp x3 <= x2 - gap
                        if (ctx.streamline.dynamicMfgThreshold3 > ctx.streamline.dynamicMfgThreshold2 - handleGap)
                            ctx.streamline.dynamicMfgThreshold3 = ctx.streamline.dynamicMfgThreshold2 - handleGap;
                        // Re-clamp downward chain (in case pushes cascaded)
                        if (ctx.streamline.dynamicMfgThreshold4 > ctx.streamline.dynamicMfgThreshold3 - handleGap)
                            ctx.streamline.dynamicMfgThreshold4 = ctx.streamline.dynamicMfgThreshold3 - handleGap;
                        if (ctx.streamline.dynamicMfgThreshold5 > ctx.streamline.dynamicMfgThreshold4 - handleGap)
                            ctx.streamline.dynamicMfgThreshold5 = ctx.streamline.dynamicMfgThreshold4 - handleGap;
                        if (ctx.streamline.dynamicMfgThreshold6 > ctx.streamline.dynamicMfgThreshold5 - handleGap)
                            ctx.streamline.dynamicMfgThreshold6 = ctx.streamline.dynamicMfgThreshold5 - handleGap;
                        // Floor
                        if (ctx.streamline.dynamicMfgThreshold6 < sliderMin)
                            ctx.streamline.dynamicMfgThreshold6 = sliderMin;
                    }

                    UxImGui::Dummy(UxImVec2(0, 2));
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            UxImGui::EndDisabled();  // ---- end of DFG section ----

            // MFG Mode Override - dropdown: OFF / x1 / x2 / x3 / x4 [/ x5 / x6 if dlssg >= 2.11]
            //
            // Maps to ctx.nvapi.mfgEnforcedMode (UI-mnemonic encoding):
            //   0 = OFF  (no override, game fully controls FG)
            //   1 = x1   (force FG disabled)
            //   2 = x2   (force FG on, 2X total)
            //   3 = x3   (force FG on, 3X total)
            //   4 = x4   (force FG on, 4X total)
            //   5 = x5   (force FG on, 5X total - needs sl.dlss_g >= 2.11)
            //   6 = x6   (force FG on, 6X total - needs sl.dlss_g >= 2.11)
            //
            // Requires Streamline >= 2.7.0 for any override; x5/x6 needs sl.dlss_g >= 2.11
            {
                // Parse Streamline version to check >= 2.7.x
                // ctx.streamline.interposerVersion is e.g. L"2.10.0.0", default L"0.0.0.0"
                bool streamlineLoaded = false;
                bool streamlineVersionOk = false;
                bool dlssg211 = false;  // sl.dlss_g >= 2.11 (5X/6X capable)
                {
                    const std::wstring& ver = ctx.streamline.interposerVersion;
                    int major = 0, minor = 0;
                    if (swscanf_s(ver.c_str(), L"%d.%d", &major, &minor) >= 2)
                    {
                        streamlineLoaded = (major > 0 || minor > 0);
                        streamlineVersionOk = (major > 2) || (major == 2 && minor >= 7);
                    }

                    // Check dlssg plugin version for 5X/6X support
                    const std::wstring& dlssgVer = ctx.streamline.dlssgVersion;
                    int dmaj = 0, dmin = 0;
                    if (swscanf_s(dlssgVer.c_str(), L"%d.%d", &dmaj, &dmin) >= 2)
                    {
                        dlssg211 = (dmaj > 2) || (dmaj == 2 && dmin >= 11);
                    }
                }
                bool mfgEnabled = streamlineLoaded && streamlineVersionOk;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, mfgEnabled ? textEnabled : textDisabled);
                UxImGui::Text("MFG Mode Override");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(65.0f));

                if (mfgEnabled)
                {
                    // Clamp DISPLAY value to valid range; don't mutate ctx silently
                    // (silent mutation would trigger unsaved-changes flag on load).
                    // If user actually interacts with combo, the new selection will overwrite ctx.
                    int currentMode = ctx.nvapi.mfgEnforcedMode;
                    int displayMode = currentMode;
                    if (displayMode < 0 || displayMode > 6) displayMode = 0;
                    if (!dlssg211 && displayMode > 4) displayMode = 0;

                    // Per-item background color for visual feedback
                    UxImVec4 bgColor;
                    switch (displayMode)
                    {
                    case 1: bgColor = UxImVec4(0.35f, 0.35f, 0.38f, 1.0f); break; // x1 - muted gray
                    case 2: bgColor = UxImVec4(0.15f, 0.45f, 0.65f, 1.0f); break; // x2 - blue
                    case 3: bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f); break;   // x3 - teal
                    case 4: bgColor = UxImVec4(0.5f, 0.3f, 0.7f, 1.0f); break;    // x4 - purple
                    case 5: bgColor = UxImVec4(0.9f, 0.5f, 0.2f, 1.0f); break;    // x5 - orange
                    case 6: bgColor = UxImVec4(0.9f, 0.2f, 0.3f, 1.0f); break;    // x6 - red
                    default: bgColor = UxImVec4(0.2f, 0.22f, 0.25f, 1.0f); break; // OFF
                    }

                    UxImGui::PushStyleColor(UxImGuiCol_FrameBg, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered,
                        UxImVec4(bgColor.x + 0.05f, bgColor.y + 0.05f, bgColor.z + 0.05f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBgActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered,
                        UxImVec4(bgColor.x + 0.05f, bgColor.y + 0.05f, bgColor.z + 0.05f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::SetNextItemWidth(S(50.0f));
                    UxImGui::PushID("MFGModeOverrideCombo");

                    // Items: always include 0..4, include 5/6 only when sl.dlss_g >= 2.11
                    static const char* kItems6[] = { "OFF", "x1", "x2", "x3", "x4", "x5", "x6" };
                    static const char* kItems4[] = { "OFF", "x1", "x2", "x3", "x4" };
                    const char* const* items = dlssg211 ? kItems6 : kItems4;
                    const int itemCount = dlssg211 ? 7 : 5;

                    int selected = displayMode; // 0..6 matches index directly
                    bool comboChanged;
                    {
                        ComboFontScaleGuard fontGuard; // Ensure popup inherits UI scale
                        comboChanged = UxImGui::Combo("##mfgmodeoverride", &selected, items, itemCount);
                    }
                    if (comboChanged)
                    {
                        ctx.nvapi.mfgEnforcedMode = selected;

                        // Reset dismiss state when value changes
                        SidePanel::g_MfgRestartWarningDismissed = false;

                        // Apply immediately via Streamline (safe - both have init guards).
                        // Any non-zero nvapi value (including x1) goes through ForceApply;
                        // switching back to OFF hands control back to the game via Restore.
                        if (ctx.nvapi.mfgEnforcedMode != 0 || ctx.streamline.mfgEnforcedMode > 0)
                            StreamlineProxy_ForceApplyMfgMode();
                        else
                            StreamlineProxy_RestoreGameDLSSGOptions();
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(7);
                }
                else
                {
                    // Disabled - show N/A with tooltip
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("MFGModeOverrideDisabled");
                    UxImGui::Button("N/A", UxImVec2(S(50.0f), S(22.0f)));

                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        if (!streamlineLoaded)
                            UxImGui::Text("Streamline not detected");
                        else
                            UxImGui::Text("Requires Streamline 2.7.0 or newer");
                        UxImGui::EndTooltip();
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 4));

            // Forced FG Activation - NO/YES -> ctx.streamline.forceLoadDLSSG
            {
                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, textEnabled);
                UxImGui::Text("Forced FG Activation");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(55.0f));

                const char* btnLabel = ctx.streamline.forceLoadDLSSG ? "YES" : "NO";
                UxImVec4 bgColor = ctx.streamline.forceLoadDLSSG
                    ? UxImVec4(0.7f, 0.35f, 0.0f, 1.0f)   // Orange when YES (caution)
                    : UxImVec4(0.2f, 0.22f, 0.25f, 1.0f);  // Dark gray when NO
                UxImVec4 hoverColor = ctx.streamline.forceLoadDLSSG
                    ? UxImVec4(0.8f, 0.45f, 0.0f, 1.0f)
                    : UxImVec4(0.25f, 0.28f, 0.32f, 1.0f);

                UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                UxImGui::PushID("ForcedFGActivation");
                if (UxImGui::Button(btnLabel, UxImVec2(S(40.0f), S(22.0f))))
                {
                    ctx.streamline.forceLoadDLSSG = !ctx.streamline.forceLoadDLSSG;
                }
                UxImGui::PopID();

                UxImGui::PopStyleVar();
                UxImGui::PopStyleColor(4);
            }

            // =====================================================
            // Separator line
            // =====================================================
            UxImGui::Dummy(UxImVec2(0, 8));
            UxImGui::SetCursorPosX(S(15.0f));
            drawList->AddLine(
                UxImVec2(windowPos.x + S(15.0f), UxImGui::GetCursorScreenPos().y),
                UxImVec2(windowPos.x + panelWidth - S(15.0f), UxImGui::GetCursorScreenPos().y),
                IM_COL32(60, 70, 80, 255)
            );
            UxImGui::Dummy(UxImVec2(0, 12));

            // =====================================================
            // INTERFACE Section
            // =====================================================
            UxImGui::SetCursorPosX(S(15.0f));
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("INTERFACE");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 4));

            // Status Bar ON/OFF -> ctx.isMonitoringEnabled
            DrawToggle("Status Bar", &ctx.isMonitoringEnabled);

            // MFG hotkeys ON/OFF -> ctx.areHotKeysEnabled
            DrawToggle("MFG hotkeys", &ctx.areHotKeysEnabled);

            // =====================================================
            // Separator line
            // =====================================================
            UxImGui::Dummy(UxImVec2(0, 8));
            UxImGui::SetCursorPosX(S(15.0f));
            drawList->AddLine(
                UxImVec2(windowPos.x + S(15.0f), UxImGui::GetCursorScreenPos().y),
                UxImVec2(windowPos.x + panelWidth - S(15.0f), UxImGui::GetCursorScreenPos().y),
                IM_COL32(60, 70, 80, 255)
            );
            UxImGui::Dummy(UxImVec2(0, 12));

            // =====================================================
            // DEBUG Section
            // =====================================================
            UxImGui::SetCursorPosX(S(15.0f));
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(0.5f, 0.55f, 0.6f, 1.0f));
            UxImGui::Text("DEBUG");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 4));

            // Frame Generation backend selector [FSR3 / DLSSG]
            // Single cycle button: FSR3 -> DLSSG -> FSR3
            // Disabled (forced FSR3) when hardware doesn't support DLSSG
            {
                bool dlssgSupported = ctx.ngx.isDlssgSupportedByHardware;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, dlssgSupported ? textEnabled : textDisabled);
                UxImGui::Text("Frame Generation");
                UxImGui::PopStyleColor();

                UxImGui::SameLine(panelWidth - S(55.0f));

                if (dlssgSupported)
                {
                    // Determine label and color based on current state
                    const char* btnLabel;
                    UxImVec4 bgColor;
                    UxImVec4 hoverColor;

                    if (ctx.ngx.isDlssgDisabled)
                    {
                        btnLabel = "FSR3";
                        bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f);      // Teal
                        hoverColor = UxImVec4(0.0f, 0.7f, 0.65f, 1.0f);
                    }
                    else
                    {
                        btnLabel = "DLSSG";
                        bgColor = UxImVec4(0.5f, 0.3f, 0.7f, 1.0f);       // Purple
                        hoverColor = UxImVec4(0.6f, 0.4f, 0.8f, 1.0f);
                    }

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("FgBackendToggle");
                    if (UxImGui::Button(btnLabel, UxImVec2(S(45.0f), S(22.0f))))
                    {
                        // Cycle: FSR3 <-> DLSSG
                        ctx.ngx.isDlssgDisabled = !ctx.ngx.isDlssgDisabled;
                        SidePanel::g_FgBackendRestartWarningDismissed = false;
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
                else
                {
                    // Disabled - show FSR3 (locked) with tooltip
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("FgBackendDisabled");
                    UxImGui::Button("FSR3", UxImVec2(S(45.0f), S(22.0f)));

                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        UxImGui::Text("DLSS-G is not supported by your hardware.\nFSR3 is used as Frame Generation backend.");
                        UxImGui::EndTooltip();
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 6));

            // Hybrid MFG - dropdown: N/A / AUTO / ON
            //
            // Maps to ctx.ngx.isHybridMfgForced:
            //   false = AUTO (default - heuristic decides)
            //   true  = ON   (force hybrid MFG path)
            //
            // Display "N/A" (disabled) when Frame Generation backend is not DLSS-G
            // (i.e. when ctx.ngx.isDlssgDisabled == true, FSR3 is used).
            // Hybrid MFG is only meaningful with DLSS-G backend.
            //
            // Requires game restart to take effect (reuses RenderHybridMfgRestartWarningBar).
            {
                // Available only when DLSS-G is the selected backend
                // (and obviously only when DLSS-G is supported by hardware).
                bool dlssgActive = ctx.ngx.isDlssgSupportedByHardware && !ctx.ngx.isDlssgDisabled;

                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, dlssgActive ? textEnabled : textDisabled);
                UxImGui::Text("Hybrid MFG");
                UxImGui::PopStyleColor();

                // Reserve enough room for the 60px combo + 15px right margin
                // (smaller than HUD detection because items are shorter: AUTO/ON/N/A).
                UxImGui::SameLine(panelWidth - S(75.0f));

                if (dlssgActive)
                {
                    // DLSS-G active - editable dropdown: AUTO (false) / ON (true)
                    int displayMode = ctx.ngx.isHybridMfgForced ? 1 : 0;

                    // Per-item background color for visual feedback
                    UxImVec4 bgColor;
                    if (displayMode == 1)
                        bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f);   // ON  - teal
                    else
                        bgColor = UxImVec4(0.2f, 0.22f, 0.25f, 1.0f);  // AUTO - dark gray

                    UxImGui::PushStyleColor(UxImGuiCol_FrameBg, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered,
                        UxImVec4(bgColor.x + 0.05f, bgColor.y + 0.05f, bgColor.z + 0.05f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_FrameBgActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered,
                        UxImVec4(bgColor.x + 0.05f, bgColor.y + 0.05f, bgColor.z + 0.05f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::SetNextItemWidth(S(60.0f));
                    UxImGui::PushID("HybridMfgCombo");

                    // Items: 0 = AUTO, 1 = ON
                    static const char* kItems[] = { "AUTO", "ON" };
                    const int itemCount = 2;

                    int selected = displayMode;
                    bool comboChanged;
                    {
                        ComboFontScaleGuard fontGuard; // Ensure popup inherits UI scale
                        comboChanged = UxImGui::Combo("##hybridmfg", &selected, kItems, itemCount);
                    }
                    if (comboChanged)
                    {
                        ctx.ngx.isHybridMfgForced = (selected == 1);
                        SidePanel::g_HybridMfgRestartWarningDismissed = false;
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(7);
                }
                else
                {
                    // DLSS-G not active - show "N/A" disabled placeholder with tooltip
                    UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.15f, 0.15f, 0.18f, 1.0f));
                    UxImGui::PushStyleColor(UxImGuiCol_Text, textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                    UxImGui::PushID("HybridMfgDisabled");
                    UxImGui::Button("N/A", UxImVec2(S(60.0f), S(22.0f)));

                    if (UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        if (!ctx.ngx.isDlssgSupportedByHardware)
                            UxImGui::Text("Hybrid MFG requires DLSS-G hardware support.");
                        else
                            UxImGui::Text("Hybrid MFG is only available when\nFrame Generation backend is set to DLSS-G.");
                        UxImGui::EndTooltip();
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);
                }
            }

            UxImGui::Dummy(UxImVec2(0, 6));

            // HUD Detection - dropdown: AUTO / ON / OFF
            //
            // Maps to ctx.ngx.hudDetectionMode:
            //   0 = AUTO (default - heuristic decides per-game)
            //   1 = OFF  (force HUD detection disabled)
            //   2 = ON   (force HUD detection enabled)
            {
                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, textEnabled);
                UxImGui::Text("HUD detection");
                UxImGui::PopStyleColor();

                // Reserve enough room for the 60px combo + 15px right margin.
                UxImGui::SameLine(panelWidth - S(75.0f));

                // Clamp DISPLAY value to valid range; don't mutate ctx silently
                // (silent mutation would trigger unsaved-changes flag on load).
                int currentMode = ctx.ngx.hudDetectionMode;
                int displayMode = currentMode;
                if (displayMode < 0 || displayMode > 2) displayMode = 0;

                // Per-item background color for visual feedback
                UxImVec4 bgColor;
                switch (displayMode)
                {
                case 1: bgColor = UxImVec4(0.6f, 0.2f, 0.2f, 1.0f); break;   // OFF - red
                case 2: bgColor = UxImVec4(0.0f, 0.6f, 0.55f, 1.0f); break;  // ON  - teal
                default: bgColor = UxImVec4(0.2f, 0.22f, 0.25f, 1.0f); break; // AUTO - dark gray
                }

                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_FrameBgHovered,
                    UxImVec4(bgColor.x + 0.05f, bgColor.y + 0.05f, bgColor.z + 0.05f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_FrameBgActive, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered,
                    UxImVec4(bgColor.x + 0.05f, bgColor.y + 0.05f, bgColor.z + 0.05f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));

                UxImGui::SetNextItemWidth(S(60.0f));
                UxImGui::PushID("HudDetectionCombo");

                // Items: 0 = AUTO, 1 = OFF, 2 = ON (matches ctx.ngx.hudDetectionMode encoding)
                static const char* kItems[] = { "GAME", "OFF", "ON" };
                const int itemCount = 3;

                int selected = displayMode; // 0..2 matches index directly
                bool comboChanged;
                {
                    ComboFontScaleGuard fontGuard; // Ensure popup inherits UI scale
                    comboChanged = UxImGui::Combo("##huddetection", &selected, kItems, itemCount);
                }
                if (comboChanged)
                {
                    ctx.ngx.hudDetectionMode = selected;
                }
                UxImGui::PopID();

                UxImGui::PopStyleVar();
                UxImGui::PopStyleColor(7);
            }

            UxImGui::Dummy(UxImVec2(0, 6));

            // Initialize Optiscaler earlier -> ctx.ngx.isEarlyInitEnabled
            // Workaround for games where Optiscaler UI doesn't appear on NVIDIA cards
            // Greyed out when real NGX is not present (non-NVIDIA)
            {
                bool nvidiaPresent = ctx.ngx.isRealNgxPresent;
                DrawToggle("Init Optiscaler early", &ctx.ngx.isEarlyInitEnabled, nvidiaPresent);
                if (UxImGui::IsItemHovered())
                {
                    UxImGui::BeginTooltip();
                    if (!nvidiaPresent)
                        UxImGui::Text("This option is only available on NVIDIA GPUs");
                    else
                        UxImGui::Text("Workaround for games where Optiscaler UI doesn't appear.\nMay disable native DLSS/DLSS-D.");
                    UxImGui::EndTooltip();
                }
            }

            UxImGui::Dummy(UxImVec2(0, 6));

            // Debug visualization toggles - only enabled when Frame Generation is active
            // These control bits in ctx.flags
            {
                bool fgActive = ctx.ngx.isFrameGenerationActive;
                bool ghostBusterActive = fgActive && ctx.ngx.isGhostBustingEnabled;

                // FG Debug label on its own line
                UxImGui::SetCursorPosX(S(15.0f));
                UxImGui::PushStyleColor(UxImGuiCol_Text, fgActive ? textEnabled : textDisabled);
                UxImGui::Text("FG Debug");
                UxImGui::PopStyleColor();

                UxImGui::Dummy(UxImVec2(0, 2));

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
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(3.0f));

                    UxImGui::PushID(label);
                    if (fgActive && UxImGui::Button(label, UxImVec2(S(width), S(20.0f))))
                    {
                        // Toggle the flag bit
                        ctx.flags ^= flag;
                    }
                    else if (!fgActive)
                    {
                        UxImGui::Button(label, UxImVec2(S(width), S(20.0f)));
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

                // GhostBuster (Anti-Ghosting) flag constants
                const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE = 0x00100000;
                const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT = 0x00200000;
                const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN = 0x00400000;
                const uint32_t MFG_DEBUG_FLAG_CAMERA_MV_DEBUG = 0x00800000;
                const uint32_t MFG_DEBUG_FLAG_TRAPEZOID_VIS = 0x01000000;

                // Update ctx.flags based on GhostBuster settings
                // Clear all anti-ghosting flags first
                ctx.flags &= ~(MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE |
                    MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT |
                    MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN);

                // Set ENABLE flag if GhostBuster is enabled
                if (ctx.ngx.isGhostBustingEnabled) {
                    ctx.flags |= MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE;
                }

                // Set debug mode flags based on ghostBusterDebugMode
                if (ctx.ghostBusterDebugMode == 1) {
                    ctx.flags |= MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN;
                }
                else if (ctx.ghostBusterDebugMode == 2) {
                    ctx.flags |= MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT;
                }

                // All debug toggles in one row
                UxImGui::SetCursorPosX(S(15.0f));
                DrawDebugToggle("FRAME", MFG_DEBUG_FLAG_FRAME_INDEX_LINE, 38.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("HUD", MFG_DEBUG_FLAG_HUD_DETECTION, 30.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("DIFF", MFG_DEBUG_FLAG_DISOCCLUSION_TINT, 32.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("GLITCH", MFG_DEBUG_FLAG_ARTIFACTS_DETECTION, 42.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("CAMV", MFG_DEBUG_FLAG_CAMERA_MV_DEBUG, 36.0f);
                UxImGui::SameLine(0, 2);
                DrawDebugToggle("TRAP", MFG_DEBUG_FLAG_TRAPEZOID_VIS, 34.0f);

                // GHOST debug toggle - Three state: OFF (0) -> SPLIT (1) -> TINT (2) -> OFF (0)
                // Only enabled when GhostBuster is active
                // All states show "GHOST" label, color indicates mode
                UxImGui::SameLine(0, 2);
                {
                    int debugMode = ctx.ghostBusterDebugMode;
                    const char* label = "GHOST";
                    UxImVec4 bgColor;
                    UxImVec4 hoverColor;

                    if (!ghostBusterActive) {
                        // Disabled state - dark gray
                        bgColor = UxImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                        hoverColor = bgColor;
                    }
                    else if (debugMode == 0) {
                        // OFF - dark gray (same as other OFF toggles)
                        bgColor = UxImVec4(0.25f, 0.25f, 0.28f, 1.0f);
                        hoverColor = UxImVec4(0.3f, 0.3f, 0.35f, 1.0f);
                    }
                    else if (debugMode == 1) {
                        // SPLIT screen mode - yellow/gold
                        bgColor = UxImVec4(0.5f, 0.4f, 0.0f, 1.0f);
                        hoverColor = UxImVec4(0.6f, 0.5f, 0.1f, 1.0f);
                    }
                    else {
                        // TINT mode (debugMode == 2) - red
                        bgColor = UxImVec4(0.6f, 0.2f, 0.2f, 1.0f);
                        hoverColor = UxImVec4(0.7f, 0.3f, 0.3f, 1.0f);
                    }

                    UxImGui::PushStyleColor(UxImGuiCol_Button, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, hoverColor);
                    UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, bgColor);
                    UxImGui::PushStyleColor(UxImGuiCol_Text, ghostBusterActive ? UxImVec4(1.0f, 1.0f, 1.0f, 1.0f) : textDisabled);
                    UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(3.0f));

                    UxImGui::PushID("GhostDebugToggle");
                    if (ghostBusterActive && UxImGui::Button(label, UxImVec2(S(38.0f), S(20.0f))))
                    {
                        // Cycle: 0 -> 1 -> 2 -> 0
                        ctx.ghostBusterDebugMode = (ctx.ghostBusterDebugMode + 1) % 3;
                    }
                    else if (!ghostBusterActive)
                    {
                        UxImGui::Button(label, UxImVec2(S(38.0f), S(20.0f)));
                    }
                    UxImGui::PopID();

                    UxImGui::PopStyleVar();
                    UxImGui::PopStyleColor(4);

                    // Tooltip explaining why disabled
                    if (!ghostBusterActive && UxImGui::IsItemHovered())
                    {
                        UxImGui::BeginTooltip();
                        if (!fgActive)
                            UxImGui::Text("Frame Generation must be active");
                        else
                            UxImGui::Text("Enable GhostBuster first");
                        UxImGui::EndTooltip();
                    }
                }

                // Tooltip for other debug toggles when FG disabled
                if (!fgActive && UxImGui::IsItemHovered())
                {
                    UxImGui::BeginTooltip();
                    UxImGui::Text("Frame Generation must be active");
                    UxImGui::EndTooltip();
                }
            }

            // Add some padding at the bottom for scrolling
            UxImGui::Dummy(UxImVec2(0, 15));

            // =====================================================
            // DLSS Enabler version label (bottom of side panel)
            // =====================================================
            {
                static std::string s_dlssEnablerVersion;  // UTF-8 for ImGui
                if (s_dlssEnablerVersion.empty())
                {
                    auto fullModulePath = Common::GetModuleFilePath();
                    std::wstring wver = Common::GetFileVersion(fullModulePath.c_str());
                    if (!wver.empty())
                    {
                        int needed = WideCharToMultiByte(CP_UTF8, 0, wver.c_str(), (int)wver.size(),
                            nullptr, 0, nullptr, nullptr);
                        if (needed > 0)
                        {
                            s_dlssEnablerVersion.resize((size_t)needed);
                            WideCharToMultiByte(CP_UTF8, 0, wver.c_str(), (int)wver.size(),
                                &s_dlssEnablerVersion[0], needed, nullptr, nullptr);
                        }
                    }
                }

                if (!s_dlssEnablerVersion.empty())
                {
                    std::string versionLine = "v" + s_dlssEnablerVersion;

                    // Separator above version
                    UxImGui::Dummy(UxImVec2(0, S(4.0f)));
                    float sepX = S(15.0f);
                    float sepW = windowSize.x - S(30.0f);
                    drawList->AddLine(
                        UxImVec2(windowPos.x + sepX, UxImGui::GetCursorScreenPos().y),
                        UxImVec2(windowPos.x + sepX + sepW, UxImGui::GetCursorScreenPos().y),
                        IM_COL32(255, 255, 255, 25),
                        1.0f
                    );
                    UxImGui::Dummy(UxImVec2(0, S(6.0f)));

                    // Center horizontally within the panel
                    float textWidth = UxImGui::CalcTextSize(versionLine.c_str()).x;
                    float centerX = (windowSize.x - textWidth) * 0.5f;
                    if (centerX < S(15.0f)) centerX = S(15.0f);
                    UxImGui::SetCursorPosX(centerX);

                    UxImGui::TextColored(UxImVec4(0.50f, 0.55f, 0.60f, 1.0f), "%s", versionLine.c_str());
                    UxImGui::Dummy(UxImVec2(0, S(8.0f)));
                }
            }
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
            const float sliderWidth = S(50.0f);
            const float sliderHeight = S(150.0f);

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
            UxImVec2 sliderPos(panelMargin + panelWidth, sliderY - sliderHeight / 2 + S(11.0f));

            // Clamp to screen bounds
            if (sliderPos.y < panelMargin)
                sliderPos.y = panelMargin;
            if (sliderPos.y + sliderHeight > io.DisplaySize.y - panelMargin)
                sliderPos.y = io.DisplaySize.y - panelMargin - sliderHeight;

            UxImGui::SetNextWindowPos(sliderPos);
            UxImGui::SetNextWindowSize(UxImVec2(sliderWidth, sliderHeight));

            // Transparent window - we'll draw custom background
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(0.0f));
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(8), S(12)));
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, S(0.0f));
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
                    UxImVec2(sliderWinPos.x + S(3.0f), sliderWinPos.y + sliderWinSize.y),
                    IM_COL32(100, 90, 20, 255)  // Same dark yellow as background strip
                );

                // Vertical slider
                UxImGui::PushStyleColor(UxImGuiCol_FrameBg, UxImVec4(0.1f, 0.12f, 0.15f, 1.0f));
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrab, UxImVec4(0.6f, 0.55f, 0.15f, 1.0f));  // Dark yellow grab
                UxImGui::PushStyleColor(UxImGuiCol_SliderGrabActive, UxImVec4(0.7f, 0.65f, 0.2f, 1.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(4.0f));
                UxImGui::PushStyleVar(UxImGuiStyleVar_GrabRounding, S(4.0f));
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

    // =============================================================================
    // MFG Mode Override - Restart Warning Bar (bottom of screen)
    // Mirrors the monitoring bar style but positioned at bottom, red-ish tint
    // =============================================================================
    static void RenderMfgRestartWarningBar()
    {
        // MFG override is now applied at runtime via StreamlineProxy
        // (slDLSSGSetOptionsWrapper), no game restart needed.
        return;

        // Initialize tracking on first call (after Load has set ctx values)
        static bool mfgModeInitialized = false;
        if (!mfgModeInitialized)
        {
            SidePanel::g_MfgModeAtLoad = ctx.nvapi.mfgEnforcedMode;
            SidePanel::g_MfgModeLastSaved = ctx.nvapi.mfgEnforcedMode;
            mfgModeInitialized = true;
        }

        // Detect when user clicks Persist � update g_MfgModeLastSaved
        {
            static bool hadUnsavedLastFrame = false;
            bool hasUnsaved = SettingsPersistence::HasUnsavedChanges();
            if (hadUnsavedLastFrame && !hasUnsaved)
            {
                SidePanel::g_MfgModeLastSaved = ctx.nvapi.mfgEnforcedMode;
                SidePanel::g_DlssgDisabledLastSaved = ctx.ngx.isDlssgDisabled;
                SidePanel::g_HybridMfgForcedLastSaved = ctx.ngx.isHybridMfgForced;
            }
            hadUnsavedLastFrame = hasUnsaved;
        }

        // Bar visible when EITHER:
        // - current != atLoad (value changed since process start, restart needed)
        // - current != lastSaved (value changed since last save, save+restart needed)
        // This covers: change->save->change back scenario (lastSaved differs from current)
        bool needsRestart = (ctx.nvapi.mfgEnforcedMode != SidePanel::g_MfgModeAtLoad);
        bool needsSave = (ctx.nvapi.mfgEnforcedMode != SidePanel::g_MfgModeLastSaved);
        bool mfgChanged = needsRestart || needsSave;

        // Nothing to show if unchanged or dismissed
        if (!mfgChanged || SidePanel::g_MfgRestartWarningDismissed)
            return;

        UxImGuiIO& io = UxImGui::GetIO();

        // Match monitoring bar dimensions and shape
        const float barMargin = S(10.0f);
        const float barWidth = S(480.0f);
        const float barHeight = S(30.0f);

        // Position: bottom center, mirroring top bar placement
        UxImVec2 barPos((io.DisplaySize.x - barWidth) * 0.5f, io.DisplaySize.y - barHeight - barMargin);

        UxImGui::SetNextWindowPos(barPos, UxImGuiCond_Always);
        UxImGui::SetNextWindowSize(UxImVec2(barWidth, barHeight), UxImGuiCond_Always);

        // Styling - same shape as monitoring bar
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(6.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(12), S(6)));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, S(0.0f));

        // Red-ish semi-transparent background
        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0.25f, 0.06f, 0.06f, 0.85f));

        UxImGuiWindowFlags flags = UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoScrollbar |
            UxImGuiWindowFlags_NoCollapse |
            UxImGuiWindowFlags_NoSavedSettings;

        if (UxImGui::Begin("##MfgRestartWarning", nullptr, flags))
        {
            // Scale font for warning bar
            UxImGui::SetWindowFontScale(g_UiScale);
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // Top accent line (red/amber) - mirrors monitoring bar's bottom accent
            UxImU32 accentColor = IM_COL32(220, 80, 60, 255);
            drawList->AddRectFilled(
                windowPos,
                UxImVec2(windowPos.x + windowSize.x, windowPos.y + 2.0f),
                accentColor
            );

            // Determine message: if not yet saved, prompt save+restart; if saved, just restart
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.7f, 0.4f, 1.0f));
            UxImGui::Text("!");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.9f, 0.85f, 1.0f));
            if (needsSave)
                UxImGui::Text("MFG Override changed. Save & restart to apply.");
            else
                UxImGui::Text("MFG Override changed. Restart game to apply.");
            UxImGui::PopStyleColor();

            // Dismiss button on the right
            UxImGui::SameLine(barWidth - S(78.0f));

            UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.45f, 0.12f, 0.12f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.55f, 0.18f, 0.18f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.35f, 0.08f, 0.08f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.85f, 0.8f, 1.0f));
            UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(3.0f));

            if (UxImGui::Button("Dismiss", UxImVec2(S(65.0f), S(18.0f))))
            {
                SidePanel::g_MfgRestartWarningDismissed = true;
            }

            UxImGui::PopStyleVar();
            UxImGui::PopStyleColor(4);
        }
        UxImGui::End();

        UxImGui::PopStyleColor();  // WindowBg
        UxImGui::PopStyleVar(3);
    }

    // =============================================================================
    // Frame Generation Backend - Restart Warning Bar (bottom of screen)
    // Similar to MFG restart warning, positioned slightly above it if both visible
    // =============================================================================
    static void RenderFgBackendRestartWarningBar()
    {
        // Initialize tracking on first call (after Load has set ctx values)
        static bool fgBackendInitialized = false;
        if (!fgBackendInitialized)
        {
            SidePanel::g_DlssgDisabledAtLoad = ctx.ngx.isDlssgDisabled;
            SidePanel::g_DlssgDisabledLastSaved = ctx.ngx.isDlssgDisabled;
            fgBackendInitialized = true;
        }

        // Detect when user clicks Persist
        {
            static bool hadUnsavedLastFrame = false;
            bool hasUnsaved = SettingsPersistence::HasUnsavedChanges();
            if (hadUnsavedLastFrame && !hasUnsaved)
            {
                SidePanel::g_DlssgDisabledLastSaved = ctx.ngx.isDlssgDisabled;
            }
            hadUnsavedLastFrame = hasUnsaved;
        }

        bool needsRestart = (ctx.ngx.isDlssgDisabled != SidePanel::g_DlssgDisabledAtLoad);
        bool needsSave = (ctx.ngx.isDlssgDisabled != SidePanel::g_DlssgDisabledLastSaved);
        bool fgBackendChanged = needsRestart || needsSave;

        if (!fgBackendChanged || SidePanel::g_FgBackendRestartWarningDismissed)
            return;

        UxImGuiIO& io = UxImGui::GetIO();

        const float barMargin = S(10.0f);
        const float barWidth = S(520.0f);
        const float barHeight = S(30.0f);

        // Position: bottom center, above MFG warning bar if it's also visible
        float yOffset = barHeight + barMargin;
        // MFG override is runtime-applied (no restart needed), so the MFG warning bar
        // is never rendered. Keep extraOffset=0 for the DLSSG-disabled bar positioning.
        bool mfgBarVisible = false;
        float extraOffset = mfgBarVisible ? (barHeight + S(4.0f)) : 0.0f;

        UxImVec2 barPos((io.DisplaySize.x - barWidth) * 0.5f, io.DisplaySize.y - barHeight - barMargin - extraOffset);

        UxImGui::SetNextWindowPos(barPos, UxImGuiCond_Always);
        UxImGui::SetNextWindowSize(UxImVec2(barWidth, barHeight), UxImGuiCond_Always);

        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(6.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(12), S(6)));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, S(0.0f));

        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0.25f, 0.06f, 0.06f, 0.85f));

        UxImGuiWindowFlags flags = UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoScrollbar |
            UxImGuiWindowFlags_NoCollapse |
            UxImGuiWindowFlags_NoSavedSettings;

        if (UxImGui::Begin("##FgBackendRestartWarning", nullptr, flags))
        {
            UxImGui::SetWindowFontScale(g_UiScale);
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // Top accent line
            UxImU32 accentColor = IM_COL32(220, 80, 60, 255);
            drawList->AddRectFilled(
                windowPos,
                UxImVec2(windowPos.x + windowSize.x, windowPos.y + 2.0f),
                accentColor
            );

            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.7f, 0.4f, 1.0f));
            UxImGui::Text("!");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            const char* backendName = ctx.ngx.isDlssgDisabled ? "FSR3" : "DLSS-G";
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.9f, 0.85f, 1.0f));
            if (needsSave)
                UxImGui::Text("FG backend changed to %s. Save & restart to apply.", backendName);
            else
                UxImGui::Text("FG backend changed to %s. Restart game to apply.", backendName);
            UxImGui::PopStyleColor();

            // Dismiss button
            UxImGui::SameLine(barWidth - S(78.0f));

            UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.45f, 0.12f, 0.12f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.55f, 0.18f, 0.18f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.35f, 0.08f, 0.08f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.85f, 0.8f, 1.0f));
            UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(3.0f));

            if (UxImGui::Button("Dismiss", UxImVec2(S(65.0f), S(18.0f))))
            {
                SidePanel::g_FgBackendRestartWarningDismissed = true;
            }

            UxImGui::PopStyleVar();
            UxImGui::PopStyleColor(4);
        }
        UxImGui::End();

        UxImGui::PopStyleColor();  // WindowBg
        UxImGui::PopStyleVar(3);
    }

    // =============================================================================
    // Hybrid MFG (force) - Restart Warning Bar (bottom of screen)
    // Mirrors RenderFgBackendRestartWarningBar; positioned above it if both visible.
    // =============================================================================
    static void RenderHybridMfgRestartWarningBar()
    {
        // Initialize tracking on first call (after Load has set ctx values)
        static bool hybridMfgInitialized = false;
        if (!hybridMfgInitialized)
        {
            SidePanel::g_HybridMfgForcedAtLoad = ctx.ngx.isHybridMfgForced;
            SidePanel::g_HybridMfgForcedLastSaved = ctx.ngx.isHybridMfgForced;
            hybridMfgInitialized = true;
        }

        // Detect when user clicks Persist
        {
            static bool hadUnsavedLastFrame = false;
            bool hasUnsaved = SettingsPersistence::HasUnsavedChanges();
            if (hadUnsavedLastFrame && !hasUnsaved)
            {
                SidePanel::g_HybridMfgForcedLastSaved = ctx.ngx.isHybridMfgForced;
            }
            hadUnsavedLastFrame = hasUnsaved;
        }

        bool needsRestart = (ctx.ngx.isHybridMfgForced != SidePanel::g_HybridMfgForcedAtLoad);
        bool needsSave = (ctx.ngx.isHybridMfgForced != SidePanel::g_HybridMfgForcedLastSaved);
        bool hybridMfgChanged = needsRestart || needsSave;

        if (!hybridMfgChanged || SidePanel::g_HybridMfgRestartWarningDismissed)
            return;

        UxImGuiIO& io = UxImGui::GetIO();

        const float barMargin = S(10.0f);
        const float barWidth = S(520.0f);
        const float barHeight = S(30.0f);

        // Position: bottom center, stacked above FG-backend warning bar if it's also visible
        bool fgBackendBarVisible =
            (ctx.ngx.isDlssgDisabled != SidePanel::g_DlssgDisabledAtLoad ||
                ctx.ngx.isDlssgDisabled != SidePanel::g_DlssgDisabledLastSaved) &&
            !SidePanel::g_FgBackendRestartWarningDismissed;
        float extraOffset = fgBackendBarVisible ? (barHeight + S(4.0f)) : 0.0f;

        UxImVec2 barPos((io.DisplaySize.x - barWidth) * 0.5f, io.DisplaySize.y - barHeight - barMargin - extraOffset);

        UxImGui::SetNextWindowPos(barPos, UxImGuiCond_Always);
        UxImGui::SetNextWindowSize(UxImVec2(barWidth, barHeight), UxImGuiCond_Always);

        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(6.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(12), S(6)));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, S(0.0f));

        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0.25f, 0.06f, 0.06f, 0.85f));

        UxImGuiWindowFlags flags = UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoScrollbar |
            UxImGuiWindowFlags_NoCollapse |
            UxImGuiWindowFlags_NoSavedSettings;

        if (UxImGui::Begin("##HybridMfgRestartWarning", nullptr, flags))
        {
            UxImGui::SetWindowFontScale(g_UiScale);
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // Top accent line
            UxImU32 accentColor = IM_COL32(220, 80, 60, 255);
            drawList->AddRectFilled(
                windowPos,
                UxImVec2(windowPos.x + windowSize.x, windowPos.y + 2.0f),
                accentColor
            );

            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.7f, 0.4f, 1.0f));
            UxImGui::Text("!");
            UxImGui::PopStyleColor();

            UxImGui::SameLine();

            const char* modeName = ctx.ngx.isHybridMfgForced ? "ON" : "AUTO";
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.9f, 0.85f, 1.0f));
            if (needsSave)
                UxImGui::Text("Hybrid MFG changed to %s. Save & restart to apply.", modeName);
            else
                UxImGui::Text("Hybrid MFG changed to %s. Restart game to apply.", modeName);
            UxImGui::PopStyleColor();

            // Dismiss button
            UxImGui::SameLine(barWidth - S(78.0f));

            UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.45f, 0.12f, 0.12f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.55f, 0.18f, 0.18f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.35f, 0.08f, 0.08f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.85f, 0.8f, 1.0f));
            UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, S(3.0f));

            if (UxImGui::Button("Dismiss", UxImVec2(S(65.0f), S(18.0f))))
            {
                SidePanel::g_HybridMfgRestartWarningDismissed = true;
            }

            UxImGui::PopStyleVar();
            UxImGui::PopStyleColor(4);
        }
        UxImGui::End();

        UxImGui::PopStyleColor();  // WindowBg
        UxImGui::PopStyleVar(3);
    }

    bool IsSidePanelEnabled()
    {
        return ctx.isSideBarEnabled;
    }

    // =============================================================================
    // UI Scale
    // =============================================================================

    float GetUiScale()
    {
        return g_UiScale;
    }

    void SetUiScaleOverride(float scale)
    {
        g_UiScaleOverride = scale;  // 0 = auto, >0 = forced
    }

    float GetUiScaleOverride()
    {
        return g_UiScaleOverride;
    }

    // =============================================================================
    // Menu Toggle Key
    // =============================================================================

    int GetMenuToggleKey()
    {
        return g_MenuToggleKey;
    }

    void SetMenuToggleKey(int vk)
    {
        g_MenuToggleKey = vk;
    }

    // VK code <-> friendly name mapping
    struct VkNameEntry { int vk; const char* name; };
    static const VkNameEntry g_VkNameMap[] = {
        // Special keys
        { VK_OEM_3,     "Tilde" },
        { VK_INSERT,    "VK_INSERT" },
        { VK_DELETE,    "VK_DELETE" },
        { VK_HOME,      "VK_HOME" },
        { VK_END,       "VK_END" },
        { VK_PRIOR,     "VK_PRIOR" },      // Page Up
        { VK_NEXT,      "VK_NEXT" },        // Page Down
        { VK_PAUSE,     "VK_PAUSE" },
        { VK_SCROLL,    "VK_SCROLL" },
        { VK_NUMLOCK,   "VK_NUMLOCK" },
        { VK_CAPITAL,   "VK_CAPITAL" },
        // F-keys
        { VK_F1,        "F1" },
        { VK_F2,        "F2" },
        { VK_F3,        "F3" },
        { VK_F4,        "F4" },
        { VK_F5,        "F5" },
        { VK_F6,        "F6" },
        { VK_F7,        "F7" },
        { VK_F8,        "F8" },
        { VK_F9,        "F9" },
        { VK_F10,       "F10" },
        { VK_F11,       "F11" },
        { VK_F12,       "F12" },
        // Numpad
        { VK_NUMPAD0,   "Numpad0" },
        { VK_NUMPAD1,   "Numpad1" },
        { VK_NUMPAD2,   "Numpad2" },
        { VK_NUMPAD3,   "Numpad3" },
        { VK_NUMPAD4,   "Numpad4" },
        { VK_NUMPAD5,   "Numpad5" },
        { VK_NUMPAD6,   "Numpad6" },
        { VK_NUMPAD7,   "Numpad7" },
        { VK_NUMPAD8,   "Numpad8" },
        { VK_NUMPAD9,   "Numpad9" },
        { VK_MULTIPLY,  "NumpadMul" },
        { VK_ADD,       "NumpadAdd" },
        { VK_SUBTRACT,  "NumpadSub" },
        { VK_DECIMAL,   "NumpadDec" },
        { VK_DIVIDE,    "NumpadDiv" },
        // OEM keys
        { VK_OEM_1,     "VK_OEM_1" },      // ;:
        { VK_OEM_PLUS,  "VK_OEM_PLUS" },   // =+
        { VK_OEM_COMMA, "VK_OEM_COMMA" },  // ,<
        { VK_OEM_MINUS, "VK_OEM_MINUS" },  // -_
        { VK_OEM_PERIOD,"VK_OEM_PERIOD" },  // .>
        { VK_OEM_2,     "VK_OEM_2" },      // /?
        { VK_OEM_4,     "VK_OEM_4" },      // [{
        { VK_OEM_5,     "VK_OEM_5" },      // \|
        { VK_OEM_6,     "VK_OEM_6" },      // ]}
        { VK_OEM_7,     "VK_OEM_7" },      // '"
        // Tab, Space, etc.
        { VK_TAB,       "VK_TAB" },
        { VK_SPACE,     "VK_SPACE" },
        { VK_BACK,      "VK_BACK" },
        { VK_ESCAPE,    "VK_ESCAPE" },
        { VK_RETURN,    "VK_RETURN" },
    };
    static constexpr int g_VkNameMapSize = sizeof(g_VkNameMap) / sizeof(g_VkNameMap[0]);

    const char* VkToFriendlyName(int vk)
    {
        // Check named keys first
        for (int i = 0; i < g_VkNameMapSize; i++)
        {
            if (g_VkNameMap[i].vk == vk)
                return g_VkNameMap[i].name;
        }
        // A-Z
        if (vk >= 'A' && vk <= 'Z')
        {
            static char buf[2] = {};
            buf[0] = (char)vk;
            return buf;
        }
        // 0-9
        if (vk >= '0' && vk <= '9')
        {
            static char buf[2] = {};
            buf[0] = (char)vk;
            return buf;
        }
        // Fallback: hex
        static char hexBuf[16] = {};
        snprintf(hexBuf, sizeof(hexBuf), "0x%02X", vk);
        return hexBuf;
    }

    int FriendlyNameToVk(const char* name)
    {
        if (!name || !name[0]) return -1;

        // Check named keys (case-insensitive)
        for (int i = 0; i < g_VkNameMapSize; i++)
        {
            if (_stricmp(name, g_VkNameMap[i].name) == 0)
                return g_VkNameMap[i].vk;
        }
        // Single character A-Z or 0-9
        if (name[1] == '\0')
        {
            char c = name[0];
            if (c >= 'a' && c <= 'z') return (int)(c - 'a' + 'A');
            if (c >= 'A' && c <= 'Z') return (int)c;
            if (c >= '0' && c <= '9') return (int)c;
        }
        // Hex format: 0x...
        if (name[0] == '0' && (name[1] == 'x' || name[1] == 'X'))
        {
            return (int)strtol(name, nullptr, 16);
        }
        // Decimal number
        char* end = nullptr;
        long val = strtol(name, &end, 10);
        if (end && end != name && *end == '\0' && val > 0 && val < 256)
            return (int)val;

        return -1;  // Unknown
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

        // Update UI scale factor each frame
        g_UiScale = ComputeUiScale(io.DisplaySize);

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
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, S(8.0f));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(0), S(0)));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, S(0.0f));

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
            float sidebarWidth = S(180.0f);

            // =====================================================================
            // Sidebar
            // =====================================================================
            UxImGui::PushStyleColor(UxImGuiCol_ChildBg, Colors::SidebarBg);
            UxImGui::BeginChild("##Sidebar", UxImVec2(sidebarWidth, 0), false);
            {
                // Title
                UxImGui::Dummy(UxImVec2(0, 15));
                UxImGui::SetCursorPosX(S(20.0f));
                UxImGui::TextColored(Colors::AccentGreen, "SETTINGS");
                UxImGui::Dummy(UxImVec2(0, 20));

                // Menu items
                for (int i = 0; i < (int)Category::COUNT; i++)
                {
                    bool isSelected = ((int)g_CurrentCategory == i);

                    UxImGui::SetCursorPosX(S(10.0f));

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
            UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(S(20), S(20)));
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
        // Update UI scale (this runs independently of Render)
        UxImGuiIO& io = UxImGui::GetIO();
        g_UiScale = ComputeUiScale(io.DisplaySize);

        // This renders the monitoring bar based solely on ctx.isMonitoringEnabled
        // It's separate from Render() which requires F1 to be pressed
        RenderMonitoringBar();

        // Scene intensity bar - thin bar below monitoring bar
        RenderSceneIntensityBar();

        // MFG restart warning bar - always visible regardless of menu state
        RenderMfgRestartWarningBar();

        // FG backend restart warning bar - always visible regardless of menu state
        RenderFgBackendRestartWarningBar();

        // Hybrid MFG restart warning bar - always visible regardless of menu state
        RenderHybridMfgRestartWarningBar();
    }
}