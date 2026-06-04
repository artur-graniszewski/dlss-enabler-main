// =============================================================================
// FpsMonitor.cpp - GPU Timestamp Queries for accurate potential FPS
// =============================================================================

#include "FpsMonitor.h"
#include "ActionIntensity.h"
#include "SwapChainEvents.h"
#include "ReflexEvents.h"
#include "../Core/Context.h"
#include "Common.h"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// External: force Streamline to apply current mfgEnforcedMode (defined in StreamlineProxy.cpp)
extern void StreamlineProxy_ForceApplyMfgMode();
// External: restore full cached game DLSSG options to Streamline
extern void StreamlineProxy_RestoreGameDLSSGOptions();

// Potential FPS overhead factors for easy tweaking
#define POTENTIAL_FPS_OVERHEAD_FACTOR 0.9         // -10% base overhead
#define POTENTIAL_FPS_DLSSG_OVERHEAD_FACTOR 0.9   // -10% additional DLSS-G overhead

// Algorithm selection for frame time calculation
// 0 = Old: Present(N) - Present(N-1) (Present-to-Present)
// 1 = New: SimulationStart(frameId) to PresentEnd(frameId) (Reflex markers)
#define FRAME_TIME_ALGORITHM 0

namespace FpsMonitor
{
    static bool g_Initialized = false;
    static LARGE_INTEGER g_PerfFrequency = {};

    // -------------------------------------------------------------------------
    // Current FPS
    // -------------------------------------------------------------------------
    static std::mutex g_FpsMutex;
    static const int FRAME_TIME_BUFFER_SIZE = 4000;  // Supports up to 2000 FPS with 2s window
    static const double FPS_WINDOW_MS = 2000.0;  // 2 second window for faster response
    static double g_FrameTimes[FRAME_TIME_BUFFER_SIZE];
    static ULONGLONG g_FrameTimestamps[FRAME_TIME_BUFFER_SIZE];
    static int g_FrameTimeHead = 0;
    static int g_FrameTimeCount = 0;
    static LARGE_INTEGER g_LastFrameTime = {};
    static std::atomic<int> g_CurrentFps{ 0 };
    static std::atomic<double> g_FrameTimeMs{ 0.0 };
    static std::atomic<double> g_AvgFrameTimeMs{ 0.0 };

    // -------------------------------------------------------------------------
    // Potential FPS & timing
    // -------------------------------------------------------------------------
    static std::atomic<double> g_SimulationTimeMs{ 0.0 };
    static std::atomic<double> g_RenderSubmitTimeMs{ 0.0 };
    static std::atomic<double> g_PresentWaitTimeMs{ 0.0 };
    static std::atomic<double> g_CpuWorkTimeMs{ 0.0 };
    static std::atomic<double> g_GpuWorkTimeMs{ 0.0 };
    static std::atomic<double> g_GpuWorkTimeRawMs{ 0.0 };  // Raw (non-averaged) GPU work time for debug
    static std::atomic<double> g_ReflexSleepTimeMs{ 0.0 };  // Time spent in NvAPI_D3D_Sleep
    static std::atomic<int> g_PotentialFps{ 0 };
    static std::atomic<bool> g_HasReflexData{ false };
    static std::atomic<bool> g_HasGpuTimestamps{ false };
    static std::atomic<uint32_t> g_LastMarkerType{ 999 };

    // -------------------------------------------------------------------------
    // Latency monitoring (from NvAPI_D3D_GetLatency)
    // -------------------------------------------------------------------------
    static std::mutex g_LatencyMutex;
    static const int LATENCY_HISTORY_SIZE = 64;
    static const double LATENCY_WINDOW_MS = 1000.0;  // 1 second window for latency averaging
    static double g_LatencyHistory[LATENCY_HISTORY_SIZE];
    static ULONGLONG g_LatencyTimestamps[LATENCY_HISTORY_SIZE];
    static int g_LatencyHead = 0;
    static int g_LatencyCount = 0;
    static std::atomic<double> g_AverageLatencyMs{ 0.0 };
    static std::atomic<ULONGLONG> g_LastLatencyUpdateTime{ 0 };  // Timestamp of last GetLatency call
    static const ULONGLONG LATENCY_STALE_THRESHOLD_MS = 1000;    // Consider data stale after 1 second
    static std::atomic<uint64_t> g_LastBaseFrameId{ 0 };         // Last base frame ID from SetLatencyMarker

    // Reflex Sleep timing
    // We need to detect if Sleep is inside Present or after Present
    static LARGE_INTEGER g_SleepStartTime = {};
    static LARGE_INTEGER g_SleepEndTime = {};
    static std::atomic<double> g_LastSleepDurationMs{ 0.0 };
    static std::atomic<double> g_LastSleepWithGapMs{ 0.0 };  // Sleep + gap between postPresent and preSleep
    static std::atomic<bool> g_SleepInsidePresent{ false };  // True if sleep occurred between prePresent and postPresent

    // Work time averaging
    static std::mutex g_WorkTimeMutex;
    static const int WORK_TIME_HISTORY_SIZE = 1000;
    static const double WORK_TIME_WINDOW_MS = 2000.0;  // 2 second window for faster response
    static double g_CpuWorkTimeHistory[WORK_TIME_HISTORY_SIZE];
    static double g_GpuWorkTimeHistory[WORK_TIME_HISTORY_SIZE];
    static ULONGLONG g_WorkTimeTimestamps[WORK_TIME_HISTORY_SIZE];
    static int g_WorkTimeHead = 0;
    static int g_WorkTimeCount = 0;

    // -------------------------------------------------------------------------
    // SIM_START queue (for CPU work time from Reflex markers)
    // -------------------------------------------------------------------------
    static const size_t SIM_START_QUEUE_SIZE = 32;
    static LARGE_INTEGER g_SimStartQueue[SIM_START_QUEUE_SIZE];
    static uint64_t g_SimStartFrameIds[SIM_START_QUEUE_SIZE];
    static size_t g_SimStartQueueHead = 0;
    static size_t g_SimStartQueueTail = 0;
    static size_t g_SimStartQueueCount = 0;
    static std::mutex g_SimStartQueueMutex;

    static LARGE_INTEGER g_LastPresentStartTime = {};

    // For new algorithm: SimStart -> PresentEnd frame time
    static LARGE_INTEGER g_CurrentFrameSimStartTime = {};  // SimStart time for current frame
    static std::atomic<double> g_SimToPresentEndMs{ 0.0 }; // Full frame time from SimStart to PresentEnd

    // -------------------------------------------------------------------------
    // GPU Timestamp Queries - D3D11
    // -------------------------------------------------------------------------
    struct D3D11TimestampQueries
    {
        ComPtr<ID3D11Device> pDevice;
        ComPtr<ID3D11DeviceContext> pContext;
        ComPtr<ID3D11Query> pDisjointQuery[3];    // Triple buffered
        ComPtr<ID3D11Query> pTimestampStart[3];
        ComPtr<ID3D11Query> pTimestampEnd[3];
        int writeIndex = 0;      // Current frame writing
        int readIndex = 0;       // Frame to read results from (2 frames behind)
        int frameCount = 0;      // Frames since init
        bool initialized = false;
        std::mutex mutex;
    };
    static D3D11TimestampQueries g_D3D11Queries;

    // -------------------------------------------------------------------------
    // GPU Timestamp Queries - D3D12
    // -------------------------------------------------------------------------
    struct D3D12TimestampQueries
    {
        ComPtr<ID3D12Device> pDevice;
        ComPtr<ID3D12CommandQueue> pCommandQueue;
        ComPtr<ID3D12QueryHeap> pQueryHeap;
        ComPtr<ID3D12Resource> pReadbackBuffer;
        ComPtr<ID3D12CommandAllocator> pCommandAllocator;
        ComPtr<ID3D12GraphicsCommandList> pCommandList;
        ComPtr<ID3D12Fence> pFence;
        HANDLE fenceEvent = nullptr;
        UINT64 fenceValue = 0;
        UINT64 gpuFrequency = 0;

        // Triple buffered (2 timestamps per frame: start + end)
        static const int NUM_FRAMES = 3;
        static const int TIMESTAMPS_PER_FRAME = 2;
        int writeIndex = 0;
        int readIndex = 0;
        int frameCount = 0;
        bool initialized = false;
        std::mutex mutex;
    };
    static D3D12TimestampQueries g_D3D12Queries;

    // -------------------------------------------------------------------------
    // Marker types
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    static double CalculateTrimmedMeanWithWindow(
        double* values, ULONGLONG* timestamps, int head, int count,
        int bufferSize, ULONGLONG currentTime, double windowMs, double trimPercent = 0.1)
    {
        if (count == 0) return 0.0;
        double temp[1000];
        int validCount = 0;
        ULONGLONG windowStart = currentTime - (ULONGLONG)windowMs;

        for (int i = 0; i < count && validCount < 1000; i++)
        {
            int idx = (head - 1 - i + bufferSize) % bufferSize;
            if (timestamps[idx] >= windowStart)
                temp[validCount++] = values[idx];
            else
                break;
        }

        if (validCount == 0) return 0.0;
        if (validCount < 10)
        {
            double sum = 0.0;
            for (int i = 0; i < validCount; i++) sum += temp[i];
            return sum / validCount;
        }

        std::sort(temp, temp + validCount);
        int trimCount = (int)(validCount * trimPercent);
        if (trimCount < 1) trimCount = 1;

        double sum = 0.0;
        int usedCount = 0;
        for (int i = trimCount; i < validCount - trimCount; i++)
        {
            sum += temp[i];
            usedCount++;
        }
        return usedCount > 0 ? sum / usedCount : temp[validCount / 2];
    }

