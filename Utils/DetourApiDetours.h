#pragma once
#include "DetourApi.h"

struct DetourApiDetours : IDetourApi {
    void RestoreAfterWith() override;
    bool TransactionBegin() override;
    void UpdateThread(HANDLE hThread) override;
    bool Attach(AttachRequest req) override;
    bool CommitEx(PVOID** pFailed = nullptr) override;
    FARPROC FindFunction(const char* m, const char* n) override;
    HMODULE GetModHandleW(const wchar_t* n) override;
    FARPROC GetProc(HMODULE h, const char* n) override;
    bool Detach(AttachRequest req) override;
};