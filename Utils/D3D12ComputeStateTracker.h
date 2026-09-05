#pragma once
// =============================================================================
// D3D12ComputeStateTracker.h
// =============================================================================
// Tracks the last-set compute pipeline state on a D3D12 command list by hooking
// methods via Microsoft Detours. Allows save/restore of compute state when
// injecting custom compute passes (like SSRTGI) into a game's command list.
// =============================================================================

#include <d3d12.h>
#include <mutex>
#include <unordered_map>
#include "../Detours/detours.h"
#include "Common.h"

namespace D3D12ComputeStateTracker
{
    struct ComputeStateSnapshot
    {
        ID3D12PipelineState* pipelineState = nullptr;
        ID3D12RootSignature* computeRootSignature = nullptr;
        ID3D12DescriptorHeap* descriptorHeaps[2] = {};
        UINT                  numDescriptorHeaps = 0;
        bool                  valid = false;
    };

    struct TrackedState
    {
        ID3D12PipelineState* lastPSO = nullptr;
        ID3D12RootSignature* lastComputeRootSig = nullptr;
        ID3D12DescriptorHeap* lastHeaps[2] = {};
        UINT                  lastNumHeaps = 0;
        uint64_t              setPSOCount = 0;
        uint64_t              setRootSigCount = 0;
        uint64_t              setHeapsCount = 0;
    };

    namespace detail
    {
        inline std::mutex g_mutex;
        inline std::unordered_map<ID3D12GraphicsCommandList*, TrackedState> g_states;
        inline bool g_hooksInstalled = false;
        inline uint64_t g_totalSetPSOCalls = 0;
        inline uint64_t g_totalSetRootSigCalls = 0;
        inline uint64_t g_totalSetHeapsCalls = 0;

        using PFN_SetPipelineState = void (STDMETHODCALLTYPE*)(
            ID3D12GraphicsCommandList*, ID3D12PipelineState*);
        using PFN_SetComputeRootSignature = void (STDMETHODCALLTYPE*)(
            ID3D12GraphicsCommandList*, ID3D12RootSignature*);
        using PFN_SetDescriptorHeaps = void (STDMETHODCALLTYPE*)(
            ID3D12GraphicsCommandList*, UINT, ID3D12DescriptorHeap* const*);
        using PFN_Reset = HRESULT(STDMETHODCALLTYPE*)(
            ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*);

        inline PFN_SetPipelineState        g_origSetPipelineState = nullptr;
        inline PFN_SetComputeRootSignature g_origSetComputeRootSignature = nullptr;
        inline PFN_SetDescriptorHeaps      g_origSetDescriptorHeaps = nullptr;
        inline PFN_Reset                   g_origReset = nullptr;

        // VTable indices for ID3D12GraphicsCommandList
        static constexpr int kVTIdx_Reset = 10;
        static constexpr int kVTIdx_SetPipelineState = 25;
        static constexpr int kVTIdx_SetDescriptorHeaps = 28;
        static constexpr int kVTIdx_SetComputeRootSignature = 29;

        inline void** GetVTable(void* obj)
        {
            return *reinterpret_cast<void***>(obj);
        }

        inline void STDMETHODCALLTYPE Hook_SetPipelineState(
            ID3D12GraphicsCommandList* self, ID3D12PipelineState* pso)
        {
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                auto& s = g_states[self];
                s.lastPSO = pso;
                s.setPSOCount++;
                g_totalSetPSOCalls++;
            }
            g_origSetPipelineState(self, pso);
        }

