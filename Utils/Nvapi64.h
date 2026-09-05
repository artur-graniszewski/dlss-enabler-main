#pragma once

#include "Common.h"
#include "GpuHelpers.h"
#include "../Core/Context.h"
#include <d3d12.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include "../Includes/nvapi.h"
#include "NvAPIInterface.h"
#include <unordered_map>
#include <map>
#include "../Includes/nvapi.h"
#include <NvApiDriverSettings.h>
#include <vulkan/vulkan_core.h>
#include "FakeNVAPI.h"
#include <powerbase.h>
#include <mutex>
#include "ReflexEvents.h"
#include "OverdriveController.h"
#include "DlssgLazyHook.h"
#include <intrin.h>
#pragma comment(lib, "powrprof.lib")


extern uint64_t streamlineSignalId;

namespace NVAPI
{
#define NV_STRUCT_VERSION(ver) ((ver) >> 16)

#define NVAPI_DISPLAY_ID_SPACE 10000000
#define NVAPI_GPU_ID_SPACE 100000000
#define NVAPI_INITIALIZE                 0x150E828UL
#define NVAPI_GET_INTERFACE_VERSION      0x01053FA5L
#define NVAPI_ENUM_PHYSICAL_GPUS         0xE5AC921FUL
#define NVAPI_SYS_GET_DRIVER_VERSION     0x2926AAADUL
#define NVAPI_GPU_GET_ARCH_INFO          0xD8265D24UL
#define NVAPI_ENUM_LOGICAL_GPUS          0x48B3EA59UL
#define NVAPI_INITIALIZE_PRE             0xAD298D3FUL
#define NVAPI_CALL_START                 0x33C7358CUL
#define NVAPI_CALL_RETURN                0x593E8644UL
#define NVAPI_D3D12_QUERY_CPU_VIDMEM     0x26322BC3UL
#define NVAPI_UNLOAD_EX                  0xD7C61344UL
#define NVAPI_UNLOAD                     0xD22BDD7EUL
#define NVAPI_GPU_QUERY_NODE_INFO        0xE9B009B9UL
#define NVAPI_D3D_GET_SLEEP_STATUS       0xAEF96CA1UL
#define NVAPI_D3D_SET_SLEEP_MODE         0xAC1CA9E0UL
#define NVAPI_D3D_SLEEP                  0x852CD1D2UL
#define NVAPI_D3D_SET_LATENCY_MARKER     0xD9984C05UL
#define NVAPI_D3D_GET_LATENCY            0x1A587F9CUL
#define REFLEX_MAX_FRAMES_PROBE_SIZE	1000000000


	// DRS Setting IDs - z oficjalnego NvApiDriverSettings.h
#define NVAPI_DRS_GET_SETTING_ID                0x73bf8338L

// NGX / DLSS-FG / MFG settings
#define NGX_DLSSG_MULTI_FRAME_COUNT_ID          0x104D6667  // Override DLSSG multi-frame count (1-4)
#define NGX_DLSS_FG_OVERRIDE_ID                 0x10E41E03  // Enable DLSS-FG override (ON/OFF)

// Warto�ci dla NGX_DLSS_FG_OVERRIDE
#define NGX_DLSS_FG_OVERRIDE_OFF                0x00000000
#define NGX_DLSS_FG_OVERRIDE_ON                 0x00000001

// Warto�ci dla NGX_DLSSG_MULTI_FRAME_COUNT (min=1, max=4, default=1)
#define NGX_DLSSG_MULTI_FRAME_COUNT_MIN         0x00000001
#define NGX_DLSSG_MULTI_FRAME_COUNT_MAX         0x00000004
#define NGX_DLSSG_MULTI_FRAME_COUNT_DEFAULT     0x00000001

	struct NV_SCG_PRIORITY_INFO
	{
		void* CommandList; // 0
		uint32_t Unknown2; // 8
		uint32_t Unknown3; // C
		uint8_t Unknown4;  // 10
		uint8_t Unknown5;  // 11
		uint8_t Unknown6;  // 12
		uint8_t Unknown7;  // 13
		uint32_t Unknown8; // 14
	};

	typedef NvAPI_Status(WINAPI* NvAPI_DRS_GetSetting_t)(NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile, NvU32 settingId, NVDRS_SETTING* pSetting);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetArchInfo_t)(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_ARCH_INFO* pGpuArchInfo);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetPCIIdentifiers_t)(NvPhysicalGpuHandle hPhysicalGpu, NvU32* pDeviceId, NvU32* pSubSystemId, NvU32* pRevisionId, NvU32* pExtDeviceId);
	typedef NvAPI_Status(WINAPI* NvAPI_Initialize_t)();
	typedef NvAPI_Status(WINAPI* NvAPI_DISP_GetGDIPrimaryDisplayId_t)(NvU32* displayId);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_CudaEnumComputeCapableGpus_t)(NV_COMPUTE_GPU_TOPOLOGY* pComputeTopo);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_SetNvShaderExtnSlotSpaceLocalThread_t)(IUnknown* pDev, NvU32 uavSlot, NvU32 uavSpace);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetLogicalGpuInfo_t)(NvLogicalGpuHandle logicalHandle, NV_LOGICAL_GPU_DATA* logicalGpuData);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetFullName_t)(NvPhysicalGpuHandle hPhysicalGpu, NvAPI_ShortString szName);
	typedef NvAPI_Status(WINAPI* NvAPI_SYS_GetDisplayDriverInfo_t)(NV_DISPLAY_DRIVER_INFO* driverInfo);
	typedef NvAPI_Status(WINAPI* NvAPI_Mosaic_GetDisplayViewportsByResolution_t)(NvU32 displayId, NvU32 srcWidth, NvU32 srcHeight, NV_RECT viewports[NV_MOSAIC_MAX_DISPLAYS], NvU8* bezelCorrected);
	typedef NvAPI_Status(WINAPI* NvAPI_EnumPhysicalGPUs_t)(int nvGPUHandle[64], unsigned long* pGpuCount);
	typedef NvAPI_Status(WINAPI* NvAPI_SYS_GetDriverAndBranchVersion_t)(unsigned long* pDriverVersion, char szBuildBranchString[64]);
	typedef NvAPI_Status(WINAPI* NvAPI_EnumLogicalGPUs_t)(NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32* pGpuCount);
	typedef NvAPI_Status(WINAPI* NvAPI_GetInterfaceVersionString_t)(NvAPI_ShortString desc);
	typedef NvAPI_Status(WINAPI* NvAPI_EnumNvidiaDisplayHandle_t)(NvU32 displayId, NvDisplayHandle* handle);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetAllClockFrequencies_t)(NvPhysicalGpuHandle hPhysicalGPU, NV_GPU_CLOCK_FREQUENCIES* pClkFreqs);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetGpuCoreCount_t)(NvPhysicalGpuHandle hPhysicalGpu, NvU32* pCount);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetConnectedDisplayIds_t)(NvPhysicalGpuHandle handle, NV_GPU_DISPLAYIDS* displayIds, NvU32* displayCount, NvU32 flags);
	typedef NvAPI_Status(WINAPI* NvAPI_DRS_LoadSettings_t)(NvDRSSessionHandle session);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_SetVerticalSyncMode_t)(IUnknown* pDevice, NVAPI_VSYNC_MODE vsyncMode);
	typedef NvAPI_Status(WINAPI* NvAPI_Unload_t)();
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_QueryCpuVisibleVidmem_t)();
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_GetSleepStatus_t)();
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_QueryNodeInfo_t)(void* a1, void* a2);
	typedef NvAPI_Status(WINAPI* NvAPI_InitializePre_t)();
	typedef NvAPI_Status(WINAPI* NvAPI_CallStart_t)();
	typedef NvAPI_Status(WINAPI* NvAPI_CallReturn_t)();
	typedef NvAPI_Status(WINAPI* NvApi_UnloadEx_t)();
	typedef NvAPI_Status(WINAPI* NvLL_VK_Sleep_t)(VkDevice device, uint64_t semaphoreValue);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_SetSleepMode_t)(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pSetSleepModeParams);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_Sleep_t)(void* nvApiDevice);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_SetLatencyMarker_t)(IUnknown* pDev, NV_LATENCY_MARKER_PARAMS* pSetLatencyMarkerParams);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_SetAsyncFrameMarker_t)(ID3D12CommandQueue* pCommandQueue, NV_ASYNC_FRAME_MARKER_PARAMS* pSetAsyncFrameMarkerParams);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_GetLatency_t)(IUnknown* pDev, NV_LATENCY_RESULT_PARAMS* pGetLatencyParams);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_GetObjectHandleForResource_t)(IUnknown* invalid, IUnknown* pResource, NVDX_ObjectHandle* pHandle);
	typedef NvAPI_Status(WINAPI* NvAPI_GetGPUIDfromPhysicalGPU_t)(NvPhysicalGpuHandle hPhysicalGpu, NvU32* pGpuId);
	typedef NvAPI_Status(WINAPI* NvAPI_GetPhysicalGPUFromGPUID_t)(NvU32 gpuId, NvPhysicalGpuHandle* pPhysicalGPU);
	typedef NvAPI_Status(WINAPI* NvAPI_GetLogicalGPUFromPhysicalGPU_t)(NvPhysicalGpuHandle physicalHandle, NvLogicalGpuHandle* logicalHandle);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_IsNvShaderExtnOpCodeSupported_t)(ID3D12Device* pDevice, NvU32 opCode, bool* pSupported);
	typedef NvAPI_Status(WINAPI* NvAPI_DISP_GetDisplayIdByDisplayName_t)(const char* displayName, NvU32* displayId);
	typedef NvAPI_Status(WINAPI* NvAPI_DRS_CreateSession_t)(NvDRSSessionHandle* session);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_GetRaytracingCaps_t)(ID3D12Device* device, NVAPI_D3D12_RAYTRACING_CAPS_TYPE type, void* pData, size_t dataSize);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_GetRaytracingAccelerationStructurePrebuildInfoEx_t)(ID3D12Device5* pDevice, NVAPI_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_EX_PARAMS* pParams);
	//typedef NvAPI_Status(WINAPI* NvAPI_EnumLogicalGPUs_t)(int nvGPUHandle[64], unsigned long* pGpuCount);
	typedef NvAPI_Status(WINAPI* NvAPI_Success_t)();
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_SetRawScgPriority_t)(NV_SCG_PRIORITY_INFO* PriorityInfo);
	typedef NvAPI_Status(WINAPI* NvAPI_DRS_GetSetting_t)(NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile, NvU32 settingId, NVDRS_SETTING* pSetting);
	typedef NvAPI_Status(WINAPI* NvAPI_DRS_GetBaseProfile_t)(NvDRSSessionHandle hSession, NvDRSProfileHandle* phProfile);
	typedef NvAPI_Status(WINAPI* NvAPI_SYS_GetDisplayIdFromGpuAndOutputId_t)(NvPhysicalGpuHandle hPhysicalGpu, NvU32 outputId, NvU32* displayId);
	typedef NvAPI_Status(WINAPI* NvAPI_SYS_GetGpuAndOutputIdFromDisplayId_t)(NvU32 displayId, NvPhysicalGpuHandle* hPhysicalGpu, NvU32* outputId);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_BuildRaytracingAccelerationStructureEx_t)(ID3D12GraphicsCommandList4* pCommandList, const NVAPI_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_EX_PARAMS* pParams);
	typedef NvAPI_Status(WINAPI* NvAPI_Disp_GetOutputMode_t)(NvU32 displayId, NV_DISPLAY_OUTPUT_MODE* pDisplayMode);
	typedef NvAPI_Status(WINAPI* NvAPI_Disp_SetOutputMode_t)(NvU32 displayId, NV_DISPLAY_OUTPUT_MODE* pDisplayMode);
	typedef NvAPI_Status(WINAPI* NvAPI_Stereo_IsEnabled_t)(NvU8* enabled);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetPstates20_t)(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATES20_INFO* pPstatesInfo);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D_GetCurrentSLIState_t)(IUnknown* pDevice, NV_GET_CURRENT_SLI_STATE* pSliState);
	typedef NvAPI_Status(WINAPI* NvAPI_GPU_GetAdapterIdFromPhysicalGpu_t)(NvPhysicalGpuHandle hPhysicalGpu, void* pOSAdapterId);
	typedef NvAPI_Status(WINAPI* NvAPI_D3D12_CreateCubinComputeShaderWithName_t)(ID3D12Device* pDevice, const void* cubinData, NvU32 cubinSize, NvU32 blockX, NvU32 blockY, NvU32 blockZ, const char* shaderName, NVDX_ObjectHandle* pShader);
	typedef NvAPI_Status(*NvAPI_GPU_GetBusId_t)(int* handle, int* busId); //added
	typedef NvAPI_Status(*NvAPI_Unload_t)(); //added
	typedef NvAPI_Status(*NvAPI_D3D_InitializeEx_t)(); //added
	// Function pointers for Vulkan functions
	typedef VkResult(VKAPI_PTR* PFN_vkCreateSemaphore)(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore*);
	typedef VkResult(VKAPI_PTR* PFN_vkQueueSubmit)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
	typedef VkResult(VKAPI_PTR* PFN_vkQueueWaitIdle)(VkQueue);
	typedef void (VKAPI_PTR* PFN_vkDestroySemaphore)(VkDevice, VkSemaphore, const VkAllocationCallbacks*);
	typedef void (VKAPI_PTR* PFN_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);

	NvAPI_D3D12_CreateCubinComputeShaderWithName_t org_NvAPI_D3D12_CreateCubinComputeShader;
	NvAPI_GPU_GetAllClockFrequencies_t org_NvAPI_GPU_GetAllClockFrequencies;
	NvAPI_GPU_GetGpuCoreCount_t org_NvAPI_GPU_GetGpuCoreCount;
	NvAPI_Mosaic_GetDisplayViewportsByResolution_t org_NvAPI_Mosaic_GetDisplayViewportsByResolution;
	NvAPI_D3D_SetSleepMode_t org_NvAPI_D3D_SetSleepMode;
	NvAPI_D3D_GetLatency_t org_NvAPI_D3D_GetLatency;
	NvAPI_D3D_Sleep_t org_NvAPI_D3D_Sleep;
	NvAPI_D3D_SetLatencyMarker_t org_NvAPI_D3D_SetLatencyMarker;
	NvAPI_D3D12_SetAsyncFrameMarker_t org_NvAPI_D3D12_SetAsyncFrameMarker;
	NvAPI_GPU_GetArchInfo_t org_NvAPI_GPU_GetArchInfo;
	NvAPI_DRS_GetSetting_t org_NvAPI_DRS_GetSetting;

	using Microsoft::WRL::ComPtr;

	struct PhysicalGpuEntry
	{
		DXGI_ADAPTER_DESC1 desc;
		ComPtr<IDXGIAdapter1> adapter;
		NvU32 gpuId;
	};

	struct LogicalGpuEntry
	{
		DXGI_ADAPTER_DESC1 desc;
		ComPtr<IDXGIAdapter1> adapter;
		NvU32 gpuId;
	};

	static std::mutex nvapiMutex;
	static double reflexSleep = 0.0f;
	static double currentTimeMsec;
	static double _lastFrameTime = 0.0f;

	static auto drs = 1U;
	static auto drsSession = reinterpret_cast<NvDRSSessionHandle>(&drs);
	static auto drsProfile = reinterpret_cast<NvDRSProfileHandle>(&drs);
	static std::vector<PhysicalGpuEntry> physicalGpus;

	static std::wstring GetModuleTag()
	{
		return L"[NVAPI] ";
	}

	static PhysicalGpuEntry* UnpackNvPhysicalGpuHandle(NvPhysicalGpuHandle h)
	{
		return reinterpret_cast<PhysicalGpuEntry*>(h);
	}

	static PhysicalGpuEntry* UnpackNvLogicalGpuHandle(NvLogicalGpuHandle h)
	{
		return reinterpret_cast<PhysicalGpuEntry*>(h);
	}

	static NvPhysicalGpuHandle PackNvPhysicalGpuHandle(PhysicalGpuEntry* e)
	{
		return reinterpret_cast<NvPhysicalGpuHandle>(e);
	}

	static NvLogicalGpuHandle PackNvLogicalGpuHandle(PhysicalGpuEntry* e)
	{
		return reinterpret_cast<NvLogicalGpuHandle>(e);
	}

	static std::string GetErrorMessage(const int16_t errorCode) {
		static const std::map<int16_t, std::string> errors{
			{-1, "NVAPI_ERROR"},
			{-2, "NVAPI_LIBRARY_NOT_FOUND"},
			{-3, "NVAPI_NO_IMPLEMENTATION"},
			{-4, "NVAPI_API_NOT_INITIALIZED"},
			{-5, "NVAPI_INVALID_ARGUMENT"},
			{-6, "NVAPI_NVIDIA_DEVICE_NOT_FOUND"},
			{-7, "NVAPI_END_ENUMERATION"},
			{-8, "NVAPI_INVALID_HANDLE"},
			{-9, "NVAPI_INCOMPATIBLE_STRUCT_VERSION"},
			{-10, "NVAPI_HANDLE_INVALIDATED"},
			{-11, "NVAPI_OPENGL_CONTEXT_NOT_CURRENT"},
			{-14, "NVAPI_INVALID_POINTER"},
			{-12, "NVAPI_NO_GL_EXPERT"},
			{-13, "NVAPI_INSTRUMENTATION_DISABLED"},
			{-15, "NVAPI_NO_GL_NSIGHT"},
			{-100, "NVAPI_EXPECTED_LOGICAL_GPU_HANDLE"},
			{-101, "NVAPI_EXPECTED_PHYSICAL_GPU_HANDLE"},
			{-102, "NVAPI_EXPECTED_DISPLAY_HANDLE"},
			{-103, "NVAPI_INVALID_COMBINATION"},
			{-104, "NVAPI_NOT_SUPPORTED"},
			{-105, "NVAPI_PORTID_NOT_FOUND"},
			{-106, "NVAPI_EXPECTED_UNATTACHED_DISPLAY_HANDLE"},
			{-107, "NVAPI_INVALID_PERF_LEVEL"},
			{-108, "NVAPI_DEVICE_BUSY"},
			{-109, "NVAPI_NV_PERSIST_FILE_NOT_FOUND"},
			{-110, "NVAPI_PERSIST_DATA_NOT_FOUND"},
			{-111, "NVAPI_EXPECTED_TV_DISPLAY"},
			{-112, "NVAPI_EXPECTED_TV_DISPLAY_ON_DCONNECTOR"},
			{-113, "NVAPI_NO_ACTIVE_SLI_TOPOLOGY"},
			{-114, "NVAPI_SLI_RENDERING_MODE_NOTALLOWED"},
			{-115, "NVAPI_EXPECTED_DIGITAL_FLAT_PANEL"},
			{-116, "NVAPI_ARGUMENT_EXCEED_MAX_SIZE"},
			{-117, "NVAPI_DEVICE_SWITCHING_NOT_ALLOWED"},
			{-118, "NVAPI_TESTING_CLOCKS_NOT_SUPPORTED"},
			{-119, "NVAPI_UNKNOWN_UNDERSCAN_CONFIG"},
			{-120, "NVAPI_TIMEOUT_RECONFIGURING_GPU_TOPO"},
			{-121, "NVAPI_DATA_NOT_FOUND"},
			{-122, "NVAPI_EXPECTED_ANALOG_DISPLAY"},
			{-123, "NVAPI_NO_VIDLINK"},
			{-124, "NVAPI_REQUIRES_REBOOT"},
			{-125, "NVAPI_INVALID_HYBRID_MODE"},
			{-126, "NVAPI_MIXED_TARGET_TYPES"},
			{-127, "NVAPI_SYSWOW64_NOT_SUPPORTED"},
			{-128, "NVAPI_IMPLICIT_SET_GPU_TOPOLOGY_CHANGE_NOT_ALLOWED"},
			{-129, "NVAPI_REQUEST_USER_TO_CLOSE_NON_MIGRATABLE_APPS"},
			{-130, "NVAPI_OUT_OF_MEMORY"},
			{-131, "NVAPI_WAS_STILL_DRAWING"},
			{-132, "NVAPI_FILE_NOT_FOUND"},
			{-133, "NVAPI_TOO_MANY_UNIQUE_STATE_OBJECTS"},
			{-134, "NVAPI_INVALID_CALL"},
			{-135, "NVAPI_D3D10_1_LIBRARY_NOT_FOUND"},
			{-136, "NVAPI_FUNCTION_NOT_FOUND"},
			{-137, "NVAPI_INVALID_USER_PRIVILEGE"},
			{-138, "NVAPI_EXPECTED_NON_PRIMARY_DISPLAY_HANDLE"},
			{-139, "NVAPI_EXPECTED_COMPUTE_GPU_HANDLE"},
			{-140, "NVAPI_STEREO_NOT_INITIALIZED"},
			{-141, "NVAPI_STEREO_REGISTRY_ACCESS_FAILED"},
			{-142, "NVAPI_STEREO_REGISTRY_PROFILE_TYPE_NOT_SUPPORTED"},
			{-143, "NVAPI_STEREO_REGISTRY_VALUE_NOT_SUPPORTED"},
			{-144, "NVAPI_STEREO_NOT_ENABLED"},
			{-145, "NVAPI_STEREO_NOT_TURNED_ON"},
			{-146, "NVAPI_STEREO_INVALID_DEVICE_INTERFACE"},
			{-147, "NVAPI_STEREO_PARAMETER_OUT_OF_RANGE"},
			{-148, "NVAPI_STEREO_FRUSTUM_ADJUST_MODE_NOT_SUPPORTED"},
			{-149, "NVAPI_TOPO_NOT_POSSIBLE"},
			{-150, "NVAPI_MODE_CHANGE_FAILED"},
			{-151, "NVAPI_D3D11_LIBRARY_NOT_FOUND"},
			{-152, "NVAPI_INVALID_ADDRESS"},
			{-153, "NVAPI_STRING_TOO_SMALL"},
			{-154, "NVAPI_MATCHING_DEVICE_NOT_FOUND"},
			{-155, "NVAPI_DRIVER_RUNNING"},
			{-156, "NVAPI_DRIVER_NOTRUNNING"},
			{-157, "NVAPI_ERROR_DRIVER_RELOAD_REQUIRED"},
			{-158, "NVAPI_SET_NOT_ALLOWED"},
			{-159, "NVAPI_ADVANCED_DISPLAY_TOPOLOGY_REQUIRED"},
			{-160, "NVAPI_SETTING_NOT_FOUND"},
			{-161, "NVAPI_SETTING_SIZE_TOO_LARGE"},
			{-162, "NVAPI_TOO_MANY_SETTINGS_IN_PROFILE"},
			{-163, "NVAPI_PROFILE_NOT_FOUND"},
			{-164, "NVAPI_PROFILE_NAME_IN_USE"},
			{-165, "NVAPI_PROFILE_NAME_EMPTY"},
			{-166, "NVAPI_EXECUTABLE_NOT_FOUND"},
			{-167, "NVAPI_EXECUTABLE_ALREADY_IN_USE"},
			{-168, "NVAPI_DATATYPE_MISMATCH"},
			{-169, "NVAPI_PROFILE_REMOVED"},
			{-170, "NVAPI_UNREGISTERED_RESOURCE"},
			{-171, "NVAPI_ID_OUT_OF_RANGE"},
			{-172, "NVAPI_DISPLAYCONFIG_VALIDATION_FAILED"},
			{-173, "NVAPI_DPMST_CHANGED"},
			{-174, "NVAPI_INSUFFICIENT_BUFFER"},
			{-175, "NVAPI_ACCESS_DENIED"},
			{-176, "NVAPI_MOSAIC_NOT_ACTIVE"},
			{-177, "NVAPI_SHARE_RESOURCE_RELOCATED"},
			{-178, "NVAPI_REQUEST_USER_TO_DISABLE_DWM"},
			{-179, "NVAPI_D3D_DEVICE_LOST"},
			{-180, "NVAPI_INVALID_CONFIGURATION"},
			{-181, "NVAPI_STEREO_HANDSHAKE_NOT_DONE"},
			{-182, "NVAPI_EXECUTABLE_PATH_IS_AMBIGUOUS"},
			{-183, "NVAPI_DEFAULT_STEREO_PROFILE_IS_NOT_DEFINED"},
			{-184, "NVAPI_DEFAULT_STEREO_PROFILE_DOES_NOT_EXIST"},
			{-185, "NVAPI_CLUSTER_ALREADY_EXISTS"},
			{-186, "NVAPI_DPMST_DISPLAY_ID_EXPECTED"},
			{-187, "NVAPI_INVALID_DISPLAY_ID"},
			{-188, "NVAPI_STREAM_IS_OUT_OF_SYNC"},
			{-189, "NVAPI_INCOMPATIBLE_AUDIO_DRIVER"},
			{-190, "NVAPI_VALUE_ALREADY_SET"},
			{-191, "NVAPI_TIMEOUT"},
			{-192, "NVAPI_GPU_WORKSTATION_FEATURE_INCOMPLETE"},
			{-193, "NVAPI_STEREO_INIT_ACTIVATION_NOT_DONE"},
			{-194, "NVAPI_SYNC_NOT_ACTIVE"},
			{-195, "NVAPI_SYNC_MASTER_NOT_FOUND"},
			{-196, "NVAPI_INVALID_SYNC_TOPOLOGY"},
			{-197, "NVAPI_ECID_SIGN_ALGO_UNSUPPORTED"},
			{-198, "NVAPI_ECID_KEY_VERIFICATION_FAILED"},
			{-199, "NVAPI_FIRMWARE_OUT_OF_DATE"},
			{-200, "NVAPI_FIRMWARE_REVISION_NOT_SUPPORTED"},
			{-201, "NVAPI_LICENSE_CALLER_AUTHENTICATION_FAILED"},
			{-202, "NVAPI_D3D_DEVICE_NOT_REGISTERED"},
			{-203, "NVAPI_RESOURCE_NOT_ACQUIRED"},
			{-204, "NVAPI_TIMING_NOT_SUPPORTED"},
			{-205, "NVAPI_HDCP_ENCRYPTION_FAILED"},
			{-206, "NVAPI_PCLK_LIMITATION_FAILED"},
			{-207, "NVAPI_NO_CONNECTOR_FOUND"},
			{-208, "NVAPI_HDCP_DISABLED"},
			{-209, "NVAPI_API_IN_USE"},
			{-210, "NVAPI_NVIDIA_DISPLAY_NOT_FOUND"},
			{-211, "NVAPI_PRIV_SEC_VIOLATION"},
			{-212, "NVAPI_INCORRECT_VENDOR"},
			{-213, "NVAPI_DISPLAY_IN_USE"},
			{-214, "NVAPI_UNSUPPORTED_CONFIG_NON_HDCP_HMD"},
			{-215, "NVAPI_MAX_DISPLAY_LIMIT_REACHED"},
			{-216, "NVAPI_INVALID_DIRECT_MODE_DISPLAY"},
			{-217, "NVAPI_GPU_IN_DEBUG_MODE"},
			{-218, "NVAPI_D3D_CONTEXT_NOT_FOUND"},
			{-219, "NVAPI_STEREO_VERSION_MISMATCH"},
			{-220, "NVAPI_GPU_NOT_POWERED"},
			{-221, "NVAPI_ERROR_DRIVER_RELOAD_IN_PROGRESS"},
			{-222, "NVAPI_WAIT_FOR_HW_RESOURCE"},
			{-223, "NVAPI_REQUIRE_FURTHER_HDCP_ACTION"},
			{-224, "NVAPI_DISPLAY_MUX_TRANSITION_FAILED"} };

		auto it = errors.find(errorCode);
		return it != errors.end() ? it->second : "UNKNOWN_ERROR";
	}

