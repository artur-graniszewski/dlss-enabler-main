#pragma once

#include <string>
#include <stack>

#include <d3d12.h>
#include "../Includes/dlss/nvsdk_ngx_defs.h"
#include <dxgi1_6.h>
#include "../Includes/nvapi.h"

#define UPSCALING_METHOD_AUTO	L"auto"
#define UPSCALING_METHOD_DLSS	L"DLSS"
#define UPSCALING_METHOD_XESS	L"XeSS"
#define UPSCALING_METHOD_FSR	L"FSR"
#define UPSCALING_METHOD_FSR22	L"FSR 2.2"
#define UPSCALING_METHOD_FSR31	L"FSR 3.1"

#define FRAMEGENERATION_METHOD_AUTO		L"auto"
#define FRAMEGENERATION_METHOD_FSR31	L"FSR 3.1"
#define FRAMEGENERATION_METHOD_FSR30	L"FSR 3.0"
#define FRAMEGENERATION_METHOD_DLSSG	L"DLSSG"
#define FRAMEGENERATION_METHOD_FSR3		L"FSR 3"

using namespace std;

struct Hardware
{
    wstring deviceName = L"DLSS Enabler GPU Adapter";
    char* desiredDeviceName;
    int refreshRate = -1; // -1 = unknown, 0 = variable?, > 0 specific refresh rate
    SIZE_T desiredDedicatedVideoMemory = 0;
    SIZE_T dedicatedVideoMemory = 0;
    LUID luid;
    UINT vendorId;
    UINT deviceId;
    UINT subSysId;
    UINT revisionId;
    bool isHagsEnabled = true;
    bool isFakeRtxNameRequired = false;
};

struct NvApi
{
    HMODULE nvapi = nullptr;
    bool isGenuineFileLoaded = false;
    bool isProxyEnabled = false;
    bool isMockEnabled = false;
    bool isProxyLoaded = false;
    bool isInitialized = false;
    bool isEmbeddedNvapiUsed = false;
    bool isXellEnabled = false;
    bool isRealHardwareDetected = false;
    bool isHighestArchEnabled = false;
    int mfgEnforcedMode = 0;
};

struct Ngx
{ 
    HMODULE ngx = nullptr;
    wstring upscalingMethod = UPSCALING_METHOD_AUTO;
    wstring configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_AUTO;
    wstring configuredUpscalingMethod = UPSCALING_METHOD_AUTO;
    wstring configuredVkUpscalingMethod = UPSCALING_METHOD_AUTO;
    bool isUiPinningEnabled = false;
    bool isUiTextureEnabled = false;
    bool isRealNgxHidden = false;
    bool isProxyEnabled = true;
    bool isEmbeddedDlssgUsed = true;
    bool isEmbeddedNgxUsed = true;
    bool isGhostBustingEnabled = true;
    bool isAutoExposureEnabled = false;
    bool isRealNgxPresent = false;
    bool isEarlyInitEnabled = false;
    bool isFrameGenerationEnabled = true;
    bool isUpscalingActive = false;
    bool isDynamicFrameGenerationEnabled = false;
    bool isDynamicFrameGenerationStartingOnThreshold = true;
    bool isFullScreenMenuDetectionEnabled = true;
    int dynamicFrameGenerationThreshold = 60;
    bool isDlssEnabled = false;
    bool isDlssgEnabled = false;
    bool isDeepDvcEnabled = true;
    bool isDlssgSupportedByHardware = false;
    bool isDlssgMultiframeSupported = true;
    bool isDlssgDisabled = false;
    bool isDlssSupportedByHardware = false;
    bool isDuplicatingFrames = false;
    bool isGeneratingFrames = false;
    bool isDlssgProfilerEnabled = false;
    bool isPerformanceModeEnabled = false;
    bool isNgxDeepSearchEnabled = false;
    unsigned int framesGenerated = 0;
    unsigned int maxFramesGenerated = 1;
    unsigned int upscalingQuality = 0;
    bool enableDlssUpscaler = true;
    bool isScreenSpaceRayTracingEnabled = false;
    bool isGlobalIlluminationEnabled = true;      // Global Illumination
    int rayTracingRange = 50;            // 1-100 (Ray Tracing range)
    int rayTracingQuality = 1;         // 0=ULTRA, 1=HIGH, 2=MEDIUM, 3=LOW (default HIGH)
    int illuminationStrength = 50;       // 1-100
    bool isAmbientOcclusionEnabled = true;        // Ambient Occlusion
    int occlusionStrength = 50;          // 1-100