    static void PushSimStart(LARGE_INTEGER time, uint64_t frameId)
    {
        std::lock_guard<std::mutex> lock(g_SimStartQueueMutex);
        g_SimStartQueue[g_SimStartQueueHead] = time;
        g_SimStartFrameIds[g_SimStartQueueHead] = frameId;
        g_SimStartQueueHead = (g_SimStartQueueHead + 1) % SIM_START_QUEUE_SIZE;
        if (g_SimStartQueueCount < SIM_START_QUEUE_SIZE)
            g_SimStartQueueCount++;
        else
            g_SimStartQueueTail = (g_SimStartQueueTail + 1) % SIM_START_QUEUE_SIZE;
    }

    static bool PopSimStart(LARGE_INTEGER* outTime, uint64_t* outFrameId)
    {
        std::lock_guard<std::mutex> lock(g_SimStartQueueMutex);
        if (g_SimStartQueueCount == 0)
            return false;
        *outTime = g_SimStartQueue[g_SimStartQueueTail];
        *outFrameId = g_SimStartFrameIds[g_SimStartQueueTail];
        g_SimStartQueueTail = (g_SimStartQueueTail + 1) % SIM_START_QUEUE_SIZE;
        g_SimStartQueueCount--;
        return true;
    }

    // -------------------------------------------------------------------------
    // D3D11 GPU Timestamp Implementation
    // -------------------------------------------------------------------------
    static bool InitD3D11Timestamps(ID3D11Device* pDevice)
    {
        std::lock_guard<std::mutex> lock(g_D3D11Queries.mutex);

        if (g_D3D11Queries.initialized)
            return true;

        if (!pDevice)
            return false;

        g_D3D11Queries.pDevice = pDevice;
        pDevice->GetImmediateContext(&g_D3D11Queries.pContext);

        if (!g_D3D11Queries.pContext)
            return false;

        D3D11_QUERY_DESC desc = {};

        for (int i = 0; i < 3; i++)
        {
            desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            desc.MiscFlags = 0;
            HRESULT hr = pDevice->CreateQuery(&desc, &g_D3D11Queries.pDisjointQuery[i]);
            if (FAILED(hr)) return false;

            desc.Query = D3D11_QUERY_TIMESTAMP;
            hr = pDevice->CreateQuery(&desc, &g_D3D11Queries.pTimestampStart[i]);
            if (FAILED(hr)) return false;

            hr = pDevice->CreateQuery(&desc, &g_D3D11Queries.pTimestampEnd[i]);
            if (FAILED(hr)) return false;
        }

        g_D3D11Queries.initialized = true;
        LOG_INFO(L"[FpsMon] D3D11 GPU timestamp queries initialized");
        return true;
    }

    static void D3D11_BeginTimestamp()
    {
        std::lock_guard<std::mutex> lock(g_D3D11Queries.mutex);

        if (!g_D3D11Queries.initialized || !g_D3D11Queries.pContext)
            return;

        int idx = g_D3D11Queries.writeIndex;
        g_D3D11Queries.pContext->Begin(g_D3D11Queries.pDisjointQuery[idx].Get());
        g_D3D11Queries.pContext->End(g_D3D11Queries.pTimestampStart[idx].Get());
    }

    static void D3D11_EndTimestamp()
    {
        std::lock_guard<std::mutex> lock(g_D3D11Queries.mutex);

        if (!g_D3D11Queries.initialized || !g_D3D11Queries.pContext)
            return;

        int idx = g_D3D11Queries.writeIndex;
        g_D3D11Queries.pContext->End(g_D3D11Queries.pTimestampEnd[idx].Get());
        g_D3D11Queries.pContext->End(g_D3D11Queries.pDisjointQuery[idx].Get());

        // Advance write index
        g_D3D11Queries.writeIndex = (g_D3D11Queries.writeIndex + 1) % 3;
        g_D3D11Queries.frameCount++;

        // Read results from 2 frames ago (to avoid stalls)
        if (g_D3D11Queries.frameCount >= 3)
        {
            int readIdx = g_D3D11Queries.readIndex;

            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
            HRESULT hr = g_D3D11Queries.pContext->GetData(
                g_D3D11Queries.pDisjointQuery[readIdx].Get(),
                &disjointData, sizeof(disjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH);

            if (hr == S_OK && !disjointData.Disjoint)
            {
                UINT64 startTime = 0, endTime = 0;

                hr = g_D3D11Queries.pContext->GetData(
                    g_D3D11Queries.pTimestampStart[readIdx].Get(),
                    &startTime, sizeof(startTime), D3D11_ASYNC_GETDATA_DONOTFLUSH);

                if (hr == S_OK)
                {
                    hr = g_D3D11Queries.pContext->GetData(
                        g_D3D11Queries.pTimestampEnd[readIdx].Get(),
                        &endTime, sizeof(endTime), D3D11_ASYNC_GETDATA_DONOTFLUSH);

                    if (hr == S_OK && endTime > startTime)
                    {
                        double gpuTimeMs = (double)(endTime - startTime) / (double)disjointData.Frequency * 1000.0;

                        if (gpuTimeMs > 0.0 && gpuTimeMs < 1000.0)
                        {
                            g_GpuWorkTimeMs.store(gpuTimeMs, std::memory_order_relaxed);
                            g_HasGpuTimestamps.store(true, std::memory_order_relaxed);
                        }
                    }
                }
            }

            g_D3D11Queries.readIndex = (g_D3D11Queries.readIndex + 1) % 3;
        }
    }

    // -------------------------------------------------------------------------
    // D3D12 GPU Timestamp Implementation
    // -------------------------------------------------------------------------
    static bool InitD3D12Timestamps(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue)
    {
        std::lock_guard<std::mutex> lock(g_D3D12Queries.mutex);

        if (g_D3D12Queries.initialized)
            return true;

        if (!pDevice || !pQueue)
            return false;

        g_D3D12Queries.pDevice = pDevice;
        g_D3D12Queries.pCommandQueue = pQueue;

        // Get GPU timestamp frequency
        HRESULT hr = pQueue->GetTimestampFrequency(&g_D3D12Queries.gpuFrequency);
        if (FAILED(hr) || g_D3D12Queries.gpuFrequency == 0)
            return false;

        // Create query heap (3 frames * 2 timestamps each)
        D3D12_QUERY_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        heapDesc.Count = D3D12TimestampQueries::NUM_FRAMES * D3D12TimestampQueries::TIMESTAMPS_PER_FRAME;
        heapDesc.NodeMask = 0;

        hr = pDevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&g_D3D12Queries.pQueryHeap));
        if (FAILED(hr)) return false;

