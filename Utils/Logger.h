#pragma once
//#define LOG_TRACE(...) spdlog::trace(__VA_ARGS__)
//#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
//#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
//#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
//#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
//#define LOG_CRITICAL(...) spdlog::critical(__VA_ARGS__)
#include <string>
#define SPDLOG_EOL "\r\n"

void LogDebug(const std::wstring& message);
void LogTrace(const std::wstring& message);
void LogError(const std::wstring& message);
void LogInfo(const std::wstring& message);
void LogWarning(const std::wstring& message);

#define LOG_DEBUG(message) LogDebug(message)
#define LOG_TRACE(message) LogTrace(message)
#define LOG_ERROR(message) LogError(message)
#define LOG_INFO(message) LogInfo(message)
#define LOG_WARNING(message) LogWarning(message)


