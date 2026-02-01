#include "Nvapi64Dispatch.h"
#include "Nvapi64.h"
#include "Common.h"
#include "FakeNVAPI.h"

namespace NVAPI
{
	static GetProcAddress_t OrgGetProcAddress = ::GetProcAddress;

	void* __cdecl NvAPI_QueryInterface(unsigned int function)
	{
		LOG_NVAPI_DEBUG(L"NvAPI_QueryInterface: " + GetNvAPIFunctionName(function) + L" queried");

		// no statics, no caching, as some engines might load/unload NVAPI multiple times
		// it seems its safe to assume that the latest NVAPI instance is OK to use.
		auto realNvAPI = GetModuleHandleW(L"nvapi64.dll");
		if (realNvAPI && !ctx.nvapi.isEmbeddedNvapiUsed) {
			OriginalNvAPI_QueryInterface = (nvapi_QueryInterface_t)OrgGetProcAddress(realNvAPI, "nvapi_QueryInterface");
		}
		else {
			OriginalNvAPI_QueryInterface = (nvapi_QueryInterface_t)&nvapi_QueryInterface;
		}

		if (OriginalNvAPI_QueryInterface) {
			if (!org_NvAPI_D3D_GetLatency) {
				org_NvAPI_D3D_GetLatency = (NvAPI_D3D_GetLatency_t)OriginalNvAPI_QueryInterface(NVAPI_D3D_GET_LATENCY);
			}

			if (!org_NvAPI_D3D_Sleep) {
				org_NvAPI_D3D_Sleep = (NvAPI_D3D_Sleep_t)OriginalNvAPI_QueryInterface(NVAPI_D3D_SLEEP);
			}

			if (!org_NvAPI_D3D_SetSleepMode) {
				org_NvAPI_D3D_SetSleepMode = (NvAPI_D3D_SetSleepMode_t)OriginalNvAPI_QueryInterface(NVAPI_D3D_SET_SLEEP_MODE);
			}

			if (!org_NvAPI_D3D_SetLatencyMarker) {
				org_NvAPI_D3D_SetLatencyMarker = (NvAPI_D3D_SetLatencyMarker_t)OriginalNvAPI_QueryInterface(NVAPI_D3D_SET_LATENCY_MARKER);
			}

			if (!org_NvAPI_D3D12_SetAsyncFrameMarker) {
				org_NvAPI_D3D12_SetAsyncFrameMarker = (NvAPI_D3D12_SetAsyncFrameMarker_t)OriginalNvAPI_QueryInterface(0x13c98f73);
			}

			if (!org_NvAPI_GPU_GetArchInfo) {
				org_NvAPI_GPU_GetArchInfo = (NvAPI_GPU_GetArchInfo_t)OriginalNvAPI_QueryInterface(NVAPI_GPU_GET_ARCH_INFO);
			}

			if (function == NVAPI_D3D_SLEEP) {
				org_NvAPI_D3D_Sleep = (NvAPI_D3D_Sleep_t)OriginalNvAPI_QueryInterface(function);
			}
		}

		// 692
		switch (function) {
			case NVAPI_INITIALIZE: return NvAPI_Initialize;
			case 0xad298d3fL: return NvAPI_InitializeEx; // implementing missing function
			case NVAPI_UNLOAD_EX: return NvAPI_UnloadEx; // implementing missing function
			case 0xd22bdd7e: return NvAPI_Unload;
			case NVAPI_D3D_SET_SLEEP_MODE: return NvAPI_D3D_SetSleepMode;
			case NVAPI_D3D_GET_LATENCY: return NvAPI_D3D_GetLatency;
			case NVAPI_D3D_SLEEP: return NvAPI_D3D_Sleep;
			case NVAPI_GPU_GET_ARCH_INFO: return NvAPI_GPU_GetArchInfo;
			case NVAPI_D3D_SET_LATENCY_MARKER: return NvAPI_D3D_SetLatencyMarker;
		}

		if (!ctx.nvapi.isEmbeddedNvapiUsed && ctx.quickBoot) switch (function) {
			case 0xa782ea46L: return NvAPI_DRS_LoadGoldSettings;
			case 0x375dbd6bL: return NvAPI_DRS_LoadSettings;
			case 0xda8466a0L: return NvAPI_DRS_GetBaseProfile;
			case 0x0694d52eL: return NvAPI_DRS_CreateSession;
			case 0x73bf8338L: return NvAPI_DRS_GetSetting;
			case 0xdad9cff8L: return NvAPI_DRS_DestroySession;
			case 0xeee566b2L: return NvAPI_DRS_FindApplicationByName;
			case 0x61cd6fd6L: return NvAPI_DRS_GetProfileInfo;
		}

		if (ctx.nvapi.isEmbeddedNvapiUsed) {
			switch (function) {
			case 0xf2400abL:					return NvAPI_GPU_GetValuesFromInstalledINF;
			case 0x01053fa5:					return NvAPI_GetInterfaceVersionString;
			case NVAPI_SYS_GET_DRIVER_VERSION:	return NvAPI_SYS_GetDriverAndBranchVersion;

				// needs further testing...
			case NVAPI_CALL_START:  return NvAPI_CallStart;
			case NVAPI_CALL_RETURN: return NvAPI_CallReturn;

				//case NVAPI_D3D_SET_LATENCY_MARKER: return NvAPI_D3D_SetLatencyMarker;
				//case NVAPI_D3D_GET_LATENCY:        return NvAPI_D3D_GetLatency;
				//case NVAPI_D3D_GET_SLEEP_STATUS:   return NvAPI_D3D_GetSleepStatus;




			case 0xfceac864:  return NvAPI_D3D_GetObjectHandleForResource;


				
			case 0x721FACEBL:	return NvAPI_SYS_GetDisplayDriverInfo;
			case 0x112ba1a5L:	return NvAPI_SYS_GetGpuAndOutputIdFromDisplayId;
			case 0x08f2bab4L:	return NvAPI_SYS_GetDisplayIdFromGpuAndOutputId;
			case 0x2ddfb66eL:				return NvAPI_GPU_GetPCIIdentifiers;
			case 0xc0599498L:				return NvAPI_GPU_GetMemoryInfoEx;
			case 0x0be17923L:				return NvAPI_GPU_GetShaderSubPipeCount;
			case 0x40a505e4L:				return NvAPI_GPU_GetOutputType;
			case 0xbaaabfccL:				return NvAPI_GPU_GetSystemType;
			case 0xceee8e9fL:				return NvAPI_GPU_GetFullName;
			case NVAPI_GPU_QUERY_NODE_INFO: return NvAPI_GPU_QueryNodeInfo;
			case NVAPI_ENUM_PHYSICAL_GPUS:	return NvAPI_EnumPhysicalGPUs;
			case NVAPI_ENUM_LOGICAL_GPUS:	return NvAPI_EnumLogicalGPUs;
			case 0xdcb616c3L:				return NvAPI_GPU_GetAllClockFrequencies;
			case 0xc7026a87L:				return NvAPI_GPU_GetGpuCoreCount;
			case 0x6ff81213L:				return NvAPI_GPU_GetPstates20;
			case 0x5786cc6eL:				return NvAPI_GPU_CudaEnumComputeCapableGpus;
			case 0x842b066eL:				return NvAPI_GPU_GetLogicalGpuInfo; // overriding nvapi64 proxy call to use consistent LUID selected by fake_NvAPI_Initialize
			case 0xadd604d1L:				return NvAPI_GetLogicalGPUFromPhysicalGPU;
			case 0x6533ea3eL:				return NvAPI_GetGPUIDfromPhysicalGPU;
			case 0x5380ad1aL:				return NvAPI_GetPhysicalGPUFromGPUID;
			case 0x0ff07fdeL:				return NvAPI_GPU_GetAdapterIdFromPhysicalGpu; // overriding nvapi64 proxy call to use consistent LUID selected by fake_NvAPI_Initialize

			// D3D
			case 0x4b708b54L: return NvAPI_D3D_GetCurrentSLIState;
			case 0xd451e834L: return NvAPI_Success; // NvAPI_D3D_UpdateSLIMask

			// D3D12
			case 0x299f5fdcL:					return nullptr; // NvAPI_D3D12_CreateCubinComputeShaderExV2
			case 0x3151211bL:					return nullptr; // NvAPI_D3D12_CreateCubinComputeShaderEx
			case 0x2a2c79e8L:					return nullptr; // NvAPI_D3D12_CreateCubinComputeShader
			case 0x1dc7261fL:					return nullptr; // NvAPI_D3D12_CreateCubinComputeShaderWithName
			case 0x70c07832L:					return NvAPI_D3D12_IsFatbinPTXSupported;
			case 0x3dfacec8L:					return NvAPI_D3D12_IsNvShaderExtnOpCodeSupported;
			case 0x43d867c0L:					return NvAPI_D3D12_SetNvShaderExtnSlotSpaceLocalThread;
			case 0x8d025b77L:					return NvAPI_D3D12_GetRaytracingAccelerationStructurePrebuildInfoEx;
			case 0x01e87354L:					return NvAPI_D3D12_GetGraphicsCapabilities;
			case 0x85a6c2a0L:					return NvAPI_D3D12_GetRaytracingCaps;
			case 0xe24ead45L:					return NvAPI_D3D12_BuildRaytracingAccelerationStructureEx;
			case NVAPI_D3D12_QUERY_CPU_VIDMEM:	return NvAPI_D3D12_QueryCpuVisibleVidmem;

			// DRS
			case 0xa782ea46L: return NvAPI_DRS_LoadGoldSettings;
			case 0x375dbd6bL: return NvAPI_DRS_LoadSettings;
			case 0xda8466a0L: return NvAPI_DRS_GetBaseProfile;
			case 0x0694d52eL: return NvAPI_DRS_CreateSession;
			case 0x73bf8338L: return NvAPI_DRS_GetSetting;
			case 0xdad9cff8L: return NvAPI_DRS_DestroySession;
			case 0xeee566b2L: return NvAPI_DRS_FindApplicationByName;
			case 0x61cd6fd6L: return NvAPI_DRS_GetProfileInfo;
			//case 0x73bf8338L: return NvAPI_DRS_GetSettingProxy;

			// DISPLAY
			case 0x11abccf8L: return NvAPI_DISP_GetDisplayConfig;
			case 0x1e9d8a31L: return NvAPI_DISP_GetGDIPrimaryDisplayId;
			case 0x81fed88dL: return NvAPI_Disp_GetOutputMode;
			case 0x98e7661aL: return NvAPI_Disp_SetOutputMode;
			case 0xae457190L: return NvAPI_DISP_GetDisplayIdByDisplayName;
			case 0x9abdd40dL: return NvAPI_EnumNvidiaDisplayHandle;
			case 0xdc6dc8d3L: return NvAPI_Mosaic_GetDisplayViewportsByResolution;
			case 0x348ff8e1L: return NvAPI_Stereo_IsEnabled;
			case 0x84f2a8dfL: return NvAPI_Disp_GetHdrCapabilities;
			//*/
	
			case 0xDBE53CB2L: return NvAPI_NotSupported; // NvAPI_D3D12_Aftermath_Initialize
			case 0xb2e3e2a2L: return NvAPI_NotSupported; // NvAPI_D3D12_Aftermath_GetContextData
			case 0x633D88E1L: return NvAPI_NotSupported; // NvAPI_D3D12_Aftermath_GetDeviceStatus
			case 0x8C68F0F1L: return NvAPI_NotSupported; // NvAPI_D3D12_Aftermath_SetMarker

			//case 0xc83c4d5dL: return NvLL_VK_0xc83c4d5d; // ????

			// FakeAPI
			case 0x21372137L:
			case 0x21382138L:
			case 0x21392139L:
			case 0x21402140L:
			case 0x21412141L:
			case 0x21422142L:
				return nvapi_QueryInterface(function);
				/*
			case 0x5d6d3840L: return NvAPI_Vulkan_NotifyOutOfBandVkQueue;
			//case 0x11a5932bL: return NvAPI_Vulkan_DestroyLowLatencyDevice;
			//case 0x5c1696b6L: return NvAPI_Vulkan_InitLowLatencyDevice;
			case 0x11a5932bL: return nvapi_QueryInterface(function); ; // NvAPI_Vulkan_DestroyLowLatencyDevice;
			//case 0x5c1696b6L: return nvapi_QueryInterface(function); ; // NvAPI_Vulkan_InitLowLatencyDevice;
			case 0x36732b1eL: return nvapi_QueryInterface(function); // sleep
			case 0x2acfd162L: return nvapi_QueryInterface(function);
				//case 0xf0f4a5e0L: return fake_NvLL_VK_Unknown; // for NMS
			case 0x3233d44aL: return nvapi_QueryInterface(function); // NvLL_VK_GetLatency
			case 0xa17d13d6L: return nvapi_QueryInterface(function); // NvLL_VK_SetLatencyMarker(m_device, &params)
			case 0xadf966afL: return nvapi_QueryInterface(function); // NvLL_VK_GetSleepStatus
			*/

			}

			if (function == 0x64d6c83d) { // sth acquireNextBufferIndex related... generates warnings about timeouts in SL
				return NvLL_VK_Unknown; // crashes Opti when enabling DLSS
			}
		}

		auto result = OriginalNvAPI_QueryInterface(function);
		//void* result = nullptr;
		
		if (function == 0x1dc7261fL && ctx.emulation.forceHighestArch) {
			org_NvAPI_D3D12_CreateCubinComputeShader = (NvAPI_D3D12_CreateCubinComputeShaderWithName_t)result;
			return NvAPI_D3D12_CreateCubinComputeShaderWithName;
		} 

		if (nullptr == result) {
			LOG_NVAPI_ERROR(L"NvAPI_QueryInterface: " + GetNvAPIFunctionName(function) + L" not implemented");
		}
		else {
			LOG_NVAPI_INFO(L"NvAPI_QueryInterface: " + GetNvAPIFunctionName(function) + L" delegated");
		}

		return result;
	}
}