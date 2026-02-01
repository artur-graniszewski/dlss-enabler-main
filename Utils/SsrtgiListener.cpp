// =============================================================================
// SsrtgiListener.cpp - SSRTGI Post-Processing Effect Registration
// =============================================================================
//
// Registers SSRTGI as a listener for NGX feature events.
// This decouples SSRTGI from NgxFrontend - NgxFrontend dispatches events,
// and this listener handles them for SuperSampling features.
//
// =============================================================================

#include "NgxFeatureEvents.h"
#include "SsrtgiPostProcessD3D12.h"
#include "PostFxRegistry.h"
#include "Common.h"
#include "../Core/Context.h"
#include <vector>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <cmath>

// =============================================================================
// Shader Loading Configuration
// =============================================================================
// Define SSRTGI_USE_EXTERNAL_CSO to load shaders from external .cso files
// (original behavior). If not defined, embedded bytecode will be used.
// 
// #define SSRTGI_USE_EXTERNAL_CSO
// =============================================================================

#ifndef SSRTGI_USE_EXTERNAL_CSO
// Include embedded shader bytecode
#include "SsrtgiCopyPass_cso.h"
#include "SsrtgiDenoise_cso.h"
#include "SsrtgiDepthMinMax_cso.h"
#include "SsrtgiGatherHZB_cso.h"
#include "SsrtgiHzbGenerate_cso.h"
#include "SsrtgiTemporal_cso.h"
#include "SsrtgiComposite_cso.h"
#endif

// =============================================================================
// Configuration
// =============================================================================

// Configurable CSO directory path (used when SSRTGI_USE_EXTERNAL_CSO is defined)
static std::wstring g_CsoDirectory = L"";

// Hot-reload enable flag (only applicable when using external CSO files)
static bool g_HotReloadEnabled = true;

// =============================================================================
// UI Parameter Mapping Functions
// =============================================================================

// Map rayTracingQuality (0=ULTRA, 1=HIGH, 2=MEDIUM, 3=LOW) to NumRays
static uint32_t MapQualityToNumRays(int quality)
{
    switch (quality)
    {
    case 0: return 32;  // ULTRA
    case 1: return 16;  // HIGH
    case 2: return 8;   // MEDIUM
    case 3: return 4;   // LOW
    default: return 16; // Default to HIGH
    }
}

// Map rayTracingRange (0-100) to FalloffEnd (for Composite shader only)
// Default COMPOSITE_FALLOFF_END was 30.0, so 50% -> 30
// Must be > COMPOSITE_FALLOFF_START (10.8), so minimum is 15
// 0% -> 15, 50% -> 30, 100% -> 100
static float MapRangeToFalloffEnd(int rangePercent)
{
    float t = static_cast<float>((std::max)(0, (std::min)(100, rangePercent))) / 100.0f;

    // Piecewise linear: 15 to 30 for t<=0.5, then 30 to 100 for t>0.5
    float result;
    if (t <= 0.5f)
    {
        result = 15.0f + (30.0f - 15.0f) * (t / 0.5f);  // 15 to 30
    }
    else
    {
        result = 30.0f + (100.0f - 30.0f) * ((t - 0.5f) / 0.5f);  // 30 to 100
    }

    return (std::max)(15.0f, (std::min)(100.0f, result));
}

// Map rayTracingRange (0-100) to RadiusPx (ray marching max distance)
// Default RadiusPx was 48.0, so 50% -> 48
// 0% -> 16, 50% -> 48, 100% -> 128
static float MapRangeToRadiusPx(int rangePercent)
{
    // Clamp input
    float t = static_cast<float>((std::max)(0, (std::min)(100, rangePercent))) / 100.0f;

    // Piecewise linear: 16 to 48 for t<=0.5, then 48 to 128 for t>0.5
    float result;
    if (t <= 0.5f)
    {
        result = 16.0f + (48.0f - 16.0f) * (t / 0.5f);  // 16 to 48
    }
    else
    {
        result = 48.0f + (128.0f - 48.0f) * ((t - 0.5f) / 0.5f);  // 48 to 128
    }

    return (std::max)(16.0f, (std::min)(128.0f, result));
}

// Map occlusionStrength (0-100) to AoStrength
// 50% -> 0.8 (original default)
// 0% -> 0, 50% -> 0.8, 100% -> 1.6
static float MapOcclusionStrength(int strengthPercent)
{
    float t = static_cast<float>((std::max)(0, (std::min)(100, strengthPercent))) / 100.0f;
    return t * 1.6f;
}

