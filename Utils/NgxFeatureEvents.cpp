// =============================================================================
// NgxFeatureEvents.cpp - Loose-Coupled Event System for NGX Features
// =============================================================================

#include "NgxFeatureEvents.h"

namespace NgxFeatureEvents
{
    // =============================================================================
    // Internal Storage - Listener entries with feature filter
    // =============================================================================

    template<typename CallbackType>
    struct ListenerEntry
    {
        CallbackType callback;
        NVSDK_NGX_Feature filter;

        bool Matches(NVSDK_NGX_Feature feature) const
        {
            return filter == AllFeatures || filter == feature;
        }
    };

    // D3D12 Listeners
    static std::vector<ListenerEntry<PreCreateD3D12Callback>> g_PreCreateD3D12;
    static std::vector<ListenerEntry<PostCreateD3D12Callback>> g_PostCreateD3D12;
    static std::vector<ListenerEntry<PreEvaluateD3D12Callback>> g_PreEvaluateD3D12;
    static std::vector<ListenerEntry<PostEvaluateD3D12Callback>> g_PostEvaluateD3D12;
    static std::vector<ListenerEntry<PreReleaseD3D12Callback>> g_PreReleaseD3D12;
    static std::vector<ListenerEntry<PostReleaseD3D12Callback>> g_PostReleaseD3D12;

    // D3D11 Listeners
    static std::vector<ListenerEntry<PreCreateD3D11Callback>> g_PreCreateD3D11;
    static std::vector<ListenerEntry<PostCreateD3D11Callback>> g_PostCreateD3D11;
    static std::vector<ListenerEntry<PreEvaluateD3D11Callback>> g_PreEvaluateD3D11;
    static std::vector<ListenerEntry<PostEvaluateD3D11Callback>> g_PostEvaluateD3D11;
    static std::vector<ListenerEntry<PreReleaseD3D11Callback>> g_PreReleaseD3D11;
    static std::vector<ListenerEntry<PostReleaseD3D11Callback>> g_PostReleaseD3D11;

    // Vulkan Listeners
    static std::vector<ListenerEntry<PreCreateVulkanCallback>> g_PreCreateVulkan;
    static std::vector<ListenerEntry<PostCreateVulkanCallback>> g_PostCreateVulkan;
    static std::vector<ListenerEntry<PreEvaluateVulkanCallback>> g_PreEvaluateVulkan;
    static std::vector<ListenerEntry<PostEvaluateVulkanCallback>> g_PostEvaluateVulkan;
    static std::vector<ListenerEntry<PreReleaseVulkanCallback>> g_PreReleaseVulkan;
    static std::vector<ListenerEntry<PostReleaseVulkanCallback>> g_PostReleaseVulkan;

    // Mutex for thread safety
    static std::mutex g_Mutex;

    // =============================================================================
    // Registration Functions - D3D12
    // =============================================================================

    void RegisterPreCreateD3D12(PreCreateD3D12Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreCreateD3D12.push_back({ std::move(callback), filter });
    }

    void RegisterPostCreateD3D12(PostCreateD3D12Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostCreateD3D12.push_back({ std::move(callback), filter });
    }

    void RegisterPreEvaluateD3D12(PreEvaluateD3D12Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreEvaluateD3D12.push_back({ std::move(callback), filter });
    }

    void RegisterPostEvaluateD3D12(PostEvaluateD3D12Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostEvaluateD3D12.push_back({ std::move(callback), filter });
    }

    void RegisterPreReleaseD3D12(PreReleaseD3D12Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreReleaseD3D12.push_back({ std::move(callback), filter });
    }

    void RegisterPostReleaseD3D12(PostReleaseD3D12Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostReleaseD3D12.push_back({ std::move(callback), filter });
    }

    // =============================================================================
    // Registration Functions - D3D11
    // =============================================================================

    void RegisterPreCreateD3D11(PreCreateD3D11Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreCreateD3D11.push_back({ std::move(callback), filter });
    }

    void RegisterPostCreateD3D11(PostCreateD3D11Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostCreateD3D11.push_back({ std::move(callback), filter });
    }

    void RegisterPreEvaluateD3D11(PreEvaluateD3D11Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreEvaluateD3D11.push_back({ std::move(callback), filter });
    }

    void RegisterPostEvaluateD3D11(PostEvaluateD3D11Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostEvaluateD3D11.push_back({ std::move(callback), filter });
    }

    void RegisterPreReleaseD3D11(PreReleaseD3D11Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreReleaseD3D11.push_back({ std::move(callback), filter });
    }

    void RegisterPostReleaseD3D11(PostReleaseD3D11Callback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostReleaseD3D11.push_back({ std::move(callback), filter });
    }

    // =============================================================================
    // Registration Functions - Vulkan
    // =============================================================================

    void RegisterPreCreateVulkan(PreCreateVulkanCallback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreCreateVulkan.push_back({ std::move(callback), filter });
    }

    void RegisterPostCreateVulkan(PostCreateVulkanCallback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostCreateVulkan.push_back({ std::move(callback), filter });
    }

    void RegisterPreEvaluateVulkan(PreEvaluateVulkanCallback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreEvaluateVulkan.push_back({ std::move(callback), filter });
    }

    void RegisterPostEvaluateVulkan(PostEvaluateVulkanCallback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostEvaluateVulkan.push_back({ std::move(callback), filter });
    }

