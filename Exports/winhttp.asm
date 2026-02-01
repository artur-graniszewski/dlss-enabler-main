ifdef RAX
	.code
		extern originalFuncsWinhttp:QWORD
		proxy_Private1 proc
			jmp QWORD ptr originalFuncsWinhttp[0 * 8]
		proxy_Private1 endp
		proxy_SvchostPushServiceGlobals proc
			jmp QWORD ptr originalFuncsWinhttp[1 * 8]
		proxy_SvchostPushServiceGlobals endp
		proxy_WinHttpAddRequestHeaders proc
			jmp QWORD ptr originalFuncsWinhttp[2 * 8]
		proxy_WinHttpAddRequestHeaders endp
		proxy_WinHttpAutoProxySvcMain proc
			jmp QWORD ptr originalFuncsWinhttp[3 * 8]
		proxy_WinHttpAutoProxySvcMain endp
		proxy_WinHttpCheckPlatform proc
			jmp QWORD ptr originalFuncsWinhttp[4 * 8]
		proxy_WinHttpCheckPlatform endp
		proxy_WinHttpCloseHandle proc
			jmp QWORD ptr originalFuncsWinhttp[5 * 8]
		proxy_WinHttpCloseHandle endp
		proxy_WinHttpConnect proc
			jmp QWORD ptr originalFuncsWinhttp[6 * 8]
		proxy_WinHttpConnect endp
		proxy_WinHttpConnectionDeletePolicyEntries proc
			jmp QWORD ptr originalFuncsWinhttp[7 * 8]
		proxy_WinHttpConnectionDeletePolicyEntries endp
		proxy_WinHttpConnectionDeleteProxyInfo proc
			jmp QWORD ptr originalFuncsWinhttp[8 * 8]
		proxy_WinHttpConnectionDeleteProxyInfo endp
		proxy_WinHttpConnectionFreeNameList proc
			jmp QWORD ptr originalFuncsWinhttp[9 * 8]
		proxy_WinHttpConnectionFreeNameList endp
		proxy_WinHttpConnectionFreeProxyInfo proc
			jmp QWORD ptr originalFuncsWinhttp[10 * 8]
		proxy_WinHttpConnectionFreeProxyInfo endp
		proxy_WinHttpConnectionFreeProxyList proc
			jmp QWORD ptr originalFuncsWinhttp[11 * 8]
		proxy_WinHttpConnectionFreeProxyList endp
		proxy_WinHttpConnectionGetNameList proc
			jmp QWORD ptr originalFuncsWinhttp[12 * 8]
		proxy_WinHttpConnectionGetNameList endp
		proxy_WinHttpConnectionGetProxyInfo proc
			jmp QWORD ptr originalFuncsWinhttp[13 * 8]
		proxy_WinHttpConnectionGetProxyInfo endp
		proxy_WinHttpConnectionGetProxyList proc
			jmp QWORD ptr originalFuncsWinhttp[14 * 8]
		proxy_WinHttpConnectionGetProxyList endp
		proxy_WinHttpConnectionSetPolicyEntries proc
			jmp QWORD ptr originalFuncsWinhttp[15 * 8]
		proxy_WinHttpConnectionSetPolicyEntries endp
		proxy_WinHttpConnectionSetProxyInfo proc
			jmp QWORD ptr originalFuncsWinhttp[16 * 8]
		proxy_WinHttpConnectionSetProxyInfo endp
		proxy_WinHttpConnectionUpdateIfIndexTable proc
			jmp QWORD ptr originalFuncsWinhttp[17 * 8]
		proxy_WinHttpConnectionUpdateIfIndexTable endp
		proxy_WinHttpCrackUrl proc
			jmp QWORD ptr originalFuncsWinhttp[18 * 8]
		proxy_WinHttpCrackUrl endp
		proxy_WinHttpCreateProxyResolver proc
			jmp QWORD ptr originalFuncsWinhttp[19 * 8]
		proxy_WinHttpCreateProxyResolver endp
		proxy_WinHttpCreateUrl proc
			jmp QWORD ptr originalFuncsWinhttp[20 * 8]
		proxy_WinHttpCreateUrl endp
		proxy_WinHttpDetectAutoProxyConfigUrl proc
			jmp QWORD ptr originalFuncsWinhttp[21 * 8]
		proxy_WinHttpDetectAutoProxyConfigUrl endp
		proxy_WinHttpFreeProxyResult proc
			jmp QWORD ptr originalFuncsWinhttp[22 * 8]
		proxy_WinHttpFreeProxyResult endp
		proxy_WinHttpFreeProxyResultEx proc
			jmp QWORD ptr originalFuncsWinhttp[23 * 8]
		proxy_WinHttpFreeProxyResultEx endp
		proxy_WinHttpFreeProxySettings proc
			jmp QWORD ptr originalFuncsWinhttp[24 * 8]
		proxy_WinHttpFreeProxySettings endp
		proxy_WinHttpGetDefaultProxyConfiguration proc
			jmp QWORD ptr originalFuncsWinhttp[25 * 8]
		proxy_WinHttpGetDefaultProxyConfiguration endp
		proxy_WinHttpGetIEProxyConfigForCurrentUser proc
			jmp QWORD ptr originalFuncsWinhttp[26 * 8]
		proxy_WinHttpGetIEProxyConfigForCurrentUser endp
		proxy_WinHttpGetProxyForUrl proc
			jmp QWORD ptr originalFuncsWinhttp[27 * 8]
		proxy_WinHttpGetProxyForUrl endp
		proxy_WinHttpGetProxyForUrlEx proc
			jmp QWORD ptr originalFuncsWinhttp[28 * 8]
		proxy_WinHttpGetProxyForUrlEx endp
		proxy_WinHttpGetProxyForUrlEx2 proc
			jmp QWORD ptr originalFuncsWinhttp[29 * 8]
		proxy_WinHttpGetProxyForUrlEx2 endp
		proxy_WinHttpGetProxyForUrlHvsi proc
			jmp QWORD ptr originalFuncsWinhttp[30 * 8]
		proxy_WinHttpGetProxyForUrlHvsi endp
		proxy_WinHttpGetProxyResult proc
			jmp QWORD ptr originalFuncsWinhttp[31 * 8]
		proxy_WinHttpGetProxyResult endp
		proxy_WinHttpGetProxyResultEx proc
			jmp QWORD ptr originalFuncsWinhttp[32 * 8]
		proxy_WinHttpGetProxyResultEx endp
		proxy_WinHttpGetProxySettingsVersion proc
			jmp QWORD ptr originalFuncsWinhttp[33 * 8]
		proxy_WinHttpGetProxySettingsVersion endp
		proxy_WinHttpGetTunnelSocket proc
			jmp QWORD ptr originalFuncsWinhttp[34 * 8]
		proxy_WinHttpGetTunnelSocket endp
		proxy_WinHttpOpen proc
			jmp QWORD ptr originalFuncsWinhttp[35 * 8]
		proxy_WinHttpOpen endp
		proxy_WinHttpOpenRequest proc
			jmp QWORD ptr originalFuncsWinhttp[36 * 8]
		proxy_WinHttpOpenRequest endp
		proxy_WinHttpPacJsWorkerMain proc
			jmp QWORD ptr originalFuncsWinhttp[37 * 8]
		proxy_WinHttpPacJsWorkerMain endp
		proxy_WinHttpProbeConnectivity proc
			jmp QWORD ptr originalFuncsWinhttp[38 * 8]
		proxy_WinHttpProbeConnectivity endp
		proxy_WinHttpQueryAuthSchemes proc
			jmp QWORD ptr originalFuncsWinhttp[39 * 8]
		proxy_WinHttpQueryAuthSchemes endp
		proxy_WinHttpQueryDataAvailable proc
			jmp QWORD ptr originalFuncsWinhttp[40 * 8]
		proxy_WinHttpQueryDataAvailable endp
		proxy_WinHttpQueryHeaders proc
			jmp QWORD ptr originalFuncsWinhttp[41 * 8]
		proxy_WinHttpQueryHeaders endp
		proxy_WinHttpQueryOption proc
			jmp QWORD ptr originalFuncsWinhttp[42 * 8]
		proxy_WinHttpQueryOption endp
		proxy_WinHttpReadData proc
			jmp QWORD ptr originalFuncsWinhttp[43 * 8]
		proxy_WinHttpReadData endp
		proxy_WinHttpReadProxySettings proc
			jmp QWORD ptr originalFuncsWinhttp[44 * 8]
		proxy_WinHttpReadProxySettings endp
		proxy_WinHttpReadProxySettingsHvsi proc
			jmp QWORD ptr originalFuncsWinhttp[45 * 8]
		proxy_WinHttpReadProxySettingsHvsi endp
		proxy_WinHttpReceiveResponse proc
			jmp QWORD ptr originalFuncsWinhttp[46 * 8]
		proxy_WinHttpReceiveResponse endp
		proxy_WinHttpResetAutoProxy proc
			jmp QWORD ptr originalFuncsWinhttp[47 * 8]
		proxy_WinHttpResetAutoProxy endp
		proxy_WinHttpSaveProxyCredentials proc
			jmp QWORD ptr originalFuncsWinhttp[48 * 8]
		proxy_WinHttpSaveProxyCredentials endp
		proxy_WinHttpSendRequest proc
			jmp QWORD ptr originalFuncsWinhttp[49 * 8]
		proxy_WinHttpSendRequest endp
		proxy_WinHttpSetCredentials proc
			jmp QWORD ptr originalFuncsWinhttp[50 * 8]
		proxy_WinHttpSetCredentials endp
		proxy_WinHttpSetDefaultProxyConfiguration proc
			jmp QWORD ptr originalFuncsWinhttp[51 * 8]
		proxy_WinHttpSetDefaultProxyConfiguration endp
		proxy_WinHttpSetOption proc
			jmp QWORD ptr originalFuncsWinhttp[52 * 8]
		proxy_WinHttpSetOption endp
		proxy_WinHttpSetStatusCallback proc
			jmp QWORD ptr originalFuncsWinhttp[53 * 8]
		proxy_WinHttpSetStatusCallback endp
		proxy_WinHttpSetTimeouts proc
			jmp QWORD ptr originalFuncsWinhttp[54 * 8]
		proxy_WinHttpSetTimeouts endp
		proxy_WinHttpTimeFromSystemTime proc
			jmp QWORD ptr originalFuncsWinhttp[55 * 8]
		proxy_WinHttpTimeFromSystemTime endp
		proxy_WinHttpTimeToSystemTime proc
			jmp QWORD ptr originalFuncsWinhttp[56 * 8]
		proxy_WinHttpTimeToSystemTime endp
		proxy_WinHttpWebSocketClose proc
			jmp QWORD ptr originalFuncsWinhttp[57 * 8]
		proxy_WinHttpWebSocketClose endp
		proxy_WinHttpWebSocketCompleteUpgrade proc
			jmp QWORD ptr originalFuncsWinhttp[58 * 8]
		proxy_WinHttpWebSocketCompleteUpgrade endp
		proxy_WinHttpWebSocketQueryCloseStatus proc
			jmp QWORD ptr originalFuncsWinhttp[59 * 8]
		proxy_WinHttpWebSocketQueryCloseStatus endp
		proxy_WinHttpWebSocketReceive proc
			jmp QWORD ptr originalFuncsWinhttp[60 * 8]
		proxy_WinHttpWebSocketReceive endp
		proxy_WinHttpWebSocketSend proc
			jmp QWORD ptr originalFuncsWinhttp[61 * 8]
		proxy_WinHttpWebSocketSend endp
		proxy_WinHttpWebSocketShutdown proc
			jmp QWORD ptr originalFuncsWinhttp[62 * 8]
		proxy_WinHttpWebSocketShutdown endp
		proxy_WinHttpWriteData proc
			jmp QWORD ptr originalFuncsWinhttp[63 * 8]
		proxy_WinHttpWriteData endp
		proxy_WinHttpWriteProxySettings proc
			jmp QWORD ptr originalFuncsWinhttp[64 * 8]
		proxy_WinHttpWriteProxySettings endp
endif
end