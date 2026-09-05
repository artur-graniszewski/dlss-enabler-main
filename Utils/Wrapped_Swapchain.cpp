#include "Wrapped_Swapchain.h"
#include "Common.h"
#include <d3d12.h>
#include <mutex>
#include <unordered_set>
#include "ffx_gpu_profiler.h"

namespace {
	// Global live-wrapper registry. Every DxgiWrappedIDXGISwapChain4 constructor
	// inserts into this set; every destructor erases from it. HookDxgi::Uninstall
	// iterates over the set to neutralize trig lambdas before tearing down DXGI,
	// so wrappers the game is still holding stop dispatching into our DLL.
	//
	// The mutex only guards the registry itself. Per-wrapper trig mutation uses
	// the wrapper's own m_trigMutex; the two mutexes are never held together.
	std::mutex g_swapchainRegistryMutex;
	std::unordered_set<DxgiWrappedIDXGISwapChain4*> g_liveSwapchains;
}

void DetachAllSwapchainTriggers()
{
	// Hold the registry mutex for the entire iteration. This prevents a wrapper
	// from being destroyed (and its memory freed) while we are about to call
	// DetachTriggers() on it - the destructor also takes this mutex to erase
	// itself from the registry.
	//
	// Lock order: g_swapchainRegistryMutex -> wrapper->m_trigMutex. Destructors
	// only take g_swapchainRegistryMutex, never m_trigMutex, so there is no
	// reverse path and no deadlock risk.
	//
	// DetachTriggers briefly takes m_trigMutex internally; we do NOT take it
	// here.
	std::lock_guard<std::mutex> lk(g_swapchainRegistryMutex);
	for (auto* w : g_liveSwapchains) {
		w->DetachTriggers();
	}
}

DxgiWrappedIDXGISwapChain4::DxgiWrappedIDXGISwapChain4(IDXGISwapChain* real,
	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> PreRenderTrig,
	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> PostRenderTrig,
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> PreRenderTrig1,
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> PostRenderTrig1,
	std::function<void(IDXGISwapChain*)> preClearTrig,
	std::function<void(IDXGISwapChain*)> postClearTrig,
	ID3D12CommandQueue* pCommandQueue,
	std::function<void(IDXGISwapChain*, HWND)> onDestroyTrig) : m_pReal(real), preRenderTrig(PreRenderTrig), postRenderTrig(PostRenderTrig),
	preRenderTrig1(PreRenderTrig1), postRenderTrig1(PostRenderTrig1), preClearTrig(preClearTrig), postClearTrig(postClearTrig), onDestroyTrig(onDestroyTrig), m_iRefcount(1)
{
	real->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&m_pReal1);
	real->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&m_pReal2);
	real->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&m_pReal3);
	real->QueryInterface(__uuidof(IDXGISwapChain4), (void**)&m_pReal4);

	// Store the command queue pointer so REFramework can find it during its
	// object scan. Without this, REFramework enters a secondary scan that
	// dereferences m_pReal, finds the queue inside the real swapchain, and
	// incorrectly treats this as a Proton/FrameGen wrapper � causing it to
	// hook the inner swapchain vtable and crash when Streamline rebuilds it.
	if (pCommandQueue) {
		pCommandQueue->AddRef();
		m_pCommandQueue = pCommandQueue;
	}

	// Register in the global live-wrapper set so HookDxgi::Uninstall can find
	// us and neutralize the trig lambdas. Insertion is the last thing in the
	// constructor: by this point all member state is initialized and it is
	// safe for another thread to call DetachTriggers() on us.
	{
		std::lock_guard<std::mutex> lk(g_swapchainRegistryMutex);
		g_liveSwapchains.insert(this);
	}
}

