// =============================================================================
// LatencyProbe.cpp - Developer-only latency instrumentation
// =============================================================================
// See LatencyProbe.h for rationale.
//
// REVISION 2 - driven by the first capture (640 frames, real NVIDIA, x6 MFG):
//
//   * Our CPU-side markers reproduced Reflex's own markers at r=1.0000 with
//     MAE 0.002-0.037 ms, so nothing in that layer changed.
//
//   * One GPU timestamp per base frame was wrong. With frame generation the
//     overlay is rendered on every generated Present, so six timestamps were
//     produced per base frame and an arbitrary one survived - error went from
//     +0.7 ms median without FG to -7.6 ms median (std 24.3) with x6. Shifting
//     the attribution by whole frames made it worse, which located the problem
//     inside the base frame rather than between frames. So: keep them all, in
//     numbered columns, and let the data say which one lines up with
//     gpuRenderEndTime instead of guessing.
//
//   * 23% of rows had no Reflex ground truth, concentrated in whole batches
//     (batch 0 and 2 were 64/64 empty). Cause: a single frameReport snapshot
//     was taken at batch close, but the game calls GetLatency rarely, so the
//     snapshot often described a completely different window of frames. Now
//     every GetLatency call feeds a rolling frameID -> FrameReport map, and
//     each batch is held back one batch-period before being written so that
//     late-arriving reports still cover its last frames.
//
//   * Dropped C2/R3 (the game never emits INPUT_SAMPLE - 0 occurrences in 640
//     frames). Split the frameId-mismatch flag by marker origin. mfgMode used
//     to report the override rather than the actual multiplier.
// =============================================================================

#include "LatencyProbe.h"

#if LATENCY_PROBE

#include "Common.h"
#include "../Core/Context.h"

#include <d3d12.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include <unordered_map>
#include <cstdio>
#include <string>

// Same idiom NgxFrontend.cpp uses for ffx_gpu_profiler.h: link opportunistically,
// compile clean when the translation unit is not part of this build.
#if __has_include("NgxFeatureEvents.h")
#include "NgxFeatureEvents.h"
#define LATPROBE_HAS_NGX_EVENTS 1
#endif

using Microsoft::WRL::ComPtr;

namespace LatencyProbe
{
    typedef NV_LATENCY_RESULT_PARAMS::FrameReport FrameReport;

    // =========================================================================
    // Marker types - mirrors FpsMonitor::ReflexMarkerType
    // =========================================================================
    enum ReflexMarkerType
    {
        SIMULATION_START = 0,
        SIMULATION_END = 1,
        RENDERSUBMIT_START = 2,
        RENDERSUBMIT_END = 3,
        PRESENT_START = 4,
        PRESENT_END = 5,
        INPUT_SAMPLE = 6,
        TRIGGER_FLASH = 7,
        PC_LATENCY_PING = 8,
        OUT_OF_BAND_RENDERSUBMIT_START = 9,
        OUT_OF_BAND_RENDERSUBMIT_END = 10,
        OUT_OF_BAND_PRESENT_START = 11,
        OUT_OF_BAND_PRESENT_END = 12
    };

    // =========================================================================
    // Per-frame slot layout
    // =========================================================================

    enum QpcIndex
    {
        Q_INPUT_SAMPLE = 0,
        Q_SIM_START,
        Q_SIM_END,
        Q_RS_START,
        Q_RS_END,
        Q_PRESENT_START,
        Q_PRESENT_END,
        Q_OOB_RS_START,
        Q_OOB_RS_END,
        Q_OOB_PRESENT_START,
        Q_OOB_PRESENT_END,
        Q_TRIGGER_FLASH,
        Q_PC_PING,
        Q_PRE_SLEEP,
        Q_POST_SLEEP,
        Q_PRE_PRESENT,
        Q_POST_PRESENT,
        Q_COUNT
    };

    static const char* const kQpcNames[Q_COUNT] =
    {
        "qpc_inputSample",
        "qpc_simStart",
        "qpc_simEnd",
        "qpc_rsStart",
        "qpc_rsEnd",
        "qpc_presentStart",
        "qpc_presentEnd",
        "qpc_oobRsStart",
        "qpc_oobRsEnd",
        "qpc_oobPresentStart",
        "qpc_oobPresentEnd",
        "qpc_triggerFlash",
        "qpc_pcPing",
        "qpc_preSleep",
        "qpc_postSleep",
        "qpc_prePresent",
        "qpc_postPresent"
    };

    // Up to x6 MFG plus observed jitter (presentCount reached 7 in the capture).
    static const int MAX_GPU_PER_FRAME = 8;

    enum SlotFlags
    {
        F_SYNTH_ID = 0x0001,   // no Reflex markers seen; frameId is ours
        F_PRESENT_FAILED = 0x0002,   // at least one Present returned a failure
        F_ASYNC_SEEN = 0x0004,   // at least one marker arrived via SetAsyncFrameMarker
        F_GPU_OVERFLOW = 0x0008,   // more than MAX_GPU_PER_FRAME timestamps offered
        F_EVICTED = 0x0010,   // closed to make room, not by PRESENT_END
        F_LATE_MARKER = 0x0020,   // a marker arrived after this frame had closed
        F_FG_OWNER_CONFLICT = 0x0040 // a second dispatch group landed on this frame
    };

    struct FrameSlot
    {
        uint64_t baseFrameId;
        uint64_t qpc[Q_COUNT];
        uint64_t gpuTicks[MAX_GPU_PER_FRAME];

        // Frame-generation window, indexed by DLSSG.MultiFrameIndex - 1.
        uint64_t fgCpuPre[MAX_GPU_PER_FRAME];    // QPC, CPU side of the evaluate
        uint64_t fgCpuPost[MAX_GPU_PER_FRAME];   // QPC, CPU side of the evaluate
        uint64_t fgGpuStart[MAX_GPU_PER_FRAME];  // raw GPU ticks
        uint64_t fgGpuEnd[MAX_GPU_PER_FRAME];    // raw GPU ticks

        uint8_t  gpuIssued;    // how many were queued on the GPU
        uint8_t  gpuStored;    // how many came back resolved
        uint8_t  fgIssued;
        uint8_t  fgResolved;

        // Audit of the frame-generation attribution. Logged rather than
        // inferred: which frame the dispatch group was assigned to, and what
        // both marker streams were reporting at the moment of the assignment.
        uint64_t fgOwnerId;
        uint64_t fgSimIdAtLatch;
        uint64_t fgPresentIdAtLatch;
        int16_t  fgOwnerShift;   // frames the group had to be moved forward
        uint64_t fgGroupSeq;     // monotonic id of the generation group
        int16_t  fgOwnerMode;    // 0=PRESENT_END, 1=present-in-progress, 2=latched, 3=shifted

        // Pipeline skew: how many frames ahead simulation already is at the
        // moment THIS frame reaches render-submit and present. Measured at the
        // marker itself, not at frame open - "last id seen when the frame
        // opened" always lags by one for trivial ordering reasons and says
        // nothing about pipelining. Zero means lockstep; CP2077 runs simulation
        // for frame N while the render thread is still on N-1, which shows up
        // here as 1 or more.
        int16_t  skewSimAtRs;
        int16_t  skewSimAtPresent;
        uint16_t presentCount;
        uint32_t markerMask;
        uint16_t flags;
    };

    struct Batch
    {
        FrameSlot   slots[SLOTS_PER_BATCH];
        int         count;

        // Filled by the worker, just before writing, from the rolling map.
        FrameReport reports[SLOTS_PER_BATCH];
        bool        reportMatched[SLOTS_PER_BATCH];

        // GPU <-> QPC clock calibration, sampled once per batch
        bool        hasCalibration;
        uint64_t    calGpuTicks;
        uint64_t    calQpcTicks;
        uint64_t    gpuFrequency;

        // Frame-generation timestamp domain. The DLSSG command list arrives
        // without a queue, so its frequency cannot be queried directly; the game
        // queue's value is reused only when the device matches.
        int         fgSameDevice;
        uint64_t    fgFrequency;

        // Present-queue configuration, sampled from the swapchain we can see.
        // Note this is the wrapper's swapchain; with Streamline the swapchain
        // the driver actually paces on may be a different one.
        int         syncInterval;
        int         bufferCount;
        int         swapEffect;
        int         swapChainFlags;
        int         devMaxFrameLatency;   // IDXGIDevice1, -1 when unavailable
        int         scMaxFrameLatency;    // IDXGISwapChain2, -1 when unavailable

        // Mode context
        int         nvapiEmbedded;
        int         nvapiGenuine;
        int         nvapiMock;
        int         mfgOverride;      // ctx.nvapi.mfgEnforcedMode (user override)
        int         mfgActual;        // framesGenerated + 1 (what actually ran)
        int         framesGenerated;

        uint32_t    batchIndex;
    };

    static const int BATCH_COUNT = 4;

    // =========================================================================
    // State
    // =========================================================================

    static bool                 g_Initialized = false;
    static std::atomic<bool>    g_Active{ false };
    static LARGE_INTEGER        g_QpcFrequency = {};

    static Batch                g_Batches[BATCH_COUNT];
    static int                  g_WriteBatch = 0;
    static uint32_t             g_NextBatchIndex = 0;

    // -------------------------------------------------------------------------
    // Open frames, keyed by frameId
    // -------------------------------------------------------------------------
    //
    // A single "current slot" delimited by SIMULATION_START only works in
    // engines where simulation and render submission run in lockstep. CP2077
    // pipelines them - SIMULATION_START(N) arrives while RENDERSUBMIT_START(N-1)
    // is still in flight - so a slot bounded by SIM_START collects markers from
    // two different frames and every derived timing is off by roughly one frame
    // period. Each marker therefore goes to the slot for ITS OWN frameId.

    static const int OPEN_FRAMES = 16;

