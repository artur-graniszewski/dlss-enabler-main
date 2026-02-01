#pragma once
#include <Windows.h>

namespace NVAPI
{
	typedef void* (__cdecl* nvapi_QueryInterface_t)(unsigned int function);
	extern nvapi_QueryInterface_t OriginalNvAPI_QueryInterface;

	void* __cdecl NvAPI_QueryInterface(unsigned int function);
}