DxgiWrappedIDXGISwapChain4::~DxgiWrappedIDXGISwapChain4()
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"Release");
#endif

	// Unregister BEFORE releasing anything. After this returns, no concurrent
	// DetachAllSwapchainTriggers() can find us (it iterates under the same
	// mutex). Wrappers already past the registry lock in DetachTriggers will
	// have finished their critical section by the time we reach this line,
	// because DetachAllSwapchainTriggers holds the registry mutex for the
	// full iteration.
	{
		std::lock_guard<std::mutex> lk(g_swapchainRegistryMutex);
		g_liveSwapchains.erase(this);
	}

	if (m_pCommandQueue != nullptr)
	{
		m_pCommandQueue->Release();
		m_pCommandQueue = nullptr;
	}

	if (m_pReal1 != nullptr)
	{
		m_pReal1->Release();
		m_pReal1 = nullptr;
	}

	if (m_pReal2 != nullptr)
	{
		m_pReal2->Release();
		m_pReal2 = nullptr;
	}

	if (m_pReal3 != nullptr)
	{
		m_pReal3->Release();
		m_pReal3 = nullptr;
	}

	if (m_pReal4 != nullptr)
	{
		m_pReal4->Release();
		m_pReal4 = nullptr;
	}

	if (m_pReal != nullptr)
	{
		m_pReal->Release();
		m_pReal = nullptr;
	}
}

void DxgiWrappedIDXGISwapChain4::SetCommandQueue(ID3D12CommandQueue* pQueue)
{
	if (m_pCommandQueue) {
		m_pCommandQueue->Release();
		m_pCommandQueue = nullptr;
	}
	if (pQueue) {
		pQueue->AddRef();
		m_pCommandQueue = pQueue;
	}
}

void DxgiWrappedIDXGISwapChain4::DetachTriggers()
{
	// Nulling std::function inside the critical section guarantees that any
	// thread mid-snapshot in Present/Present1/ResizeBuffers* will either:
	//   (a) complete the snapshot before we null (their copy survives and
	//       they finish their call safely without touching our members), or
	//   (b) take the mutex after us (snapshot reads a null function and skips
	//       the trig dispatch entirely).
	// Either way, no use-after-free on captures.
	std::lock_guard<std::mutex> lk(m_trigMutex);
	preRenderTrig = nullptr;
	postRenderTrig = nullptr;
	preRenderTrig1 = nullptr;
	postRenderTrig1 = nullptr;
	preClearTrig = nullptr;
	postClearTrig = nullptr;
	onDestroyTrig = nullptr;
}

