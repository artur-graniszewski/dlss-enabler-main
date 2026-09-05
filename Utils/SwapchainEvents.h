#pragma once
// =============================================================================
// SwapChainEvents.h - Simple event listener system for SwapChain events
// =============================================================================

#include <dxgi1_6.h>
#include <vector>
#include <functional>

namespace SwapChainEvents
{
    // =========================================================================
    // Event types - function signatures for listeners
    // =========================================================================

    // Present events (called by wrapper, can modify SyncInterval/Flags)
    using PrePresentFn = std::function<void(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& Flags)>;
    using PostPresentFn = std::function<void(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags, HRESULT result)>;

    // Present1 events
    using PrePresent1Fn = std::function<void(IDXGISwapChain1* pSwapChain, UINT& SyncInterval, UINT& Flags, const DXGI_PRESENT_PARAMETERS* pParams)>;
    using PostPresent1Fn = std::function<void(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, HRESULT result)>;

    // ResizeBuffers events (renamed from ClearTrigger)
    using PreResizeBuffersFn = std::function<void(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT Flags)>;
    using PostResizeBuffersFn = std::function<void(IDXGISwapChain* pSwapChain, HRESULT result)>;

    // Destroy event - fired from the wrapper's Release() when its refcount hits 0,
    // BEFORE the wrapper is deleted. Lets subscribers drop references to this
    // swapchain's back buffers before anything recreates a swapchain on the same HWND.
    using PreDestroyFn = std::function<void(IDXGISwapChain* pSwapChain, HWND hwnd)>;

    // =========================================================================
    // Registration functions
    // =========================================================================

    void RegisterPrePresent(PrePresentFn listener);
    void RegisterPostPresent(PostPresentFn listener);
    void RegisterPrePresent1(PrePresent1Fn listener);
    void RegisterPostPresent1(PostPresent1Fn listener);
    void RegisterPreResizeBuffers(PreResizeBuffersFn listener);
    void RegisterPostResizeBuffers(PostResizeBuffersFn listener);
    void RegisterPreDestroy(PreDestroyFn listener);

    // =========================================================================
    // Internal dispatch functions (called by SwapChain wrapper/hooks)
    // =========================================================================

    void DispatchPrePresent(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& Flags);
    void DispatchPostPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags, HRESULT result);
    void DispatchPrePresent1(IDXGISwapChain1* pSwapChain, UINT& SyncInterval, UINT& Flags, const DXGI_PRESENT_PARAMETERS* pParams);
    void DispatchPostPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, HRESULT result);
    void DispatchPreResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT Flags);
    void DispatchPostResizeBuffers(IDXGISwapChain* pSwapChain, HRESULT result);
    void DispatchPreDestroy(IDXGISwapChain* pSwapChain, HWND hwnd);
}