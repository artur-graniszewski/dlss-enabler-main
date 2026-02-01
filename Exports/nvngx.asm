ifdef RAX
	.code
		extern OriginalFuncs_nvngx:QWORD
		NVSDK_NGX_CUDA_AllocateParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[0 * 8]
		NVSDK_NGX_CUDA_AllocateParameters endp
		NVSDK_NGX_CUDA_CreateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[1 * 8]
		NVSDK_NGX_CUDA_CreateFeature endp
		NVSDK_NGX_CUDA_DestroyParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[2 * 8]
		NVSDK_NGX_CUDA_DestroyParameters endp
		NVSDK_NGX_CUDA_EvaluateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[3 * 8]
		NVSDK_NGX_CUDA_EvaluateFeature endp
		NVSDK_NGX_CUDA_GetCapabilityParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[4 * 8]
		NVSDK_NGX_CUDA_GetCapabilityParameters endp
		NVSDK_NGX_CUDA_GetParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[5 * 8]
		NVSDK_NGX_CUDA_GetParameters endp
		NVSDK_NGX_CUDA_GetScratchBufferSize proc
			jmp QWORD ptr OriginalFuncs_nvngx[6 * 8]
		NVSDK_NGX_CUDA_GetScratchBufferSize endp
		NVSDK_NGX_CUDA_Init proc
			jmp QWORD ptr OriginalFuncs_nvngx[7 * 8]
		NVSDK_NGX_CUDA_Init endp
		NVSDK_NGX_CUDA_Init_Ext proc
			jmp QWORD ptr OriginalFuncs_nvngx[8 * 8]
		NVSDK_NGX_CUDA_Init_Ext endp
		NVSDK_NGX_CUDA_Init_ProjectID proc
			jmp QWORD ptr OriginalFuncs_nvngx[9 * 8]
		NVSDK_NGX_CUDA_Init_ProjectID endp
		NVSDK_NGX_CUDA_ReleaseFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[10 * 8]
		NVSDK_NGX_CUDA_ReleaseFeature endp
		NVSDK_NGX_CUDA_Shutdown proc
			jmp QWORD ptr OriginalFuncs_nvngx[11 * 8]
		NVSDK_NGX_CUDA_Shutdown endp
		NVSDK_NGX_D3D11_AllocateParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[12 * 8]
		NVSDK_NGX_D3D11_AllocateParameters endp
		NVSDK_NGX_D3D11_CreateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[13 * 8]
		NVSDK_NGX_D3D11_CreateFeature endp
		NVSDK_NGX_D3D11_DestroyParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[14 * 8]
		NVSDK_NGX_D3D11_DestroyParameters endp
		NVSDK_NGX_D3D11_EvaluateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[15 * 8]
		NVSDK_NGX_D3D11_EvaluateFeature endp
		NVSDK_NGX_D3D11_GetCapabilityParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[16 * 8]
		NVSDK_NGX_D3D11_GetCapabilityParameters endp
		NVSDK_NGX_D3D11_GetFeatureRequirements proc
			jmp QWORD ptr OriginalFuncs_nvngx[17 * 8]
		NVSDK_NGX_D3D11_GetFeatureRequirements endp
		NVSDK_NGX_D3D11_GetParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[18 * 8]
		NVSDK_NGX_D3D11_GetParameters endp
		NVSDK_NGX_D3D11_GetScratchBufferSize proc
			jmp QWORD ptr OriginalFuncs_nvngx[19 * 8]
		NVSDK_NGX_D3D11_GetScratchBufferSize endp
		NVSDK_NGX_D3D11_Init proc
			jmp QWORD ptr OriginalFuncs_nvngx[20 * 8]
		NVSDK_NGX_D3D11_Init endp
		NVSDK_NGX_D3D11_Init_Ext proc
			jmp QWORD ptr OriginalFuncs_nvngx[21 * 8]
		NVSDK_NGX_D3D11_Init_Ext endp
		NVSDK_NGX_D3D11_Init_ProjectID proc
			jmp QWORD ptr OriginalFuncs_nvngx[22 * 8]
		NVSDK_NGX_D3D11_Init_ProjectID endp
		NVSDK_NGX_D3D11_ReleaseFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[23 * 8]
		NVSDK_NGX_D3D11_ReleaseFeature endp
		NVSDK_NGX_D3D11_Shutdown proc
			jmp QWORD ptr OriginalFuncs_nvngx[24 * 8]
		NVSDK_NGX_D3D11_Shutdown endp
		NVSDK_NGX_D3D11_Shutdown1 proc
			jmp QWORD ptr OriginalFuncs_nvngx[25 * 8]
		NVSDK_NGX_D3D11_Shutdown1 endp
		NVSDK_NGX_D3D12_AllocateParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[26 * 8]
		NVSDK_NGX_D3D12_AllocateParameters endp
		NVSDK_NGX_D3D12_CreateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[27 * 8]
		NVSDK_NGX_D3D12_CreateFeature endp
		NVSDK_NGX_D3D12_DestroyParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[28 * 8]
		NVSDK_NGX_D3D12_DestroyParameters endp
		NVSDK_NGX_D3D12_EvaluateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[29 * 8]
		NVSDK_NGX_D3D12_EvaluateFeature endp
		NVSDK_NGX_D3D12_GetCapabilityParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[30 * 8]
		NVSDK_NGX_D3D12_GetCapabilityParameters endp
		NVSDK_NGX_D3D12_GetFeatureRequirements proc
			jmp QWORD ptr OriginalFuncs_nvngx[31 * 8]
		NVSDK_NGX_D3D12_GetFeatureRequirements endp
		NVSDK_NGX_D3D12_GetParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[32 * 8]
		NVSDK_NGX_D3D12_GetParameters endp
		NVSDK_NGX_D3D12_GetScratchBufferSize proc
			jmp QWORD ptr OriginalFuncs_nvngx[33 * 8]
		NVSDK_NGX_D3D12_GetScratchBufferSize endp
		NVSDK_NGX_D3D12_Init proc
			jmp QWORD ptr OriginalFuncs_nvngx[34 * 8]
		NVSDK_NGX_D3D12_Init endp
		NVSDK_NGX_D3D12_Init_Ext proc
			jmp QWORD ptr OriginalFuncs_nvngx[35 * 8]
		NVSDK_NGX_D3D12_Init_Ext endp
		NVSDK_NGX_D3D12_Init_ProjectID proc
			jmp QWORD ptr OriginalFuncs_nvngx[36 * 8]
		NVSDK_NGX_D3D12_Init_ProjectID endp
		NVSDK_NGX_D3D12_ReleaseFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[37 * 8]
		NVSDK_NGX_D3D12_ReleaseFeature endp
		NVSDK_NGX_D3D12_Shutdown proc
			jmp QWORD ptr OriginalFuncs_nvngx[38 * 8]
		NVSDK_NGX_D3D12_Shutdown endp
		NVSDK_NGX_D3D12_Shutdown1 proc
			jmp QWORD ptr OriginalFuncs_nvngx[39 * 8]
		NVSDK_NGX_D3D12_Shutdown1 endp
		NVSDK_NGX_OTA_UPDATES_CheckForUpdate proc
			jmp QWORD ptr OriginalFuncs_nvngx[40 * 8]
		NVSDK_NGX_OTA_UPDATES_CheckForUpdate endp
		NVSDK_NGX_OTA_UPDATES_GetPath proc
			jmp QWORD ptr OriginalFuncs_nvngx[41 * 8]
		NVSDK_NGX_OTA_UPDATES_GetPath endp
		NVSDK_NGX_OTA_UPDATES_Install proc
			jmp QWORD ptr OriginalFuncs_nvngx[42 * 8]
		NVSDK_NGX_OTA_UPDATES_Install endp
		NVSDK_NGX_OTA_UPDATES_Register proc
			jmp QWORD ptr OriginalFuncs_nvngx[43 * 8]
		NVSDK_NGX_OTA_UPDATES_Register endp
		NVSDK_NGX_OTA_UPDATES_Unregister proc
			jmp QWORD ptr OriginalFuncs_nvngx[44 * 8]
		NVSDK_NGX_OTA_UPDATES_Unregister endp
		NVSDK_NGX_OTA_UPDATES_Update proc
			jmp QWORD ptr OriginalFuncs_nvngx[45 * 8]
		NVSDK_NGX_OTA_UPDATES_Update endp
		NVSDK_NGX_UpdateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[46 * 8]
		NVSDK_NGX_UpdateFeature endp
		NVSDK_NGX_VULKAN_AllocateParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[47 * 8]
		NVSDK_NGX_VULKAN_AllocateParameters endp
		NVSDK_NGX_VULKAN_CreateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[48 * 8]
		NVSDK_NGX_VULKAN_CreateFeature endp
		NVSDK_NGX_VULKAN_CreateFeature1 proc
			jmp QWORD ptr OriginalFuncs_nvngx[49 * 8]
		NVSDK_NGX_VULKAN_CreateFeature1 endp
		NVSDK_NGX_VULKAN_DestroyParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[50 * 8]
		NVSDK_NGX_VULKAN_DestroyParameters endp
		NVSDK_NGX_VULKAN_EvaluateFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[51 * 8]
		NVSDK_NGX_VULKAN_EvaluateFeature endp
		NVSDK_NGX_VULKAN_GetCapabilityParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[52 * 8]
		NVSDK_NGX_VULKAN_GetCapabilityParameters endp
		NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements proc
			jmp QWORD ptr OriginalFuncs_nvngx[53 * 8]
		NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements endp
		NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements proc
			jmp QWORD ptr OriginalFuncs_nvngx[54 * 8]
		NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements endp
		NVSDK_NGX_VULKAN_GetFeatureRequirements proc
			jmp QWORD ptr OriginalFuncs_nvngx[55 * 8]
		NVSDK_NGX_VULKAN_GetFeatureRequirements endp
		NVSDK_NGX_VULKAN_GetParameters proc
			jmp QWORD ptr OriginalFuncs_nvngx[56 * 8]
		NVSDK_NGX_VULKAN_GetParameters endp
		NVSDK_NGX_VULKAN_GetScratchBufferSize proc
			jmp QWORD ptr OriginalFuncs_nvngx[57 * 8]
		NVSDK_NGX_VULKAN_GetScratchBufferSize endp
		NVSDK_NGX_VULKAN_Init proc
			jmp QWORD ptr OriginalFuncs_nvngx[58 * 8]
		NVSDK_NGX_VULKAN_Init endp
		NVSDK_NGX_VULKAN_Init_Ext proc
			jmp QWORD ptr OriginalFuncs_nvngx[59 * 8]
		NVSDK_NGX_VULKAN_Init_Ext endp
		NVSDK_NGX_VULKAN_Init_Ext2 proc
			jmp QWORD ptr OriginalFuncs_nvngx[60 * 8]
		NVSDK_NGX_VULKAN_Init_Ext2 endp
		NVSDK_NGX_VULKAN_Init_ProjectID proc
			jmp QWORD ptr OriginalFuncs_nvngx[61 * 8]
		NVSDK_NGX_VULKAN_Init_ProjectID endp
		NVSDK_NGX_VULKAN_Init_ProjectID_Ext proc
			jmp QWORD ptr OriginalFuncs_nvngx[62 * 8]
		NVSDK_NGX_VULKAN_Init_ProjectID_Ext endp
		NVSDK_NGX_VULKAN_ReleaseFeature proc
			jmp QWORD ptr OriginalFuncs_nvngx[63 * 8]
		NVSDK_NGX_VULKAN_ReleaseFeature endp
		NVSDK_NGX_VULKAN_RequiredExtensions proc
			jmp QWORD ptr OriginalFuncs_nvngx[64 * 8]
		NVSDK_NGX_VULKAN_RequiredExtensions endp
		NVSDK_NGX_VULKAN_Shutdown proc
			jmp QWORD ptr OriginalFuncs_nvngx[65 * 8]
		NVSDK_NGX_VULKAN_Shutdown endp
		NVSDK_NGX_VULKAN_Shutdown1 proc
			jmp QWORD ptr OriginalFuncs_nvngx[66 * 8]
		NVSDK_NGX_VULKAN_Shutdown1 endp
