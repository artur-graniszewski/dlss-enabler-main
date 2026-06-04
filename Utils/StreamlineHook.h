#pragma once
#include "Hook.h"
#include "Common.h"
#include "DetourTxn.h"
#include "StreamlineProxy.h"

SLINIT original_slInit = nullptr;
SLFEATINIT original_slIsFeatureLoaded = nullptr;
SL3ARGS original_slIsFeatureSupported = nullptr;
SL0ARGS original_slShutdown = nullptr;
DLSSGSETFEATLOADED original_slSetFeatureLoaded = nullptr;
SL4ARGS original_D3D12CreateDevice = nullptr;
SL4ARGS original_slHookPresent = nullptr;
SL5ARGS original_slHookPresent1 = nullptr;
SL4ARGS original_D3D12CreateRootSignatureDeserializer = nullptr;
SL4ARGS original_D3D12CreateVersionedRootSignatureDeserializer = nullptr;
SL4ARGS original_D3D12SerializeRootSignature = nullptr;
SL3ARGS original_slSetConstants = nullptr;
SLGETFEATREQ original_slGetFeatureRequirements = nullptr;
SL2ARGS original_slFreeResources = nullptr;
SL2ARGS original_D3D12GetDebugInterface = nullptr;
SL2ARGS original_slGetFeatureVersion = nullptr;
SL2ARGS original_slGetNewFrameToken = nullptr;
SL2ARGS original_slGetNativeInterface = nullptr;
//SL3ARGS original_slGetFeatureFunction = nullptr;
SLGETFEATFUNC original_slGetFeatureFunction = nullptr;
SL3ARGS original_slAllocateResources = nullptr;
SLFEATUREARGS original_slEvaluateFeature = nullptr;
SL1ARG original_slUpgradeInterface = nullptr;
SL1ARG original_slSetD3DDevice = nullptr;
SL1ARG original_slSetVulkanInfo = nullptr;
SL2ARGS original_CreateDXGIFactory = nullptr;
SL2ARGS original_CreateDXGIFactory1 = nullptr;
SL3ARGS original_CreateDXGIFactory2 = nullptr;
SL3ARGS original_D3D12SerializeVersionedRootSignature = nullptr;
SL3ARGS original_D3D12GetInterface = nullptr;
SLSETTAG original_slSetTag = nullptr;
SLSETTAGV1 original_slSetTagV1 = nullptr;
SL2ARGS original_slDLSSSetOptions = nullptr;
DLSSGSETOPTS original_slDLSSGSetOptions = nullptr;
DLSSGSETFEATLOADED original_slSetFeatureLoaded2 = nullptr;
DEEPDVCSETOPTS original_slDeepDVCSetOptions = nullptr;

struct HookStreamline : IHook {
    const std::wstring Name() const override { return L"Streamline hooks"; }
    HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
    int         Priority() const override { return 50; }

    bool CanInstall(Context& ctx, IDetourApi& api) override {
        return api.GetModHandleW(L"sl.interposer.dll") != nullptr;
    }

	std::wstring InstallPlugin(std::wstring(pluginType), IDetourApi& api) {
		auto handle = api.GetModHandleW(pluginType.c_str());

		if (!handle) {
			return L"0.0.0.0";
		}

		auto path = Common::GetModuleFilePath(handle);
		auto version = Common::GetFileVersion(path.c_str());

		if (version == L"") {
			return L"0.0.0.0";
		}

		LOG_INFO(L"[STREAMLINE] Streamline plugin detected: " + pluginType + L"(version: " + version + L")");
		return version;
	}

