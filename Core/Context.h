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
    bool isGenuineFileLoaded = false;
    bool isProxyEnabled = false;
    bool isMockEnabled = false;
    bool isProxyLoaded = false;
    bool isInitialized = false;
    bool isEmbeddedNvapiUsed = false;
    bool isXellEnabled = false;
};

struct Ngx
{
    wstring upscalingMethod = UPSCALING_METHOD_AUTO;
    wstring configuredFrameGenerationMethod = FRAMEGENERATION_METHOD_AUTO;
    wstring configuredUpscalingMethod = UPSCALING_METHOD_AUTO;
    wstring configuredVkUpscalingMethod = UPSCALING_METHOD_AUTO;
    bool isRealNgxHidden = false;
    bool isProxyEnabled = true;
    bool isEmbeddedDlssgUsed = true;
    bool isEmbeddedNgxUsed = true;
    bool isRealNgxPresent = false;
    bool isFrameGenerationEnabled = true;
    bool isUpscalingActive = false;
    bool isDynamicFrameGenerationEnabled = false;
    bool isDynamicFrameGenerationStartingOnThreshold = true;
    int dynamicFrameGenerationThreshold = 60;
    bool isDlssEnabled = false;
    bool isDlssgEnabled = false;
    bool isDeepDvcEnabled = true;
    bool isDlssgSupportedByHardware = false;
    bool isDlssSupportedByHardware = false;
    bool isDuplicatingFrames = false;
    bool isGeneratingFrames = false;
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
};

struct Streamline
{
    wstring interposerVersion = L"0.0.0.0";
    wstring dlssgVersion = L"0.0.0.0";
    wstring dlssdVersion = L"0.0.0.0";
    wstring deepDvcVersion = L"0.0.0.0";
    wstring dlssVersion = L"0.0.0.0";
    wstring commonVersion = L"0.0.0.0";
    wstring reflexVersion = L"0.0.0.0";
    wstring interposerPath = L"";
    bool forceLoadDeepDvc = false;
    bool forceLoadDLSSG = false;
    bool spoofNextCreateFactoryCall = false;
    bool isPresent = false;
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
    unsigned long driverVersion = 56633;
    string projectId = "DLSS Enabler";
    Hardware gpu;
    NvApi nvapi;
    Ngx ngx;
    Emulation emulation;
    Streamline streamline;
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
    int overdriveMode = -1;

    bool enableNgxNativeResolution = false;
    bool enableDirectSwapchainHooking = false;
    bool enableReflexInjection = false;
    bool enableUnloadProtection = true;

    bool isProjectIdReported = false;
    bool isLoadedByDxgi = false;
    bool isValidationOn = true;
    bool isSideBarEnabled = true;
    bool isOptiscalerSwapchainHookEnabled = true;
    uint32_t flags;
    bool quickBoot = false;

    Logging logging;
    Reflex reflex;
};

// Declare the global instance of the structure
extern Context ctx;
