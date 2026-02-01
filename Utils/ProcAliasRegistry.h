#pragma once
#include <windows.h>
#include <cstring>
#include <cwchar>

class ProcAliasRegistry
{
public:
    using FuncPtr = void(*)();

    struct Entry {
        HMODULE     module;
        const wchar_t* moduleName;  // opcjonalna nazwa modu³u (nullptr jeœli u¿ywamy handle)
        const char* name;
        FuncPtr     fn;
    };

    static ProcAliasRegistry& Instance() noexcept {
        static ProcAliasRegistry g;
        return g;
    }

    // Rejestracja aliasu przez HMODULE
    void RegisterAlias(HMODULE module, const char* name, FuncPtr fn) noexcept {
        RegisterAliasInternal(module, nullptr, name, fn);
    }

    // Rejestracja aliasu przez nazwê modu³u (np. L"nvngx_dlssg.dll")
    void RegisterAlias(const wchar_t* moduleName, const char* name, FuncPtr fn) noexcept {
        RegisterAliasInternal(nullptr, moduleName, name, fn);
    }

    void Clear() noexcept {
        count_ = 0;
    }

    // Szukanie aliasu przez HMODULE
    FuncPtr TryResolve(HMODULE module, const char* name) const noexcept {
        if (!module || !name)
            return nullptr;

        if (!module || !name || reinterpret_cast<uintptr_t>(name) < 0x10000)
            return nullptr;

        for (std::size_t i = 0; i < count_; ++i) {
            // Dopasowanie przez handle
            if (entries_[i].module == module &&
                std::strcmp(entries_[i].name, name) == 0)
            {
                return entries_[i].fn;
            }

            // Dopasowanie przez nazwê modu³u
            if (entries_[i].moduleName != nullptr &&
                std::strcmp(entries_[i].name, name) == 0 &&
                ModuleNameMatches(module, entries_[i].moduleName))
            {
                return entries_[i].fn;
            }
        }
        return nullptr;
    }

private:
    static constexpr std::size_t kMaxEntries = 128;

    ProcAliasRegistry() = default;
    ~ProcAliasRegistry() = default;
    ProcAliasRegistry(const ProcAliasRegistry&) = delete;
    ProcAliasRegistry& operator=(const ProcAliasRegistry&) = delete;

    void RegisterAliasInternal(HMODULE module, const wchar_t* moduleName, const char* name, FuncPtr fn) noexcept {
        if ((!module && !moduleName) || !name || !fn)
            return;

        // jeœli ju¿ istnieje – nadpisz
        for (std::size_t i = 0; i < count_; ++i) {
            bool moduleMatch = (module && entries_[i].module == module) ||
                (moduleName && entries_[i].moduleName &&
                    _wcsicmp(entries_[i].moduleName, moduleName) == 0);

            if (moduleMatch && std::strcmp(entries_[i].name, name) == 0)
            {
                entries_[i].fn = fn;
                return;
            }
        }

        // je¿eli mamy miejsce – dodaj nowy
        if (count_ < kMaxEntries) {
            entries_[count_].module = module;
            entries_[count_].moduleName = moduleName;
            entries_[count_].name = name;
            entries_[count_].fn = fn;
            ++count_;
        }
    }

    // Sprawdza czy œcie¿ka modu³u koñczy siê podan¹ nazw¹
    static bool ModuleNameMatches(HMODULE module, const wchar_t* targetName) noexcept {
        if (!module || !targetName)
            return false;

        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(module, path, MAX_PATH) == 0)
            return false;

        // ZnajdŸ ostatni backslash
        const wchar_t* fileName = path;
        for (const wchar_t* p = path; *p; ++p) {
            if (*p == L'\\' || *p == L'/') {
                fileName = p + 1;
            }
        }

        return _wcsicmp(fileName, targetName) == 0;
    }

    Entry       entries_[kMaxEntries]{};
    std::size_t count_ = 0;
};