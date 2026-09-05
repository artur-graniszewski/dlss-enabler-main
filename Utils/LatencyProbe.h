#pragma once
// =============================================================================
// LatencyProbe.h - Developer-only latency instrumentation
// =============================================================================
//
// PURPOSE
//   Collects, for every base frame, BOTH:
//     (a) what Reflex/NVAPI claims (NV_LATENCY_RESULT_PARAMS::frameReport), and
//     (b) what we measure ourselves (QPC at every Reflex marker + a GPU
//         timestamp taken on the game's command queue),
//   and dumps them side by side to a CSV so the two can be correlated offline.
//
//   The point is to find out WHICH part of our own latency math is wrong.
//   Calibration is done on real NVIDIA hardware (where frameReport is ground
//   truth); the resulting formula is then what we ship for the non-NVIDIA path,
//   where NVAPI is served by FakeNVAPI and frameReport is garbage.
//
//   Our own numbers are computed unconditionally - independent of whether the
//   NVAPI behind us is genuine or emulated. ctx.nvapi.isEmbeddedNvapiUsed is
//   recorded as a CSV column so both regimes can be told apart after the fact.
//
// SCOPE
//   DEVELOPER TOOL. Compiled in only when LATENCY_PROBE == 1. There is no INI
//   switch by design: once a formula has been settled on, this module gets
//   hard-deleted rather than left behind as a dormant flag.
//
// OUTPUT
//   <module directory>\dlss-enabler-latency.csv
//   One row per base frame. Batched 64 frames at a time (the capacity of
//   NV_LATENCY_RESULT_PARAMS::frameReport) so that each flush carries a full
//   Reflex history covering exactly the frames we measured ourselves - the two
//   sides join on frameID with no alignment guesswork.
//
// THREADING
//   Producers are game threads and only ever do: write a few fields into a
//   preallocated POD slot under a short spinlock. No allocation, no formatting,
//   no file I/O, no blocking. A dedicated worker thread does all formatting and
//   writing, woken once per 64 frames.
//
// INTEGRATION
//   FpsMonitor.cpp : Init() + one call from each existing Reflex/SwapChain callback
//   UxHook.cpp     : D3D12_OnCommandList() before CommandList->Close()
//                    D3D12_OnSubmitted()   after  ExecuteCommandLists()
// =============================================================================

// Measurement. Ships enabled - this is where the production latency figure now
// comes from, for genuine and emulated NVAPI alike.
#ifndef LATENCY_PROBE
#define LATENCY_PROBE 1
#endif

// CSV capture. Developer tool only: batching, the writer thread and the file
// itself compile out when this is 0. The measurement above is unaffected.
#ifndef LATENCY_PROBE_CSV
#define LATENCY_PROBE_CSV 0
#endif

#include <Windows.h>
#include <cstdint>

// NV_LATENCY_RESULT_PARAMS is a typedef of _NV_LATENCY_RESULT_PARAMS, so it
// cannot be forward-declared - pull in the real declaration the same way
// ReflexEvents.h does.
#include "ReflexEvents.h"

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;
struct IDXGISwapChain;

namespace LatencyProbe
{
#if LATENCY_PROBE

    // Number of base frames buffered before a flush is handed to the worker.
    // Matches NV_LATENCY_RESULT_PARAMS::frameReport capacity - do not change
    // without understanding the join semantics described above.
    static const int SLOTS_PER_BATCH = 64;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    void Init();
    void Shutdown();
    bool IsActive();

    // -------------------------------------------------------------------------
    // Production output
    // -------------------------------------------------------------------------
    //
    // Pipelined latency of the most recently completed frame, smoothed - the
    // figure a player actually feels, and the same quantity native Reflex
    // reports:
    //
    //     simDelta + osRenderQueueDelta + gpuRenderDelta + presentDelta
    //
    // The stages overlap in wall-clock time, so this is not the span of a single
    // frame; it is how far an input travels through the pipeline, since every
    // frame queues behind the previous one at each stage.
    //
    // Reconstructed entirely from our own Reflex markers and GPU timestamps, so
    // it does not need NV_LATENCY_RESULT_PARAMS to be populated and is equally
    // valid on genuine and emulated NVAPI. Measured against the native report:
    // median -0.212 ms, MAE 0.783 ms on a 110-160 ms quantity.
    double GetLatencyMs();

    // True when a measurement arrived within the last second.
    bool HasLatency();

