#pragma once
#include <dxgi1_6.h>
#include <d3d12.h>
#include <functional>
#include <mutex>
#include "Common.h"
#include "../Core/Context.h"
#include "SwapchainColorState.h"

#define SWAPCHAIN_DEBUGA

#include <Unknwn.h>  // For IUnknown and REFIID

// Custom marker type for proxy self-detection. Used by other subsystems
// (DLSS-G / Streamline / OptiScaler hooks) to check "is this pointer one of
// our own wrapped swapchains?" via QueryInterface.
//
// IMPORTANT: This is DELIBERATELY NOT a COM interface anymore and the
// wrapper class DOES NOT inherit from it. Previously the wrapper inherited
// from both IDXGISwapChain4 AND IDXGISwapChain4Interface (derived from
// IUnknown) - which gave the object two IUnknown vtables and broke COM
// identity (QI(IID_IUnknown) returning different pointers depending on
// which sub-object path the cast used). On hybrid GPU setups
// (NVIDIA dGPU + AMD iGPU) this identity violation is caught by
// Streamline / driver-side QI verification and trips a CDPR RED_ASSERT
// int 3 during startup.
//
// The type is kept as an empty struct with __declspec(uuid(...)) so that
// existing callers using __uuidof(IDXGISwapChain4Interface) still compile.
// It is NOT a COM interface - there is no vtable. Callers doing
// QI(IID/uuidof IDXGISwapChain4Interface, ...) now receive a pointer to
// the canonical IUnknown (same as QI(IID_IUnknown)). Use the returned
// pointer ONLY as a non-null marker ("it's our wrapper"); do NOT invoke
// methods on it through any IDXGISwapChain4Interface vtable. If you need
// to call wrapper methods, immediately QI to IDXGISwapChain4.
struct __declspec(uuid("12345678-1234-1234-1234-1234567890AB"))
	IDXGISwapChain4Interface {
};

class DxgiWrappedIDXGISwapChain4 : public IDXGISwapChain4
{
	IDXGISwapChain* m_pReal = nullptr;
	IDXGISwapChain1* m_pReal1 = nullptr;
	IDXGISwapChain2* m_pReal2 = nullptr;
	IDXGISwapChain3* m_pReal3 = nullptr;
	IDXGISwapChain4* m_pReal4 = nullptr;

	// REFramework scans the swapchain object looking for a command queue pointer.
	// If it doesn't find one in the first pass, it enters a "Proton/FrameGen" detection
	// path that scans through internal pointers (like m_pReal), finds the command queue
	// inside the real swapchain, and then hooks the inner swapchain's vtable directly.
	// This causes crashes when combined with Streamline because REFramework's vtable
	// hooks end up on the wrong object layer.
	//
	// By storing the command queue pointer here, REFramework finds it immediately
	// in the first scan pass, sets a normal offset, and hooks our wrapper's vtable
	// instead � which correctly delegates everything to the real swapchain.
	ID3D12CommandQueue* m_pCommandQueue = nullptr;

	bool isShuttingDown = false;

	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> preRenderTrig = nullptr;
	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> postRenderTrig = nullptr;
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> preRenderTrig1 = nullptr;
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> postRenderTrig1 = nullptr;
	std::function<void(IDXGISwapChain*)> preClearTrig = nullptr;
	std::function<void(IDXGISwapChain*)> postClearTrig = nullptr;

	// Fired once from Release() when m_iRefcount hits 0, before delete this.
	// Lets subscribers (e.g. the overlay) drop references to this swapchain's
	// back buffers before a swapchain is recreated on the same HWND. Nulled by
	// DetachTriggers() on HookDxgi::Uninstall, like the other trigs.
	std::function<void(IDXGISwapChain*, HWND)> onDestroyTrig = nullptr;

