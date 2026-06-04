#include "DetourApi.h"
#include "../Detours/detours.h"
#include "DetourApiDetours.h"
#include "Common.h"

extern GetProcAddress_t OriginalGetProcAddress;
void DetourApiDetours::RestoreAfterWith() { DetourRestoreAfterWith(); }
bool DetourApiDetours::TransactionBegin() { return DetourTransactionBegin() == NO_ERROR; }
void DetourApiDetours::UpdateThread(HANDLE h) { DetourUpdateThread(h); }
bool DetourApiDetours::Attach(AttachRequest r) {
    return DetourAttach(reinterpret_cast<PVOID*>(r.ppTarget), r.pDetour) == NO_ERROR;
}
bool DetourApiDetours::CommitEx(PVOID** pFailed) {
    return DetourTransactionCommitEx(pFailed) == NO_ERROR;
}
FARPROC DetourApiDetours::FindFunction(const char* m, const char* n) {
    return reinterpret_cast<FARPROC>(DetourFindFunction(m, n));
}

bool DetourApiDetours::Detach(AttachRequest r) {
    return DetourDetach(reinterpret_cast<PVOID*>(r.ppTarget), r.pDetour) == NO_ERROR;
}
HMODULE DetourApiDetours::GetModHandleW(const wchar_t* n) { return ::GetModuleHandleW(n); }
FARPROC DetourApiDetours::GetProc(HMODULE h, const char* n) { return OriginalGetProcAddress(h, n); } 