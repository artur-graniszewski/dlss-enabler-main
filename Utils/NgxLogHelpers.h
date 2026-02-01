#pragma once
#include <string>
#include <sstream>
#include "../Includes/dlss/nvsdk_ngx.h"
#include "INgxBackend.h" // for INgxLogger

#define NGX_INIT_CALL(proxyFuncName) \
 \
    static constexpr char proxyName[] = proxyFuncName; \
    HMODULE backend = GetBackend(); \
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Success; \
    static std::wstring wideProxyName(proxyName, proxyName + sizeof(proxyName) - 1); \
    static const wchar_t* kEntry = wideProxyName.c_str(); \
    if (!backend) { \
        LogError(kEntry, L"backend not loaded"); \
        return NVSDK_NGX_Result_Fail; \
    }

#define NGX_INIT_SHIM(proxyFuncName) \
 \
    static constexpr char proxyName[] = proxyFuncName; \
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Success; \
    static std::wstring wideProxyName(proxyName, proxyName + sizeof(proxyName) - 1); \
    static const wchar_t* kEntry = wideProxyName.c_str();


#define NGX_INIT_CALL_INT(proxyFuncName) \
 \
    static constexpr char proxyName[] = proxyFuncName; \
    HMODULE backend = GetBackend(); \
    static std::wstring wideProxyName(proxyName, proxyName + sizeof(proxyName) - 1); \
    static const wchar_t* kEntry = wideProxyName.c_str(); \
    if (!backend) { \
        LogError(kEntry, L"backend not loaded"); \
        return 0U; \
    } 

#define NGX_RESOLVE_PROXY_ONCE(...)                                                 \
    using PfnType = NVSDK_NGX_Result(*)(__VA_ARGS__);                               \
    static PfnType proxy = nullptr;                                                 \
    if (!proxy) {                                                                   \
        proxy = reinterpret_cast<PfnType>(resolver.Resolve(backend, proxyName));    \
        if (!proxy) {                                                               \
            LogNoBackend(kEntry);                                                   \
            return NVSDK_NGX_Result_Fail;                                           \
        }                                                                           \
    }

#define NGX_RESOLVE_PROXY_ONCE_INT(...)                                             \
    using PfnType = uint32_t(*)(__VA_ARGS__);                                       \
    static PfnType proxy = nullptr;                                                 \
    if (!proxy) {                                                                   \
        proxy = reinterpret_cast<PfnType>(resolver.Resolve(backend, proxyName));    \
        if (!proxy) {                                                               \
            LogNoBackend(kEntry);                                                   \
            return 0U;                                                              \
        }                                                                           \
    }

#define NGX_VALIDATE_FEATURE_ID(featureID)                                          \
    if (featureId != featureID) {                                                   \
        LogError(kEntry, L"not proxied - no valid feature ID");                     \
        return NVSDK_NGX_Result_FAIL_FeatureNotSupported;                           \
    }


#define NGX_LOG_CALL { LogCall(logger, kEntry, kModule); }
#define NGX_LOG_RESULT_AND_RETURN { LogResult(logger, kEntry, kModule, result); return result; }
#define NGX_LOG_RESULT_AND_RETURN_INT { logger.Info(L"[" + std::wstring(kModule) + L"] " + std::wstring(kEntry) + L": returning " + std::to_wstring(result)); return result; }
#define NGX_LOG_RESULT_WITH_HANDLE_AND_RETURN { LogResultWithHandle(logger, kEntry, kModule, result, handle); return result; }

/**
TODO LIST
- try to reenable Init function calls to DLSS/DLSSD for optiscaler
- double check Init() dlssg call in some game if it works new way
- double check Shutdown() dlssg call in some game if it works new way
*/ 

// Always log: "[<entry>] call"
inline void LogCall(INgxLogger& logger, const wchar_t* entry, const wchar_t* submodule) {
    logger.Info(L"[" + std::wstring(submodule) + L"] " + std::wstring(entry));
}

inline std::wstring FormatNgxHex(NVSDK_NGX_Result result) {
    std::wstringstream ss; ss << L"(0x" << std::hex << static_cast<unsigned int>(result) << L")";
    return ss.str();
}

// Result without handle
inline void LogResult(INgxLogger& logger, const wchar_t* entry, const wchar_t* submodule, NVSDK_NGX_Result result) {
    if (NVSDK_NGX_SUCCEED(result)) {
        logger.Info(L"[" + std::wstring(submodule) + L"] " + std::wstring(entry) + L": succeeded");
    }
    else {
        logger.Error(L"[" + std::wstring(submodule) + L"] " + std::wstring(entry) + L": failed (error code: " + FormatNgxHex(result) + L")");
    }
}

inline void LogResultWithHandle(INgxLogger& logger,
    const wchar_t* entry,
    const wchar_t* submodule,
    NVSDK_NGX_Result result,
    NVSDK_NGX_Handle* handle) {
    if (NVSDK_NGX_SUCCEED(result)) {
        if (handle) {
            logger.Info(L"[" + std::wstring(submodule) + L"] " + std::wstring(entry) + L": ID=" + std::to_wstring(handle->Id));
        }
        logger.Info(L"[" + std::wstring(submodule) + L"] " + std::wstring(entry) + L": succeeded");
    }
    else {
        logger.Error(L"[" + std::wstring(submodule) + L"] " + std::wstring(entry) + L": failed (error code: " + FormatNgxHex(result) + L")");
    }
}