	// Protects the trig std::function members above. Present / Present1 /
	// ResizeBuffers / ResizeBuffers1 snapshot the relevant trig under this mutex
	// and invoke the snapshot WITHOUT holding the mutex, so the hot path is not
	// serialized with itself across threads / swapchains. DetachTriggers takes
	// the mutex to null the originals; any concurrent snapshot copy already made
	// keeps the lambda (and its captures) alive until the caller returns.
	mutable std::mutex m_trigMutex;

	volatile LONG m_iRefcount;

public:

	DxgiWrappedIDXGISwapChain4(IDXGISwapChain* real,
		std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> preRenderTrig,
		std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> postRenderTrig,
		std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> preRenderTrig1,
		std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> postRenderTrig1,
		std::function<void(IDXGISwapChain*)> preClearTrig,
		std::function<void(IDXGISwapChain*)> postClearTrig,
		ID3D12CommandQueue* pCommandQueue = nullptr,
		std::function<void(IDXGISwapChain*, HWND)> onDestroyTrig = nullptr);

	virtual ~DxgiWrappedIDXGISwapChain4();

	// Update the stored command queue pointer (e.g. after swapchain recreation)
	void SetCommandQueue(ID3D12CommandQueue* pQueue);

	// Null all trig std::function members under m_trigMutex.
	// After this returns, any future Present / Present1 / ResizeBuffers /
	// ResizeBuffers1 call on this wrapper will skip the trig dispatch.
	// In-flight calls that already took a snapshot of a trig will complete
	// safely using their own copy. Called by DetachAllSwapchainTriggers()
	// from HookDxgi::Uninstall.
	void DetachTriggers();

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

	ULONG STDMETHODCALLTYPE AddRef()
	{
		LONG ref = InterlockedIncrement(&m_iRefcount);
		return static_cast<ULONG>(ref);
	}

	ULONG STDMETHODCALLTYPE Release()
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"Release2");
#endif
		LONG ret = InterlockedDecrement(&m_iRefcount);

		if (ret == 0)
		{
			isShuttingDown = true;

			HWND hwnd = nullptr;
			if (m_pReal1) {
				m_pReal1->GetHwnd(&hwnd);
			}

			// Snapshot under m_trigMutex and invoke outside the lock, in line with
			// Present/ResizeBuffers. If DetachTriggers() (HookDxgi::Uninstall) has
			// nulled it, the snapshot is null and we skip dispatch - so a lingering
			// wrapper held by the game won't call into an unloaded DLL.
			std::function<void(IDXGISwapChain*, HWND)> snapDestroy;
			{
				std::lock_guard<std::mutex> lk(m_trigMutex);
				snapDestroy = onDestroyTrig;
			}
			if (snapDestroy)
				snapDestroy(m_pReal, hwnd);

			delete this;
		}

		return static_cast<ULONG>(ret);
	}

	//////////////////////////////
	// implement IDXGIObject

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetPrivateData");
#endif

		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal->SetPrivateData(Name, DataSize, pData);

		if (ret != S_OK) {
			LOG_ERROR(L"SetPrivateData: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetPrivateDataInterface");
#endif
		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal->SetPrivateDataInterface(Name, pUnknown);

		if (ret != S_OK) {
			LOG_ERROR(L"SetPrivateDataInterface: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetPrivateData");
#endif

		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal->GetPrivateData(Name, pDataSize, pData);
		static const GUID IID_IFfxAntiLag2Data = { 0x5083ae5b, 0x8070, 0x4fca, {0x8e, 0xe5, 0x35, 0x82, 0xdd, 0x36, 0x7d, 0x13} };
		if (Name == IID_IFfxAntiLag2Data) {
			// GetPrivateData can be invoked concurrently from multiple threads
			// (game render thread + FFX worker threads all legally hit this path).
			// optiFgCycle is a plain uint64_t in Context so use an interlocked
			// 64-bit increment to avoid torn reads / lost updates.
			// 32-bit InterlockedIncrement would silently clobber the upper half.
			InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&ctx.reflex.optiFgCycle));
			if (ctx.logging.isReflexDebugEnabled) {
				//LOG_WARNING(L"[DXGI] FSR3 FG detected!!!");
			}

			if (ctx.enableReflexInjection) {
				return DXGI_ERROR_NOT_FOUND;
			}
		}

		if (ret != S_OK) {
			//LOG_ERROR(L"GetPrivateData: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetParent");
#endif

		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		// TODO: GetParent currently returns the REAL IDXGIFactory. If the game
		// uses the returned factory to create a second swapchain, that swapchain
		// bypasses our wrapper. On hybrid GPU setups this can lead to inconsistent
		// behavior between the primary and secondary swapchain. If we ever maintain
		// a wrapped factory handle, return it here instead.
		HRESULT ret = m_pReal->GetParent(riid, ppParent);

		// E_NOINTERFACE is a contractual result when callers probe IIDs the
		// parent doesn't expose (e.g. newer IDXGIFactoryN versions than the
		// runtime provides). Don't pollute the log with expected negatives.
		if (FAILED(ret) && ret != E_NOINTERFACE) {
			LOG_ERROR(L"GetParent: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	//////////////////////////////
	// implement IDXGIDeviceSubObject

	virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice);

	//////////////////////////////
	// implement IDXGISwapChain

	virtual HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags);

	virtual HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface);

	virtual HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget);

	virtual HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget);

	virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetDesc");
