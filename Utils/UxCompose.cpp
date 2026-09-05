// =============================================================================
// UxCompose.cpp - D3D12 overlay composite pass
// =============================================================================

#include "UxCompose.h"
#include "Common.h"

#include <d3dcompiler.h>
#include <sstream>
#include <cstring>
#pragma comment(lib, "d3dcompiler")

namespace
{
    using Microsoft::WRL::ComPtr;

    ComPtr<ID3D12RootSignature>     g_RootSignature;
    ComPtr<ID3D12PipelineState>     g_Pipeline;

    ComPtr<ID3D12Resource>          g_Offscreen;
    ComPtr<ID3D12DescriptorHeap>    g_RtvHeap;      // 1 descriptor, CPU only
    ComPtr<ID3D12DescriptorHeap>    g_SrvHeap;      // 1 descriptor, shader visible

    ID3D12Device*                   g_Device = nullptr;
    UINT                            g_Width = 0;
    UINT                            g_Height = 0;
    DXGI_FORMAT                     g_BackBufferFormat = DXGI_FORMAT_UNKNOWN;

    // The offscreen target is created in RENDER_TARGET state; this tracks where
    // it currently is so the barriers stay balanced across frames.
    D3D12_RESOURCE_STATES           g_OffscreenState = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Root constants: paper white in nits, mode, two slots of padding to keep
    // the constant buffer 16-byte aligned.
    struct ComposeConstants
    {
        float   PaperWhiteNits;
        UINT    Mode;
        float   Pad0;
        float   Pad1;
    };

