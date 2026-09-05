ifdef RAX
	.code
		extern OriginalFunctions_winmm:QWORD
		CloseDriver proc
			jmp QWORD ptr OriginalFunctions_winmm[0 * 8]
		CloseDriver endp
		DefDriverProc proc
			jmp QWORD ptr OriginalFunctions_winmm[1 * 8]
		DefDriverProc endp
		DriverCallback proc
			jmp QWORD ptr OriginalFunctions_winmm[2 * 8]
		DriverCallback endp
		DrvGetModuleHandle proc
			jmp QWORD ptr OriginalFunctions_winmm[3 * 8]
		DrvGetModuleHandle endp
		GetDriverModuleHandle proc
			jmp QWORD ptr OriginalFunctions_winmm[4 * 8]
		GetDriverModuleHandle endp
		OpenDriver proc
			jmp QWORD ptr OriginalFunctions_winmm[5 * 8]
		OpenDriver endp
		PlaySound proc
			jmp QWORD ptr OriginalFunctions_winmm[6 * 8]
		PlaySound endp
		PlaySoundA proc
			jmp QWORD ptr OriginalFunctions_winmm[7 * 8]
		PlaySoundA endp
		PlaySoundW proc
			jmp QWORD ptr OriginalFunctions_winmm[8 * 8]
		PlaySoundW endp
		SendDriverMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[9 * 8]
		SendDriverMessage endp
		WOWAppExit proc
			jmp QWORD ptr OriginalFunctions_winmm[10 * 8]
		WOWAppExit endp
		auxGetDevCapsA proc
			jmp QWORD ptr OriginalFunctions_winmm[11 * 8]
		auxGetDevCapsA endp
		auxGetDevCapsW proc
			jmp QWORD ptr OriginalFunctions_winmm[12 * 8]
		auxGetDevCapsW endp
		auxGetNumDevs proc
			jmp QWORD ptr OriginalFunctions_winmm[13 * 8]
		auxGetNumDevs endp
		auxGetVolume proc
			jmp QWORD ptr OriginalFunctions_winmm[14 * 8]
		auxGetVolume endp
		auxOutMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[15 * 8]
		auxOutMessage endp
		auxSetVolume proc
			jmp QWORD ptr OriginalFunctions_winmm[16 * 8]
		auxSetVolume endp
		joyConfigChanged proc
			jmp QWORD ptr OriginalFunctions_winmm[17 * 8]
		joyConfigChanged endp
		joyGetDevCapsA proc
			jmp QWORD ptr OriginalFunctions_winmm[18 * 8]
		joyGetDevCapsA endp
		joyGetDevCapsW proc
			jmp QWORD ptr OriginalFunctions_winmm[19 * 8]
		joyGetDevCapsW endp
		joyGetNumDevs proc
			jmp QWORD ptr OriginalFunctions_winmm[20 * 8]
		joyGetNumDevs endp
		joyGetPos proc
			jmp QWORD ptr OriginalFunctions_winmm[21 * 8]
		joyGetPos endp
		joyGetPosEx proc
			jmp QWORD ptr OriginalFunctions_winmm[22 * 8]
		joyGetPosEx endp
		joyGetThreshold proc
			jmp QWORD ptr OriginalFunctions_winmm[23 * 8]
		joyGetThreshold endp
		joyReleaseCapture proc
			jmp QWORD ptr OriginalFunctions_winmm[24 * 8]
		joyReleaseCapture endp
		joySetCapture proc
			jmp QWORD ptr OriginalFunctions_winmm[25 * 8]
		joySetCapture endp
		joySetThreshold proc
			jmp QWORD ptr OriginalFunctions_winmm[26 * 8]
		joySetThreshold endp
		mciDriverNotify proc
			jmp QWORD ptr OriginalFunctions_winmm[27 * 8]
		mciDriverNotify endp
		mciDriverYield proc
			jmp QWORD ptr OriginalFunctions_winmm[28 * 8]
		mciDriverYield endp
		mciExecute proc
			jmp QWORD ptr OriginalFunctions_winmm[29 * 8]
		mciExecute endp
		mciFreeCommandResource proc
			jmp QWORD ptr OriginalFunctions_winmm[30 * 8]
		mciFreeCommandResource endp
		mciGetCreatorTask proc
			jmp QWORD ptr OriginalFunctions_winmm[31 * 8]
		mciGetCreatorTask endp
		mciGetDeviceIDA proc
			jmp QWORD ptr OriginalFunctions_winmm[32 * 8]
		mciGetDeviceIDA endp
		mciGetDeviceIDFromElementIDA proc
			jmp QWORD ptr OriginalFunctions_winmm[33 * 8]
		mciGetDeviceIDFromElementIDA endp
		mciGetDeviceIDFromElementIDW proc
			jmp QWORD ptr OriginalFunctions_winmm[34 * 8]
		mciGetDeviceIDFromElementIDW endp
		mciGetDeviceIDW proc
			jmp QWORD ptr OriginalFunctions_winmm[35 * 8]
		mciGetDeviceIDW endp
		mciGetDriverData proc
			jmp QWORD ptr OriginalFunctions_winmm[36 * 8]
		mciGetDriverData endp
		mciGetErrorStringA proc
			jmp QWORD ptr OriginalFunctions_winmm[37 * 8]
		mciGetErrorStringA endp
		mciGetErrorStringW proc
			jmp QWORD ptr OriginalFunctions_winmm[38 * 8]
		mciGetErrorStringW endp
		mciGetYieldProc proc
			jmp QWORD ptr OriginalFunctions_winmm[39 * 8]
		mciGetYieldProc endp
		mciLoadCommandResource proc
			jmp QWORD ptr OriginalFunctions_winmm[40 * 8]
		mciLoadCommandResource endp
		mciSendCommandA proc
			jmp QWORD ptr OriginalFunctions_winmm[41 * 8]
		mciSendCommandA endp
		mciSendCommandW proc
			jmp QWORD ptr OriginalFunctions_winmm[42 * 8]
		mciSendCommandW endp
		mciSendStringA proc
			jmp QWORD ptr OriginalFunctions_winmm[43 * 8]
		mciSendStringA endp
		mciSendStringW proc
			jmp QWORD ptr OriginalFunctions_winmm[44 * 8]
		mciSendStringW endp
		mciSetDriverData proc
			jmp QWORD ptr OriginalFunctions_winmm[45 * 8]
		mciSetDriverData endp
		mciSetYieldProc proc
			jmp QWORD ptr OriginalFunctions_winmm[46 * 8]
		mciSetYieldProc endp
		midiConnect proc
			jmp QWORD ptr OriginalFunctions_winmm[47 * 8]
		midiConnect endp
		midiDisconnect proc
			jmp QWORD ptr OriginalFunctions_winmm[48 * 8]
		midiDisconnect endp
		midiInAddBuffer proc
			jmp QWORD ptr OriginalFunctions_winmm[49 * 8]
		midiInAddBuffer endp
		midiInClose proc
			jmp QWORD ptr OriginalFunctions_winmm[50 * 8]
		midiInClose endp
		midiInGetDevCapsA proc
			jmp QWORD ptr OriginalFunctions_winmm[51 * 8]
		midiInGetDevCapsA endp
		midiInGetDevCapsW proc
			jmp QWORD ptr OriginalFunctions_winmm[52 * 8]
		midiInGetDevCapsW endp
		midiInGetErrorTextA proc
			jmp QWORD ptr OriginalFunctions_winmm[53 * 8]
		midiInGetErrorTextA endp
		midiInGetErrorTextW proc
			jmp QWORD ptr OriginalFunctions_winmm[54 * 8]
		midiInGetErrorTextW endp
		midiInGetID proc
			jmp QWORD ptr OriginalFunctions_winmm[55 * 8]
		midiInGetID endp
		midiInGetNumDevs proc
			jmp QWORD ptr OriginalFunctions_winmm[56 * 8]
		midiInGetNumDevs endp
		midiInMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[57 * 8]
		midiInMessage endp
		midiInOpen proc
			jmp QWORD ptr OriginalFunctions_winmm[58 * 8]
		midiInOpen endp
		midiInPrepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[59 * 8]
		midiInPrepareHeader endp
		midiInReset proc
			jmp QWORD ptr OriginalFunctions_winmm[60 * 8]
		midiInReset endp
		midiInStart proc
			jmp QWORD ptr OriginalFunctions_winmm[61 * 8]
		midiInStart endp
		midiInStop proc
			jmp QWORD ptr OriginalFunctions_winmm[62 * 8]
		midiInStop endp
		midiInUnprepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[63 * 8]
		midiInUnprepareHeader endp
		midiOutCacheDrumPatches proc
			jmp QWORD ptr OriginalFunctions_winmm[64 * 8]
		midiOutCacheDrumPatches endp
		midiOutCachePatches proc
			jmp QWORD ptr OriginalFunctions_winmm[65 * 8]
		midiOutCachePatches endp
		midiOutClose proc
			jmp QWORD ptr OriginalFunctions_winmm[66 * 8]
		midiOutClose endp
		midiOutGetDevCapsA proc
			jmp QWORD ptr OriginalFunctions_winmm[67 * 8]
		midiOutGetDevCapsA endp
		midiOutGetDevCapsW proc
			jmp QWORD ptr OriginalFunctions_winmm[68 * 8]
		midiOutGetDevCapsW endp
		midiOutGetErrorTextA proc
			jmp QWORD ptr OriginalFunctions_winmm[69 * 8]
		midiOutGetErrorTextA endp
		midiOutGetErrorTextW proc
			jmp QWORD ptr OriginalFunctions_winmm[70 * 8]
		midiOutGetErrorTextW endp
		midiOutGetID proc
			jmp QWORD ptr OriginalFunctions_winmm[71 * 8]
		midiOutGetID endp
		midiOutGetNumDevs proc
			jmp QWORD ptr OriginalFunctions_winmm[72 * 8]
		midiOutGetNumDevs endp
		midiOutGetVolume proc
			jmp QWORD ptr OriginalFunctions_winmm[73 * 8]
		midiOutGetVolume endp
		midiOutLongMsg proc
			jmp QWORD ptr OriginalFunctions_winmm[74 * 8]
		midiOutLongMsg endp
		midiOutMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[75 * 8]
		midiOutMessage endp
		midiOutOpen proc
			jmp QWORD ptr OriginalFunctions_winmm[76 * 8]
		midiOutOpen endp
		midiOutPrepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[77 * 8]
		midiOutPrepareHeader endp
		midiOutReset proc
			jmp QWORD ptr OriginalFunctions_winmm[78 * 8]
		midiOutReset endp
		midiOutSetVolume proc
			jmp QWORD ptr OriginalFunctions_winmm[79 * 8]
		midiOutSetVolume endp
		midiOutShortMsg proc
			jmp QWORD ptr OriginalFunctions_winmm[80 * 8]
		midiOutShortMsg endp
		midiOutUnprepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[81 * 8]
		midiOutUnprepareHeader endp
		midiStreamClose proc
			jmp QWORD ptr OriginalFunctions_winmm[82 * 8]
		midiStreamClose endp
		midiStreamOpen proc
			jmp QWORD ptr OriginalFunctions_winmm[83 * 8]
		midiStreamOpen endp
		midiStreamOut proc
			jmp QWORD ptr OriginalFunctions_winmm[84 * 8]
		midiStreamOut endp
		midiStreamPause proc
			jmp QWORD ptr OriginalFunctions_winmm[85 * 8]
		midiStreamPause endp
		midiStreamPosition proc
			jmp QWORD ptr OriginalFunctions_winmm[86 * 8]
		midiStreamPosition endp
		midiStreamProperty proc
			jmp QWORD ptr OriginalFunctions_winmm[87 * 8]
		midiStreamProperty endp
		midiStreamRestart proc
			jmp QWORD ptr OriginalFunctions_winmm[88 * 8]
		midiStreamRestart endp
		midiStreamStop proc
			jmp QWORD ptr OriginalFunctions_winmm[89 * 8]
		midiStreamStop endp
		mixerClose proc
			jmp QWORD ptr OriginalFunctions_winmm[90 * 8]
		mixerClose endp
		mixerGetControlDetailsA proc
			jmp QWORD ptr OriginalFunctions_winmm[91 * 8]
		mixerGetControlDetailsA endp
		mixerGetControlDetailsW proc
			jmp QWORD ptr OriginalFunctions_winmm[92 * 8]
		mixerGetControlDetailsW endp
		mixerGetDevCapsA proc
			jmp QWORD ptr OriginalFunctions_winmm[93 * 8]
		mixerGetDevCapsA endp
		mixerGetDevCapsW proc
			jmp QWORD ptr OriginalFunctions_winmm[94 * 8]
		mixerGetDevCapsW endp
		mixerGetID proc
			jmp QWORD ptr OriginalFunctions_winmm[95 * 8]
		mixerGetID endp
		mixerGetLineControlsA proc
			jmp QWORD ptr OriginalFunctions_winmm[96 * 8]
		mixerGetLineControlsA endp
		mixerGetLineControlsW proc
			jmp QWORD ptr OriginalFunctions_winmm[97 * 8]
		mixerGetLineControlsW endp
		mixerGetLineInfoA proc
			jmp QWORD ptr OriginalFunctions_winmm[98 * 8]
		mixerGetLineInfoA endp
		mixerGetLineInfoW proc
			jmp QWORD ptr OriginalFunctions_winmm[99 * 8]
		mixerGetLineInfoW endp
		mixerGetNumDevs proc
			jmp QWORD ptr OriginalFunctions_winmm[100 * 8]
		mixerGetNumDevs endp
		mixerMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[101 * 8]
		mixerMessage endp
		mixerOpen proc
			jmp QWORD ptr OriginalFunctions_winmm[102 * 8]
		mixerOpen endp
		mixerSetControlDetails proc
			jmp QWORD ptr OriginalFunctions_winmm[103 * 8]
		mixerSetControlDetails endp
		mmDrvInstall proc
			jmp QWORD ptr OriginalFunctions_winmm[104 * 8]
		mmDrvInstall endp
		mmGetCurrentTask proc
			jmp QWORD ptr OriginalFunctions_winmm[105 * 8]
		mmGetCurrentTask endp
		mmTaskBlock proc
			jmp QWORD ptr OriginalFunctions_winmm[106 * 8]
		mmTaskBlock endp
		mmTaskCreate proc
			jmp QWORD ptr OriginalFunctions_winmm[107 * 8]
		mmTaskCreate endp
		mmTaskSignal proc
			jmp QWORD ptr OriginalFunctions_winmm[108 * 8]
		mmTaskSignal endp
		mmTaskYield proc
			jmp QWORD ptr OriginalFunctions_winmm[109 * 8]
		mmTaskYield endp
		mmioAdvance proc
			jmp QWORD ptr OriginalFunctions_winmm[110 * 8]
		mmioAdvance endp
		mmioAscend proc
			jmp QWORD ptr OriginalFunctions_winmm[111 * 8]
		mmioAscend endp
		mmioClose proc
			jmp QWORD ptr OriginalFunctions_winmm[112 * 8]
		mmioClose endp
		mmioCreateChunk proc
			jmp QWORD ptr OriginalFunctions_winmm[113 * 8]
		mmioCreateChunk endp
		mmioDescend proc
			jmp QWORD ptr OriginalFunctions_winmm[114 * 8]
		mmioDescend endp
		mmioFlush proc
			jmp QWORD ptr OriginalFunctions_winmm[115 * 8]
		mmioFlush endp
		mmioGetInfo proc
			jmp QWORD ptr OriginalFunctions_winmm[116 * 8]
		mmioGetInfo endp
		mmioInstallIOProcA proc
			jmp QWORD ptr OriginalFunctions_winmm[117 * 8]
		mmioInstallIOProcA endp
		mmioInstallIOProcW proc
			jmp QWORD ptr OriginalFunctions_winmm[118 * 8]
		mmioInstallIOProcW endp
		mmioOpenA proc
			jmp QWORD ptr OriginalFunctions_winmm[119 * 8]
		mmioOpenA endp
		mmioOpenW proc
			jmp QWORD ptr OriginalFunctions_winmm[120 * 8]
		mmioOpenW endp
		mmioRead proc
			jmp QWORD ptr OriginalFunctions_winmm[121 * 8]
		mmioRead endp
		mmioRenameA proc
			jmp QWORD ptr OriginalFunctions_winmm[122 * 8]
		mmioRenameA endp
		mmioRenameW proc
			jmp QWORD ptr OriginalFunctions_winmm[123 * 8]
		mmioRenameW endp
		mmioSeek proc
			jmp QWORD ptr OriginalFunctions_winmm[124 * 8]
		mmioSeek endp
		mmioSendMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[125 * 8]
		mmioSendMessage endp
		mmioSetBuffer proc
			jmp QWORD ptr OriginalFunctions_winmm[126 * 8]
		mmioSetBuffer endp
		mmioSetInfo proc
			jmp QWORD ptr OriginalFunctions_winmm[127 * 8]
		mmioSetInfo endp
		mmioStringToFOURCCA proc
			jmp QWORD ptr OriginalFunctions_winmm[128 * 8]
		mmioStringToFOURCCA endp
		mmioStringToFOURCCW proc
			jmp QWORD ptr OriginalFunctions_winmm[129 * 8]
		mmioStringToFOURCCW endp
		mmioWrite proc
			jmp QWORD ptr OriginalFunctions_winmm[130 * 8]
		mmioWrite endp
		mmsystemGetVersion proc
			jmp QWORD ptr OriginalFunctions_winmm[131 * 8]
		mmsystemGetVersion endp
		sndPlaySoundA proc
			jmp QWORD ptr OriginalFunctions_winmm[132 * 8]
		sndPlaySoundA endp
		sndPlaySoundW proc
			jmp QWORD ptr OriginalFunctions_winmm[133 * 8]
		sndPlaySoundW endp
		dlss_enabler_timeBeginPeriod proc
			jmp QWORD ptr OriginalFunctions_winmm[134 * 8]
		dlss_enabler_timeBeginPeriod endp
		dlss_enabler_timeEndPeriod proc
			jmp QWORD ptr OriginalFunctions_winmm[135 * 8]
		dlss_enabler_timeEndPeriod endp
		dlss_enabler_timeGetDevCaps proc
			jmp QWORD ptr OriginalFunctions_winmm[136 * 8]
		dlss_enabler_timeGetDevCaps endp
		timeGetSystemTime proc
			jmp QWORD ptr OriginalFunctions_winmm[137 * 8]
		timeGetSystemTime endp
		timeGetTime proc
			jmp QWORD ptr OriginalFunctions_winmm[138 * 8]
		timeGetTime endp
		timeKillEvent proc
			jmp QWORD ptr OriginalFunctions_winmm[139 * 8]
		timeKillEvent endp
		timeSetEvent proc
			jmp QWORD ptr OriginalFunctions_winmm[140 * 8]
		timeSetEvent endp
		waveInAddBuffer proc
			jmp QWORD ptr OriginalFunctions_winmm[141 * 8]
		waveInAddBuffer endp
		waveInClose proc
			jmp QWORD ptr OriginalFunctions_winmm[142 * 8]
		waveInClose endp
		waveInGetDevCapsA proc
			jmp QWORD ptr OriginalFunctions_winmm[143 * 8]
		waveInGetDevCapsA endp
		waveInGetDevCapsW proc
			jmp QWORD ptr OriginalFunctions_winmm[144 * 8]
		waveInGetDevCapsW endp
		waveInGetErrorTextA proc
			jmp QWORD ptr OriginalFunctions_winmm[145 * 8]
		waveInGetErrorTextA endp
		waveInGetErrorTextW proc
			jmp QWORD ptr OriginalFunctions_winmm[146 * 8]
		waveInGetErrorTextW endp
		waveInGetID proc
			jmp QWORD ptr OriginalFunctions_winmm[147 * 8]
		waveInGetID endp
		waveInGetNumDevs proc
			jmp QWORD ptr OriginalFunctions_winmm[148 * 8]
		waveInGetNumDevs endp
		waveInGetPosition proc
			jmp QWORD ptr OriginalFunctions_winmm[149 * 8]
		waveInGetPosition endp
		waveInMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[150 * 8]
		waveInMessage endp
		waveInOpen proc
			jmp QWORD ptr OriginalFunctions_winmm[151 * 8]
		waveInOpen endp
		waveInPrepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[152 * 8]
		waveInPrepareHeader endp
		waveInReset proc
			jmp QWORD ptr OriginalFunctions_winmm[153 * 8]
		waveInReset endp
		waveInStart proc
			jmp QWORD ptr OriginalFunctions_winmm[154 * 8]
		waveInStart endp
		waveInStop proc
			jmp QWORD ptr OriginalFunctions_winmm[155 * 8]
		waveInStop endp
		waveInUnprepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[156 * 8]
		waveInUnprepareHeader endp
		waveOutBreakLoop proc
			jmp QWORD ptr OriginalFunctions_winmm[157 * 8]
		waveOutBreakLoop endp
		waveOutClose proc
			jmp QWORD ptr OriginalFunctions_winmm[158 * 8]
		waveOutClose endp
		waveOutGetDevCapsA proc
			jmp QWORD ptr OriginalFunctions_winmm[159 * 8]
		waveOutGetDevCapsA endp
		waveOutGetDevCapsW proc
			jmp QWORD ptr OriginalFunctions_winmm[160 * 8]
		waveOutGetDevCapsW endp
		waveOutGetErrorTextA proc
			jmp QWORD ptr OriginalFunctions_winmm[161 * 8]
		waveOutGetErrorTextA endp
		waveOutGetErrorTextW proc
			jmp QWORD ptr OriginalFunctions_winmm[162 * 8]
		waveOutGetErrorTextW endp
		waveOutGetID proc
			jmp QWORD ptr OriginalFunctions_winmm[163 * 8]
		waveOutGetID endp
		waveOutGetNumDevs proc
			jmp QWORD ptr OriginalFunctions_winmm[164 * 8]
		waveOutGetNumDevs endp
		waveOutGetPitch proc
			jmp QWORD ptr OriginalFunctions_winmm[165 * 8]
		waveOutGetPitch endp
		waveOutGetPlaybackRate proc
			jmp QWORD ptr OriginalFunctions_winmm[166 * 8]
		waveOutGetPlaybackRate endp
		waveOutGetPosition proc
			jmp QWORD ptr OriginalFunctions_winmm[167 * 8]
		waveOutGetPosition endp
		waveOutGetVolume proc
			jmp QWORD ptr OriginalFunctions_winmm[168 * 8]
		waveOutGetVolume endp
		waveOutMessage proc
			jmp QWORD ptr OriginalFunctions_winmm[169 * 8]
		waveOutMessage endp
		waveOutOpen proc
			jmp QWORD ptr OriginalFunctions_winmm[170 * 8]
		waveOutOpen endp
		waveOutPause proc
			jmp QWORD ptr OriginalFunctions_winmm[171 * 8]
		waveOutPause endp
		waveOutPrepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[172 * 8]
		waveOutPrepareHeader endp
		waveOutReset proc
			jmp QWORD ptr OriginalFunctions_winmm[173 * 8]
		waveOutReset endp
		waveOutRestart proc
			jmp QWORD ptr OriginalFunctions_winmm[174 * 8]
		waveOutRestart endp
		waveOutSetPitch proc
			jmp QWORD ptr OriginalFunctions_winmm[175 * 8]
		waveOutSetPitch endp
		waveOutSetPlaybackRate proc
			jmp QWORD ptr OriginalFunctions_winmm[176 * 8]
		waveOutSetPlaybackRate endp
		waveOutSetVolume proc
			jmp QWORD ptr OriginalFunctions_winmm[177 * 8]
		waveOutSetVolume endp
		waveOutUnprepareHeader proc
			jmp QWORD ptr OriginalFunctions_winmm[178 * 8]
		waveOutUnprepareHeader endp
		waveOutWrite proc
			jmp QWORD ptr OriginalFunctions_winmm[179 * 8]
		waveOutWrite endp
		ExportByOrdinal2 proc
			jmp QWORD ptr OriginalFunctions_winmm[180 * 8]
		ExportByOrdinal2 endp
endif
end