    bool overrideDlssUpscalerCapability = true;
    std::deque<double> frametimeHistory;
    double lastEvaluationTimeMsec = 0.0f;
    bool isFrameGenerationActive = false;
    bool isNextFrameSkippable = false;
    bool isHudInterpolationEnabled = false;
    bool isHudlessMaskEnabled = false;
    bool isHybridMfgEnabled = false;
    bool isHybridMfgForced = false;
    uint64_t dlaaId = NVSDK_NGX_PerfQuality_Value_DLAA; // can be overriden... if game is broken, eg Pragmata sets 3 and expects 5....
    int hudDetectionMode = 0;
};

struct Streamline
{
    wstring dlssgVersion = L"0.0.0.0";
    wstring dlssgName = L"sl.dlss_g.dll";
    wstring dlssdVersion = L"0.0.0.0";
    wstring dlssdName = L"sl.dlss_d.dll";
    wstring deepDvcVersion = L"0.0.0.0";
    wstring deepDvcName = L"sl.deepdvc.dll";
    wstring dlssVersion = L"0.0.0.0";
    wstring dlssName = L"sl.dlss.dll";
    wstring commonVersion = L"0.0.0.0";
    wstring commonName = L"sl.common.dll";
    wstring reflexVersion = L"0.0.0.0";
    wstring reflexName = L"sl.reflex.dll";
    wstring pclVersion = L"0.0.0.0";
    wstring pclName = L"sl.pcl.dll";
    wstring interposerVersion = L"0.0.0.0";
    wstring interposerName = L"sl.interposer.dll";
    wstring interposerPath = L"";
    float actionIntensityBoost = 1.25f;
    bool forceLoadDeepDvc = false;
    bool forceLoadDLSSG = false;
    bool isSelectiveSpoofingEnabled = false;
    bool isPresentHookEnabled = false;
    bool isPresent = false;
    bool isHudInterpolationEnabled = false;
    int mfgEnforcedMode = 0;
    bool isDynamicMfgEnabled = false;
    int dynamicMfgThreshold2 = 90;
    int dynamicMfgThreshold3 = 60;
    int dynamicMfgThreshold4 = 30;
    int dynamicMfgThreshold5 = 15;
    int dynamicMfgThreshold6 = 5;
    int dfgMode = 0;          // 0 = AUTO, 1 = CUSTOM
    int dfgTargetFps = 120;   // Target FPS
    float minDynamicFps = 30;
    bool isMinDynamicFpsActive = false;
};

struct Logging
{
    bool isUltraDebugEnabled = false;
    bool isExtraDebugEnabled = true;
    bool isDebugEnabled = false;
    bool isConsoleEnabled = false;
    bool isReflexDebugEnabled = false;
    bool isNvapiDebugEnabled = true;
    bool isNvngxDebugEnabled = true;
    bool isStreamlineDebugEnabled = true;
    bool isDxgiDebugEnabled = true;
};

struct Emulation
{
    bool isRegistrySpoofed = false;
    bool isHagsSpoofed = false;
    bool isVulkanSpoofed = false;
    bool isDxgiSpoofed = false;
    bool isHighestArch = false;
    bool forceHighestArch = false;
}; 

struct DirectX
{
    bool isAnisotropicForced = false;
    bool isTrilinearForced = false;
    bool isBilinearForced = false;
    int maxAnisotropy = 0;
    int skipTopMips = 0;
    bool isSpoofingEnabled = true;
    bool isHdrEnabled = false;
};

