ifdef RAX
	.code
		extern originalFuncsDxgi:QWORD
		proxy_CreateDXGIFactory proc
			jmp QWORD ptr originalFuncsDxgi[0 * 8]
		proxy_CreateDXGIFactory endp
		proxy_CreateDXGIFactory1 proc
			jmp QWORD ptr originalFuncsDxgi[1 * 8]
		proxy_CreateDXGIFactory1 endp
		proxy_CreateDXGIFactory2 proc
			jmp QWORD ptr originalFuncsDxgi[2 * 8]
		proxy_CreateDXGIFactory2 endp
		proxy_DXGIDeclareAdapterRemovalSupport proc
			jmp QWORD ptr originalFuncsDxgi[3 * 8]
		proxy_DXGIDeclareAdapterRemovalSupport endp
		proxy_DXGIGetDebugInterface1 proc
			jmp QWORD ptr originalFuncsDxgi[4 * 8]
		proxy_DXGIGetDebugInterface1 endp
endif
end
