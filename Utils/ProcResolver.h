#pragma once
#include <string>
#include "IProcResolver.h"

// Global accessors (kept tiny for compatibility with existing C-exports)
IProcResolver& GetProcResolver();
void InstallOriginalGetProcAddress(GetProcAddress_t proc);

class PrefixedProcResolver : public IProcResolver {
public:
    PrefixedProcResolver() = default;

    explicit PrefixedProcResolver(std::string prefix, bool allowFallback = true)
        : allowFallback_(allowFallback) {
        setPrefix(std::move(prefix));
    }

    void setPrefix(std::string prefix) {
        prefix_ = std::move(prefix);
        if (!prefix_.empty() && prefix_.back() != '_') prefix_.push_back('_');
    }

    void setAllowFallback(bool allow) { allowFallback_ = allow; }

    void Install(GetProcAddress_t proc) { originalGetProcAddress_ = proc; }

    FARPROC Resolve(HMODULE module, const char* name) override {
        if (!originalGetProcAddress_ || !module || !name) return nullptr;

        if (!prefix_.empty()) {
            std::string prefixed = prefix_;
            prefixed += name;
            if (auto fp = originalGetProcAddress_(module, prefixed.c_str())) {
                return fp;
            }
        }
        if (allowFallback_) {
            return originalGetProcAddress_(module, name);
        }
        return nullptr;
    }

private:
    GetProcAddress_t originalGetProcAddress_ = nullptr;
    std::string prefix_;
    bool allowFallback_ = true;
};

class DefaultProcResolver : public IProcResolver {
public:
    DefaultProcResolver() = default;
    void Install(GetProcAddress_t proc) { originalGetProcAddress = proc; }
    FARPROC Resolve(HMODULE module, const char* name) override {
        return originalGetProcAddress ? originalGetProcAddress(module, name) : nullptr;
    }
private:
    GetProcAddress_t originalGetProcAddress = nullptr;
};

IProcResolver& GetProcResolver();
void InstallOriginalGetProcAddress(GetProcAddress_t proc);

IProcResolver& GetPrefixedProcResolver();
void SetProcResolverPrefix(const char* prefix);
void SetProcResolverAllowFallback(bool allow);