struct Reflex
{
    bool isDoubleBufferingEnforced = false;
    bool isEmulationEnabled = false;
    bool isLocalReflexUsed = false;
    bool isEnabled = true;
    bool isOriginallyEnabled = true;
    bool isBoostOriginallyEnabled = false;
    bool isBoostEnabled = false;
    bool isBoostOverriden = false;
    bool isMarkersOptimizationEnabled = false;
    bool isReset = false;
    bool isVsyncEnabled = false;
    bool isVsyncOverrideEnabled = false;
    bool isGameVsyncEnabled = false;    // True if game sets SyncInterval > 0
    bool isGameTearingEnabled = false;    // True if game sets SyncInterval > 0
    UINT lastSyncInterval = 0;          // effective SyncInterval used in Present
    UINT lastOriginalSyncInterval = 0;  // original SyncInterval from game (unmodified)
    bool isFpsLimitEnabled = false;
    int desiredFpsLimit = 60;
    int potentialFps = 0;
    int currentFps = 0;
    double realFpsLimit = 0;
    bool isFakeFrame = false;
    bool isBaseFpsLimitEnabled = false;
    uint64_t frameId = 0;
    uint64_t evalId = 0;
    bool isOptiFgEnabled = false;
    uint64_t optiFgCycle = 0;

    double timeFrameDeltaMsec = 0.0f;
};

struct Ui
{
    bool isStatusBarEnabled = false; // should status bar be opened during the application start?
};

struct DeepDVC
{
    float intensity = 0.5;
    float saturationBoost = 0.75;
};

// Define the global structure
struct Context
{
    // data about the game
    NVSDK_NGX_EngineType engineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM;
    string engineVersion = "1.0.0";
    unsigned long driverVersion = 99933;
    string projectId = "DLSS Enabler";
    Hardware gpu;
    NvApi nvapi;
    Ngx ngx;
    Emulation emulation;
    Streamline streamline;
    bool isFirstRun = false;
    DeepDVC deepDVC;
    DirectX directX;
    unsigned long long applicationId = 0;

    // spoofing data
    bool enableRegistrySpoofing = false; // might be needed for Nixxess games, but could crash other games
    bool enableVulkanSpoofing = false;
    bool isVulkanApplication = false;
    bool isRunningUnderWindows = true;
    bool isDlssEnablerOn = true;
    bool isMonitoringEnabled = false;
    NV_GPU_ARCHITECTURE_ID targetGpuArchitecture = NV_GPU_ARCHITECTURE_AD100;
    NV_GPU_ARCHITECTURE_ID currentGpuArchitecture = NV_GPU_ARCHITECTURE_AD100;
    NV_GPU_ARCHITECTURE_ID realGpuArchitecture = NV_GPU_ARCHITECTURE_GF100;

    bool deactivateDFG = false;
    bool isOptiscalerInitialized = false;

    int fsr3fgVersion = 1; // 1 = 3.1, 0 = 3.0
    int overdriveMode = 0;
    bool isOptiPatcherActive = false;

    bool enableNgxNativeResolution = false;
    bool enableDirectSwapchainHooking = false;
    bool enableSwapchain1x1Protection = false;
    bool enableReflexInjection = false;
    bool enableUnloadProtection = true;

    bool isUiEnabled = true;
    bool isProjectIdReported = false;
    bool isLoadedByDxgi = false;
    bool isValidationOn = true;
    bool isSideBarEnabled = true;
    bool isOptiscalerSwapchainHookEnabled = true;
    uint32_t flags;
    bool quickBoot = false;
    int ghostBusterDebugMode = 0;
    bool frameGenerationMode = 0; // 0 => FSR3, 1 => DLSSG
    bool areHotKeysEnabled = false;
    Logging logging;
    Reflex reflex;
};

// Declare the global instance of the structure
extern Context ctx;