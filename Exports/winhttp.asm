ifdef RAX
	.code
		extern OriginalFunctions_winhttp:QWORD
		proxy_Private1 proc
			jmp QWORD ptr OriginalFunctions_winhttp[0 * 8]
		proxy_Private1 endp
		proxy_SvchostPushServiceGlobals proc
			jmp QWORD ptr OriginalFunctions_winhttp[1 * 8]
		proxy_SvchostPushServiceGlobals endp
		proxy_WinHttpAddRequestHeaders proc
			jmp QWORD ptr OriginalFunctions_winhttp[2 * 8]
		proxy_WinHttpAddRequestHeaders endp
		proxy_WinHttpAutoProxySvcMain proc
			jmp QWORD ptr OriginalFunctions_winhttp[3 * 8]
		proxy_WinHttpAutoProxySvcMain endp
		proxy_WinHttpCheckPlatform proc
			jmp QWORD ptr OriginalFunctions_winhttp[4 * 8]
		proxy_WinHttpCheckPlatform endp
		proxy_WinHttpCloseHandle proc
			jmp QWORD ptr OriginalFunctions_winhttp[5 * 8]
		proxy_WinHttpCloseHandle endp
		proxy_WinHttpConnect proc
			jmp QWORD ptr OriginalFunctions_winhttp[6 * 8]
		proxy_WinHttpConnect endp
		proxy_WinHttpConnectionDeletePolicyEntries proc
			jmp QWORD ptr OriginalFunctions_winhttp[7 * 8]
		proxy_WinHttpConnectionDeletePolicyEntries endp
		proxy_WinHttpConnectionDeleteProxyInfo proc
			jmp QWORD ptr OriginalFunctions_winhttp[8 * 8]
		proxy_WinHttpConnectionDeleteProxyInfo endp
		proxy_WinHttpConnectionFreeNameList proc
			jmp QWORD ptr OriginalFunctions_winhttp[9 * 8]
		proxy_WinHttpConnectionFreeNameList endp
		proxy_WinHttpConnectionFreeProxyInfo proc
			jmp QWORD ptr OriginalFunctions_winhttp[10 * 8]
		proxy_WinHttpConnectionFreeProxyInfo endp
		proxy_WinHttpConnectionFreeProxyList proc
			jmp QWORD ptr OriginalFunctions_winhttp[11 * 8]
		proxy_WinHttpConnectionFreeProxyList endp
		proxy_WinHttpConnectionGetNameList proc
			jmp QWORD ptr OriginalFunctions_winhttp[12 * 8]
		proxy_WinHttpConnectionGetNameList endp
		proxy_WinHttpConnectionGetProxyInfo proc
			jmp QWORD ptr OriginalFunctions_winhttp[13 * 8]
		proxy_WinHttpConnectionGetProxyInfo endp
		proxy_WinHttpConnectionGetProxyList proc
			jmp QWORD ptr OriginalFunctions_winhttp[14 * 8]
		proxy_WinHttpConnectionGetProxyList endp
		proxy_WinHttpConnectionSetPolicyEntries proc
			jmp QWORD ptr OriginalFunctions_winhttp[15 * 8]
		proxy_WinHttpConnectionSetPolicyEntries endp
		proxy_WinHttpConnectionSetProxyInfo proc
			jmp QWORD ptr OriginalFunctions_winhttp[16 * 8]
		proxy_WinHttpConnectionSetProxyInfo endp
		proxy_WinHttpConnectionUpdateIfIndexTable proc
			jmp QWORD ptr OriginalFunctions_winhttp[17 * 8]
		proxy_WinHttpConnectionUpdateIfIndexTable endp
		proxy_WinHttpCrackUrl proc
			jmp QWORD ptr OriginalFunctions_winhttp[18 * 8]
		proxy_WinHttpCrackUrl endp
		proxy_WinHttpCreateProxyResolver proc
			jmp QWORD ptr OriginalFunctions_winhttp[19 * 8]
		proxy_WinHttpCreateProxyResolver endp
		proxy_WinHttpCreateUrl proc
			jmp QWORD ptr OriginalFunctions_winhttp[20 * 8]
		proxy_WinHttpCreateUrl endp
		proxy_WinHttpDetectAutoProxyConfigUrl proc
			jmp QWORD ptr OriginalFunctions_winhttp[21 * 8]
		proxy_WinHttpDetectAutoProxyConfigUrl endp
		proxy_WinHttpFreeProxyResult proc
			jmp QWORD ptr OriginalFunctions_winhttp[22 * 8]
		proxy_WinHttpFreeProxyResult endp
		proxy_WinHttpFreeProxyResultEx proc
			jmp QWORD ptr OriginalFunctions_winhttp[23 * 8]
		proxy_WinHttpFreeProxyResultEx endp
		proxy_WinHttpFreeProxySettings proc
			jmp QWORD ptr OriginalFunctions_winhttp[24 * 8]
		proxy_WinHttpFreeProxySettings endp
		proxy_WinHttpGetDefaultProxyConfiguration proc
			jmp QWORD ptr OriginalFunctions_winhttp[25 * 8]
		proxy_WinHttpGetDefaultProxyConfiguration endp
		proxy_WinHttpGetIEProxyConfigForCurrentUser proc
			jmp QWORD ptr OriginalFunctions_winhttp[26 * 8]
		proxy_WinHttpGetIEProxyConfigForCurrentUser endp
		proxy_WinHttpGetProxyForUrl proc
			jmp QWORD ptr OriginalFunctions_winhttp[27 * 8]
		proxy_WinHttpGetProxyForUrl endp
		proxy_WinHttpGetProxyForUrlEx proc
			jmp QWORD ptr OriginalFunctions_winhttp[28 * 8]
		proxy_WinHttpGetProxyForUrlEx endp
		proxy_WinHttpGetProxyForUrlEx2 proc
			jmp QWORD ptr OriginalFunctions_winhttp[29 * 8]
		proxy_WinHttpGetProxyForUrlEx2 endp
		proxy_WinHttpGetProxyForUrlHvsi proc
			jmp QWORD ptr OriginalFunctions_winhttp[30 * 8]
		proxy_WinHttpGetProxyForUrlHvsi endp
		proxy_WinHttpGetProxyResult proc
			jmp QWORD ptr OriginalFunctions_winhttp[31 * 8]
		proxy_WinHttpGetProxyResult endp
		proxy_WinHttpGetProxyResultEx proc
			jmp QWORD ptr OriginalFunctions_winhttp[32 * 8]
		proxy_WinHttpGetProxyResultEx endp
		proxy_WinHttpGetProxySettingsVersion proc
			jmp QWORD ptr OriginalFunctions_winhttp[33 * 8]
		proxy_WinHttpGetProxySettingsVersion endp
		proxy_WinHttpGetTunnelSocket proc
			jmp QWORD ptr OriginalFunctions_winhttp[34 * 8]
		proxy_WinHttpGetTunnelSocket endp
		proxy_WinHttpOpen proc
			jmp QWORD ptr OriginalFunctions_winhttp[35 * 8]
		proxy_WinHttpOpen endp
		proxy_WinHttpOpenRequest proc
			jmp QWORD ptr OriginalFunctions_winhttp[36 * 8]
		proxy_WinHttpOpenRequest endp
		proxy_WinHttpPacJsWorkerMain proc
			jmp QWORD ptr OriginalFunctions_winhttp[37 * 8]
		proxy_WinHttpPacJsWorkerMain endp
		proxy_WinHttpProbeConnectivity proc
			jmp QWORD ptr OriginalFunctions_winhttp[38 * 8]
		proxy_WinHttpProbeConnectivity endp
		proxy_WinHttpQueryAuthSchemes proc
			jmp QWORD ptr OriginalFunctions_winhttp[39 * 8]
		proxy_WinHttpQueryAuthSchemes endp
		proxy_WinHttpQueryDataAvailable proc
			jmp QWORD ptr OriginalFunctions_winhttp[40 * 8]
		proxy_WinHttpQueryDataAvailable endp
		proxy_WinHttpQueryHeaders proc
			jmp QWORD ptr OriginalFunctions_winhttp[41 * 8]
		proxy_WinHttpQueryHeaders endp
		proxy_WinHttpQueryOption proc
			jmp QWORD ptr OriginalFunctions_winhttp[42 * 8]
		proxy_WinHttpQueryOption endp
		proxy_WinHttpReadData proc
			jmp QWORD ptr OriginalFunctions_winhttp[43 * 8]
		proxy_WinHttpReadData endp
		proxy_WinHttpReadProxySettings proc
			jmp QWORD ptr OriginalFunctions_winhttp[44 * 8]
		proxy_WinHttpReadProxySettings endp
		proxy_WinHttpReadProxySettingsHvsi proc
			jmp QWORD ptr OriginalFunctions_winhttp[45 * 8]
		proxy_WinHttpReadProxySettingsHvsi endp
		proxy_WinHttpReceiveResponse proc
			jmp QWORD ptr OriginalFunctions_winhttp[46 * 8]
		proxy_WinHttpReceiveResponse endp
		proxy_WinHttpResetAutoProxy proc
			jmp QWORD ptr OriginalFunctions_winhttp[47 * 8]
		proxy_WinHttpResetAutoProxy endp
		proxy_WinHttpSaveProxyCredentials proc
			jmp QWORD ptr OriginalFunctions_winhttp[48 * 8]
		proxy_WinHttpSaveProxyCredentials endp
		proxy_WinHttpSendRequest proc
			jmp QWORD ptr OriginalFunctions_winhttp[49 * 8]
		proxy_WinHttpSendRequest endp
		proxy_WinHttpSetCredentials proc
			jmp QWORD ptr OriginalFunctions_winhttp[50 * 8]
		proxy_WinHttpSetCredentials endp
		proxy_WinHttpSetDefaultProxyConfiguration proc
			jmp QWORD ptr OriginalFunctions_winhttp[51 * 8]
		proxy_WinHttpSetDefaultProxyConfiguration endp
		proxy_WinHttpSetOption proc
			jmp QWORD ptr OriginalFunctions_winhttp[52 * 8]
		proxy_WinHttpSetOption endp
		proxy_WinHttpSetStatusCallback proc
			jmp QWORD ptr OriginalFunctions_winhttp[53 * 8]
		proxy_WinHttpSetStatusCallback endp
		proxy_WinHttpSetTimeouts proc
			jmp QWORD ptr OriginalFunctions_winhttp[54 * 8]
		proxy_WinHttpSetTimeouts endp
		proxy_WinHttpTimeFromSystemTime proc
			jmp QWORD ptr OriginalFunctions_winhttp[55 * 8]
		proxy_WinHttpTimeFromSystemTime endp
		proxy_WinHttpTimeToSystemTime proc
			jmp QWORD ptr OriginalFunctions_winhttp[56 * 8]
		proxy_WinHttpTimeToSystemTime endp
		proxy_WinHttpWebSocketClose proc
			jmp QWORD ptr OriginalFunctions_winhttp[57 * 8]
		proxy_WinHttpWebSocketClose endp
		proxy_WinHttpWebSocketCompleteUpgrade proc
			jmp QWORD ptr OriginalFunctions_winhttp[58 * 8]
		proxy_WinHttpWebSocketCompleteUpgrade endp
		proxy_WinHttpWebSocketQueryCloseStatus proc
			jmp QWORD ptr OriginalFunctions_winhttp[59 * 8]
		proxy_WinHttpWebSocketQueryCloseStatus endp
		proxy_WinHttpWebSocketReceive proc
			jmp QWORD ptr OriginalFunctions_winhttp[60 * 8]
		proxy_WinHttpWebSocketReceive endp
		proxy_WinHttpWebSocketSend proc
			jmp QWORD ptr OriginalFunctions_winhttp[61 * 8]
		proxy_WinHttpWebSocketSend endp
		proxy_WinHttpWebSocketShutdown proc
			jmp QWORD ptr OriginalFunctions_winhttp[62 * 8]
		proxy_WinHttpWebSocketShutdown endp
		proxy_WinHttpWriteData proc
			jmp QWORD ptr OriginalFunctions_winhttp[63 * 8]
		proxy_WinHttpWriteData endp
		proxy_WinHttpWriteProxySettings proc
			jmp QWORD ptr OriginalFunctions_winhttp[64 * 8]
		proxy_WinHttpWriteProxySettings endp
endif
end