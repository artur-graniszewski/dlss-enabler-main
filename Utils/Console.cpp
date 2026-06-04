#include <wtypes.h>
#include <windows.h>
#include <processthreadsapi.h> // GetThreadDescription (Win10+)
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <filesystem>

#include "Console.h"
#include "Common.h"

#define SPDLOG_EOL "\r\n"
#include "spdlog/spdlog.h"
#include "spdlog/async.h" 
#include "spdlog/sinks/basic_file_sink.h"



namespace
{
    struct ConsoleState {

        // --- Thread helpers ---
        static std::wstring ThreadTag() {
            DWORD tid = GetCurrentThreadId();
            std::wostringstream oss;
            oss << L"[tid " << tid << L"]";
            return oss.str();
        }

        // (optional) one-time thread name retrieval; cached in TLS
        static std::wstring ThreadNameTag() {
            static thread_local std::wstring cached;
#if (_WIN32_WINNT >= 0x0A00) // Win10
            if (cached.empty()) {
                PWSTR nameW = nullptr;
                if (SUCCEEDED(GetThreadDescription(GetCurrentThread(), &nameW)) && nameW) {
                    cached = std::wstring(L"[tname ") + nameW + L"]";
                    LocalFree(nameW);
                }
            }
#endif
            return cached; // may be empty
        }

        // Concatenates [tid ...] and (optionally) [tname ...]
        static std::wstring ThreadTags() {
            std::wstring tags = ThreadTag();

            return tags;
        }

        // Console handles
        HANDLE out = nullptr, old_out = nullptr;
        HANDLE err = nullptr, old_err = nullptr;
        HANDLE in = nullptr, old_in = nullptr;

        // Logging - we use our own logger, NOT the global spdlog registry
        std::shared_ptr<spdlog::logger> logger;
        bool loggingEnabled = false;

        // Our own thread pool - isolated from other libraries
        std::shared_ptr<spdlog::details::thread_pool> ownThreadPool;

        // Buffered echo
        std::string inMemoryLog;
        bool logFlushed = false;
        int lastStatusLen = 0;

        // Helpers
        static ConsoleState& I() {
            static ConsoleState s;
            return s;
        }

        static std::wstring NowStampW(const wchar_t* fmt = L"%a %b %d %H:%M:%S") {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_s(&tm, &t);
            std::wostringstream oss;
            oss << L'[' << std::put_time(&tm, fmt) << L']';
            return oss.str();
        }

        static std::string WideToUtf8(const std::wstring& w) {
            if (w.empty()) return {};
            int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
            std::string out(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), size, nullptr, nullptr);
            return out;
        }

        static std::wstring ToWide(const std::wstring& s) { return s; } // convenience overload
        static std::wstring ToWide(const std::string& s) {
            if (s.empty()) return {};
            int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
            std::wstring out(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), size);
            return out;
        }

        void writeConsoleW(const std::wstring& w) {
            if (!out) return;
            DWORD written = 0;
            WriteConsoleW(out, w.c_str(), (DWORD)w.size(), &written, nullptr);
        }

        void setConsoleColors(int color, WORD& original) {
            if (!out || color < 0) return;
            CONSOLE_SCREEN_BUFFER_INFO ci{};
            if (GetConsoleScreenBufferInfo(out, &ci)) {
                original = ci.wAttributes;
                SetConsoleTextAttribute(out, (WORD)color);
            }
        }

        void restoreConsoleColors(int color, WORD original) {
            if (!out || color < 0) return;
            SetConsoleTextAttribute(out, original);
        }
    };
} // namespace

// ==============================
// Console API implementation
// ==============================

