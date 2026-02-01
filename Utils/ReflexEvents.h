#pragma once
// =============================================================================
// ReflexEvents.h - Simple event listener system for Reflex/NVAPI hooks
// =============================================================================

#include <Windows.h>
#include <functional>
#include <cstdint>
#include "../Includes/nvapi.h"

struct ID3D12CommandQueue;

namespace ReflexEvents
{
    // =========================================================================
    // Event types
    // =========================================================================

    // SetSleepMode events
    using PreSetSleepModeFn = std::function<void(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pParams)>;
    using PostSetSleepModeFn = std::function<void(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pParams, int result)>;

    // Sleep events (called each frame when Reflex is active)
    using PreSleepFn = std::function<void(void* pDevice)>;
    using PostSleepFn = std::function<void(void* pDevice, int result)>;

    // Latency Marker events
    // markerType: SIMULATION_START=0, SIMULATION_END=1, RENDERSUBMIT_START=2, RENDERSUBMIT_END=3,
    //             PRESENT_START=4, PRESENT_END=5, INPUT_SAMPLE=6, TRIGGER_FLASH=7, PC_LATENCY_PING=8
    using PreSetLatencyMarkerFn = std::function<void(void* pDevice, uint64_t frameId, uint32_t markerType)>;
    using PostSetLatencyMarkerFn = std::function<void(void* pDevice, uint64_t frameId, uint32_t markerType, int result)>;

    // Async Frame Marker events (D3D12)
    using PreSetAsyncFrameMarkerFn = std::function<void(ID3D12CommandQueue* pQueue, uint64_t frameId, uint32_t markerType)>;
    using PostSetAsyncFrameMarkerFn = std::function<void(ID3D12CommandQueue* pQueue, uint64_t frameId, uint32_t markerType, int result)>;

    // GetLatency events
    using PreGetLatencyFn = std::function<void(void* pDevice, NV_LATENCY_RESULT_PARAMS* pParams)>;
    using PostGetLatencyFn = std::function<void(void* pDevice, NV_LATENCY_RESULT_PARAMS* pParams, int result)>;

    // =========================================================================
    // Registration functions
    // =========================================================================

    void RegisterPreSetSleepMode(PreSetSleepModeFn listener);
    void RegisterPostSetSleepMode(PostSetSleepModeFn listener);

    void RegisterPreSleep(PreSleepFn listener);
    void RegisterPostSleep(PostSleepFn listener);

    void RegisterPreSetLatencyMarker(PreSetLatencyMarkerFn listener);
    void RegisterPostSetLatencyMarker(PostSetLatencyMarkerFn listener);

    void RegisterPreSetAsyncFrameMarker(PreSetAsyncFrameMarkerFn listener);
    void RegisterPostSetAsyncFrameMarker(PostSetAsyncFrameMarkerFn listener);

    void RegisterPreGetLatency(PreGetLatencyFn listener);
    void RegisterPostGetLatency(PostGetLatencyFn listener);

    // =========================================================================
    // Dispatch functions (called by NVAPI hooks)
    // =========================================================================

    void DispatchPreSetSleepMode(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pParams);
    void DispatchPostSetSleepMode(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pParams, int result);

    void DispatchPreSleep(void* pDevice);
    void DispatchPostSleep(void* pDevice, int result);

    void DispatchPreSetLatencyMarker(void* pDevice, uint64_t frameId, uint32_t markerType);
    void DispatchPostSetLatencyMarker(void* pDevice, uint64_t frameId, uint32_t markerType, int result);

    void DispatchPreSetAsyncFrameMarker(ID3D12CommandQueue* pQueue, uint64_t frameId, uint32_t markerType);
    void DispatchPostSetAsyncFrameMarker(ID3D12CommandQueue* pQueue, uint64_t frameId, uint32_t markerType, int result);

    void DispatchPreGetLatency(void* pDevice, NV_LATENCY_RESULT_PARAMS* pParams);
    void DispatchPostGetLatency(void* pDevice, NV_LATENCY_RESULT_PARAMS* pParams, int result);
}