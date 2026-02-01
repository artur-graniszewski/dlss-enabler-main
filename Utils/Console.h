#pragma once
#include <string>
#include <wtypes.h>
#include "spdlog/spdlog.h"  // needed for spdlog::level::level_enum

class Console {
public:
	static bool Info(const std::wstring& message);
	static bool Trace(const std::wstring& message);
	static bool Error(const std::wstring& message);
	static bool Warning(const std::wstring& message);
	static bool PrintMultiline(const std::wstring& message);
	static void Attach();
	static void FlushBuffer();
	static void FlushLog();  // NEW: manual logger flush (call on shutdown)
	static void EnableLogging(bool enable);
	static void ResetLogging();
	static bool ShowStatus(std::wstring message);

private:
	static bool Print(const std::wstring& lvl, const std::wstring& fmt, int color,
		spdlog::level::level_enum logLevel = spdlog::level::info);

	static BOOL CtrlHandler(DWORD fdwCtrlType);
};