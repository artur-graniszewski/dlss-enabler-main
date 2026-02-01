// ============================================================
// SSRTGI POST PROCESS D3D12 - STABILITY FIXED VERSION
// ============================================================
// Fixes applied:
// 1. Descriptor heap synchronization with proper barriers
// 2. Pre-allocated descriptors instead of per-frame creation
// 3. Verbose skip logging with counters
// 4. Frame-index based ping-pong (not toggle based)
// 5. Robust resource state tracking with validation
// ============================================================

#include "SsrtgiPostProcessD3D12.h"
#include "Common.h"
#include <fstream>

// ============================================================
// TEMPORAL PASS TOGGLE
// ============================================================
#define SSRTGI_ENABLE_TEMPORAL

// Global instance counter for debugging
static int g_ssrtgiInstanceCount = 0;
static int g_ssrtgiTotalCreated = 0;

// ============================================================
// STABILITY STATISTICS (for debugging frame skips)
// ============================================================
static uint64_t g_totalExecuteCalls = 0;
static uint64_t g_skippedFrames_NullDevice = 0;
static uint64_t g_skippedFrames_NullCL = 0;
static uint64_t g_skippedFrames_NullResources = 0;
static uint64_t g_skippedFrames_InternalNull = 0;

// ============================================================
// HOT-RELOAD HELPER: Load file to bytes
// ============================================================
static std::vector<uint8_t> LoadFileToBytes(const std::wstring& fullPath)
{
    std::ifstream f(fullPath, std::ios::binary | std::ios::ate);
    if (!f)
        return {};

    const std::streamsize size = f.tellg();
    if (size <= 0)
        return {};

    std::vector<uint8_t> data(static_cast<size_t>(size));
    f.seekg(0, std::ios::beg);

    if (!f.read(reinterpret_cast<char*>(data.data()), size))
        return {};

    return data;
}

// Destructor
SsrtgiPostProcessD3D12::~SsrtgiPostProcessD3D12()
{
    g_ssrtgiInstanceCount--;
    LOG_DEBUG(L"[SSRTGI] Destructor called - this=" + std::to_wstring((uintptr_t)this) +
        L", remaining instances: " + std::to_wstring(g_ssrtgiInstanceCount));

    // Log final statistics
    LOG_DEBUG(L"[SSRTGI] Final stats: Total=" + std::to_wstring(g_totalExecuteCalls) +
        L" Skipped(device)=" + std::to_wstring(g_skippedFrames_NullDevice) +
        L" Skipped(cl)=" + std::to_wstring(g_skippedFrames_NullCL) +
        L" Skipped(resources)=" + std::to_wstring(g_skippedFrames_NullResources) +
        L" Skipped(internal)=" + std::to_wstring(g_skippedFrames_InternalNull));
}

static constexpr UINT kDescPerPass = 9; // 6 SRV(t0..t5) + 3 UAV(u0..u2)
static constexpr UINT kPassGather = 0;
static constexpr UINT kPassDenoise = 1;
static constexpr UINT kPassTemporal = 2;
static constexpr UINT kPassComposite = 3;
static constexpr UINT kPassCopy = 4;

// ============================================================
// FIX #2: Pre-allocated descriptor slots
// We use frame-indexed descriptor regions to avoid per-frame creation
// ============================================================
static constexpr UINT kNumBufferedFrames = 2;  // Double-buffer descriptors
static constexpr UINT kDescPerFrame = kDescPerPass * 5;  // 5 passes
static constexpr UINT kTotalDescriptors = kDescPerFrame * kNumBufferedFrames + 16; // Extra for HZB

static UINT PassBase(UINT pass, UINT frameSlot)
{
    return frameSlot * kDescPerFrame + pass * kDescPerPass;
}

void SsrtgiPostProcessD3D12::Init(ID3D12Device* device,
    const void* gatherCso, size_t gatherSize,
    const void* denoiseCso, size_t denoiseSize,
    const void* temporalCso, size_t temporalSize,
    const void* compositeCso, size_t compositeSize,
    const void* copyCso, size_t copySize,
    const void* hzbGenCso, size_t hzbGenSize)
{
    g_ssrtgiInstanceCount++;
    g_ssrtgiTotalCreated++;
    LOG_DEBUG(L"[SSRTGI] Init called - instance #" + std::to_wstring(g_ssrtgiTotalCreated) +
        L", active instances: " + std::to_wstring(g_ssrtgiInstanceCount) +
        L", this=" + std::to_wstring((uintptr_t)this));

    device_ = device;
    descSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CreateRootSig();
    CreateHzbRootSig();
    CreatePso(gatherCso, gatherSize, psoGather_);
    CreatePso(denoiseCso, denoiseSize, psoDenoise_);
#ifdef SSRTGI_ENABLE_TEMPORAL
    CreatePso(temporalCso, temporalSize, psoTemporal_);
#else
    (void)temporalCso; (void)temporalSize;
#endif
    CreatePso(compositeCso, compositeSize, psoComposite_);
    CreatePso(copyCso, copySize, psoCopy_);
    CreateHzbPso(hzbGenCso, hzbGenSize);

    // ============================================================
    // FIX #2: Create descriptor heap with space for multiple frames
    // ============================================================
    CreateOrResizeHeap();

#ifdef SSRTGI_ENABLE_TEMPORAL
    LOG_DEBUG(L"[SSRTGI] Init complete (temporal ENABLED)");
#else
    LOG_DEBUG(L"[SSRTGI] Init complete (temporal DISABLED)");
#endif
}

