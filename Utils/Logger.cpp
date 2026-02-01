#include "Console.h"
#include "../Core/Context.h"

void LogTrace(const std::wstring& message)
{
	if (!ctx.logging.isUltraDebugEnabled) {
		return;
	}
	
	Console::Trace(message);
}

void LogDebug(const std::wstring& message)
{
	if (!ctx.logging.isExtraDebugEnabled) {
		return; 
	}
	
	Console::Info(message);
}

void LogError(const std::wstring& message)
{
	Console::Error(message);
}

void LogWarning(const std::wstring& message)
{
	Console::Warning(message);
}

void LogInfo(const std::wstring& message)
{
	Console::Info(message);
}
