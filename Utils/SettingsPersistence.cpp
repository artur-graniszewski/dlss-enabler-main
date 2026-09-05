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
//        ...
//    }
//
// =============================================================================

#include "SettingsPersistence.h"
#include "SettingsMenu.h"
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
        int rayTracingQuality = 1;
        int rayTracingRange = 50;
        int illuminationStrength = 50;
        int occlusionStrength = 50;
        int hudDetectionMode = 0;  // 0 = AUTO (default), 1 = OFF, 2 = ON

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

        // HUD interpolation
        bool isHudInterpolationEnabled = false;

        // Debug settings
        uint32_t debugFlags = 0;

        // GhostBuster settings
        bool isGhostBustingEnabled = false;
        int ghostBusterDebugMode = 0;

        // MFG Enforcement
        int mfgEnforcedMode = 0;

        // MFG Hotkeys (default OFF)
        bool areHotKeysEnabled = false;

        // Dynamic MFG
        bool isDynamicMfgEnabled = false;
        int dynamicMfgThreshold2 = 90;
        int dynamicMfgThreshold3 = 60;
        int dynamicMfgThreshold4 = 30;
        int dynamicMfgThreshold5 = 24;
        int dynamicMfgThreshold6 = 20;
        int dfgMode = 0;          // 0 = AUTO, 1 = CUSTOM
        int dfgTargetFps = 120;   // Target FPS for AUTO mode

        // DFG Instinct (action-intensity latency reduction)
        bool isMinDynamicFpsActive = false;
        int  minDynamicFps = 60;

        // Frame Generation backend
        bool isDlssgDisabled = true;
        bool forceLoadDLSSG = false;

        // Hybrid MFG (force) - false = AUTO, true = ON
        bool isHybridMfgForced = false;

        // Early Optiscaler init
        bool isEarlyInitEnabled = false;

        // Menu toggle key
        int menuToggleKey = VK_OEM_3;

        // UI scale override (0 = auto)
        float uiScaleOverride = 0.0f;
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
        snapshot.rayTracingQuality = ctx.ngx.rayTracingQuality;
        snapshot.rayTracingRange = ctx.ngx.rayTracingRange;
        snapshot.illuminationStrength = ctx.ngx.illuminationStrength;
        snapshot.occlusionStrength = ctx.ngx.occlusionStrength;
        snapshot.hudDetectionMode = ctx.ngx.hudDetectionMode;

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

        // HUD interpolation
        snapshot.isHudInterpolationEnabled = ctx.ngx.isHudInterpolationEnabled;

        // Debug
        snapshot.debugFlags = ctx.flags;

        // GhostBuster
        snapshot.isGhostBustingEnabled = ctx.ngx.isGhostBustingEnabled;
        snapshot.ghostBusterDebugMode = ctx.ghostBusterDebugMode;

        // MFG Enforcement
        snapshot.mfgEnforcedMode = ctx.nvapi.mfgEnforcedMode;

        // MFG Hotkeys
        snapshot.areHotKeysEnabled = ctx.areHotKeysEnabled;

        // Dynamic MFG
        snapshot.isDynamicMfgEnabled = ctx.streamline.isDynamicMfgEnabled;
        snapshot.dynamicMfgThreshold2 = ctx.streamline.dynamicMfgThreshold2;
        snapshot.dynamicMfgThreshold3 = ctx.streamline.dynamicMfgThreshold3;
        snapshot.dynamicMfgThreshold4 = ctx.streamline.dynamicMfgThreshold4;
        snapshot.dynamicMfgThreshold5 = ctx.streamline.dynamicMfgThreshold5;
        snapshot.dynamicMfgThreshold6 = ctx.streamline.dynamicMfgThreshold6;
        snapshot.dfgMode = ctx.streamline.dfgMode;
        snapshot.dfgTargetFps = ctx.streamline.dfgTargetFps;

        // DFG Instinct
        snapshot.isMinDynamicFpsActive = ctx.streamline.isMinDynamicFpsActive;
        snapshot.minDynamicFps = (int)ctx.streamline.minDynamicFps;

        // Frame Generation backend
        snapshot.isDlssgDisabled = ctx.ngx.isDlssgDisabled;
        snapshot.forceLoadDLSSG = ctx.streamline.forceLoadDLSSG;

        // Hybrid MFG (force)
        snapshot.isHybridMfgForced = ctx.ngx.isHybridMfgForced;

        // Early Optiscaler init
        snapshot.isEarlyInitEnabled = ctx.ngx.isEarlyInitEnabled;

        // Menu toggle key
        snapshot.menuToggleKey = SettingsMenu::GetMenuToggleKey();

        // UI scale override
        snapshot.uiScaleOverride = SettingsMenu::GetUiScaleOverride();
    }

    static bool CompareSnapshots(const SettingsSnapshot& a, const SettingsSnapshot& b)
    {
        // Returns true if snapshots are EQUAL (no changes)
        return
            a.isScreenSpaceRayTracingEnabled == b.isScreenSpaceRayTracingEnabled &&
            a.isGlobalIlluminationEnabled == b.isGlobalIlluminationEnabled &&
            a.isAmbientOcclusionEnabled == b.isAmbientOcclusionEnabled &&
            a.rayTracingQuality == b.rayTracingQuality &&
            a.rayTracingRange == b.rayTracingRange &&
            a.illuminationStrength == b.illuminationStrength &&
            a.occlusionStrength == b.occlusionStrength &&
            a.hudDetectionMode == b.hudDetectionMode &&
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
            a.isHudInterpolationEnabled == b.isHudInterpolationEnabled &&
            a.debugFlags == b.debugFlags &&
            a.isGhostBustingEnabled == b.isGhostBustingEnabled &&
            a.ghostBusterDebugMode == b.ghostBusterDebugMode &&
            a.mfgEnforcedMode == b.mfgEnforcedMode &&
            a.areHotKeysEnabled == b.areHotKeysEnabled &&
            a.isDynamicMfgEnabled == b.isDynamicMfgEnabled &&
            a.dynamicMfgThreshold2 == b.dynamicMfgThreshold2 &&
            a.dynamicMfgThreshold3 == b.dynamicMfgThreshold3 &&
            a.dynamicMfgThreshold4 == b.dynamicMfgThreshold4 &&
            a.dynamicMfgThreshold5 == b.dynamicMfgThreshold5 &&
            a.dynamicMfgThreshold6 == b.dynamicMfgThreshold6 &&
            a.dfgMode == b.dfgMode &&
            a.dfgTargetFps == b.dfgTargetFps &&
            a.isMinDynamicFpsActive == b.isMinDynamicFpsActive &&
            a.minDynamicFps == b.minDynamicFps &&
            a.isDlssgDisabled == b.isDlssgDisabled &&
            a.forceLoadDLSSG == b.forceLoadDLSSG &&
            a.isHybridMfgForced == b.isHybridMfgForced &&
            a.isEarlyInitEnabled == b.isEarlyInitEnabled &&
            a.menuToggleKey == b.menuToggleKey &&
            a.uiScaleOverride == b.uiScaleOverride;
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

    // Parse a dotted version ("4.9.0.8") into 4 components. Accepts commas as
    // separators too, since VERSIONINFO strings are sometimes written that way,
    // and tolerates missing trailing components ("4.9" -> 4.9.0.0).
    // Returns false if nothing numeric could be read at all.
    struct ConfigVersion
    {
        int part[4];
    };

    static bool ParseVersion(const std::string& value, ConfigVersion& out)
    {
        out.part[0] = out.part[1] = out.part[2] = out.part[3] = 0;

        int index = 0;
        bool anyDigit = false;
        bool inNumber = false;

        for (char c : value)
        {
            if (c >= '0' && c <= '9')
            {
                out.part[index] = out.part[index] * 10 + (c - '0');
                anyDigit = true;
                inNumber = true;
            }
            else if (c == '.' || c == ',')
            {
                if (index >= 3)
                    break;
                ++index;
                inNumber = false;
            }
            else if (inNumber)
            {
                // Trailing junk after the numeric part - stop here.
                break;
            }
        }

        return anyDigit;
    }

    // Negative if a < b, zero if equal, positive if a > b.
    static int CompareVersions(const ConfigVersion& a, const ConfigVersion& b)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (a.part[i] != b.part[i])
                return (a.part[i] < b.part[i]) ? -1 : 1;
        }
        return 0;
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
            // File doesn't exist - create it with current (default) settings.
            // Save() will stamp the live DLL version into [Meta] configVersion
            // so subsequent loads take the normal path.
            Save();
            return false;
        }

        // Snapshot the GhostBuster default supplied by ctx BEFORE we start
        // parsing the INI. Parsing will overwrite ctx.ngx.isGhostBustingEnabled
        // with whatever value the old INI stored; the migration block at the
        // end of Load() needs the original ctx-supplied default to restore it.
        const bool ctxGhostBusterDefault = ctx.ngx.isGhostBustingEnabled;

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
        if (settings.count("NGX.RayTracingQuality"))
            ctx.ngx.rayTracingQuality = ParseInt(settings["NGX.RayTracingQuality"], 1);
        if (settings.count("NGX.RayTracingRange"))
            ctx.ngx.rayTracingRange = ParseInt(settings["NGX.RayTracingRange"], 50);
        if (settings.count("NGX.IlluminationStrength"))
            ctx.ngx.illuminationStrength = ParseInt(settings["NGX.IlluminationStrength"], 50);
        if (settings.count("NGX.OcclusionStrength"))
            ctx.ngx.occlusionStrength = ParseInt(settings["NGX.OcclusionStrength"], 50);
        // HUD detection mode: 0 = AUTO (default when missing), 1 = OFF, 2 = ON
        if (settings.count("NGX.HudDetectionMode"))
        {
            int v = ParseInt(settings["NGX.HudDetectionMode"], 0);
            if (v < 0 || v > 2) v = 0;
            ctx.ngx.hudDetectionMode = v;
            if (v == 0) {
                ctx.ngx.isHudInterpolationEnabled = true;
            }
        }

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
        if (settings.count("Performance.HudInterpolation"))
            ctx.ngx.isHudInterpolationEnabled = ParseBool(settings["Performance.HudInterpolation"]);

        // MFG Override - new encoding (UI mnemonics):
        //   0 = OFF, 1 = x1 (force off), 2 = x2, 3 = x3, 4 = x4, 5 = x5, 6 = x6
        // New key 'MFGOverrideMode' takes priority; legacy 'MFGEnforcedMode' is migrated
        // from old encoding {0=OFF, 2=3X, 3=4X, 4=5X, 5=6X} to the new one.
        if (settings.count("Performance.MFGOverrideMode"))
        {
            int v = ParseInt(settings["Performance.MFGOverrideMode"], 0);
            if (v < 0 || v > 6) v = 0;
            ctx.nvapi.mfgEnforcedMode = v;
        }
        else if (settings.count("Performance.MFGEnforcedMode"))
        {
            // Legacy migration: old values {0,2,3,4,5} map to {OFF, x3, x4, x5, x6}
            int legacy = ParseInt(settings["Performance.MFGEnforcedMode"], 0);
            int migrated = 0;
            switch (legacy)
            {
            case 0: migrated = 0; break; // OFF stays OFF
            case 2: migrated = 3; break; // old 3X -> new x3
            case 3: migrated = 4; break; // old 4X -> new x4
            case 4: migrated = 5; break; // old 5X -> new x5
            case 5: migrated = 6; break; // old 6X -> new x6
            default: migrated = 0; break; // anything unexpected -> OFF
            }
            ctx.nvapi.mfgEnforcedMode = migrated;
        }

        if (settings.count("Performance.MFGHotkeys"))
            ctx.areHotKeysEnabled = ParseBool(settings["Performance.MFGHotkeys"]);

        if (settings.count("Performance.DynamicMFG"))
            ctx.streamline.isDynamicMfgEnabled = ParseBool(settings["Performance.DynamicMFG"]);
        if (settings.count("Performance.DynamicMFGThreshold2"))
            ctx.streamline.dynamicMfgThreshold2 = ParseInt(settings["Performance.DynamicMFGThreshold2"], 90);
        if (settings.count("Performance.DynamicMFGThreshold3"))
            ctx.streamline.dynamicMfgThreshold3 = ParseInt(settings["Performance.DynamicMFGThreshold3"], 60);
        if (settings.count("Performance.DynamicMFGThreshold4"))
            ctx.streamline.dynamicMfgThreshold4 = ParseInt(settings["Performance.DynamicMFGThreshold4"], 30);
        if (settings.count("Performance.DynamicMFGThreshold5"))
            ctx.streamline.dynamicMfgThreshold5 = ParseInt(settings["Performance.DynamicMFGThreshold5"], 24);
        if (settings.count("Performance.DynamicMFGThreshold6"))
            ctx.streamline.dynamicMfgThreshold6 = ParseInt(settings["Performance.DynamicMFGThreshold6"], 20);
        if (settings.count("Performance.DFGMode"))
            ctx.streamline.dfgMode = ParseInt(settings["Performance.DFGMode"], 0);
        if (settings.count("Performance.DFGTargetFps"))
            ctx.streamline.dfgTargetFps = ParseInt(settings["Performance.DFGTargetFps"], 120);
        if (settings.count("Performance.DFGInstinct"))
            ctx.streamline.isMinDynamicFpsActive = ParseBool(settings["Performance.DFGInstinct"]);
        if (settings.count("Performance.DFGMinFps"))
            ctx.streamline.minDynamicFps = (float)ParseInt(settings["Performance.DFGMinFps"], 60);
        if (settings.count("Performance.ForceLoadDLSSG"))
            ctx.streamline.forceLoadDLSSG = ParseBool(settings["Performance.ForceLoadDLSSG"]);

        // Debug Section
        // GhostBuster flag constants - must match SettingsMenu.cpp
        const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE = 0x00100000;
        const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT = 0x00200000;
        const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN = 0x00400000;
        const uint32_t MFG_ANTIGHOSTING_FLAGS_MASK = MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE |
            MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT |
            MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN;

        if (settings.count("Debug.Flags"))
        {
            // Load flags but mask out GhostBuster bits (they're managed separately)
            uint32_t loadedFlags = static_cast<uint32_t>(ParseInt(settings["Debug.Flags"], 0));
            ctx.flags = loadedFlags & ~MFG_ANTIGHOSTING_FLAGS_MASK;
        }

        if (settings.count("Debug.EarlyInit"))
            ctx.ngx.isEarlyInitEnabled = ParseBool(settings["Debug.EarlyInit"]);
        if (settings.count("Debug.UseFsrOnly"))
            ctx.ngx.isDlssgDisabled = ParseBool(settings["Debug.UseFsrOnly"]);
        if (settings.count("Debug.HybridMfgForced"))
            ctx.ngx.isHybridMfgForced = ParseBool(settings["Debug.HybridMfgForced"]);

        // GhostBuster Section - loaded separately and will set flags in UI code
        if (settings.count("GhostBuster.Enabled"))
            ctx.ngx.isGhostBustingEnabled = ParseBool(settings["GhostBuster.Enabled"]);
        if (settings.count("GhostBuster.DebugMode"))
            ctx.ghostBusterDebugMode = ParseInt(settings["GhostBuster.DebugMode"], 0);

        // Propagate GhostBuster settings to flags
        if (ctx.ngx.isGhostBustingEnabled) {
            ctx.flags |= MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE;
        }
        if (ctx.ghostBusterDebugMode == 1) {
            ctx.flags |= MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN;
        }
        else if (ctx.ghostBusterDebugMode == 2) {
            ctx.flags |= MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT;
        }

        // UI.ToggleKey - user-friendly name (e.g. "Tilde", "F1", "VK_INSERT")
        if (settings.count("UI.ToggleKey"))
        {
            int vk = SettingsMenu::FriendlyNameToVk(settings["UI.ToggleKey"].c_str());
            if (vk > 0)
                SettingsMenu::SetMenuToggleKey(vk);
        }

        // UI.ScaleOverride - 0 = auto, >0 = forced scale (e.g. "1.5", "2.0")
        if (settings.count("UI.ScaleOverride"))
            SettingsMenu::SetUiScaleOverride(ParseFloat(settings["UI.ScaleOverride"], 0.0f));

        // -------------------------------------------------------------------
        // Config schema migration
        // -------------------------------------------------------------------
        // Files written before kGhostBusterOnVersion get GhostBuster forced ON,
        // overriding whatever they stored, because it became the intended default
        // in that release. Two cases count as "older":
        //   - no [Meta] configVersion key at all (predates the versioned schema)
        //   - a configVersion that parses to something below the threshold
        // An unparseable configVersion is treated as older too, on the grounds
        // that a file we cannot date is a file we cannot trust to be current.
        //
        // Save() at the end stamps the writing DLL's version, so this runs once.
        {
            const ConfigVersion kGhostBusterOnVersion = { { 4, 9, 0, 8 } };

            bool iniIsOlder = true;

            auto it = settings.find("Meta.configVersion");
            if (it != settings.end())
            {
                ConfigVersion fileVersion;
                if (ParseVersion(it->second, fileVersion))
                    iniIsOlder = (CompareVersions(fileVersion, kGhostBusterOnVersion) < 0);
            }

            if (iniIsOlder)
            {
                const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE = 0x00100000;

                ctx.ngx.isGhostBustingEnabled = true;

                // Earlier in Load() the flag bit was set from the (now overridden)
                // INI value, so set it explicitly rather than relying on that.
                ctx.flags |= MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE;

                // Save() rewrites the file with the [Meta] configVersion header,
                // also calls TakeSnapshot(g_LastSavedSnapshot) and clears
                // g_HasUnsavedChanges, so we can return directly.
                Save();
                return true;
            }
        }

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

        // Meta Section - schema version. Pulled live from the DLL's own
        // VERSIONINFO resource so it always matches the binary that wrote it.
        // Removing the configVersion key on disk triggers a one-shot
        // migration on next Load() (see Load() for details).
        auto fullModulePath = Common::GetModuleFilePath();
        std::wstring dlssEnablerVersion = Common::GetFileVersion(fullModulePath.c_str());
        file << "[Meta]\n";
        file << "; Schema version, taken from the DLL that wrote this file. Delete this line to force a one-time settings migration on next launch.\n";
        file << "configVersion=" << WideToUtf8(dlssEnablerVersion) << "\n";
        file << "\n";

        // NGX Section
        file << "[NGX]\n";
        file << "; Enable screen-space ray tracing (SSRTGI master switch). Needs D3D12 + active DLSS upscaling to take effect.\n";
        file << "ScreenSpaceRayTracing=" << (ctx.ngx.isScreenSpaceRayTracingEnabled ? "true" : "false") << "\n";
        file << "; Enable ray-traced global illumination.\n";
        file << "GlobalIllumination=" << (ctx.ngx.isGlobalIlluminationEnabled ? "true" : "false") << "\n";
        file << "; Enable ray-traced ambient occlusion / contact shadows.\n";
        file << "AmbientOcclusion=" << (ctx.ngx.isAmbientOcclusionEnabled ? "true" : "false") << "\n";
        file << "; SSRTGI quality: 0 = ULTRA, 1 = HIGH, 2 = MEDIUM, 3 = LOW. Ignored while Overdrive is active.\n";
        file << "RayTracingQuality=" << ctx.ngx.rayTracingQuality << "\n";
        file << "; SSRTGI ray length / effective range, 1-100.\n";
        file << "RayTracingRange=" << ctx.ngx.rayTracingRange << "\n";
        file << "; Global illumination intensity, 1-100.\n";
        file << "IlluminationStrength=" << ctx.ngx.illuminationStrength << "\n";
        file << "; Contact-shadow / AO intensity, 1-100.\n";
        file << "OcclusionStrength=" << ctx.ngx.occlusionStrength << "\n";
        file << "; HUD detection: 0 = GAME, 1 = OFF, 2 = ON.\n";
        file << "HudDetectionMode=" << ctx.ngx.hudDetectionMode << "\n";
        file << "\n";

        // Reflex Section
        file << "[Reflex]\n";
        file << "; Override the game's V-Sync setting with the Vsync value below.\n";
        file << "VsyncOverride=" << (ctx.reflex.isVsyncOverrideEnabled ? "true" : "false") << "\n";
        file << "; V-Sync state applied when VsyncOverride is enabled.\n";
        file << "Vsync=" << (ctx.reflex.isVsyncEnabled ? "true" : "false") << "\n";
        file << "; Enable the Reflex frame-rate limiter (uses DesiredFpsLimit).\n";
        file << "FpsLimit=" << (ctx.reflex.isFpsLimitEnabled ? "true" : "false") << "\n";
        file << "; Override the game's Reflex Boost setting with the Boost value below.\n";
        file << "BoostOverride=" << (ctx.reflex.isBoostOverriden ? "true" : "false") << "\n";
        file << "; Reflex Boost state applied when BoostOverride is enabled.\n";
        file << "Boost=" << (ctx.reflex.isBoostEnabled ? "true" : "false") << "\n";
        file << "; Target FPS cap used when FpsLimit is enabled; range 30-360.\n";
        file << "DesiredFpsLimit=" << ctx.reflex.desiredFpsLimit << "\n";
        file << "\n";

        // UI Section
        file << "[UI]\n";
        file << "; Show the performance monitoring overlay.\n";
        file << "Monitoring=" << (ctx.isMonitoringEnabled ? "true" : "false") << "\n";
        file << "; Show the settings sidebar panel.\n";
        file << "SideBar=" << (ctx.isSideBarEnabled ? "true" : "false") << "\n";
        file << "; Menu toggle key. Any Win32 virtual-key code works.\n";
        file << "; Accepts a named key (Tilde, F1-F12, VK_INSERT, Numpad0-9, VK_OEM_1..7, VK_TAB...), a single A-Z / 0-9 char, hex 0xNN, or decimal 1-255. Default: Tilde.\n";
        file << "; See: https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes\n";
        file << "ToggleKey=" << SettingsMenu::VkToFriendlyName(SettingsMenu::GetMenuToggleKey()) << "\n";
        file << "; UI scale override; 0 = auto. Multiplier of base UI size, e.g. 1.5 = +50% larger, 2.0 = double, 1.0 = no change.\n";
        file << "ScaleOverride=" << SettingsMenu::GetUiScaleOverride() << "\n";
        file << "\n";

        // DeepDVC Section
        file << "[DeepDVC]\n";
        file << "; DeepDVC effect intensity (0.0-1.0).\n";
        file << "Intensity=" << ctx.deepDVC.intensity << "\n";
        file << "; DeepDVC saturation boost (0.0-1.0).\n";
        file << "SaturationBoost=" << ctx.deepDVC.saturationBoost << "\n";
        file << "\n";

        // Performance Section (only if overdrive is enabled/visible)
        if (ctx.overdriveMode >= 0)
        {
            file << "[Performance]\n";
            file << "; Overdrive preset: 0 = OFF, 1 = PERF, 2 = QUALITY, 3 = LATENCY.\n";
            file << "OverdriveMode=" << ctx.overdriveMode << "\n";
            //file << "; Interpolate the HUD together with the frame (may cause artifacts).\n";
            //file << "HudInterpolation=" << (ctx.ngx.isHudInterpolationEnabled ? "true" : "false") << "\n";
            file << "; Forced MFG multiplier: 0 = OFF (game controls), 1 = x1 (FG off), 2 = x2, 3 = x3, 4 = x4, 5 = x5, 6 = x6. x5/x6 require sl.dlss_g >= 2.11.\n";
            file << "MFGOverrideMode=" << ctx.nvapi.mfgEnforcedMode << "\n";
            file << "; Enable hotkeys for switching MFG modes at runtime.\n";
            file << "MFGHotkeys=" << (ctx.areHotKeysEnabled ? "true" : "false") << "\n";
            file << "; Enable dynamic MFG (auto-select the multiplier from base FPS).\n";
            file << "DynamicMFG=" << (ctx.streamline.isDynamicMfgEnabled ? "true" : "false") << "\n";
            file << "; Dynamic MFG thresholds (used only in CUSTOM mode, DFGMode=1). Base FPS BELOW a threshold selects that\n";
            file << "; multiplier; lower thresholds win (x6 is checked first). Range 10-200 FPS, min 5 FPS gap between them,\n";
            file << "; order must hold: Threshold6 <= Threshold5 <= Threshold4 <= Threshold3 <= Threshold2.\n";
            file << "; In AUTO mode (DFGMode=0) these are recomputed as ceil(DFGTargetFps / N) and ignored on load.\n";
            file << "; Base FPS below this -> 2X (default 90).\n";
            file << "DynamicMFGThreshold2=" << ctx.streamline.dynamicMfgThreshold2 << "\n";
            file << "; Base FPS below this -> 3X (default 60).\n";
            file << "DynamicMFGThreshold3=" << ctx.streamline.dynamicMfgThreshold3 << "\n";
            file << "; Base FPS below this -> 4X (default 30).\n";
            file << "DynamicMFGThreshold4=" << ctx.streamline.dynamicMfgThreshold4 << "\n";
            file << "; Base FPS below this -> 5X (default 24).\n";
            file << "DynamicMFGThreshold5=" << ctx.streamline.dynamicMfgThreshold5 << "\n";
            file << "; Base FPS below this -> 6X (default 20).\n";
            file << "DynamicMFGThreshold6=" << ctx.streamline.dynamicMfgThreshold6 << "\n";
            file << "; Dynamic FG threshold source: 0 = AUTO (thresholds from DFGTargetFps), 1 = CUSTOM (manual thresholds above).\n";
            file << "DFGMode=" << ctx.streamline.dfgMode << "\n";
            file << "; Target output FPS for DFG AUTO mode; range 30-300.\n";
            file << "DFGTargetFps=" << ctx.streamline.dfgTargetFps << "\n";
            file << "; DFG Instinct: raise the multiplier during action to reduce latency.\n";
            file << "DFGInstinct=" << (ctx.streamline.isMinDynamicFpsActive ? "true" : "false") << "\n";
            file << "; DFG Instinct base-FPS floor; range 30-240. Won't raise the multiplier if output FPS would drop below this.\n";
            file << "DFGMinFps=" << (int)ctx.streamline.minDynamicFps << "\n";
            file << "; Force-load the DLSSG module even when the game does not request it.\n";
            file << "ForceLoadDLSSG=" << (ctx.streamline.forceLoadDLSSG ? "true" : "false") << "\n";
            file << "\n";
        }

        // Debug Section
        // Save flags without GhostBuster bits (they're saved separately)
        const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE = 0x00100000;
        const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT = 0x00200000;
        const uint32_t MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN = 0x00400000;
        const uint32_t MFG_ANTIGHOSTING_FLAGS_MASK = MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE |
            MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT |
            MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN;
        uint32_t flagsWithoutGhostBuster = ctx.flags & ~MFG_ANTIGHOSTING_FLAGS_MASK;
        file << "[Debug]\n";
        file << "; Raw MFG debug/feature flags, stored as a decimal bitmask. Anti-ghosting bits are stripped and kept in [GhostBuster].\n";
        file << "; Feature bits:  0x02000000 DLSSG.UI as UI mask (DyingLight 2 only) | 0x04000000 temporal HUD pin | 0x08000000 HUD optical-flow interp |\n";
        file << ";                0x10000000 ignore DLSSG.UI texture | 0x40000000 pin DLSSG.Backbuffer to subframe-1.\n";
        file << "; Debug viz:     0x00010000 frame-index line | 0x00020000 HUD-detection | 0x00040000 disocclusion tint |\n";
        file << ";                0x00080000 artifacts detection | 0x00800000 camera-MV fallback tint | 0x01000000 trapezoid-zone viz.\n";
        file << "Flags=" << flagsWithoutGhostBuster << "\n";
        file << "; Initialize OptiScaler early during startup.\n";
        file << "EarlyInit=" << (ctx.ngx.isEarlyInitEnabled ? "true" : "false") << "\n";
        file << "; Force FSR-only frame generation, bypassing DLSSG.\n";
        file << "UseFsrOnly=" << (ctx.ngx.isDlssgDisabled ? "true" : "false") << "\n";
        file << "; Force hybrid MFG on; false = AUTO.\n";
        file << "HybridMfgForced=" << (ctx.ngx.isHybridMfgForced ? "true" : "false") << "\n";
        file << "; Disable the in-game overlay UI (always written as false by the app).\n";
        file << "DisableUI=false\n";
        file << "\n";

        // GhostBuster Section - saved as separate boolean values
        file << "[GhostBuster]\n";
        file << "; Enable GhostBuster anti-ghosting correction (maps to the ANTIGHOSTING_ENABLE flag bit).\n";
        file << "Enabled=" << (ctx.ngx.isGhostBustingEnabled ? "true" : "false") << "\n";
        file << "; GhostBuster debug visualization mode; 0 = off.\n";
        file << "DebugMode=" << ctx.ghostBusterDebugMode << "\n";

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

        // UI scale from SettingsMenu (proportional to 1080p, never below 1.0)
        const float sc = SettingsMenu::GetUiScale();

        // Dimensions matching side panel
        const float panelWidth = 280.0f * sc;
        const float panelMargin = 10.0f * sc;
        const float promptHeight = 75.0f * sc;

        // The side panel sizes itself to its content, so its height cannot be recomputed
        // here - it has to be asked for. Zero means the panel is not on screen, and the
        // prompt then sits at the top margin on its own.
        const float panelHeight = SettingsMenu::GetSidePanelHeight();
        const float promptWidth = panelWidth;

        // Calculate Y position - below sidebar
        float promptY = (panelHeight > 1.0f)
            ? (panelMargin + panelHeight + 10.0f * sc)
            : panelMargin;

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
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowRounding, 8.0f * sc);
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowPadding, UxImVec2(15.0f * sc, 12.0f * sc));
        UxImGui::PushStyleVar(UxImGuiStyleVar_WindowBorderSize, 1.0f * sc);
        UxImGui::PushStyleVar(UxImGuiStyleVar_ItemSpacing, UxImVec2(8.0f * sc, 8.0f * sc));

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
            // Scale font
            UxImGui::SetWindowFontScale(sc);

            UxImDrawList* drawList = UxImGui::GetWindowDrawList();
            UxImVec2 windowPos = UxImGui::GetWindowPos();
            UxImVec2 windowSize = UxImGui::GetWindowSize();

            // Left accent bar (orange/amber)
            UxImU32 accentColor = IM_COL32(255, 160, 60, 255);
            drawList->AddRectFilled(
                windowPos,
                UxImVec2(windowPos.x + 3.0f * sc, windowPos.y + windowSize.y),
                accentColor
            );

            // Text - with left padding for accent bar
            UxImGui::SetCursorPosX(8.0f * sc);
            UxImGui::PushStyleColor(UxImGuiCol_Text, UxImVec4(1.0f, 0.85f, 0.6f, 1.0f));
            UxImGui::Text("Settings changed. Save?");
            UxImGui::PopStyleColor();

            UxImGui::Dummy(UxImVec2(0, 2.0f * sc));

            // Calculate button width based on available content region
            float contentWidth = UxImGui::GetContentRegionAvail().x;
            float buttonWidth = (contentWidth - 8.0f * sc) / 2.0f;

            // Persist button (green/teal)
            UxImGui::PushStyleColor(UxImGuiCol_Button, UxImVec4(0.0f, 0.5f, 0.45f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonHovered, UxImVec4(0.0f, 0.6f, 0.55f, 1.0f));
            UxImGui::PushStyleColor(UxImGuiCol_ButtonActive, UxImVec4(0.0f, 0.4f, 0.35f, 1.0f));
            UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f * sc);

            if (UxImGui::Button("Persist", UxImVec2(buttonWidth, 26.0f * sc)))
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
            UxImGui::PushStyleVar(UxImGuiStyleVar_FrameRounding, 4.0f * sc);

            if (UxImGui::Button("Ignore", UxImVec2(buttonWidth, 26.0f * sc)))
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