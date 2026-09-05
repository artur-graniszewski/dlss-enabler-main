#include "NgxFrontend.h"
#include "NvngxCommon.h"          // NVSDK_NGX_SUCCEED, ctx, helpers
#include "../Core/Context.h" 
#include "NgxLogHelpers.h"
#include "DlssgProxy.h"
#include "Common.h"
#include "../Includes/dlss/nvsdk_ngx_params.h"
#include "../Includes/dlss/nvsdk_ngx.h"
#include "../Includes/dlss/nvsdk_ngx_defs.h"
#include "../Includes/NvParamImp.h"
#include "NgxFeatureEvents.h"
#include "DlssgRouter.h"
#include "RoutePolicy.h"
#include <d3d12.h>
#include <mutex>
#include <cstdio>
#include <atomic>
#include <algorithm>
#include <optional>

// Optional integration with FSR3 MFG per-pass GPU profiler.
// The header lives inside the FSR3 MFG static library (pulled in transitively
// via DLSSG Proxy). If the FSR3 build drops the profiler translation unit,
// this header is absent and the entire integration compiles out.
#include "ffx_gpu_profiler.h"
#if __has_include("ffx_gpu_profiler.h")

#include <deque>
#include <unordered_map> 
#include <string> 
#include <algorithm>
#include <vector>
#include <thread>
#include <condition_variable>
#include <atomic>
#endif

extern std::unique_ptr<DLSSG::DlssgProxy>  dlssgModule;
extern std::unique_ptr<DLSSG::DlssgRouter> dlssgRouter;

namespace NGX
{
	// =======================================================================
	// MFG DLSS PoC: GPU copy of DLSSG.HUDLess -> persistent texture aliased as
	// "DLSS.HUDLess" in NGX parameters. FFFrameInterpolator reads the copy via
	// LoadTextureFromNGXParameters(NGXParameters, "DLSS.HUDLess", ...).
	//
	// Why copy instead of pointer alias:
	//   1) NGX Set/Get for ID3D12Resource* may be stored in type-specific slot
	//      that FFFrameInterpolator's loader doesn't read (empirical: pointer
	//      alias resulted in resource=nullptr downstream despite immediate Get
	//      returning the set pointer).
	//   2) Even if alias worked, the source resource lifetime & state after
	//      DLSS upscaler evaluate may be unstable by the time FI consumes it.
	//      Our own persistent texture decouples us from game resource lifecycle.
	//
	// Design notes:
	//   - Persistent destination texture is lazily allocated/reallocated to
	//     match source dimensions & format. Lifetime managed by this module.
	//   - Allocated with ALLOW_SIMULTANEOUS_ACCESS so we can leave it in a
	//     read-friendly state across queue submissions without explicit tracking.
	//   - After copy, destination is left in COMMON state � with simultaneous
	//     access flag D3D12 allows implicit promotion to shader read states
	//     when the consumer (FI inpainting pass) binds it as SRV.
	//   - Source state: we don't transition the source. DLSSG.HUDLess is owned
	//     by the game and is typically in an implicit common state at DLSSG
	//     evaluate time. Common state allows implicit promotion to COPY_SOURCE.
	//   - Cleanup happens in ShutdownD3D12_1.
	// =======================================================================
	namespace
	{
		struct HudlessCopyState
		{
			ID3D12Resource* destTexture = nullptr;
			UINT64          width = 0;
			UINT            height = 0;
			DXGI_FORMAT     format = DXGI_FORMAT_UNKNOWN;
			UINT16          arraySize = 0;
			std::mutex      mtx;
		};
		static HudlessCopyState g_hudlessCopy;

		// Lazy allocate / reallocate destination texture to match source.
		// Returns true on success (destTexture usable), false on failure.
		// Must be called with g_hudlessCopy.mtx held.
		static bool EnsureHudlessCopyResource_Locked(ID3D12Device* device, const D3D12_RESOURCE_DESC& srcDesc)
		{
			const bool dimsMismatch =
				g_hudlessCopy.destTexture == nullptr ||
				g_hudlessCopy.width != srcDesc.Width ||
				g_hudlessCopy.height != srcDesc.Height ||
				g_hudlessCopy.format != srcDesc.Format ||
				g_hudlessCopy.arraySize != srcDesc.DepthOrArraySize;

			if (!dimsMismatch)
				return true;

			// Free old if any
			if (g_hudlessCopy.destTexture)
			{
				g_hudlessCopy.destTexture->Release();
				g_hudlessCopy.destTexture = nullptr;
			}

			D3D12_HEAP_PROPERTIES heapProps = {};
			heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC destDesc = srcDesc;
			// Strip write flags we don't need; keep only what's required for copy dest + SRV read.
			// ALLOW_SIMULTANEOUS_ACCESS lets us avoid explicit state transitions across queues/consumers.
			destDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
			// If the source had render-target or depth-stencil, we don't need those on the copy.
			// (Leaving them in would force non-simultaneous access mode.)
			// The copy only needs to be SRV-readable.

			HRESULT hr = device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&destDesc,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(&g_hudlessCopy.destTexture));

			if (FAILED(hr))
			{
				g_hudlessCopy.destTexture = nullptr;
				g_hudlessCopy.width = 0;
				g_hudlessCopy.height = 0;
				g_hudlessCopy.format = DXGI_FORMAT_UNKNOWN;
				g_hudlessCopy.arraySize = 0;
				return false;
			}

			g_hudlessCopy.destTexture->SetName(L"DLSS Enabler HUDLess Copy");
			g_hudlessCopy.width = srcDesc.Width;
			g_hudlessCopy.height = srcDesc.Height;
			g_hudlessCopy.format = srcDesc.Format;
			g_hudlessCopy.arraySize = srcDesc.DepthOrArraySize;
			return true;
		}

		// GPU copy DLSSG.HUDLess -> our persistent texture, then register as "DLSS.HUDLess".
		// Safe to call every frame; no-op if source is unavailable.
		static void CopyHudlessAndAliasInNGX(ID3D12GraphicsCommandList* cmdList, NVSDK_NGX_Parameter* parameters)
		{
			// DIAGNOSTIC: rate-limited logs every ~300 calls (~5s at 60fps).
			static uint32_t s_logCounter = 0;
			const bool      s_shouldLog = (s_logCounter++ % 300) == 0;

			if (s_shouldLog)
			{
				char msg[256];
				sprintf_s(msg, "[MFG HudlessCopy] #%u ENTRY cmdList=%p parameters=%p\n",
					s_logCounter, (void*)cmdList, (void*)parameters);
				OutputDebugStringA(msg);
			}

			if (!cmdList || !parameters)
			{
				if (s_shouldLog) OutputDebugStringA("[MFG HudlessCopy] EARLY-OUT: cmdList or parameters null\n");
				return;
			}

			ID3D12Resource* src = nullptr;
			parameters->Get("DLSSG.UI", &src);
			if (s_shouldLog)
			{
				char msg[256];
				sprintf_s(msg, "[MFG HudlessCopy] src from 'Depth' = %p\n", (void*)src);
				OutputDebugStringA(msg);
			}
			if (!src)
			{
				if (s_shouldLog) OutputDebugStringA("[MFG HudlessCopy] EARLY-OUT: Depth is null\n");
				return;
			}

			const D3D12_RESOURCE_DESC srcDesc = src->GetDesc();
			if (s_shouldLog)
			{
				char msg[256];
				sprintf_s(msg, "[MFG HudlessCopy] srcDesc: dim=%d width=%llu height=%u format=%d\n",
					(int)srcDesc.Dimension, (unsigned long long)srcDesc.Width, srcDesc.Height, (int)srcDesc.Format);
				OutputDebugStringA(msg);
			}
			if (srcDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || srcDesc.Width == 0 || srcDesc.Height == 0)
			{
				if (s_shouldLog) OutputDebugStringA("[MFG HudlessCopy] EARLY-OUT: src not a valid Texture2D\n");
				return;
			}

			ID3D12Device* device = nullptr;
			if (FAILED(src->GetDevice(IID_PPV_ARGS(&device))) || !device)
			{
				if (s_shouldLog) OutputDebugStringA("[MFG HudlessCopy] EARLY-OUT: GetDevice failed\n");
				return;
			}

			std::lock_guard<std::mutex> lock(g_hudlessCopy.mtx);

			if (!EnsureHudlessCopyResource_Locked(device, srcDesc))
			{
				if (s_shouldLog) OutputDebugStringA("[MFG HudlessCopy] EARLY-OUT: EnsureHudlessCopyResource failed\n");
				device->Release();
				return;
			}

			ID3D12Resource* dest = g_hudlessCopy.destTexture;
			if (s_shouldLog)
			{
				char msg[256];
				sprintf_s(msg, "[MFG HudlessCopy] dest=%p (persistent copy)\n", (void*)dest);
				OutputDebugStringA(msg);
			}

			// Destination was created with ALLOW_SIMULTANEOUS_ACCESS and left in COMMON state.
			// D3D12 allows implicit promotion COMMON -> COPY_DEST for simultaneous access textures,
			// so no explicit barrier needed on destination before CopyResource.
			//
			// Source belongs to the game; we don't control its state. We assume it's in COMMON
			// state (implicit) at DLSSG evaluate time, which allows implicit promotion to
			// COPY_SOURCE. If the game leaves it in a different non-promotable state, we'd need
			// explicit tracking � but for a PoC this is the pragmatic default.
			cmdList->CopyResource(dest, src);

			if (s_shouldLog) OutputDebugStringA("[MFG HudlessCopy] CopyResource scheduled on cmdList\n");

			// Register the copy under DLSS.HUDLess so FFFrameInterpolator picks it up via
			// LoadTextureFromNGXParameters(NGXParameters, "DLSS.HUDLess", ...).
			//
			// CRITICAL: FFFrameInterpolatorDX::LoadTextureFromNGXParameters reads via
			// NGXParameters->GetVoidPointer() � which reads from the void-pointer slot
			// in the NGX parameter map. Use Set(const char*, void*) overload explicitly
			// via static_cast<void*> to avoid ambiguity with typed overloads that would
			// write to a different storage slot.
			parameters->Set("DLSSG.OutputReal", static_cast<void*>(dest));

			// DIAGNOSTIC: immediately verify we can read back what we just wrote.
			if (s_shouldLog)
			{
				void* readback = nullptr;
				parameters->Get("DLSSG.OutputReal", &readback);
				char msg[256];
				sprintf_s(msg, "[MFG HudlessCopy] Set DLSSG.Depth=%p, GetVoidPointer readback=%p (match=%s)\n",
					(void*)dest, readback, (readback == (void*)dest ? "YES" : "NO"));
				OutputDebugStringA(msg);
			}

			device->Release();
		}

		// Release persistent copy. Called from ShutdownD3D12_1.
		static void ReleaseHudlessCopy()
		{
			std::lock_guard<std::mutex> lock(g_hudlessCopy.mtx);
			if (g_hudlessCopy.destTexture)
			{
				g_hudlessCopy.destTexture->Release();
				g_hudlessCopy.destTexture = nullptr;
			}
			g_hudlessCopy.width = 0;
			g_hudlessCopy.height = 0;
			g_hudlessCopy.format = DXGI_FORMAT_UNKNOWN;
			g_hudlessCopy.arraySize = 0;
		}

		// =======================================================================
		// MFG DEBUG: DLSSG.Backbuffer pinning across MFG subframes.
		//
		// Hypothesis: DLSSG/FSR3 may read DLSSG.Backbuffer fresh on every
		// subframe (1..N-1) and interpolate against whatever the swapchain
		// currently holds — which can shift mid-MFG-frame for runtimes that
		// present pseudo-frames between subframes. If that shift contributes
		// to MFG artifacts, pinning Backbuffer to a snapshot taken on
		// subframe 1 should remove (or change) the artifact pattern.
		//
		// Strategy (mirrors the HUDLess copy pattern used above):
		//   - Subframe 1: GPU CopyResource the game's current DLSSG.Backbuffer
		//     into our persistent destination texture. Don't override the
		//     parameter — let DLSSG see the real backbuffer this subframe.
		//   - Subframe >1: override parameters->Set("DLSSG.Backbuffer", dest)
		//     so DLSSG sees our pinned snapshot instead of whatever the game
		//     pushed for this subframe. RAII guard restores the original
		//     pointer after proxy() returns.
		//
		// Caveats:
		//   - We assume DLSSG.Backbuffer is a Texture2D resource readable by
		//     DLSSG as input. If runtimes treat it as RTV-only, the override
		//     will fail downstream — diagnose via existing RESULT logs.
		//   - Snapshot is rebuilt every MFG-frame (subframe 1 always copies),
		//     so it always reflects the most recent real backbuffer.
		//   - Lifetime owned by this module; released in ShutdownD3D12_1
		//     alongside the HUDLess copy.
		// =======================================================================
		// Double-buffered snapshot. Subframe 1 writes into the slot that
		// nobody is currently sampling from; the readIdx/writeIdx pair is
		// flipped after the CopyResource is recorded. Subframes >1 sample
		// from slots[readIdx], which is guaranteed not to be the target of
		// any pending or future CopyResource on the read side until the
		// next flip — preventing a later capture from clobbering the
		// snapshot DLSSG is still reading from on this MFG frame.
		//
		// Why two slots are enough for current MFG flow:
		//   - Eval is serialized; subframe N of frame K finishes (CPU+GPU)
		//     before subframe 1 of frame K+1 is recorded.
		//   - Single direct queue means CopyResource and the subsequent
		//     DLSSG sample on the same list/queue execute in order.
		//   - The only race we're defending against is "subframe 1 of the
		//     NEXT MFG frame overwrites the snapshot subframes >1 of the
		//     CURRENT frame are still using." With two slots, frame K+1
		//     writes to the slot frame K is NOT reading from. ✅
		//
		// If a future pipeline ever has 3+ overlapping in-flight samples
		// of the snapshot, this needs to grow into a ring buffer.
		struct BackbufferPinState
		{
			ID3D12Resource* slots[2] = { nullptr, nullptr };
			UINT64          width = 0;
			UINT            height = 0;
			DXGI_FORMAT     format = DXGI_FORMAT_UNKNOWN;
			UINT16          arraySize = 0;
			int             readIdx = 0;   // slot subframes >1 sample from
			int             writeIdx = 1;   // slot subframe 1 copies INTO
			std::mutex      mtx;
		};
		static BackbufferPinState g_bbPin;

