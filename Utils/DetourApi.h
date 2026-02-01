// DetourApi.h
#pragma once
#include <windows.h>

struct AttachRequest {
    void** ppTarget;   // &OriginalX
    void* pDetour;    // DetourX
    const char* name;  // for logs
};

struct IDetourApi {
    virtual ~IDetourApi() = default;
    virtual void RestoreAfterWith() = 0;
    virtual bool TransactionBegin() = 0;
    virtual void UpdateThread(HANDLE hThread) = 0;
    virtual bool Attach(AttachRequest req) = 0;

    virtual bool CommitEx(PVOID** pFailed = nullptr) = 0;

    virtual FARPROC FindFunction(const char* module, const char* name) = 0;
    virtual HMODULE  GetModHandleW(const wchar_t* name) = 0;
    virtual FARPROC  GetProc(HMODULE h, const char* name) = 0;
};