void SsrtgiPostProcessD3D12::InitWithHotReload(ID3D12Device* device,
    const std::wstring& csoDirectory,
    const void* gatherCso, size_t gatherSize,
    const void* denoiseCso, size_t denoiseSize,
    const void* temporalCso, size_t temporalSize,
    const void* compositeCso, size_t compositeSize,
    const void* copyCso, size_t copySize,
    const void* hzbGenCso, size_t hzbGenSize)
{
    csoDirectory_ = csoDirectory;
    if (!csoDirectory_.empty() && csoDirectory_.back() != L'\\' && csoDirectory_.back() != L'/')
        csoDirectory_ += L'\\';

    Init(device, gatherCso, gatherSize, denoiseCso, denoiseSize,
        temporalCso, temporalSize, compositeCso, compositeSize,
        copyCso, copySize, hzbGenCso, hzbGenSize);

    InitShaderFileInfo();
    hotReloadEnabled_ = true;

    LOG_DEBUG(L"[SSRTGI] Hot-reload initialized with directory: " + csoDirectory_);
}

void SsrtgiPostProcessD3D12::InitShaderFileInfo()
{
    shaderGather_.filename = L"SsrtgiGatherHZB.cso";
    shaderDenoise_.filename = L"SsrtgiDenoise.cso";
    shaderTemporal_.filename = L"SsrtgiTemporal.cso";
    shaderComposite_.filename = L"SsrtgiComposite.cso";
    shaderCopy_.filename = L"SsrtgiCopyPass.cso";
    shaderHzbGen_.filename = L"SsrtgiHzbGenerate.cso";

    auto initTimestamp = [this](ShaderFileInfo& info) {
        std::wstring fullPath = csoDirectory_ + info.filename;
        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (GetFileAttributesExW(fullPath.c_str(), GetFileExInfoStandard, &fileInfo)) {
            ULARGE_INTEGER time, size;
            time.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
            time.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;
            size.LowPart = fileInfo.nFileSizeLow;
            size.HighPart = fileInfo.nFileSizeHigh;
            info.lastWriteTime64 = time.QuadPart;
            info.lastSize = size.QuadPart;
            info.valid = true;
        }
        else {
            info.valid = false;
        }
        };

    initTimestamp(shaderGather_);
    initTimestamp(shaderDenoise_);
    initTimestamp(shaderTemporal_);
    initTimestamp(shaderComposite_);
    initTimestamp(shaderCopy_);
    initTimestamp(shaderHzbGen_);
}

bool SsrtgiPostProcessD3D12::CheckShaderTimestamp(ShaderFileInfo& info)
{
    if (csoDirectory_.empty() || info.filename.empty())
        return false;

    std::wstring fullPath = csoDirectory_ + info.filename;
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExW(fullPath.c_str(), GetFileExInfoStandard, &fileInfo)) {
        if (info.valid) {
            info.valid = false;
        }
        return false;
    }

    ULARGE_INTEGER currentTime, currentSize;
    currentTime.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
    currentTime.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;
    currentSize.LowPart = fileInfo.nFileSizeLow;
    currentSize.HighPart = fileInfo.nFileSizeHigh;

    if (!info.valid) {
        info.lastWriteTime64 = currentTime.QuadPart;
        info.lastSize = currentSize.QuadPart;
        info.valid = true;
        return false;
    }

    bool changed = (currentTime.QuadPart != info.lastWriteTime64) ||
        (currentSize.QuadPart != info.lastSize);

    if (changed) {
        LOG_WARNING(L"[SSRTGI] Hot-reload: CHANGE DETECTED in " + info.filename);
        info.lastWriteTime64 = currentTime.QuadPart;
        info.lastSize = currentSize.QuadPart;
        return true;
    }

    return false;
}

bool SsrtgiPostProcessD3D12::ReloadShader(const std::wstring& filename, ComPtr<ID3D12PipelineState>& pso, bool isHzb)
{
    if (csoDirectory_.empty() || !device_) {
        return false;
    }

    std::wstring fullPath = csoDirectory_ + filename;
    auto bytecode = LoadFileToBytes(fullPath);
    if (bytecode.empty()) {
        LOG_WARNING(L"[SSRTGI] Hot-reload: Failed to load: " + fullPath);
        return false;
    }

    ComPtr<ID3D12PipelineState> newPso;
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = isHzb ? hzbRootSig_.Get() : rootSig_.Get();
    desc.CS.pShaderBytecode = bytecode.data();
    desc.CS.BytecodeLength = bytecode.size();

    HRESULT hr = device_->CreateComputePipelineState(&desc, IID_PPV_ARGS(&newPso));
    if (FAILED(hr)) {
        LOG_WARNING(L"[SSRTGI] Hot-reload: Failed to create PSO for: " + filename);
        return false;
    }

    // Keep old PSO alive for one more frame
    pendingPsoRelease_ = pso;
    pso = std::move(newPso);

    LOG_WARNING(L"[SSRTGI] *** HOT-RELOAD SUCCESS: " + filename + L" ***");
    return true;
}

