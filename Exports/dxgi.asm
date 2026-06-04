ifdef RAX
	.code
		extern OriginalFunctions_dxgi:QWORD
		proxy_CreateDXGIFactory proc
			jmp QWORD ptr OriginalFunctions_dxgi[0 * 8]
		proxy_CreateDXGIFactory endp
		proxy_CreateDXGIFactory1 proc
			jmp QWORD ptr OriginalFunctions_dxgi[1 * 8]
		proxy_CreateDXGIFactory1 endp
		proxy_CreateDXGIFactory2 proc
			jmp QWORD ptr OriginalFunctions_dxgi[2 * 8]
		proxy_CreateDXGIFactory2 endp
		proxy_DXGIDeclareAdapterRemovalSupport proc
			jmp QWORD ptr OriginalFunctions_dxgi[3 * 8]
		proxy_DXGIDeclareAdapterRemovalSupport endp
		proxy_DXGIGetDebugInterface1 proc
			jmp QWORD ptr OriginalFunctions_dxgi[4 * 8]
		proxy_DXGIGetDebugInterface1 endp
endif
end