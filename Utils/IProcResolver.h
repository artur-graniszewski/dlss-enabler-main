#pragma once
#include <windows.h>

using GetProcAddress_t = FARPROC(WINAPI*)(HMODULE, LPCSTR);

struct IProcResolver {
    virtual ~IProcResolver() = default;
    virtual FARPROC Resolve(HMODULE module, const char* name) = 0;
};