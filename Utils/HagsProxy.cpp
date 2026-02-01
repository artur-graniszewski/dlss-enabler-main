#pragma once
#include <Shlwapi.h>
#include <d3dkmthk.h>
#include <dxgi.h>
#include "Common.h"
#include "../Core/Context.h"
#include "HagsProxy.h"   

NTSTATUS WINAPI MyD3DKMTEnumAdapters2(D3DKMT_ENUMADAPTERS2* pAdapterInfo)
{
	// a nasty haxx for streamline, the default number returned by D3DKMTEnumAdapters2 is probably 8
	// we return 7 to seed out the streamline (which always expects 8 and does not ask for an actual number in advance)
	// from the other calls that follow the actual MS documentation
	if (pAdapterInfo->NumAdapters == 0 || pAdapterInfo->pAdapters == NULL) {
		pAdapterInfo->NumAdapters = 7;
		LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: called with NumAdapters = 0");
		return 0;
	}

	if (pAdapterInfo->NumAdapters != 8) {
		// this will work for non-linux load, but what about WINE? Should I return not implemented error instead?
		LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: proxied");
		return gOrigEnumAdapters2(pAdapterInfo);
	}

	// Check if pAdapterInfo is valid
	if (pAdapterInfo == nullptr)
		return 0xC0000001; // STATUS_INVALID_PARAMETER

	LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: called with NumAdapters = 8 (Streamline?)");

	// Create DXGI factory
	IDXGIFactory* pFactory;

	typedef HRESULT(WINAPI* PFN_CREATEDXGIFACTORY)(REFIID, void**);

	HMODULE hDxgi = GetModuleHandle(L"dxgi.dll");
	if (!hDxgi) {
		LOG_ERROR(L"[NVAPI] DXGI not loaded");
		return false;
	}

	// Get the address of CreateDXGIFactory
	PFN_CREATEDXGIFACTORY pfnCreateDXGIFactory = (PFN_CREATEDXGIFACTORY)GetProcAddress(hDxgi, "CreateDXGIFactory");
	if (!pfnCreateDXGIFactory) {
		LOG_ERROR(L"[NVAPI] Failed to get address of DXGI Factory");
		return false;
	}

	LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: calling CreateDXGIFactory");
	HRESULT hr = pfnCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);
	LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: CreateDXGIFactory called");
	if (FAILED(hr))
		return 0xC0000008; // STATUS_UNSUCCESSFUL

	LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: calling EnumAdapters");
	// Enumerate adapters
	UINT i = 0;
	IDXGIAdapter* pAdapter = nullptr;
	while (pFactory->EnumAdapters(i++, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
		// Check if there's enough space to store adapter information
		if (i > pAdapterInfo->NumAdapters) {
			pFactory->Release();
			LOG_ERROR(L"[HAGS] D3DKMTEnumAdapters2: Buffer too small: " + std::to_wstring(pAdapterInfo->NumAdapters));
			return 0xC0000023; // STATUS_BUFFER_TOO_SMALL
		}

		// Get adapter description
		DXGI_ADAPTER_DESC adapterDesc;
		pAdapter->GetDesc(&adapterDesc);

		// Fill adapter information directly into D3DKMT_ADAPTERINFO structure
		pAdapterInfo->pAdapters[i - 1].AdapterLuid = adapterDesc.AdapterLuid;
		pAdapterInfo->pAdapters[i - 1].bPrecisePresentRegionsPreferred = false;
		pAdapterInfo->pAdapters[i - 1].NumOfSources = 1;
		pAdapterInfo->pAdapters[i - 1].hAdapter = 12121;
		LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: detected " + std::wstring(adapterDesc.Description));

		pAdapter->Release();
	}

	// Set the count of adapters
	pAdapterInfo->NumAdapters = i - 1;

	// Release DXGI factory
	pFactory->Release();
	LOG_DEBUG(L"[HAGS] D3DKMTEnumAdapters2: successfull");
	ctx.streamline.isPresent = true;
	ctx.streamline.spoofNextCreateFactoryCall = true;

	return 0x00000000; // STATUS_SUCCESS
}

