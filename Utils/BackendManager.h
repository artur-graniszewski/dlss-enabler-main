#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include "INgxBackend.h"


// Centralized dynamic loader for external backends (upscaler & frame generator)
class BackendManager {
public:
	BackendManager(IBackendLoader& loader, INgxLogger& logger)
		: loader(loader), logger(logger) {
	}


	// Lazy-load and cache modules; thread-safe enough for current usage pattern.
	HMODULE GetUpscaler(); // Loads e.g. L"dlss-enabler-upscaler.dll"
	HMODULE GetFrameGen(); // Loads e.g. L"dlssg_to_fsr3_amd_is_better*.dll"


	// Optional helpers to explicitly clear cached modules in tests
	void ResetForTests();


private:
	IBackendLoader& loader;
	INgxLogger& logger;
	HMODULE upscaler{ nullptr };
	HMODULE frameGen{ nullptr };
};