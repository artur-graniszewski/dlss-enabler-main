#pragma once
#include <atomic>
#include <mutex>


// Shared runtime state for NGX routing; inject into proxies for testability.
struct NgxRuntimeState {
	std::atomic<bool> isUpscalerEvaluated{ false };
	std::atomic<bool> isUpscalerResolutionReported{ false };
	std::atomic<bool> isFgEvaluated{ false };
	std::atomic<bool> isNgxEvaluated{ false };
	std::atomic<int> initCount{ 0 };
	std::mutex mtx; // For rare critical sections
};

struct NgxPointerHash {
	std::size_t operator()(const NVSDK_NGX_Handle* ptr) const noexcept {
		return std::hash<const void*>{}(static_cast<const void*>(ptr));
	}
};