NTSTATUS WINAPI MyD3DKMTQueryAdapterInfo(D3DKMT_QUERYADAPTERINFO* pInfo)
{
	unsigned int result;
#ifdef PROCLOAD_DEBUG
	LOG_WARNING(L">>> [QUERY] " + std::to_wstring(pInfo->Type));
#endif
	if (pInfo->Type == KMTQAITYPE_WDDM_2_7_CAPS || pInfo->Type == KMTQAITYPE_WDDM_2_9_CAPS) {
		LOG_INFO(L"[INIT] Hardware Accelerated GPU Scheduling is being checked");
		if (ctx.emulation.isHagsSpoofed) {
			LOG_INFO(L"[INIT] Hardware Accelerated GPU Scheduling will be " + std::wstring(ctx.gpu.isHagsEnabled ? L"enabled" : L"disabled"));
		}
		result = 0;
	}
	else {
		result = gOrigQueryAdapterInfo(pInfo);
	}
	//return result;
	std::wstring adapterStr = std::to_wstring(pInfo->hAdapter);
	bool hagsEnabled = false;
	bool hagsSupported = false;
	static bool hagsDetected = false;
	if (pInfo->Type == KMTQAITYPE_WDDM_2_7_CAPS) {
		D3DKMT_WDDM_2_7_CAPS* pCaps = static_cast<D3DKMT_WDDM_2_7_CAPS*>(pInfo->pPrivateDriverData);
		//hagsEnabled = (pCaps->HwSchEnabled != 0);
		//hagsSupported = (pCaps->HwSchSupported != 0);
		hagsDetected = true;
		if (ctx.emulation.isHagsSpoofed) {
			pCaps->HwSchEnabled = (ctx.gpu.isHagsEnabled ? 1 : 0);
		}
	}
	else if (pInfo->Type == KMTQAITYPE_WDDM_2_9_CAPS) {
		D3DKMT_WDDM_2_9_CAPS* pCaps = static_cast<D3DKMT_WDDM_2_9_CAPS*>(pInfo->pPrivateDriverData);
		//hagsEnabled = (pCaps->HwSchEnabled != 0);
		//hagsSupported = (pCaps->HwSchSupportState != 0);
		hagsDetected = true;
		if (ctx.emulation.isHagsSpoofed) {
			pCaps->HwSchEnabled = (ctx.gpu.isHagsEnabled ? 1 : 0);
		}
	}
	else if (pInfo && pInfo->Type == KMTQAITYPE_UMDRIVERPRIVATE) {
		struct NV_D3DKMT_PRIVATE_DRIVER_DATA // nvwg2umx.dll (546.33)
		{
			uint32_t Header;				 // 0 NVDA
			char Padding[0xE4];				 // 4
			uint32_t Architecture;			 // E8
		};

		if (pInfo->pPrivateDriverData && pInfo->PrivateDriverDataSize >= sizeof(NV_D3DKMT_PRIVATE_DRIVER_DATA)) {
			auto driverData = static_cast<NV_D3DKMT_PRIVATE_DRIVER_DATA*>(pInfo->pPrivateDriverData);

			if (driverData->Header == 0x4E564441) {
				static bool isPrivDataUsed = false;
				if (ctx.logging.isExtraDebugEnabled && !isPrivDataUsed) {
					isPrivDataUsed = true;
					LOG_INFO(L"[INIT] Private driver data is being checked");
				}
				driverData->Architecture = ctx.emulation.forceHighestArch ? ctx.currentGpuArchitecture : ctx.realGpuArchitecture;
			}
		}
	}

	if (hagsDetected) {
		if (hagsEnabled) {
			//LOG_INFO(L"[HAGS] Hardware Accelerated GPU Scheduling is enabled");
		}
		else {
			//Console::Warning(L"[HAGS] Hardware Accelerated GPU Scheduling is disabled");
		}

		if (ctx.emulation.isHagsSpoofed && hagsDetected) {
			hagsEnabled = true;// @fixme: its just a workaround for Linux/Wine where we cannot tell if HAGS is enabled or not
			if (ctx.gpu.isHagsEnabled && !hagsEnabled) {
				LOG_INFO(L"[INIT] Enabling Hardware Accelerated GPU Scheduling");
			}
			else if (!ctx.gpu.isHagsEnabled && hagsEnabled) {
				LOG_INFO(L"[INIT] Disabling Hardware Accelerated GPU Scheduling");
			}
		}
	}

	return result;
}
