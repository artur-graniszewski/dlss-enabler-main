#pragma once
// =============================================================================
// UxCompose.h - D3D12 overlay composite pass (SDR authoring -> back buffer space)
// =============================================================================
//
// ImGui always renders into an offscreen R8G8B8A8_UNORM target, and this pass
// composites that target onto the back buffer, converting to whatever transfer
// function the swapchain uses.
//
// WHY ALWAYS OFFSCREEN, EVEN IN SDR
// ---------------------------------
//   1. UI-over-UI blending (panels, shadows, hover states, animations) stays in
//      the sRGB-ish space it was authored in. Only the final composite of the
//      finished UI over the game happens in the target space - one nonlinear
//      blend instead of one per draw command.
//   2. The ImGui PSO is created against a FIXED render target format. The colour
//      space is often only known after init (SetColorSpace1 routinely arrives
//      after the first Present), so a format that depends on the resolved mode
//      would force a PSO rebuild mid-session.
//   3. The HDR path is structurally exercised in SDR too - the only difference
//      is the branch inside the pixel shader.
//
// Cost in SDR is one full-screen blend, which is not measurable next to the
// frame-generation work this overlay sits on top of.
//
// D3D12 ONLY. The D3D11 overlay path still renders straight to the back buffer
// and is therefore still wrong in HDR - deliberately, because there is no way
// to verify a D3D11 HDR path on the current test hardware.
// =============================================================================

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace UxCompose
{
    enum class Mode
    {
        Sdr = 0,    // pass-through: no conversion, just the composite blend
        ScRgb = 1,  // linear, 1.0 == 80 nits  (UNTESTED - no FP16 test hardware)
        Hdr10 = 2   // Rec.2020 primaries + PQ encode
    };

    // Creates the pipeline (once) and the size-dependent offscreen target.
    // Safe to call every time the back buffers are (re)created; the pipeline is
    // only built on the first call, the offscreen target only when the size or
    // the back buffer format actually changed.
    bool EnsureResources(ID3D12Device* pDevice, UINT width, UINT height, DXGI_FORMAT backBufferFormat);

    // Drops the offscreen target only (ResizeBuffers path).
    void ReleaseSizeDependent();

    // Drops everything including the PSO / root signature.
    void Shutdown();

    bool IsReady();

    ID3D12Resource* GetOffscreenResource();
    D3D12_CPU_DESCRIPTOR_HANDLE GetOffscreenRtv();

    // Transitions used by the caller around the ImGui draw.
    void TransitionOffscreenToRenderTarget(ID3D12GraphicsCommandList* pCmdList);
    void TransitionOffscreenToShaderResource(ID3D12GraphicsCommandList* pCmdList);

    // Draws the full-screen composite. The caller must already have the back
    // buffer bound as the render target and in RENDER_TARGET state, and the
    // offscreen target must be in PIXEL_SHADER_RESOURCE state.
    //
    // This sets its own descriptor heap, root signature, PSO, viewport and
    // scissor - it does NOT restore the caller's, because the caller is at the
    // end of its own command list.
    void Draw(ID3D12GraphicsCommandList* pCmdList, Mode mode, float paperWhiteNits, UINT width, UINT height);
}