void SsrtgiPostProcessD3D12::CheckAndReloadShaders()
{
    if (!hotReloadEnabled_ || csoDirectory_.empty())
        return;

    hotReloadCheckCounter_++;
    if (hotReloadCheckCounter_ < HOT_RELOAD_CHECK_INTERVAL)
        return;
    hotReloadCheckCounter_ = 0;

    if (CheckShaderTimestamp(shaderGather_))
        ReloadShader(shaderGather_.filename, psoGather_, false);
    if (CheckShaderTimestamp(shaderDenoise_))
        ReloadShader(shaderDenoise_.filename, psoDenoise_, false);
#ifdef SSRTGI_ENABLE_TEMPORAL
    if (CheckShaderTimestamp(shaderTemporal_))
        ReloadShader(shaderTemporal_.filename, psoTemporal_, false);
#endif
    if (CheckShaderTimestamp(shaderComposite_))
        ReloadShader(shaderComposite_.filename, psoComposite_, false);
    if (CheckShaderTimestamp(shaderCopy_))
        ReloadShader(shaderCopy_.filename, psoCopy_, false);
    if (CheckShaderTimestamp(shaderHzbGen_))
        ReloadShader(shaderHzbGen_.filename, psoHzbGen_, true);
}

void SsrtgiPostProcessD3D12::EnsureCompositeResource(ID3D12Resource* colorInOut)
{
    const auto cd = colorInOut->GetDesc();
    const uint32_t w = (uint32_t)cd.Width;
    const uint32_t h = (uint32_t)cd.Height;
    const DXGI_FORMAT fmt = cd.Format;

    if (colorComposite_)
    {
        auto d = colorComposite_->GetDesc();
        if ((uint32_t)d.Width == w && d.Height == h && d.Format == fmt)
            return;
    }

    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = w;
    td.Height = h;
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(device_->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &td,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr, IID_PPV_ARGS(&colorComposite_)));

    colorCompositeState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
}

void SsrtgiPostProcessD3D12::CreateRootSig()
{
    D3D12_DESCRIPTOR_RANGE rangesSrv[1]{};
    rangesSrv[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangesSrv[0].NumDescriptors = 6;
    rangesSrv[0].BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE rangesUav[1]{};
    rangesUav[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangesUav[0].NumDescriptors = 3;
    rangesUav[0].BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER rp[3]{};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[0].Constants.ShaderRegister = 0;
    rp[0].Constants.Num32BitValues = 20;
    rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[1].DescriptorTable.NumDescriptorRanges = 1;
    rp[1].DescriptorTable.pDescriptorRanges = rangesSrv;
    rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[2].DescriptorTable.NumDescriptorRanges = 1;
    rp[2].DescriptorTable.pDescriptorRanges = rangesUav;
    rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_STATIC_SAMPLER_DESC samp{};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters = _countof(rp);
    rsd.pParameters = rp;
    rsd.NumStaticSamplers = 1;
    rsd.pStaticSamplers = &samp;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
    ThrowIfFailed(device_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rootSig_)));
}

void SsrtgiPostProcessD3D12::CreatePso(const void* bytecode, size_t size, ComPtr<ID3D12PipelineState>& outPso)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
    d.pRootSignature = rootSig_.Get();
    d.CS.pShaderBytecode = bytecode;
    d.CS.BytecodeLength = size;
    ThrowIfFailed(device_->CreateComputePipelineState(&d, IID_PPV_ARGS(&outPso)));
}

// ============================================================
// FIX #5: Robust resource state tracking with validation
// ============================================================
void SsrtgiPostProcessD3D12::TransitionTracked(
    ID3D12GraphicsCommandList* cl, ID3D12Resource* r,
    D3D12_RESOURCE_STATES& cur, D3D12_RESOURCE_STATES next)
{
    if (!r) {
        LOG_ERROR(L"[SSRTGI] TransitionTracked: NULL resource!");
        return;
    }

    if (cur == next) return;

    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.StateBefore = cur;
    b.Transition.StateAfter = next;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &b);

    cur = next;
}

void SsrtgiPostProcessD3D12::ToSRV(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES& curState)
{
    TransitionTracked(cl, r, curState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void SsrtgiPostProcessD3D12::ToUAV(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES& curState)
{
    TransitionTracked(cl, r, curState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void SsrtgiPostProcessD3D12::EnsureInternalResources(ID3D12Resource* color, ID3D12Resource* depth)
{
    auto cd = color->GetDesc();
    uint32_t w = (uint32_t)cd.Width;
    uint32_t h = (uint32_t)cd.Height;
    DXGI_FORMAT fmt = cd.Format;

    if (w == width_ && h == height_ && fmt == colorFormat_ && out0_) return;

    LOG_DEBUG(L"[SSRTGI] EnsureInternalResources: Resolution change " +
        std::to_wstring(width_) + L"x" + std::to_wstring(height_) +
        L" -> " + std::to_wstring(w) + L"x" + std::to_wstring(h));

    // ============================================================
    // FIX #5: Reset ALL tracked states when resolution changes
    // ============================================================
    out0State_ = D3D12_RESOURCE_STATE_COMMON;
    out1State_ = D3D12_RESOURCE_STATE_COMMON;
    histAState_ = D3D12_RESOURCE_STATE_COMMON;
    histBState_ = D3D12_RESOURCE_STATE_COMMON;
    depthHistAState_ = D3D12_RESOURCE_STATE_COMMON;
    depthHistBState_ = D3D12_RESOURCE_STATE_COMMON;
    colorCompositeState_ = D3D12_RESOURCE_STATE_COMMON;
    hzbState_ = D3D12_RESOURCE_STATE_COMMON;
    colorTmpState_ = D3D12_RESOURCE_STATE_COMMON;

    // ============================================================
    // FIX #4: Reset frame index on resolution change to reset ping-pong
    // ============================================================
    frameIndex_ = 0;

    width_ = w;
    height_ = h;
    colorFormat_ = fmt;

    auto dd = depth->GetDesc();
    depthFormat_ = dd.Format;

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    // out0, out1
    {
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = w;
        td.Height = h;
        td.DepthOrArraySize = 1;
        td.MipLevels = 1;
        td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        out0_.Reset();
        out1_.Reset();
        ThrowIfFailed(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&out0_)));
        ThrowIfFailed(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&out1_)));
    }

    // histA, histB
    {
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = w;
        td.Height = h;
        td.DepthOrArraySize = 1;
        td.MipLevels = 1;
        td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        histA_.Reset();
        histB_.Reset();
        ThrowIfFailed(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&histA_)));
        ThrowIfFailed(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&histB_)));
    }

    // depthHistA, depthHistB
    {
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = w;
        td.Height = h;
        td.DepthOrArraySize = 1;
        td.MipLevels = 1;
        td.Format = DXGI_FORMAT_R32_FLOAT;
        td.SampleDesc.Count = 1;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        depthHistA_.Reset();
        depthHistB_.Reset();
        ThrowIfFailed(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&depthHistA_)));
        ThrowIfFailed(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&depthHistB_)));
    }

    // colorTmp
    {
        D3D12_RESOURCE_DESC td{};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = w;
        td.Height = h;
        td.DepthOrArraySize = 1;
        td.MipLevels = 1;
        td.Format = fmt;
        td.SampleDesc.Count = 1;
        td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        colorTmp_.Reset();
        ThrowIfFailed(device_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&colorTmp_)));
    }

    hzb_.Reset();
    hzbMipCount_ = 0;

    // ============================================================
    // FIX #2: Recreate heap with proper size for double-buffered descriptors
    // ============================================================
    CreateOrResizeHeap();

    // Mark descriptors as needing refresh
    descriptorsNeedRefresh_ = true;
}

