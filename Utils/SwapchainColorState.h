#pragma once
// =============================================================================
// SwapchainColorState.h - per-swapchain colour space state via DXGI private data
// =============================================================================
//
// WHY THIS EXISTS
// ---------------
// DXGI has SetColorSpace1() but NO GetColorSpace1(). The overlay needs to know
// which transfer function the game is writing into the back buffer (sRGB /
// scRGB linear / HDR10 PQ) in order to convert its own SDR-authored colours.
// The back buffer FORMAT is readable at any time (GetDesc1), the COLOUR SPACE
// is not - so we snapshot it when the game sets it.
//
// WHY PRIVATE DATA AND NOT A GLOBAL MAP
// -------------------------------------
// A side map keyed by swapchain pointer has two failure modes this avoids:
//   1. Lifetime: entries outlive the object; a recycled allocation address then
//      resolves to a stale colour space.
//   2. COM identity: Present() dispatches its trigger with IDXGISwapChain* and
//      Present1() with IDXGISwapChain1* - DIFFERENT pointer values for the SAME
//      object (different subinterface offsets). A raw-pointer map written from
//      one and read from the other silently misses.
// DXGI private data is stored per COM object and keyed by GUID, so both of the
// above are handled by the runtime. Storage lifetime == swapchain lifetime.
//
// Use SetPrivateData (plain byte blob), NEVER SetPrivateDataInterface - the
// latter takes a reference on what it stores and we do not want another
// refcount holder anywhere near swapchain teardown.
//
// WRITE through the REAL swapchain object (m_pReal) from the wrapper, so that
// readers holding any subinterface of that object can find it.
// =============================================================================

#include <dxgi1_6.h>
#include <atomic>
#include <cstdint>

namespace SwapchainColorState
{
    // {C7A3F1E4-9B2D-4F86-A5C1-3E0D7B49F2A6}
    static const GUID kPrivateDataGuid =
    { 0xc7a3f1e4, 0x9b2d, 0x4f86, { 0xa5, 0xc1, 0x3e, 0x0d, 0x7b, 0x49, 0xf2, 0xa6 } };

    static const uint32_t kMagic = 0x53434544;  // 'DECS'
    static const uint32_t kVersion = 1;

    enum BlobFlags : uint32_t
    {
        FLAG_COLOR_SPACE_SET = 1u << 0,
        FLAG_META_SET = 1u << 1,
    };

    // POD only - DXGI copies these bytes verbatim. No pointers, no members with
    // non-trivial layout. magic/version guard against a layered component
    // (Streamline, OptiScaler) or an older build reading a different layout.
    struct Blob
    {
        uint32_t                    magic;
        uint32_t                    version;
        uint32_t                    colorSpace;   // DXGI_COLOR_SPACE_TYPE
        uint32_t                    flags;
        DXGI_HDR_METADATA_HDR10     meta;
    };

    // Bumped on every successful write. Lets a consumer that has already cached
    // a resolved state notice a LATE SetColorSpace1() - which happens routinely,
    // because games often set the colour space after the first Present, i.e.
    // after the overlay has already initialised.
    //
    // Function-local static (not an inline variable) so this header stays
    // compatible with pre-C++17 builds.
    inline std::atomic<uint32_t>& Revision()
    {
        static std::atomic<uint32_t> rev(0);
        return rev;
    }

    // Returns false when nothing was stored yet, or when what is stored is not
    // ours / not this version. Callers must treat false as "unknown", NOT as
    // "SDR" - the decision of what to do with an unknown belongs to the caller.
    inline bool Load(IDXGIObject* pObject, Blob* pOut)
    {
        if (pObject == nullptr || pOut == nullptr)
            return false;

        Blob blob = {};
        UINT size = static_cast<UINT>(sizeof(blob));

        if (FAILED(pObject->GetPrivateData(kPrivateDataGuid, &size, &blob)))
            return false;

        if (size != sizeof(blob) || blob.magic != kMagic || blob.version != kVersion)
            return false;

        *pOut = blob;
        return true;
    }

    // Read-modify-write: storing a colour space must not wipe previously stored
    // HDR metadata and vice versa.
    inline void StoreColorSpace(IDXGIObject* pObject, DXGI_COLOR_SPACE_TYPE colorSpace)
    {
        if (pObject == nullptr)
            return;

        Blob blob = {};
        if (!Load(pObject, &blob))
        {
            blob = Blob();
            blob.magic = kMagic;
            blob.version = kVersion;
        }

        blob.colorSpace = static_cast<uint32_t>(colorSpace);
        blob.flags |= FLAG_COLOR_SPACE_SET;

        if (SUCCEEDED(pObject->SetPrivateData(kPrivateDataGuid, static_cast<UINT>(sizeof(blob)), &blob)))
            Revision().fetch_add(1, std::memory_order_release);
    }

    // Only HDR10 metadata is stored; other metadata types carry no luminance we
    // can use. Size is validated because the caller hands us a raw void*.
    inline void StoreHdrMetaData(IDXGIObject* pObject, DXGI_HDR_METADATA_TYPE type, UINT size, const void* pMetaData)
    {
        if (pObject == nullptr || pMetaData == nullptr)
            return;

        if (type != DXGI_HDR_METADATA_TYPE_HDR10 || size < sizeof(DXGI_HDR_METADATA_HDR10))
            return;

        Blob blob = {};
        if (!Load(pObject, &blob))
        {
            blob = Blob();
            blob.magic = kMagic;
            blob.version = kVersion;
        }

        memcpy(&blob.meta, pMetaData, sizeof(DXGI_HDR_METADATA_HDR10));
        blob.flags |= FLAG_META_SET;

        if (SUCCEEDED(pObject->SetPrivateData(kPrivateDataGuid, static_cast<UINT>(sizeof(blob)), &blob)))
            Revision().fetch_add(1, std::memory_order_release);
    }
}
