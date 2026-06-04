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

    // Base FPS (actual render rate without frame generation multiplier)
    // Calculated as: CurrentFPS / (framesGenerated + 1)
    int GetBaseFps();

    // Dynamic MFG - call every frame to adjust mfgEnforcedMode based on base FPS thresholds
    // Only acts when ctx.streamline.isDynamicMfgEnabled == true
    void UpdateDynamicMfg();

    // Returns the currently suggested MFG mode (1=2X, 2=3X, 3=4X, 4=5X, 5=6X, 0=off/disabled)
    // Updated every frame, useful for status bar display
    int GetDynamicMfgSuggestedMode();

    // DFG Ping: periodically lowers MFG by one step to escape VSync traps
    // When VSync locks base FPS to a low value at high MFG modes (3X/4X/5X/6X),
    // the system can't detect that GPU has headroom. Ping temporarily drops
    // the multiplier so DFG logic can re-evaluate at the true base FPS.
    // Only active when DFG is enabled and current mode >= 2 (3X or higher).
    bool IsDfgPingActive();           // True if a ping-down is currently in effect

    // DFG State Machine
    enum class DfgState
    {
        Disabled,           // DFG off, pass-through
        WaitForFG,          // framesGenerated == 0
        Steady,             // mode matches threshold suggestion, intensity low
        ThresholdSwitch,    // cooldown after normal threshold-based mode change
        Pinging,            // DfgPing active (VSync trap escape)
        LatencyReduction,   // intensity high, MFG lowered toward minAllowedMode
        Recovery            // intensity dropped, ramping MFG back up step-by-step
    };

    DfgState GetDfgState();             // Current state (for debug/monitoring bar)
    const wchar_t* GetDfgStateName();   // Current state as string

    // Legacy compatibility
    double GetLastSimTimeMs();        // Legacy: last sim time (returns CPU work time)
    double GetLastRenderTimeMs();     // Legacy: last render time (returns GPU work time)

    // Debug
    uint32_t GetLastMarkerType();     // Last received Reflex marker type
}