HRESULT STDMETHODCALLTYPE DxgiWrappedIDXGISwapChain4::QueryInterface(REFIID riid, void** ppvObject)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"QueryInterface");
#endif
	if (!ppvObject)
		return E_POINTER;

	*ppvObject = nullptr;

	// COM IDENTITY CANONICAL POINTER
	// --------------------------------
	// Rule: every QI path that yields "this object" must return a pointer
	// whose underlying IUnknown is bit-identical. Drivers / Streamline /
	// the DXGI runtime verify this by calling QI(IID_IUnknown) on whatever
	// interface they received and comparing the result to a cached value.
	// A mismatch is treated as a broken COM object and triggers asserts -
	// on hybrid GPU setups (NVIDIA + AMD iGPU) this is exactly the path
	// that caused REDengine's RED_ASSERT int 3 crash at startup.
	//
	// We standardize on IDXGISwapChain4* as the canonical vtable: every
	// interface in the swapchain family (IUnknown, IDXGIObject,
	// IDXGIDeviceSubObject, IDXGISwapChain, IDXGISwapChain1..4) lives in
	// the same vtable chain through single inheritance, so rooting them
	// all at IDXGISwapChain4* guarantees identical IUnknown pointers.
	IDXGISwapChain4* canonical = static_cast<IDXGISwapChain4*>(this);

	// Custom marker GUID used by DLSS-G / Streamline / OptiScaler hooks to
	// detect "this is one of our wrappers". Returns the canonical IUnknown -
	// callers should treat the result purely as a non-null marker.
	if (riid == __uuidof(IDXGISwapChain4Interface))
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"QueryInterface: proxy detected");
#endif
		* ppvObject = static_cast<IUnknown*>(canonical);
		AddRef();
		return S_OK;
	}

	// IUnknown / IDXGIObject / IDXGIDeviceSubObject all resolve to the
	// canonical vtable. Each static_cast is unambiguous because there is
	// now only one base class chain.
	if (riid == __uuidof(IUnknown))
	{
		*ppvObject = static_cast<IUnknown*>(canonical);
		AddRef();
		return S_OK;
	}
	if (riid == __uuidof(IDXGIObject))
	{
		*ppvObject = static_cast<IDXGIObject*>(canonical);
		AddRef();
		return S_OK;
	}
	if (riid == __uuidof(IDXGIDeviceSubObject))
	{
		*ppvObject = static_cast<IDXGIDeviceSubObject*>(canonical);
		AddRef();
		return S_OK;
	}

	if (riid == __uuidof(IDXGISwapChain))
	{
		*ppvObject = static_cast<IDXGISwapChain*>(canonical);
		AddRef();
		return S_OK;
	}
	else if (riid == __uuidof(IDXGISwapChain1))
	{
		if (m_pReal1)
		{
			*ppvObject = static_cast<IDXGISwapChain1*>(canonical);
			AddRef();
			return S_OK;
		}
		else
		{
#ifdef SWAPCHAIN_DEBUG
			LOG_DEBUG(L"No interface1");
#endif
			return E_NOINTERFACE;
		}
	}
	else if (riid == __uuidof(IDXGISwapChain2))
	{
		if (m_pReal2)
		{
			*ppvObject = static_cast<IDXGISwapChain2*>(canonical);
			AddRef();
			return S_OK;
		}
		else
		{
#ifdef SWAPCHAIN_DEBUG
			LOG_DEBUG(L"No interface2");
#endif
			return E_NOINTERFACE;
		}
	}
	else if (riid == __uuidof(IDXGISwapChain3))
	{
		if (m_pReal3)
		{
			*ppvObject = static_cast<IDXGISwapChain3*>(canonical);
			AddRef();
			return S_OK;
		}
		else
		{
#ifdef SWAPCHAIN_DEBUG
			LOG_DEBUG(L"No interface3");
#endif
			return E_NOINTERFACE;
		}
	}
	else if (riid == __uuidof(IDXGISwapChain4))
	{
		if (m_pReal4)
		{
			*ppvObject = canonical;
			AddRef();
			return S_OK;
		}
		else
		{
#ifdef SWAPCHAIN_DEBUG
			LOG_DEBUG(L"No interface4");
#endif
			return E_NOINTERFACE;
		}
	}
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"No interface");
#endif

	// Unknown IID - fall through to real swapchain for things we don't
	// implement (debug interfaces, private vendor IIDs, etc.). Do NOT
	// return *ppvObject = the real pointer as one of our own interfaces,
	// because the caller might then call our vtable on the returned
	// object. For foreign IIDs the real object handles itself.
	if (m_pReal == nullptr)
		return E_NOINTERFACE;

	return m_pReal->QueryInterface(riid, ppvObject);
}

HRESULT DxgiWrappedIDXGISwapChain4::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"ResizeBuffers");
#endif

	// Snapshot + AddRef - see Present() for rationale.
	IDXGISwapChain* pSwapChain = m_pReal;
	if (pSwapChain == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;
	pSwapChain->AddRef();

	// Snapshot the clear trigs under m_trigMutex (see Present for rationale).
	decltype(preClearTrig) snapPre;
	decltype(postClearTrig) snapPost;
	{
		std::lock_guard<std::mutex> lk(m_trigMutex);
		snapPre = preClearTrig;
		snapPost = postClearTrig;
	}

	if (snapPre && m_pReal1 != nullptr)
		snapPre(m_pReal1);

	HRESULT ret = pSwapChain->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] ResizeBuffers: failed (error code: " + std::to_wstring(ret) + L")");
	}

	if (snapPost && m_pReal1 != nullptr)
		snapPost(m_pReal1);

	pSwapChain->Release();
	return ret;
}

