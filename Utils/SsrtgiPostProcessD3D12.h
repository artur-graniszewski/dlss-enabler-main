#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>
#include <string>
using Microsoft::WRL::ComPtr;

struct SsrtgiParams
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    float RadiusPx = 48.0f;
    float AoStrength = 1.0f;
    float GiStrength = 1.0f;
    float DepthReject = 0.002f;
    float TemporalAlpha = 0.10f;
    float NormalizedDepth = 1.0f;
    uint32_t FrameIndex = 0;
    float FlipMotionVectors = 0.0f;
    float DepthInverted = 0.0f;
    float JitterX = 0.0f;
    float JitterY = 0.0f;

    float CameraFOV = 1.0472f;
    float CameraNear = 0.1f;
    float CameraFar = 200.0f;
    float CameraAspectRatio = 1.7778f;

    float MVScaleX = 1.0f;
    float MVScaleY = 1.0f;

    // New parameters for UI control
    uint32_t NumRays = 16;          // Ray count: 4, 8, 16, 32
    float FalloffEnd = 30.0f;       // Distance falloff end (10-200)
};

inline void ThrowIfFailed(HRESULT hr) { if (FAILED(hr)) __debugbreak(); }

static const int HZB_MAX_MIPS = 8;

struct ShaderFileInfo
{
    std::wstring filename;
    uint64_t lastWriteTime64 = 0;
    uint64_t lastSize = 0;
    bool valid = false;
};

class SsrtgiPostProcessD3D12
{
public:
    SsrtgiPostProcessD3D12() = default;
    ~SsrtgiPostProcessD3D12();

    void Init(ID3D12Device* device,
        const void* gatherCso, size_t gatherSize,
        const void* denoiseCso, size_t denoiseSize,
        const void* temporalCso, size_t temporalSize,
        const void* compositeCso, size_t compositeSize,
        const void* copyCso, size_t copySize,
        const void* hzbGenCso, size_t hzbGenSize);

    void InitWithHotReload(ID3D12Device* device,
        const std::wstring& csoDirectory,
        const void* gatherCso, size_t gatherSize,
        const void* denoiseCso, size_t denoiseSize,
        const void* temporalCso, size_t temporalSize,
        const void* compositeCso, size_t compositeSize,
        const void* copyCso, size_t copySize,
        const void* hzbGenCso, size_t hzbGenSize);

    void Execute(
        ID3D12GraphicsCommandList* cl,
        ID3D12Resource* colorInOut,
        ID3D12Resource* depth,
        ID3D12Resource* motionVectors,
        bool isDlssg = false);

    void SetParams(const SsrtgiParams& p) { params_ = p; }
    SsrtgiParams& GetParams() { return params_; }

    void CheckAndReloadShaders();
    void SetHotReloadEnabled(bool enabled) { hotReloadEnabled_ = enabled; }
    bool IsHotReloadEnabled() const { return hotReloadEnabled_; }

private:
    void CreateRootSig();
    void CreateHzbRootSig();
    void CreatePso(const void* bytecode, size_t size, ComPtr<ID3D12PipelineState>& outPso);
    void CreateHzbPso(const void* bytecode, size_t size);
    void EnsureInternalResources(ID3D12Resource* color, ID3D12Resource* depth);
    void EnsureCompositeResource(ID3D12Resource* colorInOut);
    void EnsureHzbResources();
    void GenerateHzb(ID3D12GraphicsCommandList* cl, ID3D12Resource* depth);

    void CreateOrResizeHeap();

    // ============================================================
    // FIX #2: WriteDescriptors now takes frameSlot parameter
    // ============================================================
    void WriteDescriptors(
        ID3D12Resource* colorInOut,
        ID3D12Resource* depth,
        ID3D12Resource* motionVectors,
        UINT pass,
        UINT frameSlot);

    void Transition(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    void UAVBarrier(ID3D12GraphicsCommandList* cl, ID3D12Resource* r);
    void ToSRV(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES& curState);
    void ToUAV(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES& curState);
    void TransitionTracked(
        ID3D12GraphicsCommandList* cl,
        ID3D12Resource* r,
        D3D12_RESOURCE_STATES& cur,
        D3D12_RESOURCE_STATES next);

    bool CheckShaderTimestamp(ShaderFileInfo& info);
    bool ReloadShader(const std::wstring& filename, ComPtr<ID3D12PipelineState>& pso, bool isHzb = false);
    void InitShaderFileInfo();

private:
    ID3D12Device* device_ = nullptr;

    ComPtr<ID3D12RootSignature> rootSig_;
    ComPtr<ID3D12RootSignature> hzbRootSig_;
    ComPtr<ID3D12PipelineState> psoGather_;
    ComPtr<ID3D12PipelineState> psoDenoise_;
    ComPtr<ID3D12PipelineState> psoTemporal_;
    ComPtr<ID3D12PipelineState> psoComposite_;
    ComPtr<ID3D12PipelineState> psoCopy_;
    ComPtr<ID3D12PipelineState> psoHzbGen_;
    ComPtr<ID3D12Resource> colorComposite_;

    // ============================================================
    // FIX #5: All tracked resource states
    // ============================================================
    D3D12_RESOURCE_STATES out0State_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES out1State_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES histAState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES histBState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES colorCompositeState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES colorTmpState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES depthHistAState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES depthHistBState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES hzbState_ = D3D12_RESOURCE_STATE_COMMON;

    ComPtr<ID3D12Resource> hzb_;
    ComPtr<ID3D12DescriptorHeap> hzbUavHeap_;
    int hzbMipCount_ = 0;

    ComPtr<ID3D12Resource> out0_;
    ComPtr<ID3D12Resource> out1_;
    ComPtr<ID3D12Resource> histA_;
    ComPtr<ID3D12Resource> histB_;
    ComPtr<ID3D12Resource> depthHistA_;
    ComPtr<ID3D12Resource> depthHistB_;

    // ============================================================
    // FIX #4: Removed histFlip_ - using frameIndex_ instead
    // ============================================================
    uint32_t frameIndex_ = 0;

    ComPtr<ID3D12Resource> colorTmp_;
    ComPtr<ID3D12DescriptorHeap> heap_;
    UINT descSize_ = 0;

    SsrtgiParams params_;
    DXGI_FORMAT colorFormat_ = DXGI_FORMAT_R16G16B16A16_FLOAT;
    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_R32_FLOAT;
    uint32_t width_ = 0, height_ = 0;

    // ============================================================
    // FIX #2: Flag to track if descriptors need refresh
    // ============================================================
    bool descriptorsNeedRefresh_ = true;

    // Hot-reload state
    bool hotReloadEnabled_ = false;
    std::wstring csoDirectory_;

    ShaderFileInfo shaderGather_;
    ShaderFileInfo shaderDenoise_;
    ShaderFileInfo shaderTemporal_;
    ShaderFileInfo shaderComposite_;
    ShaderFileInfo shaderCopy_;
    ShaderFileInfo shaderHzbGen_;

    uint32_t hotReloadCheckCounter_ = 0;
    static constexpr uint32_t HOT_RELOAD_CHECK_INTERVAL = 30;

    ComPtr<ID3D12PipelineState> pendingPsoRelease_;
};