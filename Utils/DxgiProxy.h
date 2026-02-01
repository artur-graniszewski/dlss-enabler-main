#pragma once
#include <wtypes.h>
#include <dxgi.h>

namespace DXGI
{
	typedef HRESULT(WINAPI* CreateDXGIFactory_t)(REFIID riid, void** ppFactory);
	typedef HRESULT(WINAPI* CreateDXGIFactory1_t)(REFIID riid, void** ppFactory);
	typedef HRESULT(WINAPI* CreateDXGIFactory2_t)(UINT Flags, REFIID riid, void** ppFactory);

	void DlssEnablerLoggerSet(void* callback);
	UINT BundledDxgiInit(UINT newVendorId, UINT newDeviceId, const char* newModelName, SIZE_T newDedicatedVideoMemory);
	void InitDxgi(HMODULE dxgi);
	extern "C" __declspec(dllexport) void DlssEnablerLogger(const char* message, unsigned int loggingLevel, const char* sourceComponent);

	HRESULT WINAPI CreateDXGIFactory(REFIID riid, _COM_Outptr_ void** ppFactory);
	HRESULT WINAPI CreateDXGIFactory1(REFIID riid, _COM_Outptr_ void** ppFactory);
	HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, _COM_Outptr_ void** ppFactory);
}