HRESULT STDMETHODCALLTYPE DxgiWrappedIDXGISwapChain4::GetContainingOutput(IDXGIOutput** ppOutput)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"GetContainingOutput");
#endif

	// Safety check
	if (m_pReal == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	HRESULT ret = m_pReal->GetContainingOutput(ppOutput);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] GetContainingOutput: failed (error code: " + std::to_wstring(ret) + L")");
	}

	return ret;
}

HRESULT DxgiWrappedIDXGISwapChain4::ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags,
	_In_reads_(BufferCount) const UINT* pCreationNodeMask, _In_reads_(BufferCount) IUnknown* const* ppPresentQueue)
{

#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"ResizeBuffers1");
#endif

	// Snapshot + AddRef - see Present() for rationale.
	IDXGISwapChain3* pSwapChain3 = m_pReal3;
	if (pSwapChain3 == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;
	pSwapChain3->AddRef();

	// Snapshot the clear trigs under m_trigMutex (see Present for rationale).
	decltype(preClearTrig) snapPre;
	decltype(postClearTrig) snapPost;
	{
		std::lock_guard<std::mutex> lk(m_trigMutex);
		snapPre = preClearTrig;
		snapPost = postClearTrig;
	}

	if (snapPre && m_pReal1 != nullptr)
		snapPre(m_pReal1);

	HRESULT ret = pSwapChain3->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] ResizeBuffers1: failed (error code: " + std::to_wstring(ret) + L")");
	}

	if (snapPost && m_pReal1 != nullptr)
		snapPost(m_pReal1);

	pSwapChain3->Release();
	return ret;
}

HRESULT DxgiWrappedIDXGISwapChain4::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"SetFullscreenState");
#endif

	// Safety check
	if (m_pReal == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	//if (preClearTrig != nullptr)
		//preClearTrig(this);

	HRESULT ret = m_pReal->SetFullscreenState(Fullscreen, pTarget);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] SetFullscreenState: failed (error code: " + std::to_wstring(ret) + L")");
	}

	//if (postClearTrig != nullptr)
		//postClearTrig(this);

	return ret;
}

HRESULT DxgiWrappedIDXGISwapChain4::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"GetFullscreenState");
#endif

	// Safety check
	if (m_pReal == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	HRESULT ret = m_pReal->GetFullscreenState(pFullscreen, ppTarget);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] GetFullscreenState: failed (error code: " + std::to_wstring(ret) + L")");
	}

	return ret;
}

HRESULT DxgiWrappedIDXGISwapChain4::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"GetBuffer");
#endif

	// Safety check
	if (m_pReal == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	HRESULT ret = m_pReal->GetBuffer(Buffer, riid, ppSurface);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] GetBuffer: failed (error code: " + std::to_wstring(ret) + L")");
	}

	return ret;
}

HRESULT DxgiWrappedIDXGISwapChain4::GetDevice(REFIID riid, void** ppDevice)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"GetDevice");
#endif

	// Safety check
	if (m_pReal == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	HRESULT ret = m_pReal->GetDevice(riid, ppDevice);

	// E_NOINTERFACE from GetDevice is a CONTRACTUAL non-error result. The DXGI
	// runtime, Streamline, NVAPI, and vendor drivers routinely probe a range
	// of IIDs here to discover device capabilities (e.g. IID_IDXGIDevice on
	// a D3D12-backed swapchain, higher ID3D12Device versions than the current
	// driver supports). Each probe that misses returns E_NOINTERFACE and is
	// expected. Logging every one as an error floods the log (observed: tens
	// of thousands of entries per minute on hybrid GPU systems) and buries
	// the interesting errors. Only log real failures.
	if (FAILED(ret) && ret != E_NOINTERFACE) {
		wchar_t iidStr[64] = {};
		StringFromGUID2(riid, iidStr, 64);
		LOG_ERROR(L"[DXGI] GetDevice[" + std::wstring(iidStr) + L"]: failed (error code: 0x"
			+ std::to_wstring(static_cast<unsigned long>(ret)) + L")");
	}

	return ret;
}