// Map illuminationStrength (0-100) to GiStrength
// Default was 1.5, so 50% -> 1.5
// 0% -> 0, 50% -> 1.5, 100% -> 3.0
static float MapIlluminationStrength(int strengthPercent)
{
    float t = static_cast<float>((std::max)(0, (std::min)(100, strengthPercent))) / 100.0f;
    return t * 3.0f;
}

// =============================================================================
// Public Configuration API
// =============================================================================

namespace SsrtgiListener
{
    void SetCsoDirectory(const std::wstring& path)
    {
        g_CsoDirectory = path;
        if (!g_CsoDirectory.empty() && g_CsoDirectory.back() != L'\\' && g_CsoDirectory.back() != L'/')
            g_CsoDirectory += L'\\';
    }

    const std::wstring& GetCsoDirectory()
    {
        return g_CsoDirectory;
    }

    void SetHotReloadEnabled(bool enabled)
    {
        g_HotReloadEnabled = enabled;
    }

    bool IsHotReloadEnabled()
    {
        return g_HotReloadEnabled;
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

#ifdef SSRTGI_USE_EXTERNAL_CSO
static std::vector<uint8_t> LoadFileToBytes(const std::wstring& filename)
{
    std::wstring fullPath = g_CsoDirectory + filename;

    std::ifstream f(fullPath, std::ios::binary | std::ios::ate);
    if (!f)
    {
        f.open(filename, std::ios::binary | std::ios::ate);
        if (!f)
            throw std::runtime_error("LoadFileToBytes: cannot open file");
    }

    const std::streamsize size = f.tellg();
    if (size <= 0)
        return {};

    std::vector<uint8_t> data(static_cast<size_t>(size));
    f.seekg(0, std::ios::beg);

    if (!f.read(reinterpret_cast<char*>(data.data()), size))
        throw std::runtime_error("LoadFileToBytes: read failed");

    return data;
}
#endif

static bool GetRes(NVSDK_NGX_Parameter* params, const char* key, ID3D12Resource** out)
{
    if (!params || !key || !out) return false;
    *out = nullptr;

    void* raw = nullptr;
    NVSDK_NGX_Result r = params->Get(key, &raw);
    if (r != NVSDK_NGX_Result_Success || !raw)
        return false;

    *out = static_cast<ID3D12Resource*>(raw);
    return true;
}

static bool GetFloat(NVSDK_NGX_Parameter* params, const char* key, float* out)
{
    if (!params || !key || !out) return false;
    *out = 0.0f;
    return params->Get(key, out) == NVSDK_NGX_Result_Success;
}

// Resource pickers for DLSS
static ID3D12Resource* PickDlssOnlyMotionVectors(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "MotionVectors", &r) && r) return r;
    if (GetRes(params, NVSDK_NGX_Parameter_MotionVectors, &r) && r) return r;
    return nullptr;
}

static ID3D12Resource* PickDlssOnlyColor(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "Color", &r) && r) return r;
    if (GetRes(params, NVSDK_NGX_Parameter_Color, &r) && r) return r;
    return nullptr;
}

static ID3D12Resource* PickDlssOnlyDepth(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "Depth", &r) && r) return r;
    if (GetRes(params, NVSDK_NGX_Parameter_Depth, &r) && r) return r;
    return nullptr;
}

// =============================================================================
// Event Handlers
// =============================================================================

