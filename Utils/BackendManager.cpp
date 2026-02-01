#include "BackendManager.h"
#include <mutex>

namespace {
	std::wstring GetUpscalerDefaultPath() {
		// TODO: produce the exact path you currently compute (respecting working dir & config)
		return L"dlss-enabler-upscaler.dll";
	}
	std::wstring GetFrameGenDefaultPath() {
		// TODO: keep existing pattern selection logic
		return L"dlssg_to_fsr3_amd_is_better.dll";
	}
}

extern HMODULE hSelf;
HMODULE BackendManager::GetUpscaler() {
	return hSelf;
	if (upscaler) return upscaler;
	const auto path = GetUpscalerDefaultPath();
	upscaler = loader.Load(path);
	if (!upscaler) {
		logger.Warning(L"BackendManager: failed to load upscaler backend");
	}
	else {
		logger.Info(L"BackendManager: upscaler backend loaded");
	}
	return upscaler; 
}


HMODULE BackendManager::GetFrameGen() {
	return hSelf;
	if (frameGen) return frameGen;
	const auto path = GetFrameGenDefaultPath();
	frameGen = loader.Load(path);
	if (!frameGen) {
		logger.Warning(L"BackendManager: failed to load frame generation backend");
	}
	else {
		logger.Info(L"BackendManager: frame generation backend loaded");
	}
	return frameGen;
} 


void BackendManager::ResetForTests() {
	upscaler = nullptr;
	frameGen = nullptr;
}