		// Lazy alloc/realloc BOTH slots to match source dims. Must be called
		// with mtx held. On dims mismatch we recreate both slots; the readIdx
		// content is invalidated by definition (resolution/format change
		// implies a scene/swapchain reset where the old snapshot is stale).
		static bool EnsureBackbufferPinResource_Locked(ID3D12Device* device, const D3D12_RESOURCE_DESC& srcDesc)
		{
			const bool dimsMismatch =
				g_bbPin.slots[0] == nullptr ||
				g_bbPin.slots[1] == nullptr ||
				g_bbPin.width != srcDesc.Width ||
				g_bbPin.height != srcDesc.Height ||
				g_bbPin.format != srcDesc.Format ||
				g_bbPin.arraySize != srcDesc.DepthOrArraySize;

			if (!dimsMismatch)
				return true;

			// Tear down both slots before recreating; either both succeed
			// or both end up null (failure path leaves a clean state).
			for (int i = 0; i < 2; ++i)
			{
				if (g_bbPin.slots[i])
				{
					g_bbPin.slots[i]->Release();
					g_bbPin.slots[i] = nullptr;
				}
			}

			D3D12_HEAP_PROPERTIES heapProps = {};
			heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC destDesc = srcDesc;
			// Strip write flags we don't need; ALLOW_SIMULTANEOUS_ACCESS lets us
			// avoid explicit state transitions across queues/consumers.
			destDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

			for (int i = 0; i < 2; ++i)
			{
				HRESULT hr = device->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&destDesc,
					D3D12_RESOURCE_STATE_COMMON,
					nullptr,
					IID_PPV_ARGS(&g_bbPin.slots[i]));

				if (FAILED(hr))
				{
					// Roll back any partial allocation so we don't leave
					// one slot live and the other null — that asymmetry
					// would confuse the swap logic below.
					for (int j = 0; j <= i; ++j)
					{
						if (g_bbPin.slots[j])
						{
							g_bbPin.slots[j]->Release();
							g_bbPin.slots[j] = nullptr;
						}
					}
					g_bbPin.width = 0;
					g_bbPin.height = 0;
					g_bbPin.format = DXGI_FORMAT_UNKNOWN;
					g_bbPin.arraySize = 0;
					return false;
				}
			}

			g_bbPin.slots[0]->SetName(L"DLSS Enabler Backbuffer Pin [0]");
			g_bbPin.slots[1]->SetName(L"DLSS Enabler Backbuffer Pin [1]");
			g_bbPin.width = srcDesc.Width;
			g_bbPin.height = srcDesc.Height;
			g_bbPin.format = srcDesc.Format;
			g_bbPin.arraySize = srcDesc.DepthOrArraySize;
			// Reset indices to a known starting state. After the first
			// CopyResource + flip, readIdx will point to slot 1 (the one
			// we just wrote into), writeIdx to slot 0.
			g_bbPin.readIdx = 0;
			g_bbPin.writeIdx = 1;
			return true;
		}

		// Subframe 1: GPU-copy the game's current DLSSG.Backbuffer into the
		// WRITE slot, then flip read/write indices so subframes >1 sample
		// from the slot we just produced. Subframe 1 itself does NOT activate
		// the pin override — DLSSG must see the real backbuffer this subframe
		// (the snapshot we just captured is identical pixels by definition,
		// so override would be redundant; meanwhile, the freshly flipped
		// readIdx is what subframes 2..N will pick up via slots[readIdx]).
		//
		// Why no return value: the snapshot pointer must be re-fetched under
		// the mutex on each subframe>1 access, otherwise callers could cache
		// a stale slot pointer across a subsequent flip. Returning a slot
		// pointer here would invite exactly that bug.
		//
		// State assumptions match the HUDLess copy: src is assumed COMMON
		// (implicit promotion to COPY_SOURCE on simultaneous-access-less
		// resources is fragile if the game left it in RENDER_TARGET; if the
		// debug layer complains, add explicit barriers around CopyResource).
		static void CaptureBackbufferSnapshot(
			ID3D12GraphicsCommandList* cmdList, NVSDK_NGX_Parameter* parameters)
		{
			static uint32_t s_logCounter = 0;
			const bool      s_shouldLog = (s_logCounter++ % 300) == 0;

			if (!cmdList || !parameters)
			{
				if (s_shouldLog) OutputDebugStringA("[MFG BackbufferPin] EARLY-OUT: cmdList or parameters null\n");
				return;
			}

			ID3D12Resource* src = nullptr;
			parameters->Get("DLSSG.Backbuffer", &src);
			if (s_shouldLog)
			{
				char msg[256];
				sprintf_s(msg, "[MFG BackbufferPin] #%u src='DLSSG.Backbuffer' = %p\n",
					s_logCounter, (void*)src);
				OutputDebugStringA(msg);
			}
			if (!src)
				return;

			const D3D12_RESOURCE_DESC srcDesc = src->GetDesc();
			if (srcDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || srcDesc.Width == 0 || srcDesc.Height == 0)
			{
				if (s_shouldLog) OutputDebugStringA("[MFG BackbufferPin] EARLY-OUT: src not a valid Texture2D\n");
				return;
			}

			ID3D12Device* device = nullptr;
			if (FAILED(src->GetDevice(IID_PPV_ARGS(&device))) || !device)
			{
				if (s_shouldLog) OutputDebugStringA("[MFG BackbufferPin] EARLY-OUT: GetDevice failed\n");
				return;
			}

			std::lock_guard<std::mutex> lock(g_bbPin.mtx);

			if (!EnsureBackbufferPinResource_Locked(device, srcDesc))
			{
				if (s_shouldLog) OutputDebugStringA("[MFG BackbufferPin] EARLY-OUT: EnsureBackbufferPinResource failed\n");
				device->Release();
				return;
			}

			// Write into the slot subframes >1 are NOT currently sampling.
			ID3D12Resource* dest = g_bbPin.slots[g_bbPin.writeIdx];
			if (s_shouldLog)
			{
				char msg[256];
				sprintf_s(msg, "[MFG BackbufferPin] CopyResource src=%p -> dest=%p (slot %d, will become readIdx)\n",
					(void*)src, (void*)dest, g_bbPin.writeIdx);
				OutputDebugStringA(msg);
			}

			// Destination created with ALLOW_SIMULTANEOUS_ACCESS, left in COMMON.
			// D3D12 allows implicit promotion COMMON -> COPY_DEST for
			// simultaneous-access textures, so no explicit barrier on dest.
			cmdList->CopyResource(dest, src);

			// Flip indices: the slot we just copied INTO becomes the slot
			// subframes >1 sample FROM; the previous read slot becomes the
			// next write target. CPU-side flip is fine even though the
			// CopyResource is only RECORDED here (not yet executed on GPU)
			// because subframe>1 sample will be recorded LATER on the same
			// queue, and D3D12 guarantees in-order execution within a queue.
			std::swap(g_bbPin.readIdx, g_bbPin.writeIdx);

			device->Release();
		}

		// RAII override: temporarily swap DLSSG.Backbuffer to our pinned
		// snapshot. Restores the original pointer in dtor so downstream
		// consumers (router, profiler post-eval) see the unmodified
		// parameter set. Used on subframes 2..N-1 of an MFG frame.
		struct BackbufferPinScope
		{
			NVSDK_NGX_Parameter* parameters = nullptr;
			ID3D12Resource* original = nullptr;
			bool                 active = false;

			BackbufferPinScope(NVSDK_NGX_Parameter* p, ID3D12Resource* snapshot)
				: parameters(p)
			{
				if (!parameters || !snapshot)
					return;
				parameters->Get("DLSSG.Backbuffer", &original);
				parameters->Set("DLSSG.Backbuffer", snapshot);
				active = true;
			}
			~BackbufferPinScope()
			{
				if (!active || !parameters)
					return;
				// Restore — even if original was nullptr, that's the correct
				// post-state to leave behind.
				parameters->Set("DLSSG.Backbuffer", original);
			}
			BackbufferPinScope(const BackbufferPinScope&) = delete;
			BackbufferPinScope& operator=(const BackbufferPinScope&) = delete;
		};

		// Release pinned snapshot slots. Called from ShutdownD3D12_1.
		static void ReleaseBackbufferPin()
		{
			std::lock_guard<std::mutex> lock(g_bbPin.mtx);
			for (int i = 0; i < 2; ++i)
			{
				if (g_bbPin.slots[i])
				{
					g_bbPin.slots[i]->Release();
					g_bbPin.slots[i] = nullptr;
				}
			}
			g_bbPin.width = 0;
			g_bbPin.height = 0;
			g_bbPin.format = DXGI_FORMAT_UNKNOWN;
			g_bbPin.arraySize = 0;
			g_bbPin.readIdx = 0;
			g_bbPin.writeIdx = 1;
		}
	}
	// =======================================================================

	// =======================================================================
	// Per-pass GPU profiler integration (optional — linked transitively via
	// FSR3 MFG static library). All calls are compiled out when the profiler
	// header is not available. See ffx_gpu_profiler.h for the API.
	// =======================================================================