static void OnPostCreateD3D12(
    ID3D12GraphicsCommandList* cmdList,
    NVSDK_NGX_Feature featureId,
    NVSDK_NGX_Parameter* params,
    NVSDK_NGX_Handle* handle,
    NVSDK_NGX_Result result)
{
    // Only handle SuperSampling (filter is already applied, but double-check)
    if (featureId != NVSDK_NGX_Feature_SuperSampling)
        return;

    ID3D12Device* dev = nullptr;
    cmdList->GetDevice(IID_PPV_ARGS(&dev));

    auto ssrtgiCtx = std::make_unique<SsrtgiPostProcessD3D12>();

    try {
#ifdef SSRTGI_USE_EXTERNAL_CSO
        // Load shaders from external CSO files
        auto gather = LoadFileToBytes(L"SsrtgiGatherHZB.cso");
        auto denoise = LoadFileToBytes(L"SsrtgiDenoise.cso");
        auto temporal = LoadFileToBytes(L"SsrtgiTemporal.cso");
        auto comp = LoadFileToBytes(L"SsrtgiComposite.cso");
        auto copyPass = LoadFileToBytes(L"SsrtgiCopyPass.cso");
        auto hzbGen = LoadFileToBytes(L"SsrtgiHzbGenerate.cso");
        auto depthMinMax = LoadFileToBytes(L"SsrtgiDepthMinMax.cso");

        const uint8_t* gatherData = gather.data();
        size_t gatherSize = gather.size();
        const uint8_t* denoiseData = denoise.data();
        size_t denoiseSize = denoise.size();
        const uint8_t* temporalData = temporal.data();
        size_t temporalSize = temporal.size();
        const uint8_t* compData = comp.data();
        size_t compSize = comp.size();
        const uint8_t* copyPassData = copyPass.data();
        size_t copyPassSize = copyPass.size();
        const uint8_t* hzbGenData = hzbGen.data();
        size_t hzbGenSize = hzbGen.size();
        const uint8_t* depthMinMaxData = depthMinMax.data();
        size_t depthMinMaxSize = depthMinMax.size();

        LOG_DEBUG(L"[SSRTGI] Loaded shaders from external CSO files");
#else
        // Use embedded shader bytecode
        const uint8_t* gatherData = EmbeddedShaders::SsrtgiGatherHZB_cso;
        size_t gatherSize = EmbeddedShaders::SsrtgiGatherHZB_cso_size;
        const uint8_t* denoiseData = EmbeddedShaders::SsrtgiDenoise_cso;
        size_t denoiseSize = EmbeddedShaders::SsrtgiDenoise_cso_size;
        const uint8_t* temporalData = EmbeddedShaders::SsrtgiTemporal_cso;
        size_t temporalSize = EmbeddedShaders::SsrtgiTemporal_cso_size;
        const uint8_t* compData = EmbeddedShaders::SsrtgiComposite_cso;
        size_t compSize = EmbeddedShaders::SsrtgiComposite_cso_size;
        const uint8_t* copyPassData = EmbeddedShaders::SsrtgiCopyPass_cso;
        size_t copyPassSize = EmbeddedShaders::SsrtgiCopyPass_cso_size;
        const uint8_t* hzbGenData = EmbeddedShaders::SsrtgiHzbGenerate_cso;
        size_t hzbGenSize = EmbeddedShaders::SsrtgiHzbGenerate_cso_size;
        const uint8_t* depthMinMaxData = EmbeddedShaders::SsrtgiDepthMinMax_cso;
        size_t depthMinMaxSize = EmbeddedShaders::SsrtgiDepthMinMax_cso_size;

        LOG_DEBUG(L"[SSRTGI] Using embedded shader bytecode");
#endif

        int _featureFlags = 0;
        float depthInverted = -1.0f;
        if (params->Get(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, &_featureFlags) == NVSDK_NGX_Result_Success) {
            depthInverted = (float)((_featureFlags & NVSDK_NGX_DLSS_Feature_Flags_DepthInverted) > 0);
        }

        LOG_DEBUG(L"[SSRTGI] Shader sizes:");
        LOG_DEBUG(L"  Gather: " + std::to_wstring(gatherSize) + L" bytes");
        LOG_DEBUG(L"  Denoise: " + std::to_wstring(denoiseSize) + L" bytes");
        LOG_DEBUG(L"  Temporal: " + std::to_wstring(temporalSize) + L" bytes");
        LOG_DEBUG(L"  Composite: " + std::to_wstring(compSize) + L" bytes");
        LOG_DEBUG(L"  CopyPass: " + std::to_wstring(copyPassSize) + L" bytes");
        LOG_DEBUG(L"  HzbGenerate: " + std::to_wstring(hzbGenSize) + L" bytes");
        LOG_DEBUG(L"  DepthMinMax: " + std::to_wstring(depthMinMaxSize) + L" bytes");
        LOG_DEBUG(L"  Depth Inverted: " + std::to_wstring(depthInverted));
#ifdef SSRTGI_USE_EXTERNAL_CSO
        LOG_DEBUG(L"  Hot-Reload: " + std::wstring(g_HotReloadEnabled ? L"ENABLED" : L"DISABLED"));
#else
        LOG_DEBUG(L"  Hot-Reload: DISABLED (embedded shaders)");
#endif

        if (depthInverted < 0.0f) {
            depthInverted = 0.0f;
        }

        if (gatherSize == 0 || denoiseSize == 0 || temporalSize == 0 ||
            compSize == 0 || copyPassSize == 0 || hzbGenSize == 0)
        {
            LOG_ERROR(L"** FAILED TO LOAD CSO FILES!");
            dev->Release();
            return;
        }

#ifdef SSRTGI_USE_EXTERNAL_CSO
        if (g_HotReloadEnabled)
        {
            ssrtgiCtx->InitWithHotReload(dev,
                g_CsoDirectory,
                gatherData, gatherSize,
                denoiseData, denoiseSize,
                temporalData, temporalSize,
                compData, compSize,
                copyPassData, copyPassSize,
                hzbGenData, hzbGenSize);
        }
        else
        {
            ssrtgiCtx->Init(dev,
                gatherData, gatherSize,
                denoiseData, denoiseSize,
                temporalData, temporalSize,
                compData, compSize,
                copyPassData, copyPassSize,
                hzbGenData, hzbGenSize);
        }
#else
        // Embedded shaders - no hot-reload support
        ssrtgiCtx->Init(dev,
            gatherData, gatherSize,
            denoiseData, denoiseSize,
            temporalData, temporalSize,
            compData, compSize,
            copyPassData, copyPassSize,
            hzbGenData, hzbGenSize);
#endif

        // Ray marching optimized parameters - will be overwritten by UI values in Evaluate
        SsrtgiParams p{};
        p.RadiusPx = MapRangeToRadiusPx(::ctx.ngx.rayTracingRange);
        p.AoStrength = MapOcclusionStrength(::ctx.ngx.occlusionStrength);
        p.GiStrength = MapIlluminationStrength(::ctx.ngx.illuminationStrength);
        p.TemporalAlpha = 0.06f;
        p.DepthReject = 0.002f;
        p.FlipMotionVectors = 0.0f;
        p.DepthInverted = depthInverted;
        p.JitterX = 0.0f;
        p.JitterY = 0.0f;
        p.NumRays = MapQualityToNumRays(::ctx.ngx.rayTracingQuality);
        p.FalloffEnd = MapRangeToFalloffEnd(::ctx.ngx.rayTracingRange);

        // Apply AO/GI enabled flags
        if (!::ctx.ngx.isAmbientOcclusionEnabled)
            p.AoStrength = 0.0f;
        if (!::ctx.ngx.isGlobalIlluminationEnabled)
            p.GiStrength = 0.0f;

        ssrtgiCtx->SetParams(p);

        PostFxRegistry::Register(static_cast<const NVSDK_NGX_Handle*>(handle), std::move(ssrtgiCtx));
        LOG_DEBUG(L"SSRTGI Ray Marching Init complete");
    }
    catch (const std::exception& e) {
        std::string errStr(e.what());
        LOG_ERROR(L"SSRTGI Init exception: " + std::wstring(errStr.begin(), errStr.end()));
    }

    dev->Release();
}

