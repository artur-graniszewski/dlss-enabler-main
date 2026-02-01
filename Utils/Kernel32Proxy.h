#pragma once
#include <Windows.h>
#include "Common.h"

typedef BOOL(WINAPI* PFreeLibrary)(HMODULE hLibModule);

extern PFreeLibrary pOriginalFreeLibrary;
extern PFreeLibrary _pOriginalFreeLibrary;
extern GetProcAddress_t OriginalGetProcAddress;
extern GetProcAddress_t _OriginalGetProcAddress;
extern LoadLibraryExW_t OriginalLoadLibraryExW;
extern LoadLibraryExW_t _OriginalLoadLibraryExW;
extern LoadLibraryW_t OriginalLoadLibraryW;
extern LoadLibraryW_t _OriginalLoadLibraryW;

BOOL WINAPI DetouredFreeLibrary(HMODULE hLibModule);
HMODULE WINAPI DetourLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE WINAPI _DetourLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE WINAPI DetourLoadLibraryW(LPCWSTR lpLibFileName);
HMODULE WINAPI _DetourLoadLibraryW(LPCWSTR lpLibFileName);
FARPROC WINAPI DetourGetProcAddress(HMODULE hModule, LPCSTR lpProcName);
FARPROC WINAPI _DetourGetProcAddress(HMODULE hModule, LPCSTR lpProcName);

void InitializeDetours();
void DetachDetours();


