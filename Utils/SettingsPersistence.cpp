// =============================================================================
// SettingsPersistence.cpp - INI-based Settings Persistence for DLSS Enabler
// =============================================================================
//
// INTEGRATION INSTRUCTIONS:
// -------------------------
// 1. In SettingsMenu.cpp, add at the top:
//    #include "SettingsPersistence.h"
//
// 2. In SettingsMenu::Init(), add:
//    SettingsPersistence::Init();
//    SettingsPersistence::Load();
//
// 3. In SettingsMenu::Render(), AFTER RenderSidePanel() call, add:
//    SettingsPersistence::RenderPersistPrompt();
//
//    Example:
//    void Render(bool* p_open)
//    {
//        ...
//        RenderSidePanel();
//        SettingsPersistence::RenderPersistPrompt();  // <-- ADD THIS LINE
//        ...
//    }
//
// =============================================================================

#include "SettingsPersistence.h"
#include "../Core/Context.h"
#include "Common.h"

// UxImGui for rendering the prompt
#include "UxImGui/imgui.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace SettingsPersistence
{
    // =============================================================================
    // Constants
    // =============================================================================

    static const wchar_t* INI_FILENAME = L"dlss-enabler.ini";

    // =============================================================================
    // Internal State
    // =============================================================================

    static bool g_Initialized = false;
    static bool g_HasUnsavedChanges = false;
    static std::wstring g_IniFilePath;

    // Snapshot of settings for change detection
    struct SettingsSnapshot
    {
        // NGX settings
        bool isScreenSpaceRayTracingEnabled = false;
        bool isGlobalIlluminationEnabled = true;
        bool isAmbientOcclusionEnabled = true;
        bool isDynamicFrameGenerationEnabled = false;
        bool isDynamicFrameGenerationStartingOnThreshold = true;
        int rayTracingQuality = 1;
        int rayTracingRange = 50;
        int illuminationStrength = 50;
        int occlusionStrength = 50;
        int dynamicFrameGenerationThreshold = 60;

        // Reflex settings
        bool isVsyncOverrideEnabled = false;
        bool isVsyncEnabled = false;
        bool isFpsLimitEnabled = false;
        bool isBoostOverriden = false;
        bool isBoostEnabled = false;
        int desiredFpsLimit = 60;

        // Global settings
        bool isMonitoringEnabled = false;
        bool isSideBarEnabled = true;

        // DeepDVC settings
        float deepDvcIntensity = 0.5f;
        float deepDvcSaturationBoost = 0.75f;

        // Performance settings
        int overdriveMode = 0;
        bool quickBoot = false;

        // Debug settings
        uint32_t debugFlags = 0;
    };

    static SettingsSnapshot g_LastSavedSnapshot;

    // =============================================================================
    // Helper Functions
    // =============================================================================

    static std::string WideToUtf8(const std::wstring& wstr)
    {
        if (wstr.empty()) return std::string();

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
        return str;
    }

    static std::wstring Utf8ToWide(const std::string& str)
    {
        if (str.empty()) return std::wstring();

        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
        return wstr;
    }

    static void TakeSnapshot(SettingsSnapshot& snapshot)
    {
        // NGX
        snapshot.isScreenSpaceRayTracingEnabled = ctx.ngx.isScreenSpaceRayTracingEnabled;
        snapshot.isGlobalIlluminationEnabled = ctx.ngx.isGlobalIlluminationEnabled;
        snapshot.isAmbientOcclusionEnabled = ctx.ngx.isAmbientOcclusionEnabled;
        snapshot.isDynamicFrameGenerationEnabled = ctx.ngx.isDynamicFrameGenerationEnabled;
        snapshot.isDynamicFrameGenerationStartingOnThreshold = ctx.ngx.isDynamicFrameGenerationStartingOnThreshold;
        snapshot.rayTracingQuality = ctx.ngx.rayTracingQuality;
        snapshot.rayTracingRange = ctx.ngx.rayTracingRange;
        snapshot.illuminationStrength = ctx.ngx.illuminationStrength;
        snapshot.occlusionStrength = ctx.ngx.occlusionStrength;
        snapshot.dynamicFrameGenerationThreshold = ctx.ngx.dynamicFrameGenerationThreshold;

        // Reflex
        snapshot.isVsyncOverrideEnabled = ctx.reflex.isVsyncOverrideEnabled;
        snapshot.isVsyncEnabled = ctx.reflex.isVsyncEnabled;
        snapshot.isFpsLimitEnabled = ctx.reflex.isFpsLimitEnabled;
        snapshot.isBoostOverriden = ctx.reflex.isBoostOverriden;
        snapshot.isBoostEnabled = ctx.reflex.isBoostEnabled;
        snapshot.desiredFpsLimit = ctx.reflex.desiredFpsLimit;

        // Global
        snapshot.isMonitoringEnabled = ctx.isMonitoringEnabled;
        snapshot.isSideBarEnabled = ctx.isSideBarEnabled;

        // DeepDVC
        snapshot.deepDvcIntensity = ctx.deepDVC.intensity;
        snapshot.deepDvcSaturationBoost = ctx.deepDVC.saturationBoost;

        // Performance
        snapshot.overdriveMode = ctx.overdriveMode;
        snapshot.quickBoot = ctx.quickBoot;

        // Debug
        snapshot.debugFlags = ctx.flags;
    }

    static bool CompareSnapshots(const SettingsSnapshot& a, const SettingsSnapshot& b)
    {
        // Returns true if snapshots are EQUAL (no changes)
        return
            a.isScreenSpaceRayTracingEnabled == b.isScreenSpaceRayTracingEnabled &&
            a.isGlobalIlluminationEnabled == b.isGlobalIlluminationEnabled &&
            a.isAmbientOcclusionEnabled == b.isAmbientOcclusionEnabled &&
            a.isDynamicFrameGenerationEnabled == b.isDynamicFrameGenerationEnabled &&
            a.isDynamicFrameGenerationStartingOnThreshold == b.isDynamicFrameGenerationStartingOnThreshold &&
            a.rayTracingQuality == b.rayTracingQuality &&
            a.rayTracingRange == b.rayTracingRange &&
            a.illuminationStrength == b.illuminationStrength &&
            a.occlusionStrength == b.occlusionStrength &&
            a.dynamicFrameGenerationThreshold == b.dynamicFrameGenerationThreshold &&
            a.isVsyncOverrideEnabled == b.isVsyncOverrideEnabled &&
            a.isVsyncEnabled == b.isVsyncEnabled &&
            a.isFpsLimitEnabled == b.isFpsLimitEnabled &&
            a.isBoostOverriden == b.isBoostOverriden &&
            a.isBoostEnabled == b.isBoostEnabled &&
            a.desiredFpsLimit == b.desiredFpsLimit &&
            a.isMonitoringEnabled == b.isMonitoringEnabled &&
            a.isSideBarEnabled == b.isSideBarEnabled &&
            a.deepDvcIntensity == b.deepDvcIntensity &&
            a.deepDvcSaturationBoost == b.deepDvcSaturationBoost &&
            a.overdriveMode == b.overdriveMode &&
            a.quickBoot == b.quickBoot &&
            a.debugFlags == b.debugFlags;
    }

    // Simple INI parser helpers
    static std::string Trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\r\n");
        size_t end = str.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return str.substr(start, end - start + 1);
    }

    static bool ParseBool(const std::string& value)
    {
        std::string v = Trim(value);
        return (v == "1" || v == "true" || v == "True" || v == "TRUE" || v == "yes" || v == "Yes" || v == "YES");
    }

    static int ParseInt(const std::string& value, int defaultValue = 0)
    {
        try {
            return std::stoi(Trim(value));
        }
        catch (...) {
            return defaultValue;
        }
    }

    static float ParseFloat(const std::string& value, float defaultValue = 0.0f)
    {
        try {
            return std::stof(Trim(value));
        }
        catch (...) {
            return defaultValue;
        }
    }

    // =============================================================================
    // Public API
    // =============================================================================

    void Init()
    {
        if (g_Initialized) return;

        // Build INI file path
        g_IniFilePath = Common::GetModuleDirectory() + L"\\" + INI_FILENAME;

        // Take initial snapshot
        TakeSnapshot(g_LastSavedSnapshot);

        g_Initialized = true;
        g_HasUnsavedChanges = false;
    }

    std::wstring GetIniFilePath()
    {
        return g_IniFilePath;
    }

    bool Load()
    {
        if (!g_Initialized) Init();

        std::string path = WideToUtf8(g_IniFilePath);
        std::ifstream file(path);

        if (!file.is_open())
        {
            // File doesn't exist - not an error, just no saved settings
            return false;
        }

        std::unordered_map<std::string, std::string> settings;
        std::string currentSection;
        std::string line;

        while (std::getline(file, line))
        {
            line = Trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;

            // Section header
            if (line[0] == '[' && line.back() == ']')
            {
                currentSection = line.substr(1, line.size() - 2);
                continue;
            }

            // Key=Value
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos)
            {
                std::string key = Trim(line.substr(0, eqPos));
                std::string value = Trim(line.substr(eqPos + 1));

                // Store with section prefix
                std::string fullKey = currentSection.empty() ? key : (currentSection + "." + key);
                settings[fullKey] = value;
            }
        }

        file.close();

        // Apply loaded settings to ctx

        // NGX Section
        if (settings.count("NGX.ScreenSpaceRayTracing"))
            ctx.ngx.isScreenSpaceRayTracingEnabled = ParseBool(settings["NGX.ScreenSpaceRayTracing"]);
        if (settings.count("NGX.GlobalIllumination"))
            ctx.ngx.isGlobalIlluminationEnabled = ParseBool(settings["NGX.GlobalIllumination"]);
        if (settings.count("NGX.AmbientOcclusion"))
            ctx.ngx.isAmbientOcclusionEnabled = ParseBool(settings["NGX.AmbientOcclusion"]);
        if (settings.count("NGX.DynamicFrameGeneration"))
            ctx.ngx.isDynamicFrameGenerationEnabled = ParseBool(settings["NGX.DynamicFrameGeneration"]);
        if (settings.count("NGX.DynamicFGStartsOnThreshold"))
            ctx.ngx.isDynamicFrameGenerationStartingOnThreshold = ParseBool(settings["NGX.DynamicFGStartsOnThreshold"]);
        if (settings.count("NGX.RayTracingQuality"))
            ctx.ngx.rayTracingQuality = ParseInt(settings["NGX.RayTracingQuality"], 1);
        if (settings.count("NGX.RayTracingRange"))
            ctx.ngx.rayTracingRange = ParseInt(settings["NGX.RayTracingRange"], 50);
        if (settings.count("NGX.IlluminationStrength"))
            ctx.ngx.illuminationStrength = ParseInt(settings["NGX.IlluminationStrength"], 50);
        if (settings.count("NGX.OcclusionStrength"))
            ctx.ngx.occlusionStrength = ParseInt(settings["NGX.OcclusionStrength"], 50);
        if (settings.count("NGX.DynamicFGThreshold"))
            ctx.ngx.dynamicFrameGenerationThreshold = ParseInt(settings["NGX.DynamicFGThreshold"], 60);

        // Reflex Section
        if (settings.count("Reflex.VsyncOverride"))
            ctx.reflex.isVsyncOverrideEnabled = ParseBool(settings["Reflex.VsyncOverride"]);
        if (settings.count("Reflex.Vsync"))
            ctx.reflex.isVsyncEnabled = ParseBool(settings["Reflex.Vsync"]);
        if (settings.count("Reflex.FpsLimit"))
            ctx.reflex.isFpsLimitEnabled = ParseBool(settings["Reflex.FpsLimit"]);
        if (settings.count("Reflex.BoostOverride"))
            ctx.reflex.isBoostOverriden = ParseBool(settings["Reflex.BoostOverride"]);
        if (settings.count("Reflex.Boost"))
            ctx.reflex.isBoostEnabled = ParseBool(settings["Reflex.Boost"]);
        if (settings.count("Reflex.DesiredFpsLimit"))
            ctx.reflex.desiredFpsLimit = ParseInt(settings["Reflex.DesiredFpsLimit"], 60);

        // UI Section
        if (settings.count("UI.Monitoring"))
            ctx.isMonitoringEnabled = ParseBool(settings["UI.Monitoring"]);
        if (settings.count("UI.SideBar"))
            ctx.isSideBarEnabled = ParseBool(settings["UI.SideBar"]);

        // DeepDVC Section
        if (settings.count("DeepDVC.Intensity"))
            ctx.deepDVC.intensity = ParseFloat(settings["DeepDVC.Intensity"], 0.5f);
        if (settings.count("DeepDVC.SaturationBoost"))
            ctx.deepDVC.saturationBoost = ParseFloat(settings["DeepDVC.SaturationBoost"], 0.75f);

        // Performance Section
        if (settings.count("Performance.OverdriveMode"))
            ctx.overdriveMode = ParseInt(settings["Performance.OverdriveMode"], 0);
        if (settings.count("Performance.QuickBoot"))
            ctx.quickBoot = ParseBool(settings["Performance.QuickBoot"]);

        // Debug Section
        if (settings.count("Debug.Flags"))
            ctx.flags = static_cast<uint32_t>(ParseInt(settings["Debug.Flags"], 0));

        // Take snapshot of loaded settings
        TakeSnapshot(g_LastSavedSnapshot);
        g_HasUnsavedChanges = false;

        return true;
    }

    bool Save()
    {
        if (!g_Initialized) Init();

        std::string path = WideToUtf8(g_IniFilePath);
        std::ofstream file(path);

        if (!file.is_open())
        {
            return false;
        }

        // Write header
        file << "; =============================================================================\n";
        file << "; DLSS Enabler Settings\n";
        file << "; This file is auto-generated. Manual edits may be overwritten.\n";
        file << "; =============================================================================\n\n";

        // NGX Section
        file << "[NGX]\n";
        file << "ScreenSpaceRayTracing=" << (ctx.ngx.isScreenSpaceRayTracingEnabled ? "true" : "false") << "\n";
        file << "GlobalIllumination=" << (ctx.ngx.isGlobalIlluminationEnabled ? "true" : "false") << "\n";
        file << "AmbientOcclusion=" << (ctx.ngx.isAmbientOcclusionEnabled ? "true" : "false") << "\n";
        file << "DynamicFrameGeneration=" << (ctx.ngx.isDynamicFrameGenerationEnabled ? "true" : "false") << "\n";
        file << "DynamicFGStartsOnThreshold=" << (ctx.ngx.isDynamicFrameGenerationStartingOnThreshold ? "true" : "false") << "\n";
        file << "RayTracingQuality=" << ctx.ngx.rayTracingQuality << "\n";
        file << "RayTracingRange=" << ctx.ngx.rayTracingRange << "\n";
        file << "IlluminationStrength=" << ctx.ngx.illuminationStrength << "\n";
        file << "OcclusionStrength=" << ctx.ngx.occlusionStrength << "\n";
        file << "DynamicFGThreshold=" << ctx.ngx.dynamicFrameGenerationThreshold << "\n";
        file << "\n";

        // Reflex Section
        file << "[Reflex]\n";
        file << "VsyncOverride=" << (ctx.reflex.isVsyncOverrideEnabled ? "true" : "false") << "\n";
        file << "Vsync=" << (ctx.reflex.isVsyncEnabled ? "true" : "false") << "\n";
        file << "FpsLimit=" << (ctx.reflex.isFpsLimitEnabled ? "true" : "false") << "\n";
        file << "BoostOverride=" << (ctx.reflex.isBoostOverriden ? "true" : "false") << "\n";
        file << "Boost=" << (ctx.reflex.isBoostEnabled ? "true" : "false") << "\n";
        file << "DesiredFpsLimit=" << ctx.reflex.desiredFpsLimit << "\n";
        file << "\n";

        // UI Section
        file << "[UI]\n";
        file << "Monitoring=" << (ctx.isMonitoringEnabled ? "true" : "false") << "\n";
        file << "SideBar=" << (ctx.isSideBarEnabled ? "true" : "false") << "\n";
        file << "\n";

        // DeepDVC Section
        file << "[DeepDVC]\n";
        file << "Intensity=" << ctx.deepDVC.intensity << "\n";
        file << "SaturationBoost=" << ctx.deepDVC.saturationBoost << "\n";
        file << "\n";

        // Performance Section (only if overdrive is enabled/visible)
        if (ctx.overdriveMode >= 0)
        {
            file << "[Performance]\n";
            file << "OverdriveMode=" << ctx.overdriveMode << "\n";
            file << "QuickBoot=" << (ctx.quickBoot ? "true" : "false") << "\n";
            file << "\n";
        }

        // Debug Section
        file << "[Debug]\n";
        file << "Flags=" << ctx.flags << "\n";

        file.close();

        // Update snapshot
        TakeSnapshot(g_LastSavedSnapshot);
        g_HasUnsavedChanges = false;

        return true;
    }

    void CheckForChanges()
    {
        if (!g_Initialized) return;

        SettingsSnapshot current;
        TakeSnapshot(current);

        bool hasChanges = !CompareSnapshots(current, g_LastSavedSnapshot);

        // Only set to true, never reset to false here
        // (user must explicitly save or ignore)
        if (hasChanges && !g_HasUnsavedChanges)
        {
            g_HasUnsavedChanges = true;
        }
    }

    bool HasUnsavedChanges()
    {
        return g_HasUnsavedChanges;
    }

    void ClearUnsavedChanges()
    {
        // Update snapshot to current state (effectively "ignoring" changes)
        TakeSnapshot(g_LastSavedSnapshot);
        g_HasUnsavedChanges = false;
    }

    void SnapshotCurrentSettings()
    {
        TakeSnapshot(g_LastSavedSnapshot);
    }

    void RenderPersistPrompt()
    {
        // Always check for changes first
        CheckForChanges();

        if (!g_HasUnsavedChanges)
            return;

        // Get IO for display size
        UxImGuiIO& io = UxImGui::GetIO();

        // Dimensions matching side panel - must calculate same way as SidePanel
        const float panelWidth = 280.0f;
        const float panelMargin = 10.0f;
        const float promptHeight = 75.0f;
        const float persistPromptReserved = 85.0f;
        const float maxPanelHeight = io.DisplaySize.y - panelMargin * 2 - persistPromptReserved;
        const float desiredPanelHeight = 750.0f;
        const float panelHeight = (desiredPanelHeight < maxPanelHeight) ? desiredPanelHeight : maxPanelHeight;
        const float promptWidth = panelWidth;

        // Calculate Y position - below sidebar
        float promptY = panelMargin + panelHeight + 10.0f;

        // If screen is too small, position at bottom of screen
        if (promptY + promptHeight > io.DisplaySize.y)
        {
            promptY = io.DisplaySize.y - promptHeight - panelMargin;
        }

        // Ensure minimum Y position
        if (promptY < panelMargin)
        {
            promptY = panelMargin;
        }

        UxImVec2 promptPos(panelMargin, promptY);

        UxImGui::SetNextWindowPos(promptPos, UxImGuiCond_Always);
        UxImGui::SetNextWindowSize(UxImVec2(promptWidth, promptHeight), UxImGuiCond_Always);

        // Styling
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, 8.0f);
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(15, 12));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, 1.0f);
        UxImGui::PushStyleVar(UxImGuiStyleVar_ItemSpacing, UxImVec2(8, 8));

        // Background color - slightly orange tinted to indicate pending changes
        UxImGui::PushStyleColor(UxImGuiCol_WindowBg, UxImVec4(0.08f, 0.06f, 0.04f, 0.95f));
        UxImGui::PushStyleColor(UxImGuiCol_Border, UxImVec4(0.6f, 0.4f, 0.2f, 0.8f));

        UxImGuiWindowFlags flags = UxImGuiWindowFlags_NoTitleBar |
            UxImGuiWindowFlags_NoResize |
            UxImGuiWindowFlags_NoMove |
            UxImGuiWindowFlags_NoScrollbar |
            UxImGuiWindowFlags_NoCollapse |
            UxImGuiWindowFlags_AlwaysAutoResize |
            UxImGuiWindowFlags_NoSavedSettings;

        bool windowOpen = true;
        if (UxImGui::Begin("##SettingsPersistPrompt", &windowOpen, flags))
        {
            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // Left accent bar (orange/amber)
            UxImU32 accentColor = IM_COL32(255, 160, 60, 255);
            drawList->AddRectFilled(
                windowPos,
                UxImVec2(windowPos.x + 3.0f, windowPos.y + windowSize.y),
                accentColor
            );

            // Text - with left padding for accent bar
            UxImGui::SetCursorPosX(8.0f);
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.85f, 0.6f, 1.0f));
            UxImGui::Text("Settings changed. Save?");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 2));

            // Calculate button width based on available content region
            float contentWidth = UxImGui::GetContentRegionAvail().x;
            float buttonWidth = (contentWidth - 8.0f) / 2.0f;  // 8 = gap between buttons

            // Persist button (green/teal)
            UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.0f, 0.5f, 0.45f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.0f, 0.6f, 0.55f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.0f, 0.4f, 0.35f, 1.0f));
            UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

            if (UxImGui::Button("Persist", UxImVec2(buttonWidth, 26.0f)))
            {
                Save();
            }

            UxImGui::PopStyleVar();
            UxImGui::PopStyleColor(3);

            UxImGui::SameLine();

            // Ignore button (gray)
            UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.25f, 0.25f, 0.28f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.35f, 0.35f, 0.40f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.18f, 0.18f, 0.20f, 1.0f));
            UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f);

            if (UxImGui::Button("Ignore", UxImVec2(buttonWidth, 26.0f)))
            {
                ClearUnsavedChanges();
            }

            UxImGui::PopStyleVar();
            UxImGui::PopStyleColor(3);
        }
        UxImGui::End();

        UxImGui::PopStyleColor(2);  // WindowBg, Border
        UxImGui::PopStyleVar(4);
    }
}