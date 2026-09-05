#pragma once

#include <filesystem>
#include <string>

namespace NgxFileSigner {

    constexpr size_t MINIMUM_FILESIZE = 32000000;
    constexpr const wchar_t* ORIGINAL_FILENAME = L"nvngx_dlss.dll";
    constexpr const wchar_t* TARGET_FILENAME = L"nvngx.dll";
    constexpr const wchar_t* DLSSG_ORIGINAL_FILENAME = L"nvngx_dlssg.dll";
    constexpr const wchar_t* DLSSG_TARGET_FILENAME = L"dlssg_to_fsr3_amd_is_better.dll";
    constexpr const wchar_t* INI_FILENAME = L"OptiScaler.ini";
    constexpr int SEARCH_DEPTH = 3;

    enum class Result {
        Success = 0,
        InvalidDirectory = 1,
        FileNotFound = 2,
        InvalidFile = 3,
        CopyFailed = 4
    };

    struct SignResult {
        Result code;
        std::wstring message;
        std::filesystem::path foundPath;
        std::filesystem::path destinationPath;

        bool IsSuccess() const { return code == Result::Success; }
    };

    // Main function - searches for DLSS file and copies it to the starting directory
    // Returns SignResult with status code and optional error message
    SignResult SignNgxFile();

    // Overload with custom search parameters
    SignResult SignNgxFile(const std::filesystem::path& startingDirectory);

    // Primes DLSSG module for OptiPatcher by copying nvngx_dlssg.dll as dlssg_to_fsr3_amd_is_better.dll
    // Only executes when OptiPatcher plugin is present
    void PrimeDlssgModule();

    // Creates default OptiScaler.ini file if it doesn't exist in the module directory
    // Returns true if file was created or already exists
    bool CreateDefaultIniFile(bool isAutoExposureEnabled);

    // Helper function to search for a file in directory and subdirectories
    bool SearchFileInDirectory(const std::filesystem::path& dir, const std::wstring& filename, std::filesystem::path& foundPath);

    // Helper function to search for a file with depth limit
    bool SearchFile(const std::filesystem::path& startPath, const std::wstring& filename, std::filesystem::path& foundPath);

    // Get human-readable error message for result code
    std::wstring GetResultMessage(Result code);

} // namespace NgxFileSigner