#if defined(FFX_GPU_PROFILER_AVAILABLE)
	namespace
	{
		// Lazy one-shot init. Safe to call every EvaluateD3D12 — becomes a
		// single atomic load after the first successful init.
		//
		// Device comes from the command list (all FSR3 work runs on one
		// device). Queue comes from the "DLSSG.CmdQueue" NGX parameter, which
		// Streamline populates for every MFG evaluate call.
		static void EnsureProfilerInitialized(ID3D12GraphicsCommandList* cmdList,
			NVSDK_NGX_Parameter* parameters)
		{
			if (FfxProf_IsEnabled()) return;
			if (!cmdList || !parameters) return;

			ID3D12Device* device = nullptr;
			if (FAILED(cmdList->GetDevice(IID_PPV_ARGS(&device))) || !device) return;

			ID3D12CommandQueue* queue = nullptr;
			parameters->Get("DLSSG.CmdQueue", reinterpret_cast<void**>(&queue));
			if (!queue) {
				device->Release();
				return;
			}

			FfxProf_Init(device, queue);

			// FfxProf_Init AddRefs both objects via ComPtr internally.
			device->Release();
			// Do NOT Release queue — NGX returned a borrowed (non-AddRef'd) pointer.
		}

		// Drain completed reports into a rolling-window aggregator. Raport
		// (avg + median + p99, per subframe slot, per pass) is emitted every
		// time a subframe slot's window fills 100 full game-frame cycles.
		//
		// Window semantics:
		//   - Each (subframeIdx, passLabel) keeps a deque of the last 100
		//     duration samples. Old samples fall off the front.
		//   - Each subframeIdx also keeps a deque of the last 100 per-frame
		//     totals (sum of all passes in that subframe).
		//   - One emit per subframe slot per 100-frame tick — that is, when
		//     its frame counter crosses a multiple of 100.
		//
		// Mode-change reset:
		//   - activeSubframeCount tracks the last-seen MFG count (3 for x4,
		//     5 for x6, etc). On change we wipe the aggregator and start
		//     gathering fresh samples for the new mode.
		//   - Also wipes on first-ever use (activeSubframeCount == -1).
		//
		// Latency: reports reflect GPU work 2-3 subframes old (profiler-side
		// lazy resolve) plus up to 100 frames of averaging. Expect the first
		// report ~1.5 s after gameplay starts; subsequent reports every ~1.5 s.

		namespace MfgAgg
		{
			static constexpr size_t kWindowSize = 100;   // frames per rolling window

			struct PassStats {
				std::deque<float> samples;

				// Presence over the same window: one entry per frame in the bucket,
				// 1 if this pass ran in that frame, 0 if it did not.
				//
				// Needed because `samples` alone cannot tell a pass that RAN and was
				// cheap from a pass that STOPPED being dispatched. A deque only pops
				// when something is pushed, so a label that goes away keeps reporting
				// its last samples forever, and the map keeps printing its row. With
				// conditionally dispatched passes that is the difference between a
				// measurement and a fossil.
				std::deque<uint8_t> presence;

				// Frame id of the most recent Push. Compared against the bucket's
				// current frame id to decide what to mark — the report only lists
				// passes that ran, so absence has to be inferred by elimination.
				uint64_t lastPushFrame = 0;

				void Push(float ms) {
					samples.push_back(ms);
					if (samples.size() > kWindowSize) samples.pop_front();
				}

				void MarkFrame(bool ran) {
					presence.push_back(ran ? uint8_t(1) : uint8_t(0));
					if (presence.size() > kWindowSize) presence.pop_front();
				}

				// Frames in the current window in which this pass actually ran.
				size_t Hits() const {
					size_t n = 0;
					for (uint8_t v : presence) n += v;
					return n;
				}
				size_t WindowLen() const { return presence.size(); }

				// Compute avg/median/p99 from current samples. Returns false if
				// sample set is empty. Uses a sorted copy — O(N log N) but only
				// called at emit time (once per 100 frames), so cost is trivial.
				bool Summarize(float& outAvg, float& outMedian, float& outP99) const {
					const size_t n = samples.size();
					if (n == 0) return false;

					std::vector<float> sorted(samples.begin(), samples.end());
					std::sort(sorted.begin(), sorted.end());

					double sum = 0.0;
					for (float v : sorted) sum += v;
					outAvg = float(sum / double(n));

					// Median: middle element for odd n, average of two middles for even.
					if (n & 1u) {
						outMedian = sorted[n / 2];
					}
					else {
						outMedian = 0.5f * (sorted[n / 2 - 1] + sorted[n / 2]);
					}

					// p99: index floor(0.99 * (n-1)), clamped to [0, n-1].
					// For small n (<100) this collapses to max or near-max.
					size_t idx = size_t(0.99 * double(n - 1) + 0.5);
					if (idx >= n) idx = n - 1;
					outP99 = sorted[idx];

					return true;
				}
			};

			// Pass category for grouped reporting. Defined inside MfgAgg so
			// PushFrame can classify as samples arrive (needed for per-group
			// per-frame totals — median/p99 of a sum isn't the sum of medians,
			// so we have to form the sum each frame before stashing it).
			enum class PassGroup { OF, FI };

			// Classify a pass label. OF pipeline labels start with "Opticalflow_"
			// (named passes from ffx_opticalflow.cpp: Luma, SCD_Histogram,
			// SCD_Divergence) or with "OF " (dynamically-named per-level passes
			// like "OF Search Level 3", "OF Filter Level 2"). Everything else
			// belongs to frame interpolation — including FI's internal
			// OPTICAL_FLOW_VECTOR_FIELD pass, which is ALL_CAPS and therefore
			// does not match either OF prefix.
			static PassGroup ClassifyPass(const wchar_t* label) {
				if (!label || !label[0]) return PassGroup::FI;
				if (label[0] == L'O' && label[1] == L'F' && label[2] == L' ') {
					return PassGroup::OF;
				}
				static const wchar_t kOfPrefix[] = L"Opticalflow_";
				for (size_t i = 0; kOfPrefix[i] != L'\0'; ++i) {
					if (label[i] != kOfPrefix[i]) return PassGroup::FI;
				}
				return PassGroup::OF;
			}

			struct SubframeBucket {
				std::unordered_map<std::wstring, PassStats> passes;
				// Per-frame totals — stashed as a sum per frame, so Summarize
				// on these deques gives accurate median/p99 (median of sums,
				// not sum of medians).
				std::deque<float> frameTotals;   // grand total (all passes)
				std::deque<float> ofTotals;      // sum of OF passes only
				std::deque<float> fiTotals;      // sum of FI passes only
				// Passes actually dispatched per frame, straight from the report.
				// Distinct from passes.size(), which counts labels EVER seen in this
				// window and therefore never shrinks.
				std::deque<float> passCounts;
				uint64_t framesSeen = 0;

				void PushFrame(const FfxProfFrameReport& r) {
					float ofSum = 0.0f;
					float fiSum = 0.0f;

					const uint64_t frameId = framesSeen + 1;   // 0 means "never pushed"

					for (uint32_t i = 0; i < r.passCount; ++i) {
						const float ms = r.passes[i].durationMs;
						// Cache a wstring key once per unique label.
						PassStats& st = passes[std::wstring(r.passes[i].label)];
						st.Push(ms);
						st.lastPushFrame = frameId;

						if (ClassifyPass(r.passes[i].label) == PassGroup::OF) {
							ofSum += ms;
						}
						else {
							fiSum += ms;
						}
					}

					// Mark presence for EVERY known label, not just the ones in this
					// report — that is the whole point. The map is ~30 entries and this
					// runs once per frame.
					for (auto& kv : passes) {
						kv.second.MarkFrame(kv.second.lastPushFrame == frameId);
					}

					auto pushBounded = [](std::deque<float>& d, float v) {
						d.push_back(v);
						if (d.size() > kWindowSize) d.pop_front();
						};

					pushBounded(frameTotals, r.totalMs);
					pushBounded(ofTotals, ofSum);
					pushBounded(fiTotals, fiSum);
					pushBounded(passCounts, float(r.passCount));
					++framesSeen;
				}
			};

			struct Aggregator {
				int32_t activeSubframeCount = -1;   // -1 = uninitialized
				std::unordered_map<int32_t, SubframeBucket> bySubframe;

				void Reset(int32_t newCount) {
					bySubframe.clear();
					activeSubframeCount = newCount;
				}
			};

			static Aggregator g_agg;
		} // namespace MfgAgg

		// =================================================================
		// Worker-thread offload for report emission
		// =================================================================
		// Formatting + logging ~30 LOG_DEBUG lines synchronously on the hot
		// path (every 100 subframes) can spike frame time. Instead, the hot
		// path snapshots the bucket into an EmitJob and queues it for a
		// worker thread, which does the sort + swprintf_s + LOG_DEBUG work
		// outside the frame.
		//
		// Lifetime:
		//   - Worker is spawned lazily on the first enqueue.
		//   - Stopped explicitly from ShutdownD3D12_1 (before FfxProf_Shutdown).
		//   - Joined cleanly — the queue mutex is released before joining,
		//     and the worker exits its loop when g_workerStop is true.
		//
		// Queue policy:
		//   - Bounded (16 jobs). At 1 job per ~100 frames per subframe slot,
		//     and ~1 emit per 1.7s for x4 MFG, this is gigantic headroom.
		//   - On overflow we drop the OLDEST job (rare; only happens if the
		//     logger backend stalls). Drop counter logged.
		//
		// Logger thread safety:
		//   - LOG_DEBUG is assumed thread-safe. If you ever change the logger
		//     to a non-reentrant backend, wrap LOG_DEBUG calls in a mutex.

		struct EmitJob {
			int32_t subframeIdx;
			int32_t subframeCount;
			MfgAgg::SubframeBucket snapshot;   // deep copy of the bucket at emit time
		};

		struct EmitterWorker {
			std::mutex                mtx;
			std::condition_variable   cv;
			std::deque<EmitJob>       queue;
			std::atomic<bool>         running{ false };
			std::atomic<bool>         stop{ false };
			std::atomic<uint32_t>     droppedJobs{ 0 };
			std::thread               thread;

			static constexpr size_t   kMaxQueued = 16;

			void EnsureStarted();
			void Enqueue(EmitJob&& job);
			void Stop();
			void Loop();
		};

		static EmitterWorker g_emitter;

		// Forward decl — defined below (uses MfgAgg internals).
		static void EmitSubframeReport(int32_t subframeIdx, int32_t subframeCount,
			const MfgAgg::SubframeBucket& bucket);

		void EmitterWorker::EnsureStarted() {
			bool expected = false;
			if (!running.compare_exchange_strong(expected, true)) return;
			stop.store(false);
			thread = std::thread(&EmitterWorker::Loop, this);
		}

		void EmitterWorker::Enqueue(EmitJob&& job) {
			EnsureStarted();
			{
				std::lock_guard<std::mutex> lock(mtx);
				if (queue.size() >= kMaxQueued) {
					queue.pop_front();   // drop oldest
					droppedJobs.fetch_add(1, std::memory_order_relaxed);
				}
				queue.push_back(std::move(job));
			}
			cv.notify_one();
		}

		void EmitterWorker::Stop() {
			if (!running.load()) return;
			stop.store(true);
			cv.notify_all();
			if (thread.joinable()) {
				thread.join();
			}
			running.store(false);
		}

		void EmitterWorker::Loop() {
			for (;;) {
				EmitJob job;
				{
					std::unique_lock<std::mutex> lock(mtx);
					cv.wait(lock, [this] { return stop.load() || !queue.empty(); });
					if (stop.load() && queue.empty()) return;
					job = std::move(queue.front());
					queue.pop_front();
				}

				// Heavy lifting — sorting, formatting, LOG_DEBUG calls — happens
				// here, off the render thread.
				EmitSubframeReport(job.subframeIdx, job.subframeCount, job.snapshot);

				// Periodically warn if we've been dropping jobs (rare).
				const uint32_t dropped = droppedJobs.exchange(0, std::memory_order_relaxed);
				if (dropped > 0) {
					wchar_t warn[128];
					swprintf_s(warn, L"[MFG-PROF] emitter dropped %u report(s) due to backlog", dropped);
					LOG_DEBUG(std::wstring(warn));
				}
			}
		}

		// Public Stop hook for ShutdownD3D12_1.
		static void StopProfilerEmitter() {
			g_emitter.Stop();
		}

		// Wipe aggregator state and drop any pending emit jobs. Called on the
		// rising edge of the enable flag (false -> true) so a fresh profiling
		// session starts with empty windows rather than stale data from the
		// previous on-period. Cheap — just clears two small containers.
		static void ResetProfilerAggregator() {
			MfgAgg::g_agg.Reset(-1);   // -1 forces re-init on next sample arrival
			std::lock_guard<std::mutex> lock(g_emitter.mtx);
			g_emitter.queue.clear();
		}

		// Format and emit the rolling-window report for a single subframe slot.
		// Called when that slot hits a multiple of kWindowSize frames.
		//
		// Output layout (one subframe, grouped OF/FI):
		//   [MFG-PROF 100f] subframe 1/3  TOTAL: avg=X.XXX ...  (29 labels, 29.0 dispatched/frame)
		//     OF: avg=X.XXX med=X.XXX p99=X.XXX  (N passes)
		//         Opticalflow_Luma       avg=0.082  med=0.081  p99=0.104  ran=100/100
		//         OF Search Level 3      avg=0.421  med=0.418  p99=0.491  ran=100/100
		//         ...
		//     FI: avg=X.XXX med=X.XXX p99=X.XXX  (M passes)
		//         INTERPOLATION          avg=2.143  med=2.131  p99=2.294  ran=100/100
		//         HUD_EXTRACT            avg=0.093  med=0.092  p99=0.108  ran=  0/100
		//         ...
		//
		// Reading the two counts in the header: LABELS is how many distinct pass
		// names this bucket has ever seen and never shrinks; DISPATCHED/FRAME is
		// what the last frames actually carried. They agree only while every pass
		// is unconditional. Per row, ran=h/w says how many of those frames the
		// pass ran in — ran=0/100 marks a row whose timings are fossils from
		// before the pass stopped being dispatched.
		static void EmitSubframeReport(int32_t subframeIdx, int32_t subframeCount,
			const MfgAgg::SubframeBucket& bucket)
		{
			// Helper: compute avg/med/p99 from a deque by wrapping it in PassStats.
			auto summarizeDeque = [](const std::deque<float>& samples,
				float& a, float& m, float& p)
				{
					MfgAgg::PassStats tmp;
					for (float v : samples) tmp.samples.push_back(v);
					return tmp.Summarize(a, m, p);
				};

			// Grand total for the header.
			float totAvg = 0.0f, totMed = 0.0f, totP99 = 0.0f;
			summarizeDeque(bucket.frameTotals, totAvg, totMed, totP99);

			// Dispatched passes per frame, median over the window. Reported next to
			// the label count because the two answer different questions: the label
			// count is how many names this bucket has ever seen, the dispatch count
			// is how many jobs the last frames actually carried. They diverge as
			// soon as any pass is dispatched conditionally, and only the second one
			// tells you whether a pass is still running.
			float dispAvg = 0.0f, dispMed = 0.0f, dispP99 = 0.0f;
			summarizeDeque(bucket.passCounts, dispAvg, dispMed, dispP99);

			wchar_t header[256];
			swprintf_s(header,
				L"[MFG-PROF %zuf] subframe %d/%d  TOTAL: avg=%.3fms  med=%.3fms  p99=%.3fms  (%zu labels, %.1f dispatched/frame)",
				MfgAgg::kWindowSize,
				int(subframeIdx),           // 1-based from Streamline; emit verbatim
				int(subframeCount),
				double(totAvg), double(totMed), double(totP99),
				bucket.passes.size(),
				double(dispAvg));
			LOG_DEBUG(std::wstring(header));

			// Collect per-pass rows per group. Sort each group by avg descending
			// so the most expensive passes float to the top — easier to scan.
			struct Row {
				const std::wstring* label;
				float avg, med, p99;
				size_t hits, window;   // frames this pass ran / frames in the window
			};
			std::vector<Row> ofRows, fiRows;
			ofRows.reserve(bucket.passes.size());
			fiRows.reserve(bucket.passes.size());

			for (const auto& kv : bucket.passes) {
				float a = 0.0f, m = 0.0f, p = 0.0f;
				if (!kv.second.Summarize(a, m, p)) continue;
				Row row{ &kv.first, a, m, p, kv.second.Hits(), kv.second.WindowLen() };
				if (MfgAgg::ClassifyPass(kv.first.c_str()) == MfgAgg::PassGroup::OF) {
					ofRows.push_back(row);
				}
				else {
					fiRows.push_back(row);
				}
			}

			auto byAvgDesc = [](const Row& lhs, const Row& rhs) { return lhs.avg > rhs.avg; };
			std::sort(ofRows.begin(), ofRows.end(), byAvgDesc);
			std::sort(fiRows.begin(), fiRows.end(), byAvgDesc);

			// Emit one group (header with group-total stats, then per-pass lines).
			// Skips empty groups — e.g. subframe 2+ has no OF work.
			auto emitGroup = [&](const wchar_t* name,
				const std::deque<float>& groupTotals,
				const std::vector<Row>& rows)
				{
					if (rows.empty()) return;

					float ga = 0.0f, gm = 0.0f, gp = 0.0f;
					summarizeDeque(groupTotals, ga, gm, gp);

					wchar_t groupLine[192];
					swprintf_s(groupLine,
						L"  %ls: avg=%.3fms  med=%.3fms  p99=%.3fms  (%zu passes)",
						name,
						double(ga), double(gm), double(gp),
						rows.size());
					LOG_DEBUG(std::wstring(groupLine));

					for (const Row& r : rows) {
						// ran=h/w is the honesty column: h < w means the timings above
						// average fewer frames than the window suggests, and ran=0/w
						// means the pass is no longer dispatched at all and the numbers
						// are leftovers from when it was.
						wchar_t line[288];
						swprintf_s(line,
							L"      %-36ls  avg=%7.3f  med=%7.3f  p99=%7.3f  ran=%3zu/%zu",
							r.label->c_str(),
							double(r.avg), double(r.med), double(r.p99),
							r.hits, r.window);
						LOG_DEBUG(std::wstring(line));
					}
				};

			emitGroup(L"OF", bucket.ofTotals, ofRows);
			emitGroup(L"FI", bucket.fiTotals, fiRows);
		}

		// Drain raw profiler reports, feed them into the aggregator, and emit
		// rolling-window summaries as windows fill up.
		static void DrainAndLogProfilerReports()
		{
			if (!FfxProf_IsEnabled()) return;

			FfxProfFrameReport report;
			while (FfxProf_PopCompletedFrame(&report))
			{
				// Detect mode change — any change in subframeCount wipes the
				// aggregator. Samples from different MFG modes aren't comparable
				// (different workload distribution), so mixing them would give
				// meaningless averages.
				if (MfgAgg::g_agg.activeSubframeCount != report.subframeCount) {
					MfgAgg::g_agg.Reset(report.subframeCount);
				}

				// Push this frame's timings into the right per-subframe bucket.
				MfgAgg::SubframeBucket& bucket =
					MfgAgg::g_agg.bySubframe[report.subframeIdx];
				bucket.PushFrame(report);

				// Emit a report every kWindowSize frames (per subframe slot).
				// Use framesSeen rather than samples.size() so we keep emitting
				// once the window has filled (size stays at kWindowSize forever).
				//
				// To avoid frame-pacing hitches we don't format/log on this
				// thread — instead we snapshot the bucket and hand the job to
				// the emitter worker. The snapshot is a deep copy of one
				// SubframeBucket (~12 KB for typical FSR3 MFG workload), which
				// happens once per ~1.7 s — negligible cost.
				if (bucket.framesSeen > 0 &&
					(bucket.framesSeen % MfgAgg::kWindowSize) == 0)
				{
					EmitJob job;
					job.subframeIdx = report.subframeIdx;
					job.subframeCount = report.subframeCount;
					job.snapshot = bucket;   // deep copy
					g_emitter.Enqueue(std::move(job));
				}
			}
		}
	} // anonymous namespace