    struct OpenFrame
    {
        FrameSlot slot;
        bool      inUse;
        bool      readyToClose;   // PRESENT_END seen; held open for trailing presents
        bool      fgGroupOpen;    // a frame-generation group is mid-flight here
        int       presentDepth;   // Present calls entered and not yet left
        uint64_t  openedQpc;
    };

    // How many frames a completed frame is held open after its PRESENT_END.
    // With frame generation the generated Presents arrive AFTER the Reflex
    // PRESENT_END of the base frame, so closing on that marker throws them away.
    static const uint64_t CLOSE_LAG = 3;

    static OpenFrame            g_Open[OPEN_FRAMES] = {};
    static bool                 g_SawAnyMarker = false;
    static uint64_t             g_SyntheticFrameId = 0;

    // Most recent frameId seen on each stream. Present-time events (Present
    // callbacks, GPU timestamps, frame-generation dispatches) belong to the
    // frame currently being presented, which is NOT necessarily the frame
    // currently being simulated.
    static uint64_t             g_LastSimId = 0;
    static uint64_t             g_LastRsId = 0;
    static uint64_t             g_LastPresentId = 0;

    // Frame whose PRESENT_END fired most recently. Measured, not assumed: across
    // the CP2077 captures the first NGX generation dispatch of a group follows
    // its own frame's PRESENT_END by +0.21 ms at x4 (99.7% of groups) and
    // +6.41 ms at x6 (100%), and lands inside the [PRESENT_START, PRESENT_END]
    // window essentially never. "Which frame just finished presenting" is a
    // different question from "which frame is newest in the present stream" and
    // from "whose Present are we inside" - both of those were wrong here.
    static uint64_t             g_LastPresentEndId = 0;

    // Owner of present-time work. Latched from the Present call sequence rather
    // than read live from the marker streams: PRESENT_START of the NEXT frame
    // can fire while the presenter thread is still flushing this frame's
    // generated Presents, and "most recent marker id" then hands the dispatch to
    // the wrong frame. Two different groups landing on one frame overwrite each
    // other at the same MultiFrameIndex, which is what emptied 38% of frames.
    static uint64_t             g_PresentOwnerId = 0;

    // Owner of the current frame-generation group, latched at MultiFrameIndex==1
    // and reused for the rest of the group so a mid-group marker change cannot
    // split one group across two frames.
    static uint64_t             g_FgOwnerId = 0;

    static uint64_t              g_FgGroupSeq = 0;
    static std::atomic<uint64_t> g_FgOwnerConflicts{ 0 };
    static std::atomic<uint64_t> g_LateMarkers{ 0 };
    static std::atomic<uint64_t> g_EvictedFrames{ 0 };

    // Present-queue configuration cache. Re-sampled periodically rather than
    // every Present: the game can change maximum frame latency at runtime, but
    // not often enough to justify three COM calls per frame.
    struct PresentConfig
    {
        std::atomic<int> syncInterval{ 0 };
        std::atomic<int> bufferCount{ 0 };
        std::atomic<int> swapEffect{ -1 };
        std::atomic<int> flags{ 0 };
        std::atomic<int> devMaxFrameLatency{ -1 };
        std::atomic<int> scMaxFrameLatency{ -1 };
        void* lastSwapChain = nullptr;
        uint32_t         sinceSample = 0;
    };
    static PresentConfig g_Present;
    static std::mutex    g_PresentMutex;

    // -------------------------------------------------------------------------
    // Production latency output
    // -------------------------------------------------------------------------
    static std::atomic<double>   g_LatencyMs{ 0.0 };
    static std::atomic<uint64_t> g_LatencyQpc{ 0 };   // when it was last updated
    static uint64_t              g_PrevGpuEndQpc = 0; // previous frame's GPU end

    static std::atomic<uint64_t> g_DroppedBatches{ 0 };
    static std::atomic<uint64_t> g_OrphanGpuTimestamps{ 0 };

    // -------------------------------------------------------------------------
    // Spinlock - held for a handful of stores, never across I/O or D3D calls.
    // -------------------------------------------------------------------------
    class SpinLock
    {
    public:
        void lock()
        {
            while (m_Flag.test_and_set(std::memory_order_acquire))
                YieldProcessor();
        }
        void unlock()
        {
            m_Flag.clear(std::memory_order_release);
        }
    private:
        std::atomic_flag m_Flag = ATOMIC_FLAG_INIT;
    };

    static SpinLock g_SlotLock;

    struct SpinGuard
    {
        SpinGuard(SpinLock& l) : m_Lock(l) { m_Lock.lock(); }
        ~SpinGuard() { m_Lock.unlock(); }
        SpinLock& m_Lock;
    };

    // -------------------------------------------------------------------------
    // Rolling frameID -> FrameReport map
    // -------------------------------------------------------------------------
    //
    // Every GetLatency call hands us 64 frames of Reflex history. Keeping only
    // the newest snapshot threw most of that away and left whole batches
    // without ground truth. Accumulating instead means a game that calls
    // GetLatency even once every couple of seconds still yields near-complete
    // coverage.

    static std::mutex                                   g_ReportMutex;
    static std::unordered_map<uint64_t, FrameReport>    g_ReportMap;
    static int                                          g_LastReportStatus = 0;
    static uint64_t                                     g_LastReportQpc = 0;
    static uint64_t                                     g_ReportCalls = 0;

    static const size_t REPORT_MAP_LIMIT = 4096;

    // -------------------------------------------------------------------------
    // Worker thread
    // -------------------------------------------------------------------------
    static std::thread                  g_Worker;
    static std::mutex                   g_QueueMutex;
    static std::condition_variable      g_QueueCv;
    static std::deque<int>              g_FlushQueue;
    static bool                         g_Quit = false;

    // =========================================================================
    // GPU timestamp state
    // =========================================================================

    static const int GPU_SLOTS = 32;   // x6 MFG needs depth; 8 was too shallow

    struct GpuPending
    {
        uint64_t fenceValue;
        uint64_t frameId;
        uint8_t  seq;
        bool     inUse;
    };

    struct GpuState
    {
        ComPtr<ID3D12QueryHeap>  queryHeap;
        ComPtr<ID3D12Resource>   readback;
        ComPtr<ID3D12Fence>      fence;
        uint64_t* mapped = nullptr;
        uint64_t                 fenceValue = 0;
        uint64_t                 frequency = 0;
        int                      writeIndex = 0;
        GpuPending               pending[GPU_SLOTS] = {};
        bool                     initialized = false;
        bool                     initFailed = false;

        std::atomic<uint64_t>    calGpu{ 0 };
        std::atomic<uint64_t>    calQpc{ 0 };
        uint32_t                 framesSinceCalibration = 0;
    };

    static GpuState   g_Gpu;
    static std::mutex g_GpuMutex;

    // -------------------------------------------------------------------------
    // Frame-generation query state
    // -------------------------------------------------------------------------
    //
    // The DLSSG evaluate hands us a command list and nothing else. No queue
    // means no fence, so readback cannot be gated on completion the way the
    // overlay path is. Instead each resolved pair ages for FG_READBACK_LAG
    // further evaluates before being read, and obviously-bad values (zero,
    // non-monotonic, implausible duration) are discarded rather than logged as
    // if they were measurements.

    static const int FG_SLOTS = 128;          // pairs, so 64 evaluates in flight
    static const int FG_READBACK_LAG = 16;    // evaluates, ~3 base frames at x6

    struct FgPending
    {
        uint64_t frameId;
        uint8_t  sub;        // MultiFrameIndex - 1
        int      idxStart;   // idxEnd == idxStart + 1
        int      age;
        bool     resolved;   // Post ran, ResolveQueryData submitted
        bool     inUse;
    };

    struct FgState
    {
        ComPtr<ID3D12Device>     device;
        ComPtr<ID3D12QueryHeap>  queryHeap;
        ComPtr<ID3D12Resource>   readback;
        uint64_t* mapped = nullptr;
        int                      writeIndex = 0;
        FgPending                pending[FG_SLOTS / 2] = {};
        bool                     initialized = false;
        bool                     initFailed = false;
        int                      sameDevice = 0;
        int                      openIdx = -1;   // pair opened by Pre, closed by Post
        std::atomic<uint64_t>    discarded{ 0 };
    };

    static FgState    g_Fg;
    static std::mutex g_FgMutex;

    // =========================================================================
    // Helpers
    // =========================================================================

    static inline uint64_t NowQpc()
    {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        return (uint64_t)li.QuadPart;
    }

    static inline double QpcToMs(uint64_t a, uint64_t b)
    {
        if (a == 0 || b == 0 || b < a || g_QpcFrequency.QuadPart == 0)
            return 0.0;
        return (double)(b - a) * 1000.0 / (double)g_QpcFrequency.QuadPart;
    }

    static int QpcIndexForMarker(uint32_t markerType)
    {
        switch (markerType)
        {
        case SIMULATION_START:               return Q_SIM_START;
        case SIMULATION_END:                 return Q_SIM_END;
        case RENDERSUBMIT_START:             return Q_RS_START;
        case RENDERSUBMIT_END:               return Q_RS_END;
        case PRESENT_START:                  return Q_PRESENT_START;
        case PRESENT_END:                    return Q_PRESENT_END;
        case INPUT_SAMPLE:                   return Q_INPUT_SAMPLE;
        case TRIGGER_FLASH:                  return Q_TRIGGER_FLASH;
        case PC_LATENCY_PING:                return Q_PC_PING;
        case OUT_OF_BAND_RENDERSUBMIT_START: return Q_OOB_RS_START;
        case OUT_OF_BAND_RENDERSUBMIT_END:   return Q_OOB_RS_END;
        case OUT_OF_BAND_PRESENT_START:      return Q_OOB_PRESENT_START;
        case OUT_OF_BAND_PRESENT_END:        return Q_OOB_PRESENT_END;
        default:                             return -1;
        }
    }

