#include "../Utils/Common.h"
#include "Context.h"
#include "Core.h"
#include "../Utils/Console.h"
#include "../Utils/Kernel32Proxy.h"
#include "../Utils/Validator.h"
#include "../Utils/Autoconfig.h"
#include "../Utils/Optiscaler.h"

static std::wstring dlssEnablerVersion = L"";  
static std::wstring bundledFSR3ModVersion = L"1.0";

void Core::ShowVersion()
{
	double driverVer = ctx.driverVersion / 100.0;
	std::wstring hags = Validator::DescribeHAGSSetting(Validator::GetHAGSRegistrySetting());
	std::wstring sigs = Validator::DescribeNVIDIASignatureSetting(Validator::GetNVIDIASignatureSetting(), ctx.logging.isDebugEnabled);
	std::wstring fsr3 = Validator::DescribeDLSStoFSR3FilesStatus(Validator::AreDLSStoFSR3FilesPresent(), bundledFSR3ModVersion);
	auto fullPath = Common::GetModuleFilePath();

	std::wstring arch = (ctx.currentGpuArchitecture == NV_GPU_ARCHITECTURE_AD100 ? L"ada" : (ctx.currentGpuArchitecture == NV_GPU_ARCHITECTURE_GA100 ? L"ampere" : L"turing"));

	Common::Info(L"DLSS Enabler version " + dlssEnablerVersion + L" present" +
		L"\nRunning : " + fullPath.wstring() +
		L"\n\nSystem settings : " +
		L"\n - DLSS to FSR3 module : " + fsr3 +
		L"\n - NVIDIA driver signature checks: " + sigs +
		L"\n\nTarget setting : \n - Architecture version : " + arch +
		L"\n - Driver version : " + std::format(L"{:.2f}", driverVer).c_str());
	Common::KillProcess();
}

void Core::ShowHelp()
{
	Common::Info(L"DLSS Enabler version " + dlssEnablerVersion + L" present" +
		L"\n\nUsage:" +
		L"\n --dlss-debug                 - Show debug information" +
		L"\n --dlss-debug=extra      - Show extensive debug information" +
		L"\n --dlss-off                       - Disable this module" +
		L"\n --dlss-logging=on/off - Enable/disable logging to the file (default: off)" +
		L"\n --dlss-skip-validation   - Skip system checks" +
		L"\n --dlss-diagnostics         - Run the diagnostics" +
		L"\n --dlss-help                     - Show this help message" +
		L"\n --dlss-version                - Show module version\n" +
		L"\n --dlss-arch=ada/turing/ampere - Select target architecture (default: ada)\n" +
		L"\n --dlss-hags=on/off/sys - Switch HAGS on or off or honor system setting (default: on)\n" +
		L"\n --dlss-upscaler=auto/fsr/xess/dlss/fsr22/fsr31 - Select target upscaler or disable it (default: auto)\n" +
		L"\n --dlss-nvapi=proxy/mock/sys - Control which NVAPI implementation to use (default: proxy), sys = provided by driver\n" +
		L"\n --dlss-gpu-name=\"3Dfx VooDoo 2\" - Override GPU name reported to the application\n" +
		L"\n --dlss-upscaler-quality=sys/ultra - Enable ultra quality where upscaler renders in native resolution (default: sys)\n"

	);
	Common::KillProcess();
}

void Core::ShowDiagnostics()
{
	std::wstring summary = Validator::GetDiagnosticsReport();
	auto fullPath = Common::GetModuleFilePath();
	
	std::wstring version = L"DLSS Enabler version " + dlssEnablerVersion + L" present" +
		L"\nRunning : " + fullPath.wstring() + L"\n\n";


	Common::Info(version + summary.c_str());
	Common::KillProcess();
}

void Core::Finish(HINSTANCE hModule)
{
	if (ctx.isOptiscalerInitialized) {
		OptiScaler_Shutdown();
	}

	DetachDetours();
}

void Core::Initialize(HINSTANCE hModule)
{
	auto fullModulePath = Common::GetModuleFilePath();
	dlssEnablerVersion = Common::GetFileVersion(fullModulePath.c_str());
	if (CMD_EXISTS("--dlss-diagnostics")) {
		ShowDiagnostics();
	}

	if (CMD_EXISTS("--dlss-help")) {
		ShowHelp();
	}

	if (CMD_EXISTS("--dlss-version")) {
		ShowVersion();
	}

	Autoconfig::Initialize();

	auto modulePath = fullModulePath.wstring();
	auto applicationPath = Common::GetModuleFilePath(NULL).wstring();
	auto exeVersion = Common::GetFileVersion(applicationPath.c_str());

	LOG_INFO(L"==================================================================");
	LOG_INFO(L"Initializing DLSS Enabler version " + dlssEnablerVersion);
	LOG_INFO(L"==================================================================");
	LOG_INFO(L"Running " + applicationPath + L" (PID: " + std::to_wstring(GetCurrentProcessId()) + L")");
	LOG_INFO(L"Executable version: " + (exeVersion != L"" ? exeVersion : L"unknown"));
	LOG_INFO(L"Module running as " + modulePath);
	
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	std::wstring concatenatedArgs = L"";
	for (int i = 1; i < argc; ++i) { // Start from 1 to skip the program name
		std::wstring arg(argv[i]);
		if (arg.find(L"--dlss-") == 0) { // Check if the argument starts with "--dlss-"
			concatenatedArgs += arg + L" ";
		}
	}

	if (concatenatedArgs != L"") {
		LOG_INFO(L"With arguments: " + concatenatedArgs);
	}
	else {
		LOG_INFO(L"With no commandline arguments");
	}

	if (!ctx.isDlssEnablerOn) {
		return;
	}

	auto optiPatcher = L"plugins\\OptiPatcher.asi";
	if (Common::IsPluginPresent(optiPatcher)) {
		typedef void (*PFN_InitializeASI)(void);
		typedef bool (*PFN_PatchResult)(void);


		auto patcher = Common::LoadPlugin(optiPatcher);
		LOG_WARNING(L"[PATCHER] OptiPatcher detected and loaded");
		auto init = (PFN_InitializeASI) GetProcAddress(patcher, "InitializeASI");
		auto patchResult = (PFN_PatchResult) GetProcAddress(patcher, "PatchResult");

		if (init != nullptr)
			init();

		if (patchResult != nullptr) {
			auto pr = patchResult();

			if (pr) {
				LOG_INFO(L"[PATCHER] Game patching is successful");

				LOG_INFO(L"[PATCHER] Disabling spoofing");
				ctx.isOptiPatcherActive = true;
			}
			else {
				LOG_ERROR(L"[PATCHER] Game patching failed");
			}
		} 

	}
	//LoadLibraryW(L"nvngx.dll");



	Validator::ValidateHAGSSetting(Validator::GetHAGSRegistrySetting());
	Validator::ValidateNvidiaSignatureSetting(Validator::GetNVIDIASignatureSetting());
	Validator::ValidateDLSStoFSR3FilesStatus(Validator::AreDLSStoFSR3FilesPresent());
	Console::PrintMultiline(Validator::GetDiagnosticsReport());
	InitializeDetours();
	Autoconfig::InitializeFrameGeneration();
	Autoconfig::PreloadUpscaler();
}