static D3D12_CPU_DESCRIPTOR_HANDLE OffsetCpu(ID3D12DescriptorHeap* h, UINT inc, UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE c = h->GetCPUDescriptorHandleForHeapStart();
    c.ptr += SIZE_T(inc) * index;
    return c;
}

static D3D12_GPU_DESCRIPTOR_HANDLE OffsetGpu(ID3D12DescriptorHeap* h, UINT inc, UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE g = h->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(inc) * index;
    return g;
}

// ============================================================
// FIX #2: Larger heap for double-buffered descriptors
// ============================================================
void SsrtgiPostProcessD3D12::CreateOrResizeHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = kTotalDescriptors;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heap_.Reset();
    ThrowIfFailed(device_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)));

    descriptorsNeedRefresh_ = true;
}

static DXGI_FORMAT PickDepthSrvFormat(DXGI_FORMAT f)
{
    switch (f)
    {
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_TYPELESS:
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_R16_UNORM:
        return f;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

// ============================================================
// FIX #2: WriteDescriptors now takes frameSlot parameter
// ============================================================
void SsrtgiPostProcessD3D12::WriteDescriptors(
    ID3D12Resource* colorInOut,
    ID3D12Resource* depth,
    ID3D12Resource* motionVectors,
    UINT pass,
    UINT frameSlot)
{
    const UINT base = PassBase(pass, frameSlot);

    DXGI_FORMAT depthFmt = depth->GetDesc().Format;
    DXGI_FORMAT srvFmt = PickDepthSrvFormat(depthFmt);
    if (srvFmt == DXGI_FORMAT_UNKNOWN)
        srvFmt = DXGI_FORMAT_R32_FLOAT;

    // t0: color
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = colorInOut->GetDesc().Format;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(colorInOut, &s, OffsetCpu(heap_.Get(), descSize_, base + 0));
    }

    // t1: depth
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = srvFmt;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(depth, &s, OffsetCpu(heap_.Get(), descSize_, base + 1));
    }

    // t2: history placeholder
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(histA_.Get(), &s, OffsetCpu(heap_.Get(), descSize_, base + 2));
    }

    // t3: motion vectors
    if (motionVectors)
    {
        DXGI_FORMAT mvFmt = motionVectors->GetDesc().Format;
        if (mvFmt == DXGI_FORMAT_R16G16_TYPELESS) mvFmt = DXGI_FORMAT_R16G16_FLOAT;
        else if (mvFmt == DXGI_FORMAT_R32G32_TYPELESS) mvFmt = DXGI_FORMAT_R32G32_FLOAT;

        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = mvFmt;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(motionVectors, &s, OffsetCpu(heap_.Get(), descSize_, base + 3));
    }
    else
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = srvFmt;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(depth, &s, OffsetCpu(heap_.Get(), descSize_, base + 3));
    }

    // t4: depth history placeholder
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = DXGI_FORMAT_R32_FLOAT;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(depthHistA_.Get(), &s, OffsetCpu(heap_.Get(), descSize_, base + 4));
    }

    // t5: HZB
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = DXGI_FORMAT_R32G32_FLOAT;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (hzb_ && hzbMipCount_ > 0)
        {
            s.Texture2D.MostDetailedMip = 0;
            s.Texture2D.MipLevels = (UINT)hzbMipCount_;
            device_->CreateShaderResourceView(hzb_.Get(), &s, OffsetCpu(heap_.Get(), descSize_, base + 5));
        }
        else
        {
            s.Format = srvFmt;
            s.Texture2D.MipLevels = 1;
            device_->CreateShaderResourceView(depth, &s, OffsetCpu(heap_.Get(), descSize_, base + 5));
        }
    }

    // u0, u1: out0, out1
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavRG16{};
    uavRG16.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    uavRG16.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    device_->CreateUnorderedAccessView(out0_.Get(), nullptr, &uavRG16, OffsetCpu(heap_.Get(), descSize_, base + 6));
    device_->CreateUnorderedAccessView(out1_.Get(), nullptr, &uavRG16, OffsetCpu(heap_.Get(), descSize_, base + 7));

    // u2: placeholder
    device_->CreateUnorderedAccessView(out0_.Get(), nullptr, &uavRG16, OffsetCpu(heap_.Get(), descSize_, base + 8));
}