#endif // FFX_GPU_PROFILER_AVAILABLE

	static bool IsDlssgFeature(NVSDK_NGX_Feature featureId) {
		// NVSDK_NGX_Feature_FrameGeneration = 11
		return featureId == NVSDK_NGX_Feature_FrameGeneration;
	}

	static bool IsDlssgHandle(const std::unordered_map<const NVSDK_NGX_Handle*, NVSDK_NGX_Feature>& registry, const NVSDK_NGX_Handle* handle) {
		auto it = registry.find(handle);
		if (it != registry.end()) {
			return IsDlssgFeature(it->second);
		}
		return false;
	}

	// =======================================================================
	// MfgParamOverride — temporarily lie to native DLSSG about the MFG slot
	// =======================================================================
	//
	// Native DLSSG was designed for x2 mode (one interpolated frame at t=0.5
	// between two real frames). When Streamline drives x4/x6 MFG it sets
	// DLSSG.MultiFrameIndex/MultiFrameCount to values like (1, 3) or (2, 5)
	// to tell DLSSG which subframe slot it should produce. The native DLSSG
	// implementation only supports the (0, 1) "x2 midpoint" pair on most
	// hardware paths — passing it (1, 3) makes it either reject the call or
	// produce a frame at the wrong t.
	//
	// In hybrid mode the router only sends a subframe to native DLSSG when
	// it sits exactly at t=0.5 anyway (see MidpointDlssgPolicy). So just
	// before the native call we can rewrite the parameter block to claim
	// (idx=0, count=1), let native DLSSG produce its x2 frame, and restore
	// the original values immediately afterwards. The restore is critical
	// because the SAME parameter block is also seen by:
	//   - downstream code in EvaluateD3D12 after this scope ends
	//   - the FSR3 fallback path if native fails (it must see the real
	//     subframe coordinates, not the lie we told native)
	//   - the next subframe's Evaluate call if the parameter block is
	//     reused (Streamline often re-publishes but we don't want to depend
	//     on it)
	//
	// RAII guarantees the restore happens even if the wrapped call throws
	// or the surrounding scope returns early.
	//
	// Set HYBRID_MFG_LIE_TO_NATIVE_DLSSG to 0 to disable the override
	// completely without removing any code — useful when bisecting whether
	// a glitch comes from the lie or from elsewhere.
	//
	// HISTORY: this was set to 1 based on a wrong hypothesis ("native DLSSG
	// can only handle x2 mode and needs to be lied to about MFG slot"). The
	// first build of the hybrid router did NOT have this override and the
	// tester reported native DLSSG actually working for x4. After the override
	// was added, native started returning FAIL_FeatureNotFound (0xBAD00005)
	// on the very first call. Conclusion: native DLSSG handles real
	// (idx, count) values just fine; rewriting them to (0, 1) corrupts the
	// state it expects. Set back to 0.
#define HYBRID_MFG_LIE_TO_NATIVE_DLSSG 1

	class MfgParamOverride
	{
	public:
		MfgParamOverride(NVSDK_NGX_Parameter* params)
			: parameters(params)
		{
#if HYBRID_MFG_LIE_TO_NATIVE_DLSSG
			if (!parameters) return;
			// Snapshot whatever the game/Streamline put in the parameter block.
			// NVSDK_NGX_Parameter::Get returns failure for missing keys; on
			// failure we leave the saved value at its default (1, 1) which is
			// also what we'll write back, so the restore is a no-op for
			// callers that never set these in the first place.
			parameters->Get("DLSSG.MultiFrameIndex", &savedIndex);
			parameters->Get("DLSSG.MultiFrameCount", &savedCount);

			// Rewrite to "I'm subframe 0 of 1, the only frame in an x2 pair".
			// Per DlssgProxy.cpp lines 467-468 these are read as int, so set
			// them as int. NVSDK_NGX_Parameter::Set has int overload.
			parameters->Set("DLSSG.MultiFrameIndex", 1);
			parameters->Set("DLSSG.MultiFrameCount", 1);
			active = true;
#endif
		}

		~MfgParamOverride()
		{
#if HYBRID_MFG_LIE_TO_NATIVE_DLSSG
			if (!active || !parameters) return;
			parameters->Set("DLSSG.MultiFrameIndex", savedIndex);
			parameters->Set("DLSSG.MultiFrameCount", savedCount);
#endif
		}

		MfgParamOverride(const MfgParamOverride&) = delete;
		MfgParamOverride& operator=(const MfgParamOverride&) = delete;

	private:
		NVSDK_NGX_Parameter* parameters = nullptr;
		int savedIndex = 0;
		int savedCount = 1;
		bool active = false;
	};
	// =======================================================================

	NVSDK_NGX_Result NGX_DeepDvcCallback()
	{
		return NVSDK_NGX_Result_Success;
	}