        // Create readback buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = sizeof(UINT64) * heapDesc.Count;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = pDevice->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE,
            &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&g_D3D12Queries.pReadbackBuffer));
        if (FAILED(hr)) return false;

        // Create command allocator
        hr = pDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&g_D3D12Queries.pCommandAllocator));
        if (FAILED(hr)) return false;

        // Create command list
        hr = pDevice->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            g_D3D12Queries.pCommandAllocator.Get(), nullptr,
            IID_PPV_ARGS(&g_D3D12Queries.pCommandList));
        if (FAILED(hr)) return false;

        g_D3D12Queries.pCommandList->Close();

        // Create fence
        hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_D3D12Queries.pFence));
        if (FAILED(hr)) return false;

        g_D3D12Queries.fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!g_D3D12Queries.fenceEvent) return false;

        g_D3D12Queries.initialized = true;
        LOG_INFO(L"[FpsMon] D3D12 GPU timestamp queries initialized");
        return true;
    }

    // -------------------------------------------------------------------------
    // GPU time estimation via Present timing
    // For D3D12: measure time GPU actually spends between frames
    // -------------------------------------------------------------------------
    static LARGE_INTEGER g_PrePresentTime = {};
    static LARGE_INTEGER g_PostPresentTime = {};
    static std::atomic<double> g_PresentDurationMs{ 0.0 };

    // GPU work time history for averaging
    static std::mutex g_GpuTimeMutex;
    static double g_GpuTimeHistory[WORK_TIME_HISTORY_SIZE];
    static ULONGLONG g_GpuTimeTimestamps[WORK_TIME_HISTORY_SIZE];
    static int g_GpuTimeHead = 0;
    static int g_GpuTimeCount = 0;

    // -------------------------------------------------------------------------
    // SwapChain callbacks
    // -------------------------------------------------------------------------
    static void OnPrePresent(IDXGISwapChain* pSwapChain, UINT& SyncInterval, UINT& Flags)
    {
        // Record pre-present time (GPU has finished rendering, about to present)
        QueryPerformanceCounter(&g_PrePresentTime);

        // Try to initialize D3D11 timestamps
        if (!g_D3D11Queries.initialized)
        {
            ComPtr<ID3D11Device> pDevice;
            HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);
            if (SUCCEEDED(hr) && pDevice)
            {
                InitD3D11Timestamps(pDevice.Get());
            }
        }

        // For D3D11: End timestamp measurement (GPU work done, about to present)
        if (g_D3D11Queries.initialized)
        {
            D3D11_EndTimestamp();
        }
    }

    static void OnPostPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags, HRESULT result)
    {
        if (FAILED(result)) return;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        ULONGLONG currentTime = GetTickCount64();

        // Record post-present time
        g_PostPresentTime = now;

        // Calculate Present duration (VSync wait time)
        double presentDurationMs = 0.0;
        if (g_PrePresentTime.QuadPart > 0)
        {
            presentDurationMs = (double)(now.QuadPart - g_PrePresentTime.QuadPart) * 1000.0 / (double)g_PerfFrequency.QuadPart;
            g_PresentDurationMs.store(presentDurationMs, std::memory_order_relaxed);
        }

        // ---- Current FPS calculation (count-based) ----
        double deltaMs = 0.0;
        if (g_LastFrameTime.QuadPart != 0)
        {
            deltaMs = (double)(now.QuadPart - g_LastFrameTime.QuadPart) * 1000.0 / (double)g_PerfFrequency.QuadPart;
            g_FrameTimeMs.store(deltaMs, std::memory_order_relaxed);

            {
                std::lock_guard<std::mutex> lock(g_FpsMutex);
                g_FrameTimes[g_FrameTimeHead] = deltaMs;
                g_FrameTimestamps[g_FrameTimeHead] = currentTime;
                g_FrameTimeHead = (g_FrameTimeHead + 1) % FRAME_TIME_BUFFER_SIZE;
                if (g_FrameTimeCount < FRAME_TIME_BUFFER_SIZE)
                    g_FrameTimeCount++;

                // Count-based FPS: count frames within the time window
                // This is more accurate than 1000/avgFrameTime because it doesn't
                // suffer from bias introduced by trimmed mean on frame times
                ULONGLONG windowStart = currentTime - (ULONGLONG)FPS_WINDOW_MS;
                int framesInWindow = 0;

                for (int i = 0; i < g_FrameTimeCount; i++)
                {
                    int idx = (g_FrameTimeHead - 1 - i + FRAME_TIME_BUFFER_SIZE) % FRAME_TIME_BUFFER_SIZE;
                    if (g_FrameTimestamps[idx] >= windowStart)
                        framesInWindow++;
                    else
                        break;  // Timestamps are in order, so we can stop early
                }

                if (framesInWindow > 0)
                {
                    // FPS = frames / seconds
                    int fps = (int)(framesInWindow * 1000.0 / FPS_WINDOW_MS + 0.5);
                    g_CurrentFps.store(fps, std::memory_order_relaxed);
                }

                // Still calculate avg frame time for display purposes (using simple mean, no trim)
                double sum = 0.0;
                int count = 0;
                for (int i = 0; i < g_FrameTimeCount; i++)
                {
                    int idx = (g_FrameTimeHead - 1 - i + FRAME_TIME_BUFFER_SIZE) % FRAME_TIME_BUFFER_SIZE;
                    if (g_FrameTimestamps[idx] >= windowStart)
                    {
                        sum += g_FrameTimes[idx];
                        count++;
                    }
                    else
                        break;
                }
                if (count > 0)
                {
                    g_AvgFrameTimeMs.store(sum / count, std::memory_order_relaxed);
                }
            }
        }
        g_LastFrameTime = now;

#if FRAME_TIME_ALGORITHM == 0
        // ---- GPU work time estimation (OLD ALGORITHM: Present-to-Present) ----
        // 
        // Formula: work = DS - S + PB
        //   DS = Desired frame time (1000 / fpsLimit)
        //   S  = Actual sleep time
        //   PB = Present blocking time (from Reflex markers)
        //
        // Logic: Reflex thinks work = DS - S, but Present blocking (PB) is also
        // part of the work that GPU needs to do. When FPS cap is active, PB is
        // shorter because GPU has more idle time, so we need to add it back.
        //
        // Note: On AMD/Intel with emulated Reflex + DLSS-G, realFpsLimit is half of
        // desiredFpsLimit because DLSS-G generates extra frames and Reflex sleeps
        // twice as often. We use realFpsLimit for accurate calculation.
        //
        // Without FPS cap: work = frame_time (PP)
        //
        if (!g_HasGpuTimestamps.load(std::memory_order_relaxed) && deltaMs > 0.0)
        {
            // Get sleep data
            double actualSleepMs = g_LastSleepDurationMs.exchange(0.0, std::memory_order_relaxed);

            // Get Present blocking time from Reflex markers
            double presentBlockingMs = g_PresentWaitTimeMs.load(std::memory_order_relaxed);

            double gpuEstimateMs;

            // Use realFpsLimit (accounts for DLSS-G frame doubling on AMD/Intel)
            int effectiveFpsLimit = ctx.reflex.realFpsLimit > 0 ? ctx.reflex.realFpsLimit : ctx.reflex.desiredFpsLimit;

            if (actualSleepMs > 1.0 && ctx.reflex.isFpsLimitEnabled && effectiveFpsLimit > 0)
            {
                // FPS cap active: work = DS - S + PB
                double desiredFrameTimeMs = 1000.0 / (double)effectiveFpsLimit;

                gpuEstimateMs = desiredFrameTimeMs - actualSleepMs + presentBlockingMs;

                // Sanity checks
                if (gpuEstimateMs < 0.5) gpuEstimateMs = 0.5;
                if (gpuEstimateMs > deltaMs) gpuEstimateMs = deltaMs;
            }
            else
            {
                // No FPS cap - work equals frame time
                gpuEstimateMs = deltaMs;
            }

            // Store for UI display
            g_ReflexSleepTimeMs.store(actualSleepMs, std::memory_order_relaxed);

            if (gpuEstimateMs > 0.5 && gpuEstimateMs < 500.0)
            {
                // Store raw value for debug
                g_GpuWorkTimeRawMs.store(gpuEstimateMs, std::memory_order_relaxed);

                // Store in GPU time history for averaging
                {
                    std::lock_guard<std::mutex> lock(g_GpuTimeMutex);

                    g_GpuTimeHistory[g_GpuTimeHead] = gpuEstimateMs;
                    g_GpuTimeTimestamps[g_GpuTimeHead] = currentTime;
                    g_GpuTimeHead = (g_GpuTimeHead + 1) % WORK_TIME_HISTORY_SIZE;
                    if (g_GpuTimeCount < WORK_TIME_HISTORY_SIZE)
                        g_GpuTimeCount++;

                    double avgGpuTime = CalculateTrimmedMeanWithWindow(
                        g_GpuTimeHistory, g_GpuTimeTimestamps, g_GpuTimeHead, g_GpuTimeCount,
                        WORK_TIME_HISTORY_SIZE, currentTime, WORK_TIME_WINDOW_MS, 0.1);

                    g_GpuWorkTimeMs.store(avgGpuTime, std::memory_order_relaxed);
                }
            }
        }
#endif
        // Note: For FRAME_TIME_ALGORITHM == 1, GPU work time is calculated in ProcessMarker (PRESENT_END)

        // ---- Potential FPS calculation ----
        // Use max(CPU work time, GPU work time) for potential FPS
        double cpuWorkMs = g_CpuWorkTimeMs.load(std::memory_order_relaxed);
        double gpuWorkMs = g_GpuWorkTimeMs.load(std::memory_order_relaxed);

        double limitingFactor = 0.0;

        if (gpuWorkMs > 0.0 && cpuWorkMs > 0.0)
        {
            // Have both CPU and GPU data - use the larger one (bottleneck)
            limitingFactor = (cpuWorkMs > gpuWorkMs) ? cpuWorkMs : gpuWorkMs;
        }
        else if (gpuWorkMs > 0.0)
        {
            limitingFactor = gpuWorkMs;
        }
        else if (cpuWorkMs > 0.0)
        {
            limitingFactor = cpuWorkMs;
        }

        int currentFps = g_CurrentFps.load(std::memory_order_relaxed);
        ctx.reflex.currentFps = currentFps;

        if (limitingFactor > 0.0)
        {
            int basePotentialFps = (int)(1000.0 / limitingFactor + 0.5);
            int potentialFps;

            // Apply overhead factors
            // DLSS-G: base * 0.9 * 2 * 0.9 = base * 1.62
            // Non-DLSS-G: base * 0.9
            if (ctx.ngx.lastEvaluationTimeMsec > 0.0f)
            {
                // DLSS-G active: (base * overhead) * 2 * dlssg_overhead
                potentialFps = (int)(basePotentialFps * POTENTIAL_FPS_OVERHEAD_FACTOR * 2.0 * POTENTIAL_FPS_DLSSG_OVERHEAD_FACTOR + 0.5);
            }
            else
            {
                // Non-DLSS-G: base * overhead
                potentialFps = (int)(basePotentialFps * POTENTIAL_FPS_OVERHEAD_FACTOR + 0.5);
            }

            // Sanity check: potential FPS should never be lower than current FPS
            if (potentialFps < currentFps)
            {
                potentialFps = currentFps;
            }

            g_PotentialFps.store(potentialFps, std::memory_order_relaxed);
        }
        else
        {
            // Fallback to current FPS
            g_PotentialFps.store(currentFps, std::memory_order_relaxed);
        }

        // ---- Start timestamp for NEXT frame (D3D11) ----
        if (g_D3D11Queries.initialized)
        {
            D3D11_BeginTimestamp();
        }

        // ---- Dynamic MFG update (every frame, independent of UI) ----
        UpdateDynamicMfg();
    }

    // -------------------------------------------------------------------------
    // Marker processing (Reflex CPU timing)
    // -------------------------------------------------------------------------
    static void ProcessMarker(uint64_t frameId, uint32_t markerType, bool isAsync)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        ULONGLONG currentTime = GetTickCount64();

        g_LastMarkerType.store(markerType, std::memory_order_relaxed);

        switch (markerType)
        {
        case SIMULATION_START:
            PushSimStart(now, frameId);
#if FRAME_TIME_ALGORITHM == 1
            // New algorithm: store SimStart time for this frame
            g_CurrentFrameSimStartTime = now;
#endif
            break;

        case PRESENT_START:
        case OUT_OF_BAND_PRESENT_START:
        {
            g_LastPresentStartTime = now;

#if FRAME_TIME_ALGORITHM == 0
            // Old algorithm: measure CPU work time (SimStart -> PresentStart)
            LARGE_INTEGER simStartTime;
            uint64_t simFrameId;
            if (PopSimStart(&simStartTime, &simFrameId))
            {
                double cpuWorkTime = (double)(now.QuadPart - simStartTime.QuadPart)
                    * 1000.0 / (double)g_PerfFrequency.QuadPart;

                if (cpuWorkTime > 0.0 && cpuWorkTime < 1000.0)
                {
                    // Store in history for averaging
                    {
                        std::lock_guard<std::mutex> lock(g_WorkTimeMutex);

                        g_CpuWorkTimeHistory[g_WorkTimeHead] = cpuWorkTime;
                        g_WorkTimeTimestamps[g_WorkTimeHead] = currentTime;
                        g_WorkTimeHead = (g_WorkTimeHead + 1) % WORK_TIME_HISTORY_SIZE;
                        if (g_WorkTimeCount < WORK_TIME_HISTORY_SIZE)
                            g_WorkTimeCount++;

                        double avgCpuWorkTime = CalculateTrimmedMeanWithWindow(
                            g_CpuWorkTimeHistory, g_WorkTimeTimestamps, g_WorkTimeHead, g_WorkTimeCount,
                            WORK_TIME_HISTORY_SIZE, currentTime, WORK_TIME_WINDOW_MS, 0.1);

                        g_CpuWorkTimeMs.store(avgCpuWorkTime, std::memory_order_relaxed);
                    }

                    g_HasReflexData.store(true, std::memory_order_relaxed);
                }
            }
#endif
            break;
        }

        case PRESENT_END:
        case OUT_OF_BAND_PRESENT_END:
            if (g_LastPresentStartTime.QuadPart > 0)
            {
                double waitMs = (double)(now.QuadPart - g_LastPresentStartTime.QuadPart) * 1000.0 / (double)g_PerfFrequency.QuadPart;
                if (waitMs >= 0.0 && waitMs < 500.0)
                    g_PresentWaitTimeMs.store(waitMs, std::memory_order_relaxed);
            }

#if FRAME_TIME_ALGORITHM == 1
            // New algorithm: measure full frame time (SimStart -> PresentEnd)
            if (g_CurrentFrameSimStartTime.QuadPart > 0)
            {
                double frameTimeMs = (double)(now.QuadPart - g_CurrentFrameSimStartTime.QuadPart)
                    * 1000.0 / (double)g_PerfFrequency.QuadPart;

                if (frameTimeMs > 0.0 && frameTimeMs < 1000.0)
                {
                    g_SimToPresentEndMs.store(frameTimeMs, std::memory_order_relaxed);

                    // Also store as GPU work time for potential FPS calculation
                    {
                        std::lock_guard<std::mutex> lock(g_GpuTimeMutex);

                        g_GpuTimeHistory[g_GpuTimeHead] = frameTimeMs;
                        g_GpuTimeTimestamps[g_GpuTimeHead] = currentTime;
                        g_GpuTimeHead = (g_GpuTimeHead + 1) % WORK_TIME_HISTORY_SIZE;
                        if (g_GpuTimeCount < WORK_TIME_HISTORY_SIZE)
                            g_GpuTimeCount++;

                        double avgGpuTime = CalculateTrimmedMeanWithWindow(
                            g_GpuTimeHistory, g_GpuTimeTimestamps, g_GpuTimeHead, g_GpuTimeCount,
                            WORK_TIME_HISTORY_SIZE, currentTime, WORK_TIME_WINDOW_MS, 0.1);

                        g_GpuWorkTimeMs.store(avgGpuTime, std::memory_order_relaxed);
                    }

                    g_HasReflexData.store(true, std::memory_order_relaxed);
                }
            }
#endif
            break;

        default:
            break;
        }
    }

    static void OnLatencyMarker(void* pDevice, uint64_t frameId, uint32_t markerType, int result)
    {
        ProcessMarker(frameId, markerType, false);

        // Track base frame ID for latency calculation
        // PRESENT_START (4) or PRESENT_END (5) marks a base frame
        // This frameID can be matched with frameReport[] in GetLatency
        if (markerType == PRESENT_START || markerType == PRESENT_END)
        {
            g_LastBaseFrameId.store(frameId, std::memory_order_relaxed);
        }
    }

    static void OnAsyncFrameMarker(ID3D12CommandQueue* pQueue, uint64_t frameId, uint32_t markerType, int result)
    {
        ProcessMarker(frameId, markerType, true);

        // Try to initialize D3D12 timestamps if we have a command queue
        if (!g_D3D12Queries.initialized && pQueue)
        {
            ComPtr<ID3D12Device> pDevice;
            HRESULT hr = pQueue->GetDevice(__uuidof(ID3D12Device), (void**)&pDevice);
            if (SUCCEEDED(hr) && pDevice)
            {
                InitD3D12Timestamps(pDevice.Get(), pQueue);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Reflex Sleep callbacks - track time spent in frame limiter
    // -------------------------------------------------------------------------
    static void OnPreSleep(void* pDevice)
    {
        QueryPerformanceCounter(&g_SleepStartTime);

        // Check if Sleep started inside Present (between prePresent and postPresent)
        // If prePresentTime > postPresentTime, we're inside a Present call
        if (g_PrePresentTime.QuadPart > g_PostPresentTime.QuadPart)
        {
            g_SleepInsidePresent.store(true, std::memory_order_relaxed);
        }
        else
        {
            g_SleepInsidePresent.store(false, std::memory_order_relaxed);
        }
    }

    static void OnPostSleep(void* pDevice, int result)
    {
        if (g_SleepStartTime.QuadPart > 0)
        {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);

            g_SleepEndTime = now;

            double sleepMs = (double)(now.QuadPart - g_SleepStartTime.QuadPart) * 1000.0 / (double)g_PerfFrequency.QuadPart;

            if (sleepMs >= 0.0 && sleepMs < 500.0)
            {
                g_LastSleepDurationMs.store(sleepMs, std::memory_order_relaxed);

                // Calculate sleep + gap (time from postPresent to preSleep)
                double sleepWithGapMs = sleepMs;
                if (!g_SleepInsidePresent.load(std::memory_order_relaxed) && g_PostPresentTime.QuadPart > 0)
                {
                    // Sleep is after Present - add gap between postPresent and preSleep
                    double gapMs = (double)(g_SleepStartTime.QuadPart - g_PostPresentTime.QuadPart) * 1000.0 / (double)g_PerfFrequency.QuadPart;
                    if (gapMs > 0.0 && gapMs < 100.0)
                    {
                        sleepWithGapMs = sleepMs + gapMs;
                    }
                }
                g_LastSleepWithGapMs.store(sleepWithGapMs, std::memory_order_relaxed);
            }

            g_SleepStartTime.QuadPart = 0;
        }
    }

    // -------------------------------------------------------------------------
    // GetLatency callback - process latency data from NvAPI_D3D_GetLatency
    // -------------------------------------------------------------------------
    static void OnPostGetLatency(void* pDevice, NV_LATENCY_RESULT_PARAMS* pParams, int result)
    {
        if (result != 0 || !pParams)  // NVAPI_OK = 0
            return;

        ULONGLONG currentTime = GetTickCount64();
        g_LastLatencyUpdateTime.store(currentTime, std::memory_order_relaxed);

        // Find the frame with matching base frame ID (from SetLatencyMarker, not SetAsyncFrameMarker)
        uint64_t targetFrameId = g_LastBaseFrameId.load(std::memory_order_relaxed);

        const NV_LATENCY_RESULT_PARAMS::FrameReport* baseFrame = nullptr;

        if (targetFrameId > 0)
        {
            for (int i = 63; i >= 0; i--)
            {
                if (pParams->frameReport[i].frameID == targetFrameId &&
                    pParams->frameReport[i].gpuRenderStartTime > 0 &&
                    pParams->frameReport[i].gpuRenderEndTime > 0)
                {
                    baseFrame = &pParams->frameReport[i];
                    break;
                }
            }
        }

        if (!baseFrame)
            baseFrame = &pParams->frameReport[63];

        if (baseFrame->frameID == 0)
            return;

        // PC Latency = simDeltaUs + osRenderQueueDeltaUs + gpuRenderDeltaUs + presentDeltaUs
        // This matches NVIDIA's overlay calculation
        NvU64 simDeltaUs = 0;
        NvU64 osRenderQueueDeltaUs = 0;
        NvU64 gpuRenderDeltaUs = 0;
        NvU64 presentDeltaUs = 0;

        if (baseFrame->simEndTime > baseFrame->simStartTime)
            simDeltaUs = baseFrame->simEndTime - baseFrame->simStartTime;

        if (baseFrame->osRenderQueueEndTime > baseFrame->osRenderQueueStartTime)
            osRenderQueueDeltaUs = baseFrame->osRenderQueueEndTime - baseFrame->osRenderQueueStartTime;

        if (baseFrame->gpuRenderEndTime > baseFrame->gpuRenderStartTime)
            gpuRenderDeltaUs = baseFrame->gpuRenderEndTime - baseFrame->gpuRenderStartTime;

        if (baseFrame->presentEndTime > baseFrame->presentStartTime)
            presentDeltaUs = baseFrame->presentEndTime - baseFrame->presentStartTime;

        NvU64 totalLatencyUs = simDeltaUs + osRenderQueueDeltaUs + gpuRenderDeltaUs + presentDeltaUs;
        double latencyMs = (double)totalLatencyUs / 1000.0;

        // Debug log (once)
        static bool debugLogged = false;
        if (!debugLogged && totalLatencyUs > 0)
        {
            LOG_DEBUG(L"[FpsMon] GetLatency: sim=" + std::to_wstring(simDeltaUs)
                + L" osQueue=" + std::to_wstring(osRenderQueueDeltaUs)
                + L" gpuRender=" + std::to_wstring(gpuRenderDeltaUs)
                + L" present=" + std::to_wstring(presentDeltaUs)
                + L" total=" + std::to_wstring(totalLatencyUs)
                + L" ms=" + std::to_wstring((int)latencyMs));
            debugLogged = true;
        }

        // Sanity check: 1ms to 500ms
        if (latencyMs > 1.0 && latencyMs < 500.0)
        {
            std::lock_guard<std::mutex> lock(g_LatencyMutex);

            g_LatencyHistory[g_LatencyHead] = latencyMs;
            g_LatencyTimestamps[g_LatencyHead] = currentTime;
            g_LatencyHead = (g_LatencyHead + 1) % LATENCY_HISTORY_SIZE;
            if (g_LatencyCount < LATENCY_HISTORY_SIZE)
                g_LatencyCount++;

            double smoothedLatency = CalculateTrimmedMeanWithWindow(
                g_LatencyHistory, g_LatencyTimestamps, g_LatencyHead, g_LatencyCount,
                LATENCY_HISTORY_SIZE, currentTime, LATENCY_WINDOW_MS, 0.1);

            g_AverageLatencyMs.store(smoothedLatency, std::memory_order_relaxed);
        }
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------
    void Init()
    {
        if (g_Initialized) return;

        QueryPerformanceFrequency(&g_PerfFrequency);

        // Initialize arrays
        for (int i = 0; i < FRAME_TIME_BUFFER_SIZE; i++)
        {
            g_FrameTimes[i] = 0.0;
            g_FrameTimestamps[i] = 0;
        }
        for (int i = 0; i < WORK_TIME_HISTORY_SIZE; i++)
        {
            g_CpuWorkTimeHistory[i] = 0.0;
            g_GpuWorkTimeHistory[i] = 0.0;
            g_WorkTimeTimestamps[i] = 0;
            g_GpuTimeHistory[i] = 0.0;
            g_GpuTimeTimestamps[i] = 0;
        }
        for (size_t i = 0; i < SIM_START_QUEUE_SIZE; i++)
        {
            g_SimStartQueue[i] = {};
            g_SimStartFrameIds[i] = 0;
        }
        for (int i = 0; i < LATENCY_HISTORY_SIZE; i++)
        {
            g_LatencyHistory[i] = 0.0;
            g_LatencyTimestamps[i] = 0;
        }

        // Register SwapChain callbacks
        SwapChainEvents::RegisterPrePresent(OnPrePresent);
        SwapChainEvents::RegisterPostPresent(OnPostPresent);

        // Register Reflex callbacks
        ReflexEvents::RegisterPostSetLatencyMarker(OnLatencyMarker);
        ReflexEvents::RegisterPostSetAsyncFrameMarker(OnAsyncFrameMarker);

        // Register Reflex Sleep callbacks (for frame limiter timing)
        ReflexEvents::RegisterPreSleep(OnPreSleep);
        ReflexEvents::RegisterPostSleep(OnPostSleep);

        // Register GetLatency callback (for latency monitoring)
        ReflexEvents::RegisterPostGetLatency(OnPostGetLatency);

        LOG_INFO(L"[FpsMon] Initialized with GPU timestamp and latency support");

        g_Initialized = true;
    }

    // Public API implementation
    int GetCurrentFps() { return g_CurrentFps.load(std::memory_order_relaxed); }
    int GetPotentialFps() { return g_PotentialFps.load(std::memory_order_relaxed); }
    double GetFrameTimeMs() { return g_AvgFrameTimeMs.load(std::memory_order_relaxed); }

    double GetWorkTimeMs()
    {
        // Return the limiting factor (max of CPU and GPU)
        double cpu = g_CpuWorkTimeMs.load(std::memory_order_relaxed);
        double gpu = g_GpuWorkTimeMs.load(std::memory_order_relaxed);
        return (cpu > gpu) ? cpu : gpu;
    }

    double GetCpuWorkTimeMs() { return g_CpuWorkTimeMs.load(std::memory_order_relaxed); }
    double GetGpuWorkTimeMs() { return g_GpuWorkTimeMs.load(std::memory_order_relaxed); }
    double GetGpuWorkTimeRawMs() { return g_GpuWorkTimeRawMs.load(std::memory_order_relaxed); }
    double GetSimToPresentEndMs() { return g_SimToPresentEndMs.load(std::memory_order_relaxed); }
    double GetReflexSleepTimeMs() { return g_ReflexSleepTimeMs.load(std::memory_order_relaxed); }
    double GetSimulationTimeMs() { return g_SimulationTimeMs.load(std::memory_order_relaxed); }
    double GetRenderSubmitTimeMs() { return g_RenderSubmitTimeMs.load(std::memory_order_relaxed); }
    double GetPresentWaitTimeMs() { return g_PresentWaitTimeMs.load(std::memory_order_relaxed); }
    bool HasReflexData() { return g_HasReflexData.load(std::memory_order_relaxed); }
    bool HasGpuTimestamps() { return g_HasGpuTimestamps.load(std::memory_order_relaxed); }
    double GetLastSimTimeMs() { return g_CpuWorkTimeMs.load(std::memory_order_relaxed); }
    double GetLastRenderTimeMs() { return g_GpuWorkTimeMs.load(std::memory_order_relaxed); }
    uint32_t GetLastMarkerType() { return g_LastMarkerType.load(std::memory_order_relaxed); }

    // Latency API
    double GetAverageLatencyMs() { return g_AverageLatencyMs.load(std::memory_order_relaxed); }

    bool HasLatencyData()
    {
        ULONGLONG lastUpdate = g_LastLatencyUpdateTime.load(std::memory_order_relaxed);
        if (lastUpdate == 0)
            return false;

        ULONGLONG currentTime = GetTickCount64();
        return (currentTime - lastUpdate) < LATENCY_STALE_THRESHOLD_MS;
    }

    int GetBaseFps()
    {
        int currentFps = g_CurrentFps.load(std::memory_order_relaxed);
        int framesGenerated = ctx.ngx.framesGenerated; // 0 = no FG, 1 = 2X, 2 = 3X, 3 = 4X
        int multiplier = framesGenerated + 1;
        if (multiplier < 1) multiplier = 1;
        return currentFps / multiplier;
    }

    // =========================================================================
    // DYNAMIC FRAME GENERATION (DFG) � State Machine
    // =========================================================================
    //
    // DFG adjusts MFG multiplier based on two independent axes:
    //   1. Base FPS thresholds  � raise multiplier when GPU is slow
    //   2. Action Intensity     � lower multiplier during high action for latency
    //
    // The min-FPS constraint ensures that lowering the multiplier for latency
    // never drops output FPS below ctx.streamline.minDynamicFps.
    //
    // States:
    //   Disabled         � DFG off, pass-through
    //   WaitForFG        � frame gen not yet producing frames
    //   Steady           � mode matches threshold suggestion, intensity low
    //   ThresholdSwitch  � cooldown after normal threshold-based mode change
    //   Pinging          � DfgPing active (VSync trap escape)
    //   LatencyReduction � intensity high, MFG lowered toward minAllowedMode
    //   Recovery         � intensity dropped, ramping MFG back up step-by-step
    // =========================================================================

    // DfgState enum defined in FpsMonitor.h

    // Helper: float to wstring with 2 decimal places (for logs)
    static std::wstring fmtF(float v)
    {
        wchar_t buf[32];
        swprintf_s(buf, L"%.2f", v);
        return buf;
    }

    static const wchar_t* DfgStateName(DfgState s)
    {
        switch (s)
        {
        case DfgState::Disabled:          return L"Disabled";
        case DfgState::WaitForFG:         return L"WaitForFG";
        case DfgState::Steady:            return L"Steady";
        case DfgState::ThresholdSwitch:   return L"ThresholdSwitch";
        case DfgState::Pinging:           return L"Pinging";
        case DfgState::LatencyReduction:  return L"LatencyReduction";
        case DfgState::Recovery:          return L"Recovery";
        default:                          return L"Unknown";
        }
    }

    // -------------------------------------------------------------------------
    // State & config
    // -------------------------------------------------------------------------

    static DfgState g_DfgState = DfgState::Disabled;
    static ULONGLONG g_LastMfgSwitchTime = 0;

    // Cooldowns (ms)
    static const ULONGLONG THRESHOLD_SWITCH_COOLDOWN_MS = 2000;  // normal threshold switch
    static const ULONGLONG LATENCY_REDUCTION_COOLDOWN_MS = 200;   // fast reaction to action
    static const ULONGLONG RECOVERY_STEP_COOLDOWN_MS = 800;   // per-step ramp back up

    // Intensity hysteresis thresholds
    static const float INTENSITY_ENTER_THRESHOLD = 0.70f;  // enter LatencyReduction
    static const float INTENSITY_EXIT_THRESHOLD = 0.40f;  // exit to Recovery

    // Suggested mode (updated every frame, visible on status bar even when cooldown blocks switch)
    static std::atomic<int> g_DynamicMfgSuggestedMode{ 0 };

    // Track DFG state transitions to restore original mode on disable
    static bool g_WasDynamicMfgEnabled = false;
    static int  g_MfgModeBeforeDynamic = 0;

    // LatencyReduction/Recovery: the mode that thresholds want (before intensity override)
    static int  g_ThresholdSuggestedMode = 1;

    // -------------------------------------------------------------------------
    // DFG Ping � escape VSync traps at high MFG modes
    // -------------------------------------------------------------------------
    static ULONGLONG g_LastDfgPingTime = 0;
    static const ULONGLONG DFG_PING_COOLDOWN_MS = 2000;
    static const ULONGLONG DFG_PING_BACKOFF_MS = 10000;
    static std::atomic<bool> g_DfgPingActive{ false };
    static int      g_PingFromMode = 0;
    static bool     g_PingBounced = false;
    static ULONGLONG g_PingBounceTime = 0;

    // Bounce floor: when a ping (or Instinct reduction) to mode N bounces back,
    // record N as a "proven too low" floor. Neither Ping nor Instinct will go
    // below this until backoff expires. This prevents repeating the same failed
    // reduction within the backoff window.
    static int      g_BounceFloorMode = 0;         // 0 = no floor active
    static ULONGLONG g_BounceFloorExpiry = 0;       // tick when floor expires

    static int GetEffectiveBounceFloor(ULONGLONG now)
    {
        if (g_BounceFloorMode > 0 && now < g_BounceFloorExpiry)
            return g_BounceFloorMode + 1;  // can't go TO bounceFloor, minimum is one above
        return 0;  // no floor
    }

    static void SetBounceFloor(int failedMode, ULONGLONG now)
    {
        // Only raise the floor, never lower it (multiple bounces within window)
        if (failedMode >= g_BounceFloorMode || now >= g_BounceFloorExpiry)
        {
            g_BounceFloorMode = failedMode;
            g_BounceFloorExpiry = now + DFG_PING_BACKOFF_MS;
        }
    }

    // -------------------------------------------------------------------------
    // Max MFG mode - gated by Streamline version
    // -------------------------------------------------------------------------
    // Modes 4 (5X) and 5 (6X) require Streamline >= 2.11.x.
    // Cached on first call since interposerVersion doesn't change at runtime.

    static int GetMaxMfgMode()
    {
        static int s_MaxMode = -1;
        if (s_MaxMode >= 0)
            return s_MaxMode;

        // Parse ctx.streamline.dlssgVersion (e.g. L"2.11.0.0")
        // This is the sl.dlss_g plugin version which determines MFG capability.
        // Default is L"0.0.0.0" before SL loads - don't cache until real version arrives.
        const std::wstring& ver = ctx.streamline.dlssgVersion;
        int major = 0, minor = 0;
        if (swscanf_s(ver.c_str(), L"%d.%d", &major, &minor) >= 2)
        {
            bool loaded = (major > 0 || minor > 0);
            if (!loaded)
                return 3;  // SL not loaded yet, conservative default, no cache

            // 5X/6X requires sl.dlss_g >= 2.11 (reports maxFramesGenerated >= 5)
            bool sl211 = (major > 2) || (major == 2 && minor >= 11);
            s_MaxMode = sl211 ? 5 : 3;  // 5 = 6X, 3 = 4X

            LOG_INFO(L"[DMFG] GetMaxMfgMode: dlssg version " + std::to_wstring(major)
                + L"." + std::to_wstring(minor) + L" -> maxMode=" + std::to_wstring(s_MaxMode)
                + L" (sl211=" + std::to_wstring(sl211 ? 1 : 0) + L")");
        }
        else
        {
            return 3;  // parse failed, don't cache
        }

        return s_MaxMode;
    }

    // -------------------------------------------------------------------------
    // BaseFPS-per-mode cache - shared knowledge between Instinct and Ping
    // -------------------------------------------------------------------------
    // When Instinct runs at a lower mode, we observe the actual baseFps at that
    // mode. This data is cached so that Ping can predict whether dropping to a
    // given mode would sustain target FPS - without needing to actually ping.
    //
    // Cache entries expire after MODE_FPS_CACHE_TTL_MS because GPU load changes
    // over time (different scenes, different complexity).

    static const ULONGLONG MODE_FPS_CACHE_TTL_MS = 8000;  // 8 seconds

    struct ModeFpsEntry
    {
        int      baseFps = 0;
        ULONGLONG timestamp = 0;
    };

    // Index by MFG mode (0=unused, 1=2X, 2=3X, 3=4X, 4=5X, 5=6X)
    static ModeFpsEntry g_ModeFpsCache[6] = {};

    // Record observed baseFps at the current mode
    static void CacheModeFps(int mode, int baseFps, ULONGLONG now)
    {
        if (mode < 1 || mode > 5) return;
        g_ModeFpsCache[mode] = { baseFps, now };
    }

    // Query: can we predict baseFps at a given mode from cache?
    // Returns true and fills outBaseFps if we have a fresh-enough entry.
    static bool GetCachedModeFps(int mode, ULONGLONG now, int* outBaseFps)
    {
        if (mode < 1 || mode > 5) return false;
        const auto& entry = g_ModeFpsCache[mode];
        if (entry.timestamp == 0) return false;
        if (now - entry.timestamp > MODE_FPS_CACHE_TTL_MS) return false;
        if (outBaseFps) *outBaseFps = entry.baseFps;
        return true;
    }

    // Predict baseFps at targetMode using cache or linear extrapolation.
    // baseFps is roughly constant across modes (GPU renders at same rate),
    // but VSync can lock it. Returns 0 if no prediction is possible.
    static int PredictBaseFpsAtMode(int targetMode, int currentMode, int currentBaseFps, ULONGLONG now)
    {
        // Direct cache hit for the target mode
        int cached = 0;
        if (GetCachedModeFps(targetMode, now, &cached))
            return cached;

        // Fallback: baseFps is approximately the same regardless of MFG mode
        // (GPU renders same number of base frames, MFG just interpolates between).
        // This assumption breaks under VSync � which is exactly what Ping tests.
        // Return current baseFps as best-effort estimate.
        return currentBaseFps;
    }

    static void ResetModeFpsCache()
    {
        for (auto& e : g_ModeFpsCache)
            e = {};
    }

    // -------------------------------------------------------------------------
    // BaseFPS stability ring buffer (for DFG Ping)
    // -------------------------------------------------------------------------
    static const int       BASE_FPS_HISTORY_SIZE = 8;
    static const ULONGLONG BASE_FPS_SAMPLE_INTERVAL_MS = 250;
    static const int       BASE_FPS_STABLE_RANGE = 3;
    static int      g_BaseFpsHistory[BASE_FPS_HISTORY_SIZE] = {};
    static int      g_BaseFpsHistoryCount = 0;
    static int      g_BaseFpsHistoryHead = 0;
    static ULONGLONG g_LastBaseFpsSampleTime = 0;

    static void SampleBaseFps(ULONGLONG now)
    {
        if (now - g_LastBaseFpsSampleTime < BASE_FPS_SAMPLE_INTERVAL_MS)
            return;
        g_LastBaseFpsSampleTime = now;

        int baseFps = GetBaseFps();
        g_BaseFpsHistory[g_BaseFpsHistoryHead] = baseFps;
        g_BaseFpsHistoryHead = (g_BaseFpsHistoryHead + 1) % BASE_FPS_HISTORY_SIZE;
        if (g_BaseFpsHistoryCount < BASE_FPS_HISTORY_SIZE)
            g_BaseFpsHistoryCount++;

        // Feed the per-mode cache so Ping can use Instinct's observations
        int currentMode = ctx.streamline.mfgEnforcedMode;
        CacheModeFps(currentMode, baseFps, now);
    }

    static bool IsBaseFpsStable(int* outMin = nullptr, int* outMax = nullptr)
    {
        if (g_BaseFpsHistoryCount < BASE_FPS_HISTORY_SIZE)
            return false;

        int minFps = g_BaseFpsHistory[0];
        int maxFps = g_BaseFpsHistory[0];
        for (int i = 1; i < BASE_FPS_HISTORY_SIZE; i++)
        {
            if (g_BaseFpsHistory[i] < minFps) minFps = g_BaseFpsHistory[i];
            if (g_BaseFpsHistory[i] > maxFps) maxFps = g_BaseFpsHistory[i];
        }

        if (outMin) *outMin = minFps;
        if (outMax) *outMax = maxFps;

        return (maxFps - minFps) <= BASE_FPS_STABLE_RANGE;
    }

    // -------------------------------------------------------------------------
    // Public accessors
    // -------------------------------------------------------------------------

    int GetDynamicMfgSuggestedMode()
    {
        return g_DynamicMfgSuggestedMode.load(std::memory_order_relaxed);
    }

    bool IsDfgPingActive()
    {
        return g_DfgPingActive.load(std::memory_order_relaxed);
    }

    DfgState GetDfgState()
    {
        return g_DfgState;
    }

    const wchar_t* GetDfgStateName()
    {
        return DfgStateName(g_DfgState);
    }

    // Forward-declare
    static void NotifyStreamlineMfgChange(int newMode);
    static int ComputeMinAllowedMode(int baseFps, float minOutputFps);
    static int ComputeThresholdMode(int baseFps);

    // -------------------------------------------------------------------------
    // DFG Ping helpers
    // -------------------------------------------------------------------------

    static bool TryDfgPing(ULONGLONG now)
    {
        int currentMode = ctx.streamline.mfgEnforcedMode;

        // Only ping from 3X (mode=2) or higher
        if (currentMode < 2)
            return false;

        // If last ping bounced back to same mode, use longer backoff
        if (g_PingBounced && currentMode == g_PingFromMode)
        {
            if (now - g_PingBounceTime < DFG_PING_BACKOFF_MS)
                return false;
            g_PingBounced = false;
        }

        if (now - g_LastDfgPingTime < DFG_PING_COOLDOWN_MS)
            return false;

        int minFps = 0, maxFps = 0;
        if (!IsBaseFpsStable(&minFps, &maxFps))
            return false;

        int baseFps = GetBaseFps();
        int newMode = currentMode - 1;
        int oldMode = currentMode;

        // Don't ping below minDynamicFps constraint
        if (ctx.streamline.isMinDynamicFpsActive)
        {
            int minAllowed = ComputeMinAllowedMode(baseFps, ctx.streamline.minDynamicFps);
            if (newMode < minAllowed)
                return false;
        }

        // Don't ping below bounce floor (proven too low within backoff window)
        int floor = GetEffectiveBounceFloor(now);
        if (floor > 0 && newMode < floor)
            return false;

        // Check cache: if Instinct recently ran at newMode and we know the
        // baseFps there, predict whether the ping would bounce. This avoids
        // a disruptive mode switch when we already know the answer.
        int predictedBaseFps = PredictBaseFpsAtMode(newMode, currentMode, baseFps, now);
        if (predictedBaseFps > 0)
        {
            int thresh = ComputeThresholdMode(predictedBaseFps);
            // If threshold logic at predicted baseFps would want a mode >= currentMode,
            // the ping will bounce back � skip it.
            if (thresh >= currentMode)
            {
                LOG_TRACE(L"[DMFG] DfgPing skipped: cache predicts baseFps="
                    + std::to_wstring(predictedBaseFps)
                    + L" at mode=" + std::to_wstring(newMode)
                    + L" -> threshMode=" + std::to_wstring(thresh)
                    + L" (would bounce)");
                return false;
            }
        }

        ctx.streamline.mfgEnforcedMode = newMode;
        g_LastDfgPingTime = now;
        g_LastMfgSwitchTime = now;
        g_DfgPingActive.store(true, std::memory_order_relaxed);
        g_PingFromMode = oldMode;

        g_BaseFpsHistoryCount = 0;
        g_BaseFpsHistoryHead = 0;

        LOG_INFO(L"[DMFG] DfgPing: " + std::to_wstring(oldMode) + L" -> " + std::to_wstring(newMode)
            + L" | baseFps=" + std::to_wstring(baseFps)
            + L" stable=[" + std::to_wstring(minFps) + L"-" + std::to_wstring(maxFps) + L"]"
            + L" framesGen=" + std::to_wstring(ctx.ngx.framesGenerated));

        NotifyStreamlineMfgChange(newMode);
        return true;
    }

    // -------------------------------------------------------------------------
    // Min-FPS constraint helpers
    // -------------------------------------------------------------------------

    // Lowest MFG mode that keeps outputFps >= minOutputFps
    //   outputFps = baseFps * (mode + 1)
    //   mode >= ceil(minOutputFps / baseFps) - 1
    static int ComputeMinAllowedMode(int baseFps, float minOutputFps)
    {
        if (baseFps <= 0)
            return GetMaxMfgMode(); // no data -> don't reduce, stay at max

        int minMode = (int)ceilf(minOutputFps / (float)baseFps) - 1;

        // Clamp to valid range: 1 (2X) .. GetMaxMfgMode()
        if (minMode < 1) minMode = 1;
        if (minMode > GetMaxMfgMode()) minMode = GetMaxMfgMode();
        return minMode;
    }

    // Map intensity to target mode: higher intensity -> lower mode (better latency)
    // Linear interpolation from suggestedMode (at enterThreshold) down to mode 1 (at intensity 1.0),
    // clamped by minAllowedMode from the min-FPS constraint.
    // Uses floor (not round) so that partial steps don't trigger a reduction �
    // avoids flicker when intensity hovers near the enter threshold.
    static int ComputeIntensityTargetMode(int suggestedMode, float intensity, int minAllowedMode)
    {
        if (suggestedMode <= 1)
            return 1; // already at minimum

        float t = (intensity - INTENSITY_ENTER_THRESHOLD)
            / (1.0f - INTENSITY_ENTER_THRESHOLD);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        // At t=0 -> suggestedMode (no reduction), t=1 -> mode 1 (max reduction)
        // floor: need t >= 0.5 for first step down (e.g. intensity >= 0.85 at threshMode=3)
        int stepsDown = (int)(t * (float)(suggestedMode - 1));
        int targetMode = suggestedMode - stepsDown;

        if (targetMode < minAllowedMode)
            targetMode = minAllowedMode;
        if (targetMode < 1) targetMode = 1;

        return targetMode;
    }

    // Compute threshold-based suggested mode (pure FPS logic, no intensity)
    static int ComputeThresholdMode(int baseFps)
    {
        int maxMode = GetMaxMfgMode();

        // One-shot diagnostic log
        static bool s_LoggedThresholds = false;
        if (!s_LoggedThresholds && maxMode > 0)
        {
            s_LoggedThresholds = true;
            LOG_INFO(L"[DMFG] ComputeThresholdMode: maxMode=" + std::to_wstring(maxMode)
                + L" thresh=[" + std::to_wstring(ctx.streamline.dynamicMfgThreshold6)
                + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold5)
                + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold4)
                + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold3)
                + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold2) + L"]"
                + L" (6X/5X/4X/3X/2X)");
        }

        // x6/x5 thresholds only evaluated when SL >= 2.11
        if (maxMode >= 5)
        {
            int thresh6 = ctx.streamline.dynamicMfgThreshold6;
            if (baseFps < thresh6) return 5; // 6X
        }
        if (maxMode >= 4)
        {
            int thresh5 = ctx.streamline.dynamicMfgThreshold5;
            if (baseFps < thresh5) return 4; // 5X
        }

        int thresh4 = ctx.streamline.dynamicMfgThreshold4;
        int thresh3 = ctx.streamline.dynamicMfgThreshold3;
        int thresh2 = ctx.streamline.dynamicMfgThreshold2;

        if (baseFps < thresh4) return 3; // 4X
        if (baseFps < thresh3) return 2; // 3X
        if (baseFps < thresh2) return 1; // 2X
        return 1; // 2X minimum
    }

    // -------------------------------------------------------------------------
    // State transition helper � applies mode change + resets stability tracking
    // -------------------------------------------------------------------------

    static void TransitionTo(DfgState newState, int newMode, ULONGLONG now, const wchar_t* reason)
    {
        DfgState oldState = g_DfgState;
        int oldMode = ctx.streamline.mfgEnforcedMode;

        g_DfgState = newState;

        if (newMode != oldMode)
        {
            ctx.streamline.mfgEnforcedMode = newMode;
            g_LastMfgSwitchTime = now;

            // Reset stability tracking when mode changes
            g_BaseFpsHistoryCount = 0;
            g_BaseFpsHistoryHead = 0;

            NotifyStreamlineMfgChange(newMode);
        }

        LOG_INFO(L"[DMFG] " + std::wstring(DfgStateName(oldState))
            + L" -> " + DfgStateName(newState)
            + L" | mode " + std::to_wstring(oldMode) + L" -> " + std::to_wstring(newMode)
            + L" | " + reason);
    }

    // =========================================================================
    // UpdateDynamicMfg � main DFG tick, called every frame
    // =========================================================================

    void UpdateDynamicMfg()
    {
        bool isEnabled = ctx.streamline.isDynamicMfgEnabled;

        // -----------------------------------------------------------------
        // Enable/disable edge detection
        // -----------------------------------------------------------------
        if (isEnabled && !g_WasDynamicMfgEnabled)
        {
            g_MfgModeBeforeDynamic = ctx.streamline.mfgEnforcedMode;
            g_DfgState = DfgState::WaitForFG;
            LOG_DEBUG(L"[DMFG] Enabled, saving original mfgEnforcedMode=" + std::to_wstring(g_MfgModeBeforeDynamic));

            // Ensure threshold5/6 are initialized (may be 0 if Context.h defaults missing)
            if (ctx.streamline.dynamicMfgThreshold5 <= 0 || ctx.streamline.dynamicMfgThreshold6 <= 0)
            {
                int target = ctx.streamline.dfgTargetFps > 0 ? ctx.streamline.dfgTargetFps : 120;
                if (ctx.streamline.dynamicMfgThreshold5 <= 0)
                    ctx.streamline.dynamicMfgThreshold5 = (target + 4) / 5;
                if (ctx.streamline.dynamicMfgThreshold6 <= 0)
                    ctx.streamline.dynamicMfgThreshold6 = (target + 5) / 6;
                LOG_INFO(L"[DMFG] Auto-initialized threshold5=" + std::to_wstring(ctx.streamline.dynamicMfgThreshold5)
                    + L" threshold6=" + std::to_wstring(ctx.streamline.dynamicMfgThreshold6)
                    + L" from targetFps=" + std::to_wstring(target));
            }
        }

        if (!isEnabled && g_WasDynamicMfgEnabled)
        {
            LOG_DEBUG(L"[DMFG] Disabled, restoring game's original DLSSG options");
            ctx.streamline.mfgEnforcedMode = g_MfgModeBeforeDynamic;
            StreamlineProxy_RestoreGameDLSSGOptions();
        }

        g_WasDynamicMfgEnabled = isEnabled;

        if (!isEnabled)
        {
            g_DfgState = DfgState::Disabled;
            g_DynamicMfgSuggestedMode.store(0, std::memory_order_relaxed);
            g_DfgPingActive.store(false, std::memory_order_relaxed);
            g_BaseFpsHistoryCount = 0;
            g_BaseFpsHistoryHead = 0;
            g_PingBounced = false;
            g_BounceFloorMode = 0;
            g_BounceFloorExpiry = 0;
            ResetModeFpsCache();
            g_ThresholdSuggestedMode = 1;
            return;
        }

        // -----------------------------------------------------------------
        // WaitForFG: defer until frame gen is producing frames
        // -----------------------------------------------------------------
        if (ctx.ngx.framesGenerated == 0)
        {
            g_DfgState = DfgState::WaitForFG;
            return;
        }

        if (g_DfgState == DfgState::WaitForFG || g_DfgState == DfgState::Disabled)
            g_DfgState = DfgState::Steady;

        // -----------------------------------------------------------------
        // Per-frame sampling
        // -----------------------------------------------------------------
        ULONGLONG now = GetTickCount64();
        SampleBaseFps(now);

        int baseFps = GetBaseFps();
        int currentMode = ctx.streamline.mfgEnforcedMode;

        // Threshold-based suggestion (always computed, even during intensity override)
        int threshMode = ComputeThresholdMode(baseFps);
        g_ThresholdSuggestedMode = threshMode;

        // Action Intensity
        ActionIntensity::Snapshot ai = ActionIntensity::GetSnapshot();
        bool intensityAvailable = ai.isAvailable && ctx.streamline.isMinDynamicFpsActive;
        float intensity = intensityAvailable ? ai.intensity : 0.0f;

        // Min-FPS constraint (Instinct's own hard floor � typically lower than target FPS)
        float minOutputFps = ctx.streamline.minDynamicFps;
        int minAllowedMode = ComputeMinAllowedMode(baseFps, minOutputFps);

        // NOTE: Bounce floor from ping failures is tracked separately (g_BounceFloorMode)
        // and enforced in TryDfgPing. It does NOT constrain Instinct's minAllowedMode
        // because ping bounces prove "mode N can't sustain target FPS", while Instinct
        // operates against a different, lower threshold (minDynamicFps). A mode that
        // fails for target FPS may be perfectly valid for minDynamicFps.

        // For status bar: show what we *want* (may differ from current during cooldown)
        // Updated below per state, default to threshold suggestion
        int suggestedForDisplay = threshMode;

        // -----------------------------------------------------------------
        // State machine
        // -----------------------------------------------------------------
        switch (g_DfgState)
        {
            // =================================================================
        case DfgState::Steady:
        {
            // Check: should we enter LatencyReduction?
            if (intensityAvailable && intensity > INTENSITY_ENTER_THRESHOLD)
            {
                int targetMode = ComputeIntensityTargetMode(threshMode, intensity, minAllowedMode);
                suggestedForDisplay = targetMode;

                if (targetMode < currentMode)
                {
                    TransitionTo(DfgState::LatencyReduction, targetMode, now,
                        (L"intensity=" + fmtF(intensity)
                            + L" target=" + std::to_wstring(targetMode)
                            + L" minAllowed=" + std::to_wstring(minAllowedMode)).c_str());
                }
                break;
            }

            suggestedForDisplay = threshMode;

            // Check: threshold wants a different mode?
            if (threshMode != currentMode)
            {
                if (now - g_LastMfgSwitchTime >= THRESHOLD_SWITCH_COOLDOWN_MS)
                {
                    // Detect bounce-back from DfgPing
                    if (g_DfgPingActive.load(std::memory_order_relaxed) && threshMode == g_PingFromMode)
                    {
                        g_PingBounced = true;
                        g_PingBounceTime = now;
                        SetBounceFloor(currentMode, now);  // currentMode = the mode we pinged TO
                        LOG_INFO(L"[DMFG] DfgPing bounce-back detected, floor="
                            + std::to_wstring(currentMode)
                            + L" backoff " + std::to_wstring(DFG_PING_BACKOFF_MS / 1000) + L"s");
                    }
                    g_DfgPingActive.store(false, std::memory_order_relaxed);

                    TransitionTo(DfgState::ThresholdSwitch, threshMode, now,
                        (L"baseFps=" + std::to_wstring(baseFps)
                            + L" thresholds=[" + std::to_wstring(ctx.streamline.dynamicMfgThreshold6)
                            + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold5)
                            + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold4)
                            + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold3)
                            + L"/" + std::to_wstring(ctx.streamline.dynamicMfgThreshold2) + L"]").c_str());
                }
                break;
            }

            // Steady and mode matches � try DFG Ping to escape VSync trap
            g_DfgPingActive.store(false, std::memory_order_relaxed);
            TryDfgPing(now);

            if (g_DfgPingActive.load(std::memory_order_relaxed))
                g_DfgState = DfgState::Pinging;

            break;
        }

        // =================================================================
        case DfgState::ThresholdSwitch:
        {
            suggestedForDisplay = threshMode;

            // ThresholdSwitch is a transient state � go back to Steady
            // once cooldown has passed (next frame effectively)
            if (now - g_LastMfgSwitchTime >= THRESHOLD_SWITCH_COOLDOWN_MS)
                g_DfgState = DfgState::Steady;

            // But: if intensity spikes during cooldown, override immediately
            if (intensityAvailable && intensity > INTENSITY_ENTER_THRESHOLD)
            {
                int targetMode = ComputeIntensityTargetMode(threshMode, intensity, minAllowedMode);
                suggestedForDisplay = targetMode;

                if (targetMode < currentMode
                    && now - g_LastMfgSwitchTime >= LATENCY_REDUCTION_COOLDOWN_MS)
                {
                    TransitionTo(DfgState::LatencyReduction, targetMode, now,
                        (L"intensity spike during ThresholdSwitch, intensity="
                            + fmtF(intensity)).c_str());
                }
            }
            break;
        }

        // =================================================================
        case DfgState::Pinging:
        {
            suggestedForDisplay = threshMode;

            // DfgPing resolves itself: either DFG threshold logic switches mode
            // (bounce-back), or the new mode sticks. Either way, return to Steady.
            if (!g_DfgPingActive.load(std::memory_order_relaxed))
            {
                g_DfgState = DfgState::Steady;
                break;
            }

            // Check threshold wants to switch (bounce-back detection)
            if (threshMode != currentMode
                && now - g_LastMfgSwitchTime >= THRESHOLD_SWITCH_COOLDOWN_MS)
            {
                if (threshMode == g_PingFromMode)
                {
                    g_PingBounced = true;
                    g_PingBounceTime = now;
                    SetBounceFloor(currentMode, now);  // currentMode = the mode we pinged TO
                    LOG_INFO(L"[DMFG] DfgPing bounce-back in Pinging state, floor="
                        + std::to_wstring(currentMode)
                        + L" backoff " + std::to_wstring(DFG_PING_BACKOFF_MS / 1000) + L"s");
                }

                g_DfgPingActive.store(false, std::memory_order_relaxed);

                TransitionTo(DfgState::Steady, threshMode, now,
                    L"Ping resolved via threshold switch");
            }

            // Intensity override even during ping
            if (intensityAvailable && intensity > INTENSITY_ENTER_THRESHOLD)
            {
                int targetMode = ComputeIntensityTargetMode(threshMode, intensity, minAllowedMode);
                if (targetMode < currentMode)
                {
                    g_DfgPingActive.store(false, std::memory_order_relaxed);
                    TransitionTo(DfgState::LatencyReduction, targetMode, now,
                        L"intensity override during Ping");
                }
            }
            break;
        }

        // =================================================================
        case DfgState::LatencyReduction:
        {
            // Re-evaluate target every frame (intensity and baseFps change)
            int targetMode = ComputeIntensityTargetMode(threshMode, intensity, minAllowedMode);
            suggestedForDisplay = targetMode;

            // Exit condition: intensity dropped below exit threshold (hysteresis)
            if (!intensityAvailable || intensity < INTENSITY_EXIT_THRESHOLD)
            {
                // Transition to Recovery � will ramp back up to threshMode
                TransitionTo(DfgState::Recovery, currentMode, now,
                    (L"intensity=" + fmtF(intensity)
                        + L" < exitThresh=" + fmtF(INTENSITY_EXIT_THRESHOLD)).c_str());
                break;
            }

            // Only allow going FURTHER DOWN, never up.
            // Going up from LatencyReduction is Recovery's job.
            if (targetMode < currentMode
                && now - g_LastMfgSwitchTime >= LATENCY_REDUCTION_COOLDOWN_MS)
            {
                TransitionTo(DfgState::LatencyReduction, targetMode, now,
                    (L"deeper: intensity=" + fmtF(intensity)
                        + L" target=" + std::to_wstring(targetMode)
                        + L" minAllowed=" + std::to_wstring(minAllowedMode)).c_str());
            }

            // If minAllowedMode rose above currentMode (baseFps dropped sharply),
            // we must go up � route through Recovery to ramp safely.
            if (minAllowedMode > currentMode)
            {
                TransitionTo(DfgState::Recovery, minAllowedMode, now,
                    (L"minAllowed=" + std::to_wstring(minAllowedMode)
                        + L" > current=" + std::to_wstring(currentMode)
                        + L", forced up").c_str());
            }
            break;
        }

        // =================================================================
        case DfgState::Recovery:
        {
            suggestedForDisplay = threshMode;

            // If intensity spikes again, go right back to LatencyReduction
            if (intensityAvailable && intensity > INTENSITY_ENTER_THRESHOLD)
            {
                int targetMode = ComputeIntensityTargetMode(threshMode, intensity, minAllowedMode);
                suggestedForDisplay = targetMode;

                if (targetMode < currentMode)
                {
                    TransitionTo(DfgState::LatencyReduction, targetMode, now,
                        L"intensity re-spike during Recovery");
                    break;
                }
            }

            // Ramp up one step at a time toward threshMode
            if (currentMode >= threshMode)
            {
                // Already at or above threshold target � done
                g_DfgState = DfgState::Steady;
                LOG_INFO(L"[DMFG] Recovery complete, entering Steady at mode="
                    + std::to_wstring(currentMode));
                break;
            }

            if (now - g_LastMfgSwitchTime >= RECOVERY_STEP_COOLDOWN_MS)
            {
                int nextMode = currentMode + 1;
                if (nextMode > threshMode)
                    nextMode = threshMode;

                TransitionTo(DfgState::Recovery, nextMode, now,
                    (L"ramp step " + std::to_wstring(currentMode)
                        + L" -> " + std::to_wstring(nextMode)
                        + L" (target=" + std::to_wstring(threshMode) + L")").c_str());
            }
            break;
        }

        default:
            g_DfgState = DfgState::Steady;
            break;
        }

        g_DynamicMfgSuggestedMode.store(suggestedForDisplay, std::memory_order_relaxed);
    }

    static void NotifyStreamlineMfgChange(int newMode)
    {
        LOG_DEBUG(L"[DMFG] Calling StreamlineProxy_ForceApplyMfgMode with mfgEnforcedMode="
            + std::to_wstring(ctx.streamline.mfgEnforcedMode));
        StreamlineProxy_ForceApplyMfgMode();
    }
}