static void OnPreEvaluateD3D12(
    ID3D12GraphicsCommandList* cmdList,
    const NVSDK_NGX_Handle* handle,
    NVSDK_NGX_Parameter* params,
    NVSDK_NGX_Feature featureId)
{
    // Only handle SuperSampling
    if (featureId != NVSDK_NGX_Feature_SuperSampling)
        return;

    // Check if SSRTGI is enabled in UI
    if (!::ctx.ngx.isScreenSpaceRayTracingEnabled)
        return;

    auto* ssrtgiCtx = PostFxRegistry::Find(handle);
    if (!ssrtgiCtx) return;

    // Get resources from DLSS parameters
    ID3D12Resource* color = PickDlssOnlyColor(params);
    ID3D12Resource* depth = PickDlssOnlyDepth(params);
    ID3D12Resource* mv = PickDlssOnlyMotionVectors(params);

    if (!color || !depth || !mv) {
        return;
    }

    // Get jitter
    float jitterX = 0.0f, jitterY = 0.0f;
    GetFloat(params, NVSDK_NGX_Parameter_Jitter_Offset_X, &jitterX);
    GetFloat(params, NVSDK_NGX_Parameter_Jitter_Offset_Y, &jitterY);

    // Get dimensions
    D3D12_RESOURCE_DESC colorDesc = color->GetDesc();
    uint32_t colorW = static_cast<uint32_t>(colorDesc.Width);
    uint32_t colorH = colorDesc.Height;

    // Update params
    SsrtgiParams& p = ssrtgiCtx->GetParams();
    p.Width = colorW;
    p.Height = colorH;
    p.JitterX = jitterX;
    p.JitterY = jitterY;

    // =============================================================================
    // Map UI parameters to SSRTGI params
    // =============================================================================

    // Ray count based on quality setting
    p.NumRays = MapQualityToNumRays(::ctx.ngx.rayTracingQuality);

    // Ray tracing range affects both radius and falloff
    // Adaptive step sizing in shader preserves small details even with large radius
    p.RadiusPx = MapRangeToRadiusPx(::ctx.ngx.rayTracingRange);
    p.FalloffEnd = MapRangeToFalloffEnd(::ctx.ngx.rayTracingRange);

    // AO strength - check if AO is enabled
    if (::ctx.ngx.isAmbientOcclusionEnabled)
    {
        p.AoStrength = MapOcclusionStrength(::ctx.ngx.occlusionStrength);
    }
    else
    {
        p.AoStrength = 0.0f;  // AO disabled
    }

    // GI strength - check if GI is enabled
    if (::ctx.ngx.isGlobalIlluminationEnabled)
    {
        p.GiStrength = MapIlluminationStrength(::ctx.ngx.illuminationStrength);
    }
    else
    {
        p.GiStrength = 0.0f;  // GI disabled
    }

    // Camera params from Streamline-style names
    float fov = 0.0f, cameraNear = 0.0f, cameraFar = 0.0f;
    if (GetFloat(params, "sl.camerafov", &fov) && fov > 0.01f) {
        GetFloat(params, "sl.cameranear", &cameraNear);
        GetFloat(params, "sl.camerafar", &cameraFar);
    }

    // Fallback
    if (fov < 0.01f) fov = 1.0472f;
    if (cameraNear <= 0.0f) cameraNear = 0.1f;
    if (cameraFar <= cameraNear) cameraFar = 1000.0f;

    p.CameraFOV = fov;
    p.CameraNear = cameraNear;
    p.CameraFar = cameraFar;
    p.CameraAspectRatio = (colorH > 0) ? static_cast<float>(colorW) / static_cast<float>(colorH) : 1.778f;

    // MV scale
    float mvScaleX = 1.0f, mvScaleY = 1.0f;
    GetFloat(params, "MV.Scale.X", &mvScaleX);
    GetFloat(params, "MV.Scale.Y", &mvScaleY);
    if (mvScaleX == 0.0f) mvScaleX = 1.0f;
    if (mvScaleY == 0.0f) mvScaleY = 1.0f;
    p.MVScaleX = mvScaleX;
    p.MVScaleY = mvScaleY;

    try {
        ssrtgiCtx->Execute(cmdList, color, depth, mv, false);
    }
    catch (const std::exception& e) {
        std::string errStr(e.what());
        LOG_ERROR(L"EXCEPTION in SSRTGI Execute: " + std::wstring(errStr.begin(), errStr.end()));
    }
}

