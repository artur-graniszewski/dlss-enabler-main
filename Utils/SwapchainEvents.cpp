// =============================================================================
// SwapChainEvents.cpp - Simple event listener system for SwapChain events
// =============================================================================

#include "SwapChainEvents.h"
#include <mutex>

namespace SwapChainEvents
{
    // =========================================================================
    // Listener storage
    // =========================================================================

    static std::vector<PrePresentFn> g_PrePresentListeners;
    static std::vector<PostPresentFn> g_PostPresentListeners;
    static std::vector<PrePresent1Fn> g_PrePresent1Listeners;
    static std::vector<PostPresent1Fn> g_PostPresent1Listeners;
    static std::vector<PreResizeBuffersFn> g_PreResizeBuffersListeners;
    static std::vector<PostResizeBuffersFn> g_PostResizeBuffersListeners;
    static std::vector<PreDestroyFn> g_PreDestroyListeners;

    static std::mutex g_Mutex;

    // =========================================================================
    // Registration functions
    // =========================================================================

    void RegisterPrePresent(PrePresentFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PrePresentListeners.push_back(std::move(listener));
    }

    void RegisterPostPresent(PostPresentFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostPresentListeners.push_back(std::move(listener));
    }

    void RegisterPrePresent1(PrePresent1Fn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PrePresent1Listeners.push_back(std::move(listener));
    }

    void RegisterPostPresent1(PostPresent1Fn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostPresent1Listeners.push_back(std::move(listener));
    }

    void RegisterPreResizeBuffers(PreResizeBuffersFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreResizeBuffersListeners.push_back(std::move(listener));
    }

    void RegisterPostResizeBuffers(PostResizeBuffersFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostResizeBuffersListeners.push_back(std::move(listener));
    }

    void RegisterPreDestroy(PreDestroyFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreDestroyListeners.push_back(std::move(listener));
    }

    // =========================================================================
    // Dispatch functions
    // =========================================================================

    void DispatchPrePresent(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& Flags)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PrePresentListeners)
        {
            listener(pSwapChain, SyncInterval, Flags);
        }
    }

    void DispatchPostPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags, HRESULT result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostPresentListeners)
        {
            listener(pSwapChain, SyncInterval, Flags, result);
        }
    }

    void DispatchPrePresent1(IDXGISwapChain1* pSwapChain, UINT& SyncInterval, UINT& Flags, const DXGI_PRESENT_PARAMETERS* pParams)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PrePresent1Listeners)
        {
            listener(pSwapChain, SyncInterval, Flags, pParams);
        }
    }

    void DispatchPostPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, HRESULT result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostPresent1Listeners)
        {
            listener(pSwapChain, SyncInterval, Flags, result);
        }
    }

    void DispatchPreResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT Flags)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PreResizeBuffersListeners)
        {
            listener(pSwapChain, BufferCount, Width, Height, Format, Flags);
        }
    }

    void DispatchPostResizeBuffers(IDXGISwapChain* pSwapChain, HRESULT result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostResizeBuffersListeners)
        {
            listener(pSwapChain, result);
        }
    }

    void DispatchPreDestroy(IDXGISwapChain* pSwapChain, HWND hwnd)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PreDestroyListeners)
        {
            listener(pSwapChain, hwnd);
        }
    }
}