    const char* kShaderSource = R"HLSL(
cbuffer Params : register(b0)
{
    float PaperWhiteNits;
    uint  Mode;
    float Pad0;
    float Pad1;
};

Texture2D    OverlayTex : register(t0);
SamplerState PointClamp : register(s0);

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// Full-screen triangle, no vertex buffer.
VSOut VSMain(uint id : SV_VertexID)
{
    VSOut o;
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float3 SrgbToLinear(float3 c)
{
    return (c <= 0.04045) ? (c / 12.92) : pow((c + 0.055) / 1.055, 2.4);
}

// SMPTE ST 2084. Input is normalised so that 1.0 == 10000 nits.
float3 LinearToPQ(float3 c)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;

    float3 y = pow(max(c, 0.0), m1);
    return pow((c1 + c2 * y) / (1.0 + c3 * y), m2);
}

static const float3x3 Rec709ToRec2020 =
{
    0.627404, 0.329283, 0.043313,
    0.069097, 0.919540, 0.011362,
    0.016391, 0.088013, 0.895595
};

float4 PSMain(VSOut i) : SV_Target
{
    // The offscreen content is PREMULTIPLIED: ImGui's alpha blend state uses
    // BlendOpAlpha ONE / INV_SRC_ALPHA, so rendering onto transparent black
    // produces premultiplied output.
    float4 src = OverlayTex.Sample(PointClamp, i.uv);

    if (Mode == 0)
        return src;

    // Un-premultiply before the nonlinear transform - running a curve on
    // premultiplied colour darkens partially transparent pixels incorrectly.
    float  a = src.a;
    float3 c = (a > 0.0) ? (src.rgb / a) : float3(0.0, 0.0, 0.0);

    c = SrgbToLinear(saturate(c));

    if (Mode == 2)
    {
        c = mul(Rec709ToRec2020, c);
        c = LinearToPQ(c * (PaperWhiteNits / 10000.0));
    }
    else
    {
        // scRGB: linear, 1.0 == 80 nits.
        c = c * (PaperWhiteNits / 80.0);
    }

    // Re-premultiply so the composite blend below stays correct. Note this
    // means the final UI-over-game blend happens in the target space (PQ),
    // which is a deliberate accepted approximation - PQ is close enough to
    // perceptually uniform that a single blend in it is not objectionable.
    // Every UI-over-UI blend already happened in the offscreen target.
    return float4(c * a, a);
}
)HLSL";

    bool CreatePipeline(ID3D12Device* pDevice, DXGI_FORMAT backBufferFormat)
    {
        // ---- root signature: SRV table + root constants + static sampler ----
        D3D12_DESCRIPTOR_RANGE srvRange = {};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0;
        srvRange.RegisterSpace = 0;
        srvRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER params[2] = {};

        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable.NumDescriptorRanges = 1;
        params[0].DescriptorTable.pDescriptorRanges = &srvRange;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[1].Constants.ShaderRegister = 0;
        params[1].Constants.RegisterSpace = 0;
        params[1].Constants.Num32BitValues = sizeof(ComposeConstants) / 4;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters = _countof(params);
        rsDesc.pParameters = params;
        rsDesc.NumStaticSamplers = 1;
        rsDesc.pStaticSamplers = &sampler;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> rsBlob;
        ComPtr<ID3DBlob> rsError;
        if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsError)))
        {
            LOG_ERROR(L"[UxCompose] D3D12SerializeRootSignature failed");
            return false;
        }

        if (FAILED(pDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(),
            IID_PPV_ARGS(&g_RootSignature))))
        {
            LOG_ERROR(L"[UxCompose] CreateRootSignature failed");
            return false;
        }

        // ---- shaders ----
        ComPtr<ID3DBlob> vs, ps, err;

        if (FAILED(D3DCompile(kShaderSource, strlen(kShaderSource), nullptr, nullptr, nullptr,
            "VSMain", "vs_5_0", 0, 0, &vs, &err)))
        {
            LOG_ERROR(L"[UxCompose] Vertex shader compilation failed");
            return false;
        }

        if (FAILED(D3DCompile(kShaderSource, strlen(kShaderSource), nullptr, nullptr, nullptr,
            "PSMain", "ps_5_0", 0, 0, &ps, &err)))
        {
            LOG_ERROR(L"[UxCompose] Pixel shader compilation failed");
            return false;
        }

        // ---- PSO ----
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = g_RootSignature.Get();
        pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;

        // Must match the RTV the caller binds. NOTE: when the back buffer is an
        // _SRGB format the hardware applies an sRGB encode on write - the same
        // double-encode the overlay already had before this pass existed, so
        // behaviour there is unchanged. That is the separate RTV-format issue
        // in CreateRenderTargets_D3D12, deliberately not addressed here.
        pso.RTVFormats[0] = backBufferFormat;
        pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
        pso.SampleDesc.Count = 1;
        pso.SampleMask = UINT_MAX;

        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = FALSE;

        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DepthStencilState.StencilEnable = FALSE;

        // Premultiplied "over": dst = src + dst * (1 - src.a)
        D3D12_RENDER_TARGET_BLEND_DESC& blend = pso.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_ONE;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(pDevice->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_Pipeline))))
        {
            LOG_ERROR(L"[UxCompose] CreateGraphicsPipelineState failed");
            return false;
        }

        LOG_INFO(L"[UxCompose] Pipeline created");
        return true;
    }

    bool CreateOffscreen(ID3D12Device* pDevice, UINT width, UINT height)
    {
        if (!g_RtvHeap)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = 1;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if (FAILED(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_RtvHeap))))
            {
                LOG_ERROR(L"[UxCompose] RTV heap creation failed");
                return false;
            }
        }

        if (!g_SrvHeap)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 1;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_SrvHeap))))
            {
                LOG_ERROR(L"[UxCompose] SRV heap creation failed");
                return false;
            }
        }

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear = {};
        clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        if (FAILED(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &clear, IID_PPV_ARGS(&g_Offscreen))))
        {
            LOG_ERROR(L"[UxCompose] Offscreen target creation failed");
            return false;
        }

        g_Offscreen->SetName(L"UxCompose Overlay Offscreen");
        g_OffscreenState = D3D12_RESOURCE_STATE_RENDER_TARGET;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        pDevice->CreateRenderTargetView(g_Offscreen.Get(), &rtvDesc,
            g_RtvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        pDevice->CreateShaderResourceView(g_Offscreen.Get(), &srvDesc,
            g_SrvHeap->GetCPUDescriptorHandleForHeapStart());

        return true;
    }

    void Transition(ID3D12GraphicsCommandList* pCmdList, D3D12_RESOURCE_STATES after)
    {
        if (!pCmdList || !g_Offscreen || g_OffscreenState == after)
            return;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_Offscreen.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = g_OffscreenState;
        barrier.Transition.StateAfter = after;
        pCmdList->ResourceBarrier(1, &barrier);

        g_OffscreenState = after;
    }
}

