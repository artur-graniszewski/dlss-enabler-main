#pragma once
#include <unordered_map>
#include <mutex>
#include <memory>

struct NVSDK_NGX_Handle;
class SsrtgiPostProcessD3D12;

class PostFxRegistry
{
public:
    static void Register(const NVSDK_NGX_Handle* h, std::unique_ptr<SsrtgiPostProcessD3D12> ctx)
    {
        if (!h || !ctx) return;
        std::scoped_lock lock(Mutex());
        Map()[h] = std::move(ctx);
    }

    static SsrtgiPostProcessD3D12* Find(const NVSDK_NGX_Handle* h)
    {
        if (!h) return nullptr;
        std::scoped_lock lock(Mutex());
        auto& m = Map();
        auto it = m.find(h);
        return (it == m.end()) ? nullptr : it->second.get();
    }

    static void Unregister(const NVSDK_NGX_Handle* h)
    {
        if (!h) return;
        std::scoped_lock lock(Mutex());
        Map().erase(h);
    }

private:
    static std::unordered_map<const NVSDK_NGX_Handle*, std::unique_ptr<SsrtgiPostProcessD3D12>>& Map()
    {
        static std::unordered_map<const NVSDK_NGX_Handle*, std::unique_ptr<SsrtgiPostProcessD3D12>> g;
        return g;
    }
    static std::mutex& Mutex()
    {
        static std::mutex m;
        return m;
    }
};
