// =============================================================================
// ReflexEvents.cpp - Simple event listener system for Reflex/NVAPI hooks
// =============================================================================

#include "ReflexEvents.h"
#include <vector>
#include <mutex>

namespace ReflexEvents
{
    // =========================================================================
    // Listener storage
    // =========================================================================

    static std::vector<PreSetSleepModeFn> g_PreSetSleepModeListeners;
    static std::vector<PostSetSleepModeFn> g_PostSetSleepModeListeners;
    static std::vector<PreSleepFn> g_PreSleepListeners;
    static std::vector<PostSleepFn> g_PostSleepListeners;
    static std::vector<PreSetLatencyMarkerFn> g_PreSetLatencyMarkerListeners;
    static std::vector<PostSetLatencyMarkerFn> g_PostSetLatencyMarkerListeners;
    static std::vector<PreSetAsyncFrameMarkerFn> g_PreSetAsyncFrameMarkerListeners;
    static std::vector<PostSetAsyncFrameMarkerFn> g_PostSetAsyncFrameMarkerListeners;
    static std::vector<PreGetLatencyFn> g_PreGetLatencyListeners;
    static std::vector<PostGetLatencyFn> g_PostGetLatencyListeners;

    static std::mutex g_Mutex;

    // =========================================================================
    // Registration functions
    // =========================================================================

    void RegisterPreSetSleepMode(PreSetSleepModeFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreSetSleepModeListeners.push_back(std::move(listener));
    }

    void RegisterPostSetSleepMode(PostSetSleepModeFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostSetSleepModeListeners.push_back(std::move(listener));
    }

    void RegisterPreSleep(PreSleepFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreSleepListeners.push_back(std::move(listener));
    }

    void RegisterPostSleep(PostSleepFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostSleepListeners.push_back(std::move(listener));
    }

    void RegisterPreSetLatencyMarker(PreSetLatencyMarkerFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreSetLatencyMarkerListeners.push_back(std::move(listener));
    }

    void RegisterPostSetLatencyMarker(PostSetLatencyMarkerFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostSetLatencyMarkerListeners.push_back(std::move(listener));
    }

    void RegisterPreSetAsyncFrameMarker(PreSetAsyncFrameMarkerFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreSetAsyncFrameMarkerListeners.push_back(std::move(listener));
    }

    void RegisterPostSetAsyncFrameMarker(PostSetAsyncFrameMarkerFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostSetAsyncFrameMarkerListeners.push_back(std::move(listener));
    }

    void RegisterPreGetLatency(PreGetLatencyFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreGetLatencyListeners.push_back(std::move(listener));
    }

    void RegisterPostGetLatency(PostGetLatencyFn listener)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostGetLatencyListeners.push_back(std::move(listener));
    }

    // =========================================================================
    // Dispatch functions
    // =========================================================================

    void DispatchPreSetSleepMode(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pParams)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PreSetSleepModeListeners)
        {
            listener(pDevice, pParams);
        }
    }

    void DispatchPostSetSleepMode(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pParams, int result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostSetSleepModeListeners)
        {
            listener(pDevice, pParams, result);
        }
    }

    void DispatchPreSleep(void* pDevice)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PreSleepListeners)
        {
            listener(pDevice);
        }
    }

    void DispatchPostSleep(void* pDevice, int result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostSleepListeners)
        {
            listener(pDevice, result);
        }
    }

    void DispatchPreSetLatencyMarker(void* pDevice, uint64_t frameId, uint32_t markerType)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PreSetLatencyMarkerListeners)
        {
            listener(pDevice, frameId, markerType);
        }
    }

    void DispatchPostSetLatencyMarker(void* pDevice, uint64_t frameId, uint32_t markerType, int result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostSetLatencyMarkerListeners)
        {
            listener(pDevice, frameId, markerType, result);
        }
    }

    void DispatchPreSetAsyncFrameMarker(ID3D12CommandQueue* pQueue, uint64_t frameId, uint32_t markerType)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PreSetAsyncFrameMarkerListeners)
        {
            listener(pQueue, frameId, markerType);
        }
    }

    void DispatchPostSetAsyncFrameMarker(ID3D12CommandQueue* pQueue, uint64_t frameId, uint32_t markerType, int result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostSetAsyncFrameMarkerListeners)
        {
            listener(pQueue, frameId, markerType, result);
        }
    }

    void DispatchPreGetLatency(void* pDevice, NV_LATENCY_RESULT_PARAMS* pParams)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PreGetLatencyListeners)
        {
            listener(pDevice, pParams);
        }
    }

    void DispatchPostGetLatency(void* pDevice, NV_LATENCY_RESULT_PARAMS* pParams, int result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& listener : g_PostGetLatencyListeners)
        {
            listener(pDevice, pParams, result);
        }
    }
}