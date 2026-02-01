#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <windows.h>
#include <stdint.h>
#include "Common.h"

#ifndef D3D_SHADER_MODEL_6_8
#define D3D_SHADER_MODEL_6_8 0x68
#endif

namespace GPU
{
    using Microsoft::WRL::ComPtr;

    bool GetShaderModelCapabilitiesD3D12(
        ID3D12Device* pDevice,
        uint16_t* major,
        uint16_t* minor,
        bool *isVrsSupported
    ) 
    {
        if (!pDevice || !major || !minor || !isVrsSupported) {
            return false;
        }

        D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData = {};
        shaderModelData.HighestShaderModel = D3D_HIGHEST_SHADER_MODEL;

        if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModelData, sizeof(shaderModelData))) && shaderModelData.HighestShaderModel > D3D_SHADER_MODEL_5_1) {
            *major = 6; *minor = shaderModelData.HighestShaderModel - D3D_SHADER_MODEL_6_0;
        }
        else {
            *major = 5; *minor = 1;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
        if (SUCCEEDED(pDevice->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS6,
            &options6,
            sizeof(options6)))) {
            *isVrsSupported = (options6.VariableShadingRateTier != D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED);
        }
        else {
            *isVrsSupported = false;
        }

        return true;
    }

    // Returns true on success and fills pTotalBytes/pFreeBytes. No hard link to DXGI.
    bool TryGetCpuVisibleVidmemD3D12(
        ID3D12Device* pDevice,
        uint64_t* pTotalBytes,
        uint64_t* pFreeBytes)
    {
        if (!pDevice || !pTotalBytes || !pFreeBytes)
            return false;

        // Resolve CreateDXGIFactory2 dynamically to avoid link-time dependency on dxgi.lib
        HMODULE hDxgi = GetModuleHandleW(L"dxgi.dll");
        if (!hDxgi) {
            hDxgi = LoadLibraryW(L"dxgi.dll"); // load if not already loaded
            if (!hDxgi)
                return false;
        }

        using PFN_CreateDXGIFactory2 = HRESULT(WINAPI*)(UINT, REFIID, void**);
        auto pCreateDXGIFactory2 = reinterpret_cast<PFN_CreateDXGIFactory2>(
            GetProcAddress(hDxgi, "CreateDXGIFactory2"));
        if (!pCreateDXGIFactory2)
            return false;

        // Query UMA/Non-UMA for segment choice
        D3D12_FEATURE_DATA_ARCHITECTURE1 arch1 = {};
        const bool hasArch1 = SUCCEEDED(pDevice->CheckFeatureSupport(
            D3D12_FEATURE_ARCHITECTURE1, &arch1, sizeof(arch1)));

        // Create factory via function pointer
        ComPtr<IDXGIFactory4> factory4;
        if (FAILED(pCreateDXGIFactory2(0, IID_PPV_ARGS(&factory4))))
            return false;

        // Get adapter by LUID
        LUID luid = pDevice->GetAdapterLuid();

        ComPtr<IDXGIAdapter> baseAdapter;
        {
            // Prefer IDXGIFactory6::EnumAdapterByLuid if available
            ComPtr<IDXGIFactory6> factory6;
            if (SUCCEEDED(factory4.As(&factory6))) {
                if (FAILED(factory6->EnumAdapterByLuid(luid, IID_PPV_ARGS(&baseAdapter)))) {
                    // Fallback: linear scan of adapters until LUID matches
                    ComPtr<IDXGIFactory1> factory1;
                    if (FAILED(factory4.As(&factory1)))
                        return false;

                    for (UINT idx = 0;; ++idx) {
                        ComPtr<IDXGIAdapter1> cand;
                        if (factory1->EnumAdapters1(idx, &cand) == DXGI_ERROR_NOT_FOUND)
                            break;

                        DXGI_ADAPTER_DESC1 d = {};
                        if (SUCCEEDED(cand->GetDesc1(&d)) && d.AdapterLuid.HighPart == luid.HighPart &&
                            d.AdapterLuid.LowPart == luid.LowPart) {
                            cand.As(&baseAdapter);
                            break;
                        }
                    }
                    if (!baseAdapter)
                        return false;
                }
            }
            else {
                // Only the scan path is possible
                ComPtr<IDXGIFactory1> factory1;
                if (FAILED(factory4.As(&factory1)))
                    return false;

                for (UINT idx = 0;; ++idx) {
                    ComPtr<IDXGIAdapter1> cand;
                    if (factory1->EnumAdapters1(idx, &cand) == DXGI_ERROR_NOT_FOUND)
                        break;

                    DXGI_ADAPTER_DESC1 d = {};
                    if (SUCCEEDED(cand->GetDesc1(&d)) && d.AdapterLuid.HighPart == luid.HighPart &&
                        d.AdapterLuid.LowPart == luid.LowPart) {
                        cand.As(&baseAdapter);
                        break;
                    }
                }
                if (!baseAdapter)
                    return false;
            }
        }

        // Use IDXGIAdapter3::QueryVideoMemoryInfo if available
        ComPtr<IDXGIAdapter3> adapter3;
        if (SUCCEEDED(baseAdapter.As(&adapter3))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
            const DXGI_MEMORY_SEGMENT_GROUP seg =
                (hasArch1 && arch1.UMA) ? DXGI_MEMORY_SEGMENT_GROUP_LOCAL
                : DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL;

            if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0 /*NodeIndex*/, seg, &info))) {
                *pTotalBytes = info.Budget;
                *pFreeBytes = (info.Budget > info.CurrentUsage)
                    ? (info.Budget - info.CurrentUsage)
                    : 0ull;

                // Optional: tighten to GPU Upload Heaps if supported (ReBAR-aware).
                // D3D12_FEATURE_DATA_GPU_UPLOAD_HEAP_SUPPORT up = {};
                // if (SUCCEEDED(pDevice->CheckFeatureSupport(
                //         (D3D12_FEATURE)D3D12_FEATURE_GPU_UPLOAD_HEAP_SUPPORT, &up, sizeof(up))) &&
                //     up.Supported && up.MaxGPUUploadHeapSizeInBytes > 0)
                // {
                //     if (up.MaxGPUUploadHeapSizeInBytes < *pTotalBytes)
                //         *pTotalBytes = up.MaxGPUUploadHeapSizeInBytes;
                //     // pFreeBytes could be refined using your own accounting of GPU_UPLOAD allocations.
                // }

                return true;
            }
        }

        // Fallback: SharedSystemMemory + GlobalMemoryStatusEx (coarse but better-than-nothing)
        ComPtr<IDXGIAdapter1> adapter1;
        if (SUCCEEDED(baseAdapter.As(&adapter1))) {
            DXGI_ADAPTER_DESC1 desc = {};
            if (SUCCEEDED(adapter1->GetDesc1(&desc))) {
                MEMORYSTATUSEX ms = { sizeof(ms) };
                if (GlobalMemoryStatusEx(&ms)) {
                    const uint64_t total = desc.SharedSystemMemory;
                    const uint64_t freePhys = ms.ullAvailPhys;
                    *pTotalBytes = total;
                    *pFreeBytes = (freePhys < total) ? freePhys : total;
                    return true;
                }
            }
        }

        return false;
    }

    bool GetPreferredGPU(DXGI_ADAPTER_DESC1 *selectedAdapterDesc)
    {
        typedef HRESULT(WINAPI* PFN_CREATEDXGIFACTORY1)(REFIID, void**);
        *selectedAdapterDesc = {};

        HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
        if (!dxgi) {
            return false;
        }

        // Get the address of CreateDXGIFactory
        PFN_CREATEDXGIFACTORY1 pfnCreateDXGIFactory1 = (PFN_CREATEDXGIFACTORY1)GetProcAddress(dxgi, "CreateDXGIFactory1");
        if (!pfnCreateDXGIFactory1) {
            return false;
        }

        IDXGIFactory6* pFactory = nullptr;
        if (FAILED(pfnCreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&pFactory))) {
            return false;
        }

        IDXGIAdapter4* pAdapter = nullptr;

        if (SUCCEEDED(pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter4), (void**)&pAdapter))) {
            if (FAILED(pAdapter->GetDesc1(selectedAdapterDesc))) {
                pAdapter->Release();
                pFactory->Release();
                return false;
            }
            pAdapter->Release();
        }

        pFactory->Release();
        return true;
    }

    bool ListGPUs()
    {
        static bool isListed = false;

        if (isListed) {
            return true;
        }

        isListed = true;
        bool isDxgiLoaded = false;
        // Load DXGI library
        LOG_DEBUG(L"Listing available GPUs");
        HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
        if (!dxgi) {
            if (Common::IsPluginPresent(L"dxgi.dll")) {
                dxgi = Common::LoadPlugin(L"dxgi.dll");
            }

            if (!dxgi) {
                dxgi = LoadLibraryW(L"dxgi.dll");
            }

            isDxgiLoaded = true;
        }

        if (!dxgi) {
            return false;
        }

        // Function pointers for DXGI functions
        typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY)(REFIID, void**);
        PFN_CREATE_DXGI_FACTORY pfnCreateDXGIFactory = nullptr;

        // Get function pointers
        pfnCreateDXGIFactory = reinterpret_cast<PFN_CREATE_DXGI_FACTORY>(
            GetProcAddress(dxgi, "CreateDXGIFactory1")
            );


        if (!pfnCreateDXGIFactory) {
            LOG_ERROR(L"Failed to get function pointer for CreateDXGIFactory1");

            if (isDxgiLoaded) {
                FreeLibrary(dxgi);
            }
            return false;
        }

        // Create DXGI factory
        IDXGIFactory6* pFactory = nullptr;
        HRESULT hr = pfnCreateDXGIFactory(__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&pFactory));
        if (FAILED(hr)) {
            LOG_ERROR(L"Failed to call CreateDXGIFactory1 (code: " + std::to_wstring(hr) + L")");

            if (isDxgiLoaded) {
                FreeLibrary(dxgi);
            }
            return false;
        }

        // Enumerate adapters
        IDXGIAdapter1* pAdapter = nullptr;
        LOG_INFO(L"Following Display Adapters detected:");
        int j = 1;
        for (UINT i = 0; pFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(pAdapter), reinterpret_cast<void**>(&pAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                LOG_INFO(L"   Adapter #" + std::to_wstring(j) + L": " + std::wstring(desc.Description) + L" (VRAM: " + std::to_wstring((int)((desc.DedicatedVideoMemory + desc.DedicatedSystemMemory) / (1024 * 1024))) + L"MB)");
                j++;
            }
        }

        // Release resources
        if (pAdapter) {
            pAdapter->Release();
        }
        pFactory->Release();
        if (isDxgiLoaded) {
            FreeLibrary(dxgi);
        }

        return true;
    }

    // Function to get the refresh rate of the display attached to a GPU by LUID
    int GetRefreshRateByLUID(LUID gpuLUID)
    {
        typedef HRESULT(WINAPI* PFN_CREATEDXGIFACTORY1)(REFIID, void**);

        HMODULE hDxgi = GetModuleHandle(L"dxgi.dll");
        if (!hDxgi) {
            LOG_ERROR(L"DXGI not loaded");
            return false;
        }

        // Get the address of CreateDXGIFactory
        PFN_CREATEDXGIFACTORY1 pfnCreateDXGIFactory1 = (PFN_CREATEDXGIFACTORY1)GetProcAddress(hDxgi, "CreateDXGIFactory1");
        if (!pfnCreateDXGIFactory1) {
            LOG_ERROR(L"failed to get address of DXGI Factory");
            return false;
        }

        IDXGIFactory4* dxgiFactory;

        HRESULT hr = pfnCreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr)) {
            //std::cerr << "Failed to create DXGI Factory." << std::endl;
            return -1;
        }

        // Iterate through adapters to find the one with matching LUID
        IDXGIAdapter1* adapter;
        for (UINT adapterIndex = 0; dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (memcmp(&desc.AdapterLuid, &gpuLUID, sizeof(LUID)) == 0) {
                // Found the adapter, now get the output (display)
                IDXGIOutput* output;
                if (adapter->EnumOutputs(0, &output) == DXGI_ERROR_NOT_FOUND) {
                    //std::cerr << "No outputs found for the adapter." << std::endl;
                    adapter->Release();
                    dxgiFactory->Release();
                    return -1;
                }

                // Get the number of display modes
                UINT numModes = 0;
                output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, nullptr);

                // Allocate memory for display modes
                DXGI_MODE_DESC* displayModes = new DXGI_MODE_DESC[numModes];
                output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModes);

                // Iterate through display modes to find the one with current resolution
                for (UINT i = 0; i < numModes; ++i) {
                    if (displayModes[i].Width == GetSystemMetrics(SM_CXSCREEN) &&
                        displayModes[i].Height == GetSystemMetrics(SM_CYSCREEN)) {
                        // Calculate refresh rate as an integer
                        int refreshRate = static_cast<int>(displayModes[i].RefreshRate.Numerator) / static_cast<int>(displayModes[i].RefreshRate.Denominator);

                        // Clean up resources
                        delete[] displayModes;
                        output->Release();
                        adapter->Release();
                        dxgiFactory->Release();

                        return refreshRate;
                    }
                }

                // Clean up resources
                delete[] displayModes;
                output->Release();
                adapter->Release();
                dxgiFactory->Release();

                // If no matching display mode found, return -1
                return -1;
            }

            adapter->Release();
        }

        // No adapter with matching LUID found
        //std::cerr << "No GPU with the specified LUID found." << std::endl;
        dxgiFactory->Release();
        return -1;
    }

}