else
	.model flat, C
	.stack 4096
	.code
		extern OriginalFuncs_nvngx:DWORD
		NVSDK_NGX_CUDA_AllocateParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[0 * 4]
		NVSDK_NGX_CUDA_AllocateParameters endp
		NVSDK_NGX_CUDA_CreateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[1 * 4]
		NVSDK_NGX_CUDA_CreateFeature endp
		NVSDK_NGX_CUDA_DestroyParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[2 * 4]
		NVSDK_NGX_CUDA_DestroyParameters endp
		NVSDK_NGX_CUDA_EvaluateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[3 * 4]
		NVSDK_NGX_CUDA_EvaluateFeature endp
		NVSDK_NGX_CUDA_GetCapabilityParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[4 * 4]
		NVSDK_NGX_CUDA_GetCapabilityParameters endp
		NVSDK_NGX_CUDA_GetParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[5 * 4]
		NVSDK_NGX_CUDA_GetParameters endp
		NVSDK_NGX_CUDA_GetScratchBufferSize proc
			jmp DWORD ptr OriginalFuncs_nvngx[6 * 4]
		NVSDK_NGX_CUDA_GetScratchBufferSize endp
		NVSDK_NGX_CUDA_Init proc
			jmp DWORD ptr OriginalFuncs_nvngx[7 * 4]
		NVSDK_NGX_CUDA_Init endp
		NVSDK_NGX_CUDA_Init_Ext proc
			jmp DWORD ptr OriginalFuncs_nvngx[8 * 4]
		NVSDK_NGX_CUDA_Init_Ext endp
		NVSDK_NGX_CUDA_Init_ProjectID proc
			jmp DWORD ptr OriginalFuncs_nvngx[9 * 4]
		NVSDK_NGX_CUDA_Init_ProjectID endp
		NVSDK_NGX_CUDA_ReleaseFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[10 * 4]
		NVSDK_NGX_CUDA_ReleaseFeature endp
		NVSDK_NGX_CUDA_Shutdown proc
			jmp DWORD ptr OriginalFuncs_nvngx[11 * 4]
		NVSDK_NGX_CUDA_Shutdown endp
		NVSDK_NGX_D3D11_AllocateParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[12 * 4]
		NVSDK_NGX_D3D11_AllocateParameters endp
		NVSDK_NGX_D3D11_CreateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[13 * 4]
		NVSDK_NGX_D3D11_CreateFeature endp
		NVSDK_NGX_D3D11_DestroyParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[14 * 4]
		NVSDK_NGX_D3D11_DestroyParameters endp
		NVSDK_NGX_D3D11_EvaluateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[15 * 4]
		NVSDK_NGX_D3D11_EvaluateFeature endp
		NVSDK_NGX_D3D11_GetCapabilityParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[16 * 4]
		NVSDK_NGX_D3D11_GetCapabilityParameters endp
		NVSDK_NGX_D3D11_GetFeatureRequirements proc
			jmp DWORD ptr OriginalFuncs_nvngx[17 * 4]
		NVSDK_NGX_D3D11_GetFeatureRequirements endp
		NVSDK_NGX_D3D11_GetParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[18 * 4]
		NVSDK_NGX_D3D11_GetParameters endp
		NVSDK_NGX_D3D11_GetScratchBufferSize proc
			jmp DWORD ptr OriginalFuncs_nvngx[19 * 4]
		NVSDK_NGX_D3D11_GetScratchBufferSize endp
		NVSDK_NGX_D3D11_Init proc
			jmp DWORD ptr OriginalFuncs_nvngx[20 * 4]
		NVSDK_NGX_D3D11_Init endp
		NVSDK_NGX_D3D11_Init_Ext proc
			jmp DWORD ptr OriginalFuncs_nvngx[21 * 4]
		NVSDK_NGX_D3D11_Init_Ext endp
		NVSDK_NGX_D3D11_Init_ProjectID proc
			jmp DWORD ptr OriginalFuncs_nvngx[22 * 4]
		NVSDK_NGX_D3D11_Init_ProjectID endp
		NVSDK_NGX_D3D11_ReleaseFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[23 * 4]
		NVSDK_NGX_D3D11_ReleaseFeature endp
		NVSDK_NGX_D3D11_Shutdown proc
			jmp DWORD ptr OriginalFuncs_nvngx[24 * 4]
		NVSDK_NGX_D3D11_Shutdown endp
		NVSDK_NGX_D3D11_Shutdown1 proc
			jmp DWORD ptr OriginalFuncs_nvngx[25 * 4]
		NVSDK_NGX_D3D11_Shutdown1 endp
		NVSDK_NGX_D3D12_AllocateParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[26 * 4]
		NVSDK_NGX_D3D12_AllocateParameters endp
		NVSDK_NGX_D3D12_CreateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[27 * 4]
		NVSDK_NGX_D3D12_CreateFeature endp
		NVSDK_NGX_D3D12_DestroyParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[28 * 4]
		NVSDK_NGX_D3D12_DestroyParameters endp
		NVSDK_NGX_D3D12_EvaluateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[29 * 4]
		NVSDK_NGX_D3D12_EvaluateFeature endp
		NVSDK_NGX_D3D12_GetCapabilityParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[30 * 4]
		NVSDK_NGX_D3D12_GetCapabilityParameters endp
		NVSDK_NGX_D3D12_GetFeatureRequirements proc
			jmp DWORD ptr OriginalFuncs_nvngx[31 * 4]
		NVSDK_NGX_D3D12_GetFeatureRequirements endp
		NVSDK_NGX_D3D12_GetParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[32 * 4]
		NVSDK_NGX_D3D12_GetParameters endp
		NVSDK_NGX_D3D12_GetScratchBufferSize proc
			jmp DWORD ptr OriginalFuncs_nvngx[33 * 4]
		NVSDK_NGX_D3D12_GetScratchBufferSize endp
		NVSDK_NGX_D3D12_Init proc
			jmp DWORD ptr OriginalFuncs_nvngx[34 * 4]
		NVSDK_NGX_D3D12_Init endp
		NVSDK_NGX_D3D12_Init_Ext proc
			jmp DWORD ptr OriginalFuncs_nvngx[35 * 4]
		NVSDK_NGX_D3D12_Init_Ext endp
		NVSDK_NGX_D3D12_Init_ProjectID proc
			jmp DWORD ptr OriginalFuncs_nvngx[36 * 4]
		NVSDK_NGX_D3D12_Init_ProjectID endp
		NVSDK_NGX_D3D12_ReleaseFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[37 * 4]
		NVSDK_NGX_D3D12_ReleaseFeature endp
		NVSDK_NGX_D3D12_Shutdown proc
			jmp DWORD ptr OriginalFuncs_nvngx[38 * 4]
		NVSDK_NGX_D3D12_Shutdown endp
		NVSDK_NGX_D3D12_Shutdown1 proc
			jmp DWORD ptr OriginalFuncs_nvngx[39 * 4]
		NVSDK_NGX_D3D12_Shutdown1 endp
		NVSDK_NGX_OTA_UPDATES_CheckForUpdate proc
			jmp DWORD ptr OriginalFuncs_nvngx[40 * 4]
		NVSDK_NGX_OTA_UPDATES_CheckForUpdate endp
		NVSDK_NGX_OTA_UPDATES_GetPath proc
			jmp DWORD ptr OriginalFuncs_nvngx[41 * 4]
		NVSDK_NGX_OTA_UPDATES_GetPath endp
		NVSDK_NGX_OTA_UPDATES_Install proc
			jmp DWORD ptr OriginalFuncs_nvngx[42 * 4]
		NVSDK_NGX_OTA_UPDATES_Install endp
		NVSDK_NGX_OTA_UPDATES_Register proc
			jmp DWORD ptr OriginalFuncs_nvngx[43 * 4]
		NVSDK_NGX_OTA_UPDATES_Register endp
		NVSDK_NGX_OTA_UPDATES_Unregister proc
			jmp DWORD ptr OriginalFuncs_nvngx[44 * 4]
		NVSDK_NGX_OTA_UPDATES_Unregister endp
		NVSDK_NGX_OTA_UPDATES_Update proc
			jmp DWORD ptr OriginalFuncs_nvngx[45 * 4]
		NVSDK_NGX_OTA_UPDATES_Update endp
		NVSDK_NGX_UpdateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[46 * 4]
		NVSDK_NGX_UpdateFeature endp
		NVSDK_NGX_VULKAN_AllocateParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[47 * 4]
		NVSDK_NGX_VULKAN_AllocateParameters endp
		NVSDK_NGX_VULKAN_CreateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[48 * 4]
		NVSDK_NGX_VULKAN_CreateFeature endp
		NVSDK_NGX_VULKAN_CreateFeature1 proc
			jmp DWORD ptr OriginalFuncs_nvngx[49 * 4]
		NVSDK_NGX_VULKAN_CreateFeature1 endp
		NVSDK_NGX_VULKAN_DestroyParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[50 * 4]
		NVSDK_NGX_VULKAN_DestroyParameters endp
		NVSDK_NGX_VULKAN_EvaluateFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[51 * 4]
		NVSDK_NGX_VULKAN_EvaluateFeature endp
		NVSDK_NGX_VULKAN_GetCapabilityParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[52 * 4]
		NVSDK_NGX_VULKAN_GetCapabilityParameters endp
		NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements proc
			jmp DWORD ptr OriginalFuncs_nvngx[53 * 4]
		NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements endp
		NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements proc
			jmp DWORD ptr OriginalFuncs_nvngx[54 * 4]
		NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements endp
		NVSDK_NGX_VULKAN_GetFeatureRequirements proc
			jmp DWORD ptr OriginalFuncs_nvngx[55 * 4]
		NVSDK_NGX_VULKAN_GetFeatureRequirements endp
		NVSDK_NGX_VULKAN_GetParameters proc
			jmp DWORD ptr OriginalFuncs_nvngx[56 * 4]
		NVSDK_NGX_VULKAN_GetParameters endp
		NVSDK_NGX_VULKAN_GetScratchBufferSize proc
			jmp DWORD ptr OriginalFuncs_nvngx[57 * 4]
		NVSDK_NGX_VULKAN_GetScratchBufferSize endp
		NVSDK_NGX_VULKAN_Init proc
			jmp DWORD ptr OriginalFuncs_nvngx[58 * 4]
		NVSDK_NGX_VULKAN_Init endp
		NVSDK_NGX_VULKAN_Init_Ext proc
			jmp DWORD ptr OriginalFuncs_nvngx[59 * 4]
		NVSDK_NGX_VULKAN_Init_Ext endp
		NVSDK_NGX_VULKAN_Init_Ext2 proc
			jmp DWORD ptr OriginalFuncs_nvngx[60 * 4]
		NVSDK_NGX_VULKAN_Init_Ext2 endp
		NVSDK_NGX_VULKAN_Init_ProjectID proc
			jmp DWORD ptr OriginalFuncs_nvngx[61 * 4]
		NVSDK_NGX_VULKAN_Init_ProjectID endp
		NVSDK_NGX_VULKAN_Init_ProjectID_Ext proc
			jmp DWORD ptr OriginalFuncs_nvngx[62 * 4]
		NVSDK_NGX_VULKAN_Init_ProjectID_Ext endp
		NVSDK_NGX_VULKAN_ReleaseFeature proc
			jmp DWORD ptr OriginalFuncs_nvngx[63 * 4]
		NVSDK_NGX_VULKAN_ReleaseFeature endp
		NVSDK_NGX_VULKAN_RequiredExtensions proc
			jmp DWORD ptr OriginalFuncs_nvngx[64 * 4]
		NVSDK_NGX_VULKAN_RequiredExtensions endp
		NVSDK_NGX_VULKAN_Shutdown proc
			jmp DWORD ptr OriginalFuncs_nvngx[65 * 4]
		NVSDK_NGX_VULKAN_Shutdown endp
		NVSDK_NGX_VULKAN_Shutdown1 proc
			jmp DWORD ptr OriginalFuncs_nvngx[66 * 4]
		NVSDK_NGX_VULKAN_Shutdown1 endp
endif
end