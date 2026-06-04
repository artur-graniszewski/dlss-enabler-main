// ffx_gpu_profiler.h
//
// Per-pass GPU timing for FSR3 MFG pipeline.
//
// Architecture:
//   - Lives in FSR3 MFG static library (same TU domain as ffx_dx12.cpp backend).
//   - Backend calls FfxProf_BeginPass/EndPass around each compute dispatch.
//   - Frontend (NgxFrontend.cpp in DLSS Enabler) calls FfxProf_BeginFrame at the
//     start of EvaluateD3D12 and drains completed frames with FfxProf_PopCompletedFrame.
//   - Symbol visibility is provided by transitive static linking:
//     DLSS Enabler.dll <- DLSSG Proxy.lib <- FSR3 MFG.lib (which contains this module)
//
// Presence detection from the consumer side:
//   - If this header is present in the include path AND the profiler translation
//     unit is linked, the macro FFX_GPU_PROFILER_AVAILABLE is defined.
//   - Consumers (e.g. NgxFrontend.cpp) wrap all usage in
//         #if defined(FFX_GPU_PROFILER_AVAILABLE)
//     so swapping to an FSR3 MFG build without this module compiles cleanly.
//
// Resolve latency:
//   - Query resolves happen on the *next* frame's cmd list (lazy resolve),
//     so PopCompletedFrame returns data that is 2-3 game frames old.
//   - This is intentional: non-recording cmd lists used by FSR3 may already be
//     Closed() by the time EvaluateD3D12 returns, so we can't resolve inline.
//
// Thread safety:
//   - All public functions take an internal mutex. Backend and frontend can
//     call from any thread; ordering is preserved within a single frame.

#ifndef FFX_GPU_PROFILER_H
#define FFX_GPU_PROFILER_H

#include <stdint.h>
#include <wchar.h>

// Marker macro used by consumers to #ifdef-guard their usage.
// Defined unconditionally here; the *absence* of the header is what signals
// "profiler unavailable" — consumers should wrap the #include itself.
#define FFX_GPU_PROFILER_AVAILABLE 1

// Maximum number of compute passes tracked per frame slot.
// FSR3 MFG peak per subframe: ~20 OF passes + ~9 FI passes = ~30. 64 is safe.
#define FFX_PROF_MAX_PASSES_PER_FRAME 64

// Maximum label length (wchar_t) for a single pass.
// jobLabel in FfxGpuJobDescription is 64 wchars; we mirror that.
#define FFX_PROF_MAX_LABEL_LEN 64

#ifdef __cplusplus
extern "C" {
#endif

// -------- Frame report (frontend-side consumption) --------

typedef struct FfxProfPassTiming {
    wchar_t  label[FFX_PROF_MAX_LABEL_LEN];
    float    durationMs;   // GPU time in milliseconds, derived via queue timestamp frequency
} FfxProfPassTiming;

typedef struct FfxProfFrameReport {
    int32_t             subframeIdx;     // whatever frontend passed to BeginFrame
    int32_t             subframeCount;
    float               totalMs;         // sum of all pass durations
    uint32_t            passCount;
    // Pointer into internal static storage; valid only until the next call to
    // FfxProf_PopCompletedFrame. Copy out before calling again.
    const FfxProfPassTiming* passes;
} FfxProfFrameReport;

// -------- Lifecycle (both sides) --------

// Idempotent. Safe to call every frame — subsequent calls are no-ops.
// Frontend should call this before the first BeginFrame, once it has the
// ID3D12CommandQueue (pulled from "DLSSG.CmdQueue" NGX parameter).
//
// If d3d12Device or d3d12CommandQueue is NULL, profiler stays disabled and
// all Begin/End calls become no-ops — this is the "graceful fallback" path.
void FfxProf_Init(void* d3d12Device, void* d3d12CommandQueue);

// Called from DLL unload path. Releases query heap, readback buffer, fence.
// Safe to call even if Init was never called.
void FfxProf_Shutdown(void);

// Returns 1 if profiler has been successfully initialized and is enabled,
// 0 otherwise. Useful in hot paths to skip formatting work cheaply.
int FfxProf_IsEnabled(void);

// -------- Per-frame markers (frontend-side) --------

// Opens a new frame slot. Must be paired with EndFrame.
// Call from EvaluateD3D12 *before* invoking dlssgModule->EvaluateD3D12(...).
// subframeIdx/Count are stored verbatim in the eventual FfxProfFrameReport.
//
// Also performs *lazy resolve* of any previously completed frame: the
// ResolveQueryData call for frame N-1 is scheduled on this frame's cmd list.
void FfxProf_BeginFrame(void* d3d12CommandList, int32_t subframeIdx, int32_t subframeCount);

// Closes the currently open frame slot. Must be paired with BeginFrame.
// Call from EvaluateD3D12 *after* dlssgModule->EvaluateD3D12(...) returns.
//
// Note: this does NOT call ResolveQueryData (the cmd list may be Closed()
// already in non-recording mode). Resolve is deferred to the next BeginFrame.
void FfxProf_EndFrame(void* d3d12CommandList);

// -------- Per-pass markers (backend-side) --------

// Writes BEGIN timestamp to query heap. label is copied (snapshot).
// Safe to call outside BeginFrame/EndFrame (becomes no-op).
void FfxProf_BeginPass(void* d3d12CommandList, const wchar_t* label);

// Writes END timestamp. Must be paired with most recent BeginPass.
void FfxProf_EndPass(void* d3d12CommandList);

// -------- Consumption (frontend-side) --------

// Pops the oldest completed frame report if one is available.
// Returns 1 and fills *out on success, 0 if nothing is ready yet.
//
// "Completed" means GPU has signaled the fence for that frame, i.e. all
// timestamps have landed in the readback buffer and been converted to
// milliseconds. Typically returns a frame 2-3 game frames old.
//
// out->passes points into internal static storage and is valid only until
// the next FfxProf_PopCompletedFrame call. Copy out if you need to hold it.
//
// Typical consumer pattern (drain all ready frames each evaluate):
//     FfxProfFrameReport report;
//     while (FfxProf_PopCompletedFrame(&report)) {
//         // log / aggregate / display
//     }
int FfxProf_PopCompletedFrame(FfxProfFrameReport* out);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FFX_GPU_PROFILER_H