#define NVSDK_NGX_Parameter_DeepDVC_GetStatsCallback        "DeepDVC.GetStatsCallback"
	using ::VkInstance;
	using ::VkDevice;
	using ::VkPhysicalDevice;

	// ===== Logging helpers =====
	void NgxFrontend::LogInfo(const wchar_t* entry, const std::wstring& message) { logger.Info(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxFrontend::LogWarning(const wchar_t* entry, const std::wstring& message) { logger.Warning(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxFrontend::LogError(const wchar_t* entry, const std::wstring& message) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
	void NgxFrontend::LogNoBackend(const wchar_t* entry) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": backend entrypoint not found"); }

	// ===== D3D12: CreateFeature (fully implemented) =====

	void NgxFrontend::OnEvaluateFeature(const NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters)
	{
		static int isThrottlingFg = 0;
		static std::deque<float> fpsHistory;

		if (fpsHistory.size() >= 10) {
			fpsHistory.pop_back();
		}

		/*
		int isDfgEnabled = 0;
		InParameters->Get("DFG.Enabled", &isDfgEnabled);

		if (ctx.ngx.isDynamicFrameGenerationEnabled != (bool)isDfgEnabled) {
			int orgDynamicFrameGenerationEnabled = 0;
			InParameters->Get("DLSSEnabler.InternalDFG.Enabled", &orgDynamicFrameGenerationEnabled);

			if (orgDynamicFrameGenerationEnabled == isDfgEnabled) {
				// its a duplicated NGX param, synchronize it with the source of truth...
				InParameters->Set("DLSSEnabler.InternalDFG.Enabled", (int)ctx.ngx.isDynamicFrameGenerationEnabled);
				InParameters->Set("DFG.Enabled", (int)ctx.ngx.isDynamicFrameGenerationEnabled);
			}
			else {
				LOG_INFO(std::wstring(L"Dynamic Frame Generation will be ") + (isDfgEnabled == 0 ? L"disabled" : L"enabled"));
				//ctx.ngx.isDynamicFrameGenerationEnabled = (bool)isDfgEnabled;
				if (!ctx.deactivateDFG) {
					ctx.ngx.isDynamicFrameGenerationEnabled = isDfgEnabled > 0;
				}
				else {
					tmpEnableDFG = (bool)isDfgEnabled;
				}
			}
		}
		*/
		int frameRate = 0;
		InParameters->Get("FramerateLimit", &frameRate);

		if (frameRate != ctx.reflex.desiredFpsLimit) {
			int orgFrameLimit = 0;
			InParameters->Get("DLSSEnabler.InternalFramerateLimit", &orgFrameLimit);

			if (orgFrameLimit == frameRate) {
				// its a duplicated NGX param, synchronize it with the source of truth...
				InParameters->Set("DLSSEnabler.InternalFramerateLimit", (int)ctx.reflex.desiredFpsLimit);
				InParameters->Set("FramerateLimit", (int)ctx.reflex.desiredFpsLimit);
			}
			else {
				//LogInfo(L"OnEvaluateFeature", L"Adjusting FPS limit to: " + std::to_wstring(frameRate));
				//ctx.reflex.desiredFpsLimit = frameRate;
				//ctx.reflex.realFpsLimit = (double)frameRate;
			}
		}
		/*
		NGX_ReportUpscalerStats(InParameters);

		float ngxDelta = 0.0f;

		InParameters->Get(NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, &ngxDelta);
		if (ngxDelta == 0.0f) {
			InParameters->Set(NVSDK_NGX_Parameter_FrameTimeDeltaInMsec, (float)ctx.reflex.timeFrameDeltaMsec);
		}

		if (frameGenerationHandles.find(InFeatureHandle) != frameGenerationHandles.end()) {
			if (!ctx.ngx.isFrameGenerationEnabled || ctx.ngx.isNextFrameSkippable) {
				InParameters->Set("DLSSG.Reset", 1);
			}

			ctx.ngx.lastEvaluationTimeMsec = Common::GetCurrentTimeMsec();

			if (ctx.reflex.desiredFpsLimit && ctx.ngx.isDynamicFrameGenerationEnabled) {
				static int threshold = 2;
				// if the frame has been duplicated previously, we need to adjust GPU potential
				if (isThrottlingFg > threshold) {
					isThrottlingFg = threshold;
				}
				else if (isThrottlingFg < -threshold) {
					isThrottlingFg = -threshold;
				}

				float potentialReflexFPStmp = (float)(isThrottlingFg < 0 ? ctx.reflex.potentialFps / 2 : ctx.reflex.potentialFps);
				fpsHistory.push_front(potentialReflexFPStmp);

				int frames = 0;
				float sum = 0;
				for (float value : fpsHistory) {
					frames++;
					sum += value;
				}

				potentialReflexFPStmp = sum / frames;


				potentialReflexFPStmp = GetPotentialFPS();

				if (potentialReflexFPStmp > ctx.reflex.desiredFpsLimit * 0.9f) {
					//if (potentialReflexFPS >= ctx.reflex.desiredFpsLimit - 2) {
					isThrottlingFg--;
				}
				else {
					isThrottlingFg++;
					if (isThrottlingFg == 0) {
						//
					}
				}

				if (isThrottlingFg <= -threshold) {
					//potentialReflexFPS /= 2;
					ctx.reflex.realFpsLimit = ctx.reflex.desiredFpsLimit;
					if (ctx.logging.isUltraDebugEnabled) {
						Console::ShowStatus(L">> Potential FPS: " + std::to_wstring(potentialReflexFPStmp)
							+ L"(" + std::to_wstring(potentialReflexFPStmp) + L") "
							+ L"/" + std::to_wstring(ctx.reflex.desiredFpsLimit)
							+ L", frametime is : " + std::to_wstring(ctx.reflex.timeFrameDeltaMsec)
							+ L": disabling FG      ");
					}
					InParameters->Set("DLSSG.Reset", 1);
					ctx.ngx.isDuplicatingFrames = true;
				}
				else {
					ctx.reflex.realFpsLimit = ctx.reflex.desiredFpsLimit * 1.5;
					if (ctx.logging.isUltraDebugEnabled) {
						Console::ShowStatus(L">> Potential FPS: " + std::to_wstring(potentialReflexFPStmp)
							+ L"(" + std::to_wstring(potentialReflexFPStmp) + L") "
							+ L"/" + std::to_wstring(ctx.reflex.desiredFpsLimit)
							+ L", frametime is : " + std::to_wstring(ctx.reflex.timeFrameDeltaMsec)
							+ L"                       ");
					}
					ctx.ngx.isDuplicatingFrames = false;
				}
			}
		}
		*/
	}


	static void GetNGXFeatureRequirements(NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		*RequirementInfo = NVSDK_NGX_FeatureRequirement();

		if (FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature::NVSDK_NGX_Feature_RayReconstruction) {
			RequirementInfo->FeatureSupported = (ctx.nvapi.isProxyLoaded || ctx.nvapi.isMockEnabled) ? NVSDK_NGX_FeatureSupportResult_NotImplemented : NVSDK_NGX_FeatureSupportResult_Supported;
		}
		else {
			RequirementInfo->FeatureSupported = NVSDK_NGX_FeatureSupportResult_Supported;
		}

		if (FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature::NVSDK_NGX_Feature_SuperSampling) {
			if (ctx.ngx.overrideDlssUpscalerCapability && !ctx.ngx.enableDlssUpscaler) {
				RequirementInfo->FeatureSupported = NVSDK_NGX_FeatureSupportResult_AdapterUnsupported;
				ctx.ngx.isDlssSupportedByHardware = false;
			}
			else {
				ctx.ngx.isDlssSupportedByHardware = true;
			}
		}

		RequirementInfo->MinHWArchitecture = 10;
		strcpy_s(RequirementInfo->MinOSVersion, "10.0.0.0");
	}

	static void InitNGX(const wchar_t* InApplicationDataPath, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo** InFeatureInfo)
	{
		if (InFeatureInfo == nullptr || *InFeatureInfo == nullptr) {
			*InFeatureInfo = new NVSDK_NGX_FeatureCommonInfo();
			(*InFeatureInfo)->PathListInfo.Length = 0;
			(*InFeatureInfo)->PathListInfo.Path = nullptr;
			LOG_WARNING(L"[NVNGX] Application did not provide Feature Common Info");
		}

		LOG_INFO(L"[NVNGX]    SDK version: " + std::to_wstring(InSDKVersion));
		auto LoggingInfo = (*InFeatureInfo)->LoggingInfo;
		LOG_INFO(L"[NVNGX]    Logging level: " + std::to_wstring(LoggingInfo.MinimumLoggingLevel));
		(*InFeatureInfo)->LoggingInfo.LoggingCallback = NGX_Logger;
		(*InFeatureInfo)->LoggingInfo.DisableOtherLoggingSinks = false;
		(*InFeatureInfo)->LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
		(*InFeatureInfo)->LoggingInfo.DisableOtherLoggingSinks = true;

		// Allocate new memory for the path list
		unsigned int newLength = (*InFeatureInfo)->PathListInfo.Length + 1;
		wchar_t const** newPathArray = new wchar_t const* [newLength];
		std::wstring appDirPath = Common::GetProcessFilePath().parent_path().wstring() + L"\\";
		bool isAppDirPathPresent = false;
		// Copy existing paths
		for (unsigned int i = 0; i < (*InFeatureInfo)->PathListInfo.Length; ++i) {
			newPathArray[i] = (*InFeatureInfo)->PathListInfo.Path[i];
			auto path = std::wstring(newPathArray[i]);
			LOG_INFO(L"[NVNGX]    Path included: " + path);
			if (appDirPath == path) {
				isAppDirPathPresent = true;
			}
		}

		// Add the new path
		if (!isAppDirPathPresent) {
			LOG_INFO(L"[NVNGX] Adding " + appDirPath + L" to the Path List Info structure");
			wchar_t* newPath = new wchar_t[appDirPath.length() + 1];
			wcscpy_s(newPath, appDirPath.length() + 1, appDirPath.c_str());
			newPathArray[(*InFeatureInfo)->PathListInfo.Length] = newPath;

			// Update PathListInfo
			(*InFeatureInfo)->PathListInfo.Path = newPathArray;
			(*InFeatureInfo)->PathListInfo.Length = newLength;
		}

		// Try loading the nvngx files
		for (unsigned int i = 0; i < (*InFeatureInfo)->PathListInfo.Length; ++i) {
			auto dir = std::wstring((*InFeatureInfo)->PathListInfo.Path[i]) + L"\\";
			if (GetModuleHandleW(L"nvngx_dlss.dll") == nullptr && GetFileAttributesW((dir + L"nvngx_dlss.dll").c_str()) != INVALID_FILE_ATTRIBUTES) {
				LOG_INFO(L"[NVNGX] Loading DLSS module");
				LoadLibraryW((dir + L"nvngx_dlss.dll").c_str());
			}
			if (GetModuleHandleW(L"nvngx_dlssg.dll") == nullptr && GetFileAttributesW((dir + L"nvngx_dlss.dll").c_str()) != INVALID_FILE_ATTRIBUTES) {
				LOG_INFO(L"[NVNGX] Loading DLSSG module");
				LoadLibraryW((dir + L"nvngx_dlssg.dll").c_str());
			}
		}

	}

	NVSDK_NGX_Result NgxFrontend::InitD3D11Ext(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		ID3D11Device* device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long applicationId,
			const wchar_t*,
			ID3D11Device*,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(applicationId, applicationDataPath, device, sdkVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D11()
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE();
		result = proxy();
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D12()
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE();
		result = proxy();
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D11_1(ID3D11Device* device)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(ID3D11Device*);
		result = proxy(device);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownVulkan()
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE();
		result = proxy();
		NGX_LOG_RESULT_AND_RETURN;
	}

	// @todo, check why its not a pointer....
	NVSDK_NGX_Result NgxFrontend::ShutdownVulkan_1(VkDevice InDevice)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown1");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(VkDevice);
		result = proxy(InDevice);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::ShutdownD3D12_1(ID3D12Device* device)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown");

		// MFG DLSS PoC: release persistent HUDLess copy before the device goes away.
		ReleaseHudlessCopy();

		// MFG DEBUG: release pinned Backbuffer snapshot alongside HUDLess copy.
		ReleaseBackbufferPin();

#if defined(FFX_GPU_PROFILER_AVAILABLE)
		// Stop the emitter worker thread first — it has no D3D12 dependency
		// but we want it joined cleanly before shutdown progresses.
		StopProfilerEmitter();

		// Tear down the per-pass GPU profiler before the device is released.
		// Safe to call even if FfxProf_Init was never invoked.
		FfxProf_Shutdown();
#endif

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(ID3D12Device*);
		result = proxy(device);
		NGX_LOG_RESULT_AND_RETURN;
	}


	NVSDK_NGX_Result NgxFrontend::InitD3D12Ext(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		ID3D12Device* device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long applicationId,
			const wchar_t*,
			ID3D12Device*,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(applicationId, applicationDataPath, device, sdkVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D12ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
		const wchar_t* InApplicationDataPath, ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Init_ProjectID");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(const char*, NVSDK_NGX_EngineType, const char*,
			const wchar_t*, ID3D12Device*, NVSDK_NGX_Version, NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D11ProjectId(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion,
		const wchar_t* InApplicationDataPath, ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Init_ProjectID");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(const char*, NVSDK_NGX_EngineType, const char*,
			const wchar_t*, ID3D11Device*, NVSDK_NGX_Version, NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(InProjectId, InEngineType, InEngineVersion, InApplicationDataPath, InDevice, InSDKVersion, InFeatureInfo);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D11(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D11Device* InDevice,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_Init");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

		NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, ID3D11Device*, NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);
		result = proxy(InApplicationId, InApplicationDataPath, InDevice, InFeatureInfo, InSDKVersion);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitD3D12(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, ID3D12Device* InDevice,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_Init");

		NGX_LOG_CALL;
		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);

		NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, ID3D12Device*, NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);
		result = proxy(InApplicationId, InApplicationDataPath, InDevice, InFeatureInfo, InSDKVersion);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetScratchBufferSizeD3D12(
		NVSDK_NGX_Feature featureId,
		const NVSDK_NGX_Parameter* parameters,
		size_t* outSizeInBytes)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_GetScratchBufferSize");

		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);
		result = proxy(featureId, parameters, outSizeInBytes);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetScratchBufferSizeVulkan(
		NVSDK_NGX_Feature featureId,
		const NVSDK_NGX_Parameter* parameters,
		size_t* outSizeInBytes)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetScratchBufferSize");

		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);
		result = proxy(featureId, parameters, outSizeInBytes);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetScratchBufferSizeD3D11(
		NVSDK_NGX_Feature featureId,
		const NVSDK_NGX_Parameter* parameters,
		size_t* outSizeInBytes)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetScratchBufferSize");

		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);
		result = proxy(featureId, parameters, outSizeInBytes);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureRequirementsD3D11(IDXGIAdapter* Adapter,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetFeatureRequirements");
		NGX_LOG_CALL;

		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

		NGX_RESOLVE_PROXY_ONCE(IDXGIAdapter*, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);
		result = proxy(Adapter, FeatureDiscoveryInfo, RequirementInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::CreateD3D11(
		ID3D11DeviceContext* cmdList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_CreateFeature");

		NGX_LOG_CALL;

		// Dispatch PRE-CREATE event
		NgxFeatureEvents::DispatchPreCreateD3D11(cmdList, featureId, parameters);

		NGX_RESOLVE_PROXY_ONCE(ID3D11DeviceContext*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
		result = proxy(cmdList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			if (featureId == NVSDK_NGX_Feature_SuperSampling || featureId == NVSDK_NGX_Feature_RayReconstruction) {
				ctx.ngx.isUpscalingActive = true;
				parameters->Get("PerfQualityValue", &ctx.ngx.upscalingQuality);
			}
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			// Dispatch POST-CREATE event
			NgxFeatureEvents::DispatchPostCreateD3D11(cmdList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::EvaluateD3D11(
		ID3D11DeviceContext* cmdList,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isNgxEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		// Get feature type for event dispatching
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		if (featureId == NVSDK_NGX_Feature_SuperSampling || featureId == NVSDK_NGX_Feature_RayReconstruction) {
			ctx.ngx.isUpscalingActive = true;
		}

		// Dispatch PRE-EVALUATE event
		NgxFeatureEvents::DispatchPreEvaluateD3D11(cmdList, featureHandle, parameters, featureId);

		OnEvaluateFeature(featureHandle, parameters);
		NGX_RESOLVE_PROXY_ONCE(ID3D11DeviceContext*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
		result = proxy(cmdList, featureHandle, parameters, callback);

		// Dispatch POST-EVALUATE event
		NgxFeatureEvents::DispatchPostEvaluateD3D11(cmdList, featureHandle, parameters, featureId, result);

		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"NGX failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Result NgxFrontend::ReleaseD3D11(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_ReleaseFeature");
		NGX_LOG_CALL;

		// Get feature type before release
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		// Dispatch PRE-RELEASE event
		NgxFeatureEvents::DispatchPreReleaseD3D11(featureHandle, featureId);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = false;
		}

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
		result = proxy(featureHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			handleRegistry.erase(featureHandle);
		}

		// Dispatch POST-RELEASE event
		NgxFeatureEvents::DispatchPostReleaseD3D11(featureHandle, featureId, result);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureRequirementsD3D12(IDXGIAdapter* Adapter,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_GetFeatureRequirements");
		NGX_LOG_CALL;


		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

		if (FeatureDiscoveryInfo->FeatureID == NVSDK_NGX_Feature::NVSDK_NGX_Feature_FrameGeneration) {
			result = NVSDK_NGX_Result_Success;
			bool isDlssgDetected = true;
			if (!GetModuleHandleW(L"nvngx_dlssg.dll")) {
				LogWarning(kEntry, L"NGX did not detect NVNGX_DLSSG.DLL file");
				isDlssgDetected = false;
			}
			else {
				LogInfo(kEntry, L"NGX detected NVNGX_DLSSG.DLL file");
			}
		}
		else
		{
			NGX_RESOLVE_PROXY_ONCE(IDXGIAdapter*, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);
			result = proxy(Adapter, FeatureDiscoveryInfo, RequirementInfo);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureRequirementsVulkan(
		const VkInstance instance,
		const VkPhysicalDevice device,
		NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetFeatureRequirements");
		NGX_LOG_CALL;

		GetNGXFeatureRequirements(FeatureDiscoveryInfo, RequirementInfo);

		NGX_RESOLVE_PROXY_ONCE(const VkInstance, const VkPhysicalDevice, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);
		result = proxy(instance, device, FeatureDiscoveryInfo, RequirementInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}


	NVSDK_NGX_Result NgxFrontend::CreateD3D12(
		ID3D12GraphicsCommandList* cmdList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_CreateFeature");

		NGX_LOG_CALL;

		// @todo: move me to the event handler
		if (ctx.ngx.dlaaId != NVSDK_NGX_PerfQuality_Value_DLAA) {
			// PerfQualityValue
			uint64_t perfValue = 0;
			parameters->Get("PerfQualityValue", &perfValue);
			if (perfValue == ctx.ngx.dlaaId) {
				parameters->Set("PerfQualityValue", NVSDK_NGX_PerfQuality_Value_DLAA);
			}
		}

		// Dispatch PRE-CREATE event (listeners can modify parameters)
		NgxFeatureEvents::DispatchPreCreateD3D12(cmdList, featureId, parameters);

		// === DLSSG REDIRECT ===
		if (IsDlssgFeature(featureId)) {
			ctx.ngx.isFrameGenerationActive = true;
		}

		if (IsDlssgFeature(featureId) && dlssgRouter && dlssgRouter->IsActive() && dlssgModule) {
			// Hybrid fanout: create on BOTH backends, hand the native handle back
			// to the game, store the FSR3 shadow handle in the router map.
			//
			// Per design: no parameter cloning. Each subframe re-publishes fresh
			// inputs from Streamline so the two backends never fight over scratch
			// state at this layer.
			LogInfo(kEntry, L"[DLSSG-HYBRID] CreateFeature -> native + FSR3 fanout");

			// Step 1: native DLSSG (this is the handle the game will hold).
			//
			// IMPORTANT: wrap in MfgParamOverride so native sees (idx=0, count=1)
			// at create-time. Without this, Streamline may publish a higher
			// MultiFrameCount (e.g. count=5 for x6) before our hybrid Evaluate
			// gets a chance to lie, and native nvngx_dlssg allocates its
			// internal pipeline under that mode. Then on first Evaluate it
			// returns FAIL_FeatureNotFound (0xBAD00005) because the (lied) x2
			// state we ask it to drive doesn't match the (truthful) x6 state
			// it allocated under. Pinning the create-time view to x2 makes
			// native allocate the x2 pipeline once and keeps it consistent
			// with what every Evaluate will tell it later.
			NVSDK_NGX_Handle* nativeHandle = nullptr;
			NVSDK_NGX_Result nativeRes;
			{
				MfgParamOverride mfgScope(parameters);
				NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
				nativeRes = proxy(cmdList, featureId, parameters, &nativeHandle);
			}
			const bool nativeOk = NVSDK_NGX_SUCCEED(nativeRes) && nativeHandle != nullptr;

			// Step 2: FSR3 shadow. Parameters are restored to their real values
			// (whatever Streamline wrote) before this call, so FSR3 sees the
			// truthful MFG mode and allocates its own pipeline accordingly.
			NVSDK_NGX_Handle* fsr3Handle = nullptr;
			NVSDK_NGX_Result fsr3Res = dlssgModule->CreateD3D12(cmdList, featureId, parameters, &fsr3Handle);
			const bool fsr3Ok = NVSDK_NGX_SUCCEED(fsr3Res) && fsr3Handle != nullptr;

			if (!nativeOk && !fsr3Ok) {
				LogError(kEntry, L"[DLSSG-HYBRID] both backends failed to create FrameGeneration");
				result = nativeOk ? fsr3Res : nativeRes;
				// Leave *outHandle untouched.
			}
			else {
				if (!nativeOk) {
					LogWarning(kEntry, L"[DLSSG-HYBRID] native create failed (raw=" + std::to_wstring((int)nativeRes) + L"), FSR3-only for this handle");
				}
				if (!fsr3Ok) {
					LogWarning(kEntry, L"[DLSSG-HYBRID] FSR3 shadow create failed (raw=" + std::to_wstring((int)fsr3Res) + L"), native passthrough for this handle");
				}
				// The handle returned to the game is whichever one we have, but
				// native is preferred since the policy will route most subframes there.
				*outHandle = nativeOk ? nativeHandle : fsr3Handle;
				dlssgRouter->RememberPair(nativeHandle, fsr3Handle, nativeOk, fsr3Ok);
				result = NVSDK_NGX_Result_Success;
			}
		}
		else if (IsDlssgFeature(featureId) && dlssgModule) {
			LogInfo(kEntry, L"[DLSSG-REDIRECT] CreateFeature -> dlssgModule");
			result = dlssgModule->CreateD3D12(cmdList, featureId, parameters, outHandle);
		}
		else {
			NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
			result = proxy(cmdList, featureId, parameters, outHandle);
		}
		// === END DLSSG REDIRECT ===

		if (NVSDK_NGX_SUCCEED(result)) {
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			if (featureId == NVSDK_NGX_Feature_SuperSampling || featureId == NVSDK_NGX_Feature_RayReconstruction) {
				ctx.ngx.isUpscalingActive = true;
				parameters->Get("PerfQualityValue", &ctx.ngx.upscalingQuality);
			}

			// Dispatch POST-CREATE event (for initialization like SSRTGI)
			NgxFeatureEvents::DispatchPostCreateD3D12(cmdList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::EvaluateD3D12(
		ID3D12GraphicsCommandList* cmdList,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isNgxEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		// Get feature type for event dispatching
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		if (featureId == NVSDK_NGX_Feature_SuperSampling || featureId == NVSDK_NGX_Feature_RayReconstruction) {
			ctx.ngx.isUpscalingActive = true;
			parameters->Set("Sharpness", 0.0f);

			// @todo: move me to the event handler
			if (ctx.ngx.dlaaId != NVSDK_NGX_PerfQuality_Value_DLAA) {
				// PerfQualityValue
				uint64_t perfValue = 0;
				parameters->Get("PerfQualityValue", &perfValue);
				if (perfValue == ctx.ngx.dlaaId) {
					parameters->Set("PerfQualityValue", NVSDK_NGX_PerfQuality_Value_DLAA);
				}
			}
		}

		// Dispatch PRE-EVALUATE event (for effects like SSRTGI)
		uint32_t flags = ctx.flags;

		parameters->Set("DLSSG.DispatchFlags", flags);

		// ---- Per-pass GPU profiler: open a frame slot ----
		// Must happen BEFORE the dispatch to FSR3/DLSSG so the subsequent
		// BeginPass/EndPass calls from the FSR3 backend land in this slot.
		// We read subframe info here again (also read below for routing) —
		// duplicate is cheap and keeps profiler integration self-contained.
#if defined(FFX_GPU_PROFILER_AVAILABLE)
		// Snapshot the enable flag once per evaluate. Using the same value
		// for BeginFrame/EndFrame/Drain in this evaluate guarantees we don't
		// strand a slot in OPEN state if the user toggles the flag mid-call.
		const bool profEnabled = ctx.ngx.isDlssgProfilerEnabled;

		// Detect rising edge (false -> true) and wipe aggregator state so
		// the new on-period starts with empty rolling windows. Falling edge
		// is no-op — pending in-flight GPU queries get resolved naturally
		// the next time profiling is enabled (their slots transition to
		// READY and get popped at that point; aggregator reset above
		// discards anything that isn't part of the current session).
		static bool s_profEnabledPrev = false;
		if (profEnabled && !s_profEnabledPrev) {
			ResetProfilerAggregator();
		}
		s_profEnabledPrev = profEnabled;

		int profSubframeIdx = 0;
		int profSubframeCount = 1;
		if (profEnabled && IsDlssgFeature(featureId)) {
			parameters->Get("DLSSG.MultiFrameIndex", &profSubframeIdx);
			parameters->Get("DLSSG.MultiFrameCount", &profSubframeCount);

			EnsureProfilerInitialized(cmdList, parameters);
			FfxProf_BeginFrame(cmdList, profSubframeIdx, profSubframeCount);
		}
#endif

		NgxFeatureEvents::DispatchPreEvaluateD3D12(cmdList, featureHandle, parameters, featureId);

		OnEvaluateFeature(featureHandle, parameters);

		if (IsDlssgFeature(featureId)) {
			{
				ctx.ngx.isGeneratingFrames = true;
			}
			int frameIndex = 1;
			int frameMax = 1;
			parameters->Get("DLSSG.MultiFrameIndex", &frameIndex);
			parameters->Get("DLSSG.MultiFrameCount", &frameMax);

			if (!ctx.ngx.isGeneratingFrames) {
				parameters->Set("DLSSG.Reset", 1);
				ctx.ngx.isDuplicatingFrames = true;
				ctx.ngx.framesGenerated = 0;
				ctx.ngx.maxFramesGenerated = max(frameMax, 1);
			}
			else {
				ctx.ngx.framesGenerated = max(frameMax, 1);
				ctx.ngx.isDuplicatingFrames = false;
				ctx.ngx.maxFramesGenerated = max(frameMax, 1);
			}
			ctx.ngx.isFrameGenerationActive = true;

#define MFG_DEBUG_FLAG_FRAME_INDEX_LINE        0x00010000  // (1 << 16)
#define MFG_DEBUG_FLAG_HUD_DETECTION           0x00020000  // (1 << 17)
#define MFG_DEBUG_FLAG_DISOCCLUSION_TINT       0x00040000  // (1 << 18)
#define MFG_DEBUG_FLAG_ARTIFACTS_DETECTION     0x00080000  // (1 << 19)
#define MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE       0x00100000  // (1 << 20) - Enable anti-ghosting correction
#define MFG_DEBUG_FLAG_ANTIGHOSTING_RED_TINT     0x00200000  // (1 << 21) - Debug: red tint on corrected pixels
#define MFG_DEBUG_FLAG_ANTIGHOSTING_SPLIT_SCREEN 0x00400000  // (1 << 22) - Debug: split screen comparison
#define MFG_DEBUG_FLAG_CAMERA_MV_DEBUG           0x00800000  // (1 << 23) - Debug: blue tint where camera MV fallback is used
#define MFG_DEBUG_FLAG_TRAPEZOID_VIS             0x01000000  // (1 << 24) - Debug: trapezoid zone visualization
#define MFG_DEBUG_FLAG_HUDLESS_UI_MASK           0x02000000  // (1 << 25) - Use HUD-less as UI mask (DL2 inverted semantics)
#define MFG_DEBUG_FLAG_TEMPORAL_HUD_PIN          0x04000000  // (1 << 26) - Enable temporal HUD pinning (present-backbuffer stability)
#define MFG_DEBUG_FLAG_HUD_INTERPOLATION         0x08000000  // (1 << 27) - HUD OF interpolation (0=legacy pin-present, 1=OF warp)
#define MFG_DEBUG_FLAG_IGNORE_UI_TEXTURE         0x10000000  // (1 << 28) - Ignore dedicated DLSSG.UI texture (force legacy HUD path)
#define MFG_DEBUG_FLAG_DP4A_ACTIVE               0x20000000  // (1 << 29) - OF pipeline using dp4a-accelerated SSD (SM 6.4+)
#define MFG_DEBUG_FLAG_PIN_BACKBUFFER            0x40000000  // (1 << 30) - Pin DLSSG.Backbuffer to subframe-1 snapshot across MFG frame

			const uint32_t MFG_DEBUG_SHOWDEBUG_IGNORED_MASK =
				MFG_DEBUG_FLAG_FRAME_INDEX_LINE |
				MFG_DEBUG_FLAG_ANTIGHOSTING_ENABLE |
				MFG_DEBUG_FLAG_TEMPORAL_HUD_PIN |
				MFG_DEBUG_FLAG_HUDLESS_UI_MASK |
				MFG_DEBUG_FLAG_HUD_INTERPOLATION |
				MFG_DEBUG_FLAG_IGNORE_UI_TEXTURE |
				MFG_DEBUG_FLAG_DP4A_ACTIVE;

			//MFG_DEBUG_FLAG_PIN_BACKBUFFER;

			if ((flags & ~MFG_DEBUG_SHOWDEBUG_IGNORED_MASK) != 0)
			{
				parameters->Set("DLSSG.ShowDebug", (int)(frameIndex == frameMax));
			}
			else {
				parameters->Set("DLSSG.ShowDebug", (int)0);
			}
		}

		// === DLSSG.HUDLess -> DLSS.HUDLess GPU COPY ===
		// PoC: we cannot pointer-alias because NGX Set/Get for ID3D12Resource* stores
		// the pointer in a type-specific slot that FFFrameInterpolator's loader does
		// not read from (empirically: alias resulted in nullptr downstream).
		//
		// Instead we GPU-copy the HUDLess buffer into our own persistent texture and
		// register THAT under "DLSS.HUDLess". The copy is owned by this module and
		// survives across the game's own resource lifecycle.
		//
		// Later, the source for this copy will be swapped from DLSSG.HUDLess to the
		// actual DLSS upscaler output captured in a separate DLSS feature hook.
		if (IsDlssgFeature(featureId)) {
			//CopyHudlessAndAliasInNGX(cmdList, parameters);
		}
		// === END DLSSG.HUDLess -> DLSS.HUDLess GPU COPY ===

		// === MFG DEBUG: DLSSG.Backbuffer pin across subframes ===
		// Subframe 1 (first interpolation in MFG frame): snapshot the game's
		// current Backbuffer into the WRITE slot, then flip indices so the
		// freshly-written slot becomes the read target. No parameter override
		// on subframe 1 — DLSSG sees the real BB (snapshot is identical).
		// Subframe >1: read slots[readIdx] under lock and BackbufferPinScope
		// swaps it into "DLSSG.Backbuffer" for proxy(), restoring the
		// original pointer when the scope ends.
		//
		// Double-buffering rationale: subframe 1 of the NEXT MFG frame writes
		// to whichever slot is NOT currently being sampled, so it cannot
		// clobber the snapshot subframes >1 of the current frame are still
		// reading. Without this, a single-buffer pin would race the next
		// capture against in-flight samples.
		//
		// Gated by MFG_DEBUG_FLAG_PIN_BACKBUFFER (1 << 30). When the flag is
		// off this entire section is no-op (no copy emitted, no parameter
		// mutation). The scope is declared at function scope so its dtor
		// runs after all DLSSG REDIRECT branches and the profiler post-eval
		// phase — guaranteeing the parameter block is restored before any
		// downstream consumer (router, profiler) reads it.
		std::optional<BackbufferPinScope> bbPinScope;
		const bool pinBackbufferEnabled = IsDlssgFeature(featureId)
			&& (flags & MFG_DEBUG_FLAG_PIN_BACKBUFFER) != 0;

		if (false && pinBackbufferEnabled) {
			int subframeIdxForPin = 1;
			int subframeCountForPin = 1;
			parameters->Get("DLSSG.MultiFrameIndex", &subframeIdxForPin);
			parameters->Get("DLSSG.MultiFrameCount", &subframeCountForPin);

			if (subframeIdxForPin <= 1) {
				// First subframe of this MFG frame — refresh the snapshot
				// (writes to writeIdx, then flips). No parameter override
				// here; DLSSG must see the real BB this subframe so the
				// snapshot we just captured is the same pixels DLSSG
				// actually rendered against.
				CaptureBackbufferSnapshot(cmdList, parameters);
			}
			else {
				// Subsequent subframe — read from the slot that subframe 1
				// flipped TO readIdx. Lock pairs with the swap inside
				// CaptureBackbufferSnapshot and with EnsureBackbufferPinResource_Locked.
				ID3D12Resource* pinnedSnapshot = nullptr;
				{
					std::lock_guard<std::mutex> lock(g_bbPin.mtx);
					pinnedSnapshot = g_bbPin.slots[g_bbPin.readIdx];
				}

				// Activate RAII override only when we have a snapshot. On
				// subframe 1 the snapshot IS the real backbuffer, so
				// override would be redundant — handled by the if/else above.
				if (pinnedSnapshot) {
					bbPinScope.emplace(parameters, pinnedSnapshot);
				}
			}
		}
		// === END MFG DEBUG: DLSSG.Backbuffer pin ===



		// === DLSSG REDIRECT ===
		if (IsDlssgFeature(featureId) && dlssgRouter && dlssgRouter->IsActive() && dlssgModule) {
			// Hybrid per-subframe routing.
			//
			// CRITICAL: NVSDK_NGX_Parameter::Get is type-strict. DlssgProxy.cpp
			// (lines 467-468) reads MultiFrameIndex/Count as `int`, so that's
			// the storage type Streamline writes. Reading the same key into
			// `uint32_t*` matches the wrong overload and either fails outright
			// (leaving the local at its default) or returns garbage. The
			// previous build used uint32_t and that's why every Decide() saw
			// (0, 1) regardless of which MFG mode Streamline was actually in.
			int subframeIdx_i = 0;
			int subframeCount_i = 1;
			NVSDK_NGX_Result idxRes = NVSDK_NGX_Result_FAIL_UnsupportedParameter;
			NVSDK_NGX_Result cntRes = NVSDK_NGX_Result_FAIL_UnsupportedParameter;
			if (parameters) {
				idxRes = parameters->Get("DLSSG.MultiFrameIndex", &subframeIdx_i);
				cntRes = parameters->Get("DLSSG.MultiFrameCount", &subframeCount_i);
			}
			// Clamp to non-negative before widening; Decide() takes uint32_t.
			const uint32_t subframeIdx = (subframeIdx_i < 0) ? 0u : (uint32_t)subframeIdx_i;
			const uint32_t subframeCount = (subframeCount_i < 0) ? 0u : (uint32_t)subframeCount_i;

			const auto entry = dlssgRouter->LookupEntry(featureHandle);
			if (entry.nativeHandle == nullptr && entry.fsr3Handle == nullptr) {
				// Unknown handle — must have been created before the router came
				// online, or by some path that bypassed us. Pass through to native.
				LogWarning(kEntry, L"[DLSSG-HYBRID] unknown handle in Evaluate, passing through to native");
				NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
				result = proxy(cmdList, featureHandle, parameters, callback);
			}
			else {
				const DLSSG::RouteTarget target = dlssgRouter->Decide(
					subframeIdx,
					subframeCount,
					ctx.ngx.isDlssgSupportedByHardware,
					/*fsr3ShadowExists*/ entry.fsr3Ok,
					/*nativeHandleExists*/ entry.nativeOk);

				// === DIAGNOSTIC: log on every change of (idx, count, target) ===
				// Logs once per unique combination so the volume stays sane but
				// we still see every routing decision Streamline asks for.
				// Also fires on first call ever. Set HYBRID_DIAG_LOG to 0 to disable.
#define HYBRID_DIAG_LOG 1
#if HYBRID_DIAG_LOG
				{
					static std::atomic<uint64_t> s_lastSig{ ~uint64_t(0) };
					const uint64_t sig = (uint64_t(uint32_t(subframeIdx_i)) << 40)
						| (uint64_t(uint32_t(subframeCount_i)) << 16)
						| (uint64_t(idxRes != NVSDK_NGX_Result_Success) << 8)
						| (uint64_t(cntRes != NVSDK_NGX_Result_Success) << 4)
						| uint64_t(target == DLSSG::RouteTarget::NativeDlssg ? 1 : 0);
					if (s_lastSig.exchange(sig) != sig) {
						const wchar_t* tname = (target == DLSSG::RouteTarget::NativeDlssg) ? L"NATIVE" : L"FSR3";
						//						LogInfo(kEntry,
						//							L"[DLSSG-HYBRID-DIAG] rawIdx=" + std::to_wstring(subframeIdx_i)
						//							+ L" rawCount=" + std::to_wstring(subframeCount_i)
						//							+ L" idxGetRes=" + std::to_wstring((int)idxRes)
						//							+ L" cntGetRes=" + std::to_wstring((int)cntRes)
						//							+ L" target=" + std::wstring(tname)
						//							+ L" nativeOk=" + std::to_wstring(entry.nativeOk ? 1 : 0)
						//							+ L" fsr3Ok=" + std::to_wstring(entry.fsr3Ok ? 1 : 0)
						//							+ L" healthy=" + std::to_wstring(dlssgRouter->IsDlssgHealthy() ? 1 : 0));

												// Probe alternative MFG-related keys to find what Streamline
												// actually publishes. Each call left commented so we don't
												// touch unknown keys, but the read attempts are harmless.
						int probeI = -999;
						unsigned int probeUI = 0xDEADBEEF;
						unsigned long long probeULL = 0xDEADBEEFCAFEBABEull;
						float probeF = -1.0f;

						auto probeKey = [&](const char* key) {
							probeI = -999;
							probeUI = 0xDEADBEEF;
							probeULL = 0xDEADBEEFCAFEBABEull;
							probeF = -1.0f;
							NVSDK_NGX_Result rI = parameters->Get(key, &probeI);
							NVSDK_NGX_Result rUI = parameters->Get(key, &probeUI);
							NVSDK_NGX_Result rULL = parameters->Get(key, &probeULL);
							NVSDK_NGX_Result rF = parameters->Get(key, &probeF);
							const bool anyOk = NVSDK_NGX_SUCCEED(rI) || NVSDK_NGX_SUCCEED(rUI)
								|| NVSDK_NGX_SUCCEED(rULL) || NVSDK_NGX_SUCCEED(rF);
							if (anyOk) {
								std::wstring k;
								for (const char* p = key; *p; ++p) k.push_back((wchar_t)*p);
								//LogInfo(kEntry,
								//	L"[DLSSG-HYBRID-PROBE] key='" + k + L"'"
								//	+ L" int=" + std::to_wstring(probeI) + L"(r" + std::to_wstring((int)rI) + L")"
								//	+ L" uint=" + std::to_wstring(probeUI) + L"(r" + std::to_wstring((int)rUI) + L")"
								//	+ L" u64=" + std::to_wstring(probeULL) + L"(r" + std::to_wstring((int)rULL) + L")"
								//	+ L" f=" + std::to_wstring(probeF) + L"(r" + std::to_wstring((int)rF) + L")");
							}
							};

						probeKey("DLSSG.MultiFrameIndex");
						probeKey("DLSSG.MultiFrameCount");
						probeKey("DLSSG.MultiFrameCountMax");
						probeKey("DLSSG.NumFramesToGenerate");
						probeKey("DLSSG.FrameIndex");
						probeKey("DLSSG.FrameCount");
						probeKey("DLSSG.NumGeneratedFrames");
						probeKey("DLSSG.SubFrameIndex");
						probeKey("DLSSG.SubFrameCount");
					}
				}
#endif
				// === END DIAGNOSTIC ===

				if (target == DLSSG::RouteTarget::NativeDlssg) {
					if (isFirstCall) {
						LogInfo(kEntry, L"[DLSSG-HYBRID] EvaluateFeature subframe -> native DLSSG");
					}
					NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
					{
						// Lie to native DLSSG: claim this is subframe 0 of 1 (x2 mode)
						// regardless of which midpoint slot Streamline actually requested.
						// Restored when this scope ends (before any FSR3 fallback below).
						MfgParamOverride mfgScope(parameters);
						result = proxy(cmdList, entry.nativeHandle, parameters, callback);
					}
					dlssgRouter->NotifyNativeEvalResult(result);

#if HYBRID_DIAG_LOG
					// Log every native eval result transition (success<->fail)
					// so we can see when DLSSG starts/stops working.
					{
						static std::atomic<int> s_lastNativeRes{ -1 };
						const int curr = NVSDK_NGX_SUCCEED(result) ? 1 : 0;
						if (s_lastNativeRes.exchange(curr) != curr) {
							LogInfo(kEntry,
								std::wstring(L"[DLSSG-HYBRID-DIAG] native eval transition: ")
								+ (curr ? L"SUCCESS" : L"FAIL")
								+ L" (raw result=" + std::to_wstring((int)result) + L")");
						}
					}
#endif

					// Soft fallback to FSR3 if native fails for this subframe.
					// Note: parameters are restored to real (idx, count) here,
					// so FSR3 sees the actual subframe coordinates it needs.
					if (!NVSDK_NGX_SUCCEED(result) && entry.fsr3Ok) {
						LogWarning(kEntry, L"[DLSSG-HYBRID] native eval failed, falling back to FSR3 for this subframe");
						result = dlssgModule->EvaluateD3D12(cmdList, entry.fsr3Handle, parameters, callback);
					}
				}
				else {
					if (isFirstCall) {
						LogInfo(kEntry, L"[DLSSG-HYBRID] EvaluateFeature subframe -> FSR3");
					}
					result = dlssgModule->EvaluateD3D12(cmdList, entry.fsr3Handle, parameters, callback);
				}
			}
		}
		else if (IsDlssgFeature(featureId) && dlssgModule) {
			if (isFirstCall) {
				LogInfo(kEntry, L"[DLSSG-REDIRECT] EvaluateFeature -> dlssgModule");
			}
			result = dlssgModule->EvaluateD3D12(cmdList, featureHandle, parameters, callback);
		}
		else {
			NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
			result = proxy(cmdList, featureHandle, parameters, callback);
		}
		// === END DLSSG REDIRECT ===

		// ---- Per-pass GPU profiler: close the frame slot and drain logs ----
		// EndFrame is a CPU-side state transition only (no GPU work emitted),
		// so it's safe even if cmdList has already been Close()d inside FSR3
		// (non-recording mode). DrainAndLogProfilerReports pops completed
		// frames whose fence has signaled — typically lagging the current
		// evaluate by 1-2 subframes.
		//
		// We use the SAME profEnabled snapshot that was read at BeginFrame —
		// otherwise a mid-evaluate flag toggle could leave the slot OPEN
		// forever (BeginFrame ran, EndFrame skipped) or vice versa.
#if defined(FFX_GPU_PROFILER_AVAILABLE)
		if (profEnabled && IsDlssgFeature(featureId)) {
			FfxProf_EndFrame(cmdList);
		}
		if (profEnabled) {
			DrainAndLogProfilerReports();
		}
#endif

		// Dispatch POST-EVALUATE event
		NgxFeatureEvents::DispatchPostEvaluateD3D12(cmdList, featureHandle, parameters, featureId, result);

		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"NGX failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Feature NgxFrontend::GetFeatureByHandleId(const NVSDK_NGX_Handle* inHandleId)
	{
		auto it = handleRegistry.find(inHandleId);
		if (it == handleRegistry.end()) {
			return NVSDK_NGX_Feature_Reserved_Unknown;
		}

		return it->second;
	}

	NVSDK_NGX_Result NgxFrontend::ReleaseD3D12(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_ReleaseFeature");

		NGX_LOG_CALL;

		// Get feature type before release
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		// Dispatch PRE-RELEASE event (for cleanup like SSRTGI)
		NgxFeatureEvents::DispatchPreReleaseD3D12(featureHandle, featureId);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = false;
		}

		ctx.ngx.isFrameGenerationActive = false;
		//if (ctx.reflex.desiredFpsLimit > 0) {
		//    ctx.reflex.desiredFpsLimit *= 2;
		//}
		if (IsDlssgFeature(featureId)) {
			ctx.ngx.isDuplicatingFrames = false;
			ctx.ngx.framesGenerated = 0;
			ctx.ngx.maxFramesGenerated = 1;
			ctx.ngx.isGeneratingFrames = false;
		}
		// === DLSSG REDIRECT ===
		if (IsDlssgFeature(featureId) && dlssgRouter && dlssgRouter->IsActive() && dlssgModule) {
			// Hybrid release: tear down whichever backends we created. The router
			// hands us back the entry it had, then forgets about it.
			const auto entry = dlssgRouter->ForgetPair(featureHandle);
			LogInfo(kEntry, L"[DLSSG-HYBRID] ReleaseFeature -> native + FSR3 teardown");

			NVSDK_NGX_Result nativeRes = NVSDK_NGX_Result_Success;
			NVSDK_NGX_Result fsr3Res = NVSDK_NGX_Result_Success;

			// Native release. Skip if it's the same pointer as fsr3 (degraded path
			// where FSR3 became primary because native create failed).
			if (entry.nativeOk && entry.nativeHandle && entry.nativeHandle != entry.fsr3Handle) {
				NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
				nativeRes = proxy(entry.nativeHandle);
			}
			if (entry.fsr3Ok && entry.fsr3Handle) {
				fsr3Res = dlssgModule->ReleaseD3D12(entry.fsr3Handle);
			}

			if (!entry.nativeOk && !entry.fsr3Ok) {
				// Unknown handle — pass through to native.
				LogWarning(kEntry, L"[DLSSG-HYBRID] unknown handle in Release, passing through to native");
				NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
				result = proxy(featureHandle);
			}
			else {
				result = NVSDK_NGX_SUCCEED(nativeRes) ? fsr3Res : nativeRes;
			}
		}
		else if (IsDlssgFeature(featureId) && dlssgModule) {
			LogInfo(kEntry, L"[DLSSG-REDIRECT] ReleaseFeature -> dlssgModule");
			result = dlssgModule->ReleaseD3D12(featureHandle);
		}
		else {
			NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
			result = proxy(featureHandle);
		}
		// === END DLSSG REDIRECT ===

		if (NVSDK_NGX_SUCCEED(result)) {
			handleRegistry.erase(featureHandle);
		}

		// Dispatch POST-RELEASE event
		NgxFeatureEvents::DispatchPostReleaseD3D12(featureHandle, featureId, result);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitVulkan(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		VkInstance instance,
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		PFN_vkGetInstanceProcAddr getInstanceProcAddr,
		PFN_vkGetDeviceProcAddr getDeviceProcAddr,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo,
		NVSDK_NGX_Version sdkVersion)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init");

		NGX_LOG_CALL;

		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long,
			const wchar_t*,
			VkInstance,
			VkPhysicalDevice,
			VkDevice,
			PFN_vkGetInstanceProcAddr,
			PFN_vkGetDeviceProcAddr,
			NVSDK_NGX_FeatureCommonInfo*,
			NVSDK_NGX_Version);
		result = proxy(applicationId, applicationDataPath, instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr, InFeatureInfo, sdkVersion);
		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitVulkanExt(
		unsigned long long applicationId,
		const wchar_t* applicationDataPath,
		VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
		NVSDK_NGX_Version sdkVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext");

		NGX_LOG_CALL;
		InitNGX(applicationDataPath, sdkVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long,
			const wchar_t*,
			VkInstance, VkPhysicalDevice, VkDevice,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);

		result = proxy(applicationId, applicationDataPath,
			instance, physicalDevice, device,
			sdkVersion, InFeatureInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::InitVulkanExt2(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
		VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice,
		PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA,
		NVSDK_NGX_Version InSDKVersion,
		NVSDK_NGX_FeatureCommonInfo* InFeatureInfo)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext2");

		NGX_LOG_CALL;

		InitNGX(InApplicationDataPath, InSDKVersion, &InFeatureInfo);
		NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*,
			VkInstance, VkPhysicalDevice, VkDevice,
			PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr,
			NVSDK_NGX_Version,
			NVSDK_NGX_FeatureCommonInfo*);
		result = proxy(InApplicationId, InApplicationDataPath, InInstance, InPD, InDevice, InGIPA, InGDPA, InSDKVersion, InFeatureInfo);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::CreateVulkan(
		VkCommandBuffer CommandList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature");

		NGX_LOG_CALL;

		// Dispatch PRE-CREATE event
		NgxFeatureEvents::DispatchPreCreateVulkan(CommandList, featureId, parameters);

		NGX_RESOLVE_PROXY_ONCE(VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
		result = proxy(CommandList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			if (featureId == NVSDK_NGX_Feature_SuperSampling || featureId == NVSDK_NGX_Feature_RayReconstruction) {
				ctx.ngx.isUpscalingActive = true;
				parameters->Get("PerfQualityValue", &ctx.ngx.upscalingQuality);
			}

			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			// Dispatch POST-CREATE event
			NgxFeatureEvents::DispatchPostCreateVulkan(CommandList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::CreateVulkan1(
		const VkDevice device,
		VkCommandBuffer CommandList,
		NVSDK_NGX_Feature featureId,
		NVSDK_NGX_Parameter* parameters,
		NVSDK_NGX_Handle** outHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature1");

		NGX_LOG_CALL;

		// Dispatch PRE-CREATE event
		NgxFeatureEvents::DispatchPreCreateVulkan(CommandList, featureId, parameters);

		NGX_RESOLVE_PROXY_ONCE(const VkDevice, VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
		result = proxy(device, CommandList, featureId, parameters, outHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			if (featureId == NVSDK_NGX_Feature_SuperSampling || featureId == NVSDK_NGX_Feature_RayReconstruction) {
				ctx.ngx.isUpscalingActive = true;
			}
			const NVSDK_NGX_Handle* id = *outHandle;
			handleRegistry.emplace(id, featureId);

			// Dispatch POST-CREATE event
			NgxFeatureEvents::DispatchPostCreateVulkan(CommandList, featureId, parameters, *outHandle, result);
		}

		NGX_LOG_RESULT_AND_RETURN;
	}

	// =======================================================================
	// DLSS Optimal-Settings callback override (DRS lockdown + ratio repair)
	//
	// The game queries NGX for the "optimal settings" callback via
	// (Get|GetCapability)Parameters. The callback computes the render
	// resolution from the chosen DLSS quality preset and ALSO declares the
	// DRS [Min..Max] band that the engine is allowed to slide between
	// frame-to-frame.
	//
	// We intercept that pointer the first time we see it on a Parameter
	// instance returned by the upstream NGX (or proxy backend), cache the
	// original, and install our own thunk. Each call we forward to the
	// original to let it compute OutWidth/OutHeight/Scale however it wants,
	// then we post-process the result:
	//
	//   1) Detect rounding error between OutWidth/OutHeight and the input
	//      display size and snap height onto the same ratio that width
	//      ended up at. This catches the off-by-1px asymmetry that comes
	//      from the original callback dividing W and H independently
	//      (`H/1.7` vs `W/1.7` rounded separately). UE periodically asks
	//      the callback with patological micro-sizes (e.g. 10x10) to probe
	//      capabilities; we treat those as out-of-band and leave them
	//      untouched - kicking in compensation there would be wrong.
	//
	//   2) Hard-lock DRS by forcing
	//          Min_Render_W = Max_Render_W = OutWidth
	//          Min_Render_H = Max_Render_H = OutHeight
	//      regardless of UltraPerf/Performance/etc. preset, regardless of
	//      Config.ExtendedLimits / DrsMinOverride / DrsMaxOverride - those
	//      knobs belong to the upstream callback's logic and we override
	//      unconditionally as instructed.
	//
	//   3) Re-emit Scale / SuperSampling_ScaleFactor consistent with the
	//      compensated OutWidth, plus mirror to the legacy EParameter_*
	//      keys some older games still read.
	//
	// Hook installation is idempotent per process: we use a single global
	// atomic to remember the original callback, and a CAS so concurrent
	// (Get|GetCapability)Parameters calls race-safely. Subsequent
	// GetParameters calls will simply re-Set our thunk on every fresh
	// Parameter instance the proxy hands back.
	// =======================================================================
	namespace
	{
		// The NGX SDK exposes the callback type and parameter key like this;
		// re-declare locally in case the bundled headers don't surface them
		// under these exact names.
		using PFN_GetOptimalSettingsCb = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Parameter*);

		static std::atomic<PFN_GetOptimalSettingsCb> g_OriginalOptimalSettingsCb{ nullptr };

		// Read+set helper to keep the callback body compact. Returns 0 if the
		// key is missing - we then leave that field alone.
		static inline bool TryGetUInt(NVSDK_NGX_Parameter* p, const char* key, unsigned int& out)
		{
			return p && p->Get(key, &out) == NVSDK_NGX_Result_Success;
		}

		// Snap height onto width's effective ratio, but only if the delta is
		// small enough to be a rounding artifact rather than an intentional
		// asymmetric request from the engine. Returns the (possibly
		// adjusted) height.
		//
		// Rationale for the tolerance: when the upstream callback divides
		// width and height independently by the same float divisor (1.5,
		// 1.7, 2.0, 3.0), the per-axis truncation can disagree by up to 1px
		// at 1080p and a couple px at 4K. Anything bigger than ~1% of the
		// display height is a deliberate non-square stretch (or a
		// patological probe like UE's 10x10) and we don't touch it.
		static inline unsigned int SnapHeightToWidthRatio(
			unsigned int displayW, unsigned int displayH,
			unsigned int outW, unsigned int outH,
			bool& outCompensated, bool& outSkipped)
		{
			outCompensated = false;
			outSkipped = false;

			// Sanity guard against patological inputs - UE engines sometimes
			// poke the callback with single-digit sizes during init/probe.
			if (displayW < 32 || displayH < 32 || outW == 0 || outH == 0) {
				outSkipped = true;
				return outH;
			}

			const float ratio = static_cast<float>(outW) / static_cast<float>(displayW);
			const unsigned int expectedH =
				static_cast<unsigned int>(static_cast<float>(displayH) * ratio + 0.5f);

			// Tolerance: 1% of display height, but never less than 2px.
			const unsigned int tolerance =
				std::max<unsigned int>(2u, displayH / 100u);

			const unsigned int delta =
				(outH > expectedH) ? (outH - expectedH) : (expectedH - outH);

			if (delta == 0) {
				return outH;        // Already exact.
			}
			if (delta <= tolerance) {
				outCompensated = true;
				return expectedH;   // Rounding artifact - snap to width's ratio.
			}
			outSkipped = true;
			return outH;            // Deliberate asymmetry - leave alone.
		}

		// Our replacement callback. Forwards to the original, then enforces
		// (a) ratio consistency and (b) DRS lockdown.
		static NVSDK_NGX_Result NVSDK_CONV HookedGetOptimalSettingsCallback(
			NVSDK_NGX_Parameter* InParams)
		{
			auto orig = g_OriginalOptimalSettingsCb.load(std::memory_order_acquire);
			if (!orig || !InParams) {
				return NVSDK_NGX_Result_Fail;
			}

			unsigned int perfValue = 0;
			TryGetUInt(InParams, "PerfQualityValue", perfValue);
			if (perfValue == ctx.ngx.dlaaId || (perfValue == 0 && ctx.ngx.dlaaId != NVSDK_NGX_PerfQuality_Value_DLAA)) {
				InParams->Set("PerfQualityValue", NVSDK_NGX_PerfQuality_Value_DLAA);
				LOG_WARNING(L"[NVNGX] Overriding DLAA setting for " + std::to_wstring(perfValue));
			}

			// Snapshot input display size before forwarding (the callback
			// itself doesn't mutate Width/Height but we want a stable copy
			// for the post-processing).
			unsigned int displayW = 0, displayH = 0;
			TryGetUInt(InParams, NVSDK_NGX_Parameter_Width, displayW);
			TryGetUInt(InParams, NVSDK_NGX_Parameter_Height, displayH);

			// Let the upstream callback compute OutWidth/OutHeight/Scale.
			NVSDK_NGX_Result r = orig(InParams);
			if (!NVSDK_NGX_SUCCEED(r)) {
				return r;
			}
			unsigned int outW = 0, outH = 0; float scaleRatio = 1.0f;
			InParams->Get(NVSDK_NGX_Parameter_Scale, &scaleRatio);
			LOG_WARNING(L"HookedGetOptimalSettingsCallback: " + std::to_wstring(scaleRatio) + L" : "
				+ std::to_wstring(displayW) + L"," + std::to_wstring(displayH) + L" -> " + std::to_wstring(outW) + L"," + std::to_wstring(outH)
			);

			// ------------------------
			return r;

			//			unsigned int outW = 0, outH = 0;
			//			if (!TryGetUInt(InParams, NVSDK_NGX_Parameter_OutWidth, outW) ||
			//				!TryGetUInt(InParams, NVSDK_NGX_Parameter_OutHeight, outH)) {
			//				// Original succeeded but didn't write OutW/OutH - bail out
			//				// without further mutation rather than poison the params.
			//				return r;
			//			}
			//
			//			// (1) Ratio repair.
			//			bool didCompensate = false;
			//			bool didSkip = false;
			//			const unsigned int snappedH = SnapHeightToWidthRatio(
			//				displayW, displayH, outW, outH, didCompensate, didSkip);
			//
			//			if (didCompensate) {
			//				outH = snappedH;
			//				InParams->Set(NVSDK_NGX_Parameter_OutHeight, outH);
			//				InParams->Set(NVSDK_NGX_EParameter_OutHeight, outH);
			//			}
			//			else if (didSkip && displayW >= 32 && displayH >= 32) {
			//				// Only warn for "real" sizes; suppress for the UE probe path
			//				// where we already short-circuited via the sanity guard.
			//				// (didSkip is set in both cases, so we need this displayW/H
			//				// re-check to distinguish "asymmetric request" from "tiny
			//				// probe input".)
			//				//
			//				// Note: we cannot call LogWarning here - this is a free
			//				// function in an anonymous namespace, no access to the
			//				// NgxFrontend instance. Drop a debug breadcrumb via the
			//				// project-wide LOG_WARN macro if available; otherwise
			//				// silently leave OutHeight as the upstream gave it.
			//#ifdef LOG_WARN
			//				LOG_WARN("HookedGetOptimalSettingsCallback: skipping ratio "
			//					"compensation, asymmetric request "
			//					"display={0}x{1} out={2}x{3}",
			//					displayW, displayH, outW, outH);
			//#endif
			//			}
			//
			//			// Recompute Scale from the (possibly snapped) OutWidth so it
			//			// stays consistent with what we actually emit. Use width as the
			//			// canonical axis - height has been snapped onto it above.
			//			float scaleRatio = 1.0f;
			//			if (displayW > 0) {
			//				scaleRatio = static_cast<float>(outW) / static_cast<float>(displayW);
			//			}
			//			InParams->Set(NVSDK_NGX_Parameter_Scale, scaleRatio);
			//			InParams->Set(NVSDK_NGX_Parameter_SuperSampling_ScaleFactor, scaleRatio);
			//			InParams->Set(NVSDK_NGX_EParameter_Scale, scaleRatio);
			//
			//			// (2) DRS lockdown - unconditional, ignores all upstream logic.
			//			InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width, outW);
			//			InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height, outH);
			//			InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width, outW);
			//			InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height, outH);
			//
			//			return r;
		}

		// Idempotent installer. Call after every successful (Get|
		// GetCapability)Parameters - if the upstream callback is already
		// our thunk, this is a no-op; if it's the real one, we cache it
		// (CAS) and install our thunk in its place on the parameter map.
		static void InstallOptimalSettingsHook(NVSDK_NGX_Parameter* params)
		{
			return;
			if (!params) return;

			void* rawCb = nullptr;
			if (params->Get(NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback, &rawCb) !=
				NVSDK_NGX_Result_Success || rawCb == nullptr) {
				return;     // Backend didn't provide one (e.g. AllocateParameters).
			}

			auto cb = reinterpret_cast<PFN_GetOptimalSettingsCb>(rawCb);
			if (cb == &HookedGetOptimalSettingsCallback) {
				return;     // Already our thunk on this Parameter instance.
			}

			// Cache the original exactly once. CAS handles the race where
			// multiple threads call GetParameters concurrently on first use.
			PFN_GetOptimalSettingsCb expected = nullptr;
			g_OriginalOptimalSettingsCb.compare_exchange_strong(
				expected, cb, std::memory_order_acq_rel);

			// Always re-set the thunk on this Parameter instance, even if
			// CAS lost the race - subsequent GetParameters calls return
			// fresh Parameter maps that need patching individually.
			params->Set(
				NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback,
				reinterpret_cast<void*>(&HookedGetOptimalSettingsCallback));
		}
	} // anonymous namespace

	NVSDK_NGX_Result NgxFrontend::AllocateParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::AllocateParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::AllocateParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_AllocateParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::DestroyParametersD3D12(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_DestroyParameters");
		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);
		result = proxy(InParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::DestroyParametersVulkan(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_DestroyParameters");
		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);
		result = proxy(InParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::DestroyParametersD3D11(NVSDK_NGX_Parameter* InParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_DestroyParameters");
		NGX_LOG_CALL;

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);
		result = proxy(InParameters);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		// === DRS LOCKDOWN: hook DLSS optimal-settings callback ===
		if (NVSDK_NGX_SUCCEED(result) && OutParameters && *OutParameters) {
			InstallOptimalSettingsHook(*OutParameters);
		}
		// === END DRS LOCKDOWN ===

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetCapabilityParametersD3D11(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D11_GetCapabilityParameters");
		result = NVSDK_NGX_Result_Success;

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		// === DRS LOCKDOWN: hook DLSS optimal-settings callback ===
		if (NVSDK_NGX_SUCCEED(result) && OutParameters && *OutParameters) {
			InstallOptimalSettingsHook(*OutParameters);
		}
		// === END DRS LOCKDOWN ===

		//if (ctx.ngx.isDeepDvcEnabled && (ctx.nvapi.isEmbeddedNvapiUsed || ctx.realGpuArchitecture < NV_GPU_ARCHITECTURE_TU100)) {
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_Available, 1);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMajor, 10);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_MinDriverVersionMinor, 10);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_FeatureInitResult, 1);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_GetStatsCallback, NGX_DeepDvcCallback);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_SizeInBytes, 1024 * 1024);
		//}
		//else if (ctx.ngx.isDeepDvcEnabled == false) {
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_Available, 0);
		//	(*OutParameters)->Set(NVSDK_NGX_Parameter_DeepDVC_FeatureInitResult, 0);
		//}

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_GetParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		// === DRS LOCKDOWN: hook DLSS optimal-settings callback ===
		if (NVSDK_NGX_SUCCEED(result) && OutParameters && *OutParameters) {
			InstallOptimalSettingsHook(*OutParameters);
		}
		// === END DRS LOCKDOWN ===

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetCapabilityParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetCapabilityParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		// === DRS LOCKDOWN: hook DLSS optimal-settings callback ===
		if (NVSDK_NGX_SUCCEED(result) && OutParameters && *OutParameters) {
			InstallOptimalSettingsHook(*OutParameters);
		}
		// === END DRS LOCKDOWN ===

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetParametersVulkan(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetCapabilityParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		// === DRS LOCKDOWN: hook DLSS optimal-settings callback ===
		if (NVSDK_NGX_SUCCEED(result) && OutParameters && *OutParameters) {
			InstallOptimalSettingsHook(*OutParameters);
		}
		// === END DRS LOCKDOWN ===

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetCapabilityParametersD3D12(NVSDK_NGX_Parameter** OutParameters)
	{
		NGX_INIT_CALL("NVSDK_NGX_D3D12_GetCapabilityParameters");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter**);
		result = proxy(OutParameters);

		// === DRS LOCKDOWN: hook DLSS optimal-settings callback ===
		if (NVSDK_NGX_SUCCEED(result) && OutParameters && *OutParameters) {
			InstallOptimalSettingsHook(*OutParameters);
		}
		// === END DRS LOCKDOWN ===

		// === DLSSG POPULATE ===
		if (NVSDK_NGX_SUCCEED(result) && dlssgModule && *OutParameters) {
			LogInfo(kEntry, L"[DLSSG] Populating capability parameters");
			dlssgModule->PopulateParametersD3D12(*OutParameters);
		}
		// === END DLSSG POPULATE ===

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::GetFeatureInstanceExtensionRequirementsVulkan(const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, uint32_t* OutExtensionCount,
		VkExtensionProperties** OutExtensionProperties
	)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_EvaluateFeature");

		NGX_LOG_CALL;
		NGX_RESOLVE_PROXY_ONCE(const NVSDK_NGX_FeatureDiscoveryInfo*, uint32_t*, VkExtensionProperties**);
		result = proxy(FeatureDiscoveryInfo, OutExtensionCount, OutExtensionProperties);

		NGX_LOG_RESULT_AND_RETURN;
	}

	NVSDK_NGX_Result NgxFrontend::EvaluateVulkan(
		VkCommandBuffer cmdBuffer,
		const NVSDK_NGX_Handle* featureHandle,
		NVSDK_NGX_Parameter* parameters,
		PFN_NVSDK_NGX_ProgressCallback callback)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_EvaluateFeature");

		// Latch first-call; we reset this later in Destroy/Release path.
		const bool isFirstCall = !state.isNgxEvaluated.exchange(true);

		if (isFirstCall) {
			NGX_LOG_CALL;
		}

		// Get feature type for event dispatching
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		if (featureId == NVSDK_NGX_Feature_SuperSampling || featureId == NVSDK_NGX_Feature_RayReconstruction) {
			ctx.ngx.isUpscalingActive = true;
		}

		// Dispatch PRE-EVALUATE event
		NgxFeatureEvents::DispatchPreEvaluateVulkan(cmdBuffer, featureHandle, parameters, featureId);

		OnEvaluateFeature(featureHandle, parameters);
		NGX_RESOLVE_PROXY_ONCE(VkCommandBuffer, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);
		result = proxy(cmdBuffer, featureHandle, parameters, callback);

		// Dispatch POST-EVALUATE event
		NgxFeatureEvents::DispatchPostEvaluateVulkan(cmdBuffer, featureHandle, parameters, featureId, result);

		if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
			// Only log failures after the first call
			if (!NVSDK_NGX_SUCCEED(result)) {
				LogWarning(kEntry, L"NGX failed for " + std::to_wstring(featureHandle->Id));
			}
			NGX_LOG_RESULT_AND_RETURN;
		}

		return result;
	}

	NVSDK_NGX_Result NgxFrontend::ReleaseVulkan(NVSDK_NGX_Handle* featureHandle)
	{
		NGX_INIT_CALL("NVSDK_NGX_VULKAN_ReleaseFeature");

		NGX_LOG_CALL;

		// Get feature type before release
		NVSDK_NGX_Feature featureId = GetFeatureByHandleId(featureHandle);

		// Dispatch PRE-RELEASE event
		NgxFeatureEvents::DispatchPreReleaseVulkan(featureHandle, featureId);

		if (featureId == NVSDK_NGX_Feature_SuperSampling) {
			ctx.ngx.isUpscalingActive = false;
		}

		NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);
		result = proxy(featureHandle);

		if (NVSDK_NGX_SUCCEED(result)) {
			handleRegistry.erase(featureHandle);
		} 

		// Dispatch POST-RELEASE event
		NgxFeatureEvents::DispatchPostReleaseVulkan(featureHandle, featureId, result);

		NGX_LOG_RESULT_AND_RETURN;
	}

	HMODULE NgxFrontend::GetBackend()
	{
		if (!ctx.ngx.isEmbeddedNgxUsed) {
			auto handle = GetModuleHandleW(L"dlss-enabler-upscaler.dll");

			if (handle) {
				return handle;
			}

			if (!Common::IsPluginPresent(L"dlss-enabler-upscaler.dll")) {
				ctx.ngx.isEmbeddedNgxUsed = true;
				return backends.GetUpscaler();
			}

			return Common::LoadPlugin(L"dlss-enabler-upscaler.dll");
		}

		return backends.GetUpscaler();
	}
}