    // Must be called with g_SlotLock held. Pushes the frame into the current
    // batch and frees its table entry.
    static void CloseFrameLocked(OpenFrame* of)
    {
        if (!of || !of->inUse)
            return;

        Batch& batch = g_Batches[g_WriteBatch];

        if (batch.count < SLOTS_PER_BATCH)
        {
            batch.slots[batch.count] = of->slot;
            batch.count++;
        }

        of->inUse = false;

        if (batch.count < SLOTS_PER_BATCH)
            return;

        // ---- Batch is full ----

        batch.batchIndex = g_NextBatchIndex++;

        batch.syncInterval = g_Present.syncInterval.load(std::memory_order_relaxed);
        batch.bufferCount = g_Present.bufferCount.load(std::memory_order_relaxed);
        batch.swapEffect = g_Present.swapEffect.load(std::memory_order_relaxed);
        batch.swapChainFlags = g_Present.flags.load(std::memory_order_relaxed);
        batch.devMaxFrameLatency = g_Present.devMaxFrameLatency.load(std::memory_order_relaxed);
        batch.scMaxFrameLatency = g_Present.scMaxFrameLatency.load(std::memory_order_relaxed);

        batch.fgSameDevice = g_Fg.sameDevice;
        batch.fgFrequency = g_Fg.sameDevice ? g_Gpu.frequency : 0;

        batch.nvapiEmbedded = ctx.nvapi.isEmbeddedNvapiUsed ? 1 : 0;
        batch.nvapiGenuine = ctx.nvapi.isGenuineFileLoaded ? 1 : 0;
        batch.nvapiMock = ctx.nvapi.isMockEnabled ? 1 : 0;
        batch.mfgOverride = (int)ctx.nvapi.mfgEnforcedMode;
        batch.framesGenerated = (int)ctx.ngx.framesGenerated;
        batch.mfgActual = batch.framesGenerated + 1;

        batch.hasCalibration = false;
        {
            uint64_t calGpu = g_Gpu.calGpu.load(std::memory_order_relaxed);
            uint64_t calQpc = g_Gpu.calQpc.load(std::memory_order_relaxed);
            if (calGpu != 0 && calQpc != 0)
            {
                batch.calGpuTicks = calGpu;
                batch.calQpcTicks = calQpc;
                batch.gpuFrequency = g_Gpu.frequency;
                batch.hasCalibration = true;
            }
        }

        int shipped = g_WriteBatch;
        g_WriteBatch = (g_WriteBatch + 1) % BATCH_COUNT;
        g_Batches[g_WriteBatch].count = 0;

#if LATENCY_PROBE_CSV
        {
            std::lock_guard<std::mutex> lock(g_QueueMutex);
            if (g_FlushQueue.size() >= (size_t)(BATCH_COUNT - 1))
                g_DroppedBatches.fetch_add(1, std::memory_order_relaxed);
            else
                g_FlushQueue.push_back(shipped);
        }
        g_QueueCv.notify_one();
#else
        (void)shipped;
#endif
    }

    // Must be called with g_SlotLock held.
    static OpenFrame* FindOpenLocked(uint64_t frameId)
    {
        for (int i = 0; i < OPEN_FRAMES; i++)
        {
            if (g_Open[i].inUse && g_Open[i].slot.baseFrameId == frameId)
                return &g_Open[i];
        }
        return nullptr;
    }

    // Must be called with g_SlotLock held. Opens the frame if it is not already
    // tracked, evicting the oldest entry when the table is full.
    static OpenFrame* GetOrOpenLocked(uint64_t frameId, bool synthetic, uint64_t now)
    {
        OpenFrame* of = FindOpenLocked(frameId);
        if (of)
            return of;

        OpenFrame* free = nullptr;
        for (int i = 0; i < OPEN_FRAMES; i++)
        {
            if (!g_Open[i].inUse) { free = &g_Open[i]; break; }
        }

        if (!free)
        {
            // Table full: a frame never received PRESENT_END. Close the oldest
            // rather than dropping the new one - a truncated frame in the log is
            // more useful than a missing one, and the flag says which it is.
            OpenFrame* oldest = &g_Open[0];
            for (int i = 1; i < OPEN_FRAMES; i++)
            {
                if (g_Open[i].openedQpc < oldest->openedQpc)
                    oldest = &g_Open[i];
            }
            oldest->fgGroupOpen = false;
            oldest->slot.flags |= F_EVICTED;
            g_EvictedFrames.fetch_add(1, std::memory_order_relaxed);
            CloseFrameLocked(oldest);
            free = oldest;
        }

        free->slot = {};
        free->slot.baseFrameId = frameId;
        if (synthetic)
            free->slot.flags |= F_SYNTH_ID;
        free->inUse = true;
        free->openedQpc = now;
        free->readyToClose = false;
        free->fgGroupOpen = false;
        free->presentDepth = 0;

        return free;
    }

    // Close every completed frame that is far enough behind. Frames go out in
    // ascending frameId order so the CSV stays in frame order.
    // Must be called with g_SlotLock held.
    static void SweepClosedLocked(uint64_t upToFrameId)
    {
        for (;;)
        {
            OpenFrame* pick = nullptr;
            for (int i = 0; i < OPEN_FRAMES; i++)
            {
                OpenFrame& of = g_Open[i];
                if (!of.inUse || !of.readyToClose)
                    continue;
                if (of.slot.baseFrameId + CLOSE_LAG > upToFrameId)
                    continue;
                // Never close a frame whose generation group is still running -
                // the remaining subframes would find no slot and be dropped,
                // which is what left frames holding a single dispatch.
                if (of.fgGroupOpen)
                    continue;
                if (!pick || of.slot.baseFrameId < pick->slot.baseFrameId)
                    pick = &of;
            }
            if (!pick)
                return;
            CloseFrameLocked(pick);
        }
    }

    // The frame whose Present is executing right now, tracked with an entry
    // counter rather than by inspecting the pre/post timestamps: with frame
    // generation there are N Present calls per base frame and they all write the
    // same two fields, so "has pre, lacks post" could only ever be true during
    // the first of them.
    //
    // Generation dispatches happen
    // inside that call, so this identifies their owner directly, without going
    // through the marker streams at all.
    //
    // This matters because the streams can be badly out of step: when
    // generation dominates the frame budget, GPU work for a group starts three
    // to four base-frame periods after its own simulation, and "most recent
    // present marker" then points at a completely different frame.
    // Must be called with g_SlotLock held.
    static OpenFrame* PresentInProgressLocked()
    {
        OpenFrame* best = nullptr;
        for (int i = 0; i < OPEN_FRAMES; i++)
        {
            OpenFrame& of = g_Open[i];
            if (!of.inUse) continue;
            if (of.presentDepth <= 0) continue;
            // Concurrent presents: the newest frame wins.
            if (!best || of.slot.baseFrameId > best->slot.baseFrameId)
                best = &of;
        }
        return best;
    }

    // The frame that present-time work belongs to: whatever the present stream
    // last reported, falling back to simulation when a game emits no present
    // markers at all.
    static uint64_t PresentSideFrameIdLocked()
    {
        if (g_LastPresentId != 0 && FindOpenLocked(g_LastPresentId))
            return g_LastPresentId;
        if (g_LastSimId != 0 && FindOpenLocked(g_LastSimId))
            return g_LastSimId;
        return 0;
    }

    // =========================================================================
    // Producers
    // =========================================================================

