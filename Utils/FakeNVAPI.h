#pragma once
#include <Wtypes.h>

struct FakeNvapiConfig {
    const wchar_t* iniPath = nullptr;
    bool hideXellLibrary = false;
};

bool FakeNvapi_Init(HMODULE self, const FakeNvapiConfig* cfg);
void FakeNvapi_Shutdown();
extern "C" void* __cdecl nvapi_QueryInterface(unsigned int id);