void SsrtgiPostProcessD3D12::Transition(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    if (before == after) return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &b);
}

void SsrtgiPostProcessD3D12::UAVBarrier(ID3D12GraphicsCommandList* cl, ID3D12Resource* r)
{
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = r;
    cl->ResourceBarrier(1, &b);
}

// ============================================================
// MAIN EXECUTE FUNCTION - WITH DEBUG AND UE5 FIX
// ============================================================
void SsrtgiPostProcessD3D12::Execute(
    ID3D12GraphicsCommandList* cl,
    ID3D12Resource* colorInOut,
    ID3D12Resource* depth,
    ID3D12Resource* motionVectors,
    bool isDlssg)
{
    g_totalExecuteCalls++;

    // ============================================================
    // FIX #3: Verbose skip logging with counters
    // ============================================================
    if (!device_) {
        g_skippedFrames_NullDevice++;
        if (g_skippedFrames_NullDevice <= 10 || g_skippedFrames_NullDevice % 100 == 0) {
            LOG_ERROR(L"[SSRTGI] SKIP: device_ is NULL! (count: " +
                std::to_wstring(g_skippedFrames_NullDevice) + L")");
        }
        return;
    }
    if (!cl) {
        g_skippedFrames_NullCL++;
        if (g_skippedFrames_NullCL <= 10 || g_skippedFrames_NullCL % 100 == 0) {
            LOG_ERROR(L"[SSRTGI] SKIP: command list is NULL! (count: " +
                std::to_wstring(g_skippedFrames_NullCL) + L")");
        }
        return;
    }
    if (!colorInOut || !depth) {
        g_skippedFrames_NullResources++;
        if (g_skippedFrames_NullResources <= 10 || g_skippedFrames_NullResources % 100 == 0) {
            LOG_ERROR(L"[SSRTGI] SKIP: colorInOut or depth is NULL! (count: " +
                std::to_wstring(g_skippedFrames_NullResources) + L")");
        }
        return;
    }

    // ============================================================
    // DEBUG: Log input buffer details (first 5 frames only)
    // ============================================================
    static int g_debugFrameCount = 0;
    const bool shouldLog = (g_debugFrameCount < 5);
    if (shouldLog) {
        g_debugFrameCount++;
        LOG_WARNING(L"[SSRTGI] ========== FRAME " + std::to_wstring(g_debugFrameCount) + L" DEBUG ==========");

        D3D12_RESOURCE_DESC colorDesc = colorInOut->GetDesc();
        LOG_WARNING(L"[SSRTGI] colorInOut: W=" + std::to_wstring(colorDesc.Width) +
            L" H=" + std::to_wstring(colorDesc.Height) +
            L" Fmt=" + std::to_wstring(colorDesc.Format) +
            L" Flags=" + std::to_wstring(colorDesc.Flags));

        D3D12_RESOURCE_DESC depthDesc = depth->GetDesc();
        LOG_WARNING(L"[SSRTGI] depth: W=" + std::to_wstring(depthDesc.Width) +
            L" H=" + std::to_wstring(depthDesc.Height) +
            L" Fmt=" + std::to_wstring(depthDesc.Format) +
            L" Flags=" + std::to_wstring(depthDesc.Flags));

        if (motionVectors) {
            D3D12_RESOURCE_DESC mvDesc = motionVectors->GetDesc();
            LOG_WARNING(L"[SSRTGI] motionVectors: W=" + std::to_wstring(mvDesc.Width) +
                L" H=" + std::to_wstring(mvDesc.Height) +
                L" Fmt=" + std::to_wstring(mvDesc.Format) +
                L" Flags=" + std::to_wstring(mvDesc.Flags));
        }
        else {
            LOG_WARNING(L"[SSRTGI] motionVectors: NULL");
        }
    }

    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before CheckAndReloadShaders");

    // Hot-reload check (if enabled)
    CheckAndReloadShaders();

    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before EnsureInternalResources");

    EnsureInternalResources(colorInOut, depth);

    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before EnsureCompositeResource");

    EnsureCompositeResource(colorInOut);

    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: After EnsureCompositeResource");

    if (!out0_ || !out1_ || !histA_ || !histB_ || !heap_ || !rootSig_) {
        g_skippedFrames_InternalNull++;
        if (g_skippedFrames_InternalNull <= 10 || g_skippedFrames_InternalNull % 100 == 0) {
            LOG_ERROR(L"[SSRTGI] SKIP: Internal resources NULL after Ensure! (count: " +
                std::to_wstring(g_skippedFrames_InternalNull) + L")");
        }
        return;
    }

    if (shouldLog) {
        LOG_WARNING(L"[SSRTGI] Internal resources OK: width_=" + std::to_wstring(width_) +
            L" height_=" + std::to_wstring(height_) +
            L" colorFormat_=" + std::to_wstring(colorFormat_));
    }

    // ============================================================
    // FIX #4: Frame-index based ping-pong (not toggle based)
    // This prevents desync if frames are skipped
    // ============================================================
    const uint32_t currentFrameIndex = ++frameIndex_;
    const bool useHistAAsInput = (currentFrameIndex % 2 == 0);
    const UINT descriptorFrameSlot = currentFrameIndex % kNumBufferedFrames;

    ID3D12Resource* histIn = useHistAAsInput ? histA_.Get() : histB_.Get();
    ID3D12Resource* histOut = useHistAAsInput ? histB_.Get() : histA_.Get();
    D3D12_RESOURCE_STATES& histInState = useHistAAsInput ? histAState_ : histBState_;
    D3D12_RESOURCE_STATES& histOutState = useHistAAsInput ? histBState_ : histAState_;

    ID3D12Resource* depthHistIn = useHistAAsInput ? depthHistA_.Get() : depthHistB_.Get();
    ID3D12Resource* depthHistOut = useHistAAsInput ? depthHistB_.Get() : depthHistA_.Get();
    D3D12_RESOURCE_STATES& depthHistInState = useHistAAsInput ? depthHistAState_ : depthHistBState_;
    D3D12_RESOURCE_STATES& depthHistOutState = useHistAAsInput ? depthHistBState_ : depthHistAState_;

    const D3D12_RESOURCE_DESC cd = colorInOut->GetDesc();
    const bool colorHasUav = (cd.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0;

    if (shouldLog) LOG_WARNING(L"[SSRTGI] colorHasUav=" + std::to_wstring(colorHasUav));

    // ============================================================
    // FIX #1: Generate HZB with proper heap synchronization
    // ============================================================
    //GenerateHzb(cl, depth);

    // ============================================================
    // FIX #1: UAV barrier after HZB generation before switching heaps
    // ============================================================
    if (hzb_) {
        UAVBarrier(cl, hzb_.Get());
    }

    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before WriteDescriptors");

    // ============================================================
    // FIX #2: Write descriptors for current frame slot
    // ============================================================
    WriteDescriptors(colorInOut, depth, motionVectors, kPassGather, descriptorFrameSlot);
    WriteDescriptors(colorInOut, depth, motionVectors, kPassDenoise, descriptorFrameSlot);
#ifdef SSRTGI_ENABLE_TEMPORAL
    WriteDescriptors(colorInOut, depth, motionVectors, kPassTemporal, descriptorFrameSlot);
#endif
    WriteDescriptors(colorInOut, depth, motionVectors, kPassComposite, descriptorFrameSlot);
    WriteDescriptors(colorInOut, depth, motionVectors, kPassCopy, descriptorFrameSlot);

    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: After WriteDescriptors");

    // ============================================================
    // FIX #1: Set main descriptor heap with barrier
    // ============================================================
    ID3D12DescriptorHeap* heaps[] = { heap_.Get() };
    cl->SetDescriptorHeaps(1, heaps);
    cl->SetComputeRootSignature(rootSig_.Get());

    SsrtgiParams p = params_;
    p.Width = width_;
    p.Height = height_;
    p.FrameIndex = currentFrameIndex;
    cl->SetComputeRoot32BitConstants(0, 20, &p, 0);

    const UINT gx = (width_ + 7) / 8;
    const UINT gy = (height_ + 7) / 8;

    if (shouldLog) LOG_WARNING(L"[SSRTGI] Dispatch size: gx=" + std::to_wstring(gx) + L" gy=" + std::to_wstring(gy));

    // Lambda helpers for descriptor updates
    auto setSRV2 = [&](UINT pass, ID3D12Resource* r, DXGI_FORMAT fmt) {
        const UINT base = PassBase(pass, descriptorFrameSlot);
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = fmt;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(r, &s, OffsetCpu(heap_.Get(), descSize_, base + 2));
        };

    auto setSRV4 = [&](UINT pass, ID3D12Resource* r, DXGI_FORMAT fmt) {
        const UINT base = PassBase(pass, descriptorFrameSlot);
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = fmt;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(r, &s, OffsetCpu(heap_.Get(), descSize_, base + 4));
        };

    auto setUAV2 = [&](UINT pass, ID3D12Resource* r, DXGI_FORMAT fmt) {
        const UINT base = PassBase(pass, descriptorFrameSlot);
        D3D12_UNORDERED_ACCESS_VIEW_DESC u{};
        u.Format = fmt;
        u.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device_->CreateUnorderedAccessView(r, nullptr, &u, OffsetCpu(heap_.Get(), descSize_, base + 8));
        };

    auto bindTables = [&](UINT pass) {
        const UINT base = PassBase(pass, descriptorFrameSlot);
        cl->SetComputeRootDescriptorTable(1, OffsetGpu(heap_.Get(), descSize_, base + 0));
        cl->SetComputeRootDescriptorTable(2, OffsetGpu(heap_.Get(), descSize_, base + 6));
        };

    // ============================================================
    // Pass 1: Gather
    // ============================================================
    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before Pass 1 (Gather)");
    {
        bindTables(kPassGather);

        ToSRV(cl, histIn, histInState);
        setSRV2(kPassGather, histIn, DXGI_FORMAT_R16G16B16A16_FLOAT);

        ToUAV(cl, out0_.Get(), out0State_);

        cl->SetPipelineState(psoGather_.Get());
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 1: Before Dispatch");
        cl->Dispatch(gx, gy, 1);
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 1: After Dispatch");
        UAVBarrier(cl, out0_.Get());
    }

    // ============================================================
    // Pass 2: Denoise
    // ============================================================
    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before Pass 2 (Denoise)");
    {
        bindTables(kPassDenoise);
        UAVBarrier(cl, out0_.Get());
        ToUAV(cl, out1_.Get(), out1State_);

        cl->SetPipelineState(psoDenoise_.Get());
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 2: Before Dispatch");
        cl->Dispatch(gx, gy, 1);
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 2: After Dispatch");
        UAVBarrier(cl, out1_.Get());
    }

#ifdef SSRTGI_ENABLE_TEMPORAL
    // ============================================================
    // Pass 3: Temporal
    // ============================================================
    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before Pass 3 (Temporal)");
    {
        bindTables(kPassTemporal);

        ToSRV(cl, histIn, histInState);
        setSRV2(kPassTemporal, histIn, DXGI_FORMAT_R16G16B16A16_FLOAT);

        ToSRV(cl, depthHistIn, depthHistInState);
        setSRV4(kPassTemporal, depthHistIn, DXGI_FORMAT_R32_FLOAT);

        ToUAV(cl, histOut, histOutState);
        setUAV2(kPassTemporal, histOut, DXGI_FORMAT_R16G16B16A16_FLOAT);

        cl->SetPipelineState(psoTemporal_.Get());
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 3: Before Dispatch");
        cl->Dispatch(gx, gy, 1);
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 3: After Dispatch");
        UAVBarrier(cl, histOut);
    }
    ID3D12Resource* compositeSource = histOut;
    D3D12_RESOURCE_STATES& compositeSourceState = histOutState;
#else
    ID3D12Resource* compositeSource = out1_.Get();
    D3D12_RESOURCE_STATES& compositeSourceState = out1State_;
    ToSRV(cl, compositeSource, compositeSourceState);
#endif

    // ============================================================
    // Pass 4: Composite
    // ============================================================
    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before Pass 4 (Composite)");
    {
        bindTables(kPassComposite);

        ToSRV(cl, compositeSource, compositeSourceState);
        setSRV2(kPassComposite, compositeSource, DXGI_FORMAT_R16G16B16A16_FLOAT);

        ToUAV(cl, colorComposite_.Get(), colorCompositeState_);
        setUAV2(kPassComposite, colorComposite_.Get(), colorComposite_->GetDesc().Format);

        cl->SetPipelineState(psoComposite_.Get());
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 4: Before Dispatch");
        cl->Dispatch(gx, gy, 1);
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 4: After Dispatch");
        UAVBarrier(cl, colorComposite_.Get());
    }

    // ============================================================
    // Pass 5: Copy (color + depth history) - UE5 FIX
    // ============================================================
    if (shouldLog) LOG_WARNING(L"[SSRTGI] Checkpoint: Before Pass 5 (Copy)");
    {
        const UINT base = PassBase(kPassCopy, descriptorFrameSlot);

        TransitionTracked(cl, colorComposite_.Get(), colorCompositeState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvComp{};
        srvComp.Format = colorComposite_->GetDesc().Format;
        srvComp.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvComp.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvComp.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(colorComposite_.Get(), &srvComp,
            OffsetCpu(heap_.Get(), descSize_, base + 2));

        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 5: Creating UAV for colorTmp_");

        // UE5 FIX: Write to colorTmp_ instead of colorInOut (colorInOut may lack UAV flag)
        ToUAV(cl, colorTmp_.Get(), colorTmpState_);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavOut{};
        uavOut.Format = colorTmp_->GetDesc().Format;
        uavOut.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device_->CreateUnorderedAccessView(colorTmp_.Get(), nullptr, &uavOut,
            OffsetCpu(heap_.Get(), descSize_, base + 6));

        ToUAV(cl, depthHistOut, depthHistOutState);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDepth{};
        uavDepth.Format = DXGI_FORMAT_R32_FLOAT;
        uavDepth.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device_->CreateUnorderedAccessView(depthHistOut, nullptr, &uavDepth,
            OffsetCpu(heap_.Get(), descSize_, base + 8));

        cl->SetComputeRootDescriptorTable(1, OffsetGpu(heap_.Get(), descSize_, base + 0));
        cl->SetComputeRootDescriptorTable(2, OffsetGpu(heap_.Get(), descSize_, base + 6));

        cl->SetPipelineState(psoCopy_.Get());
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 5: Before Dispatch");
        cl->Dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);
        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 5: After Dispatch");

        UAVBarrier(cl, colorTmp_.Get());
        UAVBarrier(cl, depthHistOut);

        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 5: Before CopyResource colorTmp_ -> colorInOut");

        // UE5 FIX: Copy colorTmp_ -> colorInOut
        // colorInOut comes in as NON_PIXEL_SHADER_RESOURCE (FFX_RESOURCE_STATE_COMPUTE_READ equivalent)
        TransitionTracked(cl, colorTmp_.Get(), colorTmpState_, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Transition(cl, colorInOut, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        cl->CopyResource(colorInOut, colorTmp_.Get());
        Transition(cl, colorInOut, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        if (shouldLog) LOG_WARNING(L"[SSRTGI] Pass 5: After CopyResource");

        TransitionTracked(cl, colorComposite_.Get(), colorCompositeState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    if (shouldLog) LOG_WARNING(L"[SSRTGI] ========== FRAME " + std::to_wstring(g_debugFrameCount) + L" COMPLETE ==========");

    // ============================================================
    // FIX #4: No more histFlip_ toggle - using frame index instead
    // ============================================================

    // Log statistics periodically
    if (g_totalExecuteCalls % 1000 == 0) {
        LOG_DEBUG(L"[SSRTGI] Stats @ frame " + std::to_wstring(g_totalExecuteCalls) +
            L": skipped(dev=" + std::to_wstring(g_skippedFrames_NullDevice) +
            L", cl=" + std::to_wstring(g_skippedFrames_NullCL) +
            L", res=" + std::to_wstring(g_skippedFrames_NullResources) +
            L", int=" + std::to_wstring(g_skippedFrames_InternalNull) + L")");
    }
}

// ============================================================
// HZB GENERATION
// ============================================================
void SsrtgiPostProcessD3D12::CreateHzbRootSig()
{
    D3D12_DESCRIPTOR_RANGE rangesSrv[1] = {};
    rangesSrv[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangesSrv[0].NumDescriptors = 1;
    rangesSrv[0].BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE rangesUav[1] = {};
    rangesUav[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rangesUav[0].NumDescriptors = 6;
    rangesUav[0].BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER rp[3] = {};

    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[0].Constants.ShaderRegister = 0;
    rp[0].Constants.Num32BitValues = 8;
    rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[1].DescriptorTable.NumDescriptorRanges = 1;
    rp[1].DescriptorTable.pDescriptorRanges = rangesSrv;
    rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[2].DescriptorTable.NumDescriptorRanges = 1;
    rp[2].DescriptorTable.pDescriptorRanges = rangesUav;
    rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters = 3;
    rsd.pParameters = rp;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
    ThrowIfFailed(device_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&hzbRootSig_)));
}

void SsrtgiPostProcessD3D12::CreateHzbPso(const void* bytecode, size_t size)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
    d.pRootSignature = hzbRootSig_.Get();
    d.CS.pShaderBytecode = bytecode;
    d.CS.BytecodeLength = size;
    ThrowIfFailed(device_->CreateComputePipelineState(&d, IID_PPV_ARGS(&psoHzbGen_)));
}

void SsrtgiPostProcessD3D12::EnsureHzbResources()
{
    if (hzb_ && hzbMipCount_ > 0) return;

    uint32_t hzbWidth = (width_ + 1) / 2;
    uint32_t hzbHeight = (height_ + 1) / 2;

    uint32_t w = hzbWidth;
    uint32_t h = hzbHeight;
    hzbMipCount_ = 1;

    const int SPD_MAX_MIPS = 6;

    while ((w > 1 || h > 1) && hzbMipCount_ < SPD_MAX_MIPS)
    {
        w = max(1u, w / 2);
        h = max(1u, h / 2);
        hzbMipCount_++;
    }

    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = hzbWidth;
    td.Height = hzbHeight;
    td.DepthOrArraySize = 1;
    td.MipLevels = (UINT16)hzbMipCount_;
    td.Format = DXGI_FORMAT_R32G32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(device_->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &td,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr, IID_PPV_ARGS(&hzb_)));

    hzbState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = 1 + 6;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&hzbUavHeap_)));
}

void SsrtgiPostProcessD3D12::GenerateHzb(ID3D12GraphicsCommandList* cl, ID3D12Resource* depth)
{
    EnsureHzbResources();

    // ============================================================
    // FIX #1: Set HZB heap with proper state tracking
    // ============================================================
    ID3D12DescriptorHeap* heaps[] = { hzbUavHeap_.Get() };
    cl->SetDescriptorHeaps(1, heaps);
    cl->SetComputeRootSignature(hzbRootSig_.Get());
    cl->SetPipelineState(psoHzbGen_.Get());

    TransitionTracked(cl, hzb_.Get(), hzbState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    struct {
        uint32_t Width;
        uint32_t Height;
        uint32_t NumMips;
        uint32_t Pad0;
        float DepthInverted;
        float CameraNear;
        float CameraFar;
        uint32_t Pad1;
    } spdParams;

    spdParams.Width = width_;
    spdParams.Height = height_;
    spdParams.NumMips = hzbMipCount_;
    spdParams.Pad0 = 0;
    spdParams.DepthInverted = params_.DepthInverted;
    spdParams.CameraNear = params_.CameraNear;
    spdParams.CameraFar = params_.CameraFar;
    spdParams.Pad1 = 0;

    cl->SetComputeRoot32BitConstants(0, 8, &spdParams, 0);

    auto cpuHandle = hzbUavHeap_->GetCPUDescriptorHandleForHeapStart();

    DXGI_FORMAT srvFmt = PickDepthSrvFormat(depth->GetDesc().Format);
    if (srvFmt == DXGI_FORMAT_UNKNOWN) srvFmt = DXGI_FORMAT_R32_FLOAT;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDepth{};
    srvDepth.Format = srvFmt;
    srvDepth.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDepth.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDepth.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(depth, &srvDepth, cpuHandle);
    cpuHandle.ptr += descSize_;

    for (int mip = 0; mip < 6; mip++)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavHzb{};
        uavHzb.Format = DXGI_FORMAT_R32G32_FLOAT;
        uavHzb.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavHzb.Texture2D.MipSlice = min(mip, hzbMipCount_ - 1);
        device_->CreateUnorderedAccessView(hzb_.Get(), nullptr, &uavHzb, cpuHandle);
        cpuHandle.ptr += descSize_;
    }

    auto gpuHandle = hzbUavHeap_->GetGPUDescriptorHandleForHeapStart();
    cl->SetComputeRootDescriptorTable(1, gpuHandle);
    gpuHandle.ptr += descSize_;
    cl->SetComputeRootDescriptorTable(2, gpuHandle);

    uint32_t mip0W = (width_ + 1) / 2;
    uint32_t mip0H = (height_ + 1) / 2;
    uint32_t groupsX = (mip0W + 31) / 32;
    uint32_t groupsY = (mip0H + 31) / 32;

    cl->Dispatch(groupsX, groupsY, 1);

    // ============================================================
    // FIX #1: UAV barrier after HZB generation
    // ============================================================
    UAVBarrier(cl, hzb_.Get());

    TransitionTracked(cl, hzb_.Get(), hzbState_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}