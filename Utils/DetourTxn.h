#pragma once
#include "DetourApi.h"

struct DetourTxn {
    IDetourApi& api;
    bool active = false;
    explicit DetourTxn(IDetourApi& a) : api(a) {
        active = api.TransactionBegin();
        api.UpdateThread(GetCurrentThread());
    }
    bool attach(void** ppTarget, void* pDetour, const char* name) {
        return api.Attach({ ppTarget, pDetour, name });
    }

    bool detach(void** ppTarget, void* pDetour, const char* name) {
        return api.Detach({ ppTarget, pDetour, name });
    }

    bool commit(PVOID** pFailed = nullptr) {
        if (!active) return true;
        active = false;
        return api.CommitEx(pFailed);
    }
    ~DetourTxn() { if (active) api.CommitEx(nullptr); }
}; 

template<typename T>
inline T DetoursFindTyped(IDetourApi& api, const char* mod, const char* name) {
    return reinterpret_cast<T>(api.FindFunction(mod, name));
}