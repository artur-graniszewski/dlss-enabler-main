ifdef RAX
	.code
		extern originalFuncsVersion:QWORD
		GetFileVersionInfoA proc
			jmp QWORD ptr originalFuncsVersion[0 * 8]
		GetFileVersionInfoA endp
		GetFileVersionInfoByHandle proc
			jmp QWORD ptr originalFuncsVersion[1 * 8]
		GetFileVersionInfoByHandle endp
		GetFileVersionInfoExA proc
			jmp QWORD ptr originalFuncsVersion[2 * 8]
		GetFileVersionInfoExA endp
		GetFileVersionInfoExW proc
			jmp QWORD ptr originalFuncsVersion[3 * 8]
		GetFileVersionInfoExW endp
		GetFileVersionInfoSizeA proc
			jmp QWORD ptr originalFuncsVersion[4 * 8]
		GetFileVersionInfoSizeA endp
		GetFileVersionInfoSizeExA proc
			jmp QWORD ptr originalFuncsVersion[5 * 8]
		GetFileVersionInfoSizeExA endp
		GetFileVersionInfoSizeExW proc
			jmp QWORD ptr originalFuncsVersion[6 * 8]
		GetFileVersionInfoSizeExW endp
		GetFileVersionInfoSizeW proc
			jmp QWORD ptr originalFuncsVersion[7 * 8]
		GetFileVersionInfoSizeW endp
		GetFileVersionInfoW proc
			jmp QWORD ptr originalFuncsVersion[8 * 8]
		GetFileVersionInfoW endp
		VerFindFileA proc
			jmp QWORD ptr originalFuncsVersion[9 * 8]
		VerFindFileA endp
		VerFindFileW proc
			jmp QWORD ptr originalFuncsVersion[10 * 8]
		VerFindFileW endp
		VerInstallFileA proc
			jmp QWORD ptr originalFuncsVersion[11 * 8]
		VerInstallFileA endp
		VerInstallFileW proc
			jmp QWORD ptr originalFuncsVersion[12 * 8]
		VerInstallFileW endp
		VerLanguageNameA proc
			jmp QWORD ptr originalFuncsVersion[13 * 8]
		VerLanguageNameA endp
		VerLanguageNameW proc
			jmp QWORD ptr originalFuncsVersion[14 * 8]
		VerLanguageNameW endp
		VerQueryValueA proc
			jmp QWORD ptr originalFuncsVersion[15 * 8]
		VerQueryValueA endp
		VerQueryValueW proc
			jmp QWORD ptr originalFuncsVersion[16 * 8]
		VerQueryValueW endp
endif
end