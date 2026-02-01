#pragma once
// =============================================================================
// FpsMonitor.h - FPS and performance monitoring with GPU timestamps
// =============================================================================

#include <cstdint>

namespace FpsMonitor
{
    // Initialize the FPS monitor
    void Init();

    // Current FPS (actual frame rate, including VSync/frame limiter wait)
    int GetCurrentFps();

    // Potential FPS (theoretical max without VSync/frame limiter)
    // Calculated as: 1000 / max(CPU_work_time, GPU_work_time)
    int GetPotentialFps();

    // Timing data
    double GetFrameTimeMs();          // Average frame time (actual, including waits)
    double GetWorkTimeMs();           // Limiting factor: max(CPU, GPU) work time
    double GetCpuWorkTimeMs();        // CPU work time (from Reflex SIM_START to PRESENT_START)
    double GetGpuWorkTimeMs();        // GPU work time (averaged)
    double GetGpuWorkTimeRawMs();     // GPU work time (raw, non-averaged, for debug)
    double GetSimToPresentEndMs();    // Frame time from SimStart to PresentEnd (new algorithm)
    double GetReflexSleepTimeMs();    // Time spent in Reflex Sleep (frame limiter)
    double GetSimulationTimeMs();     // Legacy: simulation time
    double GetRenderSubmitTimeMs();   // Legacy: render submit time  
    double GetPresentWaitTimeMs();    // Time spent waiting in Present (VSync wait)

    // Latency data (from NvAPI_D3D_GetLatency)
    double GetAverageLatencyMs();     // Average total latency (render + PC latency)
    bool HasLatencyData();            // True if latency data is available (GetLatency called within last second)

    // Data availability
    bool HasReflexData();             // True if Reflex markers are being received
    bool HasGpuTimestamps();          // True if GPU timestamp queries are working

    // Legacy compatibility
    double GetLastSimTimeMs();        // Legacy: last sim time (returns CPU work time)
    double GetLastRenderTimeMs();     // Legacy: last render time (returns GPU work time)

    // Debug
    uint32_t GetLastMarkerType();     // Last received Reflex marker type
}