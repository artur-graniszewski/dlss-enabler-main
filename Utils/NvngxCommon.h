#pragma once
#include <string>
#include <wtypes.h>
#include "Console.h"
#include "../Includes/dlss/nvsdk_ngx_params.h"
#include "../Includes/dlss/nvsdk_ngx_defs.h"

#define NGXDLLEXPORT extern "C" __declspec(dllexport)
struct NGXHandle;

struct PointerHash {
    template <typename T>
    std::size_t operator()(T* ptr) const {
        return std::hash<void*>{}(static_cast<void*>(ptr));
    }
};

struct NGXHandle
{
    uint32_t InternalId = 0;
    uint32_t InternalFeatureId = 0;

    static NGXHandle* Allocate(uint32_t FeatureId)
    {
        //constinit static uint32_t nextId = 500000000;
        constinit static uint32_t nextId = 50000;
        return new NGXHandle{ nextId++, FeatureId };
    }

    static void Free(NGXHandle* Handle)
    {
        delete Handle;
    }
};

bool NGX_HandleUnsupportedFeature(NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Handle** OutHandle);
bool NGX_IsFrameGenerationFeature(NVSDK_NGX_Handle* InFeatureHandle);
bool NGX_IsDeepDvcFeature(NVSDK_NGX_Handle* InFeatureHandle);
bool NGX_IsSuperSamplingFeature(NVSDK_NGX_Handle* InFeatureHandle);
bool NGX_RegisterFeature(NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Handle* InFeatureHandle);
bool NGX_UnregisterFeature(NVSDK_NGX_Handle* InFeatureHandle);
std::wstring NGX_FormatLogEntry(LPCWSTR functionName, std::wstring message);
std::wstring NGX_FeatureIdToString(NVSDK_NGX_Feature InFeatureID);
void NGX_PopulateNgxParameters(NVSDK_NGX_Parameter** OutParameters, bool createNew);
void NGX_CreateFeature(NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter* InParameters);
void NGX_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);
std::wstring NGX_EngineToString(NVSDK_NGX_EngineType InEngineType);
void NGX_ReportUpscalerStats(NVSDK_NGX_Parameter* InParameters);
void NGX_EvaluateFeature(NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters);
std::wstring NGX_FormatLogEntry(const std::string& message);
void NGX_Logger(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);
void NGX_ReportDlssVersions();
void NGX_InitProjectReport(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion);
void NGX_InitReport(const wchar_t* InApplicationDataPath, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo** InFeatureInfo);
void NGX_GetFeatureRequirements(NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo);
void NGX_DisableGpuSpoofing();
void NGX_EnableGpuSpoofing();

#define NVAPI_DISABLE_GPU_SPOOFING() \
	NGX_EnableGpuSpoofing();

#define NVAPI_ENABLE_GPU_SPOOFING() \
	NGX_EnableGpuSpoofing();

#define LOG_NGX_INFO(msg) \
    { \
        Console::Info(NGX_FormatLogEntry(__FUNCTIONW__, std::wstring(msg))); \
    }

#define LOG_NGX_WARNING(msg) \
    { \
		Console::Info(NGX_FormatLogEntry(__FUNCTIONW__, std::wstring(msg))); \
    }

#define LOG_NGX_ERROR(msg) \
    { \
		Console::Info(NGX_FormatLogEntry(__FUNCTIONW__, std::wstring(msg))); \
    }

#define LOG_NGX_FUNCTION_CALL_AND_RETURN(result) \
    do \
    { \
        if (ctx.logging.isExtraDebugEnabled || result != 0x1) { \
            std::wstringstream hexStream; \
            hexStream << L"0x" << std::hex << result; \
            std::wstring hexString = hexStream.str(); \
            if (NVSDK_NGX_SUCCEED(result)) { \
                Console::Info(NGX_FormatLogEntry(__FUNCTIONW__, std::wstring(L"succeeded"))); \
            } else { \
                Console::Info(NGX_FormatLogEntry(__FUNCTIONW__, std::wstring(L"failed: (" + hexString + L")"))); \
            } \
        } \
        return result; \
    } while (0)

#define LOG_NGX_FUNCTION_CALL() \
    LOG_DEBUG(NGX_FormatLogEntry(__FUNCTIONW__, std::wstring(L"")));

#define LOG_NGX_FUNCTION_CALL_WITH_ARG(arg) \
    LOG_DEBUG(NGX_FormatLogEntry(__FUNCTIONW__, std::wstring(arg + L" ")));
