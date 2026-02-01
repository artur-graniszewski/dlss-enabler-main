#pragma once
#include <wtypes.h>
#include <string>

namespace Preloader
{
	HMODULE OnLibraryLoad(std::wstring libName, bool& redirect);

	void OnModuleLoad();
}