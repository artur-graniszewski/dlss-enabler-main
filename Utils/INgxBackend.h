#pragma once
#include <windows.h>
#include <string>
#include <optional>


// Minimal argument structs; extend gradually with fields you actually use.
struct NgxInitArgs { /* TODO: fill with the used NVSDK fields */ };
struct NgxCreateArgs { /* TODO */ };
struct NgxEvalArgs { /* TODO */ };
struct NgxShutdownArgs { /* TODO */ };


struct INgxLogger {
	virtual ~INgxLogger() = default;
	virtual void Info(const std::wstring msg) = 0;
	virtual void Warning(const std::wstring msg) = 0;
	virtual void Error(const std::wstring msg) = 0;
};


struct IBackendLoader {
	virtual ~IBackendLoader() = default;
	virtual HMODULE Load(const std::wstring& path) = 0; // Mockable in tests
	virtual FARPROC Resolve(HMODULE module, const char* name) = 0;
};


struct INgxBackend {
	virtual ~INgxBackend() = default;
	virtual bool Available() const = 0;
	virtual bool Init(const NgxInitArgs&) = 0;
	virtual bool Create(const NgxCreateArgs&) = 0;
	virtual bool Evaluate(const NgxEvalArgs&) = 0;
	virtual void Release() = 0;
	virtual void Shutdown(const NgxShutdownArgs&) = 0;
};