ifdef RAX
	.code
		extern OriginalFunctions_psapi:QWORD
		EmptyWorkingSet proc
			jmp QWORD ptr OriginalFunctions_psapi[0 * 8]
		EmptyWorkingSet endp
		EnumDeviceDrivers proc
			jmp QWORD ptr OriginalFunctions_psapi[1 * 8]
		EnumDeviceDrivers endp
		EnumPageFilesA proc
			jmp QWORD ptr OriginalFunctions_psapi[2 * 8]
		EnumPageFilesA endp
		EnumPageFilesW proc
			jmp QWORD ptr OriginalFunctions_psapi[3 * 8]
		EnumPageFilesW endp
		EnumProcessModules proc
			jmp QWORD ptr OriginalFunctions_psapi[4 * 8]
		EnumProcessModules endp
		EnumProcessModulesEx proc
			jmp QWORD ptr OriginalFunctions_psapi[5 * 8]
		EnumProcessModulesEx endp
		EnumProcesses proc
			jmp QWORD ptr OriginalFunctions_psapi[6 * 8]
		EnumProcesses endp
		GetDeviceDriverBaseNameA proc
			jmp QWORD ptr OriginalFunctions_psapi[7 * 8]
		GetDeviceDriverBaseNameA endp
		GetDeviceDriverBaseNameW proc
			jmp QWORD ptr OriginalFunctions_psapi[8 * 8]
		GetDeviceDriverBaseNameW endp
		GetDeviceDriverFileNameA proc
			jmp QWORD ptr OriginalFunctions_psapi[9 * 8]
		GetDeviceDriverFileNameA endp
		GetDeviceDriverFileNameW proc
			jmp QWORD ptr OriginalFunctions_psapi[10 * 8]
		GetDeviceDriverFileNameW endp
		GetMappedFileNameA proc
			jmp QWORD ptr OriginalFunctions_psapi[11 * 8]
		GetMappedFileNameA endp
		GetMappedFileNameW proc
			jmp QWORD ptr OriginalFunctions_psapi[12 * 8]
		GetMappedFileNameW endp
		GetModuleBaseNameA proc
			jmp QWORD ptr OriginalFunctions_psapi[13 * 8]
		GetModuleBaseNameA endp
		GetModuleBaseNameW proc
			jmp QWORD ptr OriginalFunctions_psapi[14 * 8]
		GetModuleBaseNameW endp
		GetModuleFileNameExA proc
			jmp QWORD ptr OriginalFunctions_psapi[15 * 8]
		GetModuleFileNameExA endp
		GetModuleFileNameExW proc
			jmp QWORD ptr OriginalFunctions_psapi[16 * 8]
		GetModuleFileNameExW endp
		GetModuleInformation proc
			jmp QWORD ptr OriginalFunctions_psapi[17 * 8]
		GetModuleInformation endp
		GetPerformanceInfo proc
			jmp QWORD ptr OriginalFunctions_psapi[18 * 8]
		GetPerformanceInfo endp
		GetProcessImageFileNameA proc
			jmp QWORD ptr OriginalFunctions_psapi[19 * 8]
		GetProcessImageFileNameA endp
		GetProcessImageFileNameW proc
			jmp QWORD ptr OriginalFunctions_psapi[20 * 8]
		GetProcessImageFileNameW endp
		GetProcessMemoryInfo proc
			jmp QWORD ptr OriginalFunctions_psapi[21 * 8]
		GetProcessMemoryInfo endp
		GetWsChanges proc
			jmp QWORD ptr OriginalFunctions_psapi[22 * 8]
		GetWsChanges endp
		GetWsChangesEx proc
			jmp QWORD ptr OriginalFunctions_psapi[23 * 8]
		GetWsChangesEx endp
		InitializeProcessForWsWatch proc
			jmp QWORD ptr OriginalFunctions_psapi[24 * 8]
		InitializeProcessForWsWatch endp
		QueryWorkingSet proc
			jmp QWORD ptr OriginalFunctions_psapi[25 * 8]
		QueryWorkingSet endp
		QueryWorkingSetEx proc
			jmp QWORD ptr OriginalFunctions_psapi[26 * 8]
		QueryWorkingSetEx endp
endif
end