#include <string>
#include "Common.h"
#include "../Core/Context.h"

namespace Preloader
{
	static void PreloadStreamline()
	{
		auto dir = Common::GetModuleDirectory();
		bool isUE; 

		if (dir.ends_with(L"\\Binaries\\Win64")) {
			isUE = true;
			LOG_INFO(L"[INIT] Preloader: detected Unreal Engine directory structure");
		}

		if (GetModuleHandleW(L"sl.interposer.dll")) {
			LOG_WARNING(L"[INIT] Preloader: Streamline Interposer already loaded");
			return;
		}

		LOG_INFO(L"[INIT] Preloader: searching for Streamline");
		const wchar_t* bruteInterposerPaths[] = {
			L"sl.interposer.dll",
			L"..\\..\\..\\Engine\\Plugins\\Streamline\\Binaries\\ThirdParty\\Win64\\sl.interposer.dll",
			L"..\\..\\..\\Engine\\Plugins\\Runtime\\Nvidia\\Streamline\\Binaries\\ThirdParty\\Win64\\sl.interposer.dll",
		};

		bool slFound = false;
		for (auto interposer : bruteInterposerPaths) {
			auto path = dir + interposer;

			if (Common::GetFileVersion(path.c_str()) != L"") {
				ctx.streamline.interposerPath = path;
			}
		}

		if (!slFound) {
			LOG_INFO(L"[INIT] Preloader: Streamline Interposer not found");
		}
	}

	HMODULE OnLibraryLoad(std::wstring libName, bool &redirect)
	{
		redirect = false;

		if (libName == L"EOSOVH-Win64-Shipping.dll") {
			redirect = true;
			LOG_WARNING(L"[INIT] Epic Store Overlay has been disabled for compatibility reasons");
			SetLastError(ERROR_MOD_NOT_FOUND);
			return nullptr;
		}

		if (libName == L"steam_api64.dll!") {
			redirect = true;
			LOG_WARNING(L"[INIT] Steam Overlay has been disabled for compatibility reasons");
			SetLastError(ERROR_MOD_NOT_FOUND);
			return nullptr;
		}

		return nullptr;
	}

	void PreloadReshade()
	{
		std::wstring path = L"reshade64.dll";
		auto version = Common::GetPluginVersion(path.c_str());
		if (version != L"") {
			LOG_INFO(L"[INIT] Reshade (version: " + version + L") detected, loading the module");
			if (Common::LoadPlugin(path)) {
				LOG_INFO(L"[INIT] Reshade module loaded successfully");
			}
			else {
				LOG_ERROR(L"[INIT] Reshade module failed to load");
			}
		}
	}
	void OnModuleLoad()
	{
		PreloadStreamline();
		PreloadReshade();
	}
}