    void RegisterPreReleaseVulkan(PreReleaseVulkanCallback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PreReleaseVulkan.push_back({ std::move(callback), filter });
    }

    void RegisterPostReleaseVulkan(PostReleaseVulkanCallback callback, NVSDK_NGX_Feature filter)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_PostReleaseVulkan.push_back({ std::move(callback), filter });
    }

    // =============================================================================
    // Dispatch Functions - D3D12
    // =============================================================================

    void DispatchPreCreateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreCreateD3D12)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, featureId, parameters);
            }
        }
    }

    void DispatchPostCreateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostCreateD3D12)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, featureId, parameters, handle, result);
            }
        }
    }

    void DispatchPreEvaluateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreEvaluateD3D12)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, handle, parameters, featureId);
            }
        }
    }

    void DispatchPostEvaluateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostEvaluateD3D12)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, handle, parameters, featureId, result);
            }
        }
    }

    void DispatchPreReleaseD3D12(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreReleaseD3D12)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(handle, featureId);
            }
        }
    }

    void DispatchPostReleaseD3D12(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostReleaseD3D12)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(handle, featureId, result);
            }
        }
    }

    // =============================================================================
    // Dispatch Functions - D3D11
    // =============================================================================

    void DispatchPreCreateD3D11(
        ID3D11DeviceContext* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreCreateD3D11)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, featureId, parameters);
            }
        }
    }

    void DispatchPostCreateD3D11(
        ID3D11DeviceContext* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostCreateD3D11)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, featureId, parameters, handle, result);
            }
        }
    }

    void DispatchPreEvaluateD3D11(
        ID3D11DeviceContext* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreEvaluateD3D11)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, handle, parameters, featureId);
            }
        }
    }

    void DispatchPostEvaluateD3D11(
        ID3D11DeviceContext* cmdList,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostEvaluateD3D11)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdList, handle, parameters, featureId, result);
            }
        }
    }

    void DispatchPreReleaseD3D11(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreReleaseD3D11)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(handle, featureId);
            }
        }
    }

    void DispatchPostReleaseD3D11(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostReleaseD3D11)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(handle, featureId, result);
            }
        }
    }

    // =============================================================================
    // Dispatch Functions - Vulkan
    // =============================================================================

    void DispatchPreCreateVulkan(
        VkCommandBuffer cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreCreateVulkan)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdBuffer, featureId, parameters);
            }
        }
    }

    void DispatchPostCreateVulkan(
        VkCommandBuffer cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostCreateVulkan)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdBuffer, featureId, parameters, handle, result);
            }
        }
    }

    void DispatchPreEvaluateVulkan(
        VkCommandBuffer cmdBuffer,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreEvaluateVulkan)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdBuffer, handle, parameters, featureId);
            }
        }
    }

    void DispatchPostEvaluateVulkan(
        VkCommandBuffer cmdBuffer,
        const NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostEvaluateVulkan)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(cmdBuffer, handle, parameters, featureId, result);
            }
        }
    }

    void DispatchPreReleaseVulkan(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PreReleaseVulkan)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(handle, featureId);
            }
        }
    }

    void DispatchPostReleaseVulkan(
        NVSDK_NGX_Handle* handle,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Result result)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (const auto& entry : g_PostReleaseVulkan)
        {
            if (entry.Matches(featureId))
            {
                entry.callback(handle, featureId, result);
            }
        }
    }

    // =============================================================================
    // Utility Functions
    // =============================================================================

    void ClearAllListeners()
    {
        std::lock_guard<std::mutex> lock(g_Mutex);

        g_PreCreateD3D12.clear();
        g_PostCreateD3D12.clear();
        g_PreEvaluateD3D12.clear();
        g_PostEvaluateD3D12.clear();
        g_PreReleaseD3D12.clear();
        g_PostReleaseD3D12.clear();

        g_PreCreateD3D11.clear();
        g_PostCreateD3D11.clear();
        g_PreEvaluateD3D11.clear();
        g_PostEvaluateD3D11.clear();
        g_PreReleaseD3D11.clear();
        g_PostReleaseD3D11.clear();

        g_PreCreateVulkan.clear();
        g_PostCreateVulkan.clear();
        g_PreEvaluateVulkan.clear();
        g_PostEvaluateVulkan.clear();
        g_PreReleaseVulkan.clear();
        g_PostReleaseVulkan.clear();
    }

    size_t GetListenerCount()
    {
        std::lock_guard<std::mutex> lock(g_Mutex);

        return g_PreCreateD3D12.size() + g_PostCreateD3D12.size() +
            g_PreEvaluateD3D12.size() + g_PostEvaluateD3D12.size() +
            g_PreReleaseD3D12.size() + g_PostReleaseD3D12.size() +
            g_PreCreateD3D11.size() + g_PostCreateD3D11.size() +
            g_PreEvaluateD3D11.size() + g_PostEvaluateD3D11.size() +
            g_PreReleaseD3D11.size() + g_PostReleaseD3D11.size() +
            g_PreCreateVulkan.size() + g_PostCreateVulkan.size() +
            g_PreEvaluateVulkan.size() + g_PostEvaluateVulkan.size() +
            g_PreReleaseVulkan.size() + g_PostReleaseVulkan.size();
    }
}