    bool Install(Context& ctx, IDetourApi& api) override {
		if (ctx.streamline.dlssdVersion[0] == L'0') {
			ctx.streamline.dlssdVersion = InstallPlugin(L"sl.dlss_d.dll", api);
		}

		if (ctx.streamline.dlssgVersion[0] == L'0') {
			ctx.streamline.dlssgVersion = InstallPlugin(L"sl.dlss_g.dll", api);
		}

		if (ctx.streamline.commonVersion[0] == L'0') {
			ctx.streamline.commonVersion = InstallPlugin(L"sl.common.dll", api);
		}

		if (ctx.streamline.dlssVersion[0] == L'0') {
			ctx.streamline.dlssVersion = InstallPlugin(L"sl.dlss.dll", api);
		}

		if (ctx.streamline.interposerVersion[0] == L'0') {
			ctx.streamline.interposerVersion = InstallPlugin(L"sl.interposer.dll", api);
		}

		if (ctx.streamline.deepDvcVersion[0] == L'0') {
			ctx.streamline.deepDvcVersion = InstallPlugin(L"sl.deepdvc.dll", api);
		}

		static bool alertShown = false;
		// Check interposer version: warn if >= 2.0 but < 2.7
		if (ctx.isFirstRun && !alertShown) {
			alertShown = true;
			const std::wstring& ver = ctx.streamline.interposerVersion;
			int slMajor = 0, slMinor = 0;
			if (ver != L"0.0.0.0") {
				auto dot1 = ver.find(L'.');
				if (dot1 != std::wstring::npos) {
					try {
						slMajor = std::stoi(ver.substr(0, dot1));
						auto dot2 = ver.find(L'.', dot1 + 1);
						slMinor = (dot2 != std::wstring::npos)
							? std::stoi(ver.substr(dot1 + 1, dot2 - dot1 - 1))
							: std::stoi(ver.substr(dot1 + 1));
					}
					catch (...) {}
				}
			}
			if (slMajor == 2 && slMinor < 7) {
				std::wstring msg =
					L"Outdated NVIDIA Streamline detected.\n\n"
					L"Installed version: " + ver + L"\n\n"
					L"DLSS Enabler requires Streamline 2.7.32 or newer\n"
					L"for full support of MFG and DFG.\n\n"
					L"Download the latest version::\n"
					L"https://github.com/NVIDIAGameWorks/Streamline";
				MessageBoxW(
					nullptr,
					msg.c_str(),
					L"DLSS Enabler - Outdated Streamline Detected",
					MB_OK | MB_ICONWARNING
				);
			}

			if (slMajor == 1) {
				std::wstring msg =
					L"Old NVIDIA Streamline detected.\n\n"
					L"Installed version: " + ver + L"\n\n"
					L"DLSS Enabler will run in limited mode.\n"
					L"Only Frame Generation 2x is available.\n";
				MessageBoxW(
					nullptr,
					msg.c_str(),
					L"DLSS Enabler - Old Streamline Detected",
					MB_OK | MB_ICONWARNING
				);
			}
		}


		std::wstring processName = Common::GetProcessFileName();
		auto sli = api.GetModHandleW(L"sl.interposer.dll");

		if (sli == nullptr) {
			LOG_WARNING(L"[STREAMLINE] No streamline interposer detected, aborting detours appliance");
			return false;
		}

		DetourTxn txn(api);
		static bool cp2077detourEnabled = false;
		if (processName == L"Cyberpunk2077.exe" && !cp2077detourEnabled) {
			cp2077detourEnabled = true;
			LOG_WARNING(L"[STREAMLINE] slInit: Cyberpunk 2077 detected, enabling UI anti-glitch hook");
			
			original_slSetTag = (SLSETTAG)api.GetProc(sli, "slSetTag");
			if (original_slSetTag) {
				if (!txn.attach((void**)&original_slSetTag, (void*)&detoured_slSetTagForCyberpunkFixed, "slSetTag")) return false;
			}

		}

		static bool streamlineDetoursEnabled = false;
		static bool warningReported = false;
		if (streamlineDetoursEnabled || ctx.streamline.interposerVersion[0] != L'2') {
			if (!streamlineDetoursEnabled && !warningReported) {
				LOG_WARNING(L"[STREAMLINE] Incompatible streamline library detected - no detours applied");
				warningReported = true;
			}
			return false;
		}

		streamlineDetoursEnabled = true;
		original_slInit = (SLINIT)api.GetProc(sli, "slInit");
		if (original_slInit) {
			if (!txn.attach((void**)&original_slInit, (void*)&detoured_slInit, "slInit")) return false;
		}

		original_slEvaluateFeature = (SLFEATUREARGS)api.GetProc(sli, "slEvaluateFeature");
		if (original_slEvaluateFeature) {
			if (!txn.attach((void**)&original_slEvaluateFeature, (void*)&detoured_slEvaluateFeature, "slEvaluateFeature")) return false;
		}

		original_slGetFeatureFunction = (SLGETFEATFUNC)api.GetProc(sli, "slGetFeatureFunction");
		if (original_slGetFeatureFunction) {
			if (!txn.attach((void**)&original_slGetFeatureFunction, (void*)&detoured_slGetFeatureFunction, "slGetFeatureFunction")) return false;
		}

		original_slSetFeatureLoaded = (DLSSGSETFEATLOADED)api.GetProc(sli, "slSetFeatureLoaded");
		if (original_slSetFeatureLoaded) {
			if (!txn.attach((void**)&original_slSetFeatureLoaded, (void*)&detoured_slSetFeatureLoaded, "slSetFeatureLoaded")) return false;
		}

        auto result = txn.commit();

		if (result) {
			LOG_INFO(L"[STREAMLINE] Detours applied successfully");
		}
		else {
			LOG_ERROR(L"[STREAMLINE] Failed to apply detours");
		}
		return result;
    }
};