        inline void STDMETHODCALLTYPE Hook_SetComputeRootSignature(
            ID3D12GraphicsCommandList* self, ID3D12RootSignature* rootSig)
        {
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                auto& s = g_states[self];
                s.lastComputeRootSig = rootSig;
                s.setRootSigCount++;
                g_totalSetRootSigCalls++;
            }
            g_origSetComputeRootSignature(self, rootSig);
        }

        inline void STDMETHODCALLTYPE Hook_SetDescriptorHeaps(
            ID3D12GraphicsCommandList* self, UINT numHeaps, ID3D12DescriptorHeap* const* heaps)
        {
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                auto& s = g_states[self];
                s.lastNumHeaps = (numHeaps <= 2) ? numHeaps : 2;
                for (UINT i = 0; i < s.lastNumHeaps; i++)
                    s.lastHeaps[i] = heaps[i];
                for (UINT i = s.lastNumHeaps; i < 2; i++)
                    s.lastHeaps[i] = nullptr;
                s.setHeapsCount++;
                g_totalSetHeapsCalls++;
            }
            g_origSetDescriptorHeaps(self, numHeaps, heaps);
        }

        inline HRESULT STDMETHODCALLTYPE Hook_Reset(
            ID3D12GraphicsCommandList* self, ID3D12CommandAllocator* alloc, ID3D12PipelineState* initState)
        {
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_states[self] = TrackedState{};
                g_states[self].lastPSO = initState;
            }
            return g_origReset(self, alloc, initState);
        }
    }

    // =========================================================================
    // Public API
    // =========================================================================

    inline bool InstallHooks(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList) return false;
        if (detail::g_hooksInstalled) return true;

        void** vtable = detail::GetVTable(cmdList);
        if (!vtable) return false;

        // Log vtable addresses for debugging
        void* pSetPSO = vtable[detail::kVTIdx_SetPipelineState];
        void* pSetRootSig = vtable[detail::kVTIdx_SetComputeRootSignature];
        void* pSetHeaps = vtable[detail::kVTIdx_SetDescriptorHeaps];
        void* pReset = vtable[detail::kVTIdx_Reset];

        LOG_INFO(L"[StateTracker] cmdList ptr: " + std::to_wstring((uintptr_t)cmdList));
        LOG_INFO(L"[StateTracker] vtable ptr: " + std::to_wstring((uintptr_t)vtable));
        LOG_INFO(L"[StateTracker] SetPipelineState[25]: " + std::to_wstring((uintptr_t)pSetPSO));
        LOG_INFO(L"[StateTracker] SetComputeRootSig[30]: " + std::to_wstring((uintptr_t)pSetRootSig));
        LOG_INFO(L"[StateTracker] SetDescriptorHeaps[28]: " + std::to_wstring((uintptr_t)pSetHeaps));
        LOG_INFO(L"[StateTracker] Reset[10]: " + std::to_wstring((uintptr_t)pReset));

        detail::g_origSetPipelineState =
            reinterpret_cast<detail::PFN_SetPipelineState>(pSetPSO);
        detail::g_origSetComputeRootSignature =
            reinterpret_cast<detail::PFN_SetComputeRootSignature>(pSetRootSig);
        detail::g_origSetDescriptorHeaps =
            reinterpret_cast<detail::PFN_SetDescriptorHeaps>(pSetHeaps);
        detail::g_origReset =
            reinterpret_cast<detail::PFN_Reset>(pReset);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        LONG err = 0;
        err = DetourAttach(reinterpret_cast<PVOID*>(&detail::g_origSetPipelineState),
            detail::Hook_SetPipelineState);
        LOG_INFO(L"[StateTracker] DetourAttach SetPipelineState: " + std::to_wstring(err));

        err = DetourAttach(reinterpret_cast<PVOID*>(&detail::g_origSetComputeRootSignature),
            detail::Hook_SetComputeRootSignature);
        LOG_INFO(L"[StateTracker] DetourAttach SetComputeRootSig: " + std::to_wstring(err));

        err = DetourAttach(reinterpret_cast<PVOID*>(&detail::g_origSetDescriptorHeaps),
            detail::Hook_SetDescriptorHeaps);
        LOG_INFO(L"[StateTracker] DetourAttach SetDescriptorHeaps: " + std::to_wstring(err));

        err = DetourAttach(reinterpret_cast<PVOID*>(&detail::g_origReset),
            detail::Hook_Reset);
        LOG_INFO(L"[StateTracker] DetourAttach Reset: " + std::to_wstring(err));

        LONG commitErr = DetourTransactionCommit();
        LOG_INFO(L"[StateTracker] DetourTransactionCommit: " + std::to_wstring(commitErr));

        detail::g_hooksInstalled = (commitErr == NO_ERROR);
        return detail::g_hooksInstalled;
    }

    inline void RemoveHooks()
    {
        if (!detail::g_hooksInstalled) return;

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        if (detail::g_origSetPipelineState)
            DetourDetach(reinterpret_cast<PVOID*>(&detail::g_origSetPipelineState),
                detail::Hook_SetPipelineState);
        if (detail::g_origSetComputeRootSignature)
            DetourDetach(reinterpret_cast<PVOID*>(&detail::g_origSetComputeRootSignature),
                detail::Hook_SetComputeRootSignature);
        if (detail::g_origSetDescriptorHeaps)
            DetourDetach(reinterpret_cast<PVOID*>(&detail::g_origSetDescriptorHeaps),
                detail::Hook_SetDescriptorHeaps);
        if (detail::g_origReset)
            DetourDetach(reinterpret_cast<PVOID*>(&detail::g_origReset),
                detail::Hook_Reset);

        DetourTransactionCommit();
        detail::g_hooksInstalled = false;
    }

    inline ComputeStateSnapshot Capture(ID3D12GraphicsCommandList* cmdList)
    {
        ComputeStateSnapshot snap{};
        if (!cmdList) return snap;

        std::lock_guard<std::mutex> lock(detail::g_mutex);
        auto it = detail::g_states.find(cmdList);
        if (it == detail::g_states.end())
        {
            // Log that we couldn't find this cmdList - critical diagnostic
            LOG_WARNING(L"[StateTracker] Capture: cmdList " +
                std::to_wstring((uintptr_t)cmdList) +
                L" NOT FOUND in tracked states! Tracked lists: " +
                std::to_wstring(detail::g_states.size()) +
                L", total PSO calls: " + std::to_wstring(detail::g_totalSetPSOCalls) +
                L", total RootSig calls: " + std::to_wstring(detail::g_totalSetRootSigCalls) +
                L", total Heaps calls: " + std::to_wstring(detail::g_totalSetHeapsCalls));

            // Log all tracked cmdList pointers for comparison
            for (auto& [ptr, state] : detail::g_states)
            {
                LOG_WARNING(L"[StateTracker]   tracked cmdList: " +
                    std::to_wstring((uintptr_t)ptr) +
                    L" PSO=" + std::to_wstring((uintptr_t)state.lastPSO) +
                    L" calls=" + std::to_wstring(state.setPSOCount));
            }
            return snap;
        }

        const auto& s = it->second;
        snap.pipelineState = s.lastPSO;
        snap.computeRootSignature = s.lastComputeRootSig;
        snap.numDescriptorHeaps = s.lastNumHeaps;
        for (UINT i = 0; i < s.lastNumHeaps; i++)
            snap.descriptorHeaps[i] = s.lastHeaps[i];
        snap.valid = (snap.pipelineState != nullptr || snap.computeRootSignature != nullptr || snap.numDescriptorHeaps > 0);

        static uint64_t captureCount = 0;
        captureCount++;
        if (captureCount <= 5)
        {
            LOG_INFO(L"[StateTracker] Capture #" + std::to_wstring(captureCount) +
                L": valid=" + std::to_wstring(snap.valid) +
                L" PSO=" + std::to_wstring((uintptr_t)snap.pipelineState) +
                L" RootSig=" + std::to_wstring((uintptr_t)snap.computeRootSignature) +
                L" Heaps=" + std::to_wstring(snap.numDescriptorHeaps) +
                L" (cmdList " + std::to_wstring((uintptr_t)cmdList) + L")");
        }

        return snap;
    }

    inline void Restore(ID3D12GraphicsCommandList* cmdList, const ComputeStateSnapshot& snap)
    {
        if (!cmdList || !snap.valid) return;

        static uint64_t restoreCount = 0;
        restoreCount++;
        if (restoreCount <= 5)
        {
            LOG_INFO(L"[StateTracker] Restore #" + std::to_wstring(restoreCount) +
                L": PSO=" + std::to_wstring((uintptr_t)snap.pipelineState) +
                L" RootSig=" + std::to_wstring((uintptr_t)snap.computeRootSignature) +
                L" Heaps=" + std::to_wstring(snap.numDescriptorHeaps));
        }

        if (snap.numDescriptorHeaps > 0 && snap.descriptorHeaps[0])
            cmdList->SetDescriptorHeaps(snap.numDescriptorHeaps, snap.descriptorHeaps);

        if (snap.computeRootSignature)
            cmdList->SetComputeRootSignature(snap.computeRootSignature);

        if (snap.pipelineState)
            cmdList->SetPipelineState(snap.pipelineState);
    }

    inline void Untrack(ID3D12GraphicsCommandList* cmdList)
    {
        std::lock_guard<std::mutex> lock(detail::g_mutex);
        detail::g_states.erase(cmdList);
    }
}