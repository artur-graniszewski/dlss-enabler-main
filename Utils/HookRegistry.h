#pragma once
#include "Hook.h"
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>

class HookRegistry {
public:
    static HookRegistry& Instance() { static HookRegistry inst; return inst; }
    void Register(std::unique_ptr<IHook> h) {
        std::lock_guard<std::mutex> lk(m_);
        hooks_.push_back(std::move(h));
    }
    std::vector<IHook*> GetSorted(HookPhase phase) {
        std::lock_guard<std::mutex> lk(m_);
        std::vector<IHook*> out;
        for (auto& up : hooks_) if (up->Phase() == phase) out.push_back(up.get());
        std::sort(out.begin(), out.end(), [](IHook* a, IHook* b) { return a->Priority() > b->Priority(); });
        return out;
    }
private:
    std::mutex m_;
    std::vector<std::unique_ptr<IHook>> hooks_;
};

#define REGISTER_HOOK(HookType, ...) \
    namespace { struct HookType##_AutoReg { HookType##_AutoReg(){ \
        HookRegistry::Instance().Register(std::make_unique<HookType>(__VA_ARGS__)); \
    }} HookType##_AutoReg_Instance; }