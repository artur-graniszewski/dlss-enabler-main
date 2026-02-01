#pragma once
#include <d3d11.h>
#include <wtypes.h>
#include "../Includes/dlss/nvsdk_ngx.h"
#include <vulkan/vulkan_core.h>

FARPROC WINAPI DetourNgx(HMODULE hModule, LPCSTR lpProcName);

void SetOriginalGetProcAddress(GetProcAddress_t originalPointer);

NVSDK_NGX_Result proxy_NVSDK_NGX_UpdateFeature(const NVSDK_NGX_Application_Identifier* ApplicationId, const NVSDK_NGX_Feature FeatureID);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_DestroyParameters(NVSDK_NGX_Parameter* InParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_DestroyParameters(NVSDK_NGX_Parameter* InParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_DestroyParameters(NVSDK_NGX_Parameter* InParameters);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_AllocateParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_AllocateParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_AllocateParameters(NVSDK_NGX_Parameter** OutParameters);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_CreateFeature(ID3D11DeviceContext* InCmdList, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_CreateFeature(ID3D12GraphicsCommandList* InCmdList, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_CreateFeature(void* InCmdList, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_CreateFeature1(const VkDevice InDevice, void* InCmdList, NVSDK_NGX_Feature InFeatureID,
	NVSDK_NGX_Parameter* InParameters, NVSDK_NGX_Handle** OutHandle);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_ReleaseFeature(NVSDK_NGX_Handle* InstanceHandle);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_ReleaseFeature(NVSDK_NGX_Handle* InstanceHandle);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_ReleaseFeature(NVSDK_NGX_Handle* InstanceHandle);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_EvaluateFeature
(ID3D11DeviceContext* InCmdList, NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
	PFN_NVSDK_NGX_ProgressCallback InCallback);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_EvaluateFeature
(ID3D12GraphicsCommandList* InCmdList, NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
	PFN_NVSDK_NGX_ProgressCallback InCallback);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_EvaluateFeature
(void* InCmdList, NVSDK_NGX_Handle* InFeatureHandle, NVSDK_NGX_Parameter* InParameters,
	PFN_NVSDK_NGX_ProgressCallback InCallback);


NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetParameters(NVSDK_NGX_Parameter** OutParameters);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetParameters(NVSDK_NGX_Parameter** OutParameters);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Init_ProjectID(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion, const wchar_t* InApplicationDataPath,
	ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Init_ProjectID(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion, const wchar_t* InApplicationDataPath,
	ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init_ProjectID(const char* InProjectId, NVSDK_NGX_EngineType InEngineType, const char* InEngineVersion, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, void* InGIPA, void* InGDPA, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion);

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_RequiredExtensions(unsigned int* OutInstanceExtCount, const char*** OutInstanceExts, unsigned int* OutDeviceExtCount, const char*** OutDeviceExts);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter* InParameters, size_t* OutSizeInBytes);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Init(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Init(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, void* InGIPA, void* InGDPA, const NVSDK_NGX_FeatureCommonInfo* InFeatureInfo, NVSDK_NGX_Version InSDKVersion);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Init_Ext(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D11Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Init_Ext(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath,
	ID3D12Device* InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init_Ext(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Init_Ext2(unsigned long long InApplicationId, const wchar_t* InApplicationDataPath, void* InInstance, void* InPD, VkDevice InDevice, void* InGIPA, void* InGDPA, NVSDK_NGX_Version InSDKVersion, NVSDK_NGX_FeatureCommonInfo* InFeatureInfo);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Shutdown1(ID3D11Device* D3DDevice);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Shutdown1(ID3D12Device* D3DDevice);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Shutdown1(void* D3DDevice);

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_Shutdown();
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_Shutdown();
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_Shutdown();

NVSDK_NGX_Result proxy_NVSDK_NGX_D3D11_GetFeatureRequirements(
	IDXGIAdapter* Adapter,
	NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo);
NVSDK_NGX_Result proxy_NVSDK_NGX_D3D12_GetFeatureRequirements(
	IDXGIAdapter* Adapter,
	NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo);
NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetFeatureRequirements(
	void* Arg1,
	void* Arg2,
	NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo, NVSDK_NGX_FeatureRequirement* RequirementInfo);

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo,
	uint32_t* OutExtensionCount,
	void** OutExtensionProperties);

NVSDK_NGX_Result proxy_NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(void* Instance,
	void* PhysicalDevice,
	const NVSDK_NGX_FeatureDiscoveryInfo* FeatureDiscoveryInfo,
	uint32_t* OutExtensionCount,
	void** OutExtensionProperties);

extern decltype(&proxy_NVSDK_NGX_UpdateFeature) org_NVSDK_NGX_UpdateFeature;

extern decltype(&proxy_NVSDK_NGX_D3D11_Shutdown1) org_NVSDK_NGX_D3D11_Shutdown1;
extern decltype(&proxy_NVSDK_NGX_D3D12_Shutdown1) org_NVSDK_NGX_D3D12_Shutdown1;
extern decltype(&proxy_NVSDK_NGX_VULKAN_Shutdown1) org_NVSDK_NGX_VULKAN_Shutdown1;

extern decltype(&proxy_NVSDK_NGX_D3D11_Shutdown) org_NVSDK_NGX_D3D11_Shutdown;
extern decltype(&proxy_NVSDK_NGX_D3D12_Shutdown) org_NVSDK_NGX_D3D12_Shutdown;
extern decltype(&proxy_NVSDK_NGX_VULKAN_Shutdown) org_NVSDK_NGX_VULKAN_Shutdown;

extern decltype(&proxy_NVSDK_NGX_D3D11_Init_Ext) org_NVSDK_NGX_D3D11_Init_Ext;
extern decltype(&proxy_NVSDK_NGX_D3D12_Init_Ext) org_NVSDK_NGX_D3D12_Init_Ext;

extern decltype(&proxy_NVSDK_NGX_D3D11_Init) org_NVSDK_NGX_D3D11_Init;
extern decltype(&proxy_NVSDK_NGX_D3D12_Init) org_NVSDK_NGX_D3D12_Init;
extern decltype(&proxy_NVSDK_NGX_VULKAN_Init) org_NVSDK_NGX_VULKAN_Init;
extern decltype(&proxy_NVSDK_NGX_VULKAN_Init_Ext) org_NVSDK_NGX_VULKAN_Init_Ext;
extern decltype(&proxy_NVSDK_NGX_VULKAN_Init_Ext2) org_NVSDK_NGX_VULKAN_Init_Ext2;

extern decltype(&proxy_NVSDK_NGX_D3D11_GetScratchBufferSize) org_NVSDK_NGX_D3D11_GetScratchBufferSize;
extern decltype(&proxy_NVSDK_NGX_D3D12_GetScratchBufferSize) org_NVSDK_NGX_D3D12_GetScratchBufferSize;
extern decltype(&proxy_NVSDK_NGX_VULKAN_GetScratchBufferSize) org_NVSDK_NGX_VULKAN_GetScratchBufferSize;

extern decltype(&proxy_NVSDK_NGX_D3D11_Init_ProjectID) org_NVSDK_NGX_D3D11_Init_ProjectID;
extern decltype(&proxy_NVSDK_NGX_D3D12_Init_ProjectID) org_NVSDK_NGX_D3D12_Init_ProjectID;
extern decltype(&proxy_NVSDK_NGX_VULKAN_Init_ProjectID) org_NVSDK_NGX_VULKAN_Init_ProjectID;

extern decltype(&proxy_NVSDK_NGX_VULKAN_RequiredExtensions) org_NVSDK_NGX_VULKAN_RequiredExtensions;

extern decltype(&proxy_NVSDK_NGX_D3D11_GetParameters) org_NVSDK_NGX_D3D11_GetParameters;
extern decltype(&proxy_NVSDK_NGX_D3D12_GetParameters) org_NVSDK_NGX_D3D12_GetParameters;
extern decltype(&proxy_NVSDK_NGX_VULKAN_GetParameters) org_NVSDK_NGX_VULKAN_GetParameters;

extern decltype(&proxy_NVSDK_NGX_D3D11_EvaluateFeature) org_NVSDK_NGX_D3D11_EvaluateFeature;
extern decltype(&proxy_NVSDK_NGX_D3D12_EvaluateFeature) org_NVSDK_NGX_D3D12_EvaluateFeature;
extern decltype(&proxy_NVSDK_NGX_VULKAN_EvaluateFeature) org_NVSDK_NGX_VULKAN_EvaluateFeature;

extern decltype(&proxy_NVSDK_NGX_D3D11_GetCapabilityParameters) org_NVSDK_NGX_D3D11_GetCapabilityParameters;
extern decltype(&proxy_NVSDK_NGX_D3D12_GetCapabilityParameters) org_NVSDK_NGX_D3D12_GetCapabilityParameters;
extern decltype(&proxy_NVSDK_NGX_VULKAN_GetCapabilityParameters) org_NVSDK_NGX_VULKAN_GetCapabilityParameters;

extern decltype(&proxy_NVSDK_NGX_D3D11_ReleaseFeature) org_NVSDK_NGX_D3D11_ReleaseFeature;
extern decltype(&proxy_NVSDK_NGX_D3D12_ReleaseFeature) org_NVSDK_NGX_D3D12_ReleaseFeature;
extern decltype(&proxy_NVSDK_NGX_VULKAN_ReleaseFeature) org_NVSDK_NGX_VULKAN_ReleaseFeature;

extern decltype(&proxy_NVSDK_NGX_D3D11_CreateFeature) org_NVSDK_NGX_D3D11_CreateFeature;
extern decltype(&proxy_NVSDK_NGX_D3D12_CreateFeature) org_NVSDK_NGX_D3D12_CreateFeature;
extern decltype(&proxy_NVSDK_NGX_VULKAN_CreateFeature) org_NVSDK_NGX_VULKAN_CreateFeature;
extern decltype(&proxy_NVSDK_NGX_VULKAN_CreateFeature1) org_NVSDK_NGX_VULKAN_CreateFeature1;

extern decltype(&proxy_NVSDK_NGX_D3D11_DestroyParameters) org_NVSDK_NGX_D3D11_DestroyParameters;
extern decltype(&proxy_NVSDK_NGX_D3D12_DestroyParameters) org_NVSDK_NGX_D3D12_DestroyParameters;
extern decltype(&proxy_NVSDK_NGX_VULKAN_DestroyParameters) org_NVSDK_NGX_VULKAN_DestroyParameters;

extern decltype(&proxy_NVSDK_NGX_D3D11_AllocateParameters) org_NVSDK_NGX_D3D11_AllocateParameters;
extern decltype(&proxy_NVSDK_NGX_D3D12_AllocateParameters) org_NVSDK_NGX_D3D12_AllocateParameters;
extern decltype(&proxy_NVSDK_NGX_VULKAN_AllocateParameters) org_NVSDK_NGX_VULKAN_AllocateParameters;

extern decltype(&proxy_NVSDK_NGX_D3D12_GetFeatureRequirements) org_NVSDK_NGX_D3D12_GetFeatureRequirements;
extern decltype(&proxy_NVSDK_NGX_VULKAN_GetFeatureRequirements) org_NVSDK_NGX_VULKAN_GetFeatureRequirements;
extern decltype(&proxy_NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements) org_NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements;
extern decltype(&proxy_NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements) org_NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements;

