// =============================================================================
// NgxFeatureEvents.h - Loose-Coupled Event System for NGX Features
// =============================================================================
//
// Provides event dispatching for NGX Create/Evaluate/Release operations
// with filtering by feature type. Similar architecture to SwapChainEvents.
//
// USAGE:
//   1. Create a listener implementing the appropriate callback signature
//   2. Register with NgxFeatureEvents::RegisterXXX() for specific API and event type
//   3. Optionally filter by NVSDK_NGX_Feature type
//   4. Events will be dispatched from NgxFrontend automatically
//
// EXAMPLE:
//   // Register for D3D12 Evaluate events, only for SuperSampling feature
//   NgxFeatureEvents::RegisterEvaluateD3D12(
//       [](ID3D12GraphicsCommandList* cmdList, const NVSDK_NGX_Handle* handle,
//          NVSDK_NGX_Parameter* params, NVSDK_NGX_Feature feature) {
//           // Your SSRTGI code here
//       },
//       NVSDK_NGX_Feature_SuperSampling  // Filter (or NVSDK_NGX_Feature_Reserved_SDK for all)
//   );
//
// =============================================================================

#pragma once

#include <functional>
#include <vector>
#include <mutex>
#include <d3d12.h>
#include <d3d11.h>
#include <vulkan/vulkan_core.h>
#include "../Includes/dlss/nvsdk_ngx.h"
#include "../Includes/dlss/nvsdk_ngx_defs.h"

namespace NgxFeatureEvents
{
    // =============================================================================
    // Feature filter constant - use this to receive ALL features
    // =============================================================================
    constexpr NVSDK_NGX_Feature AllFeatures = NVSDK_NGX_Feature_Reserved_SDK;

    // =============================================================================
    // D3D12 Callback Types
    // =============================================================================

    // Pre-Create: Called BEFORE CreateFeature, can modify parameters
    using PreCreateD3D12Callback = std::function<void(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters
        )>;

    // Post-Create: Called AFTER successful CreateFeature
    using PostCreateD3D12Callback = std::function<void(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result
        )>;

    // Pre-Evaluate: Called BEFORE EvaluateFeature, can modify parameters
    using PreEvaluateD3D12Callback = std::function<void(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId
        )>;

    // Post-Evaluate: Called AFTER EvaluateFeature
    using PostEvaluateD3D12Callback = std::function<void(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result
        )>;

    // Pre-Release: Called BEFORE ReleaseFeature
    using PreReleaseD3D12Callback = std::function<void(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId
        )>;

    // Post-Release: Called AFTER ReleaseFeature
    using PostReleaseD3D12Callback = std::function<void(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result
        )>;

    // =============================================================================
    // D3D11 Callback Types
    // =============================================================================

    using PreCreateD3D11Callback = std::function<void(
        ID3D11DeviceContext* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters
        )>;

    using PostCreateD3D11Callback = std::function<void(
        ID3D11DeviceContext* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result
        )>;

    using PreEvaluateD3D11Callback = std::function<void(
        ID3D11DeviceContext* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId
        )>;

    using PostEvaluateD3D11Callback = std::function<void(
        ID3D11DeviceContext* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result
        )>;

    using PreReleaseD3D11Callback = std::function<void(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId
        )>;

    using PostReleaseD3D11Callback = std::function<void(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result
        )>;

    // =============================================================================
    // Vulkan Callback Types
    // =============================================================================

    using PreCreateVulkanCallback = std::function<void(
        VkCommandBuffer cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters
        )>;

    using PostCreateVulkanCallback = std::function<void(
        VkCommandBuffer cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result
        )>;

    using PreEvaluateVulkanCallback = std::function<void(
        VkCommandBuffer cmdBuffer,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId
        )>;

    using PostEvaluateVulkanCallback = std::function<void(
        VkCommandBuffer cmdBuffer,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result
        )>;

    using PreReleaseVulkanCallback = std::function<void(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId
        )>;

    using PostReleaseVulkanCallback = std::function<void(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result
        )>;

    // =============================================================================
    // Registration Functions - D3D12
    // =============================================================================

    void RegisterPreCreateD3D12(PreCreateD3D12Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostCreateD3D12(PostCreateD3D12Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPreEvaluateD3D12(PreEvaluateD3D12Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostEvaluateD3D12(PostEvaluateD3D12Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPreReleaseD3D12(PreReleaseD3D12Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostReleaseD3D12(PostReleaseD3D12Callback callback, NVSDK_NGX_Feature filter = AllFeatures);

    // =============================================================================
    // Registration Functions - D3D11
    // =============================================================================

    void RegisterPreCreateD3D11(PreCreateD3D11Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostCreateD3D11(PostCreateD3D11Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPreEvaluateD3D11(PreEvaluateD3D11Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostEvaluateD3D11(PostEvaluateD3D11Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPreReleaseD3D11(PreReleaseD3D11Callback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostReleaseD3D11(PostReleaseD3D11Callback callback, NVSDK_NGX_Feature filter = AllFeatures);

    // =============================================================================
    // Registration Functions - Vulkan
    // =============================================================================

    void RegisterPreCreateVulkan(PreCreateVulkanCallback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostCreateVulkan(PostCreateVulkanCallback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPreEvaluateVulkan(PreEvaluateVulkanCallback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostEvaluateVulkan(PostEvaluateVulkanCallback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPreReleaseVulkan(PreReleaseVulkanCallback callback, NVSDK_NGX_Feature filter = AllFeatures);
    void RegisterPostReleaseVulkan(PostReleaseVulkanCallback callback, NVSDK_NGX_Feature filter = AllFeatures);

    // =============================================================================
    // Dispatch Functions - D3D12 (called by NgxFrontend)
    // =============================================================================

    void DispatchPreCreateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters);

    void DispatchPostCreateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result);

    void DispatchPreEvaluateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId);

    void DispatchPostEvaluateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result);

    void DispatchPreReleaseD3D12(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId);

    void DispatchPostReleaseD3D12(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result);

    // =============================================================================
    // Dispatch Functions - D3D11 (called by NgxFrontend)
    // =============================================================================

    void DispatchPreCreateD3D11(
        ID3D11DeviceContext* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters);

    void DispatchPostCreateD3D11(
        ID3D11DeviceContext* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result);

    void DispatchPreEvaluateD3D11(
        ID3D11DeviceContext* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId);

    void DispatchPostEvaluateD3D11(
        ID3D11DeviceContext* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result);

    void DispatchPreReleaseD3D11(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId);

    void DispatchPostReleaseD3D11(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result);

    // =============================================================================
    // Dispatch Functions - Vulkan (called by NgxFrontend)
    // =============================================================================

    void DispatchPreCreateVulkan(
        VkCommandBuffer cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters);

    void DispatchPostCreateVulkan(
        VkCommandBuffer cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result);

    void DispatchPreEvaluateVulkan(
        VkCommandBuffer cmdBuffer,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId);

    void DispatchPostEvaluateVulkan(
        VkCommandBuffer cmdBuffer,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result);

    void DispatchPreReleaseVulkan(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId);

    void DispatchPostReleaseVulkan(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result);

    // =============================================================================
    // Utility Functions
    // =============================================================================

    // Clear all registered listeners (useful for cleanup/testing)
    void ClearAllListeners();

    // Get listener count for debugging
    size_t GetListenerCount();
}