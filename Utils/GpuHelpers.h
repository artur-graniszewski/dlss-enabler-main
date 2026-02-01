#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <windows.h>
#include <stdint.h>

namespace GPU
{
    using Microsoft::WRL::ComPtr;
    
    bool GetPreferredGPU(DXGI_ADAPTER_DESC1* selectedAdapterDesc);

    bool GetShaderModelCapabilitiesD3D12(
        ID3D12Device* pDevice,
        uint16_t* major,
        uint16_t* minor,
        bool* isVrsSupported
    );

    // Returns true on success and fills pTotalBytes/pFreeBytes. No hard link to DXGI.
    bool TryGetCpuVisibleVidmemD3D12(
        ID3D12Device* pDevice,
        uint64_t* pTotalBytes,
        uint64_t* pFreeBytes);

    bool ListGPUs();

    // Function to get the refresh rate of the display attached to a GPU by LUID
    int GetRefreshRateByLUID(LUID gpuLUID);
}