void Console::Attach()
{
    auto& S = ::ConsoleState::I();

    // Save originals
    S.old_out = GetStdHandle(STD_OUTPUT_HANDLE);
    S.old_err = GetStdHandle(STD_ERROR_HANDLE);
    S.old_in = GetStdHandle(STD_INPUT_HANDLE);

    // Create/attach console
    AllocConsole();
    AttachConsole(GetCurrentProcessId());

    // Grab fresh handles
    S.out = GetStdHandle(STD_OUTPUT_HANDLE);
    S.err = GetStdHandle(STD_ERROR_HANDLE);
    S.in = GetStdHandle(STD_INPUT_HANDLE);

    // Set reasonable console modes
    SetConsoleMode(S.out, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
    SetConsoleMode(S.in, ENABLE_INSERT_MODE | ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);

    // Ctrl handler
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)Console::CtrlHandler, TRUE);

    // Title tweak
    char title[256] = {};
    GetConsoleTitleA(title, 256);
    strcat_s(title, " - DLSS Enabler Debug Console");
    SetConsoleTitleA(title);

    // (optional) block the Close button
    if (HWND wnd = GetConsoleWindow()) {
        LONG style = GetWindowLong(wnd, GWL_STYLE);
        style &= ~WS_SYSMENU;
        SetWindowLong(wnd, GWL_STYLE, style);
    }
}

BOOL Console::CtrlHandler(DWORD code)
{
    switch (code) {
    case CTRL_CLOSE_EVENT:
        return TRUE;  // signalling the handling (to avoid hard kill)
    default:
        return FALSE;
    }
}

void Console::EnableLogging(bool enable)
{
    auto& S = ::ConsoleState::I();
    if (!enable || S.loggingEnabled) return;

    // KEY CHANGE: Create our OWN thread pool, do not use global spdlog::init_thread_pool()
    // This isolates our logger from other libraries that may also use spdlog
    S.ownThreadPool = std::make_shared<spdlog::details::thread_pool>(32768, 1);

    const std::wstring logPathW =
        Common::GetProcessFilePath().remove_filename().wstring() + L"\\dlss-enabler.log";

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPathW, /*truncate=*/true);
    fileSink->set_pattern("%v");                    // we write pre-formatted lines
    fileSink->set_level(spdlog::level::trace);

    std::vector<spdlog::sink_ptr> sinks = { fileSink };

    // Create async logger with OUR OWN thread pool
    auto logger = std::make_shared<spdlog::async_logger>(
        "dlss-enabler-isolated",  // unique name to avoid collisions
        sinks.begin(),
        sinks.end(),
        S.ownThreadPool,          // use our own thread pool!
        spdlog::async_overflow_policy::block
    );

    // DO NOT register in global registry - this protects against override by other libraries
    // spdlog::register_logger(logger);  // <-- INTENTIONALLY REMOVED

    logger->set_level(spdlog::level::trace);
    logger->set_pattern("%v");

    // Flush on error level
    logger->flush_on(spdlog::level::trace);

    S.logger = std::move(logger);
    S.loggingEnabled = true;
    ResetLogging();
}

void Console::ResetLogging()
{
    auto& S = ::ConsoleState::I();
    if (S.logger) {
        S.logger->set_pattern("%v");
    }
}

void Console::FlushBuffer()
{
    auto& S = ::ConsoleState::I();
    if (S.out && !S.inMemoryLog.empty()) {
        DWORD written = 0;
        WriteConsoleA(S.out, S.inMemoryLog.c_str(), (DWORD)S.inMemoryLog.size(), &written, nullptr);
        S.inMemoryLog.erase();
        S.logFlushed = true;
    }
}

// Manual flush - call periodically or on shutdown
void Console::FlushLog()
{
    auto& S = ::ConsoleState::I();
    if (S.logger) {
        S.logger->flush();
    }
}

bool Console::PrintMultiline(const std::wstring& message)
{
    auto& S = ::ConsoleState::I();
    std::wistringstream in(message);
    std::wstring line;
    bool printedAny = false;

    while (std::getline(in, line)) {
        // Remove trailing CR if present (handling \r\n)
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        // Empty line -> print empty info (preserving timestamp format)
        if (line.empty()) {
            Info(L"");
            printedAny = true;
            continue;
        }

        // If line looks like already formatted entry (e.g. starts with "[info]" / "[trace]" etc.),
        // treat it as ready and send without adding another [info].
        if (!line.empty() && line.front() == L'[') {
            // no color (-1) - Print won't colorize
            Print(L"", line, -1, spdlog::level::info);
            printedAny = true;
            continue;
        }

        // Normal line -> treat as info level
        Info(line);
        printedAny = true;
    }

    return printedAny;
}