namespace UxCompose
{
    bool EnsureResources(ID3D12Device* pDevice, UINT width, UINT height, DXGI_FORMAT backBufferFormat)
    {
        if (!pDevice || width == 0 || height == 0)
            return false;

        // A different device means everything we hold belongs to a dead object.
        if (g_Device != pDevice)
        {
            Shutdown();
            g_Device = pDevice;
        }

        if (!g_Pipeline || g_BackBufferFormat != backBufferFormat)
        {
            g_RootSignature.Reset();
            g_Pipeline.Reset();

            if (!CreatePipeline(pDevice, backBufferFormat))
                return false;

            g_BackBufferFormat = backBufferFormat;
        }

        if (!g_Offscreen || g_Width != width || g_Height != height)
        {
            g_Offscreen.Reset();

            if (!CreateOffscreen(pDevice, width, height))
                return false;

            g_Width = width;
            g_Height = height;

            std::wstringstream ss;
            ss << L"[UxCompose] Offscreen target " << width << L"x" << height << L" ready";
            LOG_INFO(ss.str());
        }

        return true;
    }

    void ReleaseSizeDependent()
    {
        g_Offscreen.Reset();
        g_Width = 0;
        g_Height = 0;
        g_OffscreenState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    void Shutdown()
    {
        ReleaseSizeDependent();
        g_RtvHeap.Reset();
        g_SrvHeap.Reset();
        g_Pipeline.Reset();
        g_RootSignature.Reset();
        g_Device = nullptr;
        g_BackBufferFormat = DXGI_FORMAT_UNKNOWN;
    }

    bool IsReady()
    {
        return g_Pipeline && g_Offscreen && g_RtvHeap && g_SrvHeap;
    }

    ID3D12Resource* GetOffscreenResource()
    {
        return g_Offscreen.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetOffscreenRtv()
    {
        if (!g_RtvHeap)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE null = {};
            return null;
        }
        return g_RtvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    void TransitionOffscreenToRenderTarget(ID3D12GraphicsCommandList* pCmdList)
    {
        Transition(pCmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    void TransitionOffscreenToShaderResource(ID3D12GraphicsCommandList* pCmdList)
    {
        Transition(pCmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void Draw(ID3D12GraphicsCommandList* pCmdList, Mode mode, float paperWhiteNits, UINT width, UINT height)
    {
        if (!pCmdList || !IsReady())
            return;

        ComposeConstants constants = {};
        constants.PaperWhiteNits = paperWhiteNits;
        constants.Mode = static_cast<UINT>(mode);

        ID3D12DescriptorHeap* heaps[] = { g_SrvHeap.Get() };
        pCmdList->SetDescriptorHeaps(1, heaps);

        pCmdList->SetGraphicsRootSignature(g_RootSignature.Get());
        pCmdList->SetGraphicsRootDescriptorTable(0, g_SrvHeap->GetGPUDescriptorHandleForHeapStart());
        pCmdList->SetGraphicsRoot32BitConstants(1, sizeof(ComposeConstants) / 4, &constants, 0);

        pCmdList->SetPipelineState(g_Pipeline.Get());
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D12_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MaxDepth = 1.0f;
        pCmdList->RSSetViewports(1, &viewport);

        D3D12_RECT scissor = {};
        scissor.right = static_cast<LONG>(width);
        scissor.bottom = static_cast<LONG>(height);
        pCmdList->RSSetScissorRects(1, &scissor);

        const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCmdList->OMSetBlendFactor(blendFactor);

        pCmdList->DrawInstanced(3, 1, 0, 0);
    }
}
