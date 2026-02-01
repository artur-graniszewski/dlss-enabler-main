#include "Wrapped_Swapchain.h"
#include "Common.h"
#include <d3d12.h>

DxgiWrappedIDXGISwapChain4::DxgiWrappedIDXGISwapChain4(IDXGISwapChain* real,
	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> PreRenderTrig,
	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> PostRenderTrig,
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> PreRenderTrig1,
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> PostRenderTrig1,
	std::function<void(IDXGISwapChain*)> preClearTrig,
	std::function<void(IDXGISwapChain*)> postClearTrig) : m_pReal(real), preRenderTrig(PreRenderTrig), postRenderTrig(PostRenderTrig),
	preRenderTrig1(PreRenderTrig1), postRenderTrig1(PostRenderTrig1), preClearTrig(preClearTrig), postClearTrig(postClearTrig), m_iRefcount(1)
{
	real->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&m_pReal1);
	real->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&m_pReal2);
	real->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&m_pReal3);
	real->QueryInterface(__uuidof(IDXGISwapChain4), (void**)&m_pReal4);
}

DxgiWrappedIDXGISwapChain4::~DxgiWrappedIDXGISwapChain4()
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"Release");
#endif
	;
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

HRESULT STDMETHODCALLTYPE DxgiWrappedIDXGISwapChain4::QueryInterface(REFIID riid, void** ppvObject)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"QueryInterface");
#endif
	if (!ppvObject)
		return E_POINTER;

	*ppvObject = nullptr;

	if (riid == __uuidof(IDXGISwapChain4Interface))
	{
		//*ppvObject = (IDXGISwapChain*)this;
		LOG_DEBUG(L"QueryInterface: proxy detected");
		*ppvObject = static_cast<IDXGISwapChain4Interface*>(this);
		AddRef();
		return S_OK;
	}

	if (
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject) ||
		riid == __uuidof(IDXGIDeviceSubObject))
	{
		*ppvObject = static_cast<IDXGISwapChain4*>(this);
		AddRef();
		return S_OK;
	}

	if (riid == __uuidof(IDXGISwapChain))
	{
		AddRef();
		*ppvObject = (IDXGISwapChain*)this;
		return S_OK;
	}
	else if (riid == __uuidof(IDXGISwapChain1))
	{
		if (m_pReal1)
		{
			AddRef();
			*ppvObject = (IDXGISwapChain1*)this;
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
			AddRef();
			*ppvObject = (IDXGISwapChain2*)this;
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
			AddRef();
			*ppvObject = (IDXGISwapChain3*)this;
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
			AddRef();
			*ppvObject = (IDXGISwapChain4*)this;
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

	// Safety check before forwarding
	if (m_pReal == nullptr)
		return E_NOINTERFACE;

	return m_pReal->QueryInterface(riid, ppvObject);
}

HRESULT DxgiWrappedIDXGISwapChain4::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"ResizeBuffers");
#endif

	// Safety check - SwapChain may have been released
	IDXGISwapChain* pSwapChain = m_pReal;
	if (pSwapChain == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	if (preClearTrig != nullptr && m_pReal1 != nullptr)
		preClearTrig(m_pReal1);

	HRESULT ret = pSwapChain->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] ResizeBuffers: failed (error code: " + std::to_wstring(ret) + L")");
	}

	if (postClearTrig != nullptr && m_pReal1 != nullptr)
		postClearTrig(m_pReal1);

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

	// Safety check - SwapChain3 may have been released
	IDXGISwapChain3* pSwapChain3 = m_pReal3;
	if (pSwapChain3 == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	if (preClearTrig != nullptr && m_pReal1 != nullptr)
		preClearTrig(m_pReal1);

	HRESULT ret = pSwapChain3->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] ResizeBuffers1: failed (error code: " + std::to_wstring(ret) + L")");
	}

	if (postClearTrig != nullptr && m_pReal1 != nullptr)
		postClearTrig(m_pReal1);

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

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] GetDevice: failed (error code: " + std::to_wstring(ret) + L")");
	}

	return ret;
}

#include "Console.h"
HRESULT DxgiWrappedIDXGISwapChain4::Present(UINT SyncInterval, UINT Flags)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"PRESENT!");
#endif

	// CRITICAL: Capture pointer locally to prevent race condition during shutdown
	// The SwapChain may be released by another thread between the check and the call
	IDXGISwapChain* pSwapChain = m_pReal;
	if (pSwapChain == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	if (preRenderTrig != nullptr)
		preRenderTrig(pSwapChain, SyncInterval, Flags);

	// Re-check after callback - SwapChain might have been released during preRenderTrig
	if (m_pReal == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	HRESULT ret = pSwapChain->Present(SyncInterval, Flags);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] Present: failed (error code: " + std::to_wstring(ret) + L")");
	}

	// Use m_pReal1 for post callback, check it's still valid
	IDXGISwapChain1* pSwapChain1 = m_pReal1;
	if (postRenderTrig != nullptr && pSwapChain1 != nullptr)
		postRenderTrig(pSwapChain1, SyncInterval, Flags);

	return ret;
}

HRESULT DxgiWrappedIDXGISwapChain4::Present1(UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
#ifdef SWAPCHAIN_DEBUG
	LOG_DEBUG(L"PRESENT1!");
#endif

	// CRITICAL: Capture pointer locally to prevent race condition during shutdown
	IDXGISwapChain1* pSwapChain1 = m_pReal1;
	if (pSwapChain1 == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	if (preRenderTrig1 != nullptr)
		preRenderTrig1(pSwapChain1, SyncInterval, Flags, pPresentParameters);

	// Re-check after callback
	if (m_pReal1 == nullptr)
		return DXGI_ERROR_DEVICE_REMOVED;

	HRESULT ret = pSwapChain1->Present1(SyncInterval, Flags, pPresentParameters);

	if (ret != S_OK) {
		LOG_ERROR(L"[DXGI] Present1: failed (error code: " + std::to_wstring(ret) + L")");
	}

	// Re-capture for post callback
	pSwapChain1 = m_pReal1;
	if (postRenderTrig1 != nullptr && pSwapChain1 != nullptr)
		postRenderTrig1(pSwapChain1, SyncInterval, Flags, pPresentParameters);

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