#define LOG_NVAPI_INFO(message) LOG_INFO(GetModuleTag() + message);
#define LOG_NVAPI_WARNING(message) LOG_WARNING(GetModuleTag() + message);
#define LOG_NVAPI_DEBUG(message) LOG_DEBUG(GetModuleTag() + message);
#define LOG_NVAPI_ERROR(message) LOG_ERROR(GetModuleTag() + message);

#pragma intrinsic(_ReturnAddress)
	static wstring WhoIsTheCaller(void* returnAddress)
	{
		HMODULE hModule = NULL;
		char callerPath[MAX_PATH] = { 0 };

		// Get the base address of the module containing the return address.
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)returnAddress, &hModule))
		{
			GetModuleFileNameA(hModule, callerPath, sizeof(callerPath));
			auto path = std::filesystem::path(callerPath);

			return path.filename().wstring();

			//LOG_WARNING(L"CALLED BY " + path2);
			//return Common::GetModuleHandle() == hModule;
		}

		return L"";
	}

	static std::wstring LuidToHex(LUID luid)
	{
		// Convert LUID to hexadecimal string
		std::wstringstream ss;
		if (luid.HighPart > 0) {
			ss << std::hex << std::setw(8) << std::setfill(L'0') << luid.HighPart << std::setw(8) << std::setfill(L'0') << luid.LowPart;
		}
		else {
			ss << std::hex << std::setw(8) << std::setfill(L'0') << luid.LowPart;
		}

		return ss.str();
	}

	static void LogResult(const wchar_t* functionName, NvAPI_Status result)
	{
		static size_t prefixLength = wcslen(L"NVAPI::");
		if (wcsncmp(functionName, L"NVAPI::", prefixLength) == 0) {
			functionName += prefixLength;
		}
		std::wstringstream hexStream;
		hexStream << L"0x" << std::hex << result;
		std::wstring hexString = hexStream.str();
		if (result == NVAPI_OK) {
			LOG_NVAPI_INFO(std::wstring(functionName) + L": succeeded");
		}
		else {
			std::string errorName = GetErrorMessage(result);
			LOG_NVAPI_ERROR(L"" + std::wstring(functionName) + L": failed: (" + std::wstring(errorName.begin(), errorName.end()) + L")");
		}
	}

	static void LogCall(const wchar_t* functionName)
	{
		static size_t prefixLength = wcslen(L"NVAPI::");
		if (wcsncmp(functionName, L"NVAPI::", prefixLength) == 0) {
			functionName += prefixLength;
		}

		LOG_NVAPI_INFO(std::wstring(functionName));
	}

#define LOG_NVAPI_FUNCTION_CALL_AND_RETURN(result) \
    do \
    { \
        if (ctx.logging.isExtraDebugEnabled || result != NVAPI_OK) { \
            LogResult(__FUNCTIONW__, result); \
        } \
        return result; \
    } while (0)