    // -------------------------------------------------------------------------
    // Producers - called from FpsMonitor's existing callbacks
    // -------------------------------------------------------------------------

    // Any Reflex latency marker. SIMULATION_START closes the previous base
    // frame and opens a new one; every other marker lands in the open slot.
    void OnMarker(uint64_t frameId, uint32_t markerType, bool isAsync);

    // pSwapChain is used to sample the present-queue configuration (buffer
    // count, swap effect, maximum frame latency). Those are what the whole-frame
    // shift k is suspected to fall out of - k moves in integer frame periods,
    // which is what a present queue does. Sampled, not assumed.
    void OnPrePresent(IDXGISwapChain* pSwapChain, unsigned syncInterval);
    void OnPostPresent(HRESULT result);

    void OnPreSleep();
    void OnPostSleep();

    // Snapshot of whatever NVAPI handed back. Copied wholesale; interpretation
    // happens offline. 'result' is the NvAPI_Status as returned.
    void OnGetLatency(const NV_LATENCY_RESULT_PARAMS* pParams, int result);

    // -------------------------------------------------------------------------
    // GPU timestamps - called from UxHook's D3D12 present path
    // -------------------------------------------------------------------------
    //
    // We cannot instrument the game's own command lists, so we cannot observe
    // gpuRenderStartTime. What we CAN observe is the queue-ordered moment at
    // which everything submitted before Present has retired - the analogue of
    // gpuRenderEndTime, which is the field the interesting formulas need.
    //
    // GPU ticks are mapped onto QPC via ID3D12CommandQueue::GetClockCalibration,
    // sampled once per batch. That is what makes frameReport (driver clock),
    // our markers (QPC) and our GPU timestamps (GPU ticks) comparable at all.

    // Insert the frame-end timestamp query and resolve an older one.
    // Call immediately BEFORE ID3D12GraphicsCommandList::Close().
    void D3D12_OnCommandList(ID3D12Device* pDevice,
        ID3D12CommandQueue* pQueue,
        ID3D12GraphicsCommandList* pCmdList);

    // Retire completed timestamps and refresh clock calibration.
    // Call immediately AFTER ID3D12CommandQueue::ExecuteCommandLists().
    void D3D12_OnSubmitted(ID3D12CommandQueue* pQueue);

    // -------------------------------------------------------------------------
    // Frame-generation GPU window
    // -------------------------------------------------------------------------
    //
    // Brackets the DLSSG NGX evaluate. Registered automatically against
    // NgxFeatureEvents (filtered to NVSDK_NGX_Feature_FrameGeneration) when that
    // header is reachable, so neither NgxFrontend nor DlssgProxy needs editing.
    // Exposed publicly as well, in case the event route turns out not to fire
    // and the calls have to be placed by hand.
    //
    // The same probe point serves both regimes: on NVIDIA it wraps the real
    // DLSSG snippet, on everything else it wraps our own FSR3 backend. That is
    // what makes the two directly comparable.
    //
    // subframeIndex is DLSSG.MultiFrameIndex (1-based); subframeCount is
    // DLSSG.MultiFrameCount.
    void OnDlssgEvaluatePre(ID3D12GraphicsCommandList* pCmdList, int subframeIndex, int subframeCount);
    void OnDlssgEvaluatePost(ID3D12GraphicsCommandList* pCmdList, int subframeIndex, int subframeCount);

#else // !LATENCY_PROBE 

    inline void Init() {}
    inline void Shutdown() {}
    inline bool IsActive() { return false; }
    inline double GetLatencyMs() { return 0.0; }
    inline bool HasLatency() { return false; }
    inline void OnMarker(uint64_t, uint32_t, bool) {}
    inline void OnPrePresent(IDXGISwapChain*, unsigned) {}
    inline void OnPostPresent(HRESULT) {}
    inline void OnPreSleep() {}
    inline void OnPostSleep() {}
    inline void OnGetLatency(const NV_LATENCY_RESULT_PARAMS*, int) {}
    inline void D3D12_OnCommandList(ID3D12Device*, ID3D12CommandQueue*, ID3D12GraphicsCommandList*) {}
    inline void D3D12_OnSubmitted(ID3D12CommandQueue*) {}
    inline void OnDlssgEvaluatePre(ID3D12GraphicsCommandList*, int, int) {}
    inline void OnDlssgEvaluatePost(ID3D12GraphicsCommandList*, int, int) {}

#endif // LATENCY_PROBE
}