#pragma once
#include <dxgi1_6.h>
#include <functional>
#include "Common.h"
#include "../Core/Context.h"

#define SWAPCHAIN_DEBUGA

#include <Unknwn.h>  // For IUnknown and REFIID

// Define the custom interface with a UUID using __interface or struct
__interface __declspec(uuid("12345678-1234-1234-1234-1234567890AB"))
IDXGISwapChain4Interface : public IUnknown
{

};

class DxgiWrappedIDXGISwapChain4 : public IDXGISwapChain4, public IDXGISwapChain4Interface
{
	IDXGISwapChain* m_pReal = nullptr;
	IDXGISwapChain1* m_pReal1 = nullptr;
	IDXGISwapChain2* m_pReal2 = nullptr;
	IDXGISwapChain3* m_pReal3 = nullptr;
	IDXGISwapChain4* m_pReal4 = nullptr;
	bool isShuttingDown = false;

	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> preRenderTrig = nullptr;
	std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> postRenderTrig = nullptr;
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> preRenderTrig1 = nullptr;
	std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> postRenderTrig1 = nullptr;
	std::function<void(IDXGISwapChain*)> preClearTrig = nullptr;
	std::function<void(IDXGISwapChain*)> postClearTrig = nullptr;

	volatile LONG m_iRefcount;

public:

	DxgiWrappedIDXGISwapChain4(IDXGISwapChain* real,
		std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> preRenderTrig,
		std::function<HRESULT(IDXGISwapChain*, UINT&, UINT&)> postRenderTrig,
		std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> preRenderTrig1,
		std::function<HRESULT(IDXGISwapChain1*, UINT&, UINT&, const DXGI_PRESENT_PARAMETERS*)> postRenderTrig1,
		std::function<void(IDXGISwapChain*)> preClearTrig,
		std::function<void(IDXGISwapChain*)> postClearTrig);

	virtual ~DxgiWrappedIDXGISwapChain4();

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
			//
			//std::this_thread::sleep_for(std::chrono::seconds(1));
			//if (ClearTrig != nullptr && hwnd == ImGuiOverlayBase::Handle())
				//ClearTrig(true);

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
		
		HRESULT ret = m_pReal->GetPrivateData(Name, pDataSize, pData);
		static const GUID IID_IFfxAntiLag2Data = { 0x5083ae5b, 0x8070, 0x4fca, {0x8e, 0xe5, 0x35, 0x82, 0xdd, 0x36, 0x7d, 0x13} };
		if (Name == IID_IFfxAntiLag2Data) {
			ctx.reflex.optiFgCycle++;
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
		
		HRESULT ret = m_pReal->GetParent(riid, ppParent);

		if (ret != S_OK) {
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
		
		BOOL ret = m_pReal1->IsTemporaryMonoSupported();

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput);

	virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* pColor)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetBackgroundColor");
#endif
		
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
		return m_pReal2->GetFrameLatencyWaitableObject();
	}

	virtual HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"SetMatrixTransform");
#endif
		
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
		
		HRESULT ret = m_pReal3->GetCurrentBackBufferIndex();

		if (ret < 0 || ret > 3) {
			LOG_ERROR(L"GetCurrentBackBufferIndex: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}

	virtual HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport)
	{
#ifdef SWAPCHAIN_DEBUG
		LOG_DEBUG(L"CheckColorSpaceSupport");
#endif
		
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
		
		HRESULT ret = m_pReal3->SetColorSpace1(ColorSpace);

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
		
		HRESULT ret = m_pReal4->SetHDRMetaData(Type, Size, pMetaData);

		if (ret != S_OK) {
			LOG_ERROR(L"SetHDRMetaData: failed (error code: " + std::to_wstring(ret) + L")");
		}

		return ret;
	}
};