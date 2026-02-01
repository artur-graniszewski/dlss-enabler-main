#pragma once

// =============================================================================
// UxHook.h - D3D11/D3D12 DXGI Hook with ImGui Overlay (v10 - Unified)
// =============================================================================
//
// Supports both D3D11 and D3D12 - auto-detects API from SwapChain
//
// USAGE:
//   1. Call UxInit() at DLL startup
//   2. In proxy_CreateSwapChainForHwnd:
//      - BEFORE calling original: UxHook::OnSwapChainAboutToBeCreated(hWnd);
//      - For D3D12, capture CommandQueue:
//        ID3D12CommandQueue* pQueue;
//        if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&pQueue)))) {
//            UxHook::SetSwapChainCommandQueue(pQueue);
//            pQueue->Release();
//        }
//   3. In prePresentHook or postPresentHook:
//      UxHook::RenderOverlay(pSwapChain);
//   4. In ResizeBuffers handling:
//      UxHook::OnResizeBuffers();
//      // ... do resize ...
//      UxHook::OnResizeBuffersComplete(pSwapChain);
//   5. Call UxShutdown() at DLL unload
// =============================================================================

#ifndef UX_HOOK_H
#define UX_HOOK_H

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

// =============================================================================
// Public API
// =============================================================================

bool UxInit();
void UxShutdown();

// =============================================================================
// Wrapper Integration API
// =============================================================================

namespace UxHook
{
    // Graphics API type
    enum class GraphicsAPI
    {
        Unknown,
        D3D11,
        D3D12
    };

    // Unified render state
    struct RenderState
    {
        bool                                    Initialized = false;
        GraphicsAPI                             API = GraphicsAPI::Unknown;

        // D3D11 resources
        Microsoft::WRL::ComPtr<ID3D11Device>            D3D11Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>     D3D11Context;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  D3D11RenderTargetView;

        // D3D12 resources
        Microsoft::WRL::ComPtr<ID3D12Device>            D3D12Device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue>      OwnCommandQueue;
        ID3D12CommandQueue* GameCommandQueue = nullptr;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    RtvHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    SrvHeap;
        static constexpr UINT                           MaxBufferCount = 8;
        UINT                                            BufferCount = 0;
        Microsoft::WRL::ComPtr<ID3D12Resource>          RenderTargets[MaxBufferCount];
        D3D12_CPU_DESCRIPTOR_HANDLE                     RtvHandles[MaxBufferCount];
        Microsoft::WRL::ComPtr<ID3D12Fence>             Fence;
        HANDLE                                          FenceEvent = nullptr;
        UINT64                                          FenceValue = 0;

        // Shared
        Microsoft::WRL::ComPtr<IDXGISwapChain>  SwapChain;
        HWND                                    TargetWindow = nullptr;
        WNDPROC                                 OriginalWndProc = nullptr;
        ULONGLONG                               StartTime = 0;
        bool                                    PopupShown = false;
    };

    // Get internal state
    RenderState& GetState();

    // Get detected API
    GraphicsAPI GetDetectedAPI();

    // Force specific API (call before first RenderOverlay)
    void ForceGraphicsAPI(GraphicsAPI api);

    // =========================================================================
    // CALL THESE FROM SwapchainProxy:
    // =========================================================================

    // Store CommandQueue from CreateSwapChainForHwnd (D3D12 only)
    void SetSwapChainCommandQueue(ID3D12CommandQueue* pQueue);

    // Call BEFORE CreateSwapChainForHwnd to release overlay refs
    void OnSwapChainAboutToBeCreated(HWND hwnd);

    // Render overlay - auto-detects D3D11 vs D3D12
    void RenderOverlay(IDXGISwapChain* pSwapChain);

    // Call BEFORE ResizeBuffers
    void OnResizeBuffers();

    // Call AFTER ResizeBuffers succeeded
    void OnResizeBuffersComplete(IDXGISwapChain* pSwapChain);

    // Disable overlay
    void DisableOverlay();

    // Check if overlay is disabled
    bool IsOverlayDisabled();
}

#endif // UX_HOOK_H