bool Console::Info(const std::wstring& message)
{
    return Print(L"info", message, FOREGROUND_GREEN, spdlog::level::info);
}

bool Console::Trace(const std::wstring& message)
{
    return Print(L"trace", message, FOREGROUND_GREEN | FOREGROUND_BLUE, spdlog::level::trace);
}

bool Console::Error(const std::wstring& message)
{
    return Print(L"error", message, FOREGROUND_RED | FOREGROUND_INTENSITY, spdlog::level::err);
}

bool Console::Warning(const std::wstring& message)
{
    return Print(L"warning", message, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, spdlog::level::warn);
}

bool Console::Print(const std::wstring& level, const std::wstring& message, int color, spdlog::level::level_enum logLevel)
{
    auto& S = ::ConsoleState::I();

    const std::wstring stamp = ConsoleState::NowStampW();
    auto threads = ConsoleState::ThreadTags();

    // FIXED: Add [level] to the line logged to file
    std::wstring levelTag = level.empty() ? L"" : (L"[" + level + L"] ");
    const std::wstring line_core = stamp + L" " + threads + L" " + levelTag + message; // with level!
    const std::wstring line_console = L" " + message + L"\r\n"; // EOL only for console/screen

    // preview buffer + console
    if (!S.logFlushed) {
        S.inMemoryLog += ConsoleState::WideToUtf8(stamp + L" " + threads + L" [" + level + L"] " + message + L"\r\n");
    }

    static WORD originalAttrs = 0;

    bool ok = false;
    if (S.out) {
        DWORD written = 0;
        WriteConsoleW(S.out, stamp.c_str(), (DWORD)stamp.size(), &written, nullptr);
        WriteConsoleW(S.out, L" ", 1, &written, nullptr);
        WriteConsoleW(S.out, threads.c_str(), (DWORD)threads.size(), &written, nullptr);

        WriteConsoleW(S.out, L" [", 2, &written, nullptr);
        if (originalAttrs == 0) {
            S.setConsoleColors(color, originalAttrs);
        }
        else {
            WORD attrs = 0;
            S.setConsoleColors(color, attrs);
        }
        if (level != L"") {
            WriteConsoleW(S.out, level.c_str(), (DWORD)level.size(), &written, nullptr);
        }
        S.restoreConsoleColors(color, originalAttrs);
        WriteConsoleW(S.out, L"]", 1, &written, nullptr);
        ok = !!WriteConsoleW(S.out, line_console.c_str(), (DWORD)line_console.size(), &written, nullptr);
    }

    // FIXED: Use appropriate log level instead of always info()
    if (S.loggingEnabled && S.logger) {
        std::string utf8Line = ConsoleState::WideToUtf8(line_core);

        switch (logLevel) {
        case spdlog::level::trace:
            S.logger->trace(utf8Line);
            break;
        case spdlog::level::debug:
            S.logger->debug(utf8Line);
            break;
        case spdlog::level::info:
            S.logger->info(utf8Line);
            break;
        case spdlog::level::warn:
            S.logger->warn(utf8Line);
            break;
        case spdlog::level::err:
            S.logger->error(utf8Line);
            break;
        case spdlog::level::critical:
            S.logger->critical(utf8Line);
            break;
        default:
            S.logger->info(utf8Line);
            break;
        }
    }

    return ok;
}

bool Console::ShowStatus(std::wstring message)
{
    auto& S = ::ConsoleState::I();

    if (S.lastStatusLen > (int)message.length()) {
        message += std::wstring(S.lastStatusLen - (int)message.length(), L' ');
    }
    S.lastStatusLen = (int)message.length();
    message += L"\r";

    if (!S.out) return false;
    DWORD charsWritten = 0;
    return !!WriteConsoleW(S.out, message.c_str(), (DWORD)message.length(), &charsWritten, nullptr);
}