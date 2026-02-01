ifdef RAX
	.code
		extern originalFuncsPsapi:QWORD
		EmptyWorkingSet proc
			jmp QWORD ptr originalFuncsPsapi[0 * 8]
		EmptyWorkingSet endp
		EnumDeviceDrivers proc
			jmp QWORD ptr originalFuncsPsapi[1 * 8]
		EnumDeviceDrivers endp
		EnumPageFilesA proc
			jmp QWORD ptr originalFuncsPsapi[2 * 8]
		EnumPageFilesA endp
		EnumPageFilesW proc
			jmp QWORD ptr originalFuncsPsapi[3 * 8]
		EnumPageFilesW endp
		EnumProcessModules proc
			jmp QWORD ptr originalFuncsPsapi[4 * 8]
		EnumProcessModules endp
		EnumProcessModulesEx proc
			jmp QWORD ptr originalFuncsPsapi[5 * 8]
		EnumProcessModulesEx endp
		EnumProcesses proc
			jmp QWORD ptr originalFuncsPsapi[6 * 8]
		EnumProcesses endp
		GetDeviceDriverBaseNameA proc
			jmp QWORD ptr originalFuncsPsapi[7 * 8]
		GetDeviceDriverBaseNameA endp
		GetDeviceDriverBaseNameW proc
			jmp QWORD ptr originalFuncsPsapi[8 * 8]
		GetDeviceDriverBaseNameW endp
		GetDeviceDriverFileNameA proc
			jmp QWORD ptr originalFuncsPsapi[9 * 8]
		GetDeviceDriverFileNameA endp
		GetDeviceDriverFileNameW proc
			jmp QWORD ptr originalFuncsPsapi[10 * 8]
		GetDeviceDriverFileNameW endp
		GetMappedFileNameA proc
			jmp QWORD ptr originalFuncsPsapi[11 * 8]
		GetMappedFileNameA endp
		GetMappedFileNameW proc
			jmp QWORD ptr originalFuncsPsapi[12 * 8]
		GetMappedFileNameW endp
		GetModuleBaseNameA proc
			jmp QWORD ptr originalFuncsPsapi[13 * 8]
		GetModuleBaseNameA endp
		GetModuleBaseNameW proc
			jmp QWORD ptr originalFuncsPsapi[14 * 8]
		GetModuleBaseNameW endp
		GetModuleFileNameExA proc
			jmp QWORD ptr originalFuncsPsapi[15 * 8]
		GetModuleFileNameExA endp
		GetModuleFileNameExW proc
			jmp QWORD ptr originalFuncsPsapi[16 * 8]
		GetModuleFileNameExW endp
		GetModuleInformation proc
			jmp QWORD ptr originalFuncsPsapi[17 * 8]
		GetModuleInformation endp
		GetPerformanceInfo proc
			jmp QWORD ptr originalFuncsPsapi[18 * 8]
		GetPerformanceInfo endp
		GetProcessImageFileNameA proc
			jmp QWORD ptr originalFuncsPsapi[19 * 8]
		GetProcessImageFileNameA endp
		GetProcessImageFileNameW proc
			jmp QWORD ptr originalFuncsPsapi[20 * 8]
		GetProcessImageFileNameW endp
		GetProcessMemoryInfo proc
			jmp QWORD ptr originalFuncsPsapi[21 * 8]
		GetProcessMemoryInfo endp
		GetWsChanges proc
			jmp QWORD ptr originalFuncsPsapi[22 * 8]
		GetWsChanges endp
		GetWsChangesEx proc
			jmp QWORD ptr originalFuncsPsapi[23 * 8]
		GetWsChangesEx endp
		InitializeProcessForWsWatch proc
			jmp QWORD ptr originalFuncsPsapi[24 * 8]
		InitializeProcessForWsWatch endp
		QueryWorkingSet proc
			jmp QWORD ptr originalFuncsPsapi[25 * 8]
		QueryWorkingSet endp
		QueryWorkingSetEx proc
			jmp QWORD ptr originalFuncsPsapi[26 * 8]
		QueryWorkingSetEx endp
endif
end