#pragma once
#include "ProcAliasRegistry.h"
#include <type_traits>
#include <functional>                 // std::function, std::optional
#include <type_traits>                // std::invoke
#include <utility>                    // std::forward
#include <optional>                   // std::optional
#define MAPS_TO(original) , original
#define WITH ,
#include <cstdint>                    // uint32_t


#define NGX_DEFINE_PROXY(proxy_method, original_name, ...) \
    constexpr auto proxy_method_ptr = &NgxFrontend::proxy_method; \
    thread_local std::optional<std::function<uint32_t(__VA_ARGS__)>> org_##original_name; \
    auto proxy_##original_name = [org = &org_##original_name](auto&&... args) -> uint32_t { \
        if (org && *org) { \
            return (*org)(std::forward<decltype(args)>(args)...); \
        } \
        return std::invoke(proxy_method_ptr, *ngxFrontend, std::forward<decltype(args)>(args)...); \
    };

#define NGX_MAKE_PROXY(FuncTypeName, ARGS)                         \
    uint32_t proxy_##FuncTypeName ARGS;                  \
    decltype(&proxy_##FuncTypeName) org_##FuncTypeName = nullptr; \
    uint32_t proxy_##FuncTypeName ARGS

#define NGX_ATTACH(ExportName)                                                              \
    do {                                                                                    \
        if (!ATTACH(org_##ExportName, &proxy_##ExportName, #ExportName)) {      \
            auto name = std::string(#ExportName); \
            LOG_ERROR(L"[NVNGX] Attach failed for " + std::wstring(name.begin(), name.end()));                    \
            return false;                                                                   \
        }                                                                                   \
    } while (0)

#define NGX_ALIAS(ExportName) \
    do { \
        RegisterNgxAlias(ngx, #ExportName, reinterpret_cast<void*>(&proxy_##ExportName)); \
    } while (0)


inline void RegisterNgxAlias(HMODULE ngxModule, const char* exportName, void* fnPtr)
{
    LOG_TRACE(L"[NVNGX] Aliasing " + Common::GetModuleFilePath(ngxModule).wstring());
    using FuncPtr = ProcAliasRegistry::FuncPtr;
    ProcAliasRegistry::Instance().RegisterAlias(
        ngxModule,
        exportName,
        reinterpret_cast<FuncPtr>(fnPtr));
}