#define LOG_NVAPI_FUNCTION_CALL() \
    if (ctx.logging.isExtraDebugEnabled) { \
        LogCall(__FUNCTIONW__); \
    }

	unsigned int const __forceinline GetReflexFrameTimeDuration()
	{
		unsigned int sleep = 0;

		if (ctx.reflex.realFpsLimit == 0.0f) {
			return 0;
		}

		double currentReflexFpsLimitTmp = ctx.reflex.realFpsLimit;

		if (ctx.ngx.isFrameGenerationActive && !ctx.isRunningUnderWindows) {
			currentReflexFpsLimitTmp = ((double)ctx.reflex.desiredFpsLimit) / (ctx.ngx.framesGenerated + 1); // Only real Reflex is aware of extra frames generated by DLSSG, Linux code is not
		}

		//if (ctx.ngx.lastEvaluationTimeMsec > 0.0f) {
		//	if (currentTimeMsec > ctx.ngx.lastEvaluationTimeMsec + 1000.0f) {
		//		LOG_WARNING(L"[RLFX] DLSSG stall detected");
		//		ctx.reflex.isReset = true;
		//		ctx.ngx.lastEvaluationTimeMsec = 0.0f;
		//	}
		//	else 
		//	if (!ctx.ngx.isDynamicFrameGenerationEnabled && !ctx.isRunningUnderWindows) {
		//		currentReflexFpsLimitTmp = ((double)ctx.reflex.desiredFpsLimit) / 2.0f; // Only real Reflex is aware of extra frames generated by DLSSG, Linux code is not
		//	}
		//}

		double frameDurationSeconds = 1.0 / currentReflexFpsLimitTmp;

		// Convert frame duration from seconds to microseconds
		sleep = static_cast<unsigned int>(frameDurationSeconds * 1000000);

		return sleep;
	}

	static unsigned int GetEmulatedReflexFrameTimeDuration()
	{
		// DLSSG.NotRenderingGameFrames
		// DLSSG.EnableInterp
		// DLSSG.IsRecording
		double latencyTarget;
		static bool dirtyFrameDetected = false;
		static std::deque<double> latencyHistory;
		static double lastFrametime = 0.0f;

		double latency = 0;

		if (_lastFrameTime == 0.0f) {
			_lastFrameTime = currentTimeMsec;
		}

		if (!OverdriveController::GetDynamicFrameGenerationEnabled()) {
			ctx.reflex.realFpsLimit = (double)ctx.reflex.desiredFpsLimit;
		}

		// check if game stopped evaluating DLSSG (if enabled)
		if (ctx.ngx.lastEvaluationTimeMsec > 0.0f) {
			if (currentTimeMsec > ctx.ngx.lastEvaluationTimeMsec + 1000.0f) {
				LOG_WARNING(L"[RLFX] DLSSG stall detected");
				ctx.reflex.isReset = true;
				ctx.ngx.lastEvaluationTimeMsec = 0.0f;
			}

			if (!OverdriveController::GetDynamicFrameGenerationEnabled() && (ctx.nvapi.isMockEnabled || ctx.nvapi.isProxyLoaded)) {
				ctx.reflex.realFpsLimit = ((double)ctx.reflex.desiredFpsLimit) / 2; // Only real Reflex is aware of extra frames generated by DLSSG
			}
		}

		if (ctx.reflex.isReset) {
			LOG_INFO(L"[RLFX] Reflex stats restarted");
			ctx.reflex.isReset = false;
			ctx.reflex.potentialFps = 0;
			_lastFrameTime = currentTimeMsec;
			if (latencyHistory.size() > 0) {
				latencyHistory.erase(latencyHistory.begin(), latencyHistory.end());
			}

			reflexSleep = 0.0f;
		}

		//LOG_WARNING(L"[RLFX] " + std::to_wstring(_lastFrameTime) + L" <> " + std::to_wstring(currentTimeMsec));
		latency = currentTimeMsec - _lastFrameTime;
		// try to guestimate the potential FPS without relying on FG...
		auto frameTime = latency - reflexSleep;
		if (frameTime <= 0.0f) {
			frameTime = 0.01f;
		}
		ctx.reflex.potentialFps = static_cast<unsigned int>(1000.0f / frameTime);
		ctx.reflex.timeFrameDeltaMsec = frameTime;

		_lastFrameTime = currentTimeMsec;

		if (latency > 1000.0f) {
			latency = 0.0f;
			reflexSleep = 0.0f;
		}

		// use compensation only if it applies to given frame type
		if (reflexSleep > 0.0f) {
			latency -= reflexSleep;
		}

		// -----------
		// try to detect duplicated frame....
		if (ctx.ngx.isDuplicatingFrames) {
			frameTime *= 2;
		}

		if (frameTime > 0.0f) {
			if (ctx.ngx.frametimeHistory.size() >= 1024) {
				ctx.ngx.frametimeHistory.pop_back();
			}

			ctx.ngx.frametimeHistory.push_front(frameTime);
		}

		size_t frames = 0;

		double sum = 0.0;
		for (double value : ctx.ngx.frametimeHistory) {
			frames++;
			sum += value;
			if (sum > 2000.0f) {
				break;
			}
		}

		if (ctx.ngx.frametimeHistory.size() > frames + 1) {
			ctx.ngx.frametimeHistory.erase(ctx.ngx.frametimeHistory.begin() + frames + 1);
		}

		//-----------------

		if (latency > 0.0f) {
			if (latencyHistory.size() >= 320) {
				latencyHistory.pop_back();
			}

			latencyHistory.push_front(latency);
		}

		frames = 0;
		if (latencyHistory.size() < 10 && ctx.reflex.realFpsLimit <= 0.0f) {
			return 0;
		}

		if (ctx.reflex.realFpsLimit == 0.0f) {
			double sum = 0.0;
			for (double value : latencyHistory) {
				frames++;
				sum += value;
				if (sum > 2000.0f) {
					break;
				}
			}

			if (latencyHistory.size() > frames + 1) {
				latencyHistory.erase(latencyHistory.begin() + frames + 1);
			}

			std::deque<double> sortedQueue = latencyHistory;

			std::sort(sortedQueue.begin(), sortedQueue.end());

			// remove dirty frames from the history
			if (latency > sortedQueue[(long)(0.95f * sortedQueue.size())]) {
				if (dirtyFrameDetected) {
					dirtyFrameDetected = false;
				}
				else {
					dirtyFrameDetected = true;
					if (latencyHistory.size() > 0) {
						latencyHistory.pop_front();
					}
				}
			}
			else {
				dirtyFrameDetected = false;
			}

			// Calculate the index of the 70th percentile value
			size_t index = static_cast<size_t>(0.70 * sortedQueue.size());

			latencyTarget = sortedQueue[index];
		}
		else {
			latencyTarget = 1000.0f / ctx.reflex.realFpsLimit;
		}

		double margin = ctx.reflex.realFpsLimit ? 0.0f : 0.1f;
		// check if we rendered thing prematurely
		if (latency < latencyTarget - latencyTarget * margin) {
			reflexSleep = latencyTarget - latency;
			reflexSleep -= reflexSleep * margin;
		}
		else {
			reflexSleep = 0.0f;
		}

		if (latency > 100.0f) {
			LOG_WARNING(L"[RLFX] Too low framerate, disabling compensator");
			reflexSleep = 0.0f;
			latencyHistory.pop_front();
		}

		unsigned int microseconds = static_cast<unsigned int>(reflexSleep * 1000);

		return microseconds;
	}

	NvAPI_Status __cdecl NvAPI_D3D12_SetAsyncFrameMarker(ID3D12CommandQueue* pCommandQueue, NV_ASYNC_FRAME_MARKER_PARAMS* pSetAsyncFrameMarkerParams)
	{
		static bool logged = false;

		if (!logged) {
			LOG_NVAPI_FUNCTION_CALL();
		}

		ReflexEvents::DispatchPreSetAsyncFrameMarker(pCommandQueue, pSetAsyncFrameMarkerParams->frameID, pSetAsyncFrameMarkerParams->markerType);

		bool useNativeReflex = OverdriveController::GetReflexEnabled();

		//if (!ctx.reflex.isEmulationEnabled && !ctx.enableReflexInjection) {
		if (org_NvAPI_D3D12_SetAsyncFrameMarker && useNativeReflex) {
			auto result = org_NvAPI_D3D12_SetAsyncFrameMarker(pCommandQueue, pSetAsyncFrameMarkerParams);
			if (!logged) {
				logged = true;
				ReflexEvents::DispatchPostSetAsyncFrameMarker(pCommandQueue, pSetAsyncFrameMarkerParams->frameID, pSetAsyncFrameMarkerParams->markerType, result);
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(result);
			}

			ReflexEvents::DispatchPostSetAsyncFrameMarker(pCommandQueue, pSetAsyncFrameMarkerParams->frameID, pSetAsyncFrameMarkerParams->markerType, result);
			return result;
		}
		//}

		if (logged) {
			ReflexEvents::DispatchPostSetAsyncFrameMarker(pCommandQueue, pSetAsyncFrameMarkerParams->frameID, pSetAsyncFrameMarkerParams->markerType, NVAPI_OK);
			return NVAPI_OK;
		}

		logged = true;
		ReflexEvents::DispatchPostSetAsyncFrameMarker(pCommandQueue, pSetAsyncFrameMarkerParams->frameID, pSetAsyncFrameMarkerParams->markerType, NVAPI_OK);

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	unsigned int reflexMinimumIntervalUsSetByApp = 0;

	NvAPI_Status __cdecl NvAPI_D3D_SetSleepMode(void* pDevice, NV_SET_SLEEP_MODE_PARAMS* pSetSleepModeParams)
	{
		NvAPI_Status result = NVAPI_OK;

		// DISPATCH PRE-EVENT
		ReflexEvents::DispatchPreSetSleepMode(pDevice, pSetSleepModeParams);

		static bool logged = false;


		ctx.reflex.isMarkersOptimizationEnabled = pSetSleepModeParams->bUseMarkersToOptimize;
		ctx.reflex.isBoostOriginallyEnabled = pSetSleepModeParams->bLowLatencyBoost;
		ctx.reflex.isOriginallyEnabled = pSetSleepModeParams->bLowLatencyMode;

		reflexMinimumIntervalUsSetByApp = pSetSleepModeParams->minimumIntervalUs;
		unsigned int sleep = reflexMinimumIntervalUsSetByApp;
		float divider = 1.0f;
		if (ctx.ngx.lastEvaluationTimeMsec > 0.0f && !ctx.nvapi.isGenuineFileLoaded) {
			divider = (ctx.ngx.framesGenerated + 1); // Only real Reflex is aware of extra frames generated by DLSSG, Linux code is not
			sleep *= divider;
		}

		//LOG_WARNING(L"DIVIDING BY " + std::to_wstring(divider));

		pSetSleepModeParams->minimumIntervalUs = ctx.reflex.isFpsLimitEnabled ? GetReflexFrameTimeDuration() : sleep;
		if (!logged) {
			LOG_NVAPI_FUNCTION_CALL();
			if (OverdriveController::GetBoostOverriden()) {
				pSetSleepModeParams->bLowLatencyBoost = OverdriveController::GetBoostEnabled();
				LOG_NVAPI_INFO(L"Overriding Reflex Boost");
			}
		}

		result = org_NvAPI_D3D_SetSleepMode(pDevice, pSetSleepModeParams);
		
		// DISPATCH POST-EVENT
		ReflexEvents::DispatchPostSetSleepMode(pDevice, pSetSleepModeParams, result);

		if (logged) {
			return result;
		}

		logged = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(result);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetArchInfo(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_ARCH_INFO* pGpuArchInfo)
	{
		LOG_NVAPI_FUNCTION_CALL();
		auto caller = WhoIsTheCaller(_ReturnAddress());
		LOG_DEBUG(L"[NVAPI] NvAPI_GPU_GetArchInfo: Called by: " + caller);

		if (caller == L"NvPresent64.dll") {
			pGpuArchInfo->architecture = NV_GPU_ARCHITECTURE_GF100;
			pGpuArchInfo->architecture_id = NV_GPU_ARCHITECTURE_GF100;
			pGpuArchInfo->implementation = 0x4;
			pGpuArchInfo->implementation_id = NV_GPU_ARCH_IMPLEMENTATION_AD104;
			pGpuArchInfo->revision = NV_GPU_CHIP_REV_A03;
			pGpuArchInfo->revision_id = NV_GPU_CHIP_REV_A03;
			LOG_WARNING(L"[NVAPI] NvAPI_GPU_GetArchInfo: Disabling SmoothMotion technology");
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
		}
		ctx.nvapi.isRealHardwareDetected = true;
		static bool isArchFakedAlready = false;
		auto fakeArch = ctx.currentGpuArchitecture;

		if (ctx.nvapi.isEmbeddedNvapiUsed) {
			std::lock_guard<std::mutex> lock(nvapiMutex);
			PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
			if (!e) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			DXGI_ADAPTER_DESC1 desc;
			LOG_NVAPI_DEBUG(L"Getting arch for LUID: " + LuidToHex(e->desc.AdapterLuid));
			if (GPU::GetPreferredGPU(&desc)) {
				// make sure to return incompatible arch for non-preferred GPU (just in case some games ignore that)
				if (LuidToHex((&desc)->AdapterLuid) != LuidToHex(e->desc.AdapterLuid)) {
					fakeArch = NV_GPU_ARCHITECTURE_GF100;
					LOG_NVAPI_DEBUG(L"Non-preferred GPU, lowering arch to GF100");
				}
				else {
					LOG_NVAPI_DEBUG(L"Preferred GPU, returning proper arch");
				}
			}
		}

		static NvPhysicalGpuHandle lastGpuReported = nullptr;
		if (!ctx.nvapi.isEmbeddedNvapiUsed && org_NvAPI_GPU_GetArchInfo) {
			auto result = org_NvAPI_GPU_GetArchInfo(hPhysicalGpu, pGpuArchInfo);
			if (result == NVAPI_OK) {
				ctx.realGpuArchitecture = ctx.nvapi.isEmbeddedNvapiUsed ? NV_GPU_ARCHITECTURE_GF100 : pGpuArchInfo->architecture_id;
			}
			//ctx.realGpuArchitecture = NV_GPU_ARCHITECTURE_AD100;
			ctx.nvapi.isInitialized = true;
			if (lastGpuReported == nullptr) {
				
				std::wstring arch = ctx.realGpuArchitecture == NV_GPU_ARCHITECTURE_GB200 ? L"blackwell" : (ctx.realGpuArchitecture == NV_GPU_ARCHITECTURE_AD100 ? L"ada" : (ctx.realGpuArchitecture == NV_GPU_ARCHITECTURE_GA100 ? L"ampere" : L"turing or below"));

				LOG_NVAPI_INFO(L"NvAPI_GPU_GetArchInfo: Actual architecture is " + arch + L" (" + std::to_wstring(ctx.realGpuArchitecture) + L")");
				lastGpuReported = hPhysicalGpu;

				if (ctx.realGpuArchitecture >= NV_GPU_ARCHITECTURE_AD100 || ctx.nvapi.isHighestArchEnabled) {
					ctx.ngx.isDlssgSupportedByHardware = true;
					ctx.ngx.isDlssgMultiframeSupported = ctx.realGpuArchitecture > NV_GPU_ARCHITECTURE_AD100;
					if (ctx.realGpuArchitecture == NV_GPU_ARCHITECTURE_AD100 && !ctx.ngx.isDlssgDisabled) {
						ctx.ngx.isHybridMfgEnabled = true;
					}

					if (ctx.ngx.isHybridMfgForced) {
						ctx.ngx.isHybridMfgEnabled = true;
					}
					//ctx.ngx.isHybridMfgEnabled = true;
				}
			}
		}

		if (ctx.nvapi.isEmbeddedNvapiUsed || !org_NvAPI_GPU_GetArchInfo) {
			pGpuArchInfo->architecture = NV_GPU_ARCHITECTURE_AD100;
			pGpuArchInfo->architecture_id = NV_GPU_ARCHITECTURE_AD100;
			pGpuArchInfo->implementation = 0x4;
			pGpuArchInfo->implementation_id = NV_GPU_ARCH_IMPLEMENTATION_AD104;
			pGpuArchInfo->revision = NV_GPU_CHIP_REV_A03;
			pGpuArchInfo->revision_id = NV_GPU_CHIP_REV_A03;
		}

		if (!isArchFakedAlready) {
			//isArchFakedAlready = true;
			std::wstring arch = (pGpuArchInfo->architecture == NV_GPU_ARCHITECTURE_AD100 ? L"ada" : (pGpuArchInfo->architecture == NV_GPU_ARCHITECTURE_GA100 ? L"ampere" : L"turing"));
			static std::wstring lastArch = L"";
			if (lastArch != arch) {
				lastArch = arch;
				LOG_NVAPI_DEBUG(L"NvAPI_GPU_GetArchInfo: Setting architecture to " + arch);
			}
		}

		if (ctx.nvapi.isHighestArchEnabled) {
			pGpuArchInfo->architecture = NV_GPU_ARCHITECTURE_GB200;
			pGpuArchInfo->architecture_id = NV_GPU_ARCHITECTURE_GB200;
		}

		InitializeDlssgHooks();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GetInterfaceVersionString(NvAPI_ShortString desc)
	{
		LOG_NVAPI_FUNCTION_CALL();

		strcpy_s(desc, sizeof(NvAPI_ShortString), "DLSS Enabler");
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Success()
	{
		return NVAPI_OK;
	}

	NvAPI_Status __cdecl NvAPI_NotSupported()
	{
		return NVAPI_NO_IMPLEMENTATION;
	}

	NvAPI_Status __cdecl NvAPI_D3D12_SetRawScgPriority(NV_SCG_PRIORITY_INFO* PriorityInfo)
	{
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Disp_GetHdrCapabilities(NvU32 displayId, NV_HDR_CAPABILITIES* pHdrCapabilities)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!pHdrCapabilities) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		pHdrCapabilities->isDolbyVisionSupported = 0;
		pHdrCapabilities->isEdrSupported = 0;
		pHdrCapabilities->isTraditionalHdrGammaSupported = 0;
		pHdrCapabilities->isHdr10PlusGamingSupported = 0;
		pHdrCapabilities->isHdr10PlusSupported = 0;
		pHdrCapabilities->isTraditionalSdrGammaSupported = 0;
		pHdrCapabilities->isST2084EotfSupported = 0;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DISP_GetDisplayConfig(NvU32* pathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pathInfo)
	{
		LOG_NVAPI_FUNCTION_CALL();

		LOG_NVAPI_WARNING(L"Path Info Count: " + (!pathInfoCount ? L"null" : std::to_wstring(*pathInfoCount)));
		LOG_NVAPI_WARNING(L"Path Info: " + (!pathInfo ? L"null" : L"present"));

		if (pathInfo) {
			LOG_NVAPI_WARNING(L"Path Info: " + (!pathInfo ? L"null" : L"present"));
		}

		if (!pathInfoCount || (*pathInfoCount == 0 && pathInfo) || (*pathInfoCount != 0 && pathInfo == nullptr)) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		// first pass
		if (pathInfoCount && pathInfo == nullptr) {
			*pathInfoCount = physicalGpus.size();
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
		}

		if (*pathInfoCount > 0) {
			*pathInfoCount = min(physicalGpus.size(), *pathInfoCount);
			for (int i = 0; i < *pathInfoCount; i++) {
				NV_DISPLAYCONFIG_PATH_INFO& p = pathInfo[i];
				NvU32 ver = p.version;
				//memset(&p, 0, sizeof(p));
				p.version = ver;
				p.targetInfoCount = 0;
				auto e = &physicalGpus[i];

				// check all outputs
				for (UINT outputIndex = 0; ; outputIndex++) {
					IDXGIOutput* output = nullptr;
					if (FAILED(e->adapter->EnumOutputs(outputIndex, &output))) {
						if (outputIndex == 0) {
							LOG_NVAPI_DEBUG(L" No outputs");
						}
						break; // no more outputs
					}

					DXGI_OUTPUT_DESC desc = {};
					if (SUCCEEDED(output->GetDesc(&desc))) {
						LOG_NVAPI_DEBUG(L" Found display ID: " + desc.DeviceName);
						p.targetInfoCount++;

						if (p.targetInfo != nullptr) {
							LOG_NVAPI_DEBUG(L" - Returning Target Info");
							p.targetInfo[outputIndex].displayId = ((i + 1) * NVAPI_DISPLAY_ID_SPACE) + outputIndex;
						}
						if (p.targetInfo != nullptr && p.targetInfo[outputIndex].details != nullptr) {
							DEVMODEW dm = {};
							dm.dmSize = sizeof(dm);

							if (EnumDisplaySettings(desc.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
								p.targetInfo[outputIndex].details->refreshRate1K = dm.dmDisplayFrequency * 1000;
							}

							switch (desc.Rotation) {
							case DXGI_MODE_ROTATION_ROTATE90:
								p.targetInfo[outputIndex].details->rotation = NV_ROTATE_90;
								break;
							case DXGI_MODE_ROTATION_ROTATE180:
								p.targetInfo[outputIndex].details->rotation = NV_ROTATE_180;
								break;
							case DXGI_MODE_ROTATION_ROTATE270:
								p.targetInfo[outputIndex].details->rotation = NV_ROTATE_270;
								break;
							default:
								p.targetInfo[outputIndex].details->rotation = NV_ROTATE_0;
							}

							LOG_NVAPI_DEBUG(L" - DXGI Rotation: " + std::to_wstring(desc.Rotation));

							p.targetInfo[outputIndex].details->scaling = NV_SCALING_GPU_SCALING_TO_NATIVE;
							p.targetInfo[outputIndex].details->timingOverride = NV_TIMING_OVERRIDE_CURRENT; // @todo: use EDID for actual data
							// @todo: add support for TVs
							p.targetInfo[outputIndex].details->connector = NVAPI_GPU_CONNECTOR_UNKNOWN;
							p.targetInfo[outputIndex].details->tvFormat = NV_DISPLAY_TV_FORMAT_NONE;
							p.targetInfo[outputIndex].details->interlaced = 0;

							// @todo: fill this with actual data
							p.targetInfo[outputIndex].details->disableVirtualModeSupport = 0;
							p.targetInfo[outputIndex].details->isPreferredUnscaledTarget = 0;
							p.targetInfo[outputIndex].details->primary = i == 0 && outputIndex == 0 ? 1 : 0;

							LOG_NVAPI_DEBUG(L" - Screen Refresh Rate: " + std::to_wstring(dm.dmDisplayFrequency));
							auto rotation = p.targetInfo[outputIndex].details->rotation;
							LOG_NVAPI_DEBUG(L" - Screen Rotation: " + (rotation == NV_ROTATE_0 ? L"0" : (rotation == NV_ROTATE_90 ? L"90" : (rotation == NV_ROTATE_180 ? L"180" : L"270"))));
						}

						if (p.sourceModeInfo != nullptr) {
							LOG_NVAPI_DEBUG(L" - Returning Source Mode Info");

							MONITORINFOEX mi = {};
							mi.cbSize = sizeof(mi);

							if (GetMonitorInfo(desc.Monitor, &mi)) {
								bool isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
								p.sourceModeInfo->bGDIPrimary = isPrimary ? 1 : 0;
							}
							else {
								p.sourceModeInfo->bGDIPrimary = 0;
							}

							// DXGI would do too, but required data for color depth is supported by Win10 onwards, so we use GDI instead...
							HDC hdc = CreateDC(desc.DeviceName, nullptr, nullptr, nullptr);
							int bits = GetDeviceCaps(hdc, BITSPIXEL);
							DeleteDC(hdc);

							NV_POSITION pos = {};
							NV_RESOLUTION res = {};
							res.width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
							res.height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
							switch (bits) {
							case 30: res.colorDepth = 30; break;  // 10 bpc HDR
							case 32: res.colorDepth = 24; break;  // 8 bpc SDR
							case 24: res.colorDepth = 24; break;  // 8 bpc without alpha
							case 16: res.colorDepth = 16; break;
							default: res.colorDepth = 24; break;
							}
							pos.x = desc.DesktopCoordinates.left; pos.y = desc.DesktopCoordinates.top;
							p.sourceModeInfo->bSLIFocus = 0;
							p.sourceModeInfo->position = pos; // should be 0,0 according to NVAPI specification
							p.sourceModeInfo->colorFormat = NV_FORMAT_UNKNOWN; // as required by NVAPI specification
							p.sourceModeInfo->spanningOrientation = NV_DISPLAYCONFIG_SPAN_NONE; // supported only on XP?

							LOG_NVAPI_DEBUG(L" - Screen width: " + std::to_wstring(res.width) + L", height: " + std::to_wstring(res.height) + L", depth: " + std::to_wstring(res.colorDepth));
							LOG_NVAPI_DEBUG(L" - Screen X: " + std::to_wstring(pos.x) + L", Y: " + std::to_wstring(pos.y));
							LOG_NVAPI_DEBUG(L" - GDI Primary: " + std::to_wstring(p.sourceModeInfo->bGDIPrimary));
						}
					}
					output->Release();
				}

				if (p.version == NV_DISPLAYCONFIG_PATH_INFO_VER2) {
					p.IsNonNVIDIAAdapter = 0;
					p.pOSAdapterID = nullptr; // LUID shouldn't be filled in for NVIDIA GPUs
					//if (p.pOSAdapterID) {
					//	memcpy(p.pOSAdapterID, &e->desc.AdapterLuid, sizeof(LUID));
					//}
				}
			}
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetConnectedDisplayIds(NvPhysicalGpuHandle handle, NV_GPU_DISPLAYIDS* displayIds, NvU32* displayCount, NvU32 flags)
	{
		LOG_NVAPI_FUNCTION_CALL();
		*displayCount = 0;  // return actual number, 0 will break things
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetFullName(NvPhysicalGpuHandle hPhysicalGpu, NvAPI_ShortString szName)
	{
		LOG_NVAPI_FUNCTION_CALL();
		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		int written = WideCharToMultiByte(
			CP_ACP,                         // ANSI
			0,
			e->desc.Description,
			-1,                             // null-terminated
			szName,
			sizeof(NvAPI_ShortString) - 1,  // room for \0
			nullptr,
			nullptr
		);

		szName[sizeof(NvAPI_ShortString) - 1] = '\0';

		if (written == 0) {
			strcpy_s(szName, sizeof(NvAPI_ShortString), "Unknown GPU");
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetGpuCoreCount(NvPhysicalGpuHandle hPhysicalGpu, NvU32* pCount)
	{
		LOG_NVAPI_FUNCTION_CALL();
		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pCount = 1;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetPstates20(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATES20_INFO* pPstatesInfo)
	{
		LOG_NVAPI_FUNCTION_CALL();
		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		if (pPstatesInfo == nullptr) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ERROR);
		}

		// Initialize the structure with mock data
		pPstatesInfo->version = NV_GPU_PERF_PSTATES20_INFO_VER;
		pPstatesInfo->numPstates = 2;  // Example: 2 P-states
		pPstatesInfo->numClocks = 3;
		pPstatesInfo->numBaseVoltages = 1;
		pPstatesInfo->ov.numVoltages = 0;


		// Fill mock data for P-state 0
		pPstatesInfo->pstates[0].pstateId = NVAPI_GPU_PERF_PSTATE_P0;
		pPstatesInfo->pstates[0].bIsEditable = false;
		pPstatesInfo->pstates[0].clocks[0].domainId = NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS;
		pPstatesInfo->pstates[0].clocks[0].freqDelta_kHz.value = 1000; // 1 MHz
		pPstatesInfo->pstates[0].clocks[0].freqDelta_kHz.valueRange.min = 800;
		pPstatesInfo->pstates[0].clocks[0].freqDelta_kHz.valueRange.max = 1200;
		pPstatesInfo->pstates[0].clocks[1].domainId = NVAPI_GPU_PUBLIC_CLOCK_MEMORY;
		pPstatesInfo->pstates[0].clocks[1].freqDelta_kHz.value = 1000; // 1 MHz
		pPstatesInfo->pstates[0].clocks[1].freqDelta_kHz.valueRange.min = 800;
		pPstatesInfo->pstates[0].clocks[1].freqDelta_kHz.valueRange.max = 1200;
		pPstatesInfo->pstates[0].clocks[2].domainId = NVAPI_GPU_PUBLIC_CLOCK_VIDEO;
		pPstatesInfo->pstates[0].clocks[2].freqDelta_kHz.value = 1000; // 1 MHz
		pPstatesInfo->pstates[0].clocks[2].freqDelta_kHz.valueRange.min = 800;
		pPstatesInfo->pstates[0].clocks[2].freqDelta_kHz.valueRange.max = 1200;
		pPstatesInfo->pstates[0].baseVoltages[0].volt_uV = 1000000; // 1V
		pPstatesInfo->pstates[0].baseVoltages[0].bIsEditable = false;
		pPstatesInfo->pstates[0].baseVoltages[0].voltDelta_uV.value = 1000;
		pPstatesInfo->pstates[0].baseVoltages[0].voltDelta_uV.valueRange.min = 0;
		pPstatesInfo->pstates[0].baseVoltages[0].voltDelta_uV.valueRange.max = 0;

		// Fill mock data for P-state 1
		pPstatesInfo->pstates[1].pstateId = NVAPI_GPU_PERF_PSTATE_P1;
		pPstatesInfo->pstates[1].bIsEditable = false;
		pPstatesInfo->pstates[1].clocks[0].domainId = NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS;
		pPstatesInfo->pstates[1].clocks[0].freqDelta_kHz.value = 1000; // 1 MHz
		pPstatesInfo->pstates[1].clocks[0].freqDelta_kHz.valueRange.min = 800;
		pPstatesInfo->pstates[1].clocks[0].freqDelta_kHz.valueRange.max = 1200;
		pPstatesInfo->pstates[1].clocks[1].domainId = NVAPI_GPU_PUBLIC_CLOCK_MEMORY;
		pPstatesInfo->pstates[1].clocks[1].freqDelta_kHz.value = 1000; // 1 MHz
		pPstatesInfo->pstates[1].clocks[1].freqDelta_kHz.valueRange.min = 800;
		pPstatesInfo->pstates[1].clocks[1].freqDelta_kHz.valueRange.max = 1200;
		pPstatesInfo->pstates[1].clocks[2].domainId = NVAPI_GPU_PUBLIC_CLOCK_VIDEO;
		pPstatesInfo->pstates[1].clocks[2].freqDelta_kHz.value = 1000; // 1 MHz
		pPstatesInfo->pstates[1].clocks[2].freqDelta_kHz.valueRange.min = 800;
		pPstatesInfo->pstates[1].clocks[2].freqDelta_kHz.valueRange.max = 1200;
		pPstatesInfo->pstates[1].baseVoltages[0].volt_uV = 1000000; // 1V
		pPstatesInfo->pstates[1].baseVoltages[0].bIsEditable = false;
		pPstatesInfo->pstates[1].baseVoltages[0].voltDelta_uV.value = 1000;
		pPstatesInfo->pstates[1].baseVoltages[0].voltDelta_uV.valueRange.min = 0;
		pPstatesInfo->pstates[1].baseVoltages[0].voltDelta_uV.valueRange.max = 0;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetAllClockFrequencies(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_CLOCK_FREQUENCIES* pClkFreqs)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (pClkFreqs == nullptr) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		NvU32 ver = NV_STRUCT_VERSION(pClkFreqs->version);
		if (ver < 1 || ver > 3) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INCOMPATIBLE_STRUCT_VERSION);
		}

		if (pClkFreqs->ClockType != static_cast<unsigned int>(NV_GPU_CLOCK_FREQUENCIES_CURRENT_FREQ)) {
			//LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NOT_SUPPORTED);
		}

		// Reset all clock data for all domains
		for (auto& domain : pClkFreqs->domain) {
			domain.bIsPresent = 0;
			domain.frequency = 0;
		}

		unsigned int clock = 1600 * 1000;

		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].bIsPresent = 1;
		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].frequency = clock;

		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_MEMORY].bIsPresent = 1;
		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_MEMORY].frequency = clock;

		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_VIDEO].bIsPresent = 1;
		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_VIDEO].frequency = clock * 1000;

		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_PROCESSOR].bIsPresent = 1;
		pClkFreqs->domain[NVAPI_GPU_PUBLIC_CLOCK_PROCESSOR].frequency = clock * 1000;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetMemoryInfoEx(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_MEMORY_INFO_EX* pMemoryInfo)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pMemoryInfo) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		auto memory = *pMemoryInfo;
		memory.dedicatedVideoMemoryEvictionCount = 0;
		memory.dedicatedVideoMemoryEvictionsSize = 0;
		memory.dedicatedVideoMemoryPromotionCount = 0;
		memory.dedicatedVideoMemoryPromotionsSize = 0;
		memory.availableDedicatedVideoMemory = e->desc.DedicatedVideoMemory;
		memory.sharedSystemMemory = e->desc.SharedSystemMemory;

		ComPtr<IDXGIAdapter3> adapter3;
		if (SUCCEEDED(e->adapter->QueryInterface(IID_PPV_ARGS(&adapter3)))) {
			DXGI_QUERY_VIDEO_MEMORY_INFO vmLocal = {};
			if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(
				0,
				DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
				&vmLocal)))
			{
				//memory.desiredDedicatedVideoMemory = vmLocal.Budget;
				memory.availableDedicatedVideoMemory = vmLocal.Budget; // "full budget"
				memory.curAvailableDedicatedVideoMemory =
					(vmLocal.Budget > vmLocal.CurrentUsage)
					? (vmLocal.Budget - vmLocal.CurrentUsage)
					: 0;
			}

			DXGI_QUERY_VIDEO_MEMORY_INFO vmNonLocal = {};
			if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(
				0,
				DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,
				&vmNonLocal)))
			{
				// @todo: use it or not?
			}

			adapter3->Release();
		}

		LOG_NVAPI_DEBUG(L"Memory details for GPU with LUID: " + LuidToHex(e->desc.AdapterLuid));
		LOG_NVAPI_DEBUG(L" - Shared system memory: " + std::to_wstring((int)(memory.sharedSystemMemory / 1048576)) + L"MB");
		LOG_NVAPI_DEBUG(L" - Available dedicated video memory: " + std::to_wstring((int)(memory.availableDedicatedVideoMemory / 1048576)) + L"MB");
		LOG_NVAPI_DEBUG(L" - Current available dedicated video memory: " + std::to_wstring((int)(memory.curAvailableDedicatedVideoMemory / 1048576)) + L"MB");
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DISP_GetDisplayIdByDisplayName(const char* displayName, NvU32* displayId)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (displayName == nullptr || displayId == nullptr || strlen(displayName) == 0) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		wchar_t dxgiDisplayName[64];
		swprintf(dxgiDisplayName, 64, L"%hs", displayName);

		for (int i = 0; i < physicalGpus.size(); i++) {
			auto e = &physicalGpus[i];

			for (UINT outputIndex = 0; ; outputIndex++) {
				IDXGIOutput* output = nullptr;
				if (FAILED(e->adapter->EnumOutputs(outputIndex, &output))) {
					LOG_NVAPI_DEBUG(L" - No outputs");
					break; // no more outputs
				}

				DXGI_OUTPUT_DESC desc = {};
				if (SUCCEEDED(output->GetDesc(&desc))) {
					LOG_NVAPI_DEBUG(L" Found display ID: " + desc.DeviceName);
					if (_wcsicmp(desc.DeviceName, dxgiDisplayName) == 0) {
						*displayId = ((i + 1) * NVAPI_DISPLAY_ID_SPACE) + outputIndex;
						output->Release();
						LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
					}
				}
				output->Release();
			}
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NVIDIA_DISPLAY_NOT_FOUND);
	}

	NvAPI_Status __cdecl NvAPI_Stereo_IsEnabled(NvU8* enabled)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!enabled) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*enabled = 0; // @added in 2.90.700.0
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DISP_GetGDIPrimaryDisplayId(NvU32* displayId)
	{
		LOG_NVAPI_FUNCTION_CALL();
		*displayId = 1; // fixme!!
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Mosaic_GetDisplayViewportsByResolution(NvU32 displayId, NvU32 srcWidth, NvU32 srcHeight, NV_RECT viewports[NV_MOSAIC_MAX_DISPLAYS], NvU8* bezelCorrected)
	{
		LOG_NVAPI_FUNCTION_CALL();

		LOG_TRACE(L"[NVAPI] NvAPI_Mosaic_GetDisplayViewportsByResolution: displayID: "
			+ std::to_wstring(displayId)
			+ L", width: " + std::to_wstring(srcWidth)
			+ L", height: " + std::to_wstring(srcHeight)
		);
		// report only one viewport
		viewports[0].top = 0;
		viewports[0].left = 0;
		viewports[0].right = srcWidth;
		viewports[0].bottom = srcHeight;

		// anything after that should have top = bottom, to tell the app there's no other viewport
		for (int i = 1; i < NV_MOSAIC_MAX_DISPLAYS; i++) {
			viewports[i].top = 0;
			viewports[i].left = 0;
			viewports[i].right = 0;
			viewports[i].bottom = 0;
		}
		*bezelCorrected = 1; // @added in 2.90.700.0
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_SYS_GetDisplayDriverInfo(NV_DISPLAY_DRIVER_INFO* driverInfo)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!driverInfo) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		driverInfo->driverVersion = ctx.driverVersion;
		strcpy_s(driverInfo->szBuildBranch, sizeof(NvAPI_ShortString), "production");
		driverInfo->bIsDCHDriver = 1;
		driverInfo->bIsNVIDIAStudioPackage = 1;
		driverInfo->bIsNVIDIAGameReadyPackage = 1;
		driverInfo->bIsNVIDIARTXProductionBranchPackage = 1;
		driverInfo->bIsNVIDIARTXNewFeatureBranchPackage = 1;
		if (driverInfo->version == NV_DISPLAY_DRIVER_INFO_VER2) {
			strcpy_s(driverInfo->szBuildBaseBranch, sizeof(NvAPI_ShortString), "production");
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetLogicalGpuInfo(NvLogicalGpuHandle logicalHandle, NV_LOGICAL_GPU_DATA* logicalGpuData)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!logicalGpuData) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvLogicalGpuHandle(logicalHandle);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		//memcpy(logicalGpuData->pOSAdapterId, &ctx.gpu.luid, sizeof(ctx.gpu.luid));

		if (logicalGpuData->pOSAdapterId) {
			LUID* luid = reinterpret_cast<LUID*>(logicalGpuData->pOSAdapterId);
			*luid = e->desc.AdapterLuid;
		}

		LOG_NVAPI_DEBUG(L"Returning logical GPU for LUID: " + LuidToHex(e->desc.AdapterLuid));
		logicalGpuData->physicalGpuHandles[0] = PackNvPhysicalGpuHandle(e);
		logicalGpuData->physicalGpuCount = 1;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_CudaEnumComputeCapableGpus(NV_COMPUTE_GPU_TOPOLOGY* pComputeTopo)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!pComputeTopo) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		auto pComputeTopoV1 = reinterpret_cast<NV_COMPUTE_GPU_TOPOLOGY_V1*>(pComputeTopo);
		pComputeTopoV1->gpuCount = 1;
		pComputeTopoV1->computeGpus[0].hPhysicalGpu = nullptr;
		pComputeTopoV1->computeGpus[0].flags = NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_CAPABLE | NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_ENABLE | NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_RECOMMENDED;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetPCIIdentifiers(NvPhysicalGpuHandle hPhysicalGpu, NvU32* pDeviceId, NvU32* pSubSystemId, NvU32* pRevisionId, NvU32* pExtDeviceId)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!pDeviceId || !pSubSystemId || !pRevisionId || !pExtDeviceId) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pDeviceId = (e->desc.DeviceId << 16) | e->desc.VendorId;
		*pSubSystemId = e->desc.SubSysId;
		*pRevisionId = e->desc.Revision;
		*pExtDeviceId = e->desc.DeviceId;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D12_IsNvShaderExtnOpCodeSupported(ID3D12Device* pDevice, NvU32 opCode, bool* pSupported)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!pSupported) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pSupported = false;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D12_SetNvShaderExtnSlotSpaceLocalThread(IUnknown* pDev, NvU32 uavSlot, NvU32 uavSpace)
	{
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NOT_SUPPORTED);
	}

	NvAPI_Status __cdecl NvAPI_EnumLogicalGPUs(NvLogicalGpuHandle nvGPUHandle[NVAPI_MAX_LOGICAL_GPUS], NvU32* pGpuCount)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pGpuCount) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);

		*pGpuCount = min(NVAPI_MAX_LOGICAL_GPUS, physicalGpus.size());

		int i = 0;
		for (i = 0; i < *pGpuCount; i++) {
			nvGPUHandle[i] = PackNvLogicalGpuHandle(&physicalGpus[i]);
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_SYS_GetGpuAndOutputIdFromDisplayId(NvU32 displayId, NvPhysicalGpuHandle* hPhysicalGpu, NvU32* outputId)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!outputId || !hPhysicalGpu) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		if (displayId < NVAPI_DISPLAY_ID_SPACE) {
			LOG_NVAPI_DEBUG(L" Display ID: " + std::to_wstring(displayId));
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ID_OUT_OF_RANGE);
		}

		auto _displayId = displayId % NVAPI_DISPLAY_ID_SPACE;
		auto _gpuId = (displayId - _displayId) / NVAPI_DISPLAY_ID_SPACE;
		if (_displayId > NVAPI_MAX_PHYSICAL_GPUS - 1 || _displayId + 1 > physicalGpus.size()) {
			LOG_NVAPI_DEBUG(L" Display ID: " + std::to_wstring(_displayId) + L", _gpuId: " + std::to_wstring(_gpuId));
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ID_OUT_OF_RANGE);
		}

		PhysicalGpuEntry* e = &physicalGpus[_displayId];

		*hPhysicalGpu = PackNvPhysicalGpuHandle(e);
		LOG_NVAPI_DEBUG(L" Returning GPU with LUID: " + LuidToHex(e->desc.AdapterLuid));
		*outputId = 1;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_EnumPhysicalGPUs(NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32* pGpuCount)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!nvGPUHandle || !pGpuCount) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);

		if (physicalGpus.size() > 0) {
			NvU32 available = static_cast<NvU32>(physicalGpus.size());
			NvU32 toCopy = (available < NVAPI_MAX_PHYSICAL_GPUS) ? available : NVAPI_MAX_PHYSICAL_GPUS;

			for (NvU32 i = 0; i < toCopy; ++i) {
				PhysicalGpuEntry* e = &physicalGpus[i];
				nvGPUHandle[i] = PackNvPhysicalGpuHandle(e);
			}

			*pGpuCount = toCopy;
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ARGUMENT_EXCEED_MAX_SIZE);
	}

	NvAPI_Status __cdecl NvAPI_SYS_GetDriverAndBranchVersion(NvU32* pDriverVersion, NvAPI_ShortString szBuildBranchString)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pDriverVersion) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		strcpy_s(szBuildBranchString, sizeof(NvAPI_ShortString), "DLSS Enabler");
		*pDriverVersion = ctx.driverVersion;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	static bool ConvertBuildRaytracingAccelerationStructureInputs(const NVAPI_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS_EX* nvDesc, std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* d3dDesc)
	{
		static bool reported = false;

		if (!reported) {
			LOG_NVAPI_FUNCTION_CALL();
		}

		reported = true;
		d3dDesc->Type = nvDesc->type;
		// assume that OMM via VK_EXT_opacity_micromap and DMM via VK_NV_displacement_micromap are not supported, allow only standard flags to be passed
		d3dDesc->Flags = static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(nvDesc->flags & 0x3f);
		d3dDesc->NumDescs = nvDesc->numDescs;
		d3dDesc->DescsLayout = nvDesc->descsLayout;

		if (d3dDesc->Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL) {
			d3dDesc->InstanceDescs = nvDesc->instanceDescs;
			return true;
		}

		if (d3dDesc->Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL && d3dDesc->DescsLayout == D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS) {
			d3dDesc->ppGeometryDescs = reinterpret_cast<const D3D12_RAYTRACING_GEOMETRY_DESC* const*>(nvDesc->ppGeometryDescs);
			return true;
		}

		if (d3dDesc->Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL && d3dDesc->DescsLayout == D3D12_ELEMENTS_LAYOUT_ARRAY) {
			geometryDescs.resize(d3dDesc->NumDescs);

			for (unsigned i = 0; i < d3dDesc->NumDescs; ++i) {
				auto& d3dGeoDesc = geometryDescs[i];
				auto& nvGeoDesc = *reinterpret_cast<const NVAPI_D3D12_RAYTRACING_GEOMETRY_DESC_EX*>(reinterpret_cast<const std::byte*>(nvDesc->pGeometryDescs) + (i * nvDesc->geometryDescStrideInBytes));

				d3dGeoDesc.Flags = nvGeoDesc.flags;

				switch (nvGeoDesc.type) {
				case NVAPI_D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES_EX:
					d3dGeoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
					d3dGeoDesc.Triangles = nvGeoDesc.triangles;
					break;
				case NVAPI_D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS_EX:
					d3dGeoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
					d3dGeoDesc.AABBs = nvGeoDesc.aabbs;
					break;
				case NVAPI_D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES_EX: // GetRaytracingCaps reports no OMM caps, we shouldn't reach this
					return false;
				case NVAPI_D3D12_RAYTRACING_GEOMETRY_TYPE_DMM_TRIANGLES_EX: // GetRaytracingCaps reports no DMM caps, we shouldn't reach this
					return false;
				default:
					return false;
				}
			}

			d3dDesc->pGeometryDescs = geometryDescs.data();
			return true;
		}

		return false;
	}

	NvAPI_Status __cdecl NvAPI_D3D12_GetRaytracingAccelerationStructurePrebuildInfoEx(ID3D12Device5* pDevice, NVAPI_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_EX_PARAMS* pParams)
	{
		static bool reported = false;
		if (!reported) {
			LOG_NVAPI_FUNCTION_CALL();
		}

		if (!pDevice || !pParams) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		if (pParams->version != NVAPI_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_EX_PARAMS_VER1) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INCOMPATIBLE_STRUCT_VERSION);
		}

		if (!pParams->pDesc || !pParams->pInfo) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs{};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS desc{};

		if (!ConvertBuildRaytracingAccelerationStructureInputs(pParams->pDesc, geometryDescs, &desc)) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&desc, pParams->pInfo);
		if (!reported) {
			reported = true;
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
		}

		return NVAPI_OK;
	}

	NvAPI_Status __cdecl NvAPI_D3D12_BuildRaytracingAccelerationStructureEx(ID3D12GraphicsCommandList4* pCommandList, const NVAPI_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_EX_PARAMS* pParams)
	{
		static bool reported = false;

		if (!reported) {
			LOG_NVAPI_FUNCTION_CALL();
		}

		if (!pCommandList || !pParams) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		if (pParams->version != NVAPI_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_EX_PARAMS_VER1) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INCOMPATIBLE_STRUCT_VERSION);
		}

		if (!pParams->pDesc) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		if (pParams->numPostbuildInfoDescs && !pParams->pPostbuildInfoDescs) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs{};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {
			.DestAccelerationStructureData = pParams->pDesc->destAccelerationStructureData,
			.Inputs = {},
			.SourceAccelerationStructureData = pParams->pDesc->sourceAccelerationStructureData,
			.ScratchAccelerationStructureData = pParams->pDesc->scratchAccelerationStructureData,
		};

		if (!ConvertBuildRaytracingAccelerationStructureInputs(&pParams->pDesc->inputs, geometryDescs, &desc.Inputs))
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);

		pCommandList->BuildRaytracingAccelerationStructure(&desc, pParams->numPostbuildInfoDescs, pParams->pPostbuildInfoDescs);

		if (!reported) {
			reported = true;
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
		}

		return NVAPI_OK;
	}

	NvAPI_Status __cdecl NvAPI_D3D12_GetRaytracingCaps(ID3D12Device* device, NVAPI_D3D12_RAYTRACING_CAPS_TYPE type, void* pData, size_t dataSize)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (device == nullptr || pData == nullptr) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_POINTER);
		}

		switch (type) {
		case NVAPI_D3D12_RAYTRACING_CAPS_TYPE_THREAD_REORDERING:
			if (dataSize != sizeof(NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAPS)) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			// let's hope that NvAPI_D3D12_IsNvShaderExtnOpCodeSupported returning false is enough to discourage games from attempting to use Shader Execution Reordering
			*(NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAPS*)pData = NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAP_NONE;
			break;

		case NVAPI_D3D12_RAYTRACING_CAPS_TYPE_OPACITY_MICROMAP:
			if (dataSize != sizeof(NVAPI_D3D12_RAYTRACING_OPACITY_MICROMAP_CAPS)) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			*(NVAPI_D3D12_RAYTRACING_OPACITY_MICROMAP_CAPS*)pData = NVAPI_D3D12_RAYTRACING_OPACITY_MICROMAP_CAP_NONE;
			break;

		case NVAPI_D3D12_RAYTRACING_CAPS_TYPE_DISPLACEMENT_MICROMAP:
			if (dataSize != sizeof(NVAPI_D3D12_RAYTRACING_DISPLACEMENT_MICROMAP_CAPS)) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			*(NVAPI_D3D12_RAYTRACING_DISPLACEMENT_MICROMAP_CAPS*)pData = NVAPI_D3D12_RAYTRACING_DISPLACEMENT_MICROMAP_CAP_NONE;
			break;

		case NVAPI_D3D12_RAYTRACING_CAPS_TYPE_CLUSTER_OPERATIONS:
			if (dataSize != sizeof(NVAPI_D3D12_RAYTRACING_CLUSTER_OPERATIONS_CAPS)) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			*(NVAPI_D3D12_RAYTRACING_CLUSTER_OPERATIONS_CAPS*)pData = NVAPI_D3D12_RAYTRACING_CLUSTER_OPERATIONS_CAP_NONE;
			break;

		case NVAPI_D3D12_RAYTRACING_CAPS_TYPE_PARTITIONED_TLAS:
			if (dataSize != sizeof(NVAPI_D3D12_RAYTRACING_PARTITIONED_TLAS_CAPS)) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			*(NVAPI_D3D12_RAYTRACING_PARTITIONED_TLAS_CAPS*)pData = NVAPI_D3D12_RAYTRACING_PARTITIONED_TLAS_CAP_NONE;
			break;

		case NVAPI_D3D12_RAYTRACING_CAPS_TYPE_SPHERES:
			if (dataSize != sizeof(NVAPI_D3D12_RAYTRACING_SPHERES_CAPS)) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			*(NVAPI_D3D12_RAYTRACING_SPHERES_CAPS*)pData = NVAPI_D3D12_RAYTRACING_SPHERES_CAP_NONE;
			break;

		case NVAPI_D3D12_RAYTRACING_CAPS_TYPE_LINEAR_SWEPT_SPHERES:
			if (dataSize != sizeof(NVAPI_D3D12_RAYTRACING_LINEAR_SWEPT_SPHERES_CAPS)) {
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}

			*(NVAPI_D3D12_RAYTRACING_LINEAR_SWEPT_SPHERES_CAPS*)pData = NVAPI_D3D12_RAYTRACING_LINEAR_SWEPT_SPHERES_CAP_NONE;
			break;

		default:
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D_GetObjectHandleForResource(IUnknown* invalid, IUnknown* pResource, NVDX_ObjectHandle* pHandle)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pResource) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pHandle = (NVDX_ObjectHandle)pResource;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_EnumNvidiaDisplayHandle(NvU32 displayId, NvDisplayHandle* handle)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!handle) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);

		if (displayId >= physicalGpus.size()) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_END_ENUMERATION);
		}

		PhysicalGpuEntry* e = &physicalGpus[displayId];
		*handle = reinterpret_cast<NvDisplayHandle>(e);
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}


	NvAPI_Status __cdecl NvAPI_GetLogicalGPUFromPhysicalGPU(NvPhysicalGpuHandle hPhysicalGpu, NvLogicalGpuHandle* logicalHandle)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!logicalHandle) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		*logicalHandle = PackNvLogicalGpuHandle(e);
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GetGPUIDfromPhysicalGPU(NvPhysicalGpuHandle hPhysicalGpu, NvU32* pGpuId)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pGpuId) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);

		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_EXPECTED_PHYSICAL_GPU_HANDLE);
		}

		*pGpuId = e->gpuId;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GetPhysicalGPUFromGPUID(NvU32 gpuId, NvPhysicalGpuHandle* pPhysicalGPU)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pPhysicalGPU || gpuId < NVAPI_GPU_ID_SPACE) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		auto id = gpuId - NVAPI_GPU_ID_SPACE;
		if (id >= physicalGpus.size()) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		*pPhysicalGPU = PackNvPhysicalGpuHandle(&physicalGpus[id]);
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Disp_SetOutputMode(NvU32 displayId, NV_DISPLAY_OUTPUT_MODE* pDisplayMode)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!pDisplayMode) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pDisplayMode = NV_DISPLAY_OUTPUT_MODE_SDR;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Disp_GetOutputMode(NvU32 displayId, NV_DISPLAY_OUTPUT_MODE* pDisplayMode)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!pDisplayMode) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pDisplayMode = NV_DISPLAY_OUTPUT_MODE_SDR;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	constexpr size_t NVQ_NAME_WCHARS = 512;
	using NvAPI_UnicodeHalfString = NvU16[NVQ_NAME_WCHARS];

	struct NV_INF_FEATURE_QUERY_V1 {
		NvU32                     version;                     // [0]
		NvAPI_UnicodeHalfString   name;                        // [4..1027] UTF-16LE, zero-terminated
		char                      valueAnsi[NVAPI_GENERIC_STRING_MAX]; // [1028..5123]
		NvU32                     supported;                   // [5124]
	};

	static std::wstring ReadNameUTF16(const NvU16* buf, size_t maxWchars = NVQ_NAME_WCHARS)
	{
		std::wstring out;
		out.reserve(64);
		for (size_t i = 0; i < maxWchars; ++i) {
			NvU16 ch = buf[i];
			if (ch == 0) break;
			out.push_back(static_cast<wchar_t>(ch));
		}
		return out;
	}

	static void WriteAnsi(char dst[NVAPI_GENERIC_STRING_MAX], const std::string& s)
	{
		memset(dst, 0, NVAPI_GENERIC_STRING_MAX);
#if defined(_MSC_VER)
		strncpy_s(dst, NVAPI_GENERIC_STRING_MAX, s.c_str(), _TRUNCATE);
#else
		std::strncpy(dst, s.c_str(), NVAPI_GENERIC_STRING_MAX - 1);
		dst[NVAPI_GENERIC_STRING_MAX - 1] = '\0';
#endif
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetValuesFromInstalledINF(void* /*hGpu*/, NV_INF_FEATURE_QUERY_V1* q)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!q)  LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ERROR);

		static const std::unordered_map<std::wstring, std::string> kValues = {
			{L"FeatureScore",               "CF"},
			{L"NVCheckVersion",             "1"},
			{L"NVRemoveDisplayPhantoms",    "1"},
			{L"NVSupportAnsel",             "1"},
			{L"NVSupportDisplayUpdate",     "1"},
			{L"NVSupportGFExperienceOEM",   "1"},
			{L"NVSupportGFExperienceUDA",   "1"},
			{L"NVSupportNGX",               "1"},
			{L"NVSupportPhysx",             "1"},
			{L"NvCleanInstallOnDowngrade",  "1"},
			{L"NvShowEGDowngradeMessage",   "1"},
			{L"NvSupportMSHybrid",          "1"},
			{L"NvSupportTelemetry",         "1"},
		};

		const std::wstring name = ReadNameUTF16(q->name);

		auto it = kValues.find(name);
		if (it != kValues.end()) {
			// recognized name: set the value and supported=1
			WriteAnsi(q->valueAnsi, it->second);
			q->supported = 1;
			// LOG_INFO(L"[Mock] " + name + L" -> \"" + std::wstring(it->second.begin(), it->second.end()) + L"\" (supported=1)");
		}
		else {
			// unrecognized name: supported=0, empty value
			memset(q->valueAnsi, 0, NVAPI_GENERIC_STRING_MAX);
			q->supported = 0;

			LOG_NVAPI_WARNING(L"NvAPI_GPU_GetValuesFromInstalledIN: unsupported / not found in map: " + name + L" (supported=0)");
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DRS_LoadGoldSettings(NvDRSSessionHandle hSession)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (hSession != drsSession) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Vulkan_NotifyOutOfBandVkQueue()
	{
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvLL_VK_0xa17d13d6()
	{
		static bool isReported = false;

		if (isReported) {
			return NVAPI_OK;
		}

		isReported = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvLL_VK_0xc83c4d5d()
	{
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D_GetCurrentSLIState(IUnknown* pDevice, NV_GET_CURRENT_SLI_STATE* pSliState)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!pSliState) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		switch (pSliState->version) {
		case NV_GET_CURRENT_SLI_STATE_VER1: {
			auto pSliStateV1 = reinterpret_cast<NV_GET_CURRENT_SLI_STATE_V1*>(pSliState);
			// Report that SLI is not available
			pSliStateV1->maxNumAFRGroups = 1;
			pSliStateV1->numAFRGroups = 1;
			pSliStateV1->currentAFRIndex = 0;
			pSliStateV1->nextFrameAFRIndex = 0;
			pSliStateV1->previousFrameAFRIndex = 0;
			pSliStateV1->bIsCurAFRGroupNew = false;
			break;
		}
		case NV_GET_CURRENT_SLI_STATE_VER2:
			// Report that SLI is not available
			pSliState->maxNumAFRGroups = 1;
			pSliState->numAFRGroups = 1;
			pSliState->currentAFRIndex = 0;
			pSliState->nextFrameAFRIndex = 0;
			pSliState->previousFrameAFRIndex = 0;
			pSliState->bIsCurAFRGroupNew = false;
			pSliState->numVRSLIGpus = 0;
			break;
		default:
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}


	NvAPI_Status __cdecl NvAPI_GPU_QueryNodeInfo(NvPhysicalGpuHandle hPhysicalGpu, void* unknown)
	{
		LOG_NVAPI_FUNCTION_CALL();

		uint32_t serializedData[] = { 0x2120c, 0x8, 0x1, 0x1,
			0x4433, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x2, 0x0, 0x7265764f, 0x79616c,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x1a, 0x0,
			0x4544564e, 0x3043, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0xa, 0x0, 0x434553, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0xb, 0x0,
			0x304543, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0xc, 0x1, 0x314543, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x13, 0x0,
			0x4e45534d, 0x3143, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0,
			0x1c, 0x0,
		};

		std::memcpy(unknown, serializedData, 522);
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	static bool NvAPI_InitializeInternal()
	{
		static bool isNvApiInitialized = false;

		if (OriginalNvAPI_QueryInterface) {
			LOG_NVAPI_INFO(L"NvAPI_Initialize: Calling NvAPI_Init");
			static NvAPI_Initialize_t init;

			if (!init) {
				init = (NvAPI_Initialize_t)OriginalNvAPI_QueryInterface(NVAPI_INITIALIZE);
				init();
			}

			static NvAPI_Initialize_t init2;

			if (!init2) {
				init2 = (NvAPI_Initialize_t)nvapi_QueryInterface(NVAPI_INITIALIZE);
				init2();
			}
		}

		if (isNvApiInitialized) {
			return true;
		}

		LOG_NVAPI_DEBUG(L"Listing available GPUs");
		HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
		if (!dxgi) {
			if (Common::IsPluginPresent(L"dxgi.dll")) {
				dxgi = Common::LoadPlugin(L"dxgi.dll");
			}

			if (!dxgi) {
				dxgi = LoadLibraryW(L"dxgi.dll");
			}
		}

		if (!dxgi) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NOT_SUPPORTED);
		}

		// Function pointers for DXGI functions
		typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY)(REFIID, void**);
		PFN_CREATE_DXGI_FACTORY pfnCreateDXGIFactory = nullptr;

		// Get function pointers
		pfnCreateDXGIFactory = reinterpret_cast<PFN_CREATE_DXGI_FACTORY>(
			GetProcAddress(dxgi, "CreateDXGIFactory1")
			);

		if (!pfnCreateDXGIFactory) {
			LOG_NVAPI_ERROR(L"Failed to get function pointer for CreateDXGIFactory1");

			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NOT_SUPPORTED);
		}

		// Create DXGI factory
		IDXGIFactory6* pFactory = nullptr;
		HRESULT hr = pfnCreateDXGIFactory(__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&pFactory));
		if (FAILED(hr)) {
			LOG_NVAPI_ERROR(L"Failed to call CreateDXGIFactory1 (code: " + std::to_wstring(hr) + L")");

			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NOT_SUPPORTED);
		}

		// Enumerate adapters
		IDXGIAdapter1* pAdapter = nullptr;
		LOG_NVAPI_INFO(L"Following Display Adapters detected:");
		int j = 1;
		for (UINT i = 0; pFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(pAdapter), reinterpret_cast<void**>(&pAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC1 desc;
			pAdapter->GetDesc1(&desc);
			if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
				LOG_INFO(L"   Adapter #" + std::to_wstring(j) + L": " + std::wstring(desc.Description) + L" (VRAM: " + std::to_wstring((int)((desc.DedicatedVideoMemory + desc.DedicatedSystemMemory) / (1024 * 1024))) + L"MB)");
				j++;
				PhysicalGpuEntry entry{};
				entry.desc = desc;
				entry.adapter = pAdapter;
				entry.gpuId = NVAPI_GPU_ID_SPACE + static_cast<NvU32>(physicalGpus.size());
				if (physicalGpus.empty()) {
					physicalGpus.push_back(entry);
				}
			}
		}

		//// Release resources
		//if (pAdapter) {
		//	pAdapter->Release();
		//}
		pFactory->Release();

		if (physicalGpus.size() > 0) {
			DXGI_ADAPTER_DESC1 adapterDesc = physicalGpus[0].desc;
			ctx.gpu.luid = adapterDesc.AdapterLuid;
			ctx.gpu.deviceId = adapterDesc.DeviceId;
			ctx.gpu.vendorId = adapterDesc.VendorId;
			ctx.gpu.subSysId = adapterDesc.SubSysId;
			ctx.gpu.revisionId = adapterDesc.Revision;
			ctx.gpu.deviceName = std::wstring(adapterDesc.Description);
		}

		LOG_NVAPI_DEBUG(L"NvAPI_Initialize: GPU selected: " + std::wstring(ctx.gpu.deviceName) + L" (LUID: " + LuidToHex(ctx.gpu.luid) + L")");

		if (L"0000e0e8" == LuidToHex(ctx.gpu.luid)) {
			//ctx.nvapi.isHighestArchEnabled = true;
		}
		isNvApiInitialized = true;
		return true;
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetOutputType(NvPhysicalGpuHandle hPhysicalGpu, NvU32 outputId, NV_GPU_OUTPUT_TYPE* pOutputType)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (pOutputType == nullptr) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_EXPECTED_PHYSICAL_GPU_HANDLE);
		}

		*pOutputType = NVAPI_GPU_OUTPUT_DFP;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D12_IsFatbinPTXSupported(ID3D12Device* pDevice, bool* pSupported)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (pSupported == nullptr) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pSupported = false; // its a pure NVIDIA feature, so we report false, as this code shouldn't be called in case of real NVAPI on genuine NVIDIA GPUs

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Initialize()
	{
		LOG_NVAPI_FUNCTION_CALL();

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NvAPI_InitializeInternal() ? NVAPI_OK : NVAPI_ERROR);
	}

	NvAPI_Status __cdecl NvLL_VK_SetSleepMode(VkDevice device, NV_SET_SLEEP_MODE_PARAMS* pSetSleepModeParams)
	{
		static bool logged = false;
		if (!logged) {
			LOG_NVAPI_FUNCTION_CALL();
		}
		ctx.reflex.isMarkersOptimizationEnabled = pSetSleepModeParams->bUseMarkersToOptimize;

		if (pSetSleepModeParams->bLowLatencyMode) {
			//pSetSleepModeParams->minimumIntervalUs = 0;
			if (!logged) {
				LOG_NVAPI_FUNCTION_CALL();
				if (ctx.reflex.isBoostEnabled) {
					pSetSleepModeParams->bLowLatencyBoost = ctx.reflex.isBoostEnabled;
					LOG_NVAPI_INFO(L"Enabling Reflex Boost");
				}

				if (ctx.reflex.isEnabled == false) {
					LOG_NVAPI_INFO(L"Disabling Reflex");
				}
			}

			if (ctx.reflex.isEnabled == false) {
				pSetSleepModeParams->bLowLatencyMode = false;
				ctx.reflex.isOriginallyEnabled = false;
			}
			else {
				ctx.reflex.isOriginallyEnabled = pSetSleepModeParams->bLowLatencyMode;
			}
		}

		if (!logged) {
			logged = true;
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
		}

		return NVAPI_OK;
	}

	NvAPI_Status __cdecl NvLL_VK_GetSleepStatus(VkDevice device, NV_GET_SLEEP_STATUS_PARAMS* pGetSleepStatusParams)
	{
		static bool reported = false;
		(*pGetSleepStatusParams).bLowLatencyMode = ctx.reflex.isOriginallyEnabled;
		(*pGetSleepStatusParams).version = 1;
		(*pGetSleepStatusParams).bFsVrr = false;
		(*pGetSleepStatusParams).bCplVsyncOn = false;

		if (reported) {
			return NVAPI_OK;
		}

		reported = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvLL_VK_Sleep(VkDevice device, uint64_t semaphoreValue)
	{
		static bool reported = false;
		streamlineSignalId = semaphoreValue;

		if (reported) {
			return NVAPI_OK;
		}

		reported = true;
		LOG_NVAPI_FUNCTION_CALL();

		LOG_NVAPI_DEBUG(L"NvLL_VK_Sleep: semaphoreId = " + std::to_wstring(semaphoreValue));
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN((NvAPI_Status)0);
		HMODULE vulkanLib = GetModuleHandleW(L"vulkan-1.dll");

		if (!vulkanLib)
		{
			vulkanLib = LoadLibrary(L"vulkan-1.dll");
			if (!vulkanLib)
			{
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ERROR);
			}
		}

		PFN_vkCreateSemaphore vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(GetProcAddress(vulkanLib, "vkCreateSemaphore"));
		PFN_vkQueueSubmit vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(GetProcAddress(vulkanLib, "vkQueueSubmit"));
		PFN_vkDestroySemaphore vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(GetProcAddress(vulkanLib, "vkDestroySemaphore"));
		PFN_vkGetDeviceQueue vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(GetProcAddress(vulkanLib, "vkGetDeviceQueue"));

		if (!vkCreateSemaphore || !vkQueueSubmit || !vkDestroySemaphore || !vkGetDeviceQueue)
		{
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ERROR);
		}

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkSemaphore semaphore;
		VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
		if (result != VK_SUCCESS)
		{
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ERROR);
		}

		uint32_t queueFamilyIndex = 0;
		uint32_t queueIndex = 0;

		VkQueue queue;
		vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

		VkTimelineSemaphoreSubmitInfo timelineInfo{};
		timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		timelineInfo.signalSemaphoreValueCount = 1;
		timelineInfo.pSignalSemaphoreValues = &semaphoreValue;

		// Initialize the submit info structure properly
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = &timelineInfo;
		submitInfo.waitSemaphoreCount = 0;  // Corrected
		submitInfo.pWaitSemaphores = nullptr;  // Corrected
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &semaphore;

		Sleep(15);  // Sleep before submission

		VkResult threadResult = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		if (threadResult != VK_SUCCESS)
		{
			vkDestroySemaphore(device, semaphore, nullptr);
			//FreeLibrary(vulkanLib);
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_ERROR);
		}

		//vkDestroySemaphore(device, semaphore, nullptr);
		//FreeLibrary(vulkanLib);
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_InitializeEx()
	{
		LOG_NVAPI_FUNCTION_CALL();

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NvAPI_InitializeInternal() ? NVAPI_OK : NVAPI_ERROR);
	}

	NvAPI_Status __cdecl NvAPI_SYS_GetDisplayIdFromGpuAndOutputId(NvPhysicalGpuHandle hPhysicalGpu, NvU32 outputId, NvU32* displayId)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (displayId == nullptr) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*displayId = 0;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DRS_CreateSession(NvDRSSessionHandle* session)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (session == nullptr) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*session = drsSession;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DRS_LoadSettings(NvDRSSessionHandle session)
	{
		LOG_NVAPI_FUNCTION_CALL();
		// right now we support only one session
		if (session != drsSession) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DRS_DestroySession(NvDRSSessionHandle session)
	{
		LOG_NVAPI_FUNCTION_CALL();
		// right now we support only one session
		if (session != drsSession) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DRS_GetProfileInfo(NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile, NVDRS_PROFILE* pProfileInfo)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (hSession != drsSession || !pProfileInfo) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		wcsncpy_s(reinterpret_cast<wchar_t*>(pProfileInfo->profileName), 2048, L"Default", _TRUNCATE);
		pProfileInfo->isPredefined = 1;
		pProfileInfo->numOfApps = 0;
		pProfileInfo->numOfSettings = 0;
		pProfileInfo->gpuSupport.geforce = 1;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DRS_FindApplicationByName(NvDRSSessionHandle hSession, NvAPI_UnicodeString appName, NvDRSProfileHandle* phProfile, NVDRS_APPLICATION* pApplication)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (!phProfile || !pApplication) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_POINTER);
		}

		*phProfile = drsProfile;

		// V1 fields
		pApplication->isPredefined = 1;
		wcsncpy_s(reinterpret_cast<wchar_t*>(pApplication->appName), 2048, Common::GetProcessFilePath().c_str(), _TRUNCATE);
		wcsncpy_s(reinterpret_cast<wchar_t*>(pApplication->userFriendlyName), 2048, L"", _TRUNCATE);
		wcsncpy_s(reinterpret_cast<wchar_t*>(pApplication->launcher), 2048, L"", _TRUNCATE);

		// V2 onwards
		if (pApplication->version != NVDRS_APPLICATION_VER_V1) {
			wcsncpy_s(reinterpret_cast<wchar_t*>(pApplication->fileInFolder), 2048, L"", _TRUNCATE);
		}

		// V3 onwards
		if (pApplication->version == NVDRS_APPLICATION_VER_V3 || pApplication->version == NVDRS_APPLICATION_VER_V4) {
			pApplication->isMetro = 0;
			pApplication->isCommandLine = 0;
			pApplication->reserved = 0;
		}

		// V4
		if (pApplication->version == NVDRS_APPLICATION_VER_V4) {
			wcsncpy_s(reinterpret_cast<wchar_t*>(pApplication->commandLine), 2048, L"", _TRUNCATE);
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_DRS_GetBaseProfile(NvDRSSessionHandle hSession, NvDRSProfileHandle* phProfile)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (hSession != drsSession || !phProfile) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*phProfile = drsProfile;
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Unload()
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (OriginalNvAPI_QueryInterface) {
			NvAPI_Unload_t unload = (NvAPI_Unload_t)OriginalNvAPI_QueryInterface(NVAPI_UNLOAD);
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(unload());
		}
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	// Proxy: NvAPI_DRS_GetSetting
	NvAPI_Status __cdecl NvAPI_DRS_GetSettingProxy(NvDRSSessionHandle hSession,
		NvDRSProfileHandle hProfile,
		NvU32 settingId,
		NVDRS_SETTING* pSetting /* NVDRS_SETTING_V1 */)
	{
		using PfnDRSGetSetting = NvAPI_Status(__cdecl*)(NvDRSSessionHandle, NvDRSProfileHandle, NvU32, NVDRS_SETTING*);
		static PfnDRSGetSetting s_org = nullptr;

		constexpr NvU32 kFnId_DRS_GetSetting = 0x73bf8338;

		auto Hex32 = [](NvU32 v) -> std::wstring {
			wchar_t buf[16];
			swprintf(buf, 16, L"0x%08X", v);
			return std::wstring(buf);
			};

		auto HexPtr = [](const void* p) -> std::wstring {
			wchar_t buf[32];
#if defined(_WIN64)
			swprintf(buf, 32, L"0x%016llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(p)));
#else
			swprintf(buf, 32, L"0x%08X", static_cast<unsigned>(reinterpret_cast<uintptr_t>(p)));
#endif
			return std::wstring(buf);
			};

		auto SettingTypeToStr = [](NVDRS_SETTING_TYPE t) -> const wchar_t* {
			switch (t) {
			case NVDRS_DWORD_TYPE:   return L"DWORD";
			case NVDRS_STRING_TYPE:  return L"STRING(ANSI)"; // w tej wersji i tak mamy UnicodeString w unii
			case NVDRS_WSTRING_TYPE: return L"WSTRING";
			case NVDRS_BINARY_TYPE:  return L"BINARY";
			default:                 return L"(unknown)";
			}
			};

		auto LocationToStr = [](NVDRS_SETTING_LOCATION loc) -> const wchar_t* {
			switch (loc) {
			case NVDRS_CURRENT_PROFILE_LOCATION: return L"CURRENT_PROFILE";
			case NVDRS_GLOBAL_PROFILE_LOCATION:  return L"GLOBAL_PROFILE";
			case NVDRS_BASE_PROFILE_LOCATION:    return L"BASE_PROFILE";
			default:                             return L"(unknown)";
			}
			};

		auto DumpBinaryPreview = [&](const NVDRS_BINARY_SETTING& bs, NvU32 maxBytes = 64u) -> std::wstring {
			std::wstring out;
			out.reserve(16 + maxBytes * 3);
			out += L"len=" + std::to_wstring(bs.valueLength) + L", ptr=" + HexPtr(bs.valueData);
			if (bs.valueData && bs.valueLength) {
				const NvU32 toDump = (bs.valueLength < maxBytes) ? bs.valueLength : maxBytes;
				out += L", data=";
				const unsigned char* data = static_cast<const unsigned char*>(bs.valueData);
				wchar_t byteBuf[4];
				for (NvU32 i = 0; i < toDump; ++i) {
					swprintf(byteBuf, 4, L"%02X", static_cast<unsigned>(data[i]));
					out += byteBuf;
					if (i + 1 < toDump) out += L' ';
				}
				if (bs.valueLength > maxBytes) out += L" ...";
			}
			return out;
			};

		if (!s_org) {
			void* p = OriginalNvAPI_QueryInterface(kFnId_DRS_GetSetting);
			if (!p) {
				LOG_NVAPI_WARNING(L"NvAPI_DRS_GetSetting: QueryInterface returned null (wrong ID?)");
				return NVAPI_ERROR;
			}
			s_org = reinterpret_cast<PfnDRSGetSetting>(p);
		}

		NvAPI_Status st = s_org(hSession, hProfile, settingId, pSetting);

		{
			std::wstring hdr = L"DRS_GetSetting("
				+ std::to_wstring(static_cast<unsigned>(settingId))
				+ L" / 0x";
			{
				wchar_t buf[16];
				swprintf(buf, 16, L"%08X", settingId);
				hdr += buf;
			}
			hdr += L") -> ";
			hdr += (st == NVAPI_OK) ? L"NVAPI_OK" : L"ERR" + std::to_wstring(st);
			LOG_NVAPI_WARNING(hdr);
		}

		if (st != NVAPI_OK || !pSetting)
			return st;

		const NVDRS_SETTING_V1& S = *reinterpret_cast<const NVDRS_SETTING_V1*>(pSetting);

		LOG_NVAPI_WARNING(L"  version = " + std::to_wstring(S.version));
		auto name = (S.settingName[0] ? std::wstring((wchar_t*)S.settingName) : L"<empty>");
		LOG_NVAPI_WARNING(L"  settingName = " + name);
		LOG_NVAPI_WARNING(L"  settingId = " + std::to_wstring(S.settingId) + L" (" + Hex32(S.settingId) + L")");
		LOG_NVAPI_WARNING(std::wstring(L"  settingType = ") + SettingTypeToStr(S.settingType));
		LOG_NVAPI_WARNING(std::wstring(L"  settingLocation = ") + LocationToStr(S.settingLocation));
		LOG_NVAPI_WARNING(L"  isCurrentPredefined = " + std::to_wstring(S.isCurrentPredefined));
		LOG_NVAPI_WARNING(L"  isPredefinedValid = " + std::to_wstring(S.isPredefinedValid));

		{
			LOG_NVAPI_WARNING(L"  [Predefined] u32PredefinedValue = " + std::to_wstring(S.u32PredefinedValue)
				+ L" (" + Hex32(S.u32PredefinedValue) + L")");

			LOG_NVAPI_WARNING(L"  [Predefined] binaryPredefinedValue: " + DumpBinaryPreview(S.binaryPredefinedValue, 0));

			LOG_NVAPI_WARNING(std::wstring(L"  [Predefined] wszPredefinedValue ptr = ")
				+ HexPtr(S.wszPredefinedValue)
				+ L", text=" + (S.wszPredefinedValue && S.wszPredefinedValue[0] ? std::wstring((wchar_t*)S.wszPredefinedValue) : L"<empty-or-null>"));

			switch (S.settingType) {
			case NVDRS_DWORD_TYPE:
				break;
			case NVDRS_WSTRING_TYPE:
				if (S.wszPredefinedValue && S.wszPredefinedValue[0]) {
					LOG_NVAPI_WARNING(std::wstring(L"  [Predefined] (friendly) WSTRING = ") + std::wstring((wchar_t*)S.wszPredefinedValue));
				}
				break;
			case NVDRS_STRING_TYPE:
				if (S.wszPredefinedValue && S.wszPredefinedValue[0]) {
					LOG_NVAPI_WARNING(std::wstring(L"  [Predefined] (friendly) STRING = ") + std::wstring((wchar_t*)S.wszPredefinedValue));
				}
				break;
			case NVDRS_BINARY_TYPE:
				LOG_NVAPI_WARNING(L"  [Predefined] (friendly) BINARY: " + DumpBinaryPreview(S.binaryPredefinedValue, 64));
				break;
			default:
				break;
			}
		}

		{
			LOG_NVAPI_WARNING(L"  [Current] u32CurrentValue = " + std::to_wstring(S.u32CurrentValue)
				+ L" (" + Hex32(S.u32CurrentValue) + L")");

			LOG_NVAPI_WARNING(L"  [Current] binaryCurrentValue: " + DumpBinaryPreview(S.binaryCurrentValue, 0));

			LOG_NVAPI_WARNING(std::wstring(L"  [Current] wszCurrentValue ptr = ")
				+ HexPtr(S.wszCurrentValue)
				+ L", text=" + (S.wszCurrentValue && S.wszCurrentValue[0] ? std::wstring((wchar_t*)S.wszPredefinedValue) : L"<empty-or-null>"));

			switch (S.settingType) {
			case NVDRS_DWORD_TYPE:
				break;
			case NVDRS_WSTRING_TYPE:
				if (S.wszCurrentValue && S.wszCurrentValue[0]) {
					LOG_NVAPI_WARNING(std::wstring(L"  [Current] (friendly) WSTRING = ") + std::wstring((wchar_t*)S.wszPredefinedValue));
				}
				break;
			case NVDRS_STRING_TYPE:
				if (S.wszCurrentValue && S.wszCurrentValue[0]) {
					LOG_NVAPI_WARNING(std::wstring(L"  [Current] (friendly) STRING = ") + std::wstring((wchar_t*)S.wszPredefinedValue));
				}
				break;
			case NVDRS_BINARY_TYPE:
				LOG_NVAPI_WARNING(L"  [Current] (friendly) BINARY: " + DumpBinaryPreview(S.binaryCurrentValue, 64));
				break;
			default:
				break;
			}
		}

		return st;
	}


	NvAPI_Status __cdecl NvAPI_DRS_GetSetting(NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile, NvU32 settingId, NVDRS_SETTING* pSetting)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pSetting) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}
		  
		switch (settingId) {
		//case 0x10308298: // NGX_DLSSG_MODE
		//case 0x10CF4125: // NGX_DLSSG_DYNAMIC_TARGET_FRAME_RATE
		case 0x10E41DF1: // NGX_DLSS_FG_OVERRIDE_RENDER_PRESET_SELECTION
		//case 0x10E41E03: // NGX_DLSS_FG_OVERRIDE
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_SETTING_NOT_FOUND);
		}
		
		if (settingId == 0x10E41E06 || settingId == 0x10E41DF2) {
			
		}

		// Try using the original NVAPI if pointer is available
		if (org_NvAPI_DRS_GetSetting) {
			NvAPI_Status st = org_NvAPI_DRS_GetSetting(hSession, hProfile, settingId, pSetting);

			// -----------------------------------------------------------------
			// FIX: On GPUs without native DRS (AMD/Intel with FakeNVAPI), org
			// may falsely return NVAPI_OK with value 0 for settings it doesn't
			// recognize. On a real NVIDIA driver these settings return
			// NVAPI_SETTING_NOT_FOUND (-160).
			//
			// If we don't have a native NVIDIA driver and org returned OK
			// for a known DLSS/NGX setting, treat it as NOT_FOUND to match
			// real NVIDIA driver behavior.
			// -----------------------------------------------------------------
			if (st == NVAPI_OK && (ctx.nvapi.isEmbeddedNvapiUsed || ctx.nvapi.isMockEnabled)) {
				// Known DLSS/NGX DRS settings that should return NOT_FOUND
				// on non-NVIDIA hardware
				switch (settingId) {
				case NGX_DLSS_FG_OVERRIDE_ID:        // 0x10E41E03
				case NGX_DLSSG_MULTI_FRAME_COUNT_ID: // 0x104D6667
				case 0x10A89C8E:                     // DLSS-FG related (undocumented)
				case 0x10C7D835:                     // DLSS-FG related (undocumented)
				case 0x10E41DF2:                     // DLSS-SR related (undocumented)
				case 0x10AFB76B:                      // DLSS logging filename
				case 0x10E41E01:                     // NGX_DLSS_SR_OVERRIDE_ID
				case 0x10AFB764:                     // undocumented
				{
					wchar_t buf[16];
					swprintf(buf, 16, L"%08X", settingId);
					LOG_NVAPI_DEBUG(L"DRS_GetSetting: overriding fake OK -> SETTING_NOT_FOUND for settingId=0x"
						+ std::wstring(buf));
					st = NVAPI_SETTING_NOT_FOUND;
				}
				break;
				default:
					// Other settings � pass org result through unchanged
					break;
				}
			}

			if (st == NVAPI_OK) {
				switch (settingId) {
				case 0x10308298: // NGX_DLSSG_MODE
					if (!(
						pSetting->u32CurrentValue == 2 // NGX_DLSSG_MODE_ON
						||
						pSetting->u32CurrentValue == 4 // NGX_DLSSG_MODE_DYNAMIC
						)) {
						LOG_WARNING(L"[NVAPI] NGX_DLSSG_MODE was set to " + std::to_wstring(pSetting->u32CurrentValue));
						pSetting->u32CurrentValue = 2;
					}
					break;
				}
			}

			// Log once per settingId
			static std::map<NvU32, bool> loggedSettings;
			if (loggedSettings.find(settingId) == loggedSettings.end()) {
				loggedSettings[settingId] = true;
				wchar_t buf[16];
				swprintf(buf, 16, L"%08X", settingId);
				LOG_NVAPI_DEBUG(L"DRS_GetSetting proxied for settingId=0x" + std::wstring(buf)
					+ L", result=" + std::to_wstring(st));
			}

			return st;
		}

		// =======================================================================
		// FALLBACK - when org pointer is not available (embedded mode)
		// =======================================================================

		if (hSession != drsSession || hProfile != drsProfile) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		if (pSetting->version != NVDRS_SETTING_VER1) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INCOMPATIBLE_STRUCT_VERSION);
		}

		// Embedded mode - handle known settings
		std::wstring settingName = L"";

		switch (settingId) {
		case 0x10E41E01:  // NGX_DLSS_SR_OVERRIDE_ID
			settingName = L"Enable DLSS-SR override";
			pSetting->settingType = NVDRS_DWORD_TYPE;
			pSetting->u32CurrentValue = 0;
			break;
		case NGX_DLSS_FG_OVERRIDE_ID:  // 0x10E41E03
			settingName = L"Enable DLSS-FG override";
			pSetting->settingType = NVDRS_DWORD_TYPE;
			pSetting->u32CurrentValue = NGX_DLSS_FG_OVERRIDE_OFF;
			break;
		case NGX_DLSSG_MULTI_FRAME_COUNT_ID:  // 0x104D6667
			settingName = L"Override DLSSG multi-frame count";
			pSetting->settingType = NVDRS_DWORD_TYPE;
			pSetting->u32CurrentValue = NGX_DLSSG_MULTI_FRAME_COUNT_DEFAULT;
			break;
		case 0x10AFB76B:
			settingName = L"Logging file";
			pSetting->settingType = NVDRS_WSTRING_TYPE;
			pSetting->isPredefinedValid = 1;
			pSetting->isCurrentPredefined = 1;
			wcscpy_s((wchar_t*)pSetting->wszCurrentValue, NVAPI_UNICODE_STRING_MAX, L"");
			break;
		case 0x10E41DF2:
			settingName = L"Undocumented (1)";
			pSetting->settingType = NVDRS_DWORD_TYPE;
			pSetting->u32CurrentValue = 0;
			break;
		case 0x10AFB764:
			settingName = L"Undocumented (2)";
			pSetting->settingType = NVDRS_DWORD_TYPE;
			pSetting->u32CurrentValue = 0;
			break;
		default:
		{
			wchar_t buf[16];
			swprintf(buf, 16, L"%08X", settingId);
			LOG_NVAPI_DEBUG(L"DRS_GetSetting: Unknown settingId=0x" + std::wstring(buf));
		}
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_SETTING_NOT_FOUND);
		}

		{
			wchar_t buf[16];
			swprintf(buf, 16, L"%08X", settingId);
			LOG_NVAPI_DEBUG(L"DRS_GetSetting (embedded): settingId=0x" + std::wstring(buf) + L" (" + settingName + L")");
		}

		pSetting->settingId = settingId;
		pSetting->settingLocation = NVDRS_CURRENT_PROFILE_LOCATION;
		pSetting->isCurrentPredefined = 0;
		pSetting->isPredefinedValid = 1;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_GPU_GetAdapterIdFromPhysicalGpu(NvPhysicalGpuHandle hPhysicalGpu, void* pOSAdapterId)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (pOSAdapterId) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		memcpy(pOSAdapterId, &e->desc.AdapterLuid, sizeof(e->desc.AdapterLuid));
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_Vulkan_InitLowLatencyDevice()
	{
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NOT_SUPPORTED);
	}

	NvAPI_Status __cdecl NvAPI_Vulkan_DestroyLowLatencyDevice()
	{
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_NOT_SUPPORTED);
	}

	NvAPI_Status __cdecl NvAPI_D3D12_GetGraphicsCapabilities(ID3D12Device* pDevice, NvU32 structVersion, NV_D3D12_GRAPHICS_CAPS* pGraphicsCaps)
	{
		LOG_NVAPI_FUNCTION_CALL();
		if (pDevice == nullptr || structVersion != NV_D3D12_GRAPHICS_CAPS_VER1) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		bool isVariablePixelRateShadingSupported = false;
		if (GPU::GetShaderModelCapabilitiesD3D12(pDevice, &pGraphicsCaps->majorSMVersion, &pGraphicsCaps->minorSMVersion, &isVariablePixelRateShadingSupported)) {
			pGraphicsCaps->bVariablePixelRateShadingSupported = isVariablePixelRateShadingSupported;
			pGraphicsCaps->bExclusiveScissorRectsSupported = ctx.realGpuArchitecture >= NV_GPU_ARCHITECTURE_GK100;
			pGraphicsCaps->bFastUAVClearSupported = ctx.realGpuArchitecture >= NV_GPU_ARCHITECTURE_GK100;
		}

		LOG_NVAPI_WARNING(L"SM version: " + std::to_wstring(pGraphicsCaps->majorSMVersion) + L"." + std::to_wstring(pGraphicsCaps->minorSMVersion) + L" (VRS supported: " + std::to_wstring(isVariablePixelRateShadingSupported) + L", others: " + std::to_wstring(pGraphicsCaps->bFastUAVClearSupported) + L")");
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NVAPI_INTERFACE NvAPI_GPU_GetShaderSubPipeCount(NvPhysicalGpuHandle hPhysicalGpu, NvU32* pCount)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pCount) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		*pCount = 1024; // @todo: implement AMD and Intel paths, NVIDIA is not needed here, as in proxy mode this function is not used.

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NVAPI_INTERFACE NvAPI_GPU_GetScanoutConfigurationEx(NvU32 displayId, NV_SCANOUT_INFORMATION* pScanoutInformation)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pScanoutInformation) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NVAPI_INTERFACE NvAPI_GPU_GetSystemType(
		NvPhysicalGpuHandle hPhysicalGpu,
		NV_SYSTEM_TYPE* pSystemType
	)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (!pSystemType) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		std::lock_guard<std::mutex> lock(nvapiMutex);
		PhysicalGpuEntry* e = UnpackNvPhysicalGpuHandle(hPhysicalGpu);
		if (!e) {
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
		}

		SYSTEM_POWER_STATUS ps = {};
		if (GetSystemPowerStatus(&ps)) {
			switch (ps.BatteryFlag) {
			case BATTERY_FLAG_UNKNOWN:
				*pSystemType = NV_SYSTEM_TYPE_UNKNOWN;
				break;
			case BATTERY_FLAG_NO_BATTERY:
				*pSystemType = NV_SYSTEM_TYPE_DESKTOP; // close enough, laptop without battery and on AC is like desktop;)
				break;
			default:
				*pSystemType = NV_SYSTEM_TYPE_LAPTOP;
				break;
			}
		}

		if (*pSystemType == NV_SYSTEM_TYPE_DESKTOP || *pSystemType == NV_SYSTEM_TYPE_UNKNOWN) {
			SYSTEM_POWER_CAPABILITIES caps = {};
			GetPwrCapabilities(&caps);

			if (!caps.LidPresent) {
				*pSystemType = NV_SYSTEM_TYPE_LAPTOP; // now we're almost sure its a laptop...
			}
		}

		LOG_NVAPI_DEBUG(L"System type: " + (*pSystemType == NV_SYSTEM_TYPE_DESKTOP ? L"DESKTOP" : (*pSystemType == NV_SYSTEM_TYPE_UNKNOWN ? L"UNKNOWN" : L"LAPTOP")));

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D12_QueryCpuVisibleVidmem(ID3D12Device* pDevice, uint64_t* pTotalBytes, uint64_t* pFreeBytes)
	{
		NvAPI_Status result;
		static bool logged = false;
		if (!logged) {
			LOG_NVAPI_FUNCTION_CALL();
		}

		if (!pTotalBytes || !pFreeBytes || !pDevice) {
			if (!logged) {
				logged = true;
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_INVALID_ARGUMENT);
			}
			else {
				return NVAPI_INVALID_ARGUMENT;
			}
		}

		result = GPU::TryGetCpuVisibleVidmemD3D12(pDevice, pTotalBytes, pFreeBytes) ? NVAPI_OK : NVAPI_NOT_SUPPORTED;

		if (!logged) {
			logged = true;
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(result);
		}

		return result;
	}

	NvAPI_Status __cdecl NvAPI_CallStart()
	{
		static bool logged = false;

		if (logged) {
			return NVAPI_OK;
		}

		logged = true;
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_CallReturn()
	{
		static bool logged = false;

		if (logged) {
			return NVAPI_OK;
		}

		logged = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_UnloadEx()
	{
		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvLL_VK_Unknown()
	{
		static bool logged = false;

		if (logged) {
			return NVAPI_OK;
		}

		logged = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl fake_NvLL_VK_GetLatency(VkDevice device, NV_LATENCY_RESULT_PARAMS* pGetLatencyParams)
	{
		static bool logged = false;
		static NvU64 id = streamlineSignalId;

		pGetLatencyParams->version = 1;

		for (int i = 0; i < 64; ++i) {
			pGetLatencyParams->frameReport[i].frameID = id + i;
			pGetLatencyParams->frameReport[i].inputSampleTime = 1000 + i * 10;
			pGetLatencyParams->frameReport[i].simStartTime = 2000 + i * 10;
			pGetLatencyParams->frameReport[i].simEndTime = 3000 + i * 10;
			pGetLatencyParams->frameReport[i].renderSubmitStartTime = 4000 + i * 10;
			pGetLatencyParams->frameReport[i].renderSubmitEndTime = 5000 + i * 10;
			pGetLatencyParams->frameReport[i].presentStartTime = 6000 + i * 10;
			pGetLatencyParams->frameReport[i].presentEndTime = 7000 + i * 10;
			pGetLatencyParams->frameReport[i].driverStartTime = 8000 + i * 10;
			pGetLatencyParams->frameReport[i].driverEndTime = 9000 + i * 10;
			pGetLatencyParams->frameReport[i].osRenderQueueStartTime = 10000 + i * 10;
			pGetLatencyParams->frameReport[i].osRenderQueueEndTime = 11000 + i * 10;
			pGetLatencyParams->frameReport[i].gpuRenderStartTime = 12000 + i * 10;
			pGetLatencyParams->frameReport[i].gpuRenderEndTime = 13000 + i * 10;

			// Mock GPU active render time and frame time
			pGetLatencyParams->frameReport[i].gpuActiveRenderTimeUs = 500 + i * 2; // Mocked value in microseconds
			pGetLatencyParams->frameReport[i].gpuFrameTimeUs = 16000 + i * 2;      // Mocked value in microseconds

			// Fill reserved area with zero
			memset(pGetLatencyParams->frameReport[i].rsvd, 0, sizeof(pGetLatencyParams->frameReport[i].rsvd));
		}

		// Fill reserved area in the structure with zero
		memset(pGetLatencyParams->rsvd, 0, sizeof(pGetLatencyParams->rsvd));

		if (logged) {
			return NVAPI_OK;
		}

		logged = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D12_CreateCubinComputeShaderWithName(ID3D12Device* pDevice, const void* cubinData, NvU32 cubinSize, NvU32 blockX, NvU32 blockY,
		NvU32 blockZ, const char* shaderName, NVDX_ObjectHandle* pShader)
	{
		LOG_NVAPI_FUNCTION_CALL();

		if (true) {
			std::string sShaderName = std::string(shaderName ? shaderName : "[null]");
			std::wstring wShaderName = std::wstring(sShaderName.begin(), sShaderName.end());
			LOG_NVAPI_INFO(L"NvAPI_D3D12_CreateCubinComputeShader: call handled for shader " + wShaderName);
		}

		NvAPI_Status result = (NvAPI_Status)org_NvAPI_D3D12_CreateCubinComputeShader(pDevice, cubinData, cubinSize, blockX, blockY, blockZ, shaderName, pShader);

		if (ctx.emulation.forceHighestArch) {
			if (result != NVAPI_OK) {
				//LOG_NVAPI_ERROR(L"NvAPI_D3D12_CreateCubinComputeShaderWithName: failed (" + std::to_wstring(result) + L"), faking success");
				//LOG_NVAPI_ERROR(L"NvAPI_D3D12_CreateCubinComputeShaderWithName: failed (" + std::to_wstring(result) + L"), faking success");
			}
			//LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
		}

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(result);
	}


	// faked functions with missing args!!!!
	////////////////////////////////////////
	NvAPI_Status NvAPI_SetMarker(NV_LATENCY_MARKER_TYPE markerType)
	{
		return NVAPI_OK;
	}

	NvAPI_Status __cdecl NvAPI_D3D_SetLatencyMarker(IUnknown* pDev, NV_LATENCY_MARKER_PARAMS* pSetLatencyMarkerParams)
	{
		static bool logged = false;

		if (!logged) {
			LOG_NVAPI_FUNCTION_CALL();
		}

		// Extract marker info for dispatch
		uint64_t frameId = pSetLatencyMarkerParams ? pSetLatencyMarkerParams->frameID : 0;
		uint32_t markerType = pSetLatencyMarkerParams ? (uint32_t)pSetLatencyMarkerParams->markerType : 0;

		// DISPATCH PRE-EVENT
		ReflexEvents::DispatchPreSetLatencyMarker(pDev, frameId, markerType);

		bool useNativeReflex = OverdriveController::GetReflexEnabled();

		//if (!ctx.reflex.isEmulationEnabled && !ctx.enableReflexInjection) {
		if (org_NvAPI_D3D_SetLatencyMarker && useNativeReflex) {
			auto result = org_NvAPI_D3D_SetLatencyMarker(pDev, pSetLatencyMarkerParams);
			if (!logged) {
				logged = true;
				ReflexEvents::DispatchPostSetLatencyMarker(pDev, frameId, markerType, result);
				LOG_NVAPI_FUNCTION_CALL_AND_RETURN(result);
			}

			// DISPATCH POST-EVENT
			ReflexEvents::DispatchPostSetLatencyMarker(pDev, frameId, markerType, result);
			return result;
		}
		//}


		// DISPATCH POST-EVENT
		ReflexEvents::DispatchPostSetLatencyMarker(pDev, frameId, markerType, NVAPI_OK);

		if (logged) {
			return NVAPI_OK;
		}

		logged = true;

		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D_GetLatency(IUnknown* pDev, NV_LATENCY_RESULT_PARAMS* pGetLatencyParams)
	{
		static bool logged = false;
		NvAPI_Status status = NVAPI_OK;
		if (!logged) {
			LOG_NVAPI_FUNCTION_CALL();
		}

		// DISPATCH PRE-EVENT
		ReflexEvents::DispatchPreGetLatency(pDev, pGetLatencyParams);

		if (org_NvAPI_D3D_GetLatency) {
			status = org_NvAPI_D3D_GetLatency(pDev, pGetLatencyParams);
		}

		// DISPATCH POST-EVENT
		ReflexEvents::DispatchPostGetLatency(pDev, pGetLatencyParams, status);

		if (!logged) {
			logged = true;
			LOG_NVAPI_FUNCTION_CALL_AND_RETURN(status);

		}
		return status;
	}

	NvAPI_Status __cdecl NvAPI_D3D_GetSleepStatus()
	{
		static bool logged = false;

		if (logged) {
			return NVAPI_OK;
		}

		logged = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(NVAPI_OK);
	}

	NvAPI_Status __cdecl NvAPI_D3D_Sleep(void* nvApiDevice)
	{
		static uint64_t amountOfFramesSinceLastFrameSkip = 0;
		static uint64_t latencyExtra = 0;
		static uint64_t frameId = 0;
		NvAPI_Status result = NVAPI_OK;
		static bool isReflexEmulationReported = false;
		currentTimeMsec = Common::GetCurrentTimeMsec();
		static bool logged = false;
		static bool isReflexLimitReported = false;
		static bool isReflexErrorReported = false;
		static unsigned int lastSleep = 0;
		unsigned int sleep;
		static uint64_t lastSleepCallTime = 0;
		uint64_t currentSleepCallTime = Common::GetCurrentTimeMsec();
		// Static variables for periodic GetLatency call
		static double lastGetLatencyTimeMsec = 0.0;
		static const double GET_LATENCY_INTERVAL_MSEC = 500.0;  // Call GetLatency every 500ms

		// DISPATCH PRE-EVENT
		ReflexEvents::DispatchPreSleep(nvApiDevice);


		static bool isDebugPresented = false;

		if (!isDebugPresented) {
			LOG_NVAPI_DEBUG(L"NvAPI_D3D_Sleep: will try to call real sleep function");
		}
		if (!org_NvAPI_D3D_Sleep) {
			if (!isReflexErrorReported) {
				LOG_ERROR(L"[RLFX] Failed to get Sleep pointer");
				isReflexErrorReported = true;
			}
		}
		else {
			float divider = 1;
			ctx.reflex.realFpsLimit = OverdriveController::GetDesiredFpsLimit();
			if (ctx.ngx.lastEvaluationTimeMsec > 0.0f) { // DLSSG is enabled....
				if (!OverdriveController::GetDynamicFrameGenerationEnabled() && (ctx.nvapi.isMockEnabled || !ctx.nvapi.isGenuineFileLoaded || ctx.nvapi.isEmbeddedNvapiUsed)) {
					ctx.reflex.realFpsLimit = ((double)OverdriveController::GetDesiredFpsLimit()) / (ctx.ngx.framesGenerated + 1); // Only real Reflex is aware of extra frames generated by DLSSG
				}
			}
			if (OverdriveController::GetFpsLimitEnabled()) {
				sleep = GetReflexFrameTimeDuration();
			}
			else {
				if (ctx.ngx.lastEvaluationTimeMsec > 0.0f && !ctx.nvapi.isGenuineFileLoaded) {
					divider = (ctx.ngx.framesGenerated + 1); // Only real Reflex is aware of extra frames generated by DLSSG, Linux code is not
				}

				//LOG_WARNING(L"DIVIDING BY " + std::to_wstring(divider));

				sleep = reflexMinimumIntervalUsSetByApp;
			}

			static int lastFramesGenerated = 0;
			// Check if Overdrive disabled Reflex - use C++ sleep fallback
			bool useNativeReflex = OverdriveController::GetReflexEnabled();

			// if DFG has just been disabled, reset the Reflex settings, do the same if FPS cap has been changed
			if (lastSleep != sleep || currentSleepCallTime > (lastSleepCallTime + 100)) {
				

				NvAPI_Status status = NVAPI_OK;
				NV_SET_SLEEP_MODE_PARAMS_V1 setSleepModeParams = { 0 };
				setSleepModeParams.version = NV_SET_SLEEP_MODE_PARAMS_VER1;
				setSleepModeParams.bLowLatencyBoost = OverdriveController::GetBoostOverriden() ? OverdriveController::GetBoostEnabled() : ctx.reflex.isBoostOriginallyEnabled;
				setSleepModeParams.bLowLatencyMode = useNativeReflex ? ctx.reflex.isOriginallyEnabled : 0;
				setSleepModeParams.bUseMarkersToOptimize = ctx.reflex.isMarkersOptimizationEnabled;
				setSleepModeParams.minimumIntervalUs = useNativeReflex ? sleep : 0;  // Don't set interval if using fallback
				status = org_NvAPI_D3D_SetSleepMode(nvApiDevice, &setSleepModeParams);
				if (status != NVAPI_OK) {
					if (!isReflexErrorReported) {
						LOG_ERROR(L"[RLFX] Failed to configure native Reflex feature, reverting to the built-in emulator");
						isReflexErrorReported = true;
					}
				}
				else {
					if (lastSleep != sleep) {
						LOG_DEBUG(L"[RLFX] NvAPI_D3D_Sleep: setting minimumIntervalUs to " + std::to_wstring(useNativeReflex ? sleep : 0));
					}
				}

				lastSleep = sleep;
			}

			lastFramesGenerated = ctx.ngx.framesGenerated;

			if (!isReflexLimitReported) {
				LOG_INFO(L"[RLFX] Enabling native Reflex integration for frame limiting and compensation");
				isReflexLimitReported = true;
				isReflexErrorReported = false;
			}

			if (!isDebugPresented) {
				LOG_NVAPI_DEBUG(L"NvAPI_D3D_Sleep: proxying sleep call");
			}

			// Use C++ sleep fallback when Overdrive disables Reflex (PERF mode)
			// This avoids NVAPI sleep overhead entirely for maximum performance
			if (!useNativeReflex) {
				// Only sleep if FPS limit is enabled and sleep > 0
				if (sleep > 0) {
					// Track time since last sleep for compensation
					static double lastSleepTimeMsec = 0.0;

					// Calculate elapsed time since last sleep (in microseconds)
					double elapsedMsec = currentTimeMsec - lastSleepTimeMsec;
					unsigned int elapsedUs = static_cast<unsigned int>(elapsedMsec * 1000.0);

					// Compensate: only sleep for remaining time
					if (elapsedUs < sleep) {
						unsigned int compensatedSleep = sleep - elapsedUs;
						std::this_thread::sleep_for(std::chrono::microseconds(compensatedSleep));
					}
					// else: frame took longer than target, skip sleep

					lastSleepTimeMsec = Common::GetCurrentTimeMsec();  // Update after sleep
				}
				// If no FPS limit, don't sleep at all - maximum performance

				result = NVAPI_OK;

				static bool fallbackReported = false;
				if (!fallbackReported) {
					LOG_INFO(L"[RLFX] Using C++ sleep fallback - bypassing NVAPI sleep for maximum performance (Overdrive PERF mode)");
					fallbackReported = true;
				}
			}
			else {
				// Use native NVIDIA Reflex sleep
				result = org_NvAPI_D3D_Sleep(nvApiDevice);
			}

			// Periodically call GetLatency (every 500ms) as a fallback in case no one else is calling it
			if (org_NvAPI_D3D_GetLatency && (currentTimeMsec - lastGetLatencyTimeMsec >= GET_LATENCY_INTERVAL_MSEC))
			{
				lastGetLatencyTimeMsec = currentTimeMsec;

				NV_LATENCY_RESULT_PARAMS latencyParams = {};
				latencyParams.version = NV_LATENCY_RESULT_PARAMS_VER;

				// DISPATCH PRE-EVENT
				ReflexEvents::DispatchPreGetLatency(nvApiDevice, &latencyParams);

				NvAPI_Status latencyStatus = org_NvAPI_D3D_GetLatency((IUnknown*)nvApiDevice, &latencyParams);

				// DISPATCH POST-EVENT
				ReflexEvents::DispatchPostGetLatency(nvApiDevice, &latencyParams, latencyStatus);
			}
		}

		isDebugPresented = true;

		// DISPATCH POST-EVENT
		ReflexEvents::DispatchPostSleep(nvApiDevice, result);

		if (logged) {
			return result;
		}

		logged = true;

		LOG_NVAPI_FUNCTION_CALL();
		LOG_NVAPI_FUNCTION_CALL_AND_RETURN(result);
	}
}