#endif

		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal->GetDesc(pDesc);

		if (ret != S_OK) {
			LOG_ERROR(L"GetDesc: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

	virtual HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"ResizeTarget");
#endif

		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal->ResizeTarget(pNewTargetParameters);

		if (ret != S_OK) {
			LOG_ERROR(L"ResizeTarget: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** ppOutput);

	virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetFrameStatistics");
#endif

		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal->GetFrameStatistics(pStats);

		if (ret != S_OK) {
			LOG_ERROR(L"GetFrameStatistics: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetLastPresentCount");
#endif

		if (m_pReal == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal->GetLastPresentCount(pLastPresentCount);

		if (ret != S_OK) {
			LOG_ERROR(L"GetLastPresentCount: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	//////////////////////////////
	// implement IDXGISwapChain1

	virtual HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetDesc1");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->GetDesc1(pDesc);

		if (ret != S_OK) {
			LOG_ERROR(L"GetDesc1: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetFullscreenDesc");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->GetFullscreenDesc(pDesc);

		if (ret != S_OK) {
			LOG_ERROR(L"GetFullscreenDesc: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetHwnd(HWND* pHwnd)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetHwnd");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->GetHwnd(pHwnd);

		if (ret != S_OK) {
			LOG_ERROR(L"GetHwnd: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void** ppUnk)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetCoreWindow");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->GetCoreWindow(refiid, ppUnk);

		if (ret != S_OK) {
			LOG_ERROR(L"GetCoreWindow: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);

	virtual BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported(void)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"IsTemporaryMonoSupported");
#endif

		if (m_pReal1 == nullptr)
			return FALSE;

		BOOL ret = m_pReal1->IsTemporaryMonoSupported();

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput);

	virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* pColor)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetBackgroundColor");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->SetBackgroundColor(pColor);

		if (ret != S_OK) {
			LOG_ERROR(L"SetBackgroundColor: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* pColor)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetBackgroundColor");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->GetBackgroundColor(pColor);

		if (ret != S_OK) {
			LOG_ERROR(L"GetBackgroundColor: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetRotation");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->SetRotation(Rotation);

		if (ret != S_OK) {
			LOG_ERROR(L"SetRotation: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* pRotation)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetRotation");
#endif

		if (m_pReal1 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal1->GetRotation(pRotation);

		if (ret != S_OK) {
			LOG_ERROR(L"GetRotation: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	//////////////////////////////
	// implement IDXGISwapChain2

	virtual HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetSourceSize");
#endif

		if (m_pReal2 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal2->SetSourceSize(Width, Height);

		if (ret != S_OK) {
			LOG_ERROR(L"SetSourceSize: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* pWidth, UINT* pHeight)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetSourceSize");
#endif

		if (m_pReal2 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal2->GetSourceSize(pWidth, pHeight);

		if (ret != S_OK) {
			LOG_ERROR(L"GetSourceSize: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetMaximumFrameLatency");
#endif

		if (m_pReal2 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal2->SetMaximumFrameLatency(MaxLatency);

		if (ret != S_OK) {
			LOG_ERROR(L"SetMaximumFrameLatency: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetMaximumFrameLatency");
#endif

		if (m_pReal2 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal2->GetMaximumFrameLatency(pMaxLatency);

		if (ret != S_OK) {
			LOG_ERROR(L"GetMaximumFrameLatency: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject(void)
	{
#ifdef SWAPCHAIN_DEBUG

		LOG_DEBUG(L"GetFrameLatencyWaitableObject");
#endif
		if (m_pReal2 == nullptr)
			return nullptr;
		return m_pReal2->GetFrameLatencyWaitableObject();
	}

	virtual HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetMatrixTransform");
#endif

		if (m_pReal2 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal2->SetMatrixTransform(pMatrix);

		if (ret != S_OK) {
			LOG_ERROR(L"SetMatrixTransform: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* pMatrix)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetMatrixTransform");
#endif

		if (m_pReal2 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal2->GetMatrixTransform(pMatrix);

		if (ret != S_OK) {
			LOG_ERROR(L"GetMatrixTransform: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	//////////////////////////////
	// implement IDXGISwapChain3

	virtual UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex(void)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"GetCurrentBackBufferIndex");
#endif

		// IDXGISwapChain3::GetCurrentBackBufferIndex returns UINT (the back buffer
		// index), NOT HRESULT. There is no error code to inspect here.
		if (m_pReal3 == nullptr) {
			return 0;
		}
		return m_pReal3->GetCurrentBackBufferIndex();
	}

	virtual HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"CheckColorSpaceSupport");
#endif

		if (m_pReal3 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal3->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);

		if (ret != S_OK) {
			LOG_ERROR(L"CheckColorSpaceSupport: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetColorSpace1");
#endif

		if (m_pReal3 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal3->SetColorSpace1(ColorSpace);

		// Snapshot the colour space for the overlay. DXGI has no getter for it,
		// so this is the only chance to observe it. Stored on m_pReal (the
		// canonical object) so a reader holding ANY subinterface finds it -
		// Present dispatches with IDXGISwapChain*, Present1 with
		// IDXGISwapChain1*, and those are different pointer values.
		// Only on success: a rejected SetColorSpace1 leaves the swapchain in
		// its previous space, so recording the requested value would lie.
		if (ret == S_OK) {
			SwapchainColorState::StoreColorSpace(m_pReal, ColorSpace);
		}

		if (ret != S_OK) {
			LOG_ERROR(L"SetColorSpace1: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags,
		_In_reads_(BufferCount) const UINT* pCreationNodeMask, _In_reads_(BufferCount) IUnknown* const* ppPresentQueue);

	//////////////////////////////
	// implement IDXGISwapChain4

	virtual HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, _In_reads_opt_(Size) void* pMetaData)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetHDRMetaData");
#endif

		if (m_pReal4 == nullptr)
			return DXGI_ERROR_DEVICE_REMOVED;

		HRESULT ret = m_pReal4->SetHDRMetaData(Type, Size, pMetaData);

		// Mastering luminance the game itself declares - a better paper-white
		// hint than anything we can infer from the display alone. Merged into
		// the same blob as the colour space (read-modify-write inside).
		if (ret == S_OK) {
			SwapchainColorState::StoreHdrMetaData(m_pReal, Type, Size, pMetaData);
		}

		if (ret != S_OK) {
			LOG_ERROR(L"SetHDRMetaData: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}
};

// Iterates the global live-wrapper registry and calls DetachTriggers() on each.
// Called by HookDxgi::Uninstall before tearing down the rest of the DXGI hook
// subsystem, so that any wrapper the game is still holding stops invoking
// lambdas whose captures point into our DLL.
void DetachAllSwapchainTriggers();