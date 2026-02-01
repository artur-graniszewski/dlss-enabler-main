#pragma once
#include <memory>
#include "SsrtgiPostProcessD3D12.h"
#include "../Includes/dlss/nvsdk_ngx_params.h"
#include "PostFxRegistry.h"
#include <vector>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include "Common.h"

/*
// ============================================================

// Configurable CSO directory path
// Default path for development - can be changed at runtime
// NOTE: Use SetCsoDirectory() to set the correct path at runtime if needed
static std::wstring g_CsoDirectory = L"C:\\Users\\Admin\\Desktop\\Stary pulpit\\DLSS\\DLSS Enabler - master \u2014 2.90.810.0 \u2014 kopia\\x64\\Debug\\";

// Hot-reload enable flag (can be toggled at runtime)
static bool g_HotReloadEnabled = true;

inline void SetCsoDirectory(const std::wstring& path)
{
    g_CsoDirectory = path;
    // Ensure path ends with backslash
    if (!g_CsoDirectory.empty() && g_CsoDirectory.back() != L'\\' && g_CsoDirectory.back() != L'/')
        g_CsoDirectory += L'\\';
}

inline const std::wstring& GetCsoDirectory()
{
    return g_CsoDirectory;
}

inline void SetHotReloadEnabled(bool enabled)
{
    g_HotReloadEnabled = enabled;
}

inline bool IsHotReloadEnabled()
{
    return g_HotReloadEnabled;
}

inline void RegisterPostFxForHandle(NVSDK_NGX_Handle* h, std::unique_ptr<SsrtgiPostProcessD3D12> ctx)
{
    PostFxRegistry::Register(h, std::move(ctx));
}

inline std::vector<uint8_t> LoadFileToBytes(const std::wstring& filename)
{
    // Combine directory with filename
    std::wstring fullPath = g_CsoDirectory + filename;

    std::ifstream f(fullPath, std::ios::binary | std::ios::ate);
    if (!f)
    {
        // Try current directory as fallback
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

void Hook_CreateFeature_D3D12(
    NVSDK_NGX_Parameter* params,
    ID3D12GraphicsCommandList* cl,
    NVSDK_NGX_Handle* outHandle)
{
    ID3D12Device* dev = nullptr;
    cl->GetDevice(IID_PPV_ARGS(&dev));

    auto ctx = std::make_unique<SsrtgiPostProcessD3D12>();

    auto gather = LoadFileToBytes(L"SsrtgiGatherHZB.cso");  // Use HZB version
    auto denoise = LoadFileToBytes(L"SsrtgiDenoise.cso");
    auto temporal = LoadFileToBytes(L"SsrtgiTemporal.cso");
    auto comp = LoadFileToBytes(L"SsrtgiComposite.cso");
    auto copyPass = LoadFileToBytes(L"SsrtgiCopyPass.cso");
    auto hzbGen = LoadFileToBytes(L"SsrtgiHzbGenerate.cso");
    auto depthMinMax = LoadFileToBytes(L"SsrtgiDepthMinMax.cso");

    int _featureFlags = 0;
    float depthInverted = -1.0f;
    if (params->Get(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, &_featureFlags) == NVSDK_NGX_Result_Success) {
        depthInverted = (float)((_featureFlags & NVSDK_NGX_DLSS_Feature_Flags_DepthInverted) > 0);
    }

    LOG_DEBUG(L"SSRTGI Ray Marching Init - Loaded CSO files:");
    LOG_DEBUG(L"  GatherHZB: " + std::to_wstring(gather.size()) + L" bytes");
    LOG_DEBUG(L"  Denoise: " + std::to_wstring(denoise.size()) + L" bytes");
    LOG_DEBUG(L"  Temporal: " + std::to_wstring(temporal.size()) + L" bytes");
    LOG_DEBUG(L"  Composite: " + std::to_wstring(comp.size()) + L" bytes");
    LOG_DEBUG(L"  CopyPass: " + std::to_wstring(copyPass.size()) + L" bytes");
    LOG_DEBUG(L"  HzbGenerate: " + std::to_wstring(hzbGen.size()) + L" bytes");
    LOG_DEBUG(L"  DepthMinMax: " + std::to_wstring(depthMinMax.size()) + L" bytes");
    LOG_DEBUG(L"  Depth Inverted: " + std::to_wstring(depthInverted));
    LOG_DEBUG(L"  Hot-Reload: " + std::wstring(g_HotReloadEnabled ? L"ENABLED" : L"DISABLED"));
    if (depthInverted < 0.0f) {
        depthInverted = 0.0f;
    }

    if (gather.empty() || denoise.empty() || temporal.empty() || comp.empty() || copyPass.empty() || hzbGen.empty())
    {
        LOG_ERROR(L"** FAILED TO LOAD CSO FILES!");
        return;
    }

    // ============================================================
    // Use InitWithHotReload to enable shader hot-reload
    // ============================================================
    if (g_HotReloadEnabled)
    {
        ctx->InitWithHotReload(dev,
            g_CsoDirectory,
            gather.data(), gather.size(),
            denoise.data(), denoise.size(),
            temporal.data(), temporal.size(),
            comp.data(), comp.size(),
            copyPass.data(), copyPass.size(),
            hzbGen.data(), hzbGen.size());

        // Optionally set hot-reload interval (default is 60 frames)
        // ctx->SetHotReloadEnabled(true);  // Already enabled by InitWithHotReload
    }
    else
    {
        ctx->Init(dev,
            gather.data(), gather.size(),
            denoise.data(), denoise.size(),
            temporal.data(), temporal.size(),
            comp.data(), comp.size(),
            copyPass.data(), copyPass.size(),
            hzbGen.data(), hzbGen.size());
    }

    // Ray marching optimized parameters
    SsrtgiParams p{};
    p.RadiusPx = 32.0f;
    p.AoStrength = 0.8f;        // Moderate AO
    p.GiStrength = 1.5f;        // Moderate GI
    p.TemporalAlpha = 0.06f;    // Heavy temporal accumulation
    p.DepthReject = 0.002f;
    p.FlipMotionVectors = 0.0f;
    p.DepthInverted = depthInverted;
    p.JitterX = 0.0f;
    p.JitterY = 0.0f;
    ctx->SetParams(p);

    PostFxRegistry::Register(static_cast<const NVSDK_NGX_Handle*>(outHandle), std::move(ctx));
    dev->Release();

    LOG_DEBUG(L"SSRTGI Ray Marching Init complete");
}

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

// ============================================================
// DLSSG-only resource getters (no fallback)
// ============================================================
static ID3D12Resource* PickDlssgMotionVectors(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "DLSSG.MVecs", &r) && r)
        return r;
    return nullptr;
}

static ID3D12Resource* PickDlssgColor(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "DLSSG.Backbuffer", &r) && r)
        return r;
    if (GetRes(params, "DLSSG.OutputReal", &r) && r)
        return r;
    return nullptr;
}

static ID3D12Resource* PickDlssgDepth(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "DLSSG.Depth", &r) && r)
        return r;
    return nullptr;
}

// ============================================================
// DLSS-only resource getters (no fallback to DLSSG)
// ============================================================
static ID3D12Resource* PickDlssOnlyMotionVectors(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "MotionVectors", &r) && r)
        return r;
    if (GetRes(params, "DLSS.MotionVectors", &r) && r)
        return r;
    return nullptr;
}

static ID3D12Resource* PickDlssOnlyColor(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "Color", &r) && r)
        return r;
    if (GetRes(params, "DLSS.Output", &r) && r)
        return r;
    return nullptr;
}

static ID3D12Resource* PickDlssOnlyDepth(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;
    if (GetRes(params, "Depth", &r) && r)
        return r;
    return nullptr;
}

// ============================================================
// Legacy functions with fallback (for compatibility)
// ============================================================
static ID3D12Resource* PickDlssMotionVectors(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;

    // Try DLSSG first
    if (GetRes(params, "DLSSG.MVecs", &r) && r)
        return r;

    // Fallback to DLSS
    if (GetRes(params, "MotionVectors", &r) && r)
        return r;

    if (GetRes(params, "DLSS.MotionVectors", &r) && r)
        return r;

    return nullptr;
}

static ID3D12Resource* PickDlssColor(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;

    // Try DLSSG first
    if (GetRes(params, "DLSSG.Backbuffer", &r) && r)
        return r;

    // Fallback to DLSS
    if (GetRes(params, "Color", &r) && r)
        return r;

    if (GetRes(params, "DLSS.Output", &r) && r)
        return r;

    if (GetRes(params, "DLSSG.OutputReal", &r) && r)
        return r;

    return nullptr;
}

static ID3D12Resource* PickDlssDepth(NVSDK_NGX_Parameter* params)
{
    ID3D12Resource* r = nullptr;

    // Try DLSSG first
    if (GetRes(params, "DLSSG.Depth", &r) && r)
        return r;

    // Fallback to DLSS
    if (GetRes(params, "Depth", &r) && r)
        return r;

    return nullptr;
}

HRESULT Hook_EvaluateFeature_D3D12(
    const NVSDK_NGX_Handle* featureHandle,
    NVSDK_NGX_Parameter* params,
    ID3D12GraphicsCommandList* cl,
    bool isDlssg = false)  // NEW: explicit DLSSG flag
{
    static int frameCount = 0;
    static bool jitterLogged = false;
    static bool cameraLogged = false;
    frameCount++;

    if (!featureHandle || !params || !cl) {
        LOG_ERROR(L"Invalid parameters in hook");
        return E_INVALIDARG;
    }

    auto* ctx = PostFxRegistry::Find(featureHandle);
    if (!ctx) {
        // Not an error - this handle might not have SSRTGI registered
        return S_OK;
    }

    // ============================================================
    // Use separate resource getters based on isDlssg flag
    // NO FALLBACK between DLSS and DLSSG resources!
    // ============================================================
    ID3D12Resource* depth = nullptr;
    ID3D12Resource* color = nullptr;
    ID3D12Resource* mv = nullptr;

    if (isDlssg) {
        depth = PickDlssgDepth(params);
        color = PickDlssgColor(params);
        mv = PickDlssgMotionVectors(params);

        if (!depth || !color) {
            LOG_DEBUG(L"[SSRTGI] DLSSG mode but missing DLSSG resources - skipping");
            return S_OK;
        }
    }
    else {
        depth = PickDlssOnlyDepth(params);
        color = PickDlssOnlyColor(params);
        mv = PickDlssOnlyMotionVectors(params);

        if (!depth || !color) {
            LOG_DEBUG(L"[SSRTGI] DLSS mode but missing DLSS resources - skipping");
            return S_OK;
        }
    }

    // Log which mode we're using (once)
    static bool modeLogged = false;
    static bool lastIsDlssg = false;
    if (!modeLogged || lastIsDlssg != isDlssg) {
        LOG_DEBUG(L"[SSRTGI] Running in " + std::wstring(isDlssg ? L"DLSSG" : L"DLSS") + L" mode");
        modeLogged = true;
        lastIsDlssg = isDlssg;
    }

    // =====================================================
    // DEBUG: LOG RESOURCE DIMENSIONS (once per resolution change)
    // =====================================================
    static uint32_t lastLoggedColorW = 0, lastLoggedColorH = 0;
    static uint32_t lastLoggedDepthW = 0, lastLoggedDepthH = 0;

    auto colorDesc = color->GetDesc();
    auto depthDesc = depth->GetDesc();

    uint32_t colorW = (uint32_t)colorDesc.Width;
    uint32_t colorH = (uint32_t)colorDesc.Height;
    uint32_t depthW = (uint32_t)depthDesc.Width;
    uint32_t depthH = (uint32_t)depthDesc.Height;

    if (colorW != lastLoggedColorW || colorH != lastLoggedColorH ||
        depthW != lastLoggedDepthW || depthH != lastLoggedDepthH)
    {
        lastLoggedColorW = colorW;
        lastLoggedColorH = colorH;
        lastLoggedDepthW = depthW;
        lastLoggedDepthH = depthH;

        LOG_DEBUG(L"=== SSRTGI Resource Dimensions ===");
        LOG_DEBUG(L"  Color: " + std::to_wstring(colorW) + L" x " + std::to_wstring(colorH));
        LOG_DEBUG(L"  Depth: " + std::to_wstring(depthW) + L" x " + std::to_wstring(depthH));

        if (mv) {
            auto mvDesc = mv->GetDesc();
            LOG_DEBUG(L"  MotionVectors: " + std::to_wstring((uint32_t)mvDesc.Width) +
                L" x " + std::to_wstring((uint32_t)mvDesc.Height));
        }
        else {
            LOG_DEBUG(L"  MotionVectors: NULL");
        }

        // Check for mismatch
        if (colorW != depthW || colorH != depthH) {
            LOG_DEBUG(L"  *** MISMATCH DETECTED! Color != Depth ***");
            LOG_DEBUG(L"  Ratio: " + std::to_wstring((float)depthW / colorW) +
                L" x " + std::to_wstring((float)depthH / colorH));
        }

        // Also log DLSS render size parameters if available
        unsigned int renderW = 0, renderH = 0;
        unsigned int outputW = 0, outputH = 0;
        if (params->Get("Width", &renderW) == NVSDK_NGX_Result_Success) {
            LOG_DEBUG(L"  DLSS 'Width' param: " + std::to_wstring(renderW));
        }
        if (params->Get("Height", &renderH) == NVSDK_NGX_Result_Success) {
            LOG_DEBUG(L"  DLSS 'Height' param: " + std::to_wstring(renderH));
        }
        if (params->Get("OutWidth", &outputW) == NVSDK_NGX_Result_Success) {
            LOG_DEBUG(L"  DLSS 'OutWidth' param: " + std::to_wstring(outputW));
        }
        if (params->Get("OutHeight", &outputH) == NVSDK_NGX_Result_Success) {
            LOG_DEBUG(L"  DLSS 'OutHeight' param: " + std::to_wstring(outputH));
        }

        // Log aspect ratios
        float colorAspect = (colorH > 0) ? (float)colorW / (float)colorH : 0;
        float outputAspect = (outputH > 0) ? (float)outputW / (float)outputH : 0;
        LOG_DEBUG(L"  Color aspect: " + std::to_wstring(colorAspect));
        LOG_DEBUG(L"  Output aspect: " + std::to_wstring(outputAspect));

        // Log scale factors
        if (outputW > 0 && outputH > 0) {
            float scaleX = (float)colorW / (float)outputW;
            float scaleY = (float)colorH / (float)outputH;
            LOG_DEBUG(L"  Scale (render/output): X=" + std::to_wstring(scaleX) +
                L", Y=" + std::to_wstring(scaleY));
        }

        // Log p.Width/Height vs actual dimensions
        LOG_DEBUG(L"  p.Width/Height (params): " + std::to_wstring(ctx->GetParams().Width) +
            L" x " + std::to_wstring(ctx->GetParams().Height));
        LOG_DEBUG(L"  Aspect used: " + std::to_wstring((float)colorW / (float)colorH));

        // Log ALL camera params that will be sent to shader
        SsrtgiParams& params_ref = ctx->GetParams();
        LOG_DEBUG(L"  === Shader Params ===");
        LOG_DEBUG(L"  CameraFOV: " + std::to_wstring(params_ref.CameraFOV));
        LOG_DEBUG(L"  CameraNear: " + std::to_wstring(params_ref.CameraNear));
        LOG_DEBUG(L"  CameraFar: " + std::to_wstring(params_ref.CameraFar));
        LOG_DEBUG(L"  CameraAspectRatio: " + std::to_wstring(params_ref.CameraAspectRatio));
        LOG_DEBUG(L"  DepthInverted: " + std::to_wstring(params_ref.DepthInverted));
        LOG_DEBUG(L"  MVScaleX: " + std::to_wstring(params_ref.MVScaleX));
        LOG_DEBUG(L"  MVScaleY: " + std::to_wstring(params_ref.MVScaleY));

        LOG_DEBUG(L"=================================");
    }

    // =====================================================
    // READ JITTER - DLSSG first, then DLSS fallback
    // =====================================================
    float jitterX = 0.0f;
    float jitterY = 0.0f;

    // Try DLSSG first
    if (!GetFloat(params, "DLSSG.JitterOffsetX", &jitterX)) {
        // Fallback to DLSS
        if (!GetFloat(params, "Jitter.Offset.X", &jitterX)) {
            GetFloat(params, "DLSS.Jitter.Offset.X", &jitterX);
        }
    }
    if (!GetFloat(params, "DLSSG.JitterOffsetY", &jitterY)) {
        // Fallback to DLSS
        if (!GetFloat(params, "Jitter.Offset.Y", &jitterY)) {
            GetFloat(params, "DLSS.Jitter.Offset.Y", &jitterY);
        }
    }

    if (!jitterLogged && (jitterX != 0.0f || jitterY != 0.0f)) {
        LOG_DEBUG(L"Jitter detected: X=" + std::to_wstring(jitterX) +
            L", Y=" + std::to_wstring(jitterY));
        jitterLogged = true;
    }

    // =====================================================
    // CAMERA PARAMETERS - 3-LEVEL FALLBACK
    // Level 1: DLSSG params (Frame Generation) - also caches them
    // Level 2: Cached params from previous DLSSG session
    // Level 3: Streamline-style params
    // Level 4: Hardcoded defaults
    // =====================================================

    SsrtgiParams& p = ctx->GetParams();

    float fov = 0.0f;
    float cameraNear = 0.0f;
    float cameraFar = 0.0f;

    // TEMP DEBUG: Hardcode aspect ratio to test if this is the issue
    float aspect = 1.813031f;  // Ultra Performance aspect
    // float aspect = (colorH > 0)
    //     ? static_cast<float>(colorW) / static_cast<float>(colorH)
    //     : 1.778f;

    bool gotCameraParams = false;
    // Note: using isDlssg flag (passed as parameter) instead of detecting from params

    // Get camera params based on isDlssg flag
    if (isDlssg) {
        if (GetFloat(params, "DLSSG.CameraFOV", &fov) && fov > 0.01f) {
            GetFloat(params, "DLSSG.CameraNear", &cameraNear);
            GetFloat(params, "DLSSG.CameraFar", &cameraFar);
            gotCameraParams = true;

            if (!cameraLogged) {
                LOG_DEBUG(L"Camera params from DLSSG: FOV=" + std::to_wstring(fov) +
                    L" Near=" + std::to_wstring(cameraNear) +
                    L" Far=" + std::to_wstring(cameraFar));
            }
        }
    }
    else {
        // DLSS mode - try Streamline-style parameter names
        if (GetFloat(params, "sl.camerafov", &fov) && fov > 0.01f) {
            GetFloat(params, "sl.cameranear", &cameraNear);
            GetFloat(params, "sl.camerafar", &cameraFar);
            gotCameraParams = true;

            if (!cameraLogged) {
                LOG_DEBUG(L"Camera params from Streamline: FOV=" + std::to_wstring(fov));
            }
        }
    }

    // LEVEL 3: Hardcoded fallback if nothing else available
    if (!gotCameraParams || fov < 0.01f) {
        // 60 degrees vertical FOV - matches typical DLSSG reports
        fov = 1.0472f; // 60 degrees in radians

        // Conservative near/far that works for most games
        cameraNear = 0.1f;
        cameraFar = 200.0f;  // Matches typical DLSSG Far value

        if (!cameraLogged) {
            LOG_DEBUG(L"Camera params FALLBACK (hardcoded): FOV=60deg Near=0.1 Far=200");
        }
    }

    // Validate and apply hardcoded fallbacks for invalid values
    if (fov < 0.1f || fov > 3.14159f) {
        fov = 1.0472f; // 60 degrees
    }
    if (cameraNear <= 0.0f) {
        cameraNear = 0.1f;
    }
    if (cameraFar <= cameraNear) {
        cameraFar = 1000.0f;
    }
    if (aspect < 0.5f || aspect > 4.0f) {
        aspect = 1.778f;
    }

    if (!cameraLogged) {
        LOG_DEBUG(L"Final camera: FOV=" + std::to_wstring(fov) +
            L" Near=" + std::to_wstring(cameraNear) +
            L" Far=" + std::to_wstring(cameraFar) +
            L" Aspect=" + std::to_wstring(aspect));
        cameraLogged = true;
    }

    // =====================================================
    // MOTION VECTOR SCALE - from DLSS params
    // =====================================================
    float mvScaleX = 1.0f;
    float mvScaleY = 1.0f;

    if (isDlssg) {
        GetFloat(params, "DLSSG.MvecScaleX", &mvScaleX);
    }
    else {
        GetFloat(params, "MV.Scale.X", &mvScaleX);
    }
    if (isDlssg) {
        GetFloat(params, "DLSSG.MvecScaleY", &mvScaleY);
    }
    else {
        GetFloat(params, "MV.Scale.Y", &mvScaleY);
    }

    // If scale is ~1/resolution, MV are in pixels
    // If scale is 1, MV are already in UV
    // Log once for debugging
    static bool mvScaleLogged = false;
    int tick = 1;
    if (!mvScaleLogged && tick %1000 == 0 ) {
        LOG_DEBUG(L"MV Scale: X=" + std::to_wstring(mvScaleX) +
            L", Y=" + std::to_wstring(mvScaleY));
        //tick++;
        //mvScaleLogged = true;
    }

    // Fallback: if scale not provided, assume MV are in pixels
    // (most common case) and calculate scale ourselves
    if (mvScaleX == 0.0f) mvScaleX = 1.0f;
    if (mvScaleY == 0.0f) mvScaleY = 1.0f;

    // =====================================================
    // DEPTH INVERTED - DLSSG first, then DLSS bitmask fallback
    // =====================================================
    float depthInverted = p.DepthInverted;  // Keep existing value as fallback

    // Try DLSSG first
    float dlssgDepthInverted = 0.0f;
    if (GetFloat(params, "DLSSG.DepthInverted", &dlssgDepthInverted)) {
        depthInverted = dlssgDepthInverted;
    }
    // Note: DLSS bitmask fallback is already set during Init

    // Update all params
    p.Width = colorW;
    p.Height = colorH;
    p.JitterX = jitterX;
    p.JitterY = jitterY;
    p.CameraFOV = fov;
    p.CameraNear = cameraNear;
    p.CameraFar = cameraFar;
    p.CameraAspectRatio = aspect;
    p.MVScaleX = mvScaleX;
    p.MVScaleY = mvScaleY;
    p.DepthInverted = depthInverted;

    // =====================================================
    // DEPTH REMAPPING - Enable when NOT in DLSSG mode
    // MinMax shader detects actual depth range (ignoring sky)
    // =====================================================
    if (!isDlssg) {
        //p.DepthRemapEnabled = 1.0f;
        // Min/Max will be set by ComputeDepthMinMax() from shader results
        // Initial values don't matter - they'll be overwritten

        static bool depthRemapLogged = false;
        if (!depthRemapLogged) {
            LOG_DEBUG(L"DLSS mode: Depth remap enabled (dynamic min/max from shader)");
            depthRemapLogged = true;
        }
    }
    else {
        //p.DepthRemapEnabled = 0.0f;
    }

    // Log actual params being sent to shader (after all updates)
    static uint32_t lastLoggedW2 = 0, lastLoggedH2 = 0;
    if (colorW != lastLoggedW2 || colorH != lastLoggedH2) {
        lastLoggedW2 = colorW;
        lastLoggedH2 = colorH;
        LOG_DEBUG(L"=== ACTUAL Shader Params (after update) ===");
        LOG_DEBUG(L"  isDlssg: " + std::to_wstring(isDlssg ? 1 : 0));
        //LOG_DEBUG(L"  DepthRemapEnabled: " + std::to_wstring(p.DepthRemapEnabled));
        LOG_DEBUG(L"  CameraFOV: " + std::to_wstring(p.CameraFOV));
        LOG_DEBUG(L"  CameraNear: " + std::to_wstring(p.CameraNear));
        LOG_DEBUG(L"  CameraFar: " + std::to_wstring(p.CameraFar));
        LOG_DEBUG(L"  CameraAspectRatio: " + std::to_wstring(p.CameraAspectRatio));
        LOG_DEBUG(L"  DepthInverted: " + std::to_wstring(p.DepthInverted));
        LOG_DEBUG(L"============================================");
    }

    try {
        ctx->Execute(cl, color, depth, mv, isDlssg);
    }
    catch (const std::exception& e) {
        std::string errStr(e.what());
        LOG_ERROR(L"EXCEPTION in Execute: " + std::wstring(errStr.begin(), errStr.end()));
    }
    catch (...) {
        LOG_ERROR(L"UNKNOWN EXCEPTION in Execute");
    }

    return S_OK;
}

HRESULT Hook_DestroyFeature_D3D12(const NVSDK_NGX_Handle* featureHandle)
{
    if (!featureHandle) {
        LOG_ERROR(L"DestroyFeature: Invalid handle (null)");
        return E_INVALIDARG;
    }

    auto* ctx = PostFxRegistry::Find(featureHandle);
    if (!ctx) {
        LOG_DEBUG(L"DestroyFeature: Handle not found in registry (already destroyed or never created)");
        return S_OK;
    }

    LOG_DEBUG(L"DestroyFeature: Releasing SSRTGI resources for handle");

    // Unregister will delete the unique_ptr, which triggers ~SsrtgiPostProcessD3D12
    // All ComPtr members (rootSig_, PSOs, textures, heap) are automatically released
    PostFxRegistry::Unregister(featureHandle);

    LOG_DEBUG(L"DestroyFeature: SSRTGI cleanup complete");
    return S_OK;
}
*/