#include "Console.h"
HRESULT DxgiWrappedIDXGISwapChain4::Present(UINT SyncInterval, UINT Flags)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"PRESENT!");
#endif

	// Snapshot + AddRef the real swapchain pointer to prevent a concurrent
	// Release() on another thread from freeing it mid-Present. Without the
	// AddRef a race looks like:
	//   T1: enters Present, reads m_pReal into pSwapChain
	//   T2: calls Release(), refcount -> 0, destructor releases m_pReal
	//   T1: dereferences now-freed pSwapChain -> UAF
	// AddRef keeps the real alive for the duration of this call regardless
	// of destructor ordering. Cost: two interlocked ops per frame.
	IDXGISwapChain* pSwapChain = m_pReal;
	if (pSwapChain == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;
	pSwapChain->AddRef();

	// Snapshot the trigs under m_trigMutex, then release the mutex before
	// invoking them. A local std::function copy keeps its captures alive even
	// if DetachTriggers() races to null the originals on another thread.
	decltype(preRenderTrig) snapPre;
	decltype(postRenderTrig) snapPost;
	{
		std::lock_guard<std::mutex> lk(m_trigMutex);
		snapPre = preRenderTrig;
		snapPost = postRenderTrig;
	}

	if (snapPre)
		snapPre(pSwapChain, SyncInterval, Flags);

	HRESULT ret = pSwapChain->Present(SyncInterval, Flags);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] Present: failed (error code: " + std::to_wstring(ret) + L")");
	}

	// Symmetric with pre: pass the SAME IDXGISwapChain* that preRenderTrig saw.
	// Previously post got m_pReal1 upcast which, while pointing at the same COM
	// object, had a different pointer value (different subinterface offset) -
	// that broke callbacks that identify swapchains by raw pointer equality.
	if (snapPost)
		snapPost(pSwapChain, SyncInterval, Flags);

	pSwapChain->Release();
	return ret;
}

HRESULT DxgiWrappedIDXGISwapChain4::Present1(UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"PRESENT1!");
#endif

	// See Present() for rationale on snapshot + AddRef.
	IDXGISwapChain1* pSwapChain1 = m_pReal1;
	if (pSwapChain1 == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;
	pSwapChain1->AddRef();

	// Snapshot the trigs under m_trigMutex (see Present for rationale).
	decltype(preRenderTrig1) snapPre;
	decltype(postRenderTrig1) snapPost;
	{
		std::lock_guard<std::mutex> lk(m_trigMutex);
		snapPre = preRenderTrig1;
		snapPost = postRenderTrig1;
	}

	if (snapPre)
		snapPre(pSwapChain1, SyncInterval, Flags, pPresentParameters);

	HRESULT ret = pSwapChain1->Present1(SyncInterval, Flags, pPresentParameters);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] Present1: failed (error code: " + std::to_wstring(ret) + L")");
	}

	if (snapPost)
		snapPost(pSwapChain1, SyncInterval, Flags, pPresentParameters);

	pSwapChain1->Release();
	return ret;
}

HRESULT STDMETHODCALLTYPE DxgiWrappedIDXGISwapChain4::GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"GetRestrictToOutput");
#endif

	// Safety check
	if (m_pReal1 == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	HRESULT ret = m_pReal1->GetRestrictToOutput(ppRestrictToOutput);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] GetRestrictToOutput: failed (error code: " + std::to_wstring(ret) + L")");
	}

	return ret;
}