    void OnMarker(uint64_t frameId, uint32_t markerType, bool isAsync)
    {
        if (!g_Active.load(std::memory_order_relaxed))
            return;

        const uint64_t now = NowQpc();
        const int qi = QpcIndexForMarker(markerType);

        SpinGuard guard(g_SlotLock);

        g_SawAnyMarker = true;

        // Track each stream separately - they run at different frame indices in
        // any engine that pipelines simulation against render submission.
        switch (markerType)
        {
        case SIMULATION_START:               g_LastSimId = frameId; break;
        case RENDERSUBMIT_START:             g_LastRsId = frameId; break;
        case PRESENT_START:
        case OUT_OF_BAND_PRESENT_START:      g_LastPresentId = frameId; break;
        case PRESENT_END:
        case OUT_OF_BAND_PRESENT_END:        g_LastPresentEndId = frameId; break;
        default: break;
        }

        OpenFrame* of = FindOpenLocked(frameId);

        if (!of)
        {
            // Only the first marker of a frame opens it. A marker arriving for a
            // frame that already closed is counted, not silently folded into
            // whatever happens to be open - that folding was the bug this whole
            // structure replaces.
            if (markerType == SIMULATION_START || markerType == INPUT_SAMPLE ||
                markerType == RENDERSUBMIT_START || markerType == PRESENT_START ||
                markerType == OUT_OF_BAND_PRESENT_START)
            {
                of = GetOrOpenLocked(frameId, false, now);
            }
            else
            {
                of = FindOpenLocked(frameId);
                if (!of)
                {
                    g_LateMarkers.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }
        }

        if (isAsync)
            of->slot.flags |= F_ASYNC_SEEN;

        if (qi >= 0)
        {
            of->slot.qpc[qi] = now;
            of->slot.markerMask |= (1u << qi);
        }

        // Pipeline skew, captured at the marker that reveals it.
        if (frameId != 0 && g_LastSimId != 0)
        {
            const int16_t skew = (int16_t)((int64_t)g_LastSimId - (int64_t)frameId);
            if (markerType == RENDERSUBMIT_START)
                of->slot.skewSimAtRs = skew;
            else if (markerType == PRESENT_START)
                of->slot.skewSimAtPresent = skew;
        }

        // PRESENT_END completes the Reflex frame, but not our record of it: with
        // frame generation the generated Presents still arrive after this point.
        // Mark it done and let the sweep close it CLOSE_LAG frames later.
        if (markerType == PRESENT_END)
        {
            of->readyToClose = true;
            SweepClosedLocked(frameId);
        }
    }

    // Sample buffer count, swap effect and both flavours of maximum frame
    // latency. The device-level value (IDXGIDevice1) is the one that applies to
    // ordinary swapchains; the swapchain-level one (IDXGISwapChain2) only
    // answers for waitable swapchains and returns an error otherwise, so both
    // are recorded separately instead of collapsing them into one number.
    static void SamplePresentConfig(IDXGISwapChain* pSwapChain)
    {
        if (!pSwapChain)
            return;

        std::lock_guard<std::mutex> lock(g_PresentMutex);

        const bool changed = (g_Present.lastSwapChain != (void*)pSwapChain);
        if (!changed && ++g_Present.sinceSample < 64)
            return;

        g_Present.lastSwapChain = (void*)pSwapChain;
        g_Present.sinceSample = 0;

        DXGI_SWAP_CHAIN_DESC desc = {};
        if (SUCCEEDED(pSwapChain->GetDesc(&desc)))
        {
            g_Present.bufferCount.store((int)desc.BufferCount, std::memory_order_relaxed);
            g_Present.swapEffect.store((int)desc.SwapEffect, std::memory_order_relaxed);
            g_Present.flags.store((int)desc.Flags, std::memory_order_relaxed);
        }

        int devLatency = -1;
        {
            ComPtr<IDXGIDevice1> dxgiDevice;
            if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&dxgiDevice))) && dxgiDevice)
            {
                UINT n = 0;
                if (SUCCEEDED(dxgiDevice->GetMaximumFrameLatency(&n)))
                    devLatency = (int)n;
            }
        }
        g_Present.devMaxFrameLatency.store(devLatency, std::memory_order_relaxed);

        int scLatency = -1;
        {
            ComPtr<IDXGISwapChain2> sc2;
            if (SUCCEEDED(pSwapChain->QueryInterface(IID_PPV_ARGS(&sc2))) && sc2)
            {
                UINT n = 0;
                // Only legal on waitable swapchains; failure here is information,
                // not an error, and is recorded as -1.
                if (SUCCEEDED(sc2->GetMaximumFrameLatency(&n)))
                    scLatency = (int)n;
            }
        }
        g_Present.scMaxFrameLatency.store(scLatency, std::memory_order_relaxed);
    }

    void OnPrePresent(IDXGISwapChain* pSwapChain, unsigned syncInterval)
    {
        if (!g_Active.load(std::memory_order_relaxed))
            return;

        g_Present.syncInterval.store((int)syncInterval, std::memory_order_relaxed);
        SamplePresentConfig(pSwapChain);

        const uint64_t now = NowQpc();

        SpinGuard guard(g_SlotLock);

        uint64_t id = PresentSideFrameIdLocked();
        if (id == 0)
        {
            if (g_SawAnyMarker)
                return;
            id = ++g_SyntheticFrameId;
            GetOrOpenLocked(id, true, now);
        }

        OpenFrame* of = FindOpenLocked(id);
        if (!of)
            return;

        // Latch only when the present stream actually advanced to a new frame.
        // With frame generation this callback fires once per GENERATED present
        // too; re-latching on those let the owner drift to the next base frame
        // mid-sequence, which is how dispatch groups ended up colliding.
        if (id != g_PresentOwnerId && FindOpenLocked(id))
            g_PresentOwnerId = id;

        of->presentDepth++;

        of->slot.qpc[Q_PRE_PRESENT] = now;
        of->slot.markerMask |= (1u << Q_PRE_PRESENT);
    }

    void OnPostPresent(HRESULT result)
    {
        if (!g_Active.load(std::memory_order_relaxed))
            return;

        const uint64_t now = NowQpc();

        SpinGuard guard(g_SlotLock);

        uint64_t id = PresentSideFrameIdLocked();
        if (id == 0)
        {
            if (g_SawAnyMarker)
                return;
            id = g_SyntheticFrameId;
        }

        OpenFrame* of = FindOpenLocked(id);
        if (!of)
            return;

        if (of->presentDepth > 0)
            of->presentDepth--;

        of->slot.qpc[Q_POST_PRESENT] = now;
        of->slot.markerMask |= (1u << Q_POST_PRESENT);
        of->slot.presentCount++;

        if (FAILED(result))
            of->slot.flags |= F_PRESENT_FAILED;

        // Without Reflex markers there is no PRESENT_END to close on, so Present
        // becomes the boundary.
        if (!g_SawAnyMarker)
            CloseFrameLocked(of);
    }

    void OnPreSleep()
    {
        if (!g_Active.load(std::memory_order_relaxed))
            return;

        const uint64_t now = NowQpc();

        SpinGuard guard(g_SlotLock);

        // Sleep belongs to the frame about to be simulated.
        OpenFrame* of = (g_LastSimId != 0) ? FindOpenLocked(g_LastSimId) : nullptr;
        if (!of)
            return;

        of->slot.qpc[Q_PRE_SLEEP] = now;
        of->slot.markerMask |= (1u << Q_PRE_SLEEP);
    }

    void OnPostSleep()
    {
        if (!g_Active.load(std::memory_order_relaxed))
            return;

        const uint64_t now = NowQpc();

        SpinGuard guard(g_SlotLock);

        OpenFrame* of = (g_LastSimId != 0) ? FindOpenLocked(g_LastSimId) : nullptr;
        if (!of)
            return;

        of->slot.qpc[Q_POST_SLEEP] = now;
        of->slot.markerMask |= (1u << Q_POST_SLEEP);
    }

    void OnGetLatency(const NV_LATENCY_RESULT_PARAMS* pParams, int result)
    {
        if (!g_Active.load(std::memory_order_relaxed) || !pParams)
            return;

        const uint64_t now = NowQpc();

        std::lock_guard<std::mutex> lock(g_ReportMutex);

        g_LastReportStatus = result;
        g_LastReportQpc = now;
        g_ReportCalls++;

        // Absorb the whole history, not just the newest entry. Entries with a
        // zero frameID are unpopulated slots in the driver's ring.
        for (int i = 0; i < 64; i++)
        {
            const FrameReport& fr = pParams->frameReport[i];
            if (fr.frameID == 0)
                continue;
            g_ReportMap[fr.frameID] = fr;
        }

        if (g_ReportMap.size() > REPORT_MAP_LIMIT)
        {
            // Drop the oldest half. frameID is monotonic, so a threshold works.
            uint64_t maxId = 0;
            for (const auto& kv : g_ReportMap)
            {
                if (kv.first > maxId)
                    maxId = kv.first;
            }

            const uint64_t cutoff = (maxId > REPORT_MAP_LIMIT / 2)
                ? maxId - REPORT_MAP_LIMIT / 2 : 0;

            for (auto it = g_ReportMap.begin(); it != g_ReportMap.end(); )
            {
                if (it->first < cutoff)
                    it = g_ReportMap.erase(it);
                else
                    ++it;
            }
        }
    }

    // =========================================================================
    // GPU timestamps
    // =========================================================================

    static bool InitGpuLocked(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue)
    {
        if (g_Gpu.initialized)
            return true;
        if (g_Gpu.initFailed || !pDevice || !pQueue)
            return false;

        HRESULT hr = pQueue->GetTimestampFrequency(&g_Gpu.frequency);
        if (FAILED(hr) || g_Gpu.frequency == 0)
        {
            g_Gpu.initFailed = true;
            LOG_WARNING(L"[LatProbe] GetTimestampFrequency failed - GPU timestamps disabled");
            return false;
        }

        D3D12_QUERY_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        heapDesc.Count = GPU_SLOTS;
        heapDesc.NodeMask = 0;

        hr = pDevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&g_Gpu.queryHeap));
        if (FAILED(hr))
        {
            g_Gpu.initFailed = true;
            LOG_WARNING(L"[LatProbe] CreateQueryHeap failed - GPU timestamps disabled");
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = sizeof(uint64_t) * GPU_SLOTS;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = pDevice->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE,
            &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&g_Gpu.readback));
        if (FAILED(hr))
        {
            g_Gpu.initFailed = true;
            LOG_WARNING(L"[LatProbe] Readback buffer creation failed - GPU timestamps disabled");
            return false;
        }

        D3D12_RANGE noRead = { 0, 0 };
        hr = g_Gpu.readback->Map(0, &noRead, reinterpret_cast<void**>(&g_Gpu.mapped));
        if (FAILED(hr) || !g_Gpu.mapped)
        {
            g_Gpu.initFailed = true;
            LOG_WARNING(L"[LatProbe] Readback Map failed - GPU timestamps disabled");
            return false;
        }

        hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_Gpu.fence));
        if (FAILED(hr))
        {
            g_Gpu.initFailed = true;
            LOG_WARNING(L"[LatProbe] CreateFence failed - GPU timestamps disabled");
            return false;
        }

        g_Gpu.initialized = true;
        LOG_INFO(L"[LatProbe] GPU timestamps initialized (freq=" + std::to_wstring(g_Gpu.frequency) + L")");
        return true;
    }

    void D3D12_OnCommandList(ID3D12Device* pDevice,
        ID3D12CommandQueue* pQueue,
        ID3D12GraphicsCommandList* pCmdList)
    {
        if (!g_Active.load(std::memory_order_relaxed) || !pCmdList)
            return;

        std::lock_guard<std::mutex> lock(g_GpuMutex);

        if (!InitGpuLocked(pDevice, pQueue))
            return;

        // Which base frame, and which Present within it. With frame generation
        // this runs several times per base frame - all of them are kept, in
        // order, so that the one matching gpuRenderEndTime can be identified
        // from the data rather than assumed.
        uint64_t frameId = 0;
        uint8_t  seq = 0;
        {
            SpinGuard guard(g_SlotLock);

            // The overlay renders inside the game's Present, so this timestamp
            // belongs to the frame whose Present is in progress - latched when
            // that Present started, not re-read from the marker streams now.
            frameId = g_PresentOwnerId;
            if (frameId == 0)
                return;

            OpenFrame* of = FindOpenLocked(frameId);
            if (!of)
                return;

            if (of->slot.gpuIssued >= MAX_GPU_PER_FRAME)
            {
                of->slot.flags |= F_GPU_OVERFLOW;
                return;
            }
            seq = of->slot.gpuIssued++;
        }

        if (frameId == 0)
            return;

        const int idx = g_Gpu.writeIndex;

        pCmdList->EndQuery(g_Gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, idx);
        pCmdList->ResolveQueryData(g_Gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            idx, 1, g_Gpu.readback.Get(), sizeof(uint64_t) * idx);

        g_Gpu.pending[idx].fenceValue = g_Gpu.fenceValue + 1;
        g_Gpu.pending[idx].frameId = frameId;
        g_Gpu.pending[idx].seq = seq;
        g_Gpu.pending[idx].inUse = true;

        g_Gpu.writeIndex = (g_Gpu.writeIndex + 1) % GPU_SLOTS;
    }

    // Turn a slot's newest resolved GPU timestamp into the published latency.
    // Called once the timestamp lands, which is when the frame's data actually
    // becomes complete - closing the frame happens earlier than that.
    // Must be called with g_SlotLock held.
    static void PublishLatencyLocked(const FrameSlot& slot)
    {
        const uint64_t simStart = slot.qpc[Q_SIM_START];
        if (simStart == 0)
            return;

        const uint64_t calGpu = g_Gpu.calGpu.load(std::memory_order_relaxed);
        const uint64_t calQpc = g_Gpu.calQpc.load(std::memory_order_relaxed);
        const uint64_t gpuFreq = g_Gpu.frequency;
        if (calGpu == 0 || calQpc == 0 || gpuFreq == 0)
            return;

        // With frame generation the end of the LAST generated dispatch is the
        // frame's GPU end; without it, the overlay timestamp is.
        uint64_t ticks = 0;
        for (int i = MAX_GPU_PER_FRAME - 1; i >= 0; i--)
        {
            if (slot.fgGpuEnd[i] != 0) { ticks = slot.fgGpuEnd[i]; break; }
        }
        if (ticks == 0)
            ticks = slot.gpuTicks[0];
        if (ticks == 0)
            return;

        const int64_t deltaGpu = (int64_t)ticks - (int64_t)calGpu;
        const int64_t deltaQpc = (int64_t)((double)deltaGpu
            * (double)g_QpcFrequency.QuadPart / (double)gpuFreq);
        const int64_t endSigned = (int64_t)calQpc + deltaQpc;
        if (endSigned <= (int64_t)simStart)
            return;
        const uint64_t gpuEndQpc = (uint64_t)endSigned;

        // ---------------------------------------------------------------------
        // Pipelined latency, the quantity a player actually feels.
        // ---------------------------------------------------------------------
        //
        // Reproduces what native Reflex reports:
        //
        //     simDelta + osRenderQueueDelta + gpuRenderDelta + presentDelta
        //
        // These stages overlap in wall-clock time, so the sum is NOT the span of
        // one frame - it is the distance an input travels through the pipeline,
        // because every frame waits behind the previous one at each stage. The
        // single-frame span (gpuEnd - simStart) is a different, smaller number
        // and is not what the readout should show.
        //
        // Each term measured against the NVAPI report on real hardware:
        //     simDelta   MAE 0.002 ms
        //     osqDelta   MAE 0.519 ms   (osqStart lands on our rsEnd, osqEnd == gpuEnd)
        //     gpuDelta   MAE 0.396 ms   (95.9% within 2 ms)
        //     presDelta  MAE 0.038 ms
        // Sum: median -0.212 ms, MAE 0.783 ms against a 110-160 ms quantity.

        const double simD = QpcToMs(simStart, slot.qpc[Q_SIM_END]);
        const double presD = QpcToMs(slot.qpc[Q_PRESENT_START], slot.qpc[Q_PRESENT_END]);

        // The render queue opens when the game finishes submitting and closes
        // when the GPU is done - the report's osqEnd and gpuEnd are the same
        // timestamp in 99.85% of frames. RENDERSUBMIT_END is the marker that
        // lands on osqStart; PRESENT_START sits within half a millisecond of it
        // and covers games that never emit the render-submit pair.
        uint64_t queueStart = slot.qpc[Q_RS_END];
        if (queueStart == 0)
            queueStart = slot.qpc[Q_PRESENT_START];

        const double osqD = QpcToMs(queueStart, gpuEndQpc);

        // GPU busy time. While the GPU never goes idle, that is simply the gap
        // between consecutive frame completions; the queue length bounds it from
        // above for the case where it does idle.
        double gpuD = osqD;
        if (g_PrevGpuEndQpc != 0 && gpuEndQpc > g_PrevGpuEndQpc)
        {
            const double gap = QpcToMs(g_PrevGpuEndQpc, gpuEndQpc);
            if (gap > 0.0 && gap < gpuD)
                gpuD = gap;
        }
        g_PrevGpuEndQpc = gpuEndQpc;

        const double ms = simD + osqD + gpuD + presD;

        // Reject values that cannot be a frame latency rather than letting a
        // stray timestamp yank the readout around.
        if (ms <= 0.0 || ms > 2000.0)
            return;

        const double prev = g_LatencyMs.load(std::memory_order_relaxed);
        const double smoothed = (prev > 0.0) ? (prev * 0.85 + ms * 0.15) : ms;
        g_LatencyMs.store(smoothed, std::memory_order_relaxed);
        g_LatencyQpc.store(NowQpc(), std::memory_order_relaxed);
    }

    static void AttachGpuTimestamp(uint64_t frameId, uint8_t seq, uint64_t ticks)
    {
        if (seq >= MAX_GPU_PER_FRAME)
            return;

        SpinGuard guard(g_SlotLock);

        FrameSlot* target = nullptr;

        if (OpenFrame* of = FindOpenLocked(frameId))
        {
            target = &of->slot;
        }
        else
        {
            Batch& batch = g_Batches[g_WriteBatch];
            for (int i = batch.count - 1; i >= 0; i--)
            {
                if (batch.slots[i].baseFrameId == frameId)
                {
                    target = &batch.slots[i];
                    break;
                }
            }
        }

        if (!target)
        {
            g_OrphanGpuTimestamps.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        target->gpuTicks[seq] = ticks;
        if (seq + 1 > target->gpuStored)
            target->gpuStored = (uint8_t)(seq + 1);

        // Only the no-frame-generation case publishes from here. With generation
        // active this timestamp comes from the FIRST Present of the frame, which
        // at x6 sits five subframes before the frame's real GPU end - publishing
        // it would mix two different quantities into one average.
        if (target->fgIssued == 0)
            PublishLatencyLocked(*target);
    }

    void D3D12_OnSubmitted(ID3D12CommandQueue* pQueue)
    {
        if (!g_Active.load(std::memory_order_relaxed) || !pQueue)
            return;

        std::lock_guard<std::mutex> lock(g_GpuMutex);

        if (!g_Gpu.initialized)
            return;

        pQueue->Signal(g_Gpu.fence.Get(), ++g_Gpu.fenceValue);

        const uint64_t completed = g_Gpu.fence->GetCompletedValue();

        for (int i = 0; i < GPU_SLOTS; i++)
        {
            if (!g_Gpu.pending[i].inUse)
                continue;
            if (g_Gpu.pending[i].fenceValue > completed)
                continue;

            const uint64_t ticks = g_Gpu.mapped ? g_Gpu.mapped[i] : 0;
            const uint64_t frameId = g_Gpu.pending[i].frameId;
            const uint8_t  seq = g_Gpu.pending[i].seq;
            g_Gpu.pending[i].inUse = false;

            if (ticks != 0)
                AttachGpuTimestamp(frameId, seq, ticks);
        }

        if (++g_Gpu.framesSinceCalibration >= (uint32_t)SLOTS_PER_BATCH)
        {
            g_Gpu.framesSinceCalibration = 0;

            uint64_t gpuTicks = 0, cpuTicks = 0;
            if (SUCCEEDED(pQueue->GetClockCalibration(&gpuTicks, &cpuTicks)))
            {
                g_Gpu.calGpu.store(gpuTicks, std::memory_order_relaxed);
                g_Gpu.calQpc.store(cpuTicks, std::memory_order_relaxed);
            }
        }
    }


    // =========================================================================
    // Frame-generation GPU window
    // =========================================================================

    static bool InitFgLocked(ID3D12GraphicsCommandList* pCmdList)
    {
        if (g_Fg.initialized)
            return true;
        if (g_Fg.initFailed || !pCmdList)
            return false;

        if (FAILED(pCmdList->GetDevice(IID_PPV_ARGS(&g_Fg.device))) || !g_Fg.device)
        {
            g_Fg.initFailed = true;
            LOG_WARNING(L"[LatProbe] FG: GetDevice failed - frame-gen timestamps disabled");
            return false;
        }

        D3D12_QUERY_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        heapDesc.Count = FG_SLOTS;
        heapDesc.NodeMask = 0;

        if (FAILED(g_Fg.device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&g_Fg.queryHeap))))
        {
            g_Fg.initFailed = true;
            LOG_WARNING(L"[LatProbe] FG: CreateQueryHeap failed - frame-gen timestamps disabled");
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = sizeof(uint64_t) * FG_SLOTS;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(g_Fg.device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE,
            &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&g_Fg.readback))))
        {
            g_Fg.initFailed = true;
            LOG_WARNING(L"[LatProbe] FG: readback buffer creation failed - frame-gen timestamps disabled");
            return false;
        }

        D3D12_RANGE noRead = { 0, 0 };
        if (FAILED(g_Fg.readback->Map(0, &noRead, reinterpret_cast<void**>(&g_Fg.mapped))) || !g_Fg.mapped)
        {
            g_Fg.initFailed = true;
            LOG_WARNING(L"[LatProbe] FG: readback Map failed - frame-gen timestamps disabled");
            return false;
        }

        // Whether the game queue's timestamp frequency may be reused for these
        // values. Different device (multi-GPU offload) means a different clock
        // domain and the raw ticks must not be mapped with it.
        {
            std::lock_guard<std::mutex> lock(g_GpuMutex);
            g_Fg.sameDevice = 0;
            if (g_Gpu.initialized && g_Gpu.queryHeap)
            {
                ComPtr<ID3D12Device> overlayDevice;
                if (SUCCEEDED(g_Gpu.queryHeap->GetDevice(IID_PPV_ARGS(&overlayDevice))))
                    g_Fg.sameDevice = (overlayDevice.Get() == g_Fg.device.Get()) ? 1 : 0;
            }
        }

        g_Fg.initialized = true;
        LOG_INFO(L"[LatProbe] FG timestamps initialized (sameDevice="
            + std::to_wstring(g_Fg.sameDevice) + L")");
        return true;
    }

    // Attach a resolved pair. Same search rule as the overlay path.
    static void AttachFgTimestamps(uint64_t frameId, uint8_t sub, uint64_t start, uint64_t end)
    {
        if (sub >= MAX_GPU_PER_FRAME)
            return;

        SpinGuard guard(g_SlotLock);

        FrameSlot* target = nullptr;

        if (OpenFrame* of = FindOpenLocked(frameId))
        {
            target = &of->slot;
        }
        else
        {
            Batch& batch = g_Batches[g_WriteBatch];
            for (int i = batch.count - 1; i >= 0; i--)
            {
                if (batch.slots[i].baseFrameId == frameId)
                {
                    target = &batch.slots[i];
                    break;
                }
            }
        }

        if (!target)
        {
            g_OrphanGpuTimestamps.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        target->fgGpuStart[sub] = start;
        target->fgGpuEnd[sub] = end;
        if (sub + 1 > target->fgResolved)
            target->fgResolved = (uint8_t)(sub + 1);

        // Publish once, when the whole group has come back. Subframe timestamps
        // resolve one at a time and each earlier rung of the ladder is a lower
        // number than the last - feeding them all in would smear the readout
        // across the full span of the group instead of reporting its end.
        if (target->fgIssued > 0 && target->fgResolved >= target->fgIssued)
            PublishLatencyLocked(*target);
    }

    // Age the pending pairs and read back the ones old enough to be safe.
    // Must be called with g_FgMutex held.
    static void DrainFgLocked()
    {
        const uint64_t freq = g_Gpu.frequency;

        for (int i = 0; i < FG_SLOTS / 2; i++)
        {
            FgPending& p = g_Fg.pending[i];
            if (!p.inUse || !p.resolved)
                continue;

            if (++p.age < FG_READBACK_LAG)
                continue;

            p.inUse = false;

            if (!g_Fg.mapped)
                continue;

            const uint64_t start = g_Fg.mapped[p.idxStart];
            const uint64_t end = g_Fg.mapped[p.idxStart + 1];

            // Reject anything that cannot be a measurement rather than logging
            // it as one: unwritten slots, inverted pairs, and durations that
            // exceed any plausible frame-generation cost.
            bool ok = (start != 0 && end != 0 && end > start);
            if (ok && freq != 0)
            {
                const double ms = (double)(end - start) * 1000.0 / (double)freq;
                if (ms > 250.0)
                    ok = false;
            }

            if (!ok)
            {
                g_Fg.discarded.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            AttachFgTimestamps(p.frameId, p.sub, start, end);
        }
    }

    void OnDlssgEvaluatePre(ID3D12GraphicsCommandList* pCmdList, int subframeIndex, int subframeCount)
    {
        if (!g_Active.load(std::memory_order_relaxed) || !pCmdList)
            return;

        // MultiFrameIndex is 1-based; a game that never sets it leaves 1.
        const int sub = (subframeIndex > 0) ? subframeIndex - 1 : 0;
        if (sub >= MAX_GPU_PER_FRAME)
            return;

        std::lock_guard<std::mutex> lock(g_FgMutex);

        if (!InitFgLocked(pCmdList))
            return;

        uint64_t frameId = 0;
        const uint64_t now = NowQpc();
        {
            SpinGuard guard(g_SlotLock);

            if (sub == 0)
            {
                // First dispatch of a group: this is where the owner is decided,
                // once, for the whole group.
                // Primary rule: the frame that most recently finished its
                // present. The fallbacks below exist only for engines that do
                // not emit PRESENT_END at all.
                int16_t mode = 0;
                uint64_t candidate = 0;

                if (g_LastPresentEndId != 0 && FindOpenLocked(g_LastPresentEndId))
                {
                    candidate = g_LastPresentEndId;
                }
                else if (OpenFrame* inProgress = PresentInProgressLocked())
                {
                    candidate = inProgress->slot.baseFrameId;
                    mode = 1;
                }
                else
                {
                    candidate = g_PresentOwnerId;
                    mode = 2;
                }
                int16_t shift = 0;

                // If the latched owner already carries a group, the present
                // stream has not advanced yet. Move to the next open frame that
                // has no group rather than overwriting - a group written twice
                // loses both, which is what emptied a third of the frames.
                {
                    OpenFrame* of0 = FindOpenLocked(candidate);
                    if (of0 && of0->slot.fgIssued > 0)
                    {
                        OpenFrame* best = nullptr;
                        for (int i = 0; i < OPEN_FRAMES; i++)
                        {
                            OpenFrame& c = g_Open[i];
                            if (!c.inUse || c.slot.fgIssued > 0) continue;
                            if (c.slot.baseFrameId <= candidate) continue;
                            if (!best || c.slot.baseFrameId < best->slot.baseFrameId)
                                best = &c;
                        }
                        if (best)
                        {
                            shift = (int16_t)(best->slot.baseFrameId - candidate);
                            candidate = best->slot.baseFrameId;
                            mode = 3;
                        }
                        else
                        {
                            of0->slot.flags |= F_FG_OWNER_CONFLICT;
                            g_FgOwnerConflicts.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }

                g_FgOwnerId = candidate;

                if (OpenFrame* owner = FindOpenLocked(g_FgOwnerId))
                {
                    owner->slot.fgOwnerId = g_FgOwnerId;
                    owner->slot.fgOwnerShift = shift;
                    owner->slot.fgOwnerMode = mode;
                    owner->slot.fgGroupSeq = ++g_FgGroupSeq;
                    owner->slot.fgSimIdAtLatch = g_LastSimId;
                    owner->slot.fgPresentIdAtLatch = g_LastPresentId;
                    owner->fgGroupOpen = true;
                }
            }

            frameId = g_FgOwnerId;
            if (frameId == 0)
                return;

            OpenFrame* of = FindOpenLocked(frameId);
            if (!of)
                return;

            of->slot.fgCpuPre[sub] = now;
            if (sub + 1 > of->slot.fgIssued)
                of->slot.fgIssued = (uint8_t)(sub + 1);

            // Last subframe of the group: the frame may close again.
            if (subframeCount > 0 && sub + 1 >= subframeCount)
                of->fgGroupOpen = false;
        }

        if (frameId == 0)
            return;

        const int pairIdx = g_Fg.writeIndex;
        const int idxStart = pairIdx * 2;

        pCmdList->EndQuery(g_Fg.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, idxStart);

        FgPending& p = g_Fg.pending[pairIdx];
        p.frameId = frameId;
        p.sub = (uint8_t)sub;
        p.idxStart = idxStart;
        p.age = 0;
        p.resolved = false;
        p.inUse = true;

        g_Fg.openIdx = pairIdx;
        g_Fg.writeIndex = (g_Fg.writeIndex + 1) % (FG_SLOTS / 2);
    }

    void OnDlssgEvaluatePost(ID3D12GraphicsCommandList* pCmdList, int subframeIndex, int subframeCount)
    {
        (void)subframeIndex;
        (void)subframeCount;

        if (!g_Active.load(std::memory_order_relaxed) || !pCmdList)
            return;

        std::lock_guard<std::mutex> lock(g_FgMutex);

        if (!g_Fg.initialized || g_Fg.openIdx < 0)
            return;

        const int pairIdx = g_Fg.openIdx;
        g_Fg.openIdx = -1;

        FgPending& p = g_Fg.pending[pairIdx];
        if (!p.inUse)
            return;

        const uint64_t now = NowQpc();
        {
            SpinGuard guard(g_SlotLock);
            if (p.sub < MAX_GPU_PER_FRAME)
            {
                if (OpenFrame* of = FindOpenLocked(p.frameId))
                    of->slot.fgCpuPost[p.sub] = now;
            }
        }

        pCmdList->EndQuery(g_Fg.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, p.idxStart + 1);
        pCmdList->ResolveQueryData(g_Fg.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            p.idxStart, 2, g_Fg.readback.Get(),
            sizeof(uint64_t) * p.idxStart);
        p.resolved = true;

        DrainFgLocked();
    }

    // =========================================================================
    // CSV writer (worker thread)
    // =========================================================================

#if LATENCY_PROBE_CSV

    static std::wstring CsvPath()
    {
        return Common::GetModuleDirectory() + L"dlss-enabler-latency.csv";
    }

    static void WriteHeader(FILE* f)
    {
        fprintf(f, "# DLSS Enabler latency probe (rev 10 - group owner = frame whose PRESENT_END fired last)\n");
        fprintf(f, "# qpcFrequency=%llu\n", (unsigned long long)g_QpcFrequency.QuadPart);
        fprintf(f, "# One row per base frame. Empty numeric cell = event never observed.\n");
        fprintf(f, "# rep_* = NV_LATENCY_RESULT_PARAMS (microseconds, driver clock).\n");
        fprintf(f, "# qpc_* = our own QueryPerformanceCounter readings (ticks).\n");
        fprintf(f, "# gpuTicks_N / gpuQpc_N = Nth GPU frame-end timestamp within the base frame\n");
        fprintf(f, "#   (with frame generation there is one per generated Present).\n");
        fprintf(f, "# R* derived from Reflex, C* derived from our own data (ms).\n");

        fprintf(f, "batch,slot,baseFrameId,nvapiEmbedded,nvapiGenuine,nvapiMock,mfgOverride,mfgActual,framesGenerated,");

        for (int i = 0; i < Q_COUNT; i++)
            fprintf(f, "%s,", kQpcNames[i]);

        fprintf(f, "markerMask,presentCount,skewSimAtRs,skewSimAtPresent,flagSynthId,flagPresentFailed,flagAsync,flagGpuOverflow,flagEvicted,flagLateMarker,flagFgOwnerConflict,");

        fprintf(f, "gpuIssued,gpuStored,");
        for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            fprintf(f, "gpuTicks_%d,", i);
        for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            fprintf(f, "gpuQpc_%d,", i);

        fprintf(f, "syncInterval,bufferCount,swapEffect,swapChainFlags,devMaxFrameLatency,scMaxFrameLatency,");
        fprintf(f, "fgIssued,fgResolved,fgSameDevice,fgFrequency,fgOwnerId,fgSimIdAtLatch,fgPresentIdAtLatch,fgOwnerShift,fgOwnerMode,fgGroupSeq,fgStartLagMs,");
        for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            fprintf(f, "fgCpuPre_%d,", i);
        for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            fprintf(f, "fgCpuPost_%d,", i);
        for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            fprintf(f, "fgGpuStart_%d,", i);
        for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            fprintf(f, "fgGpuEnd_%d,", i);

        fprintf(f, "calGpuTicks,calQpcTicks,gpuFrequency,");
        fprintf(f, "reportStatus,reportQpc,reportCalls,reportMatched,");
        fprintf(f, "rep_frameID,rep_inputSample,rep_simStart,rep_simEnd,rep_rsStart,rep_rsEnd,");
        fprintf(f, "rep_presentStart,rep_presentEnd,rep_driverStart,rep_driverEnd,");
        fprintf(f, "rep_osqStart,rep_osqEnd,rep_gpuStart,rep_gpuEnd,");
        fprintf(f, "R1_sumOfDeltas,R2_gpuEndMinusSimStart,");
        fprintf(f, "C1_presentEndMinusSimStart,C3first_gpuMinusSimStart,C3last_gpuMinusSimStart,C4_postPresentMinusSimStart,C5_fgEndMinusSimStart\n");

        fflush(f);
    }

    static void EmitU64(FILE* f, uint64_t v)
    {
        if (v == 0)
            fprintf(f, ",");
        else
            fprintf(f, "%llu,", (unsigned long long)v);
    }

    static void EmitMs(FILE* f, double v, bool last)
    {
        if (v == 0.0)
            fprintf(f, last ? "\n" : ",");
        else
            fprintf(f, last ? "%.4f\n" : "%.4f,", v);
    }

    // Map a GPU tick value onto the QPC timeline using this batch's calibration.
    static uint64_t GpuTicksToQpc(const Batch& batch, uint64_t ticks)
    {
        if (ticks == 0 || !batch.hasCalibration || batch.gpuFrequency == 0)
            return 0;

        const int64_t deltaGpu = (int64_t)ticks - (int64_t)batch.calGpuTicks;
        const int64_t deltaQpc = (int64_t)((double)deltaGpu
            * (double)g_QpcFrequency.QuadPart
            / (double)batch.gpuFrequency);
        const int64_t mapped = (int64_t)batch.calQpcTicks + deltaQpc;
        return (mapped > 0) ? (uint64_t)mapped : 0;
    }

    // Fill in ground truth from the rolling map. Runs on the worker, one batch
    // behind the producer, so reports that arrived after the batch closed still
    // count.
    static void ResolveReports(Batch& batch)
    {
        std::lock_guard<std::mutex> lock(g_ReportMutex);

        for (int s = 0; s < batch.count; s++)
        {
            batch.reportMatched[s] = false;

            const uint64_t id = batch.slots[s].baseFrameId;
            if (id == 0)
                continue;

            auto it = g_ReportMap.find(id);
            if (it == g_ReportMap.end())
                continue;

            batch.reports[s] = it->second;
            batch.reportMatched[s] = true;
        }
    }

    static void WriteBatch(FILE* f, const Batch& batch)
    {
        int      reportStatus = 0;
        uint64_t reportQpc = 0;
        uint64_t reportCalls = 0;
        {
            std::lock_guard<std::mutex> lock(g_ReportMutex);
            reportStatus = g_LastReportStatus;
            reportQpc = g_LastReportQpc;
            reportCalls = g_ReportCalls;
        }

        for (int s = 0; s < batch.count; s++)
        {
            const FrameSlot& slot = batch.slots[s];

            fprintf(f, "%u,%d,%llu,%d,%d,%d,%d,%d,%d,",
                batch.batchIndex, s, (unsigned long long)slot.baseFrameId,
                batch.nvapiEmbedded, batch.nvapiGenuine, batch.nvapiMock,
                batch.mfgOverride, batch.mfgActual, batch.framesGenerated);

            for (int i = 0; i < Q_COUNT; i++)
                EmitU64(f, slot.qpc[i]);

            fprintf(f, "%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
                slot.markerMask, (unsigned)slot.presentCount,
                (int)slot.skewSimAtRs, (int)slot.skewSimAtPresent,
                (slot.flags & F_SYNTH_ID) ? 1 : 0,
                (slot.flags & F_PRESENT_FAILED) ? 1 : 0,
                (slot.flags & F_ASYNC_SEEN) ? 1 : 0,
                (slot.flags & F_GPU_OVERFLOW) ? 1 : 0,
                (slot.flags & F_EVICTED) ? 1 : 0,
                (slot.flags & F_LATE_MARKER) ? 1 : 0,
                (slot.flags & F_FG_OWNER_CONFLICT) ? 1 : 0);

            fprintf(f, "%u,%u,", (unsigned)slot.gpuIssued, (unsigned)slot.gpuStored);

            for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
                EmitU64(f, slot.gpuTicks[i]);

            uint64_t gpuQpc[MAX_GPU_PER_FRAME] = {};
            for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            {
                gpuQpc[i] = GpuTicksToQpc(batch, slot.gpuTicks[i]);
                EmitU64(f, gpuQpc[i]);
            }

            fprintf(f, "%d,%d,%d,%d,%d,%d,",
                batch.syncInterval, batch.bufferCount, batch.swapEffect,
                batch.swapChainFlags, batch.devMaxFrameLatency, batch.scMaxFrameLatency);

            fprintf(f, "%u,%u,%d,", (unsigned)slot.fgIssued, (unsigned)slot.fgResolved, batch.fgSameDevice);
            EmitU64(f, batch.fgFrequency);
            EmitU64(f, slot.fgOwnerId);
            EmitU64(f, slot.fgSimIdAtLatch);
            EmitU64(f, slot.fgPresentIdAtLatch);
            fprintf(f, "%d,", (int)slot.fgOwnerShift);
            fprintf(f, "%d,", (int)slot.fgOwnerMode);
            EmitU64(f, slot.fgGroupSeq);

            // How late the group's GPU work started relative to this frame's own
            // simulation, in milliseconds. Above one base-frame period the
            // attribution is not trustworthy and the row should be treated as
            // such rather than corrected.
            {
                const uint64_t fgStartQpc = GpuTicksToQpc(batch, slot.fgGpuStart[0]);
                EmitMs(f, QpcToMs(slot.qpc[Q_SIM_START], fgStartQpc), false);
            }

            for (int i = 0; i < MAX_GPU_PER_FRAME; i++) EmitU64(f, slot.fgCpuPre[i]);
            for (int i = 0; i < MAX_GPU_PER_FRAME; i++) EmitU64(f, slot.fgCpuPost[i]);
            for (int i = 0; i < MAX_GPU_PER_FRAME; i++) EmitU64(f, slot.fgGpuStart[i]);
            for (int i = 0; i < MAX_GPU_PER_FRAME; i++) EmitU64(f, slot.fgGpuEnd[i]);

            EmitU64(f, batch.hasCalibration ? batch.calGpuTicks : 0);
            EmitU64(f, batch.hasCalibration ? batch.calQpcTicks : 0);
            EmitU64(f, batch.hasCalibration ? batch.gpuFrequency : 0);

            fprintf(f, "%d,", reportStatus);
            EmitU64(f, reportQpc);
            EmitU64(f, reportCalls);

            const bool matched = batch.reportMatched[s];
            const FrameReport* rep = matched ? &batch.reports[s] : nullptr;

            fprintf(f, "%d,", matched ? 1 : 0);

            if (rep)
            {
                EmitU64(f, (uint64_t)rep->frameID);
                EmitU64(f, (uint64_t)rep->inputSampleTime);
                EmitU64(f, (uint64_t)rep->simStartTime);
                EmitU64(f, (uint64_t)rep->simEndTime);
                EmitU64(f, (uint64_t)rep->renderSubmitStartTime);
                EmitU64(f, (uint64_t)rep->renderSubmitEndTime);
                EmitU64(f, (uint64_t)rep->presentStartTime);
                EmitU64(f, (uint64_t)rep->presentEndTime);
                EmitU64(f, (uint64_t)rep->driverStartTime);
                EmitU64(f, (uint64_t)rep->driverEndTime);
                EmitU64(f, (uint64_t)rep->osRenderQueueStartTime);
                EmitU64(f, (uint64_t)rep->osRenderQueueEndTime);
                EmitU64(f, (uint64_t)rep->gpuRenderStartTime);
                EmitU64(f, (uint64_t)rep->gpuRenderEndTime);
            }
            else
            {
                fprintf(f, ",,,,,,,,,,,,,,");
            }

            // ---- Reflex-derived ----
            double R1 = 0.0, R2 = 0.0;
            if (rep)
            {
                const uint64_t simD = (rep->simEndTime > rep->simStartTime)
                    ? rep->simEndTime - rep->simStartTime : 0;
                const uint64_t osqD = (rep->osRenderQueueEndTime > rep->osRenderQueueStartTime)
                    ? rep->osRenderQueueEndTime - rep->osRenderQueueStartTime : 0;
                const uint64_t gpuD = (rep->gpuRenderEndTime > rep->gpuRenderStartTime)
                    ? rep->gpuRenderEndTime - rep->gpuRenderStartTime : 0;
                const uint64_t preD = (rep->presentEndTime > rep->presentStartTime)
                    ? rep->presentEndTime - rep->presentStartTime : 0;

                // R1 is what OnPostGetLatency computes today. The first capture
                // showed gpuRender sits entirely inside osRenderQueue in 100% of
                // frames (osqEnd == gpuEnd exactly), so this sum counts GPU time
                // twice and appends present time that falls past the endpoint:
                //   R1 - (R2 + gpuD + presD) = -1.63 ms, std 0.53
                // Kept in the log so the correction stays verifiable.
                R1 = (double)(simD + osqD + gpuD + preD) / 1000.0;

                if (rep->gpuRenderEndTime > rep->simStartTime)
                    R2 = (double)(rep->gpuRenderEndTime - rep->simStartTime) / 1000.0;
            }

            // ---- Ours ----
            uint64_t firstGpu = 0, lastGpu = 0;
            for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
            {
                if (gpuQpc[i] == 0)
                    continue;
                if (firstGpu == 0)
                    firstGpu = gpuQpc[i];
                lastGpu = gpuQpc[i];
            }

            const double C1 = QpcToMs(slot.qpc[Q_SIM_START], slot.qpc[Q_PRESENT_END]);
            const double C3f = QpcToMs(slot.qpc[Q_SIM_START], firstGpu);
            const double C3l = QpcToMs(slot.qpc[Q_SIM_START], lastGpu);
            const double C4 = QpcToMs(slot.qpc[Q_SIM_START], slot.qpc[Q_POST_PRESENT]);

            // C5: end of the LAST frame-generation dispatch of this base frame,
            // mapped onto QPC. Only meaningful when the DLSSG command list runs
            // on the same device as the overlay - otherwise the tick domains
            // differ and the raw fgGpuEnd_* columns are all there is to work with.
            double C5 = 0.0;
            if (batch.fgSameDevice && batch.hasCalibration)
            {
                uint64_t lastFg = 0;
                for (int i = 0; i < MAX_GPU_PER_FRAME; i++)
                {
                    if (slot.fgGpuEnd[i] != 0)
                        lastFg = slot.fgGpuEnd[i];
                }
                C5 = QpcToMs(slot.qpc[Q_SIM_START], GpuTicksToQpc(batch, lastFg));
            }

            EmitMs(f, R1, false);
            EmitMs(f, R2, false);
            EmitMs(f, C1, false);
            EmitMs(f, C3f, false);
            EmitMs(f, C3l, false);
            EmitMs(f, C4, false);
            EmitMs(f, C5, true);
        }

        fflush(f);
    }

    static void WorkerMain()
    {
        FILE* f = nullptr;
        const std::wstring path = CsvPath();

        if (_wfopen_s(&f, path.c_str(), L"w") != 0 || !f)
        {
            LOG_WARNING(L"[LatProbe] Could not open " + path + L" - probe disabled");
            g_Active.store(false, std::memory_order_relaxed);
            return;
        }

        LOG_INFO(L"[LatProbe] Writing " + path);
        WriteHeader(f);

        // One batch of deliberate lag: a batch is only written once the NEXT one
        // has closed. Costs a couple of seconds of tail latency in the log and
        // buys back the ground-truth coverage that a same-instant snapshot missed.
        int held = -1;

        for (;;)
        {
            int batchIdx = -1;
            bool quitting = false;

            {
                std::unique_lock<std::mutex> lock(g_QueueMutex);
                g_QueueCv.wait(lock, [] { return g_Quit || !g_FlushQueue.empty(); });

                if (!g_FlushQueue.empty())
                {
                    batchIdx = g_FlushQueue.front();
                    g_FlushQueue.pop_front();
                }
                else
                {
                    quitting = g_Quit;
                }
            }

            if (batchIdx >= 0 && batchIdx < BATCH_COUNT)
            {
                if (held >= 0)
                {
                    ResolveReports(g_Batches[held]);
                    WriteBatch(f, g_Batches[held]);
                }
                held = batchIdx;
            }

            if (quitting)
                break;
        }

        if (held >= 0)
        {
            ResolveReports(g_Batches[held]);
            WriteBatch(f, g_Batches[held]);
        }

        fprintf(f, "# droppedBatches=%llu orphanGpuTimestamps=%llu getLatencyCalls=%llu reportMapSize=%llu fgDiscarded=%llu lateMarkers=%llu evictedFrames=%llu fgOwnerConflicts=%llu\n",
            (unsigned long long)g_DroppedBatches.load(),
            (unsigned long long)g_OrphanGpuTimestamps.load(),
            (unsigned long long)g_ReportCalls,
            (unsigned long long)g_ReportMap.size(),
            (unsigned long long)g_Fg.discarded.load(),
            (unsigned long long)g_LateMarkers.load(),
            (unsigned long long)g_EvictedFrames.load(),
            (unsigned long long)g_FgOwnerConflicts.load());
        fclose(f);
    }

#endif // LATENCY_PROBE_CSV

    // =========================================================================
    // Lifecycle
    // =========================================================================

    void Init()
    {
        if (g_Initialized)
            return;

        QueryPerformanceFrequency(&g_QpcFrequency);
        if (g_QpcFrequency.QuadPart == 0)
        {
            LOG_WARNING(L"[LatProbe] QPC unavailable - probe disabled");
            return;
        }

        for (int i = 0; i < BATCH_COUNT; i++)
        {
            g_Batches[i].count = 0;
            g_Batches[i].hasCalibration = false;
        }

        g_ReportMap.reserve(REPORT_MAP_LIMIT);

        g_Quit = false;
        g_Active.store(true, std::memory_order_relaxed);

#if LATENCY_PROBE_CSV
        g_Worker = std::thread(WorkerMain);
#endif

#if defined(LATPROBE_HAS_NGX_EVENTS)
        // Bracket the DLSSG evaluate without touching NgxFrontend or DlssgProxy.
        // The filter means these only fire for frame generation, so the upscaler
        // and every other NGX feature stay out of the way.
        NgxFeatureEvents::RegisterPreEvaluateD3D12(
            [](ID3D12GraphicsCommandList* cmdList, const NVSDK_NGX_Handle*,
                NVSDK_NGX_Parameter* parameters, NVSDK_NGX_Feature)
            {
                int idx = 1, count = 1;
                if (parameters)
                {
                    parameters->Get("DLSSG.MultiFrameIndex", &idx);
                    parameters->Get("DLSSG.MultiFrameCount", &count);
                }
                OnDlssgEvaluatePre(cmdList, idx, count);
            },
            NVSDK_NGX_Feature_FrameGeneration);

        NgxFeatureEvents::RegisterPostEvaluateD3D12(
            [](ID3D12GraphicsCommandList* cmdList, const NVSDK_NGX_Handle*,
                NVSDK_NGX_Parameter* parameters, NVSDK_NGX_Feature, NVSDK_NGX_Result)
            {
                int idx = 1, count = 1;
                if (parameters)
                {
                    parameters->Get("DLSSG.MultiFrameIndex", &idx);
                    parameters->Get("DLSSG.MultiFrameCount", &count);
                }
                OnDlssgEvaluatePost(cmdList, idx, count);
            },
            NVSDK_NGX_Feature_FrameGeneration);

        LOG_INFO(L"[LatProbe] Registered DLSSG evaluate listeners");
#else
        LOG_WARNING(L"[LatProbe] NgxFeatureEvents not reachable - frame-gen window will stay empty");
#endif

        g_Initialized = true;
        LOG_INFO(L"[LatProbe] Latency probe active (developer build)");
    }

    void Shutdown()
    {
        if (!g_Initialized)
            return;

        g_Active.store(false, std::memory_order_relaxed);

        // Flush whatever is still open so the tail of the session is not lost.
        {
            SpinGuard guard(g_SlotLock);
            for (int i = 0; i < OPEN_FRAMES; i++)
            {
                if (g_Open[i].inUse)
                    CloseFrameLocked(&g_Open[i]);
            }
        }

        {
            std::lock_guard<std::mutex> lock(g_QueueMutex);
            g_Quit = true;
        }
        g_QueueCv.notify_all();

        if (g_Worker.joinable())
            g_Worker.join();

        {
            std::lock_guard<std::mutex> lock(g_GpuMutex);
            if (g_Gpu.mapped && g_Gpu.readback)
            {
                D3D12_RANGE noWrite = { 0, 0 };
                g_Gpu.readback->Unmap(0, &noWrite);
                g_Gpu.mapped = nullptr;
            }
            g_Gpu.fence.Reset();
            g_Gpu.readback.Reset();
            g_Gpu.queryHeap.Reset();
            g_Gpu.initialized = false;
        }

        {
            std::lock_guard<std::mutex> lock(g_FgMutex);
            if (g_Fg.mapped && g_Fg.readback)
            {
                D3D12_RANGE noWrite = { 0, 0 };
                g_Fg.readback->Unmap(0, &noWrite);
                g_Fg.mapped = nullptr;
            }
            g_Fg.readback.Reset();
            g_Fg.queryHeap.Reset();
            g_Fg.device.Reset();
            g_Fg.initialized = false;
        }

        g_Initialized = false;
    }

    bool IsActive()
    {
        return g_Active.load(std::memory_order_relaxed);
    }

    double GetLatencyMs()
    {
        return g_LatencyMs.load(std::memory_order_relaxed);
    }

    bool HasLatency()
    {
        const uint64_t last = g_LatencyQpc.load(std::memory_order_relaxed);
        if (last == 0 || g_QpcFrequency.QuadPart == 0)
            return false;
        const uint64_t now = NowQpc();
        return (now > last) && ((now - last) < (uint64_t)g_QpcFrequency.QuadPart);
    }
}

#endif // LATENCY_PROBE