static void OnPreReleaseD3D12(
    NVSDK_NGX_Handle* handle,
    NVSDK_NGX_Feature featureId)
{
    // Only handle SuperSampling
    if (featureId != NVSDK_NGX_Feature_SuperSampling)
        return;

    if (!handle) {
        LOG_ERROR(L"SSRTGI DestroyFeature: Invalid handle (null)");
        return;
    }

    auto* ctx = PostFxRegistry::Find(handle);
    if (!ctx) {
        LOG_DEBUG(L"SSRTGI DestroyFeature: Handle not found in registry");
        return;
    }

    LOG_DEBUG(L"SSRTGI DestroyFeature: Releasing resources for handle");
    PostFxRegistry::Unregister(handle);
    LOG_DEBUG(L"SSRTGI DestroyFeature: Cleanup complete");
}

// =============================================================================
// Registration Function - Call this at startup
// =============================================================================

namespace SsrtgiListener
{
    void Register()
    {
        LOG_INFO(L"[SSRTGI] Registering NGX feature event listeners");
#ifdef SSRTGI_USE_EXTERNAL_CSO
        LOG_INFO(L"[SSRTGI] Shader mode: External CSO files");
#else
        LOG_INFO(L"[SSRTGI] Shader mode: Embedded bytecode");
#endif

        // Register for D3D12 SuperSampling events
        NgxFeatureEvents::RegisterPostCreateD3D12(
            OnPostCreateD3D12,
            NVSDK_NGX_Feature_SuperSampling
        );

        NgxFeatureEvents::RegisterPreEvaluateD3D12(
            OnPreEvaluateD3D12,
            NVSDK_NGX_Feature_SuperSampling
        );

        NgxFeatureEvents::RegisterPreReleaseD3D12(
            OnPreReleaseD3D12,
            NVSDK_NGX_Feature_SuperSampling
        );

        LOG_INFO(L"[SSRTGI] Event listeners registered successfully");
    }

    void Unregister()
    {
        // Note: Currently NgxFeatureEvents doesn't support unregistering individual listeners
        // This would clear ALL listeners, so use with caution
        // NgxFeatureEvents::ClearAllListeners();
    }
}